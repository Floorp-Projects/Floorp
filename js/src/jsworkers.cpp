/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsworkers.h"

#ifdef JS_THREADSAFE

#include "mozilla/DebugOnly.h"

#include "jsnativestack.h"
#include "prmjtime.h"

#include "frontend/BytecodeCompiler.h"
#include "jit/IonBuilder.h"
#include "vm/Debugger.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::DebugOnly;

namespace js {

GlobalWorkerThreadState gWorkerThreadState;

} // namespace js

bool
js::EnsureWorkerThreadsInitialized(ExclusiveContext *cx)
{
    // If 'cx' is not a JSContext, we are already off the main thread and the
    // worker threads would have already been initialized.
    if (!cx->isJSContext())
        return true;

    return WorkerThreadState().ensureInitialized();
}

static size_t
ThreadCountForCPUCount(size_t cpuCount)
{
    return Max(cpuCount, (size_t)2);
}

void
js::SetFakeCPUCount(size_t count)
{
    // This must be called before the threads have been initialized.
    JS_ASSERT(!WorkerThreadState().threads);

    WorkerThreadState().cpuCount = count;
    WorkerThreadState().threadCount = ThreadCountForCPUCount(count);
}

#ifdef JS_ION

bool
js::StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData)
{
    // Threads already initialized by the AsmJS compiler.
    JS_ASSERT(asmData->mir);
    JS_ASSERT(asmData->lir == nullptr);

    AutoLockWorkerThreadState lock;

    // Don't append this task if another failed.
    if (WorkerThreadState().asmJSWorkerFailed())
        return false;

    if (!WorkerThreadState().asmJSWorklist().append(asmData))
        return false;

    WorkerThreadState().notifyOne(GlobalWorkerThreadState::PRODUCER);
    return true;
}

bool
js::StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder)
{
    if (!EnsureWorkerThreadsInitialized(cx))
        return false;

    AutoLockWorkerThreadState lock;

    if (!WorkerThreadState().ionWorklist().append(builder))
        return false;

    WorkerThreadState().notifyOne(GlobalWorkerThreadState::PRODUCER);
    return true;
}

/*
 * Move an IonBuilder for which compilation has either finished, failed, or
 * been cancelled into the global finished compilation list. All off thread
 * compilations which are started must eventually be finished.
 */
static void
FinishOffThreadIonCompile(jit::IonBuilder *builder)
{
    WorkerThreadState().ionFinishedList().append(builder);
}

#endif // JS_ION

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
#ifdef JS_ION
    jit::JitCompartment *jitComp = compartment->jitCompartment();
    if (!jitComp)
        return;

    AutoLockWorkerThreadState lock;

    if (!WorkerThreadState().threads)
        return;

    /* Cancel any pending entries for which processing hasn't started. */
    GlobalWorkerThreadState::IonBuilderVector &worklist = WorkerThreadState().ionWorklist();
    for (size_t i = 0; i < worklist.length(); i++) {
        jit::IonBuilder *builder = worklist[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            FinishOffThreadIonCompile(builder);
            WorkerThreadState().remove(worklist, &i);
        }
    }

    /* Wait for in progress entries to finish up. */
    for (size_t i = 0; i < WorkerThreadState().threadCount; i++) {
        const WorkerThread &helper = WorkerThreadState().threads[i];
        while (helper.ionBuilder &&
               CompiledScriptMatches(compartment, script, helper.ionBuilder->script()))
        {
            helper.ionBuilder->cancel();
            WorkerThreadState().wait(GlobalWorkerThreadState::CONSUMER);
        }
    }

    /* Cancel code generation for any completed entries. */
    GlobalWorkerThreadState::IonBuilderVector &finished = WorkerThreadState().ionFinishedList();
    for (size_t i = 0; i < finished.length(); i++) {
        jit::IonBuilder *builder = finished[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            jit::FinishOffThreadBuilder(builder);
            WorkerThreadState().remove(finished, &i);
        }
    }
#endif // JS_ION
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
    alloc(JSRuntime::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE), scopeChain(initCx, scopeChain),
    exclusiveContextGlobal(initCx, exclusiveContextGlobal), optionsElement(initCx),
    optionsIntroductionScript(initCx), callback(callback), callbackData(callbackData),
    script(nullptr), errors(cx), overRecursed(false)
{
}

