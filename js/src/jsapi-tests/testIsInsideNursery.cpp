/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "vm/JSContext-inl.h"

using namespace js;

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
  CHECK(gc::IsInsideNursery(object));
  /* As are strings. */
  CHECK(gc::IsInsideNursery(string));
  /* And their contents. */
  {
    JS::AutoCheckCannotGC nogc;
    const JS::Latin1Char* strdata =
        string->asLinear().nonInlineLatin1Chars(nogc);
    CHECK(cx->nursery().isInside(strdata));
  }

  JS_GC(cx);

  /* And are tenured if still live after a GC. */
  CHECK(!gc::IsInsideNursery(object));
  CHECK(!gc::IsInsideNursery(string));
  {
    JS::AutoCheckCannotGC nogc;
    const JS::Latin1Char* strdata =
        string->asLinear().nonInlineLatin1Chars(nogc);
    CHECK(!cx->nursery().isInside(strdata));
  }

  return true;
}
END_TEST(testIsInsideNursery)

BEGIN_TEST(testSemispaceNursery) {
  AutoGCParameter enableSemispace(cx, JSGC_SEMISPACE_NURSERY_ENABLED, 1);

  JS_GC(cx);

  /* Objects are initially allocated in the nursery. */
  RootedObject object(cx, JS_NewPlainObject(cx));
  CHECK(gc::IsInsideNursery(object));

  /* Minor GC with the evict reason tenures them. */
  cx->minorGC(JS::GCReason::EVICT_NURSERY);
  CHECK(!gc::IsInsideNursery(object));

  /* Minor GC with other reasons gives them a second chance. */
  object = JS_NewPlainObject(cx);
  void* ptr = object;
  CHECK(gc::IsInsideNursery(object));
  cx->minorGC(JS::GCReason::API);
  CHECK(ptr != object);
  CHECK(gc::IsInsideNursery(object));
  ptr = object;
  cx->minorGC(JS::GCReason::API);
  CHECK(!gc::IsInsideNursery(object));
  CHECK(ptr != object);

  CHECK(testReferencesBetweenGenerations(0));
  CHECK(testReferencesBetweenGenerations(1));
  CHECK(testReferencesBetweenGenerations(2));

  return true;
}

bool testReferencesBetweenGenerations(size_t referrerGeneration) {
  MOZ_ASSERT(referrerGeneration <= 2);

  RootedObject referrer(cx, JS_NewPlainObject(cx));
  CHECK(referrer);
  CHECK(gc::IsInsideNursery(referrer));

  for (size_t i = 0; i < referrerGeneration; i++) {
    cx->minorGC(JS::GCReason::API);
  }
  MOZ_ASSERT(IsInsideNursery(referrer) == (referrerGeneration < 2));

  RootedObject object(cx, JS_NewPlainObject(cx));
  CHECK(gc::IsInsideNursery(object));
  RootedValue value(cx, JS::ObjectValue(*object));
  CHECK(JS_DefineProperty(cx, referrer, "prop", value, 0));
  CHECK(JS_GetProperty(cx, referrer, "prop", &value));
  CHECK(&value.toObject() == object);
  CHECK(JS_SetElement(cx, referrer, 0, value));
  CHECK(JS_GetElement(cx, referrer, 0, &value));
  CHECK(&value.toObject() == object);

  cx->minorGC(JS::GCReason::API);
  CHECK(gc::IsInsideNursery(object));
  CHECK(JS_GetProperty(cx, referrer, "prop", &value));
  CHECK(&value.toObject() == object);
  CHECK(JS_GetElement(cx, referrer, 0, &value));
  CHECK(&value.toObject() == object);

  cx->minorGC(JS::GCReason::API);
  CHECK(!gc::IsInsideNursery(object));
  CHECK(JS_GetProperty(cx, referrer, "prop", &value));
  CHECK(&value.toObject() == object);
  CHECK(JS_GetElement(cx, referrer, 0, &value));
  CHECK(&value.toObject() == object);

  return true;
}

END_TEST(testSemispaceNursery)
