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
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/CodeGenIncludes.h"
#include "methodjit/Compiler.h"
#include "methodjit/ICRepatcher.h"
#include "methodjit/MethodJIT-inl.h"
#include "methodjit/PolyIC.h"
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
typedef JSC::MacroAssembler::Label Label;
typedef JSC::MacroAssembler::DataLabel32 DataLabel32;
typedef JSC::MacroAssembler::DataLabelPtr DataLabelPtr;

#if defined JS_MONOIC

static void
PatchGetFallback(VMFrame &f, ic::GetGlobalNameIC *ic)
{
    Repatcher repatch(f.jit());
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stubs::GetGlobalName));
    repatch.relink(ic->slowPathCall, fptr);
}

void JS_FASTCALL
ic::GetGlobalName(VMFrame &f, ic::GetGlobalNameIC *ic)
{
    JSObject *obj = f.fp()->scopeChain().getGlobal();
    JSAtom *atom = f.fp()->script()->getAtom(GET_INDEX(f.regs.pc));
    jsid id = ATOM_TO_JSID(atom);

    const Shape *shape = obj->nativeLookup(id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->hasSlot())
    {
        if (shape)
            PatchGetFallback(f, ic);
        stubs::GetGlobalName(f);
        return;
    }
    uint32 slot = shape->slot;

    /* Patch shape guard. */
    Repatcher repatcher(f.jit());
    repatcher.repatch(ic->fastPathStart.dataLabel32AtOffset(ic->shapeOffset), obj->shape());

    /* Patch loads. */
    JSC::CodeLocationLabel label = ic->fastPathStart.labelAtOffset(ic->loadStoreOffset);
    repatcher.patchAddressOffsetForValueLoad(label, slot * sizeof(Value));

    /* Do load anyway... this time. */
    stubs::GetGlobalName(f);
}

template <JSBool strict>
static void JS_FASTCALL
DisabledSetGlobal(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.fp()->script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.regs.pc));
    stubs::SetGlobalName<strict>(f, atom);
}

template void JS_FASTCALL DisabledSetGlobal<true>(VMFrame &f, ic::SetGlobalNameIC *ic);
template void JS_FASTCALL DisabledSetGlobal<false>(VMFrame &f, ic::SetGlobalNameIC *ic);

template <JSBool strict>
static void JS_FASTCALL
DisabledSetGlobalNoCache(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.fp()->script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.regs.pc));
    stubs::SetGlobalNameNoCache<strict>(f, atom);
}

template void JS_FASTCALL DisabledSetGlobalNoCache<true>(VMFrame &f, ic::SetGlobalNameIC *ic);
template void JS_FASTCALL DisabledSetGlobalNoCache<false>(VMFrame &f, ic::SetGlobalNameIC *ic);

static void
PatchSetFallback(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.fp()->script();

    Repatcher repatch(f.jit());
    VoidStubSetGlobal stub = ic->usePropertyCache
                             ? STRICT_VARIANT(DisabledSetGlobal)
                             : STRICT_VARIANT(DisabledSetGlobalNoCache);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stub));
    repatch.relink(ic->slowPathCall, fptr);
}

void
SetGlobalNameIC::patchExtraShapeGuard(Repatcher &repatcher, int32 shape)
{
    JS_ASSERT(hasExtraStub);

    JSC::CodeLocationLabel label(JSC::MacroAssemblerCodePtr(extraStub.start()));
    repatcher.repatch(label.dataLabel32AtOffset(extraShapeGuard), shape);
}

void
SetGlobalNameIC::patchInlineShapeGuard(Repatcher &repatcher, int32 shape)
{
    JSC::CodeLocationDataLabel32 label = fastPathStart.dataLabel32AtOffset(shapeOffset);
    repatcher.repatch(label, shape);
}

static LookupStatus
UpdateSetGlobalNameStub(VMFrame &f, ic::SetGlobalNameIC *ic, JSObject *obj, const Shape *shape)
{
    Repatcher repatcher(ic->extraStub);

    ic->patchExtraShapeGuard(repatcher, obj->shape());

    JSC::CodeLocationLabel label(JSC::MacroAssemblerCodePtr(ic->extraStub.start()));
    label = label.labelAtOffset(ic->extraStoreOffset);
    repatcher.patchAddressOffsetForValueStore(label, shape->slot * sizeof(Value),
                                              ic->vr.isTypeKnown());

    return Lookup_Cacheable;
}

