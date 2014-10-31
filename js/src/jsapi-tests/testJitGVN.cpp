/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonAnalysis.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/RangeAnalysis.h"
#include "jit/ValueNumbering.h"

#include "jsapi-tests/testJitMinimalFunc.h"
#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

static MBasicBlock *
FollowTrivialGotos(MBasicBlock *block)
{
    while (block->phisEmpty() && *block->begin() == block->lastIns() && block->lastIns()->isGoto())
        block = block->lastIns()->toGoto()->getSuccessor(0);
    return block;
}

BEGIN_TEST(testJitGVN_FixupOSROnlyLoop)
{
    // This is a testcase which constructs the very rare circumstances that
    // require the FixupOSROnlyLoop logic.

    MinimalFunc func;

    MBasicBlock *entry = func.createEntryBlock();
    MBasicBlock *osrEntry = func.createOsrEntryBlock();
    MBasicBlock *outerHeader = func.createBlock(entry);
    MBasicBlock *merge = func.createBlock(outerHeader);
    MBasicBlock *innerHeader = func.createBlock(merge);
    MBasicBlock *innerBackedge = func.createBlock(innerHeader);
    MBasicBlock *outerBackedge = func.createBlock(innerHeader);
    MBasicBlock *exit = func.createBlock(outerHeader);

    MConstant *c = MConstant::New(func.alloc, BooleanValue(false));
    entry->add(c);
    entry->end(MTest::New(func.alloc, c, outerHeader, exit));
    osrEntry->end(MGoto::New(func.alloc, merge));

    merge->end(MGoto::New(func.alloc, innerHeader));

    // Use Beta nodes to hide the constants and suppress folding.
    MConstant *x = MConstant::New(func.alloc, BooleanValue(false));
    outerHeader->add(x);
    MBeta *xBeta = MBeta::New(func.alloc, x, Range::NewInt32Range(func.alloc, 0, 1));
    outerHeader->add(xBeta);
    outerHeader->end(MTest::New(func.alloc, xBeta, merge, exit));

    MConstant *y = MConstant::New(func.alloc, BooleanValue(false));
    innerHeader->add(y);
    MBeta *yBeta = MBeta::New(func.alloc, y, Range::NewInt32Range(func.alloc, 0, 1));
    innerHeader->add(yBeta);
    innerHeader->end(MTest::New(func.alloc, yBeta, innerBackedge, outerBackedge));

    innerBackedge->end(MGoto::New(func.alloc, innerHeader));
    outerBackedge->end(MGoto::New(func.alloc, outerHeader));

    MConstant *u = MConstant::New(func.alloc, UndefinedValue());
    exit->add(u);
    exit->end(MReturn::New(func.alloc, u));

    innerHeader->addPredecessorWithoutPhis(innerBackedge);
    outerHeader->addPredecessorWithoutPhis(outerBackedge);
    exit->addPredecessorWithoutPhis(entry);
    merge->addPredecessorWithoutPhis(osrEntry);

    outerHeader->setLoopHeader(outerBackedge);
    innerHeader->setLoopHeader(innerBackedge);

    if (!func.runGVN())
        return false;

    // The loops are no longer reachable from the normal entry. They are
    // doinated by the osrEntry.
    MOZ_ASSERT(func.graph.osrBlock() == osrEntry);
    MBasicBlock *newInner = FollowTrivialGotos(osrEntry->lastIns()->toGoto()->target());
    MBasicBlock *newOuter = FollowTrivialGotos(newInner->lastIns()->toTest()->ifFalse());
    MBasicBlock *newExit = FollowTrivialGotos(entry);
    MOZ_ASSERT(newInner->isLoopHeader());
    MOZ_ASSERT(newOuter->isLoopHeader());
    MOZ_ASSERT(newExit->lastIns()->isReturn());

    // One more time.
    ClearDominatorTree(func.graph);
    if (!func.runGVN())
        return false;

    // The loops are no longer reachable from the normal entry. They are
    // doinated by the osrEntry.
    MOZ_ASSERT(func.graph.osrBlock() == osrEntry);
    newInner = FollowTrivialGotos(osrEntry->lastIns()->toGoto()->target());
    newOuter = FollowTrivialGotos(newInner->lastIns()->toTest()->ifFalse());
    newExit = FollowTrivialGotos(entry);
    MOZ_ASSERT(newInner->isLoopHeader());
    MOZ_ASSERT(newOuter->isLoopHeader());
    MOZ_ASSERT(newExit->lastIns()->isReturn());

    return true;
}
END_TEST(testJitGVN_FixupOSROnlyLoop)

