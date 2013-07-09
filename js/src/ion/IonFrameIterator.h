/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_IonFrameIterator_h
#define ion_IonFrameIterator_h

#ifdef JS_ION

#include "jstypes.h"
#include "ion/IonCode.h"
#include "ion/SnapshotReader.h"

class JSFunction;
class JSScript;

namespace js {
    class ActivationIterator;
};

namespace js {
namespace ion {

enum FrameType
{
    // A JS frame is analagous to a js::StackFrame, representing one scripted
    // functon activation. OptimizedJS frames are used by the optimizing compiler.
    IonFrame_OptimizedJS,

    // JS frame used by the baseline JIT.
    IonFrame_BaselineJS,

    // Frame pushed for baseline JIT stubs that make non-tail calls, so that the
    // return address -> ICEntry mapping works.
    IonFrame_BaselineStub,

    // The entry frame is the initial prologue block transitioning from the VM
    // into the Ion world.
    IonFrame_Entry,

    // A rectifier frame sits in between two JS frames, adapting argc != nargs
    // mismatches in calls.
    IonFrame_Rectifier,

    // An unwound JS frame is a JS frame signalling that its callee frame has been
    // turned into an exit frame (see EnsureExitFrame). Used by Ion bailouts and
    // Baseline exception unwinding.
    IonFrame_Unwound_OptimizedJS,

    // Like Unwound_OptimizedJS, but the caller is a baseline stub frame.
    IonFrame_Unwound_BaselineStub,

    // An unwound rectifier frame is a rectifier frame signalling that its callee
    // frame has been turned into an exit frame (see EnsureExitFrame).
    IonFrame_Unwound_Rectifier,

    // An exit frame is necessary for transitioning from a JS frame into C++.
    // From within C++, an exit frame is always the last frame in any
    // JitActivation.
    IonFrame_Exit,

    // An OSR frame is added when performing OSR from within a bailout. It
    // looks like a JS frame, but does not push scripted arguments, as OSR
    // reads arguments from a js::StackFrame.
    IonFrame_Osr
};

class IonCommonFrameLayout;
class IonJSFrameLayout;
class IonExitFrameLayout;

class BaselineFrame;

class JitActivation;

class IonFrameIterator
{
  protected:
    uint8_t *current_;
    FrameType type_;
    uint8_t *returnAddressToFp_;
    size_t frameSize_;

  private:
    mutable const SafepointIndex *cachedSafepointIndex_;
    const JitActivation *activation_;

    void dumpBaseline() const;

  public:
    IonFrameIterator(uint8_t *top)
      : current_(top),
        type_(IonFrame_Exit),
        returnAddressToFp_(NULL),
        frameSize_(0),
        cachedSafepointIndex_(NULL),
        activation_(NULL)
    { }

    IonFrameIterator(const ActivationIterator &activations);
    IonFrameIterator(IonJSFrameLayout *fp);

    // Current frame information.
    FrameType type() const {
        return type_;
    }
    uint8_t *fp() const {
        return current_;
    }

    inline IonCommonFrameLayout *current() const;
    inline uint8_t *returnAddress() const;

    IonJSFrameLayout *jsFrame() const {
        JS_ASSERT(isScripted());
        return (IonJSFrameLayout *) fp();
    }

    // Returns true iff this exit frame was created using EnsureExitFrame.
    inline bool isFakeExitFrame() const;

    inline IonExitFrameLayout *exitFrame() const;

    // Returns whether the JS frame has been invalidated and, if so,
    // places the invalidated Ion script in |ionScript|.
    bool checkInvalidation(IonScript **ionScript) const;
    bool checkInvalidation() const;

    bool isScripted() const {
        return type_ == IonFrame_BaselineJS || type_ == IonFrame_OptimizedJS;
    }
    bool isBaselineJS() const {
        return type_ == IonFrame_BaselineJS;
    }
    bool isOptimizedJS() const {
        return type_ == IonFrame_OptimizedJS;
    }
    bool isBaselineStub() const {
        return type_ == IonFrame_BaselineStub;
    }
    bool isNative() const;
    bool isOOLNativeGetter() const;
    bool isOOLPropertyOp() const;
    bool isOOLProxyGet() const;
    bool isDOMExit() const;
    bool isEntry() const {
        return type_ == IonFrame_Entry;
    }
    bool isFunctionFrame() const;
    bool isParallelFunctionFrame() const;

    bool isConstructing() const;

    bool isEntryJSFrame() const;

    void *calleeToken() const;
    JSFunction *callee() const;
    JSFunction *maybeCallee() const;
    unsigned numActualArgs() const;
    JSScript *script() const;
    void baselineScriptAndPc(JSScript **scriptRes, jsbytecode **pcRes) const;
    Value *nativeVp() const;
    Value *actualArgs() const;

    // Returns the return address of the frame above this one (that is, the
    // return address that returns back to the current frame).
    uint8_t *returnAddressToFp() const {
        return returnAddressToFp_;
    }

