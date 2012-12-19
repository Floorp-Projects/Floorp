/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jscntxt.h"
#include "gc/Marking.h"
#include "methodjit/MethodJIT.h"
#ifdef JS_ION
#include "ion/IonFrames.h"
#include "ion/IonCompartment.h"
#include "ion/Bailouts.h"
#endif
#include "Stack.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsinterpinlines.h"

#include "jsopcode.h"

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

using mozilla::DebugOnly;

/*****************************************************************************/

void
StackFrame::initExecuteFrame(UnrootedScript script, StackFrame *prev, FrameRegs *regs,
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
        u.evalScript = (RawScript)0xbad;
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
        DebugScopes::onGeneratorFrameChange(otherfp, this, cx);
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
    mjit::JITScript *jit = p->script()->getJIT(p->isConstructing(), p->compartment()->compileBarriers());
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
StackFrame::copyRawFrameSlots(AutoValueVector *vec)
{
    if (!vec->resize(numFormalArgs() + script()->nfixed))
        return false;
    PodCopy(vec->begin(), formals(), numFormalArgs());
    PodCopy(vec->begin() + numFormalArgs(), slots(), script()->nfixed);
    return true;
}

static inline void
AssertDynamicScopeMatchesStaticScope(JSScript *script, JSObject *scope)
{
#ifdef DEBUG
    for (StaticScopeIter i(script->enclosingStaticScope()); !i.done(); i++) {
        if (i.hasDynamicScopeObject()) {
            /*
             * 'with' does not participate in the static scope of the script,
             * but it does in the dynamic scope, so skip them here.
             */
            while (scope->isWith())
                scope = &scope->asWith().enclosingScope();

            switch (i.type()) {
              case StaticScopeIter::BLOCK:
                JS_ASSERT(i.block() == scope->asClonedBlock().staticBlock());
                scope = &scope->asClonedBlock().enclosingScope();
                break;
              case StaticScopeIter::FUNCTION:
                JS_ASSERT(scope->asCall().callee().nonLazyScript() == i.funScript());
                scope = &scope->asCall().enclosingScope();
                break;
              case StaticScopeIter::NAMED_LAMBDA:
                scope = &scope->asDeclEnv().enclosingScope();
                break;
            }
        }
    }

    /*
     * Ideally, we'd JS_ASSERT(!scope->isScope()) but the enclosing lexical
     * scope chain stops at eval() boundaries. See StaticScopeIter comment.
     */
#endif
}

bool
StackFrame::initFunctionScopeObjects(JSContext *cx)
{
    CallObject *callobj = CallObject::createForFunction(cx, this);
    if (!callobj)
        return false;
    pushOnScopeChain(*callobj);
    flags_ |= HAS_CALL_OBJ;
    return true;
}

bool
StackFrame::prologue(JSContext *cx, bool newType)
{
    RootedScript script(cx, this->script());

    JS_ASSERT(!isGeneratorFrame());
    JS_ASSERT(cx->regs().pc == script->code);

    if (isEvalFrame()) {
        if (script->strict) {
            CallObject *callobj = CallObject::createForStrictEval(cx, this);
            if (!callobj)
                return false;
            pushOnScopeChain(*callobj);
            flags_ |= HAS_CALL_OBJ;
        }
        Probes::enterScript(cx, script, NULL, this);
        return true;
    }

    if (isGlobalFrame()) {
        Probes::enterScript(cx, script, NULL, this);
        return true;
    }

    JS_ASSERT(isNonEvalFunctionFrame());
    AssertDynamicScopeMatchesStaticScope(script, scopeChain());

    if (fun()->isHeavyweight() && !initFunctionScopeObjects(cx))
        return false;

    if (isConstructing()) {
        RootedObject callee(cx, &this->callee());
        JSObject *obj = js_CreateThisForFunction(cx, callee, newType);
        if (!obj)
            return false;
        functionThis() = ObjectValue(*obj);
    }

    Probes::enterScript(cx, script, script->function(), this);
    return true;
}

void
StackFrame::epilogue(JSContext *cx)
{
    JS_ASSERT(!isYielding());
    JS_ASSERT(!hasBlockChain());

    RootedScript script(cx, this->script());
    Probes::exitScript(cx, script, script->function(), this);

    if (isEvalFrame()) {
        if (isStrictEvalFrame()) {
            JS_ASSERT_IF(hasCallObj(), scopeChain()->asCall().isForEval());
            if (cx->compartment->debugMode())
                DebugScopes::onPopStrictEvalScope(this);
        } else if (isDirectEvalFrame()) {
            if (isDebuggerFrame())
                JS_ASSERT(!scopeChain()->isScope());
            else
                JS_ASSERT(scopeChain() == prev()->scopeChain());
        } else {
            /*
             * Debugger.Object.prototype.evalInGlobal creates indirect eval
             * frames scoped to the given global;
             * Debugger.Object.prototype.evalInGlobalWithBindings creates
             * indirect eval frames scoped to an object carrying the introduced
             * bindings.
             */
            if (isDebuggerFrame())
                JS_ASSERT(scopeChain()->isGlobal() || scopeChain()->enclosingScope()->isGlobal());
            else
                JS_ASSERT(scopeChain()->isGlobal());
        }
        return;
    }

    if (isGlobalFrame()) {
        JS_ASSERT(!scopeChain()->isScope());
        return;
    }

    JS_ASSERT(isNonEvalFunctionFrame());

    if (fun()->isHeavyweight())
        JS_ASSERT_IF(hasCallObj(), scopeChain()->asCall().callee().nonLazyScript() == script);
    else
        AssertDynamicScopeMatchesStaticScope(script, scopeChain());

    if (cx->compartment->debugMode())
        DebugScopes::onPopCall(this, cx);


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

        blockChain_ = blockHandle;
    } else {
        blockChain_ = &block;
    }

    flags_ |= HAS_BLOCKCHAIN;
    return true;
}

