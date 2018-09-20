//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the function RewriteAtomicFunctionExpressions.
// See the header for more details.

#include "RewriteAtomicFunctionExpressions.h"

#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
// Traverser that simplifies all the atomic function expressions into the ones that can be directly
// translated into HLSL.
class RewriteAtomicFunctionExpressionsTraverser : public TIntermTraverser
{
  public:
    RewriteAtomicFunctionExpressionsTraverser(TSymbolTable *symbolTable);

    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

  private:
    static bool IsAtomicExchangeOrCompSwapNoReturnValue(TIntermAggregate *node,
                                                        TIntermNode *parentNode);

    void separateAtomicFunctionCallNode(TIntermAggregate *oldAtomicFunctionNode);
};

RewriteAtomicFunctionExpressionsTraverser::RewriteAtomicFunctionExpressionsTraverser(
    TSymbolTable *symbolTable)
    : TIntermTraverser(true, false, false, symbolTable)
{
}

void RewriteAtomicFunctionExpressionsTraverser::separateAtomicFunctionCallNode(
    TIntermAggregate *oldAtomicFunctionNode)
{
    ASSERT(oldAtomicFunctionNode);

    TIntermSequence insertions;

    // Declare a temporary variable
    TIntermDeclaration *returnVariableDeclaration;
    TVariable *returnVariable = DeclareTempVariable(mSymbolTable, &oldAtomicFunctionNode->getType(),
                                                    EvqTemporary, &returnVariableDeclaration);
    insertions.push_back(returnVariableDeclaration);

    // Use this variable as the return value of the atomic function call.
    TIntermBinary *atomicFunctionAssignment = new TIntermBinary(
        TOperator::EOpAssign, CreateTempSymbolNode(returnVariable), oldAtomicFunctionNode);

    insertStatementsInParentBlock(insertions);
    queueReplacement(atomicFunctionAssignment, OriginalNode::IS_DROPPED);
}

bool RewriteAtomicFunctionExpressionsTraverser::IsAtomicExchangeOrCompSwapNoReturnValue(
    TIntermAggregate *node,
    TIntermNode *parentNode)
{
    ASSERT(node);
    return (node->getOp() == EOpAtomicExchange || node->getOp() == EOpAtomicCompSwap) &&
           parentNode && parentNode->getAsBlock();
}

bool RewriteAtomicFunctionExpressionsTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (IsAtomicExchangeOrCompSwapNoReturnValue(node, getParentNode()))
    {
        separateAtomicFunctionCallNode(node);
        return false;
    }

    return true;
}
}  // anonymous namespace

void RewriteAtomicFunctionExpressions(TIntermNode *root, TSymbolTable *symbolTable)
{
    RewriteAtomicFunctionExpressionsTraverser traverser(symbolTable);
    traverser.traverse(root);
    traverser.updateTree();
}
}  // namespace sh