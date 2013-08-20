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

#if defined(JS_CPU_X86)
# include "jit/x86/CodeGenerator-x86.h"
#elif defined(JS_CPU_X64)
# include "jit/x64/CodeGenerator-x64.h"
#elif defined(JS_CPU_ARM)
# include "jit/arm/CodeGenerator-arm.h"
#else
#error "CPU Not Supported"
#endif

namespace js {
namespace jit {

class OutOfLineNewParallelArray;
class OutOfLineTestObject;
class OutOfLineNewArray;
class OutOfLineNewObject;
class CheckOverRecursedFailure;
class CheckOverRecursedFailurePar;
class OutOfLineCheckInterruptPar;
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
    bool generateArgumentsChecks();
    bool generateBody();

  public:
    CodeGenerator(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm = nullptr);
    ~CodeGenerator();

  public:
    bool generate();
    bool generateAsmJS();
    bool link(JSContext *cx, types::CompilerConstraintList *constraints);

    bool visitLabel(LLabel *lir);
    bool visitNop(LNop *lir);
    bool visitOsiPoint(LOsiPoint *lir);
    bool visitGoto(LGoto *lir);
    bool visitTableSwitch(LTableSwitch *ins);
    bool visitTableSwitchV(LTableSwitchV *ins);
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
    bool visitIntToString(LIntToString *lir);
    bool visitDoubleToString(LDoubleToString *lir);
    bool visitInteger(LInteger *lir);
    bool visitRegExp(LRegExp *lir);
    bool visitRegExpTest(LRegExpTest *lir);
    bool visitLambda(LLambda *lir);
    bool visitLambdaForSingleton(LLambdaForSingleton *lir);
    bool visitLambdaPar(LLambdaPar *lir);
    bool visitPointer(LPointer *lir);
    bool visitSlots(LSlots *lir);
    bool visitStoreSlotV(LStoreSlotV *store);
    bool visitElements(LElements *lir);
    bool visitConvertElementsToDoubles(LConvertElementsToDoubles *lir);
    bool visitMaybeToDoubleElement(LMaybeToDoubleElement *lir);
    bool visitTypeBarrierV(LTypeBarrierV *lir);
    bool visitTypeBarrierO(LTypeBarrierO *lir);
    bool visitMonitorTypes(LMonitorTypes *lir);
    bool visitPostWriteBarrierO(LPostWriteBarrierO *lir);
    bool visitPostWriteBarrierV(LPostWriteBarrierV *lir);
    bool visitPostWriteBarrierAllSlots(LPostWriteBarrierAllSlots *lir);
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
    bool visitGetDynamicName(LGetDynamicName *lir);
    bool visitFilterArgumentsOrEval(LFilterArgumentsOrEval *lir);
    bool visitCallDirectEval(LCallDirectEval *lir);
    bool visitDoubleToInt32(LDoubleToInt32 *lir);
    bool visitFloat32ToInt32(LFloat32ToInt32 *lir);
    bool visitNewSlots(LNewSlots *lir);
    bool visitNewParallelArrayVMCall(LNewParallelArray *lir);
    bool visitNewParallelArray(LNewParallelArray *lir);
    bool visitOutOfLineNewParallelArray(OutOfLineNewParallelArray *ool);
    bool visitNewArrayCallVM(LNewArray *lir);
    bool visitNewArray(LNewArray *lir);
    bool visitOutOfLineNewArray(OutOfLineNewArray *ool);
    bool visitNewObjectVMCall(LNewObject *lir);
    bool visitNewObject(LNewObject *lir);
    bool visitOutOfLineNewObject(OutOfLineNewObject *ool);
    bool visitNewDeclEnvObject(LNewDeclEnvObject *lir);
    bool visitNewCallObject(LNewCallObject *lir);
    bool visitNewCallObjectPar(LNewCallObjectPar *lir);
    bool visitNewStringObject(LNewStringObject *lir);
    bool visitNewPar(LNewPar *lir);
    bool visitNewDenseArrayPar(LNewDenseArrayPar *lir);
    bool visitNewDerivedTypedObject(LNewDerivedTypedObject *lir);
    bool visitAbortPar(LAbortPar *lir);
    bool visitInitElem(LInitElem *lir);
    bool visitInitElemGetterSetter(LInitElemGetterSetter *lir);
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
    bool visitArrayLength(LArrayLength *lir);
    bool visitTypedArrayLength(LTypedArrayLength *lir);
    bool visitTypedArrayElements(LTypedArrayElements *lir);
    bool visitTypedObjectElements(LTypedObjectElements *lir);
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
    bool visitSetPropertyPolymorphicT(LSetPropertyPolymorphicT *ins);
    bool visitAbsI(LAbsI *lir);
    bool visitAtan2D(LAtan2D *lir);
    bool visitPowI(LPowI *lir);
    bool visitPowD(LPowD *lir);
    bool visitRandom(LRandom *lir);
    bool visitMathFunctionD(LMathFunctionD *ins);
    bool visitMathFunctionF(LMathFunctionF *ins);
    bool visitModD(LModD *ins);
    bool visitMinMaxI(LMinMaxI *lir);
    bool visitBinaryV(LBinaryV *lir);
    bool emitCompareS(LInstruction *lir, JSOp op, Register left, Register right,
                      Register output, Register temp);
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
    bool visitFunctionEnvironment(LFunctionEnvironment *lir);
    bool visitForkJoinSlice(LForkJoinSlice *lir);
    bool visitGuardThreadLocalObject(LGuardThreadLocalObject *lir);
    bool visitCallGetProperty(LCallGetProperty *lir);
    bool visitCallGetElement(LCallGetElement *lir);
    bool visitCallSetElement(LCallSetElement *lir);
    bool visitCallInitElementArray(LCallInitElementArray *lir);
    bool visitThrow(LThrow *lir);
    bool visitTypeOfV(LTypeOfV *lir);
    bool visitOutOfLineTypeOfV(OutOfLineTypeOfV *ool);
    bool visitToIdV(LToIdV *lir);
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
                  JSObject *templateObject);
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
    bool visitFunctionBoundary(LFunctionBoundary *lir);
    bool visitGetDOMProperty(LGetDOMProperty *lir);
    bool visitSetDOMProperty(LSetDOMProperty *lir);
    bool visitCallDOMNative(LCallDOMNative *lir);
    bool visitCallGetIntrinsicValue(LCallGetIntrinsicValue *lir);
    bool visitIsCallable(LIsCallable *lir);
    bool visitHaveSameClass(LHaveSameClass *lir);
    bool visitAsmJSCall(LAsmJSCall *lir);
    bool visitAsmJSParameter(LAsmJSParameter *lir);
    bool visitAsmJSReturn(LAsmJSReturn *ret);
    bool visitAsmJSVoidReturn(LAsmJSVoidReturn *ret);

