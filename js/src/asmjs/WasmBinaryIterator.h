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

#ifndef wasm_iterator_h
#define wasm_iterator_h

#include "mozilla/Poison.h"

#include "jsprf.h"

#include "asmjs/WasmTypes.h"
#include "jit/AtomicOp.h"

namespace js {
namespace wasm {

// The kind of a control-flow stack item.
enum class LabelKind : uint8_t { Block, Loop, Then, Else };

// A description of a unary operator's parameters.
template <typename Value>
struct UnaryRecord
{
    Value op;

    explicit UnaryRecord(Value op) : op(op) {}
};

// A description of a binary operator's parameters.
template <typename Value>
struct BinaryRecord
{
    Value lhs;
    Value rhs;

    BinaryRecord(Value lhs, Value rhs) : lhs(lhs), rhs(rhs) {}
};

// A description of a select operator's parameters.
template <typename Value>
struct SelectRecord
{
    ExprType type;
    Value trueValue;
    Value falseValue;
    Value condition;

    SelectRecord(ExprType type, Value trueValue, Value falseValue, Value condition)
      : type(type), trueValue(trueValue), falseValue(falseValue), condition(condition)
    {}
};

// Common fields for linear memory access.
template <typename Value>
struct LinearMemoryAddress
{
    Value base;
    uint32_t offset;
    uint32_t align;

    LinearMemoryAddress(Value base, uint32_t offset, uint32_t align)
      : base(base), offset(offset), align(align)
    {}
};

// A description of a load operator's parameters.
template <typename Value>
struct LoadRecord
{
    LinearMemoryAddress<Value> addr;

    LoadRecord(Value base, uint32_t offset, uint32_t align)
      : addr(base, offset, align)
    {}
};

// A description of a store operator's parameters.
template <typename Value>
struct StoreRecord
{
    LinearMemoryAddress<Value> addr;
    Value value;

    StoreRecord(Value base, uint32_t offset, uint32_t align, Value value)
      : addr(base, offset, align), value(value)
    {}
};

// A description of an if operator's parameters.
template <typename Value>
struct IfRecord
{
    Value condition;

    explicit IfRecord(Value condition) : condition(condition) {}
};

// A description of an else operator's parameters.
template <typename Value>
struct ElseRecord
{
    ExprType type;
    Value thenValue;

    ElseRecord(ExprType type, Value thenValue) : type(type), thenValue(thenValue) {}
};

// A description of a block, loop, if, or else operator's parameters.
template <typename Value>
struct EndRecord
{
    LabelKind kind;
    ExprType type;
    Value value;

    explicit EndRecord(LabelKind kind, ExprType type, Value value)
      : kind(kind), type(type), value(value)
    {}
};

// A description of a br operator's parameters.
template <typename Value>
struct BrRecord
{
    uint32_t relativeDepth;
    ExprType type;
    Value value;

    BrRecord(uint32_t relativeDepth, ExprType type, Value value)
      : relativeDepth(relativeDepth), type(type), value(value) {}
};

// A description of a br_if operator's parameters.
template <typename Value>
struct BrIfRecord
{
    uint32_t relativeDepth;
    ExprType type;
    Value value;
    Value condition;

    BrIfRecord(uint32_t relativeDepth, ExprType type, Value value, Value condition)
      : relativeDepth(relativeDepth), type(type), value(value), condition(condition)
    {}
};

// A description of a br_table operator's parameters.
template <typename Value>
struct BrTableRecord
{
    uint32_t tableLength;
    ExprType type;
    Value value;
    Value index;

    BrTableRecord(uint32_t tableLength, ExprType type, Value value, Value index)
      : tableLength(tableLength), type(type), value(value), index(index)
    {}
};

// A description of a get_local or get_global operator's parameters.
struct GetVarRecord
{
    uint32_t id;

    explicit GetVarRecord(uint32_t id) : id(id) {}
};

// A description of a set_local or set_global operator's parameters.
template <typename Value>
struct SetVarRecord
{
    uint32_t id;
    Value value;

    SetVarRecord(uint32_t id, Value value) : id(id), value(value) {}
};

// A description of a call operator's parameters, not counting the call arguments.
struct CallRecord
{
    uint32_t arity;
    uint32_t callee;

    CallRecord(uint32_t arity, uint32_t callee)
      : arity(arity), callee(callee)
    {}
};

// A description of a call_indirect operator's parameters, not counting the call arguments.
template <typename Value>
struct CallIndirectRecord
{
    uint32_t arity;
    uint32_t sigIndex;

    CallIndirectRecord(uint32_t arity, uint32_t sigIndex)
      : arity(arity), sigIndex(sigIndex)
    {}
};

// A description of a call_import operator's parameters, not counting the call arguments.
struct CallImportRecord
{
    uint32_t arity;
    uint32_t callee;

    CallImportRecord(uint32_t arity, uint32_t callee)
      : arity(arity), callee(callee)
    {}
};

// A description of a return operator's parameters.
template <typename Value>
struct ReturnRecord
{
    Value value;

    explicit ReturnRecord(Value value) : value(value) {}
};

template <typename Value>
struct AtomicLoadRecord
{
    LinearMemoryAddress<Value> addr;
    Scalar::Type viewType;
};

template <typename Value>
struct AtomicStoreRecord
{
    LinearMemoryAddress<Value> addr;
    Scalar::Type viewType;
    Value value;
};

template <typename Value>
struct AtomicBinOpRecord
{
    LinearMemoryAddress<Value> addr;
    Scalar::Type viewType;
    jit::AtomicOp op;
    Value value;
};

template <typename Value>
struct AtomicCompareExchangeRecord
{
    LinearMemoryAddress<Value> addr;
    Scalar::Type viewType;
    Value oldValue;
    Value newValue;
};

template <typename Value>
struct AtomicExchangeRecord
{
    LinearMemoryAddress<Value> addr;
    Scalar::Type viewType;
    Value value;
};

template <typename Value>
struct ExtractLaneRecord
{
    jit::SimdLane lane;
    Value vector;

    ExtractLaneRecord(jit::SimdLane lane, Value vector) : lane(lane), vector(vector) {}
};

template <typename Value>
struct ReplaceLaneRecord
{
    jit::SimdLane lane;
    Value vector;
    Value scalar;

