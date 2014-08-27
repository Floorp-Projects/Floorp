/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Stack-inl.h"

#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "asmjs/AsmJSFrameIterator.h"
#include "asmjs/AsmJSModule.h"
#include "gc/Marking.h"
#include "jit/BaselineFrame.h"
#include "jit/JitCompartment.h"
#include "js/GCAPI.h"
#include "vm/Opcodes.h"

#include "jit/JitFrameIterator-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Probes-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;

using mozilla::PodCopy;

/*****************************************************************************/

void
InterpreterFrame::initExecuteFrame(JSContext *cx, JSScript *script, AbstractFramePtr evalInFramePrev,
                                   const Value &thisv, JSObject &scopeChain, ExecuteType type)
{
    /*
     * See encoding of ExecuteType. When GLOBAL isn't set, we are executing a
     * script in the context of another frame and the frame type is determined
     * by the context.
     */
    flags_ = type | HAS_SCOPECHAIN;

    JSObject *callee = nullptr;
    if (!(flags_ & (GLOBAL))) {
        if (evalInFramePrev) {
            JS_ASSERT(evalInFramePrev.isFunctionFrame() || evalInFramePrev.isGlobalFrame());
            if (evalInFramePrev.isFunctionFrame()) {
                callee = evalInFramePrev.callee();
                flags_ |= FUNCTION;
            } else {
                flags_ |= GLOBAL;
            }
        } else {
            FrameIter iter(cx);
            JS_ASSERT(iter.isFunctionFrame() || iter.isGlobalFrame());
            JS_ASSERT(!iter.isAsmJS());
            if (iter.isFunctionFrame()) {
                callee = iter.callee();
                flags_ |= FUNCTION;
            } else {
                flags_ |= GLOBAL;
            }
        }
    }

    Value *dstvp = (Value *)this - 2;
    dstvp[1] = thisv;

    if (isFunctionFrame()) {
        dstvp[0] = ObjectValue(*callee);
        exec.fun = &callee->as<JSFunction>();
        u.evalScript = script;
    } else {
        JS_ASSERT(isGlobalFrame());
        dstvp[0] = NullValue();
        exec.script = script;
#ifdef DEBUG
        u.evalScript = (JSScript *)0xbad;
#endif
    }

    scopeChain_ = &scopeChain;
    prev_ = nullptr;
    prevpc_ = nullptr;
    prevsp_ = nullptr;

    JS_ASSERT_IF(evalInFramePrev, isDebuggerFrame());
    evalInFramePrev_ = evalInFramePrev;

#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch(&rval_, 1);
#endif
}

template <InterpreterFrame::TriggerPostBarriers doPostBarrier>
void
InterpreterFrame::copyFrameAndValues(JSContext *cx, Value *vp, InterpreterFrame *otherfp,
                                     const Value *othervp, Value *othersp)
{
    JS_ASSERT(othervp == otherfp->generatorArgsSnapshotBegin());
    JS_ASSERT(othersp >= otherfp->slots());
    JS_ASSERT(othersp <= otherfp->generatorSlotsSnapshotBegin() + otherfp->script()->nslots());

    /* Copy args, InterpreterFrame, and slots. */
    const Value *srcend = otherfp->generatorArgsSnapshotEnd();
    Value *dst = vp;
    for (const Value *src = othervp; src < srcend; src++, dst++) {
        *dst = *src;
        if (doPostBarrier)
            HeapValue::writeBarrierPost(*dst, dst);
    }

    *this = *otherfp;
    argv_ = vp + 2;
    unsetPushedSPSFrame();
    if (doPostBarrier)
        writeBarrierPost();

    srcend = othersp;
    dst = slots();
    for (const Value *src = otherfp->slots(); src < srcend; src++, dst++) {
        *dst = *src;
        if (doPostBarrier)
            HeapValue::writeBarrierPost(*dst, dst);
    }
}

/* Note: explicit instantiation for js_NewGenerator located in jsiter.cpp. */
template
void InterpreterFrame::copyFrameAndValues<InterpreterFrame::NoPostBarrier>(
                                    JSContext *, Value *, InterpreterFrame *, const Value *, Value *);
template
void InterpreterFrame::copyFrameAndValues<InterpreterFrame::DoPostBarrier>(
                                    JSContext *, Value *, InterpreterFrame *, const Value *, Value *);

void
InterpreterFrame::writeBarrierPost()
{
    /* This needs to follow the same rules as in InterpreterFrame::mark. */
    if (scopeChain_)
        JSObject::writeBarrierPost(scopeChain_, &scopeChain_);
    if (flags_ & HAS_ARGS_OBJ)
        JSObject::writeBarrierPost(argsObj_, &argsObj_);
    if (isFunctionFrame()) {
        JSFunction::writeBarrierPost(exec.fun, &exec.fun);
        if (isEvalFrame())
            JSScript::writeBarrierPost(u.evalScript, &u.evalScript);
    } else {
        JSScript::writeBarrierPost(exec.script, &exec.script);
    }
    if (hasReturnValue())
        HeapValue::writeBarrierPost(rval_, &rval_);
}

bool
InterpreterFrame::copyRawFrameSlots(AutoValueVector *vec)
{
    if (!vec->resize(numFormalArgs() + script()->nfixed()))
        return false;
    PodCopy(vec->begin(), argv(), numFormalArgs());
    PodCopy(vec->begin() + numFormalArgs(), slots(), script()->nfixed());
    return true;
}

JSObject *
InterpreterFrame::createRestParameter(JSContext *cx)
{
    JS_ASSERT(fun()->hasRest());
    unsigned nformal = fun()->nargs() - 1, nactual = numActualArgs();
    unsigned nrest = (nactual > nformal) ? nactual - nformal : 0;
    Value *restvp = argv() + nformal;
    JSObject *obj = NewDenseCopiedArray(cx, nrest, restvp, nullptr);
    if (!obj)
        return nullptr;
    types::FixRestArgumentsType(cx, obj);
    return obj;
}