    // Previous frame information extracted from the current frame.
    inline size_t prevFrameLocalSize() const;
    inline FrameType prevType() const;
    uint8_t *prevFp() const;

    // Returns the stack space used by the current frame, in bytes. This does
    // not include the size of its fixed header.
    inline size_t frameSize() const;

    // Functions used to iterate on frames. When prevType is IonFrame_Entry,
    // the current frame is the last frame.
    inline bool done() const {
        return type_ == IonFrame_Entry;
    }
    IonFrameIterator &operator++();

    // Returns the IonScript associated with this JS frame.
    IonScript *ionScript() const;

    // Returns the Safepoint associated with this JS frame. Incurs a lookup
    // overhead.
    const SafepointIndex *safepoint() const;

    // Returns the OSI index associated with this JS frame. Incurs a lookup
    // overhead.
    const OsiIndex *osiIndex() const;

    uintptr_t *spillBase() const;
    MachineState machineState() const;

    template <class Op>
    void forEachCanonicalActualArg(Op op, unsigned start, unsigned count) const {
        JS_ASSERT(isBaselineJS());

        unsigned nactual = numActualArgs();
        if (count == unsigned(-1))
            count = nactual - start;

        unsigned end = start + count;
        JS_ASSERT(start <= end && end <= nactual);

        Value *argv = actualArgs();
        for (unsigned i = start; i < end; i++)
            op(argv[i]);
    }

    void dump() const;

    inline BaselineFrame *baselineFrame() const;
};

class IonJSFrameLayout;
class IonBailoutIterator;

// Reads frame information in snapshot-encoding order (that is, outermost frame
// to innermost frame).
class SnapshotIterator : public SnapshotReader
{
    IonJSFrameLayout *fp_;
    MachineState machine_;
    IonScript *ionScript_;

  private:
    bool hasLocation(const SnapshotReader::Location &loc);
    uintptr_t fromLocation(const SnapshotReader::Location &loc);
    static Value FromTypedPayload(JSValueType type, uintptr_t payload);

    Value slotValue(const Slot &slot);
    bool slotReadable(const Slot &slot);
    void warnUnreadableSlot();

  public:
    SnapshotIterator(IonScript *ionScript, SnapshotOffset snapshotOffset,
                     IonJSFrameLayout *fp, const MachineState &machine);
    SnapshotIterator(const IonFrameIterator &iter);
    SnapshotIterator(const IonBailoutIterator &iter);
    SnapshotIterator();

    Value read() {
        return slotValue(readSlot());
    }
    Value maybeRead(bool silentFailure = false) {
        Slot s = readSlot();
        if (slotReadable(s))
            return slotValue(s);
        if (!silentFailure)
            warnUnreadableSlot();
        return UndefinedValue();
    }

    template <class Op>
    void readFrameArgs(Op &op, const Value *argv, Value *scopeChain, Value *thisv,
                       unsigned start, unsigned formalEnd, unsigned iterEnd, JSScript *script)
    {
        if (scopeChain)
            *scopeChain = read();
        else
            skip();

        // Skip slot for arguments object.
        if (script->argumentsHasVarBinding())
            skip();

        if (thisv)
            *thisv = read();
        else
            skip();

        unsigned i = 0;
        if (formalEnd < start)
            i = start;

        for (; i < start; i++)
            skip();
        for (; i < formalEnd && i < iterEnd; i++) {
            // We are not always able to read values from the snapshots, some values
            // such as non-gc things may still be live in registers and cause an
            // error while reading the machine state.
            Value v = maybeRead();
            op(v);
        }
        if (iterEnd >= formalEnd) {
            for (; i < iterEnd; i++)
                op(argv[i]);
        }
    }

    Value maybeReadSlotByIndex(size_t index) {
        while (index--) {
            JS_ASSERT(moreSlots());
            skip();
        }

        Value s = maybeRead(true);

        while (moreSlots())
            skip();

        return s;
    }
};

// Reads frame information in callstack order (that is, innermost frame to
// outermost frame).
template <AllowGC allowGC=CanGC>
class InlineFrameIteratorMaybeGC
{
    const IonFrameIterator *frame_;
    SnapshotIterator start_;
    SnapshotIterator si_;
    unsigned framesRead_;
    typename MaybeRooted<JSFunction*, allowGC>::RootType callee_;
    typename MaybeRooted<JSScript*, allowGC>::RootType script_;
    jsbytecode *pc_;
    uint32_t numActualArgs_;

  private:
    void findNextFrame();

  public:
    InlineFrameIteratorMaybeGC(JSContext *cx, const IonFrameIterator *iter)
      : callee_(cx),
        script_(cx)
    {
        resetOn(iter);
    }

    InlineFrameIteratorMaybeGC(JSContext *cx, const IonBailoutIterator *iter);

