/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonOptimizationLevels_h
#define jit_IonOptimizationLevels_h

#include "mozilla/EnumeratedArray.h"

#include "jstypes.h"

#include "jit/JitOptions.h"
#include "js/TypeDecls.h"

namespace js {
namespace jit {

// [SMDOC] Ion Optimization Levels
//
// Ion can do aggressive inlining, but inlining a lot of code will have a
// negative effect on compilation time and memory usage. It also means we spend
// more time in the slower Baseline code while compiling the Ion code
// off-thread or after an invalidation.
//
// To address this, Ion consists of two tiers:
//
// * Normal: the first tier (warm-up threshold of 1,000) only inlines small
//           functions one level deep. This tier also has recompile checks to
//           recompile the script when it becomes very hot.
//
// * Full: the second tier (warm-up threshold of 100,000) is only used for very
//         hot code so we can afford inlining a lot more code.
//
// See MRecompileCheck::RecompileCheckType for more info.

enum class OptimizationLevel : uint8_t {
  Normal,
  Full,
  Wasm,
  Count,
  DontCompile
};

#ifdef JS_JITSPEW
inline const char* OptimizationLevelString(OptimizationLevel level) {
  switch (level) {
    case OptimizationLevel::DontCompile:
      return "Optimization_DontCompile";
    case OptimizationLevel::Normal:
      return "Optimization_Normal";
    case OptimizationLevel::Full:
      return "Optimization_Full";
    case OptimizationLevel::Wasm:
      return "Optimization_Wasm";
    case OptimizationLevel::Count:;
  }
  MOZ_CRASH("Invalid OptimizationLevel");
}
#endif

// Class representing the Ion optimization settings for an OptimizationLevel.
class OptimizationInfo {
  OptimizationLevel level_;

  // Toggles whether Effective Address Analysis is performed.
  bool eaa_;

  // Toggles whether Alignment Mask Analysis is performed.
  bool ama_;

  // Toggles whether Edge Case Analysis is used.
  bool edgeCaseAnalysis_;

  // Toggles whether redundant checks get removed.
  bool eliminateRedundantChecks_;

  // Toggles whether interpreted scripts get inlined.
  bool inlineInterpreted_;

  // Toggles whether native scripts get inlined.
  bool inlineNative_;

  // Toggles whether global value numbering is used.
  bool gvn_;

  // Toggles whether loop invariant code motion is performed.
  bool licm_;

  // Toggles whether Range Analysis is used.
  bool rangeAnalysis_;

  // Toggles whether instruction reordering is performed.
  bool reordering_;

  // Toggles whether Truncation based on Range Analysis is used.
  bool autoTruncate_;

  // Toggles whether sincos is used.
  bool sincos_;

  // Toggles whether sink is used.
  bool sink_;

  // Describes which register allocator to use.
  IonRegisterAllocator registerAllocator_;

  // The maximum total bytecode size of an inline call site. We use a lower
  // value if off-thread compilation is not available, to avoid stalling the
  // main thread.
  uint32_t inlineMaxBytecodePerCallSiteHelperThread_;
  uint32_t inlineMaxBytecodePerCallSiteMainThread_;

  // The maximum value we allow for baselineScript->inlinedBytecodeLength_
  // when inlining.
  uint16_t inlineMaxCalleeInlinedBytecodeLength_;

  // The maximum bytecode length we'll inline in a single compilation.
  uint32_t inlineMaxTotalBytecodeLength_;

  // The maximum bytecode length the caller may have,
  // before we stop inlining large functions in that caller.
  uint32_t inliningMaxCallerBytecodeLength_;

  // The maximum inlining depth.
  uint32_t maxInlineDepth_;

  // Toggles whether scalar replacement is used.
  bool scalarReplacement_;

  // The maximum inlining depth for functions.
  //
  // Inlining small functions has almost no compiling overhead
  // and removes the otherwise needed call overhead.
  // The value is currently very low.
  // Actually it is only needed to make sure we don't blow out the stack.
  uint32_t smallFunctionMaxInlineDepth_;

  // How many invocations or loop iterations are needed before calls
  // are inlined, as a fraction of compilerWarmUpThreshold.
  double inliningWarmUpThresholdFactor_;

  // How many invocations or loop iterations are needed before a function
  // is hot enough to recompile the outerScript to inline that function,
  // as a multiplication of inliningWarmUpThreshold.
  uint32_t inliningRecompileThresholdFactor_;

  uint32_t baseCompilerWarmUpThreshold() const {
    switch (level_) {
      case OptimizationLevel::Normal:
        return JitOptions.normalIonWarmUpThreshold;
      case OptimizationLevel::Full:
        return JitOptions.fullIonWarmUpThreshold;
      case OptimizationLevel::DontCompile:
      case OptimizationLevel::Wasm:
      case OptimizationLevel::Count:
        break;
    }
    MOZ_CRASH("Unexpected optimization level");
  }