    bool visitCheckOverRecursed(LCheckOverRecursed *lir);
    bool visitCheckOverRecursedFailure(CheckOverRecursedFailure *ool);
    bool visitAsmJSCheckOverRecursed(LAsmJSCheckOverRecursed *lir);

    bool visitCheckOverRecursedPar(LCheckOverRecursedPar *lir);
    bool visitCheckOverRecursedFailurePar(CheckOverRecursedFailurePar *ool);

    bool visitCheckInterruptPar(LCheckInterruptPar *lir);
    bool visitOutOfLineCheckInterruptPar(OutOfLineCheckInterruptPar *ool);

    bool visitInterruptCheckImplicit(LInterruptCheckImplicit *ins);
    bool visitOutOfLineInterruptCheckImplicit(OutOfLineInterruptCheckImplicit *ins);

    bool visitUnboxFloatingPoint(LUnboxFloatingPoint *lir);
    bool visitOutOfLineUnboxFloatingPoint(OutOfLineUnboxFloatingPoint *ool);
    bool visitOutOfLineStoreElementHole(OutOfLineStoreElementHole *ool);

    bool visitOutOfLineNewGCThingPar(OutOfLineNewGCThingPar *ool);
    bool visitOutOfLineAbortPar(OutOfLineAbortPar *ool);
    bool visitOutOfLinePropagateAbortPar(OutOfLinePropagateAbortPar *ool);
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

    IonScriptCounts *extractUnassociatedScriptCounts() {
        IonScriptCounts *counts = unassociatedScriptCounts_;
        unassociatedScriptCounts_ = nullptr;  // prevent delete in dtor
        return counts;
    }

  private:
    bool addGetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                             PropertyName *name, TypedOrValueRegister output,
                             bool allowGetters);
    bool addGetElementCache(LInstruction *ins, Register obj, ConstantOrRegister index,
                            TypedOrValueRegister output, bool monitoredResult);
    bool addSetPropertyCache(LInstruction *ins, RegisterSet liveRegs, Register objReg,
                             PropertyName *name, ConstantOrRegister value, bool strict,
                             bool needsTypeBarrier);
    bool addSetElementCache(LInstruction *ins, Register obj, Register unboxIndex, Register temp,
                            FloatRegister tempFloat, ValueOperand index, ConstantOrRegister value,
                            bool strict, bool guardHoles);
    bool checkForAbortPar(LInstruction *lir);

    bool generateBranchV(const ValueOperand &value, Label *ifTrue, Label *ifFalse, FloatRegister fr);

    bool emitAllocateGCThingPar(LInstruction *lir, const Register &objReg, const Register &sliceReg,
                                const Register &tempReg1, const Register &tempReg2,
                                JSObject *templateObj);

    bool emitCallToUncompiledScriptPar(LInstruction *lir, Register calleeReg);

    void emitLambdaInit(const Register &resultReg, const Register &scopeChainReg,
                        const LambdaFunctionInfo &info);

    IonScriptCounts *maybeCreateScriptCounts();

    // Test whether value is truthy or not and jump to the corresponding label.
    // If the value can be an object that emulates |undefined|, |ool| must be
    // non-null; otherwise it may be null (and the scratch definitions should
    // be bogus), in which case an object encountered here will always be
    // truthy.
    void testValueTruthy(const ValueOperand &value,
                         const LDefinition *scratch1, const LDefinition *scratch2,
                         FloatRegister fr,
                         Label *ifTruthy, Label *ifFalsy,
                         OutOfLineTestObject *ool);

    // Like testValueTruthy but takes an object, and |ool| must be non-null.
    // (If it's known that an object can never emulate |undefined| it shouldn't
    // be tested in the first place.)
    void testObjectTruthy(Register objreg, Label *ifTruthy, Label *ifFalsy, Register scratch,
                          OutOfLineTestObject *ool);

    // Get a label for the start of block which can be used for jumping, in
    // place of jumpToBlock.
    Label *getJumpLabelForBranch(MBasicBlock *block);

    // Bailout if an element about to be written to is a hole.
    bool emitStoreHoleCheck(Register elements, const LAllocation *index, LSnapshot *snapshot);

    bool emitAssertRangeI(const Range *r, Register input);
    bool emitAssertRangeD(const Range *r, FloatRegister input, FloatRegister temp);

    // Script counts created when compiling code with no associated JSScript.
    IonScriptCounts *unassociatedScriptCounts_;

#if defined(JS_ION_PERF)
    PerfSpewer perfSpewer_;
#endif
};

} // namespace jit
} // namespace js

#endif /* jit_CodeGenerator_h */
