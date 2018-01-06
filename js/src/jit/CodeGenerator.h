/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CodeGenerator_h
#define jit_CodeGenerator_h

#include "jit/CacheIR.h"
#if defined(JS_ION_PERF)
# include "jit/PerfSpewer.h"
#endif

#if defined(JS_CODEGEN_X86)
# include "jit/x86/CodeGenerator-x86.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/CodeGenerator-x64.h"
#elif defined(JS_CODEGEN_ARM)
# include "jit/arm/CodeGenerator-arm.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/CodeGenerator-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips32/CodeGenerator-mips32.h"
#elif defined(JS_CODEGEN_MIPS64)
# include "jit/mips64/CodeGenerator-mips64.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/CodeGenerator-none.h"
#else
#error "Unknown architecture!"
#endif

namespace js {
namespace jit {

enum class SwitchTableType {
    Inline,
    OutOfLine
};

template <SwitchTableType tableType> class OutOfLineSwitch;
class OutOfLineTestObject;
class OutOfLineNewArray;
class OutOfLineNewObject;
class CheckOverRecursedFailure;
class OutOfLineInterruptCheckImplicit;
class OutOfLineUnboxFloatingPoint;
class OutOfLineStoreElementHole;
class OutOfLineTypeOfV;
class OutOfLineUpdateCache;
class OutOfLineICFallback;
class OutOfLineCallPostWriteBarrier;
class OutOfLineCallPostWriteElementBarrier;
class OutOfLineIsCallable;
class OutOfLineIsConstructor;
class OutOfLineRegExpMatcher;
class OutOfLineRegExpSearcher;
class OutOfLineRegExpTester;
class OutOfLineRegExpPrototypeOptimizable;
class OutOfLineRegExpInstanceOptimizable;
class OutOfLineLambdaArrow;
class OutOfLineNaNToZero;

class CodeGenerator final : public CodeGeneratorSpecific
{
    void generateArgumentsChecks(bool bailout = true);
    MOZ_MUST_USE bool generateBody();

    ConstantOrRegister toConstantOrRegister(LInstruction* lir, size_t n, MIRType type);

  public:
    CodeGenerator(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm = nullptr);
    ~CodeGenerator();

  public:
    MOZ_MUST_USE bool generate();
    MOZ_MUST_USE bool generateWasm(wasm::SigIdDesc sigId, wasm::BytecodeOffset trapOffset,
                                   wasm::FuncOffsets* offsets);
    MOZ_MUST_USE bool link(JSContext* cx, CompilerConstraintList* constraints);
    MOZ_MUST_USE bool linkSharedStubs(JSContext* cx);

