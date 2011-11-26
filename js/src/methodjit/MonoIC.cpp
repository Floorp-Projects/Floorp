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
#include "methodjit/PolyIC.h"
#include "InlineFrameAssembler.h"
#include "jsobj.h"

#include "builtin/RegExp.h"

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
    JSAtom *atom = f.script()->getAtom(GET_INDEX(f.pc()));
    jsid id = ATOM_TO_JSID(atom);

    const Shape *shape = obj->nativeLookup(f.cx, id);
    if (!shape ||
        !shape->hasDefaultGetterOrIsMethod() ||
        !shape->hasSlot())
    {
        if (shape)
            PatchGetFallback(f, ic);
        stubs::GetGlobalName(f);
        return;
    }
    uint32 slot = shape->slot();

    /* Patch shape guard. */
    Repatcher repatcher(f.jit());
    repatcher.repatch(ic->fastPathStart.dataLabelPtrAtOffset(ic->shapeOffset), obj->lastProperty());

    /* Patch loads. */
    uint32 index = obj->dynamicSlotIndex(slot);
    JSC::CodeLocationLabel label = ic->fastPathStart.labelAtOffset(ic->loadStoreOffset);
    repatcher.patchAddressOffsetForValueLoad(label, index * sizeof(Value));

    /* Do load anyway... this time. */
    stubs::GetGlobalName(f);
}

template <JSBool strict>
static void JS_FASTCALL
DisabledSetGlobal(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.pc()));
    stubs::SetGlobalName<strict>(f, atom);
}

template void JS_FASTCALL DisabledSetGlobal<true>(VMFrame &f, ic::SetGlobalNameIC *ic);
template void JS_FASTCALL DisabledSetGlobal<false>(VMFrame &f, ic::SetGlobalNameIC *ic);

template <JSBool strict>
static void JS_FASTCALL
DisabledSetGlobalNoCache(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.pc()));
    stubs::SetGlobalNameNoCache<strict>(f, atom);
}

template void JS_FASTCALL DisabledSetGlobalNoCache<true>(VMFrame &f, ic::SetGlobalNameIC *ic);
template void JS_FASTCALL DisabledSetGlobalNoCache<false>(VMFrame &f, ic::SetGlobalNameIC *ic);

static void
PatchSetFallback(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSScript *script = f.script();
    Repatcher repatch(f.jit());
    VoidStubSetGlobal stub = ic->usePropertyCache
                             ? STRICT_VARIANT(DisabledSetGlobal)
                             : STRICT_VARIANT(DisabledSetGlobalNoCache);
    JSC::FunctionPtr fptr(JS_FUNC_TO_DATA_PTR(void *, stub));
    repatch.relink(ic->slowPathCall, fptr);
}

void
SetGlobalNameIC::patchExtraShapeGuard(Repatcher &repatcher, const Shape *shape)
{
    JS_ASSERT(hasExtraStub);

    JSC::CodeLocationLabel label(JSC::MacroAssemblerCodePtr(extraStub.start()));
    repatcher.repatch(label.dataLabelPtrAtOffset(extraShapeGuard), shape);
}

void
SetGlobalNameIC::patchInlineShapeGuard(Repatcher &repatcher, const Shape *shape)
{
    JSC::CodeLocationDataLabelPtr label = fastPathStart.dataLabelPtrAtOffset(shapeOffset);
    repatcher.repatch(label, shape);
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
        !shape->hasSlot() ||
        obj->watched())
    {
        /* Disable the IC for weird shape attributes and watchpoints. */
        PatchSetFallback(f, ic);
        return Lookup_Uncacheable;
    }

    /* Object is not branded, so we can use the inline path. */
    Repatcher repatcher(f.jit());
    ic->patchInlineShapeGuard(repatcher, obj->lastProperty());

    uint32 index = obj->dynamicSlotIndex(shape->slot());
    JSC::CodeLocationLabel label = ic->fastPathStart.labelAtOffset(ic->loadStoreOffset);
    repatcher.patchAddressOffsetForValueStore(label, index * sizeof(Value),
                                              ic->vr.isTypeKnown());

    return Lookup_Cacheable;
}