BEGIN_TEST(testJitGVN_FixupOSROnlyLoopNested)
{
    // Same as testJitGVN_FixupOSROnlyLoop but adds another level of loop
    // nesting for added excitement.

    MinimalFunc func;

    MBasicBlock *entry = func.createEntryBlock();
    MBasicBlock *osrEntry = func.createOsrEntryBlock();
    MBasicBlock *outerHeader = func.createBlock(entry);
    MBasicBlock *middleHeader = func.createBlock(outerHeader);
    MBasicBlock *merge = func.createBlock(middleHeader);
    MBasicBlock *innerHeader = func.createBlock(merge);
    MBasicBlock *innerBackedge = func.createBlock(innerHeader);
    MBasicBlock *middleBackedge = func.createBlock(innerHeader);
    MBasicBlock *outerBackedge = func.createBlock(middleHeader);
    MBasicBlock *exit = func.createBlock(outerHeader);

    MConstant *c = MConstant::New(func.alloc, BooleanValue(false));
    entry->add(c);
    entry->end(MTest::New(func.alloc, c, outerHeader, exit));
    osrEntry->end(MGoto::New(func.alloc, merge));

    merge->end(MGoto::New(func.alloc, innerHeader));

    // Use Beta nodes to hide the constants and suppress folding.
    MConstant *x = MConstant::New(func.alloc, BooleanValue(false));
    outerHeader->add(x);
    MBeta *xBeta = MBeta::New(func.alloc, x, Range::NewInt32Range(func.alloc, 0, 1));
    outerHeader->add(xBeta);
    outerHeader->end(MTest::New(func.alloc, xBeta, middleHeader, exit));

    MConstant *y = MConstant::New(func.alloc, BooleanValue(false));
    middleHeader->add(y);
    MBeta *yBeta = MBeta::New(func.alloc, y, Range::NewInt32Range(func.alloc, 0, 1));
    middleHeader->add(yBeta);
    middleHeader->end(MTest::New(func.alloc, yBeta, merge, outerBackedge));

    MConstant *w = MConstant::New(func.alloc, BooleanValue(false));
    innerHeader->add(w);
    MBeta *wBeta = MBeta::New(func.alloc, w, Range::NewInt32Range(func.alloc, 0, 1));
    innerHeader->add(wBeta);
    innerHeader->end(MTest::New(func.alloc, wBeta, innerBackedge, middleBackedge));

    innerBackedge->end(MGoto::New(func.alloc, innerHeader));
    middleBackedge->end(MGoto::New(func.alloc, middleHeader));
    outerBackedge->end(MGoto::New(func.alloc, outerHeader));

    MConstant *u = MConstant::New(func.alloc, UndefinedValue());
    exit->add(u);
    exit->end(MReturn::New(func.alloc, u));

    innerHeader->addPredecessorWithoutPhis(innerBackedge);
    middleHeader->addPredecessorWithoutPhis(middleBackedge);
    outerHeader->addPredecessorWithoutPhis(outerBackedge);
    exit->addPredecessorWithoutPhis(entry);
    merge->addPredecessorWithoutPhis(osrEntry);

    outerHeader->setLoopHeader(outerBackedge);
    middleHeader->setLoopHeader(middleBackedge);
    innerHeader->setLoopHeader(innerBackedge);

    if (!func.runGVN())
        return false;

    // The loops are no longer reachable from the normal entry. They are
    // doinated by the osrEntry.
    MOZ_ASSERT(func.graph.osrBlock() == osrEntry);
    MBasicBlock *newInner = FollowTrivialGotos(osrEntry->lastIns()->toGoto()->target());
    MBasicBlock *newMiddle = FollowTrivialGotos(newInner->lastIns()->toTest()->ifFalse());
    MBasicBlock *newOuter = FollowTrivialGotos(newMiddle->lastIns()->toTest()->ifFalse());
    MBasicBlock *newExit = FollowTrivialGotos(entry);
    MOZ_ASSERT(newInner->isLoopHeader());
    MOZ_ASSERT(newMiddle->isLoopHeader());
    MOZ_ASSERT(newOuter->isLoopHeader());
    MOZ_ASSERT(newExit->lastIns()->isReturn());

    // One more time.
    ClearDominatorTree(func.graph);
    if (!func.runGVN())
        return false;

    // The loops are no longer reachable from the normal entry. They are
    // doinated by the osrEntry.
    MOZ_ASSERT(func.graph.osrBlock() == osrEntry);
    newInner = FollowTrivialGotos(osrEntry->lastIns()->toGoto()->target());
    newMiddle = FollowTrivialGotos(newInner->lastIns()->toTest()->ifFalse());
    newOuter = FollowTrivialGotos(newMiddle->lastIns()->toTest()->ifFalse());
    newExit = FollowTrivialGotos(entry);
    MOZ_ASSERT(newInner->isLoopHeader());
    MOZ_ASSERT(newMiddle->isLoopHeader());
    MOZ_ASSERT(newOuter->isLoopHeader());
    MOZ_ASSERT(newExit->lastIns()->isReturn());

    return true;
}
END_TEST(testJitGVN_FixupOSROnlyLoopNested)
