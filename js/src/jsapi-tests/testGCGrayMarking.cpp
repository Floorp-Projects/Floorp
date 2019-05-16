/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Heap.h"
#include "gc/Verifier.h"
#include "gc/WeakMap.h"
#include "gc/Zone.h"
#include "js/Proxy.h"
#include "jsapi-tests/tests.h"

using namespace js;
using namespace js::gc;

namespace js {

struct GCManagedObjectWeakMap : public ObjectWeakMap {
  using ObjectWeakMap::ObjectWeakMap;
};

}  // namespace js

namespace JS {

template <>
struct DeletePolicy<js::GCManagedObjectWeakMap>
    : public js::GCManagedDeletePolicy<js::GCManagedObjectWeakMap> {};

template <>
struct MapTypeToRootKind<js::GCManagedObjectWeakMap*> {
  static const JS::RootKind kind = JS::RootKind::Traceable;
};

template <>
struct GCPolicy<js::GCManagedObjectWeakMap*>
    : public NonGCPointerPolicy<js::GCManagedObjectWeakMap*> {};

}  // namespace JS

class AutoNoAnalysisForTest {
 public:
  AutoNoAnalysisForTest() {}
} JS_HAZ_GC_SUPPRESSED;

BEGIN_TEST(testGCGrayMarking) {
  AutoNoAnalysisForTest disableAnalysis;
  AutoDisableCompactingGC disableCompactingGC(cx);
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  CHECK(InitGlobals());
  JSAutoRealm ar(cx, global1);

  InitGrayRootTracer();

  // Enable incremental GC.
  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE_INCREMENTAL);

  bool ok = TestMarking() && TestJSWeakMaps() && TestInternalWeakMaps() &&
            TestCCWs() && TestGrayUnmarking();

  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_GLOBAL);

  global1 = nullptr;
  global2 = nullptr;
  RemoveGrayRootTracer();

  return ok;
}

bool TestMarking() {
  JSObject* sameTarget = AllocPlainObject();
  CHECK(sameTarget);

  JSObject* sameSource = AllocSameCompartmentSourceObject(sameTarget);
  CHECK(sameSource);

  JSObject* crossTarget = AllocPlainObject();
  CHECK(crossTarget);

  JSObject* crossSource = GetCrossCompartmentWrapper(crossTarget);
  CHECK(crossSource);

  // Test GC with black roots marks objects black.

  JS::RootedObject blackRoot1(cx, sameSource);
  JS::RootedObject blackRoot2(cx, crossSource);

  JS_GC(cx);

  CHECK(IsMarkedBlack(sameSource));
  CHECK(IsMarkedBlack(crossSource));
  CHECK(IsMarkedBlack(sameTarget));
  CHECK(IsMarkedBlack(crossTarget));

  // Test GC with black and gray roots marks objects black.

  grayRoots.grayRoot1 = sameSource;
  grayRoots.grayRoot2 = crossSource;

  JS_GC(cx);

  CHECK(IsMarkedBlack(sameSource));
  CHECK(IsMarkedBlack(crossSource));
  CHECK(IsMarkedBlack(sameTarget));
  CHECK(IsMarkedBlack(crossTarget));

  CHECK(!JS::ObjectIsMarkedGray(sameSource));

  // Test GC with gray roots marks object gray.

  blackRoot1 = nullptr;
  blackRoot2 = nullptr;

  JS_GC(cx);

  CHECK(IsMarkedGray(sameSource));
  CHECK(IsMarkedGray(crossSource));
  CHECK(IsMarkedGray(sameTarget));
  CHECK(IsMarkedGray(crossTarget));

  CHECK(JS::ObjectIsMarkedGray(sameSource));

  // Test ExposeToActiveJS marks gray objects black.

  JS::ExposeObjectToActiveJS(sameSource);
  JS::ExposeObjectToActiveJS(crossSource);
  CHECK(IsMarkedBlack(sameSource));
  CHECK(IsMarkedBlack(crossSource));
  CHECK(IsMarkedBlack(sameTarget));
  CHECK(IsMarkedBlack(crossTarget));

  // Test a zone GC with black roots marks gray object in other zone black.

  JS_GC(cx);

  CHECK(IsMarkedGray(crossSource));
  CHECK(IsMarkedGray(crossTarget));

  blackRoot1 = crossSource;
  CHECK(ZoneGC(crossSource->zone()));

  CHECK(IsMarkedBlack(crossSource));
  CHECK(IsMarkedBlack(crossTarget));

  blackRoot1 = nullptr;
  blackRoot2 = nullptr;
  grayRoots.grayRoot1 = nullptr;
  grayRoots.grayRoot2 = nullptr;

  return true;
}

