/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_LIR_shared_h
#define jit_shared_LIR_shared_h

#include "jit/AtomicOp.h"
#include "jit/shared/Assembler-shared.h"
#include "util/Memory.h"

// This file declares LIR instructions that are common to every platform.

namespace js {
namespace jit {

LIR_OPCODE_CLASS_GENERATED

class LBox : public LInstructionHelper<BOX_PIECES, 1, 0> {
  MIRType type_;

 public:
  LIR_HEADER(Box);

  LBox(const LAllocation& payload, MIRType type)
      : LInstructionHelper(classOpcode), type_(type) {
    setOperand(0, payload);
  }

  MIRType type() const { return type_; }
  const char* extraName() const { return StringFromMIRType(type_); }
};

template <size_t Temps, size_t ExtraUses = 0>
class LBinaryMath : public LInstructionHelper<1, 2 + ExtraUses, Temps> {
 protected:
  explicit LBinaryMath(LNode::Opcode opcode)
      : LInstructionHelper<1, 2 + ExtraUses, Temps>(opcode) {}

 public:
  const LAllocation* lhs() { return this->getOperand(0); }
  const LAllocation* rhs() { return this->getOperand(1); }
};

template <size_t Temps, size_t ExtraUses = 0>
class LUnaryMath : public LInstructionHelper<1, 1 + ExtraUses, Temps> {
 protected:
  explicit LUnaryMath(LNode::Opcode opcode)
      : LInstructionHelper<1, 1 + ExtraUses, Temps>(opcode) {}

 public:
  const LAllocation* input() { return this->getOperand(0); }
};

// An LOsiPoint captures a snapshot after a call and ensures enough space to
// patch in a call to the invalidation mechanism.
//
// Note: LSafepoints are 1:1 with LOsiPoints, so it holds a reference to the
// corresponding LSafepoint to inform it of the LOsiPoint's masm offset when it
// gets GC'd.
class LOsiPoint : public LInstructionHelper<0, 0, 0> {
  LSafepoint* safepoint_;

 public:
  LOsiPoint(LSafepoint* safepoint, LSnapshot* snapshot)
      : LInstructionHelper(classOpcode), safepoint_(safepoint) {
    MOZ_ASSERT(safepoint && snapshot);
    assignSnapshot(snapshot);
  }

  LSafepoint* associatedSafepoint() { return safepoint_; }

  LIR_HEADER(OsiPoint)
};

class LMove {
  LAllocation from_;
  LAllocation to_;
  LDefinition::Type type_;

 public:
  LMove(LAllocation from, LAllocation to, LDefinition::Type type)
      : from_(from), to_(to), type_(type) {}

  LAllocation from() const { return from_; }
  LAllocation to() const { return to_; }
  LDefinition::Type type() const { return type_; }
};

class LMoveGroup : public LInstructionHelper<0, 0, 0> {
  js::Vector<LMove, 2, JitAllocPolicy> moves_;

#ifdef JS_CODEGEN_X86
  // Optional general register available for use when executing moves.
  LAllocation scratchRegister_;
#endif

  explicit LMoveGroup(TempAllocator& alloc)
      : LInstructionHelper(classOpcode), moves_(alloc) {}

 public:
  LIR_HEADER(MoveGroup)

  static LMoveGroup* New(TempAllocator& alloc) {
    return new (alloc) LMoveGroup(alloc);
  }

  void printOperands(GenericPrinter& out);

  // Add a move which takes place simultaneously with all others in the group.
  bool add(LAllocation from, LAllocation to, LDefinition::Type type);

  // Add a move which takes place after existing moves in the group.
  bool addAfter(LAllocation from, LAllocation to, LDefinition::Type type);

  size_t numMoves() const { return moves_.length(); }
  const LMove& getMove(size_t i) const { return moves_[i]; }

#ifdef JS_CODEGEN_X86
  void setScratchRegister(Register reg) { scratchRegister_ = LGeneralReg(reg); }
  LAllocation maybeScratchRegister() { return scratchRegister_; }
#endif

  bool uses(Register reg) {
    for (size_t i = 0; i < numMoves(); i++) {
      LMove move = getMove(i);
      if (move.from() == LGeneralReg(reg) || move.to() == LGeneralReg(reg)) {
        return true;
      }
    }
    return false;
  }
};

// Constant 32-bit integer.
class LInteger : public LInstructionHelper<1, 0, 0> {
  int32_t i32_;

 public:
  LIR_HEADER(Integer)

  explicit LInteger(int32_t i32) : LInstructionHelper(classOpcode), i32_(i32) {}

  int32_t getValue() const { return i32_; }
};

// Constant 64-bit integer.
class LInteger64 : public LInstructionHelper<INT64_PIECES, 0, 0> {
  int64_t i64_;

 public:
  LIR_HEADER(Integer64)

  explicit LInteger64(int64_t i64)
      : LInstructionHelper(classOpcode), i64_(i64) {}

  int64_t getValue() const { return i64_; }
};

// Constant pointer.
class LPointer : public LInstructionHelper<1, 0, 0> {
  gc::Cell* ptr_;

 public:
  LIR_HEADER(Pointer)

  explicit LPointer(gc::Cell* ptr)
      : LInstructionHelper(classOpcode), ptr_(ptr) {}

  gc::Cell* gcptr() const { return ptr_; }
};

// Constant double.
class LDouble : public LInstructionHelper<1, 0, 0> {
  double d_;

 public:
  LIR_HEADER(Double);

  explicit LDouble(double d) : LInstructionHelper(classOpcode), d_(d) {}

  const double& getDouble() const { return d_; }
};

// Constant float32.
class LFloat32 : public LInstructionHelper<1, 0, 0> {
  float f_;

 public:
  LIR_HEADER(Float32);

  explicit LFloat32(float f) : LInstructionHelper(classOpcode), f_(f) {}

  const float& getFloat() const { return f_; }
};

// A constant Value.
class LValue : public LInstructionHelper<BOX_PIECES, 0, 0> {
  Value v_;

 public:
  LIR_HEADER(Value)

  explicit LValue(const Value& v) : LInstructionHelper(classOpcode), v_(v) {}

  Value value() const { return v_; }
};

// Formal argument for a function, returning a box. Formal arguments are
// initially read from the stack.
class LParameter : public LInstructionHelper<BOX_PIECES, 0, 0> {
 public:
  LIR_HEADER(Parameter)

  LParameter() : LInstructionHelper(classOpcode) {}
};

// Base class for control instructions (goto, branch, etc.)
template <size_t Succs, size_t Operands, size_t Temps>
class LControlInstructionHelper
    : public LInstructionHelper<0, Operands, Temps> {
  mozilla::Array<MBasicBlock*, Succs> successors_;

 protected:
  explicit LControlInstructionHelper(LNode::Opcode opcode)
      : LInstructionHelper<0, Operands, Temps>(opcode) {}

 public:
  size_t numSuccessors() const { return Succs; }
  MBasicBlock* getSuccessor(size_t i) const { return successors_[i]; }

  void setSuccessor(size_t i, MBasicBlock* successor) {
    successors_[i] = successor;
  }
};

// Jumps to the start of a basic block.
class LGoto : public LControlInstructionHelper<1, 0, 0> {
 public:
  LIR_HEADER(Goto)

  explicit LGoto(MBasicBlock* block) : LControlInstructionHelper(classOpcode) {
    setSuccessor(0, block);
  }

  MBasicBlock* target() const { return getSuccessor(0); }
};

class LNewArray : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(NewArray)

  explicit LNewArray(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  const char* extraName() const {
    return mir()->isVMCall() ? "VMCall" : nullptr;
  }

  const LDefinition* temp() { return getTemp(0); }

  MNewArray* mir() const { return mir_->toNewArray(); }
};

class LNewArrayDynamicLength : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(NewArrayDynamicLength)

  explicit LNewArrayDynamicLength(const LAllocation& length,
                                  const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, length);
    setTemp(0, temp);
  }

  const LAllocation* length() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  MNewArrayDynamicLength* mir() const {
    return mir_->toNewArrayDynamicLength();
  }
};

class LNewIterator : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(NewIterator)

  explicit LNewIterator(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }

  MNewIterator* mir() const { return mir_->toNewIterator(); }
};

class LNewTypedArray : public LInstructionHelper<1, 0, 2> {
 public:
  LIR_HEADER(NewTypedArray)

  LNewTypedArray(const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }

  const LDefinition* temp2() { return getTemp(1); }

  MNewTypedArray* mir() const { return mir_->toNewTypedArray(); }
};

class LNewTypedArrayDynamicLength : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(NewTypedArrayDynamicLength)

  LNewTypedArrayDynamicLength(const LAllocation& length,
                              const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, length);
    setTemp(0, temp);
  }

  const LAllocation* length() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  MNewTypedArrayDynamicLength* mir() const {
    return mir_->toNewTypedArrayDynamicLength();
  }
};

class LNewObject : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(NewObject)

  explicit LNewObject(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  const char* extraName() const {
    return mir()->isVMCall() ? "VMCall" : nullptr;
  }

  const LDefinition* temp() { return getTemp(0); }

  MNewObject* mir() const { return mir_->toNewObject(); }
};

class LNewPlainObject : public LInstructionHelper<1, 0, 3> {
 public:
  LIR_HEADER(NewPlainObject)

  explicit LNewPlainObject(const LDefinition& temp0, const LDefinition& temp1,
                           const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
  const LDefinition* temp2() { return getTemp(2); }

  MNewPlainObject* mir() const { return mir_->toNewPlainObject(); }
};

class LNewArrayObject : public LInstructionHelper<1, 0, 2> {
 public:
  LIR_HEADER(NewArrayObject)

  explicit LNewArrayObject(const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }

  MNewArrayObject* mir() const { return mir_->toNewArrayObject(); }
};

// Allocates a new NamedLambdaObject.
//
// This instruction generates two possible instruction sets:
//   (1) An inline allocation of the call object is attempted.
//   (2) Otherwise, a callVM create a new object.
//
class LNewNamedLambdaObject : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(NewNamedLambdaObject);

  explicit LNewNamedLambdaObject(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }

  MNewNamedLambdaObject* mir() const { return mir_->toNewNamedLambdaObject(); }
};

// Allocates a new CallObject.
//
// This instruction generates two possible instruction sets:
//   (1) If the call object is extensible, this is a callVM to create the
//       call object.
//   (2) Otherwise, an inline allocation of the call object is attempted.
//
class LNewCallObject : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(NewCallObject)

  explicit LNewCallObject(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }

  MNewCallObject* mir() const { return mir_->toNewCallObject(); }
};

class LNewStringObject : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(NewStringObject)

  LNewStringObject(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  MNewStringObject* mir() const { return mir_->toNewStringObject(); }
};

class LInitElemGetterSetter
    : public LCallInstructionHelper<0, 2 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(InitElemGetterSetter)

  LInitElemGetterSetter(const LAllocation& object, const LBoxAllocation& id,
                        const LAllocation& value)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, value);
    setBoxOperand(IdIndex, id);
  }

  static const size_t IdIndex = 2;

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  MInitElemGetterSetter* mir() const { return mir_->toInitElemGetterSetter(); }
};

template <size_t Defs, size_t Ops>
class LWasmReinterpretBase : public LInstructionHelper<Defs, Ops, 0> {
  typedef LInstructionHelper<Defs, Ops, 0> Base;

 protected:
  explicit LWasmReinterpretBase(LNode::Opcode opcode) : Base(opcode) {}

 public:
  const LAllocation* input() { return Base::getOperand(0); }
  MWasmReinterpret* mir() const { return Base::mir_->toWasmReinterpret(); }
};

class LWasmReinterpret : public LWasmReinterpretBase<1, 1> {
 public:
  LIR_HEADER(WasmReinterpret);
  explicit LWasmReinterpret(const LAllocation& input)
      : LWasmReinterpretBase(classOpcode) {
    setOperand(0, input);
  }
};

class LWasmReinterpretFromI64 : public LWasmReinterpretBase<1, INT64_PIECES> {
 public:
  static const size_t Input = 0;

  LIR_HEADER(WasmReinterpretFromI64);
  explicit LWasmReinterpretFromI64(const LInt64Allocation& input)
      : LWasmReinterpretBase(classOpcode) {
    setInt64Operand(Input, input);
  }
};

class LWasmReinterpretToI64 : public LWasmReinterpretBase<INT64_PIECES, 1> {
 public:
  LIR_HEADER(WasmReinterpretToI64);
  explicit LWasmReinterpretToI64(const LAllocation& input)
      : LWasmReinterpretBase(classOpcode) {
    setOperand(0, input);
  }
};

namespace details {
template <size_t Defs, size_t Ops, size_t Temps>
class RotateBase : public LInstructionHelper<Defs, Ops, Temps> {
  typedef LInstructionHelper<Defs, Ops, Temps> Base;

 protected:
  explicit RotateBase(LNode::Opcode opcode) : Base(opcode) {}

 public:
  MRotate* mir() { return Base::mir_->toRotate(); }
};
}  // namespace details

class LRotate : public details::RotateBase<1, 2, 0> {
 public:
  LIR_HEADER(Rotate);

  LRotate() : RotateBase(classOpcode) {}

  const LAllocation* input() { return getOperand(0); }
  LAllocation* count() { return getOperand(1); }
};

class LRotateI64
    : public details::RotateBase<INT64_PIECES, INT64_PIECES + 1, 1> {
 public:
  LIR_HEADER(RotateI64);

  LRotateI64() : RotateBase(classOpcode) {
    setTemp(0, LDefinition::BogusTemp());
  }

  static const size_t Input = 0;
  static const size_t Count = INT64_PIECES;

  const LInt64Allocation input() { return getInt64Operand(Input); }
  const LDefinition* temp() { return getTemp(0); }
  LAllocation* count() { return getOperand(Count); }
};

class LTypeOfV : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(TypeOfV)

  LTypeOfV(const LBoxAllocation& input, const LDefinition& tempToUnbox)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, tempToUnbox);
  }

  static const size_t Input = 0;

  const LDefinition* tempToUnbox() { return getTemp(0); }

  MTypeOf* mir() const { return mir_->toTypeOf(); }
};

class LTypeOfO : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(TypeOfO)

  explicit LTypeOfO(const LAllocation& obj) : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
  }

  const LAllocation* object() { return getOperand(0); }

  MTypeOf* mir() const { return mir_->toTypeOf(); }
};

class LToPropertyKeyCache
    : public LInstructionHelper<BOX_PIECES, BOX_PIECES, 0> {
 public:
  LIR_HEADER(ToPropertyKeyCache)

  explicit LToPropertyKeyCache(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  const MToPropertyKeyCache* mir() const {
    return mir_->toToPropertyKeyCache();
  }

  const LAllocation* input() { return getOperand(Input); }

  static const size_t Input = 0;
};

// Allocate an object for |new| on the caller-side,
// when there is no templateObject or prototype known
class LCreateThis : public LCallInstructionHelper<BOX_PIECES, 2, 0> {
 public:
  LIR_HEADER(CreateThis)

  LCreateThis(const LAllocation& callee, const LAllocation& newTarget)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, callee);
    setOperand(1, newTarget);
  }

  const LAllocation* getCallee() { return getOperand(0); }
  const LAllocation* getNewTarget() { return getOperand(1); }

  MCreateThis* mir() const { return mir_->toCreateThis(); }
};

// Allocate an object for |new| on the caller-side.
// Always performs object initialization with a fast path.
class LCreateThisWithTemplate : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(CreateThisWithTemplate)

  explicit LCreateThisWithTemplate(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  MCreateThisWithTemplate* mir() const {
    return mir_->toCreateThisWithTemplate();
  }

  const LDefinition* temp() { return getTemp(0); }
};

// Allocate a new arguments object for the frame.
class LCreateArgumentsObject : public LCallInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(CreateArgumentsObject)

  LCreateArgumentsObject(const LAllocation& callObj, const LDefinition& temp0,
                         const LDefinition& temp1, const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, callObj);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
  const LDefinition* temp2() { return getTemp(2); }

  const LAllocation* getCallObject() { return getOperand(0); }

  MCreateArgumentsObject* mir() const {
    return mir_->toCreateArgumentsObject();
  }
};

// Allocate a new arguments object for an inlined frame.
class LCreateInlinedArgumentsObject : public LVariadicInstruction<1, 1> {
 public:
  LIR_HEADER(CreateInlinedArgumentsObject)

  static const size_t CallObj = 0;
  static const size_t Callee = 1;
  static const size_t NumNonArgumentOperands = 2;
  static size_t ArgIndex(size_t i) {
    return NumNonArgumentOperands + BOX_PIECES * i;
  }

  LCreateInlinedArgumentsObject(uint32_t numOperands, const LDefinition& temp)
      : LVariadicInstruction(classOpcode, numOperands) {
    setIsCall();
    setTemp(0, temp);
  }

  const LAllocation* getCallObject() { return getOperand(CallObj); }
  const LAllocation* getCallee() { return getOperand(Callee); }

  const LDefinition* temp() { return getTemp(0); }

  MCreateInlinedArgumentsObject* mir() const {
    return mir_->toCreateInlinedArgumentsObject();
  }
};

class LGetInlinedArgument : public LVariadicInstruction<BOX_PIECES, 0> {
 public:
  LIR_HEADER(GetInlinedArgument)

  static const size_t Index = 0;
  static const size_t NumNonArgumentOperands = 1;
  static size_t ArgIndex(size_t i) {
    return NumNonArgumentOperands + BOX_PIECES * i;
  }

  explicit LGetInlinedArgument(uint32_t numOperands)
      : LVariadicInstruction(classOpcode, numOperands) {}

  const LAllocation* getIndex() { return getOperand(Index); }

  MGetInlinedArgument* mir() const { return mir_->toGetInlinedArgument(); }
};

// Get argument from arguments object.
class LGetArgumentsObjectArg : public LInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(GetArgumentsObjectArg)

  LGetArgumentsObjectArg(const LAllocation& argsObj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, argsObj);
    setTemp(0, temp);
  }

  const LAllocation* getArgsObject() { return getOperand(0); }

  MGetArgumentsObjectArg* mir() const {
    return mir_->toGetArgumentsObjectArg();
  }
};

// Set argument on arguments object.
class LSetArgumentsObjectArg : public LInstructionHelper<0, 1 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(SetArgumentsObjectArg)

  LSetArgumentsObjectArg(const LAllocation& argsObj,
                         const LBoxAllocation& value, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, argsObj);
    setBoxOperand(ValueIndex, value);
    setTemp(0, temp);
  }

  const LAllocation* getArgsObject() { return getOperand(0); }

  MSetArgumentsObjectArg* mir() const {
    return mir_->toSetArgumentsObjectArg();
  }

  static const size_t ValueIndex = 1;
};

// Load an element from an arguments object.
class LLoadArgumentsObjectArg : public LInstructionHelper<BOX_PIECES, 2, 1> {
 public:
  LIR_HEADER(LoadArgumentsObjectArg)

  LLoadArgumentsObjectArg(const LAllocation& argsObj, const LAllocation& index,
                          const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, argsObj);
    setOperand(1, index);
    setTemp(0, temp);
  }

  const LAllocation* getArgsObject() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return this->getTemp(0); }
};

// Guard that the given flags are not set on the arguments object.
class LGuardArgumentsObjectFlags : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardArgumentsObjectFlags)

  explicit LGuardArgumentsObjectFlags(const LAllocation& argsObj,
                                      const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, argsObj);
    setTemp(0, temp);
  }

  const LAllocation* getArgsObject() { return getOperand(0); }
  const LDefinition* temp() { return this->getTemp(0); }

  MGuardArgumentsObjectFlags* mir() const {
    return mir_->toGuardArgumentsObjectFlags();
  }
};

class LImplicitThis : public LCallInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(ImplicitThis)

  explicit LImplicitThis(const LAllocation& env)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, env);
  }

  const LAllocation* env() { return getOperand(0); }

  MImplicitThis* mir() const { return mir_->toImplicitThis(); }
};

// Writes a typed argument for a function call to the frame's argument vector.
class LStackArgT : public LInstructionHelper<0, 1, 0> {
  uint32_t argslot_;  // Index into frame-scope argument vector.
  MIRType type_;

 public:
  LIR_HEADER(StackArgT)

  LStackArgT(uint32_t argslot, MIRType type, const LAllocation& arg)
      : LInstructionHelper(classOpcode), argslot_(argslot), type_(type) {
    setOperand(0, arg);
  }
  uint32_t argslot() const { return argslot_; }
  MIRType type() const { return type_; }
  const LAllocation* getArgument() { return getOperand(0); }
};

// Writes an untyped argument for a function call to the frame's argument
// vector.
class LStackArgV : public LInstructionHelper<0, BOX_PIECES, 0> {
  uint32_t argslot_;  // Index into frame-scope argument vector.

 public:
  LIR_HEADER(StackArgV)

  LStackArgV(uint32_t argslot, const LBoxAllocation& value)
      : LInstructionHelper(classOpcode), argslot_(argslot) {
    setBoxOperand(0, value);
  }

  uint32_t argslot() const { return argslot_; }
};

// Common code for LIR descended from MCall.
template <size_t Defs, size_t Operands, size_t Temps>
class LJSCallInstructionHelper
    : public LCallInstructionHelper<Defs, Operands, Temps> {
 protected:
  explicit LJSCallInstructionHelper(LNode::Opcode opcode)
      : LCallInstructionHelper<Defs, Operands, Temps>(opcode) {}

 public:
  uint32_t argslot() const {
    if (JitStackValueAlignment > 1) {
      return AlignBytes(mir()->numStackArgs(), JitStackValueAlignment);
    }
    return mir()->numStackArgs();
  }
  MCall* mir() const { return this->mir_->toCall(); }

  bool hasSingleTarget() const { return getSingleTarget() != nullptr; }
  WrappedFunction* getSingleTarget() const { return mir()->getSingleTarget(); }

  // Does not include |this|.
  uint32_t numActualArgs() const { return mir()->numActualArgs(); }

  bool isConstructing() const { return mir()->isConstructing(); }
  bool ignoresReturnValue() const { return mir()->ignoresReturnValue(); }
};

// Generates a polymorphic callsite, wherein the function being called is
// unknown and anticipated to vary.
class LCallGeneric : public LJSCallInstructionHelper<BOX_PIECES, 1, 2> {
 public:
  LIR_HEADER(CallGeneric)

  LCallGeneric(const LAllocation& func, const LDefinition& nargsreg,
               const LDefinition& tmpobjreg)
      : LJSCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setTemp(0, nargsreg);
    setTemp(1, tmpobjreg);
  }

  const LAllocation* getFunction() { return getOperand(0); }
  const LDefinition* getNargsReg() { return getTemp(0); }
  const LDefinition* getTempObject() { return getTemp(1); }
};

// Generates a hardcoded callsite for a known, non-native target.
class LCallKnown : public LJSCallInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(CallKnown)

  LCallKnown(const LAllocation& func, const LDefinition& tmpobjreg)
      : LJSCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setTemp(0, tmpobjreg);
  }

  const LAllocation* getFunction() { return getOperand(0); }
  const LDefinition* getTempObject() { return getTemp(0); }
};

// Generates a hardcoded callsite for a known, native target.
class LCallNative : public LJSCallInstructionHelper<BOX_PIECES, 0, 4> {
 public:
  LIR_HEADER(CallNative)

  LCallNative(const LDefinition& argContext, const LDefinition& argUintN,
              const LDefinition& argVp, const LDefinition& tmpreg)
      : LJSCallInstructionHelper(classOpcode) {
    // Registers used for callWithABI().
    setTemp(0, argContext);
    setTemp(1, argUintN);
    setTemp(2, argVp);

    // Temporary registers.
    setTemp(3, tmpreg);
  }

  const LDefinition* getArgContextReg() { return getTemp(0); }
  const LDefinition* getArgUintNReg() { return getTemp(1); }
  const LDefinition* getArgVpReg() { return getTemp(2); }
  const LDefinition* getTempReg() { return getTemp(3); }
};

