/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Stack-inl.h"

#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "gc/Marking.h"
#include "jit/BaselineFrame.h"
#include "jit/JitcodeMap.h"
#include "jit/JitCompartment.h"
#include "js/GCAPI.h"
#include "vm/Debugger.h"
#include "vm/Opcodes.h"

#include "jit/JitFrameIterator-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Probes-inl.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodCopy;

/*****************************************************************************/

void
InterpreterFrame::initExecuteFrame(JSContext* cx, HandleScript script,
                                   AbstractFramePtr evalInFramePrev,
                                   const Value& newTargetValue, HandleObject envChain)
{
    flags_ = 0;
    script_ = script;

    // newTarget = NullValue is an initial sentinel for "please fill me in from the stack".
    // It should never be passed from Ion code.
    RootedValue newTarget(cx, newTargetValue);
    if (script->isDirectEvalInFunction()) {
        FrameIter iter(cx);
        MOZ_ASSERT(!iter.isWasm());
        if (newTarget.isNull() &&
            iter.script()->bodyScope()->hasOnChain(ScopeKind::Function))
        {
            newTarget = iter.newTarget();
        }
    } else if (evalInFramePrev) {
        if (newTarget.isNull() &&
            evalInFramePrev.script()->bodyScope()->hasOnChain(ScopeKind::Function))
        {
            newTarget = evalInFramePrev.newTarget();
        }
    }

    Value* dstvp = (Value*)this - 1;
    dstvp[0] = newTarget;

    envChain_ = envChain.get();
    prev_ = nullptr;
    prevpc_ = nullptr;
    prevsp_ = nullptr;

    evalInFramePrev_ = evalInFramePrev;
    MOZ_ASSERT_IF(evalInFramePrev, isDebuggerEvalFrame());

    if (script->isDebuggee())
        setIsDebuggee();

#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch(&rval_, 1);
#endif
}

bool
InterpreterFrame::isNonGlobalEvalFrame() const
{
    return isEvalFrame() && script()->bodyScope()->as<EvalScope>().isNonGlobal();
}

JSObject*
InterpreterFrame::createRestParameter(JSContext* cx)
{
    MOZ_ASSERT(script()->hasRest());
    unsigned nformal = callee().nargs() - 1, nactual = numActualArgs();
    unsigned nrest = (nactual > nformal) ? nactual - nformal : 0;
    Value* restvp = argv() + nformal;
    return ObjectGroup::newArrayObject(cx, restvp, nrest, GenericObject,
                                       ObjectGroup::NewArrayKind::UnknownIndex);
}

static inline void
AssertScopeMatchesEnvironment(Scope* scope, JSObject* originalEnv)
{
#ifdef DEBUG
    JSObject* env = originalEnv;
    for (ScopeIter si(scope); si; si++) {
        if (si.kind() == ScopeKind::NonSyntactic) {
            while (env->is<WithEnvironmentObject>() ||
                   env->is<NonSyntacticVariablesObject>() ||
                   (env->is<LexicalEnvironmentObject>() &&
                    !env->as<LexicalEnvironmentObject>().isSyntactic()))
            {
                MOZ_ASSERT(!IsSyntacticEnvironment(env));
                env = &env->as<EnvironmentObject>().enclosingEnvironment();
            }
        } else if (si.hasSyntacticEnvironment()) {
            switch (si.kind()) {
              case ScopeKind::Function:
                MOZ_ASSERT(env->as<CallObject>().callee().existingScriptNonDelazifying() ==
                           si.scope()->as<FunctionScope>().script());
                env = &env->as<CallObject>().enclosingEnvironment();
                break;

              case ScopeKind::FunctionBodyVar:
              case ScopeKind::ParameterExpressionVar:
                MOZ_ASSERT(&env->as<VarEnvironmentObject>().scope() == si.scope());
                env = &env->as<VarEnvironmentObject>().enclosingEnvironment();
                break;

              case ScopeKind::Lexical:
              case ScopeKind::SimpleCatch:
              case ScopeKind::Catch:
              case ScopeKind::NamedLambda:
              case ScopeKind::StrictNamedLambda:
                MOZ_ASSERT(&env->as<LexicalEnvironmentObject>().scope() == si.scope());
                env = &env->as<LexicalEnvironmentObject>().enclosingEnvironment();
                break;

              case ScopeKind::With:
                MOZ_ASSERT(&env->as<WithEnvironmentObject>().scope() == si.scope());
                env = &env->as<WithEnvironmentObject>().enclosingEnvironment();
                break;

              case ScopeKind::Eval:
              case ScopeKind::StrictEval:
                env = &env->as<VarEnvironmentObject>().enclosingEnvironment();
                break;

              case ScopeKind::Global:
                MOZ_ASSERT(env->as<LexicalEnvironmentObject>().isGlobal());
                env = &env->as<LexicalEnvironmentObject>().enclosingEnvironment();
                MOZ_ASSERT(env->is<GlobalObject>());
                break;

              case ScopeKind::NonSyntactic:
                MOZ_CRASH("NonSyntactic should not have a syntactic environment");
                break;

              case ScopeKind::Module:
                MOZ_ASSERT(env->as<ModuleEnvironmentObject>().module().script() ==
                           si.scope()->as<ModuleScope>().script());
                env = &env->as<ModuleEnvironmentObject>().enclosingEnvironment();
                break;

              case ScopeKind::WasmFunction:
                env = &env->as<WasmFunctionCallObject>().enclosingEnvironment();
                break;
            }
        }
    }

    // In the case of a non-syntactic env chain, the immediate parent of the
    // outermost non-syntactic env may be the global lexical env, or, if
    // called from Debugger, a DebugEnvironmentProxy.
    //
    // In the case of a syntactic env chain, the outermost env is always a
    // GlobalObject.
    MOZ_ASSERT(env->is<GlobalObject>() || IsGlobalLexicalEnvironment(env) ||
               env->is<DebugEnvironmentProxy>());
#endif
}

static inline void
AssertScopeMatchesEnvironment(InterpreterFrame* fp, jsbytecode* pc)
{
#ifdef DEBUG
    // If we OOMed before fully initializing the environment chain, the scope
    // and environment will definitely mismatch.
    if (fp->script()->initialEnvironmentShape() && fp->hasInitialEnvironment())
        AssertScopeMatchesEnvironment(fp->script()->innermostScope(pc), fp->environmentChain());
#endif
}

bool
InterpreterFrame::initFunctionEnvironmentObjects(JSContext* cx)
{
    return js::InitFunctionEnvironmentObjects(cx, this);
}

