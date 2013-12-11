/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/LIR.h"

#include <ctype.h>

#include "jsprf.h"

#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"

using namespace js;
using namespace js::jit;

LIRGraph::LIRGraph(MIRGraph *mir)
  : blocks_(mir->alloc()),
    constantPool_(mir->alloc()),
    safepoints_(mir->alloc()),
    nonCallSafepoints_(mir->alloc()),
    numVirtualRegisters_(0),
    numInstructions_(1), // First id is 1.
    localSlotCount_(0),
    argumentSlotCount_(0),
    entrySnapshot_(nullptr),
    osrBlock_(nullptr),
    mir_(*mir)
{
}

bool
LIRGraph::addConstantToPool(const Value &v, uint32_t *index)
{
    *index = constantPool_.length();
    return constantPool_.append(v);
}

bool
LIRGraph::noteNeedsSafepoint(LInstruction *ins)
{
    // Instructions with safepoints must be in linear order.
    JS_ASSERT_IF(safepoints_.length(), safepoints_[safepoints_.length() - 1]->id() < ins->id());
    if (!ins->isCall() && !nonCallSafepoints_.append(ins))
        return false;
    return safepoints_.append(ins);
}

void
LIRGraph::removeBlock(size_t i)
{
    blocks_.erase(blocks_.begin() + i);
}

Label *
LBlock::label()
{
    return begin()->toLabel()->label();
}

uint32_t
LBlock::firstId()
{
    if (phis_.length()) {
        return phis_[0]->id();
    } else {
        for (LInstructionIterator i(instructions_.begin()); i != instructions_.end(); i++) {
            if (i->id())
                return i->id();
        }
    }
    return 0;
}
uint32_t
LBlock::lastId()
{
    LInstruction *last = *instructions_.rbegin();
    JS_ASSERT(last->id());
    if (last->numDefs())
        return last->getDef(last->numDefs() - 1)->virtualRegister();
    return last->id();
}

LMoveGroup *
LBlock::getEntryMoveGroup(TempAllocator &alloc)
{
    if (entryMoveGroup_)
        return entryMoveGroup_;
    entryMoveGroup_ = new LMoveGroup(alloc);
    JS_ASSERT(begin()->isLabel());
    insertAfter(*begin(), entryMoveGroup_);
    return entryMoveGroup_;
}

LMoveGroup *
LBlock::getExitMoveGroup(TempAllocator &alloc)
{
    if (exitMoveGroup_)
        return exitMoveGroup_;
    exitMoveGroup_ = new LMoveGroup(alloc);
    insertBefore(*rbegin(), exitMoveGroup_);
    return exitMoveGroup_;
}

static size_t
TotalOperandCount(MResumePoint *mir)
{
    size_t accum = mir->numOperands();
    while ((mir = mir->caller()))
        accum += mir->numOperands();
    return accum;
}

LSnapshot::LSnapshot(MResumePoint *mir, BailoutKind kind)
  : numSlots_(TotalOperandCount(mir) * BOX_PIECES),
    slots_(nullptr),
    mir_(mir),
    snapshotOffset_(INVALID_SNAPSHOT_OFFSET),
    bailoutId_(INVALID_BAILOUT_ID),
    bailoutKind_(kind)
{ }

bool
LSnapshot::init(MIRGenerator *gen)
{
    slots_ = gen->allocate<LAllocation>(numSlots_);
    return !!slots_;
}

LSnapshot *
LSnapshot::New(MIRGenerator *gen, MResumePoint *mir, BailoutKind kind)
{
    LSnapshot *snapshot = new LSnapshot(mir, kind);
    if (!snapshot->init(gen))
        return nullptr;

    IonSpew(IonSpew_Snapshots, "Generating LIR snapshot %p from MIR (%p)",
            (void *)snapshot, (void *)mir);

    return snapshot;
}

