/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "vm/JSContext-inl.h"

BEGIN_TEST(testIsInsideNursery) {
  /* Non-GC things are never inside the nursery. */
  CHECK(!cx->nursery().isInside(cx));
  CHECK(!cx->nursery().isInside((void*)nullptr));

  JS_GC(cx);

  JS::RootedObject object(cx, JS_NewPlainObject(cx));

  /* Objects are initially allocated in the nursery. */
  CHECK(js::gc::IsInsideNursery(object));

  JS_GC(cx);

  /* And are tenured if still live after a GC. */
  CHECK(!js::gc::IsInsideNursery(object));

  return true;
}
END_TEST(testIsInsideNursery)
