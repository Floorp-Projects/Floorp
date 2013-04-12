/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PodOperations.h"

#include "jsapi.h"
#include "jsobj.h"
#include "jsarray.h"

#include "vm/ForkJoin.h"
#include "vm/GlobalObject.h"
#include "vm/ParallelDo.h"
#include "vm/String.h"
#include "vm/ThreadPool.h"

#include "jsinterpinlines.h"
#include "jsobjinlines.h"

#ifdef JS_ION
#include "ion/ParallelArrayAnalysis.h"
#endif // ION

#if defined(DEBUG) && defined(JS_THREADSAFE) && defined(JS_ION)
#include "ion/Ion.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/IonCompartment.h"

#include "prprf.h"
#endif // DEBUG && THREADSAFE && ION

using namespace js;
using namespace js::parallel;
using namespace js::ion;

using mozilla::PodArrayZero;

//
// Debug spew
//

#if defined(DEBUG) && defined(JS_THREADSAFE) && defined(JS_ION)

static const char *
ExecutionStatusToString(ExecutionStatus status)
{
    switch (status) {
      case ExecutionFatal:
        return "fatal";
      case ExecutionSequential:
        return "sequential";
      case ExecutionParallel:
        return "parallel";
    }
    return "(unknown status)";
}

static const char *
MethodStatusToString(MethodStatus status)
{
    switch (status) {
      case Method_Error:
        return "error";
      case Method_CantCompile:
        return "can't compile";
      case Method_Skipped:
        return "skipped";
      case Method_Compiled:
        return "compiled";
    }
    return "(unknown status)";
}

static const size_t BufferSize = 4096;

class ParallelSpewer
{
    uint32_t depth;
    bool colorable;
    bool active[NumSpewChannels];

    const char *color(const char *colorCode) {
        if (!colorable)
            return "";
        return colorCode;
    }

    const char *reset() { return color("\x1b[0m"); }
    const char *bold() { return color("\x1b[1m"); }
    const char *red() { return color("\x1b[31m"); }
    const char *green() { return color("\x1b[32m"); }
    const char *yellow() { return color("\x1b[33m"); }
    const char *cyan() { return color("\x1b[36m"); }
    const char *sliceColor(uint32_t id) {
        static const char *colors[] = {
            "\x1b[7m\x1b[31m", "\x1b[7m\x1b[32m", "\x1b[7m\x1b[33m",
            "\x1b[7m\x1b[34m", "\x1b[7m\x1b[35m", "\x1b[7m\x1b[36m",
            "\x1b[7m\x1b[37m",
            "\x1b[31m", "\x1b[32m", "\x1b[33m",
            "\x1b[34m", "\x1b[35m", "\x1b[36m",
            "\x1b[37m"
        };
        return color(colors[id % 14]);
    }

  public:
    ParallelSpewer()
      : depth(0)
    {
        const char *env;

        PodArrayZero(active);
        env = getenv("PAFLAGS");
        if (env) {
            if (strstr(env, "ops"))
                active[SpewOps] = true;
            if (strstr(env, "compile"))
                active[SpewCompile] = true;
            if (strstr(env, "bailouts"))
                active[SpewBailouts] = true;
            if (strstr(env, "full")) {
                for (uint32_t i = 0; i < NumSpewChannels; i++)
                    active[i] = true;
            }
        }

        env = getenv("TERM");
        if (env) {
            if (strcmp(env, "xterm-color") == 0 || strcmp(env, "xterm-256color") == 0)
                colorable = true;
        }
    }

    bool isActive(SpewChannel channel) {
        return active[channel];
    }