    void visitOsiPoint(LOsiPoint* lir) override;
    void visitGoto(LGoto* lir) override;
    void visitTableSwitch(LTableSwitch* ins) override;
    void visitTableSwitchV(LTableSwitchV* ins) override;
    void visitCloneLiteral(LCloneLiteral* lir) override;
    void visitParameter(LParameter* lir) override;
    void visitCallee(LCallee* lir) override;
    void visitIsConstructing(LIsConstructing* lir) override;
    void visitStart(LStart* lir) override;
    void visitReturn(LReturn* ret) override;
    void visitDefVar(LDefVar* lir) override;
    void visitDefLexical(LDefLexical* lir) override;
    void visitDefFun(LDefFun* lir) override;
    void visitOsrEntry(LOsrEntry* lir) override;
    void visitOsrEnvironmentChain(LOsrEnvironmentChain* lir) override;
    void visitOsrValue(LOsrValue* lir) override;
    void visitOsrReturnValue(LOsrReturnValue* lir) override;
    void visitOsrArgumentsObject(LOsrArgumentsObject* lir) override;
    void visitStackArgT(LStackArgT* lir) override;
    void visitStackArgV(LStackArgV* lir) override;
    void visitMoveGroup(LMoveGroup* group) override;
    void visitValueToInt32(LValueToInt32* lir) override;
    void visitValueToDouble(LValueToDouble* lir) override;
    void visitValueToFloat32(LValueToFloat32* lir) override;
    void visitFloat32ToDouble(LFloat32ToDouble* lir) override;
    void visitDoubleToFloat32(LDoubleToFloat32* lir) override;
    void visitInt32ToFloat32(LInt32ToFloat32* lir) override;
    void visitInt32ToDouble(LInt32ToDouble* lir) override;
    void emitOOLTestObject(Register objreg, Label* ifTruthy, Label* ifFalsy, Register scratch);
    void visitTestOAndBranch(LTestOAndBranch* lir) override;
    void visitTestVAndBranch(LTestVAndBranch* lir) override;
    void visitFunctionDispatch(LFunctionDispatch* lir) override;
    void visitObjectGroupDispatch(LObjectGroupDispatch* lir) override;
    void visitBooleanToString(LBooleanToString* lir) override;
    void emitIntToString(Register input, Register output, Label* ool);
    void visitIntToString(LIntToString* lir) override;
    void visitDoubleToString(LDoubleToString* lir) override;
    void visitValueToString(LValueToString* lir) override;
    void visitValueToObject(LValueToObject* lir) override;
    void visitValueToObjectOrNull(LValueToObjectOrNull* lir) override;
    void visitInteger(LInteger* lir) override;
    void visitInteger64(LInteger64* lir) override;
    void visitRegExp(LRegExp* lir) override;
    void visitRegExpMatcher(LRegExpMatcher* lir) override;
    void visitOutOfLineRegExpMatcher(OutOfLineRegExpMatcher* ool);
    void visitRegExpSearcher(LRegExpSearcher* lir) override;
    void visitOutOfLineRegExpSearcher(OutOfLineRegExpSearcher* ool);
    void visitRegExpTester(LRegExpTester* lir) override;
    void visitOutOfLineRegExpTester(OutOfLineRegExpTester* ool);
    void visitRegExpPrototypeOptimizable(LRegExpPrototypeOptimizable* lir) override;
    void visitOutOfLineRegExpPrototypeOptimizable(OutOfLineRegExpPrototypeOptimizable* ool);
    void visitRegExpInstanceOptimizable(LRegExpInstanceOptimizable* lir) override;
    void visitOutOfLineRegExpInstanceOptimizable(OutOfLineRegExpInstanceOptimizable* ool);
    void visitGetFirstDollarIndex(LGetFirstDollarIndex* lir) override;
    void visitStringReplace(LStringReplace* lir) override;
    void emitSharedStub(ICStub::Kind kind, LInstruction* lir);
    void visitBinarySharedStub(LBinarySharedStub* lir) override;
    void visitUnarySharedStub(LUnarySharedStub* lir) override;
    void visitNullarySharedStub(LNullarySharedStub* lir) override;
    void visitClassConstructor(LClassConstructor* lir) override;
    void visitLambda(LLambda* lir) override;
    void visitOutOfLineLambdaArrow(OutOfLineLambdaArrow* ool);
    void visitLambdaArrow(LLambdaArrow* lir) override;
    void visitLambdaForSingleton(LLambdaForSingleton* lir) override;
    void visitSetFunName(LSetFunName* lir) override;
    void visitPointer(LPointer* lir) override;
    void visitKeepAliveObject(LKeepAliveObject* lir) override;
    void visitSlots(LSlots* lir) override;
    void visitLoadSlotT(LLoadSlotT* lir) override;
    void visitLoadSlotV(LLoadSlotV* lir) override;
    void visitStoreSlotT(LStoreSlotT* lir) override;
    void visitStoreSlotV(LStoreSlotV* lir) override;
    void visitElements(LElements* lir) override;
    void visitConvertElementsToDoubles(LConvertElementsToDoubles* lir) override;
    void visitMaybeToDoubleElement(LMaybeToDoubleElement* lir) override;
    void visitMaybeCopyElementsForWrite(LMaybeCopyElementsForWrite* lir) override;
    void visitGuardObjectIdentity(LGuardObjectIdentity* guard) override;
    void visitGuardReceiverPolymorphic(LGuardReceiverPolymorphic* lir) override;
    void visitGuardUnboxedExpando(LGuardUnboxedExpando* lir) override;
    void visitLoadUnboxedExpando(LLoadUnboxedExpando* lir) override;
    void visitTypeBarrierV(LTypeBarrierV* lir) override;
    void visitTypeBarrierO(LTypeBarrierO* lir) override;
    void visitMonitorTypes(LMonitorTypes* lir) override;
    void emitPostWriteBarrier(const LAllocation* obj);
    void emitPostWriteBarrier(Register objreg);
    template <class LPostBarrierType>
    void visitPostWriteBarrierCommonO(LPostBarrierType* lir, OutOfLineCode* ool);
    template <class LPostBarrierType>
    void visitPostWriteBarrierCommonV(LPostBarrierType* lir, OutOfLineCode* ool);
    void visitPostWriteBarrierO(LPostWriteBarrierO* lir) override;
    void visitPostWriteElementBarrierO(LPostWriteElementBarrierO* lir) override;
    void visitPostWriteBarrierV(LPostWriteBarrierV* lir) override;
    void visitPostWriteElementBarrierV(LPostWriteElementBarrierV* lir) override;
    void visitOutOfLineCallPostWriteBarrier(OutOfLineCallPostWriteBarrier* ool);
    void visitOutOfLineCallPostWriteElementBarrier(OutOfLineCallPostWriteElementBarrier* ool);
    void visitCallNative(LCallNative* call) override;
    void emitCallInvokeFunction(LInstruction* call, Register callereg,
                                bool isConstructing, bool ignoresReturnValue,
                                uint32_t argc, uint32_t unusedStack);
    void visitCallGeneric(LCallGeneric* call) override;
    void emitCallInvokeFunctionShuffleNewTarget(LCallKnown *call,
                                                Register calleeReg,
                                                uint32_t numFormals,
                                                uint32_t unusedStack);
    void visitCallKnown(LCallKnown* call) override;
    template<typename T> void emitApplyGeneric(T* apply);
    template<typename T> void emitCallInvokeFunction(T* apply, Register extraStackSize);
    void emitAllocateSpaceForApply(Register argcreg, Register extraStackSpace, Label* end);
    void emitCopyValuesForApply(Register argvSrcBase, Register argvIndex, Register copyreg,
                                size_t argvSrcOffset, size_t argvDstOffset);
    void emitPopArguments(Register extraStackSize);
    void emitPushArguments(LApplyArgsGeneric* apply, Register extraStackSpace);
    void visitApplyArgsGeneric(LApplyArgsGeneric* apply) override;
    void emitPushArguments(LApplyArrayGeneric* apply, Register extraStackSpace);
    void visitApplyArrayGeneric(LApplyArrayGeneric* apply) override;
    void visitBail(LBail* lir) override;
    void visitUnreachable(LUnreachable* unreachable) override;
    void visitEncodeSnapshot(LEncodeSnapshot* lir) override;
    void visitGetDynamicName(LGetDynamicName* lir) override;
    void visitCallDirectEval(LCallDirectEval* lir) override;
    void visitDoubleToInt32(LDoubleToInt32* lir) override;
    void visitFloat32ToInt32(LFloat32ToInt32* lir) override;
    void visitNewArrayCallVM(LNewArray* lir);
    void visitNewArray(LNewArray* lir) override;
    void visitOutOfLineNewArray(OutOfLineNewArray* ool);
    void visitNewArrayCopyOnWrite(LNewArrayCopyOnWrite* lir) override;
    void visitNewArrayDynamicLength(LNewArrayDynamicLength* lir) override;
    void visitNewIterator(LNewIterator* lir) override;
    void visitNewTypedArray(LNewTypedArray* lir) override;
    void visitNewTypedArrayDynamicLength(LNewTypedArrayDynamicLength* lir) override;
    void visitNewObjectVMCall(LNewObject* lir);
    void visitNewObject(LNewObject* lir) override;
    void visitOutOfLineNewObject(OutOfLineNewObject* ool);
    void visitNewTypedObject(LNewTypedObject* lir) override;
    void visitSimdBox(LSimdBox* lir) override;
    void visitSimdUnbox(LSimdUnbox* lir) override;
    void visitNewNamedLambdaObject(LNewNamedLambdaObject* lir) override;
    void visitNewCallObject(LNewCallObject* lir) override;
    void visitNewSingletonCallObject(LNewSingletonCallObject* lir) override;
    void visitNewStringObject(LNewStringObject* lir) override;
    void visitNewDerivedTypedObject(LNewDerivedTypedObject* lir) override;
    void visitInitElem(LInitElem* lir) override;
    void visitInitElemGetterSetter(LInitElemGetterSetter* lir) override;
    void visitMutateProto(LMutateProto* lir) override;
    void visitInitPropGetterSetter(LInitPropGetterSetter* lir) override;
    void visitCreateThis(LCreateThis* lir) override;
    void visitCreateThisWithProto(LCreateThisWithProto* lir) override;
    void visitCreateThisWithTemplate(LCreateThisWithTemplate* lir) override;
    void visitCreateArgumentsObject(LCreateArgumentsObject* lir) override;
    void visitGetArgumentsObjectArg(LGetArgumentsObjectArg* lir) override;
    void visitSetArgumentsObjectArg(LSetArgumentsObjectArg* lir) override;
    void visitReturnFromCtor(LReturnFromCtor* lir) override;
    void visitComputeThis(LComputeThis* lir) override;
    void visitImplicitThis(LImplicitThis* lir) override;
    void visitArrayLength(LArrayLength* lir) override;
    void visitSetArrayLength(LSetArrayLength* lir) override;
    void visitGetNextEntryForIterator(LGetNextEntryForIterator* lir) override;
    void visitTypedArrayLength(LTypedArrayLength* lir) override;
    void visitTypedArrayElements(LTypedArrayElements* lir) override;
    void visitSetDisjointTypedElements(LSetDisjointTypedElements* lir) override;
    void visitTypedObjectElements(LTypedObjectElements* lir) override;
    void visitSetTypedObjectOffset(LSetTypedObjectOffset* lir) override;
    void visitTypedObjectDescr(LTypedObjectDescr* ins) override;
    void visitStringLength(LStringLength* lir) override;
    void visitSubstr(LSubstr* lir) override;
    void visitInitializedLength(LInitializedLength* lir) override;
    void visitSetInitializedLength(LSetInitializedLength* lir) override;
    void visitNotO(LNotO* ins) override;
    void visitNotV(LNotV* ins) override;
    void visitBoundsCheck(LBoundsCheck* lir) override;
    void visitBoundsCheckRange(LBoundsCheckRange* lir) override;
    void visitBoundsCheckLower(LBoundsCheckLower* lir) override;
    void visitLoadFixedSlotV(LLoadFixedSlotV* ins) override;
    void visitLoadFixedSlotAndUnbox(LLoadFixedSlotAndUnbox* lir) override;
    void visitLoadFixedSlotT(LLoadFixedSlotT* ins) override;
    void visitStoreFixedSlotV(LStoreFixedSlotV* ins) override;
    void visitStoreFixedSlotT(LStoreFixedSlotT* ins) override;
    void emitGetPropertyPolymorphic(LInstruction* lir, Register obj,
                                    Register scratch, const TypedOrValueRegister& output);
    void visitGetPropertyPolymorphicV(LGetPropertyPolymorphicV* ins) override;
    void visitGetPropertyPolymorphicT(LGetPropertyPolymorphicT* ins) override;
    void emitSetPropertyPolymorphic(LInstruction* lir, Register obj,
                                    Register scratch, const ConstantOrRegister& value);
    void visitSetPropertyPolymorphicV(LSetPropertyPolymorphicV* ins) override;
    void visitSetPropertyPolymorphicT(LSetPropertyPolymorphicT* ins) override;
    void visitAbsI(LAbsI* lir) override;
    void visitAtan2D(LAtan2D* lir) override;
    void visitHypot(LHypot* lir) override;
    void visitPowI(LPowI* lir) override;
    void visitPowD(LPowD* lir) override;
    void visitPowV(LPowV* lir) override;
    void visitMathFunctionD(LMathFunctionD* ins) override;
    void visitMathFunctionF(LMathFunctionF* ins) override;
    void visitModD(LModD* ins) override;
    void visitMinMaxI(LMinMaxI* lir) override;
    void visitBinaryV(LBinaryV* lir) override;
    void emitCompareS(LInstruction* lir, JSOp op, Register left, Register right, Register output);
    void visitCompareS(LCompareS* lir) override;
    void visitCompareStrictS(LCompareStrictS* lir) override;
    void visitCompareVM(LCompareVM* lir) override;
    void visitIsNullOrLikeUndefinedV(LIsNullOrLikeUndefinedV* lir) override;
    void visitIsNullOrLikeUndefinedT(LIsNullOrLikeUndefinedT* lir) override;
    void visitIsNullOrLikeUndefinedAndBranchV(LIsNullOrLikeUndefinedAndBranchV* lir) override;
    void visitIsNullOrLikeUndefinedAndBranchT(LIsNullOrLikeUndefinedAndBranchT* lir) override;
    void emitConcat(LInstruction* lir, Register lhs, Register rhs, Register output);
    void visitConcat(LConcat* lir) override;
    void visitCharCodeAt(LCharCodeAt* lir) override;
    void visitFromCharCode(LFromCharCode* lir) override;
    void visitFromCodePoint(LFromCodePoint* lir) override;
    void visitStringConvertCase(LStringConvertCase* lir) override;
    void visitSinCos(LSinCos *lir) override;
    void visitStringSplit(LStringSplit* lir) override;
    void visitFunctionEnvironment(LFunctionEnvironment* lir) override;
    void visitHomeObject(LHomeObject* lir) override;
    void visitHomeObjectSuperBase(LHomeObjectSuperBase* lir) override;
    void visitNewLexicalEnvironmentObject(LNewLexicalEnvironmentObject* lir) override;
    void visitCopyLexicalEnvironmentObject(LCopyLexicalEnvironmentObject* lir) override;
    void visitCallGetProperty(LCallGetProperty* lir) override;
    void visitCallGetElement(LCallGetElement* lir) override;
    void visitCallSetElement(LCallSetElement* lir) override;
    void visitCallInitElementArray(LCallInitElementArray* lir) override;
    void visitThrow(LThrow* lir) override;
    void visitTypeOfV(LTypeOfV* lir) override;
    void visitOutOfLineTypeOfV(OutOfLineTypeOfV* ool);
    void visitToAsync(LToAsync* lir) override;
    void visitToAsyncGen(LToAsyncGen* lir) override;
    void visitToAsyncIter(LToAsyncIter* lir) override;
    void visitToIdV(LToIdV* lir) override;
    template<typename T> void emitLoadElementT(LLoadElementT* lir, const T& source);
    void visitLoadElementT(LLoadElementT* lir) override;
    void visitLoadElementV(LLoadElementV* load) override;
    void visitLoadElementHole(LLoadElementHole* lir) override;
    void visitLoadUnboxedPointerV(LLoadUnboxedPointerV* lir) override;
    void visitLoadUnboxedPointerT(LLoadUnboxedPointerT* lir) override;
    void visitUnboxObjectOrNull(LUnboxObjectOrNull* lir) override;
    template <SwitchTableType tableType>
    void visitOutOfLineSwitch(OutOfLineSwitch<tableType>* ool);
    void visitLoadElementFromStateV(LLoadElementFromStateV* lir) override;
    void visitStoreElementT(LStoreElementT* lir) override;
    void visitStoreElementV(LStoreElementV* lir) override;
    template <typename T> void emitStoreElementHoleT(T* lir);
    template <typename T> void emitStoreElementHoleV(T* lir);
    void visitStoreElementHoleT(LStoreElementHoleT* lir) override;
    void visitStoreElementHoleV(LStoreElementHoleV* lir) override;
    void visitFallibleStoreElementV(LFallibleStoreElementV* lir) override;
    void visitFallibleStoreElementT(LFallibleStoreElementT* lir) override;
    void visitStoreUnboxedPointer(LStoreUnboxedPointer* lir) override;
    void visitConvertUnboxedObjectToNative(LConvertUnboxedObjectToNative* lir) override;
    void emitArrayPopShift(LInstruction* lir, const MArrayPopShift* mir, Register obj,
                           Register elementsTemp, Register lengthTemp, TypedOrValueRegister out);
    void visitArrayPopShiftV(LArrayPopShiftV* lir) override;
    void visitArrayPopShiftT(LArrayPopShiftT* lir) override;
    void emitArrayPush(LInstruction* lir, const MArrayPush* mir, Register obj,
                       const ConstantOrRegister& value, Register elementsTemp, Register length);
    void visitArrayPushV(LArrayPushV* lir) override;
    void visitArrayPushT(LArrayPushT* lir) override;
    void visitArraySlice(LArraySlice* lir) override;
    void visitArrayJoin(LArrayJoin* lir) override;
    void visitLoadUnboxedScalar(LLoadUnboxedScalar* lir) override;
    void visitLoadTypedArrayElementHole(LLoadTypedArrayElementHole* lir) override;
    void visitStoreUnboxedScalar(LStoreUnboxedScalar* lir) override;
    void visitStoreTypedArrayElementHole(LStoreTypedArrayElementHole* lir) override;
    void visitAtomicIsLockFree(LAtomicIsLockFree* lir) override;
    void visitGuardSharedTypedArray(LGuardSharedTypedArray* lir) override;
    void visitClampIToUint8(LClampIToUint8* lir) override;
    void visitClampDToUint8(LClampDToUint8* lir) override;
    void visitClampVToUint8(LClampVToUint8* lir) override;
    void visitGetIteratorCache(LGetIteratorCache* lir) override;
    void visitIteratorMore(LIteratorMore* lir) override;
    void visitIsNoIterAndBranch(LIsNoIterAndBranch* lir) override;
    void visitIteratorEnd(LIteratorEnd* lir) override;
    void visitArgumentsLength(LArgumentsLength* lir) override;
    void visitGetFrameArgument(LGetFrameArgument* lir) override;
    void visitSetFrameArgumentT(LSetFrameArgumentT* lir) override;
    void visitSetFrameArgumentC(LSetFrameArgumentC* lir) override;
    void visitSetFrameArgumentV(LSetFrameArgumentV* lir) override;
    void visitRunOncePrologue(LRunOncePrologue* lir) override;
    void emitRest(LInstruction* lir, Register array, Register numActuals,
                  Register temp0, Register temp1, unsigned numFormals,
                  JSObject* templateObject, bool saveAndRestore, Register resultreg);
    void visitRest(LRest* lir) override;
    void visitCallSetProperty(LCallSetProperty* ins) override;
    void visitCallDeleteProperty(LCallDeleteProperty* lir) override;
    void visitCallDeleteElement(LCallDeleteElement* lir) override;
    void visitBitNotV(LBitNotV* lir) override;
    void visitBitOpV(LBitOpV* lir) override;
    void emitInstanceOf(LInstruction* ins, JSObject* prototypeObject);
    void visitInCache(LInCache* ins) override;
    void visitInArray(LInArray* ins) override;
    void visitInstanceOfO(LInstanceOfO* ins) override;
    void visitInstanceOfV(LInstanceOfV* ins) override;
    void visitCallInstanceOf(LCallInstanceOf* ins) override;
    void visitGetDOMProperty(LGetDOMProperty* lir) override;
    void visitGetDOMMemberV(LGetDOMMemberV* lir) override;
    void visitGetDOMMemberT(LGetDOMMemberT* lir) override;
    void visitSetDOMProperty(LSetDOMProperty* lir) override;
    void visitCallDOMNative(LCallDOMNative* lir) override;
    void visitCallGetIntrinsicValue(LCallGetIntrinsicValue* lir) override;
    void visitCallBindVar(LCallBindVar* lir) override;
    enum CallableOrConstructor {
        Callable,
        Constructor
    };
    template <CallableOrConstructor mode>
    void emitIsCallableOrConstructor(Register object, Register output, Label* failure);
    void visitIsCallableO(LIsCallableO* lir) override;
    void visitIsCallableV(LIsCallableV* lir) override;
    void visitOutOfLineIsCallable(OutOfLineIsCallable* ool);
    void visitIsConstructor(LIsConstructor* lir) override;
    void visitOutOfLineIsConstructor(OutOfLineIsConstructor* ool);
    void visitIsArrayO(LIsArrayO* lir) override;
    void visitIsArrayV(LIsArrayV* lir) override;
    void visitIsTypedArray(LIsTypedArray* lir) override;
    void visitIsObject(LIsObject* lir) override;
    void visitIsObjectAndBranch(LIsObjectAndBranch* lir) override;
    void visitHasClass(LHasClass* lir) override;
    void visitObjectClassToString(LObjectClassToString* lir) override;
    void visitWasmParameter(LWasmParameter* lir) override;
    void visitWasmParameterI64(LWasmParameterI64* lir) override;
    void visitWasmReturn(LWasmReturn* ret) override;
    void visitWasmReturnI64(LWasmReturnI64* ret) override;
    void visitWasmReturnVoid(LWasmReturnVoid* ret) override;
    void visitLexicalCheck(LLexicalCheck* ins) override;
    void visitThrowRuntimeLexicalError(LThrowRuntimeLexicalError* ins) override;
    void visitGlobalNameConflictsCheck(LGlobalNameConflictsCheck* ins) override;
    void visitDebugger(LDebugger* ins) override;
    void visitNewTarget(LNewTarget* ins) override;
    void visitArrowNewTarget(LArrowNewTarget* ins) override;
    void visitCheckReturn(LCheckReturn* ins) override;
    void visitCheckIsObj(LCheckIsObj* ins) override;
    void visitCheckIsCallable(LCheckIsCallable* ins) override;
    void visitCheckObjCoercible(LCheckObjCoercible* ins) override;
    void visitDebugCheckSelfHosted(LDebugCheckSelfHosted* ins) override;
    void visitNaNToZero(LNaNToZero* ins) override;
    void visitOutOfLineNaNToZero(OutOfLineNaNToZero* ool);
    void visitFinishBoundFunctionInit(LFinishBoundFunctionInit* lir) override;
    void visitIsPackedArray(LIsPackedArray* lir) override;
    void visitGetPrototypeOf(LGetPrototypeOf* lir) override;

