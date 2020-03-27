/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScopeExit.h"

#include "debugger/DebugAPI.h"
#include "jit/arm/Simulator-arm.h"
#include "jit/BaselineFrame.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/CompileInfo.h"
#include "jit/Ion.h"
#include "jit/JitSpewer.h"
#include "jit/mips32/Simulator-mips32.h"
#include "jit/mips64/Simulator-mips64.h"
#include "jit/Recover.h"
#include "jit/RematerializedFrame.h"
#include "js/Utility.h"
#include "util/Memory.h"
#include "vm/ArgumentsObject.h"
#include "vm/BytecodeUtil.h"
#include "vm/TraceLogging.h"

#include "jit/JitFrames-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

// BaselineStackBuilder may reallocate its buffer if the current one is too
// small. To avoid dangling pointers, BufferPointer represents a pointer into
// this buffer as a pointer to the header and a fixed offset.
template <typename T>
class BufferPointer {
  const UniquePtr<BaselineBailoutInfo>& header_;
  size_t offset_;
  bool heap_;

 public:
  BufferPointer(const UniquePtr<BaselineBailoutInfo>& header, size_t offset,
                bool heap)
      : header_(header), offset_(offset), heap_(heap) {}

  T* get() const {
    BaselineBailoutInfo* header = header_.get();
    if (!heap_) {
      return (T*)(header->incomingStack + offset_);
    }

    uint8_t* p = header->copyStackTop - offset_;
    MOZ_ASSERT(p >= header->copyStackBottom && p < header->copyStackTop);
    return (T*)p;
  }

  void set(const T& value) { *get() = value; }

  // Note: we return a copy instead of a reference, to avoid potential memory
  // safety hazards when the underlying buffer gets resized.
  const T operator*() const { return *get(); }
  T* operator->() const { return get(); }
};

/**
 * BaselineStackBuilder helps abstract the process of rebuilding the C stack on
 * the heap. It takes a bailout iterator and keeps track of the point on the C
 * stack from which the reconstructed frames will be written.
 *
 * It exposes methods to write data into the heap memory storing the
 * reconstructed stack.  It also exposes method to easily calculate addresses.
 * This includes both the virtual address that a particular value will be at
 * when it's eventually copied onto the stack, as well as the current actual
 * address of that value (whether on the heap allocated portion being
 * constructed or the existing stack).
 *
 * The abstraction handles transparent re-allocation of the heap memory when it
 * needs to be enlarged to accommodate new data.  Similarly to the C stack, the
 * data that's written to the reconstructed stack grows from high to low in
 * memory.
 *
 * The lowest region of the allocated memory contains a BaselineBailoutInfo
 * structure that points to the start and end of the written data.
 */
struct BaselineStackBuilder {
  JSContext* cx_;
  const JSJitFrameIter& iter_;
  JitFrameLayout* frame_ = nullptr;

  size_t bufferTotal_ = 0;
  size_t bufferAvail_ = 0;
  size_t bufferUsed_ = 0;
  size_t framePushed_ = 0;

  UniquePtr<BaselineBailoutInfo> header_;

  BaselineStackBuilder(JSContext* cx, const JSJitFrameIter& iter,
                       size_t initialSize)
      : cx_(cx),
        iter_(iter),
        frame_(static_cast<JitFrameLayout*>(iter.current())),
        bufferTotal_(initialSize) {
    MOZ_ASSERT(bufferTotal_ >= sizeof(BaselineBailoutInfo));
    MOZ_ASSERT(iter.isBailoutJS());
  }

  MOZ_MUST_USE bool init() {
    MOZ_ASSERT(!header_);
    MOZ_ASSERT(bufferUsed_ == 0);

    uint8_t* bufferRaw = cx_->pod_calloc<uint8_t>(bufferTotal_);
    if (!bufferRaw) {
      return false;
    }
    bufferAvail_ = bufferTotal_ - sizeof(BaselineBailoutInfo);

    header_.reset(new (bufferRaw) BaselineBailoutInfo());
    header_->incomingStack = reinterpret_cast<uint8_t*>(frame_);
    header_->copyStackTop = bufferRaw + bufferTotal_;
    header_->copyStackBottom = header_->copyStackTop;
    return true;
  }

  MOZ_MUST_USE bool enlarge() {
    MOZ_ASSERT(header_ != nullptr);
    if (bufferTotal_ & mozilla::tl::MulOverflowMask<2>::value) {
      ReportOutOfMemory(cx_);
      return false;
    }

    size_t newSize = bufferTotal_ * 2;
    uint8_t* newBufferRaw = cx_->pod_calloc<uint8_t>(newSize);
    if (!newBufferRaw) {
      return false;
    }

    // Initialize the new buffer.
    //
    //   Before:
    //
    //     [ Header | .. | Payload ]
    //
    //   After:
    //
    //     [ Header | ............... | Payload ]
    //
    // Size of Payload is |bufferUsed_|.
    //
    // We need to copy from the old buffer and header to the new buffer before
    // we set header_ (this deletes the old buffer).
    //
    // We also need to update |copyStackBottom| and |copyStackTop| because these
    // fields point to the Payload's start and end, respectively.
    using BailoutInfoPtr = UniquePtr<BaselineBailoutInfo>;
    BailoutInfoPtr newHeader(new (newBufferRaw) BaselineBailoutInfo(*header_));
    newHeader->copyStackTop = newBufferRaw + newSize;
    newHeader->copyStackBottom = newHeader->copyStackTop - bufferUsed_;
    memcpy(newHeader->copyStackBottom, header_->copyStackBottom, bufferUsed_);
    bufferTotal_ = newSize;
    bufferAvail_ = newSize - (sizeof(BaselineBailoutInfo) + bufferUsed_);
    header_ = std::move(newHeader);
    return true;
  }

  BaselineBailoutInfo* info() {
    MOZ_ASSERT(header_);
    return header_.get();
  }

  BaselineBailoutInfo* takeBuffer() {
    MOZ_ASSERT(header_);
    return header_.release();
  }

  void resetFramePushed() { framePushed_ = 0; }

  size_t framePushed() const { return framePushed_; }

