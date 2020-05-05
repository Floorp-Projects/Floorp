/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitContext.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Unused.h"

#include "gc/FreeOp.h"
#include "gc/Marking.h"
#include "gc/PublicIterators.h"
#include "jit/AliasAnalysis.h"
#include "jit/AlignmentMaskAnalysis.h"
#include "jit/BacktrackingAllocator.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineInspector.h"
#include "jit/BaselineJIT.h"
#include "jit/CacheIRSpewer.h"
#include "jit/CodeGenerator.h"
#include "jit/EdgeCaseAnalysis.h"
#include "jit/EffectiveAddressAnalysis.h"
#include "jit/FoldLinearArithConstants.h"
#include "jit/InstructionReordering.h"
#include "jit/IonAnalysis.h"
#include "jit/IonBuilder.h"
#include "jit/IonIC.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/JitcodeMap.h"
#include "jit/JitCommon.h"
#include "jit/JitRealm.h"
#include "jit/JitSpewer.h"
#include "jit/LICM.h"
#include "jit/Linker.h"
#include "jit/LIR.h"
#include "jit/Lowering.h"
#include "jit/PerfSpewer.h"
#include "jit/RangeAnalysis.h"
#include "jit/ScalarReplacement.h"
#include "jit/Sink.h"
#include "jit/ValueNumbering.h"
#include "jit/WasmBCE.h"
#include "js/Printf.h"
#include "js/UniquePtr.h"
#include "util/Memory.h"
#include "util/Windows.h"
#include "vm/HelperThreads.h"
#include "vm/Realm.h"
#include "vm/TraceLogging.h"
#ifdef MOZ_VTUNE
#  include "vtune/VTuneWrapper.h"
#endif

#include "debugger/DebugAPI-inl.h"
#include "gc/GC-inl.h"
#include "jit/JitFrames-inl.h"
#include "jit/MacroAssembler-inl.h"
#include "jit/shared/Lowering-shared-inl.h"
#include "vm/EnvironmentObject-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Realm-inl.h"
#include "vm/Stack-inl.h"

#if defined(ANDROID)
#  include <sys/system_properties.h>
#endif

using mozilla::DebugOnly;

using namespace js;
using namespace js::jit;

// Assert that JitCode is gc::Cell aligned.
static_assert(sizeof(JitCode) % gc::CellAlignBytes == 0);

static MOZ_THREAD_LOCAL(JitContext*) TlsJitContext;

static JitContext* CurrentJitContext() {
  if (!TlsJitContext.init()) {
    return nullptr;
  }
  return TlsJitContext.get();
}

void jit::SetJitContext(JitContext* ctx) { TlsJitContext.set(ctx); }

JitContext* jit::GetJitContext() {
  MOZ_ASSERT(CurrentJitContext());
  return CurrentJitContext();
}

JitContext* jit::MaybeGetJitContext() { return CurrentJitContext(); }

JitContext::JitContext(CompileRuntime* rt, CompileRealm* realm,
                       TempAllocator* temp)
    : prev_(CurrentJitContext()), realm_(realm), temp(temp), runtime(rt) {
  MOZ_ASSERT(rt);
  MOZ_ASSERT(realm);
  MOZ_ASSERT(temp);
  SetJitContext(this);
}

JitContext::JitContext(JSContext* cx, TempAllocator* temp)
    : prev_(CurrentJitContext()),
      realm_(CompileRealm::get(cx->realm())),
      cx(cx),
      temp(temp),
      runtime(CompileRuntime::get(cx->runtime())) {
  SetJitContext(this);
}

JitContext::JitContext(TempAllocator* temp)
    : prev_(CurrentJitContext()), temp(temp) {
#ifdef DEBUG
  isCompilingWasm_ = true;
#endif
  SetJitContext(this);
}

JitContext::JitContext() : JitContext(nullptr) {}

JitContext::~JitContext() { SetJitContext(prev_); }

bool jit::InitializeJit() {
  if (!TlsJitContext.init()) {
    return false;
  }

  CheckLogging();

#ifdef JS_CACHEIR_SPEW
  const char* env = getenv("CACHEIR_LOGS");
  if (env && env[0] && env[0] != '0') {
    CacheIRSpewer::singleton().init(env);
  }
#endif

#if defined(JS_CODEGEN_ARM)
  InitARMFlags();
#endif

  // Note: these flags need to be initialized after the InitARMFlags call above.
  JitOptions.supportsFloatingPoint = MacroAssembler::SupportsFloatingPoint();
  JitOptions.supportsUnalignedAccesses =
      MacroAssembler::SupportsUnalignedAccesses();

  CheckPerf();
  return true;
}

bool jit::JitSupportsWasmSimd() {
#if defined(ENABLE_WASM_SIMD)
  return js::jit::MacroAssembler::SupportsWasmSimd();
#else
  MOZ_CRASH("Do not call");
#endif
}

bool jit::JitSupportsAtomics() {
#if defined(JS_CODEGEN_ARM)
  // Bug 1146902, bug 1077318: Enable Ion inlining of Atomics
  // operations on ARM only when the CPU has byte, halfword, and
  // doubleword load-exclusive and store-exclusive instructions,
  // until we can add support for systems that don't have those.
  return js::jit::HasLDSTREXBHD();
#else
  return true;
#endif
}
