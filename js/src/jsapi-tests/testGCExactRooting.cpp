/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Maybe.h"

#include "ds/TraceableFifo.h"
#include "gc/Policy.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_GetProperty, JS_SetProperty
#include "js/RootingAPI.h"

#include "jsapi-tests/tests.h"

using namespace js;

using mozilla::Maybe;
using mozilla::Some;

BEGIN_TEST(testGCExactRooting) {
  JS::RootedObject rootCx(cx, JS_NewPlainObject(cx));

  JS_GC(cx);

  /* Use the objects we just created to ensure that they are still alive. */
  JS_DefineProperty(cx, rootCx, "foo", JS::UndefinedHandleValue, 0);

  return true;
}
END_TEST(testGCExactRooting)

BEGIN_TEST(testGCSuppressions) {
  JS::AutoAssertNoGC nogc;
  JS::AutoCheckCannotGC checkgc;
  JS::AutoSuppressGCAnalysis noanalysis;

  JS::AutoAssertNoGC nogcCx(cx);
  JS::AutoCheckCannotGC checkgcCx(cx);
  JS::AutoSuppressGCAnalysis noanalysisCx(cx);

  return true;
}
END_TEST(testGCSuppressions)

struct MyContainer {
  int whichConstructor;
  HeapPtr<JSObject*> obj;
  HeapPtr<JSString*> str;

  MyContainer() : whichConstructor(1), obj(nullptr), str(nullptr) {}
  explicit MyContainer(double) : MyContainer() { whichConstructor = 2; }
  explicit MyContainer(JSContext* cx) : MyContainer() { whichConstructor = 3; }
  MyContainer(JSContext* cx, JSContext* cx2, JSContext* cx3) : MyContainer() {
    whichConstructor = 4;
  }
  MyContainer(const MyContainer& rhs)
      : whichConstructor(100 + rhs.whichConstructor),
        obj(rhs.obj),
        str(rhs.str) {}
  void trace(JSTracer* trc) {
    js::TraceNullableEdge(trc, &obj, "test container obj");
    js::TraceNullableEdge(trc, &str, "test container str");
  }
};

struct MyNonCopyableContainer {
  int whichConstructor;
  HeapPtr<JSObject*> obj;
  HeapPtr<JSString*> str;

  MyNonCopyableContainer() : whichConstructor(1), obj(nullptr), str(nullptr) {}
  explicit MyNonCopyableContainer(double) : MyNonCopyableContainer() {
    whichConstructor = 2;
  }
  explicit MyNonCopyableContainer(JSContext* cx) : MyNonCopyableContainer() {
    whichConstructor = 3;
  }
  explicit MyNonCopyableContainer(JSContext* cx, JSContext* cx2, JSContext* cx3)
      : MyNonCopyableContainer() {
    whichConstructor = 4;
  }

  MyNonCopyableContainer(const MyNonCopyableContainer&) = delete;
  MyNonCopyableContainer& operator=(const MyNonCopyableContainer&) = delete;

  void trace(JSTracer* trc) {
    js::TraceNullableEdge(trc, &obj, "test container obj");
    js::TraceNullableEdge(trc, &str, "test container str");
  }
};

namespace js {
template <typename Wrapper>
struct MutableWrappedPtrOperations<MyContainer, Wrapper> {
  HeapPtr<JSObject*>& obj() { return static_cast<Wrapper*>(this)->get().obj; }
  HeapPtr<JSString*>& str() { return static_cast<Wrapper*>(this)->get().str; }
  int constructor() {
    return static_cast<Wrapper*>(this)->get().whichConstructor;
  }
};

template <typename Wrapper>
struct MutableWrappedPtrOperations<MyNonCopyableContainer, Wrapper> {
  HeapPtr<JSObject*>& obj() { return static_cast<Wrapper*>(this)->get().obj; }
  HeapPtr<JSString*>& str() { return static_cast<Wrapper*>(this)->get().str; }
  int constructor() {
    return static_cast<Wrapper*>(this)->get().whichConstructor;
  }
};
}  // namespace js