static const CellColor DontMark = CellColor::White;

enum MarkKeyOrDelegate : bool { MarkKey = true, MarkDelegate = false };

bool TestJSWeakMaps() {
  for (auto keyOrDelegateColor : MarkedCellColors) {
    for (auto mapColor : MarkedCellColors) {
      for (auto markKeyOrDelegate : { MarkKey, MarkDelegate }) {
        CellColor expected = ExpectedWeakMapValueColor(keyOrDelegateColor,
                                                       mapColor);
        CHECK(TestJSWeakMap(markKeyOrDelegate, keyOrDelegateColor, mapColor,
                            expected));
#ifdef JS_GC_ZEAL
        CHECK(TestJSWeakMapWithGrayUnmarking(markKeyOrDelegate,
                                             keyOrDelegateColor, mapColor,
                                             expected));
#endif
      }
    }
  }

  return true;
}

bool TestInternalWeakMaps() {
  for (auto keyMarkColor : AllCellColors) {
    for (auto delegateMarkColor : AllCellColors) {
      if (keyMarkColor == CellColor::White &&
          delegateMarkColor == CellColor::White) {
        continue;
      }

      CellColor keyOrDelegateColor =
          ExpectedKeyAndDelegateColor(keyMarkColor, delegateMarkColor);
      CellColor expected = ExpectedWeakMapValueColor(keyOrDelegateColor,
                                                     CellColor::Black);
      CHECK(TestInternalWeakMap(keyMarkColor, delegateMarkColor, expected));

#ifdef JS_GC_ZEAL
      CHECK(TestInternalWeakMapWithGrayUnmarking(keyMarkColor,
                                                 delegateMarkColor,
                                                 expected));
#endif
    }
  }

  return true;
}

bool TestJSWeakMap(MarkKeyOrDelegate markKey, CellColor weakMapMarkColor,
                   CellColor keyOrDelegateMarkColor,
                   CellColor expectedValueColor) {
  // Test marking a JS WeakMap object.
  //
  // This marks the map and one of the key or delegate. The key/delegate and the
  // value can end up different colors depending on the color of the map.

  JSObject* weakMap;
  JSObject* key;
  JSObject* value;

  // If both map and key are marked the same color, test both possible
  // orderings.
  unsigned markOrderings = weakMapMarkColor == keyOrDelegateMarkColor ? 2 : 1;

  for (unsigned markOrder = 0; markOrder < markOrderings; markOrder++) {
    CHECK(CreateJSWeakMapObjects(&weakMap, &key, &value));

    JSObject* delegate = UncheckedUnwrapWithoutExpose(key);
    JSObject* keyOrDelegate = markKey ? key : delegate;

    RootedObject blackRoot1(cx);
    RootedObject blackRoot2(cx);

    RootObject(weakMap, weakMapMarkColor, blackRoot1, grayRoots.grayRoot1);
    RootObject(keyOrDelegate, keyOrDelegateMarkColor, blackRoot2,
               grayRoots.grayRoot2);

    if (markOrder != 0) {
      mozilla::Swap(blackRoot1.get(), blackRoot2.get());
      mozilla::Swap(grayRoots.grayRoot1, grayRoots.grayRoot2);
    }

    JS_GC(cx);

    ClearGrayRoots();

    CHECK(GetCellColor(weakMap) == weakMapMarkColor);
    CHECK(GetCellColor(keyOrDelegate) == keyOrDelegateMarkColor);
    CHECK(GetCellColor(value) == expectedValueColor);
  }

  return true;
}

