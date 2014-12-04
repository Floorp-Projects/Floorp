/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86/CodeGenerator-x86.h"

#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"

#include "jit/ExecutionMode-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using JS::GenericNaN;

CodeGeneratorX86::CodeGeneratorX86(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorX86Shared(gen, graph, masm)
{
}

static const uint32_t FrameSizes[] = { 128, 256, 512, 1024 };

FrameSizeClass
FrameSizeClass::FromDepth(uint32_t frameDepth)
{
    for (uint32_t i = 0; i < JS_ARRAY_LENGTH(FrameSizes); i++) {
        if (frameDepth < FrameSizes[i])
            return FrameSizeClass(i);
    }

    return FrameSizeClass::None();
}

FrameSizeClass
FrameSizeClass::ClassLimit()
{
    return FrameSizeClass(JS_ARRAY_LENGTH(FrameSizes));
}

uint32_t
FrameSizeClass::frameSize() const
{
    MOZ_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    MOZ_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

    return FrameSizes[class_];
}

ValueOperand
CodeGeneratorX86::ToValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToOutValue(LInstruction *ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToTempValue(LInstruction *ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

void
CodeGeneratorX86::visitValue(LValue *value)
{
    const ValueOperand out = ToOutValue(value);
    masm.moveValue(value->value(), out);
}

void
CodeGeneratorX86::visitBox(LBox *box)
{
    const LDefinition *type = box->getDef(TYPE_INDEX);

    DebugOnly<const LAllocation *> a = box->getOperand(0);
    MOZ_ASSERT(!a->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.mov(ImmWord(MIRTypeToTag(box->type())), ToRegister(type));
}

void
CodeGeneratorX86::visitBoxFloatingPoint(LBoxFloatingPoint *box)
{
    const LAllocation *in = box->getOperand(0);
    const ValueOperand out = ToOutValue(box);

    FloatRegister reg = ToFloatRegister(in);
    if (box->type() == MIRType_Float32) {
        masm.convertFloat32ToDouble(reg, ScratchFloat32Reg);
        reg = ScratchFloat32Reg;
    }
    masm.boxDouble(reg, out);
}

void
CodeGeneratorX86::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
        masm.cmpl(ToOperand(unbox->type()), Imm32(MIRTypeToTag(mir->type())));
        bailoutIf(Assembler::NotEqual, unbox->snapshot());
    }
}

void
CodeGeneratorX86::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Label notBoolean, done;
    masm.branchTestBoolean(Assembler::NotEqual, lhs, &notBoolean);
    {
        if (rhs->isConstant())
            masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
        else
            masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
        masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
        masm.jump(&done);
    }
    masm.bind(&notBoolean);
    {
        masm.move32(Imm32(mir->jsop() == JSOP_STRICTNE), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorX86::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    Assembler::Condition cond = masm.testBoolean(Assembler::NotEqual, lhs);
    jumpToBlock((mir->jsop() == JSOP_STRICTEQ) ? lir->ifFalse() : lir->ifTrue(), cond);

    if (rhs->isConstant())
        masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
    else
        masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX86::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    Label notEqual, done;
    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    masm.j(Assembler::NotEqual, &notEqual);
    {
        masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
        masm.emitSet(cond, output);
        masm.jump(&done);
    }
    masm.bind(&notEqual);
    {
        masm.move32(Imm32(cond == Assembler::NotEqual), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorX86::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock *notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    jumpToBlock(notEqual, Assembler::NotEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX86::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToDouble clobbers input.
    masm.convertUInt32ToDouble(temp, ToFloatRegister(lir->output()));
}

void
CodeGeneratorX86::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());
    FloatRegister output = ToFloatRegister(lir->output());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToFloat32 clobbers input.
    masm.convertUInt32ToFloat32(temp, output);
}

template<typename T>
void
CodeGeneratorX86::loadViewTypeElement(AsmJSHeapAccess::ViewType vt, const T &srcAddr,
                                      const LDefinition *out)
{
    switch (vt) {
      case AsmJSHeapAccess::Int8:         masm.movsblWithPatch(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Uint8Clamped:
      case AsmJSHeapAccess::Uint8:        masm.movzblWithPatch(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Int16:        masm.movswlWithPatch(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Uint16:       masm.movzwlWithPatch(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Int32:
      case AsmJSHeapAccess::Uint32:       masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case AsmJSHeapAccess::Float32:      masm.movssWithPatch(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Float64:      masm.movsdWithPatch(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Float32x4:    masm.movupsWithPatch(srcAddr, ToFloatRegister(out)); break;
      case AsmJSHeapAccess::Int32x4:      masm.movdquWithPatch(srcAddr, ToFloatRegister(out)); break;
    }
}

template<typename T>
void
CodeGeneratorX86::loadAndNoteViewTypeElement(AsmJSHeapAccess::ViewType vt, const T &srcAddr,
                                             const LDefinition *out)
{
    uint32_t before = masm.size();
    loadViewTypeElement(vt, srcAddr, out);
    uint32_t after = masm.size();
    masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out)));
}

void
CodeGeneratorX86::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    const MLoadTypedArrayElementStatic *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = AsmJSHeapAccess::ViewType(mir->viewType());
    MOZ_ASSERT_IF(vt == AsmJSHeapAccess::Float32, mir->type() == MIRType_Float32);

    Register ptr = ToRegister(ins->ptr());
    const LDefinition *out = ins->output();

    OutOfLineLoadTypedArrayOutOfBounds *ool = nullptr;
    if (!mir->fallible()) {
        ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), vt);
        addOutOfLineCode(ool, ins->mir());
    }

    masm.cmpl(ptr, Imm32(mir->length()));
    if (ool)
        masm.j(Assembler::AboveOrEqual, ool->entry());
    else
        bailoutIf(Assembler::AboveOrEqual, ins->snapshot());

    Address srcAddr(ptr, (int32_t) mir->base());
    loadViewTypeElement(vt, srcAddr, out);
    if (vt == AsmJSHeapAccess::Float64)
        masm.canonicalizeDouble(ToFloatRegister(out));
    if (vt == AsmJSHeapAccess::Float32)
        masm.canonicalizeFloat(ToFloatRegister(out));
    if (ool)
        masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::visitAsmJSCall(LAsmJSCall *ins)
{
    MAsmJSCall *mir = ins->mir();

    emitAsmJSCall(ins);

    if (IsFloatingPointType(mir->type()) && mir->callee().which() == MAsmJSCall::Callee::Builtin) {
        if (mir->type() == MIRType_Float32) {
            masm.reserveStack(sizeof(float));
            Operand op(esp, 0);
            masm.fstp32(op);
            masm.loadFloat32(op, ReturnFloat32Reg);
            masm.freeStack(sizeof(float));
        } else {
            MOZ_ASSERT(mir->type() == MIRType_Double);
            masm.reserveStack(sizeof(double));
            Operand op(esp, 0);
            masm.fstp(op);
            masm.loadDouble(op, ReturnDoubleReg);
            masm.freeStack(sizeof(double));
        }
    }
}

void
CodeGeneratorX86::memoryBarrier(MemoryBarrierBits barrier)
{
    if (barrier & MembarStoreLoad)
        masm.storeLoadFence();
}

void
CodeGeneratorX86::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    const MAsmJSLoadHeap *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    const LDefinition *out = ins->output();

    memoryBarrier(ins->mir()->barrierBefore());

    if (ptr->isConstant()) {
        // The constant displacement still needs to be added to the as-yet-unknown
        // base address of the heap. For now, embed the displacement as an
        // immediate in the instruction. This displacement will fixed up when the
        // base address is known during dynamic linking (AsmJSModule::initHeap).
        PatchedAbsoluteAddress srcAddr((void *) ptr->toConstant()->toInt32());
        loadAndNoteViewTypeElement(vt, srcAddr, out);
        memoryBarrier(ins->mir()->barrierAfter());
        return;
    }

    Register ptrReg = ToRegister(ptr);
    Address srcAddr(ptrReg, 0);

    if (!mir->needsBoundsCheck()) {
        loadAndNoteViewTypeElement(vt, srcAddr, out);
        memoryBarrier(ins->mir()->barrierAfter());
        return;
    }

    OutOfLineLoadTypedArrayOutOfBounds *ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), vt);
    addOutOfLineCode(ool, mir);

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptrReg, Imm32(0));
    masm.j(Assembler::AboveOrEqual, ool->entry());

    uint32_t before = masm.size();
    loadViewTypeElement(vt, srcAddr, out);
    uint32_t after = masm.size();
    masm.bind(ool->rejoin());
    memoryBarrier(ins->mir()->barrierAfter());
    masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out), cmp.offset()));
}

