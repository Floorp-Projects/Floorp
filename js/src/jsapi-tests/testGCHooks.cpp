/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"

#include <iterator>

#include "jsapi-tests/tests.h"

static unsigned gSliceCallbackCount = 0;
static bool gSawAllSliceCallbacks;
static bool gSawAllGCCallbacks;

static void NonIncrementalGCSliceCallback(JSContext* cx,
                                          JS::GCProgress progress,
                                          const JS::GCDescription& desc) {
  using namespace JS;
  static GCProgress expect[] = {GC_CYCLE_BEGIN, GC_SLICE_BEGIN, GC_SLICE_END,
                                GC_CYCLE_END};

  MOZ_RELEASE_ASSERT(gSliceCallbackCount < std::size(expect));
  MOZ_RELEASE_ASSERT(progress == expect[gSliceCallbackCount++]);
  MOZ_RELEASE_ASSERT(desc.isZone_ == false);
  MOZ_RELEASE_ASSERT(desc.options_ == JS::GCOptions::Normal);
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

  static constexpr struct {
    GCProgress progress;
    GCReason reason;
  } expect[] = {
      // Explicitly requested a full GC.
      {GC_CYCLE_BEGIN, GCReason::DEBUG_GC},
      {GC_SLICE_BEGIN, GCReason::DEBUG_GC},
      {GC_SLICE_END, GCReason::DEBUG_GC},
      {GC_SLICE_BEGIN, GCReason::DEBUG_GC},
      {GC_SLICE_END, GCReason::DEBUG_GC},
      {GC_CYCLE_END, GCReason::DEBUG_GC},
      // Shutdown GC with ROOTS_REMOVED.
      {GC_CYCLE_BEGIN, GCReason::ROOTS_REMOVED},
      {GC_SLICE_BEGIN, GCReason::ROOTS_REMOVED},
      {GC_SLICE_END, GCReason::ROOTS_REMOVED},
      {GC_CYCLE_END, GCReason::ROOTS_REMOVED}
      // All done.
  };

  MOZ_RELEASE_ASSERT(gSliceCallbackCount < std::size(expect));
  MOZ_RELEASE_ASSERT(progress == expect[gSliceCallbackCount].progress);
  MOZ_RELEASE_ASSERT(desc.isZone_ == false);
  MOZ_RELEASE_ASSERT(desc.options_ == JS::GCOptions::Shrink);
  MOZ_RELEASE_ASSERT(desc.reason_ == expect[gSliceCallbackCount].reason);
  gSliceCallbackCount++;
}

BEGIN_TEST(testGCRootsRemoved) {
  AutoLeaveZeal nozeal(cx);

  AutoGCParameter param1(cx, JSGC_INCREMENTAL_GC_ENABLED, true);

  gSliceCallbackCount = 0;
  JS::SetGCSliceCallback(cx, RootsRemovedGCSliceCallback);
  auto byebye =
      mozilla::MakeScopeExit([=] { JS::SetGCSliceCallback(cx, nullptr); });

  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);

  JS::PrepareForFullGC(cx);
  js::SliceBudget budget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(JS::GCOptions::Shrink, budget);
  CHECK(JS::IsIncrementalGCInProgress(cx));

  // Trigger another GC after the current one in shrinking / shutdown GCs.
  cx->runtime()->gc.notifyRootsRemoved();

  JS::FinishIncrementalGC(cx, JS::GCReason::DEBUG_GC);
  CHECK(!JS::IsIncrementalGCInProgress(cx));

  return true;
}
END_TEST(testGCRootsRemoved)

#define ASSERT_MSG(cond, ...)       \
  do {                              \
    if (!(cond)) {                  \
      fprintf(stderr, __VA_ARGS__); \
      MOZ_RELEASE_ASSERT(cond);     \
    }                               \
  } while (false)

