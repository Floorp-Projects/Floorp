/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

#include "jsbool.h"
#include "jslibmath.h"
#include "jsnum.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

using namespace js;
using namespace js::mjit;
using namespace js::analyze;
using namespace JSC;

using mozilla::Abs;

typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

bool
mjit::Compiler::tryBinaryConstantFold(JSContext *cx, FrameState &frame, JSOp op,
                                      FrameEntry *lhs, FrameEntry *rhs, Value *vp)
{
    if (!lhs->isConstant() || !rhs->isConstant())
        return false;

    const Value &L = lhs->getValue();
    const Value &R = rhs->getValue();

    if (!L.isPrimitive() || !R.isPrimitive() ||
        (op == JSOP_ADD && (L.isString() || R.isString()))) {
        return false;
    }

    bool needInt;
    switch (op) {
      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_DIV:
        needInt = false;
        break;

      case JSOP_MOD:
        needInt = (L.isInt32() && R.isInt32() &&
                   L.toInt32() >= 0 && R.toInt32() > 0);
        break;

      default:
        JS_NOT_REACHED("NYI");
        needInt = false; /* Silence compiler warning. */
        break;
    }

    double dL = 0, dR = 0;
    int32_t nL = 0, nR = 0;
    /*
     * We don't need to check for conversion failure, since primitive conversion
     * is infallible.
     */
    if (needInt) {
        JS_ALWAYS_TRUE(ToInt32(cx, L, &nL));
        JS_ALWAYS_TRUE(ToInt32(cx, R, &nR));
    } else {
        JS_ALWAYS_TRUE(ToNumber(cx, L, &dL));
        JS_ALWAYS_TRUE(ToNumber(cx, R, &dR));
    }

    switch (op) {
      case JSOP_ADD:
        dL += dR;
        break;
      case JSOP_SUB:
        dL -= dR;
        break;
      case JSOP_MUL:
        dL *= dR;
        break;
      case JSOP_DIV:
        dL = js::NumberDiv(dL, dR);
        break;
      case JSOP_MOD:
        if (needInt)
            nL %= nR;
        else if (dR == 0)
            dL = js_NaN;
        else
            dL = js_fmod(dL, dR);
        break;

      default:
        JS_NOT_REACHED("NYI");
        break;
    }

    if (needInt)
        vp->setInt32(nL);
    else
        vp->setNumber(dL);

    return true;
}

void
mjit::Compiler::slowLoadConstantDouble(Assembler &masm,
                                       FrameEntry *fe, FPRegisterID fpreg)
{
    if (fe->getValue().isInt32())
        masm.slowLoadConstantDouble((double) fe->getValue().toInt32(), fpreg);
    else
        masm.slowLoadConstantDouble(fe->getValue().toDouble(), fpreg);
}

void
mjit::Compiler::maybeJumpIfNotInt32(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                                    MaybeRegisterID &mreg)
{
    if (!fe->isTypeKnown()) {
        if (mreg.isSet())
            mj.setJump(masm.testInt32(Assembler::NotEqual, mreg.reg()));
        else
            mj.setJump(masm.testInt32(Assembler::NotEqual, frame.addressOf(fe)));
    } else if (fe->getKnownType() != JSVAL_TYPE_INT32) {
        mj.setJump(masm.jump());
    }
}

void
mjit::Compiler::maybeJumpIfNotDouble(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                                    MaybeRegisterID &mreg)
{
    if (!fe->isTypeKnown()) {
        if (mreg.isSet())
            mj.setJump(masm.testDouble(Assembler::NotEqual, mreg.reg()));
        else
            mj.setJump(masm.testDouble(Assembler::NotEqual, frame.addressOf(fe)));
    } else if (fe->getKnownType() != JSVAL_TYPE_DOUBLE) {
        mj.setJump(masm.jump());
    }
}

bool
mjit::Compiler::jsop_binary_slow(JSOp op, VoidStub stub, JSValueType type,
                                 FrameEntry *lhs, FrameEntry *rhs)
{
    bool isStringResult = (op == JSOP_ADD) &&
                          (lhs->isType(JSVAL_TYPE_STRING) || rhs->isType(JSVAL_TYPE_STRING));
    JS_ASSERT_IF(isStringResult && type != JSVAL_TYPE_UNKNOWN, type == JSVAL_TYPE_STRING);

    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stub, REJOIN_FALLTHROUGH);
    frame.popn(2);
    frame.pushSynced(isStringResult ? JSVAL_TYPE_STRING : type);
    return true;
}

bool
mjit::Compiler::jsop_binary(JSOp op, VoidStub stub, JSValueType type, types::TypeSet *typeSet)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    Value v;
    if (tryBinaryConstantFold(cx, frame, op, lhs, rhs, &v)) {
        if (!v.isInt32() && typeSet && !typeSet->hasType(types::Type::DoubleType())) {
            /*
             * OK to ignore failure here, we aren't performing the operation
             * itself. Note that monitorOverflow will propagate the type as
             * necessary if a *INC operation overflowed.
             */
            RootedScript script(cx, script_);
            types::TypeScript::MonitorOverflow(cx, script, PC);
            return false;
        }
        frame.popn(2);
        frame.push(v);
        return true;
    }

    /*
     * Bail out if there are unhandled types or ops.
     * This is temporary while ops are still being implemented.
     */
    if ((lhs->isConstant() && rhs->isConstant()) ||
        (lhs->isTypeKnown() && (lhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)) ||
        (rhs->isTypeKnown() && (rhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)))
    {
        return jsop_binary_slow(op, stub, type, lhs, rhs);
    }

    /*
     * If this is an operation on which integer overflows can be ignored, treat
     * the result as an integer even if it has been marked as overflowing by
     * the interpreter. Doing this changes the values we maintain on the stack
     * from those the interpreter would maintain; this is OK as values derived
     * from ignored overflows are not live across points where the interpreter
     * can join into JIT code (loop heads and safe points).
     */
    CrossSSAValue pushv(a->inlineIndex, SSAValue::PushedValue(PC - script_->code, 0));
    bool cannotOverflow = loop && loop->cannotIntegerOverflow(pushv);
    bool ignoreOverflow = loop && loop->ignoreIntegerOverflow(pushv);

    if (rhs->isType(JSVAL_TYPE_INT32) && lhs->isType(JSVAL_TYPE_INT32) &&
        op == JSOP_ADD && ignoreOverflow) {
        type = JSVAL_TYPE_INT32;
    }

    /* Can do int math iff there is no double constant and the op is not division. */
    bool canDoIntMath = op != JSOP_DIV && type != JSVAL_TYPE_DOUBLE &&
                        !(rhs->isType(JSVAL_TYPE_DOUBLE) || lhs->isType(JSVAL_TYPE_DOUBLE));

    if (!masm.supportsFloatingPoint() && (!canDoIntMath || frame.haveSameBacking(lhs, rhs)))
        return jsop_binary_slow(op, stub, type, lhs, rhs);

    if (canDoIntMath)
        jsop_binary_full(lhs, rhs, op, stub, type, cannotOverflow, ignoreOverflow);
    else
        jsop_binary_double(lhs, rhs, op, stub, type);

    return true;
}

