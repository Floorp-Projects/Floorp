/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsanalyze.h"
#include "jscntxt.h"
#include "jsscope.h"
#include "jsobj.h"
#include "jslibmath.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsxml.h"
#include "jsbool.h"
#include "jstypes.h"

#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/CodeLocation.h"
#include "builtin/Eval.h"
#include "methodjit/StubCalls.h"
#include "methodjit/MonoIC.h"
#include "methodjit/BaseCompiler.h"
#include "methodjit/ICRepatcher.h"
#include "vm/Debugger.h"

#include "jsinterpinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsobjinlines.h"
#include "jscntxtinlines.h"
#include "jsatominlines.h"

#include "StubCalls-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

using ic::Repatcher;

static jsbytecode *
FindExceptionHandler(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    JSScript *script = fp->script();

    if (!script->hasTrynotes())
        return NULL;

  error:
    if (cx->isExceptionPending()) {
        for (TryNoteIter tni(cx->regs()); !tni.done(); ++tni) {
            JSTryNote *tn = *tni;

            UnwindScope(cx, tn->stackDepth);

            /*
             * Set pc to the first bytecode after the the try note to point
             * to the beginning of catch or finally or to [enditer] closing
             * the for-in loop.
             */
            jsbytecode *pc = script->main() + tn->start + tn->length;
            cx->regs().pc = pc;
            cx->regs().sp = cx->regs().spForStackDepth(tn->stackDepth);

            switch (tn->kind) {
                case JSTRY_CATCH:
                  JS_ASSERT(JSOp(*pc) == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                  /* Catch cannot intercept the closing of a generator. */
                  if (JS_UNLIKELY(cx->getPendingException().isMagic(JS_GENERATOR_CLOSING)))
                      break;
#endif

                  /*
                   * Don't clear cx->throwing to save cx->exception from GC
                   * until it is pushed to the stack via [exception] in the
                   * catch block.
                   */
                  return pc;

                case JSTRY_FINALLY:
                  /*
                   * Push (true, exception) pair for finally to indicate that
                   * [retsub] should rethrow the exception.
                   */
                  cx->regs().sp[0].setBoolean(true);
                  cx->regs().sp[1] = cx->getPendingException();
                  cx->regs().sp += 2;
                  cx->clearPendingException();
                  return pc;

                case JSTRY_ITER:
                {
                  /*
                   * This is similar to JSOP_ENDITER in the interpreter loop,
                   * except the code now uses the stack slot normally used by
                   * JSOP_NEXTITER, namely regs.sp[-1] before the regs.sp -= 2
                   * adjustment and regs.sp[1] after, to save and restore the
                   * pending exception.
                   */
                  JS_ASSERT(JSOp(*pc) == JSOP_ENDITER);
                  bool ok = UnwindIteratorForException(cx, &cx->regs().sp[-1].toObject());
                  cx->regs().sp -= 1;
                  if (!ok)
                      goto error;
                }
            }
        }
    } else {
        UnwindForUncatchableException(cx, cx->regs());
    }

    return NULL;
}

/*
 * Clean up a frame and return.
 */

void JS_FASTCALL
stubs::SlowCall(VMFrame &f, uint32_t argc)
{
    if (*f.regs.pc == JSOP_FUNAPPLY && !GuardFunApplyArgumentsOptimization(f.cx))
        THROW();

    CallArgs args = CallArgsFromSp(argc, f.regs.sp);
    if (!InvokeKernel(f.cx, args))
        THROW();

    types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
}

void JS_FASTCALL
stubs::SlowNew(VMFrame &f, uint32_t argc)
{
    CallArgs args = CallArgsFromSp(argc, f.regs.sp);
    if (!InvokeConstructorKernel(f.cx, args))
        THROW();

    types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
}

static inline bool
CheckStackQuota(VMFrame &f)
{
    JS_ASSERT(f.regs.stackDepth() == 0);

    f.stackLimit = f.cx->stack.space().getStackLimit(f.cx, DONT_REPORT_ERROR);
    if (f.stackLimit)
        return true;

    /* Remove the current partially-constructed frame before throwing. */
    f.cx->stack.popFrameAfterOverflow();
    js_ReportOverRecursed(f.cx);

    return false;
}

/*
 * HitStackQuota is called after the early prologue pushing the new frame would
 * overflow f.stackLimit.
 */
void JS_FASTCALL
stubs::HitStackQuota(VMFrame &f)
{
    if (!CheckStackQuota(f))
        THROW();
}

/*
 * This function must only be called after the early prologue, since it depends
 * on fp->exec.fun.
 */
void * JS_FASTCALL
stubs::FixupArity(VMFrame &f, uint32_t nactual)
{
    JSContext *cx = f.cx;
    StackFrame *oldfp = f.fp();

    JS_ASSERT(nactual != oldfp->numFormalArgs());

    /*
     * Grossssss! *move* the stack frame. If this ends up being perf-critical,
     * we can figure out how to spot-optimize it. Be careful to touch only the
     * members that have been initialized by the caller and early prologue.
     */
    InitialFrameFlags initial = oldfp->initialFlags();
    JSFunction *fun           = oldfp->fun();
    JSScript *script          = fun->script();
    void *ncode               = oldfp->nativeReturnAddress();

    /* Pop the inline frame. */
    f.regs.popPartialFrame((Value *)oldfp);

    /* Reserve enough space for a callee frame. */
    CallArgs args = CallArgsFromSp(nactual, f.regs.sp);
    StackFrame *fp = cx->stack.getFixupFrame(cx, DONT_REPORT_ERROR, args, fun,
                                             script, ncode, initial, &f.stackLimit);

    if (!fp) {
        f.regs.updateForNcode(f.jit(), ncode);
        js_ReportOverRecursed(cx);
        THROWV(NULL);
    }

    /* The caller takes care of assigning fp to regs. */
    return fp;
}

struct ResetStubRejoin {
    VMFrame &f;
    ResetStubRejoin(VMFrame &f) : f(f) {}
    ~ResetStubRejoin() { f.stubRejoin = 0; }
};

void * JS_FASTCALL
stubs::CompileFunction(VMFrame &f, uint32_t argc)
{
    /*
     * Note: the stubRejoin kind for the frame was written before the call, and
     * needs to be cleared out on all return paths (doing this directly in the
     * IC stub will not handle cases where we recompiled or threw).
     */
    JS_ASSERT_IF(f.cx->typeInferenceEnabled(), f.stubRejoin);
    ResetStubRejoin reset(f);

    InitialFrameFlags initial = f.fp()->initialFlags();
    f.regs.popPartialFrame((Value *)f.fp());

    if (InitialFrameFlagsAreConstructing(initial))
        return UncachedNew(f, argc);
    else if (InitialFrameFlagsAreLowered(initial))
        return UncachedLoweredCall(f, argc);
    else
        return UncachedCall(f, argc);
}

static inline bool
UncachedInlineCall(VMFrame &f, InitialFrameFlags initial,
                   void **pret, bool *unjittable, uint32_t argc)
{
    JSContext *cx = f.cx;
    CallArgs args = CallArgsFromSp(argc, f.regs.sp);
    JSFunction *newfun = args.callee().toFunction();
    JSScript *newscript = newfun->script();

    bool construct = InitialFrameFlagsAreConstructing(initial);

    bool newType = construct && cx->typeInferenceEnabled() &&
        types::UseNewType(cx, f.script(), f.pc());

    if (!types::TypeMonitorCall(cx, args, construct))
        return false;

    /* Try to compile if not already compiled. */
    CompileStatus status = CanMethodJIT(cx, newscript, newscript->code, construct,
                                        CompileRequest_Interpreter, f.fp());
    if (status == Compile_Error) {
        /* A runtime exception was thrown, get out. */
        return false;
    }
    if (status == Compile_Abort)
        *unjittable = true;

    /*
     * Make sure we are not calling from an inline frame if we need to make a
     * call object for the callee, as doing so could trigger GC and cause
     * jitcode discarding / frame expansion.
     */
    if (f.regs.inlined() && newfun->isHeavyweight()) {
        ExpandInlineFrames(cx->compartment);
        JS_ASSERT(!f.regs.inlined());
    }

    /*
     * Preserve f.regs.fp while pushing the new frame, for the invariant that
     * f.regs reflects the state when we entered the stub call. This handoff is
     * tricky: we need to make sure that f.regs is not updated to the new
     * frame, and we also need to ensure that cx->regs still points to f.regs
     * when space is reserved, in case doing so throws an exception.
     */
    FrameRegs regs = f.regs;

    /* Get pointer to new frame/slots, prepare arguments. */
    if (!cx->stack.pushInlineFrame(cx, regs, args, *newfun, newscript, initial, &f.stackLimit))
        return false;

    /* Finish the handoff to the new frame regs. */
    PreserveRegsGuard regsGuard(cx, regs);

    /*
     * If newscript was successfully compiled, run it. Skip for calls which
     * will be constructing a new type object for 'this'.
     */
    if (!newType) {
        if (JITScript *jit = newscript->getJIT(regs.fp()->isConstructing(), cx->compartment->needsBarrier())) {
            if (jit->invokeEntry) {
                *pret = jit->invokeEntry;

                /* Restore the old fp around and let the JIT code repush the new fp. */
                regs.popFrame((Value *) regs.fp());
                return true;
            }
        }
    }

    /*
     * Otherwise, run newscript in the interpreter. Expand any inlined frame we
     * are calling from, as the new frame is not associated with the VMFrame
     * and will not have its prevpc info updated if frame expansion is
     * triggered while interpreting.
     */
    if (f.regs.inlined()) {
        ExpandInlineFrames(cx->compartment);
        JS_ASSERT(!f.regs.inlined());
        regs.fp()->resetInlinePrev(f.fp(), f.regs.pc);
    }

    JS_CHECK_RECURSION(cx, return false);

    bool ok = Interpret(cx, cx->fp());
    f.cx->stack.popInlineFrame(regs);

    if (ok)
        types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());

    *pret = NULL;
    return ok;
}

