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
#include <new>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsprf.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jspubtd.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "jstracer.h"

static void
FreeContext(JSContext *cx);

static void
MarkLocalRoots(JSTracer *trc, JSLocalRootStack *lrs);

void
JSThreadData::init()
{
#ifdef DEBUG
    /* The data must be already zeroed. */
    for (size_t i = 0; i != sizeof(*this); ++i)
        JS_ASSERT(reinterpret_cast<uint8*>(this)[i] == 0);
#endif
#ifdef JS_TRACER
    js_InitJIT(&traceMonitor);
#endif
    js_InitRandom(this);
}

void
JSThreadData::finish()
{
#ifdef DEBUG
    /* All GC-related things must be already removed at this point. */
    JS_ASSERT(gcFreeLists.isEmpty());
    for (size_t i = 0; i != JS_ARRAY_LENGTH(scriptsToGC); ++i)
        JS_ASSERT(!scriptsToGC[i]);
    for (size_t i = 0; i != JS_ARRAY_LENGTH(nativeEnumCache); ++i)
        JS_ASSERT(!nativeEnumCache[i]);
    JS_ASSERT(!localRootStack);
#endif

    js_FinishGSNCache(&gsnCache);
    js_FinishPropertyCache(&propertyCache);
#if defined JS_TRACER
    js_FinishJIT(&traceMonitor);
#endif
}

void
JSThreadData::mark(JSTracer *trc)
{
#ifdef JS_TRACER
    traceMonitor.mark(trc);
#endif
    if (localRootStack)
        MarkLocalRoots(trc, localRootStack);
}

void
JSThreadData::purge(JSContext *cx)
{
    purgeGCFreeLists();

    js_PurgeGSNCache(&gsnCache);

    /* FIXME: bug 506341. */
    js_PurgePropertyCache(cx, &propertyCache);

#ifdef JS_TRACER
    /*
     * If we are about to regenerate shapes, we have to flush the JIT cache,
     * which will eventually abort any current recording.
     */
    if (cx->runtime->gcRegenShapes)
        traceMonitor.needFlush = JS_TRUE;
#endif

    /* Destroy eval'ed scripts. */
    js_DestroyScriptsToGC(cx, this);

    js_PurgeCachedNativeEnumerators(cx, this);
}

void
JSThreadData::purgeGCFreeLists()
{
    if (!localRootStack) {
        gcFreeLists.purge();
    } else {
        JS_ASSERT(gcFreeLists.isEmpty());
        localRootStack->gcFreeLists.purge();
    }
}

#ifdef JS_THREADSAFE

static JSThread *
NewThread(jsword id)
{
    JS_ASSERT(js_CurrentThreadId() == id);
    JSThread *thread = (JSThread *) js_calloc(sizeof(JSThread));
    if (!thread)
        return NULL;
    JS_INIT_CLIST(&thread->contextList);
    thread->id = id;
    thread->data.init();
    return thread;
}

static void
DestroyThread(JSThread *thread)
{
    /* The thread must have zero contexts. */
    JS_ASSERT(JS_CLIST_IS_EMPTY(&thread->contextList));
    JS_ASSERT(!thread->titleToShare);
    thread->data.finish();
    js_free(thread);
}

JSThread *
js_CurrentThread(JSRuntime *rt)
{
    jsword id = js_CurrentThreadId();
    JS_LOCK_GC(rt);

    /*
     * We must not race with a GC that accesses cx->thread for JSContext
     * instances on all threads, see bug 476934.
     */
    js_WaitForGC(rt);
    JSThreadsHashEntry *entry = (JSThreadsHashEntry *)
                                JS_DHashTableOperate(&rt->threads,
                                                     (const void *) id,
                                                     JS_DHASH_LOOKUP);
    JSThread *thread;
    if (JS_DHASH_ENTRY_IS_BUSY(&entry->base)) {
        thread = entry->thread;
        JS_ASSERT(thread->id == id);
    } else {
        JS_UNLOCK_GC(rt);
        thread = NewThread(id);
        if (!thread)
            return NULL;
        JS_LOCK_GC(rt);
        js_WaitForGC(rt);
        entry = (JSThreadsHashEntry *)
                JS_DHashTableOperate(&rt->threads, (const void *) id,
                                     JS_DHASH_ADD);
        if (!entry) {
            JS_UNLOCK_GC(rt);
            DestroyThread(thread);
            return NULL;
        }

        /* Another thread cannot initialize entry->thread. */
        JS_ASSERT(!entry->thread);
        entry->thread = thread;
    }

    return thread;
}

JSBool
js_InitContextThread(JSContext *cx)
{
    JSThread *thread = js_CurrentThread(cx->runtime);
    if (!thread)
        return false;

    JS_APPEND_LINK(&cx->threadLinks, &thread->contextList);
    cx->thread = thread;
    return true;
}

void
js_ClearContextThread(JSContext *cx)
{
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
    JS_REMOVE_AND_INIT_LINK(&cx->threadLinks);
    cx->thread = NULL;
}

static JSBool
thread_matchEntry(JSDHashTable *table,
                  const JSDHashEntryHdr *hdr,
                  const void *key)
{
    const JSThreadsHashEntry *entry = (const JSThreadsHashEntry *) hdr;

    return entry->thread->id == (jsword) key;
}

static const JSDHashTableOps threads_ops = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    JS_DHashVoidPtrKeyStub,
    thread_matchEntry,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

static JSDHashOperator
thread_destroyer(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 /* index */,
                 void * /* arg */)
{
    JSThreadsHashEntry *entry = (JSThreadsHashEntry *) hdr;
    JSThread *thread = entry->thread;

    JS_ASSERT(JS_CLIST_IS_EMPTY(&thread->contextList));
    DestroyThread(thread);
    return JS_DHASH_REMOVE;
}