    void visitCheckOverRecursed(LCheckOverRecursed* lir) override;
    void visitCheckOverRecursedFailure(CheckOverRecursedFailure* ool);

    void visitUnboxFloatingPoint(LUnboxFloatingPoint* lir) override;
    void visitOutOfLineUnboxFloatingPoint(OutOfLineUnboxFloatingPoint* ool);
    void visitOutOfLineStoreElementHole(OutOfLineStoreElementHole* ool);

    void loadJSScriptForBlock(MBasicBlock* block, Register reg);
    void loadOutermostJSScript(Register reg);

    void visitOutOfLineICFallback(OutOfLineICFallback* ool);

    void visitGetPropertyCacheV(LGetPropertyCacheV* ins) override;
    void visitGetPropertyCacheT(LGetPropertyCacheT* ins) override;
    void visitGetPropSuperCacheV(LGetPropSuperCacheV* ins) override;
    void visitBindNameCache(LBindNameCache* ins) override;
    void visitCallSetProperty(LInstruction* ins);
    void visitSetPropertyCache(LSetPropertyCache* ins) override;
    void visitGetNameCache(LGetNameCache* ins) override;
    void visitHasOwnCache(LHasOwnCache* ins) override;

    void visitAssertRangeI(LAssertRangeI* ins) override;
    void visitAssertRangeD(LAssertRangeD* ins) override;
    void visitAssertRangeF(LAssertRangeF* ins) override;
    void visitAssertRangeV(LAssertRangeV* ins) override;

