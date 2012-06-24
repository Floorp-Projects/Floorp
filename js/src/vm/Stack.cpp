/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "gc/Marking.h"
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
    flags_ = type | HAS_SCOPECHAIN | HAS_BLOCKCHAIN | HAS_PREVPC;
    if (!(flags_ & GLOBAL))
        flags_ |= (prev->flags_ & (FUNCTION | GLOBAL));

    Value *dstvp = (Value *)this - 2;
    dstvp[1] = thisv;

    if (isFunctionFrame()) {
        dstvp[0] = prev->calleev();
        exec = prev->exec;
        u.evalScript = script;
    } else {
        JS_ASSERT(isGlobalFrame());
        dstvp[0] = NullValue();
        exec.script = script;
#ifdef DEBUG
        u.evalScript = (JSScript *)0xbad;
#endif
    }

    scopeChain_ = &scopeChain;
    prev_ = prev;
    prevpc_ = regs ? regs->pc : (jsbytecode *)0xbad;
    prevInline_ = regs ? regs->inlined() : NULL;
    blockChain_ = NULL;

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
    scopeChain_ = &chain;
}

template <StackFrame::TriggerPostBarriers doPostBarrier>
void
StackFrame::copyFrameAndValues(JSContext *cx, Value *vp, StackFrame *otherfp,
                               const Value *othervp, Value *othersp)
{
    JS_ASSERT(vp == (Value *)this - ((Value *)otherfp - othervp));
    JS_ASSERT(othervp == otherfp->generatorArgsSnapshotBegin());
    JS_ASSERT(othersp >= otherfp->slots());
    JS_ASSERT(othersp <= otherfp->generatorSlotsSnapshotBegin() + otherfp->script()->nslots);
    JS_ASSERT((Value *)this - vp == (Value *)otherfp - othervp);

    /* Copy args, StackFrame, and slots. */
    const Value *srcend = otherfp->generatorArgsSnapshotEnd();
    Value *dst = vp;
    for (const Value *src = othervp; src < srcend; src++, dst++) {
        *dst = *src;
        if (doPostBarrier)
            HeapValue::writeBarrierPost(*dst, dst);
    }

    *this = *otherfp;
    if (doPostBarrier)
        writeBarrierPost();

    srcend = othersp;
    dst = slots();
    for (const Value *src = otherfp->slots(); src < srcend; src++, dst++) {
        *dst = *src;
        if (doPostBarrier)
            HeapValue::writeBarrierPost(*dst, dst);
    }

    if (cx->compartment->debugMode())
        cx->runtime->debugScopes->onGeneratorFrameChange(otherfp, this, cx);
}

/* Note: explicit instantiation for js_NewGenerator located in jsiter.cpp. */
template
void StackFrame::copyFrameAndValues<StackFrame::NoPostBarrier>(
                                    JSContext *, Value *, StackFrame *, const Value *, Value *);
template
void StackFrame::copyFrameAndValues<StackFrame::DoPostBarrier>(
                                    JSContext *, Value *, StackFrame *, const Value *, Value *);

void
StackFrame::writeBarrierPost()
{
    /* This needs to follow the same rules as in StackFrame::mark. */
    if (scopeChain_)
        JSObject::writeBarrierPost(scopeChain_, (void *)&scopeChain_);
    if (isDummyFrame())
        return;
    if (flags_ & HAS_ARGS_OBJ)
        JSObject::writeBarrierPost(argsObj_, (void *)&argsObj_);
    if (isFunctionFrame()) {
        JSFunction::writeBarrierPost(exec.fun, (void *)&exec.fun);
        if (isEvalFrame())
            JSScript::writeBarrierPost(u.evalScript, (void *)&u.evalScript);
    } else {
        JSScript::writeBarrierPost(exec.script, (void *)&exec.script);
    }
    if (hasReturnValue())
        HeapValue::writeBarrierPost(rval_, &rval_);
}

