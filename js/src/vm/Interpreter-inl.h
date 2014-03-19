/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Interpreter_inl_h
#define vm_Interpreter_inl_h

#include "vm/Interpreter.h"

#include "jscompartment.h"
#include "jsinfer.h"
#include "jsnum.h"
#include "jsstr.h"

#include "jit/Ion.h"
#include "vm/ArgumentsObject.h"
#include "vm/ForkJoin.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

namespace js {

inline bool
ComputeThis(JSContext *cx, AbstractFramePtr frame)
{
    JS_ASSERT_IF(frame.isInterpreterFrame(), !frame.asInterpreterFrame()->runningInJit());
    if (frame.thisValue().isObject())
        return true;
    RootedValue thisv(cx, frame.thisValue());
    if (frame.isFunctionFrame()) {
        if (frame.fun()->strict() || frame.fun()->isSelfHostedBuiltin())
            return true;
        /*
         * Eval function frames have their own |this| slot, which is a copy of the function's
         * |this| slot. If we lazily wrap a primitive |this| in an eval function frame, the
         * eval's frame will get the wrapper, but the function's frame will not. To prevent
         * this, we always wrap a function's |this| before pushing an eval frame, and should
         * thus never see an unwrapped primitive in a non-strict eval function frame. Null
         * and undefined |this| values will unwrap to the same object in the function and
         * eval frames, so are not required to be wrapped.
         */
        JS_ASSERT_IF(frame.isEvalFrame(), thisv.isUndefined() || thisv.isNull());
    }

    JSObject *thisObj = BoxNonStrictThis(cx, thisv);
    if (!thisObj)
        return false;

    frame.thisValue().setObject(*thisObj);
    return true;
}

/*
 * Every possible consumer of MagicValue(JS_OPTIMIZED_ARGUMENTS) (as determined
 * by ScriptAnalysis::needsArgsObj) must check for these magic values and, when
 * one is received, act as if the value were the function's ArgumentsObject.
 * Additionally, it is possible that, after 'arguments' was copied into a
 * temporary, the arguments object has been created a some other failed guard
 * that called JSScript::argumentsOptimizationFailed. In this case, it is
 * always valid (and necessary) to replace JS_OPTIMIZED_ARGUMENTS with the real
 * arguments object.
 */
static inline bool
IsOptimizedArguments(AbstractFramePtr frame, Value *vp)
{
    if (vp->isMagic(JS_OPTIMIZED_ARGUMENTS) && frame.script()->needsArgsObj())
        *vp = ObjectValue(frame.argsObj());
    return vp->isMagic(JS_OPTIMIZED_ARGUMENTS);
}

/*
 * One optimized consumer of MagicValue(JS_OPTIMIZED_ARGUMENTS) is f.apply.
 * However, this speculation must be guarded before calling 'apply' in case it
 * is not the builtin Function.prototype.apply.
 */
static inline bool
GuardFunApplyArgumentsOptimization(JSContext *cx, AbstractFramePtr frame, HandleValue callee,
                                   Value *args, uint32_t argc)
{
    if (argc == 2 && IsOptimizedArguments(frame, &args[1])) {
        if (!IsNativeFunction(callee, js_fun_apply)) {
            RootedScript script(cx, frame.script());
            if (!JSScript::argumentsOptimizationFailed(cx, script))
                return false;
            args[1] = ObjectValue(frame.argsObj());
        }
    }

    return true;
}

/*
 * Return an object on which we should look for the properties of |value|.
 * This helps us implement the custom [[Get]] method that ES5's GetValue
 * algorithm uses for primitive values, without actually constructing the
 * temporary object that the specification does.
 *
 * For objects, return the object itself. For string, boolean, and number
 * primitive values, return the appropriate constructor's prototype. For
 * undefined and null, throw an error and return nullptr, attributing the
 * problem to the value at |spindex| on the stack.
 */
MOZ_ALWAYS_INLINE JSObject *
ValuePropertyBearer(JSContext *cx, InterpreterFrame *fp, HandleValue v, int spindex)
{
    if (v.isObject())
        return &v.toObject();

    Rooted<GlobalObject*> global(cx, &fp->global());

    if (v.isString())
        return GlobalObject::getOrCreateStringPrototype(cx, global);
    if (v.isNumber())
        return GlobalObject::getOrCreateNumberPrototype(cx, global);
    if (v.isBoolean())
        return GlobalObject::getOrCreateBooleanPrototype(cx, global);

    JS_ASSERT(v.isNull() || v.isUndefined());
    js_ReportIsNullOrUndefined(cx, spindex, v, NullPtr());
    return nullptr;
}

inline bool
GetLengthProperty(const Value &lval, MutableHandleValue vp)
{
    /* Optimize length accesses on strings, arrays, and arguments. */
    if (lval.isString()) {
        vp.setInt32(lval.toString()->length());
        return true;
    }
    if (lval.isObject()) {
        JSObject *obj = &lval.toObject();
        if (obj->is<ArrayObject>()) {
            vp.setNumber(obj->as<ArrayObject>().length());
            return true;
        }

        if (obj->is<ArgumentsObject>()) {
            ArgumentsObject *argsobj = &obj->as<ArgumentsObject>();
            if (!argsobj->hasOverriddenLength()) {
                uint32_t length = argsobj->initialLength();
                JS_ASSERT(length < INT32_MAX);
                vp.setInt32(int32_t(length));
                return true;
            }
        }
    }

    return false;
}

template <bool TypeOf> inline bool
FetchName(JSContext *cx, HandleObject obj, HandleObject obj2, HandlePropertyName name,
          HandleShape shape, MutableHandleValue vp)
{
    if (!shape) {
        if (TypeOf) {
            vp.setUndefined();
            return true;
        }
        JSAutoByteString printable;
        if (AtomToPrintableString(cx, name, &printable))
            js_ReportIsNotDefined(cx, printable.ptr());
        return false;
    }

    /* Take the slow path if shape was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        Rooted<jsid> id(cx, NameToId(name));
        if (!JSObject::getGeneric(cx, obj, obj, id, vp))
            return false;
    } else {
        Rooted<JSObject*> normalized(cx, obj);
        if (normalized->is<DynamicWithObject>() && !shape->hasDefaultGetter())
            normalized = &normalized->as<DynamicWithObject>().object();
        if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {
            /* Fast path for Object instance properties. */
            JS_ASSERT(shape->hasSlot());
            vp.set(obj2->nativeGetSlot(shape->slot()));
        } else if (!NativeGet(cx, normalized, obj2, shape, vp)) {
            return false;
        }
    }
    return true;
}

