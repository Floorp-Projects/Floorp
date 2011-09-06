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

#include "vm/Stack-inl.h"

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
    InvokeFrameGuard ifg_;
    Value savedCallee_, savedThis_;
    Value *formals_, *actuals_;
    unsigned nformals_;
    JSScript *script_;
    Value *stackLimit_;
    jsbytecode *stop_;

    bool optimized() const { return ifg_.pushed(); }

  public:
    InvokeSessionGuard() : args_(), ifg_() {}
    ~InvokeSessionGuard() {}

    bool start(JSContext *cx, const Value &callee, const Value &thisv, uintN argc);
    bool invoke(JSContext *cx);

    bool started() const {
        return args_.pushed();
    }

    Value &operator[](unsigned i) const {
        JS_ASSERT(i < argc());
        Value &arg = i < nformals_ ? formals_[i] : actuals_[i];
        JS_ASSERT_IF(optimized(), &arg == &ifg_.fp()->canonicalActualArg(i));
        JS_ASSERT_IF(!optimized(), &arg == &args_[i]);
        return arg;
    }

    uintN argc() const {
        return args_.argc();
    }

    const Value &rval() const {
        return optimized() ? ifg_.fp()->returnValue() : args_.rval();
    }
};

inline bool
InvokeSessionGuard::invoke(JSContext *cx)
{
    /* N.B. Must be kept in sync with Invoke */

    /* Refer to canonical (callee, this) for optimized() sessions. */
    formals_[-2] = savedCallee_;
    formals_[-1] = savedThis_;

    /* Prevent spurious accessing-callee-after-rval assert. */
    args_.calleeHasBeenReset();

    if (!optimized())
        return Invoke(cx, args_);

    /*
     * Update the types of each argument. The 'this' type and missing argument
     * types were handled when the invoke session was created.
     */
    for (unsigned i = 0; i < Min(argc(), nformals_); i++)
        types::TypeScript::SetArgument(cx, script_, i, (*this)[i]);

#ifdef JS_METHODJIT
    mjit::JITScript *jit = script_->getJIT(false /* !constructing */);
    if (!jit) {
        /* Watch in case the code was thrown away due a recompile. */
        mjit::CompileStatus status = mjit::TryCompile(cx, script_, false);
        if (status == mjit::Compile_Error)
            return false;
        JS_ASSERT(status == mjit::Compile_Okay);
        jit = script_->getJIT(false);
    }
    void *code;
    if (!(code = jit->invokeEntry))
        return Invoke(cx, args_);
#endif

    /* Clear any garbage left from the last Invoke. */
    StackFrame *fp = ifg_.fp();
    fp->resetCallFrame(script_);

    JSBool ok;
    {
        AutoPreserveEnumerators preserve(cx);
        args_.setActive();  /* From js::Invoke(InvokeArgsGuard) overload. */
        Probes::enterJSFun(cx, fp->fun(), script_);
#ifdef JS_METHODJIT
        ok = mjit::EnterMethodJIT(cx, fp, code, stackLimit_, /* partial = */ false);
        cx->regs().pc = stop_;
#else
        cx->regs().pc = script_->code;
        ok = Interpret(cx, cx->fp());
#endif
        Probes::exitJSFun(cx, fp->fun(), script_);
        args_.setInactive();
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
    static inline Class *getClass() { return &StringClass; }
};

template<>
class PrimitiveBehavior<bool> {
  public:
    static inline bool isType(const Value &v) { return v.isBoolean(); }
    static inline bool extract(const Value &v) { return v.toBoolean(); }
    static inline Class *getClass() { return &BooleanClass; }
};

template<>
class PrimitiveBehavior<double> {
  public:
    static inline bool isType(const Value &v) { return v.isNumber(); }
    static inline double extract(const Value &v) { return v.toNumber(); }
    static inline Class *getClass() { return &NumberClass; }
};

} // namespace detail

