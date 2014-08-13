/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CodeGenerator_h
#define jit_CodeGenerator_h

#include "jit/IonCaches.h"
#if defined(JS_ION_PERF)
# include "jit/PerfSpewer.h"
#endif

#if defined(JS_CODEGEN_X86)
# include "jit/x86/CodeGenerator-x86.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/CodeGenerator-x64.h"
#elif defined(JS_CODEGEN_ARM)
# include "jit/arm/CodeGenerator-arm.h"
#elif defined(JS_CODEGEN_MIPS)
# include "jit/mips/CodeGenerator-mips.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/CodeGenerator-none.h"
#else
#error "Unknown architecture!"
#endif

namespace js {
namespace jit {

class OutOfLineTestObject;
class OutOfLineNewArray;
class OutOfLineNewObject;
class CheckOverRecursedFailure;
class CheckOverRecursedFailurePar;
class OutOfLineInterruptCheckPar;
class OutOfLineInterruptCheckImplicit;
class OutOfLineUnboxFloatingPoint;
class OutOfLineStoreElementHole;
class OutOfLineTypeOfV;
class OutOfLineLoadTypedArray;
class OutOfLineNewGCThingPar;
class OutOfLineUpdateCache;
class OutOfLineCallPostWriteBarrier;

class CodeGenerator : public CodeGeneratorSpecific
{
    bool generateArgumentsChecks(bool bailout = true);
    bool generateBody();

  public:
    CodeGenerator(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm = nullptr);
    ~CodeGenerator();

  public:
    bool generate();
    bool generateAsmJS(AsmJSFunctionLabels *labels);
    bool link(JSContext *cx, types::CompilerConstraintList *constraints);

