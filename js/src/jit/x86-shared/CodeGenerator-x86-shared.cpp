/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86-shared/CodeGenerator-x86-shared.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include "jsmath.h"

#include "jit/CodeGenerator.h"
#include "jit/JitFrames.h"
#include "jit/JitRealm.h"
#include "jit/Linker.h"
#include "jit/RangeAnalysis.h"
#include "vm/TraceLogging.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Abs;
using mozilla::BitwiseCast;
using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using mozilla::FloorLog2;
using mozilla::Maybe;
using mozilla::NegativeInfinity;
using mozilla::SpecificNaN;

using JS::GenericNaN;

namespace js {
namespace jit {

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator* gen,
                                               LIRGraph* graph,
                                               MacroAssembler* masm)
    : CodeGeneratorShared(gen, graph, masm) {}

#ifdef JS_PUNBOX64
Operand CodeGeneratorX86Shared::ToOperandOrRegister64(
    const LInt64Allocation input) {
  return ToOperand(input.value());
}
#else
Register64 CodeGeneratorX86Shared::ToOperandOrRegister64(
    const LInt64Allocation input) {
  return ToRegister64(input);
}
#endif

void OutOfLineBailout::accept(CodeGeneratorX86Shared* codegen) {
  codegen->visitOutOfLineBailout(this);
}

void CodeGeneratorX86Shared::emitBranch(Assembler::Condition cond,
                                        MBasicBlock* mirTrue,
                                        MBasicBlock* mirFalse,
                                        Assembler::NaNCond ifNaN) {
  if (ifNaN == Assembler::NaN_IsFalse) {
    jumpToBlock(mirFalse, Assembler::Parity);
  } else if (ifNaN == Assembler::NaN_IsTrue) {
    jumpToBlock(mirTrue, Assembler::Parity);
  }

  if (isNextBlock(mirFalse->lir())) {
    jumpToBlock(mirTrue, cond);
  } else {
    jumpToBlock(mirFalse, Assembler::InvertCondition(cond));
    jumpToBlock(mirTrue);
  }
}

void CodeGenerator::visitDouble(LDouble* ins) {
  const LDefinition* out = ins->getDef(0);
  masm.loadConstantDouble(ins->getDouble(), ToFloatRegister(out));
}

void CodeGenerator::visitFloat32(LFloat32* ins) {
  const LDefinition* out = ins->getDef(0);
  masm.loadConstantFloat32(ins->getFloat(), ToFloatRegister(out));
}

void CodeGenerator::visitTestIAndBranch(LTestIAndBranch* test) {
  Register input = ToRegister(test->input());
  masm.test32(input, input);
  emitBranch(Assembler::NonZero, test->ifTrue(), test->ifFalse());
}

void CodeGenerator::visitTestDAndBranch(LTestDAndBranch* test) {
  const LAllocation* opd = test->input();

  // vucomisd flags:
  //             Z  P  C
  //            ---------
  //      NaN    1  1  1
  //        >    0  0  0
  //        <    0  0  1
  //        =    1  0  0
  //
  // NaN is falsey, so comparing against 0 and then using the Z flag is
  // enough to determine which branch to take.
  ScratchDoubleScope scratch(masm);
  masm.zeroDouble(scratch);
  masm.vucomisd(scratch, ToFloatRegister(opd));
  emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
}

void CodeGenerator::visitTestFAndBranch(LTestFAndBranch* test) {
  const LAllocation* opd = test->input();
  // vucomiss flags are the same as doubles; see comment above
  {
    ScratchFloat32Scope scratch(masm);
    masm.zeroFloat32(scratch);
    masm.vucomiss(scratch, ToFloatRegister(opd));
  }
  emitBranch(Assembler::NotEqual, test->ifTrue(), test->ifFalse());
}

void CodeGenerator::visitBitAndAndBranch(LBitAndAndBranch* baab) {
  if (baab->right()->isConstant()) {
    masm.test32(ToRegister(baab->left()), Imm32(ToInt32(baab->right())));
  } else {
    masm.test32(ToRegister(baab->left()), ToRegister(baab->right()));
  }
  emitBranch(baab->cond(), baab->ifTrue(), baab->ifFalse());
}

void CodeGeneratorX86Shared::emitCompare(MCompare::CompareType type,
                                         const LAllocation* left,
                                         const LAllocation* right) {
#ifdef JS_CODEGEN_X64
  if (type == MCompare::Compare_Object || type == MCompare::Compare_Symbol) {
    masm.cmpPtr(ToRegister(left), ToOperand(right));
    return;
  }
#endif

  if (right->isConstant()) {
    masm.cmp32(ToRegister(left), Imm32(ToInt32(right)));
  } else {
    masm.cmp32(ToRegister(left), ToOperand(right));
  }
}

void CodeGenerator::visitCompare(LCompare* comp) {
  MCompare* mir = comp->mir();
  emitCompare(mir->compareType(), comp->left(), comp->right());
  masm.emitSet(JSOpToCondition(mir->compareType(), comp->jsop()),
               ToRegister(comp->output()));
}

void CodeGenerator::visitCompareAndBranch(LCompareAndBranch* comp) {
  MCompare* mir = comp->cmpMir();
  emitCompare(mir->compareType(), comp->left(), comp->right());
  Assembler::Condition cond = JSOpToCondition(mir->compareType(), comp->jsop());
  emitBranch(cond, comp->ifTrue(), comp->ifFalse());
}

void CodeGenerator::visitCompareD(LCompareD* comp) {
  FloatRegister lhs = ToFloatRegister(comp->left());
  FloatRegister rhs = ToFloatRegister(comp->right());

  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
  if (comp->mir()->operandsAreNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  masm.compareDouble(cond, lhs, rhs);
  masm.emitSet(Assembler::ConditionFromDoubleCondition(cond),
               ToRegister(comp->output()), nanCond);
}

void CodeGenerator::visitCompareF(LCompareF* comp) {
  FloatRegister lhs = ToFloatRegister(comp->left());
  FloatRegister rhs = ToFloatRegister(comp->right());

  Assembler::DoubleCondition cond = JSOpToDoubleCondition(comp->mir()->jsop());

  Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
  if (comp->mir()->operandsAreNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  masm.compareFloat(cond, lhs, rhs);
  masm.emitSet(Assembler::ConditionFromDoubleCondition(cond),
               ToRegister(comp->output()), nanCond);
}

void CodeGenerator::visitNotI(LNotI* ins) {
  masm.cmp32(ToRegister(ins->input()), Imm32(0));
  masm.emitSet(Assembler::Equal, ToRegister(ins->output()));
}

void CodeGenerator::visitNotD(LNotD* ins) {
  FloatRegister opd = ToFloatRegister(ins->input());

  // Not returns true if the input is a NaN. We don't have to worry about
  // it if we know the input is never NaN though.
  Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
  if (ins->mir()->operandIsNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  ScratchDoubleScope scratch(masm);
  masm.zeroDouble(scratch);
  masm.compareDouble(Assembler::DoubleEqualOrUnordered, opd, scratch);
  masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
}

void CodeGenerator::visitNotF(LNotF* ins) {
  FloatRegister opd = ToFloatRegister(ins->input());

  // Not returns true if the input is a NaN. We don't have to worry about
  // it if we know the input is never NaN though.
  Assembler::NaNCond nanCond = Assembler::NaN_IsTrue;
  if (ins->mir()->operandIsNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  ScratchFloat32Scope scratch(masm);
  masm.zeroFloat32(scratch);
  masm.compareFloat(Assembler::DoubleEqualOrUnordered, opd, scratch);
  masm.emitSet(Assembler::Equal, ToRegister(ins->output()), nanCond);
}

void CodeGenerator::visitCompareDAndBranch(LCompareDAndBranch* comp) {
  FloatRegister lhs = ToFloatRegister(comp->left());
  FloatRegister rhs = ToFloatRegister(comp->right());

  Assembler::DoubleCondition cond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());

  Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
  if (comp->cmpMir()->operandsAreNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  masm.compareDouble(cond, lhs, rhs);
  emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(),
             comp->ifFalse(), nanCond);
}

void CodeGenerator::visitCompareFAndBranch(LCompareFAndBranch* comp) {
  FloatRegister lhs = ToFloatRegister(comp->left());
  FloatRegister rhs = ToFloatRegister(comp->right());

  Assembler::DoubleCondition cond =
      JSOpToDoubleCondition(comp->cmpMir()->jsop());

  Assembler::NaNCond nanCond = Assembler::NaNCondFromDoubleCondition(cond);
  if (comp->cmpMir()->operandsAreNeverNaN()) {
    nanCond = Assembler::NaN_HandledByCond;
  }

  masm.compareFloat(cond, lhs, rhs);
  emitBranch(Assembler::ConditionFromDoubleCondition(cond), comp->ifTrue(),
             comp->ifFalse(), nanCond);
}

void CodeGenerator::visitWasmStackArg(LWasmStackArg* ins) {
  const MWasmStackArg* mir = ins->mir();
  Address dst(StackPointer, mir->spOffset());
  if (ins->arg()->isConstant()) {
    masm.storePtr(ImmWord(ToInt32(ins->arg())), dst);
  } else if (ins->arg()->isGeneralReg()) {
    masm.storePtr(ToRegister(ins->arg()), dst);
  } else {
    switch (mir->input()->type()) {
      case MIRType::Double:
        masm.storeDouble(ToFloatRegister(ins->arg()), dst);
        return;
      case MIRType::Float32:
        masm.storeFloat32(ToFloatRegister(ins->arg()), dst);
        return;
      default:
        break;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE(
        "unexpected mir type in WasmStackArg");
  }
}

void CodeGenerator::visitWasmStackArgI64(LWasmStackArgI64* ins) {
  const MWasmStackArg* mir = ins->mir();
  Address dst(StackPointer, mir->spOffset());
  if (IsConstant(ins->arg())) {
    masm.store64(Imm64(ToInt64(ins->arg())), dst);
  } else {
    masm.store64(ToRegister64(ins->arg()), dst);
  }
}

void CodeGenerator::visitWasmSelect(LWasmSelect* ins) {
  MIRType mirType = ins->mir()->type();

  Register cond = ToRegister(ins->condExpr());
  Operand falseExpr = ToOperand(ins->falseExpr());

  masm.test32(cond, cond);

  if (mirType == MIRType::Int32 || mirType == MIRType::RefOrNull) {
    Register out = ToRegister(ins->output());
    MOZ_ASSERT(ToRegister(ins->trueExpr()) == out,
               "true expr input is reused for output");
    if (mirType == MIRType::Int32) {
      masm.cmovz32(falseExpr, out);
    } else {
      masm.cmovzPtr(falseExpr, out);
    }
    return;
  }

  FloatRegister out = ToFloatRegister(ins->output());
  MOZ_ASSERT(ToFloatRegister(ins->trueExpr()) == out,
             "true expr input is reused for output");

  Label done;
  masm.j(Assembler::NonZero, &done);

  if (mirType == MIRType::Float32) {
    if (falseExpr.kind() == Operand::FPREG) {
      masm.moveFloat32(ToFloatRegister(ins->falseExpr()), out);
    } else {
      masm.loadFloat32(falseExpr, out);
    }
  } else if (mirType == MIRType::Double) {
    if (falseExpr.kind() == Operand::FPREG) {
      masm.moveDouble(ToFloatRegister(ins->falseExpr()), out);
    } else {
      masm.loadDouble(falseExpr, out);
    }
  } else {
    MOZ_CRASH("unhandled type in visitWasmSelect!");
  }

  masm.bind(&done);
}

void CodeGenerator::visitWasmReinterpret(LWasmReinterpret* lir) {
  MOZ_ASSERT(gen->compilingWasm());
  MWasmReinterpret* ins = lir->mir();

  MIRType to = ins->type();
#ifdef DEBUG
  MIRType from = ins->input()->type();
#endif

  switch (to) {
    case MIRType::Int32:
      MOZ_ASSERT(from == MIRType::Float32);
      masm.vmovd(ToFloatRegister(lir->input()), ToRegister(lir->output()));
      break;
    case MIRType::Float32:
      MOZ_ASSERT(from == MIRType::Int32);
      masm.vmovd(ToRegister(lir->input()), ToFloatRegister(lir->output()));
      break;
    case MIRType::Double:
    case MIRType::Int64:
      MOZ_CRASH("not handled by this LIR opcode");
    default:
      MOZ_CRASH("unexpected WasmReinterpret");
  }
}

void CodeGenerator::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins) {
  const MAsmJSLoadHeap* mir = ins->mir();
  MOZ_ASSERT(mir->access().offset() == 0);

  const LAllocation* ptr = ins->ptr();
  const LAllocation* boundsCheckLimit = ins->boundsCheckLimit();
  AnyRegister out = ToAnyRegister(ins->output());

  Scalar::Type accessType = mir->accessType();

  OutOfLineLoadTypedArrayOutOfBounds* ool = nullptr;
  if (mir->needsBoundsCheck()) {
    ool = new (alloc()) OutOfLineLoadTypedArrayOutOfBounds(out, accessType);
    addOutOfLineCode(ool, mir);

    masm.wasmBoundsCheck(Assembler::AboveOrEqual, ToRegister(ptr),
                         ToRegister(boundsCheckLimit), ool->entry());
  }

#ifdef JS_CODEGEN_X86
  const LAllocation* memoryBase = ins->memoryBase();
  Operand srcAddr = ptr->isBogus() ? Operand(ToRegister(memoryBase), 0)
                                   : Operand(ToRegister(memoryBase),
                                             ToRegister(ptr), TimesOne);
#else
  MOZ_ASSERT(!mir->hasMemoryBase());
  Operand srcAddr = ptr->isBogus()
                        ? Operand(HeapReg, 0)
                        : Operand(HeapReg, ToRegister(ptr), TimesOne);
#endif

  masm.wasmLoad(mir->access(), srcAddr, out);

  if (ool) {
    masm.bind(ool->rejoin());
  }
}

void CodeGeneratorX86Shared::visitOutOfLineLoadTypedArrayOutOfBounds(
    OutOfLineLoadTypedArrayOutOfBounds* ool) {
  switch (ool->viewType()) {
    case Scalar::Int64:
    case Scalar::BigInt64:
    case Scalar::BigUint64:
    case Scalar::V128:
    case Scalar::MaxTypedArrayViewType:
      MOZ_CRASH("unexpected array type");
    case Scalar::Float32:
      masm.loadConstantFloat32(float(GenericNaN()), ool->dest().fpu());
      break;
    case Scalar::Float64:
      masm.loadConstantDouble(GenericNaN(), ool->dest().fpu());
      break;
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
    case Scalar::Uint32:
    case Scalar::Uint8Clamped:
      Register destReg = ool->dest().gpr();
      masm.mov(ImmWord(0), destReg);
      break;
  }
  masm.jmp(ool->rejoin());
}

void CodeGenerator::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins) {
  const MAsmJSStoreHeap* mir = ins->mir();
  MOZ_ASSERT(mir->offset() == 0);

  const LAllocation* ptr = ins->ptr();
  const LAllocation* value = ins->value();
  const LAllocation* boundsCheckLimit = ins->boundsCheckLimit();

  Scalar::Type accessType = mir->accessType();
  canonicalizeIfDeterministic(accessType, value);

  Label rejoin;
  if (mir->needsBoundsCheck()) {
    masm.wasmBoundsCheck(Assembler::AboveOrEqual, ToRegister(ptr),
                         ToRegister(boundsCheckLimit), &rejoin);
  }

#ifdef JS_CODEGEN_X86
  const LAllocation* memoryBase = ins->memoryBase();
  Operand dstAddr = ptr->isBogus() ? Operand(ToRegister(memoryBase), 0)
                                   : Operand(ToRegister(memoryBase),
                                             ToRegister(ptr), TimesOne);
#else
  MOZ_ASSERT(!mir->hasMemoryBase());
  Operand dstAddr = ptr->isBogus()
                        ? Operand(HeapReg, 0)
                        : Operand(HeapReg, ToRegister(ptr), TimesOne);
#endif

  masm.wasmStore(mir->access(), ToAnyRegister(value), dstAddr);

  if (rejoin.used()) {
    masm.bind(&rejoin);
  }
}

void CodeGenerator::visitWasmAddOffset(LWasmAddOffset* lir) {
  MWasmAddOffset* mir = lir->mir();
  Register base = ToRegister(lir->base());
  Register out = ToRegister(lir->output());

  if (base != out) {
    masm.move32(base, out);
  }
  masm.add32(Imm32(mir->offset()), out);

  Label ok;
  masm.j(Assembler::CarryClear, &ok);
  masm.wasmTrap(wasm::Trap::OutOfBounds, mir->bytecodeOffset());
  masm.bind(&ok);
}

void CodeGenerator::visitWasmTruncateToInt32(LWasmTruncateToInt32* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  MWasmTruncateToInt32* mir = lir->mir();
  MIRType inputType = mir->input()->type();

  MOZ_ASSERT(inputType == MIRType::Double || inputType == MIRType::Float32);

  auto* ool = new (alloc()) OutOfLineWasmTruncateCheck(mir, input, output);
  addOutOfLineCode(ool, mir);

  Label* oolEntry = ool->entry();
  if (mir->isUnsigned()) {
    if (inputType == MIRType::Double) {
      masm.wasmTruncateDoubleToUInt32(input, output, mir->isSaturating(),
                                      oolEntry);
    } else if (inputType == MIRType::Float32) {
      masm.wasmTruncateFloat32ToUInt32(input, output, mir->isSaturating(),
                                       oolEntry);
    } else {
      MOZ_CRASH("unexpected type");
    }
    if (mir->isSaturating()) {
      masm.bind(ool->rejoin());
    }
    return;
  }

  if (inputType == MIRType::Double) {
    masm.wasmTruncateDoubleToInt32(input, output, mir->isSaturating(),
                                   oolEntry);
  } else if (inputType == MIRType::Float32) {
    masm.wasmTruncateFloat32ToInt32(input, output, mir->isSaturating(),
                                    oolEntry);
  } else {
    MOZ_CRASH("unexpected type");
  }

  masm.bind(ool->rejoin());
}

bool CodeGeneratorX86Shared::generateOutOfLineCode() {
  if (!CodeGeneratorShared::generateOutOfLineCode()) {
    return false;
  }

  if (deoptLabel_.used()) {
    // All non-table-based bailouts will go here.
    masm.bind(&deoptLabel_);

    // Push the frame size, so the handler can recover the IonScript.
    masm.push(Imm32(frameSize()));

    TrampolinePtr handler = gen->jitRuntime()->getGenericBailoutHandler();
    masm.jump(handler);
  }

  return !masm.oom();
}

class BailoutJump {
  Assembler::Condition cond_;

 public:
  explicit BailoutJump(Assembler::Condition cond) : cond_(cond) {}
#ifdef JS_CODEGEN_X86
  void operator()(MacroAssembler& masm, uint8_t* code) const {
    masm.j(cond_, ImmPtr(code), RelocationKind::HARDCODED);
  }
#endif
  void operator()(MacroAssembler& masm, Label* label) const {
    masm.j(cond_, label);
  }
};

class BailoutLabel {
  Label* label_;

 public:
  explicit BailoutLabel(Label* label) : label_(label) {}
#ifdef JS_CODEGEN_X86
  void operator()(MacroAssembler& masm, uint8_t* code) const {
    masm.retarget(label_, ImmPtr(code), RelocationKind::HARDCODED);
  }
#endif
  void operator()(MacroAssembler& masm, Label* label) const {
    masm.retarget(label_, label);
  }
};

template <typename T>
void CodeGeneratorX86Shared::bailout(const T& binder, LSnapshot* snapshot) {
  encode(snapshot);

  // Though the assembler doesn't track all frame pushes, at least make sure
  // the known value makes sense. We can't use bailout tables if the stack
  // isn't properly aligned to the static frame size.
  MOZ_ASSERT_IF(frameClass_ != FrameSizeClass::None() && deoptTable_,
                frameClass_.frameSize() == masm.framePushed());

#ifdef JS_CODEGEN_X86
  // On x64, bailout tables are pointless, because 16 extra bytes are
  // reserved per external jump, whereas it takes only 10 bytes to encode a
  // a non-table based bailout.
  if (assignBailoutId(snapshot)) {
    binder(masm, deoptTable_->value +
                     snapshot->bailoutId() * BAILOUT_TABLE_ENTRY_SIZE);
    return;
  }
#endif

  // We could not use a jump table, either because all bailout IDs were
  // reserved, or a jump table is not optimal for this frame size or
  // platform. Whatever, we will generate a lazy bailout.
  //
  // All bailout code is associated with the bytecodeSite of the block we are
  // bailing out from.
  InlineScriptTree* tree = snapshot->mir()->block()->trackedTree();
  OutOfLineBailout* ool = new (alloc()) OutOfLineBailout(snapshot);
  addOutOfLineCode(ool,
                   new (alloc()) BytecodeSite(tree, tree->script()->code()));

  binder(masm, ool->entry());
}

void CodeGeneratorX86Shared::bailoutIf(Assembler::Condition condition,
                                       LSnapshot* snapshot) {
  bailout(BailoutJump(condition), snapshot);
}

void CodeGeneratorX86Shared::bailoutIf(Assembler::DoubleCondition condition,
                                       LSnapshot* snapshot) {
  MOZ_ASSERT(Assembler::NaNCondFromDoubleCondition(condition) ==
             Assembler::NaN_HandledByCond);
  bailoutIf(Assembler::ConditionFromDoubleCondition(condition), snapshot);
}

void CodeGeneratorX86Shared::bailoutFrom(Label* label, LSnapshot* snapshot) {
  MOZ_ASSERT_IF(!masm.oom(), label->used() && !label->bound());
  bailout(BailoutLabel(label), snapshot);
}

void CodeGeneratorX86Shared::bailout(LSnapshot* snapshot) {
  Label label;
  masm.jump(&label);
  bailoutFrom(&label, snapshot);
}

void CodeGeneratorX86Shared::visitOutOfLineBailout(OutOfLineBailout* ool) {
  masm.push(Imm32(ool->snapshot()->snapshotOffset()));
  masm.jmp(&deoptLabel_);
}

void CodeGenerator::visitMinMaxD(LMinMaxD* ins) {
  FloatRegister first = ToFloatRegister(ins->first());
  FloatRegister second = ToFloatRegister(ins->second());
#ifdef DEBUG
  FloatRegister output = ToFloatRegister(ins->output());
  MOZ_ASSERT(first == output);
#endif

  bool handleNaN = !ins->mir()->range() || ins->mir()->range()->canBeNaN();

  if (ins->mir()->isMax()) {
    masm.maxDouble(second, first, handleNaN);
  } else {
    masm.minDouble(second, first, handleNaN);
  }
}

void CodeGenerator::visitMinMaxF(LMinMaxF* ins) {
  FloatRegister first = ToFloatRegister(ins->first());
  FloatRegister second = ToFloatRegister(ins->second());
#ifdef DEBUG
  FloatRegister output = ToFloatRegister(ins->output());
  MOZ_ASSERT(first == output);
#endif

  bool handleNaN = !ins->mir()->range() || ins->mir()->range()->canBeNaN();

  if (ins->mir()->isMax()) {
    masm.maxFloat32(second, first, handleNaN);
  } else {
    masm.minFloat32(second, first, handleNaN);
  }
}

void CodeGenerator::visitAbsD(LAbsD* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  MOZ_ASSERT(input == ToFloatRegister(ins->output()));
  // Load a value which is all ones except for the sign bit.
  ScratchDoubleScope scratch(masm);
  masm.loadConstantDouble(
      SpecificNaN<double>(0, FloatingPoint<double>::kSignificandBits), scratch);
  masm.vandpd(scratch, input, input);
}

void CodeGenerator::visitAbsF(LAbsF* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  MOZ_ASSERT(input == ToFloatRegister(ins->output()));
  // Same trick as visitAbsD above.
  ScratchFloat32Scope scratch(masm);
  masm.loadConstantFloat32(
      SpecificNaN<float>(0, FloatingPoint<float>::kSignificandBits), scratch);
  masm.vandps(scratch, input, input);
}

void CodeGenerator::visitClzI(LClzI* ins) {
  Register input = ToRegister(ins->input());
  Register output = ToRegister(ins->output());
  bool knownNotZero = ins->mir()->operandIsNeverZero();

  masm.clz32(input, output, knownNotZero);
}

void CodeGenerator::visitCtzI(LCtzI* ins) {
  Register input = ToRegister(ins->input());
  Register output = ToRegister(ins->output());
  bool knownNotZero = ins->mir()->operandIsNeverZero();

  masm.ctz32(input, output, knownNotZero);
}

void CodeGenerator::visitPopcntI(LPopcntI* ins) {
  Register input = ToRegister(ins->input());
  Register output = ToRegister(ins->output());
  Register temp =
      ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());

  masm.popcnt32(input, output, temp);
}

void CodeGenerator::visitSqrtD(LSqrtD* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  FloatRegister output = ToFloatRegister(ins->output());
  masm.vsqrtsd(input, output, output);
}

void CodeGenerator::visitSqrtF(LSqrtF* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  FloatRegister output = ToFloatRegister(ins->output());
  masm.vsqrtss(input, output, output);
}

void CodeGenerator::visitPowHalfD(LPowHalfD* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  FloatRegister output = ToFloatRegister(ins->output());

  ScratchDoubleScope scratch(masm);

  Label done, sqrt;

  if (!ins->mir()->operandIsNeverNegativeInfinity()) {
    // Branch if not -Infinity.
    masm.loadConstantDouble(NegativeInfinity<double>(), scratch);

    Assembler::DoubleCondition cond = Assembler::DoubleNotEqualOrUnordered;
    if (ins->mir()->operandIsNeverNaN()) {
      cond = Assembler::DoubleNotEqual;
    }
    masm.branchDouble(cond, input, scratch, &sqrt);

    // Math.pow(-Infinity, 0.5) == Infinity.
    masm.zeroDouble(output);
    masm.subDouble(scratch, output);
    masm.jump(&done);

    masm.bind(&sqrt);
  }

  if (!ins->mir()->operandIsNeverNegativeZero()) {
    // Math.pow(-0, 0.5) == 0 == Math.pow(0, 0.5).
    // Adding 0 converts any -0 to 0.
    masm.zeroDouble(scratch);
    masm.addDouble(input, scratch);
    masm.vsqrtsd(scratch, output, output);
  } else {
    masm.vsqrtsd(input, output, output);
  }

  masm.bind(&done);
}

class OutOfLineUndoALUOperation
    : public OutOfLineCodeBase<CodeGeneratorX86Shared> {
  LInstruction* ins_;

 public:
  explicit OutOfLineUndoALUOperation(LInstruction* ins) : ins_(ins) {}

  virtual void accept(CodeGeneratorX86Shared* codegen) override {
    codegen->visitOutOfLineUndoALUOperation(this);
  }
  LInstruction* ins() const { return ins_; }
};

void CodeGenerator::visitAddI(LAddI* ins) {
  if (ins->rhs()->isConstant()) {
    masm.addl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
  } else {
    masm.addl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));
  }

