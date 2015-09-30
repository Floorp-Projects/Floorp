//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/InfoSink.h"

void TIntermTraverser::pushParentBlock(TIntermAggregate *node)
{
    mParentBlockStack.push_back(ParentBlock(node, 0));
}

void TIntermTraverser::incrementParentBlockPos()
{
    ++mParentBlockStack.back().pos;
}

void TIntermTraverser::popParentBlock()
{
    ASSERT(!mParentBlockStack.empty());
    mParentBlockStack.pop_back();
}

void TIntermTraverser::insertStatementsInParentBlock(const TIntermSequence &insertions)
{
    ASSERT(!mParentBlockStack.empty());
    NodeInsertMultipleEntry insert(mParentBlockStack.back().node, mParentBlockStack.back().pos, insertions);
    mInsertions.push_back(insert);
}

TIntermSymbol *TIntermTraverser::createTempSymbol(const TType &type, TQualifier qualifier)
{
    // Each traversal uses at most one temporary variable, so the index stays the same within a single traversal.
    TInfoSinkBase symbolNameOut;
    ASSERT(mTemporaryIndex != nullptr);
    symbolNameOut << "s" << (*mTemporaryIndex);
    TString symbolName = symbolNameOut.c_str();

    TIntermSymbol *node = new TIntermSymbol(0, symbolName, type);
    node->setInternal(true);
    node->getTypePointer()->setQualifier(qualifier);
    return node;
}

TIntermSymbol *TIntermTraverser::createTempSymbol(const TType &type)
{
    return createTempSymbol(type, EvqTemporary);
}

TIntermAggregate *TIntermTraverser::createTempDeclaration(const TType &type)
{
    TIntermAggregate *tempDeclaration = new TIntermAggregate(EOpDeclaration);
    tempDeclaration->getSequence()->push_back(createTempSymbol(type));
    return tempDeclaration;
}

TIntermAggregate *TIntermTraverser::createTempInitDeclaration(TIntermTyped *initializer, TQualifier qualifier)
{
    ASSERT(initializer != nullptr);
    TIntermSymbol *tempSymbol = createTempSymbol(initializer->getType(), qualifier);
    TIntermAggregate *tempDeclaration = new TIntermAggregate(EOpDeclaration);
    TIntermBinary *tempInit = new TIntermBinary(EOpInitialize);
    tempInit->setLeft(tempSymbol);
    tempInit->setRight(initializer);
    tempInit->setType(tempSymbol->getType());
    tempDeclaration->getSequence()->push_back(tempInit);
    return tempDeclaration;
}

TIntermAggregate *TIntermTraverser::createTempInitDeclaration(TIntermTyped *initializer)
{
    return createTempInitDeclaration(initializer, EvqTemporary);
}

TIntermBinary *TIntermTraverser::createTempAssignment(TIntermTyped *rightNode)
{
    ASSERT(rightNode != nullptr);
    TIntermSymbol *tempSymbol = createTempSymbol(rightNode->getType());
    TIntermBinary *assignment = new TIntermBinary(EOpAssign);
    assignment->setLeft(tempSymbol);
    assignment->setRight(rightNode);
    assignment->setType(tempSymbol->getType());
    return assignment;
}

void TIntermTraverser::useTemporaryIndex(unsigned int *temporaryIndex)
{
    mTemporaryIndex = temporaryIndex;
}

void TIntermTraverser::nextTemporaryIndex()
{
    ASSERT(mTemporaryIndex != nullptr);
    ++(*mTemporaryIndex);
}

//
// Traverse the intermediate representation tree, and
// call a node type specific function for each node.
// Done recursively through the member function Traverse().
// Node types can be skipped if their function to call is 0,
// but their subtree will still be traversed.
// Nodes with children can have their whole subtree skipped
// if preVisit is turned on and the type specific function
// returns false.
//

//
// Traversal functions for terminals are straighforward....
//
void TIntermSymbol::traverse(TIntermTraverser *it)
{
    it->visitSymbol(this);
}