static inline void
AssertDynamicScopeMatchesStaticScope(JSContext *cx, JSScript *script, JSObject *scope)
{
#ifdef DEBUG
    RootedObject enclosingScope(cx, script->enclosingStaticScope());
    for (StaticScopeIter<NoGC> i(enclosingScope); !i.done(); i++) {
        if (i.hasDynamicScopeObject()) {
            switch (i.type()) {
              case StaticScopeIter<NoGC>::BLOCK:
                JS_ASSERT(&i.block() == scope->as<ClonedBlockObject>().staticScope());
                scope = &scope->as<ClonedBlockObject>().enclosingScope();
                break;
              case StaticScopeIter<NoGC>::WITH:
                JS_ASSERT(&i.staticWith() == scope->as<DynamicWithObject>().staticScope());
                scope = &scope->as<DynamicWithObject>().enclosingScope();
                break;
              case StaticScopeIter<NoGC>::FUNCTION:
                JS_ASSERT(scope->as<CallObject>().callee().nonLazyScript() == i.funScript());
                scope = &scope->as<CallObject>().enclosingScope();
                break;
              case StaticScopeIter<NoGC>::NAMED_LAMBDA:
                scope = &scope->as<DeclEnvObject>().enclosingScope();
                break;
            }
        }
    }

    /*
     * Ideally, we'd JS_ASSERT(!scope->is<ScopeObject>()) but the enclosing
     * lexical scope chain stops at eval() boundaries. See StaticScopeIter
     * comment.
     */
#endif
}

bool
InterpreterFrame::initFunctionScopeObjects(JSContext *cx)
{
    CallObject *callobj = CallObject::createForFunction(cx, this);
    if (!callobj)
        return false;
    pushOnScopeChain(*callobj);
    flags_ |= HAS_CALL_OBJ;
    return true;
}

bool
InterpreterFrame::prologue(JSContext *cx)
{
    RootedScript script(cx, this->script());

    JS_ASSERT(!isGeneratorFrame());
    JS_ASSERT(cx->interpreterRegs().pc == script->code());

    if (isEvalFrame()) {
        if (script->strict()) {
            CallObject *callobj = CallObject::createForStrictEval(cx, this);
            if (!callobj)
                return false;
            pushOnScopeChain(*callobj);
            flags_ |= HAS_CALL_OBJ;
        }
        return probes::EnterScript(cx, script, nullptr, this);
    }

    if (isGlobalFrame())
        return probes::EnterScript(cx, script, nullptr, this);

    JS_ASSERT(isNonEvalFunctionFrame());
    AssertDynamicScopeMatchesStaticScope(cx, script, scopeChain());

    if (fun()->isHeavyweight() && !initFunctionScopeObjects(cx))
        return false;

    if (isConstructing() && functionThis().isPrimitive()) {
        RootedObject callee(cx, &this->callee());
        JSObject *obj = CreateThisForFunction(cx, callee,
                                              useNewType() ? SingletonObject : GenericObject);
        if (!obj)
            return false;
        functionThis() = ObjectValue(*obj);
    }

    return probes::EnterScript(cx, script, script->functionNonDelazifying(), this);
}

void
InterpreterFrame::epilogue(JSContext *cx)
{
    JS_ASSERT(!isYielding());

    RootedScript script(cx, this->script());
    probes::ExitScript(cx, script, script->functionNonDelazifying(), hasPushedSPSFrame());

    if (isEvalFrame()) {
        if (isStrictEvalFrame()) {
            JS_ASSERT_IF(hasCallObj(), scopeChain()->as<CallObject>().isForEval());
            if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
                DebugScopes::onPopStrictEvalScope(this);
        } else if (isDirectEvalFrame()) {
            if (isDebuggerFrame())
                JS_ASSERT(!scopeChain()->is<ScopeObject>());
        } else {
            /*
             * Debugger.Object.prototype.evalInGlobal creates indirect eval
             * frames scoped to the given global;
             * Debugger.Object.prototype.evalInGlobalWithBindings creates
             * indirect eval frames scoped to an object carrying the introduced
             * bindings.
             */
            if (isDebuggerFrame()) {
                JS_ASSERT(scopeChain()->is<GlobalObject>() ||
                          scopeChain()->enclosingScope()->is<GlobalObject>());
            } else {
                JS_ASSERT(scopeChain()->is<GlobalObject>());
            }
        }
        return;
    }

    if (isGlobalFrame()) {
        JS_ASSERT(!scopeChain()->is<ScopeObject>());
        return;
    }

    JS_ASSERT(isNonEvalFunctionFrame());

    if (fun()->isHeavyweight())
        JS_ASSERT_IF(hasCallObj(),
                     scopeChain()->as<CallObject>().callee().nonLazyScript() == script);
    else
        AssertDynamicScopeMatchesStaticScope(cx, script, scopeChain());

    if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
        DebugScopes::onPopCall(this, cx);

    if (isConstructing() && thisValue().isObject() && returnValue().isPrimitive())
        setReturnValue(ObjectValue(constructorThis()));
}

bool
InterpreterFrame::pushBlock(JSContext *cx, StaticBlockObject &block)
{
    JS_ASSERT (block.needsClone());

    Rooted<StaticBlockObject *> blockHandle(cx, &block);
    ClonedBlockObject *clone = ClonedBlockObject::create(cx, blockHandle, this);
    if (!clone)
        return false;

    pushOnScopeChain(*clone);

    return true;
}

void
InterpreterFrame::popBlock(JSContext *cx)
{
    JS_ASSERT(scopeChain_->is<ClonedBlockObject>());
    popOffScopeChain();
}

void
InterpreterFrame::popWith(JSContext *cx)
{
    if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
        DebugScopes::onPopWith(this);

    JS_ASSERT(scopeChain()->is<DynamicWithObject>());
    popOffScopeChain();
}

