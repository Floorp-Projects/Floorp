//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantFoldingTest.cpp:
//   Utilities for constant folding tests.
//

#include "tests/test_utils/ConstantFoldingTest.h"

#include "angle_gl.h"
#include "compiler/translator/TranslatorESSL.h"
#include "GLSLANG/ShaderLang.h"

using namespace sh;

void ConstantFoldingTest::SetUp()
{
    allocator.push();
    SetGlobalPoolAllocator(&allocator);
    ShBuiltInResources resources;
    InitBuiltInResources(&resources);

    mTranslatorESSL = new TranslatorESSL(GL_FRAGMENT_SHADER, SH_GLES3_SPEC);
    ASSERT_TRUE(mTranslatorESSL->Init(resources));
}

void ConstantFoldingTest::TearDown()
{
    delete mTranslatorESSL;
    SetGlobalPoolAllocator(NULL);
    allocator.pop();
}

void ConstantFoldingTest::compile(const std::string &shaderString)
{
    const char *shaderStrings[] = {shaderString.c_str()};

    mASTRoot = mTranslatorESSL->compileTreeForTesting(shaderStrings, 1, SH_OBJECT_CODE);
    if (!mASTRoot)
    {
        TInfoSink &infoSink = mTranslatorESSL->getInfoSink();
        FAIL() << "Shader compilation into ESSL failed " << infoSink.info.c_str();
    }
}

bool ConstantFoldingTest::hasWarning()
{
    TInfoSink &infoSink = mTranslatorESSL->getInfoSink();
    return infoSink.info.str().find("WARNING:") != std::string::npos;
}

void ConstantFoldingExpressionTest::evaluateFloat(const std::string &floatExpression)
{
    std::stringstream shaderStream;
    shaderStream << "#version 300 es\n"
                    "precision mediump float;\n"
                    "out float my_FragColor;\n"
                    "void main()\n"
                    "{\n"
                 << "    my_FragColor = " << floatExpression << ";\n"
                 << "}\n";
    compile(shaderStream.str());
}
