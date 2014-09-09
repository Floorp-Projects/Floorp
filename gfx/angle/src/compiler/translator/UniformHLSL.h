//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UniformHLSL.h:
//   Methods for GLSL to HLSL translation for uniforms and interface blocks.
//

#ifndef TRANSLATOR_UNIFORMHLSL_H_
#define TRANSLATOR_UNIFORMHLSL_H_

#include "common/shadervars.h"
#include "compiler/translator/Types.h"

namespace sh
{
class StructureHLSL;

class UniformHLSL
{
  public:
    UniformHLSL(StructureHLSL *structureHLSL, ShShaderOutput outputType);

    void reserveUniformRegisters(unsigned int registerCount);
    void reserveInterfaceBlockRegisters(unsigned int registerCount);
    TString uniformsHeader(ShShaderOutput outputType, const ReferencedSymbols &referencedUniforms);
    TString interfaceBlocksHeader(const ReferencedSymbols &referencedInterfaceBlocks);

    // Used for direct index references
    static TString interfaceBlockInstanceString(const TInterfaceBlock& interfaceBlock, unsigned int arrayIndex);

    const std::vector<Uniform> &getUniforms() const { return mActiveUniforms; }
    const std::vector<InterfaceBlock> &getInterfaceBlocks() const { return mActiveInterfaceBlocks; }

  private:
    TString interfaceBlockString(const TInterfaceBlock &interfaceBlock, unsigned int registerIndex, unsigned int arrayIndex);
    TString interfaceBlockMembersString(const TInterfaceBlock &interfaceBlock, TLayoutBlockStorage blockStorage);
    TString interfaceBlockStructString(const TInterfaceBlock &interfaceBlock);

    // Returns the uniform's register index
    int declareUniformAndAssignRegister(const TType &type, const TString &name);
    void declareInterfaceBlockField(const TType &type, const TString &name, std::vector<InterfaceBlockField>& output);
    Uniform declareUniformToList(const TType &type, const TString &name, int registerIndex, std::vector<Uniform> *output);

    unsigned int mUniformRegister;
    unsigned int mInterfaceBlockRegister;
    unsigned int mSamplerRegister;
    StructureHLSL *mStructureHLSL;
    ShShaderOutput mOutputType;

    std::vector<Uniform> mActiveUniforms;
    std::vector<InterfaceBlock> mActiveInterfaceBlocks;
};

}

#endif // TRANSLATOR_UNIFORMHLSL_H_
