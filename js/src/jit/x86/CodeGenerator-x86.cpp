/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86/CodeGenerator-x86.h"

#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "jit/IonCaches.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "js/Conversions.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::BitwiseCast;
using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using JS::GenericNaN;

CodeGeneratorX86::CodeGeneratorX86(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm)
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
CodeGeneratorX86::ToValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToOutValue(LInstruction* ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorX86::ToTempValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

void
CodeGeneratorX86::visitValue(LValue* value)
{
    const ValueOperand out = ToOutValue(value);
    masm.moveValue(value->value(), out);
}

void
CodeGeneratorX86::visitBox(LBox* box)
{
    const LDefinition* type = box->getDef(TYPE_INDEX);

    DebugOnly<const LAllocation*> a = box->getOperand(0);
    MOZ_ASSERT(!a->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.mov(ImmWord(MIRTypeToTag(box->type())), ToRegister(type));
}

void
CodeGeneratorX86::visitBoxFloatingPoint(LBoxFloatingPoint* box)
{
    const LAllocation* in = box->getOperand(0);
    const ValueOperand out = ToOutValue(box);

    FloatRegister reg = ToFloatRegister(in);
    if (box->type() == MIRType::Float32) {
        masm.convertFloat32ToDouble(reg, ScratchFloat32Reg);
        reg = ScratchFloat32Reg;
    }
    masm.boxDouble(reg, out);
}

void
CodeGeneratorX86::visitUnbox(LUnbox* unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox* mir = unbox->mir();

    if (mir->fallible()) {
        masm.cmp32(ToOperand(unbox->type()), Imm32(MIRTypeToTag(mir->type())));
        bailoutIf(Assembler::NotEqual, unbox->snapshot());
    }
}

void
CodeGeneratorX86::visitCompareB(LCompareB* lir)
{
    MCompare* mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation* rhs = lir->rhs();
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
CodeGeneratorX86::visitCompareBAndBranch(LCompareBAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation* rhs = lir->rhs();

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
CodeGeneratorX86::visitCompareBitwise(LCompareBitwise* lir)
{
    MCompare* mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
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
CodeGeneratorX86::visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock* notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    jumpToBlock(notEqual, Assembler::NotEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorX86::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble* lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToDouble clobbers input.
    masm.convertUInt32ToDouble(temp, ToFloatRegister(lir->output()));
}

void
CodeGeneratorX86::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32* lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());
    FloatRegister output = ToFloatRegister(lir->output());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToFloat32 clobbers input.
    masm.convertUInt32ToFloat32(temp, output);
}

void
CodeGeneratorX86::load(Scalar::Type accessType, const Operand& srcAddr, const LDefinition* out)
{
    switch (accessType) {
      case Scalar::Int8:         masm.movsblWithPatch(srcAddr, ToRegister(out)); break;
      case Scalar::Uint8Clamped:
      case Scalar::Uint8:        masm.movzblWithPatch(srcAddr, ToRegister(out)); break;
      case Scalar::Int16:        masm.movswlWithPatch(srcAddr, ToRegister(out)); break;
      case Scalar::Uint16:       masm.movzwlWithPatch(srcAddr, ToRegister(out)); break;
      case Scalar::Int32:
      case Scalar::Uint32:       masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case Scalar::Float32:      masm.vmovssWithPatch(srcAddr, ToFloatRegister(out)); break;
      case Scalar::Float64:      masm.vmovsdWithPatch(srcAddr, ToFloatRegister(out)); break;
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:      MOZ_CRASH("SIMD load should be handled in their own function");
      case Scalar::Int64:        MOZ_CRASH("should be handled in a separate function (2 regs)");
      case Scalar::MaxTypedArrayViewType: MOZ_CRASH("unexpected type");
    }
    masm.append(wasm::MemoryAccess(masm.size()));
}

void
CodeGeneratorX86::loadI64(Scalar::Type type, const Operand& srcAddr, Register64 out)
{
    switch (type) {
      case Scalar::Int8:
        MOZ_ASSERT(out == Register64(edx, eax));
        masm.movsblWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.cdq();
        break;
      case Scalar::Uint8:
        masm.movzblWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.xorl(out.high, out.high);
        break;
      case Scalar::Int16:
        MOZ_ASSERT(out == Register64(edx, eax));
        masm.movswlWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.cdq();
        break;
      case Scalar::Uint16:
        masm.movzwlWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.xorl(out.high, out.high);
        break;
      case Scalar::Int32:
        MOZ_ASSERT(out == Register64(edx, eax));
        masm.movlWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.cdq();
        break;
      case Scalar::Uint32:
        masm.movlWithPatch(srcAddr, out.low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.xorl(out.high, out.high);
        break;
      case Scalar::Int64:
        if (srcAddr.kind() == Operand::MEM_ADDRESS32) {
            Operand low(PatchedAbsoluteAddress(uint32_t(srcAddr.address()) + INT64LOW_OFFSET));
            Operand high(PatchedAbsoluteAddress(uint32_t(srcAddr.address()) + INT64HIGH_OFFSET));

            masm.movlWithPatch(low, out.low);
            masm.append(wasm::MemoryAccess(masm.size()));
            masm.movlWithPatch(high, out.high);
            masm.append(wasm::MemoryAccess(masm.size()));
        } else {
            MOZ_ASSERT(srcAddr.kind() == Operand::MEM_REG_DISP);
            Address addr = srcAddr.toAddress();
            Operand low(addr.base, addr.offset + INT64LOW_OFFSET);
            Operand high(addr.base, addr.offset + INT64HIGH_OFFSET);

            if (addr.base != out.low) {
                masm.movlWithPatch(low, out.low);
                masm.append(wasm::MemoryAccess(masm.size()));
                masm.movlWithPatch(high, out.high);
                masm.append(wasm::MemoryAccess(masm.size()));
            } else {
                MOZ_ASSERT(addr.base != out.high);
                masm.movlWithPatch(high, out.high);
                masm.append(wasm::MemoryAccess(masm.size()));
                masm.movlWithPatch(low, out.low);
                masm.append(wasm::MemoryAccess(masm.size()));
            }
        }
        break;
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

void
CodeGeneratorX86::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic* ins)
{
    const MLoadTypedArrayElementStatic* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    MOZ_ASSERT_IF(accessType == Scalar::Float32, mir->type() == MIRType::Float32);

    Register ptr = ToRegister(ins->ptr());
    const LDefinition* out = ins->output();
    OutOfLineLoadTypedArrayOutOfBounds* ool = nullptr;
    uint32_t offset = mir->offset();

    if (mir->needsBoundsCheck()) {
        MOZ_ASSERT(offset == 0);
        if (!mir->fallible()) {
            ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), accessType);
            addOutOfLineCode(ool, ins->mir());
        }

        masm.cmpPtr(ptr, ImmWord(mir->length()));
        if (ool)
            masm.j(Assembler::AboveOrEqual, ool->entry());
        else
            bailoutIf(Assembler::AboveOrEqual, ins->snapshot());
    }

    Operand srcAddr(ptr, int32_t(mir->base().asValue()) + int32_t(offset));
    load(accessType, srcAddr, out);
    if (accessType == Scalar::Float64)
        masm.canonicalizeDouble(ToFloatRegister(out));
    if (accessType == Scalar::Float32)
        masm.canonicalizeFloat(ToFloatRegister(out));
    if (ool)
        masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::emitWasmCall(LWasmCallBase* ins)
{
    MWasmCall* mir = ins->mir();

    emitWasmCallBase(ins);

    if (IsFloatingPointType(mir->type()) &&
        mir->callee().which() == MWasmCall::Callee::Builtin)
    {
        if (mir->type() == MIRType::Float32) {
            masm.reserveStack(sizeof(float));
            Operand op(esp, 0);
            masm.fstp32(op);
            masm.loadFloat32(op, ReturnFloat32Reg);
            masm.freeStack(sizeof(float));
        } else {
            MOZ_ASSERT(mir->type() == MIRType::Double);
            masm.reserveStack(sizeof(double));
            Operand op(esp, 0);
            masm.fstp(op);
            masm.loadDouble(op, ReturnDoubleReg);
            masm.freeStack(sizeof(double));
        }
    }
}

void
CodeGeneratorX86::visitWasmCall(LWasmCall* ins)
{
    emitWasmCall(ins);
}

void
CodeGeneratorX86::visitWasmCallI64(LWasmCallI64* ins)
{
    emitWasmCall(ins);
}

void
CodeGeneratorX86::memoryBarrier(MemoryBarrierBits barrier)
{
    if (barrier & MembarStoreLoad)
        masm.storeLoadFence();
}

void
CodeGeneratorX86::loadSimd(Scalar::Type type, unsigned numElems, const Operand& srcAddr,
                           FloatRegister out)
{
    switch (type) {
      case Scalar::Float32x4: {
        switch (numElems) {
          // In memory-to-register mode, movss zeroes out the high lanes.
          case 1: masm.vmovssWithPatch(srcAddr, out); break;
          // See comment above, which also applies to movsd.
          case 2: masm.vmovsdWithPatch(srcAddr, out); break;
          case 4: masm.vmovupsWithPatch(srcAddr, out); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int32x4: {
        switch (numElems) {
          // In memory-to-register mode, movd zeroes out the high lanes.
          case 1: masm.vmovdWithPatch(srcAddr, out); break;
          // See comment above, which also applies to movq.
          case 2: masm.vmovqWithPatch(srcAddr, out); break;
          case 4: masm.vmovdquWithPatch(srcAddr, out); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8x16:
        MOZ_ASSERT(numElems == 16, "unexpected partial load");
        masm.vmovdquWithPatch(srcAddr, out);
        break;
      case Scalar::Int16x8:
        MOZ_ASSERT(numElems == 8, "unexpected partial load");
        masm.vmovdquWithPatch(srcAddr, out);
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
    masm.append(wasm::MemoryAccess(masm.size()));
}

template <typename T>
void
CodeGeneratorX86::emitWasmLoad(T* ins)
{
    const MWasmLoad* mir = ins->mir();

    Scalar::Type accessType = mir->accessType();
    MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD NYI");
    MOZ_ASSERT(!mir->barrierBefore() && !mir->barrierAfter(), "atomics NYI");

    if (mir->offset() > INT32_MAX) {
        // This is unreachable because of the bounds check.
        masm.breakpoint();
        return;
    }

    const LAllocation* ptr = ins->ptr();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    if (mir->type() == MIRType::Int64)
        loadI64(accessType, srcAddr, ToOutRegister64(ins));
    else
        load(accessType, srcAddr, ins->output());
}

void
CodeGeneratorX86::visitWasmLoad(LWasmLoad* ins)
{
    emitWasmLoad(ins);
}

void
CodeGeneratorX86::visitWasmLoadI64(LWasmLoadI64* ins)
{
    emitWasmLoad(ins);
}

template <typename T>
void
CodeGeneratorX86::emitWasmStore(T* ins)
{
    const MWasmStore* mir = ins->mir();

    Scalar::Type accessType = mir->accessType();
    MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD NYI");
    MOZ_ASSERT(!mir->barrierBefore() && !mir->barrierAfter(), "atomics NYI");

    if (mir->offset() > INT32_MAX) {
        // This is unreachable because of the bounds check.
        masm.breakpoint();
        return;
    }

    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    if (accessType == Scalar::Int64)
        storeI64(accessType, ins->getInt64Operand(LWasmStoreI64::ValueIndex), dstAddr);
    else
        store(accessType, ins->getOperand(LWasmStore::ValueIndex), dstAddr);
}


void
CodeGeneratorX86::visitWasmStore(LWasmStore* ins)
{
    emitWasmStore(ins);
}

void
CodeGeneratorX86::visitWasmStoreI64(LWasmStoreI64* ins)
{
    emitWasmStore(ins);
}

void
CodeGeneratorX86::emitSimdLoad(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    Scalar::Type type = mir->accessType();
    FloatRegister out = ToFloatRegister(ins->output());
    const LAllocation* ptr = ins->ptr();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    bool hasBoundsCheck = maybeEmitThrowingAsmJSBoundsCheck(mir, mir, ptr);

    unsigned numElems = mir->numSimdElems();
    if (numElems == 3) {
        MOZ_ASSERT(type == Scalar::Int32x4 || type == Scalar::Float32x4);

        Operand srcAddrZ =
            ptr->isBogus()
            ? Operand(PatchedAbsoluteAddress(2 * sizeof(float) + mir->offset()))
            : Operand(ToRegister(ptr), 2 * sizeof(float) + mir->offset());

        // Load XY
        loadSimd(type, 2, srcAddr, out);

        // Load Z (W is zeroed)
        // This is still in bounds, as we've checked with a manual bounds check
        // or we had enough space for sure when removing the bounds check.
        loadSimd(type, 1, srcAddrZ, ScratchSimd128Reg);

        // Move ZW atop XY
        masm.vmovlhps(ScratchSimd128Reg, out, out);
    } else {
        loadSimd(type, numElems, srcAddr, out);
    }

    if (hasBoundsCheck)
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
}

void
CodeGeneratorX86::visitAsmJSLoadHeap(LAsmJSLoadHeap* ins)
{
    const MAsmJSLoadHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();

    if (Scalar::isSimdType(accessType))
        return emitSimdLoad(ins);

    const LAllocation* ptr = ins->ptr();
    const LDefinition* out = ins->output();
    Operand srcAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    memoryBarrier(mir->barrierBefore());

    OutOfLineLoadTypedArrayOutOfBounds* ool;
    DebugOnly<bool> hasBoundsCheck = maybeEmitAsmJSLoadBoundsCheck(mir, ins, &ool);

    load(accessType, srcAddr, out);

    if (ool) {
        MOZ_ASSERT(hasBoundsCheck);
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(ool->rejoin());
    }

    memoryBarrier(mir->barrierAfter());
}

void
CodeGeneratorX86::store(Scalar::Type accessType, const LAllocation* value, const Operand& dstAddr)
{
    switch (accessType) {
      case Scalar::Int8:
      case Scalar::Uint8Clamped:
      case Scalar::Uint8:
        masm.movbWithPatch(ToRegister(value), dstAddr);
        break;

      case Scalar::Int16:
      case Scalar::Uint16:
        masm.movwWithPatch(ToRegister(value), dstAddr);
        break;

      case Scalar::Int32:
      case Scalar::Uint32:
        masm.movlWithPatch(ToRegister(value), dstAddr);
        break;

      case Scalar::Float32:
        masm.vmovssWithPatch(ToFloatRegister(value), dstAddr);
        break;

      case Scalar::Float64:
        masm.vmovsdWithPatch(ToFloatRegister(value), dstAddr);
        break;

      case Scalar::Int64:
        MOZ_CRASH("Should be handled in storeI64.");

      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
        MOZ_CRASH("SIMD stores should be handled in emitSimdStore");

      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("unexpected type");
    }
    masm.append(wasm::MemoryAccess(masm.size()));
}

void
CodeGeneratorX86::storeI64(Scalar::Type accessType, const LInt64Allocation value,
                           const Operand& dstAddr)
{
    Register64 input = ToRegister64(value);
    MOZ_ASSERT(accessType == Scalar::Int64);
    if (dstAddr.kind() == Operand::MEM_ADDRESS32) {
        Operand low(PatchedAbsoluteAddress(uint32_t(dstAddr.address()) + INT64LOW_OFFSET));
        Operand high(PatchedAbsoluteAddress(uint32_t(dstAddr.address()) + INT64HIGH_OFFSET));

        masm.movlWithPatch(input.low, low);
        masm.append(wasm::MemoryAccess(masm.size()));
        masm.movlWithPatch(input.high, high);
        masm.append(wasm::MemoryAccess(masm.size()));
    } else {
        MOZ_ASSERT(dstAddr.kind() == Operand::MEM_REG_DISP);
        Address addr = dstAddr.toAddress();
        Operand low(addr.base, addr.offset + INT64LOW_OFFSET);
        Operand high(addr.base, addr.offset + INT64HIGH_OFFSET);

        if (addr.base != input.low) {
            masm.movlWithPatch(input.low, low);
            masm.append(wasm::MemoryAccess(masm.size()));
            masm.movlWithPatch(input.high, high);
            masm.append(wasm::MemoryAccess(masm.size()));
        } else {
            MOZ_ASSERT(addr.base != input.high);
            masm.movlWithPatch(input.high, high);
            masm.append(wasm::MemoryAccess(masm.size()));
            masm.movlWithPatch(input.low, low);
            masm.append(wasm::MemoryAccess(masm.size()));
        }
    }
}

void
CodeGeneratorX86::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic* ins)
{
    MStoreTypedArrayElementStatic* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptr = ToRegister(ins->ptr());
    const LAllocation* value = ins->value();

    canonicalizeIfDeterministic(accessType, value);

    uint32_t offset = mir->offset();
    if (!mir->needsBoundsCheck()) {
        Operand dstAddr(ptr, int32_t(mir->base().asValue()) + int32_t(offset));
        store(accessType, value, dstAddr);
        return;
    }

    MOZ_ASSERT(offset == 0);
    masm.cmpPtr(ptr, ImmWord(mir->length()));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    Operand dstAddr(ptr, int32_t(mir->base().asValue()));
    store(accessType, value, dstAddr);
    masm.bind(&rejoin);
}

void
CodeGeneratorX86::storeSimd(Scalar::Type type, unsigned numElems, FloatRegister in,
                            const Operand& dstAddr)
{
    switch (type) {
      case Scalar::Float32x4: {
        switch (numElems) {
          // In memory-to-register mode, movss zeroes out the high lanes.
          case 1: masm.vmovssWithPatch(in, dstAddr); break;
          // See comment above, which also applies to movsd.
          case 2: masm.vmovsdWithPatch(in, dstAddr); break;
          case 4: masm.vmovupsWithPatch(in, dstAddr); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int32x4: {
        switch (numElems) {
          // In memory-to-register mode, movd zeroes out the high lanes.
          case 1: masm.vmovdWithPatch(in, dstAddr); break;
          // See comment above, which also applies to movsd.
          case 2: masm.vmovqWithPatch(in, dstAddr); break;
          case 4: masm.vmovdquWithPatch(in, dstAddr); break;
          default: MOZ_CRASH("unexpected size for partial load");
        }
        break;
      }
      case Scalar::Int8x16:
        MOZ_ASSERT(numElems == 16, "unexpected partial store");
        masm.vmovdquWithPatch(in, dstAddr);
        break;
      case Scalar::Int16x8:
        MOZ_ASSERT(numElems == 8, "unexpected partial store");
        masm.vmovdquWithPatch(in, dstAddr);
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
    masm.append(wasm::MemoryAccess(masm.size()));
}

void
CodeGeneratorX86::emitSimdStore(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    Scalar::Type type = mir->accessType();
    FloatRegister in = ToFloatRegister(ins->value());
    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    bool hasBoundsCheck = maybeEmitThrowingAsmJSBoundsCheck(mir, mir, ptr);

    unsigned numElems = mir->numSimdElems();
    if (numElems == 3) {
        MOZ_ASSERT(type == Scalar::Int32x4 || type == Scalar::Float32x4);

        Operand dstAddrZ =
            ptr->isBogus()
            ? Operand(PatchedAbsoluteAddress(2 * sizeof(float) + mir->offset()))
            : Operand(ToRegister(ptr), 2 * sizeof(float) + mir->offset());

        // Store XY
        storeSimd(type, 2, in, dstAddr);

        masm.vmovhlps(in, ScratchSimd128Reg, ScratchSimd128Reg);

        // Store Z (W is zeroed)
        // This is still in bounds, as we've checked with a manual bounds check
        // or we had enough space for sure when removing the bounds check.
        storeSimd(type, 1, ScratchSimd128Reg, dstAddrZ);
    } else {
        storeSimd(type, numElems, in, dstAddr);
    }

    if (hasBoundsCheck)
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
}

void
CodeGeneratorX86::visitAsmJSStoreHeap(LAsmJSStoreHeap* ins)
{
    const MAsmJSStoreHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    const LAllocation* value = ins->value();

    canonicalizeIfDeterministic(accessType, value);

    if (Scalar::isSimdType(accessType))
        return emitSimdStore(ins);

    const LAllocation* ptr = ins->ptr();
    Operand dstAddr = ptr->isBogus()
                      ? Operand(PatchedAbsoluteAddress(mir->offset()))
                      : Operand(ToRegister(ptr), mir->offset());

    memoryBarrier(mir->barrierBefore());

    Label* rejoin;
    DebugOnly<bool> hasBoundsCheck = maybeEmitAsmJSStoreBoundsCheck(mir, ins, &rejoin);

    store(accessType, value, dstAddr);

    if (rejoin) {
        MOZ_ASSERT(hasBoundsCheck);
        cleanupAfterAsmJSBoundsCheckBranch(mir, ToRegister(ptr));
        masm.bind(rejoin);
    }

    memoryBarrier(mir->barrierAfter());
}

// Perform bounds checking on the access if necessary; if it fails,
// jump to out-of-line code that throws.  If the bounds check passes,
// set up the heap address in addrTemp.

void
CodeGeneratorX86::asmJSAtomicComputeAddress(Register addrTemp, Register ptrReg,
                                            const MWasmMemoryAccess* mir)
{
    maybeEmitWasmBoundsCheckBranch(mir, ptrReg);

    // Add in the actual heap pointer explicitly, to avoid opening up
    // the abstraction that is atomicBinopToTypedIntArray at this time.
    masm.movl(ptrReg, addrTemp);
    masm.addlWithPatch(Imm32(mir->offset()), addrTemp);
    masm.append(wasm::MemoryAccess(masm.size()));
}

void
CodeGeneratorX86::visitAsmJSCompareExchangeHeap(LAsmJSCompareExchangeHeap* ins)
{
    MAsmJSCompareExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register oldval = ToRegister(ins->oldValue());
    Register newval = ToRegister(ins->newValue());
    Register addrTemp = ToRegister(ins->addrTemp());

    asmJSAtomicComputeAddress(addrTemp, ptrReg, mir);

    Address memAddr(addrTemp, mir->offset());
    masm.compareExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                        memAddr,
                                        oldval,
                                        newval,
                                        InvalidReg,
                                        ToAnyRegister(ins->output()));
}

void
CodeGeneratorX86::visitAsmJSAtomicExchangeHeap(LAsmJSAtomicExchangeHeap* ins)
{
    MAsmJSAtomicExchangeHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register value = ToRegister(ins->value());
    Register addrTemp = ToRegister(ins->addrTemp());

    asmJSAtomicComputeAddress(addrTemp, ptrReg, mir);

    Address memAddr(addrTemp, mir->offset());
    masm.atomicExchangeToTypedIntArray(accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                       memAddr,
                                       value,
                                       InvalidReg,
                                       ToAnyRegister(ins->output()));
}

void
CodeGeneratorX86::visitAsmJSAtomicBinopHeap(LAsmJSAtomicBinopHeap* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register temp = ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
    Register addrTemp = ToRegister(ins->addrTemp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    asmJSAtomicComputeAddress(addrTemp, ptrReg, mir);

    Address memAddr(addrTemp, mir->offset());
    if (value->isConstant()) {
        atomicBinopToTypedIntArray(op, accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                   Imm32(ToInt32(value)),
                                   memAddr,
                                   temp,
                                   InvalidReg,
                                   ToAnyRegister(ins->output()));
    } else {
        atomicBinopToTypedIntArray(op, accessType == Scalar::Uint32 ? Scalar::Int32 : accessType,
                                   ToRegister(value),
                                   memAddr,
                                   temp,
                                   InvalidReg,
                                   ToAnyRegister(ins->output()));
    }
}

void
CodeGeneratorX86::visitAsmJSAtomicBinopHeapForEffect(LAsmJSAtomicBinopHeapForEffect* ins)
{
    MAsmJSAtomicBinopHeap* mir = ins->mir();
    Scalar::Type accessType = mir->accessType();
    Register ptrReg = ToRegister(ins->ptr());
    Register addrTemp = ToRegister(ins->addrTemp());
    const LAllocation* value = ins->value();
    AtomicOp op = mir->operation();

    MOZ_ASSERT(!mir->hasUses());

    asmJSAtomicComputeAddress(addrTemp, ptrReg, mir);

    Address memAddr(addrTemp, mir->offset());
    if (value->isConstant())
        atomicBinopToTypedIntArray(op, accessType, Imm32(ToInt32(value)), memAddr);
    else
        atomicBinopToTypedIntArray(op, accessType, ToRegister(value), memAddr);
}

void
CodeGeneratorX86::visitWasmLoadGlobalVar(LWasmLoadGlobalVar* ins)
{
    MWasmLoadGlobalVar* mir = ins->mir();
    MIRType type = mir->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffset label;
    switch (type) {
      case MIRType::Int32:
        label = masm.movlWithPatch(PatchedAbsoluteAddress(), ToRegister(ins->output()));
        break;
      case MIRType::Float32:
        label = masm.vmovssWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      case MIRType::Double:
        label = masm.vmovsdWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType::Int8x16:
      case MIRType::Int16x8:
      case MIRType::Int32x4:
      case MIRType::Bool8x16:
      case MIRType::Bool16x8:
      case MIRType::Bool32x4:
        label = masm.vmovdqaWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      case MIRType::Float32x4:
        label = masm.vmovapsWithPatch(PatchedAbsoluteAddress(), ToFloatRegister(ins->output()));
        break;
      default:
        MOZ_CRASH("unexpected type in visitWasmLoadGlobalVar");
    }
    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX86::visitWasmLoadGlobalVarI64(LWasmLoadGlobalVarI64* ins)
{
    MWasmLoadGlobalVar* mir = ins->mir();

    MOZ_ASSERT(mir->type() == MIRType::Int64);
    Register64 output = ToOutRegister64(ins);

    CodeOffset labelLow = masm.movlWithPatch(PatchedAbsoluteAddress(), output.low);
    masm.append(wasm::GlobalAccess(labelLow, mir->globalDataOffset() + INT64LOW_OFFSET));
    CodeOffset labelHigh = masm.movlWithPatch(PatchedAbsoluteAddress(), output.high);
    masm.append(wasm::GlobalAccess(labelHigh, mir->globalDataOffset() + INT64HIGH_OFFSET));
}

void
CodeGeneratorX86::visitWasmStoreGlobalVar(LWasmStoreGlobalVar* ins)
{
    MWasmStoreGlobalVar* mir = ins->mir();

    MIRType type = mir->value()->type();
    MOZ_ASSERT(IsNumberType(type) || IsSimdType(type));

    CodeOffset label;
    switch (type) {
      case MIRType::Int32:
        label = masm.movlWithPatch(ToRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType::Float32:
        label = masm.vmovssWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType::Double:
        label = masm.vmovsdWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      // Aligned access: code is aligned on PageSize + there is padding
      // before the global data section.
      case MIRType::Int32x4:
      case MIRType::Bool32x4:
        label = masm.vmovdqaWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      case MIRType::Float32x4:
        label = masm.vmovapsWithPatch(ToFloatRegister(ins->value()), PatchedAbsoluteAddress());
        break;
      default:
        MOZ_CRASH("unexpected type in visitWasmStoreGlobalVar");
    }
    masm.append(wasm::GlobalAccess(label, mir->globalDataOffset()));
}

void
CodeGeneratorX86::visitWasmStoreGlobalVarI64(LWasmStoreGlobalVarI64* ins)
{
    MWasmStoreGlobalVar* mir = ins->mir();

    MOZ_ASSERT(mir->value()->type() == MIRType::Int64);
    Register64 input = ToRegister64(ins->value());

    CodeOffset labelLow = masm.movlWithPatch(input.low, PatchedAbsoluteAddress());
    masm.append(wasm::GlobalAccess(labelLow, mir->globalDataOffset() + INT64LOW_OFFSET));
    CodeOffset labelHigh = masm.movlWithPatch(input.high, PatchedAbsoluteAddress());
    masm.append(wasm::GlobalAccess(labelHigh, mir->globalDataOffset() + INT64HIGH_OFFSET));
}

namespace js {
namespace jit {

class OutOfLineTruncate : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateDToInt32* ins_;

  public:
    OutOfLineTruncate(LTruncateDToInt32* ins)
      : ins_(ins)
    { }

    void accept(CodeGeneratorX86* codegen) {
        codegen->visitOutOfLineTruncate(this);
    }
    LTruncateDToInt32* ins() const {
        return ins_;
    }
};

class OutOfLineTruncateFloat32 : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateFToInt32* ins_;

  public:
    OutOfLineTruncateFloat32(LTruncateFToInt32* ins)
      : ins_(ins)
    { }

    void accept(CodeGeneratorX86* codegen) {
        codegen->visitOutOfLineTruncateFloat32(this);
    }
    LTruncateFToInt32* ins() const {
        return ins_;
    }
};

} // namespace jit
} // namespace js

void
CodeGeneratorX86::visitTruncateDToInt32(LTruncateDToInt32* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncate* ool = new(alloc()) OutOfLineTruncate(ins);
    addOutOfLineCode(ool, ins->mir());

    masm.branchTruncateDoubleMaybeModUint32(input, output, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::visitTruncateFToInt32(LTruncateFToInt32* ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncateFloat32* ool = new(alloc()) OutOfLineTruncateFloat32(ins);
    addOutOfLineCode(ool, ins->mir());

    masm.branchTruncateFloat32MaybeModUint32(input, output, ool->entry());
    masm.bind(ool->rejoin());
}

void
CodeGeneratorX86::visitOutOfLineTruncate(OutOfLineTruncate* ool)
{
    LTruncateDToInt32* ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        Label failPopDouble;
        // Push double.
        masm.subl(Imm32(sizeof(double)), esp);
        masm.storeDouble(input, Operand(esp, 0));

        // Check exponent to avoid fp exceptions.
        masm.branchDoubleNotInInt64Range(Address(esp, 0), output, &failPopDouble);

        // Load double, perform 64-bit truncation.
        masm.truncateDoubleToInt64(Address(esp, 0), Address(esp, 0), output);

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
        masm.zeroDouble(ScratchDoubleReg);
        masm.vucomisd(ScratchDoubleReg, input);
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

        masm.addDouble(input, temp);
        masm.vcvttsd2si(temp, output);
        masm.vcvtsi2sd(output, ScratchDoubleReg, ScratchDoubleReg);

        masm.vucomisd(ScratchDoubleReg, temp);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.setupUnalignedABICall(output);
        masm.passABIArg(input, MoveOp::DOUBLE);
        if (gen->compilingAsmJS())
            masm.callWithABI(wasm::SymbolicAddress::ToInt32);
        else
            masm.callWithABI(BitwiseCast<void*, int32_t(*)(double)>(JS::ToInt32));
        masm.storeCallResult(output);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
}

void
CodeGeneratorX86::visitOutOfLineTruncateFloat32(OutOfLineTruncateFloat32* ool)
{
    LTruncateFToInt32* ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        Label failPopFloat;

        // Push float32, but subtracts 64 bits so that the value popped by fisttp fits
        masm.subl(Imm32(sizeof(uint64_t)), esp);
        masm.storeFloat32(input, Operand(esp, 0));

        // Check exponent to avoid fp exceptions.
        masm.branchDoubleNotInInt64Range(Address(esp, 0), output, &failPopFloat);

        // Load float, perform 32-bit truncation.
        masm.truncateFloat32ToInt64(Address(esp, 0), Address(esp, 0), output);

        // Load low word, pop 64bits and jump back.
        masm.load32(Address(esp, 0), output);
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
        masm.zeroFloat32(ScratchFloat32Reg);
        masm.vucomiss(ScratchFloat32Reg, input);
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

        masm.addFloat32(input, temp);
        masm.vcvttss2si(temp, output);
        masm.vcvtsi2ss(output, ScratchFloat32Reg, ScratchFloat32Reg);

        masm.vucomiss(ScratchFloat32Reg, temp);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.push(input);
        masm.setupUnalignedABICall(output);
        masm.vcvtss2sd(input, input, input);
        masm.passABIArg(input.asDouble(), MoveOp::DOUBLE);

        if (gen->compilingAsmJS())
            masm.callWithABI(wasm::SymbolicAddress::ToInt32);
        else
            masm.callWithABI(BitwiseCast<void*, int32_t(*)(double)>(JS::ToInt32));

        masm.storeCallResult(output);
        masm.pop(input);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
}

void
CodeGeneratorX86::visitCompareI64(LCompareI64* lir)
{
    MCompare* mir = lir->mir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register64 lhsRegs = ToRegister64(lhs);
    Register output = ToRegister(lir->output());

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    Assembler::Condition condition = JSOpToCondition(lir->jsop(), isSigned);
    Label done;

    masm.move32(Imm32(1), output);

    if (IsConstant(rhs)) {
        Imm64 imm = Imm64(ToInt64(rhs));
        masm.branch64(condition, lhsRegs, imm, &done);
    } else {
        Register64 rhsRegs = ToRegister64(rhs);
        masm.branch64(condition, lhsRegs, rhsRegs, &done);
    }

    masm.xorl(output, output);
    masm.bind(&done);
}

void
CodeGeneratorX86::visitCompareI64AndBranch(LCompareI64AndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
               mir->compareType() == MCompare::Compare_UInt64);

    const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
    const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
    Register64 lhsRegs = ToRegister64(lhs);

    bool isSigned = mir->compareType() == MCompare::Compare_Int64;
    Assembler::Condition condition = JSOpToCondition(lir->jsop(), isSigned);

    Label* trueLabel = getJumpLabelForBranch(lir->ifTrue());
    Label* falseLabel = getJumpLabelForBranch(lir->ifFalse());

    if (isNextBlock(lir->ifFalse()->lir())) {
        falseLabel = nullptr;
    } else if (isNextBlock(lir->ifTrue()->lir())) {
        condition = Assembler::InvertCondition(condition);
        trueLabel = falseLabel;
        falseLabel = nullptr;
    }

    if (IsConstant(rhs)) {
        Imm64 imm = Imm64(ToInt64(rhs));
        masm.branch64(condition, lhsRegs, imm, trueLabel, falseLabel);
    } else {
        Register64 rhsRegs = ToRegister64(rhs);
        masm.branch64(condition, lhsRegs, rhsRegs, trueLabel, falseLabel);
    }
}

void
CodeGeneratorX86::visitDivOrModI64(LDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));
    Register64 output = ToOutRegister64(lir);

    MOZ_ASSERT(output == ReturnReg64);

    // We are free to clobber all registers, since this is a call instruction.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(lhs.low);
    regs.take(lhs.high);
    if (lhs != rhs) {
        regs.take(rhs.low);
        regs.take(rhs.high);
    }
    Register temp = regs.takeAny();

    Label done;

    // Handle divide by zero.
    if (lir->canBeDivideByZero())
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, wasm::JumpTarget::IntegerDivideByZero);

    // Handle an integer overflow exception from INT64_MIN / -1.
    if (lir->canBeNegativeOverflow()) {
        Label notmin;
        masm.branch64(Assembler::NotEqual, lhs, Imm64(INT64_MIN), &notmin);
        masm.branch64(Assembler::NotEqual, rhs, Imm64(-1), &notmin);
        if (lir->mir()->isMod())
            masm.xor64(output, output);
        else
            masm.jump(wasm::JumpTarget::IntegerOverflow);
        masm.jump(&done);
        masm.bind(&notmin);
    }

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingAsmJS());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::ModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::DivI64);

    // output in edx:eax, move to output register.
    masm.movl(edx, output.high);
    MOZ_ASSERT(eax == output.low);

    masm.bind(&done);
}

void
CodeGeneratorX86::visitUDivOrModI64(LUDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));
    Register64 output = ToOutRegister64(lir);

    MOZ_ASSERT(output == ReturnReg64);

    // We are free to clobber all registers, since this is a call instruction.
    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(lhs.low);
    regs.take(lhs.high);
    if (lhs != rhs) {
        regs.take(rhs.low);
        regs.take(rhs.high);
    }
    Register temp = regs.takeAny();

    // Prevent divide by zero.
    if (lir->canBeDivideByZero())
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, wasm::JumpTarget::IntegerDivideByZero);

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingAsmJS());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::UModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::UDivI64);

    // output in edx:eax, move to output register.
    masm.movl(edx, output.high);
    MOZ_ASSERT(eax == output.low);
}

void
CodeGeneratorX86::visitAsmSelectI64(LAsmSelectI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);

    Register cond = ToRegister(lir->condExpr());
    Register64 trueExpr = ToRegister64(lir->trueExpr());
    Register64 falseExpr = ToRegister64(lir->falseExpr());
    Register64 out = ToOutRegister64(lir);

    masm.move64(trueExpr, out);

    Label done;
    masm.branchTest32(Assembler::NonZero, cond, cond, &done);
    masm.movl(falseExpr.low, out.low);
    masm.movl(falseExpr.high, out.high);
    masm.bind(&done);
}

void
CodeGeneratorX86::visitAsmReinterpretFromI64(LAsmReinterpretFromI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
    Register64 input = ToRegister64(lir->getInt64Operand(0));

    masm.Push(input.high);
    masm.Push(input.low);
    masm.vmovq(Operand(esp, 0), ToFloatRegister(lir->output()));
    masm.freeStack(sizeof(uint64_t));
}

void
CodeGeneratorX86::visitAsmReinterpretToI64(LAsmReinterpretToI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
    Register64 output = ToOutRegister64(lir);

    masm.reserveStack(sizeof(uint64_t));
    masm.vmovq(ToFloatRegister(lir->input()), Operand(esp, 0));
    masm.Pop(output.low);
    masm.Pop(output.high);
}

void
CodeGeneratorX86::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir)
{
    Register64 output = ToOutRegister64(lir);
    Register input = ToRegister(lir->input());

    if (lir->mir()->isUnsigned()) {
        if (output.low != input)
            masm.movl(input, output.low);
        masm.xorl(output.high, output.high);
    } else {
        MOZ_ASSERT(output.low == input);
        MOZ_ASSERT(output.low == eax);
        MOZ_ASSERT(output.high == edx);
        masm.cdq();
    }
}

