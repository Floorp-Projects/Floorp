/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineCompiler.h"
#include "BaselineIC.h"
#include "BaselineJIT.h"
#include "CompileInfo.h"
#include "IonSpewer.h"
#include "IonFrames-inl.h"

#include "vm/Stack-inl.h"

#include "jsopcodeinlines.h"

using namespace js;
using namespace js::ion;

/**
 * BaselineStackBuilder helps abstract the process of rebuilding the C stack on the heap.
 * It takes a bailout iterator and keeps track of the point on the C stack from which
 * the reconstructed frames will be written.
 *
 * It exposes methods to write data into the heap memory storing the reconstructed
 * stack.  It also exposes method to easily calculate addresses.  This includes both the
 * virtual address that a particular value will be at when it's eventually copied onto
 * the stack, as well as the current actual address of that value (whether on the heap
 * allocated portion being constructed or the existing stack).
 *
 * The abstraction handles transparent re-allocation of the heap memory when it
 * needs to be enlarged to accomodate new data.  Similarly to the C stack, the
 * data that's written to the reconstructed stack grows from high to low in memory.
 *
 * The lowest region of the allocated memory contains a BaselineBailoutInfo structure that
 * points to the start and end of the written data.
 */
struct BaselineStackBuilder
{
    IonBailoutIterator &iter_;
    IonJSFrameLayout *frame_;

    static size_t HeaderSize() {
        return AlignBytes(sizeof(BaselineBailoutInfo), sizeof(void *));
    };
    size_t bufferTotal_;
    size_t bufferAvail_;
    size_t bufferUsed_;
    uint8_t *buffer_;
    BaselineBailoutInfo *header_;

    size_t framePushed_;

    BaselineStackBuilder(IonBailoutIterator &iter, size_t initialSize)
      : iter_(iter),
        frame_(static_cast<IonJSFrameLayout*>(iter.current())),
        bufferTotal_(initialSize),
        bufferAvail_(0),
        bufferUsed_(0),
        buffer_(NULL),
        header_(NULL),
        framePushed_(0)
    {
        JS_ASSERT(bufferTotal_ >= HeaderSize());
    }

    ~BaselineStackBuilder() {
        if (buffer_)
            js_free(buffer_);
    }

    bool init() {
        JS_ASSERT(!buffer_);
        JS_ASSERT(bufferUsed_ == 0);
        buffer_ = reinterpret_cast<uint8_t *>(js_calloc(bufferTotal_));
        if (!buffer_)
            return false;
        bufferAvail_ = bufferTotal_ - HeaderSize();
        bufferUsed_ = 0;

        header_ = reinterpret_cast<BaselineBailoutInfo *>(buffer_);
        header_->incomingStack = frame_;
        header_->copyStackTop = buffer_ + bufferTotal_;
        header_->copyStackBottom = header_->copyStackTop;
        header_->setR0 = 0;
        header_->valueR0 = UndefinedValue();
        header_->setR1 = 0;
        header_->valueR1 = UndefinedValue();
        header_->resumeFramePtr = NULL;
        header_->resumeAddr = NULL;
        header_->monitorStub = NULL;
        return true;
    }

    bool enlarge() {
        JS_ASSERT(buffer_ != NULL);
        size_t newSize = bufferTotal_ * 2;
        uint8_t *newBuffer = reinterpret_cast<uint8_t *>(js_calloc(newSize));
        if (!newBuffer)
            return false;
        memcpy((newBuffer + newSize) - bufferUsed_, header_->copyStackTop, bufferUsed_);
        memcpy(newBuffer, header_, sizeof(BaselineBailoutInfo));
        js_free(buffer_);
        buffer_ = newBuffer;
        bufferTotal_ = newSize;
        bufferAvail_ = newSize - (HeaderSize() + bufferUsed_);

        header_ = reinterpret_cast<BaselineBailoutInfo *>(buffer_);
        header_->copyStackTop = buffer_ + bufferTotal_;
        header_->copyStackBottom = header_->copyStackTop - bufferUsed_;
        return true;
    }

    BaselineBailoutInfo *info() {
        JS_ASSERT(header_ == reinterpret_cast<BaselineBailoutInfo *>(buffer_));
        return header_;
    }

    BaselineBailoutInfo *takeBuffer() {
        JS_ASSERT(header_ == reinterpret_cast<BaselineBailoutInfo *>(buffer_));
        buffer_ = NULL;
        return header_;
    }

    void resetFramePushed() {
        framePushed_ = 0;
    }

    size_t framePushed() const {
        return framePushed_;
    }