void
InterpreterFrame::mark(JSTracer *trc)
{
    /*
     * Normally we would use MarkRoot here, except that generators also take
     * this path. However, generators use a special write barrier when the stack
     * frame is copied to the floating frame. Therefore, no barrier is needed.
     */
    if (flags_ & HAS_SCOPECHAIN)
        gc::MarkObjectUnbarriered(trc, &scopeChain_, "scope chain");
    if (flags_ & HAS_ARGS_OBJ)
        gc::MarkObjectUnbarriered(trc, &argsObj_, "arguments");
    if (isFunctionFrame()) {
        gc::MarkObjectUnbarriered(trc, &exec.fun, "fun");
        if (isEvalFrame())
            gc::MarkScriptUnbarriered(trc, &u.evalScript, "eval script");
    } else {
        gc::MarkScriptUnbarriered(trc, &exec.script, "script");
    }
    if (IS_GC_MARKING_TRACER(trc))
        script()->compartment()->zone()->active = true;
    if (hasReturnValue())
        gc::MarkValueUnbarriered(trc, &rval_, "rval");
}

void
InterpreterFrame::markValues(JSTracer *trc, unsigned start, unsigned end)
{
    if (start < end)
        gc::MarkValueRootRange(trc, end - start, slots() + start, "vm_stack");
}

void
InterpreterFrame::markValues(JSTracer *trc, Value *sp, jsbytecode *pc)
{
    JS_ASSERT(sp >= slots());

    NestedScopeObject *staticScope;

    staticScope = script()->getStaticScope(pc);
    while (staticScope && !staticScope->is<StaticBlockObject>())
        staticScope = staticScope->enclosingNestedScope();

    size_t nfixed = script()->nfixed();
    size_t nlivefixed;

    if (staticScope) {
        StaticBlockObject &blockObj = staticScope->as<StaticBlockObject>();
        nlivefixed = blockObj.localOffset() + blockObj.numVariables();
    } else {
        nlivefixed = script()->nfixedvars();
    }

    if (nfixed == nlivefixed) {
        // All locals are live.
        markValues(trc, 0, sp - slots());
    } else {
        // Mark operand stack.
        markValues(trc, nfixed, sp - slots());

        // Clear dead locals.
        while (nfixed > nlivefixed)
            unaliasedLocal(--nfixed, DONT_CHECK_ALIASING).setUndefined();

        // Mark live locals.
        markValues(trc, 0, nlivefixed);
    }

    if (hasArgs()) {
        // Mark callee, |this| and arguments.
        unsigned argc = Max(numActualArgs(), numFormalArgs());
        gc::MarkValueRootRange(trc, argc + 2, argv_ - 2, "fp argv");
    } else {
        // Mark callee and |this|
        gc::MarkValueRootRange(trc, 2, ((Value *)this) - 2, "stack callee and this");
    }
}

static void
MarkInterpreterActivation(JSTracer *trc, InterpreterActivation *act)
{
    for (InterpreterFrameIterator frames(act); !frames.done(); ++frames) {
        InterpreterFrame *fp = frames.frame();
        fp->markValues(trc, frames.sp(), frames.pc());
        fp->mark(trc);
    }
}

void
js::MarkInterpreterActivations(PerThreadData *ptd, JSTracer *trc)
{
    for (ActivationIterator iter(ptd); !iter.done(); ++iter) {
        Activation *act = iter.activation();
        if (act->isInterpreter())
            MarkInterpreterActivation(trc, act->asInterpreter());
    }
}

/*****************************************************************************/

// Unlike the other methods of this calss, this method is defined here so that
// we don't have to #include jsautooplen.h in vm/Stack.h.
void
InterpreterRegs::setToEndOfScript()
{
    JSScript *script = fp()->script();
    sp = fp()->base();
    pc = script->codeEnd() - JSOP_RETRVAL_LENGTH;
    JS_ASSERT(*pc == JSOP_RETRVAL);
}

/*****************************************************************************/

InterpreterFrame *
InterpreterStack::pushInvokeFrame(JSContext *cx, const CallArgs &args, InitialFrameFlags initial)
{
    LifoAlloc::Mark mark = allocator_.mark();

    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    RootedScript script(cx, fun->nonLazyScript());

    InterpreterFrame::Flags flags = ToFrameFlags(initial);
    Value *argv;
    InterpreterFrame *fp = getCallFrame(cx, args, script, &flags, &argv);
    if (!fp)
        return nullptr;

    fp->mark_ = mark;
    fp->initCallFrame(cx, nullptr, nullptr, nullptr, *fun, script, argv, args.length(), flags);
    return fp;
}

InterpreterFrame *
InterpreterStack::pushExecuteFrame(JSContext *cx, HandleScript script, const Value &thisv,
                                   HandleObject scopeChain, ExecuteType type,
                                   AbstractFramePtr evalInFrame)
{
    LifoAlloc::Mark mark = allocator_.mark();

    unsigned nvars = 2 /* callee, this */ + script->nslots();
    uint8_t *buffer = allocateFrame(cx, sizeof(InterpreterFrame) + nvars * sizeof(Value));
    if (!buffer)
        return nullptr;

    InterpreterFrame *fp = reinterpret_cast<InterpreterFrame *>(buffer + 2 * sizeof(Value));
    fp->mark_ = mark;
    fp->initExecuteFrame(cx, script, evalInFrame, thisv, *scopeChain, type);
    fp->initVarsToUndefined();

    return fp;
}

/*****************************************************************************/

/* MSVC PGO causes xpcshell startup crashes. */
#if defined(_MSC_VER)
# pragma optimize("g", off)
#endif

void
FrameIter::popActivation()
{
    ++data_.activations_;
    settleOnActivation();
}

void
FrameIter::popInterpreterFrame()
{
    JS_ASSERT(data_.state_ == INTERP);

    ++data_.interpFrames_;

    if (data_.interpFrames_.done())
        popActivation();
    else
        data_.pc_ = data_.interpFrames_.pc();
}

