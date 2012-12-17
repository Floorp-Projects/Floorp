/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsinterpinlines_h__
#define jsinterpinlines_h__

#include "jsapi.h"
#include "jsbool.h"
#include "jscompartment.h"
#include "jsinfer.h"
#include "jsinterp.h"
#include "jslibmath.h"
#include "jsnum.h"
#include "jsprobes.h"
#include "jsstr.h"
#include "methodjit/MethodJIT.h"

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsopcodeinlines.h"
#include "jspropertycacheinlines.h"
#include "jstypedarrayinlines.h"

#ifdef JS_ION
#include "ion/Ion.h"
#include "ion/IonCompartment.h"
#endif

#include "vm/Stack-inl.h"

namespace js {

/*
 * Compute the implicit |this| parameter for a call expression where the callee
 * funval was resolved from an unqualified name reference to a property on obj
 * (an object on the scope chain).
 *
 * We can avoid computing |this| eagerly and push the implicit callee-coerced
 * |this| value, undefined, if any of these conditions hold:
 *
 * 1. The nominal |this|, obj, is a global object.
 *
 * 2. The nominal |this|, obj, has one of Block, Call, or DeclEnv class (this
 *    is what IsCacheableNonGlobalScope tests). Such objects-as-scopes must be
 *    censored with undefined.
 *
 * Otherwise, we bind |this| to obj->thisObject(). Only names inside |with|
 * statements and embedding-specific scope objects fall into this category.
 *
 * If the callee is a strict mode function, then code implementing JSOP_THIS
 * in the interpreter and JITs will leave undefined as |this|. If funval is a
 * function not in strict mode, JSOP_THIS code replaces undefined with funval's
 * global.
 *
 * We set *vp to undefined early to reduce code size and bias this code for the
 * common and future-friendly cases.
 */
inline bool
ComputeImplicitThis(JSContext *cx, HandleObject obj, Value *vp)
{
    vp->setUndefined();

    if (obj->isGlobal())
        return true;

    if (IsCacheableNonGlobalScope(obj))
        return true;

    RawObject nobj = JSObject::thisObject(cx, obj);
    if (!nobj)
        return false;

    vp->setObject(*nobj);
    return true;
}

inline bool
ComputeThis(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(!fp->runningInIon());
    Value &thisv = fp->thisValue();
    if (thisv.isObject())
        return true;
    if (fp->isFunctionFrame()) {
        if (fp->fun()->strict() || fp->fun()->isSelfHostedBuiltin())
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
IsOptimizedArguments(StackFrame *fp, Value *vp)
{
    AutoAssertNoGC nogc;
    if (vp->isMagic(JS_OPTIMIZED_ARGUMENTS) && fp->script()->needsArgsObj())
        *vp = ObjectValue(fp->argsObj());
    return vp->isMagic(JS_OPTIMIZED_ARGUMENTS);
}

/*
 * One optimized consumer of MagicValue(JS_OPTIMIZED_ARGUMENTS) is f.apply.
 * However, this speculation must be guarded before calling 'apply' in case it
 * is not the builtin Function.prototype.apply.
 */
static inline bool
GuardFunApplyArgumentsOptimization(JSContext *cx)
{
    AssertCanGC();
    FrameRegs &regs = cx->regs();
    if (IsOptimizedArguments(regs.fp(), &regs.sp[-1])) {
        CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
        if (!IsNativeFunction(args.calleev(), js_fun_apply)) {
            RootedScript script(cx, regs.fp()->script());
            if (!JSScript::argumentsOptimizationFailed(cx, script))
                return false;
            regs.sp[-1] = ObjectValue(regs.fp()->argsObj());
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
 * undefined and null, throw an error and return NULL, attributing the
 * problem to the value at |spindex| on the stack.
 */
JS_ALWAYS_INLINE JSObject *
ValuePropertyBearer(JSContext *cx, StackFrame *fp, HandleValue v, int spindex)
{
    if (v.isObject())
        return &v.toObject();

    GlobalObject &global = fp->global();

    if (v.isString())
        return global.getOrCreateStringPrototype(cx);
    if (v.isNumber())
        return global.getOrCreateNumberPrototype(cx);
    if (v.isBoolean())
        return global.getOrCreateBooleanPrototype(cx);

    JS_ASSERT(v.isNull() || v.isUndefined());
    js_ReportIsNullOrUndefined(cx, spindex, v, NullPtr());
    return NULL;
}

inline bool
NativeGet(JSContext *cx, Handle<JSObject*> obj, Handle<JSObject*> pobj, Shape *shapeArg,
          unsigned getHow, MutableHandleValue vp)
{
    if (shapeArg->isDataDescriptor() && shapeArg->hasDefaultGetter()) {
        /* Fast path for Object instance properties. */
        JS_ASSERT(shapeArg->hasSlot());
        vp.set(pobj->nativeGetSlot(shapeArg->slot()));
    } else {
        RootedShape shape(cx, shapeArg);
        if (!js_NativeGet(cx, obj, pobj, shape, getHow, vp))
            return false;
    }
    return true;
}

#if defined(DEBUG) && !defined(JS_THREADSAFE) && !defined(JSGC_ROOT_ANALYSIS)
extern void
AssertValidPropertyCacheHit(JSContext *cx, JSObject *start, JSObject *found,
                            PropertyCacheEntry *entry);
#else
inline void
AssertValidPropertyCacheHit(JSContext *cx, JSObject *start, JSObject *found,
                            PropertyCacheEntry *entry)
{}
#endif

inline bool
GetPropertyGenericMaybeCallXML(JSContext *cx, JSOp op, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    /*
     * Various XML properties behave differently when accessed in a
     * call vs. normal context, and getGeneric will not work right.
     */
#if JS_HAS_XML_SUPPORT
    if (op == JSOP_CALLPROP && obj->isXML())
        return js_GetXMLMethod(cx, obj, id, vp);
#endif

    return JSObject::getGeneric(cx, obj, obj, id, vp);
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
        if (obj->isArray()) {
            uint32_t length = obj->getArrayLength();
            vp.setNumber(length);
            return true;
        }

        if (obj->isArguments()) {
            ArgumentsObject *argsobj = &obj->asArguments();
            if (!argsobj->hasOverriddenLength()) {
                uint32_t length = argsobj->initialLength();
                JS_ASSERT(length < INT32_MAX);
                vp.setInt32(int32_t(length));
                return true;
            }
        }

        if (obj->isTypedArray()) {
            vp.setInt32(TypedArray::length(obj));
            return true;
        }
    }

    return false;
}

inline bool
GetPropertyOperation(JSContext *cx, JSScript *script, jsbytecode *pc, MutableHandleValue lval,
                     MutableHandleValue vp)
{
    JSOp op = JSOp(*pc);

    if (op == JSOP_LENGTH) {
        if (IsOptimizedArguments(cx->fp(), lval.address())) {
            vp.setInt32(cx->fp()->numActualArgs());
            return true;
        }

        if (GetLengthProperty(lval, vp))
            return true;
    }

    RootedObject obj(cx, ToObjectFromStack(cx, lval));
    if (!obj)
        return false;

    PropertyCacheEntry *entry;
    Rooted<JSObject*> obj2(cx);
    PropertyName *name;
    cx->propertyCache().test(cx, pc, obj.get(), obj2.get(), entry, name);
    if (!name) {
        AssertValidPropertyCacheHit(cx, obj, obj2, entry);
        if (!NativeGet(cx, obj, obj2, entry->prop, JSGET_CACHE_RESULT, vp))
            return false;
        return true;
    }

    RootedId id(cx, NameToId(name));

    if (obj->getOps()->getProperty) {
        if (!GetPropertyGenericMaybeCallXML(cx, op, obj, id, vp))
            return false;
    } else {
        if (!GetPropertyHelper(cx, obj, id, JSGET_CACHE_RESULT, vp))
            return false;
    }

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLPROP &&
        JS_UNLIKELY(vp.isPrimitive()) &&
        lval.isObject())
    {
        if (!OnUnknownMethod(cx, obj, IdToValue(id), vp))
            return false;
    }
#endif

    return true;
}

inline bool
SetPropertyOperation(JSContext *cx, jsbytecode *pc, HandleValue lval, HandleValue rval)
{
    JS_ASSERT(*pc == JSOP_SETPROP);

    RootedObject obj(cx, ToObjectFromStack(cx, lval));
    if (!obj)
        return false;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    PropertyName *name;
    if (cx->propertyCache().testForSet(cx, pc, obj, &entry, &obj2, &name)) {
        /*
         * Property cache hit, only partially confirmed by testForSet. We
         * know that the entry applies to regs.pc and that obj's shape
         * matches.
         *
         * The entry predicts a set either an existing "own" property, or
         * on a prototype property that has a setter.
         */
        RootedShape shape(cx, entry->prop);
        JS_ASSERT_IF(shape->isDataDescriptor(), shape->writable());
        JS_ASSERT_IF(shape->hasSlot(), entry->isOwnPropertyHit());

        if (entry->isOwnPropertyHit() ||
            ((obj2 = obj->getProto()) && obj2->lastProperty() == entry->pshape)) {
#ifdef DEBUG
            if (entry->isOwnPropertyHit()) {
                JS_ASSERT(obj->nativeLookupNoAllocation(shape->propid()) == shape);
            } else {
                JS_ASSERT(obj2->nativeLookupNoAllocation(shape->propid()) == shape);
                JS_ASSERT(entry->isPrototypePropertyHit());
                JS_ASSERT(entry->kshape != entry->pshape);
                JS_ASSERT(!shape->hasSlot());
            }
#endif

            if (shape->hasDefaultSetter() && shape->hasSlot()) {
                /* Fast path for, e.g., plain Object instance properties. */
                JSObject::nativeSetSlotWithType(cx, obj, shape, rval);
            } else {
                RootedValue rref(cx, rval);
                bool strict = cx->stack.currentScript()->strict;
                if (!js_NativeSet(cx, obj, obj, shape, false, strict, rref.address()))
                    return false;
            }
            return true;
        }

        GET_NAME_FROM_BYTECODE(cx->stack.currentScript(), pc, 0, name);
    }

    bool strict = cx->stack.currentScript()->strict;
    RootedValue rref(cx, rval);

    RootedId id(cx, NameToId(name));
    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        if (!baseops::SetPropertyHelper(cx, obj, obj, id, DNP_CACHE_RESULT, &rref, strict))
            return false;
    } else {
        if (!JSObject::setGeneric(cx, obj, obj, id, &rref, strict))
            return false;
    }

    return true;
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
        if (js_AtomToPrintableString(cx, name, &printable))
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
        if (normalized->getClass() == &WithClass && !shape->hasDefaultGetter())
            normalized = &normalized->asWith().object();
        if (!NativeGet(cx, normalized, obj2, shape, 0, vp))
            return false;
    }
    return true;
}

inline bool
IntrinsicNameOperation(JSContext *cx, JSScript *script, jsbytecode *pc, MutableHandleValue vp)
{
    JSOp op = JSOp(*pc);
    RootedPropertyName name(cx, GetNameFromBytecode(cx, script, pc, op));
    return cx->global()->getIntrinsicValue(cx, name, vp);
}

inline bool
NameOperation(JSContext *cx, jsbytecode *pc, MutableHandleValue vp)
{
    RootedObject obj(cx, cx->stack.currentScriptedScopeChain());
    RootedPropertyName name(cx, cx->stack.currentScript()->getName(pc));

    /*
     * Skip along the scope chain to the enclosing global object. This is
     * used for GNAME opcodes where the bytecode emitter has determined a
     * name access must be on the global. It also insulates us from bugs
     * in the emitter: type inference will assume that GNAME opcodes are
     * accessing the global object, and the inferred behavior should match
     * the actual behavior even if the id could be found on the scope chain
     * before the global object.
     */
    if (IsGlobalOp(JSOp(*pc)))
        obj = &obj->global();

    RootedShape shape(cx);
    RootedObject scope(cx), pobj(cx);
    if (!LookupName(cx, name, obj, &scope, &pobj, &shape))
        return false;

    /* Kludge to allow (typeof foo == "undefined") tests. */
    JSOp op2 = JSOp(pc[JSOP_NAME_LENGTH]);
    if (op2 == JSOP_TYPEOF)
        return FetchName<true>(cx, scope, pobj, name, shape, vp);
    return FetchName<false>(cx, scope, pobj, name, shape, vp);
}

inline bool
SetNameOperation(JSContext *cx, JSScript *script, jsbytecode *pc, HandleObject scope,
                 HandleValue val)
{
    JS_ASSERT(*pc == JSOP_SETNAME || *pc == JSOP_SETGNAME);
    JS_ASSERT_IF(*pc == JSOP_SETGNAME, scope == cx->global());

    bool strict = script->strict;
    RootedPropertyName name(cx, script->getName(pc));
    RootedValue valCopy(cx, val);

    /*
     * In strict-mode, we need to trigger an error when trying to assign to an
     * undeclared global variable. To do this, we call SetPropertyHelper
     * directly and pass DNP_UNQUALIFIED.
     */
    if (scope->isGlobal()) {
        JS_ASSERT(!scope->getOps()->setProperty);
        RootedId id(cx, NameToId(name));
        return baseops::SetPropertyHelper(cx, scope, scope, id, DNP_UNQUALIFIED, &valCopy, strict);
    }

    return JSObject::setProperty(cx, scope, scope, name, &valCopy, strict);
}

inline bool
DefVarOrConstOperation(JSContext *cx, HandleObject varobj, HandlePropertyName dn, unsigned attrs)
{
    JS_ASSERT(varobj->isVarObj());
    JS_ASSERT(!varobj->getOps()->defineProperty || varobj->isDebugScope());

    RootedShape prop(cx);
    RootedObject obj2(cx);
    if (!JSObject::lookupProperty(cx, varobj, dn, &obj2, &prop))
        return false;

    /* Steps 8c, 8d. */
    if (!prop || (obj2 != varobj && varobj->isGlobal())) {
        RootedValue value(cx, UndefinedValue());
        if (!JSObject::defineProperty(cx, varobj, dn, value, JS_PropertyStub,
                                      JS_StrictPropertyStub, attrs)) {
            return false;
        }
    } else {
        /*
         * Extension: ordinarily we'd be done here -- but for |const|.  If we
         * see a redeclaration that's |const|, we consider it a conflict.
         */
        unsigned oldAttrs;
        if (!JSObject::getPropertyAttributes(cx, varobj, dn, &oldAttrs))
            return false;
        if (attrs & JSPROP_READONLY) {
            JSAutoByteString bytes;
            if (js_AtomToPrintableString(cx, dn, &bytes)) {
                JS_ALWAYS_FALSE(JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR,
                                                             js_GetErrorMessage,
                                                             NULL, JSMSG_REDECLARED_VAR,
                                                             (oldAttrs & JSPROP_READONLY)
                                                             ? "const"
                                                             : "var",
                                                             bytes.ptr()));
            }
            return false;
        }
    }

    return true;
}

inline void
InterpreterFrames::enableInterruptsIfRunning(JSScript *script)
{
    if (regs->fp()->script() == script)
        enabler.enable();
}

static JS_ALWAYS_INLINE bool
AddOperation(JSContext *cx, HandleScript script, jsbytecode *pc, const Value &lhs, const Value &rhs,
             Value *res)
{
    if (lhs.isInt32() && rhs.isInt32()) {
        int32_t l = lhs.toInt32(), r = rhs.toInt32();
        int32_t sum = l + r;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000))) {
            res->setDouble(double(l) + double(r));
            types::TypeScript::MonitorOverflow(cx, script, pc);
        } else {
            res->setInt32(sum);
        }
    } else