static JSDHashOperator
thread_purger(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 /* index */,
              void *arg)
{
    JSContext* cx = (JSContext *) arg;
    JSThread *thread = ((JSThreadsHashEntry *) hdr)->thread;

    if (JS_CLIST_IS_EMPTY(&thread->contextList)) {
        JS_ASSERT(cx->thread != thread);
        js_DestroyScriptsToGC(cx, &thread->data);

        /*
         * The following is potentially suboptimal as it also zeros the caches
         * in data, but the code simplicity wins here.
         */
        thread->data.purgeGCFreeLists();
        js_PurgeCachedNativeEnumerators(cx, &thread->data);
        DestroyThread(thread);
        return JS_DHASH_REMOVE;
    }
    thread->data.purge(cx);
    thread->gcThreadMallocBytes = JS_GC_THREAD_MALLOC_LIMIT;
    return JS_DHASH_NEXT;
}

static JSDHashOperator
thread_marker(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 /* index */,
              void *arg)
{
    JSThread *thread = ((JSThreadsHashEntry *) hdr)->thread;
    thread->data.mark((JSTracer *) arg);
    return JS_DHASH_NEXT;
}

#endif /* JS_THREADSAFE */

JSThreadData *
js_CurrentThreadData(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    JSThread *thread = js_CurrentThread(rt);
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
    if (!JS_DHashTableInit(&rt->threads, &threads_ops, NULL,
                           sizeof(JSThreadsHashEntry), 4)) {
        rt->threads.ops = NULL;
        return false;
    }
#else
    rt->threadData.init();
#endif
    return true;
}

void
js_FinishThreads(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    if (!rt->threads.ops)
        return;
    JS_DHashTableEnumerate(&rt->threads, thread_destroyer, NULL);
    JS_DHashTableFinish(&rt->threads);
    rt->threads.ops = NULL;
#else
    rt->threadData.finish();
#endif
}

void
js_PurgeThreads(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_DHashTableEnumerate(&cx->runtime->threads, thread_purger, cx);
#else
    cx->runtime->threadData.purge(cx);
#endif
}

void
js_TraceThreads(JSRuntime *rt, JSTracer *trc)
{
#ifdef JS_THREADSAFE
    JS_DHashTableEnumerate(&rt->threads, thread_marker, trc);
#else
    rt->threadData.mark(trc);
#endif
}

/*
 * JSOPTION_XML and JSOPTION_ANONFUNFIX must be part of the JS version
 * associated with scripts, so in addition to storing them in cx->options we
 * duplicate them in cx->version (script->version, etc.) and ensure each bit
 * remains synchronized between the two through these two functions.
 */
void
js_SyncOptionsToVersion(JSContext* cx)
{
    if (cx->options & JSOPTION_XML)
        cx->version |= JSVERSION_HAS_XML;
    else
        cx->version &= ~JSVERSION_HAS_XML;
    if (cx->options & JSOPTION_ANONFUNFIX)
        cx->version |= JSVERSION_ANONFUNFIX;
    else
        cx->version &= ~JSVERSION_ANONFUNFIX;
}

inline void
js_SyncVersionToOptions(JSContext* cx)
{
    if (cx->version & JSVERSION_HAS_XML)
        cx->options |= JSOPTION_XML;
    else
        cx->options &= ~JSOPTION_XML;
    if (cx->version & JSVERSION_ANONFUNFIX)
        cx->options |= JSOPTION_ANONFUNFIX;
    else
        cx->options &= ~JSOPTION_ANONFUNFIX;
}

void
js_OnVersionChange(JSContext *cx)
{
#ifdef DEBUG
    JSVersion version = JSVERSION_NUMBER(cx);

    JS_ASSERT(version == JSVERSION_DEFAULT || version >= JSVERSION_ECMA_3);
#endif
}

void
js_SetVersion(JSContext *cx, JSVersion version)
{
    cx->version = version;
    js_SyncVersionToOptions(cx);
    js_OnVersionChange(cx);
}

JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    JSContext *cx;
    JSBool ok, first;
    JSContextCallback cxCallback;

    /*
     * We need to initialize the new context fully before adding it to the
     * runtime list. After that it can be accessed from another thread via
     * js_ContextIterator.
     */
    void *mem = js_calloc(sizeof *cx);
    if (!mem)
        return NULL;

    cx = new (mem) JSContext(rt);
    cx->debugHooks = &rt->globalDebugHooks;
#if JS_STACK_GROWTH_DIRECTION > 0
    cx->stackLimit = (jsuword) -1;
#endif
    cx->scriptStackQuota = JS_DEFAULT_SCRIPT_STACK_QUOTA;
    JS_STATIC_ASSERT(JSVERSION_DEFAULT == 0);
    JS_ASSERT(cx->version == JSVERSION_DEFAULT);
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JS_InitArenaPool(&cx->stackPool, "stack", stackChunkSize, sizeof(jsval),
                     &cx->scriptStackQuota);

    JS_InitArenaPool(&cx->tempPool, "temp",
                     1024,  /* FIXME: bug 421435 */
                     sizeof(jsdouble), &cx->scriptStackQuota);

    js_InitRegExpStatics(cx);
    JS_ASSERT(cx->resolveFlags == 0);

    if (!js_InitContextBusyArrayTable(cx)) {
        FreeContext(cx);
        return NULL;
    }

#ifdef JS_THREADSAFE
    if (!js_InitContextThread(cx)) {
        FreeContext(cx);
        return NULL;
    }
#endif

    /*
     * Here the GC lock is still held after js_InitContextThread took it and
     * the GC is not running on another thread.
     */
    for (;;) {
        if (rt->state == JSRTS_UP) {
            JS_ASSERT(!JS_CLIST_IS_EMPTY(&rt->contextList));
            first = JS_FALSE;
            break;
        }
        if (rt->state == JSRTS_DOWN) {
            JS_ASSERT(JS_CLIST_IS_EMPTY(&rt->contextList));
            first = JS_TRUE;
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
        ok = js_InitCommonAtoms(cx);

        /*
         * scriptFilenameTable may be left over from a previous episode of
         * non-zero contexts alive in rt, so don't re-init the table if it's
         * not necessary.
         */
        if (ok && !rt->scriptFilenameTable)
            ok = js_InitRuntimeScriptState(rt);
        if (ok)
            ok = js_InitRuntimeNumberState(cx);
        if (ok)
            ok = js_InitRuntimeStringState(cx);
#ifdef JS_THREADSAFE
        JS_EndRequest(cx);
#endif
        if (!ok) {
            js_DestroyContext(cx, JSDCM_NEW_FAILED);
            return NULL;
        }

        JS_LOCK_GC(rt);
        rt->state = JSRTS_UP;
        JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
        JS_UNLOCK_GC(rt);
    }

    cxCallback = rt->cxCallback;
    if (cxCallback && !cxCallback(cx, JSCONTEXT_NEW)) {
        js_DestroyContext(cx, JSDCM_NEW_FAILED);
        return NULL;
    }

    return cx;
}

