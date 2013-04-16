/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
        JS_ASSERT(pos < AliasSet::NumCategories);
        return pos;
    }
};

AliasAnalysis::AliasAnalysis(MIRGenerator *mir, MIRGraph &graph)
  : mir(mir),
    graph_(graph),
    loop_(NULL)
{
}

// Whether there might be a path from src to dest, excluding loop backedges. This is
// approximate and really ought to depend on precomputed reachability information.
static inline bool
BlockMightReach(MBasicBlock *src, MBasicBlock *dest)
{
    while (src->id() <= dest->id()) {
        if (src == dest)
            return true;
        switch (src->numSuccessors()) {
          case 0:
            return false;
          case 1:
            src = src->getSuccessor(0);
            break;
          default:
            return true;
        }
    }
    return false;
}

static void
IonSpewDependency(MDefinition *load, MDefinition *store, const char *verb, const char *reason)
{
    if (!IonSpewEnabled(IonSpew_Alias))
        return;

    fprintf(IonSpewFile, "Load ");
    load->printName(IonSpewFile);
    fprintf(IonSpewFile, " %s on store ", verb);
    store->printName(IonSpewFile);
    fprintf(IonSpewFile, " (%s)\n", reason);
}

static void
IonSpewAliasInfo(const char *pre, MDefinition *ins, const char *post)
{
    if (!IonSpewEnabled(IonSpew_Alias))
        return;

    fprintf(IonSpewFile, "%s ", pre);
    ins->printName(IonSpewFile);
    fprintf(IonSpewFile, " %s\n", post);
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
    FixedArityList<MDefinitionVector, AliasSet::NumCategories> stores;

    // Initialize to the first instruction.
    MDefinition *firstIns = *graph_.begin()->begin();
    for (unsigned i=0; i < AliasSet::NumCategories; i++) {
        if (!stores[i].append(firstIns))
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
                for (AliasSetIterator iter(set); iter; iter++) {
                    if (!stores[*iter].append(*def))
                        return false;
                }

                if (IonSpewEnabled(IonSpew_Alias)) {
                    fprintf(IonSpewFile, "Processing store ");
                    def->printName(IonSpewFile);
                    fprintf(IonSpewFile, " (flags %x)\n", set.flags());
                }
            } else {
                // Find the most recent store on which this instruction depends.
                MDefinition *lastStore = firstIns;

                for (AliasSetIterator iter(set); iter; iter++) {
                    MDefinitionVector &aliasedStores = stores[*iter];
                    for (int i = aliasedStores.length() - 1; i >= 0; i--) {
                        MDefinition *store = aliasedStores[i];
                        if (def->mightAlias(store) && BlockMightReach(store->block(), *block)) {
                            if (lastStore->id() < store->id())
                                lastStore = store;
                            break;
                        }
                    }
                }

                def->setDependency(lastStore);
                IonSpewDependency(*def, lastStore, "depends", "");

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
            MInstruction *firstLoopIns = *loop_->loopHeader()->begin();

            const InstructionVector &invariant = loop_->invariantLoads();

            for (unsigned i = 0; i < invariant.length(); i++) {
                MDefinition *ins = invariant[i];
                AliasSet set = ins->getAliasSet();
                JS_ASSERT(set.isLoad());

                bool hasAlias = false;
                for (AliasSetIterator iter(set); iter; iter++) {
                    MDefinitionVector &aliasedStores = stores[*iter];
                    for (int i = aliasedStores.length() - 1;; i--) {
                        MDefinition *store = aliasedStores[i];
                        if (store->id() < firstLoopIns->id())
                            break;
                        if (ins->mightAlias(store)) {
                            hasAlias = true;
                            IonSpewDependency(ins, store, "aliases", "store in loop body");
                            break;
                        }
                    }
                    if (hasAlias)
                        break;
                }

                if (hasAlias) {
                    // This instruction depends on stores inside the loop body. Mark it as having a
                    // dependency on the last instruction of the loop header. The last instruction is a
                    // control instruction and these are never hoisted.
                    MControlInstruction *controlIns = loop_->loopHeader()->lastIns();
                    IonSpewDependency(ins, controlIns, "depends", "due to stores in loop body");
                    ins->setDependency(controlIns);
                } else {
                    IonSpewAliasInfo("Load", ins, "does not depend on any stores in this loop");

                    if (outerLoop && ins->dependency()->id() < outerLoop->firstInstruction()->id()) {
                        IonSpewAliasInfo("Load", ins, "may be invariant in outer loop");
                        if (!outerLoop->addInvariantLoad(ins))
                            return false;
                    }
                }
            }
            loop_ = loop_->outer();
        }
    }

    JS_ASSERT(loop_ == NULL);
    return true;
}
