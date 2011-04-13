/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Luke Wagner <luke@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jsgcmark.h"
#include "methodjit/MethodJIT.h"
#include "Stack.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "Stack-inl.h"

/* Includes to get to low-level memory-mapping functionality. */
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

/*****************************************************************************/

#ifdef DEBUG
JSObject *const StackFrame::sInvalidScopeChain = (JSObject *)0xbeef;
#endif

jsbytecode *
StackFrame::prevpcSlow()
{
    JS_ASSERT(!(flags_ & HAS_PREVPC));
#if defined(JS_METHODJIT) && defined(JS_MONOIC)
    StackFrame *p = prev();
    js::mjit::JITScript *jit = p->script()->getJIT(p->isConstructing());
    prevpc_ = jit->nativeToPC(ncode_);
    flags_ |= HAS_PREVPC;
    return prevpc_;
#else
    JS_NOT_REACHED("Unknown PC for frame");
    return NULL;
#endif
}

jsbytecode *
StackFrame::pc(JSContext *cx, StackFrame *next)
{
    JS_ASSERT_IF(next, next->prev() == this);

    StackSegment &seg = cx->stack.space().containingSegment(this);
    FrameRegs &regs = seg.currentRegs();
    if (regs.fp() == this)
        return regs.pc;
    if (!next)
        next = seg.computeNextFrame(this);
    return next->prevpc();
}

/*****************************************************************************/

JS_REQUIRES_STACK bool
StackSegment::contains(const StackFrame *fp) const
{
    JS_ASSERT(!empty());

    if (fp < initialFrame_)
        return false;

    StackFrame *start;
    if (isActive())
        start = stack_->fp();
    else
        start = suspendedRegs_->fp();

    if (fp > start)
        return false;

#ifdef DEBUG
    bool found = false;
    StackFrame *stop = initialFrame_->prev();
    for (StackFrame *f = start; !found && f != stop; f = f->prev()) {
        if (f == fp) {
            found = true;
            break;
        }
    }
    JS_ASSERT(found);
#endif

    return true;
}

StackFrame *
StackSegment::computeNextFrame(StackFrame *fp) const
{
    JS_ASSERT(contains(fp));
    JS_ASSERT(fp != currentFrame());

    StackFrame *next = currentFrame();
    StackFrame *prev;
    while ((prev = next->prev()) != fp)
        next = prev;
    return next;
}

/*****************************************************************************/

StackSpace::StackSpace()
  : base_(NULL),
#ifdef XP_WIN
    commitEnd_(NULL),
#endif
    end_(NULL),
    seg_(NULL)
{
    override_.top = NULL;
#ifdef DEBUG
    override_.seg = NULL;
    override_.frame = NULL;
#endif
}

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
    base_ = reinterpret_cast<Value *>(p);
    commitEnd_ = base_ + COMMIT_VALS;
    end_ = base_ + CAPACITY_VALS;
#elif defined(XP_OS2)
    if (DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_ANY) &&
        DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE))
        return false;
    base_ = reinterpret_cast<Value *>(p);
    end_ = base_ + CAPACITY_VALS;
#else
    JS_ASSERT(CAPACITY_BYTES % getpagesize() == 0);
    p = mmap(NULL, CAPACITY_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base_ = reinterpret_cast<Value *>(p);
    end_ = base_ + CAPACITY_VALS;
#endif
    return true;
}

StackSpace::~StackSpace()
{
    JS_ASSERT(!seg_);
    if (!base_)
        return;
#ifdef XP_WIN
    VirtualFree(base_, (commitEnd_ - base_) * sizeof(Value), MEM_DECOMMIT);
    VirtualFree(base_, 0, MEM_RELEASE);
#elif defined(XP_OS2)
    DosFreeMem(base_);
#else
#ifdef SOLARIS
    munmap((caddr_t)base_, CAPACITY_BYTES);
#else
    munmap(base_, CAPACITY_BYTES);
#endif
#endif
}