#if defined DEBUG && defined XP_UNIX
# include <stdio.h>

class JSAutoFile {
public:
    JSAutoFile() : mFile(NULL) {}

    ~JSAutoFile() {
        if (mFile)
            fclose(mFile);
    }

    FILE *open(const char *fname, const char *mode) {
        return mFile = fopen(fname, mode);
    }
    operator FILE *() {
        return mFile;
    }

private:
    FILE *mFile;
};

#ifdef JS_EVAL_CACHE_METERING
static void
DumpEvalCacheMeter(JSContext *cx)
{
    struct {
        const char *name;
        ptrdiff_t  offset;
    } table[] = {
#define frob(x) { #x, offsetof(JSEvalCacheMeter, x) }
        EVAL_CACHE_METER_LIST(frob)
#undef frob
    };
    JSEvalCacheMeter *ecm = &JS_THREAD_DATA(cx)->evalCacheMeter;

    static JSAutoFile fp;
    if (!fp) {
        fp.open("/tmp/evalcache.stats", "w");
        if (!fp)
            return;
    }

    fprintf(fp, "eval cache meter (%p):\n",
#ifdef JS_THREADSAFE
            (void *) cx->thread
#else
            (void *) cx->runtime
#endif
            );
    for (uintN i = 0; i < JS_ARRAY_LENGTH(table); ++i) {
        fprintf(fp, "%-8.8s  %llu\n",
                table[i].name,
                (unsigned long long int) *(uint64 *)((uint8 *)ecm + table[i].offset));
    }
    fprintf(fp, "hit ratio %g%%\n", ecm->hit * 100. / ecm->probe);
    fprintf(fp, "avg steps %g\n", double(ecm->step) / ecm->probe);
    fflush(fp);
}
# define DUMP_EVAL_CACHE_METER(cx) DumpEvalCacheMeter(cx)
#endif

#ifdef JS_FUNCTION_METERING
static void
DumpFunctionMeter(JSContext *cx)
{
    struct {
        const char *name;
        ptrdiff_t  offset;
    } table[] = {
#define frob(x) { #x, offsetof(JSFunctionMeter, x) }
        FUNCTION_KIND_METER_LIST(frob)
#undef frob
    };
    JSFunctionMeter *fm = &cx->runtime->functionMeter;

    static JSAutoFile fp;
    if (!fp) {
        fp.open("/tmp/function.stats", "a");
        if (!fp)
            return;
    }

    fprintf(fp, "function meter (%s):\n", cx->runtime->lastScriptFilename);
    for (uintN i = 0; i < JS_ARRAY_LENGTH(table); ++i) {
        fprintf(fp, "%-11.11s %d\n",
                table[i].name, *(int32 *)((uint8 *)fm + table[i].offset));
    }
    fflush(fp);
}
# define DUMP_FUNCTION_METER(cx)   DumpFunctionMeter(cx)
#endif

#endif /* DEBUG && XP_UNIX */

#ifndef DUMP_EVAL_CACHE_METER
# define DUMP_EVAL_CACHE_METER(cx) ((void) 0)
#endif

#ifndef DUMP_FUNCTION_METER
# define DUMP_FUNCTION_METER(cx)   ((void) 0)
#endif

