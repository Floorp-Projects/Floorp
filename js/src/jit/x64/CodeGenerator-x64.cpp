/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/CodeGenerator-x64.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"

#include "jsscriptinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

CodeGeneratorX64::CodeGeneratorX64(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm)
  : CodeGeneratorX86Shared(gen, graph, masm)
{
}

ValueOperand
CodeGeneratorX64::ToValue(LInstruction* ins, size_t pos)
{
    return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand
CodeGeneratorX64::ToOutValue(LInstruction* ins)
{
    return ValueOperand(ToRegister(ins->getDef(0)));
}

ValueOperand
CodeGeneratorX64::ToTempValue(LInstruction* ins, size_t pos)
{
    return ValueOperand(ToRegister(ins->getTemp(pos)));
}

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(0);
}

uint32_t
FrameSizeClass::frameSize() const
{
    MOZ_CRASH("x64 does not use frame size classes");
}

void
CodeGeneratorX64::visitValue(LValue* value)
{
    LDefinition* reg = value->getDef(0);
    masm.moveValue(value->value(), ToRegister(reg));
}

void
CodeGeneratorX64::visitBox(LBox* box)
{
    const LAllocation* in = box->getOperand(0);
    const LDefinition* result = box->getDef(0);

    if (IsFloatingPointType(box->type())) {
        ScratchDoubleScope scratch(masm);
        FloatRegister reg = ToFloatRegister(in);
        if (box->type() == MIRType::Float32) {
            masm.convertFloat32ToDouble(reg, scratch);
            reg = scratch;
        }
        masm.vmovq(reg, ToRegister(result));
    } else {
        masm.boxValue(ValueTypeFromMIRType(box->type()), ToRegister(in), ToRegister(result));
    }
}

void
CodeGeneratorX64::visitUnbox(LUnbox* unbox)
{
    MUnbox* mir = unbox->mir();

    if (mir->fallible()) {
        const ValueOperand value = ToValue(unbox, LUnbox::Input);
        Assembler::Condition cond;
        switch (mir->type()) {
          case MIRType::Int32:
            cond = masm.testInt32(Assembler::NotEqual, value);
            break;
          case MIRType::Boolean:
            cond = masm.testBoolean(Assembler::NotEqual, value);
            break;
          case MIRType::Object:
            cond = masm.testObject(Assembler::NotEqual, value);
            break;
          case MIRType::String:
            cond = masm.testString(Assembler::NotEqual, value);
            break;
          case MIRType::Symbol:
            cond = masm.testSymbol(Assembler::NotEqual, value);
            break;
          default:
            MOZ_CRASH("Given MIRType cannot be unboxed.");
        }
        bailoutIf(cond, unbox->snapshot());
    }

    Operand input = ToOperand(unbox->getOperand(LUnbox::Input));
    Register result = ToRegister(unbox->output());
    switch (mir->type()) {
      case MIRType::Int32:
        masm.unboxInt32(input, result);
        break;
      case MIRType::Boolean:
        masm.unboxBoolean(input, result);
        break;
      case MIRType::Object:
        masm.unboxObject(input, result);
        break;
      case MIRType::String:
        masm.unboxString(input, result);
        break;
      case MIRType::Symbol:
        masm.unboxSymbol(input, result);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
}

void
CodeGeneratorX64::visitCompareB(LCompareB* lir)
{
    MCompare* mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation* rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    ScratchRegisterScope scratch(masm);
    if (rhs->isConstant())
        masm.moveValue(rhs->toConstant()->toJSValue(), scratch);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);

    // Perform the comparison.
    masm.cmpPtr(lhs.valueReg(), scratch);
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
}

void
CodeGeneratorX64::visitCompareBAndBranch(LCompareBAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation* rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    ScratchRegisterScope scratch(masm);
    if (rhs->isConstant())
        masm.moveValue(rhs->toConstant()->toJSValue(), scratch);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);

    // Perform the comparison.
    masm.cmpPtr(lhs.valueReg(), scratch);
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitCompareBitwise(LCompareBitwise* lir)
{
    MCompare* mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
}

