/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_IonFrameIterator_h
#define jit_IonFrameIterator_h

#ifdef JS_ION

#include "jsfun.h"
#include "jsscript.h"
#include "jstypes.h"

#include "jit/IonCode.h"
#include "jit/Snapshots.h"

namespace js {
    class ActivationIterator;
};

namespace js {
namespace jit {

enum FrameType
{
    // A JS frame is analagous to a js::InterpreterFrame, representing one scripted
    // functon activation. IonJS frames are used by the optimizing compiler.
    JitFrame_IonJS,

    // JS frame used by the baseline JIT.
    JitFrame_BaselineJS,

    // Frame pushed for baseline JIT stubs that make non-tail calls, so that the
    // return address -> ICEntry mapping works.
    JitFrame_BaselineStub,

    // The entry frame is the initial prologue block transitioning from the VM
    // into the Ion world.
    JitFrame_Entry,

    // A rectifier frame sits in between two JS frames, adapting argc != nargs
    // mismatches in calls.
    JitFrame_Rectifier,

    // An unwound JS frame is a JS frame signalling that its callee frame has been
    // turned into an exit frame (see EnsureExitFrame). Used by Ion bailouts and
    // Baseline exception unwinding.
    JitFrame_Unwound_IonJS,

    // Like Unwound_IonJS, but the caller is a baseline stub frame.
    JitFrame_Unwound_BaselineStub,

    // An unwound rectifier frame is a rectifier frame signalling that its callee
    // frame has been turned into an exit frame (see EnsureExitFrame).
    JitFrame_Unwound_Rectifier,

    // An exit frame is necessary for transitioning from a JS frame into C++.
    // From within C++, an exit frame is always the last frame in any
    // JitActivation.
    JitFrame_Exit
};

enum ReadFrameArgsBehavior {
    // Only read formals (i.e. [0 ... callee()->nargs]
    ReadFrame_Formals,

    // Only read overflown args (i.e. [callee()->nargs ... numActuals()]
    ReadFrame_Overflown,

    // Read all args (i.e. [0 ... numActuals()])
    ReadFrame_Actuals
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
    ExecutionMode mode_;

    void dumpBaseline() const;

  public:
    explicit IonFrameIterator(uint8_t *top, ExecutionMode mode)
      : current_(top),
        type_(JitFrame_Exit),
        returnAddressToFp_(nullptr),
        frameSize_(0),
        cachedSafepointIndex_(nullptr),
        activation_(nullptr),
        mode_(mode)
    { }

    explicit IonFrameIterator(JSContext *cx);
    explicit IonFrameIterator(const ActivationIterator &activations);
    explicit IonFrameIterator(IonJSFrameLayout *fp, ExecutionMode mode);

    // Current frame information.
    FrameType type() const {
        return type_;
    }
    uint8_t *fp() const {
        return current_;
    }

    IonCommonFrameLayout *current() const {
        return (IonCommonFrameLayout *)current_;
    }

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
        return type_ == JitFrame_BaselineJS || type_ == JitFrame_IonJS;
    }
    bool isBaselineJS() const {
        return type_ == JitFrame_BaselineJS;
    }
    bool isIonJS() const {
        return type_ == JitFrame_IonJS;
    }
    bool isBaselineStub() const {
        return type_ == JitFrame_BaselineStub;
    }
    bool isNative() const;
    bool isOOLNative() const;
    bool isOOLPropertyOp() const;
    bool isOOLProxy() const;
    bool isDOMExit() const;
    bool isEntry() const {
        return type_ == JitFrame_Entry;
    }
    bool isFunctionFrame() const;

    bool isConstructing() const;

    void *calleeToken() const;
    JSFunction *callee() const;
    JSFunction *maybeCallee() const;
    unsigned numActualArgs() const;
    JSScript *script() const;
    void baselineScriptAndPc(JSScript **scriptRes, jsbytecode **pcRes) const;
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
    size_t frameSize() const {
        JS_ASSERT(type_ != JitFrame_Exit);
        return frameSize_;
    }

    // Functions used to iterate on frames. When prevType is JitFrame_Entry,
    // the current frame is the last frame.
    inline bool done() const {
        return type_ == JitFrame_Entry;
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
    void unaliasedForEachActual(Op op, ReadFrameArgsBehavior behavior) const {
        JS_ASSERT(isBaselineJS());

        unsigned nactual = numActualArgs();
        unsigned start, end;
        switch (behavior) {
          case ReadFrame_Formals:
            start = 0;
            end = callee()->nargs();
            break;
          case ReadFrame_Overflown:
            start = callee()->nargs();
            end = nactual;
            break;
          case ReadFrame_Actuals:
            start = 0;
            end = nactual;
        }

        Value *argv = actualArgs();
        for (unsigned i = start; i < end; i++)
            op(argv[i]);
    }

    void dump() const;

    inline BaselineFrame *baselineFrame() const;
};

class IonJSFrameLayout;
class IonBailoutIterator;

class RResumePoint;

// Reads frame information in snapshot-encoding order (that is, outermost frame
// to innermost frame).
class SnapshotIterator
{
    SnapshotReader snapshot_;
    RecoverReader recover_;
    IonJSFrameLayout *fp_;
    MachineState machine_;
    IonScript *ionScript_;