BEGIN_TEST(testGCRootedStaticStructInternalStackStorageAugmented) {
  // Test Rooted constructors for a copyable type.
  JS::Rooted<MyContainer> r1(cx);
  JS::Rooted<MyContainer> r2(cx, 3.4);
  JS::Rooted<MyContainer> r3(cx, MyContainer(cx));
  JS::Rooted<MyContainer> r4(cx, cx);
  JS::Rooted<MyContainer> r5(cx, cx, cx, cx);

  JS::Rooted<Value> rv(cx);

  CHECK_EQUAL(r1.constructor(), 1);    // direct SafelyInitialized<T>
  CHECK_EQUAL(r2.constructor(), 2);    // direct MyContainer(3.4)
  CHECK_EQUAL(r3.constructor(), 103);  // copy of MyContainer(cx)
  CHECK_EQUAL(r4.constructor(), 3);    // direct MyContainer(cx)
  CHECK_EQUAL(r5.constructor(), 4);    // direct MyContainer(cx, cx, cx)

  // Test Rooted constructor forwarding for a non-copyable type.
  JS::Rooted<MyNonCopyableContainer> nc1(cx);
  JS::Rooted<MyNonCopyableContainer> nc2(cx, 3.4);
  // Compile error: cannot copy
  // JS::Rooted<MyNonCopyableContainer> nc3(cx, MyNonCopyableContainer(cx));
  JS::Rooted<MyNonCopyableContainer> nc4(cx, cx);
  JS::Rooted<MyNonCopyableContainer> nc5(cx, cx, cx, cx);

  CHECK_EQUAL(nc1.constructor(), 1);  // direct MyNonCopyableContainer()
  CHECK_EQUAL(nc2.constructor(), 2);  // direct MyNonCopyableContainer(3.4)
  CHECK_EQUAL(nc4.constructor(), 3);  // direct MyNonCopyableContainer(cx)
  CHECK_EQUAL(nc5.constructor(),
              4);  // direct MyNonCopyableContainer(cx, cx, cx)

  JS::Rooted<MyContainer> container(cx);
  container.obj() = JS_NewObject(cx, nullptr);
  container.str() = JS_NewStringCopyZ(cx, "Hello");

  JS_GC(cx);
  JS_GC(cx);

  JS::RootedObject obj(cx, container.obj());
  JS::RootedValue val(cx, StringValue(container.str()));
  CHECK(JS_SetProperty(cx, obj, "foo", val));
  obj = nullptr;
  val = UndefinedValue();

  {
    JS::RootedString actual(cx);
    bool same;

    // Automatic move from stack to heap.
    JS::PersistentRooted<MyContainer> heap(cx, container);

    // Copyable types in place.
    JS::PersistentRooted<MyContainer> cp1(cx);
    JS::PersistentRooted<MyContainer> cp2(cx, 7.8);
    JS::PersistentRooted<MyContainer> cp3(cx, cx);
    JS::PersistentRooted<MyContainer> cp4(cx, cx, cx, cx);

    CHECK_EQUAL(cp1.constructor(), 1);  // direct SafelyInitialized<T>
    CHECK_EQUAL(cp2.constructor(), 2);  // direct MyContainer(double)
    CHECK_EQUAL(cp3.constructor(), 3);  // direct MyContainer(cx)
    CHECK_EQUAL(cp4.constructor(), 4);  // direct MyContainer(cx, cx, cx)

    // Construct uncopyable type in place.
    JS::PersistentRooted<MyNonCopyableContainer> ncp1(cx);
    JS::PersistentRooted<MyNonCopyableContainer> ncp2(cx, 7.8);

    // We're not just using a 1-arg constructor, right?
    JS::PersistentRooted<MyNonCopyableContainer> ncp3(cx, cx);
    JS::PersistentRooted<MyNonCopyableContainer> ncp4(cx, cx, cx, cx);

    CHECK_EQUAL(ncp1.constructor(), 1);  // direct SafelyInitialized<T>
    CHECK_EQUAL(ncp2.constructor(), 2);  // direct Ctor(double)
    CHECK_EQUAL(ncp3.constructor(), 3);  // direct Ctor(cx)
    CHECK_EQUAL(ncp4.constructor(), 4);  // direct Ctor(cx, cx, cx)

    // clear prior rooting.
    container.obj() = nullptr;
    container.str() = nullptr;

    obj = heap.obj();
    CHECK(JS_GetProperty(cx, obj, "foo", &val));
    actual = val.toString();
    CHECK(JS_StringEqualsLiteral(cx, actual, "Hello", &same));
    CHECK(same);
    obj = nullptr;
    actual = nullptr;

    JS_GC(cx);
    JS_GC(cx);

    obj = heap.obj();
    CHECK(JS_GetProperty(cx, obj, "foo", &val));
    actual = val.toString();
    CHECK(JS_StringEqualsLiteral(cx, actual, "Hello", &same));
    CHECK(same);
    obj = nullptr;
    actual = nullptr;
  }

  return true;
}
END_TEST(testGCRootedStaticStructInternalStackStorageAugmented)