void JS_FASTCALL
ic::SetGlobalName(VMFrame &f, ic::SetGlobalNameIC *ic)
{
    JSObject *obj = f.fp()->scopeChain().getGlobal();
    JSScript *script = f.script();
    JSAtom *atom = script->getAtom(GET_INDEX(f.pc()));
    const Shape *shape = obj->nativeLookup(f.cx, ATOM_TO_JSID(atom));

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
        : LinkerHelper(masm, JSC::METHOD_CODE), f(f)
    { }

    bool init(JSContext *cx) {
        JSC::ExecutablePool *pool = LinkerHelper::init(cx);
        if (!pool)
            return false;
        JS_ASSERT(!f.regs.inlined());
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

        masm.loadObjClass(lvr.dataReg(), ic.tempReg);
        Jump lhsHasEq = masm.branchPtr(Assembler::NotEqual,
                                       Address(ic.tempReg, offsetof(Class, ext.equality)),
                                       ImmPtr(NULL));
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

        CodeLocationLabel cs = buffer.finalize(f);

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

static void * JS_FASTCALL
SlowCallFromIC(VMFrame &f, ic::CallICInfo *ic)
{
    stubs::SlowCall(f, ic->frameSize.getArgc(f));
    return NULL;
}

static void * JS_FASTCALL
SlowNewFromIC(VMFrame &f, ic::CallICInfo *ic)
{
    stubs::SlowNew(f, ic->frameSize.staticArgc());
    return NULL;
}

bool
NativeStubLinker::init(JSContext *cx)
{
    JSC::ExecutablePool *pool = LinkerHelper::init(cx);
    if (!pool)
        return false;

    NativeCallStub stub;
    stub.pc = pc;
    stub.pool = pool;
    stub.jump = locationOf(done);
    if (!jit->nativeCallStubs.append(stub)) {
        pool->release();
        return false;
    }

    return true;
}

/*
 * Generate epilogue code to run after a stub ABI call to a native or getter.
 * This checks for an exception, and either type checks the result against the
 * observed types for the opcode or loads the result into a register pair
 * (it will go through a type barrier afterwards).
 */
bool
mjit::NativeStubEpilogue(VMFrame &f, Assembler &masm, NativeStubLinker::FinalJump *result,
                         int32 initialFrameDepth, int32 vpOffset,
                         MaybeRegisterID typeReg, MaybeRegisterID dataReg)
{
    /* Reload fp, which may have been clobbered by restoreStackBase(). */
    masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);

    Jump hasException = masm.branchTest32(Assembler::Zero, Registers::ReturnReg,
                                          Registers::ReturnReg);

    Address resultAddress(JSFrameReg, vpOffset);

    Vector<Jump> mismatches(f.cx);
    if (f.cx->typeInferenceEnabled() && !typeReg.isSet()) {
        /*
         * Test the result of this native against the known result type set for
         * the call. We don't assume knowledge about the types that natives can
         * return, except when generating specialized paths in FastBuiltins.
         */
        types::TypeSet *types = f.script()->analysis()->bytecodeTypes(f.pc());
        if (!masm.generateTypeCheck(f.cx, resultAddress, types, &mismatches))
            THROWV(false);
    }

    /*
     * Can no longer trigger recompilation in this stub, clear the stub rejoin
     * on the VMFrame.
     */
    masm.storePtr(ImmPtr(NULL), FrameAddress(offsetof(VMFrame, stubRejoin)));

    if (typeReg.isSet())
        masm.loadValueAsComponents(resultAddress, typeReg.reg(), dataReg.reg());

    /*
     * The final jump is a indirect on x64, so that we'll always be able
     * to repatch it to the interpoline later.
     */
    Label finished = masm.label();
#ifdef JS_CPU_X64
    JSC::MacroAssembler::DataLabelPtr done = masm.moveWithPatch(ImmPtr(NULL), Registers::ValueReg);
    masm.jump(Registers::ValueReg);
#else
    Jump done = masm.jump();
#endif

    /* Generate a call for type check failures on the native result. */
    if (!mismatches.empty()) {
        for (unsigned i = 0; i < mismatches.length(); i++)
            mismatches[i].linkTo(masm.label(), &masm);
        masm.addPtr(Imm32(vpOffset), JSFrameReg, Registers::ArgReg1);
        masm.fallibleVMCall(true, JS_FUNC_TO_DATA_PTR(void *, stubs::TypeBarrierReturn),
                            f.regs.pc, NULL, initialFrameDepth);
        masm.storePtr(ImmPtr(NULL), FrameAddress(offsetof(VMFrame, stubRejoin)));
        masm.jump().linkTo(finished, &masm);
    }

    /* Move JaegerThrowpoline into register for very far jump on x64. */
    hasException.linkTo(masm.label(), &masm);
    masm.storePtr(ImmPtr(NULL), FrameAddress(offsetof(VMFrame, stubRejoin)));
    masm.throwInJIT();

    *result = done;
    return true;
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
class CallCompiler : public BaseCompiler
{
    VMFrame &f;
    CallICInfo &ic;
    bool callingNew;

  public:
    CallCompiler(VMFrame &f, CallICInfo &ic, bool callingNew)
      : BaseCompiler(f.cx), f(f), ic(ic), callingNew(callingNew)
    {
    }

    JSC::ExecutablePool *poolForSize(LinkerHelper &linker, CallICInfo::PoolIndex index)
    {
        JSC::ExecutablePool *ep = linker.init(f.cx);
        if (!ep)
            return NULL;
        JS_ASSERT(!ic.pools[index]);
        ic.pools[index] = ep;
        return ep;
    }

    void disable(JITScript *jit)
    {
        JSC::CodeLocationCall oolCall = ic.slowPathStart.callAtOffset(ic.oolCallOffset);
        Repatcher repatch(jit);
        JSC::FunctionPtr fptr = callingNew
                                ? JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, SlowNewFromIC))
                                : JSC::FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, SlowCallFromIC));
        repatch.relink(oolCall, fptr);
    }

    bool generateFullCallStub(JITScript *from, JSScript *script, uint32 flags)
    {
        /*
         * Create a stub that works with arity mismatches. Like the fast-path,
         * this allocates a frame on the caller side, but also performs extra
         * checks for compilability. Perhaps this should be a separate, shared
         * trampoline, but for now we generate it dynamically.
         */
        Assembler masm;
        InlineFrameAssembler inlFrame(masm, ic, flags);
        RegisterID t0 = inlFrame.tempRegs.takeAnyReg().reg();

        /* Generate the inline frame creation. */
        void *ncode = ic.funGuard.labelAtOffset(ic.joinPointOffset).executableAddress();
        inlFrame.assemble(ncode, f.pc());

        /* funObjReg is still valid. Check if a compilation is needed. */
        Address scriptAddr(ic.funObjReg, JSFunction::offsetOfNativeOrScript());
        masm.loadPtr(scriptAddr, t0);

        /*
         * Test if script->nmap is NULL - same as checking ncode, but faster
         * here since ncode has two failure modes and we need to load out of
         * nmap anyway.
         */
        size_t offset = callingNew
                        ? offsetof(JSScript, jitArityCheckCtor)
                        : offsetof(JSScript, jitArityCheckNormal);
        masm.loadPtr(Address(t0, offset), t0);
        Jump hasCode = masm.branchPtr(Assembler::Above, t0, ImmPtr(JS_UNJITTABLE_SCRIPT));

        /*
         * Write the rejoin state to indicate this is a compilation call made
         * from an IC (the recompiler cannot detect calls made from ICs
         * automatically).
         */
        masm.storePtr(ImmPtr((void *) ic.frameSize.rejoinState(f.pc(), false)),
                      FrameAddress(offsetof(VMFrame, stubRejoin)));

        masm.bumpStubCounter(f.script(), f.pc(), Registers::tempCallReg());

        /* Try and compile. On success we get back the nmap pointer. */
        void *compilePtr = JS_FUNC_TO_DATA_PTR(void *, stubs::CompileFunction);
        DataLabelPtr inlined;
        if (ic.frameSize.isStatic()) {
            masm.move(Imm32(ic.frameSize.staticArgc()), Registers::ArgReg1);
            masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                compilePtr, f.regs.pc, &inlined, ic.frameSize.staticLocalSlots());
        } else {
            masm.load32(FrameAddress(VMFrame::offsetOfDynamicArgc()), Registers::ArgReg1);
            masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                compilePtr, f.regs.pc, &inlined, -1);
        }

        Jump notCompiled = masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                              Registers::ReturnReg);
        masm.loadPtr(FrameAddress(VMFrame::offsetOfRegsSp()), JSFrameReg);

        /* Compute the value of ncode to use at this call site. */
        ncode = (uint8 *) f.jit()->code.m_code.executableAddress() + ic.call->codeOffset;
        masm.storePtr(ImmPtr(ncode), Address(JSFrameReg, StackFrame::offsetOfNcode()));

        masm.jump(Registers::ReturnReg);

        hasCode.linkTo(masm.label(), &masm);

        /* Get nmap[ARITY], set argc, call. */
        if (ic.frameSize.isStatic())
            masm.move(Imm32(ic.frameSize.staticArgc()), JSParamReg_Argc);
        else
            masm.load32(FrameAddress(VMFrame::offsetOfDynamicArgc()), JSParamReg_Argc);
        masm.jump(t0);

        LinkerHelper linker(masm, JSC::METHOD_CODE);
        JSC::ExecutablePool *ep = poolForSize(linker, CallICInfo::Pool_ScriptStub);
        if (!ep)
            return false;

        if (!linker.verifyRange(from)) {
            disable(from);
            return true;
        }

        linker.link(notCompiled, ic.slowPathStart.labelAtOffset(ic.slowJoinOffset));
        JSC::CodeLocationLabel cs = linker.finalize(f);

        JaegerSpew(JSpew_PICs, "generated CALL stub %p (%lu bytes)\n", cs.executableAddress(),
                   (unsigned long) masm.size());

        if (f.regs.inlined()) {
            JSC::LinkBuffer code((uint8 *) cs.executableAddress(), masm.size(), JSC::METHOD_CODE);
            code.patch(inlined, f.regs.inlined());
        }

        Repatcher repatch(from);
        JSC::CodeLocationJump oolJump = ic.slowPathStart.jumpAtOffset(ic.oolJumpOffset);
        repatch.relink(oolJump, cs);

        return true;
    }

    bool patchInlinePath(JITScript *from, JSScript *script, JSObject *obj)
    {
        JS_ASSERT(ic.frameSize.isStatic());
        JITScript *jit = script->getJIT(callingNew);

        /* Very fast path. */
        Repatcher repatch(from);

        /*
         * Use the arguments check entry if this is a monitored call, we might
         * not have accounted for all possible argument types.
         */
        void *entry = ic.typeMonitored ? jit->argsCheckEntry : jit->fastEntry;

        if (!repatch.canRelink(ic.funGuard.jumpAtOffset(ic.hotJumpOffset),
                               JSC::CodeLocationLabel(entry))) {
            return false;
        }

        ic.fastGuardedObject = obj;
        JS_APPEND_LINK(&ic.links, &jit->callers);

        repatch.repatch(ic.funGuard, obj);
        repatch.relink(ic.funGuard.jumpAtOffset(ic.hotJumpOffset),
                       JSC::CodeLocationLabel(entry));

        JaegerSpew(JSpew_PICs, "patched CALL path %p (obj: %p)\n",
                   ic.funGuard.executableAddress(),
                   static_cast<void*>(ic.fastGuardedObject));

        return true;
    }

    bool generateStubForClosures(JITScript *from, JSObject *obj)
    {
        JS_ASSERT(ic.frameSize.isStatic());

        /* Slightly less fast path - guard on fun->script() instead. */
        Assembler masm;

        Registers tempRegs(Registers::AvailRegs);
        tempRegs.takeReg(ic.funObjReg);

        RegisterID t0 = tempRegs.takeAnyReg().reg();

        /* Guard that it's actually a function object. */
        Jump claspGuard = masm.testObjClass(Assembler::NotEqual, ic.funObjReg, t0, &FunctionClass);

        /* Guard that it's the same script. */
        Address scriptAddr(ic.funObjReg, JSFunction::offsetOfNativeOrScript());
        Jump funGuard = masm.branchPtr(Assembler::NotEqual, scriptAddr,
                                       ImmPtr(obj->toFunction()->script()));
        Jump done = masm.jump();

        LinkerHelper linker(masm, JSC::METHOD_CODE);
        JSC::ExecutablePool *ep = poolForSize(linker, CallICInfo::Pool_ClosureStub);
        if (!ep)
            return false;

        ic.hasJsFunCheck = true;

        if (!linker.verifyRange(from)) {
            disable(from);
            return true;
        }

        linker.link(claspGuard, ic.slowPathStart);
        linker.link(funGuard, ic.slowPathStart);
        linker.link(done, ic.funGuard.labelAtOffset(ic.hotPathOffset));
        JSC::CodeLocationLabel cs = linker.finalize(f);

        JaegerSpew(JSpew_PICs, "generated CALL closure stub %p (%lu bytes)\n",
                   cs.executableAddress(), (unsigned long) masm.size());

        Repatcher repatch(from);
        repatch.relink(ic.funJump, cs);

        return true;
    }

    bool generateNativeStub()
    {
        JITScript *jit = f.jit();

        /* Snapshot the frameDepth before SplatApplyArgs modifies it. */
        uintN initialFrameDepth = f.regs.sp - f.fp()->slots();

        /*
         * SplatApplyArgs has not been called, so we call it here before
         * potentially touching f.u.call.dynamicArgc.
         */
        CallArgs args;
        if (ic.frameSize.isStatic()) {
            JS_ASSERT(f.regs.sp - f.fp()->slots() == (int)ic.frameSize.staticLocalSlots());
            args = CallArgsFromSp(ic.frameSize.staticArgc(), f.regs.sp);
        } else {
            JS_ASSERT(!f.regs.inlined());
            JS_ASSERT(*f.regs.pc == JSOP_FUNAPPLY && GET_ARGC(f.regs.pc) == 2);
            if (!ic::SplatApplyArgs(f))       /* updates regs.sp */
                THROWV(true);
            args = CallArgsFromSp(f.u.call.dynamicArgc, f.regs.sp);
        }

        JSFunction *fun;
        if (!IsFunctionObject(args.calleev(), &fun))
            return false;

        if ((!callingNew && !fun->isNative()) || (callingNew && !fun->isConstructor()))
            return false;

        if (callingNew)
            args.thisv().setMagicWithObjectOrNullPayload(NULL);

        RecompilationMonitor monitor(cx);

        if (!CallJSNative(cx, fun->u.n.native, args))
            THROWV(true);

        types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());

        /*
         * Native stubs are not generated for inline frames. The overhead of
         * bailing out from the IC is far greater than the time saved by
         * inlining the parent frame in the first place, so mark the immediate
         * caller as uninlineable.
         */
        if (f.script()->function()) {
            f.script()->uninlineable = true;
            MarkTypeObjectFlags(cx, f.script()->function(), types::OBJECT_FLAG_UNINLINEABLE);
        }

        /* Don't touch the IC if the call triggered a recompilation. */
        if (monitor.recompiled())
            return true;

        JS_ASSERT(!f.regs.inlined());

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
        Jump funGuard = masm.branchPtr(Assembler::NotEqual, ic.funObjReg, ImmPtr(fun));

        /*
         * Write the rejoin state for the recompiler to use if this call
         * triggers recompilation. Natives use a different stack address to
         * store the return value than FASTCALLs, and without additional
         * information we cannot tell which one is active on a VMFrame.
         */
        masm.storePtr(ImmPtr((void *) ic.frameSize.rejoinState(f.pc(), true)),
                      FrameAddress(offsetof(VMFrame, stubRejoin)));

        /* N.B. After this call, the frame will have a dynamic frame size. */
        if (ic.frameSize.isDynamic()) {
            masm.bumpStubCounter(f.script(), f.pc(), Registers::tempCallReg());
            masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                JS_FUNC_TO_DATA_PTR(void *, ic::SplatApplyArgs),
                                f.regs.pc, NULL, initialFrameDepth);
        }

        Registers tempRegs = Registers::tempCallRegMask();
        RegisterID t0 = tempRegs.takeAnyReg().reg();
        masm.bumpStubCounter(f.script(), f.pc(), t0);

        int32 storeFrameDepth = ic.frameSize.isStatic() ? initialFrameDepth : -1;
        masm.setupFallibleABICall(cx->typeInferenceEnabled(), f.regs.pc, storeFrameDepth);

        /* Grab cx. */
