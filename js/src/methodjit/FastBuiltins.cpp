/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsbool.h"
#include "jslibmath.h"
#include "jsmath.h"
#include "jsnum.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

using mozilla::DebugOnly;

typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

CompileStatus
mjit::Compiler::compileMathAbsInt(FrameEntry *arg)
{
    RegisterID reg;
    if (arg->isConstant()) {
        reg = frame.allocReg();
        masm.move(Imm32(arg->getValue().toInt32()), reg);
    } else {
        reg = frame.copyDataIntoReg(arg);
    }

    Jump isPositive = masm.branch32(Assembler::GreaterThanOrEqual, reg, Imm32(0));

    /* Math.abs(INT32_MIN) results in a double */
    Jump isMinInt = masm.branch32(Assembler::Equal, reg, Imm32(INT32_MIN));
    stubcc.linkExit(isMinInt, Uses(3));

    masm.neg32(reg);

    isPositive.linkTo(masm.label(), &masm);

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(3);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileMathAbsDouble(FrameEntry *arg)
{
    FPRegisterID fpResultReg = frame.allocFPReg();

    FPRegisterID fpReg;
    bool allocate;

    DebugOnly<MaybeJump> notNumber = loadDouble(arg, &fpReg, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber).isSet());

    masm.absDouble(fpReg, fpResultReg);

    if (allocate)
        frame.freeReg(fpReg);

    frame.popn(3);
    frame.pushDouble(fpResultReg);

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileRound(FrameEntry *arg, RoundingMode mode)
{
    FPRegisterID fpScratchReg = frame.allocFPReg();

    FPRegisterID fpReg;
    bool allocate;

    DebugOnly<MaybeJump> notNumber = loadDouble(arg, &fpReg, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber).isSet());

    masm.zeroDouble(fpScratchReg);

    /* Slow path for NaN and numbers <= 0. */
    Jump negOrNan = masm.branchDouble(Assembler::DoubleLessThanOrEqualOrUnordered, fpReg, fpScratchReg);
    stubcc.linkExit(negOrNan, Uses(3));

    /* For round add 0.5 and floor. */
    FPRegisterID fpSourceReg;
    if (mode == Round) {
        masm.slowLoadConstantDouble(0.5, fpScratchReg);
        masm.addDouble(fpReg, fpScratchReg);
        fpSourceReg = fpScratchReg;
    } else {
        fpSourceReg = fpReg;
    }

    /* Truncate to integer, slow path if this overflows. */
    RegisterID reg = frame.allocReg();
    Jump overflow = masm.branchTruncateDoubleToInt32(fpSourceReg, reg);
    stubcc.linkExit(overflow, Uses(3));

    if (allocate)
        frame.freeReg(fpReg);
    frame.freeReg(fpScratchReg);

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(3);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileMathSqrt(FrameEntry *arg)
{
    FPRegisterID fpResultReg = frame.allocFPReg();

    FPRegisterID fpReg;
    bool allocate;

    DebugOnly<MaybeJump> notNumber = loadDouble(arg, &fpReg, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber).isSet());

    masm.sqrtDouble(fpReg, fpResultReg);

    if (allocate)
        frame.freeReg(fpReg);

    frame.popn(3);
    frame.pushDouble(fpResultReg);

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileMathMinMaxDouble(FrameEntry *arg1, FrameEntry *arg2,
                                        Assembler::DoubleCondition cond)
{
    FPRegisterID fpReg1;
    FPRegisterID fpReg2;
    bool allocate;

    DebugOnly<MaybeJump> notNumber = loadDouble(arg1, &fpReg1, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber).isSet());

    if (!allocate) {
        FPRegisterID fpResultReg = frame.allocFPReg();
        masm.moveDouble(fpReg1, fpResultReg);
        fpReg1 = fpResultReg;
    }

    DebugOnly<MaybeJump> notNumber2 = loadDouble(arg2, &fpReg2, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber2).isSet());


    /* Slow path for 0 and NaN, because they have special requriments. */
    masm.zeroDouble(Registers::FPConversionTemp);
    Jump zeroOrNan = masm.branchDouble(Assembler::DoubleEqualOrUnordered, fpReg1,
                                       Registers::FPConversionTemp);
    stubcc.linkExit(zeroOrNan, Uses(4));
    Jump zeroOrNan2 = masm.branchDouble(Assembler::DoubleEqualOrUnordered, fpReg2,
                                        Registers::FPConversionTemp);
    stubcc.linkExit(zeroOrNan2, Uses(4));


    Jump ifTrue = masm.branchDouble(cond, fpReg1, fpReg2);
    masm.moveDouble(fpReg2, fpReg1);

    ifTrue.linkTo(masm.label(), &masm);

    if (allocate)
        frame.freeReg(fpReg2);

    stubcc.leave();
    stubcc.masm.move(Imm32(2), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(4);
    frame.pushDouble(fpReg1);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileMathMinMaxInt(FrameEntry *arg1, FrameEntry *arg2, Assembler::Condition cond)
{
    /* Get this case out of the way */
    if (arg1->isConstant() && arg2->isConstant()) {
        int32_t a = arg1->getValue().toInt32();
        int32_t b = arg2->getValue().toInt32();

        frame.popn(4);
        if (cond == Assembler::LessThan)
            frame.push(Int32Value(a < b ? a : b));
        else
            frame.push(Int32Value(a > b ? a : b));
        return Compile_Okay;
    }

    Jump ifTrue;
    RegisterID reg;
    if (arg1->isConstant()) {
        reg = frame.copyDataIntoReg(arg2);
        int32_t v = arg1->getValue().toInt32();

        ifTrue = masm.branch32(cond, reg, Imm32(v));
        masm.move(Imm32(v), reg);
    } else if (arg2->isConstant()) {
        reg = frame.copyDataIntoReg(arg1);
        int32_t v = arg2->getValue().toInt32();

        ifTrue = masm.branch32(cond, reg, Imm32(v));
        masm.move(Imm32(v), reg);
    } else {
        reg = frame.copyDataIntoReg(arg1);
        RegisterID regB = frame.tempRegForData(arg2);

        ifTrue = masm.branch32(cond, reg, regB);
        masm.move(regB, reg);
    }

    ifTrue.linkTo(masm.label(), &masm);
    frame.popn(4);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileMathPowSimple(FrameEntry *arg1, FrameEntry *arg2)
{
    FPRegisterID fpScratchReg = frame.allocFPReg();
    FPRegisterID fpResultReg = frame.allocFPReg();

    FPRegisterID fpReg;
    bool allocate;

    DebugOnly<MaybeJump> notNumber = loadDouble(arg1, &fpReg, &allocate);
    JS_ASSERT(!((MaybeJump)notNumber).isSet());

    /* Slow path for -Infinity (must return Infinity, not NaN). */
    masm.slowLoadConstantDouble(js_NegativeInfinity, fpResultReg);
    Jump isNegInfinity = masm.branchDouble(Assembler::DoubleEqual, fpReg, fpResultReg);
    stubcc.linkExit(isNegInfinity, Uses(4));

    /* Convert -0 to +0. */
    masm.zeroDouble(fpResultReg);
    masm.moveDouble(fpReg, fpScratchReg);
    masm.addDouble(fpResultReg, fpScratchReg);

    double y = arg2->getValue().toDouble();
    if (y == 0.5) {
        /* pow(x, 0.5) => sqrt(x) */
        masm.sqrtDouble(fpScratchReg, fpResultReg);

    } else if (y == -0.5) {
        /* pow(x, -0.5) => 1/sqrt(x) */
        masm.sqrtDouble(fpScratchReg, fpScratchReg);
        masm.slowLoadConstantDouble(1, fpResultReg);
        masm.divDouble(fpScratchReg, fpResultReg);
    }

    frame.freeReg(fpScratchReg);

    if (allocate)
        frame.freeReg(fpReg);

    stubcc.leave();
    stubcc.masm.move(Imm32(2), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(4);
    frame.pushDouble(fpResultReg);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileGetChar(FrameEntry *thisValue, FrameEntry *arg, GetCharMode mode)
{
    RegisterID reg1 = frame.allocReg();
    RegisterID reg2 = frame.allocReg();

    /* Load string in strReg. */
    RegisterID strReg;
    if (thisValue->isConstant()) {
        strReg = frame.allocReg();
        masm.move(ImmPtr(thisValue->getValue().toString()), strReg);
    } else {
        strReg = frame.tempRegForData(thisValue);
        frame.pinReg(strReg);
    }

    /* Load index in argReg. */
    RegisterID argReg;
    if (arg->isConstant()) {
        argReg = frame.allocReg();
        masm.move(Imm32(arg->getValue().toInt32()), argReg);
    } else {
        argReg = frame.tempRegForData(arg);
    }
    if (!thisValue->isConstant())
        frame.unpinReg(strReg);

    Address lengthAndFlagsAddr(strReg, JSString::offsetOfLengthAndFlags());

    /* Load lengthAndFlags in reg1 and reg2 */
    masm.loadPtr(lengthAndFlagsAddr, reg1);
    masm.move(reg1, reg2);

    /* Slow path if string is a rope */
    masm.andPtr(ImmPtr((void *)JSString::FLAGS_MASK), reg1);
    Jump isRope = masm.branchTestPtr(Assembler::Zero, reg1);
    stubcc.linkExit(isRope, Uses(3));

    /* Slow path if out-of-range. */
    masm.rshiftPtr(Imm32(JSString::LENGTH_SHIFT), reg2);
    Jump outOfRange = masm.branchPtr(Assembler::AboveOrEqual, argReg, reg2);
    stubcc.linkExit(outOfRange, Uses(3));

    /* Load char code in reg2. */
    masm.move(argReg, reg1);
    masm.loadPtr(Address(strReg, JSString::offsetOfChars()), reg2);
    masm.lshiftPtr(Imm32(1), reg1);
    masm.addPtr(reg1, reg2);
    masm.load16(Address(reg2), reg2);

    /* Convert char code to string. */
    if (mode == GetChar) {
        /* Slow path if there's no unit string for this character. */
        Jump notUnitString = masm.branch32(Assembler::AboveOrEqual, reg2,
                                           Imm32(StaticStrings::UNIT_STATIC_LIMIT));
        stubcc.linkExit(notUnitString, Uses(3));

        /* Load unit string in reg2. */
        masm.lshiftPtr(Imm32(sizeof(JSAtom *) == 4 ? 2 : 3), reg2);
        masm.addPtr(ImmPtr(&cx->runtime->staticStrings.unitStaticTable), reg2);
        masm.loadPtr(Address(reg2), reg2);
    }

    if (thisValue->isConstant())
        frame.freeReg(strReg);
    if (arg->isConstant())
        frame.freeReg(argReg);
    frame.freeReg(reg1);

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(3);
    switch(mode) {
      case GetCharCode:
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg2);
        break;
      case GetChar:
        frame.pushTypedPayload(JSVAL_TYPE_STRING, reg2);
        break;
      default:
        JS_NOT_REACHED("unknown getchar mode");
    }

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileStringFromCode(FrameEntry *arg)
{
    /* Load Char-Code into argReg */
    RegisterID argReg;
    if (arg->isConstant()) {
        argReg = frame.allocReg();
        masm.move(Imm32(arg->getValue().toInt32()), argReg);
    } else {
        argReg = frame.copyDataIntoReg(arg);
    }

    /* Slow path if there's no unit string for this character. */
    Jump notUnitString = masm.branch32(Assembler::AboveOrEqual, argReg,
                                       Imm32(StaticStrings::UNIT_STATIC_LIMIT));
    stubcc.linkExit(notUnitString, Uses(3));

    /* Load unit string in reg. */
    masm.lshiftPtr(Imm32(sizeof(JSAtom *) == 4 ? 2 : 3), argReg);
    masm.addPtr(ImmPtr(&cx->runtime->staticStrings.unitStaticTable), argReg);
    masm.loadPtr(Address(argReg), argReg);

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(3);
    frame.pushTypedPayload(JSVAL_TYPE_STRING, argReg);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayPush(FrameEntry *thisValue, FrameEntry *arg,
                                 types::StackTypeSet::DoubleConversion conversion)
{
    /* This behaves like an assignment this[this.length] = arg; */

    /* Filter out silly cases. */
    if (frame.haveSameBacking(thisValue, arg) || thisValue->isConstant())
        return Compile_InlineAbort;

    if (conversion == types::StackTypeSet::AlwaysConvertToDoubles ||
        conversion == types::StackTypeSet::MaybeConvertToDoubles)
    {
        frame.ensureDouble(arg);
    }

    /* Allocate registers. */
    ValueRemat vr;
    frame.pinEntry(arg, vr, /* breakDouble = */ false);

    RegisterID objReg = frame.tempRegForData(thisValue);
    frame.pinReg(objReg);

    RegisterID slotsReg = frame.allocReg();
    masm.loadPtr(Address(objReg, JSObject::offsetOfElements()), slotsReg);

    RegisterID lengthReg = frame.allocReg();
    masm.load32(Address(slotsReg, ObjectElements::offsetOfLength()), lengthReg);

    frame.unpinReg(objReg);

    Int32Key key = Int32Key::FromRegister(lengthReg);

    /* Test for 'length == initializedLength' */
    Jump initlenGuard = masm.guardArrayExtent(ObjectElements::offsetOfInitializedLength(),
                                              slotsReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenGuard, Uses(3));

    /* Test for 'length < capacity' */
    Jump capacityGuard = masm.guardArrayExtent(ObjectElements::offsetOfCapacity(),
                                               slotsReg, key, Assembler::BelowOrEqual);
    stubcc.linkExit(capacityGuard, Uses(3));

    masm.storeValue(vr, BaseIndex(slotsReg, lengthReg, masm.JSVAL_SCALE));

    masm.bumpKey(key, 1);
    masm.store32(lengthReg, Address(slotsReg, ObjectElements::offsetOfLength()));
    masm.store32(lengthReg, Address(slotsReg, ObjectElements::offsetOfInitializedLength()));

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.unpinEntry(vr);
    frame.freeReg(slotsReg);
    frame.popn(3);

    frame.pushTypedPayload(JSVAL_TYPE_INT32, lengthReg);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayPopShift(FrameEntry *thisValue, bool isPacked, bool isArrayPop)
{
    /* Filter out silly cases. */
    if (thisValue->isConstant())
        return Compile_InlineAbort;

#ifdef JSGC_INCREMENTAL_MJ
    /* Write barrier. */
    if (cx->zone()->compileBarriers())
        return Compile_InlineAbort;
#endif

    RegisterID objReg = frame.tempRegForData(thisValue);
    frame.pinReg(objReg);

    RegisterID lengthReg = frame.allocReg();
    RegisterID slotsReg = frame.allocReg();

    JSValueType type = knownPushedType(0);

    MaybeRegisterID dataReg, typeReg;
    if (!analysis->popGuaranteed(PC)) {
        dataReg = frame.allocReg();
        if (type == JSVAL_TYPE_UNKNOWN || type == JSVAL_TYPE_DOUBLE)
            typeReg = frame.allocReg();
    }

    if (isArrayPop) {
        frame.unpinReg(objReg);
    } else {
        /*
         * Sync up front for shift() so we can jump over the inline stub.
         * The result will be stored in memory rather than registers.
         */
        frame.syncAndKillEverything();
        frame.unpinKilledReg(objReg);
    }

    masm.loadPtr(Address(objReg, JSObject::offsetOfElements()), slotsReg);
    masm.load32(Address(slotsReg, ObjectElements::offsetOfLength()), lengthReg);

    /* Test for 'length == initializedLength' */
    Int32Key key = Int32Key::FromRegister(lengthReg);
    Jump initlenGuard = masm.guardArrayExtent(ObjectElements::offsetOfInitializedLength(),
                                              slotsReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenGuard, Uses(3));

    /*
     * Test for length != 0. On zero length either take a slow call or generate
     * an undefined value, depending on whether the call is known to produce
     * undefined.
     */
    bool maybeUndefined = pushedTypeSet(0)->hasType(types::Type::UndefinedType());
    Jump emptyGuard = masm.branch32(Assembler::Equal, lengthReg, Imm32(0));
    if (!maybeUndefined)
        stubcc.linkExit(emptyGuard, Uses(3));

    masm.bumpKey(key, -1);

    if (dataReg.isSet()) {
        Jump holeCheck;
        if (isArrayPop) {
            BaseIndex slot(slotsReg, lengthReg, masm.JSVAL_SCALE);
            holeCheck = masm.fastArrayLoadSlot(slot, !isPacked, typeReg, dataReg.reg());
        } else {
            holeCheck = masm.fastArrayLoadSlot(Address(slotsReg), !isPacked, typeReg, dataReg.reg());
            Address addr = frame.addressOf(frame.peek(-2));
            if (typeReg.isSet())
                masm.storeValueFromComponents(typeReg.reg(), dataReg.reg(), addr);
            else
                masm.storeValueFromComponents(ImmType(type), dataReg.reg(), addr);
        }
        if (!isPacked)
            stubcc.linkExit(holeCheck, Uses(3));
    }

    masm.store32(lengthReg, Address(slotsReg, ObjectElements::offsetOfLength()));
    masm.store32(lengthReg, Address(slotsReg, ObjectElements::offsetOfInitializedLength()));

    if (!isArrayPop)
        INLINE_STUBCALL(stubs::ArrayShift, REJOIN_NONE);

    stubcc.leave();
    stubcc.masm.move(Imm32(0), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.freeReg(slotsReg);
    frame.freeReg(lengthReg);
    frame.popn(2);

    if (dataReg.isSet()) {
        if (isArrayPop) {
            if (typeReg.isSet())
                frame.pushRegs(typeReg.reg(), dataReg.reg(), type);
            else
                frame.pushTypedPayload(type, dataReg.reg());
        } else {
            frame.pushSynced(type);
            if (typeReg.isSet())
                frame.freeReg(typeReg.reg());
            frame.freeReg(dataReg.reg());
        }
    } else {
        frame.push(UndefinedValue());
    }

    stubcc.rejoin(Changes(1));

    if (maybeUndefined) {
        /* Generate an OOL path to push an undefined value, and rejoin. */
        if (dataReg.isSet()) {
            stubcc.linkExitDirect(emptyGuard, stubcc.masm.label());
            if (isArrayPop) {
                if (typeReg.isSet()) {
                    stubcc.masm.loadValueAsComponents(UndefinedValue(), typeReg.reg(), dataReg.reg());
                } else {
                    JS_ASSERT(type == JSVAL_TYPE_UNDEFINED);
                    stubcc.masm.loadValuePayload(UndefinedValue(), dataReg.reg());
                }
            } else {
                stubcc.masm.storeValue(UndefinedValue(), frame.addressOf(frame.peek(-1)));
            }
            stubcc.crossJump(stubcc.masm.jump(), masm.label());
        } else {
            emptyGuard.linkTo(masm.label(), &masm);
        }
    }

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayConcat(types::TypeSet *thisTypes, types::TypeSet *argTypes,
                                   FrameEntry *thisValue, FrameEntry *argValue)
{
    frame.forgetMismatchedObject(thisValue);
    frame.forgetMismatchedObject(argValue);

    /*
     * Require the 'this' types to have a specific type matching the current
     * global, so we can create the result object inline.
     */
    if (thisTypes->getObjectCount() != 1)
        return Compile_InlineAbort;
    types::TypeObject *thisType = thisTypes->getTypeObject(0);
    if (!thisType || &thisType->proto->global() != globalObj)
        return Compile_InlineAbort;

    /*
     * Constraints modeling this concat have not been generated by inference,
     * so check that type information already reflects possible side effects of
     * this call.
     */
    types::HeapTypeSet *thisElemTypes = thisType->getProperty(cx, JSID_VOID, false);
    if (!thisElemTypes)
        return Compile_Error;
    if (!pushedTypeSet(0)->hasType(types::Type::ObjectType(thisType)))
        return Compile_InlineAbort;
    for (unsigned i = 0; i < argTypes->getObjectCount(); i++) {
        if (argTypes->getSingleObject(i))
            return Compile_InlineAbort;
        types::TypeObject *argType = argTypes->getTypeObject(i);
        if (!argType)
            continue;
        types::HeapTypeSet *elemTypes = argType->getProperty(cx, JSID_VOID, false);
        if (!elemTypes)
            return Compile_Error;
        if (!elemTypes->knownSubset(cx, thisElemTypes))
            return Compile_InlineAbort;
    }

    /* Test for 'length == initializedLength' on both arrays. */

    RegisterID slotsReg = frame.allocReg();
    RegisterID reg = frame.allocReg();

    Int32Key key = Int32Key::FromRegister(reg);

    RegisterID objReg = frame.tempRegForData(thisValue);
    masm.loadPtr(Address(objReg, JSObject::offsetOfElements()), slotsReg);
    masm.load32(Address(slotsReg, ObjectElements::offsetOfLength()), reg);
    Jump initlenOneGuard = masm.guardArrayExtent(ObjectElements::offsetOfInitializedLength(),
                                                 slotsReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenOneGuard, Uses(3));

    objReg = frame.tempRegForData(argValue);
    masm.loadPtr(Address(objReg, JSObject::offsetOfElements()), slotsReg);
    masm.load32(Address(slotsReg, ObjectElements::offsetOfLength()), reg);
    Jump initlenTwoGuard = masm.guardArrayExtent(ObjectElements::offsetOfInitializedLength(),
                                                 slotsReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenTwoGuard, Uses(3));

    frame.freeReg(reg);
    frame.freeReg(slotsReg);
    frame.syncAndForgetEverything();

    /*
     * The current stack layout is 'CALLEE THIS ARG'. Allocate the result and
     * scribble it over the callee, which will be its final position after the
     * call.
     */

    JSObject *templateObject = NewDenseEmptyArray(cx, thisType->proto);
    if (!templateObject)
        return Compile_Error;
    templateObject->setType(thisType);

    RegisterID result = Registers::ReturnReg;
    Jump emptyFreeList = getNewObject(cx, result, templateObject);
    stubcc.linkExit(emptyFreeList, Uses(3));

    masm.storeValueFromComponents(ImmType(JSVAL_TYPE_OBJECT), result, frame.addressOf(frame.peek(-3)));
    INLINE_STUBCALL(stubs::ArrayConcatTwoArrays, REJOIN_FALLTHROUGH);

    stubcc.leave();
    stubcc.masm.move(Imm32(1), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(3);
    frame.pushSynced(JSVAL_TYPE_OBJECT);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayWithLength(uint32_t argc)
{
    /* Match Array() or Array(n) for constant n. */
    JS_ASSERT(argc == 0 || argc == 1);

    int32_t length = 0;
    if (argc == 1) {
        FrameEntry *arg = frame.peek(-1);
        if (!arg->isConstant() || !arg->getValue().isInt32())
            return Compile_InlineAbort;
        length = arg->getValue().toInt32();
        if (length < 0)
            return Compile_InlineAbort;
    }

    RootedScript script(cx, script_);
    types::TypeObject *type = types::TypeScript::InitObject(cx, script, PC, JSProto_Array);
    if (!type)
        return Compile_Error;

    JSObject *templateObject = NewDenseUnallocatedArray(cx, length, type->proto);
    if (!templateObject)
        return Compile_Error;
    templateObject->setType(type);

    RegisterID result = frame.allocReg();
    Jump emptyFreeList = getNewObject(cx, result, templateObject);

    stubcc.linkExit(emptyFreeList, Uses(0));
    stubcc.leave();

    stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(argc + 2);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, result);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayWithArgs(uint32_t argc)
{
    /*
     * Match Array(x, y, z) with at least two arguments. Don't inline the case
     * where a non-number argument is passed, so we don't need to care about
     * the types of the arguments.
     */
    JS_ASSERT(argc >= 2);

    size_t maxArraySlots =
        gc::GetGCKindSlots(gc::FINALIZE_OBJECT_LAST) - ObjectElements::VALUES_PER_HEADER;

    if (argc > maxArraySlots)
        return Compile_InlineAbort;

    RootedScript script(cx, script_);
    types::TypeObject *type = types::TypeScript::InitObject(cx, script, PC, JSProto_Array);
    if (!type)
        return Compile_Error;

    JSObject *templateObject = NewDenseUnallocatedArray(cx, argc, type->proto);
    if (!templateObject)
        return Compile_Error;
    templateObject->setType(type);

    JS_ASSERT(templateObject->getDenseCapacity() >= argc);

    types::StackTypeSet::DoubleConversion conversion =
        script->analysis()->pushedTypes(PC, 0)->convertDoubleElements(cx);
    if (conversion == types::StackTypeSet::AlwaysConvertToDoubles) {
        templateObject->setShouldConvertDoubleElements();
        for (unsigned i = 0; i < argc; i++) {
            FrameEntry *arg = frame.peek(-(int32_t)argc + i);
            frame.ensureDouble(arg);
        }
    }

    RegisterID result = frame.allocReg();
    Jump emptyFreeList = getNewObject(cx, result, templateObject);
    stubcc.linkExit(emptyFreeList, Uses(0));

    int offset = JSObject::offsetOfFixedElements();
    masm.store32(Imm32(argc),
                 Address(result, offset + ObjectElements::offsetOfInitializedLength()));

    for (unsigned i = 0; i < argc; i++) {
        FrameEntry *arg = frame.peek(-(int32_t)argc + i);
        frame.storeTo(arg, Address(result, offset), /* popped = */ true);
        offset += sizeof(Value);
    }

    stubcc.leave();

    stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(argc + 2);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, result);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileParseInt(JSValueType argType, uint32_t argc)
{
    bool needStubCall = false;

    if (argc > 1) {
        FrameEntry *arg = frame.peek(-(int32_t)argc + 1);

        if (!arg->isTypeKnown() || arg->getKnownType() != JSVAL_TYPE_INT32)
            return Compile_InlineAbort;

        if (arg->isConstant()) {
            int32_t base = arg->getValue().toInt32();
            if (base != 0 && base != 10)
                return Compile_InlineAbort;
        } else {
            RegisterID baseReg = frame.tempRegForData(arg);
            needStubCall = true;

            Jump isTen = masm.branch32(Assembler::Equal, baseReg, Imm32(10));
            Jump isNotZero = masm.branch32(Assembler::NotEqual, baseReg, Imm32(0));
            stubcc.linkExit(isNotZero, Uses(2 + argc));

            isTen.linkTo(masm.label(), &masm);
        }
    }

    if (argType == JSVAL_TYPE_INT32) {
        if (needStubCall) {
            stubcc.leave();
            stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
            OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);
        }

        /*
         * Stack looks like callee, this, arg1, arg2, argN.
         * First pop all args other than arg1.
         */
        frame.popn(argc - 1);
        /* "Shimmy" arg1 to the callee slot and pop this + arg1. */
        frame.shimmy(2);

        if (needStubCall) {
            stubcc.rejoin(Changes(1));
        }
    } else {
        FrameEntry *arg = frame.peek(-(int32_t)argc);
        FPRegisterID fpScratchReg = frame.allocFPReg();
        FPRegisterID fpReg;
        bool allocate;

        DebugOnly<MaybeJump> notNumber = loadDouble(arg, &fpReg, &allocate);
        JS_ASSERT(!((MaybeJump)notNumber).isSet());

        masm.slowLoadConstantDouble(1, fpScratchReg);

        /* Slow path for NaN and numbers < 1. */
        Jump lessThanOneOrNan = masm.branchDouble(Assembler::DoubleLessThanOrUnordered,
                                                  fpReg, fpScratchReg);
        stubcc.linkExit(lessThanOneOrNan, Uses(2 + argc));

        frame.freeReg(fpScratchReg);

        /* Truncate to integer, slow path if this overflows. */
        RegisterID reg = frame.allocReg();
        Jump overflow = masm.branchTruncateDoubleToInt32(fpReg, reg);
        stubcc.linkExit(overflow, Uses(2 + argc));

        if (allocate)
            frame.freeReg(fpReg);

        stubcc.leave();
        stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
        OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

        frame.popn(2 + argc);
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

        stubcc.rejoin(Changes(1));
    }

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::inlineNativeFunction(uint32_t argc, bool callingNew)
{
    if (!cx->typeInferenceEnabled())
        return Compile_InlineAbort;

    FrameEntry *origCallee = frame.peek(-((int)argc + 2));
    FrameEntry *thisValue = frame.peek(-((int)argc + 1));
    types::StackTypeSet *thisTypes = analysis->poppedTypes(PC, argc);

    if (!origCallee->isConstant() || !origCallee->isType(JSVAL_TYPE_OBJECT))
        return Compile_InlineAbort;

    JSObject *callee = &origCallee->getValue().toObject();
    if (!callee->isFunction())
        return Compile_InlineAbort;

    /*
     * The callee must have the same parent as the script's global, otherwise
     * inference may not have accounted for any side effects correctly.
     */
    if (!globalObj || globalObj != &callee->global())
        return Compile_InlineAbort;

    Native native = callee->toFunction()->maybeNative();

    if (!native)
        return Compile_InlineAbort;

    JSValueType type = knownPushedType(0);
    JSValueType thisType = thisValue->isTypeKnown()
                           ? thisValue->getKnownType()
                           : JSVAL_TYPE_UNKNOWN;

    /*
     * Note: when adding new natives which operate on properties, add relevant
     * constraint generation to the behavior of TypeConstraintCall.
     */

    /* Handle natives that can be called either with or without 'new'. */

    if (native == js_Array && type == JSVAL_TYPE_OBJECT && globalObj) {
        if (argc == 0 || argc == 1)
            return compileArrayWithLength(argc);
        return compileArrayWithArgs(argc);
    }

    /* Remaining natives must not be called with 'new'. */
    if (callingNew)
        return Compile_InlineAbort;

    if (native == js::num_parseInt && argc >= 1) {
        FrameEntry *arg = frame.peek(-(int32_t)argc);
        JSValueType argType = arg->isTypeKnown() ? arg->getKnownType() : JSVAL_TYPE_UNKNOWN;

        if ((argType == JSVAL_TYPE_DOUBLE || argType == JSVAL_TYPE_INT32) &&
            type == JSVAL_TYPE_INT32) {
            return compileParseInt(argType, argc);
        }
    }

    if (argc == 0) {
    } else if (argc == 1) {
        FrameEntry *arg = frame.peek(-1);
        types::StackTypeSet *argTypes = frame.extra(arg).types;
        if (!argTypes)
            return Compile_InlineAbort;
        JSValueType argType = arg->isTypeKnown() ? arg->getKnownType() : JSVAL_TYPE_UNKNOWN;

        if (native == js_math_abs) {
            if (argType == JSVAL_TYPE_INT32 && type == JSVAL_TYPE_INT32)
                return compileMathAbsInt(arg);

            if (argType == JSVAL_TYPE_DOUBLE && type == JSVAL_TYPE_DOUBLE)
                return compileMathAbsDouble(arg);
        }
        if (native == js_math_floor && argType == JSVAL_TYPE_DOUBLE &&
            type == JSVAL_TYPE_INT32) {
            return compileRound(arg, Floor);
        }
        if (native == js_math_round && argType == JSVAL_TYPE_DOUBLE &&
            type == JSVAL_TYPE_INT32) {
            return compileRound(arg, Round);
        }
        if (native == js_math_sqrt && type == JSVAL_TYPE_DOUBLE &&
             masm.supportsFloatingPointSqrt() &&
            (argType == JSVAL_TYPE_INT32 || argType == JSVAL_TYPE_DOUBLE)) {
            return compileMathSqrt(arg);
        }
        if (native == js_str_charCodeAt && argType == JSVAL_TYPE_INT32 &&
            thisType == JSVAL_TYPE_STRING && type == JSVAL_TYPE_INT32) {
            return compileGetChar(thisValue, arg, GetCharCode);
        }
        if (native == js_str_charAt && argType == JSVAL_TYPE_INT32 &&
            thisType == JSVAL_TYPE_STRING && type == JSVAL_TYPE_STRING) {
            return compileGetChar(thisValue, arg, GetChar);
        }
        if (native == js::str_fromCharCode && argType == JSVAL_TYPE_INT32 &&
            type == JSVAL_TYPE_STRING) {
            return compileStringFromCode(arg);
        }
        if (native == js::array_push &&
            thisType == JSVAL_TYPE_OBJECT && type == JSVAL_TYPE_INT32) {
            /*
             * Constraints propagating properties into the 'this' object are
             * generated by TypeConstraintCall during inference.
             */
            if (thisTypes->getKnownClass() == &ArrayClass &&
                !thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                           types::OBJECT_FLAG_LENGTH_OVERFLOW) &&
                !types::ArrayPrototypeHasIndexedProperty(cx, outerScript)) {
                types::StackTypeSet::DoubleConversion conversion = thisTypes->convertDoubleElements(cx);
                if (conversion != types::StackTypeSet::AmbiguousDoubleConversion)
                    return compileArrayPush(thisValue, arg, conversion);
            }
        }
        if (native == js::array_concat && argType == JSVAL_TYPE_OBJECT &&
            thisType == JSVAL_TYPE_OBJECT && type == JSVAL_TYPE_OBJECT &&
            thisTypes->getKnownClass() == &ArrayClass &&
            !thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                       types::OBJECT_FLAG_LENGTH_OVERFLOW) &&
            argTypes->getKnownClass() == &ArrayClass &&
            !argTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                      types::OBJECT_FLAG_LENGTH_OVERFLOW) &&
            !types::ArrayPrototypeHasIndexedProperty(cx, outerScript))
        {
            return compileArrayConcat(thisTypes, argTypes, thisValue, arg);
        }
    } else if (argc == 2) {
        FrameEntry *arg1 = frame.peek(-2);
        FrameEntry *arg2 = frame.peek(-1);

        JSValueType arg1Type = arg1->isTypeKnown() ? arg1->getKnownType() : JSVAL_TYPE_UNKNOWN;
        JSValueType arg2Type = arg2->isTypeKnown() ? arg2->getKnownType() : JSVAL_TYPE_UNKNOWN;

        if (native == js_math_pow && type == JSVAL_TYPE_DOUBLE &&
             masm.supportsFloatingPointSqrt() &&
            (arg1Type == JSVAL_TYPE_DOUBLE || arg1Type == JSVAL_TYPE_INT32) &&
            arg2Type == JSVAL_TYPE_DOUBLE && arg2->isConstant())
        {
            Value arg2Value = arg2->getValue();
            if (arg2Value.toDouble() == -0.5 || arg2Value.toDouble() == 0.5)
                return compileMathPowSimple(arg1, arg2);
        }
        if ((native == js_math_min || native == js_math_max)) {
            if (arg1Type == JSVAL_TYPE_INT32 && arg2Type == JSVAL_TYPE_INT32 &&
                type == JSVAL_TYPE_INT32) {
                return compileMathMinMaxInt(arg1, arg2,
                        native == js_math_min ? Assembler::LessThan : Assembler::GreaterThan);
            }
            if ((arg1Type == JSVAL_TYPE_INT32 || arg1Type == JSVAL_TYPE_DOUBLE) &&
                (arg2Type == JSVAL_TYPE_INT32 || arg2Type == JSVAL_TYPE_DOUBLE) &&
                type == JSVAL_TYPE_DOUBLE) {
                return compileMathMinMaxDouble(arg1, arg2,
                        (native == js_math_min)
                        ? Assembler::DoubleLessThan
                        : Assembler::DoubleGreaterThan);
            }
        }
    }
    return Compile_InlineAbort;
}