    ReplaceLaneRecord(jit::SimdLane lane, Value vector, Value scalar)
      : lane(lane), vector(vector), scalar(scalar)
    {}
};

template <typename Value>
struct SwizzleRecord
{
    uint8_t lanes[4];
    Value vector;

    SwizzleRecord(uint8_t lanes[4], Value vector)
      : vector(vector)
    {
        memcpy(this->lanes, lanes, sizeof(this->lanes));
    }
};

template <typename Value>
struct ShuffleRecord
{
    uint8_t lanes[4];
    Value lhs;
    Value rhs;

    ShuffleRecord(uint8_t lanes[4], Value lhs, Value rhs)
      : lhs(lhs), rhs(rhs)
    {
        memcpy(this->lanes, lanes, sizeof(this->lanes));
    }
};

template <typename Value>
struct SimdSelectRecord
{
    Value trueValue;
    Value falseValue;
    Value condition;

    SimdSelectRecord(Value trueValue, Value falseValue, Value condition)
      : trueValue(trueValue), falseValue(falseValue), condition(condition) {}
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
struct TypeAndValue<Nothing>
{
    ExprType type_;

  public:
    TypeAndValue() {}
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
template <typename Policy>
class ExprIter : private Policy
{
    static const bool Validate = Policy::Validate;
    typedef typename Policy::Value Value;
    typedef typename Policy::ControlItem ControlItem;

    // A union containing all the expression description types.
    union ExprRecord {
        ExprRecord() {
#ifdef DEBUG
            mozWritePoison(this, sizeof(*this));
#endif
        }

        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        I32x4 i32x4;
        F32x4 f32x4;
        BrRecord<Value> br;
        BrIfRecord<Value> brIf;
        BrTableRecord<Value> brTable;
        UnaryRecord<Value> unary;
        BinaryRecord<Value> binary;
        LoadRecord<Value> load;
        StoreRecord<Value> store;
        SelectRecord<Value> select;
        GetVarRecord getVar;
        SetVarRecord<Value> setVar;
        CallRecord call;
        CallIndirectRecord<Value> callIndirect;
        CallImportRecord callImport;
        ReturnRecord<Value> return_;
        IfRecord<Value> if_;
        ElseRecord<Value> else_;
        EndRecord<Value> end;
        AtomicLoadRecord<Value> atomicLoad;
        AtomicStoreRecord<Value> atomicStore;
        AtomicBinOpRecord<Value> atomicBinOp;
        AtomicCompareExchangeRecord<Value> atomicCompareExchange;
        AtomicExchangeRecord<Value> atomicExchange;
        ExtractLaneRecord<Value> extractLane;
        ReplaceLaneRecord<Value> replaceLane;
        SwizzleRecord<Value> swizzle;
        ShuffleRecord<Value> shuffle;
        SimdSelectRecord<Value> simdSelect;
    };

    Decoder& d_;

    Vector<TypeAndValue<Value>, 0, SystemAllocPolicy> valueStack_;
    Vector<ControlStackEntry<ControlItem>, 0, SystemAllocPolicy> controlStack_;

