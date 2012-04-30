/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Intel RiverTrail implementation.
 *
 * The Initial Developer of the Original Code is
 *   Intel Corporation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stephan Herhut <stephan.a.herhut@intel.com>
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

#include "jsapi.h"
#include "jsobj.h"
#include "jsprf.h"

#include "vm/MethodGuard-inl.h"
#include "jsobjinlines.h"
#include "jsarrayinlines.h"

using namespace js;

enum {
    JSSLOT_PA_LENGTH = 0,
    JSSLOT_PA_BUFFER,
    JSSLOT_PA_MAX
};

inline uint32_t
GetLength(JSObject *obj) {
    return uint32_t(obj->getSlot(JSSLOT_PA_LENGTH).toInt32());
}

inline JSObject *
GetBuffer(JSObject *obj) {
    return &(obj->getSlot(JSSLOT_PA_BUFFER).toObject());
}

static bool
NewParallelArray(JSContext *cx, JSObject *buffer, uint32_t length, JSObject **result) {
    *result = NewBuiltinClassInstance(cx, &ParallelArrayClass);
    if (!*result)
        return false;

    (*result)->setSlot(JSSLOT_PA_LENGTH, Int32Value(int32_t(length)));
    (*result)->setSlot(JSSLOT_PA_BUFFER, ObjectValue(*buffer));

    return true;
}

static JSBool
ParallelArray_get(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, ParallelArray_get, &ParallelArrayClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "ParallelArray.get", "0", "s");
        return false;
    }

    uint32_t index;
    if (!ToUint32(cx, args[0], &index))
        return false;

    uint32_t length = GetLength(obj);

    if (index >= length)
        return false;

    args.rval() = GetBuffer(obj)->getDenseArrayElement(index);

    return true;
}

static JSBool
ParallelArray_build(JSContext *cx, uint32_t length, const Value &thisv, JSObject *elementalFun,
                    bool passElement, unsigned extrasc, Value *extrasp, Value *vp)
{
    /* create data store for results */
    JSObject *buffer = NewDenseAllocatedArray(cx, length);
    if (!buffer)
        return false;

    buffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* grab source buffer if we need to pass elements */
    JSObject *srcBuffer;
    if (passElement)
        srcBuffer = &(thisv.toObject().getSlot(JSSLOT_PA_BUFFER).toObject());

    /* prepare call frame on stack */
    InvokeArgsGuard args;
    cx->stack.pushInvokeArgs(cx, extrasc + 1, &args);

    for (uint32_t i = 0; i < length; i++) {
        args.setCallee(ObjectValue(*elementalFun));
        if (passElement)
            args[0] = srcBuffer->getDenseArrayElement(i);
        else
            args[0].setNumber(i);

        /* set value of this */
        args.thisv() = thisv;

        /* set extra arguments */
        for (unsigned j = 0; j < extrasc; j++) {
            JSObject &extra = extrasp[j].toObject();

            if (!extra.getElement(cx, &extra, i, &(args[j+1])))
                return false;
        }

        /* call */
        if (!Invoke(cx, args))
            return false;

        /* set result element */
        buffer->setDenseArrayElementWithType(cx, i, args.rval());
    }

    /* create ParallelArray wrapper class */
    JSObject *result;
    if (!NewParallelArray(cx, buffer, length, &result))
        return false;

    vp->setObject(*result);
    return true;
}

