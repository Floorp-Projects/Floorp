/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_ValueNumbering_h
#define jit_ValueNumbering_h

#include "jit/IonAllocPolicy.h"
#include "js/HashTable.h"

namespace js {
namespace jit {

class MDefinition;
class MBasicBlock;
class MIRGraph;
class MPhi;
class MInstruction;
class MIRGenerator;

class ValueNumberer
{
    // Value numbering data.
    class VisibleValues
    {
        // Hash policy for ValueSet.
        struct ValueHasher
        {
            typedef const MDefinition *Lookup;
            typedef MDefinition *Key;
            static HashNumber hash(Lookup ins);
            static bool match(Key k, Lookup l);
            static void rekey(Key &k, Key newKey);
        };

        typedef HashSet<MDefinition *, ValueHasher, IonAllocPolicy> ValueSet;

        ValueSet set_;        // Set of visible values

      public:
        explicit VisibleValues(TempAllocator &alloc);
        bool init();

        typedef ValueSet::Ptr Ptr;
        typedef ValueSet::AddPtr AddPtr;

        Ptr findLeader(const MDefinition *def) const;
        AddPtr findLeaderForAdd(MDefinition *def);
        bool insert(AddPtr p, MDefinition *def);
        void overwrite(AddPtr p, MDefinition *def);
        void forget(const MDefinition *def);
        void clear();
#ifdef DEBUG
        bool has(const MDefinition *def) const;
#endif
    };

    typedef Vector<MBasicBlock *, 4, IonAllocPolicy> BlockWorklist;
    typedef Vector<MDefinition *, 4, IonAllocPolicy> DefWorklist;

    MIRGenerator *const mir_;
    MIRGraph &graph_;
    VisibleValues values_;            // Numbered values
    DefWorklist deadDefs_;            // Worklist for deleting values
    BlockWorklist unreachableBlocks_; // Worklist for unreachable blocks
    BlockWorklist remainingBlocks_;   // Blocks remaining with fewer preds
    size_t numBlocksDeleted_;         // Num deleted blocks in current tree
    bool rerun_;                      // Should we run another GVN iteration?
    bool blocksRemoved_;              // Have any blocks been removed?
    bool updateAliasAnalysis_;        // Do we care about AliasAnalysis?
    bool dependenciesBroken_;         // Have we broken AliasAnalysis?

    bool deleteDefsRecursively(MDefinition *def);
    bool pushDeadPhiOperands(MPhi *phi, const MBasicBlock *phiBlock);
    bool pushDeadInsOperands(MInstruction *ins);
    bool deleteDef(MDefinition *def);
    bool processDeadDefs();

    bool removePredecessor(MBasicBlock *block, MBasicBlock *pred);
    bool removeBlocksRecursively(MBasicBlock *block, const MBasicBlock *root);

    MDefinition *simplified(MDefinition *def) const;
    MDefinition *leader(MDefinition *def);
    bool hasLeader(const MPhi *phi, const MBasicBlock *phiBlock) const;
    bool loopHasOptimizablePhi(MBasicBlock *backedge) const;

    bool visitDefinition(MDefinition *def);
    bool visitControlInstruction(MBasicBlock *block, const MBasicBlock *root);
    bool visitBlock(MBasicBlock *block, const MBasicBlock *root);
    bool visitDominatorTree(MBasicBlock *root, size_t *totalNumVisited);
    bool visitGraph();

  public:
    ValueNumberer(MIRGenerator *mir, MIRGraph &graph);

    enum UpdateAliasAnalysisFlag {
        DontUpdateAliasAnalysis,
        UpdateAliasAnalysis,
    };

    // Optimize the graph, performing expression simplification and
    // canonicalization, eliminating statically fully-redundant expressions,
    // deleting dead instructions, and removing unreachable blocks.
    bool run(UpdateAliasAnalysisFlag updateAliasAnalysis);
};

} // namespace jit
} // namespace js

#endif /* jit_ValueNumbering_h */