  if (ins->snapshot()) {
    if (ins->recoversInput()) {
      OutOfLineUndoALUOperation* ool =
          new (alloc()) OutOfLineUndoALUOperation(ins);
      addOutOfLineCode(ool, ins->mir());
      masm.j(Assembler::Overflow, ool->entry());
    } else {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }
  }
}

void CodeGenerator::visitAddI64(LAddI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LAddI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LAddI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (IsConstant(rhs)) {
    masm.add64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
    return;
  }

  masm.add64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
}

void CodeGenerator::visitSubI(LSubI* ins) {
  if (ins->rhs()->isConstant()) {
    masm.subl(Imm32(ToInt32(ins->rhs())), ToOperand(ins->lhs()));
  } else {
    masm.subl(ToOperand(ins->rhs()), ToRegister(ins->lhs()));
  }

  if (ins->snapshot()) {
    if (ins->recoversInput()) {
      OutOfLineUndoALUOperation* ool =
          new (alloc()) OutOfLineUndoALUOperation(ins);
      addOutOfLineCode(ool, ins->mir());
      masm.j(Assembler::Overflow, ool->entry());
    } else {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }
  }
}

void CodeGenerator::visitSubI64(LSubI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LSubI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LSubI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (IsConstant(rhs)) {
    masm.sub64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
    return;
  }

  masm.sub64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
}

