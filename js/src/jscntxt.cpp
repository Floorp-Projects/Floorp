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
#include "jsinterpinlines.h"
#include "jsobjinlines.h"

#ifdef XP_WIN
# include "jswin.h"
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
StackSegment::contains(const JSStackFrame *fp) const
{
    JS_ASSERT(inContext());
    JSStackFrame *start;
    JSStackFrame *stop;
    if (isActive()) {
        JS_ASSERT(cx->hasfp());
        start = cx->fp();
        stop = cx->activeSegment()->initialFrame->prev();
    } else {
        JS_ASSERT(suspendedRegs && suspendedRegs->fp);
        start = suspendedRegs->fp;
        stop = initialFrame->prev();
    }
    for (JSStackFrame *f = start; f != stop; f = f->prev()) {
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
    base = reinterpret_cast<Value *>(p);
    commitEnd = base + COMMIT_VALS;
    end = base + CAPACITY_VALS;
#elif defined(XP_OS2)
    if (DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_ANY) &&
        DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE))
        return false;
    base = reinterpret_cast<Value *>(p);
    end = base + CAPACITY_VALS;
#else
    JS_ASSERT(CAPACITY_BYTES % getpagesize() == 0);
    p = mmap(NULL, CAPACITY_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base = reinterpret_cast<Value *>(p);
    end = base + CAPACITY_VALS;
#endif
    return true;
}

void
StackSpace::finish()
{
#ifdef XP_WIN
    VirtualFree(base, (commitEnd - base) * sizeof(Value), MEM_DECOMMIT);
    VirtualFree(base, 0, MEM_RELEASE);
#elif defined(XP_OS2)
    DosFreeMem(base);
#else
#ifdef SOLARIS
    munmap((caddr_t)base, CAPACITY_BYTES);
#else
    munmap(base, CAPACITY_BYTES);
#endif
#endif
}

#ifdef XP_WIN
JS_FRIEND_API(bool)
StackSpace::bumpCommit(Value *from, ptrdiff_t nvals) const
{
    JS_ASSERT(end - from >= nvals);
    Value *newCommit = commitEnd;
    Value *request = from + nvals;

    /* Use a dumb loop; will probably execute once. */
    JS_ASSERT((end - newCommit) % COMMIT_VALS == 0);
    do {
        newCommit += COMMIT_VALS;
        JS_ASSERT((end - newCommit) >= 0);
    } while (newCommit < request);

    /* The cast is safe because CAPACITY_BYTES is small. */
    int32 size = static_cast<int32>(newCommit - commitEnd) * sizeof(Value);

    if (!VirtualAlloc(commitEnd, size, MEM_COMMIT, PAGE_READWRITE))
        return false;
    commitEnd = newCommit;
    return true;
}
#endif

void
StackSpace::mark(JSTracer *trc)
{
    /*
     * The correctness/completeness of marking depends on the continuity
     * invariants described by the StackSegment and StackSpace definitions.
     *
     * NB:
     * Stack slots might be torn or uninitialized in the presence of method
     * JIT'd code. Arguments are an exception and are always fully synced
     * (so they can be read by functions).
     */
    Value *end = firstUnused();
    for (StackSegment *seg = currentSegment; seg; seg = seg->getPreviousInMemory()) {
        if (seg->inContext()) {
            /* This may be the only pointer to the initialVarObj. */
            if (seg->hasInitialVarObj())
                MarkObject(trc, seg->getInitialVarObj(), "varobj");

            /* Mark slots/args trailing off of the last stack frame. */
            JSStackFrame *fp = seg->getCurrentFrame();
            MarkStackRangeConservatively(trc, fp->slots(), end);

            /* Mark stack frames and slots/args between stack frames. */
            JSStackFrame *initial = seg->getInitialFrame();
            for (JSStackFrame *f = fp; f != initial; f = f->prev()) {
                js_TraceStackFrame(trc, f);
                MarkStackRangeConservatively(trc, f->prev()->slots(), (Value *)f);
            }

            /* Mark initial stack frame and leading args. */
            js_TraceStackFrame(trc, initial);
            MarkStackRangeConservatively(trc, seg->valueRangeBegin(), (Value *)initial);
        } else {
            /* Mark slots/args trailing off segment. */
            MarkValueRange(trc, seg->valueRangeBegin(), end, "stack");
        }
        end = (Value *)seg;
    }
}

