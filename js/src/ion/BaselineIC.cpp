/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineJIT.h"
#include "BaselineCompiler.h"
#include "BaselineHelpers.h"
#include "BaselineIC.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "VMFunctions.h"
#include "IonFrames-inl.h"

namespace js {
namespace ion {

IonCode *
ICStubCompiler::getStubCode()
{
    IonCompartment *ion = cx->compartment->ionCompartment();

    // Check for existing cached stubcode.
    uint32_t stubKey = getKey();
    IonCode *stubCode = ion->getStubCode(stubKey);
    if (stubCode)
        return stubCode;

    // Compile new stubcode.
    MacroAssembler masm;
    AutoFlushCache afc("ICStubCompiler::getStubCode", ion);
    if (!generateStubCode(masm))
        return NULL;
    Linker linker(masm);
    Rooted<IonCode *> newStubCode(cx, linker.newCode(cx));
    if (!newStubCode)
        return NULL;

    // Cache newly compiled stubcode.
    if (!ion->putStubCode(stubKey, newStubCode))
        return NULL;

    return newStubCode;
}

bool
ICStubCompiler::callVM(const VMFunction &fun, MacroAssembler &masm)
{
    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *code = ion->getVMWrapper(fun);
    if (!code)
        return false;

    uint32_t argSize = fun.explicitStackSlots() * sizeof(void *);
    EmitTailCall(code, masm, argSize);
    return true;
}

//
// Compare_Fallback
//

static bool
DoCompareFallback(JSContext *cx, ICCompare_Fallback *stub, HandleValue lhs, HandleValue rhs,
                  MutableHandleValue ret)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    // Perform the compare operation.
    JSOp op = JSOp(*stub->icEntry()->pc(script));
    switch(op) {
      case JSOP_LT: {
        // Do the less than.
        JSBool out;
        if (!LessThan(cx, lhs, rhs, &out))
            return false;
        ret.setBoolean(out);
        break;
      }
      case JSOP_GT: {
        // Do the less than.
        JSBool out;
        if (!GreaterThan(cx, lhs, rhs, &out))
            return false;
        ret.setBoolean(out);
        break;
      }
      default:
        JS_ASSERT(!"Unhandled baseline compare op");
        return false;
    }


    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICCompare_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (lhs.isInt32()) {
        if (rhs.isInt32()) {
            ICCompare_Int32::Compiler compilerInt32(cx, op);
            ICStub *int32Stub = compilerInt32.getStub();
            if (!int32Stub)
                return false;

            stub->addNewStub(int32Stub);
        }
    }

    return true;
}

typedef bool (*DoCompareFallbackFn)(JSContext *, ICCompare_Fallback *, HandleValue, HandleValue,
                                    MutableHandleValue);
static const VMFunction DoCompareFallbackInfo = FunctionInfo<DoCompareFallbackFn>(DoCompareFallback);

bool
ICCompare_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return callVM(DoCompareFallbackInfo, masm);
}

//
// ToBool_Fallback
//

static bool
DoToBoolFallback(JSContext *cx, ICToBool_Fallback *stub, HandleValue arg, MutableHandleValue ret)
{
    bool cond = ToBoolean(arg);
    ret.setBoolean(cond);

    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICToBool_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (arg.isBoolean()) {
        // Attach the new bool-specialized stub.
        ICToBool_Bool::Compiler compilerBool(cx);
        ICStub *boolStub = compilerBool.getStub();
        if (!boolStub)
            return false;

        stub->addNewStub(boolStub);
    }

    return true;
}

typedef bool (*pf)(JSContext *, ICToBool_Fallback *, HandleValue, MutableHandleValue);
static const VMFunction fun = FunctionInfo<pf>(DoToBoolFallback);

bool
ICToBool_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Push arguments.
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    // Call.
    if (!callVM(fun, masm))
        return false;

    return true;
}

//
// ToBool_Bool
//

bool
ICToBool_Bool::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Just guard that R0 is a boolean and leave it be if so.
    Label failure;
    masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

//
// ToNumber_Fallback
//

static bool
DoToNumberFallback(JSContext *cx, ICToNumber_Fallback *stub, HandleValue arg, MutableHandleValue ret)
{
    ret.set(arg);
    return ToNumber(cx, ret.address());
}

