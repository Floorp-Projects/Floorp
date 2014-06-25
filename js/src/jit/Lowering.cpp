/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Lowering.h"

#include "mozilla/DebugOnly.h"

#include "jit/IonSpewer.h"
#include "jit/LIR.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace jit;

using mozilla::DebugOnly;
using JS::GenericNaN;

bool
LIRGenerator::visitCloneLiteral(MCloneLiteral *ins)
{
    JS_ASSERT(ins->type() == MIRType_Object);
    JS_ASSERT(ins->input()->type() == MIRType_Object);

    LCloneLiteral *lir = new(alloc()) LCloneLiteral(useRegisterAtStart(ins->input()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitParameter(MParameter *param)
{
    ptrdiff_t offset;
    if (param->index() == MParameter::THIS_SLOT)
        offset = THIS_FRAME_ARGSLOT;
    else
        offset = 1 + param->index();

    LParameter *ins = new(alloc()) LParameter;
    if (!defineBox(ins, param, LDefinition::FIXED))
        return false;

    offset *= sizeof(Value);
#if defined(JS_NUNBOX32)
# if defined(IS_BIG_ENDIAN)
    ins->getDef(0)->setOutput(LArgument(offset));
    ins->getDef(1)->setOutput(LArgument(offset + 4));
# else
    ins->getDef(0)->setOutput(LArgument(offset + 4));
    ins->getDef(1)->setOutput(LArgument(offset));
# endif
#elif defined(JS_PUNBOX64)
    ins->getDef(0)->setOutput(LArgument(offset));
#endif

    return true;
}

bool
LIRGenerator::visitCallee(MCallee *ins)
{
    return define(new(alloc()) LCallee(), ins);
}

bool
LIRGenerator::visitGoto(MGoto *ins)
{
    return add(new(alloc()) LGoto(ins->target()));
}

bool
LIRGenerator::visitTableSwitch(MTableSwitch *tableswitch)
{
    MDefinition *opd = tableswitch->getOperand(0);

    // There should be at least 1 successor. The default case!
    JS_ASSERT(tableswitch->numSuccessors() > 0);

    // If there are no cases, the default case is always taken.
    if (tableswitch->numSuccessors() == 1)
        return add(new(alloc()) LGoto(tableswitch->getDefault()));

    // If we don't know the type.
    if (opd->type() == MIRType_Value) {
        LTableSwitchV *lir = newLTableSwitchV(tableswitch);
        if (!useBox(lir, LTableSwitchV::InputValue, opd))
            return false;
        return add(lir);
    }

    // Case indices are numeric, so other types will always go to the default case.
    if (opd->type() != MIRType_Int32 && opd->type() != MIRType_Double)
        return add(new(alloc()) LGoto(tableswitch->getDefault()));

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
    LCheckOverRecursed *lir = new(alloc()) LCheckOverRecursed();
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCheckOverRecursedPar(MCheckOverRecursedPar *ins)
{
    LCheckOverRecursedPar *lir =
        new(alloc()) LCheckOverRecursedPar(useRegister(ins->forkJoinContext()), temp());
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitDefVar(MDefVar *ins)
{
    LDefVar *lir = new(alloc()) LDefVar(useRegisterAtStart(ins->scopeChain()));
    if (!add(lir, ins))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitDefFun(MDefFun *ins)
{
    LDefFun *lir = new(alloc()) LDefFun(useRegisterAtStart(ins->scopeChain()));
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewArray(MNewArray *ins)
{
    LNewArray *lir = new(alloc()) LNewArray(temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewObject(MNewObject *ins)
{
    LNewObject *lir = new(alloc()) LNewObject(temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewDeclEnvObject(MNewDeclEnvObject *ins)
{
    LNewDeclEnvObject *lir = new(alloc()) LNewDeclEnvObject(temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewCallObject(MNewCallObject *ins)
{
    LInstruction *lir;
    if (ins->templateObject()->hasSingletonType()) {
        LNewSingletonCallObject *singletonLir = new(alloc()) LNewSingletonCallObject(temp());
        if (!define(singletonLir, ins))
            return false;
        lir = singletonLir;
    } else {
        LNewCallObject *normalLir = new(alloc()) LNewCallObject(temp());
        if (!define(normalLir, ins))
            return false;
        lir = normalLir;
    }

    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitNewRunOnceCallObject(MNewRunOnceCallObject *ins)
{
    LNewSingletonCallObject *lir = new(alloc()) LNewSingletonCallObject(temp());
    if (!define(lir, ins))
        return false;

    if (!assignSafepoint(lir, ins))
        return false;

    return true;
}

bool
LIRGenerator::visitNewDerivedTypedObject(MNewDerivedTypedObject *ins)
{
    LNewDerivedTypedObject *lir =
        new(alloc()) LNewDerivedTypedObject(useRegisterAtStart(ins->type()),
                                            useRegisterAtStart(ins->owner()),
                                            useRegisterAtStart(ins->offset()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewCallObjectPar(MNewCallObjectPar *ins)
{
    const LAllocation &parThreadContext = useRegister(ins->forkJoinContext());
    LNewCallObjectPar *lir = LNewCallObjectPar::New(alloc(), parThreadContext, temp(), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewStringObject(MNewStringObject *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_String);

    LNewStringObject *lir = new(alloc()) LNewStringObject(useRegister(ins->input()), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitElem(MInitElem *ins)
{
    LInitElem *lir = new(alloc()) LInitElem(useRegisterAtStart(ins->getObject()));
    if (!useBoxAtStart(lir, LInitElem::IdIndex, ins->getId()))
        return false;
    if (!useBoxAtStart(lir, LInitElem::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitElemGetterSetter(MInitElemGetterSetter *ins)
{
    LInitElemGetterSetter *lir =
        new(alloc()) LInitElemGetterSetter(useRegisterAtStart(ins->object()),
                                           useRegisterAtStart(ins->value()));
    if (!useBoxAtStart(lir, LInitElemGetterSetter::IdIndex, ins->idValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitMutateProto(MMutateProto *ins)
{
    LMutateProto *lir = new(alloc()) LMutateProto(useRegisterAtStart(ins->getObject()));
    if (!useBoxAtStart(lir, LMutateProto::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitProp(MInitProp *ins)
{
    LInitProp *lir = new(alloc()) LInitProp(useRegisterAtStart(ins->getObject()));
    if (!useBoxAtStart(lir, LInitProp::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInitPropGetterSetter(MInitPropGetterSetter *ins)
{
    LInitPropGetterSetter *lir =
        new(alloc()) LInitPropGetterSetter(useRegisterAtStart(ins->object()),
                                           useRegisterAtStart(ins->value()));
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateThisWithTemplate(MCreateThisWithTemplate *ins)
{
    LCreateThisWithTemplate *lir = new(alloc()) LCreateThisWithTemplate(temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateThisWithProto(MCreateThisWithProto *ins)
{
    LCreateThisWithProto *lir =
        new(alloc()) LCreateThisWithProto(useRegisterOrConstantAtStart(ins->getCallee()),
                                          useRegisterOrConstantAtStart(ins->getPrototype()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateThis(MCreateThis *ins)
{
    LCreateThis *lir = new(alloc()) LCreateThis(useRegisterOrConstantAtStart(ins->getCallee()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCreateArgumentsObject(MCreateArgumentsObject *ins)
{
    // LAllocation callObj = useRegisterAtStart(ins->getCallObject());
    LAllocation callObj = useFixed(ins->getCallObject(), CallTempReg0);
    LCreateArgumentsObject *lir = new(alloc()) LCreateArgumentsObject(callObj, tempFixed(CallTempReg1));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetArgumentsObjectArg(MGetArgumentsObjectArg *ins)
{
    LAllocation argsObj = useRegister(ins->getArgsObject());
    LGetArgumentsObjectArg *lir = new(alloc()) LGetArgumentsObjectArg(argsObj, temp());
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitSetArgumentsObjectArg(MSetArgumentsObjectArg *ins)
{
    LAllocation argsObj = useRegister(ins->getArgsObject());
    LSetArgumentsObjectArg *lir = new(alloc()) LSetArgumentsObjectArg(argsObj, temp());
    if (!useBox(lir, LSetArgumentsObjectArg::ValueIndex, ins->getValue()))
        return false;

    return add(lir, ins);
}

bool
LIRGenerator::visitReturnFromCtor(MReturnFromCtor *ins)
{
    LReturnFromCtor *lir = new(alloc()) LReturnFromCtor(useRegister(ins->getObject()));
    if (!useBox(lir, LReturnFromCtor::ValueIndex, ins->getValue()))
        return false;

    return define(lir, ins);
}

bool
LIRGenerator::visitComputeThis(MComputeThis *ins)
{
    JS_ASSERT(ins->type() == MIRType_Object);
    JS_ASSERT(ins->input()->type() == MIRType_Value);

    LComputeThis *lir = new(alloc()) LComputeThis();

    // Don't use useBoxAtStart because ComputeThis has a safepoint and needs to
    // have its inputs in different registers than its return value so that
    // they aren't clobbered.
    if (!useBox(lir, LComputeThis::ValueIndex, ins->input()))
        return false;

    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLoadArrowThis(MLoadArrowThis *ins)
{
    JS_ASSERT(ins->type() == MIRType_Value);
    JS_ASSERT(ins->callee()->type() == MIRType_Object);

    LLoadArrowThis *lir = new(alloc()) LLoadArrowThis(useRegister(ins->callee()));
    return defineBox(lir, ins);
}

bool
LIRGenerator::lowerCallArguments(MCall *call)
{
    uint32_t argc = call->numStackArgs();
    if (argc > maxargslots_)
        maxargslots_ = argc;

    for (size_t i = 0; i < argc; i++) {
        MDefinition *arg = call->getArg(i);
        uint32_t argslot = argc - i;

        // Values take a slow path.
        if (arg->type() == MIRType_Value) {
            LStackArgV *stack = new(alloc()) LStackArgV(argslot);
            if (!useBox(stack, 0, arg) || !add(stack))
                return false;
        } else {
            // Known types can move constant types and/or payloads.
            LStackArgT *stack = new(alloc()) LStackArgT(argslot, arg->type(), useRegisterOrConstant(arg));
            if (!add(stack))
                return false;
        }
    }

    return true;
}

bool
LIRGenerator::visitCall(MCall *call)
{
    JS_ASSERT(CallTempReg0 != CallTempReg1);
    JS_ASSERT(CallTempReg0 != ArgumentsRectifierReg);
    JS_ASSERT(CallTempReg1 != ArgumentsRectifierReg);
    JS_ASSERT(call->getFunction()->type() == MIRType_Object);

    if (!lowerCallArguments(call))
        return false;

    // Height of the current argument vector.
    JSFunction *target = call->getSingleTarget();

    // Call DOM functions.
    if (call->isCallDOMNative()) {
        JS_ASSERT(target && target->isNative());
        Register cxReg, objReg, privReg, argsReg;
        GetTempRegForIntArg(0, 0, &cxReg);
        GetTempRegForIntArg(1, 0, &objReg);
        GetTempRegForIntArg(2, 0, &privReg);
        mozilla::DebugOnly<bool> ok = GetTempRegForIntArg(3, 0, &argsReg);
        MOZ_ASSERT(ok, "How can we not have four temp registers?");
        LCallDOMNative *lir = new(alloc()) LCallDOMNative(tempFixed(cxReg), tempFixed(objReg),
                                                          tempFixed(privReg), tempFixed(argsReg));
        return defineReturn(lir, call) && assignSafepoint(lir, call);
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

            LCallNative *lir = new(alloc()) LCallNative(tempFixed(cxReg), tempFixed(numReg),
                                                        tempFixed(vpReg), tempFixed(tmpReg));
            return defineReturn(lir, call) && assignSafepoint(lir, call);
        }

        LCallKnown *lir = new(alloc()) LCallKnown(useFixed(call->getFunction(), CallTempReg0),
                                                  tempFixed(CallTempReg2));
        return defineReturn(lir, call) && assignSafepoint(lir, call);
    }

    // Call anything, using the most generic code.
    LCallGeneric *lir = new(alloc()) LCallGeneric(useFixed(call->getFunction(), CallTempReg0),
                                                  tempFixed(ArgumentsRectifierReg),
                                                  tempFixed(CallTempReg2));
    return defineReturn(lir, call) && assignSafepoint(lir, call);
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

    LApplyArgsGeneric *lir = new(alloc()) LApplyArgsGeneric(
        useFixed(apply->getFunction(), CallTempReg3),
        useFixed(apply->getArgc(), CallTempReg0),
        tempFixed(CallTempReg1),  // object register
        tempFixed(CallTempReg2)); // copy register

    MDefinition *self = apply->getThis();
    if (!useBoxFixed(lir, LApplyArgsGeneric::ThisIndex, self, CallTempReg4, CallTempReg5))
        return false;

    // Bailout is only needed in the case of possible non-JSFunction callee.
    if (!apply->getSingleTarget() && !assignSnapshot(lir, Bailout_NonJSFunctionCallee))
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
    LBail *lir = new(alloc()) LBail();
    return assignSnapshot(lir, bail->bailoutKind()) && add(lir, bail);
}

bool
LIRGenerator::visitUnreachable(MUnreachable *unreachable)
{
    LUnreachable *lir = new(alloc()) LUnreachable();
    return add(lir, unreachable);
}

bool
LIRGenerator::visitAssertFloat32(MAssertFloat32 *assertion)
{
    MIRType type = assertion->input()->type();
    DebugOnly<bool> checkIsFloat32 = assertion->mustBeFloat32();

    if (!allowFloat32Optimizations())
        return true;

    if (type != MIRType_Value && !js_JitOptions.eagerCompilation) {
        JS_ASSERT_IF(checkIsFloat32, type == MIRType_Float32);
        JS_ASSERT_IF(!checkIsFloat32, type != MIRType_Float32);
    }
    return true;
}

bool
LIRGenerator::visitArraySplice(MArraySplice *ins)
{
    LArraySplice *lir = new(alloc()) LArraySplice(useRegisterAtStart(ins->object()),
                                                  useRegisterAtStart(ins->start()),
                                                  useRegisterAtStart(ins->deleteCount()));
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetDynamicName(MGetDynamicName *ins)
{
    MDefinition *scopeChain = ins->getScopeChain();
    JS_ASSERT(scopeChain->type() == MIRType_Object);

    MDefinition *name = ins->getName();
    JS_ASSERT(name->type() == MIRType_String);

    LGetDynamicName *lir = new(alloc()) LGetDynamicName(useFixed(scopeChain, CallTempReg0),
                                                        useFixed(name, CallTempReg1),
                                                        tempFixed(CallTempReg2),
                                                        tempFixed(CallTempReg3),
                                                        tempFixed(CallTempReg4));

    return assignSnapshot(lir, Bailout_DynamicNameNotFound) && defineReturn(lir, ins);
}

bool
LIRGenerator::visitFilterArgumentsOrEval(MFilterArgumentsOrEval *ins)
{
    MDefinition *string = ins->getString();
    MOZ_ASSERT(string->type() == MIRType_String || string->type() == MIRType_Value);

    LInstruction *lir;
    if (string->type() == MIRType_String) {
        lir = new(alloc()) LFilterArgumentsOrEvalS(useFixed(string, CallTempReg0),
                                                   tempFixed(CallTempReg1),
                                                   tempFixed(CallTempReg2));
    } else {
        lir = new(alloc()) LFilterArgumentsOrEvalV(tempFixed(CallTempReg0),
                                                   tempFixed(CallTempReg1),
                                                   tempFixed(CallTempReg2));
        if (!useBoxFixed(lir, LFilterArgumentsOrEvalV::Input, string,
                         CallTempReg3, CallTempReg4))
        {
            return false;
        }
    }

    return assignSnapshot(lir, Bailout_StringArgumentsEval)
           && add(lir, ins)
           && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallDirectEval(MCallDirectEval *ins)
{
    MDefinition *scopeChain = ins->getScopeChain();
    JS_ASSERT(scopeChain->type() == MIRType_Object);

    MDefinition *string = ins->getString();
    JS_ASSERT(string->type() == MIRType_String || string->type() == MIRType_Value);

    MDefinition *thisValue = ins->getThisValue();


    LInstruction *lir;
    if (string->type() == MIRType_String) {
        lir = new(alloc()) LCallDirectEvalS(useRegisterAtStart(scopeChain),
                                            useRegisterAtStart(string));
    } else {
        lir = new(alloc()) LCallDirectEvalV(useRegisterAtStart(scopeChain));
        if (!useBoxAtStart(lir, LCallDirectEvalV::Argument, string))
            return false;
    }

    if (string->type() == MIRType_String) {
        if (!useBoxAtStart(lir, LCallDirectEvalS::ThisValue, thisValue))
            return false;
    } else {
        if (!useBoxAtStart(lir, LCallDirectEvalV::ThisValue, thisValue))
            return false;
    }

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
        return ReverseCompareOp(op);
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

    // lhs and rhs are used by the commutative operator. If they have any
    // *other* uses besides, try to reorder to avoid clobbering them. To
    // be fully precise, we should check whether this is the *last* use,
    // but checking hasOneDefUse() is a decent approximation which doesn't
    // require any extra analysis.
    JS_ASSERT(lhs->hasDefUses());
    JS_ASSERT(rhs->hasDefUses());
    if (lhs->isConstant() || (rhs->hasOneDefUse() && !lhs->hasOneDefUse())) {
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
    MOZ_ASSERT(opd->type() != MIRType_String);

    if (opd->type() == MIRType_Value) {
        LDefinition temp0, temp1;
        if (test->operandMightEmulateUndefined()) {
            temp0 = temp();
            temp1 = temp();
        } else {
            temp0 = LDefinition::BogusTemp();
            temp1 = LDefinition::BogusTemp();
        }
        LTestVAndBranch *lir = new(alloc()) LTestVAndBranch(ifTrue, ifFalse, tempDouble(), temp0, temp1);
        if (!useBox(lir, LTestVAndBranch::Input, opd))
            return false;
        return add(lir, test);
    }

    if (opd->type() == MIRType_Object) {
        // If the object might emulate undefined, we have to test for that.
        if (test->operandMightEmulateUndefined())
            return add(new(alloc()) LTestOAndBranch(useRegister(opd), ifTrue, ifFalse, temp()), test);

        // Otherwise we know it's truthy.
        return add(new(alloc()) LGoto(ifTrue));
    }

    // These must be explicitly sniffed out since they are constants and have
    // no payload.
    if (opd->type() == MIRType_Undefined || opd->type() == MIRType_Null)
        return add(new(alloc()) LGoto(ifFalse));

    // All symbols are truthy.
    if (opd->type() == MIRType_Symbol)
        return add(new(alloc()) LGoto(ifTrue));

    // Constant Double operand.
    if (opd->type() == MIRType_Double && opd->isConstant()) {
        bool result = opd->toConstant()->valueToBoolean();
        return add(new(alloc()) LGoto(result ? ifTrue : ifFalse));
    }

    // Constant Float32 operand.
    if (opd->type() == MIRType_Float32 && opd->isConstant()) {
        bool result = opd->toConstant()->valueToBoolean();
        return add(new(alloc()) LGoto(result ? ifTrue : ifFalse));
    }

    // Constant Int32 operand.
    if (opd->type() == MIRType_Int32 && opd->isConstant()) {
        int32_t num = opd->toConstant()->value().toInt32();
        return add(new(alloc()) LGoto(num ? ifTrue : ifFalse));
    }

    // Constant Boolean operand.
    if (opd->type() == MIRType_Boolean && opd->isConstant()) {
        bool result = opd->toConstant()->value().toBoolean();
        return add(new(alloc()) LGoto(result ? ifTrue : ifFalse));
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
            return add(new(alloc()) LGoto(result ? ifTrue : ifFalse));

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
                    new(alloc()) LEmulatesUndefinedAndBranch(comp, useRegister(left),
                                                             ifTrue, ifFalse, temp());
                return add(lir, test);
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
                new(alloc()) LIsNullOrLikeUndefinedAndBranch(comp, ifTrue, ifFalse,
                                                             tmp, tmpToUnbox);
            if (!useBox(lir, LIsNullOrLikeUndefinedAndBranch::Value, left))
                return false;
            return add(lir, test);
        }

        // Compare and branch booleans.
        if (comp->compareType() == MCompare::Compare_Boolean) {
            JS_ASSERT(left->type() == MIRType_Value);
            JS_ASSERT(right->type() == MIRType_Boolean);

            LAllocation rhs = useRegisterOrConstant(right);
            LCompareBAndBranch *lir = new(alloc()) LCompareBAndBranch(comp, rhs, ifTrue, ifFalse);
            if (!useBox(lir, LCompareBAndBranch::Lhs, left))
                return false;
            return add(lir, test);
        }

        // Compare and branch Int32 or Object pointers.
        if (comp->isInt32Comparison() ||
            comp->compareType() == MCompare::Compare_UInt32 ||
            comp->compareType() == MCompare::Compare_Object)
        {
            JSOp op = ReorderComparison(comp->jsop(), &left, &right);
            LAllocation lhs = useRegister(left);
            LAllocation rhs;
            if (comp->isInt32Comparison() || comp->compareType() == MCompare::Compare_UInt32)
                rhs = useAnyOrConstant(right);
            else
                rhs = useRegister(right);
            LCompareAndBranch *lir = new(alloc()) LCompareAndBranch(comp, op, lhs, rhs,
                                                                    ifTrue, ifFalse);
            return add(lir, test);
        }

        // Compare and branch doubles.
        if (comp->isDoubleComparison()) {
            LAllocation lhs = useRegister(left);
            LAllocation rhs = useRegister(right);
            LCompareDAndBranch *lir = new(alloc()) LCompareDAndBranch(comp, lhs, rhs,
                                                                      ifTrue, ifFalse);
            return add(lir, test);
        }

        // Compare and branch floats.
        if (comp->isFloat32Comparison()) {
            LAllocation lhs = useRegister(left);
            LAllocation rhs = useRegister(right);
            LCompareFAndBranch *lir = new(alloc()) LCompareFAndBranch(comp, lhs, rhs,
                                                                      ifTrue, ifFalse);
            return add(lir, test);
        }

        // Compare values.
        if (comp->compareType() == MCompare::Compare_Value) {
            LCompareVAndBranch *lir = new(alloc()) LCompareVAndBranch(comp, ifTrue, ifFalse);
            if (!useBoxAtStart(lir, LCompareVAndBranch::LhsInput, left))
                return false;
            if (!useBoxAtStart(lir, LCompareVAndBranch::RhsInput, right))
                return false;
            return add(lir, test);
        }
    }

    // Check if the operand for this test is a bitand operation. If it is, we want
    // to emit an LBitAndAndBranch rather than an LTest*AndBranch.
    if (opd->isBitAnd() && opd->isEmittedAtUses()) {
        MDefinition *lhs = opd->getOperand(0);
        MDefinition *rhs = opd->getOperand(1);
        if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
            ReorderCommutative(&lhs, &rhs);
            return lowerForBitAndAndBranch(new(alloc()) LBitAndAndBranch(ifTrue, ifFalse), test, lhs, rhs);
        }
    }

    if (opd->type() == MIRType_Double)
        return add(new(alloc()) LTestDAndBranch(useRegister(opd), ifTrue, ifFalse));

    if (opd->type() == MIRType_Float32)
        return add(new(alloc()) LTestFAndBranch(useRegister(opd), ifTrue, ifFalse));

    JS_ASSERT(opd->type() == MIRType_Int32 || opd->type() == MIRType_Boolean);
    return add(new(alloc()) LTestIAndBranch(useRegister(opd), ifTrue, ifFalse));
}

bool
LIRGenerator::visitFunctionDispatch(MFunctionDispatch *ins)
{
    LFunctionDispatch *lir = new(alloc()) LFunctionDispatch(useRegister(ins->input()));
    return add(lir, ins);
}

bool
LIRGenerator::visitTypeObjectDispatch(MTypeObjectDispatch *ins)
{
    LTypeObjectDispatch *lir = new(alloc()) LTypeObjectDispatch(useRegister(ins->input()), temp());
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
        return define(new(alloc()) LInteger(result), comp);

    // Move below the emitAtUses call if we ever implement
    // LCompareSAndBranch. Doing this now wouldn't be wrong, but doesn't
    // make sense and avoids confusion.
    if (comp->compareType() == MCompare::Compare_String) {
        LCompareS *lir = new(alloc()) LCompareS(useRegister(left), useRegister(right));
        if (!define(lir, comp))
            return false;
        return assignSafepoint(lir, comp);
    }

    // Strict compare between value and string
    if (comp->compareType() == MCompare::Compare_StrictString) {
        JS_ASSERT(left->type() == MIRType_Value);
        JS_ASSERT(right->type() == MIRType_String);

        LCompareStrictS *lir = new(alloc()) LCompareStrictS(useRegister(right), tempToUnbox());
        if (!useBox(lir, LCompareStrictS::Lhs, left))
            return false;
        if (!define(lir, comp))
            return false;
        return assignSafepoint(lir, comp);
    }

    // Unknown/unspecialized compare use a VM call.
    if (comp->compareType() == MCompare::Compare_Unknown) {
        LCompareVM *lir = new(alloc()) LCompareVM();
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

            return define(new(alloc()) LEmulatesUndefined(useRegister(left)), comp);
        }

        LDefinition tmp, tmpToUnbox;
        if (comp->operandMightEmulateUndefined()) {
            tmp = temp();
            tmpToUnbox = tempToUnbox();
        } else {
            tmp = LDefinition::BogusTemp();
            tmpToUnbox = LDefinition::BogusTemp();
        }

        LIsNullOrLikeUndefined *lir = new(alloc()) LIsNullOrLikeUndefined(tmp, tmpToUnbox);
        if (!useBox(lir, LIsNullOrLikeUndefined::Value, left))
            return false;
        return define(lir, comp);
    }

    // Compare booleans.
    if (comp->compareType() == MCompare::Compare_Boolean) {
        JS_ASSERT(left->type() == MIRType_Value);
        JS_ASSERT(right->type() == MIRType_Boolean);

        LCompareB *lir = new(alloc()) LCompareB(useRegisterOrConstant(right));
        if (!useBox(lir, LCompareB::Lhs, left))
            return false;
        return define(lir, comp);
    }

    // Compare Int32 or Object pointers.
    if (comp->isInt32Comparison() ||
        comp->compareType() == MCompare::Compare_UInt32 ||
        comp->compareType() == MCompare::Compare_Object)
    {
        JSOp op = ReorderComparison(comp->jsop(), &left, &right);
        LAllocation lhs = useRegister(left);
        LAllocation rhs;
        if (comp->isInt32Comparison() ||
            comp->compareType() == MCompare::Compare_UInt32)
        {
            rhs = useAnyOrConstant(right);
        } else {
            rhs = useRegister(right);
        }
        return define(new(alloc()) LCompare(op, lhs, rhs), comp);
    }

    // Compare doubles.
    if (comp->isDoubleComparison())
        return define(new(alloc()) LCompareD(useRegister(left), useRegister(right)), comp);

    // Compare float32.
    if (comp->isFloat32Comparison())
        return define(new(alloc()) LCompareF(useRegister(left), useRegister(right)), comp);

    // Compare values.
    if (comp->compareType() == MCompare::Compare_Value) {
        LCompareV *lir = new(alloc()) LCompareV();
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
        return lowerForALU(new(alloc()) LBitOpI(op), ins, lhs, rhs);
    }

    LBitOpV *lir = new(alloc()) LBitOpV(op);
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

    LTypeOfV *lir = new(alloc()) LTypeOfV(tempToUnbox());
    if (!useBox(lir, LTypeOfV::Input, opd))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitToId(MToId *ins)
{
    LToIdV *lir = new(alloc()) LToIdV(tempDouble());
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
        return lowerForALU(new(alloc()) LBitNotI(), ins, input);

    LBitNotV *lir = new(alloc()) LBitNotV;
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

    MUseIterator iter(ins->usesBegin());
    if (iter == ins->usesEnd())
        return false;

    MNode *node = iter->consumer();
    if (!node->isDefinition())
        return false;

    if (!node->toDefinition()->isTest())
        return false;

    iter++;
    return iter == ins->usesEnd();
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

        LShiftI *lir = new(alloc()) LShiftI(op);
        if (op == JSOP_URSH) {
            if (ins->toUrsh()->fallible() && !assignSnapshot(lir, Bailout_OverflowInvalidate))
                return false;
        }
        return lowerForShift(lir, ins, lhs, rhs);
    }

    JS_ASSERT(ins->specialization() == MIRType_None);

    if (op == JSOP_URSH) {
        // Result is either int32 or double so we have to use BinaryV.
        return lowerBinaryV(JSOP_URSH, ins);
    }

    LBitOpV *lir = new(alloc()) LBitOpV(op);
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
    MIRType type = ins->num()->type();
    JS_ASSERT(IsFloatingPointType(type));

    if (type == MIRType_Double) {
        LFloor *lir = new(alloc()) LFloor(useRegister(ins->num()));
        if (!assignSnapshot(lir, Bailout_Round))
            return false;
        return define(lir, ins);
    }

    LFloorF *lir = new(alloc()) LFloorF(useRegister(ins->num()));
    if (!assignSnapshot(lir, Bailout_Round))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitCeil(MCeil *ins)
{
    MIRType type = ins->num()->type();
    JS_ASSERT(IsFloatingPointType(type));

    if (type == MIRType_Double) {
        LCeil *lir = new(alloc()) LCeil(useRegister(ins->num()));
        if (!assignSnapshot(lir, Bailout_Round))
            return false;
        return define(lir, ins);
    }

    LCeilF *lir = new(alloc()) LCeilF(useRegister(ins->num()));
    if (!assignSnapshot(lir, Bailout_Round))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitRound(MRound *ins)
{
    MIRType type = ins->num()->type();
    JS_ASSERT(IsFloatingPointType(type));

    if (type == MIRType_Double) {
        LRound *lir = new (alloc()) LRound(useRegister(ins->num()), tempDouble());
        if (!assignSnapshot(lir, Bailout_Round))
            return false;
        return define(lir, ins);
    }

    LRoundF *lir = new (alloc()) LRoundF(useRegister(ins->num()), tempFloat32());
    if (!assignSnapshot(lir, Bailout_Round))
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
        LMinMaxI *lir = new(alloc()) LMinMaxI(useRegisterAtStart(first), useRegisterOrConstant(second));
        return defineReuseInput(lir, ins, 0);
    }

    LMinMaxD *lir = new(alloc()) LMinMaxD(useRegisterAtStart(first), useRegister(second));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGenerator::visitAbs(MAbs *ins)
{
    MDefinition *num = ins->num();
    JS_ASSERT(IsNumberType(num->type()));

    if (num->type() == MIRType_Int32) {
        LAbsI *lir = new(alloc()) LAbsI(useRegisterAtStart(num));
        // needed to handle abs(INT32_MIN)
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Overflow))
            return false;
        return defineReuseInput(lir, ins, 0);
    }
    if (num->type() == MIRType_Float32) {
        LAbsF *lir = new(alloc()) LAbsF(useRegisterAtStart(num));
        return defineReuseInput(lir, ins, 0);
    }

    LAbsD *lir = new(alloc()) LAbsD(useRegisterAtStart(num));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGenerator::visitSqrt(MSqrt *ins)
{
    MDefinition *num = ins->num();
    JS_ASSERT(IsFloatingPointType(num->type()));
    if (num->type() == MIRType_Double) {
        LSqrtD *lir = new(alloc()) LSqrtD(useRegisterAtStart(num));
        return define(lir, ins);
    }

    LSqrtF *lir = new(alloc()) LSqrtF(useRegisterAtStart(num));
    return define(lir, ins);
}

bool
LIRGenerator::visitAtan2(MAtan2 *ins)
{
    MDefinition *y = ins->y();
    JS_ASSERT(y->type() == MIRType_Double);

    MDefinition *x = ins->x();
    JS_ASSERT(x->type() == MIRType_Double);

    LAtan2D *lir = new(alloc()) LAtan2D(useRegisterAtStart(y), useRegisterAtStart(x), tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitHypot(MHypot *ins)
{
    MDefinition *x = ins->x();
    JS_ASSERT(x->type() == MIRType_Double);

    MDefinition *y = ins->y();
    JS_ASSERT(y->type() == MIRType_Double);

    LHypot *lir = new(alloc()) LHypot(useRegisterAtStart(x), useRegisterAtStart(y), tempFixed(CallTempReg0));
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
        LPowI *lir = new(alloc()) LPowI(useRegisterAtStart(input), useFixed(power, CallTempReg1),
                                        tempFixed(CallTempReg0));
        return defineReturn(lir, ins);
    }

    LPowD *lir = new(alloc()) LPowD(useRegisterAtStart(input), useRegisterAtStart(power),
                                    tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitRandom(MRandom *ins)
{
    LRandom *lir = new(alloc()) LRandom(tempFixed(CallTempReg0), tempFixed(CallTempReg1));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitMathFunction(MMathFunction *ins)
{
    JS_ASSERT(IsFloatingPointType(ins->type()));
    JS_ASSERT(ins->type() == ins->input()->type());

    if (ins->type() == MIRType_Double) {
        // Note: useRegisterAtStart is safe here, the temp is not a FP register.
        LMathFunctionD *lir = new(alloc()) LMathFunctionD(useRegisterAtStart(ins->input()),
                                                          tempFixed(CallTempReg0));
        return defineReturn(lir, ins);
    }

    LMathFunctionF *lir = new(alloc()) LMathFunctionF(useRegisterAtStart(ins->input()),
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
        LAddI *lir = new(alloc()) LAddI;

        if (ins->fallible() && !assignSnapshot(lir, Bailout_OverflowInvalidate))
            return false;

        if (!lowerForALU(lir, ins, lhs, rhs))
            return false;

        MaybeSetRecoversInput(ins, lir);
        return true;
    }

    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        ReorderCommutative(&lhs, &rhs);
        return lowerForFPU(new(alloc()) LMathD(JSOP_ADD), ins, lhs, rhs);
    }

    if (ins->specialization() == MIRType_Float32) {
        JS_ASSERT(lhs->type() == MIRType_Float32);
        ReorderCommutative(&lhs, &rhs);
        return lowerForFPU(new(alloc()) LMathF(JSOP_ADD), ins, lhs, rhs);
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

        LSubI *lir = new(alloc()) LSubI;
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Overflow))
            return false;

        if (!lowerForALU(lir, ins, lhs, rhs))
            return false;

        MaybeSetRecoversInput(ins, lir);
        return true;
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        return lowerForFPU(new(alloc()) LMathD(JSOP_SUB), ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Float32) {
        JS_ASSERT(lhs->type() == MIRType_Float32);
        return lowerForFPU(new(alloc()) LMathF(JSOP_SUB), ins, lhs, rhs);
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

        // If our RHS is a constant -1 and we don't have to worry about
        // overflow, we can optimize to an LNegI.
        if (!ins->fallible() && rhs->isConstant() && rhs->toConstant()->value() == Int32Value(-1))
            return defineReuseInput(new(alloc()) LNegI(useRegisterAtStart(lhs)), ins, 0);

        return lowerMulI(ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Double) {
        JS_ASSERT(lhs->type() == MIRType_Double);
        ReorderCommutative(&lhs, &rhs);

        // If our RHS is a constant -1.0, we can optimize to an LNegD.
        if (rhs->isConstant() && rhs->toConstant()->value() == DoubleValue(-1.0))
            return defineReuseInput(new(alloc()) LNegD(useRegisterAtStart(lhs)), ins, 0);

        return lowerForFPU(new(alloc()) LMathD(JSOP_MUL), ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Float32) {
        JS_ASSERT(lhs->type() == MIRType_Float32);
        ReorderCommutative(&lhs, &rhs);

        // We apply the same optimizations as for doubles
        if (rhs->isConstant() && rhs->toConstant()->value() == Float32Value(-1.0f))
            return defineReuseInput(new(alloc()) LNegF(useRegisterAtStart(lhs)), ins, 0);

        return lowerForFPU(new(alloc()) LMathF(JSOP_MUL), ins, lhs, rhs);
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
        return lowerForFPU(new(alloc()) LMathD(JSOP_DIV), ins, lhs, rhs);
    }
    if (ins->specialization() == MIRType_Float32) {
        JS_ASSERT(lhs->type() == MIRType_Float32);
        return lowerForFPU(new(alloc()) LMathF(JSOP_DIV), ins, lhs, rhs);
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
        LModD *lir = new(alloc()) LModD(useRegisterAtStart(ins->lhs()), useRegisterAtStart(ins->rhs()),
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

    LBinaryV *lir = new(alloc()) LBinaryV(op);
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

    LConcat *lir = new(alloc()) LConcat(useFixedAtStart(lhs, CallTempReg0),
                                        useFixedAtStart(rhs, CallTempReg1),
                                        tempFixed(CallTempReg0),
                                        tempFixed(CallTempReg1),
                                        tempFixed(CallTempReg2),
                                        tempFixed(CallTempReg3),
                                        tempFixed(CallTempReg4));
    if (!defineFixed(lir, ins, LAllocation(AnyRegister(CallTempReg5))))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitConcatPar(MConcatPar *ins)
{
    MDefinition *cx = ins->forkJoinContext();
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_String);
    JS_ASSERT(rhs->type() == MIRType_String);
    JS_ASSERT(ins->type() == MIRType_String);

    LConcatPar *lir = new(alloc()) LConcatPar(useFixed(cx, CallTempReg4),
                                              useFixedAtStart(lhs, CallTempReg0),
                                              useFixedAtStart(rhs, CallTempReg1),
                                              tempFixed(CallTempReg0),
                                              tempFixed(CallTempReg1),
                                              tempFixed(CallTempReg2),
                                              tempFixed(CallTempReg3));
    if (!defineFixed(lir, ins, LAllocation(AnyRegister(CallTempReg5))))
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

    LCharCodeAt *lir = new(alloc()) LCharCodeAt(useRegister(str), useRegister(idx));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitFromCharCode(MFromCharCode *ins)
{
    MDefinition *code = ins->getOperand(0);

    JS_ASSERT(code->type() == MIRType_Int32);

    LFromCharCode *lir = new(alloc()) LFromCharCode(useRegister(code));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStart(MStart *start)
{
    // Create a snapshot that captures the initial state of the function.
    LStart *lir = new(alloc()) LStart;
    if (!assignSnapshot(lir, Bailout_InitialState))
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
LIRGenerator::visitLimitedTruncate(MLimitedTruncate *nop)
{
    return redefine(nop, nop->input());
}

bool
LIRGenerator::visitOsrEntry(MOsrEntry *entry)
{
    LOsrEntry *lir = new(alloc()) LOsrEntry;
    return defineFixed(lir, entry, LAllocation(AnyRegister(OsrFrameReg)));
}

bool
LIRGenerator::visitOsrValue(MOsrValue *value)
{
    LOsrValue *lir = new(alloc()) LOsrValue(useRegister(value->entry()));
    return defineBox(lir, value);
}

bool
LIRGenerator::visitOsrReturnValue(MOsrReturnValue *value)
{
    LOsrReturnValue *lir = new(alloc()) LOsrReturnValue(useRegister(value->entry()));
    return defineBox(lir, value);
}

bool
LIRGenerator::visitOsrScopeChain(MOsrScopeChain *object)
{
    LOsrScopeChain *lir = new(alloc()) LOsrScopeChain(useRegister(object->entry()));
    return define(lir, object);
}

bool
LIRGenerator::visitOsrArgumentsObject(MOsrArgumentsObject *object)
{
    LOsrArgumentsObject *lir = new(alloc()) LOsrArgumentsObject(useRegister(object->entry()));
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
        LValueToDouble *lir = new(alloc()) LValueToDouble();
        if (!useBox(lir, LValueToDouble::Input, opd))
            return false;
        return assignSnapshot(lir, Bailout_NonPrimitiveInput) && define(lir, convert);
      }

      case MIRType_Null:
        JS_ASSERT(conversion != MToDouble::NumbersOnly && conversion != MToDouble::NonNullNonStringPrimitives);
        return lowerConstantDouble(0, convert);

      case MIRType_Undefined:
        JS_ASSERT(conversion != MToDouble::NumbersOnly);
        return lowerConstantDouble(GenericNaN(), convert);

      case MIRType_Boolean:
        JS_ASSERT(conversion != MToDouble::NumbersOnly);
        /* FALLTHROUGH */

      case MIRType_Int32:
      {
        LInt32ToDouble *lir = new(alloc()) LInt32ToDouble(useRegister(opd));
        return define(lir, convert);
      }

      case MIRType_Float32:
      {
        LFloat32ToDouble *lir = new(alloc()) LFloat32ToDouble(useRegisterAtStart(opd));
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
LIRGenerator::visitToFloat32(MToFloat32 *convert)
{
    MDefinition *opd = convert->input();
    mozilla::DebugOnly<MToFloat32::ConversionKind> conversion = convert->conversion();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToFloat32 *lir = new(alloc()) LValueToFloat32();
        if (!useBox(lir, LValueToFloat32::Input, opd))
            return false;
        return assignSnapshot(lir, Bailout_NonPrimitiveInput) && define(lir, convert);
      }

      case MIRType_Null:
        JS_ASSERT(conversion != MToFloat32::NonStringPrimitives);
        return lowerConstantFloat32(0, convert);

      case MIRType_Undefined:
        JS_ASSERT(conversion != MToFloat32::NumbersOnly);
        return lowerConstantFloat32(GenericNaN(), convert);

      case MIRType_Boolean:
        JS_ASSERT(conversion != MToFloat32::NumbersOnly);
        /* FALLTHROUGH */

      case MIRType_Int32:
      {
        LInt32ToFloat32 *lir = new(alloc()) LInt32ToFloat32(useRegister(opd));
        return define(lir, convert);
      }

      case MIRType_Double:
      {
        LDoubleToFloat32 *lir = new(alloc()) LDoubleToFloat32(useRegister(opd));
        return define(lir, convert);
      }

      case MIRType_Float32:
        return redefine(convert, opd);

      default:
        // Objects might be effectful.
        // Strings are complicated - we don't handle them yet.
        MOZ_ASSUME_UNREACHABLE("unexpected type");
        return false;
    }
}

bool
LIRGenerator::visitToInt32(MToInt32 *convert)
{
    MDefinition *opd = convert->input();

    switch (opd->type()) {
      case MIRType_Value:
      {
        LValueToInt32 *lir =
            new(alloc()) LValueToInt32(tempDouble(), temp(), LValueToInt32::NORMAL);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return assignSnapshot(lir, Bailout_NonPrimitiveInput)
               && define(lir, convert)
               && assignSafepoint(lir, convert);
      }

      case MIRType_Null:
        return define(new(alloc()) LInteger(0), convert);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(convert, opd);

      case MIRType_Float32:
      {
        LFloat32ToInt32 *lir = new(alloc()) LFloat32ToInt32(useRegister(opd));
        return assignSnapshot(lir, Bailout_PrecisionLoss) && define(lir, convert);
      }

      case MIRType_Double:
      {
        LDoubleToInt32 *lir = new(alloc()) LDoubleToInt32(useRegister(opd));
        return assignSnapshot(lir, Bailout_PrecisionLoss) && define(lir, convert);
      }

      case MIRType_String:
      case MIRType_Symbol:
      case MIRType_Object:
      case MIRType_Undefined:
        // Objects might be effectful. Undefined and symbols coerce to NaN, not int32.
        MOZ_ASSUME_UNREACHABLE("ToInt32 invalid input type");
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
        LValueToInt32 *lir = new(alloc()) LValueToInt32(tempDouble(), temp(),
                                                        LValueToInt32::TRUNCATE);
        if (!useBox(lir, LValueToInt32::Input, opd))
            return false;
        return assignSnapshot(lir, Bailout_NonPrimitiveInput)
               && define(lir, truncate)
               && assignSafepoint(lir, truncate);
      }

      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Symbol:
        return define(new(alloc()) LInteger(0), truncate);

      case MIRType_Int32:
      case MIRType_Boolean:
        return redefine(truncate, opd);

      case MIRType_Double:
        return lowerTruncateDToInt32(truncate);

      case MIRType_Float32:
        return lowerTruncateFToInt32(truncate);

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
      case MIRType_Null: {
        const JSAtomState &names = GetIonContext()->runtime->names();
        LPointer *lir = new(alloc()) LPointer(names.null);
        return define(lir, ins);
      }

      case MIRType_Undefined: {
        const JSAtomState &names = GetIonContext()->runtime->names();
        LPointer *lir = new(alloc()) LPointer(names.undefined);
        return define(lir, ins);
      }

      case MIRType_Boolean: {
        LBooleanToString *lir = new(alloc()) LBooleanToString(useRegister(opd));
        return define(lir, ins);
      }

      case MIRType_Double: {
        LDoubleToString *lir = new(alloc()) LDoubleToString(useRegister(opd), temp());

        if (!define(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      case MIRType_Int32: {
        LIntToString *lir = new(alloc()) LIntToString(useRegister(opd));

        if (!define(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      case MIRType_String:
        return redefine(ins, ins->input());

      case MIRType_Value: {
        LValueToString *lir = new(alloc()) LValueToString(tempToUnbox());
        if (!useBox(lir, LValueToString::Input, opd))
            return false;
        if (ins->fallible() && !assignSnapshot(lir, Bailout_NonPrimitiveInput))
            return false;
        if (!define(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
      }

      default:
        // Float32 and objects are not supported.
        MOZ_ASSUME_UNREACHABLE("unexpected type");
    }
}

static bool
MustCloneRegExpForCall(MCall *call, uint32_t useIndex)
{
    // We have a regex literal flowing into a call. Return |false| iff
    // this is a native call that does not let the regex escape.

    JSFunction *target = call->getSingleTarget();
    if (!target || !target->isNative())
        return true;

    if (useIndex == MCall::IndexOfThis() &&
        (target->native() == regexp_exec || target->native() == regexp_test))
    {
        return false;
    }

    if (useIndex == MCall::IndexOfArgument(0) &&
        (target->native() == str_split ||
         target->native() == str_replace ||
         target->native() == str_match ||
         target->native() == str_search))
    {
        return false;
    }

    return true;
}


static bool
MustCloneRegExp(MRegExp *regexp)
{
    if (regexp->mustClone())
        return true;

    // If this regex literal only flows into known natives that don't let
    // it escape, we don't have to clone it.

    for (MUseIterator iter(regexp->usesBegin()); iter != regexp->usesEnd(); iter++) {
        MNode *node = iter->consumer();
        if (!node->isDefinition())
            return true;

        MDefinition *def = node->toDefinition();
        if (def->isRegExpTest()) {
            MRegExpTest *test = def->toRegExpTest();
            if (test->indexOf(*iter) == 1) {
                // Optimized RegExp.prototype.test.
                JS_ASSERT(test->regexp() == regexp);
                continue;
            }
        } else if (def->isCall()) {
            MCall *call = def->toCall();
            if (!MustCloneRegExpForCall(call, call->indexOf(*iter)))
                continue;
        }

        return true;
    }
    return false;
}

bool
LIRGenerator::visitRegExp(MRegExp *ins)
{
    if (!MustCloneRegExp(ins)) {
        RegExpObject *source = ins->source();
        return define(new(alloc()) LPointer(source), ins);
    }

    LRegExp *lir = new(alloc()) LRegExp();
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRegExpExec(MRegExpExec *ins)
{
    JS_ASSERT(ins->regexp()->type() == MIRType_Object);
    JS_ASSERT(ins->string()->type() == MIRType_String);

    LRegExpExec *lir = new(alloc()) LRegExpExec(useRegisterAtStart(ins->regexp()),
                                                useRegisterAtStart(ins->string()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRegExpTest(MRegExpTest *ins)
{
    JS_ASSERT(ins->regexp()->type() == MIRType_Object);
    JS_ASSERT(ins->string()->type() == MIRType_String);

    LRegExpTest *lir = new(alloc()) LRegExpTest(useRegisterAtStart(ins->regexp()),
                                                useRegisterAtStart(ins->string()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRegExpReplace(MRegExpReplace *ins)
{
    JS_ASSERT(ins->pattern()->type() == MIRType_Object);
    JS_ASSERT(ins->string()->type() == MIRType_String);
    JS_ASSERT(ins->replacement()->type() == MIRType_String);

    LRegExpReplace *lir = new(alloc()) LRegExpReplace(useRegisterOrConstantAtStart(ins->string()),
                                                      useRegisterAtStart(ins->pattern()),
                                                      useRegisterOrConstantAtStart(ins->replacement()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStringReplace(MStringReplace *ins)
{
    JS_ASSERT(ins->pattern()->type() == MIRType_String);
    JS_ASSERT(ins->string()->type() == MIRType_String);
    JS_ASSERT(ins->replacement()->type() == MIRType_String);

    LStringReplace *lir = new(alloc()) LStringReplace(useRegisterOrConstantAtStart(ins->string()),
                                                      useRegisterAtStart(ins->pattern()),
                                                      useRegisterOrConstantAtStart(ins->replacement()));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLambda(MLambda *ins)
{
    if (ins->info().singletonType || ins->info().useNewTypeForClone) {
        // If the function has a singleton type, this instruction will only be
        // executed once so we don't bother inlining it.
        //
        // If UseNewTypeForClone is true, we will assign a singleton type to
        // the clone and we have to clone the script, we can't do that inline.
        LLambdaForSingleton *lir = new(alloc()) LLambdaForSingleton(useRegisterAtStart(ins->scopeChain()));
        return defineReturn(lir, ins) && assignSafepoint(lir, ins);
    }

    LLambda *lir = new(alloc()) LLambda(useRegister(ins->scopeChain()), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLambdaArrow(MLambdaArrow *ins)
{
    MOZ_ASSERT(ins->scopeChain()->type() == MIRType_Object);
    MOZ_ASSERT(ins->thisDef()->type() == MIRType_Value);

    LLambdaArrow *lir = new(alloc()) LLambdaArrow(useRegister(ins->scopeChain()), temp());
    if (!useBox(lir, LLambdaArrow::ThisValue, ins->thisDef()))
        return false;
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLambdaPar(MLambdaPar *ins)
{
    JS_ASSERT(!ins->info().singletonType);
    JS_ASSERT(!ins->info().useNewTypeForClone);
    LLambdaPar *lir = new(alloc()) LLambdaPar(useRegister(ins->forkJoinContext()),
                                              useRegister(ins->scopeChain()),
                                              temp(), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitSlots(MSlots *ins)
{
    return define(new(alloc()) LSlots(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitElements(MElements *ins)
{
    return define(new(alloc()) LElements(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitConstantElements(MConstantElements *ins)
{
    return define(new(alloc()) LPointer(ins->value(), LPointer::NON_GC_THING), ins);
}

bool
LIRGenerator::visitConvertElementsToDoubles(MConvertElementsToDoubles *ins)
{
    LInstruction *check = new(alloc()) LConvertElementsToDoubles(useRegister(ins->elements()));
    return add(check, ins) && assignSafepoint(check, ins);
}

bool
LIRGenerator::visitMaybeToDoubleElement(MMaybeToDoubleElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->value()->type() == MIRType_Int32);

    LMaybeToDoubleElement *lir = new(alloc()) LMaybeToDoubleElement(useRegisterAtStart(ins->elements()),
                                                                    useRegisterAtStart(ins->value()),
                                                                    tempDouble());
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitLoadSlot(MLoadSlot *ins)
{
    switch (ins->type()) {
      case MIRType_Value:
        return defineBox(new(alloc()) LLoadSlotV(useRegister(ins->slots())), ins);

      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
        return define(new(alloc()) LLoadSlotT(useRegister(ins->slots())), ins);
    }
}

bool
LIRGenerator::visitFunctionEnvironment(MFunctionEnvironment *ins)
{
    return define(new(alloc()) LFunctionEnvironment(useRegisterAtStart(ins->function())), ins);
}

bool
LIRGenerator::visitForkJoinContext(MForkJoinContext *ins)
{
    LForkJoinContext *lir = new(alloc()) LForkJoinContext(tempFixed(CallTempReg0));
    return defineReturn(lir, ins);
}

bool
LIRGenerator::visitGuardThreadExclusive(MGuardThreadExclusive *ins)
{
    // FIXME (Bug 956281) -- For now, we always generate the most
    // general form of write guard check. we could employ TI feedback
    // to optimize this if we know that the object being tested is a
    // typed object or know that it is definitely NOT a typed object.
    LGuardThreadExclusive *lir =
        new(alloc()) LGuardThreadExclusive(useFixed(ins->forkJoinContext(), CallTempReg0),
                                           useFixed(ins->object(), CallTempReg1),
                                           tempFixed(CallTempReg2));
    lir->setMir(ins);
    return assignSnapshot(lir, Bailout_GuardThreadExclusive) && add(lir, ins);
}

bool
LIRGenerator::visitInterruptCheck(MInterruptCheck *ins)
{
    // Implicit interrupt checks require asm.js signal handlers to be
    // installed. ARM does not yet use implicit interrupt checks, see
    // bug 864220.
#ifndef JS_CODEGEN_ARM
    if (GetIonContext()->runtime->signalHandlersInstalled()) {
        LInterruptCheckImplicit *lir = new(alloc()) LInterruptCheckImplicit();
        return add(lir, ins) && assignSafepoint(lir, ins);
    }
#endif

    LInterruptCheck *lir = new(alloc()) LInterruptCheck();
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitInterruptCheckPar(MInterruptCheckPar *ins)
{
    LInterruptCheckPar *lir =
        new(alloc()) LInterruptCheckPar(useRegister(ins->forkJoinContext()), temp());
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewPar(MNewPar *ins)
{
    LNewPar *lir = new(alloc()) LNewPar(useRegister(ins->forkJoinContext()), temp(), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitNewDenseArrayPar(MNewDenseArrayPar *ins)
{
    JS_ASSERT(ins->forkJoinContext()->type() == MIRType_ForkJoinContext);
    JS_ASSERT(ins->length()->type() == MIRType_Int32);
    JS_ASSERT(ins->type() == MIRType_Object);

    LNewDenseArrayPar *lir =
        new(alloc()) LNewDenseArrayPar(useRegister(ins->forkJoinContext()),
                                       useRegister(ins->length()),
                                       temp(),
                                       temp(),
                                       temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStoreSlot(MStoreSlot *ins)
{
    LInstruction *lir;

    switch (ins->value()->type()) {
      case MIRType_Value:
        lir = new(alloc()) LStoreSlotV(useRegister(ins->slots()));
        if (!useBox(lir, LStoreSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);

      case MIRType_Double:
        return add(new(alloc()) LStoreSlotT(useRegister(ins->slots()), useRegister(ins->value())), ins);

      case MIRType_Float32:
        MOZ_ASSUME_UNREACHABLE("Float32 shouldn't be stored in a slot.");

      default:
        return add(new(alloc()) LStoreSlotT(useRegister(ins->slots()), useRegisterOrConstant(ins->value())),
                   ins);
    }

    return true;
}

bool
LIRGenerator::visitFilterTypeSet(MFilterTypeSet *ins)
{
    return redefine(ins, ins->input());
}

bool
LIRGenerator::visitTypeBarrier(MTypeBarrier *ins)
{
    // Requesting a non-GC pointer is safe here since we never re-enter C++
    // from inside a type barrier test.

    const types::TemporaryTypeSet *types = ins->resultTypeSet();
    bool needTemp = !types->unknownObject() && types->getObjectCount() > 0;

    MIRType inputType = ins->getOperand(0)->type();
    DebugOnly<MIRType> outputType = ins->type();

    JS_ASSERT(inputType == outputType);

    // Handle typebarrier that will always bail.
    // (Emit LBail for visibility).
    if (ins->alwaysBails()) {
        LBail *bail = new(alloc()) LBail();
        if (!assignSnapshot(bail, Bailout_Inevitable))
            return false;
        return redefine(ins, ins->input()) && add(bail, ins);
    }

    // Handle typebarrier with Value as input.
    if (inputType == MIRType_Value) {
        LDefinition tmp = needTemp ? temp() : tempToUnbox();
        LTypeBarrierV *barrier = new(alloc()) LTypeBarrierV(tmp);
        if (!useBox(barrier, LTypeBarrierV::Input, ins->input()))
            return false;
        if (!assignSnapshot(barrier, Bailout_TypeBarrierV))
            return false;
        return redefine(ins, ins->input()) && add(barrier, ins);
    }

    // Handle typebarrier with specific TypeObject/SingleObjects.
    if (inputType == MIRType_Object && !types->hasType(types::Type::AnyObjectType()) &&
        ins->barrierKind() != BarrierKind::TypeTagOnly)
    {
        LDefinition tmp = needTemp ? temp() : LDefinition::BogusTemp();
        LTypeBarrierO *barrier = new(alloc()) LTypeBarrierO(useRegister(ins->getOperand(0)), tmp);
        if (!assignSnapshot(barrier, Bailout_TypeBarrierO))
            return false;
        return redefine(ins, ins->getOperand(0)) && add(barrier, ins);
    }

    // Handle remaining cases: No-op, unbox did everything.
    return redefine(ins, ins->getOperand(0));
}

bool
LIRGenerator::visitMonitorTypes(MMonitorTypes *ins)
{
    // Requesting a non-GC pointer is safe here since we never re-enter C++
    // from inside a type check.

    const types::TemporaryTypeSet *types = ins->typeSet();
    bool needTemp = !types->unknownObject() && types->getObjectCount() > 0;
    LDefinition tmp = needTemp ? temp() : tempToUnbox();

    LMonitorTypes *lir = new(alloc()) LMonitorTypes(tmp);
    if (!useBox(lir, LMonitorTypes::Input, ins->input()))
        return false;
    return assignSnapshot(lir, Bailout_MonitorTypes) && add(lir, ins);
}

bool
LIRGenerator::visitPostWriteBarrier(MPostWriteBarrier *ins)
{
#ifdef JSGC_GENERATIONAL
    switch (ins->value()->type()) {
      case MIRType_Object: {
        LDefinition tmp = needTempForPostBarrier() ? temp() : LDefinition::BogusTemp();
        LPostWriteBarrierO *lir =
            new(alloc()) LPostWriteBarrierO(useRegisterOrConstant(ins->object()),
                                            useRegister(ins->value()), tmp);
        return add(lir, ins) && assignSafepoint(lir, ins);
      }
      case MIRType_Value: {
        LDefinition tmp = needTempForPostBarrier() ? temp() : LDefinition::BogusTemp();
        LPostWriteBarrierV *lir =
            new(alloc()) LPostWriteBarrierV(useRegisterOrConstant(ins->object()), tmp);
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
    return define(new(alloc()) LArrayLength(useRegisterAtStart(ins->elements())), ins);
}

bool
LIRGenerator::visitSetArrayLength(MSetArrayLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    JS_ASSERT(ins->index()->isConstant());
    return add(new(alloc()) LSetArrayLength(useRegister(ins->elements()),
                                            useRegisterOrConstant(ins->index())), ins);
}

bool
LIRGenerator::visitTypedArrayLength(MTypedArrayLength *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    return define(new(alloc()) LTypedArrayLength(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitTypedArrayElements(MTypedArrayElements *ins)
{
    JS_ASSERT(ins->type() == MIRType_Elements);
    return define(new(alloc()) LTypedArrayElements(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitTypedObjectProto(MTypedObjectProto *ins)
{
    JS_ASSERT(ins->type() == MIRType_Object);
    return defineReturn(new(alloc()) LTypedObjectProto(
                            useFixed(ins->object(), CallTempReg0),
                            tempFixed(CallTempReg1)),
                        ins);
}

bool
LIRGenerator::visitTypedObjectElements(MTypedObjectElements *ins)
{
    JS_ASSERT(ins->type() == MIRType_Elements);
    return define(new(alloc()) LTypedObjectElements(useRegisterAtStart(ins->object())), ins);
}

bool
LIRGenerator::visitSetTypedObjectOffset(MSetTypedObjectOffset *ins)
{
    return add(new(alloc()) LSetTypedObjectOffset(
                   useRegister(ins->object()),
                   useRegister(ins->offset()),
                   temp()),
               ins);
}

bool
LIRGenerator::visitInitializedLength(MInitializedLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    return define(new(alloc()) LInitializedLength(useRegisterAtStart(ins->elements())), ins);
}

bool
LIRGenerator::visitSetInitializedLength(MSetInitializedLength *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    JS_ASSERT(ins->index()->isConstant());
    return add(new(alloc()) LSetInitializedLength(useRegister(ins->elements()),
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
        MConstant *cons = MConstant::New(alloc(), Int32Value(1));
        ins->block()->insertBefore(ins, cons);
        return lowerForALU(new(alloc()) LBitOpI(JSOP_BITXOR), ins, op, cons);
      }
      case MIRType_Int32: {
        return define(new(alloc()) LNotI(useRegisterAtStart(op)), ins);
      }
      case MIRType_Double:
        return define(new(alloc()) LNotD(useRegister(op)), ins);
      case MIRType_Float32:
        return define(new(alloc()) LNotF(useRegister(op)), ins);
      case MIRType_Undefined:
      case MIRType_Null:
        return define(new(alloc()) LInteger(1), ins);
      case MIRType_Symbol:
        return define(new(alloc()) LInteger(0), ins);
      case MIRType_Object: {
        // Objects that don't emulate undefined can be constant-folded.
        if (!ins->operandMightEmulateUndefined())
            return define(new(alloc()) LInteger(0), ins);
        // All others require further work.
        return define(new(alloc()) LNotO(useRegister(op)), ins);
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

        LNotV *lir = new(alloc()) LNotV(tempDouble(), temp0, temp1);
        if (!useBox(lir, LNotV::Input, op))
            return false;
        return define(lir, ins);
      }

      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected MIRType.");
    }
}

bool
LIRGenerator::visitNeuterCheck(MNeuterCheck *ins)
{
    LNeuterCheck *chk = new(alloc()) LNeuterCheck(useRegister(ins->object()),
                                                  temp());
    if (!assignSnapshot(chk, Bailout_Neutered))
        return false;
    return redefine(ins, ins->input()) && add(chk, ins);
}

bool
LIRGenerator::visitBoundsCheck(MBoundsCheck *ins)
{
    LInstruction *check;
    if (ins->minimum() || ins->maximum()) {
        check = new(alloc()) LBoundsCheckRange(useRegisterOrConstant(ins->index()),
                                               useAny(ins->length()),
                                               temp());
    } else {
        check = new(alloc()) LBoundsCheck(useRegisterOrConstant(ins->index()),
                                          useAnyOrConstant(ins->length()));
    }
    return assignSnapshot(check, Bailout_BoundsCheck) && add(check, ins);
}

bool
LIRGenerator::visitBoundsCheckLower(MBoundsCheckLower *ins)
{
    if (!ins->fallible())
        return true;

    LInstruction *check = new(alloc()) LBoundsCheckLower(useRegister(ins->index()));
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

    LInArray *lir = new(alloc()) LInArray(useRegister(ins->elements()),
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
        LLoadElementV *lir = new(alloc()) LLoadElementV(useRegister(ins->elements()),
                                                        useRegisterOrConstant(ins->index()));
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Hole))
            return false;
        return defineBox(lir, ins);
      }
      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
      {
        LLoadElementT *lir = new(alloc()) LLoadElementT(useRegister(ins->elements()),
                                                        useRegisterOrConstant(ins->index()));
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Hole))
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

    LLoadElementHole *lir = new(alloc()) LLoadElementHole(useRegister(ins->elements()),
                                                          useRegisterOrConstant(ins->index()),
                                                          useRegister(ins->initLength()));
    if (ins->needsNegativeIntCheck() && !assignSnapshot(lir, Bailout_NegativeIndex))
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
        LInstruction *lir = new(alloc()) LStoreElementV(elements, index);
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Hole))
            return false;
        if (!useBox(lir, LStoreElementV::Value, ins->value()))
            return false;
        return add(lir, ins);
      }

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        LInstruction *lir = new(alloc()) LStoreElementT(elements, index, value);
        if (ins->fallible() && !assignSnapshot(lir, Bailout_Hole))
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
        lir = new(alloc()) LStoreElementHoleV(object, elements, index);
        if (!useBox(lir, LStoreElementHoleV::Value, ins->value()))
            return false;
        break;

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        lir = new(alloc()) LStoreElementHoleT(object, elements, index, value);
        break;
      }
    }

    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitEffectiveAddress(MEffectiveAddress *ins)
{
    return define(new(alloc()) LEffectiveAddress(useRegister(ins->base()), useRegister(ins->index())), ins);
}

bool
LIRGenerator::visitArrayPopShift(MArrayPopShift *ins)
{
    LUse object = useRegister(ins->object());

    switch (ins->type()) {
      case MIRType_Value:
      {
        LArrayPopShiftV *lir = new(alloc()) LArrayPopShiftV(object, temp(), temp());
        return defineBox(lir, ins) && assignSafepoint(lir, ins);
      }
      case MIRType_Undefined:
      case MIRType_Null:
        MOZ_ASSUME_UNREACHABLE("typed load must have a payload");

      default:
      {
        LArrayPopShiftT *lir = new(alloc()) LArrayPopShiftT(object, temp(), temp());
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
        LArrayPushV *lir = new(alloc()) LArrayPushV(object, temp());
        if (!useBox(lir, LArrayPushV::Value, ins->value()))
            return false;
        return define(lir, ins) && assignSafepoint(lir, ins);
      }

      default:
      {
        const LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
        LArrayPushT *lir = new(alloc()) LArrayPushT(object, value, temp());
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

    LArrayConcat *lir = new(alloc()) LArrayConcat(useFixed(ins->lhs(), CallTempReg1),
                                                  useFixed(ins->rhs(), CallTempReg2),
                                                  tempFixed(CallTempReg3),
                                                  tempFixed(CallTempReg4));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStringSplit(MStringSplit *ins)
{
    JS_ASSERT(ins->type() == MIRType_Object);
    JS_ASSERT(ins->string()->type() == MIRType_String);
    JS_ASSERT(ins->separator()->type() == MIRType_String);

    LStringSplit *lir = new(alloc()) LStringSplit(useRegisterAtStart(ins->string()),
                                                  useRegisterAtStart(ins->separator()));
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
    if (ins->arrayType() == ScalarTypeDescr::TYPE_UINT32 && IsFloatingPointType(ins->type()))
        tempDef = temp();

    LLoadTypedArrayElement *lir = new(alloc()) LLoadTypedArrayElement(elements, index, tempDef);
    if (ins->fallible() && !assignSnapshot(lir, Bailout_Overflow))
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
        return defineReuseInput(new(alloc()) LClampIToUint8(useRegisterAtStart(in)), ins, 0);

      case MIRType_Double:
        return define(new(alloc()) LClampDToUint8(useRegisterAtStart(in), tempCopy(in, 0)), ins);

      case MIRType_Value:
      {
        LClampVToUint8 *lir = new(alloc()) LClampVToUint8(tempDouble());
        if (!useBox(lir, LClampVToUint8::Input, in))
            return false;
        return assignSnapshot(lir, Bailout_NonPrimitiveInput)
               && define(lir, ins)
               && assignSafepoint(lir, ins);
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

    LLoadTypedArrayElementHole *lir = new(alloc()) LLoadTypedArrayElementHole(object, index);
    if (ins->fallible() && !assignSnapshot(lir, Bailout_Overflow))
        return false;
    return defineBox(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitLoadTypedArrayElementStatic(MLoadTypedArrayElementStatic *ins)
{
    LLoadTypedArrayElementStatic *lir =
        new(alloc()) LLoadTypedArrayElementStatic(useRegisterAtStart(ins->ptr()));

    // In case of out of bounds, may bail out, or may jump to ool code.
    if (ins->fallible() && !assignSnapshot(lir, Bailout_BoundsCheck))
        return false;
    return define(lir, ins);
}

bool
LIRGenerator::visitStoreTypedArrayElement(MStoreTypedArrayElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    if (ins->isFloatArray()) {
        DebugOnly<bool> optimizeFloat32 = allowFloat32Optimizations();
        JS_ASSERT_IF(optimizeFloat32 && ins->arrayType() == ScalarTypeDescr::TYPE_FLOAT32,
                     ins->value()->type() == MIRType_Float32);
        JS_ASSERT_IF(!optimizeFloat32 || ins->arrayType() == ScalarTypeDescr::TYPE_FLOAT64,
                     ins->value()->type() == MIRType_Double);
    } else {
        JS_ASSERT(ins->value()->type() == MIRType_Int32);
    }

    LUse elements = useRegister(ins->elements());
    LAllocation index = useRegisterOrConstant(ins->index());
    LAllocation value;

    // For byte arrays, the value has to be in a byte register on x86.
    if (ins->isByteArray())
        value = useByteOpRegisterOrNonDoubleConstant(ins->value());
    else
        value = useRegisterOrNonDoubleConstant(ins->value());
    return add(new(alloc()) LStoreTypedArrayElement(elements, index, value), ins);
}

bool
LIRGenerator::visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    JS_ASSERT(ins->length()->type() == MIRType_Int32);

    if (ins->isFloatArray()) {
        DebugOnly<bool> optimizeFloat32 = allowFloat32Optimizations();
        JS_ASSERT_IF(optimizeFloat32 && ins->arrayType() == ScalarTypeDescr::TYPE_FLOAT32,
                     ins->value()->type() == MIRType_Float32);
        JS_ASSERT_IF(!optimizeFloat32 || ins->arrayType() == ScalarTypeDescr::TYPE_FLOAT64,
                     ins->value()->type() == MIRType_Double);
    } else {
        JS_ASSERT(ins->value()->type() == MIRType_Int32);
    }

    LUse elements = useRegister(ins->elements());
    LAllocation length = useAnyOrConstant(ins->length());
    LAllocation index = useRegisterOrConstant(ins->index());
    LAllocation value;

    // For byte arrays, the value has to be in a byte register on x86.
    if (ins->isByteArray())
        value = useByteOpRegisterOrNonDoubleConstant(ins->value());
    else
        value = useRegisterOrNonDoubleConstant(ins->value());
    return add(new(alloc()) LStoreTypedArrayElementHole(elements, length, index, value), ins);
}

bool
LIRGenerator::visitLoadFixedSlot(MLoadFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LLoadFixedSlotV *lir = new(alloc()) LLoadFixedSlotV(useRegister(ins->object()));
        return defineBox(lir, ins);
    }

    LLoadFixedSlotT *lir = new(alloc()) LLoadFixedSlotT(useRegister(ins->object()));
    return define(lir, ins);
}

bool
LIRGenerator::visitStoreFixedSlot(MStoreFixedSlot *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->value()->type() == MIRType_Value) {
        LStoreFixedSlotV *lir = new(alloc()) LStoreFixedSlotV(useRegister(ins->object()));

        if (!useBox(lir, LStoreFixedSlotV::Value, ins->value()))
            return false;
        return add(lir, ins);
    }

    LStoreFixedSlotT *lir = new(alloc()) LStoreFixedSlotT(useRegister(ins->object()),
                                                          useRegisterOrConstant(ins->value()));
    return add(lir, ins);
}

bool
LIRGenerator::visitGetNameCache(MGetNameCache *ins)
{
    JS_ASSERT(ins->scopeObj()->type() == MIRType_Object);

    LGetNameCache *lir = new(alloc()) LGetNameCache(useRegister(ins->scopeObj()));
    if (!defineBox(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetIntrinsicValue(MCallGetIntrinsicValue *ins)
{
    LCallGetIntrinsicValue *lir = new(alloc()) LCallGetIntrinsicValue();
    if (!defineReturn(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallsiteCloneCache(MCallsiteCloneCache *ins)
{
    JS_ASSERT(ins->callee()->type() == MIRType_Object);

    LCallsiteCloneCache *lir = new(alloc()) LCallsiteCloneCache(useRegister(ins->callee()));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetPropertyCache(MGetPropertyCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    if (ins->type() == MIRType_Value) {
        LGetPropertyCacheV *lir = new(alloc()) LGetPropertyCacheV(useRegister(ins->object()));
        if (!defineBox(lir, ins))
            return false;
        return assignSafepoint(lir, ins);
    }

    LGetPropertyCacheT *lir = new(alloc()) LGetPropertyCacheT(useRegister(ins->object()),
                                                              tempForDispatchCache(ins->type()));
    if (!define(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetPropertyPolymorphic(MGetPropertyPolymorphic *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        LGetPropertyPolymorphicV *lir = new(alloc()) LGetPropertyPolymorphicV(useRegister(ins->obj()));
        return assignSnapshot(lir, Bailout_ShapeGuard) && defineBox(lir, ins);
    }

    LDefinition maybeTemp = (ins->type() == MIRType_Double) ? temp() : LDefinition::BogusTemp();
    LGetPropertyPolymorphicT *lir = new(alloc()) LGetPropertyPolymorphicT(useRegister(ins->obj()), maybeTemp);
    return assignSnapshot(lir, Bailout_ShapeGuard) && define(lir, ins);
}

bool
LIRGenerator::visitSetPropertyPolymorphic(MSetPropertyPolymorphic *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    if (ins->value()->type() == MIRType_Value) {
        LSetPropertyPolymorphicV *lir = new(alloc()) LSetPropertyPolymorphicV(useRegister(ins->obj()), temp());
        if (!useBox(lir, LSetPropertyPolymorphicV::Value, ins->value()))
            return false;
        return assignSnapshot(lir, Bailout_ShapeGuard) && add(lir, ins);
    }

    LAllocation value = useRegisterOrConstant(ins->value());
    LSetPropertyPolymorphicT *lir =
        new(alloc()) LSetPropertyPolymorphicT(useRegister(ins->obj()), value, ins->value()->type(), temp());
    return assignSnapshot(lir, Bailout_ShapeGuard) && add(lir, ins);
}

bool
LIRGenerator::visitGetElementCache(MGetElementCache *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);

    if (ins->type() == MIRType_Value) {
        JS_ASSERT(ins->index()->type() == MIRType_Value);
        LGetElementCacheV *lir = new(alloc()) LGetElementCacheV(useRegister(ins->object()));
        if (!useBox(lir, LGetElementCacheV::Index, ins->index()))
            return false;
        return defineBox(lir, ins) && assignSafepoint(lir, ins);
    }

    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    LGetElementCacheT *lir = new(alloc()) LGetElementCacheT(useRegister(ins->object()),
                                                            useRegister(ins->index()),
                                                            tempForDispatchCache(ins->type()));
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitBindNameCache(MBindNameCache *ins)
{
    JS_ASSERT(ins->scopeChain()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Object);

    LBindNameCache *lir = new(alloc()) LBindNameCache(useRegister(ins->scopeChain()));
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGuardObjectIdentity(MGuardObjectIdentity *ins)
{
    LGuardObjectIdentity *guard = new(alloc()) LGuardObjectIdentity(useRegister(ins->obj()));
    return assignSnapshot(guard, Bailout_ObjectIdentityOrTypeGuard)
           && add(guard, ins)
           && redefine(ins, ins->obj());
}

bool
LIRGenerator::visitGuardClass(MGuardClass *ins)
{
    LDefinition t = temp();
    LGuardClass *guard = new(alloc()) LGuardClass(useRegister(ins->obj()), t);
    return assignSnapshot(guard, Bailout_ObjectIdentityOrTypeGuard) && add(guard, ins);
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
LIRGenerator::visitGuardShapePolymorphic(MGuardShapePolymorphic *ins)
{
    MOZ_ASSERT(ins->obj()->type() == MIRType_Object);
    MOZ_ASSERT(ins->type() == MIRType_Object);

    LGuardShapePolymorphic *guard =
        new(alloc()) LGuardShapePolymorphic(useRegister(ins->obj()), temp());
    if (!assignSnapshot(guard, Bailout_ShapeGuard))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGenerator::visitAssertRange(MAssertRange *ins)
{
    MDefinition *input = ins->input();
    LInstruction *lir = nullptr;

    switch (input->type()) {
      case MIRType_Boolean:
      case MIRType_Int32:
        lir = new(alloc()) LAssertRangeI(useRegisterAtStart(input));
        break;

      case MIRType_Double:
        lir = new(alloc()) LAssertRangeD(useRegister(input), tempDouble());
        break;

      case MIRType_Float32: {
        LDefinition armtemp = hasMultiAlias() ? tempFloat32() : LDefinition::BogusTemp();
        lir = new(alloc()) LAssertRangeF(useRegister(input), tempFloat32(), armtemp);
        break;
      }
      case MIRType_Value:
        lir = new(alloc()) LAssertRangeV(tempToUnbox(), tempDouble(), tempDouble());
        if (!useBox(lir, LAssertRangeV::Input, input))
            return false;
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected Range for MIRType");
        break;
    }

    lir->setMir(ins);
    return add(lir);
}

bool
LIRGenerator::visitCallGetProperty(MCallGetProperty *ins)
{
    LCallGetProperty *lir = new(alloc()) LCallGetProperty();
    if (!useBoxAtStart(lir, LCallGetProperty::Value, ins->value()))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallGetElement(MCallGetElement *ins)
{
    JS_ASSERT(ins->lhs()->type() == MIRType_Value);
    JS_ASSERT(ins->rhs()->type() == MIRType_Value);

    LCallGetElement *lir = new(alloc()) LCallGetElement();
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
    LInstruction *lir = new(alloc()) LCallSetProperty(useRegisterAtStart(ins->object()));
    if (!useBoxAtStart(lir, LCallSetProperty::Value, ins->value()))
        return false;
    if (!add(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitDeleteProperty(MDeleteProperty *ins)
{
    LCallDeleteProperty *lir = new(alloc()) LCallDeleteProperty();
    if(!useBoxAtStart(lir, LCallDeleteProperty::Value, ins->value()))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitDeleteElement(MDeleteElement *ins)
{
    LCallDeleteElement *lir = new(alloc()) LCallDeleteElement();
    if(!useBoxAtStart(lir, LCallDeleteElement::Value, ins->value()))
        return false;
    if(!useBoxAtStart(lir, LCallDeleteElement::Index, ins->index()))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitSetPropertyCache(MSetPropertyCache *ins)
{
    LUse obj = useRegisterAtStart(ins->object());
    LDefinition slots = tempCopy(ins->object(), 0);
    LDefinition dispatchTemp = tempForDispatchCache();

    LInstruction *lir;
    if (ins->value()->type() == MIRType_Value) {
        lir = new(alloc()) LSetPropertyCacheV(obj, slots, dispatchTemp);
        if (!useBox(lir, LSetPropertyCacheV::Value, ins->value()))
            return false;
    } else {
        LAllocation value = useRegisterOrConstant(ins->value());
        lir = new(alloc()) LSetPropertyCacheT(obj, slots, value, dispatchTemp, ins->value()->type());
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

    // Due to lack of registers on x86, we reuse the object register as a
    // temporary. This register may be used in a 1-byte store, which on x86
    // again has constraints; thus the use of |useByteOpRegister| over
    // |useRegister| below.
    LInstruction *lir;
    if (ins->value()->type() == MIRType_Value) {
        LDefinition tempF32 = hasUnaliasedDouble() ? tempFloat32() : LDefinition::BogusTemp();
        lir = new(alloc()) LSetElementCacheV(useByteOpRegister(ins->object()), tempToUnbox(),
                                             temp(), tempDouble(), tempF32);

        if (!useBox(lir, LSetElementCacheV::Index, ins->index()))
            return false;
        if (!useBox(lir, LSetElementCacheV::Value, ins->value()))
            return false;
    } else {
        LDefinition tempF32 = hasUnaliasedDouble() ? tempFloat32() : LDefinition::BogusTemp();
        lir = new(alloc()) LSetElementCacheT(useByteOpRegister(ins->object()),
                                             useRegisterOrConstant(ins->value()),
                                             tempToUnbox(), temp(), tempDouble(),
                                             tempF32);

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

    LCallSetElement *lir = new(alloc()) LCallSetElement();
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
    LCallInitElementArray *lir = new(alloc()) LCallInitElementArray();
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
        LCallIteratorStart *lir = new(alloc()) LCallIteratorStart(useRegisterAtStart(ins->object()));
        return defineReturn(lir, ins) && assignSafepoint(lir, ins);
    }

    LIteratorStart *lir = new(alloc()) LIteratorStart(useRegister(ins->object()), temp(), temp(), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorNext(MIteratorNext *ins)
{
    LIteratorNext *lir = new(alloc()) LIteratorNext(useRegister(ins->iterator()), temp());
    return defineBox(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorMore(MIteratorMore *ins)
{
    LIteratorMore *lir = new(alloc()) LIteratorMore(useRegister(ins->iterator()), temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIteratorEnd(MIteratorEnd *ins)
{
    LIteratorEnd *lir = new(alloc()) LIteratorEnd(useRegister(ins->iterator()), temp(), temp(), temp());
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitStringLength(MStringLength *ins)
{
    JS_ASSERT(ins->string()->type() == MIRType_String);
    return define(new(alloc()) LStringLength(useRegisterAtStart(ins->string())), ins);
}

bool
LIRGenerator::visitArgumentsLength(MArgumentsLength *ins)
{
    return define(new(alloc()) LArgumentsLength(), ins);
}

bool
LIRGenerator::visitGetFrameArgument(MGetFrameArgument *ins)
{
    LGetFrameArgument *lir = new(alloc()) LGetFrameArgument(useRegisterOrConstant(ins->index()));
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitSetFrameArgument(MSetFrameArgument *ins)
{
    MDefinition *input = ins->input();

    if (input->type() == MIRType_Value) {
        LSetFrameArgumentV *lir = new(alloc()) LSetFrameArgumentV();
        if (!useBox(lir, LSetFrameArgumentV::Input, input))
            return false;
        return add(lir, ins);
    }

    if (input->type() == MIRType_Undefined || input->type() == MIRType_Null) {
        Value val = input->type() == MIRType_Undefined ? UndefinedValue() : NullValue();
        LSetFrameArgumentC *lir = new(alloc()) LSetFrameArgumentC(val);
        return add(lir, ins);
    }

    LSetFrameArgumentT *lir = new(alloc()) LSetFrameArgumentT(useRegister(input));
    return add(lir, ins);
}

bool
LIRGenerator::visitRunOncePrologue(MRunOncePrologue *ins)
{
    LRunOncePrologue *lir = new(alloc()) LRunOncePrologue;
    return add(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRest(MRest *ins)
{
    JS_ASSERT(ins->numActuals()->type() == MIRType_Int32);

    LRest *lir = new(alloc()) LRest(useFixed(ins->numActuals(), CallTempReg0),
                                    tempFixed(CallTempReg1),
                                    tempFixed(CallTempReg2),
                                    tempFixed(CallTempReg3));
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitRestPar(MRestPar *ins)
{
    JS_ASSERT(ins->numActuals()->type() == MIRType_Int32);

    LRestPar *lir = new(alloc()) LRestPar(useRegister(ins->forkJoinContext()),
                                          useRegister(ins->numActuals()),
                                          temp(),
                                          temp(),
                                          temp());
    return define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitThrow(MThrow *ins)
{
    MDefinition *value = ins->getOperand(0);
    JS_ASSERT(value->type() == MIRType_Value);

    LThrow *lir = new(alloc()) LThrow;
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

    LIn *lir = new(alloc()) LIn(useRegisterAtStart(rhs));
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
        LInstanceOfO *lir = new(alloc()) LInstanceOfO(useRegister(lhs));
        return define(lir, ins) && assignSafepoint(lir, ins);
    }

    LInstanceOfV *lir = new(alloc()) LInstanceOfV();
    return useBox(lir, LInstanceOfV::LHS, lhs) && define(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitCallInstanceOf(MCallInstanceOf *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_Value);
    JS_ASSERT(rhs->type() == MIRType_Object);

    LCallInstanceOf *lir = new(alloc()) LCallInstanceOf(useRegisterAtStart(rhs));
    if (!useBoxAtStart(lir, LCallInstanceOf::LHS, lhs))
        return false;
    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitProfilerStackOp(MProfilerStackOp *ins)
{
    LProfilerStackOp *lir = new(alloc()) LProfilerStackOp(temp());
    if (!add(lir, ins))
        return false;
    // If slow assertions are enabled, then this node will result in a callVM
    // out to a C++ function for the assertions, so we will need a safepoint.
    return !gen->options.spsSlowAssertionsEnabled() || assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitIsCallable(MIsCallable *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Boolean);
    return define(new(alloc()) LIsCallable(useRegister(ins->object())), ins);
}

bool
LIRGenerator::visitHaveSameClass(MHaveSameClass *ins)
{
    MDefinition *lhs = ins->lhs();
    MDefinition *rhs = ins->rhs();

    JS_ASSERT(lhs->type() == MIRType_Object);
    JS_ASSERT(rhs->type() == MIRType_Object);

    return define(new(alloc()) LHaveSameClass(useRegister(lhs), useRegister(rhs), temp()), ins);
}

bool
LIRGenerator::visitHasClass(MHasClass *ins)
{
    JS_ASSERT(ins->object()->type() == MIRType_Object);
    JS_ASSERT(ins->type() == MIRType_Boolean);
    return define(new(alloc()) LHasClass(useRegister(ins->object())), ins);
}

bool
LIRGenerator::visitAsmJSLoadGlobalVar(MAsmJSLoadGlobalVar *ins)
{
    return define(new(alloc()) LAsmJSLoadGlobalVar, ins);
}

bool
LIRGenerator::visitAsmJSStoreGlobalVar(MAsmJSStoreGlobalVar *ins)
{
    return add(new(alloc()) LAsmJSStoreGlobalVar(useRegisterAtStart(ins->value())), ins);
}

bool
LIRGenerator::visitAsmJSLoadFFIFunc(MAsmJSLoadFFIFunc *ins)
{
    return define(new(alloc()) LAsmJSLoadFFIFunc, ins);
}

bool
LIRGenerator::visitAsmJSParameter(MAsmJSParameter *ins)
{
    ABIArg abi = ins->abi();
    if (abi.argInRegister())
        return defineFixed(new(alloc()) LAsmJSParameter, ins, LAllocation(abi.reg()));

    JS_ASSERT(IsNumberType(ins->type()));
    return defineFixed(new(alloc()) LAsmJSParameter, ins, LArgument(abi.offsetFromArgBase()));
}

bool
LIRGenerator::visitAsmJSReturn(MAsmJSReturn *ins)
{
    MDefinition *rval = ins->getOperand(0);
    LAsmJSReturn *lir = new(alloc()) LAsmJSReturn;
    if (rval->type() == MIRType_Float32)
        lir->setOperand(0, useFixed(rval, ReturnFloat32Reg));
    else if (rval->type() == MIRType_Double)
        lir->setOperand(0, useFixed(rval, ReturnDoubleReg));
    else if (rval->type() == MIRType_Int32)
        lir->setOperand(0, useFixed(rval, ReturnReg));
    else
        MOZ_ASSUME_UNREACHABLE("Unexpected asm.js return type");
    return add(lir);
}

bool
LIRGenerator::visitAsmJSVoidReturn(MAsmJSVoidReturn *ins)
{
    return add(new(alloc()) LAsmJSVoidReturn);
}

bool
LIRGenerator::visitAsmJSPassStackArg(MAsmJSPassStackArg *ins)
{
    if (IsFloatingPointType(ins->arg()->type())) {
        JS_ASSERT(!ins->arg()->isEmittedAtUses());
        return add(new(alloc()) LAsmJSPassStackArg(useRegisterAtStart(ins->arg())), ins);
    }

    return add(new(alloc()) LAsmJSPassStackArg(useRegisterOrConstantAtStart(ins->arg())), ins);
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

    LInstruction *lir = new(alloc()) LAsmJSCall(args, ins->numOperands());
    if (ins->type() == MIRType_None) {
        return add(lir, ins);
    }
    return defineReturn(lir, ins);
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
    LSetDOMProperty *lir = new(alloc()) LSetDOMProperty(tempFixed(cxReg),
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
    LGetDOMProperty *lir = new(alloc()) LGetDOMProperty(tempFixed(cxReg),
                                                        useFixed(ins->object(), objReg),
                                                        tempFixed(privReg),
                                                        tempFixed(valueReg));

    return defineReturn(lir, ins) && assignSafepoint(lir, ins);
}

bool
LIRGenerator::visitGetDOMMember(MGetDOMMember *ins)
{
    MOZ_ASSERT(ins->isDomMovable(), "Members had better be movable");
    // We wish we could assert that ins->domAliasSet() == JSJitInfo::AliasNone,
    // but some MGetDOMMembers are for [Pure], not [Constant] properties, whose
    // value can in fact change as a result of DOM setters and method calls.
    MOZ_ASSERT(ins->domAliasSet() != JSJitInfo::AliasEverything,
               "Member gets had better not alias the world");
    LGetDOMMember *lir =
        new(alloc()) LGetDOMMember(useRegister(ins->object()));
    return defineBox(lir, ins);
}

bool
LIRGenerator::visitRecompileCheck(MRecompileCheck *ins)
{
    LRecompileCheck *lir = new(alloc()) LRecompileCheck(temp());
    if (!add(lir, ins))
        return false;
    return assignSafepoint(lir, ins);
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
            int(resumePoint->block()->info().script()->pcToOffset(resumePoint->pc())));

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
    if (ins->isRecoveredOnBailout())
        return true;

    if (!gen->ensureBallast())
        return false;
    if (!ins->accept(this))
        return false;

    if (ins->possiblyCalls())
        gen->setPerformsCall();

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
        SpewResumePoint(nullptr, ins, lastResumePoint_);
}

void
LIRGenerator::updateResumeState(MBasicBlock *block)
{
    lastResumePoint_ = block->entryResumePoint();
    if (IonSpewEnabled(IonSpew_Snapshots) && lastResumePoint_)
        SpewResumePoint(block, nullptr, lastResumePoint_);
}

bool
LIRGenerator::visitBlock(MBasicBlock *block)
{
    current = block->lir();
    updateResumeState(block);

    if (!definePhis())
        return false;

    if (gen->optimizationInfo().registerAllocator() == RegisterAllocator_LSRA) {
        if (!add(new(alloc()) LLabel()))
            return false;
    }

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
LIRGenerator::generate()
{
    // Create all blocks and prep all phis beforehand.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (gen->shouldCancel("Lowering (preparation loop)"))
            return false;

        current = LBlock::New(alloc(), *block);
        if (!current)
            return false;
        lirGraph_.setBlock(block->id(), current);
        block->assignLir(current);
    }

    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (gen->shouldCancel("Lowering (main loop)"))
            return false;

        if (!visitBlock(*block))
            return false;
    }

    lirGraph_.setArgumentSlotCount(maxargslots_);
    return true;
}
