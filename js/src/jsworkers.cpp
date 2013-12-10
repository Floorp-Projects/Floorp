/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsworkers.h"

#ifdef JS_WORKER_THREADS
#include "mozilla/DebugOnly.h"

#include "jsnativestack.h"
#include "prmjtime.h"

#include "frontend/BytecodeCompiler.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonBuilder.h"
#include "vm/Debugger.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::DebugOnly;

bool
js::EnsureWorkerThreadsInitialized(ExclusiveContext *cx)
{
    // If 'cx' is not a JSContext, we are already off the main thread and the
    // worker threads would have already been initialized.
    if (!cx->isJSContext()) {
        JS_ASSERT(cx->workerThreadState() != nullptr);
        return true;
    }

    JSRuntime *rt = cx->asJSContext()->runtime();
    if (rt->workerThreadState)
        return true;

    rt->workerThreadState = rt->new_<WorkerThreadState>(rt);
    if (!rt->workerThreadState)
        return false;

    if (!rt->workerThreadState->init()) {
        js_delete(rt->workerThreadState);
        rt->workerThreadState = nullptr;
        return false;
    }

    return true;
}

bool
js::StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData)
{
    // Threads already initialized by the AsmJS compiler.
    JS_ASSERT(cx->workerThreadState() != nullptr);
    JS_ASSERT(asmData->mir);
    JS_ASSERT(asmData->lir == nullptr);

    WorkerThreadState &state = *cx->workerThreadState();
    JS_ASSERT(state.numThreads);

    AutoLockWorkerThreadState lock(state);

    // Don't append this task if another failed.
    if (state.asmJSWorkerFailed())
        return false;

    if (!state.asmJSWorklist.append(asmData))
        return false;

    state.notifyOne(WorkerThreadState::PRODUCER);
    return true;
}

bool
js::StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder)
{
    if (!EnsureWorkerThreadsInitialized(cx))
        return false;

    WorkerThreadState &state = *cx->runtime()->workerThreadState;
    JS_ASSERT(state.numThreads);

    AutoLockWorkerThreadState lock(state);

    if (!state.ionWorklist.append(builder))
        return false;

    state.notifyAll(WorkerThreadState::PRODUCER);
    return true;
}

/*
 * Move an IonBuilder for which compilation has either finished, failed, or
 * been cancelled into the Ion compartment's finished compilations list.
 * All off thread compilations which are started must eventually be finished.
 */
static void
FinishOffThreadIonCompile(jit::IonBuilder *builder)
{
    JSCompartment *compartment = builder->script()->compartment();
    JS_ASSERT(compartment->runtimeFromAnyThread()->workerThreadState);
    JS_ASSERT(compartment->runtimeFromAnyThread()->workerThreadState->isLocked());

    compartment->jitCompartment()->finishedOffThreadCompilations().append(builder);
}

static inline bool
CompiledScriptMatches(JSCompartment *compartment, JSScript *script, JSScript *target)
{
    if (script)
        return target == script;
    return target->compartment() == compartment;
}

void
js::CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script)
{
    JSRuntime *rt = compartment->runtimeFromMainThread();

    if (!rt->workerThreadState)
        return;

    WorkerThreadState &state = *rt->workerThreadState;

    jit::JitCompartment *jitComp = compartment->jitCompartment();
    if (!jitComp)
        return;

    AutoLockWorkerThreadState lock(state);

    /* Cancel any pending entries for which processing hasn't started. */
    for (size_t i = 0; i < state.ionWorklist.length(); i++) {
        jit::IonBuilder *builder = state.ionWorklist[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            FinishOffThreadIonCompile(builder);
            state.ionWorklist[i--] = state.ionWorklist.back();
            state.ionWorklist.popBack();
        }
    }

    /* Wait for in progress entries to finish up. */
    for (size_t i = 0; i < state.numThreads; i++) {
        const WorkerThread &helper = state.threads[i];
        while (helper.ionBuilder &&
               CompiledScriptMatches(compartment, script, helper.ionBuilder->script()))
        {
            helper.ionBuilder->cancel();
            state.wait(WorkerThreadState::CONSUMER);
        }
    }

    jit::OffThreadCompilationVector &compilations = jitComp->finishedOffThreadCompilations();

    /* Cancel code generation for any completed entries. */
    for (size_t i = 0; i < compilations.length(); i++) {
        jit::IonBuilder *builder = compilations[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            jit::FinishOffThreadBuilder(builder);
            compilations[i--] = compilations.back();
            compilations.popBack();
        }
    }
}