void
CodeGeneratorX64::visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitCompareI64(LCompareI64* lir)
{
    MCompare* mir = lir->mir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register lhsReg = ToRegister64(lhs).reg;
    Register output = ToRegister(lir->output());

    if (IsConstant(rhs)) {
        ImmWord imm = ImmWord(ToInt64(rhs));
        masm.cmpPtr(lhsReg, imm);
    } else {
        Register rhsReg = ToRegister64(rhs).reg;
        masm.cmpPtr(lhsReg, Operand(rhsReg));
    }

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    masm.emitSet(JSOpToCondition(lir->jsop(), isSigned), output);
}

void
CodeGeneratorX64::visitCompareI64AndBranch(LCompareI64AndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register lhsReg = ToRegister64(lhs).reg;

    if (IsConstant(rhs)) {
        ImmWord imm = ImmWord(ToInt64(rhs));
        masm.cmpPtr(lhsReg, imm);
    } else {
        Register rhsReg = ToRegister64(rhs).reg;
        masm.cmpPtr(lhsReg, Operand(rhsReg));
    }

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    emitBranch(JSOpToCondition(lir->jsop(), isSigned), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitDivOrModI64(LDivOrModI64* lir)
{
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());
    Register output = ToRegister(lir->output());

    MOZ_ASSERT_IF(lhs != rhs, rhs != rax);
    MOZ_ASSERT(rhs != rdx);
    MOZ_ASSERT_IF(output == rax, ToRegister(lir->remainder()) == rdx);
    MOZ_ASSERT_IF(output == rdx, ToRegister(lir->remainder()) == rax);

    Label done;

    // Put the lhs in rax.
    if (lhs != rax)
        masm.mov(lhs, rax);

    // Handle divide by zero.
    if (lir->canBeDivideByZero()) {
        masm.testPtr(rhs, rhs);
        masm.j(Assembler::Zero, wasm::JumpTarget::IntegerDivideByZero);
    }

    // Handle an integer overflow exception from INT64_MIN / -1.
    if (lir->canBeNegativeOverflow()) {
        Label notmin;
        masm.branchPtr(Assembler::NotEqual, lhs, ImmWord(INT64_MIN), &notmin);
        masm.branchPtr(Assembler::NotEqual, rhs, ImmWord(-1), &notmin);
        if (lir->mir()->isMod())
            masm.xorl(output, output);
        else
            masm.jump(wasm::JumpTarget::IntegerOverflow);
        masm.jump(&done);
        masm.bind(&notmin);
    }

    // Sign extend the lhs into rdx to make rdx:rax.
    masm.cqo();
    masm.idivq(rhs);

    masm.bind(&done);
}

void
CodeGeneratorX64::visitUDivOrModI64(LUDivOrModI64* lir)
{
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());

    DebugOnly<Register> output = ToRegister(lir->output());
    MOZ_ASSERT_IF(lhs != rhs, rhs != rax);
    MOZ_ASSERT(rhs != rdx);
    MOZ_ASSERT_IF(output.value == rax, ToRegister(lir->remainder()) == rdx);
    MOZ_ASSERT_IF(output.value == rdx, ToRegister(lir->remainder()) == rax);

    // Put the lhs in rax.
    if (lhs != rax)
        masm.mov(lhs, rax);

    Label done;

    // Prevent divide by zero.
    if (lir->canBeDivideByZero()) {
        masm.testPtr(rhs, rhs);
        masm.j(Assembler::Zero, wasm::JumpTarget::IntegerDivideByZero);
    }

    // Zero extend the lhs into rdx to make (rdx:rax).
    masm.xorl(rdx, rdx);
    masm.udivq(rhs);

    masm.bind(&done);
}

void
CodeGeneratorX64::visitAsmSelectI64(LAsmSelectI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);

    Register cond = ToRegister(lir->condExpr());

    Operand falseExpr = ToOperandOrRegister64(lir->falseExpr());

    Register64 out = ToOutRegister64(lir);
    MOZ_ASSERT(ToRegister64(lir->trueExpr()) == out, "true expr is reused for input");

    masm.test32(cond, cond);
    masm.cmovzq(falseExpr, out.reg);
}

