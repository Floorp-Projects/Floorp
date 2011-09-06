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

void
StackFrame::initExecuteFrame(JSScript *script, StackFrame *prev, FrameRegs *regs,
                             const Value &thisv, JSObject &scopeChain, ExecuteType type)
{
    /*
     * See encoding of ExecuteType. When GLOBAL isn't set, we are executing a
     * script in the context of another frame and the frame type is determined
     * by the context.
     */
    flags_ = type | HAS_SCOPECHAIN | HAS_PREVPC;
    if (!(flags_ & GLOBAL))
        flags_ |= (prev->flags_ & (FUNCTION | GLOBAL));

    Value *dstvp = (Value *)this - 2;
    dstvp[1] = thisv;

    if (isFunctionFrame()) {
        dstvp[0] = prev->calleev();
        exec = prev->exec;
        args.script = script;
    } else {
        JS_ASSERT(isGlobalFrame());
        dstvp[0] = NullValue();
        exec.script = script;
#ifdef DEBUG
        args.script = (JSScript *)0xbad;
#endif
    }

    scopeChain_ = &scopeChain;
    prev_ = prev;
    prevpc_ = regs ? regs->pc : (jsbytecode *)0xbad;
    prevInline_ = regs ? regs->inlined() : NULL;

#ifdef DEBUG
    ncode_ = (void *)0xbad;
    Debug_SetValueRangeToCrashOnTouch(&rval_, 1);
    hookData_ = (void *)0xbad;
    annotation_ = (void *)0xbad;
#endif

    if (prev && prev->annotation())
        setAnnotation(prev->annotation());
}

void
StackFrame::initDummyFrame(JSContext *cx, JSObject &chain)
{
    PodZero(this);
    flags_ = DUMMY | HAS_PREVPC | HAS_SCOPECHAIN;
    initPrev(cx);
    JS_ASSERT(chain.isGlobal());
    setScopeChainNoCallObj(chain);
}

void
StackFrame::stealFrameAndSlots(Value *vp, StackFrame *otherfp,
                               Value *othervp, Value *othersp)
{
    JS_ASSERT(vp == (Value *)this - ((Value *)otherfp - othervp));
    JS_ASSERT(othervp == otherfp->actualArgs() - 2);
    JS_ASSERT(othersp >= otherfp->slots());
    JS_ASSERT(othersp <= otherfp->base() + otherfp->numSlots());

    PodCopy(vp, othervp, othersp - othervp);
    JS_ASSERT(vp == this->actualArgs() - 2);

    /* Catch bad-touching of non-canonical args (e.g., generator_trace). */
    if (otherfp->hasOverflowArgs())
        Debug_SetValueRangeToCrashOnTouch(othervp, othervp + 2 + otherfp->numFormalArgs());

    /*
     * Repoint Call, Arguments, Block and With objects to the new live frame.
     * Call and Arguments are done directly because we have pointers to them.
     * Block and With objects are done indirectly through 'liveFrame'. See
     * js_LiveFrameToFloating comment in jsiter.h.
     */
    if (hasCallObj()) {
        JSObject &obj = callObj();
        obj.setPrivate(this);
        otherfp->flags_ &= ~HAS_CALL_OBJ;
        if (js_IsNamedLambda(fun())) {
            JSObject *env = obj.getParent();
            JS_ASSERT(env->isDeclEnv());
            env->setPrivate(this);
        }
    }
    if (hasArgsObj()) {
        ArgumentsObject &argsobj = argsObj();
        if (argsobj.isNormalArguments())
            argsobj.setPrivate(this);
        else
            JS_ASSERT(!argsobj.getPrivate());
        otherfp->flags_ &= ~HAS_ARGS_OBJ;
    }
}

#ifdef DEBUG
JSObject *const StackFrame::sInvalidScopeChain = (JSObject *)0xbeef;
#endif

jsbytecode *
StackFrame::prevpcSlow(JSInlinedSite **pinlined)
{
    JS_ASSERT(!(flags_ & HAS_PREVPC));
#if defined(JS_METHODJIT) && defined(JS_MONOIC)
    StackFrame *p = prev();
    mjit::JITScript *jit = p->script()->getJIT(p->isConstructing());
    prevpc_ = jit->nativeToPC(ncode_, &prevInline_);
    flags_ |= HAS_PREVPC;
    if (pinlined)
        *pinlined = prevInline_;
    return prevpc_;
#else
    JS_NOT_REACHED("Unknown PC for frame");
    return NULL;
#endif
}