Value *
StackSpace::firstUnused() const
{
    if (!seg_) {
        JS_ASSERT(override_.top == NULL);
        return base_;
    }
    if (!seg_->empty()) {
        Value *sp = seg_->currentRegs().sp;
        if (override_.top > sp) {
            JS_ASSERT(override_.seg == seg_);
            JS_ASSERT_IF(seg_->isActive(), override_.frame == seg_->stack().fp());
            JS_ASSERT_IF(!seg_->isActive(), override_.frame == seg_->suspendedFrame());
            return override_.top;
        }
        return sp;
    }
    JS_ASSERT(override_.seg == seg_);
    return override_.top;
}

StackSegment &
StackSpace::containingSegment(const StackFrame *target) const
{
    for (StackSegment *s = seg_; s; s = s->previousInMemory()) {
        if (s->contains(target))
            return *s;
    }
    JS_NOT_REACHED("frame not in stack space");
    return *(StackSegment *)NULL;
}

JSObject &
StackSpace::varObjForFrame(const StackFrame *fp)
{
    if (fp->hasCallObj())
        return fp->callObj();
    return containingSegment(fp).initialVarObj();
}

void
StackSpace::mark(JSTracer *trc)
{
    /*
     * JIT code can leave values in an incoherent (i.e., unsafe for precise
     * marking) state, hence MarkStackRangeConservatively.
     */
    Value *end = firstUnused();
    for (StackSegment *seg = seg_; seg; seg = seg->previousInMemory()) {
        STATIC_ASSERT(ubound(end) >= 0);
        if (seg->empty()) {
            /* Mark slots/args trailing off segment. */
            MarkStackRangeConservatively(trc, seg->valueRangeBegin(), end);
        } else {
            /* This may be the only pointer to the initialVarObj. */
            if (seg->hasInitialVarObj())
                gc::MarkObject(trc, seg->initialVarObj(), "varobj");

            /* Mark slots/args trailing off of the last stack frame. */
            StackFrame *fp = seg->currentFrame();
            MarkStackRangeConservatively(trc, fp->slots(), end);

            /* Mark stack frames and slots/args between stack frames. */
            StackFrame *initial = seg->initialFrame();
            for (StackFrame *f = fp; f != initial; f = f->prev()) {
                js_TraceStackFrame(trc, f);
                MarkStackRangeConservatively(trc, f->prev()->slots(), (Value *)f);
            }

            /* Mark initial stack frame and leading args. */
            js_TraceStackFrame(trc, initial);
            MarkStackRangeConservatively(trc, seg->valueRangeBegin(), (Value *)initial);
        }
        end = (Value *)seg;
    }
}

#ifdef XP_WIN
JS_FRIEND_API(bool)
StackSpace::bumpCommit(JSContext *maybecx, Value *from, ptrdiff_t nvals) const
{
    if (end_ - from < nvals) {
        js_ReportOutOfScriptQuota(maybecx);
        return false;
    }

    Value *newCommit = commitEnd_;
    Value *request = from + nvals;

    /* Use a dumb loop; will probably execute once. */
    JS_ASSERT((end_ - newCommit) % COMMIT_VALS == 0);
    do {
        newCommit += COMMIT_VALS;
        JS_ASSERT((end_ - newCommit) >= 0);
    } while (newCommit < request);

    /* The cast is safe because CAPACITY_BYTES is small. */
    int32 size = static_cast<int32>(newCommit - commitEnd_) * sizeof(Value);

    if (!VirtualAlloc(commitEnd_, size, MEM_COMMIT, PAGE_READWRITE)) {
        js_ReportOutOfScriptQuota(maybecx);
        return false;
    }

    commitEnd_ = newCommit;
    return true;
}
#endif

bool
StackSpace::bumpLimitWithinQuota(JSContext *maybecx, StackFrame *fp, Value *sp,
                                 uintN nvals, Value **limit) const
{
    JS_ASSERT(sp >= firstUnused());
    JS_ASSERT(sp + nvals >= *limit);
#ifdef XP_WIN
    Value *quotaEnd = (Value *)fp + STACK_QUOTA;
    if (sp + nvals < quotaEnd) {
        if (!ensureSpace(NULL, sp, nvals))
            goto fail;
        *limit = Min(quotaEnd, commitEnd_);
        return true;
    }
  fail:
#endif
    js_ReportOverRecursed(maybecx);
    return false;
}

