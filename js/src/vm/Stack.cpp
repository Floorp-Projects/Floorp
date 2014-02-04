/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Stack-inl.h"

#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "gc/Marking.h"
#ifdef JS_ION
#include "jit/BaselineFrame.h"
#include "jit/JitCompartment.h"
#endif
#include "vm/Opcodes.h"

#include "jit/IonFrameIterator-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Probes-inl.h"
#include "vm/ScopeObject-inl.h"

using namespace js;

using mozilla::PodCopy;

/*****************************************************************************/

void
StackFrame::initExecuteFrame(JSContext *cx, JSScript *script, AbstractFramePtr evalInFramePrev,
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
            ScriptFrameIter iter(cx);
            JS_ASSERT(iter.isFunctionFrame() || iter.isGlobalFrame());
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
    hookData_ = (void *)0xbad;
#endif
}

template <StackFrame::TriggerPostBarriers doPostBarrier>
void
StackFrame::copyFrameAndValues(JSContext *cx, Value *vp, StackFrame *otherfp,
                               const Value *othervp, Value *othersp)
{
    JS_ASSERT(othervp == otherfp->generatorArgsSnapshotBegin());
    JS_ASSERT(othersp >= otherfp->slots());
    JS_ASSERT(othersp <= otherfp->generatorSlotsSnapshotBegin() + otherfp->script()->nslots());

    /* Copy args, StackFrame, and slots. */
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
void StackFrame::copyFrameAndValues<StackFrame::NoPostBarrier>(
                                    JSContext *, Value *, StackFrame *, const Value *, Value *);
template
void StackFrame::copyFrameAndValues<StackFrame::DoPostBarrier>(
                                    JSContext *, Value *, StackFrame *, const Value *, Value *);

void
StackFrame::writeBarrierPost()
{
    /* This needs to follow the same rules as in StackFrame::mark. */
    if (scopeChain_)
        JSObject::writeBarrierPost(scopeChain_, (void *)&scopeChain_);
    if (flags_ & HAS_ARGS_OBJ)
        JSObject::writeBarrierPost(argsObj_, (void *)&argsObj_);
    if (isFunctionFrame()) {
        JSFunction::writeBarrierPost(exec.fun, (void *)&exec.fun);
        if (isEvalFrame())
            JSScript::writeBarrierPost(u.evalScript, (void *)&u.evalScript);
    } else {
        JSScript::writeBarrierPost(exec.script, (void *)&exec.script);
    }
    if (hasReturnValue())
        HeapValue::writeBarrierPost(rval_, &rval_);
}

bool
StackFrame::copyRawFrameSlots(AutoValueVector *vec)
{
    if (!vec->resize(numFormalArgs() + script()->nfixed()))
        return false;
    PodCopy(vec->begin(), argv(), numFormalArgs());
    PodCopy(vec->begin() + numFormalArgs(), slots(), script()->nfixed());
    return true;
}

JSObject *
StackFrame::createRestParameter(JSContext *cx)
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
StackFrame::initFunctionScopeObjects(JSContext *cx)
{
    CallObject *callobj = CallObject::createForFunction(cx, this);
    if (!callobj)
        return false;
    pushOnScopeChain(*callobj);
    flags_ |= HAS_CALL_OBJ;
    return true;
}

bool
StackFrame::prologue(JSContext *cx)
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
        probes::EnterScript(cx, script, nullptr, this);
        return true;
    }

    if (isGlobalFrame()) {
        probes::EnterScript(cx, script, nullptr, this);
        return true;
    }

    JS_ASSERT(isNonEvalFunctionFrame());
    AssertDynamicScopeMatchesStaticScope(cx, script, scopeChain());

    if (fun()->isHeavyweight() && !initFunctionScopeObjects(cx))
        return false;

    if (isConstructing()) {
        RootedObject callee(cx, &this->callee());
        JSObject *obj = CreateThisForFunction(cx, callee,
                                              useNewType() ? SingletonObject : GenericObject);
        if (!obj)
            return false;
        functionThis() = ObjectValue(*obj);
    }

    probes::EnterScript(cx, script, script->functionNonDelazifying(), this);
    return true;
}

