//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ComputeShaderTest:
//   Compute shader specific tests.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include <vector>

using namespace angle;

namespace
{

class ComputeShaderTest : public ANGLETest
{
  protected:
    ComputeShaderTest() {}
};

class ComputeShaderTestES3 : public ANGLETest
{
  protected:
    ComputeShaderTestES3() {}
};

// link a simple compute program. It should be successful.
TEST_P(ComputeShaderTest, LinkComputeProgram)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);

    EXPECT_GL_NO_ERROR();
}

// link a simple compute program. There is no local size and linking should fail.
TEST_P(ComputeShaderTest, LinkComputeProgramNoLocalSizeLinkError)
{
    const std::string csSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = CompileComputeProgram(csSource, false);
    EXPECT_EQ(0u, program);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

// link a simple compute program.
// make sure that uniforms and uniform samplers get recorded
TEST_P(ComputeShaderTest, LinkComputeProgramWithUniforms)
{
    const std::string csSource =
        "#version 310 es\n"
        "precision mediump sampler2D;\n"
        "layout(local_size_x=1) in;\n"
        "uniform int myUniformInt;\n"
        "uniform sampler2D myUniformSampler;\n"
        "void main()\n"
        "{\n"
        "int q = myUniformInt;\n"
        "texture(myUniformSampler, vec2(0.0));\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);

    // It's not possible to validate uniforms are present since they are unreferenced.
    // TODO(jmadill): Make uniforms referenced.
    // GLint uniformLoc = glGetUniformLocation(program.get(), "myUniformInt");
    // EXPECT_NE(-1, uniformLoc);

    // uniformLoc = glGetUniformLocation(program.get(), "myUniformSampler");
    // EXPECT_NE(-1, uniformLoc);

    EXPECT_GL_NO_ERROR();
}

// Attach both compute and non-compute shaders. A link time error should occur.
// OpenGL ES 3.10, 7.3 Program Objects
TEST_P(ComputeShaderTest, AttachMultipleShaders)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string vsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);
    EXPECT_NE(0u, cs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    glLinkProgram(program);

    GLint linkStatus;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

    EXPECT_EQ(0, linkStatus);

    EXPECT_GL_NO_ERROR();
}

// Attach a vertex, fragment and compute shader.
// Query for the number of attached shaders and check the count.
TEST_P(ComputeShaderTest, AttachmentCount)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=1) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string vsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string fsSource =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "}\n";

    GLuint program = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint cs = CompileShader(GL_COMPUTE_SHADER, csSource);

    EXPECT_NE(0u, vs);
    EXPECT_NE(0u, fs);
    EXPECT_NE(0u, cs);

    glAttachShader(program, vs);
    glDeleteShader(vs);

    glAttachShader(program, fs);
    glDeleteShader(fs);

    glAttachShader(program, cs);
    glDeleteShader(cs);

    GLint numAttachedShaders;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &numAttachedShaders);

    EXPECT_EQ(3, numAttachedShaders);

    glDeleteProgram(program);

    EXPECT_GL_NO_ERROR();
}

// Access all compute shader special variables.
TEST_P(ComputeShaderTest, AccessAllSpecialVariables)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=4, local_size_y=3, local_size_z=2) in;\n"
        "void main()\n"
        "{\n"
        "    uvec3 temp1 = gl_NumWorkGroups;\n"
        "    uvec3 temp2 = gl_WorkGroupSize;\n"
        "    uvec3 temp3 = gl_WorkGroupID;\n"
        "    uvec3 temp4 = gl_LocalInvocationID;\n"
        "    uvec3 temp5 = gl_GlobalInvocationID;\n"
        "    uint  temp6 = gl_LocalInvocationIndex;\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);
}

// Access part compute shader special variables.
TEST_P(ComputeShaderTest, AccessPartSpecialVariables)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=4, local_size_y=3, local_size_z=2) in;\n"
        "void main()\n"
        "{\n"
        "    uvec3 temp1 = gl_WorkGroupSize;\n"
        "    uvec3 temp2 = gl_WorkGroupID;\n"
        "    uint  temp3 = gl_LocalInvocationIndex;\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);
}

// Use glDispatchCompute to define work group count.
TEST_P(ComputeShaderTest, DispatchCompute)
{
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=4, local_size_y=3, local_size_z=2) in;\n"
        "void main()\n"
        "{\n"
        "    uvec3 temp = gl_NumWorkGroups;\n"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);

    glUseProgram(program.get());
    glDispatchCompute(8, 4, 2);
    EXPECT_GL_NO_ERROR();
}