void * JS_FASTCALL
stubs::UncachedNew(VMFrame &f, uint32_t argc)
{
    UncachedCallResult ucr;
    UncachedNewHelper(f, argc, &ucr);
    return ucr.codeAddr;
}

void
stubs::UncachedNewHelper(VMFrame &f, uint32_t argc, UncachedCallResult *ucr)
{
    ucr->init();
    JSContext *cx = f.cx;
    CallArgs args = CallArgsFromSp(argc, f.regs.sp);

    /* Try to do a fast inline call before the general Invoke path. */
    if (IsFunctionObject(args.calleev(), &ucr->fun) && ucr->fun->isInterpretedConstructor()) {
        if (!UncachedInlineCall(f, INITIAL_CONSTRUCT, &ucr->codeAddr, &ucr->unjittable, argc))
            THROW();
    } else {
        if (!InvokeConstructorKernel(cx, args))
            THROW();
        types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
    }
}

void * JS_FASTCALL
stubs::UncachedCall(VMFrame &f, uint32_t argc)
{
    UncachedCallResult ucr;
    UncachedCallHelper(f, argc, false, &ucr);
    return ucr.codeAddr;
}

void * JS_FASTCALL
stubs::UncachedLoweredCall(VMFrame &f, uint32_t argc)
{
    UncachedCallResult ucr;
    UncachedCallHelper(f, argc, true, &ucr);
    return ucr.codeAddr;
}