#if JS_HAS_XML_SUPPORT
    if (IsXML(lhs) && IsXML(rhs)) {
        if (!js_ConcatenateXML(cx, &lhs.toObject(), &rhs.toObject(), res))
            return false;
        types::TypeScript::MonitorUnknown(cx, script, pc);
    } else
#endif
    {
        RootedValue lval(cx, lhs);
        RootedValue rval(cx, rhs);

        /*
         * If either operand is an object, any non-integer result must be
         * reported to inference.
         */
        bool lIsObject = lval.isObject(), rIsObject = rval.isObject();

        if (!ToPrimitive(cx, lval.address()))
            return false;
        if (!ToPrimitive(cx, rval.address()))
            return false;
        bool lIsString, rIsString;
        if ((lIsString = lval.isString()) | (rIsString = rval.isString())) {
            RootedString lstr(cx), rstr(cx);
            if (lIsString) {
                lstr = lval.toString();
            } else {
                lstr = ToString(cx, lval);
                if (!lstr)
                    return false;
            }
            if (rIsString) {
                rstr = rval.toString();
            } else {
                rstr = ToString(cx, rval);
                if (!rstr)
                    return false;
            }
            JSString *str = js_ConcatStrings(cx, lstr, rstr);
            if (!str)
                return false;
            if (lIsObject || rIsObject)
                types::TypeScript::MonitorString(cx, script, pc);
            res->setString(str);
        } else {
            double l, r;
            if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))
                return false;
            l += r;
            if (!res->setNumber(l) &&
                (lIsObject || rIsObject || (!lval.isDouble() && !rval.isDouble()))) {
                types::TypeScript::MonitorOverflow(cx, script, pc);
            }
        }
    }
    return true;
}

