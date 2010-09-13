/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "jsscope.h"
#include "jsnum.h"
#include "MonoIC.h"
#include "StubCalls.h"
#include "StubCalls-inl.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "CodeGenIncludes.h"
#include "methodjit/Compiler.h"
#include "InlineFrameAssembler.h"
#include "jsobj.h"

#include "jsinterpinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::mjit;
using namespace js::mjit::ic;

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::Address Address;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Call Call;

#if defined JS_MONOIC

static void
PatchGetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stubs::GetGlobalName));
    repatch.relink(mic.stubCall, fptr);
}

void JS_FASTCALL
ic::GetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp()->scopeChain().getGlobal();
    ic::MICInfo &mic = f.fp()->script()->mics[index];
    JSAtom *atom = f.fp()->script()->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::GET);

    JS_LOCK_OBJ(f.cx, obj);
    const Shape *shape = obj->nativeLookup(id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->hasSlot())
    {
        JS_UNLOCK_OBJ(f.cx, obj);
        if (shape)
            PatchGetFallback(f, mic);
        stubs::GetGlobalName(f);
        return;
    }
    uint32 slot = shape->slot;
    JS_UNLOCK_OBJ(f.cx, obj);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, obj->shape());

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);
    JSC::RepatchBuffer loads(mic.load.executableAddress(), 32, false);
#if defined JS_CPU_X86
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_DATA_OFFSET), slot);
    loads.repatch(mic.load.dataLabel32AtOffset(MICInfo::GET_TYPE_OFFSET), slot + 4);
#elif defined JS_CPU_ARM
    // mic.load actually points to the LDR instruction which fetches the offset, but 'repatch'
    // knows how to dereference it to find the integer value.
    loads.repatch(mic.load.dataLabel32AtOffset(0), slot);
#elif defined JS_PUNBOX64
    loads.repatch(mic.load.dataLabel32AtOffset(mic.patchValueOffset), slot);
#endif

    /* Do load anyway... this time. */
    stubs::GetGlobalName(f);
}

static void JS_FASTCALL
SetGlobalNameSlow(VMFrame &f, uint32 index)
{
    JSAtom *atom = f.fp()->script()->getAtom(GET_INDEX(f.regs.pc));
    stubs::SetGlobalName(f, atom);
}

static void
PatchSetFallback(VMFrame &f, ic::MICInfo &mic)
{
    JSC::RepatchBuffer repatch(mic.stubEntry.executableAddress(), 64);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, SetGlobalNameSlow));
    repatch.relink(mic.stubCall, fptr);
}

static VoidStubAtom
GetStubForSetGlobalName(VMFrame &f)
{
    // The property cache doesn't like inc ops, so we use a simpler
    // stub for that case.
    return js_CodeSpec[*f.regs.pc].format & (JOF_INC | JOF_DEC)
         ? stubs::SetGlobalNameDumb
         : stubs::SetGlobalName;
}

void JS_FASTCALL
ic::SetGlobalName(VMFrame &f, uint32 index)
{
    JSObject *obj = f.fp()->scopeChain().getGlobal();
    ic::MICInfo &mic = f.fp()->script()->mics[index];
    JSAtom *atom = f.fp()->script()->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    JS_ASSERT(mic.kind == ic::MICInfo::SET);

    JS_LOCK_OBJ(f.cx, obj);
    const Shape *shape = obj->nativeLookup(id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->writable() ||
        !shape->hasSlot())
    {
        JS_UNLOCK_OBJ(f.cx, obj);
        if (shape)
            PatchSetFallback(f, mic);
        GetStubForSetGlobalName(f)(f, atom);
        return;
    }
    uint32 slot = shape->slot;
    JS_UNLOCK_OBJ(f.cx, obj);

    mic.u.name.touched = true;

    /* Patch shape guard. */
    JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
    repatch.repatch(mic.shape, obj->shape());

    /* Patch loads. */
    JS_ASSERT(slot >= JS_INITIAL_NSLOTS);
    slot -= JS_INITIAL_NSLOTS;
    slot *= sizeof(Value);

    JSC::RepatchBuffer stores(mic.load.executableAddress(), 32, false);
#if defined JS_CPU_X86
    stores.repatch(mic.load.dataLabel32AtOffset(MICInfo::SET_TYPE_OFFSET), slot + 4);

    uint32 dataOffset;
    if (mic.u.name.typeConst)
        dataOffset = MICInfo::SET_DATA_CONST_TYPE_OFFSET;
    else
        dataOffset = MICInfo::SET_DATA_TYPE_OFFSET;
    stores.repatch(mic.load.dataLabel32AtOffset(dataOffset), slot);
#elif defined JS_CPU_ARM
    // mic.load actually points to the LDR instruction which fetches the offset, but 'repatch'
    // knows how to dereference it to find the integer value.
    stores.repatch(mic.load.dataLabel32AtOffset(0), slot);
#elif defined JS_PUNBOX64
    stores.repatch(mic.load.dataLabel32AtOffset(mic.patchValueOffset), slot);
#endif

    // Actually implement the op the slow way.
    GetStubForSetGlobalName(f)(f, atom);
}

