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

#ifndef Stack_inl_h__
#define Stack_inl_h__

#include "jscntxt.h"
#include "jscompartment.h"

#include "methodjit/MethodJIT.h"
#include "vm/Stack.h"

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
        scopeChain_ = callee().toFunction()->environment();
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
    JS_ASSERT_IF(isScriptFrame(), scopeChain()->compartment() == script()->compartment());
    return scopeChain()->compartment();
}

inline void
StackFrame::initPrev(JSContext *cx)
{
    JS_ASSERT(flags_ & HAS_PREVPC);
    if (FrameRegs *regs = cx->maybeRegs()) {
        prev_ = regs->fp();
        prevpc_ = regs->pc;
        prevInline_ = regs->inlined();
        JS_ASSERT_IF(!prev_->isDummyFrame(),
                     uint32_t(prevpc_ - prev_->script()->code) < prev_->script()->length);
    } else {
        prev_ = NULL;
#ifdef DEBUG
        prevpc_ = (jsbytecode *)0xbadc;
        prevInline_ = (JSInlinedSite *)0xbadc;
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
StackFrame::initInlineFrame(JSFunction *fun, StackFrame *prevfp, jsbytecode *prevpc)
{
    /*
     * Note: no need to ensure the scopeChain is instantiated for inline
     * frames. Functions which use the scope chain are never inlined.
     */
    flags_ = StackFrame::FUNCTION;
    exec.fun = fun;
    resetInlinePrev(prevfp, prevpc);
}

inline void
StackFrame::resetInlinePrev(StackFrame *prevfp, jsbytecode *prevpc)
{
    JS_ASSERT_IF(flags_ & StackFrame::HAS_PREVPC, prevInline_);
    flags_ |= StackFrame::HAS_PREVPC;
    prev_ = prevfp;
    prevpc_ = prevpc;
    prevInline_ = NULL;
}

inline void
StackFrame::initCallFrame(JSContext *cx, JSFunction &callee,
                          JSScript *script, uint32_t nactual, StackFrame::Flags flagsArg)
{
    JS_ASSERT((flagsArg & ~(CONSTRUCTING |
                            LOWERED_CALL_APPLY |
                            OVERFLOW_ARGS |
                            UNDERFLOW_ARGS)) == 0);
    JS_ASSERT(script == callee.script());

    /* Initialize stack frame members. */
    flags_ = FUNCTION | HAS_PREVPC | HAS_SCOPECHAIN | HAS_BLOCKCHAIN | flagsArg;
    exec.fun = &callee;
    u.nactual = nactual;
    scopeChain_ = callee.environment();
    ncode_ = NULL;
    initPrev(cx);
    blockChain_= NULL;
    JS_ASSERT(!hasBlockChain());
    JS_ASSERT(!hasHookData());
    JS_ASSERT(annotation() == NULL);
    JS_ASSERT(!hasCallObj());

    SetValueRangeToUndefined(slots(), script->nfixed);
}

/*
 * Reinitialize the StackFrame fields that have been initialized up to the
 * point of FixupArity in the function prologue.
 */
inline void
StackFrame::initFixupFrame(StackFrame *prev, StackFrame::Flags flags, void *ncode, unsigned nactual)
{
    JS_ASSERT((flags & ~(CONSTRUCTING |
                         LOWERED_CALL_APPLY |
                         FUNCTION |
                         OVERFLOW_ARGS |
                         UNDERFLOW_ARGS)) == 0);

    flags_ = FUNCTION | flags;
    prev_ = prev;
    ncode_ = ncode;
    u.nactual = nactual;
}

inline Value &
StackFrame::canonicalActualArg(unsigned i) const
{
    if (i < numFormalArgs())
        return formalArg(i);
    JS_ASSERT(i < numActualArgs());
    return actualArgs()[i];
}

template <class Op>
inline bool
StackFrame::forEachCanonicalActualArg(Op op, unsigned start /* = 0 */, unsigned count /* = unsigned(-1) */)
{
    unsigned nformal = fun()->nargs;
    JS_ASSERT(start <= nformal);

    Value *formals = formalArgsEnd() - nformal;
    unsigned nactual = numActualArgs();
    if (count == unsigned(-1))
        count = nactual - start;

    unsigned end = start + count;
    JS_ASSERT(end >= start);
    JS_ASSERT(end <= nactual);

    if (end <= nformal) {
        Value *p = formals + start;
        for (; start < end; ++p, ++start) {
            if (!op(start, p))
                return false;
        }
    } else {
        for (Value *p = formals + start; start < nformal; ++p, ++start) {
            if (!op(start, p))
                return false;
        }
        JS_ASSERT(start >= nformal);
        Value *actuals = formals - (nactual + 2) + start;
        for (Value *p = actuals; start < end; ++p, ++start) {
            if (!op(start, p))
                return false;
        }
    }
    return true;
}

template <class Op>
inline bool
StackFrame::forEachFormalArg(Op op)
{
    Value *formals = formalArgsEnd() - fun()->nargs;
    Value *formalsEnd = formalArgsEnd();
    unsigned i = 0;
    for (Value *p = formals; p != formalsEnd; ++p, ++i) {
        if (!op(i, p))
            return false;
    }
    return true;
}

struct CopyTo
{
    Value *dst;
    CopyTo(Value *dst) : dst(dst) {}
    bool operator()(unsigned, Value *src) {
        *dst++ = *src;
        return true;
    }
};

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

inline Value *
StackFrame::actualArgs() const
{
    JS_ASSERT(hasArgs());
    Value *argv = formalArgs();
    if (JS_UNLIKELY(flags_ & OVERFLOW_ARGS))
        return argv - (2 + u.nactual);
    return argv;
}

inline Value *
StackFrame::actualArgsEnd() const
{
    JS_ASSERT(hasArgs());
    if (JS_UNLIKELY(flags_ & OVERFLOW_ARGS))
        return formalArgs() - 2;
    return formalArgs() + numActualArgs();
}

inline void
StackFrame::setScopeChainNoCallObj(JSObject &obj)
{
#ifdef DEBUG
    JS_ASSERT(&obj != NULL);
    if (&obj != sInvalidScopeChain) {
        if (hasCallObj()) {
            JSObject *pobj = &obj;
            while (pobj && pobj->getPrivate() != this)
                pobj = pobj->enclosingScope();
            JS_ASSERT(pobj);
        } else {
            for (JSObject *pobj = &obj; pobj->isScope(); pobj = pobj->enclosingScope())
                JS_ASSERT_IF(pobj->isCall(), pobj->getPrivate() != this);
        }
    }
#endif
    scopeChain_ = &obj;
    flags_ |= HAS_SCOPECHAIN;
}

inline void
StackFrame::setScopeChainWithOwnCallObj(CallObject &obj)
{
    JS_ASSERT(&obj != NULL);
    JS_ASSERT(!hasCallObj() && obj.maybeStackFrame() == this);
    scopeChain_ = &obj;
    flags_ |= HAS_SCOPECHAIN | HAS_CALL_OBJ;
}

inline CallObject &
StackFrame::callObj() const
{
    JS_ASSERT_IF(isNonEvalFunctionFrame() || isStrictEvalFrame(), hasCallObj());

    JSObject *pobj = scopeChain();
    while (JS_UNLIKELY(!pobj->isCall()))
        pobj = pobj->enclosingScope();
    return pobj->asCall();
}

inline bool
StackFrame::maintainNestingState() const
{
    /*
     * Whether to invoke the nesting epilogue/prologue to maintain active
     * frame counts and check for reentrant outer functions.
     */
    return isNonEvalFunctionFrame() && !isGeneratorFrame() && script()->nesting();
}

inline bool
StackFrame::functionPrologue(JSContext *cx)
{
    JS_ASSERT(isNonEvalFunctionFrame());
    JS_ASSERT(!isGeneratorFrame());

    JSFunction *fun = this->fun();
    JSScript *script = fun->script();

    if (fun->isHeavyweight()) {
        if (!CallObject::createForFunction(cx, this))
            return false;
    } else {
        /* Force instantiation of the scope chain, for JIT frames. */
        scopeChain();
    }

    if (script->nesting()) {
        JS_ASSERT(maintainNestingState());
        types::NestingPrologue(cx, this);
    }

    return true;
}

inline void
StackFrame::functionEpilogue()
{
    JS_ASSERT(isNonEvalFunctionFrame());

    if (flags_ & (HAS_ARGS_OBJ | HAS_CALL_OBJ)) {
        if (hasCallObj())
            js_PutCallObject(this);
        if (hasArgsObj())
            js_PutArgsObject(this);
    }

    if (maintainNestingState())
        types::NestingEpilogue(this);
}

inline void
StackFrame::updateEpilogueFlags()
{
    if (flags_ & (HAS_ARGS_OBJ | HAS_CALL_OBJ)) {
        if (hasArgsObj() && !argsObj().maybeStackFrame())
            flags_ &= ~HAS_ARGS_OBJ;
        if (hasCallObj() && !callObj().maybeStackFrame()) {
            /*
             * For function frames, the call object may or may not have have an
             * enclosing DeclEnv object, so we use the callee's parent, since
             * it was the initial scope chain. For global (strict) eval frames,
             * there is no callee, but the call object's parent is the initial
             * scope chain.
             */
            scopeChain_ = isFunctionFrame()
                          ? callee().toFunction()->environment()
                          : &scopeChain_->asScope().enclosingScope();
            flags_ &= ~HAS_CALL_OBJ;
        }
    }

    /*
     * For outer/inner function frames, undo the active frame balancing so that
     * when we redo it in the epilogue we get the right final value. The other
     * nesting epilogue changes (update active args/vars) are idempotent.
     */
    if (maintainNestingState())
        script()->nesting()->activeFrames++;
}

/*****************************************************************************/

STATIC_POSTCONDITION(!return || ubound(from) >= nvals)
JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *cx, MaybeReportError report, Value *from, ptrdiff_t nvals,
                        JSCompartment *dest) const
{
    assertInvariants();
    JS_ASSERT(from >= firstUnused());
#ifdef XP_WIN
    JS_ASSERT(from <= commitEnd_);
#endif
    if (JS_UNLIKELY(conservativeEnd_ - from < nvals))
        return ensureSpaceSlow(cx, report, from, nvals, dest);
    return true;
}

inline Value *
StackSpace::getStackLimit(JSContext *cx, MaybeReportError report)
{
    FrameRegs &regs = cx->regs();
    unsigned nvals = regs.fp()->numSlots() + STACK_JIT_EXTRA;
    return ensureSpace(cx, report, regs.sp, nvals)
           ? conservativeEnd_
           : NULL;
}

/*****************************************************************************/

JS_ALWAYS_INLINE StackFrame *
ContextStack::getCallFrame(JSContext *cx, MaybeReportError report, const CallArgs &args,
                           JSFunction *fun, JSScript *script, StackFrame::Flags *flags) const
{
    JS_ASSERT(fun->script() == script);
    unsigned nformal = fun->nargs;

    Value *firstUnused = args.end();
    JS_ASSERT(firstUnused == space().firstUnused());

    /* Include extra space to satisfy the method-jit stackLimit invariant. */
    unsigned nvals = VALUES_PER_STACK_FRAME + script->nslots + StackSpace::STACK_JIT_EXTRA;

    /* Maintain layout invariant: &formalArgs[0] == ((Value *)fp) - nformal. */

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
    PodCopy(dst, src, ncopy);
    return reinterpret_cast<StackFrame *>(firstUnused + ncopy);
}

JS_ALWAYS_INLINE bool
ContextStack::pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                              JSFunction &callee, JSScript *script,
                              InitialFrameFlags initial)
{
    JS_ASSERT(onTop());
    JS_ASSERT(regs.sp == args.end());
    /* Cannot assert callee == args.callee() since this is called from LeaveTree. */
    JS_ASSERT(script == callee.script());

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, REPORT_ERROR, args, &callee, script, &flags);
    if (!fp)
        return false;

    /* Initialize frame, locals, regs. */
    fp->initCallFrame(cx, callee, script, args.length(), flags);

    /*
     * N.B. regs may differ from the active registers, if the parent is about
     * to repoint the active registers to regs. See UncachedInlineCall.
     */
    regs.prepareToRun(*fp, script);
    return true;
}