    void visitAssertResultV(LAssertResultV* ins) override;
    void visitAssertResultT(LAssertResultT* ins) override;
    void emitAssertResultV(const ValueOperand output, const TemporaryTypeSet* typeset);
    void emitAssertObjectOrStringResult(Register input, MIRType type, const TemporaryTypeSet* typeset);

    void visitInterruptCheck(LInterruptCheck* lir) override;
    void visitOutOfLineInterruptCheckImplicit(OutOfLineInterruptCheckImplicit* ins);
    void visitWasmTrap(LWasmTrap* lir) override;
    void visitWasmLoadTls(LWasmLoadTls* ins) override;
    void visitWasmBoundsCheck(LWasmBoundsCheck* ins) override;
    void visitWasmAlignmentCheck(LWasmAlignmentCheck* ins) override;
    void visitRecompileCheck(LRecompileCheck* ins) override;
    void visitRotate(LRotate* ins) override;

    void visitRandom(LRandom* ins) override;
    void visitSignExtendInt32(LSignExtendInt32* ins) override;

#ifdef DEBUG
    void emitDebugForceBailing(LInstruction* lir);
#endif

    IonScriptCounts* extractScriptCounts() {
        IonScriptCounts* counts = scriptCounts_;
        scriptCounts_ = nullptr;  // prevent delete in dtor
        return counts;
    }