void
CodeGeneratorX64::visitAsmReinterpretFromI64(LAsmReinterpretFromI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
    masm.vmovq(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitAsmReinterpretToI64(LAsmReinterpretToI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
    masm.vmovq(ToFloatRegister(lir->input()), ToRegister(lir->output()));
}

void
CodeGeneratorX64::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble* lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32* lir)
{
    masm.convertUInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorX64::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorX64::visitWasmCall(LWasmCall* ins)
{
    emitWasmCallBase(ins);
}

void
CodeGeneratorX64::visitWasmCallI64(LWasmCallI64* ins)
{
    emitWasmCallBase(ins);
}

void
CodeGeneratorX64::memoryBarrier(MemoryBarrierBits barrier)
{
    if (barrier & MembarStoreLoad)
        masm.storeLoadFence();
}

void
CodeGeneratorX64::loadSimd(Scalar::Type type, unsigned numElems, const Operand& srcAddr,
                           FloatRegister out)
{
    switch (type) {
      case Scalar::Float32x4: {
        switch (numElems) {
          // In memory-to-register mode, movss zeroes out the high lanes.
          case 1: masm.loadFloat32(srcAddr, out); break;
          // See comment above, which also applies to movsd.
          case 2: masm.loadDouble(srcAddr, out); break;
          case 4: masm.loadUnalignedSimd128Float(srcAddr, out); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int32x4: {
        switch (numElems) {
          // In memory-to-register mode, movd zeroes out the high lanes.
          case 1: masm.vmovd(srcAddr, out); break;
          // See comment above, which also applies to movq.
          case 2: masm.vmovq(srcAddr, out); break;
          case 4: masm.loadUnalignedSimd128Int(srcAddr, out); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8x16:
        MOZ_ASSERT(numElems == 16, "unexpected partial load");
        masm.loadUnalignedSimd128Int(srcAddr, out);
        break;
      case Scalar::Int16x8:
        MOZ_ASSERT(numElems == 8, "unexpected partial load");
        masm.loadUnalignedSimd128Int(srcAddr, out);
        break;
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::Int64:
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("should only handle SIMD types");
    }
}

static wasm::MemoryAccess
AsmJSMemoryAccess(uint32_t before, wasm::MemoryAccess::OutOfBoundsBehavior throwBehavior,
                  uint32_t offsetWithinWholeSimdVector = 0)
{
    return wasm::MemoryAccess(before, throwBehavior, wasm::MemoryAccess::WrapOffset,
                              offsetWithinWholeSimdVector);
}

static wasm::MemoryAccess
WasmMemoryAccess(uint32_t before)
{
    return wasm::MemoryAccess(before,
                              wasm::MemoryAccess::Throw,
                              wasm::MemoryAccess::DontWrapOffset);
}

void
CodeGeneratorX64::load(Scalar::Type type, const Operand& srcAddr, AnyRegister out)
{
    switch (type) {
      case Scalar::Int8:      masm.movsbl(srcAddr, out.gpr()); break;
      case Scalar::Uint8:     masm.movzbl(srcAddr, out.gpr()); break;
      case Scalar::Int16:     masm.movswl(srcAddr, out.gpr()); break;
      case Scalar::Uint16:    masm.movzwl(srcAddr, out.gpr()); break;
      case Scalar::Int32:
      case Scalar::Uint32:    masm.movl(srcAddr, out.gpr()); break;
      case Scalar::Float32:   masm.loadFloat32(srcAddr, out.fpu()); break;
      case Scalar::Float64:   masm.loadDouble(srcAddr, out.fpu()); break;
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
        MOZ_CRASH("SIMD loads should be handled in emitSimdLoad");
      case Scalar::Int64:
        MOZ_CRASH("int64 loads must use load64");
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("unexpected array type");
    }
}

void
CodeGeneratorX64::loadI64(Scalar::Type type, const Operand& srcAddr, Register64 out)
{
    switch (type) {
      case Scalar::Int8:      masm.movsbq(srcAddr, out.reg); break;
      case Scalar::Uint8:     masm.movzbq(srcAddr, out.reg); break;
      case Scalar::Int16:     masm.movswq(srcAddr, out.reg); break;
      case Scalar::Uint16:    masm.movzwq(srcAddr, out.reg); break;
      case Scalar::Int32:     masm.movslq(srcAddr, out.reg); break;
      // Int32 to int64 moves zero-extend by default.
      case Scalar::Uint32:    masm.movl(srcAddr, out.reg);   break;
      case Scalar::Int64:     masm.movq(srcAddr, out.reg);   break;
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
        MOZ_CRASH("non-int64 loads should use load()");
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("unexpected array type");
    }
}

template <typename T>
void
CodeGeneratorX64::emitWasmLoad(T* ins)
{
    const MWasmLoad* mir = ins->mir();
    bool isInt64 = mir->type() == MIRType::Int64;

    Scalar::Type accessType = mir->accessType();
    MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD NYI");
    MOZ_ASSERT(!mir->barrierBefore() && !mir->barrierAfter(), "atomics NYI");

    if (mir->offset() > INT32_MAX) {
        masm.jump(wasm::JumpTarget::OutOfBounds);
        return;
    }

    const LAllocation* ptr = ins->ptr();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    uint32_t before = masm.size();
    if (isInt64)
        loadI64(accessType, srcAddr, ToOutRegister64(ins));
    else
        load(accessType, srcAddr, ToAnyRegister(ins->output()));
    uint32_t after = masm.size();

    verifyLoadDisassembly(before, after, isInt64, accessType, /* numElems */ 0, srcAddr,
                          *ins->output()->output());

    masm.append(WasmMemoryAccess(before));
}

void
CodeGeneratorX64::visitWasmLoad(LWasmLoad* ins)
{
    emitWasmLoad(ins);
}

void
CodeGeneratorX64::visitWasmLoadI64(LWasmLoadI64* ins)
{
    emitWasmLoad(ins);
}

template <typename T>
void
CodeGeneratorX64::emitWasmStore(T* ins)
{
    const MWasmStore* mir = ins->mir();

    Scalar::Type accessType = mir->accessType();
    MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD NYI");
    MOZ_ASSERT(!mir->barrierBefore() && !mir->barrierAfter(), "atomics NYI");

    if (mir->offset() > INT32_MAX) {
        masm.jump(wasm::JumpTarget::OutOfBounds);
        return;
    }

    const LAllocation* value = ins->getOperand(ins->ValueIndex);
    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    uint32_t before = masm.size();
    store(accessType, value, dstAddr);
    uint32_t after = masm.size();

    verifyStoreDisassembly(before, after, mir->value()->type() == MIRType::Int64,
                           accessType, /* numElems */ 0, dstAddr, *value);

    masm.append(WasmMemoryAccess(before));
}

void
CodeGeneratorX64::visitWasmStore(LWasmStore* ins)
{
    emitWasmStore(ins);
}

void
CodeGeneratorX64::visitWasmStoreI64(LWasmStoreI64* ins)
{
    emitWasmStore(ins);
}

void
CodeGeneratorX64::emitSimdLoad(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    Scalar::Type type = mir->accessType();
    FloatRegister out = ToFloatRegister(ins->output());
    const LAllocation* ptr = ins->ptr();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    bool hasBoundsCheck = maybeEmitThrowingAsmJSBoundsCheck(mir, mir, ptr);

    unsigned numElems = mir->numSimdElems();
    if (numElems == 3) {
        MOZ_ASSERT(type == Scalar::Int32x4 || type == Scalar::Float32x4);

        Operand srcAddrZ =
            ptr->isBogus()
            ? Operand(HeapReg, 2 * sizeof(float) + mir->offset())
            : Operand(HeapReg, ToRegister(ptr), TimesOne, 2 * sizeof(float) + mir->offset());

        // Load XY
        uint32_t before = masm.size();
        loadSimd(type, 2, srcAddr, out);
        uint32_t after = masm.size();
        verifyLoadDisassembly(before, after, /* isInt64 */ false, type, 2, srcAddr,
                              *ins->output()->output());
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw));

        // Load Z (W is zeroed)
        // This is still in bounds, as we've checked with a manual bounds check
        // or we had enough space for sure when removing the bounds check.
        before = after;
        loadSimd(type, 1, srcAddrZ, ScratchSimd128Reg);
        after = masm.size();
        verifyLoadDisassembly(before, after, /* isInt64 */ false, type, 1, srcAddrZ,
                              LFloatReg(ScratchSimd128Reg));
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw, 8));

        // Move ZW atop XY
        masm.vmovlhps(ScratchSimd128Reg, out, out);
    } else {
        uint32_t before = masm.size();
        loadSimd(type, numElems, srcAddr, out);
        uint32_t after = masm.size();
        verifyLoadDisassembly(before, after, /* isInt64 */ true, type, numElems, srcAddr,
                              *ins->output()->output());
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw));
    }

    if (hasBoundsCheck)
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
}

