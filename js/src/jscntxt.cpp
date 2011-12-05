/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS execution context.
 */

#include <limits.h> /* make sure that <features.h> is included and we can use
                       __GLIBC__ to detect glibc presence */
#include <new>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef ANDROID
# include <android/log.h>
# include <fstream>
# include <string>
#endif  // ANDROID

#include "jsstdint.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsclist.h"
#include "jsprf.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsdtoa.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnativestack.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jspubtd.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#ifdef JS_METHODJIT
# include "assembler/assembler/MacroAssembler.h"
#endif
#include "frontend/TokenStream.h"
#include "frontend/ParseMaps.h"
#include "yarr/BumpPointerAllocator.h"

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jscompartment.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

namespace js {

ThreadData::ThreadData(JSRuntime *rt)
  : rt(rt),
    interruptFlags(0),
#ifdef JS_THREADSAFE
    requestDepth(0),
#endif
    waiveGCQuota(false),
    tempLifoAlloc(TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    execAlloc(NULL),
    bumpAlloc(NULL),
    repCache(NULL),
    dtoaState(NULL),
    nativeStackBase(GetNativeStackBase()),
    pendingProxyOperation(NULL),
    interpreterFrames(NULL)
{
#ifdef DEBUG
    noGCOrAllocationCheck = 0;
#endif
}

ThreadData::~ThreadData()
{
    JS_ASSERT(!repCache);

    rt->delete_<JSC::ExecutableAllocator>(execAlloc);
    rt->delete_<WTF::BumpPointerAllocator>(bumpAlloc);

    if (dtoaState)
        js_DestroyDtoaState(dtoaState);
}

bool
ThreadData::init()
{
    JS_ASSERT(!repCache);
    return stackSpace.init() && !!(dtoaState = js_NewDtoaState());
}

#ifdef JS_THREADSAFE
void
ThreadData::sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *normal, size_t *temporary,
                                size_t *regexpCode, size_t *stackCommitted)
{
    /*
     * There are other ThreadData members that could be measured; the ones
     * below have been seen by DMD to be worth measuring.  More stuff may be
     * added later.
     */

    /*
     * The computedSize is 0 because sizeof(DtoaState) isn't available here and
     * it's not worth making it available.
     */
    *normal = mallocSizeOf(dtoaState, /* sizeof(DtoaState) */0);

    *temporary = tempLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    size_t method = 0, regexp = 0, unused = 0;
    if (execAlloc)
        execAlloc->sizeOfCode(&method, &regexp, &unused);
    JS_ASSERT(method == 0);     /* this execAlloc is only used for regexp code */
    *regexpCode = regexp + unused;

    *stackCommitted = stackSpace.sizeOfCommitted();
}
#endif

void
ThreadData::triggerOperationCallback(JSRuntime *rt)
{
    JS_ASSERT(rt == this->rt);

    /*
     * Use JS_ATOMIC_SET and JS_ATOMIC_INCREMENT in the hope that it ensures
     * the write will become immediately visible to other processors polling
     * the flag.  Note that we only care about visibility here, not read/write
     * ordering: this field can only be written with the GC lock held.
     */
    if (interruptFlags)
        return;
    JS_ATOMIC_SET(&interruptFlags, 1);

#ifdef JS_THREADSAFE
    /* rt->interruptCounter does not reflect suspended threads. */
    if (requestDepth != 0)
        JS_ATOMIC_INCREMENT(&rt->interruptCounter);
#endif
}

JSC::ExecutableAllocator *
ThreadData::createExecutableAllocator(JSContext *cx)
{
    JS_ASSERT(!execAlloc);
    JS_ASSERT(cx->runtime == rt);

    execAlloc = rt->new_<JSC::ExecutableAllocator>();
    if (!execAlloc)
        js_ReportOutOfMemory(cx);
    return execAlloc;
}

WTF::BumpPointerAllocator *
ThreadData::createBumpPointerAllocator(JSContext *cx)
{
    JS_ASSERT(!bumpAlloc);
    JS_ASSERT(cx->runtime == rt);

    bumpAlloc = rt->new_<WTF::BumpPointerAllocator>();
    if (!bumpAlloc)
        js_ReportOutOfMemory(cx);
    return bumpAlloc;
}

RegExpPrivateCache *
ThreadData::createRegExpPrivateCache(JSContext *cx)
{
    JS_ASSERT(!repCache);
    JS_ASSERT(cx->runtime == rt);

    RegExpPrivateCache *newCache = rt->new_<RegExpPrivateCache>(rt);

    if (!newCache || !newCache->init()) {
        js_ReportOutOfMemory(cx);
        rt->delete_<RegExpPrivateCache>(newCache);
        return NULL;
    }

    repCache = newCache;
    return repCache;
}

void
ThreadData::purgeRegExpPrivateCache()
{
    rt->delete_<RegExpPrivateCache>(repCache);
    repCache = NULL;
}

} /* namespace js */

JSScript *
js_GetCurrentScript(JSContext *cx)
{
    return cx->hasfp() ? cx->fp()->maybeScript() : NULL;
}


#ifdef JS_THREADSAFE

void
JSThread::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *normal, size_t *temporary,
                              size_t *regexpCode, size_t *stackCommitted)
{
    data.sizeOfExcludingThis(mallocSizeOf, normal, temporary, regexpCode, stackCommitted);
    *normal += mallocSizeOf(this, sizeof(JSThread));
}