  MOZ_MUST_USE bool subtract(size_t size, const char* info = nullptr) {
    // enlarge the buffer if need be.
    while (size > bufferAvail_) {
      if (!enlarge()) {
        return false;
      }
    }

    // write out element.
    header_->copyStackBottom -= size;
    bufferAvail_ -= size;
    bufferUsed_ += size;
    framePushed_ += size;
    if (info) {
      JitSpew(JitSpew_BaselineBailouts, "      SUB_%03d   %p/%p %-15s",
              (int)size, header_->copyStackBottom,
              virtualPointerAtStackOffset(0), info);
    }
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool write(const T& t) {
    MOZ_ASSERT(!(uintptr_t(&t) >= uintptr_t(header_->copyStackBottom) &&
                 uintptr_t(&t) < uintptr_t(header_->copyStackTop)),
               "Should not reference memory that can be freed");
    if (!subtract(sizeof(T))) {
      return false;
    }
    memcpy(header_->copyStackBottom, &t, sizeof(T));
    return true;
  }

  template <typename T>
  MOZ_MUST_USE bool writePtr(T* t, const char* info) {
    if (!write<T*>(t)) {
      return false;
    }
    if (info) {
      JitSpew(JitSpew_BaselineBailouts, "      WRITE_PTR %p/%p %-15s %p",
              header_->copyStackBottom, virtualPointerAtStackOffset(0), info,
              t);
    }
    return true;
  }

  MOZ_MUST_USE bool writeWord(size_t w, const char* info) {
    if (!write<size_t>(w)) {
      return false;
    }
    if (info) {
      if (sizeof(size_t) == 4) {
        JitSpew(JitSpew_BaselineBailouts, "      WRITE_WRD %p/%p %-15s %08zx",
                header_->copyStackBottom, virtualPointerAtStackOffset(0), info,
                w);
      } else {
        JitSpew(JitSpew_BaselineBailouts, "      WRITE_WRD %p/%p %-15s %016zx",
                header_->copyStackBottom, virtualPointerAtStackOffset(0), info,
                w);
      }
    }
    return true;
  }

  MOZ_MUST_USE bool writeValue(const Value& val, const char* info) {
    if (!write<Value>(val)) {
      return false;
    }
    if (info) {
      JitSpew(JitSpew_BaselineBailouts,
              "      WRITE_VAL %p/%p %-15s %016" PRIx64,
              header_->copyStackBottom, virtualPointerAtStackOffset(0), info,
              *((uint64_t*)&val));
    }
    return true;
  }

  MOZ_MUST_USE bool maybeWritePadding(size_t alignment, size_t after,
                                      const char* info) {
    MOZ_ASSERT(framePushed_ % sizeof(Value) == 0);
    MOZ_ASSERT(after % sizeof(Value) == 0);
    size_t offset = ComputeByteAlignment(after, alignment);
    while (framePushed_ % alignment != offset) {
      if (!writeValue(MagicValue(JS_ARG_POISON), info)) {
        return false;
      }
    }

    return true;
  }

  Value popValue() {
    MOZ_ASSERT(bufferUsed_ >= sizeof(Value));
    MOZ_ASSERT(framePushed_ >= sizeof(Value));
    bufferAvail_ += sizeof(Value);
    bufferUsed_ -= sizeof(Value);
    framePushed_ -= sizeof(Value);
    Value result = *((Value*)header_->copyStackBottom);
    header_->copyStackBottom += sizeof(Value);
    return result;
  }

  void setResumeFramePtr(void* resumeFramePtr) {
    header_->resumeFramePtr = resumeFramePtr;
  }

  void setResumeAddr(void* resumeAddr) { header_->resumeAddr = resumeAddr; }

  void setMonitorPC(jsbytecode* pc) { header_->monitorPC = pc; }

  void setFrameSizeOfInnerMostFrame(uint32_t size) {
    header_->frameSizeOfInnerMostFrame = size;
  }

  template <typename T>
  BufferPointer<T> pointerAtStackOffset(size_t offset) {
    if (offset < bufferUsed_) {
      // Calculate offset from copyStackTop.
      offset = header_->copyStackTop - (header_->copyStackBottom + offset);
      return BufferPointer<T>(header_, offset, /* heap = */ true);
    }

    return BufferPointer<T>(header_, offset - bufferUsed_, /* heap = */ false);
  }

  BufferPointer<Value> valuePointerAtStackOffset(size_t offset) {
    return pointerAtStackOffset<Value>(offset);
  }

  inline uint8_t* virtualPointerAtStackOffset(size_t offset) {
    if (offset < bufferUsed_) {
      return reinterpret_cast<uint8_t*>(frame_) - (bufferUsed_ - offset);
    }
    return reinterpret_cast<uint8_t*>(frame_) + (offset - bufferUsed_);
  }

  inline JitFrameLayout* startFrame() { return frame_; }

  BufferPointer<JitFrameLayout> topFrameAddress() {
    return pointerAtStackOffset<JitFrameLayout>(0);
  }

  //
  // This method should only be called when the builder is in a state where it
  // is starting to construct the stack frame for the next callee.  This means
  // that the lowest value on the constructed stack is the return address for
  // the previous caller frame.
  //
  // This method is used to compute the value of the frame pointer (e.g. ebp on
  // x86) that would have been saved by the baseline jitcode when it was
  // entered.  In some cases, this value can be bogus since we can ensure that
  // the caller would have saved it anyway.
  //
  void* calculatePrevFramePtr() {
    // Get the incoming frame.
    BufferPointer<JitFrameLayout> topFrame = topFrameAddress();
    FrameType type = topFrame->prevType();

    // For IonJS, IonICCall and Entry frames, the "saved" frame pointer
    // in the baseline frame is meaningless, since Ion saves all registers
    // before calling other ion frames, and the entry frame saves all
    // registers too.
    if (JSJitFrameIter::isEntry(type) || type == FrameType::IonJS ||
        type == FrameType::IonICCall) {
      return nullptr;
    }

    // BaselineStub - Baseline calling into Ion.
    //  PrevFramePtr needs to point to the BaselineStubFrame's saved frame
    //  pointer.
    //      STACK_START_ADDR
    //          + JitFrameLayout::Size()
    //          + PREV_FRAME_SIZE
    //          - BaselineStubFrameLayout::reverseOffsetOfSavedFramePtr()
    if (type == FrameType::BaselineStub) {
      size_t offset = JitFrameLayout::Size() + topFrame->prevFrameLocalSize() +
                      BaselineStubFrameLayout::reverseOffsetOfSavedFramePtr();
      return virtualPointerAtStackOffset(offset);
    }

    MOZ_ASSERT(type == FrameType::Rectifier);
    // Rectifier - behaviour depends on the frame preceding the rectifier frame,
    // and whether the arch is x86 or not.  The x86 rectifier frame saves the
    // frame pointer, so we can calculate it directly.  For other archs, the
    // previous frame pointer is stored on the stack in the frame that precedes
    // the rectifier frame.
    size_t priorOffset =
        JitFrameLayout::Size() + topFrame->prevFrameLocalSize();
#if defined(JS_CODEGEN_X86)
    // On X86, the FramePointer is pushed as the first value in the Rectifier
    // frame.
    static_assert(BaselineFrameReg == FramePointer);
    priorOffset -= sizeof(void*);
    return virtualPointerAtStackOffset(priorOffset);
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) ||   \
    defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64) || \
    defined(JS_CODEGEN_X64)
    // On X64, ARM, ARM64, and MIPS, the frame pointer save location depends on
    // the caller of the rectifier frame.
    BufferPointer<RectifierFrameLayout> priorFrame =
        pointerAtStackOffset<RectifierFrameLayout>(priorOffset);
    FrameType priorType = priorFrame->prevType();
    MOZ_ASSERT(JSJitFrameIter::isEntry(priorType) ||
               priorType == FrameType::IonJS ||
               priorType == FrameType::BaselineStub);

    // If the frame preceding the rectifier is an IonJS or entry frame,
    // then once again the frame pointer does not matter.
    if (priorType == FrameType::IonJS || JSJitFrameIter::isEntry(priorType)) {
      return nullptr;
    }

    // Otherwise, the frame preceding the rectifier is a BaselineStub frame.
    //  let X = STACK_START_ADDR + JitFrameLayout::Size() + PREV_FRAME_SIZE
    //      X + RectifierFrameLayout::Size()
    //        + ((RectifierFrameLayout*) X)->prevFrameLocalSize()
    //        - BaselineStubFrameLayout::reverseOffsetOfSavedFramePtr()
    size_t extraOffset =
        RectifierFrameLayout::Size() + priorFrame->prevFrameLocalSize() +
        BaselineStubFrameLayout::reverseOffsetOfSavedFramePtr();
    return virtualPointerAtStackOffset(priorOffset + extraOffset);
#elif defined(JS_CODEGEN_NONE)
    (void)priorOffset;
    MOZ_CRASH();
#else
#  error "Bad architecture!"
#endif
  }
};

#ifdef DEBUG
static inline bool IsInlinableFallback(ICFallbackStub* icEntry) {
  return icEntry->isCall_Fallback() || icEntry->isGetProp_Fallback() ||
         icEntry->isSetProp_Fallback() || icEntry->isGetElem_Fallback();
}
#endif

static inline void* GetStubReturnAddress(JSContext* cx, JSOp op) {
  const BaselineICFallbackCode& code =
      cx->runtime()->jitRuntime()->baselineICFallbackCode();

  if (IsGetPropOp(op)) {
    return code.bailoutReturnAddr(BailoutReturnKind::GetProp);
  }
  if (IsSetPropOp(op)) {
    return code.bailoutReturnAddr(BailoutReturnKind::SetProp);
  }
  if (IsGetElemOp(op)) {
    return code.bailoutReturnAddr(BailoutReturnKind::GetElem);
  }

  // This should be a call op of some kind, now.
  MOZ_ASSERT(IsInvokeOp(op) && !IsSpreadOp(op));
  if (IsConstructOp(op)) {
    return code.bailoutReturnAddr(BailoutReturnKind::New);
  }
  return code.bailoutReturnAddr(BailoutReturnKind::Call);
}

static inline jsbytecode* GetNextNonLoopHeadPc(jsbytecode* pc,
                                               jsbytecode** skippedLoopHead) {
  JSOp op = JSOp(*pc);
  switch (op) {
    case JSOp::Goto:
      return pc + GET_JUMP_OFFSET(pc);

    case JSOp::LoopHead:
      *skippedLoopHead = pc;
      return GetNextPc(pc);

    case JSOp::Nop:
      return GetNextPc(pc);

    default:
      return pc;
  }
}

// Returns the pc to resume execution at in Baseline after a bailout.
static jsbytecode* GetResumePC(JSScript* script, jsbytecode* pc,
                               bool resumeAfter) {
  if (resumeAfter) {
    return GetNextPc(pc);
  }

  // If we are resuming at a LoopHead op, resume at the next op to avoid
  // a bailout -> enter Ion -> bailout loop with --ion-eager.
  //
  // The algorithm below is the "tortoise and the hare" algorithm. See bug
  // 994444 for more explanation.
  jsbytecode* skippedLoopHead = nullptr;
  jsbytecode* fasterPc = pc;
  while (true) {
    pc = GetNextNonLoopHeadPc(pc, &skippedLoopHead);
    fasterPc = GetNextNonLoopHeadPc(fasterPc, &skippedLoopHead);
    fasterPc = GetNextNonLoopHeadPc(fasterPc, &skippedLoopHead);
    if (fasterPc == pc) {
      break;
    }
  }

  return pc;
}

static bool HasLiveStackValueAtDepth(HandleScript script, jsbytecode* pc,
                                     uint32_t stackSlotIndex,
                                     uint32_t stackDepth) {
  // Return true iff stackSlotIndex is a stack value that's part of an active
  // iterator loop instead of a normal expression stack slot.

  MOZ_ASSERT(stackSlotIndex < stackDepth);

  for (TryNoteIterAllNoGC tni(script, pc); !tni.done(); ++tni) {
    const TryNote& tn = **tni;

    switch (tn.kind()) {
      case TryNoteKind::ForIn:
      case TryNoteKind::ForOf:
      case TryNoteKind::Destructuring:
        MOZ_ASSERT(tn.stackDepth <= stackDepth);
        if (stackSlotIndex < tn.stackDepth) {
          return true;
        }
        break;

      default:
        break;
    }
  }

  return false;
}

