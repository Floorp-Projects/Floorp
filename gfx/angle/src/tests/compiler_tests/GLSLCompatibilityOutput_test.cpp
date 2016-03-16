//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GLSLCompatibilityOutputTest.cpp
//   Test compiler output for glsl compatibility mode
//

#include "angle_gl.h"
#include "gtest/gtest.h"
#include "GLSLANG/ShaderLang.h"
#include "tests/test_utils/compiler_test.h"

class GLSLCompatibilityOutputTest : public testing::Test
{
  public:
    GLSLCompatibilityOutputTest() {}

  protected:
    void compile(const std::string &shaderString)
    {
        int compilationFlags = SH_VARIABLES;

        std::string infoLog;
        bool compilationSuccess =
            compileTestShader(GL_VERTEX_SHADER, SH_GLES2_SPEC, SH_GLSL_COMPATIBILITY_OUTPUT,
                              shaderString, compilationFlags, &mTranslatedSource, &infoLog);
        if (!compilationSuccess)
        {
            FAIL() << "Shader compilation failed " << infoLog;
        }
    }

    bool find(const char *name) const { return mTranslatedSource.find(name) != std::string::npos; }

  private:
    std::string mTranslatedSource;
};

// Verify gl_Position is written when compiling in compatibility mode
TEST_F(GLSLCompatibilityOutputTest, GLPositionWrittenTest)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "void main() {\n"
        "}";
    compile(shaderString);
    EXPECT_TRUE(find("gl_Position"));
}