inline bool
FetchNameNoGC(JSObject *pobj, Shape *shape, MutableHandleValue vp)
{
    if (!shape || !pobj->isNative() || !shape->isDataDescriptor() || !shape->hasDefaultGetter())
        return false;

    vp.set(pobj->nativeGetSlot(shape->slot()));
    return true;
}

inline bool
GetIntrinsicOperation(JSContext *cx, jsbytecode *pc, MutableHandleValue vp)
{
    RootedPropertyName name(cx, cx->currentScript()->getName(pc));
    return GlobalObject::getIntrinsicValue(cx, cx->global(), name, vp);
}

inline bool
SetIntrinsicOperation(JSContext *cx, JSScript *script, jsbytecode *pc, HandleValue val)
{
    RootedPropertyName name(cx, script->getName(pc));
    return cx->global()->setIntrinsicValue(cx, name, val);
}

inline bool
SetNameOperation(JSContext *cx, JSScript *script, jsbytecode *pc, HandleObject scope,
                 HandleValue val)
{
    JS_ASSERT(*pc == JSOP_SETNAME || *pc == JSOP_SETGNAME);
    JS_ASSERT_IF(*pc == JSOP_SETGNAME, scope == cx->global());

    bool strict = script->strict();
    RootedPropertyName name(cx, script->getName(pc));
    RootedValue valCopy(cx, val);

    /*
     * In strict-mode, we need to trigger an error when trying to assign to an
     * undeclared global variable. To do this, we call SetPropertyHelper
     * directly and pass DNP_UNQUALIFIED.
     */
    if (scope->is<GlobalObject>()) {
        JS_ASSERT(!scope->getOps()->setProperty);
        RootedId id(cx, NameToId(name));
        return baseops::SetPropertyHelper<SequentialExecution>(cx, scope, scope, id,
                                                               DNP_UNQUALIFIED, &valCopy, strict);
    }

    return JSObject::setProperty(cx, scope, scope, name, &valCopy, strict);
}

