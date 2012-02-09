#include "jsapi.h"

// See testValueABI.cpp

JSBool
C_ValueToObject(JSContext *cx, jsval v, JSObject **obj)
{
    return JS_ValueToObject(cx, v, obj);
}

jsval
C_GetEmptyStringValue(JSContext *cx)
{
    return JS_GetEmptyStringValue(cx);
}

size_t
C_jsvalAlignmentTest()
{
    typedef struct { char c; jsval v; } AlignTest;
    return sizeof(AlignTest);
}