static JSBool
ParallelArray_construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "ParallelArray", "0", "s");
        return false;
    }

    if (args.length() == 1) {
        /* first case: init using an array value */
        if (!args[0].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
            return false;
        }

        JSObject *src = &(args[0].toObject());

        uint32_t srcLen;
        if (!js_GetLengthProperty(cx, src, &srcLen))
            return false;

        /* allocate buffer for result */
        JSObject *buffer = NewDenseAllocatedArray(cx, srcLen);
        if (!buffer)
            return false;

        buffer->ensureDenseArrayInitializedLength(cx, srcLen, 0);

        for (uint32_t i = 0; i < srcLen; i++) {
            Value elem;
            if (src->isDenseArray() && (i < src->getDenseArrayInitializedLength())) {
                /* dense array case */
                elem = src->getDenseArrayElement(i);
                if (elem.isMagic(JS_ARRAY_HOLE))
                    elem.setUndefined();
            } else {
                /* generic case */
                if (!src->getElement(cx, src, i, &elem))
                    return false;
            }

            buffer->setDenseArrayElementWithType(cx, i, elem);
        }

        JSObject *result;
        if (!NewParallelArray(cx, buffer, srcLen, &result))
            return false;

        args.rval().setObject(*result);
        return true;
    }

    /* second case: init using length and function */
    /* extract first argument, the length */
    uint32_t length;
    if (!ToUint32(cx, args[0], &length))
        return false;

    /* extract second argument, the elemental function */
    JSObject *elementalFun = js_ValueToCallableObject(cx, &args[1], JSV2F_SEARCH_STACK);
    if (!elementalFun)
        return false;

    /* use build with |this| set to |undefined| */
    return ParallelArray_build(cx, length, UndefinedValue(), elementalFun, false, 0, NULL, &(args.rval()));
}

/* forward declaration */
static JSBool
ParallelArray_mapOrCombine(JSContext *cx, unsigned argc, Value *vp, bool isMap);

static JSBool
ParallelArray_map(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_mapOrCombine(cx, argc, vp, true);
}

static JSBool
ParallelArray_combine(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_mapOrCombine(cx, argc, vp, false);
}

static JSBool
ParallelArray_mapOrCombine(JSContext *cx, unsigned argc, Value *vp, bool isMap)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* make sure we are called on a ParallelArray */
    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, (isMap ? ParallelArray_map : ParallelArray_combine),
                                          &ParallelArrayClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             (isMap ? "map" : "combine"), "0", "s");
        return false;
    }

    /* extract first argument, the elemental function */
    JSObject *elementalFun = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!elementalFun)
        return false;

    /* check extra arguments for map to be objects */
    if (isMap && (argc > 1))
        for (unsigned i = 1; i < argc; i++)
            if (!args[i].isObject()) {
                char buffer[4];
                JS_snprintf(buffer, 4, "%d", i+1);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_MAP_INVALID_ARG,
                                     buffer);

                return false;
            }

    return ParallelArray_build(cx, GetLength(obj), ObjectValue(*obj), elementalFun, isMap,
                               (isMap ? argc-1 : 0), ((argc > 1) ? &(args[1]) : NULL), &(args.rval()));
}

/* forward declaration */
static JSBool
ParallelArray_scanOrReduce(JSContext *cx, unsigned argc, Value *vp, bool isScan);

static JSBool
ParallelArray_scan(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_scanOrReduce(cx, argc, vp, true);
}

static JSBool
ParallelArray_reduce(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_scanOrReduce(cx, argc, vp, false);
}

static JSBool
ParallelArray_scanOrReduce(JSContext *cx, unsigned argc, Value *vp, bool isScan)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* make sure we are called on a ParallelArray */
    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, (isScan ? ParallelArray_scan : ParallelArray_reduce), &ParallelArrayClass, &ok);
    if (!obj)
        return ok;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             (isScan ? "scan" : "reduce"), "0", "s");
        return false;
    }

    uint32_t length = GetLength(obj);

    JSObject *result = NULL, *resBuffer = NULL;
    if (isScan) {
        /* create data store for results */
        resBuffer = NewDenseAllocatedArray(cx, length);
        if (!resBuffer)
            return false;

        resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

        /* create ParallelArray wrapper class */
        if (!NewParallelArray(cx, resBuffer, length, &result))
            return false;
    }

    /* extract first argument, the elemental function */
    JSObject *elementalFun = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!elementalFun)
        return false;

    /* special case of empty arrays */
    if (length == 0) {
        args.rval() = (isScan ? ObjectValue(*result) : UndefinedValue());
        return true;
    }

    JSObject *buffer = GetBuffer(obj);

    Value accu = buffer->getDenseArrayElement(0);
    if (isScan)
        resBuffer->setDenseArrayElementWithType(cx, 0, accu);

    /* prepare call frame on stack */
    InvokeArgsGuard ag;
    if (!cx->stack.pushInvokeArgs(cx, 2, &ag))
        return false;

    for (uint32_t i = 1; i < length; i++) {
        /* fill frame with current values */
        ag.setCallee(ObjectValue(*elementalFun));
        ag[0] = accu;
        ag[1] = buffer->getDenseArrayElement(i);

        /* We set |this| inside of the kernel to the |this| we were invoked on. */
        /* This is a random choice, as we need some value here. */
        ag.thisv() = args.thisv();

        /* call */
        if (!Invoke(cx, ag))
            return false;

        /* remember result for next round */
        accu = ag.rval();
        if (isScan)
            resBuffer->setDenseArrayElementWithType(cx, i, accu);
    }

    args.rval() = (isScan ? ObjectValue(*result) : accu);

    return true;
}