void
CodeGeneratorX64::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();

    if (Scalar::isSimdType(accessType))
        return emitSimdLoad(ins);

    const LAllocation* ptr = ins->ptr();
    const LDefinition* out = ins->output();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    memoryBarrier(mir->barrierBefore());

    OutOfLineLoadTypedArrayOutOfBounds* ool;
    DebugOnly<bool> hasBoundsCheck = maybeEmitAsmJSLoadBoundsCheck(mir, ins, &ool);

    uint32_t before = masm.size();
    load(accessType, srcAddr, ToAnyRegister(out));
    uint32_t after = masm.size();

    verifyLoadDisassembly(before, after, /* isInt64 */ false, accessType, 0, srcAddr,
                          *out->output());

    if (ool) {
        MOZ_ASSERT(hasBoundsCheck);
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(ool->rejoin());
    }

    memoryBarrier(mir->barrierAfter());

    masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::CarryOn));
}

void
CodeGeneratorX64::store(Scalar::Type type, const LAllocation* value, const Operand& dstAddr)
{
    if (value->isConstant()) {
        const MConstant* mir = value->toConstant();
        Imm32 cst = Imm32(mir->type() == MIRType::Int32 ? mir->toInt32() : mir->toInt64());
        switch (type) {
          case Scalar::Int8:
          case Scalar::Uint8:        masm.movb(cst, dstAddr); break;
          case Scalar::Int16:
          case Scalar::Uint16:       masm.movw(cst, dstAddr); break;
          case Scalar::Int32:
          case Scalar::Uint32:       masm.movl(cst, dstAddr); break;
          case Scalar::Int64:
          case Scalar::Float32:
          case Scalar::Float64:
          case Scalar::Float32x4:
          case Scalar::Int8x16:
          case Scalar::Int16x8:
          case Scalar::Int32x4:
          case Scalar::Uint8Clamped:
          case Scalar::MaxTypedArrayViewType:
            MOZ_CRASH("unexpected array type");
        }
    } else {
        switch (type) {
          case Scalar::Int8:
          case Scalar::Uint8:
            masm.movb(ToRegister(value), dstAddr);
            break;
          case Scalar::Int16:
          case Scalar::Uint16:
            masm.movw(ToRegister(value), dstAddr);
            break;
          case Scalar::Int32:
          case Scalar::Uint32:
            masm.movl(ToRegister(value), dstAddr);
            break;
          case Scalar::Int64:
            masm.movq(ToRegister(value), dstAddr);
            break;
          case Scalar::Float32:
            masm.storeUncanonicalizedFloat32(ToFloatRegister(value), dstAddr);
            break;
          case Scalar::Float64:
            masm.storeUncanonicalizedDouble(ToFloatRegister(value), dstAddr);
            break;
          case Scalar::Float32x4:
          case Scalar::Int8x16:
          case Scalar::Int16x8:
          case Scalar::Int32x4:
            MOZ_CRASH("SIMD stores must be handled in emitSimdStore");
          case Scalar::Uint8Clamped:
          case Scalar::MaxTypedArrayViewType:
            MOZ_CRASH("unexpected array type");
        }
    }
}