static JS::PersistentRooted<JSObject*> sLongLived;
BEGIN_TEST(testGCPersistentRootedOutlivesRuntime) {
  sLongLived.init(cx, JS_NewObject(cx, nullptr));
  CHECK(sLongLived);
  return true;
}
END_TEST(testGCPersistentRootedOutlivesRuntime)

// Unlike the above, the following test is an example of an invalid usage: for
// performance and simplicity reasons, PersistentRooted<Traceable> is not
// allowed to outlive the container it belongs to. The following commented out
// test can be used to verify that the relevant assertion fires as expected.
static JS::PersistentRooted<MyContainer> sContainer;
BEGIN_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime) {
  JS::Rooted<MyContainer> container(cx);
  container.obj() = JS_NewObject(cx, nullptr);
  container.str() = JS_NewStringCopyZ(cx, "Hello");
  sContainer.init(cx, container);

  // Commenting the following line will trigger an assertion that the
  // PersistentRooted outlives the runtime it is attached to.
  sContainer.reset();

  return true;
}
END_TEST(testGCPersistentRootedTraceableCannotOutliveRuntime)

using MyHashMap = js::GCHashMap<js::Shape*, JSObject*>;

BEGIN_TEST(testGCRootedHashMap) {
  JS::Rooted<MyHashMap> map(cx, MyHashMap(cx, 15));

  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    CHECK(JS_SetProperty(cx, obj, buffer, val));
    CHECK(map.putNew(obj->shape(), obj));
  }

  JS_GC(cx);
  JS_GC(cx);

  for (auto r = map.all(); !r.empty(); r.popFront()) {
    RootedObject obj(cx, r.front().value());
    CHECK(obj->shape() == r.front().key());
  }

  return true;
}
END_TEST(testGCRootedHashMap)

// Repeat of the test above, but without rooting. This is a rooting hazard. The
// JS_EXPECT_HAZARDS annotation will cause the hazard taskcluster job to fail
// if the hazard below is *not* detected.
BEGIN_TEST_WITH_ATTRIBUTES(testUnrootedGCHashMap, JS_EXPECT_HAZARDS) {
  MyHashMap map(cx, 15);

  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    CHECK(JS_SetProperty(cx, obj, buffer, val));
    CHECK(map.putNew(obj->shape(), obj));
  }

  JS_GC(cx);

  // Access map to keep it live across the GC.
  CHECK(map.count() == 10);

  return true;
}
END_TEST(testUnrootedGCHashMap)

