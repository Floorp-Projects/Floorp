/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CodeGenerator.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "jslibmath.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsprf.h"

#include "builtin/Eval.h"
#include "builtin/TypedObject.h"
#ifdef JSGC_GENERATIONAL
# include "gc/Nursery.h"
#endif
#include "jit/ExecutionModeInlines.h"
#include "jit/IonCaches.h"
#include "jit/IonLinker.h"
#include "jit/IonOptimizationLevels.h"
#include "jit/IonSpewer.h"
#include "jit/MIRGenerator.h"
#include "jit/MoveEmitter.h"
#include "jit/ParallelFunctions.h"
#include "jit/ParallelSafetyAnalysis.h"
#include "jit/RangeAnalysis.h"
#include "vm/ForkJoin.h"

#include "jsboolinlines.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using mozilla::Maybe;
using mozilla::NegativeInfinity;
using mozilla::PositiveInfinity;
using JS::GenericNaN;

namespace js {
namespace jit {

// This out-of-line cache is used to do a double dispatch including it-self and
// the wrapped IonCache.
class OutOfLineUpdateCache :
  public OutOfLineCodeBase<CodeGenerator>,
  public IonCacheVisitor
{
  private:
    LInstruction *lir_;
    size_t cacheIndex_;
    AddCacheState state_;

  public:
    OutOfLineUpdateCache(LInstruction *lir, size_t cacheIndex)
      : lir_(lir),
        cacheIndex_(cacheIndex)
    { }

    void bind(MacroAssembler *masm) {
        // The binding of the initial jump is done in
        // CodeGenerator::visitOutOfLineCache.
    }

    size_t getCacheIndex() const {
        return cacheIndex_;
    }
    LInstruction *lir() const {
        return lir_;
    }
    AddCacheState &state() {
        return state_;
    }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineCache(this);
    }

    // ICs' visit functions delegating the work to the CodeGen visit funtions.
#define VISIT_CACHE_FUNCTION(op)                                        \
    bool visit##op##IC(CodeGenerator *codegen) {                        \
        CodeGenerator::DataPtr<op##IC> ic(codegen, getCacheIndex());    \
        return codegen->visit##op##IC(this, ic);                        \
    }

    IONCACHE_KIND_LIST(VISIT_CACHE_FUNCTION)
#undef VISIT_CACHE_FUNCTION
};