void
CodeGeneratorX64::storeSimd(Scalar::Type type, unsigned numElems, FloatRegister in,
                            const Operand& dstAddr)
{
    switch (type) {
      case Scalar::Float32x4: {
        switch (numElems) {
          // In memory-to-register mode, movss zeroes out the high lanes.
          case 1: masm.storeUncanonicalizedFloat32(in, dstAddr); break;
          // See comment above, which also applies to movsd.
          case 2: masm.storeUncanonicalizedDouble(in, dstAddr); break;
          case 4: masm.storeUnalignedSimd128Float(in, dstAddr); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int32x4: {
        switch (numElems) {
          // In memory-to-register mode, movd zeroes out the high lanes.
          case 1: masm.vmovd(in, dstAddr); break;
          // See comment above, which also applies to movq.
          case 2: masm.vmovq(in, dstAddr); break;
          case 4: masm.storeUnalignedSimd128Int(in, dstAddr); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8x16:
        MOZ_ASSERT(numElems == 16, "unexpected partial store");
        masm.storeUnalignedSimd128Int(in, dstAddr);
        break;
      case Scalar::Int16x8:
        MOZ_ASSERT(numElems == 8, "unexpected partial store");
        masm.storeUnalignedSimd128Int(in, dstAddr);
        break;
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::Int64:
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("should only handle SIMD types");
    }
}

void
CodeGeneratorX64::emitSimdStore(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    Scalar::Type type = mir->accessType();
    FloatRegister in = ToFloatRegister(ins->value());
    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    bool hasBoundsCheck = maybeEmitThrowingAsmJSBoundsCheck(mir, mir, ptr);

    unsigned numElems = mir->numSimdElems();
    if (numElems == 3) {
        MOZ_ASSERT(type == Scalar::Int32x4 || type == Scalar::Float32x4);

        Operand dstAddrZ =
            ptr->isBogus()
            ? Operand(HeapReg, 2 * sizeof(float) + mir->offset())
            : Operand(HeapReg, ToRegister(ptr), TimesOne, 2 * sizeof(float) + mir->offset());

        // It's possible that the Z could be out of bounds when the XY is in
        // bounds. To avoid storing the XY before the exception is thrown, we
        // store the Z first, and record its offset in the MemoryAccess so
        // that the signal handler knows to check the bounds of the full
        // access, rather than just the Z.
        masm.vmovhlps(in, ScratchSimd128Reg, ScratchSimd128Reg);
        uint32_t before = masm.size();
        storeSimd(type, 1, ScratchSimd128Reg, dstAddrZ);
        uint32_t after = masm.size();
        verifyStoreDisassembly(before, after, /* int64 */ false, type, 1, dstAddrZ,
                               LFloatReg(ScratchSimd128Reg));
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw, 8));

        // Store XY
        before = after;
        storeSimd(type, 2, in, dstAddr);
        after = masm.size();
        verifyStoreDisassembly(before, after, /* int64 */ false, type, 2, dstAddr,
                               *ins->value());
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw));
    } else {
        uint32_t before = masm.size();
        storeSimd(type, numElems, in, dstAddr);
        uint32_t after = masm.size();
        verifyStoreDisassembly(before, after, /* int64 */ false, type, numElems, dstAddr,
                               *ins->value());
        masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::Throw));
    }

    if (hasBoundsCheck)
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
}