static LookupStatus
AttachSetGlobalNameStub(VMFrame &f, ic::SetGlobalNameIC *ic, JSObject *obj, const Shape *shape)
{
    Assembler masm;

    Label start = masm.label();

    DataLabel32 shapeLabel;
    Jump guard = masm.branch32WithPatch(Assembler::NotEqual, ic->shapeReg, Imm32(obj->shape()),
                                        shapeLabel);

    /* A constant object needs rematerialization. */
    if (ic->objConst)
        masm.move(ImmPtr(obj), ic->objReg);

    JS_ASSERT(obj->branded());

    /*
     * Load obj->slots. If ic->objConst, then this clobbers objReg, because
     * ic->objReg == ic->shapeReg.
     */
    masm.loadPtr(Address(ic->objReg, offsetof(JSObject, slots)), ic->shapeReg);

    /* Test if overwriting a function-tagged slot. */
    Address slot(ic->shapeReg, sizeof(Value) * shape->slot);
    Jump isNotObject = masm.testObject(Assembler::NotEqual, slot);

    /* Now, test if the object is a function object. */
    masm.loadPayload(slot, ic->shapeReg);
    Jump isFun = masm.testFunction(Assembler::Equal, ic->shapeReg);

    /* Restore shapeReg to obj->slots, since we clobbered it. */
    if (ic->objConst)
        masm.move(ImmPtr(obj), ic->objReg);
    masm.loadPtr(Address(ic->objReg, offsetof(JSObject, slots)), ic->shapeReg);

    /* If the object test fails, shapeReg is still obj->slots. */
    isNotObject.linkTo(masm.label(), &masm);
    DataLabel32 store = masm.storeValueWithAddressOffsetPatch(ic->vr, slot);

    Jump done = masm.jump();

    JITScript *jit = f.jit();
    LinkerHelper linker(masm);
    JSC::ExecutablePool *ep = linker.init(f.cx);
    if (!ep)
        return Lookup_Error;
    if (!jit->execPools.append(ep)) {
        ep->release();
        js_ReportOutOfMemory(f.cx);
        return Lookup_Error;
    }

    if (!linker.verifyRange(jit))
        return Lookup_Uncacheable;

    linker.link(done, ic->fastPathStart.labelAtOffset(ic->fastRejoinOffset));
    linker.link(guard, ic->slowPathStart);
    linker.link(isFun, ic->slowPathStart);

    JSC::CodeLocationLabel cs = linker.finalize();
    JaegerSpew(JSpew_PICs, "generated setgname stub at %p\n", cs.executableAddress());

    Repatcher repatcher(f.jit());
    repatcher.relink(ic->fastPathStart.jumpAtOffset(ic->inlineShapeJump), cs);

    int offset = linker.locationOf(shapeLabel) - linker.locationOf(start);
    ic->extraShapeGuard = offset;
    JS_ASSERT(ic->extraShapeGuard == offset);

    ic->extraStub = JSC::JITCode(cs.executableAddress(), linker.size());
    offset = linker.locationOf(store) - linker.locationOf(start);
    ic->extraStoreOffset = offset;
    JS_ASSERT(ic->extraStoreOffset == offset);

    ic->hasExtraStub = true;

    return Lookup_Cacheable;
}

static LookupStatus
UpdateSetGlobalName(VMFrame &f, ic::SetGlobalNameIC *ic, JSObject *obj, const Shape *shape)
{
    /* Give globals a chance to appear. */
    if (!shape)
        return Lookup_Uncacheable;

    if (shape->isMethod() ||
        !shape->hasDefaultSetter() ||
        !shape->writable() ||
        !shape->hasSlot())
    {
        /* Disable the IC for weird shape attributes. */
        PatchSetFallback(f, ic);
        return Lookup_Uncacheable;
    }

    /* Branded sets must guard that they don't overwrite method-valued properties. */
    if (obj->branded()) {
        /*
         * If this slot has a function valued property, the tail of this opcode
         * could change the shape. Even if it doesn't, the IC is probably
         * pointless, because it will always hit the function-test path and
         * bail out. In these cases, don't bother building or updating the IC.
         */
        const Value &v = obj->getSlot(shape->slot);
        if (v.isObject() && v.toObject().isFunction()) {
            /*
             * If we're going to rebrand, the object may unbrand, allowing this
             * IC to come back to life. In that case, we don't disable the IC.
             */
            if (!ChangesMethodValue(v, f.regs.sp[-1]))
                PatchSetFallback(f, ic);
            return Lookup_Uncacheable;
        }

        if (ic->hasExtraStub)
            return UpdateSetGlobalNameStub(f, ic, obj, shape);

        return AttachSetGlobalNameStub(f, ic, obj, shape);
    }

    /* Object is not branded, so we can use the inline path. */
    Repatcher repatcher(f.jit());
    ic->patchInlineShapeGuard(repatcher, obj->shape());

    JSC::CodeLocationLabel label = ic->fastPathStart.labelAtOffset(ic->loadStoreOffset);
    repatcher.patchAddressOffsetForValueStore(label, shape->slot * sizeof(Value),
                                              ic->vr.isTypeKnown());

    return Lookup_Cacheable;
}

void JS_FASTCALL
ic::SetGlobalName(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSObject *obj = f.fp()->scopeChain().getGlobal();
    JSScript *script = f.fp()->script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.regs.pc));
    const Shape *shape = obj->nativeLookup(ATOM_TO_JSID(atom));

    LookupStatus status = UpdateSetGlobalName(f, ic, obj, shape);
    if (status == Lookup_Error)
        THROW();

    if (ic->usePropertyCache)
        STRICT_VARIANT(stubs::SetGlobalName)(f, atom);
    else
        STRICT_VARIANT(stubs::SetGlobalNameNoCache)(f, atom);
}

class EqualityICLinker : public LinkerHelper
{
    VMFrame &f;

  public:
    EqualityICLinker(Assembler &masm, VMFrame &f)
        : LinkerHelper(masm), f(f)
    { }

    bool init(JSContext *cx) {
        JSC::ExecutablePool *pool = LinkerHelper::init(cx);
        if (!pool)
            return false;
        JSScript *script = f.fp()->script();
        JITScript *jit = script->getJIT(f.fp()->isConstructing());
        if (!jit->execPools.append(pool)) {
            pool->release();
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }
};

/* Rough over-estimate of how much memory we need to unprotect. */
static const uint32 INLINE_PATH_LENGTH = 64;

class EqualityCompiler : public BaseCompiler
{
    VMFrame &f;
    EqualityICInfo &ic;

    Vector<Jump, 4, SystemAllocPolicy> jumpList;
    Jump trueJump;
    Jump falseJump;
    
  public:
    EqualityCompiler(VMFrame &f, EqualityICInfo &ic)
        : BaseCompiler(f.cx), f(f), ic(ic), jumpList(SystemAllocPolicy())
    {
    }

    void linkToStub(Jump j)
    {
        jumpList.append(j);
    }

    void linkTrue(Jump j)
    {
        trueJump = j;
    }

    void linkFalse(Jump j)
    {
        falseJump = j;
    }
    
