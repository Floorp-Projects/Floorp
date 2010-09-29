/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Luke Wagner <lw@mozilla.com>
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

#ifndef jsinterpinlines_h__
#define jsinterpinlines_h__

inline void
JSStackFrame::initCallFrame(JSContext *cx, JSObject &callee, JSFunction *fun,
                            uint32 nactual, uint32 flagsArg)
{
    JS_ASSERT((flagsArg & ~(JSFRAME_CONSTRUCTING |
                            JSFRAME_OVERFLOW_ARGS |
                            JSFRAME_UNDERFLOW_ARGS)) == 0);
    JS_ASSERT(fun == callee.getFunctionPrivate());

    /* Initialize stack frame members. */
    flags_ = JSFRAME_FUNCTION | JSFRAME_HAS_PREVPC | flagsArg;
    exec.fun = fun;
    args.nactual = nactual;  /* only need to write if over/under-flow */
    scopeChain_ = callee.getParent();
    /* prevpc_, prev_ initialized by push*Frame */
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    rval_.setUndefined();
    blockChain_ = NULL;
    JS_ASSERT(annotation() == NULL);

    JS_ASSERT(!hasCallObj());
}

inline void
JSStackFrame::initCallFrameCallerHalf(JSContext *cx, JSObject &scopeChain,
                                      uint32 nactual, uint32 flagsArg)
{
    JS_ASSERT((flagsArg & ~(JSFRAME_CONSTRUCTING |
                            JSFRAME_FUNCTION |
                            JSFRAME_OVERFLOW_ARGS |
                            JSFRAME_UNDERFLOW_ARGS)) == 0);
    JSFrameRegs *regs = cx->regs;

    /* Initialize the caller half of the stack frame members. */
    flags_ = JSFRAME_FUNCTION | flagsArg;
    args.nactual = nactual;  /* only need to write if over/under-flow */
    scopeChain_ = &scopeChain;
    prev_ = regs->fp;
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    JS_ASSERT(annotation() == NULL);

    JS_ASSERT(!hasCallObj());
}

/*
 * The "early prologue" refers to the members that are stored for the benefit
 * of slow paths before initializing the rest of the members.
 */
inline void
JSStackFrame::initCallFrameEarlyPrologue(JSFunction *fun, void *ncode)
{
    /* Initialize state that gets set early in a jitted function's prologue. */
    exec.fun = fun;
    ncode_ = ncode;
}

/*
 * The "late prologue" refers to the members that are stored after having
 * checked for stack overflow and formal/actual arg mismatch.
 */
inline void
JSStackFrame::initCallFrameLatePrologue()
{
    rval_.setUndefined();
    blockChain_ = NULL;

    SetValueRangeToUndefined(slots(), script()->nfixed);
}

inline void
JSStackFrame::initEvalFrame(JSScript *script, JSStackFrame *prev,
                            jsbytecode *prevpc, uint32 flagsArg)
{
    JS_ASSERT(flagsArg & JSFRAME_EVAL);
    JS_ASSERT((flagsArg & ~(JSFRAME_EVAL | JSFRAME_DEBUGGER)) == 0);
    JS_ASSERT(prev->flags_ & (JSFRAME_FUNCTION | JSFRAME_GLOBAL));

    /* Copy (callee, thisv). */
    js::Value *dstvp = (js::Value *)this - 2;
    js::Value *srcvp = prev->flags_ & (JSFRAME_GLOBAL | JSFRAME_EVAL)
                       ? (js::Value *)prev - 2
                       : prev->formalArgs() - 2;
    dstvp[0] = srcvp[0];
    dstvp[1] = srcvp[1];
    JS_ASSERT_IF(prev->flags_ & JSFRAME_FUNCTION,
                 dstvp[0].toObject().isFunction());

    /* Initialize stack frame members. */
    flags_ = flagsArg | JSFRAME_HAS_PREVPC |
             (prev->flags_ & (JSFRAME_FUNCTION |
                              JSFRAME_GLOBAL |
                              JSFRAME_HAS_CALL_OBJ));
    if (isFunctionFrame()) {
        exec = prev->exec;
        args.script = script;
    } else {
        exec.script = script;
    }
    scopeChain_ = &prev->scopeChain();
    JS_ASSERT_IF(isFunctionFrame(), &callObj() == &prev->callObj());

    setPrev(prev, prevpc);
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    rval_.setUndefined();
    blockChain_ = NULL;
    setAnnotation(prev->annotation());
}