static void
EmitDoubleOp(JSOp op, FPRegisterID fpRight, FPRegisterID fpLeft, Assembler &masm)
{
    switch (op) {
      case JSOP_ADD:
        masm.addDouble(fpRight, fpLeft);
        break;

      case JSOP_SUB:
        masm.subDouble(fpRight, fpLeft);
        break;

      case JSOP_MUL:
        masm.mulDouble(fpRight, fpLeft);
        break;

      case JSOP_DIV:
        masm.divDouble(fpRight, fpLeft);
        break;

      default:
        JS_NOT_REACHED("unrecognized binary op");
    }
}

mjit::MaybeJump
mjit::Compiler::loadDouble(FrameEntry *fe, FPRegisterID *fpReg, bool *allocated)
{
    MaybeJump notNumber;

    if (!fe->isConstant() && fe->isType(JSVAL_TYPE_DOUBLE)) {
        *fpReg = frame.tempFPRegForData(fe);
        *allocated = false;
        return notNumber;
    }

    *fpReg = frame.allocFPReg();
    *allocated = true;

    if (fe->isConstant()) {
        slowLoadConstantDouble(masm, fe, *fpReg);
    } else if (!fe->isTypeKnown()) {
        frame.tempRegForType(fe);
        Jump j = frame.testDouble(Assembler::Equal, fe);
        notNumber = frame.testInt32(Assembler::NotEqual, fe);
        frame.convertInt32ToDouble(masm, fe, *fpReg);
        Jump converted = masm.jump();
        j.linkTo(masm.label(), &masm);
        // CANDIDATE
        frame.loadDouble(fe, *fpReg, masm);
        converted.linkTo(masm.label(), &masm);
    } else {
        JS_ASSERT(fe->isType(JSVAL_TYPE_INT32));
        frame.tempRegForData(fe);
        frame.convertInt32ToDouble(masm, fe, *fpReg);
    }

    return notNumber;
}

/*
 * This function emits a single fast-path for handling numerical arithmetic.
 * Unlike jsop_binary_full(), all integers are converted to doubles.
 */
void
mjit::Compiler::jsop_binary_double(FrameEntry *lhs, FrameEntry *rhs, JSOp op,
                                   VoidStub stub, JSValueType type)
{
    FPRegisterID fpLeft, fpRight;
    bool allocateLeft, allocateRight;

    MaybeJump lhsNotNumber = loadDouble(lhs, &fpLeft, &allocateLeft);
    if (lhsNotNumber.isSet())
        stubcc.linkExit(lhsNotNumber.get(), Uses(2));

    /* The left register holds the result, and needs to be mutable. */
    if (!allocateLeft) {
        FPRegisterID res = frame.allocFPReg();
        masm.moveDouble(fpLeft, res);
        fpLeft = res;
        allocateLeft = true;
    }

    MaybeJump rhsNotNumber;
    if (frame.haveSameBacking(lhs, rhs)) {
        fpRight = fpLeft;
        allocateRight = false;
    } else {
        rhsNotNumber = loadDouble(rhs, &fpRight, &allocateRight);
        if (rhsNotNumber.isSet())
            stubcc.linkExit(rhsNotNumber.get(), Uses(2));
    }

    EmitDoubleOp(op, fpRight, fpLeft, masm);

    MaybeJump done;

    /*
     * Try to convert result to integer, if the result has unknown or integer type.
     * Skip this for 1/x or -1/x, as the result is unlikely to fit in an int.
     */
    if (op == JSOP_DIV &&
        (type == JSVAL_TYPE_INT32 ||
         (type == JSVAL_TYPE_UNKNOWN &&
          !(lhs->isConstant() && lhs->isType(JSVAL_TYPE_INT32) &&
            Abs(lhs->getValue().toInt32()) == 1))))
    {
        RegisterID reg = frame.allocReg();
        FPRegisterID fpReg = frame.allocFPReg();
        JumpList isDouble;
        masm.branchConvertDoubleToInt32(fpLeft, reg, isDouble, fpReg);

        masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), reg,
                                      frame.addressOf(lhs));

        frame.freeReg(reg);
        frame.freeReg(fpReg);
        done.setJump(masm.jump());

        isDouble.linkTo(masm.label(), &masm);
    }

    /*
     * Inference needs to know about any operation on integers that produces a
     * double result. Unless the pushed type set already contains the double
     * type, we need to call a stub rather than push. Note that looking at
     * the pushed type tag is not sufficient, as it will be UNKNOWN if
     * we do not yet know the possible types of the division's operands.
     */
    types::TypeSet *resultTypes = pushedTypeSet(0);
    if (resultTypes && !resultTypes->hasType(types::Type::DoubleType())) {
        /*
         * Call a stub and try harder to convert to int32_t, failing that trigger
         * recompilation of this script.
         */
        stubcc.linkExit(masm.jump(), Uses(2));
    } else {
        JS_ASSERT(type != JSVAL_TYPE_INT32);
        if (type != JSVAL_TYPE_DOUBLE)
            masm.storeDouble(fpLeft, frame.addressOf(lhs));
    }

    if (done.isSet())
        done.getJump().linkTo(masm.label(), &masm);

    stubcc.leave();
    OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

    if (allocateRight)
        frame.freeReg(fpRight);

    frame.popn(2);

    if (type == JSVAL_TYPE_DOUBLE) {
        frame.pushDouble(fpLeft);
    } else {
        frame.freeReg(fpLeft);
        frame.pushSynced(type);
    }

    stubcc.rejoin(Changes(1));
}

/*
 * Simpler version of jsop_binary_full() for when lhs == rhs.
 */