    ExprRecord u_;

#ifdef DEBUG
    Expr expr_;
    bool isInitialized_;
#endif

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

#ifdef DEBUG
    bool isI32()     const { return isInitialized_ && expr_ == Expr::I32Const; }
    bool isI64()     const { return isInitialized_ && expr_ == Expr::I64Const; }
    bool isF32()     const { return isInitialized_ && expr_ == Expr::F32Const; }
    bool isF64()     const { return isInitialized_ && expr_ == Expr::F64Const; }
    bool isI32x4()   const { return isInitialized_ && (expr_ == Expr::I32x4Const ||
                                                       expr_ == Expr::B32x4Const); }
    bool isF32x4()   const { return isInitialized_ && expr_ == Expr::F32x4Const; }
    bool isBr()      const { return isInitialized_ && expr_ == Expr::Br; }
    bool isBrIf()    const { return isInitialized_ && expr_ == Expr::BrIf; }
    bool isBrTable() const { return isInitialized_ && expr_ == Expr::BrTable; }
    bool isUnary() const {
        return isInitialized_ &&
               (expr_ == Expr::I32Clz || expr_ == Expr::I32Ctz ||
                expr_ == Expr::I32Popcnt || expr_ == Expr::I32Eqz ||
                expr_ == Expr::I64Clz || expr_ == Expr::I64Ctz ||
                expr_ == Expr::I64Popcnt || expr_ == Expr::I64Eqz ||
                expr_ == Expr::F32Abs || expr_ == Expr::F32Neg ||
                expr_ == Expr::F32Ceil || expr_ == Expr::F32Floor ||
                expr_ == Expr::F32Sqrt || expr_ == Expr::F64Abs ||
                expr_ == Expr::F64Neg || expr_ == Expr::F64Ceil ||
                expr_ == Expr::F64Floor || expr_ == Expr::F64Sqrt ||
                expr_ == Expr::I32WrapI64 || expr_ == Expr::I32TruncSF32 ||
                expr_ == Expr::I32TruncUF32 || expr_ == Expr::I32ReinterpretF32 ||
                expr_ == Expr::I32TruncSF64 || expr_ == Expr::I32TruncUF64 ||
                expr_ == Expr::I64ExtendSI32 || expr_ == Expr::I64ExtendUI32 ||
                expr_ == Expr::I64TruncSF32 || expr_ == Expr::I64TruncUF32 ||
                expr_ == Expr::I64TruncSF64 || expr_ == Expr::I64TruncUF64 ||
                expr_ == Expr::I64ReinterpretF64 || expr_ == Expr::F32ConvertSI32 ||
                expr_ == Expr::F32ConvertUI32 || expr_ == Expr::F32ReinterpretI32 ||
                expr_ == Expr::F32ConvertSI64 || expr_ == Expr::F32ConvertUI64 ||
                expr_ == Expr::F32DemoteF64 || expr_ == Expr::F64ConvertSI32 ||
                expr_ == Expr::F64ConvertUI32 || expr_ == Expr::F64ConvertSI64 ||
                expr_ == Expr::F64ConvertUI64 || expr_ == Expr::F64ReinterpretI64 ||
                expr_ == Expr::F64PromoteF32 ||
                expr_ == Expr::I32BitNot || expr_ == Expr::I32Abs ||
                expr_ == Expr::F64Sin || expr_ == Expr::F64Cos ||
                expr_ == Expr::F64Tan || expr_ == Expr::F64Asin ||
                expr_ == Expr::F64Acos || expr_ == Expr::F64Atan ||
                expr_ == Expr::F64Exp || expr_ == Expr::F64Log ||
                expr_ == Expr::I32Neg ||
                expr_ == Expr::I32x4splat || expr_ == Expr::F32x4splat ||
                expr_ == Expr::B32x4splat ||
                expr_ == Expr::I32x4check || expr_ == Expr::F32x4check ||
                expr_ == Expr::B32x4check ||
                expr_ == Expr::I32x4fromFloat32x4 ||
                expr_ == Expr::I32x4fromFloat32x4U ||
                expr_ == Expr::F32x4fromInt32x4 ||
                expr_ == Expr::F32x4fromUint32x4 ||
                expr_ == Expr::I32x4fromFloat32x4Bits ||
                expr_ == Expr::F32x4fromInt32x4Bits ||
                expr_ == Expr::F32x4fromUint32x4Bits ||
                expr_ == Expr::I32x4neg || expr_ == Expr::I32x4not ||
                expr_ == Expr::F32x4neg || expr_ == Expr::F32x4sqrt ||
                expr_ == Expr::F32x4abs ||
                expr_ == Expr::F32x4reciprocalApproximation ||
                expr_ == Expr::F32x4reciprocalSqrtApproximation ||
                expr_ == Expr::B32x4not ||
                expr_ == Expr::B32x4anyTrue ||
                expr_ == Expr::B32x4allTrue);
    }
    bool isBinary() const {
        return isInitialized_ &&
               (expr_ == Expr::I32Add || expr_ == Expr::I32Sub ||
                expr_ == Expr::I32Mul || expr_ == Expr::I32DivS ||
                expr_ == Expr::I32DivU || expr_ == Expr::I32RemS ||
                expr_ == Expr::I32RemU || expr_ == Expr::I32And ||
                expr_ == Expr::I32Or || expr_ == Expr::I32Xor ||
                expr_ == Expr::I32Shl || expr_ == Expr::I32ShrS ||
                expr_ == Expr::I32ShrU || expr_ == Expr::I32Rotl ||
                expr_ == Expr::I32Rotr || expr_ == Expr::I64Add ||
                expr_ == Expr::I64Sub || expr_ == Expr::I64Mul ||
                expr_ == Expr::I64DivS || expr_ == Expr::I64DivU ||
                expr_ == Expr::I64RemS || expr_ == Expr::I64RemU ||
                expr_ == Expr::I64And || expr_ == Expr::I64Or ||
                expr_ == Expr::I64Xor || expr_ == Expr::I64Shl ||
                expr_ == Expr::I64ShrS || expr_ == Expr::I64ShrU ||
                expr_ == Expr::I64Rotl || expr_ == Expr::I64Rotr ||
                expr_ == Expr::F32Add || expr_ == Expr::F32Sub ||
                expr_ == Expr::F32Mul || expr_ == Expr::F32Div ||
                expr_ == Expr::F32Min || expr_ == Expr::F32Max ||
                expr_ == Expr::F32CopySign || expr_ == Expr::F64Add ||
                expr_ == Expr::F64Sub || expr_ == Expr::F64Mul ||
                expr_ == Expr::F64Div || expr_ == Expr::F64Min ||
                expr_ == Expr::F64Max || expr_ == Expr::F64CopySign ||
                expr_ == Expr::I32Eq || expr_ == Expr::I32Ne ||
                expr_ == Expr::I32LtS || expr_ == Expr::I32LtU ||
                expr_ == Expr::I32LeS || expr_ == Expr::I32LeU ||
                expr_ == Expr::I32GtS || expr_ == Expr::I32GtU ||
                expr_ == Expr::I32GeS || expr_ == Expr::I32GeU ||
                expr_ == Expr::I64Eq || expr_ == Expr::I64Ne ||
                expr_ == Expr::I64LtS || expr_ == Expr::I64LtU ||
                expr_ == Expr::I64LeS || expr_ == Expr::I64LeU ||
                expr_ == Expr::I64GtS || expr_ == Expr::I64GtU ||
                expr_ == Expr::I64GeS || expr_ == Expr::I64GeU ||
                expr_ == Expr::F32Eq || expr_ == Expr::F32Ne ||
                expr_ == Expr::F32Lt || expr_ == Expr::F32Le ||
                expr_ == Expr::F32Gt || expr_ == Expr::F32Ge ||
                expr_ == Expr::F64Eq || expr_ == Expr::F64Ne ||
                expr_ == Expr::F64Lt || expr_ == Expr::F64Le ||
                expr_ == Expr::F64Gt || expr_ == Expr::F64Ge ||
                expr_ == Expr::I32Min || expr_ == Expr::I32Max ||
                expr_ == Expr::F64Mod || expr_ == Expr::F64Pow ||
                expr_ == Expr::F64Atan2 ||
                expr_ == Expr::I32x4add || expr_ == Expr::I32x4sub ||
                expr_ == Expr::I32x4mul ||
                expr_ == Expr::I32x4and || expr_ == Expr::I32x4or ||
                expr_ == Expr::I32x4xor ||
                expr_ == Expr::I32x4shiftLeftByScalar ||
                expr_ == Expr::I32x4shiftRightByScalar ||
                expr_ == Expr::I32x4shiftRightByScalarU ||
                expr_ == Expr::I32x4equal || expr_ == Expr::I32x4notEqual ||
                expr_ == Expr::I32x4greaterThan ||
                expr_ == Expr::I32x4greaterThanOrEqual ||
                expr_ == Expr::I32x4lessThan ||
                expr_ == Expr::I32x4lessThanOrEqual ||
                expr_ == Expr::I32x4greaterThanU ||
                expr_ == Expr::I32x4greaterThanOrEqualU ||
                expr_ == Expr::I32x4lessThanU ||
                expr_ == Expr::I32x4lessThanOrEqualU ||
                expr_ == Expr::F32x4add || expr_ == Expr::F32x4sub ||
                expr_ == Expr::F32x4mul || expr_ == Expr::F32x4div ||
                expr_ == Expr::F32x4min || expr_ == Expr::F32x4max ||
                expr_ == Expr::F32x4minNum || expr_ == Expr::F32x4maxNum ||
                expr_ == Expr::F32x4equal ||
                expr_ == Expr::F32x4notEqual ||
                expr_ == Expr::F32x4greaterThan ||
                expr_ == Expr::F32x4greaterThanOrEqual ||
                expr_ == Expr::F32x4lessThan ||
                expr_ == Expr::F32x4lessThanOrEqual ||
                expr_ == Expr::B32x4and || expr_ == Expr::B32x4or ||
                expr_ == Expr::B32x4xor);
    }
    bool isLoad() const {
        return isInitialized_ &&
               (expr_ == Expr::I32Load8S || expr_ == Expr::I32Load8U ||
                expr_ == Expr::I32Load16S || expr_ == Expr::I32Load16U ||
                expr_ == Expr::I64Load8S || expr_ == Expr::I64Load8U ||
                expr_ == Expr::I64Load16S || expr_ == Expr::I64Load16U ||
                expr_ == Expr::I64Load32S || expr_ == Expr::I64Load32U ||
                expr_ == Expr::I32Load || expr_ == Expr::I64Load ||
                expr_ == Expr::F32Load || expr_ == Expr::F64Load ||
                expr_ == Expr::I32x4load || expr_ == Expr::I32x4load1 ||
                expr_ == Expr::I32x4load2 || expr_ == Expr::I32x4load3 ||
                expr_ == Expr::F32x4load || expr_ == Expr::F32x4load1 ||
                expr_ == Expr::F32x4load2 || expr_ == Expr::F32x4load3);
    }
    bool isStore() const {
        return isInitialized_ &&
               (expr_ == Expr::I32Store8 || expr_ == Expr::I32Store16 ||
                expr_ == Expr::I64Store8 || expr_ == Expr::I64Store16 ||
                expr_ == Expr::I64Store32 || expr_ == Expr::I32Store ||
                expr_ == Expr::I64Store || expr_ == Expr::F32Store ||
                expr_ == Expr::F64Store ||
                expr_ == Expr::F32StoreF64 || expr_ == Expr::F64StoreF32 ||
                expr_ == Expr::I32x4store || expr_ == Expr::I32x4store1 ||
                expr_ == Expr::I32x4store2 || expr_ == Expr::I32x4store3 ||
                expr_ == Expr::F32x4store || expr_ == Expr::F32x4store1 ||
                expr_ == Expr::F32x4store2 || expr_ == Expr::F32x4store3);
    }
    bool isSelect()  const { return isInitialized_ && expr_ == Expr::Select; }
    bool isGetVar()  const { return isInitialized_ &&
                                    (expr_ == Expr::GetLocal ||
                                     expr_ == Expr::LoadGlobal); }
    bool isSetVar()  const { return isInitialized_ &&
                                    (expr_ == Expr::SetLocal ||
                                     expr_ == Expr::StoreGlobal); }
    bool isCall()    const { return isInitialized_ && expr_ == Expr::Call; }
    bool isCallIndirect() const { return isInitialized_ && expr_ == Expr::CallIndirect; }
    bool isCallImport() const { return isInitialized_ && expr_ == Expr::CallImport; }
    bool isReturn()  const {
        // Accept Limit, for use in decoding the end of a function after the body.
        return isInitialized_ &&
               (expr_ == Expr::Return ||
                expr_ == Expr::Limit);
    }
    bool isIf()      const { return isInitialized_ && expr_ == Expr::If; }
    bool isElse()    const { return isInitialized_ && expr_ == Expr::Else; }
    bool isEnd()     const { return isInitialized_ && expr_ == Expr::End; }
    bool isAtomicLoad() const { return isInitialized_ && expr_ == Expr::I32AtomicsLoad; }
    bool isAtomicStore() const { return isInitialized_ && expr_ == Expr::I32AtomicsStore; }
    bool isAtomicBinOp() const { return isInitialized_ && expr_ == Expr::I32AtomicsBinOp; }
    bool isAtomicCompareExchange() const {
        return isInitialized_ && expr_ == Expr::I32AtomicsCompareExchange;
    }
    bool isAtomicExchange() const {
        return isInitialized_ && expr_ == Expr::I32AtomicsExchange;
    }
    bool isExtractLane() const {
        return isInitialized_ &&
               (expr_ == Expr::I32x4extractLane ||
                expr_ == Expr::F32x4extractLane ||
                expr_ == Expr::B32x4extractLane);
    }
    bool isReplaceLane() const {
        return isInitialized_ &&
               (expr_ == Expr::I32x4replaceLane ||
                expr_ == Expr::F32x4replaceLane ||
                expr_ == Expr::B32x4replaceLane);
    }
    bool isSwizzle() const {
        return isInitialized_ &&
               (expr_ == Expr::I32x4swizzle ||
                expr_ == Expr::F32x4swizzle);
    }
    bool isShuffle() const {
        return isInitialized_ &&
               (expr_ == Expr::I32x4shuffle ||
                expr_ == Expr::F32x4shuffle);
    }
    bool isSimdSelect() const {
        return isInitialized_ &&
               (expr_ == Expr::I32x4select ||
                expr_ == Expr::F32x4select);
    }
#endif

#ifdef DEBUG
    bool setInitialized() {
        isInitialized_ = true;
        return true;
    }
#else
    bool setInitialized() { return true; }
#endif

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
        *value = tv.value();
        return true;
    }