void JS_FASTCALL
stubs::Eval(VMFrame &f, uint32_t argc)
{
    CallArgs args = CallArgsFromSp(argc, f.regs.sp);

    if (!IsBuiltinEvalForScope(f.fp()->scopeChain(), args.calleev())) {
        if (!InvokeKernel(f.cx, args))
            THROW();

        types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
        return;
    }

    JS_ASSERT(f.fp() == f.cx->fp());
    if (!DirectEval(f.cx, args))
        THROW();

    types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
}

void
stubs::UncachedCallHelper(VMFrame &f, uint32_t argc, bool lowered, UncachedCallResult *ucr)
{
    ucr->init();

    JSContext *cx = f.cx;
    CallArgs args = CallArgsFromSp(argc, f.regs.sp);

    if (IsFunctionObject(args.calleev(), &ucr->fun)) {
        if (ucr->fun->isInterpreted()) {
            InitialFrameFlags initial = lowered ? INITIAL_LOWERED : INITIAL_NONE;
            if (!UncachedInlineCall(f, initial, &ucr->codeAddr, &ucr->unjittable, argc))
                THROW();
            return;
        }

        if (ucr->fun->isNative()) {
            if (!CallJSNative(cx, ucr->fun->native(), args))
                THROW();
            types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
            return;
        }
    }

    if (!InvokeKernel(f.cx, args))
        THROW();

    types::TypeScript::Monitor(f.cx, f.script(), f.pc(), args.rval());
    return;
}

static void
RemoveOrphanedNative(JSContext *cx, StackFrame *fp)
{
    /*
     * Remove fp from the list of frames holding a reference on the orphaned
     * native pools. If all the references have been removed, release all the
     * pools. We don't release pools piecemeal as a pool can be referenced by
     * multiple frames.
     */
    JaegerRuntime &jr = cx->jaegerRuntime();
    if (jr.orphanedNativeFrames.empty())
        return;
    for (unsigned i = 0; i < jr.orphanedNativeFrames.length(); i++) {
        if (fp == jr.orphanedNativeFrames[i]) {
            jr.orphanedNativeFrames[i] = jr.orphanedNativeFrames.back();
            jr.orphanedNativeFrames.popBack();
            break;
        }
    }
    if (jr.orphanedNativeFrames.empty()) {
        for (unsigned i = 0; i < jr.orphanedNativePools.length(); i++)
            jr.orphanedNativePools[i]->release();
        jr.orphanedNativePools.clear();
    }
}