JSGenerator *
StackFrame::maybeSuspendedGenerator(JSRuntime *rt)
{
    /*
     * A suspended generator's frame is embedded inside the JSGenerator object
     * instead of on the contiguous stack like all active frames.
     */
    if (!isGeneratorFrame() || rt->stackSpace.containsFast(this))
        return NULL;

    /*
     * Once we know we have a suspended generator frame, there is a static
     * offset from the frame's snapshot to beginning of the JSGenerator.
     */
    char *vp = reinterpret_cast<char *>(generatorArgsSnapshotBegin());
    char *p = vp - offsetof(JSGenerator, stackSnapshot);
    JSGenerator *gen = reinterpret_cast<JSGenerator *>(p);
    JS_ASSERT(gen->fp == this);
    return gen;
}

jsbytecode *
StackFrame::prevpcSlow(InlinedSite **pinlined)
{
    JS_ASSERT(!(flags_ & HAS_PREVPC));
#if defined(JS_METHODJIT) && defined(JS_MONOIC)
    StackFrame *p = prev();
    mjit::JITScript *jit = p->script()->getJIT(p->isConstructing(), p->compartment()->needsBarrier());
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
StackFrame::pcQuadratic(const ContextStack &stack, size_t maxDepth)
{
    StackSegment &seg = stack.space().containingSegment(this);
    FrameRegs &regs = seg.regs();

    /*
     * This isn't just an optimization; seg->computeNextFrame(fp) is only
     * defined if fp != seg->regs->fp.
     */
    if (regs.fp() == this)
        return regs.pc;

    /*
     * To compute fp's pc, we need the next frame (where next->prev == fp).
     * This requires a linear search which we allow the caller to limit (in
     * cases where we do not have a hard requirement to find the correct pc).
     */
    if (StackFrame *next = seg.computeNextFrame(this, maxDepth))
        return next->prevpc();

    /* If we hit the limit, just return the beginning of the script. */
    return regs.fp()->script()->code;
}

bool
StackFrame::prologue(JSContext *cx, bool newType)
{
    JS_ASSERT(!isDummyFrame());
    JS_ASSERT(!isGeneratorFrame());
    JS_ASSERT(cx->regs().pc == script()->code);

    if (isEvalFrame()) {
        if (script()->strictModeCode) {
            CallObject *callobj = CallObject::createForStrictEval(cx, this);
            if (!callobj)
                return false;
            pushOnScopeChain(*callobj);
            flags_ |= HAS_CALL_OBJ;
        }
        return true;
    }

    if (isGlobalFrame())
        return true;

    JS_ASSERT(isNonEvalFunctionFrame());

    if (fun()->isHeavyweight()) {
        CallObject *callobj = CallObject::createForFunction(cx, this);
        if (!callobj)
            return false;
        pushOnScopeChain(*callobj);
        flags_ |= HAS_CALL_OBJ;
    }

    if (script()->nesting()) {
        types::NestingPrologue(cx, this);
        flags_ |= HAS_NESTING;
    }

    if (isConstructing()) {
        RootedObject callee(cx, &this->callee());
        JSObject *obj = js_CreateThisForFunction(cx, callee, newType);
        if (!obj)
            return false;
        functionThis() = ObjectValue(*obj);
    }

    Probes::enterJSFun(cx, fun(), script());
    return true;
}

void
StackFrame::epilogue(JSContext *cx)
{
    JS_ASSERT(!isDummyFrame());
    JS_ASSERT(!isYielding());
    JS_ASSERT(!hasBlockChain());

    if (isEvalFrame()) {
        if (isStrictEvalFrame()) {
            JS_ASSERT_IF(hasCallObj(), scopeChain()->asCall().isForEval());
            if (cx->compartment->debugMode())
                cx->runtime->debugScopes->onPopStrictEvalScope(this);
        } else if (isDirectEvalFrame()) {
            if (isDebuggerFrame())
                JS_ASSERT(!scopeChain()->isScope());
            else
                JS_ASSERT(scopeChain() == prev()->scopeChain());
        } else {
            JS_ASSERT(scopeChain()->isGlobal());
        }
        return;
    }

    if (isGlobalFrame()) {
        JS_ASSERT(!scopeChain()->isScope());
        return;
    }

    JS_ASSERT(isNonEvalFunctionFrame());
    if (fun()->isHeavyweight()) {
        JS_ASSERT_IF(hasCallObj(),
                     scopeChain()->asCall().getCalleeFunction()->script() == script());
    } else {
        JS_ASSERT(!scopeChain()->isCall() || scopeChain()->asCall().isForEval() ||
                  scopeChain()->asCall().getCalleeFunction()->script() != script());
    }

    if (cx->compartment->debugMode())
        cx->runtime->debugScopes->onPopCall(this, cx);

    Probes::exitJSFun(cx, fun(), script());

    if (script()->nesting() && (flags_ & HAS_NESTING))
        types::NestingEpilogue(this);

    if (isConstructing() && returnValue().isPrimitive())
        setReturnValue(ObjectValue(constructorThis()));
}

bool
StackFrame::jitStrictEvalPrologue(JSContext *cx)
{
    JS_ASSERT(isStrictEvalFrame());
    CallObject *callobj = CallObject::createForStrictEval(cx, this);
    if (!callobj)
        return false;

    pushOnScopeChain(*callobj);
    flags_ |= HAS_CALL_OBJ;
    return true;
}

bool
StackFrame::pushBlock(JSContext *cx, StaticBlockObject &block)
{
    JS_ASSERT_IF(hasBlockChain(), blockChain_ == block.enclosingBlock());

    if (block.needsClone()) {
        Rooted<StaticBlockObject *> blockHandle(cx, &block);
        ClonedBlockObject *clone = ClonedBlockObject::create(cx, blockHandle, this);
        if (!clone)
            return false;

        pushOnScopeChain(*clone);
    }

    flags_ |= HAS_BLOCKCHAIN;
    blockChain_ = &block;
    return true;
}

void
StackFrame::popBlock(JSContext *cx)
{
    JS_ASSERT(hasBlockChain());

    if (cx->compartment->debugMode())
        cx->runtime->debugScopes->onPopBlock(cx, this);

    if (blockChain_->needsClone()) {
        JS_ASSERT(scopeChain_->asClonedBlock().staticBlock() == *blockChain_);
        popOffScopeChain();
    }

    blockChain_ = blockChain_->enclosingBlock();
}

void
StackFrame::popWith(JSContext *cx)
{
    if (cx->compartment->debugMode())
        cx->runtime->debugScopes->onPopWith(this);

    JS_ASSERT(scopeChain()->isWith());
    popOffScopeChain();
}

void
StackFrame::mark(JSTracer *trc)
{
    /*
     * Normally we would use MarkRoot here, except that generators also take
     * this path. However, generators use a special write barrier when the stack
     * frame is copied to the floating frame. Therefore, no barrier is needed.
     */
    if (flags_ & HAS_SCOPECHAIN)
        gc::MarkObjectUnbarriered(trc, &scopeChain_, "scope chain");
    if (isDummyFrame())
        return;
    if (flags_ & HAS_ARGS_OBJ)
        gc::MarkObjectUnbarriered(trc, &argsObj_, "arguments");
    if (isFunctionFrame()) {
        gc::MarkObjectUnbarriered(trc, &exec.fun, "fun");
        if (isEvalFrame())
            gc::MarkScriptUnbarriered(trc, &u.evalScript, "eval script");
    } else {
        gc::MarkScriptUnbarriered(trc, &exec.script, "script");
    }
    if (IS_GC_MARKING_TRACER(trc))
        script()->compartment()->active = true;
    gc::MarkValueUnbarriered(trc, &returnValue(), "rval");
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
    Value *vp = call->array();
    return vp > slotsBegin() && vp <= calls_->array();
}

StackFrame *
StackSegment::computeNextFrame(const StackFrame *f, size_t maxDepth) const
{
    JS_ASSERT(contains(f) && f != fp());

    StackFrame *next = fp();
    for (size_t i = 0; i <= maxDepth; ++i) {
        if (next->prev() == f)
            return next;
        next = next->prev();
    }

    return NULL;
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
    Debug_SetValueRangeToCrashOnTouch(base_, commitEnd_);
#elif defined(XP_OS2)
    if (DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_ANY) &&
        DosAllocMem(&p, CAPACITY_BYTES, PAG_COMMIT | PAG_READ | PAG_WRITE))
        return false;
    base_ = reinterpret_cast<Value *>(p);
    trustedEnd_ = base_ + CAPACITY_VALS;
    conservativeEnd_ = defaultEnd_ = trustedEnd_ - BUFFER_VALS;
    Debug_SetValueRangeToCrashOnTouch(base_, trustedEnd_);
#else
    JS_ASSERT(CAPACITY_BYTES % getpagesize() == 0);
    p = mmap(NULL, CAPACITY_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return false;
    base_ = reinterpret_cast<Value *>(p);
    trustedEnd_ = base_ + CAPACITY_VALS;
    conservativeEnd_ = defaultEnd_ = trustedEnd_ - BUFFER_VALS;
    Debug_SetValueRangeToCrashOnTouch(base_, trustedEnd_);
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
StackSpace::markFrameValues(JSTracer *trc, StackFrame *fp, Value *slotsEnd, jsbytecode *pc)
{
    Value *slotsBegin = fp->slots();

    if (!fp->isScriptFrame()) {
        JS_ASSERT(fp->isDummyFrame());
        gc::MarkValueRootRange(trc, slotsBegin, slotsEnd, "vm_stack");
        return;
    }

    /* If it's a scripted frame, we should have a pc. */
    JS_ASSERT(pc);

    JSScript *script = fp->script();
    if (!script->hasAnalysis() || !script->analysis()->ranLifetimes()) {
        gc::MarkValueRootRange(trc, slotsBegin, slotsEnd, "vm_stack");
        return;
    }

    /*
     * If the JIT ran a lifetime analysis, then it may have left garbage in the
     * slots considered not live. We need to avoid marking them. Additionally,
     * in case the analysis information is thrown out later, we overwrite these
     * dead slots with valid values so that future GCs won't crash. Analysis
     * results are thrown away during the sweeping phase, so we always have at
     * least one GC to do this.
     */
    analyze::AutoEnterAnalysis aea(script->compartment());
    analyze::ScriptAnalysis *analysis = script->analysis();
    uint32_t offset = pc - script->code;
    Value *fixedEnd = slotsBegin + script->nfixed;
    for (Value *vp = slotsBegin; vp < fixedEnd; vp++) {
        uint32_t slot = analyze::LocalSlot(script, vp - slotsBegin);

        /*
         * Will this slot be synced by the JIT? If not, replace with a dummy
         * value with the same type tag.
         */
        if (!analysis->trackSlot(slot) || analysis->liveness(slot).live(offset)) {
            gc::MarkValueRoot(trc, vp, "vm_stack");
        } else if (vp->isDouble()) {
            *vp = DoubleValue(0.0);
        } else {
            /*
             * It's possible that *vp may not be a valid Value. For example, it
             * may be tagged as a NullValue but the low bits may be nonzero so
             * that isNull() returns false. This can cause problems later on
             * when marking the value. Extracting the type in this way and then
             * overwriting the value circumvents the problem.
             */
            JSValueType type = vp->extractNonDoubleType();
            if (type == JSVAL_TYPE_INT32)
                *vp = Int32Value(0);
            else if (type == JSVAL_TYPE_UNDEFINED)
                *vp = UndefinedValue();
            else if (type == JSVAL_TYPE_BOOLEAN)
                *vp = BooleanValue(false);
            else if (type == JSVAL_TYPE_STRING)
                *vp = StringValue(trc->runtime->atomState.nullAtom);
            else if (type == JSVAL_TYPE_NULL)
                *vp = NullValue();
            else if (type == JSVAL_TYPE_OBJECT)
                *vp = ObjectValue(fp->scopeChain()->global());
        }
    }

    gc::MarkValueRootRange(trc, fixedEnd, slotsEnd, "vm_stack");
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
         */
        Value *slotsEnd = nextSegEnd;
        jsbytecode *pc = seg->maybepc();
        for (StackFrame *fp = seg->maybefp(); (Value *)fp > (Value *)seg; fp = fp->prev()) {
            /* Mark from fp->slots() to slotsEnd. */
            markFrameValues(trc, fp, slotsEnd, pc);

            fp->mark(trc);
            slotsEnd = (Value *)fp;

            InlinedSite *site;
            pc = fp->prevpc(&site);
            JS_ASSERT_IF(fp->prev(), !site);
        }
        gc::MarkValueRootRange(trc, seg->slotsBegin(), slotsEnd, "vm_stack");
        nextSegEnd = (Value *)seg;
    }
}

void
StackSpace::markActiveCompartments()
{
    for (StackSegment *seg = seg_; seg; seg = seg->prevInMemory()) {
        for (StackFrame *fp = seg->maybefp(); (Value *)fp > (Value *)seg; fp = fp->prev())
            MarkCompartmentActive(fp);
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
        int32_t size = static_cast<int32_t>(newCommit - commitEnd_) * sizeof(Value);

        if (!VirtualAlloc(commitEnd_, size, MEM_COMMIT, PAGE_READWRITE)) {
            if (report)
                js_ReportOverRecursed(cx);
            return false;
        }

        Debug_SetValueRangeToCrashOnTouch(commitEnd_, newCommit);

        commitEnd_ = newCommit;
        conservativeEnd_ = Min(commitEnd_, defaultEnd_);
        assertInvariants();
    }
#endif

    return true;
}

bool
StackSpace::tryBumpLimit(JSContext *cx, Value *from, unsigned nvals, Value **limit)
{
    if (!ensureSpace(cx, REPORT_ERROR, from, nvals))
        return false;
    *limit = conservativeEnd_;
    return true;
}

size_t
StackSpace::sizeOfCommitted()
{
#ifdef XP_WIN
    return (commitEnd_ - base_) * sizeof(Value);
#else
    return (trustedEnd_ - base_) * sizeof(Value);
#endif
}

#ifdef DEBUG
bool
StackSpace::containsSlow(StackFrame *fp)
{
    for (AllFramesIter i(*this); !i.done(); ++i) {
        if (i.fp() == fp)
            return true;
    }
    return false;
}
#endif

/*****************************************************************************/

ContextStack::ContextStack(JSContext *cx)
  : seg_(NULL),
    space_(&cx->runtime->stackSpace),
    cx_(cx)
{}

ContextStack::~ContextStack()
{
    JS_ASSERT(!seg_);
}

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
ContextStack::ensureOnTop(JSContext *cx, MaybeReportError report, unsigned nvars,
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
        if (InlinedSite *site = regs->inlined()) {
            mjit::JITChunk *chunk = regs->fp()->jit()->chunk(regs->pc);
            fun = chunk->inlineFrames()[site->inlineIndex].fun;
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
ContextStack::pushInvokeArgs(JSContext *cx, unsigned argc, InvokeArgsGuard *iag)
{
    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);

    unsigned nvars = 2 + argc;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &iag->pushedSeg_);
    if (!firstUnused)
        return false;

    MakeRangeGCSafe(firstUnused, nvars);

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

    Value *oldend = seg_->end();

    seg_->popCall();
    if (iag.pushedSeg_)
        popSegment();

    Debug_SetValueRangeToCrashOnTouch(space().firstUnused(), oldend);
}

bool
ContextStack::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                              InitialFrameFlags initial, InvokeFrameGuard *ifg)
{
    JS_ASSERT(onTop());
    JS_ASSERT(space().firstUnused() == args.end());

    JSObject &callee = args.callee();
    JSFunction *fun = callee.toFunction();
    JSScript *script = fun->script();

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, REPORT_ERROR, args, fun, script, &flags);
    if (!fp)
        return false;

    fp->initCallFrame(cx, *fun, script, args.length(), flags);
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
    MaybeExtend extend;
    if (evalInFrame) {
        /* Though the prev-frame is given, need to search for prev-call. */
        StackSegment &seg = cx->stack.space().containingSegment(evalInFrame);
        StackIter iter(cx->runtime, seg);
        while (!iter.isScript() || iter.fp() != evalInFrame)
            ++iter;
        evalInFrameCalls = iter.calls_;
        extend = CANT_EXTEND;
    } else {
        extend = CAN_EXTEND;
    }

    unsigned nvars = 2 /* callee, this */ + VALUES_PER_STACK_FRAME + script->nslots;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, extend, &efg->pushedSeg_);
    if (!firstUnused)
        return false;

    StackFrame *prev = evalInFrame ? evalInFrame : maybefp();
    StackFrame *fp = reinterpret_cast<StackFrame *>(firstUnused + 2);
    fp->initExecuteFrame(script, prev, seg_->maybeRegs(), thisv, scopeChain, type);
    fp->initVarsToUndefined();
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

    unsigned nvars = VALUES_PER_STACK_FRAME;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &dfg->pushedSeg_, dest);
    if (!firstUnused)
        return false;

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

    Value *oldend = seg_->end();

    seg_->popRegs(fg.prevRegs_);
    if (fg.pushedSeg_)
        popSegment();

    Debug_SetValueRangeToCrashOnTouch(space().firstUnused(), oldend);

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
    HeapValue *genvp = gen->stackSnapshot;
    JS_ASSERT(genvp == HeapValueify(gen->fp->generatorArgsSnapshotBegin()));
    unsigned vplen = HeapValueify(gen->fp->generatorArgsSnapshotEnd()) - genvp;

    unsigned nvars = vplen + VALUES_PER_STACK_FRAME + gen->fp->script()->nslots;
    Value *firstUnused = ensureOnTop(cx, REPORT_ERROR, nvars, CAN_EXTEND, &gfg->pushedSeg_);
    if (!firstUnused)
        return false;

    StackFrame *stackfp = reinterpret_cast<StackFrame *>(firstUnused + vplen);
    Value *stackvp = (Value *)stackfp - vplen;

    /* Save this for popGeneratorFrame. */
    gfg->gen_ = gen;
    gfg->stackvp_ = stackvp;

    /*
     * Trigger incremental barrier on the floating frame's generator object.
     * This is normally traced through only by associated arguments/call
     * objects, but only when the generator is not actually on the stack.
     * We don't need to worry about generational barriers as the generator
     * object has a trace hook and cannot be nursery allocated.
     */
    JS_ASSERT(gen->obj->getClass()->trace);
    JSObject::writeBarrierPre(gen->obj);

    /* Copy from the generator's floating frame to the stack. */
    stackfp->copyFrameAndValues<StackFrame::NoPostBarrier>(cx, stackvp, gen->fp,
                                                           Valueify(genvp), gen->regs.sp);
    stackfp->resetGeneratorPrev(cx);
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
    HeapValue *genvp = gen->stackSnapshot;
    JS_ASSERT(genvp == HeapValueify(gen->fp->generatorArgsSnapshotBegin()));

    const FrameRegs &stackRegs = gfg.regs_;
    StackFrame *stackfp = stackRegs.fp();
    Value *stackvp = gfg.stackvp_;

    /* Copy from the stack to the generator's floating frame. */
    if (stackfp->isYielding()) {
        /*
         * Assert that the frame is not markable so that we don't need an
         * incremental write barrier when updating the generator's saved slots.
         */
        JS_ASSERT(!GeneratorHasMarkableFrame(gen));

        gen->regs.rebaseFromTo(stackRegs, *gen->fp);
        gen->fp->copyFrameAndValues<StackFrame::DoPostBarrier>(cx_, (Value *)genvp, stackfp,
                                                               stackvp, stackRegs.sp);
    }

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
    script_ = (JSScript *)0xbad;
}

