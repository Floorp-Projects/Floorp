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
#include "ion/IonFrames.h"
#include "ion/MIR.h"
#include "CodeGenerator-shared-inl.h"
#include "ion/IonSpewer.h"

using namespace js;
using namespace js::ion;

CodeGeneratorShared::CodeGeneratorShared(MIRGenerator *gen, LIRGraph &graph)
  : gen(gen),
    graph(graph),
    deoptTable_(NULL),
#ifdef DEBUG
    pushedArgs_(0),
#endif
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

    SnapshotOffset offset = snapshots_.startSnapshot(frameCount, snapshot->bailoutKind());

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
        uint32 lirOpcode = 0;
        uint32 mirOpcode = 0;
        uint32 pcOpcode = 0;
        if (ins) {
            lirOpcode = ins->op();
            if (ins->mirRaw()) {
                mirOpcode = ins->mirRaw()->op();
                if (ins->mirRaw()->trackedPc())
                    pcOpcode = *ins->mirRaw()->trackedPc();
            }
        }
        snapshots_.trackFrame(pcOpcode, mirOpcode, lirOpcode);
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

    uintN bailoutId = bailouts_.length();
    snapshot->setBailoutId(bailoutId);
    IonSpew(IonSpew_Snapshots, "Assigned snapshot bailout id %u", bailoutId);
    return bailouts_.append(snapshot->snapshotOffset());
}

void
CodeGeneratorShared::encodeSafepoint(LSafepoint *safepoint)
{
    if (safepoint->encoded())
        return;

    uint32 safepointOffset = safepoints_.startEntry();

    safepoints_.writeGcRegs(safepoint->gcRegs(), safepoint->liveRegs().gprs());
    safepoints_.writeGcSlots(safepoint->gcSlots().length(), safepoint->gcSlots().begin());
#ifdef JS_NUNBOX32
    safepoints_.writeValueSlots(safepoint->valueSlots().length(), safepoint->valueSlots().begin());
    safepoints_.writeNunboxParts(safepoint->nunboxParts().length(), safepoint->nunboxParts().begin());
#endif

    safepoints_.endEntry();
    safepoint->setOffset(safepointOffset);
}

bool
CodeGeneratorShared::assignFrameInfo(LSafepoint *safepoint, LSnapshot *snapshot)
{
    SnapshotOffset snapshotOffset = INVALID_SNAPSHOT_OFFSET;

    if (snapshot) {
        if (!encode(snapshot))
            return false;
        snapshotOffset = snapshot->snapshotOffset();
    }
    encodeSafepoint(safepoint);

    IonSpew(IonSpew_Safepoints, "Attaching safepoint %d to return displacement %d (snapshot: %d)",
            safepoint->offset(), masm.currentOffset(), snapshotOffset);

    IonFrameInfo fi(masm.currentOffset(), safepoint->offset(), snapshotOffset);
    return frameInfoTable_.append(fi);
}

// Before doing any call to Cpp, you should ensure that volatile
// registers are evicted by the register allocator.
bool
CodeGeneratorShared::callVM(const VMFunction &fun, LInstruction *ins)
{
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
    if (!createSafepoint(ins))
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
