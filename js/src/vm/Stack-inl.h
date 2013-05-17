/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Stack_inl_h__
#define Stack_inl_h__

#include "mozilla/PodOperations.h"

#include "jscntxt.h"
#include "jscompartment.h"

#include "vm/Stack.h"
#ifdef JS_ION
#include "ion/BaselineFrame.h"
#include "ion/BaselineFrame-inl.h"
#include "ion/IonFrameIterator-inl.h"
#endif
#include "jsscriptinlines.h"

#include "ArgumentsObject-inl.h"
#include "ScopeObject-inl.h"

namespace js {

/*
 * We cache name lookup results only for the global object or for native
 * non-global objects without prototype or with prototype that never mutates,
 * see bug 462734 and bug 487039.
 */
static inline bool
IsCacheableNonGlobalScope(JSObject *obj)
{
    bool cacheable = (obj->isCall() || obj->isBlock() || obj->isDeclEnv());

    JS_ASSERT_IF(cacheable, !obj->getOps()->lookupProperty);
    return cacheable;
}

inline HandleObject
StackFrame::scopeChain() const
{
    JS_ASSERT_IF(!(flags_ & HAS_SCOPECHAIN), isFunctionFrame());
    if (!(flags_ & HAS_SCOPECHAIN)) {
        scopeChain_ = callee().environment();
        flags_ |= HAS_SCOPECHAIN;
    }
    return HandleObject::fromMarkedLocation(&scopeChain_);
}

inline GlobalObject &
StackFrame::global() const
{
    return scopeChain()->global();
}

inline JSObject &
StackFrame::varObj()
{
    JSObject *obj = scopeChain();
    while (!obj->isVarObj())
        obj = obj->enclosingScope();
    return *obj;
}

inline JSCompartment *
StackFrame::compartment() const
{
    JS_ASSERT(scopeChain()->compartment() == script()->compartment());
    return scopeChain()->compartment();
}

inline void
StackFrame::initPrev(JSContext *cx)
{
    JS_ASSERT(flags_ & HAS_PREVPC);
    if (FrameRegs *regs = cx->maybeRegs()) {
        prev_ = regs->fp();
        prevpc_ = regs->pc;
        JS_ASSERT(uint32_t(prevpc_ - prev_->script()->code) < prev_->script()->length);
    } else {
        prev_ = NULL;
#ifdef DEBUG
        prevpc_ = (jsbytecode *)0xbadc;
#endif
    }
}

inline void
StackFrame::resetGeneratorPrev(JSContext *cx)
{
    flags_ |= HAS_PREVPC;
    initPrev(cx);
}

inline void
StackFrame::initCallFrame(JSContext *cx, JSFunction &callee,
                          JSScript *script, uint32_t nactual, StackFrame::Flags flagsArg)
{
    JS_ASSERT((flagsArg & ~(CONSTRUCTING |
                            OVERFLOW_ARGS |
                            UNDERFLOW_ARGS)) == 0);
    JS_ASSERT(callee.nonLazyScript() == script);

    /* Initialize stack frame members. */
    flags_ = FUNCTION | HAS_PREVPC | HAS_SCOPECHAIN | HAS_BLOCKCHAIN | flagsArg;
    exec.fun = &callee;
    u.nactual = nactual;
    scopeChain_ = callee.environment();
    initPrev(cx);
    blockChain_= NULL;
    JS_ASSERT(!hasBlockChain());
    JS_ASSERT(!hasHookData());

    initVarsToUndefined();
}

inline void
StackFrame::initVarsToUndefined()
{
    SetValueRangeToUndefined(slots(), script()->nfixed);
}

inline JSObject *
StackFrame::createRestParameter(JSContext *cx)
{
    JS_ASSERT(fun()->hasRest());
    unsigned nformal = fun()->nargs - 1, nactual = numActualArgs();
    unsigned nrest = (nactual > nformal) ? nactual - nformal : 0;
    return NewDenseCopiedArray(cx, nrest, actuals() + nformal, NULL);
}

inline Value &
StackFrame::unaliasedVar(unsigned i, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT_IF(checkAliasing, !script()->varIsAliased(i));
    JS_ASSERT(i < script()->nfixed);
    return slots()[i];
}

inline Value &
StackFrame::unaliasedLocal(unsigned i, MaybeCheckAliasing checkAliasing)
{
#ifdef DEBUG
    CheckLocalUnaliased(checkAliasing, script(), maybeBlockChain(), i);
#endif
    return slots()[i];
}

inline Value &
StackFrame::unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT(i < numFormalArgs());
    JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
    JS_ASSERT_IF(checkAliasing, !script()->formalIsAliased(i));
    return formals()[i];
}

