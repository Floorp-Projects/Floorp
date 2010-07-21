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

#include "jsstdint.h"

#include "jstypes.h"
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
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnativestack.h"
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

#include "jscntxtinlines.h"

#ifdef XP_WIN
# include <windows.h>
#elif defined(XP_OS2)
# define INCL_DOSMEMMGR
# include <os2.h>
#else
# include <unistd.h>
# include <sys/mman.h>
# if !defined(MAP_ANONYMOUS)
#  if defined(MAP_ANON)
#   define MAP_ANONYMOUS MAP_ANON
#  else
#   define MAP_ANONYMOUS 0
#  endif
# endif
#endif

using namespace js;

static const size_t ARENA_HEADER_SIZE_HACK = 40;
static const size_t TEMP_POOL_CHUNK_SIZE = 4096 - ARENA_HEADER_SIZE_HACK;

static void
FreeContext(JSContext *cx);

#ifdef DEBUG
JS_REQUIRES_STACK bool
CallStack::contains(const JSStackFrame *fp) const
{
    JS_ASSERT(inContext());
    JSStackFrame *start;
    JSStackFrame *stop;
    if (isSuspended()) {
        start = suspendedFrame;
        stop = initialFrame->down;
    } else {
        start = cx->fp;
        stop = cx->activeCallStack()->initialFrame->down;
    }
    for (JSStackFrame *f = start; f != stop; f = f->down) {
        if (f == fp)
            return true;
    }
    return false;
}
#endif

bool
StackSpace::init()
{
    void *p;
#ifdef XP_WIN
    p = VirtualAlloc(NULL, CAPACITY_BYTES, MEM_RESERVE, PAGE_READWRITE);
    if (!p)
        return false;
    void *check = VirtualAlloc(p, COMMIT_BYTES, MEM_COMMIT, PAGE_READWRITE);
    if (p != check)
        return false;
    base = reinterpret_cast<jsval *>(p);
    commitEnd = base + COMMIT_VALS;
    end = base + CAPACITY_VALS;
#elif defined(XP_OS2)
    if (DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_ANY) &&
        DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE))
        return false;
    base = reinterpret_cast<jsval *>(p);
    end = base + CAPACITY_VALS;
#else
    JS_ASSERT(CAPACITY_BYTES % getpagesize() == 0);
    p = mmap(NULL, CAPACITY_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base = reinterpret_cast<jsval *>(p);
    end = base + CAPACITY_VALS;
#endif
    return true;
}

void
StackSpace::finish()
{
#ifdef XP_WIN
    VirtualFree(base, (commitEnd - base) * sizeof(jsval), MEM_DECOMMIT);
    VirtualFree(base, 0, MEM_RELEASE);
#elif defined(XP_OS2)
    DosFreeMem(base);
#else
    munmap(base, CAPACITY_BYTES);
#endif
}

#ifdef XP_WIN
JS_FRIEND_API(bool)
StackSpace::bumpCommit(jsval *from, ptrdiff_t nvals) const
{
    JS_ASSERT(end - from >= nvals);
    jsval *newCommit = commitEnd;
    jsval *request = from + nvals;

    /* Use a dumb loop; will probably execute once. */
    JS_ASSERT((end - newCommit) % COMMIT_VALS == 0);
    do {
        newCommit += COMMIT_VALS;
        JS_ASSERT((end - newCommit) >= 0);
    } while (newCommit < request);

    /* The cast is safe because CAPACITY_BYTES is small. */
    int32 size = static_cast<int32>(newCommit - commitEnd) * sizeof(jsval);

    if (!VirtualAlloc(commitEnd, size, MEM_COMMIT, PAGE_READWRITE))
        return false;
    commitEnd = newCommit;
    return true;
}
#endif

