/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonAnalysis.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/ValueNumbering.h"

#include "jsapi-tests/testJitMinimalFunc.h"
#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::jit;

BEGIN_TEST(testJitDCEinGVN_ins)
{
    MinimalFunc func;
    MBasicBlock *block = func.createEntryBlock();

    // mul0 = p * p
    // mul1 = mul0 * mul0
    // return p
    MParameter *p = func.createParameter();
    block->add(p);
    MMul *mul0 = MMul::New(func.alloc, p, p, MIRType_Double);
    block->add(mul0);
    MMul *mul1 = MMul::New(func.alloc, mul0, mul0, MIRType_Double);
    block->add(mul1);
    MReturn *ret = MReturn::New(func.alloc, p);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // mul0 and mul1 should be deleted.
    for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
        CHECK(!ins->isMul() || (ins->getOperand(0) != p && ins->getOperand(1) != p));
        CHECK(!ins->isMul() || (ins->getOperand(0) != mul0 && ins->getOperand(1) != mul0));
    }
    return true;
}
END_TEST(testJitDCEinGVN_ins)

BEGIN_TEST(testJitDCEinGVN_phi)
{
    MinimalFunc func;
    MBasicBlock *block = func.createEntryBlock();
    MBasicBlock *thenBlock1 = func.createBlock(block);
    MBasicBlock *thenBlock2 = func.createBlock(block);
    MBasicBlock *elifBlock = func.createBlock(block);
    MBasicBlock *elseBlock = func.createBlock(block);
    MBasicBlock *joinBlock = func.createBlock(block);

    // if (p) {
    //   x = 1.0;
    //   y = 3.0;
    // } else if (q) {
    //   x = 2.0;
    //   y = 4.0;
    // } else {
    //   x = 1.0;
    //   y = 5.0;
    // }
    // x = phi(1.0, 2.0, 1.0);
    // y = phi(3.0, 4.0, 5.0);
    // z = x * y;
    // return y;

    MConstant *c1 = MConstant::New(func.alloc, DoubleValue(1.0));
    block->add(c1);
    MPhi *x = MPhi::New(func.alloc);
    MPhi *y = MPhi::New(func.alloc);

    // if (p) {
    MParameter *p = func.createParameter();
    block->add(p);
    block->end(MTest::New(func.alloc, p, thenBlock1, elifBlock));

    //   x = 1.0
    //   y = 3.0;
    x->addInputSlow(c1);
    MConstant *c3 = MConstant::New(func.alloc, DoubleValue(3.0));
    thenBlock1->add(c3);
    y->addInputSlow(c3);
    thenBlock1->end(MGoto::New(func.alloc, joinBlock));
    joinBlock->addPredecessor(func.alloc, thenBlock1);

    // } else if (q) {
    MParameter *q = func.createParameter();
    elifBlock->add(q);
    elifBlock->end(MTest::New(func.alloc, q, thenBlock2, elseBlock));

    //   x = 2.0
    //   y = 4.0;
    MConstant *c2 = MConstant::New(func.alloc, DoubleValue(2.0));
    thenBlock2->add(c2);
    x->addInputSlow(c2);
    MConstant *c4 = MConstant::New(func.alloc, DoubleValue(4.0));
    thenBlock2->add(c4);
    y->addInputSlow(c4);
    thenBlock2->end(MGoto::New(func.alloc, joinBlock));
    joinBlock->addPredecessor(func.alloc, thenBlock2);

    // } else {
    //   x = 1.0
    //   y = 5.0;
    // }
    x->addInputSlow(c1);
    MConstant *c5 = MConstant::New(func.alloc, DoubleValue(5.0));
    elseBlock->add(c5);
    y->addInputSlow(c5);
    elseBlock->end(MGoto::New(func.alloc, joinBlock));
    joinBlock->addPredecessor(func.alloc, elseBlock);

    // x = phi(1.0, 2.0, 1.0)
    // y = phi(3.0, 4.0, 5.0)
    // z = x * y
    // return y
    joinBlock->addPhi(x);
    joinBlock->addPhi(y);
    MMul *z = MMul::New(func.alloc, x, y, MIRType_Double);
    joinBlock->add(z);
    MReturn *ret = MReturn::New(func.alloc, y);
    joinBlock->end(ret);

    if (!func.runGVN())
        return false;

    // c1 should be deleted.
    for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
        CHECK(!ins->isConstant() || (ins->toConstant()->value().toNumber() != 1.0));
    }
    return true;
}
END_TEST(testJitDCEinGVN_phi)
