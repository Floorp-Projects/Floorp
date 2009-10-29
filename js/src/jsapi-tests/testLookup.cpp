#include "tests.h"
#include "jsfun.h"  // for js_IsInternalFunctionObject

BEGIN_TEST(testLookup_bug522590)
{
    // Define a function that makes method-bearing objects.
    jsvalRoot x(cx);
    EXEC("function mkobj() { return {f: function () {return 2;}} }");

    // Calling mkobj() multiple times must create multiple functions in ES5.
    EVAL("mkobj().f !== mkobj().f", x.addr());
    CHECK_SAME(x, JSVAL_TRUE);

    // Now make x.f a method.
    EVAL("mkobj()", x.addr());
    JSObject *xobj = JSVAL_TO_OBJECT(x);

    // This lookup must not return an internal function object.
    jsvalRoot r(cx);
    CHECK(JS_LookupProperty(cx, xobj, "f", r.addr()));
    CHECK(JSVAL_IS_OBJECT(r));
    JSObject *funobj = JSVAL_TO_OBJECT(r);
    CHECK(HAS_FUNCTION_CLASS(funobj));
    CHECK(!js_IsInternalFunctionObject(funobj));
    CHECK(GET_FUNCTION_PRIVATE(cx, funobj) != (JSFunction *) funobj);

    return true;
}
END_TEST(testLookup_bug522590)
