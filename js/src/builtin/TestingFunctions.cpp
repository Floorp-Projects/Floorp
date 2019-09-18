/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TestingFunctions.h"

#include "mozilla/Atomics.h"
#include "mozilla/Casting.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/Span.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Tuple.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <ctime>

#if defined(XP_UNIX) && !defined(XP_DARWIN)
#  include <time.h>
#else
#  include <chrono>
#endif

#include "jsapi.h"
#include "jsfriendapi.h"

#include "builtin/Promise.h"
#include "builtin/SelfHostingDefines.h"
#ifdef DEBUG
#  include "frontend/TokenStream.h"
#  include "irregexp/RegExpAST.h"
#  include "irregexp/RegExpEngine.h"
#  include "irregexp/RegExpParser.h"
#endif
#include "gc/Heap.h"
#include "gc/Zone.h"
#include "jit/BaselineJIT.h"
#include "jit/InlinableNatives.h"
#include "jit/JitRealm.h"
#include "js/ArrayBuffer.h"  // JS::{DetachArrayBuffer,GetArrayBufferLengthAndData,NewArrayBufferWithContents}
#include "js/CharacterEncoding.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/Date.h"
#include "js/Debug.h"
#include "js/HashTable.h"
#include "js/LocaleSensitive.h"
#include "js/PropertySpec.h"
#include "js/RegExpFlags.h"  // JS::RegExpFlag, JS::RegExpFlags
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "js/StructuredClone.h"
#include "js/UbiNode.h"
#include "js/UbiNodeBreadthFirst.h"
#include "js/UbiNodeShortestPaths.h"
#include "js/UniquePtr.h"
#include "js/Vector.h"
#include "js/Wrapper.h"
#include "threading/CpuCount.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Iteration.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/ProxyObject.h"
#include "vm/SavedStacks.h"
#include "vm/Stack.h"
#include "vm/StringType.h"
#include "vm/TraceLogging.h"
#include "wasm/AsmJS.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmTextToBinary.h"
#include "wasm/WasmTypes.h"

#include "debugger/DebugAPI-inl.h"
#include "vm/Compartment-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/StringType-inl.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::AssertedCast;
using mozilla::AsWritableChars;
using mozilla::MakeSpan;
using mozilla::Maybe;
using mozilla::Tie;
using mozilla::Tuple;

using JS::AutoStableStringChars;
using JS::CompileOptions;
using JS::RegExpFlag;
using JS::RegExpFlags;
using JS::SourceOwnership;
using JS::SourceText;

// If fuzzingSafe is set, remove functionality that could cause problems with
// fuzzers. Set this via the environment variable MOZ_FUZZING_SAFE.
mozilla::Atomic<bool> fuzzingSafe(false);

// If disableOOMFunctions is set, disable functionality that causes artificial
// OOM conditions.
static mozilla::Atomic<bool> disableOOMFunctions(false);

static bool EnvVarIsDefined(const char* name) {
  const char* value = getenv(name);
  return value && *value;
}

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
static bool EnvVarAsInt(const char* name, int* valueOut) {
  if (!EnvVarIsDefined(name)) {
    return false;
  }

  *valueOut = atoi(getenv(name));
  return true;
}
#endif

static bool GetBuildConfiguration(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject info(cx, JS_NewPlainObject(cx));
  if (!info) {
    return false;
  }

  if (!JS_SetProperty(cx, info, "rooting-analysis", FalseHandleValue)) {
    return false;
  }

  if (!JS_SetProperty(cx, info, "exact-rooting", TrueHandleValue)) {
    return false;
  }

  if (!JS_SetProperty(cx, info, "trace-jscalls-api", FalseHandleValue)) {
    return false;
  }

  if (!JS_SetProperty(cx, info, "incremental-gc", TrueHandleValue)) {
    return false;
  }

  if (!JS_SetProperty(cx, info, "generational-gc", TrueHandleValue)) {
    return false;
  }

  RootedValue value(cx);
#ifdef DEBUG
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "debug", value)) {
    return false;
  }

#ifdef RELEASE_OR_BETA
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "release_or_beta", value)) {
    return false;
  }

#ifdef MOZ_CODE_COVERAGE
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "coverage", value)) {
    return false;
  }

#ifdef JS_HAS_CTYPES
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "has-ctypes", value)) {
    return false;
  }

#if defined(_M_IX86) || defined(__i386__)
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "x86", value)) {
    return false;
  }

#if defined(_M_X64) || defined(__x86_64__)
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "x64", value)) {
    return false;
  }

#ifdef JS_CODEGEN_ARM
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "arm", value)) {
    return false;
  }

#ifdef JS_SIMULATOR_ARM
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "arm-simulator", value)) {
    return false;
  }

#ifdef ANDROID
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "android", value)) {
    return false;
  }

#ifdef JS_CODEGEN_ARM64
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "arm64", value)) {
    return false;
  }

#ifdef JS_SIMULATOR_ARM64
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "arm64-simulator", value)) {
    return false;
  }

#ifdef JS_CODEGEN_MIPS32
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "mips32", value)) {
    return false;
  }

#ifdef JS_CODEGEN_MIPS64
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "mips64", value)) {
    return false;
  }

#ifdef JS_SIMULATOR_MIPS32
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "mips32-simulator", value)) {
    return false;
  }

#ifdef JS_SIMULATOR_MIPS64
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "mips64-simulator", value)) {
    return false;
  }

#ifdef MOZ_ASAN
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "asan", value)) {
    return false;
  }

#ifdef MOZ_TSAN
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "tsan", value)) {
    return false;
  }

#ifdef JS_GC_ZEAL
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "has-gczeal", value)) {
    return false;
  }

#ifdef JS_MORE_DETERMINISTIC
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "more-deterministic", value)) {
    return false;
  }

#ifdef MOZ_PROFILING
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "profiling", value)) {
    return false;
  }

#ifdef INCLUDE_MOZILLA_DTRACE
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "dtrace", value)) {
    return false;
  }

#ifdef MOZ_VALGRIND
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "valgrind", value)) {
    return false;
  }

#ifdef JS_OOM_DO_BACKTRACES
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "oom-backtraces", value)) {
    return false;
  }

#ifdef ENABLE_TYPED_OBJECTS
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "typed-objects", value)) {
    return false;
  }

#ifdef ENABLE_INTL_API
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "intl-api", value)) {
    return false;
  }

#if defined(SOLARIS)
  value = BooleanValue(false);
#else
  value = BooleanValue(true);
#endif
  if (!JS_SetProperty(cx, info, "mapped-array-buffer", value)) {
    return false;
  }

#ifdef MOZ_MEMORY
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "moz-memory", value)) {
    return false;
  }

#if defined(JS_BUILD_BINAST)
  value = BooleanValue(true);
#else
  value = BooleanValue(false);
#endif
  if (!JS_SetProperty(cx, info, "binast", value)) {
    return false;
  }

  value.setInt32(sizeof(void*));
  if (!JS_SetProperty(cx, info, "pointer-byte-size", value)) {
    return false;
  }

  args.rval().setObject(*info);
  return true;
}

static bool ReturnStringCopy(JSContext* cx, CallArgs& args,
                             const char* message) {
  JSString* str = JS_NewStringCopyZ(cx, message);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool GC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  /*
   * If the first argument is 'zone', we collect any zones previously
   * scheduled for GC via schedulegc. If the first argument is an object, we
   * collect the object's zone (and any other zones scheduled for
   * GC). Otherwise, we collect all zones.
   */
  bool zone = false;
  if (args.length() >= 1) {
    Value arg = args[0];
    if (arg.isString()) {
      if (!JS_StringEqualsAscii(cx, arg.toString(), "zone", &zone)) {
        return false;
      }
    } else if (arg.isObject()) {
      PrepareZoneForGC(UncheckedUnwrap(&arg.toObject())->zone());
      zone = true;
    }
  }

  bool shrinking = false;
  if (args.length() >= 2) {
    Value arg = args[1];
    if (arg.isString()) {
      if (!JS_StringEqualsAscii(cx, arg.toString(), "shrinking", &shrinking)) {
        return false;
      }
    }
  }

#ifndef JS_MORE_DETERMINISTIC
  size_t preBytes = cx->runtime()->gc.heapSize.bytes();
#endif

  if (zone) {
    PrepareForDebugGC(cx->runtime());
  } else {
    JS::PrepareForFullGC(cx);
  }

  JSGCInvocationKind gckind = shrinking ? GC_SHRINK : GC_NORMAL;
  JS::NonIncrementalGC(cx, gckind, JS::GCReason::API);

  char buf[256] = {'\0'};
#ifndef JS_MORE_DETERMINISTIC
  SprintfLiteral(buf, "before %zu, after %zu\n", preBytes,
                 cx->runtime()->gc.heapSize.bytes());
#endif
  return ReturnStringCopy(cx, args, buf);
}

static bool MinorGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.get(0) == BooleanValue(true)) {
    cx->runtime()->gc.storeBuffer().setAboutToOverflow(
        JS::GCReason::FULL_GENERIC_BUFFER);
  }

  cx->minorGC(JS::GCReason::API);
  args.rval().setUndefined();
  return true;
}

#define FOR_EACH_GC_PARAM(_)                                                 \
  _("maxBytes", JSGC_MAX_BYTES, true)                                        \
  _("minNurseryBytes", JSGC_MIN_NURSERY_BYTES, true)                         \
  _("maxNurseryBytes", JSGC_MAX_NURSERY_BYTES, true)                         \
  _("gcBytes", JSGC_BYTES, false)                                            \
  _("nurseryBytes", JSGC_NURSERY_BYTES, false)                               \
  _("gcNumber", JSGC_NUMBER, false)                                          \
  _("mode", JSGC_MODE, true)                                                 \
  _("unusedChunks", JSGC_UNUSED_CHUNKS, false)                               \
  _("totalChunks", JSGC_TOTAL_CHUNKS, false)                                 \
  _("sliceTimeBudgetMS", JSGC_SLICE_TIME_BUDGET_MS, true)                    \
  _("markStackLimit", JSGC_MARK_STACK_LIMIT, true)                           \
  _("highFrequencyTimeLimit", JSGC_HIGH_FREQUENCY_TIME_LIMIT, true)          \
  _("highFrequencyLowLimit", JSGC_HIGH_FREQUENCY_LOW_LIMIT, true)            \
  _("highFrequencyHighLimit", JSGC_HIGH_FREQUENCY_HIGH_LIMIT, true)          \
  _("highFrequencyHeapGrowthMax", JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX, true) \
  _("highFrequencyHeapGrowthMin", JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN, true) \
  _("lowFrequencyHeapGrowth", JSGC_LOW_FREQUENCY_HEAP_GROWTH, true)          \
  _("dynamicHeapGrowth", JSGC_DYNAMIC_HEAP_GROWTH, true)                     \
  _("dynamicMarkSlice", JSGC_DYNAMIC_MARK_SLICE, true)                       \
  _("allocationThreshold", JSGC_ALLOCATION_THRESHOLD, true)                  \
  _("nonIncrementalFactor", JSGC_NON_INCREMENTAL_FACTOR, true)               \
  _("avoidInterruptFactor", JSGC_AVOID_INTERRUPT_FACTOR, true)               \
  _("minEmptyChunkCount", JSGC_MIN_EMPTY_CHUNK_COUNT, true)                  \
  _("maxEmptyChunkCount", JSGC_MAX_EMPTY_CHUNK_COUNT, true)                  \
  _("compactingEnabled", JSGC_COMPACTING_ENABLED, true)                      \
  _("minLastDitchGCPeriod", JSGC_MIN_LAST_DITCH_GC_PERIOD, true)             \
  _("nurseryFreeThresholdForIdleCollection",                                 \
    JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION, true)                   \
  _("nurseryFreeThresholdForIdleCollectionPercent",                          \
    JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION_PERCENT, true)           \
  _("pretenureThreshold", JSGC_PRETENURE_THRESHOLD, true)                    \
  _("pretenureGroupThreshold", JSGC_PRETENURE_GROUP_THRESHOLD, true)         \
  _("zoneAllocDelayKB", JSGC_ZONE_ALLOC_DELAY_KB, true)                      \
  _("mallocThresholdBase", JSGC_MALLOC_THRESHOLD_BASE, true)                 \
  _("mallocGrowthFactor", JSGC_MALLOC_GROWTH_FACTOR, true)

static const struct ParamInfo {
  const char* name;
  JSGCParamKey param;
  bool writable;
} paramMap[] = {
#define DEFINE_PARAM_INFO(name, key, writable) {name, key, writable},
    FOR_EACH_GC_PARAM(DEFINE_PARAM_INFO)
#undef DEFINE_PARAM_INFO
};

#define PARAM_NAME_LIST_ENTRY(name, key, writable) " " name
#define GC_PARAMETER_ARGS_LIST FOR_EACH_GC_PARAM(PARAM_NAME_LIST_ENTRY)

static bool GCParameter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JSString* str = ToString(cx, args.get(0));
  if (!str) {
    return false;
  }

  JSFlatString* flatStr = JS_FlattenString(cx, str);
  if (!flatStr) {
    return false;
  }

  size_t paramIndex = 0;
  for (;; paramIndex++) {
    if (paramIndex == ArrayLength(paramMap)) {
      JS_ReportErrorASCII(
          cx, "the first argument must be one of:" GC_PARAMETER_ARGS_LIST);
      return false;
    }
    if (JS_FlatStringEqualsAscii(flatStr, paramMap[paramIndex].name)) {
      break;
    }
  }
  const ParamInfo& info = paramMap[paramIndex];
  JSGCParamKey param = info.param;

  // Request mode.
  if (args.length() == 1) {
    uint32_t value = JS_GetGCParameter(cx, param);
    args.rval().setNumber(value);
    return true;
  }

  if (!info.writable) {
    JS_ReportErrorASCII(cx, "Attempt to change read-only parameter %s",
                        info.name);
    return false;
  }

  if (disableOOMFunctions) {
    switch (param) {
      case JSGC_MAX_BYTES:
      case JSGC_MAX_NURSERY_BYTES:
        args.rval().setUndefined();
        return true;
      default:
        break;
    }
  }

  double d;
  if (!ToNumber(cx, args[1], &d)) {
    return false;
  }

  if (d < 0 || d > UINT32_MAX) {
    JS_ReportErrorASCII(cx, "Parameter value out of range");
    return false;
  }

  uint32_t value = floor(d);
  if (param == JSGC_MARK_STACK_LIMIT && JS::IsIncrementalGCInProgress(cx)) {
    JS_ReportErrorASCII(
        cx, "attempt to set markStackLimit while a GC is in progress");
    return false;
  }

  bool ok;
  {
    JSRuntime* rt = cx->runtime();
    AutoLockGC lock(rt);
    ok = rt->gc.setParameter(param, value, lock);
  }

  if (!ok) {
    JS_ReportErrorASCII(cx, "Parameter value out of range");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static void SetAllowRelazification(JSContext* cx, bool allow) {
  JSRuntime* rt = cx->runtime();
  MOZ_ASSERT(rt->allowRelazificationForTesting != allow);
  rt->allowRelazificationForTesting = allow;

  for (AllScriptFramesIter i(cx); !i.done(); ++i) {
    i.script()->setDoNotRelazify(allow);
  }
}

static bool RelazifyFunctions(JSContext* cx, unsigned argc, Value* vp) {
  // Relazifying functions on GC is usually only done for compartments that are
  // not active. To aid fuzzing, this testing function allows us to relazify
  // even if the compartment is active.

  CallArgs args = CallArgsFromVp(argc, vp);
  SetAllowRelazification(cx, true);

  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, GC_SHRINK, JS::GCReason::API);

  SetAllowRelazification(cx, false);
  args.rval().setUndefined();
  return true;
}

static bool IsProxy(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "the function takes exactly one argument");
    return false;
  }
  if (!args[0].isObject()) {
    args.rval().setBoolean(false);
    return true;
  }
  args.rval().setBoolean(args[0].toObject().is<ProxyObject>());
  return true;
}

static bool WasmIsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasSupport(cx));
  return true;
}

static bool WasmIsSupportedByHardware(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasCompilerSupport(cx));
  return true;
}

static bool WasmDebuggingIsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasSupport(cx) && cx->options().wasmBaseline());
  return true;
}

static bool WasmStreamingIsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasStreamingSupport(cx));
  return true;
}

static bool WasmCachingIsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasCachingSupport(cx));
  return true;
}

static bool WasmHugeMemoryIsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef WASM_SUPPORTS_HUGE_MEMORY
  args.rval().setBoolean(true);
#else
  args.rval().setBoolean(false);
#endif
  return true;
}

static bool WasmUsesCranelift(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef ENABLE_WASM_CRANELIFT
  bool usesCranelift = cx->options().wasmCranelift();
#else
  bool usesCranelift = false;
#endif
  args.rval().setBoolean(usesCranelift);
  return true;
}

static bool WasmThreadsSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  bool isSupported = wasm::HasSupport(cx);
#ifdef ENABLE_WASM_CRANELIFT
  if (cx->options().wasmCranelift()) {
    isSupported = false;
  }
#endif
  args.rval().setBoolean(isSupported);
  return true;
}

static bool WasmBulkMemSupported(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef ENABLE_WASM_BULKMEM_OPS
  bool isSupported = true;
#  ifdef ENABLE_WASM_CRANELIFT
  if (cx->options().wasmCranelift()) {
    isSupported = false;
  }
#  endif
#else
  bool isSupported =
      cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled();
#endif
  args.rval().setBoolean(isSupported);
  return true;
}

static bool WasmReftypesEnabled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasReftypesSupport(cx));
  return true;
}

static bool WasmGcEnabled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(wasm::HasGcSupport(cx));
  return true;
}

static bool WasmDebugSupport(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(cx->options().wasmBaseline() &&
                         wasm::BaselineCanCompile());
  return true;
}

static bool WasmCompileMode(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool baseline = cx->options().wasmBaseline();
  bool ion = cx->options().wasmIon();
#ifdef ENABLE_WASM_CRANELIFT
  bool cranelift = cx->options().wasmCranelift();
#else
  bool cranelift = false;
#endif

  // We default to ion if nothing is enabled, as does the Wasm compiler.
  JSString* result;
  if (!wasm::HasSupport(cx)) {
    result = JS_NewStringCopyZ(cx, "none");
  } else if (baseline && ion) {
    result = JS_NewStringCopyZ(cx, "baseline+ion");
  } else if (baseline && cranelift) {
    result = JS_NewStringCopyZ(cx, "baseline+cranelift");
  } else if (baseline) {
    result = JS_NewStringCopyZ(cx, "baseline");
  } else if (cranelift) {
    result = JS_NewStringCopyZ(cx, "cranelift");
  } else {
    result = JS_NewStringCopyZ(cx, "ion");
  }

  if (!result) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

static bool WasmTextToBinary(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.requireAtLeast(cx, "wasmTextToBinary", 1)) {
    return false;
  }

  if (!args[0].isString()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a String");
    return false;
  }

  AutoStableStringChars twoByteChars(cx);
  if (!twoByteChars.initTwoByte(cx, args[0].toString())) {
    return false;
  }

  bool withOffsets = false;
  if (args.hasDefined(1)) {
    if (!args[1].isBoolean()) {
      ReportUsageErrorASCII(cx, callee,
                            "Second argument, if present, must be a boolean");
      return false;
    }
    withOffsets = ToBoolean(args[1]);
  }

  uintptr_t stackLimit = GetNativeStackLimit(cx);

  wasm::Bytes bytes;
  UniqueChars error;
  wasm::Uint32Vector offsets;
  if (!wasm::TextToBinary(twoByteChars.twoByteChars(), stackLimit, &bytes,
                          &offsets, &error)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WASM_TEXT_FAIL,
                              error.get() ? error.get() : "out of memory");
    return false;
  }

  RootedObject binary(cx, JS_NewUint8Array(cx, bytes.length()));
  if (!binary) {
    return false;
  }

  memcpy(binary->as<TypedArrayObject>().dataPointerUnshared(), bytes.begin(),
         bytes.length());

  if (!withOffsets) {
    args.rval().setObject(*binary);
    return true;
  }

  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return false;
  }

  constexpr unsigned propAttrs = JSPROP_ENUMERATE;
  if (!JS_DefineProperty(cx, obj, "binary", binary, propAttrs)) {
    return false;
  }

  RootedObject jsOffsets(cx, JS_NewArrayObject(cx, offsets.length()));
  if (!jsOffsets) {
    return false;
  }
  for (size_t i = 0; i < offsets.length(); i++) {
    uint32_t offset = offsets[i];
    RootedValue offsetVal(cx, NumberValue(offset));
    if (!JS_SetElement(cx, jsOffsets, i, offsetVal)) {
      return false;
    }
  }
  if (!JS_DefineProperty(cx, obj, "offsets", jsOffsets, propAttrs)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool ConvertToTier(JSContext* cx, HandleValue value,
                          const wasm::Code& code, wasm::Tier* tier) {
  RootedString option(cx, JS::ToString(cx, value));

  if (!option) {
    return false;
  }

  bool stableTier = false;
  bool bestTier = false;
  bool baselineTier = false;
  bool ionTier = false;

  if (!JS_StringEqualsAscii(cx, option, "stable", &stableTier) ||
      !JS_StringEqualsAscii(cx, option, "best", &bestTier) ||
      !JS_StringEqualsAscii(cx, option, "baseline", &baselineTier) ||
      !JS_StringEqualsAscii(cx, option, "ion", &ionTier)) {
    return false;
  }

  if (stableTier) {
    *tier = code.stableTier();
  } else if (bestTier) {
    *tier = code.bestTier();
  } else if (baselineTier) {
    *tier = wasm::Tier::Baseline;
  } else if (ionTier) {
    *tier = wasm::Tier::Optimized;
  } else {
    // You can omit the argument but you can't pass just anything you like
    return false;
  }

  return true;
}

static bool WasmExtractCode(JSContext* cx, unsigned argc, Value* vp) {
  if (!cx->options().wasm()) {
    JS_ReportErrorASCII(cx, "wasm support unavailable");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "argument is not an object");
    return false;
  }

  Rooted<WasmModuleObject*> module(
      cx, args[0].toObject().maybeUnwrapIf<WasmModuleObject>());
  if (!module) {
    JS_ReportErrorASCII(cx, "argument is not a WebAssembly.Module");
    return false;
  }

  wasm::Tier tier = module->module().code().stableTier();
  ;
  if (args.length() > 1 &&
      !ConvertToTier(cx, args[1], module->module().code(), &tier)) {
    args.rval().setNull();
    return false;
  }

  RootedValue result(cx);
  if (!module->module().extractCode(cx, tier, &result)) {
    return false;
  }

  args.rval().set(result);
  return true;
}

static bool WasmDisassemble(JSContext* cx, unsigned argc, Value* vp) {
  if (!cx->options().wasm()) {
    JS_ReportErrorASCII(cx, "wasm support unavailable");
    return false;
  }

  CallArgs args = CallArgsFromVp(argc, vp);

  args.rval().set(UndefinedValue());

  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "argument is not an object");
    return false;
  }

  RootedFunction func(cx, args[0].toObject().maybeUnwrapIf<JSFunction>());

  if (!func || !wasm::IsWasmExportedFunction(func)) {
    JS_ReportErrorASCII(cx, "argument is not an exported wasm function");
    return false;
  }

  wasm::Instance& instance = wasm::ExportedFunctionToInstance(func);
  uint32_t funcIndex = wasm::ExportedFunctionToFuncIndex(func);

  wasm::Tier tier = instance.code().stableTier();

  if (args.length() > 1 &&
      !ConvertToTier(cx, args[1], instance.code(), &tier)) {
    JS_ReportErrorASCII(cx, "invalid tier");
    return false;
  }

  if (!instance.code().hasTier(tier)) {
    JS_ReportErrorASCII(cx, "function missing selected tier");
    return false;
  }

  instance.disassembleExport(cx, funcIndex, tier, [](const char* text) {
    fprintf(stderr, "%s\n", text);
  });

  return true;
}

enum class Flag { Tier2Complete, Deserialized };

static bool WasmReturnFlag(JSContext* cx, unsigned argc, Value* vp, Flag flag) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "argument is not an object");
    return false;
  }

  Rooted<WasmModuleObject*> module(
      cx, args[0].toObject().maybeUnwrapIf<WasmModuleObject>());
  if (!module) {
    JS_ReportErrorASCII(cx, "argument is not a WebAssembly.Module");
    return false;
  }

  bool b;
  switch (flag) {
    case Flag::Tier2Complete:
      b = !module->module().testingTier2Active();
      break;
    case Flag::Deserialized:
      b = module->module().loggingDeserialized();
      break;
  }

  args.rval().set(BooleanValue(b));
  return true;
}

