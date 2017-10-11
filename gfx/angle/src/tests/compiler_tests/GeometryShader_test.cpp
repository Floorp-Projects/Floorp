//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GeometryShader_test.cpp:
// tests for compiling a Geometry Shader
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/TranslatorESSL.h"
#include "gtest/gtest.h"
#include "tests/test_utils/compiler_test.h"

using namespace sh;

class GeometryShaderTest : public testing::Test
{
  public:
    GeometryShaderTest() {}

  protected:
    void SetUp() override
    {
        ShBuiltInResources resources;
        InitBuiltInResources(&resources);
        resources.OES_geometry_shader = 1;

        mTranslator = new TranslatorESSL(GL_GEOMETRY_SHADER_OES, SH_GLES3_1_SPEC);
        ASSERT_TRUE(mTranslator->Init(resources));
    }

    void TearDown() override { SafeDelete(mTranslator); }

    // Return true when compilation succeeds
    bool compile(const std::string &shaderString)
    {
        const char *shaderStrings[] = {shaderString.c_str()};

        bool status         = mTranslator->compile(shaderStrings, 1, SH_OBJECT_CODE | SH_VARIABLES);
        TInfoSink &infoSink = mTranslator->getInfoSink();
        mInfoLog            = infoSink.info.c_str();
        return status;
    }

    bool compileGeometryShader(const std::string &statement1, const std::string &statement2)
    {
        std::ostringstream sstream;
        sstream << kHeader << statement1 << statement2 << kEmptyBody;
        return compile(sstream.str());
    }

    bool compileGeometryShader(const std::string &statement1,
                               const std::string &statement2,
                               const std::string &statement3,
                               const std::string &statement4)
    {
        std::ostringstream sstream;
        sstream << kHeader << statement1 << statement2 << statement3 << statement4 << kEmptyBody;
        return compile(sstream.str());
    }

    static std::string GetGeometryShaderLayout(const std::string &layoutType,
                                               const std::string &primitive,
                                               int invocations,
                                               int maxVertices)
    {
        std::ostringstream sstream;

        sstream << "layout (";
        if (!primitive.empty())
        {
            sstream << primitive;
        }
        if (invocations > 0)
        {
            sstream << ", invocations = " << invocations;
        }
        if (maxVertices >= 0)
        {
            sstream << ", max_vertices = " << maxVertices;
        }
        sstream << ") " << layoutType << ";" << std::endl;

        return sstream.str();
    }

    const std::string kHeader =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n";
    const std::string kEmptyBody =
        "void main()\n"
        "{\n"
        "}\n";
    const std::string kInputLayout  = "layout (points) in;\n";
    const std::string kOutputLayout = "layout (points, max_vertices = 1) out;\n";

    std::string mInfoLog;
    TranslatorESSL *mTranslator = nullptr;
};

