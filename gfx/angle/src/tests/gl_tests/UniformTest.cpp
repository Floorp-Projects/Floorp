//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class UniformTest : public ANGLETest
{
  protected:
    UniformTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string &vertexShader = "void main() { gl_Position = vec4(1); }";
        const std::string &fragShader =
            "precision mediump float;\n"
            "uniform float uniF;\n"
            "uniform int uniI;\n"
            "void main() { gl_FragColor = vec4(uniF + float(uniI)); }";

        mProgram = CompileProgram(vertexShader, fragShader);
        ASSERT_NE(mProgram, 0u);

        mUniformFLocation = glGetUniformLocation(mProgram, "uniF");
        ASSERT_NE(mUniformFLocation, -1);

        mUniformILocation = glGetUniformLocation(mProgram, "uniI");
        ASSERT_NE(mUniformILocation, -1);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        ANGLETest::TearDown();
    }

    GLuint mProgram;
    GLint mUniformFLocation;
    GLint mUniformILocation;
};

TEST_P(UniformTest, GetUniformNoCurrentProgram)
{
    glUseProgram(mProgram);
    glUniform1f(mUniformFLocation, 1.0f);
    glUniform1i(mUniformILocation, 1);
    glUseProgram(0);

    GLfloat f;
    glGetnUniformfvEXT(mProgram, mUniformFLocation, 4, &f);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1.0f, f);

    glGetUniformfv(mProgram, mUniformFLocation, &f);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1.0f, f);

    GLint i;
    glGetnUniformivEXT(mProgram, mUniformILocation, 4, &i);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, i);

    glGetUniformiv(mProgram, mUniformILocation, &i);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, i);
}

TEST_P(UniformTest, UniformArrayLocations)
{
    // TODO(geofflang): Figure out why this is broken on Intel OpenGL
    if (isIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on Intel OpenGL." << std::endl;
        return;
    }

    const std::string vertexShader = SHADER_SOURCE
    (
        precision mediump float;
        uniform float uPosition[4];
        void main(void)
        {
            gl_Position = vec4(uPosition[0], uPosition[1], uPosition[2], uPosition[3]);
        }
    );

    const std::string fragShader = SHADER_SOURCE
    (
        precision mediump float;
        uniform float uColor[4];
        void main(void)
        {
            gl_FragColor = vec4(uColor[0], uColor[1], uColor[2], uColor[3]);
        }
    );

    GLuint program = CompileProgram(vertexShader, fragShader);
    ASSERT_NE(program, 0u);

    // Array index zero should be equivalent to the un-indexed uniform
    EXPECT_NE(-1, glGetUniformLocation(program, "uPosition"));
    EXPECT_EQ(glGetUniformLocation(program, "uPosition"), glGetUniformLocation(program, "uPosition[0]"));

    EXPECT_NE(-1, glGetUniformLocation(program, "uColor"));
    EXPECT_EQ(glGetUniformLocation(program, "uColor"), glGetUniformLocation(program, "uColor[0]"));

    // All array uniform locations should be unique
    GLint positionLocations[4] =
    {
        glGetUniformLocation(program, "uPosition[0]"),
        glGetUniformLocation(program, "uPosition[1]"),
        glGetUniformLocation(program, "uPosition[2]"),
        glGetUniformLocation(program, "uPosition[3]"),
    };

    GLint colorLocations[4] =
    {
        glGetUniformLocation(program, "uColor[0]"),
        glGetUniformLocation(program, "uColor[1]"),
        glGetUniformLocation(program, "uColor[2]"),
        glGetUniformLocation(program, "uColor[3]"),
    };

    for (size_t i = 0; i < 4; i++)
    {
        EXPECT_NE(-1, positionLocations[i]);
        EXPECT_NE(-1, colorLocations[i]);

        for (size_t j = i + 1; j < 4; j++)
        {
            EXPECT_NE(positionLocations[i], positionLocations[j]);
            EXPECT_NE(colorLocations[i], colorLocations[j]);
        }
    }

    glDeleteProgram(program);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(UniformTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());

} // namespace