static bool WasmHasTier2CompilationCompleted(JSContext* cx, unsigned argc,
                                             Value* vp) {
  return WasmReturnFlag(cx, argc, vp, Flag::Tier2Complete);
}

static bool WasmLoadedFromCache(JSContext* cx, unsigned argc, Value* vp) {
  return WasmReturnFlag(cx, argc, vp, Flag::Deserialized);
}

static bool IsLazyFunction(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "The function takes exactly one argument.");
    return false;
  }
  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "The first argument should be a function.");
    return false;
  }
  args.rval().setBoolean(
      args[0].toObject().as<JSFunction>().isInterpretedLazy());
  return true;
}

static bool IsRelazifiableFunction(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "The function takes exactly one argument.");
    return false;
  }
  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "The first argument should be a function.");
    return false;
  }

  JSFunction* fun = &args[0].toObject().as<JSFunction>();
  args.rval().setBoolean(fun->hasScript() &&
                         fun->nonLazyScript()->isRelazifiableIgnoringJitCode());
  return true;
}

static bool InternalConst(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() == 0) {
    JS_ReportErrorASCII(cx, "the function takes exactly one argument");
    return false;
  }

  JSString* str = ToString(cx, args[0]);
  if (!str) {
    return false;
  }
  JSFlatString* flat = JS_FlattenString(cx, str);
  if (!flat) {
    return false;
  }

  if (JS_FlatStringEqualsAscii(flat, "INCREMENTAL_MARK_STACK_BASE_CAPACITY")) {
    args.rval().setNumber(uint32_t(js::INCREMENTAL_MARK_STACK_BASE_CAPACITY));
  } else {
    JS_ReportErrorASCII(cx, "unknown const name");
    return false;
  }
  return true;
}

static bool GCPreserveCode(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 0) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  cx->runtime()->gc.setAlwaysPreserveCode();

  args.rval().setUndefined();
  return true;
}

#ifdef JS_GC_ZEAL

static bool ParseGCZealMode(JSContext* cx, const CallArgs& args,
                            uint8_t* zeal) {
  uint32_t value;
  if (!ToUint32(cx, args.get(0), &value)) {
    return false;
  }

  if (value > uint32_t(gc::ZealMode::Limit)) {
    JS_ReportErrorASCII(cx, "gczeal argument out of range");
    return false;
  }

  *zeal = static_cast<uint8_t>(value);
  return true;
}

static bool GCZeal(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 2) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  uint8_t zeal;
  if (!ParseGCZealMode(cx, args, &zeal)) {
    return false;
  }

  uint32_t frequency = JS_DEFAULT_ZEAL_FREQ;
  if (args.length() >= 2) {
    if (!ToUint32(cx, args.get(1), &frequency)) {
      return false;
    }
  }

  JS_SetGCZeal(cx, zeal, frequency);
  args.rval().setUndefined();
  return true;
}

static bool UnsetGCZeal(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  uint8_t zeal;
  if (!ParseGCZealMode(cx, args, &zeal)) {
    return false;
  }

  JS_UnsetGCZeal(cx, zeal);
  args.rval().setUndefined();
  return true;
}

static bool ScheduleGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  if (args.length() == 0) {
    /* Fetch next zeal trigger only. */
  } else if (args[0].isNumber()) {
    /* Schedule a GC to happen after |arg| allocations. */
    JS_ScheduleGC(cx, std::max(int(args[0].toNumber()), 0));
  } else {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Bad argument - expecting number");
    return false;
  }

  uint32_t zealBits;
  uint32_t freq;
  uint32_t next;
  JS_GetGCZealBits(cx, &zealBits, &freq, &next);
  args.rval().setInt32(next);
  return true;
}

static bool SelectForGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  /*
   * The selectedForMarking set is intended to be manually marked at slice
   * start to detect missing pre-barriers. It is invalid for nursery things
   * to be in the set, so evict the nursery before adding items.
   */
  cx->runtime()->gc.evictNursery();

  for (unsigned i = 0; i < args.length(); i++) {
    if (args[i].isObject()) {
      if (!cx->runtime()->gc.selectForMarking(&args[i].toObject())) {
        return false;
      }
    }
  }

  args.rval().setUndefined();
  return true;
}

static bool VerifyPreBarriers(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 0) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  gc::VerifyBarriers(cx->runtime(), gc::PreBarrierVerifier);
  args.rval().setUndefined();
  return true;
}

static bool VerifyPostBarriers(JSContext* cx, unsigned argc, Value* vp) {
  // This is a no-op since the post barrier verifier was removed.
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length()) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }
  args.rval().setUndefined();
  return true;
}

static bool CurrentGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 0) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) {
    return false;
  }

  js::gc::GCRuntime& gc = cx->runtime()->gc;
  const char* state = StateName(gc.state());

  RootedString str(cx, JS_NewStringCopyZ(cx, state));
  if (!str) {
    return false;
  }
  RootedValue val(cx, StringValue(str));
  if (!JS_DefineProperty(cx, result, "incrementalState", val,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  if (gc.state() == js::gc::State::Sweep) {
    val = Int32Value(gc.getCurrentSweepGroupIndex());
    if (!JS_DefineProperty(cx, result, "sweepGroup", val, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  val = BooleanValue(gc.isShrinkingGC());
  if (!JS_DefineProperty(cx, result, "isShrinking", val, JSPROP_ENUMERATE)) {
    return false;
  }

  val = Int32Value(gc.gcNumber());
  if (!JS_DefineProperty(cx, result, "number", val, JSPROP_ENUMERATE)) {
    return false;
  }

  val = Int32Value(gc.minorGCCount());
  if (!JS_DefineProperty(cx, result, "minorCount", val, JSPROP_ENUMERATE)) {
    return false;
  }

  val = Int32Value(gc.majorGCCount());
  if (!JS_DefineProperty(cx, result, "majorCount", val, JSPROP_ENUMERATE)) {
    return false;
  }

  val = BooleanValue(gc.isFullGc());
  if (!JS_DefineProperty(cx, result, "isFull", val, JSPROP_ENUMERATE)) {
    return false;
  }

  val = BooleanValue(gc.isCompactingGc());
  if (!JS_DefineProperty(cx, result, "isCompacting", val, JSPROP_ENUMERATE)) {
    return false;
  }

#  ifdef DEBUG
  val = Int32Value(gc.marker.queuePos);
  if (!JS_DefineProperty(cx, result, "queuePos", val, JSPROP_ENUMERATE)) {
    return false;
  }
#  endif

  args.rval().setObject(*result);
  return true;
}

static bool DeterministicGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  cx->runtime()->gc.setDeterministic(ToBoolean(args[0]));
  args.rval().setUndefined();
  return true;
}

static bool DumpGCArenaInfo(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  js::gc::DumpArenaInfo();
  args.rval().setUndefined();
  return true;
}

#endif /* JS_GC_ZEAL */

static bool GCState(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  const char* state;

  if (args.length() == 1) {
    if (!args[0].isObject()) {
      RootedObject callee(cx, &args.callee());
      ReportUsageErrorASCII(cx, callee, "Expected object");
      return false;
    }

    JSObject* obj = UncheckedUnwrap(&args[0].toObject());
    state = gc::StateName(obj->zone()->gcState());
  } else {
    state = gc::StateName(cx->runtime()->gc.state());
  }

  return ReturnStringCopy(cx, args, state);
}

static bool ScheduleZoneForGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Expecting a single argument");
    return false;
  }

  if (args[0].isObject()) {
    // Ensure that |zone| is collected during the next GC.
    Zone* zone = UncheckedUnwrap(&args[0].toObject())->zone();
    PrepareZoneForGC(zone);
  } else if (args[0].isString()) {
    // This allows us to schedule the atoms zone for GC.
    Zone* zone = args[0].toString()->zoneFromAnyThread();
    if (!CurrentThreadCanAccessZone(zone)) {
      RootedObject callee(cx, &args.callee());
      ReportUsageErrorASCII(cx, callee, "Specified zone not accessible for GC");
      return false;
    }
    PrepareZoneForGC(zone);
  } else {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee,
                          "Bad argument - expecting object or string");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool StartGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 2) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  auto budget = SliceBudget::unlimited();
  if (args.length() >= 1) {
    uint32_t work = 0;
    if (!ToUint32(cx, args[0], &work)) {
      return false;
    }
    budget = SliceBudget(WorkBudget(work));
  }

  bool shrinking = false;
  if (args.length() >= 2) {
    Value arg = args[1];
    if (arg.isString()) {
      if (!JS_StringEqualsAscii(cx, arg.toString(), "shrinking", &shrinking)) {
        return false;
      }
    }
  }

  JSRuntime* rt = cx->runtime();
  if (rt->gc.isIncrementalGCInProgress()) {
    RootedObject callee(cx, &args.callee());
    JS_ReportErrorASCII(cx, "Incremental GC already in progress");
    return false;
  }

  JSGCInvocationKind gckind = shrinking ? GC_SHRINK : GC_NORMAL;
  rt->gc.startDebugGC(gckind, budget);

  args.rval().setUndefined();
  return true;
}

static bool FinishGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 0) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  JSRuntime* rt = cx->runtime();
  if (rt->gc.isIncrementalGCInProgress()) {
    rt->gc.finishGC(JS::GCReason::DEBUG_GC);
  }

  args.rval().setUndefined();
  return true;
}

static bool GCSlice(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  auto budget = SliceBudget::unlimited();
  if (args.length() == 1) {
    uint32_t work = 0;
    if (!ToUint32(cx, args[0], &work)) {
      return false;
    }
    budget = SliceBudget(WorkBudget(work));
  }

  JSRuntime* rt = cx->runtime();
  if (!rt->gc.isIncrementalGCInProgress()) {
    rt->gc.startDebugGC(GC_NORMAL, budget);
  } else {
    rt->gc.debugGCSlice(budget);
  }

  args.rval().setUndefined();
  return true;
}

static bool AbortGC(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 0) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  JS::AbortIncrementalGC(cx);
  args.rval().setUndefined();
  return true;
}

static bool FullCompartmentChecks(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  cx->runtime()->gc.setFullCompartmentChecks(ToBoolean(args[0]));
  args.rval().setUndefined();
  return true;
}

static bool NondeterministicGetWeakMapKeys(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }
  if (!args[0].isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE,
                              "nondeterministicGetWeakMapKeys", "WeakMap",
                              InformalValueTypeName(args[0]));
    return false;
  }
  RootedObject arr(cx);
  RootedObject mapObj(cx, &args[0].toObject());
  if (!JS_NondeterministicGetWeakMapKeys(cx, mapObj, &arr)) {
    return false;
  }
  if (!arr) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE,
                              "nondeterministicGetWeakMapKeys", "WeakMap",
                              args[0].toObject().getClass()->name);
    return false;
  }
  args.rval().setObject(*arr);
  return true;
}

class HasChildTracer final : public JS::CallbackTracer {
  RootedValue child_;
  bool found_;

  bool onChild(const JS::GCCellPtr& thing) override {
    if (thing.asCell() == child_.toGCThing()) {
      found_ = true;
    }
    return true;
  }

 public:
  HasChildTracer(JSContext* cx, HandleValue child)
      : JS::CallbackTracer(cx, TraceWeakMapKeysValues),
        child_(cx, child),
        found_(false) {}

  bool found() const { return found_; }
};

static bool HasChild(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedValue parent(cx, args.get(0));
  RootedValue child(cx, args.get(1));

  if (!parent.isGCThing() || !child.isGCThing()) {
    args.rval().setBoolean(false);
    return true;
  }

  HasChildTracer trc(cx, child);
  TraceChildren(&trc, parent.toGCThing(), parent.traceKind());
  args.rval().setBoolean(trc.found());
  return true;
}

static bool SetSavedStacksRNGState(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "setSavedStacksRNGState", 1)) {
    return false;
  }

  int32_t seed;
  if (!ToInt32(cx, args[0], &seed)) {
    return false;
  }

  // Either one or the other of the seed arguments must be non-zero;
  // make this true no matter what value 'seed' has.
  cx->realm()->savedStacks().setRNGState(seed, (seed + 1) * 33);
  return true;
}

static bool GetSavedFrameCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setNumber(cx->realm()->savedStacks().count());
  return true;
}

static bool ClearSavedFrames(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::SavedStacks& savedStacks = cx->realm()->savedStacks();
  savedStacks.clear();

  for (ActivationIterator iter(cx); !iter.done(); ++iter) {
    iter->clearLiveSavedFrameCache();
  }

  args.rval().setUndefined();
  return true;
}

static bool SaveStack(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JS::StackCapture capture((JS::AllFrames()));
  if (args.length() >= 1) {
    double maxDouble;
    if (!ToNumber(cx, args[0], &maxDouble)) {
      return false;
    }
    if (mozilla::IsNaN(maxDouble) || maxDouble < 0 || maxDouble > UINT32_MAX) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                       nullptr, "not a valid maximum frame count");
      return false;
    }
    uint32_t max = uint32_t(maxDouble);
    if (max > 0) {
      capture = JS::StackCapture(JS::MaxFrames(max));
    }
  }

  RootedObject compartmentObject(cx);
  if (args.length() >= 2) {
    if (!args[1].isObject()) {
      ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                       nullptr, "not an object");
      return false;
    }
    compartmentObject = UncheckedUnwrap(&args[1].toObject());
    if (!compartmentObject) {
      return false;
    }
  }

  RootedObject stack(cx);
  {
    Maybe<AutoRealm> ar;
    if (compartmentObject) {
      ar.emplace(cx, compartmentObject);
    }
    if (!JS::CaptureCurrentStack(cx, &stack, std::move(capture))) {
      return false;
    }
  }

  if (stack && !cx->compartment()->wrap(cx, &stack)) {
    return false;
  }

  args.rval().setObjectOrNull(stack);
  return true;
}

static bool CaptureFirstSubsumedFrame(JSContext* cx, unsigned argc,
                                      JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "captureFirstSubsumedFrame", 1)) {
    return false;
  }

  if (!args[0].isObject()) {
    JS_ReportErrorASCII(cx, "The argument must be an object");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  obj = CheckedUnwrapStatic(obj);
  if (!obj) {
    JS_ReportErrorASCII(cx, "Denied permission to object.");
    return false;
  }

  JS::StackCapture capture(
      JS::FirstSubsumedFrame(cx, obj->nonCCWRealm()->principals()));
  if (args.length() > 1) {
    capture.as<JS::FirstSubsumedFrame>().ignoreSelfHosted =
        JS::ToBoolean(args[1]);
  }

  JS::RootedObject capturedStack(cx);
  if (!JS::CaptureCurrentStack(cx, &capturedStack, std::move(capture))) {
    return false;
  }

  args.rval().setObjectOrNull(capturedStack);
  return true;
}

static bool CallFunctionFromNativeFrame(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "The function takes exactly one argument.");
    return false;
  }
  if (!args[0].isObject() || !IsCallable(args[0])) {
    JS_ReportErrorASCII(cx, "The first argument should be a function.");
    return false;
  }

  RootedObject function(cx, &args[0].toObject());
  return Call(cx, UndefinedHandleValue, function, JS::HandleValueArray::empty(),
              args.rval());
}

static bool CallFunctionWithAsyncStack(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 3) {
    JS_ReportErrorASCII(cx, "The function takes exactly three arguments.");
    return false;
  }
  if (!args[0].isObject() || !IsCallable(args[0])) {
    JS_ReportErrorASCII(cx, "The first argument should be a function.");
    return false;
  }
  if (!args[1].isObject() || !args[1].toObject().is<SavedFrame>()) {
    JS_ReportErrorASCII(cx, "The second argument should be a SavedFrame.");
    return false;
  }
  if (!args[2].isString() || args[2].toString()->empty()) {
    JS_ReportErrorASCII(cx, "The third argument should be a non-empty string.");
    return false;
  }

  RootedObject function(cx, &args[0].toObject());
  RootedObject stack(cx, &args[1].toObject());
  RootedString asyncCause(cx, args[2].toString());
  UniqueChars utf8Cause = JS_EncodeStringToUTF8(cx, asyncCause);
  if (!utf8Cause) {
    MOZ_ASSERT(cx->isExceptionPending());
    return false;
  }

  JS::AutoSetAsyncStackForNewCalls sas(
      cx, stack, utf8Cause.get(),
      JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::EXPLICIT);
  return Call(cx, UndefinedHandleValue, function, JS::HandleValueArray::empty(),
              args.rval());
}

static bool EnableTrackAllocations(JSContext* cx, unsigned argc, Value* vp) {
  SetAllocationMetadataBuilder(cx, &SavedStacks::metadataBuilder);
  return true;
}

static bool DisableTrackAllocations(JSContext* cx, unsigned argc, Value* vp) {
  SetAllocationMetadataBuilder(cx, nullptr);
  return true;
}

static void FinalizeExternalString(const JSStringFinalizer* fin,
                                   char16_t* chars);

static const JSStringFinalizer ExternalStringFinalizer = {
    FinalizeExternalString};

static void FinalizeExternalString(const JSStringFinalizer* fin,
                                   char16_t* chars) {
  MOZ_ASSERT(fin == &ExternalStringFinalizer);
  js_free(chars);
}

static bool NewExternalString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorASCII(cx,
                        "newExternalString takes exactly one string argument.");
    return false;
  }

  RootedString str(cx, args[0].toString());
  size_t len = str->length();

  auto buf = cx->make_pod_array<char16_t>(len);
  if (!buf) {
    return false;
  }

  if (!JS_CopyStringChars(cx, mozilla::Range<char16_t>(buf.get(), len), str)) {
    return false;
  }

  JSString* res =
      JS_NewExternalString(cx, buf.get(), len, &ExternalStringFinalizer);
  if (!res) {
    return false;
  }

  mozilla::Unused << buf.release();
  args.rval().setString(res);
  return true;
}

static bool NewMaybeExternalString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorASCII(
        cx, "newMaybeExternalString takes exactly one string argument.");
    return false;
  }

  RootedString str(cx, args[0].toString());
  size_t len = str->length();

  auto buf = cx->make_pod_array<char16_t>(len);
  if (!buf) {
    return false;
  }

  if (!JS_CopyStringChars(cx, mozilla::Range<char16_t>(buf.get(), len), str)) {
    return false;
  }

  bool allocatedExternal;
  JSString* res = JS_NewMaybeExternalString(
      cx, buf.get(), len, &ExternalStringFinalizer, &allocatedExternal);
  if (!res) {
    return false;
  }

  if (allocatedExternal) {
    mozilla::Unused << buf.release();
  }
  args.rval().setString(res);
  return true;
}

// Warning! This will let you create ropes that I'm not sure would be possible
// otherwise, specifically:
//
//   - a rope with a zero-length child
//   - a rope that would fit into an inline string
//
static bool NewRope(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.get(0).isString() || !args.get(1).isString()) {
    JS_ReportErrorASCII(cx, "newRope requires two string arguments.");
    return false;
  }

  gc::InitialHeap heap = js::gc::DefaultHeap;
  if (args.get(2).isObject()) {
    RootedObject options(cx, &args[2].toObject());
    RootedValue v(cx);
    if (!JS_GetProperty(cx, options, "nursery", &v)) {
      return false;
    }
    if (!v.isUndefined() && !ToBoolean(v)) {
      heap = js::gc::TenuredHeap;
    }
  }

  RootedString left(cx, args[0].toString());
  RootedString right(cx, args[1].toString());
  size_t length = JS_GetStringLength(left) + JS_GetStringLength(right);
  if (length > JSString::MAX_LENGTH) {
    JS_ReportErrorASCII(cx, "rope length exceeds maximum string length");
    return false;
  }

  Rooted<JSRope*> str(cx, JSRope::new_<CanGC>(cx, left, right, length, heap));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool IsRope(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(cx, "isRope requires a string argument.");
    return false;
  }

  JSString* str = args[0].toString();
  args.rval().setBoolean(str->isRope());
  return true;
}

static bool EnsureFlatString(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorASCII(cx,
                        "ensureFlatString takes exactly one string argument.");
    return false;
  }

  JSFlatString* flat = args[0].toString()->ensureFlat(cx);
  if (!flat) {
    return false;
  }

  args.rval().setString(flat);
  return true;
}

static bool RepresentativeStringArray(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject array(cx, JS_NewArrayObject(cx, 0));
  if (!array) {
    return false;
  }

  if (!JSString::fillWithRepresentatives(cx, array.as<ArrayObject>())) {
    return false;
  }

  args.rval().setObject(*array);
  return true;
}

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

static bool OOMThreadTypes(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setInt32(js::THREAD_TYPE_MAX);
  return true;
}

static bool CheckCanSimulateOOM(JSContext* cx) {
  if (js::oom::GetThreadType() != js::THREAD_TYPE_MAIN) {
    JS_ReportErrorASCII(
        cx, "Simulated OOM failure is only supported on the main thread");
    return false;
  }

  return true;
}

static bool SetupOOMFailure(JSContext* cx, bool failAlways, unsigned argc,
                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (disableOOMFunctions) {
    args.rval().setUndefined();
    return true;
  }

  if (args.length() < 1) {
    JS_ReportErrorASCII(cx, "Count argument required");
    return false;
  }

  if (args.length() > 2) {
    JS_ReportErrorASCII(cx, "Too many arguments");
    return false;
  }

  int32_t count;
  if (!JS::ToInt32(cx, args.get(0), &count)) {
    return false;
  }

  if (count <= 0) {
    JS_ReportErrorASCII(cx, "OOM cutoff should be positive");
    return false;
  }

  uint32_t targetThread = js::THREAD_TYPE_MAIN;
  if (args.length() > 1 && !ToUint32(cx, args[1], &targetThread)) {
    return false;
  }

  if (targetThread == js::THREAD_TYPE_NONE ||
      targetThread == js::THREAD_TYPE_WORKER ||
      targetThread >= js::THREAD_TYPE_MAX) {
    JS_ReportErrorASCII(cx, "Invalid thread type specified");
    return false;
  }

  if (!CheckCanSimulateOOM(cx)) {
    return false;
  }

  js::oom::simulator.simulateFailureAfter(js::oom::FailureSimulator::Kind::OOM,
                                          count, targetThread, failAlways);
  args.rval().setUndefined();
  return true;
}

static bool OOMAfterAllocations(JSContext* cx, unsigned argc, Value* vp) {
  return SetupOOMFailure(cx, true, argc, vp);
}

static bool OOMAtAllocation(JSContext* cx, unsigned argc, Value* vp) {
  return SetupOOMFailure(cx, false, argc, vp);
}

static bool ResetOOMFailure(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!CheckCanSimulateOOM(cx)) {
    return false;
  }

  args.rval().setBoolean(js::oom::HadSimulatedOOM());
  js::oom::simulator.reset();
  return true;
}

static size_t CountCompartments(JSContext* cx) {
  size_t count = 0;
  for (auto zone : cx->runtime()->gc.zones()) {
    count += zone->compartments().length();
  }
  return count;
}

// Iterative failure testing: test a function by simulating failures at indexed
// locations throughout the normal execution path and checking that the
// resulting state of the environment is consistent with the error result.
//
// For example, trigger OOM at every allocation point and test that the function
// either recovers and succeeds or raises an exception and fails.

struct MOZ_STACK_CLASS IterativeFailureTestParams {
  explicit IterativeFailureTestParams(JSContext* cx) : testFunction(cx) {}

  RootedFunction testFunction;
  unsigned threadStart = 0;
  unsigned threadEnd = 0;
  bool expectExceptionOnFailure = true;
  bool keepFailing = false;
  bool verbose = false;
};

struct IterativeFailureSimulator {
  virtual void setup(JSContext* cx) {}
  virtual void teardown(JSContext* cx) {}
  virtual void startSimulating(JSContext* cx, unsigned iteration,
                               unsigned thread, bool keepFailing) = 0;
  virtual bool stopSimulating() = 0;
  virtual void cleanup(JSContext* cx) {}
};