void
StackFrame::epilogue(JSContext *cx)
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
StackFrame::pushBlock(JSContext *cx, StaticBlockObject &block)
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
StackFrame::popBlock(JSContext *cx)
{
    JS_ASSERT(scopeChain_->is<ClonedBlockObject>());
    popOffScopeChain();
}

void
StackFrame::popWith(JSContext *cx)
{
    if (MOZ_UNLIKELY(cx->compartment()->debugMode()))
        DebugScopes::onPopWith(this);

    JS_ASSERT(scopeChain()->is<DynamicWithObject>());
    popOffScopeChain();
}

void
StackFrame::mark(JSTracer *trc)
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
    gc::MarkValueUnbarriered(trc, returnValue().address(), "rval");
}

void
StackFrame::markValues(JSTracer *trc, Value *sp)
{
    JS_ASSERT(sp >= slots());
    gc::MarkValueRootRange(trc, sp - slots(), slots(), "vm_stack");
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
        StackFrame *fp = frames.frame();
        fp->markValues(trc, frames.sp());
        fp->mark(trc);
    }
}

void
js::MarkInterpreterActivations(JSRuntime *rt, JSTracer *trc)
{
    for (ActivationIterator iter(rt); !iter.done(); ++iter) {
        Activation *act = iter.activation();
        if (act->isInterpreter())
            MarkInterpreterActivation(trc, act->asInterpreter());
    }

}

/*****************************************************************************/

// Unlike the other methods of this calss, this method is defined here so that
// we don't have to #include jsautooplen.h in vm/Stack.h.
void
FrameRegs::setToEndOfScript()
{
    JSScript *script = fp()->script();
    sp = fp()->base();
    pc = script->codeEnd() - JSOP_RETRVAL_LENGTH;
    JS_ASSERT(*pc == JSOP_RETRVAL);
}

/*****************************************************************************/

StackFrame *
InterpreterStack::pushInvokeFrame(JSContext *cx, const CallArgs &args, InitialFrameFlags initial)
{
    LifoAlloc::Mark mark = allocator_.mark();

    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    RootedScript script(cx, fun->nonLazyScript());

    StackFrame::Flags flags = ToFrameFlags(initial);
    Value *argv;
    StackFrame *fp = getCallFrame(cx, args, script, &flags, &argv);
    if (!fp)
        return nullptr;

    fp->mark_ = mark;
    fp->initCallFrame(cx, nullptr, nullptr, nullptr, *fun, script, argv, args.length(), flags);
    return fp;
}

StackFrame *
InterpreterStack::pushExecuteFrame(JSContext *cx, HandleScript script, const Value &thisv,
                                   HandleObject scopeChain, ExecuteType type,
                                   AbstractFramePtr evalInFrame)
{
    LifoAlloc::Mark mark = allocator_.mark();

    unsigned nvars = 2 /* callee, this */ + script->nslots();
    uint8_t *buffer = allocateFrame(cx, sizeof(StackFrame) + nvars * sizeof(Value));
    if (!buffer)
        return nullptr;

    StackFrame *fp = reinterpret_cast<StackFrame *>(buffer + 2 * sizeof(Value));
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
ScriptFrameIter::popActivation()
{
    ++data_.activations_;
    settleOnActivation();
}

void
ScriptFrameIter::popInterpreterFrame()
{
    JS_ASSERT(data_.state_ == SCRIPTED);

    ++data_.interpFrames_;

    if (data_.interpFrames_.done())
        popActivation();
    else
        data_.pc_ = data_.interpFrames_.pc();
}

void
ScriptFrameIter::settleOnActivation()
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
            if (JSSubsumesOp subsumes = data_.cx_->runtime()->securityCallbacks->subsumes) {
                JS::AutoAssertNoGC nogc;
                if (!subsumes(data_.principals_, activation->compartment()->principals)) {
                    ++data_.activations_;
                    continue;
                }
            }
        }