static bool IsPrologueBailout(const SnapshotIterator& iter,
                              const ExceptionBailoutInfo* excInfo) {
  // If we are propagating an exception for debug mode, we will not resume
  // into baseline code, but instead into HandleExceptionBaseline (i.e.,
  // never before the prologue).
  return iter.pcOffset() == 0 && !iter.resumeAfter() &&
         (!excInfo || !excInfo->propagatingIonExceptionForDebugMode());
}

/* clang-format off */
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
//             |        +---------------+  --- The inlined frame might OSR in Ion
//             |        |   Padding?    |  --- Thus the return address should be aligned.
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
//         +------------| Descr(BLStub) |
//                      +---------------+
//                      |  ReturnAddr   | <-- return into ICCall_Scripted IC
//             --       +===============+ --- IF CALLEE FORMAL ARGS > ActualArgC
//             |        |   Padding?    |
//             |        +---------------+
//             |        |  UndefinedU   |
//             |        +---------------+
//             |        |     ...       |
//             |        +---------------+
//             |        |  Undefined0   |
//         +--<         +---------------+
//         |   |        |     ArgA      |
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
/* clang-format on */
static bool InitFromBailout(JSContext* cx, size_t frameNo, HandleFunction fun,
                            HandleScript script, SnapshotIterator& iter,
                            bool invalidate, BaselineStackBuilder& builder,
                            MutableHandle<GCVector<Value>> startFrameFormals,
                            MutableHandleFunction nextCallee,
                            const ExceptionBailoutInfo* excInfo) {
  // The Baseline frames we will reconstruct on the heap are not rooted, so GC
  // must be suppressed here.
  MOZ_ASSERT(cx->suppressGC);

  MOZ_ASSERT(script->hasBaselineScript());

  // Are we catching an exception?
  bool catchingException = excInfo && excInfo->catchingException();

  // If we are catching an exception, we are bailing out to a catch or
  // finally block and this is the frame where we will resume. Usually the
  // expression stack should be empty in this case but there can be
  // iterators on the stack.
  uint32_t exprStackSlots;
  if (catchingException) {
    exprStackSlots = excInfo->numExprSlots();
  } else {
    exprStackSlots =
        iter.numAllocations() - (script->nfixed() + CountArgSlots(script, fun));
  }

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

  JitSpew(JitSpew_BaselineBailouts, "      Unpacking %s:%u:%u",
          script->filename(), script->lineno(), script->column());
  JitSpew(JitSpew_BaselineBailouts, "      [BASELINE-JS FRAME]");

  // Calculate and write the previous frame pointer value.
  // Record the virtual stack offset at this location.  Later on, if we end up
  // writing out a BaselineStub frame for the next callee, we'll need to save
  // the address.
  void* prevFramePtr = builder.calculatePrevFramePtr();
  if (!builder.writePtr(prevFramePtr, "PrevFramePtr")) {
    return false;
  }
  prevFramePtr = builder.virtualPointerAtStackOffset(0);

  // Write struct BaselineFrame.
  if (!builder.subtract(BaselineFrame::Size(), "BaselineFrame")) {
    return false;
  }
  BufferPointer<BaselineFrame> blFrame =
      builder.pointerAtStackOffset<BaselineFrame>(0);

  uint32_t flags = BaselineFrame::RUNNING_IN_INTERPRETER;

  // If we are bailing to a script whose execution is observed, mark the
  // baseline frame as a debuggee frame. This is to cover the case where we
  // don't rematerialize the Ion frame via the Debugger.
  if (script->isDebuggee()) {
    flags |= BaselineFrame::DEBUGGEE;
  }

  // Initialize BaselineFrame's envChain and argsObj
  JSObject* envChain = nullptr;
  Value returnValue = UndefinedValue();
  ArgumentsObject* argsObj = nullptr;
  BailoutKind bailoutKind = iter.bailoutKind();
  if (bailoutKind == Bailout_ArgumentCheck) {
    // Skip the (unused) envChain, because it could be bogus (we can fail before
    // the env chain slot is set) and use the function's initial environment.
    // This will be fixed up later if needed in |FinishBailoutToBaseline|, which
    // calls |EnsureHasEnvironmentObjects|.
    JitSpew(JitSpew_BaselineBailouts,
            "      Bailout_ArgumentCheck! (using function's environment)");
    iter.skip();
    envChain = fun->environment();

    // skip |return value|
    iter.skip();

    // Scripts with |argumentsHasVarBinding| have an extra slot.
    if (script->argumentsHasVarBinding()) {
      JitSpew(
          JitSpew_BaselineBailouts,
          "      Bailout_ArgumentCheck for script with argumentsHasVarBinding!"
          "Using empty arguments object");
      iter.skip();
    }
  } else {
    Value v = iter.read();
    if (v.isObject()) {
      envChain = &v.toObject();

      // If Ion has updated env slot from UndefinedValue, it will be the
      // complete initial environment, so we can set the HAS_INITIAL_ENV
      // flag if needed.
      if (fun && fun->needsFunctionEnvironmentObjects()) {
        MOZ_ASSERT(fun->nonLazyScript()->initialEnvironmentShape());
        MOZ_ASSERT(!fun->needsExtraBodyVarEnvironment());
        flags |= BaselineFrame::HAS_INITIAL_ENV;
      }
    } else {
      MOZ_ASSERT(v.isUndefined() || v.isMagic(JS_OPTIMIZED_OUT));

#ifdef DEBUG
      // The |envChain| slot must not be optimized out if the currently
      // active scope requires any EnvironmentObjects beyond what is
      // available at body scope. This checks that scope chain does not
      // require any such EnvironmentObjects.
      // See also: |CompileInfo::isObservableFrameSlot|
      jsbytecode* pc = script->offsetToPC(iter.pcOffset());
      Scope* scopeIter = script->innermostScope(pc);
      while (scopeIter != script->bodyScope()) {
        MOZ_ASSERT(scopeIter);
        MOZ_ASSERT(!scopeIter->hasEnvironment());
        scopeIter = scopeIter->enclosing();
      }
#endif

      // Get env chain from function or script.
      if (fun) {
        envChain = fun->environment();
      } else if (script->module()) {
        envChain = script->module()->environment();
      } else {
        // For global scripts without a non-syntactic env the env
        // chain is the script's global lexical environment (Ion does
        // not compile scripts with a non-syntactic global scope).
        // Also note that it's invalid to resume into the prologue in
        // this case because the prologue expects the env chain in R1
        // for eval and global scripts.
        MOZ_ASSERT(!script->isForEval());
        MOZ_ASSERT(!script->hasNonSyntacticScope());
        envChain = &(script->global().lexicalEnvironment());
      }
    }

    if (script->noScriptRval()) {
      // Don't use the return value (likely a JS_OPTIMIZED_OUT MagicValue) to
      // not confuse Baseline.
      iter.skip();
    } else {
      // Make sure to add HAS_RVAL to |flags| and not blFrame->flags because
      // blFrame->setFlags(flags) below clobbers all frame flags.
      flags |= BaselineFrame::HAS_RVAL;
      returnValue = iter.read();
    }

    // If script maybe has an arguments object, the third slot will hold it.
    if (script->argumentsHasVarBinding()) {
      v = iter.read();
      MOZ_ASSERT(v.isObject() || v.isUndefined() ||
                 v.isMagic(JS_OPTIMIZED_OUT));
      if (v.isObject()) {
        argsObj = &v.toObject().as<ArgumentsObject>();
      }
    }
  }

  MOZ_ASSERT(envChain);

  JitSpew(JitSpew_BaselineBailouts, "      EnvChain=%p", envChain);
  blFrame->setEnvironmentChain(envChain);
  JitSpew(JitSpew_BaselineBailouts, "      ReturnValue=%016" PRIx64,
          *((uint64_t*)&returnValue));
  blFrame->setReturnValue(returnValue);

  // Do not need to initialize scratchValue field in BaselineFrame.
  blFrame->setFlags(flags);

  // initArgsObjUnchecked modifies the frame's flags, so call it after setFlags.
  if (argsObj) {
    blFrame->initArgsObjUnchecked(*argsObj);
  }

  if (fun) {
    // The unpacked thisv and arguments should overwrite the pushed args present
    // in the calling frame.
    Value thisv = iter.read();
    JitSpew(JitSpew_BaselineBailouts, "      Is function!");
    JitSpew(JitSpew_BaselineBailouts, "      thisv=%016" PRIx64,
            *((uint64_t*)&thisv));

    size_t thisvOffset = builder.framePushed() + JitFrameLayout::offsetOfThis();
    builder.valuePointerAtStackOffset(thisvOffset).set(thisv);

    MOZ_ASSERT(iter.numAllocations() >= CountArgSlots(script, fun));
    JitSpew(JitSpew_BaselineBailouts,
            "      frame slots %u, nargs %zu, nfixed %zu",
            iter.numAllocations(), fun->nargs(), script->nfixed());

    bool argsObjAliasesFormals = script->argsObjAliasesFormals();
    if (frameNo == 0 && !argsObjAliasesFormals) {
      // This is the first (outermost) frame and we don't have an
      // arguments object aliasing the formals. Store the formals in a
      // Vector until we are done. Due to UCE and phi elimination, we
      // could store an UndefinedValue() here for formals we think are
      // unused, but locals may still reference the original argument slot
      // (MParameter/LArgument) and expect the original Value.
      MOZ_ASSERT(startFrameFormals.empty());
      if (!startFrameFormals.resize(fun->nargs())) {
        return false;
      }
    }

    for (uint32_t i = 0; i < fun->nargs(); i++) {
      Value arg = iter.read();
      JitSpew(JitSpew_BaselineBailouts, "      arg %d = %016" PRIx64, (int)i,
              *((uint64_t*)&arg));
      if (frameNo > 0) {
        size_t argOffset =
            builder.framePushed() + JitFrameLayout::offsetOfActualArg(i);
        builder.valuePointerAtStackOffset(argOffset).set(arg);
      } else if (argsObjAliasesFormals) {
        // When the arguments object aliases the formal arguments, then
        // JSOp::SetArg mutates the argument object. In such cases, the
        // list of arguments reported by the snapshot are only aliases
        // of argument object slots which are optimized to only store
        // differences compared to arguments which are on the stack.
      } else {
        startFrameFormals[i].set(arg);
      }
    }
  }

  for (uint32_t i = 0; i < script->nfixed(); i++) {
    Value slot = iter.read();
    if (!builder.writeValue(slot, "FixedValue")) {
      return false;
    }
  }

  // Get the pc. If we are handling an exception, resume at the pc of the
  // catch or finally block.
  jsbytecode* const pc = catchingException
                             ? excInfo->resumePC()
                             : script->offsetToPC(iter.pcOffset());
  const bool resumeAfter = catchingException ? false : iter.resumeAfter();

  // When pgo is enabled, increment the counter of the block in which we
  // resume, as Ion does not keep track of the code coverage.
  //
  // We need to do that when pgo is enabled, as after a specific number of
  // FirstExecution bailouts, we invalidate and recompile the script with
  // IonMonkey. Failing to increment the counter of the current basic block
  // might lead to repeated bailouts and invalidations.
  if (!JitOptions.disablePgo && script->hasScriptCounts()) {
    script->incHitCount(pc);
  }

  const JSOp op = JSOp(*pc);

  // Inlining of SpreadCall-like frames not currently supported.
  MOZ_ASSERT_IF(IsSpreadOp(op), !iter.moreFrames());

  // Fixup inlined JSOp::FunCall, JSOp::FunApply, and accessors on the caller
  // side. On the caller side this must represent like the function wasn't
  // inlined.
  uint32_t pushedSlots = 0;
  RootedValueVector savedCallerArgs(cx);
  bool needToSaveArgs =
      op == JSOp::FunApply || IsIonInlinableGetterOrSetterOp(op);
  if (iter.moreFrames() && (op == JSOp::FunCall || needToSaveArgs)) {
    uint32_t inlined_args = 0;
    if (op == JSOp::FunCall) {
      inlined_args = 2 + GET_ARGC(pc) - 1;
    } else if (op == JSOp::FunApply) {
      inlined_args = 2 + blFrame->numActualArgs();
    } else {
      MOZ_ASSERT(IsIonInlinableGetterOrSetterOp(op));
      inlined_args = 2 + IsSetPropOp(op);
    }

    MOZ_ASSERT(exprStackSlots >= inlined_args);
    pushedSlots = exprStackSlots - inlined_args;

    JitSpew(JitSpew_BaselineBailouts,
            "      pushing %u expression stack slots before fixup",
            pushedSlots);
    for (uint32_t i = 0; i < pushedSlots; i++) {
      Value v = iter.read();
      if (!builder.writeValue(v, "StackValue")) {
        return false;
      }
    }

    if (op == JSOp::FunCall) {
      // When funcall got inlined and the native js_fun_call was bypassed,
      // the stack state is incorrect. To restore correctly it must look like
      // js_fun_call was actually called. This means transforming the stack
      // from |target, this, args| to |js_fun_call, target, this, args|
      // The js_fun_call is never read, so just pushing undefined now.
      JitSpew(JitSpew_BaselineBailouts,
              "      pushing undefined to fixup funcall");
      if (!builder.writeValue(UndefinedValue(), "StackValue")) {
        return false;
      }
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
      if (op == JSOp::FunApply) {
        JitSpew(JitSpew_BaselineBailouts,
                "      pushing 4x undefined to fixup funapply");
        if (!builder.writeValue(UndefinedValue(), "StackValue")) {
          return false;
        }
        if (!builder.writeValue(UndefinedValue(), "StackValue")) {
          return false;
        }
        if (!builder.writeValue(UndefinedValue(), "StackValue")) {
          return false;
        }
        if (!builder.writeValue(UndefinedValue(), "StackValue")) {
          return false;
        }
      }
      // Save the actual arguments. They are needed on the callee side
      // as the arguments. Else we can't recover them.
      if (!savedCallerArgs.resize(inlined_args)) {
        return false;
      }
      for (uint32_t i = 0; i < inlined_args; i++) {
        savedCallerArgs[i].set(iter.read());
      }

      if (IsSetPropOp(op)) {
        // We would love to just save all the arguments and leave them
        // in the stub frame pushed below, but we will lose the inital
        // argument which the function was called with, which we must
        // leave on the stack. It's pushed as the result of the SetProp.
        Value initialArg = savedCallerArgs[inlined_args - 1];
        JitSpew(JitSpew_BaselineBailouts,
                "     pushing setter's initial argument");
        if (!builder.writeValue(initialArg, "StackValue")) {
          return false;
        }
      }
      pushedSlots = exprStackSlots;
    }
  }

  JitSpew(JitSpew_BaselineBailouts, "      pushing %u expression stack slots",
          exprStackSlots - pushedSlots);
  for (uint32_t i = pushedSlots; i < exprStackSlots; i++) {
    Value v;

    if (!iter.moreFrames() && i == exprStackSlots - 1 &&
        cx->hasIonReturnOverride()) {
      // If coming from an invalidation bailout, and this is the topmost
      // value, and a value override has been specified, don't read from the
      // iterator. Otherwise, we risk using a garbage value.
      MOZ_ASSERT(invalidate);
      iter.skip();
      JitSpew(JitSpew_BaselineBailouts, "      [Return Override]");
      v = cx->takeIonReturnOverride();
    } else if (excInfo && excInfo->propagatingIonExceptionForDebugMode()) {
      // If we are in the middle of propagating an exception from Ion by
      // bailing to baseline due to debug mode, we might not have all
      // the stack if we are at the newest frame.
      //
      // For instance, if calling |f()| pushed an Ion frame which threw,
      // the snapshot expects the return value to be pushed, but it's
      // possible nothing was pushed before we threw. We can't drop
      // iterators, however, so read them out. They will be closed by
      // HandleExceptionBaseline.
      MOZ_ASSERT(cx->realm()->isDebuggee());
      if (iter.moreFrames() ||
          HasLiveStackValueAtDepth(script, pc, i, exprStackSlots)) {
        v = iter.read();
      } else {
        iter.skip();
        v = MagicValue(JS_OPTIMIZED_OUT);
      }
    } else {
      v = iter.read();
    }
    if (!builder.writeValue(v, "StackValue")) {
      return false;
    }
  }

  // BaselineFrame::frameSize is the size of everything pushed since
  // the builder.resetFramePushed() call.
  const uint32_t frameSize = builder.framePushed();
#ifdef DEBUG
  blFrame->setDebugFrameSize(frameSize);
#endif
  JitSpew(JitSpew_BaselineBailouts, "      FrameSize=%u", frameSize);

  // debugNumValueSlots() is based on the frame size, do some sanity checks.
  MOZ_ASSERT(blFrame->debugNumValueSlots() >= script->nfixed());
  MOZ_ASSERT(blFrame->debugNumValueSlots() <= script->nslots());

  const uint32_t pcOff = script->pcToOffset(pc);
  JitScript* jitScript = script->jitScript();

#ifdef DEBUG
  uint32_t expectedDepth;
  bool reachablePC;
  if (!ReconstructStackDepth(cx, script, resumeAfter ? GetNextPc(pc) : pc,
                             &expectedDepth, &reachablePC)) {
    return false;
  }

  if (reachablePC) {
    if (op != JSOp::FunApply || !iter.moreFrames() || resumeAfter) {
      if (op == JSOp::FunCall) {
        // For fun.call(this, ...); the reconstructStackDepth will
        // include the this. When inlining that is not included.
        // So the exprStackSlots will be one less.
        MOZ_ASSERT(expectedDepth - exprStackSlots <= 1);
      } else if (iter.moreFrames() && IsIonInlinableGetterOrSetterOp(op)) {
        // Accessors coming out of ion are inlined via a complete
        // lie perpetrated by the compiler internally. Ion just rearranges
        // the stack, and pretends that it looked like a call all along.
        // This means that the depth is actually one *more* than expected
        // by the interpreter, as there is now a JSFunction, |this| and [arg],
        // rather than the expected |this| and [arg].
        // If the inlined accessor is a getelem operation, the numbers do match,
        // but that's just because getelem expects one more item on the stack.
        // Note that none of that was pushed, but it's still reflected
        // in exprStackSlots.
        MOZ_ASSERT(exprStackSlots - expectedDepth == (IsGetElemOp(op) ? 0 : 1));
      } else {
        // For fun.apply({}, arguments) the reconstructStackDepth will
        // have stackdepth 4, but it could be that we inlined the
        // funapply. In that case exprStackSlots, will have the real
        // arguments in the slots and not be 4.
        MOZ_ASSERT(exprStackSlots == expectedDepth);
      }
    }
  }
#endif

#ifdef JS_JITSPEW
  JitSpew(JitSpew_BaselineBailouts,
          "      Resuming %s pc offset %d (op %s) (line %d) of %s:%u:%u",
          resumeAfter ? "after" : "at", (int)pcOff, CodeName(op),
          PCToLineNumber(script, pc), script->filename(), script->lineno(),
          script->column());
  JitSpew(JitSpew_BaselineBailouts, "      Bailout kind: %s",
          BailoutKindString(bailoutKind));
#endif

  const BaselineInterpreter& baselineInterp =
      cx->runtime()->jitRuntime()->baselineInterpreter();

  // If this was the last inline frame, or we are bailing out to a catch or
  // finally block in this frame, then unpacking is almost done.
  if (!iter.moreFrames() || catchingException) {
    builder.setResumeFramePtr(prevFramePtr);
    builder.setFrameSizeOfInnerMostFrame(frameSize);

    // Compute the native address (within the Baseline Interpreter) that we will
    // resume at and initialize the frame's interpreter fields.
    uint8_t* resumeAddr;
    if (IsPrologueBailout(iter, excInfo)) {
      JitSpew(JitSpew_BaselineBailouts, "      Resuming into prologue.");
      MOZ_ASSERT(pc == script->code());
      blFrame->setInterpreterFieldsForPrologue(script);
      resumeAddr = baselineInterp.bailoutPrologueEntryAddr();
    } else if (excInfo && excInfo->propagatingIonExceptionForDebugMode()) {
      // When propagating an exception for debug mode, set the
      // resume pc to the throwing pc, so that Debugger hooks report
      // the correct pc offset of the throwing op instead of its
      // successor.
      jsbytecode* throwPC = script->offsetToPC(iter.pcOffset());
      blFrame->setInterpreterFields(script, throwPC);
      resumeAddr = baselineInterp.interpretOpAddr().value;
    } else {
      // If the opcode is monitored we should monitor the top stack value when
      // we finish the bailout in FinishBailoutToBaseline.
      if (resumeAfter && BytecodeOpHasTypeSet(op)) {
        builder.setMonitorPC(pc);
      }
      jsbytecode* resumePC = GetResumePC(script, pc, resumeAfter);
      blFrame->setInterpreterFields(script, resumePC);
      resumeAddr = baselineInterp.interpretOpAddr().value;
    }
    builder.setResumeAddr(resumeAddr);
    JitSpew(JitSpew_BaselineBailouts, "      Set resumeAddr=%p", resumeAddr);

    if (cx->runtime()->geckoProfiler().enabled()) {
      // Register bailout with profiler.
      const char* filename = script->filename();
      if (filename == nullptr) {
        filename = "<unknown>";
      }
      unsigned len = strlen(filename) + 200;
      UniqueChars buf(js_pod_malloc<char>(len));
      if (buf == nullptr) {
        ReportOutOfMemory(cx);
        return false;
      }
      snprintf(buf.get(), len, "%s %s %s on line %u of %s:%u",
               BailoutKindString(bailoutKind), resumeAfter ? "after" : "at",
               CodeName(op), PCToLineNumber(script, pc), filename,
               script->lineno());
      cx->runtime()->geckoProfiler().markEvent(buf.get());
    }

    return true;
  }

  // This is an outer frame for an inlined getter/setter/call.

  blFrame->setInterpreterFields(script, pc);

  // Write out descriptor of BaselineJS frame.
  size_t baselineFrameDescr = MakeFrameDescriptor(
      (uint32_t)builder.framePushed(), FrameType::BaselineJS,
      BaselineStubFrameLayout::Size());
  if (!builder.writeWord(baselineFrameDescr, "Descriptor")) {
    return false;
  }

  // Calculate and write out return address.
  // The icEntry in question MUST have an inlinable fallback stub.
  ICEntry& icEntry = jitScript->icEntryFromPCOffset(pcOff);
  MOZ_ASSERT(IsInlinableFallback(icEntry.fallbackStub()));

  uint8_t* retAddr = baselineInterp.retAddrForIC(JSOp(*pc));
  if (!builder.writePtr(retAddr, "ReturnAddr")) {
    return false;
  }

  // Build baseline stub frame:
  // +===============+
  // |    StubPtr    |
  // +---------------+
  // |   FramePtr    |
  // +---------------+
  // |   Padding?    |
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

  JitSpew(JitSpew_BaselineBailouts, "      [BASELINE-STUB FRAME]");

  size_t startOfBaselineStubFrame = builder.framePushed();

  // Write stub pointer.
  MOZ_ASSERT(IsInlinableFallback(icEntry.fallbackStub()));
  if (!builder.writePtr(icEntry.fallbackStub(), "StubPtr")) {
    return false;
  }

  // Write previous frame pointer (saved earlier).
  if (!builder.writePtr(prevFramePtr, "PrevFramePtr")) {
    return false;
  }
  prevFramePtr = builder.virtualPointerAtStackOffset(0);

  // Write out actual arguments (and thisv), copied from unpacked stack of
  // BaselineJS frame. Arguments are reversed on the BaselineJS frame's stack
  // values.
  MOZ_ASSERT(IsIonInlinableOp(op));
  bool pushedNewTarget = IsConstructPC(pc);
  unsigned actualArgc;
  Value callee;
  if (needToSaveArgs) {
    // For FunApply or an accessor, the arguments are not on the stack anymore,
    // but they are copied in a vector and are written here.
    if (op == JSOp::FunApply) {
      actualArgc = blFrame->numActualArgs();
    } else {
      actualArgc = IsSetPropOp(op);
    }
    callee = savedCallerArgs[0];

    // Align the stack based on the number of arguments.
    size_t afterFrameSize =
        (actualArgc + 1) * sizeof(Value) + JitFrameLayout::Size();
    if (!builder.maybeWritePadding(JitStackAlignment, afterFrameSize,
                                   "Padding")) {
      return false;
    }

    // Push arguments.
    MOZ_ASSERT(actualArgc + 2 <= exprStackSlots);
    MOZ_ASSERT(savedCallerArgs.length() == actualArgc + 2);
    for (unsigned i = 0; i < actualArgc + 1; i++) {
      size_t arg = savedCallerArgs.length() - (i + 1);
      if (!builder.writeValue(savedCallerArgs[arg], "ArgVal")) {
        return false;
      }
    }
  } else {
    actualArgc = GET_ARGC(pc);
    if (op == JSOp::FunCall) {
      MOZ_ASSERT(actualArgc > 0);
      actualArgc--;
    }

    // Align the stack based on the number of arguments.
    size_t afterFrameSize = (actualArgc + 1 + pushedNewTarget) * sizeof(Value) +
                            JitFrameLayout::Size();
    if (!builder.maybeWritePadding(JitStackAlignment, afterFrameSize,
                                   "Padding")) {
      return false;
    }

    // Copy the arguments and |this| from the BaselineFrame, in reverse order.
    size_t valueSlot = blFrame->numValueSlots(frameSize) - 1;
    size_t calleeSlot = valueSlot - actualArgc - 1 - pushedNewTarget;

    for (size_t i = valueSlot; i > calleeSlot; i--) {
      Value v = *blFrame->valueSlot(i);
      if (!builder.writeValue(v, "ArgVal")) {
        return false;
      }
    }

    callee = *blFrame->valueSlot(calleeSlot);
  }

  // In case these arguments need to be copied on the stack again for a
  // rectifier frame, save the framePushed values here for later use.
  size_t endOfBaselineStubArgs = builder.framePushed();

  // Calculate frame size for descriptor.
  size_t baselineStubFrameSize =
      builder.framePushed() - startOfBaselineStubFrame;
  size_t baselineStubFrameDescr =
      MakeFrameDescriptor((uint32_t)baselineStubFrameSize,
                          FrameType::BaselineStub, JitFrameLayout::Size());

  // Push actual argc
  if (!builder.writeWord(actualArgc, "ActualArgc")) {
    return false;
  }

  // Push callee token (must be a JS Function)
  JitSpew(JitSpew_BaselineBailouts, "      Callee = %016" PRIx64,
          callee.asRawBits());

  JSFunction* calleeFun = &callee.toObject().as<JSFunction>();
  if (!builder.writePtr(CalleeToToken(calleeFun, pushedNewTarget),
                        "CalleeToken")) {
    return false;
  }
  nextCallee.set(calleeFun);

  // Push BaselineStub frame descriptor
  if (!builder.writeWord(baselineStubFrameDescr, "Descriptor")) {
    return false;
  }

  // Ensure we have a TypeMonitor fallback stub so we don't crash in JIT code
  // when we try to enter it. See callers of offsetOfFallbackMonitorStub.
  if (BytecodeOpHasTypeSet(JSOp(*pc))) {
    ICFallbackStub* fallbackStub = icEntry.fallbackStub();
    if (!fallbackStub->toMonitoredFallbackStub()->getFallbackMonitorStub(
            cx, script)) {
      return false;
    }
  }

  // Push return address into ICCall_Scripted stub, immediately after the call.
  void* baselineCallReturnAddr = GetStubReturnAddress(cx, op);
  MOZ_ASSERT(baselineCallReturnAddr);
  if (!builder.writePtr(baselineCallReturnAddr, "ReturnAddr")) {
    return false;
  }
  MOZ_ASSERT(builder.framePushed() % JitStackAlignment == 0);

  // If actualArgc >= fun->nargs, then we are done.  Otherwise, we need to push
  // on a reconstructed rectifier frame.
  if (actualArgc >= calleeFun->nargs()) {
    return true;
  }

  // Push a reconstructed rectifier frame.
  // +===============+
  // |   Padding?    |
  // +---------------+
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

  JitSpew(JitSpew_BaselineBailouts, "      [RECTIFIER FRAME]");

  size_t startOfRectifierFrame = builder.framePushed();

  // On x86-only, the frame pointer is saved again in the rectifier frame.
#if defined(JS_CODEGEN_X86)
  if (!builder.writePtr(prevFramePtr, "PrevFramePtr-X86Only")) {
    return false;
  }
  // Follow the same logic as in JitRuntime::generateArgumentsRectifier.
  prevFramePtr = builder.virtualPointerAtStackOffset(0);
  if (!builder.writePtr(prevFramePtr, "Padding-X86Only")) {
    return false;
  }
#endif

  // Align the stack based on the number of arguments.
  size_t afterFrameSize =
      (calleeFun->nargs() + 1 + pushedNewTarget) * sizeof(Value) +
      RectifierFrameLayout::Size();
  if (!builder.maybeWritePadding(JitStackAlignment, afterFrameSize,
                                 "Padding")) {
    return false;
  }

  // Copy new.target, if necessary.
  if (pushedNewTarget) {
    size_t newTargetOffset = (builder.framePushed() - endOfBaselineStubArgs) +
                             (actualArgc + 1) * sizeof(Value);
    Value newTargetValue = *builder.valuePointerAtStackOffset(newTargetOffset);
    if (!builder.writeValue(newTargetValue, "CopiedNewTarget")) {
      return false;
    }
  }

  // Push undefined for missing arguments.
  for (unsigned i = 0; i < (calleeFun->nargs() - actualArgc); i++) {
    if (!builder.writeValue(UndefinedValue(), "FillerVal")) {
      return false;
    }
  }

  // Copy arguments + thisv from BaselineStub frame.
  if (!builder.subtract((actualArgc + 1) * sizeof(Value), "CopiedArgs")) {
    return false;
  }
  BufferPointer<uint8_t> stubArgsEnd = builder.pointerAtStackOffset<uint8_t>(
      builder.framePushed() - endOfBaselineStubArgs);
  JitSpew(JitSpew_BaselineBailouts, "      MemCpy from %p", stubArgsEnd.get());
  memcpy(builder.pointerAtStackOffset<uint8_t>(0).get(), stubArgsEnd.get(),
         (actualArgc + 1) * sizeof(Value));

  // Calculate frame size for descriptor.
  size_t rectifierFrameSize = builder.framePushed() - startOfRectifierFrame;
  size_t rectifierFrameDescr =
      MakeFrameDescriptor((uint32_t)rectifierFrameSize, FrameType::Rectifier,
                          JitFrameLayout::Size());

  // Push actualArgc
  if (!builder.writeWord(actualArgc, "ActualArgc")) {
    return false;
  }

  // Push calleeToken again.
  if (!builder.writePtr(CalleeToToken(calleeFun, pushedNewTarget),
                        "CalleeToken")) {
    return false;
  }

  // Push rectifier frame descriptor
  if (!builder.writeWord(rectifierFrameDescr, "Descriptor")) {
    return false;
  }

  // Push return address into the ArgumentsRectifier code, immediately after the
  // ioncode call.
  void* rectReturnAddr =
      cx->runtime()->jitRuntime()->getArgumentsRectifierReturnAddr().value;
  MOZ_ASSERT(rectReturnAddr);
  if (!builder.writePtr(rectReturnAddr, "ReturnAddr")) {
    return false;
  }
  MOZ_ASSERT(builder.framePushed() % JitStackAlignment == 0);

  return true;
}

