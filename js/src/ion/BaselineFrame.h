/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_frame_h__) && defined(JS_ION)
#define jsion_baseline_frame_h__

#include "jscntxt.h"
#include "jscompartment.h"

#include "IonFrames.h"

namespace js {
namespace ion {

// The stack looks like this, fp is the frame pointer:
//
// fp+y   arguments
// fp+x   IonJSFrameLayout (frame header)
// fp  => saved frame pointer
// fp-x   BaselineFrame
//        locals
//        stack values
class BaselineFrame
{
  public:
    enum Flags {
        // The frame has a valid return value. See also StackFrame::HAS_RVAL.
        HAS_RVAL         = 1 << 0,

        // See StackFrame::PREV_UP_TO_DATE.
        PREV_UP_TO_DATE  = 1 << 1
    };

  protected: // Silence Clang warning about unused private fields.
    // We need to split the Value into 2 fields of 32 bits, otherwise the C++
    // compiler may add some padding between the fields.
    uint32_t loScratchValue_;
    uint32_t hiScratchValue_;
    uint32_t loReturnValue_;
    uint32_t hiReturnValue_;
    size_t frameSize_;
    JSObject *scopeChain_;
    uint32_t flags_;

  public:
    // Distance between the frame pointer and the frame header (return address).
    // This is the old frame pointer saved in the prologue.
    static const uint32_t FramePointerOffset = sizeof(void *);

    size_t frameSize() const {
        return frameSize_;
    }
    UnrootedObject scopeChain() const {
        return scopeChain_;
    }
    CalleeToken calleeToken() const {
        uint8_t *pointer = (uint8_t *)this + Size() + offsetOfCalleeToken();
        return *(CalleeToken *)pointer;
    }
    UnrootedScript script() const {
        return ScriptFromCalleeToken(calleeToken());
    }
    UnrootedFunction fun() const {
        return CalleeTokenToFunction(calleeToken());
    }
    UnrootedFunction callee() const {
        return CalleeTokenToFunction(calleeToken());
    }
    Value calleev() const {
        return ObjectValue(*callee());
    }
    size_t numValueSlots() const {
        size_t size = frameSize();

        JS_ASSERT(size >= BaselineFrame::FramePointerOffset + BaselineFrame::Size());
        size -= BaselineFrame::FramePointerOffset + BaselineFrame::Size();

        JS_ASSERT((size % sizeof(Value)) == 0);
        return size / sizeof(Value);
    }
    Value *valueSlot(size_t slot) const {
        JS_ASSERT(slot < numValueSlots());
        return (Value *)this - (slot + 1);
    }

    Value &unaliasedVar(unsigned i, MaybeCheckAliasing checkAliasing) const {
        JS_ASSERT_IF(checkAliasing, !script()->varIsAliased(i));
        JS_ASSERT(i < script()->nfixed);
        return *valueSlot(i);
    }

    Value &unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing) const {
        JS_ASSERT(i < numFormalArgs());
        JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
        JS_ASSERT_IF(checkAliasing, !script()->formalIsAliased(i));
        return formals()[i];
    }

    unsigned numActualArgs() const {
        return *(unsigned *)(reinterpret_cast<const uint8_t *>(this) +
                             BaselineFrame::Size() +
                             offsetOfNumActualArgs());
    }
    unsigned numFormalArgs() const {
        return script()->function()->nargs;
    }
    Value &thisValue() const {
        return *(Value *)(reinterpret_cast<const uint8_t *>(this) +
                         BaselineFrame::Size() +
                         offsetOfThis());
    }
    Value *formals() const {
        return (Value *)(reinterpret_cast<const uint8_t *>(this) +
                         BaselineFrame::Size() +
                         offsetOfArg(0));
    }
    Value *actuals() const {
        return formals();
    }

    bool copyRawFrameSlots(AutoValueVector *vec) const;

    inline bool hasReturnValue() const {
        return flags_ & HAS_RVAL;
    }
    inline Value *returnValue() {
        return reinterpret_cast<Value *>(&loReturnValue_);
    }
    inline void setReturnValue(const Value &v) {
        flags_ |= HAS_RVAL;
        *returnValue() = v;
    }

    bool prevUpToDate() const {
        return flags_ & PREV_UP_TO_DATE;
    }
    void setPrevUpToDate() {
        flags_ |= PREV_UP_TO_DATE;
    }

    void *maybeHookData() const {
        return NULL;
    }

    void trace(JSTracer *trc);

    bool isGlobalFrame() const {
        return !script()->function();
    }
    bool isEvalFrame() const {
        return false;
    }
    bool isNonStrictDirectEvalFrame() const {
        return false;
    }
    bool isStrictEvalFrame() const {
        return false;
    }
    bool isNonEvalFunctionFrame() const {
        return !!script()->function();
    }
    bool isFunctionFrame() const {
        return !!script()->function();
    }

    // Methods below are used by the compiler.
    static size_t offsetOfCalleeToken() {
        return FramePointerOffset + js::ion::IonJSFrameLayout::offsetOfCalleeToken();
    }
    static inline size_t offsetOfThis() {
        return FramePointerOffset + js::ion::IonJSFrameLayout::offsetOfThis();
    }
    static inline size_t offsetOfArg(size_t index) {
        return FramePointerOffset + js::ion::IonJSFrameLayout::offsetOfActualArg(index);
    }
    static inline size_t offsetOfNumActualArgs() {
        return FramePointerOffset + js::ion::IonJSFrameLayout::offsetOfNumActualArgs();
    }
    static size_t Size() {
        return sizeof(BaselineFrame);
    }

    // The reverseOffsetOf methods below compute the offset relative to the
    // frame's base pointer. Since the stack grows down, these offsets are
    // negative.
    static inline size_t reverseOffsetOfFrameSize() {
        return -BaselineFrame::Size() + offsetof(BaselineFrame, frameSize_);
    }
    static inline size_t reverseOffsetOfScratchValue() {
        return -BaselineFrame::Size() + offsetof(BaselineFrame, loScratchValue_);
    }
    static inline size_t reverseOffsetOfScopeChain() {
        return -BaselineFrame::Size() + offsetof(BaselineFrame, scopeChain_);
    }
    static inline size_t reverseOffsetOfFlags() {
        return -BaselineFrame::Size() + offsetof(BaselineFrame, flags_);
    }
    static inline size_t reverseOffsetOfReturnValue() {
        return -BaselineFrame::Size() + offsetof(BaselineFrame, loReturnValue_);
    }
    static inline size_t reverseOffsetOfLocal(size_t index) {
        return -BaselineFrame::Size() - (index + 1) * sizeof(Value);
    }
};

// Ensure the frame is 8-byte aligned (required on ARM).
JS_STATIC_ASSERT(((sizeof(BaselineFrame) + BaselineFrame::FramePointerOffset) % 8) == 0);

} // namespace ion
} // namespace js

#endif

