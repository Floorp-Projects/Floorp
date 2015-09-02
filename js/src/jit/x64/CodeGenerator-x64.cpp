/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/CodeGenerator-x64.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"

#include "jsscriptinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

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
        if (box->type() == MIRType_Float32) {
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
        masm.moveValue(*rhs->toConstant(), scratch);
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
        masm.moveValue(*rhs->toConstant(), scratch);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), scratch);

    // Perform the comparison.
    masm.cmpPtr(lhs.valueReg(), scratch);
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX64::visitCompareV(LCompareV* lir)
{
    MCompare* mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
}

void
CodeGeneratorX64::visitCompareVAndBranch(LCompareVAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    masm.cmpPtr(lhs.valueReg(), rhs.valueReg());
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
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
CodeGeneratorX64::visitAsmJSCall(LAsmJSCall* ins)
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
          case 4: masm.loadUnalignedFloat32x4(srcAddr, out); break;
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
          case 4: masm.loadUnalignedInt32x4(srcAddr, out); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("should only handle SIMD types");
    }
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

    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (gen->needsAsmJSBoundsCheckBranch(mir))
        maybeCmpOffset = emitAsmJSBoundsCheckBranch(mir, mir, ToRegister(ptr),
                                                    gen->outOfBoundsLabel());

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
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/true, type, 2, srcAddr,
                                    *ins->output()->output());
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));

        // Load Z (W is zeroed)
        // This is still in bounds, as we've checked with a manual bounds check
        // or we had enough space for sure when removing the bounds check.
        before = after;
        loadSimd(type, 1, srcAddrZ, ScratchSimdReg);
        after = masm.size();
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/true, type, 1, srcAddrZ, LFloatReg(ScratchSimdReg));
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw,
                                    AsmJSHeapAccess::NoLengthCheck, 8));

        // Move ZW atop XY
        masm.vmovlhps(ScratchSimdReg, out, out);
    } else {
        uint32_t before = masm.size();
        loadSimd(type, numElems, srcAddr, out);
        uint32_t after = masm.size();
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/true, type, numElems, srcAddr, *ins->output()->output());
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
    }

    if (maybeCmpOffset != AsmJSHeapAccess::NoLengthCheck)
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
    OutOfLineLoadTypedArrayOutOfBounds* ool = nullptr;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (gen->needsAsmJSBoundsCheckBranch(mir)) {
        Label* jumpTo = nullptr;
        if (mir->isAtomicAccess()) {
            jumpTo = gen->outOfBoundsLabel();
        } else {
            ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), accessType);
            addOutOfLineCode(ool, mir);
            jumpTo = ool->entry();
        }
        maybeCmpOffset = emitAsmJSBoundsCheckBranch(mir, mir, ToRegister(ptr), jumpTo);
    }

    uint32_t before = masm.size();
    switch (accessType) {
      case Scalar::Int8:      masm.movsbl(srcAddr, ToRegister(out)); break;
      case Scalar::Uint8:     masm.movzbl(srcAddr, ToRegister(out)); break;
      case Scalar::Int16:     masm.movswl(srcAddr, ToRegister(out)); break;
      case Scalar::Uint16:    masm.movzwl(srcAddr, ToRegister(out)); break;
      case Scalar::Int32:
      case Scalar::Uint32:    masm.movl(srcAddr, ToRegister(out)); break;
      case Scalar::Float32:   masm.loadFloat32(srcAddr, ToFloatRegister(out)); break;
      case Scalar::Float64:   masm.loadDouble(srcAddr, ToFloatRegister(out)); break;
      case Scalar::Float32x4:
      case Scalar::Int32x4:   MOZ_CRASH("SIMD loads should be handled in emitSimdLoad");
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
          MOZ_CRASH("unexpected array type");
    }
    uint32_t after = masm.size();
    verifyHeapAccessDisassembly(before, after, /*isLoad=*/true, accessType, 0, srcAddr, *out->output());
    if (ool) {
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(ool->rejoin());
    }
    memoryBarrier(mir->barrierAfter());
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::CarryOn, maybeCmpOffset));
}