void
js_DestroyContext(JSContext *cx, JSDestroyContextMode mode)
{
    JSRuntime *rt;
    JSContextCallback cxCallback;
    JSBool last;

    rt = cx->runtime;
#ifdef JS_THREADSAFE
    /*
     * For API compatibility we allow to destroy contexts without a thread in
     * optimized builds. We assume that the embedding knows that an OOM error
     * cannot happen in JS_SetContextThread.
     */
    JS_ASSERT(cx->thread && CURRENT_THREAD_IS_ME(cx->thread));
    if (!cx->thread)
        JS_SetContextThread(cx);

    JS_ASSERT_IF(rt->gcRunning, cx->outstandingRequests == 0);
#endif

    if (mode != JSDCM_NEW_FAILED) {
        cxCallback = rt->cxCallback;
        if (cxCallback) {
            /*
             * JSCONTEXT_DESTROY callback is not allowed to fail and must
             * return true.
             */
#ifdef DEBUG
            JSBool callbackStatus =
#endif
            cxCallback(cx, JSCONTEXT_DESTROY);
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
    if (cx->requestDepth == 0)
        js_WaitForGC(rt);
#endif
    JS_REMOVE_LINK(&cx->link);
    last = (rt->contextList.next == &rt->contextList);
    if (last)
        rt->state = JSRTS_LANDING;
    if (last || mode == JSDCM_FORCE_GC || mode == JSDCM_MAYBE_GC
#ifdef JS_THREADSAFE
        || cx->requestDepth != 0
#endif
        ) {
        JS_ASSERT(!rt->gcRunning);

        JS_UNLOCK_GC(rt);

        if (last) {
#ifdef JS_THREADSAFE
            /*
             * If cx is not in a request already, begin one now so that we wait
             * for any racing GC started on a not-last context to finish, before
             * we plow ahead and unpin atoms.  Note that even though we begin a
             * request here if necessary, we end all requests on cx below before
             * forcing a final GC.  This lets any not-last context destruction
             * racing in another thread try to force or maybe run the GC, but by
             * that point, rt->state will not be JSRTS_UP, and that GC attempt
             * will return early.
             */
            if (cx->requestDepth == 0)
                JS_BeginRequest(cx);
#endif

            /* Unlock and clear GC things held by runtime pointers. */
            js_FinishRuntimeNumberState(cx);
            js_FinishRuntimeStringState(cx);

            /* Unpin all common atoms before final GC. */
            js_FinishCommonAtoms(cx);

            /* Clear debugging state to remove GC roots. */
            JS_ClearAllTraps(cx);
            JS_ClearAllWatchPoints(cx);
        }

        /* Remove more GC roots in regExpStatics, then collect garbage. */
        JS_ClearRegExpRoots(cx);

#ifdef JS_THREADSAFE
        /*
         * Destroying a context implicitly calls JS_EndRequest().  Also, we must
         * end our request here in case we are "last" -- in that event, another
         * js_DestroyContext that was not last might be waiting in the GC for our
         * request to end.  We'll let it run below, just before we do the truly
         * final GC and then free atom state.
         */
        while (cx->requestDepth != 0)
            JS_EndRequest(cx);
#endif

        if (last) {
            js_GC(cx, GC_LAST_CONTEXT);
            DUMP_EVAL_CACHE_METER(cx);
            DUMP_FUNCTION_METER(cx);

            /* Take the runtime down, now that it has no contexts or atoms. */
            JS_LOCK_GC(rt);
            rt->state = JSRTS_DOWN;
            JS_NOTIFY_ALL_CONDVAR(rt->stateChange);
        } else {
            if (mode == JSDCM_FORCE_GC)
                js_GC(cx, GC_NORMAL);
            else if (mode == JSDCM_MAYBE_GC)
                JS_MaybeGC(cx);
            JS_LOCK_GC(rt);
            js_WaitForGC(rt);
        }
    }
#ifdef JS_THREADSAFE
    js_ClearContextThread(cx);
#endif
    JS_UNLOCK_GC(rt);
    FreeContext(cx);
}

static void
FreeContext(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(!cx->thread);
#endif

    /* Free the stuff hanging off of cx. */
    js_FreeRegExpStatics(cx);
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JS_FinishArenaPool(&cx->stackPool);
    JS_FinishArenaPool(&cx->tempPool);

    if (cx->lastMessage)
        js_free(cx->lastMessage);

    /* Remove any argument formatters. */
    JSArgumentFormatMap *map = cx->argumentFormatMap;
    while (map) {
        JSArgumentFormatMap *temp = map;
        map = map->next;
        cx->free(temp);
    }

    /* Destroy the busy array table. */
    if (cx->busyArrayTable) {
        JS_HashTableDestroy(cx->busyArrayTable);
        cx->busyArrayTable = NULL;
    }

    /* Destroy the resolve recursion damper. */
    if (cx->resolvingTable) {
        JS_DHashTableDestroy(cx->resolvingTable);
        cx->resolvingTable = NULL;
    }

    /* Finally, free cx itself. */
    js_free(cx);
}

JSBool
js_ValidContextPointer(JSRuntime *rt, JSContext *cx)
{
    JSCList *cl;

    for (cl = rt->contextList.next; cl != &rt->contextList; cl = cl->next) {
        if (cl == &cx->link)
            return JS_TRUE;
    }
    JS_RUNTIME_METER(rt, deadContexts);
    return JS_FALSE;
}

JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp)
{
    JSContext *cx = *iterp;

    if (unlocked)
        JS_LOCK_GC(rt);
    cx = js_ContextFromLinkField(cx ? cx->link.next : rt->contextList.next);
    if (&cx->link == &rt->contextList)
        cx = NULL;
    *iterp = cx;
    if (unlocked)
        JS_UNLOCK_GC(rt);
    return cx;
}

JS_FRIEND_API(JSContext *)
js_NextActiveContext(JSRuntime *rt, JSContext *cx)
{
    JSContext *iter = cx;
#ifdef JS_THREADSAFE
    while ((cx = js_ContextIterator(rt, JS_FALSE, &iter)) != NULL) {
        if (cx->requestDepth)
            break;
    }
    return cx;
#else
    return js_ContextIterator(rt, JS_FALSE, &iter);
#endif
}

#ifdef JS_THREADSAFE

uint32
js_CountThreadRequests(JSContext *cx)
{
    JSCList *head, *link;
    uint32 nrequests;

    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
    head = &cx->thread->contextList;
    nrequests = 0;
    for (link = head->next; link != head; link = link->next) {
        JSContext *acx = CX_FROM_THREAD_LINKS(link);
        JS_ASSERT(acx->thread == cx->thread);
        if (acx->requestDepth)
            nrequests++;
    }
    return nrequests;
}

/*
 * If the GC is running and we're called on another thread, wait for this GC
 * activation to finish. We can safely wait here without fear of deadlock (in
 * the case where we are called within a request on another thread's context)
 * because the GC doesn't set rt->gcRunning until after it has waited for all
 * active requests to end.
 *
 * We call here js_CurrentThreadId() after checking for rt->gcRunning to avoid
 * expensive calls when the GC is not running.
 */
void
js_WaitForGC(JSRuntime *rt)
{
    JS_ASSERT_IF(rt->gcRunning, rt->gcLevel > 0);
    if (rt->gcRunning && rt->gcThread->id != js_CurrentThreadId()) {
        do {
            JS_AWAIT_GC_DONE(rt);
        } while (rt->gcRunning);
    }
}

uint32
js_DiscountRequestsForGC(JSContext *cx)
{
    uint32 requestDebit;

    JS_ASSERT(cx->thread);
    JS_ASSERT(cx->runtime->gcThread != cx->thread);

#ifdef JS_TRACER
    if (JS_ON_TRACE(cx)) {
        JS_UNLOCK_GC(cx->runtime);
        js_LeaveTrace(cx);
        JS_LOCK_GC(cx->runtime);
    }
#endif

    requestDebit = js_CountThreadRequests(cx);
    if (requestDebit != 0) {
        JSRuntime *rt = cx->runtime;
        JS_ASSERT(requestDebit <= rt->requestCount);
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);
    }
    return requestDebit;
}

