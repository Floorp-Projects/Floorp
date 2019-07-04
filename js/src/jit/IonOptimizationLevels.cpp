/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/IonOptimizationLevels.h"

#include "jit/Ion.h"
#include "vm/JSScript.h"

using namespace js;
using namespace js::jit;

namespace js {
namespace jit {

const OptimizationLevelInfo IonOptimizations;

void OptimizationInfo::initNormalOptimizationInfo() {
  level_ = OptimizationLevel::Normal;

  autoTruncate_ = true;
  eaa_ = true;
  edgeCaseAnalysis_ = true;
  eliminateRedundantChecks_ = true;
  inlineInterpreted_ = true;
  inlineNative_ = true;
  licm_ = true;
  gvn_ = true;
  rangeAnalysis_ = true;
  reordering_ = true;
  scalarReplacement_ = true;
  sincos_ = true;
  sink_ = true;

  registerAllocator_ = RegisterAllocator_Backtracking;

  inlineMaxBytecodePerCallSiteMainThread_ = 200;
  inlineMaxBytecodePerCallSiteHelperThread_ = 400;
  inlineMaxCalleeInlinedBytecodeLength_ = 3550;
  inlineMaxTotalBytecodeLength_ = 85000;
  inliningMaxCallerBytecodeLength_ = 1600;
  maxInlineDepth_ = 0;
  smallFunctionMaxInlineDepth_ = 1;
  inliningWarmUpThresholdFactor_ = 0.5;
  inliningRecompileThresholdFactor_ = 4;
}

void OptimizationInfo::initFullOptimizationInfo() {
  initNormalOptimizationInfo();

  level_ = OptimizationLevel::Full;

  inlineMaxBytecodePerCallSiteMainThread_ = 550;
  inlineMaxBytecodePerCallSiteHelperThread_ = 1100;
  maxInlineDepth_ = 3;
  smallFunctionMaxInlineDepth_ = 10;
  inliningWarmUpThresholdFactor_ = 0.125;
}

void OptimizationInfo::initWasmOptimizationInfo() {
  // The Wasm optimization level
  // Disables some passes that don't work well with wasm.

  // Take normal option values for not specified values.
  initNormalOptimizationInfo();

  level_ = OptimizationLevel::Wasm;

  ama_ = true;
  autoTruncate_ = false;
  edgeCaseAnalysis_ = false;
  eliminateRedundantChecks_ = false;
  scalarReplacement_ = false;  // wasm has no objects.
  sincos_ = false;
  sink_ = false;
}

uint32_t OptimizationInfo::compilerWarmUpThreshold(JSScript* script,
                                                   jsbytecode* pc) const {
  MOZ_ASSERT(pc == nullptr || pc == script->code() ||
             JSOp(*pc) == JSOP_LOOPENTRY);

  if (pc == script->code()) {
    pc = nullptr;
  }

  uint32_t warmUpThreshold = baseCompilerWarmUpThreshold();

  // If the script is too large to compile on the main thread, we can still
  // compile it off thread. In these cases, increase the warm-up counter
  // threshold to improve the compilation's type information and hopefully
  // avoid later recompilation.

  if (script->length() > JitOptions.ionMaxScriptSizeMainThread) {
    warmUpThreshold *=
        (script->length() / double(JitOptions.ionMaxScriptSizeMainThread));
  }

  uint32_t numLocalsAndArgs = NumLocalsAndArgs(script);
  if (numLocalsAndArgs > JitOptions.ionMaxLocalsAndArgsMainThread) {
    warmUpThreshold *=
        (numLocalsAndArgs / double(JitOptions.ionMaxLocalsAndArgsMainThread));
  }

  if (!pc || JitOptions.eagerIonCompilation()) {
    return warmUpThreshold;
  }

  // It's more efficient to enter outer loops, rather than inner loops, via OSR.
  // To accomplish this, we use a slightly higher threshold for inner loops.
  // Note that the loop depth is always > 0 so we will prefer non-OSR over OSR.
  uint32_t loopDepth = LoopEntryDepthHint(pc);
  MOZ_ASSERT(loopDepth > 0);
  return warmUpThreshold + loopDepth * (baseCompilerWarmUpThreshold() / 10);
}

uint32_t OptimizationInfo::recompileWarmUpThreshold(JSScript* script,
                                                    jsbytecode* pc) const {
  MOZ_ASSERT(pc == script->code() || *pc == JSOP_LOOPENTRY);

  uint32_t threshold = compilerWarmUpThreshold(script, pc);
  if (*pc != JSOP_LOOPENTRY || JitOptions.eagerIonCompilation()) {
    return threshold;
  }

  // If we're stuck in a long-running loop at a low optimization level, we have
  // to invalidate to be able to tier up. This is worse than recompiling at
  // function entry (because in that case we can use the lazy link mechanism and
  // avoid invalidation completely). Use a very high recompilation threshold for
  // loop edges so that this only affects very long-running loops.

  uint32_t loopDepth = LoopEntryDepthHint(pc);
  MOZ_ASSERT(loopDepth > 0);
  return threshold + loopDepth * (baseCompilerWarmUpThreshold() / 10);
}

OptimizationLevelInfo::OptimizationLevelInfo() {
  infos_[OptimizationLevel::Normal].initNormalOptimizationInfo();
  infos_[OptimizationLevel::Full].initFullOptimizationInfo();
  infos_[OptimizationLevel::Wasm].initWasmOptimizationInfo();

#ifdef DEBUG
  OptimizationLevel level = firstLevel();
  while (!isLastLevel(level)) {
    OptimizationLevel next = nextLevel(level);
    MOZ_ASSERT_IF(level != OptimizationLevel::DontCompile, level < next);
    level = next;
  }
#endif
}

OptimizationLevel OptimizationLevelInfo::nextLevel(
    OptimizationLevel level) const {
  MOZ_ASSERT(!isLastLevel(level));
  switch (level) {
    case OptimizationLevel::DontCompile:
      return OptimizationLevel::Normal;
    case OptimizationLevel::Normal:
      return OptimizationLevel::Full;
    case OptimizationLevel::Full:
    case OptimizationLevel::Wasm:
    case OptimizationLevel::Count:
      break;
  }
  MOZ_CRASH("Unknown optimization level.");
}

OptimizationLevel OptimizationLevelInfo::firstLevel() const {
  return nextLevel(OptimizationLevel::DontCompile);
}

bool OptimizationLevelInfo::isLastLevel(OptimizationLevel level) const {
  return level == OptimizationLevel::Full;
}

OptimizationLevel OptimizationLevelInfo::levelForScript(JSScript* script,
                                                        jsbytecode* pc) const {
  OptimizationLevel prev = OptimizationLevel::DontCompile;

  while (!isLastLevel(prev)) {
    OptimizationLevel level = nextLevel(prev);
    const OptimizationInfo* info = get(level);
    if (script->getWarmUpCount() < info->compilerWarmUpThreshold(script, pc)) {
      return prev;
    }

    prev = level;
  }

  return prev;
}

}  // namespace jit
}  // namespace js