void
CodeGeneratorX86::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir)
{
    const LInt64Allocation& input = lir->getInt64Operand(0);
    Register output = ToRegister(lir->output());

    if (lir->mir()->bottomHalf())
        masm.movl(ToRegister(input.low()), output);
    else
        masm.movl(ToRegister(input.high()), output);
}

void
CodeGeneratorX86::visitClzI64(LClzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);

    masm.clz64(input, output.low);
    masm.xorl(output.high, output.high);
}

void
CodeGeneratorX86::visitCtzI64(LCtzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);

    masm.ctz64(input, output.low);
    masm.xorl(output.high, output.high);
}

void
CodeGeneratorX86::visitNotI64(LNotI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register output = ToRegister(lir->output());

    if (input.high == output) {
        masm.orl(input.low, output);
    } else if (input.low == output) {
        masm.orl(input.high, output);
    } else {
        masm.movl(input.high, output);
        masm.orl(input.low, output);
    }

    masm.cmpl(Imm32(0), output);
    masm.emitSet(Assembler::Equal, output);
}

void
CodeGeneratorX86::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register64 output = ToOutRegister64(lir);

    MWasmTruncateToInt64* mir = lir->mir();
    Register temp = ToRegister(lir->temp1());
    FloatRegister floatTemp = ToFloatRegister(lir->temp2());

    Label fail, convert;

    MOZ_ASSERT (mir->input()->type() == MIRType::Double || mir->input()->type() == MIRType::Float32);

    auto* ool = new(alloc()) OutOfLineWasmTruncateCheck(mir, input);
    addOutOfLineCode(ool, mir);

    masm.reserveStack(2 * sizeof(int32_t));
    masm.storeDouble(input, Operand(esp, 0));

    // Make sure input fits in (u)int64.
    if (mir->input()->type() == MIRType::Float32) {
        if (mir->isUnsigned())
            masm.branchFloat32NotInUInt64Range(Address(esp, 0), temp, &fail);
        else
            masm.branchFloat32NotInInt64Range(Address(esp, 0), temp, &fail);
    } else {
        if (mir->isUnsigned())
            masm.branchDoubleNotInUInt64Range(Address(esp, 0), temp, &fail);
        else
            masm.branchDoubleNotInInt64Range(Address(esp, 0), temp, &fail);
    }
    masm.jump(&convert);

    // Handle failure in ool.
    masm.bind(&fail);
    masm.freeStack(2 * sizeof(int32_t));
    masm.jump(ool->entry());
    masm.bind(ool->rejoin());
    masm.reserveStack(2 * sizeof(int32_t));
    masm.storeDouble(input, Operand(esp, 0));

    // Convert the double/float to int64.
    masm.bind(&convert);
    if (mir->input()->type() == MIRType::Float32) {
        if (mir->isUnsigned())
            masm.truncateFloat32ToUInt64(Address(esp, 0), Address(esp, 0), temp, floatTemp);
        else
            masm.truncateFloat32ToInt64(Address(esp, 0), Address(esp, 0), temp);
    } else {
        if (mir->isUnsigned())
            masm.truncateDoubleToUInt64(Address(esp, 0), Address(esp, 0), temp, floatTemp);
        else
            masm.truncateDoubleToInt64(Address(esp, 0), Address(esp, 0), temp);
    }

    // Load value into int64 register.
    masm.load64(Address(esp, 0), output);

    masm.freeStack(2 * sizeof(int32_t));
}

