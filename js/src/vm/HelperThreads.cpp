/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/HelperThreads.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Unused.h"

#include "jsnativestack.h"

#include "builtin/Promise.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "jit/IonBuilder.h"
#include "threading/CpuCount.h"
#include "vm/Debugger.h"
#include "vm/ErrorReporting.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/Xdr.h"

#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::DebugOnly;
using mozilla::Unused;
using mozilla::TimeDuration;

namespace js {

GlobalHelperThreadState* gHelperThreadState = nullptr;

} // namespace js

bool
js::CreateHelperThreadsState()
{
    MOZ_ASSERT(!gHelperThreadState);
    gHelperThreadState = js_new<GlobalHelperThreadState>();
    return gHelperThreadState != nullptr;
}

void
js::DestroyHelperThreadsState()
{
    MOZ_ASSERT(gHelperThreadState);
    gHelperThreadState->finish();
    js_delete(gHelperThreadState);
    gHelperThreadState = nullptr;
}

bool
js::EnsureHelperThreadsInitialized()
{
    MOZ_ASSERT(gHelperThreadState);
    return gHelperThreadState->ensureInitialized();
}

static size_t
ThreadCountForCPUCount(size_t cpuCount)
{
    // Create additional threads on top of the number of cores available, to
    // provide some excess capacity in case threads pause each other.
    static const uint32_t EXCESS_THREADS = 4;
    return cpuCount + EXCESS_THREADS;
}

void
js::SetFakeCPUCount(size_t count)
{
    // This must be called before the threads have been initialized.
    MOZ_ASSERT(!HelperThreadState().threads);

    HelperThreadState().cpuCount = count;
    HelperThreadState().threadCount = ThreadCountForCPUCount(count);
}

bool
js::StartOffThreadWasmCompile(wasm::CompileTask* task)
{
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().wasmWorklist(lock).append(task))
        return false;

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
    return true;
}

bool
js::StartOffThreadIonCompile(JSContext* cx, jit::IonBuilder* builder)
{
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().ionWorklist(lock).append(builder))
        return false;

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
    return true;
}

/*
 * Move an IonBuilder for which compilation has either finished, failed, or
 * been cancelled into the global finished compilation list. All off thread
 * compilations which are started must eventually be finished.
 */
static void
FinishOffThreadIonCompile(jit::IonBuilder* builder, const AutoLockHelperThreadState& lock)
{
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!HelperThreadState().ionFinishedList(lock).append(builder))
        oomUnsafe.crash("FinishOffThreadIonCompile");
}

static JSRuntime*
GetSelectorRuntime(const CompilationSelector& selector)
{
    struct Matcher
    {
        JSRuntime* match(JSScript* script)    { return script->runtimeFromActiveCooperatingThread(); }
        JSRuntime* match(JSCompartment* comp) { return comp->runtimeFromActiveCooperatingThread(); }
        JSRuntime* match(ZonesInState zbs)    { return zbs.runtime; }
        JSRuntime* match(JSRuntime* runtime)  { return runtime; }
        JSRuntime* match(AllCompilations all) { return nullptr; }
    };

    return selector.match(Matcher());
}

static bool
JitDataStructuresExist(const CompilationSelector& selector)
{
    struct Matcher
    {
        bool match(JSScript* script)    { return !!script->compartment()->jitCompartment(); }
        bool match(JSCompartment* comp) { return !!comp->jitCompartment(); }
        bool match(ZonesInState zbs)    { return zbs.runtime->hasJitRuntime(); }
        bool match(JSRuntime* runtime)  { return runtime->hasJitRuntime(); }
        bool match(AllCompilations all) { return true; }
    };

    return selector.match(Matcher());
}

static bool
CompiledScriptMatches(const CompilationSelector& selector, JSScript* target)
{
    struct ScriptMatches
    {
        JSScript* target_;

        bool match(JSScript* script)    { return script == target_; }
        bool match(JSCompartment* comp) { return comp == target_->compartment(); }
        bool match(JSRuntime* runtime)  { return runtime == target_->runtimeFromAnyThread(); }
        bool match(AllCompilations all) { return true; }
        bool match(ZonesInState zbs)    {
            return zbs.runtime == target_->runtimeFromAnyThread() &&
                   zbs.state == target_->zoneFromAnyThread()->gcState();
        }
    };

    return selector.match(ScriptMatches{target});
}

void
js::CancelOffThreadIonCompile(const CompilationSelector& selector, bool discardLazyLinkList)
{
    if (!JitDataStructuresExist(selector))
        return;

    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads)
        return;

    /* Cancel any pending entries for which processing hasn't started. */
    GlobalHelperThreadState::IonBuilderVector& worklist = HelperThreadState().ionWorklist(lock);
    for (size_t i = 0; i < worklist.length(); i++) {
        jit::IonBuilder* builder = worklist[i];
        if (CompiledScriptMatches(selector, builder->script())) {
            FinishOffThreadIonCompile(builder, lock);
            HelperThreadState().remove(worklist, &i);
        }
    }

    /* Wait for in progress entries to finish up. */
    bool cancelled;
    do {
        cancelled = false;
        bool unpaused = false;
        for (auto& helper : *HelperThreadState().threads) {
            if (helper.ionBuilder() &&
                CompiledScriptMatches(selector, helper.ionBuilder()->script()))
            {
                helper.ionBuilder()->cancel();
                if (helper.pause) {
                    helper.pause = false;
                    unpaused = true;
                }
                cancelled = true;
            }
        }
        if (unpaused)
            HelperThreadState().notifyAll(GlobalHelperThreadState::PAUSE, lock);
        if (cancelled)
            HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
    } while (cancelled);

    /* Cancel code generation for any completed entries. */
    GlobalHelperThreadState::IonBuilderVector& finished = HelperThreadState().ionFinishedList(lock);
    for (size_t i = 0; i < finished.length(); i++) {
        jit::IonBuilder* builder = finished[i];
        if (CompiledScriptMatches(selector, builder->script())) {
            jit::FinishOffThreadBuilder(nullptr, builder, lock);
            HelperThreadState().remove(finished, &i);
        }
    }

    /* Cancel lazy linking for pending builders (attached to the ionScript). */
    if (discardLazyLinkList) {
        MOZ_ASSERT(!selector.is<AllCompilations>());
        JSRuntime* runtime = GetSelectorRuntime(selector);
        for (ZoneGroupsIter group(runtime); !group.done(); group.next()) {
            jit::IonBuilder* builder = group->ionLazyLinkList().getFirst();
            while (builder) {
                jit::IonBuilder* next = builder->getNext();
                if (CompiledScriptMatches(selector, builder->script()))
                    jit::FinishOffThreadBuilder(runtime, builder, lock);
                builder = next;
            }
        }
    }
}

#ifdef DEBUG
bool
js::HasOffThreadIonCompile(JSCompartment* comp)
{
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads || comp->isAtomsCompartment())
        return false;

    GlobalHelperThreadState::IonBuilderVector& worklist = HelperThreadState().ionWorklist(lock);
    for (size_t i = 0; i < worklist.length(); i++) {
        jit::IonBuilder* builder = worklist[i];
        if (builder->script()->compartment() == comp)
            return true;
    }

    for (auto& helper : *HelperThreadState().threads) {
        if (helper.ionBuilder() && helper.ionBuilder()->script()->compartment() == comp)
            return true;
    }

    GlobalHelperThreadState::IonBuilderVector& finished = HelperThreadState().ionFinishedList(lock);
    for (size_t i = 0; i < finished.length(); i++) {
        jit::IonBuilder* builder = finished[i];
        if (builder->script()->compartment() == comp)
            return true;
    }

    jit::IonBuilder* builder = comp->zone()->group()->ionLazyLinkList().getFirst();
    while (builder) {
        if (builder->script()->compartment() == comp)
            return true;
        builder = builder->getNext();
    }

    return false;
}
#endif

