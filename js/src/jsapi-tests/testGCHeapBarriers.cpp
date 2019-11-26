/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

#include "js/ArrayBuffer.h"  // JS::NewArrayBuffer
#include "js/RootingAPI.h"
#include "jsapi-tests/tests.h"
#include "vm/Runtime.h"

#include "vm/JSContext-inl.h"

// A heap-allocated structure containing one of our barriered pointer wrappers
// to test.
template <typename W, typename T>
struct TestStruct {
  W wrapper;

  void trace(JSTracer* trc) {
    TraceNullableEdge(trc, &wrapper, "TestStruct::wrapper");
  }

  TestStruct() {}
  explicit TestStruct(T init) : wrapper(init) {}
};

// Give the GCPtr version GCManagedDeletePolicy as required.
namespace JS {

template <typename T>
struct DeletePolicy<TestStruct<js::GCPtr<T>, T>>
    : public js::GCManagedDeletePolicy<TestStruct<js::GCPtr<T>, T>> {};

template <typename T>
struct DeletePolicy<TestStruct<const js::GCPtr<T>, T>>
    : public js::GCManagedDeletePolicy<TestStruct<const js::GCPtr<T>, T>> {};

}  // namespace JS

template <typename T>
static T* CreateGCThing(JSContext* cx) {
  MOZ_CRASH();
  return nullptr;
}

template <>
JSObject* CreateGCThing(JSContext* cx) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return nullptr;
  }
  JS_DefineProperty(cx, obj, "x", 42, 0);
  return obj;
}

template <>
JSFunction* CreateGCThing(JSContext* cx) {
  /*
   * We don't actually use the function as a function, so here we cheat and
   * cast a JSObject.
   */
  return static_cast<JSFunction*>(CreateGCThing<JSObject>(cx));
}

BEGIN_TEST(testGCHeapPostBarriers) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  /* Sanity check - objects start in the nursery and then become tenured. */
  JS_GC(cx);
  JS::RootedObject obj(cx, CreateGCThing<JSObject>(cx));
  CHECK(js::gc::IsInsideNursery(obj.get()));
  JS_GC(cx);
  CHECK(!js::gc::IsInsideNursery(obj.get()));
  JS::RootedObject tenuredObject(cx, obj);

  /* JSObject and JSFunction objects are nursery allocated. */
  CHECK(TestHeapPostBarriersForType<JSObject>());
  CHECK(TestHeapPostBarriersForType<JSFunction>());

  return true;
}

bool CanAccessObject(JSObject* obj) {
  JS::RootedObject rootedObj(cx, obj);
  JS::RootedValue value(cx);
  CHECK(JS_GetProperty(cx, rootedObj, "x", &value));
  CHECK(value.isInt32());
  CHECK(value.toInt32() == 42);
  return true;
}

template <typename T>
bool TestHeapPostBarriersForType() {
  CHECK((TestHeapPostBarriersForWrapper<JS::Heap, T>()));
  CHECK((TestHeapPostBarriersForWrapper<js::GCPtr, T>()));
  CHECK((TestHeapPostBarriersForWrapper<js::HeapPtr, T>()));
  return true;
}

template <template <typename> class W, typename T>
bool TestHeapPostBarriersForWrapper() {
  CHECK((TestHeapPostBarrierConstruction<W<T*>, T>()));
  CHECK((TestHeapPostBarrierConstruction<const W<T*>, T>()));
  CHECK((TestHeapPostBarrierUpdate<W<T*>, T>()));
  CHECK((TestHeapPostBarrierInitFailure<W<T*>, T>()));
  CHECK((TestHeapPostBarrierInitFailure<const W<T*>, T>()));
  return true;
}

template <typename W, typename T>
bool TestHeapPostBarrierConstruction() {
  T* initialObj = CreateGCThing<T>(cx);
  CHECK(initialObj != nullptr);
  CHECK(js::gc::IsInsideNursery(initialObj));
  uintptr_t initialObjAsInt = uintptr_t(initialObj);

  {
    // We don't root our structure because that would end up tracing it on minor
    // GC and we're testing that heap post barrier works for things that aren't
    // roots.
    JS::AutoSuppressGCAnalysis noAnalysis(cx);

    auto testStruct = cx->make_unique<TestStruct<W, T*>>(initialObj);
    CHECK(testStruct);

    auto& wrapper = testStruct->wrapper;
    CHECK(wrapper == initialObj);

    cx->minorGC(JS::GCReason::API);

    CHECK(uintptr_t(wrapper.get()) != initialObjAsInt);
    CHECK(!js::gc::IsInsideNursery(wrapper.get()));
    CHECK(CanAccessObject(wrapper.get()));
  }

  cx->minorGC(JS::GCReason::API);

  return true;
}

