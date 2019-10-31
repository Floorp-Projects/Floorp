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
template <typename W>
struct TestStruct {
  W wrapper;
};

// A specialized version for GCPtr that adds a zone() method.
template <typename T>
struct TestStruct<js::GCPtr<T>> {
  js::GCPtr<T> wrapper;

  void trace(JSTracer* trc) {
    TraceNullableEdge(trc, &wrapper, "TestStruct::wrapper");
  }
};

// Give the GCPtr version GCManagedDeletePolicy as required.
namespace JS {
template <typename T>
struct DeletePolicy<TestStruct<js::GCPtr<T>>>
    : public js::GCManagedDeletePolicy<TestStruct<js::GCPtr<T>>> {};
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

  /* Currently JSObject and JSFunction objects are nursery allocated. */
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
  CHECK((TestHeapPostBarriersForWrapper<T, JS::Heap<T*>>()));
  CHECK((TestHeapPostBarriersForWrapper<T, js::GCPtr<T*>>()));
  CHECK((TestHeapPostBarriersForWrapper<T, js::HeapPtr<T*>>()));
  return true;
}

template <typename T, typename W>
bool TestHeapPostBarriersForWrapper() {
  CHECK((TestHeapPostBarrierUpdate<T, W>()));
  CHECK((TestHeapPostBarrierInitFailure<T, W>()));
  return true;
}

template <typename T, typename W>
bool TestHeapPostBarrierUpdate() {
  // Normal case - allocate a heap object, write a nursery pointer into it and
  // check that it gets updated on minor GC.

  T* initialObj = CreateGCThing<T>(cx);
  CHECK(initialObj != nullptr);
  CHECK(js::gc::IsInsideNursery(initialObj));
  uintptr_t initialObjAsInt = uintptr_t(initialObj);

  TestStruct<W>* ptr = nullptr;

  {
    auto testStruct = cx->make_unique<TestStruct<W>>();
    CHECK(testStruct);

    W& wrapper = testStruct->wrapper;
    CHECK(wrapper.get() == nullptr);
    wrapper = initialObj;
    CHECK(wrapper == initialObj);

    ptr = testStruct.release();
  }

  cx->minorGC(JS::GCReason::API);

  W& wrapper = ptr->wrapper;
  CHECK(uintptr_t(wrapper.get()) != initialObjAsInt);
  CHECK(!js::gc::IsInsideNursery(wrapper.get()));
  CHECK(CanAccessObject(wrapper.get()));

  JS::DeletePolicy<TestStruct<W>>()(ptr);

  cx->minorGC(JS::GCReason::API);

  return true;
}

template <typename T, typename W>
bool TestHeapPostBarrierInitFailure() {
  // Failure case - allocate a heap object, write a nursery pointer into it
  // and fail to complete initialization.

  T* initialObj = CreateGCThing<T>(cx);
  CHECK(initialObj != nullptr);
  CHECK(js::gc::IsInsideNursery(initialObj));

  {
    auto testStruct = cx->make_unique<TestStruct<W>>();
    CHECK(testStruct);

    W& wrapper = testStruct->wrapper;
    CHECK(wrapper.get() == nullptr);
    wrapper = initialObj;
    CHECK(wrapper == initialObj);

    // testStruct deleted here, as if we left this block due to an error.
  }

  cx->minorGC(JS::GCReason::API);

  return true;
}

END_TEST(testGCHeapPostBarriers)

BEGIN_TEST(testUnbarrieredEquality) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  // Use ArrayBuffers because they have finalizers, which allows using them in
  // TenuredHeap<> without awkward conversations about nursery allocatability.
  JS::RootedObject robj(cx, JS::NewArrayBuffer(cx, 20));
  JS::RootedObject robj2(cx, JS::NewArrayBuffer(cx, 30));
  cx->runtime()->gc.evictNursery();  // Need tenured objects

  // Need some bare pointers to compare against.
  JSObject* obj = robj;
  JSObject* obj2 = robj2;
  const JSObject* constobj = robj;
  const JSObject* constobj2 = robj2;

  // Make them gray. We will make sure they stay gray. (For most reads, the
  // barrier will unmark gray.)
  using namespace js::gc;
  TenuredCell* cell = &obj->asTenured();
  TenuredCell* cell2 = &obj2->asTenured();
  cell->markIfUnmarked(MarkColor::Gray);
  cell2->markIfUnmarked(MarkColor::Gray);
  MOZ_ASSERT(cell->isMarkedGray());
  MOZ_ASSERT(cell2->isMarkedGray());

  {
    JS::Heap<JSObject*> heap(obj);
    JS::Heap<JSObject*> heap2(obj2);
    CHECK(TestWrapper(obj, obj2, heap, heap2));
    CHECK(TestWrapper(constobj, constobj2, heap, heap2));
  }

  {
    JS::TenuredHeap<JSObject*> heap(obj);
    JS::TenuredHeap<JSObject*> heap2(obj2);
    CHECK(TestWrapper(obj, obj2, heap, heap2));
    CHECK(TestWrapper(constobj, constobj2, heap, heap2));
  }

  // Sanity check that the barriers normally mark things black.
  {
    JS::Heap<JSObject*> heap(obj);
    JS::Heap<JSObject*> heap2(obj2);
    heap.get();
    heap2.get();
    CHECK(cell->isMarkedBlack());
    CHECK(cell2->isMarkedBlack());
  }

  return true;
}

template <typename ObjectT, typename WrapperT>
bool TestWrapper(ObjectT obj, ObjectT obj2, WrapperT& wrapper,
                 WrapperT& wrapper2) {
  using namespace js::gc;

  const TenuredCell& cell = obj->asTenured();
  const TenuredCell& cell2 = obj2->asTenured();

  int x = 0;

  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += obj == obj2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += obj == wrapper2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += wrapper == obj2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += wrapper == wrapper2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());

  CHECK(x == 0);

  x += obj != obj2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += obj != wrapper2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += wrapper != obj2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());
  x += wrapper != wrapper2;
  CHECK(cell.isMarkedGray());
  CHECK(cell2.isMarkedGray());

  CHECK(x == 4);

  return true;
}

END_TEST(testUnbarrieredEquality)