template<typename T>
void
CodeGeneratorX86::storeViewTypeElement(AsmJSHeapAccess::ViewType vt, const LAllocation *value,
                                       const T &dstAddr)
{
    switch (vt) {
      case AsmJSHeapAccess::Int8:
      case AsmJSHeapAccess::Uint8Clamped:
      case AsmJSHeapAccess::Uint8:        masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Int16:
      case AsmJSHeapAccess::Uint16:       masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Int32:
      case AsmJSHeapAccess::Uint32:       masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Float32:      masm.movssWithPatch(ToFloatRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Float64:      masm.movsdWithPatch(ToFloatRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Float32x4:    masm.movupsWithPatch(ToFloatRegister(value), dstAddr); break;
      case AsmJSHeapAccess::Int32x4:      masm.movdquWithPatch(ToFloatRegister(value), dstAddr); break;
    }
}

template<typename T>
void
CodeGeneratorX86::storeAndNoteViewTypeElement(AsmJSHeapAccess::ViewType vt,
                                              const LAllocation *value, const T &dstAddr)
{
    uint32_t before = masm.size();
    storeViewTypeElement(vt, value, dstAddr);
    uint32_t after = masm.size();
    masm.append(AsmJSHeapAccess(before, after, vt));
}

void
CodeGeneratorX86::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MStoreTypedArrayElementStatic *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = AsmJSHeapAccess::ViewType(mir->viewType());

    Register ptr = ToRegister(ins->ptr());
    const LAllocation *value = ins->value();

    masm.cmpl(ptr, Imm32(mir->length()));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    Address dstAddr(ptr, (int32_t) mir->base());
    storeViewTypeElement(vt, value, dstAddr);
    masm.bind(&rejoin);
}

void
CodeGeneratorX86::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    AsmJSHeapAccess::ViewType vt = mir->viewType();
    const LAllocation *value = ins->value();
    const LAllocation *ptr = ins->ptr();

    memoryBarrier(ins->mir()->barrierBefore());

    if (ptr->isConstant()) {
        // The constant displacement still needs to be added to the as-yet-unknown
        // base address of the heap. For now, embed the displacement as an
        // immediate in the instruction. This displacement will fixed up when the
        // base address is known during dynamic linking (AsmJSModule::initHeap).
        PatchedAbsoluteAddress dstAddr((void *) ptr->toConstant()->toInt32());
        storeAndNoteViewTypeElement(vt, value, dstAddr);
        memoryBarrier(ins->mir()->barrierAfter());
        return;
    }

    Register ptrReg = ToRegister(ptr);
    Address dstAddr(ptrReg, 0);

    if (!mir->needsBoundsCheck()) {
        storeAndNoteViewTypeElement(vt, value, dstAddr);
        memoryBarrier(ins->mir()->barrierAfter());
        return;
    }

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptrReg, Imm32(0));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    uint32_t before = masm.size();
    storeViewTypeElement(vt, value, dstAddr);
    uint32_t after = masm.size();
    masm.bind(&rejoin);
    memoryBarrier(ins->mir()->barrierAfter());
    masm.append(AsmJSHeapAccess(before, after, vt, cmp.offset()));
}