    void spewVA(SpewChannel channel, const char *fmt, va_list ap) {
        if (!active[channel])
            return;

        // Print into a buffer first so we use one fprintf, which usually
        // doesn't get interrupted when running with multiple threads.
        char buf[BufferSize];

        if (ForkJoinSlice *slice = ForkJoinSlice::Current()) {
            PR_snprintf(buf, BufferSize, "[%sParallel:%u%s] ",
                        sliceColor(slice->sliceId), slice->sliceId, reset());
        } else {
            PR_snprintf(buf, BufferSize, "[Parallel:M] ");
        }

        for (uint32_t i = 0; i < depth; i++)
            PR_snprintf(buf + strlen(buf), BufferSize, "  ");

        PR_vsnprintf(buf + strlen(buf), BufferSize, fmt, ap);
        PR_snprintf(buf + strlen(buf), BufferSize, "\n");

        fprintf(stderr, "%s", buf);
    }

    void spew(SpewChannel channel, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        spewVA(channel, fmt, ap);
        va_end(ap);
    }

    void beginOp(JSContext *cx, const char *name) {
        if (!active[SpewOps])
            return;

        if (cx) {
            jsbytecode *pc;
            JSScript *script = cx->stack.currentScript(&pc);
            if (script && pc) {
                NonBuiltinScriptFrameIter iter(cx);
                if (iter.done()) {
                    spew(SpewOps, "%sBEGIN %s%s (%s:%u)", bold(), name, reset(),
                         script->filename(), PCToLineNumber(script, pc));
                } else {
                    spew(SpewOps, "%sBEGIN %s%s (%s:%u -> %s:%u)", bold(), name, reset(),
                         iter.script()->filename(), PCToLineNumber(iter.script(), iter.pc()),
                         script->filename(), PCToLineNumber(script, pc));
                }
            } else {
                spew(SpewOps, "%sBEGIN %s%s", bold(), name, reset());
            }
        } else {
            spew(SpewOps, "%sBEGIN %s%s", bold(), name, reset());
        }

        depth++;
    }

    void endOp(ExecutionStatus status) {
        if (!active[SpewOps])
            return;

        JS_ASSERT(depth > 0);
        depth--;

        const char *statusColor;
        switch (status) {
          case ExecutionFatal:
            statusColor = red();
            break;
          case ExecutionSequential:
            statusColor = yellow();
            break;
          case ExecutionParallel:
            statusColor = green();
            break;
          default:
            statusColor = reset();
            break;
        }

        spew(SpewOps, "%sEND %s%s%s", bold(),
             statusColor, ExecutionStatusToString(status), reset());
    }

    void bailout(uint32_t count) {
        if (!active[SpewOps])
            return;

        spew(SpewOps, "%s%sBAILOUT %d%s", bold(), yellow(), count, reset());
    }

    void beginCompile(HandleScript script) {
        if (!active[SpewCompile])
            return;

        spew(SpewCompile, "COMPILE %p:%s:%u", script.get(), script->filename(), script->lineno);
        depth++;
    }

    void endCompile(MethodStatus status) {
        if (!active[SpewCompile])
            return;

        JS_ASSERT(depth > 0);
        depth--;

        const char *statusColor;
        switch (status) {
          case Method_Error:
          case Method_CantCompile:
            statusColor = red();
            break;
          case Method_Skipped:
            statusColor = yellow();
            break;
          case Method_Compiled:
            statusColor = green();
            break;
          default:
            statusColor = reset();
            break;
        }

        spew(SpewCompile, "END %s%s%s", statusColor, MethodStatusToString(status), reset());
    }

    void spewMIR(MDefinition *mir, const char *fmt, va_list ap) {
        if (!active[SpewCompile])
            return;

        char buf[BufferSize];
        PR_vsnprintf(buf, BufferSize, fmt, ap);

        JSScript *script = mir->block()->info().script();
        spew(SpewCompile, "%s%s%s: %s (%s:%u)", cyan(), mir->opName(), reset(), buf,
             script->filename(), PCToLineNumber(script, mir->trackedPc()));
    }

    void spewBailoutIR(uint32_t bblockId, uint32_t lirId,
                       const char *lir, const char *mir, JSScript *script, jsbytecode *pc) {
        if (!active[SpewBailouts])
            return;

        // If we didn't bail from a LIR/MIR but from a propagated parallel
        // bailout, don't bother printing anything since we've printed it
        // elsewhere.
        if (mir && script) {
            spew(SpewBailouts, "%sBailout%s: %s / %s%s%s (block %d lir %d) (%s:%u)", yellow(), reset(),
                 lir, cyan(), mir, reset(),
                 bblockId, lirId,
                 script->filename(), PCToLineNumber(script, pc));
        }
    }
};