#ifdef JS_GC_ZEAL

bool TestJSWeakMapWithGrayUnmarking(MarkKeyOrDelegate markKey,
                                    CellColor weakMapMarkColor,
                                    CellColor keyOrDelegateMarkColor,
                                    CellColor expectedValueColor) {
  // This is like the previous test, but things are marked black by gray
  // unmarking during incremental GC.

  JSObject* weakMap;
  JSObject* key;
  JSObject* value;

  // If both map and key are marked the same color, test both possible
  // orderings.
  unsigned markOrderings = weakMapMarkColor == keyOrDelegateMarkColor ? 2 : 1;

  JS_SetGCZeal(cx, uint8_t(ZealMode::YieldWhileGrayMarking), 0);

  for (unsigned markOrder = 0; markOrder < markOrderings; markOrder++) {
    CHECK(CreateJSWeakMapObjects(&weakMap, &key, &value));

    JSObject* delegate = UncheckedUnwrapWithoutExpose(key);
    JSObject* keyOrDelegate = markKey ? key : delegate;

    grayRoots.grayRoot1 = keyOrDelegate;
    grayRoots.grayRoot2 = weakMap;

    // Start an incremental GC and run until gray roots have been pushed onto
    // the mark stack.
    JS::PrepareForFullGC(cx);
    JS::StartIncrementalGC(cx, GC_NORMAL, JS::GCReason::DEBUG_GC, 1000000);
    MOZ_ASSERT(cx->runtime()->gc.state() == gc::State::Sweep);
    MOZ_ASSERT(cx->zone()->gcState() == Zone::MarkBlackAndGray);

    // Unmark gray things as specified.
    if (markOrder != 0) {
      MaybeExposeObject(weakMap, weakMapMarkColor);
      MaybeExposeObject(keyOrDelegate, keyOrDelegateMarkColor);
    } else {
      MaybeExposeObject(keyOrDelegate, keyOrDelegateMarkColor);
      MaybeExposeObject(weakMap, weakMapMarkColor);
    }

    JS::FinishIncrementalGC(cx, JS::GCReason::API);

    ClearGrayRoots();

    CHECK(GetCellColor(weakMap) == weakMapMarkColor);
    CHECK(GetCellColor(keyOrDelegate) == keyOrDelegateMarkColor);
    CHECK(GetCellColor(value) == expectedValueColor);
  }

  JS_UnsetGCZeal(cx, uint8_t(ZealMode::YieldWhileGrayMarking));

  return true;
}

static void MaybeExposeObject(JSObject* object, CellColor color) {
  if (color == CellColor::Black) {
      JS::ExposeObjectToActiveJS(object);
  }
}

#endif // JS_GC_ZEAL

bool CreateJSWeakMapObjects(JSObject** weakMapOut, JSObject** keyOut,
                            JSObject** valueOut) {
  RootedObject key(cx, AllocWeakmapKeyObject());
  CHECK(key);

  RootedObject value(cx, AllocPlainObject());
  CHECK(value);

  RootedObject weakMap(cx, JS::NewWeakMapObject(cx));
  CHECK(weakMap);

  JS::RootedValue valueValue(cx, ObjectValue(*value));
  CHECK(SetWeakMapEntry(cx, weakMap, key, valueValue));

  *weakMapOut = weakMap;
  *keyOut = key;
  *valueOut = value;
  return true;
}