static void * JS_FASTCALL
SlowCallFromIC(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic= oldscript->callICs[index];

    stubs::SlowCall(f, ic.argc);

    return NULL;
}

static void * JS_FASTCALL
SlowNewFromIC(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic = oldscript->callICs[index];

    stubs::SlowNew(f, ic.argc);

    return NULL;
}

/*
 * Calls have an inline path and an out-of-line path. The inline path is used
 * in the fastest case: the method has JIT'd code, and |argc == nargs|.
 * 
 * The inline path and OOL path are separated by a guard on the identity of
 * the callee object. This guard starts as NULL and always fails on the first
 * hit. On the OOL path, the callee is verified to be both a function and a
 * scripted function. If these conditions hold, |ic::Call| is invoked.
 *
 * |ic::Call| first ensures that the callee has JIT code. If it doesn't, the
 * call to |ic::Call| is patched to a slow path. If it does have JIT'd code,
 * the following cases can occur:
 *
 *   1) args != nargs: The call to |ic::Call| is patched with a dynamically
 *      generated stub. This stub inlines a path that looks like:
 *      ----
 *      push frame
 *      if (callee is not compiled) {
 *          Compile(callee);
 *      }
 *      call callee->arityLabel
 *
 *      The arity label is a special entry point for correcting frames for
 *      arity mismatches.
 *
 *   2) args == nargs, and the inline call site was not patched yet.
 *      The guard dividing the two paths is patched to guard on the given
 *      function object identity, and the proceeding call is patched to
 *      directly call the JIT code.
 *
 *   3) args == nargs, and the inline call site was patched already.
 *      A small stub is created which extends the original guard to also
 *      guard on the JSFunction lying underneath the function object.
 *
 * If the OOL path does not have a scripted function, but does have a
 * scripted native, then a small stub is generated which inlines the native
 * invocation.
 */
class CallCompiler
{
    VMFrame &f;
    JSContext *cx;
    CallICInfo &ic;
    Value *vp;
    bool callingNew;

  public:
    CallCompiler(VMFrame &f, CallICInfo &ic, bool callingNew)
      : f(f), cx(f.cx), ic(ic), vp(f.regs.sp - (ic.argc + 2)), callingNew(callingNew)
    {
    }

    JSC::ExecutablePool *poolForSize(size_t size, CallICInfo::PoolIndex index)
    {
        mjit::ThreadData *jm = &JS_METHODJIT_DATA(cx);
        JSC::ExecutablePool *ep = jm->execPool->poolForSize(size);
        if (!ep) {
            js_ReportOutOfMemory(f.cx);
            return NULL;
        }
        JS_ASSERT(!ic.pools[index]);
        ic.pools[index] = ep;
        return ep;
    }

