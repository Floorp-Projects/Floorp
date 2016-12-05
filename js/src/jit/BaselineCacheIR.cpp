/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineCacheIR.h"

#include "jit/CacheIR.h"
#include "jit/Linker.h"
#include "jit/SharedICHelpers.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

// OperandLocation represents the location of an OperandId. The operand is
// either in a register or on the stack, and is either boxed or unboxed.
class OperandLocation
{
  public:
    enum Kind {
        Uninitialized = 0,
        PayloadReg,
        ValueReg,
        PayloadStack,
        ValueStack,
    };

  private:
    Kind kind_;

    union Data {
        struct {
            Register reg;
            JSValueType type;
        } payloadReg;
        ValueOperand valueReg;
        struct {
            uint32_t stackPushed;
            JSValueType type;
        } payloadStack;
        uint32_t valueStackPushed;

        Data() : valueStackPushed(0) {}
    };
    Data data_;

  public:
    OperandLocation() : kind_(Uninitialized) {}

    Kind kind() const { return kind_; }

    void setUninitialized() {
        kind_ = Uninitialized;
    }

    ValueOperand valueReg() const {
        MOZ_ASSERT(kind_ == ValueReg);
        return data_.valueReg;
    }
    Register payloadReg() const {
        MOZ_ASSERT(kind_ == PayloadReg);
        return data_.payloadReg.reg;
    }
    uint32_t payloadStack() const {
        MOZ_ASSERT(kind_ == PayloadStack);
        return data_.payloadStack.stackPushed;
    }
    uint32_t valueStack() const {
        MOZ_ASSERT(kind_ == ValueStack);
        return data_.valueStackPushed;
    }
    JSValueType payloadType() const {
        if (kind_ == PayloadReg)
            return data_.payloadReg.type;
        MOZ_ASSERT(kind_ == PayloadStack);
        return data_.payloadStack.type;
    }
    void setPayloadReg(Register reg, JSValueType type) {
        kind_ = PayloadReg;
        data_.payloadReg.reg = reg;
        data_.payloadReg.type = type;
    }
    void setValueReg(ValueOperand reg) {
        kind_ = ValueReg;
        data_.valueReg = reg;
    }
    void setPayloadStack(uint32_t stackPushed, JSValueType type) {
        kind_ = PayloadStack;
        data_.payloadStack.stackPushed = stackPushed;
        data_.payloadStack.type = type;
    }
    void setValueStack(uint32_t stackPushed) {
        kind_ = ValueStack;
        data_.valueStackPushed = stackPushed;
    }

    bool aliasesReg(Register reg) {
        if (kind_ == PayloadReg)
            return payloadReg() == reg;
        if (kind_ == ValueReg)
            return valueReg().aliases(reg);
        return false;
    }
    bool aliasesReg(ValueOperand reg) {
#if defined(JS_NUNBOX32)
        return aliasesReg(reg.typeReg()) || aliasesReg(reg.payloadReg());
#else
        return aliasesReg(reg.valueReg());
#endif
    }

    bool operator==(const OperandLocation& other) const {
        if (kind_ != other.kind_)
            return false;
        switch (kind()) {
          case Uninitialized:
            return true;
          case PayloadReg:
            return payloadReg() == other.payloadReg() && payloadType() == other.payloadType();
          case ValueReg:
            return valueReg() == other.valueReg();
          case PayloadStack:
            return payloadStack() == other.payloadStack() && payloadType() == other.payloadType();
          case ValueStack:
            return valueStack() == other.valueStack();
        }
        MOZ_CRASH("Invalid OperandLocation kind");
    }
    bool operator!=(const OperandLocation& other) const { return !operator==(other); }
};

// Class to track and allocate registers while emitting IC code.
class MOZ_RAII CacheRegisterAllocator
{
    // The original location of the inputs to the cache.
    Vector<OperandLocation, 4, SystemAllocPolicy> origInputLocations_;

    // The current location of each operand.
    Vector<OperandLocation, 8, SystemAllocPolicy> operandLocations_;

    // The registers allocated while emitting the current CacheIR op.
    // This prevents us from allocating a register and then immediately
    // clobbering it for something else, while we're still holding on to it.
    LiveGeneralRegisterSet currentOpRegs_;

    const AllocatableGeneralRegisterSet allocatableRegs_;

    // Registers that are currently unused and available.
    AllocatableGeneralRegisterSet availableRegs_;

    // The number of bytes pushed on the native stack.
    uint32_t stackPushed_;

    // The index of the CacheIR instruction we're currently emitting.
    uint32_t currentInstruction_;

    const CacheIRWriter& writer_;

    CacheRegisterAllocator(const CacheRegisterAllocator&) = delete;
    CacheRegisterAllocator& operator=(const CacheRegisterAllocator&) = delete;

    void freeDeadOperandRegisters();

  public:
    friend class AutoScratchRegister;
    friend class AutoScratchRegisterExcluding;

    explicit CacheRegisterAllocator(const CacheIRWriter& writer)
      : allocatableRegs_(GeneralRegisterSet::All()),
        stackPushed_(0),
        currentInstruction_(0),
        writer_(writer)
    {}

    MOZ_MUST_USE bool init(const AllocatableGeneralRegisterSet& available) {
        availableRegs_ = available;
        if (!origInputLocations_.resize(writer_.numInputOperands()))
            return false;
        if (!operandLocations_.resize(writer_.numOperandIds()))
            return false;
        return true;
    }

    OperandLocation operandLocation(size_t i) const {
        return operandLocations_[i];
    }
    OperandLocation origInputLocation(size_t i) const {
        return origInputLocations_[i];
    }
    void initInputLocation(size_t i, ValueOperand reg) {
        origInputLocations_[i].setValueReg(reg);
        operandLocations_[i] = origInputLocations_[i];
    }

    void nextOp() {
        currentOpRegs_.clear();
        currentInstruction_++;
    }

    uint32_t stackPushed() const {
        return stackPushed_;
    }

    bool isAllocatable(Register reg) const {
        return allocatableRegs_.has(reg);
    }

    // Allocates a new register.
    Register allocateRegister(MacroAssembler& masm);
    ValueOperand allocateValueRegister(MacroAssembler& masm);
    void allocateFixedRegister(MacroAssembler& masm, Register reg);

    // Releases a register so it can be reused later.
    void releaseRegister(Register reg) {
        MOZ_ASSERT(currentOpRegs_.has(reg));
        availableRegs_.add(reg);
    }

    // Removes spilled values from the native stack. This should only be
    // called after all registers have been allocated.
    void discardStack(MacroAssembler& masm);

    // Returns the register for the given operand. If the operand is currently
    // not in a register, it will load it into one.
    ValueOperand useValueRegister(MacroAssembler& masm, ValOperandId val);
    Register useRegister(MacroAssembler& masm, TypedOperandId typedId);