// This function is declared here because it needs to instantiate an
// OutOfLineUpdateCache, but we want to keep it visible inside the
// CodeGeneratorShared such as we can specialize inline caches in function of
// the architecture.
bool
CodeGeneratorShared::addCache(LInstruction *lir, size_t cacheIndex)
{
    DataPtr<IonCache> cache(this, cacheIndex);
    MInstruction *mir = lir->mirRaw()->toInstruction();
    if (mir->resumePoint())
        cache->setScriptedLocation(mir->block()->info().script(),
                                   mir->resumePoint()->pc());
    else
        cache->setIdempotent();

    OutOfLineUpdateCache *ool = new(alloc()) OutOfLineUpdateCache(lir, cacheIndex);
    if (!addOutOfLineCode(ool))
        return false;

    // OOL-specific state depends on the type of cache.
    cache->initializeAddCacheState(lir, &ool->state());

    cache->emitInitialJump(masm, ool->state());
    masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitOutOfLineCache(OutOfLineUpdateCache *ool)
{
    DataPtr<IonCache> cache(this, ool->getCacheIndex());

    // Register the location of the OOL path in the IC.
    cache->setFallbackLabel(masm.labelForPatch());
    cache->bindInitialJump(masm, ool->state());

    // Dispatch to ICs' accept functions.
    return cache->accept(this, ool);
}

StringObject *
MNewStringObject::templateObj() const {
    return &templateObj_->as<StringObject>();
}

CodeGenerator::CodeGenerator(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
  : CodeGeneratorSpecific(gen, graph, masm)
  , ionScriptLabels_(gen->alloc())
  , unassociatedScriptCounts_(nullptr)
{
}

CodeGenerator::~CodeGenerator()
{
    JS_ASSERT_IF(!gen->compilingAsmJS(), masm.numAsmJSAbsoluteLinks() == 0);
    js_delete(unassociatedScriptCounts_);
}

typedef bool (*StringToNumberFn)(ThreadSafeContext *, JSString *, double *);
typedef bool (*StringToNumberParFn)(ForkJoinContext *, JSString *, double *);
static const VMFunctionsModal StringToNumberInfo = VMFunctionsModal(
    FunctionInfo<StringToNumberFn>(StringToNumber),
    FunctionInfo<StringToNumberParFn>(StringToNumberPar));

bool
CodeGenerator::visitValueToInt32(LValueToInt32 *lir)
{
    ValueOperand operand = ToValue(lir, LValueToInt32::Input);
    Register output = ToRegister(lir->output());
    FloatRegister temp = ToFloatRegister(lir->tempFloat());

    MDefinition *input;
    if (lir->mode() == LValueToInt32::NORMAL)
        input = lir->mirNormal()->input();
    else
        input = lir->mirTruncate()->input();

    Label fails;
    if (lir->mode() == LValueToInt32::TRUNCATE) {
        OutOfLineCode *oolDouble = oolTruncateDouble(temp, output);
        if (!oolDouble)
            return false;

        // We can only handle strings in truncation contexts, like bitwise
        // operations.
        Label *stringEntry, *stringRejoin;
        Register stringReg;
        if (input->mightBeType(MIRType_String)) {
            stringReg = ToRegister(lir->temp());
            OutOfLineCode *oolString = oolCallVM(StringToNumberInfo, lir, (ArgList(), stringReg),
                                                 StoreFloatRegisterTo(temp));
            if (!oolString)
                return false;
            stringEntry = oolString->entry();
            stringRejoin = oolString->rejoin();
        } else {
            stringReg = InvalidReg;
            stringEntry = nullptr;
            stringRejoin = nullptr;
        }

        masm.truncateValueToInt32(operand, input, stringEntry, stringRejoin, oolDouble->entry(),
                                  stringReg, temp, output, &fails);
        masm.bind(oolDouble->rejoin());
    } else {
        masm.convertValueToInt32(operand, input, temp, output, &fails,
                                 lir->mirNormal()->canBeNegativeZero(),
                                 lir->mirNormal()->conversion());
    }

    return bailoutFrom(&fails, lir->snapshot());
}

bool
CodeGenerator::visitValueToDouble(LValueToDouble *lir)
{
    MToDouble *mir = lir->mir();
    ValueOperand operand = ToValue(lir, LValueToDouble::Input);
    FloatRegister output = ToFloatRegister(lir->output());

    Register tag = masm.splitTagForTest(operand);

    Label isDouble, isInt32, isBool, isNull, isUndefined, done;
    bool hasBoolean = false, hasNull = false, hasUndefined = false;

    masm.branchTestDouble(Assembler::Equal, tag, &isDouble);
    masm.branchTestInt32(Assembler::Equal, tag, &isInt32);

    if (mir->conversion() != MToDouble::NumbersOnly) {
        masm.branchTestBoolean(Assembler::Equal, tag, &isBool);
        masm.branchTestUndefined(Assembler::Equal, tag, &isUndefined);
        hasBoolean = true;
        hasUndefined = true;
        if (mir->conversion() != MToDouble::NonNullNonStringPrimitives) {
            masm.branchTestNull(Assembler::Equal, tag, &isNull);
            hasNull = true;
        }
    }

    if (!bailout(lir->snapshot()))
        return false;

    if (hasNull) {
        masm.bind(&isNull);
        masm.loadConstantDouble(0.0, output);
        masm.jump(&done);
    }

    if (hasUndefined) {
        masm.bind(&isUndefined);
        masm.loadConstantDouble(GenericNaN(), output);
        masm.jump(&done);
    }

    if (hasBoolean) {
        masm.bind(&isBool);
        masm.boolValueToDouble(operand, output);
        masm.jump(&done);
    }

    masm.bind(&isInt32);
    masm.int32ValueToDouble(operand, output);
    masm.jump(&done);

    masm.bind(&isDouble);
    masm.unboxDouble(operand, output);
    masm.bind(&done);

    return true;
}

bool
CodeGenerator::visitValueToFloat32(LValueToFloat32 *lir)
{
    MToFloat32 *mir = lir->mir();
    ValueOperand operand = ToValue(lir, LValueToFloat32::Input);
    FloatRegister output = ToFloatRegister(lir->output());

    Register tag = masm.splitTagForTest(operand);

    Label isDouble, isInt32, isBool, isNull, isUndefined, done;
    bool hasBoolean = false, hasNull = false, hasUndefined = false;

    masm.branchTestDouble(Assembler::Equal, tag, &isDouble);
    masm.branchTestInt32(Assembler::Equal, tag, &isInt32);

    if (mir->conversion() != MToFloat32::NumbersOnly) {
        masm.branchTestBoolean(Assembler::Equal, tag, &isBool);
        masm.branchTestUndefined(Assembler::Equal, tag, &isUndefined);
        hasBoolean = true;
        hasUndefined = true;
        if (mir->conversion() != MToFloat32::NonNullNonStringPrimitives) {
            masm.branchTestNull(Assembler::Equal, tag, &isNull);
            hasNull = true;
        }
    }

    if (!bailout(lir->snapshot()))
        return false;

    if (hasNull) {
        masm.bind(&isNull);
        masm.loadConstantFloat32(0.0f, output);
        masm.jump(&done);
    }

    if (hasUndefined) {
        masm.bind(&isUndefined);
        masm.loadConstantFloat32(float(GenericNaN()), output);
        masm.jump(&done);
    }

    if (hasBoolean) {
        masm.bind(&isBool);
        masm.boolValueToFloat32(operand, output);
        masm.jump(&done);
    }

    masm.bind(&isInt32);
    masm.int32ValueToFloat32(operand, output);
    masm.jump(&done);

    masm.bind(&isDouble);
    masm.unboxDouble(operand, output);
    masm.convertDoubleToFloat32(output, output);
    masm.bind(&done);

    return true;
}

bool
CodeGenerator::visitInt32ToDouble(LInt32ToDouble *lir)
{
    masm.convertInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitFloat32ToDouble(LFloat32ToDouble *lir)
{
    masm.convertFloat32ToDouble(ToFloatRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitDoubleToFloat32(LDoubleToFloat32 *lir)
{
    masm.convertDoubleToFloat32(ToFloatRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitInt32ToFloat32(LInt32ToFloat32 *lir)
{
    masm.convertInt32ToFloat32(ToRegister(lir->input()), ToFloatRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitDoubleToInt32(LDoubleToInt32 *lir)
{
    Label fail;
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.convertDoubleToInt32(input, output, &fail, lir->mir()->canBeNegativeZero());
    if (!bailoutFrom(&fail, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitFloat32ToInt32(LFloat32ToInt32 *lir)
{
    Label fail;
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.convertFloat32ToInt32(input, output, &fail, lir->mir()->canBeNegativeZero());
    if (!bailoutFrom(&fail, lir->snapshot()))
        return false;
    return true;
}

void
CodeGenerator::emitOOLTestObject(Register objreg,
                                 Label *ifEmulatesUndefined,
                                 Label *ifDoesntEmulateUndefined,
                                 Register scratch)
{
    saveVolatile(scratch);
    masm.setupUnalignedABICall(1, scratch);
    masm.passABIArg(objreg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::EmulatesUndefined));
    masm.storeCallResult(scratch);
    restoreVolatile(scratch);

    masm.branchIfTrueBool(scratch, ifEmulatesUndefined);
    masm.jump(ifDoesntEmulateUndefined);
}

// Base out-of-line code generator for all tests of the truthiness of an
// object, where the object might not be truthy.  (Recall that per spec all
// objects are truthy, but we implement the JSCLASS_EMULATES_UNDEFINED class
// flag to permit objects to look like |undefined| in certain contexts,
// including in object truthiness testing.)  We check truthiness inline except
// when we're testing it on a proxy (or if TI guarantees us that the specified
// object will never emulate |undefined|), in which case out-of-line code will
// call EmulatesUndefined for a conclusive answer.
class OutOfLineTestObject : public OutOfLineCodeBase<CodeGenerator>
{
    Register objreg_;
    Register scratch_;

    Label *ifEmulatesUndefined_;
    Label *ifDoesntEmulateUndefined_;

#ifdef DEBUG
    bool initialized() { return ifEmulatesUndefined_ != nullptr; }
#endif

  public:
    OutOfLineTestObject()
#ifdef DEBUG
      : ifEmulatesUndefined_(nullptr), ifDoesntEmulateUndefined_(nullptr)
#endif
    { }

    bool accept(CodeGenerator *codegen) MOZ_FINAL MOZ_OVERRIDE {
        MOZ_ASSERT(initialized());
        codegen->emitOOLTestObject(objreg_, ifEmulatesUndefined_, ifDoesntEmulateUndefined_,
                                   scratch_);
        return true;
    }

    // Specify the register where the object to be tested is found, labels to
    // jump to if the object is truthy or falsy, and a scratch register for
    // use in the out-of-line path.
    void setInputAndTargets(Register objreg, Label *ifEmulatesUndefined, Label *ifDoesntEmulateUndefined,
                            Register scratch)
    {
        MOZ_ASSERT(!initialized());
        MOZ_ASSERT(ifEmulatesUndefined);
        objreg_ = objreg;
        scratch_ = scratch;
        ifEmulatesUndefined_ = ifEmulatesUndefined;
        ifDoesntEmulateUndefined_ = ifDoesntEmulateUndefined;
    }
};

// A subclass of OutOfLineTestObject containing two extra labels, for use when
// the ifTruthy/ifFalsy labels are needed in inline code as well as out-of-line
// code.  The user should bind these labels in inline code, and specify them as
// targets via setInputAndTargets, as appropriate.
class OutOfLineTestObjectWithLabels : public OutOfLineTestObject
{
    Label label1_;
    Label label2_;

  public:
    OutOfLineTestObjectWithLabels() { }

    Label *label1() { return &label1_; }
    Label *label2() { return &label2_; }
};

void
CodeGenerator::testObjectEmulatesUndefinedKernel(Register objreg,
                                                 Label *ifEmulatesUndefined,
                                                 Label *ifDoesntEmulateUndefined,
                                                 Register scratch, OutOfLineTestObject *ool)
{
    ool->setInputAndTargets(objreg, ifEmulatesUndefined, ifDoesntEmulateUndefined, scratch);

    // Perform a fast-path check of the object's class flags if the object's
    // not a proxy.  Let out-of-line code handle the slow cases that require
    // saving registers, making a function call, and restoring registers.
    masm.branchTestObjectTruthy(false, objreg, scratch, ool->entry(), ifEmulatesUndefined);
}

void
CodeGenerator::branchTestObjectEmulatesUndefined(Register objreg,
                                                 Label *ifEmulatesUndefined,
                                                 Label *ifDoesntEmulateUndefined,
                                                 Register scratch, OutOfLineTestObject *ool)
{
    MOZ_ASSERT(!ifDoesntEmulateUndefined->bound(),
               "ifDoesntEmulateUndefined will be bound to the fallthrough path");

    testObjectEmulatesUndefinedKernel(objreg, ifEmulatesUndefined, ifDoesntEmulateUndefined,
                                      scratch, ool);
    masm.bind(ifDoesntEmulateUndefined);
}

void
CodeGenerator::testObjectEmulatesUndefined(Register objreg,
                                           Label *ifEmulatesUndefined,
                                           Label *ifDoesntEmulateUndefined,
                                           Register scratch, OutOfLineTestObject *ool)
{
    testObjectEmulatesUndefinedKernel(objreg, ifEmulatesUndefined, ifDoesntEmulateUndefined,
                                      scratch, ool);
    masm.jump(ifDoesntEmulateUndefined);
}

void
CodeGenerator::testValueTruthyKernel(const ValueOperand &value,
                                     const LDefinition *scratch1, const LDefinition *scratch2,
                                     FloatRegister fr,
                                     Label *ifTruthy, Label *ifFalsy,
                                     OutOfLineTestObject *ool)
{
    Register tag = masm.splitTagForTest(value);

    // Eventually we will want some sort of type filter here. For now, just
    // emit all easy cases. For speed we use the cached tag for all comparison,
    // except for doubles, which we test last (as the operation can clobber the
    // tag, which may be in ScratchReg).
    masm.branchTestUndefined(Assembler::Equal, tag, ifFalsy);
    masm.branchTestNull(Assembler::Equal, tag, ifFalsy);

    Label notBoolean;
    masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
    masm.branchTestBooleanTruthy(false, value, ifFalsy);
    masm.jump(ifTruthy);
    masm.bind(&notBoolean);

    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, tag, &notInt32);
    masm.branchTestInt32Truthy(false, value, ifFalsy);
    masm.jump(ifTruthy);
    masm.bind(&notInt32);

    if (ool) {
        Label notObject;

        masm.branchTestObject(Assembler::NotEqual, tag, &notObject);

        Register objreg = masm.extractObject(value, ToRegister(scratch1));
        testObjectEmulatesUndefined(objreg, ifFalsy, ifTruthy, ToRegister(scratch2), ool);

        masm.bind(&notObject);
    } else {
        masm.branchTestObject(Assembler::Equal, tag, ifTruthy);
    }

    // Test if a string is non-empty.
    Label notString;
    masm.branchTestString(Assembler::NotEqual, tag, &notString);
    masm.branchTestStringTruthy(false, value, ifFalsy);
    masm.jump(ifTruthy);
    masm.bind(&notString);

    // If we reach here the value is a double.
    masm.unboxDouble(value, fr);
    masm.branchTestDoubleTruthy(false, fr, ifFalsy);

    // Fall through for truthy.
}

void
CodeGenerator::testValueTruthy(const ValueOperand &value,
                               const LDefinition *scratch1, const LDefinition *scratch2,
                               FloatRegister fr,
                               Label *ifTruthy, Label *ifFalsy,
                               OutOfLineTestObject *ool)
{
    testValueTruthyKernel(value, scratch1, scratch2, fr, ifTruthy, ifFalsy, ool);
    masm.jump(ifTruthy);
}

Label *
CodeGenerator::getJumpLabelForBranch(MBasicBlock *block)
{
    if (!labelForBackedgeWithImplicitCheck(block))
        return block->lir()->label();

    // We need to use a patchable jump for this backedge, but want to treat
    // this as a normal label target to simplify codegen. Efficiency isn't so
    // important here as these tests are extremely unlikely to be used in loop
    // backedges, so emit inline code for the patchable jump. Heap allocating
    // the label allows it to be used by out of line blocks.
    Label *res = GetIonContext()->temp->lifoAlloc()->new_<Label>();
    Label after;
    masm.jump(&after);
    masm.bind(res);
    jumpToBlock(block);
    masm.bind(&after);
    return res;
}

bool
CodeGenerator::visitTestOAndBranch(LTestOAndBranch *lir)
{
    MOZ_ASSERT(lir->mir()->operandMightEmulateUndefined(),
               "Objects which can't emulate undefined should have been constant-folded");

    OutOfLineTestObject *ool = new(alloc()) OutOfLineTestObject();
    if (!addOutOfLineCode(ool))
        return false;

    Label *truthy = getJumpLabelForBranch(lir->ifTruthy());
    Label *falsy = getJumpLabelForBranch(lir->ifFalsy());

    testObjectEmulatesUndefined(ToRegister(lir->input()), falsy, truthy,
                                ToRegister(lir->temp()), ool);
    return true;

}

bool
CodeGenerator::visitTestVAndBranch(LTestVAndBranch *lir)
{
    OutOfLineTestObject *ool = nullptr;
    if (lir->mir()->operandMightEmulateUndefined()) {
        ool = new(alloc()) OutOfLineTestObject();
        if (!addOutOfLineCode(ool))
            return false;
    }

    Label *truthy = getJumpLabelForBranch(lir->ifTruthy());
    Label *falsy = getJumpLabelForBranch(lir->ifFalsy());

    testValueTruthy(ToValue(lir, LTestVAndBranch::Input),
                    lir->temp1(), lir->temp2(),
                    ToFloatRegister(lir->tempFloat()),
                    truthy, falsy, ool);
    return true;
}

bool
CodeGenerator::visitFunctionDispatch(LFunctionDispatch *lir)
{
    MFunctionDispatch *mir = lir->mir();
    Register input = ToRegister(lir->input());
    Label *lastLabel;
    size_t casesWithFallback;

    // Determine if the last case is fallback or an ordinary case.
    if (!mir->hasFallback()) {
        JS_ASSERT(mir->numCases() > 0);
        casesWithFallback = mir->numCases();
        lastLabel = mir->getCaseBlock(mir->numCases() - 1)->lir()->label();
    } else {
        casesWithFallback = mir->numCases() + 1;
        lastLabel = mir->getFallback()->lir()->label();
    }

    // Compare function pointers, except for the last case.
    for (size_t i = 0; i < casesWithFallback - 1; i++) {
        JS_ASSERT(i < mir->numCases());
        JSFunction *func = mir->getCase(i);
        LBlock *target = mir->getCaseBlock(i)->lir();
        masm.branchPtr(Assembler::Equal, input, ImmGCPtr(func), target->label());
    }

    // Jump to the last case.
    masm.jump(lastLabel);

    return true;
}

bool
CodeGenerator::visitTypeObjectDispatch(LTypeObjectDispatch *lir)
{
    MTypeObjectDispatch *mir = lir->mir();
    Register input = ToRegister(lir->input());
    Register temp = ToRegister(lir->temp());

    // Hold the incoming TypeObject.
    masm.loadPtr(Address(input, JSObject::offsetOfType()), temp);

    // Compare TypeObjects.
    InlinePropertyTable *propTable = mir->propTable();
    for (size_t i = 0; i < mir->numCases(); i++) {
        JSFunction *func = mir->getCase(i);
        LBlock *target = mir->getCaseBlock(i)->lir();

        DebugOnly<bool> found = false;
        for (size_t j = 0; j < propTable->numEntries(); j++) {
            if (propTable->getFunction(j) != func)
                continue;
            types::TypeObject *typeObj = propTable->getTypeObject(j);
            masm.branchPtr(Assembler::Equal, temp, ImmGCPtr(typeObj), target->label());
            found = true;
        }
        JS_ASSERT(found);
    }

    // Unknown function: jump to fallback block.
    LBlock *fallback = mir->getFallback()->lir();
    masm.jump(fallback->label());
    return true;
}

bool
CodeGenerator::visitBooleanToString(LBooleanToString *lir)
{
    Register input = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    const JSAtomState &names = GetIonContext()->runtime->names();
    Label true_, done;

    masm.branchTest32(Assembler::NonZero, input, input, &true_);
    masm.movePtr(ImmGCPtr(names.false_), output);
    masm.jump(&done);

    masm.bind(&true_);
    masm.movePtr(ImmGCPtr(names.true_), output);

    masm.bind(&done);

    return true;
}

void
CodeGenerator::emitIntToString(Register input, Register output, Label *ool)
{
    masm.branch32(Assembler::AboveOrEqual, input, Imm32(StaticStrings::INT_STATIC_LIMIT), ool);

    // Fast path for small integers.
    masm.movePtr(ImmPtr(&GetIonContext()->runtime->staticStrings().intStaticTable), output);
    masm.loadPtr(BaseIndex(output, input, ScalePointer), output);
}

typedef JSFlatString *(*IntToStringFn)(ThreadSafeContext *, int);
typedef JSFlatString *(*IntToStringParFn)(ForkJoinContext *, int);
static const VMFunctionsModal IntToStringInfo = VMFunctionsModal(
    FunctionInfo<IntToStringFn>(Int32ToString<CanGC>),
    FunctionInfo<IntToStringParFn>(IntToStringPar));

bool
CodeGenerator::visitIntToString(LIntToString *lir)
{
    Register input = ToRegister(lir->input());
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(IntToStringInfo, lir, (ArgList(), input),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    emitIntToString(input, output, ool->entry());

    masm.bind(ool->rejoin());
    return true;
}

typedef JSString *(*DoubleToStringFn)(ThreadSafeContext *, double);
typedef JSString *(*DoubleToStringParFn)(ForkJoinContext *, double);
static const VMFunctionsModal DoubleToStringInfo = VMFunctionsModal(
    FunctionInfo<DoubleToStringFn>(NumberToString<CanGC>),
    FunctionInfo<DoubleToStringParFn>(DoubleToStringPar));

bool
CodeGenerator::visitDoubleToString(LDoubleToString *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register temp = ToRegister(lir->tempInt());
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(DoubleToStringInfo, lir, (ArgList(), input),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    // Try double to integer conversion and run integer to string code.
    masm.convertDoubleToInt32(input, temp, ool->entry(), true);
    emitIntToString(temp, output, ool->entry());

    masm.bind(ool->rejoin());
    return true;
}

typedef JSString *(*PrimitiveToStringFn)(JSContext *, HandleValue);
typedef JSString *(*PrimitiveToStringParFn)(ForkJoinContext *, HandleValue);
static const VMFunctionsModal PrimitiveToStringInfo = VMFunctionsModal(
    FunctionInfo<PrimitiveToStringFn>(ToStringSlow),
    FunctionInfo<PrimitiveToStringParFn>(PrimitiveToStringPar));

bool
CodeGenerator::visitPrimitiveToString(LPrimitiveToString *lir)
{
    ValueOperand input = ToValue(lir, LPrimitiveToString::Input);
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(PrimitiveToStringInfo, lir, (ArgList(), input),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    Label done;
    Register tag = masm.splitTagForTest(input);
    const JSAtomState &names = GetIonContext()->runtime->names();

    // String
    if (lir->mir()->input()->mightBeType(MIRType_String)) {
        Label notString;
        masm.branchTestString(Assembler::NotEqual, tag, &notString);
        masm.unboxString(input, output);
        masm.jump(&done);
        masm.bind(&notString);
    }

    // Integer
    if (lir->mir()->input()->mightBeType(MIRType_Int32)) {
        Label notInteger;
        masm.branchTestInt32(Assembler::NotEqual, tag, &notInteger);
        Register unboxed = ToTempUnboxRegister(lir->tempToUnbox());
        unboxed = masm.extractInt32(input, unboxed);
        emitIntToString(unboxed, output, ool->entry());
        masm.jump(&done);
        masm.bind(&notInteger);
    }

    // Double
    if (lir->mir()->input()->mightBeType(MIRType_Double)) {
        // Note: no fastpath. Need two extra registers and can only convert doubles
        // that fit integers and are smaller than StaticStrings::INT_STATIC_LIMIT.
        masm.branchTestDouble(Assembler::Equal, tag, ool->entry());
    }

    // Undefined
    if (lir->mir()->input()->mightBeType(MIRType_Undefined)) {
        Label notUndefined;
        masm.branchTestUndefined(Assembler::NotEqual, tag, &notUndefined);
        masm.movePtr(ImmGCPtr(names.undefined), output);
        masm.jump(&done);
        masm.bind(&notUndefined);
    }

    // Null
    if (lir->mir()->input()->mightBeType(MIRType_Null)) {
        Label notNull;
        masm.branchTestNull(Assembler::NotEqual, tag, &notNull);
        masm.movePtr(ImmGCPtr(names.null), output);
        masm.jump(&done);
        masm.bind(&notNull);
    }

    // Boolean
    if (lir->mir()->input()->mightBeType(MIRType_Boolean)) {
        Label notBoolean, true_;
        masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
        masm.branchTestBooleanTruthy(true, input, &true_);
        masm.movePtr(ImmGCPtr(names.false_), output);
        masm.jump(&done);
        masm.bind(&true_);
        masm.movePtr(ImmGCPtr(names.true_), output);
        masm.jump(&done);
        masm.bind(&notBoolean);
    }

#ifdef DEBUG
    // Objects are not supported or we see a type that wasn't accounted for.
    masm.assumeUnreachable("Unexpected type for MPrimitiveToString.");
#endif

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*CloneRegExpObjectFn)(JSContext *, JSObject *);
static const VMFunction CloneRegExpObjectInfo =
    FunctionInfo<CloneRegExpObjectFn>(CloneRegExpObject);

bool
CodeGenerator::visitRegExp(LRegExp *lir)
{
    pushArg(ImmGCPtr(lir->mir()->source()));
    return callVM(CloneRegExpObjectInfo, lir);
}

typedef bool (*RegExpExecRawFn)(JSContext *cx, HandleObject regexp,
                                HandleString input, MutableHandleValue output);
static const VMFunction RegExpExecRawInfo = FunctionInfo<RegExpExecRawFn>(regexp_exec_raw);

bool
CodeGenerator::visitRegExpExec(LRegExpExec *lir)
{
    pushArg(ToRegister(lir->string()));
    pushArg(ToRegister(lir->regexp()));
    return callVM(RegExpExecRawInfo, lir);
}

typedef bool (*RegExpTestRawFn)(JSContext *cx, HandleObject regexp,
                                HandleString input, bool *result);
static const VMFunction RegExpTestRawInfo = FunctionInfo<RegExpTestRawFn>(regexp_test_raw);

bool
CodeGenerator::visitRegExpTest(LRegExpTest *lir)
{
    pushArg(ToRegister(lir->string()));
    pushArg(ToRegister(lir->regexp()));
    return callVM(RegExpTestRawInfo, lir);
}

typedef JSString *(*RegExpReplaceFn)(JSContext *, HandleString, HandleObject, HandleString);
static const VMFunction RegExpReplaceInfo = FunctionInfo<RegExpReplaceFn>(RegExpReplace);

bool
CodeGenerator::visitRegExpReplace(LRegExpReplace *lir)
{
    if (lir->replacement()->isConstant())
        pushArg(ImmGCPtr(lir->replacement()->toConstant()->toString()));
    else
        pushArg(ToRegister(lir->replacement()));

    pushArg(ToRegister(lir->pattern()));

    if (lir->string()->isConstant())
        pushArg(ImmGCPtr(lir->string()->toConstant()->toString()));
    else
        pushArg(ToRegister(lir->string()));

    return callVM(RegExpReplaceInfo, lir);
}

typedef JSString *(*StringReplaceFn)(JSContext *, HandleString, HandleString, HandleString);
static const VMFunction StringReplaceInfo = FunctionInfo<StringReplaceFn>(StringReplace);

bool
CodeGenerator::visitStringReplace(LStringReplace *lir)
{
    if (lir->replacement()->isConstant())
        pushArg(ImmGCPtr(lir->replacement()->toConstant()->toString()));
    else
        pushArg(ToRegister(lir->replacement()));

    if (lir->pattern()->isConstant())
        pushArg(ImmGCPtr(lir->pattern()->toConstant()->toString()));
    else
        pushArg(ToRegister(lir->pattern()));

    if (lir->string()->isConstant())
        pushArg(ImmGCPtr(lir->string()->toConstant()->toString()));
    else
        pushArg(ToRegister(lir->string()));

    return callVM(StringReplaceInfo, lir);
}


typedef JSObject *(*LambdaFn)(JSContext *, HandleFunction, HandleObject);
static const VMFunction LambdaInfo =
    FunctionInfo<LambdaFn>(js::Lambda);

bool
CodeGenerator::visitLambdaForSingleton(LLambdaForSingleton *lir)
{
    pushArg(ToRegister(lir->scopeChain()));
    pushArg(ImmGCPtr(lir->mir()->info().fun));
    return callVM(LambdaInfo, lir);
}

bool
CodeGenerator::visitLambda(LLambda *lir)
{
    Register scopeChain = ToRegister(lir->scopeChain());
    Register output = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());
    const LambdaFunctionInfo &info = lir->mir()->info();

    OutOfLineCode *ool = oolCallVM(LambdaInfo, lir, (ArgList(), ImmGCPtr(info.fun), scopeChain),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    JS_ASSERT(!info.singletonType);

    masm.newGCThing(output, tempReg, info.fun, ool->entry(), gc::DefaultHeap);
    masm.initGCThing(output, tempReg, info.fun);

    emitLambdaInit(output, scopeChain, info);

    masm.bind(ool->rejoin());
    return true;
}

void
CodeGenerator::emitLambdaInit(Register output, Register scopeChain,
                              const LambdaFunctionInfo &info)
{
    // Initialize nargs and flags. We do this with a single uint32 to avoid
    // 16-bit writes.
    union {
        struct S {
            uint16_t nargs;
            uint16_t flags;
        } s;
        uint32_t word;
    } u;
    u.s.nargs = info.fun->nargs();
    u.s.flags = info.flags & ~JSFunction::EXTENDED;

    JS_ASSERT(JSFunction::offsetOfFlags() == JSFunction::offsetOfNargs() + 2);
    masm.store32(Imm32(u.word), Address(output, JSFunction::offsetOfNargs()));
    masm.storePtr(ImmGCPtr(info.scriptOrLazyScript),
                  Address(output, JSFunction::offsetOfNativeOrScript()));
    masm.storePtr(scopeChain, Address(output, JSFunction::offsetOfEnvironment()));
    masm.storePtr(ImmGCPtr(info.fun->displayAtom()), Address(output, JSFunction::offsetOfAtom()));
}

bool
CodeGenerator::visitLambdaPar(LLambdaPar *lir)
{
    Register resultReg = ToRegister(lir->output());
    Register cxReg = ToRegister(lir->forkJoinContext());
    Register scopeChainReg = ToRegister(lir->scopeChain());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    const LambdaFunctionInfo &info = lir->mir()->info();

    JS_ASSERT(scopeChainReg != resultReg);

    emitAllocateGCThingPar(lir, resultReg, cxReg, tempReg1, tempReg2, info.fun);
    emitLambdaInit(resultReg, scopeChainReg, info);
    return true;
}

bool
CodeGenerator::visitLabel(LLabel *lir)
{
    return true;
}

bool
CodeGenerator::visitNop(LNop *lir)
{
    return true;
}

bool
CodeGenerator::visitOsiPoint(LOsiPoint *lir)
{
    // Note: markOsiPoint ensures enough space exists between the last
    // LOsiPoint and this one to patch adjacent call instructions.

    JS_ASSERT(masm.framePushed() == frameSize());

    uint32_t osiCallPointOffset;
    if (!markOsiPoint(lir, &osiCallPointOffset))
        return false;

    LSafepoint *safepoint = lir->associatedSafepoint();
    JS_ASSERT(!safepoint->osiCallPointOffset());
    safepoint->setOsiCallPointOffset(osiCallPointOffset);

#ifdef DEBUG
    // There should be no movegroups or other instructions between
    // an instruction and its OsiPoint. This is necessary because
    // we use the OsiPoint's snapshot from within VM calls.
    for (LInstructionReverseIterator iter(current->rbegin(lir)); iter != current->rend(); iter++) {
        if (*iter == lir || iter->isNop())
            continue;
        JS_ASSERT(!iter->isMoveGroup());
        JS_ASSERT(iter->safepoint() == safepoint);
        break;
    }
#endif

#ifdef CHECK_OSIPOINT_REGISTERS
    if (shouldVerifyOsiPointRegs(safepoint))
        verifyOsiPointRegs(safepoint);
#endif

    return true;
}

bool
CodeGenerator::visitGoto(LGoto *lir)
{
    jumpToBlock(lir->target());
    return true;
}

// Out-of-line path to execute any move groups between the start of a loop
// header and its interrupt check, then invoke the interrupt handler.
class OutOfLineInterruptCheckImplicit : public OutOfLineCodeBase<CodeGenerator>
{
  public:
    LBlock *block;
    LInterruptCheckImplicit *lir;

    OutOfLineInterruptCheckImplicit(LBlock *block, LInterruptCheckImplicit *lir)
      : block(block), lir(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineInterruptCheckImplicit(this);
    }
};

bool
CodeGenerator::visitOutOfLineInterruptCheckImplicit(OutOfLineInterruptCheckImplicit *ool)
{
#ifdef CHECK_OSIPOINT_REGISTERS
    // This is path is entered from the patched back-edge of the loop. This
    // means that the JitAtivation flags used for checking the validity of the
    // OSI points are not reseted by the path generated by generateBody, so we
    // have to reset it here.
    resetOsiPointRegs(ool->lir->safepoint());
#endif

    LInstructionIterator iter = ool->block->begin();
    for (; iter != ool->block->end(); iter++) {
        if (iter->isLabel()) {
            // Nothing to do.
        } else if (iter->isMoveGroup()) {
            // Replay this move group that preceds the interrupt check at the
            // start of the loop header. Any incoming jumps here will be from
            // the backedge and will skip over the move group emitted inline.
            visitMoveGroup(iter->toMoveGroup());
        } else {
            break;
        }
    }
    JS_ASSERT(*iter == ool->lir);

    saveLive(ool->lir);
    if (!callVM(InterruptCheckInfo, ool->lir))
        return false;
    restoreLive(ool->lir);
    masm.jump(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitInterruptCheckImplicit(LInterruptCheckImplicit *lir)
{
    OutOfLineInterruptCheckImplicit *ool = new(alloc()) OutOfLineInterruptCheckImplicit(current, lir);
    if (!addOutOfLineCode(ool))
        return false;

    lir->setOolEntry(ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitTableSwitch(LTableSwitch *ins)
{
    MTableSwitch *mir = ins->mir();
    Label *defaultcase = mir->getDefault()->lir()->label();
    const LAllocation *temp;

    if (mir->getOperand(0)->type() != MIRType_Int32) {
        temp = ins->tempInt()->output();

        // The input is a double, so try and convert it to an integer.
        // If it does not fit in an integer, take the default case.
        masm.convertDoubleToInt32(ToFloatRegister(ins->index()), ToRegister(temp), defaultcase, false);
    } else {
        temp = ins->index();
    }

    return emitTableSwitchDispatch(mir, ToRegister(temp), ToRegisterOrInvalid(ins->tempPointer()));
}

bool
CodeGenerator::visitTableSwitchV(LTableSwitchV *ins)
{
    MTableSwitch *mir = ins->mir();
    Label *defaultcase = mir->getDefault()->lir()->label();

    Register index = ToRegister(ins->tempInt());
    ValueOperand value = ToValue(ins, LTableSwitchV::InputValue);
    Register tag = masm.extractTag(value, index);
    masm.branchTestNumber(Assembler::NotEqual, tag, defaultcase);

    Label unboxInt, isInt;
    masm.branchTestInt32(Assembler::Equal, tag, &unboxInt);
    {
        FloatRegister floatIndex = ToFloatRegister(ins->tempFloat());
        masm.unboxDouble(value, floatIndex);
        masm.convertDoubleToInt32(floatIndex, index, defaultcase, false);
        masm.jump(&isInt);
    }

    masm.bind(&unboxInt);
    masm.unboxInt32(value, index);

    masm.bind(&isInt);

    return emitTableSwitchDispatch(mir, index, ToRegisterOrInvalid(ins->tempPointer()));
}

typedef JSObject *(*DeepCloneObjectLiteralFn)(JSContext *, HandleObject, NewObjectKind);
static const VMFunction DeepCloneObjectLiteralInfo =
    FunctionInfo<DeepCloneObjectLiteralFn>(DeepCloneObjectLiteral);

bool
CodeGenerator::visitCloneLiteral(LCloneLiteral *lir)
{
    pushArg(ImmWord(js::MaybeSingletonObject));
    pushArg(ToRegister(lir->output()));
    return callVM(DeepCloneObjectLiteralInfo, lir);
}

bool
CodeGenerator::visitParameter(LParameter *lir)
{
    return true;
}

bool
CodeGenerator::visitCallee(LCallee *lir)
{
    // read number of actual arguments from the JS frame.
    Register callee = ToRegister(lir->output());
    Address ptr(StackPointer, frameSize() + IonJSFrameLayout::offsetOfCalleeToken());

    masm.loadPtr(ptr, callee);
    return true;
}

bool
CodeGenerator::visitStart(LStart *lir)
{
    return true;
}

bool
CodeGenerator::visitReturn(LReturn *lir)
{
#if defined(JS_NUNBOX32)
    DebugOnly<LAllocation *> type    = lir->getOperand(TYPE_INDEX);
    DebugOnly<LAllocation *> payload = lir->getOperand(PAYLOAD_INDEX);
    JS_ASSERT(ToRegister(type)    == JSReturnReg_Type);
    JS_ASSERT(ToRegister(payload) == JSReturnReg_Data);
#elif defined(JS_PUNBOX64)
    DebugOnly<LAllocation *> result = lir->getOperand(0);
    JS_ASSERT(ToRegister(result) == JSReturnReg);
#endif
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir() != *gen->graph().poBegin())
        masm.jump(&returnLabel_);
    return true;
}

bool
CodeGenerator::visitOsrEntry(LOsrEntry *lir)
{
    // Remember the OSR entry offset into the code buffer.
    masm.flushBuffer();
    setOsrEntryOffset(masm.size());

#if JS_TRACE_LOGGING
    masm.tracelogLog(TraceLogging::INFO_ENGINE_IONMONKEY);
#endif

    // Allocate the full frame for this function.
    uint32_t size = frameSize();
    if (size != 0)
        masm.subPtr(Imm32(size), StackPointer);
    return true;
}

bool
CodeGenerator::visitOsrScopeChain(LOsrScopeChain *lir)
{
    const LAllocation *frame   = lir->getOperand(0);
    const LDefinition *object  = lir->getDef(0);

    const ptrdiff_t frameOffset = BaselineFrame::reverseOffsetOfScopeChain();

    masm.loadPtr(Address(ToRegister(frame), frameOffset), ToRegister(object));
    return true;
}

bool
CodeGenerator::visitOsrArgumentsObject(LOsrArgumentsObject *lir)
{
    const LAllocation *frame   = lir->getOperand(0);
    const LDefinition *object  = lir->getDef(0);

    const ptrdiff_t frameOffset = BaselineFrame::reverseOffsetOfArgsObj();

    masm.loadPtr(Address(ToRegister(frame), frameOffset), ToRegister(object));
    return true;
}

bool
CodeGenerator::visitOsrValue(LOsrValue *value)
{
    const LAllocation *frame   = value->getOperand(0);
    const ValueOperand out     = ToOutValue(value);

    const ptrdiff_t frameOffset = value->mir()->frameOffset();

    masm.loadValue(Address(ToRegister(frame), frameOffset), out);
    return true;
}

bool
CodeGenerator::visitOsrReturnValue(LOsrReturnValue *lir)
{
    const LAllocation *frame   = lir->getOperand(0);
    const ValueOperand out     = ToOutValue(lir);

    Address flags = Address(ToRegister(frame), BaselineFrame::reverseOffsetOfFlags());
    Address retval = Address(ToRegister(frame), BaselineFrame::reverseOffsetOfReturnValue());

    masm.moveValue(UndefinedValue(), out);

    Label done;
    masm.branchTest32(Assembler::Zero, flags, Imm32(BaselineFrame::HAS_RVAL), &done);
    masm.loadValue(retval, out);
    masm.bind(&done);

    return true;
}

bool
CodeGenerator::visitStackArgT(LStackArgT *lir)
{
    const LAllocation *arg = lir->getArgument();
    MIRType argType = lir->type();
    uint32_t argslot = lir->argslot();
    JS_ASSERT(argslot - 1u < graph.argumentSlotCount());

    int32_t stack_offset = StackOffsetOfPassedArg(argslot);
    Address dest(StackPointer, stack_offset);

    if (arg->isFloatReg())
        masm.storeDouble(ToFloatRegister(arg), dest);
    else if (arg->isRegister())
        masm.storeValue(ValueTypeFromMIRType(argType), ToRegister(arg), dest);
    else
        masm.storeValue(*(arg->toConstant()), dest);

    uint32_t slot = StackOffsetToSlot(stack_offset);
    JS_ASSERT(slot - 1u < graph.totalSlotCount());
    return pushedArgumentSlots_.append(slot);
}

bool
CodeGenerator::visitStackArgV(LStackArgV *lir)
{
    ValueOperand val = ToValue(lir, 0);
    uint32_t argslot = lir->argslot();
    JS_ASSERT(argslot - 1u < graph.argumentSlotCount());

    int32_t stack_offset = StackOffsetOfPassedArg(argslot);

    masm.storeValue(val, Address(StackPointer, stack_offset));

    uint32_t slot = StackOffsetToSlot(stack_offset);
    JS_ASSERT(slot - 1u < graph.totalSlotCount());
    return pushedArgumentSlots_.append(slot);
}

bool
CodeGenerator::visitMoveGroup(LMoveGroup *group)
{
    if (!group->numMoves())
        return true;

    MoveResolver &resolver = masm.moveResolver();

    for (size_t i = 0; i < group->numMoves(); i++) {
        const LMove &move = group->getMove(i);

        const LAllocation *from = move.from();
        const LAllocation *to = move.to();
        LDefinition::Type type = move.type();

        // No bogus moves.
        JS_ASSERT(*from != *to);
        JS_ASSERT(!from->isConstant());

        MoveOp::Type moveType;
        switch (type) {
          case LDefinition::OBJECT:
          case LDefinition::SLOTS:
#ifdef JS_NUNBOX32
          case LDefinition::TYPE:
          case LDefinition::PAYLOAD:
#else
          case LDefinition::BOX:
#endif
          case LDefinition::GENERAL: moveType = MoveOp::GENERAL; break;
          case LDefinition::INT32:   moveType = MoveOp::INT32;   break;
          case LDefinition::FLOAT32: moveType = MoveOp::FLOAT32; break;
          case LDefinition::DOUBLE:  moveType = MoveOp::DOUBLE;  break;
          default: MOZ_ASSUME_UNREACHABLE("Unexpected move type");
        }

        if (!resolver.addMove(toMoveOperand(from), toMoveOperand(to), moveType))
            return false;
    }

    if (!resolver.resolve())
        return false;

    MoveEmitter emitter(masm);
    emitter.emit(resolver);
    emitter.finish();

    return true;
}

bool
CodeGenerator::visitInteger(LInteger *lir)
{
    masm.move32(Imm32(lir->getValue()), ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitPointer(LPointer *lir)
{
    if (lir->kind() == LPointer::GC_THING)
        masm.movePtr(ImmGCPtr(lir->gcptr()), ToRegister(lir->output()));
    else
        masm.movePtr(ImmPtr(lir->ptr()), ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitSlots(LSlots *lir)
{
    Address slots(ToRegister(lir->object()), JSObject::offsetOfSlots());
    masm.loadPtr(slots, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitStoreSlotV(LStoreSlotV *store)
{
    Register base = ToRegister(store->slots());
    int32_t offset = store->mir()->slot() * sizeof(Value);

    const ValueOperand value = ToValue(store, LStoreSlotV::Value);

    if (store->mir()->needsBarrier())
       emitPreBarrier(Address(base, offset), MIRType_Value);

    masm.storeValue(value, Address(base, offset));
    return true;
}

bool
CodeGenerator::emitGetPropertyPolymorphic(LInstruction *ins, Register obj, Register scratch,
                                          const TypedOrValueRegister &output)
{
    MGetPropertyPolymorphic *mir = ins->mirRaw()->toGetPropertyPolymorphic();
    JS_ASSERT(mir->numShapes() > 1);

    masm.loadObjShape(obj, scratch);

    Label done;
    for (size_t i = 0; i < mir->numShapes(); i++) {
        Label next;
        masm.branchPtr(Assembler::NotEqual, scratch, ImmGCPtr(mir->objShape(i)), &next);

        Shape *shape = mir->shape(i);
        if (shape->slot() < shape->numFixedSlots()) {
            // Fixed slot.
            masm.loadTypedOrValue(Address(obj, JSObject::getFixedSlotOffset(shape->slot())),
                                  output);
        } else {
            // Dynamic slot.
            uint32_t offset = (shape->slot() - shape->numFixedSlots()) * sizeof(js::Value);
            masm.loadPtr(Address(obj, JSObject::offsetOfSlots()), scratch);
            masm.loadTypedOrValue(Address(scratch, offset), output);
        }

        masm.jump(&done);
        masm.bind(&next);
    }

    // Bailout if no shape matches.
    if (!bailout(ins->snapshot()))
        return false;

    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitGetPropertyPolymorphicV(LGetPropertyPolymorphicV *ins)
{
    Register obj = ToRegister(ins->obj());
    ValueOperand output = GetValueOutput(ins);
    return emitGetPropertyPolymorphic(ins, obj, output.scratchReg(), output);
}

bool
CodeGenerator::visitGetPropertyPolymorphicT(LGetPropertyPolymorphicT *ins)
{
    Register obj = ToRegister(ins->obj());
    TypedOrValueRegister output(ins->mir()->type(), ToAnyRegister(ins->output()));
    Register temp = (output.type() == MIRType_Double)
                    ? ToRegister(ins->temp())
                    : output.typedReg().gpr();
    return emitGetPropertyPolymorphic(ins, obj, temp, output);
}

bool
CodeGenerator::emitSetPropertyPolymorphic(LInstruction *ins, Register obj, Register scratch,
                                          const ConstantOrRegister &value)
{
    MSetPropertyPolymorphic *mir = ins->mirRaw()->toSetPropertyPolymorphic();
    JS_ASSERT(mir->numShapes() > 1);

    masm.loadObjShape(obj, scratch);

    Label done;
    for (size_t i = 0; i < mir->numShapes(); i++) {
        Label next;
        masm.branchPtr(Assembler::NotEqual, scratch, ImmGCPtr(mir->objShape(i)), &next);

        Shape *shape = mir->shape(i);
        if (shape->slot() < shape->numFixedSlots()) {
            // Fixed slot.
            Address addr(obj, JSObject::getFixedSlotOffset(shape->slot()));
            if (mir->needsBarrier())
                emitPreBarrier(addr, MIRType_Value);
            masm.storeConstantOrRegister(value, addr);
        } else {
            // Dynamic slot.
            masm.loadPtr(Address(obj, JSObject::offsetOfSlots()), scratch);
            Address addr(scratch, (shape->slot() - shape->numFixedSlots()) * sizeof(js::Value));
            if (mir->needsBarrier())
                emitPreBarrier(addr, MIRType_Value);
            masm.storeConstantOrRegister(value, addr);
        }

        masm.jump(&done);
        masm.bind(&next);
    }

    // Bailout if no shape matches.
    if (!bailout(ins->snapshot()))
        return false;

    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitSetPropertyPolymorphicV(LSetPropertyPolymorphicV *ins)
{
    Register obj = ToRegister(ins->obj());
    Register temp = ToRegister(ins->temp());
    ValueOperand value = ToValue(ins, LSetPropertyPolymorphicV::Value);
    return emitSetPropertyPolymorphic(ins, obj, temp, TypedOrValueRegister(value));
}

bool
CodeGenerator::visitSetPropertyPolymorphicT(LSetPropertyPolymorphicT *ins)
{
    Register obj = ToRegister(ins->obj());
    Register temp = ToRegister(ins->temp());

    ConstantOrRegister value;
    if (ins->mir()->value()->isConstant())
        value = ConstantOrRegister(ins->mir()->value()->toConstant()->value());
    else
        value = TypedOrValueRegister(ins->mir()->value()->type(), ToAnyRegister(ins->value()));

    return emitSetPropertyPolymorphic(ins, obj, temp, value);
}

bool
CodeGenerator::visitElements(LElements *lir)
{
    Address elements(ToRegister(lir->object()), JSObject::offsetOfElements());
    masm.loadPtr(elements, ToRegister(lir->output()));
    return true;
}

typedef bool (*ConvertElementsToDoublesFn)(JSContext *, uintptr_t);
static const VMFunction ConvertElementsToDoublesInfo =
    FunctionInfo<ConvertElementsToDoublesFn>(ObjectElements::ConvertElementsToDoubles);

bool
CodeGenerator::visitConvertElementsToDoubles(LConvertElementsToDoubles *lir)
{
    Register elements = ToRegister(lir->elements());

    OutOfLineCode *ool = oolCallVM(ConvertElementsToDoublesInfo, lir,
                                   (ArgList(), elements), StoreNothing());
    if (!ool)
        return false;

    Address convertedAddress(elements, ObjectElements::offsetOfFlags());
    Imm32 bit(ObjectElements::CONVERT_DOUBLE_ELEMENTS);
    masm.branchTest32(Assembler::Zero, convertedAddress, bit, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitMaybeToDoubleElement(LMaybeToDoubleElement *lir)
{
    Register elements = ToRegister(lir->elements());
    Register value = ToRegister(lir->value());
    ValueOperand out = ToOutValue(lir);

    FloatRegister temp = ToFloatRegister(lir->tempFloat());
    Label convert, done;

    // If the CONVERT_DOUBLE_ELEMENTS flag is set, convert the int32
    // value to double. Else, just box it.
    masm.branchTest32(Assembler::NonZero,
                      Address(elements, ObjectElements::offsetOfFlags()),
                      Imm32(ObjectElements::CONVERT_DOUBLE_ELEMENTS),
                      &convert);

    masm.tagValue(JSVAL_TYPE_INT32, value, out);
    masm.jump(&done);

    masm.bind(&convert);
    masm.convertInt32ToDouble(value, temp);
    masm.boxDouble(temp, out);

    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitFunctionEnvironment(LFunctionEnvironment *lir)
{
    Address environment(ToRegister(lir->function()), JSFunction::offsetOfEnvironment());
    masm.loadPtr(environment, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitForkJoinContext(LForkJoinContext *lir)
{
    const Register tempReg = ToRegister(lir->getTempReg());

    masm.setupUnalignedABICall(0, tempReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ForkJoinContextPar));
    JS_ASSERT(ToRegister(lir->output()) == ReturnReg);
    return true;
}

bool
CodeGenerator::visitGuardThreadExclusive(LGuardThreadExclusive *lir)
{
    JS_ASSERT(gen->info().executionMode() == ParallelExecution);

    const Register tempReg = ToRegister(lir->getTempReg());
    masm.setupUnalignedABICall(2, tempReg);
    masm.passABIArg(ToRegister(lir->forkJoinContext()));
    masm.passABIArg(ToRegister(lir->object()));
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ParallelWriteGuard));

    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutIllegalWrite, lir);
    if (!bail)
        return false;

    // branch to the OOL failure code if false is returned
    masm.branchIfFalseBool(ReturnReg, bail->entry());
    return true;
}

bool
CodeGenerator::visitGuardObjectIdentity(LGuardObjectIdentity *guard)
{
    Register obj = ToRegister(guard->input());

    Assembler::Condition cond =
        guard->mir()->bailOnEquality() ? Assembler::Equal : Assembler::NotEqual;
    return bailoutCmpPtr(cond, obj, ImmGCPtr(guard->mir()->singleObject()), guard->snapshot());
}

bool
CodeGenerator::visitTypeBarrierV(LTypeBarrierV *lir)
{
    ValueOperand operand = ToValue(lir, LTypeBarrierV::Input);
    Register scratch = ToTempRegisterOrInvalid(lir->temp());

    Label miss;
    masm.guardTypeSet(operand, lir->mir()->resultTypeSet(), scratch, &miss);
    if (!bailoutFrom(&miss, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitTypeBarrierO(LTypeBarrierO *lir)
{
    Register obj = ToRegister(lir->object());
    Register scratch = ToTempRegisterOrInvalid(lir->temp());

    Label miss;
    masm.guardObjectType(obj, lir->mir()->resultTypeSet(), scratch, &miss);
    if (!bailoutFrom(&miss, lir->snapshot()))
        return false;
    return true;
}

bool
CodeGenerator::visitMonitorTypes(LMonitorTypes *lir)
{
    ValueOperand operand = ToValue(lir, LMonitorTypes::Input);
    Register scratch = ToTempUnboxRegister(lir->temp());

    Label matched, miss;
    masm.guardTypeSet(operand, lir->mir()->typeSet(), scratch, &miss);
    if (!bailoutFrom(&miss, lir->snapshot()))
        return false;
    return true;
}

#ifdef JSGC_GENERATIONAL
// Out-of-line path to update the store buffer.
class OutOfLineCallPostWriteBarrier : public OutOfLineCodeBase<CodeGenerator>
{
    LInstruction *lir_;
    const LAllocation *object_;

  public:
    OutOfLineCallPostWriteBarrier(LInstruction *lir, const LAllocation *object)
      : lir_(lir), object_(object)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineCallPostWriteBarrier(this);
    }

    LInstruction *lir() const {
        return lir_;
    }
    const LAllocation *object() const {
        return object_;
    }
};

bool
CodeGenerator::visitOutOfLineCallPostWriteBarrier(OutOfLineCallPostWriteBarrier *ool)
{
    saveLiveVolatile(ool->lir());

    const LAllocation *obj = ool->object();

    GeneralRegisterSet regs = GeneralRegisterSet::Volatile();

    Register objreg;
    bool isGlobal = false;
    if (obj->isConstant()) {
        JSObject *object = &obj->toConstant()->toObject();
        isGlobal = object->is<GlobalObject>();
        objreg = regs.takeAny();
        masm.movePtr(ImmGCPtr(object), objreg);
    } else {
        objreg = ToRegister(obj);
        regs.takeUnchecked(objreg);
    }

    Register runtimereg = regs.takeAny();
    masm.mov(ImmPtr(GetIonContext()->runtime), runtimereg);

    void (*fun)(JSRuntime*, JSObject*) = isGlobal ? PostGlobalWriteBarrier : PostWriteBarrier;
    masm.setupUnalignedABICall(2, regs.takeAny());
    masm.passABIArg(runtimereg);
    masm.passABIArg(objreg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, fun));

    restoreLiveVolatile(ool->lir());

    masm.jump(ool->rejoin());
    return true;
}
#endif

bool
CodeGenerator::visitPostWriteBarrierO(LPostWriteBarrierO *lir)
{
#ifdef JSGC_GENERATIONAL
    OutOfLineCallPostWriteBarrier *ool = new(alloc()) OutOfLineCallPostWriteBarrier(lir, lir->object());
    if (!addOutOfLineCode(ool))
        return false;

    const Nursery &nursery = GetIonContext()->runtime->gcNursery();

    if (lir->object()->isConstant()) {
        JS_ASSERT(!nursery.isInside(&lir->object()->toConstant()->toObject()));
    } else {
        Label tenured;
        Register objreg = ToRegister(lir->object());
        masm.branchPtr(Assembler::Below, objreg, ImmWord(nursery.start()), &tenured);
        masm.branchPtr(Assembler::Below, objreg, ImmWord(nursery.heapEnd()), ool->rejoin());
        masm.bind(&tenured);
    }

    Register valuereg = ToRegister(lir->value());
    masm.branchPtr(Assembler::Below, valuereg, ImmWord(nursery.start()), ool->rejoin());
    masm.branchPtr(Assembler::Below, valuereg, ImmWord(nursery.heapEnd()), ool->entry());

    masm.bind(ool->rejoin());
#endif
    return true;
}

bool
CodeGenerator::visitPostWriteBarrierV(LPostWriteBarrierV *lir)
{
#ifdef JSGC_GENERATIONAL
    OutOfLineCallPostWriteBarrier *ool = new(alloc()) OutOfLineCallPostWriteBarrier(lir, lir->object());
    if (!addOutOfLineCode(ool))
        return false;

    ValueOperand value = ToValue(lir, LPostWriteBarrierV::Input);
    masm.branchTestObject(Assembler::NotEqual, value, ool->rejoin());

    const Nursery &nursery = GetIonContext()->runtime->gcNursery();

    if (lir->object()->isConstant()) {
        JS_ASSERT(!nursery.isInside(&lir->object()->toConstant()->toObject()));
    } else {
        Label tenured;
        Register objreg = ToRegister(lir->object());
        masm.branchPtr(Assembler::Below, objreg, ImmWord(nursery.start()), &tenured);
        masm.branchPtr(Assembler::Below, objreg, ImmWord(nursery.heapEnd()), ool->rejoin());
        masm.bind(&tenured);
    }

    Register valuereg = masm.extractObject(value, ToTempUnboxRegister(lir->temp()));
    masm.branchPtr(Assembler::Below, valuereg, ImmWord(nursery.start()), ool->rejoin());
    masm.branchPtr(Assembler::Below, valuereg, ImmWord(nursery.heapEnd()), ool->entry());

    masm.bind(ool->rejoin());
#endif
    return true;
}

bool
CodeGenerator::visitCallNative(LCallNative *call)
{
    JSFunction *target = call->getSingleTarget();
    JS_ASSERT(target);
    JS_ASSERT(target->isNative());

    int callargslot = call->argslot();
    int unusedStack = StackOffsetOfPassedArg(callargslot);

    // Registers used for callWithABI() argument-passing.
    const Register argContextReg   = ToRegister(call->getArgContextReg());
    const Register argUintNReg     = ToRegister(call->getArgUintNReg());
    const Register argVpReg        = ToRegister(call->getArgVpReg());

    // Misc. temporary registers.
    const Register tempReg = ToRegister(call->getTempReg());

    DebugOnly<uint32_t> initialStack = masm.framePushed();

    masm.checkStackAlignment();

    // Sequential native functions have the signature:
    //  bool (*)(JSContext *, unsigned, Value *vp)
    // and parallel native functions have the signature:
    //  ParallelResult (*)(ForkJoinContext *, unsigned, Value *vp)
    // Where vp[0] is space for an outparam, vp[1] is |this|, and vp[2] onward
    // are the function arguments.

    // Allocate space for the outparam, moving the StackPointer to what will be &vp[1].
    masm.adjustStack(unusedStack);

    // Push a Value containing the callee object: natives are allowed to access their callee before
    // setitng the return value. The StackPointer is moved to &vp[0].
    masm.Push(ObjectValue(*target));

    // Preload arguments into registers.
    //
    // Note that for parallel execution, loadContext does an ABI call, so we
    // need to do this before we load the other argument registers, otherwise
    // we'll hose them.
    ExecutionMode executionMode = gen->info().executionMode();
    masm.loadContext(argContextReg, tempReg, executionMode);
    masm.move32(Imm32(call->numStackArgs()), argUintNReg);
    masm.movePtr(StackPointer, argVpReg);

    masm.Push(argUintNReg);

    // Construct native exit frame.
    uint32_t safepointOffset;
    if (!masm.buildFakeExitFrame(tempReg, &safepointOffset))
        return false;
    masm.enterFakeExitFrame(argContextReg, tempReg, executionMode);

    if (!markSafepointAt(safepointOffset, call))
        return false;

    // Construct and execute call.
    masm.setupUnalignedABICall(3, tempReg);
    masm.passABIArg(argContextReg);
    masm.passABIArg(argUintNReg);
    masm.passABIArg(argVpReg);

    switch (executionMode) {
      case SequentialExecution:
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->native()));
        break;

      case ParallelExecution:
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->parallelNative()));
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    // Test for failure.
    masm.branchIfFalseBool(ReturnReg, masm.failureLabel(executionMode));

    // Load the outparam vp[0] into output register(s).
    masm.loadValue(Address(StackPointer, IonNativeExitFrameLayout::offsetOfResult()), JSReturnOperand);

    // The next instruction is removing the footer of the exit frame, so there
    // is no need for leaveFakeExitFrame.

    // Move the StackPointer back to its original location, unwinding the native exit frame.
    masm.adjustStack(IonNativeExitFrameLayout::Size() - unusedStack);
    JS_ASSERT(masm.framePushed() == initialStack);

    dropArguments(call->numStackArgs() + 1);
    return true;
}

bool
CodeGenerator::visitCallDOMNative(LCallDOMNative *call)
{
    JSFunction *target = call->getSingleTarget();
    JS_ASSERT(target);
    JS_ASSERT(target->isNative());
    JS_ASSERT(target->jitInfo());
    JS_ASSERT(call->mir()->isCallDOMNative());

    int callargslot = call->argslot();
    int unusedStack = StackOffsetOfPassedArg(callargslot);

    // Registers used for callWithABI() argument-passing.
    const Register argJSContext = ToRegister(call->getArgJSContext());
    const Register argObj       = ToRegister(call->getArgObj());
    const Register argPrivate   = ToRegister(call->getArgPrivate());
    const Register argArgs      = ToRegister(call->getArgArgs());

    DebugOnly<uint32_t> initialStack = masm.framePushed();

    masm.checkStackAlignment();

    // DOM methods have the signature:
    //  bool (*)(JSContext *, HandleObject, void *private, const JSJitMethodCallArgs& args)
    // Where args is initialized from an argc and a vp, vp[0] is space for an
    // outparam and the callee, vp[1] is |this|, and vp[2] onward are the
    // function arguments.  Note that args stores the argv, not the vp, and
    // argv == vp + 2.

    // Nestle the stack up against the pushed arguments, leaving StackPointer at
    // &vp[1]
    masm.adjustStack(unusedStack);
    // argObj is filled with the extracted object, then returned.
    Register obj = masm.extractObject(Address(StackPointer, 0), argObj);
    JS_ASSERT(obj == argObj);

    // Push a Value containing the callee object: natives are allowed to access their callee before
    // setitng the return value. After this the StackPointer points to &vp[0].
    masm.Push(ObjectValue(*target));

    // Now compute the argv value.  Since StackPointer is pointing to &vp[0] and
    // argv is &vp[2] we just need to add 2*sizeof(Value) to the current
    // StackPointer.
    JS_STATIC_ASSERT(JSJitMethodCallArgsTraits::offsetOfArgv == 0);
    JS_STATIC_ASSERT(JSJitMethodCallArgsTraits::offsetOfArgc ==
                     IonDOMMethodExitFrameLayoutTraits::offsetOfArgcFromArgv);
    masm.computeEffectiveAddress(Address(StackPointer, 2 * sizeof(Value)), argArgs);

    // GetReservedSlot(obj, DOM_OBJECT_SLOT).toPrivate()
    masm.loadPrivate(Address(obj, JSObject::getFixedSlotOffset(0)), argPrivate);

    // Push argc from the call instruction into what will become the IonExitFrame
    masm.Push(Imm32(call->numStackArgs()));

    // Push our argv onto the stack
    masm.Push(argArgs);
    // And store our JSJitMethodCallArgs* in argArgs.
    masm.movePtr(StackPointer, argArgs);

    // Push |this| object for passing HandleObject. We push after argc to
    // maintain the same sp-relative location of the object pointer with other
    // DOMExitFrames.
    masm.Push(argObj);
    masm.movePtr(StackPointer, argObj);

    // Construct native exit frame.
    uint32_t safepointOffset;
    if (!masm.buildFakeExitFrame(argJSContext, &safepointOffset))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_DOMMETHOD);

    if (!markSafepointAt(safepointOffset, call))
        return false;

    // Construct and execute call.
    masm.setupUnalignedABICall(4, argJSContext);

    masm.loadJSContext(argJSContext);

    masm.passABIArg(argJSContext);
    masm.passABIArg(argObj);
    masm.passABIArg(argPrivate);
    masm.passABIArg(argArgs);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->jitInfo()->method));

    if (target->jitInfo()->isInfallible) {
        masm.loadValue(Address(StackPointer, IonDOMMethodExitFrameLayout::offsetOfResult()),
                       JSReturnOperand);
    } else {
        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        // Load the outparam vp[0] into output register(s).
        masm.loadValue(Address(StackPointer, IonDOMMethodExitFrameLayout::offsetOfResult()),
                       JSReturnOperand);
    }

    // The next instruction is removing the footer of the exit frame, so there
    // is no need for leaveFakeExitFrame.

    // Move the StackPointer back to its original location, unwinding the native exit frame.
    masm.adjustStack(IonDOMMethodExitFrameLayout::Size() - unusedStack);
    JS_ASSERT(masm.framePushed() == initialStack);

    dropArguments(call->numStackArgs() + 1);
    return true;
}

typedef bool (*GetIntrinsicValueFn)(JSContext *cx, HandlePropertyName, MutableHandleValue);
static const VMFunction GetIntrinsicValueInfo =
    FunctionInfo<GetIntrinsicValueFn>(GetIntrinsicValue);

bool
CodeGenerator::visitCallGetIntrinsicValue(LCallGetIntrinsicValue *lir)
{
    pushArg(ImmGCPtr(lir->mir()->name()));
    return callVM(GetIntrinsicValueInfo, lir);
}

typedef bool (*InvokeFunctionFn)(JSContext *, HandleObject, uint32_t, Value *, Value *);
static const VMFunction InvokeFunctionInfo = FunctionInfo<InvokeFunctionFn>(InvokeFunction);

bool
CodeGenerator::emitCallInvokeFunction(LInstruction *call, Register calleereg,
                                      uint32_t argc, uint32_t unusedStack)
{
    // Nestle %esp up to the argument vector.
    // Each path must account for framePushed_ separately, for callVM to be valid.
    masm.freeStack(unusedStack);

    pushArg(StackPointer); // argv.
    pushArg(Imm32(argc));  // argc.
    pushArg(calleereg);    // JSFunction *.

    if (!callVM(InvokeFunctionInfo, call))
        return false;

    // Un-nestle %esp from the argument vector. No prefix was pushed.
    masm.reserveStack(unusedStack);
    return true;
}

bool
CodeGenerator::visitCallGeneric(LCallGeneric *call)
{
    Register calleereg = ToRegister(call->getFunction());
    Register objreg    = ToRegister(call->getTempObject());
    Register nargsreg  = ToRegister(call->getNargsReg());
    uint32_t unusedStack = StackOffsetOfPassedArg(call->argslot());
    ExecutionMode executionMode = gen->info().executionMode();
    Label invoke, thunk, makeCall, end;

    // Known-target case is handled by LCallKnown.
    JS_ASSERT(!call->hasSingleTarget());

    // Generate an ArgumentsRectifier.
    JitCode *argumentsRectifier = gen->jitRuntime()->getArgumentsRectifier(executionMode);

    masm.checkStackAlignment();

    // Guard that calleereg is actually a function object.
    masm.loadObjClass(calleereg, nargsreg);
    masm.branchPtr(Assembler::NotEqual, nargsreg, ImmPtr(&JSFunction::class_), &invoke);

    // Guard that calleereg is an interpreted function with a JSScript.
    // If we are constructing, also ensure the callee is a constructor.
    if (call->mir()->isConstructing())
        masm.branchIfNotInterpretedConstructor(calleereg, nargsreg, &invoke);
    else
        masm.branchIfFunctionHasNoScript(calleereg, &invoke);

    // Knowing that calleereg is a non-native function, load the JSScript.
    masm.loadPtr(Address(calleereg, JSFunction::offsetOfNativeOrScript()), objreg);

    // Load script jitcode.
    masm.loadBaselineOrIonRaw(objreg, objreg, executionMode, &invoke);

    // Nestle the StackPointer up to the argument vector.
    masm.freeStack(unusedStack);

    // Construct the IonFramePrefix.
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS);
    masm.Push(Imm32(call->numActualArgs()));
    masm.Push(calleereg);
    masm.Push(Imm32(descriptor));

    // Check whether the provided arguments satisfy target argc.
    masm.load16ZeroExtend(Address(calleereg, JSFunction::offsetOfNargs()), nargsreg);
    masm.branch32(Assembler::Above, nargsreg, Imm32(call->numStackArgs()), &thunk);
    masm.jump(&makeCall);

    // Argument fixed needed. Load the ArgumentsRectifier.
    masm.bind(&thunk);
    {
        JS_ASSERT(ArgumentsRectifierReg != objreg);
        masm.movePtr(ImmGCPtr(argumentsRectifier), objreg); // Necessary for GC marking.
        masm.loadPtr(Address(objreg, JitCode::offsetOfCode()), objreg);
        masm.move32(Imm32(call->numStackArgs()), ArgumentsRectifierReg);
    }

    // Finally call the function in objreg.
    masm.bind(&makeCall);
    uint32_t callOffset = masm.callIon(objreg);
    if (!markSafepointAt(callOffset, call))
        return false;

    // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
    // The return address has already been removed from the Ion frame.
    int prefixGarbage = sizeof(IonJSFrameLayout) - sizeof(void *);
    masm.adjustStack(prefixGarbage - unusedStack);
    masm.jump(&end);

    // Handle uncompiled or native functions.
    masm.bind(&invoke);
    switch (executionMode) {
      case SequentialExecution:
        if (!emitCallInvokeFunction(call, calleereg, call->numActualArgs(), unusedStack))
            return false;
        break;

      case ParallelExecution:
        if (!emitCallToUncompiledScriptPar(call, calleereg))
            return false;
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    masm.bind(&end);

    // If the return value of the constructing function is Primitive,
    // replace the return value with the Object from CreateThis.
    if (call->mir()->isConstructing()) {
        Label notPrimitive;
        masm.branchTestPrimitive(Assembler::NotEqual, JSReturnOperand, &notPrimitive);
        masm.loadValue(Address(StackPointer, unusedStack), JSReturnOperand);
        masm.bind(&notPrimitive);
    }

    if (!checkForAbortPar(call))
        return false;

    dropArguments(call->numStackArgs() + 1);
    return true;
}

// Generates a call to CallToUncompiledScriptPar() and then bails out.
// |calleeReg| should contain the JSFunction*.
bool
CodeGenerator::emitCallToUncompiledScriptPar(LInstruction *lir, Register calleeReg)
{
    OutOfLineCode *bail = oolAbortPar(ParallelBailoutCalledToUncompiledScript, lir);
    if (!bail)
        return false;

    masm.movePtr(calleeReg, CallTempReg0);
    masm.setupUnalignedABICall(1, CallTempReg1);
    masm.passABIArg(CallTempReg0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, CallToUncompiledScriptPar));
    masm.jump(bail->entry());
    return true;
}

bool
CodeGenerator::visitCallKnown(LCallKnown *call)
{
    Register calleereg = ToRegister(call->getFunction());
    Register objreg    = ToRegister(call->getTempObject());
    uint32_t unusedStack = StackOffsetOfPassedArg(call->argslot());
    DebugOnly<JSFunction *> target = call->getSingleTarget();
    ExecutionMode executionMode = gen->info().executionMode();
    Label end, uncompiled;

    // Native single targets are handled by LCallNative.
    JS_ASSERT(!target->isNative());
    // Missing arguments must have been explicitly appended by the IonBuilder.
    JS_ASSERT(target->nargs() <= call->numStackArgs());

    JS_ASSERT_IF(call->mir()->isConstructing(), target->isInterpretedConstructor());

    masm.checkStackAlignment();

    // The calleereg is known to be a non-native function, but might point to
    // a LazyScript instead of a JSScript.
    masm.branchIfFunctionHasNoScript(calleereg, &uncompiled);

    // Knowing that calleereg is a non-native function, load the JSScript.
    masm.loadPtr(Address(calleereg, JSFunction::offsetOfNativeOrScript()), objreg);

    // Load script jitcode.
    if (call->mir()->needsArgCheck())
        masm.loadBaselineOrIonRaw(objreg, objreg, executionMode, &uncompiled);
    else
        masm.loadBaselineOrIonNoArgCheck(objreg, objreg, executionMode, &uncompiled);

    // Nestle the StackPointer up to the argument vector.
    masm.freeStack(unusedStack);

    // Construct the IonFramePrefix.
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), JitFrame_IonJS);
    masm.Push(Imm32(call->numActualArgs()));
    masm.Push(calleereg);
    masm.Push(Imm32(descriptor));

    // Finally call the function in objreg.
    uint32_t callOffset = masm.callIon(objreg);
    if (!markSafepointAt(callOffset, call))
        return false;

    // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
    // The return address has already been removed from the Ion frame.
    int prefixGarbage = sizeof(IonJSFrameLayout) - sizeof(void *);
    masm.adjustStack(prefixGarbage - unusedStack);
    masm.jump(&end);

    // Handle uncompiled functions.
    masm.bind(&uncompiled);
    switch (executionMode) {
      case SequentialExecution:
        if (!emitCallInvokeFunction(call, calleereg, call->numActualArgs(), unusedStack))
            return false;
        break;

      case ParallelExecution:
        if (!emitCallToUncompiledScriptPar(call, calleereg))
            return false;
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    masm.bind(&end);

    if (!checkForAbortPar(call))
        return false;

    // If the return value of the constructing function is Primitive,
    // replace the return value with the Object from CreateThis.
    if (call->mir()->isConstructing()) {
        Label notPrimitive;
        masm.branchTestPrimitive(Assembler::NotEqual, JSReturnOperand, &notPrimitive);
        masm.loadValue(Address(StackPointer, unusedStack), JSReturnOperand);
        masm.bind(&notPrimitive);
    }

    dropArguments(call->numStackArgs() + 1);
    return true;
}

bool
CodeGenerator::checkForAbortPar(LInstruction *lir)
{
    // In parallel mode, if we call another ion-compiled function and
    // it returns JS_ION_ERROR, that indicates a bailout that we have
    // to propagate up the stack.
    ExecutionMode executionMode = gen->info().executionMode();
    if (executionMode == ParallelExecution) {
        OutOfLinePropagateAbortPar *bail = oolPropagateAbortPar(lir);
        if (!bail)
            return false;
        masm.branchTestMagic(Assembler::Equal, JSReturnOperand, bail->entry());
    }
    return true;
}

bool
CodeGenerator::emitCallInvokeFunction(LApplyArgsGeneric *apply, Register extraStackSize)
{
    Register objreg = ToRegister(apply->getTempObject());
    JS_ASSERT(objreg != extraStackSize);

    // Push the space used by the arguments.
    masm.movePtr(StackPointer, objreg);
    masm.Push(extraStackSize);

    pushArg(objreg);                           // argv.
    pushArg(ToRegister(apply->getArgc()));     // argc.
    pushArg(ToRegister(apply->getFunction())); // JSFunction *.

    // This specialization og callVM restore the extraStackSize after the call.
    if (!callVM(InvokeFunctionInfo, apply, &extraStackSize))
        return false;

    masm.Pop(extraStackSize);
    return true;
}

// Do not bailout after the execution of this function since the stack no longer
// correspond to what is expected by the snapshots.
void
CodeGenerator::emitPushArguments(LApplyArgsGeneric *apply, Register extraStackSpace)
{
    // Holds the function nargs. Initially undefined.
    Register argcreg = ToRegister(apply->getArgc());

    Register copyreg = ToRegister(apply->getTempObject());
    size_t argvOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs();
    Label end;

    // Initialize the loop counter AND Compute the stack usage (if == 0)
    masm.movePtr(argcreg, extraStackSpace);
    masm.branchTestPtr(Assembler::Zero, argcreg, argcreg, &end);

    // Copy arguments.
    {
        Register count = extraStackSpace; // <- argcreg
        Label loop;
        masm.bind(&loop);

        // We remove sizeof(void*) from argvOffset because withtout it we target
        // the address after the memory area that we want to copy.
        BaseIndex disp(StackPointer, argcreg, ScaleFromElemWidth(sizeof(Value)), argvOffset - sizeof(void*));

        // Do not use Push here because other this account to 1 in the framePushed
        // instead of 0.  These push are only counted by argcreg.
        masm.loadPtr(disp, copyreg);
        masm.push(copyreg);

        // Handle 32 bits architectures.
        if (sizeof(Value) == 2 * sizeof(void*)) {
            masm.loadPtr(disp, copyreg);
            masm.push(copyreg);
        }

        masm.decBranchPtr(Assembler::NonZero, count, Imm32(1), &loop);
    }

    // Compute the stack usage.
    masm.movePtr(argcreg, extraStackSpace);
    masm.lshiftPtr(Imm32::ShiftOf(ScaleFromElemWidth(sizeof(Value))), extraStackSpace);

    // Join with all arguments copied and the extra stack usage computed.
    masm.bind(&end);

    // Push |this|.
    masm.addPtr(Imm32(sizeof(Value)), extraStackSpace);
    masm.pushValue(ToValue(apply, LApplyArgsGeneric::ThisIndex));
}

void
CodeGenerator::emitPopArguments(LApplyArgsGeneric *apply, Register extraStackSpace)
{
    // Pop |this| and Arguments.
    masm.freeStack(extraStackSpace);
}

bool
CodeGenerator::visitApplyArgsGeneric(LApplyArgsGeneric *apply)
{
    // Holds the function object.
    Register calleereg = ToRegister(apply->getFunction());

    // Temporary register for modifying the function object.
    Register objreg = ToRegister(apply->getTempObject());
    Register copyreg = ToRegister(apply->getTempCopy());

    // Holds the function nargs. Initially undefined.
    Register argcreg = ToRegister(apply->getArgc());

    // Unless already known, guard that calleereg is actually a function object.
    if (!apply->hasSingleTarget()) {
        masm.loadObjClass(calleereg, objreg);

        ImmPtr ptr = ImmPtr(&JSFunction::class_);
        if (!bailoutCmpPtr(Assembler::NotEqual, objreg, ptr, apply->snapshot()))
            return false;
    }

    // Copy the arguments of the current function.
    emitPushArguments(apply, copyreg);

    masm.checkStackAlignment();

    // If the function is known to be uncompilable, only emit the call to InvokeFunction.
    ExecutionMode executionMode = gen->info().executionMode();
    if (apply->hasSingleTarget()) {
        JSFunction *target = apply->getSingleTarget();
        if (target->isNative()) {
            if (!emitCallInvokeFunction(apply, copyreg))
                return false;
            emitPopArguments(apply, copyreg);
            return true;
        }
    }

    Label end, invoke;

    // Guard that calleereg is an interpreted function with a JSScript:
    if (!apply->hasSingleTarget()) {
        masm.branchIfFunctionHasNoScript(calleereg, &invoke);
    } else {
        // Native single targets are handled by LCallNative.
        JS_ASSERT(!apply->getSingleTarget()->isNative());
    }

    // Knowing that calleereg is a non-native function, load the JSScript.
    masm.loadPtr(Address(calleereg, JSFunction::offsetOfNativeOrScript()), objreg);

    // Load script jitcode.
    masm.loadBaselineOrIonRaw(objreg, objreg, executionMode, &invoke);

    // Call with an Ion frame or a rectifier frame.
    {
        // Create the frame descriptor.
        unsigned pushed = masm.framePushed();
        masm.addPtr(Imm32(pushed), copyreg);
        masm.makeFrameDescriptor(copyreg, JitFrame_IonJS);

        masm.Push(argcreg);
        masm.Push(calleereg);
        masm.Push(copyreg); // descriptor

        Label underflow, rejoin;

        // Check whether the provided arguments satisfy target argc.
        if (!apply->hasSingleTarget()) {
            masm.load16ZeroExtend(Address(calleereg, JSFunction::offsetOfNargs()), copyreg);
            masm.branch32(Assembler::Below, argcreg, copyreg, &underflow);
        } else {
            masm.branch32(Assembler::Below, argcreg, Imm32(apply->getSingleTarget()->nargs()),
                          &underflow);
        }

        // Skip the construction of the rectifier frame because we have no
        // underflow.
        masm.jump(&rejoin);

        // Argument fixup needed. Get ready to call the argumentsRectifier.
        {
            masm.bind(&underflow);

            // Hardcode the address of the argumentsRectifier code.
            JitCode *argumentsRectifier = gen->jitRuntime()->getArgumentsRectifier(executionMode);

            JS_ASSERT(ArgumentsRectifierReg != objreg);
            masm.movePtr(ImmGCPtr(argumentsRectifier), objreg); // Necessary for GC marking.
            masm.loadPtr(Address(objreg, JitCode::offsetOfCode()), objreg);
            masm.movePtr(argcreg, ArgumentsRectifierReg);
        }

        masm.bind(&rejoin);

        // Finally call the function in objreg, as assigned by one of the paths above.
        uint32_t callOffset = masm.callIon(objreg);
        if (!markSafepointAt(callOffset, apply))
            return false;

        // Recover the number of arguments from the frame descriptor.
        masm.loadPtr(Address(StackPointer, 0), copyreg);
        masm.rshiftPtr(Imm32(FRAMESIZE_SHIFT), copyreg);
        masm.subPtr(Imm32(pushed), copyreg);

        // Increment to remove IonFramePrefix; decrement to fill FrameSizeClass.
        // The return address has already been removed from the Ion frame.
        int prefixGarbage = sizeof(IonJSFrameLayout) - sizeof(void *);
        masm.adjustStack(prefixGarbage);
        masm.jump(&end);
    }

    // Handle uncompiled or native functions.
    {
        masm.bind(&invoke);
        if (!emitCallInvokeFunction(apply, copyreg))
            return false;
    }

    // Pop arguments and continue.
    masm.bind(&end);
    emitPopArguments(apply, copyreg);

    return true;
}

bool
CodeGenerator::visitBail(LBail *lir)
{
    return bailout(lir->snapshot());
}

bool
CodeGenerator::visitGetDynamicName(LGetDynamicName *lir)
{
    Register scopeChain = ToRegister(lir->getScopeChain());
    Register name = ToRegister(lir->getName());
    Register temp1 = ToRegister(lir->temp1());
    Register temp2 = ToRegister(lir->temp2());
    Register temp3 = ToRegister(lir->temp3());

    masm.loadJSContext(temp3);

    /* Make space for the outparam. */
    masm.adjustStack(-int32_t(sizeof(Value)));
    masm.movePtr(StackPointer, temp2);

    masm.setupUnalignedABICall(4, temp1);
    masm.passABIArg(temp3);
    masm.passABIArg(scopeChain);
    masm.passABIArg(name);
    masm.passABIArg(temp2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, GetDynamicName));

    const ValueOperand out = ToOutValue(lir);

    masm.loadValue(Address(StackPointer, 0), out);
    masm.adjustStack(sizeof(Value));

    Label undefined;
    masm.branchTestUndefined(Assembler::Equal, out, &undefined);
    return bailoutFrom(&undefined, lir->snapshot());
}

bool
CodeGenerator::emitFilterArgumentsOrEval(LInstruction *lir, Register string,
                                         Register temp1, Register temp2)
{
    masm.loadJSContext(temp2);

    masm.setupUnalignedABICall(2, temp1);
    masm.passABIArg(temp2);
    masm.passABIArg(string);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, FilterArgumentsOrEval));

    Label bail;
    masm.branchIfFalseBool(ReturnReg, &bail);
    return bailoutFrom(&bail, lir->snapshot());
}

bool
CodeGenerator::visitFilterArgumentsOrEvalS(LFilterArgumentsOrEvalS *lir)
{
    return emitFilterArgumentsOrEval(lir, ToRegister(lir->getString()),
                                     ToRegister(lir->temp1()),
                                     ToRegister(lir->temp2()));
}

bool
CodeGenerator::visitFilterArgumentsOrEvalV(LFilterArgumentsOrEvalV *lir)
{
    ValueOperand input = ToValue(lir, LFilterArgumentsOrEvalV::Input);

    // Act as nop on non-strings.
    Label done;
    masm.branchTestString(Assembler::NotEqual, input, &done);

    if (!emitFilterArgumentsOrEval(lir, masm.extractString(input, ToRegister(lir->temp3())),
                                   ToRegister(lir->temp1()), ToRegister(lir->temp2())))
    {
        return false;
    }

    masm.bind(&done);
    return true;
}

typedef bool (*DirectEvalSFn)(JSContext *, HandleObject, HandleScript, HandleValue, HandleString,
                              jsbytecode *, MutableHandleValue);
static const VMFunction DirectEvalStringInfo = FunctionInfo<DirectEvalSFn>(DirectEvalStringFromIon);

bool
CodeGenerator::visitCallDirectEvalS(LCallDirectEvalS *lir)
{
    Register scopeChain = ToRegister(lir->getScopeChain());
    Register string = ToRegister(lir->getString());

    pushArg(ImmPtr(lir->mir()->pc()));
    pushArg(string);
    pushArg(ToValue(lir, LCallDirectEvalS::ThisValue));
    pushArg(ImmGCPtr(gen->info().script()));
    pushArg(scopeChain);

    return callVM(DirectEvalStringInfo, lir);
}

typedef bool (*DirectEvalVFn)(JSContext *, HandleObject, HandleScript, HandleValue, HandleValue,
                              jsbytecode *, MutableHandleValue);
static const VMFunction DirectEvalValueInfo = FunctionInfo<DirectEvalVFn>(DirectEvalValueFromIon);

bool
CodeGenerator::visitCallDirectEvalV(LCallDirectEvalV *lir)
{
    Register scopeChain = ToRegister(lir->getScopeChain());

    pushArg(ImmPtr(lir->mir()->pc()));
    pushArg(ToValue(lir, LCallDirectEvalV::Argument));
    pushArg(ToValue(lir, LCallDirectEvalV::ThisValue));
    pushArg(ImmGCPtr(gen->info().script()));
    pushArg(scopeChain);

    return callVM(DirectEvalValueInfo, lir);
}

// Registers safe for use before generatePrologue().
static const uint32_t EntryTempMask = Registers::TempMask & ~(1 << OsrFrameReg.code());

bool
CodeGenerator::generateArgumentsChecks(bool bailout)
{
    // This function can be used the normal way to check the argument types,
    // before entering the function and bailout when arguments don't match.
    // For debug purpose, this is can also be used to force/check that the
    // arguments are correct. Upon fail it will hit a breakpoint.

    MIRGraph &mir = gen->graph();
    MResumePoint *rp = mir.entryResumePoint();

    // Reserve the amount of stack the actual frame will use. We have to undo
    // this before falling through to the method proper though, because the
    // monomorphic call case will bypass this entire path.
    masm.reserveStack(frameSize());

    // No registers are allocated yet, so it's safe to grab anything.
    Register temp = GeneralRegisterSet(EntryTempMask).getAny();

    CompileInfo &info = gen->info();

    Label miss;
    for (uint32_t i = info.startArgSlot(); i < info.endArgSlot(); i++) {
        // All initial parameters are guaranteed to be MParameters.
        MParameter *param = rp->getOperand(i)->toParameter();
        const types::TypeSet *types = param->resultTypeSet();
        if (!types || types->unknown())
            continue;

        // Calculate the offset on the stack of the argument.
        // (i - info.startArgSlot())    - Compute index of arg within arg vector.
        // ... * sizeof(Value)          - Scale by value size.
        // ArgToStackOffset(...)        - Compute displacement within arg vector.
        int32_t offset = ArgToStackOffset((i - info.startArgSlot()) * sizeof(Value));
        masm.guardTypeSet(Address(StackPointer, offset), types, temp, &miss);
    }

    if (miss.used()) {
        if (bailout) {
            if (!bailoutFrom(&miss, graph.entrySnapshot()))
                return false;
        } else {
            Label success;
            masm.jump(&success);
            masm.bind(&miss);
            masm.assumeUnreachable("Argument check fail.");
            masm.bind(&success);
        }
    }

    masm.freeStack(frameSize());

    return true;
}

// Out-of-line path to report over-recursed error and fail.
class CheckOverRecursedFailure : public OutOfLineCodeBase<CodeGenerator>
{
    LCheckOverRecursed *lir_;

  public:
    CheckOverRecursedFailure(LCheckOverRecursed *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitCheckOverRecursedFailure(this);
    }

    LCheckOverRecursed *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitCheckOverRecursed(LCheckOverRecursed *lir)
{
    // Ensure that this frame will not cross the stack limit.
    // This is a weak check, justified by Ion using the C stack: we must always
    // be some distance away from the actual limit, since if the limit is
    // crossed, an error must be thrown, which requires more frames.
    //
    // It must always be possible to trespass past the stack limit.
    // Ion may legally place frames very close to the limit. Calling additional
    // C functions may then violate the limit without any checking.

    // Since Ion frames exist on the C stack, the stack limit may be
    // dynamically set by JS_SetThreadStackLimit() and JS_SetNativeStackQuota().
    const void *limitAddr = GetIonContext()->runtime->addressOfJitStackLimit();

    CheckOverRecursedFailure *ool = new(alloc()) CheckOverRecursedFailure(lir);
    if (!addOutOfLineCode(ool))
        return false;

    // Conditional forward (unlikely) branch to failure.
    masm.branchPtr(Assembler::AboveOrEqual, AbsoluteAddress(limitAddr), StackPointer, ool->entry());
    masm.bind(ool->rejoin());

    return true;
}

typedef bool (*DefVarOrConstFn)(JSContext *, HandlePropertyName, unsigned, HandleObject);
static const VMFunction DefVarOrConstInfo =
    FunctionInfo<DefVarOrConstFn>(DefVarOrConst);

bool
CodeGenerator::visitDefVar(LDefVar *lir)
{
    Register scopeChain = ToRegister(lir->scopeChain());

    pushArg(scopeChain); // JSObject *
    pushArg(Imm32(lir->mir()->attrs())); // unsigned
    pushArg(ImmGCPtr(lir->mir()->name())); // PropertyName *

    if (!callVM(DefVarOrConstInfo, lir))
        return false;

    return true;
}

typedef bool (*DefFunOperationFn)(JSContext *, HandleScript, HandleObject, HandleFunction);
static const VMFunction DefFunOperationInfo = FunctionInfo<DefFunOperationFn>(DefFunOperation);

bool
CodeGenerator::visitDefFun(LDefFun *lir)
{
    Register scopeChain = ToRegister(lir->scopeChain());

    pushArg(ImmGCPtr(lir->mir()->fun()));
    pushArg(scopeChain);
    pushArg(ImmGCPtr(current->mir()->info().script()));

    return callVM(DefFunOperationInfo, lir);
}

typedef bool (*ReportOverRecursedFn)(JSContext *);
static const VMFunction CheckOverRecursedInfo =
    FunctionInfo<ReportOverRecursedFn>(CheckOverRecursed);

bool
CodeGenerator::visitCheckOverRecursedFailure(CheckOverRecursedFailure *ool)
{
    // The OOL path is hit if the recursion depth has been exceeded.
    // Throw an InternalError for over-recursion.

    // LFunctionEnvironment can appear before LCheckOverRecursed, so we have
    // to save all live registers to avoid crashes if CheckOverRecursed triggers
    // a GC.
    saveLive(ool->lir());

    if (!callVM(CheckOverRecursedInfo, ool->lir()))
        return false;

    restoreLive(ool->lir());
    masm.jump(ool->rejoin());
    return true;
}

// Out-of-line path to report over-recursed error and fail.
class CheckOverRecursedFailurePar : public OutOfLineCodeBase<CodeGenerator>
{
    LCheckOverRecursedPar *lir_;

  public:
    CheckOverRecursedFailurePar(LCheckOverRecursedPar *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitCheckOverRecursedFailurePar(this);
    }

    LCheckOverRecursedPar *lir() const {
        return lir_;
    }
};

bool
CodeGenerator::visitCheckOverRecursedPar(LCheckOverRecursedPar *lir)
{
    // See above: unlike visitCheckOverRecursed(), this code runs in
    // parallel mode and hence uses the jitStackLimit from the current
    // thread state.  Also, we must check the interrupt flags because
    // on interrupt or abort, only the stack limit for the main thread
    // is reset, not the worker threads.  See comment in vm/ForkJoin.h
    // for more details.

    Register cxReg = ToRegister(lir->forkJoinContext());
    Register tempReg = ToRegister(lir->getTempReg());

    masm.loadPtr(Address(cxReg, offsetof(ForkJoinContext, perThreadData)), tempReg);
    masm.loadPtr(Address(tempReg, offsetof(PerThreadData, jitStackLimit)), tempReg);

    // Conditional forward (unlikely) branch to failure.
    CheckOverRecursedFailurePar *ool = new(alloc()) CheckOverRecursedFailurePar(lir);
    if (!addOutOfLineCode(ool))
        return false;
    masm.branchPtr(Assembler::BelowOrEqual, StackPointer, tempReg, ool->entry());
    masm.checkInterruptFlagPar(tempReg, ool->entry());
    masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitCheckOverRecursedFailurePar(CheckOverRecursedFailurePar *ool)
{
    OutOfLinePropagateAbortPar *bail = oolPropagateAbortPar(ool->lir());
    if (!bail)
        return false;

    // Avoid saving/restoring the temp register since we will put the
    // ReturnReg into it below and we don't want to clobber that
    // during PopRegsInMask():
    LCheckOverRecursedPar *lir = ool->lir();
    Register tempReg = ToRegister(lir->getTempReg());
    RegisterSet saveSet(lir->safepoint()->liveRegs());
    saveSet.takeUnchecked(tempReg);

    masm.PushRegsInMask(saveSet);
    masm.movePtr(ToRegister(lir->forkJoinContext()), CallTempReg0);
    masm.setupUnalignedABICall(1, CallTempReg1);
    masm.passABIArg(CallTempReg0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, CheckOverRecursedPar));
    masm.movePtr(ReturnReg, tempReg);
    masm.PopRegsInMask(saveSet);
    masm.branchIfFalseBool(tempReg, bail->entry());
    masm.jump(ool->rejoin());

    return true;
}

// Out-of-line path to report over-recursed error and fail.
class OutOfLineInterruptCheckPar : public OutOfLineCodeBase<CodeGenerator>
{
  public:
    LInterruptCheckPar *const lir;

    OutOfLineInterruptCheckPar(LInterruptCheckPar *lir)
      : lir(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineInterruptCheckPar(this);
    }
};

bool
CodeGenerator::visitInterruptCheckPar(LInterruptCheckPar *lir)
{
    // First check for cx->shared->interrupt_.
    OutOfLineInterruptCheckPar *ool = new(alloc()) OutOfLineInterruptCheckPar(lir);
    if (!addOutOfLineCode(ool))
        return false;

    Register tempReg = ToRegister(lir->getTempReg());
    masm.checkInterruptFlagPar(tempReg, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineInterruptCheckPar(OutOfLineInterruptCheckPar *ool)
{
    OutOfLinePropagateAbortPar *bail = oolPropagateAbortPar(ool->lir);
    if (!bail)
        return false;

    // Avoid saving/restoring the temp register since we will put the
    // ReturnReg into it below and we don't want to clobber that
    // during PopRegsInMask():
    LInterruptCheckPar *lir = ool->lir;
    Register tempReg = ToRegister(lir->getTempReg());
    RegisterSet saveSet(lir->safepoint()->liveRegs());
    saveSet.takeUnchecked(tempReg);

    masm.PushRegsInMask(saveSet);
    masm.movePtr(ToRegister(ool->lir->forkJoinContext()), CallTempReg0);
    masm.setupUnalignedABICall(1, CallTempReg1);
    masm.passABIArg(CallTempReg0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, InterruptCheckPar));
    masm.movePtr(ReturnReg, tempReg);
    masm.PopRegsInMask(saveSet);
    masm.branchIfFalseBool(tempReg, bail->entry());
    masm.jump(ool->rejoin());

    return true;
}

IonScriptCounts *
CodeGenerator::maybeCreateScriptCounts()
{
    // If scripts are being profiled, create a new IonScriptCounts and attach
    // it to the script. This must be done on the main thread.
    JSContext *cx = GetIonContext()->cx;
    if (!cx || !cx->runtime()->profilingScripts)
        return nullptr;

    IonScriptCounts *counts = nullptr;

    CompileInfo *outerInfo = &gen->info();
    JSScript *script = outerInfo->script();

    if (script && !script->hasScriptCounts() && !script->initScriptCounts(cx))
        return nullptr;

    counts = js_new<IonScriptCounts>();
    if (!counts || !counts->init(graph.numBlocks())) {
        js_delete(counts);
        return nullptr;
    }

    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i)->mir();

        uint32_t offset = 0;
        if (script) {
            // Find a PC offset in the outermost script to use. If this block
            // is from an inlined script, find a location in the outer script
            // to associate information about the inlining with.
            MResumePoint *resume = block->entryResumePoint();
            while (resume->caller())
                resume = resume->caller();
            offset = script->pcToOffset(resume->pc());
        }

        if (!counts->block(i).init(block->id(), offset, block->numSuccessors())) {
            js_delete(counts);
            return nullptr;
        }
        for (size_t j = 0; j < block->numSuccessors(); j++)
            counts->block(i).setSuccessor(j, block->getSuccessor(j)->id());
    }

    if (script) {
        script->addIonCounts(counts);
    } else {
        // Compiling code for Asm.js. Leave the counts on the CodeGenerator to
        // be picked up by the AsmJSModule after generation finishes.
        unassociatedScriptCounts_ = counts;
    }

    return counts;
}

// Structure for managing the state tracked for a block by script counters.
struct ScriptCountBlockState
{
    IonBlockCounts &block;
    MacroAssembler &masm;

    Sprinter printer;

    uint32_t instructionBytes;
    uint32_t spillBytes;

    // Pointer to instructionBytes, spillBytes, or nullptr, depending on the
    // last instruction processed.
    uint32_t *last;
    uint32_t lastLength;

  public:
    ScriptCountBlockState(IonBlockCounts *block, MacroAssembler *masm)
      : block(*block), masm(*masm),
        printer(GetIonContext()->cx),
        instructionBytes(0), spillBytes(0), last(nullptr), lastLength(0)
    {
    }

    bool init()
    {
        if (!printer.init())
            return false;

        // Bump the hit count for the block at the start. This code is not
        // included in either the text for the block or the instruction byte
        // counts.
        masm.inc64(AbsoluteAddress(block.addressOfHitCount()));

        // Collect human readable assembly for the code generated in the block.
        masm.setPrinter(&printer);

        return true;
    }

    void visitInstruction(LInstruction *ins)
    {
        if (last)
            *last += masm.size() - lastLength;
        lastLength = masm.size();
        last = ins->isMoveGroup() ? &spillBytes : &instructionBytes;

        // Prefix stream of assembly instructions with their LIR instruction
        // name and any associated high level info.
        if (const char *extra = ins->extraName())
            printer.printf("[%s:%s]\n", ins->opName(), extra);
        else
            printer.printf("[%s]\n", ins->opName());
    }

    ~ScriptCountBlockState()
    {
        masm.setPrinter(nullptr);

        if (last)
            *last += masm.size() - lastLength;

        block.setCode(printer.string());
        block.setInstructionBytes(instructionBytes);
        block.setSpillBytes(spillBytes);
    }
};

#ifdef DEBUG
bool
CodeGenerator::branchIfInvalidated(Register temp, Label *invalidated)
{
    CodeOffsetLabel label = masm.movWithPatch(ImmWord(uintptr_t(-1)), temp);
    if (!ionScriptLabels_.append(label))
        return false;

    // If IonScript::refcount != 0, the script has been invalidated.
    masm.branch32(Assembler::NotEqual,
                  Address(temp, IonScript::offsetOfRefcount()),
                  Imm32(0),
                  invalidated);
    return true;
}

bool
CodeGenerator::emitObjectOrStringResultChecks(LInstruction *lir, MDefinition *mir)
{
    if (lir->numDefs() == 0)
        return true;

    JS_ASSERT(lir->numDefs() == 1);
    Register output = ToRegister(lir->getDef(0));

    GeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(output);

    Register temp = regs.takeAny();
    masm.push(temp);

    // Don't check if the script has been invalidated. In that case invalid
    // types are expected (until we reach the OsiPoint and bailout).
    Label done;
    if (!branchIfInvalidated(temp, &done))
        return false;

    if (mir->type() == MIRType_Object &&
        mir->resultTypeSet() &&
        !mir->resultTypeSet()->unknownObject())
    {
        // We have a result TypeSet, assert this object is in it.
        Label miss, ok;
        if (mir->resultTypeSet()->getObjectCount() > 0)
            masm.guardObjectType(output, mir->resultTypeSet(), temp, &miss);
        else
            masm.jump(&miss);
        masm.jump(&ok);

        masm.bind(&miss);
        masm.assumeUnreachable("MIR instruction returned object with unexpected type");

        masm.bind(&ok);
    }

    // Check that we have a valid GC pointer.
    if (gen->info().executionMode() != ParallelExecution) {
        saveVolatile();
        masm.setupUnalignedABICall(2, temp);
        masm.loadJSContext(temp);
        masm.passABIArg(temp);
        masm.passABIArg(output);
        masm.callWithABINoProfiling(mir->type() == MIRType_Object
                                    ? JS_FUNC_TO_DATA_PTR(void *, AssertValidObjectPtr)
                                    : JS_FUNC_TO_DATA_PTR(void *, AssertValidStringPtr));
        restoreVolatile();
    }

    masm.bind(&done);
    masm.pop(temp);
    return true;
}

bool
CodeGenerator::emitValueResultChecks(LInstruction *lir, MDefinition *mir)
{
    if (lir->numDefs() == 0)
        return true;

    JS_ASSERT(lir->numDefs() == BOX_PIECES);
    if (!lir->getDef(0)->output()->isRegister())
        return true;

    ValueOperand output = ToOutValue(lir);

    GeneralRegisterSet regs(GeneralRegisterSet::All());
    regs.take(output);

    Register temp1 = regs.takeAny();
    Register temp2 = regs.takeAny();
    masm.push(temp1);
    masm.push(temp2);

    // Don't check if the script has been invalidated. In that case invalid
    // types are expected (until we reach the OsiPoint and bailout).
    Label done;
    if (!branchIfInvalidated(temp1, &done))
        return false;

    if (mir->resultTypeSet() && !mir->resultTypeSet()->unknown()) {
        // We have a result TypeSet, assert this value is in it.
        Label miss, ok;
        masm.guardTypeSet(output, mir->resultTypeSet(), temp1, &miss);
        masm.jump(&ok);

        masm.bind(&miss);
        masm.assumeUnreachable("MIR instruction returned value with unexpected type");

        masm.bind(&ok);
    }

    // Check that we have a valid GC pointer.
    if (gen->info().executionMode() != ParallelExecution) {
        saveVolatile();

        masm.pushValue(output);
        masm.movePtr(StackPointer, temp1);

        masm.setupUnalignedABICall(2, temp2);
        masm.loadJSContext(temp2);
        masm.passABIArg(temp2);
        masm.passABIArg(temp1);
        masm.callWithABINoProfiling(JS_FUNC_TO_DATA_PTR(void *, AssertValidValue));
        masm.popValue(output);
        restoreVolatile();
    }

    masm.bind(&done);
    masm.pop(temp2);
    masm.pop(temp1);
    return true;
}

bool
CodeGenerator::emitDebugResultChecks(LInstruction *ins)
{
    // In debug builds, check that LIR instructions return valid values.

    MDefinition *mir = ins->mirRaw();
    if (!mir)
        return true;

    switch (mir->type()) {
      case MIRType_Object:
      case MIRType_String:
        return emitObjectOrStringResultChecks(ins, mir);
      case MIRType_Value:
        return emitValueResultChecks(ins, mir);
      default:
        return true;
    }
}
#endif

bool
CodeGenerator::generateBody()
{
    IonScriptCounts *counts = maybeCreateScriptCounts();

#if defined(JS_ION_PERF)
    PerfSpewer *perfSpewer = &perfSpewer_;
    if (gen->compilingAsmJS())
        perfSpewer = &gen->perfSpewer();
#endif

    for (size_t i = 0; i < graph.numBlocks(); i++) {
        current = graph.getBlock(i);
        masm.bind(current->label());

        mozilla::Maybe<ScriptCountBlockState> blockCounts;
        if (counts) {
            blockCounts.construct(&counts->block(i), &masm);
            if (!blockCounts.ref().init())
                return false;
        }

#if defined(JS_ION_PERF)
        perfSpewer->startBasicBlock(current->mir(), masm);
#endif

        for (LInstructionIterator iter = current->begin(); iter != current->end(); iter++) {
            IonSpewStart(IonSpew_Codegen, "instruction %s", iter->opName());
#ifdef DEBUG
            if (const char *extra = iter->extraName())
                IonSpewCont(IonSpew_Codegen, ":%s", extra);
#endif
            IonSpewFin(IonSpew_Codegen);

            if (counts)
                blockCounts.ref().visitInstruction(*iter);

            if (iter->safepoint() && pushedArgumentSlots_.length()) {
                if (!markArgumentSlots(iter->safepoint()))
                    return false;
            }

#ifdef CHECK_OSIPOINT_REGISTERS
            if (iter->safepoint())
                resetOsiPointRegs(iter->safepoint());
#endif

            if (!callTraceLIR(i, *iter))
                return false;

            if (!iter->accept(this))
                return false;

#ifdef DEBUG
            if (!emitDebugResultChecks(*iter))
                return false;
#endif
        }
        if (masm.oom())
            return false;

#if defined(JS_ION_PERF)
        perfSpewer->endBasicBlock(masm);
#endif
    }

    JS_ASSERT(pushedArgumentSlots_.empty());
    return true;
}

// Out-of-line object allocation for LNewArray.
class OutOfLineNewArray : public OutOfLineCodeBase<CodeGenerator>
{
    LNewArray *lir_;

  public:
    OutOfLineNewArray(LNewArray *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewArray(this);
    }

    LNewArray *lir() const {
        return lir_;
    }
};

typedef JSObject *(*NewInitArrayFn)(JSContext *, uint32_t, types::TypeObject *);
static const VMFunction NewInitArrayInfo =
    FunctionInfo<NewInitArrayFn>(NewInitArray);

bool
CodeGenerator::visitNewArrayCallVM(LNewArray *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);

    Register objReg = ToRegister(lir->output());

    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    JSObject *templateObject = lir->mir()->templateObject();
    types::TypeObject *type =
        templateObject->hasSingletonType() ? nullptr : templateObject->type();

    pushArg(ImmGCPtr(type));
    pushArg(Imm32(lir->mir()->count()));

    if (!callVM(NewInitArrayInfo, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);

    return true;
}

typedef JSObject *(*NewDerivedTypedObjectFn)(JSContext *,
                                             HandleObject type,
                                             HandleObject owner,
                                             int32_t offset);
static const VMFunction CreateDerivedTypedObjInfo =
    FunctionInfo<NewDerivedTypedObjectFn>(CreateDerivedTypedObj);

bool
CodeGenerator::visitNewDerivedTypedObject(LNewDerivedTypedObject *lir)
{
    // Not yet made safe for par exec:
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);

    pushArg(ToRegister(lir->offset()));
    pushArg(ToRegister(lir->owner()));
    pushArg(ToRegister(lir->type()));
    return callVM(CreateDerivedTypedObjInfo, lir);
}

bool
CodeGenerator::visitNewSlots(LNewSlots *lir)
{
    Register temp1 = ToRegister(lir->temp1());
    Register temp2 = ToRegister(lir->temp2());
    Register temp3 = ToRegister(lir->temp3());
    Register output = ToRegister(lir->output());

    masm.mov(ImmPtr(GetIonContext()->runtime), temp1);
    masm.mov(ImmWord(lir->mir()->nslots()), temp2);

    masm.setupUnalignedABICall(2, temp3);
    masm.passABIArg(temp1);
    masm.passABIArg(temp2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NewSlots));

    if (!bailoutTestPtr(Assembler::Zero, output, output, lir->snapshot()))
        return false;

    return true;
}

bool CodeGenerator::visitAtan2D(LAtan2D *lir)
{
    Register temp = ToRegister(lir->temp());
    FloatRegister y = ToFloatRegister(lir->y());
    FloatRegister x = ToFloatRegister(lir->x());

    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(y, MoveOp::DOUBLE);
    masm.passABIArg(x, MoveOp::DOUBLE);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ecmaAtan2), MoveOp::DOUBLE);

    JS_ASSERT(ToFloatRegister(lir->output()) == ReturnFloatReg);
    return true;
}

bool CodeGenerator::visitHypot(LHypot *lir)
{
    Register temp = ToRegister(lir->temp());
    FloatRegister x = ToFloatRegister(lir->x());
    FloatRegister y = ToFloatRegister(lir->y());

    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(x, MoveOp::DOUBLE);
    masm.passABIArg(y, MoveOp::DOUBLE);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ecmaHypot), MoveOp::DOUBLE);

    JS_ASSERT(ToFloatRegister(lir->output()) == ReturnFloatReg);
    return true;
}

bool
CodeGenerator::visitNewArray(LNewArray *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);
    Register objReg = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());
    JSObject *templateObject = lir->mir()->templateObject();
    DebugOnly<uint32_t> count = lir->mir()->count();

    JS_ASSERT(count < JSObject::NELEMENTS_LIMIT);

    if (lir->mir()->shouldUseVM())
        return visitNewArrayCallVM(lir);

    OutOfLineNewArray *ool = new(alloc()) OutOfLineNewArray(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.newGCThing(objReg, tempReg, templateObject, ool->entry(), lir->mir()->initialHeap());
    masm.initGCThing(objReg, tempReg, templateObject);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineNewArray(OutOfLineNewArray *ool)
{
    if (!visitNewArrayCallVM(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

// Out-of-line object allocation for JSOP_NEWOBJECT.
class OutOfLineNewObject : public OutOfLineCodeBase<CodeGenerator>
{
    LNewObject *lir_;

  public:
    OutOfLineNewObject(LNewObject *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewObject(this);
    }

    LNewObject *lir() const {
        return lir_;
    }
};

typedef JSObject *(*NewInitObjectFn)(JSContext *, HandleObject);
static const VMFunction NewInitObjectInfo = FunctionInfo<NewInitObjectFn>(NewInitObject);

typedef JSObject *(*NewInitObjectWithClassPrototypeFn)(JSContext *, HandleObject);
static const VMFunction NewInitObjectWithClassPrototypeInfo =
    FunctionInfo<NewInitObjectWithClassPrototypeFn>(NewInitObjectWithClassPrototype);

bool
CodeGenerator::visitNewObjectVMCall(LNewObject *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);

    Register objReg = ToRegister(lir->output());

    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    pushArg(ImmGCPtr(lir->mir()->templateObject()));

    // If we're making a new object with a class prototype (that is, an object
    // that derives its class from its prototype instead of being
    // JSObject::class_'d) from self-hosted code, we need a different init
    // function.
    if (lir->mir()->templateObjectIsClassPrototype()) {
        if (!callVM(NewInitObjectWithClassPrototypeInfo, lir))
            return false;
    } else if (!callVM(NewInitObjectInfo, lir)) {
        return false;
    }

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);
    return true;
}

bool
CodeGenerator::visitNewObject(LNewObject *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);
    Register objReg = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());
    JSObject *templateObject = lir->mir()->templateObject();

    if (lir->mir()->shouldUseVM())
        return visitNewObjectVMCall(lir);

    OutOfLineNewObject *ool = new(alloc()) OutOfLineNewObject(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.newGCThing(objReg, tempReg, templateObject, ool->entry(), lir->mir()->initialHeap());
    masm.initGCThing(objReg, tempReg, templateObject);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineNewObject(OutOfLineNewObject *ool)
{
    if (!visitNewObjectVMCall(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

typedef js::DeclEnvObject *(*NewDeclEnvObjectFn)(JSContext *, HandleFunction, gc::InitialHeap);
static const VMFunction NewDeclEnvObjectInfo =
    FunctionInfo<NewDeclEnvObjectFn>(DeclEnvObject::createTemplateObject);

bool
CodeGenerator::visitNewDeclEnvObject(LNewDeclEnvObject *lir)
{
    Register objReg = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());
    JSObject *templateObj = lir->mir()->templateObj();
    CompileInfo &info = lir->mir()->block()->info();

    // If we have a template object, we can inline call object creation.
    OutOfLineCode *ool = oolCallVM(NewDeclEnvObjectInfo, lir,
                                   (ArgList(), ImmGCPtr(info.funMaybeLazy()),
                                    Imm32(gc::DefaultHeap)),
                                   StoreRegisterTo(objReg));
    if (!ool)
        return false;

    masm.newGCThing(objReg, tempReg, templateObj, ool->entry(), gc::DefaultHeap);
    masm.initGCThing(objReg, tempReg, templateObj);
    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*NewCallObjectFn)(JSContext *, HandleShape, HandleTypeObject, HeapSlot *);
static const VMFunction NewCallObjectInfo =
    FunctionInfo<NewCallObjectFn>(NewCallObject);

bool
CodeGenerator::visitNewCallObject(LNewCallObject *lir)
{
    Register objReg = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());

    JSObject *templateObj = lir->mir()->templateObject();

    OutOfLineCode *ool;
    if (lir->slots()->isRegister()) {
        ool = oolCallVM(NewCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(templateObj->lastProperty()),
                                    ImmGCPtr(templateObj->type()),
                                    ToRegister(lir->slots())),
                        StoreRegisterTo(objReg));
    } else {
        ool = oolCallVM(NewCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(templateObj->lastProperty()),
                                    ImmGCPtr(templateObj->type()),
                                    ImmPtr(nullptr)),
                        StoreRegisterTo(objReg));
    }
    if (!ool)
        return false;

#ifdef JSGC_GENERATIONAL
    if (templateObj->hasDynamicSlots()) {
        // Slot initialization is unbarriered in this case, so we must either
        // allocate in the nursery or bail if that is not possible.
        masm.jump(ool->entry());
    } else
#endif
    {
        // Inline call object creation, using the OOL path only for tricky cases.
        masm.newGCThing(objReg, tempReg, templateObj, ool->entry(), gc::DefaultHeap);
        masm.initGCThing(objReg, tempReg, templateObj);
    }

    if (lir->slots()->isRegister())
        masm.storePtr(ToRegister(lir->slots()), Address(objReg, JSObject::offsetOfSlots()));

    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*NewSingletonCallObjectFn)(JSContext *, HandleShape, HeapSlot *);
static const VMFunction NewSingletonCallObjectInfo =
    FunctionInfo<NewSingletonCallObjectFn>(NewSingletonCallObject);

bool
CodeGenerator::visitNewSingletonCallObject(LNewSingletonCallObject *lir)
{
    Register objReg = ToRegister(lir->output());

    JSObject *templateObj = lir->mir()->templateObject();

    OutOfLineCode *ool;
    if (lir->slots()->isRegister()) {
        ool = oolCallVM(NewSingletonCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(templateObj->lastProperty()),
                                    ToRegister(lir->slots())),
                        StoreRegisterTo(objReg));
    } else {
        ool = oolCallVM(NewSingletonCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(templateObj->lastProperty()),
                                    ImmPtr(nullptr)),
                        StoreRegisterTo(objReg));
    }
    if (!ool)
        return false;

    // Objects can only be given singleton types in VM calls.  We make the call
    // out of line to not bloat inline code, even if (naively) this seems like
    // extra work.
    masm.jump(ool->entry());
    masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitNewCallObjectPar(LNewCallObjectPar *lir)
{
    Register resultReg = ToRegister(lir->output());
    Register cxReg = ToRegister(lir->forkJoinContext());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    JSObject *templateObj = lir->mir()->templateObj();

    emitAllocateGCThingPar(lir, resultReg, cxReg, tempReg1, tempReg2, templateObj);

    // NB: !lir->slots()->isRegister() implies that there is no slots
    // array at all, and the memory is already zeroed when copying
    // from the template object

    if (lir->slots()->isRegister()) {
        Register slotsReg = ToRegister(lir->slots());
        JS_ASSERT(slotsReg != resultReg);
        masm.storePtr(slotsReg, Address(resultReg, JSObject::offsetOfSlots()));
    }

    return true;
}

bool
CodeGenerator::visitNewDenseArrayPar(LNewDenseArrayPar *lir)
{
    Register cxReg = ToRegister(lir->forkJoinContext());
    Register lengthReg = ToRegister(lir->length());
    Register tempReg0 = ToRegister(lir->getTemp0());
    Register tempReg1 = ToRegister(lir->getTemp1());
    Register tempReg2 = ToRegister(lir->getTemp2());
    JSObject *templateObj = lir->mir()->templateObject();

    // Allocate the array into tempReg2.  Don't use resultReg because it
    // may alias cxReg etc.
    emitAllocateGCThingPar(lir, tempReg2, cxReg, tempReg0, tempReg1, templateObj);

    // Invoke a C helper to allocate the elements.  For convenience,
    // this helper also returns the array back to us, or nullptr, which
    // obviates the need to preserve the register across the call.  In
    // reality, we should probably just have the C helper also
    // *allocate* the array, but that would require that it initialize
    // the various fields of the object, and I didn't want to
    // duplicate the code in initGCThing() that already does such an
    // admirable job.
    masm.setupUnalignedABICall(3, tempReg0);
    masm.passABIArg(cxReg);
    masm.passABIArg(tempReg2);
    masm.passABIArg(lengthReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ExtendArrayPar));

    Register resultReg = ToRegister(lir->output());
    JS_ASSERT(resultReg == ReturnReg);
    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutOutOfMemory, lir);
    if (!bail)
        return false;
    masm.branchTestPtr(Assembler::Zero, resultReg, resultReg, bail->entry());

    return true;
}

typedef JSObject *(*NewStringObjectFn)(JSContext *, HandleString);
static const VMFunction NewStringObjectInfo = FunctionInfo<NewStringObjectFn>(NewStringObject);

bool
CodeGenerator::visitNewStringObject(LNewStringObject *lir)
{
    Register input = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    Register temp = ToRegister(lir->temp());

    StringObject *templateObj = lir->mir()->templateObj();

    OutOfLineCode *ool = oolCallVM(NewStringObjectInfo, lir, (ArgList(), input),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    masm.newGCThing(output, temp, templateObj, ool->entry(), gc::DefaultHeap);
    masm.initGCThing(output, temp, templateObj);

    masm.loadStringLength(input, temp);

    masm.storeValue(JSVAL_TYPE_STRING, input, Address(output, StringObject::offsetOfPrimitiveValue()));
    masm.storeValue(JSVAL_TYPE_INT32, temp, Address(output, StringObject::offsetOfLength()));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitNewPar(LNewPar *lir)
{
    Register objReg = ToRegister(lir->output());
    Register cxReg = ToRegister(lir->forkJoinContext());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    JSObject *templateObject = lir->mir()->templateObject();
    emitAllocateGCThingPar(lir, objReg, cxReg, tempReg1, tempReg2, templateObject);
    return true;
}

class OutOfLineNewGCThingPar : public OutOfLineCodeBase<CodeGenerator>
{
public:
    LInstruction *lir;
    gc::AllocKind allocKind;
    Register objReg;
    Register cxReg;

    OutOfLineNewGCThingPar(LInstruction *lir, gc::AllocKind allocKind, Register objReg,
                           Register cxReg)
      : lir(lir), allocKind(allocKind), objReg(objReg), cxReg(cxReg)
    {}

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewGCThingPar(this);
    }
};

bool
CodeGenerator::emitAllocateGCThingPar(LInstruction *lir, Register objReg, Register cxReg,
                                      Register tempReg1, Register tempReg2, JSObject *templateObj)
{
    gc::AllocKind allocKind = templateObj->tenuredGetAllocKind();
    OutOfLineNewGCThingPar *ool = new(alloc()) OutOfLineNewGCThingPar(lir, allocKind, objReg, cxReg);
    if (!ool || !addOutOfLineCode(ool))
        return false;

    masm.newGCThingPar(objReg, cxReg, tempReg1, tempReg2, templateObj, ool->entry());
    masm.bind(ool->rejoin());
    masm.initGCThing(objReg, tempReg1, templateObj);
    return true;
}

bool
CodeGenerator::visitOutOfLineNewGCThingPar(OutOfLineNewGCThingPar *ool)
{
    // As a fallback for allocation in par. exec. mode, we invoke the
    // C helper NewGCThingPar(), which calls into the GC code.  If it
    // returns nullptr, we bail.  If returns non-nullptr, we rejoin the
    // original instruction.
    Register out = ool->objReg;

    saveVolatile(out);
    masm.setupUnalignedABICall(2, out);
    masm.passABIArg(ool->cxReg);
    masm.move32(Imm32(ool->allocKind), out);
    masm.passABIArg(out);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NewGCThingPar));
    masm.storeCallResult(out);
    restoreVolatile(out);

    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutOutOfMemory, ool->lir);
    if (!bail)
        return false;
    masm.branchTestPtr(Assembler::Zero, out, out, bail->entry());
    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitAbortPar(LAbortPar *lir)
{
    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutUnsupported, lir);
    if (!bail)
        return false;
    masm.jump(bail->entry());
    return true;
}

typedef bool(*InitElemFn)(JSContext *cx, HandleObject obj,
                          HandleValue id, HandleValue value);
static const VMFunction InitElemInfo =
    FunctionInfo<InitElemFn>(InitElemOperation);

bool
CodeGenerator::visitInitElem(LInitElem *lir)
{
    Register objReg = ToRegister(lir->getObject());

    pushArg(ToValue(lir, LInitElem::ValueIndex));
    pushArg(ToValue(lir, LInitElem::IdIndex));
    pushArg(objReg);

    return callVM(InitElemInfo, lir);
}

typedef bool (*InitElemGetterSetterFn)(JSContext *, jsbytecode *, HandleObject, HandleValue,
                                       HandleObject);
static const VMFunction InitElemGetterSetterInfo =
    FunctionInfo<InitElemGetterSetterFn>(InitGetterSetterOperation);

bool
CodeGenerator::visitInitElemGetterSetter(LInitElemGetterSetter *lir)
{
    Register obj = ToRegister(lir->object());
    Register value = ToRegister(lir->value());

    pushArg(value);
    pushArg(ToValue(lir, LInitElemGetterSetter::IdIndex));
    pushArg(obj);
    pushArg(ImmPtr(lir->mir()->resumePoint()->pc()));

    return callVM(InitElemGetterSetterInfo, lir);
}

typedef bool(*MutatePrototypeFn)(JSContext *cx, HandleObject obj, HandleValue value);
static const VMFunction MutatePrototypeInfo =
    FunctionInfo<MutatePrototypeFn>(MutatePrototype);

bool
CodeGenerator::visitMutateProto(LMutateProto *lir)
{
    Register objReg = ToRegister(lir->getObject());

    pushArg(ToValue(lir, LMutateProto::ValueIndex));
    pushArg(objReg);

    return callVM(MutatePrototypeInfo, lir);
}

typedef bool(*InitPropFn)(JSContext *cx, HandleObject obj,
                          HandlePropertyName name, HandleValue value);
static const VMFunction InitPropInfo =
    FunctionInfo<InitPropFn>(InitProp);

bool
CodeGenerator::visitInitProp(LInitProp *lir)
{
    Register objReg = ToRegister(lir->getObject());

    pushArg(ToValue(lir, LInitProp::ValueIndex));
    pushArg(ImmGCPtr(lir->mir()->propertyName()));
    pushArg(objReg);

    return callVM(InitPropInfo, lir);
}

typedef bool(*InitPropGetterSetterFn)(JSContext *, jsbytecode *, HandleObject, HandlePropertyName,
                                      HandleObject);
static const VMFunction InitPropGetterSetterInfo =
    FunctionInfo<InitPropGetterSetterFn>(InitGetterSetterOperation);

bool
CodeGenerator::visitInitPropGetterSetter(LInitPropGetterSetter *lir)
{
    Register obj = ToRegister(lir->object());
    Register value = ToRegister(lir->value());

    pushArg(value);
    pushArg(ImmGCPtr(lir->mir()->name()));
    pushArg(obj);
    pushArg(ImmPtr(lir->mir()->resumePoint()->pc()));

    return callVM(InitPropGetterSetterInfo, lir);
}

typedef bool (*CreateThisFn)(JSContext *cx, HandleObject callee, MutableHandleValue rval);
static const VMFunction CreateThisInfoCodeGen = FunctionInfo<CreateThisFn>(CreateThis);

bool
CodeGenerator::visitCreateThis(LCreateThis *lir)
{
    const LAllocation *callee = lir->getCallee();

    if (callee->isConstant())
        pushArg(ImmGCPtr(&callee->toConstant()->toObject()));
    else
        pushArg(ToRegister(callee));

    return callVM(CreateThisInfoCodeGen, lir);
}

static JSObject *
CreateThisForFunctionWithProtoWrapper(JSContext *cx, js::HandleObject callee, JSObject *proto)
{
    return CreateThisForFunctionWithProto(cx, callee, proto);
}

typedef JSObject *(*CreateThisWithProtoFn)(JSContext *cx, HandleObject callee, JSObject *proto);
static const VMFunction CreateThisWithProtoInfo =
FunctionInfo<CreateThisWithProtoFn>(CreateThisForFunctionWithProtoWrapper);

bool
CodeGenerator::visitCreateThisWithProto(LCreateThisWithProto *lir)
{
    const LAllocation *callee = lir->getCallee();
    const LAllocation *proto = lir->getPrototype();

    if (proto->isConstant())
        pushArg(ImmGCPtr(&proto->toConstant()->toObject()));
    else
        pushArg(ToRegister(proto));

    if (callee->isConstant())
        pushArg(ImmGCPtr(&callee->toConstant()->toObject()));
    else
        pushArg(ToRegister(callee));

    return callVM(CreateThisWithProtoInfo, lir);
}

typedef JSObject *(*NewGCObjectFn)(JSContext *cx, gc::AllocKind allocKind,
                                   gc::InitialHeap initialHeap);
static const VMFunction NewGCObjectInfo =
    FunctionInfo<NewGCObjectFn>(js::jit::NewGCObject);

bool
CodeGenerator::visitCreateThisWithTemplate(LCreateThisWithTemplate *lir)
{
    JSObject *templateObject = lir->mir()->templateObject();
    gc::AllocKind allocKind = templateObject->tenuredGetAllocKind();
    gc::InitialHeap initialHeap = lir->mir()->initialHeap();
    Register objReg = ToRegister(lir->output());
    Register tempReg = ToRegister(lir->temp());

    OutOfLineCode *ool = oolCallVM(NewGCObjectInfo, lir,
                                   (ArgList(), Imm32(allocKind), Imm32(initialHeap)),
                                   StoreRegisterTo(objReg));
    if (!ool)
        return false;

    // Allocate. If the FreeList is empty, call to VM, which may GC.
    masm.newGCThing(objReg, tempReg, templateObject, ool->entry(), lir->mir()->initialHeap());

    // Initialize based on the templateObject.
    masm.bind(ool->rejoin());
    masm.initGCThing(objReg, tempReg, templateObject);

    return true;
}

typedef JSObject *(*NewIonArgumentsObjectFn)(JSContext *cx, IonJSFrameLayout *frame, HandleObject);
static const VMFunction NewIonArgumentsObjectInfo =
    FunctionInfo<NewIonArgumentsObjectFn>((NewIonArgumentsObjectFn) ArgumentsObject::createForIon);

bool
CodeGenerator::visitCreateArgumentsObject(LCreateArgumentsObject *lir)
{
    // This should be getting constructed in the first block only, and not any OSR entry blocks.
    JS_ASSERT(lir->mir()->block()->id() == 0);

    const LAllocation *callObj = lir->getCallObject();
    Register temp = ToRegister(lir->getTemp(0));

    masm.movePtr(StackPointer, temp);
    masm.addPtr(Imm32(frameSize()), temp);

    pushArg(ToRegister(callObj));
    pushArg(temp);
    return callVM(NewIonArgumentsObjectInfo, lir);
}

bool
CodeGenerator::visitGetArgumentsObjectArg(LGetArgumentsObjectArg *lir)
{
    Register temp = ToRegister(lir->getTemp(0));
    Register argsObj = ToRegister(lir->getArgsObject());
    ValueOperand out = ToOutValue(lir);

    masm.loadPrivate(Address(argsObj, ArgumentsObject::getDataSlotOffset()), temp);
    Address argAddr(temp, ArgumentsData::offsetOfArgs() + lir->mir()->argno() * sizeof(Value));
    masm.loadValue(argAddr, out);
#ifdef DEBUG
    Label success;
    masm.branchTestMagic(Assembler::NotEqual, out, &success);
    masm.assumeUnreachable("Result from ArgumentObject shouldn't be MIRType_Magic.");
    masm.bind(&success);
#endif
    return true;
}

bool
CodeGenerator::visitSetArgumentsObjectArg(LSetArgumentsObjectArg *lir)
{
    Register temp = ToRegister(lir->getTemp(0));
    Register argsObj = ToRegister(lir->getArgsObject());
    ValueOperand value = ToValue(lir, LSetArgumentsObjectArg::ValueIndex);

    masm.loadPrivate(Address(argsObj, ArgumentsObject::getDataSlotOffset()), temp);
    Address argAddr(temp, ArgumentsData::offsetOfArgs() + lir->mir()->argno() * sizeof(Value));
    emitPreBarrier(argAddr, MIRType_Value);
#ifdef DEBUG
    Label success;
    masm.branchTestMagic(Assembler::NotEqual, argAddr, &success);
    masm.assumeUnreachable("Result in ArgumentObject shouldn't be MIRType_Magic.");
    masm.bind(&success);
#endif
    masm.storeValue(value, argAddr);
    return true;
}

bool
CodeGenerator::visitReturnFromCtor(LReturnFromCtor *lir)
{
    ValueOperand value = ToValue(lir, LReturnFromCtor::ValueIndex);
    Register obj = ToRegister(lir->getObject());
    Register output = ToRegister(lir->output());

    Label valueIsObject, end;

    masm.branchTestObject(Assembler::Equal, value, &valueIsObject);

    // Value is not an object. Return that other object.
    masm.movePtr(obj, output);
    masm.jump(&end);

    // Value is an object. Return unbox(Value).
    masm.bind(&valueIsObject);
    Register payload = masm.extractObject(value, output);
    if (payload != output)
        masm.movePtr(payload, output);

    masm.bind(&end);
    return true;
}

typedef JSObject *(*BoxNonStrictThisFn)(JSContext *, HandleValue);
static const VMFunction BoxNonStrictThisInfo = FunctionInfo<BoxNonStrictThisFn>(BoxNonStrictThis);

bool
CodeGenerator::visitComputeThis(LComputeThis *lir)
{
    ValueOperand value = ToValue(lir, LComputeThis::ValueIndex);
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(BoxNonStrictThisInfo, lir, (ArgList(), value),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    masm.branchTestObject(Assembler::NotEqual, value, ool->entry());
    masm.unboxObject(value, output);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitLoadArrowThis(LLoadArrowThis *lir)
{
    Register callee = ToRegister(lir->callee());
    ValueOperand output = ToOutValue(lir);
    masm.loadValue(Address(callee, FunctionExtended::offsetOfArrowThisSlot()), output);
    return true;
}

bool
CodeGenerator::visitArrayLength(LArrayLength *lir)
{
    Address length(ToRegister(lir->elements()), ObjectElements::offsetOfLength());
    masm.load32(length, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitSetArrayLength(LSetArrayLength *lir)
{
    Address length(ToRegister(lir->elements()), ObjectElements::offsetOfLength());
    Int32Key newLength = ToInt32Key(lir->index());

    masm.bumpKey(&newLength, 1);
    masm.storeKey(newLength, length);
    // Restore register value if it is used/captured after.
    masm.bumpKey(&newLength, -1);
    return true;
}

bool
CodeGenerator::visitTypedArrayLength(LTypedArrayLength *lir)
{
    Register obj = ToRegister(lir->object());
    Register out = ToRegister(lir->output());
    masm.unboxInt32(Address(obj, TypedArrayObject::lengthOffset()), out);
    return true;
}

bool
CodeGenerator::visitTypedArrayElements(LTypedArrayElements *lir)
{
    Register obj = ToRegister(lir->object());
    Register out = ToRegister(lir->output());
    masm.loadPtr(Address(obj, TypedArrayObject::dataOffset()), out);
    return true;
}

bool
CodeGenerator::visitNeuterCheck(LNeuterCheck *lir)
{
    Register obj = ToRegister(lir->object());
    Register temp = ToRegister(lir->temp());

    masm.extractObject(Address(obj, TypedObject::offsetOfOwnerSlot()), temp);
    masm.unboxInt32(Address(temp, ArrayBufferObject::flagsOffset()), temp);

    Imm32 flag(ArrayBufferObject::neuteredFlag());
    if (!bailoutTest32(Assembler::NonZero, temp, flag, lir->snapshot()))
        return false;

    return true;
}

bool
CodeGenerator::visitTypedObjectElements(LTypedObjectElements *lir)
{
    Register obj = ToRegister(lir->object());
    Register out = ToRegister(lir->output());
    masm.loadPtr(Address(obj, TypedObject::offsetOfDataSlot()), out);
    return true;
}

bool
CodeGenerator::visitSetTypedObjectOffset(LSetTypedObjectOffset *lir)
{
    Register object = ToRegister(lir->object());
    Register offset = ToRegister(lir->offset());
    Register temp0 = ToRegister(lir->temp0());

    // `offset` is an absolute offset into the base buffer. One way
    // to implement this instruction would be load the base address
    // from the buffer and add `offset`. But that'd be an extra load.
    // We can instead load the current base pointer and current
    // offset, compute the difference with `offset`, and then adjust
    // the current base pointer. This is two loads but to adjacent
    // fields in the same object, which should come in the same cache
    // line.
    //
    // The C code I would probably write is the following:
    //
    // void SetTypedObjectOffset(TypedObject *obj, int32_t offset) {
    //     int32_t temp0 = obj->byteOffset;
    //     obj->pointer = obj->pointer - temp0 + offset;
    //     obj->byteOffset = offset;
    // }
    //
    // But what we actually compute is more like this, because it
    // saves us a temporary to do it this way:
    //
    // void SetTypedObjectOffset(TypedObject *obj, int32_t offset) {
    //     int32_t temp0 = obj->byteOffset;
    //     obj->pointer = obj->pointer - (temp0 - offset);
    //     obj->byteOffset = offset;
    // }

    // temp0 = typedObj->byteOffset;
    masm.unboxInt32(Address(object, TypedObject::offsetOfByteOffsetSlot()), temp0);

    // temp0 -= offset;
    masm.subPtr(offset, temp0);

    // obj->pointer -= temp0;
    masm.subPtr(temp0, Address(object, TypedObject::offsetOfDataSlot()));

    // obj->byteOffset = offset;
    masm.storeValue(JSVAL_TYPE_INT32, offset,
                    Address(object, TypedObject::offsetOfByteOffsetSlot()));

    return true;
}

bool
CodeGenerator::visitStringLength(LStringLength *lir)
{
    Register input = ToRegister(lir->string());
    Register output = ToRegister(lir->output());

    masm.loadStringLength(input, output);
    return true;
}

bool
CodeGenerator::visitMinMaxI(LMinMaxI *ins)
{
    Register first = ToRegister(ins->first());
    Register output = ToRegister(ins->output());

    JS_ASSERT(first == output);

    Label done;
    Assembler::Condition cond = ins->mir()->isMax()
                                ? Assembler::GreaterThan
                                : Assembler::LessThan;

    if (ins->second()->isConstant()) {
        masm.branch32(cond, first, Imm32(ToInt32(ins->second())), &done);
        masm.move32(Imm32(ToInt32(ins->second())), output);
    } else {
        masm.branch32(cond, first, ToRegister(ins->second()), &done);
        masm.move32(ToRegister(ins->second()), output);
    }

    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitAbsI(LAbsI *ins)
{
    Register input = ToRegister(ins->input());
    Label positive;

    JS_ASSERT(input == ToRegister(ins->output()));
    masm.branchTest32(Assembler::NotSigned, input, input, &positive);
    masm.neg32(input);
#ifdef JS_CODEGEN_MIPS
    LSnapshot *snapshot = ins->snapshot();
    if (snapshot && !bailoutCmp32(Assembler::Equal, input, Imm32(INT32_MIN), snapshot))
        return false;
#else
    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
#endif
    masm.bind(&positive);

    return true;
}

bool
CodeGenerator::visitPowI(LPowI *ins)
{
    FloatRegister value = ToFloatRegister(ins->value());
    Register power = ToRegister(ins->power());
    Register temp = ToRegister(ins->temp());

    JS_ASSERT(power != temp);

    // In all implementations, setupUnalignedABICall() relinquishes use of
    // its scratch register. We can therefore save an input register by
    // reusing the scratch register to pass constants to callWithABI.
    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(value, MoveOp::DOUBLE);
    masm.passABIArg(power);

    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::powi), MoveOp::DOUBLE);
    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);

    return true;
}

bool
CodeGenerator::visitPowD(LPowD *ins)
{
    FloatRegister value = ToFloatRegister(ins->value());
    FloatRegister power = ToFloatRegister(ins->power());
    Register temp = ToRegister(ins->temp());

    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(value, MoveOp::DOUBLE);
    masm.passABIArg(power, MoveOp::DOUBLE);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ecmaPow), MoveOp::DOUBLE);

    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);
    return true;
}

bool
CodeGenerator::visitRandom(LRandom *ins)
{
    Register temp = ToRegister(ins->temp());
    Register temp2 = ToRegister(ins->temp2());

    masm.loadJSContext(temp);

    masm.setupUnalignedABICall(1, temp2);
    masm.passABIArg(temp);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, math_random_no_outparam), MoveOp::DOUBLE);

    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);
    return true;
}