inline void
JSStackFrame::initGlobalFrame(JSScript *script, JSObject &chain, uint32 flagsArg)
{
    JS_ASSERT((flagsArg & ~(JSFRAME_EVAL | JSFRAME_DEBUGGER)) == 0);

    /* Initialize (callee, thisv). */
    js::Value *vp = (js::Value *)this - 2;
    vp[0].setUndefined();
    vp[1].setUndefined();  /* Set after frame pushed using thisObject */

    /* Initialize stack frame members. */
    flags_ = flagsArg | JSFRAME_GLOBAL | JSFRAME_HAS_PREVPC;
    exec.script = script;
    args.script = (JSScript *)0xbad;
    scopeChain_ = &chain;

    prev_ = NULL;
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    rval_.setUndefined();
    blockChain_ = NULL;
    JS_ASSERT(annotation() == NULL);
}

inline void
JSStackFrame::initDummyFrame(JSContext *cx, JSObject &chain)
{
    js::PodZero(this);
    flags_ = JSFRAME_DUMMY | JSFRAME_HAS_PREVPC;
    setPrev(cx->regs);
    chain.isGlobal();
    setScopeChainNoCallObj(chain);
}

inline void
JSStackFrame::stealFrameAndSlots(js::Value *vp, JSStackFrame *otherfp,
                                 js::Value *othervp, js::Value *othersp)
{
    JS_ASSERT(vp == (js::Value *)this - (otherfp->formalArgsEnd() - othervp));
    JS_ASSERT(othervp == otherfp->actualArgs() - 2);
    JS_ASSERT(othersp >= otherfp->slots());
    JS_ASSERT(othersp <= otherfp->base() + otherfp->numSlots());

    size_t nbytes = (othersp - othervp) * sizeof(js::Value);
    memcpy(vp, othervp, nbytes);
    JS_ASSERT(vp == actualArgs() - 2);

    /*
     * Repoint Call, Arguments, Block and With objects to the new live frame.
     * Call and Arguments are done directly because we have pointers to them.
     * Block and With objects are done indirectly through 'liveFrame'. See
     * js_LiveFrameToFloating comment in jsiter.h.
     */
    if (hasCallObj()) {
        callObj().setPrivate(this);
        otherfp->flags_ &= ~JSFRAME_HAS_CALL_OBJ;
    }
    if (hasArgsObj()) {
        argsObj().setPrivate(this);
        otherfp->flags_ &= ~JSFRAME_HAS_ARGS_OBJ;
    }
}

inline js::Value &
JSStackFrame::canonicalActualArg(uintN i) const
{
    if (i < numFormalArgs())
        return formalArg(i);
    JS_ASSERT(i < numActualArgs());
    return actualArgs()[i];
}

template <class Op>
inline void
JSStackFrame::forEachCanonicalActualArg(Op op)
{
    uintN nformal = fun()->nargs;
    js::Value *formals = formalArgsEnd() - nformal;
    uintN nactual = numActualArgs();
    if (nactual <= nformal) {
        uintN i = 0;
        js::Value *actualsEnd = formals + nactual;
        for (js::Value *p = formals; p != actualsEnd; ++p, ++i)
            op(i, p);
    } else {
        uintN i = 0;
        js::Value *formalsEnd = formalArgsEnd();
        for (js::Value *p = formals; p != formalsEnd; ++p, ++i)
            op(i, p);
        js::Value *actuals = formalsEnd - (nactual + 2);
        js::Value *actualsEnd = formals - 2;
        for (js::Value *p = actuals; p != actualsEnd; ++p, ++i)
            op(i, p);
    }
}

template <class Op>
inline void
JSStackFrame::forEachFormalArg(Op op)
{
    js::Value *formals = formalArgsEnd() - fun()->nargs;
    js::Value *formalsEnd = formalArgsEnd();
    uintN i = 0;
    for (js::Value *p = formals; p != formalsEnd; ++p, ++i)
        op(i, p);
}

inline JSObject *
JSStackFrame::computeThisObject(JSContext *cx)
{
    js::Value &thisv = thisValue();
    if (JS_LIKELY(!thisv.isPrimitive()))
        return &thisv.toObject();
    if (!js::ComputeThisFromArgv(cx, &thisv + 1))
        return NULL;
    JS_ASSERT(IsSaneThisObject(thisv.toObject()));
    return &thisv.toObject();
}