    // Allocates an output register for the given operand.
    Register defineRegister(MacroAssembler& masm, TypedOperandId typedId);
    ValueOperand defineValueRegister(MacroAssembler& masm, ValOperandId val);
};

// RAII class to allocate a scratch register and release it when we're done
// with it.
class MOZ_RAII AutoScratchRegister
{
    CacheRegisterAllocator& alloc_;
    Register reg_;

  public:
    AutoScratchRegister(CacheRegisterAllocator& alloc, MacroAssembler& masm,
                        Register reg = InvalidReg)
      : alloc_(alloc)
    {
        if (reg != InvalidReg) {
            alloc.allocateFixedRegister(masm, reg);
            reg_ = reg;
        } else {
            reg_ = alloc.allocateRegister(masm);
        }
        MOZ_ASSERT(alloc_.currentOpRegs_.has(reg_));
    }
    ~AutoScratchRegister() {
        alloc_.releaseRegister(reg_);
    }
    operator Register() const { return reg_; }
};

// Like AutoScratchRegister, but lets the caller specify a register that should
// not be allocated here.
class MOZ_RAII AutoScratchRegisterExcluding
{
    CacheRegisterAllocator& alloc_;
    Register reg_;

  public:
    AutoScratchRegisterExcluding(CacheRegisterAllocator& alloc, MacroAssembler& masm,
                                 Register excluding)
      : alloc_(alloc)
    {
        MOZ_ASSERT(excluding != InvalidReg);

        reg_ = alloc.allocateRegister(masm);

        if (reg_ == excluding) {
            // We need a different register, so try again.
            reg_ = alloc.allocateRegister(masm);
            MOZ_ASSERT(reg_ != excluding);
            alloc_.releaseRegister(excluding);
        }

        MOZ_ASSERT(alloc_.currentOpRegs_.has(reg_));
    }
    ~AutoScratchRegisterExcluding() {
        alloc_.releaseRegister(reg_);
    }
    operator Register() const { return reg_; }
};

// The FailurePath class stores everything we need to generate a failure path
// at the end of the IC code. The failure path restores the input registers, if
// needed, and jumps to the next stub.
class FailurePath
{
    Vector<OperandLocation, 4, SystemAllocPolicy> inputs_;
    NonAssertingLabel label_;
    uint32_t stackPushed_;

  public:
    FailurePath() = default;

    FailurePath(FailurePath&& other)
      : inputs_(Move(other.inputs_)),
        label_(other.label_),
        stackPushed_(other.stackPushed_)
    {}

    Label* label() { return &label_; }

    void setStackPushed(uint32_t i) { stackPushed_ = i; }
    uint32_t stackPushed() const { return stackPushed_; }

    bool appendInput(OperandLocation loc) {
        return inputs_.append(loc);
    }
    OperandLocation input(size_t i) const {
        return inputs_[i];
    }

    // If canShareFailurePath(other) returns true, the same machine code will
    // be emitted for two failure paths, so we can share them.
    bool canShareFailurePath(const FailurePath& other) const {
        if (stackPushed_ != other.stackPushed_)
            return false;

        MOZ_ASSERT(inputs_.length() == other.inputs_.length());

        for (size_t i = 0; i < inputs_.length(); i++) {
            if (inputs_[i] != other.inputs_[i])
                return false;
        }
        return true;
    }
};

// Base class for BaselineCacheIRCompiler and IonCacheIRCompiler.
class MOZ_RAII CacheIRCompiler
{
  protected:
    JSContext* cx_;
    CacheIRReader reader;
    const CacheIRWriter& writer_;
    MacroAssembler masm;

    CacheRegisterAllocator allocator;
    Vector<FailurePath, 4, SystemAllocPolicy> failurePaths;

    CacheIRCompiler(JSContext* cx, const CacheIRWriter& writer)
      : cx_(cx),
        reader(writer),
        writer_(writer),
        allocator(writer_)
    {}

    void emitFailurePath(size_t i);
};

void
CacheIRCompiler::emitFailurePath(size_t i)
{
    FailurePath& failure = failurePaths[i];

    masm.bind(failure.label());

    uint32_t stackPushed = failure.stackPushed();
    size_t numInputOperands = writer_.numInputOperands();

    for (size_t j = 0; j < numInputOperands; j++) {
        OperandLocation orig = allocator.origInputLocation(j);
        OperandLocation cur = failure.input(j);

        MOZ_ASSERT(orig.kind() == OperandLocation::ValueReg);

        // We have a cycle if a destination register will be used later
        // as source register. If that happens, just push the current value
        // on the stack and later get it from there.
        for (size_t k = j + 1; k < numInputOperands; k++) {
            OperandLocation laterSource = failure.input(k);
            switch (laterSource.kind()) {
              case OperandLocation::ValueReg:
                if (orig.aliasesReg(laterSource.valueReg())) {
                    stackPushed += sizeof(js::Value);
                    masm.pushValue(laterSource.valueReg());
                    laterSource.setValueStack(stackPushed);
                }
                break;
              case OperandLocation::PayloadReg:
                if (orig.aliasesReg(laterSource.payloadReg())) {
                    stackPushed += sizeof(uintptr_t);
                    masm.push(laterSource.payloadReg());
                    laterSource.setPayloadStack(stackPushed, laterSource.payloadType());
                }
                break;
              case OperandLocation::PayloadStack:
              case OperandLocation::ValueStack:
              case OperandLocation::Uninitialized:
                break;
            }
        }

        switch (cur.kind()) {
          case OperandLocation::ValueReg:
            masm.moveValue(cur.valueReg(), orig.valueReg());
            break;
          case OperandLocation::PayloadReg:
            masm.tagValue(cur.payloadType(), cur.payloadReg(), orig.valueReg());
            break;
          case OperandLocation::PayloadStack: {
            MOZ_ASSERT(stackPushed >= sizeof(uintptr_t));
            Register scratch = orig.valueReg().scratchReg();
            if (cur.payloadStack() == stackPushed) {
                masm.pop(scratch);
                stackPushed -= sizeof(uintptr_t);
            } else {
                MOZ_ASSERT(cur.payloadStack() < stackPushed);
                masm.loadPtr(Address(masm.getStackPointer(), stackPushed - cur.payloadStack()),
                             scratch);
            }
            masm.tagValue(cur.payloadType(), scratch, orig.valueReg());
            break;
          }
          case OperandLocation::ValueStack:
            MOZ_ASSERT(stackPushed >= sizeof(js::Value));
            if (cur.valueStack() == stackPushed) {
                masm.popValue(orig.valueReg());
                stackPushed -= sizeof(js::Value);
            } else {
                MOZ_ASSERT(cur.valueStack() < stackPushed);
                masm.loadValue(Address(masm.getStackPointer(), stackPushed - cur.valueStack()),
                               orig.valueReg());
            }
            break;
          default:
            MOZ_CRASH();
        }
    }

    allocator.discardStack(masm);
}