bool
InterpreterFrame::prologue(JSContext* cx)
{
    RootedScript script(cx, this->script());

    MOZ_ASSERT(cx->interpreterRegs().pc == script->code());

    if (isEvalFrame()) {
        if (!script->bodyScope()->hasEnvironment()) {
            MOZ_ASSERT(!script->strict());
            // Non-strict eval may introduce var bindings that conflict with
            // lexical bindings in an enclosing lexical scope.
            RootedObject varObjRoot(cx, &varObj());
            if (!CheckEvalDeclarationConflicts(cx, script, environmentChain(), varObjRoot))
                return false;
        }
        return probes::EnterScript(cx, script, nullptr, this);
    }

    if (isGlobalFrame()) {
        Rooted<LexicalEnvironmentObject*> lexicalEnv(cx);
        RootedObject varObjRoot(cx);
        if (script->hasNonSyntacticScope()) {
            lexicalEnv = &extensibleLexicalEnvironment();
            varObjRoot = &varObj();
        } else {
            lexicalEnv = &cx->global()->lexicalEnvironment();
            varObjRoot = cx->global();
        }
        if (!CheckGlobalDeclarationConflicts(cx, script, lexicalEnv, varObjRoot))
            return false;
        return probes::EnterScript(cx, script, nullptr, this);
    }

    if (isModuleFrame())
        return probes::EnterScript(cx, script, nullptr, this);

    // At this point, we've yet to push any environments. Check that they
    // match the enclosing scope.
    AssertScopeMatchesEnvironment(script->enclosingScope(), environmentChain());

    MOZ_ASSERT(isFunctionFrame());
    if (callee().needsFunctionEnvironmentObjects() && !initFunctionEnvironmentObjects(cx))
        return false;

    if (isConstructing()) {
        if (callee().isBoundFunction()) {
            thisArgument() = MagicValue(JS_UNINITIALIZED_LEXICAL);
        } else if (script->isDerivedClassConstructor()) {
            MOZ_ASSERT(callee().isClassConstructor());
            thisArgument() = MagicValue(JS_UNINITIALIZED_LEXICAL);
        } else if (thisArgument().isObject()) {
            // Nothing to do. Correctly set.
        } else {
            MOZ_ASSERT(thisArgument().isMagic(JS_IS_CONSTRUCTING));
            RootedObject callee(cx, &this->callee());
            RootedObject newTarget(cx, &this->newTarget().toObject());
            JSObject* obj = CreateThisForFunction(cx, callee, newTarget,
                                                  createSingleton() ? SingletonObject : GenericObject);
            if (!obj)
                return false;
            thisArgument() = ObjectValue(*obj);
        }
    }

    return probes::EnterScript(cx, script, script->functionNonDelazifying(), this);
}

void
InterpreterFrame::epilogue(JSContext* cx, jsbytecode* pc)
{
    RootedScript script(cx, this->script());
    probes::ExitScript(cx, script, script->functionNonDelazifying(), hasPushedGeckoProfilerFrame());

    // Check that the scope matches the environment at the point of leaving
    // the frame.
    AssertScopeMatchesEnvironment(this, pc);

    EnvironmentIter ei(cx, this, pc);
    UnwindAllEnvironmentsInFrame(cx, ei);

    if (isFunctionFrame()) {
        if (!callee().isStarGenerator() &&
            !callee().isLegacyGenerator() &&
            !callee().isAsync() &&
            isConstructing() &&
            thisArgument().isObject() &&
            returnValue().isPrimitive())
        {
            setReturnValue(thisArgument());
        }

        return;
    }

    MOZ_ASSERT(isEvalFrame() || isGlobalFrame() || isModuleFrame());
}

bool
InterpreterFrame::checkReturn(JSContext* cx, HandleValue thisv)
{
    MOZ_ASSERT(script()->isDerivedClassConstructor());
    MOZ_ASSERT(isFunctionFrame());
    MOZ_ASSERT(callee().isClassConstructor());

    HandleValue retVal = returnValue();
    if (retVal.isObject())
        return true;

    if (!retVal.isUndefined()) {
        ReportValueError(cx, JSMSG_BAD_DERIVED_RETURN, JSDVG_IGNORE_STACK, retVal, nullptr);
        return false;
    }

    if (thisv.isMagic(JS_UNINITIALIZED_LEXICAL))
        return ThrowUninitializedThis(cx, this);

    setReturnValue(thisv);
    return true;
}

bool
InterpreterFrame::pushVarEnvironment(JSContext* cx, HandleScope scope)
{
    return js::PushVarEnvironmentObject(cx, scope, this);
}

bool
InterpreterFrame::pushLexicalEnvironment(JSContext* cx, Handle<LexicalScope*> scope)
{
    LexicalEnvironmentObject* env = LexicalEnvironmentObject::create(cx, scope, this);
    if (!env)
        return false;

    pushOnEnvironmentChain(*env);
    return true;
}

bool
InterpreterFrame::freshenLexicalEnvironment(JSContext* cx)
{
    Rooted<LexicalEnvironmentObject*> env(cx, &envChain_->as<LexicalEnvironmentObject>());
    LexicalEnvironmentObject* fresh = LexicalEnvironmentObject::clone(cx, env);
    if (!fresh)
        return false;

    replaceInnermostEnvironment(*fresh);
    return true;
}

bool
InterpreterFrame::recreateLexicalEnvironment(JSContext* cx)
{
    Rooted<LexicalEnvironmentObject*> env(cx, &envChain_->as<LexicalEnvironmentObject>());
    LexicalEnvironmentObject* fresh = LexicalEnvironmentObject::recreate(cx, env);
    if (!fresh)
        return false;

    replaceInnermostEnvironment(*fresh);
    return true;
}

void
InterpreterFrame::trace(JSTracer* trc, Value* sp, jsbytecode* pc)
{
    TraceRoot(trc, &envChain_, "env chain");
    TraceRoot(trc, &script_, "script");

    if (flags_ & HAS_ARGS_OBJ)
        TraceRoot(trc, &argsObj_, "arguments");

    if (hasReturnValue())
        TraceRoot(trc, &rval_, "rval");

    MOZ_ASSERT(sp >= slots());

    if (hasArgs()) {
        // Trace the callee and |this|. When we're doing a moving GC, we
        // need to fix up the callee pointer before we use it below, under
        // numFormalArgs() and script().
        TraceRootRange(trc, 2, argv_ - 2, "fp callee and this");

        // Trace arguments.
        unsigned argc = Max(numActualArgs(), numFormalArgs());
        TraceRootRange(trc, argc + isConstructing(), argv_, "fp argv");
    } else {
        // Trace newTarget.
        TraceRoot(trc, ((Value*)this) - 1, "stack newTarget");
    }

    JSScript* script = this->script();
    size_t nfixed = script->nfixed();
    size_t nlivefixed = script->calculateLiveFixed(pc);

    if (nfixed == nlivefixed) {
        // All locals are live.
        traceValues(trc, 0, sp - slots());
    } else {
        // Trace operand stack.
        traceValues(trc, nfixed, sp - slots());

        // Clear dead block-scoped locals.
        while (nfixed > nlivefixed)
            unaliasedLocal(--nfixed).setUndefined();

        // Trace live locals.
        traceValues(trc, 0, nlivefixed);
    }

    if (script->compartment()->debugEnvs)
        script->compartment()->debugEnvs->traceLiveFrame(trc, this);
}