// Trigger some nested GCs to ensure that they get their own reasons and
// fullGCRequested state.
//
// The callbacks will form the following tree:
//
//   Begin(DEBUG_GC)
//     Begin(API)
//     End(API)
//   End(DEBUG_GC)
//     Begin(MEM_PRESSURE)
//     End(MEM_PRESSURE)
//       Begin(DOM_WINDOW_UTILS)
//       End(DOM_WINDOW_UTILS)
//
// JSGC_BEGIN and JSGC_END callbacks will be observed as a preorder traversal
// of the above tree.
//
// Note that the ordering of the *slice* callbacks don't match up simply to the
// ordering above. If a JSGC_BEGIN triggers another GC, we won't see the outer
// GC's GC_CYCLE_BEGIN until the inner one is done. The slice callbacks are
// reporting the actual order that the GCs are happening in.
//
// JSGC_END, on the other hand, won't be emitted until the GC is complete and
// the GC_CYCLE_BEGIN callback has fired. So its ordering is straightforward.
//
static void GCTreeCallback(JSContext* cx, JSGCStatus status,
                           JS::GCReason reason, void* data) {
  using namespace JS;

  static constexpr struct {
    JSGCStatus expectedStatus;
    JS::GCReason expectedReason;
    bool fireGC;
    JS::GCReason reason;
    bool requestFullGC;
  } invocations[] = {
      {JSGC_BEGIN, GCReason::DEBUG_GC, true, GCReason::API, false},
      {JSGC_BEGIN, GCReason::API, false},
      {JSGC_END, GCReason::API, false},
      {JSGC_END, GCReason::DEBUG_GC, true, GCReason::MEM_PRESSURE, true},
      {JSGC_BEGIN, GCReason::MEM_PRESSURE, false},
      {JSGC_END, GCReason::MEM_PRESSURE, true, GCReason::DOM_WINDOW_UTILS,
       false},
      {JSGC_BEGIN, GCReason::DOM_WINDOW_UTILS, false},
      {JSGC_END, GCReason::DOM_WINDOW_UTILS, false}};

  static size_t i = 0;
  MOZ_RELEASE_ASSERT(i < std::size(invocations));
  auto& invocation = invocations[i++];
  if (i == std::size(invocations)) {
    gSawAllGCCallbacks = true;
  }
  ASSERT_MSG(status == invocation.expectedStatus,
             "GC callback #%zu: got status %d expected %d\n", i, status,
             invocation.expectedStatus);
  ASSERT_MSG(reason == invocation.expectedReason,
             "GC callback #%zu: got reason %s expected %s\n", i,
             ExplainGCReason(reason),
             ExplainGCReason(invocation.expectedReason));
  if (invocation.fireGC) {
    if (invocation.requestFullGC) {
      JS::PrepareForFullGC(cx);
    }
    js::SliceBudget budget = js::SliceBudget(js::WorkBudget(1));
    cx->runtime()->gc.startGC(GCOptions::Normal, invocation.reason, budget);
    MOZ_RELEASE_ASSERT(JS::IsIncrementalGCInProgress(cx));

    JS::FinishIncrementalGC(cx, invocation.reason);
    MOZ_RELEASE_ASSERT(!JS::IsIncrementalGCInProgress(cx));
  }
}

