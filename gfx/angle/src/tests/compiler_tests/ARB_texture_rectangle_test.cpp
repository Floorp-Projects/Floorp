//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ARB_texture_rectangle_test.cpp:
//   Test for the ARB_texture_rectangle extension
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "gtest/gtest.h"
#include "tests/test_utils/ShaderCompileTreeTest.h"

using namespace sh;

class ARBTextureRectangleTestNoExt : public ShaderCompileTreeTest
{
  protected:
    ::GLenum getShaderType() const override { return GL_FRAGMENT_SHADER; }
    ShShaderSpec getShaderSpec() const override { return SH_GLES3_SPEC; }
};

class ARBTextureRectangleTest : public ARBTextureRectangleTestNoExt
{
  protected:
    void initResources(ShBuiltInResources *resources) override
    {
        resources->ARB_texture_rectangle = 1;
    }
};

// Check that new types and builtins are disallowed if the extension isn't present in the translator
// resources
TEST_F(ARBTextureRectangleTest, NewTypeAndBuiltinsWithoutTranslatorResourceExtension)
{
    // The new builtins require Sampler2DRect so we can't test them independently.
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform sampler2DRect tex;\n"
        "void main() {\n"
        "}\n";
    ASSERT_TRUE(compile(shaderString));
}

// Check that new types and builtins are usable even with the #extension directive
// Issue #15 of ARB_texture_rectangle explains that the extension was specified before the
// #extension mechanism was in place so it doesn't require explicit enabling.
TEST_F(ARBTextureRectangleTest, NewTypeAndBuiltinsWithoutExtensionDirective)
{
    const std::string &shaderString =
        "precision mediump float;\n"
        "uniform sampler2DRect tex;\n"
        "void main() {\n"
        "    vec4 color = texture2DRect(tex, vec2(1.0));"
        "    color = texture2DRectProj(tex, vec3(1.0));"
        "    color = texture2DRectProj(tex, vec4(1.0));"
        "}\n";
    ASSERT_TRUE(compile(shaderString));
}

// Test valid usage of the new types and builtins
TEST_F(ARBTextureRectangleTest, NewTypeAndBuiltingsWithExtensionDirective)
{
    const std::string &shaderString =
        "#extension GL_ARB_texture_rectangle : require\n"
        "precision mediump float;\n"
        "uniform sampler2DRect tex;\n"
        "void main() {\n"
        "    vec4 color = texture2DRect(tex, vec2(1.0));"
        "    color = texture2DRectProj(tex, vec3(1.0));"
        "    color = texture2DRectProj(tex, vec4(1.0));"
        "}\n";
    ASSERT_TRUE(compile(shaderString));
}

// Check that it is not possible to pass a sampler2DRect where sampler2D is expected, and vice versa
TEST_F(ARBTextureRectangleTest, Rect2DVs2DMismatch)
{
    const std::string &shaderString1 =
        "#extension GL_ARB_texture_rectangle : require\n"
        "precision mediump float;\n"
        "uniform sampler2DRect tex;\n"
        "void main() {\n"
        "    vec4 color = texture2D(tex, vec2(1.0));"
        "}\n";
    ASSERT_FALSE(compile(shaderString1));

    const std::string &shaderString2 =
        "#extension GL_ARB_texture_rectangle : require\n"
        "precision mediump float;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    vec4 color = texture2DRect(tex, vec2(1.0));"
        "}\n";
    ASSERT_FALSE(compile(shaderString2));
}