bool jit::BailoutIonToBaseline(JSContext* cx, JitActivation* activation,
                               const JSJitFrameIter& iter, bool invalidate,
                               BaselineBailoutInfo** bailoutInfo,
                               const ExceptionBailoutInfo* excInfo) {
  MOZ_ASSERT(bailoutInfo != nullptr);
  MOZ_ASSERT(*bailoutInfo == nullptr);
  MOZ_ASSERT(iter.isBailoutJS());

  TraceLoggerThread* logger = TraceLoggerForCurrentThread(cx);
  TraceLogStopEvent(logger, TraceLogger_IonMonkey);
  TraceLogStartEvent(logger, TraceLogger_Baseline);

  // Ion bailout can fail due to overrecursion and OOM. In such cases we
  // cannot honor any further Debugger hooks on the frame, and need to
  // ensure that its Debugger.Frame entry is cleaned up.
  auto guardRemoveRematerializedFramesFromDebugger =
      mozilla::MakeScopeExit([&] {
        activation->removeRematerializedFramesFromDebugger(cx, iter.fp());
      });

  // Always remove the RInstructionResults from the JitActivation, even in
  // case of failures as the stack frame is going away after the bailout.
  auto removeIonFrameRecovery = mozilla::MakeScopeExit(
      [&] { activation->removeIonFrameRecovery(iter.jsFrame()); });

  // The caller of the top frame must be one of the following:
  //      IonJS - Ion calling into Ion.
  //      BaselineStub - Baseline calling into Ion.
  //      Entry / WasmToJSJit - Interpreter or other (wasm) calling into Ion.
  //      Rectifier - Arguments rectifier calling into Ion.
  MOZ_ASSERT(iter.isBailoutJS());
#if defined(DEBUG) || defined(JS_JITSPEW)
  FrameType prevFrameType = iter.prevType();
  MOZ_ASSERT(JSJitFrameIter::isEntry(prevFrameType) ||
             prevFrameType == FrameType::IonJS ||
             prevFrameType == FrameType::BaselineStub ||
             prevFrameType == FrameType::Rectifier ||
             prevFrameType == FrameType::IonICCall);
#endif

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

  JitSpew(JitSpew_BaselineBailouts,
          "Bailing to baseline %s:%u:%u (IonScript=%p) (FrameType=%d)",
          iter.script()->filename(), iter.script()->lineno(),
          iter.script()->column(), (void*)iter.ionScript(), (int)prevFrameType);

  bool catchingException;
  bool propagatingExceptionForDebugMode;
  if (excInfo) {
    catchingException = excInfo->catchingException();
    propagatingExceptionForDebugMode =
        excInfo->propagatingIonExceptionForDebugMode();

    if (catchingException) {
      JitSpew(JitSpew_BaselineBailouts, "Resuming in catch or finally block");
    }

    if (propagatingExceptionForDebugMode) {
      JitSpew(JitSpew_BaselineBailouts, "Resuming in-place for debug mode");
    }
  } else {
    catchingException = false;
    propagatingExceptionForDebugMode = false;
  }

  JitSpew(JitSpew_BaselineBailouts,
          "  Reading from snapshot offset %u size %zu", iter.snapshotOffset(),
          iter.ionScript()->snapshotsListSize());

  if (!excInfo) {
    iter.ionScript()->incNumBailouts();
  }
  iter.script()->updateJitCodeRaw(cx->runtime());

  // Allocate buffer to hold stack replacement data.
  BaselineStackBuilder builder(cx, iter, 1024);
  if (!builder.init()) {
    return false;
  }
  JitSpew(JitSpew_BaselineBailouts, "  Incoming frame ptr = %p",
          builder.startFrame());

  // Under a bailout, there is no need to invalidate the frame after
  // evaluating the recover instruction, as the invalidation is only needed in
  // cases where the frame is introspected ahead of the bailout.
  MaybeReadFallback recoverBailout(cx, activation, &iter,
                                   MaybeReadFallback::Fallback_DoNothing);

  // Ensure that all value locations are readable from the SnapshotIterator.
  // Get the RInstructionResults from the JitActivation if the frame got
  // recovered ahead of the bailout.
  SnapshotIterator snapIter(iter, activation->bailoutData()->machineState());
  if (!snapIter.initInstructionResults(recoverBailout)) {
    ReportOutOfMemory(cx);
    return false;
  }

#ifdef TRACK_SNAPSHOTS
  snapIter.spewBailingFrom();
#endif

  RootedFunction callee(cx, iter.maybeCallee());
  RootedScript scr(cx, iter.script());
  if (callee) {
    JitSpew(JitSpew_BaselineBailouts, "  Callee function (%s:%u:%u)",
            scr->filename(), scr->lineno(), scr->column());
  } else {
    JitSpew(JitSpew_BaselineBailouts, "  No callee!");
  }

  if (iter.isConstructing()) {
    JitSpew(JitSpew_BaselineBailouts, "  Constructing!");
  } else {
    JitSpew(JitSpew_BaselineBailouts, "  Not constructing!");
  }

  JitSpew(JitSpew_BaselineBailouts, "  Restoring frames:");
  size_t frameNo = 0;

  // Reconstruct baseline frames using the builder.
  RootedFunction fun(cx, callee);
  Rooted<GCVector<Value>> startFrameFormals(cx, GCVector<Value>(cx));

  gc::AutoSuppressGC suppress(cx);

  while (true) {
    // Skip recover instructions as they are already recovered by
    // |initInstructionResults|.
    snapIter.settleOnFrame();

    if (frameNo > 0) {
      // TraceLogger doesn't create entries for inlined frames. But we
      // see them in Baseline. Here we create the start events of those
      // entries. So they correspond to what we will see in Baseline.
      TraceLoggerEvent scriptEvent(TraceLogger_Scripts, scr);
      TraceLogStartEvent(logger, scriptEvent);
      TraceLogStartEvent(logger, TraceLogger_Baseline);
    }

    JitSpew(JitSpew_BaselineBailouts, "    FrameNo %zu", frameNo);

    // If we are bailing out to a catch or finally block in this frame,
    // pass excInfo to InitFromBailout and don't unpack any other frames.
    bool handleException = (catchingException && excInfo->frameNo() == frameNo);

    // We also need to pass excInfo if we're bailing out in place for
    // debug mode.
    bool passExcInfo = handleException || propagatingExceptionForDebugMode;

    RootedFunction nextCallee(cx, nullptr);
    if (!InitFromBailout(cx, frameNo, fun, scr, snapIter, invalidate, builder,
                         &startFrameFormals, &nextCallee,
                         passExcInfo ? excInfo : nullptr)) {
      MOZ_ASSERT(cx->isExceptionPending());
      return false;
    }

    if (!snapIter.moreFrames()) {
      MOZ_ASSERT(!nextCallee);
      break;
    }

    if (handleException) {
      break;
    }

    MOZ_ASSERT(nextCallee);
    fun = nextCallee;
    scr = fun->nonLazyScript();

    frameNo++;

    snapIter.nextInstruction();
  }
  JitSpew(JitSpew_BaselineBailouts, "  Done restoring frames");

  BailoutKind bailoutKind = snapIter.bailoutKind();

  if (!startFrameFormals.empty()) {
    // Set the first frame's formals, see the comment in InitFromBailout.
    Value* argv = builder.startFrame()->argv() + 1;  // +1 to skip |this|.
    mozilla::PodCopy(argv, startFrameFormals.begin(),
                     startFrameFormals.length());
  }

  // Do stack check.
  bool overRecursed = false;
  BaselineBailoutInfo* info = builder.info();
  uint8_t* newsp =
      info->incomingStack - (info->copyStackTop - info->copyStackBottom);
#ifdef JS_SIMULATOR
  if (Simulator::Current()->overRecursed(uintptr_t(newsp))) {
    overRecursed = true;
  }
#else
  if (!CheckRecursionLimitWithStackPointerDontReport(cx, newsp)) {
    overRecursed = true;
  }
#endif
  if (overRecursed) {
    JitSpew(JitSpew_BaselineBailouts, "  Overrecursion check failed!");
    ReportOverRecursed(cx);
    return false;
  }

  // Take the reconstructed baseline stack so it doesn't get freed when builder
  // destructs.
  info = builder.takeBuffer();
  info->numFrames = frameNo + 1;
  info->bailoutKind.emplace(bailoutKind);
  *bailoutInfo = info;
  guardRemoveRematerializedFramesFromDebugger.release();
  return true;
}