void
CodeGeneratorX86::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap *ins)
{
    MAsmJSCompareExchangeHeap *mir = ins->mir();

    MOZ_ASSERT(mir->viewType() <= AsmJSHeapAccess::Uint32);
    Scalar::Type vt = Scalar::Type(mir->viewType());

    const LAllocation *ptr = ins->ptr();
    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());

    MOZ_ASSERT(ptr->isRegister());
    // Set up the offset within the heap in the pointer reg.
    Register ptrReg = ToRegister(ptr);

    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;

    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmplWithPatch(ptrReg, Imm32(0)).offset();
        Label goahead;
        masm.j(Assembler::LessThan, &goahead);
        memoryBarrier(MembarFull);
        Register out = ToRegister(ins->output());
        masm.xorl(out,out);
        masm.jmp(&rejoin);
        masm.bind(&goahead);
    }

    // Add in the actual heap pointer explicitly, to avoid opening up
    // the abstraction that is compareExchangeToTypedIntArray at this time.
    uint32_t before = masm.size();
    masm.addl_wide(Imm32(0), ptrReg);
    uint32_t after = masm.size();
    masm.append(AsmJSHeapAccess(before, after, mir->viewType(), maybeCmpOffset));

    Address memAddr(ToRegister(ptr), 0);
    masm.compareExchangeToTypedIntArray(vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        memAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    if (rejoin.used())
        masm.bind(&rejoin);
}