bool RunIterativeFailureTest(JSContext* cx,
                             const IterativeFailureTestParams& params,
                             IterativeFailureSimulator& simulator) {
  if (disableOOMFunctions) {
    return true;
  }

  if (!CheckCanSimulateOOM(cx)) {
    return false;
  }

  // Disallow nested tests.
  if (cx->runningOOMTest) {
    JS_ReportErrorASCII(
        cx, "Nested call to iterative failure test is not allowed.");
    return false;
  }
  cx->runningOOMTest = true;

  MOZ_ASSERT(!cx->isExceptionPending());

#  ifdef JS_GC_ZEAL
  JS_SetGCZeal(cx, 0, JS_DEFAULT_ZEAL_FREQ);
#  endif

  size_t compartmentCount = CountCompartments(cx);

  RootedValue exception(cx);

  simulator.setup(cx);

  for (unsigned thread = params.threadStart; thread <= params.threadEnd;
       thread++) {
    if (params.verbose) {
      fprintf(stderr, "thread %d\n", thread);
    }

    unsigned iteration = 1;
    bool failureWasSimulated;
    do {
      if (params.verbose) {
        fprintf(stderr, "  iteration %d\n", iteration);
      }

      MOZ_ASSERT(!cx->isExceptionPending());

      simulator.startSimulating(cx, iteration, thread, params.keepFailing);

      RootedValue result(cx);
      bool ok = JS_CallFunction(cx, cx->global(), params.testFunction,
                                HandleValueArray::empty(), &result);

      failureWasSimulated = simulator.stopSimulating();

      if (ok) {
        MOZ_ASSERT(!cx->isExceptionPending(),
                   "Thunk execution succeeded but an exception was raised - "
                   "missing error check?");
      } else if (params.expectExceptionOnFailure) {
        MOZ_ASSERT(cx->isExceptionPending(),
                   "Thunk execution failed but no exception was raised - "
                   "missing call to js::ReportOutOfMemory()?");
      }

      // Note that it is possible that the function throws an exception
      // unconnected to the simulated failure, in which case we ignore
      // it. More correct would be to have the caller pass some kind of
      // exception specification and to check the exception against it.

      if (!failureWasSimulated && cx->isExceptionPending()) {
        if (!cx->getPendingException(&exception)) {
          return false;
        }
      }
      cx->clearPendingException();
      simulator.cleanup(cx);

      gc::FinishGC(cx);

      // Some tests create a new compartment or zone on every
      // iteration. Our GC is triggered by GC allocations and not by
      // number of compartments or zones, so these won't normally get
      // cleaned up. The check here stops some tests running out of
      // memory. ("Gentlemen, you can't fight in here! This is the
      // War oom!")
      if (CountCompartments(cx) > compartmentCount + 100) {
        JS_GC(cx);
        compartmentCount = CountCompartments(cx);
      }

#  ifdef JS_TRACE_LOGGING
      // Reset the TraceLogger state if enabled.
      TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
      if (logger && logger->enabled()) {
        while (logger->enabled()) {
          logger->disable();
        }
        logger->enable(cx);
      }
#  endif

      iteration++;
    } while (failureWasSimulated);

    if (params.verbose) {
      fprintf(stderr, "  finished after %d iterations\n", iteration - 1);
      if (!exception.isUndefined()) {
        RootedString str(cx, JS::ToString(cx, exception));
        if (!str) {
          fprintf(stderr,
                  "  error while trying to print exception, giving up\n");
          return false;
        }
        UniqueChars bytes(JS_EncodeStringToLatin1(cx, str));
        if (!bytes) {
          return false;
        }
        fprintf(stderr, "  threw %s\n", bytes.get());
      }
    }
  }

  simulator.teardown(cx);

  cx->runningOOMTest = false;
  return true;
}

bool ParseIterativeFailureTestParams(JSContext* cx, const CallArgs& args,
                                     IterativeFailureTestParams* params) {
  MOZ_ASSERT(params);

  if (args.length() < 1 || args.length() > 2) {
    JS_ReportErrorASCII(cx, "function takes between 1 and 2 arguments.");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "The first argument must be the function to test.");
    return false;
  }
  params->testFunction = &args[0].toObject().as<JSFunction>();

  if (args.length() == 2) {
    if (args[1].isBoolean()) {
      params->expectExceptionOnFailure = args[1].toBoolean();
    } else if (args[1].isObject()) {
      RootedObject options(cx, &args[1].toObject());
      RootedValue value(cx);

      if (!JS_GetProperty(cx, options, "expectExceptionOnFailure", &value)) {
        return false;
      }
      if (!value.isUndefined()) {
        params->expectExceptionOnFailure = ToBoolean(value);
      }

      if (!JS_GetProperty(cx, options, "keepFailing", &value)) {
        return false;
      }
      if (!value.isUndefined()) {
        params->keepFailing = ToBoolean(value);
      }
    } else {
      JS_ReportErrorASCII(
          cx, "The optional second argument must be an object or a boolean.");
      return false;
    }
  }

  // There are some places where we do fail without raising an exception, so
  // we can't expose this to the fuzzers by default.
  if (fuzzingSafe) {
    params->expectExceptionOnFailure = false;
  }

  // Test all threads by default except worker threads.
  params->threadStart = oom::FirstThreadTypeToTest;
  params->threadEnd = oom::LastThreadTypeToTest;

  // Test a single thread type if specified by the OOM_THREAD environment
  // variable.
  int threadOption = 0;
  if (EnvVarAsInt("OOM_THREAD", &threadOption)) {
    if (threadOption < oom::FirstThreadTypeToTest ||
        threadOption > oom::LastThreadTypeToTest) {
      JS_ReportErrorASCII(cx, "OOM_THREAD value out of range.");
      return false;
    }

    params->threadStart = threadOption;
    params->threadEnd = threadOption;
  }

  params->verbose = EnvVarIsDefined("OOM_VERBOSE");

  return true;
}

struct OOMSimulator : public IterativeFailureSimulator {
  void setup(JSContext* cx) override { cx->runtime()->hadOutOfMemory = false; }

  void startSimulating(JSContext* cx, unsigned i, unsigned thread,
                       bool keepFailing) override {
    MOZ_ASSERT(!cx->runtime()->hadOutOfMemory);
    js::oom::simulator.simulateFailureAfter(
        js::oom::FailureSimulator::Kind::OOM, i, thread, keepFailing);
  }

  bool stopSimulating() override {
    bool handledOOM = js::oom::HadSimulatedOOM();
    js::oom::simulator.reset();
    return handledOOM;
  }

  void cleanup(JSContext* cx) override {
    cx->runtime()->hadOutOfMemory = false;
  }
};

static bool OOMTest(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  IterativeFailureTestParams params(cx);
  if (!ParseIterativeFailureTestParams(cx, args, &params)) {
    return false;
  }

  OOMSimulator simulator;
  if (!RunIterativeFailureTest(cx, params, simulator)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

struct StackOOMSimulator : public IterativeFailureSimulator {
  void startSimulating(JSContext* cx, unsigned i, unsigned thread,
                       bool keepFailing) override {
    js::oom::simulator.simulateFailureAfter(
        js::oom::FailureSimulator::Kind::StackOOM, i, thread, keepFailing);
  }

  bool stopSimulating() override {
    bool handledOOM = js::oom::HadSimulatedStackOOM();
    js::oom::simulator.reset();
    return handledOOM;
  }
};

static bool StackTest(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  IterativeFailureTestParams params(cx);
  if (!ParseIterativeFailureTestParams(cx, args, &params)) {
    return false;
  }

  StackOOMSimulator simulator;
  if (!RunIterativeFailureTest(cx, params, simulator)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

struct FailingIterruptSimulator : public IterativeFailureSimulator {
  JSInterruptCallback* prevEnd = nullptr;

  static bool failingInterruptCallback(JSContext* cx) { return false; }

  void setup(JSContext* cx) override {
    prevEnd = cx->interruptCallbacks().end();
    JS_AddInterruptCallback(cx, failingInterruptCallback);
  }

  void teardown(JSContext* cx) override {
    cx->interruptCallbacks().erase(prevEnd, cx->interruptCallbacks().end());
  }

  void startSimulating(JSContext* cx, unsigned i, unsigned thread,
                       bool keepFailing) override {
    js::oom::simulator.simulateFailureAfter(
        js::oom::FailureSimulator::Kind::Interrupt, i, thread, keepFailing);
  }

  bool stopSimulating() override {
    bool handledInterrupt = js::oom::HadSimulatedInterrupt();
    js::oom::simulator.reset();
    return handledInterrupt;
  }
};

static bool InterruptTest(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  IterativeFailureTestParams params(cx);
  if (!ParseIterativeFailureTestParams(cx, args, &params)) {
    return false;
  }

  FailingIterruptSimulator simulator;
  if (!RunIterativeFailureTest(cx, params, simulator)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

#endif  // defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

static bool SettlePromiseNow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "settlePromiseNow", 1)) {
    return false;
  }
  if (!args[0].isObject() || !args[0].toObject().is<PromiseObject>()) {
    JS_ReportErrorASCII(cx, "first argument must be a Promise object");
    return false;
  }

  Rooted<PromiseObject*> promise(cx, &args[0].toObject().as<PromiseObject>());
  if (IsPromiseForAsyncFunctionOrGenerator(promise)) {
    JS_ReportErrorASCII(
        cx, "async function/generator's promise shouldn't be manually settled");
    return false;
  }

  if (promise->state() != JS::PromiseState::Pending) {
    JS_ReportErrorASCII(cx, "cannot settle an already-resolved promise");
    return false;
  }

  int32_t flags = promise->flags();
  promise->setFixedSlot(
      PromiseSlot_Flags,
      Int32Value(flags | PROMISE_FLAG_RESOLVED | PROMISE_FLAG_FULFILLED));
  promise->setFixedSlot(PromiseSlot_ReactionsOrResult, UndefinedValue());

  DebugAPI::onPromiseSettled(cx, promise);
  return true;
}

static bool GetWaitForAllPromise(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "getWaitForAllPromise", 1)) {
    return false;
  }
  if (!args[0].isObject() || !IsPackedArray(&args[0].toObject())) {
    JS_ReportErrorASCII(
        cx, "first argument must be a dense Array of Promise objects");
    return false;
  }
  RootedNativeObject list(cx, &args[0].toObject().as<NativeObject>());
  RootedObjectVector promises(cx);
  uint32_t count = list->getDenseInitializedLength();
  if (!promises.resize(count)) {
    return false;
  }

  for (uint32_t i = 0; i < count; i++) {
    RootedValue elem(cx, list->getDenseElement(i));
    if (!elem.isObject() || !elem.toObject().is<PromiseObject>()) {
      JS_ReportErrorASCII(
          cx, "Each entry in the passed-in Array must be a Promise");
      return false;
    }
    promises[i].set(&elem.toObject());
  }

  RootedObject resultPromise(cx, JS::GetWaitForAllPromise(cx, promises));
  if (!resultPromise) {
    return false;
  }

  args.rval().set(ObjectValue(*resultPromise));
  return true;
}

static bool ResolvePromise(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "resolvePromise", 2)) {
    return false;
  }
  if (!args[0].isObject() ||
      !UncheckedUnwrap(&args[0].toObject())->is<PromiseObject>()) {
    JS_ReportErrorASCII(
        cx, "first argument must be a maybe-wrapped Promise object");
    return false;
  }

  RootedObject promise(cx, &args[0].toObject());
  RootedValue resolution(cx, args[1]);
  mozilla::Maybe<AutoRealm> ar;
  if (IsWrapper(promise)) {
    promise = UncheckedUnwrap(promise);
    ar.emplace(cx, promise);
    if (!cx->compartment()->wrap(cx, &resolution)) {
      return false;
    }
  }

  if (IsPromiseForAsyncFunctionOrGenerator(promise)) {
    JS_ReportErrorASCII(
        cx,
        "async function/generator's promise shouldn't be manually resolved");
    return false;
  }

  bool result = JS::ResolvePromise(cx, promise, resolution);
  if (result) {
    args.rval().setUndefined();
  }
  return result;
}

static bool RejectPromise(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "rejectPromise", 2)) {
    return false;
  }
  if (!args[0].isObject() ||
      !UncheckedUnwrap(&args[0].toObject())->is<PromiseObject>()) {
    JS_ReportErrorASCII(
        cx, "first argument must be a maybe-wrapped Promise object");
    return false;
  }

  RootedObject promise(cx, &args[0].toObject());
  RootedValue reason(cx, args[1]);
  mozilla::Maybe<AutoRealm> ar;
  if (IsWrapper(promise)) {
    promise = UncheckedUnwrap(promise);
    ar.emplace(cx, promise);
    if (!cx->compartment()->wrap(cx, &reason)) {
      return false;
    }
  }

  if (IsPromiseForAsyncFunctionOrGenerator(promise)) {
    JS_ReportErrorASCII(
        cx,
        "async function/generator's promise shouldn't be manually rejected");
    return false;
  }

  bool result = JS::RejectPromise(cx, promise, reason);
  if (result) {
    args.rval().setUndefined();
  }
  return result;
}

static bool StreamsAreEnabled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(cx->realm()->creationOptions().getStreamsEnabled());
  return true;
}

static unsigned finalizeCount = 0;

static void finalize_counter_finalize(JSFreeOp* fop, JSObject* obj) {
  ++finalizeCount;
}

static const JSClassOps FinalizeCounterClassOps = {nullptr, /* addProperty */
                                                   nullptr, /* delProperty */
                                                   nullptr, /* enumerate */
                                                   nullptr, /* newEnumerate */
                                                   nullptr, /* resolve */
                                                   nullptr, /* mayResolve */
                                                   finalize_counter_finalize};

static const JSClass FinalizeCounterClass = {
    "FinalizeCounter", JSCLASS_FOREGROUND_FINALIZE, &FinalizeCounterClassOps};

static bool MakeFinalizeObserver(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  JSObject* obj =
      JS_NewObjectWithGivenProto(cx, &FinalizeCounterClass, nullptr);
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool FinalizeCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setInt32(finalizeCount);
  return true;
}

static bool ResetFinalizeCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  finalizeCount = 0;
  args.rval().setUndefined();
  return true;
}

static bool DumpHeap(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  FILE* dumpFile = stdout;

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  if (!args.get(0).isUndefined()) {
    RootedString str(cx, ToString(cx, args[0]));
    if (!str) {
      return false;
    }
    if (!fuzzingSafe) {
      UniqueChars fileNameBytes = JS_EncodeStringToLatin1(cx, str);
      if (!fileNameBytes) {
        return false;
      }
      dumpFile = fopen(fileNameBytes.get(), "w");
      if (!dumpFile) {
        fileNameBytes = JS_EncodeStringToLatin1(cx, str);
        if (!fileNameBytes) {
          return false;
        }
        JS_ReportErrorLatin1(cx, "can't open %s", fileNameBytes.get());
        return false;
      }
    }
  }

  js::DumpHeap(cx, dumpFile, js::IgnoreNurseryObjects);

  if (dumpFile != stdout) {
    fclose(dumpFile);
  }

  args.rval().setUndefined();
  return true;
}

static bool Terminate(JSContext* cx, unsigned arg, Value* vp) {
#ifdef JS_MORE_DETERMINISTIC
  // Print a message to stderr in more-deterministic builds to help jsfunfuzz
  // find uncatchable-exception bugs.
  fprintf(stderr, "terminate called\n");
#endif

  JS_ClearPendingException(cx);
  return false;
}

static bool ReadGeckoProfilingStack(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();

  // Return boolean 'false' if profiler is not enabled.
  if (!cx->runtime()->geckoProfiler().enabled()) {
    args.rval().setBoolean(false);
    return true;
  }

  // Array holding physical jit stack frames.
  RootedObject stack(cx, NewDenseEmptyArray(cx));
  if (!stack) {
    return false;
  }

  // If profiler sampling has been suppressed, return an empty
  // stack.
  if (!cx->isProfilerSamplingEnabled()) {
    args.rval().setObject(*stack);
    return true;
  }

  struct InlineFrameInfo {
    InlineFrameInfo(const char* kind, UniqueChars label)
        : kind(kind), label(std::move(label)) {}
    const char* kind;
    UniqueChars label;
  };

  Vector<Vector<InlineFrameInfo, 0, TempAllocPolicy>, 0, TempAllocPolicy>
      frameInfo(cx);

  JS::ProfilingFrameIterator::RegisterState state;
  for (JS::ProfilingFrameIterator i(cx, state); !i.done(); ++i) {
    MOZ_ASSERT(i.stackAddress() != nullptr);

    if (!frameInfo.emplaceBack(cx)) {
      return false;
    }

    const size_t MaxInlineFrames = 16;
    JS::ProfilingFrameIterator::Frame frames[MaxInlineFrames];
    uint32_t nframes = i.extractStack(frames, 0, MaxInlineFrames);
    MOZ_ASSERT(nframes <= MaxInlineFrames);
    for (uint32_t i = 0; i < nframes; i++) {
      const char* frameKindStr = nullptr;
      switch (frames[i].kind) {
        case JS::ProfilingFrameIterator::Frame_BaselineInterpreter:
          frameKindStr = "baseline-interpreter";
          break;
        case JS::ProfilingFrameIterator::Frame_Baseline:
          frameKindStr = "baseline-jit";
          break;
        case JS::ProfilingFrameIterator::Frame_Ion:
          frameKindStr = "ion";
          break;
        case JS::ProfilingFrameIterator::Frame_Wasm:
          frameKindStr = "wasm";
          break;
        default:
          frameKindStr = "unknown";
      }

      UniqueChars label =
          DuplicateStringToArena(js::StringBufferArena, cx, frames[i].label);
      if (!label) {
        return false;
      }

      if (!frameInfo.back().emplaceBack(frameKindStr, std::move(label))) {
        return false;
      }
    }
  }

  RootedObject inlineFrameInfo(cx);
  RootedString frameKind(cx);
  RootedString frameLabel(cx);
  RootedId idx(cx);

  const unsigned propAttrs = JSPROP_ENUMERATE;

  uint32_t physicalFrameNo = 0;
  for (auto& frame : frameInfo) {
    // Array holding all inline frames in a single physical jit stack frame.
    RootedObject inlineStack(cx, NewDenseEmptyArray(cx));
    if (!inlineStack) {
      return false;
    }

    uint32_t inlineFrameNo = 0;
    for (auto& inlineFrame : frame) {
      // Object holding frame info.
      RootedObject inlineFrameInfo(cx,
                                   NewBuiltinClassInstance<PlainObject>(cx));
      if (!inlineFrameInfo) {
        return false;
      }

      frameKind = NewStringCopyZ<CanGC>(cx, inlineFrame.kind);
      if (!frameKind) {
        return false;
      }

      if (!JS_DefineProperty(cx, inlineFrameInfo, "kind", frameKind,
                             propAttrs)) {
        return false;
      }

      frameLabel = NewLatin1StringZ(cx, std::move(inlineFrame.label));
      if (!frameLabel) {
        return false;
      }

      if (!JS_DefineProperty(cx, inlineFrameInfo, "label", frameLabel,
                             propAttrs)) {
        return false;
      }

      idx = INT_TO_JSID(inlineFrameNo);
      if (!JS_DefinePropertyById(cx, inlineStack, idx, inlineFrameInfo, 0)) {
        return false;
      }

      ++inlineFrameNo;
    }

    // Push inline array into main array.
    idx = INT_TO_JSID(physicalFrameNo);
    if (!JS_DefinePropertyById(cx, stack, idx, inlineStack, 0)) {
      return false;
    }

    ++physicalFrameNo;
  }

  args.rval().setObject(*stack);
  return true;
}

static bool EnableOsiPointRegisterChecks(JSContext*, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef CHECK_OSIPOINT_REGISTERS
  jit::JitOptions.checkOsiPointRegisters = true;
#endif
  args.rval().setUndefined();
  return true;
}

static bool DisplayName(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.get(0).isObject() || !args[0].toObject().is<JSFunction>()) {
    RootedObject arg(cx, &args.callee());
    ReportUsageErrorASCII(cx, arg, "Must have one function argument");
    return false;
  }

  JSFunction* fun = &args[0].toObject().as<JSFunction>();
  JSString* str = fun->displayAtom();
  args.rval().setString(str ? str : cx->runtime()->emptyString.ref());
  return true;
}

class ShellAllocationMetadataBuilder : public AllocationMetadataBuilder {
 public:
  ShellAllocationMetadataBuilder() : AllocationMetadataBuilder() {}

  virtual JSObject* build(JSContext* cx, HandleObject,
                          AutoEnterOOMUnsafeRegion& oomUnsafe) const override;

  static const ShellAllocationMetadataBuilder metadataBuilder;
};

JSObject* ShellAllocationMetadataBuilder::build(
    JSContext* cx, HandleObject, AutoEnterOOMUnsafeRegion& oomUnsafe) const {
  RootedObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
  if (!obj) {
    oomUnsafe.crash("ShellAllocationMetadataBuilder::build");
  }

  RootedObject stack(cx, NewDenseEmptyArray(cx));
  if (!stack) {
    oomUnsafe.crash("ShellAllocationMetadataBuilder::build");
  }

  static int createdIndex = 0;
  createdIndex++;

  if (!JS_DefineProperty(cx, obj, "index", createdIndex, 0)) {
    oomUnsafe.crash("ShellAllocationMetadataBuilder::build");
  }

  if (!JS_DefineProperty(cx, obj, "stack", stack, 0)) {
    oomUnsafe.crash("ShellAllocationMetadataBuilder::build");
  }

  int stackIndex = 0;
  RootedId id(cx);
  RootedValue callee(cx);
  for (NonBuiltinScriptFrameIter iter(cx); !iter.done(); ++iter) {
    if (iter.isFunctionFrame() && iter.compartment() == cx->compartment()) {
      id = INT_TO_JSID(stackIndex);
      RootedObject callee(cx, iter.callee(cx));
      if (!JS_DefinePropertyById(cx, stack, id, callee, 0)) {
        oomUnsafe.crash("ShellAllocationMetadataBuilder::build");
      }
      stackIndex++;
    }
  }

  return obj;
}

const ShellAllocationMetadataBuilder
    ShellAllocationMetadataBuilder::metadataBuilder;

static bool EnableShellAllocationMetadataBuilder(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  SetAllocationMetadataBuilder(
      cx, &ShellAllocationMetadataBuilder::metadataBuilder);

  args.rval().setUndefined();
  return true;
}

static bool GetAllocationMetadata(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || !args[0].isObject()) {
    JS_ReportErrorASCII(cx, "Argument must be an object");
    return false;
  }

  args.rval().setObjectOrNull(GetAllocationMetadata(&args[0].toObject()));
  return true;
}

static bool testingFunc_bailout(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // NOP when not in IonMonkey
  args.rval().setUndefined();
  return true;
}

static bool testingFunc_bailAfter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || !args[0].isInt32() || args[0].toInt32() < 0) {
    JS_ReportErrorASCII(
        cx, "Argument must be a positive number that fits in an int32");
    return false;
  }

#ifdef DEBUG
  if (auto* jitRuntime = cx->runtime()->jitRuntime()) {
    jitRuntime->setIonBailAfter(args[0].toInt32());
  }
#endif

  args.rval().setUndefined();
  return true;
}

static constexpr unsigned JitWarmupResetLimit = 20;
static_assert(JitWarmupResetLimit <=
                  unsigned(JSScript::MutableFlags::WarmupResets_MASK),
              "JitWarmupResetLimit exceeds max value");

static bool testingFunc_inJit(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!jit::IsBaselineJitEnabled()) {
    return ReturnStringCopy(cx, args, "Baseline is disabled.");
  }

  // Use frame iterator to inspect caller.
  FrameIter iter(cx);
  MOZ_ASSERT(!iter.done());

  if (iter.hasScript()) {
    // Detect repeated attempts to compile, resetting the counter if inJit
    // succeeds. Note: This script may have be inlined into its caller.
    if (iter.isJSJit()) {
      iter.script()->resetWarmUpResetCounter();
    } else if (iter.script()->getWarmUpResetCount() >= JitWarmupResetLimit) {
      return ReturnStringCopy(
          cx, args, "Compilation is being repeatedly prevented. Giving up.");
    }
  }

  // Returns true for any JIT (including WASM).
  MOZ_ASSERT_IF(iter.isJSJit(), cx->currentlyRunningInJit());
  args.rval().setBoolean(cx->currentlyRunningInJit());
  return true;
}

static bool testingFunc_inIon(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!jit::IsIonEnabled()) {
    return ReturnStringCopy(cx, args, "Ion is disabled.");
  }

  // Use frame iterator to inspect caller.
  FrameIter iter(cx);
  MOZ_ASSERT(!iter.done());

  if (iter.hasScript()) {
    // Detect repeated attempts to compile, resetting the counter if inIon
    // succeeds. Note: This script may have be inlined into its caller.
    if (iter.isIon()) {
      iter.script()->resetWarmUpResetCounter();
    } else if (!iter.script()->canIonCompile()) {
      return ReturnStringCopy(cx, args, "Unable to Ion-compile this script.");
    } else if (iter.script()->getWarmUpResetCount() >= JitWarmupResetLimit) {
      return ReturnStringCopy(
          cx, args, "Compilation is being repeatedly prevented. Giving up.");
    }
  }

  args.rval().setBoolean(iter.isIon());
  return true;
}

bool js::testingFunc_assertFloat32(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportErrorASCII(cx, "Expects only 2 arguments");
    return false;
  }

  // NOP when not in IonMonkey
  args.rval().setUndefined();
  return true;
}

static bool TestingFunc_assertJitStackInvariants(JSContext* cx, unsigned argc,
                                                 Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  jit::AssertJitStackInvariants(cx);
  args.rval().setUndefined();
  return true;
}

