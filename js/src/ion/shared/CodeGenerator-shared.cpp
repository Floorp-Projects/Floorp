/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "CodeGenerator-shared.h"
#include "ion/MIRGenerator.h"
#include "ion/IonFrames-inl.h"
#include "ion/MIR.h"
#include "CodeGenerator-shared-inl.h"
#include "ion/IonSpewer.h"
#include "ion/IonMacroAssembler.h"
using namespace js;
using namespace js::ion;

namespace js {
namespace ion {

CodeGeneratorShared::CodeGeneratorShared(MIRGenerator *gen, LIRGraph &graph)
  : gen(gen),
    graph(graph),
    deoptTable_(NULL),
#ifdef DEBUG
    pushedArgs_(0),
#endif
    lastOsiPointOffset_(0),
    osrEntryOffset_(0),
    frameDepth_(graph.localSlotCount() * sizeof(STACK_SLOT_SIZE) +
                graph.argumentSlotCount() * sizeof(Value))
{
    frameClass_ = FrameSizeClass::FromDepth(frameDepth_);
}

bool
CodeGeneratorShared::generateOutOfLineCode()
{
    for (size_t i = 0; i < outOfLineCode_.length(); i++) {
        masm.setFramePushed(outOfLineCode_[i]->framePushed());
        masm.bind(outOfLineCode_[i]->entry());

        if (!outOfLineCode_[i]->generate(this))
            return false;
    }

    return true;
}

bool
CodeGeneratorShared::addOutOfLineCode(OutOfLineCode *code)
{
    code->setFramePushed(masm.framePushed());
    return outOfLineCode_.append(code);
}

static inline int32
ToStackIndex(LAllocation *a)
{
    if (a->isStackSlot()) {
        JS_ASSERT(a->toStackSlot()->slot() >= 1);
        return a->toStackSlot()->slot();
    }
    return -a->toArgument()->index();
}

bool
CodeGeneratorShared::encodeSlots(LSnapshot *snapshot, MResumePoint *resumePoint,
                                 uint32 *startIndex)
{
    IonSpew(IonSpew_Codegen, "Encoding %u of resume point %p's operands starting from %u",
            resumePoint->numOperands(), (void *) resumePoint, *startIndex);
    for (uint32 slotno = 0; slotno < resumePoint->numOperands(); slotno++) {
        uint32 i = slotno + *startIndex;
        MDefinition *mir = resumePoint->getOperand(slotno);

        if (mir->isPassArg())
            mir = mir->toPassArg()->getArgument();
        JS_ASSERT(!mir->isPassArg());

        MIRType type = mir->isUnused()
                       ? MIRType_Undefined
                       : mir->type();

        switch (type) {
          case MIRType_Undefined:
            snapshots_.addUndefinedSlot();
            break;
          case MIRType_Null:
            snapshots_.addNullSlot();
            break;
          case MIRType_Int32:
          case MIRType_String:
          case MIRType_Object:
          case MIRType_Boolean:
          case MIRType_Double:
          {
            LAllocation *payload = snapshot->payloadOfSlot(i);
            JSValueType type = ValueTypeFromMIRType(mir->type());
            if (payload->isMemory()) {
                snapshots_.addSlot(type, ToStackIndex(payload));
            } else if (payload->isGeneralReg()) {
                snapshots_.addSlot(type, ToRegister(payload));
            } else if (payload->isFloatReg()) {
                snapshots_.addSlot(ToFloatRegister(payload));
            } else {
                MConstant *constant = mir->toConstant();
                const Value &v = constant->value();

                // Don't bother with the constant pool for smallish integers.
                if (v.isInt32() && v.toInt32() >= -32 && v.toInt32() <= 32) {
                    snapshots_.addInt32Slot(v.toInt32());
                } else {
                    uint32 index;
                    if (!graph.addConstantToPool(constant, &index))
                        return false;
                    snapshots_.addConstantPoolSlot(index);
                }
            }
            break;
          }
          default:
          {
            JS_ASSERT(mir->type() == MIRType_Value);
            LAllocation *payload = snapshot->payloadOfSlot(i);
#ifdef JS_NUNBOX32
            LAllocation *type = snapshot->typeOfSlot(i);
            if (type->isRegister()) {
                if (payload->isRegister())
                    snapshots_.addSlot(ToRegister(type), ToRegister(payload));
                else
                    snapshots_.addSlot(ToRegister(type), ToStackIndex(payload));
            } else {
                if (payload->isRegister())
                    snapshots_.addSlot(ToStackIndex(type), ToRegister(payload));
                else
                    snapshots_.addSlot(ToStackIndex(type), ToStackIndex(payload));
            }
#elif JS_PUNBOX64
            if (payload->isRegister())
                snapshots_.addSlot(ToRegister(payload));
            else
                snapshots_.addSlot(ToStackIndex(payload));
#endif
            break;
          }
      }
    }

    *startIndex += resumePoint->numOperands();
    return true;
}

bool
CodeGeneratorShared::encode(LSnapshot *snapshot)
{
    if (snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET)
        return true;

    uint32 frameCount = snapshot->mir()->frameCount();

    IonSpew(IonSpew_Snapshots, "Encoding LSnapshot %p (frameCount %u)",
            (void *)snapshot, frameCount);

    MResumePoint::Mode mode = snapshot->mir()->mode();
    JS_ASSERT(mode != MResumePoint::Outer);
    bool resumeAfter = (mode == MResumePoint::ResumeAfter);

    SnapshotOffset offset = snapshots_.startSnapshot(frameCount, snapshot->bailoutKind(),
                                                     resumeAfter);

    FlattenedMResumePointIter mirOperandIter(snapshot->mir());
    if (!mirOperandIter.init())
        return false;
    
    uint32 startIndex = 0;
    for (MResumePoint **it = mirOperandIter.begin(), **end = mirOperandIter.end();
         it != end;
         ++it)
    {
        MResumePoint *mir = *it;
        MBasicBlock *block = mir->block();
        JSFunction *fun = block->info().fun();
        JSScript *script = block->info().script();
        jsbytecode *pc = mir->pc();
        uint32 exprStack = mir->stackDepth() - block->info().ninvoke();
        snapshots_.startFrame(fun, script, pc, exprStack);

#ifdef TRACK_SNAPSHOTS
        LInstruction *ins = instruction();

        uint32 pcOpcode = 0;
        uint32 lirOpcode = 0;
        uint32 lirId = 0;
        uint32 mirOpcode = 0;
        uint32 mirId = 0;

        if (ins) {
            lirOpcode = ins->op();
            lirId = ins->id();
            if (ins->mirRaw()) {
                mirOpcode = ins->mirRaw()->op();
                mirId = ins->mirRaw()->id();
                if (ins->mirRaw()->trackedPc())
                    pcOpcode = *ins->mirRaw()->trackedPc();
            }
        }
        snapshots_.trackFrame(pcOpcode, mirOpcode, mirId, lirOpcode, lirId);
#endif

        encodeSlots(snapshot, mir, &startIndex);
        snapshots_.endFrame();
    }

    snapshots_.endSnapshot();

    snapshot->setSnapshotOffset(offset);

    return !snapshots_.oom();
}

bool
CodeGeneratorShared::assignBailoutId(LSnapshot *snapshot)
{
    JS_ASSERT(snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET);

    // Can we not use bailout tables at all?
    if (!deoptTable_)
        return false;

    JS_ASSERT(frameClass_ != FrameSizeClass::None());

    if (snapshot->bailoutId() != INVALID_BAILOUT_ID)
        return true;

    // Is the bailout table full?
    if (bailouts_.length() >= BAILOUT_TABLE_SIZE)
        return false;

    unsigned bailoutId = bailouts_.length();
    snapshot->setBailoutId(bailoutId);
    IonSpew(IonSpew_Snapshots, "Assigned snapshot bailout id %u", bailoutId);
    return bailouts_.append(snapshot->snapshotOffset());
}

void
CodeGeneratorShared::encodeSafepoint(LSafepoint *safepoint)
{
    if (safepoint->encoded())
        return;
    safepoint->fixupOffset(&masm);

    uint32 safepointOffset = safepoints_.startEntry();

    JS_ASSERT(safepoint->osiCallPointOffset());

    safepoints_.writeOsiCallPointOffset(safepoint->osiCallPointOffset());
    safepoints_.writeGcRegs(safepoint->gcRegs(), safepoint->liveRegs().gprs());
    safepoints_.writeGcSlots(safepoint->gcSlots().length(), safepoint->gcSlots().begin());
#ifdef JS_NUNBOX32
    safepoints_.writeValueSlots(safepoint->valueSlots().length(), safepoint->valueSlots().begin());
    safepoints_.writeNunboxParts(safepoint->nunboxParts().length(), safepoint->nunboxParts().begin());
#endif

    safepoints_.endEntry();
    safepoint->setOffset(safepointOffset);
}

void
CodeGeneratorShared::encodeSafepoints()
{
    for (SafepointIndex *it = safepointIndices_.begin(), *end = safepointIndices_.end();
         it != end;
         ++it)
    {
        LSafepoint *safepoint = it->safepoint();

        // All safepoints must have a valid OSI displacement.
        JS_ASSERT(safepoint->osiCallPointOffset());
        encodeSafepoint(safepoint);
        it->resolve();
    }
}

bool
CodeGeneratorShared::markSafepoint(LInstruction *ins)
{
    return markSafepointAt(masm.currentOffset(), ins);
}

bool
CodeGeneratorShared::markSafepointAt(uint32 offset, LInstruction *ins)
{
    JS_ASSERT_IF(safepointIndices_.length(),
                 offset - safepointIndices_.back().displacement() >= sizeof(uint32));
    return safepointIndices_.append(SafepointIndex(offset, ins->safepoint()));
}

bool
CodeGeneratorShared::markOsiPoint(LOsiPoint *ins, uint32 *callPointOffset)
{
    if (!encode(ins->snapshot()))
        return false;

    // We need to ensure that two OSI point patches don't overlap with
    // each other, so we check that there's enough space for the near
    // call between any two OSI points.
    if (masm.currentOffset() - lastOsiPointOffset_ < Assembler::patchWrite_NearCallSize()) {
        int32 paddingSize = Assembler::patchWrite_NearCallSize();
        paddingSize -= masm.currentOffset() - lastOsiPointOffset_;
        for (int32 i = 0; i < paddingSize; ++i)
            masm.nop();
    }
    JS_ASSERT(masm.currentOffset() - lastOsiPointOffset_ >= Assembler::patchWrite_NearCallSize());
    lastOsiPointOffset_ = masm.currentOffset();

    *callPointOffset = masm.currentOffset();
    SnapshotOffset so = ins->snapshot()->snapshotOffset();
    return osiIndices_.append(OsiIndex(*callPointOffset, so));
}

// Before doing any call to Cpp, you should ensure that volatile
// registers are evicted by the register allocator.
bool
CodeGeneratorShared::callVM(const VMFunction &fun, LInstruction *ins)
{
#ifdef DEBUG
    if (ins->mirRaw()) {
        JS_ASSERT(ins->mirRaw()->isInstruction());
        MInstruction *mir = ins->mirRaw()->toInstruction();
        JS_ASSERT_IF(mir->isEffectful(), mir->resumePoint());
    }
#endif

    // Stack is:
    //    ... frame ...
    //    [args]
#ifdef DEBUG
    JS_ASSERT(pushedArgs_ == fun.explicitArgs);
    pushedArgs_ = 0;
#endif

    // Generate the wrapper of the VM function.
    IonCompartment *ion = gen->cx->compartment->ionCompartment();
    IonCode *wrapper = ion->generateVMWrapper(gen->cx, fun);
    if (!wrapper)
        return false;

    uint32 argumentPadding = 0;
    if (StackKeptAligned) {
        // We add an extra padding after the pushed arguments if we pushed an
        // odd number of arguments. This padding is removed by the wrapper when
        // it returns.
        argumentPadding = (fun.explicitStackSlots() * sizeof(void *)) % StackAlignment;
        masm.reserveStack(argumentPadding);
    }
    // Call the wrapper function.  The wrapper is in charge to unwind the stack
    // when returning from the call.  Failures are handled with exceptions based
    // on the return value of the C functions.  To guard the outcome of the
    // returned value, use another LIR instruction.
    masm.callWithExitFrame(wrapper);
    if (!markSafepoint(ins))
        return false;

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly poped when returning.
    int framePop = sizeof(IonExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(fun.explicitStackSlots() * sizeof(void *) + argumentPadding + framePop);

    // Stack is:
    //    ... frame ...
    return true;
}

class OutOfLineTruncateSlow : public OutOfLineCodeBase<CodeGeneratorShared>
{
    FloatRegister src_;
    Register dest_;

  public:
    OutOfLineTruncateSlow(FloatRegister src, Register dest)
      : src_(src), dest_(dest)
    { }

    bool accept(CodeGeneratorShared *codegen) {
        return codegen->visitOutOfLineTruncateSlow(this);
    }
    FloatRegister src() const {
        return src_;
    }
    Register dest() const {
        return dest_;
    }
};

bool
CodeGeneratorShared::emitTruncateDouble(const FloatRegister &src, const Register &dest)
{
    OutOfLineTruncateSlow *ool = new OutOfLineTruncateSlow(src, dest);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTruncateDouble(src, dest, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorShared::visitOutOfLineTruncateSlow(OutOfLineTruncateSlow *ool)
{
    FloatRegister src = ool->src();
    Register dest = ool->dest();

    saveVolatile(dest);

    masm.setupUnalignedABICall(1, dest);
    masm.passABIArg(src);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));
    masm.storeCallResult(dest);

    restoreVolatile(dest);

    masm.jump(ool->rejoin());
    return true;
}

void
CodeGeneratorShared::emitPreBarrier(Register base, const LAllocation *index, MIRType type)
{
    JSValueType jstype = (type == MIRType_Value)
                         ? JSVAL_TYPE_UNKNOWN
                         : ValueTypeFromMIRType(type);
    if (index->isConstant()) {
        Address address(base, ToInt32(index) * sizeof(Value));
        masm.emitPreBarrier(address, jstype);
    } else {
        BaseIndex address(base, ToRegister(index), TimesEight);
        masm.emitPreBarrier(address, jstype);
    }
}

} // namespace ion
} // namespace js