void
LSnapshot::rewriteRecoveredInput(LUse input)
{
    // Mark any operands to this snapshot with the same value as input as being
    // equal to the instruction's result.
    for (size_t i = 0; i < numEntries(); i++) {
        if (getEntry(i)->isUse() && getEntry(i)->toUse()->virtualRegister() == input.virtualRegister())
            setEntry(i, LUse(input.virtualRegister(), LUse::RECOVERED_INPUT));
    }
}

bool
LPhi::init(MIRGenerator *gen)
{
    inputs_ = gen->allocate<LAllocation>(numInputs_);
    return !!inputs_;
}

LPhi::LPhi(MPhi *mir)
  : numInputs_(mir->numOperands())
{
}

LPhi *
LPhi::New(MIRGenerator *gen, MPhi *ins)
{
    LPhi *phi = new LPhi(ins);
    if (!phi->init(gen))
        return nullptr;
    return phi;
}

void
LInstruction::printName(FILE *fp, Opcode op)
{
    static const char * const names[] =
    {
#define LIROP(x) #x,
        LIR_OPCODE_LIST(LIROP)
#undef LIROP
    };
    const char *name = names[op];
    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++)
        fprintf(fp, "%c", tolower(name[i]));
}

void
LInstruction::printName(FILE *fp)
{
    printName(fp, op());
}

static const char * const TypeChars[] =
{
    "i",            // INTEGER
    "o",            // OBJECT
    "f",            // DOUBLE
#ifdef JS_NUNBOX32
    "t",            // TYPE
    "d"             // PAYLOAD
#elif JS_PUNBOX64
    "x"             // BOX
#endif
};

static void
PrintDefinition(FILE *fp, const LDefinition &def)
{
    fprintf(fp, "[%s", TypeChars[def.type()]);
    if (def.virtualRegister())
        fprintf(fp, ":%d", def.virtualRegister());
    if (def.policy() == LDefinition::PRESET) {
        fprintf(fp, " (%s)", def.output()->toString());
    } else if (def.policy() == LDefinition::MUST_REUSE_INPUT) {
        fprintf(fp, " (!)");
    } else if (def.policy() == LDefinition::PASSTHROUGH) {
        fprintf(fp, " (-)");
    }
    fprintf(fp, "]");
}

#ifdef DEBUG
static void
PrintUse(char *buf, size_t size, const LUse *use)
{
    switch (use->policy()) {
      case LUse::REGISTER:
        JS_snprintf(buf, size, "v%d:r", use->virtualRegister());
        break;
      case LUse::FIXED:
        // Unfortunately, we don't know here whether the virtual register is a
        // float or a double. Should we steal a bit in LUse for help? For now,
        // nothing defines any fixed xmm registers.
        JS_snprintf(buf, size, "v%d:%s", use->virtualRegister(),
                    Registers::GetName(Registers::Code(use->registerCode())));
        break;
      case LUse::ANY:
        JS_snprintf(buf, size, "v%d:r?", use->virtualRegister());
        break;
      case LUse::KEEPALIVE:
        JS_snprintf(buf, size, "v%d:*", use->virtualRegister());
        break;
      case LUse::RECOVERED_INPUT:
        JS_snprintf(buf, size, "v%d:**", use->virtualRegister());
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("invalid use policy");
    }
}