JSThread *
js_CurrentThreadAndLockGC(JSRuntime *rt)
{
    void *id = js_CurrentThreadId();
    JS_LOCK_GC(rt);

    /*
     * We must not race with a GC that accesses cx->thread for JSContext
     * instances on all threads, see bug 476934.
     */
    js_WaitForGC(rt);

    JSThread *thread;
    JSThread::Map::AddPtr p = rt->threads.lookupForAdd(id);
    if (p) {
        thread = p->value;

        /*
         * If thread has no contexts, it might be left over from a previous
         * thread with the same id but a different stack address.
         */
        if (JS_CLIST_IS_EMPTY(&thread->contextList))
            thread->data.nativeStackBase = GetNativeStackBase();
    } else {
        JS_UNLOCK_GC(rt);

        thread = OffTheBooks::new_<JSThread>(rt, id);
        if (!thread || !thread->init()) {
            Foreground::delete_(thread);
            return NULL;
        }
        JS_LOCK_GC(rt);
        js_WaitForGC(rt);
        if (!rt->threads.relookupOrAdd(p, id, thread)) {
            JS_UNLOCK_GC(rt);
            Foreground::delete_(thread);
            return NULL;
        }

        /* Another thread cannot add an entry for the current thread id. */
        JS_ASSERT(p->value == thread);
    }
    JS_ASSERT(thread->id == id);

    /*
     * We skip the assert under glibc due to an apparent bug there, see
     * bug 608526.
     */
#ifndef __GLIBC__
    JS_ASSERT(GetNativeStackBase() == thread->data.nativeStackBase);
#endif

    return thread;
}

JSBool
js_InitContextThreadAndLockGC(JSContext *cx)
{
    JSThread *thread = js_CurrentThreadAndLockGC(cx->runtime);
    if (!thread)
        return false;

    JS_APPEND_LINK(&cx->threadLinks, &thread->contextList);
    cx->setThread(thread);
    return true;
}

void
JSContext::setThread(JSThread *thread)
{
    thread_ = thread;
    stack.threadReset();
}

void
js_ClearContextThread(JSContext *cx)
{
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread()));
    JS_REMOVE_AND_INIT_LINK(&cx->threadLinks);
    cx->setThread(NULL);
}

#endif /* JS_THREADSAFE */

ThreadData *
js_CurrentThreadData(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    JSThread *thread = js_CurrentThreadAndLockGC(rt);
    if (!thread)
        return NULL;

    return &thread->data;
#else
    return &rt->threadData;
#endif
}

JSBool
js_InitThreads(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    return rt->threads.init(4);
#else
    return rt->threadData.init();
#endif
}

void
js_FinishThreads(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    if (!rt->threads.initialized())
        return;
    for (JSThread::Map::Range r = rt->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        Foreground::delete_(thread);
    }
    rt->threads.clear();
#endif
}

void
js_PurgeThreads(JSContext *cx)
{
#ifdef JS_THREADSAFE
    for (JSThread::Map::Enum e(cx->runtime->threads);
         !e.empty();
         e.popFront()) {
        JSThread *thread = e.front().value;

        if (JS_CLIST_IS_EMPTY(&thread->contextList)) {
            JS_ASSERT(cx->thread() != thread);
            Foreground::delete_(thread);
            e.removeFront();
        } else {
            thread->data.purge(cx);
        }
    }
#else
    cx->runtime->threadData.purge(cx);
#endif
}

void
js_PurgeThreads_PostGlobalSweep(JSContext *cx)
{
#ifdef JS_THREADSAFE
    for (JSThread::Map::Enum e(cx->runtime->threads);
         !e.empty();
         e.popFront())
    {
        JSThread *thread = e.front().value;

        JS_ASSERT(!JS_CLIST_IS_EMPTY(&thread->contextList));
        thread->data.purgeRegExpPrivateCache();
    }
#else
    cx->runtime->threadData.purgeRegExpPrivateCache();
#endif
}

JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    JS_AbortIfWrongThread(rt);

    /*
     * We need to initialize the new context fully before adding it to the
     * runtime list. After that it can be accessed from another thread via
     * js_ContextIterator.
     */
    JSContext *cx = OffTheBooks::new_<JSContext>(rt);
    if (!cx)
        return NULL;

    JS_ASSERT(cx->findVersion() == JSVERSION_DEFAULT);
    VOUCH_DOES_NOT_REQUIRE_STACK();

    if (!cx->busyArrays.init()) {
        Foreground::delete_(cx);
        return NULL;
    }

#ifdef JS_THREADSAFE
    if (!js_InitContextThreadAndLockGC(cx)) {
        Foreground::delete_(cx);
        return NULL;
    }
#endif

    /*
     * Here the GC lock is still held after js_InitContextThreadAndLockGC took it and
     * the GC is not running on another thread.
     */
    bool first;
    for (;;) {
        if (rt->state == JSRTS_UP) {
            JS_ASSERT(!JS_CLIST_IS_EMPTY(&rt->contextList));
            first = false;
            break;
        }
        if (rt->state == JSRTS_DOWN) {
            JS_ASSERT(JS_CLIST_IS_EMPTY(&rt->contextList));
            first = true;
            rt->state = JSRTS_LAUNCHING;
            break;
        }
        JS_WAIT_CONDVAR(rt->stateChange, JS_NO_TIMEOUT);

        /*
         * During the above wait after we are notified about the state change
         * but before we wake up, another thread could enter the GC from
         * js_DestroyContext, bug 478336. So we must wait here to ensure that
         * when we exit the loop with the first flag set to true, that GC is
         * finished.
         */
        js_WaitForGC(rt);
    }
    JS_APPEND_LINK(&cx->link, &rt->contextList);
    JS_UNLOCK_GC(rt);

    js_InitRandom(cx);

    /*
     * If cx is the first context on this runtime, initialize well-known atoms,
     * keywords, numbers, and strings.  If one of these steps should fail, the
     * runtime will be left in a partially initialized state, with zeroes and
     * nulls stored in the default-initialized remainder of the struct.  We'll
     * clean the runtime up under js_DestroyContext, because cx will be "last"
     * as well as "first".
     */
    if (first) {
#ifdef JS_THREADSAFE
        JS_BeginRequest(cx);
#endif
        bool ok = rt->staticStrings.init(cx);
        if (ok)
            ok = js_InitCommonAtoms(cx);

#ifdef JS_THREADSAFE
        JS_EndRequest(cx);
#endif
        if (!ok) {
            js_DestroyContext(cx, JSDCM_NEW_FAILED);
            return NULL;
        }

        AutoLockGC lock(rt);
        rt->state = JSRTS_UP;
        JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
    }

    JSContextCallback cxCallback = rt->cxCallback;
    if (cxCallback && !cxCallback(cx, JSCONTEXT_NEW)) {
        js_DestroyContext(cx, JSDCM_NEW_FAILED);
        return NULL;
    }

    return cx;
}