void
mjit::Compiler::jsop_binary_full_simple(FrameEntry *fe, JSOp op, VoidStub stub, JSValueType type)
{
    FrameEntry *lhs = frame.peek(-2);

    /* Easiest case: known double. Don't bother conversion back yet? */
    if (fe->isType(JSVAL_TYPE_DOUBLE)) {
        FPRegisterID fpreg = frame.allocFPReg();
        FPRegisterID lhs = frame.tempFPRegForData(fe);
        masm.moveDouble(lhs, fpreg);
        EmitDoubleOp(op, fpreg, fpreg, masm);
        frame.popn(2);

        JS_ASSERT(type == JSVAL_TYPE_DOUBLE);  /* :XXX: can fail */
        frame.pushDouble(fpreg);
        return;
    }

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForSameBinary(fe, op, regs);

    MaybeJump notNumber;
    MaybeJump doublePathDone;
    if (!fe->isTypeKnown()) {
        Jump notInt = masm.testInt32(Assembler::NotEqual, regs.lhsType.reg());
        stubcc.linkExitDirect(notInt, stubcc.masm.label());

        notNumber = stubcc.masm.testDouble(Assembler::NotEqual, regs.lhsType.reg());
        frame.loadDouble(fe, regs.lhsFP, stubcc.masm);
        EmitDoubleOp(op, regs.lhsFP, regs.lhsFP, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(regs.lhsFP, result);

        /* Load the payload into the result reg so the rejoin is safe. */
        stubcc.masm.loadPayload(result, regs.result);

        doublePathDone = stubcc.masm.jump();
    }

    /* Okay - good to emit the integer fast-path. */
    MaybeJump overflow;
    switch (op) {
      case JSOP_ADD:
        overflow = masm.branchAdd32(Assembler::Overflow, regs.result, regs.result);
        break;

      case JSOP_SUB:
        overflow = masm.branchSub32(Assembler::Overflow, regs.result, regs.result);
        break;

      case JSOP_MUL:
        overflow = masm.branchMul32(Assembler::Overflow, regs.result, regs.result);
        break;

      default:
        JS_NOT_REACHED("unrecognized op");
    }

    JS_ASSERT(overflow.isSet());

    /*
     * Integer overflow path. Restore the original values and make a stub call,
     * which could trigger recompilation.
     */
    stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());
    frame.rematBinary(fe, NULL, regs, stubcc.masm);
    stubcc.syncExitAndJump(Uses(2));

    /* Slow paths funnel here. */
    if (notNumber.isSet())
        notNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);

    /* Slow call - use frame.sync to avoid erroneous jump repatching in stubcc. */
    frame.sync(stubcc.masm, Uses(2));
    stubcc.leave();
    OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

    /* Finish up stack operations. */
    frame.popn(2);

    if (type == JSVAL_TYPE_INT32)
        frame.pushTypedPayload(type, regs.result);
    else
        frame.pushNumber(regs.result, true);

    frame.freeReg(regs.lhsFP);

    /* Merge back OOL double path. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());

    stubcc.rejoin(Changes(1));
}

/*
 * This function emits multiple fast-paths for handling numerical arithmetic.
 * Currently, it handles only ADD, SUB, and MUL, where both LHS and RHS are
 * known not to be doubles.
 *
 * The control flow of the emitted code depends on which types are known.
 * Given both types are unknown, the full spread looks like:
 *
 * Inline                              OOL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Is LHS Int32?  ------ No -------->  Is LHS Double?  ----- No -------,
 *                                     Sync LHS                        |
 *                                     Load LHS into XMM1              |
 *                                     Is RHS Double? ---- Yes --,     |
 *                                       Is RHS Int32? ---- No --|-----|
 *                                       Convert RHS into XMM0   |     |
 *                                     Else  <-------------------'     |
 *                                       Sync RHS                      |
 *                                       Load RHS into XMM0            |
 *                                     [Add,Sub,Mul] XMM0,XMM1         |
 *                                     Jump ---------------------,     |
 *                                                               |     |
 * Is RHS Int32?  ------ No ------->   Is RHS Double? ----- No --|-----|
 *                                     Sync RHS                  |     |
 *                                     Load RHS into XMM0        |     |
 *                                     Convert LHS into XMM1     |     |
 *                                     [Add,Sub,Mul] XMM0,XMM1   |     |
 *                                     Jump ---------------------|   Slow Call
 *                                                               |
 * [Add,Sub,Mul] RHS, LHS                                        |
 * Overflow      ------ Yes ------->   Convert RHS into XMM0     |
 *                                     Convert LHS into XMM1     |
 *                                     [Add,Sub,Mul] XMM0,XMM1   |
 *                                     Sync XMM1 to stack    <---'
 *  <--------------------------------- Rejoin
 */