static JSBool
ParallelArray_filter(JSContext *cx, unsigned argc, Value *vp) {
    CallArgs args = CallArgsFromVp(argc, vp);

    /* make sure we are called on a ParallelArray */
    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, ParallelArray_filter, &ParallelArrayClass, &ok);
    if (!obj)
        return false;

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "filter", "0", "s");
        return false;
    }

    /* extract first argument, the elemental function */
    JSObject *elementalFun = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!elementalFun)
        return false;

    JSObject *buffer = GetBuffer(obj);
    uint32_t length = GetLength(obj);

    /* We just assume the length of the input as the length of the output. */
    JSObject *resBuffer = NewDenseAllocatedArray(cx, length);
    if (!resBuffer)
        return false;

    resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* prepare call frame on stack */
    InvokeArgsGuard frame;
    cx->stack.pushInvokeArgs(cx, 1, &frame);

    uint32_t pos = 0;
    for (uint32_t i = 0; i < length; i++) {
        frame.setCallee(ObjectValue(*elementalFun));
        frame[0].setNumber(i);
        frame.thisv() = ObjectValue(*obj);

        /* call */
        if (!Invoke(cx, frame))
            return false;

        if (js_ValueToBoolean(frame.rval()))
            resBuffer->setDenseArrayElementWithType(cx, pos++, buffer->getDenseArrayElement(i));
    }

    /* shrink the array to the proper size */
    resBuffer->setArrayLength(cx, pos);

    /* create ParallelArray wrapper class */
    JSObject *result;
    if (!NewParallelArray(cx, resBuffer, pos, &result))
        return false;

    args.rval() = ObjectValue(*result);
    return true;
}

static JSBool
ParallelArray_scatter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* make sure we are called on a ParallelArray */
    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, ParallelArray_scatter, &ParallelArrayClass, &ok);
    if (!obj)
        return false;

    JSObject *buffer = GetBuffer(obj);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "scatter", "0", "s");
        return false;
    }

    /* grab the scatter vector */
    if (!args[0].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_INVALID_VEC);
        return false;
    }
    JSObject &targets = args[0].toObject();

    uint32_t scatterLen;
    if (!JS_GetArrayLength(cx, &targets, &scatterLen))
        return false;

    if (scatterLen > GetLength(obj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_INVALID_VEC);
        return false;
    }

    /* next, default value */
    Value defValue = UndefinedValue();
    if (args.length() >= 2)
        defValue = args[1];

    JSObject *conflictFun = NULL;
    /* conflict resolution function */
    if ((args.length() >= 3) && !args[2].isUndefined()) {
        conflictFun = js_ValueToCallableObject(cx, &args[2], JSV2F_SEARCH_STACK);
        if (!conflictFun)
            return false;
    }

    /* optional length */
    uint32_t length;
    if (args.length() >= 4) {
        if (!ToUint32(cx, args[3], &length))
            return false;
    } else {
        /* we assume the source's length */
        length = GetLength(obj);
    }

    /* allocate space for the result */
    JSObject *resBuffer = NewDenseAllocatedArray(cx, length);
    if (!resBuffer)
        return false;

    resBuffer->ensureDenseArrayInitializedLength(cx, length, 0);

    /* iterate over the scatter vector */
    for (uint32_t i = 0; i < scatterLen; i++) {
        /* read target index */
        Value elem;
        if (!targets.getElement(cx, &targets, i, &elem))
            return false;

        uint32_t targetIdx;
        if (!ToUint32(cx, elem, &targetIdx))
            return false;

        if (targetIdx >= length) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_BNDS);
            return false;
        }

        /* read current value */
        Value readV = buffer->getDenseArrayElement(i);

        Value previous = resBuffer->getDenseArrayElement(targetIdx);

        if (!previous.isMagic(JS_ARRAY_HOLE)) {
            if (conflictFun) {
                /* we have a conflict, so call the resolution function to resovle it */
                InvokeArgsGuard ag;
                if (!cx->stack.pushInvokeArgs(cx, 2, &ag))
                    return false;
                ag.setCallee(ObjectValue(*conflictFun));
                ag[0] = readV;
                ag[1] = previous;

                /* random choice for |this| */
                ag.thisv() = args.thisv();

                if (!Invoke(cx, ag))
                    return false;

                readV = ag.rval();
            } else {
                /* no conflict function defined, yet we have a conflict -> fail */
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_SCATTER_CONFLICT);
                return false;
            }
        }

        /* write back */
        resBuffer->setDenseArrayElementWithType(cx, targetIdx, readV);
    }

    /* fill holes */
    for (uint32_t i = 0; i < length; i++) {
        if (resBuffer->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE))
            resBuffer->setDenseArrayElementWithType(cx, i, defValue);
    }

    /* create ParallelArray wrapper class */
    JSObject *result;
    if (!NewParallelArray(cx, resBuffer, length, &result))
        return false;

    args.rval() = ObjectValue(*result);
    return true;
}

