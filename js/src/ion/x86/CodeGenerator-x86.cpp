/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsnum.h"

#include "ion/x86/CodeGenerator-x86.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "vm/Shape.h"

#include "jsscriptinlines.h"
#include "ion/ExecutionModeInlines.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;
using mozilla::DoubleExponentBias;
using mozilla::DoubleExponentShift;

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

// Load a NaN or zero into a register for an out of bounds AsmJS or static
// typed array load.
class ion::OutOfLineLoadTypedArrayOutOfBounds : public OutOfLineCodeBase<CodeGeneratorX86>
{
    AnyRegister dest_;
  public:
    OutOfLineLoadTypedArrayOutOfBounds(AnyRegister dest) : dest_(dest) {}
    const AnyRegister &dest() const { return dest_; }
    bool accept(CodeGeneratorX86 *codegen) { return codegen->visitOutOfLineLoadTypedArrayOutOfBounds(this); }
};

void
CodeGeneratorX86::loadViewTypeElement(ArrayBufferView::ViewType vt, const Address &srcAddr,
                                      const LDefinition *out)
{
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movsblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT8_CLAMPED:
      case ArrayBufferView::TYPE_UINT8:   masm.movzblWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT16:   masm.movswlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movzwlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_INT32:   masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(srcAddr, ToRegister(out)); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(srcAddr, ToFloatRegister(out)); break;
      default: MOZ_ASSUME_NOT_REACHED("unexpected array type");
    }
}

bool
CodeGeneratorX86::visitLoadTypedArrayElementStatic(LLoadTypedArrayElementStatic *ins)
{
    const MLoadTypedArrayElementStatic *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LDefinition *out = ins->output();

    OutOfLineLoadTypedArrayOutOfBounds *ool = NULL;
    if (!mir->fallible()) {
        ool = new OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out));
        if (!addOutOfLineCode(ool))
            return false;
    }

    masm.cmpl(ptr, Imm32(mir->length()));
    if (ool)
        masm.j(Assembler::AboveOrEqual, ool->entry());
    else if (!bailoutIf(Assembler::AboveOrEqual, ins->snapshot()))
        return false;

    Address srcAddr(ptr, (int32_t) mir->base());
    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        FloatRegister dest = ToFloatRegister(out);
        masm.movssWithPatch(srcAddr, dest);
        masm.cvtss2sd(dest, dest);
        if (ool)
            masm.bind(ool->rejoin());
        return true;
    }
    loadViewTypeElement(vt, srcAddr, out);
    if (ool)
        masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitAsmJSLoadHeap(LAsmJSLoadHeap *ins)
{
    // This is identical to LoadTypedArrayElementStatic, except that the
    // array's base and length are not known ahead of time and can be patched
    // later on, and the instruction is always infallible.
    const MAsmJSLoadHeap *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LDefinition *out = ins->output();

    OutOfLineLoadTypedArrayOutOfBounds *ool = new OutOfLineLoadTypedArrayOutOfBounds(ToAnyRegister(out));
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
    loadViewTypeElement(vt, srcAddr, out);
    uint32_t after = masm.size();
    masm.bind(ool->rejoin());
    return gen->noteHeapAccess(AsmJSHeapAccess(cmp.offset(), before, after, vt, ToAnyRegister(out)));
}

bool
CodeGeneratorX86::visitOutOfLineLoadTypedArrayOutOfBounds(OutOfLineLoadTypedArrayOutOfBounds *ool)
{
    if (ool->dest().isFloat()) {
        masm.movsd(&js_NaN, ool->dest().fpu());
    } else {
        Register destReg = ool->dest().gpr();
        masm.xorl(destReg, destReg);
    }
    masm.jmp(ool->rejoin());
    return true;
}

void
CodeGeneratorX86::storeViewTypeElement(ArrayBufferView::ViewType vt, const LAllocation *value,
                                       const Address &dstAddr)
{
    switch (vt) {
      case ArrayBufferView::TYPE_INT8:    masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT8_CLAMPED:
      case ArrayBufferView::TYPE_UINT8:   masm.movbWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT16:   masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT16:  masm.movwWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_INT32:   masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_UINT32:  masm.movlWithPatch(ToRegister(value), dstAddr); break;
      case ArrayBufferView::TYPE_FLOAT64: masm.movsdWithPatch(ToFloatRegister(value), dstAddr); break;
      default: MOZ_ASSUME_NOT_REACHED("unexpected array type");
    }
}