void
mjit::Compiler::jsop_binary_full(FrameEntry *lhs, FrameEntry *rhs, JSOp op,
                                 VoidStub stub, JSValueType type,
                                 bool cannotOverflow, bool ignoreOverflow)
{
    if (frame.haveSameBacking(lhs, rhs)) {
        jsop_binary_full_simple(lhs, op, stub, type);
        return;
    }

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs);

    /* Quick-test some invariants. */
    JS_ASSERT_IF(lhs->isTypeKnown(), lhs->getKnownType() == JSVAL_TYPE_INT32);
    JS_ASSERT_IF(rhs->isTypeKnown(), rhs->getKnownType() == JSVAL_TYPE_INT32);

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Perform the double addition. */
    MaybeJump doublePathDone;
    if (masm.supportsFloatingPoint() && (!rhs->isTypeKnown() || !lhs->isTypeKnown())) {
        /* If the LHS type was not known, link its path here. */
        if (lhsUnknownDone.isSet())
            lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Perform the double operation. */
        EmitDoubleOp(op, regs.rhsFP, regs.lhsFP, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(regs.lhsFP, result);

        /* Load the payload into the result reg so the rejoin is safe. */
        stubcc.masm.loadPayload(result, regs.result);

        /* We'll link this back up later, at the bottom of the op. */
        doublePathDone = stubcc.masm.jump();
    }

    /* Time to do the integer path. Figure out the immutable side. */
    int32_t value = 0;
    JSOp origOp = op;
    MaybeRegisterID reg;
    MaybeJump preOverflow;
    if (!regs.resultHasRhs) {
        if (!regs.rhsData.isSet())
            value = rhs->getValue().toInt32();
        else
            reg = regs.rhsData.reg();
    } else {
        if (!regs.lhsData.isSet())
            value = lhs->getValue().toInt32();
        else
            reg = regs.lhsData.reg();
        if (op == JSOP_SUB) {
            // If the RHS is 0x80000000, the smallest negative value, neg does
            // not work. Guard against this and treat it as an overflow.
            preOverflow = masm.branch32(Assembler::Equal, regs.result, Imm32(0x80000000));
            masm.neg32(regs.result);
            op = JSOP_ADD;
        }
    }

    /* Okay - good to emit the integer fast-path. */
    MaybeJump overflow;
    switch (op) {
      case JSOP_ADD:
        if (cannotOverflow || ignoreOverflow) {
            if (reg.isSet())
                masm.add32(reg.reg(), regs.result);
            else
                masm.add32(Imm32(value), regs.result);
        } else {
            if (reg.isSet())
                overflow = masm.branchAdd32(Assembler::Overflow, reg.reg(), regs.result);
            else
                overflow = masm.branchAdd32(Assembler::Overflow, Imm32(value), regs.result);
        }
        break;

      case JSOP_SUB:
        if (cannotOverflow) {
            if (reg.isSet())
                masm.sub32(reg.reg(), regs.result);
            else
                masm.sub32(Imm32(value), regs.result);
        } else {
            if (reg.isSet())
                overflow = masm.branchSub32(Assembler::Overflow, reg.reg(), regs.result);
            else
                overflow = masm.branchSub32(Assembler::Overflow, Imm32(value), regs.result);
        }
        break;

      case JSOP_MUL:
      {
        MaybeJump storeNegZero;
        bool maybeNegZero = !ignoreOverflow;
        bool hasConstant = (lhs->isConstant() || rhs->isConstant());

        if (hasConstant && maybeNegZero) {
            value = (lhs->isConstant() ? lhs : rhs)->getValue().toInt32();
            RegisterID nonConstReg = lhs->isConstant() ? regs.rhsData.reg() : regs.lhsData.reg();

            if (value > 0)
                maybeNegZero = false;
            else if (value < 0)
                storeNegZero = masm.branchTest32(Assembler::Zero, nonConstReg);
            else
                storeNegZero = masm.branch32(Assembler::LessThan, nonConstReg, Imm32(0));
        }

        if (cannotOverflow) {
            if (reg.isSet())
                masm.mul32(reg.reg(), regs.result);
            else
                masm.mul32(Imm32(value), regs.result, regs.result);
        } else {
            if (reg.isSet()) {
                overflow = masm.branchMul32(Assembler::Overflow, reg.reg(), regs.result);
            } else {
                overflow = masm.branchMul32(Assembler::Overflow, Imm32(value), regs.result,
                                            regs.result);
            }
        }

        if (maybeNegZero) {
            if (hasConstant) {
                stubcc.linkExit(storeNegZero.get(), Uses(2));
            } else {
                Jump isZero = masm.branchTest32(Assembler::Zero, regs.result);
                stubcc.linkExitDirect(isZero, stubcc.masm.label());

                /* Restore original value. */
                if (regs.resultHasRhs) {
                    if (regs.rhsNeedsRemat)
                        stubcc.masm.loadPayload(frame.addressForDataRemat(rhs), regs.result);
                    else
                        stubcc.masm.move(regs.rhsData.reg(), regs.result);
                } else {
                    if (regs.lhsNeedsRemat)
                        stubcc.masm.loadPayload(frame.addressForDataRemat(lhs), regs.result);
                    else
                        stubcc.masm.move(regs.lhsData.reg(), regs.result);
                }
                storeNegZero = stubcc.masm.branchOr32(Assembler::Signed, reg.reg(), regs.result);
                stubcc.masm.xor32(regs.result, regs.result);
                stubcc.crossJump(stubcc.masm.jump(), masm.label());
                storeNegZero.getJump().linkTo(stubcc.masm.label(), &stubcc.masm);
                frame.rematBinary(lhs, rhs, regs, stubcc.masm);
            }
            stubcc.syncExitAndJump(Uses(2));
        }
        break;
      }

      default:
        JS_NOT_REACHED("unrecognized op");
    }
    op = origOp;

    /*
     * Integer overflow path. Restore the original values and make a stub call,
     * which could trigger recompilation.
     */
    MaybeJump overflowDone;
    if (preOverflow.isSet())
        stubcc.linkExitDirect(preOverflow.get(), stubcc.masm.label());
    if (overflow.isSet())
        stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());

    /* Restore the original operand registers for ADD. */
    if (regs.undoResult) {
        if (reg.isSet()) {
            JS_ASSERT(op == JSOP_ADD);
            stubcc.masm.neg32(reg.reg());
            stubcc.masm.add32(reg.reg(), regs.result);
            stubcc.masm.neg32(reg.reg());
        } else {
            JS_ASSERT(op == JSOP_ADD || op == JSOP_SUB);
            int32_t fixValue = (op == JSOP_ADD) ? -value : value;
            stubcc.masm.add32(Imm32(fixValue), regs.result);
        }
    }

    frame.rematBinary(lhs, rhs, regs, stubcc.masm);
    stubcc.syncExitAndJump(Uses(2));

    /* The register allocator creates at most one temporary. */
    if (regs.extraFree.isSet())
        frame.freeReg(regs.extraFree.reg());

    /* Slow paths funnel here. */
    if (lhsNotDouble.isSet()) {
        lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        if (rhsNotNumber.isSet())
            rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
    }
    if (rhsNotNumber2.isSet())
        rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

    /* Slow call - use frame.sync to avoid erroneous jump repatching in stubcc. */
    frame.sync(stubcc.masm, Uses(2));
    stubcc.leave();
    OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

    /* Finish up stack operations. */
    frame.popn(2);

    /*
     * Steal the result register if we remat the LHS/RHS by undoing the operation.
     * In this case the result register was still assigned to the corresponding
     * frame entry (so it is synced properly in OOL paths), so steal it back.
     */
    if (regs.undoResult)
        frame.takeReg(regs.result);

    if (type == JSVAL_TYPE_INT32)
        frame.pushTypedPayload(type, regs.result);
    else
        frame.pushNumber(regs.result, true);

    frame.freeReg(regs.lhsFP);
    frame.freeReg(regs.rhsFP);

    /* Merge back OOL double path. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_neg()
{
    FrameEntry *fe = frame.peek(-1);
    JSValueType type = knownPushedType(0);

    if ((fe->isTypeKnown() && fe->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET) ||
        !masm.supportsFloatingPoint())
    {
        prepareStubCall(Uses(1));
        INLINE_STUBCALL(stubs::Neg, REJOIN_FALLTHROUGH);
        frame.pop();
        frame.pushSynced(type);
        return;
    }

    JS_ASSERT(!fe->isConstant());

    /* Handle negation of a known double, or of a known integer which has previously overflowed. */
    if (fe->isType(JSVAL_TYPE_DOUBLE) ||
        (fe->isType(JSVAL_TYPE_INT32) && type == JSVAL_TYPE_DOUBLE))
    {
        FPRegisterID fpreg;
        if (fe->isType(JSVAL_TYPE_DOUBLE)) {
            fpreg = frame.tempFPRegForData(fe);
        } else {
            fpreg = frame.allocFPReg();
            frame.convertInt32ToDouble(masm, fe, fpreg);
        }

        FPRegisterID res = frame.allocFPReg();
        masm.moveDouble(fpreg, res);
        masm.negateDouble(res);

        if (!fe->isType(JSVAL_TYPE_DOUBLE))
            frame.freeReg(fpreg);

        frame.pop();
        frame.pushDouble(res);

        return;
    }

    /* Inline integer path for known integers. */
    if (fe->isType(JSVAL_TYPE_INT32) && type == JSVAL_TYPE_INT32) {
        RegisterID reg = frame.copyDataIntoReg(fe);

        /* Test for 0 and -2147483648 (both result in a double). */
        Jump zeroOrMinInt = masm.branchTest32(Assembler::Zero, reg, Imm32(0x7fffffff));
        stubcc.linkExit(zeroOrMinInt, Uses(1));

        masm.neg32(reg);

        stubcc.leave();
        OOL_STUBCALL(stubs::Neg, REJOIN_FALLTHROUGH);

        frame.pop();
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

        stubcc.rejoin(Changes(1));
        return;
    }

    /* Load type information into register. */
    MaybeRegisterID feTypeReg;
    if (!fe->isTypeKnown() && !frame.shouldAvoidTypeRemat(fe)) {
        /* Safe because only one type is loaded. */
        feTypeReg.setReg(frame.tempRegForType(fe));

        /* Don't get clobbered by copyDataIntoReg(). */
        frame.pinReg(feTypeReg.reg());
    }

    RegisterID reg = frame.copyDataIntoReg(masm, fe);
    Label feSyncTarget = stubcc.syncExitAndJump(Uses(1));

    /* Try a double path (inline). */
    MaybeJump jmpNotDbl;
    {
        maybeJumpIfNotDouble(masm, jmpNotDbl, fe, feTypeReg);

        FPRegisterID fpreg = frame.allocFPReg();
        frame.loadDouble(fe, fpreg, masm);
        masm.negateDouble(fpreg);

        /* Overwrite pushed frame's memory (before push). */
        masm.storeDouble(fpreg, frame.addressOf(fe));
        frame.freeReg(fpreg);
    }

    /* Try an integer path (out-of-line). */
    MaybeJump jmpNotInt;
    MaybeJump jmpMinIntOrIntZero;
    MaybeJump jmpIntRejoin;
    Label lblIntPath = stubcc.masm.label();
    {
        maybeJumpIfNotInt32(stubcc.masm, jmpNotInt, fe, feTypeReg);

        /* Test for 0 and -2147483648 (both result in a double). */
        jmpMinIntOrIntZero = stubcc.masm.branchTest32(Assembler::Zero, reg, Imm32(0x7fffffff));

        stubcc.masm.neg32(reg);

        /* Sync back with double path. */
        if (type == JSVAL_TYPE_DOUBLE) {
            stubcc.masm.convertInt32ToDouble(reg, Registers::FPConversionTemp);
            stubcc.masm.storeDouble(Registers::FPConversionTemp, frame.addressOf(fe));
        } else {
            stubcc.masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), reg,
                                                 frame.addressOf(fe));
        }

        jmpIntRejoin.setJump(stubcc.masm.jump());
    }

    frame.freeReg(reg);
    if (feTypeReg.isSet())
        frame.unpinReg(feTypeReg.reg());

    stubcc.leave();
    OOL_STUBCALL(stubs::Neg, REJOIN_FALLTHROUGH);

    frame.pop();
    frame.pushSynced(type);

    /* Link jumps. */
    if (jmpNotDbl.isSet())
        stubcc.linkExitDirect(jmpNotDbl.getJump(), lblIntPath);

    if (jmpNotInt.isSet())
        jmpNotInt.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpMinIntOrIntZero.isSet())
        jmpMinIntOrIntZero.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpIntRejoin.isSet())
        stubcc.crossJump(jmpIntRejoin.getJump(), masm.label());

    stubcc.rejoin(Changes(1));
}