void
js_DestroyContext(JSContext *cx, JSDestroyContextMode mode)
{
    JSRuntime *rt = cx->runtime;
    JS_AbortIfWrongThread(rt);

    JSContextCallback cxCallback;
    JSBool last;

    JS_ASSERT(!cx->enumerators);

#ifdef JS_THREADSAFE
    /*
     * For API compatibility we allow to destroy contexts without a thread in
     * optimized builds. We assume that the embedding knows that an OOM error
     * cannot happen in JS_SetContextThread.
     */
    JS_ASSERT(cx->thread() && CURRENT_THREAD_IS_ME(cx->thread()));
    if (!cx->thread())
        JS_SetContextThread(cx);

    /*
     * For API compatibility we support destroying contexts with non-zero
     * cx->outstandingRequests but we assume that all JS_BeginRequest calls
     * on this cx contributes to cx->thread->data.requestDepth and there is no
     * JS_SuspendRequest calls that set aside the counter.
     */
    JS_ASSERT(cx->outstandingRequests <= cx->thread()->data.requestDepth);
#endif

    if (mode != JSDCM_NEW_FAILED) {
        cxCallback = rt->cxCallback;
        if (cxCallback) {
            /*
             * JSCONTEXT_DESTROY callback is not allowed to fail and must
             * return true.
             */
            DebugOnly<JSBool> callbackStatus = cxCallback(cx, JSCONTEXT_DESTROY);
            JS_ASSERT(callbackStatus);
        }
    }

    JS_LOCK_GC(rt);
    JS_ASSERT(rt->state == JSRTS_UP || rt->state == JSRTS_LAUNCHING);
#ifdef JS_THREADSAFE
    /*
     * Typically we are called outside a request, so ensure that the GC is not
     * running before removing the context from rt->contextList, see bug 477021.
     */
    if (cx->thread()->data.requestDepth == 0)
        js_WaitForGC(rt);
#endif
    JS_REMOVE_LINK(&cx->link);
    last = (rt->contextList.next == &rt->contextList);
    if (last)
        rt->state = JSRTS_LANDING;
    if (last || mode == JSDCM_FORCE_GC || mode == JSDCM_MAYBE_GC
#ifdef JS_THREADSAFE
        || cx->outstandingRequests != 0
#endif
        ) {
        JS_ASSERT(!rt->gcRunning);

#ifdef JS_THREADSAFE
        rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
        JS_UNLOCK_GC(rt);

        if (last) {
#ifdef JS_THREADSAFE
            /*
             * If this thread is not in a request already, begin one now so
             * that we wait for any racing GC started on a not-last context to
             * finish, before we plow ahead and unpin atoms. Note that even
             * though we begin a request here if necessary, we end all
             * thread's requests before forcing a final GC. This lets any
             * not-last context destruction racing in another thread try to
             * force or maybe run the GC, but by that point, rt->state will
             * not be JSRTS_UP, and that GC attempt will return early.
             */
            if (cx->thread()->data.requestDepth == 0)
                JS_BeginRequest(cx);
#endif

            /*
             * Dump remaining type inference results first. This printing
             * depends on atoms still existing.
             */
            {
                AutoLockGC lock(rt);
                for (CompartmentsIter c(rt); !c.done(); c.next())
                    c->types.print(cx, false);
            }

            /* Unpin all common atoms before final GC. */
            js_FinishCommonAtoms(cx);

            /* Clear debugging state to remove GC roots. */
            for (CompartmentsIter c(rt); !c.done(); c.next())
                c->clearTraps(cx, NULL);
            JS_ClearAllWatchPoints(cx);
        }

#ifdef JS_THREADSAFE
        /*
         * Destroying a context implicitly calls JS_EndRequest().  Also, we must
         * end our request here in case we are "last" -- in that event, another
         * js_DestroyContext that was not last might be waiting in the GC for our
         * request to end.  We'll let it run below, just before we do the truly
         * final GC and then free atom state.
         */
        while (cx->outstandingRequests != 0)
            JS_EndRequest(cx);
#endif

        if (last) {
            js_GC(cx, NULL, GC_LAST_CONTEXT, gcstats::LASTCONTEXT);

            /* Take the runtime down, now that it has no contexts or atoms. */
            JS_LOCK_GC(rt);
            rt->state = JSRTS_DOWN;
            JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
        } else {
            if (mode == JSDCM_FORCE_GC)
                js_GC(cx, NULL, GC_NORMAL, gcstats::DESTROYCONTEXT);
            else if (mode == JSDCM_MAYBE_GC)
                JS_MaybeGC(cx);

            JS_LOCK_GC(rt);
            js_WaitForGC(rt);
        }
    }
#ifdef JS_THREADSAFE
#ifdef DEBUG
    JSThread *t = cx->thread();
#endif
    js_ClearContextThread(cx);
    JS_ASSERT_IF(JS_CLIST_IS_EMPTY(&t->contextList), !t->data.requestDepth);
#endif
#ifdef JS_THREADSAFE
    rt->gcHelperThread.waitBackgroundSweepEnd();
#endif
    JS_UNLOCK_GC(rt);
    Foreground::delete_(cx);
}

JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp)
{
    JSContext *cx = *iterp;

    Maybe<AutoLockGC> lockIf;
    if (unlocked)
        lockIf.construct(rt);
    cx = JSContext::fromLinkField(cx ? cx->link.next : rt->contextList.next);
    if (&cx->link == &rt->contextList)
        cx = NULL;
    *iterp = cx;
    return cx;
}

JS_FRIEND_API(JSContext *)
js_NextActiveContext(JSRuntime *rt, JSContext *cx)
{
    JSContext *iter = cx;
#ifdef JS_THREADSAFE
    while ((cx = js_ContextIterator(rt, JS_FALSE, &iter)) != NULL) {
        if (cx->outstandingRequests && cx->thread()->data.requestDepth)
            break;
    }
    return cx;
#else
    return js_ContextIterator(rt, JS_FALSE, &iter);
#endif
}