// Generates a hardcoded callsite for a known, DOM-native target.
class LCallDOMNative : public LJSCallInstructionHelper<BOX_PIECES, 0, 4> {
 public:
  LIR_HEADER(CallDOMNative)

  LCallDOMNative(const LDefinition& argJSContext, const LDefinition& argObj,
                 const LDefinition& argPrivate, const LDefinition& argArgs)
      : LJSCallInstructionHelper(classOpcode) {
    setTemp(0, argJSContext);
    setTemp(1, argObj);
    setTemp(2, argPrivate);
    setTemp(3, argArgs);
  }

  const LDefinition* getArgJSContext() { return getTemp(0); }
  const LDefinition* getArgObj() { return getTemp(1); }
  const LDefinition* getArgPrivate() { return getTemp(2); }
  const LDefinition* getArgArgs() { return getTemp(3); }
};

class LUnreachable : public LControlInstructionHelper<0, 0, 0> {
 public:
  LIR_HEADER(Unreachable)

  LUnreachable() : LControlInstructionHelper(classOpcode) {}
};

class LUnreachableResultV : public LInstructionHelper<BOX_PIECES, 0, 0> {
 public:
  LIR_HEADER(UnreachableResultV)

  LUnreachableResultV() : LInstructionHelper(classOpcode) {}
};

template <size_t defs, size_t ops>
class LDOMPropertyInstructionHelper
    : public LCallInstructionHelper<defs, 1 + ops, 3> {
 protected:
  LDOMPropertyInstructionHelper(LNode::Opcode opcode,
                                const LDefinition& JSContextReg,
                                const LAllocation& ObjectReg,
                                const LDefinition& PrivReg,
                                const LDefinition& ValueReg)
      : LCallInstructionHelper<defs, 1 + ops, 3>(opcode) {
    this->setOperand(0, ObjectReg);
    this->setTemp(0, JSContextReg);
    this->setTemp(1, PrivReg);
    this->setTemp(2, ValueReg);
  }

 public:
  const LDefinition* getJSContextReg() { return this->getTemp(0); }
  const LAllocation* getObjectReg() { return this->getOperand(0); }
  const LDefinition* getPrivReg() { return this->getTemp(1); }
  const LDefinition* getValueReg() { return this->getTemp(2); }
};

class LGetDOMProperty : public LDOMPropertyInstructionHelper<BOX_PIECES, 0> {
 public:
  LIR_HEADER(GetDOMProperty)

  LGetDOMProperty(const LDefinition& JSContextReg, const LAllocation& ObjectReg,
                  const LDefinition& PrivReg, const LDefinition& ValueReg)
      : LDOMPropertyInstructionHelper<BOX_PIECES, 0>(
            classOpcode, JSContextReg, ObjectReg, PrivReg, ValueReg) {}

  MGetDOMProperty* mir() const { return mir_->toGetDOMProperty(); }
};

class LGetDOMMemberV : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(GetDOMMemberV);
  explicit LGetDOMMemberV(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }

  const LAllocation* object() { return getOperand(0); }

  MGetDOMMember* mir() const { return mir_->toGetDOMMember(); }
};

class LGetDOMMemberT : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(GetDOMMemberT);
  explicit LGetDOMMemberT(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }

  const LAllocation* object() { return getOperand(0); }

  MGetDOMMember* mir() const { return mir_->toGetDOMMember(); }
};

class LSetDOMProperty : public LDOMPropertyInstructionHelper<0, BOX_PIECES> {
 public:
  LIR_HEADER(SetDOMProperty)

  LSetDOMProperty(const LDefinition& JSContextReg, const LAllocation& ObjectReg,
                  const LBoxAllocation& value, const LDefinition& PrivReg,
                  const LDefinition& ValueReg)
      : LDOMPropertyInstructionHelper<0, BOX_PIECES>(
            classOpcode, JSContextReg, ObjectReg, PrivReg, ValueReg) {
    setBoxOperand(Value, value);
  }

  static const size_t Value = 1;

  MSetDOMProperty* mir() const { return mir_->toSetDOMProperty(); }
};

class LLoadDOMExpandoValue : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(LoadDOMExpandoValue)

  explicit LLoadDOMExpandoValue(const LAllocation& proxy)
      : LInstructionHelper(classOpcode) {
    setOperand(0, proxy);
  }

  const LAllocation* proxy() { return getOperand(0); }

  const MLoadDOMExpandoValue* mir() const {
    return mir_->toLoadDOMExpandoValue();
  }
};

class LLoadDOMExpandoValueGuardGeneration
    : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(LoadDOMExpandoValueGuardGeneration)

  explicit LLoadDOMExpandoValueGuardGeneration(const LAllocation& proxy)
      : LInstructionHelper(classOpcode) {
    setOperand(0, proxy);
  }

  const LAllocation* proxy() { return getOperand(0); }

  const MLoadDOMExpandoValueGuardGeneration* mir() const {
    return mir_->toLoadDOMExpandoValueGuardGeneration();
  }
};

class LLoadDOMExpandoValueIgnoreGeneration
    : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(LoadDOMExpandoValueIgnoreGeneration)

  explicit LLoadDOMExpandoValueIgnoreGeneration(const LAllocation& proxy)
      : LInstructionHelper(classOpcode) {
    setOperand(0, proxy);
  }

  const LAllocation* proxy() { return getOperand(0); }
};

class LGuardDOMExpandoMissingOrGuardShape
    : public LInstructionHelper<0, BOX_PIECES, 1> {
 public:
  LIR_HEADER(GuardDOMExpandoMissingOrGuardShape)

  explicit LGuardDOMExpandoMissingOrGuardShape(const LBoxAllocation& input,
                                               const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }

  static const size_t Input = 0;

  MGuardDOMExpandoMissingOrGuardShape* mir() {
    return mir_->toGuardDOMExpandoMissingOrGuardShape();
  }
};

// Generates a polymorphic callsite, wherein the function being called is
// unknown and anticipated to vary.
class LApplyArgsGeneric
    : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES + 2, 2> {
 public:
  LIR_HEADER(ApplyArgsGeneric)

  LApplyArgsGeneric(const LAllocation& func, const LAllocation& argc,
                    const LBoxAllocation& thisv, const LDefinition& tmpobjreg,
                    const LDefinition& tmpcopy)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setOperand(1, argc);
    setBoxOperand(ThisIndex, thisv);
    setTemp(0, tmpobjreg);
    setTemp(1, tmpcopy);
  }

  MApplyArgs* mir() const { return mir_->toApplyArgs(); }

  bool hasSingleTarget() const { return getSingleTarget() != nullptr; }
  WrappedFunction* getSingleTarget() const { return mir()->getSingleTarget(); }

  const LAllocation* getFunction() { return getOperand(0); }
  const LAllocation* getArgc() { return getOperand(1); }
  static const size_t ThisIndex = 2;

  const LDefinition* getTempObject() { return getTemp(0); }
  const LDefinition* getTempStackCounter() { return getTemp(1); }
};

class LApplyArgsObj
    : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES + 2, 2> {
 public:
  LIR_HEADER(ApplyArgsObj)

  LApplyArgsObj(const LAllocation& func, const LAllocation& argsObj,
                const LBoxAllocation& thisv, const LDefinition& tmpObjReg,
                const LDefinition& tmpCopy)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setOperand(1, argsObj);
    setBoxOperand(ThisIndex, thisv);
    setTemp(0, tmpObjReg);
    setTemp(1, tmpCopy);
  }

  MApplyArgsObj* mir() const { return mir_->toApplyArgsObj(); }

  bool hasSingleTarget() const { return getSingleTarget() != nullptr; }
  WrappedFunction* getSingleTarget() const { return mir()->getSingleTarget(); }

  const LAllocation* getFunction() { return getOperand(0); }
  const LAllocation* getArgsObj() { return getOperand(1); }
  // All registers are calltemps. argc is mapped to the same register as
  // ArgsObj. argc becomes live as ArgsObj is dying.
  const LAllocation* getArgc() { return getOperand(1); }
  static const size_t ThisIndex = 2;

  const LDefinition* getTempObject() { return getTemp(0); }
  const LDefinition* getTempStackCounter() { return getTemp(1); }
};

class LApplyArrayGeneric
    : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES + 2, 2> {
 public:
  LIR_HEADER(ApplyArrayGeneric)

  LApplyArrayGeneric(const LAllocation& func, const LAllocation& elements,
                     const LBoxAllocation& thisv, const LDefinition& tmpobjreg,
                     const LDefinition& tmpcopy)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setOperand(1, elements);
    setBoxOperand(ThisIndex, thisv);
    setTemp(0, tmpobjreg);
    setTemp(1, tmpcopy);
  }

  MApplyArray* mir() const { return mir_->toApplyArray(); }

  bool hasSingleTarget() const { return getSingleTarget() != nullptr; }
  WrappedFunction* getSingleTarget() const { return mir()->getSingleTarget(); }

  const LAllocation* getFunction() { return getOperand(0); }
  const LAllocation* getElements() { return getOperand(1); }
  // argc is mapped to the same register as elements: argc becomes
  // live as elements is dying, all registers are calltemps.
  const LAllocation* getArgc() { return getOperand(1); }
  static const size_t ThisIndex = 2;

  const LDefinition* getTempObject() { return getTemp(0); }
  const LDefinition* getTempStackCounter() { return getTemp(1); }
};

class LConstructArrayGeneric
    : public LCallInstructionHelper<BOX_PIECES, BOX_PIECES + 3, 1> {
 public:
  LIR_HEADER(ConstructArrayGeneric)

  LConstructArrayGeneric(const LAllocation& func, const LAllocation& elements,
                         const LAllocation& newTarget,
                         const LBoxAllocation& thisv,
                         const LDefinition& tmpobjreg)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, func);
    setOperand(1, elements);
    setOperand(2, newTarget);
    setBoxOperand(ThisIndex, thisv);
    setTemp(0, tmpobjreg);
  }

  MConstructArray* mir() const { return mir_->toConstructArray(); }

  bool hasSingleTarget() const { return getSingleTarget() != nullptr; }
  WrappedFunction* getSingleTarget() const { return mir()->getSingleTarget(); }

  const LAllocation* getFunction() { return getOperand(0); }
  const LAllocation* getElements() { return getOperand(1); }
  const LAllocation* getNewTarget() { return getOperand(2); }

  static const size_t ThisIndex = 3;

  const LDefinition* getTempObject() { return getTemp(0); }

  // argc is mapped to the same register as elements: argc becomes
  // live as elements is dying, all registers are calltemps.
  const LAllocation* getArgc() { return getOperand(1); }

  // tempStackCounter is mapped to the same register as newTarget:
  // tempStackCounter becomes live as newTarget is dying, all registers are
  // calltemps.
  const LAllocation* getTempStackCounter() { return getOperand(2); }
};

// Takes in either an integer or boolean input and tests it for truthiness.
class LTestIAndBranch : public LControlInstructionHelper<2, 1, 0> {
 public:
  LIR_HEADER(TestIAndBranch)

  LTestIAndBranch(const LAllocation& in, MBasicBlock* ifTrue,
                  MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode) {
    setOperand(0, in);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

// Takes in an int64 input and tests it for truthiness.
class LTestI64AndBranch : public LControlInstructionHelper<2, INT64_PIECES, 0> {
 public:
  LIR_HEADER(TestI64AndBranch)

  LTestI64AndBranch(const LInt64Allocation& in, MBasicBlock* ifTrue,
                    MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode) {
    setInt64Operand(0, in);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

// Takes in a double input and tests it for truthiness.
class LTestDAndBranch : public LControlInstructionHelper<2, 1, 0> {
 public:
  LIR_HEADER(TestDAndBranch)

  LTestDAndBranch(const LAllocation& in, MBasicBlock* ifTrue,
                  MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode) {
    setOperand(0, in);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

// Takes in a float32 input and tests it for truthiness.
class LTestFAndBranch : public LControlInstructionHelper<2, 1, 0> {
 public:
  LIR_HEADER(TestFAndBranch)

  LTestFAndBranch(const LAllocation& in, MBasicBlock* ifTrue,
                  MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode) {
    setOperand(0, in);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

// Takes in a bigint input and tests it for truthiness.
class LTestBIAndBranch : public LControlInstructionHelper<2, 1, 0> {
 public:
  LIR_HEADER(TestBIAndBranch)

  LTestBIAndBranch(const LAllocation& in, MBasicBlock* ifTrue,
                   MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode) {
    setOperand(0, in);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() { return getSuccessor(0); }
  MBasicBlock* ifFalse() { return getSuccessor(1); }
};

// Takes an object and tests it for truthiness.  An object is falsy iff it
// emulates |undefined|; see js::EmulatesUndefined.
class LTestOAndBranch : public LControlInstructionHelper<2, 1, 1> {
 public:
  LIR_HEADER(TestOAndBranch)

  LTestOAndBranch(const LAllocation& input, MBasicBlock* ifTruthy,
                  MBasicBlock* ifFalsy, const LDefinition& temp)
      : LControlInstructionHelper(classOpcode) {
    setOperand(0, input);
    setSuccessor(0, ifTruthy);
    setSuccessor(1, ifFalsy);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }

  MBasicBlock* ifTruthy() { return getSuccessor(0); }
  MBasicBlock* ifFalsy() { return getSuccessor(1); }

  MTest* mir() { return mir_->toTest(); }
};

// Takes in a boxed value and tests it for truthiness.
class LTestVAndBranch : public LControlInstructionHelper<2, BOX_PIECES, 3> {
 public:
  LIR_HEADER(TestVAndBranch)

  LTestVAndBranch(MBasicBlock* ifTruthy, MBasicBlock* ifFalsy,
                  const LBoxAllocation& input, const LDefinition& temp0,
                  const LDefinition& temp1, const LDefinition& temp2)
      : LControlInstructionHelper(classOpcode) {
    setSuccessor(0, ifTruthy);
    setSuccessor(1, ifFalsy);
    setBoxOperand(Input, input);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  static const size_t Input = 0;

  const LDefinition* tempFloat() { return getTemp(0); }

  const LDefinition* temp1() { return getTemp(1); }

  const LDefinition* temp2() { return getTemp(2); }

  MBasicBlock* ifTruthy() { return getSuccessor(0); }
  MBasicBlock* ifFalsy() { return getSuccessor(1); }

  MTest* mir() const { return mir_->toTest(); }
};

// Compares two integral values of the same JS type, either integer or object.
// For objects, both operands are in registers.
class LCompare : public LInstructionHelper<1, 2, 0> {
  JSOp jsop_;

 public:
  LIR_HEADER(Compare)
  LCompare(JSOp jsop, const LAllocation& left, const LAllocation& right)
      : LInstructionHelper(classOpcode), jsop_(jsop) {
    setOperand(0, left);
    setOperand(1, right);
  }

  JSOp jsop() const { return jsop_; }
  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MCompare* mir() { return mir_->toCompare(); }
  const char* extraName() const { return CodeName(jsop_); }
};

class LCompareI64 : public LInstructionHelper<1, 2 * INT64_PIECES, 0> {
  JSOp jsop_;

 public:
  LIR_HEADER(CompareI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  LCompareI64(JSOp jsop, const LInt64Allocation& left,
              const LInt64Allocation& right)
      : LInstructionHelper(classOpcode), jsop_(jsop) {
    setInt64Operand(Lhs, left);
    setInt64Operand(Rhs, right);
  }

  JSOp jsop() const { return jsop_; }
  MCompare* mir() { return mir_->toCompare(); }
  const char* extraName() const { return CodeName(jsop_); }
};

class LCompareI64AndBranch
    : public LControlInstructionHelper<2, 2 * INT64_PIECES, 0> {
  MCompare* cmpMir_;
  JSOp jsop_;

 public:
  LIR_HEADER(CompareI64AndBranch)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  LCompareI64AndBranch(MCompare* cmpMir, JSOp jsop,
                       const LInt64Allocation& left,
                       const LInt64Allocation& right, MBasicBlock* ifTrue,
                       MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir), jsop_(jsop) {
    setInt64Operand(Lhs, left);
    setInt64Operand(Rhs, right);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  JSOp jsop() const { return jsop_; }
  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
  const char* extraName() const { return CodeName(jsop_); }
};

// Compares two integral values of the same JS type, either integer or object.
// For objects, both operands are in registers.
class LCompareAndBranch : public LControlInstructionHelper<2, 2, 0> {
  MCompare* cmpMir_;
  JSOp jsop_;

 public:
  LIR_HEADER(CompareAndBranch)
  LCompareAndBranch(MCompare* cmpMir, JSOp jsop, const LAllocation& left,
                    const LAllocation& right, MBasicBlock* ifTrue,
                    MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir), jsop_(jsop) {
    setOperand(0, left);
    setOperand(1, right);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  JSOp jsop() const { return jsop_; }
  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
  const char* extraName() const { return CodeName(jsop_); }
};

class LCompareD : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(CompareD)
  LCompareD(const LAllocation& left, const LAllocation& right)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareF : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(CompareF)

  LCompareF(const LAllocation& left, const LAllocation& right)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareDAndBranch : public LControlInstructionHelper<2, 2, 0> {
  MCompare* cmpMir_;

 public:
  LIR_HEADER(CompareDAndBranch)

  LCompareDAndBranch(MCompare* cmpMir, const LAllocation& left,
                     const LAllocation& right, MBasicBlock* ifTrue,
                     MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir) {
    setOperand(0, left);
    setOperand(1, right);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
};

class LCompareFAndBranch : public LControlInstructionHelper<2, 2, 0> {
  MCompare* cmpMir_;

 public:
  LIR_HEADER(CompareFAndBranch)
  LCompareFAndBranch(MCompare* cmpMir, const LAllocation& left,
                     const LAllocation& right, MBasicBlock* ifTrue,
                     MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir) {
    setOperand(0, left);
    setOperand(1, right);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
};

class LCompareS : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(CompareS)

  LCompareS(const LAllocation& left, const LAllocation& right)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareBigInt : public LInstructionHelper<1, 2, 3> {
 public:
  LIR_HEADER(CompareBigInt)

  LCompareBigInt(const LAllocation& left, const LAllocation& right,
                 const LDefinition& temp1, const LDefinition& temp2,
                 const LDefinition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareBigIntInt32 : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(CompareBigIntInt32)

  LCompareBigIntInt32(const LAllocation& left, const LAllocation& right,
                      const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareBigIntDouble : public LCallInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(CompareBigIntDouble)

  LCompareBigIntDouble(const LAllocation& left, const LAllocation& right,
                       const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
    setTemp(0, temp);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LCompareBigIntString : public LCallInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(CompareBigIntString)

  LCompareBigIntString(const LAllocation& left, const LAllocation& right)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  MCompare* mir() { return mir_->toCompare(); }
};

class LBitAndAndBranch : public LControlInstructionHelper<2, 2, 0> {
  Assembler::Condition cond_;

 public:
  LIR_HEADER(BitAndAndBranch)
  LBitAndAndBranch(MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                   Assembler::Condition cond = Assembler::NonZero)
      : LControlInstructionHelper(classOpcode), cond_(cond) {
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  Assembler::Condition cond() const {
    MOZ_ASSERT(cond_ == Assembler::Zero || cond_ == Assembler::NonZero);
    return cond_;
  }
};

// Takes a value and tests whether it is null, undefined, or is an object that
// emulates |undefined|, as determined by the JSCLASS_EMULATES_UNDEFINED class
// flag on unwrapped objects.  See also js::EmulatesUndefined.
class LIsNullOrLikeUndefinedV : public LInstructionHelper<1, BOX_PIECES, 2> {
 public:
  LIR_HEADER(IsNullOrLikeUndefinedV)

  LIsNullOrLikeUndefinedV(const LBoxAllocation& value, const LDefinition& temp,
                          const LDefinition& tempToUnbox)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
    setTemp(0, temp);
    setTemp(1, tempToUnbox);
  }

  static const size_t Value = 0;

  MCompare* mir() { return mir_->toCompare(); }

  const LDefinition* temp() { return getTemp(0); }

  const LDefinition* tempToUnbox() { return getTemp(1); }
};

// Takes an object pointer and tests whether it is an object that emulates
// |undefined|, as above.
class LIsNullOrLikeUndefinedT : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(IsNullOrLikeUndefinedT)

  explicit LIsNullOrLikeUndefinedT(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  MCompare* mir() { return mir_->toCompare(); }
};

class LIsNullOrLikeUndefinedAndBranchV
    : public LControlInstructionHelper<2, BOX_PIECES, 2> {
  MCompare* cmpMir_;

 public:
  LIR_HEADER(IsNullOrLikeUndefinedAndBranchV)

  LIsNullOrLikeUndefinedAndBranchV(MCompare* cmpMir, MBasicBlock* ifTrue,
                                   MBasicBlock* ifFalse,
                                   const LBoxAllocation& value,
                                   const LDefinition& temp,
                                   const LDefinition& tempToUnbox)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir) {
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
    setBoxOperand(Value, value);
    setTemp(0, temp);
    setTemp(1, tempToUnbox);
  }

  static const size_t Value = 0;

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* tempToUnbox() { return getTemp(1); }
};

class LIsNullOrLikeUndefinedAndBranchT
    : public LControlInstructionHelper<2, 1, 1> {
  MCompare* cmpMir_;

 public:
  LIR_HEADER(IsNullOrLikeUndefinedAndBranchT)

  LIsNullOrLikeUndefinedAndBranchT(MCompare* cmpMir, const LAllocation& input,
                                   MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                                   const LDefinition& temp)
      : LControlInstructionHelper(classOpcode), cmpMir_(cmpMir) {
    setOperand(0, input);
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
    setTemp(0, temp);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
  MTest* mir() const { return mir_->toTest(); }
  MCompare* cmpMir() const { return cmpMir_; }
  const LDefinition* temp() { return getTemp(0); }
};

class LSameValueDouble : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(SameValueDouble)
  LSameValueDouble(const LAllocation& left, const LAllocation& right,
                   const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, left);
    setOperand(1, right);
    setTemp(0, temp);
  }

  const LAllocation* left() { return getOperand(0); }
  const LAllocation* right() { return getOperand(1); }
  const LDefinition* tempFloat() { return getTemp(0); }
};

// Not operation on an int64.
class LNotI64 : public LInstructionHelper<1, INT64_PIECES, 0> {
 public:
  LIR_HEADER(NotI64)

  explicit LNotI64(const LInt64Allocation& input)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, input);
  }
};

// Not operation on a double.
class LNotD : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(NotD)

  explicit LNotD(const LAllocation& input) : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  MNot* mir() { return mir_->toNot(); }
};

// Not operation on a float32.
class LNotF : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(NotF)

  explicit LNotF(const LAllocation& input) : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  MNot* mir() { return mir_->toNot(); }
};

// Not operation on a BigInt.
class LNotBI : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(NotBI)

  explicit LNotBI(const LAllocation& input) : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  MNot* mir() { return mir_->toNot(); }
};

// Boolean complement operation on an object.
class LNotO : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(NotO)

  explicit LNotO(const LAllocation& input) : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  MNot* mir() { return mir_->toNot(); }
};

// Boolean complement operation on a value.
class LNotV : public LInstructionHelper<1, BOX_PIECES, 3> {
 public:
  LIR_HEADER(NotV)

  static const size_t Input = 0;
  LNotV(const LBoxAllocation& input, const LDefinition& temp0,
        const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LDefinition* tempFloat() { return getTemp(0); }

  const LDefinition* temp1() { return getTemp(1); }

  const LDefinition* temp2() { return getTemp(2); }

  MNot* mir() { return mir_->toNot(); }
};

// Bitwise not operation, takes a 32-bit integer as input and returning
// a 32-bit integer result as an output.
class LBitNotI : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(BitNotI)

  LBitNotI() : LInstructionHelper(classOpcode) {}
};

// Binary bitwise operation, taking two 32-bit integers as inputs and returning
// a 32-bit integer result as an output.
class LBitOpI : public LInstructionHelper<1, 2, 0> {
  JSOp op_;

 public:
  LIR_HEADER(BitOpI)

  explicit LBitOpI(JSOp op) : LInstructionHelper(classOpcode), op_(op) {}

  const char* extraName() const {
    if (bitop() == JSOp::Ursh && mir_->toUrsh()->bailoutsDisabled()) {
      return "ursh:BailoutsDisabled";
    }
    return CodeName(op_);
  }

  JSOp bitop() const { return op_; }
};

class LBitOpI64 : public LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0> {
  JSOp op_;

 public:
  LIR_HEADER(BitOpI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  explicit LBitOpI64(JSOp op) : LInstructionHelper(classOpcode), op_(op) {}

  const char* extraName() const { return CodeName(op_); }

  JSOp bitop() const { return op_; }
};

// Shift operation, taking two 32-bit integers as inputs and returning
// a 32-bit integer result as an output.
class LShiftI : public LBinaryMath<0> {
  JSOp op_;

 public:
  LIR_HEADER(ShiftI)

  explicit LShiftI(JSOp op) : LBinaryMath(classOpcode), op_(op) {}

  JSOp bitop() { return op_; }

  MInstruction* mir() { return mir_->toInstruction(); }

  const char* extraName() const { return CodeName(op_); }
};

class LShiftI64 : public LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 0> {
  JSOp op_;

 public:
  LIR_HEADER(ShiftI64)

  explicit LShiftI64(JSOp op) : LInstructionHelper(classOpcode), op_(op) {}

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  JSOp bitop() { return op_; }

  MInstruction* mir() { return mir_->toInstruction(); }

  const char* extraName() const { return CodeName(op_); }
};

// Sign extension
class LSignExtendInt32 : public LInstructionHelper<1, 1, 0> {
  MSignExtendInt32::Mode mode_;

 public:
  LIR_HEADER(SignExtendInt32);

  explicit LSignExtendInt32(const LAllocation& num, MSignExtendInt32::Mode mode)
      : LInstructionHelper(classOpcode), mode_(mode) {
    setOperand(0, num);
  }

  MSignExtendInt32::Mode mode() { return mode_; }
};

class LSignExtendInt64
    : public LInstructionHelper<INT64_PIECES, INT64_PIECES, 0> {
 public:
  LIR_HEADER(SignExtendInt64)

  explicit LSignExtendInt64(const LInt64Allocation& input)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, input);
  }

  const MSignExtendInt64* mir() const { return mir_->toSignExtendInt64(); }

  MSignExtendInt64::Mode mode() const { return mir()->mode(); }
};

class LUrshD : public LBinaryMath<1> {
 public:
  LIR_HEADER(UrshD)

  LUrshD(const LAllocation& lhs, const LAllocation& rhs,
         const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }
  const LDefinition* temp() { return getTemp(0); }
};

// Returns from the function being compiled (not used in inlined frames). The
// input must be a box.
class LReturn : public LInstructionHelper<0, BOX_PIECES, 0> {
  bool isGenerator_;

 public:
  LIR_HEADER(Return)

  explicit LReturn(bool isGenerator)
      : LInstructionHelper(classOpcode), isGenerator_(isGenerator) {}

  bool isGenerator() { return isGenerator_; }
};

class LMinMaxBase : public LInstructionHelper<1, 2, 0> {
 protected:
  LMinMaxBase(LNode::Opcode opcode, const LAllocation& first,
              const LAllocation& second)
      : LInstructionHelper(opcode) {
    setOperand(0, first);
    setOperand(1, second);
  }

 public:
  const LAllocation* first() { return this->getOperand(0); }
  const LAllocation* second() { return this->getOperand(1); }
  const LDefinition* output() { return this->getDef(0); }
  MMinMax* mir() const { return mir_->toMinMax(); }
  const char* extraName() const { return mir()->isMax() ? "Max" : "Min"; }
};

class LMinMaxI : public LMinMaxBase {
 public:
  LIR_HEADER(MinMaxI)
  LMinMaxI(const LAllocation& first, const LAllocation& second)
      : LMinMaxBase(classOpcode, first, second) {}
};

class LMinMaxD : public LMinMaxBase {
 public:
  LIR_HEADER(MinMaxD)
  LMinMaxD(const LAllocation& first, const LAllocation& second)
      : LMinMaxBase(classOpcode, first, second) {}
};

class LMinMaxF : public LMinMaxBase {
 public:
  LIR_HEADER(MinMaxF)
  LMinMaxF(const LAllocation& first, const LAllocation& second)
      : LMinMaxBase(classOpcode, first, second) {}
};

class LMinMaxArrayI : public LInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(MinMaxArrayI);
  LMinMaxArrayI(const LAllocation& array, const LDefinition& temp0,
                const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, array);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LAllocation* array() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }

  bool isMax() const { return mir_->toMinMaxArray()->isMax(); }
};

class LMinMaxArrayD : public LInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(MinMaxArrayD);
  LMinMaxArrayD(const LAllocation& array, const LDefinition& floatTemp,
                const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, array);
    setTemp(0, floatTemp);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LAllocation* array() { return getOperand(0); }
  const LDefinition* floatTemp() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
  const LDefinition* temp2() { return getTemp(2); }

  bool isMax() const { return mir_->toMinMaxArray()->isMax(); }
};

// Negative of an int64
class LNegI64 : public LInstructionHelper<INT64_PIECES, INT64_PIECES, 0> {
 public:
  LIR_HEADER(NegI64);
  explicit LNegI64(const LInt64Allocation& num)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, num);
  }
};

// Absolute value of an integer.
class LAbsI : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(AbsI)
  explicit LAbsI(const LAllocation& num) : LInstructionHelper(classOpcode) {
    setOperand(0, num);
  }

  MAbs* mir() const { return mir_->toAbs(); }
};

// Copysign for doubles.
class LCopySignD : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(CopySignD)
  explicit LCopySignD() : LInstructionHelper(classOpcode) {}
};

// Copysign for float32.
class LCopySignF : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(CopySignF)
  explicit LCopySignF() : LInstructionHelper(classOpcode) {}
};

// Count leading zeroes on an int32.
class LClzI : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(ClzI)
  explicit LClzI(const LAllocation& num) : LInstructionHelper(classOpcode) {
    setOperand(0, num);
  }