inline JSObject &
JSStackFrame::varobj(js::StackSegment *seg) const
{
    JS_ASSERT(seg->contains(this));
    return isFunctionFrame() ? callObj() : seg->getInitialVarObj();
}

inline JSObject &
JSStackFrame::varobj(JSContext *cx) const
{
    JS_ASSERT(cx->activeSegment()->contains(this));
    return isFunctionFrame() ? callObj() : cx->activeSegment()->getInitialVarObj();
}

inline uintN
JSStackFrame::numActualArgs() const
{
    JS_ASSERT(isFunctionFrame() && !isEvalFrame());
    if (JS_UNLIKELY(flags_ & (JSFRAME_OVERFLOW_ARGS | JSFRAME_UNDERFLOW_ARGS)))
        return hasArgsObj() ? argsObj().getArgsInitialLength() : args.nactual;
    return numFormalArgs();
}

inline js::Value *
JSStackFrame::actualArgs() const
{
    JS_ASSERT(isFunctionFrame() && !isEvalFrame());
    js::Value *argv = formalArgsEnd() - numFormalArgs();
    if (JS_UNLIKELY(flags_ & JSFRAME_OVERFLOW_ARGS)) {
        uintN nactual = hasArgsObj() ? argsObj().getArgsInitialLength() : args.nactual;
        return argv - (2 + nactual);
    }
    return argv;
}

inline js::Value *
JSStackFrame::actualArgsEnd() const
{
    JS_ASSERT(isFunctionFrame() && !isEvalFrame());
    if (JS_UNLIKELY(flags_ & JSFRAME_OVERFLOW_ARGS))
        return formalArgsEnd() - (2 + numFormalArgs());
    uintN argc = numActualArgs();
    uintN nmissing = numFormalArgs() - argc;
    return formalArgsEnd() - nmissing;
}

inline void
JSStackFrame::setArgsObj(JSObject &obj)
{
    JS_ASSERT_IF(hasArgsObj(), &obj == args.obj);
    JS_ASSERT_IF(!hasArgsObj(), numActualArgs() == obj.getArgsInitialLength());
    args.obj = &obj;
    flags_ |= JSFRAME_HAS_ARGS_OBJ;
}

inline void
JSStackFrame::clearArgsObj()
{
    JS_ASSERT(hasArgsObj());
    args.nactual = args.obj->getArgsInitialLength();
    flags_ ^= JSFRAME_HAS_ARGS_OBJ;
}

inline void
JSStackFrame::setScopeChainNoCallObj(JSObject &obj)
{
#ifdef DEBUG
    JS_ASSERT(&obj != NULL);
    JSObject *callObjBefore = maybeCallObj();
    if (!hasCallObj() && scopeChain_ != sInvalidScopeChain) {
        for (JSObject *pobj = scopeChain_; pobj; pobj = pobj->getParent())
            JS_ASSERT_IF(pobj->isCall(), pobj->getPrivate() != this);
    }
#endif
    scopeChain_ = &obj;
    JS_ASSERT(callObjBefore == maybeCallObj());
}

inline void
JSStackFrame::setScopeChainAndCallObj(JSObject &obj)
{
    JS_ASSERT(&obj != NULL);
    JS_ASSERT(!hasCallObj() && obj.isCall() && obj.getPrivate() == this);
    scopeChain_ = &obj;
    flags_ |= JSFRAME_HAS_CALL_OBJ;
}

inline void
JSStackFrame::clearCallObj()
{
    JS_ASSERT(hasCallObj());
    flags_ ^= JSFRAME_HAS_CALL_OBJ;
}

inline JSObject &
JSStackFrame::callObj() const
{
    JS_ASSERT(hasCallObj());
    JSObject *pobj = &scopeChain();
    while (JS_UNLIKELY(pobj->getClass() != &js_CallClass)) {
        JS_ASSERT(js_IsCacheableNonGlobalScope(pobj) || pobj->isWith());
        pobj = pobj->getParent();
    }
    return *pobj;
}

inline JSObject *
JSStackFrame::maybeCallObj() const
{
    return hasCallObj() ? &callObj() : NULL;
}

namespace js {

inline void
PutActivationObjects(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT(fp->isFunctionFrame() && !fp->isEvalFrame());

    /* The order is important as js_PutCallObject needs to access argsObj. */
    if (fp->hasCallObj()) {
        js_PutCallObject(cx, fp);
    } else if (fp->hasArgsObj()) {
        js_PutArgsObject(cx, fp);
    }
}

}

#endif /* jsinterpinlines_h__ */