static JS_ALWAYS_INLINE bool
SubOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 - d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
MulOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 * d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
DivOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res->setNumber(NumberDiv(d1, d2));

    if (d2 == 0 || (res->isDouble() && !(lhs.isDouble() || rhs.isDouble())))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
ModOperation(JSContext *cx, HandleScript script, jsbytecode *pc, HandleValue lhs, HandleValue rhs,
             Value *res)
{
    int32_t l, r;
    if (lhs.isInt32() && rhs.isInt32() &&
        (l = lhs.toInt32()) >= 0 && (r = rhs.toInt32()) > 0) {
        int32_t mod = l % r;
        res->setInt32(mod);
        return true;
    }

    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;

    res->setNumber(NumberMod(d1, d2));
    types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
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
        i = -i;
        res.setInt32(i);
    } else {
        double d;
        if (!ToNumber(cx, val, &d))
            return false;
        d = -d;
        if (!res.setNumber(d) && !val.isDouble())
            types::TypeScript::MonitorOverflow(cx, script, pc);
    }

    return true;
}

static inline bool
FetchElementId(JSContext *cx, JSObject *obj, const Value &idval, jsid *idp, MutableHandleValue vp)
{
    int32_t i_;
    if (ValueFitsInInt32(idval, &i_) && INT_FITS_IN_JSID(i_)) {
        *idp = INT_TO_JSID(i_);
        return true;
    }
    return !!InternNonIntElementId(cx, obj, idval, idp, vp);
}

