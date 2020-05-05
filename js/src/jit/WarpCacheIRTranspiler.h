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

class MBasicBlock;
class MDefinition;
class MInstruction;
class MIRGenerator;
class WarpCacheIR;

using MDefinitionStackVector = Vector<MDefinition*, 8, SystemAllocPolicy>;

// Generate MIR from a Baseline ICStub's CacheIR.
MOZ_MUST_USE bool TranspileCacheIRToMIR(MIRGenerator& mirGen,
                                        BytecodeLocation loc,
                                        MBasicBlock* current,
                                        const WarpCacheIR* snapshot,
                                        const MDefinitionStackVector& inputs);

}  // namespace jit
}  // namespace js

#endif /* jit_WarpCacheIRTranspiler_h */
