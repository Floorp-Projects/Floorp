/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_C1Spewer_h
#define jit_C1Spewer_h

#ifdef DEBUG

#include "NamespaceImports.h"

#include "js/RootingAPI.h"

namespace js {
namespace jit {

class BacktrackingAllocator;
class MBasicBlock;
class MIRGraph;
class LNode;

class C1Spewer
{
    MIRGraph* graph;
    FILE* spewout_;

  public:
    C1Spewer()
      : graph(nullptr), spewout_(nullptr)
    { }

    bool init(const char* path);
    void beginFunction(MIRGraph* graph, HandleScript script);
    void spewPass(const char* pass);
    void spewRanges(const char* pass, BacktrackingAllocator* regalloc);
    void endFunction();
    void finish();

  private:
    void spewPass(FILE* fp, MBasicBlock* block);
    void spewRanges(FILE* fp, BacktrackingAllocator* regalloc, LNode* ins);
    void spewRanges(FILE* fp, MBasicBlock* block, BacktrackingAllocator* regalloc);
};

} // namespace jit
} // namespace js

#endif /* DEBUG */

#endif /* jit_C1Spewer_h */