    bool generateFullCallStub(JSScript *script, uint32 flags)
    {
        /*
         * Create a stub that works with arity mismatches. Like the fast-path,
         * this allocates a frame on the caller side, but also performs extra
         * checks for compilability. Perhaps this should be a separate, shared
         * trampoline, but for now we generate it dynamically.
         */
        Assembler masm;
        InlineFrameAssembler inlFrame(masm, cx, ic, flags);
        RegisterID t0 = inlFrame.tempRegs.takeAnyReg();

        /* Generate the inline frame creation. */
        inlFrame.assemble();

        /* funPtrReg is still valid. Check if a compilation is needed. */
        Address scriptAddr(ic.funPtrReg, offsetof(JSFunction, u) +
                           offsetof(JSFunction::U::Scripted, script));
        masm.loadPtr(scriptAddr, t0);

        /*
         * Test if script->nmap is NULL - same as checking ncode, but faster
         * here since ncode has two failure modes and we need to load out of
         * nmap anyway.
         */
        masm.loadPtr(Address(t0, offsetof(JSScript, jit)), t0);
        Jump hasCode = masm.branchTestPtr(Assembler::NonZero, t0, t0);

        /* Try and compile. On success we get back the nmap pointer. */
        masm.storePtr(JSFrameReg, FrameAddress(offsetof(VMFrame, regs.fp)));
        masm.move(Imm32(ic.argc), Registers::ArgReg1);
        JSC::MacroAssembler::Call tryCompile =
            masm.stubCall(JS_FUNC_TO_DATA_PTR(void *, stubs::CompileFunction),
                          script->code, ic.frameDepth);

        Jump notCompiled = masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                              Registers::ReturnReg);

        masm.call(Registers::ReturnReg);
        Jump done = masm.jump();

        hasCode.linkTo(masm.label(), &masm);

        /* Get nmap[ARITY], set argc, call. */
        masm.move(Imm32(ic.argc), JSParamReg_Argc);
        masm.loadPtr(Address(t0, offsetof(JITScript, arityCheck)), t0);
        masm.call(t0);

        /* Rejoin with the fast path. */
        Jump rejoin = masm.jump();

        /* Worst case - function didn't compile. */
        notCompiled.linkTo(masm.label(), &masm);
        masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);
        notCompiled = masm.jump();

        JSC::ExecutablePool *ep = poolForSize(masm.size(), CallICInfo::Pool_ScriptStub);
        if (!ep)
            return false;

        JSC::LinkBuffer buffer(&masm, ep);
        buffer.link(rejoin, ic.funGuard.labelAtOffset(ic.joinPointOffset));
        buffer.link(done, ic.funGuard.labelAtOffset(ic.joinPointOffset));
        buffer.link(notCompiled, ic.slowPathStart.labelAtOffset(ic.slowJoinOffset));
        buffer.link(tryCompile,
                    JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, stubs::CompileFunction)));
        JSC::CodeLocationLabel cs = buffer.finalizeCodeAddendum();

        JaegerSpew(JSpew_PICs, "generated CALL stub %p (%d bytes)\n", cs.executableAddress(),
                   masm.size());

        JSC::CodeLocationJump oolJump = ic.slowPathStart.jumpAtOffset(ic.oolJumpOffset);
        uint8 *start = (uint8 *)oolJump.executableAddress();
        JSC::RepatchBuffer repatch(start - 32, 64);
        repatch.relink(oolJump, cs);

        return true;
    }

    void patchInlinePath(JSScript *script, JSObject *obj)
    {
        /* Very fast path. */
        uint8 *start = (uint8 *)ic.funGuard.executableAddress();
        JSC::RepatchBuffer repatch(start - 32, 64);

        ic.fastGuardedObject = obj;

        repatch.repatch(ic.funGuard, obj);
        repatch.relink(ic.funGuard.callAtOffset(ic.hotCallOffset),
                       JSC::FunctionPtr(script->ncode));

        JaegerSpew(JSpew_PICs, "patched CALL path %p (obj: %p)\n", start, ic.fastGuardedObject);
    }

    bool generateStubForClosures(JSObject *obj)
    {
        /* Slightly less fast path - guard on fun->getFunctionPrivate() instead. */
        Assembler masm;

        Registers tempRegs;
        tempRegs.takeReg(ic.funObjReg);

        RegisterID t0 = tempRegs.takeAnyReg();

        /* Guard that it's actually a function object. */
        Jump claspGuard = masm.branchPtr(Assembler::NotEqual,
                                         Address(ic.funObjReg, offsetof(JSObject, clasp)),
                                         ImmPtr(&js_FunctionClass));

        /* Guard that it's the same function. */
        JSFunction *fun = obj->getFunctionPrivate();
        masm.loadFunctionPrivate(ic.funObjReg, t0);
        Jump funGuard = masm.branchPtr(Assembler::NotEqual, t0, ImmPtr(fun));
        Jump done = masm.jump();

        JSC::ExecutablePool *ep = poolForSize(masm.size(), CallICInfo::Pool_ClosureStub);
        if (!ep)
            return false;

        JSC::LinkBuffer buffer(&masm, ep);
        buffer.link(claspGuard, ic.slowPathStart);
        buffer.link(funGuard, ic.slowPathStart);
        buffer.link(done, ic.funGuard.labelAtOffset(ic.hotPathOffset));
        JSC::CodeLocationLabel cs = buffer.finalizeCodeAddendum();

        JaegerSpew(JSpew_PICs, "generated CALL closure stub %p (%d bytes)\n",
                   cs.executableAddress(), masm.size());

        uint8 *start = (uint8 *)ic.funJump.executableAddress();
        JSC::RepatchBuffer repatch(start - 32, 64);
        repatch.relink(ic.funJump, cs);

        ic.hasJsFunCheck = true;

        return true;
    }

    bool generateNativeStub()
    {
        Value *vp = f.regs.sp - (ic.argc + 2);

        JSObject *obj;
        if (!IsFunctionObject(*vp, &obj))
            return false;

        JSFunction *fun = obj->getFunctionPrivate();
        if ((!callingNew && !fun->isNative()) || (callingNew && !fun->isConstructor()))
            return false;

        if (callingNew)
            vp[1].setMagicWithObjectOrNullPayload(NULL);

        Native fn = fun->u.n.native;
        if (!fn(cx, ic.argc, vp))
            THROWV(true);

        /* Right now, take slow-path for IC misses or multiple stubs. */
        if (ic.fastGuardedNative || ic.hasJsFunCheck)
            return true;

        /* Native MIC needs to warm up first. */
        if (!ic.hit) {
            ic.hit = true;
            return true;
        }

        /* Generate fast-path for calling this native. */
        Assembler masm;

        /* Guard on the function object identity, for now. */
        Jump funGuard = masm.branchPtr(Assembler::NotEqual, ic.funObjReg, ImmPtr(obj));

        Registers tempRegs;
#ifndef JS_CPU_X86
        tempRegs.takeReg(Registers::ArgReg0);
        tempRegs.takeReg(Registers::ArgReg1);
        tempRegs.takeReg(Registers::ArgReg2);
#endif
        RegisterID t0 = tempRegs.takeAnyReg();

        /* Store pc. */
        masm.storePtr(ImmPtr(cx->regs->pc),
                       FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, pc)));

        /* Store sp. */
        uint32 spOffset = sizeof(JSStackFrame) + ic.frameDepth * sizeof(Value);
        masm.addPtr(Imm32(spOffset), JSFrameReg, t0);
        masm.storePtr(t0, FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, sp)));

        /* Grab cx early on to avoid stack mucking on x86. */