  private:
    // Read a spilled register from the machine state.
    bool hasRegister(Register reg) const {
        return machine_.has(reg);
    }
    uintptr_t fromRegister(Register reg) const {
        return machine_.read(reg);
    }

    bool hasRegister(FloatRegister reg) const {
        return machine_.has(reg);
    }
    double fromRegister(FloatRegister reg) const {
        return machine_.read(reg);
    }

    // Read an uintptr_t from the stack.
    bool hasStack(int32_t offset) const {
        return true;
    }
    uintptr_t fromStack(int32_t offset) const;

    Value allocationValue(const RValueAllocation &a);
    bool allocationReadable(const RValueAllocation &a);
    void warnUnreadableAllocation();

  public:
    // Handle iterating over RValueAllocations of the snapshots.
    inline RValueAllocation readAllocation() {
        MOZ_ASSERT(moreAllocations());
        return snapshot_.readAllocation();
    }
    Value skip() {
        snapshot_.skipAllocation();
        return UndefinedValue();
    }

    const RResumePoint *resumePoint() const;
    const RInstruction *instruction() const {
        return recover_.instruction();
    }

    uint32_t numAllocations() const;
    inline bool moreAllocations() const {
        return snapshot_.numAllocationsRead() < numAllocations();
    }

  public:
    // Exhibits frame properties contained in the snapshot.
    uint32_t pcOffset() const;
    inline bool resumeAfter() const {
        // Inline frames are inlined on calls, which are considered as being
        // resumed on the Call as baseline will push the pc once we return from
        // the call.
        if (moreFrames())
            return false;
        return recover_.resumeAfter();
    }
    inline BailoutKind bailoutKind() const {
        return snapshot_.bailoutKind();
    }

  public:
    // Read the next instruction available and get ready to either skip it or
    // evaluate it.
    inline void nextInstruction() {
        MOZ_ASSERT(snapshot_.numAllocationsRead() == numAllocations());
        recover_.nextInstruction();
        snapshot_.resetNumAllocationsRead();
    }

    // Skip an Instruction by walking to the next instruction and by skipping
    // all the allocations corresponding to this instruction.
    void skipInstruction();

    inline bool moreInstructions() const {
        return recover_.moreInstructions();
    }

  public:
    // Handle iterating over frames of the snapshots.
    void nextFrame();

    inline bool moreFrames() const {
        // The last instruction is recovering the innermost frame, so as long as
        // there is more instruction there is necesseray more frames.
        return moreInstructions();
    }

  public:
    // Connect all informations about the current script in order to recover the
    // content of baseline frames.

    SnapshotIterator(IonScript *ionScript, SnapshotOffset snapshotOffset,
                     IonJSFrameLayout *fp, const MachineState &machine);
    SnapshotIterator(const IonFrameIterator &iter);
    SnapshotIterator(const IonBailoutIterator &iter);
    SnapshotIterator();

    Value read() {
        return allocationValue(readAllocation());
    }
    Value maybeRead(bool silentFailure = false) {
        RValueAllocation a = readAllocation();
        if (allocationReadable(a))
            return allocationValue(a);
        if (!silentFailure)
            warnUnreadableAllocation();
        return UndefinedValue();
    }

    template <class Op>
    void readFrameArgs(Op &op, Value *scopeChain, Value *thisv,
                       unsigned start, unsigned end, JSScript *script)
    {
        if (scopeChain)
            *scopeChain = read();
        else
            skip();

        // Skip slot for return value.
        skip();

        // Skip slot for arguments object.
        if (script->argumentsHasVarBinding())
            skip();

        if (thisv)
            *thisv = read();
        else
            skip();

        unsigned i = 0;
        if (end < start)
            i = start;

        for (; i < start; i++)
            skip();
        for (; i < end; i++) {
            // We are not always able to read values from the snapshots, some values
            // such as non-gc things may still be live in registers and cause an
            // error while reading the machine state.
            Value v = maybeRead();
            op(v);
        }
    }

