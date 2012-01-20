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

#ifndef jsion_codegen_shared_h__
#define jsion_codegen_shared_h__

#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/LIR.h"
#include "ion/IonCaches.h"
#include "ion/IonMacroAssembler.h"
#include "ion/IonFrames.h"
#include "ion/IonMacroAssembler.h"
#include "ion/Snapshots.h"
#include "ion/Safepoints.h"

namespace js {
namespace ion {

class OutOfLineCode;
class CodeGenerator;

class CodeGeneratorShared : public LInstructionVisitor
{
    js::Vector<OutOfLineCode *, 0, SystemAllocPolicy> outOfLineCode_;

  protected:
    MacroAssembler masm;
    MIRGenerator *gen;
    LIRGraph &graph;
    LBlock *current;
    SnapshotWriter snapshots_;
    IonCode *deoptTable_;
#ifdef DEBUG
    uint32 pushedArgs_;
#endif
    SafepointWriter safepoints_;

    // Mapping from bailout table ID to an offset in the snapshot buffer.
    js::Vector<SnapshotOffset, 0, SystemAllocPolicy> bailouts_;

    // Sorted vector of IonFrameInfo. The vector is sorted by the displacement
    // key of the IonFrameInfo which is used to lookup entries.
    js::Vector<IonFrameInfo, 0, SystemAllocPolicy> frameInfoTable_;

    // Vector of information about generated polymorphic inline caches.
    js::Vector<IonCache, 0, SystemAllocPolicy> cacheList_;

    static inline int32 ToInt32(const LAllocation *a) {
        if (a->isConstantValue()) {
            return a->toConstant()->toInt32();
        }
        if (a->isConstantIndex()) {
            return a->toConstantIndex()->index();
        }
        JS_NOT_REACHED("this is not a constant!");
        return -1;
    }

  protected:
    // The offset of the first instruction of the OSR entry block from the
    // beginning of the code buffer.
    size_t osrEntryOffset_;

    inline void setOsrEntryOffset(size_t offset) {
        JS_ASSERT(osrEntryOffset_ == 0);
        osrEntryOffset_ = offset;
    }
    inline size_t getOsrEntryOffset() const {
        return osrEntryOffset_;
    }

  protected:
    // The initial size of the frame in bytes. These are bytes beyond the
    // constant header present for every Ion frame, used for pre-determined
    // spills.
    int32 frameDepth_;

    // Frame class this frame's size falls into (see IonFrame.h).
    FrameSizeClass frameClass_;

    // For arguments to the current function.
    inline int32 ArgToStackOffset(int32 slot) const {
        return masm.framePushed() + sizeof(IonJSFrameLayout) + slot;
    }

    // For the callee of the current function.
    inline int32 CalleeStackOffset() const {
        return masm.framePushed() + IonJSFrameLayout::offsetOfCalleeToken();
    }

    inline int32 SlotToStackOffset(int32 slot) const {
        JS_ASSERT(slot > 0 && slot <= int32(graph.localSlotCount()));
        int32 offset = masm.framePushed() - (slot * STACK_SLOT_SIZE);
        JS_ASSERT(offset >= 0);
        return offset;
    }

    // For argument construction for calls. Argslots are Value-sized.
    inline int32 StackOffsetOfPassedArg(int32 slot) const {
        // A slot of 0 is permitted only to calculate %esp offset for calls.
        JS_ASSERT(slot >= 0 && slot <= int32(graph.argumentSlotCount()));
        int32 offset = masm.framePushed() -
                       (graph.localSlotCount() * STACK_SLOT_SIZE) -
                       (slot * sizeof(Value));
        // Passed arguments go below A function's local stack storage.
        // When arguments are being pushed, there is nothing important on the stack.
        // Therefore, It is safe to push the arguments down arbitrarily.  Pushing
        // by 8 is desirable since everything on the stack is a Value, which is 8
        // bytes large.

        offset &= ~7;
        JS_ASSERT(offset >= 0);
        return offset;
    }

    inline int32 ToStackOffset(const LAllocation *a) const {
        if (a->isArgument())
            return ArgToStackOffset(a->toArgument()->index());
        return SlotToStackOffset(a->toStackSlot()->slot());
    }

    uint32 frameSize() const {
        return frameClass_ == FrameSizeClass::None() ? frameDepth_ : frameClass_.frameSize();
    }

  protected:

    size_t allocateCache(const IonCache &cache) {
        size_t index = cacheList_.length();
        cacheList_.append(cache);
        return index;
    }

  protected:
    // Encodes an LSnapshot into the compressed snapshot buffer, returning
    // false on failure.
    bool encode(LSnapshot *snapshot);
    bool encodeSlots(LSnapshot *snapshot, MResumePoint *resumePoint, uint32 *startIndex);

    // Attempts to assign a BailoutId to a snapshot, if one isn't already set.
    // If the bailout table is full, this returns false, which is not a fatal
    // error (the code generator may use a slower bailout mechanism).
    bool assignBailoutId(LSnapshot *snapshot);

    // Encode a safepoint in the safepoint stream.
    void encodeSafepoint(LSafepoint *safepoint);

    // Assign a FrameInfo to the current offset and encodes the snapshot. This
    // is desirable just after call instructions to recover the FrameInfo from
    // the returnAddress and the calleeToken of the parent frame.
    bool assignFrameInfo(LSafepoint *safepoint, LSnapshot *snapshot);

    // Create a safepoint at the current location. Usually the location is just
    // after a call.
    bool createSafepoint(LInstruction *ins) {
        JS_ASSERT(ins->safepoint());
        return assignFrameInfo(ins->safepoint(), ins->postSnapshot());
    }

    inline bool isNextBlock(LBlock *block) {
        return (current->mir()->id() + 1 == block->mir()->id());
    }

  public:
    // These functions have to be called before and after any callVM and before
    // any modifications of the stack.  Modification of the stack made after
    // these calls should update the framePushed variable, needed by the exit
    // frame produced by callVM.
    inline void saveLive(LInstruction *ins);
    inline void restoreLive(LInstruction *ins);

    template <typename T>
    void pushArg(const T &t) {
        masm.Push(t);
#ifdef DEBUG
        pushedArgs_++;
#endif
    }

    bool callVM(const VMFunction &f, LInstruction *ins);

  protected:
    bool addOutOfLineCode(OutOfLineCode *code);
    bool generateOutOfLineCode();

    void linkAbsoluteLabels() {
    }

  public:
    CodeGeneratorShared(MIRGenerator *gen, LIRGraph &graph);
};

// Wrapper around Label, on the heap, to avoid a bogus assert with OOM.
struct HeapLabel
  : public TempObject,
    public Label
{
};

// An out-of-line path is generated at the end of the function.
class OutOfLineCode : public TempObject
{
    Label entry_;
    Label rejoin_;
    uint32 framePushed_;

  public:
    OutOfLineCode()
      : framePushed_(0)
    { }

    virtual bool generate(CodeGeneratorShared *codegen) = 0;

    Label *entry() {
        return &entry_;
    }
    Label *rejoin() {
        return &rejoin_;
    }
    void setFramePushed(uint32 framePushed) {
        framePushed_ = framePushed;
    }
    uint32 framePushed() const {
        return framePushed_;
    }
};

// For OOL paths that want a specific-typed code generator.
template <typename T>
class OutOfLineCodeBase : public OutOfLineCode
{
  public:
    virtual bool generate(CodeGeneratorShared *codegen) {
        return accept(static_cast<T *>(codegen));
    }

  public:
    virtual bool accept(T *codegen) = 0;
};

} // namespace ion
} // namespace js

#endif // jsion_codegen_shared_h__