void
CodeGeneratorX64::storeSimd(Scalar::Type type, unsigned numElems, FloatRegister in,
                            const Operand& dstAddr)
{
    switch (type) {
      case Scalar::Float32x4: {
        switch (numElems) {
          // In memory-to-register mode, movss zeroes out the high lanes.
          case 1: masm.storeFloat32(in, dstAddr); break;
          // See comment above, which also applies to movsd.
          case 2: masm.storeDouble(in, dstAddr); break;
          case 4: masm.storeUnalignedFloat32x4(in, dstAddr); break;
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
          case 4: masm.storeUnalignedInt32x4(in, dstAddr); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
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

    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (gen->needsAsmJSBoundsCheckBranch(mir))
        maybeCmpOffset = emitAsmJSBoundsCheckBranch(mir, mir, ToRegister(ptr),
                                                    gen->outOfBoundsLabel());

    unsigned numElems = mir->numSimdElems();
    if (numElems == 3) {
        MOZ_ASSERT(type == Scalar::Int32x4 || type == Scalar::Float32x4);

        Operand dstAddrZ =
            ptr->isBogus()
            ? Operand(HeapReg, 2 * sizeof(float) + mir->offset())
            : Operand(HeapReg, ToRegister(ptr), TimesOne, 2 * sizeof(float) + mir->offset());

        // It's possible that the Z could be out of bounds when the XY is in
        // bounds. To avoid storing the XY before the exception is thrown, we
        // store the Z first, and record its offset in the AsmJSHeapAccess so
        // that the signal handler knows to check the bounds of the full
        // access, rather than just the Z.
        masm.vmovhlps(in, ScratchSimdReg, ScratchSimdReg);
        uint32_t before = masm.size();
        storeSimd(type, 1, ScratchSimdReg, dstAddrZ);
        uint32_t after = masm.size();
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/false, type, 1, dstAddrZ, LFloatReg(ScratchSimdReg));
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset, 8));

        // Store XY
        before = after;
        storeSimd(type, 2, in, dstAddr);
        after = masm.size();
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/false, type, 2, dstAddr, *ins->value());
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw));
    } else {
        uint32_t before = masm.size();
        storeSimd(type, numElems, in, dstAddr);
        uint32_t after = masm.size();
        verifyHeapAccessDisassembly(before, after, /*isLoad=*/false, type, numElems, dstAddr, *ins->value());
        masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
    }

    if (maybeCmpOffset != AsmJSHeapAccess::NoLengthCheck)
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
}

