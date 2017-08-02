/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Lowering_h
#define jit_Lowering_h

// This file declares the structures that are used for attaching LIR to a
// MIRGraph.

#include "jit/LIR.h"
#if defined(JS_CODEGEN_X86)
# include "jit/x86/Lowering-x86.h"
#elif defined(JS_CODEGEN_X64)
# include "jit/x64/Lowering-x64.h"
#elif defined(JS_CODEGEN_ARM)
# include "jit/arm/Lowering-arm.h"
#elif defined(JS_CODEGEN_ARM64)
# include "jit/arm64/Lowering-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
# include "jit/mips32/Lowering-mips32.h"
#elif defined(JS_CODEGEN_MIPS64)
# include "jit/mips64/Lowering-mips64.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/Lowering-none.h"
#else
# error "Unknown architecture!"
#endif

namespace js {
namespace jit {

class LIRGenerator : public LIRGeneratorSpecific
{
    void updateResumeState(MInstruction* ins);
    void updateResumeState(MBasicBlock* block);

    // The maximum depth, for framesizeclass determination.
    uint32_t maxargslots_;

  public:
    LIRGenerator(MIRGenerator* gen, MIRGraph& graph, LIRGraph& lirGraph)
      : LIRGeneratorSpecific(gen, graph, lirGraph),
        maxargslots_(0)
    { }

    MOZ_MUST_USE bool generate();

  private:
    LBoxAllocation useBoxFixedAtStart(MDefinition* mir, Register reg1, Register reg2) {
        return useBoxFixed(mir, reg1, reg2, /* useAtStart = */ true);
    }

    LBoxAllocation useBoxFixedAtStart(MDefinition* mir, ValueOperand op);
    LBoxAllocation useBoxAtStart(MDefinition* mir, LUse::Policy policy = LUse::REGISTER);

    void lowerBitOp(JSOp op, MInstruction* ins);
    void lowerShiftOp(JSOp op, MShiftInstruction* ins);
    void lowerBinaryV(JSOp op, MBinaryInstruction* ins);
    void definePhis();

    MOZ_MUST_USE bool lowerCallArguments(MCall* call);

  public:
    MOZ_MUST_USE bool visitInstruction(MInstruction* ins);
    MOZ_MUST_USE bool visitBlock(MBasicBlock* block);