void
FrameIter::settleOnActivation()
{
    while (true) {
        if (data_.activations_.done()) {
            data_.state_ = DONE;
            return;
        }

        Activation *activation = data_.activations_.activation();

        // If JS_SaveFrameChain was called, stop iterating here (unless
        // GO_THROUGH_SAVED is set).
        if (data_.savedOption_ == STOP_AT_SAVED && activation->hasSavedFrameChain()) {
            data_.state_ = DONE;
            return;
        }

        // Skip activations from another context if needed.
        JS_ASSERT(activation->cx());
        JS_ASSERT(data_.cx_);
        if (data_.contextOption_ == CURRENT_CONTEXT && activation->cx() != data_.cx_) {
            ++data_.activations_;
            continue;
        }

        // If the caller supplied principals, only show activations which are subsumed (of the same
        // origin or of an origin accessible) by these principals.
        if (data_.principals_) {
            JSContext *cx = data_.cx_->asJSContext();
            if (JSSubsumesOp subsumes = cx->runtime()->securityCallbacks->subsumes) {
                if (!subsumes(data_.principals_, activation->compartment()->principals)) {
                    ++data_.activations_;
                    continue;
                }
            }
        }

        if (activation->isJit()) {
            data_.jitFrames_ = jit::JitFrameIterator(data_.activations_);

            // Stop at the first scripted frame.
            while (!data_.jitFrames_.isScripted() && !data_.jitFrames_.done())
                ++data_.jitFrames_;

            // It's possible to have an JitActivation with no scripted frames,
            // for instance if we hit an over-recursion during bailout.
            if (data_.jitFrames_.done()) {
                ++data_.activations_;
                continue;
            }

            nextJitFrame();
            data_.state_ = JIT;
            return;
        }

        if (activation->isAsmJS()) {
            data_.asmJSFrames_ = AsmJSFrameIterator(*data_.activations_->asAsmJS());

            if (data_.asmJSFrames_.done()) {
                ++data_.activations_;
                continue;
            }

            data_.state_ = ASMJS;
            return;
        }

        // ForkJoin activations don't contain iterable frames, so skip them.
        if (activation->isForkJoin()) {
            ++data_.activations_;
            continue;
        }

        JS_ASSERT(activation->isInterpreter());

        InterpreterActivation *interpAct = activation->asInterpreter();
        data_.interpFrames_ = InterpreterFrameIterator(interpAct);

        // If we OSR'ed into JIT code, skip the interpreter frame so that
        // the same frame is not reported twice.
        if (data_.interpFrames_.frame()->runningInJit()) {
            ++data_.interpFrames_;
            if (data_.interpFrames_.done()) {
                ++data_.activations_;
                continue;
            }
        }

        JS_ASSERT(!data_.interpFrames_.frame()->runningInJit());
        data_.pc_ = data_.interpFrames_.pc();
        data_.state_ = INTERP;
        return;
    }
}

FrameIter::Data::Data(ThreadSafeContext *cx, SavedOption savedOption,
                      ContextOption contextOption, JSPrincipals *principals)
  : cx_(cx),
    savedOption_(savedOption),
    contextOption_(contextOption),
    principals_(principals),
    pc_(nullptr),
    interpFrames_(nullptr),
    activations_(cx->perThreadData),
    jitFrames_((uint8_t *)nullptr, SequentialExecution),
    ionInlineFrameNo_(0),
    asmJSFrames_()
{
}

FrameIter::Data::Data(const FrameIter::Data &other)
  : cx_(other.cx_),
    savedOption_(other.savedOption_),
    contextOption_(other.contextOption_),
    principals_(other.principals_),
    state_(other.state_),
    pc_(other.pc_),
    interpFrames_(other.interpFrames_),
    activations_(other.activations_),
    jitFrames_(other.jitFrames_),
    ionInlineFrameNo_(other.ionInlineFrameNo_),
    asmJSFrames_(other.asmJSFrames_)
{
}

FrameIter::FrameIter(ThreadSafeContext *cx, SavedOption savedOption)
  : data_(cx, savedOption, CURRENT_CONTEXT, nullptr),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    // settleOnActivation can only GC if principals are given.
    JS::AutoSuppressGCAnalysis nogc;
    settleOnActivation();
}

FrameIter::FrameIter(ThreadSafeContext *cx, ContextOption contextOption,
                     SavedOption savedOption)
  : data_(cx, savedOption, contextOption, nullptr),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    // settleOnActivation can only GC if principals are given.
    JS::AutoSuppressGCAnalysis nogc;
    settleOnActivation();
}

FrameIter::FrameIter(JSContext *cx, ContextOption contextOption,
                     SavedOption savedOption, JSPrincipals *principals)
  : data_(cx, savedOption, contextOption, principals),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    settleOnActivation();
}

FrameIter::FrameIter(const FrameIter &other)
  : data_(other.data_),
    ionInlineFrames_(other.data_.cx_,
                     data_.jitFrames_.isIonJS() ? &other.ionInlineFrames_ : nullptr)
{
}

FrameIter::FrameIter(const Data &data)
  : data_(data),
    ionInlineFrames_(data.cx_, data_.jitFrames_.isIonJS() ? &data_.jitFrames_ : nullptr)
{
    JS_ASSERT(data.cx_);

    if (data_.jitFrames_.isIonJS()) {
        while (ionInlineFrames_.frameNo() != data.ionInlineFrameNo_)
            ++ionInlineFrames_;
    }
}

void
FrameIter::nextJitFrame()
{
    if (data_.jitFrames_.isIonJS()) {
        ionInlineFrames_.resetOn(&data_.jitFrames_);
        data_.pc_ = ionInlineFrames_.pc();
    } else {
        JS_ASSERT(data_.jitFrames_.isBaselineJS());
        data_.jitFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
    }
}

void
FrameIter::popJitFrame()
{
    JS_ASSERT(data_.state_ == JIT);

    if (data_.jitFrames_.isIonJS() && ionInlineFrames_.more()) {
        ++ionInlineFrames_;
        data_.pc_ = ionInlineFrames_.pc();
        return;
    }

    ++data_.jitFrames_;
    while (!data_.jitFrames_.done() && !data_.jitFrames_.isScripted())
        ++data_.jitFrames_;

    if (!data_.jitFrames_.done()) {
        nextJitFrame();
        return;
    }

    popActivation();
}

void
FrameIter::popAsmJSFrame()
{
    JS_ASSERT(data_.state_ == ASMJS);

    ++data_.asmJSFrames_;
    if (data_.asmJSFrames_.done())
        popActivation();
}

