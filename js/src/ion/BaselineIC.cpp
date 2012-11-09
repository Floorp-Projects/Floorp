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

IonCode *
ICCompare_Fallback::Compiler::generateStubCode()
{
    MacroAssembler masm;
    JS_ASSERT(R0 == JSReturnOperand);

    // Pop return address.
    masm.pop(BaselineTailCallReg);

    // Get VMFunction to call
    typedef bool (*pf)(JSContext *, ICCompare_Fallback *, HandleValue, HandleValue,
                       MutableHandleValue);
    static const VMFunction fun = FunctionInfo<pf>(DoCompareFallback);

    IonCode *wrapper = generateVMWrapper(fun);
    if (!wrapper)
        return NULL;

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    // Call.
    EmitTailCall(wrapper, masm);

    Linker linker(masm);
    return linker.newCode(cx);
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

IonCode *
ICToBool_Fallback::Compiler::generateStubCode()
{
    MacroAssembler masm;
    JS_ASSERT(R0 == JSReturnOperand);

    // Pop return address.
    masm.pop(BaselineTailCallReg);

    // Get VMFunction to call
    typedef bool (*pf)(JSContext *, ICToBool_Fallback *, HandleValue, MutableHandleValue);
    static const VMFunction fun = FunctionInfo<pf>(DoToBoolFallback);

    IonCode *wrapper = generateVMWrapper(fun);
    if (!wrapper)
        return NULL;

    // Push arguments.
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    // Call.
    EmitTailCall(wrapper, masm);

    Linker linker(masm);
    return linker.newCode(cx);
}

//
// ToBool_Bool
//

IonCode *
ICToBool_Bool::Compiler::generateStubCode()
{
    MacroAssembler masm;

    // Just guard that R0 is a boolean and leave it be if so.
    Label failure;
    masm.branchTestBoolean(Assembler::NotEqual, R0, &failure);
    masm.ret();

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    Linker linker(masm);
    return linker.newCode(cx);
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

IonCode *
ICBinaryArith_Fallback::Compiler::generateStubCode()
{
    MacroAssembler masm;
    JS_ASSERT(R0 == JSReturnOperand);

    // Pop return address.
    masm.pop(BaselineTailCallReg);

    // Get VMFunction to call
    typedef bool (*pf)(JSContext *, ICBinaryArith_Fallback *, HandleValue, HandleValue,
                       MutableHandleValue);
    static const VMFunction fun = FunctionInfo<pf>(DoBinaryArithFallback);

    IonCode *wrapper = generateVMWrapper(fun);
    if (!wrapper)
        return NULL;

    // Push arguments.
    masm.pushValue(R1);
    masm.pushValue(R0);
    masm.push(BaselineStubReg);

    // Call.
    EmitTailCall(wrapper, masm);

    Linker linker(masm);
    return linker.newCode(cx);
}

} // namespace ion
} // namespace js