JS_REQUIRES_STACK void
StackSpace::mark(JSTracer *trc)
{
    /*
     * The correctness/completeness of marking depends on the continuity
     * invariants described by the CallStack and StackSpace definitions.
     */
    jsval *end = firstUnused();
    for (CallStack *cs = currentCallStack; cs; cs = cs->getPreviousInThread()) {
        if (cs->inContext()) {
            /* This may be the only pointer to the initialVarObj. */
            if (JSObject *varobj = cs->getInitialVarObj())
                JS_CALL_OBJECT_TRACER(trc, varobj, "varobj");

            /* Mark slots/args trailing off of the last stack frame. */
            JSStackFrame *fp = cs->getCurrentFrame();
            TraceValues(trc, fp->slots(), end, "stack");

            /* Mark stack frames and slots/args between stack frames. */
            JSStackFrame *initialFrame = cs->getInitialFrame();
            for (JSStackFrame *f = fp; f != initialFrame; f = f->down) {
                js_TraceStackFrame(trc, f);
                TraceValues(trc, f->down->slots(), f->argEnd(), "stack");
            }

            /* Mark initialFrame stack frame and leading args. */
            js_TraceStackFrame(trc, initialFrame);
            TraceValues(trc, cs->getInitialArgBegin(), initialFrame->argEnd(), "stack");
        } else {
            /* Mark slots/args trailing off callstack. */
            JS_ASSERT(end == cs->getInitialArgEnd());
            TraceValues(trc, cs->getInitialArgBegin(), cs->getInitialArgEnd(), "stack");
        }
        end = cs->previousCallStackEnd();
    }
}

JS_REQUIRES_STACK bool
StackSpace::pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard &ag)
{
    jsval *start = firstUnused();
    uintN vplen = 2 + argc;
    ptrdiff_t nvals = VALUES_PER_CALL_STACK + vplen;
    if (!ensureSpace(cx, start, nvals))
        return false;
    jsval *vp = start + VALUES_PER_CALL_STACK;
    jsval *vpend = vp + vplen;
    memset(vp, 0, vplen * sizeof(jsval)); /* Init so GC-safe on exit. */

    CallStack *cs = new(start) CallStack;
    cs->setInitialArgEnd(vpend);
    cs->setPreviousInThread(currentCallStack);
    currentCallStack = cs;

    ag.cx = cx;
    ag.cs = cs;
    ag.argc = argc;
    ag.vp = vp;
    return true;
}

JS_REQUIRES_STACK JS_FRIEND_API(bool)
StackSpace::pushInvokeArgsFriendAPI(JSContext *cx, uintN argc,
                                    InvokeArgsGuard &ag)
{
    return cx->stack().pushInvokeArgs(cx, argc, ag);
}

InvokeFrameGuard::InvokeFrameGuard()
  : cx(NULL), cs(NULL), fp(NULL)
{}

/*
 * To maintain the 1 to 0..1 relationship between callstacks and js_Interpret
 * activations, a callstack is pushed if one was not pushed for the arguments
 * (viz., if the ternary InvokeArgsGuard constructor was used instead of the
 * nullary constructor + pushInvokeArgs).
 */
bool
StackSpace::getInvokeFrame(JSContext *cx, const InvokeArgsGuard &ag,
                           uintN nmissing, uintN nfixed,
                           InvokeFrameGuard &fg) const
{
    if (ag.cs) {
        JS_ASSERT(ag.cs == currentCallStack && !ag.cs->inContext());
        jsval *start = ag.cs->getInitialArgEnd();
        ptrdiff_t nvals = nmissing + VALUES_PER_STACK_FRAME + nfixed;
        if (!ensureSpace(cx, start, nvals))
            return false;
        fg.fp = reinterpret_cast<JSStackFrame *>(start + nmissing);
        return true;
    }

    assertIsCurrent(cx);
    JS_ASSERT(currentCallStack->isActive());
    jsval *start = cx->regs->sp;
    ptrdiff_t nvals = nmissing + VALUES_PER_CALL_STACK + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, start, nvals))
        return false;
    fg.cs = new(start + nmissing) CallStack;
    fg.fp = reinterpret_cast<JSStackFrame *>(fg.cs + 1);
    return true;
}

