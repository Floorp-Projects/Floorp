/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "CodeGenerator-x86.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"
#include "ion/ExecutionModeInlines.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;

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
    JS_ASSERT(class_ != NO_FRAME_SIZE_CLASS_ID);
    JS_ASSERT(class_ < JS_ARRAY_LENGTH(FrameSizes));

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

bool
CodeGeneratorX86::visitValue(LValue *value)
{
    const ValueOperand out = ToOutValue(value);
    masm.moveValue(value->value(), out);
    return true;
}

bool
CodeGeneratorX86::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame   = value->getOperand(0);
    const ValueOperand out     = ToOutValue(value);
    const ptrdiff_t frameOffset = value->mir()->frameOffset();

    masm.loadValue(Operand(ToRegister(frame), frameOffset), out);
    return true;
}

bool
CodeGeneratorX86::visitBox(LBox *box)
{
    const LDefinition *type = box->getDef(TYPE_INDEX);

    DebugOnly<const LAllocation *> a = box->getOperand(0);
    JS_ASSERT(!a->isConstant());

    // On x86, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.movl(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
    return true;
}

bool
CodeGeneratorX86::visitBoxDouble(LBoxDouble *box)
{
    const LAllocation *in = box->getOperand(0);
    const ValueOperand out = ToOutValue(box);

    masm.boxDouble(ToFloatRegister(in), out);
    return true;
}

bool
CodeGeneratorX86::visitUnbox(LUnbox *unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox *mir = unbox->mir();

    if (mir->fallible()) {
        masm.cmpl(ToOperand(unbox->type()), Imm32(MIRTypeToTag(mir->type())));
        if (!bailoutIf(Assembler::NotEqual, unbox->snapshot()))
            return false;
    }
    return true;
}

bool
CodeGeneratorX86::visitLoadSlotV(LLoadSlotV *load)
{
    const ValueOperand out = ToOutValue(load);
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    masm.loadValue(Operand(base, offset), out);
    return true;
}

bool
CodeGeneratorX86::visitLoadSlotT(LLoadSlotT *load)
{
    Register base = ToRegister(load->input());
    int32_t offset = load->mir()->slot() * sizeof(js::Value);

    if (load->mir()->type() == MIRType_Double)
        masm.loadInt32OrDouble(Operand(base, offset), ToFloatRegister(load->output()));
    else
        masm.movl(Operand(base, offset + NUNBOX32_PAYLOAD_OFFSET), ToRegister(load->output()));
    return true;
}

bool
CodeGeneratorX86::visitStoreSlotT(LStoreSlotT *store)
{
    Register base = ToRegister(store->slots());
    int32_t offset = store->mir()->slot() * sizeof(js::Value);

    const LAllocation *value = store->value();
    MIRType valueType = store->mir()->value()->type();

    if (store->mir()->needsBarrier())
        emitPreBarrier(Address(base, offset), store->mir()->slotType());

    if (valueType == MIRType_Double) {
        masm.movsd(ToFloatRegister(value), Operand(base, offset));
        return true;
    }

    // Store the type tag if needed.
    if (valueType != store->mir()->slotType())
        masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), Operand(base, offset));

    // Store the payload.
    if (value->isConstant())
        masm.storePayload(*value->toConstant(), Operand(base, offset));
    else
        masm.storePayload(ToRegister(value), Operand(base, offset));

    return true;
}

bool
CodeGeneratorX86::visitLoadElementT(LLoadElementT *load)
{
    Operand source = createArrayElementOperand(ToRegister(load->elements()), load->index());

    if (load->mir()->needsHoleCheck()) {
        Assembler::Condition cond = masm.testMagic(Assembler::Equal, source);
        if (!bailoutIf(cond, load->snapshot()))
            return false;
    }

    if (load->mir()->type() == MIRType_Double) {
        FloatRegister fpreg = ToFloatRegister(load->output());
        if (load->mir()->loadDoubles()) {
            if (source.kind() == Operand::REG_DISP)
                masm.loadDouble(source.toAddress(), fpreg);
            else
                masm.loadDouble(source.toBaseIndex(), fpreg);
        } else {
            masm.loadInt32OrDouble(source, fpreg);
        }
    } else {
        masm.movl(masm.ToPayload(source), ToRegister(load->output()));
    }

    return true;
}

