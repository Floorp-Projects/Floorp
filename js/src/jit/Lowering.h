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
#elif defined(JS_CODEGEN_MIPS)
# include "jit/mips/Lowering-mips.h"
#elif defined(JS_CODEGEN_NONE)
# include "jit/none/Lowering-none.h"
#else
# error "Unknown architecture!"
#endif

namespace js {
namespace jit {

class LIRGenerator : public LIRGeneratorSpecific
{
    void updateResumeState(MInstruction *ins);
    void updateResumeState(MBasicBlock *block);

    // The maximum depth, for framesizeclass determination.
    uint32_t maxargslots_;

  public:
    LIRGenerator(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorSpecific(gen, graph, lirGraph),
        maxargslots_(0)
    { }

    bool generate();

  private:

    bool useBoxAtStart(LInstruction *lir, size_t n, MDefinition *mir,
                       LUse::Policy policy = LUse::REGISTER) {
        return useBox(lir, n, mir, policy, true);
    }

    bool lowerBitOp(JSOp op, MInstruction *ins);
    bool lowerShiftOp(JSOp op, MShiftInstruction *ins);
    bool lowerBinaryV(JSOp op, MBinaryInstruction *ins);
    bool definePhis();

    bool lowerCallArguments(MCall *call);

  public:
    bool visitInstruction(MInstruction *ins);
    bool visitBlock(MBasicBlock *block);

    // Visitor hooks are explicit, to give CPU-specific versions a chance to
    // intercept without a bunch of explicit gunk in the .cpp.
    bool visitCloneLiteral(MCloneLiteral *ins);
    bool visitParameter(MParameter *param);
    bool visitCallee(MCallee *callee);
    bool visitIsConstructing(MIsConstructing *ins);
    bool visitGoto(MGoto *ins);
    bool visitTableSwitch(MTableSwitch *tableswitch);
    bool visitNewArray(MNewArray *ins);
    bool visitNewArrayCopyOnWrite(MNewArrayCopyOnWrite *ins);
    bool visitNewObject(MNewObject *ins);
    bool visitNewTypedObject(MNewTypedObject *ins);
    bool visitNewDeclEnvObject(MNewDeclEnvObject *ins);
    bool visitNewCallObject(MNewCallObject *ins);
    bool visitNewRunOnceCallObject(MNewRunOnceCallObject *ins);
    bool visitNewStringObject(MNewStringObject *ins);
    bool visitNewDerivedTypedObject(MNewDerivedTypedObject *ins);
    bool visitNewPar(MNewPar *ins);
    bool visitNewCallObjectPar(MNewCallObjectPar *ins);
    bool visitNewDenseArrayPar(MNewDenseArrayPar *ins);
    bool visitInitElem(MInitElem *ins);
    bool visitInitElemGetterSetter(MInitElemGetterSetter *ins);
    bool visitMutateProto(MMutateProto *ins);
    bool visitInitProp(MInitProp *ins);
    bool visitInitPropGetterSetter(MInitPropGetterSetter *ins);
    bool visitCheckOverRecursed(MCheckOverRecursed *ins);
    bool visitCheckOverRecursedPar(MCheckOverRecursedPar *ins);
    bool visitDefVar(MDefVar *ins);
    bool visitDefFun(MDefFun *ins);
    bool visitCreateThisWithTemplate(MCreateThisWithTemplate *ins);
    bool visitCreateThisWithProto(MCreateThisWithProto *ins);
    bool visitCreateThis(MCreateThis *ins);
    bool visitCreateArgumentsObject(MCreateArgumentsObject *ins);
    bool visitGetArgumentsObjectArg(MGetArgumentsObjectArg *ins);
    bool visitSetArgumentsObjectArg(MSetArgumentsObjectArg *ins);
    bool visitReturnFromCtor(MReturnFromCtor *ins);
    bool visitComputeThis(MComputeThis *ins);
    bool visitLoadArrowThis(MLoadArrowThis *ins);
    bool visitCall(MCall *call);
    bool visitApplyArgs(MApplyArgs *apply);
    bool visitArraySplice(MArraySplice *splice);
    bool visitBail(MBail *bail);
    bool visitUnreachable(MUnreachable *unreachable);
    bool visitAssertFloat32(MAssertFloat32 *ins);
    bool visitGetDynamicName(MGetDynamicName *ins);
    bool visitFilterArgumentsOrEval(MFilterArgumentsOrEval *ins);
    bool visitCallDirectEval(MCallDirectEval *ins);
    bool visitTest(MTest *test);
    bool visitGotoWithFake(MGotoWithFake *ins);
    bool visitFunctionDispatch(MFunctionDispatch *ins);
    bool visitTypeObjectDispatch(MTypeObjectDispatch *ins);
    bool visitCompare(MCompare *comp);
    bool visitTypeOf(MTypeOf *ins);
    bool visitToId(MToId *ins);
    bool visitBitNot(MBitNot *ins);
    bool visitBitAnd(MBitAnd *ins);
    bool visitBitOr(MBitOr *ins);
    bool visitBitXor(MBitXor *ins);
    bool visitLsh(MLsh *ins);
    bool visitRsh(MRsh *ins);
    bool visitUrsh(MUrsh *ins);
    bool visitFloor(MFloor *ins);
    bool visitCeil(MCeil *ins);
    bool visitRound(MRound *ins);
    bool visitMinMax(MMinMax *ins);
    bool visitAbs(MAbs *ins);
    bool visitClz(MClz *ins);
    bool visitSqrt(MSqrt *ins);
    bool visitAtan2(MAtan2 *ins);
    bool visitHypot(MHypot *ins);
    bool visitPow(MPow *ins);
    bool visitRandom(MRandom *ins);
    bool visitMathFunction(MMathFunction *ins);
    bool visitAdd(MAdd *ins);
    bool visitSub(MSub *ins);
    bool visitMul(MMul *ins);
    bool visitDiv(MDiv *ins);
    bool visitMod(MMod *ins);
    bool visitConcat(MConcat *ins);
    bool visitConcatPar(MConcatPar *ins);
    bool visitCharCodeAt(MCharCodeAt *ins);
    bool visitFromCharCode(MFromCharCode *ins);
    bool visitStringSplit(MStringSplit *ins);
    bool visitStart(MStart *start);
    bool visitOsrEntry(MOsrEntry *entry);
    bool visitNop(MNop *nop);
    bool visitLimitedTruncate(MLimitedTruncate *nop);
    bool visitOsrValue(MOsrValue *value);
    bool visitOsrScopeChain(MOsrScopeChain *object);
    bool visitOsrReturnValue(MOsrReturnValue *value);
    bool visitOsrArgumentsObject(MOsrArgumentsObject *object);
    bool visitToDouble(MToDouble *convert);
    bool visitToFloat32(MToFloat32 *convert);
    bool visitToInt32(MToInt32 *convert);
    bool visitTruncateToInt32(MTruncateToInt32 *truncate);
    bool visitToString(MToString *convert);
    bool visitToObjectOrNull(MToObjectOrNull *convert);
    bool visitRegExp(MRegExp *ins);
    bool visitRegExpExec(MRegExpExec *ins);
    bool visitRegExpTest(MRegExpTest *ins);
    bool visitRegExpReplace(MRegExpReplace *ins);
    bool visitStringReplace(MStringReplace *ins);
    bool visitLambda(MLambda *ins);
    bool visitLambdaArrow(MLambdaArrow *ins);
    bool visitLambdaPar(MLambdaPar *ins);
    bool visitSlots(MSlots *ins);
    bool visitElements(MElements *ins);
    bool visitConstantElements(MConstantElements *ins);
    bool visitConvertElementsToDoubles(MConvertElementsToDoubles *ins);
    bool visitMaybeToDoubleElement(MMaybeToDoubleElement *ins);
    bool visitMaybeCopyElementsForWrite(MMaybeCopyElementsForWrite *ins);
    bool visitLoadSlot(MLoadSlot *ins);
    bool visitFunctionEnvironment(MFunctionEnvironment *ins);
    bool visitForkJoinContext(MForkJoinContext *ins);
    bool visitGuardThreadExclusive(MGuardThreadExclusive *ins);
    bool visitInterruptCheck(MInterruptCheck *ins);
    bool visitAsmJSInterruptCheck(MAsmJSInterruptCheck *ins);
    bool visitInterruptCheckPar(MInterruptCheckPar *ins);
    bool visitStoreSlot(MStoreSlot *ins);
    bool visitFilterTypeSet(MFilterTypeSet *ins);
    bool visitTypeBarrier(MTypeBarrier *ins);
    bool visitMonitorTypes(MMonitorTypes *ins);
    bool visitPostWriteBarrier(MPostWriteBarrier *ins);
    bool visitArrayLength(MArrayLength *ins);
    bool visitSetArrayLength(MSetArrayLength *ins);
    bool visitTypedArrayLength(MTypedArrayLength *ins);
    bool visitTypedArrayElements(MTypedArrayElements *ins);
    bool visitTypedObjectElements(MTypedObjectElements *ins);
    bool visitSetTypedObjectOffset(MSetTypedObjectOffset *ins);
    bool visitTypedObjectProto(MTypedObjectProto *ins);
    bool visitInitializedLength(MInitializedLength *ins);
    bool visitSetInitializedLength(MSetInitializedLength *ins);
    bool visitNot(MNot *ins);
    bool visitBoundsCheck(MBoundsCheck *ins);
    bool visitBoundsCheckLower(MBoundsCheckLower *ins);
    bool visitLoadElement(MLoadElement *ins);
    bool visitLoadElementHole(MLoadElementHole *ins);
    bool visitLoadUnboxedObjectOrNull(MLoadUnboxedObjectOrNull *ins);
    bool visitLoadUnboxedString(MLoadUnboxedString *ins);
    bool visitStoreElement(MStoreElement *ins);
    bool visitStoreElementHole(MStoreElementHole *ins);
    bool visitStoreUnboxedObjectOrNull(MStoreUnboxedObjectOrNull *ins);
    bool visitStoreUnboxedString(MStoreUnboxedString *ins);
    bool visitEffectiveAddress(MEffectiveAddress *ins);
    bool visitArrayPopShift(MArrayPopShift *ins);
    bool visitArrayPush(MArrayPush *ins);
    bool visitArrayConcat(MArrayConcat *ins);
    bool visitArrayJoin(MArrayJoin *ins);
    bool visitLoadTypedArrayElement(MLoadTypedArrayElement *ins);
    bool visitLoadTypedArrayElementHole(MLoadTypedArrayElementHole *ins);
    bool visitLoadTypedArrayElementStatic(MLoadTypedArrayElementStatic *ins);
    bool visitStoreTypedArrayElement(MStoreTypedArrayElement *ins);
    bool visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole *ins);
    bool visitClampToUint8(MClampToUint8 *ins);
    bool visitLoadFixedSlot(MLoadFixedSlot *ins);
    bool visitStoreFixedSlot(MStoreFixedSlot *ins);
    bool visitGetPropertyCache(MGetPropertyCache *ins);
    bool visitGetPropertyPolymorphic(MGetPropertyPolymorphic *ins);
    bool visitSetPropertyPolymorphic(MSetPropertyPolymorphic *ins);
    bool visitGetElementCache(MGetElementCache *ins);
    bool visitBindNameCache(MBindNameCache *ins);
    bool visitGuardObjectIdentity(MGuardObjectIdentity *ins);
    bool visitGuardClass(MGuardClass *ins);
    bool visitGuardObject(MGuardObject *ins);
    bool visitGuardString(MGuardString *ins);
    bool visitGuardShapePolymorphic(MGuardShapePolymorphic *ins);
    bool visitAssertRange(MAssertRange *ins);
    bool visitCallGetProperty(MCallGetProperty *ins);
    bool visitDeleteProperty(MDeleteProperty *ins);
    bool visitDeleteElement(MDeleteElement *ins);
    bool visitGetNameCache(MGetNameCache *ins);
    bool visitCallGetIntrinsicValue(MCallGetIntrinsicValue *ins);
    bool visitCallsiteCloneCache(MCallsiteCloneCache *ins);
    bool visitCallGetElement(MCallGetElement *ins);
    bool visitCallSetElement(MCallSetElement *ins);
    bool visitCallInitElementArray(MCallInitElementArray *ins);
    bool visitSetPropertyCache(MSetPropertyCache *ins);
    bool visitSetElementCache(MSetElementCache *ins);
    bool visitCallSetProperty(MCallSetProperty *ins);
    bool visitIteratorStart(MIteratorStart *ins);
    bool visitIteratorMore(MIteratorMore *ins);
    bool visitIsNoIter(MIsNoIter *ins);
    bool visitIteratorEnd(MIteratorEnd *ins);
    bool visitStringLength(MStringLength *ins);
    bool visitArgumentsLength(MArgumentsLength *ins);
    bool visitGetFrameArgument(MGetFrameArgument *ins);
    bool visitSetFrameArgument(MSetFrameArgument *ins);
    bool visitRunOncePrologue(MRunOncePrologue *ins);
    bool visitRest(MRest *ins);
    bool visitRestPar(MRestPar *ins);
    bool visitThrow(MThrow *ins);
    bool visitIn(MIn *ins);
    bool visitInArray(MInArray *ins);
    bool visitInstanceOf(MInstanceOf *ins);
    bool visitCallInstanceOf(MCallInstanceOf *ins);
    bool visitProfilerStackOp(MProfilerStackOp *ins);
    bool visitIsCallable(MIsCallable *ins);
    bool visitIsObject(MIsObject *ins);
    bool visitHaveSameClass(MHaveSameClass *ins);
    bool visitHasClass(MHasClass *ins);
    bool visitAsmJSLoadGlobalVar(MAsmJSLoadGlobalVar *ins);
    bool visitAsmJSStoreGlobalVar(MAsmJSStoreGlobalVar *ins);
    bool visitAsmJSLoadFFIFunc(MAsmJSLoadFFIFunc *ins);
    bool visitAsmJSParameter(MAsmJSParameter *ins);
    bool visitAsmJSReturn(MAsmJSReturn *ins);
    bool visitAsmJSVoidReturn(MAsmJSVoidReturn *ins);
    bool visitAsmJSPassStackArg(MAsmJSPassStackArg *ins);
    bool visitAsmJSCall(MAsmJSCall *ins);
    bool visitSetDOMProperty(MSetDOMProperty *ins);
    bool visitGetDOMProperty(MGetDOMProperty *ins);
    bool visitGetDOMMember(MGetDOMMember *ins);
    bool visitRecompileCheck(MRecompileCheck *ins);
    bool visitMemoryBarrier(MMemoryBarrier *ins);
    bool visitSimdExtractElement(MSimdExtractElement *ins);
    bool visitSimdInsertElement(MSimdInsertElement *ins);
    bool visitSimdSignMask(MSimdSignMask *ins);
    bool visitSimdSwizzle(MSimdSwizzle *ins);
    bool visitSimdShuffle(MSimdShuffle *ins);
    bool visitSimdUnaryArith(MSimdUnaryArith *ins);
    bool visitSimdBinaryComp(MSimdBinaryComp *ins);
    bool visitSimdBinaryArith(MSimdBinaryArith *ins);
    bool visitSimdBinaryBitwise(MSimdBinaryBitwise *ins);
    bool visitSimdShift(MSimdShift *ins);
    bool visitSimdConstant(MSimdConstant *ins);
    bool visitSimdConvert(MSimdConvert *ins);
    bool visitSimdReinterpretCast(MSimdReinterpretCast *ins);
    bool visitPhi(MPhi *ins);
    bool visitBeta(MBeta *ins);
    bool visitObjectState(MObjectState *ins);
    bool visitArrayState(MArrayState *ins);
    bool visitUnknownValue(MUnknownValue *ins);
    bool visitLexicalCheck(MLexicalCheck *ins);
    bool visitThrowUninitializedLexical(MThrowUninitializedLexical *ins);
};

} // namespace jit
} // namespace js

#endif /* jit_Lowering_h */