void
InterpreterFrame::traceValues(JSTracer* trc, unsigned start, unsigned end)
{
    if (start < end)
        TraceRootRange(trc, end - start, slots() + start, "vm_stack");
}

static void
TraceInterpreterActivation(JSTracer* trc, InterpreterActivation* act)
{
    for (InterpreterFrameIterator frames(act); !frames.done(); ++frames) {
        InterpreterFrame* fp = frames.frame();
        fp->trace(trc, frames.sp(), frames.pc());
    }
}

void
js::TraceInterpreterActivations(JSContext* cx, const CooperatingContext& target, JSTracer* trc)
{
    for (ActivationIterator iter(cx, target); !iter.done(); ++iter) {
        Activation* act = iter.activation();
        if (act->isInterpreter())
            TraceInterpreterActivation(trc, act->asInterpreter());
    }
}

/*****************************************************************************/

// Unlike the other methods of this class, this method is defined here so that
// we don't have to #include jsautooplen.h in vm/Stack.h.
void
InterpreterRegs::setToEndOfScript()
{
    sp = fp()->base();
}

/*****************************************************************************/

InterpreterFrame*
InterpreterStack::pushInvokeFrame(JSContext* cx, const CallArgs& args, MaybeConstruct constructing)
{
    LifoAlloc::Mark mark = allocator_.mark();

    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    RootedScript script(cx, fun->nonLazyScript());

    Value* argv;
    InterpreterFrame* fp = getCallFrame(cx, args, script, constructing, &argv);
    if (!fp)
        return nullptr;

    fp->mark_ = mark;
    fp->initCallFrame(cx, nullptr, nullptr, nullptr, *fun, script, argv, args.length(),
                      constructing);
    return fp;
}

InterpreterFrame*
InterpreterStack::pushExecuteFrame(JSContext* cx, HandleScript script, const Value& newTargetValue,
                                   HandleObject envChain, AbstractFramePtr evalInFrame)
{
    LifoAlloc::Mark mark = allocator_.mark();

    unsigned nvars = 1 /* newTarget */ + script->nslots();
    uint8_t* buffer = allocateFrame(cx, sizeof(InterpreterFrame) + nvars * sizeof(Value));
    if (!buffer)
        return nullptr;

    InterpreterFrame* fp = reinterpret_cast<InterpreterFrame*>(buffer + 1 * sizeof(Value));
    fp->mark_ = mark;
    fp->initExecuteFrame(cx, script, evalInFrame, newTargetValue, envChain);
    fp->initLocals();

    return fp;
}

/*****************************************************************************/

void
FrameIter::popActivation()
{
    ++data_.activations_;
    settleOnActivation();
}

void
FrameIter::popInterpreterFrame()
{
    MOZ_ASSERT(data_.state_ == INTERP);

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

        Activation* activation = data_.activations_.activation();

        // If the caller supplied principals, only show activations which are subsumed (of the same
        // origin or of an origin accessible) by these principals.
        if (data_.principals_) {
            JSContext* cx = data_.cx_;
            if (JSSubsumesOp subsumes = cx->runtime()->securityCallbacks->subsumes) {
                if (!subsumes(data_.principals_, activation->compartment()->principals())) {
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

        if (activation->isWasm()) {
            data_.wasmFrames_ = wasm::FrameIterator(data_.activations_->asWasm());

            if (data_.wasmFrames_.done()) {
                ++data_.activations_;
                continue;
            }

            data_.pc_ = nullptr;
            data_.state_ = WASM;
            return;
        }

        MOZ_ASSERT(activation->isInterpreter());

        InterpreterActivation* interpAct = activation->asInterpreter();
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

        MOZ_ASSERT(!data_.interpFrames_.frame()->runningInJit());
        data_.pc_ = data_.interpFrames_.pc();
        data_.state_ = INTERP;
        return;
    }
}

FrameIter::Data::Data(JSContext* cx, DebuggerEvalOption debuggerEvalOption,
                      JSPrincipals* principals)
  : cx_(cx),
    debuggerEvalOption_(debuggerEvalOption),
    principals_(principals),
    state_(DONE),
    pc_(nullptr),
    interpFrames_(nullptr),
    activations_(cx),
    jitFrames_(),
    ionInlineFrameNo_(0),
    wasmFrames_()
{
}

FrameIter::Data::Data(JSContext* cx, const CooperatingContext& target,
                      DebuggerEvalOption debuggerEvalOption)
  : cx_(cx),
    debuggerEvalOption_(debuggerEvalOption),
    principals_(nullptr),
    state_(DONE),
    pc_(nullptr),
    interpFrames_(nullptr),
    activations_(cx, target),
    jitFrames_(),
    ionInlineFrameNo_(0),
    wasmFrames_()
{
}

FrameIter::Data::Data(const FrameIter::Data& other)
  : cx_(other.cx_),
    debuggerEvalOption_(other.debuggerEvalOption_),
    principals_(other.principals_),
    state_(other.state_),
    pc_(other.pc_),
    interpFrames_(other.interpFrames_),
    activations_(other.activations_),
    jitFrames_(other.jitFrames_),
    ionInlineFrameNo_(other.ionInlineFrameNo_),
    wasmFrames_(other.wasmFrames_)
{
}

FrameIter::FrameIter(JSContext* cx, const CooperatingContext& target,
                     DebuggerEvalOption debuggerEvalOption)
  : data_(cx, target, debuggerEvalOption),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    // settleOnActivation can only GC if principals are given.
    JS::AutoSuppressGCAnalysis nogc;
    settleOnActivation();
}

FrameIter::FrameIter(JSContext* cx, DebuggerEvalOption debuggerEvalOption)
  : data_(cx, debuggerEvalOption, nullptr),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    // settleOnActivation can only GC if principals are given.
    JS::AutoSuppressGCAnalysis nogc;
    settleOnActivation();
}

FrameIter::FrameIter(JSContext* cx, DebuggerEvalOption debuggerEvalOption,
                     JSPrincipals* principals)
  : data_(cx, debuggerEvalOption, principals),
    ionInlineFrames_(cx, (js::jit::JitFrameIterator*) nullptr)
{
    settleOnActivation();
}

FrameIter::FrameIter(const FrameIter& other)
  : data_(other.data_),
    ionInlineFrames_(other.data_.cx_,
                     data_.jitFrames_.isIonScripted() ? &other.ionInlineFrames_ : nullptr)
{
}

FrameIter::FrameIter(const Data& data)
  : data_(data),
    ionInlineFrames_(data.cx_, data_.jitFrames_.isIonScripted() ? &data_.jitFrames_ : nullptr)
{
    MOZ_ASSERT(data.cx_);

    if (data_.jitFrames_.isIonScripted()) {
        while (ionInlineFrames_.frameNo() != data.ionInlineFrameNo_)
            ++ionInlineFrames_;
    }
}