  MClz* mir() const { return mir_->toClz(); }
};

// Count leading zeroes on an int64.
class LClzI64 : public LInstructionHelper<INT64_PIECES, INT64_PIECES, 0> {
 public:
  LIR_HEADER(ClzI64)
  explicit LClzI64(const LInt64Allocation& num)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, num);
  }

  MClz* mir() const { return mir_->toClz(); }
};

// Count trailing zeroes on an int32.
class LCtzI : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(CtzI)
  explicit LCtzI(const LAllocation& num) : LInstructionHelper(classOpcode) {
    setOperand(0, num);
  }

  MCtz* mir() const { return mir_->toCtz(); }
};

// Count trailing zeroes on an int64.
class LCtzI64 : public LInstructionHelper<INT64_PIECES, INT64_PIECES, 0> {
 public:
  LIR_HEADER(CtzI64)
  explicit LCtzI64(const LInt64Allocation& num)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, num);
  }

  MCtz* mir() const { return mir_->toCtz(); }
};

// Count population on an int32.
class LPopcntI : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(PopcntI)
  explicit LPopcntI(const LAllocation& num, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, num);
    setTemp(0, temp);
  }

  MPopcnt* mir() const { return mir_->toPopcnt(); }

  const LDefinition* temp() { return getTemp(0); }
};

// Count population on an int64.
class LPopcntI64 : public LInstructionHelper<INT64_PIECES, INT64_PIECES, 1> {
 public:
  LIR_HEADER(PopcntI64)
  explicit LPopcntI64(const LInt64Allocation& num, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, num);
    setTemp(0, temp);
  }

  MPopcnt* mir() const { return mir_->toPopcnt(); }
};

class LAtan2D : public LCallInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(Atan2D)
  LAtan2D(const LAllocation& y, const LAllocation& x, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, y);
    setOperand(1, x);
    setTemp(0, temp);
  }

  const LAllocation* y() { return getOperand(0); }

  const LAllocation* x() { return getOperand(1); }

  const LDefinition* temp() { return getTemp(0); }

  const LDefinition* output() { return getDef(0); }
};

class LHypot : public LCallInstructionHelper<1, 4, 1> {
  uint32_t numOperands_;

 public:
  LIR_HEADER(Hypot)
  LHypot(const LAllocation& x, const LAllocation& y, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode), numOperands_(2) {
    setOperand(0, x);
    setOperand(1, y);
    setTemp(0, temp);
  }

  LHypot(const LAllocation& x, const LAllocation& y, const LAllocation& z,
         const LDefinition& temp)
      : LCallInstructionHelper(classOpcode), numOperands_(3) {
    setOperand(0, x);
    setOperand(1, y);
    setOperand(2, z);
    setTemp(0, temp);
  }

  LHypot(const LAllocation& x, const LAllocation& y, const LAllocation& z,
         const LAllocation& w, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode), numOperands_(4) {
    setOperand(0, x);
    setOperand(1, y);
    setOperand(2, z);
    setOperand(3, w);
    setTemp(0, temp);
  }

  uint32_t numArgs() const { return numOperands_; }

  const LAllocation* x() { return getOperand(0); }

  const LAllocation* y() { return getOperand(1); }

  const LDefinition* temp() { return getTemp(0); }

  const LDefinition* output() { return getDef(0); }
};

// Double raised to an integer power.
class LPowI : public LCallInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(PowI)
  LPowI(const LAllocation& value, const LAllocation& power,
        const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, value);
    setOperand(1, power);
    setTemp(0, temp);
  }

  const LAllocation* value() { return getOperand(0); }
  const LAllocation* power() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Integer raised to an integer power.
class LPowII : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(PowII)
  LPowII(const LAllocation& value, const LAllocation& power,
         const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, value);
    setOperand(1, power);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* value() { return getOperand(0); }
  const LAllocation* power() { return getOperand(1); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  MPow* mir() const { return mir_->toPow(); }
};

// Double raised to a double power.
class LPowD : public LCallInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(PowD)
  LPowD(const LAllocation& value, const LAllocation& power,
        const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, value);
    setOperand(1, power);
    setTemp(0, temp);
  }

  const LAllocation* value() { return getOperand(0); }
  const LAllocation* power() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Constant of a power of two raised to an integer power.
class LPowOfTwoI : public LInstructionHelper<1, 1, 0> {
  uint32_t base_;

 public:
  LIR_HEADER(PowOfTwoI)
  LPowOfTwoI(uint32_t base, const LAllocation& power)
      : LInstructionHelper(classOpcode), base_(base) {
    setOperand(0, power);
  }

  uint32_t base() const { return base_; }
  const LAllocation* power() { return getOperand(0); }
};

// Sign value of a double with expected int32 result.
class LSignDI : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(SignDI)
  explicit LSignDI(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
};

class LMathFunctionD : public LCallInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(MathFunctionD)
  LMathFunctionD(const LAllocation& input, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
  MMathFunction* mir() const { return mir_->toMathFunction(); }
  const char* extraName() const {
    return MMathFunction::FunctionName(mir()->function());
  }
};

class LMathFunctionF : public LCallInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(MathFunctionF)
  LMathFunctionF(const LAllocation& input, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
  MMathFunction* mir() const { return mir_->toMathFunction(); }
  const char* extraName() const {
    return MMathFunction::FunctionName(mir()->function());
  }
};

// Adds two integers, returning an integer value.
class LAddI : public LBinaryMath<0> {
  bool recoversInput_;

 public:
  LIR_HEADER(AddI)

  LAddI() : LBinaryMath(classOpcode), recoversInput_(false) {}

  const char* extraName() const {
    return snapshot() ? "OverflowCheck" : nullptr;
  }

  bool recoversInput() const { return recoversInput_; }
  void setRecoversInput() { recoversInput_ = true; }

  MAdd* mir() const { return mir_->toAdd(); }
};

class LAddI64 : public LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0> {
 public:
  LIR_HEADER(AddI64)

  LAddI64() : LInstructionHelper(classOpcode) {}

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;
};

// Subtracts two integers, returning an integer value.
class LSubI : public LBinaryMath<0> {
  bool recoversInput_;

 public:
  LIR_HEADER(SubI)

  LSubI() : LBinaryMath(classOpcode), recoversInput_(false) {}

  const char* extraName() const {
    return snapshot() ? "OverflowCheck" : nullptr;
  }

  bool recoversInput() const { return recoversInput_; }
  void setRecoversInput() { recoversInput_ = true; }
  MSub* mir() const { return mir_->toSub(); }
};

inline bool LNode::recoversInput() const {
  switch (op()) {
    case Opcode::AddI:
      return toAddI()->recoversInput();
    case Opcode::SubI:
      return toSubI()->recoversInput();
    default:
      return false;
  }
}

class LSubI64 : public LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0> {
 public:
  LIR_HEADER(SubI64)

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;

  LSubI64() : LInstructionHelper(classOpcode) {}
};

class LMulI64 : public LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 1> {
 public:
  LIR_HEADER(MulI64)

  explicit LMulI64() : LInstructionHelper(classOpcode) {
    setTemp(0, LDefinition());
  }

  const LDefinition* temp() { return getTemp(0); }

  static const size_t Lhs = 0;
  static const size_t Rhs = INT64_PIECES;
};

// Performs an add, sub, mul, or div on two double values.
class LMathD : public LBinaryMath<0> {
  JSOp jsop_;

 public:
  LIR_HEADER(MathD)

  explicit LMathD(JSOp jsop) : LBinaryMath(classOpcode), jsop_(jsop) {}

  JSOp jsop() const { return jsop_; }

  const char* extraName() const { return CodeName(jsop_); }
};

// Performs an add, sub, mul, or div on two double values.
class LMathF : public LBinaryMath<0> {
  JSOp jsop_;

 public:
  LIR_HEADER(MathF)

  explicit LMathF(JSOp jsop) : LBinaryMath(classOpcode), jsop_(jsop) {}

  JSOp jsop() const { return jsop_; }

  const char* extraName() const { return CodeName(jsop_); }
};

class LModD : public LBinaryMath<1> {
 public:
  LIR_HEADER(ModD)

  LModD(const LAllocation& lhs, const LAllocation& rhs, const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
    setIsCall();
  }
  const LDefinition* temp() { return getTemp(0); }
  MMod* mir() const { return mir_->toMod(); }
};

class LModPowTwoD : public LInstructionHelper<1, 1, 0> {
  const uint32_t divisor_;

 public:
  LIR_HEADER(ModPowTwoD)

  LModPowTwoD(const LAllocation& lhs, uint32_t divisor)
      : LInstructionHelper(classOpcode), divisor_(divisor) {
    setOperand(0, lhs);
  }

  uint32_t divisor() const { return divisor_; }
  const LAllocation* lhs() { return getOperand(0); }
  MMod* mir() const { return mir_->toMod(); }
};

class LBigIntAdd : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntAdd)

  LBigIntAdd(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntSub : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntSub)

  LBigIntSub(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntMul : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntMul)

  LBigIntMul(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntDiv : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntDiv)

  LBigIntDiv(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  const MBigIntDiv* mir() const { return mirRaw()->toBigIntDiv(); }
};

class LBigIntMod : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntMod)

  LBigIntMod(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  const MBigIntMod* mir() const { return mirRaw()->toBigIntMod(); }
};

class LBigIntPow : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntPow)

  LBigIntPow(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  const MBigIntPow* mir() const { return mirRaw()->toBigIntPow(); }
};

class LBigIntBitAnd : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntBitAnd)

  LBigIntBitAnd(const LAllocation& lhs, const LAllocation& rhs,
                const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntBitOr : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntBitOr)

  LBigIntBitOr(const LAllocation& lhs, const LAllocation& rhs,
               const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntBitXor : public LBinaryMath<2> {
 public:
  LIR_HEADER(BigIntBitXor)

  LBigIntBitXor(const LAllocation& lhs, const LAllocation& rhs,
                const LDefinition& temp1, const LDefinition& temp2)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntLsh : public LBinaryMath<3> {
 public:
  LIR_HEADER(BigIntLsh)

  LBigIntLsh(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2,
             const LDefinition& temp3)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
};

class LBigIntRsh : public LBinaryMath<3> {
 public:
  LIR_HEADER(BigIntRsh)

  LBigIntRsh(const LAllocation& lhs, const LAllocation& rhs,
             const LDefinition& temp1, const LDefinition& temp2,
             const LDefinition& temp3)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
};

class LBigIntIncrement : public LUnaryMath<2> {
 public:
  LIR_HEADER(BigIntIncrement)

  LBigIntIncrement(const LAllocation& input, const LDefinition& temp1,
                   const LDefinition& temp2)
      : LUnaryMath(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntDecrement : public LUnaryMath<2> {
 public:
  LIR_HEADER(BigIntDecrement)

  LBigIntDecrement(const LAllocation& input, const LDefinition& temp1,
                   const LDefinition& temp2)
      : LUnaryMath(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntNegate : public LUnaryMath<1> {
 public:
  LIR_HEADER(BigIntNegate)

  LBigIntNegate(const LAllocation& input, const LDefinition& temp)
      : LUnaryMath(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
};

class LBigIntBitNot : public LUnaryMath<2> {
 public:
  LIR_HEADER(BigIntBitNot)

  LBigIntBitNot(const LAllocation& input, const LDefinition& temp1,
                const LDefinition& temp2)
      : LUnaryMath(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

// Adds two string, returning a string.
class LConcat : public LInstructionHelper<1, 2, 5> {
 public:
  LIR_HEADER(Concat)

  LConcat(const LAllocation& lhs, const LAllocation& rhs,
          const LDefinition& temp1, const LDefinition& temp2,
          const LDefinition& temp3, const LDefinition& temp4,
          const LDefinition& temp5)
      : LInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
    setTemp(4, temp5);
  }

  const LAllocation* lhs() { return this->getOperand(0); }
  const LAllocation* rhs() { return this->getOperand(1); }
  const LDefinition* temp1() { return this->getTemp(0); }
  const LDefinition* temp2() { return this->getTemp(1); }
  const LDefinition* temp3() { return this->getTemp(2); }
  const LDefinition* temp4() { return this->getTemp(3); }
  const LDefinition* temp5() { return this->getTemp(4); }
};

// Get uint16 character code from a string.
class LCharCodeAt : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(CharCodeAt)

  LCharCodeAt(const LAllocation& str, const LAllocation& index,
              const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, str);
    setOperand(1, index);
    setTemp(0, temp);
  }

  const LAllocation* str() { return this->getOperand(0); }
  const LAllocation* index() { return this->getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Convert uint32 code point to a string.
class LFromCodePoint : public LInstructionHelper<1, 1, 2> {
 public:
  LIR_HEADER(FromCodePoint)

  explicit LFromCodePoint(const LAllocation& codePoint,
                          const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, codePoint);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* codePoint() { return this->getOperand(0); }

  const LDefinition* temp1() { return this->getTemp(0); }

  const LDefinition* temp2() { return this->getTemp(1); }
};

class LSubstr : public LInstructionHelper<1, 3, 3> {
 public:
  LIR_HEADER(Substr)

  LSubstr(const LAllocation& string, const LAllocation& begin,
          const LAllocation& length, const LDefinition& temp,
          const LDefinition& temp2, const LDefinition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, string);
    setOperand(1, begin);
    setOperand(2, length);
    setTemp(0, temp);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }
  const LAllocation* string() { return getOperand(0); }
  const LAllocation* begin() { return getOperand(1); }
  const LAllocation* length() { return getOperand(2); }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const MStringSplit* mir() const { return mir_->toStringSplit(); }
};

// Convert a value to a double.
class LValueToDouble : public LInstructionHelper<1, BOX_PIECES, 0> {
 public:
  LIR_HEADER(ValueToDouble)
  static const size_t Input = 0;

  explicit LValueToDouble(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  MToDouble* mir() { return mir_->toToDouble(); }
};

// Convert a value to a float32.
class LValueToFloat32 : public LInstructionHelper<1, BOX_PIECES, 0> {
 public:
  LIR_HEADER(ValueToFloat32)
  static const size_t Input = 0;

  explicit LValueToFloat32(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  MToFloat32* mir() { return mir_->toToFloat32(); }
};

// Convert a value to an int32.
//   Input: components of a Value
//   Output: 32-bit integer
//   Bailout: undefined, string, object, or non-int32 double
//   Temps: one float register, one GP register
//
// This instruction requires a temporary float register.
class LValueToInt32 : public LInstructionHelper<1, BOX_PIECES, 2> {
 public:
  enum Mode { NORMAL, TRUNCATE, TRUNCATE_NOWRAP };

 private:
  Mode mode_;

 public:
  LIR_HEADER(ValueToInt32)

  LValueToInt32(const LBoxAllocation& input, const LDefinition& temp0,
                const LDefinition& temp1, Mode mode)
      : LInstructionHelper(classOpcode), mode_(mode) {
    setBoxOperand(Input, input);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const char* extraName() const {
    return mode() == NORMAL     ? "Normal"
           : mode() == TRUNCATE ? "Truncate"
                                : "TruncateNoWrap";
  }

  static const size_t Input = 0;

  Mode mode() const { return mode_; }
  const LDefinition* tempFloat() { return getTemp(0); }
  const LDefinition* temp() { return getTemp(1); }
  MToNumberInt32* mirNormal() const {
    MOZ_ASSERT(mode_ == NORMAL);
    return mir_->toToNumberInt32();
  }
  MTruncateToInt32* mirTruncate() const {
    MOZ_ASSERT(mode_ == TRUNCATE);
    return mir_->toTruncateToInt32();
  }
  MToIntegerInt32* mirTruncateNoWrap() const {
    MOZ_ASSERT(mode_ == TRUNCATE_NOWRAP);
    return mir_->toToIntegerInt32();
  }
  MInstruction* mir() const { return mir_->toInstruction(); }
};

// Convert a value to a BigInt.
class LValueToBigInt : public LInstructionHelper<1, BOX_PIECES, 0> {
 public:
  LIR_HEADER(ValueToBigInt)
  static const size_t Input = 0;

  explicit LValueToBigInt(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  MToBigInt* mir() const { return mir_->toToBigInt(); }
};

// Convert a double to an int32.
//   Input: floating-point register
//   Output: 32-bit integer
//   Bailout: if the double cannot be converted to an integer.
class LDoubleToInt32 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(DoubleToInt32)

  explicit LDoubleToInt32(const LAllocation& in)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
  }

  MToNumberInt32* mir() const { return mir_->toToNumberInt32(); }
};

// Convert a float32 to an int32.
//   Input: floating-point register
//   Output: 32-bit integer
//   Bailout: if the float32 cannot be converted to an integer.
class LFloat32ToInt32 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(Float32ToInt32)

  explicit LFloat32ToInt32(const LAllocation& in)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
  }

  MToNumberInt32* mir() const { return mir_->toToNumberInt32(); }
};

// Truncates a double to an int32.
//   Input: floating-point register
//   Output: 32-bit integer
//   Bailout: if the double when converted to an integer exceeds the int32
//            bounds. No bailout for NaN or negative zero.
class LDoubleToIntegerInt32 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(DoubleToIntegerInt32)

  explicit LDoubleToIntegerInt32(const LAllocation& in)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
  }

  MToIntegerInt32* mir() const { return mir_->toToIntegerInt32(); }
};

// Truncates a float to an int32.
//   Input: floating-point register
//   Output: 32-bit integer
//   Bailout: if the double when converted to an integer exceeds the int32
//            bounds. No bailout for NaN or negative zero.
class LFloat32ToIntegerInt32 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(Float32ToIntegerInt32)

  explicit LFloat32ToIntegerInt32(const LAllocation& in)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
  }

  MToIntegerInt32* mir() const { return mir_->toToIntegerInt32(); }
};

// Convert a double to a truncated int32.
//   Input: floating-point register
//   Output: 32-bit integer
class LTruncateDToInt32 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(TruncateDToInt32)

  LTruncateDToInt32(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }

  const LDefinition* tempFloat() { return getTemp(0); }

  MTruncateToInt32* mir() const { return mir_->toTruncateToInt32(); }
};

// Convert a double to a truncated int32 with tls offset because we need it for
// the slow ool path.
class LWasmBuiltinTruncateDToInt32 : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(WasmBuiltinTruncateDToInt32)

  LWasmBuiltinTruncateDToInt32(const LAllocation& in, const LAllocation& tls,
                               const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setOperand(1, tls);
    setTemp(0, temp);
  }

  const LDefinition* tempFloat() { return getTemp(0); }

  MWasmBuiltinTruncateToInt32* mir() const {
    return mir_->toWasmBuiltinTruncateToInt32();
  }
};

// Convert a float32 to a truncated int32.
//   Input: floating-point register
//   Output: 32-bit integer
class LTruncateFToInt32 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(TruncateFToInt32)

  LTruncateFToInt32(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }

  const LDefinition* tempFloat() { return getTemp(0); }

  MTruncateToInt32* mir() const { return mir_->toTruncateToInt32(); }
};

