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

#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jspropertycacheinlines.h"
#include "jstypedarrayinlines.h"

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
ComputeImplicitThis(JSContext *cx, JSObject *obj, Value *vp)
{
    vp->setUndefined();

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
    FrameRegs &regs = cx->regs();
    if (IsOptimizedArguments(regs.fp(), &regs.sp[-1])) {
        CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
        if (!IsNativeFunction(args.calleev(), js_fun_apply)) {
            if (!JSScript::argumentsOptimizationFailed(cx, regs.fp()->script()))
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
ValuePropertyBearer(JSContext *cx, StackFrame *fp, const Value &v, int spindex)
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
    js_ReportIsNullOrUndefined(cx, spindex, v, NULL);
    return NULL;
}

inline bool
NativeGet(JSContext *cx, Handle<JSObject*> obj, Handle<JSObject*> pobj, Shape *shape,
          unsigned getHow, Value *vp)
{
    if (shape->isDataDescriptor() && shape->hasDefaultGetter()) {
        /* Fast path for Object instance properties. */
        JS_ASSERT(shape->hasSlot());
        *vp = pobj->nativeGetSlot(shape->slot());
    } else {
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
GetPropertyGenericMaybeCallXML(JSContext *cx, JSOp op, HandleObject obj, HandleId id, Value *vp)
{
    /*
     * Various XML properties behave differently when accessed in a
     * call vs. normal context, and getGeneric will not work right.
     */
#if JS_HAS_XML_SUPPORT
    if (op == JSOP_CALLPROP && obj->isXML())
        return js_GetXMLMethod(cx, obj, id, vp);
#endif

    return obj->getGeneric(cx, id, vp);
}

inline bool
GetPropertyOperation(JSContext *cx, JSScript *script, jsbytecode *pc, Value &lval, Value *vp)
{
    JS_ASSERT(vp != &lval);

    JSOp op = JSOp(*pc);

    if (op == JSOP_LENGTH) {
        /* Optimize length accesses on strings, arrays, and arguments. */
        if (lval.isString()) {
            *vp = Int32Value(lval.toString()->length());
            return true;
        }
        if (IsOptimizedArguments(cx->fp(), &lval)) {
            *vp = Int32Value(cx->fp()->numActualArgs());
            return true;
        }
        if (lval.isObject()) {
            JSObject *obj = &lval.toObject();
            if (obj->isArray()) {
                uint32_t length = obj->getArrayLength();
                *vp = NumberValue(length);
                return true;
            }

            if (obj->isArguments()) {
                ArgumentsObject *argsobj = &obj->asArguments();
                if (!argsobj->hasOverriddenLength()) {
                    uint32_t length = argsobj->initialLength();
                    JS_ASSERT(length < INT32_MAX);
                    *vp = Int32Value(int32_t(length));
                    return true;
                }
            }

            if (obj->isTypedArray()) {
                *vp = Int32Value(TypedArray::length(obj));
                return true;
            }
        }
    }

    RootedObject obj(cx, ValueToObject(cx, lval));
    if (!obj)
        return false;

    PropertyCacheEntry *entry;
    Rooted<JSObject*> obj2(cx);
    PropertyName *name;
    JS_PROPERTY_CACHE(cx).test(cx, pc, obj.get(), obj2.get(), entry, name);
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
        JS_UNLIKELY(vp->isPrimitive()) &&
        lval.isObject())
    {
        if (!OnUnknownMethod(cx, obj, IdToValue(id), vp))
            return false;
    }
#endif

    return true;
}

inline bool
SetPropertyOperation(JSContext *cx, jsbytecode *pc, const Value &lval, const Value &rval)
{
    RootedObject obj(cx, ValueToObject(cx, lval));
    if (!obj)
        return false;

    JS_ASSERT_IF(*pc == JSOP_SETNAME || *pc == JSOP_SETGNAME, lval.isObject());
    JS_ASSERT_IF(*pc == JSOP_SETGNAME, obj == &cx->fp()->global());

    PropertyCacheEntry *entry;
    JSObject *obj2;
    PropertyName *name;
    if (JS_PROPERTY_CACHE(cx).testForSet(cx, pc, obj, &entry, &obj2, &name)) {
        /*
         * Property cache hit, only partially confirmed by testForSet. We
         * know that the entry applies to regs.pc and that obj's shape
         * matches.
         *
         * The entry predicts a set either an existing "own" property, or
         * on a prototype property that has a setter.
         */
        Shape *shape = entry->prop;
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
                obj->nativeSetSlotWithType(cx, shape, rval);
            } else {
                RootedValue rref(cx, rval);
                bool strict = cx->stack.currentScript()->strictModeCode;
                if (!js_NativeSet(cx, obj, obj, shape, false, strict, rref.address()))
                    return false;
            }
            return true;
        }

        GET_NAME_FROM_BYTECODE(cx->stack.currentScript(), pc, 0, name);
    }

    bool strict = cx->stack.currentScript()->strictModeCode;
    RootedValue rref(cx, rval);

    JSOp op = JSOp(*pc);

    RootedId id(cx, NameToId(name));
    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        unsigned defineHow = (op == JSOP_SETNAME)
                             ? DNP_CACHE_RESULT | DNP_UNQUALIFIED
                             : DNP_CACHE_RESULT;
        if (!baseops::SetPropertyHelper(cx, obj, obj, id, defineHow, rref.address(), strict))
            return false;
    } else {
        if (!obj->setGeneric(cx, obj, id, rref.address(), strict))
            return false;
    }

    return true;
}