bool TestInternalWeakMap(CellColor keyMarkColor, CellColor delegateMarkColor,
                         CellColor expectedColor) {
  // Test marking for internal weakmaps (without an owning JSObject).
  //
  // All of the key, delegate and value are expected to end up the same color.

  UniquePtr<GCManagedObjectWeakMap> weakMap;
  JSObject* key;
  JSObject* value;

  // If both key and delegate are marked the same color, test both possible
  // orderings.
  unsigned markOrderings = keyMarkColor == delegateMarkColor ? 2 : 1;

  for (unsigned markOrder = 0; markOrder < markOrderings; markOrder++) {
    CHECK(CreateInternalWeakMapObjects(&weakMap, &key, &value));

    JSObject* delegate = UncheckedUnwrapWithoutExpose(key);

    RootedObject blackRoot1(cx);
    RootedObject blackRoot2(cx);

    Rooted<GCManagedObjectWeakMap*> rootMap(cx, weakMap.get());
    RootObject(key, keyMarkColor, blackRoot1, grayRoots.grayRoot1);
    RootObject(delegate, delegateMarkColor, blackRoot2, grayRoots.grayRoot2);

    if (markOrder != 0) {
      mozilla::Swap(blackRoot1.get(), blackRoot2.get());
      mozilla::Swap(grayRoots.grayRoot1, grayRoots.grayRoot2);
    }

    JS_GC(cx);

    ClearGrayRoots();

    CHECK(GetCellColor(key) == expectedColor);
    CHECK(GetCellColor(delegate) == expectedColor);
    CHECK(GetCellColor(value) == expectedColor);
  }

  return true;
}

#ifdef JS_GC_ZEAL

bool TestInternalWeakMapWithGrayUnmarking(CellColor keyMarkColor,
                                          CellColor delegateMarkColor,
                                          CellColor expectedColor) {
  UniquePtr<GCManagedObjectWeakMap> weakMap;
  JSObject* key;
  JSObject* value;

  // If both key and delegate are marked the same color, test both possible
  // orderings.
  unsigned markOrderings = keyMarkColor == delegateMarkColor ? 2 : 1;

  JS_SetGCZeal(cx, uint8_t(ZealMode::YieldWhileGrayMarking), 0);

  for (unsigned markOrder = 0; markOrder < markOrderings; markOrder++) {
    CHECK(CreateInternalWeakMapObjects(&weakMap, &key, &value));

    JSObject* delegate = UncheckedUnwrapWithoutExpose(key);

    Rooted<GCManagedObjectWeakMap*> rootMap(cx, weakMap.get());
    grayRoots.grayRoot1 = key;
    grayRoots.grayRoot2 = delegate;

    // Start an incremental GC and run until gray roots have been pushed onto
    // the mark stack.
    JS::PrepareForFullGC(cx);
    JS::StartIncrementalGC(cx, GC_NORMAL, JS::GCReason::DEBUG_GC, 1000000);
    MOZ_ASSERT(cx->runtime()->gc.state() == gc::State::Sweep);
    MOZ_ASSERT(cx->zone()->gcState() == Zone::MarkBlackAndGray);

    // Unmark gray things as specified.
    if (markOrder != 0) {
      MaybeExposeObject(key, keyMarkColor);
      MaybeExposeObject(delegate, delegateMarkColor);
    } else {
      MaybeExposeObject(key, keyMarkColor);
      MaybeExposeObject(delegate, delegateMarkColor);
    }

    JS::FinishIncrementalGC(cx, JS::GCReason::API);

    ClearGrayRoots();

    CHECK(GetCellColor(key) == expectedColor);
    CHECK(GetCellColor(delegate) == expectedColor);
    CHECK(GetCellColor(value) == expectedColor);
  }

  JS_UnsetGCZeal(cx, uint8_t(ZealMode::YieldWhileGrayMarking));

  return true;
}

#endif // JS_GC_ZEAL