#ifdef JS_CPU_X86
        RegisterID cxReg = tempRegs.takeAnyReg().reg();
#else
        RegisterID cxReg = Registers::ArgReg0;
#endif
        masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), cxReg);

        /*
         * Compute vp. This will always be at the same offset from fp for a
         * given callsite, regardless of any dynamically computed argc,
         * so get that offset from the active call.
         */
#ifdef JS_CPU_X86
        RegisterID vpReg = t0;
#else
        RegisterID vpReg = Registers::ArgReg2;
#endif
        uint32 vpOffset = (uint32) ((char *) args.base() - (char *) f.fp());
        masm.addPtr(Imm32(vpOffset), JSFrameReg, vpReg);

        /* Compute argc. */
        MaybeRegisterID argcReg;
        if (!ic.frameSize.isStatic()) {
            argcReg = tempRegs.takeAnyReg().reg();
            masm.load32(FrameAddress(VMFrame::offsetOfDynamicArgc()), argcReg.reg());
        }

        /* Mark vp[1] as magic for |new|. */
        if (callingNew) {
            Value v;
            v.setMagicWithObjectOrNullPayload(NULL);
            masm.storeValue(v, Address(vpReg, sizeof(Value)));
        }

        masm.restoreStackBase();
        masm.setupABICall(Registers::NormalCall, 3);
        masm.storeArg(2, vpReg);
        if (ic.frameSize.isStatic())
            masm.storeArg(1, ImmIntPtr(intptr_t(ic.frameSize.staticArgc())));
        else
            masm.storeArg(1, argcReg.reg());
        masm.storeArg(0, cxReg);

        js::Native native = fun->u.n.native;

        /*
         * Call RegExp.test instead of exec if the result will not be used or
         * will only be used to test for existence. Note that this will not
         * break inferred types for the call's result and any subsequent test,
         * as RegExp.exec has a type handler with unknown result.
         */
        if (native == regexp_exec && !CallResultEscapes(f.pc()))
            native = regexp_test;

        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, native), false);

        NativeStubLinker::FinalJump done;
        if (!NativeStubEpilogue(f, masm, &done, initialFrameDepth, vpOffset, MaybeRegisterID(), MaybeRegisterID()))
            return false;
        NativeStubLinker linker(masm, f.jit(), f.regs.pc, done);
        if (!linker.init(f.cx))
            THROWV(true);

        if (!linker.verifyRange(jit)) {
            disable(jit);
            return true;
        }

        linker.patchJump(ic.slowPathStart.labelAtOffset(ic.slowJoinOffset));

        ic.fastGuardedNative = fun;

        linker.link(funGuard, ic.slowPathStart);
        JSC::CodeLocationLabel start = linker.finalize(f);

        JaegerSpew(JSpew_PICs, "generated native CALL stub %p (%lu bytes)\n",
                   start.executableAddress(), (unsigned long) masm.size());

        Repatcher repatch(jit);
        repatch.relink(ic.funJump, start);

        return true;
    }

    void *update()
    {
        StackFrame *fp = f.fp();
        JITScript *jit = fp->jit();
        RecompilationMonitor monitor(cx);

        bool lowered = ic.frameSize.lowered(f.pc());
        JS_ASSERT_IF(lowered, !callingNew);

        stubs::UncachedCallResult ucr;
        if (callingNew)
            stubs::UncachedNewHelper(f, ic.frameSize.staticArgc(), &ucr);
        else
            stubs::UncachedCallHelper(f, ic.frameSize.getArgc(f), lowered, &ucr);

        // Watch out in case the IC was invalidated by a recompilation on the calling
        // script. This can happen either if the callee is executed or if it compiles
        // and the compilation has a static overflow.
        if (monitor.recompiled())
            return ucr.codeAddr;

        // If the function cannot be jitted (generally unjittable or empty script),
        // patch this site to go to a slow path always.
        if (!ucr.codeAddr) {
            if (ucr.unjittable)
                disable(jit);
            return NULL;
        }
            
        JSFunction *fun = ucr.fun;
        JS_ASSERT(fun);
        JSScript *script = fun->script();
        JS_ASSERT(script);

        uint32 flags = callingNew ? StackFrame::CONSTRUCTING : 0;

        if (!ic.hit) {
            ic.hit = true;
            return ucr.codeAddr;
        }

        if (!ic.frameSize.isStatic() || ic.frameSize.staticArgc() != fun->nargs) {
            if (!generateFullCallStub(jit, script, flags))
                THROWV(NULL);
        } else {
            if (!ic.fastGuardedObject && patchInlinePath(jit, script, fun)) {
                // Nothing, done.
            } else if (ic.fastGuardedObject &&
                       !ic.hasJsFunCheck &&
                       !ic.fastGuardedNative &&
                       ic.fastGuardedObject->toFunction()->script() == fun->script()) {
                /*
                 * Note: Multiple "function guard" stubs are not yet
                 * supported, thus the fastGuardedNative check.
                 */
                if (!generateStubForClosures(jit, fun))
                    THROWV(NULL);
            } else {
                if (!generateFullCallStub(jit, script, flags))
                    THROWV(NULL);
            }
        }

        return ucr.codeAddr;
    }
};