void
js_RecountRequestsAfterGC(JSRuntime *rt, uint32 requestDebit)
{
    while (rt->gcLevel > 0) {
        JS_ASSERT(rt->gcThread);
        JS_AWAIT_GC_DONE(rt);
    }
    if (requestDebit != 0)
        rt->requestCount += requestDebit;
}

#endif

static JSDHashNumber
resolving_HashKey(JSDHashTable *table, const void *ptr)
{
    const JSResolvingKey *key = (const JSResolvingKey *)ptr;

    return ((JSDHashNumber)JS_PTR_TO_UINT32(key->obj) >> JSVAL_TAGBITS) ^ key->id;
}

JS_PUBLIC_API(JSBool)
resolving_MatchEntry(JSDHashTable *table,
                     const JSDHashEntryHdr *hdr,
                     const void *ptr)
{
    const JSResolvingEntry *entry = (const JSResolvingEntry *)hdr;
    const JSResolvingKey *key = (const JSResolvingKey *)ptr;

    return entry->key.obj == key->obj && entry->key.id == key->id;
}

static const JSDHashTableOps resolving_dhash_ops = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    resolving_HashKey,
    resolving_MatchEntry,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub,
    NULL
};

JSBool
js_StartResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                  JSResolvingEntry **entryp)
{
    JSDHashTable *table;
    JSResolvingEntry *entry;

    table = cx->resolvingTable;
    if (!table) {
        table = JS_NewDHashTable(&resolving_dhash_ops, NULL,
                                 sizeof(JSResolvingEntry),
                                 JS_DHASH_MIN_SIZE);
        if (!table)
            goto outofmem;
        cx->resolvingTable = table;
    }

    entry = (JSResolvingEntry *)
            JS_DHashTableOperate(table, key, JS_DHASH_ADD);
    if (!entry)
        goto outofmem;

    if (entry->flags & flag) {
        /* An entry for (key, flag) exists already -- dampen recursion. */
        entry = NULL;
    } else {
        /* Fill in key if we were the first to add entry, then set flag. */
        if (!entry->key.obj)
            entry->key = *key;
        entry->flags |= flag;
    }
    *entryp = entry;
    return JS_TRUE;

outofmem:
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
}

void
js_StopResolving(JSContext *cx, JSResolvingKey *key, uint32 flag,
                 JSResolvingEntry *entry, uint32 generation)
{
    JSDHashTable *table;

    /*
     * Clear flag from entry->flags and return early if other flags remain.
     * We must take care to re-lookup entry if the table has changed since
     * it was found by js_StartResolving.
     */
    table = cx->resolvingTable;
    if (!entry || table->generation != generation) {
        entry = (JSResolvingEntry *)
                JS_DHashTableOperate(table, key, JS_DHASH_LOOKUP);
    }
    JS_ASSERT(JS_DHASH_ENTRY_IS_BUSY(&entry->hdr));
    entry->flags &= ~flag;
    if (entry->flags)
        return;

    /*
     * Do a raw remove only if fewer entries were removed than would cause
     * alpha to be less than .5 (alpha is at most .75).  Otherwise, we just
     * call JS_DHashTableOperate to re-lookup the key and remove its entry,
     * compressing or shrinking the table as needed.
     */
    if (table->removedCount < JS_DHASH_TABLE_SIZE(table) >> 2)
        JS_DHashTableRawRemove(table, &entry->hdr);
    else
        JS_DHashTableOperate(table, key, JS_DHASH_REMOVE);
}

JSBool
js_EnterLocalRootScope(JSContext *cx)
{
    JSThreadData *td = JS_THREAD_DATA(cx);
    JSLocalRootStack *lrs = td->localRootStack;
    if (!lrs) {
        lrs = (JSLocalRootStack *) js_malloc(sizeof *lrs);
        if (!lrs) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        lrs->scopeMark = JSLRS_NULL_MARK;
        lrs->rootCount = 0;
        lrs->topChunk = &lrs->firstChunk;
        lrs->firstChunk.down = NULL;
        td->gcFreeLists.moveTo(&lrs->gcFreeLists);
        td->localRootStack = lrs;
    }

    /* Push lrs->scopeMark to save it for restore when leaving. */
    int mark = js_PushLocalRoot(cx, lrs, INT_TO_JSVAL(lrs->scopeMark));
    if (mark < 0)
        return JS_FALSE;
    lrs->scopeMark = (uint32) mark;
    return true;
}

void
js_LeaveLocalRootScopeWithResult(JSContext *cx, jsval rval)
{
    JSLocalRootStack *lrs;
    uint32 mark, m, n;
    JSLocalRootChunk *lrc;

    /* Defend against buggy native callers. */
    lrs = JS_THREAD_DATA(cx)->localRootStack;
    JS_ASSERT(lrs && lrs->rootCount != 0);
    if (!lrs || lrs->rootCount == 0)
        return;

    mark = lrs->scopeMark;
    JS_ASSERT(mark != JSLRS_NULL_MARK);
    if (mark == JSLRS_NULL_MARK)
        return;

    /* Free any chunks being popped by this leave operation. */
    m = mark >> JSLRS_CHUNK_SHIFT;
    n = (lrs->rootCount - 1) >> JSLRS_CHUNK_SHIFT;
    while (n > m) {
        lrc = lrs->topChunk;
        JS_ASSERT(lrc != &lrs->firstChunk);
        lrs->topChunk = lrc->down;
        js_free(lrc);
        --n;
    }

    /*
     * Pop the scope, restoring lrs->scopeMark.  If rval is a GC-thing, push
     * it on the caller's scope, or store it in lastInternalResult if we are
     * leaving the outermost scope.  We don't need to allocate a new lrc
     * because we can overwrite the old mark's slot with rval.
     */
    lrc = lrs->topChunk;
    m = mark & JSLRS_CHUNK_MASK;
    lrs->scopeMark = (uint32) JSVAL_TO_INT(lrc->roots[m]);
    if (JSVAL_IS_GCTHING(rval) && !JSVAL_IS_NULL(rval)) {
        if (mark == 0) {
            cx->weakRoots.lastInternalResult = rval;
        } else {
            /*
             * Increment m to avoid the "else if (m == 0)" case below.  If
             * rval is not a GC-thing, that case would take care of freeing
             * any chunk that contained only the old mark.  Since rval *is*
             * a GC-thing here, we want to reuse that old mark's slot.
             */
            lrc->roots[m++] = rval;
            ++mark;
        }
    }
    lrs->rootCount = (uint32) mark;

    /*
     * Free the stack eagerly, risking malloc churn.  The alternative would
     * require an lrs->entryCount member, maintained by Enter and Leave, and
     * tested by the GC in addition to the cx->localRootStack non-null test.
     *
     * That approach would risk hoarding 264 bytes (net) per context.  Right
     * now it seems better to give fresh (dirty in CPU write-back cache, and
     * the data is no longer needed) memory back to the malloc heap.
     */
    if (mark == 0) {
        JSThreadData *td = JS_THREAD_DATA(cx);
        JS_ASSERT(td->gcFreeLists.isEmpty());
        lrs->gcFreeLists.moveTo(&td->gcFreeLists);
        td->localRootStack = NULL;
        js_free(lrs);
    } else if (m == 0) {
        lrs->topChunk = lrc->down;
        js_free(lrc);
    }
}