inline bool
DefVarOrConstOperation(JSContext *cx, HandleObject varobj, HandlePropertyName dn, unsigned attrs)
{
    JS_ASSERT(varobj->isVarObj());

    RootedShape prop(cx);
    RootedObject obj2(cx);
    if (!JSObject::lookupProperty(cx, varobj, dn, &obj2, &prop))
        return false;

    /* Steps 8c, 8d. */
    if (!prop || (obj2 != varobj && varobj->is<GlobalObject>())) {
        if (!JSObject::defineProperty(cx, varobj, dn, UndefinedHandleValue, JS_PropertyStub,
                                      JS_StrictPropertyStub, attrs)) {
            return false;
        }
    } else if (attrs & JSPROP_READONLY) {
        /*
         * Extension: ordinarily we'd be done here -- but for |const|.  If we
         * see a redeclaration that's |const|, we consider it a conflict.
         */
        unsigned oldAttrs;
        RootedId id(cx, NameToId(dn));
        if (!JSObject::getGenericAttributes(cx, varobj, id, &oldAttrs))
            return false;

        JSAutoByteString bytes;
        if (AtomToPrintableString(cx, dn, &bytes)) {
            JS_ALWAYS_FALSE(JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                                         js_GetErrorMessage,
                                                         nullptr, JSMSG_REDECLARED_VAR,
                                                         (oldAttrs & JSPROP_READONLY)
                                                         ? "const"
                                                         : "var",
                                                         bytes.ptr()));
        }
        return false;
    }

    return true;
}

static MOZ_ALWAYS_INLINE bool
NegOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue val,
             MutableHandleValue res)
{
    /*
     * When the operand is int jsval, INT32_FITS_IN_JSVAL(i) implies
     * INT32_FITS_IN_JSVAL(-i) unless i is 0 or INT32_MIN when the
     * results, -0.0 or INT32_MAX + 1, are double values.
     */
    int32_t i;
    if (val.isInt32() && (i = val.toInt32()) != 0 && i != INT32_MIN) {
        res.setInt32(-i);
    } else {
        double d;
        if (!ToNumber(cx, val, &d))
            return false;
        res.setNumber(-d);
    }

    return true;
}

static MOZ_ALWAYS_INLINE bool
ToIdOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue objval,
              HandleValue idval, MutableHandleValue res)
{
    if (idval.isInt32()) {
        res.set(idval);
        return true;
    }

    JSObject *obj = ToObjectFromStack(cx, objval);
    if (!obj)
        return false;

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    res.set(IdToValue(id));
    return true;
}

static MOZ_ALWAYS_INLINE bool
GetObjectElementOperation(JSContext *cx, JSOp op, JSObject *objArg, bool wasObject,
                          HandleValue rref, MutableHandleValue res)
{
    JS_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    do {
        uint32_t index;
        if (IsDefinitelyIndex(rref, &index)) {
            if (JSObject::getElementNoGC(cx, objArg, objArg, index, res.address()))
                break;

            RootedObject obj(cx, objArg);
            if (!JSObject::getElement(cx, obj, obj, index, res))
                return false;
            objArg = obj;
            break;
        }

        JSAtom *name = ToAtom<NoGC>(cx, rref);
        if (name) {
            if (name->isIndex(&index)) {
                if (JSObject::getElementNoGC(cx, objArg, objArg, index, res.address()))
                    break;
            } else {
                if (JSObject::getPropertyNoGC(cx, objArg, objArg, name->asPropertyName(), res.address()))
                    break;
            }
        }

        RootedObject obj(cx, objArg);

        name = ToAtom<CanGC>(cx, rref);
        if (!name)
            return false;

        if (name->isIndex(&index)) {
            if (!JSObject::getElement(cx, obj, obj, index, res))
                return false;
        } else {
            if (!JSObject::getProperty(cx, obj, obj, name->asPropertyName(), res))
                return false;
        }

        objArg = obj;
    } while (0);

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLELEM && MOZ_UNLIKELY(res.isUndefined()) && wasObject) {
        RootedObject obj(cx, objArg);
        if (!OnUnknownMethod(cx, obj, rref, res))
            return false;
    }
#endif

    assertSameCompartmentDebugOnly(cx, res);
    return true;
}

