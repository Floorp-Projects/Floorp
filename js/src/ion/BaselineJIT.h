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

#include "ds/LifoAlloc.h"

namespace js {
namespace ion {

class StackValue;
struct ICEntry;

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

    inline static int SlotInfoNumUnsynced(uint8_t slotInfo) {
        return static_cast<int>(slotInfo & 0x3);
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

    // Bit set when discarding JIT code, to indicate this script is
    // on the stack and should not be discarded.
    bool active_;

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
        return active_;
    }
    void setActive() {
        active_ = true;
    }
    void resetActive() {
        active_ = false;
    }

    uint32_t prologueOffset() const {
        return prologueOffset_;
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
        //JS_ASSERT(!invalidated());
        method_ = code;
    }

    ICEntry &icEntry(size_t index);
    ICEntry &icEntryFromReturnOffset(CodeOffsetLabel returnOffset);
    ICEntry &icEntryFromReturnAddress(uint8_t *returnAddr);

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

    // Toggle debug traps (used for breakpoints and step mode) in the script.
    // If |pc| is NULL, toggle traps for all ops in the script. Else, only
    // toggle traps at |pc|.
    void toggleDebugTraps(UnrootedScript script, jsbytecode *pc);
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

} // namespace ion
} // namespace js

#endif

