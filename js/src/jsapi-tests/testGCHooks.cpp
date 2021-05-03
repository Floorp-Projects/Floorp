/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"

#include <iterator>

#include "jsapi-tests/tests.h"

static unsigned gSliceCallbackCount = 0;

static void NonIncrementalGCSliceCallback(JSContext* cx,
                                          JS::GCProgress progress,
                                          const JS::GCDescription& desc) {
  using namespace JS;
  static GCProgress expect[] = {GC_CYCLE_BEGIN, GC_SLICE_BEGIN, GC_SLICE_END,
                                GC_CYCLE_END};

  MOZ_RELEASE_ASSERT(gSliceCallbackCount < std::size(expect));
  MOZ_RELEASE_ASSERT(progress == expect[gSliceCallbackCount++]);
  MOZ_RELEASE_ASSERT(desc.isZone_ == false);
  MOZ_RELEASE_ASSERT(desc.invocationKind_ == GC_NORMAL);
  MOZ_RELEASE_ASSERT(desc.reason_ == JS::GCReason::API);
  if (progress == GC_CYCLE_END) {
    mozilla::UniquePtr<char16_t> summary(desc.formatSummaryMessage(cx));
    mozilla::UniquePtr<char16_t> message(desc.formatSliceMessage(cx));
  }
}

BEGIN_TEST(testGCSliceCallback) {
  gSliceCallbackCount = 0;
  JS::SetGCSliceCallback(cx, NonIncrementalGCSliceCallback);
  JS_GC(cx);
  JS::SetGCSliceCallback(cx, nullptr);
  CHECK(gSliceCallbackCount == 4);
  return true;
}
END_TEST(testGCSliceCallback)

static void RootsRemovedGCSliceCallback(JSContext* cx, JS::GCProgress progress,
                                        const JS::GCDescription& desc) {
  using namespace JS;

  static GCProgress expectProgress[] = {
      GC_CYCLE_BEGIN, GC_SLICE_BEGIN, GC_SLICE_END,   GC_SLICE_BEGIN,
      GC_SLICE_END,   GC_CYCLE_END,   GC_CYCLE_BEGIN, GC_SLICE_BEGIN,
      GC_SLICE_END,   GC_CYCLE_END};

  static GCReason expectReasons[] = {
      GCReason::DEBUG_GC,      GCReason::DEBUG_GC,      GCReason::DEBUG_GC,
      GCReason::DEBUG_GC,      GCReason::DEBUG_GC,      GCReason::DEBUG_GC,
      GCReason::ROOTS_REMOVED, GCReason::ROOTS_REMOVED, GCReason::ROOTS_REMOVED,
      GCReason::ROOTS_REMOVED};

  static_assert(
      std::size(expectProgress) == std::size(expectReasons),
      "expectProgress and expectReasons arrays should be the same length");

  MOZ_RELEASE_ASSERT(gSliceCallbackCount < std::size(expectProgress));
  MOZ_RELEASE_ASSERT(progress == expectProgress[gSliceCallbackCount]);
  MOZ_RELEASE_ASSERT(desc.isZone_ == false);
  MOZ_RELEASE_ASSERT(desc.invocationKind_ == GC_SHRINK);
  MOZ_RELEASE_ASSERT(desc.reason_ == expectReasons[gSliceCallbackCount]);
  gSliceCallbackCount++;
}

BEGIN_TEST(testGCRootsRemoved) {
#ifdef JS_GC_ZEAL
  AutoLeaveZeal nozeal(cx);
#endif /* JS_GC_ZEAL */

  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, true);

  gSliceCallbackCount = 0;
  JS::SetGCSliceCallback(cx, RootsRemovedGCSliceCallback);

  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);

  JS::PrepareForFullGC(cx);
  js::SliceBudget budget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(GC_SHRINK, budget);
  CHECK(JS::IsIncrementalGCInProgress(cx));

  // Trigger another GC after the current one in shrinking / shutdown GCs.
  cx->runtime()->gc.notifyRootsRemoved();

  JS::FinishIncrementalGC(cx, JS::GCReason::DEBUG_GC);
  CHECK(!JS::IsIncrementalGCInProgress(cx));

  JS::SetGCSliceCallback(cx, nullptr);

  JS_SetGCParameter(cx, JSGC_INCREMENTAL_GC_ENABLED, false);

  return true;
}
END_TEST(testGCRootsRemoved)