#ifdef JS_CPU_X86
        RegisterID cxReg = tempRegs.takeAnyReg();
#else
        RegisterID cxReg = Registers::ArgReg0;
#endif
        masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), cxReg);

#ifdef JS_CPU_X86
        /* x86's stack should be 16-byte aligned. */
        masm.subPtr(Imm32(16), Assembler::stackPointerRegister);
#endif

        /* Compute vp. */
#ifdef JS_CPU_X86
        RegisterID vpReg = t0;
#else
        RegisterID vpReg = Registers::ArgReg2;
#endif
        
        uint32 vpOffset = sizeof(JSStackFrame) + (ic.frameDepth - ic.argc - 2) * sizeof(Value);
        masm.addPtr(Imm32(vpOffset), JSFrameReg, vpReg);

        /* Mark vp[1] as magic for |new|. */
        if (callingNew) {
            Value v;
            v.setMagicWithObjectOrNullPayload(NULL);
            masm.storeValue(v, Address(vpReg, sizeof(Value)));
        }

#ifdef JS_CPU_X86
        masm.storePtr(vpReg, Address(Assembler::stackPointerRegister, 8));
#endif

        /* Push argc. */
#ifdef JS_CPU_X86
        masm.store32(Imm32(ic.argc), Address(Assembler::stackPointerRegister, 4));
#else
        masm.move(Imm32(ic.argc), Registers::ArgReg1);
#endif

        /* Push cx. */
#ifdef JS_CPU_X86
        masm.storePtr(cxReg, Address(Assembler::stackPointerRegister, 0));
