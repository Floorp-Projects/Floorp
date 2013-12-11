/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_BaselineCompiler_shared_h
#define jit_shared_BaselineCompiler_shared_h

#include "jit/BaselineFrameInfo.h"
#include "jit/BaselineIC.h"
#include "jit/BytecodeAnalysis.h"
#include "jit/IonMacroAssembler.h"

namespace js {
namespace jit {

class BaselineCompilerShared
{
  protected:
    JSContext *cx;
    RootedScript script;
    jsbytecode *pc;
    MacroAssembler masm;
    bool ionCompileable_;
    bool ionOSRCompileable_;
    bool debugMode_;

    TempAllocator &alloc_;
    BytecodeAnalysis analysis_;
    FrameInfo frame;

    FallbackICStubSpace stubSpace_;
    js::Vector<ICEntry, 16, SystemAllocPolicy> icEntries_;

    // Stores the native code offset for a bytecode pc.
    struct PCMappingEntry
    {
        uint32_t pcOffset;
        uint32_t nativeOffset;
        PCMappingSlotInfo slotInfo;

        // If set, insert a PCMappingIndexEntry before encoding the
        // current entry.
        bool addIndexEntry;

        void fixupNativeOffset(MacroAssembler &masm) {
            CodeOffsetLabel offset(nativeOffset);
            offset.fixup(&masm);
            JS_ASSERT(offset.offset() <= UINT32_MAX);
            nativeOffset = (uint32_t) offset.offset();
        }
    };

    js::Vector<PCMappingEntry, 16, SystemAllocPolicy> pcMappingEntries_;

    // Labels for the 'movWithPatch' for loading IC entry pointers in
    // the generated IC-calling code in the main jitcode.  These need
    // to be patched with the actual icEntry offsets after the BaselineScript
    // has been allocated.
    struct ICLoadLabel {
        size_t icEntry;
        CodeOffsetLabel label;
    };
    js::Vector<ICLoadLabel, 16, SystemAllocPolicy> icLoadLabels_;

    uint32_t pushedBeforeCall_;
    mozilla::DebugOnly<bool> inCall_;

    CodeOffsetLabel spsPushToggleOffset_;

    BaselineCompilerShared(JSContext *cx, TempAllocator &alloc, HandleScript script);

    ICEntry *allocateICEntry(ICStub *stub, bool isForOp) {
        if (!stub)
            return nullptr;

        // Create the entry and add it to the vector.
        if (!icEntries_.append(ICEntry((uint32_t) (pc - script->code), isForOp)))
            return nullptr;
        ICEntry &vecEntry = icEntries_[icEntries_.length() - 1];

        // Set the first stub for the IC entry to the fallback stub
        vecEntry.setFirstStub(stub);

        // Return pointer to the IC entry
        return &vecEntry;
    }

    bool addICLoadLabel(CodeOffsetLabel label) {
        JS_ASSERT(!icEntries_.empty());
        ICLoadLabel loadLabel;
        loadLabel.label = label;
        loadLabel.icEntry = icEntries_.length() - 1;
        return icLoadLabels_.append(loadLabel);
    }

    JSFunction *function() const {
        return script->function();
    }

    PCMappingSlotInfo getStackTopSlotInfo() {
        JS_ASSERT(frame.numUnsyncedSlots() <= 2);
        switch (frame.numUnsyncedSlots()) {
          case 0:
            return PCMappingSlotInfo::MakeSlotInfo();
          case 1:
            return PCMappingSlotInfo::MakeSlotInfo(PCMappingSlotInfo::ToSlotLocation(frame.peek(-1)));
          case 2:
          default:
            return PCMappingSlotInfo::MakeSlotInfo(PCMappingSlotInfo::ToSlotLocation(frame.peek(-1)),
                                                   PCMappingSlotInfo::ToSlotLocation(frame.peek(-2)));
        }
    }

    template <typename T>
    void pushArg(const T& t) {
        masm.Push(t);
    }
    void prepareVMCall() {
        pushedBeforeCall_ = masm.framePushed();
        inCall_ = true;

        // Ensure everything is synced.
        frame.syncStack(0);

        // Save the frame pointer.
        masm.Push(BaselineFrameReg);
    }

    enum CallVMPhase {
        POST_INITIALIZE,
        PRE_INITIALIZE,
        CHECK_OVER_RECURSED
    };
    bool callVM(const VMFunction &fun, CallVMPhase phase=POST_INITIALIZE);

  public:
    BytecodeAnalysis &analysis() {
        return analysis_;
    }
};

} // namespace jit
} // namespace js

#endif /* jit_shared_BaselineCompiler_shared_h */