    bool subtract(size_t size, const char *info=NULL) {
        // enlarge the buffer if need be.
        while (size > bufferAvail_) {
            if (!enlarge())
                return false;
        }

        // write out element.
        header_->copyStackBottom -= size;
        bufferAvail_ -= size;
        bufferUsed_ += size;
        framePushed_ += size;
        if (info) {
            IonSpew(IonSpew_BaselineBailouts,
                    "      SUB_%03d   %p/%p %-15s",
                    (int) size, header_->copyStackBottom, virtualPointerAtStackOffset(0), info);
        }
        return true;
    }

    template <typename T>
    bool write(const T &t) {
        if (!subtract(sizeof(T)))
            return false;
        memcpy(header_->copyStackBottom, &t, sizeof(T));
        return true;
    }

    template <typename T>
    bool writePtr(T *t, const char *info) {
        if (!write<T *>(t))
            return false;
        if (info)
            IonSpew(IonSpew_BaselineBailouts,
                    "      WRITE_PTR %p/%p %-15s %p",
                    header_->copyStackBottom, virtualPointerAtStackOffset(0), info, t);
        return true;
    }

    bool writeWord(size_t w, const char *info) {
        if (!write<size_t>(w))
            return false;
        if (info) {
            IonSpew(IonSpew_BaselineBailouts,
                    "      WRITE_WRD %p/%p %-15s %016llx",
                    header_->copyStackBottom, virtualPointerAtStackOffset(0), info, w);
        }
        return true;
    }

    bool writeValue(Value val, const char *info) {
        if (!write<Value>(val))
            return false;
        if (info) {
            IonSpew(IonSpew_BaselineBailouts,
                    "      WRITE_VAL %p/%p %-15s %016llx",
                    header_->copyStackBottom, virtualPointerAtStackOffset(0), info,
                    *((uint64_t *) &val));
        }
        return true;
    }

    Value popValue() {
        JS_ASSERT(bufferUsed_ >= sizeof(Value));
        JS_ASSERT(framePushed_ >= sizeof(Value));
        bufferAvail_ += sizeof(Value);
        bufferUsed_ -= sizeof(Value);
        framePushed_ -= sizeof(Value);
        Value result = *((Value *) header_->copyStackBottom);
        header_->copyStackBottom += sizeof(Value);
        return result;
    }

    void popValueInto(PCMappingEntry::SlotLocation loc) {
        JS_ASSERT(PCMappingEntry::ValidSlotLocation(loc));
        switch(loc) {
          case PCMappingEntry::SlotInR0:
            header_->setR0 = 1;
            header_->valueR0 = popValue();
            break;
          case PCMappingEntry::SlotInR1:
            header_->setR1 = 1;
            header_->valueR1 = popValue();
            break;
          default:
            JS_ASSERT(loc == PCMappingEntry::SlotIgnore);
            popValue();
            break;
        }
    }

    void setResumeFramePtr(void *resumeFramePtr) {
        header_->resumeFramePtr = resumeFramePtr;
    }

    void setResumeAddr(void *resumeAddr) {
        header_->resumeAddr = resumeAddr;
    }

    void setMonitorStub(ICStub *stub) {
        header_->monitorStub = stub;
    }

    inline uint8_t *pointerAtStackOffset(size_t offset) {
        if (offset < bufferUsed_)
            return header_->copyStackBottom + offset;
        return reinterpret_cast<uint8_t *>(frame_) + (offset - bufferUsed_);
    }

    inline Value *valuePointerAtStackOffset(size_t offset) {
        return reinterpret_cast<Value *>(pointerAtStackOffset(offset));
    }

    inline uint8_t *virtualPointerAtStackOffset(size_t offset) {
        if (offset < bufferUsed_)
            return reinterpret_cast<uint8_t *>(frame_) - (bufferUsed_ - offset);
        return reinterpret_cast<uint8_t *>(frame_) + (offset - bufferUsed_);
    }

    inline IonJSFrameLayout *startFrame() {
        return frame_;
    }

    inline IonJSFrameLayout *topFrameAddress() {
        return reinterpret_cast<IonJSFrameLayout *>(pointerAtStackOffset(0));
    }

