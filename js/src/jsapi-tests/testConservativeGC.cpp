/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(JSGC_USE_EXACT_ROOTING)

#include "jsobj.h"

#include "jsapi-tests/tests.h"
#include "vm/String.h"

BEGIN_TEST(testConservativeGC)
{
    JS::RootedValue v2(cx);
    EVAL("({foo: 'bar'});", &v2);
    CHECK(v2.isObject());
    char objCopy[sizeof(JSObject)];
    js_memcpy(&objCopy, v2.toObjectOrNull(), sizeof(JSObject));

    JS::RootedValue v3(cx);
    EVAL("String(Math.PI);", &v3);
    CHECK(v3.isString());
    char strCopy[sizeof(JSString)];
    js_memcpy(&strCopy, v3.toString(), sizeof(JSString));

    JS::RootedValue tmp(cx);
    EVAL("({foo2: 'bar2'});", &tmp);
    CHECK(tmp.isObject());
    JS::RootedObject obj2(cx, tmp.toObjectOrNull());
    char obj2Copy[sizeof(JSObject)];
    js_memcpy(&obj2Copy, obj2, sizeof(JSObject));

    EVAL("String(Math.sqrt(3));", &tmp);
    CHECK(tmp.isString());
    JS::RootedString str2(cx, tmp.toString());
    char str2Copy[sizeof(JSString)];
    js_memcpy(&str2Copy, str2, sizeof(JSString));

    tmp = JSVAL_NULL;

    JS_GC(rt);

    EVAL("var a = [];\n"
         "for (var i = 0; i != 10000; ++i) {\n"
         "a.push(i + 0.1, [1, 2], String(Math.sqrt(i)), {a: i});\n"
         "}", &tmp);

    JS_GC(rt);

    checkObjectFields((JSObject *)objCopy, v2.toObjectOrNull());
    CHECK(!memcmp(strCopy, v3.toString(), sizeof(strCopy)));

    checkObjectFields((JSObject *)obj2Copy, obj2);
    CHECK(!memcmp(str2Copy, str2, sizeof(str2Copy)));

    return true;
}

bool checkObjectFields(JSObject *savedCopy, JSObject *obj)
{
    /* Ignore fields which are unstable across GCs. */
    CHECK(savedCopy->lastProperty() == obj->lastProperty());
    return true;
}

END_TEST(testConservativeGC)

BEGIN_TEST(testDerivedValues)
{
  JSString *str = JS_NewStringCopyZ(cx, "once upon a midnight dreary");
  JS::Anchor<JSString *> str_anchor(str);
  static const jschar expected[] = { 'o', 'n', 'c', 'e' };
  const jschar *ch = JS_GetStringCharsZ(cx, str);
  str = nullptr;

  /* Do a lot of allocation and collection. */
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 1000; j++)
      JS_NewStringCopyZ(cx, "as I pondered weak and weary");
    JS_GC(rt);
  }

  CHECK(!memcmp(ch, expected, sizeof(expected)));
  return true;
}
END_TEST(testDerivedValues)

#endif /* !defined(JSGC_USE_EXACT_ROOTING) */
