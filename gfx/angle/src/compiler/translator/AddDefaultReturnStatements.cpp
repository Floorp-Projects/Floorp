//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AddDefaultReturnStatements.cpp: Add default return statements to functions that do not end in a
//                                 return.
//

#include "compiler/translator/AddDefaultReturnStatements.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

class AddDefaultReturnStatementsTraverser : private TIntermTraverser
{
  public:
    static void Apply(TIntermNode *root)
    {
        AddDefaultReturnStatementsTraverser separateInit;
        root->traverse(&separateInit);
        separateInit.updateTree();
    }

  private:
    AddDefaultReturnStatementsTraverser() : TIntermTraverser(true, false, false) {}

    static bool IsFunctionWithoutReturnStatement(TIntermAggregate *node, TType *returnType)
    {
        *returnType = node->getType();
        if (node->getOp() != EOpFunction || node->getType().getBasicType() == EbtVoid)
        {
            return false;
        }

        TIntermAggregate *lastNode = node->getSequence()->back()->getAsAggregate();
        if (lastNode == nullptr)
        {
            return true;
        }

        TIntermBranch *returnNode = lastNode->getSequence()->front()->getAsBranchNode();
        if (returnNode != nullptr && returnNode->getFlowOp() == EOpReturn)
        {
            return false;
        }

        return true;
    }

    bool visitAggregate(Visit, TIntermAggregate *node) override
    {
        TType returnType;
        if (IsFunctionWithoutReturnStatement(node, &returnType))
        {
            TIntermBranch *branch =
                new TIntermBranch(EOpReturn, TIntermTyped::CreateZero(returnType));

            TIntermAggregate *lastNode = node->getSequence()->back()->getAsAggregate();
            lastNode->getSequence()->push_back(branch);

            return false;
        }

        return true;
    }
};
}  // anonymous namespace

void AddDefaultReturnStatements(TIntermNode *node)
{
    AddDefaultReturnStatementsTraverser::Apply(node);
}

}  // namespace sh