static const JSClass workerGlobalClass = {
    "internal-worker-global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   nullptr
};

ParseTask::ParseTask(ExclusiveContext *cx, JSObject *exclusiveContextGlobal, JSContext *initCx,
                     const jschar *chars, size_t length, JSObject *scopeChain,
                     JS::OffThreadCompileCallback callback, void *callbackData)
  : cx(cx), options(initCx), chars(chars), length(length),
    alloc(JSRuntime::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE), scopeChain(scopeChain),
    exclusiveContextGlobal(exclusiveContextGlobal), callback(callback),
    callbackData(callbackData), script(nullptr), errors(cx), overRecursed(false)
{
    JSRuntime *rt = scopeChain->runtimeFromMainThread();

    if (!AddObjectRoot(rt, &this->scopeChain, "ParseTask::scopeChain"))
        MOZ_CRASH();
    if (!AddObjectRoot(rt, &this->exclusiveContextGlobal, "ParseTask::exclusiveContextGlobal"))
        MOZ_CRASH();
}

bool
ParseTask::init(JSContext *cx, const ReadOnlyCompileOptions &options)
{
    return this->options.copy(cx, options);
}

void
ParseTask::activate(JSRuntime *rt)
{
    rt->setUsedByExclusiveThread(exclusiveContextGlobal->zone());
    cx->enterCompartment(exclusiveContextGlobal->compartment());
}

ParseTask::~ParseTask()
{
    JSRuntime *rt = scopeChain->runtimeFromMainThread();

    JS_RemoveObjectRootRT(rt, &scopeChain);
    JS_RemoveObjectRootRT(rt, &exclusiveContextGlobal);

    // ParseTask takes over ownership of its input exclusive context.
    js_delete(cx);

    for (size_t i = 0; i < errors.length(); i++)
        js_delete(errors[i]);
}

