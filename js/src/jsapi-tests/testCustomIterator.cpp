#include "tests.h"

#include "jsclass.h"

int count = 0;

static JSBool
IterNext(JSContext *cx, unsigned argc, jsval *vp)
{
    if (count++ == 100)
        return JS_ThrowStopIteration(cx);
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(count));
    return true;
}

static JSObject *
IterHook(JSContext *cx, JSObject *obj, JSBool keysonly)
{
    JSObject *iterObj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!iterObj)
        return NULL;
    if (!JS_DefineFunction(cx, iterObj, "next", IterNext, 0, 0))
        return NULL;
    return iterObj;
}

js::Class HasCustomIterClass = {
    "HasCustomIter",
    0,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,
    NULL, /* checkAccess */
    NULL, /* call */
    NULL, /* construct */
    NULL, /* hasInstance */
    NULL, /* mark */
    {
        NULL,
        NULL,
        NULL,
        IterHook,
        NULL
    }
};

JSBool
IterClassConstructor(JSContext *cx, unsigned argc, jsval *vp)
{
    JSObject *obj = JS_NewObjectForConstructor(cx, vp);
    if (!obj)
        return false;
    JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
    return true;
}

BEGIN_TEST(testCustomIterator_bug612523)
{
    CHECK(JS_InitClass(cx, JS_GetGlobalObject(cx), NULL, Jsvalify(&HasCustomIterClass),
                       IterClassConstructor, 0, NULL, NULL, NULL, NULL));

    jsval result;
    EVAL("var o = new HasCustomIter(); \n"
         "var j = 0; \n"
         "for (var i in o) { ++j; }; \n"
         "j;", &result);

    CHECK(JSVAL_IS_INT(result));
    CHECK_EQUAL(JSVAL_TO_INT(result), 100);
    CHECK_EQUAL(count, 101);

    return true;
}
END_TEST(testCustomIterator_bug612523)