void * JS_FASTCALL
ic::Call(VMFrame &f, CallICInfo *ic)
{
    CallCompiler cc(f, *ic, false);
    return cc.update();
}

void * JS_FASTCALL
ic::New(VMFrame &f, CallICInfo *ic)
{
    CallCompiler cc(f, *ic, true);
    return cc.update();
}

void * JS_FASTCALL
ic::NativeCall(VMFrame &f, CallICInfo *ic)
{
    CallCompiler cc(f, *ic, false);
    if (!cc.generateNativeStub())
        stubs::SlowCall(f, ic->frameSize.getArgc(f));
    return NULL;
}

void * JS_FASTCALL
ic::NativeNew(VMFrame &f, CallICInfo *ic)
{
    CallCompiler cc(f, *ic, true);
    if (!cc.generateNativeStub())
        stubs::SlowNew(f, ic->frameSize.staticArgc());
    return NULL;
}

static JS_ALWAYS_INLINE bool
BumpStack(VMFrame &f, uintN inc)
{
    if (f.regs.sp + inc < f.stackLimit)
        return true;
    return f.cx->stack.space().tryBumpLimit(f.cx, f.regs.sp, inc, &f.stackLimit);
}

/*
 * SplatApplyArgs is only called for expressions of the form |f.apply(x, y)|.
 * Additionally, the callee has already been checked to be the native apply.
 * All successful paths through SplatApplyArgs must set f.u.call.dynamicArgc
 * and f.regs.sp.
 */