bool
js::StartOffThreadParseScript(JSContext *cx, const ReadOnlyCompileOptions &options,
                              const jschar *chars, size_t length, HandleObject scopeChain,
                              JS::OffThreadCompileCallback callback, void *callbackData)
{
    // Suppress GC so that calls below do not trigger a new incremental GC
    // which could require barriers on the atoms compartment.
    gc::AutoSuppressGC suppress(cx);

    frontend::MaybeCallSourceHandler(cx, options, chars, length);

    if (!EnsureWorkerThreadsInitialized(cx))
        return false;

    JS::CompartmentOptions compartmentOptions(cx->compartment()->options());
    compartmentOptions.setZone(JS::FreshZone);

    JSObject *global = JS_NewGlobalObject(cx, &workerGlobalClass, nullptr,
                                          JS::FireOnNewGlobalHook, compartmentOptions);
    if (!global)
        return false;

    global->zone()->types.inferenceEnabled = cx->typeInferenceEnabled();
    JS_SetCompartmentPrincipals(global->compartment(), cx->compartment()->principals);

    RootedObject obj(cx);

    // Initialize all classes needed for parsing while we are still on the main
    // thread. Do this for both the target and the new global so that prototype
    // pointers can be changed infallibly after parsing finishes.
    if (!js_GetClassObject(cx, cx->global(), JSProto_Function, &obj) ||
        !js_GetClassObject(cx, cx->global(), JSProto_Array, &obj) ||
        !js_GetClassObject(cx, cx->global(), JSProto_RegExp, &obj) ||
        !js_GetClassObject(cx, cx->global(), JSProto_GeneratorFunction, &obj))
    {
        return false;
    }
    {
        AutoCompartment ac(cx, global);
        if (!js_GetClassObject(cx, global, JSProto_Function, &obj) ||
            !js_GetClassObject(cx, global, JSProto_Array, &obj) ||
            !js_GetClassObject(cx, global, JSProto_RegExp, &obj) ||
            !js_GetClassObject(cx, global, JSProto_GeneratorFunction, &obj))
        {
            return false;
        }
    }

    ScopedJSDeletePtr<ExclusiveContext> workercx(
        cx->new_<ExclusiveContext>(cx->runtime(), (PerThreadData *) nullptr,
                                   ThreadSafeContext::Context_Exclusive));
    if (!workercx)
        return false;

    ScopedJSDeletePtr<ParseTask> task(
        cx->new_<ParseTask>(workercx.get(), global, cx, chars, length,
                            scopeChain, callback, callbackData));
    if (!task)
        return false;

    workercx.forget();

    if (!task->init(cx, options))
        return false;

    WorkerThreadState &state = *cx->runtime()->workerThreadState;
    JS_ASSERT(state.numThreads);

    // Off thread parsing can't occur during incremental collections on the
    // atoms compartment, to avoid triggering barriers. (Outside the atoms
    // compartment, the compilation will use a new zone which doesn't require
    // barriers itself.) If an atoms-zone GC is in progress, hold off on
    // executing the parse task until the atoms-zone GC completes (see
    // EnqueuePendingParseTasksAfterGC).
    if (cx->runtime()->activeGCInAtomsZone()) {
        if (!state.parseWaitingOnGC.append(task.get()))
            return false;
    } else {
        task->activate(cx->runtime());

        AutoLockWorkerThreadState lock(state);

        if (!state.parseWorklist.append(task.get()))
            return false;

        state.notifyAll(WorkerThreadState::PRODUCER);
    }

    task.forget();

    return true;
}

void
js::EnqueuePendingParseTasksAfterGC(JSRuntime *rt)
{
    JS_ASSERT(!rt->activeGCInAtomsZone());

    if (!rt->workerThreadState || rt->workerThreadState->parseWaitingOnGC.empty())
        return;

    // This logic should mirror the contents of the !activeGCInAtomsZone()
    // branch in StartOffThreadParseScript:

    WorkerThreadState &state = *rt->workerThreadState;

    for (size_t i = 0; i < state.parseWaitingOnGC.length(); i++)
        state.parseWaitingOnGC[i]->activate(rt);

    AutoLockWorkerThreadState lock(state);

    JS_ASSERT(state.parseWorklist.empty());
    state.parseWorklist.swap(state.parseWaitingOnGC);

    state.notifyAll(WorkerThreadState::PRODUCER);
}

void
js::WaitForOffThreadParsingToFinish(JSRuntime *rt)
{
    if (!rt->workerThreadState)
        return;

    WorkerThreadState &state = *rt->workerThreadState;

    AutoLockWorkerThreadState lock(state);

    while (true) {
        if (state.parseWorklist.empty()) {
            bool parseInProgress = false;
            for (size_t i = 0; i < state.numThreads; i++)
                parseInProgress |= !!state.threads[i].parseTask;
            if (!parseInProgress)
                break;
        }
        state.wait(WorkerThreadState::CONSUMER);
    }
}

#ifdef XP_WIN
// The default stack size for new threads on Windows is 1MB, but specifying a
// smaller explicit size to NSPR on thread creation causes our visible memory
// usage to increase. Just use the default stack size on Windows.
static const uint32_t WORKER_STACK_SIZE = 0;
#else
static const uint32_t WORKER_STACK_SIZE = 512 * 1024;
#endif

static const uint32_t WORKER_STACK_QUOTA = 450 * 1024;

