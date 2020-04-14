/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpCacheIRTranspiler_h
#define jit_WarpCacheIRTranspiler_h

#include "js/AllocPolicy.h"
#include "js/Vector.h"

namespace js {
namespace jit {

class MBasicBlock;
class MDefinition;
class MInstruction;
class MIRGenerator;
class WarpCacheIR;

using MDefinitionStackVector = Vector<MDefinition*, 8, SystemAllocPolicy>;

// List of supported ops. Eventually we should use the full CacheIR ops list
// instead.
#define WARP_CACHE_IR_OPS(_)          \
  _(GuardShape)                       \
  _(GuardToObject)                    \
  _(LoadEnclosingEnvironment)         \
  _(LoadDynamicSlotResult)            \
  _(LoadFixedSlotResult)              \
  _(LoadEnvironmentFixedSlotResult)   \
  _(LoadEnvironmentDynamicSlotResult) \
  _(TypeMonitorResult)

// TranspilerOutput contains information from the transpiler that needs to be
// passed back to WarpBuilder.
struct MOZ_STACK_CLASS TranspilerOutput {
  // For ICs that return a result, this is the corresponding MIR instruction.
  MDefinition* result = nullptr;

  TranspilerOutput() = default;

  TranspilerOutput(const TranspilerOutput&) = delete;
  void operator=(const TranspilerOutput&) = delete;
};

// Generate MIR from a Baseline ICStub's CacheIR.
MOZ_MUST_USE bool TranspileCacheIRToMIR(MIRGenerator& mirGen,
                                        MBasicBlock* current,
                                        const WarpCacheIR* snapshot,
                                        const MDefinitionStackVector& inputs,
                                        TranspilerOutput& output);

}  // namespace jit
}  // namespace js

#endif /* jit_WarpCacheIRTranspiler_h */