// BaselineCacheIRCompiler compiles CacheIR to BaselineIC native code.
class MOZ_RAII BaselineCacheIRCompiler : public CacheIRCompiler
{
    // Some Baseline IC stubs can be used in IonMonkey through SharedStubs.
    // Those stubs have different machine code, so we need to track whether
    // we're compiling for Baseline or Ion.
    ICStubEngine engine_;

    uint32_t stubDataOffset_;
    bool inStubFrame_;
    bool makesGCCalls_;

    MOZ_MUST_USE bool callVM(MacroAssembler& masm, const VMFunction& fun);

  public:
    friend class AutoStubFrame;

    BaselineCacheIRCompiler(JSContext* cx, const CacheIRWriter& writer, ICStubEngine engine,
                            uint32_t stubDataOffset)
      : CacheIRCompiler(cx, writer),
        engine_(engine),
        stubDataOffset_(stubDataOffset),
        inStubFrame_(false),
        makesGCCalls_(false)
    {}

    MOZ_MUST_USE bool init(CacheKind kind);

    JitCode* compile();

    bool makesGCCalls() const { return makesGCCalls_; }

  private:
#define DEFINE_OP(op) MOZ_MUST_USE bool emit##op();
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

    Address stubAddress(uint32_t offset) const {
        return Address(ICStubReg, stubDataOffset_ + offset);
    }

    bool addFailurePath(FailurePath** failure) {
        FailurePath newFailure;
        for (size_t i = 0; i < writer_.numInputOperands(); i++) {
            if (!newFailure.appendInput(allocator.operandLocation(i)))
                return false;
        }
        newFailure.setStackPushed(allocator.stackPushed());

        // Reuse the previous failure path if the current one is the same, to
        // avoid emitting duplicate code.
        if (failurePaths.length() > 0 && failurePaths.back().canShareFailurePath(newFailure)) {
            *failure = &failurePaths.back();
            return true;
        }

        if (!failurePaths.append(Move(newFailure)))
            return false;

        *failure = &failurePaths.back();
        return true;
    }
};

// Instructions that have to perform a callVM require a stub frame. Use
// AutoStubFrame before allocating any registers, then call its enter() and
// leave() methods to enter/leave the stub frame.
class MOZ_RAII AutoStubFrame
{
    BaselineCacheIRCompiler& compiler;
#ifdef DEBUG
    uint32_t framePushedAtEnterStubFrame_;
#endif
    Maybe<AutoScratchRegister> tail;

    AutoStubFrame(const AutoStubFrame&) = delete;
    void operator=(const AutoStubFrame&) = delete;

  public:
    explicit AutoStubFrame(BaselineCacheIRCompiler& compiler)
      : compiler(compiler),
#ifdef DEBUG
        framePushedAtEnterStubFrame_(0),
#endif
        tail()
    {
        // We use ICTailCallReg when entering the stub frame, so ensure it's not
        // used for something else.
        if (compiler.allocator.isAllocatable(ICTailCallReg))
            tail.emplace(compiler.allocator, compiler.masm, ICTailCallReg);
    }

    void enter(MacroAssembler& masm, Register scratch) {
        if (compiler.engine_ == ICStubEngine::Baseline) {
            EmitBaselineEnterStubFrame(masm, scratch);
#ifdef DEBUG
            framePushedAtEnterStubFrame_ = masm.framePushed();
#endif
        } else {
            EmitIonEnterStubFrame(masm, scratch);
        }

        MOZ_ASSERT(!compiler.inStubFrame_);
        compiler.inStubFrame_ = true;
        compiler.makesGCCalls_ = true;
    }
    void leave(MacroAssembler& masm, bool calledIntoIon = false) {
        MOZ_ASSERT(compiler.inStubFrame_);
        compiler.inStubFrame_ = false;

        if (compiler.engine_ == ICStubEngine::Baseline) {
#ifdef DEBUG
            masm.setFramePushed(framePushedAtEnterStubFrame_);
            if (calledIntoIon)
                masm.adjustFrame(sizeof(intptr_t)); // Calls into ion have this extra.
#endif

            EmitBaselineLeaveStubFrame(masm, calledIntoIon);
        } else {
            EmitIonLeaveStubFrame(masm);
        }
    }

#ifdef DEBUG
    ~AutoStubFrame() {
        MOZ_ASSERT(!compiler.inStubFrame_);
    }
#endif
};

bool
BaselineCacheIRCompiler::callVM(MacroAssembler& masm, const VMFunction& fun)
{
    MOZ_ASSERT(inStubFrame_);

    JitCode* code = cx_->jitRuntime()->getVMWrapper(fun);
    if (!code)
        return false;

    MOZ_ASSERT(fun.expectTailCall == NonTailCall);
    if (engine_ == ICStubEngine::Baseline)
        EmitBaselineCallVM(code, masm);
    else
        EmitIonCallVM(code, fun.explicitStackSlots(), masm);
    return true;
}

JitCode*
BaselineCacheIRCompiler::compile()
{
#ifndef JS_USE_LINK_REGISTER
    // The first value contains the return addres,
    // which we pull into ICTailCallReg for tail calls.
    masm.adjustFrame(sizeof(intptr_t));
#endif
#ifdef JS_CODEGEN_ARM
    masm.setSecondScratchReg(BaselineSecondScratchReg);
#endif

    do {
        switch (reader.readOp()) {
#define DEFINE_OP(op)                   \
          case CacheOp::op:             \
            if (!emit##op())            \
                return nullptr;         \
            break;
    CACHE_IR_OPS(DEFINE_OP)
#undef DEFINE_OP

          default:
            MOZ_CRASH("Invalid op");
        }

        allocator.nextOp();
    } while (reader.more());

    MOZ_ASSERT(!inStubFrame_);
    masm.assumeUnreachable("Should have returned from IC");

    // Done emitting the main IC code. Now emit the failure paths.
    for (size_t i = 0; i < failurePaths.length(); i++) {
        emitFailurePath(i);
        EmitStubGuardFailure(masm);
    }

    Linker linker(masm);
    AutoFlushICache afc("getStubCode");
    Rooted<JitCode*> newStubCode(cx_, linker.newCode<NoGC>(cx_, BASELINE_CODE));
    if (!newStubCode) {
        cx_->recoverFromOutOfMemory();
        return nullptr;
    }

    // All barriers are emitted off-by-default, enable them if needed.
    if (cx_->zone()->needsIncrementalBarrier())
        newStubCode->togglePreBarriers(true, DontReprotect);

    return newStubCode;
}

ValueOperand
CacheRegisterAllocator::useValueRegister(MacroAssembler& masm, ValOperandId op)
{
    OperandLocation& loc = operandLocations_[op.id()];

    switch (loc.kind()) {
      case OperandLocation::ValueReg:
        currentOpRegs_.add(loc.valueReg());
        return loc.valueReg();

      case OperandLocation::ValueStack: {
        // The Value is on the stack. If it's on top of the stack, unbox and
        // then pop it. If we need the registers later, we can always spill
        // back. If it's not on the top of the stack, just unbox.
        ValueOperand reg = allocateValueRegister(masm);
        if (loc.valueStack() == stackPushed_) {
            masm.popValue(reg);
            MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));
            stackPushed_ -= sizeof(js::Value);
        } else {
            MOZ_ASSERT(loc.valueStack() < stackPushed_);
            masm.loadValue(Address(masm.getStackPointer(), stackPushed_ - loc.valueStack()), reg);
        }
        loc.setValueReg(reg);
        return reg;
      }

      // The operand should never be unboxed.
      case OperandLocation::PayloadStack:
      case OperandLocation::PayloadReg:
      case OperandLocation::Uninitialized:
        break;
    }

    MOZ_CRASH();
}