bool CreateInternalWeakMapObjects(UniquePtr<GCManagedObjectWeakMap>* weakMapOut,
                                  JSObject** keyOut, JSObject** valueOut) {
  RootedObject key(cx, AllocWeakmapKeyObject());
  CHECK(key);

  RootedObject value(cx, AllocPlainObject());
  CHECK(value);

  auto weakMap = cx->make_unique<GCManagedObjectWeakMap>(cx);
  CHECK(weakMap);

  CHECK(weakMap->add(cx, key, value));

  *weakMapOut = std::move(weakMap);
  *keyOut = key;
  *valueOut = value;
  return true;
}

void RootObject(JSObject* object, CellColor color, RootedObject& blackRoot,
                JS::Heap<JSObject*>& grayRoot) {
  if (color == CellColor::Black) {
    blackRoot = object;
  } else if (color == CellColor::Gray) {
    grayRoot = object;
  } else {
    MOZ_RELEASE_ASSERT(color == CellColor::White);
  }
}

bool TestCCWs() {
  JSObject* target = AllocPlainObject();
  CHECK(target);

  // Test getting a new wrapper doesn't return a gray wrapper.

  RootedObject blackRoot(cx, target);
  JSObject* wrapper = GetCrossCompartmentWrapper(target);
  CHECK(wrapper);
  CHECK(!IsMarkedGray(wrapper));

  // Test getting an existing wrapper doesn't return a gray wrapper.

  grayRoots.grayRoot1 = wrapper;
  grayRoots.grayRoot2 = nullptr;
  JS_GC(cx);
  CHECK(IsMarkedGray(wrapper));
  CHECK(IsMarkedBlack(target));

  CHECK(GetCrossCompartmentWrapper(target) == wrapper);
  CHECK(!IsMarkedGray(wrapper));

  // Test getting an existing wrapper doesn't return a gray wrapper
  // during incremental GC.

  JS_GC(cx);
  CHECK(IsMarkedGray(wrapper));
  CHECK(IsMarkedBlack(target));

  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE_INCREMENTAL);
  JS::PrepareForFullGC(cx);
  js::SliceBudget budget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(GC_NORMAL, budget);
  CHECK(JS::IsIncrementalGCInProgress(cx));

  CHECK(!IsMarkedBlack(wrapper));
  CHECK(wrapper->zone()->isGCMarkingBlackOnly());

  CHECK(GetCrossCompartmentWrapper(target) == wrapper);
  CHECK(IsMarkedBlack(wrapper));

  JS::FinishIncrementalGC(cx, JS::GCReason::API);

  // Test behaviour of gray CCWs marked black by a barrier during incremental
  // GC.

  // Initial state: source and target are gray.
  blackRoot = nullptr;
  grayRoots.grayRoot1 = wrapper;
  grayRoots.grayRoot2 = nullptr;
  JS_GC(cx);
  CHECK(IsMarkedGray(wrapper));
  CHECK(IsMarkedGray(target));

  // Incremental zone GC started: the source is now unmarked.
  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE_INCREMENTAL);
  JS::PrepareZoneForGC(wrapper->zone());
  budget = js::SliceBudget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(GC_NORMAL, budget);
  CHECK(JS::IsIncrementalGCInProgress(cx));
  CHECK(wrapper->zone()->isGCMarkingBlackOnly());
  CHECK(!target->zone()->wasGCStarted());
  CHECK(!IsMarkedBlack(wrapper));
  CHECK(!IsMarkedGray(wrapper));
  CHECK(IsMarkedGray(target));

  // Betweeen GC slices: source marked black by barrier, target is
  // still gray. Target will be marked gray
  // eventually. ObjectIsMarkedGray() is conservative and reports
  // that target is not marked gray; AssertObjectIsNotGray() will
  // assert.
  grayRoots.grayRoot1.get();
  CHECK(IsMarkedBlack(wrapper));
  CHECK(IsMarkedGray(target));
  CHECK(!JS::ObjectIsMarkedGray(target));

  // Final state: source and target are black.
  JS::FinishIncrementalGC(cx, JS::GCReason::API);
  CHECK(IsMarkedBlack(wrapper));
  CHECK(IsMarkedBlack(target));

  grayRoots.grayRoot1 = nullptr;
  grayRoots.grayRoot2 = nullptr;

  return true;
}

