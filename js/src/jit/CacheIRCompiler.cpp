/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIRCompiler.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/FunctionTypeTraits.h"
#include "mozilla/ScopeExit.h"

#include <type_traits>
#include <utility>

#include "jslibmath.h"
#include "gc/Allocator.h"
#include "jit/BaselineCacheIRCompiler.h"
#include "jit/IonCacheIRCompiler.h"
#include "jit/IonIC.h"
#include "jit/SharedICHelpers.h"
#include "jit/SharedICRegisters.h"
#include "proxy/Proxy.h"
#include "vm/ArrayBufferObject.h"
#include "vm/BigIntType.h"
#include "vm/GeneratorObject.h"

#include "builtin/Boolean-inl.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/SharedICHelpers-inl.h"
#include "jit/VMFunctionList-inl.h"
#include "vm/BytecodeUtil-inl.h"
#include "vm/Realm-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::BitwiseCast;
using mozilla::Maybe;

ValueOperand CacheRegisterAllocator::useValueRegister(MacroAssembler& masm,
                                                      ValOperandId op) {
  OperandLocation& loc = operandLocations_[op.id()];

  switch (loc.kind()) {
    case OperandLocation::ValueReg:
      currentOpRegs_.add(loc.valueReg());
      return loc.valueReg();

    case OperandLocation::ValueStack: {
      ValueOperand reg = allocateValueRegister(masm);
      popValue(masm, &loc, reg);
      return reg;
    }

    case OperandLocation::BaselineFrame: {
      ValueOperand reg = allocateValueRegister(masm);
      Address addr = addressOf(masm, loc.baselineFrameSlot());
      masm.loadValue(addr, reg);
      loc.setValueReg(reg);
      return reg;
    }

    case OperandLocation::Constant: {
      ValueOperand reg = allocateValueRegister(masm);
      masm.moveValue(loc.constant(), reg);
      loc.setValueReg(reg);
      return reg;
    }

    case OperandLocation::PayloadReg: {
      // Temporarily add the payload register to currentOpRegs_ so
      // allocateValueRegister will stay away from it.
      currentOpRegs_.add(loc.payloadReg());
      ValueOperand reg = allocateValueRegister(masm);
      masm.tagValue(loc.payloadType(), loc.payloadReg(), reg);
      currentOpRegs_.take(loc.payloadReg());
      availableRegs_.add(loc.payloadReg());
      loc.setValueReg(reg);
      return reg;
    }

    case OperandLocation::PayloadStack: {
      ValueOperand reg = allocateValueRegister(masm);
      popPayload(masm, &loc, reg.scratchReg());
      masm.tagValue(loc.payloadType(), reg.scratchReg(), reg);
      loc.setValueReg(reg);
      return reg;
    }

    case OperandLocation::DoubleReg: {
      ValueOperand reg = allocateValueRegister(masm);
      {
        ScratchDoubleScope fpscratch(masm);
        masm.boxDouble(loc.doubleReg(), reg, fpscratch);
      }
      loc.setValueReg(reg);
      return reg;
    }

    case OperandLocation::Uninitialized:
      break;
  }

  MOZ_CRASH();
}

// Load a value operand directly into a float register. Caller must have
// guarded isNumber on the provided val.
void CacheRegisterAllocator::ensureDoubleRegister(MacroAssembler& masm,
                                                  NumberOperandId op,
                                                  FloatRegister dest) {
  OperandLocation& loc = operandLocations_[op.id()];

  Label failure, done;
  switch (loc.kind()) {
    case OperandLocation::ValueReg: {
      masm.ensureDouble(loc.valueReg(), dest, &failure);
      break;
    }

    case OperandLocation::ValueStack: {
      masm.ensureDouble(valueAddress(masm, &loc), dest, &failure);
      break;
    }

    case OperandLocation::BaselineFrame: {
      Address addr = addressOf(masm, loc.baselineFrameSlot());
      masm.ensureDouble(addr, dest, &failure);
      break;
    }

    case OperandLocation::DoubleReg: {
      masm.moveDouble(loc.doubleReg(), dest);
      loc.setDoubleReg(dest);
      return;
    }

    case OperandLocation::Constant: {
      MOZ_ASSERT(loc.constant().isNumber(),
                 "Caller must ensure the operand is a number value");
      masm.loadConstantDouble(loc.constant().toNumber(), dest);
      return;
    }

    case OperandLocation::PayloadReg: {
      // Doubles can't be stored in payload registers, so this must be an int32.
      MOZ_ASSERT(loc.payloadType() == JSVAL_TYPE_INT32,
                 "Caller must ensure the operand is a number value");
      masm.convertInt32ToDouble(loc.payloadReg(), dest);
      return;
    }

    case OperandLocation::PayloadStack: {
      // Doubles can't be stored in payload registers, so this must be an int32.
      MOZ_ASSERT(loc.payloadType() == JSVAL_TYPE_INT32,
                 "Caller must ensure the operand is a number value");
      MOZ_ASSERT(loc.payloadStack() <= stackPushed_);
      masm.convertInt32ToDouble(
          Address(masm.getStackPointer(), stackPushed_ - loc.payloadStack()),
          dest);
      return;
    }

    case OperandLocation::Uninitialized:
      MOZ_CRASH("Unhandled operand type in ensureDoubleRegister");
      return;
  }
  masm.jump(&done);
  masm.bind(&failure);
  masm.assumeUnreachable(
      "Missing guard allowed non-number to hit ensureDoubleRegister");
  masm.bind(&done);
}

ValueOperand CacheRegisterAllocator::useFixedValueRegister(MacroAssembler& masm,
                                                           ValOperandId valId,
                                                           ValueOperand reg) {
  allocateFixedValueRegister(masm, reg);

  OperandLocation& loc = operandLocations_[valId.id()];
  switch (loc.kind()) {
    case OperandLocation::ValueReg:
      masm.moveValue(loc.valueReg(), reg);
      MOZ_ASSERT(!currentOpRegs_.aliases(loc.valueReg()),
                 "Register shouldn't be in use");
      availableRegs_.add(loc.valueReg());
      break;
    case OperandLocation::ValueStack:
      popValue(masm, &loc, reg);
      break;
    case OperandLocation::BaselineFrame: {
      Address addr = addressOf(masm, loc.baselineFrameSlot());
      masm.loadValue(addr, reg);
      break;
    }
    case OperandLocation::Constant:
      masm.moveValue(loc.constant(), reg);
      break;
    case OperandLocation::PayloadReg:
      masm.tagValue(loc.payloadType(), loc.payloadReg(), reg);
      MOZ_ASSERT(!currentOpRegs_.has(loc.payloadReg()),
                 "Register shouldn't be in use");
      availableRegs_.add(loc.payloadReg());
      break;
    case OperandLocation::PayloadStack:
      popPayload(masm, &loc, reg.scratchReg());
      masm.tagValue(loc.payloadType(), reg.scratchReg(), reg);
      break;
    case OperandLocation::DoubleReg: {
      ScratchDoubleScope fpscratch(masm);
      masm.boxDouble(loc.doubleReg(), reg, fpscratch);
      break;
    }
    case OperandLocation::Uninitialized:
      MOZ_CRASH();
  }

  loc.setValueReg(reg);
  return reg;
}

Register CacheRegisterAllocator::useRegister(MacroAssembler& masm,
                                             TypedOperandId typedId) {
  MOZ_ASSERT(!addedFailurePath_);

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
      masm.unboxNonDouble(val, reg, typedId.type());
      loc.setPayloadReg(reg, typedId.type());
      currentOpRegs_.add(reg);
      return reg;
    }

    case OperandLocation::PayloadStack: {
      Register reg = allocateRegister(masm);
      popPayload(masm, &loc, reg);
      return reg;
    }

    case OperandLocation::ValueStack: {
      // The value is on the stack, but boxed. If it's on top of the stack we
      // unbox it and then remove it from the stack, else we just unbox.
      Register reg = allocateRegister(masm);
      if (loc.valueStack() == stackPushed_) {
        masm.unboxNonDouble(Address(masm.getStackPointer(), 0), reg,
                            typedId.type());
        masm.addToStackPtr(Imm32(sizeof(js::Value)));
        MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));
        stackPushed_ -= sizeof(js::Value);
      } else {
        MOZ_ASSERT(loc.valueStack() < stackPushed_);
        masm.unboxNonDouble(
            Address(masm.getStackPointer(), stackPushed_ - loc.valueStack()),
            reg, typedId.type());
      }
      loc.setPayloadReg(reg, typedId.type());
      return reg;
    }

    case OperandLocation::BaselineFrame: {
      Register reg = allocateRegister(masm);
      Address addr = addressOf(masm, loc.baselineFrameSlot());
      masm.unboxNonDouble(addr, reg, typedId.type());
      loc.setPayloadReg(reg, typedId.type());
      return reg;
    };

    case OperandLocation::Constant: {
      Value v = loc.constant();
      Register reg = allocateRegister(masm);
      if (v.isString()) {
        masm.movePtr(ImmGCPtr(v.toString()), reg);
      } else if (v.isSymbol()) {
        masm.movePtr(ImmGCPtr(v.toSymbol()), reg);
      } else if (v.isBigInt()) {
        masm.movePtr(ImmGCPtr(v.toBigInt()), reg);
      } else {
        MOZ_CRASH("Unexpected Value");
      }
      loc.setPayloadReg(reg, v.extractNonDoubleType());
      return reg;
    }

    case OperandLocation::DoubleReg:
    case OperandLocation::Uninitialized:
      break;
  }

  MOZ_CRASH();
}

ConstantOrRegister CacheRegisterAllocator::useConstantOrRegister(
    MacroAssembler& masm, ValOperandId val) {
  MOZ_ASSERT(!addedFailurePath_);

  OperandLocation& loc = operandLocations_[val.id()];
  switch (loc.kind()) {
    case OperandLocation::Constant:
      return loc.constant();

    case OperandLocation::PayloadReg:
    case OperandLocation::PayloadStack: {
      JSValueType payloadType = loc.payloadType();
      Register reg = useRegister(masm, TypedOperandId(val, payloadType));
      return TypedOrValueRegister(MIRTypeFromValueType(payloadType),
                                  AnyRegister(reg));
    }

    case OperandLocation::ValueReg:
    case OperandLocation::ValueStack:
    case OperandLocation::BaselineFrame:
      return TypedOrValueRegister(useValueRegister(masm, val));

    case OperandLocation::DoubleReg:
      return TypedOrValueRegister(MIRType::Double,
                                  AnyRegister(loc.doubleReg()));

    case OperandLocation::Uninitialized:
      break;
  }

  MOZ_CRASH();
}

Register CacheRegisterAllocator::defineRegister(MacroAssembler& masm,
                                                TypedOperandId typedId) {
  MOZ_ASSERT(!addedFailurePath_);

  OperandLocation& loc = operandLocations_[typedId.id()];
  MOZ_ASSERT(loc.kind() == OperandLocation::Uninitialized);

  Register reg = allocateRegister(masm);
  loc.setPayloadReg(reg, typedId.type());
  return reg;
}

ValueOperand CacheRegisterAllocator::defineValueRegister(MacroAssembler& masm,
                                                         ValOperandId val) {
  MOZ_ASSERT(!addedFailurePath_);

  OperandLocation& loc = operandLocations_[val.id()];
  MOZ_ASSERT(loc.kind() == OperandLocation::Uninitialized);

  ValueOperand reg = allocateValueRegister(masm);
  loc.setValueReg(reg);
  return reg;
}

void CacheRegisterAllocator::freeDeadOperandLocations(MacroAssembler& masm) {
  // See if any operands are dead so we can reuse their registers. Note that
  // we skip the input operands, as those are also used by failure paths, and
  // we currently don't track those uses.
  for (size_t i = writer_.numInputOperands(); i < operandLocations_.length();
       i++) {
    if (!writer_.operandIsDead(i, currentInstruction_)) {
      continue;
    }

    OperandLocation& loc = operandLocations_[i];
    switch (loc.kind()) {
      case OperandLocation::PayloadReg:
        availableRegs_.add(loc.payloadReg());
        break;
      case OperandLocation::ValueReg:
        availableRegs_.add(loc.valueReg());
        break;
      case OperandLocation::PayloadStack:
        masm.propagateOOM(freePayloadSlots_.append(loc.payloadStack()));
        break;
      case OperandLocation::ValueStack:
        masm.propagateOOM(freeValueSlots_.append(loc.valueStack()));
        break;
      case OperandLocation::Uninitialized:
      case OperandLocation::BaselineFrame:
      case OperandLocation::Constant:
      case OperandLocation::DoubleReg:
        break;
    }
    loc.setUninitialized();
  }
}

void CacheRegisterAllocator::discardStack(MacroAssembler& masm) {
  // This should only be called when we are no longer using the operands,
  // as we're discarding everything from the native stack. Set all operand
  // locations to Uninitialized to catch bugs.
  for (size_t i = 0; i < operandLocations_.length(); i++) {
    operandLocations_[i].setUninitialized();
  }

  if (stackPushed_ > 0) {
    masm.addToStackPtr(Imm32(stackPushed_));
    stackPushed_ = 0;
  }
  freePayloadSlots_.clear();
  freeValueSlots_.clear();
}

Register CacheRegisterAllocator::allocateRegister(MacroAssembler& masm) {
  MOZ_ASSERT(!addedFailurePath_);

  if (availableRegs_.empty()) {
    freeDeadOperandLocations(masm);
  }

  if (availableRegs_.empty()) {
    // Still no registers available, try to spill unused operands to
    // the stack.
    for (size_t i = 0; i < operandLocations_.length(); i++) {
      OperandLocation& loc = operandLocations_[i];
      if (loc.kind() == OperandLocation::PayloadReg) {
        Register reg = loc.payloadReg();
        if (currentOpRegs_.has(reg)) {
          continue;
        }

        spillOperandToStack(masm, &loc);
        availableRegs_.add(reg);
        break;  // We got a register, so break out of the loop.
      }
      if (loc.kind() == OperandLocation::ValueReg) {
        ValueOperand reg = loc.valueReg();
        if (currentOpRegs_.aliases(reg)) {
          continue;
        }

        spillOperandToStack(masm, &loc);
        availableRegs_.add(reg);
        break;  // Break out of the loop.
      }
    }
  }

  if (availableRegs_.empty() && !availableRegsAfterSpill_.empty()) {
    Register reg = availableRegsAfterSpill_.takeAny();
    masm.push(reg);
    stackPushed_ += sizeof(uintptr_t);

    masm.propagateOOM(spilledRegs_.append(SpilledRegister(reg, stackPushed_)));

    availableRegs_.add(reg);
  }

  // At this point, there must be a free register.
  MOZ_RELEASE_ASSERT(!availableRegs_.empty());

  Register reg = availableRegs_.takeAny();
  currentOpRegs_.add(reg);
  return reg;
}

void CacheRegisterAllocator::allocateFixedRegister(MacroAssembler& masm,
                                                   Register reg) {
  MOZ_ASSERT(!addedFailurePath_);

  // Fixed registers should be allocated first, to ensure they're
  // still available.
  MOZ_ASSERT(!currentOpRegs_.has(reg), "Register is in use");

  freeDeadOperandLocations(masm);

  if (availableRegs_.has(reg)) {
    availableRegs_.take(reg);
    currentOpRegs_.add(reg);
    return;
  }

  // Register may be available only after spilling contents.
  if (availableRegsAfterSpill_.has(reg)) {
    availableRegsAfterSpill_.take(reg);
    masm.push(reg);
    stackPushed_ += sizeof(uintptr_t);

    masm.propagateOOM(spilledRegs_.append(SpilledRegister(reg, stackPushed_)));
    currentOpRegs_.add(reg);
    return;
  }

  // The register must be used by some operand. Spill it to the stack.
  for (size_t i = 0; i < operandLocations_.length(); i++) {
    OperandLocation& loc = operandLocations_[i];
    if (loc.kind() == OperandLocation::PayloadReg) {
      if (loc.payloadReg() != reg) {
        continue;
      }

      spillOperandToStackOrRegister(masm, &loc);
      currentOpRegs_.add(reg);
      return;
    }
    if (loc.kind() == OperandLocation::ValueReg) {
      if (!loc.valueReg().aliases(reg)) {
        continue;
      }

      ValueOperand valueReg = loc.valueReg();
      spillOperandToStackOrRegister(masm, &loc);

      availableRegs_.add(valueReg);
      availableRegs_.take(reg);
      currentOpRegs_.add(reg);
      return;
    }
  }

  MOZ_CRASH("Invalid register");
}

void CacheRegisterAllocator::allocateFixedValueRegister(MacroAssembler& masm,
                                                        ValueOperand reg) {
#ifdef JS_NUNBOX32
  allocateFixedRegister(masm, reg.payloadReg());
  allocateFixedRegister(masm, reg.typeReg());
#else
  allocateFixedRegister(masm, reg.valueReg());
#endif
}

ValueOperand CacheRegisterAllocator::allocateValueRegister(
    MacroAssembler& masm) {
#ifdef JS_NUNBOX32
  Register reg1 = allocateRegister(masm);
  Register reg2 = allocateRegister(masm);
  return ValueOperand(reg1, reg2);
#else
  Register reg = allocateRegister(masm);
  return ValueOperand(reg);
#endif
}

bool CacheRegisterAllocator::init() {
  if (!origInputLocations_.resize(writer_.numInputOperands())) {
    return false;
  }
  if (!operandLocations_.resize(writer_.numOperandIds())) {
    return false;
  }
  return true;
}

void CacheRegisterAllocator::initAvailableRegsAfterSpill() {
  // Registers not in availableRegs_ and not used by input operands are
  // available after being spilled.
  availableRegsAfterSpill_.set() = GeneralRegisterSet::Intersect(
      GeneralRegisterSet::Not(availableRegs_.set()),
      GeneralRegisterSet::Not(inputRegisterSet()));
}

void CacheRegisterAllocator::fixupAliasedInputs(MacroAssembler& masm) {
  // If IC inputs alias each other, make sure they are stored in different
  // locations so we don't have to deal with this complexity in the rest of
  // the allocator.
  //
  // Note that this can happen in IonMonkey with something like |o.foo = o|
  // or |o[i] = i|.

  size_t numInputs = writer_.numInputOperands();
  MOZ_ASSERT(origInputLocations_.length() == numInputs);

  for (size_t i = 1; i < numInputs; i++) {
    OperandLocation& loc1 = operandLocations_[i];
    if (!loc1.isInRegister()) {
      continue;
    }

    for (size_t j = 0; j < i; j++) {
      OperandLocation& loc2 = operandLocations_[j];
      if (!loc1.aliasesReg(loc2)) {
        continue;
      }

      // loc1 and loc2 alias so we spill one of them. If one is a
      // ValueReg and the other is a PayloadReg, we have to spill the
      // PayloadReg: spilling the ValueReg instead would leave its type
      // register unallocated on 32-bit platforms.
      if (loc1.kind() == OperandLocation::ValueReg) {
        spillOperandToStack(masm, &loc2);
      } else {
        MOZ_ASSERT(loc1.kind() == OperandLocation::PayloadReg);
        spillOperandToStack(masm, &loc1);
        break;  // Spilled loc1, so nothing else will alias it.
      }
    }
  }

#ifdef DEBUG
  assertValidState();
#endif
}

GeneralRegisterSet CacheRegisterAllocator::inputRegisterSet() const {
  MOZ_ASSERT(origInputLocations_.length() == writer_.numInputOperands());

  AllocatableGeneralRegisterSet result;
  for (size_t i = 0; i < writer_.numInputOperands(); i++) {
    const OperandLocation& loc = operandLocations_[i];
    MOZ_ASSERT(loc == origInputLocations_[i]);

    switch (loc.kind()) {
      case OperandLocation::PayloadReg:
        result.addUnchecked(loc.payloadReg());
        continue;
      case OperandLocation::ValueReg:
        result.addUnchecked(loc.valueReg());
        continue;
      case OperandLocation::PayloadStack:
      case OperandLocation::ValueStack:
      case OperandLocation::BaselineFrame:
      case OperandLocation::Constant:
      case OperandLocation::DoubleReg:
        continue;
      case OperandLocation::Uninitialized:
        break;
    }
    MOZ_CRASH("Invalid kind");
  }

  return result.set();
}