namespace js {

bool
AutoResolving::alreadyStartedSlow() const
{
    JS_ASSERT(link);
    AutoResolving *cursor = link;
    do {
        JS_ASSERT(this != cursor);
        if (object == cursor->object && id == cursor->id && kind == cursor->kind)
            return true;
    } while (!!(cursor = cursor->link));
    return false;
}

} /* namespace js */

static void
ReportError(JSContext *cx, const char *message, JSErrorReport *reportp,
            JSErrorCallback callback, void *userRef)
{
    /*
     * Check the error report, and set a JavaScript-catchable exception
     * if the error is defined to have an associated exception.  If an
     * exception is thrown, then the JSREPORT_EXCEPTION flag will be set
     * on the error report, and exception-aware hosts should ignore it.
     */
    JS_ASSERT(reportp);
    if ((!callback || callback == js_GetErrorMessage) &&
        reportp->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)
        reportp->flags |= JSREPORT_EXCEPTION;

    /*
     * Call the error reporter only if an exception wasn't raised.
     *
     * If an exception was raised, then we call the debugErrorHook
     * (if present) to give it a chance to see the error before it
     * propagates out of scope.  This is needed for compatibility
     * with the old scheme.
     */
    if (!JS_IsRunning(cx) ||
        !js_ErrorToException(cx, message, reportp, callback, userRef)) {
        js_ReportErrorAgain(cx, message, reportp);
    } else if (cx->debugHooks->debugErrorHook && cx->errorReporter) {
        JSDebugErrorHook hook = cx->debugHooks->debugErrorHook;
        /* test local in case debugErrorHook changed on another thread */
        if (hook)
            hook(cx, message, reportp, cx->debugHooks->debugErrorHookData);
    }
}

/* The report must be initially zeroed. */
static void
PopulateReportBlame(JSContext *cx, JSErrorReport *report)
{
    /*
     * Walk stack until we find a frame that is associated with some script
     * rather than a native frame.
     */
    for (FrameRegsIter iter(cx); !iter.done(); ++iter) {
        if (iter.fp()->isScriptFrame()) {
            report->filename = iter.fp()->script()->filename;
            report->lineno = js_FramePCToLineNumber(cx, iter.fp(), iter.pc());
            break;
        }
    }
}

/*
 * We don't post an exception in this case, since doing so runs into
 * complications of pre-allocating an exception object which required
 * running the Exception class initializer early etc.
 * Instead we just invoke the errorReporter with an "Out Of Memory"
 * type message, and then hope the process ends swiftly.
 */
void
js_ReportOutOfMemory(JSContext *cx)
{
    cx->runtime->hadOutOfMemory = true;

    JSErrorReport report;
    JSErrorReporter onError = cx->errorReporter;

    /* Get the message for this error, but we won't expand any arguments. */
    const JSErrorFormatString *efs =
        js_GetLocalizedErrorMessage(cx, NULL, NULL, JSMSG_OUT_OF_MEMORY);
    const char *msg = efs ? efs->format : "Out of memory";

    /* Fill out the report, but don't do anything that requires allocation. */
    PodZero(&report);
    report.flags = JSREPORT_ERROR;
    report.errorNumber = JSMSG_OUT_OF_MEMORY;
    PopulateReportBlame(cx, &report);

    /*
     * If debugErrorHook is present then we give it a chance to veto sending
     * the error on to the regular ErrorReporter. We also clear a pending
     * exception if any now so the hooks can replace the out-of-memory error
     * by a script-catchable exception.
     */
    cx->clearPendingException();
    if (onError) {
        JSDebugErrorHook hook = cx->debugHooks->debugErrorHook;
        if (hook &&
            !hook(cx, msg, &report, cx->debugHooks->debugErrorHookData)) {
            onError = NULL;
        }
    }

    if (onError) {
        AutoAtomicIncrement incr(&cx->runtime->inOOMReport);
        onError(cx, msg, &report);
    }
}

JS_FRIEND_API(void)
js_ReportOverRecursed(JSContext *maybecx)
{
#ifdef JS_MORE_DETERMINISTIC
    /*
     * We cannot make stack depth deterministic across different
     * implementations (e.g. JIT vs. interpreter will differ in
     * their maximum stack depth).
     * However, we can detect externally when we hit the maximum
     * stack depth which is useful for external testing programs
     * like fuzzers.
     */
    fprintf(stderr, "js_ReportOverRecursed called\n");
#endif
    if (maybecx)
        JS_ReportErrorNumber(maybecx, js_GetErrorMessage, NULL, JSMSG_OVER_RECURSED);
}

void
js_ReportAllocationOverflow(JSContext *maybecx)
{
    if (maybecx)
        JS_ReportErrorNumber(maybecx, js_GetErrorMessage, NULL, JSMSG_ALLOC_OVERFLOW);
}

/*
 * Given flags and the state of cx, decide whether we should report an
 * error, a warning, or just continue execution normally.  Return
 * true if we should continue normally, without reporting anything;
 * otherwise, adjust *flags as appropriate and return false.
 */
static bool
checkReportFlags(JSContext *cx, uintN *flags)
{
    if (JSREPORT_IS_STRICT_MODE_ERROR(*flags)) {
        /*
         * Error in strict code; warning with strict option; okay otherwise.
         * We assume that if the top frame is a native, then it is strict if
         * the nearest scripted frame is strict, see bug 536306.
         */
        JSScript *script = cx->stack.currentScript();
        if (script && script->strictModeCode)
            *flags &= ~JSREPORT_WARNING;
        else if (cx->hasStrictOption())
            *flags |= JSREPORT_WARNING;
        else
            return true;
    } else if (JSREPORT_IS_STRICT(*flags)) {
        /* Warning/error only when JSOPTION_STRICT is set. */
        if (!cx->hasStrictOption())
            return true;
    }

    /* Warnings become errors when JSOPTION_WERROR is set. */
    if (JSREPORT_IS_WARNING(*flags) && cx->hasWErrorOption())
        *flags &= ~JSREPORT_WARNING;

    return false;
}