bool
StackSpace::bumpLimit(JSContext *cx, StackFrame *fp, Value *sp,
                      uintN nvals, Value **limit) const
{
    JS_ASSERT(*limit > base_);
    JS_ASSERT(sp < *limit);

    /*
     * Ideally, we would only ensure space for 'nvals', not 'nvals + remain',
     * since this is ~500K. However, this whole call should be a rare case: some
     * script is passing a obscene number of args to 'apply' and we are just
     * trying to keep the stack limit heuristic from breaking the script.
     */
    Value *quota = (Value *)fp + STACK_QUOTA;
    uintN remain = quota - sp;
    uintN inc = nvals + remain;
    if (!ensureSpace(NULL, sp, inc))
        return false;
    *limit = sp + inc;
    return true;
}

void
StackSpace::popSegment()
{
    JS_ASSERT(seg_->empty());
    seg_ = seg_->previousInMemory();
}

void
StackSpace::pushSegment(StackSegment &seg)
{
    JS_ASSERT(seg.empty());
    seg.setPreviousInMemory(seg_);
    seg_ = &seg;
}

/*****************************************************************************/

ContextStack::ContextStack(JSContext *cx)
  : regs_(NULL),
    seg_(NULL),
    space_(&JS_THREAD_DATA(cx)->stackSpace),
    cx_(cx)
{
    threadReset();
}

ContextStack::~ContextStack()
{
    JS_ASSERT(!regs_);
    JS_ASSERT(!seg_);
}

void
ContextStack::threadReset()
{
#ifdef JS_THREADSAFE
    if (cx_->thread())
        space_ = &JS_THREAD_DATA(cx_)->stackSpace;
    else
        space_ = NULL;
#else
    space_ = &JS_THREAD_DATA(cx_)->stackSpace;
#endif
}

#ifdef DEBUG
void
ContextStack::assertSegmentsInSync() const
{
    if (regs_) {
        JS_ASSERT(seg_->isActive());
        if (StackSegment *prev = seg_->previousInContext())
            JS_ASSERT(!prev->isActive());
    } else {
        JS_ASSERT_IF(seg_, !seg_->isActive());
    }
}

void
ContextStack::assertSpaceInSync() const
{
    JS_ASSERT(space_);
    JS_ASSERT(space_ == &JS_THREAD_DATA(cx_)->stackSpace);
}

bool
ContextStack::contains(const StackFrame *fp) const
{
    return &space().containingSegment(fp).stack() == this;
}
#endif

void
ContextStack::saveActiveSegment()
{
    JS_ASSERT(regs_);
    seg_->save(*regs_);
    regs_ = NULL;
    cx_->resetCompartment();
}

void
ContextStack::restoreSegment()
{
    regs_ = &seg_->suspendedRegs();
    seg_->restore();
    cx_->resetCompartment();
}

bool
ContextStack::getSegmentAndFrame(JSContext *cx, uintN vplen, uintN nslots,
                                 FrameGuard *frameGuard) const
{
    Value *start = space().firstUnused();
    uintN nvals = VALUES_PER_STACK_SEGMENT + vplen + VALUES_PER_STACK_FRAME + nslots;
    if (!space().ensureSpace(cx, start, nvals))
        return false;

    StackSegment *seg = new(start) StackSegment;
    Value *vp = seg->valueRangeBegin();

    frameGuard->seg_ = seg;
    frameGuard->vp_ = vp;
    frameGuard->fp_ = reinterpret_cast<StackFrame *>(vp + vplen);
    return true;
}