JSBool JS_FASTCALL
ic::SplatApplyArgs(VMFrame &f)
{
    JSContext *cx = f.cx;
    JS_ASSERT(!f.regs.inlined());
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
    if (f.u.call.lazyArgsObj) {
        Value *vp = f.regs.sp - 3;
        JS_ASSERT(JS_CALLEE(cx, vp).toObject().toFunction()->u.n.native == js_fun_apply);

        StackFrame *fp = f.regs.fp();
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

                /* Step 6. */
                if (length > StackSpace::ARGS_LENGTH_MAX) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_TOO_MANY_FUN_APPLY_ARGS);
                    THROWV(false);
                }

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
    JS_ASSERT(JS_CALLEE(cx, vp).toObject().toFunction()->u.n.native == js_fun_apply);

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

    /* Step 6. */
    if (length > StackSpace::ARGS_LENGTH_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_TOO_MANY_FUN_APPLY_ARGS);
        THROWV(false);
    }

    intN delta = length - 1;
    if (delta > 0 && !BumpStack(f, delta))
        THROWV(false);
    f.regs.sp += delta;

    /* Steps 7-8. */
    if (!GetElements(cx, aobj, length, f.regs.sp - length))
        THROWV(false);

    f.u.call.dynamicArgc = length;
    return true;
}

