/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Stack_inl_h
#define vm_Stack_inl_h

#include "vm/Stack.h"

#include "mozilla/PodOperations.h"

#include "jscntxt.h"

#include "jit/BaselineFrame.h"
#include "vm/ScopeObject.h"

#include "jit/BaselineFrame-inl.h"
#include "jit/IonFrameIterator-inl.h"

namespace js {

/*
 * We cache name lookup results only for the global object or for native
 * non-global objects without prototype or with prototype that never mutates,
 * see bug 462734 and bug 487039.
 */
static inline bool
IsCacheableNonGlobalScope(JSObject *obj)
{
    bool cacheable = (obj->is<CallObject>() || obj->is<BlockObject>() || obj->is<DeclEnvObject>());

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
StackFrame::initCallFrame(JSContext *cx, StackFrame *prev, jsbytecode *prevpc, Value *prevsp, JSFunction &callee,
                          JSScript *script, Value *argv, uint32_t nactual, StackFrame::Flags flagsArg)
{
    JS_ASSERT((flagsArg & ~CONSTRUCTING) == 0);
    JS_ASSERT(callee.nonLazyScript() == script);

    /* Initialize stack frame members. */
    flags_ = FUNCTION | HAS_SCOPECHAIN | HAS_BLOCKCHAIN | flagsArg;
    argv_ = argv;
    exec.fun = &callee;
    u.nactual = nactual;
    scopeChain_ = callee.environment();
    prev_ = prev;
    prevpc_ = prevpc;
    prevsp_ = prevsp;
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
    return argv()[i];
}

inline Value &
StackFrame::unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing)
{
    JS_ASSERT(i < numActualArgs());
    JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
    JS_ASSERT_IF(checkAliasing && i < numFormalArgs(), !script()->formalIsAliased(i));
    return argv()[i];
}

template <class Op>
inline void
StackFrame::forEachUnaliasedActual(Op op)
{
    JS_ASSERT(!script()->funHasAnyAliasedFormal);
    JS_ASSERT(!script()->needsArgsObj());

    const Value *argsEnd = argv() + numActualArgs();
    for (const Value *p = argv(); p < argsEnd; ++p)
        op(*p);
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
    JSObject *scope = &scopeChain()->as<ScopeObject>();
    for (unsigned i = sc.hops; i; i--)
        scope = &scope->as<ScopeObject>().enclosingScope();
    return scope->as<ScopeObject>();
}

inline void
StackFrame::pushOnScopeChain(ScopeObject &scope)
{
    JS_ASSERT(*scopeChain() == scope.enclosingScope() ||
              *scopeChain() == scope.as<CallObject>().enclosingScope().as<DeclEnvObject>().enclosingScope());
    scopeChain_ = &scope;
    flags_ |= HAS_SCOPECHAIN;
}

inline void
StackFrame::popOffScopeChain()
{
    JS_ASSERT(flags_ & HAS_SCOPECHAIN);
    scopeChain_ = &scopeChain_->as<ScopeObject>().enclosingScope();
}

bool
StackFrame::hasCallObj() const
{
    JS_ASSERT(isStrictEvalFrame() || fun()->isHeavyweight());
    return flags_ & HAS_CALL_OBJ;
}

inline CallObject &
StackFrame::callObj() const
{
    JS_ASSERT(fun()->isHeavyweight());

    JSObject *pobj = scopeChain();
    while (JS_UNLIKELY(!pobj->is<CallObject>()))
        pobj = pobj->enclosingScope();
    return pobj->as<CallObject>();
}

/*****************************************************************************/

inline void
InterpreterStack::purge(JSRuntime *rt)
{
    rt->freeLifoAlloc.transferUnusedFrom(&allocator_);
}

uint8_t *
InterpreterStack::allocateFrame(JSContext *cx, size_t size)
{
    if (JS_UNLIKELY(frameCount_ >= MAX_FRAMES)) {
        js_ReportOverRecursed(cx);
        return NULL;
    }

    uint8_t *buffer = reinterpret_cast<uint8_t *>(allocator_.alloc(size));
    if (!buffer)
        return NULL;

    frameCount_++;
    return buffer;
}

JS_ALWAYS_INLINE StackFrame *
InterpreterStack::getCallFrame(JSContext *cx, const CallArgs &args, HandleScript script,
                               StackFrame::Flags *flags, Value **pargv)
{
    JSFunction *fun = &args.callee().as<JSFunction>();

    JS_ASSERT(fun->nonLazyScript() == script);
    unsigned nformal = fun->nargs;
    unsigned nvals = script->nslots;

    if (args.length() >= nformal) {
        *pargv = args.array();
        uint8_t *buffer = allocateFrame(cx, sizeof(StackFrame) + nvals * sizeof(Value));
        return reinterpret_cast<StackFrame *>(buffer);
    }

    // Pad any missing arguments with |undefined|.
    JS_ASSERT(args.length() < nformal);

    nvals += nformal + 2; // Include callee, |this|.
    uint8_t *buffer = allocateFrame(cx, sizeof(StackFrame) + nvals * sizeof(Value));
    if (!buffer)
        return NULL;

    Value *argv = reinterpret_cast<Value *>(buffer);
    unsigned nmissing = nformal - args.length();

    mozilla::PodCopy(argv, args.base(), 2 + args.length());
    SetValueRangeToUndefined(argv + 2 + args.length(), nmissing);

    *pargv = argv + 2;
    return reinterpret_cast<StackFrame *>(argv + 2 + nformal);
}

JS_ALWAYS_INLINE bool
InterpreterStack::pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                                  HandleScript script, InitialFrameFlags initial)
{
    RootedFunction callee(cx, &args.callee().as<JSFunction>());
    JS_ASSERT(regs.sp == args.end());
    JS_ASSERT(callee->nonLazyScript() == script);

    StackFrame *prev = regs.fp();
    jsbytecode *prevpc = regs.pc;
    Value *prevsp = regs.sp;
    JS_ASSERT(prev);

    LifoAlloc::Mark mark = allocator_.mark();

    StackFrame::Flags flags = ToFrameFlags(initial);
    Value *argv;
    StackFrame *fp = getCallFrame(cx, args, script, &flags, &argv);
    if (!fp)
        return false;

    fp->mark_ = mark;

    /* Initialize frame, locals, regs. */
    fp->initCallFrame(cx, prev, prevpc, prevsp, *callee, script, argv, args.length(), flags);

    regs.prepareToRun(*fp, script);
    return true;
}

