/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/CodeGenerator.h"

#include "jslibmath.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Util.h"

#include "jsmath.h"
#include "jsnum.h"

#include "builtin/Eval.h"
#include "gc/Nursery.h"
#include "jit/ExecutionModeInlines.h"
#include "jit/IonLinker.h"
#include "jit/IonSpewer.h"
#include "jit/MIRGenerator.h"
#include "jit/MoveEmitter.h"
#include "jit/ParallelFunctions.h"
#include "jit/ParallelSafetyAnalysis.h"
#include "jit/PerfSpewer.h"
#include "vm/ForkJoin.h"

#include "jsboolinlines.h"

#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/Interpreter-inl.h"

using namespace js;
using namespace js::ion;

using mozilla::DebugOnly;
using mozilla::Maybe;

namespace js {
namespace ion {

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
#define VISIT_CACHE_FUNCTION(op)                                \
    bool visit##op##IC(CodeGenerator *codegen, op##IC *ic) {    \
        return codegen->visit##op##IC(this, ic);                \
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
    IonCache *cache = static_cast<IonCache *>(getCache(cacheIndex));
    MInstruction *mir = lir->mirRaw()->toInstruction();
    if (mir->resumePoint())
        cache->setScriptedLocation(mir->block()->info().script(),
                                   mir->resumePoint()->pc());
    else
        cache->setIdempotent();

    OutOfLineUpdateCache *ool = new OutOfLineUpdateCache(lir, cacheIndex);
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
    size_t cacheIndex = ool->getCacheIndex();
    IonCache *cache = static_cast<IonCache *>(getCache(cacheIndex));

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
  : CodeGeneratorSpecific(gen, graph, masm),
    unassociatedScriptCounts_(NULL)
{
}

CodeGenerator::~CodeGenerator()
{
    js_delete(unassociatedScriptCounts_);
}

typedef bool (*StringToNumberFn)(ThreadSafeContext *, JSString *, double *);
typedef ParallelResult (*StringToNumberParFn)(ForkJoinSlice *, JSString *, double *);
static const VMFunctionsModal StringToNumberInfo = VMFunctionsModal(
    FunctionInfo<StringToNumberFn>(StringToNumber),
    FunctionInfo<StringToNumberParFn>(StringToNumberPar));

bool
CodeGenerator::visitValueToInt32(LValueToInt32 *lir)
{
    ValueOperand operand = ToValue(lir, LValueToInt32::Input);
    Register output = ToRegister(lir->output());

    Register tag = masm.splitTagForTest(operand);

    Label done, simple, isInt32, isBool, isString, notDouble;
    // Type-check switch.
    MDefinition *input;
    if (lir->mode() == LValueToInt32::NORMAL)
        input = lir->mirNormal()->input();
    else
        input = lir->mirTruncate()->input();
    masm.branchEqualTypeIfNeeded(MIRType_Int32, input, tag, &isInt32);
    masm.branchEqualTypeIfNeeded(MIRType_Boolean, input, tag, &isBool);
    // Only convert strings to int if we are in a truncation context, like
    // bitwise operations.
    if (lir->mode() == LValueToInt32::TRUNCATE)
        masm.branchEqualTypeIfNeeded(MIRType_String, input, tag, &isString);
    masm.branchTestDouble(Assembler::NotEqual, tag, &notDouble);

    // If the value is a double, see if it fits in a 32-bit int. We need to ask
    // the platform-specific codegenerator to do this.
    FloatRegister temp = ToFloatRegister(lir->tempFloat());
    masm.unboxDouble(operand, temp);

    Label fails, isDouble;
    masm.bind(&isDouble);
    if (lir->mode() == LValueToInt32::TRUNCATE) {
        if (!emitTruncateDouble(temp, output))
            return false;
    } else {
        masm.convertDoubleToInt32(temp, output, &fails, lir->mirNormal()->canBeNegativeZero());
    }
    masm.jump(&done);

    masm.bind(&notDouble);

    if (lir->mode() == LValueToInt32::NORMAL) {
        // If the value is not null, it's a string, object, or undefined,
        // which we can't handle here.
        masm.branchTestNull(Assembler::NotEqual, tag, &fails);
    } else {
        // Test for object - then fallthrough to null, which will also handle
        // undefined.
        masm.branchEqualTypeIfNeeded(MIRType_Object, input, tag, &fails);
    }

    if (fails.used() && !bailoutFrom(&fails, lir->snapshot()))
        return false;

    // The value is null - just emit 0.
    masm.mov(Imm32(0), output);
    masm.jump(&done);

    // Unbox a string, call StringToNumber to get a double back, and jump back
    // to the snippet generated above about dealing with doubles.
    if (isString.used()) {
        masm.bind(&isString);
        Register str = masm.extractString(operand, ToRegister(lir->temp()));
        OutOfLineCode *ool = oolCallVM(StringToNumberInfo, lir, (ArgList(), str),
                                       StoreFloatRegisterTo(temp));
        if (!ool)
            return false;

        masm.jump(ool->entry());
        masm.bind(ool->rejoin());
        masm.jump(&isDouble);
    }

    // Just unbox a bool, the result is 0 or 1.
    if (isBool.used()) {
        masm.bind(&isBool);
        masm.unboxBoolean(operand, output);
        masm.jump(&done);
    }

    // Integers can be unboxed.
    if (isInt32.used()) {
        masm.bind(&isInt32);
        masm.unboxInt32(operand, output);
    }

    masm.bind(&done);

    return true;
}

static const double DoubleZero = 0.0;

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
        masm.loadStaticDouble(&DoubleZero, output);
        masm.jump(&done);
    }

