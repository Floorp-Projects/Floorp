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
enum class LabelKind : uint8_t { Block, Loop, Then, Else };

#ifdef DEBUG
// Families of opcodes that share a signature and validation logic.
enum class ExprKind {
    Block,
    Loop,
    Unreachable,
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
    Nullary,
    Unary,
    Binary,
    Comparison,
    Conversion,
    Load,
    Store,
    Select,
    GetVar,
    SetVar,
    Call,
    CallIndirect,
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

struct Nothing {};

template <typename ControlItem>
class ControlStackEntry
{
    LabelKind kind_;
    ExprType type_;
    size_t valueStackStart_;
    ControlItem controlItem_;

  public:
    ControlStackEntry(LabelKind kind, size_t valueStackStart)
      : kind_(kind), type_(AnyType), valueStackStart_(valueStackStart)
    {}

    LabelKind kind() const { return kind_; }
    size_t valueStackStart() const { return valueStackStart_; }
    const ExprType& type() const { return type_; }
    ExprType& type() { return type_; }
    ControlItem& controlItem() { return controlItem_; }
};

// Specialization for when there is no additional data needed.
template <>
class ControlStackEntry<Nothing>
{
    LabelKind kind_;
    ExprType type_;
    size_t valueStackStart_;

  public:
    ControlStackEntry(LabelKind kind, size_t valueStackStart)
      : kind_(kind), type_(AnyType), valueStackStart_(valueStackStart)
    {}

    LabelKind kind() const { return kind_; }
    size_t valueStackStart() const { return valueStackStart_; }
    const ExprType& type() const { return type_; }
    ExprType& type() { return type_; }
    Nothing controlItem() { return Nothing(); }
};

template <typename Value>
class TypeAndValue
{
    ExprType type_;
    Value value_;