template <typename T>
inline bool
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
 * Compute the implicit |this| parameter for a call expression where the callee
 * funval was resolved from an unqualified name reference to a property on obj
 * (an object on the scope chain).
 *
 * We can avoid computing |this| eagerly and push the implicit callee-coerced
 * |this| value, undefined, if any of these conditions hold:
 *
 * 1. The callee funval is not an object.
 *
 * 2. The nominal |this|, obj, is a global object.
 *
 * 3. The nominal |this|, obj, has one of Block, Call, or DeclEnv class (this
 *    is what IsCacheableNonGlobalScope tests). Such objects-as-scopes must be
 *    censored with undefined.
 *
 * Only if funval is an object and obj is neither a declarative scope object to
 * be censored, nor a global object, do we bind |this| to obj->thisObject().
 * Only |with| statements and embedding-specific scope objects fall into this
 * last ditch.
 *
 * If funval is a strict mode function, then code implementing JSOP_THIS in the
 * interpreter and JITs will leave undefined as |this|. If funval is a function
 * not in strict mode, JSOP_THIS code replaces undefined with funval's global.
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

    if (obj->isGlobal())
        return true;

    if (IsCacheableNonGlobalScope(obj))
        return true;

    obj = obj->thisObject(cx);
    if (!obj)
        return false;

    vp->setObject(*obj);
    return true;
}

inline bool
ComputeThis(JSContext *cx, StackFrame *fp)
{
    Value &thisv = fp->thisValue();
    if (thisv.isObject())
        return true;
    if (fp->isFunctionFrame()) {
        if (fp->fun()->inStrictMode())
            return true;
        /*
         * Eval function frames have their own |this| slot, which is a copy of the function's
         * |this| slot. If we lazily wrap a primitive |this| in an eval function frame, the
         * eval's frame will get the wrapper, but the function's frame will not. To prevent
         * this, we always wrap a function's |this| before pushing an eval frame, and should
         * thus never see an unwrapped primitive in a non-strict eval function frame.
         */
        JS_ASSERT(!fp->isEvalFrame());
    }
    return BoxNonStrictThis(cx, fp->callReceiver());
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
ScriptPrologue(JSContext *cx, StackFrame *fp, bool newType)
{
    JS_ASSERT_IF(fp->isNonEvalFunctionFrame() && fp->fun()->isHeavyweight(), fp->hasCallObj());

    if (fp->isConstructing()) {
        JSObject *obj = js_CreateThisForFunction(cx, &fp->callee(), newType);
        if (!obj)
            return false;
        fp->functionThis().setObject(*obj);
    }

    Probes::enterJSFun(cx, fp->maybeFun(), fp->script());
    if (cx->compartment->debugMode())
        ScriptDebugPrologue(cx, fp);

    return true;
}

inline bool
ScriptEpilogue(JSContext *cx, StackFrame *fp, bool ok)
{
    Probes::exitJSFun(cx, fp->maybeFun(), fp->script());
    if (cx->compartment->debugMode())
        ok = ScriptDebugEpilogue(cx, fp, ok);

    /*
     * If inline-constructing, replace primitive rval with the new object
     * passed in via |this|, and instrument this constructor invocation.
     */
    if (fp->isConstructing() && ok) {
        if (fp->returnValue().isPrimitive())
            fp->setReturnValue(ObjectValue(fp->constructorThis()));
    }

    return ok;
}

inline bool
ScriptPrologueOrGeneratorResume(JSContext *cx, StackFrame *fp, bool newType)
{
    if (!fp->isGeneratorFrame())
        return ScriptPrologue(cx, fp, newType);
    if (cx->compartment->debugMode())
        ScriptDebugPrologue(cx, fp);
    return true;
}

inline bool
ScriptEpilogueOrGeneratorYield(JSContext *cx, StackFrame *fp, bool ok)
{
    if (!fp->isYielding())
        return ScriptEpilogue(cx, fp, ok);
    if (cx->compartment->debugMode())
        return ScriptDebugEpilogue(cx, fp, ok);
    return ok;
}

}  /* namespace js */

#endif /* jsinterpinlines_h__ */