    void generateStringPath(Assembler &masm)
    {
        const ValueRemat &lvr = ic.lvr;
        const ValueRemat &rvr = ic.rvr;

        JS_ASSERT_IF(lvr.isConstant(), lvr.isType(JSVAL_TYPE_STRING));
        JS_ASSERT_IF(rvr.isConstant(), rvr.isType(JSVAL_TYPE_STRING));

        if (!lvr.isType(JSVAL_TYPE_STRING)) {
            Jump lhsFail = masm.testString(Assembler::NotEqual, lvr.typeReg());
            linkToStub(lhsFail);
        }
        
        if (!rvr.isType(JSVAL_TYPE_STRING)) {
            Jump rhsFail = masm.testString(Assembler::NotEqual, rvr.typeReg());
            linkToStub(rhsFail);
        }

        RegisterID tmp = ic.tempReg;
        
        /* JSString::isAtom === (lengthAndFlags & ATOM_MASK == 0) */
        JS_STATIC_ASSERT(JSString::ATOM_FLAGS == 0);
        Imm32 atomMask(JSString::ATOM_MASK);
        
        masm.load32(Address(lvr.dataReg(), JSString::offsetOfLengthAndFlags()), tmp);
        Jump lhsNotAtomized = masm.branchTest32(Assembler::NonZero, tmp, atomMask);
        linkToStub(lhsNotAtomized);

        if (!rvr.isConstant()) {
            masm.load32(Address(rvr.dataReg(), JSString::offsetOfLengthAndFlags()), tmp);
            Jump rhsNotAtomized = masm.branchTest32(Assembler::NonZero, tmp, atomMask);
            linkToStub(rhsNotAtomized);
        }

        if (rvr.isConstant()) {
            JSString *str = rvr.value().toString();
            JS_ASSERT(str->isAtom());
            Jump test = masm.branchPtr(ic.cond, lvr.dataReg(), ImmPtr(str));
            linkTrue(test);
        } else {
            Jump test = masm.branchPtr(ic.cond, lvr.dataReg(), rvr.dataReg());
            linkTrue(test);
        }

        Jump fallthrough = masm.jump();
        linkFalse(fallthrough);
    }

    void generateObjectPath(Assembler &masm)
    {
        ValueRemat &lvr = ic.lvr;
        ValueRemat &rvr = ic.rvr;
        
        if (!lvr.isConstant() && !lvr.isType(JSVAL_TYPE_OBJECT)) {
            Jump lhsFail = masm.testObject(Assembler::NotEqual, lvr.typeReg());
            linkToStub(lhsFail);
        }
        
        if (!rvr.isConstant() && !rvr.isType(JSVAL_TYPE_OBJECT)) {
            Jump rhsFail = masm.testObject(Assembler::NotEqual, rvr.typeReg());
            linkToStub(rhsFail);
        }

        Jump lhsHasEq = masm.branchTest32(Assembler::NonZero,
                                          Address(lvr.dataReg(),
                                                  offsetof(JSObject, flags)),
                                          Imm32(JSObject::HAS_EQUALITY));
        linkToStub(lhsHasEq);

        if (rvr.isConstant()) {
            JSObject *obj = &rvr.value().toObject();
            Jump test = masm.branchPtr(ic.cond, lvr.dataReg(), ImmPtr(obj));
            linkTrue(test);
        } else {
            Jump test = masm.branchPtr(ic.cond, lvr.dataReg(), rvr.dataReg());
            linkTrue(test);
        }

        Jump fallthrough = masm.jump();
        linkFalse(fallthrough);
    }

    bool linkForIC(Assembler &masm)
    {
        EqualityICLinker buffer(masm, f);
        if (!buffer.init(cx))
            return false;

        Repatcher repatcher(f.jit());

        /* Overwrite the call to the IC with a call to the stub. */
        JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, ic.stub));
        repatcher.relink(ic.stubCall, fptr);

        // Silently fail, the IC is disabled now.
        if (!buffer.verifyRange(f.jit()))
            return true;

        /* Set the targets of all type test failures to go to the stub. */
        for (size_t i = 0; i < jumpList.length(); i++)
            buffer.link(jumpList[i], ic.stubEntry);
        jumpList.clear();

        /* Set the targets for the the success and failure of the actual equality test. */
        buffer.link(trueJump, ic.target);
        buffer.link(falseJump, ic.fallThrough);

        CodeLocationLabel cs = buffer.finalize();

        /* Jump to the newly generated code instead of to the IC. */
        repatcher.relink(ic.jumpToStub, cs);

        return true;
    }

    bool update()
    {
        if (!ic.generated) {
            Assembler masm;
            Value rval = f.regs.sp[-1];
            Value lval = f.regs.sp[-2];
            
            if (rval.isObject() && lval.isObject()) {
                generateObjectPath(masm);
                ic.generated = true;
            } else if (rval.isString() && lval.isString()) {
                generateStringPath(masm);
                ic.generated = true;
            } else {
                return true;
            }

            return linkForIC(masm);
        }

        return true;
    }
};

JSBool JS_FASTCALL
ic::Equality(VMFrame &f, ic::EqualityICInfo *ic)
{
    EqualityCompiler cc(f, *ic);
    if (!cc.update())
        THROWV(JS_FALSE);

    return ic->stub(f);
}

static const unsigned MANY_ARGS = 1024;
static const unsigned MIN_SPACE = 500;

static bool
BumpStackFull(VMFrame &f, uintN inc)
{
    /* If we are not passing many args, treat this as a normal call. */
    if (inc < MANY_ARGS) {
        if (f.regs.sp + inc < f.stackLimit)
            return true;
        StackSpace &stack = f.cx->stack();
        if (!stack.bumpCommitAndLimit(f.entryfp, f.regs.sp, inc, &f.stackLimit)) {
            js_ReportOverRecursed(f.cx);
            return false;
        }
        return true;
    }

    /*
     * The purpose of f.stackLimit is to catch over-recursion based on
     * assumptions about the average frame size. 'apply' with a large number of
     * arguments breaks these assumptions and can result in premature "out of
     * script quota" errors. Normally, apply will go through js::Invoke, which
     * effectively starts a fresh stackLimit. Here, we bump f.stackLimit,
     * if necessary, to allow for this 'apply' call, and a reasonable number of
     * subsequent calls, to succeed without hitting the stackLimit. In theory,
     * this a recursive chain containing apply to circumvent the stackLimit.
     * However, since each apply call must consume at least MANY_ARGS slots,
     * this sequence will quickly reach the end of the stack and OOM.
     */

    uintN incWithSpace = inc + MIN_SPACE;
    Value *bumpedWithSpace = f.regs.sp + incWithSpace;
    if (bumpedWithSpace < f.stackLimit)
        return true;

    StackSpace &stack = f.cx->stack();
    if (stack.bumpCommitAndLimit(f.entryfp, f.regs.sp, incWithSpace, &f.stackLimit))
        return true;

    if (!stack.ensureSpace(f.cx, f.regs.sp, incWithSpace))
        return false;
    f.stackLimit = bumpedWithSpace;
    return true;
}