#ifdef JS_ION
        if (activation->isJit()) {
            data_.ionFrames_ = jit::IonFrameIterator(data_.activations_);

            // Stop at the first scripted frame.
            while (!data_.ionFrames_.isScripted() && !data_.ionFrames_.done())
                ++data_.ionFrames_;

            // It's possible to have an JitActivation with no scripted frames,
            // for instance if we hit an over-recursion during bailout.
            if (data_.ionFrames_.done()) {
                ++data_.activations_;
                continue;
            }

            nextJitFrame();
            data_.state_ = JIT;
            return;
        }
#endif

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
        data_.state_ = SCRIPTED;
        return;
    }
}

ScriptFrameIter::Data::Data(JSContext *cx, PerThreadData *perThread, SavedOption savedOption,
                            ContextOption contextOption, JSPrincipals *principals)
  : perThread_(perThread),
    cx_(cx),
    savedOption_(savedOption),
    contextOption_(contextOption),
    principals_(principals),
    pc_(nullptr),
    interpFrames_(nullptr),
    activations_(cx->runtime())
#ifdef JS_ION
  , ionFrames_((uint8_t *)nullptr, SequentialExecution)
#endif
{
}

ScriptFrameIter::Data::Data(const ScriptFrameIter::Data &other)
  : perThread_(other.perThread_),
    cx_(other.cx_),
    savedOption_(other.savedOption_),
    contextOption_(other.contextOption_),
    principals_(other.principals_),
    state_(other.state_),
    pc_(other.pc_),
    interpFrames_(other.interpFrames_),
    activations_(other.activations_)
#ifdef JS_ION
  , ionFrames_(other.ionFrames_)
#endif
{
}

ScriptFrameIter::ScriptFrameIter(JSContext *cx, SavedOption savedOption)
  : data_(cx, &cx->runtime()->mainThread, savedOption, CURRENT_CONTEXT, nullptr)
#ifdef JS_ION
    , ionInlineFrames_(cx, (js::jit::IonFrameIterator*) nullptr)
#endif
{
    settleOnActivation();
}

ScriptFrameIter::ScriptFrameIter(JSContext *cx, ContextOption contextOption,
                                 SavedOption savedOption, JSPrincipals *principals)
  : data_(cx, &cx->runtime()->mainThread, savedOption, contextOption, principals)
#ifdef JS_ION
    , ionInlineFrames_(cx, (js::jit::IonFrameIterator*) nullptr)
#endif
{
    settleOnActivation();
}

ScriptFrameIter::ScriptFrameIter(const ScriptFrameIter &other)
  : data_(other.data_)
#ifdef JS_ION
    , ionInlineFrames_(other.data_.cx_,
                       data_.ionFrames_.isScripted() ? &other.ionInlineFrames_ : nullptr)
#endif
{
}

ScriptFrameIter::ScriptFrameIter(const Data &data)
  : data_(data)
#ifdef JS_ION
    , ionInlineFrames_(data.cx_, data_.ionFrames_.isOptimizedJS() ? &data_.ionFrames_ : nullptr)
#endif
{
    JS_ASSERT(data.cx_);
}

#ifdef JS_ION
void
ScriptFrameIter::nextJitFrame()
{
    if (data_.ionFrames_.isOptimizedJS()) {
        ionInlineFrames_.resetOn(&data_.ionFrames_);
        data_.pc_ = ionInlineFrames_.pc();
    } else {
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        data_.ionFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
    }
}

void
ScriptFrameIter::popJitFrame()
{
    JS_ASSERT(data_.state_ == JIT);

    if (data_.ionFrames_.isOptimizedJS() && ionInlineFrames_.more()) {
        ++ionInlineFrames_;
        data_.pc_ = ionInlineFrames_.pc();
        return;
    }

    ++data_.ionFrames_;
    while (!data_.ionFrames_.done() && !data_.ionFrames_.isScripted())
        ++data_.ionFrames_;

    if (!data_.ionFrames_.done()) {
        nextJitFrame();
        return;
    }

    popActivation();
}
#endif

