/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpOracle_h
#define jit_WarpOracle_h

#include "jit/JitAllocPolicy.h"
#include "jit/JitContext.h"
#include "jit/WarpSnapshot.h"

namespace js {
namespace jit {

class MIRGenerator;

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

  AbortReasonOr<WarpEnvironment> createEnvironment(HandleScript script);
  AbortReasonOr<WarpScriptSnapshot*> createScriptSnapshot(HandleScript script);

 public:
  WarpOracle(JSContext* cx, MIRGenerator& mirGen, HandleScript script);

  AbortReasonOr<WarpSnapshot*> createSnapshot();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpOracle_h */
