/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips32/CodeGenerator-mips32.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/CodeGenerator.h"
#include "jit/JitCompartment.h"
#include "jit/JitFrames.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "js/Conversions.h"
#include "vm/Shape.h"
#include "vm/TraceLogging.h"

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::jit;

class js::jit::OutOfLineTableSwitch : public OutOfLineCodeBase<CodeGeneratorMIPS>
{
    MTableSwitch* mir_;
    CodeLabel jumpLabel_;

    void accept(CodeGeneratorMIPS* codegen) {
        codegen->visitOutOfLineTableSwitch(this);
    }

  public:
    OutOfLineTableSwitch(MTableSwitch* mir)
      : mir_(mir)
    {}

    MTableSwitch* mir() const {
        return mir_;
    }

    CodeLabel* jumpLabel() {
        return &jumpLabel_;
    }
};

void
CodeGeneratorMIPS::visitOutOfLineBailout(OutOfLineBailout* ool)
{
    // Push snapshotOffset and make sure stack is aligned.
    masm.subPtr(Imm32(2 * sizeof(void*)), StackPointer);
    masm.storePtr(ImmWord(ool->snapshot()->snapshotOffset()), Address(StackPointer, 0));

    masm.jump(&deoptLabel_);
}

void
CodeGeneratorMIPS::visitOutOfLineTableSwitch(OutOfLineTableSwitch* ool)
{
    MTableSwitch* mir = ool->mir();

    masm.haltingAlign(sizeof(void*));
    masm.bind(ool->jumpLabel()->target());
    masm.addCodeLabel(*ool->jumpLabel());

    for (size_t i = 0; i < mir->numCases(); i++) {
        LBlock* caseblock = skipTrivialBlocks(mir->getCase(i))->lir();
        Label* caseheader = caseblock->label();
        uint32_t caseoffset = caseheader->offset();

        // The entries of the jump table need to be absolute addresses and thus
        // must be patched after codegen is finished.
        CodeLabel cl;
        masm.ma_li(ScratchRegister, cl.patchAt());
        masm.branch(ScratchRegister);
        cl.target()->bind(caseoffset);
        masm.addCodeLabel(cl);
    }
}

