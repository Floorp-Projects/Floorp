/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpOracle_h
#define jit_WarpOracle_h

#include "jit/JitContext.h"

namespace js {
namespace jit {

class MIRGenerator;

// Snapshot data for a single JSScript.
class WarpScriptSnapshot {
  JSScript* script_;

 public:
  explicit WarpScriptSnapshot(JSScript* script) : script_(script) {}

  JSScript* script() const { return script_; }
};

// Data allocated by WarpOracle on the main thread that's used off-thread by
// WarpBuilder to build the MIR graph.
//
// TODO: trace IC data in IonCompileTask::trace (like MRootList for non-Warp).
class WarpSnapshot {
  // The script to compile.
  WarpScriptSnapshot* script_;

 public:
  explicit WarpSnapshot(WarpScriptSnapshot* script) : script_(script) {}

  WarpScriptSnapshot* script() const { return script_; }
};

// WarpOracle creates a WarpSnapshot data structure that's used by WarpBuilder
// to generate the MIR graph off-thread.
class MOZ_STACK_CLASS WarpOracle {
  JSContext* cx_;
  MIRGenerator& mirGen_;
  TempAllocator& alloc_;
  HandleScript script_;

  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r);
  mozilla::GenericErrorResult<AbortReason> abort(AbortReason r,
                                                 const char* message, ...);

  AbortReasonOr<WarpScriptSnapshot*> createScriptSnapshot(HandleScript script);

 public:
  WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script);

  AbortReasonOr<WarpSnapshot*> createSnapshot();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpOracle_h */