void
CodeGeneratorX86::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap *ins)
{
    MAsmJSAtomicBinopHeap *mir = ins->mir();

    MOZ_ASSERT(mir->viewType() <= AsmJSHeapAccess::Uint32);
    Scalar::Type vt = Scalar::Type(mir->viewType());

    const LAllocation *ptr = ins->ptr();
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    MOZ_ASSERT(ptr->isRegister());
    // Set up the offset within the heap in the pointer reg.
    Register ptrReg = ToRegister(ptr);

    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;

    if (mir->needsBoundsCheck()) {
        maybeCmpOffset = masm.cmplWithPatch(ptrReg, Imm32(0)).offset();
        Label goahead;
        masm.j(Assembler::LessThan, &goahead);
        memoryBarrier(MembarFull);
        Register out = ToRegister(ins->output());
        masm.xorl(out,out);
        masm.jmp(&rejoin);
        masm.bind(&goahead);
    }

    // Add in the actual heap pointer explicitly, to avoid opening up
    // the abstraction that is atomicBinopToTypedIntArray at this time.
    uint32_t before = masm.size();
    masm.addl_wide(Imm32(0), ptrReg);
    uint32_t after = masm.size();
    masm.append(AsmJSHeapAccess(before, after, mir->viewType(), maybeCmpOffset));

    Address memAddr(ptrReg, 0);
    if (value->isConstant()) {
        masm.atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        Imm32(ToInt32(value)),
                                        memAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    } else {
        masm.atomicBinopToTypedIntArray(op, vt == Scalar::Uint32 ? Scalar::Int32 : vt,
                                        ToRegister(value),
                                        memAddr,
                                        temp,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
    }
    if (rejoin.used())
        masm.bind(&rejoin);
}

void
CodeGeneratorX86::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    MAsmJSLoadGlobalVar *mir = ins->mir();
    MIRType type = mir->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffsetLabel label;
    switch (type) {
      case MIRType_Int32:
        label = masm.movlWithPatch(PatchedAbsoluteAddress(), ToRegister(ins->output()));
        break;
      case MIRType_Float32:
        label = masm.movssWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      case MIRType_Double:
        label = masm.movsdWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType_Int32x4:
        label = masm.movdqaWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      case MIRType_Float32x4:
        label = masm.movapsWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitAsmJSLoadGlobalVar");
    }
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX86::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffsetLabel label;
    switch (type) {
      case MIRType_Int32:
        label = masm.movlWithPatch(ToRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType_Float32:
        label = masm.movssWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType_Double:
        label = masm.movsdWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType_Int32x4:
        label = masm.movdqaWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType_Float32x4:
        label = masm.movapsWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      default:
        MOZ_CRASH("unexpected type in visitAsmJSStoreGlobalVar");
    }
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX86::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(PatchedAbsoluteAddress(), index, TimesFour, out);
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX86::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(PatchedAbsoluteAddress(), out);
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // On x86, where there is no general purpose scratch register available,
    // child cache classes must manually specify a dispatch scratch register.
    MOZ_CRASH("x86 needs manual assignment of dispatchScratch");
}

void
GetPropertyParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have a scratch register, but only use the temp if we needed
    // one, it's BogusTemp otherwise.
    MOZ_ASSERT(ins->isGetPropertyCacheV() || ins->isGetPropertyCacheT());
    if (ins->isGetPropertyCacheV() || ins->toGetPropertyCacheT()->temp()->isBogusTemp())
        addState->dispatchScratch = output_.scratchReg().gpr();
    else
        addState->dispatchScratch = ToRegister(ins->toGetPropertyCacheT()->temp());
}