void
js_ForgetLocalRoot(JSContext *cx, jsval v)
{
    JSLocalRootStack *lrs;
    uint32 i, j, m, n, mark;
    JSLocalRootChunk *lrc, *lrc2;
    jsval top;

    lrs = JS_THREAD_DATA(cx)->localRootStack;
    JS_ASSERT(lrs && lrs->rootCount);
    if (!lrs || lrs->rootCount == 0)
        return;

    /* Prepare to pop the top-most value from the stack. */
    n = lrs->rootCount - 1;
    m = n & JSLRS_CHUNK_MASK;
    lrc = lrs->topChunk;
    top = lrc->roots[m];

    /* Be paranoid about calls on an empty scope. */
    mark = lrs->scopeMark;
    JS_ASSERT(mark < n);
    if (mark >= n)
        return;

    /* If v was not the last root pushed in the top scope, find it. */
    if (top != v) {
        /* Search downward in case v was recently pushed. */
        i = n;
        j = m;
        lrc2 = lrc;
        while (--i > mark) {
            if (j == 0)
                lrc2 = lrc2->down;
            j = i & JSLRS_CHUNK_MASK;
            if (lrc2->roots[j] == v)
                break;
        }

        /* If we didn't find v in this scope, assert and bail out. */
        JS_ASSERT(i != mark);
        if (i == mark)
            return;

        /* Swap top and v so common tail code can pop v. */
        lrc2->roots[j] = top;
    }

    /* Pop the last value from the stack. */
    lrc->roots[m] = JSVAL_NULL;
    lrs->rootCount = n;
    if (m == 0) {
        JS_ASSERT(n != 0);
        JS_ASSERT(lrc != &lrs->firstChunk);
        lrs->topChunk = lrc->down;
        cx->free(lrc);
    }
}

int
js_PushLocalRoot(JSContext *cx, JSLocalRootStack *lrs, jsval v)
{
    uint32 n, m;
    JSLocalRootChunk *lrc;

    n = lrs->rootCount;
    m = n & JSLRS_CHUNK_MASK;
    if (n == 0 || m != 0) {
        /*
         * At start of first chunk, or not at start of a non-first top chunk.
         * Check for lrs->rootCount overflow.
         */
        if ((uint32)(n + 1) == 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_TOO_MANY_LOCAL_ROOTS);
            return -1;
        }
        lrc = lrs->topChunk;
        JS_ASSERT(n != 0 || lrc == &lrs->firstChunk);
    } else {
        /*
         * After lrs->firstChunk, trying to index at a power-of-two chunk
         * boundary: need a new chunk.
         */
        lrc = (JSLocalRootChunk *) js_malloc(sizeof *lrc);
        if (!lrc) {
            js_ReportOutOfMemory(cx);
            return -1;
        }
        lrc->down = lrs->topChunk;
        lrs->topChunk = lrc;
    }
    lrs->rootCount = n + 1;
    lrc->roots[m] = v;
    return (int) n;
}

static void
MarkLocalRoots(JSTracer *trc, JSLocalRootStack *lrs)
{
    uint32 n, m, mark;
    JSLocalRootChunk *lrc;
    jsval v;

    n = lrs->rootCount;
    if (n == 0)
        return;

    mark = lrs->scopeMark;
    lrc = lrs->topChunk;
    do {
        while (--n > mark) {
            m = n & JSLRS_CHUNK_MASK;
            v = lrc->roots[m];
            JS_ASSERT(JSVAL_IS_GCTHING(v) && v != JSVAL_NULL);
            JS_SET_TRACING_INDEX(trc, "local_root", n);
            js_CallValueTracerIfGCThing(trc, v);
            if (m == 0)
                lrc = lrc->down;
        }
        m = n & JSLRS_CHUNK_MASK;
        mark = JSVAL_TO_INT(lrc->roots[m]);
        if (m == 0)
            lrc = lrc->down;
    } while (n != 0);
    JS_ASSERT(!lrc);
}

