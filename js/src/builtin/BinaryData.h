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


#define BINARYDATA_FOR_EACH_NUMERIC_TYPES(macro_)\
    macro_(uint8)\
    macro_(uint16)\
    macro_(uint32)\
    macro_(uint64)\
    macro_(int8)\
    macro_(int16)\
    macro_(int32)\
    macro_(int64)\
    macro_(float32)\
    macro_(float64)

#define BINARYDATA_NUMERIC_CLASSES(type_)\
static Class type_##BlockClass = {\
    #type_,\
    JSCLASS_HAS_CACHED_PROTO(JSProto_##type_),\
    JS_PropertyStub,       /* addProperty */\
    JS_DeletePropertyStub, /* delProperty */\
    JS_PropertyStub,       /* getProperty */\
    JS_StrictPropertyStub, /* setProperty */\
    JS_EnumerateStub,\
    JS_ResolveStub,\
    JS_ConvertStub\
};

BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_NUMERIC_CLASSES)

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