bool
StackSpace::pushSegmentForInvoke(JSContext *cx, uintN argc, InvokeArgsGuard *ag)
{
    Value *start = firstUnused();
    ptrdiff_t nvals = VALUES_PER_STACK_SEGMENT + 2 + argc;
    if (!ensureSpace(cx, start, nvals))
        return false;

    StackSegment *seg = new(start) StackSegment;
    seg->setPreviousInMemory(currentSegment);
    currentSegment = seg;

    ag->cx = cx;
    ag->seg = seg;
    ag->argv_ = seg->valueRangeBegin() + 2;
    ag->argc_ = argc;

    /* Use invokeArgEnd to root [vp, vpend) until the frame is pushed. */
#ifdef DEBUG
    ag->prevInvokeSegment = invokeSegment;
    invokeSegment = seg;
    ag->prevInvokeFrame = invokeFrame;
    invokeFrame = NULL;
#endif
    ag->prevInvokeArgEnd = invokeArgEnd;
    invokeArgEnd = ag->argv() + ag->argc();
    return true;
}

void
StackSpace::popSegmentForInvoke(const InvokeArgsGuard &ag)
{
    JS_ASSERT(!currentSegment->inContext());
    JS_ASSERT(ag.seg == currentSegment);
    JS_ASSERT(invokeSegment == currentSegment);
    JS_ASSERT(invokeArgEnd == ag.argv() + ag.argc());

    currentSegment = currentSegment->getPreviousInMemory();

#ifdef DEBUG
    invokeSegment = ag.prevInvokeSegment;
    invokeFrame = ag.prevInvokeFrame;
#endif
    invokeArgEnd = ag.prevInvokeArgEnd;
}

bool
StackSpace::getSegmentAndFrame(JSContext *cx, uintN vplen, uintN nfixed,
                               FrameGuard *fg) const
{
    Value *start = firstUnused();
    uintN nvals = VALUES_PER_STACK_SEGMENT + vplen + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, start, nvals))
        return false;

    fg->seg_ = new(start) StackSegment;
    fg->vp_ = start + VALUES_PER_STACK_SEGMENT;
    fg->fp_ = reinterpret_cast<JSStackFrame *>(fg->vp() + vplen);
    return true;
}

void
StackSpace::pushSegmentAndFrame(JSContext *cx, JSObject *initialVarObj,
                                JSFrameRegs *regs, FrameGuard *fg)
{
    /* Caller should have already initialized regs. */
    JS_ASSERT(regs->fp == fg->fp());

    /* Push segment. */
    StackSegment *seg = fg->segment();
    seg->setPreviousInMemory(currentSegment);
    currentSegment = seg;

    /* Push frame. */
    cx->pushSegmentAndFrame(seg, *regs);
    seg->setInitialVarObj(initialVarObj);
    fg->cx_ = cx;
}

void
StackSpace::popSegmentAndFrame(JSContext *cx)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    cx->popSegmentAndFrame();
    currentSegment = currentSegment->getPreviousInMemory();
}

FrameGuard::~FrameGuard()
{
    if (!pushed())
        return;
    JS_ASSERT(cx_->activeSegment() == segment());
    JS_ASSERT(cx_->maybefp() == fp());
    cx_->stack().popSegmentAndFrame(cx_);
}

bool
StackSpace::getExecuteFrame(JSContext *cx, JSScript *script, ExecuteFrameGuard *fg) const
{
    return getSegmentAndFrame(cx, 2, script->nfixed, fg);
}