    // Read the value stack entry at depth |index|.
    bool peek(uint32_t index, TypeAndValue<Value>* tv) {
        if (Validate && valueStack_.length() - controlStack_.back().valueStackStart() <= index)
            return fail("peeking at value from outside block");
        *tv = valueStack_[valueStack_.length() - index];
        return true;
    }

  public:
    ExprIter(Policy policy, Decoder& decoder)
      : Policy(policy), d_(decoder)
    {
#ifdef DEBUG
        expr_ = Expr::Limit;
        isInitialized_ = false;
#endif
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
    MOZ_MUST_USE bool readFunctionEnd(ExprType ret);
    MOZ_MUST_USE bool readReturn();
    MOZ_MUST_USE bool readBlock();
    MOZ_MUST_USE bool readLoop();
    MOZ_MUST_USE bool readIf();
    MOZ_MUST_USE bool readElse();
    MOZ_MUST_USE bool readEnd();
    MOZ_MUST_USE bool readBr();
    MOZ_MUST_USE bool readBrIf();
    MOZ_MUST_USE bool readBrTable();
    MOZ_MUST_USE bool readBrTableEntry(ExprType type, uint32_t* depth);
    MOZ_MUST_USE bool readUnreachable();
    MOZ_MUST_USE bool readUnary(ValType operandType);
    MOZ_MUST_USE bool readConversion(ValType operandType, ValType resultType);
    MOZ_MUST_USE bool readBinary(ValType operandType);
    MOZ_MUST_USE bool readComparison(ValType operandType);
    MOZ_MUST_USE bool readLoad(ValType resultType, uint32_t byteSize);
    MOZ_MUST_USE bool readStore(ValType resultType, uint32_t byteSize);
    MOZ_MUST_USE bool readTrivial();
    MOZ_MUST_USE bool readSelect();
    MOZ_MUST_USE bool readGetLocal(const ValTypeVector& locals);
    MOZ_MUST_USE bool readSetLocal(const ValTypeVector& locals);
    MOZ_MUST_USE bool readGetGlobal(const GlobalDescVector& globals);
    MOZ_MUST_USE bool readSetGlobal(const GlobalDescVector& globals);
    MOZ_MUST_USE bool readI32Const();
    MOZ_MUST_USE bool readI64Const();
    MOZ_MUST_USE bool readF32Const();
    MOZ_MUST_USE bool readF64Const();
    MOZ_MUST_USE bool readI32x4Const();
    MOZ_MUST_USE bool readF32x4Const();
    MOZ_MUST_USE bool readB32x4Const();
    MOZ_MUST_USE bool readCall();
    MOZ_MUST_USE bool readCallIndirect();
    MOZ_MUST_USE bool readCallImport();
    MOZ_MUST_USE bool readCallArg(ValType type, uint32_t numArgs, uint32_t argIndex, Value* arg);
    MOZ_MUST_USE bool readCallArgsEnd(uint32_t numArgs);
    MOZ_MUST_USE bool readCallIndirectCallee(Value* callee);
    MOZ_MUST_USE bool readCallReturn(ExprType ret);
    MOZ_MUST_USE bool readAtomicLoad();
    MOZ_MUST_USE bool readAtomicStore();
    MOZ_MUST_USE bool readAtomicBinOp();
    MOZ_MUST_USE bool readAtomicCompareExchange();
    MOZ_MUST_USE bool readAtomicExchange();
    MOZ_MUST_USE bool readSimdComparison(ValType simdType);
    MOZ_MUST_USE bool readSimdShiftByScalar(ValType simdType);
    MOZ_MUST_USE bool readSimdBooleanReduction(ValType simdType);
    MOZ_MUST_USE bool readExtractLane(ValType simdType);
    MOZ_MUST_USE bool readReplaceLane(ValType simdType);
    MOZ_MUST_USE bool readSplat(ValType simdType);
    MOZ_MUST_USE bool readSwizzle(ValType simdType);
    MOZ_MUST_USE bool readShuffle(ValType simdType);
    MOZ_MUST_USE bool readSimdSelect(ValType simdType);
    MOZ_MUST_USE bool readSimdCtor();
    MOZ_MUST_USE bool readSimdCtorArg(ValType elementType, uint32_t numElements, uint32_t argIndex, Value* arg);
    MOZ_MUST_USE bool readSimdCtorArgsEnd(uint32_t numElements);
    MOZ_MUST_USE bool readSimdCtorReturn(ValType simdType);

    // ------------------------------------------------------------------------
    // Translation interface. These methods provide the information obtained
    // through decoding.

    int32_t                     i32()     const { MOZ_ASSERT(isI32());     return u_.i32; }
    int64_t                     i64()     const { MOZ_ASSERT(isI64());     return u_.i64; }
    float                       f32()     const { MOZ_ASSERT(isF32());     return u_.f32; }
    double                      f64()     const { MOZ_ASSERT(isF64());     return u_.f64; }
    const I32x4&                i32x4()   const { MOZ_ASSERT(isI32x4());   return u_.i32x4; }
    const F32x4&                f32x4()   const { MOZ_ASSERT(isF32x4());   return u_.f32x4; }
    const BrRecord<Value>&      br()      const { MOZ_ASSERT(isBr());      return u_.br; }
    const BrIfRecord<Value>&    brIf()    const { MOZ_ASSERT(isBrIf());    return u_.brIf; }
    const BrTableRecord<Value>& brTable() const { MOZ_ASSERT(isBrTable()); return u_.brTable; }
    const UnaryRecord<Value>&   unary()   const { MOZ_ASSERT(isUnary());   return u_.unary; }
    const BinaryRecord<Value>&  binary()  const { MOZ_ASSERT(isBinary());  return u_.binary; }
    const LoadRecord<Value>&    load()    const { MOZ_ASSERT(isLoad());    return u_.load; }
    const StoreRecord<Value>&   store()   const { MOZ_ASSERT(isStore());   return u_.store; }
    const SelectRecord<Value>&  select()  const { MOZ_ASSERT(isSelect());  return u_.select; }
    const GetVarRecord&         getVar()  const { MOZ_ASSERT(isGetVar());  return u_.getVar; }
    const SetVarRecord<Value>&  setVar()  const { MOZ_ASSERT(isSetVar());  return u_.setVar; }
    const CallRecord&           call()    const { MOZ_ASSERT(isCall());    return u_.call; }
    const CallIndirectRecord<Value>& callIndirect() const
    { MOZ_ASSERT(isCallIndirect());          return u_.callIndirect; }
    const CallImportRecord&     callImport() const
    { MOZ_ASSERT(isCallImport());            return u_.callImport; }
    const ReturnRecord<Value>&  return_() const { MOZ_ASSERT(isReturn());  return u_.return_; }
    const IfRecord<Value>&      if_()     const { MOZ_ASSERT(isIf());      return u_.if_; }
    const ElseRecord<Value>&    else_()   const { MOZ_ASSERT(isElse());    return u_.else_; }
    const EndRecord<Value>&     end()     const { MOZ_ASSERT(isEnd());     return u_.end; }
    const AtomicLoadRecord<Value>& atomicLoad() const
    { MOZ_ASSERT(isAtomicLoad());            return u_.atomicLoad; }
    const AtomicStoreRecord<Value>& atomicStore() const
    { MOZ_ASSERT(isAtomicStore());           return u_.atomicStore; }
    const AtomicBinOpRecord<Value>& atomicBinOp() const
    { MOZ_ASSERT(isAtomicBinOp());           return u_.atomicBinOp; }
    const AtomicCompareExchangeRecord<Value>& atomicCompareExchange() const
    { MOZ_ASSERT(isAtomicCompareExchange()); return u_.atomicCompareExchange; }
    const AtomicExchangeRecord<Value>& atomicExchange() const
    { MOZ_ASSERT(isAtomicExchange());        return u_.atomicExchange; }
    const ExtractLaneRecord<Value>& extractLane() const
    { MOZ_ASSERT(isExtractLane()); return u_.extractLane; }
    const ReplaceLaneRecord<Value>& replaceLane() const
    { MOZ_ASSERT(isReplaceLane()); return u_.replaceLane; }
    const SwizzleRecord<Value>& swizzle() const
    { MOZ_ASSERT(isSwizzle()); return u_.swizzle; }
    const ShuffleRecord<Value>& shuffle() const
    { MOZ_ASSERT(isShuffle()); return u_.shuffle; }
    const SimdSelectRecord<Value>& simdSelect() const
    { MOZ_ASSERT(isSimdSelect()); return u_.simdSelect; }

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
inline MOZ_MUST_USE bool
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

#ifdef DEBUG
    expr_ = *expr;
    mozWritePoison(&u_, sizeof(u_));
    isInitialized_ = false;
#endif

    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readFunctionStart()
{
    MOZ_ASSERT(valueStack_.empty());
    MOZ_ASSERT(controlStack_.empty());
    MOZ_ASSERT(expr_ == Expr::Limit);

    return pushControl(LabelKind::Block);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readFunctionEnd(ExprType ret)
{
#ifdef DEBUG
    expr_ = Expr::Limit;
    mozWritePoison(&u_, sizeof(u_));
#endif

    if (Validate) {
        MOZ_ASSERT(controlStack_.length() > 0);
        if (controlStack_.length() != 1)
            return fail("unbalanced function body control flow");
    } else {
        MOZ_ASSERT(controlStack_.length() == 1);
    }

    ExprType type;
    Value value;
    LabelKind kind;
    if (!popControlAfterCheck(&kind, &type, &value))
        return false;

    MOZ_ASSERT(kind == LabelKind::Block);
    MOZ_ASSERT(valueStack_.length() == 1);

    if (!IsVoid(ret)) {
        if (!checkType(type, ret))
            return false;
    }

    u_.return_ = ReturnRecord<Value>(value);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readReturn()
{
    ControlStackEntry<ControlItem>& controlItem = controlStack_[0];
    MOZ_ASSERT(controlItem.kind() == LabelKind::Block);

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

    u_.return_ = ReturnRecord<Value>(tv.value());
    return setInitialized();
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
    return pushControl(LabelKind::Block);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLoop()
{
    return pushControl(LabelKind::Block) &&
           pushControl(LabelKind::Loop);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readIf()
{
    Value condition;
    if (!popWithType(ExprType::I32, &condition))
        return false;

    u_.if_ = IfRecord<Value>(condition);

    return setInitialized() &&
           pushControl(LabelKind::Then);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readElse()
{
    ExprType thenType;
    Value thenValue;
    LabelKind kind;
    if (!popControl(&kind, &thenType, &thenValue))
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
    controlItem.type() = thenType;

    u_.else_ = ElseRecord<Value>(thenType, thenValue);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readEnd()
{
    ExprType type;
    Value value;
    LabelKind kind;
    if (!popControl(&kind, &type, &value))
        return false;

    switch (kind) {
      case LabelKind::Block:
        break;
      case LabelKind::Loop: {
        // Note: Propose a spec change: loops don't implicitly have an end label.

        setResult(value);

        LabelKind blockKind;
        if (!popControl(&blockKind, &type, &value))
            return false;

        MOZ_ASSERT(blockKind == LabelKind::Block);
        break;
      }
      case LabelKind::Then:
        valueStack_.back() = TypeAndValue<Value>(ExprType::Void);
        type = ExprType::Void;
        break;
      case LabelKind::Else:
        break;
    }

    u_.end = EndRecord<Value>(kind, type, value);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBr()
{
    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read br arity");
    if (arity > 1)
        return fail("br arity too big");

    uint32_t relativeDepth;
    if (!readVarU32(&relativeDepth))
        return fail("unable to read br depth");

    TypeAndValue<Value> tv;
    if (arity) {
        if (!pop(&tv))
            return false;
    } else {
        tv = TypeAndValue<Value>(ExprType::Void);
    }

    if (!checkBranch(relativeDepth, tv.type()))
        return false;

    if (!push(AnyType))
        return false;

    u_.br = BrRecord<Value>(relativeDepth, tv.type(), tv.value());
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrIf()
{
    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read br_if arity");
    if (arity > 1)
        return fail("br_if arity too big");

    uint32_t relativeDepth;
    if (!readVarU32(&relativeDepth))
        return fail("unable to read br_if depth");

    Value condition;
    if (!popWithType(ExprType::I32, &condition))
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

    if (!checkBranch(relativeDepth, tv.type()))
        return false;

    u_.brIf = BrIfRecord<Value>(relativeDepth, tv.type(), tv.value(), condition);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTable()
{
    Value index;
    if (!popWithType(ExprType::I32, &index))
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

    uint32_t tableLength;
    if (!readVarU32(&tableLength))
        return fail("unable to read br_table table length");

    u_.brTable = BrTableRecord<Value>(tableLength, tv.type(), tv.value(), index);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBrTableEntry(ExprType type, uint32_t* depth)
{
    return readFixedU32(depth) &&
           checkBranch(*depth, type);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnreachable()
{
    return push(AnyType);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readUnary(ValType operandType)
{
    Value op;
    if (!popWithType(ToExprType(operandType), &op))
        return false;

    infalliblePush(ToExprType(operandType));

    u_.unary = UnaryRecord<Value>(op);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readConversion(ValType operandType, ValType resultType)
{
    Value op;
    if (!popWithType(ToExprType(operandType), &op))
        return false;

    infalliblePush(ToExprType(resultType));

    u_.unary = UnaryRecord<Value>(op);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readBinary(ValType operandType)
{
    Value rhs;
    if (!popWithType(ToExprType(operandType), &rhs))
        return false;

    Value lhs;
    if (!popWithType(ToExprType(operandType), &lhs))
        return false;

    infalliblePush(ToExprType(operandType));

    u_.binary = BinaryRecord<Value>(lhs, rhs);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readComparison(ValType operandType)
{
    Value rhs;
    if (!popWithType(ToExprType(operandType), &rhs))
        return false;

    Value lhs;
    if (!popWithType(ToExprType(operandType), &lhs))
        return false;

    infalliblePush(ExprType::I32);

    u_.binary = BinaryRecord<Value>(lhs, rhs);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLinearMemoryAddress(uint32_t byteSize, LinearMemoryAddress<Value>* addr)
{
    Value base;
    if (!popWithType(ExprType::I32, &base))
        return false;

    uint8_t alignLog2;
    if (!readFixedU8(&alignLog2))
        return fail("unable to read load alignment");
    if (Validate && (alignLog2 >= 32 || (uint32_t(1) << alignLog2) > byteSize))
        return fail("greater than natural alignment");
    uint32_t align = uint32_t(1) << alignLog2;

    uint32_t offset;
    if (!readVarU32(&offset))
        return fail("unable to read load offset");

    *addr = LinearMemoryAddress<Value>(base, offset, align);
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readLoad(ValType resultType, uint32_t byteSize)
{
    if (!readLinearMemoryAddress(byteSize, &u_.load.addr))
        return false;

    infalliblePush(ToExprType(resultType));
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readStore(ValType resultType, uint32_t byteSize)
{
    Value value;
    if (!popWithType(ToExprType(resultType), &value))
        return false;

    if (!readLinearMemoryAddress(byteSize, &u_.store.addr))
        return false;

    infalliblePush(TypeAndValue<Value>(ToExprType(resultType), value));

    u_.store.value = value;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readTrivial()
{
    return push(ExprType::Void);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSelect()
{
    Value condition;
    if (!popWithType(ExprType::I32, &condition))
        return false;

    TypeAndValue<Value> falseValue;
    if (!pop(&falseValue))
        return false;

    TypeAndValue<Value> trueValue;
    if (!pop(&trueValue))
        return false;

    ExprType resultType = Unify(trueValue.type(), falseValue.type());
    infalliblePush(resultType);

    u_.select = SelectRecord<Value>(resultType, trueValue.value(), falseValue.value(),
                                    condition);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readGetLocal(const ValTypeVector& locals)
{
    uint32_t id;
    if (!readVarU32(&id))
        return false;

    if (Validate && id >= locals.length())
        return fail("get_local index out of range");

    if (!push(ToExprType(locals[id])))
        return false;

    u_.getVar = GetVarRecord(id);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetLocal(const ValTypeVector& locals)
{
    uint32_t id;
    if (!readVarU32(&id))
        return false;

    if (Validate && id >= locals.length())
        return fail("set_local index out of range");

    Value value;
    if (!topWithType(ToExprType(locals[id]), &value))
        return false;

    u_.setVar = SetVarRecord<Value>(id, value);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readGetGlobal(const GlobalDescVector& globals)
{
    uint32_t id;
    if (!readVarU32(&id))
        return false;

    if (!push(ToExprType(globals[id].type)))
        return false;

    u_.getVar = GetVarRecord(id);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSetGlobal(const GlobalDescVector& globals)
{
    uint32_t id;
    if (!readVarU32(&id))
        return false;

    Value value;
    if (!topWithType(ToExprType(globals[id].type), &value))
        return false;

    u_.setVar = SetVarRecord<Value>(id, value);
    return setInitialized();
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readI32Const()
{
    return readVarS32(&u_.i32) &&
           setInitialized() &&
           push(ExprType::I32);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readI64Const()
{
    return readVarS64(&u_.i64) &&
           setInitialized() &&
           push(ExprType::I64);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readF32Const()
{
    if (!readFixedF32(&u_.f32))
        return false;

    if (Validate && mozilla::IsNaN(u_.f32)) {
        const float jsNaN = (float)JS::GenericNaN();
        if (memcmp(&u_.f32, &jsNaN, sizeof(u_.f32)) != 0)
            return notYetImplemented("NaN literals with custom payloads");
    }

    return setInitialized() &&
           push(ExprType::F32);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readF64Const()
{
    if (!readFixedF64(&u_.f64))
       return false;

    if (Validate && mozilla::IsNaN(u_.f64)) {
        const double jsNaN = JS::GenericNaN();
        if (memcmp(&u_.f64, &jsNaN, sizeof(u_.f64)) != 0)
            return notYetImplemented("NaN literals with custom payloads");
    }

    return setInitialized() &&
           push(ExprType::F64);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readI32x4Const()
{
    return readFixedI32x4(&u_.i32x4) &&
           setInitialized() &&
           push(ExprType::I32x4);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readF32x4Const()
{
    return readFixedF32x4(&u_.f32x4) &&
           setInitialized() &&
           push(ExprType::F32x4);
}

template <typename Policy>
inline MOZ_MUST_USE bool
ExprIter<Policy>::readB32x4Const()
{
    return readFixedI32x4(&u_.i32x4) &&
           setInitialized() &&
           push(ExprType::B32x4);
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCall()
{
    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read call arity");

    uint32_t funcIndex;
    if (!readVarU32(&funcIndex))
        return fail("unable to read call function index");

    u_.call = CallRecord(arity, funcIndex);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallIndirect()
{
    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read call_indirect arity");

    uint32_t sigIndex;
    if (!readVarU32(&sigIndex))
        return fail("unable to read call_indirect signature index");

    u_.callIndirect = CallIndirectRecord<Value>(arity, sigIndex);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readCallImport()
{
    uint32_t arity;
    if (!readVarU32(&arity))
        return fail("unable to read call_import arity");

    uint32_t importIndex;
    if (!readVarU32(&importIndex))
        return fail("unable to read call_import import index");

    u_.callImport = CallImportRecord(arity, importIndex);
    return setInitialized();
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
ExprIter<Policy>::readAtomicLoad()
{
    Scalar::Type viewType;
    if (!readAtomicViewType(&viewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(viewType);
    if (!readLinearMemoryAddress(byteSize, &u_.atomicLoad.addr))
        return false;

    infalliblePush(ExprType::I32);

    u_.atomicLoad.viewType = viewType;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicStore()
{
    Scalar::Type viewType;
    if (!readAtomicViewType(&viewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(viewType);
    if (!readLinearMemoryAddress(byteSize, &u_.atomicStore.addr))
        return false;

    Value value;
    if (!popWithType(ExprType::I32, &value))
        return false;

    infalliblePush(ExprType::I32);

    u_.atomicStore.viewType = viewType;
    u_.atomicStore.value = value;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicBinOp()
{
    Scalar::Type viewType;
    if (!readAtomicViewType(&viewType))
        return false;

    jit::AtomicOp op;
    if (!readAtomicBinOpOp(&op))
        return false;

    uint32_t byteSize = Scalar::byteSize(viewType);
    if (!readLinearMemoryAddress(byteSize, &u_.atomicStore.addr))
        return false;

    Value value;
    if (!popWithType(ExprType::I32, &value))
        return false;

    infalliblePush(ExprType::I32);

    u_.atomicBinOp.viewType = viewType;
    u_.atomicBinOp.op = op;
    u_.atomicBinOp.value = value;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicCompareExchange()
{
    Scalar::Type viewType;
    if (!readAtomicViewType(&viewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(viewType);
    if (!readLinearMemoryAddress(byteSize, &u_.atomicStore.addr))
        return false;

    Value new_;
    if (!popWithType(ExprType::I32, &new_))
        return false;

    Value old;
    if (!popWithType(ExprType::I32, &old))
        return false;

    infalliblePush(ExprType::I32);

    u_.atomicCompareExchange.viewType = viewType;
    u_.atomicCompareExchange.oldValue = old;
    u_.atomicCompareExchange.newValue = new_;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readAtomicExchange()
{
    Scalar::Type viewType;
    if (!readAtomicViewType(&viewType))
        return false;

    uint32_t byteSize = Scalar::byteSize(viewType);
    if (!readLinearMemoryAddress(byteSize, &u_.atomicStore.addr))
        return false;

    Value value;
    if (!popWithType(ExprType::I32, &value))
        return false;

    infalliblePush(ExprType::I32);

    u_.atomicExchange.viewType = viewType;
    u_.atomicExchange.value = value;
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdComparison(ValType simdType)
{
    Value rhs;
    if (!popWithType(ToExprType(simdType), &rhs))
        return false;

    Value lhs;
    if (!popWithType(ToExprType(simdType), &lhs))
        return false;

    infalliblePush(ToExprType(SimdBoolType(simdType)));

    u_.binary = BinaryRecord<Value>(lhs, rhs);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdShiftByScalar(ValType simdType)
{
    Value count;
    if (!popWithType(ExprType::I32, &count))
        return false;

    Value vector;
    if (!popWithType(ToExprType(simdType), &vector))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.binary = BinaryRecord<Value>(vector, count);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdBooleanReduction(ValType simdType)
{
    Value op;
    if (!popWithType(ToExprType(simdType), &op))
        return false;

    infalliblePush(ExprType::I32);

    u_.unary = UnaryRecord<Value>(op);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readExtractLane(ValType simdType)
{
    uint32_t lane;
    if (!readVarU32(&lane))
        return false;
    if (Validate && lane >= NumSimdElements(simdType))
        return fail("simd lane out of bounds for simd type");

    Value value;
    if (!popWithType(ToExprType(simdType), &value))
        return false;

    infalliblePush(ToExprType(SimdElementType(simdType)));

    u_.extractLane = ExtractLaneRecord<Value>(jit::SimdLane(lane), value);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readReplaceLane(ValType simdType)
{
    uint32_t lane;
    if (!readVarU32(&lane))
        return false;
    if (Validate && lane >= NumSimdElements(simdType))
        return fail("simd lane out of bounds for simd type");

    Value scalar;
    if (!popWithType(ToExprType(SimdElementType(simdType)), &scalar))
        return false;

    Value vector;
    if (!popWithType(ToExprType(simdType), &vector))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.replaceLane = ReplaceLaneRecord<Value>(jit::SimdLane(lane), vector, scalar);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSplat(ValType simdType)
{
    Value op;
    if (!popWithType(ToExprType(SimdElementType(simdType)), &op))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.unary = UnaryRecord<Value>(op);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSwizzle(ValType simdType)
{
    uint8_t lanes[4];
    uint32_t numSimdLanes = NumSimdElements(simdType);
    MOZ_ASSERT(numSimdLanes <= mozilla::ArrayLength(lanes));
    for (uint32_t i = 0; i < numSimdLanes; ++i) {
        if (!readFixedU8(&lanes[i]))
            return false;
        if (Validate && lanes[i] >= numSimdLanes)
            return fail("swizzle index out of bounds");
    }

    Value vector;
    if (!popWithType(ToExprType(simdType), &vector))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.swizzle = SwizzleRecord<Value>(lanes, vector);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readShuffle(ValType simdType)
{
    uint8_t lanes[4];
    uint32_t numSimdLanes = NumSimdElements(simdType);
    MOZ_ASSERT(numSimdLanes <= mozilla::ArrayLength(lanes));
    for (uint32_t i = 0; i < numSimdLanes; ++i) {
        if (!readFixedU8(&lanes[i]))
            return false;
        if (Validate && lanes[i] >= numSimdLanes * 2)
            return fail("shuffle index out of bounds");
    }

    Value rhs;
    if (!popWithType(ToExprType(simdType), &rhs))
        return false;

    Value lhs;
    if (!popWithType(ToExprType(simdType), &lhs))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.shuffle = ShuffleRecord<Value>(lanes, lhs, rhs);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdSelect(ValType simdType)
{
    Value falseValue;
    if (!popWithType(ToExprType(simdType), &falseValue))
        return false;
    Value trueValue;
    if (!popWithType(ToExprType(simdType), &trueValue))
        return false;
    Value condition;
    if (!popWithType(ToExprType(SimdBoolType(simdType)), &condition))
        return false;

    infalliblePush(ToExprType(simdType));

    u_.simdSelect = SimdSelectRecord<Value>(trueValue, falseValue, condition);
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtor()
{
    return setInitialized();
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtorArg(ValType elementType, uint32_t numElements, uint32_t index,
                                      Value* arg)
{
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
    MOZ_ASSERT(numElements <= valueStack_.length());
    valueStack_.shrinkBy(numElements);
    return true;
}

template <typename Policy>
inline bool
ExprIter<Policy>::readSimdCtorReturn(ValType simdType)
{
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
