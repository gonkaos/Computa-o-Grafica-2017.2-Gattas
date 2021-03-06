// renderwidget.cpp

#include <QImage>
#include <QGLWidget>
#include <QMouseEvent>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>                

#include <GL/gl.h>

#include <cmath>
#include "renderwidget.h"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

using namespace glm;

RenderWidget::RenderWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , program(nullptr)
{

    cam.at = glm::vec3(0.f,0.f,0.f);
    cam.eye = cam.at;
    cam.eye.z = cam.at.z + 20.f;
    cam.up = glm::vec3(0.f,1.f,0.f);
    cam.zNear = 0.1f;
    cam.zFar  = 100.f;
    cam.fovy  = 60.f;
    cam.width = width();
    cam.height = height();
}


RenderWidget::~RenderWidget()
{
    delete program;

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}


void RenderWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);

    glClearColor(0,0,0,1);
    glViewport(0,0,width(),height());

    //Compilar os shaders
    program = new QOpenGLShaderProgram();
    program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/vertexshader.glsl");
    program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fragmentshader.glsl");
    program->link();

    //Criar objeto e textura
    createCube();
    createTexture(":/textures/cube_texture.png");

    //createSphere();
    //createTexture(":/textures/sphere_texture.jpg");

    //Criar VBO e VAO
    createVBO();
}


void RenderWidget::paintGL()
{
    //Limpar a tela
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    //Linkar o VAO
    glBindVertexArray(VAO);

    //Linkar o programa e passar as uniformes
    program->bind();

    //Definir matriz view e projection
    view = glm::lookAt(cam.eye, cam.at, cam.up);
    glm::vec3 _radius=cam.eye-cam.at;
    radius=_radius.length();radius*=-40;
    proj = glm::perspective(glm::radians(cam.fovy), (float)cam.width/cam.height, cam.zNear, cam.zFar);

    QMatrix4x4 m(glm::value_ptr(glm::transpose(model)));
    QMatrix4x4 v(glm::value_ptr(glm::transpose(view)));
    QMatrix4x4 p(glm::value_ptr(glm::transpose(proj)));

    //model*=qrot;
    //Passar as uniformes da luz e do material
    program->setUniformValue("light.position", v*QVector3D(5,9,-5) );
    program->setUniformValue("material.ambient", QVector3D(1.f,1.f,1.f));
    program->setUniformValue("material.diffuse", QVector3D(1.0f,1.0f,1.0f));
    program->setUniformValue("material.specular", QVector3D(1.0f,1.0f,1.0f));
    program->setUniformValue("material.shininess", 100.0f);

    //Ativar e linkar a textura
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    program->setUniformValue("sampler", 0);

    //Passar as matrizes de transformação
    for( int x = -3; x <= 3; x+=3 )
    {
        for( int z = -3; z <= 3; z+=3)
        {
            QMatrix4x4 mObj;
            mObj.translate(x,0,z);

            QMatrix4x4 mv = v * (m * mObj);
            QMatrix4x4 mvp = p * mv;
            program->setUniformValue("mv", mv);
            program->setUniformValue("mv_ti", mv.inverted().transposed());
            program->setUniformValue("mvp", mvp);

            //Desenhar
            glDrawElements(GL_TRIANGLES, (GLsizei) indices.size(), GL_UNSIGNED_INT, 0);
        }
    }
    DrawRays();
}


void RenderWidget::resizeGL(int w, int h)
{
    //Atualizar a viewport
    glViewport(0,0,w,h);

    //Atualizar a câmera
    cam.width = w;
    cam.height = h;
}


void RenderWidget::mousePressEvent(QMouseEvent *event)
{
  glm::vec4 viewport = glm::vec4(0.0f, 0.0f, cam.width, cam.height);
  glm::vec3 screenPos = glm::vec3(event->x(), height()-event->y(), 0.1);
  glm::vec3 point= glm::unProject(screenPos, /*view**/model, proj, viewport);
  //glm::vec3 pt2= glm::unProject (screenPos, view*model, proj, viewport);

  //glm::vec3 _radius=cam.eye-cam.at;
  //float r=_radius.length();
  V1 = point - cam.at;
  V1=glm::normalize(V1);
  pquat = glm::quat (0,point.x,point.y,point.z);
}


