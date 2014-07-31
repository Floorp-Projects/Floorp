/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsapi_tests_jitTestGVN_h
#define jsapi_tests_jitTestGVN_h

#include "jit/IonAnalysis.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/ValueNumbering.h"

namespace js {
namespace jit {

struct MinimalFunc
{
    LifoAlloc lifo;
    TempAllocator alloc;
    JitCompileOptions options;
    CompileInfo info;
    MIRGraph graph;
    MIRGenerator mir;
    uint32_t numParams;

    MinimalFunc()
      : lifo(4096),
        alloc(&lifo),
        options(),
        info(0, SequentialExecution),
        graph(&alloc),
        mir(static_cast<CompileCompartment *>(nullptr), options, &alloc, &graph,
            &info, static_cast<const OptimizationInfo *>(nullptr)),
        numParams(0)
    { }

    MBasicBlock *createEntryBlock()
    {
        MBasicBlock *block = MBasicBlock::NewAsmJS(graph, info, nullptr, MBasicBlock::NORMAL);
        graph.addBlock(block);
        return block;
    }

    MBasicBlock *createBlock(MBasicBlock *pred)
    {
        MBasicBlock *block = MBasicBlock::NewAsmJS(graph, info, pred, MBasicBlock::NORMAL);
        graph.addBlock(block);
        return block;
    }

    MParameter *createParameter()
    {
        MParameter *p = MParameter::New(alloc, numParams++, nullptr);
        return p;
    }

    bool runGVN()
    {
        if (!SplitCriticalEdges(graph))
            return false;
        if (!RenumberBlocks(graph))
            return false;
        if (!BuildDominatorTree(graph))
            return false;
        ValueNumberer gvn(&mir, graph);
        if (!gvn.run(ValueNumberer::DontUpdateAliasAnalysis))
            return false;
        return true;
    }
};

} // namespace jit
} // namespace js

#endif