#endif

        /* Make the call. */
        Assembler::Call call = masm.call();

#ifdef JS_CPU_X86
        masm.addPtr(Imm32(16), Assembler::stackPointerRegister);
#endif
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        // Usually JaegerThrowpoline got called from return address.
        // So in JaegerThrowpoline without fastcall, esp was added by 8.
        // If we just want to jump there, we need to sub esp by 8 first.
        masm.subPtr(Imm32(8), Assembler::stackPointerRegister);
#endif

        Jump hasException = masm.branchTest32(Assembler::Zero, Registers::ReturnReg,
                                              Registers::ReturnReg);
        

#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        // Usually JaegerThrowpoline got called from return address.
        // So in JaegerThrowpoline without fastcall, esp was added by 8.
        // If we just want to jump there, we need to sub esp by 8 first.
        masm.addPtr(Imm32(8), Assembler::stackPointerRegister);
#endif

        Jump done = masm.jump();

        /* Move JaegerThrowpoline into register for very far jump on x64. */
        hasException.linkTo(masm.label(), &masm);
        masm.move(ImmPtr(JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline)), Registers::ReturnReg);
        masm.jump(Registers::ReturnReg);

        JSC::ExecutablePool *ep = poolForSize(masm.size(), CallICInfo::Pool_NativeStub);
        if (!ep)
            THROWV(true);

        JSC::LinkBuffer buffer(&masm, ep);
        buffer.link(done, ic.slowPathStart.labelAtOffset(ic.slowJoinOffset));
        buffer.link(call, JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, fun->u.n.native)));
        buffer.link(funGuard, ic.slowPathStart);
        
        JSC::CodeLocationLabel cs = buffer.finalizeCodeAddendum();

        JaegerSpew(JSpew_PICs, "generated native CALL stub %p (%d bytes)\n",
                   cs.executableAddress(), masm.size());

        uint8 *start = (uint8 *)ic.funJump.executableAddress();
        JSC::RepatchBuffer repatch(start - 32, 64);
        repatch.relink(ic.funJump, cs);

        ic.fastGuardedNative = obj;

        return true;
    }

    void *update()
    {
        JSObject *obj;
        if (!IsFunctionObject(*vp, &obj) || !(cx->options & JSOPTION_METHODJIT)) {
            /* Ugh. Can't do anything with this! */
            if (callingNew)
                stubs::SlowNew(f, ic.argc);
            else
                stubs::SlowCall(f, ic.argc);
            return NULL;
        }

        JSFunction *fun = obj->getFunctionPrivate();
        JSObject *scopeChain = obj->getParent();

        /* The slow path guards against natives. */
        JS_ASSERT(fun->isInterpreted());
        JSScript *script = fun->u.i.script;

        if (!script->ncode && !script->isEmpty()) {
            if (mjit::TryCompile(cx, script, fun, scopeChain) == Compile_Error)
                THROWV(NULL);
        }
        JS_ASSERT(script->isEmpty() || script->ncode);

        if (script->ncode == JS_UNJITTABLE_METHOD || script->isEmpty()) {
            /* This should always go to a slow path, sadly. */
            JSC::CodeLocationCall oolCall = ic.slowPathStart.callAtOffset(ic.oolCallOffset);
            uint8 *start = (uint8 *)oolCall.executableAddress();
            JSC::RepatchBuffer repatch(start - 32, 64);
            JSC::FunctionPtr fptr = callingNew
                                    ? JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, SlowNewFromIC))
                                    : JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, SlowCallFromIC));
            repatch.relink(oolCall, fptr);
            if (callingNew)
                stubs::SlowNew(f, ic.argc);
            else
                stubs::SlowCall(f, ic.argc);
            return NULL;
        }

        uint32 flags = callingNew ? JSFRAME_CONSTRUCTING : 0;
        if (callingNew)
            stubs::NewObject(f, ic.argc);

        if (!ic.hit) {
            if (ic.argc != fun->nargs) {
                if (!generateFullCallStub(script, flags))
                    THROWV(NULL);
            } else {
                if (!ic.fastGuardedObject) {
                    patchInlinePath(script, obj);
                } else if (!ic.hasJsFunCheck &&
                           !ic.fastGuardedNative &&
                           ic.fastGuardedObject->getFunctionPrivate() == fun) {
                    /*
                     * Note: Multiple "function guard" stubs are not yet
                     * supported, thus the fastGuardedNative check.
                     */
                    if (!generateStubForClosures(obj))
                        THROWV(NULL);
                } else {
                    if (!generateFullCallStub(script, flags))
                        THROWV(NULL);
                }
            }
        } else {
            ic.hit = true;
        }

        /*
         * We are about to jump into the callee's prologue, so push the frame as
         * if we had been executing in the caller's inline call path. In theory,
         * we could actually jump to the inline call path, but doing it from
         * C++ allows JSStackFrame::initCallFrameCallerHalf to serve as a
         * reference for what the jitted path should be doing.
         */
        JSStackFrame *fp = (JSStackFrame *)f.regs.sp;
        fp->initCallFrameCallerHalf(cx, *scopeChain, ic.argc, flags);
        f.regs.fp = fp;

        if (ic.argc == fun->nargs)
            return script->ncode;
        return script->jit->arityCheck;
    }
};

