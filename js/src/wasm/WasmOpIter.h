/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#ifndef wasm_op_iter_h
#define wasm_op_iter_h

#include "mozilla/Pair.h"
#include "mozilla/Poison.h"

#include "jit/AtomicOp.h"
#include "js/Printf.h"
#include "wasm/WasmValidate.h"

namespace js {
namespace wasm {

// The kind of a control-flow stack item.
enum class LabelKind : uint8_t { Body, Block, Loop, Then, Else };

// The type of values on the operand stack during validation. The Any type
// represents the type of a value produced by an unconditional branch.

class StackType {
  PackedTypeCode tc_;

#ifdef DEBUG
  bool isValidCode() {
    switch (UnpackTypeCodeType(tc_)) {
      case TypeCode::I32:
      case TypeCode::I64:
      case TypeCode::F32:
      case TypeCode::F64:
      case TypeCode::AnyRef:
      case TypeCode::FuncRef:
      case TypeCode::Ref:
      case TypeCode::NullRef:
      case TypeCode::Limit:
        return true;
      default:
        return false;
    }
  }
#endif

 public:
  enum Code {
    I32 = uint8_t(ValType::I32),
    I64 = uint8_t(ValType::I64),
    F32 = uint8_t(ValType::F32),
    F64 = uint8_t(ValType::F64),

    AnyRef = uint8_t(ValType::AnyRef),
    FuncRef = uint8_t(ValType::FuncRef),
    Ref = uint8_t(ValType::Ref),
    NullRef = uint8_t(ValType::NullRef),

    TVar = uint8_t(TypeCode::Limit),
  };

  StackType() : tc_(InvalidPackedTypeCode()) {}

  MOZ_IMPLICIT StackType(Code c) : tc_(PackTypeCode(TypeCode(c))) {
    MOZ_ASSERT(isValidCode());
  }

  explicit StackType(const ValType& t) : tc_(t.packed()) {}

  PackedTypeCode packed() const { return tc_; }

  Code code() const { return Code(UnpackTypeCodeType(tc_)); }

  uint32_t refTypeIndex() const { return UnpackTypeCodeIndex(tc_); }
  bool isRef() const { return UnpackTypeCodeType(tc_) == TypeCode::Ref; }

  bool isReference() const { return IsReferenceType(tc_); }

  bool operator==(const StackType& that) const { return tc_ == that.tc_; }
  bool operator!=(const StackType& that) const { return tc_ != that.tc_; }
  bool operator==(Code that) const {
    MOZ_ASSERT(that != Code::Ref);
    return code() == that;
  }
  bool operator!=(Code that) const { return !(*this == that); }
};

static inline ValType NonTVarToValType(StackType type) {
  MOZ_ASSERT(type != StackType::TVar);
  return ValType(type.packed());
}

#ifdef DEBUG
// Families of opcodes that share a signature and validation logic.
enum class OpKind {
  Block,
  Loop,
  Unreachable,
  Drop,
  I32,
  I64,
  F32,
  F64,
  Br,
  BrIf,
  BrTable,
  Nop,
  Unary,
  Binary,
  Comparison,
  Conversion,
  Load,
  Store,
  TeeStore,
  MemorySize,
  MemoryGrow,
  Select,
  GetLocal,
  SetLocal,
  TeeLocal,
  GetGlobal,
  SetGlobal,
  TeeGlobal,
  Call,
  CallIndirect,
  OldCallDirect,
  OldCallIndirect,
  Return,
  If,
  Else,
  End,
  Wait,
  Wake,
  Fence,
  AtomicLoad,
  AtomicStore,
  AtomicBinOp,
  AtomicCompareExchange,
  OldAtomicLoad,
  OldAtomicStore,
  OldAtomicBinOp,
  OldAtomicCompareExchange,
  OldAtomicExchange,
  ExtractLane,
  ReplaceLane,
  Swizzle,
  Shuffle,
  Splat,
  MemOrTableCopy,
  DataOrElemDrop,
  MemFill,
  MemOrTableInit,
  TableFill,
  TableGet,
  TableGrow,
  TableSet,
  TableSize,
  RefNull,
  RefFunc,
  StructNew,
  StructGet,
  StructSet,
  StructNarrow,
};

// Return the OpKind for a given Op. This is used for sanity-checking that
// API users use the correct read function for a given Op.
OpKind Classify(OpBytes op);
#endif

// Common fields for linear memory access.
template <typename Value>
struct LinearMemoryAddress {
  Value base;
  uint32_t offset;
  uint32_t align;

  LinearMemoryAddress() : offset(0), align(0) {}
  LinearMemoryAddress(Value base, uint32_t offset, uint32_t align)
      : base(base), offset(offset), align(align) {}
};

template <typename ControlItem>
class ControlStackEntry {
  // Use a Pair to optimize away empty ControlItem.
  mozilla::Pair<LabelKind, ControlItem> kindAndItem_;
  bool polymorphicBase_;
  ExprType type_;
  size_t valueStackStart_;

 public:
  ControlStackEntry(LabelKind kind, ExprType type, size_t valueStackStart)
      : kindAndItem_(kind, ControlItem()),
        polymorphicBase_(false),
        type_(type),
        valueStackStart_(valueStackStart) {
    MOZ_ASSERT(type != ExprType::Limit);
  }

  LabelKind kind() const { return kindAndItem_.first(); }
  ExprType resultType() const { return type_; }
  ExprType branchTargetType() const {
    return kind() == LabelKind::Loop ? ExprType::Void : type_;
  }
  size_t valueStackStart() const { return valueStackStart_; }
  ControlItem& controlItem() { return kindAndItem_.second(); }
  void setPolymorphicBase() { polymorphicBase_ = true; }
  bool polymorphicBase() const { return polymorphicBase_; }

  void switchToElse() {
    MOZ_ASSERT(kind() == LabelKind::Then);
    kindAndItem_.first() = LabelKind::Else;
    polymorphicBase_ = false;
  }
};

template <typename Value>
class TypeAndValue {
  // Use a Pair to optimize away empty Value.
  mozilla::Pair<StackType, Value> tv_;

 public:
  TypeAndValue() : tv_(StackType::TVar, Value()) {}
  explicit TypeAndValue(StackType type) : tv_(type, Value()) {}
  explicit TypeAndValue(ValType type) : tv_(StackType(type), Value()) {}
  TypeAndValue(StackType type, Value value) : tv_(type, value) {}
  TypeAndValue(ValType type, Value value) : tv_(StackType(type), value) {}
  StackType type() const { return tv_.first(); }
  StackType& typeRef() { return tv_.first(); }
  Value value() const { return tv_.second(); }
  void setValue(Value value) { tv_.second() = value; }
};

// An iterator over the bytes of a function body. It performs validation
// and unpacks the data into a usable form.
//
// The MOZ_STACK_CLASS attribute here is because of the use of DebugOnly.
// There's otherwise nothing inherent in this class which would require
// it to be used on the stack.
template <typename Policy>
class MOZ_STACK_CLASS OpIter : private Policy {
  typedef typename Policy::Value Value;
  typedef typename Policy::ControlItem ControlItem;

  Decoder& d_;
  const ModuleEnvironment& env_;

  Vector<TypeAndValue<Value>, 8, SystemAllocPolicy> valueStack_;
  Vector<ControlStackEntry<ControlItem>, 8, SystemAllocPolicy> controlStack_;

#ifdef DEBUG
  OpBytes op_;
#endif
  size_t offsetOfLastReadOp_;

  MOZ_MUST_USE bool readFixedU8(uint8_t* out) { return d_.readFixedU8(out); }
  MOZ_MUST_USE bool readFixedU32(uint32_t* out) { return d_.readFixedU32(out); }
  MOZ_MUST_USE bool readVarS32(int32_t* out) { return d_.readVarS32(out); }
  MOZ_MUST_USE bool readVarU32(uint32_t* out) { return d_.readVarU32(out); }
  MOZ_MUST_USE bool readVarS64(int64_t* out) { return d_.readVarS64(out); }
  MOZ_MUST_USE bool readVarU64(uint64_t* out) { return d_.readVarU64(out); }
  MOZ_MUST_USE bool readFixedF32(float* out) { return d_.readFixedF32(out); }
  MOZ_MUST_USE bool readFixedF64(double* out) { return d_.readFixedF64(out); }