JSValueType CacheRegisterAllocator::knownType(ValOperandId val) const {
  const OperandLocation& loc = operandLocations_[val.id()];

  switch (loc.kind()) {
    case OperandLocation::ValueReg:
    case OperandLocation::ValueStack:
    case OperandLocation::BaselineFrame:
      return JSVAL_TYPE_UNKNOWN;

    case OperandLocation::PayloadStack:
    case OperandLocation::PayloadReg:
      return loc.payloadType();

    case OperandLocation::Constant:
      return loc.constant().isDouble() ? JSVAL_TYPE_DOUBLE
                                       : loc.constant().extractNonDoubleType();

    case OperandLocation::DoubleReg:
      return JSVAL_TYPE_DOUBLE;

    case OperandLocation::Uninitialized:
      break;
  }

  MOZ_CRASH("Invalid kind");
}

void CacheRegisterAllocator::initInputLocation(
    size_t i, const TypedOrValueRegister& reg) {
  if (reg.hasValue()) {
    initInputLocation(i, reg.valueReg());
  } else if (reg.typedReg().isFloat()) {
    MOZ_ASSERT(reg.type() == MIRType::Double);
    initInputLocation(i, reg.typedReg().fpu());
  } else {
    initInputLocation(i, reg.typedReg().gpr(),
                      ValueTypeFromMIRType(reg.type()));
  }
}

void CacheRegisterAllocator::initInputLocation(
    size_t i, const ConstantOrRegister& value) {
  if (value.constant()) {
    initInputLocation(i, value.value());
  } else {
    initInputLocation(i, value.reg());
  }
}

void CacheRegisterAllocator::spillOperandToStack(MacroAssembler& masm,
                                                 OperandLocation* loc) {
  MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());

  if (loc->kind() == OperandLocation::ValueReg) {
    if (!freeValueSlots_.empty()) {
      uint32_t stackPos = freeValueSlots_.popCopy();
      MOZ_ASSERT(stackPos <= stackPushed_);
      masm.storeValue(loc->valueReg(),
                      Address(masm.getStackPointer(), stackPushed_ - stackPos));
      loc->setValueStack(stackPos);
      return;
    }
    stackPushed_ += sizeof(js::Value);
    masm.pushValue(loc->valueReg());
    loc->setValueStack(stackPushed_);
    return;
  }

  MOZ_ASSERT(loc->kind() == OperandLocation::PayloadReg);

  if (!freePayloadSlots_.empty()) {
    uint32_t stackPos = freePayloadSlots_.popCopy();
    MOZ_ASSERT(stackPos <= stackPushed_);
    masm.storePtr(loc->payloadReg(),
                  Address(masm.getStackPointer(), stackPushed_ - stackPos));
    loc->setPayloadStack(stackPos, loc->payloadType());
    return;
  }
  stackPushed_ += sizeof(uintptr_t);
  masm.push(loc->payloadReg());
  loc->setPayloadStack(stackPushed_, loc->payloadType());
}

void CacheRegisterAllocator::spillOperandToStackOrRegister(
    MacroAssembler& masm, OperandLocation* loc) {
  MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());

  // If enough registers are available, use them.
  if (loc->kind() == OperandLocation::ValueReg) {
    static const size_t BoxPieces = sizeof(Value) / sizeof(uintptr_t);
    if (availableRegs_.set().size() >= BoxPieces) {
      ValueOperand reg = availableRegs_.takeAnyValue();
      masm.moveValue(loc->valueReg(), reg);
      loc->setValueReg(reg);
      return;
    }
  } else {
    MOZ_ASSERT(loc->kind() == OperandLocation::PayloadReg);
    if (!availableRegs_.empty()) {
      Register reg = availableRegs_.takeAny();
      masm.movePtr(loc->payloadReg(), reg);
      loc->setPayloadReg(reg, loc->payloadType());
      return;
    }
  }

  // Not enough registers available, spill to the stack.
  spillOperandToStack(masm, loc);
}

void CacheRegisterAllocator::popPayload(MacroAssembler& masm,
                                        OperandLocation* loc, Register dest) {
  MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());
  MOZ_ASSERT(stackPushed_ >= sizeof(uintptr_t));

  // The payload is on the stack. If it's on top of the stack we can just
  // pop it, else we emit a load.
  if (loc->payloadStack() == stackPushed_) {
    masm.pop(dest);
    stackPushed_ -= sizeof(uintptr_t);
  } else {
    MOZ_ASSERT(loc->payloadStack() < stackPushed_);
    masm.loadPtr(
        Address(masm.getStackPointer(), stackPushed_ - loc->payloadStack()),
        dest);
    masm.propagateOOM(freePayloadSlots_.append(loc->payloadStack()));
  }

  loc->setPayloadReg(dest, loc->payloadType());
}

Address CacheRegisterAllocator::valueAddress(MacroAssembler& masm,
                                             OperandLocation* loc) {
  MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());
  return Address(masm.getStackPointer(), stackPushed_ - loc->valueStack());
}

void CacheRegisterAllocator::popValue(MacroAssembler& masm,
                                      OperandLocation* loc, ValueOperand dest) {
  MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());
  MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));

  // The Value is on the stack. If it's on top of the stack we can just
  // pop it, else we emit a load.
  if (loc->valueStack() == stackPushed_) {
    masm.popValue(dest);
    stackPushed_ -= sizeof(js::Value);
  } else {
    MOZ_ASSERT(loc->valueStack() < stackPushed_);
    masm.loadValue(
        Address(masm.getStackPointer(), stackPushed_ - loc->valueStack()),
        dest);
    masm.propagateOOM(freeValueSlots_.append(loc->valueStack()));
  }

  loc->setValueReg(dest);
}

#ifdef DEBUG
void CacheRegisterAllocator::assertValidState() const {
  // Assert different operands don't have aliasing storage. We depend on this
  // when spilling registers, for instance.

  if (!JitOptions.fullDebugChecks) {
    return;
  }

  for (size_t i = 0; i < operandLocations_.length(); i++) {
    const auto& loc1 = operandLocations_[i];
    if (loc1.isUninitialized()) {
      continue;
    }

    for (size_t j = 0; j < i; j++) {
      const auto& loc2 = operandLocations_[j];
      if (loc2.isUninitialized()) {
        continue;
      }
      MOZ_ASSERT(!loc1.aliasesReg(loc2));
    }
  }
}
#endif

bool OperandLocation::aliasesReg(const OperandLocation& other) const {
  MOZ_ASSERT(&other != this);

  switch (other.kind_) {
    case PayloadReg:
      return aliasesReg(other.payloadReg());
    case ValueReg:
      return aliasesReg(other.valueReg());
    case PayloadStack:
    case ValueStack:
    case BaselineFrame:
    case Constant:
    case DoubleReg:
      return false;
    case Uninitialized:
      break;
  }

  MOZ_CRASH("Invalid kind");
}

void CacheRegisterAllocator::restoreInputState(MacroAssembler& masm,
                                               bool shouldDiscardStack) {
  size_t numInputOperands = origInputLocations_.length();
  MOZ_ASSERT(writer_.numInputOperands() == numInputOperands);

  for (size_t j = 0; j < numInputOperands; j++) {
    const OperandLocation& dest = origInputLocations_[j];
    OperandLocation& cur = operandLocations_[j];
    if (dest == cur) {
      continue;
    }

    auto autoAssign = mozilla::MakeScopeExit([&] { cur = dest; });

    // We have a cycle if a destination register will be used later
    // as source register. If that happens, just push the current value
    // on the stack and later get it from there.
    for (size_t k = j + 1; k < numInputOperands; k++) {
      OperandLocation& laterSource = operandLocations_[k];
      if (dest.aliasesReg(laterSource)) {
        spillOperandToStack(masm, &laterSource);
      }
    }

    if (dest.kind() == OperandLocation::ValueReg) {
      // We have to restore a Value register.
      switch (cur.kind()) {
        case OperandLocation::ValueReg:
          masm.moveValue(cur.valueReg(), dest.valueReg());
          continue;
        case OperandLocation::PayloadReg:
          masm.tagValue(cur.payloadType(), cur.payloadReg(), dest.valueReg());
          continue;
        case OperandLocation::PayloadStack: {
          Register scratch = dest.valueReg().scratchReg();
          popPayload(masm, &cur, scratch);
          masm.tagValue(cur.payloadType(), scratch, dest.valueReg());
          continue;
        }
        case OperandLocation::ValueStack:
          popValue(masm, &cur, dest.valueReg());
          continue;
        case OperandLocation::DoubleReg:
          masm.boxDouble(cur.doubleReg(), dest.valueReg(), cur.doubleReg());
          continue;
        case OperandLocation::Constant:
        case OperandLocation::BaselineFrame:
        case OperandLocation::Uninitialized:
          break;
      }
    } else if (dest.kind() == OperandLocation::PayloadReg) {
      // We have to restore a payload register.
      switch (cur.kind()) {
        case OperandLocation::ValueReg:
          MOZ_ASSERT(dest.payloadType() != JSVAL_TYPE_DOUBLE);
          masm.unboxNonDouble(cur.valueReg(), dest.payloadReg(),
                              dest.payloadType());
          continue;
        case OperandLocation::PayloadReg:
          MOZ_ASSERT(cur.payloadType() == dest.payloadType());
          masm.mov(cur.payloadReg(), dest.payloadReg());
          continue;
        case OperandLocation::PayloadStack: {
          MOZ_ASSERT(cur.payloadType() == dest.payloadType());
          popPayload(masm, &cur, dest.payloadReg());
          continue;
        }
        case OperandLocation::ValueStack:
          MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));
          MOZ_ASSERT(cur.valueStack() <= stackPushed_);
          MOZ_ASSERT(dest.payloadType() != JSVAL_TYPE_DOUBLE);
          masm.unboxNonDouble(
              Address(masm.getStackPointer(), stackPushed_ - cur.valueStack()),
              dest.payloadReg(), dest.payloadType());
          continue;
        case OperandLocation::Constant:
        case OperandLocation::BaselineFrame:
        case OperandLocation::DoubleReg:
        case OperandLocation::Uninitialized:
          break;
      }
    } else if (dest.kind() == OperandLocation::Constant ||
               dest.kind() == OperandLocation::BaselineFrame ||
               dest.kind() == OperandLocation::DoubleReg) {
      // Nothing to do.
      continue;
    }

    MOZ_CRASH("Invalid kind");
  }

  for (const SpilledRegister& spill : spilledRegs_) {
    MOZ_ASSERT(stackPushed_ >= sizeof(uintptr_t));

    if (spill.stackPushed == stackPushed_) {
      masm.pop(spill.reg);
      stackPushed_ -= sizeof(uintptr_t);
    } else {
      MOZ_ASSERT(spill.stackPushed < stackPushed_);
      masm.loadPtr(
          Address(masm.getStackPointer(), stackPushed_ - spill.stackPushed),
          spill.reg);
    }
  }

  if (shouldDiscardStack) {
    discardStack(masm);
  }
}

size_t CacheIRStubInfo::stubDataSize() const {
  size_t field = 0;
  size_t size = 0;
  while (true) {
    StubField::Type type = fieldType(field++);
    if (type == StubField::Type::Limit) {
      return size;
    }
    size += StubField::sizeInBytes(type);
  }
}

template <typename T>
static GCPtr<T>* AsGCPtr(uintptr_t* ptr) {
  return reinterpret_cast<GCPtr<T>*>(ptr);
}

uintptr_t CacheIRStubInfo::getStubRawWord(const uint8_t* stubData,
                                          uint32_t offset) const {
  MOZ_ASSERT(uintptr_t(stubData) % sizeof(uintptr_t) == 0);
  return *reinterpret_cast<const uintptr_t*>(stubData + offset);
}

uintptr_t CacheIRStubInfo::getStubRawWord(ICStub* stub, uint32_t offset) const {
  uint8_t* stubData = (uint8_t*)stub + stubDataOffset_;
  return getStubRawWord(stubData, offset);
}

template <class Stub, class T>
GCPtr<T>& CacheIRStubInfo::getStubField(Stub* stub, uint32_t offset) const {
  uint8_t* stubData = (uint8_t*)stub + stubDataOffset_;
  MOZ_ASSERT(uintptr_t(stubData) % sizeof(uintptr_t) == 0);

  return *AsGCPtr<T>((uintptr_t*)(stubData + offset));
}

template GCPtr<Shape*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<ObjectGroup*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JSObject*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JSString*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JSFunction*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JS::Symbol*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JS::Value>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<jsid>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<JSClass*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;
template GCPtr<ArrayObject*>& CacheIRStubInfo::getStubField<ICStub>(
    ICStub* stub, uint32_t offset) const;

template <typename T, typename V>
static void InitGCPtr(uintptr_t* ptr, V val) {
  AsGCPtr<T>(ptr)->init(mozilla::BitwiseCast<T>(val));
}

void CacheIRWriter::copyStubData(uint8_t* dest) const {
  MOZ_ASSERT(!failed());

  uintptr_t* destWords = reinterpret_cast<uintptr_t*>(dest);

  for (const StubField& field : stubFields_) {
    switch (field.type()) {
      case StubField::Type::RawWord:
        *destWords = field.asWord();
        break;
      case StubField::Type::Shape:
        InitGCPtr<Shape*>(destWords, field.asWord());
        break;
      case StubField::Type::JSObject:
        InitGCPtr<JSObject*>(destWords, field.asWord());
        break;
      case StubField::Type::ObjectGroup:
        InitGCPtr<ObjectGroup*>(destWords, field.asWord());
        break;
      case StubField::Type::Symbol:
        InitGCPtr<JS::Symbol*>(destWords, field.asWord());
        break;
      case StubField::Type::String:
        InitGCPtr<JSString*>(destWords, field.asWord());
        break;
      case StubField::Type::Id:
        AsGCPtr<jsid>(destWords)->init(jsid::fromRawBits(field.asWord()));
        break;
      case StubField::Type::RawInt64:
      case StubField::Type::DOMExpandoGeneration:
        *reinterpret_cast<uint64_t*>(destWords) = field.asInt64();
        break;
      case StubField::Type::Value:
        AsGCPtr<Value>(destWords)->init(
            Value::fromRawBits(uint64_t(field.asInt64())));
        break;
      case StubField::Type::Limit:
        MOZ_CRASH("Invalid type");
    }
    destWords += StubField::sizeInBytes(field.type()) / sizeof(uintptr_t);
  }
}

template <typename T>
void jit::TraceCacheIRStub(JSTracer* trc, T* stub,
                           const CacheIRStubInfo* stubInfo) {
  uint32_t field = 0;
  size_t offset = 0;
  while (true) {
    StubField::Type fieldType = stubInfo->fieldType(field);
    switch (fieldType) {
      case StubField::Type::RawWord:
      case StubField::Type::RawInt64:
      case StubField::Type::DOMExpandoGeneration:
        break;
      case StubField::Type::Shape:
        TraceNullableEdge(trc, &stubInfo->getStubField<T, Shape*>(stub, offset),
                          "cacheir-shape");
        break;
      case StubField::Type::ObjectGroup:
        TraceNullableEdge(
            trc, &stubInfo->getStubField<T, ObjectGroup*>(stub, offset),
            "cacheir-group");
        break;
      case StubField::Type::JSObject:
        TraceNullableEdge(trc,
                          &stubInfo->getStubField<T, JSObject*>(stub, offset),
                          "cacheir-object");
        break;
      case StubField::Type::Symbol:
        TraceNullableEdge(trc,
                          &stubInfo->getStubField<T, JS::Symbol*>(stub, offset),
                          "cacheir-symbol");
        break;
      case StubField::Type::String:
        TraceNullableEdge(trc,
                          &stubInfo->getStubField<T, JSString*>(stub, offset),
                          "cacheir-string");
        break;
      case StubField::Type::Id:
        TraceEdge(trc, &stubInfo->getStubField<T, jsid>(stub, offset),
                  "cacheir-id");
        break;
      case StubField::Type::Value:
        TraceEdge(trc, &stubInfo->getStubField<T, JS::Value>(stub, offset),
                  "cacheir-value");
        break;
      case StubField::Type::Limit:
        return;  // Done.
    }
    field++;
    offset += StubField::sizeInBytes(fieldType);
  }
}

template void jit::TraceCacheIRStub(JSTracer* trc, ICStub* stub,
                                    const CacheIRStubInfo* stubInfo);

template void jit::TraceCacheIRStub(JSTracer* trc, IonICStub* stub,
                                    const CacheIRStubInfo* stubInfo);

bool CacheIRWriter::stubDataEqualsMaybeUpdate(uint8_t* stubData,
                                              bool* updated) const {
  MOZ_ASSERT(!failed());

  *updated = false;
  const uintptr_t* stubDataWords = reinterpret_cast<const uintptr_t*>(stubData);

  // If DOMExpandoGeneration fields are different but all other stub fields
  // are exactly the same, we overwrite the old stub data instead of attaching
  // a new stub, as the old stub is never going to succeed. This works because
  // even Ion stubs read the DOMExpandoGeneration field from the stub instead
  // of baking it in.
  bool expandoGenerationIsDifferent = false;

  for (const StubField& field : stubFields_) {
    if (field.sizeIsWord()) {
      if (field.asWord() != *stubDataWords) {
        return false;
      }
      stubDataWords++;
      continue;
    }

    if (field.asInt64() != *reinterpret_cast<const uint64_t*>(stubDataWords)) {
      if (field.type() != StubField::Type::DOMExpandoGeneration) {
        return false;
      }
      expandoGenerationIsDifferent = true;
    }
    stubDataWords += sizeof(uint64_t) / sizeof(uintptr_t);
  }

  if (expandoGenerationIsDifferent) {
    copyStubData(stubData);
    *updated = true;
  }

  return true;
}

HashNumber CacheIRStubKey::hash(const CacheIRStubKey::Lookup& l) {
  HashNumber hash = mozilla::HashBytes(l.code, l.length);
  hash = mozilla::AddToHash(hash, uint32_t(l.kind));
  hash = mozilla::AddToHash(hash, uint32_t(l.engine));
  return hash;
}

bool CacheIRStubKey::match(const CacheIRStubKey& entry,
                           const CacheIRStubKey::Lookup& l) {
  if (entry.stubInfo->kind() != l.kind) {
    return false;
  }

  if (entry.stubInfo->engine() != l.engine) {
    return false;
  }

  if (entry.stubInfo->codeLength() != l.length) {
    return false;
  }

  if (!mozilla::ArrayEqual(entry.stubInfo->code(), l.code, l.length)) {
    return false;
  }

  return true;
}

CacheIRReader::CacheIRReader(const CacheIRStubInfo* stubInfo)
    : CacheIRReader(stubInfo->code(),
                    stubInfo->code() + stubInfo->codeLength()) {}