JS_REQUIRES_STACK void
StackSpace::pushInvokeFrame(JSContext *cx, const InvokeArgsGuard &ag,
                            InvokeFrameGuard &fg, JSFrameRegs &regs)
{
    JS_ASSERT(!!ag.cs ^ !!fg.cs);
    JS_ASSERT_IF(ag.cs, ag.cs == currentCallStack && !ag.cs->inContext());
    if (CallStack *cs = fg.cs) {
        cs->setPreviousInThread(currentCallStack);
        currentCallStack = cs;
    }
    JSStackFrame *fp = fg.fp;
    fp->down = cx->fp;
    cx->pushCallStackAndFrame(currentCallStack, fp, regs);
    currentCallStack->setInitialVarObj(NULL);
    fg.cx = cx;
}

JS_REQUIRES_STACK
InvokeFrameGuard::~InvokeFrameGuard()
{
    if (!cx)
        return;
    JS_ASSERT(fp && fp == cx->fp);
    JS_ASSERT_IF(cs, cs == cx->stack().getCurrentCallStack());
    cx->stack().popInvokeFrame(cx, cs);
}

JS_REQUIRES_STACK void
StackSpace::popInvokeFrame(JSContext *cx, CallStack *maybecs)
{
    assertIsCurrent(cx);
    JS_ASSERT(currentCallStack->getInitialFrame() == cx->fp);
    JS_ASSERT_IF(maybecs, maybecs == currentCallStack);
    cx->popCallStackAndFrame();
    if (maybecs)
        currentCallStack = currentCallStack->getPreviousInThread();
}

ExecuteFrameGuard::ExecuteFrameGuard()
  : cx(NULL), vp(NULL), fp(NULL)
{}

JS_REQUIRES_STACK
ExecuteFrameGuard::~ExecuteFrameGuard()
{
    if (!cx)
        return;
    JS_ASSERT(cx->activeCallStack() == cs);
    JS_ASSERT(cx->fp == fp);
    cx->stack().popExecuteFrame(cx);
}

/*
 * To maintain a 1 to 0..1 relationship between callstacks and js_Interpret
 * activations, we push a callstack even if it wasn't otherwise necessary.
 */
JS_REQUIRES_STACK bool
StackSpace::getExecuteFrame(JSContext *cx, JSStackFrame *down,
                            uintN vplen, uintN nfixed,
                            ExecuteFrameGuard &fg) const
{
    jsval *start = firstUnused();
    ptrdiff_t nvals = VALUES_PER_CALL_STACK + vplen + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, start, nvals))
        return false;

    fg.cs = new(start) CallStack;
    fg.vp = start + VALUES_PER_CALL_STACK;
    fg.fp = reinterpret_cast<JSStackFrame *>(fg.vp + vplen);
    fg.down = down;
    return true;
}

JS_REQUIRES_STACK void
StackSpace::pushExecuteFrame(JSContext *cx, ExecuteFrameGuard &fg,
                             JSFrameRegs &regs, JSObject *initialVarObj)
{
    fg.fp->down = fg.down;
    CallStack *cs = fg.cs;
    cs->setPreviousInThread(currentCallStack);
    currentCallStack = cs;
    cx->pushCallStackAndFrame(cs, fg.fp, regs);
    cs->setInitialVarObj(initialVarObj);
    fg.cx = cx;
}

JS_REQUIRES_STACK void
StackSpace::popExecuteFrame(JSContext *cx)
{
    assertIsCurrent(cx);
    JS_ASSERT(cx->hasActiveCallStack());
    cx->popCallStackAndFrame();
    currentCallStack = currentCallStack->getPreviousInThread();
}