    if (hasUndefined) {
        masm.bind(&isUndefined);
        masm.loadStaticDouble(&js_NaN, output);
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
CodeGenerator::visitInt32ToDouble(LInt32ToDouble *lir)
{
    masm.convertInt32ToDouble(ToRegister(lir->input()), ToFloatRegister(lir->output()));
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

void
CodeGenerator::emitOOLTestObject(Register objreg, Label *ifTruthy, Label *ifFalsy, Register scratch)
{
    saveVolatile(scratch);
    masm.setupUnalignedABICall(1, scratch);
    masm.passABIArg(objreg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::EmulatesUndefined));
    masm.storeCallResult(scratch);
    restoreVolatile(scratch);

    masm.branchIfTrueBool(scratch, ifFalsy);
    masm.jump(ifTruthy);
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

    Label *ifTruthy_;
    Label *ifFalsy_;

#ifdef DEBUG
    bool initialized() { return ifTruthy_ != NULL; }
#endif

  public:
    OutOfLineTestObject()
#ifdef DEBUG
      : ifTruthy_(NULL), ifFalsy_(NULL)
#endif
    { }

    bool accept(CodeGenerator *codegen) MOZ_FINAL MOZ_OVERRIDE {
        MOZ_ASSERT(initialized());
        codegen->emitOOLTestObject(objreg_, ifTruthy_, ifFalsy_, scratch_);
        return true;
    }

    // Specify the register where the object to be tested is found, labels to
    // jump to if the object is truthy or falsy, and a scratch register for
    // use in the out-of-line path.
    void setInputAndTargets(Register objreg, Label *ifTruthy, Label *ifFalsy, Register scratch) {
        MOZ_ASSERT(!initialized());
        MOZ_ASSERT(ifTruthy);
        objreg_ = objreg;
        scratch_ = scratch;
        ifTruthy_ = ifTruthy;
        ifFalsy_ = ifFalsy;
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
CodeGenerator::testObjectTruthy(Register objreg, Label *ifTruthy, Label *ifFalsy, Register scratch,
                                OutOfLineTestObject *ool)
{
    ool->setInputAndTargets(objreg, ifTruthy, ifFalsy, scratch);

    // Perform a fast-path check of the object's class flags if the object's
    // not a proxy.  Let out-of-line code handle the slow cases that require
    // saving registers, making a function call, and restoring registers.
    Assembler::Condition cond = masm.branchTestObjectTruthy(true, objreg, scratch, ool->entry());
    masm.j(cond, ifTruthy);
    masm.jump(ifFalsy);
}

void
CodeGenerator::testValueTruthy(const ValueOperand &value,
                               const LDefinition *scratch1, const LDefinition *scratch2,
                               FloatRegister fr,
                               Label *ifTruthy, Label *ifFalsy,
                               OutOfLineTestObject *ool)
{
    Register tag = masm.splitTagForTest(value);
    Assembler::Condition cond;

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
    cond = masm.testInt32Truthy(false, value);
    masm.j(cond, ifFalsy);
    masm.jump(ifTruthy);
    masm.bind(&notInt32);

    if (ool) {
        Label notObject;

        masm.branchTestObject(Assembler::NotEqual, tag, &notObject);

        Register objreg = masm.extractObject(value, ToRegister(scratch1));
        testObjectTruthy(objreg, ifTruthy, ifFalsy, ToRegister(scratch2), ool);

        masm.bind(&notObject);
    } else {
        masm.branchTestObject(Assembler::Equal, tag, ifTruthy);
    }

    // Test if a string is non-empty.
    Label notString;
    masm.branchTestString(Assembler::NotEqual, tag, &notString);
    cond = masm.testStringTruthy(false, value);
    masm.j(cond, ifFalsy);
    masm.jump(ifTruthy);
    masm.bind(&notString);

    // If we reach here the value is a double.
    masm.unboxDouble(value, fr);
    cond = masm.testDoubleTruthy(false, fr);
    masm.j(cond, ifFalsy);
    masm.jump(ifTruthy);
}

bool
CodeGenerator::visitTestOAndBranch(LTestOAndBranch *lir)
{
    MOZ_ASSERT(lir->mir()->operandMightEmulateUndefined(),
               "Objects which can't emulate undefined should have been constant-folded");

    OutOfLineTestObject *ool = new OutOfLineTestObject();
    if (!addOutOfLineCode(ool))
        return false;

    testObjectTruthy(ToRegister(lir->input()), lir->ifTruthy(), lir->ifFalsy(),
                     ToRegister(lir->temp()), ool);
    return true;

}

bool
CodeGenerator::visitTestVAndBranch(LTestVAndBranch *lir)
{
    OutOfLineTestObject *ool = NULL;
    if (lir->mir()->operandMightEmulateUndefined()) {
        ool = new OutOfLineTestObject();
        if (!addOutOfLineCode(ool))
            return false;
    }

    testValueTruthy(ToValue(lir, LTestVAndBranch::Input),
                    lir->temp1(), lir->temp2(),
                    ToFloatRegister(lir->tempFloat()),
                    lir->ifTruthy(), lir->ifFalsy(), ool);
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
CodeGenerator::visitPolyInlineDispatch(LPolyInlineDispatch *lir)
{
    MPolyInlineDispatch *mir = lir->mir();
    Register inputReg = ToRegister(lir->input());

    InlinePropertyTable *inlinePropTable = mir->propTable();
    if (inlinePropTable) {
        // Temporary register is only assigned in the TypeObject case.
        Register tempReg = ToRegister(lir->temp());
        masm.loadPtr(Address(inputReg, JSObject::offsetOfType()), tempReg);

        // Detect functions by TypeObject.
        for (size_t i = 0; i < inlinePropTable->numEntries(); i++) {
            types::TypeObject *typeObj = inlinePropTable->getTypeObject(i);
            JSFunction *func = inlinePropTable->getFunction(i);
            LBlock *target = mir->getFunctionBlock(func)->lir();
            masm.branchPtr(Assembler::Equal, tempReg, ImmGCPtr(typeObj), target->label());
        }

        // Unknown function: jump to fallback block.
        LBlock *fallback = mir->fallbackPrepBlock()->lir();
        masm.jump(fallback->label());
        return true;
    }

    // Compare function pointers directly.
    for (size_t i = 0; i < mir->numCallees() - 1; i++) {
        JSFunction *func = mir->getFunction(i);
        LBlock *target = mir->getFunctionBlock(i)->lir();
        masm.branchPtr(Assembler::Equal, inputReg, ImmGCPtr(func), target->label());
    }

    // There's no fallback case, so a final guard isn't necessary.
    LBlock *target = mir->getFunctionBlock(mir->numCallees() - 1)->lir();
    masm.jump(target->label());
    return true;
}

typedef JSFlatString *(*IntToStringFn)(ThreadSafeContext *, int);
typedef ParallelResult (*IntToStringParFn)(ForkJoinSlice *, int, MutableHandleString);
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

    masm.branch32(Assembler::AboveOrEqual, input, Imm32(StaticStrings::INT_STATIC_LIMIT),
                  ool->entry());

    masm.movePtr(ImmWord(&GetIonContext()->runtime->staticStrings.intStaticTable), output);
    masm.loadPtr(BaseIndex(output, input, ScalePointer), output);

    masm.bind(ool->rejoin());
    return true;
}

typedef JSString *(*DoubleToStringFn)(ThreadSafeContext *, double);
typedef ParallelResult (*DoubleToStringParFn)(ForkJoinSlice *, double, MutableHandleString);
static const VMFunctionsModal DoubleToStringInfo = VMFunctionsModal(
    FunctionInfo<DoubleToStringFn>(js_NumberToString<CanGC>),
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

    masm.convertDoubleToInt32(input, temp, ool->entry(), true);
    masm.branch32(Assembler::AboveOrEqual, temp, Imm32(StaticStrings::INT_STATIC_LIMIT),
                  ool->entry());

    masm.movePtr(ImmWord(&GetIonContext()->runtime->staticStrings.intStaticTable), output);
    masm.loadPtr(BaseIndex(output, temp, ScalePointer), output);

    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*CloneRegExpObjectFn)(JSContext *, JSObject *, JSObject *);
static const VMFunction CloneRegExpObjectInfo =
    FunctionInfo<CloneRegExpObjectFn>(CloneRegExpObject);

bool
CodeGenerator::visitRegExp(LRegExp *lir)
{
    JSObject *proto = lir->mir()->getRegExpPrototype();

    pushArg(ImmGCPtr(proto));
    pushArg(ImmGCPtr(lir->mir()->source()));
    return callVM(CloneRegExpObjectInfo, lir);
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

typedef JSObject *(*LambdaFn)(JSContext *, HandleFunction, HandleObject);
static const VMFunction LambdaInfo =
    FunctionInfo<LambdaFn>(js::Lambda);

bool
CodeGenerator::visitLambdaForSingleton(LLambdaForSingleton *lir)
{
    pushArg(ToRegister(lir->scopeChain()));
    pushArg(ImmGCPtr(lir->mir()->fun()));
    return callVM(LambdaInfo, lir);
}

bool
CodeGenerator::visitLambda(LLambda *lir)
{
    Register scopeChain = ToRegister(lir->scopeChain());
    Register output = ToRegister(lir->output());
    JSFunction *fun = lir->mir()->fun();

    OutOfLineCode *ool = oolCallVM(LambdaInfo, lir, (ArgList(), ImmGCPtr(fun), scopeChain),
                                   StoreRegisterTo(output));
    if (!ool)
        return false;

    JS_ASSERT(gen->compartment == fun->compartment());
    JS_ASSERT(!fun->hasSingletonType());

    masm.newGCThing(output, fun, ool->entry());
    masm.initGCThing(output, fun);

    emitLambdaInit(output, scopeChain, fun);

    masm.bind(ool->rejoin());
    return true;
}

void
CodeGenerator::emitLambdaInit(const Register &output, const Register &scopeChain, JSFunction *fun)
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
    u.s.nargs = fun->nargs;
    u.s.flags = fun->flags & ~JSFunction::EXTENDED;

    JS_STATIC_ASSERT(offsetof(JSFunction, flags) == offsetof(JSFunction, nargs) + 2);
    masm.store32(Imm32(u.word), Address(output, offsetof(JSFunction, nargs)));

    // Initialize the JSFunction union with the script / native / lazyScript
    // corresponding to the flag.
    ImmGCPtr scriptOrLazyScript(NULL);
    if (fun->isInterpretedLazy())
        scriptOrLazyScript = ImmGCPtr(fun->lazyScriptOrNull());
    else
        scriptOrLazyScript = ImmGCPtr(fun->nonLazyScript());

    masm.storePtr(scriptOrLazyScript, Address(output, JSFunction::offsetOfNativeOrScript()));

    // Lambda scripted functions are given the scope chain as the environment in
    // which the caller was at the time of the creation of the lambda.
    masm.storePtr(scopeChain, Address(output, JSFunction::offsetOfEnvironment()));

    // Copy the name of the function used for diagnostics.
    masm.storePtr(ImmGCPtr(fun->displayAtom()), Address(output, JSFunction::offsetOfAtom()));
}

bool
CodeGenerator::visitLambdaPar(LLambdaPar *lir)
{
    Register resultReg = ToRegister(lir->output());
    Register sliceReg = ToRegister(lir->forkJoinSlice());
    Register scopeChainReg = ToRegister(lir->scopeChain());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    JSFunction *fun = lir->mir()->fun();

    JS_ASSERT(scopeChainReg != resultReg);

    emitAllocateGCThingPar(lir, resultReg, sliceReg, tempReg1, tempReg2, fun);
    emitLambdaInit(resultReg, scopeChainReg, fun);
    return true;
}

bool
CodeGenerator::visitLabel(LLabel *lir)
{
    masm.bind(lir->label());
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
    return true;
}

bool
CodeGenerator::visitGoto(LGoto *lir)
{
    LBlock *target = lir->target()->lir();

    // No jump necessary if we can fall through to the next block.
    if (isNextBlock(target))
        return true;

    masm.jump(target->label());
    return true;
}

bool
CodeGenerator::visitTableSwitch(LTableSwitch *ins)
{
    MTableSwitch *mir = ins->mir();
    Label *defaultcase = mir->getDefault()->lir()->label();
    const LAllocation *temp;

    if (ins->index()->isDouble()) {
        temp = ins->tempInt();

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
    masm.clearCalleeTag(callee, gen->info().executionMode());
    return true;
}

bool
CodeGenerator::visitForceUseV(LForceUseV *lir)
{
    return true;
}

bool
CodeGenerator::visitForceUseT(LForceUseT *lir)
{
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

    const ptrdiff_t frameOffset = StackFrame::offsetOfScopeChain();

    masm.loadPtr(Address(ToRegister(frame), frameOffset), ToRegister(object));
    return true;
}

bool
CodeGenerator::visitStackArgT(LStackArgT *lir)
{
    const LAllocation *arg = lir->getArgument();
    MIRType argType = lir->mir()->getArgument()->type();
    uint32_t argslot = lir->argslot();

    int32_t stack_offset = StackOffsetOfPassedArg(argslot);
    Address dest(StackPointer, stack_offset);

    if (arg->isFloatReg())
        masm.storeDouble(ToFloatRegister(arg), dest);
    else if (arg->isRegister())
        masm.storeValue(ValueTypeFromMIRType(argType), ToRegister(arg), dest);
    else
        masm.storeValue(*(arg->toConstant()), dest);

    return pushedArgumentSlots_.append(StackOffsetToSlot(stack_offset));
}

bool
CodeGenerator::visitStackArgV(LStackArgV *lir)
{
    ValueOperand val = ToValue(lir, 0);
    uint32_t argslot = lir->argslot();
    int32_t stack_offset = StackOffsetOfPassedArg(argslot);

    masm.storeValue(val, Address(StackPointer, stack_offset));
    return pushedArgumentSlots_.append(StackOffsetToSlot(stack_offset));
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

        // No bogus moves.
        JS_ASSERT(*from != *to);
        JS_ASSERT(!from->isConstant());
        JS_ASSERT(from->isDouble() == to->isDouble());

        MoveResolver::Move::Kind kind = from->isDouble()
                                        ? MoveResolver::Move::DOUBLE
                                        : MoveResolver::Move::GENERAL;

        if (!resolver.addMove(toMoveOperand(from), toMoveOperand(to), kind))
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
        masm.movePtr(ImmWord(lir->ptr()), ToRegister(lir->output()));
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
CodeGenerator::visitForkJoinSlice(LForkJoinSlice *lir)
{
    const Register tempReg = ToRegister(lir->getTempReg());

    masm.setupUnalignedABICall(0, tempReg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ForkJoinSlicePar));
    JS_ASSERT(ToRegister(lir->output()) == ReturnReg);
    return true;
}

bool
CodeGenerator::visitGuardThreadLocalObject(LGuardThreadLocalObject *lir)
{
    JS_ASSERT(gen->info().executionMode() == ParallelExecution);

    const Register tempReg = ToRegister(lir->getTempReg());
    masm.setupUnalignedABICall(2, tempReg);
    masm.passABIArg(ToRegister(lir->forkJoinSlice()));
    masm.passABIArg(ToRegister(lir->object()));
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, IsThreadLocalObject));

    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutIllegalWrite, lir);
    if (!bail)
        return false;

    // branch to the OOL failure code if false is returned
    masm.branchIfFalseBool(ReturnReg, bail->entry());
    return true;
}

bool
CodeGenerator::visitTypeBarrier(LTypeBarrier *lir)
{
    ValueOperand operand = ToValue(lir, LTypeBarrier::Input);
    Register scratch = ToTempUnboxRegister(lir->temp());

    Label matched, miss;
    masm.guardTypeSet(operand, lir->mir()->resultTypeSet(), scratch, &matched, &miss);
    masm.jump(&miss);
    if (!bailoutFrom(&miss, lir->snapshot()))
        return false;
    masm.bind(&matched);
    return true;
}

bool
CodeGenerator::visitMonitorTypes(LMonitorTypes *lir)
{
    ValueOperand operand = ToValue(lir, LMonitorTypes::Input);
    Register scratch = ToTempUnboxRegister(lir->temp());

    Label matched, miss;
    masm.guardTypeSet(operand, lir->mir()->typeSet(), scratch, &matched, &miss);
    masm.jump(&miss);
    if (!bailoutFrom(&miss, lir->snapshot()))
        return false;
    masm.bind(&matched);
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
    saveLive(ool->lir());

    const LAllocation *obj = ool->object();

    GeneralRegisterSet regs;
    regs.add(CallTempReg0);
    regs.add(CallTempReg1);
    regs.add(CallTempReg2);

    Register objreg;
    if (obj->isConstant()) {
        objreg = regs.takeAny();
        masm.movePtr(ImmGCPtr(&obj->toConstant()->toObject()), objreg);
    } else {
        objreg = ToRegister(obj);
        regs.takeUnchecked(objreg);
    }

    Register runtimereg = regs.takeAny();
    masm.mov(ImmWord(GetIonContext()->runtime), runtimereg);

    masm.setupUnalignedABICall(2, regs.takeAny());
    masm.passABIArg(runtimereg);
    masm.passABIArg(objreg);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, PostWriteBarrier));

    restoreLive(ool->lir());

    masm.jump(ool->rejoin());
    return true;
}
#endif

bool
CodeGenerator::visitPostWriteBarrierO(LPostWriteBarrierO *lir)
{
#ifdef JSGC_GENERATIONAL
    OutOfLineCallPostWriteBarrier *ool = new OutOfLineCallPostWriteBarrier(lir, lir->object());
    if (!addOutOfLineCode(ool))
        return false;

    Nursery &nursery = GetIonContext()->runtime->gcNursery;

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
    OutOfLineCallPostWriteBarrier *ool = new OutOfLineCallPostWriteBarrier(lir, lir->object());
    if (!addOutOfLineCode(ool))
        return false;

    ValueOperand value = ToValue(lir, LPostWriteBarrierV::Input);
    masm.branchTestObject(Assembler::NotEqual, value, ool->rejoin());

    Nursery &nursery = GetIonContext()->runtime->gcNursery;

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
    //  ParallelResult (*)(ForkJoinSlice *, unsigned, Value *vp)
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

        // Test for failure.
        masm.branchIfFalseBool(ReturnReg, masm.failureLabel(executionMode));
        break;

      case ParallelExecution:
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, target->parallelNative()));

        // ParallelResult has more nuanced failure, but for now we fail on
        // anything that's != TP_SUCCESS.
        masm.branch32(Assembler::NotEqual, ReturnReg, Imm32(TP_SUCCESS),
                      masm.failureLabel(executionMode));
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }

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
    JS_ASSERT(call->mir()->isDOMFunction());

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

