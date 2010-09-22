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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsarray_h___
#define jsarray_h___
/*
 * JS Array interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsobj.h"

#define ARRAY_CAPACITY_MIN      7

extern JSBool
js_StringIsIndex(JSString *str, jsuint *indexp);

inline JSBool
js_IdIsIndex(jsid id, jsuint *indexp)
{
    if (JSID_IS_INT(id)) {
        jsint i;
        i = JSID_TO_INT(id);
        if (i < 0)
            return JS_FALSE;
        *indexp = (jsuint)i;
        return JS_TRUE;
    }

    if (JS_UNLIKELY(!JSID_IS_STRING(id)))
        return JS_FALSE;

    return js_StringIsIndex(JSID_TO_STRING(id), indexp);
}

/* XML really wants to pretend jsvals are jsids. */
inline JSBool
js_IdValIsIndex(jsval id, jsuint *indexp)
{
    if (JSVAL_IS_INT(id)) {
        jsint i;
        i = JSVAL_TO_INT(id);
        if (i < 0)
            return JS_FALSE;
        *indexp = (jsuint)i;
        return JS_TRUE;
    }

    if (!JSVAL_IS_STRING(id))
        return JS_FALSE;

    return js_StringIsIndex(JSVAL_TO_STRING(id), indexp);
}

extern js::Class js_ArrayClass, js_SlowArrayClass;

inline bool
JSObject::isDenseArray() const
{
    return getClass() == &js_ArrayClass;
}

inline bool
JSObject::isSlowArray() const
{
    return getClass() == &js_SlowArrayClass;
}

inline bool
JSObject::isArray() const
{
    return isDenseArray() || isSlowArray();
}

/*
 * Dense arrays are not native -- aobj->isNative() for a dense array aobj
 * results in false, meaning aobj->map does not point to a js::Shape.
 *
 * But Array methods are called via aobj.sort(), e.g., and the interpreter and
 * the trace recorder must consult the property cache in order to perform well.
 * The cache works only for native objects.
 *
 * Therefore the interpreter (js_Interpret in JSOP_GETPROP and JSOP_CALLPROP)
 * and js_GetPropertyHelper use this inline function to skip up one link in the
 * prototype chain when obj is a dense array, in order to find a native object
 * (to wit, Array.prototype) in which to probe for cached methods.
 *
 * Note that setting aobj.__proto__ for a dense array aobj turns aobj into a
 * slow array, avoiding the neede to skip.
 *
 * Callers of js_GetProtoIfDenseArray must take care to use the original object
 * (obj) for the |this| value of a getter, setter, or method call (bug 476447).
 */
static JS_INLINE JSObject *
js_GetProtoIfDenseArray(JSObject *obj)
{
    return obj->isDenseArray() ? obj->getProto() : obj;
}

extern JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj);

extern bool
js_InitContextBusyArrayTable(JSContext *cx);

/*
 * Creates a new array with the given length and proto (NB: NULL is not
 * translated to Array.prototype), with len slots preallocated.
 */
extern JSObject * JS_FASTCALL
js_NewArrayWithSlots(JSContext* cx, JSObject* proto, uint32 len);

extern JSObject *
js_NewArrayObject(JSContext *cx, jsuint length, const js::Value *vector);

/* Create an array object that starts out already made slow/sparse. */
extern JSObject *
js_NewSlowArrayObject(JSContext *cx);

extern JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsdouble length);

extern JSBool
js_HasLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JSBool JS_FASTCALL
js_IndexToId(JSContext *cx, jsuint index, jsid *idp);

/*
 * JS-specific merge sort function.
 */
typedef JSBool (*JSComparator)(void *arg, const void *a, const void *b,
                               int *result);

enum JSMergeSortElemType {
    JS_SORTING_VALUES,
    JS_SORTING_GENERIC
};