static JS_ALWAYS_INLINE bool
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

    jsid dummy;
    if (!InternNonIntElementId(cx, obj, idval, &dummy, res))
        return false;

    if (!res.isInt32())
        types::TypeScript::MonitorUnknown(cx, script, pc);
    return true;
}

static JS_ALWAYS_INLINE bool
GetObjectElementOperation(JSContext *cx, JSOp op, HandleObject obj, const Value &rref, MutableHandleValue res)
{
#if JS_HAS_XML_SUPPORT
    if (op == JSOP_CALLELEM && JS_UNLIKELY(obj->isXML())) {
        jsid id;
        if (!FetchElementId(cx, obj, rref, &id, res))
            return false;
        return js_GetXMLMethod(cx, obj, id, res);
    }
#endif
    // Don't call GetPcScript (needed for analysis) from inside Ion since it's expensive.
    bool analyze = !cx->fp()->beginsIonActivation();

    uint32_t index;
    if (IsDefinitelyIndex(rref, &index)) {
        if (analyze && !obj->isNative() && !obj->isArray()) {
            RootedScript script(cx, NULL);
            jsbytecode *pc = NULL;
            types::TypeScript::GetPcScript(cx, &script, &pc);

            if (script->hasAnalysis())
                script->analysis()->getCode(pc).nonNativeGetElement = true;
        }

        do {
            if (obj->isDenseArray()) {
                if (index < obj->getDenseArrayInitializedLength()) {
                    res.set(obj->getDenseArrayElement(index));
                    if (!res.isMagic())
                        break;
                }
            } else if (obj->isArguments()) {
                if (obj->asArguments().maybeGetElement(index, res))
                    break;
            }
            if (!JSObject::getElement(cx, obj, obj, index, res))
                return false;
        } while(0);
    } else {
        if (analyze) {
            RootedScript script(cx, NULL);
            jsbytecode *pc = NULL;
            types::TypeScript::GetPcScript(cx, &script, &pc);

            if (script->hasAnalysis()) {
                script->analysis()->getCode(pc).getStringElement = true;

                if (!obj->isArray() && !obj->isNative())
                    script->analysis()->getCode(pc).nonNativeGetElement = true;
            }
        }

        SpecialId special;
        res.set(rref);
        if (ValueIsSpecial(obj, res, &special, cx)) {
            if (!JSObject::getSpecial(cx, obj, obj, special, res))
                return false;
        } else {
            JSAtom *name = ToAtom(cx, res);
            if (!name)
                return false;

            if (name->isIndex(&index)) {
                if (!JSObject::getElement(cx, obj, obj, index, res))
                    return false;
            } else {
                if (!JSObject::getProperty(cx, obj, obj, name->asPropertyName(), res))
                    return false;
            }
        }
    }

    assertSameCompartmentDebugOnly(cx, res);
    return true;
}