typedef bool (*InvokeFunctionFn)(JSContext *, HandleFunction, uint32_t, Value *, Value *);
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
    Label uncompiled, thunk, makeCall, end;

    // Known-target case is handled by LCallKnown.
    JS_ASSERT(!call->hasSingleTarget());

    // Generate an ArgumentsRectifier.
    IonCode *argumentsRectifier = gen->ionRuntime()->getArgumentsRectifier(executionMode);

    masm.checkStackAlignment();

    // Guard that calleereg is actually a function object.
    masm.loadObjClass(calleereg, nargsreg);
    masm.cmpPtr(nargsreg, ImmWord(&JSFunction::class_));
    if (!bailoutIf(Assembler::NotEqual, call->snapshot()))
        return false;

    // Guard that calleereg is an interpreted function with a JSScript:
    masm.branchIfFunctionHasNoScript(calleereg, &uncompiled);

    // Knowing that calleereg is a non-native function, load the JSScript.
    masm.loadPtr(Address(calleereg, JSFunction::offsetOfNativeOrScript()), objreg);

    // Load script jitcode.
    masm.loadBaselineOrIonRaw(objreg, objreg, executionMode, &uncompiled);

    // Nestle the StackPointer up to the argument vector.
    masm.freeStack(unusedStack);

    // Construct the IonFramePrefix.
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), IonFrame_OptimizedJS);
    masm.Push(Imm32(call->numActualArgs()));
    masm.PushCalleeToken(calleereg, executionMode);
    masm.Push(Imm32(descriptor));

    // Check whether the provided arguments satisfy target argc.
    masm.load16ZeroExtend(Address(calleereg, offsetof(JSFunction, nargs)), nargsreg);
    masm.cmp32(nargsreg, Imm32(call->numStackArgs()));
    masm.j(Assembler::Above, &thunk);

    masm.jump(&makeCall);

    // Argument fixed needed. Load the ArgumentsRectifier.
    masm.bind(&thunk);
    {
        JS_ASSERT(ArgumentsRectifierReg != objreg);
        masm.movePtr(ImmGCPtr(argumentsRectifier), objreg); // Necessary for GC marking.
        masm.loadPtr(Address(objreg, IonCode::offsetOfCode()), objreg);
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
    JSFunction *target = call->getSingleTarget();
    ExecutionMode executionMode = gen->info().executionMode();
    Label end, uncompiled;

    // Native single targets are handled by LCallNative.
    JS_ASSERT(!target->isNative());
    // Missing arguments must have been explicitly appended by the IonBuilder.
    JS_ASSERT(target->nargs <= call->numStackArgs());

    masm.checkStackAlignment();

    // If the function is known to be uncompilable, just emit the call to
    // Invoke in sequential mode, else mark as cannot compile.
    JS_ASSERT(call->mir()->hasRootedScript());
    JSScript *targetScript = target->nonLazyScript();
    if (GetIonScript(targetScript, executionMode) == ION_DISABLED_SCRIPT &&
        (executionMode == ParallelExecution || !targetScript->canBaselineCompile()))
    {
        if (executionMode == ParallelExecution)
            return false;

        if (!emitCallInvokeFunction(call, calleereg, call->numActualArgs(), unusedStack))
            return false;

        if (call->mir()->isConstructing()) {
            Label notPrimitive;
            masm.branchTestPrimitive(Assembler::NotEqual, JSReturnOperand, &notPrimitive);
            masm.loadValue(Address(StackPointer, unusedStack), JSReturnOperand);
            masm.bind(&notPrimitive);
        }

        dropArguments(call->numStackArgs() + 1);
        return true;
    }

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
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed(), IonFrame_OptimizedJS);
    masm.Push(Imm32(call->numActualArgs()));
    masm.PushCalleeToken(calleereg, executionMode);
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
        masm.cmpPtr(objreg, ImmWord(&JSFunction::class_));
        if (!bailoutIf(Assembler::NotEqual, apply->snapshot()))
            return false;
    }

    // Copy the arguments of the current function.
    emitPushArguments(apply, copyreg);

    masm.checkStackAlignment();

    // If the function is known to be uncompilable, only emit the call to InvokeFunction.
    ExecutionMode executionMode = gen->info().executionMode();
    if (apply->hasSingleTarget()) {
        JSFunction *target = apply->getSingleTarget();
        if (!CanIonCompile(target, executionMode)) {
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
        masm.makeFrameDescriptor(copyreg, IonFrame_OptimizedJS);

        masm.Push(argcreg);
        masm.Push(calleereg);
        masm.Push(copyreg); // descriptor

        Label underflow, rejoin;

        // Check whether the provided arguments satisfy target argc.
        if (!apply->hasSingleTarget()) {
            masm.load16ZeroExtend(Address(calleereg, offsetof(JSFunction, nargs)), copyreg);
            masm.cmp32(argcreg, copyreg);
            masm.j(Assembler::Below, &underflow);
        } else {
            masm.cmp32(argcreg, Imm32(apply->getSingleTarget()->nargs));
            masm.j(Assembler::Below, &underflow);
        }

        // Skip the construction of the rectifier frame because we have no
        // underflow.
        masm.jump(&rejoin);

        // Argument fixup needed. Get ready to call the argumentsRectifier.
        {
            masm.bind(&underflow);

            // Hardcode the address of the argumentsRectifier code.
            IonCode *argumentsRectifier = gen->ionRuntime()->getArgumentsRectifier(executionMode);

            JS_ASSERT(ArgumentsRectifierReg != objreg);
            masm.movePtr(ImmGCPtr(argumentsRectifier), objreg); // Necessary for GC marking.
            masm.loadPtr(Address(objreg, IonCode::offsetOfCode()), objreg);
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

    Assembler::Condition cond = masm.testUndefined(Assembler::Equal, out);
    return bailoutIf(cond, lir->snapshot());
}

bool
CodeGenerator::visitFilterArguments(LFilterArguments *lir)
{
    Register string = ToRegister(lir->getString());
    Register temp1 = ToRegister(lir->temp1());
    Register temp2 = ToRegister(lir->temp2());

    masm.loadJSContext(temp2);

    masm.setupUnalignedABICall(2, temp1);
    masm.passABIArg(temp2);
    masm.passABIArg(string);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, FilterArguments));

    Label bail;
    masm.branchIfFalseBool(ReturnReg, &bail);
    return bailoutFrom(&bail, lir->snapshot());
}

typedef bool (*DirectEvalFn)(JSContext *, HandleObject, HandleScript, HandleValue, HandleString,
                             jsbytecode *, MutableHandleValue);
static const VMFunction DirectEvalInfo = FunctionInfo<DirectEvalFn>(DirectEvalFromIon);

bool
CodeGenerator::visitCallDirectEval(LCallDirectEval *lir)
{
    Register scopeChain = ToRegister(lir->getScopeChain());
    Register string = ToRegister(lir->getString());

    pushArg(ImmWord(lir->mir()->pc()));
    pushArg(string);
    pushArg(ToValue(lir, LCallDirectEval::ThisValueInput));
    pushArg(ImmGCPtr(gen->info().script()));
    pushArg(scopeChain);

    return callVM(DirectEvalInfo, lir);
}

// Registers safe for use before generatePrologue().
static const uint32_t EntryTempMask = Registers::TempMask & ~(1 << OsrFrameReg.code());

bool
CodeGenerator::generateArgumentsChecks()
{
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
        Label matched;
        masm.guardTypeSet(Address(StackPointer, offset), types, temp, &matched, &miss);
        masm.jump(&miss);
        masm.bind(&matched);
    }

    if (miss.used() && !bailoutFrom(&miss, graph.entrySnapshot()))
        return false;

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

    JSRuntime *rt = GetIonContext()->runtime;

    // Since Ion frames exist on the C stack, the stack limit may be
    // dynamically set by JS_SetThreadStackLimit() and JS_SetNativeStackQuota().
    uintptr_t *limitAddr = &rt->mainThread.ionStackLimit;

    CheckOverRecursedFailure *ool = new CheckOverRecursedFailure(lir);
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
    // parallel mode and hence uses the ionStackLimit from the current
    // thread state.  Also, we must check the interrupt flags because
    // on interrupt or abort, only the stack limit for the main thread
    // is reset, not the worker threads.  See comment in vm/ForkJoin.h
    // for more details.

    Register sliceReg = ToRegister(lir->forkJoinSlice());
    Register tempReg = ToRegister(lir->getTempReg());

    masm.loadPtr(Address(sliceReg, offsetof(ForkJoinSlice, perThreadData)), tempReg);
    masm.loadPtr(Address(tempReg, offsetof(PerThreadData, ionStackLimit)), tempReg);

    // Conditional forward (unlikely) branch to failure.
    CheckOverRecursedFailurePar *ool = new CheckOverRecursedFailurePar(lir);
    if (!addOutOfLineCode(ool))
        return false;
    masm.branchPtr(Assembler::BelowOrEqual, StackPointer, tempReg, ool->entry());
    masm.checkInterruptFlagsPar(tempReg, ool->entry());
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
    saveSet.maybeTake(tempReg);

    masm.PushRegsInMask(saveSet);
    masm.movePtr(ToRegister(lir->forkJoinSlice()), CallTempReg0);
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
class OutOfLineCheckInterruptPar : public OutOfLineCodeBase<CodeGenerator>
{
  public:
    LCheckInterruptPar *const lir;

    OutOfLineCheckInterruptPar(LCheckInterruptPar *lir)
      : lir(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineCheckInterruptPar(this);
    }
};

bool
CodeGenerator::visitCheckInterruptPar(LCheckInterruptPar *lir)
{
    // First check for slice->shared->interrupt_.
    OutOfLineCheckInterruptPar *ool = new OutOfLineCheckInterruptPar(lir);
    if (!addOutOfLineCode(ool))
        return false;

    // We must check two flags:
    // - runtime->interrupt
    // - runtime->parallelAbort
    // See vm/ForkJoin.h for discussion on why we use this design.

    Register tempReg = ToRegister(lir->getTempReg());
    masm.checkInterruptFlagsPar(tempReg, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineCheckInterruptPar(OutOfLineCheckInterruptPar *ool)
{
    OutOfLinePropagateAbortPar *bail = oolPropagateAbortPar(ool->lir);
    if (!bail)
        return false;

    // Avoid saving/restoring the temp register since we will put the
    // ReturnReg into it below and we don't want to clobber that
    // during PopRegsInMask():
    LCheckInterruptPar *lir = ool->lir;
    Register tempReg = ToRegister(lir->getTempReg());
    RegisterSet saveSet(lir->safepoint()->liveRegs());
    saveSet.maybeTake(tempReg);

    masm.PushRegsInMask(saveSet);
    masm.movePtr(ToRegister(ool->lir->forkJoinSlice()), CallTempReg0);
    masm.setupUnalignedABICall(1, CallTempReg1);
    masm.passABIArg(CallTempReg0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, CheckInterruptPar));
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
        return NULL;

    IonScriptCounts *counts = NULL;

    CompileInfo *outerInfo = &gen->info();
    JSScript *script = outerInfo->script();

    if (script && !script->hasScriptCounts && !script->initScriptCounts(cx))
        return NULL;

    counts = js_new<IonScriptCounts>();
    if (!counts || !counts->init(graph.numBlocks())) {
        js_delete(counts);
        return NULL;
    }

    if (script)
        script->addIonCounts(counts);

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
            DebugOnly<uint32_t> offset = resume->pc() - script->code;
            JS_ASSERT(offset < script->length);
        }

        if (!counts->block(i).init(block->id(), offset, block->numSuccessors()))
            return NULL;
        for (size_t j = 0; j < block->numSuccessors(); j++)
            counts->block(i).setSuccessor(j, block->getSuccessor(j)->id());
    }

    if (!script) {
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

    // Pointer to instructionBytes, spillBytes, or NULL, depending on the last
    // instruction processed.
    uint32_t *last;
    uint32_t lastLength;

  public:
    ScriptCountBlockState(IonBlockCounts *block, MacroAssembler *masm)
      : block(*block), masm(*masm),
        printer(GetIonContext()->cx),
        instructionBytes(0), spillBytes(0), last(NULL), lastLength(0)
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
        masm.setPrinter(NULL);

        if (last)
            *last += masm.size() - lastLength;

        block.setCode(printer.string());
        block.setInstructionBytes(instructionBytes);
        block.setSpillBytes(spillBytes);
    }
};

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

        LInstructionIterator iter = current->begin();

        // Separately visit the label at the start of every block, so that
        // count instrumentation is inserted after the block label is bound.
        if (!iter->accept(this))
            return false;
        iter++;

        mozilla::Maybe<ScriptCountBlockState> blockCounts;
        if (counts) {
            blockCounts.construct(&counts->block(i), &masm);
            if (!blockCounts.ref().init())
                return false;
        }

#if defined(JS_ION_PERF)
        perfSpewer->startBasicBlock(current->mir(), masm);
#endif

        for (; iter != current->end(); iter++) {
            IonSpew(IonSpew_Codegen, "instruction %s", iter->opName());

            if (counts)
                blockCounts.ref().visitInstruction(*iter);

            if (iter->safepoint() && pushedArgumentSlots_.length()) {
                if (!markArgumentSlots(iter->safepoint()))
                    return false;
            }

            if (!callTraceLIR(i, *iter))
                return false;

            if (!iter->accept(this))
                return false;
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

// Out-of-line object allocation for LNewParallelArray.
class OutOfLineNewParallelArray : public OutOfLineCodeBase<CodeGenerator>
{
    LNewParallelArray *lir_;

  public:
    OutOfLineNewParallelArray(LNewParallelArray *lir)
      : lir_(lir)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewParallelArray(this);
    }

    LNewParallelArray *lir() const {
        return lir_;
    }
};

typedef JSObject *(*NewInitParallelArrayFn)(JSContext *, HandleObject);
static const VMFunction NewInitParallelArrayInfo =
    FunctionInfo<NewInitParallelArrayFn>(NewInitParallelArray);

bool
CodeGenerator::visitNewParallelArrayVMCall(LNewParallelArray *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);

    Register objReg = ToRegister(lir->output());

    JS_ASSERT(!lir->isCall());
    saveLive(lir);

    pushArg(ImmGCPtr(lir->mir()->templateObject()));
    if (!callVM(NewInitParallelArrayInfo, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);
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
    types::TypeObject *type = templateObject->hasSingletonType() ? NULL : templateObject->type();

    pushArg(ImmGCPtr(type));
    pushArg(Imm32(lir->mir()->count()));

    if (!callVM(NewInitArrayInfo, lir))
        return false;

    if (ReturnReg != objReg)
        masm.movePtr(ReturnReg, objReg);

    restoreLive(lir);

    return true;
}

