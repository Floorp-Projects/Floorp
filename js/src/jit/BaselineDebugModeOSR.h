/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineDebugModeOSR_h
#define jit_BaselineDebugModeOSR_h

#include "debugger/DebugAPI.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/JSJitFrameIter.h"

namespace js {
namespace jit {

// Note that this file and the corresponding .cpp implement debug mode
// on-stack recompilation. This is to be distinguished from ordinary
// Baseline->Ion OSR, which is used to jump into compiled loops.

//
// A frame iterator that updates internal JSJitFrameIter in case of
// recompilation of an on-stack baseline script.
//

class DebugModeOSRVolatileJitFrameIter : public JitFrameIter {
  DebugModeOSRVolatileJitFrameIter** stack;
  DebugModeOSRVolatileJitFrameIter* prev;

 public:
  explicit DebugModeOSRVolatileJitFrameIter(JSContext* cx)
      : JitFrameIter(cx->activation()->asJit(),
                     /* mustUnwindActivation */ true) {
    stack = &cx->liveVolatileJitFrameIter_.ref();
    prev = *stack;
    *stack = this;
  }

  ~DebugModeOSRVolatileJitFrameIter() {
    MOZ_ASSERT(*stack == this);
    *stack = prev;
  }

  static void forwardLiveIterators(JSContext* cx, uint8_t* oldAddr,
                                   uint8_t* newAddr);
};

MOZ_MUST_USE bool RecompileOnStackBaselineScriptsForDebugMode(
    JSContext* cx, const DebugAPI::ExecutionObservableSet& obs,
    bool observing);

}  // namespace jit
}  // namespace js

#endif  // jit_BaselineDebugModeOSR_h