void
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    const LAllocation* value = ins->value();

    canonicalizeIfDeterministic(accessType, value);

    if (Scalar::isSimdType(accessType))
        return emitSimdStore(ins);

    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    memoryBarrier(mir->barrierBefore());

    Label* rejoin;
    DebugOnly<bool> hasBoundsCheck = maybeEmitAsmJSStoreBoundsCheck(mir, ins, &rejoin);

    uint32_t before = masm.size();
    store(accessType, value, dstAddr);
    uint32_t after = masm.size();

    verifyStoreDisassembly(before, after, /* int64 */ false, accessType, 0, dstAddr, *value);

    if (rejoin) {
        MOZ_ASSERT(hasBoundsCheck);
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(rejoin);
    }

    memoryBarrier(mir->barrierAfter());

    masm.append(AsmJSMemoryAccess(before, wasm::MemoryAccess::CarryOn));
}

void
CodeGeneratorX64::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins)
{
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    MAsmJSCompareExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();

    Register ptr = ToRegister(ins->ptr());
    BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->offset());
    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());

    // Note that we can't use the same machinery as normal asm.js loads/stores
    // since signal-handler bounds checking is not yet implemented for atomic
    // accesses.
    maybeEmitWasmBoundsCheckBranch(mir, ptr);

    masm.compareExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        srcAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
}