void
StackFrame::popBlock(JSContext *cx)
{
    JS_ASSERT(hasBlockChain());

    if (cx->compartment->debugMode())
        DebugScopes::onPopBlock(cx, this);

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
        DebugScopes::onPopWith(this);

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
StackSpace::markFrame(JSTracer *trc, StackFrame *fp, Value *slotsEnd)
{
    Value *slotsBegin = fp->slots();
    gc::MarkValueRootRange(trc, slotsBegin, slotsEnd, "vm_stack");
}

void
StackSpace::mark(JSTracer *trc)
{
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
        for (StackFrame *fp = seg->maybefp(); (Value *)fp > (Value *)seg; fp = fp->prev()) {
            /* Mark from fp->slots() to slotsEnd. */
            markFrame(trc, fp, slotsEnd);

            fp->mark(trc);
            slotsEnd = (Value *)fp;

            InlinedSite *site;
            fp->prevpc(&site);
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
StackSpace::ensureSpaceSlow(JSContext *cx, MaybeReportError report, Value *from, ptrdiff_t nvals) const
{
    mozilla::Maybe<AutoAssertNoGC> maybeNoGC;
    if (report)
        AssertCanGC();
    else
        maybeNoGC.construct();

    assertInvariants();

    JSCompartment *dest = cx->compartment;
    bool trusted = dest->principals == cx->runtime->trustedPrincipals();
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
StackSpace::sizeOf()
{
#if defined(XP_UNIX)
    /*
     * Measure how many of our pages are resident in RAM using mincore, and
     * return that as our size.  This is slow, but hopefully nobody expects
     * this method to be fast.
     *
     * Note that using mincore means that we don't count pages of the stack
     * which are swapped out to disk.  We really should, but what we have here
     * is better than counting the whole stack!
     */

    const int pageSize = getpagesize();
    size_t numBytes = (trustedEnd_ - base_) * sizeof(Value);
    size_t numPages = (numBytes + pageSize - 1) / pageSize;

    // On Linux, mincore's third argument has type unsigned char*.
#ifdef __linux__
    typedef unsigned char MincoreArgType;
#else
    typedef char MincoreArgType;
#endif

    MincoreArgType *vec = (MincoreArgType *) js_malloc(numPages);
    int result = mincore(base_, numBytes, vec);
    if (result) {
        js_free(vec);
        /*
         * If mincore fails us, return the vsize (like we do below if we're not
         * on Windows or Unix).
         */
        return (trustedEnd_ - base_) * sizeof(Value);
    }

    size_t residentBytes = 0;
    for (size_t i = 0; i < numPages; i++) {
        /* vec[i] has its least-significant bit set iff page i is in RAM. */
        if (vec[i] & 0x1)
            residentBytes += pageSize;
    }
    js_free(vec);
    return residentBytes;

#elif defined(XP_WIN)
    return (commitEnd_ - base_) * sizeof(Value);
#else
    /*
     * Return the stack's virtual size, which is at least an upper bound on its
     * resident size.
     */
    return (trustedEnd_ - base_) * sizeof(Value);
#endif
}

#ifdef DEBUG
bool
StackSpace::containsSlow(StackFrame *fp)
{
    for (AllFramesIter i(*this); !i.done(); ++i) {
        /*
         * Debug-mode currently disables Ion compilation in the compartment of
         * the debuggee.
         */
        if (i.isIon())
            continue;
        if (i.interpFrame() == fp)
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

ptrdiff_t
ContextStack::spIndexOf(const Value *vp)
{
    if (!hasfp())
        return JSDVG_SEARCH_STACK;

    Value *base = fp()->base();
    Value *sp = regs().sp;
    if (vp < base || vp >= sp)
        return JSDVG_SEARCH_STACK;

    return vp - sp;
}

bool
ContextStack::onTop() const
{
    return seg_ && seg_ == space().seg_;
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
                          MaybeExtend extend, bool *pushedSeg)
{
    mozilla::Maybe<AutoAssertNoGC> maybeNoGC;
    if (report)
        AssertCanGC();
    else
        maybeNoGC.construct();

    Value *firstUnused = space().firstUnused();
    FrameRegs *regs = cx->maybeRegs();

#ifdef JS_METHODJIT
    /*
     * The only calls made by inlined methodjit frames can be to other JIT
     * frames associated with the same VMFrame. If we try to Invoke(),
     * Execute() or so forth, any topmost inline frame will need to be
     * expanded (along with other inline frames in the compartment).
     * To avoid pathological behavior here, make sure to mark any topmost
     * function as uninlineable, which will expand inline frames if there are
     * any and prevent the function from being inlined in the future.
     *
     * Note: When called from pushBailoutFrame, error = DONT_REPORT_ERROR. Use
     * this to deny potential invalidation, which would read from
     * runtime->ionTop.
     */
    if (regs && report != DONT_REPORT_ERROR) {
        RootedFunction fun(cx);
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
            AutoCompartment ac(cx, fun);
            fun->nonLazyScript()->uninlineable = true;
            types::MarkTypeObjectFlags(cx, fun, types::OBJECT_FLAG_UNINLINEABLE);
        }
    }
    JS_ASSERT_IF(cx->hasfp(), !cx->regs().inlined());
#endif

    if (onTop() && extend) {
        if (!space().ensureSpace(cx, report, firstUnused, nvars))
            return NULL;
        return firstUnused;
    }

    if (!space().ensureSpace(cx, report, firstUnused, VALUES_PER_STACK_SEGMENT + nvars))
        return NULL;

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
ContextStack::pushInvokeArgs(JSContext *cx, unsigned argc, InvokeArgsGuard *iag,
                             MaybeReportError report)
{
    mozilla::Maybe<AutoAssertNoGC> maybeNoGC;
    if (report)
        AssertCanGC();
    else
        maybeNoGC.construct();

    JS_ASSERT(argc <= StackSpace::ARGS_LENGTH_MAX);

    unsigned nvars = 2 + argc;
    Value *firstUnused = ensureOnTop(cx, report, nvars, CAN_EXTEND, &iag->pushedSeg_);
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

StackFrame *
ContextStack::pushInvokeFrame(JSContext *cx, MaybeReportError report,
                              const CallArgs &args, JSFunction *fun,
                              InitialFrameFlags initial, FrameGuard *fg)
{
    mozilla::Maybe<AutoAssertNoGC> maybeNoGC;
    if (report)
        AssertCanGC();
    else
        maybeNoGC.construct();

    JS_ASSERT(onTop());
    JS_ASSERT(space().firstUnused() == args.end());

    RootedScript script(cx, fun->nonLazyScript());

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, report, args, fun, script, &flags);
    if (!fp)
        return NULL;

    fp->initCallFrame(cx, *fun, script, args.length(), flags);
    fg->regs_.prepareToRun(*fp, script);

    fg->prevRegs_ = seg_->pushRegs(fg->regs_);
    JS_ASSERT(space().firstUnused() == fg->regs_.sp);
    fg->setPushed(*this);
    return fp;
}

bool
ContextStack::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                              InitialFrameFlags initial, InvokeFrameGuard *ifg)
{
    JSObject &callee = args.callee();
    JSFunction *fun = callee.toFunction();
    if (!pushInvokeFrame(cx, REPORT_ERROR, args, fun, initial, ifg))
        return false;
    return true;
}

bool
ContextStack::pushExecuteFrame(JSContext *cx, JSScript *script, const Value &thisv,
                               JSObject &scopeChain, ExecuteType type,
                               StackFrame *evalInFrame, ExecuteFrameGuard *efg)
{
    AssertCanGC();

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
        /* Debug-mode currently disables Ion compilation. */
        JS_ASSERT(!evalInFrame->runningInIon());
        JS_ASSERT_IF(evalInFrame->compartment() == iter.compartment(), !iter.isIon());
        while (!iter.isScript() || iter.isIon() || iter.interpFrame() != evalInFrame) {
            ++iter;
            JS_ASSERT_IF(evalInFrame->compartment() == iter.compartment(), !iter.isIon());
        }
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

#ifdef JS_ION
bool
ContextStack::pushBailoutArgs(JSContext *cx, const ion::IonBailoutIterator &it, InvokeArgsGuard *iag)
{
    unsigned argc = it.numActualArgs();
    ion::SnapshotIterator s(it);

    if (!pushInvokeArgs(cx, argc, iag, DONT_REPORT_ERROR))
        return false;

    JSFunction *fun = it.callee();
    iag->setCallee(ObjectValue(*fun));

    CopyTo dst(iag->array());
    Value *src = it.actualArgs();
    Value thisv = iag->thisv();
    s.readFrameArgs(dst, src, NULL, &thisv, 0, fun->nargs, argc);
    return true;
}

StackFrame *
ContextStack::pushBailoutFrame(JSContext *cx, const ion::IonBailoutIterator &it,
                               const CallArgs &args, BailoutFrameGuard *bfg)
{
    JSFunction *fun = it.callee();
    return pushInvokeFrame(cx, DONT_REPORT_ERROR, args, fun, INITIAL_NONE, bfg);
}
#endif

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
}

bool
ContextStack::pushGeneratorFrame(JSContext *cx, JSGenerator *gen, GeneratorFrameGuard *gfg)
{
    AssertCanGC();
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
    AssertCanGC();

    bool pushedSeg;
    if (!ensureOnTop(cx_, REPORT_ERROR, 0, CANT_EXTEND, &pushedSeg))
        return false;

    JS_ASSERT(pushedSeg);
    JS_ASSERT(!hasfp());
    JS_ASSERT(onTop());
    JS_ASSERT(seg_->isEmpty());
    return true;
}

void
ContextStack::restoreFrameChain()
{
    JS_ASSERT(!hasfp());
    JS_ASSERT(onTop());
    JS_ASSERT(seg_->isEmpty());

    popSegment();
}

/*****************************************************************************/

void
StackIter::poisonRegs()
{
    pc_ = (jsbytecode *)0xbad;
    script_ = (RawScript)0xbad;
}

void
StackIter::popFrame()
{
    AutoAssertNoGC nogc;
    StackFrame *oldfp = fp_;
    JS_ASSERT(seg_->contains(oldfp));
    fp_ = fp_->prev();

    if (seg_->contains(fp_)) {
        InlinedSite *inline_;
        pc_ = oldfp->prevpc(&inline_);
        JS_ASSERT(!inline_);
        script_ = fp_->script();
    } else {
        poisonRegs();
    }
}

void
StackIter::popCall()
{
    DebugOnly<CallArgsList*> oldCall = calls_;
    JS_ASSERT(seg_->contains(oldCall));
    calls_ = calls_->prev();
    if (!seg_->contains(fp_))
        poisonRegs();
}

void
StackIter::settleOnNewSegment()
{
    AutoAssertNoGC nogc;
    if (FrameRegs *regs = seg_->maybeRegs()) {
        pc_ = regs->pc;
        if (fp_)
            script_ = fp_->script();
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
 *  - a native call's 'callee' argument is clobbered on return while the
 *    CallArgsList element is still visible.
 */
void
StackIter::settleOnNewState()
{
    AutoAssertNoGC nogc;

    /* Reset whether or we popped a call last time we settled. */
    poppedCallDuringSettle_ = false;

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
                while (!tmp.isScript() || tmp.fp_ != fp_)
                    ++tmp;
                JS_ASSERT(tmp.isScript() && tmp.seg_ == seg_ && tmp.fp_ == fp_);
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
#ifdef JS_ION
            if (fp_->beginsIonActivation()) {
                ionFrames_ = ion::IonFrameIterator(ionActivations_);

                if (ionFrames_.isNative()) {
                    state_ = ION;
                    return;
                }

                while (!ionFrames_.isScripted() && !ionFrames_.done())
                    ++ionFrames_;

                // When invoked from JM, we don't re-use the entryfp, so we
                // may have an empty Ion activation.
                if (ionFrames_.done()) {
                    state_ = SCRIPTED;
                    script_ = fp_->script();
                    return;
                }

                state_ = ION;
                nextIonFrame();
                return;
            }
#endif /* JS_ION */

            state_ = SCRIPTED;
            script_ = fp_->script();
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
        poppedCallDuringSettle_ = true;
    }
}

StackIter::StackIter(JSContext *cx, SavedOption savedOption)
  : perThread_(&cx->runtime->mainThread),
    maybecx_(cx),
    savedOption_(savedOption),
    script_(cx, NULL),
    poppedCallDuringSettle_(false)
#ifdef JS_ION
    , ionActivations_(cx),
    ionFrames_((uint8_t *)NULL),
    ionInlineFrames_((js::ion::IonFrameIterator*) NULL)
#endif
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
  : perThread_(&rt->mainThread),
    maybecx_(NULL),
    savedOption_(STOP_AT_SAVED),
    script_(rt, NULL),
    poppedCallDuringSettle_(false)
#ifdef JS_ION
    , ionActivations_(rt),
    ionFrames_((uint8_t *)NULL),
    ionInlineFrames_((js::ion::IonFrameIterator*) NULL)
#endif
{
#ifdef JS_METHODJIT
    CompartmentVector &v = rt->compartments;
    for (size_t i = 0; i < v.length(); i++)
        mjit::ExpandInlineFrames(v[i]);
#endif
    startOnSegment(&seg);
    settleOnNewState();
}

StackIter::StackIter(const StackIter &other)
  : perThread_(other.perThread_),
    maybecx_(other.maybecx_),
    savedOption_(other.savedOption_),
    state_(other.state_),
    fp_(other.fp_),
    calls_(other.calls_),
    seg_(other.seg_),
    pc_(other.pc_),
    script_(perThread_, other.script_),
    args_(other.args_),
    poppedCallDuringSettle_(other.poppedCallDuringSettle_)
#ifdef JS_ION
    , ionActivations_(other.ionActivations_),
    ionFrames_(other.ionFrames_),
    ionInlineFrames_(other.ionInlineFrames_)
#endif
{
}

#ifdef JS_ION
void
StackIter::nextIonFrame()
{
    if (ionFrames_.isOptimizedJS()) {
        ionInlineFrames_ = ion::InlineFrameIterator(&ionFrames_);
        pc_ = ionInlineFrames_.pc();
        script_ = ionInlineFrames_.script();
    } else {
        JS_ASSERT(ionFrames_.isBaselineJS());
        ionFrames_.baselineScriptAndPc(&script_, &pc_);
    }
}

void
StackIter::popIonFrame()
{
    AutoAssertNoGC nogc;
    // Keep fp which describes all ion frames.
    poisonRegs();
    if (ionFrames_.isOptimizedJS() && ionInlineFrames_.more()) {
        ++ionInlineFrames_;
        pc_ = ionInlineFrames_.pc();
        script_ = ionInlineFrames_.script();
    } else {
        ++ionFrames_;
        while (!ionFrames_.done() && !ionFrames_.isScripted())
            ++ionFrames_;

        if (!ionFrames_.done()) {
            nextIonFrame();
            return;
        }

        // The activation has no other frames. If entryfp is NULL, it was invoked
        // by a native written in C++, using FastInvoke, on top of another activation.
        ion::IonActivation *activation = ionActivations_.activation();
        if (!activation->entryfp()) {
            JS_ASSERT(activation->prevpc());
            JS_ASSERT(fp_->beginsIonActivation());
            ++ionActivations_;
            settleOnNewState();
            return;
        }

        if (fp_->runningInIon()) {
            ++ionActivations_;
            popFrame();
            settleOnNewState();
        } else {
            JS_ASSERT(fp_->callingIntoIon());
            state_ = SCRIPTED;
            script_ = fp_->script();
            pc_ = ionActivations_.activation()->prevpc();
            ++ionActivations_;
        }
    }
}
#endif

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
      case ION:
#ifdef JS_ION
        popIonFrame();
        break;
#else
        JS_NOT_REACHED("Unexpected state");
#endif
    }
    return *this;
}

bool
StackIter::operator==(const StackIter &rhs) const
{
    return done() == rhs.done() &&
           (done() ||
            (isScript() == rhs.isScript() &&
             ((isScript() && fp_ == rhs.fp_) ||
              (!isScript() && nativeArgs().base() == rhs.nativeArgs().base()))));
}

JSCompartment *
StackIter::compartment() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        return fp_->compartment();
      case ION:
#ifdef  JS_ION
        return ionActivations_.activation()->compartment();
#else
        break;
#endif
      case NATIVE:
        return calls_->callee().compartment();
    }
    JS_NOT_REACHED("Unexpected state");
    return NULL;
}

bool
StackIter::isFunctionFrame() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        return interpFrame()->isFunctionFrame();
      case ION:
#ifdef  JS_ION
        JS_ASSERT(ionFrames_.isScripted());
        if (ionFrames_.isBaselineJS())
            return ionFrames_.isFunctionFrame();
        return ionInlineFrames_.isFunctionFrame();
#else
        break;
#endif
      case NATIVE:
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
        return interpFrame()->isEvalFrame();
      case ION:
      case NATIVE:
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
        return interpFrame()->isNonEvalFunctionFrame();
      case ION:
      case NATIVE:
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
        break;
      case ION:
#ifdef JS_ION
        if (ionFrames_.isOptimizedJS())
            return ionInlineFrames_.isConstructing();
        JS_ASSERT(ionFrames_.isBaselineJS());
        return ionFrames_.isConstructing();
#else
        break;
#endif        
      case SCRIPTED:
      case NATIVE:
        return fp_->isConstructing();
    }
    JS_NOT_REACHED("Unexpected state");
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
        return &interpFrame()->callee();
      case ION:
#ifdef JS_ION
        if (ionFrames_.isBaselineJS())
            return ionFrames_.callee();
        if (ionFrames_.isOptimizedJS())
            return ionInlineFrames_.callee();
        JS_ASSERT(ionFrames_.isNative());
        return ionFrames_.callee();
#else
        break;
#endif
      case NATIVE:
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
        return interpFrame()->calleev();
      case ION:
#ifdef JS_ION
        return ObjectValue(*callee());
#else
        break;
#endif
      case NATIVE:
        return nativeArgs().calleev();
    }
    JS_NOT_REACHED("Unexpected state");
    return Value();
}

