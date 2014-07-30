/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonOptimizationLevels_h
#define jit_IonOptimizationLevels_h

#include "jsbytecode.h"
#include "jstypes.h"

#include "jit/JitOptions.h"
#include "js/TypeDecls.h"

namespace js {
namespace jit {

enum OptimizationLevel
{
    Optimization_DontCompile,
    Optimization_Normal,
    Optimization_AsmJS,
    Optimization_Count
};

#ifdef DEBUG
inline const char *
OptimizationLevelString(OptimizationLevel level)
{
    switch (level) {
      case Optimization_DontCompile:
        return "Optimization_DontCompile";
      case Optimization_Normal:
        return "Optimization_Normal";
      case Optimization_AsmJS:
        return "Optimization_AsmJS";
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid OptimizationLevel");
    }
}
#endif

class OptimizationInfo
{
  public:
    OptimizationLevel level_;

    // Toggles whether Effective Address Analysis is performed.
    bool eaa_;

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

    // Toggles whether Unreachable Code Elimination is performed.
    bool uce_;

    // Toggles whether Range Analysis is used.
    bool rangeAnalysis_;

    // Toggles whether Truncation based on Range Analysis is used.
    bool autoTruncate_;

    // Describes which register allocator to use.
    IonRegisterAllocator registerAllocator_;

    // The maximum total bytecode size of an inline call site.
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

    // How many invocations or loop iterations are needed before functions
    // are compiled.
    uint32_t usesBeforeCompile_;

    // How many invocations or loop iterations are needed before calls
    // are inlined, as a fraction of usesBeforeCompile.
    double usesBeforeInliningFactor_;

    OptimizationInfo()
    { }

    void initNormalOptimizationInfo();
    void initAsmjsOptimizationInfo();

    OptimizationLevel level() const {
        return level_;
    }

    bool inlineInterpreted() const {
        return inlineInterpreted_ && !js_JitOptions.disableInlining;
    }

    bool inlineNative() const {
        return inlineNative_ && !js_JitOptions.disableInlining;
    }

    uint32_t usesBeforeCompile(JSScript *script, jsbytecode *pc = nullptr) const;

    bool gvnEnabled() const {
        return gvn_ && !js_JitOptions.disableGvn;
    }

    bool licmEnabled() const {
        return licm_ && !js_JitOptions.disableLicm;
    }

    bool uceEnabled() const {
        return uce_ && !js_JitOptions.disableUce;
    }

    bool rangeAnalysisEnabled() const {
        return rangeAnalysis_ && !js_JitOptions.disableRangeAnalysis;
    }

    bool autoTruncateEnabled() const {
        return autoTruncate_ && rangeAnalysisEnabled();
    }

    bool eaaEnabled() const {
        return eaa_ && !js_JitOptions.disableEaa;
    }

    bool edgeCaseAnalysisEnabled() const {
        return edgeCaseAnalysis_ && !js_JitOptions.disableEdgeCaseAnalysis;
    }

    bool eliminateRedundantChecksEnabled() const {
        return eliminateRedundantChecks_;
    }

    IonRegisterAllocator registerAllocator() const {
        if (!js_JitOptions.forceRegisterAllocator)
            return registerAllocator_;
        return js_JitOptions.forcedRegisterAllocator;
    }

    bool scalarReplacementEnabled() const {
        return scalarReplacement_ && !js_JitOptions.disableScalarReplacement;
    }

    uint32_t smallFunctionMaxInlineDepth() const {
        return smallFunctionMaxInlineDepth_;
    }

    bool isSmallFunction(JSScript *script) const;

    uint32_t maxInlineDepth() const {
        return maxInlineDepth_;
    }

    uint32_t inlineMaxTotalBytecodeLength() const {
        return inlineMaxTotalBytecodeLength_;
    }

    uint32_t inliningMaxCallerBytecodeLength() const {
        return inlineMaxTotalBytecodeLength_;
    }

    uint32_t usesBeforeInlining() const {
        uint32_t usesBeforeCompile = usesBeforeCompile_;
        if (js_JitOptions.forceDefaultIonUsesBeforeCompile)
            usesBeforeCompile = js_JitOptions.forcedDefaultIonUsesBeforeCompile;
        return usesBeforeCompile * usesBeforeInliningFactor_;
    }
};

class OptimizationInfos
{
  private:
    OptimizationInfo infos_[Optimization_Count - 1];

  public:
    OptimizationInfos();

    const OptimizationInfo *get(OptimizationLevel level) const {
        JS_ASSERT(level < Optimization_Count);
        JS_ASSERT(level != Optimization_DontCompile);

        return &infos_[level - 1];
    }

    OptimizationLevel nextLevel(OptimizationLevel level) const;
    OptimizationLevel firstLevel() const;
    bool isLastLevel(OptimizationLevel level) const;
    OptimizationLevel levelForScript(JSScript *script, jsbytecode *pc = nullptr) const;
};

extern OptimizationInfos js_IonOptimizations;

} // namespace jit
} // namespace js

#endif /* jit_IonOptimizationLevels_h */