    //
    // This method should only be called when the builder is in a state where it is
    // starting to construct the stack frame for the next callee.  This means that
    // the lowest value on the constructed stack is the return address for the previous
    // caller frame.
    //
    // This method is used to compute the value of the frame pointer (e.g. ebp on x86)
    // that would have been saved by the baseline jitcode when it was entered.  In some
    // cases, this value can be bogus since we can ensure that the caller would have saved
    // it anyway.
    //
    void *calculatePrevFramePtr() {
        // Get the incoming frame.
        IonJSFrameLayout *topFrame = topFrameAddress();
        FrameType type = topFrame->prevType();

        // For OptimizedJS and Entry frames, the "saved" frame pointer in the baseline
        // frame is meaningless, since Ion saves all registers before calling other ion
        // frames, and the entry frame saves all registers too.
        if (type == IonFrame_OptimizedJS || type == IonFrame_Entry)
            return NULL;

        // BaselineStub - Baseline calling into Ion.
        //  PrevFramePtr needs to point to the BaselineStubFrame's saved frame pointer.
        //      STACK_START_ADDR + IonJSFrameLayout::Size() + PREV_FRAME_SIZE
        //                      - IonBaselineStubFrameLayout::reverseOffsetOfSavedFramePtr()
        if (type == IonFrame_BaselineStub) {
            size_t offset = IonJSFrameLayout::Size() + topFrame->prevFrameLocalSize() +
                            IonBaselineStubFrameLayout::reverseOffsetOfSavedFramePtr();
            return virtualPointerAtStackOffset(offset);
        }

        JS_ASSERT(type == IonFrame_Rectifier);
        // Rectifier - behaviour depends on the frame preceding the rectifier frame, and
        // whether the arch is x86 or not.  The x86 rectifier frame saves the frame pointer,
        // so we can calculate it directly.  For other archs, the previous frame pointer
        // is stored on the stack in the frame that precedes the rectifier frame.
        size_t priorOffset = IonJSFrameLayout::Size() + topFrame->prevFrameLocalSize();
#if defined(JS_CPU_X86)
        // On X86, the FramePointer is pushed as the first value in the Rectifier frame.
        JS_ASSERT(BaselineFrameReg == FramePointer);
        priorOffset -= sizeof(void *);
        return virtualPointerAtStackOffset(priorOffset);
#elif defined(JS_CPU_X64) || defined(JS_CPU_ARM)
        // On X64 and ARM, the frame pointer save location depends on the caller of the
        // the rectifier frame.
        IonRectifierFrameLayout *priorFrame = (IonRectifierFrameLayout *)
                                                    pointerAtStackOffset(priorOffset);
        FrameType priorType = priorFrame->prevType();
        JS_ASSERT(priorType == IonFrame_OptimizedJS || priorType == IonFrame_BaselineStub);

        // If the frame preceding the rectifier is an OptimizedJS frame, then once again
        // the frame pointer does not matter.
        if (priorType == IonFrame_OptimizedJS)
            return NULL;

        // Otherwise, the frame preceding the rectifier is a BaselineStub frame.
        //  let X = STACK_START_ADDR + IonJSFrameLayout::Size() + PREV_FRAME_SIZE
        //      X + IonRectifierFrameLayout::Size()
        //        + ((IonRectifierFrameLayout *) X)->prevFrameLocalSize()
        //        - BaselineStubFrameLayout::reverseOffsetOfSavedFramePtr()
        size_t extraOffset = IonRectifierFrameLayout::Size() + priorFrame->prevFrameLocalSize() +
                             IonBaselineStubFrameLayout::reverseOffsetOfSavedFramePtr();
        return virtualPointerAtStackOffset(priorOffset + extraOffset);
#else
#  error "Bad architecture!"
#endif
    }
};