  private:
    void addGetPropertyCache(LInstruction* ins, LiveRegisterSet liveRegs,
                             TypedOrValueRegister value, const ConstantOrRegister& id,
                             TypedOrValueRegister output, Register maybeTemp,
                             GetPropertyResultFlags flags, jsbytecode* profilerLeavePc);
    void addSetPropertyCache(LInstruction* ins, LiveRegisterSet liveRegs, Register objReg,
                             Register temp, FloatRegister tempDouble,
                             FloatRegister tempF32, const ConstantOrRegister& id,
                             const ConstantOrRegister& value,
                             bool strict, bool needsPostBarrier, bool needsTypeBarrier,
                             bool guardHoles, jsbytecode* profilerLeavePc);

    MOZ_MUST_USE bool generateBranchV(const ValueOperand& value, Label* ifTrue, Label* ifFalse,
                                      FloatRegister fr);

    void emitLambdaInit(Register resultReg, Register envChainReg,
                        const LambdaFunctionInfo& info);

    void emitFilterArgumentsOrEval(LInstruction* lir, Register string, Register temp1,
                                   Register temp2);

    template <class IteratorObject, class OrderedHashTable>
    void emitGetNextEntryForIterator(LGetNextEntryForIterator* lir);

    template <class OrderedHashTable>
    void emitLoadIteratorValues(Register result, Register temp, Register front);

