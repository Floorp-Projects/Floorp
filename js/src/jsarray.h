/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsarray_h___
#define jsarray_h___
/*
 * JS Array interface.
 */
#include "jscntxt.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsatom.h"
#include "jsobj.h"

/* Small arrays are dense, no matter what. */
const unsigned MIN_SPARSE_INDEX = 256;

namespace js {
/* 2^32-2, inclusive */
const uint32_t MAX_ARRAY_INDEX = 4294967294u;
}

inline JSBool
js_IdIsIndex(jsid id, uint32_t *indexp)
{
    if (JSID_IS_INT(id)) {
        int32_t i = JSID_TO_INT(id);
        if (i < 0)
            return JS_FALSE;
        *indexp = (uint32_t)i;
        return JS_TRUE;
    }

    if (JS_UNLIKELY(!JSID_IS_STRING(id)))
        return JS_FALSE;

    return js::StringIsArrayIndex(JSID_TO_ATOM(id), indexp);
}

extern JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj);

extern bool
js_InitContextBusyArrayTable(JSContext *cx);

namespace js {

/* Create a dense array with no capacity allocated, length set to 0. */
extern JSObject * JS_FASTCALL
NewDenseEmptyArray(JSContext *cx, RawObject proto = NULL);

/* Create a dense array with length and capacity == 'length', initialized length set to 0. */
extern JSObject * JS_FASTCALL
NewDenseAllocatedArray(JSContext *cx, uint32_t length, RawObject proto = NULL);

/*
 * Create a dense array with a set length, but without allocating space for the
 * contents. This is useful, e.g., when accepting length from the user.
 */
extern JSObject * JS_FASTCALL
NewDenseUnallocatedArray(JSContext *cx, uint32_t length, RawObject proto = NULL);

/* Create a dense array with a copy of vp. */
extern JSObject *
NewDenseCopiedArray(JSContext *cx, uint32_t length, const Value *vp, RawObject proto = NULL);

/* Create a sparse array. */
extern JSObject *
NewSlowEmptyArray(JSContext *cx);

} /* namespace js */

extern JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, uint32_t *lengthp);

extern JSBool
js_SetLengthProperty(JSContext *cx, js::HandleObject obj, double length);

namespace js {

extern JSBool
array_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue value,
                    PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

extern JSBool
array_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                    MutableHandleValue rval, JSBool strict);

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

JSBool
js_PrototypeHasIndexedProperties(JSContext *cx, JSObject *obj);

/*
 * Utility to access the value from the id returned by array_lookupProperty.
 */
JSBool
js_GetDenseArrayElementValue(JSContext *cx, js::HandleObject obj, jsid id,
                             js::Value *vp);

/* Array constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Array(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsarray_h___ */