void
GetElementParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have a scratch register, but only use the temp if we needed
    // one, it's BogusTemp otherwise.
    MOZ_ASSERT(ins->isGetElementCacheV() || ins->isGetElementCacheT());
    if (ins->isGetElementCacheV() || ins->toGetElementCacheT()->temp()->isBogusTemp())
        addState->dispatchScratch = output_.scratchReg().gpr();
    else
        addState->dispatchScratch = ToRegister(ins->toGetElementCacheT()->temp());
}

void
SetPropertyParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have an output register to reuse, so we always need a temp.
    MOZ_ASSERT(ins->isSetPropertyCacheV() || ins->isSetPropertyCacheT());
    if (ins->isSetPropertyCacheV())
        addState->dispatchScratch = ToRegister(ins->toSetPropertyCacheV()->tempForDispatchCache());
    else
        addState->dispatchScratch = ToRegister(ins->toSetPropertyCacheT()->tempForDispatchCache());
}

void
SetElementParIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have an output register to reuse, but luckily SetElementCache
    // already needs a temp.
    MOZ_ASSERT(ins->isSetElementCacheV() || ins->isSetElementCacheT());
    if (ins->isSetElementCacheV())
        addState->dispatchScratch = ToRegister(ins->toSetElementCacheV()->temp());
    else
        addState->dispatchScratch = ToRegister(ins->toSetElementCacheT()->temp());
}

namespace js {
namespace jit {

class OutOfLineTruncate : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateDToInt32 *ins_;

  public:
    OutOfLineTruncate(LTruncateDToInt32 *ins)
      : ins_(ins)
    { }

    void accept(CodeGeneratorX86 *codegen) {
        codegen->visitOutOfLineTruncate(this);
    }
    LTruncateDToInt32 *ins() const {
        return ins_;
    }
};

class OutOfLineTruncateFloat32 : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateFToInt32 *ins_;

  public:
    OutOfLineTruncateFloat32(LTruncateFToInt32 *ins)
      : ins_(ins)
    { }

    void accept(CodeGeneratorX86 *codegen) {
        codegen->visitOutOfLineTruncateFloat32(this);
    }
    LTruncateFToInt32 *ins() const {
        return ins_;
    }
};

} // namespace jit
} // namespace js