void
StackSpace::pushExecuteFrame(JSContext *cx, JSObject *initialVarObj, ExecuteFrameGuard *fg)
{
    JSStackFrame *fp = fg->fp();
    JSScript *script = fp->script();
    fg->regs_.pc = script->code;
    fg->regs_.fp = fp;
    fg->regs_.sp = fp->base();
    pushSegmentAndFrame(cx, initialVarObj, &fg->regs_, fg);
}

bool
StackSpace::pushDummyFrame(JSContext *cx, JSObject &scopeChain, DummyFrameGuard *fg)
{
    if (!getSegmentAndFrame(cx, 0 /*vplen*/, 0 /*nfixed*/, fg))
        return false;
    fg->fp()->initDummyFrame(cx, scopeChain);
    fg->regs_.fp = fg->fp();
    fg->regs_.pc = NULL;
    fg->regs_.sp = fg->fp()->slots();
    pushSegmentAndFrame(cx, NULL /*varobj*/, &fg->regs_, fg);
    return true;
}

bool
StackSpace::getGeneratorFrame(JSContext *cx, uintN vplen, uintN nfixed, GeneratorFrameGuard *fg)
{
    return getSegmentAndFrame(cx, vplen, nfixed, fg);
}

void
StackSpace::pushGeneratorFrame(JSContext *cx, JSFrameRegs *regs, GeneratorFrameGuard *fg)
{
    JS_ASSERT(regs->fp == fg->fp());
    JS_ASSERT(regs->fp->prev() == cx->maybefp());
    pushSegmentAndFrame(cx, NULL /*varobj*/, regs, fg);
}

bool
StackSpace::bumpCommitAndLimit(JSStackFrame *base, Value *sp, uintN nvals, Value **limit) const
{
    JS_ASSERT(sp == firstUnused());
    JS_ASSERT(sp + nvals >= *limit);
#ifdef XP_WIN
    if (commitEnd <= *limit) {
        Value *quotaEnd = (Value *)base + STACK_QUOTA;
        if (sp + nvals < quotaEnd) {
            if (!ensureSpace(NULL, sp, nvals))
                return false;
            *limit = Min(quotaEnd, commitEnd);
            return true;
        }
    }
#endif
    return false;
}

void
FrameRegsIter::initSlow()
{
    if (!curseg) {
        curfp = NULL;
        cursp = NULL;
        curpc = NULL;
        return;
    }

    JS_ASSERT(curseg->isSuspended());
    curfp = curseg->getSuspendedFrame();
    cursp = curseg->getSuspendedRegs()->sp;
    curpc = curseg->getSuspendedRegs()->pc;
}

/*
 * Using the invariant described in the js::StackSegment comment, we know that,
 * when a pair of prev-linked stack frames are in the same segment, the
 * first frame's address is the top of the prev-frame's stack, modulo missing
 * arguments.
 */
void
FrameRegsIter::incSlow(JSStackFrame *fp, JSStackFrame *prev)
{
    JS_ASSERT(prev);
    JS_ASSERT(curpc == prev->savedpc_);
    JS_ASSERT(fp == curseg->getInitialFrame());

    /*
     * If fp is in cs and the prev-frame is in csprev, it is not necessarily
     * the case that |cs->getPreviousInContext == csprev| or that
     * |csprev->getSuspendedFrame == prev| (because of indirect eval and
     * JS_EvaluateInStackFrame). To compute prev's sp, we need to do a linear
     * scan, keeping track of what is immediately after prev in memory.
     */
    curseg = curseg->getPreviousInContext();
    cursp = curseg->getSuspendedRegs()->sp;
    JSStackFrame *f = curseg->getSuspendedFrame();
    while (f != prev) {
        if (f == curseg->getInitialFrame()) {
            curseg = curseg->getPreviousInContext();
            cursp = curseg->getSuspendedRegs()->sp;
            f = curseg->getSuspendedFrame();
        } else {
            cursp = f->formalArgsEnd();
            f = f->prev();
        }
    }
}

AllFramesIter::AllFramesIter(JSContext *cx)
  : curcs(cx->stack().getCurrentSegment()),
    curfp(curcs ? curcs->getCurrentFrame() : NULL)
{
}