Register
CacheRegisterAllocator::useRegister(MacroAssembler& masm, TypedOperandId typedId)
{
    OperandLocation& loc = operandLocations_[typedId.id()];
    switch (loc.kind()) {
      case OperandLocation::PayloadReg:
        currentOpRegs_.add(loc.payloadReg());
        return loc.payloadReg();

      case OperandLocation::ValueReg: {
        // It's possible the value is still boxed: as an optimization, we unbox
        // the first time we use a value as object.
        ValueOperand val = loc.valueReg();
        availableRegs_.add(val);
        Register reg = val.scratchReg();
        availableRegs_.take(reg);
        masm.unboxObject(val, reg);
        loc.setPayloadReg(reg, typedId.type());
        currentOpRegs_.add(reg);
        return reg;
      }

      case OperandLocation::PayloadStack: {
        // The payload is on the stack. If it's on top of the stack we can just
        // pop it, else we emit a load.
        Register reg = allocateRegister(masm);
        if (loc.payloadStack() == stackPushed_) {
            masm.pop(reg);
            MOZ_ASSERT(stackPushed_ >= sizeof(uintptr_t));
            stackPushed_ -= sizeof(uintptr_t);
        } else {
            MOZ_ASSERT(loc.payloadStack() < stackPushed_);
            masm.loadPtr(Address(masm.getStackPointer(), stackPushed_ - loc.payloadStack()), reg);
        }
        loc.setPayloadReg(reg, loc.payloadType());
        return reg;
      }

      case OperandLocation::ValueStack: {
        // The value is on the stack, but boxed. If it's on top of the stack we
        // unbox it and then remove it from the stack, else we just unbox.
        Register reg = allocateRegister(masm);
        if (loc.valueStack() == stackPushed_) {
            masm.unboxObject(Address(masm.getStackPointer(), 0), reg);
            masm.addToStackPtr(Imm32(sizeof(js::Value)));
            MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));
            stackPushed_ -= sizeof(js::Value);
        } else {
            MOZ_ASSERT(loc.valueStack() < stackPushed_);
            masm.unboxObject(Address(masm.getStackPointer(), stackPushed_ - loc.valueStack()),
                             reg);
        }
        loc.setPayloadReg(reg, typedId.type());
        return reg;
      }

      case OperandLocation::Uninitialized:
        break;
    }

    MOZ_CRASH();
}

Register
CacheRegisterAllocator::defineRegister(MacroAssembler& masm, TypedOperandId typedId)
{
    OperandLocation& loc = operandLocations_[typedId.id()];
    MOZ_ASSERT(loc.kind() == OperandLocation::Uninitialized);

    Register reg = allocateRegister(masm);
    loc.setPayloadReg(reg, typedId.type());
    return reg;
}

ValueOperand
CacheRegisterAllocator::defineValueRegister(MacroAssembler& masm, ValOperandId val)
{
    OperandLocation& loc = operandLocations_[val.id()];
    MOZ_ASSERT(loc.kind() == OperandLocation::Uninitialized);

    ValueOperand reg = allocateValueRegister(masm);
    loc.setValueReg(reg);
    return reg;
}

void
CacheRegisterAllocator::freeDeadOperandRegisters()
{
    // See if any operands are dead so we can reuse their registers. Note that
    // we skip the input operands, as those are also used by failure paths, and
    // we currently don't track those uses.
    for (size_t i = writer_.numInputOperands(); i < operandLocations_.length(); i++) {
        if (!writer_.operandIsDead(i, currentInstruction_))
            continue;

        OperandLocation& loc = operandLocations_[i];
        switch (loc.kind()) {
          case OperandLocation::PayloadReg:
            availableRegs_.add(loc.payloadReg());
            break;
          case OperandLocation::ValueReg:
            availableRegs_.add(loc.valueReg());
            break;
          case OperandLocation::Uninitialized:
          case OperandLocation::PayloadStack:
          case OperandLocation::ValueStack:
            break;
        }
        loc.setUninitialized();
    }
}

void
CacheRegisterAllocator::discardStack(MacroAssembler& masm)
{
    // This should only be called when we are no longer using the operands,
    // as we're discarding everything from the native stack. Set all operand
    // locations to Uninitialized to catch bugs.
    for (size_t i = 0; i < operandLocations_.length(); i++)
        operandLocations_[i].setUninitialized();

    if (stackPushed_ > 0) {
        masm.addToStackPtr(Imm32(stackPushed_));
        stackPushed_ = 0;
    }
}

Register
CacheRegisterAllocator::allocateRegister(MacroAssembler& masm)
{
    if (availableRegs_.empty())
        freeDeadOperandRegisters();

    if (availableRegs_.empty()) {
        // Still no registers available, try to spill unused operands to
        // the stack.
        for (size_t i = 0; i < operandLocations_.length(); i++) {
            OperandLocation& loc = operandLocations_[i];
            if (loc.kind() == OperandLocation::PayloadReg) {
                Register reg = loc.payloadReg();
                if (currentOpRegs_.has(reg))
                    continue;

                masm.push(reg);
                stackPushed_ += sizeof(uintptr_t);
                loc.setPayloadStack(stackPushed_, loc.payloadType());
                availableRegs_.add(reg);
                break; // We got a register, so break out of the loop.
            }
            if (loc.kind() == OperandLocation::ValueReg) {
                ValueOperand reg = loc.valueReg();
                if (currentOpRegs_.aliases(reg))
                    continue;

                masm.pushValue(reg);
                stackPushed_ += sizeof(js::Value);
                loc.setValueStack(stackPushed_);
                availableRegs_.add(reg);
                break; // Break out of the loop.
            }
        }
    }

    // At this point, there must be a free register. (Ion ICs don't have as
    // many registers available, so once we support Ion code generation, we may
    // have to spill some unrelated registers.)
    MOZ_RELEASE_ASSERT(!availableRegs_.empty());

    Register reg = availableRegs_.takeAny();
    currentOpRegs_.add(reg);
    return reg;
}