void TIntermConstantUnion::traverse(TIntermTraverser *it)
{
    it->visitConstantUnion(this);
}

//
// Traverse a binary node.
//
void TIntermBinary::traverse(TIntermTraverser *it)
{
    bool visit = true;

    //
    // visit the node before children if pre-visiting.
    //
    if (it->preVisit)
        visit = it->visitBinary(PreVisit, this);

    //
    // Visit the children, in the right order.
    //
    if (visit)
    {
        it->incrementDepth(this);

        if (mLeft)
            mLeft->traverse(it);

        if (it->inVisit)
            visit = it->visitBinary(InVisit, this);

        if (visit && mRight)
            mRight->traverse(it);

        it->decrementDepth();
    }

    //
    // Visit the node after the children, if requested and the traversal
    // hasn't been cancelled yet.
    //
    if (visit && it->postVisit)
        it->visitBinary(PostVisit, this);
}

//
// Traverse a unary node.  Same comments in binary node apply here.
//
void TIntermUnary::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitUnary(PreVisit, this);

    if (visit) {
        it->incrementDepth(this);
        mOperand->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitUnary(PostVisit, this);
}

//
// Traverse an aggregate node.  Same comments in binary node apply here.
//
void TIntermAggregate::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitAggregate(PreVisit, this);

    if (visit)
    {
        if (mOp == EOpSequence)
            it->pushParentBlock(this);

        it->incrementDepth(this);

        for (TIntermSequence::iterator sit = mSequence.begin();
                sit != mSequence.end(); sit++)
        {
            (*sit)->traverse(it);

            if (visit && it->inVisit)
            {
                if (*sit != mSequence.back())
                    visit = it->visitAggregate(InVisit, this);
            }
            if (mOp == EOpSequence)
            {
                it->incrementParentBlockPos();
            }
        }

        it->decrementDepth();

        if (mOp == EOpSequence)
            it->popParentBlock();
    }

    if (visit && it->postVisit)
        it->visitAggregate(PostVisit, this);
}

//
// Traverse a selection node.  Same comments in binary node apply here.
//
void TIntermSelection::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitSelection(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);
        mCondition->traverse(it);
        if (mTrueBlock)
            mTrueBlock->traverse(it);
        if (mFalseBlock)
            mFalseBlock->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitSelection(PostVisit, this);
}

//
// Traverse a switch node.  Same comments in binary node apply here.
//
void TIntermSwitch::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitSwitch(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);
        mInit->traverse(it);
        if (it->inVisit)
            visit = it->visitSwitch(InVisit, this);
        if (visit && mStatementList)
            mStatementList->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitSwitch(PostVisit, this);
}

//
// Traverse a switch node.  Same comments in binary node apply here.
//
void TIntermCase::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitCase(PreVisit, this);

    if (visit && mCondition)
        mCondition->traverse(it);

    if (visit && it->postVisit)
        it->visitCase(PostVisit, this);
}

//
// Traverse a loop node.  Same comments in binary node apply here.
//
void TIntermLoop::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitLoop(PreVisit, this);

    if (visit)
    {
        it->incrementDepth(this);

        if (mInit)
            mInit->traverse(it);

        if (mCond)
            mCond->traverse(it);

        if (mBody)
            mBody->traverse(it);

        if (mExpr)
            mExpr->traverse(it);

        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitLoop(PostVisit, this);
}

//
// Traverse a branch node.  Same comments in binary node apply here.
//
void TIntermBranch::traverse(TIntermTraverser *it)
{
    bool visit = true;

    if (it->preVisit)
        visit = it->visitBranch(PreVisit, this);

    if (visit && mExpression) {
        it->incrementDepth(this);
        mExpression->traverse(it);
        it->decrementDepth();
    }

    if (visit && it->postVisit)
        it->visitBranch(PostVisit, this);
}

void TIntermRaw::traverse(TIntermTraverser *it)
{
    it->visitRaw(this);
}