ScriptFrameIter &
ScriptFrameIter::operator++()
{
    switch (data_.state_) {
      case DONE:
        MOZ_ASSUME_UNREACHABLE("Unexpected state");
      case SCRIPTED:
        if (interpFrame()->isDebuggerFrame() && interpFrame()->evalInFramePrev()) {
            AbstractFramePtr eifPrev = interpFrame()->evalInFramePrev();

            // Eval-in-frame can cross contexts and works across saved frame
            // chains.
            ContextOption prevContextOption = data_.contextOption_;
            SavedOption prevSavedOption = data_.savedOption_;
            data_.contextOption_ = ALL_CONTEXTS;
            data_.savedOption_ = GO_THROUGH_SAVED;

            popInterpreterFrame();

            while (isIon() || abstractFramePtr() != eifPrev) {
                if (data_.state_ == JIT) {
#ifdef JS_ION
                    popJitFrame();
#else
                    MOZ_ASSUME_UNREACHABLE("Invalid state");
#endif
                } else {
                    popInterpreterFrame();
                }
            }

            data_.contextOption_ = prevContextOption;
            data_.savedOption_ = prevSavedOption;
            data_.cx_ = data_.activations_.activation()->cx();
            break;
        }
        popInterpreterFrame();
        break;
      case JIT:
#ifdef JS_ION
        popJitFrame();
        break;
#else
        MOZ_ASSUME_UNREACHABLE("Unexpected state");
#endif
    }
    return *this;
}

ScriptFrameIter::Data *
ScriptFrameIter::copyData() const
{
#ifdef JS_ION
    /*
     * This doesn't work for optimized Ion frames since ionInlineFrames_ is
     * not copied.
     */
    JS_ASSERT(data_.ionFrames_.type() != jit::IonFrame_OptimizedJS);
#endif
    return data_.cx_->new_<Data>(data_);
}

AbstractFramePtr
ScriptFrameIter::copyDataAsAbstractFramePtr() const
{
    AbstractFramePtr frame;
    if (Data *data = copyData())
        frame.ptr_ = uintptr_t(data);
    return frame;
}

JSCompartment *
ScriptFrameIter::compartment() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
      case JIT:
        return data_.activations_.activation()->compartment();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isFunctionFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isFunctionFrame();
      case JIT:
#ifdef JS_ION
        JS_ASSERT(data_.ionFrames_.isScripted());
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.isFunctionFrame();
        return ionInlineFrames_.isFunctionFrame();
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isGlobalFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isGlobalFrame();
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.baselineFrame()->isGlobalFrame();
        JS_ASSERT(!script()->isForEval());
        return !script()->functionNonDelazifying();
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isEvalFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isEvalFrame();
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.baselineFrame()->isEvalFrame();
        JS_ASSERT(!script()->isForEval());
        return false;
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isNonEvalFunctionFrame() const
{
    JS_ASSERT(!done());
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isNonEvalFunctionFrame();
      case JIT:
        return !isEvalFrame() && isFunctionFrame();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isGeneratorFrame() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isGeneratorFrame();
      case JIT:
        return false;
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::isConstructing() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS())
            return ionInlineFrames_.isConstructing();
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        return data_.ionFrames_.isConstructing();
#else
        break;
#endif        
      case SCRIPTED:
        return interpFrame()->isConstructing();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

AbstractFramePtr
ScriptFrameIter::abstractFramePtr() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.baselineFrame();
#endif
        break;
      case SCRIPTED:
        JS_ASSERT(interpFrame());
        return AbstractFramePtr(interpFrame());
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