// Geometry Shaders are not supported in GLSL ES shaders version lower than 310.
TEST_F(GeometryShaderTest, Version300)
{
    const std::string &shaderString =
        "#version 300 es\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders are not supported in GLSL ES shaders version 310 without extension
// OES_geometry_shader enabled.
TEST_F(GeometryShaderTest, Version310WithoutExtension)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders are supported in GLSL ES shaders version 310 with OES_geometry_shader enabled.
TEST_F(GeometryShaderTest, Version310WithOESExtension)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Missing the declaration of input primitive in a geometry shader should be a link error instead of
// a compile error.
TEST_F(GeometryShaderTest, NoInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout(points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders can only support 5 kinds of input primitives, which cannot be used as output
// primitives except 'points'.
// Skip testing "points" as it can be used as both input and output primitives.
TEST_F(GeometryShaderTest, ValidInputPrimitives)
{
    const std::array<std::string, 4> kInputPrimitives = {
        {"lines", "lines_adjacency", "triangles", "triangles_adjacency"}};

    for (const std::string &inputPrimitive : kInputPrimitives)
    {
        if (!compileGeometryShader(GetGeometryShaderLayout("in", inputPrimitive, -1, -1),
                                   kOutputLayout))
        {
            FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
        }
        if (compileGeometryShader(kInputLayout,
                                  GetGeometryShaderLayout("out", inputPrimitive, -1, 6)))
        {
            FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
        }
    }
}

// Geometry Shaders allow duplicated declaration of input primitive, but all of them must be same.
TEST_F(GeometryShaderTest, RedeclareInputPrimitives)
{
    const std::array<std::string, 5> kInputPrimitives = {
        {"points", "lines", "lines_adjacency", "triangles", "triangles_adjacency"}};

    for (GLuint i = 0; i < kInputPrimitives.size(); ++i)
    {
        const std::string &inputLayoutStr1 =
            GetGeometryShaderLayout("in", kInputPrimitives[i], -1, -1);
        if (!compileGeometryShader(inputLayoutStr1, inputLayoutStr1, kOutputLayout, ""))
        {
            FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
        }

        for (GLuint j = i + 1; j < kInputPrimitives.size(); ++j)
        {
            const std::string &inputLayoutStr2 =
                GetGeometryShaderLayout("in", kInputPrimitives[j], -1, -1);
            if (compileGeometryShader(inputLayoutStr1, inputLayoutStr2, kOutputLayout, ""))
            {
                FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
            }
        }
    }
}

// Geometry Shaders don't allow declaring different input primitives in one layout.
TEST_F(GeometryShaderTest, DeclareDifferentInputPrimitivesInOneLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, triangles) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow 'invocations' < 1.
TEST_F(GeometryShaderTest, InvocationsLessThanOne)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 0) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders allow declaring 'invocations' == 1 together with input primitive declaration in
// one layout.
TEST_F(GeometryShaderTest, InvocationsEqualsOne)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 1) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow declaring 'invocations' == 1 in an individual layout.
TEST_F(GeometryShaderTest, InvocationsEqualsOneInSeparatedLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (invocations = 1) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow 'invocations' larger than the implementation-dependent maximum value
// (32 in this test).
TEST_F(GeometryShaderTest, TooLargeInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 9989899) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders allow 'invocations' declared together with input primitives in one layout.
TEST_F(GeometryShaderTest, InvocationsDeclaredWithInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 3) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow 'invocations' declared before input primitives in one input layout.
TEST_F(GeometryShaderTest, InvocationsBeforeInputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (invocations = 3, points) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow 'invocations' declared in an individual input layout.
TEST_F(GeometryShaderTest, InvocationsInIndividualLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (invocations = 3) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow duplicated 'invocations' declarations.
TEST_F(GeometryShaderTest, DuplicatedInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 3) in;\n"
        "layout (invocations = 3) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow multiple different 'invocations' declarations in different
// layouts.
TEST_F(GeometryShaderTest, RedeclareDifferentInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 3) in;\n"
        "layout (invocations = 5) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow multiple different 'invocations' declarations in different
// layouts.
TEST_F(GeometryShaderTest, RedeclareDifferentInvocationsAfterInvocationEqualsOne)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 1) in;\n"
        "layout (invocations = 5) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow multiple different 'invocations' declarations in one layout.
TEST_F(GeometryShaderTest, RedeclareDifferentInvocationsInOneLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 3, invocations = 5) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow 'invocations' in out layouts.
TEST_F(GeometryShaderTest, DeclareInvocationsInOutLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, invocations = 3, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow 'invocations' in layouts without 'in' qualifier.
TEST_F(GeometryShaderTest, DeclareInvocationsInLayoutNoQualifier)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (invocations = 3);\n"
        "layout (points, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders allow declaring output primitive before input primitive declaration.
TEST_F(GeometryShaderTest, DeclareOutputPrimitiveBeforeInputPrimitiveDeclare)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, max_vertices = 1) out;\n"
        "layout (points) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow declaring 'max_vertices' before output primitive in one output layout.
TEST_F(GeometryShaderTest, DeclareMaxVerticesBeforeOutputPrimitive)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (max_vertices = 1, points) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Missing the declaration of output primitive should be a link error instead of a compile error in
// a geometry shader.
TEST_F(GeometryShaderTest, NoOutputPrimitives)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders can only support 3 kinds of output primitives, which cannot be used as input
// primitives except 'points'.
// Skip testing "points" as it can be used as both input and output primitives.
TEST_F(GeometryShaderTest, ValidateOutputPrimitives)
{
    const std::string outputPrimitives[] = {"line_strip", "triangle_strip"};

    for (const std::string &outPrimitive : outputPrimitives)
    {
        if (!compileGeometryShader(kInputLayout,
                                   GetGeometryShaderLayout("out", outPrimitive, -1, 6)))
        {
            FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
        }

        if (compileGeometryShader(GetGeometryShaderLayout("in", outPrimitive, -1, -1),
                                  kOutputLayout))
        {
            FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
        }
    }
}

// Geometry Shaders allow duplicated output primitive declarations, but all of them must be same.
TEST_F(GeometryShaderTest, RedeclareOutputPrimitives)
{
    const std::array<std::string, 3> outPrimitives = {{"points", "line_strip", "triangle_strip"}};

    for (GLuint i = 0; i < outPrimitives.size(); i++)
    {
        constexpr int maxVertices = 1;
        const std::string &outputLayoutStr1 =
            GetGeometryShaderLayout("out", outPrimitives[i], -1, maxVertices);
        const std::string &outputLayoutStr2 =
            GetGeometryShaderLayout("out", outPrimitives[i], -1, -1);
        if (!compileGeometryShader(kInputLayout, outputLayoutStr1, outputLayoutStr2, ""))
        {
            FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
        }
        for (GLuint j = i + 1; j < outPrimitives.size(); j++)
        {
            const std::string &outputLayoutStr3 =
                GetGeometryShaderLayout("out", outPrimitives[j], -1, -1);
            if (compileGeometryShader(kInputLayout, outputLayoutStr1, outputLayoutStr3, ""))
            {
                FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
            }
        }
    }
}

// Geometry Shaders don't allow declaring different output primitives in one layout.
TEST_F(GeometryShaderTest, RedeclareDifferentOutputPrimitivesInOneLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 3, line_strip) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Missing the declarations of output primitives and 'max_vertices' in a geometry shader should
// be a link error instead of a compile error.
TEST_F(GeometryShaderTest, NoOutLayouts)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Missing the declarations of 'max_vertices' in a geometry shader should be a link error
// instead of a compile error.
TEST_F(GeometryShaderTest, NoMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders cannot declare a negative 'max_vertices'.
TEST_F(GeometryShaderTest, NegativeMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = -1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders allow max_vertices == 0.
TEST_F(GeometryShaderTest, ZeroMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 0) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders cannot declare a 'max_vertices' that is greater than
// MAX_GEOMETRY_OUTPUT_VERTICES_EXT (256 in this test).
TEST_F(GeometryShaderTest, TooLargeMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 257) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders can declare 'max_vertices' in an individual out layout.
TEST_F(GeometryShaderTest, MaxVerticesInIndividualLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points) out;\n"
        "layout (max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders allow duplicated 'max_vertices' declarations.
TEST_F(GeometryShaderTest, DuplicatedMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "layout (max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow declaring different 'max_vertices'.
TEST_F(GeometryShaderTest, RedeclareDifferentMaxVertices)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 1) out;\n"
        "layout (max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow declaring different 'max_vertices'.
TEST_F(GeometryShaderTest, RedeclareDifferentMaxVerticesInOneLayout)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2, max_vertices = 1) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow 'location' declared with input/output primitives in one layout.
TEST_F(GeometryShaderTest, invalidLocation)
{
    const std::string &shaderString1 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, location = 1) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString2 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (invocations = 2, location = 1) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString3 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, location = 3, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString4 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points) out;\n"
        "layout (max_vertices = 2, location = 3) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString1) || compile(shaderString2) || compile(shaderString3) ||
        compile(shaderString4))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Geometry Shaders don't allow invalid layout qualifier declarations.
TEST_F(GeometryShaderTest, invalidLayoutQualifiers)
{
    const std::string &shaderString1 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, abc) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString2 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, abc, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString3 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, xyz = 2) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    const std::string &shaderString4 =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points) out;\n"
        "layout (max_vertices = 2, xyz = 3) out;\n"
        "void main()\n"
        "{\n"
        "}\n";

    if (compile(shaderString1) || compile(shaderString2) || compile(shaderString3) ||
        compile(shaderString4))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that indexing an array with a constant integer on gl_in is legal.
