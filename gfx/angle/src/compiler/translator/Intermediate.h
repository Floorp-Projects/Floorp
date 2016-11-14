//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_INTERMEDIATE_H_
#define COMPILER_TRANSLATOR_INTERMEDIATE_H_

#include "compiler/translator/IntermNode.h"

struct TVectorFields
{
    int offsets[4];
    int num;
};

//
// Set of helper functions to help build the tree.
//
class TIntermediate
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TIntermediate() {}

    TIntermSymbol *addSymbol(
        int id, const TString &, const TType &, const TSourceLoc &);
    TIntermTyped *addIndex(TOperator op,
                           TIntermTyped *base,
                           TIntermTyped *index,
                           const TSourceLoc &line,
                           TDiagnostics *diagnostics);
    TIntermTyped *addUnaryMath(
        TOperator op, TIntermTyped *child, const TSourceLoc &line, const TType *funcReturnType);
    TIntermAggregate *growAggregate(
        TIntermNode *left, TIntermNode *right, const TSourceLoc &);
    TIntermAggregate *makeAggregate(TIntermNode *node, const TSourceLoc &);
    TIntermAggregate *ensureSequence(TIntermNode *node);
    TIntermAggregate *setAggregateOperator(TIntermNode *, TOperator, const TSourceLoc &);
    TIntermNode *addSelection(TIntermTyped *cond, TIntermNodePair code, const TSourceLoc &line);
    static TIntermTyped *AddTernarySelection(TIntermTyped *cond,
                                             TIntermTyped *trueExpression,
                                             TIntermTyped *falseExpression,
                                             const TSourceLoc &line);
    TIntermSwitch *addSwitch(
        TIntermTyped *init, TIntermAggregate *statementList, const TSourceLoc &line);
    TIntermCase *addCase(
        TIntermTyped *condition, const TSourceLoc &line);
    TIntermTyped *addComma(TIntermTyped *left,
                           TIntermTyped *right,
                           const TSourceLoc &line,
                           int shaderVersion);
    TIntermConstantUnion *addConstantUnion(const TConstantUnion *constantUnion,
                                           const TType &type,
                                           const TSourceLoc &line);
    TIntermNode *addLoop(TLoopType, TIntermNode *, TIntermTyped *, TIntermTyped *,
                         TIntermNode *, const TSourceLoc &);
    TIntermBranch *addBranch(TOperator, const TSourceLoc &);
    TIntermBranch *addBranch(TOperator, TIntermTyped *, const TSourceLoc &);
    TIntermTyped *addSwizzle(TVectorFields &, const TSourceLoc &);
    static TIntermAggregate *PostProcess(TIntermNode *root);

    static void outputTree(TIntermNode *, TInfoSinkBase &);

    TIntermTyped *foldAggregateBuiltIn(TIntermAggregate *aggregate, TDiagnostics *diagnostics);

  private:
    void operator=(TIntermediate &); // prevent assignments
};

#endif  // COMPILER_TRANSLATOR_INTERMEDIATE_H_