const char *
LAllocation::toString() const
{
    // Not reentrant!
    static char buf[40];

    switch (kind()) {
      case LAllocation::CONSTANT_VALUE:
      case LAllocation::CONSTANT_INDEX:
        return "c";
      case LAllocation::GPR:
        JS_snprintf(buf, sizeof(buf), "=%s", toGeneralReg()->reg().name());
        return buf;
      case LAllocation::FPU:
        JS_snprintf(buf, sizeof(buf), "=%s", toFloatReg()->reg().name());
        return buf;
      case LAllocation::STACK_SLOT:
        JS_snprintf(buf, sizeof(buf), "stack:i%d", toStackSlot()->slot());
        return buf;
      case LAllocation::DOUBLE_SLOT:
        JS_snprintf(buf, sizeof(buf), "stack:d%d", toStackSlot()->slot());
        return buf;
      case LAllocation::INT_ARGUMENT:
        JS_snprintf(buf, sizeof(buf), "arg:%d", toArgument()->index());
        return buf;
      case LAllocation::DOUBLE_ARGUMENT:
        JS_snprintf(buf, sizeof(buf), "arg:%d", toArgument()->index());
        return buf;
      case LAllocation::USE:
        PrintUse(buf, sizeof(buf), toUse());
        return buf;
      default:
        MOZ_ASSUME_UNREACHABLE("what?");
    }
}
#endif // DEBUG

void
LInstruction::printOperands(FILE *fp)
{
    for (size_t i = 0, e = numOperands(); i < e; i++) {
        fprintf(fp, " (%s)", getOperand(i)->toString());
        if (i != numOperands() - 1)
            fprintf(fp, ",");
    }
}

void
LInstruction::assignSnapshot(LSnapshot *snapshot)
{
    JS_ASSERT(!snapshot_);
    snapshot_ = snapshot;

#ifdef DEBUG
    if (IonSpewEnabled(IonSpew_Snapshots)) {
        IonSpewHeader(IonSpew_Snapshots);
        fprintf(IonSpewFile, "Assigning snapshot %p to instruction %p (",
                (void *)snapshot, (void *)this);
        printName(IonSpewFile);
        fprintf(IonSpewFile, ")\n");
    }
#endif
}

void
LInstruction::print(FILE *fp)
{
    fprintf(fp, "{");
    for (size_t i = 0; i < numDefs(); i++) {
        PrintDefinition(fp, *getDef(i));
        if (i != numDefs() - 1)
            fprintf(fp, ", ");
    }
    fprintf(fp, "} <- ");

    printName(fp);


    printInfo(fp);

    if (numTemps()) {
        fprintf(fp, " t=(");
        for (size_t i = 0; i < numTemps(); i++) {
            PrintDefinition(fp, *getTemp(i));
            if (i != numTemps() - 1)
                fprintf(fp, ", ");
        }
        fprintf(fp, ")");
    }
}

void
LInstruction::initSafepoint(TempAllocator &alloc)
{
    JS_ASSERT(!safepoint_);
    safepoint_ = new LSafepoint(alloc);
    JS_ASSERT(safepoint_);
}

bool
LMoveGroup::add(LAllocation *from, LAllocation *to)
{
#ifdef DEBUG
    JS_ASSERT(*from != *to);
    for (size_t i = 0; i < moves_.length(); i++)
        JS_ASSERT(*to != *moves_[i].to());
#endif
    return moves_.append(LMove(from, to));
}

bool
LMoveGroup::addAfter(LAllocation *from, LAllocation *to)
{
    // Transform the operands to this move so that performing the result
    // simultaneously with existing moves in the group will have the same
    // effect as if the original move took place after the existing moves.

    for (size_t i = 0; i < moves_.length(); i++) {
        if (*moves_[i].to() == *from) {
            from = moves_[i].from();
            break;
        }
    }

    if (*from == *to)
        return true;

    for (size_t i = 0; i < moves_.length(); i++) {
        if (*to == *moves_[i].to()) {
            moves_[i] = LMove(from, to);
            return true;
        }
    }

    return add(from, to);
}

void
LMoveGroup::printOperands(FILE *fp)
{
    for (size_t i = 0; i < numMoves(); i++) {
        const LMove &move = getMove(i);
        // Use two printfs, as LAllocation::toString is not reentrant.
        fprintf(fp, "[%s", move.from()->toString());
        fprintf(fp, " -> %s]", move.to()->toString());
        if (i != numMoves() - 1)
            fprintf(fp, ", ");
    }
}