// Singleton instance of the spewer.
static ParallelSpewer spewer;

bool
parallel::SpewEnabled(SpewChannel channel)
{
    return spewer.isActive(channel);
}

void
parallel::Spew(SpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    spewer.spewVA(channel, fmt, ap);
    va_end(ap);
}

void
parallel::SpewBeginOp(JSContext *cx, const char *name)
{
    spewer.beginOp(cx, name);
}

ExecutionStatus
parallel::SpewEndOp(ExecutionStatus status)
{
    spewer.endOp(status);
    return status;
}

void
parallel::SpewBailout(uint32_t count)
{
    spewer.bailout(count);
}

void
parallel::SpewBeginCompile(HandleScript script)
{
    spewer.beginCompile(script);
}

MethodStatus
parallel::SpewEndCompile(MethodStatus status)
{
    spewer.endCompile(status);
    return status;
}

void
parallel::SpewMIR(MDefinition *mir, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    spewer.spewMIR(mir, fmt, ap);
    va_end(ap);
}

void
parallel::SpewBailoutIR(uint32_t bblockId, uint32_t lirId,
                        const char *lir, const char *mir,
                        JSScript *script, jsbytecode *pc)
{
    spewer.spewBailoutIR(bblockId, lirId, lir, mir, script, pc);
}

#endif // DEBUG && JS_THREADSAFE && JS_ION

#ifdef JS_ION
class AutoEnterWarmup
{
    JSRuntime *runtime_;

  public:
    AutoEnterWarmup(JSRuntime *runtime) : runtime_(runtime) { runtime_->parallelWarmup++; }
    ~AutoEnterWarmup() { runtime_->parallelWarmup--; }
};

// Can only enter callees with a valid IonScript.
template <uint32_t maxArgc>
class ParallelIonInvoke
{
    EnterIonCode enter_;
    void *jitcode_;
    void *calleeToken_;
    Value argv_[maxArgc + 2];
    uint32_t argc_;

  public:
    Value *args;

    ParallelIonInvoke(JSContext *cx, HandleFunction callee, uint32_t argc)
      : argc_(argc),
        args(argv_ + 2)
    {
        JS_ASSERT(argc <= maxArgc + 2);

        // Set 'callee' and 'this'.
        argv_[0] = ObjectValue(*callee);
        argv_[1] = UndefinedValue();

        // Find JIT code pointer.
        IonScript *ion = callee->nonLazyScript()->parallelIonScript();
        IonCode *code = ion->method();
        jitcode_ = code->raw();
        enter_ = cx->compartment->ionCompartment()->enterJIT();
        calleeToken_ = CalleeToParallelToken(callee);
    }

    bool invoke(JSContext *cx) {
        RootedValue result(cx);
        enter_(jitcode_, argc_ + 1, argv_ + 1, NULL, calleeToken_, NULL, 0, result.address());
        return !result.isMagic();
    }
};
#endif // JS_ION

class ParallelDo : public ForkJoinOp
{
    JSContext *cx_;
    RootedObject fun_;

  public:
    // For tests, make sure to keep this in sync with minItemsTestingThreshold.
    const static uint32_t MAX_BAILOUTS = 3;
    uint32_t bailouts;
    AutoScriptVector pendingInvalidations;