jsbytecode *
StackFrame::pcQuadratic(const ContextStack &stack, StackFrame *next, JSInlinedSite **pinlined)
{
    JS_ASSERT_IF(next, next->prev() == this);

    StackSegment &seg = stack.space().containingSegment(this);
    FrameRegs &regs = seg.regs();

    /*
     * This isn't just an optimization; seg->computeNextFrame(fp) is only
     * defined if fp != seg->currentFrame.
     */
    if (regs.fp() == this) {
        if (pinlined)
            *pinlined = regs.inlined();
        return regs.pc;
    }

    if (!next)
        next = seg.computeNextFrame(this);
    return next->prevpc(pinlined);
}

/*****************************************************************************/

bool
StackSegment::contains(const StackFrame *fp) const
{
    /* NB: this depends on the continuity of segments in memory. */
    return (Value *)fp >= slotsBegin() && (Value *)fp <= (Value *)maybefp();
}

bool
StackSegment::contains(const FrameRegs *regs) const
{
    return regs && contains(regs->fp());
}

bool
StackSegment::contains(const CallArgsList *call) const
{
    if (!call || !calls_)
        return false;

    /* NB: this depends on the continuity of segments in memory. */
    Value *vp = call->argv();
    bool ret = vp > slotsBegin() && vp <= calls_->argv();

    /*
     * :XXX: Disabled. Including this check changes the asymptotic complexity
     * of code which calls this function.
     */
#if 0
#ifdef DEBUG
    bool found = false;
    for (CallArgsList *c = maybeCalls(); c->argv() > slotsBegin(); c = c->prev()) {
        if (c == call) {
            found = true;
            break;
        }
    }
    JS_ASSERT(found == ret);
#endif
#endif

    return ret;
}

StackFrame *
StackSegment::computeNextFrame(const StackFrame *f) const
{
    JS_ASSERT(contains(f) && f != fp());

    StackFrame *next = fp();
    StackFrame *prev;
    while ((prev = next->prev()) != f)
        next = prev;
    return next;
}

Value *
StackSegment::end() const
{
    /* NB: this depends on the continuity of segments in memory. */
    JS_ASSERT_IF(calls_ || regs_, contains(calls_) || contains(regs_));
    Value *p = calls_
               ? regs_
                 ? Max(regs_->sp, calls_->end())
                 : calls_->end()
               : regs_
                 ? regs_->sp
                 : slotsBegin();
    JS_ASSERT(p >= slotsBegin());
    return p;
}

FrameRegs *
StackSegment::pushRegs(FrameRegs &regs)
{
    JS_ASSERT_IF(contains(regs_), regs.fp()->prev() == regs_->fp());
    FrameRegs *prev = regs_;
    regs_ = &regs;
    return prev;
}

void
StackSegment::popRegs(FrameRegs *regs)
{
    JS_ASSERT_IF(regs && contains(regs->fp()), regs->fp() == regs_->fp()->prev());
    regs_ = regs;
}

void
StackSegment::pushCall(CallArgsList &callList)
{
    callList.prev_ = calls_;
    calls_ = &callList;
}

void
StackSegment::pointAtCall(CallArgsList &callList)
{
    calls_ = &callList;
}

void
StackSegment::popCall()
{
    calls_ = calls_->prev_;
}

/*****************************************************************************/

StackSpace::StackSpace()
  : seg_(NULL),
    base_(NULL),
    conservativeEnd_(NULL),
#ifdef XP_WIN
    commitEnd_(NULL),
#endif
    defaultEnd_(NULL),
    trustedEnd_(NULL)
{
    assertInvariants();
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
    conservativeEnd_ = commitEnd_ = base_ + COMMIT_VALS;
    trustedEnd_ = base_ + CAPACITY_VALS;
    defaultEnd_ = trustedEnd_ - BUFFER_VALS;
#elif defined(XP_OS2)
    if (DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_ANY) &&
        DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE))
        return false;
    base_ = reinterpret_cast<Value *>(p);
    trustedEnd_ = base_ + CAPACITY_VALS;
    conservativeEnd_ = defaultEnd_ = trustedEnd_ - BUFFER_VALS;