  MOZ_MUST_USE bool readMemOrTableIndex(bool isMem, uint32_t* index);
  MOZ_MUST_USE bool readLinearMemoryAddress(uint32_t byteSize,
                                            LinearMemoryAddress<Value>* addr);
  MOZ_MUST_USE bool readLinearMemoryAddressAligned(
      uint32_t byteSize, LinearMemoryAddress<Value>* addr);
  MOZ_MUST_USE bool readBlockType(ExprType* expr);
  MOZ_MUST_USE bool readStructTypeIndex(uint32_t* typeIndex);
  MOZ_MUST_USE bool readFieldIndex(uint32_t* fieldIndex,
                                   const StructType& structType);

  MOZ_MUST_USE bool popCallArgs(const ValTypeVector& expectedTypes,
                                Vector<Value, 8, SystemAllocPolicy>* values);

  MOZ_MUST_USE bool failEmptyStack();
  MOZ_MUST_USE bool popStackType(StackType* type, Value* value);
  MOZ_MUST_USE bool popWithType(ValType valType, Value* value);
  MOZ_MUST_USE bool popWithType(ExprType expectedType, Value* value);
  MOZ_MUST_USE bool topWithType(ExprType expectedType, Value* value);
  MOZ_MUST_USE bool topWithType(ValType valType, Value* value);

  MOZ_MUST_USE bool pushControl(LabelKind kind, ExprType type);
  MOZ_MUST_USE bool checkStackAtEndOfBlock(ExprType* type, Value* value);
  MOZ_MUST_USE bool getControl(uint32_t relativeDepth,
                               ControlStackEntry<ControlItem>** controlEntry);
  MOZ_MUST_USE bool checkBranchValue(uint32_t relativeDepth, ExprType* type,
                                     Value* value);
  MOZ_MUST_USE bool checkBrTableEntry(uint32_t* relativeDepth,
                                      ExprType* branchValueType,
                                      Value* branchValue);

  MOZ_MUST_USE bool push(StackType t) { return valueStack_.emplaceBack(t); }
  MOZ_MUST_USE bool push(ValType t) { return valueStack_.emplaceBack(t); }
  MOZ_MUST_USE bool push(ExprType t) {
    return IsVoid(t) || push(NonVoidToValType(t));
  }
  MOZ_MUST_USE bool push(TypeAndValue<Value> tv) {
    return valueStack_.append(tv);
  }
  void infalliblePush(StackType t) { valueStack_.infallibleEmplaceBack(t); }
  void infalliblePush(ValType t) {
    valueStack_.infallibleEmplaceBack(StackType(t));
  }
  void infalliblePush(TypeAndValue<Value> tv) {
    valueStack_.infallibleAppend(tv);
  }

  void afterUnconditionalBranch() {
    valueStack_.shrinkTo(controlStack_.back().valueStackStart());
    controlStack_.back().setPolymorphicBase();
  }

  inline bool Join(StackType one, StackType two, StackType* result) const;
  inline bool checkIsSubtypeOf(ValType lhs, ValType rhs);

 public:
  typedef Vector<Value, 8, SystemAllocPolicy> ValueVector;

#ifdef DEBUG
  explicit OpIter(const ModuleEnvironment& env, Decoder& decoder)
      : d_(decoder),
        env_(env),
        op_(OpBytes(Op::Limit)),
        offsetOfLastReadOp_(0) {}
#else
  explicit OpIter(const ModuleEnvironment& env, Decoder& decoder)
      : d_(decoder), env_(env), offsetOfLastReadOp_(0) {}
#endif

  // Return the decoding byte offset.
  uint32_t currentOffset() const { return d_.currentOffset(); }

  // Return the offset within the entire module of the last-read op.
  size_t lastOpcodeOffset() const {
    return offsetOfLastReadOp_ ? offsetOfLastReadOp_ : d_.currentOffset();
  }

  // Return a BytecodeOffset describing where the current op should be reported
  // to trap/call.
  BytecodeOffset bytecodeOffset() const {
    return BytecodeOffset(lastOpcodeOffset());
  }

  // Test whether the iterator has reached the end of the buffer.
  bool done() const { return d_.done(); }

  // Return a pointer to the end of the buffer being decoded by this iterator.
  const uint8_t* end() const { return d_.end(); }

  // Report a general failure.
  MOZ_MUST_USE bool fail(const char* msg) MOZ_COLD;

  // Report a general failure with a context
  MOZ_MUST_USE bool fail_ctx(const char* fmt, const char* context) MOZ_COLD;

  // Report an unrecognized opcode.
  MOZ_MUST_USE bool unrecognizedOpcode(const OpBytes* expr) MOZ_COLD;

  // Return whether the innermost block has a polymorphic base of its stack.
  // Ideally this accessor would be removed; consider using something else.
  bool currentBlockHasPolymorphicBase() const {
    return !controlStack_.empty() && controlStack_.back().polymorphicBase();
  }

  // ------------------------------------------------------------------------
  // Decoding and validation interface.

