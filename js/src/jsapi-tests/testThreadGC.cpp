/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#ifdef JS_THREADSAFE

#include "tests.h"
#include "prthread.h"

#include "jscntxt.h"

/*
 * We test that if a GC callback cancels the GC on a child thread the GC can
 * still proceed on the main thread even if the child thread continue to
 * run uninterrupted.
 */

struct SharedData {
    enum ChildState {
        CHILD_STARTING,
        CHILD_RUNNING,
        CHILD_DONE,
        CHILD_ERROR
    };

    JSRuntime   *const runtime;
    PRThread    *const mainThread;
    PRLock      *const lock;
    PRCondVar   *const signal;
    ChildState  childState;
    bool        childShouldStop;
    JSContext   *childContext;

    SharedData(JSRuntime *rt, bool *ok)
      : runtime(rt),
        mainThread(PR_GetCurrentThread()),
        lock(PR_NewLock()),
        signal(lock ? PR_NewCondVar(lock) : NULL),
        childState(CHILD_STARTING),
        childShouldStop(false),
        childContext(NULL)
    {
        JS_ASSERT(!*ok);
        *ok = !!signal;
    }

    ~SharedData() {
        if (signal)
            PR_DestroyCondVar(signal);
        if (lock)
            PR_DestroyLock(lock);
    }
};

static SharedData *shared;

static JSBool
CancelNonMainThreadGCCallback(JSContext *cx, JSGCStatus status)
{
    return status != JSGC_BEGIN || PR_GetCurrentThread() == shared->mainThread;
}

static JSBool
StopChildOperationCallback(JSContext *cx)
{
    bool shouldStop;
    PR_Lock(shared->lock);
    shouldStop = shared->childShouldStop;
    PR_Unlock(shared->lock);
    return !shouldStop;
}

static JSBool
NotifyMainThreadAboutBusyLoop(JSContext *cx, uintN argc, jsval *vp)
{
    PR_Lock(shared->lock);
    JS_ASSERT(shared->childState == SharedData::CHILD_STARTING);
    shared->childState = SharedData::CHILD_RUNNING;
    shared->childContext = cx;
    PR_NotifyCondVar(shared->signal);
    PR_Unlock(shared->lock);

    return true;
}

static void
ChildThreadMain(void *arg)
{
    JS_ASSERT(!arg);
    bool error = true;
    JSContext *cx = JS_NewContext(shared->runtime, 8192);
    if (cx) {
        JS_SetOperationCallback(cx, StopChildOperationCallback);
        JSAutoRequest ar(cx);
        JSObject *global = JS_NewCompartmentAndGlobalObject(cx, JSAPITest::basicGlobalClass(),
                                                            NULL);
        if (global) {
            JS_SetGlobalObject(cx, global);
            if (JS_InitStandardClasses(cx, global) &&
                JS_DefineFunction(cx, global, "notify", NotifyMainThreadAboutBusyLoop, 0, 0)) {

                jsval rval;
                static const char code[] = "var i = 0; notify(); for (var i = 0; ; ++i);";
                JSBool ok = JS_EvaluateScript(cx, global, code, strlen(code),
                                              __FILE__, __LINE__, &rval);
                if (!ok && !JS_IsExceptionPending(cx)) {
                    /* Evaluate should only return via the callback cancellation. */
                    error = false;
                }
            }
        }
    }

    PR_Lock(shared->lock);
    shared->childState = error ? SharedData::CHILD_DONE : SharedData::CHILD_ERROR;
    shared->childContext = NULL;
    PR_NotifyCondVar(shared->signal);
    PR_Unlock(shared->lock);

    if (cx)
        JS_DestroyContextNoGC(cx);
}

BEGIN_TEST(testThreadGC_bug590533)
    {
        /*
         * Test the child thread busy running while the current thread calls
         * the GC both with JSRuntime->gcIsNeeded set and unset.
         */
        bool ok = TestChildThread(true);
        CHECK(ok);
        ok = TestChildThread(false);
        CHECK(ok);
        return ok;
    }

    bool TestChildThread(bool setGCIsNeeded)
    {
        bool ok = false;
        shared = new SharedData(rt, &ok);
        CHECK(ok);

        JSGCCallback oldGCCallback = JS_SetGCCallback(cx, CancelNonMainThreadGCCallback);

        PRThread *thread =
            PR_CreateThread(PR_USER_THREAD, ChildThreadMain, NULL,
                            PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (!thread)
            return false;

        PR_Lock(shared->lock);
        while (shared->childState == SharedData::CHILD_STARTING)
            PR_WaitCondVar(shared->signal, PR_INTERVAL_NO_TIMEOUT);
        JS_ASSERT(shared->childState != SharedData::CHILD_DONE);
        ok = (shared->childState == SharedData::CHILD_RUNNING);
        PR_Unlock(shared->lock);

        CHECK(ok);

        if (setGCIsNeeded) {
            /*
             * Use JS internal API to set the GC trigger flag after we know
             * that the child is in a request and is about to run an infinite
             * loop. Then run the GC with JSRuntime->gcIsNeeded flag set.
             */
            js::AutoLockGC lock(rt);
            js::TriggerGC(rt, js::gcstats::PUBLIC_API);
        }

        JS_GC(cx);

        PR_Lock(shared->lock);
        shared->childShouldStop = true;
        while (shared->childState == SharedData::CHILD_RUNNING) {
            JS_TriggerOperationCallback(shared->childContext);
            PR_WaitCondVar(shared->signal, PR_INTERVAL_NO_TIMEOUT);
        }
        JS_ASSERT(shared->childState != SharedData::CHILD_STARTING);
        ok = (shared->childState == SharedData::CHILD_DONE);
        PR_Unlock(shared->lock);

        JS_SetGCCallback(cx, oldGCCallback);

        PR_JoinThread(thread);

        delete shared;
        shared = NULL;

        return true;
    }


END_TEST(testThreadGC_bug590533)

#endif