#else
    JS_ASSERT(CAPACITY_BYTES % getpagesize() == 0);
    p = mmap(NULL, CAPACITY_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base_ = reinterpret_cast<Value *>(p);
    trustedEnd_ = base_ + CAPACITY_VALS;
    conservativeEnd_ = defaultEnd_ = trustedEnd_ - BUFFER_VALS;
#endif
    assertInvariants();
    return true;
}

StackSpace::~StackSpace()
{
    assertInvariants();
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

StackSegment &
StackSpace::containingSegment(const StackFrame *target) const
{
    for (StackSegment *s = seg_; s; s = s->prevInMemory()) {
        if (s->contains(target))
            return *s;
    }
    JS_NOT_REACHED("frame not in stack space");
    return *(StackSegment *)NULL;
}

void
StackSpace::mark(JSTracer *trc)
{
    /*
     * JIT code can leave values in an incoherent (i.e., unsafe for precise
     * marking) state, hence MarkStackRangeConservatively.
     */

    /* NB: this depends on the continuity of segments in memory. */
    Value *nextSegEnd = firstUnused();
    for (StackSegment *seg = seg_; seg; seg = seg->prevInMemory()) {
        /*
         * A segment describes a linear region of memory that contains a stack
         * of native and interpreted calls. For marking purposes, though, we
         * only need to distinguish between frames and values and mark
         * accordingly. Since native calls only push values on the stack, we
         * can effectively lump them together and just iterate over interpreted
         * calls. Thus, marking can view the stack as the regex:
         *   (segment slots (frame slots)*)*
         * which gets marked in reverse order.
         *
         */
        Value *slotsEnd = nextSegEnd;
        for (StackFrame *fp = seg->maybefp(); (Value *)fp > (Value *)seg; fp = fp->prev()) {
            MarkStackRangeConservatively(trc, fp->slots(), slotsEnd);
            js_TraceStackFrame(trc, fp);
            slotsEnd = (Value *)fp;
        }
        MarkStackRangeConservatively(trc, seg->slotsBegin(), slotsEnd);
        nextSegEnd = (Value *)seg;
    }
}

JS_FRIEND_API(bool)
StackSpace::ensureSpaceSlow(JSContext *cx, MaybeReportError report, Value *from, ptrdiff_t nvals,
                            JSCompartment *dest) const
{
    assertInvariants();

    /* See CX_COMPARTMENT comment. */
    if (dest == (JSCompartment *)CX_COMPARTMENT)
        dest = cx->compartment;

    bool trusted = !dest || dest->principals == cx->runtime->trustedPrincipals();
    Value *end = trusted ? trustedEnd_ : defaultEnd_;

    /*
     * conservativeEnd_ must stay below defaultEnd_: if conservativeEnd_ were
     * to be bumped past defaultEnd_, untrusted JS would be able to consume the
     * buffer space at the end of the stack reserved for trusted JS.
     */

    if (end - from < nvals) {
        if (report)
            js_ReportOverRecursed(cx);
        return false;
    }

#ifdef XP_WIN
    if (commitEnd_ - from < nvals) {
        Value *newCommit = commitEnd_;
        Value *request = from + nvals;

        /* Use a dumb loop; will probably execute once. */
        JS_ASSERT((trustedEnd_ - newCommit) % COMMIT_VALS == 0);
        do {
            newCommit += COMMIT_VALS;
            JS_ASSERT((trustedEnd_ - newCommit) >= 0);
        } while (newCommit < request);

        /* The cast is safe because CAPACITY_BYTES is small. */
        int32 size = static_cast<int32>(newCommit - commitEnd_) * sizeof(Value);

        if (!VirtualAlloc(commitEnd_, size, MEM_COMMIT, PAGE_READWRITE)) {
            if (report)
                js_ReportOverRecursed(cx);
            return false;
        }

        commitEnd_ = newCommit;
        conservativeEnd_ = Min(commitEnd_, defaultEnd_);
        assertInvariants();
    }
#endif

    return true;
}

bool
StackSpace::tryBumpLimit(JSContext *cx, Value *from, uintN nvals, Value **limit)
{
    if (!ensureSpace(cx, REPORT_ERROR, from, nvals))
        return false;
    *limit = conservativeEnd_;
    return true;
}

size_t
StackSpace::committedSize()
{
#ifdef XP_WIN
    return (commitEnd_ - base_) * sizeof(Value);
#else
    return (trustedEnd_ - base_) * sizeof(Value);
#endif
}

/*****************************************************************************/

ContextStack::ContextStack(JSContext *cx)
  : seg_(NULL),
    space_(&JS_THREAD_DATA(cx)->stackSpace),
    cx_(cx)
{
    threadReset();
}

ContextStack::~ContextStack()
{
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
ContextStack::assertSpaceInSync() const
{
    JS_ASSERT(space_);
    JS_ASSERT(space_ == &JS_THREAD_DATA(cx_)->stackSpace);
}
#endif

bool
ContextStack::onTop() const
{
    return seg_ && seg_ == space().seg_;
}

bool
ContextStack::containsSlow(const StackFrame *target) const
{
    for (StackSegment *s = seg_; s; s = s->prevInContext()) {
        if (s->contains(target))
            return true;
    }
    return false;
}

/*
 * This helper function brings the ContextStack to the top of the thread stack
 * (so that it can be extended to push a frame and/or arguments) by potentially
 * pushing a StackSegment. The 'pushedSeg' outparam indicates whether such a
 * segment was pushed (and hence whether the caller needs to call popSegment).
 *
 * Additionally, to minimize calls to ensureSpace, ensureOnTop ensures that
 * there is space for nvars slots on top of the stack.
 */
Value *
ContextStack::ensureOnTop(JSContext *cx, MaybeReportError report, uintN nvars,
                          MaybeExtend extend, bool *pushedSeg, JSCompartment *dest)
{
    Value *firstUnused = space().firstUnused();

#ifdef JS_METHODJIT
    /*
     * The only calls made by inlined methodjit frames can be to other JIT
     * frames associated with the same VMFrame. If we try to Invoke(),
     * Execute() or so forth, any topmost inline frame will need to be
     * expanded (along with other inline frames in the compartment).
     * To avoid pathological behavior here, make sure to mark any topmost
     * function as uninlineable, which will expand inline frames if there are
     * any and prevent the function from being inlined in the future.
     */
    if (FrameRegs *regs = cx->maybeRegs()) {
        JSFunction *fun = NULL;
        if (JSInlinedSite *site = regs->inlined()) {
            fun = regs->fp()->jit()->inlineFrames()[site->inlineIndex].fun;
        } else {
            StackFrame *fp = regs->fp();
            if (fp->isFunctionFrame()) {
                JSFunction *f = fp->fun();
                if (f->isInterpreted())
                    fun = f;
            }
        }

        if (fun) {
            fun->script()->uninlineable = true;
            types::MarkTypeObjectFlags(cx, fun, types::OBJECT_FLAG_UNINLINEABLE);
        }
    }
    JS_ASSERT_IF(cx->hasfp(), !cx->regs().inlined());
#endif

    if (onTop() && extend) {
        if (!space().ensureSpace(cx, report, firstUnused, nvars, dest))
            return NULL;
        return firstUnused;
    }

    if (!space().ensureSpace(cx, report, firstUnused, VALUES_PER_STACK_SEGMENT + nvars, dest))
        return NULL;

    FrameRegs *regs;
    CallArgsList *calls;
    if (seg_ && extend) {
        regs = seg_->maybeRegs();
        calls = seg_->maybeCalls();
    } else {
        regs = NULL;
        calls = NULL;
    }

    seg_ = new(firstUnused) StackSegment(seg_, space().seg_, regs, calls);
    space().seg_ = seg_;
    *pushedSeg = true;
    return seg_->slotsBegin();
}

void
ContextStack::popSegment()
{
    space().seg_ = seg_->prevInMemory();
    seg_ = seg_->prevInContext();

    if (!seg_)
        cx_->maybeMigrateVersionOverride();
}

bool
ContextStack::pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard *iag)
{
    LeaveTrace(cx);
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);

    uintN nvars = 2 + argc;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &iag->pushedSeg_);
    if (!firstUnused)
        return false;

    ImplicitCast<CallArgs>(*iag) = CallArgsFromVp(argc, firstUnused);

    seg_->pushCall(*iag);
    JS_ASSERT(space().firstUnused() == iag->end());
    iag->setPushed(*this);
    return true;
}

