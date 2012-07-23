/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

BEGIN_TEST(test_BindCallable)
{
  jsval v;
  EVAL("({ somename : 1717 })", &v);
  CHECK(v.isObject());

  jsval func;
  EVAL("(function() { return this.somename; })", &func);
  CHECK(func.isObject());

  JS::RootedObject funcObj(cx, JSVAL_TO_OBJECT(func));
  JS::RootedObject vObj(cx, JSVAL_TO_OBJECT(v));
  JSObject* newCallable = JS_BindCallable(cx, funcObj, vObj);
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