unsigned
StackIter::numActualArgs() const
{
    switch (state_) {
      case DONE:
        break;
      case SCRIPTED:
        JS_ASSERT(isFunctionFrame());
        return interpFrame()->numActualArgs();
      case ION:
#ifdef JS_ION
        if (ionFrames_.isOptimizedJS())
            return ionInlineFrames_.numActualArgs();

        JS_ASSERT(ionFrames_.isBaselineJS());
        return ionFrames_.numActualArgs();
#else
        break;
#endif
      case NATIVE:
        return nativeArgs().length();
    }
    JS_NOT_REACHED("Unexpected state");
    return 0;
}

JSObject *
StackIter::scopeChain() const
{
    switch (state_) {
      case DONE:
        break;
      case ION:
#ifdef JS_ION
        return ionInlineFrames_.scopeChain();
#else
        break;
#endif
      case SCRIPTED:
        return interpFrame()->scopeChain();
      case NATIVE:
        break;
    }
    JS_NOT_REACHED("Unexpected state");
    return NULL;
}

bool
StackIter::computeThis() const
{
    if (isScript() && !isIon()) {
        JS_ASSERT(maybecx_);
        return ComputeThis(maybecx_, interpFrame());
    }
    return true;
}

Value
StackIter::thisv() const
{
    switch (state_) {
      case DONE:
        break;
      case ION:
#ifdef JS_ION
        return ObjectValue(*ionInlineFrames_.thisObject());
#else
        break;
#endif
      case SCRIPTED:
      case NATIVE:
        return fp_->thisValue();
    }
    JS_NOT_REACHED("Unexpected state");
    return NullValue();
}