CacheIRStubInfo* CacheIRStubInfo::New(CacheKind kind, ICStubEngine engine,
                                      bool makesGCCalls,
                                      uint32_t stubDataOffset,
                                      const CacheIRWriter& writer) {
  size_t numStubFields = writer.numStubFields();
  size_t bytesNeeded =
      sizeof(CacheIRStubInfo) + writer.codeLength() +
      (numStubFields + 1);  // +1 for the GCType::Limit terminator.
  uint8_t* p = js_pod_malloc<uint8_t>(bytesNeeded);
  if (!p) {
    return nullptr;
  }

  // Copy the CacheIR code.
  uint8_t* codeStart = p + sizeof(CacheIRStubInfo);
  mozilla::PodCopy(codeStart, writer.codeStart(), writer.codeLength());

  static_assert(sizeof(StubField::Type) == sizeof(uint8_t),
                "StubField::Type must fit in uint8_t");

  // Copy the stub field types.
  uint8_t* fieldTypes = codeStart + writer.codeLength();
  for (size_t i = 0; i < numStubFields; i++) {
    fieldTypes[i] = uint8_t(writer.stubFieldType(i));
  }
  fieldTypes[numStubFields] = uint8_t(StubField::Type::Limit);

  return new (p) CacheIRStubInfo(kind, engine, makesGCCalls, stubDataOffset,
                                 codeStart, writer.codeLength(), fieldTypes);
}

bool OperandLocation::operator==(const OperandLocation& other) const {
  if (kind_ != other.kind_) {
    return false;
  }

  switch (kind()) {
    case Uninitialized:
      return true;
    case PayloadReg:
      return payloadReg() == other.payloadReg() &&
             payloadType() == other.payloadType();
    case ValueReg:
      return valueReg() == other.valueReg();
    case PayloadStack:
      return payloadStack() == other.payloadStack() &&
             payloadType() == other.payloadType();
    case ValueStack:
      return valueStack() == other.valueStack();
    case BaselineFrame:
      return baselineFrameSlot() == other.baselineFrameSlot();
    case Constant:
      return constant() == other.constant();
    case DoubleReg:
      return doubleReg() == other.doubleReg();
  }

  MOZ_CRASH("Invalid OperandLocation kind");
}

AutoOutputRegister::AutoOutputRegister(CacheIRCompiler& compiler)
    : output_(compiler.outputUnchecked_.ref()), alloc_(compiler.allocator) {
  if (output_.hasValue()) {
    alloc_.allocateFixedValueRegister(compiler.masm, output_.valueReg());
  } else if (!output_.typedReg().isFloat()) {
    alloc_.allocateFixedRegister(compiler.masm, output_.typedReg().gpr());
  }
}

AutoOutputRegister::~AutoOutputRegister() {
  if (output_.hasValue()) {
    alloc_.releaseValueRegister(output_.valueReg());
  } else if (!output_.typedReg().isFloat()) {
    alloc_.releaseRegister(output_.typedReg().gpr());
  }
}

bool FailurePath::canShareFailurePath(const FailurePath& other) const {
  if (stackPushed_ != other.stackPushed_) {
    return false;
  }

  if (spilledRegs_.length() != other.spilledRegs_.length()) {
    return false;
  }

  for (size_t i = 0; i < spilledRegs_.length(); i++) {
    if (spilledRegs_[i] != other.spilledRegs_[i]) {
      return false;
    }
  }

  MOZ_ASSERT(inputs_.length() == other.inputs_.length());

  for (size_t i = 0; i < inputs_.length(); i++) {
    if (inputs_[i] != other.inputs_[i]) {
      return false;
    }
  }
  return true;
}

bool CacheIRCompiler::addFailurePath(FailurePath** failure) {
#ifdef DEBUG
  allocator.setAddedFailurePath();
#endif

  FailurePath newFailure;
  for (size_t i = 0; i < writer_.numInputOperands(); i++) {
    if (!newFailure.appendInput(allocator.operandLocation(i))) {
      return false;
    }
  }
  if (!newFailure.setSpilledRegs(allocator.spilledRegs())) {
    return false;
  }
  newFailure.setStackPushed(allocator.stackPushed());

  // Reuse the previous failure path if the current one is the same, to
  // avoid emitting duplicate code.
  if (failurePaths.length() > 0 &&
      failurePaths.back().canShareFailurePath(newFailure)) {
    *failure = &failurePaths.back();
    return true;
  }

  if (!failurePaths.append(std::move(newFailure))) {
    return false;
  }

  *failure = &failurePaths.back();
  return true;
}

bool CacheIRCompiler::emitFailurePath(size_t index) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  FailurePath& failure = failurePaths[index];

  allocator.setStackPushed(failure.stackPushed());

  for (size_t i = 0; i < writer_.numInputOperands(); i++) {
    allocator.setOperandLocation(i, failure.input(i));
  }

  if (!allocator.setSpilledRegs(failure.spilledRegs())) {
    return false;
  }

  masm.bind(failure.label());
  allocator.restoreInputState(masm);
  return true;
}

bool CacheIRCompiler::emitGuardIsNumber() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);

  // Doubles and ints are numbers!
  if (knownType == JSVAL_TYPE_DOUBLE || knownType == JSVAL_TYPE_INT32) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestNumber(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardToObject() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  if (allocator.knownType(inputId) == JSVAL_TYPE_OBJECT) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }
  masm.branchTestObject(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardIsNullOrUndefined() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);
  if (knownType == JSVAL_TYPE_UNDEFINED || knownType == JSVAL_TYPE_NULL) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label success;
  masm.branchTestNull(Assembler::Equal, input, &success);
  masm.branchTestUndefined(Assembler::NotEqual, input, failure->label());

  masm.bind(&success);
  return true;
}

bool CacheIRCompiler::emitGuardIsNotNullOrUndefined() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);
  if (knownType == JSVAL_TYPE_UNDEFINED || knownType == JSVAL_TYPE_NULL) {
    return false;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestNull(Assembler::Equal, input, failure->label());
  masm.branchTestUndefined(Assembler::Equal, input, failure->label());

  return true;
}

bool CacheIRCompiler::emitGuardIsNull() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);
  if (knownType == JSVAL_TYPE_NULL) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label success;
  masm.branchTestNull(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardIsUndefined() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);
  if (knownType == JSVAL_TYPE_UNDEFINED) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestUndefined(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardIsObjectOrNull() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  JSValueType knownType = allocator.knownType(inputId);
  if (knownType == JSVAL_TYPE_OBJECT || knownType == JSVAL_TYPE_NULL) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label done;
  masm.branchTestObject(Assembler::Equal, input, &done);
  masm.branchTestNull(Assembler::NotEqual, input, failure->label());
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitGuardToBoolean() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_BOOLEAN) {
    Register input = allocator.useRegister(masm, Int32OperandId(inputId.id()));
    masm.move32(input, output);
    return true;
  }
  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestBoolean(Assembler::NotEqual, input, failure->label());
  masm.unboxBoolean(input, output);
  return true;
}

bool CacheIRCompiler::emitGuardToString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  if (allocator.knownType(inputId) == JSVAL_TYPE_STRING) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }
  masm.branchTestString(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardToSymbol() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  if (allocator.knownType(inputId) == JSVAL_TYPE_SYMBOL) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }
  masm.branchTestSymbol(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardToBigInt() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  if (allocator.knownType(inputId) == JSVAL_TYPE_BIGINT) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }
  masm.branchTestBigInt(Assembler::NotEqual, input, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardToInt32() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_INT32) {
    Register input = allocator.useRegister(masm, Int32OperandId(inputId.id()));
    masm.move32(input, output);
    return true;
  }
  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestInt32(Assembler::NotEqual, input, failure->label());
  masm.unboxInt32(input, output);
  return true;
}

// Infallible |emitDouble| emitters can use this implementation to avoid
// generating extra clean-up instructions to restore the scratch float register.
// To select this function simply omit the |Label* fail| parameter for the
// emitter lambda function.
template <typename EmitDouble>
static std::enable_if_t<mozilla::FunctionTypeTraits<EmitDouble>::arity == 1,
                        void>
EmitGuardDouble(CacheIRCompiler* compiler, MacroAssembler& masm,
                ValueOperand input, FailurePath* failure,
                EmitDouble emitDouble) {
  AutoScratchFloatRegister floatReg(compiler);

  masm.unboxDouble(input, floatReg);
  emitDouble(floatReg.get());
}

template <typename EmitDouble>
static std::enable_if_t<mozilla::FunctionTypeTraits<EmitDouble>::arity == 2,
                        void>
EmitGuardDouble(CacheIRCompiler* compiler, MacroAssembler& masm,
                ValueOperand input, FailurePath* failure,
                EmitDouble emitDouble) {
  AutoScratchFloatRegister floatReg(compiler, failure);

  masm.unboxDouble(input, floatReg);
  emitDouble(floatReg.get(), floatReg.failure());
}

template <typename EmitInt32, typename EmitDouble>
static void EmitGuardInt32OrDouble(CacheIRCompiler* compiler,
                                   MacroAssembler& masm, ValueOperand input,
                                   Register output, FailurePath* failure,
                                   EmitInt32 emitInt32, EmitDouble emitDouble) {
  Label done;

  {
    ScratchTagScope tag(masm, input);
    masm.splitTagForTest(input, tag);

    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, tag, &notInt32);
    {
      ScratchTagScopeRelease _(&tag);

      masm.unboxInt32(input, output);
      emitInt32();

      masm.jump(&done);
    }
    masm.bind(&notInt32);

    masm.branchTestDouble(Assembler::NotEqual, tag, failure->label());
    {
      ScratchTagScopeRelease _(&tag);

      EmitGuardDouble(compiler, masm, input, failure, emitDouble);
    }
  }

  masm.bind(&done);
}

bool CacheIRCompiler::emitGuardToInt32Index() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_INT32) {
    Register input = allocator.useRegister(masm, Int32OperandId(inputId.id()));
    masm.move32(input, output);
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitGuardInt32OrDouble(
      this, masm, input, output, failure,
      []() {
        // No-op if the value is already an int32.
      },
      [&](FloatRegister floatReg, Label* fail) {
        // ToPropertyKey(-0.0) is "0", so we can truncate -0.0 to 0 here.
        masm.convertDoubleToInt32(floatReg, output, fail, false);
      });

  return true;
}

bool CacheIRCompiler::emitGuardToTypedArrayIndex() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_INT32) {
    Register input = allocator.useRegister(masm, Int32OperandId(inputId.id()));
    masm.move32(input, output);
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitGuardInt32OrDouble(
      this, masm, input, output, failure,
      []() {
        // No-op if the value is already an int32.
      },
      [&](FloatRegister floatReg) {
        static_assert(
            TypedArrayObject::MAX_BYTE_LENGTH <= INT32_MAX,
            "Double exceeding Int32 range can't be in-bounds array access");

        // ToPropertyKey(-0.0) is "0", so we can truncate -0.0 to 0 here.
        Label done, fail;
        masm.convertDoubleToInt32(floatReg, output, &fail, false);
        masm.jump(&done);

        // Substitute the invalid index with an arbitrary out-of-bounds index.
        masm.bind(&fail);
        masm.move32(Imm32(-1), output);

        masm.bind(&done);
      });

  return true;
}

bool CacheIRCompiler::emitGuardToInt32ModUint32() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_INT32) {
    ConstantOrRegister input = allocator.useConstantOrRegister(masm, inputId);
    if (input.constant()) {
      masm.move32(Imm32(input.value().toInt32()), output);
    } else {
      MOZ_ASSERT(input.reg().type() == MIRType::Int32);
      masm.move32(input.reg().typedReg().gpr(), output);
    }
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitGuardInt32OrDouble(
      this, masm, input, output, failure,
      []() {
        // No-op if the value is already an int32.
      },
      [&](FloatRegister floatReg, Label* fail) {
        masm.branchTruncateDoubleMaybeModUint32(floatReg, output, fail);
      });

  return true;
}

bool CacheIRCompiler::emitGuardToUint8Clamped() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  if (allocator.knownType(inputId) == JSVAL_TYPE_INT32) {
    ConstantOrRegister input = allocator.useConstantOrRegister(masm, inputId);
    if (input.constant()) {
      masm.move32(Imm32(ClampDoubleToUint8(input.value().toInt32())), output);
    } else {
      MOZ_ASSERT(input.reg().type() == MIRType::Int32);
      masm.move32(input.reg().typedReg().gpr(), output);
      masm.clampIntToUint8(output);
    }
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  EmitGuardInt32OrDouble(
      this, masm, input, output, failure,
      [&]() {
        // |output| holds the unboxed int32 value.
        masm.clampIntToUint8(output);
      },
      [&](FloatRegister floatReg) {
        masm.clampDoubleToUint8(floatReg, output);
      });

  return true;
}

bool CacheIRCompiler::emitGuardType() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValOperandId inputId = reader.valOperandId();
  ValueType type = reader.valueType();

  if (allocator.knownType(inputId) == JSValueType(type)) {
    return true;
  }

  ValueOperand input = allocator.useValueRegister(masm, inputId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  switch (type) {
    case ValueType::String:
      masm.branchTestString(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Symbol:
      masm.branchTestSymbol(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::BigInt:
      masm.branchTestBigInt(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Int32:
      masm.branchTestInt32(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Double:
      masm.branchTestDouble(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Boolean:
      masm.branchTestBoolean(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Undefined:
      masm.branchTestUndefined(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Null:
      masm.branchTestNull(Assembler::NotEqual, input, failure->label());
      break;
    case ValueType::Magic:
    case ValueType::PrivateGCThing:
    case ValueType::Object:
      MOZ_CRASH("unexpected type");
  }

  return true;
}

bool CacheIRCompiler::emitGuardClass(ObjOperandId objId, GuardClassKind kind) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  const JSClass* clasp = nullptr;
  switch (kind) {
    case GuardClassKind::Array:
      clasp = &ArrayObject::class_;
      break;
    case GuardClassKind::MappedArguments:
      clasp = &MappedArgumentsObject::class_;
      break;
    case GuardClassKind::UnmappedArguments:
      clasp = &UnmappedArgumentsObject::class_;
      break;
    case GuardClassKind::WindowProxy:
      clasp = cx_->runtime()->maybeWindowProxyClass();
      break;
    case GuardClassKind::JSFunction:
      clasp = &JSFunction::class_;
      break;
  }
  MOZ_ASSERT(clasp);

  if (objectGuardNeedsSpectreMitigations(objId)) {
    masm.branchTestObjClass(Assembler::NotEqual, obj, clasp, scratch, obj,
                            failure->label());
  } else {
    masm.branchTestObjClassNoSpectreMitigations(Assembler::NotEqual, obj, clasp,
                                                scratch, failure->label());
  }

  return true;
}

bool CacheIRCompiler::emitGuardIsExtensible(ObjOperandId objId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Address shape(obj, JSObject::offsetOfShape());
  masm.loadPtr(shape, scratch);

  Address baseShape(scratch, Shape::offsetOfBaseShape());
  masm.loadPtr(baseShape, scratch);

  Address baseShapeFlags(scratch, BaseShape::offsetOfFlags());
  masm.loadPtr(baseShapeFlags, scratch);

  masm.and32(Imm32(js::BaseShape::NOT_EXTENSIBLE), scratch);

  // Spectre-style checks are not needed here because we do not
  // interpret data based on this check.
  masm.branch32(Assembler::Equal, scratch, Imm32(js::BaseShape::NOT_EXTENSIBLE),
                failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardSpecificNativeFunction(ObjOperandId objId,
                                                      JSNative native) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Ensure obj is a function.
  const JSClass* clasp = &JSFunction::class_;
  masm.branchTestObjClass(Assembler::NotEqual, obj, clasp, scratch, obj,
                          failure->label());

  // Ensure function native matches.
  masm.branchPtr(Assembler::NotEqual,
                 Address(obj, JSFunction::offsetOfNativeOrEnv()),
                 ImmPtr(native), failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardFunctionPrototype() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register prototypeObject = allocator.useRegister(masm, reader.objOperandId());

  // Allocate registers before the failure path to make sure they're registered
  // by addFailurePath.
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Guard on the .prototype object.
  StubFieldOffset slot(reader.stubOffset(), StubField::Type::RawWord);
  masm.loadPtr(Address(obj, NativeObject::offsetOfSlots()), scratch1);
  emitLoadStubField(slot, scratch2);
  BaseObjectSlotIndex prototypeSlot(scratch1, scratch2);
  masm.branchTestObject(Assembler::NotEqual, prototypeSlot, failure->label());
  masm.unboxObject(prototypeSlot, scratch1);
  masm.branchPtr(Assembler::NotEqual, prototypeObject, scratch1,
                 failure->label());

  return true;
}

bool CacheIRCompiler::emitGuardIsNativeObject(ObjOperandId objId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchIfNonNativeObj(obj, scratch, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardIsProxy(ObjOperandId objId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestObjectIsProxy(false, obj, scratch, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardNotDOMProxy(ObjOperandId objId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestProxyHandlerFamily(Assembler::Equal, obj, scratch,
                                    GetDOMProxyHandlerFamily(),
                                    failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardSpecificInt32Immediate(Int32OperandId integerId,
                                                      int32_t expected) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register reg = allocator.useRegister(masm, integerId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branch32(Assembler::NotEqual, reg, Imm32(expected), failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardMagicValue(ValOperandId valId,
                                          JSWhyMagic magic) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValueOperand val = allocator.useValueRegister(masm, valId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchTestMagicValue(Assembler::NotEqual, val, magic, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardNoDenseElements(ObjOperandId objId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  // Make sure there are no dense elements.
  Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
  masm.branch32(Assembler::NotEqual, initLength, Imm32(0), failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardAndGetInt32FromString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register str = allocator.useRegister(masm, reader.stringOperandId());
  Register output = allocator.defineRegister(masm, reader.int32OperandId());
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label vmCall, done;
  // Use indexed value as fast path if possible.
  masm.loadStringIndexValue(str, output, &vmCall);
  masm.jump(&done);
  {
    masm.bind(&vmCall);

    // Reserve stack for holding the result value of the call.
    masm.reserveStack(sizeof(int32_t));
    masm.moveStackPtrTo(output);

    // We cannot use callVM, as callVM expects to be able to clobber all
    // operands, however, since this op is not the last in the generated IC, we
    // want to be able to reference other live values.
    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                                 liveVolatileFloatRegs());
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch);
    masm.loadJSContext(scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(str);
    masm.passABIArg(output);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, GetInt32FromStringPure));
    masm.mov(ReturnReg, scratch);

    LiveRegisterSet ignore;
    ignore.add(scratch);
    masm.PopRegsInMaskIgnore(volatileRegs, ignore);

    Label ok;
    masm.branchIfTrueBool(scratch, &ok);
    {
      // OOM path, recovered by GetInt32FromStringPure.
      //
      // Use addToStackPtr instead of freeStack as freeStack tracks stack height
      // flow-insensitively, and using it twice would confuse the stack height
      // tracking.
      masm.addToStackPtr(Imm32(sizeof(int32_t)));
      masm.jump(failure->label());
    }
    masm.bind(&ok);

    masm.load32(Address(output, 0), output);
    masm.freeStack(sizeof(int32_t));
  }
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitGuardAndGetNumberFromString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register str = allocator.useRegister(masm, reader.stringOperandId());
  ValueOperand output =
      allocator.defineValueRegister(masm, reader.valOperandId());
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label vmCall, done;
  // Use indexed value as fast path if possible.
  masm.loadStringIndexValue(str, scratch, &vmCall);
  masm.tagValue(JSVAL_TYPE_INT32, scratch, output);
  masm.jump(&done);
  {
    masm.bind(&vmCall);

    // Reserve stack for holding the result value of the call.
    masm.reserveStack(sizeof(double));
    masm.moveStackPtrTo(output.payloadOrValueReg());

    // We cannot use callVM, as callVM expects to be able to clobber all
    // operands, however, since this op is not the last in the generated IC, we
    // want to be able to reference other live values.
    LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                                 liveVolatileFloatRegs());
    masm.PushRegsInMask(volatileRegs);

    masm.setupUnalignedABICall(scratch);
    masm.loadJSContext(scratch);
    masm.passABIArg(scratch);
    masm.passABIArg(str);
    masm.passABIArg(output.payloadOrValueReg());
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, StringToNumberPure));
    masm.mov(ReturnReg, scratch);

    LiveRegisterSet ignore;
    ignore.add(scratch);
    masm.PopRegsInMaskIgnore(volatileRegs, ignore);

    Label ok;
    masm.branchIfTrueBool(scratch, &ok);
    {
      // OOM path, recovered by StringToNumberPure.
      //
      // Use addToStackPtr instead of freeStack as freeStack tracks stack height
      // flow-insensitively, and using it twice would confuse the stack height
      // tracking.
      masm.addToStackPtr(Imm32(sizeof(double)));
      masm.jump(failure->label());
    }
    masm.bind(&ok);

    {
      ScratchDoubleScope fpscratch(masm);
      masm.loadDouble(Address(output.payloadOrValueReg(), 0), fpscratch);
      masm.boxDouble(fpscratch, output, fpscratch);
    }
    masm.freeStack(sizeof(double));
  }
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitGuardAndGetNumberFromBoolean() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register boolean = allocator.useRegister(masm, reader.int32OperandId());
  ValueOperand output =
      allocator.defineValueRegister(masm, reader.valOperandId());
  masm.tagValue(JSVAL_TYPE_INT32, boolean, output);
  return true;
}

bool CacheIRCompiler::emitGuardAndGetIndexFromString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register str = allocator.useRegister(masm, reader.stringOperandId());
  Register output = allocator.defineRegister(masm, reader.int32OperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label vmCall, done;
  masm.loadStringIndexValue(str, output, &vmCall);
  masm.jump(&done);

  {
    masm.bind(&vmCall);
    LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                         liveVolatileFloatRegs());
    masm.PushRegsInMask(save);

    masm.setupUnalignedABICall(output);
    masm.passABIArg(str);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, GetIndexFromString));
    masm.mov(ReturnReg, output);

    LiveRegisterSet ignore;
    ignore.add(output);
    masm.PopRegsInMaskIgnore(save, ignore);

    // GetIndexFromString returns a negative value on failure.
    masm.branchTest32(Assembler::Signed, output, output, failure->label());
  }

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadProto() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register reg = allocator.defineRegister(masm, reader.objOperandId());
  masm.loadObjProto(obj, reg);
  return true;
}

bool CacheIRCompiler::emitLoadEnclosingEnvironment() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register reg = allocator.defineRegister(masm, reader.objOperandId());
  masm.unboxObject(
      Address(obj, EnvironmentObject::offsetOfEnclosingEnvironment()), reg);
  return true;
}

bool CacheIRCompiler::emitLoadWrapperTarget() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register reg = allocator.defineRegister(masm, reader.objOperandId());

  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), reg);
  masm.unboxObject(
      Address(reg, js::detail::ProxyReservedSlots::offsetOfPrivateSlot()), reg);
  return true;
}

bool CacheIRCompiler::emitLoadValueTag() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
  Register res = allocator.defineRegister(masm, reader.valueTagOperandId());

  Register tag = masm.extractTag(val, res);
  if (tag != res) {
    masm.mov(tag, res);
  }
  return true;
}

bool CacheIRCompiler::emitLoadDOMExpandoValue() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ValueOperand val = allocator.defineValueRegister(masm, reader.valOperandId());

  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()),
               val.scratchReg());
  masm.loadValue(Address(val.scratchReg(),
                         js::detail::ProxyReservedSlots::offsetOfPrivateSlot()),
                 val);
  return true;
}

bool CacheIRCompiler::emitLoadDOMExpandoValueIgnoreGeneration() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ValueOperand output =
      allocator.defineValueRegister(masm, reader.valOperandId());

  // Determine the expando's Address.
  Register scratch = output.scratchReg();
  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), scratch);
  Address expandoAddr(scratch,
                      js::detail::ProxyReservedSlots::offsetOfPrivateSlot());

