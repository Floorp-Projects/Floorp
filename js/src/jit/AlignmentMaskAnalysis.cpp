/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AlignmentMaskAnalysis.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace jit;

static bool
IsAlignmentMask(uint32_t m)
{
    // Test whether m is just leading ones and trailing zeros.
    return (-m & ~m) == 0;
}

static void
AnalyzeAsmHeapAddress(MDefinition *ptr, MIRGraph &graph)
{
    // Fold (a+i)&m to (a&m)+i, since the users of the BitAnd include heap
    // accesses. This will expose the redundancy for GVN when expressions
    // like this:
    //   a&m
    //   (a+1)&m,
    //   (a+2)&m,
    // are transformed into this:
    //   a&m
    //   (a&m)+1
    //   (a&m)+2
    // and it will allow the constants to be folded by the
    // EffectiveAddressAnalysis pass.

    if (!ptr->isBitAnd())
        return;

    MDefinition *lhs = ptr->toBitAnd()->getOperand(0);
    MDefinition *rhs = ptr->toBitAnd()->getOperand(1);
    int lhsIndex = 0;
    if (lhs->isConstantValue()) {
        mozilla::Swap(lhs, rhs);
        lhsIndex = 1;
    }
    if (!lhs->isAdd() || !lhs->hasOneUse() || !rhs->isConstantValue())
        return;

    MDefinition *op0 = lhs->toAdd()->getOperand(0);
    MDefinition *op1 = lhs->toAdd()->getOperand(1);
    int op0Index = 0;
    if (op0->isConstantValue()) {
        mozilla::Swap(op0, op1);
        op0Index = 1;
    }
    if (!op1->isConstantValue())
        return;

    uint32_t i = op1->constantValue().toInt32();
    uint32_t m = rhs->constantValue().toInt32();
    if (!IsAlignmentMask(m) || ((i & m) != i))
        return;

    ptr->replaceAllUsesWith(lhs);
    ptr->toBitAnd()->replaceOperand(lhsIndex, op0);
    lhs->toAdd()->replaceOperand(op0Index, ptr);

    MInstructionIterator iter = ptr->block()->begin(ptr->toBitAnd());
    ++iter;
    lhs->block()->moveBefore(*iter, lhs->toAdd());
}

bool
AlignmentMaskAnalysis::analyze()
{
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
            // Note that we don't check for MAsmJSCompareExchangeHeap
            // or MAsmJSAtomicBinopHeap, because the backend and the OOB
            // mechanism don't support non-zero offsets for them yet.
            if (i->isAsmJSLoadHeap())
                AnalyzeAsmHeapAddress(i->toAsmJSLoadHeap()->ptr(), graph_);
            else if (i->isAsmJSStoreHeap())
                AnalyzeAsmHeapAddress(i->toAsmJSStoreHeap()->ptr(), graph_);
        }
    }
    return true;
}