bool
CodeGenerator::visitMathFunctionD(LMathFunctionD *ins)
{
    Register temp = ToRegister(ins->temp());
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);

    const MathCache *mathCache = ins->mir()->cache();

    masm.setupUnalignedABICall(mathCache ? 2 : 1, temp);
    if (mathCache) {
        masm.movePtr(ImmPtr(mathCache), temp);
        masm.passABIArg(temp);
    }
    masm.passABIArg(input, MoveOp::DOUBLE);

#   define MAYBE_CACHED(fcn) (mathCache ? (void*)fcn ## _impl : (void*)fcn ## _uncached)

    void *funptr = nullptr;
    switch (ins->mir()->function()) {
      case MMathFunction::Log:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_log));
        break;
      case MMathFunction::Sin:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_sin));
        break;
      case MMathFunction::Cos:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_cos));
        break;
      case MMathFunction::Exp:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_exp));
        break;
      case MMathFunction::Tan:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_tan));
        break;
      case MMathFunction::ATan:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_atan));
        break;
      case MMathFunction::ASin:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_asin));
        break;
      case MMathFunction::ACos:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_acos));
        break;
      case MMathFunction::Log10:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_log10));
        break;
      case MMathFunction::Log2:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_log2));
        break;
      case MMathFunction::Log1P:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_log1p));
        break;
      case MMathFunction::ExpM1:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_expm1));
        break;
      case MMathFunction::CosH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_cosh));
        break;
      case MMathFunction::SinH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_sinh));
        break;
      case MMathFunction::TanH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_tanh));
        break;
      case MMathFunction::ACosH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_acosh));
        break;
      case MMathFunction::ASinH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_asinh));
        break;
      case MMathFunction::ATanH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_atanh));
        break;
      case MMathFunction::Sign:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_sign));
        break;
      case MMathFunction::Trunc:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_trunc));
        break;
      case MMathFunction::Cbrt:
        funptr = JS_FUNC_TO_DATA_PTR(void *, MAYBE_CACHED(js::math_cbrt));
        break;
      case MMathFunction::Floor:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_floor_impl);
        break;
      case MMathFunction::Ceil:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_ceil_impl);
        break;
      case MMathFunction::Round:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_round_impl);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown math function");
    }