FrameIter &
FrameIter::operator++()
{
    switch (data_.state_) {
      case DONE:
        MOZ_CRASH("Unexpected state");
      case INTERP:
        if (interpFrame()->isDebuggerFrame() && interpFrame()->evalInFramePrev()) {
            AbstractFramePtr eifPrev = interpFrame()->evalInFramePrev();
            MOZ_ASSERT(!eifPrev.isRematerializedFrame());

            // Eval-in-frame can cross contexts and works across saved frame
            // chains.
            ContextOption prevContextOption = data_.contextOption_;
            SavedOption prevSavedOption = data_.savedOption_;
            data_.contextOption_ = ALL_CONTEXTS;
            data_.savedOption_ = GO_THROUGH_SAVED;

            popInterpreterFrame();

            while (isIon() || abstractFramePtr() != eifPrev) {
                if (data_.state_ == JIT)
                    popJitFrame();
                else
                    popInterpreterFrame();
            }

            data_.contextOption_ = prevContextOption;
            data_.savedOption_ = prevSavedOption;
            data_.cx_ = data_.activations_->cx();
            break;
        }
        popInterpreterFrame();
        break;
      case JIT:
        popJitFrame();
        break;
      case ASMJS:
        popAsmJSFrame();
        break;
    }
    return *this;
}

FrameIter::Data *
FrameIter::copyData() const
{
    Data *data = data_.cx_->new_<Data>(data_);
    JS_ASSERT(data_.state_ != ASMJS);
    if (data && data_.jitFrames_.isIonJS())
        data->ionInlineFrameNo_ = ionInlineFrames_.frameNo();
    return data;
}

AbstractFramePtr
FrameIter::copyDataAsAbstractFramePtr() const
{
    AbstractFramePtr frame;
    if (Data *data = copyData())
        frame.ptr_ = uintptr_t(data);
    return frame;
}

JSCompartment *
FrameIter::compartment() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
      case ASMJS:
        return data_.activations_->compartment();
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isFunctionFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isFunctionFrame();
      case JIT:
        JS_ASSERT(data_.jitFrames_.isScripted());
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.isFunctionFrame();
        return ionInlineFrames_.isFunctionFrame();
      case ASMJS:
        return true;
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isGlobalFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isGlobalFrame();
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame()->isGlobalFrame();
        JS_ASSERT(!script()->isForEval());
        return !script()->functionNonDelazifying();
      case ASMJS:
        return false;
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isEvalFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isEvalFrame();
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame()->isEvalFrame();
        JS_ASSERT(!script()->isForEval());
        return false;
      case ASMJS:
        return false;
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isNonEvalFunctionFrame() const
{
    JS_ASSERT(!done());
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isNonEvalFunctionFrame();
      case JIT:
        return !isEvalFrame() && isFunctionFrame();
      case ASMJS:
        return true;
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isGeneratorFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isGeneratorFrame();
      case JIT:
        return false;
      case ASMJS:
        return false;
    }
    MOZ_CRASH("Unexpected state");
}

JSAtom *
FrameIter::functionDisplayAtom() const
{
    JS_ASSERT(isNonEvalFunctionFrame());

    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return callee()->displayAtom();
      case ASMJS:
        return data_.asmJSFrames_.functionDisplayAtom();
    }

    MOZ_CRASH("Unexpected state");
}

ScriptSource *
FrameIter::scriptSource() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return script()->scriptSource();
      case ASMJS:
        return data_.activations_->asAsmJS()->module().scriptSource();
    }

    MOZ_CRASH("Unexpected state");
}

const char *
FrameIter::scriptFilename() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return script()->filename();
      case ASMJS:
        return data_.activations_->asAsmJS()->module().scriptSource()->filename();
    }

    MOZ_CRASH("Unexpected state");
}

unsigned
FrameIter::computeLine(uint32_t *column) const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return PCToLineNumber(script(), pc(), column);
      case ASMJS:
        return data_.asmJSFrames_.computeLine(column);
    }

    MOZ_CRASH("Unexpected state");
}

JSPrincipals *
FrameIter::originPrincipals() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return script()->originPrincipals();
      case ASMJS:
        return data_.activations_->asAsmJS()->module().scriptSource()->originPrincipals();
    }

    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isConstructing() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isIonJS())
            return ionInlineFrames_.isConstructing();
        JS_ASSERT(data_.jitFrames_.isBaselineJS());
        return data_.jitFrames_.isConstructing();
      case INTERP:
        return interpFrame()->isConstructing();
    }

    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::ensureHasRematerializedFrame(ThreadSafeContext *cx)
{
    MOZ_ASSERT(isIon());
    return !!activation()->asJit()->getRematerializedFrame(cx, data_.jitFrames_);
}

bool
FrameIter::hasUsableAbstractFramePtr() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        return false;
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return true;

        MOZ_ASSERT(data_.jitFrames_.isIonJS());
        return !!activation()->asJit()->lookupRematerializedFrame(data_.jitFrames_.fp(),
                                                                  ionInlineFrames_.frameNo());
        break;
      case INTERP:
        return true;
    }
    MOZ_CRASH("Unexpected state");
}

AbstractFramePtr
FrameIter::abstractFramePtr() const
{
    MOZ_ASSERT(hasUsableAbstractFramePtr());
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT: {
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame();

        MOZ_ASSERT(data_.jitFrames_.isIonJS());
        return activation()->asJit()->lookupRematerializedFrame(data_.jitFrames_.fp(),
                                                                ionInlineFrames_.frameNo());
        break;
      }
      case INTERP:
        JS_ASSERT(interpFrame());
        return AbstractFramePtr(interpFrame());
    }
    MOZ_CRASH("Unexpected state");
}