static void
ReportError(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    /*
     * Check the error report, and set a JavaScript-catchable exception
     * if the error is defined to have an associated exception.  If an
     * exception is thrown, then the JSREPORT_EXCEPTION flag will be set
     * on the error report, and exception-aware hosts should ignore it.
     */
    JS_ASSERT(reportp);
    if (reportp->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)
        reportp->flags |= JSREPORT_EXCEPTION;

    /*
     * Call the error reporter only if an exception wasn't raised.
     *
     * If an exception was raised, then we call the debugErrorHook
     * (if present) to give it a chance to see the error before it
     * propagates out of scope.  This is needed for compatability
     * with the old scheme.
     */
    if (!JS_IsRunning(cx) || !js_ErrorToException(cx, message, reportp)) {
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
    JSStackFrame *fp;

    /*
     * Walk stack until we find a frame that is associated with some script
     * rather than a native frame.
     */
    for (fp = js_GetTopStackFrame(cx); fp; fp = fp->down) {
        if (fp->regs) {
            report->filename = fp->script->filename;
            report->lineno = js_FramePCToLineNumber(cx, fp);
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
#ifdef JS_TRACER
    /*
     * If we are in a builtin called directly from trace, don't report an
     * error. We will retry in the interpreter instead.
     */
    if (JS_ON_TRACE(cx) && !cx->bailExit)
        return;
#endif

    JSErrorReport report;
    JSErrorReporter onError = cx->errorReporter;

    /* Get the message for this error, but we won't expand any arguments. */
    const JSErrorFormatString *efs =
        js_GetLocalizedErrorMessage(cx, NULL, NULL, JSMSG_OUT_OF_MEMORY);
    const char *msg = efs ? efs->format : "Out of memory";

    /* Fill out the report, but don't do anything that requires allocation. */
    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = JSREPORT_ERROR;
    report.errorNumber = JSMSG_OUT_OF_MEMORY;
    PopulateReportBlame(cx, &report);

    /*
     * If debugErrorHook is present then we give it a chance to veto sending
     * the error on to the regular ErrorReporter. We also clear a pending
     * exception if any now so the hooks can replace the out-of-memory error
     * by a script-catchable exception.
     */
    cx->throwing = JS_FALSE;
    if (onError) {
        JSDebugErrorHook hook = cx->debugHooks->debugErrorHook;
        if (hook &&
            !hook(cx, msg, &report, cx->debugHooks->debugErrorHookData)) {
            onError = NULL;
        }
    }

    if (onError)
        onError(cx, msg, &report);
}

void
js_ReportOutOfScriptQuota(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_SCRIPT_STACK_QUOTA);
}

void
js_ReportOverRecursed(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_OVER_RECURSED);
}

void
js_ReportAllocationOverflow(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ALLOC_OVERFLOW);
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
        /* Error in strict code; warning with strict option; okay otherwise. */
        JS_ASSERT(JS_IsRunning(cx));
        if (js_GetTopStackFrame(cx)->script->strictModeCode)
            *flags &= ~JSREPORT_WARNING;
        else if (JS_HAS_STRICT_OPTION(cx))
            *flags |= JSREPORT_WARNING;
        else
            return true;
    } else if (JSREPORT_IS_STRICT(*flags)) {
        /* Warning/error only when JSOPTION_STRICT is set. */
        if (!JS_HAS_STRICT_OPTION(cx))
            return true;
    }

    /* Warnings become errors when JSOPTION_WERROR is set. */
    if (JSREPORT_IS_WARNING(*flags) && JS_HAS_WERROR_OPTION(cx))
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

    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = flags;
    report.errorNumber = JSMSG_USER_DEFINED_ERROR;
    report.ucmessage = ucmessage = js_InflateString(cx, message, &messagelen);
    PopulateReportBlame(cx, &report);

    warning = JSREPORT_IS_WARNING(report.flags);

    ReportError(cx, message, &report);
    js_free(message);
    cx->free(ucmessage);
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
                cx->malloc(sizeof(jschar *) * (argCount + 1));
            if (!reportp->messageArgs)
                return JS_FALSE;
            reportp->messageArgs[argCount] = NULL;
            for (i = 0; i < argCount; i++) {
                if (charArgs) {
                    char *charArg = va_arg(ap, char *);
                    size_t charArgLength = strlen(charArg);
                    reportp->messageArgs[i]
                        = js_InflateString(cx, charArg, &charArgLength);
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

                buffer = fmt = js_InflateString (cx, efs->format, &len);
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
                    cx->malloc((expandedLength + 1) * sizeof(jschar));
                if (!out) {
                    cx->free(buffer);
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
                cx->free(buffer);
                *messagep =
                    js_DeflateString(cx, reportp->ucmessage,
                                     (size_t)(out - reportp->ucmessage));
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
                reportp->ucmessage = js_InflateString(cx, *messagep, &len);
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
        *messagep = (char *)cx->malloc(nbytes);
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
                cx->free((void *)reportp->messageArgs[i++]);
        }
        cx->free((void *)reportp->messageArgs);
        reportp->messageArgs = NULL;
    }
    if (reportp->ucmessage) {
        cx->free((void *)reportp->ucmessage);
        reportp->ucmessage = NULL;
    }
    if (*messagep) {
        cx->free((void *)*messagep);
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

    memset(&report, 0, sizeof (struct JSErrorReport));
    report.flags = flags;
    report.errorNumber = errorNumber;
    PopulateReportBlame(cx, &report);

    if (!js_ExpandErrorArguments(cx, callback, userRef, errorNumber,
                                 &message, &report, charArgs, ap)) {
        return JS_FALSE;
    }

    ReportError(cx, message, &report);

    if (message)
        cx->free(message);
    if (report.messageArgs) {
        /*
         * js_ExpandErrorArguments owns its messageArgs only if it had to
         * inflate the arguments (from regular |char *|s).
         */
        if (charArgs) {
            int i = 0;
            while (report.messageArgs[i])
                cx->free((void *)report.messageArgs[i++]);
        }
        cx->free((void *)report.messageArgs);
    }
    if (report.ucmessage)
        cx->free((void *)report.ucmessage);

    return warning;
}

JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    JSErrorReporter onError;

    if (!message)
        return;

    if (cx->lastMessage)
        js_free(cx->lastMessage);
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
js_ReportIsNullOrUndefined(JSContext *cx, intN spindex, jsval v,
                           JSString *fallback)
{
    char *bytes;
    JSBool ok;

    bytes = js_DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return JS_FALSE;

    if (strcmp(bytes, js_undefined_str) == 0 ||
        strcmp(bytes, js_null_str) == 0) {
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_NO_PROPERTIES, bytes,
                                          NULL, NULL);
    } else if (JSVAL_IS_VOID(v)) {
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNEXPECTED_TYPE, bytes,
                                          js_undefined_str, NULL);
    } else {
        JS_ASSERT(JSVAL_IS_NULL(v));
        ok = JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_UNEXPECTED_TYPE, bytes,
                                          js_null_str, NULL);
    }

    cx->free(bytes);
    return ok;
}

