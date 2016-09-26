/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_binary_iterator_h
#define wasm_binary_iterator_h

#include "mozilla/Poison.h"

#include "jsprf.h"

#include "asmjs/WasmTypes.h"
#include "jit/AtomicOp.h"

namespace js {
namespace wasm {

// The kind of a control-flow stack item.
enum class LabelKind : uint8_t {
    Block,
    Loop,
    Then,
    UnreachableThen, // like Then, but not reachable
    Else
};

#ifdef DEBUG
// Families of opcodes that share a signature and validation logic.
enum class ExprKind {
    Block,
    Loop,
    Unreachable,
    Drop,
    I32,
    I64,
    F32,
    F64,
    I8x16,
    I16x8,
    I32x4,
    F32x4,
    B8x16,
    B16x8,
    B32x4,
    Br,
    BrIf,
    BrTable,
    Nop,
    Nullary,
    Unary,
    Binary,
    Comparison,
    Conversion,
    Load,
    Store,
    TeeStore,
    Select,
    GetLocal,
    SetLocal,
    TeeLocal,
    GetGlobal,
    SetGlobal,
    TeeGlobal,
    Call,
    CallIndirect,
    OldCallIndirect,
    CallImport,
    Return,
    If,
    Else,
    End,
    AtomicLoad,
    AtomicStore,
    AtomicBinOp,
    AtomicCompareExchange,
    AtomicExchange,
    ExtractLane,
    ReplaceLane,
    Swizzle,
    Shuffle,
    Splat,
    SimdSelect,
    SimdCtor,
    SimdBooleanReduction,
    SimdShiftByScalar,
    SimdComparison,
};

// Return the ExprKind for a given Expr. This is used for sanity-checking that
// API users use the correct read function for a given Expr.
ExprKind
Classify(Expr expr);
#endif

// Common fields for linear memory access.
template <typename Value>
struct LinearMemoryAddress
{
    Value base;
    uint32_t offset;
    uint32_t align;

    LinearMemoryAddress()
    {}
    LinearMemoryAddress(Value base, uint32_t offset, uint32_t align)
      : base(base), offset(offset), align(align)
    {}
};

template <typename ControlItem>
class ControlStackEntry
{
    LabelKind kind_;
    bool reachable_;
    ExprType type_;
    size_t valueStackStart_;
    ControlItem controlItem_;

  public:
    ControlStackEntry(LabelKind kind, ExprType type, bool reachable, size_t valueStackStart)
      : kind_(kind), reachable_(reachable), type_(type), valueStackStart_(valueStackStart),
        controlItem_()
    {
        MOZ_ASSERT(type != ExprType::Limit);
    }

    LabelKind kind() const { return kind_; }
    ExprType type() const { return type_; }
    bool reachable() const { return reachable_; }
    size_t valueStackStart() const { return valueStackStart_; }
    ControlItem& controlItem() { return controlItem_; }

    void setReachable() { reachable_ = true; }

    void switchToElse(bool reachable) {
        MOZ_ASSERT(kind_ == LabelKind::Then || kind_ == LabelKind::UnreachableThen);
        reachable_ = reachable;
        kind_ = LabelKind::Else;
        controlItem_ = ControlItem();
    }
};

// Specialization for when there is no additional data needed.
template <>
class ControlStackEntry<Nothing>
{
    LabelKind kind_;
    bool reachable_;
    ExprType type_;
    size_t valueStackStart_;

  public:
    ControlStackEntry(LabelKind kind, ExprType type, bool reachable, size_t valueStackStart)
      : kind_(kind), reachable_(reachable), type_(type), valueStackStart_(valueStackStart)
    {
        MOZ_ASSERT(type != ExprType::Limit);
    }

    LabelKind kind() const { return kind_; }
    ExprType type() const { return type_; }
    bool reachable() const { return reachable_; }
    size_t valueStackStart() const { return valueStackStart_; }
    Nothing controlItem() { return Nothing(); }

    void setReachable() { reachable_ = true; }

    void switchToElse(bool reachable) {
        MOZ_ASSERT(kind_ == LabelKind::Then || kind_ == LabelKind::UnreachableThen);
        reachable_ = reachable;
        kind_ = LabelKind::Else;
    }
};

template <typename Value>
class TypeAndValue
{
    ValType type_;
    Value value_;

  public:
    TypeAndValue() : type_(ValType::Limit), value_() {}
    explicit TypeAndValue(ValType type)
      : type_(type), value_()
    {}
    TypeAndValue(ValType type, Value value)
      : type_(type), value_(value)
    {}
    ValType type() const {
        return type_;
    }
    Value value() const {
        return value_;
    }
    void setValue(Value value) {
        value_ = value;
    }
};

// Specialization for when there is no additional data needed.
template <>
class TypeAndValue<Nothing>
{
    ValType type_;

  public:
    TypeAndValue() : type_(ValType::Limit) {}
    explicit TypeAndValue(ValType type) : type_(type) {}

    TypeAndValue(ValType type, Nothing value)
      : type_(type)
    {}

    ValType type() const { return type_; }
    Nothing value() const { return Nothing(); }
    void setValue(Nothing value) {}
};

// A policy class for configuring ExprIter. Clients can use this as a
// base class, and override the behavior as needed.
struct ExprIterPolicy
{
    // Should the iterator perform validation, such as type checking and
    // validity checking?
    static const bool Validate = false;

    // Should the iterator produce output values?
    static const bool Output = false;

    // These members allow clients to add additional information to the value
    // and control stacks, respectively. Using Nothing means that no additional
    // field is added.
    typedef Nothing Value;
    typedef Nothing ControlItem;
};

// An iterator over the bytes of a function body. It performs validation
// (if Policy::Validate is true) and unpacks the data into a usable form.
//
// The MOZ_STACK_CLASS attribute here is because of the use of DebugOnly.
// There's otherwise nothing inherent in this class which would require
// it to be used on the stack.
template <typename Policy>
class MOZ_STACK_CLASS ExprIter : private Policy
{
    static const bool Validate = Policy::Validate;
    static const bool Output = Policy::Output;
    typedef typename Policy::Value Value;
    typedef typename Policy::ControlItem ControlItem;