TEST_F(GeometryShaderTest, IndexGLInByConstantInteger)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    vec4 position;\n"
        "    position = gl_in[0].gl_Position;\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Verify that indexing an array with an integer variable on gl_in is legal.
TEST_F(GeometryShaderTest, IndexGLInByVariable)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (lines) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    vec4 position;\n"
        "    for (int i = 0; i < 2; i++)\n"
        "    {\n"
        "        position = gl_in[i].gl_Position;\n"
        "    }\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Verify that indexing an array on gl_in without input primitive declaration causes a compile
// error.
TEST_F(GeometryShaderTest, IndexGLInWithoutInputPrimitive)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    vec4 position = gl_in[0].gl_Position;\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that using gl_in.length() without input primitive declaration causes a compile error.
TEST_F(GeometryShaderTest, UseGLInLengthWithoutInputPrimitive)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    int length = gl_in.length();\n"
        "}\n";

    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that using gl_in.length() with input primitive declaration can compile.
TEST_F(GeometryShaderTest, UseGLInLengthWithInputPrimitive)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    int length = gl_in.length();\n"
        "}\n";

    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Verify that gl_in[].gl_Position cannot be l-value.
TEST_F(GeometryShaderTest, AssignValueToGLIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    gl_in[0].gl_Position = vec4(0, 0, 0, 1);\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify Geometry Shader supports all required built-in variables.
TEST_F(GeometryShaderTest, BuiltInVariables)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 2) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    int invocation = gl_InvocationID;\n"
        "    gl_Layer = invocation;\n"
        "    int primitiveIn = gl_PrimitiveIDIn;\n"
        "    gl_PrimitiveID = primitiveIn;\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Verify that gl_PrimitiveIDIn cannot be l-value.