JS_ALWAYS_INLINE bool
ContextStack::pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                              JSFunction &callee, JSScript *script,
                              InitialFrameFlags initial, Value **stackLimit)
{
    if (!pushInlineFrame(cx, regs, args, callee, script, initial))
        return false;
    *stackLimit = space().conservativeEnd_;
    return true;
}

JS_ALWAYS_INLINE StackFrame *
ContextStack::getFixupFrame(JSContext *cx, MaybeReportError report,
                            const CallArgs &args, JSFunction *fun, JSScript *script,
                            void *ncode, InitialFrameFlags initial, Value **stackLimit)
{
    JS_ASSERT(onTop());
    JS_ASSERT(fun->script() == args.callee().toFunction()->script());
    JS_ASSERT(fun->script() == script);

    StackFrame::Flags flags = ToFrameFlags(initial);
    StackFrame *fp = getCallFrame(cx, report, args, fun, script, &flags);
    if (!fp)
        return NULL;

    /* Do not init late prologue or regs; this is done by jit code. */
    fp->initFixupFrame(cx->fp(), flags, ncode, args.length());

    *stackLimit = space().conservativeEnd_;
    return fp;
}

JS_ALWAYS_INLINE void
ContextStack::popInlineFrame(FrameRegs &regs)
{
    JS_ASSERT(onTop());
    JS_ASSERT(&regs == &seg_->regs());

    StackFrame *fp = regs.fp();
    fp->functionEpilogue();

    Value *newsp = fp->actualArgs() - 1;
    JS_ASSERT(newsp >= fp->prev()->base());

    newsp[-1] = fp->returnValue();
    regs.popFrame(newsp);
}

