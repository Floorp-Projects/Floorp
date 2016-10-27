//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of texelFetchOffset translation issue workaround.
// See header for more info.

#include "compiler/translator/RewriteTexelFetchOffset.h"

#include "common/angleutils.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

class Traverser : public TIntermTraverser
{
  public:
    static void Apply(TIntermNode *root,
                      unsigned int *tempIndex,
                      const TSymbolTable &symbolTable,
                      int shaderVersion);

  private:
    Traverser(const TSymbolTable &symbolTable, int shaderVersion);
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    void nextIteration();

    const TSymbolTable *symbolTable;
    const int shaderVersion;
    bool mFound = false;
};

Traverser::Traverser(const TSymbolTable &symbolTable, int shaderVersion)
    : TIntermTraverser(true, false, false), symbolTable(&symbolTable), shaderVersion(shaderVersion)
{
}

// static
void Traverser::Apply(TIntermNode *root,
                      unsigned int *tempIndex,
                      const TSymbolTable &symbolTable,
                      int shaderVersion)
{
    Traverser traverser(symbolTable, shaderVersion);
    traverser.useTemporaryIndex(tempIndex);
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.mFound)
        {
            traverser.updateTree();
        }
    } while (traverser.mFound);
}

void Traverser::nextIteration()
{
    mFound = false;
    nextTemporaryIndex();
}

bool Traverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (mFound)
    {
        return false;
    }

    // Decide if the node represents the call of texelFetchOffset.
    if (node->getOp() != EOpFunctionCall || node->isUserDefined())
    {
        return true;
    }

    if (node->getName().compare(0, 16, "texelFetchOffset") != 0)
    {
        return true;
    }

    // Potential problem case detected, apply workaround.
    const TIntermSequence *sequence = node->getSequence();
    ASSERT(sequence->size() == 4u);
    nextTemporaryIndex();

    // Decide if there is a 2DArray sampler.
    bool is2DArray = node->getName().find("s2a1") != TString::npos;

    // Create new argument list from node->getName().
    // e.g. Get "(is2a1;vi3;i1;" from "texelFetchOffset(is2a1;vi3;i1;vi2;"
    TString newArgs           = node->getName().substr(16, node->getName().length() - 20);
    TString newName           = "texelFetch" + newArgs;
    TSymbol *texelFetchSymbol = symbolTable->findBuiltIn(newName, shaderVersion);
    ASSERT(texelFetchSymbol);
    int uniqueId = texelFetchSymbol->getUniqueId();

    // Create new node that represents the call of function texelFetch.
    TIntermAggregate *texelFetchNode = new TIntermAggregate(EOpFunctionCall);
    texelFetchNode->setName(newName);
    texelFetchNode->setFunctionId(uniqueId);
    texelFetchNode->setType(node->getType());
    texelFetchNode->setLine(node->getLine());

    // Create argument List of texelFetch(sampler, Position+offset, lod).
    TIntermSequence newsequence;

    // sampler
    newsequence.push_back(sequence->at(0));

    // Position+offset
    TIntermBinary *add = new TIntermBinary(EOpAdd);
    add->setType(node->getType());
    // Position
    TIntermTyped *texCoordNode = sequence->at(1)->getAsTyped();
    ASSERT(texCoordNode);
    add->setLine(texCoordNode->getLine());
    add->setType(texCoordNode->getType());
    add->setLeft(texCoordNode);
    // offset
    ASSERT(sequence->at(3)->getAsTyped());
    if (is2DArray)
    {
        // For 2DArray samplers, Position is ivec3 and offset is ivec2;
        // So offset must be converted into an ivec3 before being added to Position.
        TIntermAggregate *constructIVec3Node = new TIntermAggregate(EOpConstructIVec3);
        constructIVec3Node->setLine(texCoordNode->getLine());
        constructIVec3Node->setType(texCoordNode->getType());

        TIntermSequence ivec3Sequence;
        ivec3Sequence.push_back(sequence->at(3)->getAsTyped());

        TConstantUnion *zero = new TConstantUnion();
        zero->setIConst(0);
        TType *intType = new TType(EbtInt);

        TIntermConstantUnion *zeroNode = new TIntermConstantUnion(zero, *intType);
        ivec3Sequence.push_back(zeroNode);
        constructIVec3Node->insertChildNodes(0, ivec3Sequence);

        add->setRight(constructIVec3Node);
    }
    else
    {
        add->setRight(sequence->at(3)->getAsTyped());
    }
    newsequence.push_back(add);

    // lod
    newsequence.push_back(sequence->at(2));
    texelFetchNode->insertChildNodes(0, newsequence);

    // Replace the old node by this new node.
    queueReplacement(node, texelFetchNode, OriginalNode::IS_DROPPED);
    mFound = true;
    return false;
}

}  // anonymous namespace

void RewriteTexelFetchOffset(TIntermNode *root,
                             unsigned int *tempIndex,
                             const TSymbolTable &symbolTable,
                             int shaderVersion)
{
    // texelFetchOffset is only valid in GLSL 3.0 and later.
    if (shaderVersion < 300)
        return;

    Traverser::Apply(root, tempIndex, symbolTable, shaderVersion);
}

}  // namespace sh