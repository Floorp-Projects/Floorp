/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_jit_h__) && defined(JS_ION)
#define jsion_baseline_jit_h__

#include "jscntxt.h"
#include "jscompartment.h"

#include "IonCode.h"
#include "IonMacroAssembler.h"
#include "Bailouts.h"

#include "ds/LifoAlloc.h"

namespace js {
namespace ion {

class StackValue;
struct ICEntry;
class ICStub;

// ICStubSpace is an abstraction for allocation policy and storage for stub data.
struct ICStubSpace
{
  private:
    const static size_t STUB_DEFAULT_CHUNK_SIZE = 256;
    LifoAlloc allocator_;

    inline void *alloc_(size_t size) {
        return allocator_.alloc(size);
    }

  public:
    inline ICStubSpace()
      : allocator_(STUB_DEFAULT_CHUNK_SIZE) {}

    JS_DECLARE_NEW_METHODS(allocate, alloc_, inline)

    inline void adoptFrom(ICStubSpace *other) {
        allocator_.transferFrom(&(other->allocator_));
    }

    static ICStubSpace *FallbackStubSpaceFor(JSScript *script);
    static ICStubSpace *StubSpaceFor(JSScript *script);
};

// Stores the native code offset for a bytecode pc.
struct PCMappingEntry
{
    enum SlotLocation { SlotInR0 = 0, SlotInR1 = 1, SlotIgnore = 3 };

    static inline bool ValidSlotLocation(SlotLocation loc) {
        return (loc == SlotInR0) || (loc == SlotInR1) || (loc == SlotIgnore);
    }

    uint32_t pcOffset;
    uint32_t nativeOffset;
    // SlotInfo encoding:
    //  Bits 0 & 1: number of slots at top of stack which are unsynced.
    //  Bits 2 & 3: SlotLocation of top slot value (only relevant if numUnsynced > 0).
    //  Bits 3 & 4: SlotLocation of next slot value (only relevant if numUnsynced > 1).
    uint8_t slotInfo;

    void fixupNativeOffset(MacroAssembler &masm) {
        CodeOffsetLabel offset(nativeOffset);
        offset.fixup(&masm);
        JS_ASSERT(offset.offset() <= UINT32_MAX);
        nativeOffset = (uint32_t) offset.offset();
    }

    static SlotLocation ToSlotLocation(const StackValue *stackVal);
    inline static uint8_t MakeSlotInfo() { return static_cast<uint8_t>(0); }
    inline static uint8_t MakeSlotInfo(SlotLocation topSlotLoc) {
        JS_ASSERT(ValidSlotLocation(topSlotLoc));
        return static_cast<uint8_t>(1) | (static_cast<uint8_t>(topSlotLoc)) << 2;
    }
    inline static uint8_t MakeSlotInfo(SlotLocation topSlotLoc, SlotLocation nextSlotLoc) {
        JS_ASSERT(ValidSlotLocation(topSlotLoc));
        JS_ASSERT(ValidSlotLocation(nextSlotLoc));
        return static_cast<uint8_t>(2) | (static_cast<uint8_t>(topSlotLoc) << 2)
                                       | (static_cast<uint8_t>(nextSlotLoc) << 4);
    }

    inline static unsigned SlotInfoNumUnsynced(uint8_t slotInfo) {
        return static_cast<unsigned>(slotInfo & 0x3);
    }
    inline static SlotLocation SlotInfoTopSlotLocation(uint8_t slotInfo) {
        return static_cast<SlotLocation>((slotInfo >> 2) & 0x3);
    }
    inline static SlotLocation SlotInfoNextSlotLocation(uint8_t slotInfo) {
        return static_cast<SlotLocation>((slotInfo >> 4) & 0x3);
    }
};

struct BaselineScript
{
  private:
    // Code pointer containing the actual method.
    HeapPtr<IonCode> method_;

    // Allocated space for fallback stubs.
    ICStubSpace fallbackStubSpace_;

    // Allocated space for optimized stubs.
    ICStubSpace optimizedStubSpace_;

    // Native code offset right before the scope chain is initialized.
    uint32_t prologueOffset_;

  public:
    enum Flag {
        // Flag set by JSScript::argumentsOptimizationFailed. Similar to
        // JSScript::needsArgsObj_, but can be read from JIT code.
        NEEDS_ARGS_OBJ = 1 << 0,

        // Flag set when discarding JIT code, to indicate this script is
        // on the stack and should not be discarded.
        ACTIVE         = 1 << 1
    };

  private:
    uint32_t flags_;

  private:
    void trace(JSTracer *trc);

    uint32_t icEntriesOffset_;
    uint32_t icEntries_;

    uint32_t pcMappingOffset_;
    uint32_t pcMappingEntries_;