void * JS_FASTCALL
ic::Call(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic = oldscript->callICs[index];
    CallCompiler cc(f, ic, false);
    return cc.update();
}

void * JS_FASTCALL
ic::New(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic = oldscript->callICs[index];
    CallCompiler cc(f, ic, true);
    return cc.update();
}

void JS_FASTCALL
ic::NativeCall(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic = oldscript->callICs[index];
    CallCompiler cc(f, ic, false);
    if (!cc.generateNativeStub())
        stubs::SlowCall(f, ic.argc);
}

void JS_FASTCALL
ic::NativeNew(VMFrame &f, uint32 index)
{
    JSScript *oldscript = f.fp()->script();
    CallICInfo &ic = oldscript->callICs[index];
    CallCompiler cc(f, ic, true);
    if (!cc.generateNativeStub())
        stubs::SlowNew(f, ic.argc);
}

void
ic::PurgeMICs(JSContext *cx, JSScript *script)
{
    /* MICs are purged during GC to handle changing shapes. */
    JS_ASSERT(cx->runtime->gcRegenShapes);

    uint32 nmics = script->jit->nMICs;
    for (uint32 i = 0; i < nmics; i++) {
        ic::MICInfo &mic = script->mics[i];
        switch (mic.kind) {
          case ic::MICInfo::SET:
          case ic::MICInfo::GET:
          {
            /* Patch shape guard. */
            JSC::RepatchBuffer repatch(mic.entry.executableAddress(), 50);
            repatch.repatch(mic.shape, int(JSObjectMap::INVALID_SHAPE));

            /* 
             * If the stub call was patched, leave it alone -- it probably will
             * just be invalidated again.
             */
            break;
          }
          case ic::MICInfo::TRACER:
            /* Nothing to patch! */
            break;
          default:
            JS_NOT_REACHED("Unknown MIC type during purge");
            break;
        }
    }
}

void
ic::SweepCallICs(JSContext *cx, JSScript *script)
{
    for (uint32 i = 0; i < script->jit->nCallICs; i++) {
        ic::CallICInfo &ic = script->callICs[i];

        /*
         * If the object is unreachable, we're guaranteed not to be currently
         * executing a stub generated by a guard on that object. This lets us
         * precisely GC call ICs while keeping the identity guard safe.
         */
        bool fastFunDead = ic.fastGuardedObject && js_IsAboutToBeFinalized(ic.fastGuardedObject);
        bool nativeDead = ic.fastGuardedNative && js_IsAboutToBeFinalized(ic.fastGuardedNative);

        if (!fastFunDead && !nativeDead)
            continue;

        uint8 *start = (uint8 *)ic.funGuard.executableAddress();
        JSC::RepatchBuffer repatch(start - 32, 64);

        if (fastFunDead) {
            repatch.repatch(ic.funGuard, NULL);
            ic.releasePool(CallICInfo::Pool_ClosureStub);
            ic.hasJsFunCheck = false;
            ic.fastGuardedObject = NULL;
        }

        if (nativeDead) {
            ic.releasePool(CallICInfo::Pool_NativeStub);
            ic.fastGuardedNative = NULL;
        }

        repatch.relink(ic.funJump, ic.slowPathStart);

        ic.hit = false;
    }
}

#endif /* JS_MONOIC */