static JS_ALWAYS_INLINE bool
GetElementOperation(JSContext *cx, JSOp op, HandleValue lref, HandleValue rref,
                    MutableHandleValue res)
{
    AssertCanGC();
    JS_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    uint32_t index;
    if (lref.isString() && IsDefinitelyIndex(rref, &index)) {
        JSString *str = lref.toString();
        if (index < str->length()) {
            str = cx->runtime->staticStrings.getUnitStringForElement(cx, str, index);
            if (!str)
                return false;
            res.setString(str);
            return true;
        }
    }

    StackFrame *fp = cx->fp();
    RootedValue lval(cx, lref);
    if (IsOptimizedArguments(fp, lval.address())) {
        if (rref.isInt32()) {
            int32_t i = rref.toInt32();
            if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
                res.set(fp->unaliasedActual(i));
                return true;
            }
        }

        RootedScript script(cx, fp->script());
        if (!JSScript::argumentsOptimizationFailed(cx, script))
            return false;

        lval = ObjectValue(fp->argsObj());
    }

    bool isObject = lval.isObject();
    RootedObject obj(cx, ToObjectFromStack(cx, lval));
    if (!obj)
        return false;
    if (!GetObjectElementOperation(cx, op, obj, rref, res))
        return false;

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLELEM && JS_UNLIKELY(res.isPrimitive()) && isObject) {
        if (!OnUnknownMethod(cx, obj, rref, res))
            return false;
    }