JSBool
js_ReportErrorVA(JSContext *cx, uintN flags, const char *format, va_list ap)
{
    char *message;
    jschar *ucmessage;
    size_t messagelen;
    JSErrorReport report;
    JSBool warning;

    if (checkReportFlags(cx, &flags))
        return JS_TRUE;

    message = JS_vsmprintf(format, ap);
    if (!message)
        return JS_FALSE;
    messagelen = strlen(message);

    PodZero(&report);
    report.flags = flags;
    report.errorNumber = JSMSG_USER_DEFINED_ERROR;
    report.ucmessage = ucmessage = InflateString(cx, message, &messagelen);
    PopulateReportBlame(cx, &report);

    warning = JSREPORT_IS_WARNING(report.flags);

    ReportError(cx, message, &report, NULL, NULL);
    Foreground::free_(message);
    Foreground::free_(ucmessage);
    return warning;
}

/*
 * The arguments from ap need to be packaged up into an array and stored
 * into the report struct.
 *
 * The format string addressed by the error number may contain operands
 * identified by the format {N}, where N is a decimal digit. Each of these
 * is to be replaced by the Nth argument from the va_list. The complete
 * message is placed into reportp->ucmessage converted to a JSString.
 *
 * Returns true if the expansion succeeds (can fail if out of memory).
 */
JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const uintN errorNumber,
                        char **messagep, JSErrorReport *reportp,
                        bool charArgs, va_list ap)
{
    const JSErrorFormatString *efs;
    int i;
    int argCount;

    *messagep = NULL;

    /* Most calls supply js_GetErrorMessage; if this is so, assume NULL. */
    if (!callback || callback == js_GetErrorMessage)
        efs = js_GetLocalizedErrorMessage(cx, userRef, NULL, errorNumber);
    else
        efs = callback(userRef, NULL, errorNumber);
    if (efs) {
        size_t totalArgsLength = 0;
        size_t argLengths[10]; /* only {0} thru {9} supported */
        argCount = efs->argCount;
        JS_ASSERT(argCount <= 10);
        if (argCount > 0) {
            /*
             * Gather the arguments into an array, and accumulate
             * their sizes. We allocate 1 more than necessary and
             * null it out to act as the caboose when we free the
             * pointers later.
             */
            reportp->messageArgs = (const jschar **)
                cx->malloc_(sizeof(jschar *) * (argCount + 1));
            if (!reportp->messageArgs)
                return JS_FALSE;
            reportp->messageArgs[argCount] = NULL;
            for (i = 0; i < argCount; i++) {
                if (charArgs) {
                    char *charArg = va_arg(ap, char *);
                    size_t charArgLength = strlen(charArg);
                    reportp->messageArgs[i] = InflateString(cx, charArg, &charArgLength);
                    if (!reportp->messageArgs[i])
                        goto error;
                } else {
                    reportp->messageArgs[i] = va_arg(ap, jschar *);
                }
                argLengths[i] = js_strlen(reportp->messageArgs[i]);
                totalArgsLength += argLengths[i];
            }
            /* NULL-terminate for easy copying. */
            reportp->messageArgs[i] = NULL;
        }
        /*
         * Parse the error format, substituting the argument X
         * for {X} in the format.
         */
        if (argCount > 0) {
            if (efs->format) {
                jschar *buffer, *fmt, *out;
                int expandedArgs = 0;
                size_t expandedLength;
                size_t len = strlen(efs->format);

                buffer = fmt = InflateString(cx, efs->format, &len);
                if (!buffer)
                    goto error;
                expandedLength = len
                                 - (3 * argCount)       /* exclude the {n} */
                                 + totalArgsLength;

                /*
                * Note - the above calculation assumes that each argument
                * is used once and only once in the expansion !!!
                */
                reportp->ucmessage = out = (jschar *)
                    cx->malloc_((expandedLength + 1) * sizeof(jschar));
                if (!out) {
                    cx->free_(buffer);
                    goto error;
                }
                while (*fmt) {
                    if (*fmt == '{') {
                        if (isdigit(fmt[1])) {
                            int d = JS7_UNDEC(fmt[1]);
                            JS_ASSERT(d < argCount);
                            js_strncpy(out, reportp->messageArgs[d],
                                       argLengths[d]);
                            out += argLengths[d];
                            fmt += 3;
                            expandedArgs++;
                            continue;
                        }
                    }
                    *out++ = *fmt++;
                }
                JS_ASSERT(expandedArgs == argCount);
                *out = 0;
                cx->free_(buffer);
                *messagep = DeflateString(cx, reportp->ucmessage,
                                          size_t(out - reportp->ucmessage));
                if (!*messagep)
                    goto error;
            }
        } else {
            /*
             * Zero arguments: the format string (if it exists) is the
             * entire message.
             */
            if (efs->format) {
                size_t len;
                *messagep = JS_strdup(cx, efs->format);
                if (!*messagep)
                    goto error;
                len = strlen(*messagep);
                reportp->ucmessage = InflateString(cx, *messagep, &len);
                if (!reportp->ucmessage)
                    goto error;
            }
        }
    }
    if (*messagep == NULL) {
        /* where's the right place for this ??? */
        const char *defaultErrorMessage
            = "No error message available for error number %d";
        size_t nbytes = strlen(defaultErrorMessage) + 16;
        *messagep = (char *)cx->malloc_(nbytes);
        if (!*messagep)
            goto error;
        JS_snprintf(*messagep, nbytes, defaultErrorMessage, errorNumber);
    }
    return JS_TRUE;

error:
    if (reportp->messageArgs) {
        /* free the arguments only if we allocated them */
        if (charArgs) {
            i = 0;
            while (reportp->messageArgs[i])
                cx->free_((void *)reportp->messageArgs[i++]);
        }
        cx->free_((void *)reportp->messageArgs);
        reportp->messageArgs = NULL;
    }
    if (reportp->ucmessage) {
        cx->free_((void *)reportp->ucmessage);
        reportp->ucmessage = NULL;
    }
    if (*messagep) {
        cx->free_((void *)*messagep);
        *messagep = NULL;
    }
    return JS_FALSE;
}

