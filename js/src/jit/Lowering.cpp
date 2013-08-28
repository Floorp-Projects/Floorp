/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Lowering.h"

#include "mozilla/DebugOnly.h"

#include "jsanalyze.h"
#include "jsbool.h"
#include "jsnum.h"

#include "jit/IonSpewer.h"
#include "jit/LIR.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "jit/RangeAnalysis.h"

#include "jsinferinlines.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace ion;

bool
LIRGenerator::visitParameter(MParameter *param)
{
    ptrdiff_t offset;
    if (param->index() == MParameter::THIS_SLOT)
        offset = THIS_FRAME_SLOT;
    else
        offset = 1 + param->index();

    LParameter *ins = new LParameter;
    if (!defineBox(ins, param, LDefinition::PRESET))
        return false;

    offset *= sizeof(Value);
#if defined(JS_NUNBOX32)
# if defined(IS_BIG_ENDIAN)
    ins->getDef(0)->setOutput(LArgument(LAllocation::INT_ARGUMENT, offset));
    ins->getDef(1)->setOutput(LArgument(LAllocation::INT_ARGUMENT, offset + 4));
# else
    ins->getDef(0)->setOutput(LArgument(LAllocation::INT_ARGUMENT, offset + 4));
    ins->getDef(1)->setOutput(LArgument(LAllocation::INT_ARGUMENT, offset));
# endif
#elif defined(JS_PUNBOX64)
    ins->getDef(0)->setOutput(LArgument(LAllocation::INT_ARGUMENT, offset));
#endif

    return true;
}

bool
LIRGenerator::visitCallee(MCallee *ins)
{
    return define(new LCallee(), ins);
}

bool
LIRGenerator::visitForceUse(MForceUse *ins)
{
    if (ins->input()->type() == MIRType_Value) {
        LForceUseV *lir = new LForceUseV();
        if (!useBox(lir, 0, ins->input()))
            return false;
        return add(lir);
    }

    LForceUseT *lir = new LForceUseT(useAnyOrConstant(ins->input()));
    return add(lir);
}

bool
LIRGenerator::visitGoto(MGoto *ins)
{
    return add(new LGoto(ins->target()));
}

bool
LIRGenerator::visitTableSwitch(MTableSwitch *tableswitch)
{
    MDefinition *opd = tableswitch->getOperand(0);

    // There should be at least 1 successor. The default case!
    JS_ASSERT(tableswitch->numSuccessors() > 0);

    // If there are no cases, the default case is always taken.
    if (tableswitch->numSuccessors() == 1)
        return add(new LGoto(tableswitch->getDefault()));

    // If we don't know the type.
    if (opd->type() == MIRType_Value) {
        LTableSwitchV *lir = newLTableSwitchV(tableswitch);
        if (!useBox(lir, LTableSwitchV::InputValue, opd))
            return false;
        return add(lir);
    }

    // Case indices are numeric, so other types will always go to the default case.
    if (opd->type() != MIRType_Int32 && opd->type() != MIRType_Double)
        return add(new LGoto(tableswitch->getDefault()));

    // Return an LTableSwitch, capable of handling either an integer or
    // floating-point index.
    LAllocation index;
    LDefinition tempInt;
    if (opd->type() == MIRType_Int32) {
        index = useRegisterAtStart(opd);
        tempInt = tempCopy(opd, 0);
    } else {
        index = useRegister(opd);
        tempInt = temp(LDefinition::GENERAL);
    }
    return add(newLTableSwitch(index, tempInt, tableswitch));
}

