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
#include "ion/IonMacroAssembler.h"

#include "ds/LifoAlloc.h"

namespace js {
namespace ion {

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
    uint32_t pcOffset;
    uint32_t nativeOffset;
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

  private:
    void trace(JSTracer *trc);

    uint32_t icEntriesOffset_;
    uint32_t icEntries_;

    uint32_t pcMappingOffset_;
    uint32_t pcMappingEntries_;

  public:
    // Do not call directly, use BaselineScript::New. This is public for cx->new_.
    BaselineScript();

    static BaselineScript *New(JSContext *cx, size_t icEntries, size_t pcMappingEntries);
    static void Trace(JSTracer *trc, BaselineScript *script);
    static void Destroy(FreeOp *fop, BaselineScript *script);

    static inline size_t offsetOfMethod() {
        return offsetof(BaselineScript, method_);
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
    void copyPCMappingEntries(const PCMappingEntry *entries);
    uint8_t *nativeCodeForPC(HandleScript script, jsbytecode *pc);
};

inline bool IsBaselineEnabled(JSContext *cx)
{
    return cx->hasRunOption(JSOPTION_BASELINE);
}

MethodStatus
CanEnterBaselineJIT(JSContext *cx, HandleScript script, StackFrame *fp);

IonExecStatus
EnterBaselineMethod(JSContext *cx, StackFrame *fp);

} // namespace ion
} // namespace js

#endif