void
ContextStack::popInvokeArgs(const InvokeArgsGuard &iag)
{
    JS_ASSERT(iag.pushed());
    JS_ASSERT(onTop());
    JS_ASSERT(space().firstUnused() == seg_->calls().end());

    seg_->popCall();
    if (iag.pushedSeg_)
        popSegment();
}

bool
ContextStack::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                              InitialFrameFlags initial, InvokeFrameGuard *ifg)
{
    JS_ASSERT(onTop());
    JS_ASSERT(space().firstUnused() == args.end());

    JSObject &callee = args.callee();
    JSFunction *fun = callee.getFunctionPrivate();
    JSScript *script = fun->script();

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, REPORT_ERROR, args, fun, script, &flags);
    if (!fp)
        return false;

    fp->initCallFrame(cx, callee, fun, script, args.argc(), flags);
    ifg->regs_.prepareToRun(*fp, script);

    ifg->prevRegs_ = seg_->pushRegs(ifg->regs_);
    JS_ASSERT(space().firstUnused() == ifg->regs_.sp);
    ifg->setPushed(*this);
    return true;
}

bool
ContextStack::pushExecuteFrame(JSContext *cx, JSScript *script, const Value &thisv,
                               JSObject &scopeChain, ExecuteType type,
                               StackFrame *evalInFrame, ExecuteFrameGuard *efg)
{
    /*
     * Even though global code and indirect eval do not execute in the context
     * of the current frame, prev-link these to the current frame so that the
     * callstack looks right to the debugger (via CAN_EXTEND). This is safe
     * since the scope chain is what determines name lookup and access, not
     * prev-links.
     *
     * Eval-in-frame is the exception since it prev-links to an arbitrary frame
     * (possibly in the middle of some previous segment). Thus pass CANT_EXTEND
     * (to start a new segment) and link the frame and call chain manually
     * below.
     */
    CallArgsList *evalInFrameCalls = NULL;  /* quell overwarning */
    StackFrame *prev;
    MaybeExtend extend;
    if (evalInFrame) {
        /* Though the prev-frame is given, need to search for prev-call. */
        StackIter iter(cx, StackIter::GO_THROUGH_SAVED);
        while (!iter.isScript() || iter.fp() != evalInFrame)
            ++iter;
        evalInFrameCalls = iter.calls_;
        prev = evalInFrame;
        extend = CANT_EXTEND;
    } else {
        prev = maybefp();
        extend = CAN_EXTEND;
    }

    uintN nvars = 2 /* callee, this */ + VALUES_PER_STACK_FRAME + script->nslots;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, extend, &efg->pushedSeg_);
    if (!firstUnused)
        return NULL;

    StackFrame *fp = reinterpret_cast<StackFrame *>(firstUnused + 2);
    fp->initExecuteFrame(script, prev, seg_->maybeRegs(), thisv, scopeChain, type);
    SetValueRangeToUndefined(fp->slots(), script->nfixed);
    efg->regs_.prepareToRun(*fp, script);

    /* pushRegs() below links the prev-frame; manually link the prev-call. */
    if (evalInFrame && evalInFrameCalls)
        seg_->pointAtCall(*evalInFrameCalls);

    efg->prevRegs_ = seg_->pushRegs(efg->regs_);
    JS_ASSERT(space().firstUnused() == efg->regs_.sp);
    efg->setPushed(*this);
    return true;
}