// For every inline frame, we write out the following data:
//
//                      |      ...      |
//                      +---------------+
//                      |  Descr(???)   |  --- Descr size here is (PREV_FRAME_SIZE)
//                      +---------------+
//                      |  ReturnAddr   |
//             --       +===============+  --- OVERWRITE STARTS HERE  (START_STACK_ADDR)
//             |        | PrevFramePtr  |
//             |    +-> +---------------+
//             |    |   |   Baseline    |
//             |    |   |    Frame      |
//             |    |   +---------------+
//             |    |   |    Fixed0     |
//             |    |   +---------------+
//         +--<     |   |     ...       |
//         |   |    |   +---------------+
//         |   |    |   |    FixedF     |
//         |   |    |   +---------------+
//         |   |    |   |    Stack0     |
//         |   |    |   +---------------+
//         |   |    |   |     ...       |
//         |   |    |   +---------------+
//         |   |    |   |    StackS     |
//         |   --   |   +---------------+  --- IF NOT LAST INLINE FRAME,
//         +------------|  Descr(BLJS)  |  --- CALLING INFO STARTS HERE
//                  |   +---------------+
//                  |   |  ReturnAddr   | <-- return into main jitcode after IC
//             --   |   +===============+
//             |    |   |    StubPtr    |
//             |    |   +---------------+
//             |    +---|   FramePtr    |
//             |        +---------------+
//             |        |     ArgA      |
//             |        +---------------+
//             |        |     ...       |
//         +--<         +---------------+
//         |   |        |     Arg0      |
//         |   |        +---------------+
//         |   |        |     ThisV     |
//         |   --       +---------------+
//         |            |  ActualArgC   |
//         |            +---------------+
//         |            |  CalleeToken  |
//         |            +---------------+
//         +------------| Descr(BLStub) |
//                      +---------------+
//                      |  ReturnAddr   | <-- return into ICCall_Scripted IC
//             --       +===============+ --- IF CALLEE FORMAL ARGS > ActualArgC
//             |        |  UndefinedU   |
//             |        +---------------+
//             |        |     ...       |
//             |        +---------------+
//             |        |  Undefined0   |
//             |        +---------------+
//         +--<         |     ArgA      |
//         |   |        +---------------+
//         |   |        |     ...       |
//         |   |        +---------------+
//         |   |        |     Arg0      |
//         |   |        +---------------+
//         |   |        |     ThisV     |
//         |   --       +---------------+
//         |            |  ActualArgC   |
//         |            +---------------+
//         |            |  CalleeToken  |
//         |            +---------------+
//         +------------|  Descr(Rect)  |
//                      +---------------+
//                      |  ReturnAddr   | <-- return into ArgumentsRectifier after call
//                      +===============+
//
static bool
InitFromBailout(JSContext *cx, HandleFunction fun, HandleScript script, SnapshotIterator &iter,
                bool invalidate, BaselineStackBuilder &builder, MutableHandleFunction nextCallee)
{
    AutoAssertNoGC nogc;
    uint32_t exprStackSlots = iter.slots() - (script->nfixed + CountArgSlots(fun));

    builder.resetFramePushed();

    // Build first baseline frame:
    // +===============+
    // | PrevFramePtr  |
    // +---------------+
    // |   Baseline    |
    // |    Frame      |
    // +---------------+
    // |    Fixed0     |
    // +---------------+
    // |     ...       |
    // +---------------+
    // |    FixedF     |
    // +---------------+
    // |    Stack0     |
    // +---------------+
    // |     ...       |
    // +---------------+
    // |    StackS     |
    // +---------------+  --- IF NOT LAST INLINE FRAME,
    // |  Descr(BLJS)  |  --- CALLING INFO STARTS HERE
    // +---------------+
    // |  ReturnAddr   | <-- return into main jitcode after IC
    // +===============+

    IonSpew(IonSpew_BaselineBailouts, "      Unpacking %s:%d", script->filename, script->lineno);
    IonSpew(IonSpew_BaselineBailouts, "      [BASELINE-JS FRAME]");

    // Calculate and write the previous frame pointer value.
    // Record the virtual stack offset at this location.  Later on, if we end up
    // writing out a BaselineStub frame for the next callee, we'll need to save the
    // address.
    void *prevFramePtr = builder.calculatePrevFramePtr();
    if (!builder.writePtr(prevFramePtr, "PrevFramePtr"))
        return false;
    prevFramePtr = builder.virtualPointerAtStackOffset(0);

    // Write struct BaselineFrame.
    if (!builder.subtract(BaselineFrame::Size(), "BaselineFrame"))
        return false;
    BaselineFrame *blFrame = reinterpret_cast<BaselineFrame *>(builder.pointerAtStackOffset(0));

    // Initialize BaselineFrame::frameSize
    size_t frameSize = BaselineFrame::Size() + BaselineFrame::FramePointerOffset +
                       (sizeof(Value) * (script->nfixed + exprStackSlots));
    IonSpew(IonSpew_BaselineBailouts, "      FrameSize=%d", (int) frameSize);
    blFrame->setFrameSize(frameSize);

    // Initialize BaselineFrame::scopeChain
    JSObject *scopeChain = NULL;
    if (iter.bailoutKind() == Bailout_ArgumentCheck) {
        // Temporary hack -- skip the (unused) scopeChain, because it could be
        // bogus (we can fail before the scope chain slot is set). Strip the
        // hasScopeChain flag and we'll check this later to run prologue().
        IonSpew(IonSpew_BaselineBailouts, "      Bailout_ArgumentCheck! (no valid scopeChain)");
        iter.skip();
    } else {
        Value v = iter.read();
        if (v.isObject())
            scopeChain = &v.toObject();
        else
            JS_ASSERT(v.isUndefined());
        // TODO: check for heavyweight function? (fun->isHeavyweight())

        // Get scope chain from function or script if not already set.
        if (!scopeChain) {
            if (fun)
                scopeChain = fun->environment();
            else
                scopeChain = &(script->global());
        }
    }
    IonSpew(IonSpew_BaselineBailouts, "      ScopeChain=%p", scopeChain);
    blFrame->setScopeChain(scopeChain);
    // Do not need to initialize scratchValue or returnValue fields in BaselineFrame.

    // No flags are set.
    blFrame->setFlags(0);

    // Ion doesn't compile code with try/catch, so the block object will always be
    // null.
    blFrame->setBlockChainNull();

    if (fun) {
        // The unpacked thisv and arguments should overwrite the pushed args present
        // in the calling frame.
        Value thisv = iter.read();
        IonSpew(IonSpew_BaselineBailouts, "      Is function!");
        IonSpew(IonSpew_BaselineBailouts, "      thisv=%016llx", *((uint64_t *) &thisv));

        size_t thisvOffset = builder.framePushed() + IonJSFrameLayout::offsetOfThis();
        *builder.valuePointerAtStackOffset(thisvOffset) = thisv;

        JS_ASSERT(iter.slots() >= CountArgSlots(fun));
        IonSpew(IonSpew_BaselineBailouts, "      frame slots %u, nargs %u, nfixed %u",
                iter.slots(), fun->nargs, script->nfixed);

        for (uint32_t i = 0; i < fun->nargs; i++) {
            Value arg = iter.read();
            IonSpew(IonSpew_BaselineBailouts, "      arg %d = %016llx",
                        (int) i, *((uint64_t *) &arg));
            size_t argOffset = builder.framePushed() + IonJSFrameLayout::offsetOfActualArg(i);
            *builder.valuePointerAtStackOffset(argOffset) = arg;
        }
    }

    for (uint32_t i = 0; i < script->nfixed; i++) {
        Value slot = iter.read();
        if (!builder.writeValue(slot, "FixedValue"))
            return false;
    }

    IonSpew(IonSpew_BaselineBailouts, "      pushing %d expression stack slots",
                                      (int) exprStackSlots);
    for (uint32_t i = 0; i < exprStackSlots; i++) {
        Value v;

        // If coming from an invalidation bailout, and this is the topmost
        // value, and a value override has been specified, don't read from the
        // iterator. Otherwise, we risk using a garbage value.
        if (!iter.moreFrames() && i == exprStackSlots - 1 && cx->runtime->hasIonReturnOverride()) {
            JS_ASSERT(invalidate);
            iter.skip();
            IonSpew(IonSpew_BaselineBailouts, "      [Return Override]");
            v = cx->runtime->takeIonReturnOverride();
        } else {
            v = iter.read();
        }
        if (!builder.writeValue(v, "StackValue"))
            return false;
    }

    size_t endOfBaselineJSFrameStack = builder.framePushed();

    // Get the PC
    uint32_t pcOff = iter.pcOffset();
    jsbytecode *pc = script->code + pcOff;
    JSOp op = JSOp(*pc);
    bool resumeAfter = iter.resumeAfter();
    BaselineScript *baselineScript = script->baselineScript();

    JS_ASSERT(js_ReconstructStackDepth(cx, script, resumeAfter ? GetNextPc(pc) : pc)
                == exprStackSlots);

    IonSpew(IonSpew_BaselineBailouts, "      Resuming %s pc offset %d of %s:%d",
                resumeAfter ? "after" : "at", (int) pcOff, script->filename, (int) script->lineno);

    // If this was the last inline frame, then unpacking is almost done.
    if (!iter.moreFrames()) {

        // If the bailout was a resumeAfter, and the opcode is
        // monitored, then the bailed out state should be in a position to enter
        // into the ICTypeMonitor chain for the op.
        bool enterMonitorChain = resumeAfter ? !!(js_CodeSpec[op].format & JOF_TYPESET) : false;
        bool isCall = js_CodeSpec[*pc].format & JOF_INVOKE;
        uint32_t numCallArgs = isCall ? GET_ARGC(pc) : 0;

        if (resumeAfter && !enterMonitorChain)
            pc = GetNextPc(pc);

        builder.setResumeFramePtr(prevFramePtr);

        if (enterMonitorChain) {
            ICEntry &icEntry = baselineScript->icEntryFromPCOffset(pcOff);
            ICFallbackStub *fallbackStub = icEntry.firstStub()->getChainFallback();
            JS_ASSERT(fallbackStub->isMonitoredFallback());
            IonSpew(IonSpew_BaselineBailouts, "      [TYPE-MONITOR CHAIN]");
            ICMonitoredFallbackStub *monFallbackStub = fallbackStub->toMonitoredFallbackStub();
            ICStub *firstMonStub = monFallbackStub->fallbackMonitorStub()->firstMonitorStub();

            // To enter a monitoring chain, we load the top stack value into R0
            IonSpew(IonSpew_BaselineBailouts, "      Popping top stack value into R0.");
            builder.popValueInto(PCMappingEntry::SlotInR0);

            // Need to adjust the frameSize for the frame to match the values popped
            // into registers.
            frameSize -= sizeof(Value);
            blFrame->setFrameSize(frameSize);
            IonSpew(IonSpew_BaselineBailouts, "      Adjusted framesize -= %d: %d",
                            (int) sizeof(Value), (int) frameSize);

            // If resuming into a JSOP_CALL, baseline keeps the arguments on the
            // stack and pops them only after returning from the call IC.
            // Push undefs onto the stack in anticipatin of the callee, thisv, and
            // actual arguments passed to the caller.
            if (isCall) {
                builder.writeValue(UndefinedValue(), "CallOp FillerCallee");
                builder.writeValue(UndefinedValue(), "CallOp FillerThis");
                for (uint32_t i = 0; i < numCallArgs; i++)
                    builder.writeValue(UndefinedValue(), "CallOp FillerArg");

                frameSize += (numCallArgs + 2) * sizeof(Value);
                blFrame->setFrameSize(frameSize);
                IonSpew(IonSpew_BaselineBailouts, "      Adjusted framesize += %d: %d",
                                (int) ((numCallArgs + 2) * sizeof(Value)), (int) frameSize);
            }

            // Set the resume address to the return point from the IC, and set
            // the monitor stub addr.
            builder.setResumeAddr(baselineScript->returnAddressForIC(icEntry));
            builder.setMonitorStub(firstMonStub);
            IonSpew(IonSpew_BaselineBailouts, "      Set resumeAddr=%p monitorStub=%p",
                    baselineScript->returnAddressForIC(icEntry), firstMonStub);

        } else {
            // If needed, initialize BaselineBailoutInfo's valueR0 and/or valueR1 with the
            // top stack values.
            uint8_t slotInfo = baselineScript->slotInfoForPC(script, pc);
            unsigned numUnsynced = PCMappingEntry::SlotInfoNumUnsynced(slotInfo);
            JS_ASSERT(numUnsynced <= 2);
            PCMappingEntry::SlotLocation loc1, loc2;
            if (numUnsynced > 0) {
                loc1 = PCMappingEntry::SlotInfoTopSlotLocation(slotInfo);
                IonSpew(IonSpew_BaselineBailouts, "      Popping top stack value into %d.",
                            (int) loc1);
                builder.popValueInto(loc1);
            }
            if (numUnsynced > 1) {
                loc2 = PCMappingEntry::SlotInfoNextSlotLocation(slotInfo);
                IonSpew(IonSpew_BaselineBailouts, "      Popping next stack value into %d.",
                            (int) loc2);
                JS_ASSERT(loc1 != loc2);
                builder.popValueInto(loc2);
            }

            // Need to adjust the frameSize for the frame to match the values popped
            // into registers.
            frameSize -= sizeof(Value) * numUnsynced;
            blFrame->setFrameSize(frameSize);
            IonSpew(IonSpew_BaselineBailouts, "      Adjusted framesize -= %d: %d",
                            int(sizeof(Value) * numUnsynced), int(frameSize));

            // If scopeChain is NULL, then bailout is occurring during argument check.
            // In this case, resume into the prologue.
            uint8_t *opReturnAddr;
            if (scopeChain == NULL) {
                JS_ASSERT(numUnsynced == 0);
                opReturnAddr = baselineScript->prologueEntryAddr();
                IonSpew(IonSpew_BaselineBailouts, "      Resuming into prologue.");
            } else {
                opReturnAddr = baselineScript->nativeCodeForPC(script, pc);
            }
            builder.setResumeAddr(opReturnAddr);
            IonSpew(IonSpew_BaselineBailouts, "      Set resumeAddr=%p", opReturnAddr);
        }

        return true;
    }

    // Write out descriptor of BaselineJS frame.
    size_t baselineFrameDescr = MakeFrameDescriptor((uint32_t) builder.framePushed(),
                                                    IonFrame_BaselineJS);
    if (!builder.writeWord(baselineFrameDescr, "Descriptor"))
        return false;

    // Calculate and write out return address.
    // The icEntry in question MUST have a ICCall_Fallback as its fallback stub.
    ICEntry &icEntry = baselineScript->icEntryFromPCOffset(pcOff);
    ICFallbackStub *fallbackStub = icEntry.firstStub()->getChainFallback();
    JS_ASSERT(fallbackStub->isCall_Fallback());
    if (!builder.writePtr(baselineScript->returnAddressForIC(icEntry), "ReturnAddr"))
        return false;

    // Build baseline stub frame:
    // +===============+
    // |    StubPtr    |
    // +---------------+
    // |   FramePtr    |
    // +---------------+
    // |     ArgA      |
    // +---------------+
    // |     ...       |
    // +---------------+
    // |     Arg0      |
    // +---------------+
    // |     ThisV     |
    // +---------------+
    // |  ActualArgC   |
    // +---------------+
    // |  CalleeToken  |
    // +---------------+
    // | Descr(BLStub) |
    // +---------------+
    // |  ReturnAddr   |
    // +===============+

    IonSpew(IonSpew_BaselineBailouts, "      [BASELINE-STUB FRAME]");

    size_t startOfBaselineStubFrame = builder.framePushed();

    // Write stub pointer.
    if (!builder.writePtr(icEntry.firstStub(), "StubPtr"))
        return false;

    // Write previous frame pointer (saved earlier).
    if (!builder.writePtr(prevFramePtr, "PrevFramePtr"))
        return false;
    prevFramePtr = builder.virtualPointerAtStackOffset(0);

    // Write out actual arguments (and thisv), copied from unpacked stack of BaselineJS frame.
    // Arguments are reversed on the BaselineJS frame's stack values.
    JS_ASSERT(op == JSOP_CALL || op == JSOP_NEW);
    unsigned actualArgc = GET_ARGC(pc);
    JS_ASSERT(actualArgc + 2 <= exprStackSlots);
    for (unsigned i = 0; i < actualArgc + 1; i++) {
        size_t argSlot = ((script->nfixed + exprStackSlots) - (actualArgc + 1) - 1) + i;
        if (!builder.writeValue(*blFrame->valueSlot(argSlot), "ArgVal"))
            return false;
    }
    // In case these arguments need to be copied on the stack again for a rectifier frame,
    // save the framePushed values here for later use.
    size_t endOfBaselineStubArgs = builder.framePushed();

    // Calculate frame size for descriptor.
    size_t baselineStubFrameSize = builder.framePushed() - startOfBaselineStubFrame;
    size_t baselineStubFrameDescr = MakeFrameDescriptor((uint32_t) baselineStubFrameSize,
                                                        IonFrame_BaselineStub);

    // Push actual argc
    if (!builder.writeWord(actualArgc, "ActualArgc"))
        return false;

    // Push callee token (must be a JS Function)
    uint32_t calleeStackSlot = exprStackSlots - uint32_t(actualArgc + 2);
    size_t calleeOffset = (builder.framePushed() - endOfBaselineJSFrameStack)
                            + ((exprStackSlots - (calleeStackSlot + 1)) * sizeof(Value));
    Value callee = *builder.valuePointerAtStackOffset(calleeOffset);
    IonSpew(IonSpew_BaselineBailouts, "      CalleeStackSlot=%d", (int) calleeStackSlot);
    IonSpew(IonSpew_BaselineBailouts, "      Callee = %016llx", *((uint64_t *) &callee));
    JS_ASSERT(callee.isObject() && callee.toObject().isFunction());
    JSFunction *calleeFun = callee.toObject().toFunction();
    if (!builder.writePtr(CalleeToToken(calleeFun), "CalleeToken"))
        return false;
    nextCallee.set(calleeFun);

    // Push BaselineStub frame descriptor
    if (!builder.writeWord(baselineStubFrameDescr, "Descriptor"))
        return false;

    // Push return address into ICCall_Scripted stub, immediately after the call.
    void *baselineCallReturnAddr = cx->compartment->ionCompartment()->baselineCallReturnAddr();
    if (!builder.writePtr(baselineCallReturnAddr, "ReturnAddr"))
        return false;

    // If actualArgc >= fun->nargs, then we are done.  Otherwise, we need to push on
    // a reconstructed rectifier frame.
    if (actualArgc >= calleeFun->nargs)
        return true;

    // Push a reconstructed rectifier frame.
    // +===============+
    // |  UndefinedU   |
    // +---------------+
    // |     ...       |
    // +---------------+
    // |  Undefined0   |
    // +---------------+
    // |     ArgA      |
    // +---------------+
    // |     ...       |
    // +---------------+
    // |     Arg0      |
    // +---------------+
    // |     ThisV     |
    // +---------------+
    // |  ActualArgC   |
    // +---------------+
    // |  CalleeToken  |
    // +---------------+
    // |  Descr(Rect)  |
    // +---------------+
    // |  ReturnAddr   |
    // +===============+

    IonSpew(IonSpew_BaselineBailouts, "      [RECTIFIER FRAME]");

    size_t startOfRectifierFrame = builder.framePushed();

    // On x86-only, the frame pointer is saved again in the rectifier frame.
#if defined(JS_CPU_X86)
    if (!builder.writePtr(prevFramePtr, "PrevFramePtr-X86Only"))
        return false;
#endif

    // Push undefined for missing arguments.
    for (unsigned i = 0; i < (calleeFun->nargs - actualArgc); i++) {
        if (!builder.writeValue(UndefinedValue(), "FillerVal"))
            return false;
    }

    // Copy arguments + thisv from BaselineStub frame.
    if (!builder.subtract((actualArgc + 1) * sizeof(Value), "CopiedArgs"))
        return false;
    uint8_t *stubArgsEnd = builder.pointerAtStackOffset(builder.framePushed() - endOfBaselineStubArgs);
    IonSpew(IonSpew_BaselineBailouts, "      MemCpy from %p", stubArgsEnd);
    memcpy(builder.pointerAtStackOffset(0), stubArgsEnd, (actualArgc + 1) * sizeof(Value));

    // Calculate frame size for descriptor.
    size_t rectifierFrameSize = builder.framePushed() - startOfRectifierFrame;
    size_t rectifierFrameDescr = MakeFrameDescriptor((uint32_t) rectifierFrameSize,
                                                     IonFrame_Rectifier);

    // Push actualArgc
    if (!builder.writeWord(actualArgc, "ActualArgc"))
        return false;

    // Push calleeToken again.
    if (!builder.writePtr(CalleeToToken(calleeFun), "CalleeToken"))
        return false;

    // Push rectifier frame descriptor
    if (!builder.writeWord(rectifierFrameDescr, "Descriptor"))
        return false;

    // Push return address into the ArgumentsRectifier code, immediately after the ioncode
    // call.
    void *rectReturnAddr = cx->compartment->ionCompartment()->getArgumentsRectifierReturnAddr();
    if (!builder.writePtr(rectReturnAddr, "ReturnAddr"))
        return false;

    return true;
}

