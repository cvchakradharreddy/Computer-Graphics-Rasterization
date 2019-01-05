// This example is heavily based on the tutorial at https://open.gl

// OpenGL Helpers to reduce the clutter
#include "Helpers.h"

// GLFW is necessary to handle the OpenGL context
#include <GLFW/glfw3.h>

// Linear Algebra Library
#include <Eigen/Core>

// Timer
#include <chrono>

#include <iostream>
#include <string>
using namespace std;

#define PI 3.14159265

// VertexBufferObject wrapper
VertexBufferObject VBO;

// VertexBufferObject wrapper
VertexBufferObject VBO_C;

// Contains the vertex positions
Eigen::MatrixXf V;

// Contains the color value for respective vertices
Eigen::MatrixXf C;

// Contains the view transformation
Eigen::Matrix4f view(4,4);
Eigen::Matrix4f translateView(4,4);

Eigen::MatrixXf colorCode(3,12);

enum ObjectType
{
    UNKNOWN,
    POINT,
    LINE,
    LINE_DRAG,
    LINELOOP,
    TRIANGLE
};

enum Action
{
    NONE,
    INSERTION,
    TRANSLATION,
    DELETION,
    ROTATE_CLK,
    ROTATE_CNT_CLK,
    SCALE_UP,
    SCALE_DOWN,
    COLOR_MODIFICATION
};

int no_of_clicks_insertion = 0;
int no_of_clicks_translate = 0;
Action actionTriggered;
ObjectType drawObject;
bool enableCursorTrack = false;
int selectedObjectIndex = -1;
float translation_x = 0.0;
float translation_y = 0.0;
float pointer_x = 0.0;
float pointer_y = 0.0;
int selectedVertex = -1;

bool ptInTriangle(float px, float py, float v0x, float v0y, float v1x, float v1y, float v2x, float v2y) {
    float dX = px-v2x;
    float dY = py-v2y;
    float dX21 = v2x-v1x;
    float dY12 = v1y-v2y;
    float D = dY12*(v0x-v2x) + dX21*(v0y-v2y);
    float s = dY12*dX + dX21*dY;
    float t = (v2y-v0y)*dX + (v0x-v2x)*dY;
    if (D<0) return s<=0 && t<=0 && s+t>=D;
    return s>=0 && t>=0 && s+t<=D;
}

void removeColumn(Eigen::MatrixXf& matrix, unsigned int colToRemove)
{
    unsigned int numRows = matrix.rows();
    unsigned int numCols = matrix.cols()-1;
    
    if( colToRemove < numCols )
        matrix.block(0,colToRemove,numRows,numCols-colToRemove) = matrix.rightCols(numCols-colToRemove);
    
    matrix.conservativeResize(numRows,numCols);
}


Eigen::Matrix4f rotate(double degree){
    // Contains the rotation transformation
    Eigen::Matrix4f rotation(4,4);
    
    rotation <<
    cos(degree*PI/180), -sin(degree*PI/180), 0, 0,
    sin(degree*PI/180), cos(degree*PI/180), 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1;
    
    return rotation;
}

Eigen::Matrix4f translate(float x, float y){
    // Contains the rotation transformation
    Eigen::Matrix4f translate(4,4);
    
    translate <<
    1, 0, 0, x,
    0, 1, 0, y,
    0, 0, 1, 0,
    0, 0, 0, 1;
    
    return translate;
}

Eigen::Matrix4f scale(float zoom){
    // Contains the rotation transformation
    Eigen::Matrix4f scale(4,4);
    
    scale <<
    zoom, 0, 0, 0,
    0, zoom, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1;
    
    return scale;
}

void centroid_of_triangle(float v0x, float v0y, float v1x, float v1y, float v2x, float v2y, float &px, float &py){
    px = (v0x + v1x + v2x) / 3;
    py = (v0y + v1y + v2y) / 3;
}