extern "C" void *
js_InternalThrow(VMFrame &f)
{
    JSContext *cx = f.cx;

    ExpandInlineFrames(cx->compartment);

    // The current frame may have an associated orphaned native, if the native
    // or SplatApplyArgs threw an exception.
    RemoveOrphanedNative(cx, f.fp());

    JS_ASSERT(!f.fp()->finishedInInterpreter());

    // Make sure sp is up to date.
    JS_ASSERT(&cx->regs() == &f.regs);

    jsbytecode *pc = NULL;
    for (;;) {
        if (cx->isExceptionPending()) {
            // Call the throw hook if necessary
            JSThrowHook handler = cx->runtime->debugHooks.throwHook;
            if (handler || !cx->compartment->getDebuggees().empty()) {
                Value rval;
                JSTrapStatus st = Debugger::onExceptionUnwind(cx, &rval);
                if (st == JSTRAP_CONTINUE && handler) {
                    st = handler(cx, cx->fp()->script(), cx->regs().pc, &rval,
                                 cx->runtime->debugHooks.throwHookData);
                }

                switch (st) {
                case JSTRAP_ERROR:
                    cx->clearPendingException();
                    break;

                case JSTRAP_CONTINUE:
                    break;

                case JSTRAP_RETURN:
                    cx->clearPendingException();
                    cx->fp()->setReturnValue(rval);
                    return cx->jaegerRuntime().forceReturnFromExternC();

                case JSTRAP_THROW:
                    cx->setPendingException(rval);
                    break;

                default:
                    JS_NOT_REACHED("bad onExceptionUnwind status");
                }
            }
        }

        pc = FindExceptionHandler(cx);
        if (pc)
            break;

        // The JIT guarantees that ScriptDebugEpilogue() and ScriptEpilogue()
        // have always been run upon exiting to its caller. This is important
        // for consistency, where execution modes make similar guarantees about
        // prologues and epilogues. Interpret(), and Invoke() all rely on this
        // property.
        JS_ASSERT(!f.fp()->finishedInInterpreter());
        UnwindScope(cx, 0);
        f.regs.setToEndOfScript();

        if (cx->compartment->debugMode()) {
            // This can turn a throw or error into a healthy return. Note that
            // we will run ScriptDebugEpilogue again (from AnyFrameEpilogue);
            // ScriptDebugEpilogue is prepared for this eventuality.
            if (js::ScriptDebugEpilogue(cx, f.fp(), false))
                return cx->jaegerRuntime().forceReturnFromExternC();
        }


        f.fp()->epilogue(f.cx);

        // Don't remove the last frame, this is the responsibility of
        // JaegerShot()'s caller. We only guarantee that ScriptEpilogue()
        // has been run.
        if (f.entryfp == f.fp())
            break;

        f.cx->stack.popInlineFrame(f.regs);
        DebugOnly<JSOp> op = JSOp(*f.regs.pc);
        JS_ASSERT(op == JSOP_CALL ||
                  op == JSOP_NEW ||
                  op == JSOP_EVAL ||
                  op == JSOP_FUNCALL ||
                  op == JSOP_FUNAPPLY);
        f.regs.pc += JSOP_CALL_LENGTH;
    }

    JS_ASSERT(&cx->regs() == &f.regs);

    if (!pc)
        return NULL;

    StackFrame *fp = cx->fp();
    JSScript *script = fp->script();

    /*
     * Fall back to EnterMethodJIT and finish the frame in the interpreter.
     * With type inference enabled, we may wipe out all JIT code on the
     * stack without patching ncode values to jump to the interpreter, and
     * thus can only enter JIT code via EnterMethodJIT (which overwrites
     * its entry frame's ncode). See ClearAllFrames.
     */
    cx->jaegerRuntime().setLastUnfinished(Jaeger_Unfinished);

    if (!script->ensureRanAnalysis(cx)) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    analyze::AutoEnterAnalysis enter(cx);

    /*
     * Interpret the ENTERBLOCK and EXCEPTION opcodes, so that we don't go
     * back into the interpreter with a pending exception. This will cause
     * it to immediately rethrow.
     */
    if (cx->isExceptionPending()) {
        JS_ASSERT(JSOp(*pc) == JSOP_ENTERBLOCK);
        StaticBlockObject &blockObj = script->getObject(GET_UINT32_INDEX(pc))->asStaticBlock();
        Value *vp = cx->regs().sp + blockObj.slotCount();
        SetValueRangeToUndefined(cx->regs().sp, vp);
        cx->regs().sp = vp;
        if (!cx->regs().fp()->pushBlock(cx, blockObj))
            return NULL;

        JS_ASSERT(JSOp(pc[JSOP_ENTERBLOCK_LENGTH]) == JSOP_EXCEPTION);
        cx->regs().sp[0] = cx->getPendingException();
        cx->clearPendingException();
        cx->regs().sp++;

        cx->regs().pc = pc + JSOP_ENTERBLOCK_LENGTH + JSOP_EXCEPTION_LENGTH;
    }

    *f.oldregs = f.regs;

    return NULL;
}