static JSBool
ParallelArray_forward_method(JSContext *cx, unsigned argc, Value *vp, Native native, jsid id)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool ok;
    JSObject *obj = NonGenericMethodGuard(cx, args, native, &ParallelArrayClass, &ok);
    if (!ok)
        return false;

    Value callable;
    RootedVarObject buffer(cx, GetBuffer(obj));
    if (!js_GetMethod(cx, buffer, id, 0, &callable))
        return false;

    Value rval;
    if (!Invoke(cx, ObjectOrNullValue(buffer), callable, argc, vp, &rval))
        return false;

    *vp = rval;
    return true;
}

static JSBool
ParallelArray_toString(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_forward_method(cx, argc, vp, ParallelArray_toString,
                                        ATOM_TO_JSID(cx->runtime->atomState.toStringAtom));
}

static JSBool
ParallelArray_toLocaleString(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_forward_method(cx, argc, vp, ParallelArray_toLocaleString,
                                        ATOM_TO_JSID(cx->runtime->atomState.toStringAtom));
}

static JSBool
ParallelArray_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    return ParallelArray_forward_method(cx, argc, vp, ParallelArray_toSource,
                                        ATOM_TO_JSID(cx->runtime->atomState.toStringAtom));
}

static JSBool
ParallelArray_length_getter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /* we do not support prototype chaining for now */
    if (obj->getClass() != &ParallelArrayClass)
        return false;

    vp->setNumber(GetLength(obj));
    return true;
}

/* Checks whether the index is in range. We guarantee dense arrays. */
static inline bool
IsDenseArrayIndex(JSObject *obj, uint32_t index)
{
    JS_ASSERT(obj->isDenseArray());

    return index < obj->getDenseArrayInitializedLength();
}

/* checks whether id is an index */
static inline bool
IsDenseArrayId(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(obj->isDenseArray());

    uint32_t i;
    return (js_IdIsIndex(id, &i) && IsDenseArrayIndex(obj, i));
}

