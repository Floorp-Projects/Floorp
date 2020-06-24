/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpBuilderShared_h
#define jit_WarpBuilderShared_h

#include "jit/MIR.h"

namespace js {
namespace jit {

class CallInfo;
class WarpSnapshot;

// Base class for code sharing between WarpBuilder and WarpCacheIRTranspiler.
// Because this code is used by WarpCacheIRTranspiler we should
// generally assume that we only have access to the current basic block.
class WarpBuilderShared {
  WarpSnapshot& snapshot_;
  MIRGenerator& mirGen_;
  TempAllocator& alloc_;

 protected:
  MBasicBlock* current;

  WarpBuilderShared(WarpSnapshot& snapshot, MIRGenerator& mirGen,
                    MBasicBlock* current_);

  WarpSnapshot& snapshot() const { return snapshot_; }
  MIRGenerator& mirGen() { return mirGen_; }
  TempAllocator& alloc() { return alloc_; }

  MOZ_MUST_USE bool resumeAfter(MInstruction* ins, BytecodeLocation loc);

  MConstant* constant(const Value& v);
  void pushConstant(const Value& v);

  MCall* makeCall(CallInfo& callInfo, bool needsThisCheck,
                  WrappedFunction* target = nullptr);
};

}  // namespace jit
}  // namespace js

#endif
