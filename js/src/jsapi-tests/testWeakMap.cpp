/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Zone.h"
#include "js/Array.h"               // JS::GetArrayLength
#include "js/Exception.h"           // JS_IsExceptionPending
#include "js/GlobalObject.h"        // JS_NewGlobalObject
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "js/WeakMap.h"
#include "jsapi-tests/tests.h"
#include "vm/Realm.h"

using namespace js;

static bool checkSize(JSContext* cx, JS::HandleObject map, uint32_t expected) {
  JS::RootedObject keys(cx);
  if (!JS_NondeterministicGetWeakMapKeys(cx, map, &keys)) {
    return false;
  }

  uint32_t length;
  if (!JS::GetArrayLength(cx, keys, &length)) {
    return false;
  }

  return length == expected;
}

JSObject* keyDelegate = nullptr;

BEGIN_TEST(testWeakMap_basicOperations) {
  JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
  CHECK(IsWeakMapObject(map));

  JS::RootedValue key(cx, JS::ObjectOrNullValue(newKey()));
  CHECK(!key.isNull());
  CHECK(!JS::IsWeakMapObject(&key.toObject()));

  JS::RootedValue r(cx);
  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r.isUndefined());

  CHECK(checkSize(cx, map, 0));

  JS::RootedValue val(cx, JS::Int32Value(1));
  CHECK(SetWeakMapEntry(cx, map, key, val));

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(cx, map, 1));

  JS_GC(cx);

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(cx, map, 1));

  key.setUndefined();
  JS_GC(cx);

  CHECK(checkSize(cx, map, 0));

  return true;
}

JSObject* newKey() { return JS_NewPlainObject(cx); }
END_TEST(testWeakMap_basicOperations)

BEGIN_TEST(testWeakMap_setWeakMapEntry_invalid_key) {
  JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
  CHECK(IsWeakMapObject(map));
  CHECK(checkSize(cx, map, 0));

  JS::RootedString test(cx, JS_NewStringCopyZ(cx, "test"));
  // sym is a Symbol in global Symbol registry and hence can't be used as a key.
  JS::RootedSymbol sym(cx, JS::GetSymbolFor(cx, test));
  JS::RootedValue key(cx, JS::SymbolValue(sym));
  CHECK(!key.isUndefined());

  JS::RootedValue val(cx, JS::Int32Value(1));

  CHECK(!JS_IsExceptionPending(cx));
  CHECK(SetWeakMapEntry(cx, map, key, val) == false);

  CHECK(JS_IsExceptionPending(cx));
  JS::Rooted<JS::Value> exn(cx);
  CHECK(JS_GetPendingException(cx, &exn));
  JS::Rooted<JSObject*> obj(cx, &exn.toObject());
  JSErrorReport* err = JS_ErrorFromException(cx, obj);
  CHECK(err->exnType == JSEXN_TYPEERR);

  JS_ClearPendingException(cx);

  JS::RootedValue r(cx);
  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == JS::UndefinedValue());
  CHECK(checkSize(cx, map, 0));

  return true;
}
END_TEST(testWeakMap_setWeakMapEntry_invalid_key)

#ifdef NIGHTLY_BUILD
BEGIN_TEST(testWeakMap_basicOperations_symbols_as_keys) {
  JS::RootedObject map(cx, JS::NewWeakMapObject(cx));
  CHECK(IsWeakMapObject(map));

  JS::RootedString test(cx, JS_NewStringCopyZ(cx, "test"));
  JS::RootedSymbol sym(cx, JS::NewSymbol(cx, test));
  CHECK(sym);
  JS::RootedValue key(cx, JS::SymbolValue(sym));

  JS::RootedValue r(cx);
  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r.isUndefined());

  CHECK(checkSize(cx, map, 0));

  JS::RootedValue val(cx, JS::Int32Value(1));
  CHECK(SetWeakMapEntry(cx, map, key, val));

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(cx, map, 1));

  JS_GC(cx);

  CHECK(GetWeakMapEntry(cx, map, key, &r));
  CHECK(r == val);
  CHECK(checkSize(cx, map, 1));

  sym = nullptr;
  key.setUndefined();
  JS_GC(cx);

  CHECK(checkSize(cx, map, 0));

  return true;
}
END_TEST(testWeakMap_basicOperations_symbols_as_keys)
#endif

BEGIN_TEST(testWeakMap_keyDelegates) {
  AutoLeaveZeal nozeal(cx);

  AutoGCParameter param(cx, JSGC_INCREMENTAL_GC_ENABLED, true);
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
  CHECK(newCCW(map, delegateRoot));
  performIncrementalGC();
#ifdef DEBUG
  CHECK(map->zone()->lastSweepGroupIndex() <
        delegateRoot->zone()->lastSweepGroupIndex());
#endif

  /* Add our entry to the weakmap. */
  JS::RootedValue keyVal(cx, JS::ObjectValue(*key));
  JS::RootedValue val(cx, JS::Int32Value(1));
  CHECK(SetWeakMapEntry(cx, map, keyVal, val));
  CHECK(checkSize(cx, map, 1));

  /*
   * Check the delegate keeps the entry alive even if the key is not reachable.
   */
  key = nullptr;
  keyVal.setUndefined();
  CHECK(newCCW(map, delegateRoot));
  performIncrementalGC();
  CHECK(checkSize(cx, map, 1));

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
  CHECK(checkSize(cx, map, 0));

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
      "keyWithDelegate", JSCLASS_HAS_RESERVED_SLOTS(1),
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

void performIncrementalGC() {
  JSRuntime* rt = cx->runtime();
  js::SliceBudget budget(js::WorkBudget(1000));
  rt->gc.startDebugGC(JS::GCOptions::Normal, budget);

  // Wait until we've started marking before finishing the GC
  // non-incrementally.
  while (rt->gc.state() == gc::State::Prepare) {
    rt->gc.debugGCSlice(budget);
  }
  if (JS::IsIncrementalGCInProgress(cx)) {
    rt->gc.finishGC(JS::GCReason::DEBUG_GC);
  }
}
END_TEST(testWeakMap_keyDelegates)