bool
ParseTask::init(JSContext *cx, const ReadOnlyCompileOptions &options)
{
    if (!this->options.copy(cx, options))
        return false;

    // Save those compilation options that the ScriptSourceObject can't
    // point at while it's in the compilation's temporary compartment.
    optionsElement = this->options.element();
    this->options.setElement(nullptr);
    optionsIntroductionScript = this->options.introductionScript();
    this->options.setIntroductionScript(nullptr);

    return true;
}

void
ParseTask::activate(JSRuntime *rt)
{
    rt->setUsedByExclusiveThread(exclusiveContextGlobal->zone());
    cx->enterCompartment(exclusiveContextGlobal->compartment());
}

void
ParseTask::finish()
{
    if (script) {
        // Initialize the ScriptSourceObject slots that we couldn't while the SSO
        // was in the temporary compartment.
        ScriptSourceObject &sso = script->sourceObject()->as<ScriptSourceObject>();
        sso.initElement(optionsElement);
        sso.initIntroductionScript(optionsIntroductionScript);
    }
}

ParseTask::~ParseTask()
{
    // ParseTask takes over ownership of its input exclusive context.
    js_delete(cx);

    for (size_t i = 0; i < errors.length(); i++)
        js_delete(errors[i]);
}

void
js::CancelOffThreadParses(JSRuntime *rt)
{
    AutoLockWorkerThreadState lock;

    if (!WorkerThreadState().threads)
        return;

    // Instead of forcibly canceling pending parse tasks, just wait for all scheduled
    // and in progress ones to complete. Otherwise the final GC may not collect
    // everything due to zones being used off thread.
    while (true) {
        bool pending = false;
        GlobalWorkerThreadState::ParseTaskVector &worklist = WorkerThreadState().parseWorklist();
        for (size_t i = 0; i < worklist.length(); i++) {
            ParseTask *task = worklist[i];
            if (task->runtimeMatches(rt))
                pending = true;
        }
        if (!pending) {
            bool inProgress = false;
            for (size_t i = 0; i < WorkerThreadState().threadCount; i++) {
                ParseTask *task = WorkerThreadState().threads[i].parseTask;
                if (task && task->runtimeMatches(rt))
                    inProgress = true;
            }
            if (!inProgress)
                break;
        }
        WorkerThreadState().wait(GlobalWorkerThreadState::CONSUMER);
    }

    // Clean up any parse tasks which haven't been finished by the main thread.
    GlobalWorkerThreadState::ParseTaskVector &finished = WorkerThreadState().parseFinishedList();
    while (true) {
        bool found = false;
        for (size_t i = 0; i < finished.length(); i++) {
            ParseTask *task = finished[i];
            if (task->runtimeMatches(rt)) {
                found = true;
                AutoUnlockWorkerThreadState unlock;
                WorkerThreadState().finishParseTask(/* maybecx = */ nullptr, rt, task);
            }
        }
        if (!found)
            break;
    }
}

