/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_alias_analysis_h__
#define jsion_alias_analysis_h__

#include "MIR.h"
#include "MIRGraph.h"

namespace js {
namespace ion {

class MIRGraph;

typedef Vector<MDefinition *, 4, IonAllocPolicy> InstructionVector;

class LoopAliasInfo : public TempObject {
  private:
    LoopAliasInfo *outer_;
    AliasSet loopStores_;
    MBasicBlock *loopHeader_;
    InstructionVector invariantLoads_;

  public:
    LoopAliasInfo(LoopAliasInfo *outer, MBasicBlock *loopHeader)
      : outer_(outer), loopStores_(AliasSet::None()), loopHeader_(loopHeader)
    { }

    void addStore(AliasSet store) {
        loopStores_ = loopStores_ | store;
    }
    MBasicBlock *loopHeader() const {
        return loopHeader_;
    }
    LoopAliasInfo *outer() const {
        return outer_;
    }
    bool addInvariantLoad(MDefinition *ins) {
        return invariantLoads_.append(ins);
    }
    const InstructionVector& invariantLoads() const {
        return invariantLoads_;
    }
    AliasSet loopStores() const {
        return loopStores_;
    }
    MDefinition *firstInstruction() const {
        return *loopHeader_->begin();
    }
};

class AliasAnalysis
{
    MIRGraph &graph_;
    LoopAliasInfo *loop_;

  public:
    AliasAnalysis(MIRGraph &graph);
    bool analyze();
};

} // namespace js
} // namespace ion

#endif // jsion_alias_analysis_h__