static JS_ALWAYS_INLINE bool
BumpStack(VMFrame &f, uintN inc)
{
    /* Fast path BumpStackFull. */
    if (inc < MANY_ARGS && f.regs.sp + inc < f.stackLimit)
        return true;
    return BumpStackFull(f, inc);
}

/*
 * SplatApplyArgs is only called for expressions of the form |f.apply(x, y)|.
 * Additionally, the callee has already been checked to be the native apply.
 * All successful paths through SplatApplyArgs must set f.u.call.dynamicArgc
 * and f.regs.sp.
 */
template <bool LazyArgsObj>
JSBool JS_FASTCALL
ic::SplatApplyArgs(VMFrame &f)
{
    JSContext *cx = f.cx;
    JS_ASSERT(GET_ARGC(f.regs.pc) == 2);

    /*
     * The lazyArgsObj flag indicates an optimized call |f.apply(x, arguments)|
     * where the args obj has not been created or pushed on the stack. Thus,
     * if lazyArgsObj is set, the stack for |f.apply(x, arguments)| is:
     *
     *  | Function.prototype.apply | f | x |
     *
     * Otherwise, if !lazyArgsObj, the stack is a normal 2-argument apply:
     *
     *  | Function.prototype.apply | f | x | arguments |
     */
    if (LazyArgsObj) {
        Value *vp = f.regs.sp - 3;
        JS_ASSERT(JS_CALLEE(cx, vp).toObject().getFunctionPrivate()->u.n.native == js_fun_apply);

        JSStackFrame *fp = f.regs.fp;
        if (!fp->hasOverriddenArgs()) {
            uintN n;
            if (!fp->hasArgsObj()) {
                /* Extract the common/fast path where there is no args obj. */
                n = fp->numActualArgs();
                if (!BumpStack(f, n))
                    THROWV(false);
                Value *argv = JS_ARGV(cx, vp + 1 /* vp[1]'s argv */);
                f.regs.sp += n;
                fp->forEachCanonicalActualArg(CopyTo(argv));
            } else {
                /* Simulate the argument-pushing part of js_fun_apply: */
                JSObject *aobj = &fp->argsObj();

                /* Steps 4-5 */
                uintN length;
                if (!js_GetLengthProperty(cx, aobj, &length))
                    THROWV(false);

                /* Step 6 */
                JS_ASSERT(length <= JS_ARGS_LENGTH_MAX);
                n = length;

                if (!BumpStack(f, n))
                    THROWV(false);

                /* Steps 7-8 */
                Value *argv = JS_ARGV(cx, &vp[1]);  /* vp[1] is the callee */
                f.regs.sp += n;  /* GetElements may reenter, so inc early. */
                if (!GetElements(cx, aobj, n, argv))
                    THROWV(false);
            }

            f.u.call.dynamicArgc = n;
            return true;
        }

        /*
         * Push the arguments value so that the stack matches the !lazyArgsObj
         * stack state described above.
         */
        f.regs.sp++;
        if (!js_GetArgsValue(cx, fp, &vp[3]))
            THROWV(false);
    }

    Value *vp = f.regs.sp - 4;
    JS_ASSERT(JS_CALLEE(cx, vp).toObject().getFunctionPrivate()->u.n.native == js_fun_apply);

    /*
     * This stub should mimic the steps taken by js_fun_apply. Step 1 and part
     * of Step 2 have already been taken care of by calling jit code.
     */

    /* Step 2 (part 2). */
    if (vp[3].isNullOrUndefined()) {
        f.regs.sp--;
        f.u.call.dynamicArgc = 0;
        return true;
    }

    /* Step 3. */
    if (!vp[3].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
        THROWV(false);
    }

    /* Steps 4-5. */
    JSObject *aobj = &vp[3].toObject();
    jsuint length;
    if (!js_GetLengthProperty(cx, aobj, &length))
        THROWV(false);

    JS_ASSERT(!JS_ON_TRACE(cx));

    /* Step 6. */
    uintN n = uintN(JS_MIN(length, JS_ARGS_LENGTH_MAX));

    intN delta = n - 1;
    if (delta > 0 && !BumpStack(f, delta))
        THROWV(false);
    f.regs.sp += delta;

    /* Steps 7-8. */
    if (!GetElements(cx, aobj, n, f.regs.sp - n))
        THROWV(false);

    f.u.call.dynamicArgc = n;
    return true;
}
template JSBool JS_FASTCALL ic::SplatApplyArgs<true>(VMFrame &f);
template JSBool JS_FASTCALL ic::SplatApplyArgs<false>(VMFrame &f);