    ParallelDo(JSContext *cx, HandleObject fun)
      : cx_(cx),
        fun_(cx, fun),
        bailouts(0),
        pendingInvalidations(cx)
    { }

#ifndef JS_ION
    ExecutionStatus apply() {
        if (!executeSequentially())
            return ExecutionFatal;
        return ExecutionSequential;
    }
#else
    ExecutionStatus apply() {
        SpewBeginOp(cx_, "ParallelDo");

        if (!ion::IsEnabled(cx_))
            return SpewEndOp(disqualifyFromParallelExecution());

        if (!pendingInvalidations.resize(ForkJoinSlices(cx_)))
            return SpewEndOp(ExecutionFatal);

        // Try to execute in parallel.  If a bailout occurs, re-warmup
        // and then try again.  Repeat this a few times.
        while (bailouts < MAX_BAILOUTS) {
            MethodStatus status = compileForParallelExecution();
            if (status == Method_Error)
                return SpewEndOp(ExecutionFatal);
            if (status != Method_Compiled)
                return SpewEndOp(disqualifyFromParallelExecution());

            ParallelResult result = js::ExecuteForkJoinOp(cx_, *this);
            switch (result) {
              case TP_RETRY_SEQUENTIALLY:
                Spew(SpewBailouts, "Bailout not categorized");
                break;

              case TP_SUCCESS:
                return SpewEndOp(ExecutionParallel);

              case TP_FATAL:
                return SpewEndOp(ExecutionFatal);
            }

            bailouts += 1;
            SpewBailout(bailouts);

            if (!invalidateBailedOutScripts())
                return SpewEndOp(ExecutionFatal);

            if (!warmupForParallelExecution())
                return SpewEndOp(ExecutionFatal);
        }

        // After enough tries, just execute sequentially.
        return SpewEndOp(disqualifyFromParallelExecution());
    }

    MethodStatus compileForParallelExecution() {
        // The kernel should be a self-hosted function.
        if (!fun_->isFunction())
            return Method_Skipped;

        RootedFunction callee(cx_, fun_->toFunction());

        if (!callee->isInterpreted() || !callee->isSelfHostedBuiltin())
            return Method_Skipped;

        if (callee->isInterpretedLazy() && !callee->initializeLazyScript(cx_))
            return Method_Error;

        // If this function has not been run enough to enable parallel
        // execution, perform a warmup.
        RootedScript script(cx_, callee->nonLazyScript());
        if (script->getUseCount() < js_IonOptions.usesBeforeCompileParallel) {
            if (!warmupForParallelExecution())
                return Method_Error;
        }

        if (script->hasParallelIonScript() &&
            !script->parallelIonScript()->hasInvalidatedCallTarget())
        {
            Spew(SpewOps, "Already compiled");
            return Method_Compiled;
        }

        Spew(SpewOps, "Compiling all reachable functions");

        ParallelCompileContext compileContext(cx_);
        if (!compileContext.appendToWorklist(script))
            return Method_Error;

        MethodStatus status = compileContext.compileTransitively();
        if (status != Method_Compiled)
            return status;

        // it can happen that during transitive compilation, our
        // callee's parallel ion script is invalidated or GC'd. So
        // before we declare success, double check that it's still
        // compiled!
        if (!callee->nonLazyScript()->hasParallelIonScript())
            return Method_Skipped;

        return Method_Compiled;
    }

    ExecutionStatus disqualifyFromParallelExecution() {
        if (!executeSequentially())
            return ExecutionFatal;
        return ExecutionSequential;
    }

    bool invalidateBailedOutScripts() {
        RootedScript script(cx_, fun_->toFunction()->nonLazyScript());

        // Sometimes the script is collected or invalidated already,
        // for example when a full GC runs at an inconvenient time.
        if (!script->hasParallelIonScript()) {
            JS_ASSERT(hasNoPendingInvalidations());
            return true;
        }

        Vector<types::RecompileInfo> invalid(cx_);
        for (uint32_t i = 0; i < pendingInvalidations.length(); i++) {
            JSScript *script = pendingInvalidations[i];
            if (script && !hasScript(invalid, script)) {
                JS_ASSERT(script->hasParallelIonScript());
                if (!invalid.append(script->parallelIonScript()->recompileInfo()))
                    return false;
            }
            pendingInvalidations[i] = NULL;
        }
        Invalidate(cx_, invalid);
        return true;
    }

