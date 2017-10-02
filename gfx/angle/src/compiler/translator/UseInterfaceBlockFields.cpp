//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// UseInterfaceBlockFields.cpp: insert statements to reference all members in InterfaceBlock list at
// the beginning of main. This is to work around a Mac driver that treats unused standard/shared
// uniform blocks as inactive.

#include "compiler/translator/UseInterfaceBlockFields.h"

#include "compiler/translator/FindMain.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

void AddFieldUseStatements(const ShaderVariable &var,
                           TIntermSequence *sequence,
                           const TSymbolTable &symbolTable)
{
    TString name = TString(var.name.c_str());
    if (var.isArray())
    {
        size_t pos = name.find_last_of('[');
        if (pos != TString::npos)
        {
            name = name.substr(0, pos);
        }
    }
    TIntermSymbol *symbol = ReferenceGlobalVariable(name, symbolTable);
    if (var.isArray())
    {
        for (unsigned int i = 0u; i < var.arraySize; ++i)
        {
            TIntermBinary *element =
                new TIntermBinary(EOpIndexDirect, symbol->deepCopy(), CreateIndexNode(i));
            sequence->insert(sequence->begin(), element);
        }
    }
    else
    {
        sequence->insert(sequence->begin(), symbol);
    }
}

void InsertUseCode(const InterfaceBlock &block, TIntermTyped *blockNode, TIntermSequence *sequence)
{
    for (unsigned int i = 0; i < block.fields.size(); ++i)
    {
        TIntermBinary *element = new TIntermBinary(EOpIndexDirectInterfaceBlock,
                                                   blockNode->deepCopy(), CreateIndexNode(i));
        sequence->insert(sequence->begin(), element);
    }
}

void InsertUseCode(TIntermSequence *sequence,
                   const InterfaceBlockList &blocks,
                   const TSymbolTable &symbolTable)
{
    for (const auto &block : blocks)
    {
        if (block.instanceName.empty())
        {
            for (const auto &var : block.fields)
            {
                AddFieldUseStatements(var, sequence, symbolTable);
            }
        }
        else if (block.arraySize > 0u)
        {
            TString name(block.instanceName.c_str());
            TIntermSymbol *arraySymbol = ReferenceGlobalVariable(name, symbolTable);
            for (unsigned int i = 0u; i < block.arraySize; ++i)
            {
                TIntermBinary *elementSymbol =
                    new TIntermBinary(EOpIndexDirect, arraySymbol->deepCopy(), CreateIndexNode(i));
                InsertUseCode(block, elementSymbol, sequence);
            }
        }
        else
        {
            TString name(block.instanceName.c_str());
            TIntermSymbol *blockSymbol = ReferenceGlobalVariable(name, symbolTable);
            InsertUseCode(block, blockSymbol, sequence);
        }
    }
}

}  // namespace anonymous

void UseInterfaceBlockFields(TIntermBlock *root,
                             const InterfaceBlockList &blocks,
                             const TSymbolTable &symbolTable)
{
    TIntermBlock *mainBody = FindMainBody(root);
    InsertUseCode(mainBody->getSequence(), blocks, symbolTable);
}

}  // namespace sh