    IonScriptCounts* maybeCreateScriptCounts();

    // This function behaves like testValueTruthy with the exception that it can
    // choose to let control flow fall through when the object is truthy, as
    // an optimization. Use testValueTruthy when it's required to branch to one
    // of the two labels.
    void testValueTruthyKernel(const ValueOperand& value,
                               const LDefinition* scratch1, const LDefinition* scratch2,
                               FloatRegister fr,
                               Label* ifTruthy, Label* ifFalsy,
                               OutOfLineTestObject* ool,
                               MDefinition* valueMIR);

    // Test whether value is truthy or not and jump to the corresponding label.
    // If the value can be an object that emulates |undefined|, |ool| must be
    // non-null; otherwise it may be null (and the scratch definitions should
    // be bogus), in which case an object encountered here will always be
    // truthy.
    void testValueTruthy(const ValueOperand& value,
                         const LDefinition* scratch1, const LDefinition* scratch2,
                         FloatRegister fr,
                         Label* ifTruthy, Label* ifFalsy,
                         OutOfLineTestObject* ool,
                         MDefinition* valueMIR);

    // This function behaves like testObjectEmulatesUndefined with the exception
    // that it can choose to let control flow fall through when the object
    // doesn't emulate undefined, as an optimization. Use the regular
    // testObjectEmulatesUndefined when it's required to branch to one of the
    // two labels.
    void testObjectEmulatesUndefinedKernel(Register objreg,
                                           Label* ifEmulatesUndefined,
                                           Label* ifDoesntEmulateUndefined,
                                           Register scratch, OutOfLineTestObject* ool);