bool
WorkerThreadState::init()
{
    JS_ASSERT(numThreads == 0);

    if (!runtime->useHelperThreads())
        return true;

    workerLock = PR_NewLock();
    if (!workerLock)
        return false;

    consumerWakeup = PR_NewCondVar(workerLock);
    if (!consumerWakeup)
        return false;

    producerWakeup = PR_NewCondVar(workerLock);
    if (!producerWakeup)
        return false;

    threads = (WorkerThread*) js_pod_calloc<WorkerThread>(runtime->workerThreadCount());
    if (!threads)
        return false;

    for (size_t i = 0; i < runtime->workerThreadCount(); i++) {
        WorkerThread &helper = threads[i];
        helper.runtime = runtime;
        helper.threadData.construct(runtime);
        helper.threadData.ref().addToThreadList();
        helper.thread = PR_CreateThread(PR_USER_THREAD,
                                        WorkerThread::ThreadMain, &helper,
                                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, WORKER_STACK_SIZE);
        if (!helper.thread || !helper.threadData.ref().init()) {
            for (size_t j = 0; j < runtime->workerThreadCount(); j++)
                threads[j].destroy();
            js_free(threads);
            threads = nullptr;
            return false;
        }
    }

    numThreads = runtime->workerThreadCount();
    resetAsmJSFailureState();
    return true;
}

void
WorkerThreadState::cleanup()
{
    // Do preparatory work for shutdown before the final GC has destroyed most
    // of the GC heap.

    // Join created threads, to ensure there is no in progress work.
    if (threads) {
        for (size_t i = 0; i < numThreads; i++)
            threads[i].destroy();
        js_free(threads);
        threads = nullptr;
        numThreads = 0;
    }

    // Clean up any parse tasks which haven't been finished yet.
    while (!parseFinishedList.empty())
        finishParseTask(/* maybecx = */ nullptr, runtime, parseFinishedList[0]);
}

WorkerThreadState::~WorkerThreadState()
{
    JS_ASSERT(!threads);
    JS_ASSERT(parseFinishedList.empty());

    if (workerLock)
        PR_DestroyLock(workerLock);

    if (consumerWakeup)
        PR_DestroyCondVar(consumerWakeup);

    if (producerWakeup)
        PR_DestroyCondVar(producerWakeup);
}

void
WorkerThreadState::lock()
{
    runtime->assertCanLock(JSRuntime::WorkerThreadStateLock);
    PR_Lock(workerLock);
#ifdef DEBUG
    lockOwner = PR_GetCurrentThread();
#endif
}

void
WorkerThreadState::unlock()
{
    JS_ASSERT(isLocked());
#ifdef DEBUG
    lockOwner = nullptr;
#endif
    PR_Unlock(workerLock);
}

#ifdef DEBUG
bool
WorkerThreadState::isLocked()
{
    return lockOwner == PR_GetCurrentThread();
}
#endif

void
WorkerThreadState::wait(CondVar which, uint32_t millis)
{
    JS_ASSERT(isLocked());
#ifdef DEBUG
    lockOwner = nullptr;
#endif
    DebugOnly<PRStatus> status =
        PR_WaitCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup,
                       millis ? PR_MillisecondsToInterval(millis) : PR_INTERVAL_NO_TIMEOUT);
    JS_ASSERT(status == PR_SUCCESS);
#ifdef DEBUG
    lockOwner = PR_GetCurrentThread();
#endif
}