JS_REQUIRES_STACK void
StackSpace::getSynthesizedSlowNativeFrame(JSContext *cx, CallStack *&cs, JSStackFrame *&fp)
{
    jsval *start = firstUnused();
    JS_ASSERT(size_t(end - start) >= VALUES_PER_CALL_STACK + VALUES_PER_STACK_FRAME);
    cs = new(start) CallStack;
    fp = reinterpret_cast<JSStackFrame *>(cs + 1);
}

JS_REQUIRES_STACK void
StackSpace::pushSynthesizedSlowNativeFrame(JSContext *cx, CallStack *cs, JSStackFrame *fp,
                                           JSFrameRegs &regs)
{
    JS_ASSERT(!fp->script && FUN_SLOW_NATIVE(fp->fun));
    fp->down = cx->fp;
    cs->setPreviousInThread(currentCallStack);
    currentCallStack = cs;
    cx->pushCallStackAndFrame(cs, fp, regs);
    cs->setInitialVarObj(NULL);
}

JS_REQUIRES_STACK void
StackSpace::popSynthesizedSlowNativeFrame(JSContext *cx)
{
    assertIsCurrent(cx);
    JS_ASSERT(cx->hasActiveCallStack());
    JS_ASSERT(currentCallStack->getInitialFrame() == cx->fp);
    JS_ASSERT(!cx->fp->script && FUN_SLOW_NATIVE(cx->fp->fun));
    cx->popCallStackAndFrame();
    currentCallStack = currentCallStack->getPreviousInThread();
}

/*
 * When a pair of down-linked stack frames are in the same callstack, the
 * up-frame's address is the top of the down-frame's stack, modulo missing
 * arguments.
 */
static inline jsval *
InlineDownFrameSP(JSStackFrame *up)
{
    JS_ASSERT(up->fun && up->script);
    jsval *sp = up->argv + up->argc;
#ifdef DEBUG
    uint16 nargs = up->fun->nargs;
    uintN argc = up->argc;
    uintN missing = argc < nargs ? nargs - argc : 0;
    JS_ASSERT(sp == (jsval *)up - missing);
#endif
    return sp;
}

JS_REQUIRES_STACK
FrameRegsIter::FrameRegsIter(JSContext *cx)
{
    curcs = cx->getCurrentCallStack();
    if (!curcs) {
        curfp = NULL;
        return;
    }
    if (curcs->isSuspended()) {
        curfp = curcs->getSuspendedFrame();
        cursp = curcs->getSuspendedRegs()->sp;
        curpc = curcs->getSuspendedRegs()->pc;
        return;
    }
    JS_ASSERT(cx->fp);
    curfp = cx->fp;
    cursp = cx->regs->sp;
    curpc = cx->regs->pc;
    return;
}

FrameRegsIter &
FrameRegsIter::operator++()
{
    JSStackFrame *up = curfp;
    JSStackFrame *down = curfp = curfp->down;
    if (!down)
        return *this;

    curpc = down->savedPC;

    /* For a contiguous down and up, compute sp from up. */
    if (up != curcs->getInitialFrame()) {
        cursp = InlineDownFrameSP(up);
        return *this;
    }

    /*
     * If the up-frame is in csup and the down-frame is in csdown, it is not
     * necessarily the case that |csup->getPreviousInContext == csdown| or that
     * |csdown->getSuspendedFrame == down| (because of indirect eval and
     * JS_EvaluateInStackFrame). To compute down's sp, we need to do a linear
     * scan, keeping track of what is immediately after down in memory.
     */
    curcs = curcs->getPreviousInContext();
    cursp = curcs->getSuspendedSP();
    JSStackFrame *f = curcs->getSuspendedFrame();
    while (f != down) {
        if (f == curcs->getInitialFrame()) {
            curcs = curcs->getPreviousInContext();
            cursp = curcs->getSuspendedSP();
            f = curcs->getSuspendedFrame();
        } else {
            cursp = InlineDownFrameSP(f);
            f = f->down;
        }
    }
    return *this;
}