bool
CodeGenerator::visitNewSlots(LNewSlots *lir)
{
    Register temp1 = ToRegister(lir->temp1());
    Register temp2 = ToRegister(lir->temp2());
    Register temp3 = ToRegister(lir->temp3());
    Register output = ToRegister(lir->output());

    masm.mov(ImmWord(GetIonContext()->runtime), temp1);
    masm.mov(Imm32(lir->mir()->nslots()), temp2);

    masm.setupUnalignedABICall(2, temp3);
    masm.passABIArg(temp1);
    masm.passABIArg(temp2);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NewSlots));

    masm.testPtr(output, output);
    if (!bailoutIf(Assembler::Zero, lir->snapshot()))
        return false;

    return true;
}

bool CodeGenerator::visitAtan2D(LAtan2D *lir)
{
    Register temp = ToRegister(lir->temp());
    FloatRegister y = ToFloatRegister(lir->y());
    FloatRegister x = ToFloatRegister(lir->x());

    masm.setupUnalignedABICall(2, temp);
    masm.passABIArg(y);
    masm.passABIArg(x);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ecmaAtan2), MacroAssembler::DOUBLE);

    JS_ASSERT(ToFloatRegister(lir->output()) == ReturnFloatReg);
    return true;
}

bool
CodeGenerator::visitNewParallelArray(LNewParallelArray *lir)
{
    Register objReg = ToRegister(lir->output());
    JSObject *templateObject = lir->mir()->templateObject();

    OutOfLineNewParallelArray *ool = new OutOfLineNewParallelArray(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.newGCThing(objReg, templateObject, ool->entry());
    masm.initGCThing(objReg, templateObject);

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineNewParallelArray(OutOfLineNewParallelArray *ool)
{
    if (!visitNewParallelArrayVMCall(ool->lir()))
        return false;
    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitNewArray(LNewArray *lir)
{
    JS_ASSERT(gen->info().executionMode() == SequentialExecution);
    Register objReg = ToRegister(lir->output());
    JSObject *templateObject = lir->mir()->templateObject();
    DebugOnly<uint32_t> count = lir->mir()->count();

    JS_ASSERT(count < JSObject::NELEMENTS_LIMIT);

    if (lir->mir()->shouldUseVM())
        return visitNewArrayCallVM(lir);

    OutOfLineNewArray *ool = new OutOfLineNewArray(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.newGCThing(objReg, templateObject, ool->entry());
    masm.initGCThing(objReg, templateObject);

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
    JSObject *templateObject = lir->mir()->templateObject();

    if (lir->mir()->shouldUseVM())
        return visitNewObjectVMCall(lir);

    OutOfLineNewObject *ool = new OutOfLineNewObject(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.newGCThing(objReg, templateObject, ool->entry());
    masm.initGCThing(objReg, templateObject);

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
    Register obj = ToRegister(lir->output());
    JSObject *templateObj = lir->mir()->templateObj();
    CompileInfo &info = lir->mir()->block()->info();

    // If we have a template object, we can inline call object creation.
    OutOfLineCode *ool = oolCallVM(NewDeclEnvObjectInfo, lir,
                                   (ArgList(), ImmGCPtr(info.fun()), Imm32(gc::DefaultHeap)),
                                   StoreRegisterTo(obj));
    if (!ool)
        return false;

    masm.newGCThing(obj, templateObj, ool->entry());
    masm.initGCThing(obj, templateObj);
    masm.bind(ool->rejoin());
    return true;
}

typedef JSObject *(*NewCallObjectFn)(JSContext *, HandleScript, HandleShape,
                                     HandleTypeObject, HeapSlot *);
static const VMFunction NewCallObjectInfo =
    FunctionInfo<NewCallObjectFn>(NewCallObject);

bool
CodeGenerator::visitNewCallObject(LNewCallObject *lir)
{
    Register obj = ToRegister(lir->output());

    JSObject *templateObj = lir->mir()->templateObject();

    // If we have a template object, we can inline call object creation.
    OutOfLineCode *ool;
    if (lir->slots()->isRegister()) {
        ool = oolCallVM(NewCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(lir->mir()->block()->info().script()),
                                    ImmGCPtr(templateObj->lastProperty()),
                                    ImmGCPtr(templateObj->hasLazyType() ? NULL : templateObj->type()),
                                    ToRegister(lir->slots())),
                        StoreRegisterTo(obj));
    } else {
        ool = oolCallVM(NewCallObjectInfo, lir,
                        (ArgList(), ImmGCPtr(lir->mir()->block()->info().script()),
                                    ImmGCPtr(templateObj->lastProperty()),
                                    ImmGCPtr(templateObj->hasLazyType() ? NULL : templateObj->type()),
                                    ImmWord((void *)NULL)),
                        StoreRegisterTo(obj));
    }
    if (!ool)
        return false;

    if (lir->mir()->needsSingletonType()) {
        // Objects can only be given singleton types in VM calls.
        masm.jump(ool->entry());
    } else {
        masm.newGCThing(obj, templateObj, ool->entry());
        masm.initGCThing(obj, templateObj);

        if (lir->slots()->isRegister())
            masm.storePtr(ToRegister(lir->slots()), Address(obj, JSObject::offsetOfSlots()));
    }

    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitNewCallObjectPar(LNewCallObjectPar *lir)
{
    Register resultReg = ToRegister(lir->output());
    Register sliceReg = ToRegister(lir->forkJoinSlice());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    JSObject *templateObj = lir->mir()->templateObj();

    emitAllocateGCThingPar(lir, resultReg, sliceReg, tempReg1, tempReg2, templateObj);

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
    Register sliceReg = ToRegister(lir->forkJoinSlice());
    Register lengthReg = ToRegister(lir->length());
    Register tempReg0 = ToRegister(lir->getTemp0());
    Register tempReg1 = ToRegister(lir->getTemp1());
    Register tempReg2 = ToRegister(lir->getTemp2());
    JSObject *templateObj = lir->mir()->templateObject();

    // Allocate the array into tempReg2.  Don't use resultReg because it
    // may alias sliceReg etc.
    emitAllocateGCThingPar(lir, tempReg2, sliceReg, tempReg0, tempReg1, templateObj);

    // Invoke a C helper to allocate the elements.  For convenience,
    // this helper also returns the array back to us, or NULL, which
    // obviates the need to preserve the register across the call.  In
    // reality, we should probably just have the C helper also
    // *allocate* the array, but that would require that it initialize
    // the various fields of the object, and I didn't want to
    // duplicate the code in initGCThing() that already does such an
    // admirable job.
    masm.setupUnalignedABICall(3, CallTempReg3);
    masm.passABIArg(sliceReg);
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

    masm.newGCThing(output, templateObj, ool->entry());
    masm.initGCThing(output, templateObj);

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
    Register sliceReg = ToRegister(lir->forkJoinSlice());
    Register tempReg1 = ToRegister(lir->getTemp0());
    Register tempReg2 = ToRegister(lir->getTemp1());
    JSObject *templateObject = lir->mir()->templateObject();
    emitAllocateGCThingPar(lir, objReg, sliceReg, tempReg1, tempReg2, templateObject);
    return true;
}

class OutOfLineNewGCThingPar : public OutOfLineCodeBase<CodeGenerator>
{
public:
    LInstruction *lir;
    gc::AllocKind allocKind;
    Register objReg;

    OutOfLineNewGCThingPar(LInstruction *lir, gc::AllocKind allocKind, Register objReg)
      : lir(lir), allocKind(allocKind), objReg(objReg)
    {}

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineNewGCThingPar(this);
    }
};

bool
CodeGenerator::emitAllocateGCThingPar(LInstruction *lir, const Register &objReg,
                                      const Register &sliceReg, const Register &tempReg1,
                                      const Register &tempReg2, JSObject *templateObj)
{
    gc::AllocKind allocKind = templateObj->tenuredGetAllocKind();
    OutOfLineNewGCThingPar *ool = new OutOfLineNewGCThingPar(lir, allocKind, objReg);
    if (!ool || !addOutOfLineCode(ool))
        return false;

    masm.newGCThingPar(objReg, sliceReg, tempReg1, tempReg2,
                            templateObj, ool->entry());
    masm.bind(ool->rejoin());
    masm.initGCThing(objReg, templateObj);
    return true;
}

bool
CodeGenerator::visitOutOfLineNewGCThingPar(OutOfLineNewGCThingPar *ool)
{
    // As a fallback for allocation in par. exec. mode, we invoke the
    // C helper NewGCThingPar(), which calls into the GC code.  If it
    // returns NULL, we bail.  If returns non-NULL, we rejoin the
    // original instruction.

    // This saves all caller-save registers, regardless of whether
    // they are live.  This is wasteful but a simplification, given
    // that for some of the LIR that this is used with
    // (e.g., LLambdaPar) there are values in those registers
    // that must not be clobbered but which are not technically
    // considered live.
    RegisterSet saveSet(RegisterSet::Volatile());

    // Also preserve the temps we're about to overwrite,
    // but don't bother to save the objReg.
    saveSet.addUnchecked(CallTempReg0);
    saveSet.addUnchecked(CallTempReg1);
    saveSet.maybeTake(AnyRegister(ool->objReg));

    masm.PushRegsInMask(saveSet);
    masm.move32(Imm32(ool->allocKind), CallTempReg0);
    masm.setupUnalignedABICall(1, CallTempReg1);
    masm.passABIArg(CallTempReg0);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NewGCThingPar));
    masm.movePtr(ReturnReg, ool->objReg);
    masm.PopRegsInMask(saveSet);
    OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutOutOfMemory, ool->lir);
    if (!bail)
        return false;
    masm.branchTestPtr(Assembler::Zero, ool->objReg, ool->objReg, bail->entry());
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
    pushArg(ImmWord(lir->mir()->resumePoint()->pc()));

    return callVM(InitElemGetterSetterInfo, lir);
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
    pushArg(ImmWord(lir->mir()->resumePoint()->pc()));

    return callVM(InitPropGetterSetterInfo, lir);
}

typedef bool (*CreateThisFn)(JSContext *cx, HandleObject callee, MutableHandleValue rval);
static const VMFunction CreateThisInfo = FunctionInfo<CreateThisFn>(CreateThis);

bool
CodeGenerator::visitCreateThis(LCreateThis *lir)
{
    const LAllocation *callee = lir->getCallee();

    if (callee->isConstant())
        pushArg(ImmGCPtr(&callee->toConstant()->toObject()));
    else
        pushArg(ToRegister(callee));

    return callVM(CreateThisInfo, lir);
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

typedef JSObject *(*NewGCThingFn)(JSContext *cx, gc::AllocKind allocKind, size_t thingSize);
static const VMFunction NewGCThingInfo =
    FunctionInfo<NewGCThingFn>(js::ion::NewGCThing);

bool
CodeGenerator::visitCreateThisWithTemplate(LCreateThisWithTemplate *lir)
{
    JSObject *templateObject = lir->mir()->getTemplateObject();
    gc::AllocKind allocKind = templateObject->tenuredGetAllocKind();
    int thingSize = (int)gc::Arena::thingSize(allocKind);
    Register objReg = ToRegister(lir->output());

    OutOfLineCode *ool = oolCallVM(NewGCThingInfo, lir,
                                   (ArgList(), Imm32(allocKind), Imm32(thingSize)),
                                   StoreRegisterTo(objReg));
    if (!ool)
        return false;

    // Allocate. If the FreeList is empty, call to VM, which may GC.
    masm.newGCThing(objReg, templateObject, ool->entry());

    // Initialize based on the templateObject.
    masm.bind(ool->rejoin());
    masm.initGCThing(objReg, templateObject);

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
    masm.breakpoint();
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
    masm.breakpoint();
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

bool
CodeGenerator::visitArrayLength(LArrayLength *lir)
{
    Address length(ToRegister(lir->elements()), ObjectElements::offsetOfLength());
    masm.load32(length, ToRegister(lir->output()));
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

    if (ins->second()->isConstant())
        masm.cmp32(first, Imm32(ToInt32(ins->second())));
    else
        masm.cmp32(first, ToRegister(ins->second()));

    Label done;
    if (ins->mir()->isMax())
        masm.j(Assembler::GreaterThan, &done);
    else
        masm.j(Assembler::LessThan, &done);

    if (ins->second()->isConstant())
        masm.move32(Imm32(ToInt32(ins->second())), output);
    else
        masm.mov(ToRegister(ins->second()), output);


    masm.bind(&done);
    return true;
}

bool
CodeGenerator::visitAbsI(LAbsI *ins)
{
    Register input = ToRegister(ins->input());
    Label positive;

    JS_ASSERT(input == ToRegister(ins->output()));
    masm.test32(input, input);
    masm.j(Assembler::GreaterThanOrEqual, &positive);
    masm.neg32(input);
    if (ins->snapshot() && !bailoutIf(Assembler::Overflow, ins->snapshot()))
        return false;
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
    masm.passABIArg(value);
    masm.passABIArg(power);

    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::powi), MacroAssembler::DOUBLE);
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
    masm.passABIArg(value);
    masm.passABIArg(power);
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, ecmaPow), MacroAssembler::DOUBLE);

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
    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, math_random_no_outparam), MacroAssembler::DOUBLE);

    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);
    return true;
}

