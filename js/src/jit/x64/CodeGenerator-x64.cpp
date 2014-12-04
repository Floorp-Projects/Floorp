/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/CodeGenerator-x64.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"

#include "jsscriptinlines.h"

#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

CodeGeneratorX64::CodeGeneratorX64(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorX86Shared(gen, graph, masm)
{
}

ValueOperand
CodeGeneratorX64::ToValue(LInstruction *ins, size_t pos)
{
    return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand
CodeGeneratorX64::ToOutValue(LInstruction *ins)
{
    return ValueOperand(ToRegister(ins->getDef(0)));
}

ValueOperand
CodeGeneratorX64::ToTempValue(LInstruction *ins, size_t pos)
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
CodeGeneratorX64::visitValue(LValue *value)
{
    LDefinition *reg = value->getDef(0);
    masm.moveValue(value->value(), ToRegister(reg));
}

void
CodeGeneratorX64::visitBox(LBox *box)
{
    const LAllocation *in = box->getOperand(0);
    const LDefinition *result = box->getDef(0);

    if (IsFloatingPointType(box->type())) {
        FloatRegister reg = ToFloatRegister(in);
        if (box->type() == MIRType_Float32) {
            masm.convertFloat32ToDouble(reg, ScratchDoubleReg);
            reg = ScratchDoubleReg;
        }
        masm.movq(reg, ToRegister(result));
    } else {
        masm.boxValue(ValueTypeFromMIRType(box->type()), ToRegister(in), ToRegister(result));
    }
}

void
CodeGeneratorX64::visitUnbox(LUnbox *unbox)
{
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
        const ValueOperand value = ToValue(unbox, LUnbox::Input);
        Assembler::Condition cond;
        switch (mir->type()) {
          case MIRType_Int32:
            cond = masm.testInt32(Assembler::NotEqual, value);
            break;
          case MIRType_Boolean:
            cond = masm.testBoolean(Assembler::NotEqual, value);
            break;
          case MIRType_Object:
            cond = masm.testObject(Assembler::NotEqual, value);
            break;
          case MIRType_String:
            cond = masm.testString(Assembler::NotEqual, value);
            break;
          case MIRType_Symbol:
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
      case MIRType_Int32:
        masm.unboxInt32(input, result);
        break;
      case MIRType_Boolean:
        masm.unboxBoolean(input, result);
        break;
      case MIRType_Object:
        masm.unboxObject(input, result);
        break;
      case MIRType_String:
        masm.unboxString(input, result);
        break;
      case MIRType_Symbol:
        masm.unboxSymbol(input, result);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
}

void
CodeGeneratorX64::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    if (rhs->isConstant())
        masm.moveValue(*rhs->toConstant(), ScratchReg);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), ScratchReg);

    // Perform the comparison.
    masm.cmpq(lhs.valueReg(), ScratchReg);
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
}

void
CodeGeneratorX64::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    if (rhs->isConstant())
        masm.moveValue(*rhs->toConstant(), ScratchReg);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), ScratchReg);

    // Perform the comparison.
    masm.cmpq(lhs.valueReg(), ScratchReg);
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    masm.cmpq(lhs.valueReg(), rhs.valueReg());
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
}

void
CodeGeneratorX64::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    masm.cmpq(lhs.valueReg(), rhs.valueReg());
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir)
{
    masm.convertUInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void
CodeGeneratorX64::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorX64::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MOZ_CRASH("NYI");
}