template <bool IsNew>
static bool
CallHelper(VMFrame &f, ic::CallIC *ic, CallDescription &call)
{
    Value *vp = f.regs.sp - (2 + call.argc);

    // Take the slowest path if the callee is not a funobj, or if the function
    // is native and constructing.
    if (vp->isObject()) {
        call.callee = &vp->toObject();
        call.fun = call.callee->isFunction() ? call.callee->getFunctionPrivate() : NULL;
    } else {
        call.callee = call.fun = NULL;
    }

    if (!call.fun || call.fun->isNative()) {
        if (IsNew)
            return InvokeConstructor(f.cx, InvokeArgsAlreadyOnTheStack(vp, call.argc));
        if (!call.fun)
            return Invoke(f.cx, InvokeArgsAlreadyOnTheStack(vp, call.argc), 0);
        return CallJSNative(f.cx, call.fun->u.n.native, call.argc, vp);
    }

    // The function is interpreted.
    uint32 flags = IsNew ? JSFRAME_CONSTRUCTING : 0;
    JSScript *newscript = call.fun->script();
    StackSpace &stack = f.cx->stack();
    JSStackFrame *newfp = stack.getInlineFrameWithinLimit(f.cx, f.regs.sp, call.argc,
                                                          call.fun, newscript, &flags,
                                                          f.entryfp, &f.stackLimit);
    if (!newfp)
        return false;

    // Initialize the frame and locals.
    newfp->initCallFrame(f.cx, *call.callee, call.fun, call.argc, flags);
    SetValueRangeToUndefined(newfp->slots(), newscript->nfixed);

    // Officially push the frame.
    stack.pushInlineFrame(f.cx, newscript, newfp, &f.regs);
    JS_ASSERT(newfp == f.regs.fp);

    /* Scope with a call object parented by callee's parent. */
    if (call.fun->isHeavyweight() && !js::CreateFunCallObject(f.cx, newfp))
        return false;

    // Try to compile the method if it's not already compiled.
    call.unjittable = false;
    if (newscript->getJITStatus(IsNew) == JITScript_None) {
        CompileStatus status = CanMethodJIT(f.cx, newscript, newfp, CompileRequest_Interpreter);
        if (status == Compile_Error) {
            // A runtime exception was thrown, get out.
            InlineReturn(f);
            return false;
        }
        if (status == Compile_Abort)
            call.unjittable = true;
    }

    // If the script was successfully compiled, we're now done.
    if (JITScript *jit = newscript->getJIT(IsNew)) {
        newfp->setNativeReturnAddress(ic->returnAddress());
        call.code = jit->invokeEntry;
        return true;
    }

    // Otherwise, run the script in the interpreter.
    bool ok = !!Interpret(f.cx, f.fp());
    InlineReturn(f);
    call.code = NULL;

    return ok;
}

template <bool IsNew, bool UpdateIC>
static bool
CallICTail(VMFrame &f, ic::CallIC *ic, CallDescription &call)
{
    JITScript *jit = f.jit();
    (void)jit;

    if (!CallHelper<IsNew>(f, ic, call))
        return false;
    if (UpdateIC) {
        if (!ic->update(f.cx, jit, call))
            return false;
    }
    return true;
}

void * JS_FASTCALL
ic::FailedFunApplyLazyArgs(VMFrame &f, ic::CallIC *ic)
{
    // Reify the lazy argsobj.
    f.regs.sp++;
    if (!js_GetArgsValue(f.cx, f.fp(), &f.regs.sp[-1]))
        THROWV(NULL);
    return ic::FailedFunApply(f, ic);
}

void * JS_FASTCALL
ic::FailedFunApply(VMFrame &f, ic::CallIC *ic)
{
    CallDescription call(2);
    if (!CallICTail<false, false>(f, ic, call))
        THROWV(NULL);
    return call.code;
}

void * JS_FASTCALL
ic::FailedFunCall(VMFrame &f, ic::CallIC *ic)
{
    CallDescription call(ic->frameSize.staticArgc() + 1);
    if (!CallICTail<false, false>(f, ic, call))
        THROWV(NULL);
    return call.code;
}

template <bool DynamicArgc, bool UpdateIC>
void * JS_FASTCALL
ic::Call(VMFrame &f, ic::CallIC *ic)
{
    CallDescription call(DynamicArgc
                         ? f.u.call.dynamicArgc
                         : ic->frameSize.staticArgc());
    if (!CallICTail<false, UpdateIC>(f, ic, call))
        THROWV(NULL);
    return call.code;
}
template void * JS_FASTCALL ic::Call<true, true>(VMFrame &f, ic::CallIC *ic);
template void * JS_FASTCALL ic::Call<true, false>(VMFrame &f, ic::CallIC *ic);
template void * JS_FASTCALL ic::Call<false, true>(VMFrame &f, ic::CallIC *ic);
template void * JS_FASTCALL ic::Call<false, false>(VMFrame &f, ic::CallIC *ic);

template <bool UpdateIC>
void * JS_FASTCALL
ic::New(VMFrame &f, ic::CallIC *ic)
{
    CallDescription call(ic->frameSize.staticArgc());
    if (!CallICTail<true, UpdateIC>(f, ic, call))
        THROWV(NULL);
    return call.code;
}
template void * JS_FASTCALL ic::New<true>(VMFrame &f, ic::CallIC *ic);
template void * JS_FASTCALL ic::New<false>(VMFrame &f, ic::CallIC *ic);

JSOp
ic::CallIC::op() const
{
    return JSOp(*pc);
}

class CallAssembler
{
    JSContext *cx;
    const ic::CallIC &ic;
    JITScript *jit;
    CallDescription &call;
    Registers tempRegs;

    // Guards that always go to the slowest path.
    MaybeJump claspGuard;

    // Must be patched during linking.
    MaybeJump nativeDone;
    MaybeJump takeSlowPath;
    MaybeJump interpDone;

  public:
    Assembler masm;

  public:
    CallAssembler(JSContext *cx, ic::CallIC &ic, JITScript *jit, CallDescription &call)
      : cx(cx),
        ic(ic),
        jit(jit),
        call(call)
    {
        tempRegs.takeReg(ic.calleeData);
    }

    void emitStaticFrame(uint32 frameDepth)
    {
        masm.emitStaticFrame(ic.op() == JSOP_NEW, frameDepth, ic.returnAddress());
    }