  public:
    // Do not call directly, use BaselineScript::New. This is public for cx->new_.
    BaselineScript(uint32_t prologueOffset);

    static BaselineScript *New(JSContext *cx, uint32_t prologueOffset, size_t icEntries,
                               size_t pcMappingEntries);
    static void Trace(JSTracer *trc, BaselineScript *script);
    static void Destroy(FreeOp *fop, BaselineScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(BaselineScript, method_);
    }

    bool active() const {
        return flags_ & ACTIVE;
    }
    void setActive() {
        flags_ |= ACTIVE;
    }
    void resetActive() {
        flags_ &= ~ACTIVE;
    }

    void setNeedsArgsObj() {
        flags_ |= NEEDS_ARGS_OBJ;
    }

    uint32_t prologueOffset() const {
        return prologueOffset_;
    }
    uint8_t *prologueEntryAddr() const {
        return method_->raw() + prologueOffset_;
    }

    ICEntry *icEntryList() {
        return (ICEntry *)(reinterpret_cast<uint8_t *>(this) + icEntriesOffset_);
    }
    PCMappingEntry *pcMappingEntryList() {
        return (PCMappingEntry *)(reinterpret_cast<uint8_t *>(this) + pcMappingOffset_);
    }

    ICStubSpace *fallbackStubSpace() {
        return &fallbackStubSpace_;
    }

    ICStubSpace *optimizedStubSpace() {
        return &optimizedStubSpace_;
    }

    IonCode *method() const {
        return method_;
    }
    void setMethod(IonCode *code) {
        JS_ASSERT(!method_);
        method_ = code;
    }

    void toggleBarriers(bool enabled) {
        method()->togglePreBarriers(enabled);
    }

    ICEntry &icEntry(size_t index);
    ICEntry &icEntryFromReturnOffset(CodeOffsetLabel returnOffset);
    ICEntry &icEntryFromPCOffset(uint32_t pcOffset);
    ICEntry &icEntryFromReturnAddress(uint8_t *returnAddr);
    uint8_t *returnAddressForIC(const ICEntry &ent);

    size_t numICEntries() const {
        return icEntries_;
    }

    void copyICEntries(const ICEntry *entries, MacroAssembler &masm);
    void adoptFallbackStubs(ICStubSpace *stubSpace);

    size_t numPCMappingEntries() const {
        return pcMappingEntries_;
    }

    PCMappingEntry &pcMappingEntry(size_t index);
    void copyPCMappingEntries(const PCMappingEntry *entries, MacroAssembler &masm);
    uint8_t *nativeCodeForPC(HandleScript script, jsbytecode *pc);
    uint8_t slotInfoForPC(HandleScript script, jsbytecode *pc);

    // Toggle debug traps (used for breakpoints and step mode) in the script.
    // If |pc| is NULL, toggle traps for all ops in the script. Else, only
    // toggle traps at |pc|.
    void toggleDebugTraps(UnrootedScript script, jsbytecode *pc);

    static size_t offsetOfFlags() {
        return offsetof(BaselineScript, flags_);
    }
};

inline bool IsBaselineEnabled(JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_BASELINE);
}

MethodStatus
CanEnterBaselineJIT(JSContext *cx, JSScript *scriptArg, StackFrame *fp, bool newType);

IonExecStatus
EnterBaselineMethod(JSContext *cx, StackFrame *fp);

void
FinishDiscardBaselineScript(FreeOp *fop, UnrootedScript script);

struct BaselineBailoutInfo
{
    // Pointer into the current C stack, where overwriting will start.
    void *incomingStack;

    // The top and bottom heapspace addresses of the reconstructed stack
    // which will be copied to the bottom.
    uint8_t *copyStackTop;
    uint8_t *copyStackBottom;

    // Fields to store the top-of-stack baseline values that are held
    // in registers.  The setR0 and setR1 fields are flags indicating
    // whether each one is initialized.
    uint32_t setR0;
    Value valueR0;
    uint32_t setR1;
    Value valueR1;

    // The value of the frame pointer register on resume.
    void *resumeFramePtr;

    // The native code address to resume into.
    void *resumeAddr;

    // If resuming into a TypeMonitor IC chain, this field holds the
    // address of the first stub in that chain.  If this field is
    // set, then the actual jitcode resumed into is the jitcode for
    // the first stub, not the resumeAddr above.  The resumeAddr
    // above, in this case, is pushed onto the stack so that the
    // TypeMonitor chain can tail-return into the main jitcode when done.
    ICStub *monitorStub;
};

uint32_t
BailoutIonToBaseline(JSContext *cx, IonActivation *activation, IonBailoutIterator &iter,
                     bool invalidate, BaselineBailoutInfo **bailoutInfo);

} // namespace ion
} // namespace js

#endif