#ifdef DEBUG
  // Private values are stored as doubles, so assert we have a double.
  Label ok;
  masm.branchTestDouble(Assembler::Equal, expandoAddr, &ok);
  masm.assumeUnreachable("DOM expando is not a PrivateValue!");
  masm.bind(&ok);
#endif

  // Load the ExpandoAndGeneration* from the PrivateValue.
  masm.loadPrivate(expandoAddr, scratch);

  // Load expandoAndGeneration->expando into the output Value register.
  masm.loadValue(Address(scratch, ExpandoAndGeneration::offsetOfExpando()),
                 output);
  return true;
}

bool CacheIRCompiler::emitLoadUndefinedResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  if (output.hasValue()) {
    masm.moveValue(UndefinedValue(), output.valueReg());
  } else {
    masm.assumeUnreachable("Should have monitored undefined result");
  }
  return true;
}

static void EmitStoreBoolean(MacroAssembler& masm, bool b,
                             const AutoOutputRegister& output) {
  if (output.hasValue()) {
    Value val = BooleanValue(b);
    masm.moveValue(val, output.valueReg());
  } else {
    MOZ_ASSERT(output.type() == JSVAL_TYPE_BOOLEAN);
    masm.movePtr(ImmWord(b), output.typedReg().gpr());
  }
}

bool CacheIRCompiler::emitLoadBooleanResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  bool b = reader.readBool();
  EmitStoreBoolean(masm, b, output);

  return true;
}

static void EmitStoreResult(MacroAssembler& masm, Register reg,
                            JSValueType type,
                            const AutoOutputRegister& output) {
  if (output.hasValue()) {
    masm.tagValue(type, reg, output.valueReg());
    return;
  }
  if (type == JSVAL_TYPE_INT32 && output.typedReg().isFloat()) {
    masm.convertInt32ToDouble(reg, output.typedReg().fpu());
    return;
  }
  if (type == output.type()) {
    masm.mov(reg, output.typedReg().gpr());
    return;
  }
  masm.assumeUnreachable("Should have monitored result");
}

bool CacheIRCompiler::emitLoadInt32ArrayLengthResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
  masm.load32(Address(scratch, ObjectElements::offsetOfLength()), scratch);

  // Guard length fits in an int32.
  masm.branchTest32(Assembler::Signed, scratch, scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitDoubleAddResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  // Float register must be preserved. The BinaryArith ICs use
  // the fact that baseline has them available, as well as fixed temps on
  // LBinaryCache.
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  masm.addDouble(FloatReg1, FloatReg0);
  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}
bool CacheIRCompiler::emitDoubleSubResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  masm.subDouble(FloatReg1, FloatReg0);
  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}
bool CacheIRCompiler::emitDoubleMulResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  masm.mulDouble(FloatReg1, FloatReg0);
  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}
bool CacheIRCompiler::emitDoubleDivResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  masm.divDouble(FloatReg1, FloatReg0);
  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}
bool CacheIRCompiler::emitDoubleModResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
  masm.passABIArg(FloatReg1, MoveOp::DOUBLE);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, js::NumberMod), MoveOp::DOUBLE);
  masm.storeCallFloatResult(FloatReg0);

  LiveRegisterSet ignore;
  ignore.add(FloatReg0);
  masm.PopRegsInMaskIgnore(save, ignore);

  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}
bool CacheIRCompiler::emitDoublePowResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
  masm.passABIArg(FloatReg1, MoveOp::DOUBLE);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, js::ecmaPow), MoveOp::DOUBLE);
  masm.storeCallFloatResult(FloatReg0);

  LiveRegisterSet ignore;
  ignore.add(FloatReg0);
  masm.PopRegsInMaskIgnore(save, ignore);

  masm.boxDouble(FloatReg0, output.valueReg(), FloatReg0);

  return true;
}

bool CacheIRCompiler::emitInt32AddResult(Int32OperandId lhsId,
                                         Int32OperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Register lhs = allocator.useRegister(masm, lhsId);
  Register rhs = allocator.useRegister(masm, rhsId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.mov(rhs, scratch);
  masm.branchAdd32(Assembler::Overflow, lhs, scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}
bool CacheIRCompiler::emitInt32SubResult(Int32OperandId lhsId,
                                         Int32OperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
  Register lhs = allocator.useRegister(masm, lhsId);
  Register rhs = allocator.useRegister(masm, rhsId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.mov(lhs, scratch);
  masm.branchSub32(Assembler::Overflow, rhs, scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

bool CacheIRCompiler::emitInt32MulResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegister scratch(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label maybeNegZero, done;
  masm.mov(lhs, scratch);
  masm.branchMul32(Assembler::Overflow, rhs, scratch, failure->label());
  masm.branchTest32(Assembler::Zero, scratch, scratch, &maybeNegZero);
  masm.jump(&done);

  masm.bind(&maybeNegZero);
  masm.mov(lhs, scratch2);
  // Result is -0 if exactly one of lhs or rhs is negative.
  masm.or32(rhs, scratch2);
  masm.branchTest32(Assembler::Signed, scratch2, scratch2, failure->label());

  masm.bind(&done);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitInt32DivResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegister rem(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Prevent division by 0.
  masm.branchTest32(Assembler::Zero, rhs, rhs, failure->label());

  // Prevent negative 0 and -2147483648 / -1.
  masm.branch32(Assembler::Equal, lhs, Imm32(INT32_MIN), failure->label());

  Label notZero;
  masm.branch32(Assembler::NotEqual, lhs, Imm32(0), &notZero);
  masm.branchTest32(Assembler::Signed, rhs, rhs, failure->label());
  masm.bind(&notZero);

  masm.mov(lhs, scratch);
  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  masm.flexibleDivMod32(rhs, scratch, rem, false, volatileRegs);

  // A remainder implies a double result.
  masm.branchTest32(Assembler::NonZero, rem, rem, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitInt32ModResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Modulo takes the sign of the dividend; don't handle negative dividends
  // here.
  masm.branchTest32(Assembler::Signed, lhs, lhs, failure->label());

  // Negative divisor (could be fixed with abs)
  masm.branchTest32(Assembler::Signed, rhs, rhs, failure->label());

  // x % 0 results in NaN
  masm.branchTest32(Assembler::Zero, rhs, rhs, failure->label());

  // Prevent negative 0 and -2147483648 / -1.
  masm.branch32(Assembler::Equal, lhs, Imm32(INT32_MIN), failure->label());
  masm.mov(lhs, scratch);
  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  masm.flexibleRemainder32(rhs, scratch, false, volatileRegs);

  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

// Like AutoScratchRegisterMaybeOutput, but tries to use the ValueOperand's
// type register for the scratch register on 32-bit.
//
// Word of warning: Passing an instance of this class and AutoOutputRegister to
// functions may not work correctly, because no guarantee is given that the type
// register is used last when modifying the output's ValueOperand.
class MOZ_RAII AutoScratchRegisterMaybeOutputType {
  mozilla::Maybe<AutoScratchRegister> scratch_;
  Register scratchReg_;

 public:
  AutoScratchRegisterMaybeOutputType(CacheRegisterAllocator& alloc,
                                     MacroAssembler& masm,
                                     const AutoOutputRegister& output) {
#if defined(JS_NUNBOX32)
    scratchReg_ = output.hasValue() ? output.valueReg().typeReg() : InvalidReg;
#else
    scratchReg_ = InvalidReg;
#endif
    if (scratchReg_ == InvalidReg) {
      scratch_.emplace(alloc, masm);
      scratchReg_ = scratch_.ref();
    }
  }

  AutoScratchRegisterMaybeOutputType(
      const AutoScratchRegisterMaybeOutputType&) = delete;

  void operator=(const AutoScratchRegisterMaybeOutputType&) = delete;

  operator Register() const { return scratchReg_; }
};

bool CacheIRCompiler::emitInt32PowResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register base = allocator.useRegister(masm, reader.int32OperandId());
  Register power = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegisterMaybeOutputType scratch2(allocator, masm, output);
  AutoScratchRegister scratch3(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.pow32(base, power, scratch1, scratch2, scratch3, failure->label());

  EmitStoreResult(masm, scratch1, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitInt32BitOrResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());

  masm.mov(rhs, scratch);
  masm.or32(lhs, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}
bool CacheIRCompiler::emitInt32BitXorResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());

  masm.mov(rhs, scratch);
  masm.xor32(lhs, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}
bool CacheIRCompiler::emitInt32BitAndResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());

  masm.mov(rhs, scratch);
  masm.and32(lhs, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}
bool CacheIRCompiler::emitInt32LeftShiftResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  masm.mov(lhs, scratch);
  // Mask shift amount as specified by 12.9.3.1 Step 7
  masm.and32(Imm32(0x1F), rhs);
  masm.flexibleLshift32(rhs, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

bool CacheIRCompiler::emitInt32RightShiftResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  masm.mov(lhs, scratch);
  // Mask shift amount as specified by 12.9.4.1 Step 7
  masm.and32(Imm32(0x1F), rhs);
  masm.flexibleRshift32Arithmetic(rhs, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

bool CacheIRCompiler::emitInt32URightShiftResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register lhs = allocator.useRegister(masm, reader.int32OperandId());
  Register rhs = allocator.useRegister(masm, reader.int32OperandId());
  bool allowDouble = reader.readBool();
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.mov(lhs, scratch);
  // Mask shift amount as specified by 12.9.4.1 Step 7
  masm.and32(Imm32(0x1F), rhs);
  masm.flexibleRshift32(rhs, scratch);
  Label intDone, floatDone;
  if (allowDouble) {
    Label toUint;
    masm.branchTest32(Assembler::Signed, scratch, scratch, &toUint);
    masm.jump(&intDone);

    masm.bind(&toUint);
    ScratchDoubleScope fpscratch(masm);
    masm.convertUInt32ToDouble(scratch, fpscratch);
    masm.boxDouble(fpscratch, output.valueReg(), fpscratch);
    masm.jump(&floatDone);
  } else {
    masm.branchTest32(Assembler::Signed, scratch, scratch, failure->label());
  }
  masm.bind(&intDone);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  masm.bind(&floatDone);
  return true;
}

bool CacheIRCompiler::emitInt32NegationResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register val = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Guard against 0 and MIN_INT by checking if low 31-bits are all zero.
  // Both of these result in a double.
  masm.branchTest32(Assembler::Zero, val, Imm32(0x7fffffff), failure->label());
  masm.mov(val, scratch);
  masm.neg32(scratch);
  masm.tagValue(JSVAL_TYPE_INT32, scratch, output.valueReg());
  return true;
}

bool CacheIRCompiler::emitInt32IncResult() {
  AutoOutputRegister output(*this);
  Register input = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.mov(input, scratch);
  masm.branchAdd32(Assembler::Overflow, Imm32(1), scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

bool CacheIRCompiler::emitInt32DecResult() {
  AutoOutputRegister output(*this);
  Register input = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.mov(input, scratch);
  masm.branchSub32(Assembler::Overflow, Imm32(1), scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);

  return true;
}

bool CacheIRCompiler::emitInt32NotResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register val = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  masm.mov(val, scratch);
  masm.not32(scratch);
  masm.tagValue(JSVAL_TYPE_INT32, scratch, output.valueReg());
  return true;
}

bool CacheIRCompiler::emitDoubleNegationResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  AutoScratchFloatRegister floatReg(this, failure);

  masm.ensureDouble(val, floatReg, floatReg.failure());
  masm.negateDouble(floatReg);
  masm.boxDouble(floatReg, output.valueReg(), floatReg);

  return true;
}

bool CacheIRCompiler::emitDoubleIncDecResult(bool isInc) {
  AutoOutputRegister output(*this);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  AutoScratchFloatRegister floatReg(this, failure);

  masm.ensureDouble(val, floatReg, floatReg.failure());
  {
    ScratchDoubleScope fpscratch(masm);
    masm.loadConstantDouble(1.0, fpscratch);
    if (isInc) {
      masm.addDouble(fpscratch, floatReg);
    } else {
      masm.subDouble(fpscratch, floatReg);
    }
  }
  masm.boxDouble(floatReg, output.valueReg(), floatReg);

  return true;
}

bool CacheIRCompiler::emitDoubleIncResult() {
  return emitDoubleIncDecResult(true);
}

bool CacheIRCompiler::emitDoubleDecResult() {
  return emitDoubleIncDecResult(false);
}

template <typename Fn, Fn fn>
bool CacheIRCompiler::emitBigIntBinaryOperationShared() {
  AutoCallVM callvm(masm, this, allocator);
  Register lhs = allocator.useRegister(masm, reader.bigIntOperandId());
  Register rhs = allocator.useRegister(masm, reader.bigIntOperandId());

  callvm.prepare();

  masm.Push(rhs);
  masm.Push(lhs);

  callvm.call<Fn, fn>();
  return true;
}

bool CacheIRCompiler::emitBigIntAddResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::add>();
}

bool CacheIRCompiler::emitBigIntSubResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::sub>();
}

bool CacheIRCompiler::emitBigIntMulResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::mul>();
}

bool CacheIRCompiler::emitBigIntDivResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::div>();
}

bool CacheIRCompiler::emitBigIntModResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::mod>();
}

bool CacheIRCompiler::emitBigIntPowResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::pow>();
}

bool CacheIRCompiler::emitBigIntBitAndResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::bitAnd>();
}

bool CacheIRCompiler::emitBigIntBitOrResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::bitOr>();
}

bool CacheIRCompiler::emitBigIntBitXorResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::bitXor>();
}

bool CacheIRCompiler::emitBigIntLeftShiftResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::lsh>();
}

bool CacheIRCompiler::emitBigIntRightShiftResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt, HandleBigInt);
  return emitBigIntBinaryOperationShared<Fn, BigInt::rsh>();
}

template <typename Fn, Fn fn>
bool CacheIRCompiler::emitBigIntUnaryOperationShared() {
  AutoCallVM callvm(masm, this, allocator);
  Register val = allocator.useRegister(masm, reader.bigIntOperandId());

  callvm.prepare();

  masm.Push(val);

  callvm.call<Fn, fn>();
  return true;
}

bool CacheIRCompiler::emitBigIntNotResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt);
  return emitBigIntUnaryOperationShared<Fn, BigInt::bitNot>();
}

bool CacheIRCompiler::emitBigIntNegationResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt);
  return emitBigIntUnaryOperationShared<Fn, BigInt::neg>();
}

bool CacheIRCompiler::emitBigIntIncResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt);
  return emitBigIntUnaryOperationShared<Fn, BigInt::inc>();
}

bool CacheIRCompiler::emitBigIntDecResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  using Fn = BigInt* (*)(JSContext*, HandleBigInt);
  return emitBigIntUnaryOperationShared<Fn, BigInt::dec>();
}

