/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static const unsigned BufSize = 20;
static unsigned FinalizeCalls = 0;
static JSFinalizeStatus StatusBuffer[BufSize];

BEGIN_TEST(testGCFinalizeCallback) {
  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, true);
  JS_SetGCParameter(cx, JSGC_PER_ZONE_GC_ENABLED, true);

  /* Full GC, non-incremental. */
  FinalizeCalls = 0;
  JS_GC(cx);
  CHECK(cx->runtime()->gc.isFullGc());
  CHECK(checkSingleGroup());
  CHECK(checkFinalizeStatus());

  /* Full GC, incremental. */
  FinalizeCalls = 0;
  JS::PrepareForFullGC(cx);
  JS::StartIncrementalGC(cx, GC_NORMAL, JS::GCReason::API, 1000000);
  while (cx->runtime()->gc.isIncrementalGCInProgress()) {
    JS::PrepareForFullGC(cx);
    JS::IncrementalGCSlice(cx, JS::GCReason::API, 1000000);
  }
  CHECK(!cx->runtime()->gc.isIncrementalGCInProgress());
  CHECK(cx->runtime()->gc.isFullGc());
  CHECK(checkMultipleGroups());
  CHECK(checkFinalizeStatus());

#ifdef JS_GC_ZEAL
  // Bug 1377593 - the below tests want to control how many zones are GC'ing,
  // and some zeal modes will convert them into all-zones GCs.
  JS_SetGCZeal(cx, 0, 0);
#endif

  JS::RootedObject global1(cx, createTestGlobal());
  JS::RootedObject global2(cx, createTestGlobal());
  JS::RootedObject global3(cx, createTestGlobal());
  CHECK(global1);
  CHECK(global2);
  CHECK(global3);

  /* Zone GC, non-incremental, single zone. */
  FinalizeCalls = 0;
  JS::PrepareZoneForGC(cx, global1->zone());
  JS::NonIncrementalGC(cx, GC_NORMAL, JS::GCReason::API);
  CHECK(!cx->runtime()->gc.isFullGc());
  CHECK(checkSingleGroup());
  CHECK(checkFinalizeStatus());

  /* Zone GC, non-incremental, multiple zones. */
  FinalizeCalls = 0;
  JS::PrepareZoneForGC(cx, global1->zone());
  JS::PrepareZoneForGC(cx, global2->zone());
  JS::PrepareZoneForGC(cx, global3->zone());
  JS::NonIncrementalGC(cx, GC_NORMAL, JS::GCReason::API);
  CHECK(!cx->runtime()->gc.isFullGc());
  CHECK(checkSingleGroup());
  CHECK(checkFinalizeStatus());

  /* Zone GC, incremental, single zone. */
  FinalizeCalls = 0;
  JS::PrepareZoneForGC(cx, global1->zone());
  JS::StartIncrementalGC(cx, GC_NORMAL, JS::GCReason::API, 1000000);
  while (cx->runtime()->gc.isIncrementalGCInProgress()) {
    JS::PrepareZoneForGC(cx, global1->zone());
    JS::IncrementalGCSlice(cx, JS::GCReason::API, 1000000);
  }
  CHECK(!cx->runtime()->gc.isIncrementalGCInProgress());
  CHECK(!cx->runtime()->gc.isFullGc());
  CHECK(checkSingleGroup());
  CHECK(checkFinalizeStatus());

  /* Zone GC, incremental, multiple zones. */
  FinalizeCalls = 0;
  JS::PrepareZoneForGC(cx, global1->zone());
  JS::PrepareZoneForGC(cx, global2->zone());
  JS::PrepareZoneForGC(cx, global3->zone());
  JS::StartIncrementalGC(cx, GC_NORMAL, JS::GCReason::API, 1000000);
  while (cx->runtime()->gc.isIncrementalGCInProgress()) {
    JS::PrepareZoneForGC(cx, global1->zone());
    JS::PrepareZoneForGC(cx, global2->zone());
    JS::PrepareZoneForGC(cx, global3->zone());
    JS::IncrementalGCSlice(cx, JS::GCReason::API, 1000000);
  }
  CHECK(!cx->runtime()->gc.isIncrementalGCInProgress());
  CHECK(!cx->runtime()->gc.isFullGc());
  CHECK(checkMultipleGroups());
  CHECK(checkFinalizeStatus());