bool TestGrayUnmarking() {
  const size_t length = 2000;

  JSObject* chain = AllocObjectChain(length);
  CHECK(chain);

  RootedObject blackRoot(cx, chain);
  JS_GC(cx);
  size_t count;
  CHECK(IterateObjectChain(chain, ColorCheckFunctor(MarkColor::Black, &count)));
  CHECK(count == length);

  blackRoot = nullptr;
  grayRoots.grayRoot1 = chain;
  JS_GC(cx);
  CHECK(cx->runtime()->gc.areGrayBitsValid());
  CHECK(IterateObjectChain(chain, ColorCheckFunctor(MarkColor::Gray, &count)));
  CHECK(count == length);

  JS::ExposeObjectToActiveJS(chain);
  CHECK(cx->runtime()->gc.areGrayBitsValid());
  CHECK(IterateObjectChain(chain, ColorCheckFunctor(MarkColor::Black, &count)));
  CHECK(count == length);

  grayRoots.grayRoot1 = nullptr;

  return true;
}

struct ColorCheckFunctor {
  MarkColor color;
  size_t& count;

  ColorCheckFunctor(MarkColor colorArg, size_t* countArg)
      : color(colorArg), count(*countArg) {
    count = 0;
  }

  bool operator()(JSObject* obj) {
    if (!CheckCellColor(obj, color)) {
      return false;
    }

    NativeObject& nobj = obj->as<NativeObject>();
    if (!CheckCellColor(nobj.shape(), color)) {
      return false;
    }

    Shape* shape = nobj.shape();
    if (!CheckCellColor(shape, color)) {
      return false;
    }

    // Shapes and symbols are never marked gray.
    jsid id = shape->propid();
    if (JSID_IS_GCTHING(id) &&
        !CheckCellColor(JSID_TO_GCTHING(id).asCell(), MarkColor::Black)) {
      return false;
    }

    count++;
    return true;
  }
};

JS::PersistentRootedObject global1;
JS::PersistentRootedObject global2;

struct GrayRoots {
  JS::Heap<JSObject*> grayRoot1;
  JS::Heap<JSObject*> grayRoot2;
};

GrayRoots grayRoots;

bool InitGlobals() {
  global1.init(cx, global);
  if (!createGlobal()) {
    return false;
  }
  global2.init(cx, global);
  return global2 != nullptr;
}

void ClearGrayRoots() {
  grayRoots.grayRoot1 = nullptr;
  grayRoots.grayRoot2 = nullptr;
}

void InitGrayRootTracer() {
  ClearGrayRoots();
  JS_SetGrayGCRootsTracer(cx, TraceGrayRoots, &grayRoots);
}

void RemoveGrayRootTracer() {
  ClearGrayRoots();
  JS_SetGrayGCRootsTracer(cx, nullptr, nullptr);
}

static void TraceGrayRoots(JSTracer* trc, void* data) {
  auto grayRoots = static_cast<GrayRoots*>(data);
  TraceEdge(trc, &grayRoots->grayRoot1, "gray root 1");
  TraceEdge(trc, &grayRoots->grayRoot2, "gray root 2");
}

JSObject* AllocPlainObject() {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  EvictNursery();

  MOZ_ASSERT(obj->compartment() == global1->compartment());
  return obj;
}