void
CodeGeneratorMIPS::emitTableSwitchDispatch(MTableSwitch* mir, Register index,
                                           Register address)
{
    Label* defaultcase = skipTrivialBlocks(mir->getDefault())->lir()->label();

    // Lower value with low value
    if (mir->low() != 0)
        masm.subPtr(Imm32(mir->low()), index);

    // Jump to default case if input is out of range
    int32_t cases = mir->numCases();
    masm.branchPtr(Assembler::AboveOrEqual, index, ImmWord(cases), defaultcase);

    // To fill in the CodeLabels for the case entries, we need to first
    // generate the case entries (we don't yet know their offsets in the
    // instruction stream).
    OutOfLineTableSwitch* ool = new(alloc()) OutOfLineTableSwitch(mir);
    addOutOfLineCode(ool, mir);

    // Compute the position where a pointer to the right case stands.
    masm.ma_li(address, ool->jumpLabel()->patchAt());
    masm.lshiftPtr(Imm32(4), index);
    masm.addPtr(index, address);

    masm.branch(address);
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
CodeGeneratorMIPS::ToValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getOperand(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getOperand(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorMIPS::ToOutValue(LInstruction* ins)
{
    Register typeReg = ToRegister(ins->getDef(TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getDef(PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

ValueOperand
CodeGeneratorMIPS::ToTempValue(LInstruction* ins, size_t pos)
{
    Register typeReg = ToRegister(ins->getTemp(pos + TYPE_INDEX));
    Register payloadReg = ToRegister(ins->getTemp(pos + PAYLOAD_INDEX));
    return ValueOperand(typeReg, payloadReg);
}

void
CodeGeneratorMIPS::visitBox(LBox* box)
{
    const LDefinition* type = box->getDef(TYPE_INDEX);

    MOZ_ASSERT(!box->getOperand(0)->isConstant());

    // For NUNBOX32, the input operand and the output payload have the same
    // virtual register. All that needs to be written is the type tag for
    // the type definition.
    masm.move32(Imm32(MIRTypeToTag(box->type())), ToRegister(type));
}

void
CodeGeneratorMIPS::visitBoxFloatingPoint(LBoxFloatingPoint* box)
{
    const LDefinition* payload = box->getDef(PAYLOAD_INDEX);
    const LDefinition* type = box->getDef(TYPE_INDEX);
    const LAllocation* in = box->getOperand(0);

    FloatRegister reg = ToFloatRegister(in);
    if (box->type() == MIRType::Float32) {
        masm.convertFloat32ToDouble(reg, ScratchDoubleReg);
        reg = ScratchDoubleReg;
    }
    masm.ma_mv(reg, ValueOperand(ToRegister(type), ToRegister(payload)));
}

void
CodeGeneratorMIPS::visitUnbox(LUnbox* unbox)
{
    // Note that for unbox, the type and payload indexes are switched on the
    // inputs.
    MUnbox* mir = unbox->mir();
    Register type = ToRegister(unbox->type());

    if (mir->fallible()) {
        bailoutCmp32(Assembler::NotEqual, type, Imm32(MIRTypeToTag(mir->type())),
                     unbox->snapshot());
    }
}

Register
CodeGeneratorMIPS::splitTagForTest(const ValueOperand& value)
{
    return value.typeReg();
}

void
CodeGeneratorMIPS::visitCompareB(LCompareB* lir)
{
    MCompare* mir = lir->mir();

    const ValueOperand lhs = ToValue(lir, LCompareB::Lhs);
    const LAllocation* rhs = lir->rhs();
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());

    Label notBoolean, done;
    masm.branchTestBoolean(Assembler::NotEqual, lhs, &notBoolean);
    {
        if (rhs->isConstant())
            masm.cmp32Set(cond, lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()), output);
        else
            masm.cmp32Set(cond, lhs.payloadReg(), ToRegister(rhs), output);
        masm.jump(&done);
    }

    masm.bind(&notBoolean);
    {
        masm.move32(Imm32(mir->jsop() == JSOP_STRICTNE), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorMIPS::visitCompareBAndBranch(LCompareBAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    const ValueOperand lhs = ToValue(lir, LCompareBAndBranch::Lhs);
    const LAllocation* rhs = lir->rhs();

    MOZ_ASSERT(mir->jsop() == JSOP_STRICTEQ || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock* mirNotBoolean = (mir->jsop() == JSOP_STRICTEQ) ? lir->ifFalse() : lir->ifTrue();
    branchToBlock(lhs.typeReg(), ImmType(JSVAL_TYPE_BOOLEAN), mirNotBoolean, Assembler::NotEqual);

    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    if (rhs->isConstant())
        emitBranch(lhs.payloadReg(), Imm32(rhs->toConstant()->toBoolean()), cond, lir->ifTrue(),
                   lir->ifFalse());
    else
        emitBranch(lhs.payloadReg(), ToRegister(rhs), cond, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorMIPS::visitCompareBitwise(LCompareBitwise* lir)
{
    MCompare* mir = lir->mir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwise::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwise::RhsInput);
    const Register output = ToRegister(lir->output());

    MOZ_ASSERT(IsEqualityOp(mir->jsop()));

    Label notEqual, done;
    masm.ma_b(lhs.typeReg(), rhs.typeReg(), &notEqual, Assembler::NotEqual, ShortJump);
    {
        masm.cmp32Set(cond, lhs.payloadReg(), rhs.payloadReg(), output);
        masm.ma_b(&done, ShortJump);
    }
    masm.bind(&notEqual);
    {
        masm.move32(Imm32(cond == Assembler::NotEqual), output);
    }

    masm.bind(&done);
}

void
CodeGeneratorMIPS::visitCompareBitwiseAndBranch(LCompareBitwiseAndBranch* lir)
{
    MCompare* mir = lir->cmpMir();
    Assembler::Condition cond = JSOpToCondition(mir->compareType(), mir->jsop());
    const ValueOperand lhs = ToValue(lir, LCompareBitwiseAndBranch::LhsInput);
    const ValueOperand rhs = ToValue(lir, LCompareBitwiseAndBranch::RhsInput);

    MOZ_ASSERT(mir->jsop() == JSOP_EQ || mir->jsop() == JSOP_STRICTEQ ||
               mir->jsop() == JSOP_NE || mir->jsop() == JSOP_STRICTNE);

    MBasicBlock* notEqual = (cond == Assembler::Equal) ? lir->ifFalse() : lir->ifTrue();

    branchToBlock(lhs.typeReg(), rhs.typeReg(), notEqual, Assembler::NotEqual);
    emitBranch(lhs.payloadReg(), rhs.payloadReg(), cond, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorMIPS::visitCompareI64(LCompareI64* lir)
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

    masm.move32(Imm32(0), output);
    masm.bind(&done);
}

void
CodeGeneratorMIPS::visitCompareI64AndBranch(LCompareI64AndBranch* lir)
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
CodeGeneratorMIPS::visitDivOrModI64(LDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));
    Register64 output = ToOutRegister64(lir);

    MOZ_ASSERT(output == ReturnReg64);

    // All inputs are useAtStart for a call instruction. As a result we cannot
    // ask for a non-aliasing temp. Using the following to get such a temp.
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
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, trap(lir, wasm::Trap::IntegerDivideByZero));

    // Handle an integer overflow exception from INT64_MIN / -1.
    if (lir->canBeNegativeOverflow()) {
        Label notmin;
        masm.branch64(Assembler::NotEqual, lhs, Imm64(INT64_MIN), &notmin);
        masm.branch64(Assembler::NotEqual, rhs, Imm64(-1), &notmin);
        if (lir->mir()->isMod()) {
            masm.xor64(output, output);
        } else {
            masm.jump(trap(lir, wasm::Trap::IntegerOverflow));
        }
        masm.jump(&done);
        masm.bind(&notmin);
    }

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingWasm());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::ModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::DivI64);
    MOZ_ASSERT(ReturnReg64 == output);

    masm.bind(&done);
}

void
CodeGeneratorMIPS::visitUDivOrModI64(LUDivOrModI64* lir)
{
    Register64 lhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Lhs));
    Register64 rhs = ToRegister64(lir->getInt64Operand(LDivOrModI64::Rhs));

    MOZ_ASSERT(ToOutRegister64(lir) == ReturnReg64);

    // All inputs are useAtStart for a call instruction. As a result we cannot
    // ask for a non-aliasing temp. Using the following to get such a temp.
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
        masm.branchTest64(Assembler::Zero, rhs, rhs, temp, trap(lir, wasm::Trap::IntegerDivideByZero));

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(lhs.high);
    masm.passABIArg(lhs.low);
    masm.passABIArg(rhs.high);
    masm.passABIArg(rhs.low);

    MOZ_ASSERT(gen->compilingWasm());
    if (lir->mir()->isMod())
        masm.callWithABI(wasm::SymbolicAddress::UModI64);
    else
        masm.callWithABI(wasm::SymbolicAddress::UDivI64);
}

template <typename T>
void
CodeGeneratorMIPS::emitWasmLoadI64(T* lir)
{
    const MWasmLoad* mir = lir->mir();
    Register64 output = ToOutRegister64(lir);

    uint32_t offset = mir->access().offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptr());

    if (offset) {
        Register ptrPlusOffset = ToRegister(lir->ptrCopy());
        masm.addPtr(Imm32(offset), ptrPlusOffset);
        ptr = ptrPlusOffset;
    } else {
        MOZ_ASSERT(lir->ptrCopy()->isBogusTemp());
    }

    unsigned byteSize = mir->access().byteSize();
    bool isSigned;
    switch (mir->access().type()) {
        case Scalar::Int8:   isSigned = true; break;
        case Scalar::Uint8:  isSigned = false; break;
        case Scalar::Int16:  isSigned = true; break;
        case Scalar::Uint16: isSigned = false; break;
        case Scalar::Int32:  isSigned = true; break;
        case Scalar::Uint32: isSigned = false; break;
        case Scalar::Int64:  isSigned = true; break;
        default: MOZ_CRASH("unexpected array type");
    }

    masm.memoryBarrier(mir->access().barrierBefore());

    MOZ_ASSERT(INT64LOW_OFFSET == 0);
    if (IsUnaligned(mir->access())) {
        Register temp = ToRegister(lir->getTemp(1));

        if (byteSize <= 4) {
            masm.ma_load_unaligned(output.low, BaseIndex(HeapReg, ptr, TimesOne),
                                   temp, static_cast<LoadStoreSize>(8 * byteSize),
                                   isSigned ? SignExtend : ZeroExtend);
            if (!isSigned)
                masm.move32(Imm32(0), output.high);
            else
                masm.ma_sra(output.high, output.low, Imm32(31));
        } else {
            ScratchRegisterScope scratch(masm);
            masm.ma_load_unaligned(output.low, BaseIndex(HeapReg, ptr, TimesOne),
                                   temp, SizeWord, isSigned ? SignExtend : ZeroExtend);
            masm.ma_addu(scratch, ptr, Imm32(INT64HIGH_OFFSET));
            masm.ma_load_unaligned(output.high, BaseIndex(HeapReg, scratch, TimesOne),
                                   temp, SizeWord, isSigned ? SignExtend : ZeroExtend);
        }
        return;
    }

    if (byteSize <= 4) {
        masm.ma_load(output.low, BaseIndex(HeapReg, ptr, TimesOne),
                     static_cast<LoadStoreSize>(8 * byteSize), isSigned ? SignExtend : ZeroExtend);
        if (!isSigned)
            masm.move32(Imm32(0), output.high);
        else
            masm.ma_sra(output.high, output.low, Imm32(31));
    } else {
        ScratchRegisterScope scratch(masm);
        masm.ma_load(output.low, BaseIndex(HeapReg, ptr, TimesOne), SizeWord);
        masm.ma_addu(scratch, ptr, Imm32(INT64HIGH_OFFSET));
        masm.ma_load(output.high, BaseIndex(HeapReg, scratch, TimesOne), SizeWord);
    }

    masm.memoryBarrier(mir->access().barrierAfter());
}

void
CodeGeneratorMIPS::visitWasmLoadI64(LWasmLoadI64* lir)
{
    emitWasmLoadI64(lir);
}

void
CodeGeneratorMIPS::visitWasmUnalignedLoadI64(LWasmUnalignedLoadI64* lir)
{
    emitWasmLoadI64(lir);
}

template <typename T>
void
CodeGeneratorMIPS::emitWasmStoreI64(T* lir)
{
    const MWasmStore* mir = lir->mir();
    Register64 value = ToRegister64(lir->getInt64Operand(lir->ValueIndex));

    uint32_t offset = mir->access().offset();
    MOZ_ASSERT(offset < wasm::OffsetGuardLimit);

    Register ptr = ToRegister(lir->ptr());

    if (offset) {
        Register ptrPlusOffset = ToRegister(lir->ptrCopy());
        masm.addPtr(Imm32(offset), ptrPlusOffset);
        ptr = ptrPlusOffset;
    } else {
        MOZ_ASSERT(lir->ptrCopy()->isBogusTemp());
    }

    unsigned byteSize = mir->access().byteSize();
    bool isSigned;
    switch (mir->access().type()) {
        case Scalar::Int8:   isSigned = true; break;
        case Scalar::Uint8:  isSigned = false; break;
        case Scalar::Int16:  isSigned = true; break;
        case Scalar::Uint16: isSigned = false; break;
        case Scalar::Int32:  isSigned = true; break;
        case Scalar::Uint32: isSigned = false; break;
        case Scalar::Int64:  isSigned = true; break;
        default: MOZ_CRASH("unexpected array type");
    }

    masm.memoryBarrier(mir->access().barrierBefore());

    MOZ_ASSERT(INT64LOW_OFFSET == 0);
    if (IsUnaligned(mir->access())) {
        Register temp = ToRegister(lir->getTemp(1));

        if (byteSize <= 4) {
            masm.ma_store_unaligned(value.low, BaseIndex(HeapReg, ptr, TimesOne),
                                    temp, static_cast<LoadStoreSize>(8 * byteSize),
                                    isSigned ? SignExtend : ZeroExtend);
        } else {
            ScratchRegisterScope scratch(masm);
            masm.ma_store_unaligned(value.low, BaseIndex(HeapReg, ptr, TimesOne),
                                    temp, SizeWord, isSigned ? SignExtend : ZeroExtend);
            masm.ma_addu(scratch, ptr, Imm32(INT64HIGH_OFFSET));
            masm.ma_store_unaligned(value.high, BaseIndex(HeapReg, scratch, TimesOne),
                                    temp, SizeWord, isSigned ? SignExtend : ZeroExtend);
        }
        return;
    }

    if (byteSize <= 4) {
        masm.ma_store(value.low, BaseIndex(HeapReg, ptr, TimesOne),
                      static_cast<LoadStoreSize>(8 * byteSize));
    } else {
        ScratchRegisterScope scratch(masm);
        masm.ma_store(value.low, BaseIndex(HeapReg, ptr, TimesOne), SizeWord);
        masm.ma_addu(scratch, ptr, Imm32(INT64HIGH_OFFSET));
        masm.ma_store(value.high, BaseIndex(HeapReg, scratch, TimesOne), SizeWord);
    }

    masm.memoryBarrier(mir->access().barrierAfter());
}

void
CodeGeneratorMIPS::visitWasmStoreI64(LWasmStoreI64* lir)
{
    emitWasmStoreI64(lir);
}

void
CodeGeneratorMIPS::visitWasmUnalignedStoreI64(LWasmUnalignedStoreI64* lir)
{
    emitWasmStoreI64(lir);
}

void
CodeGeneratorMIPS::visitWasmSelectI64(LWasmSelectI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    Register cond = ToRegister(lir->condExpr());
    const LInt64Allocation trueExpr = lir->trueExpr();
    const LInt64Allocation falseExpr = lir->falseExpr();

    Register64 output = ToOutRegister64(lir);

    masm.move64(ToRegister64(trueExpr), output);

    if (falseExpr.low().isRegister()) {
        masm.as_movz(output.low, ToRegister(falseExpr.low()), cond);
        masm.as_movz(output.high, ToRegister(falseExpr.high()), cond);
    } else {
        Label done;
        masm.ma_b(cond, cond, &done, Assembler::NonZero, ShortJump);
        masm.loadPtr(ToAddress(falseExpr.low()), output.low);
        masm.loadPtr(ToAddress(falseExpr.high()), output.high);
        masm.bind(&done);
    }
}

void
CodeGeneratorMIPS::visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    masm.moveToDoubleLo(input.low, output);
    masm.moveToDoubleHi(input.high, output);
}

void
CodeGeneratorMIPS::visitWasmReinterpretToI64(LWasmReinterpretToI64* lir)
{
    MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
    MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
    FloatRegister input = ToFloatRegister(lir->getOperand(0));
    Register64 output = ToOutRegister64(lir);

    masm.moveFromDoubleLo(input, output.low);
    masm.moveFromDoubleHi(input, output.high);
}

void
CodeGeneratorMIPS::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir)
{
    Register input = ToRegister(lir->input());
    Register64 output = ToOutRegister64(lir);

    if (input != output.low)
        masm.move32(input, output.low);
    if (lir->mir()->isUnsigned())
        masm.move32(Imm32(0), output.high);
    else
        masm.ma_sra(output.high, output.low, Imm32(31));
}

void
CodeGeneratorMIPS::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir)
{
    const LInt64Allocation& input = lir->getInt64Operand(0);
    Register output = ToRegister(lir->output());

    if (lir->mir()->bottomHalf())
        masm.move32(ToRegister(input.low()), output);
    else
        masm.move32(ToRegister(input.high()), output);
}