#   undef MAYBE_CACHED

    masm.callWithABI(funptr, MoveOp::DOUBLE);
    return true;
}

bool
CodeGenerator::visitMathFunctionF(LMathFunctionF *ins)
{
    Register temp = ToRegister(ins->temp());
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);

    masm.setupUnalignedABICall(1, temp);
    masm.passABIArg(input, MoveOp::FLOAT32);

    void *funptr = nullptr;
    switch (ins->mir()->function()) {
      case MMathFunction::Floor: funptr = JS_FUNC_TO_DATA_PTR(void *, floorf);           break;
      case MMathFunction::Round: funptr = JS_FUNC_TO_DATA_PTR(void *, math_roundf_impl); break;
      case MMathFunction::Ceil:  funptr = JS_FUNC_TO_DATA_PTR(void *, ceilf);            break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown or unsupported float32 math function");
    }

    masm.callWithABI(funptr, MoveOp::FLOAT32);
    return true;
}

bool
CodeGenerator::visitModD(LModD *ins)
{
    FloatRegister lhs = ToFloatRegister(ins->lhs());
    FloatRegister rhs = ToFloatRegister(ins->rhs());
    Register temp = ToRegister(ins->temp());

    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);

    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(lhs, MoveOp::DOUBLE);
    masm.passABIArg(rhs, MoveOp::DOUBLE);

    if (gen->compilingAsmJS())
        masm.callWithABI(AsmJSImm_ModD, MoveOp::DOUBLE);
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NumberMod), MoveOp::DOUBLE);
    return true;
}