void
StackIter::popFrame()
{
    StackFrame *oldfp = fp_;
    JS_ASSERT(seg_->contains(oldfp));
    fp_ = fp_->prev();
    if (seg_->contains(fp_)) {
        InlinedSite *inline_;
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
            sp_ = oldfp->generatorArgsSnapshotBegin();
        } else if (oldfp->isNonEvalFunctionFrame()) {
            /*
             * When Invoke is called from a native, there will be an enclosing
             * pushInvokeArgs which pushes a CallArgsList element so we can
             * ignore that case. The other two cases of function call frames are
             * Invoke called directly from script and pushInlineFrmae. In both
             * cases, the actual arguments of the callee should be included in
             * the caller's expr stack.
             */
            sp_ = oldfp->actuals() + oldfp->numActualArgs();
        } else if (oldfp->isFramePushedByExecute()) {
            /* pushExecuteFrame pushes exactly (callee, this) before frame. */
            sp_ = (Value *)oldfp - 2;
        } else {
            /* pushDummyFrame pushes exactly 0 slots before frame. */
            JS_ASSERT(oldfp->isDummyFrame());
            sp_ = (Value *)oldfp;
        }

        script_ = fp_->maybeScript();
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
        if (fp_)
            script_ = fp_->maybeScript();
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
    Value *slots = (Value *)(fp + 1);
    if (vp < slots || vp >= slots + fp->script()->nslots) {
        MOZ_ASSERT(false, "About to dereference invalid slot");
        *(volatile int *)0xbad = 0;  // show up nicely in crash-stats
        MOZ_CRASH();
    }
}