void
CodeGeneratorX86::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncate *ool = new(alloc()) OutOfLineTruncate(ins);
    addOutOfLineCode(ool, ins->mir());

    masm.branchTruncateDouble(input, output, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::visitTruncateFToInt32(LTruncateFToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncateFloat32 *ool = new(alloc()) OutOfLineTruncateFloat32(ins);
    addOutOfLineCode(ool, ins->mir());

    masm.branchTruncateFloat32(input, output, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::visitOutOfLineTruncate(OutOfLineTruncate *ool)
{
    LTruncateDToInt32 *ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        // Push double.
        masm.subl(Imm32(sizeof(double)), esp);
        masm.storeDouble(input, Operand(esp, 0));

        static const uint32_t EXPONENT_MASK = 0x7ff00000;
        static const uint32_t EXPONENT_SHIFT = FloatingPoint<double>::kExponentShift - 32;
        static const uint32_t TOO_BIG_EXPONENT = (FloatingPoint<double>::kExponentBias + 63)
                                                 << EXPONENT_SHIFT;

        // Check exponent to avoid fp exceptions.
        Label failPopDouble;
        masm.load32(Address(esp, 4), output);
        masm.and32(Imm32(EXPONENT_MASK), output);
        masm.branch32(Assembler::GreaterThanOrEqual, output, Imm32(TOO_BIG_EXPONENT), &failPopDouble);

        // Load double, perform 64-bit truncation.
        masm.fld(Operand(esp, 0));
        masm.fisttp(Operand(esp, 0));

        // Load low word, pop double and jump back.
        masm.load32(Address(esp, 0), output);
        masm.addl(Imm32(sizeof(double)), esp);
        masm.jump(ool->rejoin());

        masm.bind(&failPopDouble);
        masm.addl(Imm32(sizeof(double)), esp);
        masm.jump(&fail);
    } else {
        FloatRegister temp = ToFloatRegister(ins->tempFloat());

        // Try to convert doubles representing integers within 2^32 of a signed
        // integer, by adding/subtracting 2^32 and then trying to convert to int32.
        // This has to be an exact conversion, as otherwise the truncation works
        // incorrectly on the modified value.
        masm.xorpd(ScratchDoubleReg, ScratchDoubleReg);
        masm.ucomisd(input, ScratchDoubleReg);
        masm.j(Assembler::Parity, &fail);

        {
            Label positive;
            masm.j(Assembler::Above, &positive);

            masm.loadConstantDouble(4294967296.0, temp);
            Label skip;
            masm.jmp(&skip);

            masm.bind(&positive);
            masm.loadConstantDouble(-4294967296.0, temp);
            masm.bind(&skip);
        }

        masm.addsd(input, temp);
        masm.cvttsd2si(temp, output);
        masm.cvtsi2sd(output, ScratchDoubleReg);

        masm.ucomisd(temp, ScratchDoubleReg);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.setupUnalignedABICall(1, output);
        masm.passABIArg(input, MoveOp::DOUBLE);
        if (gen->compilingAsmJS())
            masm.callWithABI(AsmJSImm_ToInt32);
        else
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));
        masm.storeCallResult(output);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
}

void
CodeGeneratorX86::visitOutOfLineTruncateFloat32(OutOfLineTruncateFloat32 *ool)
{
    LTruncateFToInt32 *ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        // Push float32, but subtracts 64 bits so that the value popped by fisttp fits
        masm.subl(Imm32(sizeof(uint64_t)), esp);
        masm.storeFloat32(input, Operand(esp, 0));

        static const uint32_t EXPONENT_MASK = FloatingPoint<float>::kExponentBits;
        static const uint32_t EXPONENT_SHIFT = FloatingPoint<float>::kExponentShift;
        // Integers are still 64 bits long, so we can still test for an exponent > 63.
        static const uint32_t TOO_BIG_EXPONENT = (FloatingPoint<float>::kExponentBias + 63)
                                                 << EXPONENT_SHIFT;

        // Check exponent to avoid fp exceptions.
        Label failPopFloat;
        masm.movl(Operand(esp, 0), output);
        masm.and32(Imm32(EXPONENT_MASK), output);
        masm.branch32(Assembler::GreaterThanOrEqual, output, Imm32(TOO_BIG_EXPONENT), &failPopFloat);

        // Load float, perform 32-bit truncation.
        masm.fld32(Operand(esp, 0));
        masm.fisttp(Operand(esp, 0));

        // Load low word, pop 64bits and jump back.
        masm.movl(Operand(esp, 0), output);
        masm.addl(Imm32(sizeof(uint64_t)), esp);
        masm.jump(ool->rejoin());

        masm.bind(&failPopFloat);
        masm.addl(Imm32(sizeof(uint64_t)), esp);
        masm.jump(&fail);
    } else {
        FloatRegister temp = ToFloatRegister(ins->tempFloat());

        // Try to convert float32 representing integers within 2^32 of a signed
        // integer, by adding/subtracting 2^32 and then trying to convert to int32.
        // This has to be an exact conversion, as otherwise the truncation works
        // incorrectly on the modified value.
        masm.xorps(ScratchFloat32Reg, ScratchFloat32Reg);
        masm.ucomiss(input, ScratchFloat32Reg);
        masm.j(Assembler::Parity, &fail);

        {
            Label positive;
            masm.j(Assembler::Above, &positive);

            masm.loadConstantFloat32(4294967296.f, temp);
            Label skip;
            masm.jmp(&skip);

            masm.bind(&positive);
            masm.loadConstantFloat32(-4294967296.f, temp);
            masm.bind(&skip);
        }

        masm.addss(input, temp);
        masm.cvttss2si(temp, output);
        masm.cvtsi2ss(output, ScratchFloat32Reg);

        masm.ucomiss(temp, ScratchFloat32Reg);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.push(input);
        masm.setupUnalignedABICall(1, output);
        masm.cvtss2sd(input, input);
        masm.passABIArg(input, MoveOp::DOUBLE);

        if (gen->compilingAsmJS())
            masm.callWithABI(AsmJSImm_ToInt32);
        else
            masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));

        masm.storeCallResult(output);
        masm.pop(input);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
}
