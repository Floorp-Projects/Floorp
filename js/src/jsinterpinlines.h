/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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

#include "jsapi.h"
#include "jsbool.h"
#include "jscompartment.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsprobes.h"
#include "jsstr.h"
#include "methodjit/MethodJIT.h"

#include "jsfuninlines.h"

inline void
JSStackFrame::initPrev(JSContext *cx)
{
    JS_ASSERT(flags_ & JSFRAME_HAS_PREVPC);
    if (JSFrameRegs *regs = cx->regs) {
        prev_ = regs->fp;
        prevpc_ = regs->pc;
        JS_ASSERT_IF(!prev_->isDummyFrame() && !prev_->hasImacropc(),
                     uint32(prevpc_ - prev_->script()->code) < prev_->script()->length);
    } else {
        prev_ = NULL;
#ifdef DEBUG
        prevpc_ = (jsbytecode *)0xbadc;
#endif
    }
}

inline void
JSStackFrame::resetGeneratorPrev(JSContext *cx)
{
    flags_ |= JSFRAME_HAS_PREVPC;
    initPrev(cx);
}

inline void
JSStackFrame::initCallFrame(JSContext *cx, JSObject &callee, JSFunction *fun,
                            uint32 nactual, uint32 flagsArg)
{
    JS_ASSERT((flagsArg & ~(JSFRAME_CONSTRUCTING |
                            JSFRAME_OVERFLOW_ARGS |
                            JSFRAME_UNDERFLOW_ARGS)) == 0);
    JS_ASSERT(fun == callee.getFunctionPrivate());

    /* Initialize stack frame members. */
    flags_ = JSFRAME_FUNCTION | JSFRAME_HAS_PREVPC | JSFRAME_HAS_SCOPECHAIN | flagsArg;
    exec.fun = fun;
    args.nactual = nactual;  /* only need to write if over/under-flow */
    scopeChain_ = callee.getParent();
    initPrev(cx);
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    JS_ASSERT(annotation() == NULL);
    JS_ASSERT(!hasCallObj());
}

inline void
JSStackFrame::resetInvokeCallFrame()
{
    /* Undo changes to frame made during execution; see initCallFrame */

    JS_ASSERT(!(flags_ & ~(JSFRAME_FUNCTION |
                           JSFRAME_OVERFLOW_ARGS |
                           JSFRAME_UNDERFLOW_ARGS |
                           JSFRAME_OVERRIDE_ARGS |
                           JSFRAME_HAS_PREVPC |
                           JSFRAME_HAS_RVAL |
                           JSFRAME_HAS_SCOPECHAIN |
                           JSFRAME_HAS_ANNOTATION |
                           JSFRAME_HAS_HOOK_DATA |
                           JSFRAME_HAS_CALL_OBJ |
                           JSFRAME_HAS_ARGS_OBJ |
                           JSFRAME_FINISHED_IN_INTERP)));

    /*
     * Since the stack frame is usually popped after PutActivationObjects,
     * these bits aren't cleared. The activation objects must have actually
     * been put, though.
     */
    JS_ASSERT_IF(flags_ & JSFRAME_HAS_CALL_OBJ, callObj().getPrivate() == NULL);
    JS_ASSERT_IF(flags_ & JSFRAME_HAS_ARGS_OBJ, argsObj().getPrivate() == NULL);

    flags_ &= JSFRAME_FUNCTION |
              JSFRAME_OVERFLOW_ARGS |
              JSFRAME_HAS_PREVPC |
              JSFRAME_UNDERFLOW_ARGS;

    JS_ASSERT(exec.fun == callee().getFunctionPrivate());
    scopeChain_ = callee().getParent();
}

inline void
JSStackFrame::initCallFrameCallerHalf(JSContext *cx, uint32 flagsArg,
                                      void *ncode)
{
    JS_ASSERT((flagsArg & ~(JSFRAME_CONSTRUCTING |
                            JSFRAME_FUNCTION |
                            JSFRAME_OVERFLOW_ARGS |
                            JSFRAME_UNDERFLOW_ARGS)) == 0);

    flags_ = JSFRAME_FUNCTION | flagsArg;
    prev_ = cx->regs->fp;
    ncode_ = ncode;
}