inline Value &
StackFrame::unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT(i < numActualArgs());
    JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
    JS_ASSERT_IF(checkAliasing && i < numFormalArgs(), !script()->formalIsAliased(i));
    return i < numFormalArgs() ? formals()[i] : actuals()[i];
}

template <class Op>
inline void
StackFrame::forEachUnaliasedActual(Op op)
{
    JS_ASSERT(!script()->funHasAnyAliasedFormal);
    JS_ASSERT(!script()->needsArgsObj());

    unsigned nformal = numFormalArgs();
    unsigned nactual = numActualArgs();

    const Value *formalsEnd = (const Value *)this;
    const Value *formals = formalsEnd - nformal;

    if (nactual <= nformal) {
        const Value *actualsEnd = formals + nactual;
        for (const Value *p = formals; p < actualsEnd; ++p)
            op(*p);
    } else {
        for (const Value *p = formals; p < formalsEnd; ++p)
            op(*p);

        const Value *actualsEnd = formals - 2;
        const Value *actuals = actualsEnd - nactual;
        for (const Value *p = actuals + nformal; p < actualsEnd; ++p)
            op(*p);
    }
}

struct CopyTo
{
    Value *dst;
    CopyTo(Value *dst) : dst(dst) {}
    void operator()(const Value &src) { *dst++ = src; }
};

struct CopyToHeap
{
    HeapValue *dst;
    CopyToHeap(HeapValue *dst) : dst(dst) {}
    void operator()(const Value &src) { dst->init(src); ++dst; }
};

inline unsigned
StackFrame::numFormalArgs() const
{
    JS_ASSERT(hasArgs());
    return fun()->nargs;
}

inline unsigned
StackFrame::numActualArgs() const
{
    /*
     * u.nactual is always coherent, except for method JIT frames where the
     * callee does not access its arguments and the number of actual arguments
     * matches the number of formal arguments. The JIT requires that all frames
     * which do not have an arguments object and use their arguments have a
     * coherent u.nactual (even though the below code may not use it), as
     * JIT code may access the field directly.
     */
    JS_ASSERT(hasArgs());
    if (JS_UNLIKELY(flags_ & (OVERFLOW_ARGS | UNDERFLOW_ARGS)))
        return u.nactual;
    return numFormalArgs();
}

inline ArgumentsObject &
StackFrame::argsObj() const
{
    JS_ASSERT(script()->needsArgsObj());
    JS_ASSERT(flags_ & HAS_ARGS_OBJ);
    return *argsObj_;
}

inline void
StackFrame::initArgsObj(ArgumentsObject &argsobj)
{
    JS_ASSERT(script()->needsArgsObj());
    flags_ |= HAS_ARGS_OBJ;
    argsObj_ = &argsobj;
}

inline ScopeObject &
StackFrame::aliasedVarScope(ScopeCoordinate sc) const
{
    JSObject *scope = &scopeChain()->asScope();
    for (unsigned i = sc.hops; i; i--)
        scope = &scope->asScope().enclosingScope();
    return scope->asScope();
}

inline void
StackFrame::pushOnScopeChain(ScopeObject &scope)
{
    JS_ASSERT(*scopeChain() == scope.enclosingScope() ||
              *scopeChain() == scope.asCall().enclosingScope().asDeclEnv().enclosingScope());
    scopeChain_ = &scope;
    flags_ |= HAS_SCOPECHAIN;
}

inline void
StackFrame::popOffScopeChain()
{
    JS_ASSERT(flags_ & HAS_SCOPECHAIN);
    scopeChain_ = &scopeChain_->asScope().enclosingScope();
}

inline CallObject &
StackFrame::callObj() const
{
    JS_ASSERT(fun()->isHeavyweight());

    JSObject *pobj = scopeChain();
    while (JS_UNLIKELY(!pobj->isCall()))
        pobj = pobj->enclosingScope();
    return pobj->asCall();
}

/*****************************************************************************/

STATIC_POSTCONDITION(!return || ubound(from) >= nvals)
JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *cx, MaybeReportError report, Value *from, ptrdiff_t nvals) const
{
    assertInvariants();
    JS_ASSERT(from >= firstUnused());
#ifdef XP_WIN
    JS_ASSERT(from <= commitEnd_);
#endif
    if (JS_UNLIKELY(conservativeEnd_ - from < nvals))
        return ensureSpaceSlow(cx, report, from, nvals);
    return true;
}

/*****************************************************************************/

