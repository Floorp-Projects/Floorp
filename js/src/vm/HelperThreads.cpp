/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/HelperThreads.h"

#ifdef JS_THREADSAFE

#include "mozilla/DebugOnly.h"

#include "jsnativestack.h"
#include "prmjtime.h"

#include "frontend/BytecodeCompiler.h"
#include "jit/IonBuilder.h"
#include "vm/Debugger.h"
#include "vm/TraceLogging.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::DebugOnly;

namespace js {

GlobalHelperThreadState gHelperThreadState;

} // namespace js

void
js::EnsureHelperThreadsInitialized(ExclusiveContext *cx)
{
    // If 'cx' is not a JSContext, we are already off the main thread and the
    // helper threads would have already been initialized.
    if (!cx->isJSContext())
        return;

    HelperThreadState().ensureInitialized();
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
    JS_ASSERT(!HelperThreadState().threads);

    HelperThreadState().cpuCount = count;
    HelperThreadState().threadCount = ThreadCountForCPUCount(count);
}

#ifdef JS_ION

bool
js::StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData)
{
    // Threads already initialized by the AsmJS compiler.
    JS_ASSERT(asmData->mir);
    JS_ASSERT(asmData->lir == nullptr);

    AutoLockHelperThreadState lock;

    // Don't append this task if another failed.
    if (HelperThreadState().asmJSFailed())
        return false;

    if (!HelperThreadState().asmJSWorklist().append(asmData))
        return false;

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER);
    return true;
}

bool
js::StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder)
{
    EnsureHelperThreadsInitialized(cx);

    AutoLockHelperThreadState lock;

    if (!HelperThreadState().ionWorklist().append(builder))
        return false;

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER);
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
    HelperThreadState().ionFinishedList().append(builder);
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

    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads)
        return;

    /* Cancel any pending entries for which processing hasn't started. */
    GlobalHelperThreadState::IonBuilderVector &worklist = HelperThreadState().ionWorklist();
    for (size_t i = 0; i < worklist.length(); i++) {
        jit::IonBuilder *builder = worklist[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            FinishOffThreadIonCompile(builder);
            HelperThreadState().remove(worklist, &i);
        }
    }

    /* Wait for in progress entries to finish up. */
    for (size_t i = 0; i < HelperThreadState().threadCount; i++) {
        const HelperThread &helper = HelperThreadState().threads[i];
        while (helper.ionBuilder &&
               CompiledScriptMatches(compartment, script, helper.ionBuilder->script()))
        {
            helper.ionBuilder->cancel();
            HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
        }
    }

    /* Cancel code generation for any completed entries. */
    GlobalHelperThreadState::IonBuilderVector &finished = HelperThreadState().ionFinishedList();
    for (size_t i = 0; i < finished.length(); i++) {
        jit::IonBuilder *builder = finished[i];
        if (CompiledScriptMatches(compartment, script, builder->script())) {
            jit::FinishOffThreadBuilder(builder);
            HelperThreadState().remove(finished, &i);
        }
    }
#endif // JS_ION
}