static void InvalidateAfterBailout(JSContext* cx, HandleScript outerScript,
                                   const char* reason) {
  // In some cases, the computation of recover instruction can invalidate the
  // Ion script before we reach the end of the bailout. Thus, if the outer
  // script no longer have any Ion script attached, then we just skip the
  // invalidation.
  //
  // For example, such case can happen if the template object for an unboxed
  // objects no longer match the content of its properties (see Bug 1174547)
  if (!outerScript->hasIonScript()) {
    JitSpew(JitSpew_BaselineBailouts, "Ion script is already invalidated");
    return;
  }

  MOZ_ASSERT(!outerScript->ionScript()->invalidated());

  JitSpew(JitSpew_BaselineBailouts, "Invalidating due to %s", reason);
  Invalidate(cx, outerScript);
}

static void HandleBoundsCheckFailure(JSContext* cx, HandleScript outerScript,
                                     HandleScript innerScript) {
  JitSpew(JitSpew_IonBailouts,
          "Bounds check failure %s:%u:%u, inlined into %s:%u:%u",
          innerScript->filename(), innerScript->lineno(), innerScript->column(),
          outerScript->filename(), outerScript->lineno(),
          outerScript->column());

  if (!innerScript->failedBoundsCheck()) {
    innerScript->setFailedBoundsCheck();
  }

  InvalidateAfterBailout(cx, outerScript, "bounds check failure");
  if (innerScript->hasIonScript()) {
    Invalidate(cx, innerScript);
  }
}