AllFramesIter&
AllFramesIter::operator++()
{
    JS_ASSERT(!done());
    if (curfp == curcs->getInitialFrame()) {
        curcs = curcs->getPreviousInMemory();
        curfp = curcs ? curcs->getCurrentFrame() : NULL;
    } else {
        curfp = curfp->prev();
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
#ifdef JS_METHODJIT
    jmData.Initialize();
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
#endif

    if (dtoaState)
        js_DestroyDtoaState(dtoaState);

    js_FinishGSNCache(&gsnCache);
    propertyCache.~PropertyCache();
#if defined JS_TRACER
    FinishJIT(&traceMonitor);
#endif
#if defined JS_METHODJIT
    jmData.Finish();
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
    lastNativeIterator = NULL;

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

    /*
     * The conservative GC scanner should be disabled when the thread leaves
     * the last request.
     */
    JS_ASSERT(!thread->data.conservativeGC.hasStackToScan());

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
        }
    }
#else
    cx->runtime->threadData.purge(cx);
#endif
}

bool
js::SyncOptionsToVersion(JSContext* cx)
{
    JSVersion version = cx->findVersion();
    uint32 options = cx->options;
    if (OptionsHasXML(options) == VersionHasXML(version) &&
        OptionsHasAnonFunFix(options) == VersionHasAnonFunFix(version)) {
        /* No need to override. */
        return false;
    }
    VersionSetXML(&version, OptionsHasXML(options));
    VersionSetAnonFunFix(&version, OptionsHasAnonFunFix(options));
    cx->maybeOverrideVersion(version);
    return true;
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
    JS_ASSERT(cx->findVersion() == JSVERSION_DEFAULT);
    VOUCH_DOES_NOT_REQUIRE_STACK();

    JS_InitArenaPool(&cx->tempPool, "temp", TEMP_POOL_CHUNK_SIZE, sizeof(jsdouble),
                     &cx->scriptStackQuota);
    JS_InitArenaPool(&cx->regExpPool, "regExp", TEMP_POOL_CHUNK_SIZE, sizeof(int),
                     &cx->scriptStackQuota);

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
             * Shape::initRuntimeState get the desired special shapes.
             * (The rt->state dance above guarantees that this abuse of
             * rt->shapeGen is thread-safe.)
             */
            uint32 shapeGen = rt->shapeGen;
            rt->shapeGen = 0;
            ok = Shape::initRuntimeState(cx);
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

static void
DumpEvalCacheMeter(JSContext *cx)
{
    if (const char *filename = getenv("JS_EVALCACHE_STATFILE")) {
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
        if (!fp && !fp.open(filename, "w"))
            return;

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
}
# define DUMP_EVAL_CACHE_METER(cx) DumpEvalCacheMeter(cx)

static void
DumpFunctionCountMap(const char *title, JSRuntime::FunctionCountMap &map, FILE *fp)
{
    fprintf(fp, "\n%s count map:\n", title);

    for (JSRuntime::FunctionCountMap::Range r = map.all(); !r.empty(); r.popFront()) {
        JSFunction *fun = r.front().key;
        int32 count = r.front().value;

        fprintf(fp, "%10d %s:%u\n", count, fun->u.i.script->filename, fun->u.i.script->lineno);
    }
}

static void
DumpFunctionMeter(JSContext *cx)
{
    if (const char *filename = cx->runtime->functionMeterFilename) {
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
        if (!fp && !fp.open(filename, "w"))
            return;

        fprintf(fp, "function meter (%s):\n", cx->runtime->lastScriptFilename);
        for (uintN i = 0; i < JS_ARRAY_LENGTH(table); ++i)
            fprintf(fp, "%-19.19s %d\n", table[i].name, *(int32 *)((uint8 *)fm + table[i].offset));

        DumpFunctionCountMap("method read barrier", cx->runtime->methodReadBarrierCountMap, fp);
        DumpFunctionCountMap("unjoined function", cx->runtime->unjoinedFunctionCountMap, fp);

        putc('\n', fp);
        fflush(fp);
    }
}

# define DUMP_FUNCTION_METER(cx)   DumpFunctionMeter(cx)

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

    /*
     * For API compatibility we support destroying contexts with non-zero
     * cx->outstandingRequests but we assume that all JS_BeginRequest calls
     * on this cx contributes to cx->thread->requestDepth and there is no
     * JS_SuspendRequest calls that set aside the counter.
     */
    JS_ASSERT(cx->outstandingRequests <= cx->thread->requestDepth);
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
    if (cx->thread->requestDepth == 0)
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
            if (cx->thread->requestDepth == 0)
                JS_BeginRequest(cx);
#endif

            Shape::finishRuntimeState(cx);
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
        while (cx->outstandingRequests != 0)
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
#ifdef DEBUG
    JSThread *t = cx->thread;
#endif
    js_ClearContextThread(cx);
    JS_ASSERT_IF(JS_CLIST_IS_EMPTY(&t->contextList), !t->requestDepth);
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
    cx->regExpStatics.clear();
    VOUCH_DOES_NOT_REQUIRE_STACK();
    JS_FinishArenaPool(&cx->tempPool);
    JS_FinishArenaPool(&cx->regExpPool);

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
        if (cx->outstandingRequests && cx->thread->requestDepth)
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

    return (JSDHashNumber(uintptr_t(key->obj)) >> JS_GCTHING_ALIGN) ^ JSID_BITS(key->id);
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
    for (JSStackFrame *fp = js_GetTopStackFrame(cx); fp; fp = fp->prev()) {
        if (fp->pc(cx)) {
            report->filename = fp->script()->filename;
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

JS_FRIEND_API(void)
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
        if (fp && fp->script()->strictModeCode)
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

    cx->free(bytes);
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
        atom = GET_FUNCTION_PRIVATE(cx, &v.toObject())->atom;
        bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK,
                                        v, ATOM_TO_STRING(atom));
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
    JS_ASSERT_REQUEST_DEPTH(cx);
    JS_ASSERT(JS_THREAD_DATA(cx)->interruptFlags & JSThreadData::INTERRUPT_OPERATION_CALLBACK);

    /*
     * Reset the callback flag first, then yield. If another thread is racing
     * us here we will accumulate another callback request which will be
     * serviced at the next opportunity.
     */
    JS_ATOMIC_CLEAR_MASK(&JS_THREAD_DATA(cx)->interruptFlags,
                         JSThreadData::INTERRUPT_OPERATION_CALLBACK);

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

JSBool
js_HandleExecutionInterrupt(JSContext *cx)
{
    JSBool result = JS_TRUE;
    if (JS_THREAD_DATA(cx)->interruptFlags & JSThreadData::INTERRUPT_OPERATION_CALLBACK)
        result = js_InvokeOperationCallback(cx) && result;
    return result;
}

namespace js {

void
TriggerAllOperationCallbacks(JSRuntime *rt)
{
    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->triggerOperationCallback();
}

} /* namespace js */

JSStackFrame *
js_GetScriptedCaller(JSContext *cx, JSStackFrame *fp)
{
    if (!fp)
        fp = js_GetTopStackFrame(cx);
    while (fp && fp->isDummyFrame())
        fp = fp->prev();
    JS_ASSERT_IF(fp, fp->isScriptFrame());
    return fp;
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
        imacpc = cx->fp()->maybeImacropc();
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
    if (JS_ON_TRACE(cx))
        return cx->bailExit->imacpc != NULL;
    return cx->fp()->hasImacropc();
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
    oldOffsetMilliseconds = 0;
    oldRangeStartSeconds = oldRangeEndSeconds = INT64_MIN;

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
    regs(NULL),
    regExpStatics(this),
    busyArrays(this)
{}

void
JSContext::pushSegmentAndFrame(js::StackSegment *newseg, JSFrameRegs &newregs)
{
    JS_ASSERT(regs != &newregs);
    if (hasActiveSegment()) {
        JS_ASSERT(regs->fp->savedpc_ == JSStackFrame::sInvalidpc);
        regs->fp->savedpc_ = regs->pc;
        currentSegment->suspend(regs);
    }
    newseg->setPreviousInContext(currentSegment);
    currentSegment = newseg;
#ifdef DEBUG
    newregs.fp->savedpc_ = JSStackFrame::sInvalidpc;
#endif
    setCurrentRegs(&newregs);
    newseg->joinContext(this, newregs.fp);
}

void
JSContext::popSegmentAndFrame()
{
    JS_ASSERT(currentSegment->maybeContext() == this);
    JS_ASSERT(currentSegment->getInitialFrame() == regs->fp);
    JS_ASSERT(regs->fp->savedpc_ == JSStackFrame::sInvalidpc);
    currentSegment->leaveContext();
    currentSegment = currentSegment->getPreviousInContext();
    if (currentSegment) {
        if (currentSegment->isSaved()) {
            setCurrentRegs(NULL);
        } else {
            setCurrentRegs(currentSegment->getSuspendedRegs());
            currentSegment->resume();
#ifdef DEBUG
            regs->fp->savedpc_ = JSStackFrame::sInvalidpc;
#endif
        }
    } else {
        JS_ASSERT(regs->fp->prev() == NULL);
        setCurrentRegs(NULL);
    }
}

void
JSContext::saveActiveSegment()
{
    JS_ASSERT(hasActiveSegment());
    currentSegment->save(regs);
    JS_ASSERT(regs->fp->savedpc_ == JSStackFrame::sInvalidpc);
    regs->fp->savedpc_ = regs->pc;
    setCurrentRegs(NULL);
}

void
JSContext::restoreSegment()
{
    js::StackSegment *ccs = currentSegment;
    setCurrentRegs(ccs->getSuspendedRegs());
    ccs->restore();
#ifdef DEBUG
    regs->fp->savedpc_ = JSStackFrame::sInvalidpc;
#endif
}

JSGenerator *
JSContext::generatorFor(JSStackFrame *fp) const
{
    JS_ASSERT(stack().contains(fp) && fp->isGeneratorFrame());
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

StackSegment *
JSContext::containingSegment(const JSStackFrame *target)
{
    /* The context may have nothing running. */
    StackSegment *seg = currentSegment;
    if (!seg)
        return NULL;

    /* The active segments's top frame is cx->regs->fp. */
    if (regs) {
        JS_ASSERT(regs->fp);
        JS_ASSERT(activeSegment() == seg);
        JSStackFrame *f = regs->fp;
        JSStackFrame *stop = seg->getInitialFrame()->prev();
        for (; f != stop; f = f->prev()) {
            if (f == target)
                return seg;
        }
        seg = seg->getPreviousInContext();
    }

    /* A suspended segment's top frame is its suspended frame. */
    for (; seg; seg = seg->getPreviousInContext()) {
        JSStackFrame *f = seg->getSuspendedFrame();
        JSStackFrame *stop = seg->getInitialFrame()->prev();
        for (; f != stop; f = f->prev()) {
            if (f == target)
                return seg;
        }
    }

    return NULL;
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
    TriggerGC(this);
}

JS_FRIEND_API(void *)
JSRuntime::onOutOfMemory(void *p, size_t nbytes, JSContext *cx)
{
#ifdef JS_THREADSAFE
    gcHelperThread.waitBackgroundSweepEnd(this);
    if (!p)
        p = ::js_malloc(nbytes);
    else if (p == reinterpret_cast<void *>(1))
        p = ::js_calloc(nbytes);
    else
      p = ::js_realloc(p, nbytes);
    if (p)
        return p;
#endif
    if (cx)
        js_ReportOutOfMemory(cx);
    return NULL;
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
    FreeOldArenas(runtime, &regExpPool);
    /* FIXME: bug 586161 */
    compartment->purge(this);
}

namespace js {

void
SetPendingException(JSContext *cx, const Value &v)
{
    cx->throwing = JS_TRUE;
    cx->exception = v;
}

} /* namespace js */