void
FrameIter::nextJitFrame()
{
    if (data_.jitFrames_.isIonScripted()) {
        ionInlineFrames_.resetOn(&data_.jitFrames_);
        data_.pc_ = ionInlineFrames_.pc();
    } else {
        MOZ_ASSERT(data_.jitFrames_.isBaselineJS());
        data_.jitFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
    }
}

void
FrameIter::popJitFrame()
{
    MOZ_ASSERT(data_.state_ == JIT);

    if (data_.jitFrames_.isIonScripted() && ionInlineFrames_.more()) {
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
FrameIter::popWasmFrame()
{
    MOZ_ASSERT(data_.state_ == WASM);

    ++data_.wasmFrames_;
    data_.pc_ = nullptr;
    if (data_.wasmFrames_.done())
        popActivation();
}

FrameIter&
FrameIter::operator++()
{
    switch (data_.state_) {
      case DONE:
        MOZ_CRASH("Unexpected state");
      case INTERP:
        if (interpFrame()->isDebuggerEvalFrame() &&
            interpFrame()->evalInFramePrev() &&
            data_.debuggerEvalOption_ == FOLLOW_DEBUGGER_EVAL_PREV_LINK)
        {
            AbstractFramePtr eifPrev = interpFrame()->evalInFramePrev();

            popInterpreterFrame();

            while (!hasUsableAbstractFramePtr() || abstractFramePtr() != eifPrev) {
                if (data_.state_ == JIT)
                    popJitFrame();
                else if (data_.state_ == WASM)
                    popWasmFrame();
                else
                    popInterpreterFrame();
            }

            break;
        }
        popInterpreterFrame();
        break;
      case JIT:
        popJitFrame();
        break;
      case WASM:
        popWasmFrame();
        break;
    }
    return *this;
}

FrameIter::Data*
FrameIter::copyData() const
{
    Data* data = data_.cx_->new_<Data>(data_);
    if (!data)
        return nullptr;

    if (data && data_.jitFrames_.isIonScripted())
        data->ionInlineFrameNo_ = ionInlineFrames_.frameNo();
    return data;
}

AbstractFramePtr
FrameIter::copyDataAsAbstractFramePtr() const
{
    AbstractFramePtr frame;
    if (Data* data = copyData())
        frame.ptr_ = uintptr_t(data);
    return frame;
}

void*
FrameIter::rawFramePtr() const
{
    switch (data_.state_) {
      case DONE:
        return nullptr;
      case JIT:
        return data_.jitFrames_.fp();
      case INTERP:
        return interpFrame();
      case WASM:
        return nullptr;
    }
    MOZ_CRASH("Unexpected state");
}

JSCompartment*
FrameIter::compartment() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
      case WASM:
        return data_.activations_->compartment();
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
        MOZ_ASSERT(!script()->isForEval());
        return false;
      case WASM:
        return false;
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isFunctionFrame() const
{
    MOZ_ASSERT(!done());
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
        return interpFrame()->isFunctionFrame();
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame()->isFunctionFrame();
        return script()->functionNonDelazifying();
      case WASM:
        return false;
    }
    MOZ_CRASH("Unexpected state");
}

JSAtom*
FrameIter::functionDisplayAtom() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        MOZ_ASSERT(isFunctionFrame());
        return calleeTemplate()->displayAtom();
      case WASM:
        MOZ_ASSERT(isWasm());
        return data_.wasmFrames_.functionDisplayAtom();
    }

    MOZ_CRASH("Unexpected state");
}

ScriptSource*
FrameIter::scriptSource() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case INTERP:
      case JIT:
        return script()->scriptSource();
    }

    MOZ_CRASH("Unexpected state");
}

const char*
FrameIter::filename() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return script()->filename();
      case WASM:
        return data_.wasmFrames_.filename();
    }

    MOZ_CRASH("Unexpected state");
}

const char16_t*
FrameIter::displayURL() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT: {
        ScriptSource* ss = script()->scriptSource();
        return ss->hasDisplayURL() ? ss->displayURL() : nullptr;
      }
      case WASM:
        return data_.wasmFrames_.displayURL();
    }
    MOZ_CRASH("Unexpected state");
}

unsigned
FrameIter::computeLine(uint32_t* column) const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return PCToLineNumber(script(), pc(), column);
      case WASM:
        if (column)
            *column = 0;
        return data_.wasmFrames_.lineOrBytecode();
    }

    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::mutedErrors() const
{
    switch (data_.state_) {
      case DONE:
        break;
      case INTERP:
      case JIT:
        return script()->mutedErrors();
      case WASM:
        return data_.wasmFrames_.mutedErrors();
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::isConstructing() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case JIT:
        if (data_.jitFrames_.isIonScripted())
            return ionInlineFrames_.isConstructing();
        MOZ_ASSERT(data_.jitFrames_.isBaselineJS());
        return data_.jitFrames_.isConstructing();
      case INTERP:
        return interpFrame()->isConstructing();
    }

    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::ensureHasRematerializedFrame(JSContext* cx)
{
    MOZ_ASSERT(isIon());
    return !!activation()->asJit()->getRematerializedFrame(cx, data_.jitFrames_);
}

bool
FrameIter::hasUsableAbstractFramePtr() const
{
    switch (data_.state_) {
      case DONE:
        return false;
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return true;

        MOZ_ASSERT(data_.jitFrames_.isIonScripted());
        return !!activation()->asJit()->lookupRematerializedFrame(data_.jitFrames_.fp(),
                                                                  ionInlineFrames_.frameNo());
        break;
      case INTERP:
        return true;
      case WASM:
        return data_.wasmFrames_.debugEnabled();
    }
    MOZ_CRASH("Unexpected state");
}

AbstractFramePtr
FrameIter::abstractFramePtr() const
{
    MOZ_ASSERT(hasUsableAbstractFramePtr());
    switch (data_.state_) {
      case DONE:
        break;
      case JIT: {
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.baselineFrame();

        MOZ_ASSERT(data_.jitFrames_.isIonScripted());
        return activation()->asJit()->lookupRematerializedFrame(data_.jitFrames_.fp(),
                                                                ionInlineFrames_.frameNo());
        break;
      }
      case INTERP:
        MOZ_ASSERT(interpFrame());
        return AbstractFramePtr(interpFrame());
      case WASM:
        MOZ_ASSERT(data_.wasmFrames_.debugEnabled());
        return data_.wasmFrames_.debugFrame();
    }
    MOZ_CRASH("Unexpected state");
}

void
FrameIter::updatePcQuadratic()
{
    switch (data_.state_) {
      case WASM:
      case DONE:
        break;
      case INTERP: {
        InterpreterFrame* frame = interpFrame();
        InterpreterActivation* activation = data_.activations_->asInterpreter();

        // Look for the current frame.
        data_.interpFrames_ = InterpreterFrameIterator(activation);
        while (data_.interpFrames_.frame() != frame)
            ++data_.interpFrames_;

        // Update the pc.
        MOZ_ASSERT(data_.interpFrames_.frame() == frame);
        data_.pc_ = data_.interpFrames_.pc();
        return;
      }
      case JIT:
        if (data_.jitFrames_.isBaselineJS()) {
            jit::BaselineFrame* frame = data_.jitFrames_.baselineFrame();
            jit::JitActivation* activation = data_.activations_->asJit();

            // activation's exitFP may be invalid, so create a new
            // activation iterator.
            data_.activations_ = ActivationIterator(data_.cx_);
            while (data_.activations_.activation() != activation)
                ++data_.activations_;

            // Look for the current frame.
            data_.jitFrames_ = jit::JitFrameIterator(data_.activations_);
            while (!data_.jitFrames_.isBaselineJS() || data_.jitFrames_.baselineFrame() != frame)
                ++data_.jitFrames_;

            // Update the pc.
            MOZ_ASSERT(data_.jitFrames_.baselineFrame() == frame);
            data_.jitFrames_.baselineScriptAndPc(nullptr, &data_.pc_);
            return;
        }
        break;
    }
    MOZ_CRASH("Unexpected state");
}

void
FrameIter::wasmUpdateBytecodeOffset()
{
    MOZ_RELEASE_ASSERT(data_.state_ == WASM, "Unexpected state");

    wasm::DebugFrame* frame = data_.wasmFrames_.debugFrame();
    WasmActivation* activation = data_.activations_->asWasm();

    // Relookup the current frame, updating the bytecode offset in the process.
    data_.wasmFrames_ = wasm::FrameIterator(activation);
    while (data_.wasmFrames_.debugFrame() != frame)
        ++data_.wasmFrames_;

    MOZ_ASSERT(data_.wasmFrames_.debugFrame() == frame);
}

JSFunction*
FrameIter::calleeTemplate() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case INTERP:
        MOZ_ASSERT(isFunctionFrame());
        return &interpFrame()->callee();
      case JIT:
        if (data_.jitFrames_.isBaselineJS())
            return data_.jitFrames_.callee();
        MOZ_ASSERT(data_.jitFrames_.isIonScripted());
        return ionInlineFrames_.calleeTemplate();
    }
    MOZ_CRASH("Unexpected state");
}