inline void
ContextStack::popFrameAfterOverflow()
{
    /* Restore the regs to what they were on entry to JSOP_CALL. */
    FrameRegs &regs = seg_->regs();
    StackFrame *fp = regs.fp();
    regs.popFrame(fp->actualArgsEnd());
}

inline JSScript *
ContextStack::currentScript(jsbytecode **ppc) const
{
    if (ppc)
        *ppc = NULL;

    FrameRegs *regs = maybeRegs();
    StackFrame *fp = regs ? regs->fp() : NULL;
    while (fp && fp->isDummyFrame())
        fp = fp->prev();
    if (!fp)
        return NULL;

#ifdef JS_METHODJIT
    mjit::CallSite *inlined = regs->inlined();
    if (inlined) {
        mjit::JITChunk *chunk = fp->jit()->chunk(regs->pc);
        JS_ASSERT(inlined->inlineIndex < chunk->nInlineFrames);
        mjit::InlineFrame *frame = &chunk->inlineFrames()[inlined->inlineIndex];
        JSScript *script = frame->fun->script();
        if (script->compartment() != cx_->compartment)
            return NULL;
        if (ppc)
            *ppc = script->code + inlined->pcOffset;
        return script;
    }
#endif

    JSScript *script = fp->script();
    if (script->compartment() != cx_->compartment)
        return NULL;

    if (ppc)
        *ppc = fp->pcQuadratic(*this);
    return script;
}