void
CodeGeneratorX64::visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins)
{
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());
    MOZ_ASSERT(ins->mir()->accessType() <= Scalar::Uint32);

    MAsmJSAtomicExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();

    Register ptr = ToRegister(ins->ptr());
    BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->offset());
    Register value = ToRegister(ins->value());

    // See comment in visitAsmJSCompareExchangeHeap.
    maybeEmitWasmBoundsCheckBranch(mir, ptr);

    masm.atomicExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                       srcAddr,
                                       value,
                                       InvalidReg,
                                       ToAnyRegister(ins->output()));
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins)
{
    MOZ_ASSERT(ins->mir()->hasUses());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    accessType = accessType == Scalar::Uint32 ? Scalar::Int32 : accessType;
    AtomicOp op = mir->operation();

    Register ptr = ToRegister(ins->ptr());
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->offset());

    const LAllocation* value = ins->value();

    // See comment in visitAsmJSCompareExchangeHeap.
    maybeEmitWasmBoundsCheckBranch(mir, ptr);

    AnyRegister output = ToAnyRegister(ins->output());
    if (value->isConstant()) {
        atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), srcAddr, temp, InvalidReg,
                                   output);
    } else {
        atomicBinopToTypedIntArray(op, accessType, ToRegister(value), srcAddr, temp, InvalidReg,
                                   output);
    }

    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins)
{
    MOZ_ASSERT(!ins->mir()->hasUses());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    AtomicOp op = mir->operation();

    Register ptr = ToRegister(ins->ptr());
    BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->offset());
    const LAllocation* value = ins->value();

    // See comment in visitAsmJSCompareExchangeHeap.
    maybeEmitWasmBoundsCheckBranch(mir, ptr);

    if (value->isConstant())
        atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), srcAddr);
    else
        atomicBinopToTypedIntArray(op, accessType, ToRegister(value), srcAddr);
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
}