    InlineFrameIteratorMaybeGC(JSContext *cx, const InlineFrameIteratorMaybeGC *iter)
      : frame_(iter ? iter->frame_ : NULL),
        framesRead_(0),
        callee_(cx),
        script_(cx)
    {
        if (frame_) {
            start_ = SnapshotIterator(*frame_);
            // findNextFrame will iterate to the next frame and init. everything.
            // Therefore to settle on the same frame, we report one frame less readed.
            framesRead_ = iter->framesRead_ - 1;
            findNextFrame();
        }
    }

    bool more() const {
        return frame_ && framesRead_ < start_.frameCount();
    }
    JSFunction *callee() const {
        JS_ASSERT(callee_);
        return callee_;
    }
    JSFunction *maybeCallee() const {
        return callee_;
    }

    unsigned numActualArgs() const {
        // The number of actual arguments of inline frames is recovered by the
        // iteration process. It is recovered from the bytecode because this
        // property still hold since the for inlined frames. This property does not
        // hold for the parent frame because it can have optimize a call to
        // js_fun_call or js_fun_apply.
        if (more())
            return numActualArgs_;

        return frame_->numActualArgs();
    }

    template <class Op>
    void forEachCanonicalActualArg(JSContext *cx, Op op, unsigned start, unsigned count) const {
        unsigned nactual = numActualArgs();
        if (count == unsigned(-1))
            count = nactual - start;

        unsigned end = start + count;
        unsigned nformal = callee()->nargs;

        JS_ASSERT(start <= end && end <= nactual);

        if (more()) {
            // There is still a parent frame of this inlined frame.
            // The not overflown arguments are taken from the inlined frame,
            // because it will have the updated value when JSOP_SETARG is done.
            // All arguments (also the overflown) are the last pushed values in the parent frame.
            // To get the overflown arguments, we need to take them from there.

            // Get the non overflown arguments
            unsigned formal_end = (end < nformal) ? end : nformal;
            SnapshotIterator s(si_);
            s.readFrameArgs(op, NULL, NULL, NULL, start, nformal, formal_end, script());

            // The overflown arguments are not available in current frame.
            // They are the last pushed arguments in the parent frame of this inlined frame.
            InlineFrameIteratorMaybeGC it(cx, this);
            ++it;
            unsigned argsObjAdj = it.script()->argumentsHasVarBinding() ? 1 : 0;
            SnapshotIterator parent_s(it.snapshotIterator());

            // Skip over all slots untill we get to the last slots (= arguments slots of callee)
            // the +2 is for [this] and [scopechain], and maybe +1 for [argsObj]
            JS_ASSERT(parent_s.slots() >= nactual + 2 + argsObjAdj);
            unsigned skip = parent_s.slots() - nactual - 2 - argsObjAdj;
            for (unsigned j = 0; j < skip; j++)
                parent_s.skip();

            // Get the overflown arguments
            parent_s.readFrameArgs(op, NULL, NULL, NULL, nformal, nactual, end, it.script());
        } else {
            SnapshotIterator s(si_);
            Value *argv = frame_->actualArgs();
            s.readFrameArgs(op, argv, NULL, NULL, start, nformal, end, script());
        }
    }

    JSScript *script() const {
        return script_;
    }
    jsbytecode *pc() const {
        return pc_;
    }
    SnapshotIterator snapshotIterator() const {
        return si_;
    }
    bool isFunctionFrame() const;
    bool isConstructing() const;

    JSObject *scopeChain() const {
        SnapshotIterator s(si_);

        // scopeChain
        Value v = s.read();
        if (v.isObject()) {
            JS_ASSERT_IF(script()->hasAnalysis(), script()->analysis()->usesScopeChain());
            return &v.toObject();
        }

        return callee()->environment();
    }

    JSObject *thisObject() const {
        // JS_ASSERT(isConstructing(...));
        SnapshotIterator s(si_);

        // scopeChain
        s.skip();

        // Arguments object.
        if (script()->argumentsHasVarBinding())
            s.skip();

        // In strict modes, |this| may not be an object and thus may not be
        // readable which can either segv in read or trigger the assertion.
        Value v = s.read();
        JS_ASSERT(v.isObject());
        return &v.toObject();
    }

    InlineFrameIteratorMaybeGC &operator++() {
        findNextFrame();
        return *this;
    }

    void dump() const;

    void resetOn(const IonFrameIterator *iter);

  private:
    InlineFrameIteratorMaybeGC() MOZ_DELETE;
    InlineFrameIteratorMaybeGC(const InlineFrameIteratorMaybeGC &iter) MOZ_DELETE;
};
typedef InlineFrameIteratorMaybeGC<CanGC> InlineFrameIterator;
typedef InlineFrameIteratorMaybeGC<NoGC> InlineFrameIteratorNoGC;

} // namespace ion
} // namespace js

#endif // JS_ION

#endif /* ion_IonFrameIterator_h */