bool js::testingFunc_assertRecoveredOnBailout(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportErrorASCII(cx, "Expects only 2 arguments");
    return false;
  }

  // NOP when not in IonMonkey
  args.rval().setUndefined();
  return true;
}

static bool GetJitCompilerOptions(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject info(cx, JS_NewPlainObject(cx));
  if (!info) {
    return false;
  }

  uint32_t intValue = 0;
  RootedValue value(cx);

#define JIT_COMPILER_MATCH(key, string)                         \
  opt = JSJITCOMPILER_##key;                                    \
  if (JS_GetGlobalJitCompilerOption(cx, opt, &intValue)) {      \
    value.setInt32(intValue);                                   \
    if (!JS_SetProperty(cx, info, string, value)) return false; \
  }

  JSJitCompilerOption opt = JSJITCOMPILER_NOT_AN_OPTION;
  JIT_COMPILER_OPTIONS(JIT_COMPILER_MATCH);
#undef JIT_COMPILER_MATCH

  args.rval().setObject(*info);
  return true;
}

static bool SetIonCheckGraphCoherency(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  jit::JitOptions.checkGraphConsistency = ToBoolean(args.get(0));
  args.rval().setUndefined();
  return true;
}

// A JSObject that holds structured clone data, similar to the C++ class
// JSAutoStructuredCloneBuffer.
class CloneBufferObject : public NativeObject {
  static const JSPropertySpec props_[3];

  static const size_t DATA_SLOT = 0;
  static const size_t SYNTHETIC_SLOT = 1;
  static const size_t NUM_SLOTS = 2;

 public:
  static const JSClass class_;

  static CloneBufferObject* Create(JSContext* cx) {
    RootedObject obj(cx, JS_NewObject(cx, &class_));
    if (!obj) {
      return nullptr;
    }
    obj->as<CloneBufferObject>().setReservedSlot(DATA_SLOT,
                                                 PrivateValue(nullptr));
    obj->as<CloneBufferObject>().setReservedSlot(SYNTHETIC_SLOT,
                                                 BooleanValue(false));

    if (!JS_DefineProperties(cx, obj, props_)) {
      return nullptr;
    }

    return &obj->as<CloneBufferObject>();
  }

  static CloneBufferObject* Create(JSContext* cx,
                                   JSAutoStructuredCloneBuffer* buffer) {
    Rooted<CloneBufferObject*> obj(cx, Create(cx));
    if (!obj) {
      return nullptr;
    }
    auto data = js::MakeUnique<JSStructuredCloneData>(buffer->scope());
    if (!data) {
      ReportOutOfMemory(cx);
      return nullptr;
    }
    buffer->steal(data.get());
    obj->setData(data.release(), false);
    return obj;
  }

  JSStructuredCloneData* data() const {
    return static_cast<JSStructuredCloneData*>(
        getReservedSlot(DATA_SLOT).toPrivate());
  }

  bool isSynthetic() const {
    return getReservedSlot(SYNTHETIC_SLOT).toBoolean();
  }

  void setData(JSStructuredCloneData* aData, bool synthetic) {
    MOZ_ASSERT(!data());
    setReservedSlot(DATA_SLOT, PrivateValue(aData));
    setReservedSlot(SYNTHETIC_SLOT, BooleanValue(synthetic));
  }

  // Discard an owned clone buffer.
  void discard() {
    js_delete(data());
    setReservedSlot(DATA_SLOT, PrivateValue(nullptr));
  }

  static bool setCloneBuffer_impl(JSContext* cx, const CallArgs& args) {
    Rooted<CloneBufferObject*> obj(
        cx, &args.thisv().toObject().as<CloneBufferObject>());

    const char* data = nullptr;
    UniqueChars dataOwner;
    uint32_t nbytes;

    if (args.get(0).isObject() && args[0].toObject().is<ArrayBufferObject>()) {
      ArrayBufferObject* buffer = &args[0].toObject().as<ArrayBufferObject>();
      bool isSharedMemory;
      uint8_t* dataBytes = nullptr;
      JS::GetArrayBufferLengthAndData(buffer, &nbytes, &isSharedMemory,
                                      &dataBytes);
      MOZ_ASSERT(!isSharedMemory);
      data = reinterpret_cast<char*>(dataBytes);
    } else {
      JSString* str = JS::ToString(cx, args.get(0));
      if (!str) {
        return false;
      }
      dataOwner = JS_EncodeStringToLatin1(cx, str);
      if (!dataOwner) {
        return false;
      }
      data = dataOwner.get();
      nbytes = JS_GetStringLength(str);
    }

    if (nbytes == 0 || (nbytes % sizeof(uint64_t) != 0)) {
      JS_ReportErrorASCII(cx, "Invalid length for clonebuffer data");
      return false;
    }

    auto buf = js::MakeUnique<JSStructuredCloneData>(
        JS::StructuredCloneScope::DifferentProcess);
    if (!buf || !buf->Init(nbytes)) {
      ReportOutOfMemory(cx);
      return false;
    }

    MOZ_ALWAYS_TRUE(buf->AppendBytes(data, nbytes));
    obj->discard();
    obj->setData(buf.release(), true);

    args.rval().setUndefined();
    return true;
  }

  static bool is(HandleValue v) {
    return v.isObject() && v.toObject().is<CloneBufferObject>();
  }

  static bool setCloneBuffer(JSContext* cx, unsigned int argc, JS::Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, setCloneBuffer_impl>(cx, args);
  }

  static bool getData(JSContext* cx, Handle<CloneBufferObject*> obj,
                      JSStructuredCloneData** data) {
    if (!obj->data()) {
      *data = nullptr;
      return true;
    }

    bool hasTransferable;
    if (!JS_StructuredCloneHasTransferables(*obj->data(), &hasTransferable)) {
      return false;
    }

    if (hasTransferable) {
      JS_ReportErrorASCII(
          cx, "cannot retrieve structured clone buffer with transferables");
      return false;
    }

    *data = obj->data();
    return true;
  }

  static bool getCloneBuffer_impl(JSContext* cx, const CallArgs& args) {
    Rooted<CloneBufferObject*> obj(
        cx, &args.thisv().toObject().as<CloneBufferObject>());
    MOZ_ASSERT(args.length() == 0);

    JSStructuredCloneData* data;
    if (!getData(cx, obj, &data)) {
      return false;
    }

    size_t size = data->Size();
    UniqueChars buffer(js_pod_malloc<char>(size));
    if (!buffer) {
      ReportOutOfMemory(cx);
      return false;
    }
    auto iter = data->Start();
    if (!data->ReadBytes(iter, buffer.get(), size)) {
      ReportOutOfMemory(cx);
      return false;
    }
    JSString* str = JS_NewStringCopyN(cx, buffer.get(), size);
    if (!str) {
      return false;
    }
    args.rval().setString(str);
    return true;
  }

  static bool getCloneBuffer(JSContext* cx, unsigned int argc, JS::Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getCloneBuffer_impl>(cx, args);
  }

  static bool getCloneBufferAsArrayBuffer_impl(JSContext* cx,
                                               const CallArgs& args) {
    Rooted<CloneBufferObject*> obj(
        cx, &args.thisv().toObject().as<CloneBufferObject>());
    MOZ_ASSERT(args.length() == 0);

    JSStructuredCloneData* data;
    if (!getData(cx, obj, &data)) {
      return false;
    }

    size_t size = data->Size();
    UniqueChars buffer(js_pod_malloc<char>(size));
    if (!buffer) {
      ReportOutOfMemory(cx);
      return false;
    }
    auto iter = data->Start();
    if (!data->ReadBytes(iter, buffer.get(), size)) {
      ReportOutOfMemory(cx);
      return false;
    }

    auto* rawBuffer = buffer.release();
    JSObject* arrayBuffer = JS::NewArrayBufferWithContents(cx, size, rawBuffer);
    if (!arrayBuffer) {
      js_free(rawBuffer);
      return false;
    }

    args.rval().setObject(*arrayBuffer);
    return true;
  }

  static bool getCloneBufferAsArrayBuffer(JSContext* cx, unsigned int argc,
                                          JS::Value* vp) {
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<is, getCloneBufferAsArrayBuffer_impl>(cx, args);
  }

  static void Finalize(JSFreeOp* fop, JSObject* obj) {
    obj->as<CloneBufferObject>().discard();
  }
};

static const JSClassOps CloneBufferObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    CloneBufferObject::Finalize};

const JSClass CloneBufferObject::class_ = {
    "CloneBuffer",
    JSCLASS_HAS_RESERVED_SLOTS(CloneBufferObject::NUM_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &CloneBufferObjectClassOps};

const JSPropertySpec CloneBufferObject::props_[] = {
    JS_PSGS("clonebuffer", getCloneBuffer, setCloneBuffer, 0),
    JS_PSGS("arraybuffer", getCloneBufferAsArrayBuffer, setCloneBuffer, 0),
    JS_PS_END};

static mozilla::Maybe<JS::StructuredCloneScope> ParseCloneScope(
    JSContext* cx, HandleString str) {
  mozilla::Maybe<JS::StructuredCloneScope> scope;

  JSLinearString* scopeStr = str->ensureLinear(cx);
  if (!scopeStr) {
    return scope;
  }

  if (StringEqualsAscii(scopeStr, "SameProcessSameThread")) {
    scope.emplace(JS::StructuredCloneScope::SameProcessSameThread);
  } else if (StringEqualsAscii(scopeStr, "SameProcessDifferentThread")) {
    scope.emplace(JS::StructuredCloneScope::SameProcessDifferentThread);
  } else if (StringEqualsAscii(scopeStr, "DifferentProcess")) {
    scope.emplace(JS::StructuredCloneScope::DifferentProcess);
  } else if (StringEqualsAscii(scopeStr, "DifferentProcessForIndexedDB")) {
    scope.emplace(JS::StructuredCloneScope::DifferentProcessForIndexedDB);
  }

  return scope;
}

bool js::testingFunc_serialize(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  mozilla::Maybe<JSAutoStructuredCloneBuffer> clonebuf;
  JS::CloneDataPolicy policy;

  if (!args.get(2).isUndefined()) {
    RootedObject opts(cx, ToObject(cx, args.get(2)));
    if (!opts) {
      return false;
    }

    RootedValue v(cx);
    if (!JS_GetProperty(cx, opts, "SharedArrayBuffer", &v)) {
      return false;
    }

    if (!v.isUndefined()) {
      JSString* str = JS::ToString(cx, v);
      if (!str) {
        return false;
      }
      JSLinearString* poli = str->ensureLinear(cx);
      if (!poli) {
        return false;
      }

      if (StringEqualsAscii(poli, "allow")) {
        // default
      } else if (StringEqualsAscii(poli, "deny")) {
        policy.denySharedArrayBuffer();
      } else {
        JS_ReportErrorASCII(cx, "Invalid policy value for 'SharedArrayBuffer'");
        return false;
      }
    }

    if (!JS_GetProperty(cx, opts, "scope", &v)) {
      return false;
    }

    if (!v.isUndefined()) {
      RootedString str(cx, JS::ToString(cx, v));
      if (!str) {
        return false;
      }
      auto scope = ParseCloneScope(cx, str);
      if (!scope) {
        JS_ReportErrorASCII(cx, "Invalid structured clone scope");
        return false;
      }
      clonebuf.emplace(*scope, nullptr, nullptr);
    }
  }

  if (!clonebuf) {
    clonebuf.emplace(JS::StructuredCloneScope::SameProcessSameThread, nullptr,
                     nullptr);
  }

  if (!clonebuf->write(cx, args.get(0), args.get(1), policy)) {
    return false;
  }

  RootedObject obj(cx, CloneBufferObject::Create(cx, clonebuf.ptr()));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool Deserialize(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!args.get(0).isObject() || !args[0].toObject().is<CloneBufferObject>()) {
    JS_ReportErrorASCII(cx, "deserialize requires a clonebuffer argument");
    return false;
  }
  Rooted<CloneBufferObject*> obj(cx,
                                 &args[0].toObject().as<CloneBufferObject>());

  JS::StructuredCloneScope scope =
      obj->isSynthetic() ? JS::StructuredCloneScope::DifferentProcess
                         : JS::StructuredCloneScope::SameProcessSameThread;
  if (args.get(1).isObject()) {
    RootedObject opts(cx, &args[1].toObject());
    if (!opts) {
      return false;
    }

    RootedValue v(cx);
    if (!JS_GetProperty(cx, opts, "scope", &v)) {
      return false;
    }

    if (!v.isUndefined()) {
      RootedString str(cx, JS::ToString(cx, v));
      if (!str) {
        return false;
      }
      auto maybeScope = ParseCloneScope(cx, str);
      if (!maybeScope) {
        JS_ReportErrorASCII(cx, "Invalid structured clone scope");
        return false;
      }

      if (*maybeScope < scope) {
        JS_ReportErrorASCII(cx,
                            "Cannot use less restrictive scope "
                            "than the deserialized clone buffer's scope");
        return false;
      }

      scope = *maybeScope;
    }
  }

  // Clone buffer was already consumed?
  if (!obj->data()) {
    JS_ReportErrorASCII(cx,
                        "deserialize given invalid clone buffer "
                        "(transferables already consumed?)");
    return false;
  }

  bool hasTransferable;
  if (!JS_StructuredCloneHasTransferables(*obj->data(), &hasTransferable)) {
    return false;
  }

  RootedValue deserialized(cx);
  if (!JS_ReadStructuredClone(cx, *obj->data(), JS_STRUCTURED_CLONE_VERSION,
                              scope, &deserialized, nullptr, nullptr)) {
    return false;
  }
  args.rval().set(deserialized);

  // Consume any clone buffer with transferables; throw an error if it is
  // deserialized again.
  if (hasTransferable) {
    obj->discard();
  }

  return true;
}

static bool DetachArrayBuffer(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "detachArrayBuffer() requires a single argument");
    return false;
  }

  if (!args[0].isObject()) {
    JS_ReportErrorASCII(cx, "detachArrayBuffer must be passed an object");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  if (!JS::DetachArrayBuffer(cx, obj)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool HelperThreadCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef JS_MORE_DETERMINISTIC
  // Always return 0 to get consistent output with and without --no-threads.
  args.rval().setInt32(0);
#else
  if (CanUseExtraThreads()) {
    args.rval().setInt32(HelperThreadState().threadCount);
  } else {
    args.rval().setInt32(0);
  }
#endif
  return true;
}

static bool EnableShapeConsistencyChecks(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
#ifdef DEBUG
  NativeObject::enableShapeConsistencyChecks();
#endif
  args.rval().setUndefined();
  return true;
}

#ifdef JS_TRACE_LOGGING
static bool EnableTraceLogger(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  if (!TraceLoggerEnable(logger, cx)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool DisableTraceLogger(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  args.rval().setBoolean(TraceLoggerDisable(logger));

  return true;
}
#endif

#if defined(DEBUG) || defined(JS_JITSPEW)
static bool DumpObject(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject obj(cx, ToObject(cx, args.get(0)));
  if (!obj) {
    return false;
  }

  DumpObject(obj);

  args.rval().setUndefined();
  return true;
}
#endif

static bool SharedMemoryEnabled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setBoolean(
      cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled());
  return true;
}

static bool SharedArrayRawBufferCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setInt32(LiveMappedBufferCount());
  return true;
}

static bool SharedArrayRawBufferRefcount(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1 || !args[0].isObject()) {
    JS_ReportErrorASCII(cx, "Expected SharedArrayBuffer object");
    return false;
  }
  RootedObject obj(cx, &args[0].toObject());
  if (!obj->is<SharedArrayBufferObject>()) {
    JS_ReportErrorASCII(cx, "Expected SharedArrayBuffer object");
    return false;
  }
  args.rval().setInt32(
      obj->as<SharedArrayBufferObject>().rawBufferObject()->refcount());
  return true;
}

#ifdef NIGHTLY_BUILD
static bool ObjectAddress(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }
  if (!args[0].isObject()) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Expected object");
    return false;
  }

#  ifdef JS_MORE_DETERMINISTIC
  args.rval().setInt32(0);
  return true;
#  else
  void* ptr = js::UncheckedUnwrap(&args[0].toObject(), true);
  char buffer[64];
  SprintfLiteral(buffer, "%p", ptr);

  return ReturnStringCopy(cx, args, buffer);
#  endif
}

static bool SharedAddress(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }
  if (!args[0].isObject()) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Expected object");
    return false;
  }

#  ifdef JS_MORE_DETERMINISTIC
  args.rval().setString(cx->staticStrings().getUint(0));
#  else
  RootedObject obj(cx, CheckedUnwrapStatic(&args[0].toObject()));
  if (!obj) {
    ReportAccessDenied(cx);
    return false;
  }
  if (!obj->is<SharedArrayBufferObject>()) {
    JS_ReportErrorASCII(cx, "Argument must be a SharedArrayBuffer");
    return false;
  }
  char buffer[64];
  uint32_t nchar = SprintfLiteral(
      buffer, "%p",
      obj->as<SharedArrayBufferObject>().dataPointerShared().unwrap(
          /*safeish*/));

  JSString* str = JS_NewStringCopyN(cx, buffer, nchar);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
#  endif

  return true;
}
#endif

static bool DumpBacktrace(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  DumpBacktrace(cx);
  args.rval().setUndefined();
  return true;
}

static bool GetBacktrace(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool showArgs = false;
  bool showLocals = false;
  bool showThisProps = false;

  if (args.length() > 1) {
    RootedObject callee(cx, &args.callee());
    ReportUsageErrorASCII(cx, callee, "Too many arguments");
    return false;
  }

  if (args.length() == 1) {
    RootedObject cfg(cx, ToObject(cx, args[0]));
    if (!cfg) {
      return false;
    }
    RootedValue v(cx);

    if (!JS_GetProperty(cx, cfg, "args", &v)) {
      return false;
    }
    showArgs = ToBoolean(v);

    if (!JS_GetProperty(cx, cfg, "locals", &v)) {
      return false;
    }
    showLocals = ToBoolean(v);

    if (!JS_GetProperty(cx, cfg, "thisprops", &v)) {
      return false;
    }
    showThisProps = ToBoolean(v);
  }

  JS::UniqueChars buf =
      JS::FormatStackDump(cx, showArgs, showLocals, showThisProps);
  if (!buf) {
    return false;
  }

  JS::ConstUTF8CharsZ utf8chars(buf.get(), strlen(buf.get()));
  JSString* str = NewStringCopyUTF8Z<CanGC>(cx, utf8chars);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool ReportOutOfMemory(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  JS_ReportOutOfMemory(cx);
  cx->clearPendingException();
  args.rval().setUndefined();
  return true;
}

static bool ThrowOutOfMemory(JSContext* cx, unsigned argc, Value* vp) {
  JS_ReportOutOfMemory(cx);
  return false;
}

static bool ReportLargeAllocationFailure(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  void* buf = cx->runtime()->onOutOfMemoryCanGC(
      AllocFunction::Malloc, js::MallocArena, JSRuntime::LARGE_ALLOCATION);
  js_free(buf);
  args.rval().setUndefined();
  return true;
}

namespace heaptools {

typedef UniqueTwoByteChars EdgeName;

// An edge to a node from its predecessor in a path through the graph.
class BackEdge {
  // The node from which this edge starts.
  JS::ubi::Node predecessor_;

  // The name of this edge.
  EdgeName name_;

 public:
  BackEdge() : name_(nullptr) {}
  // Construct an initialized back edge, taking ownership of |name|.
  BackEdge(JS::ubi::Node predecessor, EdgeName name)
      : predecessor_(predecessor), name_(std::move(name)) {}
  BackEdge(BackEdge&& rhs)
      : predecessor_(rhs.predecessor_), name_(std::move(rhs.name_)) {}
  BackEdge& operator=(BackEdge&& rhs) {
    MOZ_ASSERT(&rhs != this);
    this->~BackEdge();
    new (this) BackEdge(std::move(rhs));
    return *this;
  }

  EdgeName forgetName() { return std::move(name_); }
  JS::ubi::Node predecessor() const { return predecessor_; }

 private:
  // No copy constructor or copying assignment.
  BackEdge(const BackEdge&) = delete;
  BackEdge& operator=(const BackEdge&) = delete;
};

// A path-finding handler class for use with JS::ubi::BreadthFirst.
struct FindPathHandler {
  typedef BackEdge NodeData;
  typedef JS::ubi::BreadthFirst<FindPathHandler> Traversal;

  FindPathHandler(JSContext* cx, JS::ubi::Node start, JS::ubi::Node target,
                  MutableHandle<GCVector<Value>> nodes, Vector<EdgeName>& edges)
      : cx(cx),
        start(start),
        target(target),
        foundPath(false),
        nodes(nodes),
        edges(edges) {}

  bool operator()(Traversal& traversal, JS::ubi::Node origin,
                  const JS::ubi::Edge& edge, BackEdge* backEdge, bool first) {
    // We take care of each node the first time we visit it, so there's
    // nothing to be done on subsequent visits.
    if (!first) {
      return true;
    }

    // Record how we reached this node. This is the last edge on a
    // shortest path to this node.
    EdgeName edgeName =
        DuplicateStringToArena(js::StringBufferArena, cx, edge.name.get());
    if (!edgeName) {
      return false;
    }
    *backEdge = BackEdge(origin, std::move(edgeName));

    // Have we reached our final target node?
    if (edge.referent == target) {
      // Record the path that got us here, which must be a shortest path.
      if (!recordPath(traversal, backEdge)) {
        return false;
      }
      foundPath = true;
      traversal.stop();
    }

    return true;
  }

  // We've found a path to our target. Walk the backlinks to produce the
  // (reversed) path, saving the path in |nodes| and |edges|. |nodes| is
  // rooted, so it can hold the path's nodes as we leave the scope of
  // the AutoCheckCannotGC. Note that nodes are added to |visited| after we
  // return from operator() so we have to pass the target BackEdge* to this
  // function.
  bool recordPath(Traversal& traversal, BackEdge* targetBackEdge) {
    JS::ubi::Node here = target;

    do {
      BackEdge* backEdge = targetBackEdge;
      if (here != target) {
        Traversal::NodeMap::Ptr p = traversal.visited.lookup(here);
        MOZ_ASSERT(p);
        backEdge = &p->value();
      }
      JS::ubi::Node predecessor = backEdge->predecessor();
      if (!nodes.append(predecessor.exposeToJS()) ||
          !edges.append(backEdge->forgetName())) {
        return false;
      }
      here = predecessor;
    } while (here != start);

    return true;
  }

  JSContext* cx;

  // The node we're starting from.
  JS::ubi::Node start;

  // The node we're looking for.
  JS::ubi::Node target;

  // True if we found a path to target, false if we didn't.
  bool foundPath;

  // The nodes and edges of the path --- should we find one. The path is
  // stored in reverse order, because that's how it's easiest for us to
  // construct it:
  // - edges[i] is the name of the edge from nodes[i] to nodes[i-1].
  // - edges[0] is the name of the edge from nodes[0] to the target.
  // - The last node, nodes[n-1], is the start node.
  MutableHandle<GCVector<Value>> nodes;
  Vector<EdgeName>& edges;
};

}  // namespace heaptools

static bool FindPath(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "findPath", 2)) {
    return false;
  }

  // We don't ToString non-objects given as 'start' or 'target', because this
  // test is all about object identity, and ToString doesn't preserve that.
  // Non-GCThing endpoints don't make much sense.
  if (!args[0].isObject() && !args[0].isString() && !args[0].isSymbol()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                     nullptr, "not an object, string, or symbol");
    return false;
  }

  if (!args[1].isObject() && !args[1].isString() && !args[1].isSymbol()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                     nullptr, "not an object, string, or symbol");
    return false;
  }

  Rooted<GCVector<Value>> nodes(cx, GCVector<Value>(cx));
  Vector<heaptools::EdgeName> edges(cx);

  {
    // We can't tolerate the GC moving things around while we're searching
    // the heap. Check that nothing we do causes a GC.
    JS::AutoCheckCannotGC autoCannotGC;

    JS::ubi::Node start(args[0]), target(args[1]);

    heaptools::FindPathHandler handler(cx, start, target, &nodes, edges);
    heaptools::FindPathHandler::Traversal traversal(cx, handler, autoCannotGC);
    if (!traversal.addStart(start)) {
      ReportOutOfMemory(cx);
      return false;
    }

    if (!traversal.traverse()) {
      if (!cx->isExceptionPending()) {
        ReportOutOfMemory(cx);
      }
      return false;
    }

    if (!handler.foundPath) {
      // We didn't find any paths from the start to the target.
      args.rval().setUndefined();
      return true;
    }
  }

  // |nodes| and |edges| contain the path from |start| to |target|, reversed.
  // Construct a JavaScript array describing the path from the start to the
  // target. Each element has the form:
  //
  //   {
  //     node: <object or string or symbol>,
  //     edge: <string describing outgoing edge from node>
  //   }
  //
  // or, if the node is some internal thing that isn't a proper JavaScript
  // value:
  //
  //   { node: undefined, edge: <string> }
  size_t length = nodes.length();
  RootedArrayObject result(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!result) {
    return false;
  }
  result->ensureDenseInitializedLength(cx, 0, length);

  // Walk |nodes| and |edges| in the stored order, and construct the result
  // array in start-to-target order.
  for (size_t i = 0; i < length; i++) {
    // Build an object describing the node and edge.
    RootedObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!obj) {
      return false;
    }

    RootedValue wrapped(cx, nodes[i]);
    if (!cx->compartment()->wrap(cx, &wrapped)) {
      return false;
    }

    if (!JS_DefineProperty(cx, obj, "node", wrapped, JSPROP_ENUMERATE)) {
      return false;
    }

    heaptools::EdgeName edgeName = std::move(edges[i]);

    size_t edgeNameLength = js_strlen(edgeName.get());
    RootedString edgeStr(
        cx, NewString<CanGC>(cx, std::move(edgeName), edgeNameLength));
    if (!edgeStr) {
      return false;
    }

    if (!JS_DefineProperty(cx, obj, "edge", edgeStr, JSPROP_ENUMERATE)) {
      return false;
    }

    result->setDenseElement(length - i - 1, ObjectValue(*obj));
  }

  args.rval().setObject(*result);
  return true;
}