void
ScriptFrameIter::updatePcQuadratic()
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED: {
        StackFrame *frame = interpFrame();
        InterpreterActivation *activation = data_.activations_.activation()->asInterpreter();

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
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS()) {
            jit::BaselineFrame *frame = data_.ionFrames_.baselineFrame();
            jit::JitActivation *activation = data_.activations_.activation()->asJit();

            // ActivationIterator::ionTop_ may be invalid, so create a new
            // activation iterator.
            data_.activations_ = ActivationIterator(data_.cx_->runtime());
            while (data_.activations_.activation() != activation)
                ++data_.activations_;

            // Look for the current frame.
            data_.ionFrames_ = jit::IonFrameIterator(data_.activations_);
            while (!data_.ionFrames_.isBaselineJS() || data_.ionFrames_.baselineFrame() != frame)
                ++data_.ionFrames_;

            // Update the pc.
            JS_ASSERT(data_.ionFrames_.baselineFrame() == frame);
            data_.ionFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
            return;
        }
#endif
        break;
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

JSFunction *
ScriptFrameIter::callee() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return &interpFrame()->callee();
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.callee();
        JS_ASSERT(data_.ionFrames_.isOptimizedJS());
        return ionInlineFrames_.callee();
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

Value
ScriptFrameIter::calleev() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return interpFrame()->calleev();
      case JIT:
#ifdef JS_ION
        return ObjectValue(*callee());
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

unsigned
ScriptFrameIter::numActualArgs() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return interpFrame()->numActualArgs();
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS())
            return ionInlineFrames_.numActualArgs();

        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        return data_.ionFrames_.numActualArgs();
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

Value
ScriptFrameIter::unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing) const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->unaliasedActual(i, checkAliasing);
      case JIT:
#ifdef JS_ION
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        return data_.ionFrames_.baselineFrame()->unaliasedActual(i, checkAliasing);
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

JSObject *
ScriptFrameIter::scopeChain() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS())
            return ionInlineFrames_.scopeChain();
        return data_.ionFrames_.baselineFrame()->scopeChain();
#else
        break;
#endif
      case SCRIPTED:
        return interpFrame()->scopeChain();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

CallObject &
ScriptFrameIter::callObj() const
{
    JS_ASSERT(callee()->isHeavyweight());

    JSObject *pobj = scopeChain();
    while (!pobj->is<CallObject>())
        pobj = pobj->enclosingScope();
    return pobj->as<CallObject>();
}

bool
ScriptFrameIter::hasArgsObj() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->hasArgsObj();
      case JIT:
#ifdef JS_ION
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        return data_.ionFrames_.baselineFrame()->hasArgsObj();
#else
        break;
#endif
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

ArgumentsObject &
ScriptFrameIter::argsObj() const
{
    JS_ASSERT(hasArgsObj());

    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        return data_.ionFrames_.baselineFrame()->argsObj();
#else
        break;
#endif
      case SCRIPTED:
        return interpFrame()->argsObj();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

bool
ScriptFrameIter::computeThis(JSContext *cx) const
{
    JS_ASSERT(!done());
    if (!isIon()) {
        assertSameCompartment(cx, scopeChain());
        return ComputeThis(cx, abstractFramePtr());
    }
    return true;
}

Value
ScriptFrameIter::thisv() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS())
            return ObjectValue(*ionInlineFrames_.thisObject());
        return data_.ionFrames_.baselineFrame()->thisValue();
#else
        break;
#endif
      case SCRIPTED:
        return interpFrame()->thisValue();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

Value
ScriptFrameIter::returnValue() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS())
            return data_.ionFrames_.baselineFrame()->returnValue();
#endif
        break;
      case SCRIPTED:
        return interpFrame()->returnValue();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

void
ScriptFrameIter::setReturnValue(const Value &v)
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isBaselineJS()) {
            data_.ionFrames_.baselineFrame()->setReturnValue(v);
            return;
        }
#endif
        break;
      case SCRIPTED:
        interpFrame()->setReturnValue(v);
        return;
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