bool
ContextStack::pushDummyFrame(JSContext *cx, JSCompartment *dest, JSObject &scopeChain, DummyFrameGuard *dfg)
{
    JS_ASSERT(dest == scopeChain.compartment());

    uintN nvars = VALUES_PER_STACK_FRAME;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &dfg->pushedSeg_, dest);
    if (!firstUnused)
        return NULL;

    StackFrame *fp = reinterpret_cast<StackFrame *>(firstUnused);
    fp->initDummyFrame(cx, scopeChain);
    dfg->regs_.initDummyFrame(*fp);

    cx->setCompartment(dest);
    dfg->prevRegs_ = seg_->pushRegs(dfg->regs_);
    JS_ASSERT(space().firstUnused() == dfg->regs_.sp);
    dfg->setPushed(*this);
    return true;
}

void
ContextStack::popFrame(const FrameGuard &fg)
{
    JS_ASSERT(fg.pushed());
    JS_ASSERT(onTop());
    JS_ASSERT(space().firstUnused() == fg.regs_.sp);
    JS_ASSERT(&fg.regs_ == &seg_->regs());

    fg.regs_.fp()->putActivationObjects();

    seg_->popRegs(fg.prevRegs_);
    if (fg.pushedSeg_)
        popSegment();

    /*
     * NB: this code can call out and observe the stack (e.g., through GC), so
     * it should only be called from a consistent stack state.
     */
    if (!hasfp())
        cx_->resetCompartment();
}