static bool ShortestPaths(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "shortestPaths", 3)) {
    return false;
  }

  // We don't ToString non-objects given as 'start' or 'target', because this
  // test is all about object identity, and ToString doesn't preserve that.
  // Non-GCThing endpoints don't make much sense.
  if (!args[0].isObject() && !args[0].isString() && !args[0].isSymbol()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[0],
                     nullptr, "not an object, string, or symbol");
    return false;
  }

  if (!args[1].isObject() || !args[1].toObject().is<ArrayObject>()) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[1],
                     nullptr, "not an array object");
    return false;
  }

  RootedArrayObject objs(cx, &args[1].toObject().as<ArrayObject>());
  size_t length = objs->getDenseInitializedLength();
  if (length == 0) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[1],
                     nullptr,
                     "not a dense array object with one or more elements");
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    RootedValue el(cx, objs->getDenseElement(i));
    if (!el.isObject() && !el.isString() && !el.isSymbol()) {
      JS_ReportErrorASCII(cx,
                          "Each target must be an object, string, or symbol");
      return false;
    }
  }

  int32_t maxNumPaths;
  if (!JS::ToInt32(cx, args[2], &maxNumPaths)) {
    return false;
  }
  if (maxNumPaths <= 0) {
    ReportValueError(cx, JSMSG_UNEXPECTED_TYPE, JSDVG_SEARCH_STACK, args[2],
                     nullptr, "not greater than 0");
    return false;
  }

  // We accumulate the results into a GC-stable form, due to the fact that the
  // JS::ubi::ShortestPaths lifetime (when operating on the live heap graph)
  // is bounded within an AutoCheckCannotGC.
  Rooted<GCVector<GCVector<GCVector<Value>>>> values(
      cx, GCVector<GCVector<GCVector<Value>>>(cx));
  Vector<Vector<Vector<JS::ubi::EdgeName>>> names(cx);

  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;

    for (size_t i = 0; i < length; i++) {
      RootedValue val(cx, objs->getDenseElement(i));
      JS::ubi::Node node(val);
      if (!targets.put(node)) {
        ReportOutOfMemory(cx);
        return false;
      }
    }

    JS::ubi::Node root(args[0]);
    auto maybeShortestPaths = JS::ubi::ShortestPaths::Create(
        cx, noGC, maxNumPaths, root, std::move(targets));
    if (maybeShortestPaths.isNothing()) {
      ReportOutOfMemory(cx);
      return false;
    }
    auto& shortestPaths = *maybeShortestPaths;

    for (size_t i = 0; i < length; i++) {
      if (!values.append(GCVector<GCVector<Value>>(cx)) ||
          !names.append(Vector<Vector<JS::ubi::EdgeName>>(cx))) {
        return false;
      }

      RootedValue val(cx, objs->getDenseElement(i));
      JS::ubi::Node target(val);

      bool ok = shortestPaths.forEachPath(target, [&](JS::ubi::Path& path) {
        Rooted<GCVector<Value>> pathVals(cx, GCVector<Value>(cx));
        Vector<JS::ubi::EdgeName> pathNames(cx);

        for (auto& part : path) {
          if (!pathVals.append(part->predecessor().exposeToJS()) ||
              !pathNames.append(std::move(part->name()))) {
            return false;
          }
        }

        return values.back().append(std::move(pathVals.get())) &&
               names.back().append(std::move(pathNames));
      });

      if (!ok) {
        return false;
      }
    }
  }

  MOZ_ASSERT(values.length() == names.length());
  MOZ_ASSERT(values.length() == length);

  RootedArrayObject results(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!results) {
    return false;
  }
  results->ensureDenseInitializedLength(cx, 0, length);

  for (size_t i = 0; i < length; i++) {
    size_t numPaths = values[i].length();
    MOZ_ASSERT(names[i].length() == numPaths);

    RootedArrayObject pathsArray(cx, NewDenseFullyAllocatedArray(cx, numPaths));
    if (!pathsArray) {
      return false;
    }
    pathsArray->ensureDenseInitializedLength(cx, 0, numPaths);

    for (size_t j = 0; j < numPaths; j++) {
      size_t pathLength = values[i][j].length();
      MOZ_ASSERT(names[i][j].length() == pathLength);

      RootedArrayObject path(cx, NewDenseFullyAllocatedArray(cx, pathLength));
      if (!path) {
        return false;
      }
      path->ensureDenseInitializedLength(cx, 0, pathLength);

      for (size_t k = 0; k < pathLength; k++) {
        RootedPlainObject part(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!part) {
          return false;
        }

        RootedValue predecessor(cx, values[i][j][k]);
        if (!cx->compartment()->wrap(cx, &predecessor) ||
            !JS_DefineProperty(cx, part, "predecessor", predecessor,
                               JSPROP_ENUMERATE)) {
          return false;
        }

        if (names[i][j][k]) {
          RootedString edge(cx,
                            NewStringCopyZ<CanGC>(cx, names[i][j][k].get()));
          if (!edge ||
              !JS_DefineProperty(cx, part, "edge", edge, JSPROP_ENUMERATE)) {
            return false;
          }
        }

        path->setDenseElement(k, ObjectValue(*part));
      }

      pathsArray->setDenseElement(j, ObjectValue(*path));
    }

    results->setDenseElement(i, ObjectValue(*pathsArray));
  }

  args.rval().setObject(*results);
  return true;
}

static bool EvalReturningScope(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "evalReturningScope", 1)) {
    return false;
  }

  RootedString str(cx, ToString(cx, args[0]));
  if (!str) {
    return false;
  }

  RootedObject global(cx);
  if (args.hasDefined(1)) {
    global = ToObject(cx, args[1]);
    if (!global) {
      return false;
    }
  }

  AutoStableStringChars strChars(cx);
  if (!strChars.initTwoByte(cx, str)) {
    return false;
  }

  mozilla::Range<const char16_t> chars = strChars.twoByteRange();
  size_t srclen = chars.length();
  const char16_t* src = chars.begin().get();

  JS::AutoFilename filename;
  unsigned lineno;

  JS::DescribeScriptedCaller(cx, &filename, &lineno);

  JS::CompileOptions options(cx);
  options.setFileAndLine(filename.get(), lineno);
  options.setNoScriptRval(true);

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, src, srclen, SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx, JS::CompileForNonSyntacticScope(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  if (global) {
    global = CheckedUnwrapDynamic(global, cx, /* stopAtWindowProxy = */ false);
    if (!global) {
      JS_ReportErrorASCII(cx, "Permission denied to access global");
      return false;
    }
    if (!global->is<GlobalObject>()) {
      JS_ReportErrorASCII(cx, "Argument must be a global object");
      return false;
    }
  } else {
    global = JS::CurrentGlobalOrNull(cx);
  }

  RootedObject varObj(cx);
  RootedObject lexicalScope(cx);

  {
    // If we're switching globals here, ExecuteInFrameScriptEnvironment will
    // take care of cloning the script into that compartment before
    // executing it.
    AutoRealm ar(cx, global);
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    if (!obj) {
      return false;
    }

    if (!js::ExecuteInFrameScriptEnvironment(cx, obj, script, &lexicalScope)) {
      return false;
    }

    varObj = lexicalScope->enclosingEnvironment()->enclosingEnvironment();
  }

  RootedObject rv(cx, JS_NewPlainObject(cx));
  if (!rv) {
    return false;
  }

  RootedValue varObjVal(cx, ObjectValue(*varObj));
  if (!cx->compartment()->wrap(cx, &varObjVal)) {
    return false;
  }
  if (!JS_SetProperty(cx, rv, "vars", varObjVal)) {
    return false;
  }

  RootedValue lexicalScopeVal(cx, ObjectValue(*lexicalScope));
  if (!cx->compartment()->wrap(cx, &lexicalScopeVal)) {
    return false;
  }
  if (!JS_SetProperty(cx, rv, "lexicals", lexicalScopeVal)) {
    return false;
  }

  args.rval().setObject(*rv);
  return true;
}

static bool ShellCloneAndExecuteScript(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "cloneAndExecuteScript", 2)) {
    return false;
  }

  RootedString str(cx, ToString(cx, args[0]));
  if (!str) {
    return false;
  }

  RootedObject global(cx, ToObject(cx, args[1]));
  if (!global) {
    return false;
  }

  AutoStableStringChars strChars(cx);
  if (!strChars.initTwoByte(cx, str)) {
    return false;
  }

  mozilla::Range<const char16_t> chars = strChars.twoByteRange();
  size_t srclen = chars.length();
  const char16_t* src = chars.begin().get();

  JS::AutoFilename filename;
  unsigned lineno;

  JS::DescribeScriptedCaller(cx, &filename, &lineno);

  JS::CompileOptions options(cx);
  options.setFileAndLine(filename.get(), lineno);
  options.setNoScriptRval(true);

  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(cx, src, srclen, SourceOwnership::Borrowed)) {
    return false;
  }

  RootedScript script(cx, JS::Compile(cx, options, srcBuf));
  if (!script) {
    return false;
  }

  global = CheckedUnwrapDynamic(global, cx, /* stopAtWindowProxy = */ false);
  if (!global) {
    JS_ReportErrorASCII(cx, "Permission denied to access global");
    return false;
  }
  if (!global->is<GlobalObject>()) {
    JS_ReportErrorASCII(cx, "Argument must be a global object");
    return false;
  }

  AutoRealm ar(cx, global);

  JS::RootedValue rval(cx);
  if (!JS::CloneAndExecuteScript(cx, script, &rval)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ByteSize(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  mozilla::MallocSizeOf mallocSizeOf = cx->runtime()->debuggerMallocSizeOf;

  {
    // We can't tolerate the GC moving things around while we're using a
    // ubi::Node. Check that nothing we do causes a GC.
    JS::AutoCheckCannotGC autoCannotGC;

    JS::ubi::Node node = args.get(0);
    if (node) {
      args.rval().setNumber(uint32_t(node.size(mallocSizeOf)));
    } else {
      args.rval().setUndefined();
    }
  }
  return true;
}

static bool ByteSizeOfScript(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "byteSizeOfScript", 1)) {
    return false;
  }
  if (!args[0].isObject() || !args[0].toObject().is<JSFunction>()) {
    JS_ReportErrorASCII(cx, "Argument must be a Function object");
    return false;
  }

  RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
  if (fun->isNative()) {
    JS_ReportErrorASCII(cx, "Argument must be a scripted function");
    return false;
  }

  RootedScript script(cx, JSFunction::getOrCreateScript(cx, fun));
  if (!script) {
    return false;
  }

  mozilla::MallocSizeOf mallocSizeOf = cx->runtime()->debuggerMallocSizeOf;

  {
    // We can't tolerate the GC moving things around while we're using a
    // ubi::Node. Check that nothing we do causes a GC.
    JS::AutoCheckCannotGC autoCannotGC;

    JS::ubi::Node node = script;
    if (node) {
      args.rval().setNumber(uint32_t(node.size(mallocSizeOf)));
    } else {
      args.rval().setUndefined();
    }
  }
  return true;
}

static bool SetImmutablePrototype(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.get(0).isObject()) {
    JS_ReportErrorASCII(cx, "setImmutablePrototype: object expected");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());

  bool succeeded;
  if (!js::SetImmutablePrototype(cx, obj, &succeeded)) {
    return false;
  }

  args.rval().setBoolean(succeeded);
  return true;
}

#ifdef DEBUG
static bool DumpStringRepresentation(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedString str(cx, ToString(cx, args.get(0)));
  if (!str) {
    return false;
  }

  Fprinter out(stderr);
  str->dumpRepresentation(out, 0);

  args.rval().setUndefined();
  return true;
}
#endif

static bool SetLazyParsingDisabled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool disable = !args.hasDefined(0) || ToBoolean(args[0]);
  cx->realm()->behaviors().setDisableLazyParsing(disable);

  args.rval().setUndefined();
  return true;
}

static bool SetDiscardSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool discard = !args.hasDefined(0) || ToBoolean(args[0]);
  cx->realm()->behaviors().setDiscardSource(discard);

  args.rval().setUndefined();
  return true;
}

static bool GetConstructorName(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "getConstructorName", 1)) {
    return false;
  }

  if (!args[0].isObject()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE, "getConstructorName",
                              "Object", InformalValueTypeName(args[0]));
    return false;
  }

  RootedAtom name(cx);
  RootedObject obj(cx, &args[0].toObject());
  if (!JSObject::constructorDisplayAtom(cx, obj, &name)) {
    return false;
  }

  if (name) {
    args.rval().setString(name);
  } else {
    args.rval().setNull();
  }
  return true;
}

static bool AllocationMarker(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  bool allocateInsideNursery = true;
  if (args.length() > 0 && args[0].isObject()) {
    RootedObject options(cx, &args[0].toObject());

    RootedValue nurseryVal(cx);
    if (!JS_GetProperty(cx, options, "nursery", &nurseryVal)) {
      return false;
    }
    allocateInsideNursery = ToBoolean(nurseryVal);
  }

  static const JSClass cls = {"AllocationMarker"};

  auto newKind = allocateInsideNursery ? GenericObject : TenuredObject;
  RootedObject obj(cx, NewObjectWithGivenProto(cx, &cls, nullptr, newKind));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

namespace gcCallback {

struct MajorGC {
  int32_t depth;
  int32_t phases;
};

static void majorGC(JSContext* cx, JSGCStatus status, void* data) {
  auto info = static_cast<MajorGC*>(data);
  if (!(info->phases & (1 << status))) {
    return;
  }

  if (info->depth > 0) {
    info->depth--;
    JS::PrepareForFullGC(cx);
    JS::NonIncrementalGC(cx, GC_NORMAL, JS::GCReason::API);
    info->depth++;
  }
}

struct MinorGC {
  int32_t phases;
  bool active;
};

static void minorGC(JSContext* cx, JSGCStatus status, void* data) {
  auto info = static_cast<MinorGC*>(data);
  if (!(info->phases & (1 << status))) {
    return;
  }

  if (info->active) {
    info->active = false;
    if (cx->zone() && !cx->zone()->isAtomsZone()) {
      cx->runtime()->gc.evictNursery(JS::GCReason::DEBUG_GC);
    }
    info->active = true;
  }
}

// Process global, should really be runtime-local.
static MajorGC majorGCInfo;
static MinorGC minorGCInfo;

static void enterNullRealm(JSContext* cx, JSGCStatus status, void* data) {
  JSAutoNullableRealm enterRealm(cx, nullptr);
}

} /* namespace gcCallback */

static bool SetGCCallback(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  RootedObject opts(cx, ToObject(cx, args[0]));
  if (!opts) {
    return false;
  }

  RootedValue v(cx);
  if (!JS_GetProperty(cx, opts, "action", &v)) {
    return false;
  }

  JSString* str = JS::ToString(cx, v);
  if (!str) {
    return false;
  }
  RootedLinearString action(cx, str->ensureLinear(cx));
  if (!action) {
    return false;
  }

  int32_t phases = 0;
  if (StringEqualsAscii(action, "minorGC") ||
      StringEqualsAscii(action, "majorGC")) {
    if (!JS_GetProperty(cx, opts, "phases", &v)) {
      return false;
    }
    if (v.isUndefined()) {
      phases = (1 << JSGC_END);
    } else {
      JSString* str = JS::ToString(cx, v);
      if (!str) {
        return false;
      }
      JSLinearString* phasesStr = str->ensureLinear(cx);
      if (!phasesStr) {
        return false;
      }

      if (StringEqualsAscii(phasesStr, "begin")) {
        phases = (1 << JSGC_BEGIN);
      } else if (StringEqualsAscii(phasesStr, "end")) {
        phases = (1 << JSGC_END);
      } else if (StringEqualsAscii(phasesStr, "both")) {
        phases = (1 << JSGC_BEGIN) | (1 << JSGC_END);
      } else {
        JS_ReportErrorASCII(cx, "Invalid callback phase");
        return false;
      }
    }
  }

  if (StringEqualsAscii(action, "minorGC")) {
    gcCallback::minorGCInfo.phases = phases;
    gcCallback::minorGCInfo.active = true;
    JS_SetGCCallback(cx, gcCallback::minorGC, &gcCallback::minorGCInfo);
  } else if (StringEqualsAscii(action, "majorGC")) {
    if (!JS_GetProperty(cx, opts, "depth", &v)) {
      return false;
    }
    int32_t depth = 1;
    if (!v.isUndefined()) {
      if (!ToInt32(cx, v, &depth)) {
        return false;
      }
    }
    if (depth < 0) {
      JS_ReportErrorASCII(cx, "Nesting depth cannot be negative");
      return false;
    }
    if (depth + gcstats::MAX_PHASE_NESTING >
        gcstats::Statistics::MAX_SUSPENDED_PHASES) {
      JS_ReportErrorASCII(cx, "Nesting depth too large, would overflow");
      return false;
    }

    gcCallback::majorGCInfo.phases = phases;
    gcCallback::majorGCInfo.depth = depth;
    JS_SetGCCallback(cx, gcCallback::majorGC, &gcCallback::majorGCInfo);
  } else if (StringEqualsAscii(action, "enterNullRealm")) {
    JS_SetGCCallback(cx, gcCallback::enterNullRealm, nullptr);
  } else {
    JS_ReportErrorASCII(cx, "Unknown GC callback action");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

#ifdef DEBUG
static bool EnqueueMark(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto& queue = cx->runtime()->gc.marker.markQueue;

  if (args.get(0).isString()) {
    RootedString val(cx, args[0].toString());
    if (!val->ensureLinear(cx)) {
      return false;
    }
    if (!queue.append(StringValue(val))) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  } else if (args.get(0).isObject()) {
    if (!queue.append(args[0])) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  } else {
    JS_ReportErrorASCII(cx, "Argument must be a string or object");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool GetMarkQueue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  auto& queue = cx->runtime()->gc.marker.markQueue.get();

  RootedObject result(cx, JS_NewArrayObject(cx, queue.length()));
  if (!result) {
    return false;
  }
  for (size_t i = 0; i < queue.length(); i++) {
    RootedValue val(cx, queue[i]);
    if (!JS_WrapValue(cx, &val)) {
      return false;
    }
    if (!JS_SetElement(cx, result, i, val)) {
      return false;
    }
  }

  args.rval().setObject(*result);
  return true;
}

static bool ClearMarkQueue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  cx->runtime()->gc.marker.markQueue.clear();
  args.rval().setUndefined();
  return true;
}
#endif  // DEBUG

static bool GetLcovInfo(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() > 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  RootedObject global(cx);
  if (args.hasDefined(0)) {
    global = ToObject(cx, args[0]);
    if (!global) {
      JS_ReportErrorASCII(cx, "Permission denied to access global");
      return false;
    }
    global = CheckedUnwrapDynamic(global, cx, /* stopAtWindowProxy = */ false);
    if (!global) {
      ReportAccessDenied(cx);
      return false;
    }
    if (!global->is<GlobalObject>()) {
      JS_ReportErrorASCII(cx, "Argument must be a global object");
      return false;
    }
  } else {
    global = JS::CurrentGlobalOrNull(cx);
  }

  size_t length = 0;
  UniqueChars content;
  {
    AutoRealm ar(cx, global);
    content.reset(js::GetCodeCoverageSummary(cx, &length));
  }

  if (!content) {
    return false;
  }

  JSString* str = JS_NewStringCopyN(cx, content.get(), length);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

#ifdef DEBUG
static bool SetRNGState(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "SetRNGState", 2)) {
    return false;
  }

  double d0;
  if (!ToNumber(cx, args[0], &d0)) {
    return false;
  }

  double d1;
  if (!ToNumber(cx, args[1], &d1)) {
    return false;
  }

  uint64_t seed0 = static_cast<uint64_t>(d0);
  uint64_t seed1 = static_cast<uint64_t>(d1);

  if (seed0 == 0 && seed1 == 0) {
    JS_ReportErrorASCII(cx, "RNG requires non-zero seed");
    return false;
  }

  cx->realm()->getOrCreateRandomNumberGenerator().setState(seed0, seed1);

  args.rval().setUndefined();
  return true;
}
#endif

static ModuleEnvironmentObject* GetModuleEnvironment(
    JSContext* cx, HandleModuleObject module) {
  // Use the initial environment so that tests can check bindings exists
  // before they have been instantiated.
  RootedModuleEnvironmentObject env(cx, &module->initialEnvironment());
  MOZ_ASSERT(env);
  return env;
}

static bool GetModuleEnvironmentNames(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 1) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ModuleObject>()) {
    JS_ReportErrorASCII(cx, "First argument should be a ModuleObject");
    return false;
  }

  RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
  if (module->hadEvaluationError()) {
    JS_ReportErrorASCII(cx, "Module environment unavailable");
    return false;
  }

  RootedModuleEnvironmentObject env(cx, GetModuleEnvironment(cx, module));
  Rooted<IdVector> ids(cx, IdVector(cx));
  if (!JS_Enumerate(cx, env, &ids)) {
    return false;
  }

  uint32_t length = ids.length();
  RootedArrayObject array(cx, NewDenseFullyAllocatedArray(cx, length));
  if (!array) {
    return false;
  }

  array->setDenseInitializedLength(length);
  for (uint32_t i = 0; i < length; i++) {
    array->initDenseElement(i, StringValue(JSID_TO_STRING(ids[i])));
  }

  args.rval().setObject(*array);
  return true;
}

static bool GetModuleEnvironmentValue(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 2) {
    JS_ReportErrorASCII(cx, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ModuleObject>()) {
    JS_ReportErrorASCII(cx, "First argument should be a ModuleObject");
    return false;
  }

  if (!args[1].isString()) {
    JS_ReportErrorASCII(cx, "Second argument should be a string");
    return false;
  }

  RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
  if (module->hadEvaluationError()) {
    JS_ReportErrorASCII(cx, "Module environment unavailable");
    return false;
  }

  RootedModuleEnvironmentObject env(cx, GetModuleEnvironment(cx, module));
  RootedString name(cx, args[1].toString());
  RootedId id(cx);
  if (!JS_StringToId(cx, name, &id)) {
    return false;
  }

  if (!GetProperty(cx, env, env, id, args.rval())) {
    return false;
  }

  if (args.rval().isMagic(JS_UNINITIALIZED_LEXICAL)) {
    ReportRuntimeLexicalError(cx, JSMSG_UNINITIALIZED_LEXICAL, id);
    return false;
  }

  return true;
}

#ifdef DEBUG
static const char* AssertionTypeToString(
    irregexp::RegExpAssertion::AssertionType type) {
  switch (type) {
    case irregexp::RegExpAssertion::START_OF_LINE:
      return "START_OF_LINE";
    case irregexp::RegExpAssertion::START_OF_INPUT:
      return "START_OF_INPUT";
    case irregexp::RegExpAssertion::END_OF_LINE:
      return "END_OF_LINE";
    case irregexp::RegExpAssertion::END_OF_INPUT:
      return "END_OF_INPUT";
    case irregexp::RegExpAssertion::BOUNDARY:
      return "BOUNDARY";
    case irregexp::RegExpAssertion::NON_BOUNDARY:
      return "NON_BOUNDARY";
    case irregexp::RegExpAssertion::NOT_AFTER_LEAD_SURROGATE:
      return "NOT_AFTER_LEAD_SURROGATE";
    case irregexp::RegExpAssertion::NOT_IN_SURROGATE_PAIR:
      return "NOT_IN_SURROGATE_PAIR";
  }
  MOZ_CRASH("unexpected AssertionType");
}