void
WorkerThreadState::notifyAll(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyAllCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

void
WorkerThreadState::notifyOne(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

bool
WorkerThreadState::canStartAsmJSCompile()
{
    // Don't execute an AsmJS job if an earlier one failed.
    JS_ASSERT(isLocked());
    return (!asmJSWorklist.empty() && !numAsmJSFailedJobs);
}

bool
WorkerThreadState::canStartIonCompile()
{
    // A worker thread can begin an Ion compilation if (a) there is some script
    // which is waiting to be compiled, and (b) no other worker thread is
    // currently compiling a script. The latter condition ensures that two
    // compilations cannot simultaneously occur.
    if (ionWorklist.empty())
        return false;
    for (size_t i = 0; i < numThreads; i++) {
        if (threads[i].ionBuilder)
            return false;
    }
    return true;
}

bool
WorkerThreadState::canStartParseTask()
{
    // Don't allow simultaneous off thread parses, to reduce contention on the
    // atoms table. Note that asm.js compilation depends on this to avoid
    // stalling the worker thread, as off thread parse tasks can trigger and
    // block on other off thread asm.js compilation tasks.
    JS_ASSERT(isLocked());
    if (parseWorklist.empty())
        return false;
    for (size_t i = 0; i < numThreads; i++) {
        if (threads[i].parseTask)
            return false;
    }
    return true;
}

bool
WorkerThreadState::canStartCompressionTask()
{
    return !compressionWorklist.empty();
}

static void
CallNewScriptHookForAllScripts(JSContext *cx, HandleScript script)
{
    // We should never hit this, since nested scripts are also constructed via
    // BytecodeEmitter instances on the stack.
    JS_CHECK_RECURSION(cx, return);

    // Recurse to any nested scripts.
    if (script->hasObjects()) {
        ObjectArray *objects = script->objects();
        for (size_t i = 0; i < objects->length; i++) {
            JSObject *obj = objects->vector[i];
            if (obj->is<JSFunction>()) {
                JSFunction *fun = &obj->as<JSFunction>();
                if (fun->hasScript()) {
                    RootedScript nested(cx, fun->nonLazyScript());
                    CallNewScriptHookForAllScripts(cx, nested);
                }
            }
        }
    }

    // The global new script hook is called on every script that was compiled.
    RootedFunction function(cx, script->function());
    CallNewScriptHook(cx, script, function);
}

JSScript *
WorkerThreadState::finishParseTask(JSContext *maybecx, JSRuntime *rt, void *token)
{
    ParseTask *parseTask = nullptr;

    // The token is a ParseTask* which should be in the finished list.
    // Find and remove its entry.
    {
        AutoLockWorkerThreadState lock(*rt->workerThreadState);
        for (size_t i = 0; i < parseFinishedList.length(); i++) {
            if (parseFinishedList[i] == token) {
                parseTask = parseFinishedList[i];
                parseFinishedList[i] = parseFinishedList.back();
                parseFinishedList.popBack();
                break;
            }
        }
    }
    JS_ASSERT(parseTask);

    // Mark the zone as no longer in use by an ExclusiveContext, and available
    // to be collected by the GC.
    rt->clearUsedByExclusiveThread(parseTask->cx->zone());

    // Point the prototypes of any objects in the script's compartment to refer
    // to the corresponding prototype in the new compartment. This will briefly
    // create cross compartment pointers, which will be fixed by the
    // MergeCompartments call below.
    for (gc::CellIter iter(parseTask->cx->zone(), gc::FINALIZE_TYPE_OBJECT);
         !iter.done();
         iter.next())
    {
        types::TypeObject *object = iter.get<types::TypeObject>();
        TaggedProto proto(object->proto);
        if (!proto.isObject())
            continue;

        JSProtoKey key = js_IdentifyClassPrototype(proto.toObject());
        if (key == JSProto_Null)
            continue;

        JSObject *newProto = GetClassPrototypePure(&parseTask->scopeChain->global(), key);
        JS_ASSERT(newProto);

        object->proto = newProto;
    }

    // Move the parsed script and all its contents into the desired compartment.
    gc::MergeCompartments(parseTask->cx->compartment(), parseTask->scopeChain->compartment());

    RootedScript script(rt, parseTask->script);

    // If we have a context, report any error or warnings generated during the
    // parse, and inform the debugger about the compiled scripts.
    if (maybecx) {
        AutoCompartment ac(maybecx, parseTask->scopeChain);
        for (size_t i = 0; i < parseTask->errors.length(); i++)
            parseTask->errors[i]->throwError(maybecx);
        if (parseTask->overRecursed)
            js_ReportOverRecursed(maybecx);

        if (script) {
            // The Debugger only needs to be told about the topmost script that was compiled.
            GlobalObject *compileAndGoGlobal = nullptr;
            if (script->compileAndGo)
                compileAndGoGlobal = &script->global();
            Debugger::onNewScript(maybecx, script, compileAndGoGlobal);

            // The NewScript hook needs to be called for all compiled scripts.
            CallNewScriptHookForAllScripts(maybecx, script);
        }
    }

    js_delete(parseTask);
    return script;
}

void
WorkerThread::destroy()
{
    WorkerThreadState &state = *runtime->workerThreadState;

    if (thread) {
        {
            AutoLockWorkerThreadState lock(state);
            terminate = true;

            /* Notify all workers, to ensure that this thread wakes up. */
            state.notifyAll(WorkerThreadState::PRODUCER);
        }

        PR_JoinThread(thread);
    }

    if (!threadData.empty()) {
        threadData.ref().removeFromThreadList();
        threadData.destroy();
    }
}

/* static */
void
WorkerThread::ThreadMain(void *arg)
{
    PR_SetCurrentThreadName("Analysis Helper");
    static_cast<WorkerThread *>(arg)->threadLoop();
}

void
WorkerThread::handleAsmJSWorkload(WorkerThreadState &state)
{
    JS_ASSERT(state.isLocked());
    JS_ASSERT(state.canStartAsmJSCompile());
    JS_ASSERT(idle());

    asmData = state.asmJSWorklist.popCopy();
    bool success = false;

    state.unlock();
    do {
        jit::IonContext icx(jit::CompileRuntime::get(runtime), asmData->mir->compartment, &asmData->mir->alloc());

        int64_t before = PRMJ_Now();

        if (!OptimizeMIR(asmData->mir))
            break;

        asmData->lir = GenerateLIR(asmData->mir);
        if (!asmData->lir)
            break;

        int64_t after = PRMJ_Now();
        asmData->compileTime = (after - before) / PRMJ_USEC_PER_MSEC;

        success = true;
    } while(0);
    state.lock();

    // On failure, signal parent for harvesting in CancelOutstandingJobs().
    if (!success) {
        state.noteAsmJSFailure(asmData->func);
        state.notifyAll(WorkerThreadState::CONSUMER);
        asmData = nullptr;
        return;
    }

    // On success, move work to the finished list.
    state.asmJSFinishedList.append(asmData);
    asmData = nullptr;

    // Notify the main thread in case it's blocked waiting for a LifoAlloc.
    state.notifyAll(WorkerThreadState::CONSUMER);
}

void
WorkerThread::handleIonWorkload(WorkerThreadState &state)
{
    JS_ASSERT(state.isLocked());
    JS_ASSERT(state.canStartIonCompile());
    JS_ASSERT(idle());

    ionBuilder = state.ionWorklist.popCopy();

    DebugOnly<ExecutionMode> executionMode = ionBuilder->info().executionMode();
    JS_ASSERT(jit::GetIonScript(ionBuilder->script(), executionMode) == ION_COMPILING_SCRIPT);

#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::getLogger(TraceLogging::ION_BACKGROUND_COMPILER),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        ionBuilder->script());
#endif

    state.unlock();
    {
        jit::IonContext ictx(jit::CompileRuntime::get(runtime),
                             jit::CompileCompartment::get(ionBuilder->script()->compartment()),
                             &ionBuilder->alloc());
        ionBuilder->setBackgroundCodegen(jit::CompileBackEnd(ionBuilder));
    }
    state.lock();

    FinishOffThreadIonCompile(ionBuilder);
    ionBuilder = nullptr;

    // Notify the main thread in case it is waiting for the compilation to finish.
    state.notifyAll(WorkerThreadState::CONSUMER);

    // Ping the main thread so that the compiled code can be incorporated
    // at the next operation callback. Don't interrupt Ion code for this, as
    // this incorporation can be delayed indefinitely without affecting
    // performance as long as the main thread is actually executing Ion code.
    runtime->triggerOperationCallback(JSRuntime::TriggerCallbackAnyThreadDontStopIon);
}

void
ExclusiveContext::setWorkerThread(WorkerThread *workerThread)
{
    workerThread_ = workerThread;
    perThreadData = workerThread->threadData.addr();
}

frontend::CompileError &
ExclusiveContext::addPendingCompileError()
{
    frontend::CompileError *error = js_new<frontend::CompileError>();
    if (!error)
        MOZ_CRASH();
    if (!workerThread()->parseTask->errors.append(error))
        MOZ_CRASH();
    return *error;
}

void
ExclusiveContext::addPendingOverRecursed()
{
    if (workerThread()->parseTask)
        workerThread()->parseTask->overRecursed = true;
}

void
WorkerThread::handleParseWorkload(WorkerThreadState &state)
{
    JS_ASSERT(state.isLocked());
    JS_ASSERT(state.canStartParseTask());
    JS_ASSERT(idle());

    parseTask = state.parseWorklist.popCopy();
    parseTask->cx->setWorkerThread(this);

    {
        AutoUnlockWorkerThreadState unlock(runtime);
        parseTask->script = frontend::CompileScript(parseTask->cx, &parseTask->alloc,
                                                    NullPtr(), NullPtr(),
                                                    parseTask->options,
                                                    parseTask->chars, parseTask->length);
    }

    // The callback is invoked while we are still off the main thread.
    parseTask->callback(parseTask, parseTask->callbackData);

    // FinishOffThreadScript will need to be called on the script to
    // migrate it into the correct compartment.
    state.parseFinishedList.append(parseTask);

    parseTask = nullptr;

    // Notify the main thread in case it is waiting for the parse/emit to finish.
    state.notifyAll(WorkerThreadState::CONSUMER);
}

void
WorkerThread::handleCompressionWorkload(WorkerThreadState &state)
{
    JS_ASSERT(state.isLocked());
    JS_ASSERT(state.canStartCompressionTask());
    JS_ASSERT(idle());

    compressionTask = state.compressionWorklist.popCopy();
    compressionTask->workerThread = this;

    {
        AutoUnlockWorkerThreadState unlock(runtime);
        if (!compressionTask->compress())
            compressionTask->setOOM();
    }

    compressionTask->workerThread = nullptr;
    compressionTask = nullptr;

    // Notify the main thread in case it is waiting for the compression to finish.
    state.notifyAll(WorkerThreadState::CONSUMER);
}

bool
js::StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task)
{
    if (!EnsureWorkerThreadsInitialized(cx))
        return false;

    WorkerThreadState &state = *cx->workerThreadState();
    AutoLockWorkerThreadState lock(state);

    if (!state.compressionWorklist.append(task))
        return false;

    state.notifyAll(WorkerThreadState::PRODUCER);
    return true;
}

bool
WorkerThreadState::compressionInProgress(SourceCompressionTask *task)
{
    JS_ASSERT(isLocked());
    for (size_t i = 0; i < compressionWorklist.length(); i++) {
        if (compressionWorklist[i] == task)
            return true;
    }
    for (size_t i = 0; i < numThreads; i++) {
        if (threads[i].compressionTask == task)
            return true;
    }
    return false;
}

bool
SourceCompressionTask::complete()
{
    JS_ASSERT_IF(!ss, !chars);
    if (active()) {
        WorkerThreadState &state = *cx->workerThreadState();
        AutoLockWorkerThreadState lock(state);

        while (state.compressionInProgress(this))
            state.wait(WorkerThreadState::CONSUMER);

        ss->ready_ = true;

        // Update memory accounting.
        if (!oom)
            cx->updateMallocCounter(ss->computedSizeOfData());

        ss = nullptr;
        chars = nullptr;
    }
    if (oom) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

SourceCompressionTask *
WorkerThreadState::compressionTaskForSource(ScriptSource *ss)
{
    JS_ASSERT(isLocked());
    for (size_t i = 0; i < compressionWorklist.length(); i++) {
        SourceCompressionTask *task = compressionWorklist[i];
        if (task->source() == ss)
            return task;
    }
    for (size_t i = 0; i < numThreads; i++) {
        SourceCompressionTask *task = threads[i].compressionTask;
        if (task && task->source() == ss)
            return task;
    }
    return nullptr;
}

const jschar *
ScriptSource::getOffThreadCompressionChars(ExclusiveContext *cx)
{
    // If this is being compressed off thread, return its uncompressed chars.

    if (ready()) {
        // Compression has already finished on the source.
        return nullptr;
    }

    WorkerThreadState &state = *cx->workerThreadState();
    AutoLockWorkerThreadState lock(state);

    // Look for a token that hasn't finished compressing and whose source is
    // the given ScriptSource.
    if (SourceCompressionTask *task = state.compressionTaskForSource(this))
        return task->uncompressedChars();

    // Compressing has finished, so this ScriptSource is ready. Avoid future
    // queries on the worker thread state when getting the chars.
    ready_ = true;

    return nullptr;
}

void
WorkerThread::threadLoop()
{
    WorkerThreadState &state = *runtime->workerThreadState;
    AutoLockWorkerThreadState lock(state);

    js::TlsPerThreadData.set(threadData.addr());

    // Compute the thread's stack limit, for over-recursed checks.
    uintptr_t stackLimit = GetNativeStackBase();
#if JS_STACK_GROWTH_DIRECTION > 0
    stackLimit += WORKER_STACK_QUOTA;
#else
    stackLimit -= WORKER_STACK_QUOTA;
#endif
    for (size_t i = 0; i < ArrayLength(threadData.ref().nativeStackLimit); i++)
        threadData.ref().nativeStackLimit[i] = stackLimit;

    while (true) {
        JS_ASSERT(!ionBuilder && !asmData);

        // Block until a task is available.
        while (true) {
            if (terminate)
                return;
            if (state.canStartIonCompile() ||
                state.canStartAsmJSCompile() ||
                state.canStartParseTask() ||
                state.canStartCompressionTask())
            {
                break;
            }
            state.wait(WorkerThreadState::PRODUCER);
        }

        // Dispatch tasks, prioritizing AsmJS work.
        if (state.canStartAsmJSCompile())
            handleAsmJSWorkload(state);
        else if (state.canStartIonCompile())
            handleIonWorkload(state);
        else if (state.canStartParseTask())
            handleParseWorkload(state);
        else if (state.canStartCompressionTask())
            handleCompressionWorkload(state);
        else
            MOZ_ASSUME_UNREACHABLE("No task to perform");
    }
}

#else /* JS_WORKER_THREADS */

using namespace js;

bool
js::StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData)
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available in non-THREADSAFE builds");
}

bool
js::StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder)
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available in non-THREADSAFE builds");
}

void
js::CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script)
{
}

bool
js::StartOffThreadParseScript(JSContext *cx, const ReadOnlyCompileOptions &options,
                              const jschar *chars, size_t length, HandleObject scopeChain,
                              JS::OffThreadCompileCallback callback, void *callbackData)
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available in non-THREADSAFE builds");
}

void
js::WaitForOffThreadParsingToFinish(JSRuntime *rt)
{
}

bool
js::StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task)
{
    MOZ_ASSUME_UNREACHABLE("Off thread compression not available");
}

bool
SourceCompressionTask::complete()
{
    JS_ASSERT(!active() && !oom);
    return true;
}

const jschar *
ScriptSource::getOffThreadCompressionChars(ExclusiveContext *cx)
{
    JS_ASSERT(ready());
    return nullptr;
}

frontend::CompileError &
ExclusiveContext::addPendingCompileError()
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available.");
}

void
ExclusiveContext::addPendingOverRecursed()
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available.");
}

#endif /* JS_WORKER_THREADS */