void
ic::GenerateArgumentCheckStub(VMFrame &f)
{
    JS_ASSERT(f.cx->typeInferenceEnabled());

    JITScript *jit = f.jit();
    StackFrame *fp = f.fp();
    JSFunction *fun = fp->fun();
    JSScript *script = fun->script();

    if (jit->argsCheckPool)
        jit->resetArgsCheck();

    Assembler masm;
    Vector<Jump> mismatches(f.cx);

    if (!f.fp()->isConstructing()) {
        types::TypeSet *types = types::TypeScript::ThisTypes(script);
        Address address(JSFrameReg, StackFrame::offsetOfThis(fun));
        if (!masm.generateTypeCheck(f.cx, address, types, &mismatches))
            return;
    }

    for (unsigned i = 0; i < fun->nargs; i++) {
        types::TypeSet *types = types::TypeScript::ArgTypes(script, i);
        Address address(JSFrameReg, StackFrame::offsetOfFormalArg(fun, i));
        if (!masm.generateTypeCheck(f.cx, address, types, &mismatches))
            return;
    }

    Jump done = masm.jump();

    LinkerHelper linker(masm, JSC::METHOD_CODE);
    JSC::ExecutablePool *ep = linker.init(f.cx);
    if (!ep)
        return;
    jit->argsCheckPool = ep;

    if (!linker.verifyRange(jit)) {
        jit->resetArgsCheck();
        return;
    }

    for (unsigned i = 0; i < mismatches.length(); i++)
        linker.link(mismatches[i], jit->argsCheckStub);
    linker.link(done, jit->argsCheckFallthrough);

    JSC::CodeLocationLabel cs = linker.finalize(f);

    JaegerSpew(JSpew_PICs, "generated ARGS CHECK stub %p (%lu bytes)\n",
               cs.executableAddress(), (unsigned long)masm.size());

    Repatcher repatch(jit);
    repatch.relink(jit->argsCheckJump, cs);
}

void
JITScript::resetArgsCheck()
{
    argsCheckPool->release();
    argsCheckPool = NULL;

    Repatcher repatch(this);
    repatch.relink(argsCheckJump, argsCheckStub);
}

#endif /* JS_MONOIC */