inline JSScript *
ContextStack::currentScriptWithDiagnostics(jsbytecode **ppc) const
{
    if (ppc)
        *ppc = NULL;

    FrameRegs *regs = maybeRegs();
    StackFrame *fp = regs ? regs->fp() : NULL;
    while (fp && fp->isDummyFrame())
        fp = fp->prev();
    if (!fp)
        *(int *) 0x10 = 0;

#ifdef JS_METHODJIT
    mjit::CallSite *inlined = regs->inlined();
    if (inlined) {
        mjit::JITChunk *chunk = fp->jit()->chunk(regs->pc);
        JS_ASSERT(inlined->inlineIndex < chunk->nInlineFrames);
        mjit::InlineFrame *frame = &chunk->inlineFrames()[inlined->inlineIndex];
        JSScript *script = frame->fun->script();
        if (script->compartment() != cx_->compartment)
            *(int *) 0x20 = 0;
        if (ppc)
            *ppc = script->code + inlined->pcOffset;
        return script;
    }
#endif

    JSScript *script = fp->script();
    if (script->compartment() != cx_->compartment)
        *(int *) 0x30 = 0;

    if (ppc)
        *ppc = fp->pcQuadratic(*this);
    if (!script)
        *(int *) 0x40 = 0;
    return script;
}

inline HandleObject
ContextStack::currentScriptedScopeChain() const
{
    return fp()->scopeChain();
}

} /* namespace js */
#endif /* Stack_inl_h__ */