bool
mjit::Compiler::jsop_mod()
{
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    JSValueType type = knownPushedType(0);
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    Value v;
    if (tryBinaryConstantFold(cx, frame, JSOP_MOD, lhs, rhs, &v)) {
        types::TypeSet *pushed = pushedTypeSet(0);
        if (!v.isInt32() && pushed && !pushed->hasType(types::Type::DoubleType())) {
            RootedScript script(cx, script_);
            types::TypeScript::MonitorOverflow(cx, script, PC);
            return false;
        }
        frame.popn(2);
        frame.push(v);
        return true;
    }

    if ((lhs->isConstant() && rhs->isConstant()) ||
        (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (rhs->isTypeKnown() && rhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (type != JSVAL_TYPE_INT32 && type != JSVAL_TYPE_UNKNOWN))
#endif
    {
        prepareStubCall(Uses(2));
        INLINE_STUBCALL(stubs::Mod, REJOIN_FALLTHROUGH);
        frame.popn(2);
        frame.pushSynced(knownPushedType(0));
        return true;
    }

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    if (!lhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(j, Uses(2));
    }
    if (!rhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
    }

    /* LHS must be in EAX:EDX */
    if (!lhs->isConstant()) {
        frame.copyDataIntoReg(lhs, X86Registers::eax);
    } else {
        frame.takeReg(X86Registers::eax);
        masm.move(Imm32(lhs->getValue().toInt32()), X86Registers::eax);
    }

    /* Get RHS into anything but EDX - could avoid more spilling? */
    MaybeRegisterID temp;
    RegisterID rhsReg;
    uint32_t mask = Registers::AvailRegs & ~Registers::maskReg(X86Registers::edx);
    if (!rhs->isConstant()) {
        rhsReg = frame.tempRegInMaskForData(rhs, mask).reg();
        JS_ASSERT(rhsReg != X86Registers::edx);
    } else {
        rhsReg = frame.allocReg(mask).reg();
        JS_ASSERT(rhsReg != X86Registers::edx);
        masm.move(Imm32(rhs->getValue().toInt32()), rhsReg);
        temp = rhsReg;
    }
    frame.takeReg(X86Registers::edx);
    frame.freeReg(X86Registers::eax);

    if (temp.isSet())
        frame.freeReg(temp.reg());

    bool slowPath = !(lhs->isTypeKnown() && rhs->isTypeKnown());
    if (rhs->isConstant() && rhs->getValue().toInt32() != 0) {
        if (rhs->getValue().toInt32() == -1) {
            /* Guard against -1 / INT_MIN which throws a hardware exception. */
            Jump checkDivExc = masm.branch32(Assembler::Equal, X86Registers::eax,
                                             Imm32(0x80000000));
            stubcc.linkExit(checkDivExc, Uses(2));
            slowPath = true;
        }
    } else {
        Jump checkDivExc = masm.branch32(Assembler::Equal, X86Registers::eax, Imm32(0x80000000));
        stubcc.linkExit(checkDivExc, Uses(2));
        Jump checkZero = masm.branchTest32(Assembler::Zero, rhsReg, rhsReg);
        stubcc.linkExit(checkZero, Uses(2));
        slowPath = true;
    }

    /* Perform division. */
    masm.idiv(rhsReg);

    /* ECMA-262 11.5.3 requires the result to have the same sign as the lhs.
     * Thus, if the remainder of the div instruction is zero and the lhs is
     * negative, we must return negative 0. */

    bool lhsMaybeNeg = true;
    bool lhsIsNeg = false;
    if (lhs->isConstant()) {
        /* This condition is established at the top of this function. */
        JS_ASSERT(lhs->getValue().isInt32());
        lhsMaybeNeg = lhsIsNeg = (lhs->getValue().toInt32() < 0);
    }

    MaybeJump gotNegZero;
    MaybeJump done;
    if (lhsMaybeNeg) {
        MaybeRegisterID lhsData;
        if (!lhsIsNeg)
            lhsData = frame.tempRegForData(lhs);
        Jump negZero1 = masm.branchTest32(Assembler::NonZero, X86Registers::edx);
        MaybeJump negZero2;
        if (!lhsIsNeg)
            negZero2 = masm.branchTest32(Assembler::Zero, lhsData.reg(), Imm32(0x80000000));
        /*
         * Darn, negative 0. This goes to a stub call (after our in progress call)
         * which triggers recompilation if necessary.
         */
        gotNegZero = masm.jump();

        /* :TODO: This is wrong, must load into EDX as well. */

        done = masm.jump();
        negZero1.linkTo(masm.label(), &masm);
        if (negZero2.isSet())
            negZero2.getJump().linkTo(masm.label(), &masm);
    }

    /* Better - integer. */
    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(lhs));

    if (done.isSet())
        done.getJump().linkTo(masm.label(), &masm);

    if (slowPath) {
        stubcc.leave();
        OOL_STUBCALL(stubs::Mod, REJOIN_FALLTHROUGH);
    }

    frame.popn(2);

    if (type == JSVAL_TYPE_INT32)
        frame.pushTypedPayload(type, X86Registers::edx);
    else
        frame.pushNumber(X86Registers::edx);

    if (slowPath)
        stubcc.rejoin(Changes(1));

    if (gotNegZero.isSet()) {
        stubcc.linkExit(gotNegZero.getJump(), Uses(2));
        stubcc.leave();
        OOL_STUBCALL(stubs::NegZeroHelper, REJOIN_FALLTHROUGH);
        stubcc.rejoin(Changes(1));
    }
#endif

    return true;
}

bool
mjit::Compiler::jsop_equality_int_string(JSOp op, BoolStub stub,
                                         jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Swap the LHS and RHS if it makes register allocation better... or possible. */
    if (lhs->isConstant() ||
        (frame.shouldAvoidDataRemat(lhs) && !rhs->isConstant())) {
        FrameEntry *temp = rhs;
        rhs = lhs;
        lhs = temp;
    }

    bool lhsInt = lhs->isType(JSVAL_TYPE_INT32);
    bool rhsInt = rhs->isType(JSVAL_TYPE_INT32);

    /* Invert the condition if fusing with an IFEQ branch. */
    bool flipCondition = (target && fused == JSOP_IFEQ);

    /* Get the condition being tested. */
    Assembler::Condition cond;
    switch (op) {
      case JSOP_EQ:
        cond = flipCondition ? Assembler::NotEqual : Assembler::Equal;
        break;
      case JSOP_NE:
        cond = flipCondition ? Assembler::Equal : Assembler::NotEqual;
        break;
      default:
        JS_NOT_REACHED("wat");
        return false;
    }

    if (target) {
        Value rval = UndefinedValue();  /* quiet gcc warning */
        bool rhsConst = false;
        if (rhs->isConstant()) {
            rhsConst = true;
            rval = rhs->getValue();
        }

        ValueRemat lvr, rvr;
        frame.pinEntry(lhs, lvr);
        frame.pinEntry(rhs, rvr);

        /*
         * Sync everything except the top two entries.
         * We will handle the lhs/rhs in the stub call path.
         */
        frame.syncAndKill(Registers(Registers::AvailRegs), Uses(frame.frameSlots()), Uses(2));

        RegisterID tempReg = frame.allocReg();

        JaegerSpew(JSpew_Insns, " ---- BEGIN STUB CALL CODE ---- \n");

        RESERVE_OOL_SPACE(stubcc.masm);

        /* Start of the slow path for equality stub call. */
        Label stubEntry = stubcc.masm.label();

        /* The lhs/rhs need to be synced in the stub call path. */
        frame.ensureValueSynced(stubcc.masm, lhs, lvr);
        frame.ensureValueSynced(stubcc.masm, rhs, rvr);

        bool needIntPath = (!lhs->isTypeKnown() || lhsInt) && (!rhs->isTypeKnown() || rhsInt);

        frame.pop();
        frame.pop();
        frame.discardFrame();

        bool needStub = true;

#ifdef JS_MONOIC
        EqualityGenInfo ic;

        ic.cond = cond;
        ic.tempReg = tempReg;
        ic.lvr = lvr;
        ic.rvr = rvr;
        ic.stubEntry = stubEntry;
        ic.stub = stub;

        bool useIC = !a->parent && bytecodeInChunk(target);

        /* Call the IC stub, which may generate a fast path. */
        if (useIC) {
            /* Adjust for the two values just pushed. */
            ic.addrLabel = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
            ic.stubCall = OOL_STUBCALL_LOCAL_SLOTS(ic::Equality, REJOIN_BRANCH,
                                                   frame.totalDepth() + 2);
            needStub = false;
        }
#endif

        if (needStub)
            OOL_STUBCALL_LOCAL_SLOTS(stub, REJOIN_BRANCH, frame.totalDepth() + 2);

        /*
         * The stub call has no need to rejoin, since state is synced.
         * Instead, we can just test the return value.
         */
        Jump stubBranch = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                                   Registers::ReturnReg, Registers::ReturnReg);
        Jump stubFallthrough = stubcc.masm.jump();

        JaegerSpew(JSpew_Insns, " ---- END STUB CALL CODE ---- \n");
        CHECK_OOL_SPACE();

        Jump fast;
        MaybeJump firstStubJump;

        if (needIntPath) {
            if (!lhsInt) {
                Jump lhsFail = masm.testInt32(Assembler::NotEqual, lvr.typeReg());
                stubcc.linkExitDirect(lhsFail, stubEntry);
                firstStubJump = lhsFail;
            }
            if (!rhsInt) {
                Jump rhsFail = masm.testInt32(Assembler::NotEqual, rvr.typeReg());
                stubcc.linkExitDirect(rhsFail, stubEntry);
                if (!firstStubJump.isSet())
                    firstStubJump = rhsFail;
            }

            if (rhsConst)
                fast = masm.branch32(cond, lvr.dataReg(), Imm32(rval.toInt32()));
            else
                fast = masm.branch32(cond, lvr.dataReg(), rvr.dataReg());
        } else {
            Jump j = masm.jump();
            stubcc.linkExitDirect(j, stubEntry);
            firstStubJump = j;

            /* This is just a dummy jump. */
            fast = masm.jump();
        }

        /* Jump from the stub call fallthrough to here. */
        stubcc.crossJump(stubFallthrough, masm.label());

        bool *ptrampoline = NULL;
#ifdef JS_MONOIC
        /* Remember the stub label in case there is a trampoline for the IC. */
        ic.trampoline = false;
        ic.trampolineStart = stubcc.masm.label();
        if (useIC)
            ptrampoline = &ic.trampoline;
#endif

        /*
         * NB: jumpAndRun emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        if (!jumpAndRun(fast, target, &stubBranch, ptrampoline))
            return false;

#ifdef JS_MONOIC
        if (useIC) {
            ic.jumpToStub = firstStubJump;
            ic.fallThrough = masm.label();
            ic.jumpTarget = target;
            equalityICs.append(ic);
        }
#endif

    } else {
        /* No fusing. Compare, set, and push a boolean. */

        /* Should have filtered these out in the caller. */
        JS_ASSERT(!lhs->isType(JSVAL_TYPE_STRING) && !rhs->isType(JSVAL_TYPE_STRING));

        /* Test the types. */
        if ((lhs->isTypeKnown() && !lhsInt) || (rhs->isTypeKnown() && !rhsInt)) {
            stubcc.linkExit(masm.jump(), Uses(2));
        } else {
            if (!lhsInt) {
                Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
                stubcc.linkExit(lhsFail, Uses(2));
            }
            if (!rhsInt) {
                Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
                stubcc.linkExit(rhsFail, Uses(2));
            }
        }

        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

        RegisterID reg = frame.ownRegForData(lhs);

        /* x86/64's SET instruction can only take single-byte regs.*/
        RegisterID resultReg = reg;
        if (!(Registers::maskReg(reg) & Registers::SingleByteRegs))
            resultReg = frame.allocReg(Registers::SingleByteRegs).reg();

        /* Emit the compare & set. */
        if (rhs->isConstant()) {
            masm.set32(cond, reg, Imm32(rhs->getValue().toInt32()), resultReg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            masm.set32(cond, reg,
                       masm.payloadOf(frame.addressOf(rhs)),
                       resultReg);
        } else {
            masm.set32(cond, reg, frame.tempRegForData(rhs), resultReg);
        }

        /* Clean up and push a boolean. */
        frame.pop();
        frame.pop();
        if (reg != resultReg)
            frame.freeReg(reg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, resultReg);
        stubcc.rejoin(Changes(1));
    }
    return true;
}

/*
 * Emit an OOL path for a possibly double LHS, and possibly int32_t or number RHS.
 */
void
mjit::Compiler::emitLeftDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                   MaybeJump &lhsNotDouble, MaybeJump &rhsNotNumber,
                                   MaybeJump &lhsUnknownDone)
{
    /* If the LHS is not a 32-bit integer, take OOL path. */
    Jump lhsNotInt32 = masm.testInt32(Assembler::NotEqual, regs.lhsType.reg());
    stubcc.linkExitDirect(lhsNotInt32, stubcc.masm.label());

    if (!masm.supportsFloatingPoint()) {
        lhsNotDouble = stubcc.masm.jump();
        return;
    }

    /* OOL path for LHS as a double - first test LHS is double. */
    lhsNotDouble = stubcc.masm.testDouble(Assembler::NotEqual, regs.lhsType.reg());

    /* Ensure the RHS is a number. */
    MaybeJump rhsIsDouble;
    if (!rhs->isTypeKnown()) {
        rhsIsDouble = stubcc.masm.testDouble(Assembler::Equal, regs.rhsType.reg());
        rhsNotNumber = stubcc.masm.testInt32(Assembler::NotEqual, regs.rhsType.reg());
    }

    /* If RHS is constant, convert now. */
    if (rhs->isConstant())
        slowLoadConstantDouble(stubcc.masm, rhs, regs.rhsFP);
    else
        stubcc.masm.convertInt32ToDouble(regs.rhsData.reg(), regs.rhsFP);

    if (!rhs->isTypeKnown()) {
        /* Jump past double load, bind double type check. */
        Jump converted = stubcc.masm.jump();
        rhsIsDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Load the double. */
        frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                         rhs, regs.rhsFP, stubcc.masm);

        converted.linkTo(stubcc.masm.label(), &stubcc.masm);
    }

    /* Load the LHS. */
    frame.loadDouble(regs.lhsType.reg(), regs.lhsData.reg(),
                     lhs, regs.lhsFP, stubcc.masm);
    lhsUnknownDone = stubcc.masm.jump();
}