static void HandleShapeGuardFailure(JSContext* cx, HandleScript outerScript,
                                    HandleScript innerScript) {
  JitSpew(JitSpew_IonBailouts,
          "Shape guard failure %s:%u:%u, inlined into %s:%u:%u",
          innerScript->filename(), innerScript->lineno(), innerScript->column(),
          outerScript->filename(), outerScript->lineno(),
          outerScript->column());

  // TODO: Currently this mimic's Ion's handling of this case.  Investigate
  // setting the flag on innerScript as opposed to outerScript, and maybe
  // invalidating both inner and outer scripts, instead of just the outer one.
  outerScript->setFailedShapeGuard();

  InvalidateAfterBailout(cx, outerScript, "shape guard failure");
}

static void HandleBaselineInfoBailout(JSContext* cx, HandleScript outerScript,
                                      HandleScript innerScript) {
  JitSpew(JitSpew_IonBailouts,
          "Baseline info failure %s:%u:%u, inlined into %s:%u:%u",
          innerScript->filename(), innerScript->lineno(), innerScript->column(),
          outerScript->filename(), outerScript->lineno(),
          outerScript->column());

  InvalidateAfterBailout(cx, outerScript, "invalid baseline info");
}

static void HandleLexicalCheckFailure(JSContext* cx, HandleScript outerScript,
                                      HandleScript innerScript) {
  JitSpew(JitSpew_IonBailouts,
          "Lexical check failure %s:%u:%u, inlined into %s:%u:%u",
          innerScript->filename(), innerScript->lineno(), innerScript->column(),
          outerScript->filename(), outerScript->lineno(),
          outerScript->column());

  if (!innerScript->failedLexicalCheck()) {
    innerScript->setFailedLexicalCheck();
  }

  InvalidateAfterBailout(cx, outerScript, "lexical check failure");
  if (innerScript->hasIonScript()) {
    Invalidate(cx, innerScript);
  }
}