    bool visitLabel(LLabel *lir);
    bool visitNop(LNop *lir);
    bool visitOsiPoint(LOsiPoint *lir);
    bool visitGoto(LGoto *lir);
    bool visitTableSwitch(LTableSwitch *ins);
    bool visitTableSwitchV(LTableSwitchV *ins);
    bool visitCloneLiteral(LCloneLiteral *lir);
    bool visitParameter(LParameter *lir);
    bool visitCallee(LCallee *lir);
    bool visitStart(LStart *lir);
    bool visitReturn(LReturn *ret);
    bool visitDefVar(LDefVar *lir);
    bool visitDefFun(LDefFun *lir);
    bool visitOsrEntry(LOsrEntry *lir);
    bool visitOsrScopeChain(LOsrScopeChain *lir);
    bool visitOsrValue(LOsrValue *lir);
    bool visitOsrReturnValue(LOsrReturnValue *lir);
    bool visitOsrArgumentsObject(LOsrArgumentsObject *lir);
    bool visitStackArgT(LStackArgT *lir);
    bool visitStackArgV(LStackArgV *lir);
    bool visitMoveGroup(LMoveGroup *group);
    bool visitValueToInt32(LValueToInt32 *lir);
    bool visitValueToDouble(LValueToDouble *lir);
    bool visitValueToFloat32(LValueToFloat32 *lir);
    bool visitFloat32ToDouble(LFloat32ToDouble *lir);
    bool visitDoubleToFloat32(LDoubleToFloat32 *lir);
    bool visitInt32ToFloat32(LInt32ToFloat32 *lir);
    bool visitInt32ToDouble(LInt32ToDouble *lir);
    void emitOOLTestObject(Register objreg, Label *ifTruthy, Label *ifFalsy, Register scratch);
    bool visitTestOAndBranch(LTestOAndBranch *lir);
    bool visitTestVAndBranch(LTestVAndBranch *lir);
    bool visitFunctionDispatch(LFunctionDispatch *lir);
    bool visitTypeObjectDispatch(LTypeObjectDispatch *lir);
    bool visitBooleanToString(LBooleanToString *lir);
    void emitIntToString(Register input, Register output, Label *ool);
    bool visitIntToString(LIntToString *lir);
    bool visitDoubleToString(LDoubleToString *lir);
    bool visitValueToString(LValueToString *lir);
    bool visitInteger(LInteger *lir);
    bool visitRegExp(LRegExp *lir);
    bool visitRegExpExec(LRegExpExec *lir);
    bool visitRegExpTest(LRegExpTest *lir);
    bool visitRegExpReplace(LRegExpReplace *lir);
    bool visitStringReplace(LStringReplace *lir);
    bool visitLambda(LLambda *lir);
    bool visitLambdaArrow(LLambdaArrow *lir);
    bool visitLambdaForSingleton(LLambdaForSingleton *lir);
    bool visitLambdaPar(LLambdaPar *lir);
    bool visitPointer(LPointer *lir);
    bool visitSlots(LSlots *lir);
    bool visitLoadSlotT(LLoadSlotT *lir);
    bool visitLoadSlotV(LLoadSlotV *lir);
    bool visitStoreSlotT(LStoreSlotT *lir);
    bool visitStoreSlotV(LStoreSlotV *lir);
    bool visitElements(LElements *lir);
    bool visitConvertElementsToDoubles(LConvertElementsToDoubles *lir);
    bool visitMaybeToDoubleElement(LMaybeToDoubleElement *lir);
    bool visitGuardObjectIdentity(LGuardObjectIdentity *guard);
    bool visitGuardShapePolymorphic(LGuardShapePolymorphic *lir);
    bool visitTypeBarrierV(LTypeBarrierV *lir);
    bool visitTypeBarrierO(LTypeBarrierO *lir);
    bool visitMonitorTypes(LMonitorTypes *lir);
    bool visitPostWriteBarrierO(LPostWriteBarrierO *lir);
    bool visitPostWriteBarrierV(LPostWriteBarrierV *lir);
    bool visitOutOfLineCallPostWriteBarrier(OutOfLineCallPostWriteBarrier *ool);
    bool visitCallNative(LCallNative *call);
    bool emitCallInvokeFunction(LInstruction *call, Register callereg,
                                uint32_t argc, uint32_t unusedStack);
    bool visitCallGeneric(LCallGeneric *call);
    bool visitCallKnown(LCallKnown *call);
    bool emitCallInvokeFunction(LApplyArgsGeneric *apply, Register extraStackSize);
    void emitPushArguments(LApplyArgsGeneric *apply, Register extraStackSpace);
    void emitPopArguments(LApplyArgsGeneric *apply, Register extraStackSize);
    bool visitApplyArgsGeneric(LApplyArgsGeneric *apply);
    bool visitBail(LBail *lir);
    bool visitUnreachable(LUnreachable *unreachable);
    bool visitGetDynamicName(LGetDynamicName *lir);
    bool visitFilterArgumentsOrEvalS(LFilterArgumentsOrEvalS *lir);
    bool visitFilterArgumentsOrEvalV(LFilterArgumentsOrEvalV *lir);
    bool visitCallDirectEvalS(LCallDirectEvalS *lir);
    bool visitCallDirectEvalV(LCallDirectEvalV *lir);
    bool visitDoubleToInt32(LDoubleToInt32 *lir);
    bool visitFloat32ToInt32(LFloat32ToInt32 *lir);
    bool visitNewArrayCallVM(LNewArray *lir);
    bool visitNewArray(LNewArray *lir);
    bool visitOutOfLineNewArray(OutOfLineNewArray *ool);
    bool visitNewObjectVMCall(LNewObject *lir);
    bool visitNewObject(LNewObject *lir);
    bool visitOutOfLineNewObject(OutOfLineNewObject *ool);
    bool visitNewDeclEnvObject(LNewDeclEnvObject *lir);
    bool visitNewCallObject(LNewCallObject *lir);
    bool visitNewSingletonCallObject(LNewSingletonCallObject *lir);
    bool visitNewCallObjectPar(LNewCallObjectPar *lir);
    bool visitNewStringObject(LNewStringObject *lir);
    bool visitNewPar(LNewPar *lir);
    bool visitNewDenseArrayPar(LNewDenseArrayPar *lir);
    bool visitNewDerivedTypedObject(LNewDerivedTypedObject *lir);
    bool visitInitElem(LInitElem *lir);
    bool visitInitElemGetterSetter(LInitElemGetterSetter *lir);
    bool visitMutateProto(LMutateProto *lir);
    bool visitInitProp(LInitProp *lir);
    bool visitInitPropGetterSetter(LInitPropGetterSetter *lir);
    bool visitCreateThis(LCreateThis *lir);
    bool visitCreateThisWithProto(LCreateThisWithProto *lir);
    bool visitCreateThisWithTemplate(LCreateThisWithTemplate *lir);
    bool visitCreateArgumentsObject(LCreateArgumentsObject *lir);
    bool visitGetArgumentsObjectArg(LGetArgumentsObjectArg *lir);
    bool visitSetArgumentsObjectArg(LSetArgumentsObjectArg *lir);
    bool visitReturnFromCtor(LReturnFromCtor *lir);
    bool visitComputeThis(LComputeThis *lir);
    bool visitLoadArrowThis(LLoadArrowThis *lir);
    bool visitArrayLength(LArrayLength *lir);
    bool visitSetArrayLength(LSetArrayLength *lir);
    bool visitTypedArrayLength(LTypedArrayLength *lir);
    bool visitTypedArrayElements(LTypedArrayElements *lir);
    bool visitNeuterCheck(LNeuterCheck *lir);
    bool visitTypedObjectElements(LTypedObjectElements *lir);
    bool visitSetTypedObjectOffset(LSetTypedObjectOffset *lir);
    bool visitTypedObjectProto(LTypedObjectProto *ins);
    bool visitStringLength(LStringLength *lir);
    bool visitInitializedLength(LInitializedLength *lir);
    bool visitSetInitializedLength(LSetInitializedLength *lir);
    bool visitNotO(LNotO *ins);
    bool visitNotV(LNotV *ins);
    bool visitBoundsCheck(LBoundsCheck *lir);
    bool visitBoundsCheckRange(LBoundsCheckRange *lir);
    bool visitBoundsCheckLower(LBoundsCheckLower *lir);
    bool visitLoadFixedSlotV(LLoadFixedSlotV *ins);
    bool visitLoadFixedSlotT(LLoadFixedSlotT *ins);
    bool visitStoreFixedSlotV(LStoreFixedSlotV *ins);
    bool visitStoreFixedSlotT(LStoreFixedSlotT *ins);
    bool emitGetPropertyPolymorphic(LInstruction *lir, Register obj,
                                    Register scratch, const TypedOrValueRegister &output);
    bool visitGetPropertyPolymorphicV(LGetPropertyPolymorphicV *ins);
    bool visitGetPropertyPolymorphicT(LGetPropertyPolymorphicT *ins);
    bool emitSetPropertyPolymorphic(LInstruction *lir, Register obj,
                                    Register scratch, const ConstantOrRegister &value);
    bool visitSetPropertyPolymorphicV(LSetPropertyPolymorphicV *ins);
    bool visitArraySplice(LArraySplice *splice);
    bool visitSetPropertyPolymorphicT(LSetPropertyPolymorphicT *ins);
    bool visitAbsI(LAbsI *lir);
    bool visitAtan2D(LAtan2D *lir);
    bool visitHypot(LHypot *lir);
    bool visitPowI(LPowI *lir);
    bool visitPowD(LPowD *lir);
    bool visitRandom(LRandom *lir);
    bool visitMathFunctionD(LMathFunctionD *ins);
    bool visitMathFunctionF(LMathFunctionF *ins);
    bool visitModD(LModD *ins);
    bool visitMinMaxI(LMinMaxI *lir);
    bool visitBinaryV(LBinaryV *lir);
    bool emitCompareS(LInstruction *lir, JSOp op, Register left, Register right, Register output);
    bool visitCompareS(LCompareS *lir);
    bool visitCompareStrictS(LCompareStrictS *lir);
    bool visitCompareVM(LCompareVM *lir);
    bool visitIsNullOrLikeUndefined(LIsNullOrLikeUndefined *lir);
    bool visitIsNullOrLikeUndefinedAndBranch(LIsNullOrLikeUndefinedAndBranch *lir);
    bool visitEmulatesUndefined(LEmulatesUndefined *lir);
    bool visitEmulatesUndefinedAndBranch(LEmulatesUndefinedAndBranch *lir);
    bool emitConcat(LInstruction *lir, Register lhs, Register rhs, Register output);
    bool visitConcat(LConcat *lir);
    bool visitConcatPar(LConcatPar *lir);
    bool visitCharCodeAt(LCharCodeAt *lir);
    bool visitFromCharCode(LFromCharCode *lir);
    bool visitStringSplit(LStringSplit *lir);
    bool visitFunctionEnvironment(LFunctionEnvironment *lir);
    bool visitForkJoinContext(LForkJoinContext *lir);
    bool visitGuardThreadExclusive(LGuardThreadExclusive *lir);
    bool visitCallGetProperty(LCallGetProperty *lir);
    bool visitCallGetElement(LCallGetElement *lir);
    bool visitCallSetElement(LCallSetElement *lir);
    bool visitCallInitElementArray(LCallInitElementArray *lir);
    bool visitThrow(LThrow *lir);
    bool visitTypeOfV(LTypeOfV *lir);
    bool visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool);
    bool visitToIdV(LToIdV *lir);
    template<typename T> bool emitLoadElementT(LLoadElementT *lir, const T &source);
    bool visitLoadElementT(LLoadElementT *lir);
    bool visitLoadElementV(LLoadElementV *load);
    bool visitLoadElementHole(LLoadElementHole *lir);
    bool visitStoreElementT(LStoreElementT *lir);
    bool visitStoreElementV(LStoreElementV *lir);
    bool visitStoreElementHoleT(LStoreElementHoleT *lir);
    bool visitStoreElementHoleV(LStoreElementHoleV *lir);
    bool emitArrayPopShift(LInstruction *lir, const MArrayPopShift *mir, Register obj,
                           Register elementsTemp, Register lengthTemp, TypedOrValueRegister out);
    bool visitArrayPopShiftV(LArrayPopShiftV *lir);
    bool visitArrayPopShiftT(LArrayPopShiftT *lir);
    bool emitArrayPush(LInstruction *lir, const MArrayPush *mir, Register obj,
                       ConstantOrRegister value, Register elementsTemp, Register length);
    bool visitArrayPushV(LArrayPushV *lir);
    bool visitArrayPushT(LArrayPushT *lir);
    bool visitArrayConcat(LArrayConcat *lir);
    bool visitArrayJoin(LArrayJoin *lir);
    bool visitLoadTypedArrayElement(LLoadTypedArrayElement *lir);
    bool visitLoadTypedArrayElementHole(LLoadTypedArrayElementHole *lir);
    bool visitStoreTypedArrayElement(LStoreTypedArrayElement *lir);
    bool visitStoreTypedArrayElementHole(LStoreTypedArrayElementHole *lir);
    bool visitClampIToUint8(LClampIToUint8 *lir);
    bool visitClampDToUint8(LClampDToUint8 *lir);
    bool visitClampVToUint8(LClampVToUint8 *lir);
    bool visitCallIteratorStart(LCallIteratorStart *lir);
    bool visitIteratorStart(LIteratorStart *lir);
    bool visitIteratorNext(LIteratorNext *lir);
    bool visitIteratorMore(LIteratorMore *lir);
    bool visitIteratorEnd(LIteratorEnd *lir);
    bool visitArgumentsLength(LArgumentsLength *lir);
    bool visitGetFrameArgument(LGetFrameArgument *lir);
    bool visitSetFrameArgumentT(LSetFrameArgumentT *lir);
    bool visitSetFrameArgumentC(LSetFrameArgumentC *lir);
    bool visitSetFrameArgumentV(LSetFrameArgumentV *lir);
    bool visitRunOncePrologue(LRunOncePrologue *lir);
    bool emitRest(LInstruction *lir, Register array, Register numActuals,
                  Register temp0, Register temp1, unsigned numFormals,
                  JSObject *templateObject, bool saveAndRestore, Register resultreg);
    bool visitRest(LRest *lir);
    bool visitRestPar(LRestPar *lir);
    bool visitCallSetProperty(LCallSetProperty *ins);
    bool visitCallDeleteProperty(LCallDeleteProperty *lir);
    bool visitCallDeleteElement(LCallDeleteElement *lir);
    bool visitBitNotV(LBitNotV *lir);
    bool visitBitOpV(LBitOpV *lir);
    bool emitInstanceOf(LInstruction *ins, JSObject *prototypeObject);
    bool visitIn(LIn *ins);
    bool visitInArray(LInArray *ins);
    bool visitInstanceOfO(LInstanceOfO *ins);
    bool visitInstanceOfV(LInstanceOfV *ins);
    bool visitCallInstanceOf(LCallInstanceOf *ins);
    bool visitProfilerStackOp(LProfilerStackOp *lir);
    bool visitGetDOMProperty(LGetDOMProperty *lir);
    bool visitGetDOMMember(LGetDOMMember *lir);
    bool visitSetDOMProperty(LSetDOMProperty *lir);
    bool visitCallDOMNative(LCallDOMNative *lir);
    bool visitCallGetIntrinsicValue(LCallGetIntrinsicValue *lir);
    bool visitIsCallable(LIsCallable *lir);
    bool visitIsObject(LIsObject *lir);
    bool visitHaveSameClass(LHaveSameClass *lir);
    bool visitHasClass(LHasClass *lir);
    bool visitAsmJSCall(LAsmJSCall *lir);
    bool visitAsmJSParameter(LAsmJSParameter *lir);
    bool visitAsmJSReturn(LAsmJSReturn *ret);
    bool visitAsmJSVoidReturn(LAsmJSVoidReturn *ret);