/*
 * NB: vec is the array to be sorted, tmp is temporary space at least as big
 * as vec. Both should be GC-rooted if appropriate.
 *
 * isValue should true iff vec points to an array of js::Value
 *
 * The sorted result is in vec. vec may be in an inconsistent state if the
 * comparator function cmp returns an error inside a comparison, so remember
 * to check the return value of this function.
 */
extern bool
js_MergeSort(void *vec, size_t nel, size_t elsize, JSComparator cmp,
             void *arg, void *tmp, JSMergeSortElemType elemType);

/*
 * The Array.prototype.sort fast-native entry point is exported for joined
 * function optimization in js{interp,tracer}.cpp.
 */
namespace js {
extern JSBool
array_sort(JSContext *cx, uintN argc, js::Value *vp);
}

#ifdef DEBUG
extern JSBool
js_ArrayInfo(JSContext *cx, uintN argc, jsval *vp);
#endif

extern JSBool
js_ArrayCompPush(JSContext *cx, JSObject *obj, const js::Value &vp);

/*
 * Fast dense-array-to-buffer conversion for use by canvas.
 *
 * If the array is a dense array, fill [offset..offset+count] values into
 * destination, assuming that types are consistent.  Return JS_TRUE if
 * successful, otherwise JS_FALSE -- note that the destination buffer may be
 * modified even if JS_FALSE is returned (e.g. due to finding an inappropriate
 * type later on in the array).  If JS_FALSE is returned, no error conditions
 * or exceptions are set on the context.
 *
 * This method succeeds if each element of the array is an integer or a double.
 * Values outside the 0-255 range are clamped to that range.  Double values are
 * converted to integers in this range by clamping and then rounding to
 * nearest, ties to even.
 */

JS_FRIEND_API(JSBool)
js_CoerceArrayToCanvasImageData(JSObject *obj, jsuint offset, jsuint count,
                                JSUint8 *dest);

JSBool
js_PrototypeHasIndexedProperties(JSContext *cx, JSObject *obj);

/*
 * Utility to access the value from the id returned by array_lookupProperty.
 */
JSBool
js_GetDenseArrayElementValue(JSContext *cx, JSObject *obj, jsid id,
                             js::Value *vp);

/* Array constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Array(JSContext *cx, uintN argc, js::Value *vp);

/*
 * Friend api function that allows direct creation of an array object with a
 * given capacity.  Non-null return value means allocation of the internal
 * buffer for a capacity of at least |capacity| succeeded.  A pointer to the
 * first element of this internal buffer is returned in the |vector| out
 * parameter.  The caller promises to fill in the first |capacity| values
 * starting from that pointer immediately after this function returns and
 * without triggering GC (so this method is allowed to leave those
 * uninitialized) and to set them to non-JS_ARRAY_HOLE-magic-why values, so
 * that the resulting array has length and count both equal to |capacity|.
 *
 * FIXME: for some strange reason, when this file is included from
 * dom/ipc/TabParent.cpp in MSVC, jsuint resolves to a slightly different
 * builtin than when mozjs.dll is built, resulting in a link error in xul.dll.
 * It would be useful to find out what is causing this insanity.
 */
JS_FRIEND_API(JSObject *)
js_NewArrayObjectWithCapacity(JSContext *cx, uint32_t capacity, jsval **vector);

/*
 * Makes a fast clone of a dense array as long as the array only contains
 * primitive values.
 *
 * If the return value is JS_FALSE then clone will not be set.
 *
 * If the return value is JS_TRUE then clone will either be set to the address
 * of a new JSObject or to NULL if the array was not dense or contained values
 * that were not primitives.
 */
JS_FRIEND_API(JSBool)
js_CloneDensePrimitiveArray(JSContext *cx, JSObject *obj, JSObject **clone);

/*
 * Returns JS_TRUE if the given object is a dense array that contains only
 * primitive values.
 */
JS_FRIEND_API(JSBool)
js_IsDensePrimitiveArray(JSObject *obj);

extern JSBool JS_FASTCALL
js_EnsureDenseArrayCapacity(JSContext *cx, JSObject *obj, jsint i);

#endif /* jsarray_h___ */