void CodeGeneratorX86Shared::visitOutOfLineUndoALUOperation(
    OutOfLineUndoALUOperation* ool) {
  LInstruction* ins = ool->ins();
  Register reg = ToRegister(ins->getDef(0));

  DebugOnly<LAllocation*> lhs = ins->getOperand(0);
  LAllocation* rhs = ins->getOperand(1);

  MOZ_ASSERT(reg == ToRegister(lhs));
  MOZ_ASSERT_IF(rhs->isGeneralReg(), reg != ToRegister(rhs));

  // Undo the effect of the ALU operation, which was performed on the output
  // register and overflowed. Writing to the output register clobbered an
  // input reg, and the original value of the input needs to be recovered
  // to satisfy the constraint imposed by any RECOVERED_INPUT operands to
  // the bailout snapshot.

  if (rhs->isConstant()) {
    Imm32 constant(ToInt32(rhs));
    if (ins->isAddI()) {
      masm.subl(constant, reg);
    } else {
      masm.addl(constant, reg);
    }
  } else {
    if (ins->isAddI()) {
      masm.subl(ToOperand(rhs), reg);
    } else {
      masm.addl(ToOperand(rhs), reg);
    }
  }

  bailout(ool->ins()->snapshot());
}

class MulNegativeZeroCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared> {
  LMulI* ins_;

 public:
  explicit MulNegativeZeroCheck(LMulI* ins) : ins_(ins) {}

  virtual void accept(CodeGeneratorX86Shared* codegen) override {
    codegen->visitMulNegativeZeroCheck(this);
  }
  LMulI* ins() const { return ins_; }
};

void CodeGenerator::visitMulI(LMulI* ins) {
  const LAllocation* lhs = ins->lhs();
  const LAllocation* rhs = ins->rhs();
  MMul* mul = ins->mir();
  MOZ_ASSERT_IF(mul->mode() == MMul::Integer,
                !mul->canBeNegativeZero() && !mul->canOverflow());

  if (rhs->isConstant()) {
    // Bailout on -0.0
    int32_t constant = ToInt32(rhs);
    if (mul->canBeNegativeZero() && constant <= 0) {
      Assembler::Condition bailoutCond =
          (constant == 0) ? Assembler::Signed : Assembler::Equal;
      masm.test32(ToRegister(lhs), ToRegister(lhs));
      bailoutIf(bailoutCond, ins->snapshot());
    }

    switch (constant) {
      case -1:
        masm.negl(ToOperand(lhs));
        break;
      case 0:
        masm.xorl(ToOperand(lhs), ToRegister(lhs));
        return;  // escape overflow check;
      case 1:
        // nop
        return;  // escape overflow check;
      case 2:
        masm.addl(ToOperand(lhs), ToRegister(lhs));
        break;
      default:
        if (!mul->canOverflow() && constant > 0) {
          // Use shift if cannot overflow and constant is power of 2
          int32_t shift = FloorLog2(constant);
          if ((1 << shift) == constant) {
            masm.shll(Imm32(shift), ToRegister(lhs));
            return;
          }
        }
        masm.imull(Imm32(ToInt32(rhs)), ToRegister(lhs));
    }

    // Bailout on overflow
    if (mul->canOverflow()) {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }
  } else {
    masm.imull(ToOperand(rhs), ToRegister(lhs));

    // Bailout on overflow
    if (mul->canOverflow()) {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    }

    if (mul->canBeNegativeZero()) {
      // Jump to an OOL path if the result is 0.
      MulNegativeZeroCheck* ool = new (alloc()) MulNegativeZeroCheck(ins);
      addOutOfLineCode(ool, mul);

      masm.test32(ToRegister(lhs), ToRegister(lhs));
      masm.j(Assembler::Zero, ool->entry());
      masm.bind(ool->rejoin());
    }
  }
}

void CodeGenerator::visitMulI64(LMulI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LMulI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LMulI64::Rhs);

  MOZ_ASSERT(ToRegister64(lhs) == ToOutRegister64(lir));

  if (IsConstant(rhs)) {
    int64_t constant = ToInt64(rhs);
    switch (constant) {
      case -1:
        masm.neg64(ToRegister64(lhs));
        return;
      case 0:
        masm.xor64(ToRegister64(lhs), ToRegister64(lhs));
        return;
      case 1:
        // nop
        return;
      case 2:
        masm.add64(ToRegister64(lhs), ToRegister64(lhs));
        return;
      default:
        if (constant > 0) {
          // Use shift if constant is power of 2.
          int32_t shift = mozilla::FloorLog2(constant);
          if (int64_t(1) << shift == constant) {
            masm.lshift64(Imm32(shift), ToRegister64(lhs));
            return;
          }
        }
        Register temp = ToTempRegisterOrInvalid(lir->temp());
        masm.mul64(Imm64(constant), ToRegister64(lhs), temp);
    }
  } else {
    Register temp = ToTempRegisterOrInvalid(lir->temp());
    masm.mul64(ToOperandOrRegister64(rhs), ToRegister64(lhs), temp);
  }
}

class ReturnZero : public OutOfLineCodeBase<CodeGeneratorX86Shared> {
  Register reg_;

 public:
  explicit ReturnZero(Register reg) : reg_(reg) {}

  virtual void accept(CodeGeneratorX86Shared* codegen) override {
    codegen->visitReturnZero(this);
  }
  Register reg() const { return reg_; }
};

void CodeGeneratorX86Shared::visitReturnZero(ReturnZero* ool) {
  masm.mov(ImmWord(0), ool->reg());
  masm.jmp(ool->rejoin());
}