// Convert a float32 to a truncated int32 with tls offset because we need it for
// the slow ool path.
class LWasmBuiltinTruncateFToInt32 : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(WasmBuiltinTruncateFToInt32)

  LWasmBuiltinTruncateFToInt32(const LAllocation& in, const LAllocation& tls,
                               const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setOperand(1, tls);
    setTemp(0, temp);
  }

  const LDefinition* tempFloat() { return getTemp(0); }

  MWasmBuiltinTruncateToInt32* mir() const {
    return mir_->toWasmBuiltinTruncateToInt32();
  }
};

class LExtendInt32ToInt64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(ExtendInt32ToInt64)

  explicit LExtendInt32ToInt64(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  const MExtendInt32ToInt64* mir() { return mir_->toExtendInt32ToInt64(); }
};

// Convert a boolean value to a string.
class LBooleanToString : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(BooleanToString)

  explicit LBooleanToString(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  const MToString* mir() { return mir_->toToString(); }
};

// Convert an integer hosted on one definition to a string with a function call.
class LIntToString : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(IntToString)

  explicit LIntToString(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  const MToString* mir() { return mir_->toToString(); }
};

// Convert a double hosted on one definition to a string with a function call.
class LDoubleToString : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(DoubleToString)

  LDoubleToString(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* tempInt() { return getTemp(0); }
  const MToString* mir() { return mir_->toToString(); }
};

// Convert a primitive to a string with a function call.
class LValueToString : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(ValueToString)

  LValueToString(const LBoxAllocation& input, const LDefinition& tempToUnbox)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, tempToUnbox);
  }

  static const size_t Input = 0;

  const MToString* mir() { return mir_->toToString(); }

  const LDefinition* tempToUnbox() { return getTemp(0); }
};

// Double raised to a half power.
class LPowHalfD : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(PowHalfD);
  explicit LPowHalfD(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* output() { return getDef(0); }
  MPowHalf* mir() const { return mir_->toPowHalf(); }
};

class LNaNToZero : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(NaNToZero)

  explicit LNaNToZero(const LAllocation& input, const LDefinition& tempDouble)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, tempDouble);
  }

  const MNaNToZero* mir() { return mir_->toNaNToZero(); }
  const LAllocation* input() { return getOperand(0); }
  const LDefinition* output() { return getDef(0); }
  const LDefinition* tempDouble() { return getTemp(0); }
};

// Passed the BaselineFrame address in the OsrFrameReg via the IonOsrTempData
// populated by PrepareOsrTempData.
//
// Forwards this object to the LOsrValues for Value materialization.
class LOsrEntry : public LInstructionHelper<1, 0, 1> {
 protected:
  Label label_;
  uint32_t frameDepth_;

 public:
  LIR_HEADER(OsrEntry)

  explicit LOsrEntry(const LDefinition& temp)
      : LInstructionHelper(classOpcode), frameDepth_(0) {
    setTemp(0, temp);
  }

  void setFrameDepth(uint32_t depth) { frameDepth_ = depth; }
  uint32_t getFrameDepth() { return frameDepth_; }
  Label* label() { return &label_; }
  const LDefinition* temp() { return getTemp(0); }
};

// Materialize a Value stored in an interpreter frame for OSR.
class LOsrValue : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(OsrValue)

  explicit LOsrValue(const LAllocation& entry)
      : LInstructionHelper(classOpcode) {
    setOperand(0, entry);
  }

  const MOsrValue* mir() { return mir_->toOsrValue(); }
};

// Materialize a JSObject env chain stored in an interpreter frame for OSR.
class LOsrReturnValue : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(OsrReturnValue)

  explicit LOsrReturnValue(const LAllocation& entry)
      : LInstructionHelper(classOpcode) {
    setOperand(0, entry);
  }

  const MOsrReturnValue* mir() { return mir_->toOsrReturnValue(); }
};

class LRegExp : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(RegExp)

  explicit LRegExp(const LDefinition& temp) : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }
  const LDefinition* temp() { return getTemp(0); }
  const MRegExp* mir() const { return mir_->toRegExp(); }
};

class LRegExpMatcher : public LCallInstructionHelper<BOX_PIECES, 3, 0> {
 public:
  LIR_HEADER(RegExpMatcher)

  LRegExpMatcher(const LAllocation& regexp, const LAllocation& string,
                 const LAllocation& lastIndex)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, regexp);
    setOperand(1, string);
    setOperand(2, lastIndex);
  }

  const LAllocation* regexp() { return getOperand(0); }
  const LAllocation* string() { return getOperand(1); }
  const LAllocation* lastIndex() { return getOperand(2); }

  const MRegExpMatcher* mir() const { return mir_->toRegExpMatcher(); }
};

class LRegExpPrototypeOptimizable : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(RegExpPrototypeOptimizable);
  LRegExpPrototypeOptimizable(const LAllocation& object,
                              const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  MRegExpPrototypeOptimizable* mir() const {
    return mir_->toRegExpPrototypeOptimizable();
  }
};

class LRegExpInstanceOptimizable : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(RegExpInstanceOptimizable);
  LRegExpInstanceOptimizable(const LAllocation& object,
                             const LAllocation& proto, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, proto);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* proto() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  MRegExpInstanceOptimizable* mir() const {
    return mir_->toRegExpInstanceOptimizable();
  }
};

class LGetFirstDollarIndex : public LInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(GetFirstDollarIndex);
  LGetFirstDollarIndex(const LAllocation& str, const LDefinition& temp0,
                       const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, str);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const LAllocation* str() { return getOperand(0); }
  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
  const LDefinition* temp2() { return getTemp(2); }
};

class LBinaryValueCache
    : public LInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 2> {
 public:
  LIR_HEADER(BinaryValueCache)

  // Takes two temps: these are intendend to be FloatReg0 and FloatReg1
  // To allow the actual cache code to safely clobber those values without
  // save and restore.
  LBinaryValueCache(const LBoxAllocation& lhs, const LBoxAllocation& rhs,
                    const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(LhsInput, lhs);
    setBoxOperand(RhsInput, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const MBinaryCache* mir() const { return mir_->toBinaryCache(); }

  static const size_t LhsInput = 0;
  static const size_t RhsInput = BOX_PIECES;
};

class LBinaryBoolCache : public LInstructionHelper<1, 2 * BOX_PIECES, 2> {
 public:
  LIR_HEADER(BinaryBoolCache)

  // Takes two temps: these are intendend to be FloatReg0 and FloatReg1
  // To allow the actual cache code to safely clobber those values without
  // save and restore.
  LBinaryBoolCache(const LBoxAllocation& lhs, const LBoxAllocation& rhs,
                   const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(LhsInput, lhs);
    setBoxOperand(RhsInput, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const MBinaryCache* mir() const { return mir_->toBinaryCache(); }

  static const size_t LhsInput = 0;
  static const size_t RhsInput = BOX_PIECES;
};

class LUnaryCache : public LInstructionHelper<BOX_PIECES, BOX_PIECES, 0> {
 public:
  LIR_HEADER(UnaryCache)

  explicit LUnaryCache(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  const MUnaryCache* mir() const { return mir_->toUnaryCache(); }

  const LAllocation* input() { return getOperand(Input); }

  static const size_t Input = 0;
};

class LLambda : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(Lambda)

  LLambda(const LAllocation& envChain, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, envChain);
    setTemp(0, temp);
  }
  const LAllocation* environmentChain() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const MLambda* mir() const { return mir_->toLambda(); }
};

class LLambdaArrow : public LInstructionHelper<1, 1 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(LambdaArrow)

  static const size_t NewTargetValue = 1;

  LLambdaArrow(const LAllocation& envChain, const LBoxAllocation& newTarget,
               const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, envChain);
    setBoxOperand(NewTargetValue, newTarget);
    setTemp(0, temp);
  }
  const LAllocation* environmentChain() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const MLambdaArrow* mir() const { return mir_->toLambdaArrow(); }
};

class LGetNextEntryForIterator : public LInstructionHelper<1, 2, 3> {
 public:
  LIR_HEADER(GetNextEntryForIterator)

  explicit LGetNextEntryForIterator(const LAllocation& iter,
                                    const LAllocation& result,
                                    const LDefinition& temp0,
                                    const LDefinition& temp1,
                                    const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, iter);
    setOperand(1, result);
    setTemp(0, temp0);
    setTemp(1, temp1);
    setTemp(2, temp2);
  }

  const MGetNextEntryForIterator* mir() const {
    return mir_->toGetNextEntryForIterator();
  }
  const LAllocation* iter() { return getOperand(0); }
  const LAllocation* result() { return getOperand(1); }
  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
  const LDefinition* temp2() { return getTemp(2); }
};

class LGuardHasAttachedArrayBuffer : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardHasAttachedArrayBuffer)

  LGuardHasAttachedArrayBuffer(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

// Bailout if index + minimum < 0 or index + maximum >= length.
class LBoundsCheckRange : public LInstructionHelper<0, 2, 1> {
 public:
  LIR_HEADER(BoundsCheckRange)

  LBoundsCheckRange(const LAllocation& index, const LAllocation& length,
                    const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, index);
    setOperand(1, length);
    setTemp(0, temp);
  }
  const MBoundsCheck* mir() const { return mir_->toBoundsCheck(); }
  const LAllocation* index() { return getOperand(0); }
  const LAllocation* length() { return getOperand(1); }
};

// Load a value from a dense array's elements vector. Bail out if it's the hole
// value.
class LLoadElementV : public LInstructionHelper<BOX_PIECES, 2, 0> {
 public:
  LIR_HEADER(LoadElementV)

  LLoadElementV(const LAllocation& elements, const LAllocation& index)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
  }

  const char* extraName() const {
    return mir()->needsHoleCheck() ? "HoleCheck" : nullptr;
  }

  const MLoadElement* mir() const { return mir_->toLoadElement(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
};

// Load a value from an array's elements vector, loading |undefined| if we hit a
// hole. Bail out if we get a negative index.
class LLoadElementHole : public LInstructionHelper<BOX_PIECES, 3, 0> {
 public:
  LIR_HEADER(LoadElementHole)

  LLoadElementHole(const LAllocation& elements, const LAllocation& index,
                   const LAllocation& initLength)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, initLength);
  }

  const char* extraName() const {
    return mir()->needsHoleCheck() ? "HoleCheck" : nullptr;
  }

  const MLoadElementHole* mir() const { return mir_->toLoadElementHole(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* initLength() { return getOperand(2); }
};

// Store a boxed value to a dense array's element vector.
class LStoreElementV : public LInstructionHelper<0, 2 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(StoreElementV)

  LStoreElementV(const LAllocation& elements, const LAllocation& index,
                 const LBoxAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setBoxOperand(Value, value);
  }

  const char* extraName() const {
    return mir()->needsHoleCheck() ? "HoleCheck" : nullptr;
  }

  static const size_t Value = 2;

  const MStoreElement* mir() const { return mir_->toStoreElement(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
};

// Store a typed value to a dense array's elements vector. Compared to
// LStoreElementV, this instruction can store doubles and constants directly,
// and does not store the type tag if the array is monomorphic and known to
// be packed.
class LStoreElementT : public LInstructionHelper<0, 3, 0> {
 public:
  LIR_HEADER(StoreElementT)

  LStoreElementT(const LAllocation& elements, const LAllocation& index,
                 const LAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
  }

  const char* extraName() const {
    return mir()->needsHoleCheck() ? "HoleCheck" : nullptr;
  }

  const MStoreElement* mir() const { return mir_->toStoreElement(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
};

// Like LStoreElementV, but supports indexes >= initialized length.
class LStoreElementHoleV : public LInstructionHelper<0, 3 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(StoreElementHoleV)

  LStoreElementHoleV(const LAllocation& object, const LAllocation& elements,
                     const LAllocation& index, const LBoxAllocation& value,
                     const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, elements);
    setOperand(2, index);
    setBoxOperand(Value, value);
    setTemp(0, spectreTemp);
  }

  static const size_t Value = 3;

  const MStoreElementHole* mir() const { return mir_->toStoreElementHole(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* elements() { return getOperand(1); }
  const LAllocation* index() { return getOperand(2); }
  const LDefinition* spectreTemp() { return getTemp(0); }
};

// Like LStoreElementT, but supports indexes >= initialized length.
class LStoreElementHoleT : public LInstructionHelper<0, 4, 1> {
 public:
  LIR_HEADER(StoreElementHoleT)

  LStoreElementHoleT(const LAllocation& object, const LAllocation& elements,
                     const LAllocation& index, const LAllocation& value,
                     const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, elements);
    setOperand(2, index);
    setOperand(3, value);
    setTemp(0, spectreTemp);
  }

  const MStoreElementHole* mir() const { return mir_->toStoreElementHole(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* elements() { return getOperand(1); }
  const LAllocation* index() { return getOperand(2); }
  const LAllocation* value() { return getOperand(3); }
  const LDefinition* spectreTemp() { return getTemp(0); }
};

class LArrayPopShift : public LInstructionHelper<BOX_PIECES, 1, 2> {
 public:
  LIR_HEADER(ArrayPopShift)

  LArrayPopShift(const LAllocation& object, const LDefinition& temp0,
                 const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const char* extraName() const {
    return mir()->mode() == MArrayPopShift::Pop ? "Pop" : "Shift";
  }

  const MArrayPopShift* mir() const { return mir_->toArrayPopShift(); }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp0() { return getTemp(0); }
  const LDefinition* temp1() { return getTemp(1); }
};

class LArrayPush : public LInstructionHelper<1, 1 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(ArrayPush)

  LArrayPush(const LAllocation& object, const LBoxAllocation& value,
             const LDefinition& temp, const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setBoxOperand(Value, value);
    setTemp(0, temp);
    setTemp(1, spectreTemp);
  }

  static const size_t Value = 1;

  const MArrayPush* mir() const { return mir_->toArrayPush(); }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* spectreTemp() { return getTemp(1); }
};

class LArraySlice : public LCallInstructionHelper<1, 3, 2> {
 public:
  LIR_HEADER(ArraySlice)

  LArraySlice(const LAllocation& obj, const LAllocation& begin,
              const LAllocation& end, const LDefinition& temp1,
              const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, begin);
    setOperand(2, end);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }
  const MArraySlice* mir() const { return mir_->toArraySlice(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* begin() { return getOperand(1); }
  const LAllocation* end() { return getOperand(2); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LArrayJoin : public LCallInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(ArrayJoin)

  LArrayJoin(const LAllocation& array, const LAllocation& sep,
             const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, array);
    setOperand(1, sep);
    setTemp(0, temp);
  }

  const MArrayJoin* mir() const { return mir_->toArrayJoin(); }
  const LDefinition* output() { return getDef(0); }
  const LAllocation* array() { return getOperand(0); }
  const LAllocation* separator() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

class LLoadUnboxedScalar : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(LoadUnboxedScalar)

  LLoadUnboxedScalar(const LAllocation& elements, const LAllocation& index,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setTemp(0, temp);
  }
  const MLoadUnboxedScalar* mir() const { return mir_->toLoadUnboxedScalar(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

class LLoadUnboxedBigInt : public LInstructionHelper<1, 2, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(LoadUnboxedBigInt)

  LLoadUnboxedBigInt(const LAllocation& elements, const LAllocation& index,
                     const LDefinition& temp, const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }
  const MLoadUnboxedScalar* mir() const { return mir_->toLoadUnboxedScalar(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  const LInt64Definition temp64() { return getInt64Temp(1); }
};

class LLoadDataViewElement : public LInstructionHelper<1, 3, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(LoadDataViewElement)

  LLoadDataViewElement(const LAllocation& elements, const LAllocation& index,
                       const LAllocation& littleEndian, const LDefinition& temp,
                       const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, littleEndian);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }
  const MLoadDataViewElement* mir() const {
    return mir_->toLoadDataViewElement();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* littleEndian() { return getOperand(2); }
  const LDefinition* temp() { return getTemp(0); }
  const LInt64Definition temp64() { return getInt64Temp(1); }
};

class LLoadTypedArrayElementHole : public LInstructionHelper<BOX_PIECES, 2, 1> {
 public:
  LIR_HEADER(LoadTypedArrayElementHole)

  LLoadTypedArrayElementHole(const LAllocation& object,
                             const LAllocation& index, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
    setTemp(0, temp);
  }
  const MLoadTypedArrayElementHole* mir() const {
    return mir_->toLoadTypedArrayElementHole();
  }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

class LLoadTypedArrayElementHoleBigInt
    : public LInstructionHelper<BOX_PIECES, 2, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(LoadTypedArrayElementHoleBigInt)

  LLoadTypedArrayElementHoleBigInt(const LAllocation& object,
                                   const LAllocation& index,
                                   const LDefinition& temp,
                                   const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }
  const MLoadTypedArrayElementHole* mir() const {
    return mir_->toLoadTypedArrayElementHole();
  }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  const LInt64Definition temp64() { return getInt64Temp(1); }
};

class LStoreUnboxedBigInt : public LInstructionHelper<0, 3, INT64_PIECES> {
 public:
  LIR_HEADER(StoreUnboxedBigInt)

  LStoreUnboxedBigInt(const LAllocation& elements, const LAllocation& index,
                      const LAllocation& value, const LInt64Definition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp);
  }

  const MStoreUnboxedScalar* mir() const {
    return mir_->toStoreUnboxedScalar();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  LInt64Definition temp() { return getInt64Temp(0); }
};

class LStoreDataViewElement
    : public LInstructionHelper<0, 4, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(StoreDataViewElement)

  LStoreDataViewElement(const LAllocation& elements, const LAllocation& index,
                        const LAllocation& value,
                        const LAllocation& littleEndian,
                        const LDefinition& temp, const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setOperand(3, littleEndian);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }

  const MStoreDataViewElement* mir() const {
    return mir_->toStoreDataViewElement();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  const LAllocation* littleEndian() { return getOperand(3); }
  const LDefinition* temp() { return getTemp(0); }
  const LInt64Definition temp64() { return getInt64Temp(1); }
};

class LStoreTypedArrayElementHole : public LInstructionHelper<0, 4, 1> {
 public:
  LIR_HEADER(StoreTypedArrayElementHole)

  LStoreTypedArrayElementHole(const LAllocation& elements,
                              const LAllocation& length,
                              const LAllocation& index,
                              const LAllocation& value,
                              const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, length);
    setOperand(2, index);
    setOperand(3, value);
    setTemp(0, spectreTemp);
  }

  const MStoreTypedArrayElementHole* mir() const {
    return mir_->toStoreTypedArrayElementHole();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* length() { return getOperand(1); }
  const LAllocation* index() { return getOperand(2); }
  const LAllocation* value() { return getOperand(3); }
  const LDefinition* spectreTemp() { return getTemp(0); }
};

class LStoreTypedArrayElementHoleBigInt
    : public LInstructionHelper<0, 4, INT64_PIECES> {
 public:
  LIR_HEADER(StoreTypedArrayElementHoleBigInt)

  LStoreTypedArrayElementHoleBigInt(const LAllocation& elements,
                                    const LAllocation& length,
                                    const LAllocation& index,
                                    const LAllocation& value,
                                    const LInt64Definition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, length);
    setOperand(2, index);
    setOperand(3, value);
    setInt64Temp(0, temp);
  }

  const MStoreTypedArrayElementHole* mir() const {
    return mir_->toStoreTypedArrayElementHole();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* length() { return getOperand(1); }
  const LAllocation* index() { return getOperand(2); }
  const LAllocation* value() { return getOperand(3); }
  LInt64Definition temp() { return getInt64Temp(0); }
};

class LCompareExchangeTypedArrayElement : public LInstructionHelper<1, 4, 4> {
 public:
  LIR_HEADER(CompareExchangeTypedArrayElement)

  // ARM, ARM64, x86, x64
  LCompareExchangeTypedArrayElement(const LAllocation& elements,
                                    const LAllocation& index,
                                    const LAllocation& oldval,
                                    const LAllocation& newval,
                                    const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, oldval);
    setOperand(3, newval);
    setTemp(0, temp);
  }
  // MIPS32, MIPS64
  LCompareExchangeTypedArrayElement(
      const LAllocation& elements, const LAllocation& index,
      const LAllocation& oldval, const LAllocation& newval,
      const LDefinition& temp, const LDefinition& valueTemp,
      const LDefinition& offsetTemp, const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, oldval);
    setOperand(3, newval);
    setTemp(0, temp);
    setTemp(1, valueTemp);
    setTemp(2, offsetTemp);
    setTemp(3, maskTemp);
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* oldval() { return getOperand(2); }
  const LAllocation* newval() { return getOperand(3); }
  const LDefinition* temp() { return getTemp(0); }

  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(1); }
  const LDefinition* offsetTemp() { return getTemp(2); }
  const LDefinition* maskTemp() { return getTemp(3); }

  const MCompareExchangeTypedArrayElement* mir() const {
    return mir_->toCompareExchangeTypedArrayElement();
  }
};

class LAtomicExchangeTypedArrayElement : public LInstructionHelper<1, 3, 4> {
 public:
  LIR_HEADER(AtomicExchangeTypedArrayElement)

  // ARM, ARM64, x86, x64
  LAtomicExchangeTypedArrayElement(const LAllocation& elements,
                                   const LAllocation& index,
                                   const LAllocation& value,
                                   const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, temp);
  }
  // MIPS32, MIPS64
  LAtomicExchangeTypedArrayElement(const LAllocation& elements,
                                   const LAllocation& index,
                                   const LAllocation& value,
                                   const LDefinition& temp,
                                   const LDefinition& valueTemp,
                                   const LDefinition& offsetTemp,
                                   const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, temp);
    setTemp(1, valueTemp);
    setTemp(2, offsetTemp);
    setTemp(3, maskTemp);
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  const LDefinition* temp() { return getTemp(0); }

  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(1); }
  const LDefinition* offsetTemp() { return getTemp(2); }
  const LDefinition* maskTemp() { return getTemp(3); }

  const MAtomicExchangeTypedArrayElement* mir() const {
    return mir_->toAtomicExchangeTypedArrayElement();
  }
};

class LAtomicTypedArrayElementBinop : public LInstructionHelper<1, 3, 5> {
 public:
  LIR_HEADER(AtomicTypedArrayElementBinop)

  static const int32_t valueOp = 2;

  // ARM, ARM64, x86, x64
  LAtomicTypedArrayElementBinop(const LAllocation& elements,
                                const LAllocation& index,
                                const LAllocation& value,
                                const LDefinition& temp1,
                                const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }
  // MIPS32, MIPS64
  LAtomicTypedArrayElementBinop(const LAllocation& elements,
                                const LAllocation& index,
                                const LAllocation& value,
                                const LDefinition& temp2,
                                const LDefinition& valueTemp,
                                const LDefinition& offsetTemp,
                                const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, temp2);
    setTemp(2, valueTemp);
    setTemp(3, offsetTemp);
    setTemp(4, maskTemp);
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() {
    MOZ_ASSERT(valueOp == 2);
    return getOperand(2);
  }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(2); }
  const LDefinition* offsetTemp() { return getTemp(3); }
  const LDefinition* maskTemp() { return getTemp(4); }

  const MAtomicTypedArrayElementBinop* mir() const {
    return mir_->toAtomicTypedArrayElementBinop();
  }
};

// Atomic binary operation where the result is discarded.
class LAtomicTypedArrayElementBinopForEffect
    : public LInstructionHelper<0, 3, 4> {
 public:
  LIR_HEADER(AtomicTypedArrayElementBinopForEffect)

  // ARM, ARM64, x86, x64
  LAtomicTypedArrayElementBinopForEffect(
      const LAllocation& elements, const LAllocation& index,
      const LAllocation& value,
      const LDefinition& flagTemp = LDefinition::BogusTemp())
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, flagTemp);
  }
  // MIPS32, MIPS64
  LAtomicTypedArrayElementBinopForEffect(const LAllocation& elements,
                                         const LAllocation& index,
                                         const LAllocation& value,
                                         const LDefinition& valueTemp,
                                         const LDefinition& offsetTemp,
                                         const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, valueTemp);
    setTemp(2, offsetTemp);
    setTemp(3, maskTemp);
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }

  // Temp that may be used on LL/SC platforms for the flag result of the store.
  const LDefinition* flagTemp() { return getTemp(0); }
  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(1); }
  const LDefinition* offsetTemp() { return getTemp(2); }
  const LDefinition* maskTemp() { return getTemp(3); }

  const MAtomicTypedArrayElementBinop* mir() const {
    return mir_->toAtomicTypedArrayElementBinop();
  }
};