void
CodeGeneratorX64::visitAsmJSCall(LAsmJSCall *ins)
{
    emitAsmJSCall(ins);

#ifdef DEBUG
    Register scratch = ABIArgGenerator::NonReturn_VolatileReg0;
    masm.movePtr(HeapReg, scratch);
    masm.loadAsmJSHeapRegisterFromGlobalData();
    Label ok;
    masm.branchPtr(Assembler::Equal, HeapReg, scratch, &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif
}

void
CodeGeneratorX64::memoryBarrier(MemoryBarrierBits barrier)
{
    if (barrier & MembarStoreLoad)
        masm.storeLoadFence();
}

void
CodeGeneratorX64::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    MAsmJSLoadHeap *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    const LDefinition *out = ins->output();
    Operand srcAddr(HeapReg);

    if (ptr->isConstant()) {
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);
        srcAddr = Operand(HeapReg, ptrImm);
    } else {
        srcAddr = Operand(HeapReg, ToRegister(ptr), TimesOne);
    }

    memoryBarrier(ins->mir()->barrierBefore());
    OutOfLineLoadTypedArrayOutOfBounds *ool = nullptr;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), vt);
        addOutOfLineCode(ool, ins->mir());

        CodeOffsetLabel cmp = masm.cmplWithPatch(ToRegister(ptr), Imm32(0));
        masm.j(Assembler::AboveOrEqual, ool->entry());
        maybeCmpOffset = cmp.offset();
    }

    uint32_t before = masm.size();
    switch (vt) {
      case AsmJSHeapAccess::Int8:      masm.movsbl(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Uint8:     masm.movzbl(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Int16:     masm.movswl(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Uint16:    masm.movzwl(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Int32:
      case AsmJSHeapAccess::Uint32:    masm.movl(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Float32:   masm.loadFloat32(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Float64:   masm.loadDouble(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Float32x4: masm.loadUnalignedFloat32x4(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Int32x4:   masm.loadUnalignedInt32x4(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Uint8Clamped: MOZ_CRASH("unexpected array type");
    }
    uint32_t after = masm.size();
    if (ool)
        masm.bind(ool->rejoin());
    memoryBarrier(ins->mir()->barrierAfter());
    masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out), maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    Operand dstAddr(HeapReg);

    if (ptr->isConstant()) {
        int32_t ptrImm = ptr->toConstant()->toInt32();
        MOZ_ASSERT(ptrImm >= 0);
        dstAddr = Operand(HeapReg, ptrImm);
    } else {
        dstAddr = Operand(HeapReg, ToRegister(ptr), TimesOne);
    }

    memoryBarrier(ins->mir()->barrierBefore());
    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        CodeOffsetLabel cmp = masm.cmplWithPatch(ToRegister(ptr), Imm32(0));
        masm.j(Assembler::AboveOrEqual, &rejoin);
        maybeCmpOffset = cmp.offset();
    }

    uint32_t before = masm.size();
    if (ins->value()->isConstant()) {
        switch (vt) {
          case AsmJSHeapAccess::Int8:
          case AsmJSHeapAccess::Uint8:        masm.movb(Imm32(ToInt32(ins->value())), dstAddr); break;
          case AsmJSHeapAccess::Int16:
          case AsmJSHeapAccess::Uint16:       masm.movw(Imm32(ToInt32(ins->value())), dstAddr); break;
          case AsmJSHeapAccess::Int32:
          case AsmJSHeapAccess::Uint32:       masm.movl(Imm32(ToInt32(ins->value())), dstAddr); break;
          case AsmJSHeapAccess::Float32:
          case AsmJSHeapAccess::Float64:
          case AsmJSHeapAccess::Float32x4:
          case AsmJSHeapAccess::Int32x4:
          case AsmJSHeapAccess::Uint8Clamped: MOZ_CRASH("unexpected array type");
        }
    } else {
        switch (vt) {
          case AsmJSHeapAccess::Int8:
          case AsmJSHeapAccess::Uint8:        masm.movb(ToRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Int16:
          case AsmJSHeapAccess::Uint16:       masm.movw(ToRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Int32:
          case AsmJSHeapAccess::Uint32:       masm.movl(ToRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Float32:      masm.storeFloat32(ToFloatRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Float64:      masm.storeDouble(ToFloatRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Float32x4:    masm.storeUnalignedFloat32x4(ToFloatRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Int32x4:      masm.storeUnalignedInt32x4(ToFloatRegister(ins->value()), dstAddr); break;
          case AsmJSHeapAccess::Uint8Clamped: MOZ_CRASH("unexpected array type");
        }
    }
    uint32_t after = masm.size();
    if (rejoin.used())
        masm.bind(&rejoin);
    memoryBarrier(ins->mir()->barrierAfter());
    masm.append(AsmJSHeapAccess(before, after, vt, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap *ins)
{
    MAsmJSCompareExchangeHeap *mir = ins->mir();

    MOZ_ASSERT(mir->viewType() <= AsmJSHeapAccess::Uint32);
    Scalar::Type vt = Scalar::Type(mir->viewType());

    const LAllocation *ptr = ins->ptr();

    MOZ_ASSERT(ptr->isRegister());
    BaseIndex srcAddr(HeapReg, ToRegister(ptr), TimesOne);

    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());

    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    MOZ_ASSERT(mir->needsBoundsCheck());
    {
        maybeCmpOffset = masm.cmplWithPatch(ToRegister(ptr), Imm32(0)).offset();
        Label goahead;
        masm.j(Assembler::LessThan, &goahead);
        memoryBarrier(MembarFull);
        Register out = ToRegister(ins->output());
        masm.xorl(out, out);
        masm.jmp(&rejoin);
        masm.bind(&goahead);
    }
    masm.compareExchangeToTypedIntArray(vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        srcAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    uint32_t after = masm.size();
    if (rejoin.used())
        masm.bind(&rejoin);
    masm.append(AsmJSHeapAccess(after, after, mir->viewType(), maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap *ins)
{
    MAsmJSAtomicBinopHeap *mir = ins->mir();

    MOZ_ASSERT(mir->viewType() <= AsmJSHeapAccess::Uint32);
    Scalar::Type vt = Scalar::Type(mir->viewType());

    const LAllocation *ptr = ins->ptr();
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    MOZ_ASSERT(ptr->isRegister());
    BaseIndex srcAddr(HeapReg, ToRegister(ptr), TimesOne);

    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    MOZ_ASSERT(mir->needsBoundsCheck());
    {
        maybeCmpOffset = masm.cmplWithPatch(ToRegister(ptr), Imm32(0)).offset();
        Label goahead;
        masm.j(Assembler::LessThan, &goahead);
        memoryBarrier(MembarFull);
        Register out = ToRegister(ins->output());
        masm.xorl(out,out);
        masm.jmp(&rejoin);
        masm.bind(&goahead);
    }
    if (value->isConstant()) {
        masm.atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        Imm32(ToInt32(value)),
                                        srcAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    } else {
        masm.atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        ToRegister(value),
                                        srcAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    }
    uint32_t after = masm.size();
    if (rejoin.used())
        masm.bind(&rejoin);
    masm.append(AsmJSHeapAccess(after, after, mir->viewType(), maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    MAsmJSLoadGlobalVar *mir = ins->mir();

    MIRType type = mir->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffsetLabel label;
    switch (type) {
      case MIRType_Int32:
        label = masm.loadRipRelativeInt32(ToRegister(ins->output()));
        break;
      case MIRType_Float32:
        label = masm.loadRipRelativeFloat32(ToFloatRegister(ins->output()));
        break;
      case MIRType_Double:
        label = masm.loadRipRelativeDouble(ToFloatRegister(ins->output()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType_Int32x4:
        label = masm.loadRipRelativeInt32x4(ToFloatRegister(ins->output()));
        break;
      case MIRType_Float32x4:
        label = masm.loadRipRelativeFloat32x4(ToFloatRegister(ins->output()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitAsmJSLoadGlobalVar");
    }

    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffsetLabel label;
    switch (type) {
      case MIRType_Int32:
        label = masm.storeRipRelativeInt32(ToRegister(ins->value()));
        break;
      case MIRType_Float32:
        label = masm.storeRipRelativeFloat32(ToFloatRegister(ins->value()));
        break;
      case MIRType_Double:
        label = masm.storeRipRelativeDouble(ToFloatRegister(ins->value()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType_Int32x4:
        label = masm.storeRipRelativeInt32x4(ToFloatRegister(ins->value()));
        break;
      case MIRType_Float32x4:
        label = masm.storeRipRelativeFloat32x4(ToFloatRegister(ins->value()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitAsmJSStoreGlobalVar");
    }

    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register tmp = ToRegister(ins->temp());
    Register out = ToRegister(ins->output());

    CodeOffsetLabel label = masm.leaRipRelative(tmp);
    masm.loadPtr(Operand(tmp, index, TimesEight, 0), out);
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    CodeOffsetLabel label = masm.loadRipRelativeInt64(ToRegister(ins->output()));
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // Can always use the scratch register on x64.
    addState->dispatchScratch = ScratchReg;
}

void
CodeGeneratorX64::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateDouble uses cvttsd2sq. Unlike the x86
    // implementation, this should handle most doubles and we can just
    // call a stub if it fails.
    emitTruncateDouble(input, output, ins->mir());
}

void
CodeGeneratorX64::visitTruncateFToInt32(LTruncateFToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateFloat32 uses cvttss2sq. Unlike the x86
    // implementation, this should handle most floats and we can just
    // call a stub if it fails.
    emitTruncateFloat32(input, output, ins->mir());
}