void updateChangesToSelecteObj(){
    if(selectedObjectIndex > -1){
        for(unsigned int i = 0; i < 3; i++){
            Eigen::Vector4f selectedObj;
            selectedObj << V.col(selectedObjectIndex+i).x(), V.col(selectedObjectIndex+i).y(), 0.0, 1.0;
            selectedObj = translateView * selectedObj;
            V.col(selectedObjectIndex+i) << selectedObj.x(), selectedObj.y();
        }
        VBO.update(V);
        selectedObjectIndex = -1;
        no_of_clicks_translate = 0;
    }
    if(selectedVertex > -1){
        selectedVertex = -1;
    }
}

void updateObjectColor(Eigen::MatrixXf &C, unsigned int startIndex, Eigen::Vector3f color){
    for(unsigned int i=0; i < 3; i++){
        C.col(startIndex+i) << color;
    }
    VBO_C.update(C);
}

void findNearestVertex(float click_x, float click_y) {
    float nearDistance = 999999.0;
    float distance;
    for(unsigned int i = 0; i < V.cols(); i++){
        distance = sqrt(pow(V.col(i).x() - click_x, 2) + pow(V.col(i).y() - click_y, 2));
        if(distance < nearDistance){
            selectedVertex = i;
            nearDistance = distance;
        }
    }
}

