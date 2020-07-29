/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpCacheIRTranspiler_h
#define jit_WarpCacheIRTranspiler_h

#include "js/AllocPolicy.h"
#include "js/Vector.h"

#include "vm/BytecodeLocation.h"

namespace js {
namespace jit {

class CallInfo;
class MBasicBlock;
class MDefinition;
class MInstruction;
class WarpBuilder;
class WarpCacheIR;

using MDefinitionStackVector = Vector<MDefinition*, 8, SystemAllocPolicy>;

// Generate MIR from a Baseline ICStub's CacheIR.
MOZ_MUST_USE bool TranspileCacheIRToMIR(WarpBuilder* builder,
                                        BytecodeLocation loc,
                                        const WarpCacheIR* cacheIRSnapshot,
                                        const MDefinitionStackVector& inputs);

// Overload used to pass information for calls to the transpiler.
MOZ_MUST_USE bool TranspileCacheIRToMIR(WarpBuilder* builder,
                                        BytecodeLocation loc,
                                        const WarpCacheIR* cacheIRSnapshot,
                                        CallInfo& callInfo);

}  // namespace jit
}  // namespace js

#endif /* jit_WarpCacheIRTranspiler_h */