/*
 * Emit an OOL path for an integer LHS, possibly double RHS.
 */
void
mjit::Compiler::emitRightDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                    MaybeJump &rhsNotNumber2)
{
    /* If the RHS is not a double, take OOL path. */
    Jump notInt32 = masm.testInt32(Assembler::NotEqual, regs.rhsType.reg());
    stubcc.linkExitDirect(notInt32, stubcc.masm.label());

    if (!masm.supportsFloatingPoint()) {
        rhsNotNumber2 = stubcc.masm.jump();
        return;
    }

    /* Now test if RHS is a double. */
    rhsNotNumber2 = stubcc.masm.testDouble(Assembler::NotEqual, regs.rhsType.reg());

    /* We know LHS is an integer. */
    if (lhs->isConstant())
        slowLoadConstantDouble(stubcc.masm, lhs, regs.lhsFP);
    else
        stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), regs.lhsFP);

    /* Load the RHS. */
    frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                     rhs, regs.rhsFP, stubcc.masm);
}

static inline Assembler::DoubleCondition
DoubleCondForOp(JSOp op, JSOp fused)
{
    bool ifeq = fused == JSOP_IFEQ;
    switch (op) {
      case JSOP_GT:
        return ifeq
               ? Assembler::DoubleLessThanOrEqualOrUnordered
               : Assembler::DoubleGreaterThan;
      case JSOP_GE:
        return ifeq
               ? Assembler::DoubleLessThanOrUnordered
               : Assembler::DoubleGreaterThanOrEqual;
      case JSOP_LT:
        return ifeq
               ? Assembler::DoubleGreaterThanOrEqualOrUnordered
               : Assembler::DoubleLessThan;
      case JSOP_LE:
        return ifeq
               ? Assembler::DoubleGreaterThanOrUnordered
               : Assembler::DoubleLessThanOrEqual;
      default:
        JS_NOT_REACHED("unrecognized op");
        return Assembler::DoubleLessThan;
    }
}