class LAtomicLoad64 : public LInstructionHelper<1, 2, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(AtomicLoad64)

  LAtomicLoad64(const LAllocation& elements, const LAllocation& index,
                const LDefinition& temp, const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }
  const MLoadUnboxedScalar* mir() const { return mir_->toLoadUnboxedScalar(); }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  LInt64Definition temp64() { return getInt64Temp(1); }
};

class LAtomicStore64 : public LInstructionHelper<0, 3, 2 * INT64_PIECES + 1> {
 public:
  LIR_HEADER(AtomicStore64)

  // x64, ARM64
  LAtomicStore64(const LAllocation& elements, const LAllocation& index,
                 const LAllocation& value, const LInt64Definition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(2 * INT64_PIECES, LDefinition::BogusTemp());
  }

  // ARM32
  LAtomicStore64(const LAllocation& elements, const LAllocation& index,
                 const LAllocation& value, const LInt64Definition& temp1,
                 const LInt64Definition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setTemp(2 * INT64_PIECES, LDefinition::BogusTemp());
  }

  // x86
  LAtomicStore64(const LAllocation& elements, const LAllocation& index,
                 const LAllocation& value, const LInt64Definition& temp1,
                 const LDefinition& tempLow)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(2 * INT64_PIECES, tempLow);
  }

  const MStoreUnboxedScalar* mir() const {
    return mir_->toStoreUnboxedScalar();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  LInt64Definition temp1() { return getInt64Temp(0); }
  LInt64Definition temp2() { return getInt64Temp(INT64_PIECES); }
  const LDefinition* tempLow() { return getTemp(2 * INT64_PIECES); }
};

class LCompareExchangeTypedArrayElement64
    : public LInstructionHelper<1, 4, 3 * INT64_PIECES + 1> {
 public:
  LIR_HEADER(CompareExchangeTypedArrayElement64)

  // x64, ARM64
  LCompareExchangeTypedArrayElement64(const LAllocation& elements,
                                      const LAllocation& index,
                                      const LAllocation& oldval,
                                      const LAllocation& newval,
                                      const LInt64Definition& temp1,
                                      const LInt64Definition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, oldval);
    setOperand(3, newval);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setInt64Temp(2 * INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(3 * INT64_PIECES, LDefinition::BogusTemp());
  }

  // x86
  LCompareExchangeTypedArrayElement64(const LAllocation& elements,
                                      const LAllocation& index,
                                      const LAllocation& oldval,
                                      const LAllocation& newval,
                                      const LDefinition& tempLow)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, oldval);
    setOperand(3, newval);
    setInt64Temp(0, LInt64Definition::BogusTemp());
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setInt64Temp(2 * INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(3 * INT64_PIECES, tempLow);
  }

  // ARM
  LCompareExchangeTypedArrayElement64(const LAllocation& elements,
                                      const LAllocation& index,
                                      const LAllocation& oldval,
                                      const LAllocation& newval,
                                      const LInt64Definition& temp1,
                                      const LInt64Definition& temp2,
                                      const LInt64Definition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, oldval);
    setOperand(3, newval);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setInt64Temp(2 * INT64_PIECES, temp3);
    setTemp(3 * INT64_PIECES, LDefinition::BogusTemp());
  }

  const MCompareExchangeTypedArrayElement* mir() const {
    return mir_->toCompareExchangeTypedArrayElement();
  }
  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* oldval() { return getOperand(2); }
  const LAllocation* newval() { return getOperand(3); }
  LInt64Definition temp1() { return getInt64Temp(0); }
  LInt64Definition temp2() { return getInt64Temp(INT64_PIECES); }
  LInt64Definition temp3() { return getInt64Temp(2 * INT64_PIECES); }
  const LDefinition* tempLow() { return getTemp(3 * INT64_PIECES); }
};

class LAtomicExchangeTypedArrayElement64
    : public LInstructionHelper<1, 3, INT64_PIECES + 1> {
 public:
  LIR_HEADER(AtomicExchangeTypedArrayElement64)

  // ARM, ARM64, x64
  LAtomicExchangeTypedArrayElement64(const LAllocation& elements,
                                     const LAllocation& index,
                                     const LAllocation& value,
                                     const LInt64Definition& temp1,
                                     const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setTemp(INT64_PIECES, temp2);
  }

  // x86
  LAtomicExchangeTypedArrayElement64(const LAllocation& elements,
                                     const LAllocation& index,
                                     const LAllocation& value,
                                     const LInt64Definition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp);
    setTemp(INT64_PIECES, LDefinition::BogusTemp());
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  LInt64Definition temp1() { return getInt64Temp(0); }
  const LDefinition* temp2() { return getTemp(INT64_PIECES); }

  const MAtomicExchangeTypedArrayElement* mir() const {
    return mir_->toAtomicExchangeTypedArrayElement();
  }
};

class LAtomicTypedArrayElementBinop64
    : public LInstructionHelper<1, 3, 3 * INT64_PIECES> {
 public:
  LIR_HEADER(AtomicTypedArrayElementBinop64)

  // x86
  LAtomicTypedArrayElementBinop64(const LAllocation& elements,
                                  const LAllocation& index,
                                  const LAllocation& value,
                                  const LInt64Definition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setInt64Temp(2 * INT64_PIECES, LInt64Definition::BogusTemp());
  }

  // ARM64, x64
  LAtomicTypedArrayElementBinop64(const LAllocation& elements,
                                  const LAllocation& index,
                                  const LAllocation& value,
                                  const LInt64Definition& temp1,
                                  const LInt64Definition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setInt64Temp(2 * INT64_PIECES, LInt64Definition::BogusTemp());
  }

  // ARM
  LAtomicTypedArrayElementBinop64(const LAllocation& elements,
                                  const LAllocation& index,
                                  const LAllocation& value,
                                  const LInt64Definition& temp1,
                                  const LInt64Definition& temp2,
                                  const LInt64Definition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setInt64Temp(2 * INT64_PIECES, temp3);
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  LInt64Definition temp1() { return getInt64Temp(0); }
  LInt64Definition temp2() { return getInt64Temp(INT64_PIECES); }
  LInt64Definition temp3() { return getInt64Temp(2 * INT64_PIECES); }

  const MAtomicTypedArrayElementBinop* mir() const {
    return mir_->toAtomicTypedArrayElementBinop();
  }
};

// Atomic binary operation where the result is discarded.
class LAtomicTypedArrayElementBinopForEffect64
    : public LInstructionHelper<0, 3, 2 * INT64_PIECES + 1> {
 public:
  LIR_HEADER(AtomicTypedArrayElementBinopForEffect64)

  // x86
  LAtomicTypedArrayElementBinopForEffect64(const LAllocation& elements,
                                           const LAllocation& index,
                                           const LAllocation& value,
                                           const LInt64Definition& temp,
                                           const LDefinition& tempLow)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp);
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(2 * INT64_PIECES, tempLow);
  }

  // x64
  LAtomicTypedArrayElementBinopForEffect64(const LAllocation& elements,
                                           const LAllocation& index,
                                           const LAllocation& value,
                                           const LInt64Definition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp);
    setInt64Temp(INT64_PIECES, LInt64Definition::BogusTemp());
    setTemp(2 * INT64_PIECES, LDefinition::BogusTemp());
  }

  // ARM64
  LAtomicTypedArrayElementBinopForEffect64(const LAllocation& elements,
                                           const LAllocation& index,
                                           const LAllocation& value,
                                           const LInt64Definition& temp1,
                                           const LInt64Definition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, elements);
    setOperand(1, index);
    setOperand(2, value);
    setInt64Temp(0, temp1);
    setInt64Temp(INT64_PIECES, temp2);
    setTemp(2 * INT64_PIECES, LDefinition::BogusTemp());
  }

  const LAllocation* elements() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  LInt64Definition temp1() { return getInt64Temp(0); }
  LInt64Definition temp2() { return getInt64Temp(INT64_PIECES); }
  const LDefinition* tempLow() { return getTemp(2 * INT64_PIECES); }

  const MAtomicTypedArrayElementBinop* mir() const {
    return mir_->toAtomicTypedArrayElementBinop();
  }
};

class LClampDToUint8 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(ClampDToUint8)

  LClampDToUint8(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }
};

class LClampVToUint8 : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(ClampVToUint8)

  LClampVToUint8(const LBoxAllocation& input, const LDefinition& tempFloat)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, tempFloat);
  }

  static const size_t Input = 0;

  const LDefinition* tempFloat() { return getTemp(0); }
  const MClampToUint8* mir() const { return mir_->toClampToUint8(); }
};

// Load a boxed value from an object's fixed slot.
class LLoadFixedSlotV : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(LoadFixedSlotV)

  explicit LLoadFixedSlotV(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }
  const MLoadFixedSlot* mir() const { return mir_->toLoadFixedSlot(); }
};

// Load a typed value from an object's fixed slot.
class LLoadFixedSlotT : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(LoadFixedSlotT)

  explicit LLoadFixedSlotT(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }
  const MLoadFixedSlot* mir() const { return mir_->toLoadFixedSlot(); }
};

class LAddAndStoreSlot : public LInstructionHelper<0, 1 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(AddAndStoreSlot)

  LAddAndStoreSlot(const LAllocation& obj, const LBoxAllocation& value,
                   const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Value, value);
    setTemp(0, temp);
  }

  static const size_t Value = 1;

  const MAddAndStoreSlot* mir() const { return mir_->toAddAndStoreSlot(); }
};

class LAllocateAndStoreSlot
    : public LCallInstructionHelper<0, 1 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(AllocateAndStoreSlot)

  LAllocateAndStoreSlot(const LAllocation& obj, const LBoxAllocation& value,
                        const LDefinition& temp1, const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Value, value);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static const size_t Value = 1;

  const MAllocateAndStoreSlot* mir() const {
    return mir_->toAllocateAndStoreSlot();
  }
};

// Store a boxed value to an object's fixed slot.
class LStoreFixedSlotV : public LInstructionHelper<0, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(StoreFixedSlotV)

  LStoreFixedSlotV(const LAllocation& obj, const LBoxAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Value, value);
  }

  static const size_t Value = 1;

  const MStoreFixedSlot* mir() const { return mir_->toStoreFixedSlot(); }
  const LAllocation* obj() { return getOperand(0); }
};

// Store a typed value to an object's fixed slot.
class LStoreFixedSlotT : public LInstructionHelper<0, 2, 0> {
 public:
  LIR_HEADER(StoreFixedSlotT)

  LStoreFixedSlotT(const LAllocation& obj, const LAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
  }
  const MStoreFixedSlot* mir() const { return mir_->toStoreFixedSlot(); }
  const LAllocation* obj() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
};

// Note, Name ICs always return a Value. There are no V/T variants.
class LGetNameCache : public LInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(GetNameCache)

  LGetNameCache(const LAllocation& envObj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, envObj);
    setTemp(0, temp);
  }
  const LAllocation* envObj() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const MGetNameCache* mir() const { return mir_->toGetNameCache(); }
};

class LCallGetIntrinsicValue : public LCallInstructionHelper<BOX_PIECES, 0, 0> {
 public:
  LIR_HEADER(CallGetIntrinsicValue)

  const MCallGetIntrinsicValue* mir() const {
    return mir_->toCallGetIntrinsicValue();
  }

  LCallGetIntrinsicValue() : LCallInstructionHelper(classOpcode) {}
};

class LGetPropSuperCache
    : public LInstructionHelper<BOX_PIECES, 1 + 2 * BOX_PIECES, 0> {
 public:
  LIR_HEADER(GetPropSuperCache)

  static const size_t Receiver = 1;
  static const size_t Id = Receiver + BOX_PIECES;

  LGetPropSuperCache(const LAllocation& obj, const LBoxAllocation& receiver,
                     const LBoxAllocation& id)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Receiver, receiver);
    setBoxOperand(Id, id);
  }
  const LAllocation* obj() { return getOperand(0); }
  const MGetPropSuperCache* mir() const { return mir_->toGetPropSuperCache(); }
};

// Patchable jump to stubs generated for a GetProperty cache, which loads a
// boxed value.
class LGetPropertyCache
    : public LInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 0> {
 public:
  LIR_HEADER(GetPropertyCache)

  static const size_t Value = 0;
  static const size_t Id = BOX_PIECES;

  LGetPropertyCache(const LBoxAllocation& value, const LBoxAllocation& id)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
    setBoxOperand(Id, id);
  }
  const MGetPropertyCache* mir() const { return mir_->toGetPropertyCache(); }
};

class LBindNameCache : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(BindNameCache)

  LBindNameCache(const LAllocation& envChain, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, envChain);
    setTemp(0, temp);
  }
  const LAllocation* environmentChain() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const MBindNameCache* mir() const { return mir_->toBindNameCache(); }
};

// Load a value from an object's dslots or a slots vector.
class LLoadDynamicSlotV : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(LoadDynamicSlotV)

  explicit LLoadDynamicSlotV(const LAllocation& in)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
  }
  const MLoadDynamicSlot* mir() const { return mir_->toLoadDynamicSlot(); }
};

// Store a value to an object's dslots or a slots vector.
class LStoreDynamicSlotV : public LInstructionHelper<0, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(StoreDynamicSlotV)

  LStoreDynamicSlotV(const LAllocation& slots, const LBoxAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, slots);
    setBoxOperand(Value, value);
  }

  static const size_t Value = 1;

  const MStoreDynamicSlot* mir() const { return mir_->toStoreDynamicSlot(); }
  const LAllocation* slots() { return getOperand(0); }
};

// Store a typed value to an object's dslots or a slots vector. This has a
// few advantages over LStoreDynamicSlotV:
// 1) We can bypass storing the type tag if the slot has the same type as
//    the value.
// 2) Better register allocation: we can store constants and FP regs directly
//    without requiring a second register for the value.
class LStoreDynamicSlotT : public LInstructionHelper<0, 2, 0> {
 public:
  LIR_HEADER(StoreDynamicSlotT)

  LStoreDynamicSlotT(const LAllocation& slots, const LAllocation& value)
      : LInstructionHelper(classOpcode) {
    setOperand(0, slots);
    setOperand(1, value);
  }
  const MStoreDynamicSlot* mir() const { return mir_->toStoreDynamicSlot(); }
  const LAllocation* slots() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
};

// Round a double precision number and converts it to an int32.
// Implements Math.round().
class LRound : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(Round)

  LRound(const LAllocation& num, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, num);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
  MRound* mir() const { return mir_->toRound(); }
};

// Round a single precision number and converts it to an int32.
// Implements Math.round().
class LRoundF : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(RoundF)

  LRoundF(const LAllocation& num, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, num);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
  MRound* mir() const { return mir_->toRound(); }
};

// Rounds a single precision number accordingly to mir()->roundingMode(),
// and keeps a single output.
class LNearbyIntF : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(NearbyIntF)

  explicit LNearbyIntF(const LAllocation& num)
      : LInstructionHelper(classOpcode) {
    setOperand(0, num);
  }
  MNearbyInt* mir() const { return mir_->toNearbyInt(); }
};

class LCallSetElement
    : public LCallInstructionHelper<0, 1 + 2 * BOX_PIECES, 0> {
 public:
  LIR_HEADER(CallSetElement)

  static const size_t Index = 1;
  static const size_t Value = 1 + BOX_PIECES;

  LCallSetElement(const LAllocation& obj, const LBoxAllocation& index,
                  const LBoxAllocation& value)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Index, index);
    setBoxOperand(Value, value);
  }

  const MCallSetElement* mir() const { return mir_->toCallSetElement(); }
};

class LCallDeleteProperty : public LCallInstructionHelper<1, BOX_PIECES, 0> {
 public:
  LIR_HEADER(CallDeleteProperty)

  static const size_t Value = 0;

  explicit LCallDeleteProperty(const LBoxAllocation& value)
      : LCallInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
  }

  MDeleteProperty* mir() const { return mir_->toDeleteProperty(); }
};

class LCallDeleteElement : public LCallInstructionHelper<1, 2 * BOX_PIECES, 0> {
 public:
  LIR_HEADER(CallDeleteElement)

  static const size_t Value = 0;
  static const size_t Index = BOX_PIECES;

  LCallDeleteElement(const LBoxAllocation& value, const LBoxAllocation& index)
      : LCallInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
    setBoxOperand(Index, index);
  }

  MDeleteElement* mir() const { return mir_->toDeleteElement(); }
};

// Patchable jump to stubs generated for a SetProperty cache.
class LSetPropertyCache : public LInstructionHelper<0, 1 + 2 * BOX_PIECES, 2> {
 public:
  LIR_HEADER(SetPropertyCache)

  // Takes an additional temp: this is intendend to be FloatReg0 to allow the
  // actual cache code to safely clobber that value without save and restore.
  LSetPropertyCache(const LAllocation& object, const LBoxAllocation& id,
                    const LBoxAllocation& value, const LDefinition& temp,
                    const LDefinition& tempDouble)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setBoxOperand(Id, id);
    setBoxOperand(Value, value);
    setTemp(0, temp);
    setTemp(1, tempDouble);
  }

  static const size_t Id = 1;
  static const size_t Value = 1 + BOX_PIECES;

  const MSetPropertyCache* mir() const { return mir_->toSetPropertyCache(); }

  const LDefinition* temp() { return getTemp(0); }
};

class LGetIteratorCache : public LInstructionHelper<1, BOX_PIECES, 2> {
 public:
  LIR_HEADER(GetIteratorCache)

  static const size_t Value = 0;

  LGetIteratorCache(const LBoxAllocation& value, const LDefinition& temp1,
                    const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }
  const MGetIteratorCache* mir() const { return mir_->toGetIteratorCache(); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LOptimizeSpreadCallCache : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(OptimizeSpreadCallCache)

  static const size_t Value = 0;

  LOptimizeSpreadCallCache(const LBoxAllocation& value, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Value, value);
    setTemp(0, temp);
  }
  const MOptimizeSpreadCallCache* mir() const {
    return mir_->toOptimizeSpreadCallCache();
  }
  const LDefinition* temp() { return getTemp(0); }
};

class LIteratorMore : public LInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(IteratorMore)

  LIteratorMore(const LAllocation& iterator, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, iterator);
    setTemp(0, temp);
  }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  MIteratorMore* mir() const { return mir_->toIteratorMore(); }
};

class LIsNoIterAndBranch : public LControlInstructionHelper<2, BOX_PIECES, 0> {
 public:
  LIR_HEADER(IsNoIterAndBranch)

  LIsNoIterAndBranch(MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                     const LBoxAllocation& input)
      : LControlInstructionHelper(classOpcode) {
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
    setBoxOperand(Input, input);
  }

  static const size_t Input = 0;

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

class LIteratorEnd : public LInstructionHelper<0, 1, 3> {
 public:
  LIR_HEADER(IteratorEnd)

  LIteratorEnd(const LAllocation& iterator, const LDefinition& temp1,
               const LDefinition& temp2, const LDefinition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, iterator);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  MIteratorEnd* mir() const { return mir_->toIteratorEnd(); }
};

// Load a value from the actual arguments.
class LGetFrameArgument : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(GetFrameArgument)

  explicit LGetFrameArgument(const LAllocation& index)
      : LInstructionHelper(classOpcode) {
    setOperand(0, index);
  }
  const LAllocation* index() { return getOperand(0); }
};

// Create the rest parameter.
class LRest : public LCallInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(Rest)

  LRest(const LAllocation& numActuals, const LDefinition& temp1,
        const LDefinition& temp2, const LDefinition& temp3)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, numActuals);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }
  const LAllocation* numActuals() { return getOperand(0); }
  MRest* mir() const { return mir_->toRest(); }
};

// Convert a Boolean to an Int64, following ToBigInt.
class LBooleanToInt64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(BooleanToInt64)

  explicit LBooleanToInt64(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(Input, input);
  }

  static const size_t Input = 0;

  const MToInt64* mir() const { return mir_->toToInt64(); }
};

// Convert a String to an Int64, following ToBigInt.
class LStringToInt64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(StringToInt64)

  explicit LStringToInt64(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(Input, input);
  }

  static const size_t Input = 0;

  const MToInt64* mir() const { return mir_->toToInt64(); }
};

// Simulate ToBigInt on a Value and produce a matching Int64.
class LValueToInt64 : public LInstructionHelper<INT64_PIECES, BOX_PIECES, 1> {
 public:
  LIR_HEADER(ValueToInt64)

  explicit LValueToInt64(const LBoxAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, temp);
  }

  static const size_t Input = 0;

  const MToInt64* mir() const { return mir_->toToInt64(); }
  const LDefinition* temp() { return getTemp(0); }
};

// Truncate a BigInt to an unboxed int64.
class LTruncateBigIntToInt64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(TruncateBigIntToInt64)

  explicit LTruncateBigIntToInt64(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(Input, input);
  }

  static const size_t Input = 0;

  const MTruncateBigIntToInt64* mir() const {
    return mir_->toTruncateBigIntToInt64();
  }
};

// Create a new BigInt* from an unboxed int64.
class LInt64ToBigInt : public LInstructionHelper<1, INT64_PIECES, 1> {
 public:
  LIR_HEADER(Int64ToBigInt)

  LInt64ToBigInt(const LInt64Allocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(Input, input);
    setTemp(0, temp);
  }

  static const size_t Input = 0;

  const MInt64ToBigInt* mir() const { return mir_->toInt64ToBigInt(); }
  const LInt64Allocation input() { return getInt64Operand(Input); }
  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing an object to another object.
class LPostWriteBarrierO : public LInstructionHelper<0, 2, 1> {
 public:
  LIR_HEADER(PostWriteBarrierO)

  LPostWriteBarrierO(const LAllocation& obj, const LAllocation& value,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setTemp(0, temp);
  }