    void emitDynamicFrame()
    {
        RegisterID newfp = tempRegs.takeAnyReg();
        masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), newfp);
    
        Address flagsAddr(newfp, JSStackFrame::offsetOfFlags());
        uint32 extraFlags = (ic.op() == JSOP_NEW) ? JSFRAME_CONSTRUCTING : 0;
        masm.store32(Imm32(JSFRAME_FUNCTION | extraFlags), flagsAddr);
    
        Address prevAddr(newfp, JSStackFrame::offsetOfPrev());
        masm.storePtr(JSFrameReg, prevAddr);
    
        Address ncodeAddr(newfp, JSStackFrame::offsetOfncode());
        masm.storePtr(ImmPtr(ic.returnAddress()), ncodeAddr);
    
        masm.move(newfp, JSFrameReg);
        tempRegs.putReg(newfp);
    }

    Jump emitNativeCall(Native native)
    {
        // Call RegExp.test instead of exec if the result will not eb used or
        // will only be used to test for existance.
        if (native == js_regexp_exec && !CallResultEscapes(cx->regs->pc))
            native = js_regexp_test;

        Registers tempRegs; // note, separate from member variable for convenience.
#ifndef JS_CPU_X86
        tempRegs.takeReg(Registers::ArgReg0);
        tempRegs.takeReg(Registers::ArgReg1);
        tempRegs.takeReg(Registers::ArgReg2);
#endif

        // Store the PC.
        masm.storePtr(ImmPtr(ic.pc), FrameAddress(offsetof(VMFrame, regs.pc)));

        // Store |sp| (SplatApplyArgs has already been called so that's not
        // needed here).
        RegisterID t0 = tempRegs.takeAnyReg();
        if (ic.frameSize.isStatic()) {
            uint32 spOffset = sizeof(JSStackFrame) + ic.frameSize.staticLocalSlots() * sizeof(Value);
            masm.addPtr(Imm32(spOffset), JSFrameReg, t0);
            masm.storePtr(t0, FrameAddress(offsetof(VMFrame, regs.sp)));
        }

        // Store the frame pointer.
        masm.storePtr(JSFrameReg, FrameAddress(offsetof(VMFrame, regs.fp)));

        // Grab cx.
#ifdef JS_CPU_X86
        RegisterID cxReg = tempRegs.takeAnyReg();
#else
        RegisterID cxReg = Registers::ArgReg0;
#endif
        masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), cxReg);

        // Compute vp.
#ifdef JS_CPU_X86
        RegisterID argcReg = tempRegs.takeAnyReg();
        RegisterID vpReg = t0;
#else
        RegisterID argcReg = Registers::ArgReg1;
        RegisterID vpReg = Registers::ArgReg2;
