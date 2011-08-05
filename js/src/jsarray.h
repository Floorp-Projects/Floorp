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
#include "jsatom.h"
#include "jsobj.h"

/* Small arrays are dense, no matter what. */
const uintN MIN_SPARSE_INDEX = 256;

inline JSObject::EnsureDenseResult
JSObject::ensureDenseArrayElements(JSContext *cx, uintN index, uintN extra)
{
    JS_ASSERT(isDenseArray());
    uintN currentCapacity = numSlots();

    uintN requiredCapacity;
    if (extra == 1) {
        /* Optimize for the common case. */
        if (index < currentCapacity)
            return ED_OK;
        requiredCapacity = index + 1;
        if (requiredCapacity == 0) {
            /* Overflow. */
            return ED_SPARSE;
        }
    } else {
        requiredCapacity = index + extra;
        if (requiredCapacity < index) {
            /* Overflow. */
            return ED_SPARSE;
        }
        if (requiredCapacity <= currentCapacity)
            return ED_OK;
    }

    /*
     * We use the extra argument also as a hint about number of non-hole
     * elements to be inserted.
     */
    if (requiredCapacity > MIN_SPARSE_INDEX &&
        willBeSparseDenseArray(requiredCapacity, extra)) {
        return ED_SPARSE;
    }
    return growSlots(cx, requiredCapacity) ? ED_OK : ED_FAILED;
}

namespace js {
/* 2^32-2, inclusive */
const uint32 MAX_ARRAY_INDEX = 4294967294u;
    
extern bool
StringIsArrayIndex(JSLinearString *str, jsuint *indexp);
    
}

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

    return js::StringIsArrayIndex(JSID_TO_ATOM(id), indexp);
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

namespace js
{

/* Create a dense array with no capacity allocated, length set to 0. */
extern JSObject * JS_FASTCALL
NewDenseEmptyArray(JSContext *cx, JSObject *proto=NULL);

/* Create a dense array with length and capacity == 'length'. */
extern JSObject * JS_FASTCALL
NewDenseAllocatedArray(JSContext *cx, uint length, JSObject *proto=NULL);

/*
 * Create a dense array with a set length, but without allocating space for the
 * contents. This is useful, e.g., when accepting length from the user.
 */
extern JSObject * JS_FASTCALL
NewDenseUnallocatedArray(JSContext *cx, uint length, JSObject *proto=NULL);

/* Create a dense array with a copy of vp. */
extern JSObject *
NewDenseCopiedArray(JSContext *cx, uint length, const Value *vp, JSObject *proto=NULL);

/* Create a sparse array. */
extern JSObject *
NewSlowEmptyArray(JSContext *cx);

}

extern JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsdouble length);

namespace js {

extern JSBool
array_defineProperty(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                     PropertyOp getter, StrictPropertyOp setter, uintN attrs);

extern JSBool
array_deleteProperty(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict);

/*
 * This function assumes 'length' is effectively the result of calling
 * js_GetLengthProperty on aobj.
 */
extern bool
GetElements(JSContext *cx, JSObject *aobj, jsuint length, js::Value *vp);

}

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

/*
 * Append the given (non-hole) value to the end of an array.  The array must be
 * a newborn array -- that is, one which has not been exposed to script for
 * arbitrary manipulation.  (This method optimizes on the assumption that
 * extending the array to accommodate the element will never make the array
 * sparse, which requires that the array be completely filled.)
 */
extern JSBool
js_NewbornArrayPush(JSContext *cx, JSObject *obj, const js::Value &v);

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

extern JSBool JS_FASTCALL
js_EnsureDenseArrayCapacity(JSContext *cx, JSObject *obj, jsint i);

#endif /* jsarray_h___ */