  const MPostWriteBarrier* mir() const { return mir_->toPostWriteBarrier(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a string to an object.
class LPostWriteBarrierS : public LInstructionHelper<0, 2, 1> {
 public:
  LIR_HEADER(PostWriteBarrierS)

  LPostWriteBarrierS(const LAllocation& obj, const LAllocation& value,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setTemp(0, temp);
  }

  const MPostWriteBarrier* mir() const { return mir_->toPostWriteBarrier(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a BigInt to an object.
class LPostWriteBarrierBI : public LInstructionHelper<0, 2, 1> {
 public:
  LIR_HEADER(PostWriteBarrierBI)

  LPostWriteBarrierBI(const LAllocation& obj, const LAllocation& value,
                      const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setTemp(0, temp);
  }

  const MPostWriteBarrier* mir() const { return mir_->toPostWriteBarrier(); }
  const LAllocation* object() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a value to another object.
class LPostWriteBarrierV : public LInstructionHelper<0, 1 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(PostWriteBarrierV)

  LPostWriteBarrierV(const LAllocation& obj, const LBoxAllocation& value,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(Input, value);
    setTemp(0, temp);
  }

  static const size_t Input = 1;

  const MPostWriteBarrier* mir() const { return mir_->toPostWriteBarrier(); }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing an object to another object's
// elements.
class LPostWriteElementBarrierO : public LInstructionHelper<0, 3, 1> {
 public:
  LIR_HEADER(PostWriteElementBarrierO)

  LPostWriteElementBarrierO(const LAllocation& obj, const LAllocation& value,
                            const LAllocation& index, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setOperand(2, index);
    setTemp(0, temp);
  }

  const MPostWriteElementBarrier* mir() const {
    return mir_->toPostWriteElementBarrier();
  }

  const LAllocation* object() { return getOperand(0); }

  const LAllocation* value() { return getOperand(1); }

  const LAllocation* index() { return getOperand(2); }

  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a string to an object's
// elements.
class LPostWriteElementBarrierS : public LInstructionHelper<0, 3, 1> {
 public:
  LIR_HEADER(PostWriteElementBarrierS)

  LPostWriteElementBarrierS(const LAllocation& obj, const LAllocation& value,
                            const LAllocation& index, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setOperand(2, index);
    setTemp(0, temp);
  }

  const MPostWriteElementBarrier* mir() const {
    return mir_->toPostWriteElementBarrier();
  }

  const LAllocation* object() { return getOperand(0); }

  const LAllocation* value() { return getOperand(1); }

  const LAllocation* index() { return getOperand(2); }

  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a BigInt to an object's
// elements.
class LPostWriteElementBarrierBI : public LInstructionHelper<0, 3, 1> {
 public:
  LIR_HEADER(PostWriteElementBarrierBI)

  LPostWriteElementBarrierBI(const LAllocation& obj, const LAllocation& value,
                             const LAllocation& index, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, value);
    setOperand(2, index);
    setTemp(0, temp);
  }

  const MPostWriteElementBarrier* mir() const {
    return mir_->toPostWriteElementBarrier();
  }

  const LAllocation* object() { return getOperand(0); }

  const LAllocation* value() { return getOperand(1); }

  const LAllocation* index() { return getOperand(2); }

  const LDefinition* temp() { return getTemp(0); }
};

// Generational write barrier used when writing a value to another object's
// elements.
class LPostWriteElementBarrierV
    : public LInstructionHelper<0, 2 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(PostWriteElementBarrierV)

  LPostWriteElementBarrierV(const LAllocation& obj, const LAllocation& index,
                            const LBoxAllocation& value,
                            const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, index);
    setBoxOperand(Input, value);
    setTemp(0, temp);
  }

  static const size_t Input = 2;

  const MPostWriteElementBarrier* mir() const {
    return mir_->toPostWriteElementBarrier();
  }

  const LAllocation* object() { return getOperand(0); }

  const LAllocation* index() { return getOperand(1); }

  const LDefinition* temp() { return getTemp(0); }
};

class LGuardSpecificAtom : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardSpecificAtom)

  LGuardSpecificAtom(const LAllocation& str, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, str);
    setTemp(0, temp);
  }
  const LAllocation* str() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  const MGuardSpecificAtom* mir() const { return mir_->toGuardSpecificAtom(); }
};

class LGuardStringToInt32 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(GuardStringToInt32)

  LGuardStringToInt32(const LAllocation& str, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, str);
    setTemp(0, temp);
  }

  const LAllocation* string() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardStringToDouble : public LInstructionHelper<1, 1, 2> {
 public:
  LIR_HEADER(GuardStringToDouble)

  LGuardStringToDouble(const LAllocation& str, const LDefinition& temp1,
                       const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, str);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* string() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LGuardShape : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(GuardShape)

  LGuardShape(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }
  const LDefinition* temp() { return getTemp(0); }
  const MGuardShape* mir() const { return mir_->toGuardShape(); }
};

class LGuardProto : public LInstructionHelper<0, 2, 1> {
 public:
  LIR_HEADER(GuardProto)

  LGuardProto(const LAllocation& obj, const LAllocation& expected,
              const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setOperand(1, expected);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* expected() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardNullProto : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardNullProto)

  LGuardNullProto(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIsNativeObject : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsNativeObject)

  LGuardIsNativeObject(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIsProxy : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsProxy)

  LGuardIsProxy(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIsNotProxy : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsNotProxy)

  LGuardIsNotProxy(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIsNotDOMProxy : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsNotDOMProxy)

  LGuardIsNotDOMProxy(const LAllocation& proxy, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, proxy);
    setTemp(0, temp);
  }

  const LAllocation* proxy() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LProxyGet : public LCallInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(ProxyGet)

  LProxyGet(const LAllocation& proxy, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, proxy);
    setTemp(0, temp);
  }

  const LAllocation* proxy() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  MProxyGet* mir() const { return mir_->toProxyGet(); }
};

class LProxyGetByValue
    : public LCallInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(ProxyGetByValue)

  LProxyGetByValue(const LAllocation& proxy, const LBoxAllocation& idVal)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, proxy);
    setBoxOperand(IdIndex, idVal);
  }

  static const size_t IdIndex = 1;

  const LAllocation* proxy() { return getOperand(0); }
};

class LProxyHasProp
    : public LCallInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(ProxyHasProp)

  LProxyHasProp(const LAllocation& proxy, const LBoxAllocation& idVal)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, proxy);
    setBoxOperand(IdIndex, idVal);
  }

  static const size_t IdIndex = 1;

  const LAllocation* proxy() { return getOperand(0); }

  MProxyHasProp* mir() const { return mir_->toProxyHasProp(); }
};

class LProxySet : public LCallInstructionHelper<0, 1 + BOX_PIECES, 1> {
 public:
  LIR_HEADER(ProxySet)

  LProxySet(const LAllocation& proxy, const LBoxAllocation& rhs,
            const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, proxy);
    setBoxOperand(RhsIndex, rhs);
    setTemp(0, temp);
  }

  static const size_t RhsIndex = 1;

  const LAllocation* proxy() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  MProxySet* mir() const { return mir_->toProxySet(); }
};

class LMegamorphicLoadSlot : public LCallInstructionHelper<BOX_PIECES, 1, 3> {
 public:
  LIR_HEADER(MegamorphicLoadSlot)

  LMegamorphicLoadSlot(const LAllocation& obj, const LDefinition& temp1,
                       const LDefinition& temp2, const LDefinition& temp3)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }

  MMegamorphicLoadSlot* mir() const { return mir_->toMegamorphicLoadSlot(); }
};

class LMegamorphicLoadSlotByValue
    : public LCallInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(MegamorphicLoadSlotByValue)

  LMegamorphicLoadSlotByValue(const LAllocation& obj,
                              const LBoxAllocation& idVal,
                              const LDefinition& temp1,
                              const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(IdIndex, idVal);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static const size_t IdIndex = 1;

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  MMegamorphicLoadSlotByValue* mir() const {
    return mir_->toMegamorphicLoadSlotByValue();
  }
};

class LMegamorphicStoreSlot
    : public LCallInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 3> {
 public:
  LIR_HEADER(MegamorphicStoreSlot)

  LMegamorphicStoreSlot(const LAllocation& obj, const LBoxAllocation& rhs,
                        const LDefinition& temp1, const LDefinition& temp2,
                        const LDefinition& temp3)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(RhsIndex, rhs);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  static const size_t RhsIndex = 1;

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }

  MMegamorphicStoreSlot* mir() const { return mir_->toMegamorphicStoreSlot(); }
};

class LMegamorphicHasProp
    : public LCallInstructionHelper<1, 1 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(MegamorphicHasProp)

  LMegamorphicHasProp(const LAllocation& obj, const LBoxAllocation& idVal,
                      const LDefinition& temp1, const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setBoxOperand(IdIndex, idVal);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static const size_t IdIndex = 1;

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  MMegamorphicHasProp* mir() const { return mir_->toMegamorphicHasProp(); }
};

class LGuardIsNotArrayBufferMaybeShared : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsNotArrayBufferMaybeShared)

  LGuardIsNotArrayBufferMaybeShared(const LAllocation& obj,
                                    const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIsTypedArray : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsTypedArray)

  LGuardIsTypedArray(const LAllocation& obj, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, obj);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardNoDenseElements : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardNoDenseElements)

  LGuardNoDenseElements(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
};

class LInCache : public LInstructionHelper<1, BOX_PIECES + 1, 1> {
 public:
  LIR_HEADER(InCache)
  LInCache(const LBoxAllocation& lhs, const LAllocation& rhs,
           const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(LHS, lhs);
    setOperand(RHS, rhs);
    setTemp(0, temp);
  }

  const LAllocation* lhs() { return getOperand(LHS); }
  const LAllocation* rhs() { return getOperand(RHS); }
  const LDefinition* temp() { return getTemp(0); }
  const MInCache* mir() const { return mir_->toInCache(); }

  static const size_t LHS = 0;
  static const size_t RHS = BOX_PIECES;
};

class LNewPrivateName : public LCallInstructionHelper<1, 0, 0> {
 public:
  LIR_HEADER(NewPrivateName)

  LNewPrivateName() : LCallInstructionHelper(classOpcode) {}

  const MNewPrivateName* mir() const { return mir_->toNewPrivateName(); }
};

class LInstanceOfO : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(InstanceOfO)
  explicit LInstanceOfO(const LAllocation& lhs, const LAllocation& rhs)
      : LInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
  }

  MInstanceOf* mir() const { return mir_->toInstanceOf(); }

  const LAllocation* lhs() { return getOperand(0); }
  const LAllocation* rhs() { return getOperand(1); }
};

class LInstanceOfV : public LInstructionHelper<1, BOX_PIECES + 1, 0> {
 public:
  LIR_HEADER(InstanceOfV)
  explicit LInstanceOfV(const LBoxAllocation& lhs, const LAllocation& rhs)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(LHS, lhs);
    setOperand(RHS, rhs);
  }

  MInstanceOf* mir() const { return mir_->toInstanceOf(); }

  const LAllocation* rhs() { return getOperand(RHS); }

  static const size_t LHS = 0;
  static const size_t RHS = BOX_PIECES;
};

class LInstanceOfCache : public LInstructionHelper<1, BOX_PIECES + 1, 0> {
 public:
  LIR_HEADER(InstanceOfCache)
  LInstanceOfCache(const LBoxAllocation& lhs, const LAllocation& rhs)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(LHS, lhs);
    setOperand(RHS, rhs);
  }

  const LDefinition* output() { return this->getDef(0); }
  const LAllocation* rhs() { return getOperand(RHS); }

  static const size_t LHS = 0;
  static const size_t RHS = BOX_PIECES;
};

class LIsCallableO : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(IsCallableO);
  explicit LIsCallableO(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }

  const LAllocation* object() { return getOperand(0); }
  MIsCallable* mir() const { return mir_->toIsCallable(); }
};

class LIsCallableV : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(IsCallableV);
  static const size_t Value = 0;

  LIsCallableV(const LBoxAllocation& value, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(0, value);
    setTemp(0, temp);
  }
  const LDefinition* temp() { return getTemp(0); }
  MIsCallable* mir() const { return mir_->toIsCallable(); }
};

class LIsArrayO : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(IsArrayO);

  explicit LIsArrayO(const LAllocation& object)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
  }
  const LAllocation* object() { return getOperand(0); }
  MIsArray* mir() const { return mir_->toIsArray(); }
};

class LIsArrayV : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(IsArrayV);
  static const size_t Value = 0;

  LIsArrayV(const LBoxAllocation& value, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(0, value);
    setTemp(0, temp);
  }
  const LDefinition* temp() { return getTemp(0); }
  MIsArray* mir() const { return mir_->toIsArray(); }
};

class LIsObjectAndBranch : public LControlInstructionHelper<2, BOX_PIECES, 0> {
 public:
  LIR_HEADER(IsObjectAndBranch)

  LIsObjectAndBranch(MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                     const LBoxAllocation& input)
      : LControlInstructionHelper(classOpcode) {
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
    setBoxOperand(Input, input);
  }

  static const size_t Input = 0;

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }
};

class LIsNullOrUndefinedAndBranch
    : public LControlInstructionHelper<2, BOX_PIECES, 0> {
  MIsNullOrUndefined* isNullOrUndefined_;

 public:
  LIR_HEADER(IsNullOrUndefinedAndBranch)
  static const size_t Input = 0;

  LIsNullOrUndefinedAndBranch(MIsNullOrUndefined* isNullOrUndefined,
                              MBasicBlock* ifTrue, MBasicBlock* ifFalse,
                              const LBoxAllocation& input)
      : LControlInstructionHelper(classOpcode),
        isNullOrUndefined_(isNullOrUndefined) {
    setSuccessor(0, ifTrue);
    setSuccessor(1, ifFalse);
    setBoxOperand(Input, input);
  }

  MBasicBlock* ifTrue() const { return getSuccessor(0); }
  MBasicBlock* ifFalse() const { return getSuccessor(1); }

  MIsNullOrUndefined* isNullOrUndefinedMir() const {
    return isNullOrUndefined_;
  }
};

class LGuardToClass : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(GuardToClass);
  explicit LGuardToClass(const LAllocation& lhs, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* lhs() { return getOperand(0); }

  const LDefinition* temp() { return getTemp(0); }

  MGuardToClass* mir() const { return mir_->toGuardToClass(); }
};

class LObjectClassToString : public LCallInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(ObjectClassToString);

  LObjectClassToString(const LAllocation& lhs, const LDefinition& temp)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }
  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  MObjectClassToString* mir() const { return mir_->toObjectClassToString(); }
};

template <size_t Defs, size_t Ops>
class LWasmSelectBase : public LInstructionHelper<Defs, Ops, 0> {
  typedef LInstructionHelper<Defs, Ops, 0> Base;

 protected:
  explicit LWasmSelectBase(LNode::Opcode opcode) : Base(opcode) {}

 public:
  MWasmSelect* mir() const { return Base::mir_->toWasmSelect(); }
};

class LWasmSelect : public LWasmSelectBase<1, 3> {
 public:
  LIR_HEADER(WasmSelect);

  static const size_t TrueExprIndex = 0;
  static const size_t FalseExprIndex = 1;
  static const size_t CondIndex = 2;

  LWasmSelect(const LAllocation& trueExpr, const LAllocation& falseExpr,
              const LAllocation& cond)
      : LWasmSelectBase(classOpcode) {
    setOperand(TrueExprIndex, trueExpr);
    setOperand(FalseExprIndex, falseExpr);
    setOperand(CondIndex, cond);
  }

  const LAllocation* trueExpr() { return getOperand(TrueExprIndex); }
  const LAllocation* falseExpr() { return getOperand(FalseExprIndex); }
  const LAllocation* condExpr() { return getOperand(CondIndex); }
};

class LWasmSelectI64
    : public LWasmSelectBase<INT64_PIECES, 2 * INT64_PIECES + 1> {
 public:
  LIR_HEADER(WasmSelectI64);

  static const size_t TrueExprIndex = 0;
  static const size_t FalseExprIndex = INT64_PIECES;
  static const size_t CondIndex = INT64_PIECES * 2;

  LWasmSelectI64(const LInt64Allocation& trueExpr,
                 const LInt64Allocation& falseExpr, const LAllocation& cond)
      : LWasmSelectBase(classOpcode) {
    setInt64Operand(TrueExprIndex, trueExpr);
    setInt64Operand(FalseExprIndex, falseExpr);
    setOperand(CondIndex, cond);
  }

  const LInt64Allocation trueExpr() { return getInt64Operand(TrueExprIndex); }
  const LInt64Allocation falseExpr() { return getInt64Operand(FalseExprIndex); }
  const LAllocation* condExpr() { return getOperand(CondIndex); }
};

class LWasmCompareAndSelect : public LWasmSelectBase<1, 4> {
  MCompare::CompareType compareType_;
  JSOp jsop_;

 public:
  LIR_HEADER(WasmCompareAndSelect);

  static const size_t LeftExprIndex = 0;
  static const size_t RightExprIndex = 1;
  static const size_t IfTrueExprIndex = 2;
  static const size_t IfFalseExprIndex = 3;

  LWasmCompareAndSelect(const LAllocation& leftExpr,
                        const LAllocation& rightExpr,
                        MCompare::CompareType compareType, JSOp jsop,
                        const LAllocation& ifTrueExpr,
                        const LAllocation& ifFalseExpr)
      : LWasmSelectBase(classOpcode), compareType_(compareType), jsop_(jsop) {
    setOperand(LeftExprIndex, leftExpr);
    setOperand(RightExprIndex, rightExpr);
    setOperand(IfTrueExprIndex, ifTrueExpr);
    setOperand(IfFalseExprIndex, ifFalseExpr);
  }

  const LAllocation* leftExpr() { return getOperand(LeftExprIndex); }
  const LAllocation* rightExpr() { return getOperand(RightExprIndex); }
  const LAllocation* ifTrueExpr() { return getOperand(IfTrueExprIndex); }
  const LAllocation* ifFalseExpr() { return getOperand(IfFalseExprIndex); }

  MCompare::CompareType compareType() { return compareType_; }
  JSOp jsop() { return jsop_; }
};

class LWasmBoundsCheck64
    : public LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0> {
 public:
  LIR_HEADER(WasmBoundsCheck64);
  explicit LWasmBoundsCheck64(const LInt64Allocation& ptr,
                              const LInt64Allocation& boundsCheckLimit)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, ptr);
    setInt64Operand(INT64_PIECES, boundsCheckLimit);
  }
  MWasmBoundsCheck* mir() const { return mir_->toWasmBoundsCheck(); }
  LInt64Allocation ptr() { return getInt64Operand(0); }
  LInt64Allocation boundsCheckLimit() { return getInt64Operand(INT64_PIECES); }
};

namespace details {

// This is a base class for LWasmLoad/LWasmLoadI64.
template <size_t Defs, size_t Temp>
class LWasmLoadBase : public LInstructionHelper<Defs, 2, Temp> {
 public:
  typedef LInstructionHelper<Defs, 2, Temp> Base;
  explicit LWasmLoadBase(LNode::Opcode opcode, const LAllocation& ptr,
                         const LAllocation& memoryBase)
      : Base(opcode) {
    Base::setOperand(0, ptr);
    Base::setOperand(1, memoryBase);
  }
  MWasmLoad* mir() const { return Base::mir_->toWasmLoad(); }
  const LAllocation* ptr() { return Base::getOperand(0); }
  const LAllocation* memoryBase() { return Base::getOperand(1); }
};

}  // namespace details

class LWasmLoad : public details::LWasmLoadBase<1, 1> {
 public:
  explicit LWasmLoad(const LAllocation& ptr,
                     const LAllocation& memoryBase = LAllocation())
      : LWasmLoadBase(classOpcode, ptr, memoryBase) {
    setTemp(0, LDefinition::BogusTemp());
  }

  const LDefinition* ptrCopy() { return Base::getTemp(0); }

  LIR_HEADER(WasmLoad);
};

class LWasmLoadI64 : public details::LWasmLoadBase<INT64_PIECES, 1> {
 public:
  explicit LWasmLoadI64(const LAllocation& ptr,
                        const LAllocation& memoryBase = LAllocation())
      : LWasmLoadBase(classOpcode, ptr, memoryBase) {
    setTemp(0, LDefinition::BogusTemp());
  }

  const LDefinition* ptrCopy() { return Base::getTemp(0); }

  LIR_HEADER(WasmLoadI64);
};

class LWasmStore : public LInstructionHelper<0, 3, 1> {
 public:
  LIR_HEADER(WasmStore);

  static const size_t PtrIndex = 0;
  static const size_t ValueIndex = 1;
  static const size_t MemoryBaseIndex = 2;

  LWasmStore(const LAllocation& ptr, const LAllocation& value,
             const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(PtrIndex, ptr);
    setOperand(ValueIndex, value);
    setOperand(MemoryBaseIndex, memoryBase);
    setTemp(0, LDefinition::BogusTemp());
  }
  MWasmStore* mir() const { return mir_->toWasmStore(); }
  const LAllocation* ptr() { return getOperand(PtrIndex); }
  const LDefinition* ptrCopy() { return getTemp(0); }
  const LAllocation* value() { return getOperand(ValueIndex); }
  const LAllocation* memoryBase() { return getOperand(MemoryBaseIndex); }
};

class LWasmStoreI64 : public LInstructionHelper<0, INT64_PIECES + 2, 1> {
 public:
  LIR_HEADER(WasmStoreI64);

  static const size_t PtrIndex = 0;
  static const size_t MemoryBaseIndex = 1;
  static const size_t ValueIndex = 2;

  LWasmStoreI64(const LAllocation& ptr, const LInt64Allocation& value,
                const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(PtrIndex, ptr);
    setOperand(MemoryBaseIndex, memoryBase);
    setInt64Operand(ValueIndex, value);
    setTemp(0, LDefinition::BogusTemp());
  }
  MWasmStore* mir() const { return mir_->toWasmStore(); }
  const LAllocation* ptr() { return getOperand(PtrIndex); }
  const LAllocation* memoryBase() { return getOperand(MemoryBaseIndex); }
  const LDefinition* ptrCopy() { return getTemp(0); }
  const LInt64Allocation value() { return getInt64Operand(ValueIndex); }
};

class LWasmCompareExchangeHeap : public LInstructionHelper<1, 4, 4> {
 public:
  LIR_HEADER(WasmCompareExchangeHeap);

  // ARM, ARM64, x86, x64
  LWasmCompareExchangeHeap(const LAllocation& ptr, const LAllocation& oldValue,
                           const LAllocation& newValue,
                           const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, oldValue);
    setOperand(2, newValue);
    setOperand(3, memoryBase);
    setTemp(0, LDefinition::BogusTemp());
  }
  // MIPS32, MIPS64
  LWasmCompareExchangeHeap(const LAllocation& ptr, const LAllocation& oldValue,
                           const LAllocation& newValue,
                           const LDefinition& valueTemp,
                           const LDefinition& offsetTemp,
                           const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, oldValue);
    setOperand(2, newValue);
    setOperand(3, LAllocation());
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, valueTemp);
    setTemp(2, offsetTemp);
    setTemp(3, maskTemp);
  }

  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* oldValue() { return getOperand(1); }
  const LAllocation* newValue() { return getOperand(2); }
  const LAllocation* memoryBase() { return getOperand(3); }
  const LDefinition* addrTemp() { return getTemp(0); }

  void setAddrTemp(const LDefinition& addrTemp) { setTemp(0, addrTemp); }

  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(1); }
  const LDefinition* offsetTemp() { return getTemp(2); }
  const LDefinition* maskTemp() { return getTemp(3); }

  MWasmCompareExchangeHeap* mir() const {
    return mir_->toWasmCompareExchangeHeap();
  }
};

class LWasmAtomicExchangeHeap : public LInstructionHelper<1, 3, 4> {
 public:
  LIR_HEADER(WasmAtomicExchangeHeap);

  // ARM, ARM64, x86, x64
  LWasmAtomicExchangeHeap(const LAllocation& ptr, const LAllocation& value,
                          const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, memoryBase);
    setTemp(0, LDefinition::BogusTemp());
  }
  // MIPS32, MIPS64
  LWasmAtomicExchangeHeap(const LAllocation& ptr, const LAllocation& value,
                          const LDefinition& valueTemp,
                          const LDefinition& offsetTemp,
                          const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, LAllocation());
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, valueTemp);
    setTemp(2, offsetTemp);
    setTemp(3, maskTemp);
  }

  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  const LAllocation* memoryBase() { return getOperand(2); }
  const LDefinition* addrTemp() { return getTemp(0); }

  void setAddrTemp(const LDefinition& addrTemp) { setTemp(0, addrTemp); }

  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(1); }
  const LDefinition* offsetTemp() { return getTemp(2); }
  const LDefinition* maskTemp() { return getTemp(3); }

  MWasmAtomicExchangeHeap* mir() const {
    return mir_->toWasmAtomicExchangeHeap();
  }
};