void
CacheRegisterAllocator::allocateFixedRegister(MacroAssembler& masm, Register reg)
{
    // Fixed registers should be allocated first, to ensure they're
    // still available.
    MOZ_ASSERT(!currentOpRegs_.has(reg), "Register is in use");

    freeDeadOperandRegisters();

    if (availableRegs_.has(reg)) {
        availableRegs_.take(reg);
        currentOpRegs_.add(reg);
        return;
    }

    // The register must be used by some operand. Spill it to the stack.
    for (size_t i = 0; i < operandLocations_.length(); i++) {
        OperandLocation& loc = operandLocations_[i];
        if (loc.kind() == OperandLocation::PayloadReg) {
            if (loc.payloadReg() != reg)
                continue;

            masm.push(reg);
            stackPushed_ += sizeof(uintptr_t);
            loc.setPayloadStack(stackPushed_, loc.payloadType());
            currentOpRegs_.add(reg);
            return;
        }
        if (loc.kind() == OperandLocation::ValueReg) {
            if (!loc.valueReg().aliases(reg))
                continue;

            masm.pushValue(loc.valueReg());
            stackPushed_ += sizeof(js::Value);
            loc.setValueStack(stackPushed_);
            availableRegs_.add(loc.valueReg());
            availableRegs_.take(reg);
            currentOpRegs_.add(reg);
            return;
        }
    }

    MOZ_CRASH("Invalid register");
}

ValueOperand
CacheRegisterAllocator::allocateValueRegister(MacroAssembler& masm)
{
#ifdef JS_NUNBOX32
    Register reg1 = allocateRegister(masm);
    Register reg2 = allocateRegister(masm);
    return ValueOperand(reg1, reg2);
#else
    Register reg = allocateRegister(masm);
    return ValueOperand(reg);
#endif
}

