#include "jsapi.h"

JSBool C_ValueToObject(JSContext *cx, jsval v, JSObject **obj)
{
    return JS_ValueToObject(cx, v, obj);
}

jsval
C_GetEmptyStringValue(JSContext *cx)
{
    return JS_GetEmptyStringValue(cx);
}