static const JSClassOps parseTaskGlobalClassOps = {
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

static const JSClass parseTaskGlobalClass = {
    "internal-parse-task-global", JSCLASS_GLOBAL_FLAGS,
    &parseTaskGlobalClassOps
};

ParseTask::ParseTask(ParseTaskKind kind, JSContext* cx, JSObject* parseGlobal,
                     const char16_t* chars, size_t length,
                     JS::OffThreadCompileCallback callback, void* callbackData)
  : kind(kind), options(cx), chars(chars), length(length),
    alloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    parseGlobal(parseGlobal),
    callback(callback), callbackData(callbackData),
    script(nullptr), sourceObject(nullptr),
    overRecursed(false), outOfMemory(false)
{
}

ParseTask::ParseTask(ParseTaskKind kind, JSContext* cx, JSObject* parseGlobal,
                     JS::TranscodeBuffer& buffer, size_t cursor,
                     JS::OffThreadCompileCallback callback, void* callbackData)
  : kind(kind), options(cx), buffer(&buffer), cursor(cursor),
    alloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    parseGlobal(parseGlobal),
    callback(callback), callbackData(callbackData),
    script(nullptr), sourceObject(nullptr),
    overRecursed(false), outOfMemory(false)
{
}

bool
ParseTask::init(JSContext* cx, const ReadOnlyCompileOptions& options)
{
    if (!this->options.copy(cx, options))
        return false;

    return true;
}

void
ParseTask::activate(JSRuntime* rt)
{
    rt->setUsedByHelperThread(parseGlobal->zone());
}

bool
ParseTask::finish(JSContext* cx)
{
    if (sourceObject) {
        RootedScriptSource sso(cx, sourceObject);
        if (!ScriptSourceObject::initFromOptions(cx, sso, options))
            return false;
        if (!sso->source()->tryCompressOffThread(cx))
            return false;
    }

    return true;
}

ParseTask::~ParseTask()
{
    for (size_t i = 0; i < errors.length(); i++)
        js_delete(errors[i]);
}

void
ParseTask::trace(JSTracer* trc)
{
    if (parseGlobal->runtimeFromAnyThread() != trc->runtime())
        return;
    Zone* zone = MaybeForwarded(parseGlobal)->zoneFromAnyThread();
    if (zone->usedByHelperThread()) {
        MOZ_ASSERT(!zone->isCollecting());
        return;
    }

    TraceManuallyBarrieredEdge(trc, &parseGlobal, "ParseTask::parseGlobal");
    if (script)
        TraceManuallyBarrieredEdge(trc, &script, "ParseTask::script");
    if (sourceObject)
        TraceManuallyBarrieredEdge(trc, &sourceObject, "ParseTask::sourceObject");
}

ScriptParseTask::ScriptParseTask(JSContext* cx, JSObject* parseGlobal,
                                 const char16_t* chars, size_t length,
                                 JS::OffThreadCompileCallback callback, void* callbackData)
  : ParseTask(ParseTaskKind::Script, cx, parseGlobal, chars, length, callback,
              callbackData)
{
}

void
ScriptParseTask::parse(JSContext* cx)
{
    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
    script = frontend::CompileGlobalScript(cx, alloc, ScopeKind::Global,
                                           options, srcBuf,
                                           /* sourceObjectOut = */ &sourceObject);
}

ModuleParseTask::ModuleParseTask(JSContext* cx, JSObject* parseGlobal,
                                 const char16_t* chars, size_t length,
                                 JS::OffThreadCompileCallback callback, void* callbackData)
  : ParseTask(ParseTaskKind::Module, cx, parseGlobal, chars, length, callback,
              callbackData)
{
}

void
ModuleParseTask::parse(JSContext* cx)
{
    SourceBufferHolder srcBuf(chars, length, SourceBufferHolder::NoOwnership);
    ModuleObject* module = frontend::CompileModule(cx, options, srcBuf, alloc, &sourceObject);
    if (module)
        script = module->script();
}

ScriptDecodeTask::ScriptDecodeTask(JSContext* cx, JSObject* parseGlobal,
                                   JS::TranscodeBuffer& buffer, size_t cursor,
                                   JS::OffThreadCompileCallback callback, void* callbackData)
  : ParseTask(ParseTaskKind::ScriptDecode, cx, parseGlobal,
              buffer, cursor, callback, callbackData)
{
}

void
ScriptDecodeTask::parse(JSContext* cx)
{
    RootedScript resultScript(cx);
    XDROffThreadDecoder decoder(cx, alloc, &options, /* sourceObjectOut = */ &sourceObject,
                                *buffer, cursor);
    decoder.codeScript(&resultScript);
    MOZ_ASSERT(bool(resultScript) == (decoder.resultCode() == JS::TranscodeResult_Ok));
    if (decoder.resultCode() == JS::TranscodeResult_Ok) {
        script = resultScript.get();
    } else {
        sourceObject = nullptr;
    }
}

void
js::CancelOffThreadParses(JSRuntime* rt)
{
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads)
        return;

#ifdef DEBUG
    GlobalHelperThreadState::ParseTaskVector& waitingOnGC =
        HelperThreadState().parseWaitingOnGC(lock);
    for (size_t i = 0; i < waitingOnGC.length(); i++)
        MOZ_ASSERT(!waitingOnGC[i]->runtimeMatches(rt));
#endif

    // Instead of forcibly canceling pending parse tasks, just wait for all scheduled
    // and in progress ones to complete. Otherwise the final GC may not collect
    // everything due to zones being used off thread.
    while (true) {
        bool pending = false;
        GlobalHelperThreadState::ParseTaskVector& worklist = HelperThreadState().parseWorklist(lock);
        for (size_t i = 0; i < worklist.length(); i++) {
            ParseTask* task = worklist[i];
            if (task->runtimeMatches(rt))
                pending = true;
        }
        if (!pending) {
            bool inProgress = false;
            for (auto& thread : *HelperThreadState().threads) {
                ParseTask* task = thread.parseTask();
                if (task && task->runtimeMatches(rt))
                    inProgress = true;
            }
            if (!inProgress)
                break;
        }
        HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
    }

    // Clean up any parse tasks which haven't been finished by the active thread.
    GlobalHelperThreadState::ParseTaskVector& finished = HelperThreadState().parseFinishedList(lock);
    while (true) {
        bool found = false;
        for (size_t i = 0; i < finished.length(); i++) {
            ParseTask* task = finished[i];
            if (task->runtimeMatches(rt)) {
                found = true;
                AutoUnlockHelperThreadState unlock(lock);
                HelperThreadState().cancelParseTask(rt, task->kind, task);
            }
        }
        if (!found)
            break;
    }
}

bool
js::OffThreadParsingMustWaitForGC(JSRuntime* rt)
{
    // Off thread parsing can't occur during incremental collections on the
    // atoms compartment, to avoid triggering barriers. (Outside the atoms
    // compartment, the compilation will use a new zone that is never
    // collected.) If an atoms-zone GC is in progress, hold off on executing the
    // parse task until the atoms-zone GC completes (see
    // EnqueuePendingParseTasksAfterGC).
    return rt->activeGCInAtomsZone();
}