BEGIN_TEST(testSafelyUnrootedGCHashMap) {
  // This is not rooted, but it doesn't use GC pointers as keys or values so
  // it's ok.
  js::GCHashMap<uint64_t, uint64_t> map(cx, 15);

  JS_GC(cx);
  CHECK(map.putNew(12, 13));

  return true;
}
END_TEST(testSafelyUnrootedGCHashMap)

static bool FillMyHashMap(JSContext* cx, MutableHandle<MyHashMap> map) {
  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    if (!JS_SetProperty(cx, obj, buffer, val)) {
      return false;
    }
    if (!map.putNew(obj->shape(), obj)) {
      return false;
    }
  }
  return true;
}

static bool CheckMyHashMap(JSContext* cx, Handle<MyHashMap> map) {
  for (auto r = map.all(); !r.empty(); r.popFront()) {
    RootedObject obj(cx, r.front().value());
    if (obj->shape() != r.front().key()) {
      return false;
    }
  }
  return true;
}

BEGIN_TEST(testGCHandleHashMap) {
  JS::Rooted<MyHashMap> map(cx, MyHashMap(cx, 15));

  CHECK(FillMyHashMap(cx, &map));

  JS_GC(cx);
  JS_GC(cx);

  CHECK(CheckMyHashMap(cx, map));

  return true;
}
END_TEST(testGCHandleHashMap)

using ShapeVec = GCVector<Shape*>;

BEGIN_TEST(testGCRootedVector) {
  JS::Rooted<ShapeVec> shapes(cx, cx);

  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    CHECK(JS_SetProperty(cx, obj, buffer, val));
    CHECK(shapes.append(obj->shape()));
  }

  JS_GC(cx);
  JS_GC(cx);

  for (size_t i = 0; i < 10; ++i) {
    // Check the shape to ensure it did not get collected.
    char letter = 'a' + i;
    bool match;
    ShapePropertyIter<NoGC> iter(shapes[i]);
    CHECK(JS_StringEqualsAscii(cx, JSID_TO_STRING(iter->key()), &letter, 1,
                               &match));
    CHECK(match);
  }

  // Ensure iterator enumeration works through the rooted.
  for (auto shape : shapes) {
    CHECK(shape);
  }

  CHECK(receiveConstRefToShapeVector(shapes));

  // Ensure rooted converts to handles.
  CHECK(receiveHandleToShapeVector(shapes));
  CHECK(receiveMutableHandleToShapeVector(&shapes));

  return true;
}

bool receiveConstRefToShapeVector(const JS::Rooted<GCVector<Shape*>>& rooted) {
  // Ensure range enumeration works through the reference.
  for (auto shape : rooted) {
    CHECK(shape);
  }
  return true;
}

bool receiveHandleToShapeVector(JS::Handle<GCVector<Shape*>> handle) {
  // Ensure range enumeration works through the handle.
  for (auto shape : handle) {
    CHECK(shape);
  }
  return true;
}

bool receiveMutableHandleToShapeVector(
    JS::MutableHandle<GCVector<Shape*>> handle) {
  // Ensure range enumeration works through the handle.
  for (auto shape : handle) {
    CHECK(shape);
  }
  return true;
}
END_TEST(testGCRootedVector)

BEGIN_TEST(testTraceableFifo) {
  using ShapeFifo = TraceableFifo<Shape*>;
  JS::Rooted<ShapeFifo> shapes(cx, ShapeFifo(cx));
  CHECK(shapes.empty());

  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    CHECK(JS_SetProperty(cx, obj, buffer, val));
    CHECK(shapes.pushBack(obj->shape()));
  }

  CHECK(shapes.length() == 10);

  JS_GC(cx);
  JS_GC(cx);

  for (size_t i = 0; i < 10; ++i) {
    // Check the shape to ensure it did not get collected.
    char letter = 'a' + i;
    bool match;
    ShapePropertyIter<NoGC> iter(shapes.front());
    CHECK(JS_StringEqualsAscii(cx, JSID_TO_STRING(iter->key()), &letter, 1,
                               &match));
    CHECK(match);
    shapes.popFront();
  }

  CHECK(shapes.empty());
  return true;
}
END_TEST(testTraceableFifo)

