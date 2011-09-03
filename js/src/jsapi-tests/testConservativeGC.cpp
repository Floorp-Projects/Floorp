#include "tests.h"
#include "jsobj.h"
#include "vm/String.h"

BEGIN_TEST(testConservativeGC)
{
    jsval v2;
    EVAL("({foo: 'bar'});", &v2);
    CHECK(JSVAL_IS_OBJECT(v2));
    JSObject objCopy = *JSVAL_TO_OBJECT(v2);

    jsval v3;
    EVAL("String(Math.PI);", &v3);
    CHECK(JSVAL_IS_STRING(v3));
    JSString strCopy = *JSVAL_TO_STRING(v3);

    jsval tmp;
    EVAL("({foo2: 'bar2'});", &tmp);
    CHECK(JSVAL_IS_OBJECT(tmp));
    JSObject *obj2 = JSVAL_TO_OBJECT(tmp);
    JSObject obj2Copy = *obj2;

    EVAL("String(Math.sqrt(3));", &tmp);
    CHECK(JSVAL_IS_STRING(tmp));
    JSString *str2 = JSVAL_TO_STRING(tmp);
    JSString str2Copy = *str2;

    tmp = JSVAL_NULL;

    JS_GC(cx);

    EVAL("var a = [];\n"
         "for (var i = 0; i != 10000; ++i) {\n"
         "a.push(i + 0.1, [1, 2], String(Math.sqrt(i)), {a: i});\n"
         "}", &tmp);

    JS_GC(cx);

    checkObjectFields(&objCopy, JSVAL_TO_OBJECT(v2));
    CHECK(!memcmp(&strCopy, JSVAL_TO_STRING(v3), sizeof(strCopy)));

    checkObjectFields(&obj2Copy, obj2);
    CHECK(!memcmp(&str2Copy, str2, sizeof(str2Copy)));

    return true;
}

bool checkObjectFields(JSObject *savedCopy, JSObject *obj)
{
    /* Ignore fields which are unstable across GCs. */
    CHECK(savedCopy->lastProp == obj->lastProp);
    CHECK(savedCopy->clasp == obj->clasp);
    CHECK(savedCopy->flags == obj->flags);
    CHECK(savedCopy->newType == obj->newType);
    CHECK(savedCopy->getProto() == obj->getProto());
    CHECK(savedCopy->parent == obj->parent);
    CHECK(savedCopy->privateData == obj->privateData);
    return true;
}

END_TEST(testConservativeGC)

BEGIN_TEST(testDerivedValues)
{
  JSString *str = JS_NewStringCopyZ(cx, "once upon a midnight dreary");
  JS::Anchor<JSString *> str_anchor(str);
  static const jschar expected[] = { 'o', 'n', 'c', 'e' };
  const jschar *ch = JS_GetStringCharsZ(cx, str);
  str = NULL;

  /* Do a lot of allocation and collection. */
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 1000; j++)
      JS_NewStringCopyZ(cx, "as I pondered weak and weary");
    JS_GC(cx);
  }

  CHECK(!memcmp(ch, expected, sizeof(expected)));
  return true;
}
END_TEST(testDerivedValues)