#endif
        if (ic.frameSize.isStatic()) {
            uint32 vpOffset = ic.frameSize.staticLocalSlots() - (ic.frameSize.staticArgc() + 2);
            masm.addPtr(Imm32(sizeof(JSStackFrame) + vpOffset * sizeof(Value)), JSFrameReg, vpReg);
        } else {
            masm.load32(FrameAddress(offsetof(VMFrame, u.call.dynamicArgc)), argcReg);
            masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), vpReg);

            // vpOff = (argc + 2) * sizeof(Value)
            RegisterID vpOff = tempRegs.takeAnyReg();
            masm.move(argcReg, vpOff);
            masm.add32(Imm32(2), vpOff); // callee, this
            JS_STATIC_ASSERT(sizeof(Value) == 8);
            masm.lshift32(Imm32(3), vpOff);
            masm.subPtr(vpOff, vpReg);
            tempRegs.putReg(vpOff);
        }

        // Mark vp[1] as magic for |new|.
        if (ic.op() == JSOP_NEW) {
            Value v;
            v.setMagicWithObjectOrNullPayload(NULL);
            masm.storeValue(v, Address(vpReg, sizeof(Value)));
        }

        masm.setupABICall(Registers::NormalCall, 3);
        masm.storeArg(2, vpReg);
        if (ic.frameSize.isStatic())
            masm.storeArg(1, Imm32(ic.frameSize.staticArgc()));
        else
            masm.storeArg(1, argcReg);
        masm.storeArg(0, cxReg);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, native), false);

        Jump hasException = masm.branchTest32(Assembler::Zero, Registers::ReturnReg,
                                              Registers::ReturnReg);

        Address rval(JSFrameReg, ic.rvalOffset);
        masm.loadValueAsComponents(rval, JSReturnReg_Type, JSReturnReg_Data);
        Jump done = masm.jump();

        hasException.linkTo(masm.label(), &masm);
        masm.throwInJIT();

        return done;
    }

    void emitLoadArgc(RegisterID reg)
    {
        if (ic.frameSize.isStatic())
            masm.move(Imm32(ic.frameSize.staticArgc()), reg);
        else
            masm.load32(FrameAddress(offsetof(VMFrame, u.call.dynamicArgc)), reg);
    }

    void emitGeneric()
    {
        RegisterID tmp = tempRegs.takeAnyReg();

        // ic.calleeData is definitely a funobj at this point.
        masm.loadObjPrivate(ic.calleeData, ic.calleeData);
        masm.load16(Address(ic.calleeData, offsetof(JSFunction, flags)), tmp);
        masm.and32(Imm32(JSFUN_KINDMASK), tmp);

        takeSlowPath = masm.branch32(Assembler::Below, tmp, Imm32(JSFUN_INTERPRETED));
        if (ic.frameSize.isStatic())
            emitStaticFrame(ic.frameSize.staticLocalSlots());
        else
            emitDynamicFrame();

        Address scriptAddr(ic.calleeData, offsetof(JSFunction, u.i.script));
        masm.loadPtr(scriptAddr, tmp);

        // Test if the script has JIT code.
        size_t offset = (ic.op() == JSOP_NEW)
                        ? offsetof(JSScript, jitArityCheckCtor)
                        : offsetof(JSScript, jitArityCheckNormal);
        masm.loadPtr(Address(tmp, offset), Registers::ReturnReg);
        Jump hasCode = masm.branchPtr(Assembler::Above, Registers::ReturnReg,
                                      ImmPtr(JS_UNJITTABLE_SCRIPT));

        // Try and compile. On success we get back the nmap pointer.
        masm.storePtr(JSFrameReg, FrameAddress(offsetof(VMFrame, regs.fp)));
        emitLoadArgc(Registers::ArgReg1);

        void *ptr = JS_FUNC_TO_DATA_PTR(void *, stubs::CompileFunction);
        if (ic.frameSize.isStatic())
            masm.fallibleVMCall(ptr, NULL, ic.frameSize.staticLocalSlots());
        else
            masm.fallibleVMCall(ptr, NULL, -1);

        // Must reload JSFrameReg since CompileFunction could re-adjust it.
        masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);

        // NULL return means Interpret() was called, and this will jump to the
        // "completed call" join point. Otherwise, we jump directly to JIT
        // code: the frame is setup and ncode (the "inline call" join point)
        // has been stored one way or another.
        interpDone = masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                        Registers::ReturnReg);

        masm.jump(Registers::ReturnReg);

        hasCode.linkTo(masm.label(), &masm);
        emitLoadArgc(JSParamReg_Argc);
        masm.jump(Registers::ReturnReg);
    }

    bool link(Repatcher &repatcher)
    {
        JS_ASSERT(!ic.pool);
        JS_ASSERT(!ic.guardedNative);

        LinkerHelper linker(masm);
        JSC::ExecutablePool *ep = linker.init(cx);
        if (!ep)
            return false;
        if (!linker.verifyRange(jit)) {
            ep->release();
            return true;
        }

        ic.pool = ep;
        linker.link(claspGuard.get(), ic.slowPathStart);
        linker.link(takeSlowPath.get(), ic.slowPathStart);
        linker.link(interpDone.get(), ic.completedRejoinLabel());

        if (nativeDone.isSet()) {
            // Note: jump to the "completed call" finish point, not the inline
            // call finish point.
            linker.link(nativeDone.get(), ic.completedRejoinLabel());
            ic.guardedNative = call.callee;
        }

        JSC::CodeLocationLabel cs = linker.finalize();
        JaegerSpew(JSpew_PICs, "generated call IC at %p\n", cs.executableAddress());

        // Finally, patch the inline path.
        repatcher.relink(ic.inlineJump(), cs);
        return true;
    }

  public:
    bool assemble(Repatcher &repatcher)
    {
        // Call ICs generate one or more fast-paths. The fast paths fall into
        // the following categories:
        //
        // Monomorphic scripted function-object, with matching args:
        //   After a type check and callee guard, frame creation is completely
        //   inlined, as is the function invocaton. If the callee guard fails,
        //   there is an additional path that tries to unravel the funobj to
        //   see if its internal JSFunction matches.
        //
        // Monomorphic native function-object:
        //   After a type check and callee guard, native invocation is
        //   completely inlined.
        //
        // If these guards fail, a slow path is taken instead. The slow path
        // checks whether the callee is a function or not, and then, whether it
        // is scripted or native. If not a function, it jumps back to the OOL
        // path. If scripted, it attempts an inline call (and may try to compile
        // an uncompiled function). If native, it goes to the slow path, but
        // it would not be difficult to invoke a generic native handler using
        // the same emitNativeCall() routine.
 
        // If there's a native, generate a specialized path to call it.
        MaybeJump calleeGuard;
        if (call.fun->isNative()) {
            calleeGuard = masm.branchPtr(Assembler::NotEqual, ic.calleeData, ImmPtr(call.callee));
            nativeDone = emitNativeCall(call.fun->u.n.native);
        }
    
        // Emit a general call path. We reach this if all callee guards fail, or
        // if the frame size is dynamic (fun.apply speculation), this path is
        // always taken. Note that the clasp guard might already have been
        // emitted.
        if (calleeGuard.isSet())
            calleeGuard.get().linkTo(masm.label(), &masm);
        claspGuard = masm.testFunction(Assembler::NotEqual, ic.calleeData);

        emitGeneric();

        return link(repatcher);
    }
};

typedef void * (JS_FASTCALL *CallICFun)(VMFrame &, ic::CallIC *);
template <bool UpdateIC>
CallICFun GetCallICFun(JSOp op, const FrameSize &frameSize)
{
    if (op == JSOP_NEW)
        return ic::New<UpdateIC>;
    if (frameSize.isStatic())
        return ic::Call<false, UpdateIC>;
    return ic::Call<true, UpdateIC>;
}

void
CallIC::disable(Repatcher &repatcher)
{
    CallICFun fun = GetCallICFun<false>(op(), frameSize);
    repatcher.relink(slowCall(), JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, fun)));
}

void
CallIC::reenable(JITScript *jit, Repatcher &repatcher)
{
    // Never re-enable if in debug mode.
    if (jit->script->debugMode)
        return;
    CallICFun fun = GetCallICFun<true>(op(), frameSize);
    repatcher.relink(slowCall(), JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, fun)));
}

void
CallIC::purgeInlineGuard(Repatcher &repatcher)
{
    JS_ASSERT(hasExtendedInlinePath);
    repatcher.repatch(calleePtr(), NULL);
    inlineCallee = NULL;
}

void
CallIC::purgeStub(Repatcher &repatcher)
{
    repatcher.relink(inlineJump(), slowPathStart);
    pool->release();
    pool = NULL;
    guardedNative = NULL;
}

void
CallIC::purge(JITScript *jit, Repatcher &repatcher)
{
    reenable(jit, repatcher);
    if (inlineCallee)
        purgeInlineGuard(repatcher);
    if (pool)
        purgeStub(repatcher);
}

bool
CallIC::shouldDisable(JSContext *cx, JITScript *jit, CallDescription &call)
{
    // Callee is not a funobj, don't bother caching.
    if (!call.fun)
        return true;

    // If interpreted and unjittable, also don't bother caching.
    if (call.fun->isInterpreted() && call.unjittable)
        return true;

    // CallICs do not support debug mode yet.
    JS_ASSERT(!jit->script->debugMode);
    return false;
}