void
FrameIter::updatePcQuadratic()
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case INTERP: {
        InterpreterFrame *frame = interpFrame();
        InterpreterActivation *activation = data_.activations_->asInterpreter();

        // Look for the current frame.
        data_.interpFrames_ = InterpreterFrameIterator(activation);
        while (data_.interpFrames_.frame() != frame)
            ++data_.interpFrames_;

        // Update the pc.
        JS_ASSERT(data_.interpFrames_.frame() == frame);
        data_.pc_ = data_.interpFrames_.pc();
        return;
      }
      case JIT:
        if (data_.jitFrames_.isBaselineJS()) {
            jit::BaselineFrame *frame = data_.jitFrames_.baselineFrame();
            jit::JitActivation *activation = data_.activations_->asJit();

            // ActivationIterator::jitTop_ may be invalid, so create a new
            // activation iterator.
            data_.activations_ = ActivationIterator(data_.cx_->perThreadData);
            while (data_.activations_.activation() != activation)
                ++data_.activations_;

            // Look for the current frame.
            data_.jitFrames_ = jit::JitFrameIterator(data_.activations_);
            while (!data_.jitFrames_.isBaselineJS() || data_.jitFrames_.baselineFrame() != frame)
                ++data_.jitFrames_;

            // Update the pc.
            JS_ASSERT(data_.jitFrames_.baselineFrame() == frame);
            data_.jitFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
            return;
        }
        break;
    }
    MOZ_CRASH("Unexpected state");
}

JSFunction *
FrameIter::callee() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case INTERP:
        JS_ASSERT(isFunctionFrame());
        return &interpFrame()->callee();
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.callee();
        JS_ASSERT(data_.jitFrames_.isIonJS());
        return ionInlineFrames_.callee();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::calleev() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case INTERP:
        JS_ASSERT(isFunctionFrame());
        return interpFrame()->calleev();
      case JIT:
        return ObjectValue(*callee());
    }
    MOZ_CRASH("Unexpected state");
}

unsigned
FrameIter::numActualArgs() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case INTERP:
        JS_ASSERT(isFunctionFrame());
        return interpFrame()->numActualArgs();
      case JIT:
        if (data_.jitFrames_.isIonJS())
            return ionInlineFrames_.numActualArgs();

        JS_ASSERT(data_.jitFrames_.isBaselineJS());
        return data_.jitFrames_.numActualArgs();
    }
    MOZ_CRASH("Unexpected state");
}

unsigned
FrameIter::numFormalArgs() const
{
    return script()->functionNonDelazifying()->nargs();
}

Value
FrameIter::unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing) const
{
    return abstractFramePtr().unaliasedActual(i, checkAliasing);
}

JSObject *
FrameIter::scopeChain() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isIonJS())
            return ionInlineFrames_.scopeChain();
        return data_.jitFrames_.baselineFrame()->scopeChain();
      case INTERP:
        return interpFrame()->scopeChain();
    }
    MOZ_CRASH("Unexpected state");
}

CallObject &
FrameIter::callObj() const
{
    JS_ASSERT(callee()->isHeavyweight());

    JSObject *pobj = scopeChain();
    while (!pobj->is<CallObject>())
        pobj = pobj->enclosingScope();
    return pobj->as<CallObject>();
}

bool
FrameIter::hasArgsObj() const
{
    return abstractFramePtr().hasArgsObj();
}

ArgumentsObject &
FrameIter::argsObj() const
{
    MOZ_ASSERT(hasArgsObj());
    return abstractFramePtr().argsObj();
}

bool
FrameIter::computeThis(JSContext *cx) const
{
    JS_ASSERT(!done() && !isAsmJS());
    assertSameCompartment(cx, scopeChain());
    return ComputeThis(cx, abstractFramePtr());
}

Value
FrameIter::computedThisValue() const
{
    return abstractFramePtr().thisValue();
}

Value
FrameIter::thisv() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isIonJS())
            return ionInlineFrames_.thisValue();
        return data_.jitFrames_.baselineFrame()->thisValue();
      case INTERP:
        return interpFrame()->thisValue();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::returnValue() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame()->returnValue();
        break;
      case INTERP:
        return interpFrame()->returnValue();
    }
    MOZ_CRASH("Unexpected state");
}

void
FrameIter::setReturnValue(const Value &v)
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isBaselineJS()) {
            data_.jitFrames_.baselineFrame()->setReturnValue(v);
            return;
        }
        break;
      case INTERP:
        interpFrame()->setReturnValue(v);
        return;
    }
    MOZ_CRASH("Unexpected state");
}

size_t
FrameIter::numFrameSlots() const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT: {
        if (data_.jitFrames_.isIonJS()) {
            return ionInlineFrames_.snapshotIterator().numAllocations() -
                ionInlineFrames_.script()->nfixed();
        }
        jit::BaselineFrame *frame = data_.jitFrames_.baselineFrame();
        return frame->numValueSlots() - data_.jitFrames_.script()->nfixed();
      }
      case INTERP:
        JS_ASSERT(data_.interpFrames_.sp() >= interpFrame()->base());
        return data_.interpFrames_.sp() - interpFrame()->base();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::frameSlotValue(size_t index) const
{
    switch (data_.state_) {
      case DONE:
      case ASMJS:
        break;
      case JIT:
        if (data_.jitFrames_.isIonJS()) {
            jit::SnapshotIterator si(ionInlineFrames_.snapshotIterator());
            index += ionInlineFrames_.script()->nfixed();
            return si.maybeReadAllocByIndex(index);
        }

        index += data_.jitFrames_.script()->nfixed();
        return *data_.jitFrames_.baselineFrame()->valueSlot(index);
      case INTERP:
          return interpFrame()->base()[index];
    }
    MOZ_CRASH("Unexpected state");
}

#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

#ifdef DEBUG
bool
js::SelfHostedFramesVisible()
{
    static bool checked = false;
    static bool visible = false;
    if (!checked) {
        checked = true;
        char *env = getenv("MOZ_SHOW_ALL_JS_FRAMES");
        visible = !!env;
    }
    return visible;
}
#endif

void
NonBuiltinFrameIter::settle()
{
    if (!SelfHostedFramesVisible()) {
        while (!done() && hasScript() && script()->selfHosted())
            FrameIter::operator++();
    }
}

