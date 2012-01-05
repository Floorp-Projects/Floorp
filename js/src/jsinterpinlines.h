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
#include "jspropertycacheinlines.h"
#include "jstypedarrayinlines.h"

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
NativeGet(JSContext *cx, JSObject *obj, JSObject *pobj, const Shape *shape, uintN getHow, Value *vp)
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

#if defined(DEBUG) && !defined(JS_THREADSAFE)
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
GetPropertyGenericMaybeCallXML(JSContext *cx, JSOp op, JSObject *obj, jsid id, Value *vp)
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
GetPropertyOperation(JSContext *cx, jsbytecode *pc, const Value &lval, Value *vp)
{
    JS_ASSERT(vp != &lval);

    JSOp op = JSOp(*pc);

    if (op == JSOP_LENGTH) {
        /* Optimize length accesses on strings, arrays, and arguments. */
        if (lval.isString()) {
            *vp = Int32Value(lval.toString()->length());
            return true;
        }
        if (lval.isMagic(JS_LAZY_ARGUMENTS)) {
            *vp = Int32Value(cx->fp()->numActualArgs());
            return true;
        }
        if (lval.isObject()) {
            JSObject *obj = &lval.toObject();
            if (obj->isArray()) {
                jsuint length = obj->getArrayLength();
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

            if (js_IsTypedArray(obj)) {
                JSObject *tarray = TypedArray::getTypedArray(obj);
                *vp = Int32Value(TypedArray::getLength(tarray));
                return true;
            }
        }
    }

    JSObject *obj = ValueToObjectOrPrototype(cx, lval);
    if (!obj)
        return false;

    uintN flags = (op == JSOP_CALLPROP)
                  ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                  : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    PropertyName *name;
    JS_PROPERTY_CACHE(cx).test(cx, pc, obj, obj2, entry, name);
    if (!name) {
        AssertValidPropertyCacheHit(cx, obj, obj2, entry);
        if (!NativeGet(cx, obj, obj2, entry->prop, flags, vp))
            return false;
        return true;
    }

    jsid id = ATOM_TO_JSID(name);

    if (obj->getOps()->getProperty) {
        if (!GetPropertyGenericMaybeCallXML(cx, op, obj, id, vp))
            return false;
    } else {
        if (!GetPropertyHelper(cx, obj, id, flags, vp))
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
    JSObject *obj = ValueToObject(cx, lval);
    if (!obj)
        return false;

    JS_ASSERT_IF(*pc == JSOP_SETMETHOD, IsFunctionObject(rval));
    JS_ASSERT_IF(*pc == JSOP_SETNAME || *pc == JSOP_SETGNAME, lval.isObject());
    JS_ASSERT_IF(*pc == JSOP_SETGNAME, obj == &cx->fp()->scopeChain().global());

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
        const Shape *shape = entry->prop;
        JS_ASSERT_IF(shape->isDataDescriptor(), shape->writable());
        JS_ASSERT_IF(shape->hasSlot(), entry->isOwnPropertyHit());

        if (entry->isOwnPropertyHit() ||
            ((obj2 = obj->getProto()) && obj2->lastProperty() == entry->pshape)) {
#ifdef DEBUG
            if (entry->isOwnPropertyHit()) {
                JS_ASSERT(obj->nativeContains(cx, *shape));
            } else {
                JS_ASSERT(obj2->nativeContains(cx, *shape));
                JS_ASSERT(entry->isPrototypePropertyHit());
                JS_ASSERT(entry->kshape != entry->pshape);
                JS_ASSERT(!shape->hasSlot());
            }
#endif

            if (shape->hasDefaultSetter() && shape->hasSlot() && !shape->isMethod()) {
                /* Fast path for, e.g., plain Object instance properties. */
                obj->nativeSetSlotWithType(cx, shape, rval);
            } else {
                Value rref = rval;
                bool strict = cx->stack.currentScript()->strictModeCode;
                if (!js_NativeSet(cx, obj, shape, false, strict, &rref))
                    return false;
            }
            return true;
        }

        GET_NAME_FROM_BYTECODE(cx->stack.currentScript(), pc, 0, name);
    }

    bool strict = cx->stack.currentScript()->strictModeCode;
    Value rref = rval;

    JSOp op = JSOp(*pc);

    jsid id = ATOM_TO_JSID(name);
    if (JS_LIKELY(!obj->getOps()->setProperty)) {
        uintN defineHow;
        if (op == JSOP_SETMETHOD)
            defineHow = DNP_CACHE_RESULT | DNP_SET_METHOD;
        else if (op == JSOP_SETNAME)
            defineHow = DNP_CACHE_RESULT | DNP_UNQUALIFIED;
        else
            defineHow = DNP_CACHE_RESULT;
        if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rref, strict))
            return false;
    } else {
        if (!obj->setGeneric(cx, id, &rref, strict))
            return false;
    }

    return true;
}

inline bool
NameOperation(JSContext *cx, jsbytecode *pc, Value *vp)
{
    JSObject *obj = cx->stack.currentScriptedScopeChain();

    bool global = js_CodeSpec[*pc].format & JOF_GNAME;
    if (global)
        obj = &obj->global();

    PropertyCacheEntry *entry;
    JSObject *obj2;
    PropertyName *name;
    JS_PROPERTY_CACHE(cx).test(cx, pc, obj, obj2, entry, name);
    if (!name) {
        AssertValidPropertyCacheHit(cx, obj, obj2, entry);
        if (!NativeGet(cx, obj, obj2, entry->prop, JSGET_METHOD_BARRIER, vp))
            return false;
        return true;
    }

    jsid id = ATOM_TO_JSID(name);

    JSProperty *prop;
    if (!FindPropertyHelper(cx, name, true, global, &obj, &obj2, &prop))
        return false;
    if (!prop) {
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

    /* Take the slow path if prop was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        if (!obj->getGeneric(cx, id, vp))
            return false;
    } else {
        Shape *shape = (Shape *)prop;
        JSObject *normalized = obj;
        if (normalized->getClass() == &WithClass && !shape->hasDefaultGetter())
            normalized = &normalized->asWith().object();
        if (!NativeGet(cx, normalized, obj2, shape, JSGET_METHOD_BARRIER, vp))
            return false;
    }

    return true;
}

inline bool
FunctionNeedsPrologue(JSContext *cx, JSFunction *fun)
{
    /* Heavyweight functions need call objects created. */
    if (fun->isHeavyweight())
        return true;

    /* Outer and inner functions need to preserve nesting invariants. */
    if (cx->typeInferenceEnabled() && fun->script()->nesting())
        return true;

    return false;
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

    return true;
}

inline bool
ScriptEpilogue(JSContext *cx, StackFrame *fp, bool ok)
{
    Probes::exitJSFun(cx, fp->maybeFun(), fp->script());

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
    return true;
}

inline bool
ScriptEpilogueOrGeneratorYield(JSContext *cx, StackFrame *fp, bool ok)
{
    if (!fp->isYielding())
        return ScriptEpilogue(cx, fp, ok);
    return ok;
}

inline void
InterpreterFrames::enableInterruptsIfRunning(JSScript *script)
{
    if (script == regs->fp()->script())
        enabler.enableInterrupts();
}

}  /* namespace js */

#endif /* jsinterpinlines_h__ */