static bool
EnsureConstructor(JSContext* cx, Handle<GlobalObject*> global, JSProtoKey key)
{
    if (!GlobalObject::ensureConstructor(cx, global, key))
        return false;

    MOZ_ASSERT(global->getPrototype(key).toObject().isDelegate(),
               "standard class prototype wasn't a delegate from birth");
    return true;
}

// Initialize all classes potentially created during parsing for use in parser
// data structures, template objects, &c.
static bool
EnsureParserCreatedClasses(JSContext* cx, ParseTaskKind kind)
{
    Handle<GlobalObject*> global = cx->global();

    if (!EnsureConstructor(cx, global, JSProto_Function))
        return false; // needed by functions, also adds object literals' proto

    if (!EnsureConstructor(cx, global, JSProto_Array))
        return false; // needed by array literals

    if (!EnsureConstructor(cx, global, JSProto_RegExp))
        return false; // needed by regular expression literals

    if (!EnsureConstructor(cx, global, JSProto_Iterator))
        return false; // needed by ???

    if (!GlobalObject::initStarGenerators(cx, global))
        return false; // needed by function*() {} and generator comprehensions

    if (kind == ParseTaskKind::Module && !GlobalObject::ensureModulePrototypesCreated(cx, global))
        return false;

    return true;
}

static JSObject*
CreateGlobalForOffThreadParse(JSContext* cx, ParseTaskKind kind, const gc::AutoSuppressGC& nogc)
{
    JSCompartment* currentCompartment = cx->compartment();

    JS::CompartmentOptions compartmentOptions(currentCompartment->creationOptions(),
                                              currentCompartment->behaviors());

    auto& creationOptions = compartmentOptions.creationOptions();

    creationOptions.setInvisibleToDebugger(true)
                   .setMergeable(true)
                   .setNewZoneInNewZoneGroup();

    // Don't falsely inherit the host's global trace hook.
    creationOptions.setTrace(nullptr);

    JSObject* global = JS_NewGlobalObject(cx, &parseTaskGlobalClass, nullptr,
                                          JS::FireOnNewGlobalHook, compartmentOptions);
    if (!global)
        return nullptr;

    JS_SetCompartmentPrincipals(global->compartment(), currentCompartment->principals());

    // Initialize all classes required for parsing while still on the active
    // thread, for both the target and the new global so that prototype
    // pointers can be changed infallibly after parsing finishes.
    if (!EnsureParserCreatedClasses(cx, kind))
        return nullptr;
    {
        AutoCompartment ac(cx, global);
        if (!EnsureParserCreatedClasses(cx, kind))
            return nullptr;
    }

    return global;
}

static bool
QueueOffThreadParseTask(JSContext* cx, ParseTask* task)
{
    if (OffThreadParsingMustWaitForGC(cx->runtime())) {
        AutoLockHelperThreadState lock;
        if (!HelperThreadState().parseWaitingOnGC(lock).append(task)) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else {
        AutoLockHelperThreadState lock;
        if (!HelperThreadState().parseWorklist(lock).append(task)) {
            ReportOutOfMemory(cx);
            return false;
        }

        task->activate(cx->runtime());
        HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
    }

    return true;
}

template <typename TaskFunctor>
bool
StartOffThreadParseTask(JSContext* cx, const ReadOnlyCompileOptions& options,
                        ParseTaskKind kind, TaskFunctor& taskFunctor)
{
    // Suppress GC so that calls below do not trigger a new incremental GC
    // which could require barriers on the atoms compartment.
    gc::AutoSuppressGC nogc(cx);
    gc::AutoAssertNoNurseryAlloc noNurseryAlloc;
    AutoSuppressAllocationMetadataBuilder suppressMetadata(cx);

    JSObject* global = CreateGlobalForOffThreadParse(cx, kind, nogc);
    if (!global)
        return false;

    ScopedJSDeletePtr<ParseTask> task(taskFunctor(global));
    if (!task)
        return false;

    if (!task->init(cx, options) || !QueueOffThreadParseTask(cx, task))
        return false;

    task.forget();

    return true;
}


bool
js::StartOffThreadParseScript(JSContext* cx, const ReadOnlyCompileOptions& options,
                              const char16_t* chars, size_t length,
                              JS::OffThreadCompileCallback callback, void* callbackData)
{
    auto functor = [&](JSObject* global) -> ScriptParseTask* {
        return cx->new_<ScriptParseTask>(cx, global, chars, length,
                                         callback, callbackData);
    };
    return StartOffThreadParseTask(cx, options, ParseTaskKind::Script, functor);
}

bool
js::StartOffThreadParseModule(JSContext* cx, const ReadOnlyCompileOptions& options,
                              const char16_t* chars, size_t length,
                              JS::OffThreadCompileCallback callback, void* callbackData)
{
    auto functor = [&](JSObject* global) -> ModuleParseTask* {
        return cx->new_<ModuleParseTask>(cx, global, chars, length,
                                         callback, callbackData);
    };
    return StartOffThreadParseTask(cx, options, ParseTaskKind::Module, functor);
}

bool
js::StartOffThreadDecodeScript(JSContext* cx, const ReadOnlyCompileOptions& options,
                               JS::TranscodeBuffer& buffer, size_t cursor,
                               JS::OffThreadCompileCallback callback, void* callbackData)
{
    auto functor = [&](JSObject* global) -> ScriptDecodeTask* {
        return cx->new_<ScriptDecodeTask>(cx, global, buffer, cursor,
                                          callback, callbackData);
    };
    return StartOffThreadParseTask(cx, options, ParseTaskKind::ScriptDecode, functor);
}

void
js::EnqueuePendingParseTasksAfterGC(JSRuntime* rt)
{
    MOZ_ASSERT(!OffThreadParsingMustWaitForGC(rt));

    GlobalHelperThreadState::ParseTaskVector newTasks;
    {
        AutoLockHelperThreadState lock;
        GlobalHelperThreadState::ParseTaskVector& waiting =
            HelperThreadState().parseWaitingOnGC(lock);

        for (size_t i = 0; i < waiting.length(); i++) {
            ParseTask* task = waiting[i];
            if (task->runtimeMatches(rt) && !task->parseGlobal->zone()->wasGCStarted()) {
                AutoEnterOOMUnsafeRegion oomUnsafe;
                if (!newTasks.append(task))
                    oomUnsafe.crash("EnqueuePendingParseTasksAfterGC");
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

    {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!HelperThreadState().parseWorklist(lock).appendAll(newTasks))
            oomUnsafe.crash("EnqueuePendingParseTasksAfterGC");
    }

    HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER, lock);
}

static const uint32_t kDefaultHelperStackSize = 2048 * 1024;
static const uint32_t kDefaultHelperStackQuota = 1800 * 1024;

// TSan enforces a minimum stack size that's just slightly larger than our
// default helper stack size.  It does this to store blobs of TSan-specific
// data on each thread's stack.  Unfortunately, that means that even though
// we'll actually receive a larger stack than we requested, the effective
// usable space of that stack is significantly less than what we expect.
// To offset TSan stealing our stack space from underneath us, double the
// default.
//
// Note that we don't need this for ASan/MOZ_ASAN because ASan doesn't
// require all the thread-specific state that TSan does.
#if defined(MOZ_TSAN)
static const uint32_t HELPER_STACK_SIZE = 2 * kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = 2 * kDefaultHelperStackQuota;
#else
static const uint32_t HELPER_STACK_SIZE = kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = kDefaultHelperStackQuota;
#endif

bool
GlobalHelperThreadState::ensureInitialized()
{
    MOZ_ASSERT(CanUseExtraThreads());

    MOZ_ASSERT(this == &HelperThreadState());
    AutoLockHelperThreadState lock;

    if (threads)
        return true;

    threads = js::MakeUnique<HelperThreadVector>();
    if (!threads || !threads->initCapacity(threadCount))
        return false;

    for (size_t i = 0; i < threadCount; i++) {
        threads->infallibleEmplaceBack();
        HelperThread& helper = (*threads)[i];

        helper.thread = mozilla::Some(Thread(Thread::Options().setStackSize(HELPER_STACK_SIZE)));
        if (!helper.thread->init(HelperThread::ThreadMain, &helper))
            goto error;

        continue;

    error:
        // Ensure that we do not leave uninitialized threads in the `threads`
        // vector.
        threads->popBack();
        finishThreads();
        return false;
    }

    return true;
}

GlobalHelperThreadState::GlobalHelperThreadState()
 : cpuCount(0),
   threadCount(0),
   threads(nullptr),
   wasmCompilationInProgress(false),
   numWasmFailedJobs(0),
   helperLock(mutexid::GlobalHelperThreadState)
{
    cpuCount = GetCPUCount();
    threadCount = ThreadCountForCPUCount(cpuCount);

    MOZ_ASSERT(cpuCount > 0, "GetCPUCount() seems broken");
}

void
GlobalHelperThreadState::finish()
{
    finishThreads();
}

void
GlobalHelperThreadState::finishThreads()
{
    if (!threads)
        return;

    MOZ_ASSERT(CanUseExtraThreads());
    for (auto& thread : *threads)
        thread.destroy();
    threads.reset(nullptr);
}

void
GlobalHelperThreadState::lock()
{
    helperLock.lock();
}

void
GlobalHelperThreadState::unlock()
{
    helperLock.unlock();
}

#ifdef DEBUG
bool
GlobalHelperThreadState::isLockedByCurrentThread()
{
    return helperLock.ownedByCurrentThread();
}
#endif // DEBUG

void
GlobalHelperThreadState::wait(AutoLockHelperThreadState& locked, CondVar which,
                              TimeDuration timeout /* = TimeDuration::Forever() */)
{
    whichWakeup(which).wait_for(locked, timeout);
}

void
GlobalHelperThreadState::notifyAll(CondVar which, const AutoLockHelperThreadState&)
{
    whichWakeup(which).notify_all();
}

void
GlobalHelperThreadState::notifyOne(CondVar which, const AutoLockHelperThreadState&)
{
    whichWakeup(which).notify_one();
}

bool
GlobalHelperThreadState::hasActiveThreads(const AutoLockHelperThreadState&)
{
    if (!threads)
        return false;

    for (auto& thread : *threads) {
        if (!thread.idle())
            return true;
    }

    return false;
}

void
GlobalHelperThreadState::waitForAllThreads()
{
    CancelOffThreadIonCompile();

    AutoLockHelperThreadState lock;
    while (hasActiveThreads(lock))
        wait(lock, CONSUMER);
}

template <typename T>
bool
GlobalHelperThreadState::checkTaskThreadLimit(size_t maxThreads) const
{
    if (maxThreads >= threadCount)
        return true;

    size_t count = 0;
    for (auto& thread : *threads) {
        if (thread.currentTask.isSome() && thread.currentTask->is<T>())
            count++;
        if (count >= maxThreads)
            return false;
    }

    return true;
}

struct MOZ_RAII AutoSetContextRuntime
{
    explicit AutoSetContextRuntime(JSRuntime* rt) {
        TlsContext.get()->setRuntime(rt);
    }
    ~AutoSetContextRuntime() {
        TlsContext.get()->setRuntime(nullptr);
    }
};

static inline bool
IsHelperThreadSimulatingOOM(js::oom::ThreadType threadType)
{
#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
    return js::oom::targetThread == threadType;
#else
    return false;
#endif
}

size_t
GlobalHelperThreadState::maxIonCompilationThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_ION))
        return 1;
    return threadCount;
}