void JS_FASTCALL
stubs::CreateThis(VMFrame &f, JSObject *proto)
{
    JSContext *cx = f.cx;
    StackFrame *fp = f.fp();
    RootedObject callee(cx, &fp->callee());
    JSObject *obj = js_CreateThisForFunctionWithProto(cx, callee, proto);
    if (!obj)
        THROW();
    fp->thisValue() = ObjectValue(*obj);
}

void JS_FASTCALL
stubs::ScriptDebugPrologue(VMFrame &f)
{
    Probes::enterScript(f.cx, f.script(), f.script()->function(), f.fp());
    JSTrapStatus status = js::ScriptDebugPrologue(f.cx, f.fp());
    switch (status) {
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_RETURN:
        *f.returnAddressLocation() = f.cx->jaegerRuntime().forceReturnFromFastCall();
        return;
      case JSTRAP_ERROR:
      case JSTRAP_THROW:
        THROW();
      default:
        JS_NOT_REACHED("bad ScriptDebugPrologue status");
    }
}

void JS_FASTCALL
stubs::ScriptDebugEpilogue(VMFrame &f)
{
    if (!js::ScriptDebugEpilogue(f.cx, f.fp(), JS_TRUE))
        THROW();
}

void JS_FASTCALL
stubs::ScriptProbeOnlyPrologue(VMFrame &f)
{
    Probes::enterScript(f.cx, f.script(), f.script()->function(), f.fp());
}

void JS_FASTCALL
stubs::ScriptProbeOnlyEpilogue(VMFrame &f)
{
    Probes::exitScript(f.cx, f.script(), f.script()->function(), f.fp());
}

void JS_FASTCALL
stubs::CrossChunkShim(VMFrame &f, void *edge_)
{
    DebugOnly<CrossChunkEdge*> edge = (CrossChunkEdge *) edge_;

    mjit::ExpandInlineFrames(f.cx->compartment);

    JSScript *script = f.script();
    JS_ASSERT(edge->target < script->length);
    JS_ASSERT(script->code + edge->target == f.pc());

    CompileStatus status = CanMethodJIT(f.cx, script, f.pc(), f.fp()->isConstructing(),
                                        CompileRequest_Interpreter, f.fp());
    if (status == Compile_Error)
        THROW();

    void **addr = f.returnAddressLocation();
    *addr = JS_FUNC_TO_DATA_PTR(void *, JaegerInterpoline);

    f.fp()->setRejoin(StubRejoin(REJOIN_RESUME));
}

JS_STATIC_ASSERT(JSOP_NOP == 0);

/* :XXX: common out with identical copy in Compiler.cpp */
#if defined(JS_METHODJIT_SPEW)
static const char *OpcodeNames[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) #name,
# include "jsopcode.tbl"
# undef OPDEF
};
#endif

static void
FinishVarIncOp(VMFrame &f, RejoinState rejoin, Value ov, Value nv, Value *vp)
{
    /* Finish an increment operation on a LOCAL or ARG. These do not involve property accesses. */
    JS_ASSERT(rejoin == REJOIN_POS || rejoin == REJOIN_BINARY);

    JSContext *cx = f.cx;

    JSOp op = JSOp(*f.pc());
    JS_ASSERT(op == JSOP_LOCALINC || op == JSOP_INCLOCAL ||
              op == JSOP_LOCALDEC || op == JSOP_DECLOCAL ||
              op == JSOP_ARGINC || op == JSOP_INCARG ||
              op == JSOP_ARGDEC || op == JSOP_DECARG);
    const JSCodeSpec *cs = &js_CodeSpec[op];

    if (rejoin == REJOIN_POS) {
        double d = ov.toNumber();
        double N = (cs->format & JOF_INC) ? 1 : -1;
        if (!nv.setNumber(d + N))
            types::TypeScript::MonitorOverflow(cx, f.script(), f.pc());
    }

    unsigned i = GET_SLOTNO(f.pc());
    if (JOF_TYPE(cs->format) == JOF_LOCAL)
        f.fp()->unaliasedLocal(i) = nv;
    else if (f.fp()->script()->argsObjAliasesFormals())
        f.fp()->argsObj().setArg(i, nv);
    else
        f.fp()->unaliasedFormal(i) = nv;

    *vp = (cs->format & JOF_POST) ? ov : nv;
}