void CodeGenerator::visitUDivOrMod(LUDivOrMod* ins) {
  Register lhs = ToRegister(ins->lhs());
  Register rhs = ToRegister(ins->rhs());
  Register output = ToRegister(ins->output());

  MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
  MOZ_ASSERT(rhs != edx);
  MOZ_ASSERT_IF(output == eax, ToRegister(ins->remainder()) == edx);

  ReturnZero* ool = nullptr;

  // Put the lhs in eax.
  if (lhs != eax) {
    masm.mov(lhs, eax);
  }

  // Prevent divide by zero.
  if (ins->canBeDivideByZero()) {
    masm.test32(rhs, rhs);
    if (ins->mir()->isTruncated()) {
      if (ins->trapOnError()) {
        Label nonZero;
        masm.j(Assembler::NonZero, &nonZero);
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, ins->bytecodeOffset());
        masm.bind(&nonZero);
      } else {
        ool = new (alloc()) ReturnZero(output);
        masm.j(Assembler::Zero, ool->entry());
      }
    } else {
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  // Zero extend the lhs into edx to make (edx:eax), since udiv is 64-bit.
  masm.mov(ImmWord(0), edx);
  masm.udiv(rhs);

  // If the remainder is > 0, bailout since this must be a double.
  if (ins->mir()->isDiv() && !ins->mir()->toDiv()->canTruncateRemainder()) {
    Register remainder = ToRegister(ins->remainder());
    masm.test32(remainder, remainder);
    bailoutIf(Assembler::NonZero, ins->snapshot());
  }

  // Unsigned div or mod can return a value that's not a signed int32.
  // If our users aren't expecting that, bail.
  if (!ins->mir()->isTruncated()) {
    masm.test32(output, output);
    bailoutIf(Assembler::Signed, ins->snapshot());
  }

  if (ool) {
    addOutOfLineCode(ool, ins->mir());
    masm.bind(ool->rejoin());
  }
}

void CodeGenerator::visitUDivOrModConstant(LUDivOrModConstant* ins) {
  Register lhs = ToRegister(ins->numerator());
  Register output = ToRegister(ins->output());
  uint32_t d = ins->denominator();

  // This emits the division answer into edx or the modulus answer into eax.
  MOZ_ASSERT(output == eax || output == edx);
  MOZ_ASSERT(lhs != eax && lhs != edx);
  bool isDiv = (output == edx);

  if (d == 0) {
    if (ins->mir()->isTruncated()) {
      if (ins->trapOnError()) {
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, ins->bytecodeOffset());
      } else {
        masm.xorl(output, output);
      }
    } else {
      bailout(ins->snapshot());
    }
    return;
  }

  // The denominator isn't a power of 2 (see LDivPowTwoI and LModPowTwoI).
  MOZ_ASSERT((d & (d - 1)) != 0);

  ReciprocalMulConstants rmc = computeDivisionConstants(d, /* maxLog = */ 32);

  // We first compute (M * n) >> 32, where M = rmc.multiplier.
  masm.movl(Imm32(rmc.multiplier), eax);
  masm.umull(lhs);
  if (rmc.multiplier > UINT32_MAX) {
    // M >= 2^32 and shift == 0 is impossible, as d >= 2 implies that
    // ((M * n) >> (32 + shift)) >= n > floor(n/d) whenever n >= d,
    // contradicting the proof of correctness in computeDivisionConstants.
    MOZ_ASSERT(rmc.shiftAmount > 0);
    MOZ_ASSERT(rmc.multiplier < (int64_t(1) << 33));

    // We actually computed edx = ((uint32_t(M) * n) >> 32) instead. Since
    // (M * n) >> (32 + shift) is the same as (edx + n) >> shift, we can
    // correct for the overflow. This case is a bit trickier than the signed
    // case, though, as the (edx + n) addition itself can overflow; however,
    // note that (edx + n) >> shift == (((n - edx) >> 1) + edx) >> (shift - 1),
    // which is overflow-free. See Hacker's Delight, section 10-8 for details.

    // Compute (n - edx) >> 1 into eax.
    masm.movl(lhs, eax);
    masm.subl(edx, eax);
    masm.shrl(Imm32(1), eax);

    // Finish the computation.
    masm.addl(eax, edx);
    masm.shrl(Imm32(rmc.shiftAmount - 1), edx);
  } else {
    masm.shrl(Imm32(rmc.shiftAmount), edx);
  }

  // We now have the truncated division value in edx. If we're
  // computing a modulus or checking whether the division resulted
  // in an integer, we need to multiply the obtained value by d and
  // finish the computation/check.
  if (!isDiv) {
    masm.imull(Imm32(d), edx, edx);
    masm.movl(lhs, eax);
    masm.subl(edx, eax);

    // The final result of the modulus op, just computed above by the
    // sub instruction, can be a number in the range [2^31, 2^32). If
    // this is the case and the modulus is not truncated, we must bail
    // out.
    if (!ins->mir()->isTruncated()) {
      bailoutIf(Assembler::Signed, ins->snapshot());
    }
  } else if (!ins->mir()->isTruncated()) {
    masm.imull(Imm32(d), edx, eax);
    masm.cmpl(lhs, eax);
    bailoutIf(Assembler::NotEqual, ins->snapshot());
  }
}

void CodeGeneratorX86Shared::visitMulNegativeZeroCheck(
    MulNegativeZeroCheck* ool) {
  LMulI* ins = ool->ins();
  Register result = ToRegister(ins->output());
  Operand lhsCopy = ToOperand(ins->lhsCopy());
  Operand rhs = ToOperand(ins->rhs());
  MOZ_ASSERT_IF(lhsCopy.kind() == Operand::REG, lhsCopy.reg() != result.code());

  // Result is -0 if lhs or rhs is negative.
  masm.movl(lhsCopy, result);
  masm.orl(rhs, result);
  bailoutIf(Assembler::Signed, ins->snapshot());

  masm.mov(ImmWord(0), result);
  masm.jmp(ool->rejoin());
}

void CodeGenerator::visitDivPowTwoI(LDivPowTwoI* ins) {
  Register lhs = ToRegister(ins->numerator());
  DebugOnly<Register> output = ToRegister(ins->output());

  int32_t shift = ins->shift();
  bool negativeDivisor = ins->negativeDivisor();
  MDiv* mir = ins->mir();

  // We use defineReuseInput so these should always be the same, which is
  // convenient since all of our instructions here are two-address.
  MOZ_ASSERT(lhs == output);

  if (!mir->isTruncated() && negativeDivisor) {
    // 0 divided by a negative number must return a double.
    masm.test32(lhs, lhs);
    bailoutIf(Assembler::Zero, ins->snapshot());
  }

  if (shift) {
    if (!mir->isTruncated()) {
      // If the remainder is != 0, bailout since this must be a double.
      masm.test32(lhs, Imm32(UINT32_MAX >> (32 - shift)));
      bailoutIf(Assembler::NonZero, ins->snapshot());
    }

    if (mir->isUnsigned()) {
      masm.shrl(Imm32(shift), lhs);
    } else {
      // Adjust the value so that shifting produces a correctly
      // rounded result when the numerator is negative. See 10-1
      // "Signed Division by a Known Power of 2" in Henry
      // S. Warren, Jr.'s Hacker's Delight.
      if (mir->canBeNegativeDividend() && mir->isTruncated()) {
        // Note: There is no need to execute this code, which handles how to
        // round the signed integer division towards 0, if we previously bailed
        // due to a non-zero remainder.
        Register lhsCopy = ToRegister(ins->numeratorCopy());
        MOZ_ASSERT(lhsCopy != lhs);
        if (shift > 1) {
          // Copy the sign bit of the numerator. (= (2^32 - 1) or 0)
          masm.sarl(Imm32(31), lhs);
        }
        // Divide by 2^(32 - shift)
        // i.e. (= (2^32 - 1) / 2^(32 - shift) or 0)
        // i.e. (= (2^shift - 1) or 0)
        masm.shrl(Imm32(32 - shift), lhs);
        // If signed, make any 1 bit below the shifted bits to bubble up, such
        // that once shifted the value would be rounded towards 0.
        masm.addl(lhsCopy, lhs);
      }
      masm.sarl(Imm32(shift), lhs);

      if (negativeDivisor) {
        masm.negl(lhs);
      }
    }
    return;
  }

  if (negativeDivisor) {
    // INT32_MIN / -1 overflows.
    masm.negl(lhs);
    if (!mir->isTruncated()) {
      bailoutIf(Assembler::Overflow, ins->snapshot());
    } else if (mir->trapOnError()) {
      Label ok;
      masm.j(Assembler::NoOverflow, &ok);
      masm.wasmTrap(wasm::Trap::IntegerOverflow, mir->bytecodeOffset());
      masm.bind(&ok);
    }
  } else if (mir->isUnsigned() && !mir->isTruncated()) {
    // Unsigned division by 1 can overflow if output is not
    // truncated.
    masm.test32(lhs, lhs);
    bailoutIf(Assembler::Signed, ins->snapshot());
  }
}

void CodeGenerator::visitDivOrModConstantI(LDivOrModConstantI* ins) {
  Register lhs = ToRegister(ins->numerator());
  Register output = ToRegister(ins->output());
  int32_t d = ins->denominator();

  // This emits the division answer into edx or the modulus answer into eax.
  MOZ_ASSERT(output == eax || output == edx);
  MOZ_ASSERT(lhs != eax && lhs != edx);
  bool isDiv = (output == edx);

  // The absolute value of the denominator isn't a power of 2 (see LDivPowTwoI
  // and LModPowTwoI).
  MOZ_ASSERT((Abs(d) & (Abs(d) - 1)) != 0);

  // We will first divide by Abs(d), and negate the answer if d is negative.
  // If desired, this can be avoided by generalizing computeDivisionConstants.
  ReciprocalMulConstants rmc =
      computeDivisionConstants(Abs(d), /* maxLog = */ 31);

  // We first compute (M * n) >> 32, where M = rmc.multiplier.
  masm.movl(Imm32(rmc.multiplier), eax);
  masm.imull(lhs);
  if (rmc.multiplier > INT32_MAX) {
    MOZ_ASSERT(rmc.multiplier < (int64_t(1) << 32));

    // We actually computed edx = ((int32_t(M) * n) >> 32) instead. Since
    // (M * n) >> 32 is the same as (edx + n), we can correct for the overflow.
    // (edx + n) can't overflow, as n and edx have opposite signs because
    // int32_t(M) is negative.
    masm.addl(lhs, edx);
  }
  // (M * n) >> (32 + shift) is the truncated division answer if n is
  // non-negative, as proved in the comments of computeDivisionConstants. We
  // must add 1 later if n is negative to get the right answer in all cases.
  masm.sarl(Imm32(rmc.shiftAmount), edx);

  // We'll subtract -1 instead of adding 1, because (n < 0 ? -1 : 0) can be
  // computed with just a sign-extending shift of 31 bits.
  if (ins->canBeNegativeDividend()) {
    masm.movl(lhs, eax);
    masm.sarl(Imm32(31), eax);
    masm.subl(eax, edx);
  }

  // After this, edx contains the correct truncated division result.
  if (d < 0) {
    masm.negl(edx);
  }

  if (!isDiv) {
    masm.imull(Imm32(-d), edx, eax);
    masm.addl(lhs, eax);
  }

  if (!ins->mir()->isTruncated()) {
    if (isDiv) {
      // This is a division op. Multiply the obtained value by d to check if
      // the correct answer is an integer. This cannot overflow, since |d| > 1.
      masm.imull(Imm32(d), edx, eax);
      masm.cmp32(lhs, eax);
      bailoutIf(Assembler::NotEqual, ins->snapshot());

      // If lhs is zero and the divisor is negative, the answer should have
      // been -0.
      if (d < 0) {
        masm.test32(lhs, lhs);
        bailoutIf(Assembler::Zero, ins->snapshot());
      }
    } else if (ins->canBeNegativeDividend()) {
      // This is a mod op. If the computed value is zero and lhs
      // is negative, the answer should have been -0.
      Label done;

      masm.cmp32(lhs, Imm32(0));
      masm.j(Assembler::GreaterThanOrEqual, &done);

      masm.test32(eax, eax);
      bailoutIf(Assembler::Zero, ins->snapshot());

      masm.bind(&done);
    }
  }
}