  public:
    TypeAndValue() = default;
    explicit TypeAndValue(ExprType type)
      : type_(type)
    {}
    TypeAndValue(ExprType type, Value value)
      : type_(type), value_(value)
    {}
    ExprType type() const {
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
    ExprType type_;

  public:
    TypeAndValue() = default;
    explicit TypeAndValue(ExprType type) : type_(type) {}

    TypeAndValue(ExprType type, Nothing value)
      : type_(type)
    {}

    ExprType type() const { return type_; }
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

    // This function is called to report failures.
    static bool fail(const char*, Decoder&) {
        MOZ_CRASH("unexpected validation failure");
        return false;
    }

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
    MOZ_MUST_USE bool readFixedF32(float* out) {
        if (Validate)
            return d_.readFixedF32(out);
        *out = d_.uncheckedReadFixedF32();
        return true;
    }
    MOZ_MUST_USE bool readFixedF64(double* out) {
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
            return false;
        if (Validate && x >= Scalar::MaxTypedArrayViewType)
            return fail("invalid atomic view type");
        *viewType = Scalar::Type(x);
        return true;
    }

    MOZ_MUST_USE bool readAtomicBinOpOp(jit::AtomicOp* op) {
        uint8_t x;
        if (!readFixedU8(&x))
            return false;
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

    MOZ_MUST_USE bool typeMismatch(ExprType actual, ExprType expected) MOZ_COLD;
    MOZ_MUST_USE bool checkType(ExprType actual, ExprType expected);
    MOZ_MUST_USE bool readFunctionReturnValue(ExprType ret);
    MOZ_MUST_USE bool checkBranch(uint32_t relativeDepth, ExprType type);
    MOZ_MUST_USE bool pushControl(LabelKind kind);
    MOZ_MUST_USE bool popControl(LabelKind* kind, ExprType* type, Value* value);
    MOZ_MUST_USE bool popControlAfterCheck(LabelKind* kind, ExprType* type, Value* value);
    MOZ_MUST_USE bool push(ExprType t) { return valueStack_.emplaceBack(t); }
    MOZ_MUST_USE bool push(TypeAndValue<Value> tv) { return valueStack_.append(tv); }

    MOZ_MUST_USE bool readLinearMemoryAddress(uint32_t byteSize, LinearMemoryAddress<Value>* addr);

    void infallibleCheckSuccessor(ControlStackEntry<ControlItem>& controlItem, ExprType type);
    void infalliblePush(ExprType t) { valueStack_.infallibleEmplaceBack(t); }
    void infalliblePush(TypeAndValue<Value> tv) { valueStack_.infallibleAppend(tv); }

    // Test whether reading the top of the value stack is currently valid.
    MOZ_MUST_USE bool checkTop() {
        if (Validate && valueStack_.length() <= controlStack_.back().valueStackStart())
            return fail("popping value from outside block");
        return true;
    }

    // Pop the top of the value stack.
    MOZ_MUST_USE bool pop(TypeAndValue<Value>* tv) {
        if (!checkTop())
            return false;
        *tv = valueStack_.popCopy();
        return true;
    }

    // Pop the top of the value stack and check that it has the given type.
    MOZ_MUST_USE bool popWithType(ExprType expectedType, Value* value) {
        if (!checkTop())
            return false;
        TypeAndValue<Value> tv = valueStack_.popCopy();
        if (!checkType(tv.type(), expectedType))
            return false;
        if (Output)
            *value = tv.value();
        return true;
    }

    // Read the top of the value stack (without popping it).
    MOZ_MUST_USE bool top(TypeAndValue<Value>* tv) {
        if (!checkTop())
            return false;
        *tv = valueStack_.back();
        return true;
    }

    // Read the top of the value stack (without popping it) and check that it
    // has the given type.
    MOZ_MUST_USE bool topWithType(ExprType expectedType, Value* value) {
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
        if (Validate && valueStack_.length() - controlStack_.back().valueStackStart() <= index)
            return fail("peeking at value from outside block");
        *tv = valueStack_[valueStack_.length() - index];
        return true;
    }

  public:
    ExprIter(Policy policy, Decoder& decoder)
      : Policy(policy), d_(decoder)
    {
        expr_ = Expr::Limit;
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

    // ------------------------------------------------------------------------
    // Decoding and validation interface.

    MOZ_MUST_USE bool readExpr(Expr* expr);
    MOZ_MUST_USE bool readFunctionStart();
    MOZ_MUST_USE bool readFunctionEnd(ExprType ret, Value* value);
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
    MOZ_MUST_USE bool readBrTableEntry(ExprType type, uint32_t* depth);
    MOZ_MUST_USE bool readUnreachable();
    MOZ_MUST_USE bool readUnary(ValType operandType, Value* input);
    MOZ_MUST_USE bool readConversion(ValType operandType, ValType resultType, Value* input);
    MOZ_MUST_USE bool readBinary(ValType operandType, Value* lhs, Value* rhs);
    MOZ_MUST_USE bool readComparison(ValType operandType, Value* lhs, Value* rhs);
    MOZ_MUST_USE bool readLoad(ValType resultType, uint32_t byteSize,
                               LinearMemoryAddress<Value>* addr);
    MOZ_MUST_USE bool readStore(ValType resultType, uint32_t byteSize,
                                LinearMemoryAddress<Value>* addr, Value* value);
    MOZ_MUST_USE bool readNullary();
    MOZ_MUST_USE bool readSelect(ExprType* type,
                                 Value* trueValue, Value* falseValue, Value* condition);
    MOZ_MUST_USE bool readGetLocal(const ValTypeVector& locals, uint32_t* id);
    MOZ_MUST_USE bool readSetLocal(const ValTypeVector& locals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readGetGlobal(const GlobalDescVector& globals, uint32_t* id);
    MOZ_MUST_USE bool readSetGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value);
    MOZ_MUST_USE bool readI32Const(int32_t* i32);
    MOZ_MUST_USE bool readI64Const(int64_t* i64);
    MOZ_MUST_USE bool readF32Const(float* f32);
    MOZ_MUST_USE bool readF64Const(double* f64);
    MOZ_MUST_USE bool readI8x16Const(I8x16* i8x16);
    MOZ_MUST_USE bool readI16x8Const(I16x8* i16x8);
    MOZ_MUST_USE bool readI32x4Const(I32x4* i32x4);
    MOZ_MUST_USE bool readF32x4Const(F32x4* f32x4);
    MOZ_MUST_USE bool readB8x16Const(I8x16* i8x16);
    MOZ_MUST_USE bool readB16x8Const(I16x8* i16x8);
    MOZ_MUST_USE bool readB32x4Const(I32x4* i32x4);
    MOZ_MUST_USE bool readCall(uint32_t* calleeIndex, uint32_t* arity);
    MOZ_MUST_USE bool readCallIndirect(uint32_t* sigIndex, uint32_t* arity);
    MOZ_MUST_USE bool readCallImport(uint32_t* importIndex, uint32_t* arity);
    MOZ_MUST_USE bool readCallArg(ValType type, uint32_t numArgs, uint32_t argIndex, Value* arg);
    MOZ_MUST_USE bool readCallArgsEnd(uint32_t numArgs);
    MOZ_MUST_USE bool readCallIndirectCallee(Value* callee);
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
    MOZ_MUST_USE bool readSimdCtorArg(ValType elementType, uint32_t numElements, uint32_t argIndex, Value* arg);
    MOZ_MUST_USE bool readSimdCtorArgsEnd(uint32_t numElements);
    MOZ_MUST_USE bool readSimdCtorReturn(ValType simdType);

    // ------------------------------------------------------------------------
    // Stack management.

    // Set the result value of the current top-of-value-stack expression.
    void setResult(Value value) {
        valueStack_.back().setValue(value);
    }

    // Return the result value of the current top-of-value-stack expression.
    Value getResult() {
        return valueStack_.back().value();
    }

    // Return a reference to the top of the control stack.
    ControlItem& controlItem() {
        return controlStack_.back().controlItem();
    }
};

template <typename Policy>
inline bool
ExprIter<Policy>::typeMismatch(ExprType actual, ExprType expected)
{
    UniqueChars error(JS_smprintf("type mismatch: expression has type %s but expected %s",
                                  ToCString(actual), ToCString(expected)));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkType(ExprType actual, ExprType expected)
{
    if (!Validate) {
        MOZ_ASSERT(actual == AnyType || actual == expected, "type mismatch");
        return true;
    }

    if (MOZ_LIKELY(actual == AnyType || actual == expected))
        return true;

    return typeMismatch(actual, expected);
}

template <typename Policy>
inline bool
ExprIter<Policy>::notYetImplemented(const char* what)
{
    UniqueChars error(JS_smprintf("not yet implemented: %s", what));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
inline bool
ExprIter<Policy>::unrecognizedOpcode(Expr expr)
{
    UniqueChars error(JS_smprintf("unrecognized opcode: %x", uint32_t(expr)));
    if (!error)
        return false;

    return fail(error.get());
}

template <typename Policy>
inline bool
ExprIter<Policy>::fail(const char* msg) {
    return Policy::fail(msg, d_);
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
ExprIter<Policy>::readFunctionStart()
{
    MOZ_ASSERT(valueStack_.empty());
    MOZ_ASSERT(controlStack_.empty());
    MOZ_ASSERT(Expr(expr_) == Expr::Limit);

    return pushControl(LabelKind::Block);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readFunctionEnd(ExprType ret, Value* value)
{
    expr_ = Expr::Limit;

    if (Validate) {
        MOZ_ASSERT(controlStack_.length() > 0);
        if (controlStack_.length() != 1)
            return fail("unbalanced function body control flow");
    } else {
        MOZ_ASSERT(controlStack_.length() == 1);
    }

    ExprType type;
    LabelKind kind;
    if (!popControlAfterCheck(&kind, &type, value))
        return false;

    MOZ_ASSERT(kind == LabelKind::Block);
    MOZ_ASSERT(valueStack_.length() == 1);

    if (!IsVoid(ret)) {
        if (!checkType(type, ret))
            return false;
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readReturn(Value* value)
{
    ControlStackEntry<ControlItem>& controlItem = controlStack_[0];
    MOZ_ASSERT(controlItem.kind() == LabelKind::Block);
    MOZ_ASSERT(Classify(expr_) == ExprKind::Return);

    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("failed to read return arity");
    if (arity > 1)
        return fail("return arity too big");

    TypeAndValue<Value> tv;
    if (arity) {
        if (!pop(&tv))
            return false;
    } else {
        tv = TypeAndValue<Value>(ExprType::Void);
    }

    infallibleCheckSuccessor(controlItem, tv.type());

    if (!push(AnyType))
        return false;

    if (Output)
        *value = tv.value();

    return true;
}

template <typename Policy>
inline void
ExprIter<Policy>::infallibleCheckSuccessor(ControlStackEntry<ControlItem>& controlItem,
                                     ExprType type)
{
    controlItem.type() = Unify(controlItem.type(), type);
}

template <typename Policy>
inline bool
ExprIter<Policy>::checkBranch(uint32_t relativeDepth, ExprType type)
{
    // FIXME: Don't allow branching to the function-body block for now.
    if (Validate && relativeDepth >= controlStack_.length() - 1)
        return fail("branch depth exceeds current nesting level");

    ControlStackEntry<ControlItem>& controlItem =
        controlStack_[controlStack_.length() - 1 - relativeDepth];

    if (controlItem.kind() != LabelKind::Loop)
       infallibleCheckSuccessor(controlItem, type);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::pushControl(LabelKind kind)
{
    size_t length = valueStack_.length();

    // Push a void value at the start of every control region, in case the
    // region is empty.
    if (!push(ExprType::Void))
        return false;

    return controlStack_.emplaceBack(kind, length);
}

template <typename Policy>
inline bool
ExprIter<Policy>::popControl(LabelKind* kind, ExprType* type, Value* value)
{
    MOZ_ASSERT(controlStack_.length() > 0);
    if (controlStack_.length() <= 1)
        return fail("unbalanced function body control flow");

    return popControlAfterCheck(kind, type, value);
}

template <typename Policy>
inline bool
ExprIter<Policy>::popControlAfterCheck(LabelKind* kind, ExprType* type, Value* value)
{
    TypeAndValue<Value> tv;
    if (!pop(&tv))
        return false;

    if (Output)
        *value = tv.value();

    ControlStackEntry<ControlItem> controlItem = controlStack_.popCopy();
    *kind = controlItem.kind();

    infallibleCheckSuccessor(controlItem, tv.type());

    *type = controlItem.type();

    // Clear out the value stack up to the start of the block/loop.
    valueStack_.shrinkTo(controlItem.valueStackStart());

    infalliblePush(controlItem.type());
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBlock()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Block);

    return pushControl(LabelKind::Block);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLoop()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Loop);

    return pushControl(LabelKind::Block) &&
           pushControl(LabelKind::Loop);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readIf(Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::If);

    if (!popWithType(ExprType::I32, condition))
        return false;

    return pushControl(LabelKind::Then);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readElse(ExprType* thenType, Value* thenValue)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Else);

    ExprType type;
    LabelKind kind;
    if (!popControl(&kind, &type, thenValue))
        return false;

    if (Validate && kind != LabelKind::Then)
        return fail("else can only be used within an if");

    // Pop and discard the old then value for now.
    TypeAndValue<Value> tv;
    if (!pop(&tv))
        return false;

    if (!pushControl(LabelKind::Else))
        return false;

    // Initialize the else block's type with the then block's type, so that
    // the two get unified.
    ControlStackEntry<ControlItem>& controlItem = controlStack_.back();
    controlItem.type() = type;

    if (Output)
        *thenType = type;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readEnd(LabelKind* kind, ExprType* type, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::End);

    LabelKind validateKind;
    ExprType validateType;
    if (!popControl(&validateKind, &validateType, value))
        return false;

    switch (validateKind) {
      case LabelKind::Block:
        break;
      case LabelKind::Loop: {
        // Note: Propose a spec change: loops don't implicitly have an end label.

        if (Output)
            setResult(*value);

        LabelKind blockKind;
        if (!popControl(&blockKind, &validateType, value))
            return false;

        MOZ_ASSERT(blockKind == LabelKind::Block);
        break;
      }
      case LabelKind::Then:
        valueStack_.back() = TypeAndValue<Value>(ExprType::Void);
        if (Output)
            *type = ExprType::Void;
        break;
      case LabelKind::Else:
        break;
    }

    if (Output) {
        *kind = validateKind;
        *type = validateType;
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBr(uint32_t* relativeDepth, ExprType* type, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Br);

    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read br arity");
    if (arity > 1)
        return fail("br arity too big");

    uint32_t validateRelativeDepth;
    if (!readVarU32(&validateRelativeDepth))
        return fail("unable to read br depth");

    TypeAndValue<Value> tv;
    if (arity) {
        if (!pop(&tv))
            return false;
    } else {
        tv = TypeAndValue<Value>(ExprType::Void);
    }

    if (!checkBranch(validateRelativeDepth, tv.type()))
        return false;

    if (!push(AnyType))
        return false;

    if (Output) {
        *relativeDepth = validateRelativeDepth;
        *type = tv.type();
        *value = tv.value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrIf(uint32_t* relativeDepth, ExprType* type, Value* value, Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrIf);

    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read br_if arity");
    if (arity > 1)
        return fail("br_if arity too big");

    uint32_t validateRelativeDepth;
    if (!readVarU32(&validateRelativeDepth))
        return fail("unable to read br_if depth");

    if (!popWithType(ExprType::I32, condition))
        return false;

    TypeAndValue<Value> tv;
    if (arity) {
        if (!top(&tv))
            return false;
    } else {
        tv = TypeAndValue<Value>(ExprType::Void);
        if (!push(tv))
            return false;
    }

    if (!checkBranch(validateRelativeDepth, tv.type()))
        return false;

    if (Output) {
        *relativeDepth = validateRelativeDepth;
        *type = tv.type();
        *value = tv.value();
    }

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTable(uint32_t* tableLength, ExprType* type,
                              Value* value, Value* index)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrTable);

    if (!popWithType(ExprType::I32, index))
        return false;

    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read br_table arity");
    if (arity > 1)
        return fail("br_table arity too big");

    TypeAndValue<Value> tv;
    if (arity) {
        if (!top(&tv))
            return false;
    } else {
        tv = TypeAndValue<Value>(ExprType::Void);
        if (!push(tv))
            return false;
    }
    *type = tv.type();
    if (Output)
        *value = tv.value();

    if (!readVarU32(tableLength))
        return fail("unable to read br_table table length");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTableEntry(ExprType type, uint32_t* depth)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::BrTable);

    return readFixedU32(depth) &&
           checkBranch(*depth, type);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnreachable()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Unreachable);

    return push(AnyType);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnary(ValType operandType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Unary);

    if (!popWithType(ToExprType(operandType), input))
        return false;

    infalliblePush(ToExprType(operandType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readConversion(ValType operandType, ValType resultType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Conversion);

    if (!popWithType(ToExprType(operandType), input))
        return false;

    infalliblePush(ToExprType(resultType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBinary(ValType operandType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Binary);

    if (!popWithType(ToExprType(operandType), rhs))
        return false;

    if (!popWithType(ToExprType(operandType), lhs))
        return false;

    infalliblePush(ToExprType(operandType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readComparison(ValType operandType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Comparison);

    if (!popWithType(ToExprType(operandType), rhs))
        return false;

    if (!popWithType(ToExprType(operandType), lhs))
        return false;

    infalliblePush(ExprType::I32);

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLinearMemoryAddress(uint32_t byteSize, LinearMemoryAddress<Value>* addr)
{
    Value unused;
    if (!popWithType(ExprType::I32, Output ? &addr->base : &unused))
        return false;

    uint8_t alignLog2;
    if (!readFixedU8(&alignLog2))
        return fail("unable to read load alignment");
    if (Validate && (alignLog2 >= 32 || (uint32_t(1) << alignLog2) > byteSize))
        return fail("greater than natural alignment");
    if (Output)
        addr->align = uint32_t(1) << alignLog2;

    uint32_t unusedOffset;
    if (!readVarU32(Output ? &addr->offset : &unusedOffset))
        return fail("unable to read load offset");

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

    infalliblePush(ToExprType(resultType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readStore(ValType resultType, uint32_t byteSize,
                            LinearMemoryAddress<Value>* addr, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Store);

    if (!popWithType(ToExprType(resultType), value))
        return false;

    if (!readLinearMemoryAddress(byteSize, addr))
        return false;

    infalliblePush(TypeAndValue<Value>(ToExprType(resultType), Output ? *value : Value()));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readNullary()
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Nullary);

    return push(ExprType::Void);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSelect(ExprType* type, Value* trueValue, Value* falseValue, Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Select);

    if (!popWithType(ExprType::I32, condition))
        return false;

    TypeAndValue<Value> false_;
    if (!pop(&false_))
        return false;

    TypeAndValue<Value> true_;
    if (!pop(&true_))
        return false;

    ExprType resultType = Unify(true_.type(), false_.type());
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
    MOZ_ASSERT(Classify(expr_) == ExprKind::GetVar);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= locals.length())
        return fail("get_local index out of range");

    if (!push(ToExprType(locals[validateId])))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetLocal(const ValTypeVector& locals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SetVar);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= locals.length())
        return fail("set_local index out of range");

    if (!topWithType(ToExprType(locals[validateId]), value))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readGetGlobal(const GlobalDescVector& globals, uint32_t* id)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::GetVar);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= globals.length())
        return fail("get_global index out of range");

    if (!push(ToExprType(globals[validateId].type)))
        return false;

    if (Output)
        *id = validateId;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetGlobal(const GlobalDescVector& globals, uint32_t* id, Value* value)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SetVar);

    uint32_t validateId;
    if (!readVarU32(&validateId))
        return false;

    if (Validate && validateId >= globals.length())
        return fail("set_global index out of range");

    if (!topWithType(ToExprType(globals[validateId].type), value))
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
    return readVarS32(Output ? i32 : &unused) &&
           push(ExprType::I32);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI64Const(int64_t* i64)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I64);

    int64_t unused;
    return readVarS64(Output ? i64 : &unused) &&
           push(ExprType::I64);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF32Const(float* f32)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F32);

    float validateF32;
    if (!readFixedF32(Output ? f32 : &validateF32))
        return false;

    if (Validate && mozilla::IsNaN(Output ? *f32 : validateF32)) {
        const float jsNaN = (float)JS::GenericNaN();
        if (memcmp(Output ? f32 : &validateF32, &jsNaN, sizeof(*f32)) != 0)
            return notYetImplemented("NaN literals with custom payloads");
    }

    return push(ExprType::F32);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF64Const(double* f64)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F64);

    double validateF64;
    if (!readFixedF64(Output ? f64 : &validateF64))
       return false;

    if (Validate && mozilla::IsNaN(Output ? *f64 : validateF64)) {
        const double jsNaN = JS::GenericNaN();
        if (memcmp(Output ? f64 : &validateF64, &jsNaN, sizeof(*f64)) != 0)
            return notYetImplemented("NaN literals with custom payloads");
    }

    return push(ExprType::F64);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI8x16Const(I8x16* i8x16)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I8x16);

    I8x16 unused;
    return readFixedI8x16(Output ? i8x16 : &unused) &&
           push(ExprType::I8x16);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI16x8Const(I16x8* i16x8)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I16x8);

    I16x8 unused;
    return readFixedI16x8(Output ? i16x8 : &unused) &&
           push(ExprType::I16x8);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readI32x4Const(I32x4* i32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::I32x4);

    I32x4 unused;
    return readFixedI32x4(Output ? i32x4 : &unused) &&
           push(ExprType::I32x4);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readF32x4Const(F32x4* f32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::F32x4);

    F32x4 unused;
    return readFixedF32x4(Output ? f32x4 : &unused) &&
           push(ExprType::F32x4);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB8x16Const(I8x16* i8x16)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B8x16);

    I8x16 unused;
    return readFixedI8x16(Output ? i8x16 : &unused) &&
           push(ExprType::B8x16);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB16x8Const(I16x8* i16x8)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B16x8);

    I16x8 unused;
    return readFixedI16x8(Output ? i16x8 : &unused) &&
           push(ExprType::B16x8);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readB32x4Const(I32x4* i32x4)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::B32x4);

    I32x4 unused;
    return readFixedI32x4(Output ? i32x4 : &unused) &&
           push(ExprType::B32x4);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCall(uint32_t* calleeIndex, uint32_t* arity)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Call);