/*
 * The "early prologue" refers to the members that are stored for the benefit
 * of slow paths before initializing the rest of the members.
 */
inline void
JSStackFrame::initCallFrameEarlyPrologue(JSFunction *fun, uint32 nactual)
{
    exec.fun = fun;
    if (flags_ & (JSFRAME_OVERFLOW_ARGS | JSFRAME_UNDERFLOW_ARGS))
        args.nactual = nactual;
}

/*
 * The "late prologue" refers to the members that are stored after having
 * checked for stack overflow and formal/actual arg mismatch.
 */
inline void
JSStackFrame::initCallFrameLatePrologue()
{
    SetValueRangeToUndefined(slots(), script()->nfixed);
}

inline void
JSStackFrame::initEvalFrame(JSContext *cx, JSScript *script, JSStackFrame *prev, uint32 flagsArg)
{
    JS_ASSERT(flagsArg & JSFRAME_EVAL);
    JS_ASSERT((flagsArg & ~(JSFRAME_EVAL | JSFRAME_DEBUGGER)) == 0);
    JS_ASSERT(prev->isScriptFrame());

    /* Copy (callee, thisv). */
    js::Value *dstvp = (js::Value *)this - 2;
    js::Value *srcvp = prev->hasArgs()
                       ? prev->formalArgs() - 2
                       : (js::Value *)prev - 2;
    dstvp[0] = srcvp[0];
    dstvp[1] = srcvp[1];
    JS_ASSERT_IF(prev->isFunctionFrame(),
                 dstvp[0].toObject().isFunction());

    /* Initialize stack frame members. */
    flags_ = flagsArg | JSFRAME_HAS_PREVPC | JSFRAME_HAS_SCOPECHAIN |
             (prev->flags_ & (JSFRAME_FUNCTION | JSFRAME_GLOBAL));
    if (isFunctionFrame()) {
        exec = prev->exec;
        args.script = script;
    } else {
        exec.script = script;
    }

    scopeChain_ = &prev->scopeChain();
    prev_ = prev;
    prevpc_ = prev->pc(cx);
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
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
    flags_ = flagsArg | JSFRAME_GLOBAL | JSFRAME_HAS_PREVPC | JSFRAME_HAS_SCOPECHAIN;
    exec.script = script;
    args.script = (JSScript *)0xbad;
    scopeChain_ = &chain;
    prev_ = NULL;
    JS_ASSERT(!hasImacropc());
    JS_ASSERT(!hasHookData());
    JS_ASSERT(annotation() == NULL);
}