bool
mjit::Compiler::jsop_relational_double(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    JS_ASSERT_IF(!target, fused != JSOP_IFEQ);

    FPRegisterID fpLeft, fpRight;
    bool allocateLeft, allocateRight;

    MaybeJump lhsNotNumber = loadDouble(lhs, &fpLeft, &allocateLeft);
    if (lhsNotNumber.isSet()) {
        if (target)
            stubcc.linkExitForBranch(lhsNotNumber.get());
        else
            stubcc.linkExit(lhsNotNumber.get(), Uses(2));
    }
    if (!allocateLeft)
        frame.pinReg(fpLeft);

    MaybeJump rhsNotNumber = loadDouble(rhs, &fpRight, &allocateRight);
    if (rhsNotNumber.isSet()) {
        if (target)
            stubcc.linkExitForBranch(rhsNotNumber.get());
        else
            stubcc.linkExit(rhsNotNumber.get(), Uses(2));
    }
    if (!allocateLeft)
        frame.unpinReg(fpLeft);

    Assembler::DoubleCondition dblCond = DoubleCondForOp(op, fused);

    if (target) {
        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_BRANCH);

        if (!allocateLeft)
            frame.pinReg(fpLeft);
        if (!allocateRight)
            frame.pinReg(fpRight);

        frame.syncAndKillEverything();

        Jump j = masm.branchDouble(dblCond, fpLeft, fpRight);

        if (allocateLeft)
            frame.freeReg(fpLeft);
        else
            frame.unpinKilledReg(fpLeft);

        if (allocateRight)
            frame.freeReg(fpRight);
        else
            frame.unpinKilledReg(fpRight);

        frame.popn(2);

        Jump sj = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                           Registers::ReturnReg, Registers::ReturnReg);

        /* Rejoin from the slow path. */
        stubcc.rejoin(Changes(0));

        /*
         * NB: jumpAndRun emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        if (!jumpAndRun(j, target, &sj))
            return false;
    } else {
        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

        frame.popn(2);

        RegisterID reg = frame.allocReg();
        Jump j = masm.branchDouble(dblCond, fpLeft, fpRight);
        masm.move(Imm32(0), reg);
        Jump skip = masm.jump();
        j.linkTo(masm.label(), &masm);
        masm.move(Imm32(1), reg);
        skip.linkTo(masm.label(), &masm);

        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);

        stubcc.rejoin(Changes(1));

        if (allocateLeft)
            frame.freeReg(fpLeft);
        if (allocateRight)
            frame.freeReg(fpRight);
    }

    return true;
}

bool
mjit::Compiler::jsop_relational_int(JSOp op, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Reverse N cmp A comparisons.  The left side must be in a register. */
    if (lhs->isConstant()) {
        JS_ASSERT(!rhs->isConstant());
        FrameEntry *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
        op = ReverseCompareOp(op);
    }

    JS_ASSERT_IF(!target, fused != JSOP_IFEQ);
    Assembler::Condition cond = GetCompareCondition(op, fused);

    if (target) {
        if (!frame.syncForBranch(target, Uses(2)))
            return false;

        RegisterID lreg = frame.tempRegForData(lhs);
        Jump fast;
        if (rhs->isConstant()) {
            fast = masm.branch32(cond, lreg, Imm32(rhs->getValue().toInt32()));
        } else {
            frame.pinReg(lreg);
            RegisterID rreg = frame.tempRegForData(rhs);
            frame.unpinReg(lreg);
            fast = masm.branch32(cond, lreg, rreg);
        }
        frame.popn(2);

        Jump sj = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                           Registers::ReturnReg, Registers::ReturnReg);

        return jumpAndRun(fast, target, &sj);
    } else {
        RegisterID result = frame.allocReg();
        RegisterID lreg = frame.tempRegForData(lhs);

        if (rhs->isConstant()) {
            masm.branchValue(cond, lreg, rhs->getValue().toInt32(), result);
        } else {
            frame.pinReg(lreg);
            RegisterID rreg = frame.tempRegForData(rhs);
            frame.unpinReg(lreg);
            masm.branchValue(cond, lreg, rreg, result);
        }

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
    }

    return true;
}

