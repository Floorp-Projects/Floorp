/* -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set ts=4 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsworkers.h"

#include "ion/IonBuilder.h"

using namespace js;

#ifdef JS_THREADSAFE

bool
js::StartOffThreadIonCompile(JSContext *cx, ion::IonBuilder *builder)
{
    WorkerThreadState &state = *cx->runtime->workerThreadState;

    JS_ASSERT(state.numThreads);

    AutoLockWorkerThreadState lock(cx->runtime);

    if (!state.ionWorklist.append(builder))
        return false;

    state.notify(WorkerThreadState::WORKER);

    return true;
}

/*
 * Move an IonBuilder for which compilation has either finished, failed, or
 * been cancelled into the Ion compartment's finished compilations list.
 * All off thread compilations which are started must eventually be finished.
 */
static void
FinishOffThreadIonCompile(ion::IonBuilder *builder)
{
    JSCompartment *compartment = builder->script->compartment();
    JS_ASSERT(compartment->rt->workerThreadState->isLocked());

    compartment->ionCompartment()->finishedOffThreadCompilations().append(builder);
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
    WorkerThreadState &state = *compartment->rt->workerThreadState;

    ion::IonCompartment *ion = compartment->ionCompartment();
    if (!ion)
        return;

    AutoLockWorkerThreadState lock(compartment->rt);

    /* Cancel any pending entries for which processing hasn't started. */
    for (size_t i = 0; i < state.ionWorklist.length(); i++) {
        ion::IonBuilder *builder = state.ionWorklist[i];
        if (CompiledScriptMatches(compartment, script, builder->script)) {
            FinishOffThreadIonCompile(builder);
            state.ionWorklist[i--] = state.ionWorklist.back();
            state.ionWorklist.popBack();
        }
    }

    /* Wait for in progress entries to finish up. */
    for (size_t i = 0; i < state.numThreads; i++) {
        const WorkerThread &helper = state.threads[i];
        while (helper.ionScript && CompiledScriptMatches(compartment, script, helper.ionScript))
            state.wait(WorkerThreadState::MAIN);
    }

    ion::OffThreadCompilationVector &compilations = ion->finishedOffThreadCompilations();

    /* Cancel code generation for any completed entries. */
    for (size_t i = 0; i < compilations.length(); i++) {
        ion::IonBuilder *builder = compilations[i];
        if (CompiledScriptMatches(compartment, script, builder->script)) {
            ion::FinishOffThreadBuilder(builder);
            compilations[i--] = compilations.back();
            compilations.popBack();
        }
    }
}

bool
WorkerThreadState::init(JSRuntime *rt)
{
    workerLock = PR_NewLock();
    if (!workerLock)
        return false;

    mainWakeup = PR_NewCondVar(workerLock);
    if (!mainWakeup)
        return false;

    helperWakeup = PR_NewCondVar(workerLock);
    if (!helperWakeup)
        return false;

    numThreads = GetCPUCount() - 1;

    threads = (WorkerThread*) rt->calloc_(sizeof(WorkerThread) * numThreads);
    if (!threads) {
        numThreads = 0;
        return false;
    }

    for (size_t i = 0; i < numThreads; i++) {
        WorkerThread &helper = threads[i];
        helper.runtime = rt;
        helper.thread = PR_CreateThread(PR_USER_THREAD,
                                        WorkerThread::ThreadMain, &helper,
                                        PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (!helper.thread) {
            for (size_t j = 0; j < numThreads; j++)
                threads[j].destroy();
            Foreground::free_(threads);
            threads = NULL;
            numThreads = 0;
            return false;
        }
    }

    return true;
}

WorkerThreadState::~WorkerThreadState()
{
    /*
     * Join created threads first, which needs locks and condition variables
     * to be intact.
     */
    if (threads) {
        for (size_t i = 0; i < numThreads; i++)
            threads[i].destroy();
        Foreground::free_(threads);
    }

    if (workerLock)
        PR_DestroyLock(workerLock);

    if (mainWakeup)
        PR_DestroyCondVar(mainWakeup);

    if (helperWakeup)
        PR_DestroyCondVar(helperWakeup);
}

void
WorkerThreadState::lock()
{
    JS_ASSERT(!isLocked());
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
    lockOwner = NULL;
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
    lockOwner = NULL;
#endif
    DebugOnly<PRStatus> status =
        PR_WaitCondVar((which == MAIN) ? mainWakeup : helperWakeup,
                       millis ? PR_MillisecondsToInterval(millis) : PR_INTERVAL_NO_TIMEOUT);
    JS_ASSERT(status == PR_SUCCESS);
#ifdef DEBUG
    lockOwner = PR_GetCurrentThread();
#endif
}

void
WorkerThreadState::notify(CondVar which)
{
    JS_ASSERT(isLocked());
    PR_NotifyAllCondVar((which == MAIN) ? mainWakeup : helperWakeup);
}

void
WorkerThread::destroy()
{
    WorkerThreadState &state = *runtime->workerThreadState;

    if (!thread)
        return;

    {
        AutoLockWorkerThreadState lock(runtime);
        terminate = true;
        state.notify(WorkerThreadState::WORKER);
    }
    PR_JoinThread(thread);
}

/* static */
void
WorkerThread::ThreadMain(void *arg)
{
    PR_SetCurrentThreadName("Analysis Helper");
    static_cast<WorkerThread *>(arg)->threadLoop();
}

void
WorkerThread::threadLoop()
{
    WorkerThreadState &state = *runtime->workerThreadState;
    state.lock();

    while (true) {
        JS_ASSERT(!ionScript);

        while (state.ionWorklist.empty()) {
            if (terminate) {
                state.unlock();
                return;
            }
            state.wait(WorkerThreadState::WORKER);
        }

        ion::IonBuilder *builder = state.ionWorklist.popCopy();
        ionScript = builder->script;

        JS_ASSERT(ionScript->ion == ION_COMPILING_SCRIPT);

        state.unlock();

        {
            ion::IonContext ictx(NULL, ionScript->compartment(), &builder->temp());
            ion::CompileBackEnd(builder);
        }

        state.lock();

        ionScript = NULL;
        FinishOffThreadIonCompile(builder);

        /*
         * Notify the main thread in case it is waiting for the compilation to
         * finish.
         */
        state.notify(WorkerThreadState::MAIN);

        /*
         * Ping the main thread so that the compiled code can be incorporated
         * at the next operation callback.
         */
        runtime->triggerOperationCallback();
    }
}

#else /* JS_THREADSAFE */

bool
js::StartOffThreadIonCompile(JSContext *cx, ion::IonBuilder *builder)
{
    JS_NOT_REACHED("Off thread compilation not available in non-THREADSAFE builds");
    return false;
}

void
js::CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script)
{
}

#endif /* JS_THREADSAFE */