 public:
  constexpr OptimizationInfo()
      : level_(OptimizationLevel::Normal),
        eaa_(false),
        ama_(false),
        edgeCaseAnalysis_(false),
        eliminateRedundantChecks_(false),
        inlineInterpreted_(false),
        inlineNative_(false),
        gvn_(false),
        licm_(false),
        rangeAnalysis_(false),
        reordering_(false),
        autoTruncate_(false),
        sincos_(false),
        sink_(false),
        registerAllocator_(RegisterAllocator_Backtracking),
        inlineMaxBytecodePerCallSiteHelperThread_(0),
        inlineMaxBytecodePerCallSiteMainThread_(0),
        inlineMaxCalleeInlinedBytecodeLength_(0),
        inlineMaxTotalBytecodeLength_(0),
        inliningMaxCallerBytecodeLength_(0),
        maxInlineDepth_(0),
        scalarReplacement_(false),
        smallFunctionMaxInlineDepth_(0),
        inliningWarmUpThresholdFactor_(0.0),
        inliningRecompileThresholdFactor_(0) {}

  void initNormalOptimizationInfo();
  void initFullOptimizationInfo();
  void initWasmOptimizationInfo();

  OptimizationLevel level() const { return level_; }

  bool inlineInterpreted() const {
    return inlineInterpreted_ && !JitOptions.disableInlining;
  }

  bool inlineNative() const {
    return inlineNative_ && !JitOptions.disableInlining;
  }

  uint32_t compilerWarmUpThreshold(JSScript* script,
                                   jsbytecode* pc = nullptr) const;

  uint32_t recompileWarmUpThreshold(JSScript* script, jsbytecode* pc) const;

  bool gvnEnabled() const { return gvn_ && !JitOptions.disableGvn; }

  bool licmEnabled() const { return licm_ && !JitOptions.disableLicm; }

  bool rangeAnalysisEnabled() const {
    return rangeAnalysis_ && !JitOptions.disableRangeAnalysis;
  }

  bool instructionReorderingEnabled() const {
    return reordering_ && !JitOptions.disableInstructionReordering;
  }

  bool autoTruncateEnabled() const {
    return autoTruncate_ && rangeAnalysisEnabled();
  }

  bool sincosEnabled() const { return sincos_ && !JitOptions.disableSincos; }

  bool sinkEnabled() const { return sink_ && !JitOptions.disableSink; }

  bool eaaEnabled() const { return eaa_ && !JitOptions.disableEaa; }

  bool amaEnabled() const { return ama_ && !JitOptions.disableAma; }

  bool edgeCaseAnalysisEnabled() const {
    return edgeCaseAnalysis_ && !JitOptions.disableEdgeCaseAnalysis;
  }

  bool eliminateRedundantChecksEnabled() const {
    return eliminateRedundantChecks_;
  }

  IonRegisterAllocator registerAllocator() const {
    return JitOptions.forcedRegisterAllocator.valueOr(registerAllocator_);
  }

  bool scalarReplacementEnabled() const {
    return scalarReplacement_ && !JitOptions.disableScalarReplacement;
  }

  uint32_t smallFunctionMaxInlineDepth() const {
    return smallFunctionMaxInlineDepth_;
  }

  bool isSmallFunction(JSScript* script) const;

  uint32_t maxInlineDepth() const { return maxInlineDepth_; }

  uint32_t inlineMaxBytecodePerCallSite(bool offThread) const {
    return (offThread || !JitOptions.limitScriptSize)
               ? inlineMaxBytecodePerCallSiteHelperThread_
               : inlineMaxBytecodePerCallSiteMainThread_;
  }

  uint16_t inlineMaxCalleeInlinedBytecodeLength() const {
    return inlineMaxCalleeInlinedBytecodeLength_;
  }

  uint32_t inlineMaxTotalBytecodeLength() const {
    return inlineMaxTotalBytecodeLength_;
  }

  uint32_t inliningMaxCallerBytecodeLength() const {
    return inliningMaxCallerBytecodeLength_;
  }

  uint32_t inliningWarmUpThreshold() const {
    return baseCompilerWarmUpThreshold() * inliningWarmUpThresholdFactor_;
  }

  uint32_t inliningRecompileThreshold() const {
    return inliningWarmUpThreshold() * inliningRecompileThresholdFactor_;
  }
};

class OptimizationLevelInfo {
 private:
  mozilla::EnumeratedArray<OptimizationLevel, OptimizationLevel::Count,
                           OptimizationInfo>
      infos_;

 public:
  OptimizationLevelInfo();

  const OptimizationInfo* get(OptimizationLevel level) const {
    return &infos_[level];
  }

  OptimizationLevel nextLevel(OptimizationLevel level) const;
  OptimizationLevel firstLevel() const;
  bool isLastLevel(OptimizationLevel level) const;
  OptimizationLevel levelForScript(JSScript* script,
                                   jsbytecode* pc = nullptr) const;
};

extern const OptimizationLevelInfo IonOptimizations;

}  // namespace jit
}  // namespace js

#endif /* jit_IonOptimizationLevels_h */
