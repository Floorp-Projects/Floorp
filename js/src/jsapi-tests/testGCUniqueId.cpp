/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/GCVector.h"

#include "jsapi-tests/tests.h"

#include "gc/Zone-inl.h"

static void MinimizeHeap(JSContext* cx) {
  // The second collection is to force us to wait for the background
  // sweeping that the first GC started to finish.
  JS_GC(cx);
  JS_GC(cx);
  js::gc::FinishGC(cx);
}

BEGIN_TEST(testGCUID) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  uint64_t uid = 0;
  uint64_t tmp = 0;

  // Ensure the heap is as minimal as it can get.
  MinimizeHeap(cx);

  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  uintptr_t nurseryAddr = uintptr_t(obj.get());
  CHECK(obj);
  CHECK(js::gc::IsInsideNursery(obj));

  // Do not start with an ID.
  CHECK(!obj->zone()->hasUniqueId(obj));

  // Ensure we can get a new UID.
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &uid));
  CHECK(uid > js::gc::LargestTaggedNullCellPointer);

  // We should now have an id.
  CHECK(obj->zone()->hasUniqueId(obj));

  // Calling again should get us the same thing.
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &tmp));
  CHECK(uid == tmp);

  // Tenure the thing and check that the UID moved with it.
  MinimizeHeap(cx);
  uintptr_t tenuredAddr = uintptr_t(obj.get());
  CHECK(tenuredAddr != nurseryAddr);
  CHECK(!js::gc::IsInsideNursery(obj));
  CHECK(obj->zone()->hasUniqueId(obj));
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &tmp));
  CHECK(uid == tmp);

  // Allocate a new nursery thing in the same location and check that we
  // removed the prior uid that was attached to the location.
  obj = JS_NewPlainObject(cx);
  CHECK(obj);
  CHECK(uintptr_t(obj.get()) == nurseryAddr);
  CHECK(!obj->zone()->hasUniqueId(obj));

  // Try to get another tenured object in the same location and check that
  // the uid was removed correctly.
  obj = nullptr;
  MinimizeHeap(cx);
  obj = JS_NewPlainObject(cx);
  MinimizeHeap(cx);
  CHECK(uintptr_t(obj.get()) == tenuredAddr);
  CHECK(!obj->zone()->hasUniqueId(obj));
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &tmp));
  CHECK(uid != tmp);
  uid = tmp;

  // Allocate a few arenas worth of objects to ensure we get some compaction.
  const static size_t N = 2049;
  using ObjectVector = JS::GCVector<JSObject*>;
  JS::Rooted<ObjectVector> vec(cx, ObjectVector(cx));
  for (size_t i = 0; i < N; ++i) {
    obj = JS_NewPlainObject(cx);
    CHECK(obj);
    CHECK(vec.append(obj));
  }

  // Transfer our vector to tenured if it isn't there already.
  MinimizeHeap(cx);

  // Tear holes in the heap by unrooting the even objects and collecting.
  JS::Rooted<ObjectVector> vec2(cx, ObjectVector(cx));
  for (size_t i = 0; i < N; ++i) {
    if (i % 2 == 1) {
      CHECK(vec2.append(vec[i]));
    }
  }
  vec.clear();

  // Grab the last object in the vector as our object of interest.
  obj = vec2.back();
  CHECK(obj);
  CHECK(!js::gc::IsInsideNursery(obj));
  tenuredAddr = uintptr_t(obj.get());
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &uid));

  // Force a compaction to move the object and check that the uid moved to
  // the new tenured heap location.
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);

  // There's a very low probability that this check could fail, but it is
  // possible.  If it becomes an annoying intermittent then we should make
  // this test more robust by recording IDs of many objects and then checking
  // that some have moved.
  CHECK(uintptr_t(obj.get()) != tenuredAddr);
  CHECK(obj->zone()->hasUniqueId(obj));
  CHECK(obj->zone()->getOrCreateUniqueId(obj, &tmp));
  CHECK(uid == tmp);

  return true;
}
END_TEST(testGCUID)
