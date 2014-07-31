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

BEGIN_TEST(testJitFoldsTo_DivReciprocal)
{
    MinimalFunc func;
    MBasicBlock *block = func.createEntryBlock();

    // return p / 4.0
    MParameter *p = func.createParameter();
    block->add(p);
    MConstant *c = MConstant::New(func.alloc, DoubleValue(4.0));
    block->add(c);
    MDiv *div = MDiv::New(func.alloc, p, c, MIRType_Double);
    block->add(div);
    MReturn *ret = MReturn::New(func.alloc, div);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the div got folded to p * 0.25.
    MDefinition *op = ret->getOperand(0);
    CHECK(op->isMul());
    CHECK(op->getOperand(0) == p);
    CHECK(op->getOperand(1)->isConstant());
    CHECK(op->getOperand(1)->toConstant()->value().toNumber() == 0.25);
    return true;
}
END_TEST(testJitFoldsTo_DivReciprocal)

BEGIN_TEST(testJitFoldsTo_NoDivReciprocal)
{
    MinimalFunc func;
    MBasicBlock *block = func.createEntryBlock();

    // return p / 5.0
    MParameter *p = func.createParameter();
    block->add(p);
    MConstant *c = MConstant::New(func.alloc, DoubleValue(5.0));
    block->add(c);
    MDiv *div = MDiv::New(func.alloc, p, c, MIRType_Double);
    block->add(div);
    MReturn *ret = MReturn::New(func.alloc, div);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the div didn't get folded.
    MDefinition *op = ret->getOperand(0);
    CHECK(op->isDiv());
    CHECK(op->getOperand(0) == p);
    CHECK(op->getOperand(1) == c);
    return true;
}
END_TEST(testJitFoldsTo_NoDivReciprocal)