/* See jsop_binary_full() for more information on how this works. */
bool
mjit::Compiler::jsop_relational_full(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs, !target);

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Both double paths will join here. */
    bool hasDoublePath = false;
    if (masm.supportsFloatingPoint() && (!rhs->isTypeKnown() || !lhs->isTypeKnown()))
        hasDoublePath = true;

    /* Integer path - figure out the immutable side. */
    JSOp cmpOp = op;
    int32_t value = 0;
    RegisterID cmpReg;
    MaybeRegisterID reg;
    if (regs.lhsData.isSet()) {
        cmpReg = regs.lhsData.reg();
        if (!regs.rhsData.isSet())
            value = rhs->getValue().toInt32();
        else
            reg = regs.rhsData.reg();
    } else {
        cmpReg = regs.rhsData.reg();
        value = lhs->getValue().toInt32();
        cmpOp = ReverseCompareOp(op);
    }

    /*
     * Emit the actual comparisons. When a fusion is in play, it's faster to
     * combine the comparison with the jump, so these two cases are implemented
     * separately.
     */

    if (target) {
        /*
         * Emit the double path now, necessary to complete the OOL fast-path
         * before emitting the slow path.
         *
         * Note: doubles have not been swapped yet. Use original op.
         */
        MaybeJump doubleTest, doubleFall;
        Assembler::DoubleCondition dblCond = DoubleCondForOp(op, fused);
        if (hasDoublePath) {
            if (lhsUnknownDone.isSet())
                lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            frame.sync(stubcc.masm, Uses(frame.frameSlots()));
            doubleTest = stubcc.masm.branchDouble(dblCond, regs.lhsFP, regs.rhsFP);
            doubleFall = stubcc.masm.jump();
        }

        /* Link all incoming slow paths to here. */
        if (lhsNotDouble.isSet()) {
            lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            if (rhsNotNumber.isSet())
                rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        }
        if (rhsNotNumber2.isSet())
            rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /*
         * For fusions, spill the tracker state. xmm* remain intact. Note
         * that frame.sync() must be used directly, to avoid syncExit()'s
         * jumping logic.
         */
        frame.sync(stubcc.masm, Uses(frame.frameSlots()));
        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_BRANCH);

        /* Forget the world, preserving data. */
        frame.pinReg(cmpReg);
        if (reg.isSet())
            frame.pinReg(reg.reg());

        frame.popn(2);

        frame.syncAndKillEverything();
        frame.unpinKilledReg(cmpReg);
        if (reg.isSet())
            frame.unpinKilledReg(reg.reg());
        frame.freeReg(regs.lhsFP);
        frame.freeReg(regs.rhsFP);

        /* Operands could have been reordered, so use cmpOp. */
        Assembler::Condition i32Cond = GetCompareCondition(cmpOp, fused);

        /* Emit the i32 path. */
        Jump fast;
        if (reg.isSet())
            fast = masm.branch32(i32Cond, cmpReg, reg.reg());
        else
            fast = masm.branch32(i32Cond, cmpReg, Imm32(value));

        /*
         * The stub call has no need to rejoin since state is synced. Instead,
         * we can just test the return value.
         */
        Jump j = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                          Registers::ReturnReg, Registers::ReturnReg);

        /* Rejoin from the slow path. */
        Jump j2 = stubcc.masm.jump();
        stubcc.crossJump(j2, masm.label());

        if (hasDoublePath) {
            j.linkTo(stubcc.masm.label(), &stubcc.masm);
            doubleTest.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            j = stubcc.masm.jump();
        }

        /*
         * NB: jumpAndRun emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        if (!jumpAndRun(fast, target, &j))
            return false;

        /* Rejoin from the double path. */
        if (hasDoublePath)
            stubcc.crossJump(doubleFall.get(), masm.label());
    } else {
        /*
         * Emit the double path now, necessary to complete the OOL fast-path
         * before emitting the slow path.
         */
        MaybeJump doubleDone;
        Assembler::DoubleCondition dblCond = DoubleCondForOp(op, JSOP_NOP);
        if (hasDoublePath) {
            if (lhsUnknownDone.isSet())
                lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            /* :FIXME: Use SET if we can? */
            Jump test = stubcc.masm.branchDouble(dblCond, regs.lhsFP, regs.rhsFP);
            stubcc.masm.move(Imm32(0), regs.result);
            Jump skip = stubcc.masm.jump();
            test.linkTo(stubcc.masm.label(), &stubcc.masm);
            stubcc.masm.move(Imm32(1), regs.result);
            skip.linkTo(stubcc.masm.label(), &stubcc.masm);
            doubleDone = stubcc.masm.jump();
        }

        /* Link all incoming slow paths to here. */
        if (lhsNotDouble.isSet()) {
            lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            if (rhsNotNumber.isSet())
                rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        }
        if (rhsNotNumber2.isSet())
            rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Emit the slow path - note full frame syncage. */
        frame.sync(stubcc.masm, Uses(2));
        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

        /* Get an integer comparison condition. */
        Assembler::Condition i32Cond = GetCompareCondition(cmpOp, fused);

        /* Emit the compare & set. */
        if (reg.isSet())
            masm.branchValue(i32Cond, cmpReg, reg.reg(), regs.result);
        else
            masm.branchValue(i32Cond, cmpReg, value, regs.result);

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, regs.result);

        if (hasDoublePath)
            stubcc.crossJump(doubleDone.get(), masm.label());
        stubcc.rejoin(Changes(1));

        frame.freeReg(regs.lhsFP);
        frame.freeReg(regs.rhsFP);
    }

    return true;
}

