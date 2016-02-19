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
    MBasicBlock* block = func.createEntryBlock();

    // return p / 4.0
    MParameter* p = func.createParameter();
    block->add(p);
    MConstant* c = MConstant::New(func.alloc, DoubleValue(4.0));
    block->add(c);
    MDiv* div = MDiv::New(func.alloc, p, c, MIRType_Double);
    block->add(div);
    if (!div->typePolicy()->adjustInputs(func.alloc, div))
        return false;
    MDefinition* left = div->getOperand(0);
    MReturn* ret = MReturn::New(func.alloc, div);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the div got folded to p * 0.25.
    MDefinition* op = ret->getOperand(0);
    CHECK(op->isMul());
    CHECK(op->getOperand(0) == left);
    CHECK(op->getOperand(1)->isConstant());
    CHECK(op->getOperand(1)->toConstant()->numberToDouble() == 0.25);
    return true;
}
END_TEST(testJitFoldsTo_DivReciprocal)

BEGIN_TEST(testJitFoldsTo_NoDivReciprocal)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();

    // return p / 5.0
    MParameter* p = func.createParameter();
    block->add(p);
    MConstant* c = MConstant::New(func.alloc, DoubleValue(5.0));
    block->add(c);
    MDiv* div = MDiv::New(func.alloc, p, c, MIRType_Double);
    block->add(div);
    if (!div->typePolicy()->adjustInputs(func.alloc, div))
        return false;
    MDefinition* left = div->getOperand(0);
    MDefinition* right = div->getOperand(1);
    MReturn* ret = MReturn::New(func.alloc, div);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the div didn't get folded.
    MDefinition* op = ret->getOperand(0);
    CHECK(op->isDiv());
    CHECK(op->getOperand(0) == left);
    CHECK(op->getOperand(1) == right);
    return true;
}
END_TEST(testJitFoldsTo_NoDivReciprocal)

BEGIN_TEST(testJitNotNot)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();

    // return Not(Not(p))
    MParameter* p = func.createParameter();
    block->add(p);
    MNot* not0 = MNot::New(func.alloc, p);
    block->add(not0);
    MNot* not1 = MNot::New(func.alloc, not0);
    block->add(not1);
    MReturn* ret = MReturn::New(func.alloc, not1);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the nots did not get folded.
    MDefinition* op = ret->getOperand(0);
    CHECK(op->isNot());
    CHECK(op->getOperand(0)->isNot());
    CHECK(op->getOperand(0)->getOperand(0) == p);
    return true;
}
END_TEST(testJitNotNot)

BEGIN_TEST(testJitNotNotNot)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();

    // return Not(Not(Not(p)))
    MParameter* p = func.createParameter();
    block->add(p);
    MNot* not0 = MNot::New(func.alloc, p);
    block->add(not0);
    MNot* not1 = MNot::New(func.alloc, not0);
    block->add(not1);
    MNot* not2 = MNot::New(func.alloc, not1);
    block->add(not2);
    MReturn* ret = MReturn::New(func.alloc, not2);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the nots got folded.
    MDefinition* op = ret->getOperand(0);
    CHECK(op->isNot());
    CHECK(op->getOperand(0) == p);
    return true;
}
END_TEST(testJitNotNotNot)

BEGIN_TEST(testJitNotTest)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();
    MBasicBlock* then = func.createBlock(block);
    MBasicBlock* else_ = func.createBlock(block);
    MBasicBlock* exit = func.createBlock(block);

    // MTest(Not(p))
    MParameter* p = func.createParameter();
    block->add(p);
    MNot* not0 = MNot::New(func.alloc, p);
    block->add(not0);
    MTest* test = MTest::New(func.alloc, not0, then, else_);
    block->end(test);

    then->end(MGoto::New(func.alloc, exit));

    else_->end(MGoto::New(func.alloc, exit));

    MReturn* ret = MReturn::New(func.alloc, p);
    exit->end(ret);

    exit->addPredecessorWithoutPhis(then);

    if (!func.runGVN())
        return false;

    // Test that the not got folded.
    test = block->lastIns()->toTest();
    CHECK(test->getOperand(0) == p);
    CHECK(test->getSuccessor(0) == else_);
    CHECK(test->getSuccessor(1) == then);
    return true;
}
END_TEST(testJitNotTest)

BEGIN_TEST(testJitNotNotTest)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();
    MBasicBlock* then = func.createBlock(block);
    MBasicBlock* else_ = func.createBlock(block);
    MBasicBlock* exit = func.createBlock(block);

    // MTest(Not(Not(p)))
    MParameter* p = func.createParameter();
    block->add(p);
    MNot* not0 = MNot::New(func.alloc, p);
    block->add(not0);
    MNot* not1 = MNot::New(func.alloc, not0);
    block->add(not1);
    MTest* test = MTest::New(func.alloc, not1, then, else_);
    block->end(test);

    then->end(MGoto::New(func.alloc, exit));

    else_->end(MGoto::New(func.alloc, exit));

    MReturn* ret = MReturn::New(func.alloc, p);
    exit->end(ret);

    exit->addPredecessorWithoutPhis(then);

    if (!func.runGVN())
        return false;

    // Test that the nots got folded.
    test = block->lastIns()->toTest();
    CHECK(test->getOperand(0) == p);
    CHECK(test->getSuccessor(0) == then);
    CHECK(test->getSuccessor(1) == else_);
    return true;
}
END_TEST(testJitNotNotTest)

BEGIN_TEST(testJitFoldsTo_UnsignedDiv)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();

    // return 1.0 / 0xffffffff
    MConstant* c0 = MConstant::New(func.alloc, Int32Value(1));
    block->add(c0);
    MConstant* c1 = MConstant::New(func.alloc, Int32Value(0xffffffff));
    block->add(c1);
    MDiv* div = MDiv::NewAsmJS(func.alloc, c0, c1, MIRType_Int32, /*unsignd=*/true);
    block->add(div);
    MReturn* ret = MReturn::New(func.alloc, div);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the div got folded to 0.
    MConstant* op = ret->getOperand(0)->toConstant();
    CHECK(mozilla::NumbersAreIdentical(op->numberToDouble(), 0.0));
    return true;
}
END_TEST(testJitFoldsTo_UnsignedDiv)

BEGIN_TEST(testJitFoldsTo_UnsignedMod)
{
    MinimalFunc func;
    MBasicBlock* block = func.createEntryBlock();

    // return 1.0 % 0xffffffff
    MConstant* c0 = MConstant::New(func.alloc, Int32Value(1));
    block->add(c0);
    MConstant* c1 = MConstant::New(func.alloc, Int32Value(0xffffffff));
    block->add(c1);
    MMod* mod = MMod::NewAsmJS(func.alloc, c0, c1, MIRType_Int32, /*unsignd=*/true);
    block->add(mod);
    MReturn* ret = MReturn::New(func.alloc, mod);
    block->end(ret);

    if (!func.runGVN())
        return false;

    // Test that the mod got folded to 1.
    MConstant* op = ret->getOperand(0)->toConstant();
    CHECK(mozilla::NumbersAreIdentical(op->numberToDouble(), 1.0));
    return true;
}
END_TEST(testJitFoldsTo_UnsignedMod)