bool CacheIRCompiler::emitTruncateDoubleToUInt32() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
  Register res = allocator.defineRegister(masm, reader.int32OperandId());

  Label int32, done;
  masm.branchTestInt32(Assembler::Equal, val, &int32);

  {
    Label doneTruncate, truncateABICall;

    AutoScratchFloatRegister floatReg(this);

    masm.unboxDouble(val, floatReg);
    masm.branchTruncateDoubleMaybeModUint32(floatReg, res, &truncateABICall);
    masm.jump(&doneTruncate);

    masm.bind(&truncateABICall);
    LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                         liveVolatileFloatRegs());
    save.takeUnchecked(floatReg);
    // Bug 1451976
    save.takeUnchecked(floatReg.get().asSingle());
    masm.PushRegsInMask(save);

    masm.setupUnalignedABICall(res);
    masm.passABIArg(floatReg, MoveOp::DOUBLE);
    masm.callWithABI(BitwiseCast<void*, int32_t (*)(double)>(JS::ToInt32),
                     MoveOp::GENERAL, CheckUnsafeCallWithABI::DontCheckOther);
    masm.storeCallInt32Result(res);

    LiveRegisterSet ignore;
    ignore.add(res);
    masm.PopRegsInMaskIgnore(save, ignore);

    masm.bind(&doneTruncate);
  }
  masm.jump(&done);
  masm.bind(&int32);

  masm.unboxInt32(val, res);

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadArgumentsObjectLengthResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Get initial length value.
  masm.unboxInt32(Address(obj, ArgumentsObject::getInitialLengthSlotOffset()),
                  scratch);

  // Test if length has been overridden.
  masm.branchTest32(Assembler::NonZero, scratch,
                    Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT),
                    failure->label());

  // Shift out arguments length and return it. No need to type monitor
  // because this stub always returns int32.
  masm.rshiftPtr(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitLoadFunctionLengthResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Get the JSFunction flags.
  masm.load16ZeroExtend(Address(obj, JSFunction::offsetOfFlags()), scratch);

  // Functions with a SelfHostedLazyScript must be compiled with the slow-path
  // before the function length is known. If the length was previously resolved,
  // the length property may be shadowed.
  masm.branchTest32(
      Assembler::NonZero, scratch,
      Imm32(FunctionFlags::SELFHOSTLAZY | FunctionFlags::RESOLVED_LENGTH),
      failure->label());

  masm.loadFunctionLength(obj, scratch, scratch, failure->label());
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitLoadStringLengthResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register str = allocator.useRegister(masm, reader.stringOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  masm.loadStringLength(str, scratch);
  EmitStoreResult(masm, scratch, JSVAL_TYPE_INT32, output);
  return true;
}

bool CacheIRCompiler::emitLoadStringCharResult(StringOperandId strId,
                                               Int32OperandId indexId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register str = allocator.useRegister(masm, strId);
  Register index = allocator.useRegister(masm, indexId);
  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Bounds check, load string char.
  masm.spectreBoundsCheck32(index, Address(str, JSString::offsetOfLength()),
                            scratch1, failure->label());
  masm.loadStringChar(str, index, scratch1, scratch2, failure->label());

  // Load StaticString for this char.
  masm.boundsCheck32PowerOfTwo(scratch1, StaticStrings::UNIT_STATIC_LIMIT,
                               failure->label());
  masm.movePtr(ImmPtr(&cx_->staticStrings().unitStaticTable), scratch2);
  masm.loadPtr(BaseIndex(scratch2, scratch1, ScalePointer), scratch2);

  EmitStoreResult(masm, scratch2, JSVAL_TYPE_STRING, output);
  return true;
}

bool CacheIRCompiler::emitLoadArgumentsObjectArgResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Get initial length value.
  masm.unboxInt32(Address(obj, ArgumentsObject::getInitialLengthSlotOffset()),
                  scratch1);

  // Ensure no overridden length/element.
  masm.branchTest32(Assembler::NonZero, scratch1,
                    Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT |
                          ArgumentsObject::ELEMENT_OVERRIDDEN_BIT),
                    failure->label());

  // Bounds check.
  masm.rshift32(Imm32(ArgumentsObject::PACKED_BITS_COUNT), scratch1);
  masm.spectreBoundsCheck32(index, scratch1, scratch2, failure->label());

  // Load ArgumentsData.
  masm.loadPrivate(Address(obj, ArgumentsObject::getDataSlotOffset()),
                   scratch1);

  // Fail if we have a RareArgumentsData (elements were deleted).
  masm.branchPtr(Assembler::NotEqual,
                 Address(scratch1, offsetof(ArgumentsData, rareData)),
                 ImmWord(0), failure->label());

  // Guard the argument is not a FORWARD_TO_CALL_SLOT MagicValue.
  BaseValueIndex argValue(scratch1, index, ArgumentsData::offsetOfArgs());
  masm.branchTestMagic(Assembler::Equal, argValue, failure->label());
  masm.loadValue(argValue, output.valueReg());
  return true;
}

bool CacheIRCompiler::emitLoadDenseElementResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch1);

  // Bounds check.
  Address initLength(scratch1, ObjectElements::offsetOfInitializedLength());
  masm.spectreBoundsCheck32(index, initLength, scratch2, failure->label());

  // Hole check.
  BaseObjectElementIndex element(scratch1, index);
  masm.branchTestMagic(Assembler::Equal, element, failure->label());
  masm.loadTypedOrValue(element, output);
  return true;
}

bool CacheIRCompiler::emitGuardIndexIsNonNegative(Int32OperandId indexId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register index = allocator.useRegister(masm, indexId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branch32(Assembler::LessThan, index, Imm32(0), failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardIndexGreaterThanDenseInitLength(
    ObjOperandId objId, Int32OperandId indexId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Register index = allocator.useRegister(masm, indexId);
  AutoScratchRegister scratch(allocator, masm);
  AutoSpectreBoundsScratchRegister spectreScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  // Ensure index >= initLength.
  Label outOfBounds;
  Address capacity(scratch, ObjectElements::offsetOfInitializedLength());
  masm.spectreBoundsCheck32(index, capacity, spectreScratch, &outOfBounds);
  masm.jump(failure->label());
  masm.bind(&outOfBounds);

  return true;
}

bool CacheIRCompiler::emitGuardIndexGreaterThanArrayLength(
    ObjOperandId objId, Int32OperandId indexId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Register index = allocator.useRegister(masm, indexId);
  AutoScratchRegister scratch(allocator, masm);
  AutoSpectreBoundsScratchRegister spectreScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  // Ensure index >= length;
  Label outOfBounds;
  Address length(scratch, ObjectElements::offsetOfLength());
  masm.spectreBoundsCheck32(index, length, spectreScratch, &outOfBounds);
  masm.jump(failure->label());
  masm.bind(&outOfBounds);
  return true;
}

bool CacheIRCompiler::emitGuardIndexIsValidUpdateOrAdd(ObjOperandId objId,
                                                       Int32OperandId indexId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, objId);
  Register index = allocator.useRegister(masm, indexId);
  AutoScratchRegister scratch(allocator, masm);
  AutoSpectreBoundsScratchRegister spectreScratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  Label success;

  // If length is writable, branch to &success.  All indices are writable.
  Address flags(scratch, ObjectElements::offsetOfFlags());
  masm.branchTest32(Assembler::Zero, flags,
                    Imm32(ObjectElements::Flags::NONWRITABLE_ARRAY_LENGTH),
                    &success);

  // Otherwise, ensure index is in bounds.
  Address length(scratch, ObjectElements::offsetOfLength());
  masm.spectreBoundsCheck32(index, length, spectreScratch,
                            /* failure = */ failure->label());
  masm.bind(&success);
  return true;
}

bool CacheIRCompiler::emitGuardTagNotEqual() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register lhs = allocator.useRegister(masm, reader.valueTagOperandId());
  Register rhs = allocator.useRegister(masm, reader.valueTagOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label done;
  masm.branch32(Assembler::Equal, lhs, rhs, failure->label());

  // If both lhs and rhs are numbers, can't use tag comparison to do inequality
  // comparison
  masm.branchTestNumber(Assembler::NotEqual, lhs, &done);
  masm.branchTestNumber(Assembler::NotEqual, rhs, &done);
  masm.jump(failure->label());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitGuardXrayExpandoShapeAndDefaultProto() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  bool hasExpando = reader.readBool();
  StubFieldOffset shapeWrapper(reader.stubOffset(), StubField::Type::JSObject);

  AutoScratchRegister scratch(allocator, masm);
  Maybe<AutoScratchRegister> scratch2, scratch3;
  if (hasExpando) {
    scratch2.emplace(allocator, masm);
    scratch3.emplace(allocator, masm);
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.loadPtr(Address(obj, ProxyObject::offsetOfReservedSlots()), scratch);
  Address holderAddress(scratch,
                        sizeof(Value) * GetXrayJitInfo()->xrayHolderSlot);
  Address expandoAddress(scratch, NativeObject::getFixedSlotOffset(
                                      GetXrayJitInfo()->holderExpandoSlot));

  if (hasExpando) {
    masm.branchTestObject(Assembler::NotEqual, holderAddress, failure->label());
    masm.unboxObject(holderAddress, scratch);
    masm.branchTestObject(Assembler::NotEqual, expandoAddress,
                          failure->label());
    masm.unboxObject(expandoAddress, scratch);

    // Unwrap the expando before checking its shape.
    masm.loadPtr(Address(scratch, ProxyObject::offsetOfReservedSlots()),
                 scratch);
    masm.unboxObject(
        Address(scratch, js::detail::ProxyReservedSlots::offsetOfPrivateSlot()),
        scratch);

    emitLoadStubField(shapeWrapper, scratch2.ref());
    LoadShapeWrapperContents(masm, scratch2.ref(), scratch2.ref(),
                             failure->label());
    masm.branchTestObjShape(Assembler::NotEqual, scratch, *scratch2, *scratch3,
                            scratch, failure->label());

    // The reserved slots on the expando should all be in fixed slots.
    Address protoAddress(scratch, NativeObject::getFixedSlotOffset(
                                      GetXrayJitInfo()->expandoProtoSlot));
    masm.branchTestUndefined(Assembler::NotEqual, protoAddress,
                             failure->label());
  } else {
    Label done;
    masm.branchTestObject(Assembler::NotEqual, holderAddress, &done);
    masm.unboxObject(holderAddress, scratch);
    masm.branchTestObject(Assembler::Equal, expandoAddress, failure->label());
    masm.bind(&done);
  }

  return true;
}

bool CacheIRCompiler::emitGuardNoAllocationMetadataBuilder() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchPtr(Assembler::NotEqual,
                 AbsoluteAddress(cx_->realm()->addressOfMetadataBuilder()),
                 ImmWord(0), failure->label());

  return true;
}

bool CacheIRCompiler::emitGuardObjectGroupNotPretenured(uint32_t groupOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  StubFieldOffset group(groupOffset, StubField::Type::ObjectGroup);
  emitLoadStubField(group, scratch);

  masm.branchIfPretenuredGroup(scratch, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardFunctionHasJitEntry(ObjOperandId funId,
                                                   bool constructing) {
  Register fun = allocator.useRegister(masm, funId);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchIfFunctionHasNoJitEntry(fun, constructing, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardFunctionIsNative() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Ensure obj is not an interpreted function.
  masm.branchIfInterpreted(obj, /*isConstructing =*/false, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardFunctionIsConstructor() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register funcReg = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Ensure obj is a constructor
  masm.branchTestFunctionFlags(funcReg, FunctionFlags::CONSTRUCTOR,
                               Assembler::Zero, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardNotClassConstructor() {
  Register fun = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.branchFunctionKind(Assembler::Equal, FunctionFlags::ClassConstructor,
                          fun, scratch, failure->label());
  return true;
}

bool CacheIRCompiler::emitLoadDenseElementHoleResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);

  if (!output.hasValue()) {
    masm.assumeUnreachable(
        "Should have monitored undefined value after attaching stub");
    return true;
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Make sure the index is nonnegative.
  masm.branch32(Assembler::LessThan, index, Imm32(0), failure->label());

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch1);

  // Guard on the initialized length.
  Label hole;
  Address initLength(scratch1, ObjectElements::offsetOfInitializedLength());
  masm.spectreBoundsCheck32(index, initLength, scratch2, &hole);

  // Load the value.
  Label done;
  masm.loadValue(BaseObjectElementIndex(scratch1, index), output.valueReg());
  masm.branchTestMagic(Assembler::NotEqual, output.valueReg(), &done);

  // Load undefined for the hole.
  masm.bind(&hole);
  masm.moveValue(UndefinedValue(), output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadTypedElementExistsResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  TypedThingLayout layout = reader.typedThingLayout();
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Label outOfBounds, done;

  // Bound check.
  LoadTypedThingLength(masm, layout, obj, scratch);
  masm.branch32(Assembler::BelowOrEqual, scratch, index, &outOfBounds);
  EmitStoreBoolean(masm, true, output);
  masm.jump(&done);

  masm.bind(&outOfBounds);
  EmitStoreBoolean(masm, false, output);

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadDenseElementExistsResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  // Bounds check. Unsigned compare sends negative indices to next IC.
  Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
  masm.branch32(Assembler::BelowOrEqual, initLength, index, failure->label());

  // Hole check.
  BaseObjectElementIndex element(scratch, index);
  masm.branchTestMagic(Assembler::Equal, element, failure->label());

  EmitStoreBoolean(masm, true, output);
  return true;
}

bool CacheIRCompiler::emitLoadDenseElementHoleExistsResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Make sure the index is nonnegative.
  masm.branch32(Assembler::LessThan, index, Imm32(0), failure->label());

  // Load obj->elements.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

  // Guard on the initialized length.
  Label hole;
  Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
  masm.branch32(Assembler::BelowOrEqual, initLength, index, &hole);

  // Load value and replace with true.
  Label done;
  BaseObjectElementIndex element(scratch, index);
  masm.branchTestMagic(Assembler::Equal, element, &hole);
  EmitStoreBoolean(masm, true, output);
  masm.jump(&done);

  // Load false for the hole.
  masm.bind(&hole);
  EmitStoreBoolean(masm, false, output);

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitArrayJoinResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  ObjOperandId objId = reader.objOperandId();

  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, objId);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Load obj->elements in scratch.
  masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);
  Address lengthAddr(scratch, ObjectElements::offsetOfLength());

  // If array length is 0, return empty string.
  Label finished;

  {
    Label arrayNotEmpty;
    masm.branch32(Assembler::NotEqual, lengthAddr, Imm32(0), &arrayNotEmpty);
    masm.movePtr(ImmGCPtr(cx_->names().empty), scratch);
    masm.tagValue(JSVAL_TYPE_STRING, scratch, output.valueReg());
    masm.jump(&finished);
    masm.bind(&arrayNotEmpty);
  }

  // Otherwise, handle array length 1 case.
  masm.branch32(Assembler::NotEqual, lengthAddr, Imm32(1), failure->label());

  // But only if initializedLength is also 1.
  Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
  masm.branch32(Assembler::NotEqual, initLength, Imm32(1), failure->label());

  // And only if elem0 is a string.
  Address elementAddr(scratch, 0);
  masm.branchTestString(Assembler::NotEqual, elementAddr, failure->label());

  // Store the value.
  masm.loadValue(elementAddr, output.valueReg());

  masm.bind(&finished);

  return true;
}

bool CacheIRCompiler::emitStoreTypedElement() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  TypedThingLayout layout = reader.typedThingLayout();
  Scalar::Type type = reader.scalarType();
  Register index = allocator.useRegister(masm, reader.int32OperandId());

  Maybe<Register> valInt32;
  Maybe<Register> valBigInt;
  switch (type) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
    case Scalar::Uint8Clamped:
      valInt32.emplace(allocator.useRegister(masm, reader.int32OperandId()));
      break;

    case Scalar::Float32:
    case Scalar::Float64:
      // Float register must be preserved. The SetProp ICs use the fact that
      // baseline has them available, as well as fixed temps on
      // LSetPropertyCache.
      allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
      break;

    case Scalar::BigInt64:
    case Scalar::BigUint64:
      valBigInt.emplace(allocator.useRegister(masm, reader.bigIntOperandId()));
      break;

    case Scalar::MaxTypedArrayViewType:
    case Scalar::Int64:
      MOZ_CRASH("Unsupported TypedArray type");
  }

  bool handleOOB = reader.readBool();

  AutoScratchRegister scratch1(allocator, masm);
  Maybe<AutoScratchRegister> scratch2;
  Maybe<AutoSpectreBoundsScratchRegister> spectreScratch;
  if (Scalar::isBigIntType(type)) {
    scratch2.emplace(allocator, masm);
  } else {
    spectreScratch.emplace(allocator, masm);
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Bounds check.
  Label done;
  Register spectreTemp = scratch2 ? scratch2->get() : spectreScratch->get();
  LoadTypedThingLength(masm, layout, obj, scratch1);
  masm.spectreBoundsCheck32(index, scratch1, spectreTemp,
                            handleOOB ? &done : failure->label());

  // Load the elements vector.
  LoadTypedThingData(masm, layout, obj, scratch1);

  BaseIndex dest(scratch1, index, ScaleFromElemWidth(Scalar::byteSize(type)));

  if (Scalar::isBigIntType(type)) {
#ifdef JS_PUNBOX64
    Register64 temp(scratch2->get());
#else
    // We don't have more registers available on x86, so spill |obj|.
    masm.push(obj);
    Register64 temp(scratch2->get(), obj);
#endif

    masm.loadBigInt64(*valBigInt, temp);
    masm.storeToTypedBigIntArray(type, temp, dest);

#ifndef JS_PUNBOX64
    masm.pop(obj);
#endif
  } else if (type == Scalar::Float32) {
    ScratchFloat32Scope fpscratch(masm);
    masm.convertDoubleToFloat32(FloatReg0, fpscratch);
    masm.storeToTypedFloatArray(type, fpscratch, dest);
  } else if (type == Scalar::Float64) {
    masm.storeToTypedFloatArray(type, FloatReg0, dest);
  } else {
    masm.storeToTypedIntArray(type, *valInt32, dest);
  }

  masm.bind(&done);
  return true;
}

static bool CanNurseryAllocateBigInt(JSContext* cx) {
  JS::Zone* zone = cx->zone();
  return zone->runtimeFromAnyThread()->gc.nursery().canAllocateBigInts() &&
         zone->allocNurseryBigInts;
}

static void EmitAllocateBigInt(MacroAssembler& masm, Register result,
                               Register temp, const LiveRegisterSet& liveSet,
                               Label* fail, bool attemptNursery) {
  Label fallback, done;
  masm.newGCBigInt(result, temp, &fallback, attemptNursery);
  masm.jump(&done);
  {
    masm.bind(&fallback);
    masm.PushRegsInMask(liveSet);

    masm.setupUnalignedABICall(temp);
    masm.loadJSContext(temp);
    masm.passABIArg(temp);
    masm.move32(Imm32(attemptNursery), result);
    masm.passABIArg(result);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, jit::AllocateBigIntNoGC));
    masm.storeCallPointerResult(result);

    masm.PopRegsInMask(liveSet);
    masm.branchPtr(Assembler::Equal, result, ImmWord(0), fail);
  }
  masm.bind(&done);
}

bool CacheIRCompiler::emitLoadTypedElementResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());
  TypedThingLayout layout = reader.typedThingLayout();
  Scalar::Type type = reader.scalarType();
  bool handleOOB = reader.readBool();

  AutoScratchRegister scratch1(allocator, masm);
#ifdef JS_PUNBOX64
  AutoScratchRegister scratch2(allocator, masm);
#else
  // There are too few registers available on x86, so we may need to reuse the
  // output's scratch register.
  AutoScratchRegisterMaybeOutput scratch2(allocator, masm, output);