void
CodeGeneratorX86::storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                                    const Register &elements, const LAllocation *index)
{
    Operand dest = createArrayElementOperand(elements, index);

    if (valueType == MIRType_Double) {
        masm.movsd(ToFloatRegister(value), dest);
        return;
    }

    // Store the type tag if needed.
    if (valueType != elementType)
        masm.storeTypeTag(ImmType(ValueTypeFromMIRType(valueType)), dest);

    // Store the payload.
    if (value->isConstant())
        masm.storePayload(*value->toConstant(), dest);
    else
        masm.storePayload(ToRegister(value), dest);
}

bool
CodeGeneratorX86::visitImplicitThis(LImplicitThis *lir)
{
    Register callee = ToRegister(lir->callee());
    const ValueOperand out = ToOutValue(lir);

    // The implicit |this| is always |undefined| if the function's environment
    // is the current global.
    GlobalObject *global = &gen->info().script()->global();
    masm.cmpPtr(Operand(callee, JSFunction::offsetOfEnvironment()), ImmGCPtr(global));

    // TODO: OOL stub path.
    if (!bailoutIf(Assembler::NotEqual, lir->snapshot()))
        return false;

    masm.moveValue(UndefinedValue(), out);
    return true;
}

typedef bool (*InterruptCheckFn)(JSContext *);
static const VMFunction InterruptCheckInfo = FunctionInfo<InterruptCheckFn>(InterruptCheck);

bool
CodeGeneratorX86::visitInterruptCheck(LInterruptCheck *lir)
{
    OutOfLineCode *ool = oolCallVM(InterruptCheckInfo, lir, (ArgList()), StoreNothing());
    if (!ool)
        return false;

    void *interrupt = (void*)&gen->compartment->rt->interrupt;
    masm.cmpl(Operand(interrupt), Imm32(0));
    masm.j(Assembler::NonZero, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitCompareB(LCompareB *lir)
{
    MCompare *mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation *rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

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
    return true;
}

bool
CodeGeneratorX86::visitCompareBAndBranch(LCompareBAndBranch *lir)
{
    MCompare *mir = lir->mir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation *rhs = lir->rhs();

    JS_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    if (mir->jsop() == JSOP_STRICTEQ)
        masm.branchTestBoolean(Assembler::NotEqual, lhs, lir->ifFalse()->lir()->label());
    else
        masm.branchTestBoolean(Assembler::NotEqual, lhs, lir->ifTrue()->lir()->label());

    if (rhs->isConstant())
        masm.cmp32(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()));
    else
        masm.cmp32(lhs.payloadReg(), ToRegister(rhs));
    emitBranch(JSOpToCondition(mir->compareType(), mir->jsop()), lir->ifTrue(), lir->ifFalse());
    return true;
}

bool
CodeGeneratorX86::visitCompareV(LCompareV *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareV::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareV::RhsInput);
    const Register output = ToRegister(lir->output());

    JS_ASSERT(IsEqualityOp(mir->jsop()));

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
    return true;
}

bool
CodeGeneratorX86::visitCompareVAndBranch(LCompareVAndBranch *lir)
{
    MCompare *mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareVAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareVAndBranch::RhsInput);

    JS_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
              mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    Label *notEqual;
    if (cond == Assembler::Equal)
        notEqual = lir->ifFalse()->lir()->label();
    else
        notEqual = lir->ifTrue()->lir()->label();

    masm.cmp32(lhs.typeReg(), rhs.typeReg());
    masm.j(Assembler::NotEqual, notEqual);
    masm.cmp32(lhs.payloadReg(), rhs.payloadReg());
    emitBranch(cond, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGeneratorX86::visitUInt32ToDouble(LUInt32ToDouble *lir)
{
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());

    if (input != temp)
        masm.mov(input, temp);

    // Beware: convertUInt32ToDouble clobbers input.
    masm.convertUInt32ToDouble(temp, ToFloatRegister(lir->output()));
    return true;
}

class ion::OutOfLineAsmJSLoadHeapOutOfBounds : public OutOfLineCodeBase<CodeGeneratorX86>
{
    AnyRegister dest_;
  public:
    OutOfLineAsmJSLoadHeapOutOfBounds(AnyRegister dest) : dest_(dest) {}
    const AnyRegister &dest() const { return dest_; }
    bool accept(CodeGeneratorX86 *codegen) { return codegen->visitOutOfLineAsmJSLoadHeapOutOfBounds(this); }
};

