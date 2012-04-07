#include "tests.h"

BEGIN_TEST(test_BindCallable)
{
  jsval v;
  EVAL("({ somename : 1717 })", &v);
  CHECK(JSVAL_IS_OBJECT(v));

  jsval func;
  EVAL("(function() { return this.somename; })", &func);
  CHECK(JSVAL_IS_OBJECT(func));

  JSObject* newCallable = JS_BindCallable(cx, JSVAL_TO_OBJECT(func),
					  JSVAL_TO_OBJECT(v));
  CHECK(newCallable);

  jsval retval;
  bool called = JS_CallFunctionValue(cx, NULL, OBJECT_TO_JSVAL(newCallable),
				     0, NULL, &retval);
  CHECK(called);

  CHECK(JSVAL_IS_INT(retval));

  CHECK(JSVAL_TO_INT(retval) == 1717);
  return true;
}
END_TEST(test_BindCallable)
