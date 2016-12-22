/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CacheIRCompiler.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Maybe;

ValueOperand
CacheRegisterAllocator::useValueRegister(MacroAssembler& masm, ValOperandId op)
{
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

      case OperandLocation::Constant: {
        ValueOperand reg = allocateValueRegister(masm);
        masm.moveValue(loc.constant(), reg);
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
        Register reg = allocateRegister(masm);
        popPayload(masm, &loc, reg);
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

      case OperandLocation::Constant: {
        Value v = loc.constant();
        Register reg = allocateRegister(masm);
        if (v.isString())
            masm.movePtr(ImmGCPtr(v.toString()), reg);
        else if (v.isSymbol())
            masm.movePtr(ImmGCPtr(v.toSymbol()), reg);
        else
            MOZ_CRASH("Unexpected Value");
        loc.setPayloadReg(reg, v.extractNonDoubleType());
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
          case OperandLocation::Constant:
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

                spillOperand(masm, &loc);
                availableRegs_.add(reg);
                break; // We got a register, so break out of the loop.
            }
            if (loc.kind() == OperandLocation::ValueReg) {
                ValueOperand reg = loc.valueReg();
                if (currentOpRegs_.aliases(reg))
                    continue;

                spillOperand(masm, &loc);
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

            spillOperand(masm, &loc);
            currentOpRegs_.add(reg);
            return;
        }
        if (loc.kind() == OperandLocation::ValueReg) {
            if (!loc.valueReg().aliases(reg))
                continue;

            ValueOperand valueReg = loc.valueReg();
            spillOperand(masm, &loc);

            availableRegs_.add(valueReg);
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
CacheRegisterAllocator::init(const AllocatableGeneralRegisterSet& available)
{
    availableRegs_ = available;
    if (!origInputLocations_.resize(writer_.numInputOperands()))
        return false;
    if (!operandLocations_.resize(writer_.numOperandIds()))
        return false;
    return true;
}

JSValueType
CacheRegisterAllocator::knownType(ValOperandId val) const
{
    const OperandLocation& loc = operandLocations_[val.id()];

    switch (loc.kind()) {
      case OperandLocation::ValueReg:
      case OperandLocation::ValueStack:
        return JSVAL_TYPE_UNKNOWN;

      case OperandLocation::PayloadStack:
      case OperandLocation::PayloadReg:
        return loc.payloadType();

      case OperandLocation::Constant:
        return loc.constant().isDouble()
               ? JSVAL_TYPE_DOUBLE
               : loc.constant().extractNonDoubleType();

      case OperandLocation::Uninitialized:
        break;
    }

    MOZ_CRASH("Invalid kind");
}

void
CacheRegisterAllocator::initInputLocation(size_t i, const TypedOrValueRegister& reg)
{
    if (reg.hasValue()) {
        initInputLocation(i, reg.valueReg());
    } else {
        MOZ_ASSERT(!reg.typedReg().isFloat());
        initInputLocation(i, reg.typedReg().gpr(), ValueTypeFromMIRType(reg.type()));
    }
}

void
CacheRegisterAllocator::initInputLocation(size_t i, const ConstantOrRegister& value)
{
    if (value.constant())
        initInputLocation(i, value.value());
    else
        initInputLocation(i, value.reg());
}

void
CacheRegisterAllocator::spillOperand(MacroAssembler& masm, OperandLocation* loc)
{
    MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());

    if (loc->kind() == OperandLocation::ValueReg) {
        stackPushed_ += sizeof(js::Value);
        masm.pushValue(loc->valueReg());
        loc->setValueStack(stackPushed_);
        return;
    }

    MOZ_ASSERT(loc->kind() == OperandLocation::PayloadReg);

    stackPushed_ += sizeof(uintptr_t);
    masm.push(loc->payloadReg());
    loc->setPayloadStack(stackPushed_, loc->payloadType());
}

void
CacheRegisterAllocator::popPayload(MacroAssembler& masm, OperandLocation* loc, Register dest)
{
    MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());
    MOZ_ASSERT(stackPushed_ >= sizeof(uintptr_t));

    // The payload is on the stack. If it's on top of the stack we can just
    // pop it, else we emit a load.
    if (loc->payloadStack() == stackPushed_) {
        masm.pop(dest);
        stackPushed_ -= sizeof(uintptr_t);
    } else {
        MOZ_ASSERT(loc->payloadStack() < stackPushed_);
        masm.loadPtr(Address(masm.getStackPointer(), stackPushed_ - loc->payloadStack()), dest);
    }

    loc->setPayloadReg(dest, loc->payloadType());
}

void
CacheRegisterAllocator::popValue(MacroAssembler& masm, OperandLocation* loc, ValueOperand dest)
{
    MOZ_ASSERT(loc >= operandLocations_.begin() && loc < operandLocations_.end());
    MOZ_ASSERT(stackPushed_ >= sizeof(js::Value));

    // The Value is on the stack. If it's on top of the stack we can just
    // pop it, else we emit a load.
    if (loc->valueStack() == stackPushed_) {
        masm.popValue(dest);
        stackPushed_ -= sizeof(js::Value);
    } else {
        MOZ_ASSERT(loc->valueStack() < stackPushed_);
        masm.loadValue(Address(masm.getStackPointer(), stackPushed_ - loc->valueStack()), dest);
    }

    loc->setValueReg(dest);
}

bool
OperandLocation::aliasesReg(const OperandLocation& other) const
{
    MOZ_ASSERT(&other != this);

    switch (other.kind_) {
      case PayloadReg:
        return aliasesReg(other.payloadReg());
      case ValueReg:
        return aliasesReg(other.valueReg());
      case PayloadStack:
      case ValueStack:
      case Constant:
        return false;
      case Uninitialized:
        break;
    }

    MOZ_CRASH("Invalid kind");
}

void
CacheRegisterAllocator::restoreInputState(MacroAssembler& masm)
{
    size_t numInputOperands = origInputLocations_.length();
    MOZ_ASSERT(writer_.numInputOperands() == numInputOperands);

    for (size_t j = 0; j < numInputOperands; j++) {
        const OperandLocation& dest = origInputLocations_[j];
        OperandLocation& cur = operandLocations_[j];
        if (dest == cur)
            continue;

        // We have a cycle if a destination register will be used later
        // as source register. If that happens, just push the current value
        // on the stack and later get it from there.
        for (size_t k = j + 1; k < numInputOperands; k++) {
            OperandLocation& laterSource = operandLocations_[k];
            if (dest.aliasesReg(laterSource))
                spillOperand(masm, &laterSource);
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
              case OperandLocation::Constant:
              case OperandLocation::Uninitialized:
                break;
            }
        } else if (dest.kind() == OperandLocation::PayloadReg) {
            // We have to restore a payload register.
            switch (cur.kind()) {
              case OperandLocation::ValueReg:
                MOZ_ASSERT(dest.payloadType() != JSVAL_TYPE_DOUBLE);
                masm.unboxNonDouble(cur.valueReg(), dest.payloadReg());
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
                masm.unboxNonDouble(Address(masm.getStackPointer(), stackPushed_ - cur.valueStack()),
                                    dest.payloadReg());
                continue;
              case OperandLocation::Constant:
              case OperandLocation::Uninitialized:
                break;
            }
        } else if (dest.kind() == OperandLocation::Constant) {
            // Nothing to do.
            continue;
        }

        MOZ_CRASH("Invalid kind");
    }

    discardStack(masm);
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
          case StubField::Type::Symbol:
            getStubField<JS::Symbol*>(dest, offset).init(getStubField<JS::Symbol*>(src, offset));
            break;
          case StubField::Type::String:
            getStubField<JSString*>(dest, offset).init(getStubField<JSString*>(src, offset));
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
template GCPtr<JSString*>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;
template GCPtr<JS::Symbol*>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;
template GCPtr<JS::Value>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;
template GCPtr<jsid>& CacheIRStubInfo::getStubField(ICStub* stub, uint32_t offset) const;

template <typename T, typename V>
static void
InitGCPtr(uintptr_t* ptr, V val)
{
    AsGCPtr<T>(ptr)->init(mozilla::BitwiseCast<T>(val));
}

void
CacheIRWriter::copyStubData(uint8_t* dest) const
{
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
            InitGCPtr<jsid>(destWords, field.asWord());
            break;
          case StubField::Type::RawInt64:
            *reinterpret_cast<uint64_t*>(destWords) = field.asInt64();
            break;
          case StubField::Type::Value:
            InitGCPtr<JS::Value>(destWords, field.asInt64());
            break;
          case StubField::Type::Limit:
            MOZ_CRASH("Invalid type");
        }
        destWords += StubField::sizeInBytes(field.type()) / sizeof(uintptr_t);
    }
}