size_t
ScriptFrameIter::numFrameSlots() const
{
    switch (data_.state_) {
      case DONE:
        break;
     case JIT: {
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS())
            return ionInlineFrames_.snapshotIterator().slots() - ionInlineFrames_.script()->nfixed();
        jit::BaselineFrame *frame = data_.ionFrames_.baselineFrame();
        return frame->numValueSlots() - data_.ionFrames_.script()->nfixed();
#else
        break;
#endif
      }
      case SCRIPTED:
        JS_ASSERT(data_.interpFrames_.sp() >= interpFrame()->base());
        return data_.interpFrames_.sp() - interpFrame()->base();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

Value
ScriptFrameIter::frameSlotValue(size_t index) const
{
    switch (data_.state_) {
      case DONE:
        break;
      case JIT:
#ifdef JS_ION
        if (data_.ionFrames_.isOptimizedJS()) {
            jit::SnapshotIterator si(ionInlineFrames_.snapshotIterator());
            index += ionInlineFrames_.script()->nfixed();
            return si.maybeReadSlotByIndex(index);
        }

        index += data_.ionFrames_.script()->nfixed();
        return *data_.ionFrames_.baselineFrame()->valueSlot(index);
#else
        break;
#endif
      case SCRIPTED:
          return interpFrame()->base()[index];
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected state");
}

#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

#ifdef DEBUG
/* static */
bool NonBuiltinScriptFrameIter::includeSelfhostedFrames() {
    static char* env = getenv("MOZ_SHOW_ALL_JS_FRAMES");
    return (bool)env;
}
#endif

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
    if (isStackFrame())
        return asStackFrame()->hasPushedSPSFrame();
#ifdef JS_ION
    return asBaselineFrame()->hasPushedSPSFrame();
#else
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
#endif
}

#ifdef DEBUG
void
js::CheckLocalUnaliased(MaybeCheckAliasing checkAliasing, JSScript *script, uint32_t i)
{
    if (!checkAliasing)
        return;

    JS_ASSERT(i < script->nslots());
    if (i < script->nfixed()) {
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
    active_(active)
{
    if (active) {
        prevIonTop_ = cx->mainThread().ionTop;
        prevIonJSContext_ = cx->mainThread().ionJSContext;
        cx->mainThread().ionJSContext = cx;
    } else {
        prevIonTop_ = nullptr;
        prevIonJSContext_ = nullptr;
    }
}

jit::JitActivation::~JitActivation()
{
    if (active_) {
        cx_->mainThread().ionTop = prevIonTop_;
        cx_->mainThread().ionJSContext = prevIonJSContext_;
    }
}

void
jit::JitActivation::setActive(JSContext *cx, bool active)
{
    // Only allowed to deactivate/activate if activation is top.
    // (Not tested and will probably fail in other situations.)
    JS_ASSERT(cx->mainThread().activation_ == this);
    JS_ASSERT(active != active_);
    active_ = active;

    if (active) {
        prevIonTop_ = cx->mainThread().ionTop;
        prevIonJSContext_ = cx->mainThread().ionJSContext;
        cx->mainThread().ionJSContext = cx;
    } else {
        cx->mainThread().ionTop = prevIonTop_;
        cx->mainThread().ionJSContext = prevIonJSContext_;
    }
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

ActivationIterator::ActivationIterator(JSRuntime *rt)
  : jitTop_(rt->mainThread.ionTop),
    activation_(rt->mainThread.activation_)
{
    settle();
}

ActivationIterator &
ActivationIterator::operator++()
{
    JS_ASSERT(activation_);
    if (activation_->isJit() && activation_->asJit()->isActive())
        jitTop_ = activation_->asJit()->prevIonTop();
    activation_ = activation_->prev();
    settle();
    return *this;
}

void
ActivationIterator::settle()
{
    while (!done() && activation_->isJit() && !activation_->asJit()->isActive()) {
        if (activation_->asJit()->isActive())
            jitTop_ = activation_->asJit()->prevIonTop();
        activation_ = activation_->prev();
    }
}