#endif

  // BigInt values are always boxed.
  MOZ_ASSERT_IF(Scalar::isBigIntType(type), output.hasValue());

  if (!output.hasValue()) {
    if (type == Scalar::Float32 || type == Scalar::Float64) {
      if (output.type() != JSVAL_TYPE_DOUBLE) {
        masm.assumeUnreachable(
            "Should have monitored double after attaching stub");
        return true;
      }
    } else {
      if (output.type() != JSVAL_TYPE_INT32 &&
          output.type() != JSVAL_TYPE_DOUBLE) {
        masm.assumeUnreachable(
            "Should have monitored int32 after attaching stub");
        return true;
      }
    }
  }

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Bounds check.
  Label outOfBounds;
  LoadTypedThingLength(masm, layout, obj, scratch1);
  masm.spectreBoundsCheck32(index, scratch1, scratch2,
                            handleOOB ? &outOfBounds : failure->label());

  // Allocate BigInt if needed. The code after this should be infallible.
  Maybe<Register> bigInt;
  if (Scalar::isBigIntType(type)) {
    bigInt.emplace(output.valueReg().scratchReg());

    LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                         liveVolatileFloatRegs());
    save.takeUnchecked(scratch1);
    save.takeUnchecked(scratch2);
    save.takeUnchecked(output);

    bool attemptNursery = CanNurseryAllocateBigInt(cx_);
    EmitAllocateBigInt(masm, *bigInt, scratch1, save, failure->label(),
                       attemptNursery);
  }

  // Load the elements vector.
  LoadTypedThingData(masm, layout, obj, scratch1);

  // Load the value.
  BaseIndex source(scratch1, index, ScaleFromElemWidth(Scalar::byteSize(type)));

  if (output.hasValue()) {
    if (Scalar::isBigIntType(type)) {
#ifdef JS_PUNBOX64
      Register64 temp(scratch2);
#else
      // We don't have more registers available on x86, so spill |obj| and
      // additionally use the output's type register.
      MOZ_ASSERT(output.valueReg().scratchReg() != output.valueReg().typeReg());
      masm.push(obj);
      Register64 temp(output.valueReg().typeReg(), obj);
#endif

      masm.loadFromTypedBigIntArray(type, source, *bigInt, temp);

#ifndef JS_PUNBOX64
      masm.pop(obj);
#endif

      masm.tagValue(JSVAL_TYPE_BIGINT, *bigInt, output.valueReg());
    } else {
      masm.loadFromTypedArray(type, source, output.valueReg(),
                              *allowDoubleResult_, scratch1, failure->label());
    }
  } else {
    bool needGpr = (type == Scalar::Int8 || type == Scalar::Uint8 ||
                    type == Scalar::Int16 || type == Scalar::Uint16 ||
                    type == Scalar::Uint8Clamped || type == Scalar::Int32);
    if (needGpr && output.type() == JSVAL_TYPE_DOUBLE) {
      // Load the element as integer, then convert it to double.
      masm.loadFromTypedArray(type, source, AnyRegister(scratch1), scratch1,
                              failure->label());
      masm.convertInt32ToDouble(source, output.typedReg().fpu());
    } else {
      masm.loadFromTypedArray(type, source, output.typedReg(), scratch1,
                              failure->label());
    }
  }

  if (handleOOB) {
    Label done;
    masm.jump(&done);

    masm.bind(&outOfBounds);
    if (output.hasValue()) {
      masm.moveValue(UndefinedValue(), output.valueReg());
    } else {
      masm.assumeUnreachable("Should have monitored undefined result");
    }

    masm.bind(&done);
  }

  return true;
}

bool CacheIRCompiler::emitStoreTypedObjectScalarProperty() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  StubFieldOffset offset(reader.stubOffset(), StubField::Type::RawWord);
  TypedThingLayout layout = reader.typedThingLayout();
  Scalar::Type type = reader.scalarType();

  Maybe<Register> valInt32;
  Maybe<Register> valBigInt;
  switch (type) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
    case Scalar::Uint8Clamped:
      valInt32.emplace(allocator.useRegister(masm, reader.int32OperandId()));
      break;

    case Scalar::Float32:
    case Scalar::Float64:
      // Float register must be preserved. The SetProp ICs use the fact that
      // baseline has them available, as well as fixed temps on
      // LSetPropertyCache.
      allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
      break;

    case Scalar::BigInt64:
    case Scalar::BigUint64:
      valBigInt.emplace(allocator.useRegister(masm, reader.bigIntOperandId()));
      break;

    case Scalar::MaxTypedArrayViewType:
    case Scalar::Int64:
      MOZ_CRASH("Unsupported TypedArray type");
  }

  AutoScratchRegister scratch(allocator, masm);
  Maybe<AutoScratchRegister> bigIntScratch;
  if (Scalar::isBigIntType(type)) {
    bigIntScratch.emplace(allocator, masm);
  }

  // Compute the address being written to.
  LoadTypedThingData(masm, layout, obj, scratch);
  Address dest = emitAddressFromStubField(offset, scratch);

  if (Scalar::isBigIntType(type)) {
#ifdef JS_PUNBOX64
    Register64 temp(bigIntScratch->get());
#else
    // We don't have more registers available on x86, so spill |obj|.
    masm.push(obj);
    Register64 temp(bigIntScratch->get(), obj);
#endif

    masm.loadBigInt64(*valBigInt, temp);
    masm.storeToTypedBigIntArray(type, temp, dest);

#ifndef JS_PUNBOX64
    masm.pop(obj);
#endif
  } else if (type == Scalar::Float32) {
    ScratchFloat32Scope fpscratch(masm);
    masm.convertDoubleToFloat32(FloatReg0, fpscratch);
    masm.storeToTypedFloatArray(type, fpscratch, dest);
  } else if (type == Scalar::Float64) {
    masm.storeToTypedFloatArray(type, FloatReg0, dest);
  } else {
    masm.storeToTypedIntArray(type, *valInt32, dest);
  }
  return true;
}

bool CacheIRCompiler::emitLoadTypedObjectResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  MOZ_ASSERT(output.hasValue());
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  TypedThingLayout layout = reader.typedThingLayout();
  uint32_t typeDescr = reader.typeDescrKey();
  StubFieldOffset offset(reader.stubOffset(), StubField::Type::RawWord);

  // Allocate BigInt if needed. The code after this should be infallible.
  Maybe<Register> bigInt;
  if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
    Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);
    if (Scalar::isBigIntType(type)) {
      FailurePath* failure;
      if (!addFailurePath(&failure)) {
        return false;
      }

      bigInt.emplace(output.valueReg().scratchReg());

      LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                           liveVolatileFloatRegs());
      save.takeUnchecked(scratch1);
      save.takeUnchecked(scratch2);
      save.takeUnchecked(output);

      bool attemptNursery = CanNurseryAllocateBigInt(cx_);
      EmitAllocateBigInt(masm, *bigInt, scratch1, save, failure->label(),
                         attemptNursery);
    }
  }

  // Get the object's data pointer.
  LoadTypedThingData(masm, layout, obj, scratch1);

  // Get the address being written to.
  Address fieldAddr = emitAddressFromStubField(offset, scratch1);

  if (SimpleTypeDescrKeyIsScalar(typeDescr)) {
    Scalar::Type type = ScalarTypeFromSimpleTypeDescrKey(typeDescr);

    if (Scalar::isBigIntType(type)) {
#ifdef JS_PUNBOX64
      Register64 temp(scratch2);
#else
      // We don't have more registers available on x86, so spill |obj|.
      masm.push(obj);
      Register64 temp(scratch2, obj);
#endif

      masm.loadFromTypedBigIntArray(type, fieldAddr, *bigInt, temp);

#ifndef JS_PUNBOX64
      masm.pop(obj);
#endif

      masm.tagValue(JSVAL_TYPE_BIGINT, *bigInt, output.valueReg());
    } else {
      masm.loadFromTypedArray(type, fieldAddr, output.valueReg(),
                              /* allowDouble = */ true, scratch2, nullptr);
    }
  } else {
    ReferenceType type = ReferenceTypeFromSimpleTypeDescrKey(typeDescr);
    switch (type) {
      case ReferenceType::TYPE_ANY:
        masm.loadValue(fieldAddr, output.valueReg());
        break;

      case ReferenceType::TYPE_WASM_ANYREF:
        // TODO/AnyRef-boxing: With boxed immediates and strings this may be
        // more complicated.
      case ReferenceType::TYPE_OBJECT: {
        Label notNull, done;
        masm.loadPtr(fieldAddr, scratch2);
        masm.branchTestPtr(Assembler::NonZero, scratch2, scratch2, &notNull);
        masm.moveValue(NullValue(), output.valueReg());
        masm.jump(&done);
        masm.bind(&notNull);
        masm.tagValue(JSVAL_TYPE_OBJECT, scratch2, output.valueReg());
        masm.bind(&done);
        break;
      }

      case ReferenceType::TYPE_STRING:
        masm.loadPtr(fieldAddr, scratch2);
        masm.tagValue(JSVAL_TYPE_STRING, scratch2, output.valueReg());
        break;

      default:
        MOZ_CRASH("Invalid ReferenceTypeDescr");
    }
  }
  return true;
}

bool CacheIRCompiler::emitLoadObjectResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());

  if (output.hasValue()) {
    masm.tagValue(JSVAL_TYPE_OBJECT, obj, output.valueReg());
  } else {
    masm.mov(obj, output.typedReg().gpr());
  }

  return true;
}

bool CacheIRCompiler::emitLoadInt32Result() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register val = allocator.useRegister(masm, reader.int32OperandId());

  if (output.hasValue()) {
    masm.tagValue(JSVAL_TYPE_INT32, val, output.valueReg());
  } else {
    masm.mov(val, output.typedReg().gpr());
  }

  return true;
}

bool CacheIRCompiler::emitLoadDoubleResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

#ifdef DEBUG
  Label ok;
  masm.branchTestDouble(Assembler::Equal, val, &ok);
  masm.branchTestInt32(Assembler::Equal, val, &ok);
  masm.assumeUnreachable("input must be double or int32");
  masm.bind(&ok);
#endif

  masm.moveValue(val, output.valueReg());
  masm.convertInt32ValueToDouble(output.valueReg());

  return true;
}

bool CacheIRCompiler::emitLoadTypeOfObjectResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Label slowCheck, isObject, isCallable, isUndefined, done;
  masm.typeOfObject(obj, scratch, &slowCheck, &isObject, &isCallable,
                    &isUndefined);

  masm.bind(&isCallable);
  masm.moveValue(StringValue(cx_->names().function), output.valueReg());
  masm.jump(&done);

  masm.bind(&isUndefined);
  masm.moveValue(StringValue(cx_->names().undefined), output.valueReg());
  masm.jump(&done);

  masm.bind(&isObject);
  masm.moveValue(StringValue(cx_->names().object), output.valueReg());
  masm.jump(&done);

  {
    masm.bind(&slowCheck);
    LiveRegisterSet save(GeneralRegisterSet::Volatile(),
                         liveVolatileFloatRegs());
    masm.PushRegsInMask(save);

    masm.setupUnalignedABICall(scratch);
    masm.passABIArg(obj);
    masm.movePtr(ImmPtr(cx_->runtime()), scratch);
    masm.passABIArg(scratch);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, TypeOfObject));
    masm.mov(ReturnReg, scratch);

    LiveRegisterSet ignore;
    ignore.add(scratch);
    masm.PopRegsInMaskIgnore(save, ignore);

    masm.tagValue(JSVAL_TYPE_STRING, scratch, output.valueReg());
  }

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadInt32TruthyResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

  Label ifFalse, done;
  masm.branchTestInt32Truthy(false, val, &ifFalse);
  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&ifFalse);
  masm.moveValue(BooleanValue(false), output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadStringTruthyResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register str = allocator.useRegister(masm, reader.stringOperandId());

  Label ifFalse, done;
  masm.branch32(Assembler::Equal, Address(str, JSString::offsetOfLength()),
                Imm32(0), &ifFalse);
  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&ifFalse);
  masm.moveValue(BooleanValue(false), output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadDoubleTruthyResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());

  AutoScratchFloatRegister floatReg(this);

  Label ifFalse, done;

  masm.unboxDouble(val, floatReg);

  masm.branchTestDoubleTruthy(false, floatReg, &ifFalse);
  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&ifFalse);
  masm.moveValue(BooleanValue(false), output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadObjectTruthyResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Label emulatesUndefined, slowPath, done;
  masm.branchIfObjectEmulatesUndefined(obj, scratch, &slowPath,
                                       &emulatesUndefined);
  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&emulatesUndefined);
  masm.moveValue(BooleanValue(false), output.valueReg());
  masm.jump(&done);

  masm.bind(&slowPath);
  masm.setupUnalignedABICall(scratch);
  masm.passABIArg(obj);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, js::EmulatesUndefined));
  masm.convertBoolToInt32(ReturnReg, ReturnReg);
  masm.xor32(Imm32(1), ReturnReg);
  masm.tagValue(JSVAL_TYPE_BOOLEAN, ReturnReg, output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadBigIntTruthyResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register bigInt = allocator.useRegister(masm, reader.bigIntOperandId());

  Label ifFalse, done;
  masm.branch32(Assembler::Equal,
                Address(bigInt, BigInt::offsetOfDigitLength()), Imm32(0),
                &ifFalse);
  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&ifFalse);
  masm.moveValue(BooleanValue(false), output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitLoadNewObjectFromTemplateResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegister obj(allocator, masm);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  TemplateObject templateObj(objectStubFieldUnchecked(reader.stubOffset()));

  // Consume the disambiguation id (2 halves)
  mozilla::Unused << reader.uint32Immediate();
  mozilla::Unused << reader.uint32Immediate();

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.createGCObject(obj, scratch, templateObj, gc::DefaultHeap,
                      failure->label());
  masm.tagValue(JSVAL_TYPE_OBJECT, obj, output.valueReg());
  return true;
}

bool CacheIRCompiler::emitComparePointerResultShared(bool symbol) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register left = symbol ? allocator.useRegister(masm, reader.symbolOperandId())
                         : allocator.useRegister(masm, reader.objOperandId());
  Register right = symbol
                       ? allocator.useRegister(masm, reader.symbolOperandId())
                       : allocator.useRegister(masm, reader.objOperandId());
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  Label ifTrue, done;
  masm.branchPtr(JSOpToCondition(op, /* signed = */ true), left, right,
                 &ifTrue);

  EmitStoreBoolean(masm, false, output);
  masm.jump(&done);

  masm.bind(&ifTrue);
  EmitStoreBoolean(masm, true, output);
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitCompareObjectResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  return emitComparePointerResultShared(false);
}

bool CacheIRCompiler::emitCompareSymbolResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  return emitComparePointerResultShared(true);
}

bool CacheIRCompiler::emitCompareInt32Result(JSOp op, Int32OperandId lhsId,
                                             Int32OperandId rhsId) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register left = allocator.useRegister(masm, lhsId);
  Register right = allocator.useRegister(masm, rhsId);

  Label ifTrue, done;
  masm.branch32(JSOpToCondition(op, /* signed = */ true), left, right, &ifTrue);

  EmitStoreBoolean(masm, false, output);
  masm.jump(&done);

  masm.bind(&ifTrue);
  EmitStoreBoolean(masm, true, output);
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitCompareDoubleResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // Float register must be preserved. The Compare ICs use the fact that
  // baseline has them available, as well as fixed temps on LBinaryBoolCache.
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg1);
  JSOp op = reader.jsop();

  Label done, ifTrue;
  masm.branchDouble(JSOpToDoubleCondition(op), FloatReg0, FloatReg1, &ifTrue);
  EmitStoreBoolean(masm, false, output);
  masm.jump(&done);

  masm.bind(&ifTrue);
  EmitStoreBoolean(masm, true, output);
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitCompareBigIntResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register lhs = allocator.useRegister(masm, reader.bigIntOperandId());
  Register rhs = allocator.useRegister(masm, reader.bigIntOperandId());
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  if (op == JSOp::Le || op == JSOp::Gt) {
    masm.passABIArg(rhs);
    masm.passABIArg(lhs);
  } else {
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);
  }

  using Fn = bool (*)(BigInt*, BigInt*);
  Fn fn;
  if (op == JSOp::Eq || op == JSOp::StrictEq) {
    fn = jit::BigIntEqual<EqualityKind::Equal>;
  } else if (op == JSOp::Ne || op == JSOp::StrictNe) {
    fn = jit::BigIntEqual<EqualityKind::NotEqual>;
  } else if (op == JSOp::Lt || op == JSOp::Gt) {
    fn = jit::BigIntCompare<ComparisonKind::LessThan>;
  } else {
    MOZ_ASSERT(op == JSOp::Le || op == JSOp::Ge);
    fn = jit::BigIntCompare<ComparisonKind::GreaterThanOrEqual>;
  }

  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, fn));
  masm.storeCallBoolResult(scratch);

  LiveRegisterSet ignore;
  ignore.add(scratch);
  masm.PopRegsInMaskIgnore(save, ignore);

  EmitStoreResult(masm, scratch, JSVAL_TYPE_BOOLEAN, output);
  return true;
}

bool CacheIRCompiler::emitCompareBigIntInt32ResultShared(
    Register bigInt, Register int32, Register scratch1, Register scratch2,
    JSOp op, const AutoOutputRegister& output) {
  MOZ_ASSERT(IsLooseEqualityOp(op) || IsRelationalOp(op));

  static_assert(std::is_same_v<BigInt::Digit, uintptr_t>,
                "BigInt digit can be loaded in a pointer-sized register");
  static_assert(sizeof(BigInt::Digit) >= sizeof(uint32_t),
                "BigInt digit stores at least an uint32");

  // Test for too large numbers.
  //
  // If the absolute value of the BigInt can't be expressed in an uint32/uint64,
  // the result of the comparison is a constant.
  Label ifTrue, ifFalse;
  if (op == JSOp::Eq || op == JSOp::Ne) {
    Label* tooLarge = op == JSOp::Eq ? &ifFalse : &ifTrue;
    masm.branch32(Assembler::GreaterThan,
                  Address(bigInt, BigInt::offsetOfDigitLength()), Imm32(1),
                  tooLarge);
  } else {
    Label doCompare;
    masm.branch32(Assembler::LessThanOrEqual,
                  Address(bigInt, BigInt::offsetOfDigitLength()), Imm32(1),
                  &doCompare);

    // Still need to take the sign-bit into account for relational operations.
    if (op == JSOp::Lt || op == JSOp::Le) {
      masm.branchIfNegativeBigInt(bigInt, &ifTrue);
      masm.jump(&ifFalse);
    } else {
      masm.branchIfNegativeBigInt(bigInt, &ifFalse);
      masm.jump(&ifTrue);
    }

    masm.bind(&doCompare);
  }

  // Test for mismatched signs and, if the signs are equal, load |abs(x)| in
  // |scratch1| and |abs(y)| in |scratch2| and then compare the absolute numbers
  // against each other.
  {
    // Jump to |ifTrue| resp. |ifFalse| if the BigInt is strictly less than
    // resp. strictly greater than the int32 value, depending on the comparison
    // operator.
    Label* greaterThan;
    Label* lessThan;
    if (op == JSOp::Eq) {
      greaterThan = &ifFalse;
      lessThan = &ifFalse;
    } else if (op == JSOp::Ne) {
      greaterThan = &ifTrue;
      lessThan = &ifTrue;
    } else if (op == JSOp::Lt || op == JSOp::Le) {
      greaterThan = &ifFalse;
      lessThan = &ifTrue;
    } else {
      MOZ_ASSERT(op == JSOp::Gt || op == JSOp::Ge);
      greaterThan = &ifTrue;
      lessThan = &ifFalse;
    }

    // BigInt digits are always stored as an absolute number.
    masm.loadFirstBigIntDigitOrZero(bigInt, scratch1);

    // Load the int32 into |scratch2| and negate it for negative numbers.
    masm.move32(int32, scratch2);

    Label isNegative, doCompare;
    masm.branchIfNegativeBigInt(bigInt, &isNegative);
    masm.branch32(Assembler::LessThan, int32, Imm32(0), greaterThan);
    masm.jump(&doCompare);

    // We rely on |neg32(INT32_MIN)| staying INT32_MIN, because we're using an
    // unsigned comparison below.
    masm.bind(&isNegative);
    masm.branch32(Assembler::GreaterThanOrEqual, int32, Imm32(0), lessThan);
    masm.neg32(scratch2);

    // Not all supported platforms (e.g. MIPS64) zero-extend 32-bit operations,
    // so we need to explicitly clear any high 32-bits.
    masm.move32ZeroExtendToPtr(scratch2, scratch2);

    // Reverse the relational comparator for negative numbers.
    // |-x < -y| <=> |+x > +y|.
    // |-x ≤ -y| <=> |+x ≥ +y|.
    // |-x > -y| <=> |+x < +y|.
    // |-x ≥ -y| <=> |+x ≤ +y|.
    JSOp reversed = ReverseCompareOp(op);
    if (reversed != op) {
      masm.branchPtr(JSOpToCondition(reversed, /* signed = */ false), scratch1,
                     scratch2, &ifTrue);
      masm.jump(&ifFalse);
    }

    masm.bind(&doCompare);
    masm.branchPtr(JSOpToCondition(op, /* signed = */ false), scratch1,
                   scratch2, &ifTrue);
  }

  Label done;
  masm.bind(&ifFalse);
  EmitStoreBoolean(masm, false, output);
  masm.jump(&done);

  masm.bind(&ifTrue);
  EmitStoreBoolean(masm, true, output);

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitCompareBigIntInt32Result() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register left = allocator.useRegister(masm, reader.bigIntOperandId());
  Register right = allocator.useRegister(masm, reader.int32OperandId());
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);

  return emitCompareBigIntInt32ResultShared(left, right, scratch1, scratch2, op,
                                            output);
}