extern "C" void *
js_InternalInterpret(void *returnData, void *returnType, void *returnReg, js::VMFrame &f)
{
    FrameRejoinState jsrejoin = f.fp()->rejoin();
    RejoinState rejoin;
    if (jsrejoin & 0x1) {
        /* Rejoin after a scripted call finished. Restore f.regs.pc and f.regs.inlined (NULL) */
        uint32_t pcOffset = jsrejoin >> 1;
        f.regs.pc = f.fp()->script()->code + pcOffset;
        f.regs.clearInlined();
        rejoin = REJOIN_SCRIPTED;
    } else {
        rejoin = (RejoinState) (jsrejoin >> 1);
    }

    JSContext *cx = f.cx;
    StackFrame *fp = f.regs.fp();
    JSScript *script = fp->script();

    jsbytecode *pc = f.regs.pc;

    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];

    if (!script->ensureRanAnalysis(cx)) {
        js_ReportOutOfMemory(cx);
        return js_InternalThrow(f);
    }

    analyze::AutoEnterAnalysis enter(cx);
    analyze::ScriptAnalysis *analysis = script->analysis();

    /*
     * f.regs.sp is not normally maintained by stubs (except for call prologues
     * where it indicates the new frame), so is not expected to be coherent
     * here. Update it to its value at the start of the opcode.
     */
    Value *oldsp = f.regs.sp;
    f.regs.sp = f.regs.spForStackDepth(analysis->getCode(pc).stackDepth);

    jsbytecode *nextpc = pc + GetBytecodeLength(pc);
    Value *nextsp = NULL;
    if (nextpc != script->code + script->length && analysis->maybeCode(nextpc))
        nextsp = f.regs.spForStackDepth(analysis->getCode(nextpc).stackDepth);

    JS_ASSERT(&cx->regs() == &f.regs);

#ifdef JS_METHODJIT_SPEW
    JaegerSpew(JSpew_Recompile, "interpreter rejoin (file \"%s\") (line \"%d\") (op %s) (opline \"%d\")\n",
               script->filename, script->lineno, OpcodeNames[op], PCToLineNumber(script, pc));