size_t
StackIter::numFrameSlots() const
{
    AutoAssertNoGC nogc;
    switch (state_) {
      case DONE:
      case NATIVE:
        break;
      case ION:
#ifdef JS_ION
        if (ionFrames_.isOptimizedJS())
            return ionInlineFrames_.snapshotIterator().slots() - ionInlineFrames_.script()->nfixed;
        return ionFrames_.numBaselineStackValues() - ionFrames_.script()->nfixed;
#else
        break;
#endif
      case SCRIPTED:
        JS_ASSERT(maybecx_);
        JS_ASSERT(maybecx_->regs().spForStackDepth(0) == interpFrame()->base());
        return maybecx_->regs().sp - interpFrame()->base();
    }
    JS_NOT_REACHED("Unexpected state");
    return 0;
}

Value
StackIter::frameSlotValue(size_t index) const
{
    AutoAssertNoGC nogc;
    switch (state_) {
      case DONE:
      case NATIVE:
        break;
      case ION:
#ifdef JS_ION
        if (ionFrames_.isOptimizedJS()) {
            ion::SnapshotIterator si(ionInlineFrames_.snapshotIterator());
            index += ionInlineFrames_.script()->nfixed;
            return si.maybeReadSlotByIndex(index);
        }

        index += ionFrames_.script()->nfixed;
        return ionFrames_.baselineStackValue(index);
#else
        break;
#endif
      case SCRIPTED:
          return interpFrame()->base()[index];
    }
    JS_NOT_REACHED("Unexpected state");
    return NullValue();
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
