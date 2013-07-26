/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_BinaryData_h
#define builtin_BinaryData_h

#include "jsapi.h"
#include "jsobj.h"
#include "jsfriendapi.h"
#include "gc/Heap.h"

namespace js {
class Block : public gc::Cell
{
};

typedef float float32_t;
typedef double float64_t;

enum {
    NUMERICTYPE_UINT8 = 0,
    NUMERICTYPE_UINT16,
    NUMERICTYPE_UINT32,
    NUMERICTYPE_UINT64,
    NUMERICTYPE_INT8,
    NUMERICTYPE_INT16,
    NUMERICTYPE_INT32,
    NUMERICTYPE_INT64,
    NUMERICTYPE_FLOAT32,
    NUMERICTYPE_FLOAT64,
    NUMERICTYPES
};

enum TypeCommonSlots {
    SLOT_MEMSIZE = 0,
    SLOT_ALIGN,
    TYPE_RESERVED_SLOTS
};

enum BlockCommonSlots {
    SLOT_DATATYPE = 0,
    BLOCK_RESERVED_SLOTS
};

static Class DataClass = {
    "Data",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Data),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static Class TypeClass = {
    "Type",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Type),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template <typename T>
class NumericType
{
    private:
        static Class * typeToClass();
    public:
        static bool convert(JSContext *cx, HandleValue val, T *converted);
        static bool reify(JSContext *cx, void *mem, MutableHandleValue vp);
        static JSBool call(JSContext *cx, unsigned argc, Value *vp);
};

template <typename T>
JS_ALWAYS_INLINE
bool NumericType<T>::reify(JSContext *cx, void *mem, MutableHandleValue vp)
{
    vp.setInt32(* ((T*)mem) );
    return true;
}

template <>
JS_ALWAYS_INLINE
bool NumericType<float32_t>::reify(JSContext *cx, void *mem, MutableHandleValue vp)
{
    vp.setNumber(* ((float32_t*)mem) );
    return true;
}

template <>
JS_ALWAYS_INLINE
bool NumericType<float64_t>::reify(JSContext *cx, void *mem, MutableHandleValue vp)
{
    vp.setNumber(* ((float64_t*)mem) );
    return true;
}

#define BINARYDATA_FOR_EACH_NUMERIC_TYPES(macro_)\
    macro_(NUMERICTYPE_UINT8,    uint8)\
    macro_(NUMERICTYPE_UINT16,   uint16)\
    macro_(NUMERICTYPE_UINT32,   uint32)\
    macro_(NUMERICTYPE_UINT64,   uint64)\
    macro_(NUMERICTYPE_INT8,     int8)\
    macro_(NUMERICTYPE_INT16,    int16)\
    macro_(NUMERICTYPE_INT32,    int32)\
    macro_(NUMERICTYPE_INT64,    int64)\
    macro_(NUMERICTYPE_FLOAT32,  float32)\
    macro_(NUMERICTYPE_FLOAT64,  float64)

#define BINARYDATA_NUMERIC_CLASSES(constant_, type_)\
{\
    #type_,\
    JSCLASS_HAS_RESERVED_SLOTS(1) |\
    JSCLASS_HAS_CACHED_PROTO(JSProto_##type_),\
    JS_PropertyStub,       /* addProperty */\
    JS_DeletePropertyStub, /* delProperty */\
    JS_PropertyStub,       /* getProperty */\
    JS_StrictPropertyStub, /* setProperty */\
    JS_EnumerateStub,\
    JS_ResolveStub,\
    JS_ConvertStub,\
    NULL,\
    NULL,\
    NumericType<type_##_t>::call,\
    NULL,\
    NULL,\
    NULL\
},

static Class NumericTypeClasses[NUMERICTYPES] = {
    BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_NUMERIC_CLASSES)
};

static Class ArrayTypeClass = {
    "ArrayType",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static Class StructTypeClass = {
    "StructType",
    JSCLASS_HAS_CACHED_PROTO(JSProto_StructType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

class ArrayTypeObject : public JSObject
{
    public:
        static JSBool repeat(JSContext *cx, unsigned int argc, jsval *vp);
};
}

#endif /* builtin_BinaryData_h */