bool
CallIC::update(JSContext *cx, JITScript *jit, CallDescription &call)
{
    if (shouldDisable(cx, jit, call)) {
        Repatcher repatcher(jit);
        disable(repatcher);
        return true;
    }

    // If the callee is interpreted, but doesn't yet have JIT code, delay
    // building the IC until it has become hot enough to compile.
    if (call.fun->isInterpreted() && !call.code) {
        // This should have been caught earlier.
        JS_ASSERT(!call.unjittable);
        return true;
    }

    // Disable the IC builder now that we'll be attaching one IC.
    Repatcher repatcher(jit);
    disable(repatcher);

    // At this point, the IC is probably incomplete in one of two ways:
    // (1) The inline path is not patched (either unhit or GC'd), or
    // (2) There is no generated stub (either unhit or GC'd).

    // Try to patch the inline path, if we can.
    if (call.fun->isInterpreted() &&
        hasExtendedInlinePath &&
        frameSize.staticArgc() == call.fun->nargs &&
        !inlineCallee)
    {
        JS_ASSERT(cx->fp()->jit() == call.fun->script()->getJIT(op() == JSOP_NEW));

        JSC::CodeLocationLabel target(cx->fp()->jit()->fastEntry);
        repatcher.relink(inlineCall(), target);
        repatcher.repatch(calleePtr(), call.callee);
        inlineCallee = call.callee;
    }

    // Always generate a stub.
    if (!pool) {
        CallAssembler ca(cx, *this, jit, call);
        return ca.assemble(repatcher);
    }
    return true;
}

void
JITScript::purgeMICs()
{
    if (!nGetGlobalNames || !nSetGlobalNames)
        return;

    Repatcher repatch(this);

    ic::GetGlobalNameIC *getGlobalNames_ = getGlobalNames();
    for (uint32 i = 0; i < nGetGlobalNames; i++) {
        ic::GetGlobalNameIC &ic = getGlobalNames_[i];
        JSC::CodeLocationDataLabel32 label = ic.fastPathStart.dataLabel32AtOffset(ic.shapeOffset);
        repatch.repatch(label, int(INVALID_SHAPE));
    }

    ic::SetGlobalNameIC *setGlobalNames_ = setGlobalNames();
    for (uint32 i = 0; i < nSetGlobalNames; i++) {
        ic::SetGlobalNameIC &ic = setGlobalNames_[i];
        ic.patchInlineShapeGuard(repatch, int32(INVALID_SHAPE));

        if (ic.hasExtraStub) {
            Repatcher repatcher(ic.extraStub);
            ic.patchExtraShapeGuard(repatcher, int32(INVALID_SHAPE));
        }
    }
}

void
ic::PurgeMICs(JSContext *cx, JSScript *script)
{
    /* MICs are purged during GC to handle changing shapes. */
    JS_ASSERT(cx->runtime->gcRegenShapes);

    if (script->jitNormal)
        script->jitNormal->purgeMICs();
    if (script->jitCtor)
        script->jitCtor->purgeMICs();
}

void
JITScript::sweepICs(JSContext *cx)
{
    Repatcher repatcher(this);

    ic::CallIC *callICs_ = callICs();
    for (uint32 i = 0; i < nCallICs; i++) {
        ic::CallIC &ic = callICs_[i];
        bool killedInline = ic.inlineCallee && IsAboutToBeFinalized(cx, ic.inlineCallee);
        bool killedNative = ic.guardedNative && IsAboutToBeFinalized(cx, ic.guardedNative);
        if (killedInline)
            ic.purgeInlineGuard(repatcher);
        if (killedNative)
            ic.purgeStub(repatcher);
        if (killedInline || killedNative)
            ic.reenable(this, repatcher);
    }
}

void
JITScript::purgeICs(JSContext *cx)
{
    Repatcher repatcher(this);

    /*
     * If purgeAll is set, purge stubs in the script except those covered by PurgePICs
     * (which is always called during GC). We want to remove references which can keep
     * alive pools that we are trying to destroy (see JSCompartment::sweep).
     */

    uint32 released = 0;

    ic::CallIC *callICs_ = callICs();
    for (uint32 i = 0; i < nCallICs; i++)
        callICs_[i].purge(this, repatcher);

    ic::EqualityICInfo *equalityICs_ = equalityICs();
    for (uint32 i = 0; i < nEqualityICs; i++) {
        ic::EqualityICInfo &ic = equalityICs_[i];
        if (!ic.generated)
            continue;

        JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, ic::Equality));
        repatcher.relink(ic.stubCall, fptr);
        repatcher.relink(ic.jumpToStub, ic.stubEntry);

        ic.generated = false;
        released++;
    }

    ic::SetGlobalNameIC *setGlobalNames_ = setGlobalNames();
    for (uint32 i = 0; i < nSetGlobalNames; i ++) {
        ic::SetGlobalNameIC &ic = setGlobalNames_[i];
        if (!ic.hasExtraStub)
            continue;
        repatcher.relink(ic.fastPathStart.jumpAtOffset(ic.inlineShapeJump), ic.slowPathStart);
        ic.hasExtraStub = false;
        released++;
    }

    JS_ASSERT(released == execPools.length());
    for (uint32 i = 0; i < released; i++)
        execPools[i]->release();
    execPools.clear();
}

void
ic::SweepICs(JSContext *cx, JSScript *script)
{
    if (script->jitNormal)
        script->jitNormal->sweepICs(cx);
    if (script->jitCtor)
        script->jitCtor->sweepICs(cx);
}

void
ic::PurgeICs(JSContext *cx, JSScript *script)
{
    if (script->jitNormal)
        script->jitNormal->purgeICs(cx);
    if (script->jitCtor)
        script->jitCtor->purgeICs(cx);
}

#endif /* JS_MONOIC */