void
CodeGeneratorMIPS::visitClzI64(LClzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);
    masm.clz64(input, output.low);
    masm.move32(Imm32(0), output.high);
}

void
CodeGeneratorMIPS::visitCtzI64(LCtzI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register64 output = ToOutRegister64(lir);
    masm.ctz64(input, output.low);
    masm.move32(Imm32(0), output.high);
}

void
CodeGeneratorMIPS::visitNotI64(LNotI64* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    Register output = ToRegister(lir->output());

    masm.as_or(output, input.low, input.high);
    masm.cmp32Set(Assembler::Equal, output, Imm32(0), output);
}

void
CodeGeneratorMIPS::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    FloatRegister scratch = input;
    Register64 output = ToOutRegister64(lir);
    MWasmTruncateToInt64* mir = lir->mir();
    MIRType fromType = mir->input()->type();

    auto* ool = new(alloc()) OutOfLineWasmTruncateCheck(mir, input);
    addOutOfLineCode(ool, mir);

    if (fromType == MIRType::Double) {
        masm.branchDouble(Assembler::DoubleUnordered, input, input, ool->entry());
    } else if (fromType == MIRType::Float32) {
        masm.branchFloat(Assembler::DoubleUnordered, input, input, ool->entry());
        scratch = ScratchDoubleReg;
        masm.convertFloat32ToDouble(input, scratch);
    } else {
        MOZ_CRASH("unexpected type in visitOutOfLineWasmTruncateCheck");
    }

    masm.setupUnalignedABICall(output.high);
    masm.passABIArg(scratch, MoveOp::DOUBLE);
    if (lir->mir()->isUnsigned())
        masm.callWithABI(wasm::SymbolicAddress::TruncateDoubleToUint64);
    else
        masm.callWithABI(wasm::SymbolicAddress::TruncateDoubleToInt64);
    masm.ma_b(output.high, Imm32(0x80000000), ool->rejoin(), Assembler::NotEqual);
    masm.ma_b(output.low, Imm32(0x00000000), ool->rejoin(), Assembler::NotEqual);
    masm.ma_b(ool->entry());

    masm.bind(ool->rejoin());

    MOZ_ASSERT(ReturnReg64 == output);
}