#endif
    return true;
}

static JS_ALWAYS_INLINE bool
SetObjectElementOperation(JSContext *cx, Handle<JSObject*> obj, HandleId id, const Value &value, bool strict)
{
    types::TypeScript::MonitorAssign(cx, obj, id);

    do {
        if (obj->isDenseArray() && JSID_IS_INT(id)) {
            uint32_t length = obj->getDenseArrayInitializedLength();
            int32_t i = JSID_TO_INT(id);
            if ((uint32_t)i < length) {
                if (obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
                    if (js_PrototypeHasIndexedProperties(obj))
                        break;
                    if ((uint32_t)i >= obj->getArrayLength())
                        JSObject::setArrayLength(cx, obj, i + 1);
                }
                JSObject::setDenseArrayElementWithType(cx, obj, i, value);
                return true;
            } else {
                if (!cx->fp()->beginsIonActivation()) {
                    RootedScript script(cx);
                    jsbytecode *pc;
                    types::TypeScript::GetPcScript(cx, &script, &pc);

                    if (script->hasAnalysis())
                        script->analysis()->getCode(pc).arrayWriteHole = true;
                }
            }
        }
    } while (0);

    RootedValue tmp(cx, value);
    return JSObject::setGeneric(cx, obj, obj, id, &tmp, strict);
}