JS_ALWAYS_INLINE StackFrame *
ContextStack::getCallFrame(JSContext *cx, MaybeReportError report, const CallArgs &args,
                           JSFunction *fun, HandleScript script, StackFrame::Flags *flags) const
{
    JS_ASSERT(fun->nonLazyScript() == script);
    unsigned nformal = fun->nargs;

    Value *firstUnused = args.end();
    JS_ASSERT(firstUnused == space().firstUnused());

    unsigned nvals = VALUES_PER_STACK_FRAME + script->nslots;

    /* Maintain layout invariant: &formals[0] == ((Value *)fp) - nformal. */

    if (args.length() == nformal) {
        if (!space().ensureSpace(cx, report, firstUnused, nvals))
            return NULL;
        return reinterpret_cast<StackFrame *>(firstUnused);
    }

    if (args.length() < nformal) {
        *flags = StackFrame::Flags(*flags | StackFrame::UNDERFLOW_ARGS);
        unsigned nmissing = nformal - args.length();
        if (!space().ensureSpace(cx, report, firstUnused, nmissing + nvals))
            return NULL;
        SetValueRangeToUndefined(firstUnused, nmissing);
        return reinterpret_cast<StackFrame *>(firstUnused + nmissing);
    }

    *flags = StackFrame::Flags(*flags | StackFrame::OVERFLOW_ARGS);
    unsigned ncopy = 2 + nformal;
    if (!space().ensureSpace(cx, report, firstUnused, ncopy + nvals))
        return NULL;
    Value *dst = firstUnused;
    Value *src = args.base();
    mozilla::PodCopy(dst, src, ncopy);
    return reinterpret_cast<StackFrame *>(firstUnused + ncopy);
}

JS_ALWAYS_INLINE bool
ContextStack::pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                              HandleFunction callee, HandleScript script,
                              InitialFrameFlags initial, MaybeReportError report)
{
    JS_ASSERT(onTop());
    JS_ASSERT(regs.sp == args.end());
    /* Cannot assert callee == args.callee() since this is called from LeaveTree. */
    JS_ASSERT(callee->nonLazyScript() == script);

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, report, args, callee, script, &flags);
    if (!fp)
        return false;

    /* Initialize frame, locals, regs. */
    fp->initCallFrame(cx, *callee, script, args.length(), flags);

    /*
     * N.B. regs may differ from the active registers, if the parent is about
     * to repoint the active registers to regs. See UncachedInlineCall.
     */
    regs.prepareToRun(*fp, script);
    return true;
}

JS_ALWAYS_INLINE bool
ContextStack::pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                              HandleFunction callee, HandleScript script,
                              InitialFrameFlags initial, Value **stackLimit)
{
    if (!pushInlineFrame(cx, regs, args, callee, script, initial))
        return false;
    *stackLimit = space().conservativeEnd_;
    return true;
}

JS_ALWAYS_INLINE void
ContextStack::popInlineFrame(FrameRegs &regs)
{
    JS_ASSERT(onTop());
    JS_ASSERT(&regs == &seg_->regs());

    StackFrame *fp = regs.fp();
    Value *newsp = fp->actuals() - 1;
    JS_ASSERT(newsp >= fp->prev()->base());

    newsp[-1] = fp->returnValue();
    regs.popFrame(newsp);
}

inline JSScript *
ContextStack::currentScript(jsbytecode **ppc,
                            MaybeAllowCrossCompartment allowCrossCompartment) const
{
    if (ppc)
        *ppc = NULL;

    if (!hasfp())
        return NULL;

    FrameRegs &regs = this->regs();
    StackFrame *fp = regs.fp();

#ifdef JS_ION
    if (fp->beginsIonActivation()) {
        JSScript *script = NULL;
        ion::GetPcScript(cx_, &script, ppc);
        if (!allowCrossCompartment && script->compartment() != cx_->compartment)
            return NULL;
        return script;
    }
#endif

    JSScript *script = fp->script();
    if (!allowCrossCompartment && script->compartment() != cx_->compartment)
        return NULL;

    if (ppc)
        *ppc = fp->pcQuadratic(*this);
    return script;
}

inline HandleObject
ContextStack::currentScriptedScopeChain() const
{
    return fp()->scopeChain();
}

template <class Op>
inline void
ScriptFrameIter::ionForEachCanonicalActualArg(JSContext *cx, Op op)
{
    JS_ASSERT(isIon());
#ifdef JS_ION
    if (data_.ionFrames_.isOptimizedJS()) {
        ionInlineFrames_.forEachCanonicalActualArg(cx, op, 0, -1);
    } else {
        JS_ASSERT(data_.ionFrames_.isBaselineJS());
        data_.ionFrames_.forEachCanonicalActualArg(op, 0, -1);
    }
#endif
}