void cursor_position_callback(GLFWwindow *window, double x, double y)
{
    if (enableCursorTrack)
    {
        // Get the size of the window
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        // Convert screen position to world coordinates
        double xworld = ((x / double(width)) * 2) - 1;
        double yworld = (((height - 1 - y) / double(height)) * 2) - 1; // NOTE: y axis is flipped in glfw
        if(actionTriggered == Action::INSERTION){
            switch (no_of_clicks_insertion)
            {
                case 1:
                    V.col(V.cols()-1) << xworld, yworld;
                    break;
                case 2:
                    V.col(V.cols()-1) << xworld, yworld;
                    break;
                default:
                    break;
            }
            // Upload the change to the GPU
            VBO.update(V);
        } else if(actionTriggered == Action::TRANSLATION){
            switch (no_of_clicks_translate)
            {
                case 1:
                    translation_x = xworld - pointer_x;
                    translation_y = yworld - pointer_y;
                    break;
                default:
                    translation_x = 0.0;
                    translation_y = 0.0;
                    break;
            }
            translateView = translate(translation_x, translation_y);
        }
    }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    // Get the position of the mouse in the window
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Get the size of the window
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Convert screen position to world coordinates
    double xworld = ((xpos / double(width)) * 2) - 1;
    double yworld = (((height - 1 - ypos) / double(height)) * 2) - 1; // NOTE: y axis is flipped in glfw

    // Update the position of the first vertex if the left button is pressed
    if (actionTriggered == Action::INSERTION)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            switch (action)
            {
            case GLFW_PRESS:
                    break;
            case GLFW_RELEASE:
                no_of_clicks_insertion = no_of_clicks_insertion + 1;
                switch (no_of_clicks_insertion)
                {
                case 1:
                    V.conservativeResize(Eigen::NoChange,V.cols()+1);
                    C.conservativeResize(Eigen::NoChange,V.cols()+1);
                    V.col(V.cols()-1) << xworld, yworld;
                    C.col(V.cols()-1) << colorCode.col(0);
                    drawObject = ObjectType::POINT;
                    V.conservativeResize(Eigen::NoChange,V.cols()+1);
                    C.conservativeResize(Eigen::NoChange,V.cols()+1);
                    V.col(V.cols()-1) << xworld, yworld;
                    C.col(V.cols()-1) << colorCode.col(0);
                    drawObject = ObjectType::LINE;
                    enableCursorTrack = true;
                    break;
                case 2:
                    V.conservativeResize(Eigen::NoChange,V.cols()+1);
                    C.conservativeResize(Eigen::NoChange,V.cols()+1);
                    V.col(V.cols()-1) << xworld, yworld;
                    drawObject = ObjectType::LINELOOP;
                    enableCursorTrack = true;
                    break;
                case 3:
                    V.col(V.cols()-1) << xworld, yworld;
                    updateObjectColor(C, V.cols()-3, colorCode.col(10));   //set traiangle color to red
                    drawObject = ObjectType::TRIANGLE;
                    enableCursorTrack = false;
                    no_of_clicks_insertion = 0;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
        // Upload the change to the GPU
        VBO.update(V);
        VBO_C.update(C);
    } else if(actionTriggered == Action::TRANSLATION) {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            switch (action)
            {
                case GLFW_PRESS:
                    no_of_clicks_translate = no_of_clicks_translate + 1;
                    switch (no_of_clicks_translate)
                    {
                        case 1:
                        {
                            unsigned int triangleBlock = 0;
                            for(unsigned int i = 0; i < V.cols()/3; i++){
                                if(ptInTriangle(xworld, yworld, V.col(triangleBlock).x(), V.col(triangleBlock).y(), V.col(triangleBlock+1).x(), V.col(triangleBlock+1).y(), V.col(triangleBlock+2).x(), V.col(triangleBlock+2).y())){
                                    pointer_x = xworld;
                                    pointer_y = yworld;
                                    selectedObjectIndex = triangleBlock;
                                    enableCursorTrack = true;
                                }
                                triangleBlock = triangleBlock + 3;
                                translateView = translate(0, 0);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                case GLFW_RELEASE:
                    switch (no_of_clicks_translate)
                    {
                    case 1:
                        if(selectedObjectIndex > -1){
                            V.col(selectedObjectIndex) << translation_x + V.col(selectedObjectIndex).x() , translation_y + V.col(selectedObjectIndex).y();
                            V.col(selectedObjectIndex+1) << translation_x + V.col(selectedObjectIndex+1).x() , translation_y + V.col(selectedObjectIndex+1).y();
                            V.col(selectedObjectIndex+2) << translation_x + V.col(selectedObjectIndex+2).x() , translation_y + V.col(selectedObjectIndex+2).y();
                            VBO.update(V);
                        }
                        enableCursorTrack = false;
                            translation_x = 0.0;
                            translation_y = 0.0;
                            translateView = translate(translation_x, translation_y);
                        break;
                    case 2:
                        updateChangesToSelecteObj();
                    default:
                        break;
                    }
                default:
                    break;
            }
        }
    } else if(actionTriggered == Action::DELETION){
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            switch (action)
            {
                case GLFW_PRESS:
                {
                    unsigned int triangleBlock = 0;
                    for(unsigned int i = 0; i < V.cols()/3; i++){
                        if(ptInTriangle(xworld, yworld, V.col(triangleBlock).x(), V.col(triangleBlock).y(), V.col(triangleBlock+1).x(), V.col(triangleBlock+1).y(), V.col(triangleBlock+2).x(), V.col(triangleBlock+2).y())){
                            //remove all the three columns from V
                            //note that for each removal matrix values shifts to the left and rearranges it's size
                            removeColumn(V, triangleBlock); // removes first vertex of this block
                            removeColumn(C, triangleBlock); // removes first vertex color of this block
                            removeColumn(V, triangleBlock); // removes second vertex of this block
                            removeColumn(C, triangleBlock); // removes second vertex color of this block
                            removeColumn(V, triangleBlock); // removes third vertex of this block
                            removeColumn(C, triangleBlock); //removes third vertex color of this block
                            VBO.update(V);
                            VBO_C.update(C);
                        }
                        triangleBlock = triangleBlock + 3;
                    }
                    break;
                }
                case GLFW_RELEASE:
                default:
                    selectedObjectIndex = -1;
                    break;
            }
        }
    } else if(actionTriggered == Action::COLOR_MODIFICATION) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        {
            findNearestVertex(xworld, yworld);
        }
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS || action == GLFW_REPEAT){
        // Update the position of the first vertex if the keys 1,2, or 3 are pressed
        switch (key)
        {
            case GLFW_KEY_I:
                actionTriggered = Action::INSERTION;
                updateChangesToSelecteObj();
                break;
            case GLFW_KEY_O:
                actionTriggered = Action::TRANSLATION;
                updateChangesToSelecteObj();
                break;
            case GLFW_KEY_P:
                actionTriggered = Action::DELETION;
                updateChangesToSelecteObj();
                break;
            case GLFW_KEY_H:
                if(actionTriggered == Action::TRANSLATION && selectedObjectIndex != -1){
                    float px, py;
                    centroid_of_triangle(V.col(selectedObjectIndex).x(), V.col(selectedObjectIndex).y(), V.col(selectedObjectIndex+1).x(), V.col(selectedObjectIndex+1).y(), V.col(selectedObjectIndex+2).x(), V.col(selectedObjectIndex+2).y(), px, py);
                    translateView = translate(px, py) * rotate(-10) * translate(-px, -py) * translateView;
                }
                break;
            case GLFW_KEY_J:
                if(actionTriggered == Action::TRANSLATION && selectedObjectIndex != -1){
                    float px, py;
                    centroid_of_triangle(V.col(selectedObjectIndex).x(), V.col(selectedObjectIndex).y(), V.col(selectedObjectIndex+1).x(), V.col(selectedObjectIndex+1).y(), V.col(selectedObjectIndex+2).x(), V.col(selectedObjectIndex+2).y(), px, py);
                    translateView = translate(px, py) * rotate(10) * translate(-px, -py) * translateView;
                }
                break;
            case GLFW_KEY_K:
                if(actionTriggered == Action::TRANSLATION && selectedObjectIndex != -1){
                    float px, py;
                    centroid_of_triangle(V.col(selectedObjectIndex).x(), V.col(selectedObjectIndex).y(), V.col(selectedObjectIndex+1).x(), V.col(selectedObjectIndex+1).y(), V.col(selectedObjectIndex+2).x(), V.col(selectedObjectIndex+2).y(), px, py);
                    translateView = translate(px, py) * scale(1.25) * translate(-px, -py) * translateView;
                }
                break;
            case GLFW_KEY_L:
                if(actionTriggered == Action::TRANSLATION && selectedObjectIndex != -1){
                    float px, py;
                    centroid_of_triangle(V.col(selectedObjectIndex).x(), V.col(selectedObjectIndex).y(), V.col(selectedObjectIndex+1).x(), V.col(selectedObjectIndex+1).y(), V.col(selectedObjectIndex+2).x(), V.col(selectedObjectIndex+2).y(), px, py);
                    translateView = translate(px, py) * scale(0.75) * translate(-px, -py)* translateView;
                }
                break;
            case GLFW_KEY_C:
                actionTriggered = Action::COLOR_MODIFICATION;
                updateChangesToSelecteObj();
                break;
            case GLFW_KEY_1:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(1);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_2:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(2);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_3:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(3);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_4:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(4);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_5:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(5);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_6:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(6);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_7:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(7);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_8:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(8);
                    VBO_C.update(C);
                }
                break;
            case GLFW_KEY_9:
                if(actionTriggered == Action::COLOR_MODIFICATION && selectedVertex > -1){
                    C.col(selectedVertex) << colorCode.col(9);
                    VBO_C.update(C);
                }
                break;
            default:
                break;
        }
    }
}

