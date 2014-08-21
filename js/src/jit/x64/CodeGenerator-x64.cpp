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

bool
CodeGeneratorX64::visitValue(LValue *value)
{
    LDefinition *reg = value->getDef(0);
    masm.moveValue(value->value(), ToRegister(reg));
    return true;
}

bool
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
    return true;
}

bool
CodeGeneratorX64::visitUnbox(LUnbox *unbox)
{
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    const LDefinition *result = unbox->output();
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
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
        if (!bailoutIf(cond, unbox->snapshot()))
            return false;
    }

    switch (mir->type()) {
      case MIRType_Int32:
        masm.unboxInt32(value, ToRegister(result));
        break;
      case MIRType_Boolean:
        masm.unboxBoolean(value, ToRegister(result));
        break;
      case MIRType_Object:
        masm.unboxObject(value, ToRegister(result));
        break;
      case MIRType_String:
        masm.unboxString(value, ToRegister(result));
        break;
      case MIRType_Symbol:
        masm.unboxSymbol(value, ToRegister(result));
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }

    return true;
}

bool
CodeGeneratorX64::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    if (rhs->isConstant())
        masm.moveValue(*rhs->toConstant(), ScratchReg);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), ScratchReg);

    // Perform the comparison.
    masm.cmpq(lhs.valueReg(), ScratchReg);
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
    return true;
}

bool
CodeGeneratorX64::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    // Load boxed boolean in ScratchReg.
    if (rhs->isConstant())
        masm.moveValue(*rhs->toConstant(), ScratchReg);
    else
        masm.boxValue(JSVAL_TYPE_BOOLEAN, ToRegister(rhs), ScratchReg);

    // Perform the comparison.
    masm.cmpq(lhs.valueReg(), ScratchReg);
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}
bool
CodeGeneratorX64::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    JS_ASSERT(IsEqualityOp(mir->jsop()));

    masm.cmpq(lhs.valueReg(), rhs.valueReg());
    masm.emitSet(JSOpToCondition(mir->compareType(), mir->jsop()), output);
    return true;
}

bool
CodeGeneratorX64::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->cmpMir();

    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
              mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    masm.cmpq(lhs.valueReg(), rhs.valueReg());
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGeneratorX64::visitAsmJSUInt32ToDouble(LAsmJSUInt32ToDouble *lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSUInt32ToFloat32(LAsmJSUInt32ToFloat32 *lir)
{
    masm.convertUInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorX64::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    MOZ_CRASH("NYI");
}

bool
CodeGeneratorX64::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MOZ_CRASH("NYI");
}