bool
CodeGeneratorX86::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    const MAsmJSLoadHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LDefinition *out = ins->output();

    OutOfLineAsmJSLoadHeapOutOfBounds *ool = new OutOfLineAsmJSLoadHeapOutOfBounds(ToAnyRegister(out));
    if (!addOutOfLineCode(ool))
        return false;

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptr, Imm32(0));
    masm.j(Assembler::AboveOrEqual, ool->entry());

    Address srcAddr(ptr, 0);
    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        FloatRegister dest = ToFloatRegister(out);
        uint32_t before = masm.size();
        masm.movssWithPatch(srcAddr, dest);
        uint32_t after = masm.size();
        masm.cvtss2sd(dest, dest);
        masm.bind(ool->rejoin());
        return gen->noteHeapAccess(AsmJSHeapAccess(cmp.offset(), before, after, vt, AnyRegister(dest)));
    }
    uint32_t before = masm.size();
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movxblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT8:   masm.movzblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT16:   masm.movxwlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movzwlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT32:   masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(srcAddr, ToFloatRegister(out)); break;
      default: JS_NOT_REACHED("unexpected array type");
    }
    uint32_t after = masm.size();
    masm.bind(ool->rejoin());
    return gen->noteHeapAccess(AsmJSHeapAccess(cmp.offset(), before, after, vt, ToAnyRegister(out)));
}

bool
CodeGeneratorX86::visitOutOfLineAsmJSLoadHeapOutOfBounds(OutOfLineAsmJSLoadHeapOutOfBounds *ool)
{
    if (ool->dest().isFloat())
        masm.movsd(&js_NaN, ool->dest().fpu());
    else
        masm.movl(Imm32(0), ool->dest().gpr());
    masm.jmp(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    MAsmJSStoreHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LAllocation *value = ins->value();

    CodeOffsetLabel cmp = masm.cmplWithPatch(ptr, Imm32(0));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    Address dstAddr(ptr, 0);
    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        masm.convertDoubleToFloat(ToFloatRegister(value), ScratchFloatReg);
        uint32_t before = masm.size();
        masm.movssWithPatch(ScratchFloatReg, dstAddr);
        uint32_t after = masm.size();
        masm.bind(&rejoin);
        return gen->noteHeapAccess(AsmJSHeapAccess(cmp.offset(), before, after));
    }
    uint32_t before = masm.size();
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT8:   masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT16:   masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT32:   masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(ToFloatRegister(value), dstAddr); break;
      default: JS_NOT_REACHED("unexpected array type");
    }
    uint32_t after = masm.size();
    masm.bind(&rejoin);
    return gen->noteHeapAccess(AsmJSHeapAccess(cmp.offset(), before, after));
}

bool
CodeGeneratorX86::visitAsmJSLoadGlobalVar(LAsmJSLoadGlobalVar *ins)
{
    MAsmJSLoadGlobalVar *mir = ins->mir();

    CodeOffsetLabel label;
    if (mir->type() == MIRType_Int32)
        label = masm.movlWithPatch(NULL, ToRegister(ins->output()));
    else
        label = masm.movsdWithPatch(NULL, ToFloatRegister(ins->output()));

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

bool
CodeGeneratorX86::visitAsmJSStoreGlobalVar(LAsmJSStoreGlobalVar *ins)
{
    MAsmJSStoreGlobalVar *mir = ins->mir();

    MIRType type = mir->value()->type();
    JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double);

    CodeOffsetLabel label;
    if (type == MIRType_Int32)
        label = masm.movlWithPatch(ToRegister(ins->value()), NULL);
    else
        label = masm.movsdWithPatch(ToFloatRegister(ins->value()), NULL);

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

bool
CodeGeneratorX86::visitAsmJSLoadFuncPtr(LAsmJSLoadFuncPtr *ins)
{
    MAsmJSLoadFuncPtr *mir = ins->mir();

    Register index = ToRegister(ins->index());
    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(NULL, index, TimesFour, out);

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

bool
CodeGeneratorX86::visitAsmJSLoadFFIFunc(LAsmJSLoadFFIFunc *ins)
{
    MAsmJSLoadFFIFunc *mir = ins->mir();

    Register out = ToRegister(ins->output());
    CodeOffsetLabel label = masm.movlWithPatch(NULL, out);

    return gen->noteGlobalAccess(label.offset(), mir->globalDataOffset());
}

void
CodeGeneratorX86::postAsmJSCall(LAsmJSCall *lir)
{
    MAsmJSCall *mir = lir->mir();
    if (mir->type() != MIRType_Double || mir->callee().which() != MAsmJSCall::Callee::Builtin)
        return;

    masm.reserveStack(sizeof(double));
    masm.fstp(Operand(esp, 0));
    masm.movsd(Operand(esp, 0), ReturnFloatReg);
    masm.freeStack(sizeof(double));
}