bool CacheIRCompiler::emitCompareInt32BigIntResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  Register left = allocator.useRegister(masm, reader.int32OperandId());
  Register right = allocator.useRegister(masm, reader.bigIntOperandId());
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);

  return emitCompareBigIntInt32ResultShared(right, left, scratch1, scratch2,
                                            ReverseCompareOp(op), output);
}

bool CacheIRCompiler::emitCompareBigIntNumberResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  // Float register must be preserved. The Compare ICs use the fact that
  // baseline has them available, as well as fixed temps on LBinaryBoolCache.
  Register lhs = allocator.useRegister(masm, reader.bigIntOperandId());
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  if (op == JSOp::Le || op == JSOp::Gt) {
    masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
    masm.passABIArg(lhs);
  } else {
    masm.passABIArg(lhs);
    masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
  }

  using FnBigIntNumber = bool (*)(BigInt*, double);
  using FnNumberBigInt = bool (*)(double, BigInt*);
  void* fun;
  switch (op) {
    case JSOp::Eq: {
      FnBigIntNumber fn = jit::BigIntNumberEqual<EqualityKind::Equal>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Ne: {
      FnBigIntNumber fn = jit::BigIntNumberEqual<EqualityKind::NotEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Lt: {
      FnBigIntNumber fn = jit::BigIntNumberCompare<ComparisonKind::LessThan>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Gt: {
      FnNumberBigInt fn = jit::NumberBigIntCompare<ComparisonKind::LessThan>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Le: {
      FnNumberBigInt fn =
          jit::NumberBigIntCompare<ComparisonKind::GreaterThanOrEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Ge: {
      FnBigIntNumber fn =
          jit::BigIntNumberCompare<ComparisonKind::GreaterThanOrEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    default:
      MOZ_CRASH("unhandled op");
  }

  masm.callWithABI(fun);
  masm.storeCallBoolResult(scratch);

  LiveRegisterSet ignore;
  ignore.add(scratch);
  masm.PopRegsInMaskIgnore(save, ignore);

  EmitStoreResult(masm, scratch, JSVAL_TYPE_BOOLEAN, output);
  return true;
}

bool CacheIRCompiler::emitCompareNumberBigIntResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  // Float register must be preserved. The Compare ICs use the fact that
  // baseline has them available, as well as fixed temps on LBinaryBoolCache.
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  Register rhs = allocator.useRegister(masm, reader.bigIntOperandId());
  JSOp op = reader.jsop();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  // Also push the operands in reverse order for JSOp::Eq and JSOp::Ne.
  if (op == JSOp::Lt || op == JSOp::Ge) {
    masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
    masm.passABIArg(rhs);
  } else {
    masm.passABIArg(rhs);
    masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
  }

  using FnBigIntNumber = bool (*)(BigInt*, double);
  using FnNumberBigInt = bool (*)(double, BigInt*);
  void* fun;
  switch (op) {
    case JSOp::Eq: {
      FnBigIntNumber fn = jit::BigIntNumberEqual<EqualityKind::Equal>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Ne: {
      FnBigIntNumber fn = jit::BigIntNumberEqual<EqualityKind::NotEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Lt: {
      FnNumberBigInt fn = jit::NumberBigIntCompare<ComparisonKind::LessThan>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Gt: {
      FnBigIntNumber fn = jit::BigIntNumberCompare<ComparisonKind::LessThan>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Le: {
      FnBigIntNumber fn =
          jit::BigIntNumberCompare<ComparisonKind::GreaterThanOrEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    case JSOp::Ge: {
      FnNumberBigInt fn =
          jit::NumberBigIntCompare<ComparisonKind::GreaterThanOrEqual>;
      fun = JS_FUNC_TO_DATA_PTR(void*, fn);
      break;
    }
    default:
      MOZ_CRASH("unhandled op");
  }

  masm.callWithABI(fun);
  masm.storeCallBoolResult(scratch);

  LiveRegisterSet ignore;
  ignore.add(scratch);
  masm.PopRegsInMaskIgnore(save, ignore);

  EmitStoreResult(masm, scratch, JSVAL_TYPE_BOOLEAN, output);
  return true;
}

bool CacheIRCompiler::emitCompareBigIntStringResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register lhs = allocator.useRegister(masm, reader.bigIntOperandId());
  Register rhs = allocator.useRegister(masm, reader.stringOperandId());
  JSOp op = reader.jsop();

  callvm.prepare();

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  if (op == JSOp::Le || op == JSOp::Gt) {
    masm.Push(lhs);
    masm.Push(rhs);
  } else {
    masm.Push(rhs);
    masm.Push(lhs);
  }

  using FnBigIntString =
      bool (*)(JSContext*, HandleBigInt, HandleString, bool*);
  using FnStringBigInt =
      bool (*)(JSContext*, HandleString, HandleBigInt, bool*);

  switch (op) {
    case JSOp::Eq: {
      constexpr auto Equal = EqualityKind::Equal;
      callvm.call<FnBigIntString, BigIntStringEqual<Equal>>();
      break;
    }
    case JSOp::Ne: {
      constexpr auto NotEqual = EqualityKind::NotEqual;
      callvm.call<FnBigIntString, BigIntStringEqual<NotEqual>>();
      break;
    }
    case JSOp::Lt: {
      constexpr auto LessThan = ComparisonKind::LessThan;
      callvm.call<FnBigIntString, BigIntStringCompare<LessThan>>();
      break;
    }
    case JSOp::Gt: {
      constexpr auto LessThan = ComparisonKind::LessThan;
      callvm.call<FnStringBigInt, StringBigIntCompare<LessThan>>();
      break;
    }
    case JSOp::Le: {
      constexpr auto GreaterThanOrEqual = ComparisonKind::GreaterThanOrEqual;
      callvm.call<FnStringBigInt, StringBigIntCompare<GreaterThanOrEqual>>();
      break;
    }
    case JSOp::Ge: {
      constexpr auto GreaterThanOrEqual = ComparisonKind::GreaterThanOrEqual;
      callvm.call<FnBigIntString, BigIntStringCompare<GreaterThanOrEqual>>();
      break;
    }
    default:
      MOZ_CRASH("unhandled op");
  }
  return true;
}

bool CacheIRCompiler::emitCompareStringBigIntResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register lhs = allocator.useRegister(masm, reader.stringOperandId());
  Register rhs = allocator.useRegister(masm, reader.bigIntOperandId());
  JSOp op = reader.jsop();

  callvm.prepare();

  // Push the operands in reverse order for JSOp::Le and JSOp::Gt:
  // - |left <= right| is implemented as |right >= left|.
  // - |left > right| is implemented as |right < left|.
  // Also push the operands in reverse order for JSOp::Eq and JSOp::Ne.
  if (op == JSOp::Lt || op == JSOp::Ge) {
    masm.Push(rhs);
    masm.Push(lhs);
  } else {
    masm.Push(lhs);
    masm.Push(rhs);
  }

  using FnBigIntString =
      bool (*)(JSContext*, HandleBigInt, HandleString, bool*);
  using FnStringBigInt =
      bool (*)(JSContext*, HandleString, HandleBigInt, bool*);

  switch (op) {
    case JSOp::Eq: {
      constexpr auto Equal = EqualityKind::Equal;
      callvm.call<FnBigIntString, BigIntStringEqual<Equal>>();
      break;
    }
    case JSOp::Ne: {
      constexpr auto NotEqual = EqualityKind::NotEqual;
      callvm.call<FnBigIntString, BigIntStringEqual<NotEqual>>();
      break;
    }
    case JSOp::Lt: {
      constexpr auto LessThan = ComparisonKind::LessThan;
      callvm.call<FnStringBigInt, StringBigIntCompare<LessThan>>();
      break;
    }
    case JSOp::Gt: {
      constexpr auto LessThan = ComparisonKind::LessThan;
      callvm.call<FnBigIntString, BigIntStringCompare<LessThan>>();
      break;
    }
    case JSOp::Le: {
      constexpr auto GreaterThanOrEqual = ComparisonKind::GreaterThanOrEqual;
      callvm.call<FnBigIntString, BigIntStringCompare<GreaterThanOrEqual>>();
      break;
    }
    case JSOp::Ge: {
      constexpr auto GreaterThanOrEqual = ComparisonKind::GreaterThanOrEqual;
      callvm.call<FnStringBigInt, StringBigIntCompare<GreaterThanOrEqual>>();
      break;
    }
    default:
      MOZ_CRASH("unhandled op");
  }
  return true;
}

bool CacheIRCompiler::emitCompareObjectUndefinedNullResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  JSOp op = reader.jsop();

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  if (op == JSOp::StrictEq || op == JSOp::StrictNe) {
    // obj !== undefined/null for all objects.
    EmitStoreBoolean(masm, op == JSOp::StrictNe, output);
  } else {
    MOZ_ASSERT(op == JSOp::Eq || op == JSOp::Ne);
    AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
    Label done, emulatesUndefined;
    masm.branchIfObjectEmulatesUndefined(obj, scratch, failure->label(),
                                         &emulatesUndefined);
    EmitStoreBoolean(masm, op == JSOp::Ne, output);
    masm.jump(&done);
    masm.bind(&emulatesUndefined);
    EmitStoreBoolean(masm, op == JSOp::Eq, output);
    masm.bind(&done);
  }
  return true;
}

bool CacheIRCompiler::emitCallPrintString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  const char* str = reinterpret_cast<char*>(reader.pointer());
  masm.printf(str);
  return true;
}

bool CacheIRCompiler::emitBreakpoint() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  masm.breakpoint();
  return true;
}

void CacheIRCompiler::emitStoreTypedObjectReferenceProp(ValueOperand val,
                                                        ReferenceType type,
                                                        const Address& dest,
                                                        Register scratch) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  // Callers will post-barrier this store.

  switch (type) {
    case ReferenceType::TYPE_ANY:
      EmitPreBarrier(masm, dest, MIRType::Value);
      masm.storeValue(val, dest);
      break;

    case ReferenceType::TYPE_WASM_ANYREF:
      // TODO/AnyRef-boxing: With boxed immediates and strings this may be
      // more complicated.
    case ReferenceType::TYPE_OBJECT: {
      EmitPreBarrier(masm, dest, MIRType::Object);
      Label isNull, done;
      masm.branchTestObject(Assembler::NotEqual, val, &isNull);
      masm.unboxObject(val, scratch);
      masm.storePtr(scratch, dest);
      masm.jump(&done);
      masm.bind(&isNull);
      masm.storePtr(ImmWord(0), dest);
      masm.bind(&done);
      break;
    }

    case ReferenceType::TYPE_STRING:
      EmitPreBarrier(masm, dest, MIRType::String);
      masm.unboxString(val, scratch);
      masm.storePtr(scratch, dest);
      break;
  }
}

void CacheIRCompiler::emitRegisterEnumerator(Register enumeratorsList,
                                             Register iter, Register scratch) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  // iter->next = list
  masm.storePtr(enumeratorsList, Address(iter, NativeIterator::offsetOfNext()));

  // iter->prev = list->prev
  masm.loadPtr(Address(enumeratorsList, NativeIterator::offsetOfPrev()),
               scratch);
  masm.storePtr(scratch, Address(iter, NativeIterator::offsetOfPrev()));

  // list->prev->next = iter
  masm.storePtr(iter, Address(scratch, NativeIterator::offsetOfNext()));

  // list->prev = ni
  masm.storePtr(iter, Address(enumeratorsList, NativeIterator::offsetOfPrev()));
}

void CacheIRCompiler::emitPostBarrierShared(Register obj,
                                            const ConstantOrRegister& val,
                                            Register scratch,
                                            Register maybeIndex) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);

  if (val.constant()) {
    MOZ_ASSERT_IF(val.value().isGCThing(),
                  !IsInsideNursery(val.value().toGCThing()));
    return;
  }

  TypedOrValueRegister reg = val.reg();
  if (reg.hasTyped()) {
    if (reg.type() != MIRType::Object && reg.type() != MIRType::String &&
        reg.type() != MIRType::BigInt) {
      return;
    }
  }

  Label skipBarrier;
  if (reg.hasValue()) {
    masm.branchValueIsNurseryCell(Assembler::NotEqual, reg.valueReg(), scratch,
                                  &skipBarrier);
  } else {
    masm.branchPtrInNurseryChunk(Assembler::NotEqual, reg.typedReg().gpr(),
                                 scratch, &skipBarrier);
  }
  masm.branchPtrInNurseryChunk(Assembler::Equal, obj, scratch, &skipBarrier);

  // Call one of these, depending on maybeIndex:
  //
  //   void PostWriteBarrier(JSRuntime* rt, JSObject* obj);
  //   void PostWriteElementBarrier(JSRuntime* rt, JSObject* obj,
  //                                int32_t index);
  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);
  masm.setupUnalignedABICall(scratch);
  masm.movePtr(ImmPtr(cx_->runtime()), scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(obj);
  if (maybeIndex != InvalidReg) {
    masm.passABIArg(maybeIndex);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(
        void*, (PostWriteElementBarrier<IndexInBounds::Yes>)));
  } else {
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, PostWriteBarrier));
  }
  masm.PopRegsInMask(save);

  masm.bind(&skipBarrier);
}

bool CacheIRCompiler::emitWrapResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegister scratch(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label done;
  // We only have to wrap objects, because we are in the same zone.
  masm.branchTestObject(Assembler::NotEqual, output.valueReg(), &done);

  Register obj = output.valueReg().scratchReg();
  masm.unboxObject(output.valueReg(), obj);

  LiveRegisterSet save(GeneralRegisterSet::Volatile(), liveVolatileFloatRegs());
  masm.PushRegsInMask(save);

  masm.setupUnalignedABICall(scratch);
  masm.loadJSContext(scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(obj);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, WrapObjectPure));
  masm.mov(ReturnReg, obj);

  LiveRegisterSet ignore;
  ignore.add(obj);
  masm.PopRegsInMaskIgnore(save, ignore);

  // We could not get a wrapper for this object.
  masm.branchTestPtr(Assembler::Zero, obj, obj, failure->label());

  // We clobbered the output register, so we have to retag.
  masm.tagValue(JSVAL_TYPE_OBJECT, obj, output.valueReg());

  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitMegamorphicLoadSlotByValueResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());
  bool handleMissing = reader.readBool();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // The object must be Native.
  masm.branchIfNonNativeObj(obj, scratch, failure->label());

  // idVal will be in vp[0], result will be stored in vp[1].
  masm.reserveStack(sizeof(Value));
  masm.Push(idVal);
  masm.moveStackPtrTo(idVal.scratchReg());

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch);
  volatileRegs.takeUnchecked(idVal);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch);
  masm.loadJSContext(scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(obj);
  masm.passABIArg(idVal.scratchReg());
  if (handleMissing) {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataPropertyByValuePure<true>)));
  } else {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataPropertyByValuePure<false>)));
  }
  masm.mov(ReturnReg, scratch);
  masm.PopRegsInMask(volatileRegs);

  masm.Pop(idVal);

  Label ok;
  uint32_t framePushed = masm.framePushed();
  masm.branchIfTrueBool(scratch, &ok);
  masm.adjustStack(sizeof(Value));
  masm.jump(failure->label());

  masm.bind(&ok);
  if (JitOptions.spectreJitToCxxCalls) {
    masm.speculationBarrier();
  }
  masm.setFramePushed(framePushed);
  masm.loadTypedOrValue(Address(masm.getStackPointer(), 0), output);
  masm.adjustStack(sizeof(Value));
  return true;
}

bool CacheIRCompiler::emitMegamorphicHasPropResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());
  bool hasOwn = reader.readBool();

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // idVal will be in vp[0], result will be stored in vp[1].
  masm.reserveStack(sizeof(Value));
  masm.Push(idVal);
  masm.moveStackPtrTo(idVal.scratchReg());

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch);
  volatileRegs.takeUnchecked(idVal);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch);
  masm.loadJSContext(scratch);
  masm.passABIArg(scratch);
  masm.passABIArg(obj);
  masm.passABIArg(idVal.scratchReg());
  if (hasOwn) {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, HasNativeDataPropertyPure<true>));
  } else {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, HasNativeDataPropertyPure<false>));
  }
  masm.mov(ReturnReg, scratch);
  masm.PopRegsInMask(volatileRegs);

  masm.Pop(idVal);

  Label ok;
  uint32_t framePushed = masm.framePushed();
  masm.branchIfTrueBool(scratch, &ok);
  masm.adjustStack(sizeof(Value));
  masm.jump(failure->label());

  masm.bind(&ok);
  masm.setFramePushed(framePushed);
  masm.loadTypedOrValue(Address(masm.getStackPointer(), 0), output);
  masm.adjustStack(sizeof(Value));
  return true;
}

bool CacheIRCompiler::emitCallObjectHasSparseElementResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());

  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.reserveStack(sizeof(Value));
  masm.moveStackPtrTo(scratch2.get());

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch1);
  volatileRegs.takeUnchecked(index);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch1);
  masm.loadJSContext(scratch1);
  masm.passABIArg(scratch1);
  masm.passABIArg(obj);
  masm.passABIArg(index);
  masm.passABIArg(scratch2);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, HasNativeElementPure));
  masm.mov(ReturnReg, scratch1);
  masm.PopRegsInMask(volatileRegs);

  Label ok;
  uint32_t framePushed = masm.framePushed();
  masm.branchIfTrueBool(scratch1, &ok);
  masm.adjustStack(sizeof(Value));
  masm.jump(failure->label());

  masm.bind(&ok);
  masm.setFramePushed(framePushed);
  masm.loadTypedOrValue(Address(masm.getStackPointer(), 0), output);
  masm.adjustStack(sizeof(Value));
  return true;
}

/*
 * Move a constant value into register dest.
 */
void CacheIRCompiler::emitLoadStubFieldConstant(StubFieldOffset val,
                                                Register dest) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  MOZ_ASSERT(mode_ == Mode::Ion);
  switch (val.getStubFieldType()) {
    case StubField::Type::Shape:
      masm.movePtr(ImmGCPtr(shapeStubField(val.getOffset())), dest);
      break;
    case StubField::Type::String:
      masm.movePtr(ImmGCPtr(stringStubField(val.getOffset())), dest);
      break;
    case StubField::Type::ObjectGroup:
      masm.movePtr(ImmGCPtr(groupStubField(val.getOffset())), dest);
      break;
    case StubField::Type::JSObject:
      masm.movePtr(ImmGCPtr(objectStubField(val.getOffset())), dest);
      break;
    case StubField::Type::RawWord:
      masm.move32(Imm32(int32StubField(val.getOffset())), dest);
      break;
    default:
      MOZ_CRASH("Unhandled stub field constant type");
  }
}

/*
 * After this is done executing, dest contains the value; either through a
 * constant load or through the load from the stub data.
 *
 * The current policy is that Baseline will use loads from the stub data (to
 * allow IC sharing), where as Ion doesn't share ICs, and so we can safely use
 * constants in the IC.
 */