static bool CopyFromRematerializedFrame(JSContext* cx, JitActivation* act,
                                        uint8_t* fp, size_t inlineDepth,
                                        BaselineFrame* frame) {
  RematerializedFrame* rematFrame =
      act->lookupRematerializedFrame(fp, inlineDepth);

  // We might not have rematerialized a frame if the user never requested a
  // Debugger.Frame for it.
  if (!rematFrame) {
    return true;
  }

  MOZ_ASSERT(rematFrame->script() == frame->script());
  MOZ_ASSERT(rematFrame->numActualArgs() == frame->numActualArgs());

  frame->setEnvironmentChain(rematFrame->environmentChain());

  if (frame->isFunctionFrame()) {
    frame->thisArgument() = rematFrame->thisArgument();
  }

  for (unsigned i = 0; i < frame->numActualArgs(); i++) {
    frame->argv()[i] = rematFrame->argv()[i];
  }

  for (size_t i = 0; i < frame->script()->nfixed(); i++) {
    *frame->valueSlot(i) = rematFrame->locals()[i];
  }

  frame->setReturnValue(rematFrame->returnValue());

  // Don't copy over the hasCachedSavedFrame bit. The new BaselineFrame we're
  // building has a different AbstractFramePtr, so it won't be found in the
  // LiveSavedFrameCache if we look there.

  JitSpew(JitSpew_BaselineBailouts,
          "  Copied from rematerialized frame at (%p,%zu)", fp, inlineDepth);

  // Propagate the debuggee frame flag. For the case where the Debugger did
  // not rematerialize an Ion frame, the baseline frame has its debuggee
  // flag set iff its script is considered a debuggee. See the debuggee case
  // in InitFromBailout.
  if (rematFrame->isDebuggee()) {
    frame->setIsDebuggee();
    return DebugAPI::handleIonBailout(cx, rematFrame, frame);
  }

  return true;
}