typedef bool (*BinaryFn)(JSContext *, MutableHandleValue, MutableHandleValue, MutableHandleValue);
typedef bool (*BinaryParFn)(ForkJoinContext *, HandleValue, HandleValue, MutableHandleValue);

static const VMFunction AddInfo = FunctionInfo<BinaryFn>(js::AddValues);
static const VMFunction SubInfo = FunctionInfo<BinaryFn>(js::SubValues);
static const VMFunction MulInfo = FunctionInfo<BinaryFn>(js::MulValues);
static const VMFunction DivInfo = FunctionInfo<BinaryFn>(js::DivValues);
static const VMFunction ModInfo = FunctionInfo<BinaryFn>(js::ModValues);
static const VMFunctionsModal UrshInfo = VMFunctionsModal(
    FunctionInfo<BinaryFn>(js::UrshValues),
    FunctionInfo<BinaryParFn>(UrshValuesPar));

bool
CodeGenerator::visitBinaryV(LBinaryV *lir)
{
    pushArg(ToValue(lir, LBinaryV::RhsInput));
    pushArg(ToValue(lir, LBinaryV::LhsInput));

    switch (lir->jsop()) {
      case JSOP_ADD:
        return callVM(AddInfo, lir);

      case JSOP_SUB:
        return callVM(SubInfo, lir);

      case JSOP_MUL:
        return callVM(MulInfo, lir);

      case JSOP_DIV:
        return callVM(DivInfo, lir);

      case JSOP_MOD:
        return callVM(ModInfo, lir);

      case JSOP_URSH:
        return callVM(UrshInfo, lir);

      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected binary op");
    }
}

typedef bool (*StringCompareFn)(JSContext *, HandleString, HandleString, bool *);
typedef bool (*StringCompareParFn)(ForkJoinContext *, HandleString, HandleString, bool *);
static const VMFunctionsModal StringsEqualInfo = VMFunctionsModal(
    FunctionInfo<StringCompareFn>(jit::StringsEqual<true>),
    FunctionInfo<StringCompareParFn>(jit::StringsEqualPar));
static const VMFunctionsModal StringsNotEqualInfo = VMFunctionsModal(
    FunctionInfo<StringCompareFn>(jit::StringsEqual<false>),
    FunctionInfo<StringCompareParFn>(jit::StringsUnequalPar));

bool
CodeGenerator::emitCompareS(LInstruction *lir, JSOp op, Register left, Register right,
                            Register output, Register temp)
{
    JS_ASSERT(lir->isCompareS() || lir->isCompareStrictS());

    OutOfLineCode *ool = nullptr;

    if (op == JSOP_EQ || op == JSOP_STRICTEQ) {
        ool = oolCallVM(StringsEqualInfo, lir, (ArgList(), left, right),  StoreRegisterTo(output));
    } else {
        JS_ASSERT(op == JSOP_NE || op == JSOP_STRICTNE);
        ool = oolCallVM(StringsNotEqualInfo, lir, (ArgList(), left, right), StoreRegisterTo(output));
    }
    if (!ool)
        return false;

    masm.compareStrings(op, left, right, output, temp, ool->entry());

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitCompareStrictS(LCompareStrictS *lir)
{
    JSOp op = lir->mir()->jsop();
    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    const ValueOperand leftV = ToValue(lir, LCompareStrictS::Lhs);
    Register right = ToRegister(lir->right());
    Register output = ToRegister(lir->output());
    Register temp = ToRegister(lir->temp());
    Register tempToUnbox = ToTempUnboxRegister(lir->tempToUnbox());

    Label string, done;

    masm.branchTestString(Assembler::Equal, leftV, &string);
    masm.move32(Imm32(op == JSOP_STRICTNE), output);
    masm.jump(&done);

    masm.bind(&string);
    Register left = masm.extractString(leftV, tempToUnbox);
    if (!emitCompareS(lir, op, left, right, output, temp))
        return false;

    masm.bind(&done);

    return true;
}

bool
CodeGenerator::visitCompareS(LCompareS *lir)
{
    JSOp op = lir->mir()->jsop();
    Register left = ToRegister(lir->left());
    Register right = ToRegister(lir->right());
    Register output = ToRegister(lir->output());
    Register temp = ToRegister(lir->temp());

    return emitCompareS(lir, op, left, right, output, temp);
}

typedef bool (*CompareFn)(JSContext *, MutableHandleValue, MutableHandleValue, bool *);
typedef bool (*CompareParFn)(ForkJoinContext *, MutableHandleValue, MutableHandleValue, bool *);
static const VMFunctionsModal EqInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::LooselyEqual<true>),
    FunctionInfo<CompareParFn>(jit::LooselyEqualPar));
static const VMFunctionsModal NeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::LooselyEqual<false>),
    FunctionInfo<CompareParFn>(jit::LooselyUnequalPar));
static const VMFunctionsModal StrictEqInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::StrictlyEqual<true>),
    FunctionInfo<CompareParFn>(jit::StrictlyEqualPar));
static const VMFunctionsModal StrictNeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::StrictlyEqual<false>),
    FunctionInfo<CompareParFn>(jit::StrictlyUnequalPar));
static const VMFunctionsModal LtInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::LessThan),
    FunctionInfo<CompareParFn>(jit::LessThanPar));
static const VMFunctionsModal LeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::LessThanOrEqual),
    FunctionInfo<CompareParFn>(jit::LessThanOrEqualPar));
static const VMFunctionsModal GtInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::GreaterThan),
    FunctionInfo<CompareParFn>(jit::GreaterThanPar));
static const VMFunctionsModal GeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(jit::GreaterThanOrEqual),
    FunctionInfo<CompareParFn>(jit::GreaterThanOrEqualPar));

bool
CodeGenerator::visitCompareVM(LCompareVM *lir)
{
    pushArg(ToValue(lir, LBinaryV::RhsInput));
    pushArg(ToValue(lir, LBinaryV::LhsInput));

    switch (lir->mir()->jsop()) {
      case JSOP_EQ:
        return callVM(EqInfo, lir);

      case JSOP_NE:
        return callVM(NeInfo, lir);

      case JSOP_STRICTEQ:
        return callVM(StrictEqInfo, lir);

      case JSOP_STRICTNE:
        return callVM(StrictNeInfo, lir);

      case JSOP_LT:
        return callVM(LtInfo, lir);

      case JSOP_LE:
        return callVM(LeInfo, lir);

      case JSOP_GT:
        return callVM(GtInfo, lir);

      case JSOP_GE:
        return callVM(GeInfo, lir);

      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected compare op");
    }
}

bool
CodeGenerator::visitIsNullOrLikeUndefined(LIsNullOrLikeUndefined *lir)
{
    JSOp op = lir->mir()->jsop();
    MCompare::CompareType compareType = lir->mir()->compareType();
    JS_ASSERT(compareType == MCompare::Compare_Undefined ||
              compareType == MCompare::Compare_Null);

    const ValueOperand value = ToValue(lir, LIsNullOrLikeUndefined::Value);
    Register output = ToRegister(lir->output());

    if (op == JSOP_EQ || op == JSOP_NE) {
        MOZ_ASSERT(lir->mir()->lhs()->type() != MIRType_Object ||
                   lir->mir()->operandMightEmulateUndefined(),
                   "Operands which can't emulate undefined should have been folded");

        OutOfLineTestObjectWithLabels *ool = nullptr;
        Maybe<Label> label1, label2;
        Label *nullOrLikeUndefined;
        Label *notNullOrLikeUndefined;
        if (lir->mir()->operandMightEmulateUndefined()) {
            ool = new(alloc()) OutOfLineTestObjectWithLabels();
            if (!addOutOfLineCode(ool))
                return false;
            nullOrLikeUndefined = ool->label1();
            notNullOrLikeUndefined = ool->label2();
        } else {
            label1.construct();
            label2.construct();
            nullOrLikeUndefined = label1.addr();
            notNullOrLikeUndefined = label2.addr();
        }

        Register tag = masm.splitTagForTest(value);

        masm.branchTestNull(Assembler::Equal, tag, nullOrLikeUndefined);
        masm.branchTestUndefined(Assembler::Equal, tag, nullOrLikeUndefined);

        if (ool) {
            // Check whether it's a truthy object or a falsy object that emulates
            // undefined.
            masm.branchTestObject(Assembler::NotEqual, tag, notNullOrLikeUndefined);

            Register objreg = masm.extractObject(value, ToTempUnboxRegister(lir->tempToUnbox()));
            branchTestObjectEmulatesUndefined(objreg, nullOrLikeUndefined, notNullOrLikeUndefined,
                                              ToRegister(lir->temp()), ool);
            // fall through
        }

        Label done;

        // It's not null or undefined, and if it's an object it doesn't
        // emulate undefined, so it's not like undefined.
        masm.move32(Imm32(op == JSOP_NE), output);
        masm.jump(&done);

        masm.bind(nullOrLikeUndefined);
        masm.move32(Imm32(op == JSOP_EQ), output);

        // Both branches meet here.
        masm.bind(&done);
        return true;
    }

    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    Assembler::Condition cond = JSOpToCondition(compareType, op);
    if (compareType == MCompare::Compare_Null)
        masm.testNullSet(cond, value, output);
    else
        masm.testUndefinedSet(cond, value, output);

    return true;
}

bool
CodeGenerator::visitIsNullOrLikeUndefinedAndBranch(LIsNullOrLikeUndefinedAndBranch *lir)
{
    JSOp op = lir->cmpMir()->jsop();
    MCompare::CompareType compareType = lir->cmpMir()->compareType();
    JS_ASSERT(compareType == MCompare::Compare_Undefined ||
              compareType == MCompare::Compare_Null);

    const ValueOperand value = ToValue(lir, LIsNullOrLikeUndefinedAndBranch::Value);

    if (op == JSOP_EQ || op == JSOP_NE) {
        MBasicBlock *ifTrue;
        MBasicBlock *ifFalse;

        if (op == JSOP_EQ) {
            ifTrue = lir->ifTrue();
            ifFalse = lir->ifFalse();
        } else {
            // Swap branches.
            ifTrue = lir->ifFalse();
            ifFalse = lir->ifTrue();
            op = JSOP_EQ;
        }

        MOZ_ASSERT(lir->cmpMir()->lhs()->type() != MIRType_Object ||
                   lir->cmpMir()->operandMightEmulateUndefined(),
                   "Operands which can't emulate undefined should have been folded");

        OutOfLineTestObject *ool = nullptr;
        if (lir->cmpMir()->operandMightEmulateUndefined()) {
            ool = new(alloc()) OutOfLineTestObject();
            if (!addOutOfLineCode(ool))
                return false;
        }

        Register tag = masm.splitTagForTest(value);

        Label *ifTrueLabel = getJumpLabelForBranch(ifTrue);
        Label *ifFalseLabel = getJumpLabelForBranch(ifFalse);

        masm.branchTestNull(Assembler::Equal, tag, ifTrueLabel);
        masm.branchTestUndefined(Assembler::Equal, tag, ifTrueLabel);

        if (ool) {
            masm.branchTestObject(Assembler::NotEqual, tag, ifFalseLabel);

            // Objects that emulate undefined are loosely equal to null/undefined.
            Register objreg = masm.extractObject(value, ToTempUnboxRegister(lir->tempToUnbox()));
            Register scratch = ToRegister(lir->temp());
            testObjectEmulatesUndefined(objreg, ifTrueLabel, ifFalseLabel, scratch, ool);
        } else {
            masm.jump(ifFalseLabel);
        }
        return true;
    }

    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    Assembler::Condition cond = JSOpToCondition(compareType, op);
    if (compareType == MCompare::Compare_Null)
        testNullEmitBranch(cond, value, lir->ifTrue(), lir->ifFalse());
    else
        testUndefinedEmitBranch(cond, value, lir->ifTrue(), lir->ifFalse());

    return true;
}

bool
CodeGenerator::visitEmulatesUndefined(LEmulatesUndefined *lir)
{
    MOZ_ASSERT(lir->mir()->compareType() == MCompare::Compare_Undefined ||
               lir->mir()->compareType() == MCompare::Compare_Null);
    MOZ_ASSERT(lir->mir()->lhs()->type() == MIRType_Object);
    MOZ_ASSERT(lir->mir()->operandMightEmulateUndefined(),
               "If the object couldn't emulate undefined, this should have been folded.");

    JSOp op = lir->mir()->jsop();
    MOZ_ASSERT(op == JSOP_EQ || op == JSOP_NE, "Strict equality should have been folded");

    OutOfLineTestObjectWithLabels *ool = new(alloc()) OutOfLineTestObjectWithLabels();
    if (!addOutOfLineCode(ool))
        return false;

    Label *emulatesUndefined = ool->label1();
    Label *doesntEmulateUndefined = ool->label2();

    Register objreg = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    branchTestObjectEmulatesUndefined(objreg, emulatesUndefined, doesntEmulateUndefined,
                                      output, ool);

    Label done;

    masm.move32(Imm32(op == JSOP_NE), output);
    masm.jump(&done);

    masm.bind(emulatesUndefined);
    masm.move32(Imm32(op == JSOP_EQ), output);
    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitEmulatesUndefinedAndBranch(LEmulatesUndefinedAndBranch *lir)
{
    MOZ_ASSERT(lir->cmpMir()->compareType() == MCompare::Compare_Undefined ||
               lir->cmpMir()->compareType() == MCompare::Compare_Null);
    MOZ_ASSERT(lir->cmpMir()->operandMightEmulateUndefined(),
               "Operands which can't emulate undefined should have been folded");

    JSOp op = lir->cmpMir()->jsop();
    MOZ_ASSERT(op == JSOP_EQ || op == JSOP_NE, "Strict equality should have been folded");

    OutOfLineTestObject *ool = new(alloc()) OutOfLineTestObject();
    if (!addOutOfLineCode(ool))
        return false;

    Label *equal;
    Label *unequal;

    {
        MBasicBlock *ifTrue;
        MBasicBlock *ifFalse;

        if (op == JSOP_EQ) {
            ifTrue = lir->ifTrue();
            ifFalse = lir->ifFalse();
        } else {
            // Swap branches.
            ifTrue = lir->ifFalse();
            ifFalse = lir->ifTrue();
            op = JSOP_EQ;
        }

        equal = getJumpLabelForBranch(ifTrue);
        unequal = getJumpLabelForBranch(ifFalse);
    }

    Register objreg = ToRegister(lir->input());

    testObjectEmulatesUndefined(objreg, equal, unequal, ToRegister(lir->temp()), ool);
    return true;
}

typedef JSString *(*ConcatStringsFn)(ThreadSafeContext *, HandleString, HandleString);
typedef JSString *(*ConcatStringsParFn)(ForkJoinContext *, HandleString, HandleString);
static const VMFunctionsModal ConcatStringsInfo = VMFunctionsModal(
    FunctionInfo<ConcatStringsFn>(ConcatStrings<CanGC>),
    FunctionInfo<ConcatStringsParFn>(ConcatStringsPar));

bool
CodeGenerator::emitConcat(LInstruction *lir, Register lhs, Register rhs, Register output)
{
    OutOfLineCode *ool = oolCallVM(ConcatStringsInfo, lir, (ArgList(), lhs, rhs),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    ExecutionMode mode = gen->info().executionMode();
    JitCode *stringConcatStub = gen->compartment->jitCompartment()->stringConcatStub(mode);
    masm.call(stringConcatStub);
    masm.branchTestPtr(Assembler::Zero, output, output, ool->entry());

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitConcat(LConcat *lir)
{
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());

    Register output = ToRegister(lir->output());

    JS_ASSERT(lhs == CallTempReg0);
    JS_ASSERT(rhs == CallTempReg1);
    JS_ASSERT(ToRegister(lir->temp1()) == CallTempReg0);
    JS_ASSERT(ToRegister(lir->temp2()) == CallTempReg1);
    JS_ASSERT(ToRegister(lir->temp3()) == CallTempReg2);
    JS_ASSERT(ToRegister(lir->temp4()) == CallTempReg3);
    JS_ASSERT(ToRegister(lir->temp5()) == CallTempReg4);
    JS_ASSERT(output == CallTempReg5);

    return emitConcat(lir, lhs, rhs, output);
}

bool
CodeGenerator::visitConcatPar(LConcatPar *lir)
{
    DebugOnly<Register> cx = ToRegister(lir->forkJoinContext());
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());
    Register output = ToRegister(lir->output());

    JS_ASSERT(lhs == CallTempReg0);
    JS_ASSERT(rhs == CallTempReg1);
    JS_ASSERT((Register)cx == CallTempReg4);
    JS_ASSERT(ToRegister(lir->temp1()) == CallTempReg0);
    JS_ASSERT(ToRegister(lir->temp2()) == CallTempReg1);
    JS_ASSERT(ToRegister(lir->temp3()) == CallTempReg2);
    JS_ASSERT(ToRegister(lir->temp4()) == CallTempReg3);
    JS_ASSERT(output == CallTempReg5);

    return emitConcat(lir, lhs, rhs, output);
}

static void
CopyStringChars(MacroAssembler &masm, Register to, Register from, Register len, Register scratch)
{
    // Copy |len| jschars from |from| to |to|. Assumes len > 0 (checked below in
    // debug builds), and when done |to| must point to the next available char.

#ifdef DEBUG
    Label ok;
    masm.branch32(Assembler::GreaterThan, len, Imm32(0), &ok);
    masm.assumeUnreachable("Length should be greater than 0.");
    masm.bind(&ok);
#endif

    JS_STATIC_ASSERT(sizeof(jschar) == 2);

    Label start;
    masm.bind(&start);
    masm.load16ZeroExtend(Address(from, 0), scratch);
    masm.store16(scratch, Address(to, 0));
    masm.addPtr(Imm32(2), from);
    masm.addPtr(Imm32(2), to);
    masm.branchSub32(Assembler::NonZero, Imm32(1), len, &start);
}

JitCode *
JitCompartment::generateStringConcatStub(JSContext *cx, ExecutionMode mode)
{
    MacroAssembler masm(cx);

    Register lhs = CallTempReg0;
    Register rhs = CallTempReg1;
    Register temp1 = CallTempReg2;
    Register temp2 = CallTempReg3;
    Register temp3 = CallTempReg4;
    Register output = CallTempReg5;

    // In parallel execution, we pass in the ForkJoinContext in CallTempReg4, as
    // by the time we need to use the temp3 we no longer have need of the
    // cx.
    Register forkJoinContext = CallTempReg4;

    Label failure, failurePopTemps;

    // If lhs is empty, return rhs.
    Label leftEmpty;
    masm.loadStringLength(lhs, temp1);
    masm.branchTest32(Assembler::Zero, temp1, temp1, &leftEmpty);

    // If rhs is empty, return lhs.
    Label rightEmpty;
    masm.loadStringLength(rhs, temp2);
    masm.branchTest32(Assembler::Zero, temp2, temp2, &rightEmpty);

    masm.add32(temp1, temp2);

    // Check if we can use a JSFatInlineString.
    Label isFatInline;
    masm.branch32(Assembler::BelowOrEqual, temp2, Imm32(JSFatInlineString::MAX_FAT_INLINE_LENGTH),
                  &isFatInline);

    // Ensure result length <= JSString::MAX_LENGTH.
    masm.branch32(Assembler::Above, temp2, Imm32(JSString::MAX_LENGTH), &failure);

    // Allocate a new rope.
    switch (mode) {
      case SequentialExecution:
        masm.newGCString(output, temp3, &failure);
        break;
      case ParallelExecution:
        masm.push(temp1);
        masm.push(temp2);
        masm.newGCStringPar(output, forkJoinContext, temp1, temp2, &failurePopTemps);
        masm.pop(temp2);
        masm.pop(temp1);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    // Store lengthAndFlags.
    JS_STATIC_ASSERT(JSString::ROPE_FLAGS == 0);
    masm.lshiftPtr(Imm32(JSString::LENGTH_SHIFT), temp2);
    masm.storePtr(temp2, Address(output, JSString::offsetOfLengthAndFlags()));

    // Store left and right nodes.
    masm.storePtr(lhs, Address(output, JSRope::offsetOfLeft()));
    masm.storePtr(rhs, Address(output, JSRope::offsetOfRight()));
    masm.ret();

    masm.bind(&leftEmpty);
    masm.mov(rhs, output);
    masm.ret();

    masm.bind(&rightEmpty);
    masm.mov(lhs, output);
    masm.ret();

    masm.bind(&isFatInline);

    // State: lhs length in temp1, result length in temp2.

    // Ensure both strings are linear (flags != 0).
    JS_STATIC_ASSERT(JSString::ROPE_FLAGS == 0);
    masm.branchTestPtr(Assembler::Zero, Address(lhs, JSString::offsetOfLengthAndFlags()),
                       Imm32(JSString::FLAGS_MASK), &failure);
    masm.branchTestPtr(Assembler::Zero, Address(rhs, JSString::offsetOfLengthAndFlags()),
                       Imm32(JSString::FLAGS_MASK), &failure);

    // Allocate a JSFatInlineString.
    switch (mode) {
      case SequentialExecution:
        masm.newGCFatInlineString(output, temp3, &failure);
        break;
      case ParallelExecution:
        masm.push(temp1);
        masm.push(temp2);
        masm.newGCFatInlineStringPar(output, forkJoinContext, temp1, temp2, &failurePopTemps);
        masm.pop(temp2);
        masm.pop(temp1);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    // Set lengthAndFlags.
    masm.lshiftPtr(Imm32(JSString::LENGTH_SHIFT), temp2);
    masm.orPtr(Imm32(JSString::FIXED_FLAGS), temp2);
    masm.storePtr(temp2, Address(output, JSString::offsetOfLengthAndFlags()));

    // Set chars pointer, keep in temp2 for copy loop below.
    masm.computeEffectiveAddress(Address(output, JSFatInlineString::offsetOfInlineStorage()), temp2);
    masm.storePtr(temp2, Address(output, JSFatInlineString::offsetOfChars()));

    {
        // We use temp3 in this block, which in parallel execution also holds
        // a live ForkJoinContext pointer. If we are compiling for parallel
        // execution, be sure to save and restore the ForkJoinContext.
        if (mode == ParallelExecution)
            masm.push(temp3);

        // Copy lhs chars. Temp1 still holds the lhs length. Note that this
        // advances temp2 to point to the next char. Note that this also
        // repurposes the lhs register.
        masm.loadPtr(Address(lhs, JSString::offsetOfChars()), lhs);
        CopyStringChars(masm, temp2, lhs, temp1, temp3);

        // Copy rhs chars.
        masm.loadStringLength(rhs, temp1);
        masm.loadPtr(Address(rhs, JSString::offsetOfChars()), rhs);
        CopyStringChars(masm, temp2, rhs, temp1, temp3);

        if (mode == ParallelExecution)
            masm.pop(temp3);
    }

    // Null-terminate.
    masm.store16(Imm32(0), Address(temp2, 0));
    masm.ret();

    masm.bind(&failurePopTemps);
    masm.pop(temp2);
    masm.pop(temp1);

    masm.bind(&failure);
    masm.movePtr(ImmPtr(nullptr), output);
    masm.ret();

    Linker linker(masm);
    JitCode *code = linker.newCode<CanGC>(cx, JSC::OTHER_CODE);

#ifdef JS_ION_PERF
    writePerfSpewerJitCodeProfile(code, "StringConcatStub");
#endif

    return code;
}

typedef bool (*CharCodeAtFn)(JSContext *, HandleString, int32_t, uint32_t *);
static const VMFunction CharCodeAtInfo = FunctionInfo<CharCodeAtFn>(jit::CharCodeAt);

bool
CodeGenerator::visitCharCodeAt(LCharCodeAt *lir)
{
    Register str = ToRegister(lir->str());
    Register index = ToRegister(lir->index());
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(CharCodeAtInfo, lir, (ArgList(), str, index), StoreRegisterTo(output));
    if (!ool)
        return false;

    Address lengthAndFlagsAddr(str, JSString::offsetOfLengthAndFlags());
    masm.branchTest32(Assembler::Zero, lengthAndFlagsAddr, Imm32(JSString::FLAGS_MASK), ool->entry());

    // getChars
    Address charsAddr(str, JSString::offsetOfChars());
    masm.loadPtr(charsAddr, output);
    masm.load16ZeroExtend(BaseIndex(output, index, TimesTwo, 0), output);

    masm.bind(ool->rejoin());
    return true;
}

typedef JSFlatString *(*StringFromCharCodeFn)(JSContext *, int32_t);
static const VMFunction StringFromCharCodeInfo = FunctionInfo<StringFromCharCodeFn>(jit::StringFromCharCode);

bool
CodeGenerator::visitFromCharCode(LFromCharCode *lir)
{
    Register code = ToRegister(lir->code());
    Register output = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(StringFromCharCodeInfo, lir, (ArgList(), code), StoreRegisterTo(output));
    if (!ool)
        return false;

    // OOL path if code >= UNIT_STATIC_LIMIT.
    masm.branch32(Assembler::AboveOrEqual, code, Imm32(StaticStrings::UNIT_STATIC_LIMIT),
                  ool->entry());

    masm.movePtr(ImmPtr(&GetIonContext()->runtime->staticStrings().unitStaticTable), output);
    masm.loadPtr(BaseIndex(output, code, ScalePointer), output);

    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*StringSplitFn)(JSContext *, HandleTypeObject, HandleString, HandleString);
static const VMFunction StringSplitInfo = FunctionInfo<StringSplitFn>(js::str_split_string);

bool
CodeGenerator::visitStringSplit(LStringSplit *lir)
{
    pushArg(ToRegister(lir->separator()));
    pushArg(ToRegister(lir->string()));
    pushArg(ImmGCPtr(lir->mir()->typeObject()));

    return callVM(StringSplitInfo, lir);
}

bool
CodeGenerator::visitInitializedLength(LInitializedLength *lir)
{
    Address initLength(ToRegister(lir->elements()), ObjectElements::offsetOfInitializedLength());
    masm.load32(initLength, ToRegister(lir->output()));
    return true;
}

bool
CodeGenerator::visitSetInitializedLength(LSetInitializedLength *lir)
{
    Address initLength(ToRegister(lir->elements()), ObjectElements::offsetOfInitializedLength());
    Int32Key index = ToInt32Key(lir->index());

    masm.bumpKey(&index, 1);
    masm.storeKey(index, initLength);
    // Restore register value if it is used/captured after.
    masm.bumpKey(&index, -1);
    return true;
}

bool
CodeGenerator::visitNotO(LNotO *lir)
{
    MOZ_ASSERT(lir->mir()->operandMightEmulateUndefined(),
               "This should be constant-folded if the object can't emulate undefined.");

    OutOfLineTestObjectWithLabels *ool = new(alloc()) OutOfLineTestObjectWithLabels();
    if (!addOutOfLineCode(ool))
        return false;

    Label *ifEmulatesUndefined = ool->label1();
    Label *ifDoesntEmulateUndefined = ool->label2();

    Register objreg = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    branchTestObjectEmulatesUndefined(objreg, ifEmulatesUndefined, ifDoesntEmulateUndefined,
                                      output, ool);
    // fall through

    Label join;

    masm.move32(Imm32(0), output);
    masm.jump(&join);

    masm.bind(ifEmulatesUndefined);
    masm.move32(Imm32(1), output);

    masm.bind(&join);
    return true;
}

bool
CodeGenerator::visitNotV(LNotV *lir)
{
    Maybe<Label> ifTruthyLabel, ifFalsyLabel;
    Label *ifTruthy;
    Label *ifFalsy;

    OutOfLineTestObjectWithLabels *ool = nullptr;
    if (lir->mir()->operandMightEmulateUndefined()) {
        ool = new(alloc()) OutOfLineTestObjectWithLabels();
        if (!addOutOfLineCode(ool))
            return false;
        ifTruthy = ool->label1();
        ifFalsy = ool->label2();
    } else {
        ifTruthyLabel.construct();
        ifFalsyLabel.construct();
        ifTruthy = ifTruthyLabel.addr();
        ifFalsy = ifFalsyLabel.addr();
    }

    testValueTruthyKernel(ToValue(lir, LNotV::Input), lir->temp1(), lir->temp2(),
                          ToFloatRegister(lir->tempFloat()),
                          ifTruthy, ifFalsy, ool);

    Label join;
    Register output = ToRegister(lir->output());

    // Note that the testValueTruthyKernel call above may choose to fall through
    // to ifTruthy instead of branching there.
    masm.bind(ifTruthy);
    masm.move32(Imm32(0), output);
    masm.jump(&join);

    masm.bind(ifFalsy);
    masm.move32(Imm32(1), output);

    // both branches meet here.
    masm.bind(&join);
    return true;
}

bool
CodeGenerator::visitBoundsCheck(LBoundsCheck *lir)
{
    if (lir->index()->isConstant()) {
        // Use uint32 so that the comparison is unsigned.
        uint32_t index = ToInt32(lir->index());
        if (lir->length()->isConstant()) {
            uint32_t length = ToInt32(lir->length());
            if (index < length)
                return true;
            return bailout(lir->snapshot());
        }
        return bailoutCmp32(Assembler::BelowOrEqual, ToOperand(lir->length()), Imm32(index),
                            lir->snapshot());
    }
    if (lir->length()->isConstant()) {
        return bailoutCmp32(Assembler::AboveOrEqual, ToRegister(lir->index()),
                             Imm32(ToInt32(lir->length())), lir->snapshot());
    }
    return bailoutCmp32(Assembler::BelowOrEqual, ToOperand(lir->length()),
                        ToRegister(lir->index()), lir->snapshot());
}

bool
CodeGenerator::visitBoundsCheckRange(LBoundsCheckRange *lir)
{
    int32_t min = lir->mir()->minimum();
    int32_t max = lir->mir()->maximum();
    JS_ASSERT(max >= min);

    Register temp = ToRegister(lir->getTemp(0));
    if (lir->index()->isConstant()) {
        int32_t nmin, nmax;
        int32_t index = ToInt32(lir->index());
        if (SafeAdd(index, min, &nmin) && SafeAdd(index, max, &nmax) && nmin >= 0) {
            return bailoutCmp32(Assembler::BelowOrEqual, ToOperand(lir->length()), Imm32(nmax),
                                lir->snapshot());
        }
        masm.mov(ImmWord(index), temp);
    } else {
        masm.mov(ToRegister(lir->index()), temp);
    }

    // If the minimum and maximum differ then do an underflow check first.
    // If the two are the same then doing an unsigned comparison on the
    // length will also catch a negative index.
    if (min != max) {
        if (min != 0) {
            Label bail;
            masm.branchAdd32(Assembler::Overflow, Imm32(min), temp, &bail);
            if (!bailoutFrom(&bail, lir->snapshot()))
                return false;
        }

        if (!bailoutCmp32(Assembler::LessThan, temp, Imm32(0), lir->snapshot()))
            return false;

        if (min != 0) {
            int32_t diff;
            if (SafeSub(max, min, &diff))
                max = diff;
            else
                masm.sub32(Imm32(min), temp);
        }
    }

    // Compute the maximum possible index. No overflow check is needed when
    // max > 0. We can only wraparound to a negative number, which will test as
    // larger than all nonnegative numbers in the unsigned comparison, and the
    // length is required to be nonnegative (else testing a negative length
    // would succeed on any nonnegative index).
    if (max != 0) {
        if (max < 0) {
            Label bail;
            masm.branchAdd32(Assembler::Overflow, Imm32(max), temp, &bail);
            if (!bailoutFrom(&bail, lir->snapshot()))
                return false;
        } else {
            masm.add32(Imm32(max), temp);
        }
    }

    return bailoutCmp32(Assembler::BelowOrEqual, ToOperand(lir->length()), temp, lir->snapshot());
}

bool
CodeGenerator::visitBoundsCheckLower(LBoundsCheckLower *lir)
{
    int32_t min = lir->mir()->minimum();
    return bailoutCmp32(Assembler::LessThan, ToRegister(lir->index()), Imm32(min),
                        lir->snapshot());
}

class OutOfLineStoreElementHole : public OutOfLineCodeBase<CodeGenerator>
{
    LInstruction *ins_;
    Label rejoinStore_;

  public:
    OutOfLineStoreElementHole(LInstruction *ins)
      : ins_(ins)
    {
        JS_ASSERT(ins->isStoreElementHoleV() || ins->isStoreElementHoleT());
    }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineStoreElementHole(this);
    }
    LInstruction *ins() const {
        return ins_;
    }
    Label *rejoinStore() {
        return &rejoinStore_;
    }
};

bool
CodeGenerator::emitStoreHoleCheck(Register elements, const LAllocation *index, LSnapshot *snapshot)
{
    Label bail;
    if (index->isConstant()) {
        masm.branchTestMagic(Assembler::Equal,
                             Address(elements, ToInt32(index) * sizeof(js::Value)), &bail);
    } else {
        masm.branchTestMagic(Assembler::Equal,
                             BaseIndex(elements, ToRegister(index), TimesEight), &bail);
    }
    return bailoutFrom(&bail, snapshot);
}

bool
CodeGenerator::visitStoreElementT(LStoreElementT *store)
{
    Register elements = ToRegister(store->elements());
    const LAllocation *index = store->index();

    if (store->mir()->needsBarrier())
       emitPreBarrier(elements, index, store->mir()->elementType());

    if (store->mir()->needsHoleCheck() && !emitStoreHoleCheck(elements, index, store->snapshot()))
        return false;

    storeElementTyped(store->value(), store->mir()->value()->type(), store->mir()->elementType(),
                      elements, index);
    return true;
}

bool
CodeGenerator::visitStoreElementV(LStoreElementV *lir)
{
    const ValueOperand value = ToValue(lir, LStoreElementV::Value);
    Register elements = ToRegister(lir->elements());
    const LAllocation *index = lir->index();

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, index, MIRType_Value);

    if (lir->mir()->needsHoleCheck() && !emitStoreHoleCheck(elements, index, lir->snapshot()))
        return false;

    if (lir->index()->isConstant())
        masm.storeValue(value, Address(elements, ToInt32(lir->index()) * sizeof(js::Value)));
    else
        masm.storeValue(value, BaseIndex(elements, ToRegister(lir->index()), TimesEight));
    return true;
}

bool
CodeGenerator::visitStoreElementHoleT(LStoreElementHoleT *lir)
{
    OutOfLineStoreElementHole *ool = new(alloc()) OutOfLineStoreElementHole(lir);
    if (!addOutOfLineCode(ool))
        return false;

    Register elements = ToRegister(lir->elements());
    const LAllocation *index = lir->index();

    // OOL path if index >= initializedLength.
    Address initLength(elements, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::BelowOrEqual, initLength, ToInt32Key(index), ool->entry());

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, index, lir->mir()->elementType());

    masm.bind(ool->rejoinStore());
    storeElementTyped(lir->value(), lir->mir()->value()->type(), lir->mir()->elementType(),
                      elements, index);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitStoreElementHoleV(LStoreElementHoleV *lir)
{
    OutOfLineStoreElementHole *ool = new(alloc()) OutOfLineStoreElementHole(lir);
    if (!addOutOfLineCode(ool))
        return false;

    Register elements = ToRegister(lir->elements());
    const LAllocation *index = lir->index();
    const ValueOperand value = ToValue(lir, LStoreElementHoleV::Value);

    // OOL path if index >= initializedLength.
    Address initLength(elements, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::BelowOrEqual, initLength, ToInt32Key(index), ool->entry());

    if (lir->mir()->needsBarrier())
        emitPreBarrier(elements, index, lir->mir()->elementType());

    masm.bind(ool->rejoinStore());
    if (lir->index()->isConstant())
        masm.storeValue(value, Address(elements, ToInt32(lir->index()) * sizeof(js::Value)));
    else
        masm.storeValue(value, BaseIndex(elements, ToRegister(lir->index()), TimesEight));

    masm.bind(ool->rejoin());
    return true;
}

typedef bool (*SetObjectElementFn)(JSContext *, HandleObject, HandleValue, HandleValue,
                                   bool strict);
typedef bool (*SetElementParFn)(ForkJoinContext *, HandleObject, HandleValue, HandleValue, bool);
static const VMFunctionsModal SetObjectElementInfo = VMFunctionsModal(
    FunctionInfo<SetObjectElementFn>(SetObjectElement),
    FunctionInfo<SetElementParFn>(SetElementPar));

bool
CodeGenerator::visitOutOfLineStoreElementHole(OutOfLineStoreElementHole *ool)
{
    Register object, elements;
    LInstruction *ins = ool->ins();
    const LAllocation *index;
    MIRType valueType;
    ConstantOrRegister value;

    if (ins->isStoreElementHoleV()) {
        LStoreElementHoleV *store = ins->toStoreElementHoleV();
        object = ToRegister(store->object());
        elements = ToRegister(store->elements());
        index = store->index();
        valueType = store->mir()->value()->type();
        value = TypedOrValueRegister(ToValue(store, LStoreElementHoleV::Value));
    } else {
        LStoreElementHoleT *store = ins->toStoreElementHoleT();
        object = ToRegister(store->object());
        elements = ToRegister(store->elements());
        index = store->index();
        valueType = store->mir()->value()->type();
        if (store->value()->isConstant())
            value = ConstantOrRegister(*store->value()->toConstant());
        else
            value = TypedOrValueRegister(valueType, ToAnyRegister(store->value()));
    }

    // If index == initializedLength, try to bump the initialized length inline.
    // If index > initializedLength, call a stub. Note that this relies on the
    // condition flags sticking from the incoming branch.
    Label callStub;
#ifdef JS_CODEGEN_MIPS
    // Had to reimplement for MIPS because there are no flags.
    Address initLength(elements, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::NotEqual, initLength, ToInt32Key(index), &callStub);
#else
    masm.j(Assembler::NotEqual, &callStub);
#endif

    Int32Key key = ToInt32Key(index);

    // Check array capacity.
    masm.branchKey(Assembler::BelowOrEqual, Address(elements, ObjectElements::offsetOfCapacity()),
                   key, &callStub);

    // Update initialized length. The capacity guard above ensures this won't overflow,
    // due to NELEMENTS_LIMIT.
    masm.bumpKey(&key, 1);
    masm.storeKey(key, Address(elements, ObjectElements::offsetOfInitializedLength()));

    // Update length if length < initializedLength.
    Label dontUpdate;
    masm.branchKey(Assembler::AboveOrEqual, Address(elements, ObjectElements::offsetOfLength()),
                   key, &dontUpdate);
    masm.storeKey(key, Address(elements, ObjectElements::offsetOfLength()));
    masm.bind(&dontUpdate);

    masm.bumpKey(&key, -1);

    if (ins->isStoreElementHoleT() && valueType != MIRType_Double) {
        // The inline path for StoreElementHoleT does not always store the type tag,
        // so we do the store on the OOL path. We use MIRType_None for the element type
        // so that storeElementTyped will always store the type tag.
        storeElementTyped(ins->toStoreElementHoleT()->value(), valueType, MIRType_None, elements,
                          index);
        masm.jump(ool->rejoin());
    } else {
        // Jump to the inline path where we will store the value.
        masm.jump(ool->rejoinStore());
    }

    masm.bind(&callStub);
    saveLive(ins);

    pushArg(Imm32(current->mir()->strict()));
    pushArg(value);
    if (index->isConstant())
        pushArg(*index->toConstant());
    else
        pushArg(TypedOrValueRegister(MIRType_Int32, ToAnyRegister(index)));
    pushArg(object);
    if (!callVM(SetObjectElementInfo, ins))
        return false;

    restoreLive(ins);
    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*ArrayPopShiftFn)(JSContext *, HandleObject, MutableHandleValue);
static const VMFunction ArrayPopDenseInfo = FunctionInfo<ArrayPopShiftFn>(jit::ArrayPopDense);
static const VMFunction ArrayShiftDenseInfo = FunctionInfo<ArrayPopShiftFn>(jit::ArrayShiftDense);

bool
CodeGenerator::emitArrayPopShift(LInstruction *lir, const MArrayPopShift *mir, Register obj,
                                 Register elementsTemp, Register lengthTemp, TypedOrValueRegister out)
{
    OutOfLineCode *ool;

    if (mir->mode() == MArrayPopShift::Pop) {
        ool = oolCallVM(ArrayPopDenseInfo, lir, (ArgList(), obj), StoreValueTo(out));
        if (!ool)
            return false;
    } else {
        JS_ASSERT(mir->mode() == MArrayPopShift::Shift);
        ool = oolCallVM(ArrayShiftDenseInfo, lir, (ArgList(), obj), StoreValueTo(out));
        if (!ool)
            return false;
    }

    // VM call if a write barrier is necessary.
    masm.branchTestNeedsBarrier(Assembler::NonZero, lengthTemp, ool->entry());

    // Load elements and length.
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), elementsTemp);
    masm.load32(Address(elementsTemp, ObjectElements::offsetOfLength()), lengthTemp);

    // VM call if length != initializedLength.
    Int32Key key = Int32Key(lengthTemp);
    Address initLength(elementsTemp, ObjectElements::offsetOfInitializedLength());
    masm.branchKey(Assembler::NotEqual, initLength, key, ool->entry());

    // Test for length != 0. On zero length either take a VM call or generate
    // an undefined value, depending on whether the call is known to produce
    // undefined.
    Label done;
    if (mir->maybeUndefined()) {
        Label notEmpty;
        masm.branchTest32(Assembler::NonZero, lengthTemp, lengthTemp, &notEmpty);
        masm.moveValue(UndefinedValue(), out.valueReg());
        masm.jump(&done);
        masm.bind(&notEmpty);
    } else {
        masm.branchTest32(Assembler::Zero, lengthTemp, lengthTemp, ool->entry());
    }

    masm.bumpKey(&key, -1);

    if (mir->mode() == MArrayPopShift::Pop) {
        masm.loadElementTypedOrValue(BaseIndex(elementsTemp, lengthTemp, TimesEight), out,
                                     mir->needsHoleCheck(), ool->entry());
    } else {
        JS_ASSERT(mir->mode() == MArrayPopShift::Shift);
        masm.loadElementTypedOrValue(Address(elementsTemp, 0), out, mir->needsHoleCheck(),
                                     ool->entry());
    }

    // Handle the failure case when the array length is non-writable in the
    // OOL path.  (Unlike in the adding-an-element cases, we can't rely on the
    // capacity <= length invariant for such arrays to avoid an explicit
    // check.)
    Address elementFlags(elementsTemp, ObjectElements::offsetOfFlags());
    Imm32 bit(ObjectElements::NONWRITABLE_ARRAY_LENGTH);
    masm.branchTest32(Assembler::NonZero, elementFlags, bit, ool->entry());

    // Now adjust length and initializedLength.
    masm.store32(lengthTemp, Address(elementsTemp, ObjectElements::offsetOfLength()));
    masm.store32(lengthTemp, Address(elementsTemp, ObjectElements::offsetOfInitializedLength()));

    if (mir->mode() == MArrayPopShift::Shift) {
        // Don't save the temp registers.
        RegisterSet temps;
        temps.add(elementsTemp);
        temps.add(lengthTemp);

        saveVolatile(temps);
        masm.setupUnalignedABICall(1, lengthTemp);
        masm.passABIArg(obj);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ArrayShiftMoveElements));
        restoreVolatile(temps);
    }

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArrayPopShiftV(LArrayPopShiftV *lir)
{
    Register obj = ToRegister(lir->object());
    Register elements = ToRegister(lir->temp0());
    Register length = ToRegister(lir->temp1());
    TypedOrValueRegister out(ToOutValue(lir));
    return emitArrayPopShift(lir, lir->mir(), obj, elements, length, out);
}