JSFunction*
FrameIter::callee(JSContext* cx) const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case INTERP:
        return calleeTemplate();
      case JIT:
        if (data_.jitFrames_.isIonScripted()) {
            jit::MaybeReadFallback recover(cx, activation()->asJit(), &data_.jitFrames_);
            return ionInlineFrames_.callee(recover);
        }
        MOZ_ASSERT(data_.jitFrames_.isBaselineJS());
        return calleeTemplate();
    }
    MOZ_CRASH("Unexpected state");
}

bool
FrameIter::matchCallee(JSContext* cx, HandleFunction fun) const
{
    RootedFunction currentCallee(cx, calleeTemplate());

    // As we do not know if the calleeTemplate is the real function, or the
    // template from which it would be cloned, we compare properties which are
    // stable across the cloning of JSFunctions.
    if (((currentCallee->flags() ^ fun->flags()) & JSFunction::STABLE_ACROSS_CLONES) != 0 ||
        currentCallee->nargs() != fun->nargs())
    {
        return false;
    }

    // Use the same condition as |js::CloneFunctionObject|, to know if we should
    // expect both functions to have the same JSScript. If so, and if they are
    // different, then they cannot be equal.
    RootedObject global(cx, &fun->global());
    bool useSameScript = CanReuseScriptForClone(fun->compartment(), currentCallee, global);
    if (useSameScript &&
        (currentCallee->hasScript() != fun->hasScript() ||
         currentCallee->nonLazyScript() != fun->nonLazyScript()))
    {
        return false;
    }

    // If none of the previous filters worked, then take the risk of
    // invalidating the frame to identify the JSFunction.
    return callee(cx) == fun;
}

unsigned
FrameIter::numActualArgs() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case INTERP:
        MOZ_ASSERT(isFunctionFrame());
        return interpFrame()->numActualArgs();
      case JIT:
        if (data_.jitFrames_.isIonScripted())
            return ionInlineFrames_.numActualArgs();

        MOZ_ASSERT(data_.jitFrames_.isBaselineJS());
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

JSObject*
FrameIter::environmentChain(JSContext* cx) const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case JIT:
        if (data_.jitFrames_.isIonScripted()) {
            jit::MaybeReadFallback recover(cx, activation()->asJit(), &data_.jitFrames_);
            return ionInlineFrames_.environmentChain(recover);
        }
        return data_.jitFrames_.baselineFrame()->environmentChain();
      case INTERP:
        return interpFrame()->environmentChain();
    }
    MOZ_CRASH("Unexpected state");
}

CallObject&
FrameIter::callObj(JSContext* cx) const
{
    MOZ_ASSERT(calleeTemplate()->needsCallObject());

    JSObject* pobj = environmentChain(cx);
    while (!pobj->is<CallObject>())
        pobj = pobj->enclosingEnvironment();
    return pobj->as<CallObject>();
}

bool
FrameIter::hasArgsObj() const
{
    return abstractFramePtr().hasArgsObj();
}

ArgumentsObject&
FrameIter::argsObj() const
{
    MOZ_ASSERT(hasArgsObj());
    return abstractFramePtr().argsObj();
}