JSBool
js_ReportErrorNumberVA(JSContext *cx, uintN flags, JSErrorCallback callback,
                       void *userRef, const uintN errorNumber,
                       JSBool charArgs, va_list ap)
{
    JSErrorReport report;
    char *message;
    JSBool warning;

    if (checkReportFlags(cx, &flags))
        return JS_TRUE;
    warning = JSREPORT_IS_WARNING(flags);

    PodZero(&report);
    report.flags = flags;
    report.errorNumber = errorNumber;
    PopulateReportBlame(cx, &report);

    if (!js_ExpandErrorArguments(cx, callback, userRef, errorNumber,
                                 &message, &report, !!charArgs, ap)) {
        return JS_FALSE;
    }

    ReportError(cx, message, &report, callback, userRef);

    if (message)
        cx->free_(message);
    if (report.messageArgs) {
        /*
         * js_ExpandErrorArguments owns its messageArgs only if it had to
         * inflate the arguments (from regular |char *|s).
         */
        if (charArgs) {
            int i = 0;
            while (report.messageArgs[i])
                cx->free_((void *)report.messageArgs[i++]);
        }
        cx->free_((void *)report.messageArgs);
    }
    if (report.ucmessage)
        cx->free_((void *)report.ucmessage);

    return warning;
}

JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    JSErrorReporter onError;

    if (!message)
        return;

    if (cx->lastMessage)
        Foreground::free_(cx->lastMessage);
    cx->lastMessage = JS_strdup(cx, message);
    if (!cx->lastMessage)
        return;
    onError = cx->errorReporter;

    /*
     * If debugErrorHook is present then we give it a chance to veto
     * sending the error on to the regular ErrorReporter.
     */
    if (onError) {
        JSDebugErrorHook hook = cx->debugHooks->debugErrorHook;
        if (hook &&
            !hook(cx, cx->lastMessage, reportp,
                  cx->debugHooks->debugErrorHookData)) {
            onError = NULL;
        }
    }
    if (onError)
        onError(cx, cx->lastMessage, reportp);
}

void
js_ReportIsNotDefined(JSContext *cx, const char *name)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_DEFINED, name);
}

JSBool
js_ReportIsNullOrUndefined(JSContext *cx, intN spindex, const Value &v,
                           JSString *fallback)
{
    char *bytes;
    JSBool ok;

    bytes = DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return JS_FALSE;

    if (strcmp(bytes, js_undefined_str) == 0 ||
        strcmp(bytes, js_null_str) == 0) {
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_NO_PROPERTIES, bytes,
                                          NULL, NULL);
    } else if (v.isUndefined()) {
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNEXPECTED_TYPE, bytes,
                                          js_undefined_str, NULL);
    } else {
        JS_ASSERT(v.isNull());
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNEXPECTED_TYPE, bytes,
                                          js_null_str, NULL);
    }

    cx->free_(bytes);
    return ok;
}

void
js_ReportMissingArg(JSContext *cx, const Value &v, uintN arg)
{
    char argbuf[11];
    char *bytes;
    JSAtom *atom;

    JS_snprintf(argbuf, sizeof argbuf, "%u", arg);
    bytes = NULL;
    if (IsFunctionObject(v)) {
        atom = v.toObject().toFunction()->atom;
        bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK,
                                        v, atom);
        if (!bytes)
            return;
    }
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_MISSING_FUN_ARG, argbuf,
                         bytes ? bytes : "");
    cx->free_(bytes);
}

JSBool
js_ReportValueErrorFlags(JSContext *cx, uintN flags, const uintN errorNumber,
                         intN spindex, const Value &v, JSString *fallback,
                         const char *arg1, const char *arg2)
{
    char *bytes;
    JSBool ok;

    JS_ASSERT(js_ErrorFormatString[errorNumber].argCount >= 1);
    JS_ASSERT(js_ErrorFormatString[errorNumber].argCount <= 3);
    bytes = DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return JS_FALSE;

    ok = JS_ReportErrorFlagsAndNumber(cx, flags, js_GetErrorMessage,
                                      NULL, errorNumber, bytes, arg1, arg2);
    cx->free_(bytes);
    return ok;
}

JSErrorFormatString js_ErrorFormatString[JSErr_Limit] = {
#define MSG_DEF(name, number, count, exception, format) \
    { format, count, exception } ,
#include "js.msg"
#undef MSG_DEF
};

JS_FRIEND_API(const JSErrorFormatString *)
js_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber)
{
    if ((errorNumber > 0) && (errorNumber < JSErr_Limit))
        return &js_ErrorFormatString[errorNumber];
    return NULL;
}

bool
checkOutOfMemory(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    return rt->gcBytes > rt->gcMaxBytes;
}

JSBool
js_InvokeOperationCallback(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    ThreadData *td = JS_THREAD_DATA(cx);

    JS_ASSERT_REQUEST_DEPTH(cx);
    JS_ASSERT(td->interruptFlags != 0);

    /*
     * Reset the callback counter first, then run GC and yield. If another
     * thread is racing us here we will accumulate another callback request
     * which will be serviced at the next opportunity.
     */
    JS_LOCK_GC(rt);
    td->interruptFlags = 0;
#ifdef JS_THREADSAFE
    JS_ATOMIC_DECREMENT(&rt->interruptCounter);
#endif
    JS_UNLOCK_GC(rt);

    if (rt->gcIsNeeded) {
        js_GC(cx, rt->gcTriggerCompartment, GC_NORMAL, rt->gcTriggerReason);

        /*
         * On trace we can exceed the GC quota, see comments in NewGCArena. So
         * we check the quota and report OOM here when we are off trace.
         */
        if (checkOutOfMemory(rt)) {
#ifdef JS_THREADSAFE
            /*
            * We have to wait until the background thread is done in order
            * to get a correct answer.
            */
            {
                AutoLockGC lock(rt);
                rt->gcHelperThread.waitBackgroundSweepEnd();
            }
            if (checkOutOfMemory(rt)) {
                js_ReportOutOfMemory(cx);
                return false;
            }
#else
            js_ReportOutOfMemory(cx);
            return false;
#endif
        }
    }

#ifdef JS_THREADSAFE
    /*
     * We automatically yield the current context every time the operation
     * callback is hit since we might be called as a result of an impending
     * GC on another thread, which would deadlock if we do not yield.
     * Operation callbacks are supposed to happen rarely (seconds, not
     * milliseconds) so it is acceptable to yield at every callback.
     *
     * As the GC can be canceled before it does any request checks we yield
     * even if rt->gcIsNeeded was true above. See bug 590533.
     */
    JS_YieldRequest(cx);
#endif

    JSOperationCallback cb = cx->operationCallback;

    /*
     * Important: Additional callbacks can occur inside the callback handler
     * if it re-enters the JS engine. The embedding must ensure that the
     * callback is disconnected before attempting such re-entry.
     */

    return !cb || cb(cx);
}