typedef bool (*DoToNumberFallbackFn)(JSContext *, ICToNumber_Fallback *, HandleValue, MutableHandleValue);
static const VMFunction DoToNumberFallbackInfo = FunctionInfo<DoToNumberFallbackFn>(DoToNumberFallback);

bool
ICToNumber_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return callVM(DoToNumberFallbackInfo, masm);
}

//
// BinaryArith_Fallback
//

static bool
DoBinaryArithFallback(JSContext *cx, ICBinaryArith_Fallback *stub, HandleValue lhs,
                      HandleValue rhs, MutableHandleValue ret)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    // Perform the compare operation.
    JSOp op = JSOp(*stub->icEntry()->pc(script));
    switch(op) {
      case JSOP_ADD: {
        // Do an add.
        if (!AddValues(cx, script, stub->icEntry()->pc(script), lhs, rhs, ret.address()))
            return false;
        break;
      }
      default:
        JS_ASSERT(!"Unhandled baseline compare op");
        return false;
    }

    // Check to see if a new stub should be generated.
    if (stub->numOptimizedStubs() >= ICBinaryArith_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (lhs.isInt32()) {
        if (rhs.isInt32()) {
            ICBinaryArith_Int32::Compiler compilerInt32(cx, op);
            ICStub *int32Stub = compilerInt32.getStub();
            if (!int32Stub)
                return false;

            stub->addNewStub(int32Stub);
        }
    }

    return true;
}

typedef bool (*DoBinaryArithFallbackFn)(JSContext *, ICBinaryArith_Fallback *, HandleValue, HandleValue,
                                        MutableHandleValue);
static const VMFunction DoBinaryArithFallbackInfo =
    FunctionInfo<DoBinaryArithFallbackFn>(DoBinaryArithFallback);

bool
ICBinaryArith_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return callVM(DoBinaryArithFallbackInfo, masm);
}

static bool
DoGetElemFallback(JSContext *cx, ICGetElem_Fallback *stub, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    if (!GetElementMonitored(cx, lhs, rhs, res))
        return false;

    if (stub->numOptimizedStubs() >= ICGetElem_Fallback::MAX_OPTIMIZED_STUBS) {
        // TODO: Discard all stubs in this IC and replace with inert megamorphic stub.
        // But for now we just bail.
        return true;
    }

    // Try to generate new stubs.
    if (!lhs.isObject())
        return true;

    RootedObject obj(cx, &lhs.toObject());
    if (obj->isDenseArray() && rhs.isInt32()) {
        // Don't attach multiple dense array stubs.
        if (stub->hasStub(ICStub::GetElem_Dense))
            return true;

        ICGetElem_Dense::Compiler compiler(cx);
        ICStub *denseStub = compiler.getStub();
        if (!denseStub)
            return false;

        stub->addNewStub(denseStub);
    }

    return true;
}

typedef bool (*DoGetElemFallbackFn)(JSContext *, ICGetElem_Fallback *, HandleValue, HandleValue,
                                    MutableHandleValue);
static const VMFunction DoGetElemFallbackInfo = FunctionInfo<DoGetElemFallbackFn>(DoGetElemFallback);

bool
ICGetElem_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    return callVM(DoGetElemFallbackInfo, masm);
}

bool
ICGetElem_Dense::Compiler::generateStubCode(MacroAssembler &masm)
{
    Label failure;
    masm.branchTestObject(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    RootedShape shape(cx, GetDenseArrayShape(cx, cx->global()));
    if (!shape)
        return false;

    // Unbox R0 and guard it's a dense array.
    Register obj = masm.extractObject(R0, ExtractTemp0);
    masm.branchTestObjShape(Assembler::NotEqual, obj, shape, &failure);

    // Load obj->elements in scratchReg.
    GeneralRegisterSet regs(availableGeneralRegs(2));
    Register scratchReg = regs.takeAny();
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), scratchReg);

    // Unbox key.
    Register key = masm.extractInt32(R1, ExtractTemp1);

    // Bounds check.
    Address initLength(scratchReg, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::BelowOrEqual, initLength, key, &failure);

    // Hole check and load value.
    BaseIndex element(scratchReg, key, TimesEight);
    masm.branchTestMagic(Assembler::Equal, element, &failure);
    masm.loadValue(element, R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);
    return true;
}