static const JSClass parseTaskGlobalClass = {
    "internal-parse-task-global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_DeletePropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

ParseTask::ParseTask(ExclusiveContext *cx, JSObject *exclusiveContextGlobal, JSContext *initCx,
                     const jschar *chars, size_t length,
                     JS::OffThreadCompileCallback callback, void *callbackData)
  : cx(cx), options(initCx), chars(chars), length(length),
    alloc(JSRuntime::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
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
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads)
        return;

    // Instead of forcibly canceling pending parse tasks, just wait for all scheduled
    // and in progress ones to complete. Otherwise the final GC may not collect
    // everything due to zones being used off thread.
    while (true) {
        bool pending = false;
        GlobalHelperThreadState::ParseTaskVector &worklist = HelperThreadState().parseWorklist();
        for (size_t i = 0; i < worklist.length(); i++) {
            ParseTask *task = worklist[i];
            if (task->runtimeMatches(rt))
                pending = true;
        }
        if (!pending) {
            bool inProgress = false;
            for (size_t i = 0; i < HelperThreadState().threadCount; i++) {
                ParseTask *task = HelperThreadState().threads[i].parseTask;
                if (task && task->runtimeMatches(rt))
                    inProgress = true;
            }
            if (!inProgress)
                break;
        }
        HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
    }

    // Clean up any parse tasks which haven't been finished by the main thread.
    GlobalHelperThreadState::ParseTaskVector &finished = HelperThreadState().parseFinishedList();
    while (true) {
        bool found = false;
        for (size_t i = 0; i < finished.length(); i++) {
            ParseTask *task = finished[i];
            if (task->runtimeMatches(rt)) {
                found = true;
                AutoUnlockHelperThreadState unlock;
                HelperThreadState().finishParseTask(/* maybecx = */ nullptr, rt, task);
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
                              const jschar *chars, size_t length,
                              JS::OffThreadCompileCallback callback, void *callbackData)
{
    // Suppress GC so that calls below do not trigger a new incremental GC
    // which could require barriers on the atoms compartment.
    gc::AutoSuppressGC suppress(cx);

    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
    frontend::MaybeCallSourceHandler(cx, options, srcBuf);

    EnsureHelperThreadsInitialized(cx);

    JS::CompartmentOptions compartmentOptions(cx->compartment()->options());
    compartmentOptions.setZone(JS::FreshZone);
    compartmentOptions.setInvisibleToDebugger(true);
    compartmentOptions.setMergeable(true);

    // Don't falsely inherit the host's global trace hook.
    compartmentOptions.setTrace(nullptr);

    JSObject *global = JS_NewGlobalObject(cx, &parseTaskGlobalClass, nullptr,
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

    ScopedJSDeletePtr<ExclusiveContext> helpercx(
        cx->new_<ExclusiveContext>(cx->runtime(), (PerThreadData *) nullptr,
                                   ThreadSafeContext::Context_Exclusive));
    if (!helpercx)
        return false;

    ScopedJSDeletePtr<ParseTask> task(
        cx->new_<ParseTask>(helpercx.get(), global, cx, chars, length,
                            callback, callbackData));
    if (!task)
        return false;

    helpercx.forget();

    if (!task->init(cx, options))
        return false;

    if (OffThreadParsingMustWaitForGC(cx->runtime())) {
        AutoLockHelperThreadState lock;
        if (!HelperThreadState().parseWaitingOnGC().append(task.get()))
            return false;
    } else {
        task->activate(cx->runtime());

        AutoLockHelperThreadState lock;

        if (!HelperThreadState().parseWorklist().append(task.get()))
            return false;

        HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER);
    }

    task.forget();

    return true;
}

void
js::EnqueuePendingParseTasksAfterGC(JSRuntime *rt)
{
    JS_ASSERT(!OffThreadParsingMustWaitForGC(rt));

    GlobalHelperThreadState::ParseTaskVector newTasks;
    {
        AutoLockHelperThreadState lock;
        GlobalHelperThreadState::ParseTaskVector &waiting = HelperThreadState().parseWaitingOnGC();

        for (size_t i = 0; i < waiting.length(); i++) {
            ParseTask *task = waiting[i];
            if (task->runtimeMatches(rt)) {
                newTasks.append(task);
                HelperThreadState().remove(waiting, &i);
            }
        }
    }

    if (newTasks.empty())
        return;

    // This logic should mirror the contents of the !activeGCInAtomsZone()
    // branch in StartOffThreadParseScript:

    for (size_t i = 0; i < newTasks.length(); i++)
        newTasks[i]->activate(rt);

    AutoLockHelperThreadState lock;

    for (size_t i = 0; i < newTasks.length(); i++)
        HelperThreadState().parseWorklist().append(newTasks[i]);

    HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER);
}

static const uint32_t HELPER_STACK_SIZE = 512 * 1024;
static const uint32_t HELPER_STACK_QUOTA = 450 * 1024;

void
GlobalHelperThreadState::ensureInitialized()
{
    JS_ASSERT(this == &HelperThreadState());
    AutoLockHelperThreadState lock;

    if (threads)
        return;

    threads = js_pod_calloc<HelperThread>(threadCount);
    if (!threads)
        CrashAtUnhandlableOOM("GlobalHelperThreadState::ensureInitialized");

    for (size_t i = 0; i < threadCount; i++) {
        HelperThread &helper = threads[i];
        helper.threadData.construct(static_cast<JSRuntime *>(nullptr));
        helper.thread = PR_CreateThread(PR_USER_THREAD,
                                        HelperThread::ThreadMain, &helper,
                                        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, HELPER_STACK_SIZE);
        if (!helper.thread || !helper.threadData.ref().init())
            CrashAtUnhandlableOOM("GlobalHelperThreadState::ensureInitialized");
    }

    resetAsmJSFailureState();
}

GlobalHelperThreadState::GlobalHelperThreadState()
{
    mozilla::PodZero(this);

    cpuCount = GetCPUCount();
    threadCount = ThreadCountForCPUCount(cpuCount);

    MOZ_ASSERT(cpuCount > 0, "GetCPUCount() seems broken");

    helperLock = PR_NewLock();
    consumerWakeup = PR_NewCondVar(helperLock);
    producerWakeup = PR_NewCondVar(helperLock);
}

void
GlobalHelperThreadState::finish()
{
    if (threads) {
        for (size_t i = 0; i < threadCount; i++)
            threads[i].destroy();
        js_free(threads);
    }

    PR_DestroyCondVar(consumerWakeup);
    PR_DestroyCondVar(producerWakeup);
    PR_DestroyLock(helperLock);
}

void
GlobalHelperThreadState::lock()
{
    JS_ASSERT(!isLocked());
    AssertCurrentThreadCanLock(HelperThreadStateLock);
    PR_Lock(helperLock);
#ifdef DEBUG
    lockOwner = PR_GetCurrentThread();
#endif
}

void
GlobalHelperThreadState::unlock()
{
    JS_ASSERT(isLocked());
#ifdef DEBUG
    lockOwner = nullptr;
#endif
    PR_Unlock(helperLock);
}

#ifdef DEBUG
bool
GlobalHelperThreadState::isLocked()
{
    return lockOwner == PR_GetCurrentThread();
}
#endif

void
GlobalHelperThreadState::wait(CondVar which, uint32_t millis)
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
GlobalHelperThreadState::notifyAll(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyAllCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

void
GlobalHelperThreadState::notifyOne(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyCondVar((which == CONSUMER) ? consumerWakeup : producerWakeup);
}

bool
GlobalHelperThreadState::canStartAsmJSCompile()
{
    // Don't execute an AsmJS job if an earlier one failed.
    JS_ASSERT(isLocked());
    return !asmJSWorklist().empty() && !numAsmJSFailedJobs;
}

bool
GlobalHelperThreadState::canStartIonCompile()
{
    // A helper thread can begin an Ion compilation if (a) there is some script
    // which is waiting to be compiled, and (b) no other helper thread is
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
GlobalHelperThreadState::canStartParseTask()
{
    // Don't allow simultaneous off thread parses, to reduce contention on the
    // atoms table. Note that asm.js compilation depends on this to avoid
    // stalling the helper thread, as off thread parse tasks can trigger and
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
GlobalHelperThreadState::canStartCompressionTask()
{
    return !compressionWorklist().empty();
}

bool
GlobalHelperThreadState::canStartGCHelperTask()
{
    return !gcHelperWorklist().empty();
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
GlobalHelperThreadState::finishParseTask(JSContext *maybecx, JSRuntime *rt, void *token)
{
    ScopedJSDeletePtr<ParseTask> parseTask;

    // The token is a ParseTask* which should be in the finished list.
    // Find and remove its entry.
    {
        AutoLockHelperThreadState lock;
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
    parseTask->cx->leaveCompartment(parseTask->cx->compartment());
    rt->clearUsedByExclusiveThread(parseTask->cx->zone());
    if (!maybecx) {
        return nullptr;
    }
    JSContext *cx = maybecx;
    JS_ASSERT(cx->compartment());

    // Make sure we have all the constructors we need for the prototype
    // remapping below, since we can't GC while that's happening.
    Rooted<GlobalObject*> global(cx, &cx->global()->as<GlobalObject>());
    if (!GlobalObject::ensureConstructor(cx, global, JSProto_Object) ||
        !GlobalObject::ensureConstructor(cx, global, JSProto_Array) ||
        !GlobalObject::ensureConstructor(cx, global, JSProto_Function) ||
        !GlobalObject::ensureConstructor(cx, global, JSProto_RegExp) ||
        !GlobalObject::ensureConstructor(cx, global, JSProto_Iterator))
    {
        return nullptr;
    }

    // Point the prototypes of any objects in the script's compartment to refer
    // to the corresponding prototype in the new compartment. This will briefly
    // create cross compartment pointers, which will be fixed by the
    // MergeCompartments call below.
    for (gc::ZoneCellIter iter(parseTask->cx->zone(), gc::FINALIZE_TYPE_OBJECT);
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
        JS_ASSERT(key == JSProto_Object || key == JSProto_Array ||
                  key == JSProto_Function || key == JSProto_RegExp ||
                  key == JSProto_Iterator);

        JSObject *newProto = GetBuiltinPrototypePure(global, key);
        JS_ASSERT(newProto);

        object->setProtoUnchecked(TaggedProto(newProto));
    }

    // Move the parsed script and all its contents into the desired compartment.
    gc::MergeCompartments(parseTask->cx->compartment(), cx->compartment());
    parseTask->finish();

    RootedScript script(rt, parseTask->script);
    assertSameCompartment(cx, script);

    // Report any error or warnings generated during the parse, and inform the
    // debugger about the compiled scripts.
    for (size_t i = 0; i < parseTask->errors.length(); i++)
        parseTask->errors[i]->throwError(cx);
    if (parseTask->overRecursed)
        js_ReportOverRecursed(cx);

    if (script) {
        // The Debugger only needs to be told about the topmost script that was compiled.
        GlobalObject *compileAndGoGlobal = nullptr;
        if (script->compileAndGo())
            compileAndGoGlobal = &script->global();
        Debugger::onNewScript(cx, script, compileAndGoGlobal);

        // The NewScript hook needs to be called for all compiled scripts.
        CallNewScriptHookForAllScripts(cx, script);

        // Update the compressed source table with the result. This is normally
        // called by setCompressedSource when compilation occurs on the main thread.
        if (script->scriptSource()->hasCompressedSource())
            script->scriptSource()->updateCompressedSourceSet(rt);
    }

    return script;
}

void
HelperThread::destroy()
{
    if (thread) {
        {
            AutoLockHelperThreadState lock;
            terminate = true;

            /* Notify all helpers, to ensure that this thread wakes up. */
            HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER);
        }

        PR_JoinThread(thread);
    }

    if (!threadData.empty())
        threadData.destroy();
}

#ifdef MOZ_NUWA_PROCESS
extern "C" {
MFBT_API bool IsNuwaProcess();
MFBT_API void NuwaMarkCurrentThread(void (*recreate)(void *), void *arg);
}
#endif

/* static */
void
HelperThread::ThreadMain(void *arg)
{
    PR_SetCurrentThreadName("Analysis Helper");

#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
        JS_ASSERT(NuwaMarkCurrentThread != nullptr);
        NuwaMarkCurrentThread(nullptr, nullptr);
    }
#endif

    static_cast<HelperThread *>(arg)->threadLoop();
}

void
HelperThread::handleAsmJSWorkload()
{
#ifdef JS_ION
    JS_ASSERT(HelperThreadState().isLocked());
    JS_ASSERT(HelperThreadState().canStartAsmJSCompile());
    JS_ASSERT(idle());

    asmData = HelperThreadState().asmJSWorklist().popCopy();
    bool success = false;

    do {
        AutoUnlockHelperThreadState unlock;
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
        HelperThreadState().noteAsmJSFailure(asmData->func);
        HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER);
        asmData = nullptr;
        return;
    }

    // On success, move work to the finished list.
    HelperThreadState().asmJSFinishedList().append(asmData);
    asmData = nullptr;

    // Notify the main thread in case it's blocked waiting for a LifoAlloc.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER);
#else
    MOZ_CRASH();
#endif // JS_ION
}

void
HelperThread::handleIonWorkload()
{
#ifdef JS_ION
    JS_ASSERT(HelperThreadState().isLocked());
    JS_ASSERT(HelperThreadState().canStartIonCompile());
    JS_ASSERT(idle());

    // Find the ionBuilder with the script having the highest usecount.
    GlobalHelperThreadState::IonBuilderVector &ionWorklist = HelperThreadState().ionWorklist();
    size_t highest = 0;
    for (size_t i = 1; i < ionWorklist.length(); i++) {
        if (ionWorklist[i]->script()->getUseCount() >
            ionWorklist[highest]->script()->getUseCount())
        {
            highest = i;
        }
    }
    ionBuilder = ionWorklist[highest];

    // Pop the top IonBuilder and move it to the original place of the
    // IonBuilder we took to start compiling. If both are the same, only pop.
    if (highest != ionWorklist.length() - 1)
        ionWorklist[highest] = ionWorklist.popCopy();
    else
        ionWorklist.popBack();

    TraceLogger *logger = TraceLoggerForCurrentThread();
    AutoTraceLog logScript(logger, TraceLogCreateTextId(logger, ionBuilder->script()));
    AutoTraceLog logCompile(logger, TraceLogger::IonCompilation);

    JSRuntime *rt = ionBuilder->script()->compartment()->runtimeFromAnyThread();

    {
        AutoUnlockHelperThreadState unlock;
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
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER);
#else
    MOZ_CRASH();
#endif // JS_ION
}

void
ExclusiveContext::setHelperThread(HelperThread *thread)
{
    helperThread_ = thread;
    perThreadData = thread->threadData.addr();
}

frontend::CompileError &
ExclusiveContext::addPendingCompileError()
{
    frontend::CompileError *error = js_new<frontend::CompileError>();
    if (!error)
        MOZ_CRASH();
    if (!helperThread()->parseTask->errors.append(error))
        MOZ_CRASH();
    return *error;
}

void
ExclusiveContext::addPendingOverRecursed()
{
    if (helperThread()->parseTask)
        helperThread()->parseTask->overRecursed = true;
}

void
HelperThread::handleParseWorkload()
{
    JS_ASSERT(HelperThreadState().isLocked());
    JS_ASSERT(HelperThreadState().canStartParseTask());
    JS_ASSERT(idle());

    parseTask = HelperThreadState().parseWorklist().popCopy();
    parseTask->cx->setHelperThread(this);

    {
        AutoUnlockHelperThreadState unlock;
        PerThreadData::AutoEnterRuntime enter(threadData.addr(),
                                              parseTask->exclusiveContextGlobal->runtimeFromAnyThread());
        SourceBufferHolder srcBuf(parseTask->chars, parseTask->length,
                                  SourceBufferHolder::NoOwnership);
        parseTask->script = frontend::CompileScript(parseTask->cx, &parseTask->alloc,
                                                    NullPtr(), NullPtr(),
                                                    parseTask->options,
                                                    srcBuf);
    }

    // The callback is invoked while we are still off the main thread.
    parseTask->callback(parseTask, parseTask->callbackData);

    // FinishOffThreadScript will need to be called on the script to
    // migrate it into the correct compartment.
    HelperThreadState().parseFinishedList().append(parseTask);

    parseTask = nullptr;

    // Notify the main thread in case it is waiting for the parse/emit to finish.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER);
}

void
HelperThread::handleCompressionWorkload()
{
    JS_ASSERT(HelperThreadState().isLocked());
    JS_ASSERT(HelperThreadState().canStartCompressionTask());
    JS_ASSERT(idle());

    compressionTask = HelperThreadState().compressionWorklist().popCopy();
    compressionTask->helperThread = this;

    {
        AutoUnlockHelperThreadState unlock;
        compressionTask->result = compressionTask->work();
    }

    compressionTask->helperThread = nullptr;
    compressionTask = nullptr;

    // Notify the main thread in case it is waiting for the compression to finish.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER);
}

bool
js::StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task)
{
    EnsureHelperThreadsInitialized(cx);

    AutoLockHelperThreadState lock;

    if (!HelperThreadState().compressionWorklist().append(task)) {
        if (JSContext *maybecx = cx->maybeJSContext())
            js_ReportOutOfMemory(maybecx);
        return false;
    }

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER);
    return true;
}

bool
GlobalHelperThreadState::compressionInProgress(SourceCompressionTask *task)
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
    if (!active()) {
        JS_ASSERT(!compressed);
        return true;
    }

    {
        AutoLockHelperThreadState lock;
        while (HelperThreadState().compressionInProgress(this))
            HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
    }

    if (result == Success) {
        ss->setCompressedSource(cx->isJSContext() ? cx->asJSContext()->runtime() : nullptr,
                                compressed, compressedBytes, compressedHash);

        // Update memory accounting.
        cx->updateMallocCounter(ss->computedSizeOfData());
    } else {
        js_free(compressed);

        if (result == OOM)
            js_ReportOutOfMemory(cx);
        else if (result == Aborted && !ss->ensureOwnsSource(cx))
            result = OOM;
    }

    ss = nullptr;
    compressed = nullptr;
    JS_ASSERT(!active());

    return result != OOM;
}

SourceCompressionTask *
GlobalHelperThreadState::compressionTaskForSource(ScriptSource *ss)
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

void
HelperThread::handleGCHelperWorkload()
{
    JS_ASSERT(HelperThreadState().isLocked());
    JS_ASSERT(HelperThreadState().canStartGCHelperTask());
    JS_ASSERT(idle());

    JS_ASSERT(!gcHelperState);
    gcHelperState = HelperThreadState().gcHelperWorklist().popCopy();

    {
        AutoUnlockHelperThreadState unlock;
        gcHelperState->work();
    }

    gcHelperState = nullptr;
}

void
HelperThread::threadLoop()
{
    JS::AutoSuppressGCAnalysis nogc;
    AutoLockHelperThreadState lock;

    js::TlsPerThreadData.set(threadData.addr());

    // Compute the thread's stack limit, for over-recursed checks.
    uintptr_t stackLimit = GetNativeStackBase();
#if JS_STACK_GROWTH_DIRECTION > 0
    stackLimit += HELPER_STACK_QUOTA;
#else
    stackLimit -= HELPER_STACK_QUOTA;
#endif
    for (size_t i = 0; i < ArrayLength(threadData.ref().nativeStackLimit); i++)
        threadData.ref().nativeStackLimit[i] = stackLimit;

    while (true) {
        JS_ASSERT(!ionBuilder && !asmData);

        // Block until a task is available.
        while (true) {
            if (terminate)
                return;
            if (HelperThreadState().canStartIonCompile() ||
                HelperThreadState().canStartAsmJSCompile() ||
                HelperThreadState().canStartParseTask() ||
                HelperThreadState().canStartCompressionTask() ||
                HelperThreadState().canStartGCHelperTask())
            {
                break;
            }
            HelperThreadState().wait(GlobalHelperThreadState::PRODUCER);
        }

        // Dispatch tasks, prioritizing AsmJS work.
        if (HelperThreadState().canStartAsmJSCompile())
            handleAsmJSWorkload();
        else if (HelperThreadState().canStartIonCompile())
            handleIonWorkload();
        else if (HelperThreadState().canStartParseTask())
            handleParseWorkload();
        else if (HelperThreadState().canStartCompressionTask())
            handleCompressionWorkload();
        else if (HelperThreadState().canStartGCHelperTask())
            handleGCHelperWorkload();
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
                              const jschar *chars, size_t length,
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
    JS_ASSERT(!ss);
    return true;
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