bool
JSThreadData::init()
{
#ifdef DEBUG
    /* The data must be already zeroed. */
    for (size_t i = 0; i != sizeof(*this); ++i)
        JS_ASSERT(reinterpret_cast<uint8*>(this)[i] == 0);
#endif
    if (!stackSpace.init())
        return false;
#ifdef JS_TRACER
    InitJIT(&traceMonitor);
#endif
    dtoaState = js_NewDtoaState();
    if (!dtoaState) {
        finish();
        return false;
    }
    nativeStackBase = GetNativeStackBase();
    return true;
}

void
JSThreadData::finish()
{
#ifdef DEBUG
    /* All GC-related things must be already removed at this point. */
    JS_ASSERT(gcFreeLists.isEmpty());
    for (size_t i = 0; i != JS_ARRAY_LENGTH(scriptsToGC); ++i)
        JS_ASSERT(!scriptsToGC[i]);
    JS_ASSERT(!conservativeGC.isEnabled());
#endif

    if (dtoaState)
        js_DestroyDtoaState(dtoaState);

    js_FinishGSNCache(&gsnCache);
    propertyCache.~PropertyCache();
#if defined JS_TRACER
    FinishJIT(&traceMonitor);
#endif
    stackSpace.finish();
}

void
JSThreadData::mark(JSTracer *trc)
{
    stackSpace.mark(trc);
#ifdef JS_TRACER
    traceMonitor.mark(trc);
#endif
}

void
JSThreadData::purge(JSContext *cx)
{
    gcFreeLists.purge();

    js_PurgeGSNCache(&gsnCache);

    /* FIXME: bug 506341. */
    propertyCache.purge(cx);

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

    /* Purge cached native iterators. */
    memset(cachedNativeIterators, 0, sizeof(cachedNativeIterators));

    dtoaCache.s = NULL;
}

#ifdef JS_THREADSAFE

static JSThread *
NewThread(void *id)
{
    JS_ASSERT(js_CurrentThreadId() == id);
    JSThread *thread = (JSThread *) js_calloc(sizeof(JSThread));
    if (!thread)
        return NULL;
    JS_INIT_CLIST(&thread->contextList);
    thread->id = id;
    if (!thread->data.init()) {
        js_free(thread);
        return NULL;
    }
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
    } else {
        JS_UNLOCK_GC(rt);
        thread = NewThread(id);
        if (!thread)
            return NULL;
        JS_LOCK_GC(rt);
        js_WaitForGC(rt);
        if (!rt->threads.relookupOrAdd(p, id, thread)) {
            JS_UNLOCK_GC(rt);
            DestroyThread(thread);
            return NULL;
        }

        /* Another thread cannot add an entry for the current thread id. */
        JS_ASSERT(p->value == thread);
    }
    JS_ASSERT(thread->id == id);

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
    if (!rt->threads.init(4))
        return false;
#else
    if (!rt->threadData.init())
        return false;
#endif
    return true;
}

void
js_FinishThreads(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    if (!rt->threads.initialized())
        return;
    for (JSThread::Map::Range r = rt->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        JS_ASSERT(JS_CLIST_IS_EMPTY(&thread->contextList));
        DestroyThread(thread);
    }
    rt->threads.clear();
#else
    rt->threadData.finish();
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
            JS_ASSERT(cx->thread != thread);
            js_DestroyScriptsToGC(cx, &thread->data);

            /*
             * The following is potentially suboptimal as it also zeros the
             * caches in data, but the code simplicity wins here.
             */
            thread->data.gcFreeLists.purge();
            DestroyThread(thread);
            e.removeFront();
        } else {
            thread->data.purge(cx);
            thread->gcThreadMallocBytes = JS_GC_THREAD_MALLOC_LIMIT;
        }
    }