size_t
GlobalHelperThreadState::maxUnpausedIonCompilationThreads() const
{
    return 1;
}

size_t
GlobalHelperThreadState::maxWasmCompilationThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_WASM))
        return 1;
    return cpuCount;
}

size_t
GlobalHelperThreadState::maxParseThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_PARSE))
        return 1;

    // Don't allow simultaneous off thread parses, to reduce contention on the
    // atoms table. Note that wasm compilation depends on this to avoid
    // stalling the helper thread, as off thread parse tasks can trigger and
    // block on other off thread wasm compilation tasks.
    return 1;
}

size_t
GlobalHelperThreadState::maxCompressionThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_COMPRESS))
        return 1;

    // Compression is triggered on major GCs to compress ScriptSources. It is
    // considered low priority work.
    return 1;
}

size_t
GlobalHelperThreadState::maxGCHelperThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_GCHELPER))
        return 1;
    return threadCount;
}

size_t
GlobalHelperThreadState::maxGCParallelThreads() const
{
    if (IsHelperThreadSimulatingOOM(js::oom::THREAD_TYPE_GCPARALLEL))
        return 1;
    return threadCount;
}

bool
GlobalHelperThreadState::canStartWasmCompile(const AutoLockHelperThreadState& lock)
{
    // Don't execute an wasm job if an earlier one failed.
    if (wasmWorklist(lock).empty() || numWasmFailedJobs)
        return false;

    // Honor the maximum allowed threads to compile wasm jobs at once,
    // to avoid oversaturating the machine.
    if (!checkTaskThreadLimit<wasm::CompileTask*>(maxWasmCompilationThreads()))
        return false;

    return true;
}

bool
GlobalHelperThreadState::canStartPromiseTask(const AutoLockHelperThreadState& lock)
{
    return !promiseTasks(lock).empty();
}

static bool
IonBuilderHasHigherPriority(jit::IonBuilder* first, jit::IonBuilder* second)
{
    // This method can return whatever it wants, though it really ought to be a
    // total order. The ordering is allowed to race (change on the fly), however.

    // A lower optimization level indicates a higher priority.
    if (first->optimizationInfo().level() != second->optimizationInfo().level())
        return first->optimizationInfo().level() < second->optimizationInfo().level();

    // A script without an IonScript has precedence on one with.
    if (first->scriptHasIonScript() != second->scriptHasIonScript())
        return !first->scriptHasIonScript();

    // A higher warm-up counter indicates a higher priority.
    return first->script()->getWarmUpCount() / first->script()->length() >
           second->script()->getWarmUpCount() / second->script()->length();
}

bool
GlobalHelperThreadState::canStartIonCompile(const AutoLockHelperThreadState& lock)
{
    return !ionWorklist(lock).empty() &&
           checkTaskThreadLimit<jit::IonBuilder*>(maxIonCompilationThreads());
}

jit::IonBuilder*
GlobalHelperThreadState::highestPriorityPendingIonCompile(const AutoLockHelperThreadState& lock,
                                                          bool remove /* = false */)
{
    auto& worklist = ionWorklist(lock);
    if (worklist.empty()) {
        MOZ_ASSERT(!remove);
        return nullptr;
    }

    // Get the highest priority IonBuilder which has not started compilation yet.
    size_t index = 0;
    for (size_t i = 1; i < worklist.length(); i++) {
        if (IonBuilderHasHigherPriority(worklist[i], worklist[index]))
            index = i;
    }
    jit::IonBuilder* builder = worklist[index];
    if (remove)
        worklist.erase(&worklist[index]);
    return builder;
}