TEST_F(GeometryShaderTest, AssignValueToGLPrimitiveIn)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 2) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    gl_PrimitiveIDIn = 1;\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that gl_InvocationID cannot be l-value.
TEST_F(GeometryShaderTest, AssignValueToGLInvocations)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points, invocations = 2) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    gl_InvocationID = 1;\n"
        "}\n";
    if (compile(shaderString))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that both EmitVertex() and EndPrimitive() are supported in Geometry Shader.
TEST_F(GeometryShaderTest, GeometryShaderBuiltInFunctions)
{
    const std::string &shaderString =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = gl_in[0].gl_Position;\n"
        "    EmitVertex();\n"
        "    EndPrimitive();\n"
        "}\n";
    if (!compile(shaderString))
    {
        FAIL() << "Shader compilation failed, expecting success:\n" << mInfoLog;
    }
}

// Verify that using EmitVertex() or EndPrimitive() without GL_OES_geometry_shader declared causes a
// compile error.
TEST_F(GeometryShaderTest, GeometryShaderBuiltInFunctionsWithoutExtension)
{
    const std::string &shaderString1 =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "    EmitVertex();\n"
        "}\n";

    const std::string &shaderString2 =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "    EndPrimitive();\n"
        "}\n";

    if (compile(shaderString1) || compile(shaderString2))
    {
        FAIL() << "Shader compilation succeeded, expecting failure:\n" << mInfoLog;
    }
}

// Verify that all required built-in constant values are supported in Geometry Shaders
TEST_F(GeometryShaderTest, GeometryShaderBuiltInConstants)
{
    const std::string &kShaderHeader =
        "#version 310 es\n"
        "#extension GL_OES_geometry_shader : require\n"
        "layout (points) in;\n"
        "layout (points, max_vertices = 2) out;\n"
        "void main()\n"
        "{\n"
        "    int val = ";

    const std::array<std::string, 9> kGeometryShaderBuiltinConstants = {{
        "gl_MaxGeometryInputComponents", "gl_MaxGeometryOutputComponents",
        "gl_MaxGeometryImageUniforms", "gl_MaxGeometryTextureImageUnits",
        "gl_MaxGeometryOutputVertices", "gl_MaxGeometryTotalOutputComponents",
        "gl_MaxGeometryUniformComponents", "gl_MaxGeometryAtomicCounters",
        "gl_MaxGeometryAtomicCounterBuffers",
    }};

    const std::string &kShaderTail =
        ";\n"
        "}\n";

    for (const std::string &kGSBuiltinConstant : kGeometryShaderBuiltinConstants)
    {
        std::ostringstream ostream;
        ostream << kShaderHeader << kGSBuiltinConstant << kShaderTail;
        if (!compile(ostream.str()))
        {
            FAIL() << "Shader compilation failed, expecting success: \n" << mInfoLog;
        }
    }
}

// Verify that using any Geometry Shader built-in constant values without GL_OES_geometry_shader
// declared causes a compile error.
TEST_F(GeometryShaderTest, GeometryShaderBuiltInConstantsWithoutExtension)
{
    const std::string &kShaderHeader =
        "#version 310 es\n"
        "void main()\n"
        "{\n"
        "    int val = ";

    const std::array<std::string, 9> kGeometryShaderBuiltinConstants = {{
        "gl_MaxGeometryInputComponents", "gl_MaxGeometryOutputComponents",
        "gl_MaxGeometryImageUniforms", "gl_MaxGeometryTextureImageUnits",
        "gl_MaxGeometryOutputVertices", "gl_MaxGeometryTotalOutputComponents",
        "gl_MaxGeometryUniformComponents", "gl_MaxGeometryAtomicCounters",
        "gl_MaxGeometryAtomicCounterBuffers",
    }};

    const std::string &kShaderTail =
        ";\n"
        "}\n";

    for (const std::string &kGSBuiltinConstant : kGeometryShaderBuiltinConstants)
    {
        std::ostringstream ostream;
        ostream << kShaderHeader << kGSBuiltinConstant << kShaderTail;
        if (compile(ostream.str()))
        {
            FAIL() << "Shader compilation succeeded, expecting failure: \n" << mInfoLog;
        }
    }
}
