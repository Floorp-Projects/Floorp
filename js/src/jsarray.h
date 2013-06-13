/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Array interface. */

#ifndef jsarray_h___
#define jsarray_h___

#include "jspubtd.h"
#include "jsobj.h"

namespace js {
/* 2^32-2, inclusive */
const uint32_t MAX_ARRAY_INDEX = 4294967294u;
}

inline JSBool
js_IdIsIndex(jsid id, uint32_t *indexp)
{
    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        JS_ASSERT(i >= 0);
        *indexp = (uint32_t)i;
        return true;
    }

    if (JS_UNLIKELY(!JSID_IS_STRING(id)))
        return false;

    return js::StringIsArrayIndex(JSID_TO_ATOM(id), indexp);
}

extern JSObject *
js_InitArrayClass(JSContext *cx, js::HandleObject obj);

extern bool
js_InitContextBusyArrayTable(JSContext *cx);

namespace js {

/* Create a dense array with no capacity allocated, length set to 0. */
extern JSObject * JS_FASTCALL
NewDenseEmptyArray(JSContext *cx, JSObject *proto = NULL,
                   NewObjectKind newKind = GenericObject);

/* Create a dense array with length and capacity == 'length', initialized length set to 0. */
extern JSObject * JS_FASTCALL
NewDenseAllocatedArray(JSContext *cx, uint32_t length, JSObject *proto = NULL,
                       NewObjectKind newKind = GenericObject);

/*
 * Create a dense array with a set length, but without allocating space for the
 * contents. This is useful, e.g., when accepting length from the user.
 */
extern JSObject * JS_FASTCALL
NewDenseUnallocatedArray(JSContext *cx, uint32_t length, JSObject *proto = NULL,
                         NewObjectKind newKind = GenericObject);

/* Create a dense array with a copy of the dense array elements in src. */
extern JSObject *
NewDenseCopiedArray(JSContext *cx, uint32_t length, HandleObject src, uint32_t elementOffset, JSObject *proto = NULL);

/* Create a dense array from the given array values, which must be rooted */
extern JSObject *
NewDenseCopiedArray(JSContext *cx, uint32_t length, const Value *values, JSObject *proto = NULL,
                    NewObjectKind newKind = GenericObject);

/*
 * Determines whether a write to the given element on |obj| should fail because
 * |obj| is an Array with a non-writable length, and writing that element would
 * increase the length of the array.
 */
extern bool
WouldDefinePastNonwritableLength(JSContext *cx, HandleObject obj, uint32_t index, bool strict,
                                 bool *definesPast);

/*
 * Canonicalize |vp| to a uint32_t value potentially suitable for use as an
 * array length.
 */
extern bool
CanonicalizeArrayLengthValue(JSContext *cx, HandleValue v, uint32_t *canonicalized);

extern JSBool
GetLengthProperty(JSContext *cx, HandleObject obj, uint32_t *lengthp);

extern JSBool
SetLengthProperty(JSContext *cx, HandleObject obj, double length);

extern JSBool
ObjectMayHaveExtraIndexedProperties(JSObject *obj);

/*
 * Copy 'length' elements from aobj to vp.
 *
 * This function assumes 'length' is effectively the result of calling
 * js_GetLengthProperty on aobj. vp must point to rooted memory.
 */
extern bool
GetElements(JSContext *cx, HandleObject aobj, uint32_t length, js::Value *vp);

/* Natives exposed for optimization by the interpreter and JITs. */

extern JSBool
array_sort(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
array_push(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
array_pop(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
array_concat(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
array_concat_dense(JSContext *cx, HandleObject obj1, HandleObject obj2, HandleObject result);

extern void
ArrayShiftMoveElements(JSObject *obj);

extern JSBool
array_shift(JSContext *cx, unsigned argc, js::Value *vp);

} /* namespace js */

#ifdef DEBUG
extern JSBool
js_ArrayInfo(JSContext *cx, unsigned argc, js::Value *vp);
#endif

/*
 * Append the given (non-hole) value to the end of an array.  The array must be
 * a newborn array -- that is, one which has not been exposed to script for
 * arbitrary manipulation.  (This method optimizes on the assumption that
 * extending the array to accommodate the element will never make the array
 * sparse, which requires that the array be completely filled.)
 */
extern JSBool
js_NewbornArrayPush(JSContext *cx, js::HandleObject obj, const js::Value &v);

/* Array constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Array(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsarray_h___ */
