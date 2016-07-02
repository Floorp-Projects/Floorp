/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/EffectiveAddressAnalysis.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace jit;

static void
AnalyzeLsh(TempAllocator& alloc, MLsh* lsh)
{
    if (lsh->specialization() != MIRType::Int32)
        return;

    if (lsh->isRecoveredOnBailout())
        return;

    MDefinition* index = lsh->lhs();
    MOZ_ASSERT(index->type() == MIRType::Int32);

    MConstant* shiftValue = lsh->rhs()->maybeConstantValue();
    if (!shiftValue)
        return;

    if (shiftValue->type() != MIRType::Int32 || !IsShiftInScaleRange(shiftValue->toInt32()))
        return;

    Scale scale = ShiftToScale(shiftValue->toInt32());

    int32_t displacement = 0;
    MInstruction* last = lsh;
    MDefinition* base = nullptr;
    while (true) {
        if (!last->hasOneUse())
            break;

        MUseIterator use = last->usesBegin();
        if (!use->consumer()->isDefinition() || !use->consumer()->toDefinition()->isAdd())
            break;

        MAdd* add = use->consumer()->toDefinition()->toAdd();
        if (add->specialization() != MIRType::Int32 || !add->isTruncated())
            break;

        MDefinition* other = add->getOperand(1 - add->indexOf(*use));

        if (MConstant* otherConst = other->maybeConstantValue()) {
            displacement += otherConst->toInt32();
        } else {
            if (base)
                break;
            base = other;
        }

        last = add;
        if (last->isRecoveredOnBailout())
            return;
    }

    if (!base) {
        uint32_t elemSize = 1 << ScaleToShift(scale);
        if (displacement % elemSize != 0)
            return;

        if (!last->hasOneUse())
            return;

        MUseIterator use = last->usesBegin();
        if (!use->consumer()->isDefinition() || !use->consumer()->toDefinition()->isBitAnd())
            return;

        MBitAnd* bitAnd = use->consumer()->toDefinition()->toBitAnd();
        if (bitAnd->isRecoveredOnBailout())
            return;

        MDefinition* other = bitAnd->getOperand(1 - bitAnd->indexOf(*use));
        MConstant* otherConst = other->maybeConstantValue();
        if (!otherConst || otherConst->type() != MIRType::Int32)
            return;

        uint32_t bitsClearedByShift = elemSize - 1;
        uint32_t bitsClearedByMask = ~uint32_t(otherConst->toInt32());
        if ((bitsClearedByShift & bitsClearedByMask) != bitsClearedByMask)
            return;

        bitAnd->replaceAllUsesWith(last);
        return;
    }

    if (base->isRecoveredOnBailout())
        return;

    MEffectiveAddress* eaddr = MEffectiveAddress::New(alloc, base, index, scale, displacement);
    last->replaceAllUsesWith(eaddr);
    last->block()->insertAfter(last, eaddr);
}

template<typename MWasmMemoryAccessType>
bool
EffectiveAddressAnalysis::tryAddDisplacement(MWasmMemoryAccessType* ins, int32_t o)
{
    // Compute the new offset. Check for overflow.
    uint32_t oldOffset = ins->offset();
    uint32_t newOffset = oldOffset + o;
    if (o < 0 ? (newOffset >= oldOffset) : (newOffset < oldOffset))
        return false;

    // Compute the new offset to the end of the access. Check for overflow
    // here also.
    uint32_t newEnd = newOffset + ins->byteSize();
    if (newEnd < newOffset)
        return false;

    // Determine the range of valid offsets which can be folded into this
    // instruction and check whether our computed offset is within that range.
    size_t range = mir_->foldableOffsetRange(ins);
    if (size_t(newEnd) > range)
        return false;

    // Everything checks out. This is the new offset.
    ins->setOffset(newOffset);
    return true;
}

template<typename MWasmMemoryAccessType>
void
EffectiveAddressAnalysis::analyzeAsmHeapAccess(MWasmMemoryAccessType* ins)
{
    MDefinition* base = ins->base();

    if (base->isConstant()) {
        // Look for heap[i] where i is a constant offset, and fold the offset.
        // By doing the folding now, we simplify the task of codegen; the offset
        // is always the address mode immediate. This also allows it to avoid
        // a situation where the sum of a constant pointer value and a non-zero
        // offset doesn't actually fit into the address mode immediate.
        int32_t imm = base->toConstant()->toInt32();
        if (imm != 0 && tryAddDisplacement(ins, imm)) {
            MInstruction* zero = MConstant::New(graph_.alloc(), Int32Value(0));
            ins->block()->insertBefore(ins, zero);
            ins->replaceBase(zero);
        }

        // If the index is within the minimum heap length, we can optimize
        // away the bounds check.
        if (imm >= 0) {
            int32_t end = (uint32_t)imm + ins->byteSize();
            if (end >= imm && (uint32_t)end <= mir_->minAsmJSHeapLength())
                 ins->removeBoundsCheck();
        }
    } else if (base->isAdd()) {
        // Look for heap[a+i] where i is a constant offset, and fold the offset.
        // Alignment masks have already been moved out of the way by the
        // Alignment Mask Analysis pass.
        MDefinition* op0 = base->toAdd()->getOperand(0);
        MDefinition* op1 = base->toAdd()->getOperand(1);
        if (op0->isConstant())
            mozilla::Swap(op0, op1);
        if (op1->isConstant()) {
            int32_t imm = op1->toConstant()->toInt32();
            if (tryAddDisplacement(ins, imm))
                ins->replaceBase(op0);
        }
    }
}

// This analysis converts patterns of the form:
//   truncate(x + (y << {0,1,2,3}))
//   truncate(x + (y << {0,1,2,3}) + imm32)
// into a single lea instruction, and patterns of the form:
//   asmload(x + imm32)
//   asmload(x << {0,1,2,3})
//   asmload((x << {0,1,2,3}) + imm32)
//   asmload((x << {0,1,2,3}) & mask)            (where mask is redundant with shift)
//   asmload(((x << {0,1,2,3}) + imm32) & mask)  (where mask is redundant with shift + imm32)
// into a single asmload instruction (and for asmstore too).
//
// Additionally, we should consider the general forms:
//   truncate(x + y + imm32)
//   truncate((y << {0,1,2,3}) + imm32)
bool
EffectiveAddressAnalysis::analyze()
{
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
            if (!graph_.alloc().ensureBallast())
                return false;

            // Note that we don't check for MAsmJSCompareExchangeHeap
            // or MAsmJSAtomicBinopHeap, because the backend and the OOB
            // mechanism don't support non-zero offsets for them yet
            // (TODO bug 1254935).
            if (i->isLsh())
                AnalyzeLsh(graph_.alloc(), i->toLsh());
            else if (i->isAsmJSLoadHeap())
                analyzeAsmHeapAccess(i->toAsmJSLoadHeap());
            else if (i->isAsmJSStoreHeap())
                analyzeAsmHeapAccess(i->toAsmJSStoreHeap());
        }
    }
    return true;
}