static JS_ALWAYS_INLINE JSString *
TypeOfOperation(JSContext *cx, HandleValue v)
{
    JSType type = JS_TypeOfValue(cx, v);
    return TypeName(type, cx);
}

static JS_ALWAYS_INLINE bool
InitElemOperation(JSContext *cx, HandleObject obj, MutableHandleValue idval, HandleValue val)
{
    JS_ASSERT(!obj->isDenseArray());
    JS_ASSERT(!val.isMagic(JS_ARRAY_HOLE));

    RootedId id(cx);
    if (!FetchElementId(cx, obj, idval, id.address(), idval))
        return false;

    return JSObject::defineGeneric(cx, obj, id, val, NULL, NULL, JSPROP_ENUMERATE);
}

static JS_ALWAYS_INLINE bool
InitArrayElemOperation(JSContext *cx, jsbytecode *pc, HandleObject obj, uint32_t index, HandleValue val)
{
    JSOp op = JSOp(*pc);
    JS_ASSERT(op == JSOP_INITELEM_ARRAY || op == JSOP_INITELEM_INC);

    JS_ASSERT(obj->isArray());

    /*
     * If val is a hole, do not call JSObject::defineElement. In this case,
     * if the current op is the last element initialiser, set the array length
     * to one greater than id.
     */
    if (val.isMagic(JS_ARRAY_HOLE)) {
        JSOp next = JSOp(*GetNextPc(pc));

        if ((op == JSOP_INITELEM_ARRAY && next == JSOP_ENDINIT) ||
            (op == JSOP_INITELEM_INC && next == JSOP_POP))
        {
            if (!SetLengthProperty(cx, obj, index + 1))
                return false;
        }
    } else {
        if (!JSObject::defineElement(cx, obj, index, val, NULL, NULL, JSPROP_ENUMERATE))
            return false;
    }

    if (op == JSOP_INITELEM_INC && index == INT32_MAX) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_SPREAD_TOO_LARGE);
        return false;
    }

    return true;
}

#define RELATIONAL_OP(OP)                                                     \
    JS_BEGIN_MACRO                                                            \
        RootedValue lvalRoot(cx, lhs), rvalRoot(cx, rhs);                     \
        Value &lval = lvalRoot.get();                                         \
        Value &rval = rvalRoot.get();                                         \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if (lval.isInt32() && rval.isInt32()) {                               \
            *res = lval.toInt32() OP rval.toInt32();                          \
        } else {                                                              \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, &lval))                       \
                return false;                                                 \
            if (!ToPrimitive(cx, JSTYPE_NUMBER, &rval))                       \
                return false;                                                 \
            if (lval.isString() && rval.isString()) {                         \
                JSString *l = lval.toString(), *r = rval.toString();          \
                int32_t result;                                               \
                if (!CompareStrings(cx, l, r, &result))                       \
                    return false;                                             \
                *res = result OP 0;                                           \
            } else {                                                          \
                double l, r;                                                  \
                if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))       \
                    return false;;                                            \
                *res = (l OP r);                                              \
            }                                                                 \
        }                                                                     \
        return true;                                                          \
    JS_END_MACRO

static JS_ALWAYS_INLINE bool
LessThanOperation(JSContext *cx, const Value &lhs, const Value &rhs, bool *res) {
    RELATIONAL_OP(<);
}

static JS_ALWAYS_INLINE bool
LessThanOrEqualOperation(JSContext *cx, const Value &lhs, const Value &rhs, bool *res) {
    RELATIONAL_OP(<=);
}