void RenderWidget::mouseReleaseEvent(QMouseEvent *event)
{
    //V1=V2;
}


void RenderWidget::mouseMoveEvent(QMouseEvent *event)
{
   if(event->buttons() & Qt::LeftButton) {

       glm::vec4 viewport = glm::vec4(0.0f, 0.0f, cam.width, cam.height);
       glm::vec3 screenPos = glm::vec3(event->x(), height()-event->y(), 0.1 );
       glm::vec3 point= glm::unProject (screenPos, /*view**/model, proj, viewport);
       //glm::vec3 pt1= glm::unProject (screenPos, view*model, proj, viewport);
       //pt1.x=(screenPos.x-cam.at.x)/radius;


       V2 = point - cam.at;
       V2=glm::normalize(V2);

       glm::vec3 rot = glm::cross(V1,V2);
       float cosT = glm::dot(V1,V2);
       float Theta=acos(cosT)*0.5;
       cosT=cos(Theta);
       float sinT = sin(Theta);
       rot=glm::normalize(rot);
       rot*=sinT;
       rquat = glm::quat (cosT,rot.x,rot.y,rot.z);
       qrot=rquat*pquat*glm::conjugate(rquat);
       glm::vec3 newpos = vec3(qrot.x,qrot.y,qrot.z);
       cam.eye=radius*newpos;
       update();

   }
}

//zoom
void RenderWidget::wheelEvent(QWheelEvent *event)
{
    float scalefactor=1.2;
    if(event->delta()<0){
        cam.eye*=scalefactor;
    }
    else{
        cam.eye/=scalefactor;
    }
    update();
}


void RenderWidget::createCube()
{
    //Definir vértices, normais e índices
    vertices = {
        { -1, -1, -1 }, { -1, -1, -1 }, { -1, -1, -1 },
        { +1, -1, -1 }, { +1, -1, -1 }, { +1, -1, -1 },
        { +1, -1, +1 }, { +1, -1, +1 }, { +1, -1, +1 },
        { -1, -1, +1 }, { -1, -1, +1 }, { -1, -1, +1 },
        { -1, +1, -1 }, { -1, +1, -1 }, { -1, +1, -1 },
        { +1, +1, -1 }, { +1, +1, -1 }, { +1, +1, -1 },
        { +1, +1, +1 }, { +1, +1, +1 }, { +1, +1, +1 },
        { -1, +1, +1 }, { -1, +1, +1 }, { -1, +1, +1 }
    };
    
    normals = {
        {  0, -1,  0 }, { -1,  0,  0 }, {  0,  0, -1 },
        {  0, -1,  0 }, { +1,  0,  0 }, {  0,  0, -1 },
        {  0, -1,  0 }, { +1,  0,  0 }, {  0,  0, +1 },
        {  0, -1,  0 }, { -1,  0,  0 }, {  0,  0, +1 },
        { -1,  0,  0 }, {  0,  0, -1 }, {  0, +1,  0 },
        { +1,  0,  0 }, {  0,  0, -1 }, {  0, +1,  0 },
        { +1,  0,  0 }, {  0,  0, +1 }, {  0, +1,  0 },
        { -1,  0,  0 }, {  0,  0, +1 }, {  0, +1,  0 }
    };

    texCoords = {
        {0.25, 0.50}, {0.25, 0.50}, {0.50, 0.75},
        {0.00, 0.50}, {1.00, 0.50}, {0.75, 0.75},
        {0.00, 0.25}, {1.00, 0.25}, {0.75, 0.00},
        {0.25, 0.25}, {0.25, 0.25}, {0.50, 0.00},
        {0.50, 0.50}, {0.50, 0.50}, {0.50, 0.50},
        {0.75, 0.50}, {0.75, 0.50}, {0.75, 0.50},
        {0.75, 0.25}, {0.75, 0.25}, {0.75, 0.25},
        {0.50, 0.25}, {0.50, 0.25}, {0.50, 0.25}
    };

    indices = {
        0,   3,  6, //normal: (  0, -1,  0 )
        0,   6,  9, //normal: (  0, -1,  0 )
        12,  1, 10, //normal: ( -1,  0,  0 )
        12, 10, 21, //normal: ( -1,  0,  0 )
        18,  7,  4, //normal: ( +1,  0,  0 )
        18,  4, 15, //normal: ( +1,  0,  0 )
        22, 11,  8, //normal: (  0,  0, +1 )
        22,  8, 19, //normal: (  0,  0, +1 )
        16,  5,  2, //normal: (  0,  0, -1 )
        16,  2, 13, //normal: (  0,  0, -1 )
        23, 20, 17, //normal: (  0, +1,  0 )
        23, 17, 14  //normal: (  0, +1,  0 )
    };
}