static bool
DoSetElemFallback(JSContext *cx, ICSetElem_Fallback *stub, HandleValue rhs, HandleValue objv,
                  HandleValue index)
{
    RootedObject obj(cx, ToObject(cx, objv));
    if (!obj)
        return false;

    RootedScript script(cx, GetTopIonJSScript(cx));
    if (!SetObjectElement(cx, obj, index, rhs, script->strictModeCode))
        return false;

    return true;
}

typedef bool (*DoSetElemFallbackFn)(JSContext *, ICSetElem_Fallback *, HandleValue, HandleValue,
                                    HandleValue);
static const VMFunction DoSetElemFallbackInfo = FunctionInfo<DoSetElemFallbackFn>(DoSetElemFallback);

bool
ICSetElem_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    EmitRestoreTailCallReg(masm);

    // Push key and object.
    masm.pushValue(R1);
    masm.pushValue(R0);

    // Push RHS. On x86 and ARM two push instructions are emitted so use a
    // separate register to store the old stack pointer.
    masm.mov(BaselineStackReg, R0.scratchReg());
    masm.pushValue(Address(R0.scratchReg(), 2 * sizeof(Value)));

    masm.push(BaselineStubReg);

    return callVM(DoSetElemFallbackInfo, masm);
}

//
// Call_Fallback
//

static bool
DoCallFallback(JSContext *cx, ICCall_Fallback *stub, uint32_t argc, Value *vp, MutableHandleValue res)
{
    RootedValue callee(cx, vp[0]);
    RootedValue thisv(cx, vp[1]);

    Value *args = vp + 2;

    RootedScript script(cx, GetTopIonJSScript(cx));
    bool ok = false;

    JSOp op = JSOp(*stub->icEntry()->pc(script));
    switch (op) {
      case JSOP_CALL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
        // Run the function in the interpreter.
        ok = Invoke(cx, thisv, callee, argc, args, res.address());
        break;
      case JSOP_NEW:
        ok = InvokeConstructor(cx, callee, argc, args, res.address());
        break;
      default:
        JS_NOT_REACHED("Invalid call op");
    }

    if (ok)
        types::TypeScript::Monitor(cx, res);

    return ok;
}

void
ICCallStubCompiler::pushCallArguments(MacroAssembler &masm, Register argcReg)
{
    GeneralRegisterSet regs(availableGeneralRegs(0));
    regs.take(BaselineTailCallReg);
    regs.take(argcReg);

    // Push the callee and |this| too.
    Register count = regs.takeAny();
    masm.mov(argcReg, count);
    masm.add32(Imm32(2), count);

    // argPtr initially points to the last argument.
    Register argPtr = regs.takeAny();
    masm.mov(BaselineStackReg, argPtr);

    // Push all values, starting at the last one.
    Label loop, done;
    masm.bind(&loop);
    masm.branchTest32(Assembler::Zero, count, count, &done);
    {
        masm.pushValue(Address(argPtr, 0));
        masm.addPtr(Imm32(sizeof(Value)), argPtr);

        masm.sub32(Imm32(1), count);
        masm.jump(&loop);
    }
    masm.bind(&done);
}

typedef bool (*DoCallFallbackFn)(JSContext *, ICCall_Fallback *, uint32_t, Value *, MutableHandleValue);
static const VMFunction DoCallFallbackInfo = FunctionInfo<DoCallFallbackFn>(DoCallFallback);

bool
ICCall_Fallback::Compiler::generateStubCode(MacroAssembler &masm)
{
    JS_ASSERT(R0 == JSReturnOperand);

    // Restore the tail call register.
    EmitRestoreTailCallReg(masm);

    // Values are on the stack left-to-right. Calling convention wants them
    // right-to-left so duplicate them on the stack in reverse order.
    // |this| and callee are pushed last.

    // R0.scratchReg() holds argc.
    pushCallArguments(masm, R0.scratchReg());

    masm.push(BaselineStackReg);
    masm.push(R0.scratchReg());
    masm.push(BaselineStubReg);

    return callVM(DoCallFallbackInfo, masm);
}

} // namespace ion
} // namespace js
