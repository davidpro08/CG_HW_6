#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>
#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>
#include <random>
#include <limits>
#define _USE_MATH_DEFINES
#include <cmath>

#define GLM_SWIZZLE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

const int Width = 512;
const int Height = 512;
std::vector<float> OutputImage;
std::vector<float> DepthBuffer;

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct Triangle {
    int v0, v1, v2;
};

std::vector<Vertex> vertices;
std::vector<Triangle> triangles;

void create_scene() {
    int w = 32, h = 16;
    vertices.clear();
    triangles.clear();

    for (int j = 1; j < h - 1; ++j) {
        for (int i = 0; i < w; ++i) {
            float theta = (float)j / (h - 1) * glm::pi<float>();
            float phi = (float)i / (w - 1) * glm::pi<float>() * 2.0f;

            float x = sinf(theta) * cosf(phi);
            float y = cosf(theta);
            float z = -sinf(theta) * sinf(phi);
            vertices.push_back({ glm::vec3(x, y, z), glm::vec3(0.0f) });
        }
    }
    vertices.push_back({ glm::vec3(0, 1, 0), glm::vec3(0.0f) });     // top
    vertices.push_back({ glm::vec3(0, -1, 0), glm::vec3(0.0f) });    // bottom

    for (int j = 0; j < h - 3; ++j) {
        for (int i = 0; i < w - 1; ++i) {
            int a = j * w + i;
            int b = (j + 1) * w + (i + 1);
            int c = j * w + (i + 1);
            int d = (j + 1) * w + i;
            triangles.push_back({ a, b, c });
            triangles.push_back({ a, d, b });
        }
    }
    int top = vertices.size() - 2;
    int bottom = vertices.size() - 1;
    for (int i = 0; i < w - 1; ++i) {
        triangles.push_back({ top, i, i + 1 });
        int j = (h - 3) * w;
        triangles.push_back({ bottom, j + i + 1, j + i });
    }

    // Vertex normals 계산 (평균 법선)
    for (auto& v : vertices) v.normal = glm::vec3(0.0f);
    for (auto tri : triangles) {
        glm::vec3 p0 = vertices[tri.v0].position;
        glm::vec3 p1 = vertices[tri.v1].position;
        glm::vec3 p2 = vertices[tri.v2].position;
        glm::vec3 n = glm::normalize(glm::cross(p2 - p0, p1 - p0));
        vertices[tri.v0].normal += n;
        vertices[tri.v1].normal += n;
        vertices[tri.v2].normal += n;
    }
    for (auto& v : vertices) v.normal = glm::normalize(v.normal);
}

const glm::vec3 ka(0.0f, 1.0f, 0.0f);
const glm::vec3 kd(0.0f, 0.5f, 0.0f);
const glm::vec3 ks(0.5f);
const float p = 32.0f;
const glm::vec3 Ia(0.2f);
const glm::vec3 lightPos(-4.0f, 4.0f, -3.0f);
const glm::vec3 lightColor(1.0f);

glm::vec3 gamma_correct(const glm::vec3& color) {
    return glm::pow(color, glm::vec3(1.0f / 2.2f));
}

void render_gouraud() {
    create_scene();
    OutputImage.assign(Width * Height * 3, 0.0f);
    DepthBuffer.assign(Width * Height, std::numeric_limits<float>::infinity());

    glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, -7)) * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    glm::mat4 V = glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    float l = -0.1f, r = 0.1f, b = -0.1f, t = 0.1f, n = -0.1f, f = -1000.0f;
    glm::mat4 P(0.0f);
    P[0][0] = 2 * n / (r - l);
    P[1][1] = 2 * n / (t - b);
    P[2][0] = (r + l) / (r - l);
    P[2][1] = (t + b) / (t - b);
    P[2][2] = (n + f) / (n - f);
    P[2][3] = -1.0f;
    P[3][2] = 2 * f * n / (n - f);
    glm::mat4 VP(1.0f);
    VP[0][0] = Width / 2.0f; VP[1][1] = Height / 2.0f;
    VP[3][0] = Width / 2.0f; VP[3][1] = Height / 2.0f;

    glm::mat4 MVP = VP * P * V * M;
    glm::mat4 MV = V * M;
    glm::vec3 light_cam = glm::vec3(V * glm::vec4(lightPos, 1.0f));

    std::vector<glm::vec3> transformed(vertices.size());
    std::vector<glm::vec3> shadedColor(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        glm::vec4 pos_cam = MV * glm::vec4(vertices[i].position, 1.0f);
        glm::vec3 pos3 = glm::vec3(pos_cam);
        glm::vec3 N = glm::normalize(glm::mat3(MV) * vertices[i].normal);
        glm::vec3 L = glm::normalize(light_cam - pos3);
        glm::vec3 Vdir = glm::normalize(-pos3);
        glm::vec3 H = glm::normalize(L + Vdir);

        glm::vec3 c = ka * Ia + kd * lightColor * std::max(0.0f, glm::dot(N, L)) + ks * lightColor * std::pow(std::max(0.0f, glm::dot(N, H)), p);
        shadedColor[i] = gamma_correct(c);

        glm::vec4 p = MVP * glm::vec4(vertices[i].position, 1.0f);
        p /= p.w;
        transformed[i] = glm::vec3(p);
    }

    for (auto tri : triangles) {
        glm::vec3 p0 = transformed[tri.v0];
        glm::vec3 p1 = transformed[tri.v1];
        glm::vec3 p2 = transformed[tri.v2];
        glm::vec3 c0 = shadedColor[tri.v0];
        glm::vec3 c1 = shadedColor[tri.v1];
        glm::vec3 c2 = shadedColor[tri.v2];

        float xmin = std::max(0.0f, std::floor(std::min({ p0.x, p1.x, p2.x })));
        float xmax = std::min((float)Width - 1, std::ceil(std::max({ p0.x, p1.x, p2.x })));
        float ymin = std::max(0.0f, std::floor(std::min({ p0.y, p1.y, p2.y })));
        float ymax = std::min((float)Height - 1, std::ceil(std::max({ p0.y, p1.y, p2.y })));

        for (int y = (int)ymin; y <= (int)ymax; ++y) {
            for (int x = (int)xmin; x <= (int)xmax; ++x) {
                glm::vec2 p(x + 0.5f, y + 0.5f);
                glm::vec2 a(p0.x, p0.y), b(p1.x, p1.y), c(p2.x, p2.y);
                float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
                float alpha = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
                float beta = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
                float gamma = 1.0f - alpha - beta;

                if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                    float z = alpha * p0.z + beta * p1.z + gamma * p2.z;
                    int idx = y * Width + x;
                    if (z < DepthBuffer[idx]) {
                        DepthBuffer[idx] = z;
                        glm::vec3 color = alpha * c0 + beta * c1 + gamma * c2;
                        OutputImage[idx * 3 + 0] = color.r;
                        OutputImage[idx * 3 + 1] = color.g;
                        OutputImage[idx * 3 + 2] = color.b;
                    }
                }
            }
        }
    }
}

int main() {
    GLFWwindow* window;
    if (!glfwInit()) return -1;
    window = glfwCreateWindow(Width, Height, "Gouraud Shading (Q2)", NULL, NULL);
    if (!window) {
        glfwTerminate(); return -1;
    }
    glfwMakeContextCurrent(window);
    glViewport(0, 0, Width, Height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, Width, 0.0, Height, 1.0, -1.0);
    OutputImage.reserve(Width * Height * 3);

    render_gouraud();

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
        glfwSwapBuffers(window);
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}