uint32_t
ion::BailoutIonToBaseline(JSContext *cx, IonActivation *activation, IonBailoutIterator &iter,
                          bool invalidate, BaselineBailoutInfo **bailoutInfo)
{
    JS_ASSERT(bailoutInfo != NULL);
    JS_ASSERT(*bailoutInfo == NULL);

    // The caller of the top frame must be one of the following:
    //      OptimizedJS - Ion calling into Ion.
    //      BaselineStub - Baseline calling into Ion.
    //      Entry - Interpreter or other calling into Ion.
    //      Rectifier - Arguments rectifier calling into Ion.
    JS_ASSERT(iter.isOptimizedJS());
    FrameType prevFrameType = iter.prevType();
    JS_ASSERT(prevFrameType == IonFrame_OptimizedJS ||
              prevFrameType == IonFrame_BaselineStub ||
              prevFrameType == IonFrame_Entry ||
              prevFrameType == IonFrame_Rectifier);

    // All incoming frames are going to look like this:
    //
    //      +---------------+
    //      |     ...       |
    //      +---------------+
    //      |     Args      |
    //      |     ...       |
    //      +---------------+
    //      |    ThisV      |
    //      +---------------+
    //      |  ActualArgC   |
    //      +---------------+
    //      |  CalleeToken  |
    //      +---------------+
    //      |  Descriptor   |
    //      +---------------+
    //      |  ReturnAddr   |
    //      +---------------+
    //      |    |||||      | <---- Overwrite starting here.
    //      |    |||||      |
    //      |    |||||      |
    //      +---------------+

    IonSpew(IonSpew_BaselineBailouts, "Bailing to baseline %s:%u (IonScript=%p) (FrameType=%d)",
            iter.script()->filename, iter.script()->lineno, (void *) iter.ionScript(),
            (int) prevFrameType);
    IonSpew(IonSpew_BaselineBailouts, "  Reading from snapshot offset %u size %u",
            iter.snapshotOffset(), iter.ionScript()->snapshotsSize());
    iter.ionScript()->setBailoutExpected();

    // Allocate buffer to hold stack replacement data.
    BaselineStackBuilder builder(iter, 1024);
    if (!builder.init())
        return BAILOUT_RETURN_FATAL_ERROR;
    IonSpew(IonSpew_BaselineBailouts, "  Incoming frame ptr = %p", builder.startFrame());

    SnapshotIterator snapIter(iter);

    RootedFunction callee(cx, iter.maybeCallee());
    if (callee) {
        IonSpew(IonSpew_BaselineBailouts, "  Callee function (%s:%u)",
                callee->nonLazyScript()->filename, callee->nonLazyScript()->lineno);
    } else {
        IonSpew(IonSpew_BaselineBailouts, "  No callee!");
    }

    if (iter.isConstructing())
        IonSpew(IonSpew_BaselineBailouts, "  Constructing!");
    else
        IonSpew(IonSpew_BaselineBailouts, "  Not constructing!");

    IonSpew(IonSpew_BaselineBailouts, "  Restoring frames:");
    int frameNo = 0;

    // Reconstruct baseline frames using the builder.
    RootedFunction fun(cx, callee);
    RootedScript scr(cx, iter.script());
    while (true) {
        IonSpew(IonSpew_BaselineBailouts, "    FrameNo %d", frameNo);
        RootedFunction nextCallee(cx, NULL);
        if (!InitFromBailout(cx, fun, scr, snapIter, invalidate, builder, &nextCallee))
            return BAILOUT_RETURN_FATAL_ERROR;

        if (!snapIter.moreFrames())
             break;

        JS_ASSERT(nextCallee);
        fun = nextCallee;
        scr = fun->nonLazyScript();
        snapIter.nextFrame();

        frameNo++;
    }
    IonSpew(IonSpew_BaselineBailouts, "  Done restoring frames");

    // Take the reconstructed baseline stack so it doesn't get freed when builder destructs.
    BaselineBailoutInfo *info = builder.takeBuffer();

    // Do stack check.
    bool overRecursed = false;
    JS_CHECK_RECURSION_WITH_EXTRA(cx, info->copyStackTop - info->copyStackBottom,
                                  overRecursed = true);
    if (overRecursed)
        return BAILOUT_RETURN_OVERRECURSED;

    *bailoutInfo = info;
    return BAILOUT_RETURN_BASELINE;
}