static JSBool
ParallelArray_lookupGeneric(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                            JSProperty **propp)
{
    JSObject *buffer = GetBuffer(obj);

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
        IsDenseArrayId(cx, buffer, id))
    {
        *propp = (JSProperty *) 1; /* TRUE */
        *objp = obj;
        return true;
    }

    JSObject *proto = obj->getProto();
    if (proto)
        return proto->lookupGeneric(cx, id, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

static JSBool
ParallelArray_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name, JSObject **objp,
                             JSProperty **propp)
{
    return ParallelArray_lookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

static JSBool
ParallelArray_lookupElement(JSContext *cx, JSObject *obj, uint32_t index, JSObject **objp,
                            JSProperty **propp)
{
    if (IsDenseArrayIndex(GetBuffer(obj), index)) {
        *propp = (JSProperty *) 1;  /* TRUE */
        *objp = obj;
        return true;
    }

    JSObject *proto = obj->getProto();
    if (proto)
        return proto->lookupElement(cx, index, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

static JSBool
ParallelArray_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp,
                            JSProperty **propp)
{
    return ParallelArray_lookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

static JSBool
ParallelArray_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        vp->setNumber(GetLength(obj));
        return true;
    }

    JSObject *buffer = GetBuffer(obj);
    if (IsDenseArrayId(cx, buffer, id))
        return buffer->getGeneric(cx, receiver, id, vp);

    JSObject *proto = obj->getProto();
    if (proto)
        return proto->getGeneric(cx, receiver, id, vp);

    vp->setUndefined();
    return true;
}

static JSBool
ParallelArray_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name, Value *vp)
{
    return ParallelArray_getGeneric(cx, obj, receiver, ATOM_TO_JSID(name), vp);
}

static JSBool
ParallelArray_getElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
{
    JSObject *buffer = GetBuffer(obj);
    if (IsDenseArrayIndex(buffer, index)) {
        *vp = buffer->getDenseArrayElement(index);
        return true;
    }

    JSObject *proto = obj->getProto();
    if (proto)
        return proto->getElement(cx, receiver, index, vp);

    vp->setUndefined();
    return true;
}

static JSBool
ParallelArray_getSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
{
    return ParallelArray_getGeneric(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

static JSBool
ParallelArray_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                            JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_defineProperty(JSContext *cx, JSObject *obj, PropertyName *name, const Value *value,
                             JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return ParallelArray_defineGeneric(cx, obj, ATOM_TO_JSID(name), value, getter, setter, attrs);
}

static JSBool
ParallelArray_defineElement(JSContext *cx, JSObject *obj, uint32_t index, const Value *value,
                            PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *value,
                            PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return ParallelArray_defineGeneric(cx, obj, SPECIALID_TO_JSID(sid), value, getter, setter, attrs);
}

static JSBool
ParallelArray_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
{
    return ParallelArray_setGeneric(cx, obj, ATOM_TO_JSID(name), vp, strict);
}

static JSBool
ParallelArray_setElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
{
    return ParallelArray_setGeneric(cx, obj, SPECIALID_TO_JSID(sid), vp, strict);
}

static JSBool
ParallelArray_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY;
    } else {
        /* this must be an element then */
        *attrsp = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE;
    }

    return true;
}

static JSBool
ParallelArray_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    return ParallelArray_getGenericAttributes(cx, obj, ATOM_TO_JSID(name), attrsp);
}

static JSBool
ParallelArray_getElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    *attrsp = JSPROP_PERMANENT | JSPROP_READONLY | JSPROP_ENUMERATE;
    return true;
}

static JSBool
ParallelArray_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    return ParallelArray_getGenericAttributes(cx, obj, SPECIALID_TO_JSID(sid), attrsp);
}

static JSBool
ParallelArray_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, unsigned *attrsp)
{
    return ParallelArray_setGenericAttributes(cx, obj, ATOM_TO_JSID(name), attrsp);
}

static JSBool
ParallelArray_setElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, unsigned *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, unsigned *attrsp)
{
    return ParallelArray_setGenericAttributes(cx, obj, SPECIALID_TO_JSID(sid), attrsp);
}

static JSBool
ParallelArray_deleteGeneric(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_deleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
{
    return ParallelArray_deleteGeneric(cx, obj, ATOM_TO_JSID(name), rval, strict);
}

static JSBool
ParallelArray_deleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_PAR_ARRAY_IMMUTABLE);

    return false;
}

static JSBool
ParallelArray_deleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
{
    return ParallelArray_deleteGeneric(cx, obj, SPECIALID_TO_JSID(sid), rval, strict);
}