void
NonBuiltinScriptFrameIter::settle()
{
    if (!SelfHostedFramesVisible()) {
        while (!done() && script()->selfHosted())
            ScriptFrameIter::operator++();
    }
}

/*****************************************************************************/

JSObject *
AbstractFramePtr::evalPrevScopeChain(JSContext *cx) const
{
    // Eval frames are not compiled by Ion, though their caller might be.
    AllFramesIter iter(cx);
    while (iter.isIon() || iter.abstractFramePtr() != *this)
        ++iter;
    ++iter;
    return iter.scopeChain();
}

bool
AbstractFramePtr::hasPushedSPSFrame() const
{
    if (isInterpreterFrame())
        return asInterpreterFrame()->hasPushedSPSFrame();
    return asBaselineFrame()->hasPushedSPSFrame();
}

#ifdef DEBUG
void
js::CheckLocalUnaliased(MaybeCheckAliasing checkAliasing, JSScript *script, uint32_t i)
{
    if (!checkAliasing)
        return;

    JS_ASSERT(i < script->nfixed());
    if (i < script->bindings.numVars()) {
        JS_ASSERT(!script->varIsAliased(i));
    } else {
        // FIXME: The callers of this function do not easily have the PC of the
        // current frame, and so they do not know the block scope.
    }
}
#endif

jit::JitActivation::JitActivation(JSContext *cx, bool firstFrameIsConstructing, bool active)
  : Activation(cx, Jit),
    firstFrameIsConstructing_(firstFrameIsConstructing),
    active_(active),
    rematerializedFrames_(nullptr)
{
    if (active) {
        prevJitTop_ = cx->mainThread().jitTop;
        prevJitJSContext_ = cx->mainThread().jitJSContext;
        cx->mainThread().jitJSContext = cx;
    } else {
        prevJitTop_ = nullptr;
        prevJitJSContext_ = nullptr;
    }
}

jit::JitActivation::JitActivation(ForkJoinContext *cx)
  : Activation(cx, Jit),
    firstFrameIsConstructing_(false),
    active_(true),
    rematerializedFrames_(nullptr)
{
    prevJitTop_ = cx->perThreadData->jitTop;
    prevJitJSContext_ = cx->perThreadData->jitJSContext;
    cx->perThreadData->jitJSContext = nullptr;
}

jit::JitActivation::~JitActivation()
{
    if (active_) {
        cx_->perThreadData->jitTop = prevJitTop_;
        cx_->perThreadData->jitJSContext = prevJitJSContext_;
    }

    clearRematerializedFrames();
    js_delete(rematerializedFrames_);
}

// setActive() is inlined in GenerateFFIIonExit() with explicit masm instructions so
// changes to the logic here need to be reflected in GenerateFFIIonExit() in the enable
// and disable activation instruction sequences.
void
jit::JitActivation::setActive(JSContext *cx, bool active)
{
    // Only allowed to deactivate/activate if activation is top.
    // (Not tested and will probably fail in other situations.)
    JS_ASSERT(cx->mainThread().activation_ == this);
    JS_ASSERT(active != active_);
    active_ = active;

    if (active) {
        prevJitTop_ = cx->mainThread().jitTop;
        prevJitJSContext_ = cx->mainThread().jitJSContext;
        cx->mainThread().jitJSContext = cx;
    } else {
        cx->mainThread().jitTop = prevJitTop_;
        cx->mainThread().jitJSContext = prevJitJSContext_;
    }
}

void
jit::JitActivation::removeRematerializedFrame(uint8_t *top)
{
    if (!rematerializedFrames_)
        return;

    if (RematerializedFrameTable::Ptr p = rematerializedFrames_->lookup(top)) {
        RematerializedFrame::FreeInVector(p->value());
        rematerializedFrames_->remove(p);
    }
}

void
jit::JitActivation::clearRematerializedFrames()
{
    if (!rematerializedFrames_)
        return;

    for (RematerializedFrameTable::Enum e(*rematerializedFrames_); !e.empty(); e.popFront()) {
        RematerializedFrame::FreeInVector(e.front().value());
        e.removeFront();
    }
}

template <class T>
jit::RematerializedFrame *
jit::JitActivation::getRematerializedFrame(ThreadSafeContext *cx, const T &iter, size_t inlineDepth)
{
    // Only allow rematerializing from the same thread.
    MOZ_ASSERT(cx->perThreadData == cx_->perThreadData);
    MOZ_ASSERT(iter.activation() == this);
    MOZ_ASSERT(iter.isIonJS());

    if (!rematerializedFrames_) {
        rematerializedFrames_ = cx->new_<RematerializedFrameTable>(cx);
        if (!rematerializedFrames_ || !rematerializedFrames_->init()) {
            rematerializedFrames_ = nullptr;
            return nullptr;
        }
    }

    uint8_t *top = iter.fp();
    RematerializedFrameTable::AddPtr p = rematerializedFrames_->lookupForAdd(top);
    if (!p) {
        RematerializedFrameVector empty(cx);
        if (!rematerializedFrames_->add(p, top, Move(empty)))
            return nullptr;

        // The unit of rematerialization is an uninlined frame and its inlined
        // frames. Since inlined frames do not exist outside of snapshots, it
        // is impossible to synchronize their rematerialized copies to
        // preserve identity. Therefore, we always rematerialize an uninlined
        // frame and all its inlined frames at once.
        InlineFrameIterator inlineIter(cx, &iter);
        if (!RematerializedFrame::RematerializeInlineFrames(cx, top, inlineIter, p->value()))
            return nullptr;
    }

    return p->value()[inlineDepth];
}

template jit::RematerializedFrame *
jit::JitActivation::getRematerializedFrame<jit::JitFrameIterator>(ThreadSafeContext *cx,
                                                                  const jit::JitFrameIterator &iter,
                                                                  size_t inlineDepth);
template jit::RematerializedFrame *
jit::JitActivation::getRematerializedFrame<jit::IonBailoutIterator>(ThreadSafeContext *cx,
                                                                    const jit::IonBailoutIterator &iter,
                                                                    size_t inlineDepth);

