/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "MIR.h"
#include "AliasAnalysis.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonBuilder.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

// Iterates over the flags in an AliasSet.
class AliasSetIterator
{
  private:
    uint32_t flags;
    unsigned pos;

  public:
    AliasSetIterator(AliasSet set)
      : flags(set.flags()), pos(0)
    {
        while (flags && (flags & 1) == 0) {
            flags >>= 1;
            pos++;
        }
    }
    AliasSetIterator& operator ++(int) {
        do {
            flags >>= 1;
            pos++;
        } while (flags && (flags & 1) == 0);
        return *this;
    }
    operator bool() const {
        return !!flags;
    }
    unsigned operator *() const {
        return pos;
    }
};

AliasAnalysis::AliasAnalysis(MIRGenerator *mir, MIRGraph &graph)
  : mir(mir),
    graph_(graph),
    loop_(NULL)
{
}

// This pass annotates every load instruction with the last store instruction
// on which it depends. The algorithm is optimistic in that it ignores explicit
// dependencies and only considers loads and stores.
//
// Loads inside loops only have an implicit dependency on a store before the
// loop header if no instruction inside the loop body aliases it. To calculate
// this efficiently, we maintain a list of maybe-invariant loads and the combined
// alias set for all stores inside the loop. When we see the loop's backedge, this
// information is used to mark every load we wrongly assumed to be loop invaraint as
// having an implicit dependency on the last instruction of the loop header, so that
// it's never moved before the loop header.
//
// The algorithm depends on the invariant that both control instructions and effectful
// instructions (stores) are never hoisted.
bool
AliasAnalysis::analyze()
{
    Vector<MDefinition *, 16, SystemAllocPolicy> stores;

    // Initialize to the first instruction.
    MDefinition *firstIns = *graph_.begin()->begin();
    for (unsigned i=0; i < NUM_ALIAS_SETS; i++) {
        if (!stores.append(firstIns))
            return false;
    }

    // Type analysis may have inserted new instructions. Since this pass depends
    // on the instruction number ordering, all instructions are renumbered.
    // We start with 1 because some passes use 0 to denote failure.
    uint32_t newId = 1;

    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (mir->shouldCancel("Alias Analysis (main loop)"))
            return false;

        if (block->isLoopHeader()) {
            IonSpew(IonSpew_Alias, "Processing loop header %d", block->id());
            loop_ = new LoopAliasInfo(loop_, *block);
        }

        for (MDefinitionIterator def(*block); def; def++) {
            def->setId(newId++);

            AliasSet set = def->getAliasSet();
            if (set.isNone())
                continue;

            if (set.isStore()) {
                for (AliasSetIterator iter(set); iter; iter++)
                    stores[*iter] = *def;

                IonSpew(IonSpew_Alias, "Processing store %d (flags %x)", def->id(), set.flags());

                if (loop_)
                    loop_->addStore(set);
            } else {
                // Find the most recent store on which this instruction depends.
                MDefinition *lastStore = NULL;

                for (AliasSetIterator iter(set); iter; iter++) {
                    MDefinition *store = stores[*iter];
                    if (!lastStore || lastStore->id() < store->id())
                        lastStore = store;
                }

                def->setDependency(lastStore);
                IonSpew(IonSpew_Alias, "Load %d depends on store %d", def->id(), lastStore->id());

                // If the last store was before the current loop, we assume this load
                // is loop invariant. If a later instruction writes to the same location,
                // we will fix this at the end of the loop.
                if (loop_ && lastStore->id() < loop_->firstInstruction()->id()) {
                    if (!loop_->addInvariantLoad(*def))
                        return false;
                }
            }
        }

        if (block->isLoopBackedge()) {
            JS_ASSERT(loop_->loopHeader() == block->loopHeaderOfBackedge());
            IonSpew(IonSpew_Alias, "Processing loop backedge %d (header %d)", block->id(),
                    loop_->loopHeader()->id());
            LoopAliasInfo *outerLoop = loop_->outer();

            // Propagate stores in this loop to the outer loop.
            if (outerLoop)
                outerLoop->addStore(loop_->loopStores());

            const InstructionVector &invariant = loop_->invariantLoads();

            for (unsigned i = 0; i < invariant.length(); i++) {
                MDefinition *ins = invariant[i];
                AliasSet set = ins->getAliasSet();
                JS_ASSERT(set.isLoad());

                if ((loop_->loopStores() & set).isNone()) {
                    IonSpew(IonSpew_Alias, "Load %d does not depend on any stores in this loop",
                            ins->id());

                    if (outerLoop && ins->dependency()->id() < outerLoop->firstInstruction()->id()) {
                        IonSpew(IonSpew_Alias, "Load %d may be invariant in outer loop", ins->id());
                        if (!outerLoop->addInvariantLoad(ins))
                            return false;
                    }
                } else {
                    // This instruction depends on stores inside the loop body. Mark it as having a
                    // dependency on the last instruction of the loop header. The last instruction is a
                    // control instruction and these are never hoisted.
                    MControlInstruction *controlIns = loop_->loopHeader()->lastIns();
                    IonSpew(IonSpew_Alias, "Load %d depends on %d (due to stores in loop body)",
                            ins->id(), controlIns->id());
                    ins->setDependency(controlIns);
                }
            }
            loop_ = loop_->outer();
        }
    }

    JS_ASSERT(loop_ == NULL);
    return true;
}