static void GCTreeSliceCallback(JSContext* cx, JS::GCProgress progress,
                                const JS::GCDescription& desc) {
  using namespace JS;

  static constexpr struct {
    GCProgress progress;
    GCReason reason;
    bool isZonal;
  } expectations[] = {
      // JSGC_BEGIN triggers a new GC before we get any slice callbacks from the
      // original outer GC. So the very first reason observed is API, not
      // DEBUG_GC.
      {GC_CYCLE_BEGIN, GCReason::API, true},
      {GC_SLICE_BEGIN, GCReason::API, true},
      {GC_SLICE_END, GCReason::API, true},
      {GC_SLICE_BEGIN, GCReason::API, true},
      {GC_SLICE_END, GCReason::API, true},
      {GC_CYCLE_END, GCReason::API, true},
      // Now the "outer" GC runs. It requested a full GC.
      {GC_CYCLE_BEGIN, GCReason::DEBUG_GC, false},
      {GC_SLICE_BEGIN, GCReason::DEBUG_GC, false},
      {GC_SLICE_END, GCReason::DEBUG_GC, false},
      {GC_SLICE_BEGIN, GCReason::DEBUG_GC, false},
      {GC_SLICE_END, GCReason::DEBUG_GC, false},
      {GC_CYCLE_END, GCReason::DEBUG_GC, false},
      // The outer JSGC_DEBUG GC's end callback triggers a full MEM_PRESSURE
      // GC, which runs next. (Its JSGC_BEGIN does not run a GC.)
      {GC_CYCLE_BEGIN, GCReason::MEM_PRESSURE, false},
      {GC_SLICE_BEGIN, GCReason::MEM_PRESSURE, false},
      {GC_SLICE_END, GCReason::MEM_PRESSURE, false},
      {GC_SLICE_BEGIN, GCReason::MEM_PRESSURE, false},
      {GC_SLICE_END, GCReason::MEM_PRESSURE, false},
      {GC_CYCLE_END, GCReason::MEM_PRESSURE, false},
      // The MEM_PRESSURE's GC's end callback now triggers a (zonal)
      // DOM_WINDOW_UTILS GC.
      {GC_CYCLE_BEGIN, GCReason::DOM_WINDOW_UTILS, true},
      {GC_SLICE_BEGIN, GCReason::DOM_WINDOW_UTILS, true},
      {GC_SLICE_END, GCReason::DOM_WINDOW_UTILS, true},
      {GC_SLICE_BEGIN, GCReason::DOM_WINDOW_UTILS, true},
      {GC_SLICE_END, GCReason::DOM_WINDOW_UTILS, true},
      {GC_CYCLE_END, GCReason::DOM_WINDOW_UTILS, true},
      // All done.
  };

  MOZ_RELEASE_ASSERT(gSliceCallbackCount < std::size(expectations));
  auto& expect = expectations[gSliceCallbackCount];
  ASSERT_MSG(progress == expect.progress, "iteration %d: wrong progress\n",
             gSliceCallbackCount);
  ASSERT_MSG(desc.reason_ == expect.reason,
             "iteration %d: expected %s got %s\n", gSliceCallbackCount,
             JS::ExplainGCReason(expect.reason),
             JS::ExplainGCReason(desc.reason_));
  ASSERT_MSG(desc.isZone_ == expect.isZonal, "iteration %d: wrong zonal\n",
             gSliceCallbackCount);
  MOZ_RELEASE_ASSERT(desc.options_ == JS::GCOptions::Normal);
  gSliceCallbackCount++;
  if (gSliceCallbackCount == std::size(expectations)) {
    gSawAllSliceCallbacks = true;
  }
}

BEGIN_TEST(testGCTree) {
  AutoLeaveZeal nozeal(cx);

  AutoGCParameter param1(cx, JSGC_INCREMENTAL_GC_ENABLED, true);

  gSliceCallbackCount = 0;
  gSawAllSliceCallbacks = false;
  gSawAllGCCallbacks = false;
  JS::SetGCSliceCallback(cx, GCTreeSliceCallback);
  JS_SetGCCallback(cx, GCTreeCallback, nullptr);

  // Automate the callback clearing. Otherwise if a CHECK fails, it will get
  // cluttered with additional failures from the callback unexpectedly firing
  // during the final shutdown GC.
  auto byebye = mozilla::MakeScopeExit([=] {
    JS::SetGCSliceCallback(cx, nullptr);
    JS_SetGCCallback(cx, nullptr, nullptr);
  });

  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  CHECK(obj);

  // Outer GC is a full GC.
  JS::PrepareForFullGC(cx);
  js::SliceBudget budget(js::WorkBudget(1));
  cx->runtime()->gc.startDebugGC(JS::GCOptions::Normal, budget);
  CHECK(JS::IsIncrementalGCInProgress(cx));

  JS::FinishIncrementalGC(cx, JS::GCReason::DEBUG_GC);
  CHECK(!JS::IsIncrementalGCInProgress(cx));
  CHECK(gSawAllSliceCallbacks);
  CHECK(gSawAllGCCallbacks);

  return true;
}
END_TEST(testGCTree)
