//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ResourcesHLSL.h:
//   Methods for GLSL to HLSL translation for uniforms and interface blocks.
//

#ifndef COMPILER_TRANSLATOR_RESOURCESHLSL_H_
#define COMPILER_TRANSLATOR_RESOURCESHLSL_H_

#include "compiler/translator/OutputHLSL.h"
#include "compiler/translator/UtilsHLSL.h"

namespace sh
{
class ImmutableString;
class StructureHLSL;
class TSymbolTable;

class ResourcesHLSL : angle::NonCopyable
{
  public:
    ResourcesHLSL(StructureHLSL *structureHLSL,
                  ShShaderOutput outputType,
                  const std::vector<Uniform> &uniforms,
                  unsigned int firstUniformRegister);

    void reserveUniformRegisters(unsigned int registerCount);
    void reserveUniformBlockRegisters(unsigned int registerCount);
    void uniformsHeader(TInfoSinkBase &out,
                        ShShaderOutput outputType,
                        const ReferencedVariables &referencedUniforms,
                        TSymbolTable *symbolTable);

    // Must be called after uniformsHeader
    void samplerMetadataUniforms(TInfoSinkBase &out, const char *reg);

    TString uniformBlocksHeader(const ReferencedInterfaceBlocks &referencedInterfaceBlocks);

    // Used for direct index references
    static TString UniformBlockInstanceString(const ImmutableString &instanceName,
                                              unsigned int arrayIndex);

    const std::map<std::string, unsigned int> &getUniformBlockRegisterMap() const
    {
        return mUniformBlockRegisterMap;
    }
    const std::map<std::string, unsigned int> &getUniformRegisterMap() const
    {
        return mUniformRegisterMap;
    }

  private:
    TString uniformBlockString(const TInterfaceBlock &interfaceBlock,
                               const TVariable *instanceVariable,
                               unsigned int registerIndex,
                               unsigned int arrayIndex);
    TString uniformBlockMembersString(const TInterfaceBlock &interfaceBlock,
                                      TLayoutBlockStorage blockStorage);
    TString uniformBlockStructString(const TInterfaceBlock &interfaceBlock);
    const Uniform *findUniformByName(const ImmutableString &name) const;

    void outputHLSL4_0_FL9_3Sampler(TInfoSinkBase &out,
                                    const TType &type,
                                    const TVariable &variable,
                                    const unsigned int registerIndex);
    void outputUniform(TInfoSinkBase &out,
                       const TType &type,
                       const TVariable &variable,
                       const unsigned int registerIndex);

    // Returns the uniform's register index
    unsigned int assignUniformRegister(const TType &type,
                                       const ImmutableString &name,
                                       unsigned int *outRegisterCount);
    unsigned int assignSamplerInStructUniformRegister(const TType &type,
                                                      const TString &name,
                                                      unsigned int *outRegisterCount);

    void outputHLSLSamplerUniformGroup(
        TInfoSinkBase &out,
        const HLSLTextureGroup textureGroup,
        const TVector<const TVariable *> &group,
        const TMap<const TVariable *, TString> &samplerInStructSymbolsToAPINames,
        unsigned int *groupTextureRegisterIndex);

    void outputHLSLImageUniformIndices(TInfoSinkBase &out,
                                       const TVector<const TVariable *> &group,
                                       unsigned int imageArrayIndex,
                                       unsigned int *groupRegisterCount);
    void outputHLSLReadonlyImageUniformGroup(TInfoSinkBase &out,
                                             const HLSLTextureGroup textureGroup,
                                             const TVector<const TVariable *> &group,
                                             unsigned int *groupTextureRegisterIndex,
                                             unsigned int *imageUniformGroupIndex);
    void outputHLSLImageUniformGroup(TInfoSinkBase &out,
                                     const HLSLRWTextureGroup textureGroup,
                                     const TVector<const TVariable *> &group,
                                     unsigned int *groupTextureRegisterIndex,
                                     unsigned int *imageUniformGroupIndex);

    unsigned int mUniformRegister;
    unsigned int mUniformBlockRegister;
    unsigned int mTextureRegister;
    unsigned int mRWTextureRegister;
    unsigned int mSamplerCount;
    StructureHLSL *mStructureHLSL;
    ShShaderOutput mOutputType;

    const std::vector<Uniform> &mUniforms;
    std::map<std::string, unsigned int> mUniformBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformRegisterMap;
};
}

#endif  // COMPILER_TRANSLATOR_RESOURCESHLSL_H_