using ShapeVec = GCVector<Shape*>;

static bool FillVector(JSContext* cx, MutableHandle<ShapeVec> shapes) {
  for (size_t i = 0; i < 10; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    RootedValue val(cx, UndefinedValue());
    // Construct a unique property name to ensure that the object creates a
    // new shape.
    char buffer[2];
    buffer[0] = 'a' + i;
    buffer[1] = '\0';
    if (!JS_SetProperty(cx, obj, buffer, val)) {
      return false;
    }
    if (!shapes.append(obj->shape())) {
      return false;
    }
  }

  // Ensure iterator enumeration works through the mutable handle.
  for (auto shape : shapes) {
    if (!shape) {
      return false;
    }
  }

  return true;
}

static bool CheckVector(JSContext* cx, Handle<ShapeVec> shapes) {
  for (size_t i = 0; i < 10; ++i) {
    // Check the shape to ensure it did not get collected.
    char letter = 'a' + i;
    bool match;
    ShapePropertyIter<NoGC> iter(shapes[i]);
    if (!JS_StringEqualsAscii(cx, JSID_TO_STRING(iter->key()), &letter, 1,
                              &match)) {
      return false;
    }
    if (!match) {
      return false;
    }
  }

  // Ensure iterator enumeration works through the handle.
  for (auto shape : shapes) {
    if (!shape) {
      return false;
    }
  }

  return true;
}

BEGIN_TEST(testGCHandleVector) {
  JS::Rooted<ShapeVec> vec(cx, ShapeVec(cx));

  CHECK(FillVector(cx, &vec));

  JS_GC(cx);
  JS_GC(cx);

  CHECK(CheckVector(cx, vec));

  return true;
}
END_TEST(testGCHandleVector)

class Foo {
 public:
  Foo(int, int) {}
  void trace(JSTracer*) {}
};

using FooVector = JS::GCVector<Foo>;

BEGIN_TEST(testGCVectorEmplaceBack) {
  JS::Rooted<FooVector> vector(cx, FooVector(cx));

  CHECK(vector.emplaceBack(1, 2));

  return true;
}
END_TEST(testGCVectorEmplaceBack)

BEGIN_TEST(testRootedMaybeValue) {
  JS::Rooted<Maybe<Value>> maybeNothing(cx);
  CHECK(maybeNothing.isNothing());
  CHECK(!maybeNothing.isSome());

  JS::Rooted<Maybe<Value>> maybe(cx, Some(UndefinedValue()));
  CHECK(CheckConstOperations<Rooted<Maybe<Value>>&>(maybe));
  CHECK(CheckConstOperations<Handle<Maybe<Value>>>(maybe));
  CHECK(CheckConstOperations<MutableHandle<Maybe<Value>>>(&maybe));

  maybe = Some(JS::TrueValue());
  CHECK(CheckMutableOperations<Rooted<Maybe<Value>>&>(maybe));

  maybe = Some(JS::TrueValue());
  CHECK(CheckMutableOperations<MutableHandle<Maybe<Value>>>(&maybe));

  CHECK(JS::NothingHandleValue.isNothing());

  return true;
}

template <typename T>
bool CheckConstOperations(T someUndefinedValue) {
  CHECK(someUndefinedValue.isSome());
  CHECK(someUndefinedValue.value().isUndefined());
  CHECK(someUndefinedValue->isUndefined());
  CHECK((*someUndefinedValue).isUndefined());
  return true;
}

template <typename T>
bool CheckMutableOperations(T maybe) {
  CHECK(maybe->isTrue());

  *maybe = JS::FalseValue();
  CHECK(maybe->isFalse());

  maybe.reset();
  CHECK(maybe.isNothing());

  return true;
}

END_TEST(testRootedMaybeValue)