    Decoder& d_;

    Vector<TypeAndValue<Value>, 0, SystemAllocPolicy> valueStack_;
    Vector<ControlStackEntry<ControlItem>, 0, SystemAllocPolicy> controlStack_;
    bool reachable_;

    DebugOnly<Expr> expr_;

    MOZ_MUST_USE bool readFixedU8(uint8_t* out) {
        if (Validate)
            return d_.readFixedU8(out);
        *out = d_.uncheckedReadFixedU8();
        return true;
    }
    MOZ_MUST_USE bool readFixedU32(uint32_t* out) {
        if (Validate)
            return d_.readFixedU32(out);
        *out = d_.uncheckedReadFixedU32();
        return true;
    }
    MOZ_MUST_USE bool readVarS32(int32_t* out) {
        if (Validate)
            return d_.readVarS32(out);
        *out = d_.uncheckedReadVarS32();
        return true;
    }
    MOZ_MUST_USE bool readVarU32(uint32_t* out) {
        if (Validate)
            return d_.readVarU32(out);
        *out = d_.uncheckedReadVarU32();
        return true;
    }
    MOZ_MUST_USE bool readVarS64(int64_t* out) {
        if (Validate)
            return d_.readVarS64(out);
        *out = d_.uncheckedReadVarS64();
        return true;
    }
    MOZ_MUST_USE bool readVarU64(uint64_t* out) {
        if (Validate)
            return d_.readVarU64(out);
        *out = d_.uncheckedReadVarU64();
        return true;
    }
    MOZ_MUST_USE bool readFixedF32(RawF32* out) {
        if (Validate)
            return d_.readFixedF32(out);
        *out = d_.uncheckedReadFixedF32();
        return true;
    }
    MOZ_MUST_USE bool readFixedF64(RawF64* out) {
        if (Validate)
            return d_.readFixedF64(out);
        *out = d_.uncheckedReadFixedF64();
        return true;
    }
    MOZ_MUST_USE bool readFixedI8x16(I8x16* out) {
        if (Validate)
            return d_.readFixedI8x16(out);
        d_.uncheckedReadFixedI8x16(out);
        return true;
    }
    MOZ_MUST_USE bool readFixedI16x8(I16x8* out) {
        if (Validate)
            return d_.readFixedI16x8(out);
        d_.uncheckedReadFixedI16x8(out);
        return true;
    }
    MOZ_MUST_USE bool readFixedI32x4(I32x4* out) {
        if (Validate)
            return d_.readFixedI32x4(out);
        d_.uncheckedReadFixedI32x4(out);
        return true;
    }
    MOZ_MUST_USE bool readFixedF32x4(F32x4* out) {
        if (Validate)
            return d_.readFixedF32x4(out);
        d_.uncheckedReadFixedF32x4(out);
        return true;
    }

    MOZ_MUST_USE bool readAtomicViewType(Scalar::Type* viewType) {
        uint8_t x;
        if (!readFixedU8(&x))
            return fail("unable to read atomic view");
        if (Validate && x >= Scalar::MaxTypedArrayViewType)
            return fail("invalid atomic view type");
        *viewType = Scalar::Type(x);
        return true;
    }

    MOZ_MUST_USE bool readAtomicBinOpOp(jit::AtomicOp* op) {
        uint8_t x;
        if (!readFixedU8(&x))
            return fail("unable to read atomic opcode");
        if (Validate) {
            switch (x) {
              case jit::AtomicFetchAddOp:
              case jit::AtomicFetchSubOp:
              case jit::AtomicFetchAndOp:
              case jit::AtomicFetchOrOp:
              case jit::AtomicFetchXorOp:
                break;
              default:
                return fail("unrecognized atomic binop");
            }
        }
        *op = jit::AtomicOp(x);
        return true;
    }

    MOZ_MUST_USE bool readLinearMemoryAddress(uint32_t byteSize, LinearMemoryAddress<Value>* addr);
    MOZ_MUST_USE bool readExprType(ExprType* expr);

    MOZ_MUST_USE bool typeMismatch(ExprType actual, ExprType expected) MOZ_COLD;
    MOZ_MUST_USE bool checkType(ValType actual, ValType expected);
    MOZ_MUST_USE bool checkType(ExprType actual, ExprType expected);

    MOZ_MUST_USE bool pushControl(LabelKind kind, ExprType type, bool reachable);
    MOZ_MUST_USE bool mergeControl(LabelKind* kind, ExprType* type, Value* value);
    MOZ_MUST_USE bool popControl(LabelKind* kind, ExprType* type, Value* value);

    MOZ_MUST_USE bool push(ValType t) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        return valueStack_.emplaceBack(t);
    }
    MOZ_MUST_USE bool push(TypeAndValue<Value> tv) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        return valueStack_.append(tv);
    }
    void infalliblePush(ValType t) {
        if (MOZ_UNLIKELY(!reachable_))
            return;
        valueStack_.infallibleEmplaceBack(t);
    }
    void infalliblePush(TypeAndValue<Value> tv) {
        if (MOZ_UNLIKELY(!reachable_))
            return;
        valueStack_.infallibleAppend(tv);
    }

    // Test whether reading the top of the value stack is currently valid.
    MOZ_MUST_USE bool checkTop() {
        MOZ_ASSERT(reachable_);
        if (Validate && valueStack_.length() <= controlStack_.back().valueStackStart()) {
            if (valueStack_.empty())
                return fail("popping value from empty stack");
            return fail("popping value from outside block");
        }
        return true;
    }

