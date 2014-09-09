//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATORHLSL_H_
#define COMPILER_TRANSLATORHLSL_H_

#include "compiler/translator/Compiler.h"
#include "common/shadervars.h"

class TranslatorHLSL : public TCompiler {
public:
    TranslatorHLSL(ShShaderType type, ShShaderSpec spec, ShShaderOutput output);

    virtual TranslatorHLSL *getAsTranslatorHLSL() { return this; }
    const std::vector<sh::Uniform> &getUniforms() { return mActiveUniforms; }
    const std::vector<sh::InterfaceBlock> &getInterfaceBlocks() const { return mActiveInterfaceBlocks; }
    const std::vector<sh::Attribute> &getOutputVariables() { return mActiveOutputVariables; }
    const std::vector<sh::Attribute> &getAttributes() { return mActiveAttributes; }
    const std::vector<sh::Varying> &getVaryings() { return mActiveVaryings; }

protected:
    virtual void translate(TIntermNode* root);

    std::vector<sh::Uniform> mActiveUniforms;
    std::vector<sh::InterfaceBlock> mActiveInterfaceBlocks;
    std::vector<sh::Attribute> mActiveOutputVariables;
    std::vector<sh::Attribute> mActiveAttributes;
    std::vector<sh::Varying> mActiveVaryings;
};

#endif  // COMPILER_TRANSLATORHLSL_H_