/*
 * Given that the iterator's current value of fp_ and calls_ (initialized on
 * construction or after operator++ popped the previous scripted/native call),
 * "settle" the iterator on a new StackIter::State value. The goal is to
 * present the client a simple linear sequence of native/scripted calls while
 * covering up unpleasant stack implementation details:
 *  - The frame change can be "saved" and "restored" (see JS_SaveFrameChain).
 *    This artificially cuts the call chain and the StackIter client may want
 *    to continue through this cut to the previous frame by passing
 *    GO_THROUGH_SAVED.
 *  - fp->prev can be in a different contiguous segment from fp. In this case,
 *    the current values of sp/pc after calling popFrame/popCall are incorrect
 *    and should be recovered from fp->prev's segment.
 *  - there is no explicit relationship to determine whether fp_ or calls_ is
 *    the innermost invocation so implicit memory ordering is used since both
 *    push values on the stack.
 *  - calls to natives directly from JS do not push a record and thus the
 *    native call must be recovered by sniffing the stack.
 *  - a native call's 'callee' argument is clobbered on return while the
 *    CallArgsList element is still visible.
 */
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
            /* Eval-in-frame can cross contexts, so use prevInMemory. */
            seg_ = seg_->prevInMemory();
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
        if (containsFrame && (!containsCall || (Value *)fp_ >= calls_->array())) {
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
             * The Function.prototype.call optimization leaves no record when
             * 'this' is a native function. Thus, if the following expression
             * runs and breaks in the debugger, the call to 'replace' will not
             * appear on the callstack.
             *
             *   (String.prototype.replace).call('a',/a/,function(){debugger});
             *
             * Function.prototype.call will however appear, hence the debugger
             * can, by inspecting 'args.thisv', give some useful information.
             *
             * For Function.prototype.apply, the situation is even worse: since
             * a dynamic number of arguments have been pushed onto the stack
             * (see SplatApplyArgs), there is no efficient way to know how to
             * find the callee. Thus, calls to apply are lost completely.
             */
            JSOp op = JSOp(*pc_);
            if (op == JSOP_CALL || op == JSOP_FUNCALL) {
                unsigned argc = GET_ARGC(pc_);
                DebugOnly<unsigned> spoff = sp_ - fp_->base();
                JS_ASSERT_IF(maybecx_ && maybecx_->stackIterAssertionEnabled,
                             spoff == js_ReconstructStackDepth(maybecx_, fp_->script(), pc_));
                Value *vp = sp_ - (2 + argc);

                CrashIfInvalidSlot(fp_, vp);
                if (IsNativeFunction(*vp)) {
                    state_ = IMPLICIT_NATIVE;
                    args_ = CallArgsFromVp(argc, vp);
                    return;
                }
            }

            state_ = SCRIPTED;
            script_ = fp_->script();

            /*
             * Check sp and pc. JM's getter ICs may push 2 extra values on the
             * stack; this is okay since the methodjit reserves some extra slots
             * for loop temporaries.
             */
            if (op == JSOP_GETPROP || op == JSOP_CALLPROP)
                JS_ASSERT(sp_ >= fp_->base() && sp_ <= fp_->slots() + script_->nslots + 2);
            else if (op != JSOP_FUNAPPLY)
                JS_ASSERT(sp_ >= fp_->base() && sp_ <= fp_->slots() + script_->nslots);
            JS_ASSERT(pc_ >= script_->code && pc_ < script_->code + script_->length);
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
  : maybecx_(cx),
    savedOption_(savedOption)
{
#ifdef JS_METHODJIT
    CompartmentVector &v = cx->runtime->compartments;
    for (size_t i = 0; i < v.length(); i++)
        mjit::ExpandInlineFrames(v[i]);
#endif

    if (StackSegment *seg = cx->stack.seg_) {
        startOnSegment(seg);
        settleOnNewState();
    } else {
        state_ = DONE;
    }
}

StackIter::StackIter(JSRuntime *rt, StackSegment &seg)
  : maybecx_(NULL), savedOption_(STOP_AT_SAVED)
{
#ifdef JS_METHODJIT
    CompartmentVector &v = rt->compartments;
    for (size_t i = 0; i < v.length(); i++)
        mjit::ExpandInlineFrames(v[i]);
#endif
    startOnSegment(&seg);
    settleOnNewState();
}

StackIter &
StackIter::operator++()
{
    switch (state_) {
      case DONE:
        JS_NOT_REACHED("Unexpected state");
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

bool
StackIter::isFunctionFrame() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        return fp()->isFunctionFrame();
      case NATIVE:
      case IMPLICIT_NATIVE:
        return false;
    }
    JS_NOT_REACHED("Unexpected state");
    return false;
}

bool
StackIter::isEvalFrame() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        return fp()->isEvalFrame();
      case NATIVE:
      case IMPLICIT_NATIVE:
        return false;
    }
    JS_NOT_REACHED("Unexpected state");
    return false;
}