bool
CodeGeneratorX86::visitStoreTypedArrayElementStatic(LStoreTypedArrayElementStatic *ins)
{
    MStoreTypedArrayElementStatic *mir = ins->mir();
    ArrayBufferView::ViewType vt = mir->viewType();

    Register ptr = ToRegister(ins->ptr());
    const LAllocation *value = ins->value();

    masm.cmpl(ptr, Imm32(mir->length()));
    Label rejoin;
    masm.j(Assembler::AboveOrEqual, &rejoin);

    Address dstAddr(ptr, (int32_t) mir->base());
    if (vt == ArrayBufferView::TYPE_FLOAT32) {
        masm.convertDoubleToFloat(ToFloatRegister(value), ScratchFloatReg);
        masm.movssWithPatch(ScratchFloatReg, dstAddr);
        masm.bind(&rejoin);
        return true;
    }
    storeViewTypeElement(vt, value, dstAddr);
    masm.bind(&rejoin);
    return true;
}

bool
CodeGeneratorX86::visitAsmJSStoreHeap(LAsmJSStoreHeap *ins)
{
    // This is identical to StoreTypedArrayElementStatic, except that the
    // array's base and length are not known ahead of time and can be patched
    // later on.
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
    storeViewTypeElement(vt, value, dstAddr);
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

void
ParallelGetPropertyIC::initializeAddCacheState(LInstruction *ins, AddCacheState *addState)
{
    // We don't have a scratch register, but only use the temp if we needed
    // one, it's BogusTemp otherwise.
    JS_ASSERT(ins->isGetPropertyCacheV() || ins->isGetPropertyCacheT());
    if (ins->isGetPropertyCacheV() || ins->toGetPropertyCacheT()->temp()->isBogusTemp())
        addState->dispatchScratch = output_.scratchReg().gpr();
    else
        addState->dispatchScratch = ToRegister(ins->toGetPropertyCacheT()->temp());
}

namespace js {
namespace ion {

class OutOfLineTruncate : public OutOfLineCodeBase<CodeGeneratorX86>
{
    LTruncateDToInt32 *ins_;

  public:
    OutOfLineTruncate(LTruncateDToInt32 *ins)
      : ins_(ins)
    { }

    bool accept(CodeGeneratorX86 *codegen) {
        return codegen->visitOutOfLineTruncate(this);
    }
    LTruncateDToInt32 *ins() const {
        return ins_;
    }
};

} // namespace ion
} // namespace js

bool
CodeGeneratorX86::visitTruncateDToInt32(LTruncateDToInt32 *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    OutOfLineTruncate *ool = new OutOfLineTruncate(ins);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTruncateDouble(input, output, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorX86::visitOutOfLineTruncate(OutOfLineTruncate *ool)
{
    LTruncateDToInt32 *ins = ool->ins();
    FloatRegister input = ToFloatRegister(ins->input());
    Register output = ToRegister(ins->output());

    Label fail;

    if (Assembler::HasSSE3()) {
        // Push double.
        masm.subl(Imm32(sizeof(double)), esp);
        masm.movsd(input, Operand(esp, 0));

        static const uint32_t EXPONENT_MASK = 0x7ff00000;
        static const uint32_t EXPONENT_SHIFT = DoubleExponentShift - 32;
        static const uint32_t TOO_BIG_EXPONENT = (DoubleExponentBias + 63) << EXPONENT_SHIFT;

        // Check exponent to avoid fp exceptions.
        Label failPopDouble;
        masm.movl(Operand(esp, 4), output);
        masm.and32(Imm32(EXPONENT_MASK), output);
        masm.branch32(Assembler::GreaterThanOrEqual, output, Imm32(TOO_BIG_EXPONENT), &failPopDouble);

        // Load double, perform 64-bit truncation.
        masm.fld(Operand(esp, 0));
        masm.fisttp(Operand(esp, 0));

        // Load low word, pop double and jump back.
        masm.movl(Operand(esp, 0), output);
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
        masm.xorpd(ScratchFloatReg, ScratchFloatReg);
        masm.ucomisd(input, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);

        {
            Label positive;
            masm.j(Assembler::Above, &positive);

            static const double shiftNeg = 4294967296.0;
            masm.loadStaticDouble(&shiftNeg, temp);
            Label skip;
            masm.jmp(&skip);

            masm.bind(&positive);
            static const double shiftPos = -4294967296.0;
            masm.loadStaticDouble(&shiftPos, temp);
            masm.bind(&skip);
        }

        masm.addsd(input, temp);
        masm.cvttsd2si(temp, output);
        masm.cvtsi2sd(output, ScratchFloatReg);

        masm.ucomisd(temp, ScratchFloatReg);
        masm.j(Assembler::Parity, &fail);
        masm.j(Assembler::Equal, ool->rejoin());
    }

    masm.bind(&fail);
    {
        saveVolatile(output);

        masm.setupUnalignedABICall(1, output);
        masm.passABIArg(input);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));
        masm.storeCallResult(output);

        restoreVolatile(output);
    }

    masm.jump(ool->rejoin());
    return true;
}
