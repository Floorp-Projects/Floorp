/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_FlowAliasAnalysis_h
#define jit_FlowAliasAnalysis_h

#include "jit/AliasAnalysisShared.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

namespace js {
namespace jit {

class LoopInfo;
class MIRGraph;
class GraphStoreInfo;

typedef MDefinitionVector BlockStoreInfo;
typedef Vector<BlockStoreInfo*, 50, JitAllocPolicy> GraphStoreVector;

class FlowAliasAnalysis : public AliasAnalysisShared
{
    // Info on the graph.
    LoopInfo* loop_;
    GraphStoreInfo* stores_;

    // Helper vectors. In order to not have to recreate them the whole time.
    MDefinitionVector output_;
    MDefinitionVector worklist_;

  public:
    FlowAliasAnalysis(MIRGenerator* mir, MIRGraph& graph);
    bool analyze() override;

  protected:
    /* Process instructions. */
    bool processStore(BlockStoreInfo& stores, MDefinition* store);
    bool processLoad(BlockStoreInfo& stores, MDefinition* load);
    bool processDeferredLoads(LoopInfo* info);

    /* Improve dependency and helpers. */
    bool improveDependency(MDefinition* load, MDefinitionVector& inputStores,
                           MDefinitionVector& outputStores);
    bool improveNonAliasedStores(MDefinition* load, MDefinitionVector& inputStores,
                                 MDefinitionVector& outputStores, bool* improved,
                                 bool onlyControlInstructions = false);
    bool improveStoresInFinishedLoops(MDefinition* load, MDefinitionVector& stores, bool* improved);

    bool improveLoopDependency(MDefinition* load, MDefinitionVector& inputStores,
                               MDefinitionVector& outputStores);
    bool deferImproveDependency(MDefinitionVector& stores);

    /* Save dependency info. */
    void saveLoadDependency(MDefinition* load, MDefinitionVector& dependencies);
    bool saveStoreDependency(MDefinition* store, BlockStoreInfo& prevStores);

    /* Helper functions. */
    bool computeBlockStores(MBasicBlock* block);
    bool isLoopInvariant(MDefinition* load, MDefinition* store, bool* loopinvariant);
    bool loopIsFinished(MBasicBlock* loopheader);

};

} // namespace jit
} // namespace js

#endif /* jit_FlowAliasAnalysis_h */
