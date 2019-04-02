/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/RegExp.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testObjectIsRegExp) {
  JS::RootedValue val(cx);

  bool isRegExp;

  EVAL("new Object", &val);
  JS::RootedObject obj(cx, val.toObjectOrNull());
  CHECK(JS::ObjectIsRegExp(cx, obj, &isRegExp));
  CHECK(!isRegExp);

  EVAL("/foopy/", &val);
  obj = val.toObjectOrNull();
  CHECK(JS::ObjectIsRegExp(cx, obj, &isRegExp));
  CHECK(isRegExp);

  return true;
}
END_TEST(testObjectIsRegExp)

BEGIN_TEST(testGetRegExpFlags) {
  JS::RootedValue val(cx);
  JS::RootedObject obj(cx);

  EVAL("/foopy/", &val);
  obj = val.toObjectOrNull();
  CHECK_EQUAL(JS::GetRegExpFlags(cx, obj), 0u);

  EVAL("/foopy/g", &val);
  obj = val.toObjectOrNull();
  CHECK(JS::GetRegExpFlags(cx, obj) == JS::RegExpFlags::Global);

  EVAL("/foopy/gi", &val);
  obj = val.toObjectOrNull();
  CHECK(JS::GetRegExpFlags(cx, obj) ==
        (JS::RegExpFlags::Global | JS::RegExpFlags::IgnoreCase));

  return true;
}
END_TEST(testGetRegExpFlags)

BEGIN_TEST(testGetRegExpSource) {
  JS::RootedValue val(cx);
  JS::RootedObject obj(cx);

  EVAL("/foopy/", &val);
  obj = val.toObjectOrNull();
  JSString* source = JS::GetRegExpSource(cx, obj);
  CHECK(source);
  CHECK(JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(source), "foopy"));

  return true;
}
END_TEST(testGetRegExpSource)