void
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();

    if (Scalar::isSimdType(accessType))
        return emitSimdStore(ins);

    const LAllocation* value = ins->value();
    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(HeapReg, mir->offset())
                      : Operand(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    memoryBarrier(mir->barrierBefore());
    Label* rejoin = nullptr;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (gen->needsAsmJSBoundsCheckBranch(mir)) {
        Label* jumpTo = nullptr;
        if (mir->isAtomicAccess())
            jumpTo = gen->outOfBoundsLabel();
        else
            rejoin = jumpTo = alloc().lifoAlloc()->new_<Label>();
        maybeCmpOffset = emitAsmJSBoundsCheckBranch(mir, mir, ToRegister(ptr), jumpTo);
    }

    uint32_t before = masm.size();
    if (value->isConstant()) {
        switch (accessType) {
          case Scalar::Int8:
          case Scalar::Uint8:        masm.movb(Imm32(ToInt32(value)), dstAddr); break;
          case Scalar::Int16:
          case Scalar::Uint16:       masm.movw(Imm32(ToInt32(value)), dstAddr); break;
          case Scalar::Int32:
          case Scalar::Uint32:       masm.movl(Imm32(ToInt32(value)), dstAddr); break;
          case Scalar::Float32:
          case Scalar::Float64:
          case Scalar::Float32x4:
          case Scalar::Int32x4:
          case Scalar::Uint8Clamped:
          case Scalar::MaxTypedArrayViewType:
              MOZ_CRASH("unexpected array type");
        }
    } else {
        switch (accessType) {
          case Scalar::Int8:
          case Scalar::Uint8:        masm.movb(ToRegister(value), dstAddr); break;
          case Scalar::Int16:
          case Scalar::Uint16:       masm.movw(ToRegister(value), dstAddr); break;
          case Scalar::Int32:
          case Scalar::Uint32:       masm.movl(ToRegister(value), dstAddr); break;
          case Scalar::Float32:      masm.storeFloat32(ToFloatRegister(value), dstAddr); break;
          case Scalar::Float64:      masm.storeDouble(ToFloatRegister(value), dstAddr); break;
          case Scalar::Float32x4:
          case Scalar::Int32x4:      MOZ_CRASH("SIMD stores must be handled in emitSimdStore");
          case Scalar::Uint8Clamped:
          case Scalar::MaxTypedArrayViewType:
              MOZ_CRASH("unexpected array type");
        }
    }
    uint32_t after = masm.size();
    verifyHeapAccessDisassembly(before, after, /*isLoad=*/false, accessType, 0, dstAddr, *value);
    if (rejoin) {
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(rejoin);
    }
    memoryBarrier(mir->barrierAfter());
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::CarryOn, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins)
{
    MAsmJSCompareExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    const LAllocation* ptr = ins->ptr();

    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());
    MOZ_ASSERT(ptr->isRegister());
    BaseIndex srcAddr(HeapReg, ToRegister(ptr), TimesOne, mir->offset());

    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());

    // Note that we can't use
    // needsAsmJSBoundsCheckBranch/emitAsmJSBoundsCheckBranch/cleanupAfterAsmJSBoundsCheckBranch
    // since signal-handler bounds checking is not yet implemented for atomic accesses.
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmp32WithPatch(ToRegister(ptr), Imm32(-mir->endOffset())).offset();
        masm.j(Assembler::Above, gen->outOfBoundsLabel());
    }
    uint32_t before = masm.size();
    masm.compareExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        srcAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins)
{
    MAsmJSAtomicExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    const LAllocation* ptr = ins->ptr();

    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());
    MOZ_ASSERT(ptr->isRegister());
    MOZ_ASSERT(accessType <= Scalar::Uint32);

    BaseIndex srcAddr(HeapReg, ToRegister(ptr), TimesOne, mir->offset());
    Register value = ToRegister(ins->value());

    // Note that we can't use
    // needsAsmJSBoundsCheckBranch/emitAsmJSBoundsCheckBranch/cleanupAfterAsmJSBoundsCheckBranch
    // since signal-handler bounds checking is not yet implemented for atomic accesses.
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmp32WithPatch(ToRegister(ptr), Imm32(-mir->endOffset())).offset();
        masm.j(Assembler::Above, gen->outOfBoundsLabel());
    }
    uint32_t before = masm.size();
    masm.atomicExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                       srcAddr,
                                       value,
                                       InvalidReg,
                                       ToAnyRegister(ins->output()));
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins)
{
    MOZ_ASSERT(ins->mir()->hasUses());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne, mir->offset());

    // Note that we can't use
    // needsAsmJSBoundsCheckBranch/emitAsmJSBoundsCheckBranch/cleanupAfterAsmJSBoundsCheckBranch
    // since signal-handler bounds checking is not yet implemented for atomic accesses.
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmp32WithPatch(ptrReg, Imm32(-mir->endOffset())).offset();
        masm.j(Assembler::Above, gen->outOfBoundsLabel());
    }
    uint32_t before = masm.size();
    if (value->isConstant()) {
        masm.atomicBinopToTypedIntArray(op, accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        Imm32(ToInt32(value)),
                                        srcAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    } else {
        masm.atomicBinopToTypedIntArray(op, accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        ToRegister(value),
                                        srcAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    }
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins)
{
    MOZ_ASSERT(!ins->mir()->hasUses());
    MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    BaseIndex srcAddr(HeapReg, ptrReg, TimesOne, mir->offset());

    // Note that we can't use
    // needsAsmJSBoundsCheckBranch/emitAsmJSBoundsCheckBranch/cleanupAfterAsmJSBoundsCheckBranch
    // since signal-handler bounds checking is not yet implemented for atomic accesses.
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmp32WithPatch(ptrReg, Imm32(-mir->endOffset())).offset();
        masm.j(Assembler::Above, gen->outOfBoundsLabel());
    }

    uint32_t before = masm.size();
    if (value->isConstant())
        masm.atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), srcAddr);
    else
        masm.atomicBinopToTypedIntArray(op, accessType, ToRegister(value), srcAddr);
    MOZ_ASSERT(mir->offset() == 0,
               "The AsmJS signal handler doesn't yet support emulating "
               "atomic accesses in the case of a fault from an unwrapped offset");
    masm.append(AsmJSHeapAccess(before, AsmJSHeapAccess::Throw, maybeCmpOffset));
}