bool jit::FinishBailoutToBaseline(BaselineBailoutInfo* bailoutInfoArg) {
  JitSpew(JitSpew_BaselineBailouts, "  Done restoring frames");

  // Use UniquePtr to free the bailoutInfo before we return.
  UniquePtr<BaselineBailoutInfo> bailoutInfo(bailoutInfoArg);
  bailoutInfoArg = nullptr;

  JSContext* cx = TlsContext.get();
  BaselineFrame* topFrame = GetTopBaselineFrame(cx);

  // We have to get rid of the rematerialized frame, whether it is
  // restored or unwound.
  uint8_t* incomingStack = bailoutInfo->incomingStack;
  auto guardRemoveRematerializedFramesFromDebugger =
      mozilla::MakeScopeExit([&] {
        JitActivation* act = cx->activation()->asJit();
        act->removeRematerializedFramesFromDebugger(cx, incomingStack);
      });

  // Ensure the frame has a call object if it needs one.
  if (!EnsureHasEnvironmentObjects(cx, topFrame)) {
    return false;
  }

  // Monitor the top stack value if we are resuming after a JOF_TYPESET op.
  if (jsbytecode* monitorPC = bailoutInfo->monitorPC) {
    MOZ_ASSERT(BytecodeOpHasTypeSet(JSOp(*monitorPC)));
    MOZ_ASSERT(GetNextPc(monitorPC) == topFrame->interpreterPC());

    RootedScript script(cx, topFrame->script());
    uint32_t monitorOffset = script->pcToOffset(monitorPC);
    ICEntry& icEntry = script->jitScript()->icEntryFromPCOffset(monitorOffset);
    ICFallbackStub* fallbackStub = icEntry.fallbackStub();

    // Not every monitored op has a monitored fallback stub, e.g.
    // JSOp::NewObject, which always returns the same type for a
    // particular script/pc location.
    if (fallbackStub->isMonitoredFallback()) {
      ICMonitoredFallbackStub* stub = fallbackStub->toMonitoredFallbackStub();
      uint32_t frameSize = bailoutInfo->frameSizeOfInnerMostFrame;
      RootedValue val(cx, topFrame->topStackValue(frameSize));
      if (!TypeMonitorResult(cx, stub, topFrame, script, monitorPC, val)) {
        return false;
      }
    }
  }

  // Create arguments objects for bailed out frames, to maintain the invariant
  // that script->needsArgsObj() implies frame->hasArgsObj().
  RootedScript innerScript(cx, nullptr);
  RootedScript outerScript(cx, nullptr);

  MOZ_ASSERT(cx->currentlyRunningInJit());
  JSJitFrameIter iter(cx->activation()->asJit());
  uint8_t* outerFp = nullptr;

  // Iter currently points at the exit frame.  Get the previous frame
  // (which must be a baseline frame), and set it as the last profiling
  // frame.
  if (cx->runtime()->jitRuntime()->isProfilerInstrumentationEnabled(
          cx->runtime())) {
    cx->jitActivation->setLastProfilingFrame(iter.prevFp());
  }

  uint32_t numFrames = bailoutInfo->numFrames;
  MOZ_ASSERT(numFrames > 0);

  uint32_t frameno = 0;
  while (frameno < numFrames) {
    MOZ_ASSERT(!iter.isIonJS());

    if (iter.isBaselineJS()) {
      BaselineFrame* frame = iter.baselineFrame();
      MOZ_ASSERT(frame->script()->hasBaselineScript());

      // If the frame doesn't even have a env chain set yet, then it's resuming
      // into the the prologue before the env chain is initialized.  Any
      // necessary args object will also be initialized there.
      if (frame->environmentChain() && frame->script()->needsArgsObj()) {
        ArgumentsObject* argsObj;
        if (frame->hasArgsObj()) {
          argsObj = &frame->argsObj();
        } else {
          argsObj = ArgumentsObject::createExpected(cx, frame);
          if (!argsObj) {
            return false;
          }
        }

        // The arguments is a local binding and needsArgsObj does not
        // check if it is clobbered. Ensure that the local binding
        // restored during bailout before storing the arguments object
        // to the slot.
        RootedScript script(cx, frame->script());
        SetFrameArgumentsObject(cx, frame, script, argsObj);
      }

      if (frameno == 0) {
        innerScript = frame->script();
      }

      if (frameno == numFrames - 1) {
        outerScript = frame->script();
        outerFp = iter.fp();
        MOZ_ASSERT(outerFp == incomingStack);
      }

      frameno++;
    }

    ++iter;
  }

  MOZ_ASSERT(innerScript);
  MOZ_ASSERT(outerScript);
  MOZ_ASSERT(outerFp);

  // If we rematerialized Ion frames due to debug mode toggling, copy their
  // values into the baseline frame. We need to do this even when debug mode
  // is off, as we should respect the mutations made while debug mode was
  // on.
  JitActivation* act = cx->activation()->asJit();
  if (act->hasRematerializedFrame(outerFp)) {
    JSJitFrameIter iter(act);
    size_t inlineDepth = numFrames;
    bool ok = true;
    while (inlineDepth > 0) {
      if (iter.isBaselineJS()) {
        // We must attempt to copy all rematerialized frames over,
        // even if earlier ones failed, to invoke the proper frame
        // cleanup in the Debugger.
        if (!CopyFromRematerializedFrame(cx, act, outerFp, --inlineDepth,
                                         iter.baselineFrame())) {
          ok = false;
        }
      }
      ++iter;
    }

    if (!ok) {
      return false;
    }

    // After copying from all the rematerialized frames, remove them from
    // the table to keep the table up to date.
    guardRemoveRematerializedFramesFromDebugger.release();
    act->removeRematerializedFrame(outerFp);
  }

  // If we are catching an exception, we need to unwind scopes.
  // See |SettleOnTryNote|
  if (cx->isExceptionPending() && bailoutInfo->faultPC) {
    EnvironmentIter ei(cx, topFrame, bailoutInfo->faultPC);
    UnwindEnvironment(cx, ei, bailoutInfo->tryPC);
  }

  // Check for interrupts now because we might miss an interrupt check in JIT
  // code when resuming in the prologue, after the stack/interrupt check.
  if (!cx->isExceptionPending()) {
    if (!CheckForInterrupt(cx)) {
      return false;
    }
  }

  BailoutKind bailoutKind = *bailoutInfo->bailoutKind;
  JitSpew(JitSpew_BaselineBailouts,
          "  Restored outerScript=(%s:%u:%u,%u) innerScript=(%s:%u:%u,%u) "
          "(bailoutKind=%u)",
          outerScript->filename(), outerScript->lineno(), outerScript->column(),
          outerScript->getWarmUpCount(), innerScript->filename(),
          innerScript->lineno(), innerScript->column(),
          innerScript->getWarmUpCount(), (unsigned)bailoutKind);

  switch (bailoutKind) {
    // Normal bailouts.
    case Bailout_Inevitable:
    case Bailout_DuringVMCall:
    case Bailout_TooManyArguments:
    case Bailout_DynamicNameNotFound:
    case Bailout_StringArgumentsEval:
    case Bailout_Overflow:
    case Bailout_Round:
    case Bailout_NonPrimitiveInput:
    case Bailout_PrecisionLoss:
    case Bailout_TypeBarrierO:
    case Bailout_TypeBarrierV:
    case Bailout_MonitorTypes:
    case Bailout_Hole:
    case Bailout_NegativeIndex:
    case Bailout_NonInt32Input:
    case Bailout_NonNumericInput:
    case Bailout_NonBooleanInput:
    case Bailout_NonObjectInput:
    case Bailout_NonStringInput:
    case Bailout_NonSymbolInput:
    case Bailout_NonBigIntInput:
    case Bailout_NonSharedTypedArrayInput:
    case Bailout_Debugger:
      // Do nothing.
      break;

    case Bailout_FirstExecution:
      // Do not return directly, as this was not frequent in the first place,
      // thus rely on the check for frequent bailouts to recompile the current
      // script.
      break;

    // Invalid assumption based on baseline code.
    case Bailout_OverflowInvalidate:
      outerScript->setHadOverflowBailout();
      [[fallthrough]];
    case Bailout_DoubleOutput:
    case Bailout_ObjectIdentityOrTypeGuard:
      HandleBaselineInfoBailout(cx, outerScript, innerScript);
      break;

    case Bailout_ArgumentCheck:
      // Do nothing, bailout will resume before the argument monitor ICs.
      break;
    case Bailout_BoundsCheck:
      HandleBoundsCheckFailure(cx, outerScript, innerScript);
      break;
    case Bailout_ShapeGuard:
      HandleShapeGuardFailure(cx, outerScript, innerScript);
      break;
    case Bailout_UninitializedLexical:
      HandleLexicalCheckFailure(cx, outerScript, innerScript);
      break;
    case Bailout_IonExceptionDebugMode:
      // Return false to resume in HandleException with reconstructed
      // baseline frame.
      return false;
    default:
      MOZ_CRASH("Unknown bailout kind!");
  }

  CheckFrequentBailouts(cx, outerScript, bailoutKind);
  return true;
}
