/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsnum.h"

#include "CodeGenerator-x64.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "vm/Shape.h"

#include "vm/Shape-inl.h"

using namespace js;
using namespace js::ion;

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
    JS_NOT_REACHED("x64 does not use frame size classes");
    return 0;
}

bool
CodeGeneratorX64::visitValue(LValue *value)
{
    LDefinition *reg = value->getDef(0);
    masm.moveValue(value->value(), ToRegister(reg));
    return true;
}

bool
CodeGeneratorX64::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame  = value->getOperand(0);
    const LDefinition *target = value->getDef(0);

    const ptrdiff_t valueOffset = value->mir()->frameOffset();

    masm.movq(Operand(ToRegister(frame), valueOffset), ToRegister(target));

    return true;
}

bool
CodeGeneratorX64::visitBox(LBox *box)
{
    const LAllocation *in = box->getOperand(0);
    const LDefinition *result = box->getDef(0);

    if (box->type() != MIRType_Double)
        masm.boxValue(ValueTypeFromMIRType(box->type()), ToRegister(in), ToRegister(result));
    else
        masm.movqsd(ToFloatRegister(in), ToRegister(result));
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
          default:
            JS_NOT_REACHED("Given MIRType cannot be unboxed.");
            return false;
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
      default:
        JS_NOT_REACHED("Given MIRType cannot be unboxed.");
        break;
    }
    
    return true;
}

bool
CodeGeneratorX64::visitLoadSlotV(LLoadSlotV *load)
{
    Register dest = ToRegister(load->outputValue());
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    masm.movq(Operand(base, offset), dest);
    return true;
}

void
CodeGeneratorX64::loadUnboxedValue(Operand source, MIRType type, const LDefinition *dest)
{
    switch (type) {
      case MIRType_Double:
        masm.loadInt32OrDouble(source, ToFloatRegister(dest));
        break;

      case MIRType_Object:
      case MIRType_String:
        masm.unboxObject(source, ToRegister(dest));
        break;

      case MIRType_Int32:
      case MIRType_Boolean:
        masm.movl(source, ToRegister(dest));
        break;

      default:
        JS_NOT_REACHED("unexpected type");
    }
}

bool
CodeGeneratorX64::visitLoadSlotT(LLoadSlotT *load)
{
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    loadUnboxedValue(Operand(base, offset), load->mir()->type(), load->output());

    return true;
}

void
CodeGeneratorX64::storeUnboxedValue(const LAllocation *value, MIRType valueType,
                                    Operand dest, MIRType slotType)
{
    if (valueType == MIRType_Double) {
        masm.movsd(ToFloatRegister(value), dest);
        return;
    }

    // For known integers and booleans, we can just store the unboxed value if
    // the slot has the same type.
    if ((valueType == MIRType_Int32 || valueType == MIRType_Boolean) && slotType == valueType) {
        if (value->isConstant()) {
            Value val = *value->toConstant();
            if (valueType == MIRType_Int32)
                masm.movl(Imm32(val.toInt32()), dest);
            else
                masm.movl(Imm32(val.toBoolean() ? 1 : 0), dest);
        } else {
            masm.movl(ToRegister(value), dest);
        }
        return;
    }

    if (value->isConstant()) {
        masm.moveValue(*value->toConstant(), ScratchReg);
        masm.movq(ScratchReg, dest);
    } else {
        masm.storeValue(ValueTypeFromMIRType(valueType), ToRegister(value), dest);
    }
}

bool
CodeGeneratorX64::visitStoreSlotT(LStoreSlotT *store)
{
    Register base = ToRegister(store->slots());
    int32_t offset = store->mir()->slot() * sizeof(js::Value);

    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();
    MIRType slotType = store->mir()->slotType();

    if (store->mir()->needsBarrier())
        emitPreBarrier(Address(base, offset), slotType);

    storeUnboxedValue(value, valueType, Operand(base, offset), slotType);
    return true;
}

bool
CodeGeneratorX64::visitLoadElementT(LLoadElementT *load)
{
    Operand source = createArrayElementOperand(ToRegister(load->elements()), load->index());

    if (load->mir()->loadDoubles()) {
        FloatRegister fpreg = ToFloatRegister(load->output());
        if (source.kind() == Operand::REG_DISP)
            masm.loadDouble(source.toAddress(), fpreg);
        else
            masm.loadDouble(source.toBaseIndex(), fpreg);
    } else {
        loadUnboxedValue(source, load->mir()->type(), load->output());
    }

    JS_ASSERT(!load->mir()->needsHoleCheck());
    return true;
}


void
CodeGeneratorX64::storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                                    const Register &elements, const LAllocation *index)
{
    Operand dest = createArrayElementOperand(elements, index);
    storeUnboxedValue(value, valueType, dest, elementType);
}