    // Test whether an object emulates |undefined|.  If it does, jump to
    // |ifEmulatesUndefined|; the caller is responsible for binding this label.
    // If it doesn't, fall through; the label |ifDoesntEmulateUndefined| (which
    // must be initially unbound) will be bound at this point.
    void branchTestObjectEmulatesUndefined(Register objreg,
                                           Label* ifEmulatesUndefined,
                                           Label* ifDoesntEmulateUndefined,
                                           Register scratch, OutOfLineTestObject* ool);

    // Test whether an object emulates |undefined|, and jump to the
    // corresponding label.
    //
    // This method should be used when subsequent code can't be laid out in a
    // straight line; if it can, branchTest* should be used instead.
    void testObjectEmulatesUndefined(Register objreg,
                                     Label* ifEmulatesUndefined,
                                     Label* ifDoesntEmulateUndefined,
                                     Register scratch, OutOfLineTestObject* ool);

    void emitStoreElementTyped(const LAllocation* value, MIRType valueType, MIRType elementType,
                               Register elements, const LAllocation* index,
                               int32_t offsetAdjustment);

    // Bailout if an element about to be written to is a hole.
    void emitStoreHoleCheck(Register elements, const LAllocation* index, int32_t offsetAdjustment,
                            LSnapshot* snapshot);