Eigen::MatrixXf backupOriginalColor(3,11);

void drawOutput(Program program)
{
        unsigned int triangleBlock = 0;
        backupOriginalColor = C;
        for(unsigned int i = 0; i < (V.cols()/3); i++){
            // Draw a triangle
            if(!(i == (V.cols()/3)-1 && drawObject == ObjectType::LINELOOP)){
                if(selectedObjectIndex == triangleBlock){
                    updateObjectColor(C, triangleBlock, colorCode.col(11));
                    view = translateView;
                } else {
                    view = translate(0, 0);
                }
                glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, view.data());
                glDrawArrays(GL_TRIANGLES, triangleBlock, 3);
            }
            updateObjectColor(C, triangleBlock, colorCode.col(0));
            glDrawArrays(GL_LINE_LOOP, triangleBlock, 3);
            triangleBlock = triangleBlock + 3;
        }
        C = backupOriginalColor;
        VBO_C.update(C);
        switch (drawObject)
        {
            case POINT:
                // Draw a point
                glDrawArrays(GL_POINTS, triangleBlock, 1);
                break;
            case LINE:
                // Draw a line
                glDrawArrays(GL_LINES, triangleBlock, 2);
                break;
            default:
                break;
        }
}

int main(void)
{
    GLFWwindow *window;

    // Initialize the library
    if (!glfwInit())
        return -1;

    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);

    // Ensure that we get at least a 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(640, 480, "Assignment2_Task3", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

