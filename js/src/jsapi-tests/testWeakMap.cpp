/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone.h"
#include "js/Array.h"  // JS::GetArrayLength
#include "jsapi-tests/tests.h"
#include "vm/Realm.h"

using namespace js;

JSObject* keyDelegate = nullptr;

BEGIN_TEST(testWeakMap_basicOperations) {
  JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
  CHECK(IsWeakMapObject(map));

  JS::RootedObject key(cx, newKey());
  CHECK(key);
  CHECK(!IsWeakMapObject(key));

  JS::RootedValue r(cx);
  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r.isUndefined());

  CHECK(checkSize(map, 0));

  JS::RootedValue val(cx, JS::Int32Value(1));
  CHECK(SetWeakMapEntry(cx, map, key, val));

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(map, 1));

  JS_GC(cx);

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(map, 1));

  key = nullptr;
  JS_GC(cx);

  CHECK(checkSize(map, 0));

  return true;
}

JSObject* newKey() { return JS_NewPlainObject(cx); }

bool checkSize(JS::HandleObject map, uint32_t expected) {
  JS::RootedObject keys(cx);
  CHECK(JS_NondeterministicGetWeakMapKeys(cx, map, &keys));

  uint32_t length;
  CHECK(JS::GetArrayLength(cx, keys, &length));
  CHECK(length == expected);

  return true;
}
END_TEST(testWeakMap_basicOperations)

BEGIN_TEST(testWeakMap_keyDelegates) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, true);
  JS_GC(cx);
  JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
  CHECK(map);

  JS::RootedObject delegate(cx, newDelegate());
  JS::RootedObject key(cx, delegate);
  if (!JS_WrapObject(cx, &key)) {
    return false;
  }
  CHECK(key);
  CHECK(delegate);

  keyDelegate = delegate;

  JS::RootedObject delegateRoot(cx);
  {
    JSAutoRealm ar(cx, delegate);
    delegateRoot = JS_NewPlainObject(cx);
    CHECK(delegateRoot);
    JS::RootedValue delegateValue(cx, JS::ObjectValue(*delegate));
    CHECK(JS_DefineProperty(cx, delegateRoot, "delegate", delegateValue, 0));
  }
  delegate = nullptr;

  /*
   * Perform an incremental GC, introducing an unmarked CCW to force the map
   * zone to finish marking before the delegate zone.
   */
  JSRuntime* rt = cx->runtime();
  CHECK(newCCW(map, delegateRoot));
  js::SliceBudget budget(js::WorkBudget(1000));
  rt->gc.startDebugGC(JS::GCOptions::Normal, budget);
  if (JS::IsIncrementalGCInProgress(cx)) {
    // Wait until we've started marking before finishing the GC
    // non-incrementally.
    while (rt->gc.state() == gc::State::Prepare) {
      rt->gc.debugGCSlice(budget);
    }
    rt->gc.finishGC(JS::GCReason::DEBUG_GC);
  }
#ifdef DEBUG
  CHECK(map->zone()->lastSweepGroupIndex() <
        delegateRoot->zone()->lastSweepGroupIndex());
#endif

  /* Add our entry to the weakmap. */
  JS::RootedValue val(cx, JS::Int32Value(1));
  CHECK(SetWeakMapEntry(cx, map, key, val));
  CHECK(checkSize(map, 1));

  /*
   * Check the delegate keeps the entry alive even if the key is not reachable.
   */
  key = nullptr;
  CHECK(newCCW(map, delegateRoot));
  budget = js::SliceBudget(js::WorkBudget(1000));
  rt->gc.startDebugGC(JS::GCOptions::Normal, budget);
  if (JS::IsIncrementalGCInProgress(cx)) {
    // Wait until we've started marking before finishing the GC
    // non-incrementally.
    while (rt->gc.state() == gc::State::Prepare) {
      rt->gc.debugGCSlice(budget);
    }
    rt->gc.finishGC(JS::GCReason::DEBUG_GC);
  }
  CHECK(checkSize(map, 1));

  /*
   * Check that the zones finished marking at the same time, which is
   * necessary because of the presence of the delegate and the CCW.
   */
#ifdef DEBUG
  CHECK(map->zone()->lastSweepGroupIndex() ==
        delegateRoot->zone()->lastSweepGroupIndex());
#endif

  /* Check that when the delegate becomes unreachable the entry is removed. */
  delegateRoot = nullptr;
  keyDelegate = nullptr;
  JS_GC(cx);
  CHECK(checkSize(map, 0));

  return true;
}

static size_t DelegateObjectMoved(JSObject* obj, JSObject* old) {
  if (!keyDelegate) {
    return 0;  // Object got moved before we set keyDelegate to point to it.
  }

  MOZ_RELEASE_ASSERT(keyDelegate == old);
  keyDelegate = obj;
  return 0;
}

JSObject* newKey() {
  static const JSClass keyClass = {
      "keyWithDelegate", JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
      JS_NULL_CLASS_OPS, JS_NULL_CLASS_SPEC,
      JS_NULL_CLASS_EXT, JS_NULL_OBJECT_OPS};

  JS::RootedObject key(cx, JS_NewObject(cx, &keyClass));
  if (!key) {
    return nullptr;
  }

  return key;
}

JSObject* newCCW(JS::HandleObject sourceZone, JS::HandleObject destZone) {
  /*
   * Now ensure that this zone will be swept first by adding a cross
   * compartment wrapper to a new object in the same zone as the
   * delegate object.
   */
  JS::RootedObject object(cx);
  {
    JSAutoRealm ar(cx, destZone);
    object = JS_NewPlainObject(cx);
    if (!object) {
      return nullptr;
    }
  }
  {
    JSAutoRealm ar(cx, sourceZone);
    if (!JS_WrapObject(cx, &object)) {
      return nullptr;
    }
  }

  // In order to test the SCC algorithm, we need the wrapper/wrappee to be
  // tenured.
  cx->runtime()->gc.evictNursery();

  return object;
}

JSObject* newDelegate() {
  static const JSClassOps delegateClassOps = {
      nullptr,                   // addProperty
      nullptr,                   // delProperty
      nullptr,                   // enumerate
      nullptr,                   // newEnumerate
      nullptr,                   // resolve
      nullptr,                   // mayResolve
      nullptr,                   // finalize
      nullptr,                   // call
      nullptr,                   // hasInstance
      nullptr,                   // construct
      JS_GlobalObjectTraceHook,  // trace
  };

  static const js::ClassExtension delegateClassExtension = {
      DelegateObjectMoved,  // objectMovedOp
  };

  static const JSClass delegateClass = {
      "delegate",
      JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_RESERVED_SLOTS(1),
      &delegateClassOps,
      JS_NULL_CLASS_SPEC,
      &delegateClassExtension,
      JS_NULL_OBJECT_OPS};

  /* Create the global object. */
  JS::RealmOptions options;
  JS::RootedObject global(cx,
                          JS_NewGlobalObject(cx, &delegateClass, nullptr,
                                             JS::FireOnNewGlobalHook, options));
  if (!global) {
    return nullptr;
  }

  JS_SetReservedSlot(global, 0, JS::Int32Value(42));
  return global;
}

bool checkSize(JS::HandleObject map, uint32_t expected) {
  JS::RootedObject keys(cx);
  CHECK(JS_NondeterministicGetWeakMapKeys(cx, map, &keys));

  uint32_t length;
  CHECK(JS::GetArrayLength(cx, keys, &length));
  CHECK(length == expected);

  return true;
}
END_TEST(testWeakMap_keyDelegates)