JSBool
js_HandleExecutionInterrupt(JSContext *cx)
{
    JSBool result = JS_TRUE;
    if (JS_THREAD_DATA(cx)->interruptFlags)
        result = js_InvokeOperationCallback(cx) && result;
    return result;
}

namespace js {

void
TriggerOperationCallback(JSContext *cx)
{
    /*
     * We allow for cx to come from another thread. Thus we must deal with
     * possible JS_ClearContextThread calls when accessing cx->thread. But we
     * assume that the calling thread is in a request so JSThread cannot be
     * GC-ed.
     */
    ThreadData *td;
#ifdef JS_THREADSAFE
    JSThread *thread = cx->thread();
    if (!thread)
        return;
    td = &thread->data;
#else
    td = JS_THREAD_DATA(cx);
#endif
    td->triggerOperationCallback(cx->runtime);
}

void
TriggerAllOperationCallbacks(JSRuntime *rt)
{
    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->triggerOperationCallback(rt);
}

} /* namespace js */

StackFrame *
js_GetScriptedCaller(JSContext *cx, StackFrame *fp)
{
    if (!fp)
        fp = js_GetTopStackFrame(cx, FRAME_EXPAND_ALL);
    while (fp && fp->isDummyFrame())
        fp = fp->prev();
    JS_ASSERT_IF(fp, fp->isScriptFrame());
    return fp;
}

jsbytecode*
js_GetCurrentBytecodePC(JSContext* cx)
{
    return cx->hasfp() ? cx->regs().pc : NULL;
}

void
DSTOffsetCache::purge()
{
    /*
     * NB: The initial range values are carefully chosen to result in a cache
     *     miss on first use given the range of possible values.  Be careful
     *     to keep these values and the caching algorithm in sync!
     */
    offsetMilliseconds = 0;
    rangeStartSeconds = rangeEndSeconds = INT64_MIN;
    oldOffsetMilliseconds = 0;
    oldRangeStartSeconds = oldRangeEndSeconds = INT64_MIN;

    sanityCheck();
}

/*
 * Since getDSTOffsetMilliseconds guarantees that all times seen will be
 * positive, we can initialize the range at construction time with large
 * negative numbers to ensure the first computation is always a cache miss and
 * doesn't return a bogus offset.
 */
DSTOffsetCache::DSTOffsetCache()
{
    purge();
}

JSContext::JSContext(JSRuntime *rt)
  : defaultVersion(JSVERSION_DEFAULT),
    hasVersionOverride(false),
    throwing(false),
    exception(UndefinedValue()),
    runOptions(0),
    reportGranularity(JS_DEFAULT_JITREPORT_GRANULARITY),
    localeCallbacks(NULL),
    resolvingList(NULL),
    generatingError(false),
#if JS_STACK_GROWTH_DIRECTION > 0
    stackLimit((jsuword)-1),
#else
    stackLimit(0),
#endif
    runtime(rt),
    compartment(NULL),
#ifdef JS_THREADSAFE
    thread_(NULL),
#endif
    stack(thisDuringConstruction()),  /* depends on cx->thread_ */
    parseMapPool_(NULL),
    globalObject(NULL),
    argumentFormatMap(NULL),
    lastMessage(NULL),
    errorReporter(NULL),
    operationCallback(NULL),
    data(NULL),
    data2(NULL),
#ifdef JS_THREADSAFE
    outstandingRequests(0),
#endif
    autoGCRooters(NULL),
    debugHooks(&rt->globalDebugHooks),
    securityCallbacks(NULL),
    resolveFlags(0),
    rngSeed(0),
    iterValue(MagicValue(JS_NO_ITER_VALUE)),
#ifdef JS_METHODJIT
    methodJitEnabled(false),
#endif
    inferenceEnabled(false),
#ifdef MOZ_TRACE_JSCALLS
    functionCallback(NULL),
#endif
    enumerators(NULL),
#ifdef JS_THREADSAFE
    gcBackgroundFree(NULL),
#endif
    activeCompilations(0)
#ifdef DEBUG
    , stackIterAssertionEnabled(true)
#endif
{
    PodZero(&sharpObjectMap);
    PodZero(&link);
#ifdef JS_THREADSAFE
    PodZero(&threadLinks);
#endif
}

JSContext::~JSContext()
{
#ifdef JS_THREADSAFE
    JS_ASSERT(!thread_);
#endif

    /* Free the stuff hanging off of cx. */
    VOUCH_DOES_NOT_REQUIRE_STACK();
    if (parseMapPool_)
        Foreground::delete_<ParseMapPool>(parseMapPool_);

    if (lastMessage)
        Foreground::free_(lastMessage);

    /* Remove any argument formatters. */
    JSArgumentFormatMap *map = argumentFormatMap;
    while (map) {
        JSArgumentFormatMap *temp = map;
        map = map->next;
        Foreground::free_(temp);
    }

    JS_ASSERT(!resolvingList);
}

void
JSContext::resetCompartment()
{
    JSObject *scopeobj;
    if (stack.hasfp()) {
        scopeobj = &fp()->scopeChain();
    } else {
        scopeobj = globalObject;
        if (!scopeobj)
            goto error;

        /*
         * Innerize. Assert, but check anyway, that this succeeds. (It
         * can only fail due to bugs in the engine or embedding.)
         */
        OBJ_TO_INNER_OBJECT(this, scopeobj);
        if (!scopeobj)
            goto error;
    }

    compartment = scopeobj->compartment();
    inferenceEnabled = compartment->types.inferenceEnabled;

    if (isExceptionPending())
        wrapPendingException();
    updateJITEnabled();
    return;

error:

    /*
     * If we try to use the context without a selected compartment,
     * we will crash.
     */
    compartment = NULL;
}