static JSBool
ParallelArray_enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op, Value *statep, jsid *idp)
{
    /*
     * Iteration is "length" (if JSENUMERATE_INIT_ALL), then [0, length).
     * *statep is JSVAL_TRUE if enumerating "length" and
     * JSVAL_TO_INT(index) when enumerating index.
     */
    switch (enum_op) {
      case JSENUMERATE_INIT_ALL:
        statep->setBoolean(true);
        if (idp)
            *idp = ::INT_TO_JSID(GetLength(obj) + 1);
        break;

      case JSENUMERATE_INIT:
        statep->setInt32(0);
        if (idp)
            *idp = ::INT_TO_JSID(GetLength(obj));
        break;

      case JSENUMERATE_NEXT:
        if (statep->isTrue()) {
            *idp = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
            statep->setInt32(0);
        } else {
            uint32_t index = statep->toInt32();
            if (index < GetLength(obj)) {
                *idp = ::INT_TO_JSID(index);
                statep->setInt32(index + 1);
            } else {
                JS_ASSERT(index == GetLength(obj));
                statep->setNull();
            }
        }
        break;

      case JSENUMERATE_DESTROY:
        statep->setNull();
        break;
    }

    return true;
}

Class js::ParallelArrayProtoClass = {
    "ParallelArray",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class js::ParallelArrayClass = {
    "ParallelArray",
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_PA_MAX) | JSCLASS_HAS_CACHED_PROTO(JSProto_ParallelArray) | Class::NON_NATIVE,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* reserved0   */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    NULL,                    /* trace */
    JS_NULL_CLASS_EXT,
    {
        ParallelArray_lookupGeneric,
        ParallelArray_lookupProperty,
        ParallelArray_lookupElement,
        ParallelArray_lookupSpecial,
        ParallelArray_defineGeneric,
        ParallelArray_defineProperty,
        ParallelArray_defineElement,
        ParallelArray_defineSpecial,
        ParallelArray_getGeneric,
        ParallelArray_getProperty,
        ParallelArray_getElement,
        NULL,       /* getElementIfPresent */
        ParallelArray_getSpecial,
        ParallelArray_setGeneric,
        ParallelArray_setProperty,
        ParallelArray_setElement,
        ParallelArray_setSpecial,
        ParallelArray_getGenericAttributes,
        ParallelArray_getPropertyAttributes,
        ParallelArray_getElementAttributes,
        ParallelArray_getSpecialAttributes,
        ParallelArray_setGenericAttributes,
        ParallelArray_setPropertyAttributes,
        ParallelArray_setElementAttributes,
        ParallelArray_setSpecialAttributes,
        ParallelArray_deleteProperty,
        ParallelArray_deleteElement,
        ParallelArray_deleteSpecial,
        ParallelArray_enumerate,
        NULL,       /* typeof         */
        NULL,       /* thisObject     */
        NULL,       /* clear          */
    }
};

static JSFunctionSpec parallel_array_methods[] = {
    JS_FN("get",                 ParallelArray_get,            1, 0),
    JS_FN("map",                 ParallelArray_map,            1, 0),
    JS_FN("combine",             ParallelArray_combine,        1, 0),
    JS_FN("scan",                ParallelArray_scan,           1, 0),
    JS_FN("reduce",              ParallelArray_reduce,         1, 0),
    JS_FN("filter",              ParallelArray_filter,         1, 0),
    JS_FN("scatter",             ParallelArray_scatter,        1, 0),
    JS_FN(js_toString_str,       ParallelArray_toString,       0, 0),
    JS_FN(js_toLocaleString_str, ParallelArray_toLocaleString, 0, 0),
    JS_FN(js_toSource_str,       ParallelArray_toSource,       0, 0),
    JS_FS_END
};

JSObject *
js_InitParallelArrayClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    GlobalObject *global = &obj->asGlobal();

    JSObject *parallelArrayProto = global->createBlankPrototype(cx, &ParallelArrayProtoClass);
    if (!parallelArrayProto)
        return NULL;
    /* define the length property */
    const jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);

    parallelArrayProto->addProperty(cx, lengthId, ParallelArray_length_getter, NULL,
                                    SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_READONLY, 0, 0);

    JSFunction *ctor = global->createConstructor(cx, ParallelArray_construct, CLASS_ATOM(cx, ParallelArray), 0);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, parallelArrayProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, parallelArrayProto, NULL, parallel_array_methods))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_ParallelArray, ctor, parallelArrayProto))
        return NULL;
    return parallelArrayProto;
}