bool
LIRGenerator::visitCheckOverRecursed(MCheckOverRecursed *ins)
{
    LCheckOverRecursed *lir = new LCheckOverRecursed();

    if (!add(lir))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitCheckOverRecursedPar(MCheckOverRecursedPar *ins)
{
    LCheckOverRecursedPar *lir =
        new LCheckOverRecursedPar(useRegister(ins->forkJoinSlice()), temp());
    if (!add(lir, ins))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;
    return true;
}

bool
LIRGenerator::visitDefVar(MDefVar *ins)
{
    LDefVar *lir = new LDefVar(useRegisterAtStart(ins->scopeChain()));
    if (!add(lir, ins))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitDefFun(MDefFun *ins)
{
    LDefFun *lir = new LDefFun(useRegisterAtStart(ins->scopeChain()));
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewSlots(MNewSlots *ins)
{
    // No safepoint needed, since we don't pass a cx.
    LNewSlots *lir = new LNewSlots(tempFixed(CallTempReg0), tempFixed(CallTempReg1),
                                   tempFixed(CallTempReg2));
    if (!assignSnapshot(lir))
        return false;
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitNewParallelArray(MNewParallelArray *ins)
{
    LNewParallelArray *lir = new LNewParallelArray();
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewArray(MNewArray *ins)
{
    LNewArray *lir = new LNewArray();
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewObject(MNewObject *ins)
{
    LNewObject *lir = new LNewObject();
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewDeclEnvObject(MNewDeclEnvObject *ins)
{
    LNewDeclEnvObject *lir = new LNewDeclEnvObject();
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewCallObject(MNewCallObject *ins)
{
    LAllocation slots;
    if (ins->slots()->type() == MIRType_Slots)
        slots = useRegister(ins->slots());
    else
        slots = LConstantIndex::Bogus();

    LNewCallObject *lir = new LNewCallObject(slots);
    if (!define(lir, ins))
        return false;

    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitNewCallObjectPar(MNewCallObjectPar *ins)
{
    const LAllocation &parThreadContext = useRegister(ins->forkJoinSlice());
    const LDefinition &temp1 = temp();
    const LDefinition &temp2 = temp();

    LNewCallObjectPar *lir;
    if (ins->slots()->type() == MIRType_Slots) {
        const LAllocation &slots = useRegister(ins->slots());
        lir = LNewCallObjectPar::NewWithSlots(parThreadContext, slots, temp1, temp2);
    } else {
        lir = LNewCallObjectPar::NewSansSlots(parThreadContext, temp1, temp2);
    }

    return define(lir, ins);
}

bool
LIRGenerator::visitNewStringObject(MNewStringObject *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_String);

    LNewStringObject *lir = new LNewStringObject(useRegister(ins->input()), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitAbortPar(MAbortPar *ins)
{
    LAbortPar *lir = new LAbortPar();
    return add(lir, ins);
}

bool
LIRGenerator::visitInitElem(MInitElem *ins)
{
    LInitElem *lir = new LInitElem(useRegisterAtStart(ins->getObject()));
    if (!useBoxAtStart(lir, LInitElem::IdIndex, ins->getId()))
        return false;
    if (!useBoxAtStart(lir, LInitElem::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitElemGetterSetter(MInitElemGetterSetter *ins)
{
    LInitElemGetterSetter *lir = new LInitElemGetterSetter(useRegisterAtStart(ins->object()),
                                                           useRegisterAtStart(ins->value()));
    if (!useBoxAtStart(lir, LInitElemGetterSetter::IdIndex, ins->idValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitProp(MInitProp *ins)
{
    LInitProp *lir = new LInitProp(useRegisterAtStart(ins->getObject()));
    if (!useBoxAtStart(lir, LInitProp::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitPropGetterSetter(MInitPropGetterSetter *ins)
{
    LInitPropGetterSetter *lir = new LInitPropGetterSetter(useRegisterAtStart(ins->object()),
                                                           useRegisterAtStart(ins->value()));
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitPrepareCall(MPrepareCall *ins)
{
    allocateArguments(ins->argc());

#ifdef DEBUG
    if (!prepareCallStack_.append(ins))
        return false;
#endif

    return true;
}

bool
LIRGenerator::visitPassArg(MPassArg *arg)
{
    MDefinition *opd = arg->getArgument();
    uint32_t argslot = getArgumentSlot(arg->getArgnum());

    // Pass through the virtual register of the operand.
    // This causes snapshots to correctly copy the operand on the stack.
    //
    // This keeps the backing store around longer than strictly required.
    // We could do better by informing snapshots about the argument vector.
    arg->setVirtualRegister(opd->virtualRegister());

    // Values take a slow path.
    if (opd->type() == MIRType_Value) {
        LStackArgV *stack = new LStackArgV(argslot);
        return useBox(stack, 0, opd) && add(stack);
    }

    // Known types can move constant types and/or payloads.
    LStackArgT *stack = new LStackArgT(argslot, useRegisterOrConstant(opd));
    return add(stack, arg);
}

bool
LIRGenerator::visitCreateThisWithTemplate(MCreateThisWithTemplate *ins)
{
    LCreateThisWithTemplate *lir = new LCreateThisWithTemplate();
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateThisWithProto(MCreateThisWithProto *ins)
{
    LCreateThisWithProto *lir =
        new LCreateThisWithProto(useRegisterOrConstantAtStart(ins->getCallee()),
                                 useRegisterOrConstantAtStart(ins->getPrototype()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateThis(MCreateThis *ins)
{
    LCreateThis *lir = new LCreateThis(useRegisterOrConstantAtStart(ins->getCallee()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateArgumentsObject(MCreateArgumentsObject *ins)
{
    // LAllocation callObj = useRegisterAtStart(ins->getCallObject());
    LAllocation callObj = useFixed(ins->getCallObject(), CallTempReg0);
    LCreateArgumentsObject *lir = new LCreateArgumentsObject(callObj, tempFixed(CallTempReg1));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetArgumentsObjectArg(MGetArgumentsObjectArg *ins)
{
    LAllocation argsObj = useRegister(ins->getArgsObject());
    LGetArgumentsObjectArg *lir = new LGetArgumentsObjectArg(argsObj, temp());
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitSetArgumentsObjectArg(MSetArgumentsObjectArg *ins)
{
    LAllocation argsObj = useRegister(ins->getArgsObject());
    LSetArgumentsObjectArg *lir = new LSetArgumentsObjectArg(argsObj, temp());
    if (!useBox(lir, LSetArgumentsObjectArg::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins);
}

bool
LIRGenerator::visitReturnFromCtor(MReturnFromCtor *ins)
{
    LReturnFromCtor *lir = new LReturnFromCtor(useRegister(ins->getObject()));
    if (!useBox(lir, LReturnFromCtor::ValueIndex, ins->getValue()))
        return false;

    return define(lir, ins);
}

bool
LIRGenerator::visitCall(MCall *call)
{
    JS_ASSERT(CallTempReg0 != CallTempReg1);
    JS_ASSERT(CallTempReg0 != ArgumentsRectifierReg);
    JS_ASSERT(CallTempReg1 != ArgumentsRectifierReg);
    JS_ASSERT(call->getFunction()->type() == MIRType_Object);

    // Height of the current argument vector.
    uint32_t argslot = getArgumentSlotForCall();
    freeArguments(call->numStackArgs());

    // Check MPrepareCall/MCall nesting.
    JS_ASSERT(prepareCallStack_.popCopy() == call->getPrepareCall());

    JSFunction *target = call->getSingleTarget();

    // Call DOM functions.
    if (call->isDOMFunction()) {
        JS_ASSERT(target && target->isNative());
        Register cxReg, objReg, privReg, argsReg;
        GetTempRegForIntArg(0, 0, &cxReg);
        GetTempRegForIntArg(1, 0, &objReg);
        GetTempRegForIntArg(2, 0, &privReg);
        mozilla::DebugOnly<bool> ok = GetTempRegForIntArg(3, 0, &argsReg);
        MOZ_ASSERT(ok, "How can we not have four temp registers?");
        LCallDOMNative *lir = new LCallDOMNative(argslot, tempFixed(cxReg),
                                                 tempFixed(objReg), tempFixed(privReg),
                                                 tempFixed(argsReg));
        return (defineReturn(lir, call) && assignSafepoint(lir, call));
    }

    // Call known functions.
    if (target) {
        if (target->isNative()) {
            Register cxReg, numReg, vpReg, tmpReg;
            GetTempRegForIntArg(0, 0, &cxReg);
            GetTempRegForIntArg(1, 0, &numReg);
            GetTempRegForIntArg(2, 0, &vpReg);

            // Even though this is just a temp reg, use the same API to avoid
            // register collisions.
            mozilla::DebugOnly<bool> ok = GetTempRegForIntArg(3, 0, &tmpReg);
            MOZ_ASSERT(ok, "How can we not have four temp registers?");

            LCallNative *lir = new LCallNative(argslot, tempFixed(cxReg),
                                               tempFixed(numReg),
                                               tempFixed(vpReg),
                                               tempFixed(tmpReg));
            return (defineReturn(lir, call) && assignSafepoint(lir, call));
        }

        LCallKnown *lir = new LCallKnown(useFixed(call->getFunction(), CallTempReg0),
                                         argslot, tempFixed(CallTempReg2));
        return (defineReturn(lir, call) && assignSafepoint(lir, call));
    }

    // Call anything, using the most generic code.
    LCallGeneric *lir = new LCallGeneric(useFixed(call->getFunction(), CallTempReg0),
        argslot, tempFixed(ArgumentsRectifierReg), tempFixed(CallTempReg2));
    return (assignSnapshot(lir) && defineReturn(lir, call) && assignSafepoint(lir, call));
}

bool
LIRGenerator::visitApplyArgs(MApplyArgs *apply)
{
    JS_ASSERT(apply->getFunction()->type() == MIRType_Object);

    // Assert if we cannot build a rectifier frame.
    JS_ASSERT(CallTempReg0 != ArgumentsRectifierReg);
    JS_ASSERT(CallTempReg1 != ArgumentsRectifierReg);

    // Assert if the return value is already erased.
    JS_ASSERT(CallTempReg2 != JSReturnReg_Type);
    JS_ASSERT(CallTempReg2 != JSReturnReg_Data);

    LApplyArgsGeneric *lir = new LApplyArgsGeneric(
        useFixed(apply->getFunction(), CallTempReg3),
        useFixed(apply->getArgc(), CallTempReg0),
        tempFixed(CallTempReg1),  // object register
        tempFixed(CallTempReg2)); // copy register

    MDefinition *self = apply->getThis();
    if (!useBoxFixed(lir, LApplyArgsGeneric::ThisIndex, self, CallTempReg4, CallTempReg5))
        return false;

    // Bailout is only needed in the case of possible non-JSFunction callee.
    if (!apply->getSingleTarget() && !assignSnapshot(lir))
        return false;

    if (!defineReturn(lir, apply))
        return false;
    if (!assignSafepoint(lir, apply))
        return false;
    return true;
}

bool
LIRGenerator::visitBail(MBail *bail)
{
    LBail *lir = new LBail();
    return assignSnapshot(lir) && add(lir, bail);
}

bool
LIRGenerator::visitGetDynamicName(MGetDynamicName *ins)
{
    MDefinition *scopeChain = ins->getScopeChain();
    JS_ASSERT(scopeChain->type() == MIRType_Object);

    MDefinition *name = ins->getName();
    JS_ASSERT(name->type() == MIRType_String);

    LGetDynamicName *lir = new LGetDynamicName(useFixed(scopeChain, CallTempReg0),
                                               useFixed(name, CallTempReg1),
                                               tempFixed(CallTempReg2),
                                               tempFixed(CallTempReg3),
                                               tempFixed(CallTempReg4));

    return assignSnapshot(lir) && defineReturn(lir, ins);
}

bool
LIRGenerator::visitFilterArguments(MFilterArguments *ins)
{
    MDefinition *string = ins->getString();
    JS_ASSERT(string->type() == MIRType_String);

    LFilterArguments *lir = new LFilterArguments(useFixed(string, CallTempReg0),
                                                 tempFixed(CallTempReg1),
                                                 tempFixed(CallTempReg2));

    return assignSnapshot(lir) && add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallDirectEval(MCallDirectEval *ins)
{
    MDefinition *scopeChain = ins->getScopeChain();
    JS_ASSERT(scopeChain->type() == MIRType_Object);

    MDefinition *string = ins->getString();
    JS_ASSERT(string->type() == MIRType_String);

    MDefinition *thisValue = ins->getThisValue();

    LCallDirectEval *lir = new LCallDirectEval(useRegisterAtStart(scopeChain),
                                               useRegisterAtStart(string));

    if (!useBoxAtStart(lir, LCallDirectEval::ThisValueInput, thisValue))
        return false;

    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

static JSOp
ReorderComparison(JSOp op, MDefinition **lhsp, MDefinition **rhsp)
{
    MDefinition *lhs = *lhsp;
    MDefinition *rhs = *rhsp;

    if (lhs->isConstant()) {
        *rhsp = lhs;
        *lhsp = rhs;
        return js::analyze::ReverseCompareOp(op);
    }
    return op;
}

static void
ReorderCommutative(MDefinition **lhsp, MDefinition **rhsp)
{
    MDefinition *lhs = *lhsp;
    MDefinition *rhs = *rhsp;

    // Ensure that if there is a constant, then it is in rhs.
    // In addition, since clobbering binary operations clobber the left
    // operand, prefer a non-constant lhs operand with no further uses.

    if (rhs->isConstant())
        return;

    if (lhs->isConstant() ||
        (rhs->defUseCount() == 1 && lhs->defUseCount() > 1))
    {
        *rhsp = lhs;
        *lhsp = rhs;
    }
}

bool
LIRGenerator::visitTest(MTest *test)
{
    MDefinition *opd = test->getOperand(0);
    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    // String is converted to length of string in the type analysis phase (see
    // TestPolicy).
    JS_ASSERT(opd->type() != MIRType_String);

    if (opd->type() == MIRType_Value) {
        LDefinition temp0, temp1;
        if (test->operandMightEmulateUndefined()) {
            temp0 = temp();
            temp1 = temp();
        } else {
            temp0 = LDefinition::BogusTemp();
            temp1 = LDefinition::BogusTemp();
        }
        LTestVAndBranch *lir = new LTestVAndBranch(ifTrue, ifFalse, tempFloat(), temp0, temp1);
        if (!useBox(lir, LTestVAndBranch::Input, opd))
            return false;
        return add(lir, test);
    }

    if (opd->type() == MIRType_Object) {
        // If the object might emulate undefined, we have to test for that.
        if (test->operandMightEmulateUndefined())
            return add(new LTestOAndBranch(useRegister(opd), ifTrue, ifFalse, temp()), test);

        // Otherwise we know it's truthy.
        return add(new LGoto(ifTrue));
    }

    // These must be explicitly sniffed out since they are constants and have
    // no payload.
    if (opd->type() == MIRType_Undefined || opd->type() == MIRType_Null)
        return add(new LGoto(ifFalse));

    // Constant Double operand.
    if (opd->type() == MIRType_Double && opd->isConstant()) {
        bool result = ToBoolean(opd->toConstant()->value());
        return add(new LGoto(result ? ifTrue : ifFalse));
    }

    // Constant Int32 operand.
    if (opd->type() == MIRType_Int32 && opd->isConstant()) {
        int32_t num = opd->toConstant()->value().toInt32();
        return add(new LGoto(num ? ifTrue : ifFalse));
    }

    // Constant Boolean operand.
    if (opd->type() == MIRType_Boolean && opd->isConstant()) {
        bool result = opd->toConstant()->value().toBoolean();
        return add(new LGoto(result ? ifTrue : ifFalse));
    }

    // Check if the operand for this test is a compare operation. If it is, we want
    // to emit an LCompare*AndBranch rather than an LTest*AndBranch, to fuse the
    // compare and jump instructions.
    if (opd->isCompare() && opd->isEmittedAtUses()) {
        MCompare *comp = opd->toCompare();
        MDefinition *left = comp->lhs();
        MDefinition *right = comp->rhs();

        // Try to fold the comparison so that we don't have to handle all cases.
        bool result;
        if (comp->tryFold(&result))
            return add(new LGoto(result ? ifTrue : ifFalse));

        // Emit LCompare*AndBranch.

        // Compare and branch null/undefined.
        // The second operand has known null/undefined type,
        // so just test the first operand.
        if (comp->compareType() == MCompare::Compare_Null ||
            comp->compareType() == MCompare::Compare_Undefined)
        {
            if (left->type() == MIRType_Object) {
                MOZ_ASSERT(comp->operandMightEmulateUndefined(),
                           "MCompare::tryFold should handle the never-emulates-undefined case");

                LEmulatesUndefinedAndBranch *lir =
                    new LEmulatesUndefinedAndBranch(useRegister(left), ifTrue, ifFalse, temp());
                return add(lir, comp);
            }

            LDefinition tmp, tmpToUnbox;
            if (comp->operandMightEmulateUndefined()) {
                tmp = temp();
                tmpToUnbox = tempToUnbox();
            } else {
                tmp = LDefinition::BogusTemp();
                tmpToUnbox = LDefinition::BogusTemp();
            }

            LIsNullOrLikeUndefinedAndBranch *lir =
                new LIsNullOrLikeUndefinedAndBranch(ifTrue, ifFalse, tmp, tmpToUnbox);
            if (!useBox(lir, LIsNullOrLikeUndefinedAndBranch::Value, left))
                return false;
            return add(lir, comp);
        }

        // Compare and branch booleans.
        if (comp->compareType() == MCompare::Compare_Boolean) {
            JS_ASSERT(left->type() == MIRType_Value);
            JS_ASSERT(right->type() == MIRType_Boolean);

            LAllocation rhs = useRegisterOrConstant(right);
            LCompareBAndBranch *lir = new LCompareBAndBranch(rhs, ifTrue, ifFalse);
            if (!useBox(lir, LCompareBAndBranch::Lhs, left))
                return false;
            return add(lir, comp);
        }

        // Compare and branch Int32 or Object pointers.
        if (comp->compareType() == MCompare::Compare_Int32 ||
            comp->compareType() == MCompare::Compare_UInt32 ||
            comp->compareType() == MCompare::Compare_Object)
        {
            JSOp op = ReorderComparison(comp->jsop(), &left, &right);
            LAllocation lhs = useRegister(left);
            LAllocation rhs;
            if (comp->compareType() == MCompare::Compare_Int32 ||
                comp->compareType() == MCompare::Compare_UInt32)
            {
                rhs = useAnyOrConstant(right);
            } else {
                rhs = useRegister(right);
            }
            LCompareAndBranch *lir = new LCompareAndBranch(op, lhs, rhs, ifTrue, ifFalse);
            return add(lir, comp);
        }

        // Compare and branch doubles.
        if (comp->isDoubleComparison()) {
            LAllocation lhs = useRegister(left);
            LAllocation rhs = useRegister(right);
            LCompareDAndBranch *lir = new LCompareDAndBranch(lhs, rhs, ifTrue, ifFalse);
            return add(lir, comp);
        }

        // Compare values.
        if (comp->compareType() == MCompare::Compare_Value) {
            LCompareVAndBranch *lir = new LCompareVAndBranch(ifTrue, ifFalse);
            if (!useBoxAtStart(lir, LCompareVAndBranch::LhsInput, left))
                return false;
            if (!useBoxAtStart(lir, LCompareVAndBranch::RhsInput, right))
                return false;
            return add(lir, comp);
        }
    }

    // Check if the operand for this test is a bitand operation. If it is, we want
    // to emit an LBitAndAndBranch rather than an LTest*AndBranch.
    if (opd->isBitAnd() && opd->isEmittedAtUses()) {
        MDefinition *lhs = opd->getOperand(0);
        MDefinition *rhs = opd->getOperand(1);
        if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
            ReorderCommutative(&lhs, &rhs);
            return lowerForBitAndAndBranch(new LBitAndAndBranch(ifTrue, ifFalse), test, lhs, rhs);
        }
    }

    if (opd->type() == MIRType_Double)
        return add(new LTestDAndBranch(useRegister(opd), ifTrue, ifFalse));

    JS_ASSERT(opd->type() == MIRType_Int32 || opd->type() == MIRType_Boolean);
    return add(new LTestIAndBranch(useRegister(opd), ifTrue, ifFalse));
}

bool
LIRGenerator::visitFunctionDispatch(MFunctionDispatch *ins)
{
    LFunctionDispatch *lir = new LFunctionDispatch(useRegister(ins->input()));
    return add(lir, ins);
}

bool
LIRGenerator::visitTypeObjectDispatch(MTypeObjectDispatch *ins)
{
    LTypeObjectDispatch *lir = new LTypeObjectDispatch(useRegister(ins->input()), temp());
    return add(lir, ins);
}

bool
LIRGenerator::visitPolyInlineDispatch(MPolyInlineDispatch *ins)
{
    LDefinition tempDef = LDefinition::BogusTemp();
    if (ins->propTable())
        tempDef = temp();
    LPolyInlineDispatch *lir = new LPolyInlineDispatch(useRegister(ins->input()), tempDef);
    return add(lir, ins);
}

static inline bool
CanEmitCompareAtUses(MInstruction *ins)
{
    if (!ins->canEmitAtUses())
        return false;

    bool foundTest = false;
    for (MUseIterator iter(ins->usesBegin()); iter != ins->usesEnd(); iter++) {
        MNode *node = iter->consumer();
        if (!node->isDefinition())
            return false;
        if (!node->toDefinition()->isTest())
            return false;
        if (foundTest)
            return false;
        foundTest = true;
    }
    return true;
}

bool
LIRGenerator::visitCompare(MCompare *comp)
{
    MDefinition *left = comp->lhs();
    MDefinition *right = comp->rhs();

    // Try to fold the comparison so that we don't have to handle all cases.
    bool result;
    if (comp->tryFold(&result))
        return define(new LInteger(result), comp);

    // Move below the emitAtUses call if we ever implement
    // LCompareSAndBranch. Doing this now wouldn't be wrong, but doesn't
    // make sense and avoids confusion.
    if (comp->compareType() == MCompare::Compare_String) {
        LCompareS *lir = new LCompareS(useRegister(left), useRegister(right), temp());
        if (!define(lir, comp))
            return false;
        return assignSafepoint(lir, comp);
    }

    // Strict compare between value and string
    if (comp->compareType() == MCompare::Compare_StrictString) {
        JS_ASSERT(left->type() == MIRType_Value);
        JS_ASSERT(right->type() == MIRType_String);

        LCompareStrictS *lir = new LCompareStrictS(useRegister(right), temp(), tempToUnbox());
        if (!useBox(lir, LCompareStrictS::Lhs, left))
            return false;
        if (!define(lir, comp))
            return false;
        return assignSafepoint(lir, comp);
    }

    // Unknown/unspecialized compare use a VM call.
    if (comp->compareType() == MCompare::Compare_Unknown) {
        LCompareVM *lir = new LCompareVM();
        if (!useBoxAtStart(lir, LCompareVM::LhsInput, left))
            return false;
        if (!useBoxAtStart(lir, LCompareVM::RhsInput, right))
            return false;
        return defineReturn(lir, comp) && assignSafepoint(lir, comp);
    }

    // Sniff out if the output of this compare is used only for a branching.
    // If it is, then we will emit an LCompare*AndBranch instruction in place
    // of this compare and any test that uses this compare. Thus, we can
    // ignore this Compare.
    if (CanEmitCompareAtUses(comp))
        return emitAtUses(comp);

    // Compare Null and Undefined.
    if (comp->compareType() == MCompare::Compare_Null ||
        comp->compareType() == MCompare::Compare_Undefined)
    {
        if (left->type() == MIRType_Object) {
            MOZ_ASSERT(comp->operandMightEmulateUndefined(),
                       "MCompare::tryFold should have folded this away");

            return define(new LEmulatesUndefined(useRegister(left)), comp);
        }

        LDefinition tmp, tmpToUnbox;
        if (comp->operandMightEmulateUndefined()) {
            tmp = temp();
            tmpToUnbox = tempToUnbox();
        } else {
            tmp = LDefinition::BogusTemp();
            tmpToUnbox = LDefinition::BogusTemp();
        }

        LIsNullOrLikeUndefined *lir = new LIsNullOrLikeUndefined(tmp, tmpToUnbox);
        if (!useBox(lir, LIsNullOrLikeUndefined::Value, left))
            return false;
        return define(lir, comp);
    }

    // Compare booleans.
    if (comp->compareType() == MCompare::Compare_Boolean) {
        JS_ASSERT(left->type() == MIRType_Value);
        JS_ASSERT(right->type() == MIRType_Boolean);

        LCompareB *lir = new LCompareB(useRegisterOrConstant(right));
        if (!useBox(lir, LCompareB::Lhs, left))
            return false;
        return define(lir, comp);
    }

    // Compare Int32 or Object pointers.
    if (comp->compareType() == MCompare::Compare_Int32 ||
        comp->compareType() == MCompare::Compare_UInt32 ||
        comp->compareType() == MCompare::Compare_Object)
    {
        JSOp op = ReorderComparison(comp->jsop(), &left, &right);
        LAllocation lhs = useRegister(left);
        LAllocation rhs;
        if (comp->compareType() == MCompare::Compare_Int32 ||
            comp->compareType() == MCompare::Compare_UInt32)
        {
            rhs = useAnyOrConstant(right);
        } else {
            rhs = useRegister(right);
        }
        return define(new LCompare(op, lhs, rhs), comp);
    }

    // Compare doubles.
    if (comp->isDoubleComparison())
        return define(new LCompareD(useRegister(left), useRegister(right)), comp);

    // Compare values.
    if (comp->compareType() == MCompare::Compare_Value) {
        LCompareV *lir = new LCompareV();
        if (!useBoxAtStart(lir, LCompareV::LhsInput, left))
            return false;
        if (!useBoxAtStart(lir, LCompareV::RhsInput, right))
            return false;
        return define(lir, comp);
    }

    MOZ_ASSUME_UNREACHABLE("Unrecognized compare type.");
}

bool
LIRGenerator::lowerBitOp(JSOp op, MInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        ReorderCommutative(&lhs, &rhs);
        return lowerForALU(new LBitOpI(op), ins, lhs, rhs);
    }

    LBitOpV *lir = new LBitOpV(op);
    if (!useBoxAtStart(lir, LBitOpV::LhsInput, lhs))
        return false;
    if (!useBoxAtStart(lir, LBitOpV::RhsInput, rhs))
        return false;

    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitTypeOf(MTypeOf *ins)
{
    MDefinition *opd = ins->input();
    JS_ASSERT(opd->type() == MIRType_Value);

    LTypeOfV *lir = new LTypeOfV();
    if (!useBox(lir, LTypeOfV::Input, opd))
        return false;
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitToId(MToId *ins)
{
    LToIdV *lir = new LToIdV(tempFloat());
    if (!useBox(lir, LToIdV::Object, ins->lhs()))
        return false;
    if (!useBox(lir, LToIdV::Index, ins->rhs()))
        return false;
    return defineBox(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitBitNot(MBitNot *ins)
{
    MDefinition *input = ins->getOperand(0);

    if (input->type() == MIRType_Int32)
        return lowerForALU(new LBitNotI(), ins, input);

    LBitNotV *lir = new LBitNotV;
    if (!useBoxAtStart(lir, LBitNotV::Input, input))
        return false;
    if (!defineReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

static bool
CanEmitBitAndAtUses(MInstruction *ins)
{
    if (!ins->canEmitAtUses())
        return false;

    if (ins->getOperand(0)->type() != MIRType_Int32 || ins->getOperand(1)->type() != MIRType_Int32)
        return false;

    MUseDefIterator iter(ins);
    if (!iter)
        return false;

    if (!iter.def()->isTest())
        return false;

    iter++;
    return !iter;
}

bool
LIRGenerator::visitBitAnd(MBitAnd *ins)
{
    // Sniff out if the output of this bitand is used only for a branching.
    // If it is, then we will emit an LBitAndAndBranch instruction in place
    // of this bitand and any test that uses this bitand. Thus, we can
    // ignore this BitAnd.
    if (CanEmitBitAndAtUses(ins))
        return emitAtUses(ins);

    return lowerBitOp(JSOP_BITAND, ins);
}

bool
LIRGenerator::visitBitOr(MBitOr *ins)
{
    return lowerBitOp(JSOP_BITOR, ins);
}

bool
LIRGenerator::visitBitXor(MBitXor *ins)
{
    return lowerBitOp(JSOP_BITXOR, ins);
}

bool
LIRGenerator::lowerShiftOp(JSOp op, MShiftInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        if (ins->type() == MIRType_Double) {
            JS_ASSERT(op == JSOP_URSH);
            return lowerUrshD(ins->toUrsh());
        }

        LShiftI *lir = new LShiftI(op);
        if (op == JSOP_URSH) {
            if (ins->toUrsh()->fallible() && !assignSnapshot(lir))
                return false;
        }
        return lowerForShift(lir, ins, lhs, rhs);
    }

    JS_ASSERT(ins->specialization() == MIRType_None);

    if (op == JSOP_URSH) {
        // Result is either int32 or double so we have to use BinaryV.
        return lowerBinaryV(JSOP_URSH, ins);
    }

    LBitOpV *lir = new LBitOpV(op);
    if (!useBoxAtStart(lir, LBitOpV::LhsInput, lhs))
        return false;
    if (!useBoxAtStart(lir, LBitOpV::RhsInput, rhs))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLsh(MLsh *ins)
{
    return lowerShiftOp(JSOP_LSH, ins);
}

bool
LIRGenerator::visitRsh(MRsh *ins)
{
    return lowerShiftOp(JSOP_RSH, ins);
}

bool
LIRGenerator::visitUrsh(MUrsh *ins)
{
    return lowerShiftOp(JSOP_URSH, ins);
}

bool
LIRGenerator::visitFloor(MFloor *ins)
{
    JS_ASSERT(ins->num()->type() == MIRType_Double);
    LFloor *lir = new LFloor(useRegister(ins->num()));
    if (!assignSnapshot(lir))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitRound(MRound *ins)
{
    JS_ASSERT(ins->num()->type() == MIRType_Double);
    LRound *lir = new LRound(useRegister(ins->num()), tempFloat());
    if (!assignSnapshot(lir))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitMinMax(MMinMax *ins)
{
    MDefinition *first = ins->getOperand(0);
    MDefinition *second = ins->getOperand(1);

    ReorderCommutative(&first, &second);

    if (ins->specialization() == MIRType_Int32) {
        LMinMaxI *lir = new LMinMaxI(useRegisterAtStart(first), useRegisterOrConstant(second));
        return defineReuseInput(lir, ins, 0);
    }

    LMinMaxD *lir = new LMinMaxD(useRegisterAtStart(first), useRegister(second));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGenerator::visitAbs(MAbs *ins)
{
    MDefinition *num = ins->num();

    if (num->type() == MIRType_Int32) {
        LAbsI *lir = new LAbsI(useRegisterAtStart(num));
        // needed to handle abs(INT32_MIN)
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        return defineReuseInput(lir, ins, 0);
    }

    JS_ASSERT(num->type() == MIRType_Double);
    LAbsD *lir = new LAbsD(useRegisterAtStart(num));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGenerator::visitSqrt(MSqrt *ins)
{
    MDefinition *num = ins->num();
    JS_ASSERT(num->type() == MIRType_Double);
    LSqrtD *lir = new LSqrtD(useRegisterAtStart(num));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGenerator::visitAtan2(MAtan2 *ins)
{
    MDefinition *y = ins->y();
    JS_ASSERT(y->type() == MIRType_Double);

    MDefinition *x = ins->x();
    JS_ASSERT(x->type() == MIRType_Double);

    LAtan2D *lir = new LAtan2D(useRegisterAtStart(y), useRegisterAtStart(x), tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitPow(MPow *ins)
{
    MDefinition *input = ins->input();
    JS_ASSERT(input->type() == MIRType_Double);

    MDefinition *power = ins->power();
    JS_ASSERT(power->type() == MIRType_Int32 || power->type() == MIRType_Double);

    if (power->type() == MIRType_Int32) {
        // Note: useRegisterAtStart here is safe, the temp is a GP register so
        // it will never get the same register.
        LPowI *lir = new LPowI(useRegisterAtStart(input), useFixed(power, CallTempReg1),
                               tempFixed(CallTempReg0));
        return defineReturn(lir, ins);
    }

    LPowD *lir = new LPowD(useRegisterAtStart(input), useRegisterAtStart(power),
                           tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitRandom(MRandom *ins)
{
    LRandom *lir = new LRandom(tempFixed(CallTempReg0), tempFixed(CallTempReg1));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitMathFunction(MMathFunction *ins)
{
    JS_ASSERT(ins->type() == MIRType_Double);
    JS_ASSERT(ins->input()->type() == MIRType_Double);

    // Note: useRegisterAtStart is safe here, the temp is not a FP register.
    LMathFunctionD *lir = new LMathFunctionD(useRegisterAtStart(ins->input()),
                                             tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

// Try to mark an add or sub instruction as able to recover its input when
// bailing out.
template <typename S, typename T>
static void
MaybeSetRecoversInput(S *mir, T *lir)
{
    JS_ASSERT(lir->mirRaw() == mir);
    if (!mir->fallible())
        return;

    if (lir->output()->policy() != LDefinition::MUST_REUSE_INPUT)
        return;

    // The original operands to an add or sub can't be recovered if they both
    // use the same register.
    if (lir->lhs()->isUse() && lir->rhs()->isUse() &&
        lir->lhs()->toUse()->virtualRegister() == lir->rhs()->toUse()->virtualRegister())
    {
        return;
    }

    // Add instructions that are on two different values can recover
    // the input they clobbered via MUST_REUSE_INPUT. Thus, a copy
    // of that input does not need to be kept alive in the snapshot
    // for the instruction.

    lir->setRecoversInput();

    const LUse *input = lir->getOperand(lir->output()->getReusedInput())->toUse();
    lir->snapshot()->rewriteRecoveredInput(*input);
}

bool
LIRGenerator::visitAdd(MAdd *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        ReorderCommutative(&lhs, &rhs);
        LAddI *lir = new LAddI;

        if (ins->fallible() && !assignSnapshot(lir))
            return false;

        if (!lowerForALU(lir, ins, lhs, rhs))
            return false;

        MaybeSetRecoversInput(ins, lir);
        return true;
    }

    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        ReorderCommutative(&lhs, &rhs);
        return lowerForFPU(new LMathD(JSOP_ADD), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_ADD, ins);
}

bool
LIRGenerator::visitSub(MSub *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);

        LSubI *lir = new LSubI;
        if (ins->fallible() && !assignSnapshot(lir))
            return false;

        if (!lowerForALU(lir, ins, lhs, rhs))
            return false;

        MaybeSetRecoversInput(ins, lir);
        return true;
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_SUB), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_SUB, ins);
}

bool
LIRGenerator::visitMul(MMul *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();
    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        ReorderCommutative(&lhs, &rhs);
        return lowerMulI(ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        ReorderCommutative(&lhs, &rhs);

        // If our RHS is a constant -1.0, we can optimize to an LNegD.
        if (rhs->isConstant() && rhs->toConstant()->value() == DoubleValue(-1.0))
            return defineReuseInput(new LNegD(useRegisterAtStart(lhs)), ins, 0);

        return lowerForFPU(new LMathD(JSOP_MUL), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_MUL, ins);
}

bool
LIRGenerator::visitDiv(MDiv *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();
    JS_ASSERT(lhs->type() == rhs->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(lhs->type() == MIRType_Int32);
        return lowerDivI(ins);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new LMathD(JSOP_DIV), ins, lhs, rhs);
    }

    return lowerBinaryV(JSOP_DIV, ins);
}

bool
LIRGenerator::visitMod(MMod *ins)
{
    JS_ASSERT(ins->lhs()->type() == ins->rhs()->type());

    if (ins->specialization() == MIRType_Int32) {
        JS_ASSERT(ins->type() == MIRType_Int32);
        JS_ASSERT(ins->lhs()->type() == MIRType_Int32);
        return lowerModI(ins);
    }

    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(ins->type() == MIRType_Double);
        JS_ASSERT(ins->lhs()->type() == MIRType_Double);
        JS_ASSERT(ins->rhs()->type() == MIRType_Double);

        // Note: useRegisterAtStart is safe here, the temp is not a FP register.
        LModD *lir = new LModD(useRegisterAtStart(ins->lhs()), useRegisterAtStart(ins->rhs()),
                               tempFixed(CallTempReg0));
        return defineReturn(lir, ins);
    }

    return lowerBinaryV(JSOP_MOD, ins);
}

bool
LIRGenerator::lowerBinaryV(JSOp op, MBinaryInstruction *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == MIRType_Value);
    JS_ASSERT(rhs->type() == MIRType_Value);

    LBinaryV *lir = new LBinaryV(op);
    if (!useBoxAtStart(lir, LBinaryV::LhsInput, lhs))
        return false;
    if (!useBoxAtStart(lir, LBinaryV::RhsInput, rhs))
        return false;
    if (!defineReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitConcat(MConcat *ins)
{
    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    JS_ASSERT(lhs->type() == MIRType_String);
    JS_ASSERT(rhs->type() == MIRType_String);
    JS_ASSERT(ins->type() == MIRType_String);

    LConcat *lir = new LConcat(useFixed(lhs, CallTempReg0),
                               useFixed(rhs, CallTempReg1),
                               tempFixed(CallTempReg2),
                               tempFixed(CallTempReg3),
                               tempFixed(CallTempReg4),
                               tempFixed(CallTempReg5));
    if (!defineFixed(lir, ins, LAllocation(AnyRegister(CallTempReg6))))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitConcatPar(MConcatPar *ins)
{
    MDefinition *slice = ins->forkJoinSlice();
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_String);
    JS_ASSERT(rhs->type() == MIRType_String);
    JS_ASSERT(ins->type() == MIRType_String);

    LConcatPar *lir = new LConcatPar(useFixed(slice, CallTempReg5),
                                     useFixed(lhs, CallTempReg0),
                                     useFixed(rhs, CallTempReg1),
                                     tempFixed(CallTempReg2),
                                     tempFixed(CallTempReg3),
                                     tempFixed(CallTempReg4));
    if (!defineFixed(lir, ins, LAllocation(AnyRegister(CallTempReg6))))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCharCodeAt(MCharCodeAt *ins)
{
    MDefinition *str = ins->getOperand(0);
    MDefinition *idx = ins->getOperand(1);

    JS_ASSERT(str->type() == MIRType_String);
    JS_ASSERT(idx->type() == MIRType_Int32);

    LCharCodeAt *lir = new LCharCodeAt(useRegister(str), useRegister(idx));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitFromCharCode(MFromCharCode *ins)
{
    MDefinition *code = ins->getOperand(0);

    JS_ASSERT(code->type() == MIRType_Int32);

    LFromCharCode *lir = new LFromCharCode(useRegister(code));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStart(MStart *start)
{
    // Create a snapshot that captures the initial state of the function.
    LStart *lir = new LStart;
    if (!assignSnapshot(lir))
        return false;

    if (start->startType() == MStart::StartType_Default)
        lirGraph_.setEntrySnapshot(lir->snapshot());
    return add(lir);
}

bool
LIRGenerator::visitNop(MNop *nop)
{
    return true;
}

bool
LIRGenerator::visitOsrEntry(MOsrEntry *entry)
{
    LOsrEntry *lir = new LOsrEntry;
    return defineFixed(lir, entry, LAllocation(AnyRegister(OsrFrameReg)));
}

bool
LIRGenerator::visitOsrValue(MOsrValue *value)
{
    LOsrValue *lir = new LOsrValue(useRegister(value->entry()));
    return defineBox(lir, value);
}

bool
LIRGenerator::visitOsrScopeChain(MOsrScopeChain *object)
{
    LOsrScopeChain *lir = new LOsrScopeChain(useRegister(object->entry()));
    return define(lir, object);
}

bool
LIRGenerator::visitToDouble(MToDouble *convert)
{
    MDefinition *opd = convert->input();
    mozilla::DebugOnly<MToDouble::ConversionKind> conversion = convert->conversion();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToDouble *lir = new LValueToDouble();
        if (!useBox(lir, LValueToDouble::Input, opd))
            return false;
        return assignSnapshot(lir) && define(lir, convert);
      }

      case MIRType_Null:
        JS_ASSERT(conversion != MToDouble::NumbersOnly && conversion != MToDouble::NonNullNonStringPrimitives);
        return lowerConstantDouble(0, convert);

      case MIRType_Undefined:
        JS_ASSERT(conversion != MToDouble::NumbersOnly);
        return lowerConstantDouble(js_NaN, convert);

      case MIRType_Boolean:
        JS_ASSERT(conversion != MToDouble::NumbersOnly);
        /* FALLTHROUGH */

      case MIRType_Int32:
      {
        LInt32ToDouble *lir = new LInt32ToDouble(useRegister(opd));
        return define(lir, convert);
      }

      case MIRType_Double:
        return redefine(convert, opd);

      default:
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

bool
LIRGenerator::visitToInt32(MToInt32 *convert)
{
    MDefinition *opd = convert->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToInt32 *lir = new LValueToInt32(tempFloat(), temp(), LValueToInt32::NORMAL);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return assignSnapshot(lir) && define(lir, convert) && assignSafepoint(lir, convert);
      }

      case MIRType_Null:
        return define(new LInteger(0), convert);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(convert, opd);

      case MIRType_Double:
      {
        LDoubleToInt32 *lir = new LDoubleToInt32(useRegister(opd));
        return assignSnapshot(lir) && define(lir, convert);
      }

      case MIRType_String:
        // Strings are complicated - we don't handle them yet.
        IonSpew(IonSpew_Abort, "String to Int32 not supported yet.");
        return false;

      case MIRType_Object:
        // Objects might be effectful.
        IonSpew(IonSpew_Abort, "Object to Int32 not supported yet.");
        return false;

      case MIRType_Undefined:
        IonSpew(IonSpew_Abort, "Undefined coerces to NaN, not int32_t.");
        return false;

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

bool
LIRGenerator::visitTruncateToInt32(MTruncateToInt32 *truncate)
{
    MDefinition *opd = truncate->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToInt32 *lir = new LValueToInt32(tempFloat(), temp(), LValueToInt32::TRUNCATE);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return assignSnapshot(lir) && define(lir, truncate) && assignSafepoint(lir, truncate);
      }

      case MIRType_Null:
      case MIRType_Undefined:
        return define(new LInteger(0), truncate);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(truncate, opd);

      case MIRType_Double:
        return lowerTruncateDToInt32(truncate);

      default:
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

bool
LIRGenerator::visitToString(MToString *ins)
{
    MDefinition *opd = ins->input();

    switch (opd->type()) {
      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Boolean:
        MOZ_ASSUME_UNREACHABLE("NYI: Lower MToString");

      case MIRType_Double: {
        LDoubleToString *lir = new LDoubleToString(useRegister(opd), temp());

        if (!define(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      case MIRType_Int32: {
        LIntToString *lir = new LIntToString(useRegister(opd));

        if (!define(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      default:
        // Objects might be effectful. (see ToPrimitive)
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

bool
LIRGenerator::visitRegExp(MRegExp *ins)
{
    LRegExp *lir = new LRegExp();
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRegExpTest(MRegExpTest *ins)
{
    JS_ASSERT(ins->regexp()->type() == MIRType_Object);
    JS_ASSERT(ins->string()->type() == MIRType_String);

    LRegExpTest *lir = new LRegExpTest(useRegisterAtStart(ins->regexp()),
                                       useRegisterAtStart(ins->string()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLambda(MLambda *ins)
{
    if (ins->fun()->hasSingletonType() || types::UseNewTypeForClone(ins->fun())) {
        // If the function has a singleton type, this instruction will only be
        // executed once so we don't bother inlining it.
        //
        // If UseNewTypeForClone is true, we will assign a singleton type to
        // the clone and we have to clone the script, we can't do that inline.
        LLambdaForSingleton *lir = new LLambdaForSingleton(useRegisterAtStart(ins->scopeChain()));
        return defineReturn(lir, ins) && assignSafepoint(lir, ins);
    }

    LLambda *lir = new LLambda(useRegister(ins->scopeChain()));
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLambdaPar(MLambdaPar *ins)
{
    JS_ASSERT(!ins->fun()->hasSingletonType());
    JS_ASSERT(!types::UseNewTypeForClone(ins->fun()));
    LLambdaPar *lir = new LLambdaPar(useRegister(ins->forkJoinSlice()),
                                     useRegister(ins->scopeChain()),
                                     temp(), temp());
    return define(lir, ins);
}

bool
LIRGenerator::visitImplicitThis(MImplicitThis *ins)
{
    JS_ASSERT(ins->callee()->type() == MIRType_Object);

    LImplicitThis *lir = new LImplicitThis(useRegister(ins->callee()));
    return assignSnapshot(lir) && defineBox(lir, ins);
}

bool
LIRGenerator::visitSlots(MSlots *ins)
{
    return define(new LSlots(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitElements(MElements *ins)
{
    return define(new LElements(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitConstantElements(MConstantElements *ins)
{
    return define(new LPointer(ins->value(), LPointer::NON_GC_THING), ins);
}

bool
LIRGenerator::visitConvertElementsToDoubles(MConvertElementsToDoubles *ins)
{
    LInstruction *check = new LConvertElementsToDoubles(useRegister(ins->elements()));
    return add(check, ins) && assignSafepoint(check, ins);
}

bool
LIRGenerator::visitMaybeToDoubleElement(MMaybeToDoubleElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->value()->type() == MIRType_Int32);

    LMaybeToDoubleElement *lir = new LMaybeToDoubleElement(useRegisterAtStart(ins->elements()),
                                                           useRegisterAtStart(ins->value()),
                                                           tempFloat());
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitLoadSlot(MLoadSlot *ins)
{
    switch (ins->type()) {
      case MIRType_Value:
        return defineBox(new LLoadSlotV(useRegister(ins->slots())), ins);

      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
        return define(new LLoadSlotT(useRegister(ins->slots())), ins);
    }
}

bool
LIRGenerator::visitFunctionEnvironment(MFunctionEnvironment *ins)
{
    return define(new LFunctionEnvironment(useRegisterAtStart(ins->function())), ins);
}

bool
LIRGenerator::visitForkJoinSlice(MForkJoinSlice *ins)
{
    LForkJoinSlice *lir = new LForkJoinSlice(tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitGuardThreadLocalObject(MGuardThreadLocalObject *ins)
{
    LGuardThreadLocalObject *lir =
        new LGuardThreadLocalObject(useFixed(ins->forkJoinSlice(), CallTempReg0),
                                    useFixed(ins->object(), CallTempReg1),
                                    tempFixed(CallTempReg2));
    lir->setMir(ins);
    return add(lir, ins);
}

bool
LIRGenerator::visitCheckInterruptPar(MCheckInterruptPar *ins)
{
    LCheckInterruptPar *lir =
        new LCheckInterruptPar(useRegister(ins->forkJoinSlice()), temp());
    if (!add(lir, ins))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;
    return true;
}

bool
LIRGenerator::visitNewPar(MNewPar *ins)
{
    LNewPar *lir = new LNewPar(useRegister(ins->forkJoinSlice()), temp(), temp());
    return define(lir, ins);
}

bool
LIRGenerator::visitNewDenseArrayPar(MNewDenseArrayPar *ins)
{
    LNewDenseArrayPar *lir =
        new LNewDenseArrayPar(useFixed(ins->forkJoinSlice(), CallTempReg0),
                              useFixed(ins->length(), CallTempReg1),
                              tempFixed(CallTempReg2),
                              tempFixed(CallTempReg3),
                              tempFixed(CallTempReg4));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitStoreSlot(MStoreSlot *ins)
{
    LInstruction *lir;

    switch (ins->value()->type()) {
      case MIRType_Value:
        lir = new LStoreSlotV(useRegister(ins->slots()));
        if (!useBox(lir, LStoreSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);

      case MIRType_Double:
        return add(new LStoreSlotT(useRegister(ins->slots()), useRegister(ins->value())), ins);

      default:
        return add(new LStoreSlotT(useRegister(ins->slots()), useRegisterOrConstant(ins->value())),
                   ins);
    }

    return true;
}

bool
LIRGenerator::visitTypeBarrier(MTypeBarrier *ins)
{
    // Requesting a non-GC pointer is safe here since we never re-enter C++
    // from inside a type barrier test.

    const types::StackTypeSet *types = ins->resultTypeSet();
    bool needTemp = !types->unknownObject() && types->getObjectCount() > 0;
    LDefinition tmp = needTemp ? temp() : tempToUnbox();

    LTypeBarrier *barrier = new LTypeBarrier(tmp);
    if (!useBox(barrier, LTypeBarrier::Input, ins->input()))
        return false;
    if (!assignSnapshot(barrier, ins->bailoutKind()))
        return false;
    return redefine(ins, ins->input()) && add(barrier, ins);
}

bool
LIRGenerator::visitMonitorTypes(MMonitorTypes *ins)
{
    // Requesting a non-GC pointer is safe here since we never re-enter C++
    // from inside a type check.

    const types::StackTypeSet *types = ins->typeSet();
    bool needTemp = !types->unknownObject() && types->getObjectCount() > 0;
    LDefinition tmp = needTemp ? temp() : tempToUnbox();

    LMonitorTypes *lir = new LMonitorTypes(tmp);
    if (!useBox(lir, LMonitorTypes::Input, ins->input()))
        return false;
    return assignSnapshot(lir, Bailout_Normal) && add(lir, ins);
}

bool
LIRGenerator::visitPostWriteBarrier(MPostWriteBarrier *ins)
{
#ifdef JSGC_GENERATIONAL
    switch (ins->value()->type()) {
      case MIRType_Object: {
        LPostWriteBarrierO *lir = new LPostWriteBarrierO(useRegisterOrConstant(ins->object()),
                                                         useRegister(ins->value()));
        return add(lir, ins) && assignSafepoint(lir, ins);
      }
      case MIRType_Value: {
        LPostWriteBarrierV *lir =
            new LPostWriteBarrierV(useRegisterOrConstant(ins->object()), tempToUnbox());
        if (!useBox(lir, LPostWriteBarrierV::Input, ins->value()))
            return false;
        return add(lir, ins) && assignSafepoint(lir, ins);
      }
      default:
        // Currently, only objects can be in the nursery. Other instruction
        // types cannot hold nursery pointers.
        return true;
    }
#endif // JSGC_GENERATIONAL
    return true;
}

bool
LIRGenerator::visitArrayLength(MArrayLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    return define(new LArrayLength(useRegisterAtStart(ins->elements())), ins);
}

bool
LIRGenerator::visitTypedArrayLength(MTypedArrayLength *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    return define(new LTypedArrayLength(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitTypedArrayElements(MTypedArrayElements *ins)
{
    JS_ASSERT(ins->type() == MIRType_Elements);
    return define(new LTypedArrayElements(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitInitializedLength(MInitializedLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    return define(new LInitializedLength(useRegisterAtStart(ins->elements())), ins);
}

bool
LIRGenerator::visitSetInitializedLength(MSetInitializedLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    JS_ASSERT(ins->index()->isConstant());
    return add(new LSetInitializedLength(useRegister(ins->elements()),
                                         useRegisterOrConstant(ins->index())), ins);
}

bool
LIRGenerator::visitNot(MNot *ins)
{
    MDefinition *op = ins->operand();

    // String is converted to length of string in the type analysis phase (see
    // TestPolicy).
    JS_ASSERT(op->type() != MIRType_String);

    // - boolean: x xor 1
    // - int32: LCompare(x, 0)
    // - double: LCompare(x, 0)
    // - null or undefined: true
    // - object: false if it never emulates undefined, else LNotO(x)
    switch (op->type()) {
      case MIRType_Boolean: {
        MConstant *cons = MConstant::New(Int32Value(1));
        ins->block()->insertBefore(ins, cons);
        return lowerForALU(new LBitOpI(JSOP_BITXOR), ins, op, cons);
      }
      case MIRType_Int32: {
        return define(new LNotI(useRegisterAtStart(op)), ins);
      }
      case MIRType_Double:
        return define(new LNotD(useRegister(op)), ins);
      case MIRType_Undefined:
      case MIRType_Null:
        return define(new LInteger(1), ins);
      case MIRType_Object: {
        // Objects that don't emulate undefined can be constant-folded.
        if (!ins->operandMightEmulateUndefined())
            return define(new LInteger(0), ins);
        // All others require further work.
        return define(new LNotO(useRegister(op)), ins);
      }
      case MIRType_Value: {
        LDefinition temp0, temp1;
        if (ins->operandMightEmulateUndefined()) {
            temp0 = temp();
            temp1 = temp();
        } else {
            temp0 = LDefinition::BogusTemp();
            temp1 = LDefinition::BogusTemp();
        }

        LNotV *lir = new LNotV(tempFloat(), temp0, temp1);
        if (!useBox(lir, LNotV::Input, op))
            return false;
        return define(lir, ins);
      }

      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected MIRType.");
    }
}

bool
LIRGenerator::visitBoundsCheck(MBoundsCheck *ins)
{
    LInstruction *check;
    if (ins->minimum() || ins->maximum()) {
        check = new LBoundsCheckRange(useRegisterOrConstant(ins->index()),
                                      useAny(ins->length()),
                                      temp());
    } else {
        check = new LBoundsCheck(useRegisterOrConstant(ins->index()),
                                 useAnyOrConstant(ins->length()));
    }
    return assignSnapshot(check, Bailout_BoundsCheck) && add(check, ins);
}

bool
LIRGenerator::visitBoundsCheckLower(MBoundsCheckLower *ins)
{
    if (!ins->fallible())
        return true;

    LInstruction *check = new LBoundsCheckLower(useRegister(ins->index()));
    return assignSnapshot(check, Bailout_BoundsCheck) && add(check, ins);
}

bool
LIRGenerator::visitInArray(MInArray *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    JS_ASSERT(ins->initLength()->type() == MIRType_Int32);
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Boolean);

    LAllocation object;
    if (ins->needsNegativeIntCheck())
        object = useRegister(ins->object());
    else
        object = LConstantIndex::Bogus();

    LInArray *lir = new LInArray(useRegister(ins->elements()),
                                 useRegisterOrConstant(ins->index()),
                                 useRegister(ins->initLength()),
                                 object);
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLoadElement(MLoadElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    switch (ins->type()) {
      case MIRType_Value:
      {
        LLoadElementV *lir = new LLoadElementV(useRegister(ins->elements()),
                                               useRegisterOrConstant(ins->index()));
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        return defineBox(lir, ins);
      }
      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
      {
        LLoadElementT *lir = new LLoadElementT(useRegister(ins->elements()),
                                               useRegisterOrConstant(ins->index()));
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        return define(lir, ins);
      }
    }
}

bool
LIRGenerator::visitLoadElementHole(MLoadElementHole *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    JS_ASSERT(ins->initLength()->type() == MIRType_Int32);
    JS_ASSERT(ins->type() == MIRType_Value);

    LLoadElementHole *lir = new LLoadElementHole(useRegister(ins->elements()),
                                                 useRegisterOrConstant(ins->index()),
                                                 useRegister(ins->initLength()));
    if (ins->needsNegativeIntCheck() && !assignSnapshot(lir))
        return false;
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitStoreElement(MStoreElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    const LUse elements = useRegister(ins->elements());
    const LAllocation index = useRegisterOrConstant(ins->index());

    switch (ins->value()->type()) {
      case MIRType_Value:
      {
        LInstruction *lir = new LStoreElementV(elements, index);
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        if (!useBox(lir, LStoreElementV::Value, ins->value()))
            return false;
        return add(lir, ins);
      }

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        LInstruction *lir = new LStoreElementT(elements, index, value);
        if (ins->fallible() && !assignSnapshot(lir))
            return false;
        return add(lir, ins);
      }
    }
}

bool
LIRGenerator::visitStoreElementHole(MStoreElementHole *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    const LUse object = useRegister(ins->object());
    const LUse elements = useRegister(ins->elements());
    const LAllocation index = useRegisterOrConstant(ins->index());

    LInstruction *lir;
    switch (ins->value()->type()) {
      case MIRType_Value:
        lir = new LStoreElementHoleV(object, elements, index);
        if (!useBox(lir, LStoreElementHoleV::Value, ins->value()))
            return false;
        break;

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        lir = new LStoreElementHoleT(object, elements, index, value);
        break;
      }
    }

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitEffectiveAddress(MEffectiveAddress *ins)
{
    return define(new LEffectiveAddress(useRegister(ins->base()), useRegister(ins->index())), ins);
}

bool
LIRGenerator::visitArrayPopShift(MArrayPopShift *ins)
{
    LUse object = useRegister(ins->object());

    switch (ins->type()) {
      case MIRType_Value:
      {
        LArrayPopShiftV *lir = new LArrayPopShiftV(object, temp(), temp());
        return defineBox(lir, ins) && assignSafepoint(lir, ins);
      }
      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
      {
        LArrayPopShiftT *lir = new LArrayPopShiftT(object, temp(), temp());
        return define(lir, ins) && assignSafepoint(lir, ins);
      }
    }
}

bool
LIRGenerator::visitArrayPush(MArrayPush *ins)
{
    JS_ASSERT(ins->type() == MIRType_Int32);

    LUse object = useRegister(ins->object());

    switch (ins->value()->type()) {
      case MIRType_Value:
      {
        LArrayPushV *lir = new LArrayPushV(object, temp());
        if (!useBox(lir, LArrayPushV::Value, ins->value()))
            return false;
        return define(lir, ins) && assignSafepoint(lir, ins);
      }

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        LArrayPushT *lir = new LArrayPushT(object, value, temp());
        return define(lir, ins) && assignSafepoint(lir, ins);
      }
    }
}

bool
LIRGenerator::visitArrayConcat(MArrayConcat *ins)
{
    JS_ASSERT(ins->type() == MIRType_Object);
    JS_ASSERT(ins->lhs()->type() == MIRType_Object);
    JS_ASSERT(ins->rhs()->type() == MIRType_Object);

    LArrayConcat *lir = new LArrayConcat(useFixed(ins->lhs(), CallTempReg1),
                                         useFixed(ins->rhs(), CallTempReg2),
                                         tempFixed(CallTempReg3),
                                         tempFixed(CallTempReg4));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLoadTypedArrayElement(MLoadTypedArrayElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    const LUse elements = useRegister(ins->elements());
    const LAllocation index = useRegisterOrConstant(ins->index());

    JS_ASSERT(IsNumberType(ins->type()));

    // We need a temp register for Uint32Array with known double result.
    LDefinition tempDef = LDefinition::BogusTemp();
    if (ins->arrayType() == TypedArrayObject::TYPE_UINT32 && ins->type() == MIRType_Double)
        tempDef = temp();

    LLoadTypedArrayElement *lir = new LLoadTypedArrayElement(elements, index, tempDef);
    if (ins->fallible() && !assignSnapshot(lir))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitClampToUint8(MClampToUint8 *ins)
{
    MDefinition *in = ins->input();

    switch (in->type()) {
      case MIRType_Boolean:
        return redefine(ins, in);

      case MIRType_Int32:
        return define(new LClampIToUint8(useRegisterAtStart(in)), ins);

      case MIRType_Double:
        return define(new LClampDToUint8(useRegisterAtStart(in), tempCopy(in, 0)), ins);

      case MIRType_Value:
      {
        LClampVToUint8 *lir = new LClampVToUint8(tempFloat());
        if (!useBox(lir, LClampVToUint8::Input, in))
            return false;
        return assignSnapshot(lir) && define(lir, ins);
      }

      default:
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

bool
LIRGenerator::visitLoadTypedArrayElementHole(MLoadTypedArrayElementHole *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    JS_ASSERT(ins->type() == MIRType_Value);

    const LUse object = useRegister(ins->object());
    const LAllocation index = useRegisterOrConstant(ins->index());

    LLoadTypedArrayElementHole *lir = new LLoadTypedArrayElementHole(object, index);
    if (ins->fallible() && !assignSnapshot(lir))
        return false;
    return defineBox(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLoadTypedArrayElementStatic(MLoadTypedArrayElementStatic *ins)
{
    LLoadTypedArrayElementStatic *lir =
        new LLoadTypedArrayElementStatic(useRegisterAtStart(ins->ptr()));

    if (ins->fallible() && !assignSnapshot(lir))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitLoadFixedSlot(MLoadFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LLoadFixedSlotV *lir = new LLoadFixedSlotV(useRegister(ins->object()));
        return defineBox(lir, ins);
    }

    LLoadFixedSlotT *lir = new LLoadFixedSlotT(useRegister(ins->object()));
    return define(lir, ins);
}

bool
LIRGenerator::visitStoreFixedSlot(MStoreFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->value()->type() == MIRType_Value) {
        LStoreFixedSlotV *lir = new LStoreFixedSlotV(useRegister(ins->object()));

        if (!useBox(lir, LStoreFixedSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);
    }

    LStoreFixedSlotT *lir = new LStoreFixedSlotT(useRegister(ins->object()),
                                                 useRegisterOrConstant(ins->value()));

    return add(lir, ins);
}

bool
LIRGenerator::visitGetNameCache(MGetNameCache *ins)
{
    JS_ASSERT(ins->scopeObj()->type() == MIRType_Object);

    LGetNameCache *lir = new LGetNameCache(useRegister(ins->scopeObj()));
    if (!defineBox(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetIntrinsicValue(MCallGetIntrinsicValue *ins)
{
    LCallGetIntrinsicValue *lir = new LCallGetIntrinsicValue();
    if (!defineReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallsiteCloneCache(MCallsiteCloneCache *ins)
{
    JS_ASSERT(ins->callee()->type() == MIRType_Object);

    LCallsiteCloneCache *lir = new LCallsiteCloneCache(useRegister(ins->callee()));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetPropertyCache(MGetPropertyCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    if (ins->type() == MIRType_Value) {
        LGetPropertyCacheV *lir = new LGetPropertyCacheV(useRegister(ins->object()));
        if (!defineBox(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
    }

    LGetPropertyCacheT *lir = newLGetPropertyCacheT(ins);
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetPropertyPolymorphic(MGetPropertyPolymorphic *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LGetPropertyPolymorphicV *lir = new LGetPropertyPolymorphicV(useRegister(ins->obj()));
        return assignSnapshot(lir, Bailout_CachedShapeGuard) && defineBox(lir, ins);
    }

    LDefinition maybeTemp = (ins->type() == MIRType_Double) ? temp() : LDefinition::BogusTemp();
    LGetPropertyPolymorphicT *lir = new LGetPropertyPolymorphicT(useRegister(ins->obj()), maybeTemp);
    return assignSnapshot(lir, Bailout_CachedShapeGuard) && define(lir, ins);
}

bool
LIRGenerator::visitSetPropertyPolymorphic(MSetPropertyPolymorphic *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    if (ins->value()->type() == MIRType_Value) {
        LSetPropertyPolymorphicV *lir = new LSetPropertyPolymorphicV(useRegister(ins->obj()), temp());
        if (!useBox(lir, LSetPropertyPolymorphicV::Value, ins->value()))
            return false;
        return assignSnapshot(lir, Bailout_CachedShapeGuard) && add(lir, ins);
    }

    LAllocation value = useRegisterOrConstant(ins->value());
    LSetPropertyPolymorphicT *lir =
        new LSetPropertyPolymorphicT(useRegister(ins->obj()), value, ins->value()->type(), temp());
    return assignSnapshot(lir, Bailout_CachedShapeGuard) && add(lir, ins);
}

bool
LIRGenerator::visitGetElementCache(MGetElementCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        JS_ASSERT(ins->index()->type() == MIRType_Value);
        LGetElementCacheV *lir = new LGetElementCacheV(useRegister(ins->object()));
        if (!useBox(lir, LGetElementCacheV::Index, ins->index()))
            return false;
        return defineBox(lir, ins) && assignSafepoint(lir, ins);
    }

    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    LGetElementCacheT *lir = newLGetElementCacheT(ins);
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitBindNameCache(MBindNameCache *ins)
{
    JS_ASSERT(ins->scopeChain()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Object);

    LBindNameCache *lir = new LBindNameCache(useRegister(ins->scopeChain()));
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGuardClass(MGuardClass *ins)
{
    LDefinition t = temp();
    LGuardClass *guard = new LGuardClass(useRegister(ins->obj()), t);
    return assignSnapshot(guard) && add(guard, ins);
}

bool
LIRGenerator::visitGuardObject(MGuardObject *ins)
{
    // The type policy does all the work, so at this point the input
    // is guaranteed to be an object.
    JS_ASSERT(ins->input()->type() == MIRType_Object);
    return redefine(ins, ins->input());
}

bool
LIRGenerator::visitGuardString(MGuardString *ins)
{
    // The type policy does all the work, so at this point the input
    // is guaranteed to be a string.
    JS_ASSERT(ins->input()->type() == MIRType_String);
    return redefine(ins, ins->input());
}

bool
LIRGenerator::visitCallGetProperty(MCallGetProperty *ins)
{
    LCallGetProperty *lir = new LCallGetProperty();
    if (!useBoxAtStart(lir, LCallGetProperty::Value, ins->value()))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetElement(MCallGetElement *ins)
{
    JS_ASSERT(ins->lhs()->type() == MIRType_Value);
    JS_ASSERT(ins->rhs()->type() == MIRType_Value);

    LCallGetElement *lir = new LCallGetElement();
    if (!useBoxAtStart(lir, LCallGetElement::LhsInput, ins->lhs()))
        return false;
    if (!useBoxAtStart(lir, LCallGetElement::RhsInput, ins->rhs()))
        return false;
    if (!defineReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallSetProperty(MCallSetProperty *ins)
{
    LInstruction *lir = new LCallSetProperty(useRegisterAtStart(ins->obj()));
    if (!useBoxAtStart(lir, LCallSetProperty::Value, ins->value()))
        return false;
    if (!add(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitDeleteProperty(MDeleteProperty *ins)
{
    LCallDeleteProperty *lir = new LCallDeleteProperty();
    if(!useBoxAtStart(lir, LCallDeleteProperty::Value, ins->value()))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitSetPropertyCache(MSetPropertyCache *ins)
{
    LUse obj = useRegisterAtStart(ins->obj());
    LDefinition slots = tempCopy(ins->obj(), 0);

    LInstruction *lir;
    if (ins->value()->type() == MIRType_Value) {
        lir = new LSetPropertyCacheV(obj, slots);
        if (!useBox(lir, LSetPropertyCacheV::Value, ins->value()))
            return false;
    } else {
        LAllocation value = useRegisterOrConstant(ins->value());
        lir = new LSetPropertyCacheT(obj, slots, value, ins->value()->type());
    }

    if (!add(lir, ins))
        return false;

    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitSetElementCache(MSetElementCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->index()->type() == MIRType_Value);

    LInstruction *lir;
    if (ins->value()->type() == MIRType_Value) {
        lir = new LSetElementCacheV(useRegister(ins->object()), tempToUnbox(), temp());

        if (!useBox(lir, LSetElementCacheV::Index, ins->index()))
            return false;
        if (!useBox(lir, LSetElementCacheV::Value, ins->value()))
            return false;
    } else {
        lir = new LSetElementCacheT(
            useRegister(ins->object()),
            useRegisterOrConstant(ins->value()),
            tempToUnbox(), temp());

        if (!useBox(lir, LSetElementCacheT::Index, ins->index()))
            return false;
    }

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallSetElement(MCallSetElement *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->index()->type() == MIRType_Value);
    JS_ASSERT(ins->value()->type() == MIRType_Value);

    LCallSetElement *lir = new LCallSetElement();
    lir->setOperand(0, useRegisterAtStart(ins->object()));
    if (!useBoxAtStart(lir, LCallSetElement::Index, ins->index()))
        return false;
    if (!useBoxAtStart(lir, LCallSetElement::Value, ins->value()))
        return false;
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallInitElementArray(MCallInitElementArray *ins)
{
    LCallInitElementArray *lir = new LCallInitElementArray();
    lir->setOperand(0, useRegisterAtStart(ins->object()));
    if (!useBoxAtStart(lir, LCallInitElementArray::Value, ins->value()))
        return false;
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorStart(MIteratorStart *ins)
{
    // Call a stub if this is not a simple for-in loop.
    if (ins->flags() != JSITER_ENUMERATE) {
        LCallIteratorStart *lir = new LCallIteratorStart(useRegisterAtStart(ins->object()));
        return defineReturn(lir, ins) && assignSafepoint(lir, ins);
    }

    LIteratorStart *lir = new LIteratorStart(useRegister(ins->object()), temp(), temp(), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorNext(MIteratorNext *ins)
{
    LIteratorNext *lir = new LIteratorNext(useRegister(ins->iterator()), temp());
    return defineBox(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorMore(MIteratorMore *ins)
{
    LIteratorMore *lir = new LIteratorMore(useRegister(ins->iterator()), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorEnd(MIteratorEnd *ins)
{
    LIteratorEnd *lir = new LIteratorEnd(useRegister(ins->iterator()), temp(), temp(), temp());
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStringLength(MStringLength *ins)
{
    JS_ASSERT(ins->string()->type() == MIRType_String);
    return define(new LStringLength(useRegisterAtStart(ins->string())), ins);
}

bool
LIRGenerator::visitArgumentsLength(MArgumentsLength *ins)
{
    return define(new LArgumentsLength(), ins);
}

bool
LIRGenerator::visitGetArgument(MGetArgument *ins)
{
    LGetArgument *lir = new LGetArgument(useRegisterOrConstant(ins->index()));
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitRunOncePrologue(MRunOncePrologue *ins)
{
    LRunOncePrologue *lir = new LRunOncePrologue;
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRest(MRest *ins)
{
    JS_ASSERT(ins->numActuals()->type() == MIRType_Int32);

    LRest *lir = new LRest(useFixed(ins->numActuals(), CallTempReg0),
                           tempFixed(CallTempReg1),
                           tempFixed(CallTempReg2),
                           tempFixed(CallTempReg3));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRestPar(MRestPar *ins)
{
    JS_ASSERT(ins->numActuals()->type() == MIRType_Int32);

    LRestPar *lir = new LRestPar(useFixed(ins->forkJoinSlice(), CallTempReg0),
                                 useFixed(ins->numActuals(), CallTempReg1),
                                 tempFixed(CallTempReg2),
                                 tempFixed(CallTempReg3),
                                 tempFixed(CallTempReg4));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitThrow(MThrow *ins)
{
    MDefinition *value = ins->getOperand(0);
    JS_ASSERT(value->type() == MIRType_Value);

    LThrow *lir = new LThrow;
    if (!useBoxAtStart(lir, LThrow::Value, value))
        return false;
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIn(MIn *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_Value);
    JS_ASSERT(rhs->type() == MIRType_Object);

    LIn *lir = new LIn(useRegisterAtStart(rhs));
    if (!useBoxAtStart(lir, LIn::LHS, lhs))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInstanceOf(MInstanceOf *ins)
{
    MDefinition *lhs = ins->getOperand(0);

    JS_ASSERT(lhs->type() == MIRType_Value || lhs->type() == MIRType_Object);

    if (lhs->type() == MIRType_Object) {
        LInstanceOfO *lir = new LInstanceOfO(useRegister(lhs));
        return define(lir, ins) && assignSafepoint(lir, ins);
    }

    LInstanceOfV *lir = new LInstanceOfV();
    return useBox(lir, LInstanceOfV::LHS, lhs) && define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallInstanceOf(MCallInstanceOf *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_Value);
    JS_ASSERT(rhs->type() == MIRType_Object);

    LCallInstanceOf *lir = new LCallInstanceOf(useRegisterAtStart(rhs));
    if (!useBoxAtStart(lir, LCallInstanceOf::LHS, lhs))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitFunctionBoundary(MFunctionBoundary *ins)
{
    LFunctionBoundary *lir = new LFunctionBoundary(temp());
    if (!add(lir, ins))
        return false;
    // If slow assertions are enabled, then this node will result in a callVM
    // out to a C++ function for the assertions, so we will need a safepoint.
    return !gen->compartment->rt->spsProfiler.slowAssertionsEnabled() ||
           assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIsCallable(MIsCallable *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Boolean);
    return define(new LIsCallable(useRegister(ins->object())), ins);
}

bool
LIRGenerator::visitHaveSameClass(MHaveSameClass *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_Object);
    JS_ASSERT(rhs->type() == MIRType_Object);

    return define(new LHaveSameClass(useRegister(lhs), useRegister(rhs), temp()), ins);
}

bool
LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap *ins)
{
    LAsmJSLoadHeap *lir = new LAsmJSLoadHeap(useRegisterAtStart(ins->ptr()));
    return define(lir, ins);
}

bool
LIRGenerator::visitAsmJSLoadGlobalVar(MAsmJSLoadGlobalVar *ins)
{
    return define(new LAsmJSLoadGlobalVar, ins);
}

bool
LIRGenerator::visitAsmJSStoreGlobalVar(MAsmJSStoreGlobalVar *ins)
{
    return add(new LAsmJSStoreGlobalVar(useRegisterAtStart(ins->value())), ins);
}

bool
LIRGenerator::visitAsmJSLoadFFIFunc(MAsmJSLoadFFIFunc *ins)
{
    return define(new LAsmJSLoadFFIFunc, ins);
}

bool
LIRGenerator::visitAsmJSParameter(MAsmJSParameter *ins)
{
    ABIArg abi = ins->abi();
    if (abi.argInRegister())
        return defineFixed(new LAsmJSParameter, ins, LAllocation(abi.reg()));

    JS_ASSERT(ins->type() == MIRType_Int32 || ins->type() == MIRType_Double);
    LAllocation::Kind argKind = ins->type() == MIRType_Int32
                                ? LAllocation::INT_ARGUMENT
                                : LAllocation::DOUBLE_ARGUMENT;
    return defineFixed(new LAsmJSParameter, ins, LArgument(argKind, abi.offsetFromArgBase()));
}

bool
LIRGenerator::visitAsmJSReturn(MAsmJSReturn *ins)
{
    MDefinition *rval = ins->getOperand(0);
    LAsmJSReturn *lir = new LAsmJSReturn;
    if (rval->type() == MIRType_Double)
        lir->setOperand(0, useFixed(rval, ReturnFloatReg));
    else if (rval->type() == MIRType_Int32)
        lir->setOperand(0, useFixed(rval, ReturnReg));
    else
        MOZ_ASSUME_UNREACHABLE("Unexpected asm.js return type");
    return add(lir);
}

bool
LIRGenerator::visitAsmJSVoidReturn(MAsmJSVoidReturn *ins)
{
    return add(new LAsmJSVoidReturn);
}

bool
LIRGenerator::visitAsmJSPassStackArg(MAsmJSPassStackArg *ins)
{
    if (ins->arg()->type() == MIRType_Double) {
        JS_ASSERT(!ins->arg()->isEmittedAtUses());
        return add(new LAsmJSPassStackArg(useRegisterAtStart(ins->arg())), ins);
    }

    return add(new LAsmJSPassStackArg(useRegisterOrConstantAtStart(ins->arg())), ins);
}

bool
LIRGenerator::visitAsmJSCall(MAsmJSCall *ins)
{
    gen->setPerformsAsmJSCall();

    LAllocation *args = gen->allocate<LAllocation>(ins->numOperands());
    if (!args)
        return false;

    for (unsigned i = 0; i < ins->numArgs(); i++)
        args[i] = useFixed(ins->getOperand(i), ins->registerForArg(i));

    if (ins->callee().which() == MAsmJSCall::Callee::Dynamic)
        args[ins->dynamicCalleeOperandIndex()] = useFixed(ins->callee().dynamic(), CallTempReg0);

    LInstruction *lir = new LAsmJSCall(args, ins->numOperands());
    if (ins->type() == MIRType_None) {
        return add(lir, ins);
    }
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitAsmJSCheckOverRecursed(MAsmJSCheckOverRecursed *ins)
{
    return add(new LAsmJSCheckOverRecursed(), ins);
}

bool
LIRGenerator::visitSetDOMProperty(MSetDOMProperty *ins)
{
    MDefinition *val = ins->value();

    Register cxReg, objReg, privReg, valueReg;
    GetTempRegForIntArg(0, 0, &cxReg);
    GetTempRegForIntArg(1, 0, &objReg);
    GetTempRegForIntArg(2, 0, &privReg);
    GetTempRegForIntArg(3, 0, &valueReg);
    LSetDOMProperty *lir = new LSetDOMProperty(tempFixed(cxReg),
                                               useFixed(ins->object(), objReg),
                                               tempFixed(privReg),
                                               tempFixed(valueReg));

    // Keep using GetTempRegForIntArg, since we want to make sure we
    // don't clobber registers we're already using.
    Register tempReg1, tempReg2;
    GetTempRegForIntArg(4, 0, &tempReg1);
    mozilla::DebugOnly<bool> ok = GetTempRegForIntArg(5, 0, &tempReg2);
    MOZ_ASSERT(ok, "How can we not have six temp registers?");
    if (!useBoxFixed(lir, LSetDOMProperty::Value, val, tempReg1, tempReg2))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetDOMProperty(MGetDOMProperty *ins)
{
    Register cxReg, objReg, privReg, valueReg;
    GetTempRegForIntArg(0, 0, &cxReg);
    GetTempRegForIntArg(1, 0, &objReg);
    GetTempRegForIntArg(2, 0, &privReg);
    mozilla::DebugOnly<bool> ok = GetTempRegForIntArg(3, 0, &valueReg);
    MOZ_ASSERT(ok, "How can we not have four temp registers?");
    LGetDOMProperty *lir = new LGetDOMProperty(tempFixed(cxReg),
                                               useFixed(ins->object(), objReg),
                                               tempFixed(privReg),
                                               tempFixed(valueReg));

    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

static void
SpewResumePoint(MBasicBlock *block, MInstruction *ins, MResumePoint *resumePoint)
{
    fprintf(IonSpewFile, "Current resume point %p details:\n", (void *)resumePoint);
    fprintf(IonSpewFile, "    frame count: %u\n", resumePoint->frameCount());

    if (ins) {
        fprintf(IonSpewFile, "    taken after: ");
        ins->printName(IonSpewFile);
    } else {
        fprintf(IonSpewFile, "    taken at block %d entry", block->id());
    }
    fprintf(IonSpewFile, "\n");

    fprintf(IonSpewFile, "    pc: %p (script: %p, offset: %d)\n",
            (void *)resumePoint->pc(),
            (void *)resumePoint->block()->info().script(),
            int(resumePoint->pc() - resumePoint->block()->info().script()->code));

    for (size_t i = 0, e = resumePoint->numOperands(); i < e; i++) {
        MDefinition *in = resumePoint->getOperand(i);
        fprintf(IonSpewFile, "    slot%u: ", (unsigned)i);
        in->printName(IonSpewFile);
        fprintf(IonSpewFile, "\n");
    }
}

bool
LIRGenerator::visitInstruction(MInstruction *ins)
{
    if (!gen->ensureBallast())
        return false;
    if (!ins->accept(this))
        return false;

    if (ins->resumePoint())
        updateResumeState(ins);

    if (gen->errored())
        return false;
#ifdef DEBUG
    ins->setInWorklistUnchecked();
#endif

    // If no safepoint was created, there's no need for an OSI point.
    if (LOsiPoint *osiPoint = popOsiPoint()) {
        if (!add(osiPoint))
            return false;
    }

    return true;
}

bool
LIRGenerator::definePhis()
{
    size_t lirIndex = 0;
    MBasicBlock *block = current->mir();
    for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
        if (phi->type() == MIRType_Value) {
            if (!defineUntypedPhi(*phi, lirIndex))
                return false;
            lirIndex += BOX_PIECES;
        } else {
            if (!defineTypedPhi(*phi, lirIndex))
                return false;
            lirIndex += 1;
        }
    }
    return true;
}

void
LIRGenerator::updateResumeState(MInstruction *ins)
{
    lastResumePoint_ = ins->resumePoint();
    if (IonSpewEnabled(IonSpew_Snapshots) && lastResumePoint_)
        SpewResumePoint(NULL, ins, lastResumePoint_);
}

void
LIRGenerator::updateResumeState(MBasicBlock *block)
{
    lastResumePoint_ = block->entryResumePoint();
    if (IonSpewEnabled(IonSpew_Snapshots) && lastResumePoint_)
        SpewResumePoint(block, NULL, lastResumePoint_);
}

void
LIRGenerator::allocateArguments(uint32_t argc)
{
    argslots_ += argc;
    if (argslots_ > maxargslots_)
        maxargslots_ = argslots_;
}

uint32_t
LIRGenerator::getArgumentSlot(uint32_t argnum)
{
    // First slot has index 1.
    JS_ASSERT(argnum < argslots_);
    return argslots_ - argnum ;
}

void
LIRGenerator::freeArguments(uint32_t argc)
{
    JS_ASSERT(argc <= argslots_);
    argslots_ -= argc;
}

bool
LIRGenerator::visitBlock(MBasicBlock *block)
{
    current = block->lir();
    updateResumeState(block);

    if (!definePhis())
        return false;

    if (!add(new LLabel()))
        return false;

    for (MInstructionIterator iter = block->begin(); *iter != block->lastIns(); iter++) {
        if (!visitInstruction(*iter))
            return false;
    }

    if (block->successorWithPhis()) {
        // If we have a successor with phis, lower the phi input now that we
        // are approaching the join point.
        MBasicBlock *successor = block->successorWithPhis();
        uint32_t position = block->positionInPhiSuccessor();
        size_t lirIndex = 0;
        for (MPhiIterator phi(successor->phisBegin()); phi != successor->phisEnd(); phi++) {
            MDefinition *opd = phi->getOperand(position);
            if (!ensureDefined(opd))
                return false;

            JS_ASSERT(opd->type() == phi->type());

            if (phi->type() == MIRType_Value) {
                lowerUntypedPhiInput(*phi, position, successor->lir(), lirIndex);
                lirIndex += BOX_PIECES;
            } else {
                lowerTypedPhiInput(*phi, position, successor->lir(), lirIndex);
                lirIndex += 1;
            }
        }
    }

    // Now emit the last instruction, which is some form of branch.
    if (!visitInstruction(block->lastIns()))
        return false;

    return true;
}

bool
LIRGenerator::precreatePhi(LBlock *block, MPhi *phi)
{
    LPhi *lir = LPhi::New(gen, phi);
    if (!lir)
        return false;
    if (!block->addPhi(lir))
        return false;
    return true;
}

bool
LIRGenerator::generate()
{
    // Create all blocks and prep all phis beforehand.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (gen->shouldCancel("Lowering (preparation loop)"))
            return false;

        current = LBlock::New(*block);
        if (!current)
            return false;
        if (!lirGraph_.addBlock(current))
            return false;
        block->assignLir(current);

        // For each MIR phi, add LIR phis as appropriate. We'll fill in their
        // operands on each incoming edge, and set their definitions at the
        // start of their defining block.
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            int numPhis = (phi->type() == MIRType_Value) ? BOX_PIECES : 1;
            for (int i = 0; i < numPhis; i++) {
                if (!precreatePhi(block->lir(), *phi))
                    return false;
            }
        }
    }

    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (gen->shouldCancel("Lowering (main loop)"))
            return false;

        if (!visitBlock(*block))
            return false;
    }

    if (graph.osrBlock())
        lirGraph_.setOsrBlock(graph.osrBlock()->lir());

    lirGraph_.setArgumentSlotCount(maxargslots_);

    JS_ASSERT(argslots_ == 0);
    JS_ASSERT(prepareCallStack_.empty());
    return true;
}