    bool warmupForParallelExecution() {
        AutoEnterWarmup warmup(cx_->runtime);
        return executeSequentially();
    }
#endif // JS_ION

    bool executeSequentially() {
        uint32_t numSlices = ForkJoinSlices(cx_);
        RootedValue funVal(cx_, ObjectValue(*fun_));
        FastInvokeGuard fig(cx_, funVal);
        for (uint32_t i = 0; i < numSlices; i++) {
            InvokeArgsGuard &args = fig.args();
            if (!args.pushed() && !cx_->stack.pushInvokeArgs(cx_, 3, &args))
                return false;
            args.setCallee(funVal);
            args.setThis(UndefinedValue());
            args[0].setInt32(i);
            args[1].setInt32(numSlices);
            args[2].setBoolean(!!cx_->runtime->parallelWarmup);
            if (!fig.invoke(cx_))
                return false;
        }
        return true;
    }

    virtual bool parallel(ForkJoinSlice &slice) {
#ifndef JS_ION
        JS_NOT_REACHED("Parallel execution without ion");
        return false;
#else
        Spew(SpewOps, "Up");

        // Make a new IonContext for the slice, which is needed if we need to
        // re-enter the VM.
        IonContext icx(cx_, NULL);

        JS_ASSERT(pendingInvalidations[slice.sliceId] == NULL);

        JS_ASSERT(fun_->isFunction());
        RootedFunction callee(cx_, fun_->toFunction());
        if (!callee->nonLazyScript()->hasParallelIonScript()) {
            // Sometimes, particularly with GCZeal, the parallel ion
            // script can be collected between starting the parallel
            // op and reaching this point.  In that case, we just fail
            // and fallback.
            Spew(SpewOps, "Down (Script no longer present)");
            return false;
        }

        ParallelIonInvoke<3> fii(cx_, callee, 3);

        fii.args[0] = Int32Value(slice.sliceId);
        fii.args[1] = Int32Value(slice.numSlices);
        fii.args[2] = BooleanValue(false);

        bool ok = fii.invoke(cx_);
        JS_ASSERT(ok == !slice.abortedScript);
        if (!ok) {
            JSScript *script = slice.abortedScript;
            Spew(SpewBailouts, "Aborted script: %p (hasParallelIonScript? %d)",
                 script, script->hasParallelIonScript());
            JS_ASSERT(script->hasParallelIonScript());
            pendingInvalidations[slice.sliceId] = script;
        }

        Spew(SpewOps, "Down");

        return ok;
#endif // JS_ION
    }

    inline bool hasScript(Vector<types::RecompileInfo> &scripts, JSScript *script) {
        for (uint32_t i = 0; i < scripts.length(); i++) {
            if (scripts[i] == script->parallelIonScript()->recompileInfo())
                return true;
        }
        return false;
    }

    inline bool hasNoPendingInvalidations() {
        for (uint32_t i = 0; i < pendingInvalidations.length(); i++) {
            if (pendingInvalidations[i] != NULL)
                return false;
        }
        return true;
    }
};

bool
js::parallel::Do(JSContext *cx, CallArgs &args)
{
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().isFunction());

    RootedObject fun(cx, &args[0].toObject());
    ParallelDo op(cx, fun);
    ExecutionStatus status = op.apply();
    if (status == ExecutionFatal)
        return false;

    if (args[1].isObject()) {
        RootedObject feedback(cx, &args[1].toObject());
        if (feedback && feedback->isFunction()) {
            InvokeArgsGuard feedbackArgs;
            if (!cx->stack.pushInvokeArgs(cx, 1, &feedbackArgs))
                return false;
            feedbackArgs.setCallee(ObjectValue(*feedback));
            feedbackArgs.setThis(UndefinedValue());
            if (status == ExecutionParallel)
                feedbackArgs[0].setInt32(op.bailouts);
            else
                feedbackArgs[0] = cx->runtime->positiveInfinityValue;
            if (!Invoke(cx, feedbackArgs))
                return false;
        }
    }

    return true;
}