    bool visitCheckOverRecursed(LCheckOverRecursed *lir);
    bool visitCheckOverRecursedFailure(CheckOverRecursedFailure *ool);

    bool visitCheckOverRecursedPar(LCheckOverRecursedPar *lir);

    bool visitInterruptCheckPar(LInterruptCheckPar *lir);
    bool visitOutOfLineInterruptCheckPar(OutOfLineInterruptCheckPar *ool);

    bool visitInterruptCheckImplicit(LInterruptCheckImplicit *ins);
    bool visitOutOfLineInterruptCheckImplicit(OutOfLineInterruptCheckImplicit *ins);

    bool visitUnboxFloatingPoint(LUnboxFloatingPoint *lir);
    bool visitOutOfLineUnboxFloatingPoint(OutOfLineUnboxFloatingPoint *ool);
    bool visitOutOfLineStoreElementHole(OutOfLineStoreElementHole *ool);

    bool visitOutOfLineNewGCThingPar(OutOfLineNewGCThingPar *ool);
    void loadJSScriptForBlock(MBasicBlock *block, Register reg);
    void loadOutermostJSScript(Register reg);

    // Inline caches visitors.
    bool visitOutOfLineCache(OutOfLineUpdateCache *ool);

    bool visitGetPropertyCacheV(LGetPropertyCacheV *ins);
    bool visitGetPropertyCacheT(LGetPropertyCacheT *ins);
    bool visitGetElementCacheV(LGetElementCacheV *ins);
    bool visitGetElementCacheT(LGetElementCacheT *ins);
    bool visitSetElementCacheV(LSetElementCacheV *ins);
    bool visitSetElementCacheT(LSetElementCacheT *ins);
    bool visitBindNameCache(LBindNameCache *ins);
    bool visitCallSetProperty(LInstruction *ins);
    bool visitSetPropertyCacheV(LSetPropertyCacheV *ins);
    bool visitSetPropertyCacheT(LSetPropertyCacheT *ins);
    bool visitGetNameCache(LGetNameCache *ins);
    bool visitCallsiteCloneCache(LCallsiteCloneCache *ins);