template <typename W, typename T>
bool TestHeapPostBarrierUpdate() {
  // Normal case - allocate a heap object, write a nursery pointer into it and
  // check that it gets updated on minor GC.

  T* initialObj = CreateGCThing<T>(cx);
  CHECK(initialObj != nullptr);
  CHECK(js::gc::IsInsideNursery(initialObj));
  uintptr_t initialObjAsInt = uintptr_t(initialObj);

  {
    // We don't root our structure because that would end up tracing it on minor
    // GC and we're testing that heap post barrier works for things that aren't
    // roots.
    JS::AutoSuppressGCAnalysis noAnalysis(cx);

    auto testStruct = cx->make_unique<TestStruct<W, T*>>();
    CHECK(testStruct);

    auto& wrapper = testStruct->wrapper;
    CHECK(wrapper.get() == nullptr);

    wrapper = initialObj;
    CHECK(wrapper == initialObj);

    cx->minorGC(JS::GCReason::API);

    CHECK(uintptr_t(wrapper.get()) != initialObjAsInt);
    CHECK(!js::gc::IsInsideNursery(wrapper.get()));
    CHECK(CanAccessObject(wrapper.get()));
  }

  cx->minorGC(JS::GCReason::API);

  return true;
}

template <typename W, typename T>
bool TestHeapPostBarrierInitFailure() {
  // Failure case - allocate a heap object, write a nursery pointer into it
  // and fail to complete initialization.

  T* initialObj = CreateGCThing<T>(cx);
  CHECK(initialObj != nullptr);
  CHECK(js::gc::IsInsideNursery(initialObj));

  {
    // We don't root our structure because that would end up tracing it on minor
    // GC and we're testing that heap post barrier works for things that aren't
    // roots.
    JS::AutoSuppressGCAnalysis noAnalysis(cx);

    auto testStruct = cx->make_unique<TestStruct<W, T*>>(initialObj);
    CHECK(testStruct);

    auto& wrapper = testStruct->wrapper;
    CHECK(wrapper == initialObj);

    // testStruct deleted here, as if we left this block due to an error.
  }

  cx->minorGC(JS::GCReason::API);

  return true;
}

END_TEST(testGCHeapPostBarriers)

// Check that equality comparisons on external heap wrapper types do not trigger
// the read barrier. This applies to Heap<T> and TenuredHeap<T>; internal
// wrappers types do not have this barrier.
BEGIN_TEST(testGCHeapBarrierComparisons) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  // Use ArrayBuffers because they have finalizers, which allows using them in
  // TenuredHeap<> without awkward conversations about nursery allocatability.
  JS::RootedObject obj(cx, JS::NewArrayBuffer(cx, 20));
  JS::RootedObject obj2(cx, JS::NewArrayBuffer(cx, 30));
  cx->runtime()->gc.evictNursery();  // Need tenured objects

  // Make them gray. We will make sure they stay gray. (For most reads, the
  // barrier will unmark gray.)
  using namespace js::gc;
  TenuredCell* cell = &obj->asTenured();
  TenuredCell* cell2 = &obj2->asTenured();
  cell->markIfUnmarked(MarkColor::Gray);
  cell2->markIfUnmarked(MarkColor::Gray);
  MOZ_ASSERT(cell->isMarkedGray());
  MOZ_ASSERT(cell2->isMarkedGray());

  CHECK((TestWrapperType<JS::Heap<JSObject*>, JSObject*>(obj, obj2)));
  CHECK((TestWrapperType<JS::TenuredHeap<JSObject*>, JSObject*>(obj, obj2)));

  // Sanity check that the read barrier normally marks gray things black.
  {
    JS::Heap<JSObject*> heap(obj);
    JS::TenuredHeap<JSObject*> heap2(obj2);
    heap.get();
    heap2.getPtr();
    CHECK(cell->isMarkedBlack());
    CHECK(cell2->isMarkedBlack());
  }

  return true;
}

template <typename WrapperT, typename ObjectT>
bool TestWrapperType(ObjectT obj, ObjectT obj2) {
  WrapperT wrapper(obj);
  WrapperT wrapper2(obj2);
  const ObjectT constobj = obj;
  const ObjectT constobj2 = obj2;
  CHECK(TestWrapper(obj, obj2, wrapper, wrapper2));
  CHECK(TestWrapper(constobj, constobj2, wrapper, wrapper2));
  return true;
}

template <typename WrapperT, typename ObjectT>
bool TestWrapper(ObjectT obj, ObjectT obj2, WrapperT& wrapper,
                 WrapperT& wrapper2) {
  int x = 0;

  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += obj == obj2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += obj == wrapper2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += wrapper == obj2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += wrapper == wrapper2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());

  CHECK(x == 0);

  x += obj != obj2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += obj != wrapper2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += wrapper != obj2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());
  x += wrapper != wrapper2;
  CHECK(obj->isMarkedGray());
  CHECK(obj2->isMarkedGray());

  CHECK(x == 4);

  return true;
}

END_TEST(testGCHeapBarrierComparisons)
