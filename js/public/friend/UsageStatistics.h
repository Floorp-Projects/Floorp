/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Telemetry and use counter functionality. */

#ifndef js_friend_UsageStatistics_h
#define js_friend_UsageStatistics_h

#include <stdint.h>  // uint32_t

#include "jstypes.h"  // JS_FRIEND_API

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;

/*
 * Telemetry reasons passed to the accumulate telemetry callback.
 *
 * It's OK for these enum values to change as they will be mapped to a fixed
 * member of the mozilla::Telemetry::HistogramID enum by the callback.
 */
enum {
  JS_TELEMETRY_GC_REASON,
  JS_TELEMETRY_GC_IS_ZONE_GC,
  JS_TELEMETRY_GC_MS,
  JS_TELEMETRY_GC_BUDGET_MS_2,
  JS_TELEMETRY_GC_BUDGET_OVERRUN,
  JS_TELEMETRY_GC_ANIMATION_MS,
  JS_TELEMETRY_GC_MAX_PAUSE_MS_2,
  JS_TELEMETRY_GC_PREPARE_MS,
  JS_TELEMETRY_GC_MARK_MS,
  JS_TELEMETRY_GC_SWEEP_MS,
  JS_TELEMETRY_GC_COMPACT_MS,
  JS_TELEMETRY_GC_MARK_ROOTS_US,
  JS_TELEMETRY_GC_MARK_GRAY_MS_2,
  JS_TELEMETRY_GC_MARK_WEAK_MS,
  JS_TELEMETRY_GC_SLICE_MS,
  JS_TELEMETRY_GC_SLOW_PHASE,
  JS_TELEMETRY_GC_SLOW_TASK,
  JS_TELEMETRY_GC_MMU_50,
  JS_TELEMETRY_GC_RESET,
  JS_TELEMETRY_GC_RESET_REASON,
  JS_TELEMETRY_GC_NON_INCREMENTAL,
  JS_TELEMETRY_GC_NON_INCREMENTAL_REASON,
  JS_TELEMETRY_GC_MINOR_REASON,
  JS_TELEMETRY_GC_MINOR_REASON_LONG,
  JS_TELEMETRY_GC_MINOR_US,
  JS_TELEMETRY_GC_NURSERY_BYTES,
  JS_TELEMETRY_GC_PRETENURE_COUNT_2,
  JS_TELEMETRY_GC_NURSERY_PROMOTION_RATE,
  JS_TELEMETRY_GC_TENURED_SURVIVAL_RATE,
  JS_TELEMETRY_GC_MARK_RATE_2,
  JS_TELEMETRY_GC_TIME_BETWEEN_S,
  JS_TELEMETRY_GC_TIME_BETWEEN_SLICES_MS,
  JS_TELEMETRY_GC_SLICE_COUNT,
  JS_TELEMETRY_GC_EFFECTIVENESS,
  JS_TELEMETRY_PRIVILEGED_PARSER_COMPILE_LAZY_AFTER_MS,
  JS_TELEMETRY_WEB_PARSER_COMPILE_LAZY_AFTER_MS,
  JS_TELEMETRY_END
};

using JSAccumulateTelemetryDataCallback = void (*)(int, uint32_t, const char*);

extern JS_FRIEND_API void JS_SetAccumulateTelemetryCallback(
    JSContext* cx, JSAccumulateTelemetryDataCallback callback);

/*
 * Use counter names passed to the accumulate use counter callback.
 *
 * It's OK to for these enum values to change as they will be mapped to a
 * fixed member of the mozilla::UseCounter enum by the callback.
 */

enum class JSUseCounter { ASMJS, WASM };

using JSSetUseCounterCallback = void (*)(JSObject*, JSUseCounter);

extern JS_FRIEND_API void JS_SetSetUseCounterCallback(
    JSContext* cx, JSSetUseCounterCallback callback);

#endif  // js_friend_UsageStatistics_h