void
ContextStack::pushSegmentAndFrameImpl(FrameRegs &regs, StackSegment &seg)
{
    JS_ASSERT(&seg == space().currentSegment());

    if (regs_)
        seg_->suspend(*regs_);
    regs_ = &regs;

    seg.setPreviousInContext(seg_);
    seg_ = &seg;
    seg.joinContext(*this, *regs.fp());
}

void
ContextStack::pushSegmentAndFrame(FrameRegs &regs, FrameGuard *frameGuard)
{
    space().pushSegment(*frameGuard->seg_);
    pushSegmentAndFrameImpl(regs, *frameGuard->seg_);
    frameGuard->stack_ = this;
}

void
ContextStack::popSegmentAndFrameImpl()
{
    JS_ASSERT(isCurrentAndActive());
    JS_ASSERT(&seg_->stack() == this);
    JS_ASSERT(seg_->initialFrame() == regs_->fp());

    regs_->fp()->putActivationObjects();

    seg_->leaveContext();
    seg_ = seg_->previousInContext();
    if (seg_) {
        if (seg_->isSaved()) {
            regs_ = NULL;
        } else {
            regs_ = &seg_->suspendedRegs();
            seg_->resume();
        }
    } else {
        JS_ASSERT(regs_->fp()->prev() == NULL);
        regs_ = NULL;
    }
}

void
ContextStack::popSegmentAndFrame()
{
    popSegmentAndFrameImpl();
    space().popSegment();
    notifyIfNoCodeRunning();
}

FrameGuard::~FrameGuard()
{
    if (!pushed())
        return;
    JS_ASSERT(stack_->currentSegment() == seg_);
    JS_ASSERT(stack_->currentSegment()->currentFrame() == fp_);
    stack_->popSegmentAndFrame();
}

bool
ContextStack::getExecuteFrame(JSContext *cx, JSScript *script,
                              ExecuteFrameGuard *frameGuard) const
{
    if (!getSegmentAndFrame(cx, 2, script->nslots, frameGuard))
        return false;
    frameGuard->regs_.prepareToRun(frameGuard->fp(), script);
    return true;
}

void
ContextStack::pushExecuteFrame(JSObject *initialVarObj,
                               ExecuteFrameGuard *frameGuard)
{
    pushSegmentAndFrame(frameGuard->regs_, frameGuard);
    frameGuard->seg_->setInitialVarObj(initialVarObj);
}

bool
ContextStack::pushDummyFrame(JSContext *cx, JSObject &scopeChain,
                             DummyFrameGuard *frameGuard)
{
    if (!getSegmentAndFrame(cx, 0 /*vplen*/, 0 /*nslots*/, frameGuard))
        return false;

    StackFrame *fp = frameGuard->fp();
    fp->initDummyFrame(cx, scopeChain);
    frameGuard->regs_.initDummyFrame(fp);

    pushSegmentAndFrame(frameGuard->regs_, frameGuard);
    return true;
}

bool
ContextStack::getGeneratorFrame(JSContext *cx, uintN vplen, uintN nslots,
                                GeneratorFrameGuard *frameGuard)
{
    /* The regs will be set by SendToGenerator. */
    return getSegmentAndFrame(cx, vplen, nslots, frameGuard);
}

void
ContextStack::pushGeneratorFrame(FrameRegs &regs,
                                 GeneratorFrameGuard *frameGuard)
{
    JS_ASSERT(regs.fp() == frameGuard->fp());
    JS_ASSERT(regs.fp()->prev() == regs_->fp());

    pushSegmentAndFrame(regs, frameGuard);
}

bool
ContextStack::pushInvokeArgsSlow(JSContext *cx, uintN argc,
                                 InvokeArgsGuard *argsGuard)
{
    /*
     * Either there is no code running on this context or its not at the top of
     * the contiguous stack. Either way, push a new empty segment which will
     * root the args for invoke and later contain the frame pushed by Invoke.
     */
    JS_ASSERT(!isCurrentAndActive());

    Value *start = space().firstUnused();
    size_t vplen = 2 + argc;
    ptrdiff_t nvals = VALUES_PER_STACK_SEGMENT + vplen;
    if (!space().ensureSpace(cx, start, nvals))
        return false;

    StackSegment *seg = new(start) StackSegment;
    argsGuard->seg_ = seg;

    Value *vp = seg->valueRangeBegin();
    ImplicitCast<CallArgs>(*argsGuard) = CallArgsFromVp(argc, vp);

    /*
     * Use stack override to root vp until the frame is pushed. Don't need to
     * MakeRangeGCSafe: the VM stack is conservatively marked.
     */
    space().pushSegment(*seg);
    space().pushOverride(vp + vplen, &argsGuard->prevOverride_);

    argsGuard->stack_ = this;
    return true;
}