bool
ContextStack::pushGeneratorFrame(JSContext *cx, JSGenerator *gen, GeneratorFrameGuard *gfg)
{
    StackFrame *genfp = gen->floatingFrame();
    Value *genvp = gen->floatingStack;
    uintN vplen = (Value *)genfp - genvp;

    uintN nvars = vplen + VALUES_PER_STACK_FRAME + genfp->numSlots();
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &gfg->pushedSeg_);
    if (!firstUnused)
        return false;

    StackFrame *stackfp = reinterpret_cast<StackFrame *>(firstUnused + vplen);
    Value *stackvp = (Value *)stackfp - vplen;

    /* Save this for popGeneratorFrame. */
    gfg->gen_ = gen;
    gfg->stackvp_ = stackvp;

    /* Copy from the generator's floating frame to the stack. */
    stackfp->stealFrameAndSlots(stackvp, genfp, genvp, gen->regs.sp);
    stackfp->resetGeneratorPrev(cx);
    stackfp->unsetFloatingGenerator();
    gfg->regs_.rebaseFromTo(gen->regs, *stackfp);

    gfg->prevRegs_ = seg_->pushRegs(gfg->regs_);
    JS_ASSERT(space().firstUnused() == gfg->regs_.sp);
    gfg->setPushed(*this);
    return true;
}

void
ContextStack::popGeneratorFrame(const GeneratorFrameGuard &gfg)
{
    JSGenerator *gen = gfg.gen_;
    StackFrame *genfp = gen->floatingFrame();
    Value *genvp = gen->floatingStack;

    const FrameRegs &stackRegs = gfg.regs_;
    StackFrame *stackfp = stackRegs.fp();
    Value *stackvp = gfg.stackvp_;

    /* Copy from the stack to the generator's floating frame. */
    gen->regs.rebaseFromTo(stackRegs, *genfp);
    genfp->stealFrameAndSlots(genvp, stackfp, stackvp, stackRegs.sp);
    genfp->setFloatingGenerator();

    /* ~FrameGuard/popFrame will finish the popping. */
    JS_ASSERT(ImplicitCast<const FrameGuard>(gfg).pushed());
}

bool
ContextStack::saveFrameChain()
{
    JSCompartment *dest = NULL;

    bool pushedSeg;
    if (!ensureOnTop(cx_, REPORT_ERROR, 0, CANT_EXTEND, &pushedSeg, dest))
        return false;

    JS_ASSERT(pushedSeg);
    JS_ASSERT(!hasfp());
    JS_ASSERT(onTop() && seg_->isEmpty());

    cx_->resetCompartment();
    return true;
}

void
ContextStack::restoreFrameChain()
{
    JS_ASSERT(onTop() && seg_->isEmpty());

    popSegment();
    cx_->resetCompartment();
}

/*****************************************************************************/

void
StackIter::poisonRegs()
{
    sp_ = (Value *)0xbad;
    pc_ = (jsbytecode *)0xbad;
}

void
StackIter::popFrame()
{
    StackFrame *oldfp = fp_;
    JS_ASSERT(seg_->contains(oldfp));
    fp_ = fp_->prev();
    if (seg_->contains(fp_)) {
        JSInlinedSite *inline_;
        pc_ = oldfp->prevpc(&inline_);
        JS_ASSERT(!inline_);

        /*
         * If there is a CallArgsList element between oldfp and fp_, then sp_
         * is ignored, so we only consider the case where there is no
         * intervening CallArgsList. The stack representation is not optimized
         * for this operation so we need to do a full case analysis of how
         * frames are pushed by considering each ContextStack::push*Frame.
         */
        if (oldfp->isGeneratorFrame()) {
            /* Generator's args do not overlap with the caller's expr stack. */
            sp_ = (Value *)oldfp->actualArgs() - 2;
        } else if (oldfp->isNonEvalFunctionFrame()) {
            /*
             * When Invoke is called from a native, there will be an enclosing
             * pushInvokeArgs which pushes a CallArgsList element so we can
             * ignore that case. The other two cases of function call frames are
             * Invoke called directly from script and pushInlineFrmae. In both
             * cases, the actual arguments of the callee should be included in
             * the caller's expr stack.
             */
            sp_ = oldfp->actualArgsEnd();
        } else if (oldfp->isFramePushedByExecute()) {
            /* pushExecuteFrame pushes exactly (callee, this) before frame. */
            sp_ = (Value *)oldfp - 2;
        } else {
            /* pushDummyFrame pushes exactly 0 slots before frame. */
            JS_ASSERT(oldfp->isDummyFrame());
            sp_ = (Value *)oldfp;
        }
    } else {
        poisonRegs();
    }
}

