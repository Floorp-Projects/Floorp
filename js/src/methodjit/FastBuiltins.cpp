/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   Jan de Mooij <jandemooij@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "jsbool.h"
#include "jslibmath.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jstracer.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

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
        int32 a = arg1->getValue().toInt32();
        int32 b = arg2->getValue().toInt32();

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
    masm.andPtr(ImmPtr((void *)JSString::ROPE_BIT), reg1);
    Jump isRope = masm.branchTestPtr(Assembler::NonZero, reg1);
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
mjit::Compiler::compileArrayPush(FrameEntry *thisValue, FrameEntry *arg)
{
    /* This behaves like an assignment this[this.length] = arg; */

    /* Filter out silly cases. */
    if (frame.haveSameBacking(thisValue, arg) || thisValue->isConstant())
        return Compile_InlineAbort;

    /* Allocate registers. */
    ValueRemat vr;
    frame.pinEntry(arg, vr, /* breakDouble = */ false);

    RegisterID objReg = frame.tempRegForData(thisValue);
    frame.pinReg(objReg);

    RegisterID slotsReg = frame.allocReg();

    RegisterID lengthReg = frame.allocReg();
    masm.load32(Address(objReg, offsetof(JSObject, privateData)), lengthReg);

    frame.unpinReg(objReg);

    Int32Key key = Int32Key::FromRegister(lengthReg);

    /* Test for 'length == initializedLength' */
    Jump initlenGuard = masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                              objReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenGuard, Uses(3));

    /* Test for 'length < capacity' */
    Jump capacityGuard = masm.guardArrayExtent(offsetof(JSObject, capacity),
                                               objReg, key, Assembler::BelowOrEqual);
    stubcc.linkExit(capacityGuard, Uses(3));

    masm.loadPtr(Address(objReg, JSObject::offsetOfSlots()), slotsReg);
    masm.storeValue(vr, BaseIndex(slotsReg, lengthReg, masm.JSVAL_SCALE));

    masm.bumpKey(key, 1);
    masm.store32(lengthReg, Address(objReg, offsetof(JSObject, privateData)));
    masm.store32(lengthReg, Address(objReg, offsetof(JSObject, initializedLength)));

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
mjit::Compiler::compileArrayPop(FrameEntry *thisValue, bool isPacked)
{
    /* Filter out silly cases. */
    if (thisValue->isConstant())
        return Compile_InlineAbort;

    RegisterID objReg = frame.tempRegForData(thisValue);
    frame.pinReg(objReg);

    RegisterID lengthReg = frame.allocReg();
    masm.load32(Address(objReg, offsetof(JSObject, privateData)), lengthReg);

    JSValueType type = knownPushedType(0);

    MaybeRegisterID slotsReg, dataReg, typeReg;
    if (!analysis->popGuaranteed(PC)) {
        slotsReg = frame.allocReg();
        dataReg = frame.allocReg();
        if (type == JSVAL_TYPE_UNKNOWN || type == JSVAL_TYPE_DOUBLE)
            typeReg = frame.allocReg();
    }

    frame.unpinReg(objReg);

    /* Test for 'length == initializedLength' */
    Int32Key key = Int32Key::FromRegister(lengthReg);
    Jump initlenGuard = masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                              objReg, key, Assembler::NotEqual);
    stubcc.linkExit(initlenGuard, Uses(3));

    /* Test for length != 0 */
    Jump emptyGuard = masm.branch32(Assembler::Equal, lengthReg, Imm32(0));
    stubcc.linkExit(emptyGuard, Uses(3));

    masm.bumpKey(key, -1);

    if (dataReg.isSet()) {
        masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), slotsReg.reg());
        BaseIndex slot(slotsReg.reg(), lengthReg, masm.JSVAL_SCALE);
        Jump holeCheck = masm.fastArrayLoadSlot(slot, !isPacked, typeReg, dataReg.reg());
        if (!isPacked)
            stubcc.linkExit(holeCheck, Uses(3));
        frame.freeReg(slotsReg.reg());
    }

    masm.store32(lengthReg, Address(objReg, offsetof(JSObject, privateData)));
    masm.store32(lengthReg, Address(objReg, offsetof(JSObject, initializedLength)));

    stubcc.leave();
    stubcc.masm.move(Imm32(0), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.freeReg(lengthReg);
    frame.popn(2);

    if (dataReg.isSet()) {
        if (type == JSVAL_TYPE_UNKNOWN || type == JSVAL_TYPE_DOUBLE)
            frame.pushRegs(typeReg.reg(), dataReg.reg(), type);
        else
            frame.pushTypedPayload(type, dataReg.reg());
    } else {
        frame.push(UndefinedValue());
    }

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::compileArrayWithLength(uint32 argc)
{
    /* Match Array() or Array(n) for constant n. */
    JS_ASSERT(argc == 0 || argc == 1);

    int32 length = 0;
    if (argc == 1) {
        FrameEntry *arg = frame.peek(-1);
        if (!arg->isConstant() || !arg->getValue().isInt32())
            return Compile_InlineAbort;
        length = arg->getValue().toInt32();
        if (length < 0)
            return Compile_InlineAbort;
    }

    types::TypeObject *type = types::TypeScript::InitObject(cx, script, PC, JSProto_Array);
    if (!type)
        return Compile_Error;

    JSObject *templateObject = NewDenseUnallocatedArray(cx, length, type->proto);
    if (!templateObject)
        return Compile_Error;
    templateObject->setType(type);

    RegisterID result = frame.allocReg();
    Jump emptyFreeList = masm.getNewObject(cx, result, templateObject);

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
mjit::Compiler::compileArrayWithArgs(uint32 argc)
{
    /*
     * Match Array(x, y, z) with at least two arguments. Don't inline the case
     * where a non-number argument is passed, so we don't need to care about
     * the types of the arguments.
     */
    JS_ASSERT(argc >= 2);

    if (argc >= gc::GetGCKindSlots(gc::FINALIZE_OBJECT_LAST))
        return Compile_InlineAbort;

    types::TypeObject *type = types::TypeScript::InitObject(cx, script, PC, JSProto_Array);
    if (!type)
        return Compile_Error;

    JSObject *templateObject = NewDenseUnallocatedArray(cx, argc, type->proto);
    if (!templateObject)
        return Compile_Error;
    templateObject->setType(type);

    JS_ASSERT(templateObject->getDenseArrayCapacity() >= argc);

    RegisterID result = frame.allocReg();
    Jump emptyFreeList = masm.getNewObject(cx, result, templateObject);
    stubcc.linkExit(emptyFreeList, Uses(0));

    for (unsigned i = 0; i < argc; i++) {
        FrameEntry *arg = frame.peek(-(int)argc + i);
        frame.storeTo(arg, Address(result, JSObject::getFixedSlotOffset(i)), /* popped = */ true);
    }

    masm.storePtr(ImmPtr((void *) argc), Address(result, offsetof(JSObject, initializedLength)));

    stubcc.leave();

    stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
    OOL_STUBCALL(stubs::SlowCall, REJOIN_FALLTHROUGH);

    frame.popn(argc + 2);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, result);

    stubcc.rejoin(Changes(1));
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::inlineNativeFunction(uint32 argc, bool callingNew)
{
    if (!cx->typeInferenceEnabled())
        return Compile_InlineAbort;

    if (applyTricks == LazyArgsObj)
        return Compile_InlineAbort;

    FrameEntry *origCallee = frame.peek(-((int)argc + 2));
    FrameEntry *thisValue = frame.peek(-((int)argc + 1));
    types::TypeSet *thisTypes = analysis->poppedTypes(PC, argc);

    if (!origCallee->isConstant() || !origCallee->isType(JSVAL_TYPE_OBJECT))
        return Compile_InlineAbort;

    JSObject *callee = &origCallee->getValue().toObject();
    if (!callee->isFunction())
        return Compile_InlineAbort;

    /*
     * The callee must have the same parent as the script's global, otherwise
     * inference may not have accounted for any side effects correctly.
     */
    if (!globalObj || globalObj != callee->getGlobal())
        return Compile_InlineAbort;

    JSFunction *fun = callee->getFunctionPrivate();
    Native native = fun->maybeNative();

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

    if (argc == 0) {
        if (native == js::array_pop && thisType == JSVAL_TYPE_OBJECT) {
            /*
             * Only inline pop() on dense arrays which have never been used in
             * an iterator --- when popping elements we don't account for
             * suppressing deleted properties in active iterators.
             *
             * Constraints propagating properties directly into the result
             * type set are generated by TypeConstraintCall during inference.
             */
            if (!thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY |
                                           types::OBJECT_FLAG_ITERATED) &&
                !arrayPrototypeHasIndexedProperty()) {
                bool packed = !thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
                return compileArrayPop(thisValue, packed);
            }
        }
    } else if (argc == 1) {
        FrameEntry *arg = frame.peek(-1);
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
        if (native == js::array_push &&
            thisType == JSVAL_TYPE_OBJECT && type == JSVAL_TYPE_INT32) {
            /*
             * Constraints propagating properties into the 'this' object are
             * generated by TypeConstraintCall during inference.
             */
            if (!thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY) &&
                !arrayPrototypeHasIndexedProperty()) {
                return compileArrayPush(thisValue, arg);
            }
        }
    } else if (argc == 2) {
        FrameEntry *arg1 = frame.peek(-2);
        FrameEntry *arg2 = frame.peek(-1);

        JSValueType arg1Type = arg1->isTypeKnown() ? arg1->getKnownType() : JSVAL_TYPE_UNKNOWN;
        JSValueType arg2Type = arg2->isTypeKnown() ? arg2->getKnownType() : JSVAL_TYPE_UNKNOWN;

        if (native == js_math_pow && type == JSVAL_TYPE_DOUBLE &&
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

