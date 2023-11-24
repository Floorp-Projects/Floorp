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

  // Skip test if some part of the nursery is disabled (via env var, for
  // example.)
  if (!cx->zone()->allocNurseryObjects() ||
      !cx->zone()->allocNurseryStrings()) {
    return true;
  }

  JS_GC(cx);

  JS::Rooted<JSObject*> object(cx, JS_NewPlainObject(cx));
  const char oolstr[] =
      "my hands are floppy dark red pieces of liver, large "
      "enough to exceed the space of an inline string";
  JS::Rooted<JSString*> string(cx, JS_NewStringCopyZ(cx, oolstr));

  /* Objects are initially allocated in the nursery. */
  CHECK(js::gc::IsInsideNursery(object));
  /* As are strings. */
  CHECK(js::gc::IsInsideNursery(string));
  /* And their contents. */
  {
    JS::AutoCheckCannotGC nogc;
    const JS::Latin1Char* strdata =
        string->asLinear().nonInlineLatin1Chars(nogc);
    CHECK(cx->nursery().isInside(strdata));
  }

  JS_GC(cx);

  /* And are tenured if still live after a GC. */
  CHECK(!js::gc::IsInsideNursery(object));
  CHECK(!js::gc::IsInsideNursery(string));
  {
    JS::AutoCheckCannotGC nogc;
    const JS::Latin1Char* strdata =
        string->asLinear().nonInlineLatin1Chars(nogc);
    CHECK(!cx->nursery().isInside(strdata));
  }

  return true;
}
END_TEST(testIsInsideNursery)