void
StackIter::popCall()
{
    CallArgsList *oldCall = calls_;
    JS_ASSERT(seg_->contains(oldCall));
    calls_ = calls_->prev();
    if (seg_->contains(fp_)) {
        /* pc_ keeps its same value. */
        sp_ = oldCall->base();
    } else {
        poisonRegs();
    }
}

void
StackIter::settleOnNewSegment()
{
    if (FrameRegs *regs = seg_->maybeRegs()) {
        sp_ = regs->sp;
        pc_ = regs->pc;
    } else {
        poisonRegs();
    }
}

void
StackIter::startOnSegment(StackSegment *seg)
{
    seg_ = seg;
    fp_ = seg_->maybefp();
    calls_ = seg_->maybeCalls();
    settleOnNewSegment();
}

static void JS_NEVER_INLINE
CrashIfInvalidSlot(StackFrame *fp, Value *vp)
{
    if (vp < fp->slots() || vp >= fp->slots() + fp->script()->nslots) {
        JS_ASSERT(false && "About to dereference invalid slot");
        *(int *)0xbad = 0;  // show up nicely in crash-stats
        JS_Assert("About to dereference invalid slot", __FILE__, __LINE__);
    }
}

void
StackIter::settleOnNewState()
{
    /*
     * There are elements of the calls_ and fp_ chains that we want to skip
     * over so iterate until we settle on one or until there are no more.
     */
    while (true) {
        if (!fp_ && !calls_) {
            if (savedOption_ == GO_THROUGH_SAVED && seg_->prevInContext()) {
                startOnSegment(seg_->prevInContext());
                continue;
            }
            state_ = DONE;
            return;
        }

        /* Check if popFrame/popCall changed segment. */
        bool containsFrame = seg_->contains(fp_);
        bool containsCall = seg_->contains(calls_);
        while (!containsFrame && !containsCall) {
            seg_ = seg_->prevInContext();
            containsFrame = seg_->contains(fp_);
            containsCall = seg_->contains(calls_);

            /* Eval-in-frame allows jumping into the middle of a segment. */
            if (containsFrame && seg_->fp() != fp_) {
                /* Avoid duplicating logic; seg_ contains fp_, so no iloop. */
                StackIter tmp = *this;
                tmp.startOnSegment(seg_);
                while (!tmp.isScript() || tmp.fp() != fp_)
                    ++tmp;
                JS_ASSERT(tmp.state_ == SCRIPTED && tmp.seg_ == seg_ && tmp.fp_ == fp_);
                *this = tmp;
                return;
            }
            /* There is no eval-in-frame equivalent for native calls. */
            JS_ASSERT_IF(containsCall, &seg_->calls() == calls_);
            settleOnNewSegment();
        }

        /*
         * In case of both a scripted frame and call record, use linear memory
         * ordering to decide which was the most recent.
         */
        if (containsFrame && (!containsCall || (Value *)fp_ >= calls_->argv())) {
            /* Nobody wants to see dummy frames. */
            if (fp_->isDummyFrame()) {
                popFrame();
                continue;
            }

            /*
             * As an optimization, there is no CallArgsList element pushed for
             * natives called directly by a script (compiled or interpreted).
             * We catch these by inspecting the bytecode and stack. This check
             * relies on the property that, at a call opcode,
             *
             *   regs.sp == vp + 2 + argc
             *
             * The mjit Function.prototype.apply optimization breaks this
             * invariant (see ic::SplatApplyArgs). Thus, for JSOP_FUNAPPLY we
             * need to (slowly) reconstruct the depth.
             *
             * Additionally, the Function.prototype.{call,apply} optimizations
             * leave no record when 'this' is a native function. Thus, if the
             * following expression runs and breaks in the debugger, the call
             * to 'replace' will not appear on the callstack.
             *
             *   (String.prototype.replace).call('a',/a/,function(){debugger});
             *
             * Function.prototype.call will however appear, hence the debugger
             * can, by inspecting 'args.thisv', give some useful information.
             */
            JSOp op = js_GetOpcode(cx_, fp_->script(), pc_);
            if (op == JSOP_CALL || op == JSOP_FUNCALL) {
                uintN argc = GET_ARGC(pc_);
                DebugOnly<uintN> spoff = sp_ - fp_->base();
                JS_ASSERT_IF(cx_->stackIterAssertionEnabled && !fp_->hasImacropc(),
                             spoff == js_ReconstructStackDepth(cx_, fp_->script(), pc_));
                Value *vp = sp_ - (2 + argc);

                CrashIfInvalidSlot(fp_, vp);
                if (IsNativeFunction(*vp)) {
                    state_ = IMPLICIT_NATIVE;
                    args_ = CallArgsFromVp(argc, vp);
                    return;
                }
            } else if (op == JSOP_FUNAPPLY) {
                JS_ASSERT(!fp_->hasImacropc());
                uintN argc = GET_ARGC(pc_);
                uintN spoff = js_ReconstructStackDepth(cx_, fp_->script(), pc_);
                Value *sp = fp_->base() + spoff;
                Value *vp = sp - (2 + argc);

                CrashIfInvalidSlot(fp_, vp);
                if (IsNativeFunction(*vp)) {
                    if (sp_ != sp) {
                        JS_ASSERT(argc == 2);
                        JS_ASSERT(vp[0].toObject().getFunctionPrivate()->native() == js_fun_apply);
                        JS_ASSERT(sp_ >= vp + 3);
                        argc = sp_ - (vp + 2);
                    }
                    state_ = IMPLICIT_NATIVE;
                    args_ = CallArgsFromVp(argc, vp);
                    return;
                }
            }

            state_ = SCRIPTED;
            JS_ASSERT(sp_ >= fp_->base() && sp_ <= fp_->slots() + fp_->script()->nslots);
            DebugOnly<JSScript *> script = fp_->script();
            JS_ASSERT_IF(!fp_->hasImacropc(),
                         pc_ >= script->code && pc_ < script->code + script->length);
            return;
        }

        /*
         * A CallArgsList element is pushed for any call to Invoke, regardless
         * of whether the callee is a scripted function or even a callable
         * object. Thus, it is necessary to filter calleev for natives.
         *
         * Second, stuff can happen after the args are pushed but before/after
         * the actual call, so only consider "active" calls. (Since Invoke
         * necessarily clobbers the callee, "active" is also necessary to
         * ensure that the callee slot is valid.)
         */
        if (calls_->active() && IsNativeFunction(calls_->calleev())) {
            state_ = NATIVE;
            args_ = *calls_;
            return;
        }

        /* Pop the call and keep looking. */
        popCall();
    }
}