    bool visitGetPropertyIC(OutOfLineUpdateCache *ool, DataPtr<GetPropertyIC> &ic);
    bool visitGetPropertyParIC(OutOfLineUpdateCache *ool, DataPtr<GetPropertyParIC> &ic);
    bool visitSetPropertyIC(OutOfLineUpdateCache *ool, DataPtr<SetPropertyIC> &ic);
    bool visitSetPropertyParIC(OutOfLineUpdateCache *ool, DataPtr<SetPropertyParIC> &ic);
    bool visitGetElementIC(OutOfLineUpdateCache *ool, DataPtr<GetElementIC> &ic);
    bool visitGetElementParIC(OutOfLineUpdateCache *ool, DataPtr<GetElementParIC> &ic);
    bool visitSetElementIC(OutOfLineUpdateCache *ool, DataPtr<SetElementIC> &ic);
    bool visitSetElementParIC(OutOfLineUpdateCache *ool, DataPtr<SetElementParIC> &ic);
    bool visitBindNameIC(OutOfLineUpdateCache *ool, DataPtr<BindNameIC> &ic);
    bool visitNameIC(OutOfLineUpdateCache *ool, DataPtr<NameIC> &ic);
    bool visitCallsiteCloneIC(OutOfLineUpdateCache *ool, DataPtr<CallsiteCloneIC> &ic);

