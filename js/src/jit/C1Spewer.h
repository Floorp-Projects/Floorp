/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_C1Spewer_h
#define jit_C1Spewer_h

#ifdef DEBUG

#include "NamespaceImports.h"

#include "jit/JitAllocPolicy.h"
#include "js/RootingAPI.h"
#include "vm/Printer.h"

namespace js {
namespace jit {

class BacktrackingAllocator;
class MBasicBlock;
class MIRGraph;
class LNode;

class C1Spewer
{
    MIRGraph* graph;
    LSprinter out_;

  public:
    explicit C1Spewer(TempAllocator *alloc)
      : graph(nullptr), out_(alloc->lifoAlloc())
    { }

    void beginFunction(MIRGraph* graph, JSScript* script);
    void spewPass(const char* pass);
    void spewIntervals(const char* pass, BacktrackingAllocator* regalloc);
    void endFunction();

    void dump(Fprinter &file);

  private:
    void spewPass(GenericPrinter& out, MBasicBlock* block);
    void spewIntervals(GenericPrinter& out, BacktrackingAllocator* regalloc, LNode* ins, size_t& nextId);
    void spewIntervals(GenericPrinter& out, MBasicBlock* block, BacktrackingAllocator* regalloc, size_t& nextId);
};

} // namespace jit
} // namespace js

#endif /* DEBUG */

#endif /* jit_C1Spewer_h */