class LWasmAtomicBinopHeap : public LInstructionHelper<1, 3, 6> {
 public:
  LIR_HEADER(WasmAtomicBinopHeap);

  static const int32_t valueOp = 1;

  // ARM, ARM64, x86, x64
  LWasmAtomicBinopHeap(const LAllocation& ptr, const LAllocation& value,
                       const LDefinition& temp,
                       const LDefinition& flagTemp = LDefinition::BogusTemp(),
                       const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, memoryBase);
    setTemp(0, temp);
    setTemp(1, LDefinition::BogusTemp());
    setTemp(2, flagTemp);
  }
  // MIPS32, MIPS64
  LWasmAtomicBinopHeap(const LAllocation& ptr, const LAllocation& value,
                       const LDefinition& valueTemp,
                       const LDefinition& offsetTemp,
                       const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, LAllocation());
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, LDefinition::BogusTemp());
    setTemp(2, LDefinition::BogusTemp());
    setTemp(3, valueTemp);
    setTemp(4, offsetTemp);
    setTemp(5, maskTemp);
  }
  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* value() {
    MOZ_ASSERT(valueOp == 1);
    return getOperand(1);
  }
  const LAllocation* memoryBase() { return getOperand(2); }
  const LDefinition* temp() { return getTemp(0); }

  // Temp that may be used on some platforms to hold a computed address.
  const LDefinition* addrTemp() { return getTemp(1); }
  void setAddrTemp(const LDefinition& addrTemp) { setTemp(1, addrTemp); }

  // Temp that may be used on LL/SC platforms for the flag result of the store.
  const LDefinition* flagTemp() { return getTemp(2); }
  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(3); }
  const LDefinition* offsetTemp() { return getTemp(4); }
  const LDefinition* maskTemp() { return getTemp(5); }

  MWasmAtomicBinopHeap* mir() const { return mir_->toWasmAtomicBinopHeap(); }
};

// Atomic binary operation where the result is discarded.
class LWasmAtomicBinopHeapForEffect : public LInstructionHelper<0, 3, 5> {
 public:
  LIR_HEADER(WasmAtomicBinopHeapForEffect);
  // ARM, ARM64, x86, x64
  LWasmAtomicBinopHeapForEffect(
      const LAllocation& ptr, const LAllocation& value,
      const LDefinition& flagTemp = LDefinition::BogusTemp(),
      const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, memoryBase);
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, flagTemp);
  }
  // MIPS32, MIPS64
  LWasmAtomicBinopHeapForEffect(const LAllocation& ptr,
                                const LAllocation& value,
                                const LDefinition& valueTemp,
                                const LDefinition& offsetTemp,
                                const LDefinition& maskTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, value);
    setOperand(2, LAllocation());
    setTemp(0, LDefinition::BogusTemp());
    setTemp(1, LDefinition::BogusTemp());
    setTemp(2, valueTemp);
    setTemp(3, offsetTemp);
    setTemp(4, maskTemp);
  }
  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* value() { return getOperand(1); }
  const LAllocation* memoryBase() { return getOperand(2); }

  // Temp that may be used on some platforms to hold a computed address.
  const LDefinition* addrTemp() { return getTemp(0); }
  void setAddrTemp(const LDefinition& addrTemp) { setTemp(0, addrTemp); }

  // Temp that may be used on LL/SC platforms for the flag result of the store.
  const LDefinition* flagTemp() { return getTemp(1); }
  // Temp that may be used on LL/SC platforms for extract/insert bits of word.
  const LDefinition* valueTemp() { return getTemp(2); }
  const LDefinition* offsetTemp() { return getTemp(3); }
  const LDefinition* maskTemp() { return getTemp(4); }

  MWasmAtomicBinopHeap* mir() const { return mir_->toWasmAtomicBinopHeap(); }
};

class LWasmLoadSlot : public LInstructionHelper<1, 1, 0> {
  size_t offset_;
  MIRType type_;

 public:
  LIR_HEADER(WasmLoadSlot);
  explicit LWasmLoadSlot(const LAllocation& containerRef, size_t offset,
                         MIRType type)
      : LInstructionHelper(classOpcode), offset_(offset), type_(type) {
    setOperand(0, containerRef);
  }
  const LAllocation* containerRef() { return getOperand(0); }
  size_t offset() const { return offset_; }
  MIRType type() const { return type_; }
};

class LWasmLoadSlotI64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
  size_t offset_;

 public:
  LIR_HEADER(WasmLoadSlotI64);
  explicit LWasmLoadSlotI64(const LAllocation& containerRef, size_t offset)
      : LInstructionHelper(classOpcode), offset_(offset) {
    setOperand(0, containerRef);
  }
  const LAllocation* containerRef() { return getOperand(0); }
  size_t offset() const { return offset_; }
};

class LWasmStoreSlot : public LInstructionHelper<0, 2, 0> {
  size_t offset_;
  MIRType type_;

 public:
  LIR_HEADER(WasmStoreSlot);
  LWasmStoreSlot(const LAllocation& value, const LAllocation& containerRef,
                 size_t offset, MIRType type)
      : LInstructionHelper(classOpcode), offset_(offset), type_(type) {
    setOperand(0, value);
    setOperand(1, containerRef);
  }
  const LAllocation* value() { return getOperand(0); }
  const LAllocation* containerRef() { return getOperand(1); }
  size_t offset() const { return offset_; }
  MIRType type() const { return type_; }
};

class LWasmStoreSlotI64 : public LInstructionHelper<0, INT64_PIECES + 1, 0> {
  size_t offset_;

 public:
  LIR_HEADER(WasmStoreSlotI64);
  LWasmStoreSlotI64(const LInt64Allocation& value,
                    const LAllocation& containerRef, size_t offset)
      : LInstructionHelper(classOpcode), offset_(offset) {
    setInt64Operand(0, value);
    setOperand(INT64_PIECES, containerRef);
  }
  const LInt64Allocation value() { return getInt64Operand(0); }
  const LAllocation* containerRef() { return getOperand(INT64_PIECES); }
  size_t offset() const { return offset_; }
};

class LWasmDerivedPointer : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmDerivedPointer);
  explicit LWasmDerivedPointer(const LAllocation& base)
      : LInstructionHelper(classOpcode) {
    setOperand(0, base);
  }
  const LAllocation* base() { return getOperand(0); }
  size_t offset() { return mirRaw()->toWasmDerivedPointer()->offset(); }
};

class LWasmStoreRef : public LInstructionHelper<0, 3, 1> {
 public:
  LIR_HEADER(WasmStoreRef);
  LWasmStoreRef(const LAllocation& tls, const LAllocation& valueAddr,
                const LAllocation& value, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, tls);
    setOperand(1, valueAddr);
    setOperand(2, value);
    setTemp(0, temp);
  }
  MWasmStoreRef* mir() const { return mirRaw()->toWasmStoreRef(); }
  const LAllocation* tls() { return getOperand(0); }
  const LAllocation* valueAddr() { return getOperand(1); }
  const LAllocation* value() { return getOperand(2); }
  const LDefinition* temp() { return getTemp(0); }
};

class LWasmParameterI64 : public LInstructionHelper<INT64_PIECES, 0, 0> {
 public:
  LIR_HEADER(WasmParameterI64);

  LWasmParameterI64() : LInstructionHelper(classOpcode) {}
};

class LWasmStackArgI64 : public LInstructionHelper<0, INT64_PIECES, 0> {
 public:
  LIR_HEADER(WasmStackArgI64);
  explicit LWasmStackArgI64(const LInt64Allocation& arg)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, arg);
  }
  MWasmStackArg* mir() const { return mirRaw()->toWasmStackArg(); }
  const LInt64Allocation arg() { return getInt64Operand(0); }
};

class LWasmCall : public LVariadicInstruction<0, 0> {
  bool needsBoundsCheck_;

 public:
  LIR_HEADER(WasmCall);

  LWasmCall(uint32_t numOperands, bool needsBoundsCheck)
      : LVariadicInstruction(classOpcode, numOperands),
        needsBoundsCheck_(needsBoundsCheck) {
    this->setIsCall();
  }

  MWasmCall* mir() const { return mir_->toWasmCall(); }

  static bool isCallPreserved(AnyRegister reg) {
    // All MWasmCalls preserve the TLS register:
    //  - internal/indirect calls do by the internal wasm ABI
    //  - import calls do by explicitly saving/restoring at the callsite
    //  - builtin calls do because the TLS reg is non-volatile
    // See also CodeGeneratorShared::emitWasmCall.
    return !reg.isFloat() && reg.gpr() == WasmTlsReg;
  }

  bool needsBoundsCheck() const { return needsBoundsCheck_; }
};

class LWasmRegisterResult : public LInstructionHelper<1, 0, 0> {
 public:
  LIR_HEADER(WasmRegisterResult);

  LWasmRegisterResult() : LInstructionHelper(classOpcode) {}

  MWasmRegisterResult* mir() const {
    if (!mir_->isWasmRegisterResult()) {
      return nullptr;
    }
    return mir_->toWasmRegisterResult();
  }
};

class LWasmRegisterPairResult : public LInstructionHelper<2, 0, 0> {
 public:
  LIR_HEADER(WasmRegisterPairResult);

  LWasmRegisterPairResult() : LInstructionHelper(classOpcode) {}

  MDefinition* mir() const { return mirRaw(); }
};

class LWasmStackResultArea : public LInstructionHelper<1, 0, 1> {
 public:
  LIR_HEADER(WasmStackResultArea);

  explicit LWasmStackResultArea(const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp);
  }

  MWasmStackResultArea* mir() const { return mir_->toWasmStackResultArea(); }
  const LDefinition* temp() { return getTemp(0); }
};

inline uint32_t LStackArea::base() const {
  return ins()->toWasmStackResultArea()->mir()->base();
}
inline void LStackArea::setBase(uint32_t base) {
  ins()->toWasmStackResultArea()->mir()->setBase(base);
}
inline uint32_t LStackArea::size() const {
  return ins()->toWasmStackResultArea()->mir()->byteSize();
}

inline bool LStackArea::ResultIterator::done() const {
  return idx_ == alloc_.ins()->toWasmStackResultArea()->mir()->resultCount();
}
inline void LStackArea::ResultIterator::next() {
  MOZ_ASSERT(!done());
  idx_++;
}
inline LAllocation LStackArea::ResultIterator::alloc() const {
  MOZ_ASSERT(!done());
  MWasmStackResultArea* area = alloc_.ins()->toWasmStackResultArea()->mir();
  return LStackSlot(area->base() - area->result(idx_).offset());
}
inline bool LStackArea::ResultIterator::isGcPointer() const {
  MOZ_ASSERT(!done());
  MWasmStackResultArea* area = alloc_.ins()->toWasmStackResultArea()->mir();
  MIRType type = area->result(idx_).type();
#ifndef JS_PUNBOX64
  // LDefinition::TypeFrom isn't defined for MIRType::Int64 values on
  // this platform, so here we have a special case.
  if (type == MIRType::Int64) {
    return false;
  }
#endif
  return LDefinition::TypeFrom(type) == LDefinition::OBJECT;
}

class LWasmStackResult : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmStackResult);

  LWasmStackResult() : LInstructionHelper(classOpcode) {}

  MWasmStackResult* mir() const { return mir_->toWasmStackResult(); }
  LStackSlot result(uint32_t base) const {
    return LStackSlot(base - mir()->result().offset());
  }
};

class LWasmStackResult64 : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(WasmStackResult64);

  LWasmStackResult64() : LInstructionHelper(classOpcode) {}

  MWasmStackResult* mir() const { return mir_->toWasmStackResult(); }
  LStackSlot result(uint32_t base, LDefinition* def) {
    uint32_t offset = base - mir()->result().offset();
#if defined(JS_NUNBOX32)
    if (def == getDef(INT64LOW_INDEX)) {
      offset -= INT64LOW_OFFSET;
    } else {
      MOZ_ASSERT(def == getDef(INT64HIGH_INDEX));
      offset -= INT64HIGH_OFFSET;
    }
#else
    MOZ_ASSERT(def == getDef(0));
#endif
    return LStackSlot(offset);
  }
};

inline LStackSlot LStackArea::resultAlloc(LInstruction* lir,
                                          LDefinition* def) const {
  if (lir->isWasmStackResult64()) {
    return lir->toWasmStackResult64()->result(base(), def);
  }
  MOZ_ASSERT(def == lir->getDef(0));
  return lir->toWasmStackResult()->result(base());
}

inline bool LNode::isCallPreserved(AnyRegister reg) const {
  return isWasmCall() && LWasmCall::isCallPreserved(reg);
}

class LAssertRangeI : public LInstructionHelper<0, 1, 0> {
 public:
  LIR_HEADER(AssertRangeI)

  explicit LAssertRangeI(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }

  const LAllocation* input() { return getOperand(0); }

  MAssertRange* mir() { return mir_->toAssertRange(); }
  const Range* range() { return mir()->assertedRange(); }
};

class LAssertRangeD : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(AssertRangeD)

  LAssertRangeD(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LAllocation* input() { return getOperand(0); }

  const LDefinition* temp() { return getTemp(0); }

  MAssertRange* mir() { return mir_->toAssertRange(); }
  const Range* range() { return mir()->assertedRange(); }
};

class LAssertRangeF : public LInstructionHelper<0, 1, 2> {
 public:
  LIR_HEADER(AssertRangeF)
  LAssertRangeF(const LAllocation& input, const LDefinition& temp,
                const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
    setTemp(1, temp2);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  MAssertRange* mir() { return mir_->toAssertRange(); }
  const Range* range() { return mir()->assertedRange(); }
};

class LAssertRangeV : public LInstructionHelper<0, BOX_PIECES, 3> {
 public:
  LIR_HEADER(AssertRangeV)

  LAssertRangeV(const LBoxAllocation& input, const LDefinition& temp,
                const LDefinition& floatTemp1, const LDefinition& floatTemp2)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, temp);
    setTemp(1, floatTemp1);
    setTemp(2, floatTemp2);
  }

  static const size_t Input = 0;

  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* floatTemp1() { return getTemp(1); }
  const LDefinition* floatTemp2() { return getTemp(2); }

  MAssertRange* mir() { return mir_->toAssertRange(); }
  const Range* range() { return mir()->assertedRange(); }
};

class LAssertClass : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(AssertClass)

  explicit LAssertClass(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LAllocation* input() { return getOperand(0); }

  MAssertClass* mir() { return mir_->toAssertClass(); }
};

class LGuardFunctionIsNonBuiltinCtor : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardFunctionIsNonBuiltinCtor)

  LGuardFunctionIsNonBuiltinCtor(const LAllocation& fun,
                                 const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, fun);
    setTemp(0, temp);
  }

  const LAllocation* function() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardFunctionKind : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardFunctionKind)

  explicit LGuardFunctionKind(const LAllocation& fun, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, fun);
    setTemp(0, temp);
  }

  const LAllocation* function() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }

  MGuardFunctionKind* mir() { return mir_->toGuardFunctionKind(); }
};

class LIncrementWarmUpCounter : public LInstructionHelper<0, 0, 1> {
 public:
  LIR_HEADER(IncrementWarmUpCounter)

  explicit LIncrementWarmUpCounter(const LDefinition& scratch)
      : LInstructionHelper(classOpcode) {
    setTemp(0, scratch);
  }

  const LDefinition* scratch() { return getTemp(0); }
  MIncrementWarmUpCounter* mir() { return mir_->toIncrementWarmUpCounter(); }
};

class LMemoryBarrier : public LInstructionHelper<0, 0, 0> {
 private:
  const MemoryBarrierBits type_;

 public:
  LIR_HEADER(MemoryBarrier)

  // The parameter 'type' is a bitwise 'or' of the barrier types needed,
  // see AtomicOp.h.
  explicit LMemoryBarrier(MemoryBarrierBits type)
      : LInstructionHelper(classOpcode), type_(type) {
    MOZ_ASSERT((type_ & ~MembarAllbits) == MembarNobits);
  }

  MemoryBarrierBits type() const { return type_; }
};

class LDebugger : public LCallInstructionHelper<0, 0, 2> {
 public:
  LIR_HEADER(Debugger)

  LDebugger(const LDefinition& temp1, const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setTemp(0, temp1);
    setTemp(1, temp2);
  }
};

class LNewTarget : public LInstructionHelper<BOX_PIECES, 0, 0> {
 public:
  LIR_HEADER(NewTarget)

  LNewTarget() : LInstructionHelper(classOpcode) {}
};

class LArrowNewTarget : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  explicit LArrowNewTarget(const LAllocation& callee)
      : LInstructionHelper(classOpcode) {
    setOperand(0, callee);
  }

  LIR_HEADER(ArrowNewTarget)

  const LAllocation* callee() { return getOperand(0); }
};

// Math.random().
class LRandom : public LInstructionHelper<1, 0, 1 + 2 * INT64_PIECES> {
 public:
  LIR_HEADER(Random)
  LRandom(const LDefinition& temp0, const LInt64Definition& temp1,
          const LInt64Definition& temp2)
      : LInstructionHelper(classOpcode) {
    setTemp(0, temp0);
    setInt64Temp(1, temp1);
    setInt64Temp(1 + INT64_PIECES, temp2);
  }
  const LDefinition* temp0() { return getTemp(0); }
  LInt64Definition temp1() { return getInt64Temp(1); }
  LInt64Definition temp2() { return getInt64Temp(1 + INT64_PIECES); }

  MRandom* mir() const { return mir_->toRandom(); }
};

class LCheckReturn : public LInstructionHelper<BOX_PIECES, 2 * BOX_PIECES, 0> {
 public:
  LIR_HEADER(CheckReturn)

  LCheckReturn(const LBoxAllocation& retVal, const LBoxAllocation& thisVal)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(ReturnValue, retVal);
    setBoxOperand(ThisValue, thisVal);
  }

  static const size_t ReturnValue = 0;
  static const size_t ThisValue = BOX_PIECES;
};

class LCheckClassHeritage : public LInstructionHelper<0, BOX_PIECES, 2> {
 public:
  LIR_HEADER(CheckClassHeritage)

  static const size_t Heritage = 0;

  LCheckClassHeritage(const LBoxAllocation& value, const LDefinition& temp1,
                      const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Heritage, value);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LMaybeExtractAwaitValue
    : public LCallInstructionHelper</* defs= */ BOX_PIECES,
                                    /* defs= */ BOX_PIECES + 1,
                                    /* temps = */ 0> {
 public:
  LIR_HEADER(MaybeExtractAwaitValue);

  static const size_t ValueInput = 0;
  static const size_t CanSkipInput = BOX_PIECES;

  explicit LMaybeExtractAwaitValue(const LBoxAllocation& value,
                                   const LAllocation& canSkip)
      : LCallInstructionHelper(classOpcode) {
    setBoxOperand(ValueInput, value);
    setOperand(CanSkipInput, canSkip);
  }

  MMaybeExtractAwaitValue* mir() { return mir_->toMaybeExtractAwaitValue(); }
  const LAllocation* canSkip() { return getOperand(CanSkipInput); }
};

class LFinishBoundFunctionInit : public LInstructionHelper<0, 3, 2> {
 public:
  LIR_HEADER(FinishBoundFunctionInit)

  LFinishBoundFunctionInit(const LAllocation& bound, const LAllocation& target,
                           const LAllocation& argCount,
                           const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, bound);
    setOperand(1, target);
    setOperand(2, argCount);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* bound() { return getOperand(0); }
  const LAllocation* target() { return getOperand(1); }
  const LAllocation* argCount() { return getOperand(2); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LIsPackedArray : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(IsPackedArray)

  LIsPackedArray(const LAllocation& object, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardArrayIsPacked : public LInstructionHelper<0, 1, 2> {
 public:
  LIR_HEADER(GuardArrayIsPacked)

  explicit LGuardArrayIsPacked(const LAllocation& array,
                               const LDefinition& temp1,
                               const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, array);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* array() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }

  MGuardArrayIsPacked* mir() { return mir_->toGuardArrayIsPacked(); }
};

class LGetPrototypeOf : public LInstructionHelper<BOX_PIECES, 1, 0> {
 public:
  LIR_HEADER(GetPrototypeOf)

  explicit LGetPrototypeOf(const LAllocation& target)
      : LInstructionHelper(classOpcode) {
    setOperand(0, target);
  }

  const LAllocation* target() { return getOperand(0); }
};

class LSuperFunction : public LInstructionHelper<BOX_PIECES, 1, 1> {
 public:
  LIR_HEADER(SuperFunction)

  explicit LSuperFunction(const LAllocation& callee, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, callee);
    setTemp(0, temp);
  }

  const LAllocation* callee() { return getOperand(0); }
  const LDefinition* temp() { return this->getTemp(0); }
};

class LGuardHasGetterSetter : public LCallInstructionHelper<0, 1, 3> {
 public:
  LIR_HEADER(GuardHasGetterSetter)

  LGuardHasGetterSetter(const LAllocation& object, const LDefinition& temp1,
                        const LDefinition& temp2, const LDefinition& temp3)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, object);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }

  MGuardHasGetterSetter* mir() const { return mir_->toGuardHasGetterSetter(); }
};

class LGuardIsExtensible : public LInstructionHelper<0, 1, 1> {
 public:
  LIR_HEADER(GuardIsExtensible)

  LGuardIsExtensible(const LAllocation& object, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setTemp(0, temp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
};

class LGuardIndexGreaterThanDenseInitLength
    : public LInstructionHelper<0, 2, 2> {
 public:
  LIR_HEADER(GuardIndexGreaterThanDenseInitLength)

  LGuardIndexGreaterThanDenseInitLength(const LAllocation& object,
                                        const LAllocation& index,
                                        const LDefinition& temp,
                                        const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
    setTemp(0, temp);
    setTemp(1, spectreTemp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* spectreTemp() { return getTemp(1); }
};

class LGuardIndexIsValidUpdateOrAdd : public LInstructionHelper<0, 2, 2> {
 public:
  LIR_HEADER(GuardIndexIsValidUpdateOrAdd)

  LGuardIndexIsValidUpdateOrAdd(const LAllocation& object,
                                const LAllocation& index,
                                const LDefinition& temp,
                                const LDefinition& spectreTemp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
    setTemp(0, temp);
    setTemp(1, spectreTemp);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp() { return getTemp(0); }
  const LDefinition* spectreTemp() { return getTemp(1); }
};

class LCallGetSparseElement : public LCallInstructionHelper<BOX_PIECES, 2, 0> {
 public:
  LIR_HEADER(CallGetSparseElement)

  LCallGetSparseElement(const LAllocation& object, const LAllocation& index)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
};

class LCallNativeGetElement : public LCallInstructionHelper<BOX_PIECES, 2, 0> {
 public:
  LIR_HEADER(CallNativeGetElement)

  LCallNativeGetElement(const LAllocation& object, const LAllocation& index)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
};

class LCallObjectHasSparseElement : public LCallInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(CallObjectHasSparseElement)

  LCallObjectHasSparseElement(const LAllocation& object,
                              const LAllocation& index,
                              const LDefinition& temp1,
                              const LDefinition& temp2)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, object);
    setOperand(1, index);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  const LAllocation* object() { return getOperand(0); }
  const LAllocation* index() { return getOperand(1); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LBigIntAsIntN64 : public LInstructionHelper<1, 1, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(BigIntAsIntN64)

  LBigIntAsIntN64(const LAllocation& input, const LDefinition& temp,
                  const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  LInt64Definition temp64() { return getInt64Temp(1); }
};

class LBigIntAsIntN32 : public LInstructionHelper<1, 1, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(BigIntAsIntN32)

  LBigIntAsIntN32(const LAllocation& input, const LDefinition& temp,
                  const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  LInt64Definition temp64() { return getInt64Temp(1); }
};

class LBigIntAsUintN64 : public LInstructionHelper<1, 1, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(BigIntAsUintN64)

  LBigIntAsUintN64(const LAllocation& input, const LDefinition& temp,
                   const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  LInt64Definition temp64() { return getInt64Temp(1); }
};

class LBigIntAsUintN32 : public LInstructionHelper<1, 1, 1 + INT64_PIECES> {
 public:
  LIR_HEADER(BigIntAsUintN32)

  LBigIntAsUintN32(const LAllocation& input, const LDefinition& temp,
                   const LInt64Definition& temp64)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
    setInt64Temp(1, temp64);
  }

  const LAllocation* input() { return getOperand(0); }
  const LDefinition* temp() { return getTemp(0); }
  LInt64Definition temp64() { return getInt64Temp(1); }
};

class LGuardNonGCThing : public LInstructionHelper<0, BOX_PIECES, 0> {
 public:
  LIR_HEADER(GuardNonGCThing)

  explicit LGuardNonGCThing(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  static constexpr size_t Input = 0;
};

class LToHashableNonGCThing
    : public LInstructionHelper<BOX_PIECES, BOX_PIECES, 1> {
 public:
  LIR_HEADER(ToHashableNonGCThing)

  LToHashableNonGCThing(const LBoxAllocation& input,
                        const LDefinition& tempFloat)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, tempFloat);
  }

  static constexpr size_t Input = 0;

  const LDefinition* tempFloat() { return getTemp(0); }
};

class LToHashableString : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(ToHashableString)

  explicit LToHashableString(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }
};

class LToHashableValue : public LInstructionHelper<BOX_PIECES, BOX_PIECES, 1> {
 public:
  LIR_HEADER(ToHashableValue)

  LToHashableValue(const LBoxAllocation& input, const LDefinition& tempFloat)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, tempFloat);
  }