    bool visitAssertRangeI(LAssertRangeI *ins);
    bool visitAssertRangeD(LAssertRangeD *ins);
    bool visitAssertRangeF(LAssertRangeF *ins);
    bool visitAssertRangeV(LAssertRangeV *ins);

    bool visitInterruptCheck(LInterruptCheck *lir);
    bool visitAsmJSInterruptCheck(LAsmJSInterruptCheck *lir);
    bool visitRecompileCheck(LRecompileCheck *ins);

    IonScriptCounts *extractScriptCounts() {
        IonScriptCounts *counts = scriptCounts_;
        scriptCounts_ = nullptr;  // prevent delete in dtor
        return counts;
    }

  private:
    bool addGetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                             PropertyName *name, TypedOrValueRegister output,
                             bool monitoredResult, jsbytecode *profilerLeavePc);
    bool addGetElementCache(LInstruction *ins, Register obj, ConstantOrRegister index,
                            TypedOrValueRegister output, bool monitoredResult,
                            bool allowDoubleResult, jsbytecode *profilerLeavePc);
    bool addSetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                             PropertyName *name, ConstantOrRegister value, bool strict,
                             bool needsTypeBarrier, jsbytecode *profilerLeavePc);
    bool addSetElementCache(LInstruction *ins, Register obj, Register unboxIndex, Register temp,
                            FloatRegister tempDouble, FloatRegister tempFloat32,
                            ValueOperand index, ConstantOrRegister value,
                            bool strict, bool guardHoles, jsbytecode *profilerLeavePc);

    bool generateBranchV(const ValueOperand &value, Label *ifTrue, Label *ifFalse, FloatRegister fr);

    bool emitAllocateGCThingPar(LInstruction *lir, Register objReg, Register cxReg,
                                Register tempReg1, Register tempReg2,
                                JSObject *templateObj);

    bool emitCallToUncompiledScriptPar(LInstruction *lir, Register calleeReg);

    void emitLambdaInit(Register resultReg, Register scopeChainReg,
                        const LambdaFunctionInfo &info);

    bool emitFilterArgumentsOrEval(LInstruction *lir, Register string, Register temp1,
                                   Register temp2);

    IonScriptCounts *maybeCreateScriptCounts();

    // This function behaves like testValueTruthy with the exception that it can
    // choose to let control flow fall through when the object is truthy, as
    // an optimization. Use testValueTruthy when it's required to branch to one
    // of the two labels.
    void testValueTruthyKernel(const ValueOperand &value,
                               const LDefinition *scratch1, const LDefinition *scratch2,
                               FloatRegister fr,
                               Label *ifTruthy, Label *ifFalsy,
                               OutOfLineTestObject *ool,
                               MDefinition *valueMIR);

    // Test whether value is truthy or not and jump to the corresponding label.
    // If the value can be an object that emulates |undefined|, |ool| must be
    // non-null; otherwise it may be null (and the scratch definitions should
    // be bogus), in which case an object encountered here will always be
    // truthy.
    void testValueTruthy(const ValueOperand &value,
                         const LDefinition *scratch1, const LDefinition *scratch2,
                         FloatRegister fr,
                         Label *ifTruthy, Label *ifFalsy,
                         OutOfLineTestObject *ool,
                         MDefinition *valueMIR);

    // This function behaves like testObjectEmulatesUndefined with the exception
    // that it can choose to let control flow fall through when the object
    // doesn't emulate undefined, as an optimization. Use the regular
    // testObjectEmulatesUndefined when it's required to branch to one of the
    // two labels.
    void testObjectEmulatesUndefinedKernel(Register objreg,
                                           Label *ifEmulatesUndefined,
                                           Label *ifDoesntEmulateUndefined,
                                           Register scratch, OutOfLineTestObject *ool);

    // Test whether an object emulates |undefined|.  If it does, jump to
    // |ifEmulatesUndefined|; the caller is responsible for binding this label.
    // If it doesn't, fall through; the label |ifDoesntEmulateUndefined| (which
    // must be initially unbound) will be bound at this point.
    void branchTestObjectEmulatesUndefined(Register objreg,
                                           Label *ifEmulatesUndefined,
                                           Label *ifDoesntEmulateUndefined,
                                           Register scratch, OutOfLineTestObject *ool);

    // Test whether an object emulates |undefined|, and jump to the
    // corresponding label.
    //
    // This method should be used when subsequent code can't be laid out in a
    // straight line; if it can, branchTest* should be used instead.
    void testObjectEmulatesUndefined(Register objreg,
                                     Label *ifEmulatesUndefined,
                                     Label *ifDoesntEmulateUndefined,
                                     Register scratch, OutOfLineTestObject *ool);

    // Get a label for the start of block which can be used for jumping, in
    // place of jumpToBlock.
    Label *getJumpLabelForBranch(MBasicBlock *block);

    void emitStoreElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                               Register elements, const LAllocation *index);

    // Bailout if an element about to be written to is a hole.
    bool emitStoreHoleCheck(Register elements, const LAllocation *index, LSnapshot *snapshot);

    bool emitAssertRangeI(const Range *r, Register input);
    bool emitAssertRangeD(const Range *r, FloatRegister input, FloatRegister temp);

    Vector<CodeOffsetLabel, 0, IonAllocPolicy> ionScriptLabels_;
#ifdef DEBUG
    bool branchIfInvalidated(Register temp, Label *invalidated);

    bool emitDebugResultChecks(LInstruction *ins);
    bool emitObjectOrStringResultChecks(LInstruction *lir, MDefinition *mir);
    bool emitValueResultChecks(LInstruction *lir, MDefinition *mir);
#endif

    // Script counts created during code generation.
    IonScriptCounts *scriptCounts_;

#if defined(JS_ION_PERF)
    PerfSpewer perfSpewer_;
#endif
};

} // namespace jit
} // namespace js

#endif /* jit_CodeGenerator_h */