    // Pop the top of the value stack.
    MOZ_MUST_USE bool pop(TypeAndValue<Value>* tv) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        if (!checkTop())
            return false;
        *tv = valueStack_.popCopy();
        return true;
    }

    // Pop the top of the value stack and check that it has the given type.
    MOZ_MUST_USE bool popWithType(ValType expectedType, Value* value) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        if (!checkTop())
            return false;
        TypeAndValue<Value> tv = valueStack_.popCopy();
        if (!checkType(tv.type(), expectedType))
            return false;
        if (Output)
            *value = tv.value();
        return true;
    }

    // Pop the top of the value stack and discard the result.
    MOZ_MUST_USE bool pop() {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        if (!checkTop())
            return false;
        valueStack_.popBack();
        return true;
    }

    // Read the top of the value stack (without popping it).
    MOZ_MUST_USE bool top(TypeAndValue<Value>* tv) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        if (!checkTop())
            return false;
        *tv = valueStack_.back();
        return true;
    }

    // Read the top of the value stack (without popping it) and check that it
    // has the given type.
    MOZ_MUST_USE bool topWithType(ValType expectedType, Value* value) {
        if (MOZ_UNLIKELY(!reachable_))
            return true;
        if (!checkTop())
            return false;
        TypeAndValue<Value>& tv = valueStack_.back();
        if (!checkType(tv.type(), expectedType))
            return false;
        if (Output)
            *value = tv.value();
        return true;
    }

    // Read the value stack entry at depth |index|.
    MOZ_MUST_USE bool peek(uint32_t index, TypeAndValue<Value>* tv) {
        MOZ_ASSERT(reachable_);
        if (Validate && valueStack_.length() - controlStack_.back().valueStackStart() < index)
            return fail("peeking at value from outside block");
        *tv = valueStack_[valueStack_.length() - index];
        return true;
    }

    bool getControl(uint32_t relativeDepth, ControlStackEntry<ControlItem>** controlEntry) {
        if (Validate && relativeDepth >= controlStack_.length())
            return fail("branch depth exceeds current nesting level");

        *controlEntry = &controlStack_[controlStack_.length() - 1 - relativeDepth];
        return true;
    }

    void enterUnreachableCode() {
        valueStack_.shrinkTo(controlStack_.back().valueStackStart());
        reachable_ = false;
    }

    bool checkBrValue(uint32_t relativeDepth, ExprType* type, Value* value);
    bool checkBrIfValues(uint32_t relativeDepth, Value* condition, ExprType* type, Value* value);

  public:
    explicit ExprIter(Decoder& decoder)
      : d_(decoder), reachable_(true), expr_(Expr::Limit)
    {
    }

    // Return the decoding byte offset.
    uint32_t currentOffset() const { return d_.currentOffset(); }

    // Test whether the iterator has reached the end of the buffer.
    bool done() const { return d_.done(); }

    // Report a general failure.
    MOZ_MUST_USE bool fail(const char* msg) MOZ_COLD;

    // Report an unimplemented feature.
    MOZ_MUST_USE bool notYetImplemented(const char* what) MOZ_COLD;

    // Report an unrecognized opcode.
    MOZ_MUST_USE bool unrecognizedOpcode(Expr expr) MOZ_COLD;

    // Test whether the iterator is currently in "reachable" code.
    bool inReachableCode() const { return reachable_; }

    // ------------------------------------------------------------------------
    // Decoding and validation interface.

    MOZ_MUST_USE bool readExpr(Expr* expr);
    MOZ_MUST_USE bool readFunctionStart(ExprType ret);
    MOZ_MUST_USE bool readFunctionEnd();
    MOZ_MUST_USE bool readReturn(Value* value);
    MOZ_MUST_USE bool readBlock();
    MOZ_MUST_USE bool readLoop();
    MOZ_MUST_USE bool readIf(Value* condition);
    MOZ_MUST_USE bool readElse(ExprType* thenType, Value* thenValue);
    MOZ_MUST_USE bool readEnd(LabelKind* kind, ExprType* type, Value* value);
    MOZ_MUST_USE bool readBr(uint32_t* relativeDepth, ExprType* type, Value* value);
    MOZ_MUST_USE bool readBrIf(uint32_t* relativeDepth, ExprType* type,
                               Value* value, Value* condition);
    MOZ_MUST_USE bool readBrTable(uint32_t* tableLength, ExprType* type,
                                  Value* value, Value* index);
    MOZ_MUST_USE bool readBrTableEntry(ExprType* type, Value* value, uint32_t* depth);
    MOZ_MUST_USE bool readBrTableDefault(ExprType* type, Value* value, uint32_t* depth);
    MOZ_MUST_USE bool readUnreachable();
    MOZ_MUST_USE bool readDrop();
    MOZ_MUST_USE bool readUnary(ValType operandType, Value* input);
    MOZ_MUST_USE bool readConversion(ValType operandType, ValType resultType, Value* input);
    MOZ_MUST_USE bool readBinary(ValType operandType, Value* lhs, Value* rhs);
    MOZ_MUST_USE bool readComparison(ValType operandType, Value* lhs, Value* rhs);
    MOZ_MUST_USE bool readLoad(ValType resultType, uint32_t byteSize,
                               LinearMemoryAddress<Value>* addr);
    MOZ_MUST_USE bool readStore(ValType resultType, uint32_t byteSize,
                                LinearMemoryAddress<Value>* addr, Value* value);
    MOZ_MUST_USE bool readTeeStore(ValType resultType, uint32_t byteSize,
                                   LinearMemoryAddress<Value>* addr, Value* value);
    MOZ_MUST_USE bool readNop();
    MOZ_MUST_USE bool readNullary(ValType retType);
    MOZ_MUST_USE bool readSelect(ValType* type,
                                 Value* trueValue, Value* falseValue, Value* condition);
    MOZ_MUST_USE bool readGetLocal(const ValTypeVector& locals, uint32_t* id);
    MOZ_MUST_USE bool readSetLocal(const ValTypeVector& locals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readTeeLocal(const ValTypeVector& locals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readGetGlobal(const GlobalDescVector& globals, uint32_t* id);
    MOZ_MUST_USE bool readSetGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readTeeGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readI32Const(int32_t* i32);
    MOZ_MUST_USE bool readI64Const(int64_t* i64);
    MOZ_MUST_USE bool readF32Const(RawF32* f32);
    MOZ_MUST_USE bool readF64Const(RawF64* f64);
    MOZ_MUST_USE bool readI8x16Const(I8x16* i8x16);
    MOZ_MUST_USE bool readI16x8Const(I16x8* i16x8);
    MOZ_MUST_USE bool readI32x4Const(I32x4* i32x4);
    MOZ_MUST_USE bool readF32x4Const(F32x4* f32x4);
    MOZ_MUST_USE bool readB8x16Const(I8x16* i8x16);
    MOZ_MUST_USE bool readB16x8Const(I16x8* i16x8);
    MOZ_MUST_USE bool readB32x4Const(I32x4* i32x4);
    MOZ_MUST_USE bool readCall(uint32_t* calleeIndex);
    MOZ_MUST_USE bool readCallIndirect(uint32_t* sigIndex, Value* callee);
    MOZ_MUST_USE bool readOldCallIndirect(uint32_t* sigIndex);
    MOZ_MUST_USE bool readCallImport(uint32_t* importIndex);
    MOZ_MUST_USE bool readCallArg(ValType type, uint32_t numArgs, uint32_t argIndex, Value* arg);
    MOZ_MUST_USE bool readCallArgsEnd(uint32_t numArgs);
    MOZ_MUST_USE bool readOldCallIndirectCallee(Value* callee);
    MOZ_MUST_USE bool readCallReturn(ExprType ret);
    MOZ_MUST_USE bool readAtomicLoad(LinearMemoryAddress<Value>* addr,
                                     Scalar::Type* viewType);
    MOZ_MUST_USE bool readAtomicStore(LinearMemoryAddress<Value>* addr,
                                      Scalar::Type* viewType,
                                      Value* value);
    MOZ_MUST_USE bool readAtomicBinOp(LinearMemoryAddress<Value>* addr,
                                      Scalar::Type* viewType,
                                      jit::AtomicOp* op,
                                      Value* value);
    MOZ_MUST_USE bool readAtomicCompareExchange(LinearMemoryAddress<Value>* addr,
                                                Scalar::Type* viewType,
                                                Value* oldValue,
                                                Value* newValue);
    MOZ_MUST_USE bool readAtomicExchange(LinearMemoryAddress<Value>* addr,
                                         Scalar::Type* viewType,
                                         Value* newValue);
    MOZ_MUST_USE bool readSimdComparison(ValType simdType, Value* lhs,
                                         Value* rhs);
    MOZ_MUST_USE bool readSimdShiftByScalar(ValType simdType, Value* lhs,
                                            Value* rhs);
    MOZ_MUST_USE bool readSimdBooleanReduction(ValType simdType, Value* input);
    MOZ_MUST_USE bool readExtractLane(ValType simdType, uint8_t* lane,
                                      Value* vector);
    MOZ_MUST_USE bool readReplaceLane(ValType simdType, uint8_t* lane,
                                      Value* vector, Value* scalar);
    MOZ_MUST_USE bool readSplat(ValType simdType, Value* scalar);
    MOZ_MUST_USE bool readSwizzle(ValType simdType, uint8_t (* lanes)[16], Value* vector);
    MOZ_MUST_USE bool readShuffle(ValType simdType, uint8_t (* lanes)[16],
                                  Value* lhs, Value* rhs);
    MOZ_MUST_USE bool readSimdSelect(ValType simdType, Value* trueValue,
                                     Value* falseValue,
                                     Value* condition);
    MOZ_MUST_USE bool readSimdCtor();
    MOZ_MUST_USE bool readSimdCtorArg(ValType elementType, uint32_t numElements, uint32_t argIndex,
                                      Value* arg);
    MOZ_MUST_USE bool readSimdCtorArgsEnd(uint32_t numElements);
    MOZ_MUST_USE bool readSimdCtorReturn(ValType simdType);

    // ------------------------------------------------------------------------
    // Stack management.

    // Set the result value of the current top-of-value-stack expression.
    void setResult(Value value) {
        if (MOZ_LIKELY(reachable_))
            valueStack_.back().setValue(value);
    }

    // Return the result value of the current top-of-value-stack expression.
    Value getResult() {
        MOZ_ASSERT(reachable_);
        return valueStack_.back().value();
    }

    // Return a reference to the top of the control stack.
    ControlItem& controlItem() {
        return controlStack_.back().controlItem();
    }

    // Return the signature of the top of the control stack.
    ExprType controlType() {
        return controlStack_.back().type();
    }

    // Test whether the control-stack is empty, meaning we've consumed the final
    // end of the function body.
    bool controlStackEmpty() const {
        return controlStack_.empty();
    }
};

template <typename Policy>
bool
ExprIter<Policy>::typeMismatch(ExprType actual, ExprType expected)
{
    MOZ_ASSERT(Validate);
    MOZ_ASSERT(reachable_);

    UniqueChars error(JS_smprintf("type mismatch: expression has type %s but expected %s",
                                  ToCString(actual), ToCString(expected)));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkType(ValType actual, ValType expected)
{
    return checkType(ToExprType(actual), ToExprType(expected));
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkType(ExprType actual, ExprType expected)
{
    MOZ_ASSERT(reachable_);

    if (!Validate) {
        MOZ_ASSERT(actual == expected, "type mismatch");
        return true;
    }

    if (MOZ_LIKELY(actual == expected))
        return true;

    return typeMismatch(actual, expected);
}

template <typename Policy>
bool
ExprIter<Policy>::notYetImplemented(const char* what)
{
    UniqueChars error(JS_smprintf("not yet implemented: %s", what));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
bool
ExprIter<Policy>::unrecognizedOpcode(Expr expr)
{
    UniqueChars error(JS_smprintf("unrecognized opcode: %x", uint32_t(expr)));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
bool
ExprIter<Policy>::fail(const char* msg)
{
    return d_.fail(msg);
}

template <typename Policy>
inline bool
ExprIter<Policy>::pushControl(LabelKind kind, ExprType type, bool reachable)
{
    return controlStack_.emplaceBack(kind, type, reachable, valueStack_.length());
}

template <typename Policy>
inline bool
ExprIter<Policy>::mergeControl(LabelKind* kind, ExprType* type, Value* value)
{
    MOZ_ASSERT(!controlStack_.empty());

    ControlStackEntry<ControlItem>& controlItem = controlStack_.back();
    *kind = controlItem.kind();

    if (reachable_) {
        // Unlike branching, exiting a scope via fallthrough does not implicitly
        // pop excess items on the stack.
        size_t valueStackStart = controlItem.valueStackStart();
        size_t valueStackLength = valueStack_.length();
        MOZ_ASSERT(valueStackLength >= valueStackStart);
        if (valueStackLength == valueStackStart) {
            *type = ExprType::Void;
            if (!checkType(ExprType::Void, controlItem.type()))
                return false;
        } else {
            *type = controlItem.type();
            if (Validate && valueStackLength - valueStackStart > (IsVoid(*type) ? 0u : 1u))
                return fail("unused values not explicitly dropped by end of block");
            if (!topWithType(NonVoidToValType(*type), value))
                return false;
        }
    } else {
        if (*kind != LabelKind::Loop && controlItem.reachable()) {
            // There was no fallthrough path, but there was some other reachable
            // branch to the end.
            reachable_ = true;
            *type = controlItem.type();
            if (!IsVoid(*type)) {
                if (!push(NonVoidToValType(*type)))
                    return false;
            }
        } else {
            // No fallthrough and no branch to the end either; we remain
            // unreachable.
            *type = ExprType::Void;
        }
        if (Output)
            *value = Value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::popControl(LabelKind* kind, ExprType* type, Value* value)
{
    if (!mergeControl(kind, type, value))
        return false;

    if (*kind == LabelKind::Then) {
        // A reachable If without an Else. Forbid a result value.
        if (reachable_) {
            if (Validate && !IsVoid(*type))
                return fail("if without else with a result value");
        }
        reachable_ = true;
    }

    controlStack_.popBack();

    if (!reachable_ && !controlStack_.empty())
        valueStack_.shrinkTo(controlStack_.back().valueStackStart());

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readExprType(ExprType* type)
{
    uint8_t byte;
    if (!readFixedU8(&byte))
        return fail("unable to read block signature");

    if (Validate && byte >= uint8_t(ExprType::Limit))
        return fail("invalid inline type");

    *type = ExprType(byte);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readExpr(Expr* expr)
{
    if (Validate) {
        if (MOZ_UNLIKELY(!d_.readExpr(expr)))
            return fail("unable to read opcode");
    } else {
        *expr = d_.uncheckedReadExpr();
    }

    expr_ = *expr;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readFunctionStart(ExprType ret)
{
    MOZ_ASSERT(valueStack_.empty());
    MOZ_ASSERT(controlStack_.empty());
    MOZ_ASSERT(Expr(expr_) == Expr::Limit);
    MOZ_ASSERT(reachable_);

    return pushControl(LabelKind::Block, ret, false);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readFunctionEnd()
{
    if (Validate) {
        if (!controlStack_.empty())
            return fail("unbalanced function body control flow");
    } else {
        MOZ_ASSERT(controlStack_.empty());
    }

    expr_ = Expr::Limit;
    valueStack_.clear();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readReturn(Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Return);

    if (MOZ_LIKELY(reachable_)) {
        ControlStackEntry<ControlItem>& controlItem = controlStack_[0];
        MOZ_ASSERT(controlItem.kind() == LabelKind::Block);

        controlItem.setReachable();

        if (!IsVoid(controlItem.type())) {
            if (!popWithType(NonVoidToValType(controlItem.type()), value))
                return false;
        }
    }

    enterUnreachableCode();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBlock()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Block);

    ExprType type = ExprType::Limit;
    if (!readExprType(&type))
        return false;

    return pushControl(LabelKind::Block, type, false);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLoop()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Loop);

    ExprType type = ExprType::Limit;
    if (!readExprType(&type))
        return false;

    return pushControl(LabelKind::Loop, type, reachable_);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readIf(Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::If);

    ExprType type = ExprType::Limit;
    if (!readExprType(&type))
        return false;

    if (MOZ_LIKELY(reachable_)) {
        if (!popWithType(ValType::I32, condition))
            return false;

        return pushControl(LabelKind::Then, type, false);
    }

    return pushControl(LabelKind::UnreachableThen, type, false);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readElse(ExprType* thenType, Value* thenValue)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Else);

    // Finish up the then arm.
    ExprType type = ExprType::Limit;
    LabelKind kind;
    if (!mergeControl(&kind, &type, thenValue))
        return false;

    if (Output)
        *thenType = type;

    // Pop the old then value from the stack.
    if (!IsVoid(type))
        valueStack_.popBack();

    if (Validate && kind != LabelKind::Then && kind != LabelKind::UnreachableThen)
        return fail("else can only be used within an if");

    // Switch to the else arm.
    controlStack_.back().switchToElse(reachable_);

    reachable_ = kind != LabelKind::UnreachableThen;

    MOZ_ASSERT(valueStack_.length() == controlStack_.back().valueStackStart());

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readEnd(LabelKind* kind, ExprType* type, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::End);

    LabelKind validateKind = static_cast<LabelKind>(-1);
    ExprType validateType = ExprType::Limit;
    if (!popControl(&validateKind, &validateType, value))
        return false;

    if (Output) {
        *kind = validateKind;
        *type = validateType;
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkBrValue(uint32_t relativeDepth, ExprType* type, Value* value)
{
    if (MOZ_LIKELY(reachable_)) {
        ControlStackEntry<ControlItem>* controlItem = nullptr;
        if (!getControl(relativeDepth, &controlItem))
            return false;

        if (controlItem->kind() != LabelKind::Loop) {
            controlItem->setReachable();

            ExprType expectedType = controlItem->type();
            if (Output)
                *type = expectedType;

            if (!IsVoid(expectedType))
                return topWithType(NonVoidToValType(expectedType), value);
        }
    }

    if (Output) {
        *type = ExprType::Void;
        *value = Value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBr(uint32_t* relativeDepth, ExprType* type, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Br);

    uint32_t validateRelativeDepth;
    if (!readVarU32(&validateRelativeDepth))
        return fail("unable to read br depth");

    if (!checkBrValue(validateRelativeDepth, type, value))
        return false;

    if (Output)
        *relativeDepth = validateRelativeDepth;

    enterUnreachableCode();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkBrIfValues(uint32_t relativeDepth, Value* condition,
                                  ExprType* type, Value* value)
{
    if (MOZ_LIKELY(reachable_)) {
        if (!popWithType(ValType::I32, condition))
            return false;

        ControlStackEntry<ControlItem>* controlItem = nullptr;
        if (!getControl(relativeDepth, &controlItem))
            return false;

        if (controlItem->kind() != LabelKind::Loop) {
            controlItem->setReachable();

            ExprType expectedType = controlItem->type();
            if (Output)
                *type = expectedType;

            if (!IsVoid(expectedType))
                return topWithType(NonVoidToValType(expectedType), value);
        }
    }

    if (Output) {
        *type = ExprType::Void;
        *value = Value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrIf(uint32_t* relativeDepth, ExprType* type, Value* value, Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrIf);

    uint32_t validateRelativeDepth;
    if (!readVarU32(&validateRelativeDepth))
        return fail("unable to read br_if depth");

    if (!checkBrIfValues(validateRelativeDepth, condition, type, value))
        return false;

    if (Output)
        *relativeDepth = validateRelativeDepth;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTable(uint32_t* tableLength, ExprType* type,
                              Value* value, Value* index)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrTable);

    if (!readVarU32(tableLength))
        return fail("unable to read br_table table length");

    if (MOZ_LIKELY(reachable_)) {
        if (!popWithType(ValType::I32, index))
            return false;
    }

    // Set *type to indicate that we don't know the type yet.
    *type = ExprType::Limit;
    if (Output)
        *value = Value();

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTableEntry(ExprType* type, Value* value, uint32_t* depth)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrTable);

    if (!readVarU32(depth))
        return false;

    if (MOZ_LIKELY(reachable_)) {
        ControlStackEntry<ControlItem>* controlItem = nullptr;
        if (!getControl(*depth, &controlItem))
            return false;

        if (controlItem->kind() != LabelKind::Loop) {
            controlItem->setReachable();

            // If we've already seen one label, we know the type and can check
            // that the type for the current label matches it.
            ExprType knownType = *type;
            if (knownType != ExprType::Limit)
                return checkType(knownType, controlItem->type());

            // This is the first label; record the type and the value now.
            ExprType expectedType = controlItem->type();
            if (!IsVoid(expectedType)) {
                *type = expectedType;
                return popWithType(NonVoidToValType(expectedType), value);
            }
        }
    }

    *type = ExprType::Void;
    if (Output)
        *value = Value();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTableDefault(ExprType* type, Value* value, uint32_t* depth)
{
    if (!readBrTableEntry(type, value, depth))
        return false;

    MOZ_ASSERT(!reachable_ || *type != ExprType::Limit);

    enterUnreachableCode();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnreachable()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Unreachable);

    enterUnreachableCode();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readDrop()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Drop);

    if (!pop())
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnary(ValType operandType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Unary);

    if (!popWithType(operandType, input))
        return false;

    infalliblePush(operandType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readConversion(ValType operandType, ValType resultType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Conversion);

    if (!popWithType(operandType, input))
        return false;

    infalliblePush(resultType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBinary(ValType operandType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Binary);

    if (!popWithType(operandType, rhs))
        return false;

    if (!popWithType(operandType, lhs))
        return false;

    infalliblePush(operandType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readComparison(ValType operandType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Comparison);

    if (!popWithType(operandType, rhs))
        return false;

    if (!popWithType(operandType, lhs))
        return false;

    infalliblePush(ValType::I32);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLinearMemoryAddress(uint32_t byteSize, LinearMemoryAddress<Value>* addr)
{
    uint8_t alignLog2;
    if (!readFixedU8(&alignLog2))
        return fail("unable to read load alignment");

    uint32_t unusedOffset;
    if (!readVarU32(Output ? &addr->offset : &unusedOffset))
        return fail("unable to read load offset");

    if (Validate && (alignLog2 >= 32 || (uint32_t(1) << alignLog2) > byteSize))
        return fail("greater than natural alignment");

    Value unused;
    if (!popWithType(ValType::I32, Output ? &addr->base : &unused))
        return false;

    if (Output)
        addr->align = uint32_t(1) << alignLog2;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLoad(ValType resultType, uint32_t byteSize,
                           LinearMemoryAddress<Value>* addr)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Load);

    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    infalliblePush(resultType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readStore(ValType resultType, uint32_t byteSize,
                            LinearMemoryAddress<Value>* addr, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Store);

    if (!popWithType(resultType, value))
        return false;

    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readTeeStore(ValType resultType, uint32_t byteSize,
                               LinearMemoryAddress<Value>* addr, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::TeeStore);

    if (!popWithType(resultType, value))
        return false;

    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    infalliblePush(TypeAndValue<Value>(resultType, Output ? *value : Value()));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readNop()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Nop);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readNullary(ValType retType)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Nullary);

    if (!push(retType))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSelect(ValType* type, Value* trueValue, Value* falseValue, Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Select);

    if (!popWithType(ValType::I32, condition))
        return false;

    TypeAndValue<Value> false_;
    if (!pop(&false_))
        return false;

    TypeAndValue<Value> true_;
    if (!pop(&true_))
        return false;

    ValType resultType = true_.type();
    if (Validate && resultType != false_.type())
        return fail("select operand types must match");

    infalliblePush(resultType);

    if (Output) {
        *type = resultType;
        *trueValue = true_.value();
        *falseValue = false_.value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readGetLocal(const ValTypeVector& locals, uint32_t* id)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::GetLocal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= locals.length())
        return fail("get_local index out of range");

    if (!push(locals[validateId]))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetLocal(const ValTypeVector& locals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SetLocal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= locals.length())
        return fail("set_local index out of range");

    if (!popWithType(locals[validateId], value))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readTeeLocal(const ValTypeVector& locals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::TeeLocal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= locals.length())
        return fail("set_local index out of range");

    if (!topWithType(locals[validateId], value))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readGetGlobal(const GlobalDescVector& globals, uint32_t* id)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::GetGlobal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= globals.length())
        return fail("get_global index out of range");

    if (!push(globals[validateId].type()))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SetGlobal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= globals.length())
        return fail("set_global index out of range");

    if (Validate && !globals[validateId].isMutable())
        return fail("can't write an immutable global");

    if (!popWithType(globals[validateId].type(), value))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readTeeGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::TeeGlobal);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= globals.length())
        return fail("set_global index out of range");

    if (Validate && !globals[validateId].isMutable())
        return fail("can't write an immutable global");

    if (!topWithType(globals[validateId].type(), value))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI32Const(int32_t* i32)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I32);

    int32_t unused;
    if (!readVarS32(Output ? i32 : &unused))
        return false;

    if (!push(ValType::I32))
       return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI64Const(int64_t* i64)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I64);

    int64_t unused;
    if (!readVarS64(Output ? i64 : &unused))
        return false;

    if (!push(ValType::I64))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF32Const(RawF32* f32)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F32);

    RawF32 unused;
    if (!readFixedF32(Output ? f32 : &unused))
        return false;

    if (!push(ValType::F32))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF64Const(RawF64* f64)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F64);

    RawF64 unused;
    if (!readFixedF64(Output ? f64 : &unused))
       return false;

    if (!push(ValType::F64))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI8x16Const(I8x16* i8x16)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I8x16);

    I8x16 unused;
    if (!readFixedI8x16(Output ? i8x16 : &unused))
        return false;

    if (!push(ValType::I8x16))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI16x8Const(I16x8* i16x8)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I16x8);

    I16x8 unused;
    if (!readFixedI16x8(Output ? i16x8 : &unused))
        return false;

    if (!push(ValType::I16x8))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI32x4Const(I32x4* i32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I32x4);

    I32x4 unused;
    if (!readFixedI32x4(Output ? i32x4 : &unused))
        return false;

    if (!push(ValType::I32x4))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF32x4Const(F32x4* f32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F32x4);

    F32x4 unused;
    if (!readFixedF32x4(Output ? f32x4 : &unused))
        return false;

    if (!push(ValType::F32x4))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB8x16Const(I8x16* i8x16)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B8x16);

    I8x16 unused;
    if (!readFixedI8x16(Output ? i8x16 : &unused))
        return false;

    if (!push(ValType::B8x16))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB16x8Const(I16x8* i16x8)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B16x8);

    I16x8 unused;
    if (!readFixedI16x8(Output ? i16x8 : &unused))
        return false;

    if (!push(ValType::B16x8))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB32x4Const(I32x4* i32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B32x4);

    I32x4 unused;
    if (!readFixedI32x4(Output ? i32x4 : &unused))
        return false;

    if (!push(ValType::B32x4))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCall(uint32_t* calleeIndex)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Call);

    if (!readVarU32(calleeIndex))
        return fail("unable to read call function index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallIndirect(uint32_t* sigIndex, Value* callee)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::CallIndirect);

    if (!readVarU32(sigIndex))
        return fail("unable to read call_indirect signature index");

    if (reachable_) {
        if (!popWithType(ValType::I32, callee))
            return false;
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readOldCallIndirect(uint32_t* sigIndex)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::OldCallIndirect);

    if (!readVarU32(sigIndex))
        return fail("unable to read call_indirect signature index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallImport(uint32_t* importIndex)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::CallImport);

    if (!readVarU32(importIndex))
        return fail("unable to read call_import import index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallArg(ValType type, uint32_t numArgs, uint32_t argIndex, Value* arg)
{
    MOZ_ASSERT(reachable_);

    TypeAndValue<Value> tv;

    if (!peek(numArgs - argIndex, &tv))
        return false;
    if (!checkType(tv.type(), type))
        return false;

    if (Output)
        *arg = tv.value();

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallArgsEnd(uint32_t numArgs)
{
    MOZ_ASSERT(reachable_);
    MOZ_ASSERT(numArgs <= valueStack_.length());

    valueStack_.shrinkBy(numArgs);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readOldCallIndirectCallee(Value* callee)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::OldCallIndirect);
    MOZ_ASSERT(reachable_);

    if (!popWithType(ValType::I32, callee))
        return false;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallReturn(ExprType ret)
{
    MOZ_ASSERT(reachable_);

    if (!IsVoid(ret)) {
        if (!push(NonVoidToValType(ret)))
            return false;
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicLoad(LinearMemoryAddress<Value>* addr, Scalar::Type* viewType)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::AtomicLoad);

    Scalar::Type validateViewType;
    if (!readAtomicViewType(&validateViewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(validateViewType);
    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    infalliblePush(ValType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicStore(LinearMemoryAddress<Value>* addr,
                                  Scalar::Type* viewType, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::AtomicStore);

    Scalar::Type validateViewType;
    if (!readAtomicViewType(&validateViewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(validateViewType);
    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    if (!popWithType(ValType::I32, value))
        return false;

    infalliblePush(ValType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicBinOp(LinearMemoryAddress<Value>* addr, Scalar::Type* viewType,
                                  jit::AtomicOp* op, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::AtomicBinOp);

    Scalar::Type validateViewType;
    if (!readAtomicViewType(&validateViewType))
        return false;

    if (!readAtomicBinOpOp(op))
        return false;

    uint32_t byteSize = Scalar::byteSize(validateViewType);
    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    if (!popWithType(ValType::I32, value))
        return false;

    infalliblePush(ValType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicCompareExchange(LinearMemoryAddress<Value>* addr,
                                            Scalar::Type* viewType,
                                            Value* oldValue, Value* newValue)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::AtomicCompareExchange);

    Scalar::Type validateViewType;
    if (!readAtomicViewType(&validateViewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(validateViewType);
    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    if (!popWithType(ValType::I32, newValue))
        return false;

    if (!popWithType(ValType::I32, oldValue))
        return false;

    infalliblePush(ValType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicExchange(LinearMemoryAddress<Value>* addr,
                                     Scalar::Type* viewType,
                                     Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::AtomicExchange);

    Scalar::Type validateViewType;
    if (!readAtomicViewType(&validateViewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(validateViewType);
    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    if (!popWithType(ValType::I32, value))
        return false;

    infalliblePush(ValType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdComparison(ValType simdType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdComparison);

    if (!popWithType(simdType, rhs))
        return false;

    if (!popWithType(simdType, lhs))
        return false;

    infalliblePush(SimdBoolType(simdType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdShiftByScalar(ValType simdType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdShiftByScalar);

    if (!popWithType(ValType::I32, rhs))
        return false;

    if (!popWithType(simdType, lhs))
        return false;

    infalliblePush(simdType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdBooleanReduction(ValType simdType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdBooleanReduction);

    if (!popWithType(simdType, input))
        return false;

    infalliblePush(ValType::I32);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readExtractLane(ValType simdType, uint8_t* lane, Value* vector)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::ExtractLane);

    uint32_t laneBits;
    if (!readVarU32(&laneBits))
        return false;

    if (Validate && laneBits >= NumSimdElements(simdType))
        return fail("simd lane out of bounds for simd type");

    if (!popWithType(simdType, vector))
        return false;

    infalliblePush(SimdElementType(simdType));

    if (Output)
        *lane = uint8_t(laneBits);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readReplaceLane(ValType simdType, uint8_t* lane, Value* vector, Value* scalar)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::ReplaceLane);

    uint32_t laneBits;
    if (!readVarU32(&laneBits))
        return false;

    if (Validate && laneBits >= NumSimdElements(simdType))
        return fail("simd lane out of bounds for simd type");

    if (!popWithType(SimdElementType(simdType), scalar))
        return false;

    if (!popWithType(simdType, vector))
        return false;

    infalliblePush(simdType);

    if (Output)
        *lane = uint8_t(laneBits);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSplat(ValType simdType, Value* scalar)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Splat);

    if (!popWithType(SimdElementType(simdType), scalar))
        return false;

    infalliblePush(simdType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSwizzle(ValType simdType, uint8_t (* lanes)[16], Value* vector)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Swizzle);

    uint32_t numSimdLanes = NumSimdElements(simdType);
    MOZ_ASSERT(numSimdLanes <= mozilla::ArrayLength(*lanes));
    for (uint32_t i = 0; i < numSimdLanes; ++i) {
        uint8_t validateLane;
        if (!readFixedU8(Output ? &(*lanes)[i] : &validateLane))
            return fail("unable to read swizzle lane");
        if (Validate && (Output ? (*lanes)[i] : validateLane) >= numSimdLanes)
            return fail("swizzle index out of bounds");
    }

    if (!popWithType(simdType, vector))
        return false;

    infalliblePush(simdType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readShuffle(ValType simdType, uint8_t (* lanes)[16], Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Shuffle);

    uint32_t numSimdLanes = NumSimdElements(simdType);
    MOZ_ASSERT(numSimdLanes <= mozilla::ArrayLength(*lanes));
    for (uint32_t i = 0; i < numSimdLanes; ++i) {
        uint8_t validateLane;
        if (!readFixedU8(Output ? &(*lanes)[i] : &validateLane))
            return fail("unable to read shuffle lane");
        if (Validate && (Output ? (*lanes)[i] : validateLane) >= numSimdLanes * 2)
            return fail("shuffle index out of bounds");
    }

    if (!popWithType(simdType, rhs))
        return false;

    if (!popWithType(simdType, lhs))
        return false;

    infalliblePush(simdType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdSelect(ValType simdType, Value* trueValue, Value* falseValue,
                                 Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdSelect);

    if (!popWithType(simdType, falseValue))
        return false;
    if (!popWithType(simdType, trueValue))
        return false;
    if (!popWithType(SimdBoolType(simdType), condition))
        return false;

    infalliblePush(simdType);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtor()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdCtor);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtorArg(ValType elementType, uint32_t numElements, uint32_t index,
                                  Value* arg)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdCtor);
    MOZ_ASSERT(numElements > 0);

    TypeAndValue<Value> tv;

    if (!peek(numElements - index, &tv))
        return false;
    if (!checkType(tv.type(), elementType))
        return false;

    *arg = tv.value();
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtorArgsEnd(uint32_t numElements)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdCtor);
    MOZ_ASSERT(numElements <= valueStack_.length());

    valueStack_.shrinkBy(numElements);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtorReturn(ValType simdType)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdCtor);

    infalliblePush(simdType);

    return true;
}

} // namespace wasm
} // namespace js

namespace mozilla {

// Specialize IsPod for the Nothing specializations.
template<> struct IsPod<js::wasm::TypeAndValue<Nothing>> : TrueType {};
template<> struct IsPod<js::wasm::ControlStackEntry<Nothing>> : TrueType {};

} // namespace mozilla

#endif // wasm_iterator_h