  MOZ_MUST_USE bool readOp(OpBytes* op);
  MOZ_MUST_USE bool readFunctionStart(ExprType ret);
  MOZ_MUST_USE bool readFunctionEnd(const uint8_t* bodyEnd);
  MOZ_MUST_USE bool readReturn(Value* value);
  MOZ_MUST_USE bool readBlock();
  MOZ_MUST_USE bool readLoop();
  MOZ_MUST_USE bool readIf(Value* condition);
  MOZ_MUST_USE bool readElse(ExprType* thenType, Value* thenValue);
  MOZ_MUST_USE bool readEnd(LabelKind* kind, ExprType* type, Value* value);
  void popEnd();
  MOZ_MUST_USE bool readBr(uint32_t* relativeDepth, ExprType* type,
                           Value* value);
  MOZ_MUST_USE bool readBrIf(uint32_t* relativeDepth, ExprType* type,
                             Value* value, Value* condition);
  MOZ_MUST_USE bool readBrTable(Uint32Vector* depths, uint32_t* defaultDepth,
                                ExprType* branchValueType, Value* branchValue,
                                Value* index);
  MOZ_MUST_USE bool readUnreachable();
  MOZ_MUST_USE bool readDrop();
  MOZ_MUST_USE bool readUnary(ValType operandType, Value* input);
  MOZ_MUST_USE bool readConversion(ValType operandType, ValType resultType,
                                   Value* input);
  MOZ_MUST_USE bool readBinary(ValType operandType, Value* lhs, Value* rhs);
  MOZ_MUST_USE bool readComparison(ValType operandType, Value* lhs, Value* rhs);
  MOZ_MUST_USE bool readLoad(ValType resultType, uint32_t byteSize,
                             LinearMemoryAddress<Value>* addr);
  MOZ_MUST_USE bool readStore(ValType resultType, uint32_t byteSize,
                              LinearMemoryAddress<Value>* addr, Value* value);
  MOZ_MUST_USE bool readTeeStore(ValType resultType, uint32_t byteSize,
                                 LinearMemoryAddress<Value>* addr,
                                 Value* value);
  MOZ_MUST_USE bool readNop();
  MOZ_MUST_USE bool readMemorySize();
  MOZ_MUST_USE bool readMemoryGrow(Value* input);
  MOZ_MUST_USE bool readSelect(StackType* type, Value* trueValue,
                               Value* falseValue, Value* condition);
  MOZ_MUST_USE bool readGetLocal(const ValTypeVector& locals, uint32_t* id);
  MOZ_MUST_USE bool readSetLocal(const ValTypeVector& locals, uint32_t* id,
                                 Value* value);
  MOZ_MUST_USE bool readTeeLocal(const ValTypeVector& locals, uint32_t* id,
                                 Value* value);
  MOZ_MUST_USE bool readGetGlobal(uint32_t* id);
  MOZ_MUST_USE bool readSetGlobal(uint32_t* id, Value* value);
  MOZ_MUST_USE bool readTeeGlobal(uint32_t* id, Value* value);
  MOZ_MUST_USE bool readI32Const(int32_t* i32);
  MOZ_MUST_USE bool readI64Const(int64_t* i64);
  MOZ_MUST_USE bool readF32Const(float* f32);
  MOZ_MUST_USE bool readF64Const(double* f64);
  MOZ_MUST_USE bool readRefFunc(uint32_t* funcTypeIndex);
  MOZ_MUST_USE bool readRefNull();
  MOZ_MUST_USE bool readCall(uint32_t* calleeIndex, ValueVector* argValues);
  MOZ_MUST_USE bool readCallIndirect(uint32_t* funcTypeIndex,
                                     uint32_t* tableIndex, Value* callee,
                                     ValueVector* argValues);
  MOZ_MUST_USE bool readOldCallDirect(uint32_t numFuncImports,
                                      uint32_t* funcIndex,
                                      ValueVector* argValues);
  MOZ_MUST_USE bool readOldCallIndirect(uint32_t* funcTypeIndex, Value* callee,
                                        ValueVector* argValues);
  MOZ_MUST_USE bool readWake(LinearMemoryAddress<Value>* addr, Value* count);
  MOZ_MUST_USE bool readWait(LinearMemoryAddress<Value>* addr,
                             ValType resultType, uint32_t byteSize,
                             Value* value, Value* timeout);
  MOZ_MUST_USE bool readFence();
  MOZ_MUST_USE bool readAtomicLoad(LinearMemoryAddress<Value>* addr,
                                   ValType resultType, uint32_t byteSize);
  MOZ_MUST_USE bool readAtomicStore(LinearMemoryAddress<Value>* addr,
                                    ValType resultType, uint32_t byteSize,
                                    Value* value);
  MOZ_MUST_USE bool readAtomicRMW(LinearMemoryAddress<Value>* addr,
                                  ValType resultType, uint32_t byteSize,
                                  Value* value);
  MOZ_MUST_USE bool readAtomicCmpXchg(LinearMemoryAddress<Value>* addr,
                                      ValType resultType, uint32_t byteSize,
                                      Value* oldValue, Value* newValue);
  MOZ_MUST_USE bool readMemOrTableCopy(bool isMem, uint32_t* dstMemOrTableIndex,
                                       Value* dst, uint32_t* srcMemOrTableIndex,
                                       Value* src, Value* len);
  MOZ_MUST_USE bool readDataOrElemDrop(bool isData, uint32_t* segIndex);
  MOZ_MUST_USE bool readMemFill(Value* start, Value* val, Value* len);
  MOZ_MUST_USE bool readMemOrTableInit(bool isMem, uint32_t* segIndex,
                                       uint32_t* dstTableIndex, Value* dst,
                                       Value* src, Value* len);
  MOZ_MUST_USE bool readTableFill(uint32_t* tableIndex, Value* start,
                                  Value* val, Value* len);
  MOZ_MUST_USE bool readTableGet(uint32_t* tableIndex, Value* index);
  MOZ_MUST_USE bool readTableGrow(uint32_t* tableIndex, Value* initValue,
                                  Value* delta);
  MOZ_MUST_USE bool readTableSet(uint32_t* tableIndex, Value* index,
                                 Value* value);
  MOZ_MUST_USE bool readTableSize(uint32_t* tableIndex);
  MOZ_MUST_USE bool readStructNew(uint32_t* typeIndex, ValueVector* argValues);
  MOZ_MUST_USE bool readStructGet(uint32_t* typeIndex, uint32_t* fieldIndex,
                                  Value* ptr);
  MOZ_MUST_USE bool readStructSet(uint32_t* typeIndex, uint32_t* fieldIndex,
                                  Value* ptr, Value* val);
  MOZ_MUST_USE bool readStructNarrow(ValType* inputType, ValType* outputType,
                                     Value* ptr);
  MOZ_MUST_USE bool readValType(ValType* type);
  MOZ_MUST_USE bool readReferenceType(ValType* type, const char* const context);

  // At a location where readOp is allowed, peek at the next opcode
  // without consuming it or updating any internal state.
  // Never fails: returns uint16_t(Op::Limit) in op->b0 if it can't read.
  void peekOp(OpBytes* op);

  // ------------------------------------------------------------------------
  // Stack management.

  // Set the result value of the current top-of-value-stack expression.
  void setResult(Value value) { valueStack_.back().setValue(value); }

  // Return the result value of the current top-of-value-stack expression.
  Value getResult() { return valueStack_.back().value(); }

  // Return a reference to the top of the control stack.
  ControlItem& controlItem() { return controlStack_.back().controlItem(); }

  // Return a reference to an element in the control stack.
  ControlItem& controlItem(uint32_t relativeDepth) {
    return controlStack_[controlStack_.length() - 1 - relativeDepth]
        .controlItem();
  }

  // Return a reference to the outermost element on the control stack.
  ControlItem& controlOutermost() { return controlStack_[0].controlItem(); }