    void emitAssertRangeI(const Range* r, Register input);
    void emitAssertRangeD(const Range* r, FloatRegister input, FloatRegister temp);

    void maybeEmitGlobalBarrierCheck(const LAllocation* maybeGlobal, OutOfLineCode* ool);

    Vector<CodeOffset, 0, JitAllocPolicy> ionScriptLabels_;

    struct SharedStub {
        ICStub::Kind kind;
        IonICEntry entry;
        CodeOffset label;

        SharedStub(ICStub::Kind kind, IonICEntry entry, CodeOffset label)
          : kind(kind), entry(entry), label(label)
        {}
    };

    Vector<SharedStub, 0, SystemAllocPolicy> sharedStubs_;

    void branchIfInvalidated(Register temp, Label* invalidated);

#ifdef DEBUG
    void emitDebugResultChecks(LInstruction* ins);
    void emitObjectOrStringResultChecks(LInstruction* lir, MDefinition* mir);
    void emitValueResultChecks(LInstruction* lir, MDefinition* mir);
#endif

    // Script counts created during code generation.
    IonScriptCounts* scriptCounts_;

#if defined(JS_ION_PERF)
    PerfSpewer perfSpewer_;
#endif

    // This integer is a bit mask of all SimdTypeDescr::Type indexes.  When a
    // MSimdBox instruction is encoded, it might have either been created by
    // IonBuilder, or by the Eager Simd Unbox phase.
    //
    // As the template objects are weak references, the JitCompartment is using
    // Read Barriers, but such barrier cannot be used during the compilation. To
    // work around this issue, the barriers are captured during
    // CodeGenerator::link.
    //
    // Instead of saving the pointers, we just save the index of the Read
    // Barriered objects in a bit mask.
    uint32_t simdRefreshTemplatesDuringLink_;

    void registerSimdTemplate(SimdType simdType);
    void captureSimdTemplate(JSContext* cx);
};

} // namespace jit
} // namespace js

#endif /* jit_CodeGenerator_h */