    if (!readVarU32(arity))
        return fail("unable to read call arity");

    if (!readVarU32(calleeIndex))
        return fail("unable to read call function index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallIndirect(uint32_t* sigIndex, uint32_t* arity)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::CallIndirect);

    if (!readVarU32(arity))
        return fail("unable to read call_indirect arity");

    if (!readVarU32(sigIndex))
        return fail("unable to read call_indirect signature index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallImport(uint32_t* importIndex, uint32_t* arity)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::CallImport);

    if (!readVarU32(arity))
        return fail("unable to read call_import arity");

    if (!readVarU32(importIndex))
        return fail("unable to read call_import import index");

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallArg(ValType type, uint32_t numArgs, uint32_t argIndex, Value* arg)
{
    TypeAndValue<Value> tv;
    if (!peek(numArgs - argIndex, &tv))
        return false;
    if (!checkType(tv.type(), ToExprType(type)))
        return false;

    if (Output)
        *arg = tv.value();

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallArgsEnd(uint32_t numArgs)
{
    MOZ_ASSERT(numArgs <= valueStack_.length());

    valueStack_.shrinkBy(numArgs);
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallIndirectCallee(Value* callee)
{
    return popWithType(ExprType::I32, callee);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallReturn(ExprType ret)
{
    return push(ret);
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

    infalliblePush(ExprType::I32);

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

    if (!popWithType(ExprType::I32, value))
        return false;

    infalliblePush(ExprType::I32);

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

    if (!popWithType(ExprType::I32, value))
        return false;

    infalliblePush(ExprType::I32);

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

    if (!popWithType(ExprType::I32, newValue))
        return false;

    if (!popWithType(ExprType::I32, oldValue))
        return false;

    infalliblePush(ExprType::I32);

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

    if (!popWithType(ExprType::I32, value))
        return false;

    infalliblePush(ExprType::I32);

    if (Output)
        *viewType = validateViewType;

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdComparison(ValType simdType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdComparison);

    if (!popWithType(ToExprType(simdType), rhs))
        return false;

    if (!popWithType(ToExprType(simdType), lhs))
        return false;

    infalliblePush(ToExprType(SimdBoolType(simdType)));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdShiftByScalar(ValType simdType, Value* lhs, Value* rhs)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdShiftByScalar);

    if (!popWithType(ExprType::I32, rhs))
        return false;

    if (!popWithType(ToExprType(simdType), lhs))
        return false;

    infalliblePush(ToExprType(simdType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdBooleanReduction(ValType simdType, Value* input)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdBooleanReduction);

    if (!popWithType(ToExprType(simdType), input))
        return false;

    infalliblePush(ExprType::I32);

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
    if (Output)
        *lane = uint8_t(laneBits);

    if (!popWithType(ToExprType(simdType), vector))
        return false;

    infalliblePush(ToExprType(SimdElementType(simdType)));

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
    if (Output)
        *lane = uint8_t(laneBits);

    if (!popWithType(ToExprType(SimdElementType(simdType)), scalar))
        return false;

    if (!popWithType(ToExprType(simdType), vector))
        return false;

    infalliblePush(ToExprType(simdType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSplat(ValType simdType, Value* scalar)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::Splat);

    if (!popWithType(ToExprType(SimdElementType(simdType)), scalar))
        return false;

    infalliblePush(ToExprType(simdType));

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
            return false;
        if (Validate && (Output ? (*lanes)[i] : validateLane) >= numSimdLanes)
            return fail("swizzle index out of bounds");
    }

    if (!popWithType(ToExprType(simdType), vector))
        return false;

    infalliblePush(ToExprType(simdType));

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
            return false;
        if (Validate && (Output ? (*lanes)[i] : validateLane) >= numSimdLanes * 2)
            return fail("shuffle index out of bounds");
    }

    if (!popWithType(ToExprType(simdType), rhs))
        return false;

    if (!popWithType(ToExprType(simdType), lhs))
        return false;

    infalliblePush(ToExprType(simdType));

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdSelect(ValType simdType, Value* trueValue, Value* falseValue,
                                 Value* condition)
{
    MOZ_ASSERT(Classify(expr_) == ExprKind::SimdSelect);

    if (!popWithType(ToExprType(simdType), falseValue))
        return false;
    if (!popWithType(ToExprType(simdType), trueValue))
        return false;
    if (!popWithType(ToExprType(SimdBoolType(simdType)), condition))
        return false;

    infalliblePush(ToExprType(simdType));

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

    TypeAndValue<Value> tv;
    if (!peek(numElements - index, &tv))
        return false;
    if (!checkType(tv.type(), ToExprType(elementType)))
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

    return push(ToExprType(simdType));
}

} // namespace wasm
} // namespace js

namespace mozilla {

// Specialize IsPod for the Nothing specializations.
template<> struct IsPod<js::wasm::TypeAndValue<js::wasm::Nothing>> : TrueType {};
template<> struct IsPod<js::wasm::ControlStackEntry<js::wasm::Nothing>> : TrueType {};

} // namespace mozilla

#endif // wasm_iterator_h