  // Test whether the control-stack is empty, meaning we've consumed the final
  // end of the function body.
  bool controlStackEmpty() const { return controlStack_.empty(); }
};

template <typename Policy>
inline bool OpIter<Policy>::Join(StackType one, StackType two,
                                 StackType* result) const {
  if (MOZ_LIKELY(one == two)) {
    *result = one;
    return true;
  }

  if (one == StackType::TVar) {
    *result = two;
    return true;
  }

  if (two == StackType::TVar) {
    *result = one;
    return true;
  }

  if (one.isReference() && two.isReference()) {
    if (env_.isRefSubtypeOf(NonTVarToValType(two), NonTVarToValType(one))) {
      *result = one;
      return true;
    }

    if (env_.isRefSubtypeOf(NonTVarToValType(one), NonTVarToValType(two))) {
      *result = two;
      return true;
    }

    // No subtyping relations between the two types.
    *result = StackType::AnyRef;
    return true;
  }

  return false;
}

template <typename Policy>
inline bool OpIter<Policy>::checkIsSubtypeOf(ValType actual, ValType expected) {
  if (actual == expected) {
    return true;
  }

  if (actual.isReference() && expected.isReference() &&
      env_.isRefSubtypeOf(actual, expected)) {
    return true;
  }

  UniqueChars error(
      JS_smprintf("type mismatch: expression has type %s but expected %s",
                  ToCString(actual), ToCString(expected)));
  if (!error) {
    return false;
  }

  return fail(error.get());
}

template <typename Policy>
inline bool OpIter<Policy>::unrecognizedOpcode(const OpBytes* expr) {
  UniqueChars error(JS_smprintf("unrecognized opcode: %x %x", expr->b0,
                                IsPrefixByte(expr->b0) ? expr->b1 : 0));
  if (!error) {
    return false;
  }

  return fail(error.get());
}

template <typename Policy>
inline bool OpIter<Policy>::fail(const char* msg) {
  return d_.fail(lastOpcodeOffset(), msg);
}

template <typename Policy>
inline bool OpIter<Policy>::fail_ctx(const char* fmt, const char* context) {
  UniqueChars error(JS_smprintf(fmt, context));
  if (!error) {
    return false;
  }
  return fail(error.get());
}

template <typename Policy>
inline bool OpIter<Policy>::failEmptyStack() {
  return valueStack_.empty() ? fail("popping value from empty stack")
                             : fail("popping value from outside block");
}

// This function pops exactly one value from the stack, yielding TVar types in
// various cases and therefore making it the caller's responsibility to do the
// right thing for StackType::TVar. Prefer (pop|top)WithType.
template <typename Policy>
inline bool OpIter<Policy>::popStackType(StackType* type, Value* value) {
  ControlStackEntry<ControlItem>& block = controlStack_.back();

  MOZ_ASSERT(valueStack_.length() >= block.valueStackStart());
  if (MOZ_UNLIKELY(valueStack_.length() == block.valueStackStart())) {
    // If the base of this block's stack is polymorphic, then we can pop a
    // dummy value of any type; it won't be used since we're in unreachable
    // code.
    if (block.polymorphicBase()) {
      *type = StackType::TVar;
      *value = Value();

      // Maintain the invariant that, after a pop, there is always memory
      // reserved to push a value infallibly.
      return valueStack_.reserve(valueStack_.length() + 1);
    }

    return failEmptyStack();
  }

  TypeAndValue<Value>& tv = valueStack_.back();
  *type = tv.type();
  *value = tv.value();
  valueStack_.popBack();
  return true;
}

// This function pops exactly one value from the stack, checking that it has the
// expected type which can either be a specific value type or a type variable.
template <typename Policy>
inline bool OpIter<Policy>::popWithType(ValType expectedType, Value* value) {
  StackType stackType(expectedType);
  if (!popStackType(&stackType, value)) {
    return false;
  }

  return stackType == StackType::TVar ||
         checkIsSubtypeOf(NonTVarToValType(stackType), expectedType);
}

// This function pops as many types from the stack as determined by the given
// signature. Currently, all signatures are limited to 0 or 1 types, with
// ExprType::Void meaning 0 and all other ValTypes meaning 1, but this will be
// generalized in the future.
template <typename Policy>
inline bool OpIter<Policy>::popWithType(ExprType expectedType, Value* value) {
  if (IsVoid(expectedType)) {
    *value = Value();
    return true;
  }

  return popWithType(NonVoidToValType(expectedType), value);
}

// This function is just an optimization of popWithType + push.
template <typename Policy>
inline bool OpIter<Policy>::topWithType(ValType expectedType, Value* value) {
  ControlStackEntry<ControlItem>& block = controlStack_.back();

  MOZ_ASSERT(valueStack_.length() >= block.valueStackStart());
  if (valueStack_.length() == block.valueStackStart()) {
    // If the base of this block's stack is polymorphic, then we can just
    // pull out a dummy value of the expected type; it won't be used since
    // we're in unreachable code. We must however push this value onto the
    // stack since it is now fixed to a specific type by this type
    // constraint.
    if (block.polymorphicBase()) {
      if (!valueStack_.emplaceBack(expectedType, Value())) {
        return false;
      }

      *value = Value();
      return true;
    }

    return failEmptyStack();
  }

  TypeAndValue<Value>& observed = valueStack_.back();

  if (observed.type() == StackType::TVar) {
    observed.typeRef() = StackType(expectedType);
    *value = Value();
    return true;
  }

  if (!checkIsSubtypeOf(NonTVarToValType(observed.type()), expectedType)) {
    return false;
  }

  *value = observed.value();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::topWithType(ExprType expectedType, Value* value) {
  if (IsVoid(expectedType)) {
    *value = Value();
    return true;
  }

  return topWithType(NonVoidToValType(expectedType), value);
}

template <typename Policy>
inline bool OpIter<Policy>::pushControl(LabelKind kind, ExprType type) {
  return controlStack_.emplaceBack(kind, type, valueStack_.length());
}

template <typename Policy>
inline bool OpIter<Policy>::checkStackAtEndOfBlock(ExprType* type,
                                                   Value* value) {
  ControlStackEntry<ControlItem>& block = controlStack_.back();

  MOZ_ASSERT(valueStack_.length() >= block.valueStackStart());
  size_t pushed = valueStack_.length() - block.valueStackStart();
  if (pushed > (IsVoid(block.resultType()) ? 0u : 1u)) {
    return fail("unused values not explicitly dropped by end of block");
  }

  if (!topWithType(block.resultType(), value)) {
    return false;
  }

  *type = block.resultType();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::getControl(
    uint32_t relativeDepth, ControlStackEntry<ControlItem>** controlEntry) {
  if (relativeDepth >= controlStack_.length()) {
    return fail("branch depth exceeds current nesting level");
  }

  *controlEntry = &controlStack_[controlStack_.length() - 1 - relativeDepth];
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readBlockType(ExprType* type) {
  uint8_t uncheckedCode;
  uint32_t uncheckedRefTypeIndex;
  if (!d_.readBlockType(&uncheckedCode, &uncheckedRefTypeIndex)) {
    return fail("unable to read block signature");
  }

  bool known = false;
  switch (uncheckedCode) {
    case uint8_t(ExprType::Void):
    case uint8_t(ExprType::I32):
    case uint8_t(ExprType::I64):
    case uint8_t(ExprType::F32):
    case uint8_t(ExprType::F64):
      known = true;
      break;
    case uint8_t(ExprType::FuncRef):
    case uint8_t(ExprType::AnyRef):
#ifdef ENABLE_WASM_REFTYPES
      known = env_.refTypesEnabled();
#endif
      break;
    case uint8_t(ExprType::Ref):
      known = env_.gcTypesEnabled() && uncheckedRefTypeIndex < MaxTypes &&
              uncheckedRefTypeIndex < env_.types.length();
      break;
    case uint8_t(ExprType::Limit):
      break;
  }

  if (!known) {
    return fail("invalid inline block type");
  }

  *type = ExprType(ExprType::Code(uncheckedCode), uncheckedRefTypeIndex);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readOp(OpBytes* op) {
  MOZ_ASSERT(!controlStack_.empty());

  offsetOfLastReadOp_ = d_.currentOffset();

  if (MOZ_UNLIKELY(!d_.readOp(op))) {
    return fail("unable to read opcode");
  }

#ifdef DEBUG
  op_ = *op;
#endif

  return true;
}

template <typename Policy>
inline void OpIter<Policy>::peekOp(OpBytes* op) {
  const uint8_t* pos = d_.currentPosition();

  if (MOZ_UNLIKELY(!d_.readOp(op))) {
    op->b0 = uint16_t(Op::Limit);
  }

  d_.rollbackPosition(pos);
}

template <typename Policy>
inline bool OpIter<Policy>::readFunctionStart(ExprType ret) {
  MOZ_ASSERT(valueStack_.empty());
  MOZ_ASSERT(controlStack_.empty());
  MOZ_ASSERT(op_.b0 == uint16_t(Op::Limit));

  return pushControl(LabelKind::Body, ret);
}

template <typename Policy>
inline bool OpIter<Policy>::readFunctionEnd(const uint8_t* bodyEnd) {
  if (d_.currentPosition() != bodyEnd) {
    return fail("function body length mismatch");
  }

  if (!controlStack_.empty()) {
    return fail("unbalanced function body control flow");
  }

#ifdef DEBUG
  op_ = OpBytes(Op::Limit);
#endif
  valueStack_.clear();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readReturn(Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::Return);

  ControlStackEntry<ControlItem>& body = controlStack_[0];
  MOZ_ASSERT(body.kind() == LabelKind::Body);

  if (!popWithType(body.resultType(), value)) {
    return false;
  }

  afterUnconditionalBranch();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readBlock() {
  MOZ_ASSERT(Classify(op_) == OpKind::Block);

  ExprType type = ExprType::Limit;
  if (!readBlockType(&type)) {
    return false;
  }

  return pushControl(LabelKind::Block, type);
}

template <typename Policy>
inline bool OpIter<Policy>::readLoop() {
  MOZ_ASSERT(Classify(op_) == OpKind::Loop);

  ExprType type = ExprType::Limit;
  if (!readBlockType(&type)) {
    return false;
  }

  return pushControl(LabelKind::Loop, type);
}

template <typename Policy>
inline bool OpIter<Policy>::readIf(Value* condition) {
  MOZ_ASSERT(Classify(op_) == OpKind::If);

  ExprType type = ExprType::Limit;
  if (!readBlockType(&type)) {
    return false;
  }

  if (!popWithType(ValType::I32, condition)) {
    return false;
  }

  return pushControl(LabelKind::Then, type);
}

template <typename Policy>
inline bool OpIter<Policy>::readElse(ExprType* type, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::Else);

  // Finish checking the then-block.

  if (!checkStackAtEndOfBlock(type, value)) {
    return false;
  }

  ControlStackEntry<ControlItem>& block = controlStack_.back();

  if (block.kind() != LabelKind::Then) {
    return fail("else can only be used within an if");
  }

  // Switch to the else-block.

  if (!IsVoid(block.resultType())) {
    valueStack_.popBack();
  }

  MOZ_ASSERT(valueStack_.length() == block.valueStackStart());

  block.switchToElse();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readEnd(LabelKind* kind, ExprType* type,
                                    Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::End);

  if (!checkStackAtEndOfBlock(type, value)) {
    return false;
  }

  ControlStackEntry<ControlItem>& block = controlStack_.back();

  // If an `if` block ends with `end` instead of `else`, then we must
  // additionally validate that the then-block doesn't push anything.
  if (block.kind() == LabelKind::Then && !IsVoid(block.resultType())) {
    return fail("if without else with a result value");
  }

  *kind = block.kind();
  return true;
}

template <typename Policy>
inline void OpIter<Policy>::popEnd() {
  MOZ_ASSERT(Classify(op_) == OpKind::End);

  controlStack_.popBack();
}

template <typename Policy>
inline bool OpIter<Policy>::checkBranchValue(uint32_t relativeDepth,
                                             ExprType* type, Value* value) {
  ControlStackEntry<ControlItem>* block = nullptr;
  if (!getControl(relativeDepth, &block)) {
    return false;
  }

  *type = block->branchTargetType();
  return topWithType(*type, value);
}

template <typename Policy>
inline bool OpIter<Policy>::readBr(uint32_t* relativeDepth, ExprType* type,
                                   Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::Br);

  if (!readVarU32(relativeDepth)) {
    return fail("unable to read br depth");
  }

  if (!checkBranchValue(*relativeDepth, type, value)) {
    return false;
  }

  afterUnconditionalBranch();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readBrIf(uint32_t* relativeDepth, ExprType* type,
                                     Value* value, Value* condition) {
  MOZ_ASSERT(Classify(op_) == OpKind::BrIf);

  if (!readVarU32(relativeDepth)) {
    return fail("unable to read br_if depth");
  }

  if (!popWithType(ValType::I32, condition)) {
    return false;
  }

  return checkBranchValue(*relativeDepth, type, value);
}

template <typename Policy>
inline bool OpIter<Policy>::checkBrTableEntry(uint32_t* relativeDepth,
                                              ExprType* branchValueType,
                                              Value* branchValue) {
  if (!readVarU32(relativeDepth)) {
    return false;
  }

  // For the first encountered branch target, do a normal branch value type
  // check which will change *branchValueType to a non-sentinel value. For all
  // subsequent branch targets, check that the branch target matches the
  // now-known branch value type.

  if (*branchValueType == ExprType::Limit) {
    if (!checkBranchValue(*relativeDepth, branchValueType, branchValue)) {
      return false;
    }
  } else {
    ControlStackEntry<ControlItem>* block = nullptr;
    if (!getControl(*relativeDepth, &block)) {
      return false;
    }

    if (*branchValueType != block->branchTargetType()) {
      return fail("br_table targets must all have the same value type");
    }
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readBrTable(Uint32Vector* depths,
                                        uint32_t* defaultDepth,
                                        ExprType* branchValueType,
                                        Value* branchValue, Value* index) {
  MOZ_ASSERT(Classify(op_) == OpKind::BrTable);

  uint32_t tableLength;
  if (!readVarU32(&tableLength)) {
    return fail("unable to read br_table table length");
  }

  if (tableLength > MaxBrTableElems) {
    return fail("br_table too big");
  }

  if (!popWithType(ValType::I32, index)) {
    return false;
  }

  if (!depths->resize(tableLength)) {
    return false;
  }

  *branchValueType = ExprType::Limit;

  for (uint32_t i = 0; i < tableLength; i++) {
    if (!checkBrTableEntry(&(*depths)[i], branchValueType, branchValue)) {
      return false;
    }
  }

  if (!checkBrTableEntry(defaultDepth, branchValueType, branchValue)) {
    return false;
  }

  MOZ_ASSERT(*branchValueType != ExprType::Limit);

  afterUnconditionalBranch();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readUnreachable() {
  MOZ_ASSERT(Classify(op_) == OpKind::Unreachable);

  afterUnconditionalBranch();
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readDrop() {
  MOZ_ASSERT(Classify(op_) == OpKind::Drop);
  StackType type;
  Value value;
  return popStackType(&type, &value);
}

template <typename Policy>
inline bool OpIter<Policy>::readUnary(ValType operandType, Value* input) {
  MOZ_ASSERT(Classify(op_) == OpKind::Unary);

  if (!popWithType(operandType, input)) {
    return false;
  }

  infalliblePush(operandType);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readConversion(ValType operandType,
                                           ValType resultType, Value* input) {
  MOZ_ASSERT(Classify(op_) == OpKind::Conversion);

  if (!popWithType(operandType, input)) {
    return false;
  }

  infalliblePush(resultType);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readBinary(ValType operandType, Value* lhs,
                                       Value* rhs) {
  MOZ_ASSERT(Classify(op_) == OpKind::Binary);

  if (!popWithType(operandType, rhs)) {
    return false;
  }

  if (!popWithType(operandType, lhs)) {
    return false;
  }

  infalliblePush(operandType);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readComparison(ValType operandType, Value* lhs,
                                           Value* rhs) {
  MOZ_ASSERT(Classify(op_) == OpKind::Comparison);

  if (!popWithType(operandType, rhs)) {
    return false;
  }

  if (!popWithType(operandType, lhs)) {
    return false;
  }

  infalliblePush(ValType::I32);

  return true;
}

// For memories, the index is currently always a placeholder zero byte.
//
// For tables, the index is a placeholder zero byte until we get multi-table
// with the reftypes proposal.
//
// The zero-ness of the value must be checked by the caller.
template <typename Policy>
inline bool OpIter<Policy>::readMemOrTableIndex(bool isMem, uint32_t* index) {
#ifdef ENABLE_WASM_REFTYPES
  bool readByte = isMem;
#else
  bool readByte = true;
#endif
  if (readByte) {
    uint8_t indexTmp;
    if (!readFixedU8(&indexTmp)) {
      return false;
    }
    *index = indexTmp;
  } else {
    if (!readVarU32(index)) {
      return false;
    }
  }
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readLinearMemoryAddress(
    uint32_t byteSize, LinearMemoryAddress<Value>* addr) {
  if (!env_.usesMemory()) {
    return fail("can't touch memory without memory");
  }

  uint8_t alignLog2;
  if (!readFixedU8(&alignLog2)) {
    return fail("unable to read load alignment");
  }

  if (!readVarU32(&addr->offset)) {
    return fail("unable to read load offset");
  }

  if (alignLog2 >= 32 || (uint32_t(1) << alignLog2) > byteSize) {
    return fail("greater than natural alignment");
  }

  if (!popWithType(ValType::I32, &addr->base)) {
    return false;
  }

  addr->align = uint32_t(1) << alignLog2;
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readLinearMemoryAddressAligned(
    uint32_t byteSize, LinearMemoryAddress<Value>* addr) {
  if (!readLinearMemoryAddress(byteSize, addr)) {
    return false;
  }

  if (addr->align != byteSize) {
    return fail("not natural alignment");
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readLoad(ValType resultType, uint32_t byteSize,
                                     LinearMemoryAddress<Value>* addr) {
  MOZ_ASSERT(Classify(op_) == OpKind::Load);

  if (!readLinearMemoryAddress(byteSize, addr)) {
    return false;
  }

  infalliblePush(resultType);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readStore(ValType resultType, uint32_t byteSize,
                                      LinearMemoryAddress<Value>* addr,
                                      Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::Store);

  if (!popWithType(resultType, value)) {
    return false;
  }

  if (!readLinearMemoryAddress(byteSize, addr)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTeeStore(ValType resultType, uint32_t byteSize,
                                         LinearMemoryAddress<Value>* addr,
                                         Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::TeeStore);

  if (!popWithType(resultType, value)) {
    return false;
  }

  if (!readLinearMemoryAddress(byteSize, addr)) {
    return false;
  }

  infalliblePush(TypeAndValue<Value>(resultType, *value));
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readNop() {
  MOZ_ASSERT(Classify(op_) == OpKind::Nop);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readMemorySize() {
  MOZ_ASSERT(Classify(op_) == OpKind::MemorySize);

  if (!env_.usesMemory()) {
    return fail("can't touch memory without memory");
  }

  uint8_t flags;
  if (!readFixedU8(&flags)) {
    return false;
  }

  if (flags != uint8_t(MemoryTableFlags::Default)) {
    return fail("unexpected flags");
  }

  return push(ValType::I32);
}

template <typename Policy>
inline bool OpIter<Policy>::readMemoryGrow(Value* input) {
  MOZ_ASSERT(Classify(op_) == OpKind::MemoryGrow);

  if (!env_.usesMemory()) {
    return fail("can't touch memory without memory");
  }

  uint8_t flags;
  if (!readFixedU8(&flags)) {
    return false;
  }

  if (flags != uint8_t(MemoryTableFlags::Default)) {
    return fail("unexpected flags");
  }

  if (!popWithType(ValType::I32, input)) {
    return false;
  }

  infalliblePush(ValType::I32);

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readSelect(StackType* type, Value* trueValue,
                                       Value* falseValue, Value* condition) {
  MOZ_ASSERT(Classify(op_) == OpKind::Select);

  if (!popWithType(ValType::I32, condition)) {
    return false;
  }

  StackType falseType;
  if (!popStackType(&falseType, falseValue)) {
    return false;
  }

  StackType trueType;
  if (!popStackType(&trueType, trueValue)) {
    return false;
  }

  if (!Join(falseType, trueType, type)) {
    return fail("select operand types must match");
  }

  infalliblePush(*type);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readGetLocal(const ValTypeVector& locals,
                                         uint32_t* id) {
  MOZ_ASSERT(Classify(op_) == OpKind::GetLocal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= locals.length()) {
    return fail("local.get index out of range");
  }

  return push(locals[*id]);
}

template <typename Policy>
inline bool OpIter<Policy>::readSetLocal(const ValTypeVector& locals,
                                         uint32_t* id, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::SetLocal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= locals.length()) {
    return fail("local.set index out of range");
  }

  return popWithType(locals[*id], value);
}

template <typename Policy>
inline bool OpIter<Policy>::readTeeLocal(const ValTypeVector& locals,
                                         uint32_t* id, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::TeeLocal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= locals.length()) {
    return fail("local.set index out of range");
  }

  return topWithType(locals[*id], value);
}

template <typename Policy>
inline bool OpIter<Policy>::readGetGlobal(uint32_t* id) {
  MOZ_ASSERT(Classify(op_) == OpKind::GetGlobal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= env_.globals.length()) {
    return fail("global.get index out of range");
  }

  return push(env_.globals[*id].type());
}

template <typename Policy>
inline bool OpIter<Policy>::readSetGlobal(uint32_t* id, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::SetGlobal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= env_.globals.length()) {
    return fail("global.set index out of range");
  }

  if (!env_.globals[*id].isMutable()) {
    return fail("can't write an immutable global");
  }

  return popWithType(env_.globals[*id].type(), value);
}

template <typename Policy>
inline bool OpIter<Policy>::readTeeGlobal(uint32_t* id, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::TeeGlobal);

  if (!readVarU32(id)) {
    return false;
  }

  if (*id >= env_.globals.length()) {
    return fail("global.set index out of range");
  }

  if (!env_.globals[*id].isMutable()) {
    return fail("can't write an immutable global");
  }

  return topWithType(env_.globals[*id].type(), value);
}

template <typename Policy>
inline bool OpIter<Policy>::readI32Const(int32_t* i32) {
  MOZ_ASSERT(Classify(op_) == OpKind::I32);

  return readVarS32(i32) && push(ValType::I32);
}

template <typename Policy>
inline bool OpIter<Policy>::readI64Const(int64_t* i64) {
  MOZ_ASSERT(Classify(op_) == OpKind::I64);

  return readVarS64(i64) && push(ValType::I64);
}

template <typename Policy>
inline bool OpIter<Policy>::readF32Const(float* f32) {
  MOZ_ASSERT(Classify(op_) == OpKind::F32);

  return readFixedF32(f32) && push(ValType::F32);
}

template <typename Policy>
inline bool OpIter<Policy>::readF64Const(double* f64) {
  MOZ_ASSERT(Classify(op_) == OpKind::F64);

  return readFixedF64(f64) && push(ValType::F64);
}

template <typename Policy>
inline bool OpIter<Policy>::readRefFunc(uint32_t* funcTypeIndex) {
  MOZ_ASSERT(Classify(op_) == OpKind::RefFunc);

  if (!readVarU32(funcTypeIndex)) {
    return fail("unable to read function index");
  }
  if (*funcTypeIndex >= env_.funcTypes.length()) {
    return fail("function index out of range");
  }
  if (!env_.validForRefFunc.getBit(*funcTypeIndex)) {
    return fail("function index is not in an element segment");
  }
  return push(StackType(ValType::FuncRef));
}

template <typename Policy>
inline bool OpIter<Policy>::readRefNull() {
  MOZ_ASSERT(Classify(op_) == OpKind::RefNull);

  return push(StackType(ValType::NullRef));
}

template <typename Policy>
inline bool OpIter<Policy>::readValType(ValType* type) {
  return d_.readValType(env_.types, env_.refTypesEnabled(),
                        env_.gcTypesEnabled(), type);
}

template <typename Policy>
inline bool OpIter<Policy>::readReferenceType(ValType* type,
                                              const char* context) {
  if (!readValType(type) || !type->isReference()) {
    return fail_ctx("invalid reference type for %s", context);
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::popCallArgs(const ValTypeVector& expectedTypes,
                                        ValueVector* values) {
  // Iterate through the argument types backward so that pops occur in the
  // right order.

  if (!values->resize(expectedTypes.length())) {
    return false;
  }

  for (int32_t i = expectedTypes.length() - 1; i >= 0; i--) {
    if (!popWithType(expectedTypes[i], &(*values)[i])) {
      return false;
    }
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readCall(uint32_t* funcTypeIndex,
                                     ValueVector* argValues) {
  MOZ_ASSERT(Classify(op_) == OpKind::Call);

  if (!readVarU32(funcTypeIndex)) {
    return fail("unable to read call function index");
  }

  if (*funcTypeIndex >= env_.funcTypes.length()) {
    return fail("callee index out of range");
  }

  const FuncType& funcType = *env_.funcTypes[*funcTypeIndex];

  if (!popCallArgs(funcType.args(), argValues)) {
    return false;
  }

  return push(funcType.ret());
}

template <typename Policy>
inline bool OpIter<Policy>::readCallIndirect(uint32_t* funcTypeIndex,
                                             uint32_t* tableIndex,
                                             Value* callee,
                                             ValueVector* argValues) {
  MOZ_ASSERT(Classify(op_) == OpKind::CallIndirect);
  MOZ_ASSERT(funcTypeIndex != tableIndex);

  if (!readVarU32(funcTypeIndex)) {
    return fail("unable to read call_indirect signature index");
  }

  if (*funcTypeIndex >= env_.numTypes()) {
    return fail("signature index out of range");
  }

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    // Special case this for improved user experience.
    if (!env_.tables.length()) {
      return fail("can't call_indirect without a table");
    }
    return fail("table index out of range for call_indirect");
  }
  if (env_.tables[*tableIndex].kind != TableKind::FuncRef) {
    return fail("indirect calls must go through a table of 'funcref'");
  }

  if (!popWithType(ValType::I32, callee)) {
    return false;
  }

  if (!env_.types[*funcTypeIndex].isFuncType()) {
    return fail("expected signature type");
  }

  const FuncType& funcType = env_.types[*funcTypeIndex].funcType();

#ifdef WASM_PRIVATE_REFTYPES
  if (env_.tables[*tableIndex].importedOrExported && funcType.exposesRef()) {
    return fail("cannot expose reference type");
  }
#endif

  if (!popCallArgs(funcType.args(), argValues)) {
    return false;
  }

  return push(funcType.ret());
}

template <typename Policy>
inline bool OpIter<Policy>::readOldCallDirect(uint32_t numFuncImports,
                                              uint32_t* funcTypeIndex,
                                              ValueVector* argValues) {
  MOZ_ASSERT(Classify(op_) == OpKind::OldCallDirect);

  uint32_t funcDefIndex;
  if (!readVarU32(&funcDefIndex)) {
    return fail("unable to read call function index");
  }

  if (UINT32_MAX - funcDefIndex < numFuncImports) {
    return fail("callee index out of range");
  }

  *funcTypeIndex = numFuncImports + funcDefIndex;

  if (*funcTypeIndex >= env_.funcTypes.length()) {
    return fail("callee index out of range");
  }

  const FuncType& funcType = *env_.funcTypes[*funcTypeIndex];

  if (!popCallArgs(funcType.args(), argValues)) {
    return false;
  }

  return push(funcType.ret());
}

template <typename Policy>
inline bool OpIter<Policy>::readOldCallIndirect(uint32_t* funcTypeIndex,
                                                Value* callee,
                                                ValueVector* argValues) {
  MOZ_ASSERT(Classify(op_) == OpKind::OldCallIndirect);

  if (!readVarU32(funcTypeIndex)) {
    return fail("unable to read call_indirect signature index");
  }

  if (*funcTypeIndex >= env_.numTypes()) {
    return fail("signature index out of range");
  }

  if (!env_.types[*funcTypeIndex].isFuncType()) {
    return fail("expected signature type");
  }

  const FuncType& funcType = env_.types[*funcTypeIndex].funcType();

  if (!popCallArgs(funcType.args(), argValues)) {
    return false;
  }

  if (!popWithType(ValType::I32, callee)) {
    return false;
  }

  if (!push(funcType.ret())) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readWake(LinearMemoryAddress<Value>* addr,
                                     Value* count) {
  MOZ_ASSERT(Classify(op_) == OpKind::Wake);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!popWithType(ValType::I32, count)) {
    return false;
  }

  uint32_t byteSize = 4;  // Per spec; smallest WAIT is i32.

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  infalliblePush(ValType::I32);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readWait(LinearMemoryAddress<Value>* addr,
                                     ValType valueType, uint32_t byteSize,
                                     Value* value, Value* timeout) {
  MOZ_ASSERT(Classify(op_) == OpKind::Wait);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!popWithType(ValType::I64, timeout)) {
    return false;
  }

  if (!popWithType(valueType, value)) {
    return false;
  }

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  infalliblePush(ValType::I32);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readFence() {
  MOZ_ASSERT(Classify(op_) == OpKind::Fence);
  uint8_t flags;
  if (!readFixedU8(&flags)) {
    return fail("expected memory order after fence");
  }
  if (flags != 0) {
    return fail("non-zero memory order not supported yet");
  }
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readAtomicLoad(LinearMemoryAddress<Value>* addr,
                                           ValType resultType,
                                           uint32_t byteSize) {
  MOZ_ASSERT(Classify(op_) == OpKind::AtomicLoad);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  infalliblePush(resultType);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readAtomicStore(LinearMemoryAddress<Value>* addr,
                                            ValType resultType,
                                            uint32_t byteSize, Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::AtomicStore);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!popWithType(resultType, value)) {
    return false;
  }

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readAtomicRMW(LinearMemoryAddress<Value>* addr,
                                          ValType resultType, uint32_t byteSize,
                                          Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::AtomicBinOp);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!popWithType(resultType, value)) {
    return false;
  }

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  infalliblePush(resultType);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readAtomicCmpXchg(LinearMemoryAddress<Value>* addr,
                                              ValType resultType,
                                              uint32_t byteSize,
                                              Value* oldValue,
                                              Value* newValue) {
  MOZ_ASSERT(Classify(op_) == OpKind::AtomicCompareExchange);

  if (!env_.usesSharedMemory()) {
    return fail(
        "can't touch memory with atomic operations without shared memory");
  }

  if (!popWithType(resultType, newValue)) {
    return false;
  }

  if (!popWithType(resultType, oldValue)) {
    return false;
  }

  if (!readLinearMemoryAddressAligned(byteSize, addr)) {
    return false;
  }

  infalliblePush(resultType);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readMemOrTableCopy(bool isMem,
                                               uint32_t* dstMemOrTableIndex,
                                               Value* dst,
                                               uint32_t* srcMemOrTableIndex,
                                               Value* src, Value* len) {
  MOZ_ASSERT(Classify(op_) == OpKind::MemOrTableCopy);
  MOZ_ASSERT(dstMemOrTableIndex != srcMemOrTableIndex);

  // We use (dest, src) everywhere in code but the spec requires (src, dest)
  // encoding order for the immediates.
  if (!readMemOrTableIndex(isMem, srcMemOrTableIndex)) {
    return false;
  }
  if (!readMemOrTableIndex(isMem, dstMemOrTableIndex)) {
    return false;
  }

  if (isMem) {
    if (!env_.usesMemory()) {
      return fail("can't touch memory without memory");
    }
    if (*srcMemOrTableIndex != 0 || *dstMemOrTableIndex != 0) {
      return fail("memory index out of range for memory.copy");
    }
  } else {
    if (*dstMemOrTableIndex >= env_.tables.length() ||
        *srcMemOrTableIndex >= env_.tables.length()) {
      return fail("table index out of range for table.copy");
    }
  }

  if (!popWithType(ValType::I32, len)) {
    return false;
  }

  if (!popWithType(ValType::I32, src)) {
    return false;
  }

  if (!popWithType(ValType::I32, dst)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readDataOrElemDrop(bool isData,
                                               uint32_t* segIndex) {
  MOZ_ASSERT(Classify(op_) == OpKind::DataOrElemDrop);

  if (!readVarU32(segIndex)) {
    return false;
  }

  if (isData) {
    if (env_.dataCount.isNothing()) {
      return fail("data.drop requires a DataCount section");
    }
    if (*segIndex >= *env_.dataCount) {
      return fail("data.drop segment index out of range");
    }
  } else {
    if (*segIndex >= env_.elemSegments.length()) {
      return fail("element segment index out of range for elem.drop");
    }
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readMemFill(Value* start, Value* val, Value* len) {
  MOZ_ASSERT(Classify(op_) == OpKind::MemFill);

  if (!env_.usesMemory()) {
    return fail("can't touch memory without memory");
  }

  uint8_t memoryIndex;
  if (!readFixedU8(&memoryIndex)) {
    return false;
  }
  if (!env_.usesMemory()) {
    return fail("can't touch memory without memory");
  }
  if (memoryIndex != 0) {
    return fail("memory index must be zero");
  }

  if (!popWithType(ValType::I32, len)) {
    return false;
  }

  if (!popWithType(ValType::I32, val)) {
    return false;
  }

  if (!popWithType(ValType::I32, start)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readMemOrTableInit(bool isMem, uint32_t* segIndex,
                                               uint32_t* dstTableIndex,
                                               Value* dst, Value* src,
                                               Value* len) {
  MOZ_ASSERT(Classify(op_) == OpKind::MemOrTableInit);
  MOZ_ASSERT(segIndex != dstTableIndex);

  if (!popWithType(ValType::I32, len)) {
    return false;
  }

  if (!popWithType(ValType::I32, src)) {
    return false;
  }

  if (!popWithType(ValType::I32, dst)) {
    return false;
  }

  if (!readVarU32(segIndex)) {
    return false;
  }

  uint32_t memOrTableIndex = 0;
  if (!readMemOrTableIndex(isMem, &memOrTableIndex)) {
    return false;
  }
  if (isMem) {
    if (!env_.usesMemory()) {
      return fail("can't touch memory without memory");
    }
    if (memOrTableIndex != 0) {
      return fail("memory index must be zero");
    }
    if (env_.dataCount.isNothing()) {
      return fail("memory.init requires a DataCount section");
    }
    if (*segIndex >= *env_.dataCount) {
      return fail("memory.init segment index out of range");
    }
  } else {
    if (memOrTableIndex >= env_.tables.length()) {
      return fail("table index out of range for table.init");
    }
    *dstTableIndex = memOrTableIndex;

    // Element segments must carry functions exclusively and funcref is not
    // yet a subtype of anyref.
    if (env_.tables[*dstTableIndex].kind != TableKind::FuncRef) {
      return fail("only tables of 'funcref' may have element segments");
    }
    if (*segIndex >= env_.elemSegments.length()) {
      return fail("table.init segment index out of range");
    }
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTableFill(uint32_t* tableIndex, Value* start,
                                          Value* val, Value* len) {
  MOZ_ASSERT(Classify(op_) == OpKind::TableFill);

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    return fail("table index out of range for table.fill");
  }

  if (!popWithType(ValType::I32, len)) {
    return false;
  }
  if (!popWithType(ToElemValType(env_.tables[*tableIndex].kind), val)) {
    return false;
  }
  if (!popWithType(ValType::I32, start)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTableGet(uint32_t* tableIndex, Value* index) {
  MOZ_ASSERT(Classify(op_) == OpKind::TableGet);

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    return fail("table index out of range for table.get");
  }

  if (!popWithType(ValType::I32, index)) {
    return false;
  }

  infalliblePush(ToElemValType(env_.tables[*tableIndex].kind));
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTableGrow(uint32_t* tableIndex,
                                          Value* initValue, Value* delta) {
  MOZ_ASSERT(Classify(op_) == OpKind::TableGrow);

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    return fail("table index out of range for table.grow");
  }

  if (!popWithType(ValType::I32, delta)) {
    return false;
  }
  if (!popWithType(ToElemValType(env_.tables[*tableIndex].kind), initValue)) {
    return false;
  }

  infalliblePush(ValType::I32);
  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTableSet(uint32_t* tableIndex, Value* index,
                                         Value* value) {
  MOZ_ASSERT(Classify(op_) == OpKind::TableSet);

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    return fail("table index out of range for table.set");
  }

  if (!popWithType(ToElemValType(env_.tables[*tableIndex].kind), value)) {
    return false;
  }
  if (!popWithType(ValType::I32, index)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readTableSize(uint32_t* tableIndex) {
  MOZ_ASSERT(Classify(op_) == OpKind::TableSize);

  *tableIndex = 0;

  if (!readVarU32(tableIndex)) {
    return false;
  }
  if (*tableIndex >= env_.tables.length()) {
    return fail("table index out of range for table.size");
  }

  return push(ValType::I32);
}

template <typename Policy>
inline bool OpIter<Policy>::readStructTypeIndex(uint32_t* typeIndex) {
  if (!readVarU32(typeIndex)) {
    return fail("unable to read type index");
  }

  if (*typeIndex >= env_.types.length()) {
    return fail("type index out of range");
  }

  if (!env_.types[*typeIndex].isStructType()) {
    return fail("not a struct type");
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readFieldIndex(uint32_t* fieldIndex,
                                           const StructType& structType) {
  if (!readVarU32(fieldIndex)) {
    return fail("unable to read field index");
  }

  if (structType.fields_.length() <= *fieldIndex) {
    return fail("field index out of range");
  }

  return true;
}

// Semantics of struct.new, struct.get, struct.set, and struct.narrow documented
// (for now) on https://github.com/lars-t-hansen/moz-gc-experiments.

template <typename Policy>
inline bool OpIter<Policy>::readStructNew(uint32_t* typeIndex,
                                          ValueVector* argValues) {
  MOZ_ASSERT(Classify(op_) == OpKind::StructNew);

  if (!readStructTypeIndex(typeIndex)) {
    return false;
  }

  const StructType& str = env_.types[*typeIndex].structType();

  if (!argValues->resize(str.fields_.length())) {
    return false;
  }

  static_assert(MaxStructFields <= INT32_MAX, "Or we iloop below");

  for (int32_t i = str.fields_.length() - 1; i >= 0; i--) {
    if (!popWithType(str.fields_[i].type, &(*argValues)[i])) {
      return false;
    }
  }

  return push(ValType(ValType::Ref, *typeIndex));
}

template <typename Policy>
inline bool OpIter<Policy>::readStructGet(uint32_t* typeIndex,
                                          uint32_t* fieldIndex, Value* ptr) {
  MOZ_ASSERT(typeIndex != fieldIndex);
  MOZ_ASSERT(Classify(op_) == OpKind::StructGet);

  if (!readStructTypeIndex(typeIndex)) {
    return false;
  }

  const StructType& structType = env_.types[*typeIndex].structType();

  if (!readFieldIndex(fieldIndex, structType)) {
    return false;
  }

  if (!popWithType(ValType(ValType::Ref, *typeIndex), ptr)) {
    return false;
  }

  return push(structType.fields_[*fieldIndex].type);
}

template <typename Policy>
inline bool OpIter<Policy>::readStructSet(uint32_t* typeIndex,
                                          uint32_t* fieldIndex, Value* ptr,
                                          Value* val) {
  MOZ_ASSERT(typeIndex != fieldIndex);
  MOZ_ASSERT(Classify(op_) == OpKind::StructSet);

  if (!readStructTypeIndex(typeIndex)) {
    return false;
  }

  const StructType& structType = env_.types[*typeIndex].structType();

  if (!readFieldIndex(fieldIndex, structType)) {
    return false;
  }

  if (!popWithType(structType.fields_[*fieldIndex].type, val)) {
    return false;
  }

  if (!structType.fields_[*fieldIndex].isMutable) {
    return fail("field is not mutable");
  }

  if (!popWithType(ValType(ValType::Ref, *typeIndex), ptr)) {
    return false;
  }

  return true;
}

template <typename Policy>
inline bool OpIter<Policy>::readStructNarrow(ValType* inputType,
                                             ValType* outputType, Value* ptr) {
  MOZ_ASSERT(inputType != outputType);
  MOZ_ASSERT(Classify(op_) == OpKind::StructNarrow);

  if (!readReferenceType(inputType, "struct.narrow")) {
    return false;
  }

  if (!readReferenceType(outputType, "struct.narrow")) {
    return false;
  }

  if (inputType->isRef()) {
    if (!outputType->isRef()) {
      return fail("invalid type combination in struct.narrow");
    }

    const StructType& inputStruct =
        env_.types[inputType->refTypeIndex()].structType();
    const StructType& outputStruct =
        env_.types[outputType->refTypeIndex()].structType();

    if (!outputStruct.hasPrefix(inputStruct)) {
      return fail("invalid narrowing operation");
    }
  } else if (*outputType == ValType::AnyRef) {
    if (*inputType != ValType::AnyRef) {
      return fail("invalid type combination in struct.narrow");
    }
  }

  if (!popWithType(*inputType, ptr)) {
    return false;
  }

  return push(*outputType);
}

}  // namespace wasm
}  // namespace js

namespace mozilla {

// Specialize IsPod for the Nothing specializations.
template <>
struct IsPod<js::wasm::TypeAndValue<Nothing>> : TrueType {};
template <>
struct IsPod<js::wasm::ControlStackEntry<Nothing>> : TrueType {};

}  // namespace mozilla

#endif  // wasm_op_iter_h