// Use image uniform to write texture in compute shader, and verify the content is expected.
TEST_P(ComputeShaderTest, BindImageTexture)
{
    ANGLE_SKIP_TEST_IF(IsD3D11());

    GLTexture mTexture[2];
    GLFramebuffer mFramebuffer;
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=2, local_size_y=2, local_size_z=1) in;\n"
        "layout(r32ui, binding = 0) writeonly uniform highp uimage2D uImage[2];"
        "void main()\n"
        "{\n"
        "    imageStore(uImage[0], ivec2(gl_LocalInvocationIndex, gl_WorkGroupID.x), uvec4(100, 0, "
        "0, 0));"
        "    imageStore(uImage[1], ivec2(gl_LocalInvocationIndex, gl_WorkGroupID.x), uvec4(100, 0, "
        "0, 0));"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);
    glUseProgram(program.get());
    int width = 4, height = 2;
    GLuint inputValues[] = {200, 200, 200, 200, 200, 200, 200, 200};

    glBindTexture(GL_TEXTURE_2D, mTexture[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, width, height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_INT,
                    inputValues);
    EXPECT_GL_NO_ERROR();

    glBindImageTexture(0, mTexture[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    EXPECT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTexture[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, width, height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_INT,
                    inputValues);
    EXPECT_GL_NO_ERROR();

    glBindImageTexture(1, mTexture[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    EXPECT_GL_NO_ERROR();

    glDispatchCompute(2, 1, 1);
    EXPECT_GL_NO_ERROR();

    glUseProgram(0);
    GLuint outputValues[2][8];
    GLuint expectedValue = 100;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture[0],
                           0);
    EXPECT_GL_NO_ERROR();
    glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_INT, outputValues[0]);
    EXPECT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture[1],
                           0);
    EXPECT_GL_NO_ERROR();
    glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_INT, outputValues[1]);
    EXPECT_GL_NO_ERROR();

    for (int i = 0; i < width * height; i++)
    {
        EXPECT_EQ(expectedValue, outputValues[0][i]);
        EXPECT_EQ(expectedValue, outputValues[1][i]);
    }
}

// When declare a image array without a binding qualifier, all elements are bound to unit zero.
TEST_P(ComputeShaderTest, ImageArrayWithoutBindingQualifier)
{
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // TODO(xinghua.cao@intel.com): On AMD desktop OpenGL, bind two image variables to unit 0,
    // only one variable is valid.
    ANGLE_SKIP_TEST_IF(IsAMD() && IsDesktopOpenGL());

    GLTexture mTexture;
    GLFramebuffer mFramebuffer;
    const std::string csSource =
        "#version 310 es\n"
        "layout(local_size_x=2, local_size_y=2, local_size_z=1) in;\n"
        "layout(r32ui) writeonly uniform highp uimage2D uImage[2];"
        "void main()\n"
        "{\n"
        "    imageStore(uImage[0], ivec2(gl_LocalInvocationIndex, 0), uvec4(100, 0, 0, 0));"
        "    imageStore(uImage[1], ivec2(gl_LocalInvocationIndex, 1), uvec4(100, 0, 0, 0));"
        "}\n";

    ANGLE_GL_COMPUTE_PROGRAM(program, csSource);
    glUseProgram(program.get());
    constexpr int kTextureWidth = 4, kTextureHeight = 2;
    GLuint inputValues[] = {200, 200, 200, 200, 200, 200, 200, 200};

    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, kTextureWidth, kTextureHeight);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTextureWidth, kTextureHeight, GL_RED_INTEGER,
                    GL_UNSIGNED_INT, inputValues);
    EXPECT_GL_NO_ERROR();

    glBindImageTexture(0, mTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    glDispatchCompute(1, 1, 1);
    EXPECT_GL_NO_ERROR();

    glUseProgram(0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
    GLuint outputValues[8];
    glReadPixels(0, 0, kTextureWidth, kTextureHeight, GL_RED_INTEGER, GL_UNSIGNED_INT,
                 outputValues);
    EXPECT_GL_NO_ERROR();

    GLuint expectedValue = 100;
    for (int i = 0; i < kTextureWidth * kTextureHeight; i++)
    {
        EXPECT_EQ(expectedValue, outputValues[i]);
    }
}

// Check that it is not possible to create a compute shader when the context does not support ES
// 3.10
TEST_P(ComputeShaderTestES3, NotSupported)
{
    GLuint computeShaderHandle = glCreateShader(GL_COMPUTE_SHADER);
    EXPECT_EQ(0u, computeShaderHandle);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

ANGLE_INSTANTIATE_TEST(ComputeShaderTest, ES31_OPENGL(), ES31_OPENGLES(), ES31_D3D11());
ANGLE_INSTANTIATE_TEST(ComputeShaderTestES3, ES3_OPENGL(), ES3_OPENGLES());

}  // namespace