static JS_ALWAYS_INLINE bool
GreaterThanOperation(JSContext *cx, const Value &lhs, const Value &rhs, bool *res) {
    RELATIONAL_OP(>);
}

static JS_ALWAYS_INLINE bool
GreaterThanOrEqualOperation(JSContext *cx, const Value &lhs, const Value &rhs, bool *res) {
    RELATIONAL_OP(>=);
}

static JS_ALWAYS_INLINE bool
BitNot(JSContext *cx, HandleValue in, int *out)
{
    int i;
    if (!ToInt32(cx, in, &i))
        return false;
    *out = ~i;
    return true;
}

static JS_ALWAYS_INLINE bool
BitXor(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left ^ right;
    return true;
}

static JS_ALWAYS_INLINE bool
BitOr(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left | right;
    return true;
}

static JS_ALWAYS_INLINE bool
BitAnd(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left & right;
    return true;
}

static JS_ALWAYS_INLINE bool
BitLsh(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int32_t left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left << (right & 31);
    return true;
}

static JS_ALWAYS_INLINE bool
BitRsh(JSContext *cx, HandleValue lhs, HandleValue rhs, int *out)
{
    int32_t left, right;
    if (!ToInt32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    *out = left >> (right & 31);
    return true;
}

static JS_ALWAYS_INLINE bool
UrshOperation(JSContext *cx, HandleScript script, jsbytecode *pc,
              HandleValue lhs, HandleValue rhs, Value *out)
{
    uint32_t left;
    int32_t  right;
    if (!ToUint32(cx, lhs, &left) || !ToInt32(cx, rhs, &right))
        return false;
    left >>= right & 31;
    if (!out->setNumber(uint32_t(left)))
        types::TypeScript::MonitorOverflow(cx, script, pc);
    return true;
}

#undef RELATIONAL_OP

inline JSFunction *
ReportIfNotFunction(JSContext *cx, const Value &v, MaybeConstruct construct = NO_CONSTRUCT)
{
    if (v.isObject() && v.toObject().isFunction())
        return v.toObject().toFunction();

    ReportIsNotFunction(cx, v, construct);
    return NULL;
}

/*
 * FastInvokeGuard is used to optimize calls to JS functions from natives written
 * in C++, for instance Array.map. If the callee is not Ion-compiled, this will
 * just call Invoke. If the callee has a valid IonScript, however, it will enter
 * Ion directly.
 */
class FastInvokeGuard
{
    InvokeArgsGuard args_;
    RootedFunction fun_;
    RootedScript script_;
#ifdef JS_ION
    ion::IonContext ictx_;
    bool useIon_;
#endif

  public:
    FastInvokeGuard(JSContext *cx, const Value &fval)
      : fun_(cx)
      , script_(cx)
#ifdef JS_ION
      , ictx_(cx, cx->compartment, NULL)
      , useIon_(ion::IsEnabled(cx))
#endif
    {
        initFunction(fval);
    }

    void initFunction(const Value &fval) {
        if (fval.isObject() && fval.toObject().isFunction()) {
            JSFunction *fun = fval.toObject().toFunction();
            if (fun->hasScript()) {
                fun_ = fun;
                script_ = fun->nonLazyScript();
            }
        }
    }

    InvokeArgsGuard &args() {
        return args_;
    }

    bool invoke(JSContext *cx) {
#ifdef JS_ION
        if (useIon_ && fun_) {
            JS_ASSERT(fun_->nonLazyScript() == script_);

            ion::MethodStatus status = ion::CanEnterUsingFastInvoke(cx, script_, args_.length());
            if (status == ion::Method_Error)
                return false;
            if (status == ion::Method_Compiled) {
                ion::IonExecStatus result = ion::FastInvoke(cx, fun_, args_);
                if (IsErrorStatus(result))
                    return false;

                JS_ASSERT(result == ion::IonExec_Ok);
                return true;
            }

            JS_ASSERT(status == ion::Method_Skipped);

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

#endif /* jsinterpinlines_h__ */
