/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Id.h"
#include "js/PropertyAndElement.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testDeleteProperty) {
  JS::RootedValue val(cx);
  EVAL("var obj = {a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8}; obj", &val);
  CHECK(val.isObject());
  JS::RootedObject obj(cx, &val.toObject());

  JS::Rooted<JS::PropertyKey> key(cx);

  auto createPropertyKey = [](JSContext* cx, const char* s,
                              JS::MutableHandle<JS::PropertyKey> key) {
    JSString* atom = JS_AtomizeString(cx, s);
    if (!atom) {
      return false;
    }
    key.set(JS::PropertyKey::NonIntAtom(atom));
    return true;
  };

  // Test delete APIs without an ObjectOpResult argument.
  CHECK(JS_DeleteProperty(cx, obj, "b"));
  CHECK(createPropertyKey(cx, "d", &key));
  CHECK(JS_DeletePropertyById(cx, obj, key));

  // Test delete APIs with an ObjectOpResult argument.
  JS::ObjectOpResult result;
  CHECK(JS_DeleteProperty(cx, obj, "e", result));
  CHECK(result);
  CHECK(createPropertyKey(cx, "f", &key));
  CHECK(JS_DeletePropertyById(cx, obj, key, result));
  CHECK(result);

  // Check properties were deleted.
  EVAL("JSON.stringify(obj)", &val);
  CHECK(val.isString());
  bool match = false;
  CHECK(JS_StringEqualsAscii(cx, val.toString(),
                             "{\"a\":1,\"c\":3,\"g\":7,\"h\":8}", &match));
  CHECK(match);

  return true;
}
END_TEST(testDeleteProperty)