StackIter::StackIter(JSContext *cx, SavedOption savedOption)
  : cx_(cx),
    savedOption_(savedOption)
{
#ifdef JS_METHODJIT
    mjit::ExpandInlineFrames(cx->compartment);
#endif

    LeaveTrace(cx);

    if (StackSegment *seg = cx->stack.seg_) {
        startOnSegment(seg);
        settleOnNewState();
    } else {
        state_ = DONE;
    }
}

StackIter &
StackIter::operator++()
{
    JS_ASSERT(!done());
    switch (state_) {
      case DONE:
        JS_NOT_REACHED("");
      case SCRIPTED:
        popFrame();
        settleOnNewState();
        break;
      case NATIVE:
        popCall();
        settleOnNewState();
        break;
      case IMPLICIT_NATIVE:
        state_ = SCRIPTED;
        break;
    }
    return *this;
}

bool
StackIter::operator==(const StackIter &rhs) const
{
    return done() == rhs.done() &&
           (done() ||
            (isScript() == rhs.isScript() &&
             ((isScript() && fp() == rhs.fp()) ||
              (!isScript() && nativeArgs().base() == rhs.nativeArgs().base()))));
}

/*****************************************************************************/

AllFramesIter::AllFramesIter(StackSpace &space)
  : seg_(space.seg_),
    fp_(seg_ ? seg_->maybefp() : NULL)
{}

AllFramesIter&
AllFramesIter::operator++()
{
    JS_ASSERT(!done());
    fp_ = fp_->prev();
    if (!seg_->contains(fp_)) {
        seg_ = seg_->prevInMemory();
        while (seg_) {
            fp_ = seg_->maybefp();
            if (fp_)
                return *this;
            seg_ = seg_->prevInMemory();
        }
        JS_ASSERT(!fp_);
    }
    return *this;
}