Value
FrameIter::thisArgument(JSContext* cx) const
{
    MOZ_ASSERT(isFunctionFrame());

    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case JIT:
        if (data_.jitFrames_.isIonScripted()) {
            jit::MaybeReadFallback recover(cx, activation()->asJit(), &data_.jitFrames_);
            return ionInlineFrames_.thisArgument(recover);
        }
        return data_.jitFrames_.baselineFrame()->thisArgument();
      case INTERP:
        return interpFrame()->thisArgument();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::newTarget() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case INTERP:
        return interpFrame()->newTarget();
      case JIT:
        MOZ_ASSERT(data_.jitFrames_.isBaselineJS());
        return data_.jitFrames_.baselineFrame()->newTarget();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::returnValue() const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
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
FrameIter::setReturnValue(const Value& v)
{
    switch (data_.state_) {
      case DONE:
      case WASM:
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
      case WASM:
        break;
      case JIT: {
        if (data_.jitFrames_.isIonScripted()) {
            return ionInlineFrames_.snapshotIterator().numAllocations() -
                ionInlineFrames_.script()->nfixed();
        }
        jit::BaselineFrame* frame = data_.jitFrames_.baselineFrame();
        return frame->numValueSlots() - data_.jitFrames_.script()->nfixed();
      }
      case INTERP:
        MOZ_ASSERT(data_.interpFrames_.sp() >= interpFrame()->base());
        return data_.interpFrames_.sp() - interpFrame()->base();
    }
    MOZ_CRASH("Unexpected state");
}

Value
FrameIter::frameSlotValue(size_t index) const
{
    switch (data_.state_) {
      case DONE:
      case WASM:
        break;
      case JIT:
        if (data_.jitFrames_.isIonScripted()) {
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

#ifdef DEBUG
bool
js::SelfHostedFramesVisible()
{
    static bool checked = false;
    static bool visible = false;
    if (!checked) {
        checked = true;
        char* env = getenv("MOZ_SHOW_ALL_JS_FRAMES");
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

ActivationEntryMonitor::ActivationEntryMonitor(JSContext* cx)
  : cx_(cx), entryMonitor_(cx->entryMonitor)
{
    cx->entryMonitor = nullptr;
}

Value
ActivationEntryMonitor::asyncStack(JSContext* cx)
{
    RootedValue stack(cx, ObjectOrNullValue(cx->asyncStackForNewActivations()));
    if (!cx->compartment()->wrap(cx, &stack)) {
        cx->clearPendingException();
        return UndefinedValue();
    }
    return stack;
}

ActivationEntryMonitor::ActivationEntryMonitor(JSContext* cx, InterpreterFrame* entryFrame)
  : ActivationEntryMonitor(cx)
{
    if (entryMonitor_) {
        // The InterpreterFrame is not yet part of an Activation, so it won't
        // be traced if we trigger GC here. Suppress GC to avoid this.
        gc::AutoSuppressGC suppressGC(cx);
        RootedValue stack(cx, asyncStack(cx));
        const char* asyncCause = cx->asyncCauseForNewActivations;
        if (entryFrame->isFunctionFrame())
            entryMonitor_->Entry(cx, &entryFrame->callee(), stack, asyncCause);
        else
            entryMonitor_->Entry(cx, entryFrame->script(), stack, asyncCause);
    }
}

ActivationEntryMonitor::ActivationEntryMonitor(JSContext* cx, jit::CalleeToken entryToken)
  : ActivationEntryMonitor(cx)
{
    if (entryMonitor_) {
        // The CalleeToken is not traced at this point and we also don't want
        // a GC to discard the code we're about to enter, so we suppress GC.
        gc::AutoSuppressGC suppressGC(cx);
        RootedValue stack(cx, asyncStack(cx));
        const char* asyncCause = cx->asyncCauseForNewActivations;
        if (jit::CalleeTokenIsFunction(entryToken))
            entryMonitor_->Entry(cx_, jit::CalleeTokenToFunction(entryToken), stack, asyncCause);
        else
            entryMonitor_->Entry(cx_, jit::CalleeTokenToScript(entryToken), stack, asyncCause);
    }
}

/*****************************************************************************/

jit::JitActivation::JitActivation(JSContext* cx, bool active)
  : Activation(cx, Jit),
    exitFP_(nullptr),
    prevJitActivation_(cx->jitActivation),
    active_(active),
    rematerializedFrames_(nullptr),
    ionRecovery_(cx),
    bailoutData_(nullptr),
    lastProfilingFrame_(nullptr),
    lastProfilingCallSite_(nullptr)
{
    if (active) {
        cx->jitActivation = this;
        registerProfiling();
    }
}

jit::JitActivation::~JitActivation()
{
    if (active_) {
        if (isProfiling())
            unregisterProfiling();
        cx_->jitActivation = prevJitActivation_;
    } else {
        MOZ_ASSERT(cx_->jitActivation == prevJitActivation_);
    }

    // All reocvered value are taken from activation during the bailout.
    MOZ_ASSERT(ionRecovery_.empty());

    // The BailoutFrameInfo should have unregistered itself from the
    // JitActivations.
    MOZ_ASSERT(!bailoutData_);

    clearRematerializedFrames();
    js_delete(rematerializedFrames_);
}

void
jit::JitActivation::setBailoutData(jit::BailoutFrameInfo* bailoutData)
{
    MOZ_ASSERT(!bailoutData_);
    bailoutData_ = bailoutData;
}

void
jit::JitActivation::cleanBailoutData()
{
    MOZ_ASSERT(bailoutData_);
    bailoutData_ = nullptr;
}

// setActive() is inlined in GenerateJitExit() with explicit masm instructions so
// changes to the logic here need to be reflected in GenerateJitExit() in the enable
// and disable activation instruction sequences.
void
jit::JitActivation::setActive(JSContext* cx, bool active)
{
    // Only allowed to deactivate/activate if activation is top.
    // (Not tested and will probably fail in other situations.)
    MOZ_ASSERT(cx->activation_ == this);
    MOZ_ASSERT(active != active_);

    if (active) {
        *((volatile bool*) active_) = true;
        MOZ_ASSERT(prevJitActivation_ == cx->jitActivation);
        cx->jitActivation = this;

        registerProfiling();
    } else {
        unregisterProfiling();

        cx->jitActivation = prevJitActivation_;
        *((volatile bool*) active_) = false;
    }
}

void
jit::JitActivation::removeRematerializedFrame(uint8_t* top)
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

jit::RematerializedFrame*
jit::JitActivation::getRematerializedFrame(JSContext* cx, const JitFrameIterator& iter,
                                           size_t inlineDepth)
{
    MOZ_ASSERT(iter.activation() == this);
    MOZ_ASSERT(iter.isIonScripted());

    if (!rematerializedFrames_) {
        rematerializedFrames_ = cx->new_<RematerializedFrameTable>(cx);
        if (!rematerializedFrames_)
            return nullptr;
        if (!rematerializedFrames_->init()) {
            rematerializedFrames_ = nullptr;
            ReportOutOfMemory(cx);
            return nullptr;
        }
    }

    uint8_t* top = iter.fp();
    RematerializedFrameTable::AddPtr p = rematerializedFrames_->lookupForAdd(top);
    if (!p) {
        RematerializedFrameVector frames(cx);

        // The unit of rematerialization is an uninlined frame and its inlined
        // frames. Since inlined frames do not exist outside of snapshots, it
        // is impossible to synchronize their rematerialized copies to
        // preserve identity. Therefore, we always rematerialize an uninlined
        // frame and all its inlined frames at once.
        InlineFrameIterator inlineIter(cx, &iter);
        MaybeReadFallback recover(cx, this, &iter);

        // Frames are often rematerialized with the cx inside a Debugger's
        // compartment. To recover slots and to create CallObjects, we need to
        // be in the activation's compartment.
        AutoCompartmentUnchecked ac(cx, compartment_);

        if (!RematerializedFrame::RematerializeInlineFrames(cx, top, inlineIter, recover, frames))
            return nullptr;

        if (!rematerializedFrames_->add(p, top, Move(frames))) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

        // See comment in unsetPrevUpToDateUntil.
        DebugEnvironments::unsetPrevUpToDateUntil(cx, p->value()[inlineDepth]);
    }

    return p->value()[inlineDepth];
}

jit::RematerializedFrame*
jit::JitActivation::lookupRematerializedFrame(uint8_t* top, size_t inlineDepth)
{
    if (!rematerializedFrames_)
        return nullptr;
    if (RematerializedFrameTable::Ptr p = rematerializedFrames_->lookup(top))
        return inlineDepth < p->value().length() ? p->value()[inlineDepth] : nullptr;
    return nullptr;
}

void
jit::JitActivation::removeRematerializedFramesFromDebugger(JSContext* cx, uint8_t* top)
{
    // Ion bailout can fail due to overrecursion and OOM. In such cases we
    // cannot honor any further Debugger hooks on the frame, and need to
    // ensure that its Debugger.Frame entry is cleaned up.
    if (!cx->compartment()->isDebuggee() || !rematerializedFrames_)
        return;
    if (RematerializedFrameTable::Ptr p = rematerializedFrames_->lookup(top)) {
        for (uint32_t i = 0; i < p->value().length(); i++)
            Debugger::handleUnrecoverableIonBailoutError(cx, p->value()[i]);
    }
}

void
jit::JitActivation::traceRematerializedFrames(JSTracer* trc)
{
    if (!rematerializedFrames_)
        return;
    for (RematerializedFrameTable::Enum e(*rematerializedFrames_); !e.empty(); e.popFront())
        e.front().value().trace(trc);
}

bool
jit::JitActivation::registerIonFrameRecovery(RInstructionResults&& results)
{
    // Check that there is no entry in the vector yet.
    MOZ_ASSERT(!maybeIonFrameRecovery(results.frame()));
    if (!ionRecovery_.append(mozilla::Move(results)))
        return false;

    return true;
}

jit::RInstructionResults*
jit::JitActivation::maybeIonFrameRecovery(JitFrameLayout* fp)
{
    for (RInstructionResults* it = ionRecovery_.begin(); it != ionRecovery_.end(); ) {
        if (it->frame() == fp)
            return it;
    }

    return nullptr;
}

void
jit::JitActivation::removeIonFrameRecovery(JitFrameLayout* fp)
{
    RInstructionResults* elem = maybeIonFrameRecovery(fp);
    if (!elem)
        return;

    ionRecovery_.erase(elem);
}

void
jit::JitActivation::traceIonRecovery(JSTracer* trc)
{
    for (RInstructionResults* it = ionRecovery_.begin(); it != ionRecovery_.end(); it++)
        it->trace(trc);
}

WasmActivation::WasmActivation(JSContext* cx)
  : Activation(cx, Wasm),
    exitFP_(nullptr)
{
    // Now that the WasmActivation is fully initialized, make it visible to
    // asynchronous profiling.
    registerProfiling();
}

WasmActivation::~WasmActivation()
{
    // Hide this activation from the profiler before is is destroyed.
    unregisterProfiling();

    MOZ_ASSERT(!interrupted());
    MOZ_ASSERT(exitFP_ == nullptr);
}

void
WasmActivation::unwindExitFP(wasm::Frame* exitFP)
{
    exitFP_ = exitFP;
}

void
WasmActivation::startInterrupt(const JS::ProfilingFrameIterator::RegisterState& state)
{
    MOZ_ASSERT(state.pc);
    MOZ_ASSERT(state.fp);

    // Execution can only be interrupted in function code. Afterwards, control
    // flow does not reenter function code and thus there should be no
    // interrupt-during-interrupt.
    MOZ_ASSERT(!interrupted());

    bool ignoredUnwound;
    wasm::UnwindState unwindState;
    MOZ_ALWAYS_TRUE(wasm::StartUnwinding(*this, state, &unwindState, &ignoredUnwound));

    void* unwindPC = unwindState.pc;
    MOZ_ASSERT(compartment()->wasm.lookupCode(unwindPC)->lookupRange(unwindPC)->isFunction());

    cx_->runtime()->startWasmInterrupt(state.pc, unwindPC);
    exitFP_ = reinterpret_cast<wasm::Frame*>(unwindState.fp);

    MOZ_ASSERT(compartment() == exitFP_->tls->instance->compartment());
    MOZ_ASSERT(interrupted());
}

void
WasmActivation::finishInterrupt()
{
    MOZ_ASSERT(interrupted());
    MOZ_ASSERT(exitFP_);

    cx_->runtime()->finishWasmInterrupt();
    exitFP_ = nullptr;
}

bool
WasmActivation::interrupted() const
{
    void* pc = cx_->runtime()->wasmUnwindPC();
    if (!pc)
        return false;

    Activation* act = cx_->activation();
    while (act && !act->isWasm())
        act = act->prev();

    if (act->asWasm() != this)
        return false;

    DebugOnly<wasm::Frame*> fp = act->asWasm()->exitFP();
    MOZ_ASSERT(fp && fp->instance()->code().containsFunctionPC(pc));
    return true;
}

void*
WasmActivation::unwindPC() const
{
    MOZ_ASSERT(interrupted());
    return cx_->runtime()->wasmUnwindPC();
}

void*
WasmActivation::resumePC() const
{
    MOZ_ASSERT(interrupted());
    return cx_->runtime()->wasmResumePC();
}

InterpreterFrameIterator&
InterpreterFrameIterator::operator++()
{
    MOZ_ASSERT(!done());
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
    MOZ_ASSERT(isProfiling());
    cx_->profilingActivation_ = this;
}

void
Activation::unregisterProfiling()
{
    MOZ_ASSERT(isProfiling());
    MOZ_ASSERT(cx_->profilingActivation_ == this);

    // There may be a non-active jit activation in the linked list.  Skip past it.
    Activation* prevProfiling = prevProfiling_;
    while (prevProfiling && prevProfiling->isJit() && !prevProfiling->asJit()->isActive())
        prevProfiling = prevProfiling->prevProfiling_;

    cx_->profilingActivation_ = prevProfiling;
}

ActivationIterator::ActivationIterator(JSContext* cx)
  : activation_(cx->activation_)
{
    MOZ_ASSERT(cx == TlsContext.get());
    settle();
}

ActivationIterator::ActivationIterator(JSContext* cx, const CooperatingContext& target)
{
    MOZ_ASSERT(cx == TlsContext.get());

    // If target was specified --- even if it is the same as cx itself --- then
    // we must be in a scope where changes of the active context are prohibited.
    // Otherwise our state would be corrupted if the target thread resumed
    // execution while we are iterating over its state.
    MOZ_ASSERT(cx->runtime()->activeContextChangeProhibited() ||
               !cx->runtime()->gc.canChangeActiveContext(cx));

    // Tolerate a null target context, in case we are iterating over the
    // activations for a zone group that is not in use by any thread.
    activation_ = target.context() ? target.context()->activation_.ref() : nullptr;

    settle();
}

ActivationIterator&
ActivationIterator::operator++()
{
    MOZ_ASSERT(activation_);
    activation_ = activation_->prev();
    settle();
    return *this;
}

void
ActivationIterator::settle()
{
    // Stop at the next active activation.
    while (!done() && activation_->isJit() && !activation_->asJit()->isActive())
        activation_ = activation_->prev();
}

JS::ProfilingFrameIterator::ProfilingFrameIterator(JSContext* cx, const RegisterState& state,
                                                   uint32_t sampleBufferGen)
  : cx_(cx),
    sampleBufferGen_(sampleBufferGen),
    activation_(nullptr)
{
    if (!cx->runtime()->geckoProfiler().enabled())
        MOZ_CRASH("ProfilingFrameIterator called when geckoProfiler not enabled for runtime.");

    if (!cx->profilingActivation())
        return;

    // If profiler sampling is not enabled, skip.
    if (!cx->isProfilerSamplingEnabled())
        return;

    activation_ = cx->profilingActivation();

    MOZ_ASSERT(activation_->isProfiling());

    static_assert(sizeof(wasm::ProfilingFrameIterator) <= StorageSpace &&
                  sizeof(jit::JitProfilingFrameIterator) <= StorageSpace,
                  "ProfilingFrameIterator::storage_ is too small");
    static_assert(alignof(void*) >= alignof(wasm::ProfilingFrameIterator) &&
                  alignof(void*) >= alignof(jit::JitProfilingFrameIterator),
                  "ProfilingFrameIterator::storage_ is too weakly aligned");

    iteratorConstruct(state);
    settle();
}

JS::ProfilingFrameIterator::~ProfilingFrameIterator()
{
    if (!done()) {
        MOZ_ASSERT(activation_->isProfiling());
        iteratorDestroy();
    }
}

void
JS::ProfilingFrameIterator::operator++()
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm()) {
        ++wasmIter();
        settle();
        return;
    }

    ++jitIter();
    settle();
}

void
JS::ProfilingFrameIterator::settle()
{
    while (iteratorDone()) {
        iteratorDestroy();
        activation_ = activation_->prevProfiling();

        // Skip past any non-active jit activations in the list.
        while (activation_ && activation_->isJit() && !activation_->asJit()->isActive())
            activation_ = activation_->prevProfiling();

        if (!activation_)
            return;
        iteratorConstruct();
    }
}

void
JS::ProfilingFrameIterator::iteratorConstruct(const RegisterState& state)
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm()) {
        new (storage()) wasm::ProfilingFrameIterator(*activation_->asWasm(), state);
        return;
    }

    MOZ_ASSERT(activation_->asJit()->isActive());
    new (storage()) jit::JitProfilingFrameIterator(cx_, state);
}

void
JS::ProfilingFrameIterator::iteratorConstruct()
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm()) {
        new (storage()) wasm::ProfilingFrameIterator(*activation_->asWasm());
        return;
    }

    MOZ_ASSERT(activation_->asJit()->isActive());
    new (storage()) jit::JitProfilingFrameIterator(activation_->asJit()->exitFP());
}