void
CodeGeneratorX86::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    MIRType outputType = lir->mir()->type();
    MOZ_ASSERT(outputType == MIRType::Double || outputType == MIRType::Float32);

    // Zero the output register to break dependencies, see convertInt32ToDouble.
    if (outputType == MIRType::Double)
        masm.zeroDouble(output);
    else
        masm.zeroFloat32(output);

    masm.Push(input.high);
    masm.Push(input.low);
    masm.fild(Operand(esp, 0));

    if (lir->mir()->isUnsigned()) {
        Label notNegative;
        masm.branch32(Assembler::NotSigned, input.high, Imm32(0), &notNegative);
        double add_constant = 18446744073709551616.0; // 2^64
        masm.store64(Imm64(mozilla::BitwiseCast<uint64_t>(add_constant)), Address(esp, 0));
        masm.fld(Operand(esp, 0));
        masm.faddp();
        masm.bind(&notNegative);
    }

    masm.fstp(Operand(esp, 0));
    masm.vmovsd(Address(esp, 0), output);
    masm.freeStack(2*sizeof(intptr_t));

    if (outputType == MIRType::Float32)
        masm.convertDoubleToFloat32(output, output);
}

void
CodeGeneratorX86::visitTestI64AndBranch(LTestI64AndBranch* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));

    masm.testl(input.high, input.high);
    jumpToBlock(lir->ifTrue(), Assembler::NonZero);
    masm.testl(input.low, input.low);
    emitBranch(Assembler::NonZero, lir->ifTrue(), lir->ifFalse());
}