static JSObject* ConvertRegExpTreeToObject(JSContext* cx, LifoAlloc& alloc,
                                           irregexp::RegExpTree* tree) {
  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return nullptr;
  }

  auto IntProp = [](JSContext* cx, HandleObject obj, const char* name,
                    int32_t value) {
    RootedValue val(cx, Int32Value(value));
    return JS_SetProperty(cx, obj, name, val);
  };

  auto BooleanProp = [](JSContext* cx, HandleObject obj, const char* name,
                        bool value) {
    RootedValue val(cx, BooleanValue(value));
    return JS_SetProperty(cx, obj, name, val);
  };

  auto StringProp = [](JSContext* cx, HandleObject obj, const char* name,
                       const char* value) {
    RootedString valueStr(cx, JS_NewStringCopyZ(cx, value));
    if (!valueStr) {
      return false;
    }

    RootedValue val(cx, StringValue(valueStr));
    return JS_SetProperty(cx, obj, name, val);
  };

  auto ObjectProp = [](JSContext* cx, HandleObject obj, const char* name,
                       HandleObject value) {
    RootedValue val(cx, ObjectValue(*value));
    return JS_SetProperty(cx, obj, name, val);
  };

  auto CharVectorProp = [](JSContext* cx, HandleObject obj, const char* name,
                           const irregexp::CharacterVector& data) {
    RootedString valueStr(cx,
                          JS_NewUCStringCopyN(cx, data.begin(), data.length()));
    if (!valueStr) {
      return false;
    }

    RootedValue val(cx, StringValue(valueStr));
    return JS_SetProperty(cx, obj, name, val);
  };

  auto TreeProp = [&ObjectProp, &alloc](JSContext* cx, HandleObject obj,
                                        const char* name,
                                        irregexp::RegExpTree* tree) {
    RootedObject treeObj(cx, ConvertRegExpTreeToObject(cx, alloc, tree));
    if (!treeObj) {
      return false;
    }
    return ObjectProp(cx, obj, name, treeObj);
  };

  auto TreeVectorProp = [&ObjectProp, &alloc](
                            JSContext* cx, HandleObject obj, const char* name,
                            const irregexp::RegExpTreeVector& nodes) {
    size_t len = nodes.length();
    RootedObject array(cx, JS_NewArrayObject(cx, len));
    if (!array) {
      return false;
    }

    for (size_t i = 0; i < len; i++) {
      RootedObject child(cx, ConvertRegExpTreeToObject(cx, alloc, nodes[i]));
      if (!child) {
        return false;
      }

      RootedValue childVal(cx, ObjectValue(*child));
      if (!JS_SetElement(cx, array, i, childVal)) {
        return false;
      }
    }
    return ObjectProp(cx, obj, name, array);
  };

  auto CharRangesProp = [&ObjectProp](
                            JSContext* cx, HandleObject obj, const char* name,
                            const irregexp::CharacterRangeVector& ranges) {
    size_t len = ranges.length();
    RootedObject array(cx, JS_NewArrayObject(cx, len));
    if (!array) {
      return false;
    }

    for (size_t i = 0; i < len; i++) {
      const irregexp::CharacterRange& range = ranges[i];
      RootedObject rangeObj(cx, JS_NewPlainObject(cx));
      if (!rangeObj) {
        return false;
      }

      auto CharProp = [](JSContext* cx, HandleObject obj, const char* name,
                         char16_t c) {
        RootedString valueStr(cx, JS_NewUCStringCopyN(cx, &c, 1));
        if (!valueStr) {
          return false;
        }
        RootedValue val(cx, StringValue(valueStr));
        return JS_SetProperty(cx, obj, name, val);
      };

      if (!CharProp(cx, rangeObj, "from", range.from())) {
        return false;
      }
      if (!CharProp(cx, rangeObj, "to", range.to())) {
        return false;
      }

      RootedValue rangeVal(cx, ObjectValue(*rangeObj));
      if (!JS_SetElement(cx, array, i, rangeVal)) {
        return false;
      }
    }
    return ObjectProp(cx, obj, name, array);
  };

  auto ElemProp = [&ObjectProp, &alloc](
                      JSContext* cx, HandleObject obj, const char* name,
                      const irregexp::TextElementVector& elements) {
    size_t len = elements.length();
    RootedObject array(cx, JS_NewArrayObject(cx, len));
    if (!array) {
      return false;
    }

    for (size_t i = 0; i < len; i++) {
      const irregexp::TextElement& element = elements[i];
      RootedObject elemTree(
          cx, ConvertRegExpTreeToObject(cx, alloc, element.tree()));
      if (!elemTree) {
        return false;
      }

      RootedValue elemTreeVal(cx, ObjectValue(*elemTree));
      if (!JS_SetElement(cx, array, i, elemTreeVal)) {
        return false;
      }
    }
    return ObjectProp(cx, obj, name, array);
  };

  if (tree->IsDisjunction()) {
    if (!StringProp(cx, obj, "type", "Disjunction")) {
      return nullptr;
    }
    irregexp::RegExpDisjunction* t = tree->AsDisjunction();
    if (!TreeVectorProp(cx, obj, "alternatives", t->alternatives())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsAlternative()) {
    if (!StringProp(cx, obj, "type", "Alternative")) {
      return nullptr;
    }
    irregexp::RegExpAlternative* t = tree->AsAlternative();
    if (!TreeVectorProp(cx, obj, "nodes", t->nodes())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsAssertion()) {
    if (!StringProp(cx, obj, "type", "Assertion")) {
      return nullptr;
    }
    irregexp::RegExpAssertion* t = tree->AsAssertion();
    if (!StringProp(cx, obj, "assertion_type",
                    AssertionTypeToString(t->assertion_type()))) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsCharacterClass()) {
    if (!StringProp(cx, obj, "type", "CharacterClass")) {
      return nullptr;
    }
    irregexp::RegExpCharacterClass* t = tree->AsCharacterClass();
    if (!BooleanProp(cx, obj, "is_negated", t->is_negated())) {
      return nullptr;
    }
    if (!CharRangesProp(cx, obj, "ranges", t->ranges(&alloc))) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsAtom()) {
    if (!StringProp(cx, obj, "type", "Atom")) {
      return nullptr;
    }
    irregexp::RegExpAtom* t = tree->AsAtom();
    if (!CharVectorProp(cx, obj, "data", t->data())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsText()) {
    if (!StringProp(cx, obj, "type", "Text")) {
      return nullptr;
    }
    irregexp::RegExpText* t = tree->AsText();
    if (!ElemProp(cx, obj, "elements", t->elements())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsQuantifier()) {
    if (!StringProp(cx, obj, "type", "Quantifier")) {
      return nullptr;
    }
    irregexp::RegExpQuantifier* t = tree->AsQuantifier();
    if (!IntProp(cx, obj, "min", t->min())) {
      return nullptr;
    }
    if (!IntProp(cx, obj, "max", t->max())) {
      return nullptr;
    }
    if (!StringProp(cx, obj, "quantifier_type",
                    t->is_possessive()
                        ? "POSSESSIVE"
                        : t->is_non_greedy() ? "NON_GREEDY" : "GREEDY"))
      return nullptr;
    if (!TreeProp(cx, obj, "body", t->body())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsCapture()) {
    if (!StringProp(cx, obj, "type", "Capture")) {
      return nullptr;
    }
    irregexp::RegExpCapture* t = tree->AsCapture();
    if (!IntProp(cx, obj, "index", t->index())) {
      return nullptr;
    }
    if (!TreeProp(cx, obj, "body", t->body())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsLookahead()) {
    if (!StringProp(cx, obj, "type", "Lookahead")) {
      return nullptr;
    }
    irregexp::RegExpLookahead* t = tree->AsLookahead();
    if (!BooleanProp(cx, obj, "is_positive", t->is_positive())) {
      return nullptr;
    }
    if (!TreeProp(cx, obj, "body", t->body())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsBackReference()) {
    if (!StringProp(cx, obj, "type", "BackReference")) {
      return nullptr;
    }
    irregexp::RegExpBackReference* t = tree->AsBackReference();
    if (!IntProp(cx, obj, "index", t->index())) {
      return nullptr;
    }
    return obj;
  }
  if (tree->IsEmpty()) {
    if (!StringProp(cx, obj, "type", "Empty")) {
      return nullptr;
    }
    return obj;
  }

  MOZ_CRASH("unexpected RegExpTree type");
}

static bool ParseRegExp(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() == 0) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isString()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a String");
    return false;
  }

  RegExpFlags flags = RegExpFlag::NoFlags;
  if (!args.get(1).isUndefined()) {
    if (!args.get(1).isString()) {
      ReportUsageErrorASCII(cx, callee,
                            "Second argument, if present, must be a String");
      return false;
    }
    RootedString flagStr(cx, args[1].toString());
    if (!ParseRegExpFlags(cx, flagStr, &flags)) {
      return false;
    }
  }

  bool match_only = false;
  if (!args.get(2).isUndefined()) {
    if (!args.get(2).isBoolean()) {
      ReportUsageErrorASCII(cx, callee,
                            "Third argument, if present, must be a Boolean");
      return false;
    }
    match_only = args[2].toBoolean();
  }

  RootedAtom pattern(cx, AtomizeString(cx, args[0].toString()));
  if (!pattern) {
    return false;
  }

  CompileOptions options(cx);
  frontend::TokenStream dummyTokenStream(cx, options, nullptr, 0, nullptr);

  // Data lifetime is controlled by LifoAllocScope.
  LifoAllocScope allocScope(&cx->tempLifoAlloc());
  irregexp::RegExpCompileData data;
  if (!irregexp::ParsePattern(dummyTokenStream, allocScope.alloc(), pattern,
                              match_only, flags, &data)) {
    return false;
  }

  RootedObject obj(
      cx, ConvertRegExpTreeToObject(cx, allocScope.alloc(), data.tree));
  if (!obj) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool DisRegExp(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() == 0) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<RegExpObject>()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a RegExp");
    return false;
  }

  Rooted<RegExpObject*> reobj(cx, &args[0].toObject().as<RegExpObject>());

  bool match_only = false;
  if (!args.get(1).isUndefined()) {
    if (!args.get(1).isBoolean()) {
      ReportUsageErrorASCII(cx, callee,
                            "Second argument, if present, must be a Boolean");
      return false;
    }
    match_only = args[1].toBoolean();
  }

  RootedLinearString input(cx, cx->runtime()->emptyString);
  if (!args.get(2).isUndefined()) {
    if (!args.get(2).isString()) {
      ReportUsageErrorASCII(cx, callee,
                            "Third argument, if present, must be a String");
      return false;
    }
    RootedString inputStr(cx, args[2].toString());
    input = inputStr->ensureLinear(cx);
    if (!input) {
      return false;
    }
  }

  if (!RegExpObject::dumpBytecode(cx, reobj, match_only, input)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}
#endif  // DEBUG

static bool GetTimeZone(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 0) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  auto getTimeZone = [](std::time_t* now) -> const char* {
    std::tm local{};
#if defined(_WIN32)
    _tzset();
    if (localtime_s(&local, now) == 0) {
      return _tzname[local.tm_isdst > 0];
    }
#else
    tzset();
#  if defined(HAVE_LOCALTIME_R)
    if (localtime_r(now, &local)) {
#  else
    std::tm* localtm = std::localtime(now);
    if (localtm) {
      *local = *localtm;
#  endif /* HAVE_LOCALTIME_R */

#  if defined(HAVE_TM_ZONE_TM_GMTOFF)
      return local.tm_zone;
#  else
      return tzname[local.tm_isdst > 0];
#  endif /* HAVE_TM_ZONE_TM_GMTOFF */
    }
#endif   /* _WIN32 */
    return nullptr;
  };

  std::time_t now = std::time(nullptr);
  if (now != static_cast<std::time_t>(-1)) {
    if (const char* tz = getTimeZone(&now)) {
      return ReturnStringCopy(cx, args, tz);
    }
  }

  args.rval().setUndefined();
  return true;
}

static bool SetTimeZone(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isString() && !args[0].isUndefined()) {
    ReportUsageErrorASCII(cx, callee,
                          "First argument should be a string or undefined");
    return false;
  }

  auto setTimeZone = [](const char* value) {
#if defined(_WIN32)
    return _putenv_s("TZ", value) == 0;
#else
    return setenv("TZ", value, true) == 0;
#endif /* _WIN32 */
  };

  auto unsetTimeZone = []() {
#if defined(_WIN32)
    return _putenv_s("TZ", "") == 0;
#else
    return unsetenv("TZ") == 0;
#endif /* _WIN32 */
  };

  if (args[0].isString() && !args[0].toString()->empty()) {
    RootedLinearString str(cx, args[0].toString()->ensureLinear(cx));
    if (!str) {
      return false;
    }

    if (!StringIsAscii(str)) {
      ReportUsageErrorASCII(cx, callee,
                            "First argument contains non-ASCII characters");
      return false;
    }

    UniqueChars timeZone = JS_EncodeStringToASCII(cx, str);
    if (!timeZone) {
      return false;
    }

    if (!setTimeZone(timeZone.get())) {
      JS_ReportErrorASCII(cx, "Failed to set 'TZ' environment variable");
      return false;
    }
  } else {
    if (!unsetTimeZone()) {
      JS_ReportErrorASCII(cx, "Failed to unset 'TZ' environment variable");
      return false;
    }
  }

#if defined(_WIN32)
  _tzset();
#else
  tzset();
#endif /* _WIN32 */

  JS::ResetTimeZone();

  args.rval().setUndefined();
  return true;
}

static bool GetCoreCount(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 0) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  args.rval().setInt32(GetCPUCount());
  return true;
}

static bool GetDefaultLocale(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 0) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  UniqueChars locale = JS_GetDefaultLocale(cx);
  if (!locale) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_DEFAULT_LOCALE_ERROR);
    return false;
  }

  return ReturnStringCopy(cx, args, locale.get());
}

static bool SetDefaultLocale(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (args.length() != 1) {
    ReportUsageErrorASCII(cx, callee, "Wrong number of arguments");
    return false;
  }

  if (!args[0].isString() && !args[0].isUndefined()) {
    ReportUsageErrorASCII(cx, callee,
                          "First argument should be a string or undefined");
    return false;
  }

  if (args[0].isString() && !args[0].toString()->empty()) {
    RootedLinearString str(cx, args[0].toString()->ensureLinear(cx));
    if (!str) {
      return false;
    }

    if (!StringIsAscii(str)) {
      ReportUsageErrorASCII(cx, callee,
                            "First argument contains non-ASCII characters");
      return false;
    }

    UniqueChars locale = JS_EncodeStringToASCII(cx, str);
    if (!locale) {
      return false;
    }

    bool containsOnlyValidBCP47Characters =
        mozilla::IsAsciiAlpha(locale[0]) &&
        std::all_of(locale.get(), locale.get() + str->length(), [](auto c) {
          return mozilla::IsAsciiAlphanumeric(c) || c == '-';
        });

    if (!containsOnlyValidBCP47Characters) {
      ReportUsageErrorASCII(cx, callee,
                            "First argument should be a BCP47 language tag");
      return false;
    }

    if (!JS_SetDefaultLocale(cx->runtime(), locale.get())) {
      ReportOutOfMemory(cx);
      return false;
    }
  } else {
    JS_ResetDefaultLocale(cx->runtime());
  }

  args.rval().setUndefined();
  return true;
}

#if defined(FUZZING) && defined(__AFL_COMPILER)
static bool AflLoop(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  uint32_t max_cnt;
  if (!ToUint32(cx, args.get(0), &max_cnt)) {
    return false;
  }

  args.rval().setBoolean(!!__AFL_LOOP(max_cnt));
  return true;
}
#endif

static bool MonotonicNow(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  double now;

// The std::chrono symbols are too new to be present in STL on all platforms we
// care about, so use raw POSIX clock APIs when it might be necessary.
#if defined(XP_UNIX) && !defined(XP_DARWIN)
  auto ComputeNow = [](const timespec& ts) {
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  };

  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    // Use a monotonic clock if available.
    now = ComputeNow(ts);
  } else {
    // Use a realtime clock as fallback.
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
      // Fail if no clock is available.
      JS_ReportErrorASCII(cx, "can't retrieve system clock");
      return false;
    }

    now = ComputeNow(ts);

    // Manually enforce atomicity on a non-monotonic clock.
    {
      static mozilla::Atomic<bool, mozilla::ReleaseAcquire> spinLock;
      while (!spinLock.compareExchange(false, true)) {
        continue;
      }

      static double lastNow = -FLT_MAX;
      now = lastNow = std::max(now, lastNow);

      spinLock = false;
    }
  }
#else
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::steady_clock;
  now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count();
#endif  // XP_UNIX && !XP_DARWIN

  args.rval().setNumber(now);
  return true;
}

static bool TimeSinceCreation(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  double when =
      (mozilla::TimeStamp::Now() - mozilla::TimeStamp::ProcessCreation())
          .ToMilliseconds();
  args.rval().setNumber(when);
  return true;
}

static bool GetErrorNotes(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "getErrorNotes", 1)) {
    return false;
  }

  if (!args[0].isObject() || !args[0].toObject().is<ErrorObject>()) {
    args.rval().setNull();
    return true;
  }

  JSErrorReport* report = args[0].toObject().as<ErrorObject>().getErrorReport();
  if (!report) {
    args.rval().setNull();
    return true;
  }

  RootedObject notesArray(cx, CreateErrorNotesArray(cx, report));
  if (!notesArray) {
    return false;
  }

  args.rval().setObject(*notesArray);
  return true;
}

static bool IsConstructor(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() < 1) {
    args.rval().setBoolean(false);
  } else {
    args.rval().setBoolean(IsConstructor(args[0]));
  }
  return true;
}

static bool SetTimeResolution(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.requireAtLeast(cx, "setTimeResolution", 2)) {
    return false;
  }

  if (!args[0].isInt32()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be an Int32.");
    return false;
  }
  int32_t resolution = args[0].toInt32();

  if (!args[1].isBoolean()) {
    ReportUsageErrorASCII(cx, callee, "Second argument must be a Boolean");
    return false;
  }
  bool jitter = args[1].toBoolean();

  JS::SetTimeResolutionUsec(resolution, jitter);

  args.rval().setUndefined();
  return true;
}

static bool ScriptedCallerGlobal(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  RootedObject obj(cx, JS::GetScriptedCallerGlobal(cx));
  if (!obj) {
    args.rval().setNull();
    return true;
  }

  obj = ToWindowProxyIfWindow(obj);

  if (!cx->compartment()->wrap(cx, &obj)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool ObjectGlobal(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.get(0).isObject()) {
    ReportUsageErrorASCII(cx, callee, "Argument must be an object");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  if (IsCrossCompartmentWrapper(obj)) {
    args.rval().setNull();
    return true;
  }

  obj = ToWindowProxyIfWindow(&obj->nonCCWGlobal());

  args.rval().setObject(*obj);
  return true;
}

static bool IsSameCompartment(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.get(0).isObject() || !args.get(1).isObject()) {
    ReportUsageErrorASCII(cx, callee, "Both arguments must be objects");
    return false;
  }

  RootedObject obj1(cx, UncheckedUnwrap(&args[0].toObject()));
  RootedObject obj2(cx, UncheckedUnwrap(&args[1].toObject()));

  args.rval().setBoolean(obj1->compartment() == obj2->compartment());
  return true;
}

static bool FirstGlobalInCompartment(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.get(0).isObject()) {
    ReportUsageErrorASCII(cx, callee, "Argument must be an object");
    return false;
  }

  RootedObject obj(cx, UncheckedUnwrap(&args[0].toObject()));
  obj = ToWindowProxyIfWindow(GetFirstGlobalInCompartment(obj->compartment()));

  if (!cx->compartment()->wrap(cx, &obj)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool AssertCorrectRealm(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_RELEASE_ASSERT(cx->realm() == args.callee().as<JSFunction>().realm());
  args.rval().setUndefined();
  return true;
}

static bool GlobalLexicals(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<LexicalEnvironmentObject*> globalLexical(
      cx, &cx->global()->lexicalEnvironment());

  RootedIdVector props(cx);
  if (!GetPropertyKeys(cx, globalLexical, JSITER_HIDDEN, &props)) {
    return false;
  }

  RootedObject res(cx, JS_NewPlainObject(cx));
  if (!res) {
    return false;
  }

  RootedValue val(cx);
  for (size_t i = 0; i < props.length(); i++) {
    HandleId id = props[i];
    if (!JS_GetPropertyById(cx, globalLexical, id, &val)) {
      return false;
    }
    if (val.isMagic(JS_UNINITIALIZED_LEXICAL)) {
      continue;
    }
    if (!JS_DefinePropertyById(cx, res, id, val, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  args.rval().setObject(*res);
  return true;
}

static bool MonitorType(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  // First argument must be either a scripted function or null/undefined (in
  // this case we use the caller's script).
  RootedFunction fun(cx);
  RootedScript script(cx);
  if (args.get(0).isNullOrUndefined()) {
    script = cx->currentScript();
    if (!script) {
      ReportUsageErrorASCII(cx, callee, "No scripted caller");
      return false;
    }
  } else {
    if (!IsFunctionObject(args.get(0), fun.address()) ||
        !fun->isInterpreted()) {
      ReportUsageErrorASCII(
          cx, callee,
          "First argument must be a scripted function or null/undefined");
      return false;
    }

    script = JSFunction::getOrCreateScript(cx, fun);
    if (!script) {
      return false;
    }
  }

  int32_t index;
  if (!ToInt32(cx, args.get(1), &index)) {
    return false;
  }

  if (index < 0 || uint32_t(index) >= jit::JitScript::NumTypeSets(script)) {
    ReportUsageErrorASCII(cx, callee, "Index out of range");
    return false;
  }

  RootedValue val(cx, args.get(2));

  // If val is "unknown" or "unknownObject" we mark the TypeSet as unknown or
  // unknownObject, respectively.
  bool unknown = false;
  bool unknownObject = false;
  if (val.isString()) {
    if (!JS_StringEqualsAscii(cx, val.toString(), "unknown", &unknown)) {
      return false;
    }
    if (!JS_StringEqualsAscii(cx, val.toString(), "unknownObject",
                              &unknownObject)) {
      return false;
    }
  }

  // Avoid assertion failures if Baseline is disabled or we can't Baseline
  // Interpret this script.
  if (!jit::IsBaselineInterpreterEnabled() ||
      !jit::CanBaselineInterpretScript(script)) {
    args.rval().setUndefined();
    return true;
  }

  AutoRealm ar(cx, script);

  if (!cx->realm()->ensureJitRealmExists(cx)) {
    return false;
  }

  jit::AutoKeepJitScripts keepJitScript(cx);
  if (!script->ensureHasJitScript(cx, keepJitScript)) {
    return false;
  }

  AutoEnterAnalysis enter(cx);
  AutoSweepJitScript sweep(script);
  StackTypeSet* typeSet = script->jitScript()->typeArray(sweep) + index;
  if (unknown) {
    typeSet->addType(sweep, cx, TypeSet::UnknownType());
  } else if (unknownObject) {
    typeSet->addType(sweep, cx, TypeSet::AnyObjectType());
  } else {
    typeSet->addType(sweep, cx, TypeSet::GetValueType(val));
  }

  args.rval().setUndefined();
  return true;
}

static bool MarkObjectPropertiesUnknown(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  if (!args.get(0).isObject()) {
    ReportUsageErrorASCII(cx, callee, "Argument must be an object");
    return false;
  }

  RootedObject obj(cx, &args[0].toObject());
  RootedObjectGroup group(cx, JSObject::getGroup(cx, obj));
  if (!group) {
    return false;
  }

  MarkObjectGroupUnknownProperties(cx, group);

  args.rval().setUndefined();
  return true;
}

static bool EncodeAsUtf8InBuffer(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "encodeAsUtf8InBuffer", 2)) {
    return false;
  }

  RootedObject callee(cx, &args.callee());

  if (!args[0].isString()) {
    ReportUsageErrorASCII(cx, callee, "First argument must be a String");
    return false;
  }

  // Create the amounts array early so that the raw pointer into Uint8Array
  // data has as short a lifetime as possible
  RootedArrayObject array(cx, NewDenseFullyAllocatedArray(cx, 2));
  if (!array) {
    return false;
  }
  array->ensureDenseInitializedLength(cx, 0, 2);

  uint32_t length;
  bool isSharedMemory;
  uint8_t* data;
  if (!args[1].isObject() ||
      !JS_GetObjectAsUint8Array(&args[1].toObject(), &length, &isSharedMemory,
                                &data) ||
      isSharedMemory ||  // excluded views of SharedArrayBuffers
      !data) {           // exclude views of detached ArrayBuffers
    ReportUsageErrorASCII(cx, callee, "Second argument must be a Uint8Array");
    return false;
  }

  Maybe<Tuple<size_t, size_t>> amounts = JS_EncodeStringToUTF8BufferPartial(
      cx, args[0].toString(), AsWritableChars(MakeSpan(data, length)));
  if (!amounts) {
    ReportOutOfMemory(cx);
    return false;
  }

  size_t unitsRead, bytesWritten;
  Tie(unitsRead, bytesWritten) = *amounts;

  array->initDenseElement(0, Int32Value(AssertedCast<int32_t>(unitsRead)));
  array->initDenseElement(1, Int32Value(AssertedCast<int32_t>(bytesWritten)));

  args.rval().setObject(*array);
  return true;
}

JSScript* js::TestingFunctionArgumentToScript(
    JSContext* cx, HandleValue v, JSFunction** funp /* = nullptr */) {
  if (v.isString()) {
    // To convert a string to a script, compile it. Parse it as an ES6 Program.
    RootedLinearString linearStr(cx, StringToLinearString(cx, v.toString()));
    if (!linearStr) {
      return nullptr;
    }
    size_t len = GetLinearStringLength(linearStr);
    AutoStableStringChars linearChars(cx);
    if (!linearChars.initTwoByte(cx, linearStr)) {
      return nullptr;
    }
    const char16_t* chars = linearChars.twoByteRange().begin().get();

    SourceText<char16_t> source;
    if (!source.init(cx, chars, len, SourceOwnership::Borrowed)) {
      return nullptr;
    }

    CompileOptions options(cx);
    return JS::Compile(cx, options, source);
  }

  RootedFunction fun(cx, JS_ValueToFunction(cx, v));
  if (!fun) {
    return nullptr;
  }

  // Unwrap bound functions.
  while (fun->isBoundFunction()) {
    JSObject* target = fun->getBoundFunctionTarget();
    if (target && target->is<JSFunction>()) {
      fun = &target->as<JSFunction>();
    } else {
      break;
    }
  }

  if (!fun->isInterpreted()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_TESTING_SCRIPTS_ONLY);
    return nullptr;
  }

  JSScript* script = JSFunction::getOrCreateScript(cx, fun);
  if (!script) {
    return nullptr;
  }

  if (funp) {
    *funp = fun;
  }

  return script;
}

static bool BaselineCompile(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  RootedScript script(cx);
  if (args.length() == 0) {
    NonBuiltinScriptFrameIter iter(cx);
    if (iter.done()) {
      ReportUsageErrorASCII(cx, callee,
                            "no script argument and no script caller");
      return false;
    }
    script = iter.script();
  } else {
    script = TestingFunctionArgumentToScript(cx, args[0]);
    if (!script) {
      return false;
    }
  }

  bool forceDebug = false;
  if (args.length() > 1) {
    if (args.length() > 2) {
      ReportUsageErrorASCII(cx, callee, "too many arguments");
      return false;
    }
    if (!args[1].isBoolean() && !args[1].isUndefined()) {
      ReportUsageErrorASCII(
          cx, callee, "forceDebugInstrumentation argument should be boolean");
      return false;
    }
    forceDebug = ToBoolean(args[1]);
  }

  const char* returnedStr = nullptr;
  do {
#ifdef JS_MORE_DETERMINISTIC
    // In order to check for differential behaviour, baselineCompile should have
    // the same output whether --no-baseline is used or not.
    if (fuzzingSafe) {
      returnedStr = "skipped (fuzzing-safe)";
      break;
    }
#endif

    AutoRealm ar(cx, script);
    if (script->hasBaselineScript()) {
      if (forceDebug && !script->baselineScript()->hasDebugInstrumentation()) {
        // There isn't an easy way to do this for a script that might be on
        // stack right now. See
        // js::jit::RecompileOnStackBaselineScriptsForDebugMode.
        ReportUsageErrorASCII(
            cx, callee, "unsupported case: recompiling script for debug mode");
        return false;
      }

      args.rval().setUndefined();
      return true;
    }

    if (!jit::IsBaselineJitEnabled()) {
      returnedStr = "baseline disabled";
      break;
    }
    if (!script->canBaselineCompile()) {
      returnedStr = "can't compile";
      break;
    }
    if (!cx->realm()->ensureJitRealmExists(cx)) {
      return false;
    }

    jit::MethodStatus status = jit::BaselineCompile(cx, script, forceDebug);
    switch (status) {
      case jit::Method_Error:
        return false;
      case jit::Method_CantCompile:
        returnedStr = "can't compile";
        break;
      case jit::Method_Skipped:
        returnedStr = "skipped";
        break;
      case jit::Method_Compiled:
        args.rval().setUndefined();
    }
  } while (false);

  if (returnedStr) {
    return ReturnStringCopy(cx, args, returnedStr);
  }

  return true;
}

static bool PCCountProfiling_Start(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::StartPCCountProfiling(cx);

  args.rval().setUndefined();
  return true;
}

static bool PCCountProfiling_Stop(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::StopPCCountProfiling(cx);

  args.rval().setUndefined();
  return true;
}

static bool PCCountProfiling_Purge(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  js::PurgePCCounts(cx);

  args.rval().setUndefined();
  return true;
}

static bool PCCountProfiling_ScriptCount(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  size_t length = js::GetPCCountScriptCount(cx);

  args.rval().setNumber(double(length));
  return true;
}

static bool PCCountProfiling_ScriptSummary(JSContext* cx, unsigned argc,
                                           Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "summary", 1)) {
    return false;
  }

  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index)) {
    return false;
  }

  JSString* str = js::GetPCCountScriptSummary(cx, index);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool PCCountProfiling_ScriptContents(JSContext* cx, unsigned argc,
                                            Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.requireAtLeast(cx, "contents", 1)) {
    return false;
  }

  uint32_t index;
  if (!JS::ToUint32(cx, args[0], &index)) {
    return false;
  }

  JSString* str = js::GetPCCountScriptContents(cx, index);
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