inline bool
NameOperation(JSContext *cx, JSScript *script, jsbytecode *pc, Value *vp)
{
    RootedObject obj(cx, cx->stack.currentScriptedScopeChain());

    /*
     * Skip along the scope chain to the enclosing global object. This is
     * used for GNAME opcodes where the bytecode emitter has determined a
     * name access must be on the global. It also insulates us from bugs
     * in the emitter: type inference will assume that GNAME opcodes are
     * accessing the global object, and the inferred behavior should match
     * the actual behavior even if the id could be found on the scope chain
     * before the global object.
     */
    if (js_CodeSpec[*pc].format & JOF_GNAME)
        obj = &obj->global();

    PropertyCacheEntry *entry;
    Rooted<JSObject*> obj2(cx);
    RootedPropertyName name(cx);
    JS_PROPERTY_CACHE(cx).test(cx, pc, obj.get(), obj2.get(), entry, name.get());
    if (!name) {
        AssertValidPropertyCacheHit(cx, obj, obj2, entry);
        if (!NativeGet(cx, obj, obj2, entry->prop, 0, vp))
            return false;
        return true;
    }

    RootedShape shape(cx);
    if (!FindPropertyHelper(cx, name, true, obj, &obj, &obj2, &shape))
        return false;
    if (!shape) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        JSOp op2 = JSOp(pc[JSOP_NAME_LENGTH]);
        if (op2 == JSOP_TYPEOF) {
            vp->setUndefined();
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
        if (!obj->getGeneric(cx, id, vp))
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
DefVarOrConstOperation(JSContext *cx, HandleObject varobj, HandlePropertyName dn, unsigned attrs)
{
    JS_ASSERT(varobj->isVarObj());
    JS_ASSERT(!varobj->getOps()->defineProperty || varobj->isDebugScope());

    RootedShape prop(cx);
    RootedObject obj2(cx);
    if (!varobj->lookupProperty(cx, dn, &obj2, &prop))
        return false;

    /* Steps 8c, 8d. */
    if (!prop || (obj2 != varobj && varobj->isGlobal())) {
        if (!varobj->defineProperty(cx, dn, UndefinedValue(), JS_PropertyStub,
                                    JS_StrictPropertyStub, attrs)) {
            return false;
        }
    } else {
        /*
         * Extension: ordinarily we'd be done here -- but for |const|.  If we
         * see a redeclaration that's |const|, we consider it a conflict.
         */
        unsigned oldAttrs;
        if (!varobj->getPropertyAttributes(cx, dn, &oldAttrs))
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
    if (script == regs->fp()->script())
        enabler.enable();
}

static JS_ALWAYS_INLINE bool
AddOperation(JSContext *cx, const Value &lhs, const Value &rhs, Value *res)
{
    if (lhs.isInt32() && rhs.isInt32()) {
        int32_t l = lhs.toInt32(), r = rhs.toInt32();
        int32_t sum = l + r;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000))) {
            res->setDouble(double(l) + double(r));
            types::TypeScript::MonitorOverflow(cx);
        } else {
            res->setInt32(sum);
        }
    } else
#if JS_HAS_XML_SUPPORT
    if (IsXML(lhs) && IsXML(rhs)) {
        if (!js_ConcatenateXML(cx, &lhs.toObject(), &rhs.toObject(), res))
            return false;
        types::TypeScript::MonitorUnknown(cx);
    } else
#endif
    {
        RootedValue lval_(cx, lhs);
        RootedValue rval_(cx, rhs);
        Value &lval = lval_.get();
        Value &rval = rval_.get();

        /*
         * If either operand is an object, any non-integer result must be
         * reported to inference.
         */
        bool lIsObject = lval.isObject(), rIsObject = rval.isObject();

        if (!ToPrimitive(cx, &lval))
            return false;
        if (!ToPrimitive(cx, &rval))
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
                types::TypeScript::MonitorString(cx);
            res->setString(str);
        } else {
            double l, r;
            if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))
                return false;
            l += r;
            if (!res->setNumber(l) &&
                (lIsObject || rIsObject || (!lval.isDouble() && !rval.isDouble()))) {
                types::TypeScript::MonitorOverflow(cx);
            }
        }
    }
    return true;
}