bool
CodeGenerator::visitMathFunctionD(LMathFunctionD *ins)
{
    Register temp = ToRegister(ins->temp());
    FloatRegister input = ToFloatRegister(ins->input());
    JS_ASSERT(ToFloatRegister(ins->output()) == ReturnFloatReg);

    MathCache *mathCache = ins->mir()->cache();

    masm.setupUnalignedABICall(2, temp);
    masm.movePtr(ImmWord(mathCache), temp);
    masm.passABIArg(temp);
    masm.passABIArg(input);

    void *funptr = NULL;
    switch (ins->mir()->function()) {
      case MMathFunction::Log:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_log_impl);
        break;
      case MMathFunction::Sin:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_sin_impl);
        break;
      case MMathFunction::Cos:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_cos_impl);
        break;
      case MMathFunction::Exp:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_exp_impl);
        break;
      case MMathFunction::Tan:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_tan_impl);
        break;
      case MMathFunction::ATan:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_atan_impl);
        break;
      case MMathFunction::ASin:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_asin_impl);
        break;
      case MMathFunction::ACos:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_acos_impl);
        break;
      case MMathFunction::Log10:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_log10_impl);
        break;
      case MMathFunction::Log2:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_log2_impl);
        break;
      case MMathFunction::Log1P:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_log1p_impl);
        break;
      case MMathFunction::ExpM1:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_expm1_impl);
        break;
      case MMathFunction::CosH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_cosh_impl);
        break;
      case MMathFunction::SinH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_sinh_impl);
        break;
      case MMathFunction::TanH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_tanh_impl);
        break;
      case MMathFunction::ACosH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_acosh_impl);
        break;
      case MMathFunction::ASinH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_asinh_impl);
        break;
      case MMathFunction::ATanH:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_atanh_impl);
        break;
      case MMathFunction::Sign:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_sign_impl);
        break;
      case MMathFunction::Trunc:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_trunc_impl);
        break;
      case MMathFunction::Cbrt:
        funptr = JS_FUNC_TO_DATA_PTR(void *, js::math_cbrt_impl);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unknown math function");
    }

    masm.callWithABI(funptr, MacroAssembler::DOUBLE);
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
    masm.passABIArg(lhs);
    masm.passABIArg(rhs);

    masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, NumberMod), MacroAssembler::DOUBLE);
    return true;
}

typedef bool (*BinaryFn)(JSContext *, HandleScript, jsbytecode *,
                         MutableHandleValue, MutableHandleValue, Value *);
typedef ParallelResult (*BinaryParFn)(ForkJoinSlice *, HandleValue, HandleValue,
                                      Value *);

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
    if (gen->info().executionMode() == SequentialExecution) {
        pushArg(ImmWord(lir->mirRaw()->toInstruction()->resumePoint()->pc()));
        pushArg(ImmGCPtr(current->mir()->info().script()));
    }

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
typedef ParallelResult (*StringCompareParFn)(ForkJoinSlice *, HandleString, HandleString, bool *);
static const VMFunctionsModal StringsEqualInfo = VMFunctionsModal(
    FunctionInfo<StringCompareFn>(ion::StringsEqual<true>),
    FunctionInfo<StringCompareParFn>(ion::StringsEqualPar));
static const VMFunctionsModal StringsNotEqualInfo = VMFunctionsModal(
    FunctionInfo<StringCompareFn>(ion::StringsEqual<false>),
    FunctionInfo<StringCompareParFn>(ion::StringsUnequalPar));

bool
CodeGenerator::emitCompareS(LInstruction *lir, JSOp op, Register left, Register right,
                            Register output, Register temp)
{
    JS_ASSERT(lir->isCompareS() || lir->isCompareStrictS());

    OutOfLineCode *ool = NULL;

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
typedef ParallelResult (*CompareParFn)(ForkJoinSlice *, MutableHandleValue, MutableHandleValue,
                                       bool *);
static const VMFunctionsModal EqInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::LooselyEqual<true>),
    FunctionInfo<CompareParFn>(ion::LooselyEqualPar));
static const VMFunctionsModal NeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::LooselyEqual<false>),
    FunctionInfo<CompareParFn>(ion::LooselyUnequalPar));
static const VMFunctionsModal StrictEqInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::StrictlyEqual<true>),
    FunctionInfo<CompareParFn>(ion::StrictlyEqualPar));
static const VMFunctionsModal StrictNeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::StrictlyEqual<false>),
    FunctionInfo<CompareParFn>(ion::StrictlyUnequalPar));
static const VMFunctionsModal LtInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::LessThan),
    FunctionInfo<CompareParFn>(ion::LessThanPar));
static const VMFunctionsModal LeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::LessThanOrEqual),
    FunctionInfo<CompareParFn>(ion::LessThanOrEqualPar));
static const VMFunctionsModal GtInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::GreaterThan),
    FunctionInfo<CompareParFn>(ion::GreaterThanPar));
static const VMFunctionsModal GeInfo = VMFunctionsModal(
    FunctionInfo<CompareFn>(ion::GreaterThanOrEqual),
    FunctionInfo<CompareParFn>(ion::GreaterThanOrEqualPar));

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

        OutOfLineTestObjectWithLabels *ool = NULL;
        Maybe<Label> label1, label2;
        Label *nullOrLikeUndefined;
        Label *notNullOrLikeUndefined;
        if (lir->mir()->operandMightEmulateUndefined()) {
            ool = new OutOfLineTestObjectWithLabels();
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
            testObjectTruthy(objreg, notNullOrLikeUndefined, nullOrLikeUndefined,
                             ToRegister(lir->temp()), ool);
        }

        Label done;

        // It's not null or undefined, and if it's an object it doesn't
        // emulate undefined, so it's not like undefined.
        masm.bind(notNullOrLikeUndefined);
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
        cond = masm.testNull(cond, value);
    else
        cond = masm.testUndefined(cond, value);

    masm.emitSet(cond, output);
    return true;
}

bool
CodeGenerator::visitIsNullOrLikeUndefinedAndBranch(LIsNullOrLikeUndefinedAndBranch *lir)
{
    JSOp op = lir->mir()->jsop();
    MCompare::CompareType compareType = lir->mir()->compareType();
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

        MOZ_ASSERT(lir->mir()->lhs()->type() != MIRType_Object ||
                   lir->mir()->operandMightEmulateUndefined(),
                   "Operands which can't emulate undefined should have been folded");

        OutOfLineTestObject *ool = NULL;
        if (lir->mir()->operandMightEmulateUndefined()) {
            ool = new OutOfLineTestObject();
            if (!addOutOfLineCode(ool))
                return false;
        }

        Register tag = masm.splitTagForTest(value);
        Label *ifTrueLabel = ifTrue->lir()->label();
        Label *ifFalseLabel = ifFalse->lir()->label();

        masm.branchTestNull(Assembler::Equal, tag, ifTrueLabel);
        masm.branchTestUndefined(Assembler::Equal, tag, ifTrueLabel);

        if (ool) {
            masm.branchTestObject(Assembler::NotEqual, tag, ifFalseLabel);

            // Objects that emulate undefined are loosely equal to null/undefined.
            Register objreg = masm.extractObject(value, ToTempUnboxRegister(lir->tempToUnbox()));
            testObjectTruthy(objreg, ifFalseLabel, ifTrueLabel, ToRegister(lir->temp()), ool);
        } else {
            masm.jump(ifFalseLabel);
        }
        return true;
    }

    JS_ASSERT(op == JSOP_STRICTEQ || op == JSOP_STRICTNE);

    Assembler::Condition cond = JSOpToCondition(compareType, op);
    if (compareType == MCompare::Compare_Null)
        cond = masm.testNull(cond, value);
    else
        cond = masm.testUndefined(cond, value);

    emitBranch(cond, lir->ifTrue(), lir->ifFalse());
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

    OutOfLineTestObjectWithLabels *ool = new OutOfLineTestObjectWithLabels();
    if (!addOutOfLineCode(ool))
        return false;

    Label *emulatesUndefined = ool->label1();
    Label *doesntEmulateUndefined = ool->label2();

    Register objreg = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    testObjectTruthy(objreg, doesntEmulateUndefined, emulatesUndefined, output, ool);

    Label done;

    masm.bind(doesntEmulateUndefined);
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
    MOZ_ASSERT(lir->mir()->compareType() == MCompare::Compare_Undefined ||
               lir->mir()->compareType() == MCompare::Compare_Null);
    MOZ_ASSERT(lir->mir()->operandMightEmulateUndefined(),
               "Operands which can't emulate undefined should have been folded");

    JSOp op = lir->mir()->jsop();
    MOZ_ASSERT(op == JSOP_EQ || op == JSOP_NE, "Strict equality should have been folded");

    OutOfLineTestObject *ool = new OutOfLineTestObject();
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

        equal = ifTrue->lir()->label();
        unequal = ifFalse->lir()->label();
    }

    Register objreg = ToRegister(lir->input());

    testObjectTruthy(objreg, unequal, equal, ToRegister(lir->temp()), ool);
    return true;
}

typedef JSString *(*ConcatStringsFn)(ThreadSafeContext *, HandleString, HandleString);
typedef ParallelResult (*ConcatStringsParFn)(ForkJoinSlice *, HandleString, HandleString,
                                             MutableHandleString);
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
    IonCode *stringConcatStub = gen->ionCompartment()->stringConcatStub(mode);
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
    JS_ASSERT(ToRegister(lir->temp1()) == CallTempReg2);
    JS_ASSERT(ToRegister(lir->temp2()) == CallTempReg3);
    JS_ASSERT(ToRegister(lir->temp3()) == CallTempReg4);
    JS_ASSERT(ToRegister(lir->temp4()) == CallTempReg5);
    JS_ASSERT(output == CallTempReg6);

    return emitConcat(lir, lhs, rhs, output);
}

bool
CodeGenerator::visitConcatPar(LConcatPar *lir)
{
    DebugOnly<Register> slice = ToRegister(lir->forkJoinSlice());
    Register lhs = ToRegister(lir->lhs());
    Register rhs = ToRegister(lir->rhs());
    Register output = ToRegister(lir->output());

    JS_ASSERT(lhs == CallTempReg0);
    JS_ASSERT(rhs == CallTempReg1);
    JS_ASSERT((Register)slice == CallTempReg5);
    JS_ASSERT(ToRegister(lir->temp1()) == CallTempReg2);
    JS_ASSERT(ToRegister(lir->temp2()) == CallTempReg3);
    JS_ASSERT(ToRegister(lir->temp3()) == CallTempReg4);
    JS_ASSERT(output == CallTempReg6);

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
    masm.breakpoint();
    masm.bind(&ok);
#endif

    JS_STATIC_ASSERT(sizeof(jschar) == 2);

    Label start;
    masm.bind(&start);
    masm.load16ZeroExtend(Address(from, 0), scratch);
    masm.store16(scratch, Address(to, 0));
    masm.addPtr(Imm32(2), from);
    masm.addPtr(Imm32(2), to);
    masm.sub32(Imm32(1), len);
    masm.j(Assembler::NonZero, &start);
}