#ifndef __APPLE__
    glewExperimental = true;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }
    glGetError(); // pull and savely ignonre unhandled errors like GL_INVALID_ENUM
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
#endif

    int major, minor, rev;
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    rev = glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
    printf("OpenGL version recieved: %d.%d.%d\n", major, minor, rev);
    printf("Supported OpenGL is %s\n", (const char *)glGetString(GL_VERSION));
    printf("Supported GLSL is %s\n", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
    
    cout << "******* Task3: Colors *******" << endl;
    cout << "Press key 'c' to modify colors to the objects on the screen. After pressing key 'c' every mouse click will select the nearest vertex and on pressing any key from '1' to '9' will apply a color to the vertex. The color is interpolated linearly inside the triangles. " << endl;
    colorCode <<
    0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.5, 0.0, 0.5, 0.75,  1.0, 0.0,
    0.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.5, 0.5, 0.0, 0.75,  0.0, 0.0,
    0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.5, 0.5, 0.0,   0.0, 1.0;
    
    // Initialize the VAO
    // A Vertex Array Object (or VAO) is an object that describes how the vertex
    // attributes are stored in a Vertex Buffer Object (or VBO). This means that
    // the VAO is not the actual object storing the vertex data,
    // but the descriptor of the vertex data.
    VertexArrayObject VAO;
    VAO.init();
    VAO.bind();

    // Initialize the VBO with the vertices data
    // A VBO is a data container that lives in the GPU memory
    VBO.init();
    V.resize(2,0);
    VBO.update(V);
    
    VBO_C.init();
    C.resize(3,0);
    VBO_C.update(C);
    
    // Initialize the OpenGL Program
    // A program controls the OpenGL pipeline and it must contains
    // at least a vertex shader and a fragment shader to be valid
    Program program;
    const GLchar *vertex_shader =
        "#version 150 core\n"
        "in vec2 position;"
        "uniform mat4 view;"
        "in vec3 color;"
        "out vec3 f_color;"
        "void main()"
        "{"
        "    gl_Position = view * vec4(position, 0.0, 1.0);"
        "    f_color = color;"
        "}";
    const GLchar *fragment_shader =
        "#version 150 core\n"
        "in vec3 f_color;"
        "out vec4 outColor;"
        "void main()"
        "{"
        "    outColor = vec4(f_color, 1.0);"
        "}";

    // Compile the two shaders and upload the binary to the GPU
    // Note that we have to explicitly specify that the output "slot" called outColor
    // is the one that we want in the fragment buffer (and thus on screen)
    program.init(vertex_shader, fragment_shader, "outColor");
    program.bind();

    // The vertex shader wants the position of the vertices as an input.
    // The following line connects the VBO we defined above with the position "slot"
    // in the vertex shader
    program.bindVertexAttribArray("position", VBO);
    program.bindVertexAttribArray("color", VBO_C);
    view = translate(0.0, 0.0);
    glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, view.data());

    // Register the keyboard callback
    glfwSetKeyCallback(window, key_callback);

    // Register the mouse callback
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Register the cursor position callback
    glfwSetCursorPosCallback(window, cursor_position_callback);

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Bind your VAO (not necessary if you have only one)
        VAO.bind();

        // Bind your program
        program.bind();

        // Clear the framebuffer
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        drawOutput(program);

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    // Deallocate opengl memory
    program.free();
    VAO.free();
    VBO.free();

    // Deallocate glfw internals
    glfwTerminate();
    return 0;
}