void
CodeGeneratorX64::visitWasmLoadGlobalVar(LWasmLoadGlobalVar* ins)
{
    MWasmLoadGlobalVar* mir = ins->mir();

    MIRType type = mir->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffset label;
    switch (type) {
      case MIRType::Int32:
        label = masm.loadRipRelativeInt32(ToRegister(ins->output()));
        break;
      case MIRType::Float32:
        label = masm.loadRipRelativeFloat32(ToFloatRegister(ins->output()));
        break;
      case MIRType::Double:
        label = masm.loadRipRelativeDouble(ToFloatRegister(ins->output()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType::Int8x16:
      case MIRType::Int16x8:
      case MIRType::Int32x4:
      case MIRType::Bool8x16:
      case MIRType::Bool16x8:
      case MIRType::Bool32x4:
        label = masm.loadRipRelativeInt32x4(ToFloatRegister(ins->output()));
        break;
      case MIRType::Float32x4:
        label = masm.loadRipRelativeFloat32x4(ToFloatRegister(ins->output()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitWasmLoadGlobalVar");
    }

    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitWasmLoadGlobalVarI64(LWasmLoadGlobalVarI64* ins)
{
    MWasmLoadGlobalVar* mir = ins->mir();
    MOZ_ASSERT(mir->type() == MIRType::Int64);
    CodeOffset label = masm.loadRipRelativeInt64(ToRegister(ins->output()));
    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitWasmStoreGlobalVar(LWasmStoreGlobalVar* ins)
{
    MWasmStoreGlobalVar* mir = ins->mir();

    MIRType type = mir->value()->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffset label;
    switch (type) {
      case MIRType::Int32:
        label = masm.storeRipRelativeInt32(ToRegister(ins->value()));
        break;
      case MIRType::Float32:
        label = masm.storeRipRelativeFloat32(ToFloatRegister(ins->value()));
        break;
      case MIRType::Double:
        label = masm.storeRipRelativeDouble(ToFloatRegister(ins->value()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType::Int32x4:
      case MIRType::Bool32x4:
        label = masm.storeRipRelativeInt32x4(ToFloatRegister(ins->value()));
        break;
      case MIRType::Float32x4:
        label = masm.storeRipRelativeFloat32x4(ToFloatRegister(ins->value()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitWasmStoreGlobalVar");
    }

    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitWasmStoreGlobalVarI64(LWasmStoreGlobalVarI64* ins)
{
    MWasmStoreGlobalVar* mir = ins->mir();
    MOZ_ASSERT(mir->value()->type() == MIRType::Int64);
    Register value = ToRegister(ins->getOperand(LWasmStoreGlobalVarI64::InputIndex));
    CodeOffset label = masm.storeRipRelativeInt64(value);
    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitTruncateDToInt32(LTruncateDToInt32* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateDouble uses vcvttsd2sq. Unlike the x86
    // implementation, this should handle most doubles and we can just
    // call a stub if it fails.
    emitTruncateDouble(input, output, ins->mir());
}

void
CodeGeneratorX64::visitTruncateFToInt32(LTruncateFToInt32* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateFloat32 uses vcvttss2sq. Unlike the x86
    // implementation, this should handle most floats and we can just
    // call a stub if it fails.
    emitTruncateFloat32(input, output, ins->mir());
}

void
CodeGeneratorX64::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir)
{
    const LAllocation* input = lir->getOperand(0);
    Register output = ToRegister(lir->output());

    if (lir->mir()->bottomHalf())
        masm.movl(ToOperand(input), output);
    else
        MOZ_CRASH("Not implemented.");
}

void
CodeGeneratorX64::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir)
{
    const LAllocation* input = lir->getOperand(0);
    Register output = ToRegister(lir->output());

    if (lir->mir()->isUnsigned())
        masm.movl(ToOperand(input), output);
    else
        masm.movslq(ToOperand(input), output);
}

void
CodeGeneratorX64::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());

    MWasmTruncateToInt64* mir = lir->mir();
    MIRType inputType = mir->input()->type();

    MOZ_ASSERT(inputType == MIRType::Double || inputType == MIRType::Float32);

    auto* ool = new(alloc()) OutOfLineWasmTruncateCheck(mir, input);
    addOutOfLineCode(ool, mir);

    FloatRegister temp = mir->isUnsigned() ? ToFloatRegister(lir->temp()) : InvalidFloatReg;

    Label* oolEntry = ool->entry();
    Label* oolRejoin = ool->rejoin();
    if (inputType == MIRType::Double) {
        if (mir->isUnsigned())
            masm.wasmTruncateDoubleToUInt64(input, output, oolEntry, oolRejoin, temp);
        else
            masm.wasmTruncateDoubleToInt64(input, output, oolEntry, oolRejoin, temp);
    } else {
        if (mir->isUnsigned())
            masm.wasmTruncateFloat32ToUInt64(input, output, oolEntry, oolRejoin, temp);
        else
            masm.wasmTruncateFloat32ToInt64(input, output, oolEntry, oolRejoin, temp);
    }

    masm.bind(ool->rejoin());
}

void
CodeGeneratorX64::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir)
{
    Register input = ToRegister(lir->input());
    FloatRegister output = ToFloatRegister(lir->output());

    MIRType outputType = lir->mir()->type();
    MOZ_ASSERT(outputType == MIRType::Double || outputType == MIRType::Float32);

    if (outputType == MIRType::Double) {
        if (lir->mir()->isUnsigned())
            masm.convertUInt64ToDouble(input, output);
        else
            masm.convertInt64ToDouble(input, output);
    } else {
        if (lir->mir()->isUnsigned())
            masm.convertUInt64ToFloat32(input, output);
        else
            masm.convertInt64ToFloat32(input, output);
    }
}

void
CodeGeneratorX64::visitNotI64(LNotI64* lir)
{
    masm.cmpq(Imm32(0), ToRegister(lir->input()));
    masm.emitSet(Assembler::Equal, ToRegister(lir->output()));
}

void
CodeGeneratorX64::visitClzI64(LClzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);
    masm.clz64(input, output.reg);
}

void
CodeGeneratorX64::visitCtzI64(LCtzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);
    masm.ctz64(input, output.reg);
}

void
CodeGeneratorX64::visitTestI64AndBranch(LTestI64AndBranch* lir)
{
    Register input = ToRegister(lir->input());
    masm.testq(input, input);
    emitBranch(Assembler::NonZero, lir->ifTrue(), lir->ifFalse());
}