bool
js::OffThreadParsingMustWaitForGC(JSRuntime *rt)
{
    // Off thread parsing can't occur during incremental collections on the
    // atoms compartment, to avoid triggering barriers. (Outside the atoms
    // compartment, the compilation will use a new zone that is never
    // collected.) If an atoms-zone GC is in progress, hold off on executing the
    // parse task until the atoms-zone GC completes (see
    // EnqueuePendingParseTasksAfterGC).
    return rt->activeGCInAtomsZone();
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
    compartmentOptions.setInvisibleToDebugger(true);
    compartmentOptions.setMergeable(true);

    JSObject *global = JS_NewGlobalObject(cx, &workerGlobalClass, nullptr,
                                          JS::FireOnNewGlobalHook, compartmentOptions);
    if (!global)
        return false;

    JS_SetCompartmentPrincipals(global->compartment(), cx->compartment()->principals);

    RootedObject obj(cx);

    // Initialize all classes needed for parsing while we are still on the main
    // thread. Do this for both the target and the new global so that prototype
    // pointers can be changed infallibly after parsing finishes.
    if (!GetBuiltinConstructor(cx, JSProto_Function, &obj) ||
        !GetBuiltinConstructor(cx, JSProto_Array, &obj) ||
        !GetBuiltinConstructor(cx, JSProto_RegExp, &obj) ||
        !GetBuiltinConstructor(cx, JSProto_Iterator, &obj))
    {
        return false;
    }
    {
        AutoCompartment ac(cx, global);
        if (!GetBuiltinConstructor(cx, JSProto_Function, &obj) ||
            !GetBuiltinConstructor(cx, JSProto_Array, &obj) ||
            !GetBuiltinConstructor(cx, JSProto_RegExp, &obj) ||
            !GetBuiltinConstructor(cx, JSProto_Iterator, &obj))
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

    if (OffThreadParsingMustWaitForGC(cx->runtime())) {
        AutoLockWorkerThreadState lock;
        if (!WorkerThreadState().parseWaitingOnGC().append(task.get()))
            return false;
    } else {
        task->activate(cx->runtime());

        AutoLockWorkerThreadState lock;

        if (!WorkerThreadState().parseWorklist().append(task.get()))
            return false;

        WorkerThreadState().notifyOne(GlobalWorkerThreadState::PRODUCER);
    }

    task.forget();

    return true;
}

void
js::EnqueuePendingParseTasksAfterGC(JSRuntime *rt)
{
    JS_ASSERT(!OffThreadParsingMustWaitForGC(rt));

    GlobalWorkerThreadState::ParseTaskVector newTasks;
    {
        AutoLockWorkerThreadState lock;
        GlobalWorkerThreadState::ParseTaskVector &waiting = WorkerThreadState().parseWaitingOnGC();

        for (size_t i = 0; i < waiting.length(); i++) {
            ParseTask *task = waiting[i];
            if (task->runtimeMatches(rt)) {
                newTasks.append(task);
                WorkerThreadState().remove(waiting, &i);
            }
        }
    }

    if (newTasks.empty())
        return;

    // This logic should mirror the contents of the !activeGCInAtomsZone()
    // branch in StartOffThreadParseScript:

    for (size_t i = 0; i < newTasks.length(); i++)
        newTasks[i]->activate(rt);

    AutoLockWorkerThreadState lock;

    for (size_t i = 0; i < newTasks.length(); i++)
        WorkerThreadState().parseWorklist().append(newTasks[i]);

    WorkerThreadState().notifyAll(GlobalWorkerThreadState::PRODUCER);
}

static const uint32_t WORKER_STACK_SIZE = 512 * 1024;
static const uint32_t WORKER_STACK_QUOTA = 450 * 1024;

bool
GlobalWorkerThreadState::ensureInitialized()
{
    JS_ASSERT(this == &WorkerThreadState());
    AutoLockWorkerThreadState lock;

    if (threads)
        return true;

    threads = js_pod_calloc<WorkerThread>(threadCount);
    if (!threads)
        return false;

    for (size_t i = 0; i < threadCount; i++) {
        WorkerThread &helper = threads[i];
        helper.threadData.construct(static_cast<JSRuntime *>(nullptr));
        helper.thread = PR_CreateThread(PR_USER_THREAD,
                                        WorkerThread::ThreadMain, &helper,
                                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, WORKER_STACK_SIZE);
        if (!helper.thread || !helper.threadData.ref().init()) {
            for (size_t j = 0; j < threadCount; j++)
                threads[j].destroy();
            js_free(threads);
            threads = nullptr;
            return false;
        }
    }

    resetAsmJSFailureState();
    return true;
}

GlobalWorkerThreadState::GlobalWorkerThreadState()
{
    mozilla::PodZero(this);

    cpuCount = GetCPUCount();
    threadCount = ThreadCountForCPUCount(cpuCount);

    MOZ_ASSERT(cpuCount > 0, "GetCPUCount() seems broken");

    workerLock = PR_NewLock();
    consumerWakeup = PR_NewCondVar(workerLock);
    producerWakeup = PR_NewCondVar(workerLock);
}

void
GlobalWorkerThreadState::finish()
{
    if (threads) {
        for (size_t i = 0; i < threadCount; i++)
            threads[i].destroy();
        js_free(threads);
    }

    PR_DestroyCondVar(consumerWakeup);
    PR_DestroyCondVar(producerWakeup);
    PR_DestroyLock(workerLock);
}

void
GlobalWorkerThreadState::lock()
{
    JS_ASSERT(!isLocked());
    AssertCurrentThreadCanLock(WorkerThreadStateLock);
    PR_Lock(workerLock);
#ifdef DEBUG
    lockOwner = PR_GetCurrentThread();
#endif
}

void
GlobalWorkerThreadState::unlock()
{
    JS_ASSERT(isLocked());
#ifdef DEBUG
    lockOwner = nullptr;
#endif
    PR_Unlock(workerLock);
}

#ifdef DEBUG
bool
GlobalWorkerThreadState::isLocked()
{
    return lockOwner == PR_GetCurrentThread();
}
#endif

void
GlobalWorkerThreadState::wait(CondVar which, uint32_t millis)
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
GlobalWorkerThreadState::notifyAll(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyAllCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

void
GlobalWorkerThreadState::notifyOne(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

bool
GlobalWorkerThreadState::canStartAsmJSCompile()
{
    // Don't execute an AsmJS job if an earlier one failed.
    JS_ASSERT(isLocked());
    return (!asmJSWorklist().empty() && !numAsmJSFailedJobs);
}

bool
GlobalWorkerThreadState::canStartIonCompile()
{
    // A worker thread can begin an Ion compilation if (a) there is some script
    // which is waiting to be compiled, and (b) no other worker thread is
    // currently compiling a script. The latter condition ensures that two
    // compilations cannot simultaneously occur.
    if (ionWorklist().empty())
        return false;
    for (size_t i = 0; i < threadCount; i++) {
        if (threads[i].ionBuilder)
            return false;
    }
    return true;
}

bool
GlobalWorkerThreadState::canStartParseTask()
{
    // Don't allow simultaneous off thread parses, to reduce contention on the
    // atoms table. Note that asm.js compilation depends on this to avoid
    // stalling the worker thread, as off thread parse tasks can trigger and
    // block on other off thread asm.js compilation tasks.
    JS_ASSERT(isLocked());
    if (parseWorklist().empty())
        return false;
    for (size_t i = 0; i < threadCount; i++) {
        if (threads[i].parseTask)
            return false;
    }
    return true;
}

bool
GlobalWorkerThreadState::canStartCompressionTask()
{
    return !compressionWorklist().empty();
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
    RootedFunction function(cx, script->functionNonDelazifying());
    CallNewScriptHook(cx, script, function);
}

JSScript *
GlobalWorkerThreadState::finishParseTask(JSContext *maybecx, JSRuntime *rt, void *token)
{
    ParseTask *parseTask = nullptr;

    // The token is a ParseTask* which should be in the finished list.
    // Find and remove its entry.
    {
        AutoLockWorkerThreadState lock;
        ParseTaskVector &finished = parseFinishedList();
        for (size_t i = 0; i < finished.length(); i++) {
            if (finished[i] == token) {
                parseTask = finished[i];
                remove(finished, &i);
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
        TaggedProto proto(object->proto());
        if (!proto.isObject())
            continue;

        JSProtoKey key = JS::IdentifyStandardPrototype(proto.toObject());
        if (key == JSProto_Null)
            continue;

        JSObject *newProto = GetBuiltinPrototypePure(&parseTask->scopeChain->global(), key);
        JS_ASSERT(newProto);

        object->setProtoUnchecked(newProto);
    }

    // Move the parsed script and all its contents into the desired compartment.
    gc::MergeCompartments(parseTask->cx->compartment(), parseTask->scopeChain->compartment());
    parseTask->finish();

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
            if (script->compileAndGo())
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
    if (thread) {
        {
            AutoLockWorkerThreadState lock;
            terminate = true;

            /* Notify all workers, to ensure that this thread wakes up. */
            WorkerThreadState().notifyAll(GlobalWorkerThreadState::PRODUCER);
        }

        PR_JoinThread(thread);
    }

    if (!threadData.empty())
        threadData.destroy();
}

/* static */
void
WorkerThread::ThreadMain(void *arg)
{
    PR_SetCurrentThreadName("Analysis Helper");
    static_cast<WorkerThread *>(arg)->threadLoop();
}

void
WorkerThread::handleAsmJSWorkload()
{
#ifdef JS_ION
    JS_ASSERT(WorkerThreadState().isLocked());
    JS_ASSERT(WorkerThreadState().canStartAsmJSCompile());
    JS_ASSERT(idle());

    asmData = WorkerThreadState().asmJSWorklist().popCopy();
    bool success = false;

    do {
        AutoUnlockWorkerThreadState unlock;
        PerThreadData::AutoEnterRuntime enter(threadData.addr(), asmData->runtime);

        jit::IonContext icx(asmData->mir->compartment->runtime(),
                            asmData->mir->compartment,
                            &asmData->mir->alloc());

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

    // On failure, signal parent for harvesting in CancelOutstandingJobs().
    if (!success) {
        WorkerThreadState().noteAsmJSFailure(asmData->func);
        WorkerThreadState().notifyAll(GlobalWorkerThreadState::CONSUMER);
        asmData = nullptr;
        return;
    }

    // On success, move work to the finished list.
    WorkerThreadState().asmJSFinishedList().append(asmData);
    asmData = nullptr;

    // Notify the main thread in case it's blocked waiting for a LifoAlloc.
    WorkerThreadState().notifyAll(GlobalWorkerThreadState::CONSUMER);
#else
    MOZ_CRASH();
#endif // JS_ION
}

void
WorkerThread::handleIonWorkload()
{
#ifdef JS_ION
    JS_ASSERT(WorkerThreadState().isLocked());
    JS_ASSERT(WorkerThreadState().canStartIonCompile());
    JS_ASSERT(idle());

    ionBuilder = WorkerThreadState().ionWorklist().popCopy();

#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::getLogger(TraceLogging::ION_BACKGROUND_COMPILER),
                        TraceLogging::ION_COMPILE_START,
                        TraceLogging::ION_COMPILE_STOP,
                        ionBuilder->script());
#endif

    JSRuntime *rt = ionBuilder->script()->compartment()->runtimeFromAnyThread();

    {
        AutoUnlockWorkerThreadState unlock;
        PerThreadData::AutoEnterRuntime enter(threadData.addr(),
                                              ionBuilder->script()->runtimeFromAnyThread());
        jit::IonContext ictx(jit::CompileRuntime::get(rt),
                             jit::CompileCompartment::get(ionBuilder->script()->compartment()),
                             &ionBuilder->alloc());
        ionBuilder->setBackgroundCodegen(jit::CompileBackEnd(ionBuilder));
    }

    FinishOffThreadIonCompile(ionBuilder);
    ionBuilder = nullptr;

    // Ping the main thread so that the compiled code can be incorporated
    // at the next interrupt callback. Don't interrupt Ion code for this, as
    // this incorporation can be delayed indefinitely without affecting
    // performance as long as the main thread is actually executing Ion code.
    rt->requestInterrupt(JSRuntime::RequestInterruptAnyThreadDontStopIon);

    // Notify the main thread in case it is waiting for the compilation to finish.
    WorkerThreadState().notifyAll(GlobalWorkerThreadState::CONSUMER);
#else
    MOZ_CRASH();
#endif // JS_ION
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
WorkerThread::handleParseWorkload()
{
    JS_ASSERT(WorkerThreadState().isLocked());
    JS_ASSERT(WorkerThreadState().canStartParseTask());
    JS_ASSERT(idle());

    parseTask = WorkerThreadState().parseWorklist().popCopy();
    parseTask->cx->setWorkerThread(this);

    {
        AutoUnlockWorkerThreadState unlock;
        PerThreadData::AutoEnterRuntime enter(threadData.addr(),
                                              parseTask->exclusiveContextGlobal->runtimeFromAnyThread());
        parseTask->script = frontend::CompileScript(parseTask->cx, &parseTask->alloc,
                                                    NullPtr(), NullPtr(),
                                                    parseTask->options,
                                                    parseTask->chars, parseTask->length);
    }

    // The callback is invoked while we are still off the main thread.
    parseTask->callback(parseTask, parseTask->callbackData);

    // FinishOffThreadScript will need to be called on the script to
    // migrate it into the correct compartment.
    WorkerThreadState().parseFinishedList().append(parseTask);

    parseTask = nullptr;

    // Notify the main thread in case it is waiting for the parse/emit to finish.
    WorkerThreadState().notifyAll(GlobalWorkerThreadState::CONSUMER);
}

void
WorkerThread::handleCompressionWorkload()
{
    JS_ASSERT(WorkerThreadState().isLocked());
    JS_ASSERT(WorkerThreadState().canStartCompressionTask());
    JS_ASSERT(idle());

    compressionTask = WorkerThreadState().compressionWorklist().popCopy();
    compressionTask->workerThread = this;

    {
        AutoUnlockWorkerThreadState unlock;
        if (!compressionTask->work())
            compressionTask->setOOM();
    }

    compressionTask->workerThread = nullptr;
    compressionTask = nullptr;

    // Notify the main thread in case it is waiting for the compression to finish.
    WorkerThreadState().notifyAll(GlobalWorkerThreadState::CONSUMER);
}

bool
js::StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task)
{
    if (!EnsureWorkerThreadsInitialized(cx))
        return false;

    AutoLockWorkerThreadState lock;

    if (!WorkerThreadState().compressionWorklist().append(task))
        return false;

    WorkerThreadState().notifyOne(GlobalWorkerThreadState::PRODUCER);
    return true;
}

bool
GlobalWorkerThreadState::compressionInProgress(SourceCompressionTask *task)
{
    JS_ASSERT(isLocked());
    for (size_t i = 0; i < compressionWorklist().length(); i++) {
        if (compressionWorklist()[i] == task)
            return true;
    }
    for (size_t i = 0; i < threadCount; i++) {
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
        AutoLockWorkerThreadState lock;

        while (WorkerThreadState().compressionInProgress(this))
            WorkerThreadState().wait(GlobalWorkerThreadState::CONSUMER);

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
GlobalWorkerThreadState::compressionTaskForSource(ScriptSource *ss)
{
    JS_ASSERT(isLocked());
    for (size_t i = 0; i < compressionWorklist().length(); i++) {
        SourceCompressionTask *task = compressionWorklist()[i];
        if (task->source() == ss)
            return task;
    }
    for (size_t i = 0; i < threadCount; i++) {
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

    AutoLockWorkerThreadState lock;

    // Look for a token that hasn't finished compressing and whose source is
    // the given ScriptSource.
    if (SourceCompressionTask *task = WorkerThreadState().compressionTaskForSource(this))
        return task->uncompressedChars();

    // Compressing has finished, so this ScriptSource is ready. Avoid future
    // queries on the worker thread state when getting the chars.
    ready_ = true;

    return nullptr;
}

void
WorkerThread::threadLoop()
{
    JS::AutoAssertNoGC nogc;
    AutoLockWorkerThreadState lock;

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
            if (WorkerThreadState().canStartIonCompile() ||
                WorkerThreadState().canStartAsmJSCompile() ||
                WorkerThreadState().canStartParseTask() ||
                WorkerThreadState().canStartCompressionTask())
            {
                break;
            }
            WorkerThreadState().wait(GlobalWorkerThreadState::PRODUCER);
        }

        // Dispatch tasks, prioritizing AsmJS work.
        if (WorkerThreadState().canStartAsmJSCompile())
            handleAsmJSWorkload();
        else if (WorkerThreadState().canStartIonCompile())
            handleIonWorkload();
        else if (WorkerThreadState().canStartParseTask())
            handleParseWorkload();
        else if (WorkerThreadState().canStartCompressionTask())
            handleCompressionWorkload();
        else
            MOZ_ASSUME_UNREACHABLE("No task to perform");
    }
}

#else /* JS_THREADSAFE */

using namespace js;

#ifdef JS_ION

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

#endif // JS_ION

void
js::CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script)
{
}

void
js::CancelOffThreadParses(JSRuntime *rt)
{
}

bool
js::StartOffThreadParseScript(JSContext *cx, const ReadOnlyCompileOptions &options,
                              const jschar *chars, size_t length, HandleObject scopeChain,
                              JS::OffThreadCompileCallback callback, void *callbackData)
{
    MOZ_ASSUME_UNREACHABLE("Off thread compilation not available in non-THREADSAFE builds");
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

#endif /* JS_THREADSAFE */
