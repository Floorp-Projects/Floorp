/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_BaselineFrame_h
#define jit_BaselineFrame_h

#ifdef JS_ION

#include "jit/IonFrames.h"
#include "vm/Stack.h"

namespace js {
namespace jit {

// The stack looks like this, fp is the frame pointer:
//
// fp+y   arguments
// fp+x   IonJSFrameLayout (frame header)
// fp  => saved frame pointer
// fp-x   BaselineFrame
//        locals
//        stack values

// Eval frames
//
// Like js::StackFrame, every BaselineFrame is either a global frame
// or a function frame. Both global and function frames can optionally
// be "eval frames". The callee token for eval function frames is the
// enclosing function. BaselineFrame::evalScript_ stores the eval script
// itself.
class BaselineFrame
{
  public:
    enum Flags {
        // The frame has a valid return value. See also StackFrame::HAS_RVAL.
        HAS_RVAL         = 1 << 0,

        // A call object has been pushed on the scope chain.
        HAS_CALL_OBJ     = 1 << 2,

        // Frame has an arguments object, argsObj_.
        HAS_ARGS_OBJ     = 1 << 4,

        // See StackFrame::PREV_UP_TO_DATE.
        PREV_UP_TO_DATE  = 1 << 5,

        // Eval frame, see the "eval frames" comment.
        EVAL             = 1 << 6,

        // Frame has hookData_ set.
        HAS_HOOK_DATA    = 1 << 7,

        // Frame has profiler entry pushed.
        HAS_PUSHED_SPS_FRAME = 1 << 8,

        // Frame has over-recursed on an early check.
        OVER_RECURSED    = 1 << 9
    };

  protected: // Silence Clang warning about unused private fields.
    // We need to split the Value into 2 fields of 32 bits, otherwise the C++
    // compiler may add some padding between the fields.
    uint32_t loScratchValue_;
    uint32_t hiScratchValue_;
    uint32_t loReturnValue_;        // If HAS_RVAL, the frame's return value.
    uint32_t hiReturnValue_;
    uint32_t frameSize_;
    JSObject *scopeChain_;          // Scope chain (always initialized).
    JSScript *evalScript_;          // If isEvalFrame(), the current eval script.
    ArgumentsObject *argsObj_;      // If HAS_ARGS_OBJ, the arguments object.
    void *hookData_;                // If HAS_HOOK_DATA, debugger call hook data.
    uint32_t flags_;
#if JS_BITS_PER_WORD == 32
    uint32_t padding_;              // Pad to 8-byte alignment.
#endif

  public:
    // Distance between the frame pointer and the frame header (return address).
    // This is the old frame pointer saved in the prologue.
    static const uint32_t FramePointerOffset = sizeof(void *);

    bool initForOsr(StackFrame *fp, uint32_t numStackValues);

    uint32_t frameSize() const {
        return frameSize_;
    }
    void setFrameSize(uint32_t frameSize) {
        frameSize_ = frameSize;
    }
    inline uint32_t *addressOfFrameSize() {
        return &frameSize_;
    }
    JSObject *scopeChain() const {
        return scopeChain_;
    }
    void setScopeChain(JSObject *scopeChain) {
        scopeChain_ = scopeChain;
    }
    inline JSObject **addressOfScopeChain() {
        return &scopeChain_;
    }

    inline Value *addressOfScratchValue() {
        return reinterpret_cast<Value *>(&loScratchValue_);
    }

    inline void pushOnScopeChain(ScopeObject &scope);
    inline void popOffScopeChain();