inline void *
AbstractFramePtr::maybeHookData() const
{
    if (isStackFrame())
        return asStackFrame()->maybeHookData();
#ifdef JS_ION
    return asBaselineFrame()->maybeHookData();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline void
AbstractFramePtr::setHookData(void *data) const
{
    if (isStackFrame()) {
        asStackFrame()->setHookData(data);
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->setHookData(data);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value
AbstractFramePtr::returnValue() const
{
    if (isStackFrame())
        return asStackFrame()->returnValue();
#ifdef JS_ION
    return *asBaselineFrame()->returnValue();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline void
AbstractFramePtr::setReturnValue(const Value &rval) const
{
    if (isStackFrame()) {
        asStackFrame()->setReturnValue(rval);
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->setReturnValue(rval);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline bool
AbstractFramePtr::hasPushedSPSFrame() const
{
    if (isStackFrame())
        return asStackFrame()->hasPushedSPSFrame();
#ifdef JS_ION
    return asBaselineFrame()->hasPushedSPSFrame();
#else
    JS_NOT_REACHED("Invalid frame");
    return false;
#endif
}

inline JSObject *
AbstractFramePtr::scopeChain() const
{
    if (isStackFrame())
        return asStackFrame()->scopeChain();
#ifdef JS_ION
    return asBaselineFrame()->scopeChain();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline CallObject &
AbstractFramePtr::callObj() const
{
    if (isStackFrame())
        return asStackFrame()->callObj();
#ifdef JS_ION
    return asBaselineFrame()->callObj();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline bool
AbstractFramePtr::initFunctionScopeObjects(JSContext *cx)
{
    if (isStackFrame())
        return asStackFrame()->initFunctionScopeObjects(cx);
#ifdef JS_ION
    return asBaselineFrame()->initFunctionScopeObjects(cx);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline JSCompartment *
AbstractFramePtr::compartment() const
{
    return scopeChain()->compartment();
}

inline unsigned
AbstractFramePtr::numActualArgs() const
{
    if (isStackFrame())
        return asStackFrame()->numActualArgs();
#ifdef JS_ION
    return asBaselineFrame()->numActualArgs();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline unsigned
AbstractFramePtr::numFormalArgs() const
{
    if (isStackFrame())
        return asStackFrame()->numFormalArgs();
#ifdef JS_ION
    return asBaselineFrame()->numFormalArgs();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value &
AbstractFramePtr::unaliasedVar(unsigned i, MaybeCheckAliasing checkAliasing)
{
    if (isStackFrame())
        return asStackFrame()->unaliasedVar(i, checkAliasing);
#ifdef JS_ION
    return asBaselineFrame()->unaliasedVar(i, checkAliasing);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value &
AbstractFramePtr::unaliasedLocal(unsigned i, MaybeCheckAliasing checkAliasing)
{
    if (isStackFrame())
        return asStackFrame()->unaliasedLocal(i, checkAliasing);
#ifdef JS_ION
    return asBaselineFrame()->unaliasedLocal(i, checkAliasing);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value &
AbstractFramePtr::unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing)
{
    if (isStackFrame())
        return asStackFrame()->unaliasedFormal(i, checkAliasing);
#ifdef JS_ION
    return asBaselineFrame()->unaliasedFormal(i, checkAliasing);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value &
AbstractFramePtr::unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing)
{
    if (isStackFrame())
        return asStackFrame()->unaliasedActual(i, checkAliasing);
#ifdef JS_ION
    return asBaselineFrame()->unaliasedActual(i, checkAliasing);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline JSGenerator *
AbstractFramePtr::maybeSuspendedGenerator(JSRuntime *rt) const
{
    if (isStackFrame())
        return asStackFrame()->maybeSuspendedGenerator(rt);
    return NULL;
}

inline StaticBlockObject *
AbstractFramePtr::maybeBlockChain() const
{
    if (isStackFrame())
        return asStackFrame()->maybeBlockChain();
#ifdef JS_ION
    return asBaselineFrame()->maybeBlockChain();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::hasCallObj() const
{
    if (isStackFrame())
        return asStackFrame()->hasCallObj();
#ifdef JS_ION
    return asBaselineFrame()->hasCallObj();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::useNewType() const
{
    if (isStackFrame())
        return asStackFrame()->useNewType();
    return false;
}
inline bool
AbstractFramePtr::isGeneratorFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isGeneratorFrame();
    return false;
}
inline bool
AbstractFramePtr::isYielding() const
{
    if (isStackFrame())
        return asStackFrame()->isYielding();
    return false;
}
inline bool
AbstractFramePtr::isFunctionFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isFunctionFrame();
#ifdef JS_ION
    return asBaselineFrame()->isFunctionFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isGlobalFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isGlobalFrame();
#ifdef JS_ION
    return asBaselineFrame()->isGlobalFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isEvalFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isEvalFrame();
#ifdef JS_ION
    return asBaselineFrame()->isEvalFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isFramePushedByExecute() const
{
    return isGlobalFrame() || isEvalFrame();
}
inline bool
AbstractFramePtr::isDebuggerFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isDebuggerFrame();
#ifdef JS_ION
    return asBaselineFrame()->isDebuggerFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
    return false;
}
inline JSScript *
AbstractFramePtr::script() const
{
    if (isStackFrame())
        return asStackFrame()->script();
#ifdef JS_ION
    return asBaselineFrame()->script();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline JSFunction *
AbstractFramePtr::fun() const
{
    if (isStackFrame())
        return asStackFrame()->fun();
#ifdef JS_ION
    return asBaselineFrame()->fun();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline JSFunction *
AbstractFramePtr::maybeFun() const
{
    if (isStackFrame())
        return asStackFrame()->maybeFun();
#ifdef JS_ION
    return asBaselineFrame()->maybeFun();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline JSFunction *
AbstractFramePtr::callee() const
{
    if (isStackFrame())
        return &asStackFrame()->callee();
#ifdef JS_ION
    return asBaselineFrame()->callee();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline Value
AbstractFramePtr::calleev() const
{
    if (isStackFrame())
        return asStackFrame()->calleev();
#ifdef JS_ION
    return asBaselineFrame()->calleev();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isNonEvalFunctionFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isNonEvalFunctionFrame();
#ifdef JS_ION
    return asBaselineFrame()->isNonEvalFunctionFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isNonStrictDirectEvalFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isNonStrictDirectEvalFrame();
#ifdef JS_ION
    return asBaselineFrame()->isNonStrictDirectEvalFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::isStrictEvalFrame() const
{
    if (isStackFrame())
        return asStackFrame()->isStrictEvalFrame();
#ifdef JS_ION
    return asBaselineFrame()->isStrictEvalFrame();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value *
AbstractFramePtr::formals() const
{
    if (isStackFrame())
        return asStackFrame()->formals();
#ifdef JS_ION
    return asBaselineFrame()->formals();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline Value *
AbstractFramePtr::actuals() const
{
    if (isStackFrame())
        return asStackFrame()->actuals();
#ifdef JS_ION
    return asBaselineFrame()->actuals();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::hasArgsObj() const
{
    if (isStackFrame())
        return asStackFrame()->hasArgsObj();
#ifdef JS_ION
    return asBaselineFrame()->hasArgsObj();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline ArgumentsObject &
AbstractFramePtr::argsObj() const
{
    if (isStackFrame())
        return asStackFrame()->argsObj();
#ifdef JS_ION
    return asBaselineFrame()->argsObj();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline void
AbstractFramePtr::initArgsObj(ArgumentsObject &argsobj) const
{
    if (isStackFrame()) {
        asStackFrame()->initArgsObj(argsobj);
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->initArgsObj(argsobj);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline bool
AbstractFramePtr::copyRawFrameSlots(AutoValueVector *vec) const
{
    if (isStackFrame())
        return asStackFrame()->copyRawFrameSlots(vec);
#ifdef JS_ION
    return asBaselineFrame()->copyRawFrameSlots(vec);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline bool
AbstractFramePtr::prevUpToDate() const
{
    if (isStackFrame())
        return asStackFrame()->prevUpToDate();
#ifdef JS_ION
    return asBaselineFrame()->prevUpToDate();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}
inline void
AbstractFramePtr::setPrevUpToDate() const
{
    if (isStackFrame()) {
        asStackFrame()->setPrevUpToDate();
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->setPrevUpToDate();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline Value &
AbstractFramePtr::thisValue() const
{
    if (isStackFrame())
        return asStackFrame()->thisValue();
#ifdef JS_ION
    return asBaselineFrame()->thisValue();
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline void
AbstractFramePtr::popBlock(JSContext *cx) const
{
    if (isStackFrame()) {
        asStackFrame()->popBlock(cx);
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->popBlock(cx);
#else
    JS_NOT_REACHED("Invalid frame");
#endif
}

inline void
AbstractFramePtr::popWith(JSContext *cx) const
{
    if (isStackFrame())
        asStackFrame()->popWith(cx);
    else
        JS_NOT_REACHED("Invalid frame");
}

} /* namespace js */
#endif /* Stack_inl_h__ */