static MOZ_ALWAYS_INLINE bool
GetElemOptimizedArguments(JSContext *cx, AbstractFramePtr frame, MutableHandleValue lref,
                          HandleValue rref, MutableHandleValue res, bool *done)
{
    JS_ASSERT(!*done);

    if (IsOptimizedArguments(frame, lref.address())) {
        if (rref.isInt32()) {
            int32_t i = rref.toInt32();
            if (i >= 0 && uint32_t(i) < frame.numActualArgs()) {
                res.set(frame.unaliasedActual(i));
                *done = true;
                return true;
            }
        }

        RootedScript script(cx, frame.script());
        if (!JSScript::argumentsOptimizationFailed(cx, script))
            return false;

        lref.set(ObjectValue(frame.argsObj()));
    }

    return true;
}

static MOZ_ALWAYS_INLINE bool
GetElementOperation(JSContext *cx, JSOp op, MutableHandleValue lref, HandleValue rref,
                    MutableHandleValue res)
{
    JS_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    uint32_t index;
    if (lref.isString() && IsDefinitelyIndex(rref, &index)) {
        JSString *str = lref.toString();
        if (index < str->length()) {
            str = cx->staticStrings().getUnitStringForElement(cx, str, index);
            if (!str)
                return false;
            res.setString(str);
            return true;
        }
    }

    bool isObject = lref.isObject();
    JSObject *obj = ToObjectFromStack(cx, lref);
    if (!obj)
        return false;
    return GetObjectElementOperation(cx, op, obj, isObject, rref, res);
}

static MOZ_ALWAYS_INLINE JSString *
TypeOfOperation(const Value &v, JSRuntime *rt)
{
    JSType type = js::TypeOfValue(v);
    return TypeName(type, *rt->commonNames);
}

static inline JSString *
TypeOfObjectOperation(JSObject *obj, JSRuntime *rt)
{
    JSType type = js::TypeOfObject(obj);
    return TypeName(type, *rt->commonNames);
}

static MOZ_ALWAYS_INLINE bool
InitElemOperation(JSContext *cx, HandleObject obj, HandleValue idval, HandleValue val)
{
    JS_ASSERT(!val.isMagic(JS_ELEMENTS_HOLE));

    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, idval, &id))
        return false;

    return JSObject::defineGeneric(cx, obj, id, val, nullptr, nullptr, JSPROP_ENUMERATE);
}

static MOZ_ALWAYS_INLINE bool
InitArrayElemOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, uint32_t index, HandleValue val)
{
    JSOp op = JSOp(*pc);
    JS_ASSERT(op == JSOP_INITELEM_ARRAY || op == JSOP_INITELEM_INC);

    JS_ASSERT(obj->is<ArrayObject>());

    /*
     * If val is a hole, do not call JSObject::defineElement. In this case,
     * if the current op is the last element initialiser, set the array length
     * to one greater than id.
     */
    if (val.isMagic(JS_ELEMENTS_HOLE)) {
        JSOp next = JSOp(*GetNextPc(pc));

        if ((op == JSOP_INITELEM_ARRAY && next == JSOP_ENDINIT) ||
            (op == JSOP_INITELEM_INC && next == JSOP_POP))
        {
            if (!SetLengthProperty(cx, obj, index + 1))
                return false;
        }
    } else {
        if (!JSObject::defineElement(cx, obj, index, val, nullptr, nullptr, JSPROP_ENUMERATE))
            return false;
    }

    if (op == JSOP_INITELEM_INC && index == INT32_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SPREAD_TOO_LARGE);
        return false;
    }

    return true;
}

#define RELATIONAL_OP(OP)                                                     \
    JS_BEGIN_MACRO                                                            \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if (lhs.isInt32() && rhs.isInt32()) {                                 \
            *res = lhs.toInt32() OP rhs.toInt32();                            \
        } else {                                                              \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, lhs))                         \
                return false;                                                 \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, rhs))                         \
                return false;                                                 \
            if (lhs.isString() && rhs.isString()) {                           \
                JSString *l = lhs.toString(), *r = rhs.toString();            \
                int32_t result;                                               \
                if (!CompareStrings(cx, l, r, &result))                       \
                    return false;                                             \
                *res = result OP 0;                                           \
            } else {                                                          \
                double l, r;                                                  \
                if (!ToNumber(cx, lhs, &l) || !ToNumber(cx, rhs, &r))         \
                    return false;;                                            \
                *res = (l OP r);                                              \
            }                                                                 \
        }                                                                     \
        return true;                                                          \
    JS_END_MACRO

static MOZ_ALWAYS_INLINE bool
LessThanOperation(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res) {
    RELATIONAL_OP(<);
}