void
ContextStack::popInvokeArgsSlow(const InvokeArgsGuard &argsGuard)
{
    JS_ASSERT(space().currentSegment()->empty());

    space().popOverride(argsGuard.prevOverride_);
    space().popSegment();
    notifyIfNoCodeRunning();
}

void
ContextStack::pushInvokeFrameSlow(InvokeFrameGuard *frameGuard)
{
    JS_ASSERT(space().seg_->empty());
    pushSegmentAndFrameImpl(frameGuard->regs_, *space().seg_);
    frameGuard->stack_ = this;
}

void
ContextStack::popInvokeFrameSlow(const InvokeFrameGuard &frameGuard)
{
    JS_ASSERT(frameGuard.regs_.fp() == seg_->initialFrame());
    popSegmentAndFrameImpl();
}

/*
 * NB: this function can call out and observe the stack (e.g., through GC), so
 * it should only be called from a consistent stack state.
 */
void
ContextStack::notifyIfNoCodeRunning()
{
    if (regs_)
        return;

    cx_->resetCompartment();
    cx_->maybeMigrateVersionOverride();
}

/*****************************************************************************/

void
FrameRegsIter::initSlow()
{
    if (!seg_) {
        fp_ = NULL;
        sp_ = NULL;
        pc_ = NULL;
        return;
    }

    JS_ASSERT(seg_->isSuspended());
    fp_ = seg_->suspendedFrame();
    sp_ = seg_->suspendedRegs().sp;
    pc_ = seg_->suspendedRegs().pc;
}

/*
 * Using the invariant described in the js::StackSegment comment, we know that,
 * when a pair of prev-linked stack frames are in the same segment, the
 * first frame's address is the top of the prev-frame's stack, modulo missing
 * arguments.
 */
void
FrameRegsIter::incSlow(StackFrame *oldfp)
{
    JS_ASSERT(oldfp == seg_->initialFrame());
    JS_ASSERT(fp_ == oldfp->prev());

    /*
     * Segments from arbitrary context stacks can interleave so we must do a
     * linear scan over segments in this context stack. Furthermore, 'prev' can
     * be any frame in the segment (not only the suspendedFrame), so we must
     * scan each stack frame in each segment. Fortunately, this is not hot code.
     */
    seg_ = seg_->previousInContext();
    sp_ = seg_->suspendedRegs().sp;
    pc_ = seg_->suspendedRegs().pc;
    StackFrame *f = seg_->suspendedFrame();
    while (f != fp_) {
        if (f == seg_->initialFrame()) {
            seg_ = seg_->previousInContext();
            sp_ = seg_->suspendedRegs().sp;
            pc_ = seg_->suspendedRegs().pc;
            f = seg_->suspendedFrame();
        } else {
            sp_ = f->formalArgsEnd();
            pc_ = f->prevpc();
            f = f->prev();
        }
    }
}

/*****************************************************************************/

AllFramesIter::AllFramesIter(JSContext *cx)
  : seg_(cx->stack.currentSegment()),
    fp_(seg_ ? seg_->currentFrame() : NULL)
{
}

AllFramesIter&
AllFramesIter::operator++()
{
    JS_ASSERT(!done());
    if (fp_ == seg_->initialFrame()) {
        seg_ = seg_->previousInMemory();
        fp_ = seg_ ? seg_->currentFrame() : NULL;
    } else {
        fp_ = fp_->prev();
    }
    return *this;
}