#else
    cx->runtime->threadData.purge(cx);
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

    JS_InitArenaPool(&cx->tempPool, "temp", TEMP_POOL_CHUNK_SIZE, sizeof(jsdouble),
                     &cx->scriptStackQuota);

    js_InitRegExpStatics(cx);
    JS_ASSERT(cx->resolveFlags == 0);

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
        if (ok) {
            /*
             * Ensure that the empty scopes initialized by
             * JSScope::initRuntimeState get the desired special shapes.
             * (The rt->state dance above guarantees that this abuse of
             * rt->shapeGen is thread-safe.)
             */
            uint32 shapeGen = rt->shapeGen;
            rt->shapeGen = 0;
            ok = JSScope::initRuntimeState(cx);
            if (rt->shapeGen < shapeGen)
                rt->shapeGen = shapeGen;
        }

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

    cxCallback = rt->cxCallback;
    if (cxCallback && !cxCallback(cx, JSCONTEXT_NEW)) {
        js_DestroyContext(cx, JSDCM_NEW_FAILED);
        return NULL;
    }

    /* Using ContextAllocPolicy, so init after JSContext is ready. */
    if (!cx->busyArrays.init()) {
        FreeContext(cx);
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

    JS_ASSERT(!cx->enumerators);

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

            JSScope::finishRuntimeState(cx);
            js_FinishRuntimeNumberState(cx);

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
#ifdef JS_METER_DST_OFFSET_CACHING
    cx->dstOffsetCache.dumpStats();
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

    /* Destroy the resolve recursion damper. */
    if (cx->resolvingTable) {
        JS_DHashTableDestroy(cx->resolvingTable);
        cx->resolvingTable = NULL;
    }

    /* Finally, free cx itself. */
    cx->~JSContext();
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

    Conditionally<AutoLockGC> lockIf(!!unlocked, rt);
    cx = js_ContextFromLinkField(cx ? cx->link.next : rt->contextList.next);
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
        if (cx->requestDepth)
            break;
    }
    return cx;
#else
    return js_ContextIterator(rt, JS_FALSE, &iter);
#endif
}

static JSDHashNumber
resolving_HashKey(JSDHashTable *table, const void *ptr)
{
    const JSResolvingKey *key = (const JSResolvingKey *)ptr;

    return (JSDHashNumber(uintptr_t(key->obj)) >> JSVAL_TAGBITS) ^ key->id;
}