bool
CodeGeneratorX64::visitImplicitThis(LImplicitThis *lir)
{
    Register callee = ToRegister(lir->callee());

    // The implicit |this| is always |undefined| if the function's environment
    // is the current global.
    GlobalObject *global = &gen->info().script()->global();
    masm.cmpPtr(Operand(callee, JSFunction::offsetOfEnvironment()), ImmGCPtr(global));

    // TODO: OOL stub path.
    if (!bailoutIf(Assembler::NotEqual, lir->snapshot()))
        return false;

    masm.moveValue(UndefinedValue(), ToOutValue(lir));
    return true;
}

typedef bool (*InterruptCheckFn)(JSContext *);
static const VMFunction InterruptCheckInfo = FunctionInfo<InterruptCheckFn>(InterruptCheck);

bool
CodeGeneratorX64::visitInterruptCheck(LInterruptCheck *lir)
{
    OutOfLineCode *ool = oolCallVM(InterruptCheckInfo, lir, (ArgList()), StoreNothing());
    if (!ool)
        return false;

    void *interrupt = (void*)&gen->compartment->rt->interrupt;
    masm.movq(ImmWord(interrupt), ScratchReg);
    masm.cmpl(Operand(ScratchReg, 0), Imm32(0));
    masm.j(Assembler::NonZero, ool->entry());
    masm.bind(ool->rejoin());
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
    MCompare *mir = lir->mir();

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
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
              mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    masm.cmpq(lhs.valueReg(), rhs.valueReg());
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGeneratorX64::visitUInt32ToDouble(LUInt32ToDouble *lir)
{
    masm.convertUInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGeneratorX64::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    MAsmJSLoadHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Operand srcAddr(HeapReg, ToRegister(ins->ptr()), TimesOne);

    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        FloatRegister dest = ToFloatRegister(ins->output());
        uint32_t before = masm.size();
        masm.movss(srcAddr, dest);
        uint32_t after = masm.size();
        masm.cvtss2sd(dest, dest);
        return gen->noteHeapAccess(AsmJSHeapAccess(before, after, vt, ToAnyRegister(ins->output())));
    }

    uint32_t before = masm.size();
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movxbl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_UINT8:   masm.movzbl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_INT16:   masm.movxwl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movzwl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_INT32:   masm.movl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_UINT32:  masm.movl(srcAddr, ToRegister(ins->output())); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsd(srcAddr, ToFloatRegister(ins->output())); break;
      default: JS_NOT_REACHED("unexpected array type");
    }
    uint32_t after = masm.size();
    return gen->noteHeapAccess(AsmJSHeapAccess(before, after, vt, ToAnyRegister(ins->output())));
}

bool
CodeGeneratorX64::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Operand dstAddr(HeapReg, ToRegister(ins->ptr()), TimesOne);

    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        masm.convertDoubleToFloat(ToFloatRegister(ins->value()), ScratchFloatReg);
        uint32_t before = masm.size();
        masm.movss(ScratchFloatReg, dstAddr);
        uint32_t after = masm.size();
        return gen->noteHeapAccess(AsmJSHeapAccess(before, after));
    }

    uint32_t before = masm.size();
    if (ins->value()->isConstant()) {
        switch (vt) {
          case ArrayBufferView::TYPE_INT8:    masm.movb(Imm32(ToInt32(ins->value())), dstAddr); break;
          case ArrayBufferView::TYPE_UINT8:   masm.movb(Imm32(ToInt32(ins->value())), dstAddr); break;
          case ArrayBufferView::TYPE_INT16:   masm.movw(Imm32(ToInt32(ins->value())), dstAddr); break;
          case ArrayBufferView::TYPE_UINT16:  masm.movw(Imm32(ToInt32(ins->value())), dstAddr); break;
          case ArrayBufferView::TYPE_INT32:   masm.movl(Imm32(ToInt32(ins->value())), dstAddr); break;
          case ArrayBufferView::TYPE_UINT32:  masm.movl(Imm32(ToInt32(ins->value())), dstAddr); break;
          default: JS_NOT_REACHED("unexpected array type");
        }
    } else {
        switch (vt) {
          case ArrayBufferView::TYPE_INT8:    masm.movb(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_UINT8:   masm.movb(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_INT16:   masm.movw(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_UINT16:  masm.movw(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_INT32:   masm.movl(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_UINT32:  masm.movl(ToRegister(ins->value()), dstAddr); break;
          case ArrayBufferView::TYPE_FLOAT64: masm.movsd(ToFloatRegister(ins->value()), dstAddr); break;
          default: JS_NOT_REACHED("unexpected array type");
        }
    }
    uint32_t after = masm.size();
    return gen->noteHeapAccess(AsmJSHeapAccess(before, after));
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

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

bool
CodeGeneratorX64::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double);

    CodeOffsetLabel label;
    if (type == MIRType_Int32)
        label = masm.storeRipRelativeInt32(ToRegister(ins->value()));
    else
        label = masm.storeRipRelativeDouble(ToFloatRegister(ins->value()));

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
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

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

bool
CodeGeneratorX64::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    CodeOffsetLabel label = masm.loadRipRelativeInt64(ToRegister(ins->output()));

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