void
CodeGeneratorMIPS::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));
    FloatRegister output = ToFloatRegister(lir->output());

    MInt64ToFloatingPoint* mir = lir->mir();
    MIRType toType = mir->type();

    AllocatableGeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(input.low);
    regs.take(input.high);
    Register temp = regs.takeAny();

    masm.setupUnalignedABICall(temp);
    masm.passABIArg(input.high);
    masm.passABIArg(input.low);

    if (lir->mir()->isUnsigned())
        masm.callWithABI(wasm::SymbolicAddress::Uint64ToFloatingPoint, MoveOp::DOUBLE);
    else
        masm.callWithABI(wasm::SymbolicAddress::Int64ToFloatingPoint, MoveOp::DOUBLE);

    MOZ_ASSERT_IF(toType == MIRType::Double, output == ReturnDoubleReg);
    if (toType == MIRType::Float32) {
         MOZ_ASSERT(output == ReturnFloat32Reg);
         masm.convertDoubleToFloat32(ReturnDoubleReg, output);
    }
}

void
CodeGeneratorMIPS::visitTestI64AndBranch(LTestI64AndBranch* lir)
{
    Register64 input = ToRegister64(lir->getInt64Operand(0));

    branchToBlock(input.high, Imm32(0), lir->ifTrue(), Assembler::NonZero);
    emitBranch(input.low, Imm32(0), Assembler::NonZero, lir->ifTrue(), lir->ifFalse());
}

void
CodeGeneratorMIPS::setReturnDoubleRegs(LiveRegisterSet* regs)
{
    MOZ_ASSERT(ReturnFloat32Reg.code_ == ReturnDoubleReg.code_);
    regs->add(ReturnFloat32Reg);
    regs->add(ReturnDoubleReg.singleOverlay(1));
    regs->add(ReturnDoubleReg);
}