bool
StackIter::isNonEvalFunctionFrame() const
{
    JS_ASSERT(!done());
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        return fp()->isNonEvalFunctionFrame();
      case NATIVE:
      case IMPLICIT_NATIVE:
        return !isEvalFrame() && isFunctionFrame();
    }
    JS_NOT_REACHED("Unexpected state");
    return false;
}

bool
StackIter::isConstructing() const
{
    switch (state_) {
      case DONE:
        JS_NOT_REACHED("Unexpected state");
        return false;
      case SCRIPTED:
      case NATIVE:
      case IMPLICIT_NATIVE:
        return fp()->isConstructing();
    }
    return false;
}

JSFunction *
StackIter::callee() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return &fp()->callee();
      case NATIVE:
      case IMPLICIT_NATIVE:
        return nativeArgs().callee().toFunction();
    }
    JS_NOT_REACHED("Unexpected state");
    return NULL;
}

Value
StackIter::calleev() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return fp()->calleev();
      case NATIVE:
      case IMPLICIT_NATIVE:
        return nativeArgs().calleev();
    }
    JS_NOT_REACHED("Unexpected state");
    return Value();
}

Value
StackIter::thisv() const
{
    switch (state_) {
      case DONE:
        MOZ_NOT_REACHED("Unexpected state");
        return Value();
      case SCRIPTED:
      case NATIVE:
      case IMPLICIT_NATIVE:
        return fp()->thisValue();
    }
    MOZ_NOT_REACHED("unexpected state");
    return Value();
}

/*****************************************************************************/

AllFramesIter::AllFramesIter(StackSpace &space)
  : seg_(space.seg_),
    fp_(seg_ ? seg_->maybefp() : NULL)
{
    settle();
}

AllFramesIter&
AllFramesIter::operator++()
{
    JS_ASSERT(!done());
    fp_ = fp_->prev();
    settle();
    return *this;
}

void
AllFramesIter::settle()
{
    while (seg_ && (!fp_ || !seg_->contains(fp_))) {
        seg_ = seg_->prevInMemory();
        fp_ = seg_ ? seg_->maybefp() : NULL;
    }

    JS_ASSERT(!!seg_ == !!fp_);
    JS_ASSERT_IF(fp_, seg_->contains(fp_));
}
