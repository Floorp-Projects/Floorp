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

Operand
CodeGeneratorX64::ToOperand64(const LInt64Allocation& a64)
{
    const LAllocation& a = a64.value();
    MOZ_ASSERT(!a.isFloatReg());
    if (a.isGeneralReg())
        return Operand(a.toGeneralReg()->reg());
    return Operand(masm.getStackPointer(), ToStackOffset(a));
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

    if (IsConstant(rhs))
        masm.cmpPtr(lhsReg, ImmWord(ToInt64(rhs)));
    else
        masm.cmpPtr(lhsReg, ToOperand64(rhs));

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    masm.emitSet(JSOpToCondition(lir->jsop(), isSigned), output);
}

void
CodeGeneratorX64::visitCompareI64AndBranch(LCompareI64AndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register lhsReg = ToRegister64(lhs).reg;

    if (IsConstant(rhs))
        masm.cmpPtr(lhsReg, ImmWord(ToInt64(rhs)));
    else
        masm.cmpPtr(lhsReg, ToOperand64(rhs));

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
        masm.branchTestPtr(Assembler::Zero, rhs, rhs, trap(lir, wasm::Trap::IntegerDivideByZero));
    }

    // Handle an integer overflow exception from INT64_MIN / -1.
    if (lir->canBeNegativeOverflow()) {
        Label notmin;
        masm.branchPtr(Assembler::NotEqual, lhs, ImmWord(INT64_MIN), &notmin);
        masm.branchPtr(Assembler::NotEqual, rhs, ImmWord(-1), &notmin);
        if (lir->mir()->isMod())
            masm.xorl(output, output);
        else
            masm.jump(trap(lir, wasm::Trap::IntegerOverflow));
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
    if (lir->canBeDivideByZero())
        masm.branchTestPtr(Assembler::Zero, rhs, rhs, trap(lir, wasm::Trap::IntegerDivideByZero));

    // Zero extend the lhs into rdx to make (rdx:rax).
    masm.xorl(rdx, rdx);
    masm.udivq(rhs);

    masm.bind(&done);
}

void
CodeGeneratorX64::visitWasmSelectI64(LWasmSelectI64* lir)
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
CodeGeneratorX64::visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
    masm.vmovq(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitWasmReinterpretToI64(LWasmReinterpretToI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
    masm.vmovq(ToFloatRegister(lir->input()), ToRegister(lir->output()));
}

void
CodeGeneratorX64::visitWasmUint32ToDouble(LWasmUint32ToDouble* lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir)
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
CodeGeneratorX64::wasmStore(const wasm::MemoryAccessDesc& access, const LAllocation* value,
                            Operand dstAddr)
{
    if (value->isConstant()) {
        MOZ_ASSERT(!access.isSimd());

        masm.memoryBarrier(access.barrierBefore());

        const MConstant* mir = value->toConstant();
        Imm32 cst = Imm32(mir->type() == MIRType::Int32 ? mir->toInt32() : mir->toInt64());

        size_t storeOffset = masm.size();
        switch (access.type()) {
          case Scalar::Int8:
          case Scalar::Uint8:
            masm.movb(cst, dstAddr);
            break;
          case Scalar::Int16:
          case Scalar::Uint16:
            masm.movw(cst, dstAddr);
            break;
          case Scalar::Int32:
          case Scalar::Uint32:
            masm.movl(cst, dstAddr);
            break;
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
        masm.append(access, storeOffset, masm.framePushed());

        masm.memoryBarrier(access.barrierAfter());
    } else {
        masm.wasmStore(access, ToAnyRegister(value), dstAddr);
    }
}

template <typename T>
void
CodeGeneratorX64::emitWasmLoad(T* ins)
{
    const MWasmLoad* mir = ins->mir();

    uint32_t offset = mir->access().offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    const LAllocation* ptr = ins->ptr();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(HeapReg, offset)
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, offset);

    if (mir->type() == MIRType::Int64)
        masm.wasmLoadI64(mir->access(), srcAddr, ToOutRegister64(ins));
    else
        masm.wasmLoad(mir->access(), srcAddr, ToAnyRegister(ins->output()));
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

    uint32_t offset = mir->access().offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    const LAllocation* value = ins->getOperand(ins->ValueIndex);
    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, offset)
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, offset);

    wasmStore(mir->access(), value, dstAddr);
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
CodeGeneratorX64::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() < wasm::OffsetGuardLimit);

    const LAllocation* ptr = ins->ptr();
    const LDefinition* out = ins->output();

    Scalar::Type accessType = mir->access().type();
    MOZ_ASSERT(!Scalar::isSimdType(accessType));

    Operand srcAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    uint32_t before = masm.size();
    masm.wasmLoad(mir->access(), srcAddr, ToAnyRegister(out));
    uint32_t after = masm.size();
    verifyLoadDisassembly(before, after, accessType, srcAddr, *out->output());
}