    // Visitor hooks are explicit, to give CPU-specific versions a chance to
    // intercept without a bunch of explicit gunk in the .cpp.
    void visitCloneLiteral(MCloneLiteral* ins);
    void visitParameter(MParameter* param);
    void visitCallee(MCallee* callee);
    void visitIsConstructing(MIsConstructing* ins);
    void visitGoto(MGoto* ins);
    void visitTableSwitch(MTableSwitch* tableswitch);
    void visitNewArray(MNewArray* ins);
    void visitNewArrayCopyOnWrite(MNewArrayCopyOnWrite* ins);
    void visitNewArrayDynamicLength(MNewArrayDynamicLength* ins);
    void visitNewIterator(MNewIterator* ins);
    void visitNewTypedArray(MNewTypedArray* ins);
    void visitNewTypedArrayDynamicLength(MNewTypedArrayDynamicLength* ins);
    void visitNewObject(MNewObject* ins);
    void visitNewTypedObject(MNewTypedObject* ins);
    void visitNewNamedLambdaObject(MNewNamedLambdaObject* ins);
    void visitNewCallObject(MNewCallObject* ins);
    void visitNewSingletonCallObject(MNewSingletonCallObject* ins);
    void visitNewStringObject(MNewStringObject* ins);
    void visitNewDerivedTypedObject(MNewDerivedTypedObject* ins);
    void visitInitElem(MInitElem* ins);
    void visitInitElemGetterSetter(MInitElemGetterSetter* ins);
    void visitMutateProto(MMutateProto* ins);
    void visitInitPropGetterSetter(MInitPropGetterSetter* ins);
    void visitCheckOverRecursed(MCheckOverRecursed* ins);
    void visitDefVar(MDefVar* ins);
    void visitDefLexical(MDefLexical* ins);
    void visitDefFun(MDefFun* ins);
    void visitCreateThisWithTemplate(MCreateThisWithTemplate* ins);
    void visitCreateThisWithProto(MCreateThisWithProto* ins);
    void visitCreateThis(MCreateThis* ins);
    void visitCreateArgumentsObject(MCreateArgumentsObject* ins);
    void visitGetArgumentsObjectArg(MGetArgumentsObjectArg* ins);
    void visitSetArgumentsObjectArg(MSetArgumentsObjectArg* ins);
    void visitReturnFromCtor(MReturnFromCtor* ins);
    void visitComputeThis(MComputeThis* ins);
    void visitCall(MCall* call);
    void visitApplyArgs(MApplyArgs* apply);
    void visitApplyArray(MApplyArray* apply);
    void visitBail(MBail* bail);
    void visitUnreachable(MUnreachable* unreachable);
    void visitEncodeSnapshot(MEncodeSnapshot* ins);
    void visitAssertFloat32(MAssertFloat32* ins);
    void visitAssertRecoveredOnBailout(MAssertRecoveredOnBailout* ins);
    void visitGetDynamicName(MGetDynamicName* ins);
    void visitCallDirectEval(MCallDirectEval* ins);
    void visitTest(MTest* test);
    void visitGotoWithFake(MGotoWithFake* ins);
    void visitFunctionDispatch(MFunctionDispatch* ins);
    void visitObjectGroupDispatch(MObjectGroupDispatch* ins);
    void visitCompare(MCompare* comp);
    void visitTypeOf(MTypeOf* ins);
    void visitToAsync(MToAsync* ins);
    void visitToAsyncGen(MToAsyncGen* ins);
    void visitToAsyncIter(MToAsyncIter* ins);
    void visitToId(MToId* ins);
    void visitBitNot(MBitNot* ins);
    void visitBitAnd(MBitAnd* ins);
    void visitBitOr(MBitOr* ins);
    void visitBitXor(MBitXor* ins);
    void visitLsh(MLsh* ins);
    void visitRsh(MRsh* ins);
    void visitUrsh(MUrsh* ins);
    void visitSignExtend(MSignExtend* ins);
    void visitRotate(MRotate* ins);
    void visitFloor(MFloor* ins);
    void visitCeil(MCeil* ins);
    void visitRound(MRound* ins);
    void visitNearbyInt(MNearbyInt* ins);
    void visitMinMax(MMinMax* ins);
    void visitAbs(MAbs* ins);
    void visitClz(MClz* ins);
    void visitCtz(MCtz* ins);
    void visitSqrt(MSqrt* ins);
    void visitPopcnt(MPopcnt* ins);
    void visitAtan2(MAtan2* ins);
    void visitHypot(MHypot* ins);
    void visitPow(MPow* ins);
    void visitMathFunction(MMathFunction* ins);
    void visitAdd(MAdd* ins);
    void visitSub(MSub* ins);
    void visitMul(MMul* ins);
    void visitDiv(MDiv* ins);
    void visitMod(MMod* ins);
    void visitConcat(MConcat* ins);
    void visitCharCodeAt(MCharCodeAt* ins);
    void visitFromCharCode(MFromCharCode* ins);
    void visitFromCodePoint(MFromCodePoint* ins);
    void visitSinCos(MSinCos *ins);
    void visitStringSplit(MStringSplit* ins);
    void visitStart(MStart* start);
    void visitOsrEntry(MOsrEntry* entry);
    void visitNop(MNop* nop);
    void visitLimitedTruncate(MLimitedTruncate* nop);
    void visitOsrValue(MOsrValue* value);
    void visitOsrEnvironmentChain(MOsrEnvironmentChain* object);
    void visitOsrReturnValue(MOsrReturnValue* value);
    void visitOsrArgumentsObject(MOsrArgumentsObject* object);
    void visitToDouble(MToDouble* convert);
    void visitToFloat32(MToFloat32* convert);
    void visitToInt32(MToInt32* convert);
    void visitTruncateToInt32(MTruncateToInt32* truncate);
    void visitWasmTruncateToInt32(MWasmTruncateToInt32* truncate);
    void visitWrapInt64ToInt32(MWrapInt64ToInt32* ins);
    void visitToString(MToString* convert);
    void visitToObject(MToObject* convert);
    void visitToObjectOrNull(MToObjectOrNull* convert);
    void visitRegExp(MRegExp* ins);
    void visitRegExpMatcher(MRegExpMatcher* ins);
    void visitRegExpSearcher(MRegExpSearcher* ins);
    void visitRegExpTester(MRegExpTester* ins);
    void visitRegExpPrototypeOptimizable(MRegExpPrototypeOptimizable* ins);
    void visitRegExpInstanceOptimizable(MRegExpInstanceOptimizable* ins);
    void visitGetFirstDollarIndex(MGetFirstDollarIndex* ins);
    void visitStringReplace(MStringReplace* ins);
    void visitBinarySharedStub(MBinarySharedStub* ins);
    void visitUnarySharedStub(MUnarySharedStub* ins);
    void visitNullarySharedStub(MNullarySharedStub* ins);
    void visitClassConstructor(MClassConstructor* ins);
    void visitLambda(MLambda* ins);
    void visitLambdaArrow(MLambdaArrow* ins);
    void visitSetFunName(MSetFunName* ins);
    void visitNewLexicalEnvironmentObject(MNewLexicalEnvironmentObject* ins);
    void visitCopyLexicalEnvironmentObject(MCopyLexicalEnvironmentObject* ins);
    void visitKeepAliveObject(MKeepAliveObject* ins);
    void visitSlots(MSlots* ins);
    void visitElements(MElements* ins);
    void visitConstantElements(MConstantElements* ins);
    void visitConvertElementsToDoubles(MConvertElementsToDoubles* ins);
    void visitMaybeToDoubleElement(MMaybeToDoubleElement* ins);
    void visitMaybeCopyElementsForWrite(MMaybeCopyElementsForWrite* ins);
    void visitLoadSlot(MLoadSlot* ins);
    void visitLoadFixedSlotAndUnbox(MLoadFixedSlotAndUnbox* ins);
    void visitFunctionEnvironment(MFunctionEnvironment* ins);
    void visitInterruptCheck(MInterruptCheck* ins);
    void visitWasmTrap(MWasmTrap* ins);
    void visitWasmReinterpret(MWasmReinterpret* ins);
    void visitStoreSlot(MStoreSlot* ins);
    void visitFilterTypeSet(MFilterTypeSet* ins);
    void visitTypeBarrier(MTypeBarrier* ins);
    void visitMonitorTypes(MMonitorTypes* ins);
    void visitPostWriteBarrier(MPostWriteBarrier* ins);
    void visitPostWriteElementBarrier(MPostWriteElementBarrier* ins);
    void visitArrayLength(MArrayLength* ins);
    void visitSetArrayLength(MSetArrayLength* ins);
    void visitGetNextEntryForIterator(MGetNextEntryForIterator* ins);
    void visitTypedArrayLength(MTypedArrayLength* ins);
    void visitTypedArrayElements(MTypedArrayElements* ins);
    void visitSetDisjointTypedElements(MSetDisjointTypedElements* ins);
    void visitTypedObjectElements(MTypedObjectElements* ins);
    void visitSetTypedObjectOffset(MSetTypedObjectOffset* ins);
    void visitTypedObjectDescr(MTypedObjectDescr* ins);
    void visitInitializedLength(MInitializedLength* ins);
    void visitSetInitializedLength(MSetInitializedLength* ins);
    void visitUnboxedArrayLength(MUnboxedArrayLength* ins);
    void visitUnboxedArrayInitializedLength(MUnboxedArrayInitializedLength* ins);
    void visitIncrementUnboxedArrayInitializedLength(MIncrementUnboxedArrayInitializedLength* ins);
    void visitSetUnboxedArrayInitializedLength(MSetUnboxedArrayInitializedLength* ins);
    void visitNot(MNot* ins);
    void visitBoundsCheck(MBoundsCheck* ins);
    void visitBoundsCheckLower(MBoundsCheckLower* ins);
    void visitLoadElement(MLoadElement* ins);
    void visitLoadElementHole(MLoadElementHole* ins);
    void visitLoadUnboxedObjectOrNull(MLoadUnboxedObjectOrNull* ins);
    void visitLoadUnboxedString(MLoadUnboxedString* ins);
    void visitLoadElementFromState(MLoadElementFromState* ins);
    void visitStoreElement(MStoreElement* ins);
    void visitStoreElementHole(MStoreElementHole* ins);
    void visitFallibleStoreElement(MFallibleStoreElement* ins);
    void visitStoreUnboxedObjectOrNull(MStoreUnboxedObjectOrNull* ins);
    void visitStoreUnboxedString(MStoreUnboxedString* ins);
    void visitConvertUnboxedObjectToNative(MConvertUnboxedObjectToNative* ins);
    void visitEffectiveAddress(MEffectiveAddress* ins);
    void visitArrayPopShift(MArrayPopShift* ins);
    void visitArrayPush(MArrayPush* ins);
    void visitArraySlice(MArraySlice* ins);
    void visitArrayJoin(MArrayJoin* ins);
    void visitLoadUnboxedScalar(MLoadUnboxedScalar* ins);
    void visitLoadTypedArrayElementHole(MLoadTypedArrayElementHole* ins);
    void visitLoadTypedArrayElementStatic(MLoadTypedArrayElementStatic* ins);
    void visitStoreUnboxedScalar(MStoreUnboxedScalar* ins);
    void visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole* ins);
    void visitClampToUint8(MClampToUint8* ins);
    void visitLoadFixedSlot(MLoadFixedSlot* ins);
    void visitStoreFixedSlot(MStoreFixedSlot* ins);
    void visitGetPropertyCache(MGetPropertyCache* ins);
    void visitGetPropertyPolymorphic(MGetPropertyPolymorphic* ins);
    void visitSetPropertyPolymorphic(MSetPropertyPolymorphic* ins);
    void visitBindNameCache(MBindNameCache* ins);
    void visitCallBindVar(MCallBindVar* ins);
    void visitGuardObjectIdentity(MGuardObjectIdentity* ins);
    void visitGuardClass(MGuardClass* ins);
    void visitGuardObject(MGuardObject* ins);
    void visitGuardString(MGuardString* ins);
    void visitGuardReceiverPolymorphic(MGuardReceiverPolymorphic* ins);
    void visitGuardUnboxedExpando(MGuardUnboxedExpando* ins);
    void visitLoadUnboxedExpando(MLoadUnboxedExpando* ins);
    void visitPolyInlineGuard(MPolyInlineGuard* ins);
    void visitAssertRange(MAssertRange* ins);
    void visitCallGetProperty(MCallGetProperty* ins);
    void visitDeleteProperty(MDeleteProperty* ins);
    void visitDeleteElement(MDeleteElement* ins);
    void visitGetNameCache(MGetNameCache* ins);
    void visitCallGetIntrinsicValue(MCallGetIntrinsicValue* ins);
    void visitCallGetElement(MCallGetElement* ins);
    void visitCallSetElement(MCallSetElement* ins);
    void visitCallInitElementArray(MCallInitElementArray* ins);
    void visitSetPropertyCache(MSetPropertyCache* ins);
    void visitCallSetProperty(MCallSetProperty* ins);
    void visitGetIteratorCache(MGetIteratorCache* ins);
    void visitIteratorMore(MIteratorMore* ins);
    void visitIsNoIter(MIsNoIter* ins);
    void visitIteratorEnd(MIteratorEnd* ins);
    void visitStringLength(MStringLength* ins);
    void visitArgumentsLength(MArgumentsLength* ins);
    void visitGetFrameArgument(MGetFrameArgument* ins);
    void visitSetFrameArgument(MSetFrameArgument* ins);
    void visitRunOncePrologue(MRunOncePrologue* ins);
    void visitRest(MRest* ins);
    void visitThrow(MThrow* ins);
    void visitInCache(MInCache* ins);
    void visitInArray(MInArray* ins);
    void visitHasOwnCache(MHasOwnCache* ins);
    void visitInstanceOf(MInstanceOf* ins);
    void visitCallInstanceOf(MCallInstanceOf* ins);
    void visitIsCallable(MIsCallable* ins);
    void visitIsConstructor(MIsConstructor* ins);
    void visitIsArray(MIsArray* ins);
    void visitIsTypedArray(MIsTypedArray* ins);
    void visitIsObject(MIsObject* ins);
    void visitHasClass(MHasClass* ins);
    void visitObjectClassToString(MObjectClassToString* ins);
    void visitWasmAddOffset(MWasmAddOffset* ins);
    void visitWasmLoadTls(MWasmLoadTls* ins);
    void visitWasmBoundsCheck(MWasmBoundsCheck* ins);
    void visitWasmLoadGlobalVar(MWasmLoadGlobalVar* ins);
    void visitWasmStoreGlobalVar(MWasmStoreGlobalVar* ins);
    void visitWasmParameter(MWasmParameter* ins);
    void visitWasmReturn(MWasmReturn* ins);
    void visitWasmReturnVoid(MWasmReturnVoid* ins);
    void visitWasmStackArg(MWasmStackArg* ins);
    void visitWasmCall(MWasmCall* ins);
    void visitSetDOMProperty(MSetDOMProperty* ins);
    void visitGetDOMProperty(MGetDOMProperty* ins);
    void visitGetDOMMember(MGetDOMMember* ins);
    void visitRecompileCheck(MRecompileCheck* ins);
    void visitSimdBox(MSimdBox* ins);
    void visitSimdUnbox(MSimdUnbox* ins);
    void visitSimdUnaryArith(MSimdUnaryArith* ins);
    void visitSimdBinaryComp(MSimdBinaryComp* ins);
    void visitSimdBinaryBitwise(MSimdBinaryBitwise* ins);
    void visitSimdShift(MSimdShift* ins);
    void visitSimdConstant(MSimdConstant* ins);
    void visitSimdConvert(MSimdConvert* ins);
    void visitSimdReinterpretCast(MSimdReinterpretCast* ins);
    void visitSimdAllTrue(MSimdAllTrue* ins);
    void visitSimdAnyTrue(MSimdAnyTrue* ins);
    void visitPhi(MPhi* ins);
    void visitBeta(MBeta* ins);
    void visitObjectState(MObjectState* ins);
    void visitArrayState(MArrayState* ins);
    void visitArgumentState(MArgumentState* ins);
    void visitUnknownValue(MUnknownValue* ins);
    void visitLexicalCheck(MLexicalCheck* ins);
    void visitThrowRuntimeLexicalError(MThrowRuntimeLexicalError* ins);
    void visitGlobalNameConflictsCheck(MGlobalNameConflictsCheck* ins);
    void visitDebugger(MDebugger* ins);
    void visitNewTarget(MNewTarget* ins);
    void visitArrowNewTarget(MArrowNewTarget* ins);
    void visitNaNToZero(MNaNToZero *ins);
    void visitAtomicIsLockFree(MAtomicIsLockFree* ins);
    void visitGuardSharedTypedArray(MGuardSharedTypedArray* ins);
    void visitCheckReturn(MCheckReturn* ins);
    void visitCheckIsObj(MCheckIsObj* ins);
    void visitCheckIsCallable(MCheckIsCallable* ins);
    void visitCheckObjCoercible(MCheckObjCoercible* ins);
    void visitDebugCheckSelfHosted(MDebugCheckSelfHosted* ins);
    void visitFinishBoundFunctionInit(MFinishBoundFunctionInit* ins);
    void visitIsPackedArray(MIsPackedArray* ins);
    void visitGetPrototypeOf(MGetPrototypeOf* ins);
};

} // namespace jit
} // namespace js

#endif /* jit_Lowering_h */