JSObject* AllocSameCompartmentSourceObject(JSObject* target) {
  JS::RootedObject source(cx, JS_NewPlainObject(cx));
  if (!source) {
    return nullptr;
  }

  JS::RootedObject obj(cx, target);
  if (!JS_DefineProperty(cx, source, "ptr", obj, 0)) {
    return nullptr;
  }

  EvictNursery();

  MOZ_ASSERT(source->compartment() == global1->compartment());
  return source;
}

JSObject* GetCrossCompartmentWrapper(JSObject* target) {
  MOZ_ASSERT(target->compartment() == global1->compartment());
  JS::RootedObject obj(cx, target);
  JSAutoRealm ar(cx, global2);
  if (!JS_WrapObject(cx, &obj)) {
    return nullptr;
  }

  EvictNursery();

  MOZ_ASSERT(obj->compartment() == global2->compartment());
  return obj;
}

JSObject* AllocWeakmapKeyObject() {
  JS::RootedObject delegate(cx, JS_NewPlainObject(cx));
  if (!delegate) {
    return nullptr;
  }

  JS::RootedObject key(cx,
                       js::Wrapper::New(cx, delegate, &js::Wrapper::singleton));

  EvictNursery();
  return key;
}

JSObject* AllocObjectChain(size_t length) {
  // Allocate a chain of linked JSObjects.

  // Use a unique property name so the shape is not shared with any other
  // objects.
  RootedString nextPropName(cx, JS_NewStringCopyZ(cx, "unique14142135"));
  RootedId nextId(cx);
  if (!JS_StringToId(cx, nextPropName, &nextId)) {
    return nullptr;
  }

  RootedObject head(cx);
  for (size_t i = 0; i < length; i++) {
    RootedValue next(cx, ObjectOrNullValue(head));
    head = AllocPlainObject();
    if (!head) {
      return nullptr;
    }
    if (!JS_DefinePropertyById(cx, head, nextId, next, 0)) {
      return nullptr;
    }
  }

  return head;
}

template <typename F>
bool IterateObjectChain(JSObject* chain, F f) {
  RootedObject obj(cx, chain);
  while (obj) {
    if (!f(obj)) {
      return false;
    }

    // Access the 'next' property via the object's slots to avoid triggering
    // gray marking assertions when calling JS_GetPropertyById.
    NativeObject& nobj = obj->as<NativeObject>();
    MOZ_ASSERT(nobj.slotSpan() == 1);
    obj = nobj.getSlot(0).toObjectOrNull();
  }

  return true;
}

static bool IsMarkedBlack(Cell* cell) {
  TenuredCell* tc = &cell->asTenured();
  return tc->isMarkedBlack();
}

static bool IsMarkedGray(Cell* cell) {
  TenuredCell* tc = &cell->asTenured();
  bool isGray = tc->isMarkedGray();
  MOZ_ASSERT_IF(isGray, tc->isMarkedAny());
  return isGray;
}

static bool CheckCellColor(Cell* cell, MarkColor color) {
  MOZ_ASSERT(color == MarkColor::Black || color == MarkColor::Gray);
  if (color == MarkColor::Black && !IsMarkedBlack(cell)) {
    printf("Found non-black cell: %p\n", cell);
    return false;
  } else if (color == MarkColor::Gray && !IsMarkedGray(cell)) {
    printf("Found non-gray cell: %p\n", cell);
    return false;
  }

  return true;
}

void EvictNursery() { cx->runtime()->gc.evictNursery(); }

bool ZoneGC(JS::Zone* zone) {
  uint32_t oldMode = JS_GetGCParameter(cx, JSGC_MODE);
  JS_SetGCParameter(cx, JSGC_MODE, JSGC_MODE_ZONE);
  JS::PrepareZoneForGC(zone);
  cx->runtime()->gc.gc(GC_NORMAL, JS::GCReason::API);
  CHECK(!cx->runtime()->gc.isFullGc());
  JS_SetGCParameter(cx, JSGC_MODE, oldMode);
  return true;
}

END_TEST(testGCGrayMarking)