IonCode *
IonCompartment::generateStringConcatStub(JSContext *cx, ExecutionMode mode)
{
    MacroAssembler masm(cx);

    Register lhs = CallTempReg0;
    Register rhs = CallTempReg1;
    Register temp1 = CallTempReg2;
    Register temp2 = CallTempReg3;
    Register temp3 = CallTempReg4;
    Register temp4 = CallTempReg5;
    Register output = CallTempReg6;

    // In parallel execution, we pass in the ForkJoinSlice in CallTempReg5, as
    // by the time we need to use the temp4 we no longer have need of the
    // slice.
    Register forkJoinSlice = CallTempReg5;

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

    // Check if we can use a JSShortString.
    Label isShort;
    masm.branch32(Assembler::BelowOrEqual, temp2, Imm32(JSShortString::MAX_SHORT_LENGTH),
                  &isShort);

    // Ensure result length <= JSString::MAX_LENGTH.
    masm.branch32(Assembler::Above, temp2, Imm32(JSString::MAX_LENGTH), &failure);

    // Allocate a new rope.
    switch (mode) {
      case SequentialExecution:
        masm.newGCString(output, &failure);
        break;
      case ParallelExecution:
        masm.push(temp1);
        masm.push(temp2);
        masm.newGCStringPar(output, forkJoinSlice, temp1, temp2, &failurePopTemps);
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

    masm.bind(&isShort);

    // State: lhs length in temp1, result length in temp2.

    // Ensure both strings are linear (flags != 0).
    JS_STATIC_ASSERT(JSString::ROPE_FLAGS == 0);
    masm.branchTestPtr(Assembler::Zero, Address(lhs, JSString::offsetOfLengthAndFlags()),
                       Imm32(JSString::FLAGS_MASK), &failure);
    masm.branchTestPtr(Assembler::Zero, Address(rhs, JSString::offsetOfLengthAndFlags()),
                       Imm32(JSString::FLAGS_MASK), &failure);

    // Allocate a JSShortString.
    switch (mode) {
      case SequentialExecution:
        masm.newGCShortString(output, &failure);
        break;
      case ParallelExecution:
        masm.push(temp1);
        masm.push(temp2);
        masm.newGCShortStringPar(output, forkJoinSlice, temp1, temp2, &failurePopTemps);
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
    masm.computeEffectiveAddress(Address(output, JSShortString::offsetOfInlineStorage()), temp2);
    masm.storePtr(temp2, Address(output, JSShortString::offsetOfChars()));

    // Copy lhs chars. Temp1 still holds the lhs length. Note that this
    // advances temp2 to point to the next char.
    masm.loadPtr(Address(lhs, JSString::offsetOfChars()), temp3);
    CopyStringChars(masm, temp2, temp3, temp1, temp4);

    // Copy rhs chars.
    masm.loadPtr(Address(rhs, JSString::offsetOfChars()), temp3);
    masm.loadStringLength(rhs, temp1);
    CopyStringChars(masm, temp2, temp3, temp1, temp4);

    // Null-terminate.
    masm.store16(Imm32(0), Address(temp2, 0));
    masm.ret();

    masm.bind(&failurePopTemps);
    masm.pop(temp2);
    masm.pop(temp1);

    masm.bind(&failure);
    masm.movePtr(ImmWord((void *)NULL), output);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx, JSC::OTHER_CODE);
}

typedef bool (*CharCodeAtFn)(JSContext *, HandleString, int32_t, uint32_t *);
static const VMFunction CharCodeAtInfo = FunctionInfo<CharCodeAtFn>(ion::CharCodeAt);

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
    masm.loadPtr(lengthAndFlagsAddr, output);

    masm.branchTest32(Assembler::Zero, output, Imm32(JSString::FLAGS_MASK), ool->entry());

    // getChars
    Address charsAddr(str, JSString::offsetOfChars());
    masm.loadPtr(charsAddr, output);
    masm.load16ZeroExtend(BaseIndex(output, index, TimesTwo, 0), output);

    masm.bind(ool->rejoin());
    return true;
}

typedef JSFlatString *(*StringFromCharCodeFn)(JSContext *, int32_t);
static const VMFunction StringFromCharCodeInfo = FunctionInfo<StringFromCharCodeFn>(ion::StringFromCharCode);

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

    masm.movePtr(ImmWord(&GetIonContext()->runtime->staticStrings.unitStaticTable), output);
    masm.loadPtr(BaseIndex(output, code, ScalePointer), output);

    masm.bind(ool->rejoin());
    return true;
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

    OutOfLineTestObjectWithLabels *ool = new OutOfLineTestObjectWithLabels();
    if (!addOutOfLineCode(ool))
        return false;

    Label *ifTruthy = ool->label1();
    Label *ifFalsy = ool->label2();

    Register objreg = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    testObjectTruthy(objreg, ifTruthy, ifFalsy, output, ool);

    Label join;

    masm.bind(ifTruthy);
    masm.move32(Imm32(0), output);
    masm.jump(&join);

    masm.bind(ifFalsy);
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

    OutOfLineTestObjectWithLabels *ool = NULL;
    if (lir->mir()->operandMightEmulateUndefined()) {
        ool = new OutOfLineTestObjectWithLabels();
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

    testValueTruthy(ToValue(lir, LNotV::Input), lir->temp1(), lir->temp2(),
                    ToFloatRegister(lir->tempFloat()),
                    ifTruthy, ifFalsy, ool);

    Label join;
    Register output = ToRegister(lir->output());

    masm.bind(ifFalsy);
    masm.move32(Imm32(1), output);
    masm.jump(&join);

    masm.bind(ifTruthy);
    masm.move32(Imm32(0), output);

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
        masm.cmp32(ToOperand(lir->length()), Imm32(index));
        return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
    }
    if (lir->length()->isConstant()) {
        masm.cmp32(ToRegister(lir->index()), Imm32(ToInt32(lir->length())));
        return bailoutIf(Assembler::AboveOrEqual, lir->snapshot());
    }
    masm.cmp32(ToOperand(lir->length()), ToRegister(lir->index()));
    return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
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
            masm.cmp32(ToOperand(lir->length()), Imm32(nmax));
            return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
        }
        masm.mov(Imm32(index), temp);
    } else {
        masm.mov(ToRegister(lir->index()), temp);
    }

    // If the minimum and maximum differ then do an underflow check first.
    // If the two are the same then doing an unsigned comparison on the
    // length will also catch a negative index.
    if (min != max) {
        if (min != 0) {
            masm.add32(Imm32(min), temp);
            if (!bailoutIf(Assembler::Overflow, lir->snapshot()))
                return false;
            int32_t diff;
            if (SafeSub(max, min, &diff))
                max = diff;
            else
                masm.sub32(Imm32(min), temp);
        }

        masm.cmp32(temp, Imm32(0));
        if (!bailoutIf(Assembler::LessThan, lir->snapshot()))
            return false;
    }

    // Compute the maximum possible index. No overflow check is needed when
    // max > 0. We can only wraparound to a negative number, which will test as
    // larger than all nonnegative numbers in the unsigned comparison, and the
    // length is required to be nonnegative (else testing a negative length
    // would succeed on any nonnegative index).
    if (max != 0) {
        masm.add32(Imm32(max), temp);
        if (max < 0 && !bailoutIf(Assembler::Overflow, lir->snapshot()))
            return false;
    }

    masm.cmp32(ToOperand(lir->length()), temp);
    return bailoutIf(Assembler::BelowOrEqual, lir->snapshot());
}

bool
CodeGenerator::visitBoundsCheckLower(LBoundsCheckLower *lir)
{
    int32_t min = lir->mir()->minimum();
    masm.cmp32(ToRegister(lir->index()), Imm32(min));
    return bailoutIf(Assembler::LessThan, lir->snapshot());
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
    Assembler::Condition cond;
    if (index->isConstant())
        cond = masm.testMagic(Assembler::Equal, Address(elements, ToInt32(index) * sizeof(js::Value)));
    else
        cond = masm.testMagic(Assembler::Equal, BaseIndex(elements, ToRegister(index), TimesEight));
    return bailoutIf(cond, snapshot);
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
    OutOfLineStoreElementHole *ool = new OutOfLineStoreElementHole(lir);
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
    OutOfLineStoreElementHole *ool = new OutOfLineStoreElementHole(lir);
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
static const VMFunction SetObjectElementInfo = FunctionInfo<SetObjectElementFn>(SetObjectElement);

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

    // We can bump the initialized length inline if index ==
    // initializedLength and index < capacity.  Otherwise, we have to
    // consider fallback options.  In fallback cases, we branch to one
    // of two labels because (at least in parallel mode) we can
    // recover from index < capacity but not index !=
    // initializedLength.
    Label indexNotInitLen;
    Label indexWouldExceedCapacity;

    // If index == initializedLength, try to bump the initialized length inline.
    // If index > initializedLength, call a stub. Note that this relies on the
    // condition flags sticking from the incoming branch.
    masm.j(Assembler::NotEqual, &indexNotInitLen);

    Int32Key key = ToInt32Key(index);

    // Check array capacity.
    masm.branchKey(Assembler::BelowOrEqual, Address(elements, ObjectElements::offsetOfCapacity()),
                   key, &indexWouldExceedCapacity);

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

    switch (gen->info().executionMode()) {
      case SequentialExecution:
        masm.bind(&indexNotInitLen);
        masm.bind(&indexWouldExceedCapacity);
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

      case ParallelExecution:
        //////////////////////////////////////////////////////////////
        // If the problem is that we do not have sufficient capacity,
        // try to reallocate the elements array and then branch back
        // to perform the actual write.  Note that we do not want to
        // force the reg alloc to assign any particular register, so
        // we make space on the stack and pass the arguments that way.
        // (Also, outside of the VM call mechanism, it's very hard to
        // pass in a Value to a C function!).
        masm.bind(&indexWouldExceedCapacity);

        OutOfLineAbortPar *bail = oolAbortPar(ParallelBailoutOutOfMemory, ins);
        if (!bail)
            return false;

        // The use of registers here is somewhat subtle.  We need to
        // save and restore the volatile registers but we also need to
        // preserve the ReturnReg. Normally we'd just add a constraint
        // to the regalloc, but since this is the slow path of a hot
        // instruction we don't want to do that.  So instead we push
        // the volatile registers but we don't save the register
        // `object`.  We will copy the ReturnReg into `object`.  The
        // function we are calling (`PushPar`) agrees to either return
        // `object` unchanged or NULL.  This way after we restore the
        // registers, we can examine `object` to know whether an error
        // occurred.
        RegisterSet saveSet(ins->safepoint()->liveRegs());
        saveSet.maybeTake(object);

        masm.PushRegsInMask(saveSet);
        masm.reserveStack(sizeof(PushParArgs));
        masm.storePtr(object, Address(StackPointer, offsetof(PushParArgs, object)));
        masm.storeConstantOrRegister(value, Address(StackPointer,
                                                    offsetof(PushParArgs, value)));
        masm.movePtr(StackPointer, CallTempReg0);
        masm.setupUnalignedABICall(1, CallTempReg1);
        masm.passABIArg(CallTempReg0);
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, PushPar));
        masm.freeStack(sizeof(PushParArgs));
        masm.movePtr(ReturnReg, object);
        masm.PopRegsInMask(saveSet);
        masm.branchTestPtr(Assembler::Zero, object, object, bail->entry());
        masm.jump(ool->rejoin());

        //////////////////////////////////////////////////////////////
        // If the problem is that we are trying to write an index that
        // is not the initialized length, that would result in a
        // sparse array, and since we don't want to think about that
        // case right now, we just bail out.
        masm.bind(&indexNotInitLen);
        OutOfLineAbortPar *bail1 = oolAbortPar(ParallelBailoutUnsupportedSparseArray, ins);
        if (!bail1)
            return false;
        masm.jump(bail1->entry());
        return true;
    }

    JS_ASSERT(false);
    return false;
}