void
js_ReportMissingArg(JSContext *cx, jsval *vp, uintN arg)
{
    char argbuf[11];
    char *bytes;
    JSAtom *atom;

    JS_snprintf(argbuf, sizeof argbuf, "%u", arg);
    bytes = NULL;
    if (VALUE_IS_FUNCTION(cx, *vp)) {
        atom = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(*vp))->atom;
        bytes = js_DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, *vp,
                                           ATOM_TO_STRING(atom));
        if (!bytes)
            return;
    }
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_MISSING_FUN_ARG, argbuf,
                         bytes ? bytes : "");
    cx->free(bytes);
}

JSBool
js_ReportValueErrorFlags(JSContext *cx, uintN flags, const uintN errorNumber,
                         intN spindex, jsval v, JSString *fallback,
                         const char *arg1, const char *arg2)
{
    char *bytes;
    JSBool ok;

    JS_ASSERT(js_ErrorFormatString[errorNumber].argCount >= 1);
    JS_ASSERT(js_ErrorFormatString[errorNumber].argCount <= 3);
    bytes = js_DecompileValueGenerator(cx, spindex, v, fallback);
    if (!bytes)
        return JS_FALSE;

    ok = JS_ReportErrorFlagsAndNumber(cx, flags, js_GetErrorMessage,
                                      NULL, errorNumber, bytes, arg1, arg2);
    cx->free(bytes);
    return ok;
}

#if defined DEBUG && defined XP_UNIX
/* For gdb usage. */
void js_traceon(JSContext *cx)  { cx->tracefp = stderr; cx->tracePrevPc = NULL; }
void js_traceoff(JSContext *cx) { cx->tracefp = NULL; }
#endif

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

JSBool
js_InvokeOperationCallback(JSContext *cx)
{
    JS_ASSERT(cx->operationCallbackFlag);

    /*
     * Reset the callback flag first, then yield. If another thread is racing
     * us here we will accumulate another callback request which will be
     * serviced at the next opportunity.
     */
    cx->operationCallbackFlag = 0;

    /*
     * Unless we are going to run the GC, we automatically yield the current
     * context every time the operation callback is hit since we might be
     * called as a result of an impending GC, which would deadlock if we do
     * not yield. Operation callbacks are supposed to happen rarely (seconds,
     * not milliseconds) so it is acceptable to yield at every callback.
     */
    if (cx->runtime->gcIsNeeded)
        js_GC(cx, GC_NORMAL);
#ifdef JS_THREADSAFE
    else
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

void
js_TriggerAllOperationCallbacks(JSRuntime *rt, JSBool gcLocked)
{
    JSContext *acx, *iter;
#ifdef JS_THREADSAFE
    if (!gcLocked)
        JS_LOCK_GC(rt);
#endif
    iter = NULL;
    while ((acx = js_ContextIterator(rt, JS_FALSE, &iter)))
        JS_TriggerOperationCallback(acx);
#ifdef JS_THREADSAFE
    if (!gcLocked)
        JS_UNLOCK_GC(rt);
#endif
}

JSStackFrame *
js_GetScriptedCaller(JSContext *cx, JSStackFrame *fp)
{
    if (!fp)
        fp = js_GetTopStackFrame(cx);
    while (fp) {
        if (fp->script)
            return fp;
        fp = fp->down;
    }
    return NULL;
}

jsbytecode*
js_GetCurrentBytecodePC(JSContext* cx)
{
    jsbytecode *pc, *imacpc;

#ifdef JS_TRACER
    if (JS_ON_TRACE(cx)) {
        pc = cx->bailExit->pc;
        imacpc = cx->bailExit->imacpc;
    } else
#endif
    {
        JS_ASSERT_NOT_ON_TRACE(cx);  /* for static analysis */
        JSStackFrame* fp = cx->fp;
        if (fp && fp->regs) {
            pc = fp->regs->pc;
            imacpc = fp->imacpc;
        } else {
            return NULL;
        }
    }

    /*
     * If we are inside GetProperty_tn or similar, return a pointer to the
     * current instruction in the script, not the CALL instruction in the
     * imacro, for the benefit of callers doing bytecode inspection.
     */
    return (*pc == JSOP_CALL && imacpc) ? imacpc : pc;
}

bool
js_CurrentPCIsInImacro(JSContext *cx)
{
#ifdef JS_TRACER
    VOUCH_DOES_NOT_REQUIRE_STACK();
    return (JS_ON_TRACE(cx) ? cx->bailExit->imacpc : cx->fp->imacpc) != NULL;
#else
    return false;
#endif
}

void
JSContext::checkMallocGCPressure(void *p)
{
    if (!p) {
        js_ReportOutOfMemory(this);
        return;
    }

#ifdef JS_THREADSAFE
    JS_ASSERT(thread->gcThreadMallocBytes <= 0);
    ptrdiff_t n = JS_GC_THREAD_MALLOC_LIMIT - thread->gcThreadMallocBytes;
    thread->gcThreadMallocBytes = JS_GC_THREAD_MALLOC_LIMIT;

    JS_LOCK_GC(runtime);
    runtime->gcMallocBytes -= n;
    if (runtime->isGCMallocLimitReached())
#endif
    {
        JS_ASSERT(runtime->isGCMallocLimitReached());
        runtime->gcMallocBytes = -1;

        /*
         * Empty the GC free lists to trigger a last-ditch GC when allocating
         * any GC thing later on this thread. This minimizes the amount of
         * checks on the fast path of the GC allocator. Note that we cannot
         * touch the free lists on other threads as their manipulation is not
         * thread-safe.
         */
        JS_THREAD_DATA(this)->purgeGCFreeLists();
        js_TriggerGC(this, true);
    }
    JS_UNLOCK_GC(runtime);
}