    CalleeToken calleeToken() const {
        uint8_t *pointer = (uint8_t *)this + Size() + offsetOfCalleeToken();
        return *(CalleeToken *)pointer;
    }
    void replaceCalleeToken(CalleeToken token) {
        uint8_t *pointer = (uint8_t *)this + Size() + offsetOfCalleeToken();
        *(CalleeToken *)pointer = token;
    }
    JSScript *script() const {
        if (isEvalFrame())
            return evalScript();
        return ScriptFromCalleeToken(calleeToken());
    }
    JSFunction *fun() const {
        return CalleeTokenToFunction(calleeToken());
    }
    JSFunction *maybeFun() const {
        return isFunctionFrame() ? fun() : nullptr;
    }
    JSFunction *callee() const {
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

    Value &unaliasedVar(uint32_t i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) const {
        JS_ASSERT_IF(checkAliasing, !script()->varIsAliased(i));
        JS_ASSERT(i < script()->nfixed());
        return *valueSlot(i);
    }

    Value &unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) const {
        JS_ASSERT(i < numFormalArgs());
        JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals() &&
                                    !script()->formalIsAliased(i));
        return argv()[i];
    }

    Value &unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) const {
        JS_ASSERT(i < numActualArgs());
        JS_ASSERT_IF(checkAliasing, !script()->argsObjAliasesFormals());
        JS_ASSERT_IF(checkAliasing && i < numFormalArgs(), !script()->formalIsAliased(i));
        return argv()[i];
    }

    Value &unaliasedLocal(uint32_t i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING) const {
#ifdef DEBUG
        CheckLocalUnaliased(checkAliasing, script(), i);
#endif
        return *valueSlot(i);
    }

    unsigned numActualArgs() const {
        return *(size_t *)(reinterpret_cast<const uint8_t *>(this) +
                             BaselineFrame::Size() +
                             offsetOfNumActualArgs());
    }
    unsigned numFormalArgs() const {
        return script()->functionNonDelazifying()->nargs();
    }
    Value &thisValue() const {
        return *(Value *)(reinterpret_cast<const uint8_t *>(this) +
                         BaselineFrame::Size() +
                         offsetOfThis());
    }
    Value *argv() const {
        return (Value *)(reinterpret_cast<const uint8_t *>(this) +
                         BaselineFrame::Size() +
                         offsetOfArg(0));
    }

    bool copyRawFrameSlots(AutoValueVector *vec) const;

    bool hasReturnValue() const {
        return flags_ & HAS_RVAL;
    }
    MutableHandleValue returnValue() {
        return MutableHandleValue::fromMarkedLocation(reinterpret_cast<Value *>(&loReturnValue_));
    }
    void setReturnValue(const Value &v) {
        flags_ |= HAS_RVAL;
        returnValue().set(v);
    }
    inline Value *addressOfReturnValue() {
        return reinterpret_cast<Value *>(&loReturnValue_);
    }

    bool hasCallObj() const {
        return flags_ & HAS_CALL_OBJ;
    }

    inline CallObject &callObj() const;

    void setFlags(uint32_t flags) {
        flags_ = flags;
    }
    uint32_t *addressOfFlags() {
        return &flags_;
    }

    inline bool pushBlock(JSContext *cx, Handle<StaticBlockObject *> block);
    inline void popBlock(JSContext *cx);

    bool strictEvalPrologue(JSContext *cx);
    bool heavyweightFunPrologue(JSContext *cx);
    bool initFunctionScopeObjects(JSContext *cx);

    void initArgsObjUnchecked(ArgumentsObject &argsobj) {
        flags_ |= HAS_ARGS_OBJ;
        argsObj_ = &argsobj;
    }
    void initArgsObj(ArgumentsObject &argsobj) {
        JS_ASSERT(script()->needsArgsObj());
        initArgsObjUnchecked(argsobj);
    }
    bool hasArgsObj() const {
        return flags_ & HAS_ARGS_OBJ;
    }
    ArgumentsObject &argsObj() const {
        JS_ASSERT(hasArgsObj());
        JS_ASSERT(script()->needsArgsObj());
        return *argsObj_;
    }

    bool prevUpToDate() const {
        return flags_ & PREV_UP_TO_DATE;
    }
    void setPrevUpToDate() {
        flags_ |= PREV_UP_TO_DATE;
    }

    JSScript *evalScript() const {
        JS_ASSERT(isEvalFrame());
        return evalScript_;
    }

    bool hasHookData() const {
        return flags_ & HAS_HOOK_DATA;
    }