JS_ALWAYS_INLINE void
InterpreterStack::popInlineFrame(FrameRegs &regs)
{
    StackFrame *fp = regs.fp();
    regs.popInlineFrame();
    regs.sp[-1] = fp->returnValue();
    releaseFrame(fp);
    JS_ASSERT(regs.fp());
}

template <class Op>
inline void
ScriptFrameIter::ionForEachCanonicalActualArg(JSContext *cx, Op op)
{
    JS_ASSERT(isJit());
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
#endif
}

inline void
AbstractFramePtr::pushOnScopeChain(ScopeObject &scope)
{
    if (isStackFrame()) {
        asStackFrame()->pushOnScopeChain(scope);
        return;
    }
#ifdef JS_ION
    asBaselineFrame()->pushOnScopeChain(scope);
#else
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
#endif
}
inline JSScript *
AbstractFramePtr::script() const
{
    if (isStackFrame())
        return asStackFrame()->script();
#ifdef JS_ION
    return asBaselineFrame()->script();
#else
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
#endif
}

inline Value *
AbstractFramePtr::argv() const
{
    if (isStackFrame())
        return asStackFrame()->argv();
#ifdef JS_ION
    return asBaselineFrame()->argv();
#else
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
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
    MOZ_ASSUME_UNREACHABLE("Invalid frame");
#endif
}

inline void
AbstractFramePtr::popWith(JSContext *cx) const
{
    if (isStackFrame())
        asStackFrame()->popWith(cx);
    else
        MOZ_ASSUME_UNREACHABLE("Invalid frame");
}

Activation::Activation(JSContext *cx, Kind kind)
  : cx_(cx),
    compartment_(cx->compartment()),
    prev_(cx->mainThread().activation_),
    savedFrameChain_(0),
    kind_(kind)
{
    cx->mainThread().activation_ = this;
}

Activation::~Activation()
{
    JS_ASSERT(cx_->mainThread().activation_ == this);
    cx_->mainThread().activation_ = prev_;
}

InterpreterActivation::InterpreterActivation(JSContext *cx, StackFrame *entry, FrameRegs &regs,
                                             int *const switchMask)
  : Activation(cx, Interpreter),
    entry_(entry),
    regs_(regs),
    switchMask_(switchMask)
#ifdef DEBUG
  , oldFrameCount_(cx_->runtime()->interpreterStack().frameCount_)
#endif
{}

InterpreterActivation::~InterpreterActivation()
{
    // Pop all inline frames.
    while (regs_.fp() != entry_)
        popInlineFrame(regs_.fp());

    JS_ASSERT(oldFrameCount_ == cx_->runtime()->interpreterStack().frameCount_);
    JS_ASSERT_IF(oldFrameCount_ == 0, cx_->runtime()->interpreterStack().allocator_.used() == 0);
}

inline bool
InterpreterActivation::pushInlineFrame(const CallArgs &args, HandleScript script,
                                       InitialFrameFlags initial)
{
    if (!cx_->runtime()->interpreterStack().pushInlineFrame(cx_, regs_, args, script, initial))
        return false;
    JS_ASSERT(regs_.fp()->script()->compartment() == compartment_);
    return true;
}

inline void
InterpreterActivation::popInlineFrame(StackFrame *frame)
{
    (void)frame; // Quell compiler warning.
    JS_ASSERT(regs_.fp() == frame);
    JS_ASSERT(regs_.fp() != entry_);

    cx_->runtime()->interpreterStack().popInlineFrame(regs_);
}

} /* namespace js */

#endif /* vm_Stack_inl_h */