HelperThread*
GlobalHelperThreadState::lowestPriorityUnpausedIonCompileAtThreshold(
    const AutoLockHelperThreadState& lock)
{
    // Get the lowest priority IonBuilder which has started compilation and
    // isn't paused, unless there are still fewer than the maximum number of
    // such builders permitted.
    size_t numBuilderThreads = 0;
    HelperThread* thread = nullptr;
    for (auto& thisThread : *threads) {
        if (thisThread.ionBuilder() && !thisThread.pause) {
            numBuilderThreads++;
            if (!thread ||
                IonBuilderHasHigherPriority(thread->ionBuilder(), thisThread.ionBuilder()))
            {
                thread = &thisThread;
            }
        }
    }
    if (numBuilderThreads < maxUnpausedIonCompilationThreads())
        return nullptr;
    return thread;
}

HelperThread*
GlobalHelperThreadState::highestPriorityPausedIonCompile(const AutoLockHelperThreadState& lock)
{
    // Get the highest priority IonBuilder which has started compilation but
    // which was subsequently paused.
    HelperThread* thread = nullptr;
    for (auto& thisThread : *threads) {
        if (thisThread.pause) {
            // Currently, only threads with IonBuilders can be paused.
            MOZ_ASSERT(thisThread.ionBuilder());
            if (!thread ||
                IonBuilderHasHigherPriority(thisThread.ionBuilder(), thread->ionBuilder()))
            {
                thread = &thisThread;
            }
        }
    }
    return thread;
}

bool
GlobalHelperThreadState::pendingIonCompileHasSufficientPriority(
    const AutoLockHelperThreadState& lock)
{
    // Can't compile anything if there are no scripts to compile.
    if (!canStartIonCompile(lock))
        return false;

    // Count the number of threads currently compiling scripts, and look for
    // the thread with the lowest priority.
    HelperThread* lowestPriorityThread = lowestPriorityUnpausedIonCompileAtThreshold(lock);

    // If the number of threads building scripts is less than the maximum, the
    // compilation can start immediately.
    if (!lowestPriorityThread)
        return true;

    // If there is a builder in the worklist with higher priority than some
    // builder currently being compiled, then that current compilation can be
    // paused, so allow the compilation.
    if (IonBuilderHasHigherPriority(highestPriorityPendingIonCompile(lock),
                                    lowestPriorityThread->ionBuilder()))
        return true;

    // Compilation will have to wait until one of the active compilations finishes.
    return false;
}

bool
GlobalHelperThreadState::canStartParseTask(const AutoLockHelperThreadState& lock)
{
    return !parseWorklist(lock).empty() && checkTaskThreadLimit<ParseTask*>(maxParseThreads());
}

bool
GlobalHelperThreadState::canStartCompressionTask(const AutoLockHelperThreadState& lock)
{
    return !compressionWorklist(lock).empty() &&
           checkTaskThreadLimit<SourceCompressionTask*>(maxCompressionThreads());
}

void
GlobalHelperThreadState::startHandlingCompressionTasks(const AutoLockHelperThreadState& lock)
{
    scheduleCompressionTasks(lock);
    if (canStartCompressionTask(lock))
        notifyOne(PRODUCER, lock);
}

void
GlobalHelperThreadState::scheduleCompressionTasks(const AutoLockHelperThreadState& lock)
{
    auto& pending = compressionPendingList(lock);
    auto& worklist = compressionWorklist(lock);

    for (size_t i = 0; i < pending.length(); i++) {
        if (pending[i]->shouldStart()) {
            // OOMing during appending results in the task not being scheduled
            // and deleted.
            Unused << worklist.append(Move(pending[i]));
            remove(pending, &i);
        }
    }
}

bool
GlobalHelperThreadState::canStartGCHelperTask(const AutoLockHelperThreadState& lock)
{
    return !gcHelperWorklist(lock).empty() &&
           checkTaskThreadLimit<GCHelperState*>(maxGCHelperThreads());
}

bool
GlobalHelperThreadState::canStartGCParallelTask(const AutoLockHelperThreadState& lock)
{
    return !gcParallelWorklist(lock).empty() &&
           checkTaskThreadLimit<GCParallelTask*>(maxGCParallelThreads());
}

js::GCParallelTask::~GCParallelTask()
{
    // Only most-derived classes' destructors may do the join: base class
    // destructors run after those for derived classes' members, so a join in a
    // base class can't ensure that the task is done using the members. All we
    // can do now is check that someone has previously stopped the task.
#ifdef DEBUG
    AutoLockHelperThreadState helperLock;
    MOZ_ASSERT(state == NotStarted);
#endif
}

bool
js::GCParallelTask::startWithLockHeld(AutoLockHelperThreadState& lock)
{
    // Tasks cannot be started twice.
    MOZ_ASSERT(state == NotStarted);

    // If we do the shutdown GC before running anything, we may never
    // have initialized the helper threads. Just use the serial path
    // since we cannot safely intialize them at this point.
    if (!HelperThreadState().threads)
        return false;

    if (!HelperThreadState().gcParallelWorklist(lock).append(this))
        return false;
    state = Dispatched;

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);

    return true;
}

bool
js::GCParallelTask::start()
{
    AutoLockHelperThreadState helperLock;
    return startWithLockHeld(helperLock);
}

void
js::GCParallelTask::joinWithLockHeld(AutoLockHelperThreadState& locked)
{
    if (state == NotStarted)
        return;

    while (state != Finished)
        HelperThreadState().wait(locked, GlobalHelperThreadState::CONSUMER);
    state = NotStarted;
    cancel_ = false;
}

void
js::GCParallelTask::join()
{
    AutoLockHelperThreadState helperLock;
    joinWithLockHeld(helperLock);
}

void
js::GCParallelTask::runFromActiveCooperatingThread(JSRuntime* rt)
{
    MOZ_ASSERT(state == NotStarted);
    MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(rt));
    mozilla::TimeStamp timeStart = mozilla::TimeStamp::Now();
    run();
    duration_ = mozilla::TimeStamp::Now() - timeStart;
}

void
js::GCParallelTask::runFromHelperThread(AutoLockHelperThreadState& locked)
{
    AutoSetContextRuntime ascr(runtime());
    gc::AutoSetThreadIsPerformingGC performingGC;

    {
        AutoUnlockHelperThreadState parallelSection(locked);
        mozilla::TimeStamp timeStart = mozilla::TimeStamp::Now();
        TlsContext.get()->heapState = JS::HeapState::MajorCollecting;
        run();
        TlsContext.get()->heapState = JS::HeapState::Idle;
        duration_ = mozilla::TimeStamp::Now() - timeStart;
    }

    state = Finished;
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

bool
js::GCParallelTask::isRunningWithLockHeld(const AutoLockHelperThreadState& locked) const
{
    return state == Dispatched;
}

bool
js::GCParallelTask::isRunning() const
{
    AutoLockHelperThreadState helperLock;
    return isRunningWithLockHeld(helperLock);
}

void
HelperThread::handleGCParallelWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartGCParallelTask(locked));
    MOZ_ASSERT(idle());

    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logCompile(logger, TraceLogger_GC);

    currentTask.emplace(HelperThreadState().gcParallelWorklist(locked).popCopy());
    gcParallelTask()->runFromHelperThread(locked);
    currentTask.reset();
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