void
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    MOZ_ASSERT(mir->offset() < wasm::OffsetGuardLimit);

    const LAllocation* ptr = ins->ptr();
    const LAllocation* value = ins->value();

    Scalar::Type accessType = mir->access().type();
    MOZ_ASSERT(!Scalar::isSimdType(accessType));

    canonicalizeIfDeterministic(accessType, value);

    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    uint32_t before = masm.size();
    wasmStore(mir->access(), value, dstAddr);
    uint32_t after = masm.size();
    verifyStoreDisassembly(before, after, accessType, dstAddr, *value);
}

void
CodeGeneratorX64::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins)
{
    MAsmJSCompareExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->access().offset() == 0);

    Register ptr = ToRegister(ins->ptr());
    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    Scalar::Type accessType = mir->access().type();
    BaseIndex srcAddr(HeapReg, ptr, TimesOne);

    masm.compareExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        srcAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
}

void
CodeGeneratorX64::visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins)
{
    MAsmJSAtomicExchangeHeap* mir = ins->mir();
    MOZ_ASSERT(mir->access().offset() == 0);

    Register ptr = ToRegister(ins->ptr());
    Register value = ToRegister(ins->value());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    Scalar::Type accessType = mir->access().type();
    MOZ_ASSERT(accessType <= Scalar::Uint32);

    BaseIndex srcAddr(HeapReg, ptr, TimesOne);

    masm.atomicExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                       srcAddr,
                                       value,
                                       InvalidReg,
                                       ToAnyRegister(ins->output()));
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    MOZ_ASSERT(mir->access().offset() == 0);
    MOZ_ASSERT(mir->hasUses());

    Register ptr = ToRegister(ins->ptr());
    const LAllocation* value = ins->value();
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    AnyRegister output = ToAnyRegister(ins->output());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    Scalar::Type accessType = mir->access().type();
    if (accessType == Scalar::Uint32)
        accessType = Scalar::Int32;

    AtomicOp op = mir->operation();
    BaseIndex srcAddr(HeapReg, ptr, TimesOne);

    if (value->isConstant()) {
        atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), srcAddr, temp, InvalidReg,
                                   output);
    } else {
        atomicBinopToTypedIntArray(op, accessType, ToRegister(value), srcAddr, temp, InvalidReg,
                                   output);
    }
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    MOZ_ASSERT(mir->access().offset() == 0);
    MOZ_ASSERT(!mir->hasUses());

    Register ptr = ToRegister(ins->ptr());
    const LAllocation* value = ins->value();
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    Scalar::Type accessType = mir->access().type();
    AtomicOp op = mir->operation();

    BaseIndex srcAddr(HeapReg, ptr, TimesOne);

    if (value->isConstant())
        atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), srcAddr);
    else
        atomicBinopToTypedIntArray(op, accessType, ToRegister(value), srcAddr);
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
    Register64 output = ToOutRegister64(lir);

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
}

void
CodeGeneratorX64::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    MIRType outputType = lir->mir()->type();
    MOZ_ASSERT(outputType == MIRType::Double || outputType == MIRType::Float32);

    if (outputType == MIRType::Double) {
        if (lir->mir()->isUnsigned())
            masm.convertUInt64ToDouble(input, output, Register::Invalid());
        else
            masm.convertInt64ToDouble(input, output);
    } else {
        if (lir->mir()->isUnsigned())
            masm.convertUInt64ToFloat32(input, output, Register::Invalid());
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