void
JS::ProfilingFrameIterator::iteratorDestroy()
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm()) {
        wasmIter().~ProfilingFrameIterator();
        return;
    }

    jitIter().~JitProfilingFrameIterator();
}

bool
JS::ProfilingFrameIterator::iteratorDone()
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm())
        return wasmIter().done();

    return jitIter().done();
}

void*
JS::ProfilingFrameIterator::stackAddress() const
{
    MOZ_ASSERT(!done());
    MOZ_ASSERT(activation_->isWasm() || activation_->isJit());

    if (activation_->isWasm())
        return wasmIter().stackAddress();

    return jitIter().stackAddress();
}

Maybe<JS::ProfilingFrameIterator::Frame>
JS::ProfilingFrameIterator::getPhysicalFrameAndEntry(jit::JitcodeGlobalEntry* entry) const
{
    void* stackAddr = stackAddress();

    if (isWasm()) {
        Frame frame;
        frame.kind = Frame_Wasm;
        frame.stackAddress = stackAddr;
        frame.returnAddress = nullptr;
        frame.activation = activation_;
        frame.label = nullptr;
        return mozilla::Some(frame);
    }

    MOZ_ASSERT(isJit());

    // Look up an entry for the return address.
    void* returnAddr = jitIter().returnAddressToFp();
    jit::JitcodeGlobalTable* table = cx_->runtime()->jitRuntime()->getJitcodeGlobalTable();
    if (hasSampleBufferGen())
        *entry = table->lookupForSamplerInfallible(returnAddr, cx_->runtime(), sampleBufferGen_);
    else
        *entry = table->lookupInfallible(returnAddr);

    MOZ_ASSERT(entry->isIon() || entry->isIonCache() || entry->isBaseline() || entry->isDummy());

    // Dummy frames produce no stack frames.
    if (entry->isDummy())
        return mozilla::Nothing();

    Frame frame;
    frame.kind = entry->isBaseline() ? Frame_Baseline : Frame_Ion;
    frame.stackAddress = stackAddr;
    frame.returnAddress = returnAddr;
    frame.activation = activation_;
    frame.label = nullptr;
    return mozilla::Some(frame);
}