bool
CodeGenerator::visitArrayPopShiftT(LArrayPopShiftT *lir)
{
    Register obj = ToRegister(lir->object());
    Register elements = ToRegister(lir->temp0());
    Register length = ToRegister(lir->temp1());
    TypedOrValueRegister out(lir->mir()->type(), ToAnyRegister(lir->output()));
    return emitArrayPopShift(lir, lir->mir(), obj, elements, length, out);
}

typedef bool (*ArrayPushDenseFn)(JSContext *, HandleObject, HandleValue, uint32_t *);
static const VMFunction ArrayPushDenseInfo =
    FunctionInfo<ArrayPushDenseFn>(jit::ArrayPushDense);

bool
CodeGenerator::emitArrayPush(LInstruction *lir, const MArrayPush *mir, Register obj,
                             ConstantOrRegister value, Register elementsTemp, Register length)
{
    OutOfLineCode *ool = oolCallVM(ArrayPushDenseInfo, lir, (ArgList(), obj, value), StoreRegisterTo(length));
    if (!ool)
        return false;

    // Load elements and length.
    masm.loadPtr(Address(obj, JSObject::offsetOfElements()), elementsTemp);
    masm.load32(Address(elementsTemp, ObjectElements::offsetOfLength()), length);

    Int32Key key = Int32Key(length);
    Address initLength(elementsTemp, ObjectElements::offsetOfInitializedLength());
    Address capacity(elementsTemp, ObjectElements::offsetOfCapacity());

    // Guard length == initializedLength.
    masm.branchKey(Assembler::NotEqual, initLength, key, ool->entry());

    // Guard length < capacity.
    masm.branchKey(Assembler::BelowOrEqual, capacity, key, ool->entry());

    masm.storeConstantOrRegister(value, BaseIndex(elementsTemp, length, TimesEight));

    masm.bumpKey(&key, 1);
    masm.store32(length, Address(elementsTemp, ObjectElements::offsetOfLength()));
    masm.store32(length, Address(elementsTemp, ObjectElements::offsetOfInitializedLength()));

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArrayPushV(LArrayPushV *lir)
{
    Register obj = ToRegister(lir->object());
    Register elementsTemp = ToRegister(lir->temp());
    Register length = ToRegister(lir->output());
    ConstantOrRegister value = TypedOrValueRegister(ToValue(lir, LArrayPushV::Value));
    return emitArrayPush(lir, lir->mir(), obj, value, elementsTemp, length);
}

bool
CodeGenerator::visitArrayPushT(LArrayPushT *lir)
{
    Register obj = ToRegister(lir->object());
    Register elementsTemp = ToRegister(lir->temp());
    Register length = ToRegister(lir->output());
    ConstantOrRegister value;
    if (lir->value()->isConstant())
        value = ConstantOrRegister(*lir->value()->toConstant());
    else
        value = TypedOrValueRegister(lir->mir()->value()->type(), ToAnyRegister(lir->value()));
    return emitArrayPush(lir, lir->mir(), obj, value, elementsTemp, length);
}

typedef JSObject *(*ArrayConcatDenseFn)(JSContext *, HandleObject, HandleObject, HandleObject);
static const VMFunction ArrayConcatDenseInfo = FunctionInfo<ArrayConcatDenseFn>(ArrayConcatDense);

bool
CodeGenerator::visitArrayConcat(LArrayConcat *lir)
{
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());
    Register temp1 = ToRegister(lir->temp1());
    Register temp2 = ToRegister(lir->temp2());

    // If 'length == initializedLength' for both arrays we try to allocate an object
    // inline and pass it to the stub. Else, we just pass nullptr and the stub falls
    // back to a slow path.
    Label fail, call;
    masm.loadPtr(Address(lhs, JSObject::offsetOfElements()), temp1);
    masm.load32(Address(temp1, ObjectElements::offsetOfInitializedLength()), temp2);
    masm.branch32(Assembler::NotEqual, Address(temp1, ObjectElements::offsetOfLength()), temp2, &fail);

    masm.loadPtr(Address(rhs, JSObject::offsetOfElements()), temp1);
    masm.load32(Address(temp1, ObjectElements::offsetOfInitializedLength()), temp2);
    masm.branch32(Assembler::NotEqual, Address(temp1, ObjectElements::offsetOfLength()), temp2, &fail);

    // Try to allocate an object.
    JSObject *templateObj = lir->mir()->templateObj();
    masm.newGCThing(temp1, temp2, templateObj, &fail, lir->mir()->initialHeap());
    masm.initGCThing(temp1, temp2, templateObj);
    masm.jump(&call);
    {
        masm.bind(&fail);
        masm.movePtr(ImmPtr(nullptr), temp1);
    }
    masm.bind(&call);

    pushArg(temp1);
    pushArg(ToRegister(lir->rhs()));
    pushArg(ToRegister(lir->lhs()));
    return callVM(ArrayConcatDenseInfo, lir);
}

typedef JSObject *(*GetIteratorObjectFn)(JSContext *, HandleObject, uint32_t);
static const VMFunction GetIteratorObjectInfo = FunctionInfo<GetIteratorObjectFn>(GetIteratorObject);

bool
CodeGenerator::visitCallIteratorStart(LCallIteratorStart *lir)
{
    pushArg(Imm32(lir->mir()->flags()));
    pushArg(ToRegister(lir->object()));
    return callVM(GetIteratorObjectInfo, lir);
}

bool
CodeGenerator::visitIteratorStart(LIteratorStart *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register output = ToRegister(lir->output());

    uint32_t flags = lir->mir()->flags();

    OutOfLineCode *ool = oolCallVM(GetIteratorObjectInfo, lir,
                                   (ArgList(), obj, Imm32(flags)), StoreRegisterTo(output));
    if (!ool)
        return false;

    const Register temp1 = ToRegister(lir->temp1());
    const Register temp2 = ToRegister(lir->temp2());
    const Register niTemp = ToRegister(lir->temp3()); // Holds the NativeIterator object.

    // Iterators other than for-in should use LCallIteratorStart.
    JS_ASSERT(flags == JSITER_ENUMERATE);

    // Fetch the most recent iterator and ensure it's not nullptr.
    masm.loadPtr(AbsoluteAddress(GetIonContext()->runtime->addressOfLastCachedNativeIterator()), output);
    masm.branchTestPtr(Assembler::Zero, output, output, ool->entry());

    // Load NativeIterator.
    masm.loadObjPrivate(output, JSObject::ITER_CLASS_NFIXED_SLOTS, niTemp);

    // Ensure the |active| and |unreusable| bits are not set.
    masm.branchTest32(Assembler::NonZero, Address(niTemp, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_ACTIVE|JSITER_UNREUSABLE), ool->entry());

    // Load the iterator's shape array.
    masm.loadPtr(Address(niTemp, offsetof(NativeIterator, shapes_array)), temp2);

    // Compare shape of object with the first shape.
    masm.loadObjShape(obj, temp1);
    masm.branchPtr(Assembler::NotEqual, Address(temp2, 0), temp1, ool->entry());

    // Compare shape of object's prototype with the second shape.
    masm.loadObjProto(obj, temp1);
    masm.loadObjShape(temp1, temp1);
    masm.branchPtr(Assembler::NotEqual, Address(temp2, sizeof(Shape *)), temp1, ool->entry());

    // Ensure the object's prototype's prototype is nullptr. The last native
    // iterator will always have a prototype chain length of one (i.e. it must
    // be a plain ), so we do not need to generate a loop here.
    masm.loadObjProto(obj, temp1);
    masm.loadObjProto(temp1, temp1);
    masm.branchTestPtr(Assembler::NonZero, temp1, temp1, ool->entry());

    // Ensure the object does not have any elements. The presence of dense
    // elements is not captured by the shape tests above.
    masm.branchPtr(Assembler::NotEqual,
                   Address(obj, JSObject::offsetOfElements()),
                   ImmPtr(js::emptyObjectElements),
                   ool->entry());

    // Write barrier for stores to the iterator. We only need to take a write
    // barrier if NativeIterator::obj is actually going to change.
    {
#ifdef JSGC_GENERATIONAL
        // Bug 867815: When using a nursery, we unconditionally take this out-
        // of-line so that we do not have to post-barrier the store to
        // NativeIter::obj. This just needs JIT support for the Cell* buffer.
        Address objAddr(niTemp, offsetof(NativeIterator, obj));
        masm.branchPtr(Assembler::NotEqual, objAddr, obj, ool->entry());
#else
        Label noBarrier;
        masm.branchTestNeedsBarrier(Assembler::Zero, temp1, &noBarrier);

        Address objAddr(niTemp, offsetof(NativeIterator, obj));
        masm.branchPtr(Assembler::NotEqual, objAddr, obj, ool->entry());

        masm.bind(&noBarrier);
#endif // !JSGC_GENERATIONAL
    }

    // Mark iterator as active.
    masm.storePtr(obj, Address(niTemp, offsetof(NativeIterator, obj)));
    masm.or32(Imm32(JSITER_ACTIVE), Address(niTemp, offsetof(NativeIterator, flags)));

    // Chain onto the active iterator stack.
    masm.loadPtr(AbsoluteAddress(gen->compartment->addressOfEnumerators()), temp1);

    // ni->next = list
    masm.storePtr(temp1, Address(niTemp, NativeIterator::offsetOfNext()));

    // ni->prev = list->prev
    masm.loadPtr(Address(temp1, NativeIterator::offsetOfPrev()), temp2);
    masm.storePtr(temp2, Address(niTemp, NativeIterator::offsetOfPrev()));

    // list->prev->next = ni
    masm.storePtr(niTemp, Address(temp2, NativeIterator::offsetOfNext()));

    // list->prev = ni
    masm.storePtr(niTemp, Address(temp1, NativeIterator::offsetOfPrev()));

    masm.bind(ool->rejoin());
    return true;
}

static void
LoadNativeIterator(MacroAssembler &masm, Register obj, Register dest, Label *failures)
{
    JS_ASSERT(obj != dest);

    // Test class.
    masm.branchTestObjClass(Assembler::NotEqual, obj, dest, &PropertyIteratorObject::class_, failures);

    // Load NativeIterator object.
    masm.loadObjPrivate(obj, JSObject::ITER_CLASS_NFIXED_SLOTS, dest);
}

typedef bool (*IteratorNextFn)(JSContext *, HandleObject, MutableHandleValue);
static const VMFunction IteratorNextInfo = FunctionInfo<IteratorNextFn>(js_IteratorNext);

bool
CodeGenerator::visitIteratorNext(LIteratorNext *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register temp = ToRegister(lir->temp());
    const ValueOperand output = ToOutValue(lir);

    OutOfLineCode *ool = oolCallVM(IteratorNextInfo, lir, (ArgList(), obj), StoreValueTo(output));
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, temp, ool->entry());

    masm.branchTest32(Assembler::NonZero, Address(temp, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_FOREACH), ool->entry());

    // Get cursor, next string.
    masm.loadPtr(Address(temp, offsetof(NativeIterator, props_cursor)), output.scratchReg());
    masm.loadPtr(Address(output.scratchReg(), 0), output.scratchReg());
    masm.tagValue(JSVAL_TYPE_STRING, output.scratchReg(), output);

    // Increase the cursor.
    masm.addPtr(Imm32(sizeof(JSString *)), Address(temp, offsetof(NativeIterator, props_cursor)));

    masm.bind(ool->rejoin());
    return true;
}

typedef bool (*IteratorMoreFn)(JSContext *, HandleObject, bool *);
static const VMFunction IteratorMoreInfo = FunctionInfo<IteratorMoreFn>(jit::IteratorMore);

bool
CodeGenerator::visitIteratorMore(LIteratorMore *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register output = ToRegister(lir->output());
    const Register temp = ToRegister(lir->temp());

    OutOfLineCode *ool = oolCallVM(IteratorMoreInfo, lir,
                                   (ArgList(), obj), StoreRegisterTo(output));
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, output, ool->entry());

    masm.branchTest32(Assembler::NonZero, Address(output, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_FOREACH), ool->entry());

    // Set output to true if props_cursor < props_end.
    masm.loadPtr(Address(output, offsetof(NativeIterator, props_end)), temp);
    masm.cmpPtrSet(Assembler::LessThan, Address(output, offsetof(NativeIterator, props_cursor)),
                   temp, output);

    masm.bind(ool->rejoin());
    return true;
}

typedef bool (*CloseIteratorFn)(JSContext *, HandleObject);
static const VMFunction CloseIteratorInfo = FunctionInfo<CloseIteratorFn>(CloseIterator);

bool
CodeGenerator::visitIteratorEnd(LIteratorEnd *lir)
{
    const Register obj = ToRegister(lir->object());
    const Register temp1 = ToRegister(lir->temp1());
    const Register temp2 = ToRegister(lir->temp2());
    const Register temp3 = ToRegister(lir->temp3());

    OutOfLineCode *ool = oolCallVM(CloseIteratorInfo, lir, (ArgList(), obj), StoreNothing());
    if (!ool)
        return false;

    LoadNativeIterator(masm, obj, temp1, ool->entry());

    masm.branchTest32(Assembler::Zero, Address(temp1, offsetof(NativeIterator, flags)),
                      Imm32(JSITER_ENUMERATE), ool->entry());

    // Clear active bit.
    masm.and32(Imm32(~JSITER_ACTIVE), Address(temp1, offsetof(NativeIterator, flags)));

    // Reset property cursor.
    masm.loadPtr(Address(temp1, offsetof(NativeIterator, props_array)), temp2);
    masm.storePtr(temp2, Address(temp1, offsetof(NativeIterator, props_cursor)));

    // Unlink from the iterator list.
    const Register next = temp2;
    const Register prev = temp3;
    masm.loadPtr(Address(temp1, NativeIterator::offsetOfNext()), next);
    masm.loadPtr(Address(temp1, NativeIterator::offsetOfPrev()), prev);
    masm.storePtr(prev, Address(next, NativeIterator::offsetOfPrev()));
    masm.storePtr(next, Address(prev, NativeIterator::offsetOfNext()));
#ifdef DEBUG
    masm.storePtr(ImmPtr(nullptr), Address(temp1, NativeIterator::offsetOfNext()));
    masm.storePtr(ImmPtr(nullptr), Address(temp1, NativeIterator::offsetOfPrev()));
#endif

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitArgumentsLength(LArgumentsLength *lir)
{
    // read number of actual arguments from the JS frame.
    Register argc = ToRegister(lir->output());
    Address ptr(StackPointer, frameSize() + IonJSFrameLayout::offsetOfNumActualArgs());

    masm.loadPtr(ptr, argc);
    return true;
}

bool
CodeGenerator::visitGetFrameArgument(LGetFrameArgument *lir)
{
    ValueOperand result = GetValueOutput(lir);
    const LAllocation *index = lir->index();
    size_t argvOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs();

    if (index->isConstant()) {
        int32_t i = index->toConstant()->toInt32();
        Address argPtr(StackPointer, sizeof(Value) * i + argvOffset);
        masm.loadValue(argPtr, result);
    } else {
        Register i = ToRegister(index);
        BaseIndex argPtr(StackPointer, i, ScaleFromElemWidth(sizeof(Value)), argvOffset);
        masm.loadValue(argPtr, result);
    }
    return true;
}

bool
CodeGenerator::visitSetFrameArgumentT(LSetFrameArgumentT *lir)
{
    size_t argOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs() +
                       (sizeof(Value) * lir->mir()->argno());

    MIRType type = lir->mir()->value()->type();

    if (type == MIRType_Double) {
        // Store doubles directly.
        FloatRegister input = ToFloatRegister(lir->input());
        masm.storeDouble(input, Address(StackPointer, argOffset));

    } else {
        Register input = ToRegister(lir->input());
        masm.storeValue(ValueTypeFromMIRType(type), input, Address(StackPointer, argOffset));
    }
    return true;
}

bool
CodeGenerator:: visitSetFrameArgumentC(LSetFrameArgumentC *lir)
{
    size_t argOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs() +
                       (sizeof(Value) * lir->mir()->argno());
    masm.storeValue(lir->val(), Address(StackPointer, argOffset));
    return true;
}

bool
CodeGenerator:: visitSetFrameArgumentV(LSetFrameArgumentV *lir)
{
    const ValueOperand val = ToValue(lir, LSetFrameArgumentV::Input);
    size_t argOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs() +
                       (sizeof(Value) * lir->mir()->argno());
    masm.storeValue(val, Address(StackPointer, argOffset));
    return true;
}

typedef bool (*RunOnceScriptPrologueFn)(JSContext *, HandleScript);
static const VMFunction RunOnceScriptPrologueInfo =
    FunctionInfo<RunOnceScriptPrologueFn>(js::RunOnceScriptPrologue);

bool
CodeGenerator::visitRunOncePrologue(LRunOncePrologue *lir)
{
    pushArg(ImmGCPtr(lir->mir()->block()->info().script()));
    return callVM(RunOnceScriptPrologueInfo, lir);
}


typedef JSObject *(*InitRestParameterFn)(JSContext *, uint32_t, Value *, HandleObject,
                                         HandleObject);
typedef JSObject *(*InitRestParameterParFn)(ForkJoinContext *, uint32_t, Value *,
                                            HandleObject, HandleObject);
static const VMFunctionsModal InitRestParameterInfo = VMFunctionsModal(
    FunctionInfo<InitRestParameterFn>(InitRestParameter),
    FunctionInfo<InitRestParameterParFn>(InitRestParameterPar));

bool
CodeGenerator::emitRest(LInstruction *lir, Register array, Register numActuals,
                        Register temp0, Register temp1, unsigned numFormals,
                        JSObject *templateObject)
{
    // Compute actuals() + numFormals.
    size_t actualsOffset = frameSize() + IonJSFrameLayout::offsetOfActualArgs();
    masm.movePtr(StackPointer, temp1);
    masm.addPtr(Imm32(sizeof(Value) * numFormals + actualsOffset), temp1);

    // Compute numActuals - numFormals.
    Label emptyLength, joinLength;
    masm.movePtr(numActuals, temp0);
    masm.branch32(Assembler::LessThanOrEqual, temp0, Imm32(numFormals), &emptyLength);
    masm.sub32(Imm32(numFormals), temp0);
    masm.jump(&joinLength);
    {
        masm.bind(&emptyLength);
        masm.move32(Imm32(0), temp0);
    }
    masm.bind(&joinLength);

    pushArg(array);
    pushArg(ImmGCPtr(templateObject));
    pushArg(temp1);
    pushArg(temp0);

    return callVM(InitRestParameterInfo, lir);
}

bool
CodeGenerator::visitRest(LRest *lir)
{
    Register numActuals = ToRegister(lir->numActuals());
    Register temp0 = ToRegister(lir->getTemp(0));
    Register temp1 = ToRegister(lir->getTemp(1));
    Register temp2 = ToRegister(lir->getTemp(2));
    unsigned numFormals = lir->mir()->numFormals();
    JSObject *templateObject = lir->mir()->templateObject();

    Label joinAlloc, failAlloc;
    masm.newGCThing(temp2, temp0, templateObject, &failAlloc, gc::DefaultHeap);
    masm.initGCThing(temp2, temp0, templateObject);
    masm.jump(&joinAlloc);
    {
        masm.bind(&failAlloc);
        masm.movePtr(ImmPtr(nullptr), temp2);
    }
    masm.bind(&joinAlloc);

    return emitRest(lir, temp2, numActuals, temp0, temp1, numFormals, templateObject);
}

bool
CodeGenerator::visitRestPar(LRestPar *lir)
{
    Register numActuals = ToRegister(lir->numActuals());
    Register cx = ToRegister(lir->forkJoinContext());
    Register temp0 = ToRegister(lir->getTemp(0));
    Register temp1 = ToRegister(lir->getTemp(1));
    Register temp2 = ToRegister(lir->getTemp(2));
    unsigned numFormals = lir->mir()->numFormals();
    JSObject *templateObject = lir->mir()->templateObject();

    if (!emitAllocateGCThingPar(lir, temp2, cx, temp0, temp1, templateObject))
        return false;

    return emitRest(lir, temp2, numActuals, temp0, temp1, numFormals, templateObject);
}

bool
CodeGenerator::generateAsmJS()
{
    IonSpew(IonSpew_Codegen, "# Emitting asm.js code");

    // AsmJS doesn't do profiler instrumentation.
    sps_.disable();

    // The caller (either another asm.js function or the external-entry
    // trampoline) has placed all arguments in registers and on the stack
    // according to the system ABI. The MAsmJSParameters which represent these
    // parameters have been useFixed()ed to these ABI-specified positions.
    // Thus, there is nothing special to do in the prologue except (possibly)
    // bump the stack.
    if (!generatePrologue())
        return false;
    if (!generateBody())
        return false;
    if (!generateEpilogue())
        return false;
#if defined(JS_ION_PERF)
    // Note the end of the inline code and start of the OOL code.
    gen->perfSpewer().noteEndInlineCode(masm);
#endif
    if (!generateOutOfLineCode())
        return false;

    // The only remaining work needed to compile this function is to patch the
    // switch-statement jump tables (the entries of the table need the absolute
    // address of the cases). These table entries are accmulated as CodeLabels
    // in the MacroAssembler's codeLabels_ list and processed all at once at in
    // the "static-link" phase of module compilation. It is critical that there
    // is nothing else to do after this point since the LifoAlloc memory
    // holding the MIR graph is about to be popped and reused. In particular,
    // every step in CodeGenerator::link must be a nop, as asserted here:
    JS_ASSERT(snapshots_.listSize() == 0);
    JS_ASSERT(snapshots_.RVATableSize() == 0);
    JS_ASSERT(recovers_.size() == 0);
    JS_ASSERT(bailouts_.empty());
    JS_ASSERT(graph.numConstants() == 0);
    JS_ASSERT(safepointIndices_.empty());
    JS_ASSERT(osiIndices_.empty());
    JS_ASSERT(cacheList_.empty());
    JS_ASSERT(safepoints_.size() == 0);
    return true;
}

bool
CodeGenerator::generate()
{
    IonSpew(IonSpew_Codegen, "# Emitting code for script %s:%d",
            gen->info().script()->filename(),
            gen->info().script()->lineno());

    if (!snapshots_.init())
        return false;

    if (!safepoints_.init(gen->alloc(), graph.totalSlotCount()))
        return false;

#if JS_TRACE_LOGGING
    masm.tracelogStart(gen->info().script());
    masm.tracelogLog(TraceLogging::INFO_ENGINE_IONMONKEY);
#endif

    // Before generating any code, we generate type checks for all parameters.
    // This comes before deoptTable_, because we can't use deopt tables without
    // creating the actual frame.
    if (!generateArgumentsChecks())
        return false;

    if (frameClass_ != FrameSizeClass::None()) {
        deoptTable_ = gen->jitRuntime()->getBailoutTable(frameClass_);
        if (!deoptTable_)
            return false;
    }

#if JS_TRACE_LOGGING
    Label skip;
    masm.jump(&skip);
#endif

    // Remember the entry offset to skip the argument check.
    masm.flushBuffer();
    setSkipArgCheckEntryOffset(masm.size());

#if JS_TRACE_LOGGING
    masm.tracelogStart(gen->info().script());
    masm.tracelogLog(TraceLogging::INFO_ENGINE_IONMONKEY);
    masm.bind(&skip);
#endif

#ifdef DEBUG
    // Assert that the argument types are correct.
    if (!generateArgumentsChecks(/* bailout = */ false))
        return false;
#endif

    if (!generatePrologue())
        return false;
    if (!generateBody())
        return false;
    if (!generateEpilogue())
        return false;
    if (!generateInvalidateEpilogue())
        return false;
#if defined(JS_ION_PERF)
    // Note the end of the inline code and start of the OOL code.
    perfSpewer_.noteEndInlineCode(masm);
#endif
    if (!generateOutOfLineCode())
        return false;

    return !masm.oom();
}

bool
CodeGenerator::link(JSContext *cx, types::CompilerConstraintList *constraints)
{
    RootedScript script(cx, gen->info().script());
    ExecutionMode executionMode = gen->info().executionMode();
    OptimizationLevel optimizationLevel = gen->optimizationInfo().level();

    JS_ASSERT_IF(HasIonScript(script, executionMode), executionMode == SequentialExecution);

    // We finished the new IonScript. Invalidate the current active IonScript,
    // so we can replace it with this new (probably higher optimized) version.
    if (HasIonScript(script, executionMode)) {
        JS_ASSERT(GetIonScript(script, executionMode)->isRecompiling());
        // Do a normal invalidate, except don't cancel offThread compilations,
        // since that will cancel this compilation too.
        if (!Invalidate(cx, script, SequentialExecution,
                        /* resetUses */ false, /* cancelOffThread*/ false))
        {
            return false;
        }
    }

    // Check to make sure we didn't have a mid-build invalidation. If so, we
    // will trickle to jit::Compile() and return Method_Skipped.
    types::RecompileInfo recompileInfo;
    if (!types::FinishCompilation(cx, script, executionMode, constraints, &recompileInfo))
        return true;

    uint32_t scriptFrameSize = frameClass_ == FrameSizeClass::None()
                           ? frameDepth_
                           : FrameSizeClass::FromDepth(frameDepth_).frameSize();

    // We encode safepoints after the OSI-point offsets have been determined.
    encodeSafepoints();

    // List of possible scripts that this graph may call. Currently this is
    // only tracked when compiling for parallel execution.
    CallTargetVector callTargets(alloc());
    if (executionMode == ParallelExecution)
        AddPossibleCallees(cx, graph.mir(), callTargets);

    IonScript *ionScript =
      IonScript::New(cx, recompileInfo,
                     graph.totalSlotCount(), scriptFrameSize,
                     snapshots_.listSize(), snapshots_.RVATableSize(),
                     recovers_.size(), bailouts_.length(), graph.numConstants(),
                     safepointIndices_.length(), osiIndices_.length(),
                     cacheList_.length(), runtimeData_.length(),
                     safepoints_.size(), callTargets.length(),
                     patchableBackedges_.length(), optimizationLevel);
    if (!ionScript) {
        recompileInfo.compilerOutput(cx->zone()->types)->invalidate();
        return false;
    }

    // Lock the runtime against interrupt callbacks during the link.
    // We don't want an interrupt request to protect the code for the script
    // before it has been filled in, as we could segv before the runtime's
    // patchable backedges have been fully updated.
    JSRuntime::AutoLockForInterrupt lock(cx->runtime());

    // Make sure we don't segv while filling in the code, to avoid deadlocking
    // inside the signal handler.
    cx->runtime()->jitRuntime()->ensureIonCodeAccessible(cx->runtime());

    // Implicit interrupts are used only for sequential code. In parallel mode
    // use the normal executable allocator so that we cannot segv during
    // execution off the main thread.
    Linker linker(masm);
    JitCode *code = (executionMode == SequentialExecution)
                    ? linker.newCodeForIonScript(cx)
                    : linker.newCode<CanGC>(cx, JSC::ION_CODE);
    if (!code) {
        // Use js_free instead of IonScript::Destroy: the cache list and
        // backedge list are still uninitialized.
        js_free(ionScript);
        recompileInfo.compilerOutput(cx->zone()->types)->invalidate();
        return false;
    }

    ionScript->setMethod(code);
    ionScript->setSkipArgCheckEntryOffset(getSkipArgCheckEntryOffset());

    // If SPS is enabled, mark IonScript as having been instrumented with SPS
    if (sps_.enabled())
        ionScript->setHasSPSInstrumentation();

    SetIonScript(script, executionMode, ionScript);

    if (cx->runtime()->spsProfiler.enabled()) {
        const char *filename = script->filename();
        if (filename == nullptr)
            filename = "<unknown>";
        unsigned len = strlen(filename) + 50;
        char *buf = js_pod_malloc<char>(len);
        if (!buf)
            return false;
        JS_snprintf(buf, len, "Ion compiled %s:%d", filename, (int) script->lineno());
        cx->runtime()->spsProfiler.markEvent(buf);
        js_free(buf);
    }

    // In parallel execution mode, when we first compile a script, we
    // don't know that its potential callees are compiled, so set a
    // flag warning that the callees may not be fully compiled.
    if (!callTargets.empty())
        ionScript->setHasUncompiledCallTarget();

    invalidateEpilogueData_.fixup(&masm);
    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, invalidateEpilogueData_),
                                       ImmPtr(ionScript),
                                       ImmPtr((void*)-1));

    IonSpew(IonSpew_Codegen, "Created IonScript %p (raw %p)",
            (void *) ionScript, (void *) code->raw());

    ionScript->setInvalidationEpilogueDataOffset(invalidateEpilogueData_.offset());
    ionScript->setOsrPc(gen->info().osrPc());
    ionScript->setOsrEntryOffset(getOsrEntryOffset());
    ptrdiff_t real_invalidate = masm.actualOffset(invalidate_.offset());
    ionScript->setInvalidationEpilogueOffset(real_invalidate);

    ionScript->setDeoptTable(deoptTable_);

