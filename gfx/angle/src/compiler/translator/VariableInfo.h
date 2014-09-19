//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_VARIABLE_INFO_H_
#define COMPILER_VARIABLE_INFO_H_

#include <GLSLANG/ShaderLang.h>

#include "compiler/translator/IntermNode.h"

// Traverses intermediate tree to collect all attributes, uniforms, varyings.
class CollectVariables : public TIntermTraverser
{
  public:
    CollectVariables(std::vector<sh::Attribute> *attribs,
                     std::vector<sh::Attribute> *outputVariables,
                     std::vector<sh::Uniform> *uniforms,
                     std::vector<sh::Varying> *varyings,
                     std::vector<sh::InterfaceBlock> *interfaceBlocks,
                     ShHashFunction64 hashFunction);

    virtual void visitSymbol(TIntermSymbol *symbol);
    virtual bool visitAggregate(Visit, TIntermAggregate *node);
    virtual bool visitBinary(Visit visit, TIntermBinary *binaryNode);

  private:
    template <typename VarT>
    void visitVariable(const TIntermSymbol *variable, std::vector<VarT> *infoList) const;

    template <typename VarT>
    void visitInfoList(const TIntermSequence &sequence, std::vector<VarT> *infoList) const;

    std::vector<sh::Attribute> *mAttribs;
    std::vector<sh::Attribute> *mOutputVariables;
    std::vector<sh::Uniform> *mUniforms;
    std::vector<sh::Varying> *mVaryings;
    std::vector<sh::InterfaceBlock> *mInterfaceBlocks;

    std::map<std::string, sh::InterfaceBlockField *> mInterfaceBlockFields;

    bool mPointCoordAdded;
    bool mFrontFacingAdded;
    bool mFragCoordAdded;

    ShHashFunction64 mHashFunction;
};

// Expand struct variables to flattened lists of split variables
template <typename VarT>
void ExpandVariables(const std::vector<VarT> &compact,
                     std::vector<sh::ShaderVariable> *expanded);

#endif  // COMPILER_VARIABLE_INFO_H_