static void
LeaveParseTaskZone(JSRuntime* rt, ParseTask* task)
{
    // Mark the zone as no longer in use by a helper thread, and available
    // to be collected by the GC.
    rt->clearUsedByHelperThread(task->parseGlobal->zone());
}

ParseTask*
GlobalHelperThreadState::removeFinishedParseTask(ParseTaskKind kind, void* token)
{
    // The token is a ParseTask* which should be in the finished list.
    // Find and remove its entry.

    AutoLockHelperThreadState lock;
    ParseTaskVector& finished = parseFinishedList(lock);

    for (size_t i = 0; i < finished.length(); i++) {
        if (finished[i] == token) {
            ParseTask* parseTask = finished[i];
            remove(finished, &i);
            MOZ_ASSERT(parseTask);
            MOZ_ASSERT(parseTask->kind == kind);
            return parseTask;
        }
    }

    MOZ_CRASH("Invalid ParseTask token");
}

JSScript*
GlobalHelperThreadState::finishParseTask(JSContext* cx, ParseTaskKind kind, void* token)
{
    MOZ_ASSERT(cx->compartment());

    ScopedJSDeletePtr<ParseTask> parseTask(removeFinishedParseTask(kind, token));

    // Make sure we have all the constructors we need for the prototype
    // remapping below, since we can't GC while that's happening.
    Rooted<GlobalObject*> global(cx, &cx->global()->as<GlobalObject>());
    if (!EnsureParserCreatedClasses(cx, kind)) {
        LeaveParseTaskZone(cx->runtime(), parseTask);
        return nullptr;
    }

    mergeParseTaskCompartment(cx, parseTask, global, cx->compartment());

    RootedScript script(cx, parseTask->script);
    releaseAssertSameCompartment(cx, script);

    if (!parseTask->finish(cx))
        return nullptr;

    // Report out of memory errors eagerly, or errors could be malformed.
    if (parseTask->outOfMemory) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    // Report any error or warnings generated during the parse, and inform the
    // debugger about the compiled scripts.
    for (size_t i = 0; i < parseTask->errors.length(); i++)
        parseTask->errors[i]->throwError(cx);
    if (parseTask->overRecursed)
        ReportOverRecursed(cx);
    if (cx->isExceptionPending())
        return nullptr;

    if (!script) {
        // No error was reported, but no script produced. Assume we hit out of
        // memory.
        ReportOutOfMemory(cx);
        return nullptr;
    }

    // The Debugger only needs to be told about the topmost script that was compiled.
    Debugger::onNewScript(cx, script);

    return script;
}

JSScript*
GlobalHelperThreadState::finishScriptParseTask(JSContext* cx, void* token)
{
    JSScript* script = finishParseTask(cx, ParseTaskKind::Script, token);
    MOZ_ASSERT_IF(script, script->isGlobalCode());
    return script;
}

JSScript*
GlobalHelperThreadState::finishScriptDecodeTask(JSContext* cx, void* token)
{
    JSScript* script = finishParseTask(cx, ParseTaskKind::ScriptDecode, token);
    MOZ_ASSERT_IF(script, script->isGlobalCode());
    return script;
}

JSObject*
GlobalHelperThreadState::finishModuleParseTask(JSContext* cx, void* token)
{
    JSScript* script = finishParseTask(cx, ParseTaskKind::Module, token);
    if (!script)
        return nullptr;

    MOZ_ASSERT(script->module());

    RootedModuleObject module(cx, script->module());
    module->fixEnvironmentsAfterCompartmentMerge();
    if (!ModuleObject::Freeze(cx, module))
        return nullptr;

    return module;
}

void
GlobalHelperThreadState::cancelParseTask(JSRuntime* rt, ParseTaskKind kind, void* token)
{
    ScopedJSDeletePtr<ParseTask> parseTask(removeFinishedParseTask(kind, token));
    LeaveParseTaskZone(rt, parseTask);
}

JSObject*
GlobalObject::getStarGeneratorFunctionPrototype()
{
    const Value& v = getReservedSlot(STAR_GENERATOR_FUNCTION_PROTO);
    return v.isObject() ? &v.toObject() : nullptr;
}

void
GlobalHelperThreadState::mergeParseTaskCompartment(JSContext* cx, ParseTask* parseTask,
                                                   Handle<GlobalObject*> global,
                                                   JSCompartment* dest)
{
    // After we call LeaveParseTaskZone() it's not safe to GC until we have
    // finished merging the contents of the parse task's compartment into the
    // destination compartment.  Finish any ongoing incremental GC first and
    // assert that no allocation can occur.
    gc::FinishGC(cx);
    JS::AutoAssertNoGC nogc(cx);

    LeaveParseTaskZone(cx->runtime(), parseTask);
    AutoCompartment ac(cx, parseTask->parseGlobal);

    {
        // Generator functions don't have Function.prototype as prototype but a
        // different function object, so the IdentifyStandardPrototype trick
        // below won't work.  Just special-case it.
        GlobalObject* parseGlobal = &parseTask->parseGlobal->as<GlobalObject>();
        JSObject* parseTaskStarGenFunctionProto = parseGlobal->getStarGeneratorFunctionPrototype();

        // Module objects don't have standard prototypes either.
        JSObject* moduleProto = parseGlobal->maybeGetModulePrototype();
        JSObject* importEntryProto = parseGlobal->maybeGetImportEntryPrototype();
        JSObject* exportEntryProto = parseGlobal->maybeGetExportEntryPrototype();

        // Point the prototypes of any objects in the script's compartment to refer
        // to the corresponding prototype in the new compartment. This will briefly
        // create cross compartment pointers, which will be fixed by the
        // MergeCompartments call below.
        Zone* parseZone = parseTask->parseGlobal->zone();
        for (auto group = parseZone->cellIter<ObjectGroup>(); !group.done(); group.next()) {
            TaggedProto proto(group->proto());
            if (!proto.isObject())
                continue;

            JSObject* protoObj = proto.toObject();

            JSObject* newProto;
            JSProtoKey key = JS::IdentifyStandardPrototype(protoObj);
            if (key != JSProto_Null) {
                MOZ_ASSERT(key == JSProto_Object || key == JSProto_Array ||
                           key == JSProto_Function || key == JSProto_RegExp ||
                           key == JSProto_Iterator);
                newProto = GetBuiltinPrototypePure(global, key);
            } else if (protoObj == parseTaskStarGenFunctionProto) {
                newProto = global->getStarGeneratorFunctionPrototype();
            } else if (protoObj == moduleProto) {
                newProto = global->getModulePrototype();
            } else if (protoObj == importEntryProto) {
                newProto = global->getImportEntryPrototype();
            } else if (protoObj == exportEntryProto) {
                newProto = global->getExportEntryPrototype();
            } else {
                continue;
            }

            group->setProtoUnchecked(TaggedProto(newProto));
        }
    }

    // Move the parsed script and all its contents into the desired compartment.
    gc::MergeCompartments(parseTask->parseGlobal->compartment(), dest);
}

void
HelperThread::destroy()
{
    if (thread.isSome()) {
        {
            AutoLockHelperThreadState lock;
            terminate = true;

            /* Notify all helpers, to ensure that this thread wakes up. */
            HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER, lock);
        }

        thread->join();
        thread.reset();
    }
}

/* static */
void
HelperThread::ThreadMain(void* arg)
{
    ThisThread::SetName("JS Helper");

    static_cast<HelperThread*>(arg)->threadLoop();
    Mutex::ShutDown();
}