#if defined(JS_ION_PERF)
    if (PerfEnabled())
        perfSpewer_.writeProfile(script, code, masm);
#endif

    for (size_t i = 0; i < ionScriptLabels_.length(); i++) {
        ionScriptLabels_[i].fixup(&masm);
        Assembler::patchDataWithValueCheck(CodeLocationLabel(code, ionScriptLabels_[i]),
                                           ImmPtr(ionScript),
                                           ImmPtr((void*)-1));
    }

    // for generating inline caches during the execution.
    if (runtimeData_.length())
        ionScript->copyRuntimeData(&runtimeData_[0]);
    if (cacheList_.length())
        ionScript->copyCacheEntries(&cacheList_[0], masm);

    // for marking during GC.
    if (safepointIndices_.length())
        ionScript->copySafepointIndices(&safepointIndices_[0], masm);
    if (safepoints_.size())
        ionScript->copySafepoints(&safepoints_);

    // for reconvering from an Ion Frame.
    if (bailouts_.length())
        ionScript->copyBailoutTable(&bailouts_[0]);
    if (osiIndices_.length())
        ionScript->copyOsiIndices(&osiIndices_[0], masm);
    if (snapshots_.listSize())
        ionScript->copySnapshots(&snapshots_);
    MOZ_ASSERT_IF(snapshots_.listSize(), recovers_.size());
    if (recovers_.size())
        ionScript->copyRecovers(&recovers_);
    if (graph.numConstants())
        ionScript->copyConstants(graph.constantPool());
    if (callTargets.length() > 0)
        ionScript->copyCallTargetEntries(callTargets.begin());
    if (patchableBackedges_.length() > 0)
        ionScript->copyPatchableBackedges(cx, code, patchableBackedges_.begin());

    switch (executionMode) {
      case SequentialExecution:
        // The correct state for prebarriers is unknown until the end of compilation,
        // since a GC can occur during code generation. All barriers are emitted
        // off-by-default, and are toggled on here if necessary.
        if (cx->zone()->needsBarrier())
            ionScript->toggleBarriers(true);
        break;
      case ParallelExecution:
        // We don't run incremental GC during parallel execution; no need to
        // turn on barriers.
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

    return true;
}

// An out-of-line path to convert a boxed int32 to either a float or double.
class OutOfLineUnboxFloatingPoint : public OutOfLineCodeBase<CodeGenerator>
{
    LUnboxFloatingPoint *unboxFloatingPoint_;

  public:
    OutOfLineUnboxFloatingPoint(LUnboxFloatingPoint *unboxFloatingPoint)
      : unboxFloatingPoint_(unboxFloatingPoint)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineUnboxFloatingPoint(this);
    }

    LUnboxFloatingPoint *unboxFloatingPoint() const {
        return unboxFloatingPoint_;
    }
};

bool
CodeGenerator::visitUnboxFloatingPoint(LUnboxFloatingPoint *lir)
{
    const ValueOperand box = ToValue(lir, LUnboxFloatingPoint::Input);
    const LDefinition *result = lir->output();

    // Out-of-line path to convert int32 to double or bailout
    // if this instruction is fallible.
    OutOfLineUnboxFloatingPoint *ool = new(alloc()) OutOfLineUnboxFloatingPoint(lir);
    if (!addOutOfLineCode(ool))
        return false;

    FloatRegister resultReg = ToFloatRegister(result);
    masm.branchTestDouble(Assembler::NotEqual, box, ool->entry());
    masm.unboxDouble(box, resultReg);
    if (lir->type() == MIRType_Float32)
        masm.convertDoubleToFloat32(resultReg, resultReg);
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineUnboxFloatingPoint(OutOfLineUnboxFloatingPoint *ool)
{
    LUnboxFloatingPoint *ins = ool->unboxFloatingPoint();
    const ValueOperand value = ToValue(ins, LUnboxFloatingPoint::Input);

    if (ins->mir()->fallible()) {
        Label bail;
        masm.branchTestInt32(Assembler::NotEqual, value, &bail);
        if (!bailoutFrom(&bail, ins->snapshot()))
            return false;
    }
    masm.int32ValueToFloatingPoint(value, ToFloatRegister(ins->output()), ins->type());
    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*GetPropertyFn)(JSContext *, HandleValue, HandlePropertyName, MutableHandleValue);
static const VMFunction GetPropertyInfo = FunctionInfo<GetPropertyFn>(GetProperty);
static const VMFunction CallPropertyInfo = FunctionInfo<GetPropertyFn>(CallProperty);

bool
CodeGenerator::visitCallGetProperty(LCallGetProperty *lir)
{
    pushArg(ImmGCPtr(lir->mir()->name()));
    pushArg(ToValue(lir, LCallGetProperty::Value));

    if (lir->mir()->callprop())
        return callVM(CallPropertyInfo, lir);
    return callVM(GetPropertyInfo, lir);
}

typedef bool (*GetOrCallElementFn)(JSContext *, MutableHandleValue, HandleValue, MutableHandleValue);
static const VMFunction GetElementInfo = FunctionInfo<GetOrCallElementFn>(js::GetElement);
static const VMFunction CallElementInfo = FunctionInfo<GetOrCallElementFn>(js::CallElement);

bool
CodeGenerator::visitCallGetElement(LCallGetElement *lir)
{
    pushArg(ToValue(lir, LCallGetElement::RhsInput));
    pushArg(ToValue(lir, LCallGetElement::LhsInput));

    JSOp op = JSOp(*lir->mir()->resumePoint()->pc());

    if (op == JSOP_GETELEM) {
        return callVM(GetElementInfo, lir);
    } else {
        JS_ASSERT(op == JSOP_CALLELEM);
        return callVM(CallElementInfo, lir);
    }
}

bool
CodeGenerator::visitCallSetElement(LCallSetElement *lir)
{
    pushArg(Imm32(current->mir()->strict()));
    pushArg(ToValue(lir, LCallSetElement::Value));
    pushArg(ToValue(lir, LCallSetElement::Index));
    pushArg(ToRegister(lir->getOperand(0)));
    return callVM(SetObjectElementInfo, lir);
}

typedef bool (*InitElementArrayFn)(JSContext *, jsbytecode *, HandleObject, uint32_t, HandleValue);
static const VMFunction InitElementArrayInfo = FunctionInfo<InitElementArrayFn>(js::InitElementArray);

bool
CodeGenerator::visitCallInitElementArray(LCallInitElementArray *lir)
{
    pushArg(ToValue(lir, LCallInitElementArray::Value));
    pushArg(Imm32(lir->mir()->index()));
    pushArg(ToRegister(lir->getOperand(0)));
    pushArg(ImmPtr(lir->mir()->resumePoint()->pc()));
    return callVM(InitElementArrayInfo, lir);
}

bool
CodeGenerator::visitLoadFixedSlotV(LLoadFixedSlotV *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();
    ValueOperand result = GetValueOutput(ins);

    masm.loadValue(Address(obj, JSObject::getFixedSlotOffset(slot)), result);
    return true;
}

bool
CodeGenerator::visitLoadFixedSlotT(LLoadFixedSlotT *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();
    AnyRegister result = ToAnyRegister(ins->getDef(0));
    MIRType type = ins->mir()->type();

    masm.loadUnboxedValue(Address(obj, JSObject::getFixedSlotOffset(slot)), type, result);

    return true;
}

bool
CodeGenerator::visitStoreFixedSlotV(LStoreFixedSlotV *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();

    const ValueOperand value = ToValue(ins, LStoreFixedSlotV::Value);

    Address address(obj, JSObject::getFixedSlotOffset(slot));
    if (ins->mir()->needsBarrier())
        emitPreBarrier(address, MIRType_Value);

    masm.storeValue(value, address);

    return true;
}

bool
CodeGenerator::visitStoreFixedSlotT(LStoreFixedSlotT *ins)
{
    const Register obj = ToRegister(ins->getOperand(0));
    size_t slot = ins->mir()->slot();

    const LAllocation *value = ins->value();
    MIRType valueType = ins->mir()->value()->type();

    ConstantOrRegister nvalue = value->isConstant()
                              ? ConstantOrRegister(*value->toConstant())
                              : TypedOrValueRegister(valueType, ToAnyRegister(value));

    Address address(obj, JSObject::getFixedSlotOffset(slot));
    if (ins->mir()->needsBarrier())
        emitPreBarrier(address, MIRType_Value);

    masm.storeConstantOrRegister(nvalue, address);

    return true;
}

bool
CodeGenerator::visitCallsiteCloneCache(LCallsiteCloneCache *ins)
{
    const MCallsiteCloneCache *mir = ins->mir();
    Register callee = ToRegister(ins->callee());
    Register output = ToRegister(ins->output());

    CallsiteCloneIC cache(callee, mir->block()->info().script(), mir->callPc(), output);
    return addCache(ins, allocateCache(cache));
}

typedef JSObject *(*CallsiteCloneICFn)(JSContext *, size_t, HandleObject);
const VMFunction CallsiteCloneIC::UpdateInfo =
    FunctionInfo<CallsiteCloneICFn>(CallsiteCloneIC::update);

bool
CodeGenerator::visitCallsiteCloneIC(OutOfLineUpdateCache *ool, DataPtr<CallsiteCloneIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->calleeReg());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(CallsiteCloneIC::UpdateInfo, lir))
        return false;
    StoreRegisterTo(ic->outputReg()).generate(this);
    restoreLiveIgnore(lir, StoreRegisterTo(ic->outputReg()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitGetNameCache(LGetNameCache *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register scopeChain = ToRegister(ins->scopeObj());
    TypedOrValueRegister output(GetValueOutput(ins));
    bool isTypeOf = ins->mir()->accessKind() != MGetNameCache::NAME;

    NameIC cache(liveRegs, isTypeOf, scopeChain, ins->mir()->name(), output);
    return addCache(ins, allocateCache(cache));
}

typedef bool (*NameICFn)(JSContext *, size_t, HandleObject, MutableHandleValue);
const VMFunction NameIC::UpdateInfo = FunctionInfo<NameICFn>(NameIC::update);

bool
CodeGenerator::visitNameIC(OutOfLineUpdateCache *ool, DataPtr<NameIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->scopeChainReg());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(NameIC::UpdateInfo, lir))
        return false;
    StoreValueTo(ic->outputReg()).generate(this);
    restoreLiveIgnore(lir, StoreValueTo(ic->outputReg()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::addGetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                                   PropertyName *name, TypedOrValueRegister output,
                                   bool monitoredResult)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
        GetPropertyIC cache(liveRegs, objReg, name, output, monitoredResult);
        return addCache(ins, allocateCache(cache));
      }
      case ParallelExecution: {
        GetPropertyParIC cache(objReg, name, output);
        return addCache(ins, allocateCache(cache));
      }
      default:
        MOZ_ASSUME_UNREACHABLE("Bad execution mode");
    }
}

bool
CodeGenerator::addSetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                                   PropertyName *name, ConstantOrRegister value, bool strict,
                                   bool needsTypeBarrier)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
          SetPropertyIC cache(liveRegs, objReg, name, value, strict, needsTypeBarrier);
          return addCache(ins, allocateCache(cache));
      }
      case ParallelExecution: {
          SetPropertyParIC cache(objReg, name, value, strict, needsTypeBarrier);
          return addCache(ins, allocateCache(cache));
      }
      default:
        MOZ_ASSUME_UNREACHABLE("Bad execution mode");
    }
}

bool
CodeGenerator::addSetElementCache(LInstruction *ins, Register obj, Register unboxIndex,
                                  Register temp, FloatRegister tempFloat, ValueOperand index,
                                  ConstantOrRegister value, bool strict, bool guardHoles)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
        SetElementIC cache(obj, unboxIndex, temp, tempFloat, index, value, strict,
                           guardHoles);
        return addCache(ins, allocateCache(cache));
      }
      case ParallelExecution: {
        SetElementParIC cache(obj, unboxIndex, temp, tempFloat, index, value, strict,
                              guardHoles);
        return addCache(ins, allocateCache(cache));
      }
      default:
        MOZ_ASSUME_UNREACHABLE("Bad execution mode");
    }
}

bool
CodeGenerator::visitGetPropertyCacheV(LGetPropertyCacheV *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    PropertyName *name = ins->mir()->name();
    bool monitoredResult = ins->mir()->monitoredResult();
    TypedOrValueRegister output = TypedOrValueRegister(GetValueOutput(ins));

    return addGetPropertyCache(ins, liveRegs, objReg, name, output, monitoredResult);
}

bool
CodeGenerator::visitGetPropertyCacheT(LGetPropertyCacheT *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    PropertyName *name = ins->mir()->name();
    bool monitoredResult = ins->mir()->monitoredResult();
    TypedOrValueRegister output(ins->mir()->type(), ToAnyRegister(ins->getDef(0)));

    return addGetPropertyCache(ins, liveRegs, objReg, name, output, monitoredResult);
}

typedef bool (*GetPropertyICFn)(JSContext *, size_t, HandleObject, MutableHandleValue);
const VMFunction GetPropertyIC::UpdateInfo =
    FunctionInfo<GetPropertyICFn>(GetPropertyIC::update);

bool
CodeGenerator::visitGetPropertyIC(OutOfLineUpdateCache *ool, DataPtr<GetPropertyIC> &ic)
{
    LInstruction *lir = ool->lir();

    if (ic->idempotent()) {
        size_t numLocs;
        CacheLocationList &cacheLocs = lir->mirRaw()->toGetPropertyCache()->location();
        size_t locationBase = addCacheLocations(cacheLocs, &numLocs);
        ic->setLocationInfo(locationBase, numLocs);
    }

    saveLive(lir);

    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(GetPropertyIC::UpdateInfo, lir))
        return false;
    StoreValueTo(ic->output()).generate(this);
    restoreLiveIgnore(lir, StoreValueTo(ic->output()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*GetPropertyParICFn)(ForkJoinContext *, size_t, HandleObject, MutableHandleValue);
const VMFunction GetPropertyParIC::UpdateInfo =
    FunctionInfo<GetPropertyParICFn>(GetPropertyParIC::update);

bool
CodeGenerator::visitGetPropertyParIC(OutOfLineUpdateCache *ool, DataPtr<GetPropertyParIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(GetPropertyParIC::UpdateInfo, lir))
        return false;
    StoreValueTo(ic->output()).generate(this);
    restoreLiveIgnore(lir, StoreValueTo(ic->output()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::addGetElementCache(LInstruction *ins, Register obj, ConstantOrRegister index,
                                  TypedOrValueRegister output, bool monitoredResult,
                                  bool allowDoubleResult)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
        RegisterSet liveRegs = ins->safepoint()->liveRegs();
        GetElementIC cache(liveRegs, obj, index, output, monitoredResult, allowDoubleResult);
        return addCache(ins, allocateCache(cache));
      }
      case ParallelExecution: {
        GetElementParIC cache(obj, index, output, monitoredResult, allowDoubleResult);
        return addCache(ins, allocateCache(cache));
      }
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
}

bool
CodeGenerator::visitGetElementCacheV(LGetElementCacheV *ins)
{
    Register obj = ToRegister(ins->object());
    ConstantOrRegister index = TypedOrValueRegister(ToValue(ins, LGetElementCacheV::Index));
    TypedOrValueRegister output = TypedOrValueRegister(GetValueOutput(ins));
    const MGetElementCache *mir = ins->mir();

    return addGetElementCache(ins, obj, index, output, mir->monitoredResult(), mir->allowDoubleResult());
}

bool
CodeGenerator::visitGetElementCacheT(LGetElementCacheT *ins)
{
    Register obj = ToRegister(ins->object());
    ConstantOrRegister index = TypedOrValueRegister(MIRType_Int32, ToAnyRegister(ins->index()));
    TypedOrValueRegister output(ins->mir()->type(), ToAnyRegister(ins->output()));
    const MGetElementCache *mir = ins->mir();

    return addGetElementCache(ins, obj, index, output, mir->monitoredResult(), mir->allowDoubleResult());
}

typedef bool (*GetElementICFn)(JSContext *, size_t, HandleObject, HandleValue, MutableHandleValue);
const VMFunction GetElementIC::UpdateInfo =
    FunctionInfo<GetElementICFn>(GetElementIC::update);

bool
CodeGenerator::visitGetElementIC(OutOfLineUpdateCache *ool, DataPtr<GetElementIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->index());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(GetElementIC::UpdateInfo, lir))
        return false;
    StoreValueTo(ic->output()).generate(this);
    restoreLiveIgnore(lir, StoreValueTo(ic->output()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitSetElementCacheV(LSetElementCacheV *ins)
{
    Register obj = ToRegister(ins->object());
    Register unboxIndex = ToTempUnboxRegister(ins->tempToUnboxIndex());
    Register temp = ToRegister(ins->temp());
    FloatRegister tempFloat = ToFloatRegister(ins->tempFloat());
    ValueOperand index = ToValue(ins, LSetElementCacheV::Index);
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LSetElementCacheV::Value));

    return addSetElementCache(ins, obj, unboxIndex, temp, tempFloat, index, value,
                              ins->mir()->strict(), ins->mir()->guardHoles());
}

bool
CodeGenerator::visitSetElementCacheT(LSetElementCacheT *ins)
{
    Register obj = ToRegister(ins->object());
    Register unboxIndex = ToTempUnboxRegister(ins->tempToUnboxIndex());
    Register temp = ToRegister(ins->temp());
    FloatRegister tempFloat = ToFloatRegister(ins->tempFloat());
    ValueOperand index = ToValue(ins, LSetElementCacheT::Index);
    ConstantOrRegister value;
    const LAllocation *tmp = ins->value();
    if (tmp->isConstant())
        value = *tmp->toConstant();
    else
        value = TypedOrValueRegister(ins->mir()->value()->type(), ToAnyRegister(tmp));

    return addSetElementCache(ins, obj, unboxIndex, temp, tempFloat, index, value,
                              ins->mir()->strict(), ins->mir()->guardHoles());
}

typedef bool (*SetElementICFn)(JSContext *, size_t, HandleObject, HandleValue, HandleValue);
const VMFunction SetElementIC::UpdateInfo =
    FunctionInfo<SetElementICFn>(SetElementIC::update);

bool
CodeGenerator::visitSetElementIC(OutOfLineUpdateCache *ool, DataPtr<SetElementIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->value());
    pushArg(ic->index());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(SetElementIC::UpdateInfo, lir))
        return false;
    restoreLive(lir);

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*SetElementParICFn)(ForkJoinContext *, size_t, HandleObject, HandleValue, HandleValue);
const VMFunction SetElementParIC::UpdateInfo =
    FunctionInfo<SetElementParICFn>(SetElementParIC::update);

bool
CodeGenerator::visitSetElementParIC(OutOfLineUpdateCache *ool, DataPtr<SetElementParIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->value());
    pushArg(ic->index());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(SetElementParIC::UpdateInfo, lir))
        return false;
    restoreLive(lir);

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*GetElementParICFn)(ForkJoinContext *, size_t, HandleObject, HandleValue,
                                  MutableHandleValue);
const VMFunction GetElementParIC::UpdateInfo =
    FunctionInfo<GetElementParICFn>(GetElementParIC::update);

bool
CodeGenerator::visitGetElementParIC(OutOfLineUpdateCache *ool, DataPtr<GetElementParIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->index());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(GetElementParIC::UpdateInfo, lir))
        return false;
    StoreValueTo(ic->output()).generate(this);
    restoreLiveIgnore(lir, StoreValueTo(ic->output()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitBindNameCache(LBindNameCache *ins)
{
    Register scopeChain = ToRegister(ins->scopeChain());
    Register output = ToRegister(ins->output());
    BindNameIC cache(scopeChain, ins->mir()->name(), output);

    return addCache(ins, allocateCache(cache));
}

typedef JSObject *(*BindNameICFn)(JSContext *, size_t, HandleObject);
const VMFunction BindNameIC::UpdateInfo =
    FunctionInfo<BindNameICFn>(BindNameIC::update);

bool
CodeGenerator::visitBindNameIC(OutOfLineUpdateCache *ool, DataPtr<BindNameIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->scopeChainReg());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(BindNameIC::UpdateInfo, lir))
        return false;
    StoreRegisterTo(ic->outputReg()).generate(this);
    restoreLiveIgnore(lir, StoreRegisterTo(ic->outputReg()).clobbered());

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*SetPropertyFn)(JSContext *, HandleObject,
                              HandlePropertyName, const HandleValue, bool, jsbytecode *);
typedef bool (*SetPropertyParFn)(ForkJoinContext *, HandleObject,
                                 HandlePropertyName, const HandleValue, bool, jsbytecode *);
static const VMFunctionsModal SetPropertyInfo = VMFunctionsModal(
    FunctionInfo<SetPropertyFn>(SetProperty),
    FunctionInfo<SetPropertyParFn>(SetPropertyPar));

bool
CodeGenerator::visitCallSetProperty(LCallSetProperty *ins)
{
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LCallSetProperty::Value));

    const Register objReg = ToRegister(ins->getOperand(0));

    pushArg(ImmPtr(ins->mir()->resumePoint()->pc()));
    pushArg(Imm32(ins->mir()->strict()));

    pushArg(value);
    pushArg(ImmGCPtr(ins->mir()->name()));
    pushArg(objReg);

    return callVM(SetPropertyInfo, ins);
}

typedef bool (*DeletePropertyFn)(JSContext *, HandleValue, HandlePropertyName, bool *);
static const VMFunction DeletePropertyStrictInfo =
    FunctionInfo<DeletePropertyFn>(DeleteProperty<true>);
static const VMFunction DeletePropertyNonStrictInfo =
    FunctionInfo<DeletePropertyFn>(DeleteProperty<false>);

bool
CodeGenerator::visitCallDeleteProperty(LCallDeleteProperty *lir)
{
    pushArg(ImmGCPtr(lir->mir()->name()));
    pushArg(ToValue(lir, LCallDeleteProperty::Value));

    if (lir->mir()->block()->info().script()->strict())
        return callVM(DeletePropertyStrictInfo, lir);

    return callVM(DeletePropertyNonStrictInfo, lir);
}

typedef bool (*DeleteElementFn)(JSContext *, HandleValue, HandleValue, bool *);
static const VMFunction DeleteElementStrictInfo =
    FunctionInfo<DeleteElementFn>(DeleteElement<true>);
static const VMFunction DeleteElementNonStrictInfo =
    FunctionInfo<DeleteElementFn>(DeleteElement<false>);

bool
CodeGenerator::visitCallDeleteElement(LCallDeleteElement *lir)
{
    pushArg(ToValue(lir, LCallDeleteElement::Index));
    pushArg(ToValue(lir, LCallDeleteElement::Value));

    if (lir->mir()->block()->info().script()->strict())
        return callVM(DeleteElementStrictInfo, lir);

    return callVM(DeleteElementNonStrictInfo, lir);
}

bool
CodeGenerator::visitSetPropertyCacheV(LSetPropertyCacheV *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LSetPropertyCacheV::Value));

    return addSetPropertyCache(ins, liveRegs, objReg, ins->mir()->name(), value,
                               ins->mir()->strict(), ins->mir()->needsTypeBarrier());
}

bool
CodeGenerator::visitSetPropertyCacheT(LSetPropertyCacheT *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    ConstantOrRegister value;

    if (ins->getOperand(1)->isConstant())
        value = ConstantOrRegister(*ins->getOperand(1)->toConstant());
    else
        value = TypedOrValueRegister(ins->valueType(), ToAnyRegister(ins->getOperand(1)));

    return addSetPropertyCache(ins, liveRegs, objReg, ins->mir()->name(), value,
                               ins->mir()->strict(), ins->mir()->needsTypeBarrier());
}

typedef bool (*SetPropertyICFn)(JSContext *, size_t, HandleObject, HandleValue);
const VMFunction SetPropertyIC::UpdateInfo =
    FunctionInfo<SetPropertyICFn>(SetPropertyIC::update);

bool
CodeGenerator::visitSetPropertyIC(OutOfLineUpdateCache *ool, DataPtr<SetPropertyIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->value());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(SetPropertyIC::UpdateInfo, lir))
        return false;
    restoreLive(lir);

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*SetPropertyParICFn)(ForkJoinContext *, size_t, HandleObject, HandleValue);
const VMFunction SetPropertyParIC::UpdateInfo =
    FunctionInfo<SetPropertyParICFn>(SetPropertyParIC::update);

bool
CodeGenerator::visitSetPropertyParIC(OutOfLineUpdateCache *ool, DataPtr<SetPropertyParIC> &ic)
{
    LInstruction *lir = ool->lir();
    saveLive(lir);

    pushArg(ic->value());
    pushArg(ic->object());
    pushArg(Imm32(ool->getCacheIndex()));
    if (!callVM(SetPropertyParIC::UpdateInfo, lir))
        return false;
    restoreLive(lir);

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*ThrowFn)(JSContext *, HandleValue);
static const VMFunction ThrowInfoCodeGen = FunctionInfo<ThrowFn>(js::Throw);

bool
CodeGenerator::visitThrow(LThrow *lir)
{
    pushArg(ToValue(lir, LThrow::Value));
    return callVM(ThrowInfoCodeGen, lir);
}

typedef bool (*BitNotFn)(JSContext *, HandleValue, int *p);
typedef bool (*BitNotParFn)(ForkJoinContext *, HandleValue, int32_t *);
static const VMFunctionsModal BitNotInfo = VMFunctionsModal(
    FunctionInfo<BitNotFn>(BitNot),
    FunctionInfo<BitNotParFn>(BitNotPar));

bool
CodeGenerator::visitBitNotV(LBitNotV *lir)
{
    pushArg(ToValue(lir, LBitNotV::Input));
    return callVM(BitNotInfo, lir);
}

typedef bool (*BitopFn)(JSContext *, HandleValue, HandleValue, int *p);
typedef bool (*BitopParFn)(ForkJoinContext *, HandleValue, HandleValue, int32_t *);
static const VMFunctionsModal BitAndInfo = VMFunctionsModal(
    FunctionInfo<BitopFn>(BitAnd),
    FunctionInfo<BitopParFn>(BitAndPar));
static const VMFunctionsModal BitOrInfo = VMFunctionsModal(
    FunctionInfo<BitopFn>(BitOr),
    FunctionInfo<BitopParFn>(BitOrPar));
static const VMFunctionsModal BitXorInfo = VMFunctionsModal(
    FunctionInfo<BitopFn>(BitXor),
    FunctionInfo<BitopParFn>(BitXorPar));
static const VMFunctionsModal BitLhsInfo = VMFunctionsModal(
    FunctionInfo<BitopFn>(BitLsh),
    FunctionInfo<BitopParFn>(BitLshPar));
static const VMFunctionsModal BitRhsInfo = VMFunctionsModal(
    FunctionInfo<BitopFn>(BitRsh),
    FunctionInfo<BitopParFn>(BitRshPar));

bool
CodeGenerator::visitBitOpV(LBitOpV *lir)
{
    pushArg(ToValue(lir, LBitOpV::RhsInput));
    pushArg(ToValue(lir, LBitOpV::LhsInput));

    switch (lir->jsop()) {
      case JSOP_BITAND:
        return callVM(BitAndInfo, lir);
      case JSOP_BITOR:
        return callVM(BitOrInfo, lir);
      case JSOP_BITXOR:
        return callVM(BitXorInfo, lir);
      case JSOP_LSH:
        return callVM(BitLhsInfo, lir);
      case JSOP_RSH:
        return callVM(BitRhsInfo, lir);
      default:
        break;
    }
    MOZ_ASSUME_UNREACHABLE("unexpected bitop");
}

class OutOfLineTypeOfV : public OutOfLineCodeBase<CodeGenerator>
{
    LTypeOfV *ins_;

  public:
    OutOfLineTypeOfV(LTypeOfV *ins)
      : ins_(ins)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineTypeOfV(this);
    }
    LTypeOfV *ins() const {
        return ins_;
    }
};

bool
CodeGenerator::visitTypeOfV(LTypeOfV *lir)
{
    const ValueOperand value = ToValue(lir, LTypeOfV::Input);
    Register output = ToRegister(lir->output());
    Register tag = masm.splitTagForTest(value);

    const JSAtomState &names = GetIonContext()->runtime->names();
    Label done;

    OutOfLineTypeOfV *ool = nullptr;
    if (lir->mir()->inputMaybeCallableOrEmulatesUndefined()) {
        // The input may be a callable object (result is "function") or may
        // emulate undefined (result is "undefined"). Use an OOL path.
        ool = new(alloc()) OutOfLineTypeOfV(lir);
        if (!addOutOfLineCode(ool))
            return false;

        masm.branchTestObject(Assembler::Equal, tag, ool->entry());
    } else {
        // Input is not callable and does not emulate undefined, so if
        // it's an object the result is always "object".
        Label notObject;
        masm.branchTestObject(Assembler::NotEqual, tag, &notObject);
        masm.movePtr(ImmGCPtr(names.object), output);
        masm.jump(&done);
        masm.bind(&notObject);
    }

    Label notNumber;
    masm.branchTestNumber(Assembler::NotEqual, tag, &notNumber);
    masm.movePtr(ImmGCPtr(names.number), output);
    masm.jump(&done);
    masm.bind(&notNumber);

    Label notUndefined;
    masm.branchTestUndefined(Assembler::NotEqual, tag, &notUndefined);
    masm.movePtr(ImmGCPtr(names.undefined), output);
    masm.jump(&done);
    masm.bind(&notUndefined);

    Label notNull;
    masm.branchTestNull(Assembler::NotEqual, tag, &notNull);
    masm.movePtr(ImmGCPtr(names.object), output);
    masm.jump(&done);
    masm.bind(&notNull);

    Label notBoolean;
    masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
    masm.movePtr(ImmGCPtr(names.boolean), output);
    masm.jump(&done);
    masm.bind(&notBoolean);

    masm.movePtr(ImmGCPtr(names.string), output);

    masm.bind(&done);
    if (ool)
        masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool)
{
    LTypeOfV *ins = ool->ins();

    ValueOperand input = ToValue(ins, LTypeOfV::Input);
    Register temp = ToTempUnboxRegister(ins->tempToUnbox());
    Register output = ToRegister(ins->output());

    Register obj = masm.extractObject(input, temp);

    saveVolatile(output);
    masm.setupUnalignedABICall(2, output);
    masm.passABIArg(obj);
    masm.movePtr(ImmPtr(GetIonContext()->runtime), output);
    masm.passABIArg(output);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::TypeOfObjectOperation));
    masm.storeCallResult(output);
    restoreVolatile(output);

    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*ToIdFn)(JSContext *, HandleScript, jsbytecode *, HandleValue, HandleValue,
                       MutableHandleValue);
static const VMFunction ToIdInfo = FunctionInfo<ToIdFn>(ToIdOperation);