// clang-format off
static const JSFunctionSpecWithHelp TestingFunctions[] = {
    JS_FN_HELP("gc", ::GC, 0, 0,
"gc([obj] | 'zone' [, 'shrinking'])",
"  Run the garbage collector. When obj is given, GC only its zone.\n"
"  If 'zone' is given, GC any zones that were scheduled for\n"
"  GC via schedulegc.\n"
"  If 'shrinking' is passed as the optional second argument, perform a\n"
"  shrinking GC rather than a normal GC."),

    JS_FN_HELP("minorgc", ::MinorGC, 0, 0,
"minorgc([aboutToOverflow])",
"  Run a minor collector on the Nursery. When aboutToOverflow is true, marks\n"
"  the store buffer as about-to-overflow before collecting."),

    JS_FN_HELP("gcparam", GCParameter, 2, 0,
"gcparam(name [, value])",
"  Wrapper for JS_[GS]etGCParameter. The name is one of:" GC_PARAMETER_ARGS_LIST),

    JS_FN_HELP("relazifyFunctions", RelazifyFunctions, 0, 0,
"relazifyFunctions(...)",
"  Perform a GC and allow relazification of functions. Accepts the same\n"
"  arguments as gc()."),

    JS_FN_HELP("getBuildConfiguration", GetBuildConfiguration, 0, 0,
"getBuildConfiguration()",
"  Return an object describing some of the configuration options SpiderMonkey\n"
"  was built with."),

    JS_FN_HELP("hasChild", HasChild, 0, 0,
"hasChild(parent, child)",
"  Return true if |child| is a child of |parent|, as determined by a call to\n"
"  TraceChildren"),

    JS_FN_HELP("setSavedStacksRNGState", SetSavedStacksRNGState, 1, 0,
"setSavedStacksRNGState(seed)",
"  Set this compartment's SavedStacks' RNG state.\n"),

    JS_FN_HELP("getSavedFrameCount", GetSavedFrameCount, 0, 0,
"getSavedFrameCount()",
"  Return the number of SavedFrame instances stored in this compartment's\n"
"  SavedStacks cache."),

    JS_FN_HELP("clearSavedFrames", ClearSavedFrames, 0, 0,
"clearSavedFrames()",
"  Empty the current compartment's cache of SavedFrame objects, so that\n"
"  subsequent stack captures allocate fresh objects to represent frames.\n"
"  Clear the current stack's LiveSavedFrameCaches."),

    JS_FN_HELP("saveStack", SaveStack, 0, 0,
"saveStack([maxDepth [, compartment]])",
"  Capture a stack. If 'maxDepth' is given, capture at most 'maxDepth' number\n"
"  of frames. If 'compartment' is given, allocate the js::SavedFrame instances\n"
"  with the given object's compartment."),

    JS_FN_HELP("captureFirstSubsumedFrame", CaptureFirstSubsumedFrame, 1, 0,
"saveStack(object [, shouldIgnoreSelfHosted = true]])",
"  Capture a stack back to the first frame whose principals are subsumed by the\n"
"  object's compartment's principals. If 'shouldIgnoreSelfHosted' is given,\n"
"  control whether self-hosted frames are considered when checking principals."),

    JS_FN_HELP("callFunctionFromNativeFrame", CallFunctionFromNativeFrame, 1, 0,
"callFunctionFromNativeFrame(function)",
"  Call 'function' with a (C++-)native frame on stack.\n"
"  Required for testing that SaveStack properly handles native frames."),

    JS_FN_HELP("callFunctionWithAsyncStack", CallFunctionWithAsyncStack, 0, 0,
"callFunctionWithAsyncStack(function, stack, asyncCause)",
"  Call 'function', using the provided stack as the async stack responsible\n"
"  for the call, and propagate its return value or the exception it throws.\n"
"  The function is called with no arguments, and 'this' is 'undefined'. The\n"
"  specified |asyncCause| is attached to the provided stack frame."),

    JS_FN_HELP("enableTrackAllocations", EnableTrackAllocations, 0, 0,
"enableTrackAllocations()",
"  Start capturing the JS stack at every allocation. Note that this sets an\n"
"  object metadata callback that will override any other object metadata\n"
"  callback that may be set."),

    JS_FN_HELP("disableTrackAllocations", DisableTrackAllocations, 0, 0,
"disableTrackAllocations()",
"  Stop capturing the JS stack at every allocation."),

    JS_FN_HELP("newExternalString", NewExternalString, 1, 0,
"newExternalString(str)",
"  Copies str's chars and returns a new external string."),

    JS_FN_HELP("newMaybeExternalString", NewMaybeExternalString, 1, 0,
"newMaybeExternalString(str)",
"  Like newExternalString but uses the JS_NewMaybeExternalString API."),

    JS_FN_HELP("ensureFlatString", EnsureFlatString, 1, 0,
"ensureFlatString(str)",
"  Ensures str is a flat (null-terminated) string and returns it."),

    JS_FN_HELP("representativeStringArray", RepresentativeStringArray, 0, 0,
"representativeStringArray()",
"  Returns an array of strings that represent the various internal string\n"
"  types and character encodings."),

#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

    JS_FN_HELP("oomThreadTypes", OOMThreadTypes, 0, 0,
"oomThreadTypes()",
"  Get the number of thread types that can be used as an argument for\n"
"  oomAfterAllocations() and oomAtAllocation()."),

    JS_FN_HELP("oomAfterAllocations", OOMAfterAllocations, 2, 0,
"oomAfterAllocations(count [,threadType])",
"  After 'count' js_malloc memory allocations, fail every following allocation\n"
"  (return nullptr). The optional thread type limits the effect to the\n"
"  specified type of helper thread."),

    JS_FN_HELP("oomAtAllocation", OOMAtAllocation, 2, 0,
"oomAtAllocation(count [,threadType])",
"  After 'count' js_malloc memory allocations, fail the next allocation\n"
"  (return nullptr). The optional thread type limits the effect to the\n"
"  specified type of helper thread."),

    JS_FN_HELP("resetOOMFailure", ResetOOMFailure, 0, 0,
"resetOOMFailure()",
"  Remove the allocation failure scheduled by either oomAfterAllocations() or\n"
"  oomAtAllocation() and return whether any allocation had been caused to fail."),

    JS_FN_HELP("oomTest", OOMTest, 0, 0,
"oomTest(function, [expectExceptionOnFailure = true | options])",
"  Test that the passed function behaves correctly under OOM conditions by\n"
"  repeatedly executing it and simulating allocation failure at successive\n"
"  allocations until the function completes without seeing a failure.\n"
"  By default this tests that an exception is raised if execution fails, but\n"
"  this can be disabled by passing false as the optional second parameter.\n"
"  This is also disabled when --fuzzing-safe is specified.\n"
"  Alternatively an object can be passed to set the following options:\n"
"    expectExceptionOnFailure: bool - as described above.\n"
"    keepFailing: bool - continue to fail after first simulated failure.\n"
"\n"
"  WARNING: By design, oomTest assumes the test-function follows the same\n"
"  code path each time it is called, right up to the point where OOM occurs.\n"
"  If on iteration 70 it finishes and caches a unit of work that saves 65\n"
"  allocations the next time we run, then the subsequent 65 allocation\n"
"  points will go untested.\n"
"\n"
"  Things in this category include lazy parsing and baseline compilation,\n"
"  so it is very easy to accidentally write an oomTest that only tests one\n"
"  or the other of those, and not the functionality you meant to test!\n"
"  To avoid lazy parsing, call the test function once first before passing\n"
"  it to oomTest. The jits can be disabled via the test harness.\n"),

    JS_FN_HELP("stackTest", StackTest, 0, 0,
"stackTest(function, [expectExceptionOnFailure = true])",
"  This function behaves exactly like oomTest with the difference that\n"
"  instead of simulating regular OOM conditions, it simulates the engine\n"
"  running out of stack space (failing recursion check).\n"
"\n"
"  See the WARNING in help('oomTest').\n"),

    JS_FN_HELP("interruptTest", InterruptTest, 0, 0,
"interruptTest(function)",
"  This function simulates interrupts similar to how oomTest simulates OOM conditions."
"\n"
"  See the WARNING in help('oomTest').\n"),

#endif // defined(DEBUG) || defined(JS_OOM_BREAKPOINT)

    JS_FN_HELP("newRope", NewRope, 3, 0,
"newRope(left, right[, options])",
"  Creates a rope with the given left/right strings.\n"
"  Available options:\n"
"    nursery: bool - force the string to be created in/out of the nursery, if possible.\n"),

    JS_FN_HELP("isRope", IsRope, 1, 0,
"isRope(str)",
"  Returns true if the parameter is a rope"),

    JS_FN_HELP("settlePromiseNow", SettlePromiseNow, 1, 0,
"settlePromiseNow(promise)",
"  'Settle' a 'promise' immediately. This just marks the promise as resolved\n"
"  with a value of `undefined` and causes the firing of any onPromiseSettled\n"
"  hooks set on Debugger instances that are observing the given promise's\n"
"  global as a debuggee."),
    JS_FN_HELP("getWaitForAllPromise", GetWaitForAllPromise, 1, 0,
"getWaitForAllPromise(densePromisesArray)",
"  Calls the 'GetWaitForAllPromise' JSAPI function and returns the result\n"
"  Promise."),
JS_FN_HELP("resolvePromise", ResolvePromise, 2, 0,
"resolvePromise(promise, resolution)",
"  Resolve a Promise by calling the JSAPI function JS::ResolvePromise."),
JS_FN_HELP("rejectPromise", RejectPromise, 2, 0,
"rejectPromise(promise, reason)",
"  Reject a Promise by calling the JSAPI function JS::RejectPromise."),

JS_FN_HELP("streamsAreEnabled", StreamsAreEnabled, 0, 0,
"streamsAreEnabled()",
"  Returns a boolean indicating whether WHATWG Streams are enabled for the current realm."),

    JS_FN_HELP("makeFinalizeObserver", MakeFinalizeObserver, 0, 0,
"makeFinalizeObserver()",
"  Get a special object whose finalization increases the counter returned\n"
"  by the finalizeCount function."),

    JS_FN_HELP("finalizeCount", FinalizeCount, 0, 0,
"finalizeCount()",
"  Return the current value of the finalization counter that is incremented\n"
"  each time an object returned by the makeFinalizeObserver is finalized."),

    JS_FN_HELP("resetFinalizeCount", ResetFinalizeCount, 0, 0,
"resetFinalizeCount()",
"  Reset the value returned by finalizeCount()."),

    JS_FN_HELP("gcPreserveCode", GCPreserveCode, 0, 0,
"gcPreserveCode()",
"  Preserve JIT code during garbage collections."),

#ifdef JS_GC_ZEAL
    JS_FN_HELP("gczeal", GCZeal, 2, 0,
"gczeal(mode, [frequency])",
gc::ZealModeHelpText),

    JS_FN_HELP("unsetgczeal", UnsetGCZeal, 2, 0,
"unsetgczeal(mode)",
"  Turn off a single zeal mode set with gczeal() and don't finish any ongoing\n"
"  collection that may be happening."),

    JS_FN_HELP("schedulegc", ScheduleGC, 1, 0,
"schedulegc([num])",
"  If num is given, schedule a GC after num allocations.\n"
"  Returns the number of allocations before the next trigger."),

    JS_FN_HELP("selectforgc", SelectForGC, 0, 0,
"selectforgc(obj1, obj2, ...)",
"  Schedule the given objects to be marked in the next GC slice."),

    JS_FN_HELP("verifyprebarriers", VerifyPreBarriers, 0, 0,
"verifyprebarriers()",
"  Start or end a run of the pre-write barrier verifier."),

    JS_FN_HELP("verifypostbarriers", VerifyPostBarriers, 0, 0,
"verifypostbarriers()",
"  Does nothing (the post-write barrier verifier has been remove)."),

    JS_FN_HELP("currentgc", CurrentGC, 0, 0,
"currentgc()",
"  Report various information about the currently running incremental GC,\n"
"  if one is running."),

    JS_FN_HELP("deterministicgc", DeterministicGC, 1, 0,
"deterministicgc(true|false)",
"  If true, only allow determinstic GCs to run."),

    JS_FN_HELP("dumpGCArenaInfo", DumpGCArenaInfo, 0, 0,
"dumpGCArenaInfo()",
"  Prints information about the different GC things and how they are arranged\n"
"  in arenas.\n"),
#endif

    JS_FN_HELP("gcstate", GCState, 0, 0,
"gcstate([obj])",
"  Report the global GC state, or the GC state for the zone containing |obj|."),

    JS_FN_HELP("schedulezone", ScheduleZoneForGC, 1, 0,
"schedulezone([obj | string])",
"  If obj is given, schedule a GC of obj's zone.\n"
"  If string is given, schedule a GC of the string's zone if possible."),

    JS_FN_HELP("startgc", StartGC, 1, 0,
"startgc([n [, 'shrinking']])",
"  Start an incremental GC and run a slice that processes about n objects.\n"
"  If 'shrinking' is passesd as the optional second argument, perform a\n"
"  shrinking GC rather than a normal GC. If no zones have been selected with\n"
"  schedulegc(), a full GC will be performed."),

    JS_FN_HELP("finishgc", FinishGC, 0, 0,
"finishgc()",
"   Finish an in-progress incremental GC, if none is running then do nothing."),

    JS_FN_HELP("gcslice", GCSlice, 1, 0,
"gcslice([n])",
"  Start or continue an an incremental GC, running a slice that processes about n objects."),

    JS_FN_HELP("abortgc", AbortGC, 1, 0,
"abortgc()",
"  Abort the current incremental GC."),

    JS_FN_HELP("fullcompartmentchecks", FullCompartmentChecks, 1, 0,
"fullcompartmentchecks(true|false)",
"  If true, check for compartment mismatches before every GC."),

    JS_FN_HELP("nondeterministicGetWeakMapKeys", NondeterministicGetWeakMapKeys, 1, 0,
"nondeterministicGetWeakMapKeys(weakmap)",
"  Return an array of the keys in the given WeakMap."),

    JS_FN_HELP("internalConst", InternalConst, 1, 0,
"internalConst(name)",
"  Query an internal constant for the engine. See InternalConst source for\n"
"  the list of constant names."),

    JS_FN_HELP("isProxy", IsProxy, 1, 0,
"isProxy(obj)",
"  If true, obj is a proxy of some sort"),

    JS_FN_HELP("dumpHeap", DumpHeap, 1, 0,
"dumpHeap([filename])",
"  Dump reachable and unreachable objects to the named file, or to stdout. Objects\n"
"  in the nursery are ignored, so if you wish to include them, consider calling\n"
"  minorgc() first."),

    JS_FN_HELP("terminate", Terminate, 0, 0,
"terminate()",
"  Terminate JavaScript execution, as if we had run out of\n"
"  memory or been terminated by the slow script dialog."),

    JS_FN_HELP("readGeckoProfilingStack", ReadGeckoProfilingStack, 0, 0,
"readGeckoProfilingStack()",
"  Reads the jit stack using ProfilingFrameIterator."),

    JS_FN_HELP("enableOsiPointRegisterChecks", EnableOsiPointRegisterChecks, 0, 0,
"enableOsiPointRegisterChecks()",
"  Emit extra code to verify live regs at the start of a VM call are not\n"
"  modified before its OsiPoint."),

    JS_FN_HELP("displayName", DisplayName, 1, 0,
"displayName(fn)",
"  Gets the display name for a function, which can possibly be a guessed or\n"
"  inferred name based on where the function was defined. This can be\n"
"  different from the 'name' property on the function."),

    JS_FN_HELP("isAsmJSCompilationAvailable", IsAsmJSCompilationAvailable, 0, 0,
"isAsmJSCompilationAvailable",
"  Returns whether asm.js compilation is currently available or whether it is disabled\n"
"  (e.g., by the debugger)."),

    JS_FN_HELP("getJitCompilerOptions", GetJitCompilerOptions, 0, 0,
"getJitCompilerOptions()",
"  Return an object describing some of the JIT compiler options.\n"),

    JS_FN_HELP("isAsmJSModule", IsAsmJSModule, 1, 0,
"isAsmJSModule(fn)",
"  Returns whether the given value is a function containing \"use asm\" that has been\n"
"  validated according to the asm.js spec."),

    JS_FN_HELP("isAsmJSFunction", IsAsmJSFunction, 1, 0,
"isAsmJSFunction(fn)",
"  Returns whether the given value is a nested function in an asm.js module that has been\n"
"  both compile- and link-time validated."),

    JS_FN_HELP("wasmIsSupported", WasmIsSupported, 0, 0,
"wasmIsSupported()",
"  Returns a boolean indicating whether WebAssembly is supported on the current device."),

    JS_FN_HELP("wasmIsSupportedByHardware", WasmIsSupportedByHardware, 0, 0,
"wasmIsSupportedByHardware()",
"  Returns a boolean indicating whether WebAssembly is supported on the current hardware (regardless of whether we've enabled support)."),

    JS_FN_HELP("wasmDebuggingIsSupported", WasmDebuggingIsSupported, 0, 0,
"wasmDebuggingIsSupported()",
"  Returns a boolean indicating whether WebAssembly debugging is supported on the current device;\n"
"  returns false also if WebAssembly is not supported"),

    JS_FN_HELP("wasmStreamingIsSupported", WasmStreamingIsSupported, 0, 0,
"wasmStreamingIsSupported()",
"  Returns a boolean indicating whether WebAssembly caching is supported by the runtime."),

    JS_FN_HELP("wasmCachingIsSupported", WasmCachingIsSupported, 0, 0,
"wasmCachingIsSupported()",
"  Returns a boolean indicating whether WebAssembly caching is supported by the runtime."),

    JS_FN_HELP("wasmHugeMemoryIsSupported", WasmHugeMemoryIsSupported, 0, 0,
"wasmHugeMemoryIsSupported()",
"  Returns a boolean indicating whether WebAssembly supports using a large"
"  virtual memory reservation in order to elide bounds checks on this platform."),

    JS_FN_HELP("wasmUsesCranelift", WasmUsesCranelift, 0, 0,
"wasmUsesCranelift()",
"  Returns a boolean indicating whether Cranelift is currently enabled for backend\n"
"  compilation. This doesn't necessarily mean a module will be compiled with \n"
"  Cranelift (e.g. when baseline is also enabled)."),

    JS_FN_HELP("wasmThreadsSupported", WasmThreadsSupported, 0, 0,
"wasmThreadsSupported()",
"  Returns a boolean indicating whether the WebAssembly threads proposal is\n"
"  supported on the current device."),

    JS_FN_HELP("wasmBulkMemSupported", WasmBulkMemSupported, 0, 0,
"wasmBulkMemSupported()",
"  Returns a boolean indicating whether the WebAssembly bulk memory proposal is\n"
"  supported on the current device."),

    JS_FN_HELP("wasmCompileMode", WasmCompileMode, 0, 0,
"wasmCompileMode()",
"  Returns a string indicating the available compile policy: 'baseline', 'ion',\n"
"  'baseline-or-ion', or 'disabled' (if wasm is not available at all)."),

    JS_FN_HELP("wasmTextToBinary", WasmTextToBinary, 1, 0,
"wasmTextToBinary(str)",
"  Translates the given text wasm module into its binary encoding."),

    JS_FN_HELP("wasmExtractCode", WasmExtractCode, 1, 0,
"wasmExtractCode(module[, tier])",
"  Extracts generated machine code from WebAssembly.Module.  The tier is a string,\n"
"  'stable', 'best', 'baseline', or 'ion'; the default is 'stable'.  If the request\n"
"  cannot be satisfied then null is returned.  If the request is 'ion' then block\n"
"  until background compilation is complete."),

    JS_FN_HELP("wasmDis", WasmDisassemble, 1, 0,
"wasmDis(function[, tier])",
"  Disassembles generated machine code from an exported WebAssembly function.\n"
"  The tier is a string, 'stable', 'best', 'baseline', or 'ion'; the default is\n"
"  'stable'."),

    JS_FN_HELP("wasmHasTier2CompilationCompleted", WasmHasTier2CompilationCompleted, 1, 0,
"wasmHasTier2CompilationCompleted(module)",
"  Returns a boolean indicating whether a given module has finished compiled code for tier2. \n"
"This will return true early if compilation isn't two-tiered. "),

    JS_FN_HELP("wasmLoadedFromCache", WasmLoadedFromCache, 1, 0,
"wasmLoadedFromCache(module)",
"  Returns a boolean indicating whether a given module was deserialized directly from a\n"
"  cache (as opposed to compiled from bytecode)."),

    JS_FN_HELP("wasmReftypesEnabled", WasmReftypesEnabled, 1, 0,
"wasmReftypesEnabled()",
"  Returns a boolean indicating whether the WebAssembly reftypes proposal is enabled."),

    JS_FN_HELP("wasmGcEnabled", WasmGcEnabled, 1, 0,
"wasmGcEnabled()",
"  Returns a boolean indicating whether the WebAssembly GC types proposal is enabled."),

    JS_FN_HELP("wasmDebugSupport", WasmDebugSupport, 1, 0,
"wasmDebugSupport()",
"  Returns a boolean indicating whether the WebAssembly compilers support debugging."),

    JS_FN_HELP("isLazyFunction", IsLazyFunction, 1, 0,
"isLazyFunction(fun)",
"  True if fun is a lazy JSFunction."),

    JS_FN_HELP("isRelazifiableFunction", IsRelazifiableFunction, 1, 0,
"isRelazifiableFunction(fun)",
"  True if fun is a JSFunction with a relazifiable JSScript."),

    JS_FN_HELP("enableShellAllocationMetadataBuilder", EnableShellAllocationMetadataBuilder, 0, 0,
"enableShellAllocationMetadataBuilder()",
"  Use ShellAllocationMetadataBuilder to supply metadata for all newly created objects."),

    JS_FN_HELP("getAllocationMetadata", GetAllocationMetadata, 1, 0,
"getAllocationMetadata(obj)",
"  Get the metadata for an object."),

    JS_INLINABLE_FN_HELP("bailout", testingFunc_bailout, 0, 0, TestBailout,
"bailout()",
"  Force a bailout out of ionmonkey (if running in ionmonkey)."),

    JS_FN_HELP("bailAfter", testingFunc_bailAfter, 1, 0,
"bailAfter(number)",
"  Start a counter to bail once after passing the given amount of possible bailout positions in\n"
"  ionmonkey.\n"),


    JS_FN_HELP("inJit", testingFunc_inJit, 0, 0,
"inJit()",
"  Returns true when called within (jit-)compiled code. When jit compilation is disabled this\n"
"  function returns an error string. This function returns false in all other cases.\n"
"  Depending on truthiness, you should continue to wait for compilation to happen or stop execution.\n"),

    JS_FN_HELP("inIon", testingFunc_inIon, 0, 0,
"inIon()",
"  Returns true when called within ion. When ion is disabled or when compilation is abnormally\n"
"  slow to start, this function returns an error string. Otherwise, this function returns false.\n"
"  This behaviour ensures that a falsy value means that we are not in ion, but expect a\n"
"  compilation to occur in the future. Conversely, a truthy value means that we are either in\n"
"  ion or that there is litle or no chance of ion ever compiling the current script."),

    JS_FN_HELP("assertJitStackInvariants", TestingFunc_assertJitStackInvariants, 0, 0,
"assertJitStackInvariants()",
"  Iterates the Jit stack and check that stack invariants hold."),

    JS_FN_HELP("setIonCheckGraphCoherency", SetIonCheckGraphCoherency, 1, 0,
"setIonCheckGraphCoherency(bool)",
"  Set whether Ion should perform graph consistency (DEBUG-only) assertions. These assertions\n"
"  are valuable and should be generally enabled, however they can be very expensive for large\n"
"  (wasm) programs."),

    JS_FN_HELP("serialize", testingFunc_serialize, 1, 0,
"serialize(data, [transferables, [policy]])",
"  Serialize 'data' using JS_WriteStructuredClone. Returns a structured\n"
"  clone buffer object. 'policy' may be an options hash. Valid keys:\n"
"    'SharedArrayBuffer' - either 'allow' (the default) or 'deny'\n"
"      to specify whether SharedArrayBuffers may be serialized.\n"
"    'scope' - SameProcessSameThread, SameProcessDifferentThread,\n"
"      DifferentProcess, or DifferentProcessForIndexedDB. Determines how some\n"
"      values will be serialized. Clone buffers may only be deserialized with a\n"
"      compatible scope. NOTE - For DifferentProcess/DifferentProcessForIndexedDB,\n"
"      must also set SharedArrayBuffer:'deny' if data contains any shared memory\n"
"      object."),

    JS_FN_HELP("deserialize", Deserialize, 1, 0,
"deserialize(clonebuffer[, opts])",
"  Deserialize data generated by serialize. 'opts' is an options hash with one\n"
"  recognized key 'scope', which limits the clone buffers that are considered\n"
"  valid. Allowed values: 'SameProcessSameThread', 'SameProcessDifferentThread',\n"
"  'DifferentProcess', and 'DifferentProcessForIndexedDB'. So for example, a\n"
"  DifferentProcessForIndexedDB clone buffer may be deserialized in any scope, but\n"
"  a SameProcessSameThread clone buffer cannot be deserialized in a\n"
"  DifferentProcess scope."),

    JS_FN_HELP("detachArrayBuffer", DetachArrayBuffer, 1, 0,
"detachArrayBuffer(buffer)",
"  Detach the given ArrayBuffer object from its memory, i.e. as if it\n"
"  had been transferred to a WebWorker."),

    JS_FN_HELP("helperThreadCount", HelperThreadCount, 0, 0,
"helperThreadCount()",
"  Returns the number of helper threads available for off-thread tasks."),

    JS_FN_HELP("enableShapeConsistencyChecks", EnableShapeConsistencyChecks, 0, 0,
"enableShapeConsistencyChecks()",
"  Enable some slow Shape assertions.\n"),

#ifdef JS_TRACE_LOGGING
    JS_FN_HELP("startTraceLogger", EnableTraceLogger, 0, 0,
"startTraceLogger()",
"  Start logging this thread.\n"),

    JS_FN_HELP("stopTraceLogger", DisableTraceLogger, 0, 0,
"stopTraceLogger()",
"  Stop logging this thread."),
#endif

    JS_FN_HELP("reportOutOfMemory", ReportOutOfMemory, 0, 0,
"reportOutOfMemory()",
"  Report OOM, then clear the exception and return undefined. For crash testing."),

    JS_FN_HELP("throwOutOfMemory", ThrowOutOfMemory, 0, 0,
"throwOutOfMemory()",
"  Throw out of memory exception, for OOM handling testing."),

    JS_FN_HELP("reportLargeAllocationFailure", ReportLargeAllocationFailure, 0, 0,
"reportLargeAllocationFailure()",
"  Call the large allocation failure callback, as though a large malloc call failed,\n"
"  then return undefined. In Gecko, this sends a memory pressure notification, which\n"
"  can free up some memory."),

    JS_FN_HELP("findPath", FindPath, 2, 0,
"findPath(start, target)",
"  Return an array describing one of the shortest paths of GC heap edges from\n"
"  |start| to |target|, or |undefined| if |target| is unreachable from |start|.\n"
"  Each element of the array is either of the form:\n"
"    { node: <object or string>, edge: <string describing edge from node> }\n"
"  if the node is a JavaScript object or value; or of the form:\n"
"    { type: <string describing node>, edge: <string describing edge> }\n"
"  if the node is some internal thing that is not a proper JavaScript value\n"
"  (like a shape or a scope chain element). The destination of the i'th array\n"
"  element's edge is the node of the i+1'th array element; the destination of\n"
"  the last array element is implicitly |target|.\n"),

    JS_FN_HELP("shortestPaths", ShortestPaths, 3, 0,
"shortestPaths(start, targets, maxNumPaths)",
"  Return an array of arrays of shortest retaining paths. There is an array of\n"
"  shortest retaining paths for each object in |targets|. The maximum number of\n"
"  paths in each of those arrays is bounded by |maxNumPaths|. Each element in a\n"
"  path is of the form |{ predecessor, edge }|."),

#if defined(DEBUG) || defined(JS_JITSPEW)
    JS_FN_HELP("dumpObject", DumpObject, 1, 0,
"dumpObject()",
"  Dump an internal representation of an object."),
#endif

    JS_FN_HELP("sharedMemoryEnabled", SharedMemoryEnabled, 0, 0,
"sharedMemoryEnabled()",
"  Return true if SharedArrayBuffer and Atomics are enabled"),

    JS_FN_HELP("sharedArrayRawBufferCount", SharedArrayRawBufferCount, 0, 0,
"sharedArrayRawBufferCount()",
"  Return the number of live SharedArrayRawBuffer objects"),

    JS_FN_HELP("sharedArrayRawBufferRefcount", SharedArrayRawBufferRefcount, 0, 0,
"sharedArrayRawBufferRefcount(sab)",
"  Return the reference count of the SharedArrayRawBuffer object held by sab"),

#ifdef NIGHTLY_BUILD
    JS_FN_HELP("objectAddress", ObjectAddress, 1, 0,
"objectAddress(obj)",
"  Return the current address of the object. For debugging only--this\n"
"  address may change during a moving GC."),

    JS_FN_HELP("sharedAddress", SharedAddress, 1, 0,
"sharedAddress(obj)",
"  Return the address of the shared storage of a SharedArrayBuffer."),
#endif

    JS_FN_HELP("evalReturningScope", EvalReturningScope, 1, 0,
"evalReturningScope(scriptStr, [global])",
"  Evaluate the script in a new scope and return the scope.\n"
"  If |global| is present, clone the script to |global| before executing."),

    JS_FN_HELP("cloneAndExecuteScript", ShellCloneAndExecuteScript, 2, 0,
"cloneAndExecuteScript(source, global)",
"  Compile |source| in the current compartment, clone it into |global|'s\n"
"  compartment, and run it there."),

    JS_FN_HELP("backtrace", DumpBacktrace, 1, 0,
"backtrace()",
"  Dump out a brief backtrace."),

    JS_FN_HELP("getBacktrace", GetBacktrace, 1, 0,
"getBacktrace([options])",
"  Return the current stack as a string. Takes an optional options object,\n"
"  which may contain any or all of the boolean properties\n"
"    options.args - show arguments to each function\n"
"    options.locals - show local variables in each frame\n"
"    options.thisprops - show the properties of the 'this' object of each frame\n"),

    JS_FN_HELP("byteSize", ByteSize, 1, 0,
"byteSize(value)",
"  Return the size in bytes occupied by |value|, or |undefined| if value\n"
"  is not allocated in memory.\n"),

    JS_FN_HELP("byteSizeOfScript", ByteSizeOfScript, 1, 0,
"byteSizeOfScript(f)",
"  Return the size in bytes occupied by the function |f|'s JSScript.\n"),

    JS_FN_HELP("setImmutablePrototype", SetImmutablePrototype, 1, 0,
"setImmutablePrototype(obj)",
"  Try to make obj's [[Prototype]] immutable, such that subsequent attempts to\n"
"  change it will fail.  Return true if obj's [[Prototype]] was successfully made\n"
"  immutable (or if it already was immutable), false otherwise.  Throws in case\n"
"  of internal error, or if the operation doesn't even make sense (for example,\n"
"  because the object is a revoked proxy)."),

#ifdef DEBUG
    JS_FN_HELP("dumpStringRepresentation", DumpStringRepresentation, 1, 0,
"dumpStringRepresentation(str)",
"  Print a human-readable description of how the string |str| is represented.\n"),
#endif

    JS_FN_HELP("setLazyParsingDisabled", SetLazyParsingDisabled, 1, 0,
"setLazyParsingDisabled(bool)",
"  Explicitly disable lazy parsing in the current compartment.  The default is that lazy "
"  parsing is not explicitly disabled."),

    JS_FN_HELP("setDiscardSource", SetDiscardSource, 1, 0,
"setDiscardSource(bool)",
"  Explicitly enable source discarding in the current compartment.  The default is that "
"  source discarding is not explicitly enabled."),

    JS_FN_HELP("getConstructorName", GetConstructorName, 1, 0,
"getConstructorName(object)",
"  If the given object was created with `new Ctor`, return the constructor's display name. "
"  Otherwise, return null."),

    JS_FN_HELP("allocationMarker", AllocationMarker, 0, 0,
"allocationMarker([options])",
"  Return a freshly allocated object whose [[Class]] name is\n"
"  \"AllocationMarker\". Such objects are allocated only by calls\n"
"  to this function, never implicitly by the system, making them\n"
"  suitable for use in allocation tooling tests. Takes an optional\n"
"  options object which may contain the following properties:\n"
"    * nursery: bool, whether to allocate the object in the nursery\n"),

    JS_FN_HELP("setGCCallback", SetGCCallback, 1, 0,
"setGCCallback({action:\"...\", options...})",
"  Set the GC callback. action may be:\n"
"    'minorGC' - run a nursery collection\n"
"    'majorGC' - run a major collection, nesting up to a given 'depth'\n"),

#ifdef DEBUG
    JS_FN_HELP("enqueueMark", EnqueueMark, 1, 0,
"enqueueMark(obj|string)",
"  Add an object to the queue of objects to mark at the beginning every GC. (Note\n"
"  that the objects will actually be marked at the beginning of every slice, but\n"
"  after the first slice they will already be marked so nothing will happen.)\n"
"  \n"
"  Instead of an object, a few magic strings may be used:\n"
"    'yield' - cause the current marking slice to end, as if the mark budget were\n"
"      exceeded.\n"
"    'enter-weak-marking-mode' - divide the list into two segments. The items after\n"
"      this string will not be marked until we enter weak marking mode. Note that weak\n"
"      marking mode may be entered zero or multiple times for one GC.\n"
"    'abort-weak-marking-mode' - same as above, but then abort weak marking to fall back\n"
"      on the old iterative marking code path.\n"
"    'drain' - fully drain the mark stack before continuing.\n"
"    'set-color-black' - force everything following in the mark queue to be marked black.\n"
"    'set-color-gray' - continue with the regular GC until gray marking is possible, then force\n"
"       everything following in the mark queue to be marked gray.\n"
"    'unset-color' - stop forcing the mark color."),

    JS_FN_HELP("clearMarkQueue", ClearMarkQueue, 0, 0,
"clearMarkQueue()",
"  Cancel the special marking of all objects enqueue with enqueueMark()."),

    JS_FN_HELP("getMarkQueue", GetMarkQueue, 0, 0,
"getMarkQueue()",
"  Return the current mark queue set up via enqueueMark calls. Note that all\n"
"  returned values will be wrapped into the current compartment, so this loses\n"
"  some fidelity."),
#endif // DEBUG

    JS_FN_HELP("getLcovInfo", GetLcovInfo, 1, 0,
"getLcovInfo(global)",
"  Generate LCOV tracefile for the given compartment.  If no global are provided then\n"
"  the current global is used as the default one.\n"),

#ifdef DEBUG
    JS_FN_HELP("setRNGState", SetRNGState, 2, 0,
"setRNGState(seed0, seed1)",
"  Set this compartment's RNG state.\n"),
#endif

    JS_FN_HELP("getModuleEnvironmentNames", GetModuleEnvironmentNames, 1, 0,
"getModuleEnvironmentNames(module)",
"  Get the list of a module environment's bound names for a specified module.\n"),

    JS_FN_HELP("getModuleEnvironmentValue", GetModuleEnvironmentValue, 2, 0,
"getModuleEnvironmentValue(module, name)",
"  Get the value of a bound name in a module environment.\n"),

#if defined(FUZZING) && defined(__AFL_COMPILER)
    JS_FN_HELP("aflloop", AflLoop, 1, 0,
"aflloop(max_cnt)",
"  Call the __AFL_LOOP() runtime function (see AFL docs)\n"),
#endif

    JS_FN_HELP("monotonicNow", MonotonicNow, 0, 0,
"monotonicNow()",
"  Return a timestamp reflecting the current elapsed system time.\n"
"  This is monotonically increasing.\n"),

    JS_FN_HELP("timeSinceCreation", TimeSinceCreation, 0, 0,
"TimeSinceCreation()",
"  Returns the time in milliseconds since process creation.\n"
"  This uses a clock compatible with the profiler.\n"),

    JS_FN_HELP("isConstructor", IsConstructor, 1, 0,
"isConstructor(value)",
"  Returns whether the value is considered IsConstructor.\n"),

    JS_FN_HELP("getTimeZone", GetTimeZone, 0, 0,
"getTimeZone()",
"  Get the current time zone.\n"),

    JS_FN_HELP("getDefaultLocale", GetDefaultLocale, 0, 0,
"getDefaultLocale()",
"  Get the current default locale.\n"),

    JS_FN_HELP("getCoreCount", GetCoreCount, 0, 0,
"getCoreCount()",
"  Get the number of CPU cores from the platform layer.  Typically this\n"
"  means the number of hyperthreads on systems where that makes sense.\n"),

    JS_FN_HELP("setTimeResolution", SetTimeResolution, 2, 0,
"setTimeResolution(resolution, jitter)",
"  Enables time clamping and jittering. Specify a time resolution in\n"
"  microseconds and whether or not to jitter\n"),

    JS_FN_HELP("scriptedCallerGlobal", ScriptedCallerGlobal, 0, 0,
"scriptedCallerGlobal()",
"  Get the caller's global (or null). See JS::GetScriptedCallerGlobal.\n"),

    JS_FN_HELP("objectGlobal", ObjectGlobal, 1, 0,
"objectGlobal(obj)",
"  Returns the object's global object or null if the object is a wrapper.\n"),

    JS_FN_HELP("isSameCompartment", IsSameCompartment, 2, 0,
"isSameCompartment(obj1, obj2)",
"  Unwraps obj1 and obj2 and returns whether the unwrapped objects are\n"
"  same-compartment.\n"),

    JS_FN_HELP("firstGlobalInCompartment", FirstGlobalInCompartment, 1, 0,
"firstGlobalInCompartment(obj)",
"  Returns the first global in obj's compartment.\n"),

    JS_FN_HELP("assertCorrectRealm", AssertCorrectRealm, 0, 0,
"assertCorrectRealm()",
"  Asserts cx->realm matches callee->realm.\n"),

    JS_FN_HELP("globalLexicals", GlobalLexicals, 0, 0,
"globalLexicals()",
"  Returns an object containing a copy of all global lexical bindings.\n"
"  Example use: let x = 1; assertEq(globalLexicals().x, 1);\n"),

    JS_FN_HELP("monitorType", MonitorType, 3, 0,
"monitorType(fun, index, val)",
"  Adds val's type to the index'th StackTypeSet of the function's\n"
"  JitScript.\n"
"  If fun is null or undefined, the caller's script is used.\n"
"  If the value is the string 'unknown' or 'unknownObject'\n"
"  the TypeSet is marked as unknown or unknownObject, respectively.\n"),

    JS_FN_HELP("baselineCompile", BaselineCompile, 2, 0,
"baselineCompile([fun/code], forceDebugInstrumentation=false)",
"  Baseline-compiles the given JS function or script.\n"
"  Without arguments, baseline-compiles the caller's script; but note\n"
"  that extra boilerplate is needed afterwards to cause the VM to start\n"
"  running the jitcode rather than staying in the interpreter:\n"
"    baselineCompile();  for (var i=0; i<1; i++) {} ...\n"
"  The interpreter will enter the new jitcode at the loop header unless\n"
"  baselineCompile returned a string or threw an error.\n"),

    JS_FN_HELP("markObjectPropertiesUnknown", MarkObjectPropertiesUnknown, 1, 0,
"markObjectPropertiesUnknown(obj)",
"  Mark all objects in obj's object group as having unknown properties.\n"),

    JS_FN_HELP("encodeAsUtf8InBuffer", EncodeAsUtf8InBuffer, 2, 0,
"encodeAsUtf8InBuffer(str, uint8Array)",
"  Encode as many whole code points from the string str into the provided\n"
"  Uint8Array as will completely fit in it, converting lone surrogates to\n"
"  REPLACEMENT CHARACTER.  Return an array [r, w] where |r| is the\n"
"  number of 16-bit units read and |w| is the number of bytes of UTF-8\n"
"  written."),

    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp FuzzingUnsafeTestingFunctions[] = {
#ifdef DEBUG
    JS_FN_HELP("parseRegExp", ParseRegExp, 3, 0,
"parseRegExp(pattern[, flags[, match_only])",
"  Parses a RegExp pattern and returns a tree, potentially throwing."),

    JS_FN_HELP("disRegExp", DisRegExp, 3, 0,
"disRegExp(regexp[, match_only[, input]])",
"  Dumps RegExp bytecode."),
#endif

    JS_FN_HELP("getErrorNotes", GetErrorNotes, 1, 0,
"getErrorNotes(error)",
"  Returns an array of error notes."),

    JS_FN_HELP("setTimeZone", SetTimeZone, 1, 0,
"setTimeZone(tzname)",
"  Set the 'TZ' environment variable to the given time zone and applies the new time zone.\n"
"  An empty string or undefined resets the time zone to its default value.\n"
"  NOTE: The input string is not validated and will be passed verbatim to setenv()."),

JS_FN_HELP("setDefaultLocale", SetDefaultLocale, 1, 0,
"setDefaultLocale(locale)",
"  Set the runtime default locale to the given value.\n"
"  An empty string or undefined resets the runtime locale to its default value.\n"
"  NOTE: The input string is not fully validated, it must be a valid BCP-47 language tag."),

    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp PCCountProfilingTestingFunctions[] = {
    JS_FN_HELP("start", PCCountProfiling_Start, 0, 0,
    "start()",
    "  Start PC count profiling."),

    JS_FN_HELP("stop", PCCountProfiling_Stop, 0, 0,
    "stop()",
    "  Stop PC count profiling."),

    JS_FN_HELP("purge", PCCountProfiling_Purge, 0, 0,
    "purge()",
    "  Purge the collected PC count profiling data."),

    JS_FN_HELP("count", PCCountProfiling_ScriptCount, 0, 0,
    "count()",
    "  Return the number of profiled scripts."),

    JS_FN_HELP("summary", PCCountProfiling_ScriptSummary, 1, 0,
    "summary(index)",
    "  Return the PC count profiling summary for the given script index.\n"
    "  The script index must be in the range [0, pc.count())."),

    JS_FN_HELP("contents", PCCountProfiling_ScriptContents, 1, 0,
    "contents(index)",
    "  Return the complete profiling contents for the given script index.\n"
    "  The script index must be in the range [0, pc.count())."),

    JS_FS_HELP_END
};
// clang-format on

bool js::DefineTestingFunctions(JSContext* cx, HandleObject obj,
                                bool fuzzingSafe_, bool disableOOMFunctions_) {
  fuzzingSafe = fuzzingSafe_;
  if (EnvVarIsDefined("MOZ_FUZZING_SAFE")) {
    fuzzingSafe = true;
  }

  disableOOMFunctions = disableOOMFunctions_;

  if (!fuzzingSafe) {
    if (!JS_DefineFunctionsWithHelp(cx, obj, FuzzingUnsafeTestingFunctions)) {
      return false;
    }

    RootedObject pccount(cx, JS_NewPlainObject(cx));
    if (!pccount) {
      return false;
    }

    if (!JS_DefineProperty(cx, obj, "pccount", pccount, 0)) {
      return false;
    }

    if (!JS_DefineFunctionsWithHelp(cx, pccount,
                                    PCCountProfilingTestingFunctions)) {
      return false;
    }
  }

  return JS_DefineFunctionsWithHelp(cx, obj, TestingFunctions);
}
