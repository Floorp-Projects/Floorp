//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimpleOperationTest:
//   Basic GL commands such as linking a program, initializing a buffer, etc.

#include "test_utils/ANGLETest.h"

#include <vector>

#include "random_utils.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class SimpleOperationTest : public ANGLETest
{
  protected:
    SimpleOperationTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void verifyBuffer(const std::vector<uint8_t> &data, GLenum binding);
};

void SimpleOperationTest::verifyBuffer(const std::vector<uint8_t> &data, GLenum binding)
{
    if (!extensionEnabled("GL_EXT_map_buffer_range"))
    {
        return;
    }

    uint8_t *mapPointer =
        static_cast<uint8_t *>(glMapBufferRangeEXT(GL_ARRAY_BUFFER, 0, 1024, GL_MAP_READ_BIT));
    ASSERT_GL_NO_ERROR();

    std::vector<uint8_t> readbackData(data.size());
    memcpy(readbackData.data(), mapPointer, data.size());
    glUnmapBufferOES(GL_ARRAY_BUFFER);

    EXPECT_EQ(data, readbackData);
}

TEST_P(SimpleOperationTest, CompileVertexShader)
{
    const std::string source =
        R"(attribute vec4 a_input;
        void main()
        {
            gl_Position = a_input;
        })";

    GLuint shader = CompileShader(GL_VERTEX_SHADER, source);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, CompileFragmentShader)
{
    const std::string source =
        R"(precision mediump float;
        varying vec4 v_input;
        void main()
        {
            gl_FragColor = v_input;
        })";

    GLuint shader = CompileShader(GL_FRAGMENT_SHADER, source);
    EXPECT_NE(shader, 0u);
    glDeleteShader(shader);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgram)
{
    const std::string vsSource =
        R"(void main()
        {
            gl_Position = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    const std::string fsSource =
        R"(void main()
        {
            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);
    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithUniforms)
{
    if (IsVulkan())
    {
        // TODO(jmadill): Complete Vulkan implementation.
        std::cout << "Test skipped on Vulkan." << std::endl;
        return;
    }

    const std::string vsSource =
        R"(void main()
        {
            gl_Position = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    const std::string fsSource =
        R"(precision mediump float;
        uniform vec4 u_input;
        void main()
        {
            gl_FragColor = u_input;
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);

    GLint uniformLoc = glGetUniformLocation(program, "u_input");
    EXPECT_NE(-1, uniformLoc);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, LinkProgramWithAttributes)
{
    if (IsVulkan())
    {
        // TODO(jmadill): Complete Vulkan implementation.
        std::cout << "Test skipped on Vulkan." << std::endl;
        return;
    }

    const std::string vsSource =
        R"(attribute vec4 a_input;
        void main()
        {
            gl_Position = a_input;
        })";

    const std::string fsSource =
        R"(void main()
        {
            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        })";

    GLuint program = CompileProgram(vsSource, fsSource);
    EXPECT_NE(program, 0u);

    GLint attribLoc = glGetAttribLocation(program, "a_input");
    EXPECT_NE(-1, attribLoc);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());

    std::vector<uint8_t> data(1024);
    FillVectorWithRandomUBytes(&data);
    glBufferData(GL_ARRAY_BUFFER, data.size(), &data[0], GL_STATIC_DRAW);

    verifyBuffer(data, GL_ARRAY_BUFFER);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferDataWithNoData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

    EXPECT_GL_NO_ERROR();
}

TEST_P(SimpleOperationTest, BufferSubData)
{
    GLBuffer buffer;
    glBindBuffer(GL_ARRAY_BUFFER, buffer.get());

    constexpr size_t bufferSize = 1024;
    std::vector<uint8_t> data(bufferSize);
    FillVectorWithRandomUBytes(&data);

    glBufferData(GL_ARRAY_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);

    constexpr size_t subDataCount = 16;
    constexpr size_t sliceSize    = bufferSize / subDataCount;
    for (size_t i = 0; i < subDataCount; i++)
    {
        size_t offset = i * sliceSize;
        glBufferSubData(GL_ARRAY_BUFFER, offset, sliceSize, &data[offset]);
    }

    verifyBuffer(data, GL_ARRAY_BUFFER);

    EXPECT_GL_NO_ERROR();
}

// Simple quad test.
TEST_P(SimpleOperationTest, DrawQuad)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    drawQuad(program.get(), "position", 0.5f, 1.0f, true);

    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Simple repeatd draw and swap test.
TEST_P(SimpleOperationTest, DrawQuadAndSwap)
{
    const std::string &vertexShader =
        "attribute vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1);\n"
        "}";
    const std::string &fragmentShader =
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(0, 1, 0, 1);\n"
        "}";
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);

    for (int i = 0; i < 8; ++i)
    {
        drawQuad(program.get(), "position", 0.5f, 1.0f, true);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
        swapBuffers();
    }

    EXPECT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(SimpleOperationTest,
                       ES2_D3D9(),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE),
                       ES2_D3D11(EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());

} // namespace
