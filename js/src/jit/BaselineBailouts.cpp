/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/CompileInfo.h"
#include "jit/IonSpewer.h"
#include "vm/ArgumentsObject.h"

#include "jsscriptinlines.h"

#include "jit/IonFrames-inl.h"

using namespace js;
using namespace js::jit;

// BaselineStackBuilder may reallocate its buffer if the current one is too
// small. To avoid dangling pointers, BufferPointer represents a pointer into
// this buffer as a pointer to the header and a fixed offset.
template <typename T>
class BufferPointer
{
    BaselineBailoutInfo **header_;
    size_t offset_;
    bool heap_;

  public:
    BufferPointer(BaselineBailoutInfo **header, size_t offset, bool heap)
      : header_(header), offset_(offset), heap_(heap)
    { }

    T *get() const {
        BaselineBailoutInfo *header = *header_;
        if (!heap_)
            return (T*)(header->incomingStack + offset_);

        uint8_t *p = header->copyStackTop - offset_;
        JS_ASSERT(p >= header->copyStackBottom && p < header->copyStackTop);
        return (T*)p;
    }

    T &operator*() const { return *get(); }
    T *operator->() const { return get(); }
};

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
        buffer_(nullptr),
        header_(nullptr),
        framePushed_(0)
    {
        JS_ASSERT(bufferTotal_ >= HeaderSize());
    }

    ~BaselineStackBuilder() {
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
        header_->incomingStack = reinterpret_cast<uint8_t *>(frame_);
        header_->copyStackTop = buffer_ + bufferTotal_;
        header_->copyStackBottom = header_->copyStackTop;
        header_->setR0 = 0;
        header_->valueR0 = UndefinedValue();
        header_->setR1 = 0;
        header_->valueR1 = UndefinedValue();
        header_->resumeFramePtr = nullptr;
        header_->resumeAddr = nullptr;
        header_->monitorStub = nullptr;
        header_->numFrames = 0;
        return true;
    }

    bool enlarge() {
        JS_ASSERT(buffer_ != nullptr);
        if (bufferTotal_ & mozilla::tl::MulOverflowMask<2>::value)
            return false;
        size_t newSize = bufferTotal_ * 2;
        uint8_t *newBuffer = reinterpret_cast<uint8_t *>(js_calloc(newSize));
        if (!newBuffer)
            return false;
        memcpy((newBuffer + newSize) - bufferUsed_, header_->copyStackBottom, bufferUsed_);
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
        buffer_ = nullptr;
        return header_;
    }

    void resetFramePushed() {
        framePushed_ = 0;
    }

    size_t framePushed() const {
        return framePushed_;
    }

    bool subtract(size_t size, const char *info = nullptr) {
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
            if (sizeof(size_t) == 4) {
                IonSpew(IonSpew_BaselineBailouts,
                        "      WRITE_WRD %p/%p %-15s %08x",
                        header_->copyStackBottom, virtualPointerAtStackOffset(0), info, w);
            } else {
                IonSpew(IonSpew_BaselineBailouts,
                        "      WRITE_WRD %p/%p %-15s %016llx",
                        header_->copyStackBottom, virtualPointerAtStackOffset(0), info, w);
            }
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

    void popValueInto(PCMappingSlotInfo::SlotLocation loc) {
        JS_ASSERT(PCMappingSlotInfo::ValidSlotLocation(loc));
        switch(loc) {
          case PCMappingSlotInfo::SlotInR0:
            header_->setR0 = 1;
            header_->valueR0 = popValue();
            break;
          case PCMappingSlotInfo::SlotInR1:
            header_->setR1 = 1;
            header_->valueR1 = popValue();
            break;
          default:
            JS_ASSERT(loc == PCMappingSlotInfo::SlotIgnore);
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

    template <typename T>
    BufferPointer<T> pointerAtStackOffset(size_t offset) {
        if (offset < bufferUsed_) {
            // Calculate offset from copyStackTop.
            offset = header_->copyStackTop - (header_->copyStackBottom + offset);
            return BufferPointer<T>(&header_, offset, /* heap = */ true);
        }

        return BufferPointer<T>(&header_, offset - bufferUsed_, /* heap = */ false);
    }

    BufferPointer<Value> valuePointerAtStackOffset(size_t offset) {
        return pointerAtStackOffset<Value>(offset);
    }

    inline uint8_t *virtualPointerAtStackOffset(size_t offset) {
        if (offset < bufferUsed_)
            return reinterpret_cast<uint8_t *>(frame_) - (bufferUsed_ - offset);
        return reinterpret_cast<uint8_t *>(frame_) + (offset - bufferUsed_);
    }

    inline IonJSFrameLayout *startFrame() {
        return frame_;
    }

    BufferPointer<IonJSFrameLayout> topFrameAddress() {
        return pointerAtStackOffset<IonJSFrameLayout>(0);
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
        BufferPointer<IonJSFrameLayout> topFrame = topFrameAddress();
        FrameType type = topFrame->prevType();

        // For OptimizedJS and Entry frames, the "saved" frame pointer in the baseline
        // frame is meaningless, since Ion saves all registers before calling other ion
        // frames, and the entry frame saves all registers too.
        if (type == IonFrame_OptimizedJS || type == IonFrame_Entry)
            return nullptr;

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
        BufferPointer<IonRectifierFrameLayout> priorFrame =
            pointerAtStackOffset<IonRectifierFrameLayout>(priorOffset);
        FrameType priorType = priorFrame->prevType();
        JS_ASSERT(priorType == IonFrame_OptimizedJS || priorType == IonFrame_BaselineStub);

        // If the frame preceding the rectifier is an OptimizedJS frame, then once again
        // the frame pointer does not matter.
        if (priorType == IonFrame_OptimizedJS)
            return nullptr;

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

static inline bool
IsInlinableFallback(ICFallbackStub *icEntry)
{
    return icEntry->isCall_Fallback() || icEntry->isGetProp_Fallback() ||
           icEntry->isSetProp_Fallback();
}

static inline void*
GetStubReturnAddress(JSContext *cx, jsbytecode *pc)
{
    if (IsGetPropPC(pc))
        return cx->compartment()->jitCompartment()->baselineGetPropReturnAddr();
    if (IsSetPropPC(pc))
        return cx->compartment()->jitCompartment()->baselineSetPropReturnAddr();
    // This should be a call op of some kind, now.
    JS_ASSERT(IsCallPC(pc));
    return cx->compartment()->jitCompartment()->baselineCallReturnAddr();
}

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
InitFromBailout(JSContext *cx, HandleScript caller, jsbytecode *callerPC,
                HandleFunction fun, HandleScript script, IonScript *ionScript,
                SnapshotIterator &iter, bool invalidate, BaselineStackBuilder &builder,
                AutoValueVector &startFrameFormals, MutableHandleFunction nextCallee,
                jsbytecode **callPC, const ExceptionBailoutInfo *excInfo)
{
    // If excInfo is non-nullptr, we are bailing out to a catch or finally block
    // and this is the frame where we will resume. Usually the expression stack
    // should be empty in this case but there can be iterators on the stack.
    uint32_t exprStackSlots;
    if (excInfo)
        exprStackSlots = excInfo->numExprSlots;
    else
        exprStackSlots = iter.slots() - (script->nfixed() + CountArgSlots(script, fun));

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

    IonSpew(IonSpew_BaselineBailouts, "      Unpacking %s:%d", script->filename(), script->lineno());
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
    BufferPointer<BaselineFrame> blFrame = builder.pointerAtStackOffset<BaselineFrame>(0);

    // Initialize BaselineFrame::frameSize
    uint32_t frameSize = BaselineFrame::Size() + BaselineFrame::FramePointerOffset +
                         (sizeof(Value) * (script->nfixed() + exprStackSlots));
    IonSpew(IonSpew_BaselineBailouts, "      FrameSize=%d", (int) frameSize);
    blFrame->setFrameSize(frameSize);

    uint32_t flags = 0;

    // If SPS Profiler is enabled, mark the frame as having pushed an SPS entry.
    // This may be wrong for the last frame of ArgumentCheck bailout, but
    // that will be fixed later.
    if (cx->runtime()->spsProfiler.enabled() && ionScript->hasSPSInstrumentation()) {
        IonSpew(IonSpew_BaselineBailouts, "      Setting SPS flag on frame!");
        flags |= BaselineFrame::HAS_PUSHED_SPS_FRAME;
    }

    // Initialize BaselineFrame's scopeChain and argsObj
    JSObject *scopeChain = nullptr;
    Value returnValue;
    ArgumentsObject *argsObj = nullptr;
    BailoutKind bailoutKind = iter.bailoutKind();
    if (bailoutKind == Bailout_ArgumentCheck) {
        // Temporary hack -- skip the (unused) scopeChain, because it could be
        // bogus (we can fail before the scope chain slot is set). Strip the
        // hasScopeChain flag and this will be fixed up later in |FinishBailoutToBaseline|,
        // which calls |EnsureHasScopeObjects|.
        IonSpew(IonSpew_BaselineBailouts, "      Bailout_ArgumentCheck! (no valid scopeChain)");
        iter.skip();

        // skip |return value|
        iter.skip();
        returnValue = UndefinedValue();

        // Scripts with |argumentsHasVarBinding| have an extra slot.
        if (script->argumentsHasVarBinding()) {
            IonSpew(IonSpew_BaselineBailouts,
                    "      Bailout_ArgumentCheck for script with argumentsHasVarBinding!"
                    "Using empty arguments object");
            iter.skip();
        }
    } else {
        Value v = iter.read();
        if (v.isObject()) {
            scopeChain = &v.toObject();
            if (fun && fun->isHeavyweight())
                flags |= BaselineFrame::HAS_CALL_OBJ;
        } else {
            JS_ASSERT(v.isUndefined());

            // Get scope chain from function or script.
            if (fun) {
                // If pcOffset == 0, we may have to push a new call object, so
                // we leave scopeChain nullptr and enter baseline code before
                // the prologue.
                if (iter.pcOffset() != 0 || iter.resumeAfter())
                    scopeChain = fun->environment();
            } else {
                // For global, compile-and-go scripts the scope chain is the
                // script's global (Ion does not compile non-compile-and-go
                // scripts). Also note that it's invalid to resume into the
                // prologue in this case because the prologue expects the scope
                // chain in R1 for eval and global scripts.
                JS_ASSERT(!script->isForEval());
                JS_ASSERT(script->compileAndGo());
                scopeChain = &(script->global());
            }
        }

        // Make sure to add HAS_RVAL to flags here because setFlags() below
        // will clobber it.
        returnValue = iter.read();
        flags |= BaselineFrame::HAS_RVAL;

        // If script maybe has an arguments object, the third slot will hold it.
        if (script->argumentsHasVarBinding()) {
            v = iter.read();
            JS_ASSERT(v.isObject() || v.isUndefined());
            if (v.isObject())
                argsObj = &v.toObject().as<ArgumentsObject>();
        }
    }
    IonSpew(IonSpew_BaselineBailouts, "      ScopeChain=%p", scopeChain);
    blFrame->setScopeChain(scopeChain);
    IonSpew(IonSpew_BaselineBailouts, "      ReturnValue=%016llx", *((uint64_t *) &returnValue));
    blFrame->setReturnValue(returnValue);

    // Do not need to initialize scratchValue field in BaselineFrame.
    blFrame->setFlags(flags);

    // initArgsObjUnchecked modifies the frame's flags, so call it after setFlags.
    if (argsObj)
        blFrame->initArgsObjUnchecked(*argsObj);

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

        JS_ASSERT(iter.slots() >= CountArgSlots(script, fun));
        IonSpew(IonSpew_BaselineBailouts, "      frame slots %u, nargs %u, nfixed %u",
                iter.slots(), fun->nargs, script->nfixed());

        if (!callerPC) {
            // This is the first frame. Store the formals in a Vector until we
            // are done. Due to UCE and phi elimination, we could store an
            // UndefinedValue() here for formals we think are unused, but
            // locals may still reference the original argument slot
            // (MParameter/LArgument) and expect the original Value.
            JS_ASSERT(startFrameFormals.empty());
            if (!startFrameFormals.resize(fun->nargs))
                return false;
        }

        for (uint32_t i = 0; i < fun->nargs; i++) {
            Value arg = iter.read();
            IonSpew(IonSpew_BaselineBailouts, "      arg %d = %016llx",
                        (int) i, *((uint64_t *) &arg));
            if (callerPC) {
                size_t argOffset = builder.framePushed() + IonJSFrameLayout::offsetOfActualArg(i);
                *builder.valuePointerAtStackOffset(argOffset) = arg;
            } else {
                startFrameFormals[i] = arg;
            }
        }
    }

    for (uint32_t i = 0; i < script->nfixed(); i++) {
        Value slot = iter.read();
        if (!builder.writeValue(slot, "FixedValue"))
            return false;
    }

    // Get the pc. If we are handling an exception, resume at the pc of the
    // catch or finally block.
    jsbytecode *pc = excInfo ? excInfo->resumePC : script->offsetToPC(iter.pcOffset());
    bool resumeAfter = excInfo ? false : iter.resumeAfter();

    JSOp op = JSOp(*pc);

    // Fixup inlined JSOP_FUNCALL, JSOP_FUNAPPLY, and accessors on the caller side.
    // On the caller side this must represent like the function wasn't inlined.
    uint32_t pushedSlots = 0;
    AutoValueVector savedCallerArgs(cx);
    bool needToSaveArgs = op == JSOP_FUNAPPLY || IsGetPropPC(pc) || IsSetPropPC(pc);
    if (iter.moreFrames() && (op == JSOP_FUNCALL || needToSaveArgs))
    {
        uint32_t inlined_args = 0;
        if (op == JSOP_FUNCALL)
            inlined_args = 2 + GET_ARGC(pc) - 1;
        else if (op == JSOP_FUNAPPLY)
            inlined_args = 2 + blFrame->numActualArgs();
        else
            inlined_args = 2 + IsSetPropPC(pc);

        JS_ASSERT(exprStackSlots >= inlined_args);
        pushedSlots = exprStackSlots - inlined_args;

        IonSpew(IonSpew_BaselineBailouts,
                "      pushing %u expression stack slots before fixup",
                pushedSlots);
        for (uint32_t i = 0; i < pushedSlots; i++) {
            Value v = iter.read();
            if (!builder.writeValue(v, "StackValue"))
                return false;
        }

        if (op == JSOP_FUNCALL) {
            // When funcall got inlined and the native js_fun_call was bypassed,
            // the stack state is incorrect. To restore correctly it must look like
            // js_fun_call was actually called. This means transforming the stack
            // from |target, this, args| to |js_fun_call, target, this, args|
            // The js_fun_call is never read, so just pushing undefined now.
            IonSpew(IonSpew_BaselineBailouts, "      pushing undefined to fixup funcall");
            if (!builder.writeValue(UndefinedValue(), "StackValue"))
                return false;
        }

        if (needToSaveArgs) {
            // When an accessor is inlined, the whole thing is a lie. There
            // should never have been a call there. Fix the caller's stack to
            // forget it ever happened.

            // When funapply gets inlined we take all arguments out of the
            // arguments array. So the stack state is incorrect. To restore
            // correctly it must look like js_fun_apply was actually called.
            // This means transforming the stack from |target, this, arg1, ...|
            // to |js_fun_apply, target, this, argObject|.
            // Since the information is never read, we can just push undefined
            // for all values.
            if (op == JSOP_FUNAPPLY) {
                IonSpew(IonSpew_BaselineBailouts, "      pushing 4x undefined to fixup funapply");
                if (!builder.writeValue(UndefinedValue(), "StackValue"))
                    return false;
                if (!builder.writeValue(UndefinedValue(), "StackValue"))
                    return false;
                if (!builder.writeValue(UndefinedValue(), "StackValue"))
                    return false;
                if (!builder.writeValue(UndefinedValue(), "StackValue"))
                    return false;
            }
            // Save the actual arguments. They are needed on the callee side
            // as the arguments. Else we can't recover them.
            if (!savedCallerArgs.resize(inlined_args))
                return false;
            for (uint32_t i = 0; i < inlined_args; i++)
                savedCallerArgs[i] = iter.read();

            if (IsSetPropPC(pc)) {
                // We would love to just save all the arguments and leave them
                // in the stub frame pushed below, but we will lose the inital
                // argument which the function was called with, which we must
                // return to the caller, even if the setter internally modifies
                // its arguments. Stash the initial argument on the stack, to be
                // later retrieved by the SetProp_Fallback stub.
                Value initialArg = savedCallerArgs[inlined_args - 1];
                IonSpew(IonSpew_BaselineBailouts, "     pushing setter's initial argument");
                if (!builder.writeValue(initialArg, "StackValue"))
                    return false;
            }
            pushedSlots = exprStackSlots;
        }
    }

    IonSpew(IonSpew_BaselineBailouts, "      pushing %u expression stack slots",
                                      exprStackSlots - pushedSlots);
    for (uint32_t i = pushedSlots; i < exprStackSlots; i++) {
        Value v;

        // If coming from an invalidation bailout, and this is the topmost
        // value, and a value override has been specified, don't read from the
        // iterator. Otherwise, we risk using a garbage value.
        if (!iter.moreFrames() && i == exprStackSlots - 1 &&
            cx->runtime()->hasIonReturnOverride())
        {
            JS_ASSERT(invalidate);
            iter.skip();
            IonSpew(IonSpew_BaselineBailouts, "      [Return Override]");
            v = cx->runtime()->takeIonReturnOverride();
        } else {
            v = iter.read();
        }
        if (!builder.writeValue(v, "StackValue"))
            return false;
    }

    size_t endOfBaselineJSFrameStack = builder.framePushed();

    // If we are resuming at a LOOPENTRY op, resume at the next op to avoid
    // a bailout -> enter Ion -> bailout loop with --ion-eager. See also
    // ThunkToInterpreter.
    if (!resumeAfter) {
        while (true) {
            op = JSOp(*pc);
            if (op == JSOP_GOTO)
                pc += GET_JUMP_OFFSET(pc);
            else if (op == JSOP_LOOPENTRY || op == JSOP_NOP || op == JSOP_LOOPHEAD)
                pc = GetNextPc(pc);
            else
                break;
        }
    }

    uint32_t pcOff = script->pcToOffset(pc);
    bool isCall = IsCallPC(pc);
    BaselineScript *baselineScript = script->baselineScript();

#ifdef DEBUG
    uint32_t expectedDepth;
    bool reachablePC;
    if (!ReconstructStackDepth(cx, script, resumeAfter ? GetNextPc(pc) : pc, &expectedDepth, &reachablePC))
        return false;

    if (reachablePC) {
        if (op != JSOP_FUNAPPLY || !iter.moreFrames() || resumeAfter) {
            if (op == JSOP_FUNCALL) {
                // For fun.call(this, ...); the reconstructStackDepth will
                // include the this. When inlining that is not included.
                // So the exprStackSlots will be one less.
                JS_ASSERT(expectedDepth - exprStackSlots <= 1);
            } else if (iter.moreFrames() && (IsGetPropPC(pc) || IsSetPropPC(pc))) {
                // Accessors coming out of ion are inlined via a complete
                // lie perpetrated by the compiler internally. Ion just rearranges
                // the stack, and pretends that it looked like a call all along.
                // This means that the depth is actually one *more* than expected
                // by the interpreter, as there is now a JSFunction, |this| and [arg],
                // rather than the expected |this| and [arg]
                // Note that none of that was pushed, but it's still reflected
                // in exprStackSlots.
                JS_ASSERT(exprStackSlots - expectedDepth == 1);
            } else {
                // For fun.apply({}, arguments) the reconstructStackDepth will
                // have stackdepth 4, but it could be that we inlined the
                // funapply. In that case exprStackSlots, will have the real
                // arguments in the slots and not be 4.
                JS_ASSERT(exprStackSlots == expectedDepth);
            }
        }
    }

    IonSpew(IonSpew_BaselineBailouts, "      Resuming %s pc offset %d (op %s) (line %d) of %s:%d",
                resumeAfter ? "after" : "at", (int) pcOff, js_CodeName[op],
                PCToLineNumber(script, pc), script->filename(), (int) script->lineno());
    IonSpew(IonSpew_BaselineBailouts, "      Bailout kind: %s",
            BailoutKindString(bailoutKind));
#endif

    // If this was the last inline frame, or we are bailing out to a catch or
    // finally block in this frame, then unpacking is almost done.
    if (!iter.moreFrames() || excInfo) {
        // Last frame, so PC for call to next frame is set to nullptr.
        *callPC = nullptr;

        // If the bailout was a resumeAfter, and the opcode is monitored,
        // then the bailed out state should be in a position to enter
        // into the ICTypeMonitor chain for the op.
        bool enterMonitorChain = false;
        if (resumeAfter && (js_CodeSpec[op].format & JOF_TYPESET)) {
            // Not every monitored op has a monitored fallback stub, e.g.
            // JSOP_NEWOBJECT, which always returns the same type for a
            // particular script/pc location.
            ICEntry &icEntry = baselineScript->icEntryFromPCOffset(pcOff);
            ICFallbackStub *fallbackStub = icEntry.firstStub()->getChainFallback();
            if (fallbackStub->isMonitoredFallback())
                enterMonitorChain = true;
        }

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
            builder.popValueInto(PCMappingSlotInfo::SlotInR0);

            // Need to adjust the frameSize for the frame to match the values popped
            // into registers.
            frameSize -= sizeof(Value);
            blFrame->setFrameSize(frameSize);
            IonSpew(IonSpew_BaselineBailouts, "      Adjusted framesize -= %d: %d",
                            (int) sizeof(Value), (int) frameSize);

            // If resuming into a JSOP_CALL, baseline keeps the arguments on the
            // stack and pops them only after returning from the call IC.
            // Push undefs onto the stack in anticipation of the popping of the
            // callee, thisv, and actual arguments passed from the caller's frame.
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
            PCMappingSlotInfo slotInfo;
            uint8_t *nativeCodeForPC = baselineScript->nativeCodeForPC(script, pc, &slotInfo);
            unsigned numUnsynced = slotInfo.numUnsynced();
            JS_ASSERT(numUnsynced <= 2);
            PCMappingSlotInfo::SlotLocation loc1, loc2;
            if (numUnsynced > 0) {
                loc1 = slotInfo.topSlotLocation();
                IonSpew(IonSpew_BaselineBailouts, "      Popping top stack value into %d.",
                            (int) loc1);
                builder.popValueInto(loc1);
            }
            if (numUnsynced > 1) {
                loc2 = slotInfo.nextSlotLocation();
                IonSpew(IonSpew_BaselineBailouts, "      Popping next stack value into %d.",
                            (int) loc2);
                JS_ASSERT_IF(loc1 != PCMappingSlotInfo::SlotIgnore, loc1 != loc2);
                builder.popValueInto(loc2);
            }

            // Need to adjust the frameSize for the frame to match the values popped
            // into registers.
            frameSize -= sizeof(Value) * numUnsynced;
            blFrame->setFrameSize(frameSize);
            IonSpew(IonSpew_BaselineBailouts, "      Adjusted framesize -= %d: %d",
                            int(sizeof(Value) * numUnsynced), int(frameSize));

            // If scopeChain is nullptr, then bailout is occurring during argument check.
            // In this case, resume into the prologue.
            uint8_t *opReturnAddr;
            if (scopeChain == nullptr) {
                // Global and eval scripts expect the scope chain in R1, so only
                // resume into the prologue for function scripts.
                JS_ASSERT(fun);
                JS_ASSERT(numUnsynced == 0);
                opReturnAddr = baselineScript->prologueEntryAddr();
                IonSpew(IonSpew_BaselineBailouts, "      Resuming into prologue.");

                // If bailing into prologue, HAS_PUSHED_SPS_FRAME should not be set on frame.
                blFrame->unsetPushedSPSFrame();

                // Additionally, if SPS is enabled, there are two corner cases to handle:
                //  1. If resuming into the prologue, and innermost frame is an inlined frame,
                //     and bailout is because of argument check failure, then:
                //          Top SPS profiler entry would be for caller frame.
                //          Ion would not have set the PC index field on that frame
                //              (since this bailout happens before MFunctionBoundary).
                //          Make sure that's done now.
                //  2. If resuming into the prologue, and the bailout is NOT because of an
                //     argument check, then:
                //          Top SPS profiler entry would be for callee frame.
                //          Ion would already have pushed an SPS entry for this frame.
                //          The pc for this entry would be set to nullptr.
                //          Make sure it's set to script->pc.
                if (cx->runtime()->spsProfiler.enabled()) {
                    if (caller && bailoutKind == Bailout_ArgumentCheck) {
                        IonSpew(IonSpew_BaselineBailouts, "      Setting PCidx on innermost "
                                "inlined frame's parent's SPS entry (%s:%d) (pcIdx=%d)!",
                                caller->filename(), caller->lineno(), caller->pcToOffset(callerPC));
                        cx->runtime()->spsProfiler.updatePC(caller, callerPC);
                    } else if (bailoutKind != Bailout_ArgumentCheck) {
                        IonSpew(IonSpew_BaselineBailouts,
                                "      Popping SPS entry for innermost inlined frame's SPS entry");
                        cx->runtime()->spsProfiler.exit(cx, script, fun);
                    }
                }
            } else {
                opReturnAddr = nativeCodeForPC;
            }
            builder.setResumeAddr(opReturnAddr);
            IonSpew(IonSpew_BaselineBailouts, "      Set resumeAddr=%p", opReturnAddr);
        }

        return true;
    }

    *callPC = pc;

    // Write out descriptor of BaselineJS frame.
    size_t baselineFrameDescr = MakeFrameDescriptor((uint32_t) builder.framePushed(),
                                                    IonFrame_BaselineJS);
    if (!builder.writeWord(baselineFrameDescr, "Descriptor"))
        return false;

    // Calculate and write out return address.
    // The icEntry in question MUST have an inlinable fallback stub.
    ICEntry &icEntry = baselineScript->icEntryFromPCOffset(pcOff);
    JS_ASSERT(IsInlinableFallback(icEntry.firstStub()->getChainFallback()));
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
    JS_ASSERT(IsInlinableFallback(icEntry.fallbackStub()));
    if (!builder.writePtr(icEntry.fallbackStub(), "StubPtr"))
        return false;

    // Write previous frame pointer (saved earlier).
    if (!builder.writePtr(prevFramePtr, "PrevFramePtr"))
        return false;
    prevFramePtr = builder.virtualPointerAtStackOffset(0);

    // Write out actual arguments (and thisv), copied from unpacked stack of BaselineJS frame.
    // Arguments are reversed on the BaselineJS frame's stack values.
    JS_ASSERT(IsIonInlinablePC(pc));
    unsigned actualArgc;
    if (needToSaveArgs) {
        // For FUNAPPLY or an accessor, the arguments are not on the stack anymore,
        // but they are copied in a vector and are written here.
        if (op == JSOP_FUNAPPLY)
            actualArgc = blFrame->numActualArgs();
        else
            actualArgc = IsSetPropPC(pc);

        JS_ASSERT(actualArgc + 2 <= exprStackSlots);
        JS_ASSERT(savedCallerArgs.length() == actualArgc + 2);
        for (unsigned i = 0; i < actualArgc + 1; i++) {
            size_t arg = savedCallerArgs.length() - (i + 1);
            if (!builder.writeValue(savedCallerArgs[arg], "ArgVal"))
                return false;
        }
    } else {
        actualArgc = GET_ARGC(pc);
        if (op == JSOP_FUNCALL) {
            JS_ASSERT(actualArgc > 0);
            actualArgc--;
        }

        JS_ASSERT(actualArgc + 2 <= exprStackSlots);
        for (unsigned i = 0; i < actualArgc + 1; i++) {
            size_t argSlot = (script->nfixed() + exprStackSlots) - (i + 1);
            if (!builder.writeValue(*blFrame->valueSlot(argSlot), "ArgVal"))
                return false;
        }
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
    Value callee;
    if (needToSaveArgs) {
        // The arguments of FUNAPPLY or inlined accessors are not writen to the stack.
        // So get the callee from the specially saved vector.
        callee = savedCallerArgs[0];
    } else {
        uint32_t calleeStackSlot = exprStackSlots - uint32_t(actualArgc + 2);
        size_t calleeOffset = (builder.framePushed() - endOfBaselineJSFrameStack)
            + ((exprStackSlots - (calleeStackSlot + 1)) * sizeof(Value));
        callee = *builder.valuePointerAtStackOffset(calleeOffset);
        IonSpew(IonSpew_BaselineBailouts, "      CalleeStackSlot=%d", (int) calleeStackSlot);
    }
    IonSpew(IonSpew_BaselineBailouts, "      Callee = %016llx", *((uint64_t *) &callee));
    JS_ASSERT(callee.isObject() && callee.toObject().is<JSFunction>());
    JSFunction *calleeFun = &callee.toObject().as<JSFunction>();
    if (!builder.writePtr(CalleeToToken(calleeFun), "CalleeToken"))
        return false;
    nextCallee.set(calleeFun);

    // Push BaselineStub frame descriptor
    if (!builder.writeWord(baselineStubFrameDescr, "Descriptor"))
        return false;

    // Push return address into ICCall_Scripted stub, immediately after the call.
    void *baselineCallReturnAddr = GetStubReturnAddress(cx, pc);
    JS_ASSERT(baselineCallReturnAddr);
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
    BufferPointer<uint8_t> stubArgsEnd =
        builder.pointerAtStackOffset<uint8_t>(builder.framePushed() - endOfBaselineStubArgs);
    IonSpew(IonSpew_BaselineBailouts, "      MemCpy from %p", stubArgsEnd.get());
    memcpy(builder.pointerAtStackOffset<uint8_t>(0).get(), stubArgsEnd.get(),
           (actualArgc + 1) * sizeof(Value));

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
    void *rectReturnAddr = cx->runtime()->jitRuntime()->getArgumentsRectifierReturnAddr();
    JS_ASSERT(rectReturnAddr);
    if (!builder.writePtr(rectReturnAddr, "ReturnAddr"))
        return false;

    return true;
}

uint32_t
jit::BailoutIonToBaseline(JSContext *cx, JitActivation *activation, IonBailoutIterator &iter,
                          bool invalidate, BaselineBailoutInfo **bailoutInfo,
                          const ExceptionBailoutInfo *excInfo)
{
    JS_ASSERT(bailoutInfo != nullptr);
    JS_ASSERT(*bailoutInfo == nullptr);

#if JS_TRACE_LOGGING
    TraceLogging::defaultLogger()->log(TraceLogging::INFO_ENGINE_BASELINE);
#endif

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
            iter.script()->filename(), iter.script()->lineno(), (void *) iter.ionScript(),
            (int) prevFrameType);

    if (excInfo)
        IonSpew(IonSpew_BaselineBailouts, "Resuming in catch or finally block");

    IonSpew(IonSpew_BaselineBailouts, "  Reading from snapshot offset %u size %u",
            iter.snapshotOffset(), iter.ionScript()->snapshotsSize());

    if (!excInfo)
        iter.ionScript()->incNumBailouts();
    iter.script()->updateBaselineOrIonRaw();

    // Allocate buffer to hold stack replacement data.
    BaselineStackBuilder builder(iter, 1024);
    if (!builder.init())
        return BAILOUT_RETURN_FATAL_ERROR;
    IonSpew(IonSpew_BaselineBailouts, "  Incoming frame ptr = %p", builder.startFrame());

    SnapshotIterator snapIter(iter);

    RootedFunction callee(cx, iter.maybeCallee());
    if (callee) {
        IonSpew(IonSpew_BaselineBailouts, "  Callee function (%s:%u)",
                callee->existingScript()->filename(), callee->existingScript()->lineno());
    } else {
        IonSpew(IonSpew_BaselineBailouts, "  No callee!");
    }

    if (iter.isConstructing())
        IonSpew(IonSpew_BaselineBailouts, "  Constructing!");
    else
        IonSpew(IonSpew_BaselineBailouts, "  Not constructing!");

    IonSpew(IonSpew_BaselineBailouts, "  Restoring frames:");
    size_t frameNo = 0;

    // Reconstruct baseline frames using the builder.
    RootedScript caller(cx);
    jsbytecode *callerPC = nullptr;
    RootedFunction fun(cx, callee);
    RootedScript scr(cx, iter.script());
    AutoValueVector startFrameFormals(cx);

    while (true) {
#if JS_TRACE_LOGGING
        if (frameNo > 0) {
            TraceLogging::defaultLogger()->log(TraceLogging::SCRIPT_START, scr);
            TraceLogging::defaultLogger()->log(TraceLogging::INFO_ENGINE_BASELINE);
        }
#endif
        IonSpew(IonSpew_BaselineBailouts, "    FrameNo %d", frameNo);

        // If we are bailing out to a catch or finally block in this frame,
        // pass excInfo to InitFromBailout and don't unpack any other frames.
        bool handleException = (excInfo && excInfo->frameNo == frameNo);

        jsbytecode *callPC = nullptr;
        RootedFunction nextCallee(cx, nullptr);
        if (!InitFromBailout(cx, caller, callerPC, fun, scr, iter.ionScript(),
                             snapIter, invalidate, builder, startFrameFormals,
                             &nextCallee, &callPC, handleException ? excInfo : nullptr))
        {
            return BAILOUT_RETURN_FATAL_ERROR;
        }

        if (!snapIter.moreFrames()) {
            JS_ASSERT(!callPC);
            break;
        }

        if (handleException)
            break;

        JS_ASSERT(nextCallee);
        JS_ASSERT(callPC);
        caller = scr;
        callerPC = callPC;
        fun = nextCallee;
        scr = fun->existingScript();
        snapIter.nextFrame();

        frameNo++;
    }
    IonSpew(IonSpew_BaselineBailouts, "  Done restoring frames");
    BailoutKind bailoutKind = snapIter.bailoutKind();

    if (!startFrameFormals.empty()) {
        // Set the first frame's formals, see the comment in InitFromBailout.
        Value *argv = builder.startFrame()->argv() + 1; // +1 to skip |this|.
        mozilla::PodCopy(argv, startFrameFormals.begin(), startFrameFormals.length());
    }

    // Take the reconstructed baseline stack so it doesn't get freed when builder destructs.
    BaselineBailoutInfo *info = builder.takeBuffer();
    info->numFrames = frameNo + 1;

    // Do stack check.
    bool overRecursed = false;
    uint8_t *newsp = info->incomingStack - (info->copyStackTop - info->copyStackBottom);
    JS_CHECK_RECURSION_WITH_SP_DONT_REPORT(cx, newsp, overRecursed = true);
    if (overRecursed) {
        IonSpew(IonSpew_BaselineBailouts, "  Overrecursion check failed!");
        return BAILOUT_RETURN_OVERRECURSED;
    }

    info->bailoutKind = bailoutKind;
    *bailoutInfo = info;
    return BAILOUT_RETURN_OK;
}

static bool
HandleBoundsCheckFailure(JSContext *cx, HandleScript outerScript, HandleScript innerScript)
{
    IonSpew(IonSpew_Bailouts, "Bounds check failure %s:%d, inlined into %s:%d",
            innerScript->filename(), innerScript->lineno(),
            outerScript->filename(), outerScript->lineno());

    JS_ASSERT(!outerScript->ionScript()->invalidated());

    // TODO: Currently this mimic's Ion's handling of this case.  Investigate setting
    // the flag on innerScript as opposed to outerScript, and maybe invalidating both
    // inner and outer scripts, instead of just the outer one.
    if (!outerScript->failedBoundsCheck())
        outerScript->setFailedBoundsCheck();
    IonSpew(IonSpew_BaselineBailouts, "Invalidating due to bounds check failure");
    return Invalidate(cx, outerScript);
}

static bool
HandleShapeGuardFailure(JSContext *cx, HandleScript outerScript, HandleScript innerScript)
{
    IonSpew(IonSpew_Bailouts, "Shape guard failure %s:%d, inlined into %s:%d",
            innerScript->filename(), innerScript->lineno(),
            outerScript->filename(), outerScript->lineno());

    JS_ASSERT(!outerScript->ionScript()->invalidated());

    // TODO: Currently this mimic's Ion's handling of this case.  Investigate setting
    // the flag on innerScript as opposed to outerScript, and maybe invalidating both
    // inner and outer scripts, instead of just the outer one.
    outerScript->setFailedShapeGuard();
    IonSpew(IonSpew_BaselineBailouts, "Invalidating due to shape guard failure");
    return Invalidate(cx, outerScript);
}

static bool
HandleBaselineInfoBailout(JSContext *cx, JSScript *outerScript, JSScript *innerScript)
{
    IonSpew(IonSpew_Bailouts, "Baseline info failure %s:%d, inlined into %s:%d",
            innerScript->filename(), innerScript->lineno(),
            outerScript->filename(), outerScript->lineno());

    JS_ASSERT(!outerScript->ionScript()->invalidated());

    IonSpew(IonSpew_BaselineBailouts, "Invalidating due to invalid baseline info");
    return Invalidate(cx, outerScript);
}

uint32_t
jit::FinishBailoutToBaseline(BaselineBailoutInfo *bailoutInfo)
{
    // The caller pushes R0 and R1 on the stack without rooting them.
    // Since GC here is very unlikely just suppress it.
    JSContext *cx = GetIonContext()->cx;
    js::gc::AutoSuppressGC suppressGC(cx);

    IonSpew(IonSpew_BaselineBailouts, "  Done restoring frames");

    // Check that we can get the current script's PC.
#ifdef DEBUG
    jsbytecode *pc;
    cx->currentScript(&pc);
    IonSpew(IonSpew_BaselineBailouts, "  Got pc=%p", pc);
#endif

    uint32_t numFrames = bailoutInfo->numFrames;
    JS_ASSERT(numFrames > 0);
    BailoutKind bailoutKind = bailoutInfo->bailoutKind;

    // Free the bailout buffer.
    js_free(bailoutInfo);
    bailoutInfo = nullptr;

    // Ensure the frame has a call object if it needs one. If the scope chain
    // is nullptr, we will enter baseline code at the prologue so no need to do
    // anything in that case.
    BaselineFrame *topFrame = GetTopBaselineFrame(cx);
    if (topFrame->scopeChain() && !EnsureHasScopeObjects(cx, topFrame))
        return false;

    // Create arguments objects for bailed out frames, to maintain the invariant
    // that script->needsArgsObj() implies frame->hasArgsObj().
    RootedScript innerScript(cx, nullptr);
    RootedScript outerScript(cx, nullptr);

    JS_ASSERT(cx->currentlyRunningInJit());
    IonFrameIterator iter(cx);

    uint32_t frameno = 0;
    while (frameno < numFrames) {
        JS_ASSERT(!iter.isOptimizedJS());

        if (iter.isBaselineJS()) {
            BaselineFrame *frame = iter.baselineFrame();

            // If the frame doesn't even have a scope chain set yet, then it's resuming
            // into the the prologue before the scope chain is initialized.  Any
            // necessary args object will also be initialized there.
            if (frame->scopeChain() && frame->script()->needsArgsObj()) {
                ArgumentsObject *argsObj;
                if (frame->hasArgsObj()) {
                    argsObj = &frame->argsObj();
                } else {
                    argsObj = ArgumentsObject::createExpected(cx, frame);
                    if (!argsObj)
                        return false;
                }

                // The arguments is a local binding and needsArgsObj does not
                // check if it is clobbered. Ensure that the local binding
                // restored during bailout before storing the arguments object
                // to the slot.
                RootedScript script(cx, frame->script());
                SetFrameArgumentsObject(cx, frame, script, argsObj);
            }

            if (frameno == 0)
                innerScript = frame->script();

            if (frameno == numFrames - 1)
                outerScript = frame->script();

            frameno++;
        }

        ++iter;
    }

    JS_ASSERT(innerScript);
    JS_ASSERT(outerScript);
    IonSpew(IonSpew_BaselineBailouts,
            "  Restored outerScript=(%s:%u,%u) innerScript=(%s:%u,%u) (bailoutKind=%u)",
            outerScript->filename(), outerScript->lineno(), outerScript->getUseCount(),
            innerScript->filename(), innerScript->lineno(), innerScript->getUseCount(),
            (unsigned) bailoutKind);

    switch (bailoutKind) {
      case Bailout_Normal:
        // Do nothing.
        break;
      case Bailout_ArgumentCheck:
        // Do nothing, bailout will resume before the argument monitor ICs.
        break;
      case Bailout_BoundsCheck:
        if (!HandleBoundsCheckFailure(cx, outerScript, innerScript))
            return false;
        break;
      case Bailout_ShapeGuard:
        if (!HandleShapeGuardFailure(cx, outerScript, innerScript))
            return false;
        break;
      case Bailout_BaselineInfo:
        if (!HandleBaselineInfoBailout(cx, outerScript, innerScript))
            return false;
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown bailout kind!");
    }

    if (!CheckFrequentBailouts(cx, outerScript))
        return false;

    return true;
}