inline void
JSStackFrame::initDummyFrame(JSContext *cx, JSObject &chain)
{
    js::PodZero(this);
    flags_ = JSFRAME_DUMMY | JSFRAME_HAS_PREVPC | JSFRAME_HAS_SCOPECHAIN;
    initPrev(cx);
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
        otherfp->flags_ &= ~JSFRAME_HAS_CALL_OBJ;
        if (js_IsNamedLambda(fun())) {
            JSObject *env = obj.getParent();
            JS_ASSERT(env->getClass() == &js_DeclEnvClass);
            env->setPrivate(this);
        }
    }
    if (hasArgsObj()) {
        JSObject &args = argsObj();
        JS_ASSERT(args.isArguments());
        if (args.isNormalArguments())
            args.setPrivate(this);
        else
            JS_ASSERT(!args.getPrivate());
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
inline bool
JSStackFrame::forEachCanonicalActualArg(Op op)
{
    uintN nformal = fun()->nargs;
    js::Value *formals = formalArgsEnd() - nformal;
    uintN nactual = numActualArgs();
    if (nactual <= nformal) {
        uintN i = 0;
        js::Value *actualsEnd = formals + nactual;
        for (js::Value *p = formals; p != actualsEnd; ++p, ++i) {
            if (!op(i, p))
                return false;
        }
    } else {
        uintN i = 0;
        js::Value *formalsEnd = formalArgsEnd();
        for (js::Value *p = formals; p != formalsEnd; ++p, ++i) {
            if (!op(i, p))
                return false;
        }
        js::Value *actuals = formalsEnd - (nactual + 2);
        js::Value *actualsEnd = formals - 2;
        for (js::Value *p = actuals; p != actualsEnd; ++p, ++i) {
            if (!op(i, p))
                return false;
        }
    }
    return true;
}

template <class Op>
inline bool
JSStackFrame::forEachFormalArg(Op op)
{
    js::Value *formals = formalArgsEnd() - fun()->nargs;
    js::Value *formalsEnd = formalArgsEnd();
    uintN i = 0;
    for (js::Value *p = formals; p != formalsEnd; ++p, ++i) {
        if (!op(i, p))
            return false;
    }
    return true;
}

namespace js {

struct CopyTo
{
    Value *dst;
    CopyTo(Value *dst) : dst(dst) {}
    bool operator()(uintN, Value *src) {
        *dst++ = *src;
        return true;
    }
};

}

JS_ALWAYS_INLINE void
JSStackFrame::clearMissingArgs()
{
    if (flags_ & JSFRAME_UNDERFLOW_ARGS)
        SetValueRangeToUndefined(formalArgs() + numActualArgs(), formalArgsEnd());
}

inline bool
JSStackFrame::computeThis(JSContext *cx)
{
    js::Value &thisv = thisValue();
    if (thisv.isObject())
        return true;
    if (isFunctionFrame()) {
        if (fun()->inStrictMode())
            return true;
        /*
         * Eval function frames have their own |this| slot, which is a copy of the function's
         * |this| slot. If we lazily wrap a primitive |this| in an eval function frame, the
         * eval's frame will get the wrapper, but the function's frame will not. To prevent
         * this, we always wrap a function's |this| before pushing an eval frame, and should
         * thus never see an unwrapped primitive in a non-strict eval function frame.
         */
        JS_ASSERT(!isEvalFrame());
    }
    if (!js::BoxThisForVp(cx, &thisv - 1))
        return false;
    return true;
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
    JS_ASSERT(hasArgs());
    if (JS_UNLIKELY(flags_ & (JSFRAME_OVERFLOW_ARGS | JSFRAME_UNDERFLOW_ARGS)))
        return hasArgsObj() ? argsObj().getArgsInitialLength() : args.nactual;
    return numFormalArgs();
}

inline js::Value *
JSStackFrame::actualArgs() const
{
    JS_ASSERT(hasArgs());
    js::Value *argv = formalArgs();
    if (JS_UNLIKELY(flags_ & JSFRAME_OVERFLOW_ARGS)) {
        uintN nactual = hasArgsObj() ? argsObj().getArgsInitialLength() : args.nactual;
        return argv - (2 + nactual);
    }
    return argv;
}

inline js::Value *
JSStackFrame::actualArgsEnd() const
{
    JS_ASSERT(hasArgs());
    if (JS_UNLIKELY(flags_ & JSFRAME_OVERFLOW_ARGS))
        return formalArgs() - 2;
    return formalArgs() + numActualArgs();
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
JSStackFrame::setScopeChainNoCallObj(JSObject &obj)
{
#ifdef DEBUG
    JS_ASSERT(&obj != NULL);
    if (&obj != sInvalidScopeChain) {
        if (hasCallObj()) {
            JSObject *pobj = &obj;
            while (pobj && pobj->getPrivate() != this)
                pobj = pobj->getParent();
            JS_ASSERT(pobj);
        } else {
            for (JSObject *pobj = &obj; pobj; pobj = pobj->getParent())
                JS_ASSERT_IF(pobj->isCall(), pobj->getPrivate() != this);
        }
    }
#endif
    scopeChain_ = &obj;
    flags_ |= JSFRAME_HAS_SCOPECHAIN;
}

inline void
JSStackFrame::setScopeChainWithOwnCallObj(JSObject &obj)
{
    JS_ASSERT(&obj != NULL);
    JS_ASSERT(!hasCallObj() && obj.isCall() && obj.getPrivate() == this);
    scopeChain_ = &obj;
    flags_ |= JSFRAME_HAS_SCOPECHAIN | JSFRAME_HAS_CALL_OBJ;
}

inline JSObject &
JSStackFrame::callObj() const
{
    JS_ASSERT_IF(isNonEvalFunctionFrame() || isStrictEvalFrame(), hasCallObj());

    JSObject *pobj = &scopeChain();
    while (JS_UNLIKELY(pobj->getClass() != &js_CallClass)) {
        JS_ASSERT(js::IsCacheableNonGlobalScope(pobj) || pobj->isWith());
        pobj = pobj->getParent();
    }
    return *pobj;
}

inline void
JSStackFrame::markActivationObjectsAsPut()
{
    if (flags_ & (JSFRAME_HAS_ARGS_OBJ | JSFRAME_HAS_CALL_OBJ)) {
        if (hasArgsObj() && !argsObj().getPrivate()) {
            args.nactual = args.obj->getArgsInitialLength();
            flags_ &= ~JSFRAME_HAS_ARGS_OBJ;
        }
        if (hasCallObj() && !callObj().getPrivate()) {
            /*
             * For function frames, the call object may or may not have have an
             * enclosing DeclEnv object, so we use the callee's parent, since
             * it was the initial scope chain. For global (strict) eval frames,
             * there is no calle, but the call object's parent is the initial
             * scope chain.
             */
            scopeChain_ = isFunctionFrame()
                          ? callee().getParent()
                          : scopeChain_->getParent();
            flags_ &= ~JSFRAME_HAS_CALL_OBJ;
        }
    }
}

namespace js {

class AutoPreserveEnumerators {
    JSContext *cx;
    JSObject *enumerators;

  public:
    AutoPreserveEnumerators(JSContext *cx) : cx(cx), enumerators(cx->enumerators)
    {
    }

    ~AutoPreserveEnumerators()
    {
        cx->enumerators = enumerators;
    }
};

class InvokeSessionGuard
{
    InvokeArgsGuard args_;
    InvokeFrameGuard frame_;
    Value savedCallee_, savedThis_;
    Value *formals_, *actuals_;
    unsigned nformals_;
    JSScript *script_;
    Value *stackLimit_;
    jsbytecode *stop_;

    bool optimized() const { return frame_.pushed(); }

  public:
    InvokeSessionGuard() : args_(), frame_() {}
    ~InvokeSessionGuard() {}

    bool start(JSContext *cx, const Value &callee, const Value &thisv, uintN argc);
    bool invoke(JSContext *cx) const;

    bool started() const {
        return args_.pushed();
    }

    Value &operator[](unsigned i) const {
        JS_ASSERT(i < argc());
        Value &arg = i < nformals_ ? formals_[i] : actuals_[i];
        JS_ASSERT_IF(optimized(), &arg == &frame_.fp()->canonicalActualArg(i));
        JS_ASSERT_IF(!optimized(), &arg == &args_[i]);
        return arg;
    }

    uintN argc() const {
        return args_.argc();
    }

    const Value &rval() const {
        return optimized() ? frame_.fp()->returnValue() : args_.rval();
    }
};

inline bool
InvokeSessionGuard::invoke(JSContext *cx) const
{
    /* N.B. Must be kept in sync with Invoke */

    /* Refer to canonical (callee, this) for optimized() sessions. */
    formals_[-2] = savedCallee_;
    formals_[-1] = savedThis_;

#ifdef JS_METHODJIT
    void *code;
    if (!optimized() || !(code = script_->getJIT(false /* !constructing */)->invokeEntry))
#else
    if (!optimized())
#endif
        return Invoke(cx, args_, 0);

    /* Clear any garbage left from the last Invoke. */
    JSStackFrame *fp = frame_.fp();
    fp->clearMissingArgs();
    PutActivationObjects(cx, frame_.fp());
    fp->resetInvokeCallFrame();
    SetValueRangeToUndefined(fp->slots(), script_->nfixed);

    JSBool ok;
    {
        AutoPreserveEnumerators preserve(cx);
        Probes::enterJSFun(cx, fp->fun(), script_);
#ifdef JS_METHODJIT
        ok = mjit::EnterMethodJIT(cx, fp, code, stackLimit_);
        cx->regs->pc = stop_;
#else
        cx->regs->pc = script_->code;
        ok = Interpret(cx, cx->fp());
#endif
        Probes::exitJSFun(cx, fp->fun(), script_);
    }

    /* Don't clobber callee with rval; rval gets read from fp->rval. */
    return ok;
}

namespace detail {

template<typename T> class PrimitiveBehavior { };

template<>
class PrimitiveBehavior<JSString *> {
  public:
    static inline bool isType(const Value &v) { return v.isString(); }
    static inline JSString *extract(const Value &v) { return v.toString(); }
    static inline Class *getClass() { return &js_StringClass; }
};

template<>
class PrimitiveBehavior<bool> {
  public:
    static inline bool isType(const Value &v) { return v.isBoolean(); }
    static inline bool extract(const Value &v) { return v.toBoolean(); }
    static inline Class *getClass() { return &js_BooleanClass; }
};

template<>
class PrimitiveBehavior<double> {
  public:
    static inline bool isType(const Value &v) { return v.isNumber(); }
    static inline double extract(const Value &v) { return v.toNumber(); }
    static inline Class *getClass() { return &js_NumberClass; }
};

} // namespace detail

/*
 * Compute the implicit |this| parameter for a call expression where the callee
 * is an unqualified name reference.
 *
 * We can avoid computing |this| eagerly and push the implicit callee-coerced
 * |this| value, undefined, according to this decision tree:
 *
 * 1. If the called value, funval, is not an object, bind |this| to undefined.
 *
 * 2. The nominal |this|, obj, has one of Block, Call, or DeclEnv class (this
 *    is what IsCacheableNonGlobalScope tests). Such objects-as-scopes must be
 *    censored.
 *
 * 3. obj is a global. There are several sub-cases:
 *
 * a) obj is a proxy: we try unwrapping it (see jswrapper.cpp) in order to find
 *    a function object inside. If the proxy is not a wrapper, or else it wraps
 *    a non-function, then bind |this| to undefined per ES5-strict/Harmony.
 *
 *    [Else fall through with callee pointing to an unwrapped function object.]
 *
 * b) If callee is a function (after unwrapping if necessary), check whether it
 *    is interpreted and in strict mode. If so, then bind |this| to undefined
 *    per ES5 strict.
 *
 * c) Now check that callee is scoped by the same global object as the object
 *    in which its unqualified name was bound as a property. ES1-3 bound |this|
 *    to the name's "Reference base object", which in the context of multiple
 *    global objects may not be the callee's global. If globals match, bind
 *    |this| to undefined.
 *
 *    This is a backward compatibility measure; see bug 634590.
 *
 * 4. Finally, obj is neither a declarative scope object to be censored, nor a
 *    global where the callee requires no backward-compatible special handling
 *    or future-proofing based on (explicit or imputed by Harmony status in the
 *    proxy case) strict mode opt-in. Bind |this| to obj->thisObject().
 *
 * We set *vp to undefined early to reduce code size and bias this code for the
 * common and future-friendly cases.
 */
inline bool
ComputeImplicitThis(JSContext *cx, JSObject *obj, const Value &funval, Value *vp)
{
    vp->setUndefined();

    if (!funval.isObject())
        return true;

    if (!obj->isGlobal()) {
        if (IsCacheableNonGlobalScope(obj))
            return true;
    } else {
        JSObject *callee = &funval.toObject();

        if (callee->isProxy()) {
            callee = callee->unwrap();
            if (!callee->isFunction())
                return true; // treat any non-wrapped-function proxy as strict
        }
        if (callee->isFunction()) {
            JSFunction *fun = callee->getFunctionPrivate();
            if (fun->isInterpreted() && fun->inStrictMode())
                return true;
        }
        if (callee->getGlobal() == cx->fp()->scopeChain().getGlobal())
            return true;;
    }

    obj = obj->thisObject(cx);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
}

template <typename T>
bool
GetPrimitiveThis(JSContext *cx, Value *vp, T *v)
{
    typedef detail::PrimitiveBehavior<T> Behavior;

    const Value &thisv = vp[1];
    if (Behavior::isType(thisv)) {
        *v = Behavior::extract(thisv);
        return true;
    }

    if (thisv.isObject() && thisv.toObject().getClass() == Behavior::getClass()) {
        *v = Behavior::extract(thisv.toObject().getPrimitiveThis());
        return true;
    }

    ReportIncompatibleMethod(cx, vp, Behavior::getClass());
    return false;
}

/*
 * Return an object on which we should look for the properties of |value|.
 * This helps us implement the custom [[Get]] method that ES5's GetValue
 * algorithm uses for primitive values, without actually constructing the
 * temporary object that the specification does.
 * 
 * For objects, return the object itself. For string, boolean, and number
 * primitive values, return the appropriate constructor's prototype. For
 * undefined and null, throw an error and return NULL, attributing the
 * problem to the value at |spindex| on the stack.
 */
JS_ALWAYS_INLINE JSObject *
ValuePropertyBearer(JSContext *cx, const Value &v, int spindex)
{
    if (v.isObject())
        return &v.toObject();

    JSProtoKey protoKey;
    if (v.isString()) {
        protoKey = JSProto_String;
    } else if (v.isNumber()) {
        protoKey = JSProto_Number;
    } else if (v.isBoolean()) {
        protoKey = JSProto_Boolean;
    } else {
        JS_ASSERT(v.isNull() || v.isUndefined());
        js_ReportIsNullOrUndefined(cx, spindex, v, NULL);
        return NULL;
    }

    JSObject *pobj;
    if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
        return NULL;
    return pobj;
}

inline bool
ScriptPrologue(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT_IF(fp->isNonEvalFunctionFrame() && fp->fun()->isHeavyweight(), fp->hasCallObj());

    if (fp->isConstructing()) {
        JSObject *obj = js_CreateThisForFunction(cx, &fp->callee());
        if (!obj)
            return false;
        fp->functionThis().setObject(*obj);
    }

    if (cx->compartment->debugMode)
        ScriptDebugPrologue(cx, fp);
    return true;
}

inline bool
ScriptEpilogue(JSContext *cx, JSStackFrame *fp, bool ok)
{
    if (cx->compartment->debugMode)
        ok = ScriptDebugEpilogue(cx, fp, ok);

    /*
     * If inline-constructing, replace primitive rval with the new object
     * passed in via |this|, and instrument this constructor invocation.
     */
    if (fp->isConstructing() && ok) {
        if (fp->returnValue().isPrimitive())
            fp->setReturnValue(ObjectValue(fp->constructorThis()));
        JS_RUNTIME_METER(cx->runtime, constructs);
    }

    return ok;
}

inline bool
ScriptPrologueOrGeneratorResume(JSContext *cx, JSStackFrame *fp)
{
    if (!fp->isGeneratorFrame())
        return ScriptPrologue(cx, fp);
    if (cx->compartment->debugMode)
        ScriptDebugPrologue(cx, fp);
    return true;
}

inline bool
ScriptEpilogueOrGeneratorYield(JSContext *cx, JSStackFrame *fp, bool ok)
{
    if (!fp->isYielding())
        return ScriptEpilogue(cx, fp, ok);
    if (cx->compartment->debugMode)
        return ScriptDebugEpilogue(cx, fp, ok);
    return ok;
}

}

#endif /* jsinterpinlines_h__ */