static JSBool
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
     * propagates out of scope.  This is needed for compatability
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
    for (JSStackFrame *fp = js_GetTopStackFrame(cx); fp; fp = fp->down) {
        if (fp->pc(cx)) {
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
        /*
         * Error in strict code; warning with strict option; okay otherwise.
         * We assume that if the top frame is a native, then it is strict if
         * the nearest scripted frame is strict, see bug 536306.
         */
        JSStackFrame *fp = js_GetScriptedCaller(cx, NULL);
        if (fp && fp->script->strictModeCode)
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

    PodZero(&report);
    report.flags = flags;
    report.errorNumber = JSMSG_USER_DEFINED_ERROR;
    report.ucmessage = ucmessage = js_InflateString(cx, message, &messagelen);
    PopulateReportBlame(cx, &report);

    warning = JSREPORT_IS_WARNING(report.flags);

    ReportError(cx, message, &report, NULL, NULL);
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
    JSRuntime *rt = cx->runtime;
    if (rt->gcIsNeeded) {
        js_GC(cx, GC_NORMAL);

        /*
         * On trace we can exceed the GC quota, see comments in NewGCArena. So
         * we check the quota and report OOM here when we are off trace.
         */
        bool delayedOutOfMemory;
        JS_LOCK_GC(rt);
        delayedOutOfMemory = (rt->gcBytes > rt->gcMaxBytes);
        JS_UNLOCK_GC(rt);
        if (delayedOutOfMemory) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }
#ifdef JS_THREADSAFE
    else {
        JS_YieldRequest(cx);
    }
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
#ifdef JS_THREADSAFE
    Conditionally<AutoLockGC> lockIf(!gcLocked, rt);
#endif
    JSContext *iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_FALSE, &iter))
        JS_TriggerOperationCallback(acx);
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
        pc = cx->regs ? cx->regs->pc : NULL;
        if (!pc)
            return NULL;
        imacpc = cx->fp->imacpc;
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
DSTOffsetCache::purge()
{
    /*
     * NB: The initial range values are carefully chosen to result in a cache
     *     miss on first use given the range of possible values.  Be careful
     *     to keep these values and the caching algorithm in sync!
     */
    offsetMilliseconds = 0;
    rangeStartSeconds = rangeEndSeconds = INT64_MIN;

#ifdef JS_METER_DST_OFFSET_CACHING
    totalCalculations = 0;
    hit = 0;
    missIncreasing = missDecreasing = 0;
    missIncreasingOffsetChangeExpand = missIncreasingOffsetChangeUpper = 0;
    missDecreasingOffsetChangeExpand = missDecreasingOffsetChangeLower = 0;
    missLargeIncrease = missLargeDecrease = 0;
#endif

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
  : runtime(rt),
    compartment(rt->defaultCompartment),
    fp(NULL),
    regs(NULL),
    regExpStatics(this),
    busyArrays(this)
{}

void
JSContext::pushCallStackAndFrame(js::CallStack *newcs, JSStackFrame *newfp,
                                 JSFrameRegs &newregs)
{
    if (hasActiveCallStack()) {
        JS_ASSERT(fp->savedPC == JSStackFrame::sInvalidPC);
        fp->savedPC = regs->pc;
        currentCallStack->suspend(fp, regs);
    }
    newcs->setPreviousInContext(currentCallStack);
    currentCallStack = newcs;
#ifdef DEBUG
    newfp->savedPC = JSStackFrame::sInvalidPC;
#endif
    setCurrentFrame(newfp);
    setCurrentRegs(&newregs);
    newcs->joinContext(this, newfp);
}

void
JSContext::popCallStackAndFrame()
{
    JS_ASSERT(currentCallStack->maybeContext() == this);
    JS_ASSERT(currentCallStack->getInitialFrame() == fp);
    JS_ASSERT(fp->savedPC == JSStackFrame::sInvalidPC);
    currentCallStack->leaveContext();
    currentCallStack = currentCallStack->getPreviousInContext();
    if (currentCallStack) {
        if (currentCallStack->isSaved()) {
            setCurrentFrame(NULL);
            setCurrentRegs(NULL);
        } else {
            setCurrentFrame(currentCallStack->getSuspendedFrame());
            setCurrentRegs(currentCallStack->getSuspendedRegs());
            currentCallStack->resume();
#ifdef DEBUG
            fp->savedPC = JSStackFrame::sInvalidPC;
#endif
        }
    } else {
        JS_ASSERT(fp->down == NULL);
        setCurrentFrame(NULL);
        setCurrentRegs(NULL);
    }
}

void
JSContext::saveActiveCallStack()
{
    JS_ASSERT(hasActiveCallStack());
    currentCallStack->save(fp, regs);
    JS_ASSERT(fp->savedPC == JSStackFrame::sInvalidPC);
    fp->savedPC = regs->pc;
    setCurrentFrame(NULL);
    setCurrentRegs(NULL);
}

void
JSContext::restoreCallStack()
{
    js::CallStack *ccs = currentCallStack;
    setCurrentFrame(ccs->getSuspendedFrame());
    setCurrentRegs(ccs->getSuspendedRegs());
    ccs->restore();
#ifdef DEBUG
    fp->savedPC = JSStackFrame::sInvalidPC;
#endif
}

JSGenerator *
JSContext::generatorFor(JSStackFrame *fp) const
{
    JS_ASSERT(stack().contains(fp) && fp->isGenerator());
    JS_ASSERT(!fp->isFloatingGenerator());
    JS_ASSERT(!genStack.empty());

    if (JS_LIKELY(fp == genStack.back()->liveFrame))
        return genStack.back();

    /* General case; should only be needed for debug APIs. */
    for (size_t i = 0; i < genStack.length(); ++i) {
        if (genStack[i]->liveFrame == fp)
            return genStack[i];
    }
    JS_NOT_REACHED("no matching generator");
    return NULL;
}

CallStack *
JSContext::containingCallStack(const JSStackFrame *target)
{
    /* The context may have nothing running. */
    CallStack *cs = currentCallStack;
    if (!cs)
        return NULL;

    /* The active callstack's top frame is cx->fp. */
    if (fp) {
        JS_ASSERT(activeCallStack() == cs);
        JSStackFrame *f = fp;
        JSStackFrame *stop = cs->getInitialFrame()->down;
        for (; f != stop; f = f->down) {
            if (f == target)
                return cs;
        }
        cs = cs->getPreviousInContext();
    }

    /* A suspended callstack's top frame is its suspended frame. */
    for (; cs; cs = cs->getPreviousInContext()) {
        JSStackFrame *f = cs->getSuspendedFrame();
        JSStackFrame *stop = cs->getInitialFrame()->down;
        for (; f != stop; f = f->down) {
            if (f == target)
                return cs;
        }
    }

    return NULL;
}

void
JSContext::checkMallocGCPressure(void *p)
{
    if (!p) {
        js_ReportOutOfMemory(this);
        return;
    }

#ifdef JS_THREADSAFE
    JS_ASSERT(thread);
    JS_ASSERT(thread->gcThreadMallocBytes <= 0);
    ptrdiff_t n = JS_GC_THREAD_MALLOC_LIMIT - thread->gcThreadMallocBytes;
    thread->gcThreadMallocBytes = JS_GC_THREAD_MALLOC_LIMIT;

    AutoLockGC lock(runtime);
    runtime->gcMallocBytes -= n;

    /*
     * Trigger the GC on memory pressure but only if we are inside a request
     * and not inside a GC.
     */
    if (runtime->isGCMallocLimitReached() && requestDepth != 0)
#endif
    {
        if (!runtime->gcRunning) {
            JS_ASSERT(runtime->isGCMallocLimitReached());
            runtime->gcMallocBytes = -1;

            /*
             * Empty the GC free lists to trigger a last-ditch GC when any GC
             * thing is allocated later on this thread. This makes unnecessary
             * to check for the memory pressure on the fast path of the GC
             * allocator. We cannot touch the free lists on other threads as
             * their manipulation is not thread-safe.
             */
            JS_THREAD_DATA(this)->gcFreeLists.purge();
            js_TriggerGC(this, true);
        }
    }
}

bool
JSContext::isConstructing()
{
#ifdef JS_TRACER
    if (JS_ON_TRACE(this)) {
        JS_ASSERT(bailExit);
        return *bailExit->pc == JSOP_NEW;
    }
#endif
    JSStackFrame *fp = js_GetTopStackFrame(this);
    return fp && (fp->flags & JSFRAME_CONSTRUCTING);
}


/*
 * Release pool's arenas if the stackPool has existed for longer than the
 * limit specified by gcEmptyArenaPoolLifespan.
 */
inline void
FreeOldArenas(JSRuntime *rt, JSArenaPool *pool)
{
    JSArena *a = pool->current;
    if (a == pool->first.next && a->avail == a->base + sizeof(int64)) {
        int64 age = JS_Now() - *(int64 *) a->base;
        if (age > int64(rt->gcEmptyArenaPoolLifespan) * 1000)
            JS_FreeArenaPool(pool);
    }
}

void
JSContext::purge()
{
    FreeOldArenas(runtime, &regexpPool);
}