static MOZ_ALWAYS_INLINE bool
LessThanOrEqualOperation(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res) {
    RELATIONAL_OP(<=);
}

static MOZ_ALWAYS_INLINE bool
GreaterThanOperation(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res) {
    RELATIONAL_OP(>);
}

static MOZ_ALWAYS_INLINE bool
GreaterThanOrEqualOperation(JSContext *cx, MutableHandleValue lhs, MutableHandleValue rhs, bool *res) {
    RELATIONAL_OP(>=);
}

static MOZ_ALWAYS_INLINE bool
BitNot(JSContext *cx, HandleValue in, int *out)
{
    int i;
    if (!ToInt32(cx, in, &i))
        return false;
    *out = ~i;
    return true;
}

static MOZ_ALWAYS_INLINE bool
BitXor(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left ^ right;
    return true;
}

static MOZ_ALWAYS_INLINE bool
BitOr(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left | right;
    return true;
}

static MOZ_ALWAYS_INLINE bool
BitAnd(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left & right;
    return true;
}

static MOZ_ALWAYS_INLINE bool
BitLsh(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int32_t left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = uint32_t(left) << (right & 31);
    return true;
}

static MOZ_ALWAYS_INLINE bool
BitRsh(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int32_t left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left >> (right & 31);
    return true;
}

static MOZ_ALWAYS_INLINE bool
UrshOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, MutableHandleValue out)
{
    uint32_t left;
    int32_t  right;
    if (!ToUint32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    left >>= right & 31;
    out.setNumber(uint32_t(left));
    return true;
}

#undef RELATIONAL_OP

inline JSFunction *
ReportIfNotFunction(JSContext *cx, HandleValue v, MaybeConstruct construct = NO_CONSTRUCT)
{
    if (v.isObject() && v.toObject().is<JSFunction>())
        return &v.toObject().as<JSFunction>();

    ReportIsNotFunction(cx, v, -1, construct);
    return nullptr;
}

/*
 * FastInvokeGuard is used to optimize calls to JS functions from natives written
 * in C++, for instance Array.map. If the callee is not Ion-compiled, this will
 * just call Invoke. If the callee has a valid IonScript, however, it will enter
 * Ion directly.
 */
class FastInvokeGuard
{
    InvokeArgs args_;
    RootedFunction fun_;
    RootedScript script_;
#ifdef JS_ION
    // Constructing an IonContext is pretty expensive due to the TLS access,
    // so only do this if we have to.
    bool useIon_;
#endif

  public:
    FastInvokeGuard(JSContext *cx, const Value &fval)
      : args_(cx)
      , fun_(cx)
      , script_(cx)
#ifdef JS_ION
      , useIon_(jit::IsIonEnabled(cx))
#endif
    {
        JS_ASSERT(!InParallelSection());
        initFunction(fval);
    }

    void initFunction(const Value &fval) {
        if (fval.isObject() && fval.toObject().is<JSFunction>()) {
            JSFunction *fun = &fval.toObject().as<JSFunction>();
            if (fun->isInterpreted())
                fun_ = fun;
        }
    }

    InvokeArgs &args() {
        return args_;
    }

    bool invoke(JSContext *cx) {
#ifdef JS_ION
        if (useIon_ && fun_) {
            if (!script_) {
                script_ = fun_->getOrCreateScript(cx);
                if (!script_)
                    return false;
            }
            JS_ASSERT(fun_->nonLazyScript() == script_);

            jit::MethodStatus status = jit::CanEnterUsingFastInvoke(cx, script_, args_.length());
            if (status == jit::Method_Error)
                return false;
            if (status == jit::Method_Compiled) {
                jit::IonExecStatus result = jit::FastInvoke(cx, fun_, args_);
                if (IsErrorStatus(result))
                    return false;

                JS_ASSERT(result == jit::IonExec_Ok);
                return true;
            }

            JS_ASSERT(status == jit::Method_Skipped);

            if (script_->canIonCompile()) {
                // This script is not yet hot. Since calling into Ion is much
                // faster here, bump the use count a bit to account for this.
                script_->incUseCount(5);
            }
        }
#endif

        return Invoke(cx, args_);
    }

  private:
    FastInvokeGuard(const FastInvokeGuard& other) MOZ_DELETE;
    const FastInvokeGuard& operator=(const FastInvokeGuard& other) MOZ_DELETE;
};

}  /* namespace js */

#endif /* vm_Interpreter_inl_h */