jit::RematerializedFrame *
jit::JitActivation::lookupRematerializedFrame(uint8_t *top, size_t inlineDepth)
{
    if (!rematerializedFrames_)
        return nullptr;
    if (RematerializedFrameTable::Ptr p = rematerializedFrames_->lookup(top))
        return inlineDepth < p->value().length() ? p->value()[inlineDepth] : nullptr;
    return nullptr;
}

void
jit::JitActivation::markRematerializedFrames(JSTracer *trc)
{
    if (!rematerializedFrames_)
        return;
    for (RematerializedFrameTable::Enum e(*rematerializedFrames_); !e.empty(); e.popFront())
        RematerializedFrame::MarkInVector(trc, e.front().value());
}

AsmJSActivation::AsmJSActivation(JSContext *cx, AsmJSModule &module)
  : Activation(cx, AsmJS),
    module_(module),
    errorRejoinSP_(nullptr),
    profiler_(nullptr),
    resumePC_(nullptr),
    fp_(nullptr),
    exitReason_(AsmJSExit::None)
{
    if (cx->runtime()->spsProfiler.enabled()) {
        // Use a profiler string that matches jsMatch regex in
        // browser/devtools/profiler/cleopatra/js/parserWorker.js.
        // (For now use a single static string to avoid further slowing down
        // calls into asm.js.)
        profiler_ = &cx->runtime()->spsProfiler;
        profiler_->enterAsmJS("asm.js code :0", this);
    }

    prevAsmJSForModule_ = module.activation();
    module.activation() = this;

    prevAsmJS_ = cx->mainThread().asmJSActivationStack_;

    JSRuntime::AutoLockForInterrupt lock(cx->runtime());
    cx->mainThread().asmJSActivationStack_ = this;

    (void) errorRejoinSP_;  // squelch GCC warning
}

AsmJSActivation::~AsmJSActivation()
{
    if (profiler_)
        profiler_->exitAsmJS();

    JS_ASSERT(fp_ == nullptr);

    JS_ASSERT(module_.activation() == this);
    module_.activation() = prevAsmJSForModule_;

    JSContext *cx = cx_->asJSContext();
    JS_ASSERT(cx->mainThread().asmJSActivationStack_ == this);

    JSRuntime::AutoLockForInterrupt lock(cx->runtime());
    cx->mainThread().asmJSActivationStack_ = prevAsmJS_;
}

InterpreterFrameIterator &
InterpreterFrameIterator::operator++()
{
    JS_ASSERT(!done());
    if (fp_ != activation_->entryFrame_) {
        pc_ = fp_->prevpc();
        sp_ = fp_->prevsp();
        fp_ = fp_->prev();
    } else {
        pc_ = nullptr;
        sp_ = nullptr;
        fp_ = nullptr;
    }
    return *this;
}

void
Activation::registerProfiling()
{
    JS_ASSERT(isProfiling());
    JSRuntime::AutoLockForInterrupt lock(cx_->asJSContext()->runtime());
    cx_->perThreadData->profilingActivation_ = this;
}

void
Activation::unregisterProfiling()
{
    JS_ASSERT(isProfiling());
    JSRuntime::AutoLockForInterrupt lock(cx_->asJSContext()->runtime());
    JS_ASSERT(cx_->perThreadData->profilingActivation_ == this);
    cx_->perThreadData->profilingActivation_ = prevProfiling_;
}

ActivationIterator::ActivationIterator(JSRuntime *rt)
  : jitTop_(rt->mainThread.jitTop),
    activation_(rt->mainThread.activation_)
{
    settle();
}

ActivationIterator::ActivationIterator(PerThreadData *perThreadData)
  : jitTop_(perThreadData->jitTop),
    activation_(perThreadData->activation_)
{
    settle();
}

ActivationIterator &
ActivationIterator::operator++()
{
    JS_ASSERT(activation_);
    if (activation_->isJit() && activation_->asJit()->isActive())
        jitTop_ = activation_->asJit()->prevJitTop();
    activation_ = activation_->prev();
    settle();
    return *this;
}

void
ActivationIterator::settle()
{
    // Stop at the next active activation. No need to update jitTop_, since
    // we don't iterate over an active jit activation.
    while (!done() && activation_->isJit() && !activation_->asJit()->isActive())
        activation_ = activation_->prev();
}

JS::ProfilingFrameIterator::ProfilingFrameIterator(JSRuntime *rt, const RegisterState &state)
  : activation_(rt->mainThread.profilingActivation())
{
    if (!activation_)
        return;

    JS_ASSERT(activation_->isProfiling());

    static_assert(sizeof(AsmJSProfilingFrameIterator) <= StorageSpace, "Need to increase storage");

    iteratorConstruct(state);
    settle();
}

JS::ProfilingFrameIterator::~ProfilingFrameIterator()
{
    if (!done()) {
        JS_ASSERT(activation_->isProfiling());
        iteratorDestroy();
    }
}

void
JS::ProfilingFrameIterator::operator++()
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    ++asmJSIter();
    settle();
}

void
JS::ProfilingFrameIterator::settle()
{
    while (iteratorDone()) {
        iteratorDestroy();
        activation_ = activation_->prevProfiling();
        if (!activation_)
            return;
        iteratorConstruct();
    }
}

void
JS::ProfilingFrameIterator::iteratorConstruct(const RegisterState &state)
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    new (storage_.addr()) AsmJSProfilingFrameIterator(*activation_->asAsmJS(), state);
}

void
JS::ProfilingFrameIterator::iteratorConstruct()
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    new (storage_.addr()) AsmJSProfilingFrameIterator(*activation_->asAsmJS());
}

void
JS::ProfilingFrameIterator::iteratorDestroy()
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    asmJSIter().~AsmJSProfilingFrameIterator();
}

bool
JS::ProfilingFrameIterator::iteratorDone()
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    return asmJSIter().done();
}

void *
JS::ProfilingFrameIterator::stackAddress() const
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    return asmJSIter().stackAddress();
}

const char *
JS::ProfilingFrameIterator::label() const
{
    JS_ASSERT(!done());

    JS_ASSERT(activation_->isAsmJS());
    return asmJSIter().label();
}