int getIndex( int i, int j, int n )
{
    return j + i * ( n + 1 );
}
void RenderWidget::createSphere()
{
    const int n = 100;
    const int m = 100;

    const int numTriangles = 2 * n * m;
    const int numVertices = ( n + 1 ) * ( m + 1 );

    for( unsigned int i = 0; i <= n; i++ )
    {
        for( unsigned int j = 0; j <= m; j++ )
        {
            //Atualizar as coordenadas de textura
            float s = (float) i / n;
            float t = (float) j / m;
            texCoords.push_back(glm::vec2(s,t));

            //Calcula os parâmetros
            double theta = 2 * s * M_PI;
            double phi = t * M_PI;
            double sinTheta = sin( theta );
            double cosTheta = cos( theta );
            double sinPhi = sin( phi );
            double cosPhi = cos( phi );

            //Calcula os vértices == equacao da esfera
            vertices.push_back( glm::vec3(cosTheta * sinPhi,
                                          cosPhi,
                                          sinTheta * sinPhi) );
        }
    }

    normals = vertices;

    indices.resize(numTriangles*3);

    //Preenche o vetor com a triangulação
    unsigned int k = 0;
    for( unsigned int i = 0; i < n; i++ )
    {
        for( unsigned int j = 0; j < m; j++ )
        {
            indices[ k++ ] = getIndex( i + 1, j, n );
            indices[ k++ ] = getIndex( i + 1, j + 1, n );
            indices[ k++ ] = getIndex( i, j, n );


            indices[ k++ ] = getIndex( i + 1, j + 1, n );
            indices[ k++ ] = getIndex( i, j + 1, n );
            indices[ k++ ] = getIndex( i, j, n );
        }
    }
}


void RenderWidget::createVBO()
{
    //Construir vetor do vbo
    //OBS: Os dados já poderiam estar sendo armazenados assim na classe.
    struct vertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 texCoord;
    };

    std::vector< vertex > vbo;
    vbo.reserve( vertices.size() );
    for( unsigned int i = 0; i < vertices.size(); i++ )
    {
        vbo.push_back({vertices[i], normals[i], texCoords[i]});
    }

    //Criar VBO, linkar e copiar os dados
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vbo.size()*sizeof(vertex), &vbo[0], GL_STATIC_DRAW);

    //Criar EBO, linkar e copiar os dados
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    //Criar VAO, linkar e definir layouts
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //Habilitar, linkar e definir o layout dos buffers
    glBindBuffer( GL_ARRAY_BUFFER, VBO );

    glEnableVertexAttribArray( 0 );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE,
                           sizeof(vertex),
                           (void*)0 );

    glEnableVertexAttribArray( 1 );
    glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE,
                           sizeof(vertex),
                           (void*)sizeof(glm::vec3) );

    glEnableVertexAttribArray( 2 );
    glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE,
                           sizeof(vertex),
                           (void*)(2*sizeof(glm::vec3)) );

    //Linkar o EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
}


void RenderWidget::createTexture(const std::string& imagePath)
{
    //Criar a textura
    glGenTextures(1, &textureID);
    
    //Linkar (bind) a textura criada
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    //Abrir arquivo de imagem com o Qt
    QImage texImage = QGLWidget::convertToGLFormat(QImage(imagePath.c_str()));

    //Enviar a imagem para o OpenGL
    glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA,
                 texImage.width(), texImage.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.bits());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);
}
