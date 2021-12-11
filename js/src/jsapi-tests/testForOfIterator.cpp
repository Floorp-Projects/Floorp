/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ForOfIterator.h"
#include "jsapi-tests/tests.h"

BEGIN_TEST(testForOfIterator_basicNonIterable) {
  JS::RootedValue v(cx);
  // Hack to make it simple to produce an object that has a property
  // named Symbol.iterator.
  EVAL("({[Symbol.iterator]: 5})", &v);
  JS::ForOfIterator iter(cx);
  bool ok = iter.init(v);
  CHECK(!ok);
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testForOfIterator_basicNonIterable)

BEGIN_TEST(testForOfIterator_bug515273_part1) {
  JS::RootedValue v(cx);

  // Hack to make it simple to produce an object that has a property
  // named Symbol.iterator.
  EVAL("({[Symbol.iterator]: 5})", &v);

  JS::ForOfIterator iter(cx);
  bool ok = iter.init(v, JS::ForOfIterator::AllowNonIterable);
  CHECK(!ok);
  JS_ClearPendingException(cx);
  return true;
}
END_TEST(testForOfIterator_bug515273_part1)

BEGIN_TEST(testForOfIterator_bug515273_part2) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);
  JS::RootedValue v(cx, JS::ObjectValue(*obj));

  JS::ForOfIterator iter(cx);
  bool ok = iter.init(v, JS::ForOfIterator::AllowNonIterable);
  CHECK(ok);
  CHECK(!iter.valueIsIterable());
  return true;
}
END_TEST(testForOfIterator_bug515273_part2)