#endif

    uint32_t nextDepth = UINT32_MAX;
    bool skipTrap = false;

    if ((cs->format & (JOF_INC | JOF_DEC)) &&
        (rejoin == REJOIN_POS || rejoin == REJOIN_BINARY)) {
        /*
         * We may reenter the interpreter while finishing the INC/DEC operation
         * on a local or arg (property INC/DEC operations will rejoin into the
         * decomposed version of the op.
         */
        JS_ASSERT(cs->format & (JOF_LOCAL | JOF_QARG));

        nextDepth = analysis->getCode(nextpc).stackDepth;
        enter.leave();

        if (rejoin != REJOIN_BINARY || !analysis->incrementInitialValueObserved(pc)) {
            /* Stack layout is 'V', 'N' or 'N+1' (only if the N is not needed) */
            FinishVarIncOp(f, rejoin, nextsp[-1], nextsp[-1], &nextsp[-1]);
        } else {
            /* Stack layout is 'N N+1' */
            FinishVarIncOp(f, rejoin, nextsp[-1], nextsp[0], &nextsp[-1]);
        }

        rejoin = REJOIN_FALLTHROUGH;
    }

    switch (rejoin) {
      case REJOIN_SCRIPTED: {
        jsval_layout rval;
#ifdef JS_NUNBOX32
        rval.asBits = ((uint64_t)returnType << 32) | (uint32_t)returnData;
#elif JS_PUNBOX64
        rval.asBits = (uint64_t)returnType | (uint64_t)returnData;
#else
#error "Unknown boxing format"
#endif

        nextsp[-1] = IMPL_TO_JSVAL(rval);

        /*
         * When making a scripted call at monitored sites, it is the caller's
         * responsibility to update the pushed type set.
         */
        types::TypeScript::Monitor(cx, script, pc, nextsp[-1]);
        f.regs.pc = nextpc;
        break;
      }

      case REJOIN_NONE:
        JS_NOT_REACHED("Unpossible rejoin!");
        break;

      case REJOIN_RESUME:
        break;

      case REJOIN_TRAP:
        /*
         * Make sure when resuming in the interpreter we do not execute the
         * trap again. Watch out for the case where the trap removed itself.
         */
        if (script->hasBreakpointsAt(pc))
            skipTrap = true;
        break;

      case REJOIN_FALLTHROUGH:
        f.regs.pc = nextpc;
        break;

      case REJOIN_NATIVE:
      case REJOIN_NATIVE_LOWERED:
      case REJOIN_NATIVE_GETTER: {
        /*
         * We don't rejoin until after the native stub finishes execution, in
         * which case the return value will be in memory. For lowered natives,
         * the return value will be in the 'this' value's slot.
         */
        if (rejoin != REJOIN_NATIVE)
            nextsp[-1] = nextsp[0];

        /* Release this reference on the orphaned native stub. */
        RemoveOrphanedNative(cx, fp);

        f.regs.pc = nextpc;
        break;
      }

      case REJOIN_PUSH_BOOLEAN:
        nextsp[-1].setBoolean(returnReg != NULL);
        f.regs.pc = nextpc;
        break;

      case REJOIN_PUSH_OBJECT:
        nextsp[-1].setObject(* (JSObject *) returnReg);
        f.regs.pc = nextpc;
        break;

      case REJOIN_THIS_PROTOTYPE: {
        RootedObject callee(cx, &fp->callee());
        JSObject *proto = f.regs.sp[0].isObject() ? &f.regs.sp[0].toObject() : NULL;
        JSObject *obj = js_CreateThisForFunctionWithProto(cx, callee, proto);
        if (!obj)
            return js_InternalThrow(f);
        fp->thisValue() = ObjectValue(*obj);

        Probes::enterScript(f.cx, f.script(), f.script()->function(), fp);

        if (script->debugMode) {
            JSTrapStatus status = js::ScriptDebugPrologue(f.cx, f.fp());
            switch (status) {
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                *f.returnAddressLocation() = f.cx->jaegerRuntime().forceReturnFromExternC();
                return NULL;
              case JSTRAP_THROW:
              case JSTRAP_ERROR:
                return js_InternalThrow(f);
              default:
                JS_NOT_REACHED("bad ScriptDebugPrologue status");
            }
        }

        break;
      }

      /*
       * Each of these cases indicates a point of progress through
       * generatePrologue. Execute the rest of the prologue here.
       */
      case REJOIN_CHECK_ARGUMENTS:
        if (!CheckStackQuota(f))
            return js_InternalThrow(f);
        fp->initVarsToUndefined();
        fp->scopeChain();
        if (!fp->prologue(cx, types::UseNewTypeAtEntry(cx, fp)))
            return js_InternalThrow(f);

        /*
         * We would normally call ScriptDebugPrologue here. But in debug mode,
         * we only use JITted functions' invokeEntry entry point, whereas
         * CheckArgumentTypes (REJOIN_CHECK_ARGUMENTS) is only reachable via
         * the other entry points.
         *
         * If we fix bug 699196 ("Debug mode code could use inline caches
         * now"), then this case will become reachable again.
         */
        JS_ASSERT(!cx->compartment->debugMode());
        break;

      /* Finish executing the tail of generatePrologue. */
      case REJOIN_FUNCTION_PROLOGUE:
        if (fp->isConstructing()) {
            RootedObject callee(cx, &fp->callee());
            JSObject *obj = js_CreateThisForFunction(cx, callee, types::UseNewTypeAtEntry(cx, fp));
            if (!obj)
                return js_InternalThrow(f);
            fp->functionThis() = ObjectValue(*obj);
        }
        /* FALLTHROUGH */
      case REJOIN_EVAL_PROLOGUE:
        Probes::enterScript(cx, f.script(), f.script()->function(), fp);
        if (cx->compartment->debugMode()) {
            JSTrapStatus status = ScriptDebugPrologue(cx, fp);
            switch (status) {
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                return f.cx->jaegerRuntime().forceReturnFromFastCall();
              case JSTRAP_ERROR:
              case JSTRAP_THROW:
                return js_InternalThrow(f);
              default:
                JS_NOT_REACHED("bad ScriptDebugPrologue status");
            }
        }
        break;

      case REJOIN_CALL_PROLOGUE:
      case REJOIN_CALL_PROLOGUE_LOWERED_CALL:
      case REJOIN_CALL_PROLOGUE_LOWERED_APPLY:
        if (returnReg) {
            uint32_t argc = 0;
            if (rejoin == REJOIN_CALL_PROLOGUE)
                argc = GET_ARGC(pc);
            else if (rejoin == REJOIN_CALL_PROLOGUE_LOWERED_CALL)
                argc = GET_ARGC(pc) - 1;
            else
                argc = f.u.call.dynamicArgc;

            /*
             * The caller frame's code was discarded, but we still need to
             * execute the callee and have a JIT code pointer to do so.
             * Set the argc and frame registers as the call path does, but set
             * the callee frame's return address to jump back into the
             * Interpoline, and change the caller frame's rejoin to reflect the
             * state after the call.
             */
            f.regs.restorePartialFrame(oldsp); /* f.regs.sp stored the new frame */
            f.scratch = (void *) uintptr_t(argc); /* The interpoline will load f.scratch into argc */
            f.fp()->setNativeReturnAddress(JS_FUNC_TO_DATA_PTR(void *, JaegerInterpolineScripted));
            fp->setRejoin(REJOIN_SCRIPTED | ((pc - script->code) << 1));
            return returnReg;
        } else {
            /*
             * The call has already finished, and the return value is on the
             * stack. For lowered call/apply, the return value has been stored
             * in the wrong slot, so adjust it here.
             */
            f.regs.pc = nextpc;
            if (rejoin != REJOIN_CALL_PROLOGUE) {
                /* Same offset return value as for lowered native calls. */
                nextsp[-1] = nextsp[0];
            }
        }
        break;

      case REJOIN_CALL_SPLAT: {
        /* Leave analysis early and do the Invoke which SplatApplyArgs prepared. */
        nextDepth = analysis->getCode(nextpc).stackDepth;
        enter.leave();
        f.regs.sp = nextsp + 2 + f.u.call.dynamicArgc;
        if (!InvokeKernel(cx, CallArgsFromSp(f.u.call.dynamicArgc, f.regs.sp)))
            return js_InternalThrow(f);
        nextsp[-1] = nextsp[0];
        f.regs.pc = nextpc;
        break;
      }

      case REJOIN_GETTER:
        /*
         * Match the PC to figure out whether this property fetch is part of a
         * fused opcode which needs to be finished.
         */
        switch (op) {
          case JSOP_INSTANCEOF: {
            /*
             * If we recompiled from a getprop used within JSOP_INSTANCEOF,
             * the stack looks like 'LHS RHS protov'. Inline the remaining
             * portion of fun_hasInstance.
             */
            if (f.regs.sp[0].isPrimitive()) {
                js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, f.regs.sp[-1], NULL);
                return js_InternalThrow(f);
            }
            nextsp[-1].setBoolean(js_IsDelegate(cx, &f.regs.sp[0].toObject(), f.regs.sp[-2]));
            f.regs.pc = nextpc;
            break;
          }

          default:
            f.regs.pc = nextpc;
            break;
        }
        break;

      case REJOIN_POS:
        /* Convert-to-number which might be part of an INC* op. */
        JS_ASSERT(op == JSOP_POS);
        f.regs.pc = nextpc;
        break;

      case REJOIN_BINARY:
        /* Binary arithmetic op which might be part of an INC* op. */
        JS_ASSERT(op == JSOP_ADD || op == JSOP_SUB || op == JSOP_MUL || op == JSOP_DIV);
        f.regs.pc = nextpc;
        break;

      case REJOIN_BRANCH: {
        /*
         * This must be an opcode fused with IFNE/IFEQ. Unfused IFNE/IFEQ are
         * implemented in terms of ValueToBoolean, which is infallible and
         * cannot trigger recompilation.
         */
        bool takeBranch = false;
        switch (JSOp(*nextpc)) {
          case JSOP_IFNE:
            takeBranch = returnReg != NULL;
            break;
          case JSOP_IFEQ:
            takeBranch = returnReg == NULL;
            break;
          default:
            JS_NOT_REACHED("Bad branch op");
        }
        if (takeBranch)
            f.regs.pc = nextpc + GET_JUMP_OFFSET(nextpc);
        else
            f.regs.pc = nextpc + GetBytecodeLength(nextpc);
        break;
      }

      default:
        JS_NOT_REACHED("Missing rejoin");
    }

    if (nextDepth == UINT32_MAX)
        nextDepth = analysis->getCode(f.regs.pc).stackDepth;
    f.regs.sp = f.regs.spForStackDepth(nextDepth);

    /*
     * Monitor the result of the previous op when finishing a JOF_TYPESET op.
     * The result may not have been marked if we bailed out while inside a stub
     * for the op.
     */
    if (f.regs.pc == nextpc && (js_CodeSpec[op].format & JOF_TYPESET))
        types::TypeScript::Monitor(cx, script, pc, f.regs.sp[-1]);

    /* Mark the entry frame as unfinished, and update the regs to resume at. */
    JaegerStatus status = skipTrap ? Jaeger_UnfinishedAtTrap : Jaeger_Unfinished;
    cx->jaegerRuntime().setLastUnfinished(status);
    *f.oldregs = f.regs;

    return NULL;
}
