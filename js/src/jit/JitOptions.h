/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitOptions_h
#define jit_JitOptions_h

#include "mozilla/Maybe.h"

#include "jit/IonTypes.h"
#include "js/TypeDecls.h"

namespace js {
namespace jit {

// Possible register allocators which may be used.
enum IonRegisterAllocator {
  RegisterAllocator_Backtracking,
  RegisterAllocator_Testbed,
};

// Which register to use as base register to access stack slots: frame pointer,
// stack pointer, or whichever is the default for this platform. See comment
// for baseRegForLocals in JitOptions.cpp for more information.
enum class BaseRegForAddress { Default, FP, SP };

static inline mozilla::Maybe<IonRegisterAllocator> LookupRegisterAllocator(
    const char* name) {
  if (!strcmp(name, "backtracking")) {
    return mozilla::Some(RegisterAllocator_Backtracking);
  }
  if (!strcmp(name, "testbed")) {
    return mozilla::Some(RegisterAllocator_Testbed);
  }
  return mozilla::Nothing();
}

struct DefaultJitOptions {
  bool checkGraphConsistency;
#ifdef CHECK_OSIPOINT_REGISTERS
  bool checkOsiPointRegisters;
#endif
  bool checkRangeAnalysis;
  bool runExtraChecks;
  bool disableJitBackend;
  bool disableAma;
  bool disableEaa;
  bool disableEdgeCaseAnalysis;
  bool disableGvn;
  bool disableInlining;
  bool disableLicm;
  bool disablePruning;
  bool disableInstructionReordering;
  bool disableRangeAnalysis;
  bool disableRecoverIns;
  bool disableScalarReplacement;
  bool disableCacheIR;
  bool disableSink;
  bool disableRedundantShapeGuards;
  bool disableBailoutLoopCheck;
  bool baselineInterpreter;
  bool baselineJit;
  bool ion;
  bool jitForTrustedPrincipals;
  bool nativeRegExp;
  bool forceInlineCaches;
  bool forceMegamorphicICs;
  bool fullDebugChecks;
  bool limitScriptSize;
  bool osr;
  bool wasmFoldOffsets;
  bool wasmDelayTier2;
  bool traceRegExpParser;
  bool traceRegExpAssembler;
  bool traceRegExpInterpreter;
  bool traceRegExpPeephole;
  bool lessDebugCode;
  bool enableWatchtowerMegamorphic;
  bool enableWasmJitExit;
  bool enableWasmJitEntry;
  bool enableWasmIonFastCalls;
#ifdef WASM_CODEGEN_DEBUG
  bool enableWasmImportCallSpew;
  bool enableWasmFuncCallSpew;
#endif
  uint32_t baselineInterpreterWarmUpThreshold;
  uint32_t baselineJitWarmUpThreshold;
  uint32_t trialInliningWarmUpThreshold;
  uint32_t trialInliningInitialWarmUpCount;
  uint32_t normalIonWarmUpThreshold;
  uint32_t regexpWarmUpThreshold;
  uint32_t exceptionBailoutThreshold;
  uint32_t frequentBailoutThreshold;
  uint32_t maxStackArgs;
  uint32_t osrPcMismatchesBeforeRecompile;
  uint32_t smallFunctionMaxBytecodeLength;
  uint32_t inliningEntryThreshold;
  uint32_t jumpThreshold;
  uint32_t branchPruningHitCountFactor;
  uint32_t branchPruningInstFactor;
  uint32_t branchPruningBlockSpanFactor;
  uint32_t branchPruningEffectfulInstFactor;
  uint32_t branchPruningThreshold;
  uint32_t ionMaxScriptSize;
  uint32_t ionMaxScriptSizeMainThread;
  uint32_t ionMaxLocalsAndArgs;
  uint32_t ionMaxLocalsAndArgsMainThread;
  uint32_t wasmBatchBaselineThreshold;
  uint32_t wasmBatchIonThreshold;
  mozilla::Maybe<IonRegisterAllocator> forcedRegisterAllocator;

  // Spectre mitigation flags. Each mitigation has its own flag in order to
  // measure the effectiveness of each mitigation with various proof of
  // concept.
  bool spectreIndexMasking;
  bool spectreObjectMitigations;
  bool spectreStringMitigations;
  bool spectreValueMasking;
  bool spectreJitToCxxCalls;

  bool supportsUnalignedAccesses;
  BaseRegForAddress baseRegForLocals;

  DefaultJitOptions();
  bool isSmallFunction(JSScript* script) const;
  void setEagerBaselineCompilation();
  void setEagerIonCompilation();
  void setNormalIonWarmUpThreshold(uint32_t warmUpThreshold);
  void resetNormalIonWarmUpThreshold();
  void enableGvn(bool val);
  void setFastWarmUp();

  bool eagerIonCompilation() const { return normalIonWarmUpThreshold == 0; }
};

extern DefaultJitOptions JitOptions;

inline bool HasJitBackend() {
#if defined(JS_CODEGEN_NONE)
  return false;
#else
  return !JitOptions.disableJitBackend;
#endif
}

inline bool IsBaselineInterpreterEnabled() {
  return HasJitBackend() && JitOptions.baselineInterpreter;
}

}  // namespace jit

extern mozilla::Atomic<bool> fuzzingSafe;

static inline bool IsFuzzing() {
#ifdef FUZZING
  return true;
#else
  return fuzzingSafe;
#endif
}

}  // namespace js

#endif /* jit_JitOptions_h */