bool
CacheIRWriter::stubDataEquals(const uint8_t* stubData) const
{
    MOZ_ASSERT(!failed());

    const uintptr_t* stubDataWords = reinterpret_cast<const uintptr_t*>(stubData);

    for (const StubField& field : stubFields_) {
        if (field.sizeIsWord()) {
            if (field.asWord() != *stubDataWords)
                return false;
            stubDataWords++;
            continue;
        }

        if (field.asInt64() != *reinterpret_cast<const uint64_t*>(stubDataWords))
            return false;
        stubDataWords += sizeof(uint64_t) / sizeof(uintptr_t);
    }

    return true;
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

bool
OperandLocation::operator==(const OperandLocation& other) const
{
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
      case Constant:
        return constant() == other.constant();
    }

    MOZ_CRASH("Invalid OperandLocation kind");
}

bool
FailurePath::canShareFailurePath(const FailurePath& other) const
{
    if (stackPushed_ != other.stackPushed_)
        return false;

    MOZ_ASSERT(inputs_.length() == other.inputs_.length());

    for (size_t i = 0; i < inputs_.length(); i++) {
        if (inputs_[i] != other.inputs_[i])
	    return false;
    }
    return true;
}

bool
CacheIRCompiler::addFailurePath(FailurePath** failure)
{
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

void
CacheIRCompiler::emitFailurePath(size_t index)
{
    FailurePath& failure = failurePaths[index];

    allocator.setStackPushed(failure.stackPushed());

    for (size_t i = 0; i < writer_.numInputOperands(); i++)
        allocator.setOperandLocation(i, failure.input(i));

    masm.bind(failure.label());
    allocator.restoreInputState(masm);
}

bool
CacheIRCompiler::emitGuardIsObject()
{
    ValOperandId inputId = reader.valOperandId();
    if (allocator.knownType(inputId) == JSVAL_TYPE_OBJECT)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;
    masm.branchTestObject(Assembler::NotEqual, input, failure->label());
    return true;
}

bool
CacheIRCompiler::emitGuardIsString()
{
    ValOperandId inputId = reader.valOperandId();
    if (allocator.knownType(inputId) == JSVAL_TYPE_STRING)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;
    masm.branchTestString(Assembler::NotEqual, input, failure->label());
    return true;
}

bool
CacheIRCompiler::emitGuardIsSymbol()
{
    ValOperandId inputId = reader.valOperandId();
    if (allocator.knownType(inputId) == JSVAL_TYPE_SYMBOL)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;
    masm.branchTestSymbol(Assembler::NotEqual, input, failure->label());
    return true;
}

bool
CacheIRCompiler::emitGuardType()
{
    ValOperandId inputId = reader.valOperandId();
    JSValueType type = reader.valueType();

    if (allocator.knownType(inputId) == type)
        return true;

    ValueOperand input = allocator.useValueRegister(masm, inputId);

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
CacheIRCompiler::emitGuardClass()
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
CacheIRCompiler::emitGuardIsProxy()
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
CacheIRCompiler::emitGuardNotDOMProxy()
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
CacheIRCompiler::emitGuardMagicValue()
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
CacheIRCompiler::emitGuardNoUnboxedExpando()
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
CacheIRCompiler::emitGuardAndLoadUnboxedExpando()
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
CacheIRCompiler::emitGuardNoDetachedTypedObjects()
{
    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    CheckForTypedObjectWithDetachedStorage(cx_, masm, failure->label());
    return true;
}

bool
CacheIRCompiler::emitGuardNoDenseElements()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    AutoScratchRegister scratch(allocator, masm);

    FailurePath* failure;
    if (!addFailurePath(&failure))
        return false;

    // Load obj->elements.
    masm.loadPtr(Address(obj, NativeObject::offsetOfElements()), scratch);

    // Make sure there are no dense elements.
    Address initLength(scratch, ObjectElements::offsetOfInitializedLength());
    masm.branch32(Assembler::NotEqual, initLength, Imm32(0), failure->label());
    return true;
}

bool
CacheIRCompiler::emitLoadProto()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    Register reg = allocator.defineRegister(masm, reader.objOperandId());
    masm.loadObjProto(obj, reg);
    return true;
}

bool
CacheIRCompiler::emitLoadDOMExpandoValue()
{
    Register obj = allocator.useRegister(masm, reader.objOperandId());
    ValueOperand val = allocator.defineValueRegister(masm, reader.valOperandId());

    masm.loadPtr(Address(obj, ProxyObject::offsetOfValues()), val.scratchReg());
    masm.loadValue(Address(val.scratchReg(),
                           ProxyObject::offsetOfExtraSlotInValues(GetDOMProxyExpandoSlot())),
                   val);
    return true;
}