    void *maybeHookData() const {
        return hasHookData() ? hookData_ : nullptr;
    }

    void setHookData(void *v) {
        hookData_ = v;
        flags_ |= HAS_HOOK_DATA;
    }

    bool hasPushedSPSFrame() const {
        return flags_ & HAS_PUSHED_SPS_FRAME;
    }

    void setPushedSPSFrame() {
        flags_ |= HAS_PUSHED_SPS_FRAME;
    }

    void unsetPushedSPSFrame() {
        flags_ &= ~HAS_PUSHED_SPS_FRAME;
    }

    bool overRecursed() const {
        return flags_ & OVER_RECURSED;
    }

    void setOverRecursed() {
        flags_ |= OVER_RECURSED;
    }

    void trace(JSTracer *trc);

    bool isFunctionFrame() const {
        return CalleeTokenIsFunction(calleeToken());
    }
    bool isGlobalFrame() const {
        return !CalleeTokenIsFunction(calleeToken());
    }
     bool isEvalFrame() const {
        return flags_ & EVAL;
    }
    bool isStrictEvalFrame() const {
        return isEvalFrame() && script()->strict();
    }
    bool isNonStrictEvalFrame() const {
        return isEvalFrame() && !script()->strict();
    }
    bool isDirectEvalFrame() const {
        return isEvalFrame() && script()->staticLevel() > 0;
    }
    bool isNonStrictDirectEvalFrame() const {
        return isNonStrictEvalFrame() && isDirectEvalFrame();
    }
    bool isNonEvalFunctionFrame() const {
        return isFunctionFrame() && !isEvalFrame();
    }
    bool isDebuggerFrame() const {
        return false;
    }
    bool isGeneratorFrame() const {
        return false;
    }

    IonJSFrameLayout *framePrefix() const {
        uint8_t *fp = (uint8_t *)this + Size() + FramePointerOffset;
        return (IonJSFrameLayout *)fp;
    }

    // Methods below are used by the compiler.
    static size_t offsetOfCalleeToken() {
        return FramePointerOffset + js::jit::IonJSFrameLayout::offsetOfCalleeToken();
    }
    static size_t offsetOfThis() {
        return FramePointerOffset + js::jit::IonJSFrameLayout::offsetOfThis();
    }
    static size_t offsetOfArg(size_t index) {
        return FramePointerOffset + js::jit::IonJSFrameLayout::offsetOfActualArg(index);
    }
    static size_t offsetOfNumActualArgs() {
        return FramePointerOffset + js::jit::IonJSFrameLayout::offsetOfNumActualArgs();
    }
    static size_t Size() {
        return sizeof(BaselineFrame);
    }

    // The reverseOffsetOf methods below compute the offset relative to the
    // frame's base pointer. Since the stack grows down, these offsets are
    // negative.
    static int reverseOffsetOfFrameSize() {
        return -int(Size()) + offsetof(BaselineFrame, frameSize_);
    }
    static int reverseOffsetOfScratchValue() {
        return -int(Size()) + offsetof(BaselineFrame, loScratchValue_);
    }
    static int reverseOffsetOfScopeChain() {
        return -int(Size()) + offsetof(BaselineFrame, scopeChain_);
    }
    static int reverseOffsetOfArgsObj() {
        return -int(Size()) + offsetof(BaselineFrame, argsObj_);
    }
    static int reverseOffsetOfFlags() {
        return -int(Size()) + offsetof(BaselineFrame, flags_);
    }
    static int reverseOffsetOfEvalScript() {
        return -int(Size()) + offsetof(BaselineFrame, evalScript_);
    }
    static int reverseOffsetOfReturnValue() {
        return -int(Size()) + offsetof(BaselineFrame, loReturnValue_);
    }
    static int reverseOffsetOfLocal(size_t index) {
        return -int(Size()) - (index + 1) * sizeof(Value);
    }
};

// Ensure the frame is 8-byte aligned (required on ARM).
JS_STATIC_ASSERT(((sizeof(BaselineFrame) + BaselineFrame::FramePointerOffset) % 8) == 0);

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_BaselineFrame_h */