    Value maybeReadAllocByIndex(size_t index) {
        while (index--) {
            JS_ASSERT(moreAllocations());
            skip();
        }

        Value s = maybeRead(true);

        while (moreAllocations())
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
    uint32_t framesRead_;

    // When the inline-frame-iterator is created, this variable is defined to
    // UINT32_MAX. Then the first iteration of findNextFrame, which settle on
    // the innermost frame, is used to update this counter to the number of
    // frames contained in the recover buffer.
    uint32_t frameCount_;

    typename MaybeRooted<JSFunction*, allowGC>::RootType callee_;
    typename MaybeRooted<JSScript*, allowGC>::RootType script_;
    jsbytecode *pc_;
    uint32_t numActualArgs_;

    struct Nop {
        void operator()(const Value &v) { }
    };

  private:
    void findNextFrame();

  public:
    InlineFrameIteratorMaybeGC(JSContext *cx, const IonFrameIterator *iter)
      : callee_(cx),
        script_(cx)
    {
        resetOn(iter);
    }

    InlineFrameIteratorMaybeGC(JSRuntime *rt, const IonFrameIterator *iter)
      : callee_(rt),
        script_(rt)
    {
        resetOn(iter);
    }

    InlineFrameIteratorMaybeGC(JSContext *cx, const IonBailoutIterator *iter);

    InlineFrameIteratorMaybeGC(JSContext *cx, const InlineFrameIteratorMaybeGC *iter)
      : frame_(iter ? iter->frame_ : nullptr),
        framesRead_(0),
        frameCount_(iter ? iter->frameCount_ : UINT32_MAX),
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
        return frame_ && framesRead_ < frameCount_;
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

    template <class ArgOp, class LocalOp>
    void readFrameArgsAndLocals(JSContext *cx, ArgOp &argOp, LocalOp &localOp,
                                Value *scopeChain, Value *thisv,
                                ReadFrameArgsBehavior behavior) const
    {
        unsigned nactual = numActualArgs();
        unsigned nformal = callee()->nargs();

        // Get the non overflown arguments, which are taken from the inlined
        // frame, because it will have the updated value when JSOP_SETARG is
        // done.
        SnapshotIterator s(si_);
        if (behavior != ReadFrame_Overflown)
            s.readFrameArgs(argOp, scopeChain, thisv, 0, nformal, script());

        if (behavior != ReadFrame_Formals) {
            if (more()) {
                // There is still a parent frame of this inlined frame.  All
                // arguments (also the overflown) are the last pushed values
                // in the parent frame.  To get the overflown arguments, we
                // need to take them from there.

                // The overflown arguments are not available in current frame.
                // They are the last pushed arguments in the parent frame of
                // this inlined frame.
                InlineFrameIteratorMaybeGC it(cx, this);
                ++it;
                unsigned argsObjAdj = it.script()->argumentsHasVarBinding() ? 1 : 0;
                SnapshotIterator parent_s(it.snapshotIterator());

                // Skip over all slots until we get to the last slots
                // (= arguments slots of callee) the +3 is for [this], [returnvalue],
                // [scopechain], and maybe +1 for [argsObj]
                JS_ASSERT(parent_s.numAllocations() >= nactual + 3 + argsObjAdj);
                unsigned skip = parent_s.numAllocations() - nactual - 3 - argsObjAdj;
                for (unsigned j = 0; j < skip; j++)
                    parent_s.skip();

                // Get the overflown arguments
                parent_s.readFrameArgs(argOp, nullptr, nullptr, nformal, nactual, it.script());
            } else {
                // There is no parent frame to this inlined frame, we can read
                // from the frame's Value vector directly.
                Value *argv = frame_->actualArgs();
                for (unsigned i = nformal; i < nactual; i++)
                    argOp(argv[i]);
            }
        }

        // At this point we've read all the formals in s, and can read the
        // locals.
        for (unsigned i = 0; i < script()->nfixed(); i++)
            localOp(s.read());
    }

    template <class Op>
    void unaliasedForEachActual(JSContext *cx, Op op, ReadFrameArgsBehavior behavior) const {
        Nop nop;
        readFrameArgsAndLocals(cx, op, nop, nullptr, nullptr, behavior);
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
        if (v.isObject())
            return &v.toObject();

        return callee()->environment();
    }

    JSObject *thisObject() const {
        // JS_ASSERT(isConstructing(...));
        SnapshotIterator s(si_);

        // scopeChain
        s.skip();

        // return value
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

    const IonFrameIterator &frame() const {
        return *frame_;
    }

    // Inline frame number, 0 for the outermost (non-inlined) frame.
    size_t frameNo() const {
        MOZ_ASSERT(frameCount_ != UINT32_MAX);
        return frameCount_ - framesRead_;
    }

  private:
    InlineFrameIteratorMaybeGC() MOZ_DELETE;
    InlineFrameIteratorMaybeGC(const InlineFrameIteratorMaybeGC &iter) MOZ_DELETE;
};
typedef InlineFrameIteratorMaybeGC<CanGC> InlineFrameIterator;
typedef InlineFrameIteratorMaybeGC<NoGC> InlineFrameIteratorNoGC;

} // namespace jit
} // namespace js

#endif // JS_ION

#endif /* jit_IonFrameIterator_h */