uint32_t
JS::ProfilingFrameIterator::extractStack(Frame* frames, uint32_t offset, uint32_t end) const
{
    if (offset >= end)
        return 0;

    jit::JitcodeGlobalEntry entry;
    Maybe<Frame> physicalFrame = getPhysicalFrameAndEntry(&entry);

    // Dummy frames produce no stack frames.
    if (physicalFrame.isNothing())
        return 0;

    if (isWasm()) {
        frames[offset] = physicalFrame.value();
        frames[offset].label = wasmIter().label();
        return 1;
    }

    // Extract the stack for the entry.  Assume maximum inlining depth is <64
    const char* labels[64];
    uint32_t depth = entry.callStackAtAddr(cx_->runtime(), jitIter().returnAddressToFp(),
                                           labels, ArrayLength(labels));
    MOZ_ASSERT(depth < ArrayLength(labels));
    for (uint32_t i = 0; i < depth; i++) {
        if (offset + i >= end)
            return i;
        frames[offset + i] = physicalFrame.value();
        frames[offset + i].label = labels[i];
    }

    return depth;
}

Maybe<JS::ProfilingFrameIterator::Frame>
JS::ProfilingFrameIterator::getPhysicalFrameWithoutLabel() const
{
    jit::JitcodeGlobalEntry unused;
    return getPhysicalFrameAndEntry(&unused);
}

bool
JS::ProfilingFrameIterator::isWasm() const
{
    MOZ_ASSERT(!done());
    return activation_->isWasm();
}

bool
JS::ProfilingFrameIterator::isJit() const
{
    return activation_->isJit();
}