/*
 * Since this function is only called in the context of a pending exception,
 * the caller must subsequently take an error path. If wrapping fails, it will
 * set a new (uncatchable) exception to be used in place of the original.
 */
void
JSContext::wrapPendingException()
{
    Value v = getPendingException();
    clearPendingException();
    if (compartment->wrap(this, &v))
        setPendingException(v);
}

JSGenerator *
JSContext::generatorFor(StackFrame *fp) const
{
    JS_ASSERT(stack.containsSlow(fp));
    JS_ASSERT(fp->isGeneratorFrame());
    JS_ASSERT(!fp->isFloatingGenerator());
    JS_ASSERT(!genStack.empty());

    if (JS_LIKELY(fp == genStack.back()->liveFrame()))
        return genStack.back();

    /* General case; should only be needed for debug APIs. */
    for (size_t i = 0; i < genStack.length(); ++i) {
        if (genStack[i]->liveFrame() == fp)
            return genStack[i];
    }
    JS_NOT_REACHED("no matching generator");
    return NULL;
}

bool
JSContext::runningWithTrustedPrincipals() const
{
    return !compartment || compartment->principals == runtime->trustedPrincipals();
}

JS_FRIEND_API(void)
JSRuntime::onTooMuchMalloc()
{
#ifdef JS_THREADSAFE
    AutoLockGC lock(this);

    /*
     * We can be called outside a request and can race against a GC that
     * mutates the JSThread set during the sweeping phase.
     */
    js_WaitForGC(this);
#endif
    TriggerGC(this, gcstats::TOOMUCHMALLOC);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes, JSContext *cx)
{
    /*
     * Retry when we are done with the background sweeping and have stopped
     * all the allocations and released the empty GC chunks.
     */
    {
#ifdef JS_THREADSAFE
        AutoLockGC lock(this);
        gcHelperThread.waitBackgroundSweepOrAllocEnd();
#endif
        gcChunkPool.expire(this, true);
    }
    if (!p)
        p = OffTheBooks::malloc_(nbytes);
    else if (p == reinterpret_cast<void *>(1))
        p = OffTheBooks::calloc_(nbytes);
    else
      p = OffTheBooks::realloc_(p, nbytes);
    if (p)
        return p;
    if (cx)
        js_ReportOutOfMemory(cx);
    return NULL;
}

void
JSContext::purge()
{
    if (!activeCompilations) {
        Foreground::delete_<ParseMapPool>(parseMapPool_);
        parseMapPool_ = NULL;
    }
}

#if defined(JS_METHODJIT)
static bool
ComputeIsJITBroken()
{
#if !defined(ANDROID) || defined(GONK)
    return false;
#else  // ANDROID
    if (getenv("JS_IGNORE_JIT_BROKENNESS")) {
        return false;
    }

    std::string line;

    // Check for the known-bad kernel version (2.6.29).
    std::ifstream osrelease("/proc/sys/kernel/osrelease");
    std::getline(osrelease, line);
    __android_log_print(ANDROID_LOG_INFO, "Gecko", "Detected osrelease `%s'",
                        line.c_str());

    if (line.npos == line.find("2.6.29")) {
        // We're using something other than 2.6.29, so the JITs should work.
        __android_log_print(ANDROID_LOG_INFO, "Gecko", "JITs are not broken");
        return false;
    }

    // We're using 2.6.29, and this causes trouble with the JITs on i9000.
    line = "";
    bool broken = false;
    std::ifstream cpuinfo("/proc/cpuinfo");
    do {
        if (0 == line.find("Hardware")) {
            const char* blacklist[] = {
                "SCH-I400",     // Samsung Continuum
                "SGH-T959",     // Samsung i9000, Vibrant device
                "SGH-I897",     // Samsung i9000, Captivate device
                "SCH-I500",     // Samsung i9000, Fascinate device
                "SPH-D700",     // Samsung i9000, Epic device
                "GT-I9000",     // Samsung i9000, UK/Europe device
                NULL
            };
            for (const char** hw = &blacklist[0]; *hw; ++hw) {
                if (line.npos != line.find(*hw)) {
                    __android_log_print(ANDROID_LOG_INFO, "Gecko",
                                        "Blacklisted device `%s'", *hw);
                    broken = true;
                    break;
                }
            }
            break;
        }
        std::getline(cpuinfo, line);
    } while(!cpuinfo.fail() && !cpuinfo.eof());

    __android_log_print(ANDROID_LOG_INFO, "Gecko", "JITs are %sbroken",
                        broken ? "" : "not ");

    return broken;
#endif  // ifndef ANDROID
}

static bool
IsJITBrokenHere()
{
    static bool computedIsBroken = false;
    static bool isBroken = false;
    if (!computedIsBroken) {
        isBroken = ComputeIsJITBroken();
        computedIsBroken = true;
    }
    return isBroken;
}
#endif

void
JSContext::updateJITEnabled()
{
#ifdef JS_METHODJIT
    methodJitEnabled = (runOptions & JSOPTION_METHODJIT) &&
                       !IsJITBrokenHere()
# if defined JS_CPU_X86 || defined JS_CPU_X64
                       && JSC::MacroAssemblerX86Common::getSSEState() >=
                          JSC::MacroAssemblerX86Common::HasSSE2
# endif
                        ;
#endif
}

size_t
JSContext::sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const
{
    /*
     * There are other JSContext members that could be measured; the following
     * ones have been found by DMD to be worth measuring.  More stuff may be
     * added later.
     */
    return mallocSizeOf(this, sizeof(JSContext)) +
           busyArrays.sizeOfExcludingThis(mallocSizeOf);
}

namespace js {

AutoEnumStateRooter::~AutoEnumStateRooter()
{
    if (!stateValue.isNull()) {
        DebugOnly<JSBool> ok =
            obj->enumerate(context, JSENUMERATE_DESTROY, &stateValue, 0);
        JS_ASSERT(ok);
    }
}

} /* namespace js */