  static constexpr size_t Input = 0;

  const LDefinition* tempFloat() { return getTemp(0); }
};

class LHashNonGCThing : public LInstructionHelper<1, BOX_PIECES, 1> {
 public:
  LIR_HEADER(HashNonGCThing)

  LHashNonGCThing(const LBoxAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
    setTemp(0, temp);
  }

  static constexpr size_t Input = 0;

  const LDefinition* temp() { return getTemp(0); }
};

class LHashString : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(HashString)

  LHashString(const LAllocation& input, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp);
  }

  const LDefinition* temp() { return getTemp(0); }
};

class LHashSymbol : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(HashSymbol)

  explicit LHashSymbol(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }
};

class LHashBigInt : public LInstructionHelper<1, 1, 3> {
 public:
  LIR_HEADER(HashBigInt)

  LHashBigInt(const LAllocation& input, const LDefinition& temp1,
              const LDefinition& temp2, const LDefinition& temp3)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
  }

  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
};

class LHashObject : public LInstructionHelper<1, 1 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(HashObject)

  LHashObject(const LAllocation& setObject, const LBoxAllocation& input,
              const LDefinition& temp1, const LDefinition& temp2,
              const LDefinition& temp3, const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LHashValue : public LInstructionHelper<1, 1 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(HashValue)

  LHashValue(const LAllocation& setObject, const LBoxAllocation& input,
             const LDefinition& temp1, const LDefinition& temp2,
             const LDefinition& temp3, const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LSetObjectHasNonBigInt : public LInstructionHelper<1, 2 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(SetObjectHasNonBigInt)

  LSetObjectHasNonBigInt(const LAllocation& setObject,
                         const LBoxAllocation& input, const LAllocation& hash,
                         const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LSetObjectHasBigInt : public LInstructionHelper<1, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(SetObjectHasBigInt)

  LSetObjectHasBigInt(const LAllocation& setObject, const LBoxAllocation& input,
                      const LAllocation& hash, const LDefinition& temp1,
                      const LDefinition& temp2, const LDefinition& temp3,
                      const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LSetObjectHasValue : public LInstructionHelper<1, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(SetObjectHasValue)

  LSetObjectHasValue(const LAllocation& setObject, const LBoxAllocation& input,
                     const LAllocation& hash, const LDefinition& temp1,
                     const LDefinition& temp2, const LDefinition& temp3,
                     const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LSetObjectHasValueVMCall
    : public LCallInstructionHelper<1, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(SetObjectHasValueVMCall)

  LSetObjectHasValueVMCall(const LAllocation& setObject,
                           const LBoxAllocation& input)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, setObject);
    setBoxOperand(Input, input);
  }

  static constexpr size_t Input = 1;

  const LAllocation* setObject() { return getOperand(0); }
};

class LMapObjectHasNonBigInt : public LInstructionHelper<1, 2 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(MapObjectHasNonBigInt)

  LMapObjectHasNonBigInt(const LAllocation& mapObject,
                         const LBoxAllocation& input, const LAllocation& hash,
                         const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LMapObjectHasBigInt : public LInstructionHelper<1, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(MapObjectHasBigInt)

  LMapObjectHasBigInt(const LAllocation& mapObject, const LBoxAllocation& input,
                      const LAllocation& hash, const LDefinition& temp1,
                      const LDefinition& temp2, const LDefinition& temp3,
                      const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LMapObjectHasValue : public LInstructionHelper<1, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(MapObjectHasValue)

  LMapObjectHasValue(const LAllocation& mapObject, const LBoxAllocation& input,
                     const LAllocation& hash, const LDefinition& temp1,
                     const LDefinition& temp2, const LDefinition& temp3,
                     const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LMapObjectHasValueVMCall
    : public LCallInstructionHelper<1, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(MapObjectHasValueVMCall)

  LMapObjectHasValueVMCall(const LAllocation& mapObject,
                           const LBoxAllocation& input)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
};

class LMapObjectGetNonBigInt
    : public LInstructionHelper<BOX_PIECES, 2 + BOX_PIECES, 2> {
 public:
  LIR_HEADER(MapObjectGetNonBigInt)

  LMapObjectGetNonBigInt(const LAllocation& mapObject,
                         const LBoxAllocation& input, const LAllocation& hash,
                         const LDefinition& temp1, const LDefinition& temp2)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
};

class LMapObjectGetBigInt
    : public LInstructionHelper<BOX_PIECES, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(MapObjectGetBigInt)

  LMapObjectGetBigInt(const LAllocation& mapObject, const LBoxAllocation& input,
                      const LAllocation& hash, const LDefinition& temp1,
                      const LDefinition& temp2, const LDefinition& temp3,
                      const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LMapObjectGetValue
    : public LInstructionHelper<BOX_PIECES, 2 + BOX_PIECES, 4> {
 public:
  LIR_HEADER(MapObjectGetValue)

  LMapObjectGetValue(const LAllocation& mapObject, const LBoxAllocation& input,
                     const LAllocation& hash, const LDefinition& temp1,
                     const LDefinition& temp2, const LDefinition& temp3,
                     const LDefinition& temp4)
      : LInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
    setOperand(Input + BOX_PIECES, hash);
    setTemp(0, temp1);
    setTemp(1, temp2);
    setTemp(2, temp3);
    setTemp(3, temp4);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
  const LAllocation* hash() { return getOperand(Input + BOX_PIECES); }
  const LDefinition* temp1() { return getTemp(0); }
  const LDefinition* temp2() { return getTemp(1); }
  const LDefinition* temp3() { return getTemp(2); }
  const LDefinition* temp4() { return getTemp(3); }
};

class LMapObjectGetValueVMCall
    : public LCallInstructionHelper<BOX_PIECES, 1 + BOX_PIECES, 0> {
 public:
  LIR_HEADER(MapObjectGetValueVMCall)

  LMapObjectGetValueVMCall(const LAllocation& mapObject,
                           const LBoxAllocation& input)
      : LCallInstructionHelper(classOpcode) {
    setOperand(0, mapObject);
    setBoxOperand(Input, input);
  }

  static constexpr size_t Input = 1;

  const LAllocation* mapObject() { return getOperand(0); }
};

template <size_t NumDefs>
class LIonToWasmCallBase : public LVariadicInstruction<NumDefs, 2> {
  using Base = LVariadicInstruction<NumDefs, 2>;

 public:
  explicit LIonToWasmCallBase(LNode::Opcode classOpcode, uint32_t numOperands,
                              const LDefinition& temp, const LDefinition& fp)
      : Base(classOpcode, numOperands) {
    this->setIsCall();
    this->setTemp(0, temp);
    this->setTemp(1, fp);
  }
  MIonToWasmCall* mir() const { return this->mir_->toIonToWasmCall(); }
  const LDefinition* temp() { return this->getTemp(0); }
};

class LIonToWasmCall : public LIonToWasmCallBase<1> {
 public:
  LIR_HEADER(IonToWasmCall);
  LIonToWasmCall(uint32_t numOperands, const LDefinition& temp,
                 const LDefinition& fp)
      : LIonToWasmCallBase<1>(classOpcode, numOperands, temp, fp) {}
};

class LIonToWasmCallV : public LIonToWasmCallBase<BOX_PIECES> {
 public:
  LIR_HEADER(IonToWasmCallV);
  LIonToWasmCallV(uint32_t numOperands, const LDefinition& temp,
                  const LDefinition& fp)
      : LIonToWasmCallBase<BOX_PIECES>(classOpcode, numOperands, temp, fp) {}
};

class LIonToWasmCallI64 : public LIonToWasmCallBase<INT64_PIECES> {
 public:
  LIR_HEADER(IonToWasmCallI64);
  LIonToWasmCallI64(uint32_t numOperands, const LDefinition& temp,
                    const LDefinition& fp)
      : LIonToWasmCallBase<INT64_PIECES>(classOpcode, numOperands, temp, fp) {}
};

class LWasmBoxValue : public LInstructionHelper<1, BOX_PIECES, 0> {
 public:
  LIR_HEADER(WasmBoxValue)

  explicit LWasmBoxValue(const LBoxAllocation& input)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(Input, input);
  }

  static const size_t Input = 0;
};
// Wasm SIMD.

// Constant Simd128
class LSimd128 : public LInstructionHelper<1, 0, 0> {
  SimdConstant v_;

 public:
  LIR_HEADER(Simd128);

  explicit LSimd128(SimdConstant v) : LInstructionHelper(classOpcode), v_(v) {}

  const SimdConstant& getSimd128() const { return v_; }
};

// (v128, v128, v128) -> v128 effect-free operation.
// temp is FPR (and always in use).
class LWasmBitselectSimd128 : public LInstructionHelper<1, 3, 1> {
 public:
  LIR_HEADER(WasmBitselectSimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;
  static constexpr uint32_t Control = 2;

  LWasmBitselectSimd128(const LAllocation& lhs, const LAllocation& rhs,
                        const LAllocation& control, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(Lhs, lhs);
    setOperand(Rhs, rhs);
    setOperand(Control, control);
    setTemp(0, temp);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  const LAllocation* control() { return getOperand(Control); }
  const LDefinition* temp() { return getTemp(0); }
};

// (v128, v128) -> v128 effect-free operations
// lhs and dest are the same.
// temps (if in use) are FPR.
// The op may differ from the MIR node's op.
class LWasmBinarySimd128 : public LInstructionHelper<1, 2, 2> {
  wasm::SimdOp op_;

 public:
  LIR_HEADER(WasmBinarySimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmBinarySimd128(wasm::SimdOp op, const LAllocation& lhs,
                     const LAllocation& rhs, const LDefinition& temp0,
                     const LDefinition& temp1)
      : LInstructionHelper(classOpcode), op_(op) {
    setOperand(Lhs, lhs);
    setOperand(Rhs, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  wasm::SimdOp simdOp() const { return op_; }

  static bool SpecializeForConstantRhs(wasm::SimdOp op);
};

class LWasmBinarySimd128WithConstant : public LInstructionHelper<1, 1, 0> {
  SimdConstant rhs_;

 public:
  LIR_HEADER(WasmBinarySimd128WithConstant)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;

  LWasmBinarySimd128WithConstant(const LAllocation& lhs,
                                 const SimdConstant& rhs)
      : LInstructionHelper(classOpcode), rhs_(rhs) {
    setOperand(Lhs, lhs);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const SimdConstant& rhs() { return rhs_; }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmBinarySimd128WithConstant()->simdOp();
  }
};

// (v128, i32) -> v128 effect-free variable-width shift operations
// lhs and dest are the same.
// temp0 is a GPR (if in use).
// temp1 is an FPR (if in use).
class LWasmVariableShiftSimd128 : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(WasmVariableShiftSimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmVariableShiftSimd128(const LAllocation& lhs, const LAllocation& rhs,
                            const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(Lhs, lhs);
    setOperand(Rhs, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  wasm::SimdOp simdOp() const { return mir_->toWasmShiftSimd128()->simdOp(); }
};

// (v128, i32) -> v128 effect-free constant-width shift operations
class LWasmConstantShiftSimd128 : public LInstructionHelper<1, 1, 0> {
  int32_t shift_;

 public:
  LIR_HEADER(WasmConstantShiftSimd128)

  static constexpr uint32_t Src = 0;

  LWasmConstantShiftSimd128(const LAllocation& src, int32_t shift)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  int32_t shift() { return shift_; }
  wasm::SimdOp simdOp() const { return mir_->toWasmShiftSimd128()->simdOp(); }
};

// (v128) -> v128 sign replication operation.
class LWasmSignReplicationSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmSignReplicationSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmSignReplicationSimd128(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  wasm::SimdOp simdOp() const { return mir_->toWasmShiftSimd128()->simdOp(); }
};

// (v128, v128, imm_simd) -> v128 effect-free operation.
// temp is FPR (and always in use).
class LWasmShuffleSimd128 : public LInstructionHelper<1, 2, 1> {
 public:
  // Shuffle operations.  NOTE: these may still be x86-centric, but the set can
  // accomodate operations from other architectures.
  enum Op {
    // Blend bytes.  control_ has the blend mask as an I8x16: 0 to select from
    // the lhs, -1 to select from the rhs.
    BLEND_8x16,

    // Blend words.  control_ has the blend mask as an I16x8: 0 to select from
    // the lhs, -1 to select from the rhs.
    BLEND_16x8,

    // Concat the lhs in front of the rhs and shift right by bytes, extracting
    // the low 16 bytes; control_[0] has the shift count.
    CONCAT_RIGHT_SHIFT_8x16,

    // Interleave qwords/dwords/words/bytes from high/low halves of operands.
    // The low-order item in the result comes from the lhs, then the next from
    // the rhs, and so on.  control_ is ignored.
    INTERLEAVE_HIGH_8x16,
    INTERLEAVE_HIGH_16x8,
    INTERLEAVE_HIGH_32x4,
    INTERLEAVE_HIGH_64x2,
    INTERLEAVE_LOW_8x16,
    INTERLEAVE_LOW_16x8,
    INTERLEAVE_LOW_32x4,
    INTERLEAVE_LOW_64x2,

    // Fully general shuffle+blend.  control_ has the shuffle mask.
    SHUFFLE_BLEND_8x16,
  };

 private:
  Op op_;
  SimdConstant control_;

 public:
  LIR_HEADER(WasmShuffleSimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmShuffleSimd128(const LAllocation& lhs, const LAllocation& rhs,
                      const LDefinition& temp, Op op, SimdConstant control)
      : LInstructionHelper(classOpcode), op_(op), control_(control) {
    setOperand(Lhs, lhs);
    setOperand(Rhs, rhs);
    setTemp(0, temp);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  const LDefinition* temp() { return getTemp(0); }
  Op op() { return op_; }
  SimdConstant control() { return control_; }
};

// (v128, imm_simd) -> v128 effect-free operation.
class LWasmPermuteSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  // Permutation operations.  NOTE: these may still be x86-centric, but the set
  // can accomodate operations from other architectures.
  //
  // The "low-order" byte is in lane 0 of an 8x16 datum, the "high-order" byte
  // in lane 15.  The low-order byte is also the "rightmost".  In wasm, the
  // constant (v128.const i8x16 0 1 2 ... 15) has 0 in the low-order byte and 15
  // in the high-order byte.
  enum Op {
    // A single byte lane is copied into all the other byte lanes.  control_[0]
    // has the source lane.
    BROADCAST_8x16,

    // A single word lane is copied into all the other word lanes.  control_[0]
    // has the source lane.
    BROADCAST_16x8,

    // Copy input to output.
    MOVE,

    // control_ has bytes in range 0..15 s.t. control_[i] holds the source lane
    // for output lane i.
    PERMUTE_8x16,

    // control_ has int16s in range 0..7, as for 8x16.  In addition, the high
    // byte of control_[0] has flags detailing the operation, values taken
    // from the Perm16x8Action enum below.
    PERMUTE_16x8,

    // control_ has int32s in range 0..3, as for 8x16.
    PERMUTE_32x4,

    // control_[0] has the number of places to rotate by.
    ROTATE_RIGHT_8x16,

    // Zeroes are shifted into high-order bytes and low-order bytes are lost.
    // control_[0] has the number of places to shift by.
    SHIFT_RIGHT_8x16,

    // Zeroes are shifted into low-order bytes and high-order bytes are lost.
    // control_[0] has the number of places to shift by.
    SHIFT_LEFT_8x16,
  };

 private:
  Op op_;
  SimdConstant control_;

 public:
  LIR_HEADER(WasmPermuteSimd128)

  static constexpr uint32_t Src = 0;

  LWasmPermuteSimd128(const LAllocation& src, Op op, SimdConstant control)
      : LInstructionHelper(classOpcode), op_(op), control_(control) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  Op op() { return op_; }
  SimdConstant control() { return control_; }
};

class LWasmReplaceLaneSimd128 : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(WasmReplaceLaneSimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmReplaceLaneSimd128(const LAllocation& lhs, const LAllocation& rhs)
      : LInstructionHelper(classOpcode) {
    setOperand(Lhs, lhs);
    setOperand(Rhs, rhs);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  uint32_t laneIndex() const {
    return mir_->toWasmReplaceLaneSimd128()->laneIndex();
  }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmReplaceLaneSimd128()->simdOp();
  }
};

class LWasmReplaceInt64LaneSimd128
    : public LInstructionHelper<1, INT64_PIECES + 1, 0> {
 public:
  LIR_HEADER(WasmReplaceInt64LaneSimd128)

  static constexpr uint32_t Lhs = 0;
  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmReplaceInt64LaneSimd128(const LAllocation& lhs,
                               const LInt64Allocation& rhs)
      : LInstructionHelper(classOpcode) {
    setOperand(Lhs, lhs);
    setInt64Operand(Rhs, rhs);
  }

  const LAllocation* lhs() { return getOperand(Lhs); }
  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LInt64Allocation rhs() { return getInt64Operand(Rhs); }
  uint32_t laneIndex() const {
    return mir_->toWasmReplaceLaneSimd128()->laneIndex();
  }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmReplaceLaneSimd128()->simdOp();
  }
};

// (scalar) -> v128 effect-free operations, scalar != int64
class LWasmScalarToSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmScalarToSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmScalarToSimd128(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmScalarToSimd128()->simdOp();
  }
};

// (int64) -> v128 effect-free operations
class LWasmInt64ToSimd128 : public LInstructionHelper<1, INT64_PIECES, 0> {
 public:
  LIR_HEADER(WasmInt64ToSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmInt64ToSimd128(const LInt64Allocation& src)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(Src, src);
  }

  const LInt64Allocation src() { return getInt64Operand(Src); }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmScalarToSimd128()->simdOp();
  }
};

// (v128) -> v128 effect-free operations
// temp is FPR (if in use).
class LWasmUnarySimd128 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(WasmUnarySimd128)

  static constexpr uint32_t Src = 0;

  LWasmUnarySimd128(const LAllocation& src, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* src() { return getOperand(Src); }
  const LDefinition* temp() { return getTemp(0); }
  wasm::SimdOp simdOp() const { return mir_->toWasmUnarySimd128()->simdOp(); }
};

// (v128, imm) -> scalar effect-free operations.
// temp is FPR (if in use).
class LWasmReduceSimd128 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(WasmReduceSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmReduceSimd128(const LAllocation& src, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* src() { return getOperand(Src); }
  uint32_t imm() const { return mir_->toWasmReduceSimd128()->imm(); }
  wasm::SimdOp simdOp() const { return mir_->toWasmReduceSimd128()->simdOp(); }
};

// (v128, onTrue, onFalse) test-and-branch operations.
class LWasmReduceAndBranchSimd128 : public LControlInstructionHelper<2, 1, 0> {
  wasm::SimdOp op_;

 public:
  LIR_HEADER(WasmReduceAndBranchSimd128)

  static constexpr uint32_t Src = 0;
  static constexpr uint32_t IfTrue = 0;
  static constexpr uint32_t IfFalse = 1;

  LWasmReduceAndBranchSimd128(const LAllocation& src, wasm::SimdOp op,
                              MBasicBlock* ifTrue, MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), op_(op) {
    setOperand(Src, src);
    setSuccessor(IfTrue, ifTrue);
    setSuccessor(IfFalse, ifFalse);
  }

  const LAllocation* src() { return getOperand(Src); }
  wasm::SimdOp simdOp() const { return op_; }
  MBasicBlock* ifTrue() const { return getSuccessor(IfTrue); }
  MBasicBlock* ifFalse() const { return getSuccessor(IfFalse); }
};

// (v128, imm) -> i64 effect-free operations
class LWasmReduceSimd128ToInt64
    : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(WasmReduceSimd128ToInt64)

  static constexpr uint32_t Src = 0;

  explicit LWasmReduceSimd128ToInt64(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  uint32_t imm() const { return mir_->toWasmReduceSimd128()->imm(); }
  wasm::SimdOp simdOp() const { return mir_->toWasmReduceSimd128()->simdOp(); }
};

class LWasmLoadLaneSimd128 : public LInstructionHelper<1, 3, 1> {
 public:
  LIR_HEADER(WasmLoadLaneSimd128);

  static constexpr uint32_t Src = 2;

  explicit LWasmLoadLaneSimd128(const LAllocation& ptr, const LAllocation& src,
                                const LDefinition& temp,
                                const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, memoryBase);
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* memoryBase() { return getOperand(1); }
  const LAllocation* src() { return getOperand(Src); }
  const LDefinition* temp() { return getTemp(0); }
  MWasmLoadLaneSimd128* mir() const { return mir_->toWasmLoadLaneSimd128(); }
  uint32_t laneSize() const {
    return mir_->toWasmLoadLaneSimd128()->laneSize();
  }
  uint32_t laneIndex() const {
    return mir_->toWasmLoadLaneSimd128()->laneIndex();
  }
};

class LWasmStoreLaneSimd128 : public LInstructionHelper<1, 3, 1> {
 public:
  LIR_HEADER(WasmStoreLaneSimd128);

  static constexpr uint32_t Src = 2;

  explicit LWasmStoreLaneSimd128(const LAllocation& ptr, const LAllocation& src,
                                 const LDefinition& temp,
                                 const LAllocation& memoryBase = LAllocation())
      : LInstructionHelper(classOpcode) {
    setOperand(0, ptr);
    setOperand(1, memoryBase);
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* ptr() { return getOperand(0); }
  const LAllocation* memoryBase() { return getOperand(1); }
  const LAllocation* src() { return getOperand(Src); }
  const LDefinition* temp() { return getTemp(0); }
  MWasmStoreLaneSimd128* mir() const { return mir_->toWasmStoreLaneSimd128(); }
  uint32_t laneSize() const {
    return mir_->toWasmStoreLaneSimd128()->laneSize();
  }
  uint32_t laneIndex() const {
    return mir_->toWasmStoreLaneSimd128()->laneIndex();
  }
};

// End Wasm SIMD

}  // namespace jit
}  // namespace js

#endif /* jit_shared_LIR_shared_h */
