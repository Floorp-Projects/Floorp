/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_lowering_h__
#define jsion_ion_lowering_h__

// This file declares the structures that are used for attaching LIR to a
// MIRGraph.

#include "IonAllocPolicy.h"
#include "LIR.h"
#include "MOpcodes.h"

#if defined(JS_CPU_X86)
# include "x86/Lowering-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Lowering-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Lowering-arm.h"
#else
# error "CPU!"
#endif

namespace js {
namespace ion {

class LIRGenerator : public LIRGeneratorSpecific
{
    void updateResumeState(MInstruction *ins);
    void updateResumeState(MBasicBlock *block);

    // The active depth of the (perhaps nested) call argument vectors.
    uint32_t argslots_;
    // The maximum depth, for framesizeclass determination.
    uint32_t maxargslots_;

  public:
    LIRGenerator(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorSpecific(gen, graph, lirGraph),
        argslots_(0), maxargslots_(0)
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
    bool precreatePhi(LBlock *block, MPhi *phi);
    bool definePhis();

    // Allocate argument slots for a future function call.
    void allocateArguments(uint32_t argc);
    // Map an MPassArg's argument number to a slot in the frame arg vector.
    // Slots are indexed from 1. argnum is indexed from 0.
    uint32_t getArgumentSlot(uint32_t argnum);
    uint32_t getArgumentSlotForCall() { return argslots_; }
    // Free argument slots following a function call.
    void freeArguments(uint32_t argc);

  public:
    bool visitInstruction(MInstruction *ins);
    bool visitBlock(MBasicBlock *block);

    // Visitor hooks are explicit, to give CPU-specific versions a chance to
    // intercept without a bunch of explicit gunk in the .cpp.
    bool visitParameter(MParameter *param);
    bool visitCallee(MCallee *callee);
    bool visitGoto(MGoto *ins);
    bool visitTableSwitch(MTableSwitch *tableswitch);
    bool visitNewSlots(MNewSlots *ins);
    bool visitNewArray(MNewArray *ins);
    bool visitNewObject(MNewObject *ins);
    bool visitNewDeclEnvObject(MNewDeclEnvObject *ins);
    bool visitNewCallObject(MNewCallObject *ins);
    bool visitNewStringObject(MNewStringObject *ins);
    bool visitInitProp(MInitProp *ins);
    bool visitCheckOverRecursed(MCheckOverRecursed *ins);
    bool visitDefVar(MDefVar *ins);
    bool visitPrepareCall(MPrepareCall *ins);
    bool visitPassArg(MPassArg *arg);
    bool visitCreateThis(MCreateThis *ins);
    bool visitReturnFromCtor(MReturnFromCtor *ins);
    bool visitCall(MCall *call);
    bool visitApplyArgs(MApplyArgs *apply);
    bool visitTest(MTest *test);
    bool visitPolyInlineDispatch(MPolyInlineDispatch *ins);
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
    bool visitRound(MRound *ins);
    bool visitMinMax(MMinMax *ins);
    bool visitAbs(MAbs *ins);
    bool visitSqrt(MSqrt *ins);
    bool visitPow(MPow *ins);
    bool visitRandom(MRandom *ins);
    bool visitMathFunction(MMathFunction *ins);
    bool visitAdd(MAdd *ins);
    bool visitSub(MSub *ins);
    bool visitMul(MMul *ins);
    bool visitDiv(MDiv *ins);
    bool visitMod(MMod *ins);
    bool visitConcat(MConcat *ins);
    bool visitCharCodeAt(MCharCodeAt *ins);
    bool visitFromCharCode(MFromCharCode *ins);
    bool visitStart(MStart *start);
    bool visitOsrEntry(MOsrEntry *entry);
    bool visitNop(MNop *nop);
    bool visitOsrValue(MOsrValue *value);
    bool visitOsrScopeChain(MOsrScopeChain *object);
    bool visitToDouble(MToDouble *convert);
    bool visitToInt32(MToInt32 *convert);
    bool visitTruncateToInt32(MTruncateToInt32 *truncate);
    bool visitToString(MToString *convert);
    bool visitRegExp(MRegExp *ins);
    bool visitRegExpTest(MRegExpTest *ins);
    bool visitLambda(MLambda *ins);
    bool visitImplicitThis(MImplicitThis *ins);
    bool visitSlots(MSlots *ins);
    bool visitElements(MElements *ins);
    bool visitConstantElements(MConstantElements *ins);
    bool visitLoadSlot(MLoadSlot *ins);
    bool visitFunctionEnvironment(MFunctionEnvironment *ins);
    bool visitStoreSlot(MStoreSlot *ins);
    bool visitTypeBarrier(MTypeBarrier *ins);
    bool visitMonitorTypes(MMonitorTypes *ins);
    bool visitArrayLength(MArrayLength *ins);
    bool visitTypedArrayLength(MTypedArrayLength *ins);
    bool visitTypedArrayElements(MTypedArrayElements *ins);
    bool visitInitializedLength(MInitializedLength *ins);
    bool visitSetInitializedLength(MSetInitializedLength *ins);
    bool visitNot(MNot *ins);
    bool visitBoundsCheck(MBoundsCheck *ins);
    bool visitBoundsCheckLower(MBoundsCheckLower *ins);
    bool visitLoadElement(MLoadElement *ins);
    bool visitLoadElementHole(MLoadElementHole *ins);
    bool visitStoreElement(MStoreElement *ins);
    bool visitStoreElementHole(MStoreElementHole *ins);
    bool visitArrayPopShift(MArrayPopShift *ins);
    bool visitArrayPush(MArrayPush *ins);
    bool visitArrayConcat(MArrayConcat *ins);
    bool visitLoadTypedArrayElement(MLoadTypedArrayElement *ins);
    bool visitLoadTypedArrayElementHole(MLoadTypedArrayElementHole *ins);
    bool visitClampToUint8(MClampToUint8 *ins);
    bool visitLoadFixedSlot(MLoadFixedSlot *ins);
    bool visitStoreFixedSlot(MStoreFixedSlot *ins);
    bool visitGetPropertyCache(MGetPropertyCache *ins);
    bool visitGetElementCache(MGetElementCache *ins);
    bool visitBindNameCache(MBindNameCache *ins);
    bool visitGuardClass(MGuardClass *ins);
    bool visitGuardObject(MGuardObject *ins);
    bool visitGuardString(MGuardString *ins);
    bool visitCallGetProperty(MCallGetProperty *ins);
    bool visitDeleteProperty(MDeleteProperty *ins);
    bool visitGetNameCache(MGetNameCache *ins);
    bool visitCallGetIntrinsicValue(MCallGetIntrinsicValue *ins);
    bool visitCallGetElement(MCallGetElement *ins);
    bool visitCallSetElement(MCallSetElement *ins);
    bool visitSetPropertyCache(MSetPropertyCache *ins);
    bool visitCallSetProperty(MCallSetProperty *ins);
    bool visitIteratorStart(MIteratorStart *ins);
    bool visitIteratorNext(MIteratorNext *ins);
    bool visitIteratorMore(MIteratorMore *ins);
    bool visitIteratorEnd(MIteratorEnd *ins);
    bool visitStringLength(MStringLength *ins);
    bool visitArgumentsLength(MArgumentsLength *ins);
    bool visitGetArgument(MGetArgument *ins);
    bool visitThrow(MThrow *ins);
    bool visitIn(MIn *ins);
    bool visitInArray(MInArray *ins);
    bool visitInstanceOf(MInstanceOf *ins);
    bool visitCallInstanceOf(MCallInstanceOf *ins);
    bool visitFunctionBoundary(MFunctionBoundary *ins);
    bool visitSetDOMProperty(MSetDOMProperty *ins);
    bool visitGetDOMProperty(MGetDOMProperty *ins);
};

} // namespace ion
} // namespace js

#endif // jsion_ion_lowering_h__