void
CodeGeneratorX64::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar* ins)
{
    MAsmJSLoadGlobalVar* mir = ins->mir();

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
CodeGeneratorX64::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar* ins)
{
    MAsmJSStoreGlobalVar* mir = ins->mir();

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
CodeGeneratorX64::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr* ins)
{
    MAsmJSLoadFuncPtr* mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register tmp = ToRegister(ins->temp());
    Register out = ToRegister(ins->output());

    CodeOffsetLabel label = masm.leaRipRelative(tmp);
    masm.loadPtr(Operand(tmp, index, TimesEight, 0), out);
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX64::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc* ins)
{
    MAsmJSLoadFFIFunc* mir = ins->mir();

    CodeOffsetLabel label = masm.loadRipRelativeInt64(ToRegister(ins->output()));
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
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

namespace js {
namespace jit {

// Out-of-line math_random_no_outparam call for LRandom.
class OutOfLineRandom : public OutOfLineCodeBase<CodeGeneratorX64>
{
    LRandom* lir_;

  public:
    explicit OutOfLineRandom(LRandom* lir)
      : lir_(lir)
    { }

    void accept(CodeGeneratorX64* codegen) {
        codegen->visitOutOfLineRandom(this);
    }

    LRandom* lir() const {
        return lir_;
    }
};

} // namespace jit
} // namespace js

static const double RNG_DSCALE_INV = 1 / RNG_DSCALE;

void
CodeGeneratorX64::visitRandom(LRandom* ins)
{
    FloatRegister output = ToFloatRegister(ins->output());

    Register JSCompartmentReg = ToRegister(ins->temp());
    Register rngStateReg = ToRegister(ins->temp2());
    Register highReg = ToRegister(ins->temp3());
    Register lowReg = ToRegister(ins->temp4());
    Register rngMaskReg = ToRegister(ins->temp5());

    // rngState = cx->compartment()->rngState
    masm.loadJSContext(JSCompartmentReg);
    masm.loadPtr(Address(JSCompartmentReg, JSContext::offsetOfCompartment()), JSCompartmentReg);
    masm.loadPtr(Address(JSCompartmentReg, JSCompartment::offsetOfRngState()), rngStateReg);

    // if rngState == 0, escape from inlined code and call
    // math_random_no_outparam.
    OutOfLineRandom* ool = new(alloc()) OutOfLineRandom(ins);
    addOutOfLineCode(ool, ins->mir());
    masm.branchTestPtr(Assembler::Zero, rngStateReg, rngStateReg, ool->entry());

    // nextstate = rngState * RNG_MULTIPLIER;
    Register& rngMultiplierReg = lowReg;
    masm.movq(ImmWord(RNG_MULTIPLIER), rngMultiplierReg);
    masm.imulq(rngMultiplierReg, rngStateReg);

    // nextstate += RNG_ADDEND;
    masm.addq(Imm32(RNG_ADDEND), rngStateReg);

    // nextstate &= RNG_MASK;
    masm.movq(ImmWord(RNG_MASK), rngMaskReg);
    masm.andq(rngMaskReg, rngStateReg);

    // rngState = nextstate

    // if rngState == 0, escape from inlined code and call
    // math_random_no_outparam.
    masm.j(Assembler::Zero, ool->entry());

    // high = (nextstate >> (RNG_STATE_WIDTH - RNG_HIGH_BITS)) << RNG_LOW_BITS;
    masm.movq(rngStateReg, highReg);
    masm.shrq(Imm32(RNG_STATE_WIDTH - RNG_HIGH_BITS), highReg);
    masm.shlq(Imm32(RNG_LOW_BITS), highReg);

    // nextstate = rngState * RNG_MULTIPLIER;
    masm.imulq(rngMultiplierReg, rngStateReg);

    // nextstate += RNG_ADDEND;
    masm.addq(Imm32(RNG_ADDEND), rngStateReg);

    // nextstate &= RNG_MASK;
    masm.andq(rngMaskReg, rngStateReg);

    // low = nextstate >> (RNG_STATE_WIDTH - RNG_LOW_BITS);
    masm.movq(rngStateReg, lowReg);
    masm.shrq(Imm32(RNG_STATE_WIDTH - RNG_LOW_BITS), lowReg);

    // output = double(high | low);
    masm.orq(highReg, lowReg);
    masm.vcvtsi2sdq(lowReg, output);

    // output = output * RNG_DSCALE_INV;
    Register& rngDscaleInvReg = lowReg;
    masm.movq(ImmPtr(&RNG_DSCALE_INV), rngDscaleInvReg);
    masm.vmulsd(Operand(rngDscaleInvReg, 0), output, output);

    // cx->compartment()->rngState = nextstate
    masm.storePtr(rngStateReg, Address(JSCompartmentReg, JSCompartment::offsetOfRngState()));

    masm.bind(ool->rejoin());
}

void
CodeGeneratorX64::visitOutOfLineRandom(OutOfLineRandom* ool)
{
    LRandom* ins = ool->lir();
    Register temp = ToRegister(ins->temp());
    Register temp2 = ToRegister(ins->temp2());
    MOZ_ASSERT(ToFloatRegister(ins->output()) == ReturnDoubleReg);

    LiveRegisterSet regs;
    regs.add(ReturnFloat32Reg);
    regs.add(ReturnDoubleReg);
    regs.add(ReturnInt32x4Reg);
    regs.add(ReturnFloat32x4Reg);
    saveVolatile(regs);

    masm.loadJSContext(temp);

    masm.setupUnalignedABICall(temp2);
    masm.passABIArg(temp);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void*, math_random_no_outparam), MoveOp::DOUBLE);

    restoreVolatile(regs);

    masm.jump(ool->rejoin());
}