static JS_ALWAYS_INLINE bool
SubOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 - d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx);
    return true;
}

static JS_ALWAYS_INLINE bool
MulOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    double d = d1 * d2;
    if (!res->setNumber(d) && !(lhs.isDouble() || rhs.isDouble()))
        types::TypeScript::MonitorOverflow(cx);
    return true;
}

static JS_ALWAYS_INLINE bool
DivOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, Value *res)
{
    double d1, d2;
    if (!ToNumber(cx, lhs, &d1) || !ToNumber(cx, rhs, &d2))
        return false;
    res->setNumber(NumberDiv(d1, d2));

    if (d2 == 0 || (res->isDouble() && !(lhs.isDouble() || rhs.isDouble())))
        types::TypeScript::MonitorOverflow(cx);
    return true;
}

static JS_ALWAYS_INLINE bool
ModOperation(JSContext *cx, HandleValue lhs, HandleValue rhs, Value *res)
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

    if (d2 == 0)
        res->setDouble(js_NaN);
    else
        res->setDouble(js_fmod(d1, d2));
    types::TypeScript::MonitorOverflow(cx);
    return true;
}

static inline bool
FetchElementId(JSContext *cx, JSObject *obj, const Value &idval, jsid *idp, Value *vp)
{
    int32_t i_;
    if (ValueFitsInInt32(idval, &i_) && INT_FITS_IN_JSID(i_)) {
        *idp = INT_TO_JSID(i_);
        return true;
    }
    return !!InternNonIntElementId(cx, obj, idval, idp, vp);
}

static JS_ALWAYS_INLINE bool
ToIdOperation(JSContext *cx, const Value &objval, const Value &idval, Value *res)
{
    if (idval.isInt32()) {
        *res = idval;
        return true;
    }

    JSObject *obj = ValueToObject(cx, objval);
    if (!obj)
        return false;

    jsid dummy;
    if (!InternNonIntElementId(cx, obj, idval, &dummy, res))
        return false;

    if (!res->isInt32())
        types::TypeScript::MonitorUnknown(cx);
    return true;
}