bool
CodeGenerator::visitToIdV(LToIdV *lir)
{
    Label notInt32;
    FloatRegister temp = ToFloatRegister(lir->tempFloat());
    const ValueOperand out = ToOutValue(lir);
    ValueOperand index = ToValue(lir, LToIdV::Index);

    OutOfLineCode *ool = oolCallVM(ToIdInfo, lir,
                                   (ArgList(),
                                   ImmGCPtr(current->mir()->info().script()),
                                   ImmPtr(lir->mir()->resumePoint()->pc()),
                                   ToValue(lir, LToIdV::Object),
                                   ToValue(lir, LToIdV::Index)),
                                   StoreValueTo(out));

    Register tag = masm.splitTagForTest(index);

    masm.branchTestInt32(Assembler::NotEqual, tag, &notInt32);
    masm.moveValue(index, out);
    masm.jump(ool->rejoin());

    masm.bind(&notInt32);
    masm.branchTestDouble(Assembler::NotEqual, tag, ool->entry());
    masm.unboxDouble(index, temp);
    masm.convertDoubleToInt32(temp, out.scratchReg(), ool->entry(), true);
    masm.tagValue(JSVAL_TYPE_INT32, out.scratchReg(), out);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitLoadElementV(LLoadElementV *load)
{
    Register elements = ToRegister(load->elements());
    const ValueOperand out = ToOutValue(load);

    if (load->index()->isConstant())
        masm.loadValue(Address(elements, ToInt32(load->index()) * sizeof(Value)), out);
    else
        masm.loadValue(BaseIndex(elements, ToRegister(load->index()), TimesEight), out);

    if (load->mir()->needsHoleCheck()) {
        Label testMagic;
        masm.branchTestMagic(Assembler::Equal, out, &testMagic);
        if (!bailoutFrom(&testMagic, load->snapshot()))
            return false;
    }

    return true;
}

bool
CodeGenerator::visitLoadElementHole(LLoadElementHole *lir)
{
    Register elements = ToRegister(lir->elements());
    Register initLength = ToRegister(lir->initLength());
    const ValueOperand out = ToOutValue(lir);

    const MLoadElementHole *mir = lir->mir();

    // If the index is out of bounds, load |undefined|. Otherwise, load the
    // value.
    Label undefined, done;
    if (lir->index()->isConstant()) {
        masm.branch32(Assembler::BelowOrEqual, initLength, Imm32(ToInt32(lir->index())), &undefined);
        masm.loadValue(Address(elements, ToInt32(lir->index()) * sizeof(Value)), out);
    } else {
        masm.branch32(Assembler::BelowOrEqual, initLength, ToRegister(lir->index()), &undefined);
        masm.loadValue(BaseIndex(elements, ToRegister(lir->index()), TimesEight), out);
    }

    // If a hole check is needed, and the value wasn't a hole, we're done.
    // Otherwise, we'll load undefined.
    if (lir->mir()->needsHoleCheck())
        masm.branchTestMagic(Assembler::NotEqual, out, &done);
    else
        masm.jump(&done);

    masm.bind(&undefined);

    if (mir->needsNegativeIntCheck()) {
        if (lir->index()->isConstant()) {
            if (ToInt32(lir->index()) < 0 && !bailout(lir->snapshot()))
                return false;
        } else {
            Label negative;
            masm.branch32(Assembler::LessThan, ToRegister(lir->index()), Imm32(0), &negative);
            if (!bailoutFrom(&negative, lir->snapshot()))
                return false;
        }
    }

    masm.moveValue(UndefinedValue(), out);
    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitLoadTypedArrayElement(LLoadTypedArrayElement *lir)
{
    Register elements = ToRegister(lir->elements());
    Register temp = lir->temp()->isBogusTemp() ? InvalidReg : ToRegister(lir->temp());
    AnyRegister out = ToAnyRegister(lir->output());

    int arrayType = lir->mir()->arrayType();
    int width = TypedArrayObject::slotWidth(arrayType);

    Label fail;
    if (lir->index()->isConstant()) {
        Address source(elements, ToInt32(lir->index()) * width);
        masm.loadFromTypedArray(arrayType, source, out, temp, &fail);
    } else {
        BaseIndex source(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        masm.loadFromTypedArray(arrayType, source, out, temp, &fail);
    }

    if (fail.used() && !bailoutFrom(&fail, lir->snapshot()))
        return false;

    return true;
}

bool
CodeGenerator::visitLoadTypedArrayElementHole(LLoadTypedArrayElementHole *lir)
{
    Register object = ToRegister(lir->object());
    const ValueOperand out = ToOutValue(lir);

    // Load the length.
    Register scratch = out.scratchReg();
    Int32Key key = ToInt32Key(lir->index());
    masm.unboxInt32(Address(object, TypedArrayObject::lengthOffset()), scratch);

    // Load undefined unless length > key.
    Label inbounds, done;
    masm.branchKey(Assembler::Above, scratch, key, &inbounds);
    masm.moveValue(UndefinedValue(), out);
    masm.jump(&done);

    // Load the elements vector.
    masm.bind(&inbounds);
    masm.loadPtr(Address(object, TypedArrayObject::dataOffset()), scratch);

    int arrayType = lir->mir()->arrayType();
    int width = TypedArrayObject::slotWidth(arrayType);

    Label fail;
    if (key.isConstant()) {
        Address source(scratch, key.constant() * width);
        masm.loadFromTypedArray(arrayType, source, out, lir->mir()->allowDouble(),
                                out.scratchReg(), &fail);
    } else {
        BaseIndex source(scratch, key.reg(), ScaleFromElemWidth(width));
        masm.loadFromTypedArray(arrayType, source, out, lir->mir()->allowDouble(),
                                out.scratchReg(), &fail);
    }

    if (fail.used() && !bailoutFrom(&fail, lir->snapshot()))
        return false;

    masm.bind(&done);
    return true;
}

template <typename T>
static inline void
StoreToTypedArray(MacroAssembler &masm, int arrayType, const LAllocation *value, const T &dest)
{
    if (arrayType == ScalarTypeDescr::TYPE_FLOAT32 ||
        arrayType == ScalarTypeDescr::TYPE_FLOAT64)
    {
        masm.storeToTypedFloatArray(arrayType, ToFloatRegister(value), dest);
    } else {
        if (value->isConstant())
            masm.storeToTypedIntArray(arrayType, Imm32(ToInt32(value)), dest);
        else
            masm.storeToTypedIntArray(arrayType, ToRegister(value), dest);
    }
}

bool
CodeGenerator::visitStoreTypedArrayElement(LStoreTypedArrayElement *lir)
{
    Register elements = ToRegister(lir->elements());
    const LAllocation *value = lir->value();

    int arrayType = lir->mir()->arrayType();
    int width = TypedArrayObject::slotWidth(arrayType);

    if (lir->index()->isConstant()) {
        Address dest(elements, ToInt32(lir->index()) * width);
        StoreToTypedArray(masm, arrayType, value, dest);
    } else {
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        StoreToTypedArray(masm, arrayType, value, dest);
    }

    return true;
}

bool
CodeGenerator::visitStoreTypedArrayElementHole(LStoreTypedArrayElementHole *lir)
{
    Register elements = ToRegister(lir->elements());
    const LAllocation *value = lir->value();

    int arrayType = lir->mir()->arrayType();
    int width = TypedArrayObject::slotWidth(arrayType);

    bool guardLength = true;
    if (lir->index()->isConstant() && lir->length()->isConstant()) {
        uint32_t idx = ToInt32(lir->index());
        uint32_t len = ToInt32(lir->length());
        if (idx >= len)
            return true;
        guardLength = false;
    }
    Label skip;
    if (lir->index()->isConstant()) {
        uint32_t idx = ToInt32(lir->index());
        if (guardLength)
            masm.branch32(Assembler::BelowOrEqual, ToOperand(lir->length()), Imm32(idx), &skip);
        Address dest(elements, idx * width);
        StoreToTypedArray(masm, arrayType, value, dest);
    } else {
        Register idxReg = ToRegister(lir->index());
        JS_ASSERT(guardLength);
        if (lir->length()->isConstant())
            masm.branch32(Assembler::AboveOrEqual, idxReg, Imm32(ToInt32(lir->length())), &skip);
        else
            masm.branch32(Assembler::BelowOrEqual, ToOperand(lir->length()), idxReg, &skip);
        BaseIndex dest(elements, ToRegister(lir->index()), ScaleFromElemWidth(width));
        StoreToTypedArray(masm, arrayType, value, dest);
    }
    if (guardLength)
        masm.bind(&skip);

    return true;
}

bool
CodeGenerator::visitClampIToUint8(LClampIToUint8 *lir)
{
    Register output = ToRegister(lir->output());
    JS_ASSERT(output == ToRegister(lir->input()));
    masm.clampIntToUint8(output);
    return true;
}

bool
CodeGenerator::visitClampDToUint8(LClampDToUint8 *lir)
{
    FloatRegister input = ToFloatRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.clampDoubleToUint8(input, output);
    return true;
}

bool
CodeGenerator::visitClampVToUint8(LClampVToUint8 *lir)
{
    ValueOperand operand = ToValue(lir, LClampVToUint8::Input);
    FloatRegister tempFloat = ToFloatRegister(lir->tempFloat());
    Register output = ToRegister(lir->output());
    MDefinition *input = lir->mir()->input();

    Label *stringEntry, *stringRejoin;
    if (input->mightBeType(MIRType_String)) {
        OutOfLineCode *oolString = oolCallVM(StringToNumberInfo, lir, (ArgList(), output),
                                             StoreFloatRegisterTo(tempFloat));
        if (!oolString)
            return false;
        stringEntry = oolString->entry();
        stringRejoin = oolString->rejoin();
    } else {
        stringEntry = nullptr;
        stringRejoin = nullptr;
    }

    Label fails;
    masm.clampValueToUint8(operand, input,
                           stringEntry, stringRejoin,
                           output, tempFloat, output, &fails);

    if (!bailoutFrom(&fails, lir->snapshot()))
        return false;

    return true;
}

typedef bool (*OperatorInFn)(JSContext *, HandleValue, HandleObject, bool *);
static const VMFunction OperatorInInfo = FunctionInfo<OperatorInFn>(OperatorIn);

bool
CodeGenerator::visitIn(LIn *ins)
{
    pushArg(ToRegister(ins->rhs()));
    pushArg(ToValue(ins, LIn::LHS));

    return callVM(OperatorInInfo, ins);
}

typedef bool (*OperatorInIFn)(JSContext *, uint32_t, HandleObject, bool *);
static const VMFunction OperatorInIInfo = FunctionInfo<OperatorInIFn>(OperatorInI);

bool
CodeGenerator::visitInArray(LInArray *lir)
{
    const MInArray *mir = lir->mir();
    Register elements = ToRegister(lir->elements());
    Register initLength = ToRegister(lir->initLength());
    Register output = ToRegister(lir->output());

    // When the array is not packed we need to do a hole check in addition to the bounds check.
    Label falseBranch, done, trueBranch;

    OutOfLineCode *ool = nullptr;
    Label* failedInitLength = &falseBranch;

    if (lir->index()->isConstant()) {
        int32_t index = ToInt32(lir->index());

        JS_ASSERT_IF(index < 0, mir->needsNegativeIntCheck());
        if (mir->needsNegativeIntCheck()) {
            ool = oolCallVM(OperatorInIInfo, lir,
                            (ArgList(), Imm32(index), ToRegister(lir->object())),
                            StoreRegisterTo(output));
            failedInitLength = ool->entry();
        }

        masm.branch32(Assembler::BelowOrEqual, initLength, Imm32(index), failedInitLength);
        if (mir->needsHoleCheck()) {
            Address address = Address(elements, index * sizeof(Value));
            masm.branchTestMagic(Assembler::Equal, address, &falseBranch);
        }
    } else {
        Label negativeIntCheck;
        Register index = ToRegister(lir->index());

        if (mir->needsNegativeIntCheck())
            failedInitLength = &negativeIntCheck;

        masm.branch32(Assembler::BelowOrEqual, initLength, index, failedInitLength);
        if (mir->needsHoleCheck()) {
            BaseIndex address = BaseIndex(elements, ToRegister(lir->index()), TimesEight);
            masm.branchTestMagic(Assembler::Equal, address, &falseBranch);
        }
        masm.jump(&trueBranch);

        if (mir->needsNegativeIntCheck()) {
            masm.bind(&negativeIntCheck);
            ool = oolCallVM(OperatorInIInfo, lir,
                            (ArgList(), index, ToRegister(lir->object())),
                            StoreRegisterTo(output));

            masm.branch32(Assembler::LessThan, index, Imm32(0), ool->entry());
            masm.jump(&falseBranch);
        }
    }

    masm.bind(&trueBranch);
    masm.move32(Imm32(1), output);
    masm.jump(&done);

    masm.bind(&falseBranch);
    masm.move32(Imm32(0), output);
    masm.bind(&done);

    if (ool)
        masm.bind(ool->rejoin());

    return true;
}

bool
CodeGenerator::visitInstanceOfO(LInstanceOfO *ins)
{
    return emitInstanceOf(ins, ins->mir()->prototypeObject());
}

bool
CodeGenerator::visitInstanceOfV(LInstanceOfV *ins)
{
    return emitInstanceOf(ins, ins->mir()->prototypeObject());
}

// Wrap IsDelegateOfObject, which takes a JSObject*, not a HandleObject
static bool
IsDelegateObject(JSContext *cx, HandleObject protoObj, HandleObject obj, bool *res)
{
    return IsDelegateOfObject(cx, protoObj, obj, res);
}

typedef bool (*IsDelegateObjectFn)(JSContext *, HandleObject, HandleObject, bool *);
static const VMFunction IsDelegateObjectInfo = FunctionInfo<IsDelegateObjectFn>(IsDelegateObject);

bool
CodeGenerator::emitInstanceOf(LInstruction *ins, JSObject *prototypeObject)
{
    // This path implements fun_hasInstance when the function's prototype is
    // known to be prototypeObject.

    Label done;
    Register output = ToRegister(ins->getDef(0));

    // If the lhs is a primitive, the result is false.
    Register objReg;
    if (ins->isInstanceOfV()) {
        Label isObject;
        ValueOperand lhsValue = ToValue(ins, LInstanceOfV::LHS);
        masm.branchTestObject(Assembler::Equal, lhsValue, &isObject);
        masm.mov(ImmWord(0), output);
        masm.jump(&done);
        masm.bind(&isObject);
        objReg = masm.extractObject(lhsValue, output);
    } else {
        objReg = ToRegister(ins->toInstanceOfO()->lhs());
    }

    // Crawl the lhs's prototype chain in a loop to search for prototypeObject.
    // This follows the main loop of js::IsDelegate, though additionally breaks
    // out of the loop on Proxy::LazyProto.

    // Load the lhs's prototype.
    masm.loadObjProto(objReg, output);

    Label testLazy;
    {
        Label loopPrototypeChain;
        masm.bind(&loopPrototypeChain);

        // Test for the target prototype object.
        Label notPrototypeObject;
        masm.branchPtr(Assembler::NotEqual, output, ImmGCPtr(prototypeObject), &notPrototypeObject);
        masm.mov(ImmWord(1), output);
        masm.jump(&done);
        masm.bind(&notPrototypeObject);

        JS_ASSERT(uintptr_t(TaggedProto::LazyProto) == 1);

        // Test for nullptr or Proxy::LazyProto
        masm.branchPtr(Assembler::BelowOrEqual, output, ImmWord(1), &testLazy);

        // Load the current object's prototype.
        masm.loadObjProto(output, output);

        masm.jump(&loopPrototypeChain);
    }

    // Make a VM call if an object with a lazy proto was found on the prototype
    // chain. This currently occurs only for cross compartment wrappers, which
    // we do not expect to be compared with non-wrapper functions from this
    // compartment. Otherwise, we stopped on a nullptr prototype and the output
    // register is already correct.

    OutOfLineCode *ool = oolCallVM(IsDelegateObjectInfo, ins,
                                   (ArgList(), ImmGCPtr(prototypeObject), objReg),
                                   StoreRegisterTo(output));

    // Regenerate the original lhs object for the VM call.
    Label regenerate, *lazyEntry;
    if (objReg != output) {
        lazyEntry = ool->entry();
    } else {
        masm.bind(&regenerate);
        lazyEntry = &regenerate;
        if (ins->isInstanceOfV()) {
            ValueOperand lhsValue = ToValue(ins, LInstanceOfV::LHS);
            objReg = masm.extractObject(lhsValue, output);
        } else {
            objReg = ToRegister(ins->toInstanceOfO()->lhs());
        }
        JS_ASSERT(objReg == output);
        masm.jump(ool->entry());
    }

    masm.bind(&testLazy);
    masm.branchPtr(Assembler::Equal, output, ImmWord(1), lazyEntry);

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

typedef bool (*HasInstanceFn)(JSContext *, HandleObject, HandleValue, bool *);
static const VMFunction HasInstanceInfo = FunctionInfo<HasInstanceFn>(js::HasInstance);

bool
CodeGenerator::visitCallInstanceOf(LCallInstanceOf *ins)
{
    ValueOperand lhs = ToValue(ins, LCallInstanceOf::LHS);
    Register rhs = ToRegister(ins->rhs());
    JS_ASSERT(ToRegister(ins->output()) == ReturnReg);

    pushArg(lhs);
    pushArg(rhs);
    return callVM(HasInstanceInfo, ins);
}

bool
CodeGenerator::visitGetDOMProperty(LGetDOMProperty *ins)
{
    const Register JSContextReg = ToRegister(ins->getJSContextReg());
    const Register ObjectReg = ToRegister(ins->getObjectReg());
    const Register PrivateReg = ToRegister(ins->getPrivReg());
    const Register ValueReg = ToRegister(ins->getValueReg());

    DebugOnly<uint32_t> initialStack = masm.framePushed();

    masm.checkStackAlignment();

    // Make space for the outparam.  Pre-initialize it to UndefinedValue so we
    // can trace it at GC time.
    masm.Push(UndefinedValue());
    // We pass the pointer to our out param as an instance of
    // JSJitGetterCallArgs, since on the binary level it's the same thing.
    JS_STATIC_ASSERT(sizeof(JSJitGetterCallArgs) == sizeof(Value*));
    masm.movePtr(StackPointer, ValueReg);

    masm.Push(ObjectReg);

    // GetReservedSlot(obj, DOM_OBJECT_SLOT).toPrivate()
    masm.loadPrivate(Address(ObjectReg, JSObject::getFixedSlotOffset(0)), PrivateReg);

    // Rooting will happen at GC time.
    masm.movePtr(StackPointer, ObjectReg);

    uint32_t safepointOffset;
    if (!masm.buildFakeExitFrame(JSContextReg, &safepointOffset))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_DOMGETTER);

    if (!markSafepointAt(safepointOffset, ins))
        return false;

    masm.setupUnalignedABICall(4, JSContextReg);

    masm.loadJSContext(JSContextReg);

    masm.passABIArg(JSContextReg);
    masm.passABIArg(ObjectReg);
    masm.passABIArg(PrivateReg);
    masm.passABIArg(ValueReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ins->mir()->fun()));

    if (ins->mir()->isInfallible()) {
        masm.loadValue(Address(StackPointer, IonDOMExitFrameLayout::offsetOfResult()),
                       JSReturnOperand);
    } else {
        masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

        masm.loadValue(Address(StackPointer, IonDOMExitFrameLayout::offsetOfResult()),
                       JSReturnOperand);
    }
    masm.adjustStack(IonDOMExitFrameLayout::Size());

    JS_ASSERT(masm.framePushed() == initialStack);
    return true;
}

bool
CodeGenerator::visitGetDOMMember(LGetDOMMember *ins)
{
    // It's simple to duplicate visitLoadFixedSlotV here than it is to try to
    // use an LLoadFixedSlotV or some subclass of it for this case: that would
    // require us to have MGetDOMMember inherit from MLoadFixedSlot, and then
    // we'd have to duplicate a bunch of stuff we now get for free from
    // MGetDOMProperty.
    Register object = ToRegister(ins->object());
    size_t slot = ins->mir()->domMemberSlotIndex();
    ValueOperand result = GetValueOutput(ins);

    masm.loadValue(Address(object, JSObject::getFixedSlotOffset(slot)), result);
    return true;
}

bool
CodeGenerator::visitSetDOMProperty(LSetDOMProperty *ins)
{
    const Register JSContextReg = ToRegister(ins->getJSContextReg());
    const Register ObjectReg = ToRegister(ins->getObjectReg());
    const Register PrivateReg = ToRegister(ins->getPrivReg());
    const Register ValueReg = ToRegister(ins->getValueReg());

    DebugOnly<uint32_t> initialStack = masm.framePushed();

    masm.checkStackAlignment();

    // Push the argument. Rooting will happen at GC time.
    ValueOperand argVal = ToValue(ins, LSetDOMProperty::Value);
    masm.Push(argVal);
    // We pass the pointer to our out param as an instance of
    // JSJitGetterCallArgs, since on the binary level it's the same thing.
    JS_STATIC_ASSERT(sizeof(JSJitSetterCallArgs) == sizeof(Value*));
    masm.movePtr(StackPointer, ValueReg);

    masm.Push(ObjectReg);

    // GetReservedSlot(obj, DOM_OBJECT_SLOT).toPrivate()
    masm.loadPrivate(Address(ObjectReg, JSObject::getFixedSlotOffset(0)), PrivateReg);

    // Rooting will happen at GC time.
    masm.movePtr(StackPointer, ObjectReg);

    uint32_t safepointOffset;
    if (!masm.buildFakeExitFrame(JSContextReg, &safepointOffset))
        return false;
    masm.enterFakeExitFrame(ION_FRAME_DOMSETTER);

    if (!markSafepointAt(safepointOffset, ins))
        return false;

    masm.setupUnalignedABICall(4, JSContextReg);

    masm.loadJSContext(JSContextReg);

    masm.passABIArg(JSContextReg);
    masm.passABIArg(ObjectReg);
    masm.passABIArg(PrivateReg);
    masm.passABIArg(ValueReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ins->mir()->fun()));

    masm.branchIfFalseBool(ReturnReg, masm.exceptionLabel());

    masm.adjustStack(IonDOMExitFrameLayout::Size());

    JS_ASSERT(masm.framePushed() == initialStack);
    return true;
}

typedef bool(*SPSFn)(JSContext *, HandleScript);
static const VMFunction SPSEnterInfo = FunctionInfo<SPSFn>(SPSEnter);
static const VMFunction SPSExitInfo = FunctionInfo<SPSFn>(SPSExit);

bool
CodeGenerator::visitProfilerStackOp(LProfilerStackOp *lir)
{
    Register temp = ToRegister(lir->temp()->output());
    bool inlinedFunction = lir->inlineLevel() > 0;

    switch (lir->type()) {
        case MProfilerStackOp::InlineEnter:
            // Multiple scripts can be inlined at one depth, but there is only
            // one InlineExit node to signify this. To deal with this, if we
            // reach the entry of another inline script on the same level, then
            // just reset the sps metadata about the frame. We must balance
            // calls to leave()/reenter(), so perform the balance without
            // emitting any instrumentation. Technically the previous inline
            // call at this same depth has reentered, but the instrumentation
            // will be emitted at the common join point for all inlines at the
            // same depth.
            if (sps_.inliningDepth() == lir->inlineLevel()) {
                sps_.leaveInlineFrame();
                sps_.skipNextReenter();
                sps_.reenter(masm, temp);
            }

            sps_.leave(masm, temp, /* inlinedFunction = */ true);
            if (!sps_.enterInlineFrame())
                return false;
            // fallthrough

        case MProfilerStackOp::Enter:
            if (gen->options.spsSlowAssertionsEnabled()) {
                if (!inlinedFunction || js_JitOptions.profileInlineFrames) {
                    saveLive(lir);
                    pushArg(ImmGCPtr(lir->script()));
                    if (!callVM(SPSEnterInfo, lir))
                        return false;
                    restoreLive(lir);
                }
                sps_.pushManual(lir->script(), masm, temp, /* inlinedFunction = */ inlinedFunction);
                return true;
            }

            return sps_.push(lir->script(), masm, temp, /* inlinedFunction = */ inlinedFunction);

        case MProfilerStackOp::InlineExit:
            // all inline returns were covered with ::Exit, so we just need to
            // maintain the state of inline frames currently active and then
            // reenter the caller
            sps_.leaveInlineFrame();
            sps_.reenter(masm, temp, /* inlinedFunction = */ true);
            return true;

        case MProfilerStackOp::Exit:
            if (gen->options.spsSlowAssertionsEnabled()) {
                if (!inlinedFunction || js_JitOptions.profileInlineFrames) {
                    saveLive(lir);
                    pushArg(ImmGCPtr(lir->script()));
                    // Once we've exited, then we shouldn't emit instrumentation for
                    // the corresponding reenter() because we no longer have a
                    // frame.
                    sps_.skipNextReenter();
                    if (!callVM(SPSExitInfo, lir))
                        return false;
                    restoreLive(lir);
                }
                return true;
            }

            sps_.pop(masm, temp, /* inlinedFunction = */ inlinedFunction);
            return true;

        default:
            MOZ_ASSUME_UNREACHABLE("invalid LProfilerStackOp type");
    }
}

bool
CodeGenerator::visitOutOfLineAbortPar(OutOfLineAbortPar *ool)
{
    ParallelBailoutCause cause = ool->cause();
    jsbytecode *bytecode = ool->bytecode();

    masm.move32(Imm32(cause), CallTempReg0);
    loadOutermostJSScript(CallTempReg1);
    loadJSScriptForBlock(ool->basicBlock(), CallTempReg2);
    masm.movePtr(ImmPtr(bytecode), CallTempReg3);

    masm.setupUnalignedABICall(4, CallTempReg4);
    masm.passABIArg(CallTempReg0);
    masm.passABIArg(CallTempReg1);
    masm.passABIArg(CallTempReg2);
    masm.passABIArg(CallTempReg3);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, AbortPar));

    masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    masm.jump(&returnLabel_);
    return true;
}

bool
CodeGenerator::visitIsCallable(LIsCallable *ins)
{
    Register object = ToRegister(ins->object());
    Register output = ToRegister(ins->output());

    masm.loadObjClass(object, output);

    // An object is callable iff (is<JSFunction>() || getClass()->call).
    Label notFunction, done, notCall;
    masm.branchPtr(Assembler::NotEqual, output, ImmPtr(&JSFunction::class_), &notFunction);
    masm.move32(Imm32(1), output);
    masm.jump(&done);

    masm.bind(&notFunction);
    masm.cmpPtrSet(Assembler::NonZero, Address(output, offsetof(js::Class, call)), ImmPtr(nullptr), output);
    masm.bind(&done);

    return true;
}

void
CodeGenerator::loadOutermostJSScript(Register reg)
{
    // The "outermost" JSScript means the script that we are compiling
    // basically; this is not always the script associated with the
    // current basic block, which might be an inlined script.

    MIRGraph &graph = current->mir()->graph();
    MBasicBlock *entryBlock = graph.entryBlock();
    masm.movePtr(ImmGCPtr(entryBlock->info().script()), reg);
}

void
CodeGenerator::loadJSScriptForBlock(MBasicBlock *block, Register reg)
{
    // The current JSScript means the script for the current
    // basic block. This may be an inlined script.

    JSScript *script = block->info().script();
    masm.movePtr(ImmGCPtr(script), reg);
}

bool
CodeGenerator::visitOutOfLinePropagateAbortPar(OutOfLinePropagateAbortPar *ool)
{
    loadOutermostJSScript(CallTempReg0);
    loadJSScriptForBlock(ool->lir()->mirRaw()->block(), CallTempReg1);

    masm.setupUnalignedABICall(2, CallTempReg2);
    masm.passABIArg(CallTempReg0);
    masm.passABIArg(CallTempReg1);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, PropagateAbortPar));

    masm.moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    masm.jump(&returnLabel_);
    return true;
}

bool
CodeGenerator::visitHaveSameClass(LHaveSameClass *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register rhs = ToRegister(ins->rhs());
    Register temp = ToRegister(ins->getTemp(0));
    Register output = ToRegister(ins->output());

    masm.loadObjClass(lhs, temp);
    masm.loadObjClass(rhs, output);
    masm.cmpPtrSet(Assembler::Equal, temp, output, output);

    return true;
}

bool
CodeGenerator::visitHasClass(LHasClass *ins)
{
    Register lhs = ToRegister(ins->lhs());
    Register output = ToRegister(ins->output());

    masm.loadObjClass(lhs, output);
    masm.cmpPtrSet(Assembler::Equal, output, ImmPtr(ins->mir()->getClass()), output);

    return true;
}

bool
CodeGenerator::visitAsmJSCall(LAsmJSCall *ins)
{
    MAsmJSCall *mir = ins->mir();

#if defined(JS_CODEGEN_ARM)
    if (!useHardFpABI() && mir->callee().which() == MAsmJSCall::Callee::Builtin) {
        for (unsigned i = 0, e = ins->numOperands(); i < e; i++) {
            LAllocation *a = ins->getOperand(i);
            if (a->isFloatReg()) {
                FloatRegister fr = ToFloatRegister(a);
                int srcId = fr.code() * 2;
                masm.ma_vxfer(fr, Register::FromCode(srcId), Register::FromCode(srcId+1));
            }
        }
    }
#endif
   if (mir->spIncrement())
        masm.freeStack(mir->spIncrement());

   JS_ASSERT((AlignmentAtPrologue +  masm.framePushed()) % StackAlignment == 0);

#ifdef DEBUG
    Label ok;
    JS_ASSERT(IsPowerOfTwo(StackAlignment));
    masm.branchTestPtr(Assembler::Zero, StackPointer, Imm32(StackAlignment - 1), &ok);
    masm.assumeUnreachable("Stack should be aligned.");
    masm.bind(&ok);
#endif

    MAsmJSCall::Callee callee = mir->callee();
    switch (callee.which()) {
      case MAsmJSCall::Callee::Internal:
        masm.call(callee.internal());
        break;
      case MAsmJSCall::Callee::Dynamic:
        masm.call(ToRegister(ins->getOperand(mir->dynamicCalleeOperandIndex())));
        break;
      case MAsmJSCall::Callee::Builtin:
        masm.call(callee.builtin());
        break;
    }

    if (mir->spIncrement())
        masm.reserveStack(mir->spIncrement());

    postAsmJSCall(ins);
    return true;
}

bool
CodeGenerator::visitAsmJSParameter(LAsmJSParameter *lir)
{
    return true;
}

bool
CodeGenerator::visitAsmJSReturn(LAsmJSReturn *lir)
{
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir() != *gen->graph().poBegin())
        masm.jump(&returnLabel_);
    return true;
}

bool
CodeGenerator::visitAsmJSVoidReturn(LAsmJSVoidReturn *lir)
{
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir() != *gen->graph().poBegin())
        masm.jump(&returnLabel_);
    return true;
}

bool
CodeGenerator::visitAsmJSCheckOverRecursed(LAsmJSCheckOverRecursed *lir)
{
    masm.branchPtr(Assembler::AboveOrEqual,
                   AsmJSAbsoluteAddress(AsmJSImm_StackLimit),
                   StackPointer,
                   lir->mir()->onError());
    return true;
}

bool
CodeGenerator::emitAssertRangeI(const Range *r, Register input)
{
    // Check the lower bound.
    if (r->hasInt32LowerBound() && r->lower() > INT32_MIN) {
        Label success;
        masm.branch32(Assembler::GreaterThanOrEqual, input, Imm32(r->lower()), &success);
        masm.assumeUnreachable("Integer input should be equal or higher than Lowerbound.");
        masm.bind(&success);
    }

    // Check the upper bound.
    if (r->hasInt32UpperBound() && r->upper() < INT32_MAX) {
        Label success;
        masm.branch32(Assembler::LessThanOrEqual, input, Imm32(r->upper()), &success);
        masm.assumeUnreachable("Integer input should be lower or equal than Upperbound.");
        masm.bind(&success);
    }

    // For r->canHaveFractionalPart() and r->exponent(), there's nothing to check, because
    // if we ended up in the integer range checking code, the value is already
    // in an integer register in the integer range.

    return true;
}

bool
CodeGenerator::emitAssertRangeD(const Range *r, FloatRegister input, FloatRegister temp)
{
    // Check the lower bound.
    if (r->hasInt32LowerBound()) {
        Label success;
        masm.loadConstantDouble(r->lower(), temp);
        if (r->canBeNaN())
            masm.branchDouble(Assembler::DoubleUnordered, input, input, &success);
        masm.branchDouble(Assembler::DoubleGreaterThanOrEqual, input, temp, &success);
        masm.assumeUnreachable("Double input should be equal or higher than Lowerbound.");
        masm.bind(&success);
    }
    // Check the upper bound.
    if (r->hasInt32UpperBound()) {
        Label success;
        masm.loadConstantDouble(r->upper(), temp);
        if (r->canBeNaN())
            masm.branchDouble(Assembler::DoubleUnordered, input, input, &success);
        masm.branchDouble(Assembler::DoubleLessThanOrEqual, input, temp, &success);
        masm.assumeUnreachable("Double input should be lower or equal than Upperbound.");
        masm.bind(&success);
    }

    // This code does not yet check r->canHaveFractionalPart(). This would require new
    // assembler interfaces to make rounding instructions available.

    if (!r->hasInt32Bounds() && !r->canBeInfiniteOrNaN() &&
        r->exponent() < FloatingPoint<double>::ExponentBias)
    {
        // Check the bounds implied by the maximum exponent.
        Label exponentLoOk;
        masm.loadConstantDouble(pow(2.0, r->exponent() + 1), temp);
        masm.branchDouble(Assembler::DoubleUnordered, input, input, &exponentLoOk);
        masm.branchDouble(Assembler::DoubleLessThanOrEqual, input, temp, &exponentLoOk);
        masm.assumeUnreachable("Check for exponent failed.");
        masm.bind(&exponentLoOk);

        Label exponentHiOk;
        masm.loadConstantDouble(-pow(2.0, r->exponent() + 1), temp);
        masm.branchDouble(Assembler::DoubleUnordered, input, input, &exponentHiOk);
        masm.branchDouble(Assembler::DoubleGreaterThanOrEqual, input, temp, &exponentHiOk);
        masm.assumeUnreachable("Check for exponent failed.");
        masm.bind(&exponentHiOk);
    } else if (!r->hasInt32Bounds() && !r->canBeNaN()) {
        // If we think the value can't be NaN, check that it isn't.
        Label notnan;
        masm.branchDouble(Assembler::DoubleOrdered, input, input, &notnan);
        masm.assumeUnreachable("Input shouldn't be NaN.");
        masm.bind(&notnan);

        // If we think the value also can't be an infinity, check that it isn't.
        if (!r->canBeInfiniteOrNaN()) {
            Label notposinf;
            masm.loadConstantDouble(PositiveInfinity<double>(), temp);
            masm.branchDouble(Assembler::DoubleLessThan, input, temp, &notposinf);
            masm.assumeUnreachable("Input shouldn't be +Inf.");
            masm.bind(&notposinf);

            Label notneginf;
            masm.loadConstantDouble(NegativeInfinity<double>(), temp);
            masm.branchDouble(Assembler::DoubleGreaterThan, input, temp, &notneginf);
            masm.assumeUnreachable("Input shouldn't be -Inf.");
            masm.bind(&notneginf);
        }
    }

    return true;
}

bool
CodeGenerator::visitAssertRangeI(LAssertRangeI *ins)
{
    Register input = ToRegister(ins->input());
    const Range *r = ins->range();

    return emitAssertRangeI(r, input);
}

bool
CodeGenerator::visitAssertRangeD(LAssertRangeD *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister temp = ToFloatRegister(ins->temp());
    const Range *r = ins->range();

    return emitAssertRangeD(r, input, temp);
}

bool
CodeGenerator::visitAssertRangeF(LAssertRangeF *ins)
{
    FloatRegister input = ToFloatRegister(ins->input());
    FloatRegister temp = ToFloatRegister(ins->temp());
    const Range *r = ins->range();

    masm.convertFloat32ToDouble(input, input);
    bool success = emitAssertRangeD(r, input, temp);
    masm.convertDoubleToFloat32(input, input);
    return success;
}

bool
CodeGenerator::visitAssertRangeV(LAssertRangeV *ins)
{
    const Range *r = ins->range();
    const ValueOperand value = ToValue(ins, LAssertRangeV::Input);
    Register tag = masm.splitTagForTest(value);
    Label done;

    {
        Label isNotInt32;
        masm.branchTestInt32(Assembler::NotEqual, tag, &isNotInt32);
        Register unboxInt32 = ToTempUnboxRegister(ins->temp());
        Register input = masm.extractInt32(value, unboxInt32);
        emitAssertRangeI(r, input);
        masm.jump(&done);
        masm.bind(&isNotInt32);
    }

    {
        Label isNotDouble;
        masm.branchTestDouble(Assembler::NotEqual, tag, &isNotDouble);
        FloatRegister input = ToFloatRegister(ins->floatTemp1());
        FloatRegister temp = ToFloatRegister(ins->floatTemp2());
        masm.unboxDouble(value, input);
        emitAssertRangeD(r, input, temp);
        masm.jump(&done);
        masm.bind(&isNotDouble);
    }

    masm.assumeUnreachable("Incorrect range for Value.");
    masm.bind(&done);
    return true;
}

typedef bool (*RecompileFn)(JSContext *);
static const VMFunction RecompileFnInfo = FunctionInfo<RecompileFn>(Recompile);

bool
CodeGenerator::visitRecompileCheck(LRecompileCheck *ins)
{
    Label done;
    Register tmp = ToRegister(ins->scratch());
    OutOfLineCode *ool = oolCallVM(RecompileFnInfo, ins, (ArgList()), StoreRegisterTo(tmp));

    // Check if usecount is high enough.
    masm.movePtr(ImmPtr(ins->mir()->script()->addressOfUseCount()), tmp);
    Address ptr(tmp, 0);
    masm.add32(Imm32(1), tmp);
    masm.branch32(Assembler::BelowOrEqual, ptr, Imm32(ins->mir()->useCount()), &done);

    // Check if not yet recompiling.
    CodeOffsetLabel label = masm.movWithPatch(ImmWord(uintptr_t(-1)), tmp);
    if (!ionScriptLabels_.append(label))
        return false;
    masm.branch32(Assembler::Equal,
                  Address(tmp, IonScript::offsetOfRecompiling()),
                  Imm32(0),
                  ool->entry());
    masm.bind(ool->rejoin());
    masm.bind(&done);

    return true;
}

} // namespace jit
} // namespace js
