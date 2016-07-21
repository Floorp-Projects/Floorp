/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "jit/WasmBCE.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;
using namespace mozilla;

struct DefAndOffset {
    MDefinition* loc;
    uint32_t endOffset;
};

typedef js::HashMap<uint32_t, DefAndOffset, DefaultHasher<uint32_t>, SystemAllocPolicy>
    LastSeenMap;

// The Wasm Bounds Check Elimination (BCE) pass looks for bounds checks
// on SSA values that have already been checked. (in the same block or in a
// dominating block). These bounds checks are redundant and thus eliminated.
//
// Note: This is safe in the presense of dynamic memory sizes as long as they
// can ONLY GROW. If we allow SHRINKING the heap, this pass should be
// RECONSIDERED.
//
// TODO (dbounov): Are there a lot of cases where there is no single dominating
// check, but a set of checks that together dominate a redundant check?
//
// TODO (dbounov): Generalize to constant additions relative to one base
bool jit::EliminateBoundsChecks(MIRGenerator* mir, MIRGraph& graph) {
    // Map for dominating block where a given definition was checked
    LastSeenMap lastSeen;
    if (!lastSeen.init())
        return false;

    for (ReversePostorderIterator bIter(graph.rpoBegin()); bIter != graph.rpoEnd(); bIter++) {
        MBasicBlock* block = *bIter;
        for (MDefinitionIterator dIter(block); dIter;) {
            MDefinition* def = *dIter++;

            switch (def->op()) {
              case MDefinition::Op_WasmBoundsCheck: {
                MWasmBoundsCheck* bc = def->toWasmBoundsCheck();
                MDefinition* addr = def->getOperand(0);
                LastSeenMap::Ptr checkPtr = lastSeen.lookup(addr->id());

                if (checkPtr &&
                    checkPtr->value().endOffset >= bc->endOffset() &&
                    checkPtr->value().loc->block()->dominates(block)) {
                    // Address already checked. Discard current check
                    bc->setRedundant(true);
                } else {
                    DefAndOffset defOff = { def, bc->endOffset() };
                    // Address not previously checked - remember current check
                    if (!lastSeen.put(addr->id(), defOff))
                        return false;
                }
                break;
              }
              case MDefinition::Op_Phi: {
                MPhi* phi = def->toPhi();
                bool phiChecked = true;
                uint32_t off = UINT32_MAX;

                MOZ_ASSERT(phi->numOperands() > 0);

                // If all incoming values to a phi node are safe (i.e. have a
                // check that dominates this block) then we can consider this
                // phi node checked.
                //
                // Note that any phi that is part of a cycle
                // will not be "safe" since the value coming on the backedge
                // cannot be in lastSeen because its block hasn't been traversed yet.
                for (int i = 0, nOps = phi->numOperands(); i < nOps; i++) {
                    MDefinition* src = phi->getOperand(i);
                    LastSeenMap::Ptr checkPtr = lastSeen.lookup(src->id());

                    if (!checkPtr || !checkPtr->value().loc->block()->dominates(block)) {
                        phiChecked = false;
                        break;
                    } else {
                        off = Min(off, checkPtr->value().endOffset);
                    }
                }

                if (phiChecked) {
                    DefAndOffset defOff = { def, off };
                    if (!lastSeen.put(def->id(), defOff))
                        return false;
                }

                break;
              }
              default:
                break;
            }
        }
    }

    return true;
}
