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

    // Toggles whether global value numbering is optimistic or pessimistic.
    IonGvnKind gvnKind_;

    // Toggles whether loop invariant code motion is performed.
    bool licm_;

    // Toggles whether Unreachable Code Elimination is performed.
    bool uce_;

    // Toggles whether Range Analysis is used.
    bool rangeAnalysis_;

    // Describes which register allocator to use.
    IonRegisterAllocator registerAllocator_;

    // The maximum total bytecode size of an inline call site.
    uint32_t inlineMaxTotalBytecodeLength_;

    // The maximum bytecode length the caller may have,
    // before we stop inlining large functions in that caller.
    uint32_t inliningMaxCallerBytecodeLength_;

    // The maximum inlining depth.
    uint32_t maxInlineDepth_;

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

    uint32_t usesBeforeCompile() const {
        if (js_JitOptions.forceDefaultIonUsesBeforeCompile)
            return js_JitOptions.forcedDefaultIonUsesBeforeCompile;
        return usesBeforeCompile_;
    }

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

    bool eaaEnabled() const {
        return eaa_ && !js_JitOptions.disableEaa;
    }

    bool edgeCaseAnalysisEnabled() const {
        return edgeCaseAnalysis_ && !js_JitOptions.disableEdgeCaseAnalysis;
    }

    bool eliminateRedundantChecksEnabled() const {
        return eliminateRedundantChecks_;
    }

    IonGvnKind gvnKind() const {
        if (!js_JitOptions.forceGvnKind)
            return gvnKind_;
        return js_JitOptions.forcedGvnKind;
    }

    IonRegisterAllocator registerAllocator() const {
        if (!js_JitOptions.forceRegisterAllocator)
            return registerAllocator_;
        return js_JitOptions.forcedRegisterAllocator;
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
        return usesBeforeCompile() * usesBeforeInliningFactor_;
    }
};

class OptimizationInfos
{
  private:
    OptimizationInfo infos_[Optimization_Count - 1];

  public:
    OptimizationInfos();

    const OptimizationInfo *get(OptimizationLevel level) {
        JS_ASSERT(level < Optimization_Count);
        JS_ASSERT(level != Optimization_DontCompile);

        return &infos_[level - 1];
    }

    OptimizationLevel nextLevel(OptimizationLevel level);
    bool isLastLevel(OptimizationLevel level);
    OptimizationLevel levelForUseCount(uint32_t useCount);
};

extern OptimizationInfos js_IonOptimizations;

} // namespace jit
} // namespace js

#endif /* jit_IonOptimizationLevels_h */
