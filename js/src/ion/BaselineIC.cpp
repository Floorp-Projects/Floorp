/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineJIT.h"
#include "BaselineCompiler.h"
#include "BaselineIC.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "VMFunctions.h"
#include "IonFrames-inl.h"

using namespace js;
using namespace js::ion;

bool
UpdateBinaryOpCache(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    BinaryOpCache cache(script->baseline->cacheDataFromReturnAddr(returnAddr));

    JS_ASSERT(JSOp(*cache.data.pc) == JSOP_ADD);

    if (!AddValues(cx, script, cache.data.pc, lhs, rhs, res.address()))
        return false;

    return cache.update(cx, lhs, rhs, res);
}

BinaryOpCache::State
BinaryOpCache::getTargetState(HandleValue lhs, HandleValue rhs, HandleValue res)
{
    DebugOnly<State> state = getState();
    if (lhs.isInt32() && rhs.isInt32() && res.isInt32()) {
        JS_ASSERT(state != Int32);
        return Int32;
    }

    JS_NOT_REACHED("Unexpected target state");
    return Uninitialized;
}

bool
BinaryOpCache::update(JSContext *cx, HandleValue lhs, HandleValue rhs, HandleValue res)
{
    JS_ASSERT(getState() == Uninitialized); //XXX

    State target = getTargetState(lhs, rhs, res);
    JS_ASSERT(getState() != target);

    setState(target);
    IonCode *stub = generate(cx);
    if (!stub)
        return false;

    PatchCall(data.call, CodeLocationLabel(stub));
    return true;
}

IonCode *
BinaryOpCache::generate(JSContext *cx)
{
    MacroAssembler masm;

    switch (getState()) {
      case Uninitialized:
        if (!generateUpdate(cx, masm))
            return NULL;
        break;
      case Int32:
        if (!generateInt32(cx, masm))
            return NULL;
        break;
      default:
        JS_NOT_REACHED("foo");
    }

    Linker linker(masm);
    return linker.newCode(cx);
}

bool
BinaryOpCache::generateUpdate(JSContext *cx, MacroAssembler &masm)
{
    // Pop return address.
    masm.pop(esi);
    masm.pushValue(R1);
    masm.pushValue(R0);

    typedef bool (*pf)(JSContext *, HandleValue, HandleValue, MutableHandleValue);
    static const VMFunction fun = FunctionInfo<pf>(UpdateBinaryOpCache);

    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *wrapper = ion->generateVMWrapper(cx, fun);
    if (!wrapper)
        return false;

    masm.tailCallWithExitFrameFromBaseline(wrapper);
    masm.breakpoint();
    return true;
}

bool
BinaryOpCache::generateInt32(JSContext *cx, MacroAssembler &masm)
{
    Label notInt32, overflow;
    masm.branchTestInt32(Assembler::NotEqual, R0, &notInt32);
    masm.branchTestInt32(Assembler::NotEqual, R1, &notInt32);

    masm.addl(R1.payloadReg(), R0.payloadReg());
    masm.j(Assembler::Overflow, &overflow);

    masm.ret();

    // Overflow.
    masm.bind(&overflow);
    //XXX: restore R0.

    // Update cache state.
    masm.bind(&notInt32);
    generateUpdate(cx, masm);
    return true;
}

bool
UpdateCompareCache(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue res)
{
    uint8_t *returnAddr;
    RootedScript script(cx, GetTopIonJSScript(cx, NULL, (void **)&returnAddr));

    CompareCache cache(script->baseline->cacheDataFromReturnAddr(returnAddr));

    JS_ASSERT(JSOp(*cache.data.pc) == JSOP_LT);

    JSBool b;
    if (!LessThan(cx, lhs, rhs, &b))
        return false;

    res.setBoolean(b);
    return cache.update(cx, lhs, rhs);
}

CompareCache::State
CompareCache::getTargetState(HandleValue lhs, HandleValue rhs)
{
    DebugOnly<State> state = getState();
    if (lhs.isInt32() && rhs.isInt32()) {
        JS_ASSERT(state != Int32);
        return Int32;
    }

    JS_NOT_REACHED("Unexpected target state");
    return Uninitialized;
}

bool
CompareCache::update(JSContext *cx, HandleValue lhs, HandleValue rhs)
{
    JS_ASSERT(getState() == Uninitialized); //XXX

    State target = getTargetState(lhs, rhs);
    JS_ASSERT(getState() != target);

    setState(target);
    IonCode *stub = generate(cx);
    if (!stub)
        return false;

    PatchCall(data.call, CodeLocationLabel(stub));
    return true;
}

IonCode *
CompareCache::generate(JSContext *cx)
{
    MacroAssembler masm;

    switch (getState()) {
      case Uninitialized:
        if (!generateUpdate(cx, masm))
            return NULL;
        break;
      case Int32:
        if (!generateInt32(cx, masm))
            return NULL;
        break;
      default:
        JS_NOT_REACHED("foo");
    }

    Linker linker(masm);
    return linker.newCode(cx);
}

bool
CompareCache::generateUpdate(JSContext *cx, MacroAssembler &masm)
{
    // Pop return address.
    masm.pop(esi);
    masm.pushValue(R1);
    masm.pushValue(R0);

    typedef bool (*pf)(JSContext *, HandleValue, HandleValue, MutableHandleValue);
    static const VMFunction fun = FunctionInfo<pf>(UpdateCompareCache);

    IonCompartment *ion = cx->compartment->ionCompartment();
    IonCode *wrapper = ion->generateVMWrapper(cx, fun);
    if (!wrapper)
        return false;

    masm.tailCallWithExitFrameFromBaseline(wrapper);
    masm.breakpoint();
    return true;
}

bool
CompareCache::generateInt32(JSContext *cx, MacroAssembler &masm)
{
    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, R0, &notInt32);
    masm.branchTestInt32(Assembler::NotEqual, R1, &notInt32);

    masm.cmpl(R0.payloadReg(), R1.payloadReg());

    switch (JSOp(*data.pc)) {
      case JSOP_LT:
        masm.setCC(Assembler::LessThan, R0.payloadReg());
        break;

      default:
        JS_NOT_REACHED("Unexpected compare op");
        break;
    }

    masm.movzxbl(R0.payloadReg(), R0.payloadReg());
    masm.movl(ImmType(JSVAL_TYPE_BOOLEAN), R0.typeReg());

    masm.ret();

    // Update cache state.
    masm.bind(&notInt32);
    generateUpdate(cx, masm);
    return true;
}