void
HelperThread::handleWasmWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartWasmCompile(locked));
    MOZ_ASSERT(idle());

    currentTask.emplace(HelperThreadState().wasmWorklist(locked).popCopy());
    bool success = false;
    UniqueChars error;

    wasm::CompileTask* task = wasmTask();
    {
        AutoUnlockHelperThreadState unlock(locked);
        success = wasm::CompileFunction(task, &error);
    }

    // On success, try to move work to the finished list.
    if (success)
        success = HelperThreadState().wasmFinishedList(locked).append(task);

    // On failure, note the failure for harvesting by the parent.
    if (!success) {
        HelperThreadState().noteWasmFailure(locked);
        HelperThreadState().setWasmError(locked, Move(error));
    }

    // Notify the active thread in case it's waiting.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
    currentTask.reset();
}

void
HelperThread::handlePromiseTaskWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartPromiseTask(locked));
    MOZ_ASSERT(idle());

    PromiseTask* task = HelperThreadState().promiseTasks(locked).popCopy();
    currentTask.emplace(task);

    {
        AutoUnlockHelperThreadState unlock(locked);

        task->execute();

        if (!task->runtime()->finishAsyncTaskCallback(task)) {
            // We cannot simply delete the task now because the PromiseTask must
            // be destroyed on its runtime's thread. Add it to a list of tasks
            // to delete before the next GC.
            AutoEnterOOMUnsafeRegion oomUnsafe;
            if (!task->runtime()->promiseTasksToDestroy.lock()->append(task))
                oomUnsafe.crash("handlePromiseTaskWorkload");
        }
    }

    // Notify the active thread in case it's waiting.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
    currentTask.reset();
}

void
HelperThread::handleIonWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartIonCompile(locked));
    MOZ_ASSERT(idle());

    // Find the IonBuilder in the worklist with the highest priority, and
    // remove it from the worklist.
    jit::IonBuilder* builder =
        HelperThreadState().highestPriorityPendingIonCompile(locked, /* remove = */ true);

    // If there are now too many threads with active IonBuilders, indicate to
    // the one with the lowest priority that it should pause. Note that due to
    // builder priorities changing since pendingIonCompileHasSufficientPriority
    // was called, the builder we are pausing may actually be higher priority
    // than the one we are about to start. Oh well.
    HelperThread* other = HelperThreadState().lowestPriorityUnpausedIonCompileAtThreshold(locked);
    if (other) {
        MOZ_ASSERT(other->ionBuilder() && !other->pause);
        other->pause = true;
    }

    currentTask.emplace(builder);
    builder->setPauseFlag(&pause);

    JSRuntime* rt = builder->script()->compartment()->runtimeFromAnyThread();

    {
        AutoUnlockHelperThreadState unlock(locked);

        TraceLoggerThread* logger = TraceLoggerForCurrentThread();
        TraceLoggerEvent event(TraceLogger_AnnotateScripts, builder->script());
        AutoTraceLog logScript(logger, event);
        AutoTraceLog logCompile(logger, TraceLogger_IonCompilation);

        AutoSetContextRuntime ascr(rt);
        jit::JitContext jctx(jit::CompileRuntime::get(rt),
                             jit::CompileCompartment::get(builder->script()->compartment()),
                             &builder->alloc());
        builder->setBackgroundCodegen(jit::CompileBackEnd(builder));
    }

    FinishOffThreadIonCompile(builder, locked);

    // Ping any thread currently operating on the compiled script's zone group
    // so that the compiled code can be incorporated at the next interrupt
    // callback. Don't interrupt Ion code for this, as this incorporation can
    // be delayed indefinitely without affecting performance as long as the
    // active thread is actually executing Ion code.
    //
    // This must happen before the current task is reset. DestroyContext
    // cancels in progress Ion compilations before destroying its target
    // context, and after we reset the current task we are no longer considered
    // to be Ion compiling.
    JSContext* target = builder->script()->zoneFromAnyThread()->group()->ownerContext().context();
    if (target)
        target->requestInterrupt(JSContext::RequestInterruptCanWait);

    currentTask.reset();
    pause = false;

    // Notify the active thread in case it is waiting for the compilation to finish.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);

    // When finishing Ion compilation jobs, we can start unpausing compilation
    // threads that were paused to restrict the number of active compilations.
    // Only unpause one at a time, to make sure we don't exceed the restriction.
    // Since threads are currently only paused for Ion compilations, this
    // strategy will eventually unpause all paused threads, regardless of how
    // many there are, since each thread we unpause will eventually finish and
    // end up back here.
    if (HelperThread* other = HelperThreadState().highestPriorityPausedIonCompile(locked)) {
        MOZ_ASSERT(other->ionBuilder() && other->pause);

        // Only unpause the other thread if there isn't a higher priority
        // builder which this thread or another can start on.
        jit::IonBuilder* builder = HelperThreadState().highestPriorityPendingIonCompile(locked);
        if (!builder || IonBuilderHasHigherPriority(other->ionBuilder(), builder)) {
            other->pause = false;

            // Notify all paused threads, to make sure the one we just
            // unpaused wakes up.
            HelperThreadState().notifyAll(GlobalHelperThreadState::PAUSE, locked);
        }
    }
}

HelperThread*
js::CurrentHelperThread()
{
    if (!HelperThreadState().threads)
        return nullptr;
    auto threadId = ThisThread::GetId();
    for (auto& thisThread : *HelperThreadState().threads) {
        if (thisThread.thread.isSome() && threadId == thisThread.thread->get_id())
            return &thisThread;
    }
    return nullptr;
}

void
js::PauseCurrentHelperThread()
{
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logPaused(logger, TraceLogger_IonCompilationPaused);

    HelperThread* thread = CurrentHelperThread();
    MOZ_ASSERT(thread);

    AutoLockHelperThreadState lock;
    while (thread->pause)
        HelperThreadState().wait(lock, GlobalHelperThreadState::PAUSE);
}

bool
JSContext::addPendingCompileError(js::CompileError** error)
{
    auto errorPtr = make_unique<js::CompileError>();
    if (!errorPtr)
        return false;
    if (!helperThread()->parseTask()->errors.append(errorPtr.get())) {
        ReportOutOfMemory(this);
        return false;
    }
    *error = errorPtr.release();
    return true;
}

void
JSContext::addPendingOverRecursed()
{
    if (helperThread()->parseTask())
        helperThread()->parseTask()->overRecursed = true;
}

void
JSContext::addPendingOutOfMemory()
{
    // Keep in sync with recoverFromOutOfMemory.
    if (helperThread()->parseTask())
        helperThread()->parseTask()->outOfMemory = true;
}

void
HelperThread::handleParseWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartParseTask(locked));
    MOZ_ASSERT(idle());

    currentTask.emplace(HelperThreadState().parseWorklist(locked).popCopy());
    ParseTask* task = parseTask();

    {
        AutoUnlockHelperThreadState unlock(locked);
        AutoSetContextRuntime ascr(task->parseGlobal->runtimeFromAnyThread());

        JSContext* cx = TlsContext.get();
        AutoCompartment ac(cx, task->parseGlobal);

        task->parse(cx);
    }

    // The callback is invoked while we are still off thread.
    task->callback(task, task->callbackData);

    // FinishOffThreadScript will need to be called on the script to
    // migrate it into the correct compartment.
    {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!HelperThreadState().parseFinishedList(locked).append(task))
            oomUnsafe.crash("handleParseWorkload");
    }

    currentTask.reset();

    // Notify the active thread in case it is waiting for the parse/emit to finish.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void