void CodeGenerator::visitDivI(LDivI* ins) {
  Register remainder = ToRegister(ins->remainder());
  Register lhs = ToRegister(ins->lhs());
  Register rhs = ToRegister(ins->rhs());
  Register output = ToRegister(ins->output());

  MDiv* mir = ins->mir();

  MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
  MOZ_ASSERT(rhs != edx);
  MOZ_ASSERT(remainder == edx);
  MOZ_ASSERT(output == eax);

  Label done;
  ReturnZero* ool = nullptr;

  // Put the lhs in eax, for either the negative overflow case or the regular
  // divide case.
  if (lhs != eax) {
    masm.mov(lhs, eax);
  }

  // Handle divide by zero.
  if (mir->canBeDivideByZero()) {
    masm.test32(rhs, rhs);
    if (mir->trapOnError()) {
      Label nonZero;
      masm.j(Assembler::NonZero, &nonZero);
      masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
      masm.bind(&nonZero);
    } else if (mir->canTruncateInfinities()) {
      // Truncated division by zero is zero (Infinity|0 == 0)
      if (!ool) {
        ool = new (alloc()) ReturnZero(output);
      }
      masm.j(Assembler::Zero, ool->entry());
    } else {
      MOZ_ASSERT(mir->fallible());
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  // Handle an integer overflow exception from -2147483648 / -1.
  if (mir->canBeNegativeOverflow()) {
    Label notOverflow;
    masm.cmp32(lhs, Imm32(INT32_MIN));
    masm.j(Assembler::NotEqual, &notOverflow);
    masm.cmp32(rhs, Imm32(-1));
    if (mir->trapOnError()) {
      masm.j(Assembler::NotEqual, &notOverflow);
      masm.wasmTrap(wasm::Trap::IntegerOverflow, mir->bytecodeOffset());
    } else if (mir->canTruncateOverflow()) {
      // (-INT32_MIN)|0 == INT32_MIN and INT32_MIN is already in the
      // output register (lhs == eax).
      masm.j(Assembler::Equal, &done);
    } else {
      MOZ_ASSERT(mir->fallible());
      bailoutIf(Assembler::Equal, ins->snapshot());
    }
    masm.bind(&notOverflow);
  }

  // Handle negative 0.
  if (!mir->canTruncateNegativeZero() && mir->canBeNegativeZero()) {
    Label nonzero;
    masm.test32(lhs, lhs);
    masm.j(Assembler::NonZero, &nonzero);
    masm.cmp32(rhs, Imm32(0));
    bailoutIf(Assembler::LessThan, ins->snapshot());
    masm.bind(&nonzero);
  }

  // Sign extend the lhs into edx to make (edx:eax), since idiv is 64-bit.
  if (lhs != eax) {
    masm.mov(lhs, eax);
  }
  masm.cdq();
  masm.idiv(rhs);

  if (!mir->canTruncateRemainder()) {
    // If the remainder is > 0, bailout since this must be a double.
    masm.test32(remainder, remainder);
    bailoutIf(Assembler::NonZero, ins->snapshot());
  }

  masm.bind(&done);

  if (ool) {
    addOutOfLineCode(ool, mir);
    masm.bind(ool->rejoin());
  }
}

void CodeGenerator::visitModPowTwoI(LModPowTwoI* ins) {
  Register lhs = ToRegister(ins->getOperand(0));
  int32_t shift = ins->shift();

  Label negative;

  if (!ins->mir()->isUnsigned() && ins->mir()->canBeNegativeDividend()) {
    // Switch based on sign of the lhs.
    // Positive numbers are just a bitmask
    masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
  }

  masm.andl(Imm32((uint32_t(1) << shift) - 1), lhs);

  if (!ins->mir()->isUnsigned() && ins->mir()->canBeNegativeDividend()) {
    Label done;
    masm.jump(&done);

    // Negative numbers need a negate, bitmask, negate
    masm.bind(&negative);

    // Unlike in the visitModI case, we are not computing the mod by means of a
    // division. Therefore, the divisor = -1 case isn't problematic (the andl
    // always returns 0, which is what we expect).
    //
    // The negl instruction overflows if lhs == INT32_MIN, but this is also not
    // a problem: shift is at most 31, and so the andl also always returns 0.
    masm.negl(lhs);
    masm.andl(Imm32((uint32_t(1) << shift) - 1), lhs);
    masm.negl(lhs);

    // Since a%b has the same sign as b, and a is negative in this branch,
    // an answer of 0 means the correct result is actually -0. Bail out.
    if (!ins->mir()->isTruncated()) {
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
    masm.bind(&done);
  }
}

class ModOverflowCheck : public OutOfLineCodeBase<CodeGeneratorX86Shared> {
  Label done_;
  LModI* ins_;
  Register rhs_;

 public:
  explicit ModOverflowCheck(LModI* ins, Register rhs) : ins_(ins), rhs_(rhs) {}

  virtual void accept(CodeGeneratorX86Shared* codegen) override {
    codegen->visitModOverflowCheck(this);
  }
  Label* done() { return &done_; }
  LModI* ins() const { return ins_; }
  Register rhs() const { return rhs_; }
};

void CodeGeneratorX86Shared::visitModOverflowCheck(ModOverflowCheck* ool) {
  masm.cmp32(ool->rhs(), Imm32(-1));
  if (ool->ins()->mir()->isTruncated()) {
    masm.j(Assembler::NotEqual, ool->rejoin());
    masm.mov(ImmWord(0), edx);
    masm.jmp(ool->done());
  } else {
    bailoutIf(Assembler::Equal, ool->ins()->snapshot());
    masm.jmp(ool->rejoin());
  }
}

void CodeGenerator::visitModI(LModI* ins) {
  Register remainder = ToRegister(ins->remainder());
  Register lhs = ToRegister(ins->lhs());
  Register rhs = ToRegister(ins->rhs());

  // Required to use idiv.
  MOZ_ASSERT_IF(lhs != rhs, rhs != eax);
  MOZ_ASSERT(rhs != edx);
  MOZ_ASSERT(remainder == edx);
  MOZ_ASSERT(ToRegister(ins->getTemp(0)) == eax);

  Label done;
  ReturnZero* ool = nullptr;
  ModOverflowCheck* overflow = nullptr;

  // Set up eax in preparation for doing a div.
  if (lhs != eax) {
    masm.mov(lhs, eax);
  }

  MMod* mir = ins->mir();

  // Prevent divide by zero.
  if (mir->canBeDivideByZero()) {
    masm.test32(rhs, rhs);
    if (mir->isTruncated()) {
      if (mir->trapOnError()) {
        Label nonZero;
        masm.j(Assembler::NonZero, &nonZero);
        masm.wasmTrap(wasm::Trap::IntegerDivideByZero, mir->bytecodeOffset());
        masm.bind(&nonZero);
      } else {
        if (!ool) {
          ool = new (alloc()) ReturnZero(edx);
        }
        masm.j(Assembler::Zero, ool->entry());
      }
    } else {
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  Label negative;

  // Switch based on sign of the lhs.
  if (mir->canBeNegativeDividend()) {
    masm.branchTest32(Assembler::Signed, lhs, lhs, &negative);
  }

  // If lhs >= 0 then remainder = lhs % rhs. The remainder must be positive.
  {
    // Check if rhs is a power-of-two.
    if (mir->canBePowerOfTwoDivisor()) {
      MOZ_ASSERT(rhs != remainder);

      // Rhs y is a power-of-two if (y & (y-1)) == 0. Note that if
      // y is any negative number other than INT32_MIN, both y and
      // y-1 will have the sign bit set so these are never optimized
      // as powers-of-two. If y is INT32_MIN, y-1 will be INT32_MAX
      // and because lhs >= 0 at this point, lhs & INT32_MAX returns
      // the correct value.
      Label notPowerOfTwo;
      masm.mov(rhs, remainder);
      masm.subl(Imm32(1), remainder);
      masm.branchTest32(Assembler::NonZero, remainder, rhs, &notPowerOfTwo);
      {
        masm.andl(lhs, remainder);
        masm.jmp(&done);
      }
      masm.bind(&notPowerOfTwo);
    }

    // Since lhs >= 0, the sign-extension will be 0
    masm.mov(ImmWord(0), edx);
    masm.idiv(rhs);
  }

  // Otherwise, we have to beware of two special cases:
  if (mir->canBeNegativeDividend()) {
    masm.jump(&done);

    masm.bind(&negative);

    // Prevent an integer overflow exception from -2147483648 % -1
    Label notmin;
    masm.cmp32(lhs, Imm32(INT32_MIN));
    overflow = new (alloc()) ModOverflowCheck(ins, rhs);
    masm.j(Assembler::Equal, overflow->entry());
    masm.bind(overflow->rejoin());
    masm.cdq();
    masm.idiv(rhs);

    if (!mir->isTruncated()) {
      // A remainder of 0 means that the rval must be -0, which is a double.
      masm.test32(remainder, remainder);
      bailoutIf(Assembler::Zero, ins->snapshot());
    }
  }

  masm.bind(&done);

  if (overflow) {
    addOutOfLineCode(overflow, mir);
    masm.bind(overflow->done());
  }

  if (ool) {
    addOutOfLineCode(ool, mir);
    masm.bind(ool->rejoin());
  }
}

void CodeGenerator::visitBitNotI(LBitNotI* ins) {
  const LAllocation* input = ins->getOperand(0);
  MOZ_ASSERT(!input->isConstant());

  masm.notl(ToOperand(input));
}

void CodeGenerator::visitBitOpI(LBitOpI* ins) {
  const LAllocation* lhs = ins->getOperand(0);
  const LAllocation* rhs = ins->getOperand(1);

  switch (ins->bitop()) {
    case JSOp::BitOr:
      if (rhs->isConstant()) {
        masm.orl(Imm32(ToInt32(rhs)), ToOperand(lhs));
      } else {
        masm.orl(ToOperand(rhs), ToRegister(lhs));
      }
      break;
    case JSOp::BitXor:
      if (rhs->isConstant()) {
        masm.xorl(Imm32(ToInt32(rhs)), ToOperand(lhs));
      } else {
        masm.xorl(ToOperand(rhs), ToRegister(lhs));
      }
      break;
    case JSOp::BitAnd:
      if (rhs->isConstant()) {
        masm.andl(Imm32(ToInt32(rhs)), ToOperand(lhs));
      } else {
        masm.andl(ToOperand(rhs), ToRegister(lhs));
      }
      break;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

void CodeGenerator::visitBitOpI64(LBitOpI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LBitOpI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LBitOpI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  switch (lir->bitop()) {
    case JSOp::BitOr:
      if (IsConstant(rhs)) {
        masm.or64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.or64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
      }
      break;
    case JSOp::BitXor:
      if (IsConstant(rhs)) {
        masm.xor64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.xor64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
      }
      break;
    case JSOp::BitAnd:
      if (IsConstant(rhs)) {
        masm.and64(Imm64(ToInt64(rhs)), ToRegister64(lhs));
      } else {
        masm.and64(ToOperandOrRegister64(rhs), ToRegister64(lhs));
      }
      break;
    default:
      MOZ_CRASH("unexpected binary opcode");
  }
}

void CodeGenerator::visitShiftI(LShiftI* ins) {
  Register lhs = ToRegister(ins->lhs());
  const LAllocation* rhs = ins->rhs();

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    switch (ins->bitop()) {
      case JSOp::Lsh:
        if (shift) {
          masm.shll(Imm32(shift), lhs);
        }
        break;
      case JSOp::Rsh:
        if (shift) {
          masm.sarl(Imm32(shift), lhs);
        }
        break;
      case JSOp::Ursh:
        if (shift) {
          masm.shrl(Imm32(shift), lhs);
        } else if (ins->mir()->toUrsh()->fallible()) {
          // x >>> 0 can overflow.
          masm.test32(lhs, lhs);
          bailoutIf(Assembler::Signed, ins->snapshot());
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  } else {
    MOZ_ASSERT(ToRegister(rhs) == ecx);
    switch (ins->bitop()) {
      case JSOp::Lsh:
        masm.shll_cl(lhs);
        break;
      case JSOp::Rsh:
        masm.sarl_cl(lhs);
        break;
      case JSOp::Ursh:
        masm.shrl_cl(lhs);
        if (ins->mir()->toUrsh()->fallible()) {
          // x >>> 0 can overflow.
          masm.test32(lhs, lhs);
          bailoutIf(Assembler::Signed, ins->snapshot());
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
  }
}

void CodeGenerator::visitShiftI64(LShiftI64* lir) {
  const LInt64Allocation lhs = lir->getInt64Operand(LShiftI64::Lhs);
  LAllocation* rhs = lir->getOperand(LShiftI64::Rhs);

  MOZ_ASSERT(ToOutRegister64(lir) == ToRegister64(lhs));

  if (rhs->isConstant()) {
    int32_t shift = int32_t(rhs->toConstant()->toInt64() & 0x3F);
    switch (lir->bitop()) {
      case JSOp::Lsh:
        if (shift) {
          masm.lshift64(Imm32(shift), ToRegister64(lhs));
        }
        break;
      case JSOp::Rsh:
        if (shift) {
          masm.rshift64Arithmetic(Imm32(shift), ToRegister64(lhs));
        }
        break;
      case JSOp::Ursh:
        if (shift) {
          masm.rshift64(Imm32(shift), ToRegister64(lhs));
        }
        break;
      default:
        MOZ_CRASH("Unexpected shift op");
    }
    return;
  }

  MOZ_ASSERT(ToRegister(rhs) == ecx);
  switch (lir->bitop()) {
    case JSOp::Lsh:
      masm.lshift64(ecx, ToRegister64(lhs));
      break;
    case JSOp::Rsh:
      masm.rshift64Arithmetic(ecx, ToRegister64(lhs));
      break;
    case JSOp::Ursh:
      masm.rshift64(ecx, ToRegister64(lhs));
      break;
    default:
      MOZ_CRASH("Unexpected shift op");
  }
}

void CodeGenerator::visitUrshD(LUrshD* ins) {
  Register lhs = ToRegister(ins->lhs());
  MOZ_ASSERT(ToRegister(ins->temp()) == lhs);

  const LAllocation* rhs = ins->rhs();
  FloatRegister out = ToFloatRegister(ins->output());

  if (rhs->isConstant()) {
    int32_t shift = ToInt32(rhs) & 0x1F;
    if (shift) {
      masm.shrl(Imm32(shift), lhs);
    }
  } else {
    MOZ_ASSERT(ToRegister(rhs) == ecx);
    masm.shrl_cl(lhs);
  }

  masm.convertUInt32ToDouble(lhs, out);
}

Operand CodeGeneratorX86Shared::ToOperand(const LAllocation& a) {
  if (a.isGeneralReg()) {
    return Operand(a.toGeneralReg()->reg());
  }
  if (a.isFloatReg()) {
    return Operand(a.toFloatReg()->reg());
  }
  return Operand(ToAddress(a));
}

Operand CodeGeneratorX86Shared::ToOperand(const LAllocation* a) {
  return ToOperand(*a);
}

Operand CodeGeneratorX86Shared::ToOperand(const LDefinition* def) {
  return ToOperand(def->output());
}

MoveOperand CodeGeneratorX86Shared::toMoveOperand(LAllocation a) const {
  if (a.isGeneralReg()) {
    return MoveOperand(ToRegister(a));
  }
  if (a.isFloatReg()) {
    return MoveOperand(ToFloatRegister(a));
  }
  MoveOperand::Kind kind =
      a.isStackArea() ? MoveOperand::EFFECTIVE_ADDRESS : MoveOperand::MEMORY;
  return MoveOperand(ToAddress(a), kind);
}

class OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorX86Shared> {
  MTableSwitch* mir_;
  CodeLabel jumpLabel_;

  void accept(CodeGeneratorX86Shared* codegen) override {
    codegen->visitOutOfLineTableSwitch(this);
  }

 public:
  explicit OutOfLineTableSwitch(MTableSwitch* mir) : mir_(mir) {}

  MTableSwitch* mir() const { return mir_; }

  CodeLabel* jumpLabel() { return &jumpLabel_; }
};

void CodeGeneratorX86Shared::visitOutOfLineTableSwitch(
    OutOfLineTableSwitch* ool) {
  MTableSwitch* mir = ool->mir();

  masm.haltingAlign(sizeof(void*));
  masm.bind(ool->jumpLabel());
  masm.addCodeLabel(*ool->jumpLabel());

  for (size_t i = 0; i < mir->numCases(); i++) {
    LBlock* caseblock = skipTrivialBlocks(mir->getCase(i))->lir();
    Label* caseheader = caseblock->label();
    uint32_t caseoffset = caseheader->offset();

    // The entries of the jump table need to be absolute addresses and thus
    // must be patched after codegen is finished.
    CodeLabel cl;
    masm.writeCodePointer(&cl);
    cl.target()->bind(caseoffset);
    masm.addCodeLabel(cl);
  }
}

void CodeGeneratorX86Shared::emitTableSwitchDispatch(MTableSwitch* mir,
                                                     Register index,
                                                     Register base) {
  Label* defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

  // Lower value with low value
  if (mir->low() != 0) {
    masm.subl(Imm32(mir->low()), index);
  }

  // Jump to default case if input is out of range
  int32_t cases = mir->numCases();
  masm.cmp32(index, Imm32(cases));
  masm.j(AssemblerX86Shared::AboveOrEqual, defaultcase);

  // To fill in the CodeLabels for the case entries, we need to first
  // generate the case entries (we don't yet know their offsets in the
  // instruction stream).
  OutOfLineTableSwitch* ool = new (alloc()) OutOfLineTableSwitch(mir);
  addOutOfLineCode(ool, mir);

  // Compute the position where a pointer to the right case stands.
  masm.mov(ool->jumpLabel(), base);
  BaseIndex pointer(base, index, ScalePointer);

  // Jump to the right case
  masm.branchToComputedAddress(pointer);
}

void CodeGenerator::visitMathD(LMathD* math) {
  FloatRegister lhs = ToFloatRegister(math->lhs());
  Operand rhs = ToOperand(math->rhs());
  FloatRegister output = ToFloatRegister(math->output());

  switch (math->jsop()) {
    case JSOp::Add:
      masm.vaddsd(rhs, lhs, output);
      break;
    case JSOp::Sub:
      masm.vsubsd(rhs, lhs, output);
      break;
    case JSOp::Mul:
      masm.vmulsd(rhs, lhs, output);
      break;
    case JSOp::Div:
      masm.vdivsd(rhs, lhs, output);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitMathF(LMathF* math) {
  FloatRegister lhs = ToFloatRegister(math->lhs());
  Operand rhs = ToOperand(math->rhs());
  FloatRegister output = ToFloatRegister(math->output());

  switch (math->jsop()) {
    case JSOp::Add:
      masm.vaddss(rhs, lhs, output);
      break;
    case JSOp::Sub:
      masm.vsubss(rhs, lhs, output);
      break;
    case JSOp::Mul:
      masm.vmulss(rhs, lhs, output);
      break;
    case JSOp::Div:
      masm.vdivss(rhs, lhs, output);
      break;
    default:
      MOZ_CRASH("unexpected opcode");
  }
}

void CodeGenerator::visitFloor(LFloor* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;

  if (AssemblerX86Shared::HasSSE41()) {
    // Bail on negative-zero.
    masm.branchNegativeZero(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());

    // Round toward -Infinity.
    {
      ScratchDoubleScope scratch(masm);
      masm.vroundsd(X86Encoding::RoundDown, input, scratch, scratch);
      bailoutCvttsd2si(scratch, output, lir->snapshot());
    }
  } else {
    Label negative, end;

    // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
    {
      ScratchDoubleScope scratch(masm);
      masm.zeroDouble(scratch);
      masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &negative);
    }

    // Bail on negative-zero.
    masm.branchNegativeZero(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());

    // Input is non-negative, so truncation correctly rounds.
    bailoutCvttsd2si(input, output, lir->snapshot());

    masm.jump(&end);

    // Input is negative, but isn't -0.
    // Negative values go on a comparatively expensive path, since no
    // native rounding mode matches JS semantics. Still better than callVM.
    masm.bind(&negative);
    {
      // Truncate and round toward zero.
      // This is off-by-one for everything but integer-valued inputs.
      bailoutCvttsd2si(input, output, lir->snapshot());

      // Test whether the input double was integer-valued.
      {
        ScratchDoubleScope scratch(masm);
        masm.convertInt32ToDouble(output, scratch);
        masm.branchDouble(Assembler::DoubleEqualOrUnordered, input, scratch,
                          &end);
      }

      // Input is not integer-valued, so we rounded off-by-one in the
      // wrong direction. Correct by subtraction.
      masm.subl(Imm32(1), output);
      // Cannot overflow: output was already checked against INT_MIN.
    }

    masm.bind(&end);
  }
}

void CodeGenerator::visitFloorF(LFloorF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout;

  if (AssemblerX86Shared::HasSSE41()) {
    // Bail on negative-zero.
    masm.branchNegativeZeroFloat32(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());

    // Round toward -Infinity.
    {
      ScratchFloat32Scope scratch(masm);
      masm.vroundss(X86Encoding::RoundDown, input, scratch, scratch);
      bailoutCvttss2si(scratch, output, lir->snapshot());
    }
  } else {
    Label negative, end;

    // Branch to a slow path for negative inputs. Doesn't catch NaN or -0.
    {
      ScratchFloat32Scope scratch(masm);
      masm.zeroFloat32(scratch);
      masm.branchFloat(Assembler::DoubleLessThan, input, scratch, &negative);
    }

    // Bail on negative-zero.
    masm.branchNegativeZeroFloat32(input, output, &bailout);
    bailoutFrom(&bailout, lir->snapshot());

    // Input is non-negative, so truncation correctly rounds.
    bailoutCvttss2si(input, output, lir->snapshot());

    masm.jump(&end);

    // Input is negative, but isn't -0.
    // Negative values go on a comparatively expensive path, since no
    // native rounding mode matches JS semantics. Still better than callVM.
    masm.bind(&negative);
    {
      // Truncate and round toward zero.
      // This is off-by-one for everything but integer-valued inputs.
      bailoutCvttss2si(input, output, lir->snapshot());

      // Test whether the input double was integer-valued.
      {
        ScratchFloat32Scope scratch(masm);
        masm.convertInt32ToFloat32(output, scratch);
        masm.branchFloat(Assembler::DoubleEqualOrUnordered, input, scratch,
                         &end);
      }

      // Input is not integer-valued, so we rounded off-by-one in the
      // wrong direction. Correct by subtraction.
      masm.subl(Imm32(1), output);
      // Cannot overflow: output was already checked against INT_MIN.
    }

    masm.bind(&end);
  }
}

void CodeGenerator::visitCeil(LCeil* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  ScratchDoubleScope scratch(masm);
  Register output = ToRegister(lir->output());

  Label bailout, lessThanMinusOne;

  // Bail on ]-1; -0] range
  masm.loadConstantDouble(-1, scratch);
  masm.branchDouble(Assembler::DoubleLessThanOrEqualOrUnordered, input, scratch,
                    &lessThanMinusOne);

  // Test for remaining values with the sign bit set, i.e. ]-1; -0]
  masm.vmovmskpd(input, output);
  masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
  bailoutFrom(&bailout, lir->snapshot());

  if (AssemblerX86Shared::HasSSE41()) {
    // x <= -1 or x > -0
    masm.bind(&lessThanMinusOne);
    // Round toward +Infinity.
    masm.vroundsd(X86Encoding::RoundUp, input, scratch, scratch);
    bailoutCvttsd2si(scratch, output, lir->snapshot());
    return;
  }

  // No SSE4.1
  Label end;

  // x >= 0 and x is not -0.0, we can truncate (resp. truncate and add 1) for
  // integer (resp. non-integer) values.
  // Will also work for values >= INT_MAX + 1, as the truncate
  // operation will return INT_MIN and there'll be a bailout.
  bailoutCvttsd2si(input, output, lir->snapshot());
  masm.convertInt32ToDouble(output, scratch);
  masm.branchDouble(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

  // Input is not integer-valued, add 1 to obtain the ceiling value
  masm.addl(Imm32(1), output);
  // if input > INT_MAX, output == INT_MAX so adding 1 will overflow.
  bailoutIf(Assembler::Overflow, lir->snapshot());
  masm.jump(&end);

  // x <= -1, truncation is the way to go.
  masm.bind(&lessThanMinusOne);
  bailoutCvttsd2si(input, output, lir->snapshot());

  masm.bind(&end);
}

void CodeGenerator::visitCeilF(LCeilF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  ScratchFloat32Scope scratch(masm);
  Register output = ToRegister(lir->output());

  Label bailout, lessThanMinusOne;

  // Bail on ]-1; -0] range
  masm.loadConstantFloat32(-1.f, scratch);
  masm.branchFloat(Assembler::DoubleLessThanOrEqualOrUnordered, input, scratch,
                   &lessThanMinusOne);

  // Test for remaining values with the sign bit set, i.e. ]-1; -0]
  masm.vmovmskps(input, output);
  masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
  bailoutFrom(&bailout, lir->snapshot());

  if (AssemblerX86Shared::HasSSE41()) {
    // x <= -1 or x > -0
    masm.bind(&lessThanMinusOne);
    // Round toward +Infinity.
    masm.vroundss(X86Encoding::RoundUp, input, scratch, scratch);
    bailoutCvttss2si(scratch, output, lir->snapshot());
    return;
  }

  // No SSE4.1
  Label end;

  // x >= 0 and x is not -0.0, we can truncate (resp. truncate and add 1) for
  // integer (resp. non-integer) values.
  // Will also work for values >= INT_MAX + 1, as the truncate
  // operation will return INT_MIN and there'll be a bailout.
  bailoutCvttss2si(input, output, lir->snapshot());
  masm.convertInt32ToFloat32(output, scratch);
  masm.branchFloat(Assembler::DoubleEqualOrUnordered, input, scratch, &end);

  // Input is not integer-valued, add 1 to obtain the ceiling value
  masm.addl(Imm32(1), output);
  // if input > INT_MAX, output == INT_MAX so adding 1 will overflow.
  bailoutIf(Assembler::Overflow, lir->snapshot());
  masm.jump(&end);

  // x <= -1, truncation is the way to go.
  masm.bind(&lessThanMinusOne);
  bailoutCvttss2si(input, output, lir->snapshot());

  masm.bind(&end);
}

void CodeGenerator::visitRound(LRound* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister temp = ToFloatRegister(lir->temp());
  ScratchDoubleScope scratch(masm);
  Register output = ToRegister(lir->output());

  Label negativeOrZero, negative, end, bailout;

  // Branch to a slow path for non-positive inputs. Doesn't catch NaN.
  masm.zeroDouble(scratch);
  masm.loadConstantDouble(GetBiggestNumberLessThan(0.5), temp);
  masm.branchDouble(Assembler::DoubleLessThanOrEqual, input, scratch,
                    &negativeOrZero);

  // Input is positive. Add the biggest double less than 0.5 and
  // truncate, rounding down (because if the input is the biggest double less
  // than 0.5, adding 0.5 would undesirably round up to 1). Note that we have
  // to add the input to the temp register because we're not allowed to
  // modify the input register.
  masm.addDouble(input, temp);
  bailoutCvttsd2si(temp, output, lir->snapshot());

  masm.jump(&end);

  // Input is negative, +0 or -0.
  masm.bind(&negativeOrZero);
  // Branch on negative input.
  masm.j(Assembler::NotEqual, &negative);

  // Bail on negative-zero.
  masm.branchNegativeZero(input, output, &bailout, /* maybeNonZero = */ false);
  bailoutFrom(&bailout, lir->snapshot());

  // Input is +0
  masm.xor32(output, output);
  masm.jump(&end);

  // Input is negative.
  masm.bind(&negative);

  // Inputs in ]-0.5; 0] need to be added 0.5, other negative inputs need to
  // be added the biggest double less than 0.5.
  Label loadJoin;
  masm.loadConstantDouble(-0.5, scratch);
  masm.branchDouble(Assembler::DoubleLessThan, input, scratch, &loadJoin);
  masm.loadConstantDouble(0.5, temp);
  masm.bind(&loadJoin);

  if (AssemblerX86Shared::HasSSE41()) {
    // Add 0.5 and round toward -Infinity. The result is stored in the temp
    // register (currently contains 0.5).
    masm.addDouble(input, temp);
    masm.vroundsd(X86Encoding::RoundDown, temp, scratch, scratch);

    // Truncate.
    bailoutCvttsd2si(scratch, output, lir->snapshot());

    // If the result is positive zero, then the actual result is -0. Bail.
    // Otherwise, the truncation will have produced the correct negative
    // integer.
    masm.test32(output, output);
    bailoutIf(Assembler::Zero, lir->snapshot());
  } else {
    masm.addDouble(input, temp);

    // Round toward -Infinity without the benefit of ROUNDSD.
    {
      // If input + 0.5 >= 0, input is a negative number >= -0.5 and the result
      // is -0.
      masm.compareDouble(Assembler::DoubleGreaterThanOrEqual, temp, scratch);
      bailoutIf(Assembler::DoubleGreaterThanOrEqual, lir->snapshot());

      // Truncate and round toward zero.
      // This is off-by-one for everything but integer-valued inputs.
      bailoutCvttsd2si(temp, output, lir->snapshot());

      // Test whether the truncated double was integer-valued.
      masm.convertInt32ToDouble(output, scratch);
      masm.branchDouble(Assembler::DoubleEqualOrUnordered, temp, scratch, &end);

      // Input is not integer-valued, so we rounded off-by-one in the
      // wrong direction. Correct by subtraction.
      masm.subl(Imm32(1), output);
      // Cannot overflow: output was already checked against INT_MIN.
    }
  }

  masm.bind(&end);
}

void CodeGenerator::visitRoundF(LRoundF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister temp = ToFloatRegister(lir->temp());
  ScratchFloat32Scope scratch(masm);
  Register output = ToRegister(lir->output());

  Label negativeOrZero, negative, end, bailout;

  // Branch to a slow path for non-positive inputs. Doesn't catch NaN.
  masm.zeroFloat32(scratch);
  masm.loadConstantFloat32(GetBiggestNumberLessThan(0.5f), temp);
  masm.branchFloat(Assembler::DoubleLessThanOrEqual, input, scratch,
                   &negativeOrZero);

  // Input is non-negative. Add the biggest float less than 0.5 and truncate,
  // rounding down (because if the input is the biggest float less than 0.5,
  // adding 0.5 would undesirably round up to 1). Note that we have to add
  // the input to the temp register because we're not allowed to modify the
  // input register.
  masm.addFloat32(input, temp);

  bailoutCvttss2si(temp, output, lir->snapshot());

  masm.jump(&end);

  // Input is negative, +0 or -0.
  masm.bind(&negativeOrZero);
  // Branch on negative input.
  masm.j(Assembler::NotEqual, &negative);

  // Bail on negative-zero.
  masm.branchNegativeZeroFloat32(input, output, &bailout);
  bailoutFrom(&bailout, lir->snapshot());

  // Input is +0.
  masm.xor32(output, output);
  masm.jump(&end);

  // Input is negative.
  masm.bind(&negative);

  // Inputs in ]-0.5; 0] need to be added 0.5, other negative inputs need to
  // be added the biggest double less than 0.5.
  Label loadJoin;
  masm.loadConstantFloat32(-0.5f, scratch);
  masm.branchFloat(Assembler::DoubleLessThan, input, scratch, &loadJoin);
  masm.loadConstantFloat32(0.5f, temp);
  masm.bind(&loadJoin);

  if (AssemblerX86Shared::HasSSE41()) {
    // Add 0.5 and round toward -Infinity. The result is stored in the temp
    // register (currently contains 0.5).
    masm.addFloat32(input, temp);
    masm.vroundss(X86Encoding::RoundDown, temp, scratch, scratch);

    // Truncate.
    bailoutCvttss2si(scratch, output, lir->snapshot());

    // If the result is positive zero, then the actual result is -0. Bail.
    // Otherwise, the truncation will have produced the correct negative
    // integer.
    masm.test32(output, output);
    bailoutIf(Assembler::Zero, lir->snapshot());
  } else {
    masm.addFloat32(input, temp);
    // Round toward -Infinity without the benefit of ROUNDSS.
    {
      // If input + 0.5 >= 0, input is a negative number >= -0.5 and the result
      // is -0.
      masm.compareFloat(Assembler::DoubleGreaterThanOrEqual, temp, scratch);
      bailoutIf(Assembler::DoubleGreaterThanOrEqual, lir->snapshot());

      // Truncate and round toward zero.
      // This is off-by-one for everything but integer-valued inputs.
      bailoutCvttss2si(temp, output, lir->snapshot());

      // Test whether the truncated double was integer-valued.
      masm.convertInt32ToFloat32(output, scratch);
      masm.branchFloat(Assembler::DoubleEqualOrUnordered, temp, scratch, &end);

      // Input is not integer-valued, so we rounded off-by-one in the
      // wrong direction. Correct by subtraction.
      masm.subl(Imm32(1), output);
      // Cannot overflow: output was already checked against INT_MIN.
    }
  }

  masm.bind(&end);
}

void CodeGenerator::visitTrunc(LTrunc* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout, lessThanMinusOne;

  // Bail on ]-1; -0] range
  {
    ScratchDoubleScope scratch(masm);
    masm.loadConstantDouble(-1, scratch);
    masm.branchDouble(Assembler::DoubleLessThanOrEqualOrUnordered, input,
                      scratch, &lessThanMinusOne);
  }

  // Test for remaining values with the sign bit set, i.e. ]-1; -0]
  masm.vmovmskpd(input, output);
  masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
  bailoutFrom(&bailout, lir->snapshot());

  // x <= -1 or x >= +0, truncation is the way to go.
  masm.bind(&lessThanMinusOne);
  bailoutCvttsd2si(input, output, lir->snapshot());
}

void CodeGenerator::visitTruncF(LTruncF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register output = ToRegister(lir->output());

  Label bailout, lessThanMinusOne;

  // Bail on ]-1; -0] range
  {
    ScratchFloat32Scope scratch(masm);
    masm.loadConstantFloat32(-1.f, scratch);
    masm.branchFloat(Assembler::DoubleLessThanOrEqualOrUnordered, input,
                     scratch, &lessThanMinusOne);
  }

  // Test for remaining values with the sign bit set, i.e. ]-1; -0]
  masm.vmovmskps(input, output);
  masm.branchTest32(Assembler::NonZero, output, Imm32(1), &bailout);
  bailoutFrom(&bailout, lir->snapshot());

  // x <= -1 or x >= +0, truncation is the way to go.
  masm.bind(&lessThanMinusOne);
  bailoutCvttss2si(input, output, lir->snapshot());
}

void CodeGenerator::visitNearbyInt(LNearbyInt* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister output = ToFloatRegister(lir->output());

  RoundingMode roundingMode = lir->mir()->roundingMode();
  masm.vroundsd(Assembler::ToX86RoundingMode(roundingMode), input, output,
                output);
}

void CodeGenerator::visitNearbyIntF(LNearbyIntF* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  FloatRegister output = ToFloatRegister(lir->output());

  RoundingMode roundingMode = lir->mir()->roundingMode();
  masm.vroundss(Assembler::ToX86RoundingMode(roundingMode), input, output,
                output);
}

void CodeGenerator::visitEffectiveAddress(LEffectiveAddress* ins) {
  const MEffectiveAddress* mir = ins->mir();
  Register base = ToRegister(ins->base());
  Register index = ToRegister(ins->index());
  Register output = ToRegister(ins->output());
  masm.leal(Operand(base, index, mir->scale(), mir->displacement()), output);
}

void CodeGeneratorX86Shared::generateInvalidateEpilogue() {
  // Ensure that there is enough space in the buffer for the OsiPoint
  // patching to occur. Otherwise, we could overwrite the invalidation
  // epilogue.
  for (size_t i = 0; i < sizeof(void*); i += Assembler::NopSize()) {
    masm.nop();
  }

  masm.bind(&invalidate_);

  // Push the Ion script onto the stack (when we determine what that pointer
  // is).
  invalidateEpilogueData_ = masm.pushWithPatch(ImmWord(uintptr_t(-1)));

  // Jump to the invalidator which will replace the current frame.
  TrampolinePtr thunk = gen->jitRuntime()->getInvalidationThunk();
  masm.jump(thunk);
}

void CodeGenerator::visitNegI(LNegI* ins) {
  Register input = ToRegister(ins->input());
  MOZ_ASSERT(input == ToRegister(ins->output()));

  masm.neg32(input);
}

void CodeGenerator::visitNegD(LNegD* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  MOZ_ASSERT(input == ToFloatRegister(ins->output()));

  masm.negateDouble(input);
}

void CodeGenerator::visitNegF(LNegF* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  MOZ_ASSERT(input == ToFloatRegister(ins->output()));

  masm.negateFloat(input);
}

void CodeGenerator::visitCompareExchangeTypedArrayElement(
    LCompareExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register oldval = ToRegister(lir->oldval());
  Register newval = ToRegister(lir->newval());

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
    masm.compareExchangeJS(arrayType, Synchronization::Full(), dest, oldval,
                           newval, temp, output);
  }
}

void CodeGenerator::visitAtomicExchangeTypedArrayElement(
    LAtomicExchangeTypedArrayElement* lir) {
  Register elements = ToRegister(lir->elements());
  AnyRegister output = ToAnyRegister(lir->output());
  Register temp =
      lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());

  Register value = ToRegister(lir->value());

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address dest(elements, ToInt32(lir->index()) * width);
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromElemWidth(width));
    masm.atomicExchangeJS(arrayType, Synchronization::Full(), dest, value, temp,
                          output);
  }
}

template <typename T>
static inline void AtomicBinopToTypedArray(MacroAssembler& masm, AtomicOp op,
                                           Scalar::Type arrayType,
                                           const LAllocation* value,
                                           const T& mem, Register temp1,
                                           Register temp2, AnyRegister output) {
  if (value->isConstant()) {
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(), op,
                         Imm32(ToInt32(value)), mem, temp1, temp2, output);
  } else {
    masm.atomicFetchOpJS(arrayType, Synchronization::Full(), op,
                         ToRegister(value), mem, temp1, temp2, output);
  }
}

void CodeGenerator::visitAtomicTypedArrayElementBinop(
    LAtomicTypedArrayElementBinop* lir) {
  MOZ_ASSERT(lir->mir()->hasUses());

  AnyRegister output = ToAnyRegister(lir->output());
  Register elements = ToRegister(lir->elements());
  Register temp1 =
      lir->temp1()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp1());
  Register temp2 =
      lir->temp2()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp2());
  const LAllocation* value = lir->value();

  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address mem(elements, ToInt32(lir->index()) * width);
    AtomicBinopToTypedArray(masm, lir->mir()->operation(), arrayType, value,
                            mem, temp1, temp2, output);
  } else {
    BaseIndex mem(elements, ToRegister(lir->index()),
                  ScaleFromElemWidth(width));
    AtomicBinopToTypedArray(masm, lir->mir()->operation(), arrayType, value,
                            mem, temp1, temp2, output);
  }
}

template <typename T>
static inline void AtomicBinopToTypedArray(MacroAssembler& masm,
                                           Scalar::Type arrayType, AtomicOp op,
                                           const LAllocation* value,
                                           const T& mem) {
  if (value->isConstant()) {
    masm.atomicEffectOpJS(arrayType, Synchronization::Full(), op,
                          Imm32(ToInt32(value)), mem, InvalidReg);
  } else {
    masm.atomicEffectOpJS(arrayType, Synchronization::Full(), op,
                          ToRegister(value), mem, InvalidReg);
  }
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect(
    LAtomicTypedArrayElementBinopForEffect* lir) {
  MOZ_ASSERT(!lir->mir()->hasUses());

  Register elements = ToRegister(lir->elements());
  const LAllocation* value = lir->value();
  Scalar::Type arrayType = lir->mir()->arrayType();
  size_t width = Scalar::byteSize(arrayType);

  if (lir->index()->isConstant()) {
    Address mem(elements, ToInt32(lir->index()) * width);
    AtomicBinopToTypedArray(masm, arrayType, lir->mir()->operation(), value,
                            mem);
  } else {
    BaseIndex mem(elements, ToRegister(lir->index()),
                  ScaleFromElemWidth(width));
    AtomicBinopToTypedArray(masm, arrayType, lir->mir()->operation(), value,
                            mem);
  }
}

void CodeGenerator::visitMemoryBarrier(LMemoryBarrier* ins) {
  if (ins->type() & MembarStoreLoad) {
    masm.storeLoadFence();
  }
}

void CodeGeneratorX86Shared::visitOutOfLineWasmTruncateCheck(
    OutOfLineWasmTruncateCheck* ool) {
  FloatRegister input = ool->input();
  Register output = ool->output();
  Register64 output64 = ool->output64();
  MIRType fromType = ool->fromType();
  MIRType toType = ool->toType();
  Label* oolRejoin = ool->rejoin();
  TruncFlags flags = ool->flags();
  wasm::BytecodeOffset off = ool->bytecodeOffset();

  if (fromType == MIRType::Float32) {
    if (toType == MIRType::Int32) {
      masm.oolWasmTruncateCheckF32ToI32(input, output, flags, off, oolRejoin);
    } else if (toType == MIRType::Int64) {
      masm.oolWasmTruncateCheckF32ToI64(input, output64, flags, off, oolRejoin);
    } else {
      MOZ_CRASH("unexpected type");
    }
  } else if (fromType == MIRType::Double) {
    if (toType == MIRType::Int32) {
      masm.oolWasmTruncateCheckF64ToI32(input, output, flags, off, oolRejoin);
    } else if (toType == MIRType::Int64) {
      masm.oolWasmTruncateCheckF64ToI64(input, output64, flags, off, oolRejoin);
    } else {
      MOZ_CRASH("unexpected type");
    }
  } else {
    MOZ_CRASH("unexpected type");
  }
}

void CodeGeneratorX86Shared::canonicalizeIfDeterministic(
    Scalar::Type type, const LAllocation* value) {
#ifdef JS_MORE_DETERMINISTIC
  switch (type) {
    case Scalar::Float32: {
      FloatRegister in = ToFloatRegister(value);
      masm.canonicalizeFloatIfDeterministic(in);
      break;
    }
    case Scalar::Float64: {
      FloatRegister in = ToFloatRegister(value);
      masm.canonicalizeDoubleIfDeterministic(in);
      break;
    }
    default: {
      // Other types don't need canonicalization.
      break;
    }
  }
#endif  // JS_MORE_DETERMINISTIC
}

void CodeGenerator::visitCopySignF(LCopySignF* lir) {
  FloatRegister lhs = ToFloatRegister(lir->getOperand(0));
  FloatRegister rhs = ToFloatRegister(lir->getOperand(1));

  FloatRegister out = ToFloatRegister(lir->output());

  if (lhs == rhs) {
    if (lhs != out) {
      masm.moveFloat32(lhs, out);
    }
    return;
  }

  ScratchFloat32Scope scratch(masm);

  float clearSignMask = BitwiseCast<float>(INT32_MAX);
  masm.loadConstantFloat32(clearSignMask, scratch);
  masm.vandps(scratch, lhs, out);

  float keepSignMask = BitwiseCast<float>(INT32_MIN);
  masm.loadConstantFloat32(keepSignMask, scratch);
  masm.vandps(rhs, scratch, scratch);

  masm.vorps(scratch, out, out);
}

void CodeGenerator::visitCopySignD(LCopySignD* lir) {
  FloatRegister lhs = ToFloatRegister(lir->getOperand(0));
  FloatRegister rhs = ToFloatRegister(lir->getOperand(1));

  FloatRegister out = ToFloatRegister(lir->output());

  if (lhs == rhs) {
    if (lhs != out) {
      masm.moveDouble(lhs, out);
    }
    return;
  }

  ScratchDoubleScope scratch(masm);

  double clearSignMask = BitwiseCast<double>(INT64_MAX);
  masm.loadConstantDouble(clearSignMask, scratch);
  masm.vandpd(scratch, lhs, out);

  double keepSignMask = BitwiseCast<double>(INT64_MIN);
  masm.loadConstantDouble(keepSignMask, scratch);
  masm.vandpd(rhs, scratch, scratch);

  masm.vorpd(scratch, out, out);
}

void CodeGenerator::visitRotateI64(LRotateI64* lir) {
  MRotate* mir = lir->mir();
  LAllocation* count = lir->count();

  Register64 input = ToRegister64(lir->input());
  Register64 output = ToOutRegister64(lir);
  Register temp = ToTempRegisterOrInvalid(lir->temp());

  MOZ_ASSERT(input == output);

  if (count->isConstant()) {
    int32_t c = int32_t(count->toConstant()->toInt64() & 0x3F);
    if (!c) {
      return;
    }
    if (mir->isLeftRotate()) {
      masm.rotateLeft64(Imm32(c), input, output, temp);
    } else {
      masm.rotateRight64(Imm32(c), input, output, temp);
    }
  } else {
    if (mir->isLeftRotate()) {
      masm.rotateLeft64(ToRegister(count), input, output, temp);
    } else {
      masm.rotateRight64(ToRegister(count), input, output, temp);
    }
  }
}

void CodeGenerator::visitPopcntI64(LPopcntI64* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  Register64 output = ToOutRegister64(lir);
  Register temp = InvalidReg;
  if (!AssemblerX86Shared::HasPOPCNT()) {
    temp = ToRegister(lir->getTemp(0));
  }

  masm.popcnt64(input, output, temp);
}

}  // namespace jit
}  // namespace js