typedef bool (*ArrayPopShiftFn)(JSContext *, HandleObject, MutableHandleValue);
static const VMFunction ArrayPopDenseInfo = FunctionInfo<ArrayPopShiftFn>(ion::ArrayPopDense);
static const VMFunction ArrayShiftDenseInfo = FunctionInfo<ArrayPopShiftFn>(ion::ArrayShiftDense);

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
    FunctionInfo<ArrayPushDenseFn>(ion::ArrayPushDense);

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
    // inline and pass it to the stub. Else, we just pass NULL and the stub falls back
    // to a slow path.
    Label fail, call;
    masm.loadPtr(Address(lhs, JSObject::offsetOfElements()), temp1);
    masm.load32(Address(temp1, ObjectElements::offsetOfInitializedLength()), temp2);
    masm.branch32(Assembler::NotEqual, Address(temp1, ObjectElements::offsetOfLength()), temp2, &fail);

    masm.loadPtr(Address(rhs, JSObject::offsetOfElements()), temp1);
    masm.load32(Address(temp1, ObjectElements::offsetOfInitializedLength()), temp2);
    masm.branch32(Assembler::NotEqual, Address(temp1, ObjectElements::offsetOfLength()), temp2, &fail);

    // Try to allocate an object.
    JSObject *templateObj = lir->mir()->templateObj();
    masm.newGCThing(temp1, templateObj, &fail);
    masm.initGCThing(temp1, templateObj);
    masm.jump(&call);
    {
        masm.bind(&fail);
        masm.movePtr(ImmWord((void *)NULL), temp1);
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

    // Fetch the most recent iterator and ensure it's not NULL.
    masm.loadPtr(AbsoluteAddress(&GetIonContext()->runtime->nativeIterCache.last), output);
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

    // Ensure the object's prototype's prototype is NULL. The last native iterator
    // will always have a prototype chain length of one (i.e. it must be a plain
    // object), so we do not need to generate a loop here.
    masm.loadObjProto(obj, temp1);
    masm.loadObjProto(temp1, temp1);
    masm.branchTestPtr(Assembler::NonZero, temp1, temp1, ool->entry());

    // Ensure the object does not have any elements. The presence of dense
    // elements is not captured by the shape tests above.
    masm.branchPtr(Assembler::NotEqual,
                   Address(obj, JSObject::offsetOfElements()),
                   ImmWord(js::emptyObjectElements),
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
    masm.movePtr(ImmWord(gen->compartment), temp1);
    masm.loadPtr(Address(temp1, offsetof(JSCompartment, enumerators)), temp1);

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
static const VMFunction IteratorMoreInfo = FunctionInfo<IteratorMoreFn>(ion::IteratorMore);

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
    masm.cmpPtr(Address(output, offsetof(NativeIterator, props_cursor)), temp);
    masm.emitSet(Assembler::LessThan, output);

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
    masm.storePtr(ImmWord(uintptr_t(0)), Address(temp1, NativeIterator::offsetOfNext()));
    masm.storePtr(ImmWord(uintptr_t(0)), Address(temp1, NativeIterator::offsetOfPrev()));
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
CodeGenerator::visitGetArgument(LGetArgument *lir)
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
typedef ParallelResult (*InitRestParameterParFn)(ForkJoinSlice *, uint32_t, Value *,
                                                 HandleObject, HandleObject,
                                                 MutableHandleObject);
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
    masm.newGCThing(temp2, templateObject, &failAlloc);
    masm.initGCThing(temp2, templateObject);
    masm.jump(&joinAlloc);
    {
        masm.bind(&failAlloc);
        masm.movePtr(ImmWord((void *)NULL), temp2);
    }
    masm.bind(&joinAlloc);

    return emitRest(lir, temp2, numActuals, temp0, temp1, numFormals, templateObject);
}

bool
CodeGenerator::visitRestPar(LRestPar *lir)
{
    Register numActuals = ToRegister(lir->numActuals());
    Register slice = ToRegister(lir->forkJoinSlice());
    Register temp0 = ToRegister(lir->getTemp(0));
    Register temp1 = ToRegister(lir->getTemp(1));
    Register temp2 = ToRegister(lir->getTemp(2));
    unsigned numFormals = lir->mir()->numFormals();
    JSObject *templateObject = lir->mir()->templateObject();

    if (!emitAllocateGCThingPar(lir, temp2, slice, temp0, temp1, templateObject))
        return false;

    return emitRest(lir, temp2, numActuals, temp0, temp1, numFormals, templateObject);
}

bool
CodeGenerator::generateAsmJS()
{
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
    JS_ASSERT(snapshots_.size() == 0);
    JS_ASSERT(bailouts_.empty());
    JS_ASSERT(graph.numConstants() == 0);
    JS_ASSERT(safepointIndices_.empty());
    JS_ASSERT(osiIndices_.empty());
    JS_ASSERT(cacheList_.empty());
    JS_ASSERT(safepoints_.size() == 0);
    JS_ASSERT(graph.mir().numScripts() == 0);
    return true;
}

bool
CodeGenerator::generate()
{
    if (!safepoints_.init(graph.totalSlotCount()))
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
        deoptTable_ = gen->ionRuntime()->getBailoutTable(frameClass_);
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

    if (!generatePrologue())
        return false;
    if (!generateBody())
        return false;
    if (!generateEpilogue())
        return false;
    if (!generateInvalidateEpilogue())
        return false;
    if (!generateOutOfLineCode())
        return false;

    return !masm.oom();
}

bool
CodeGenerator::link()
{
    JSContext *cx = GetIonContext()->cx;

    Linker linker(masm);
    IonCode *code = linker.newCode(cx, JSC::ION_CODE);
    if (!code)
        return false;

    // We encode safepoints after the OSI-point offsets have been determined.
    encodeSafepoints();

    JSScript *script = gen->info().script();
    ExecutionMode executionMode = gen->info().executionMode();
    JS_ASSERT(!HasIonScript(script, executionMode));

    uint32_t scriptFrameSize = frameClass_ == FrameSizeClass::None()
                           ? frameDepth_
                           : FrameSizeClass::FromDepth(frameDepth_).frameSize();

    // Check to make sure we didn't have a mid-build invalidation. If so, we
    // will trickle to ion::Compile() and return Method_Skipped.
    if (cx->compartment()->types.compiledInfo.compilerOutput(cx)->isInvalidated())
        return true;

    // List of possible scripts that this graph may call. Currently this is
    // only tracked when compiling for parallel execution.
    CallTargetVector callTargets;
    if (executionMode == ParallelExecution)
        AddPossibleCallees(graph.mir(), callTargets);

    IonScript *ionScript =
      IonScript::New(cx, graph.totalSlotCount(), scriptFrameSize, snapshots_.size(),
                     bailouts_.length(), graph.numConstants(),
                     safepointIndices_.length(), osiIndices_.length(),
                     cacheList_.length(), runtimeData_.length(),
                     safepoints_.size(), graph.mir().numScripts(),
                     callTargets.length());

    ionScript->setMethod(code);
    ionScript->setSkipArgCheckEntryOffset(getSkipArgCheckEntryOffset());

    // If SPS is enabled, mark IonScript as having been instrumented with SPS
    if (sps_.enabled())
        ionScript->setHasSPSInstrumentation();

    SetIonScript(script, executionMode, ionScript);

    // In parallel execution mode, when we first compile a script, we
    // don't know that its potential callees are compiled, so set a
    // flag warning that the callees may not be fully compiled.
    if (callTargets.length() != 0)
        ionScript->setHasUncompiledCallTarget();

    if (!ionScript)
        return false;
    invalidateEpilogueData_.fixup(&masm);
    Assembler::patchDataWithValueCheck(CodeLocationLabel(code, invalidateEpilogueData_),
                                       ImmWord(uintptr_t(ionScript)),
                                       ImmWord(uintptr_t(-1)));

    IonSpew(IonSpew_Codegen, "Created IonScript %p (raw %p)",
            (void *) ionScript, (void *) code->raw());

    ionScript->setInvalidationEpilogueDataOffset(invalidateEpilogueData_.offset());
    ionScript->setOsrPc(gen->info().osrPc());
    ionScript->setOsrEntryOffset(getOsrEntryOffset());
    ptrdiff_t real_invalidate = masm.actualOffset(invalidate_.offset());
    ionScript->setInvalidationEpilogueOffset(real_invalidate);

    ionScript->setDeoptTable(deoptTable_);

    if (PerfEnabled())
        perfSpewer_.writeProfile(script, code, masm);

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
    if (snapshots_.size())
        ionScript->copySnapshots(&snapshots_);
    if (graph.numConstants())
        ionScript->copyConstants(graph.constantPool());
    JS_ASSERT(graph.mir().numScripts() > 0);
    ionScript->copyScriptEntries(graph.mir().scripts());
    if (callTargets.length() > 0)
        ionScript->copyCallTargetEntries(callTargets.begin());

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

// An out-of-line path to convert a boxed int32 to a double.
class OutOfLineUnboxDouble : public OutOfLineCodeBase<CodeGenerator>
{
    LUnboxDouble *unboxDouble_;

  public:
    OutOfLineUnboxDouble(LUnboxDouble *unboxDouble)
      : unboxDouble_(unboxDouble)
    { }

    bool accept(CodeGenerator *codegen) {
        return codegen->visitOutOfLineUnboxDouble(this);
    }

    LUnboxDouble *unboxDouble() const {
        return unboxDouble_;
    }
};

bool
CodeGenerator::visitUnboxDouble(LUnboxDouble *lir)
{
    const ValueOperand box = ToValue(lir, LUnboxDouble::Input);
    const LDefinition *result = lir->output();

    // Out-of-line path to convert int32 to double or bailout
    // if this instruction is fallible.
    OutOfLineUnboxDouble *ool = new OutOfLineUnboxDouble(lir);
    if (!addOutOfLineCode(ool))
        return false;

    masm.branchTestDouble(Assembler::NotEqual, box, ool->entry());
    masm.unboxDouble(box, ToFloatRegister(result));
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGenerator::visitOutOfLineUnboxDouble(OutOfLineUnboxDouble *ool)
{
    LUnboxDouble *ins = ool->unboxDouble();
    const ValueOperand value = ToValue(ins, LUnboxDouble::Input);

    if (ins->mir()->fallible()) {
        Assembler::Condition cond = masm.testInt32(Assembler::NotEqual, value);
        if (!bailoutIf(cond, ins->snapshot()))
            return false;
    }
    masm.int32ValueToDouble(value, ToFloatRegister(ins->output()));
    masm.jump(ool->rejoin());
    return true;
}

typedef bool (*GetPropertyFn)(JSContext *, HandleValue, HandlePropertyName, MutableHandleValue);
static const VMFunction GetPropertyInfo = FunctionInfo<GetPropertyFn>(GetProperty);

bool
CodeGenerator::visitCallGetProperty(LCallGetProperty *lir)
{
    pushArg(ImmGCPtr(lir->mir()->name()));
    pushArg(ToValue(lir, LCallGetProperty::Value));
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
    pushArg(ImmWord(lir->mir()->resumePoint()->pc()));
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
CodeGenerator::visitCallsiteCloneIC(OutOfLineUpdateCache *ool, CallsiteCloneIC *ic)
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
CodeGenerator::visitNameIC(OutOfLineUpdateCache *ool, NameIC *ic)
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
                                   bool allowGetters)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
        GetPropertyIC cache(liveRegs, objReg, name, output, allowGetters);
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
CodeGenerator::visitGetPropertyCacheV(LGetPropertyCacheV *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    PropertyName *name = ins->mir()->name();
    bool allowGetters = ins->mir()->allowGetters();
    TypedOrValueRegister output = TypedOrValueRegister(GetValueOutput(ins));

    return addGetPropertyCache(ins, liveRegs, objReg, name, output, allowGetters);
}

bool
CodeGenerator::visitGetPropertyCacheT(LGetPropertyCacheT *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    PropertyName *name = ins->mir()->name();
    bool allowGetters = ins->mir()->allowGetters();
    TypedOrValueRegister output(ins->mir()->type(), ToAnyRegister(ins->getDef(0)));

    return addGetPropertyCache(ins, liveRegs, objReg, name, output, allowGetters);
}

typedef bool (*GetPropertyICFn)(JSContext *, size_t, HandleObject, MutableHandleValue);
const VMFunction GetPropertyIC::UpdateInfo =
    FunctionInfo<GetPropertyICFn>(GetPropertyIC::update);

bool
CodeGenerator::visitGetPropertyIC(OutOfLineUpdateCache *ool, GetPropertyIC *ic)
{
    LInstruction *lir = ool->lir();
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

typedef ParallelResult (*GetPropertyParICFn)(ForkJoinSlice *, size_t, HandleObject,
                                             MutableHandleValue);
const VMFunction GetPropertyParIC::UpdateInfo =
    FunctionInfo<GetPropertyParICFn>(GetPropertyParIC::update);

bool
CodeGenerator::visitGetPropertyParIC(OutOfLineUpdateCache *ool, GetPropertyParIC *ic)
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
                                  TypedOrValueRegister output, bool monitoredResult)
{
    switch (gen->info().executionMode()) {
      case SequentialExecution: {
        GetElementIC cache(obj, index, output, monitoredResult);
        return addCache(ins, allocateCache(cache));
      }
      case ParallelExecution: {
        GetElementParIC cache(obj, index, output, monitoredResult);
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

    return addGetElementCache(ins, obj, index, output, ins->mir()->monitoredResult());
}

bool
CodeGenerator::visitGetElementCacheT(LGetElementCacheT *ins)
{
    Register obj = ToRegister(ins->object());
    ConstantOrRegister index = TypedOrValueRegister(MIRType_Int32, ToAnyRegister(ins->index()));
    TypedOrValueRegister output(ins->mir()->type(), ToAnyRegister(ins->output()));

    return addGetElementCache(ins, obj, index, output, ins->mir()->monitoredResult());
}

typedef bool (*GetElementICFn)(JSContext *, size_t, HandleObject, HandleValue, MutableHandleValue);
const VMFunction GetElementIC::UpdateInfo =
    FunctionInfo<GetElementICFn>(GetElementIC::update);

bool
CodeGenerator::visitGetElementIC(OutOfLineUpdateCache *ool, GetElementIC *ic)
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
    ValueOperand index = ToValue(ins, LSetElementCacheV::Index);
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LSetElementCacheV::Value));

    SetElementIC cache(obj, unboxIndex, temp, index, value, ins->mir()->strict());

    return addCache(ins, allocateCache(cache));
}

bool
CodeGenerator::visitSetElementCacheT(LSetElementCacheT *ins)
{
    Register obj = ToRegister(ins->object());
    Register unboxIndex = ToTempUnboxRegister(ins->tempToUnboxIndex());
    Register temp = ToRegister(ins->temp());
    ValueOperand index = ToValue(ins, LSetElementCacheT::Index);
    ConstantOrRegister value;
    const LAllocation *tmp = ins->value();
    if (tmp->isConstant())
        value = *tmp->toConstant();
    else
        value = TypedOrValueRegister(ins->mir()->value()->type(), ToAnyRegister(tmp));

    SetElementIC cache(obj, unboxIndex, temp, index, value, ins->mir()->strict());

    return addCache(ins, allocateCache(cache));
}

typedef bool (*SetElementICFn)(JSContext *, size_t, HandleObject, HandleValue, HandleValue);
const VMFunction SetElementIC::UpdateInfo =
    FunctionInfo<SetElementICFn>(SetElementIC::update);

bool
CodeGenerator::visitSetElementIC(OutOfLineUpdateCache *ool, SetElementIC *ic)
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

typedef ParallelResult (*GetElementParICFn)(ForkJoinSlice *, size_t, HandleObject,
                                            HandleValue, MutableHandleValue);
const VMFunction GetElementParIC::UpdateInfo =
    FunctionInfo<GetElementParICFn>(GetElementParIC::update);

bool
CodeGenerator::visitGetElementParIC(OutOfLineUpdateCache *ool, GetElementParIC *ic)
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
CodeGenerator::visitBindNameIC(OutOfLineUpdateCache *ool, BindNameIC *ic)
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
                              HandlePropertyName, const HandleValue, bool, int);
static const VMFunction SetPropertyInfo =
    FunctionInfo<SetPropertyFn>(SetProperty);

bool
CodeGenerator::visitCallSetProperty(LCallSetProperty *ins)
{
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LCallSetProperty::Value));

    const Register objReg = ToRegister(ins->getOperand(0));
    JSOp op = JSOp(*ins->mir()->resumePoint()->pc());

    pushArg(Imm32(op));
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

    if (lir->mir()->block()->info().script()->strict)
        return callVM(DeletePropertyStrictInfo, lir);

    return callVM(DeletePropertyNonStrictInfo, lir);
}

