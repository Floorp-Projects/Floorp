/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/Lowering-shared-inl.h"

#include "jit/LIR.h"
#include "jit/MIR.h"

using namespace js;
using namespace jit;

bool
LIRGeneratorShared::visitConstant(MConstant *ins)
{
    const Value &v = ins->value();
    switch (ins->type()) {
      case MIRType_Boolean:
        return define(new(alloc()) LInteger(v.toBoolean()), ins);
      case MIRType_Int32:
        return define(new(alloc()) LInteger(v.toInt32()), ins);
      case MIRType_String:
        return define(new(alloc()) LPointer(v.toString()), ins);
      case MIRType_Object:
        return define(new(alloc()) LPointer(&v.toObject()), ins);
      default:
        // Constants of special types (undefined, null) should never flow into
        // here directly. Operations blindly consuming them require a Box.
        JS_ASSERT(!"unexpected constant type");
        return false;
    }
}

bool
LIRGeneratorShared::defineTypedPhi(MPhi *phi, size_t lirIndex)
{
    LPhi *lir = current->getPhi(lirIndex);

    uint32_t vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    phi->setVirtualRegister(vreg);
    lir->setDef(0, LDefinition(vreg, LDefinition::TypeFrom(phi->type())));
    annotate(lir);
    return true;
}

void
LIRGeneratorShared::lowerTypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex)
{
    MDefinition *operand = phi->getOperand(inputPosition);
    LPhi *lir = block->getPhi(lirIndex);
    lir->setOperand(inputPosition, LUse(operand->virtualRegister(), LUse::ANY));
}

LRecoverInfo *
LIRGeneratorShared::getRecoverInfo(MResumePoint *rp)
{
    if (cachedRecoverInfo_ && cachedRecoverInfo_->mir() == rp)
        return cachedRecoverInfo_;

    LRecoverInfo *recoverInfo = LRecoverInfo::New(gen, rp);
    if (!recoverInfo)
        return nullptr;

    cachedRecoverInfo_ = recoverInfo;
    return recoverInfo;
}

#ifdef DEBUG
bool
LRecoverInfo::OperandIter::canOptimizeOutIfUnused()
{
    MDefinition *ins = **this;

    // We check ins->type() in addition to ins->isUnused() because
    // EliminateDeadResumePointOperands may replace nodes with the constant
    // MagicValue(JS_OPTIMIZED_OUT).
    if ((ins->isUnused() || ins->type() == MIRType_MagicOptimizedOut) &&
        (*it_)->isResumePoint())
    {
        return !(*it_)->toResumePoint()->isObservableOperand(op_);
    }

    return true;
}
#endif

#ifdef JS_NUNBOX32
LSnapshot *
LIRGeneratorShared::buildSnapshot(LInstruction *ins, MResumePoint *rp, BailoutKind kind)
{
    LRecoverInfo *recoverInfo = getRecoverInfo(rp);
    if (!recoverInfo)
        return nullptr;

    LSnapshot *snapshot = LSnapshot::New(gen, recoverInfo, kind);
    if (!snapshot)
        return nullptr;

    size_t index = 0;
    LRecoverInfo::OperandIter it(recoverInfo->begin());
    LRecoverInfo::OperandIter end(recoverInfo->end());
    for (; it != end; ++it) {
        // Check that optimized out operands are in eliminable slots.
        MOZ_ASSERT(it.canOptimizeOutIfUnused());

        MDefinition *ins = *it;

        if (ins->isRecoveredOnBailout())
            continue;

        LAllocation *type = snapshot->typeOfSlot(index);
        LAllocation *payload = snapshot->payloadOfSlot(index);
        ++index;

        if (ins->isBox())
            ins = ins->toBox()->getOperand(0);

        // Guards should never be eliminated.
        MOZ_ASSERT_IF(ins->isUnused(), !ins->isGuard());

        // Snapshot operands other than constants should never be
        // emitted-at-uses. Try-catch support depends on there being no
        // code between an instruction and the LOsiPoint that follows it.
        MOZ_ASSERT_IF(!ins->isConstant(), !ins->isEmittedAtUses());

        // The register allocation will fill these fields in with actual
        // register/stack assignments. During code generation, we can restore
        // interpreter state with the given information. Note that for
        // constants, including known types, we record a dummy placeholder,
        // since we can recover the same information, much cleaner, from MIR.
        if (ins->isConstant() || ins->isUnused()) {
            *type = LConstantIndex::Bogus();
            *payload = LConstantIndex::Bogus();
        } else if (ins->type() != MIRType_Value) {
            *type = LConstantIndex::Bogus();
            *payload = use(ins, LUse(LUse::KEEPALIVE));
        } else {
            *type = useType(ins, LUse::KEEPALIVE);
            *payload = usePayload(ins, LUse::KEEPALIVE);
        }
    }

    return snapshot;
}

#elif JS_PUNBOX64

LSnapshot *
LIRGeneratorShared::buildSnapshot(LInstruction *ins, MResumePoint *rp, BailoutKind kind)
{
    LRecoverInfo *recoverInfo = getRecoverInfo(rp);
    if (!recoverInfo)
        return nullptr;

    LSnapshot *snapshot = LSnapshot::New(gen, recoverInfo, kind);
    if (!snapshot)
        return nullptr;

    size_t index = 0;
    LRecoverInfo::OperandIter it(recoverInfo->begin());
    LRecoverInfo::OperandIter end(recoverInfo->end());
    for (; it != end; ++it) {
        // Check that optimized out operands are in eliminable slots.
        MOZ_ASSERT(it.canOptimizeOutIfUnused());

        MDefinition *def = *it;

        if (def->isRecoveredOnBailout())
            continue;

        if (def->isBox())
            def = def->toBox()->getOperand(0);

        // Guards should never be eliminated.
        MOZ_ASSERT_IF(def->isUnused(), !def->isGuard());

        // Snapshot operands other than constants should never be
        // emitted-at-uses. Try-catch support depends on there being no
        // code between an instruction and the LOsiPoint that follows it.
        MOZ_ASSERT_IF(!def->isConstant(), !def->isEmittedAtUses());

        LAllocation *a = snapshot->getEntry(index++);

        if (def->isUnused()) {
            *a = LConstantIndex::Bogus();
            continue;
        }

        *a = useKeepaliveOrConstant(def);
    }

    return snapshot;
}
#endif

bool
LIRGeneratorShared::assignSnapshot(LInstruction *ins, BailoutKind kind)
{
    // assignSnapshot must be called before define/add, since
    // it may add new instructions for emitted-at-use operands.
    JS_ASSERT(ins->id() == 0);

    LSnapshot *snapshot = buildSnapshot(ins, lastResumePoint_, kind);
    if (!snapshot)
        return false;

    ins->assignSnapshot(snapshot);
    return true;
}

bool
LIRGeneratorShared::assignSafepoint(LInstruction *ins, MInstruction *mir)
{
    JS_ASSERT(!osiPoint_);
    JS_ASSERT(!ins->safepoint());

    ins->initSafepoint(alloc());

    MResumePoint *mrp = mir->resumePoint() ? mir->resumePoint() : lastResumePoint_;
    LSnapshot *postSnapshot = buildSnapshot(ins, mrp, Bailout_Normal);
    if (!postSnapshot)
        return false;

    osiPoint_ = new(alloc()) LOsiPoint(ins->safepoint(), postSnapshot);

    return lirGraph_.noteNeedsSafepoint(ins);
}