bool
CodeGeneratorX64::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    MAsmJSLoadHeap *mir = ins->mir();
    Scalar::Type vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    const LDefinition *out = ins->output();
    Operand srcAddr(HeapReg);

    if (ptr->isConstant()) {
        int32_t ptrImm = ptr->toConstant()->toInt32();
        JS_ASSERT(ptrImm >= 0);
        srcAddr = Operand(HeapReg, ptrImm);
    } else {
        srcAddr = Operand(HeapReg, ToRegister(ptr), TimesOne);
    }

    OutOfLineLoadTypedArrayOutOfBounds *ool = nullptr;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (!mir->skipBoundsCheck()) {
        bool isFloat32Load = vt == Scalar::Float32;
        ool = new(alloc()) OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out), isFloat32Load);
        if (!addOutOfLineCode(ool, ins->mir()))
            return false;

        CodeOffsetLabel cmp = masm.cmplWithPatch(ToRegister(ptr), Imm32(0));
        masm.j(Assembler::AboveOrEqual, ool->entry());
        maybeCmpOffset = cmp.offset();
    }

    uint32_t before = masm.size();
    switch (vt) {
      case Scalar::Int8:    masm.movsbl(srcAddr, ToRegister(out)); break;
      case Scalar::Uint8:   masm.movzbl(srcAddr, ToRegister(out)); break;
      case Scalar::Int16:   masm.movswl(srcAddr, ToRegister(out)); break;
      case Scalar::Uint16:  masm.movzwl(srcAddr, ToRegister(out)); break;
      case Scalar::Int32:
      case Scalar::Uint32:  masm.movl(srcAddr, ToRegister(out)); break;
      case Scalar::Float32: masm.loadFloat32(srcAddr, ToFloatRegister(out)); break;
      case Scalar::Float64: masm.loadDouble(srcAddr, ToFloatRegister(out)); break;
      default: MOZ_CRASH("unexpected array type");
    }
    uint32_t after = masm.size();
    if (ool)
        masm.bind(ool->rejoin());
    masm.append(AsmJSHeapAccess(before, after, vt, ToAnyRegister(out), maybeCmpOffset));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    Scalar::Type vt = mir->viewType();
    const LAllocation *ptr = ins->ptr();
    Operand dstAddr(HeapReg);

    if (ptr->isConstant()) {
        int32_t ptrImm = ptr->toConstant()->toInt32();
        JS_ASSERT(ptrImm >= 0);
        dstAddr = Operand(HeapReg, ptrImm);
    } else {
        dstAddr = Operand(HeapReg, ToRegister(ptr), TimesOne);
    }

    Label rejoin;
    uint32_t maybeCmpOffset = AsmJSHeapAccess::NoLengthCheck;
    if (!mir->skipBoundsCheck()) {
        CodeOffsetLabel cmp = masm.cmplWithPatch(ToRegister(ptr), Imm32(0));
        masm.j(Assembler::AboveOrEqual, &rejoin);
        maybeCmpOffset = cmp.offset();
    }

    uint32_t before = masm.size();
    if (ins->value()->isConstant()) {
        switch (vt) {
          case Scalar::Int8:
          case Scalar::Uint8:   masm.movb(Imm32(ToInt32(ins->value())), dstAddr); break;
          case Scalar::Int16:
          case Scalar::Uint16:  masm.movw(Imm32(ToInt32(ins->value())), dstAddr); break;
          case Scalar::Int32:
          case Scalar::Uint32:  masm.movl(Imm32(ToInt32(ins->value())), dstAddr); break;
          default: MOZ_CRASH("unexpected array type");
        }
    } else {
        switch (vt) {
          case Scalar::Int8:
          case Scalar::Uint8:   masm.movb(ToRegister(ins->value()), dstAddr); break;
          case Scalar::Int16:
          case Scalar::Uint16:  masm.movw(ToRegister(ins->value()), dstAddr); break;
          case Scalar::Int32:
          case Scalar::Uint32:  masm.movl(ToRegister(ins->value()), dstAddr); break;
          case Scalar::Float32: masm.storeFloat32(ToFloatRegister(ins->value()), dstAddr); break;
          case Scalar::Float64: masm.storeDouble(ToFloatRegister(ins->value()), dstAddr); break;
          default: MOZ_CRASH("unexpected array type");
        }
    }
    uint32_t after = masm.size();
    if (rejoin.used())
        masm.bind(&rejoin);
    masm.append(AsmJSHeapAccess(before, after, maybeCmpOffset));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    MAsmJSLoadGlobalVar *mir = ins->mir();

    CodeOffsetLabel label;
    if (mir->type() == MIRType_Int32)
        label = masm.loadRipRelativeInt32(ToRegister(ins->output()));
    else
        label = masm.loadRipRelativeDouble(ToFloatRegister(ins->output()));
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    JS_ASSERT(IsNumberType(type));

    CodeOffsetLabel label;
    if (type == MIRType_Int32)
        label = masm.storeRipRelativeInt32(ToRegister(ins->value()));
    else
        label = masm.storeRipRelativeDouble(ToFloatRegister(ins->value()));
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register tmp = ToRegister(ins->temp());
    Register out = ToRegister(ins->output());

    CodeOffsetLabel label = masm.leaRipRelative(tmp);
    masm.loadPtr(Operand(tmp, index, TimesEight, 0), out);
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    CodeOffsetLabel label = masm.loadRipRelativeInt64(ToRegister(ins->output()));
    masm.append(AsmJSGlobalAccess(label, mir->globalDataOffset()));
    return true;
}

void
DispatchIonCache::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // Can always use the scratch register on x64.
    addState->dispatchScratch = ScratchReg;
}

bool
CodeGeneratorX64::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateDouble uses cvttsd2sq. Unlike the x86
    // implementation, this should handle most doubles and we can just
    // call a stub if it fails.
    return emitTruncateDouble(input, output, ins->mir());
}

bool
CodeGeneratorX64::visitTruncateFToInt32(LTruncateFToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    // On x64, branchTruncateFloat32 uses cvttss2sq. Unlike the x86
    // implementation, this should handle most floats and we can just
    // call a stub if it fails.
    return emitTruncateFloat32(input, output, ins->mir());
}