#ifdef JS_GC_ZEAL

  /* Full GC with reset due to new zone, becoming zone GC. */

  FinalizeCalls = 0;
  JS_SetGCZeal(cx, 9, 1000000);
  JS::PrepareForFullGC(cx);
  js::SliceBudget budget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(GC_NORMAL, budget);
  CHECK(cx->runtime()->gc.state() == js::gc::State::Mark);
  CHECK(cx->runtime()->gc.isFullGc());

  JS::RootedObject global4(cx, createTestGlobal());
  budget = js::SliceBudget(js::WorkBudget(1));
  cx->runtime()->gc.debugGCSlice(budget);
  while (cx->runtime()->gc.isIncrementalGCInProgress()) {
    cx->runtime()->gc.debugGCSlice(budget);
  }
  CHECK(!cx->runtime()->gc.isIncrementalGCInProgress());
  CHECK(checkSingleGroup());
  CHECK(checkFinalizeStatus());

  JS_SetGCZeal(cx, 0, 0);

#endif

  /*
   * Make some use of the globals here to ensure the compiler doesn't optimize
   * them away in release builds, causing the zones to be collected and
   * the test to fail.
   */
  CHECK(JS_IsGlobalObject(global1));
  CHECK(JS_IsGlobalObject(global2));
  CHECK(JS_IsGlobalObject(global3));

  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, false);
  JS_SetGCParameter(cx, JSGC_PER_ZONE_GC_ENABLED, false);

  return true;
}

JSObject* createTestGlobal() {
  JS::RealmOptions options;
  return JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                            JS::FireOnNewGlobalHook, options);
}

virtual bool init() override {
  if (!JSAPITest::init()) {
    return false;
  }

  JS_AddFinalizeCallback(cx, FinalizeCallback, nullptr);
  return true;
}

virtual void uninit() override {
  JS_RemoveFinalizeCallback(cx, FinalizeCallback);
  JSAPITest::uninit();
}

bool checkSingleGroup() {
  CHECK(FinalizeCalls < BufSize);
  CHECK(FinalizeCalls == 4);
  return true;
}

bool checkMultipleGroups() {
  CHECK(FinalizeCalls < BufSize);
  CHECK(FinalizeCalls % 3 == 1);
  CHECK((FinalizeCalls - 1) / 3 > 1);
  return true;
}

bool checkFinalizeStatus() {
  /*
   * The finalize callback should be called twice for each sweep group
   * finalized, with status JSFINALIZE_GROUP_START and JSFINALIZE_GROUP_END,
   * and then once more with JSFINALIZE_COLLECTION_END.
   */

  for (unsigned i = 0; i < FinalizeCalls - 1; i += 3) {
    CHECK(StatusBuffer[i] == JSFINALIZE_GROUP_PREPARE);
    CHECK(StatusBuffer[i + 1] == JSFINALIZE_GROUP_START);
    CHECK(StatusBuffer[i + 2] == JSFINALIZE_GROUP_END);
  }

  CHECK(StatusBuffer[FinalizeCalls - 1] == JSFINALIZE_COLLECTION_END);

  return true;
}

static void FinalizeCallback(JSFreeOp* fop, JSFinalizeStatus status,
                             void* data) {
  if (FinalizeCalls < BufSize) {
    StatusBuffer[FinalizeCalls] = status;
  }
  ++FinalizeCalls;
}
END_TEST(testGCFinalizeCallback)