HelperThread::handleCompressionWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartCompressionTask(locked));
    MOZ_ASSERT(idle());

    UniquePtr<SourceCompressionTask> task;
    {
        auto& worklist = HelperThreadState().compressionWorklist(locked);
        task = Move(worklist.back());
        worklist.popBack();
        currentTask.emplace(task.get());
    }

    {
        AutoUnlockHelperThreadState unlock(locked);

        TraceLoggerThread* logger = TraceLoggerForCurrentThread();
        AutoTraceLog logCompile(logger, TraceLogger_CompressSource);

        task->work();
    }

    {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!HelperThreadState().compressionFinishedList(locked).append(Move(task)))
            oomUnsafe.crash("handleCompressionWorkload");
    }

    currentTask.reset();

    // Notify the active thread in case it is waiting for the compression to finish.
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

bool
js::EnqueueOffThreadCompression(JSContext* cx, UniquePtr<SourceCompressionTask> task)
{
    AutoLockHelperThreadState lock;

    auto& pending = HelperThreadState().compressionPendingList(lock);
    if (!pending.append(Move(task))) {
        if (!cx->helperThread())
            ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

template <typename T>
static void
ClearCompressionTaskList(T& list, JSRuntime* runtime)
{
    for (size_t i = 0; i < list.length(); i++) {
        if (list[i]->runtimeMatches(runtime))
            HelperThreadState().remove(list, &i);
    }
}

void
js::CancelOffThreadCompressions(JSRuntime* runtime)
{
    AutoLockHelperThreadState lock;

    if (!HelperThreadState().threads)
        return;

    // Cancel all pending compression tasks.
    ClearCompressionTaskList(HelperThreadState().compressionPendingList(lock), runtime);
    ClearCompressionTaskList(HelperThreadState().compressionWorklist(lock), runtime);

    // Cancel all in-process compression tasks and wait for them to join so we
    // clean up the finished tasks.
    while (true) {
        bool inProgress = false;
        for (auto& thread : *HelperThreadState().threads) {
            SourceCompressionTask* task = thread.compressionTask();
            if (task && task->runtimeMatches(runtime))
                inProgress = true;
        }

        if (!inProgress)
            break;

        HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
    }

    // Clean up finished tasks.
    ClearCompressionTaskList(HelperThreadState().compressionFinishedList(lock), runtime);
}

bool
js::StartPromiseTask(JSContext* cx, UniquePtr<PromiseTask> task)
{
    // Execute synchronously if there are no helper threads.
    if (!CanUseExtraThreads())
        return task->executeAndFinish(cx);

    // If we fail to start, by interface contract, it is because the JSContext
    // is in the process of shutting down. Since promise handlers are not
    // necessarily run while shutting down *anyway*, we simply ignore the error.
    // This is symmetric with the handling of errors in finishAsyncTaskCallback
    // which, since it is off the JSContext's owner thread, cannot report an
    // error anyway.
    if (!cx->runtime()->startAsyncTaskCallback(cx, task.get())) {
        MOZ_ASSERT(!cx->isExceptionPending());
        return true;
    }

    // Per interface contract, after startAsyncTaskCallback succeeds,
    // finishAsyncTaskCallback *must* be called on all paths.

    AutoLockHelperThreadState lock;

    if (!HelperThreadState().promiseTasks(lock).append(task.get())) {
        Unused << cx->runtime()->finishAsyncTaskCallback(task.get());
        ReportOutOfMemory(cx);
        return false;
    }

    Unused << task.release();

    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
    return true;
}

void
GlobalHelperThreadState::trace(JSTracer* trc)
{
    AutoLockHelperThreadState lock;
    for (auto builder : ionWorklist(lock))
        builder->trace(trc);
    for (auto builder : ionFinishedList(lock))
        builder->trace(trc);

    if (HelperThreadState().threads) {
        for (auto& helper : *HelperThreadState().threads) {
            if (auto builder = helper.ionBuilder())
                builder->trace(trc);
        }
    }

    for (ZoneGroupsIter group(trc->runtime()); !group.done(); group.next()) {
        jit::IonBuilder* builder = group->ionLazyLinkList().getFirst();
        while (builder) {
            builder->trace(trc);
            builder = builder->getNext();
        }
    }

    for (auto parseTask : parseWorklist_)
        parseTask->trace(trc);
    for (auto parseTask : parseFinishedList_)
        parseTask->trace(trc);
    for (auto parseTask : parseWaitingOnGC_)
        parseTask->trace(trc);
}

void
HelperThread::handleGCHelperWorkload(AutoLockHelperThreadState& locked)
{
    MOZ_ASSERT(HelperThreadState().canStartGCHelperTask(locked));
    MOZ_ASSERT(idle());

    currentTask.emplace(HelperThreadState().gcHelperWorklist(locked).popCopy());
    GCHelperState* task = gcHelperTask();

    AutoSetContextRuntime ascr(task->runtime());

    {
        AutoUnlockHelperThreadState unlock(locked);
        task->work();
    }

    currentTask.reset();
    HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void
JSContext::setHelperThread(HelperThread* thread)
{
    helperThread_ = thread;
}

void
HelperThread::threadLoop()
{
    MOZ_ASSERT(CanUseExtraThreads());

    JS::AutoSuppressGCAnalysis nogc;
    AutoLockHelperThreadState lock;

    JSContext cx(nullptr, JS::ContextOptions());
    {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!cx.init(ContextKind::Background))
            oomUnsafe.crash("HelperThread cx.init()");
    }
    cx.setHelperThread(this);
    JS_SetNativeStackQuota(&cx, HELPER_STACK_QUOTA);

    while (true) {
        MOZ_ASSERT(idle());

        // Block until a task is available. Save the value of whether we are
        // going to do an Ion compile, in case the value returned by the method
        // changes.
        bool ionCompile = false;
        while (true) {
            if (terminate)
                return;
            if ((ionCompile = HelperThreadState().pendingIonCompileHasSufficientPriority(lock)) ||
                HelperThreadState().canStartWasmCompile(lock) ||
                HelperThreadState().canStartPromiseTask(lock) ||
                HelperThreadState().canStartParseTask(lock) ||
                HelperThreadState().canStartCompressionTask(lock) ||
                HelperThreadState().canStartGCHelperTask(lock) ||
                HelperThreadState().canStartGCParallelTask(lock))
            {
                break;
            }
            HelperThreadState().wait(lock, GlobalHelperThreadState::PRODUCER);
        }

        if (ionCompile) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_ION);
            handleIonWorkload(lock);
        } else if (HelperThreadState().canStartWasmCompile(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_WASM);
            handleWasmWorkload(lock);
        } else if (HelperThreadState().canStartPromiseTask(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_PROMISE_TASK);
            handlePromiseTaskWorkload(lock);
        } else if (HelperThreadState().canStartParseTask(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_PARSE);
            handleParseWorkload(lock);
        } else if (HelperThreadState().canStartCompressionTask(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_COMPRESS);
            handleCompressionWorkload(lock);
        } else if (HelperThreadState().canStartGCHelperTask(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_GCHELPER);
            handleGCHelperWorkload(lock);
        } else if (HelperThreadState().canStartGCParallelTask(lock)) {
            js::oom::SetThreadType(js::oom::THREAD_TYPE_GCPARALLEL);
            handleGCParallelWorkload(lock);
        } else {
            MOZ_CRASH("No task to perform");
        }
    }
}