void CacheIRCompiler::emitLoadStubField(StubFieldOffset val, Register dest) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  if (stubFieldPolicy_ == StubFieldPolicy::Constant) {
    emitLoadStubFieldConstant(val, dest);
  } else {
    Address load(ICStubReg, stubDataOffset_ + val.getOffset());
    masm.loadPtr(load, dest);
  }
}

Address CacheIRCompiler::emitAddressFromStubField(StubFieldOffset val,
                                                  Register base) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  MOZ_ASSERT(val.getStubFieldType() == StubField::Type::RawWord);

  if (stubFieldPolicy_ == StubFieldPolicy::Constant) {
    int32_t offset = int32StubField(val.getOffset());
    return Address(base, offset);
  }

  Address offsetAddr(ICStubReg, stubDataOffset_ + val.getOffset());
  masm.addPtr(offsetAddr, base);
  return Address(base, 0);
}

bool CacheIRCompiler::emitLoadInstanceOfObjectResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  ValueOperand lhs = allocator.useValueRegister(masm, reader.valOperandId());
  Register proto = allocator.useRegister(masm, reader.objOperandId());

  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  Label returnFalse, returnTrue, done;
  masm.branchTestObject(Assembler::NotEqual, lhs, &returnFalse);

  // LHS is an object. Load its proto.
  masm.unboxObject(lhs, scratch);
  masm.loadObjProto(scratch, scratch);
  {
    // Walk the proto chain until we either reach the target object,
    // nullptr or LazyProto.
    Label loop;
    masm.bind(&loop);

    masm.branchPtr(Assembler::Equal, scratch, proto, &returnTrue);
    masm.branchTestPtr(Assembler::Zero, scratch, scratch, &returnFalse);

    MOZ_ASSERT(uintptr_t(TaggedProto::LazyProto) == 1);
    masm.branchPtr(Assembler::Equal, scratch, ImmWord(1), failure->label());

    masm.loadObjProto(scratch, scratch);
    masm.jump(&loop);
  }

  masm.bind(&returnFalse);
  EmitStoreBoolean(masm, false, output);
  masm.jump(&done);

  masm.bind(&returnTrue);
  EmitStoreBoolean(masm, true, output);
  // fallthrough
  masm.bind(&done);
  return true;
}

bool CacheIRCompiler::emitMegamorphicLoadSlotResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  StubFieldOffset name(reader.stubOffset(), StubField::Type::String);
  bool handleMissing = reader.readBool();

  AutoScratchRegisterMaybeOutput scratch1(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);
  AutoScratchRegister scratch3(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  // The object must be Native.
  masm.branchIfNonNativeObj(obj, scratch3, failure->label());

  masm.Push(UndefinedValue());
  masm.moveStackPtrTo(scratch3.get());

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch1);
  volatileRegs.takeUnchecked(scratch2);
  volatileRegs.takeUnchecked(scratch3);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch1);
  masm.loadJSContext(scratch1);
  masm.passABIArg(scratch1);
  masm.passABIArg(obj);
  emitLoadStubField(name, scratch2);
  masm.passABIArg(scratch2);
  masm.passABIArg(scratch3);
  if (handleMissing) {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataPropertyPure<true>)));
  } else {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (GetNativeDataPropertyPure<false>)));
  }
  masm.mov(ReturnReg, scratch2);
  masm.PopRegsInMask(volatileRegs);

  masm.loadTypedOrValue(Address(masm.getStackPointer(), 0), output);
  masm.adjustStack(sizeof(Value));

  masm.branchIfFalseBool(scratch2, failure->label());
  if (JitOptions.spectreJitToCxxCalls) {
    masm.speculationBarrier();
  }

  return true;
}

bool CacheIRCompiler::emitMegamorphicStoreSlot() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register obj = allocator.useRegister(masm, reader.objOperandId());
  StubFieldOffset name(reader.stubOffset(), StubField::Type::String);
  ValueOperand val = allocator.useValueRegister(masm, reader.valOperandId());
  bool needsTypeBarrier = reader.readBool();

  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  masm.Push(val);
  masm.moveStackPtrTo(val.scratchReg());

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(scratch1);
  volatileRegs.takeUnchecked(scratch2);
  volatileRegs.takeUnchecked(val);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(scratch1);
  masm.loadJSContext(scratch1);
  masm.passABIArg(scratch1);
  masm.passABIArg(obj);
  emitLoadStubField(name, scratch2);
  masm.passABIArg(scratch2);
  masm.passABIArg(val.scratchReg());
  if (needsTypeBarrier) {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (SetNativeDataPropertyPure<true>)));
  } else {
    masm.callWithABI(
        JS_FUNC_TO_DATA_PTR(void*, (SetNativeDataPropertyPure<false>)));
  }
  masm.mov(ReturnReg, scratch1);
  masm.PopRegsInMask(volatileRegs);

  masm.loadValue(Address(masm.getStackPointer(), 0), val);
  masm.adjustStack(sizeof(Value));

  masm.branchIfFalseBool(scratch1, failure->label());
  return true;
}

bool CacheIRCompiler::emitGuardGroupHasUnanalyzedNewScript(
    uint32_t groupOffset) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  StubFieldOffset group(groupOffset, StubField::Type::ObjectGroup);
  AutoScratchRegister scratch1(allocator, masm);
  AutoScratchRegister scratch2(allocator, masm);

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  emitLoadStubField(group, scratch1);
  masm.guardGroupHasUnanalyzedNewScript(scratch1, scratch2, failure->label());
  return true;
}

bool CacheIRCompiler::emitLoadObject() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register reg = allocator.defineRegister(masm, reader.objOperandId());
  StubFieldOffset obj(reader.stubOffset(), StubField::Type::JSObject);
  emitLoadStubField(obj, reg);
  return true;
}

bool CacheIRCompiler::emitCallInt32ToString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register input = allocator.useRegister(masm, reader.int32OperandId());
  Register result = allocator.defineRegister(masm, reader.stringOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(result);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(result);
  masm.loadJSContext(result);
  masm.passABIArg(result);
  masm.passABIArg(input);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (js::Int32ToStringHelperPure)));

  masm.mov(ReturnReg, result);
  masm.PopRegsInMask(volatileRegs);

  masm.branchPtr(Assembler::Equal, result, ImmPtr(0), failure->label());
  return true;
}

bool CacheIRCompiler::emitCallNumberToString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  // Float register must be preserved. The BinaryArith ICs use
  // the fact that baseline has them available, as well as fixed temps on
  // LBinaryCache.
  allocator.ensureDoubleRegister(masm, reader.numberOperandId(), FloatReg0);
  Register result = allocator.defineRegister(masm, reader.stringOperandId());

  FailurePath* failure;
  if (!addFailurePath(&failure)) {
    return false;
  }

  LiveRegisterSet volatileRegs(GeneralRegisterSet::Volatile(),
                               liveVolatileFloatRegs());
  volatileRegs.takeUnchecked(result);
  volatileRegs.addUnchecked(FloatReg0);
  masm.PushRegsInMask(volatileRegs);

  masm.setupUnalignedABICall(result);
  masm.loadJSContext(result);
  masm.passABIArg(result);
  masm.passABIArg(FloatReg0, MoveOp::DOUBLE);
  masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, (js::NumberToStringHelperPure)));

  masm.mov(ReturnReg, result);
  masm.PopRegsInMask(volatileRegs);

  masm.branchPtr(Assembler::Equal, result, ImmPtr(0), failure->label());
  return true;
}

bool CacheIRCompiler::emitBooleanToString() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  Register boolean = allocator.useRegister(masm, reader.int32OperandId());
  Register result = allocator.defineRegister(masm, reader.stringOperandId());
  const JSAtomState& names = cx_->names();
  Label true_, done;

  masm.branchTest32(Assembler::NonZero, boolean, boolean, &true_);

  // False case
  masm.movePtr(ImmGCPtr(names.false_), result);
  masm.jump(&done);

  // True case
  masm.bind(&true_);
  masm.movePtr(ImmGCPtr(names.true_), result);
  masm.bind(&done);

  return true;
}

void js::jit::LoadTypedThingData(MacroAssembler& masm, TypedThingLayout layout,
                                 Register obj, Register result) {
  switch (layout) {
    case Layout_TypedArray:
      masm.loadPtr(Address(obj, TypedArrayObject::dataOffset()), result);
      break;
    case Layout_OutlineTypedObject:
      masm.loadPtr(Address(obj, OutlineTypedObject::offsetOfData()), result);
      break;
    case Layout_InlineTypedObject:
      masm.computeEffectiveAddress(
          Address(obj, InlineTypedObject::offsetOfDataStart()), result);
      break;
    default:
      MOZ_CRASH();
  }
}

void js::jit::LoadTypedThingLength(MacroAssembler& masm,
                                   TypedThingLayout layout, Register obj,
                                   Register result) {
  switch (layout) {
    case Layout_TypedArray:
      masm.unboxInt32(Address(obj, TypedArrayObject::lengthOffset()), result);
      break;
    case Layout_OutlineTypedObject:
    case Layout_InlineTypedObject:
      masm.loadTypedObjectLength(obj, result);
      break;
    default:
      MOZ_CRASH();
  }
}

bool CacheIRCompiler::emitCallStringConcatResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register lhs = allocator.useRegister(masm, reader.stringOperandId());
  Register rhs = allocator.useRegister(masm, reader.stringOperandId());

  callvm.prepare();

  masm.Push(rhs);
  masm.Push(lhs);

  using Fn = JSString* (*)(JSContext*, HandleString, HandleString);
  callvm.call<Fn, ConcatStrings<CanGC>>();

  return true;
}

bool CacheIRCompiler::emitCallIsSuspendedGeneratorResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoOutputRegister output(*this);
  AutoScratchRegisterMaybeOutput scratch(allocator, masm, output);
  AutoScratchRegister scratch2(allocator, masm);
  ValueOperand input = allocator.useValueRegister(masm, reader.valOperandId());

  // Test if it's an object.
  Label returnFalse, done;
  masm.branchTestObject(Assembler::NotEqual, input, &returnFalse);

  // Test if it's a GeneratorObject.
  masm.unboxObject(input, scratch);
  masm.branchTestObjClass(Assembler::NotEqual, scratch,
                          &GeneratorObject::class_, scratch2, scratch,
                          &returnFalse);

  // If the resumeIndex slot holds an int32 value < RESUME_INDEX_RUNNING,
  // the generator is suspended.
  Address addr(scratch, AbstractGeneratorObject::offsetOfResumeIndexSlot());
  masm.branchTestInt32(Assembler::NotEqual, addr, &returnFalse);
  masm.unboxInt32(addr, scratch);
  masm.branch32(Assembler::AboveOrEqual, scratch,
                Imm32(AbstractGeneratorObject::RESUME_INDEX_RUNNING),
                &returnFalse);

  masm.moveValue(BooleanValue(true), output.valueReg());
  masm.jump(&done);

  masm.bind(&returnFalse);
  masm.moveValue(BooleanValue(false), output.valueReg());

  masm.bind(&done);
  return true;
}

// This op generates no code. It is consumed by BaselineInspector.
bool CacheIRCompiler::emitMetaTwoByte() {
  mozilla::Unused << reader.readByte();  // meta kind
  mozilla::Unused << reader.readByte();  // payload byte 1
  mozilla::Unused << reader.readByte();  // payload byte 2

  return true;
}

bool CacheIRCompiler::emitCallNativeGetElementResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register index = allocator.useRegister(masm, reader.int32OperandId());

  callvm.prepare();

  masm.Push(index);
  masm.Push(TypedOrValueRegister(MIRType::Object, AnyRegister(obj)));
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleNativeObject, HandleValue, int32_t,
                      MutableHandleValue);
  callvm.call<Fn, NativeGetElement>();

  return true;
}

bool CacheIRCompiler::emitCallProxyHasPropResult(ObjOperandId objId,
                                                 ValOperandId idId,
                                                 bool hasOwn) {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register obj = allocator.useRegister(masm, objId);
  ValueOperand idVal = allocator.useValueRegister(masm, idId);

  callvm.prepare();

  masm.Push(idVal);
  masm.Push(obj);

  using Fn = bool (*)(JSContext*, HandleObject, HandleValue, bool*);
  if (hasOwn) {
    callvm.call<Fn, ProxyHasOwn>();
  } else {
    callvm.call<Fn, ProxyHas>();
  }
  return true;
}

bool CacheIRCompiler::emitCallProxyGetByValueResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);
  AutoCallVM callvm(masm, this, allocator);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  ValueOperand idVal = allocator.useValueRegister(masm, reader.valOperandId());

  callvm.prepare();
  masm.Push(idVal);
  masm.Push(obj);

  using Fn =
      bool (*)(JSContext*, HandleObject, HandleValue, MutableHandleValue);
  callvm.call<Fn, ProxyGetPropertyByValue>();
  return true;
}

bool CacheIRCompiler::emitCallGetSparseElementResult() {
  JitSpew(JitSpew_Codegen, "%s", __FUNCTION__);

  AutoCallVM callvm(masm, this, allocator);

  Register obj = allocator.useRegister(masm, reader.objOperandId());
  Register id = allocator.useRegister(masm, reader.int32OperandId());

  callvm.prepare();
  masm.Push(id);
  masm.Push(obj);

  using Fn = bool (*)(JSContext * cx, HandleArrayObject obj, int32_t int_id,
                      MutableHandleValue result);
  callvm.call<Fn, GetSparseElementHelper>();
  return true;
}

template <typename Fn, Fn fn>
void CacheIRCompiler::callVM(MacroAssembler& masm) {
  VMFunctionId id = VMFunctionToId<Fn, fn>::id;
  callVMInternal(masm, id);
}

void CacheIRCompiler::callVMInternal(MacroAssembler& masm, VMFunctionId id) {
  if (mode_ == Mode::Ion) {
    MOZ_ASSERT(preparedForVMCall_);
    TrampolinePtr code = cx_->runtime()->jitRuntime()->getVMWrapper(id);
    const VMFunctionData& fun = GetVMFunction(id);
    uint32_t frameSize = fun.explicitStackSlots() * sizeof(void*);
    uint32_t descriptor = MakeFrameDescriptor(frameSize, FrameType::IonICCall,
                                              ExitFrameLayout::Size());
    masm.Push(Imm32(descriptor));
    masm.callJit(code);

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly popped when returning.
    int framePop = sizeof(ExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(frameSize + framePop);
    masm.freeStack(IonICCallFrameLayout::Size());
    return;
  }

  MOZ_ASSERT(mode_ == Mode::Baseline);

  MOZ_ASSERT(preparedForVMCall_);

  TrampolinePtr code = cx_->runtime()->jitRuntime()->getVMWrapper(id);
  MOZ_ASSERT(GetVMFunction(id).expectTailCall == NonTailCall);

  EmitBaselineCallVM(code, masm);
}

bool CacheIRCompiler::isBaseline() { return mode_ == Mode::Baseline; }

bool CacheIRCompiler::isIon() { return mode_ == Mode::Ion; }

BaselineCacheIRCompiler* CacheIRCompiler::asBaseline() {
  MOZ_ASSERT(this->isBaseline());
  return static_cast<BaselineCacheIRCompiler*>(this);
}

IonCacheIRCompiler* CacheIRCompiler::asIon() {
  MOZ_ASSERT(this->isIon());
  return static_cast<IonCacheIRCompiler*>(this);
}

AutoCallVM::AutoCallVM(MacroAssembler& masm, CacheIRCompiler* compiler,
                       CacheRegisterAllocator& allocator)
    : masm_(masm), compiler_(compiler), allocator_(allocator) {
  // Ion needs to `prepareVMCall` before it can callVM and it also needs to
  // initialize AutoSaveLiveRegisters.
  if (compiler_->mode_ == CacheIRCompiler::Mode::Ion) {
    // Will need to use a downcast here as well, in order to pass the
    // stub to AutoSaveLiveRegisters
    save_.emplace(*compiler_->asIon());
  }

  output_.emplace(*compiler);

  if (compiler_->mode_ == CacheIRCompiler::Mode::Baseline) {
    stubFrame_.emplace(*compiler_->asBaseline());
    scratch_.emplace(allocator_, masm_, output_.ref());
  }
}

void AutoCallVM::prepare() {
  allocator_.discardStack(masm_);
  MOZ_ASSERT(compiler_ != nullptr);
  if (compiler_->mode_ == CacheIRCompiler::Mode::Ion) {
    compiler_->asIon()->prepareVMCall(masm_, *save_.ptr());
    return;
  }
  MOZ_ASSERT(compiler_->mode_ == CacheIRCompiler::Mode::Baseline);
  stubFrame_->enter(masm_, scratch_.ref());
}

void AutoCallVM::storeResult(JSValueType returnType) {
  MOZ_ASSERT(returnType != JSVAL_TYPE_DOUBLE);

  if (returnType == JSVAL_TYPE_UNKNOWN) {
    masm_.storeCallResultValue(output_.ref());
  } else {
    if (output_->hasValue()) {
      masm_.tagValue(returnType, ReturnReg, output_->valueReg());
    } else {
      Register out = output_->typedReg().gpr();
      if (out != ReturnReg) {
        masm_.mov(ReturnReg, out);
      }
    }
  }

  if (compiler_->mode_ == CacheIRCompiler::Mode::Baseline) {
    stubFrame_->leave(masm_);
  }
}

template <typename...>
struct VMFunctionReturnType;

template <class R, typename... Args>
struct VMFunctionReturnType<R (*)(JSContext*, Args...)> {
  using LastArgument = typename LastArg<Args...>::Type;

  // By convention VMFunctions returning `bool` use an output parameter.
  using ReturnType =
      std::conditional_t<std::is_same_v<R, bool>, LastArgument, R>;
};

template <class>
struct ReturnTypeToJSValueType;

// Definitions for the currently used return types.
template <>
struct ReturnTypeToJSValueType<MutableHandleValue> {
  static constexpr JSValueType result = JSVAL_TYPE_UNKNOWN;
};
template <>
struct ReturnTypeToJSValueType<bool*> {
  static constexpr JSValueType result = JSVAL_TYPE_BOOLEAN;
};
template <>
struct ReturnTypeToJSValueType<JSString*> {
  static constexpr JSValueType result = JSVAL_TYPE_STRING;
};
template <>
struct ReturnTypeToJSValueType<BigInt*> {
  static constexpr JSValueType result = JSVAL_TYPE_BIGINT;
};

template <typename Fn>
void AutoCallVM::storeResult() {
  using ReturnType = typename VMFunctionReturnType<Fn>::ReturnType;
  storeResult(ReturnTypeToJSValueType<ReturnType>::result);
}

AutoScratchFloatRegister::AutoScratchFloatRegister(CacheIRCompiler* compiler,
                                                   FailurePath* failure)
    : compiler_(compiler), failure_(failure) {
  // If we're compiling a Baseline IC, FloatReg0 is always available.
  if (!compiler_->isBaseline()) {
    MacroAssembler& masm = compiler_->masm;
    masm.push(FloatReg0);
  }

  if (failure_) {
    failure_->setHasAutoScratchFloatRegister();
  }
}

AutoScratchFloatRegister::~AutoScratchFloatRegister() {
  if (failure_) {
    failure_->clearHasAutoScratchFloatRegister();
  }

  if (!compiler_->isBaseline()) {
    MacroAssembler& masm = compiler_->masm;
    masm.pop(FloatReg0);

    if (failure_) {
      Label done;
      masm.jump(&done);
      masm.bind(&failurePopReg_);
      masm.pop(FloatReg0);
      masm.jump(failure_->label());
      masm.bind(&done);
    }
  }
}

Label* AutoScratchFloatRegister::failure() {
  MOZ_ASSERT(failure_);

  if (!compiler_->isBaseline()) {
    return &failurePopReg_;
  }
  return failure_->labelUnchecked();
}