bool
BaselineCacheIRCompiler::emitGuardIsObject()
{
    ValueOperand input = allocator.useValueRegister(masm, reader.valOperandId());
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;
    masm.branchTestObject(Assembler::NotEqual, input, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardIsString()
{
    ValueOperand input = allocator.useValueRegister(masm, reader.valOperandId());
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;
    masm.branchTestString(Assembler::NotEqual, input, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardType()
{
    ValueOperand input = allocator.useValueRegister(masm, reader.valOperandId());
    JSValueType type = reader.valueType();

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    switch (type) {
      case JSVAL_TYPE_STRING:
        masm.branchTestString(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_SYMBOL:
        masm.branchTestSymbol(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_DOUBLE:
        masm.branchTestNumber(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_BOOLEAN:
        masm.branchTestBoolean(Assembler::NotEqual, input, failure->label());
        break;
      case JSVAL_TYPE_UNDEFINED:
        masm.branchTestUndefined(Assembler::NotEqual, input, failure->label());
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }

    return true;
}

bool
BaselineCacheIRCompiler::emitGuardShape()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadPtr(addr, scratch);
    masm.branchTestObjShape(Assembler::NotEqual, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardGroup()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadPtr(addr, scratch);
    masm.branchTestObjGroup(Assembler::NotEqual, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.loadObjProto(obj, scratch);
    masm.branchPtr(Assembler::NotEqual, addr, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardClass()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    const Class* clasp = nullptr;
    switch (reader.guardClassKind()) {
      case GuardClassKind::Array:
        clasp = &ArrayObject::class_;
        break;
      case GuardClassKind::UnboxedArray:
        clasp = &UnboxedArrayObject::class_;
        break;
      case GuardClassKind::MappedArguments:
        clasp = &MappedArgumentsObject::class_;
        break;
      case GuardClassKind::UnmappedArguments:
        clasp = &UnmappedArgumentsObject::class_;
        break;
      case GuardClassKind::WindowProxy:
        clasp = cx_->maybeWindowProxyClass();
        break;
    }

    MOZ_ASSERT(clasp);
    masm.branchTestObjClass(Assembler::NotEqual, obj, scratch, clasp, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardIsProxy()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestObjectIsProxy(false, obj, scratch, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNotDOMProxy()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestProxyHandlerFamily(Assembler::Equal, obj, scratch,
                                      GetDOMProxyHandlerFamily(), failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardSpecificObject()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address addr(stubAddress(reader.stubOffset()));
    masm.branchPtr(Assembler::NotEqual, addr, obj, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardMagicValue()
{
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    JSWhyMagic magic = reader.whyMagic();

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTestMagicValue(Assembler::NotEqual, val, magic, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNoUnboxedExpando()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address expandoAddr(obj, UnboxedPlainObject::offsetOfExpando());
    masm.branchPtr(Assembler::NotEqual, expandoAddr, ImmWord(0), failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardAndLoadUnboxedExpando()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register output = allocator.defineRegister(masm, reader.objOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Address expandoAddr(obj, UnboxedPlainObject::offsetOfExpando());
    masm.loadPtr(expandoAddr, output);
    masm.branchTestPtr(Assembler::Zero, output, output, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFixedSlotResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadValue(BaseIndex(obj, scratch, TimesOne), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDynamicSlotResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    // We're about to return, so it's safe to clobber obj now.
    masm.load32(stubAddress(reader.stubOffset()), scratch);
    masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), obj);
    masm.loadValue(BaseIndex(obj, scratch, TimesOne), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitCallScriptedGetterResult()
{
    MOZ_ASSERT(engine_ == ICStubEngine::Baseline);

    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address getterAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegisterExcluding code(allocator, masm, ArgumentsRectifierReg);
    AutoScratchRegister callee(allocator, masm);
    AutoScratchRegister scratch(allocator, masm);

    // First, ensure our getter is non-lazy and has JIT code.
    {
        FailurePath* failure;
        if (!addFailurePath(&failure))
            return false;

        masm.loadPtr(getterAddr, callee);
        masm.branchIfFunctionHasNoScript(callee, failure->label());
        masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), code);
        masm.loadBaselineOrIonRaw(code, code, failure->label());
    }

    allocator.discardStack(masm);

    stubFrame.enter(masm, scratch);

    // Align the stack such that the JitFrameLayout is aligned on
    // JitStackAlignment.
    masm.alignJitStackBasedOnNArgs(0);

    // Getter is called with 0 arguments, just |obj| as thisv.
    // Note that we use Push, not push, so that callJit will align the stack
    // properly on ARM.
    masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));

    EmitBaselineCreateStubFrameDescriptor(masm, scratch, JitFrameLayout::Size());
    masm.Push(Imm32(0));  // ActualArgc is 0
    masm.Push(callee);
    masm.Push(scratch);

    // Handle arguments underflow.
    Label noUnderflow;
    masm.load16ZeroExtend(Address(callee, JSFunction::offsetOfNargs()), callee);
    masm.branch32(Assembler::Equal, callee, Imm32(0), &noUnderflow);
    {
        // Call the arguments rectifier.
        MOZ_ASSERT(ArgumentsRectifierReg != code);

        JitCode* argumentsRectifier = cx_->runtime()->jitRuntime()->getArgumentsRectifier();
        masm.movePtr(ImmGCPtr(argumentsRectifier), code);
        masm.loadPtr(Address(code, JitCode::offsetOfCode()), code);
        masm.movePtr(ImmWord(0), ArgumentsRectifierReg);
    }

    masm.bind(&noUnderflow);
    masm.callJit(code);

    stubFrame.leave(masm, true);
    return true;
}

typedef bool (*DoCallNativeGetterFn)(JSContext*, HandleFunction, HandleObject, MutableHandleValue);
static const VMFunction DoCallNativeGetterInfo =
    FunctionInfo<DoCallNativeGetterFn>(DoCallNativeGetter, "DoCallNativeGetter");

bool
BaselineCacheIRCompiler::emitCallNativeGetterResult()
{
    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address getterAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    stubFrame.enter(masm, scratch);

    // Load the callee in the scratch register.
    masm.loadPtr(getterAddr, scratch);

    masm.Push(obj);
    masm.Push(scratch);

    if (!callVM(masm, DoCallNativeGetterInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

typedef bool (*ProxyGetPropertyFn)(JSContext*, HandleObject, HandleId, MutableHandleValue);
static const VMFunction ProxyGetPropertyInfo =
    FunctionInfo<ProxyGetPropertyFn>(ProxyGetProperty, "ProxyGetProperty");

bool
BaselineCacheIRCompiler::emitCallProxyGetResult()
{
    AutoStubFrame stubFrame(*this);

    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address idAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);

    allocator.discardStack(masm);

    stubFrame.enter(masm, scratch);

    // Load the jsid in the scratch register.
    masm.loadPtr(idAddr, scratch);

    masm.Push(scratch);
    masm.Push(obj);

    if (!callVM(masm, ProxyGetPropertyInfo))
        return false;

    stubFrame.leave(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedPropertyResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    JSValueType fieldType = reader.valueType();
    Address fieldOffset(stubAddress(reader.stubOffset()));
    masm.load32(fieldOffset, scratch);
    masm.loadUnboxedProperty(BaseIndex(obj, scratch, TimesOne), fieldType, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardNoDetachedTypedObjects()
{
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    CheckForTypedObjectWithDetachedStorage(cx_, masm, failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardFrameHasNoArgumentsObject()
{
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.branchTest32(Assembler::NonZero,
                      Address(BaselineFrameReg, BaselineFrame::reverseOffsetOfFlags()),
                      Imm32(BaselineFrame::HAS_ARGS_OBJ),
                      failure->label());
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameCalleeResult()
{
    Address callee(BaselineFrameReg, BaselineFrame::offsetOfCalleeToken());
    masm.loadFunctionFromCalleeToken(callee, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_OBJECT, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadFrameNumActualArgsResult()
{
    Address actualArgs(BaselineFrameReg, BaselineFrame::offsetOfNumActualArgs());
    masm.loadPtr(actualArgs, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadTypedObjectResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch1(allocator, masm);
    AutoScratchRegister scratch2(allocator, masm);

    TypedThingLayout layout = reader.typedThingLayout();
    uint32_t typeDescr = reader.typeDescrKey();
    Address fieldOffset(stubAddress(reader.stubOffset()));

    // Get the object's data pointer.
    LoadTypedThingData(masm, layout, obj, scratch1);

    // Get the address being written to.
    masm.load32(fieldOffset, scratch2);
    masm.addPtr(scratch2, scratch1);

    if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
        Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);
        masm.loadFromTypedArray(type, Address(scratch1, 0), R0, /* allowDouble = */ true,
                                scratch2, nullptr);
    } else {
        ReferenceTypeDescr::Type type = ReferenceTypeFromSimpleTypeDescrKey(typeDescr);
        switch (type) {
          case ReferenceTypeDescr::TYPE_ANY:
            masm.loadValue(Address(scratch1, 0), R0);
            break;

          case ReferenceTypeDescr::TYPE_OBJECT: {
            Label notNull, done;
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.branchTestPtr(Assembler::NonZero, scratch1, scratch1, &notNull);
            masm.moveValue(NullValue(), R0);
            masm.jump(&done);
            masm.bind(&notNull);
            masm.tagValue(JSVAL_TYPE_OBJECT, scratch1, R0);
            masm.bind(&done);
            break;
          }

          case ReferenceTypeDescr::TYPE_STRING:
            masm.loadPtr(Address(scratch1, 0), scratch1);
            masm.tagValue(JSVAL_TYPE_STRING, scratch1, R0);
            break;

          default:
            MOZ_CRASH("Invalid ReferenceTypeDescr");
        }
    }

    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUndefinedResult()
{
    masm.moveValue(UndefinedValue(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadInt32ArrayLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
    masm.load32(Address(scratch, ObjectElements::offsetOfLength()), scratch);

    // Guard length fits in an int32.
    masm.branchTest32(Assembler::Signed, scratch, scratch, failure->label());
    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadUnboxedArrayLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    masm.load32(Address(obj, UnboxedArrayObject::offsetOfLength()), R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadArgumentsObjectLengthResult()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Get initial length value.
    masm.unboxInt32(Address(obj, ArgumentsObject::getInitialLengthSlotOffset()), scratch);

    // Test if length has been overridden.
    masm.branchTest32(Assembler::NonZero,
                      scratch,
                      Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                      failure->label());

    // Shift out arguments length and return it. No need to type monitor
    // because this stub always returns int32.
    masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratch);
    masm.tagValue(JSVAL_TYPE_INT32, scratch, R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadStringLengthResult()
{
    Register str = allocator.useRegister(masm, reader.stringOperandId());
    AutoScratchRegister scratch(allocator, masm);

    masm.loadStringLength(str, R0.scratchReg());
    masm.tagValue(JSVAL_TYPE_INT32, R0.scratchReg(), R0);
    return true;
}

bool
BaselineCacheIRCompiler::emitTypeMonitorResult()
{
    allocator.discardStack(masm);
    EmitEnterTypeMonitorIC(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitReturnFromIC()
{
    allocator.discardStack(masm);
    EmitReturnFromIC(masm);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadObject()
{
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    masm.loadPtr(stubAddress(reader.stubOffset()), reg);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    masm.loadObjProto(obj, reg);
    return true;
}

bool
BaselineCacheIRCompiler::emitLoadDOMExpandoValue()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand val = allocator.defineValueRegister(masm, reader.valOperandId());

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), val.scratchReg());
    masm.loadValue(Address(val.scratchReg(),
                           ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot())),
                   val);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardDOMExpandoObject()
{
    ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
    AutoScratchRegister shapeScratch(allocator, masm);
    AutoScratchRegister objScratch(allocator, masm);
    Address shapeAddr(stubAddress(reader.stubOffset()));

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    Label done;
    masm.branchTestUndefined(Assembler::Equal, val, &done);

    masm.loadPtr(shapeAddr, shapeScratch);
    masm.unboxObject(val, objScratch);
    masm.branchTestObjShape(Assembler::NotEqual, objScratch, shapeScratch, failure->label());

    masm.bind(&done);
    return true;
}

bool
BaselineCacheIRCompiler::emitGuardDOMExpandoGeneration()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Address expandoAndGenerationAddr(stubAddress(reader.stubOffset()));
    Address generationAddr(stubAddress(reader.stubOffset()));

    AutoScratchRegister scratch(allocator, masm);
    ValueOperand output = allocator.defineValueRegister(masm, reader.valOperandId());

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), scratch);
    Address expandoAddr(scratch, ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot()));

    // Load the ExpandoAndGeneration* in the output scratch register and guard
    // it matches the proxy's ExpandoAndGeneration.
    masm.loadPtr(expandoAndGenerationAddr, output.scratchReg());
    masm.branchPrivatePtr(Assembler::NotEqual, expandoAddr, output.scratchReg(), failure->label());

    // Guard expandoAndGeneration->generation matches the expected generation.
    masm.branch64(Assembler::NotEqual,
                  Address(output.scratchReg(), ExpandoAndGeneration::offsetOfGeneration()),
                  generationAddr,
                  scratch, failure->label());

    // Load expandoAndGeneration->expando into the output Value register.
    masm.loadValue(Address(output.scratchReg(), ExpandoAndGeneration::offsetOfExpando()), output);
    return true;
}

bool
BaselineCacheIRCompiler::init(CacheKind kind)
{
    size_t numInputs = writer_.numInputOperands();
    if (!allocator.init(ICStubCompiler::availableGeneralRegs(numInputs)))
        return false;

    MOZ_ASSERT(numInputs == 1);
    allocator.initInputLocation(0, R0);

    return true;
}

template <typename T>
static GCPtr<T>*
AsGCPtr(uintptr_t* ptr)
{
    return reinterpret_cast<GCPtr<T>*>(ptr);
}

template<class T>
GCPtr<T>&
CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const
{
    uint8_t* stubData = (uint8_t*)stub + stubDataOffset_;
    MOZ_ASSERT(uintptr_t(stubData) % sizeof(uintptr_t) == 0);

    return *AsGCPtr<T>((uintptr_t*)(stubData + offset));
}

template GCPtr<Shape*>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;
template GCPtr<ObjectGroup*>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;
template GCPtr<JSObject*>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;

template <typename T, typename V>
static void
InitGCPtr(uintptr_t* ptr, V val)
{
    AsGCPtr<T>(ptr)->init(mozilla::BitwiseCast<T>(val));
}

void
CacheIRWriter::copyStubData(uint8_t* dest) const
{
    uintptr_t* destWords = reinterpret_cast<uintptr_t*>(dest);

    for (size_t i = 0; i < stubFields_.length(); i++) {
        StubField::Type type = stubFields_[i].type();
        switch (type) {
          case StubField::Type::RawWord:
            *destWords = stubFields_[i].asWord();
            break;
          case StubField::Type::Shape:
            InitGCPtr<Shape*>(destWords, stubFields_[i].asWord());
            break;
          case StubField::Type::JSObject:
            InitGCPtr<JSObject*>(destWords, stubFields_[i].asWord());
            break;
          case StubField::Type::ObjectGroup:
            InitGCPtr<ObjectGroup*>(destWords, stubFields_[i].asWord());
            break;
          case StubField::Type::Id:
            InitGCPtr<jsid>(destWords, stubFields_[i].asWord());
            break;
          case StubField::Type::RawInt64:
            *reinterpret_cast<uint64_t*>(destWords) = stubFields_[i].asInt64();
            break;
          case StubField::Type::Value:
            InitGCPtr<JS::Value>(destWords, stubFields_[i].asInt64());
            break;
          case StubField::Type::Limit:
            MOZ_CRASH("Invalid type");
        }
        destWords += StubField::sizeInBytes(type) / sizeof(uintptr_t);
    }
}

HashNumber
CacheIRStubKey::hash(const CacheIRStubKey::Lookup& l)
{
    HashNumber hash = mozilla::HashBytes(l.code, l.length);
    hash = mozilla::AddToHash(hash, uint32_t(l.kind));
    hash = mozilla::AddToHash(hash, uint32_t(l.engine));
    return hash;
}

bool
CacheIRStubKey::match(const CacheIRStubKey& entry, const CacheIRStubKey::Lookup& l)
{
    if (entry.stubInfo->kind() != l.kind)
        return false;

    if (entry.stubInfo->engine() != l.engine)
        return false;

    if (entry.stubInfo->codeLength() != l.length)
        return false;

    if (!mozilla::PodEqual(entry.stubInfo->code(), l.code, l.length))
        return false;

    return true;
}

CacheIRReader::CacheIRReader(const CacheIRStubInfo* stubInfo)
  : CacheIRReader(stubInfo->code(), stubInfo->code() + stubInfo->codeLength())
{}

CacheIRStubInfo*
CacheIRStubInfo::New(CacheKind kind, ICStubEngine engine, bool makesGCCalls,
                     uint32_t stubDataOffset, const CacheIRWriter& writer)
{
    size_t numStubFields = writer.numStubFields();
    size_t bytesNeeded = sizeof(CacheIRStubInfo) +
                         writer.codeLength() +
                         (numStubFields + 1); // +1 for the GCType::Limit terminator.
    uint8_t* p = js_pod_malloc<uint8_t>(bytesNeeded);
    if (!p)
        return nullptr;

    // Copy the CacheIR code.
    uint8_t* codeStart = p + sizeof(CacheIRStubInfo);
    mozilla::PodCopy(codeStart, writer.codeStart(), writer.codeLength());

    static_assert(sizeof(StubField::Type) == sizeof(uint8_t),
                  "StubField::Type must fit in uint8_t");

    // Copy the stub field types.
    uint8_t* fieldTypes = codeStart + writer.codeLength();
    for (size_t i = 0; i < numStubFields; i++)
        fieldTypes[i] = uint8_t(writer.stubFieldType(i));
    fieldTypes[numStubFields] = uint8_t(StubField::Type::Limit);

    return new(p) CacheIRStubInfo(kind, engine, makesGCCalls, stubDataOffset, codeStart,
                                  writer.codeLength(), fieldTypes);
}

static const size_t MaxOptimizedCacheIRStubs = 16;

ICStub*
jit::AttachBaselineCacheIRStub(JSContext* cx, const CacheIRWriter& writer,
                               CacheKind kind, ICStubEngine engine, JSScript* outerScript,
                               ICFallbackStub* stub)
{
    // We shouldn't GC or report OOM (or any other exception) here.
    AutoAssertNoPendingException aanpe(cx);
    JS::AutoCheckCannotGC nogc;

    if (writer.failed())
        return nullptr;

    // Just a sanity check: the caller should ensure we don't attach an
    // unlimited number of stubs.
    MOZ_ASSERT(stub->numOptimizedStubs() < MaxOptimizedCacheIRStubs);

    MOZ_ASSERT(kind == CacheKind::GetProp);
    uint32_t stubDataOffset = sizeof(ICCacheIR_Monitored);

    JitCompartment* jitCompartment = cx->compartment()->jitCompartment();

    // Check if we already have JitCode for this stub.
    CacheIRStubInfo* stubInfo;
    CacheIRStubKey::Lookup lookup(kind, engine, writer.codeStart(), writer.codeLength());
    JitCode* code = jitCompartment->getCacheIRStubCode(lookup, &stubInfo);
    if (!code) {
        // We have to generate stub code.
        JitContext jctx(cx, nullptr);
        BaselineCacheIRCompiler comp(cx, writer, engine, stubDataOffset);
        if (!comp.init(kind))
            return nullptr;

        code = comp.compile();
        if (!code)
            return nullptr;

        // Allocate the shared CacheIRStubInfo. Note that the putCacheIRStubCode
        // call below will transfer ownership to the stub code HashMap, so we
        // don't have to worry about freeing it below.
        MOZ_ASSERT(!stubInfo);
        stubInfo = CacheIRStubInfo::New(kind, engine, comp.makesGCCalls(), stubDataOffset, writer);
        if (!stubInfo)
            return nullptr;

        CacheIRStubKey key(stubInfo);
        if (!jitCompartment->putCacheIRStubCode(lookup, key, code))
            return nullptr;
    }

    // We got our shared stub code and stub info. Time to allocate and attach a
    // new stub.

    MOZ_ASSERT(code);
    MOZ_ASSERT(stubInfo);
    MOZ_ASSERT(stub->isMonitoredFallback());
    MOZ_ASSERT(stubInfo->stubDataSize() == writer.stubDataSize());

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();

    ICStubSpace* stubSpace = ICStubCompiler::StubSpaceForStub(stubInfo->makesGCCalls(),
                                                              outerScript, engine);
    void* newStub = stubSpace->alloc(bytesNeeded);
    if (!newStub)
        return nullptr;

    ICStub* monitorStub = stub->toMonitoredFallbackStub()->fallbackMonitorStub()->firstMonitorStub();
    new(newStub) ICCacheIR_Monitored(code, monitorStub, stubInfo);

    writer.copyStubData((uint8_t*)newStub + stubInfo->stubDataOffset());
    stub->addNewStub((ICStub*)newStub);
    return (ICStub*)newStub;
}

void
jit::TraceBaselineCacheIRStub(JSTracer* trc, ICStub* stub, const CacheIRStubInfo* stubInfo)
{
    uint32_t field = 0;
    size_t offset = 0;
    while (true) {
        StubField::Type fieldType = stubInfo->fieldType(field);
        switch (fieldType) {
          case StubField::Type::RawWord:
          case StubField::Type::RawInt64:
            break;
          case StubField::Type::Shape:
            TraceNullableEdge(trc, &stubInfo->getStubField<Shape*>(stub, offset),
                              "baseline-cacheir-shape");
            break;
          case StubField::Type::ObjectGroup:
            TraceNullableEdge(trc, &stubInfo->getStubField<ObjectGroup*>(stub, offset),
                              "baseline-cacheir-group");
            break;
          case StubField::Type::JSObject:
            TraceNullableEdge(trc, &stubInfo->getStubField<JSObject*>(stub, offset),
                              "baseline-cacheir-object");
            break;
          case StubField::Type::Id:
            TraceEdge(trc, &stubInfo->getStubField<jsid>(stub, offset), "baseline-cacheir-id");
            break;
          case StubField::Type::Value:
            TraceEdge(trc, &stubInfo->getStubField<JS::Value>(stub, offset),
                      "baseline-cacheir-value");
            break;
          case StubField::Type::Limit:
            return; // Done.
        }
        field++;
        offset += StubField::sizeInBytes(fieldType);
    }
}

size_t
CacheIRStubInfo::stubDataSize() const
{
    size_t field = 0;
    size_t size = 0;
    while (true) {
        StubField::Type type = fieldType(field++);
        if (type == StubField::Type::Limit)
            return size;
        size += StubField::sizeInBytes(type);
    }
}

void
CacheIRStubInfo::copyStubData(ICStub* src, ICStub* dest) const
{
    uint8_t* srcBytes = reinterpret_cast<uint8_t*>(src);
    uint8_t* destBytes = reinterpret_cast<uint8_t*>(dest);

    size_t field = 0;
    size_t offset = 0;
    while (true) {
        StubField::Type type = fieldType(field);
        switch (type) {
          case StubField::Type::RawWord:
            *reinterpret_cast<uintptr_t*>(destBytes + offset) =
                *reinterpret_cast<uintptr_t*>(srcBytes + offset);
            break;
          case StubField::Type::RawInt64:
            *reinterpret_cast<uint64_t*>(destBytes + offset) =
                *reinterpret_cast<uint64_t*>(srcBytes + offset);
            break;
          case StubField::Type::Shape:
            getStubField<Shape*>(dest, offset).init(getStubField<Shape*>(src, offset));
            break;
          case StubField::Type::JSObject:
            getStubField<JSObject*>(dest, offset).init(getStubField<JSObject*>(src, offset));
            break;
          case StubField::Type::ObjectGroup:
            getStubField<ObjectGroup*>(dest, offset).init(getStubField<ObjectGroup*>(src, offset));
            break;
          case StubField::Type::Id:
            getStubField<jsid>(dest, offset).init(getStubField<jsid>(src, offset));
            break;
          case StubField::Type::Value:
            getStubField<Value>(dest, offset).init(getStubField<Value>(src, offset));
            break;
          case StubField::Type::Limit:
            return; // Done.
        }
        field++;
        offset += StubField::sizeInBytes(type);
    }
}


/* static */ ICCacheIR_Monitored*
ICCacheIR_Monitored::Clone(JSContext* cx, ICStubSpace* space, ICStub* firstMonitorStub,
                           ICCacheIR_Monitored& other)
{
    const CacheIRStubInfo* stubInfo = other.stubInfo();
    MOZ_ASSERT(stubInfo->makesGCCalls());

    size_t bytesNeeded = stubInfo->stubDataOffset() + stubInfo->stubDataSize();
    void* newStub = space->alloc(bytesNeeded);
    if (!newStub)
        return nullptr;

    ICCacheIR_Monitored* res = new(newStub) ICCacheIR_Monitored(other.jitCode(), firstMonitorStub,
                                                                stubInfo);
    stubInfo->copyStubData(&other, res);
    return res;
}