static JS_ALWAYS_INLINE bool
GetObjectElementOperation(JSContext *cx, JSOp op, HandleObject obj, const Value &rref, Value *res)
{
#if JS_HAS_XML_SUPPORT
    if (op == JSOP_CALLELEM && JS_UNLIKELY(obj->isXML())) {
        jsid id;
        if (!FetchElementId(cx, obj, rref, &id, res))
            return false;
        return js_GetXMLMethod(cx, obj, id, res);
    }
#endif

    uint32_t index;
    if (IsDefinitelyIndex(rref, &index)) {
        do {
            if (obj->isDenseArray()) {
                if (index < obj->getDenseArrayInitializedLength()) {
                    *res = obj->getDenseArrayElement(index);
                    if (!res->isMagic())
                        break;
                }
            } else if (obj->isArguments()) {
                if (obj->asArguments().maybeGetElement(index, res))
                    break;
            }
            if (!obj->getElement(cx, index, res))
                return false;
        } while(0);
    } else {
        JSScript *script;
        jsbytecode *pc;
        types::TypeScript::GetPcScript(cx, &script, &pc);

        if (script->hasAnalysis())
            script->analysis()->getCode(pc).getStringElement = true;

        SpecialId special;
        *res = rref;
        if (ValueIsSpecial(obj, res, &special, cx)) {
            if (!obj->getSpecial(cx, obj, special, res))
                return false;
        } else {
            JSAtom *name = ToAtom(cx, *res);
            if (!name)
                return false;

            if (name->isIndex(&index)) {
                if (!obj->getElement(cx, index, res))
                    return false;
            } else {
                if (!obj->getProperty(cx, name->asPropertyName(), res))
                    return false;
            }
        }
    }

    assertSameCompartment(cx, *res);
    return true;
}

static JS_ALWAYS_INLINE bool
GetElementOperation(JSContext *cx, JSOp op, Value &lref, const Value &rref, Value *res)
{
    JS_ASSERT(op == JSOP_GETELEM || op == JSOP_CALLELEM);

    if (lref.isString() && rref.isInt32()) {
        JSString *str = lref.toString();
        int32_t i = rref.toInt32();
        if (size_t(i) < str->length()) {
            str = cx->runtime->staticStrings.getUnitStringForElement(cx, str, size_t(i));
            if (!str)
                return false;
            res->setString(str);
            return true;
        }
    }

    StackFrame *fp = cx->fp();
    if (IsOptimizedArguments(fp, &lref)) {
        if (rref.isInt32()) {
            int32_t i = rref.toInt32();
            if (i >= 0 && uint32_t(i) < fp->numActualArgs()) {
                *res = fp->unaliasedActual(i);
                return true;
            }
        }

        if (!JSScript::argumentsOptimizationFailed(cx, fp->script()))
            return false;

        lref = ObjectValue(fp->argsObj());
    }

    bool isObject = lref.isObject();
    RootedObject obj(cx, ValueToObject(cx, lref));
    if (!obj)
        return false;
    if (!GetObjectElementOperation(cx, op, obj, rref, res))
        return false;

#if JS_HAS_NO_SUCH_METHOD
    if (op == JSOP_CALLELEM && JS_UNLIKELY(res->isPrimitive()) && isObject) {
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
                    if (js_PrototypeHasIndexedProperties(cx, obj))
                        break;
                    if ((uint32_t)i >= obj->getArrayLength())
                        obj->setArrayLength(cx, i + 1);
                }
                obj->setDenseArrayElementWithType(cx, i, value);
                return true;
            } else {
                JSScript *script;
                jsbytecode *pc;
                types::TypeScript::GetPcScript(cx, &script, &pc);

                if (script->hasAnalysis())
                    script->analysis()->getCode(pc).arrayWriteHole = true;
            }
        }
    } while (0);

    RootedValue tmp(cx, value);
    return obj->setGeneric(cx, obj, id, tmp.address(), strict);
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

#undef RELATIONAL_OP

}  /* namespace js */

#endif /* jsinterpinlines_h__ */