bool
CodeGenerator::visitSetPropertyCacheV(LSetPropertyCacheV *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    ConstantOrRegister value = TypedOrValueRegister(ToValue(ins, LSetPropertyCacheV::Value));
    jsbytecode *pc = ins->mir()->resumePoint()->pc();
    bool isSetName = JSOp(*pc) == JSOP_SETNAME || JSOp(*pc) == JSOP_SETGNAME;

    SetPropertyIC cache(liveRegs, objReg, ins->mir()->name(), value,
                        isSetName, ins->mir()->strict());
    return addCache(ins, allocateCache(cache));
}

bool
CodeGenerator::visitSetPropertyCacheT(LSetPropertyCacheT *ins)
{
    RegisterSet liveRegs = ins->safepoint()->liveRegs();
    Register objReg = ToRegister(ins->getOperand(0));
    ConstantOrRegister value;
    jsbytecode *pc = ins->mir()->resumePoint()->pc();
    bool isSetName = JSOp(*pc) == JSOP_SETNAME || JSOp(*pc) == JSOP_SETGNAME;

    if (ins->getOperand(1)->isConstant())
        value = ConstantOrRegister(*ins->getOperand(1)->toConstant());
    else
        value = TypedOrValueRegister(ins->valueType(), ToAnyRegister(ins->getOperand(1)));

    SetPropertyIC cache(liveRegs, objReg, ins->mir()->name(), value,
                        isSetName, ins->mir()->strict());
    return addCache(ins, allocateCache(cache));
}

typedef bool (*SetPropertyICFn)(JSContext *, size_t, HandleObject, HandleValue);
const VMFunction SetPropertyIC::UpdateInfo =
    FunctionInfo<SetPropertyICFn>(SetPropertyIC::update);

bool
CodeGenerator::visitSetPropertyIC(OutOfLineUpdateCache *ool, SetPropertyIC *ic)
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

typedef bool (*ThrowFn)(JSContext *, HandleValue);
static const VMFunction ThrowInfo = FunctionInfo<ThrowFn>(js::Throw);

bool
CodeGenerator::visitThrow(LThrow *lir)
{
    pushArg(ToValue(lir, LThrow::Value));
    return callVM(ThrowInfo, lir);
}

typedef bool (*BitNotFn)(JSContext *, HandleValue, int *p);
typedef ParallelResult (*BitNotParFn)(ForkJoinSlice *, HandleValue, int32_t *);
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
typedef ParallelResult (*BitopParFn)(ForkJoinSlice *, HandleValue, HandleValue, int32_t *);
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

    OutOfLineTypeOfV *ool = new OutOfLineTypeOfV(lir);
    if (!addOutOfLineCode(ool))
        return false;

    JSRuntime *rt = GetIonContext()->runtime;

    // Jump to the OOL path if the value is an object. Objects are complicated
    // since they may have a typeof hook.
    masm.branchTestObject(Assembler::Equal, tag, ool->entry());

    Label done;

    Label notNumber;
    masm.branchTestNumber(Assembler::NotEqual, tag, &notNumber);
    masm.movePtr(ImmGCPtr(rt->atomState.number), output);
    masm.jump(&done);
    masm.bind(&notNumber);

    Label notUndefined;
    masm.branchTestUndefined(Assembler::NotEqual, tag, &notUndefined);
    masm.movePtr(ImmGCPtr(rt->atomState.undefined), output);
    masm.jump(&done);
    masm.bind(&notUndefined);

    Label notNull;
    masm.branchTestNull(Assembler::NotEqual, tag, &notNull);
    masm.movePtr(ImmGCPtr(rt->atomState.object), output);
    masm.jump(&done);
    masm.bind(&notNull);

    Label notBoolean;
    masm.branchTestBoolean(Assembler::NotEqual, tag, &notBoolean);
    masm.movePtr(ImmGCPtr(rt->atomState.boolean), output);
    masm.jump(&done);
    masm.bind(&notBoolean);

    masm.movePtr(ImmGCPtr(rt->atomState.string), output);

    masm.bind(&done);
    masm.bind(ool->rejoin());
    return true;
}

typedef JSString *(*TypeOfFn)(JSContext *, HandleValue);
static const VMFunction TypeOfInfo = FunctionInfo<TypeOfFn>(TypeOfOperation);

bool
CodeGenerator::visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool)
{
    LTypeOfV *ins = ool->ins();
    saveLive(ins);

    pushArg(ToValue(ins, LTypeOfV::Input));
    if (!callVM(TypeOfInfo, ins))
        return false;

    masm.storeCallResult(ToRegister(ins->output()));
    restoreLive(ins);

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
                                   ImmWord(lir->mir()->resumePoint()->pc()),
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
        Assembler::Condition cond = masm.testMagic(Assembler::Equal, out);
        if (!bailoutIf(cond, load->snapshot()))
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
    if (arrayType == TypedArrayObject::TYPE_FLOAT32 || arrayType == TypedArrayObject::TYPE_FLOAT64) {
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
    Register input = ToRegister(lir->input());
    Register output = ToRegister(lir->output());
    masm.clampIntToUint8(input, output);
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
    ValueOperand input = ToValue(lir, LClampVToUint8::Input);
    FloatRegister tempFloat = ToFloatRegister(lir->tempFloat());
    Register output = ToRegister(lir->output());

    Register tag = masm.splitTagForTest(input);

    Label done;
    Label isInt32, isDouble, isBoolean;
    masm.branchTestInt32(Assembler::Equal, tag, &isInt32);
    masm.branchTestDouble(Assembler::Equal, tag, &isDouble);
    masm.branchTestBoolean(Assembler::Equal, tag, &isBoolean);

    // Undefined, null and objects are always 0.
    Label isZero;
    masm.branchTestUndefined(Assembler::Equal, tag, &isZero);
    masm.branchTestNull(Assembler::Equal, tag, &isZero);
    masm.branchTestObject(Assembler::Equal, tag, &isZero);

    // Bailout for everything else (strings).
    if (!bailout(lir->snapshot()))
        return false;

    masm.bind(&isInt32);
    masm.unboxInt32(input, output);
    masm.clampIntToUint8(output, output);
    masm.jump(&done);

    masm.bind(&isDouble);
    masm.unboxDouble(input, tempFloat);
    masm.clampDoubleToUint8(tempFloat, output);
    masm.jump(&done);

    masm.bind(&isBoolean);
    masm.unboxBoolean(input, output);
    masm.jump(&done);

    masm.bind(&isZero);
    masm.move32(Imm32(0), output);

    masm.bind(&done);
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

    OutOfLineCode *ool = NULL;
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

// Wrap IsDelegate, which takes a Value for the lhs of an instanceof.
static bool
IsDelegateObject(JSContext *cx, HandleObject protoObj, HandleObject obj, bool *res)
{
    return IsDelegate(cx, protoObj, ObjectValue(*obj), res);
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
        masm.mov(Imm32(0), output);
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
    masm.loadPtr(Address(objReg, JSObject::offsetOfType()), output);
    masm.loadPtr(Address(output, offsetof(types::TypeObject, proto)), output);

    Label testLazy;
    {
        Label loopPrototypeChain;
        masm.bind(&loopPrototypeChain);

        // Test for the target prototype object.
        Label notPrototypeObject;
        masm.branchPtr(Assembler::NotEqual, output, ImmGCPtr(prototypeObject), &notPrototypeObject);
        masm.mov(Imm32(1), output);
        masm.jump(&done);
        masm.bind(&notPrototypeObject);

        JS_ASSERT(uintptr_t(Proxy::LazyProto) == 1);

        // Test for NULL or Proxy::LazyProto
        masm.branchPtr(Assembler::BelowOrEqual, output, ImmWord(1), &testLazy);

        // Load the current object's prototype.
        masm.loadPtr(Address(output, JSObject::offsetOfType()), output);
        masm.loadPtr(Address(output, offsetof(types::TypeObject, proto)), output);

        masm.jump(&loopPrototypeChain);
    }

    // Make a VM call if an object with a lazy proto was found on the prototype
    // chain. This currently occurs only for cross compartment wrappers, which
    // we do not expect to be compared with non-wrapper functions from this
    // compartment. Otherwise, we stopped on a NULL prototype and the output
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
CodeGenerator::visitFunctionBoundary(LFunctionBoundary *lir)
{
    Register temp = ToRegister(lir->temp()->output());

    switch (lir->type()) {
        case MFunctionBoundary::Inline_Enter:
            // Multiple scripts can be inlined at one depth, but there is only
            // one Inline_Exit node to signify this. To deal with this, if we
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

            sps_.leave(masm, temp);
            if (!sps_.enterInlineFrame())
                return false;
            // fallthrough

        case MFunctionBoundary::Enter:
            if (sps_.slowAssertions()) {
                saveLive(lir);
                pushArg(ImmGCPtr(lir->script()));
                if (!callVM(SPSEnterInfo, lir))
                    return false;
                restoreLive(lir);
                sps_.pushManual(lir->script(), masm, temp);
                return true;
            }

            return sps_.push(GetIonContext()->cx, lir->script(), masm, temp);

        case MFunctionBoundary::Inline_Exit:
            // all inline returns were covered with ::Exit, so we just need to
            // maintain the state of inline frames currently active and then
            // reenter the caller
            sps_.leaveInlineFrame();
            sps_.reenter(masm, temp);
            return true;

        case MFunctionBoundary::Exit:
            if (sps_.slowAssertions()) {
                saveLive(lir);
                pushArg(ImmGCPtr(lir->script()));
                // Once we've exited, then we shouldn't emit instrumentation for
                // the corresponding reenter() because we no longer have a
                // frame.
                sps_.skipNextReenter();
                if (!callVM(SPSExitInfo, lir))
                    return false;
                restoreLive(lir);
                return true;
            }

            sps_.pop(masm, temp);
            return true;

        default:
            MOZ_ASSUME_UNREACHABLE("invalid LFunctionBoundary type");
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
    masm.movePtr(ImmWord((void *) bytecode), CallTempReg3);

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
    Label notFunction, done;
    masm.branchPtr(Assembler::NotEqual, output, ImmWord(&JSFunction::class_), &notFunction);
    masm.move32(Imm32(1), output);
    masm.jump(&done);

    masm.bind(&notFunction);
    masm.cmpPtr(Address(output, offsetof(js::Class, call)), ImmWord((void *)NULL));
    masm.emitSet(Assembler::NonZero, output);
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
    masm.cmpPtr(temp, output);
    masm.emitSet(Assembler::Equal, output);

    return true;
}

bool
CodeGenerator::visitAsmJSCall(LAsmJSCall *ins)
{
    MAsmJSCall *mir = ins->mir();

#if defined(JS_CPU_ARM) && !defined(JS_CPU_ARM_HARDFP)
    for (unsigned i = 0, e = ins->numOperands(); i < e; i++) {
        LAllocation *a = ins->getOperand(i);
        if (a->isFloatReg()) {
            FloatRegister fr = ToFloatRegister(a);
            int srcId = fr.code() * 2;
            masm.ma_vxfer(fr, Register::FromCode(srcId), Register::FromCode(srcId+1));
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
    masm.breakpoint();
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
        masm.call(ImmWord(callee.builtin()));
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
#if defined(JS_CPU_ARM) && !defined(JS_CPU_ARM_HARDFP)
    // softfp transfers some double values in gprs.
    // undo this.
    LAllocation *a = lir->getDef(0)->output();
    if (a->isFloatReg()) {
        FloatRegister fr = ToFloatRegister(a);
        int srcId = fr.code() * 2;
        masm.ma_vxfer(Register::FromCode(srcId), Register::FromCode(srcId+1), fr);
    }
#endif
    return true;
}

bool
CodeGenerator::visitAsmJSReturn(LAsmJSReturn *lir)
{
    // Don't emit a jump to the return label if this is the last block.
#if defined(JS_CPU_ARM) && !defined(JS_CPU_ARM_HARDFP)
    if (lir->getOperand(0)->isFloatReg())
        masm.ma_vxfer(d0, r0, r1);
#endif
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
    uintptr_t *limitAddr = &GetIonContext()->runtime->mainThread.nativeStackLimit;
    masm.branchPtr(Assembler::AboveOrEqual,
                   AbsoluteAddress(limitAddr),
                   StackPointer,
                   lir->mir()->onError());
    return true;
}

} // namespace ion
} // namespace js
