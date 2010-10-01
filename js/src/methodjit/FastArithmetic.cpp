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
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
 *   Sean Stangl    <sstangl@mozilla.com>
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
#include "jsnum.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

bool
mjit::Compiler::tryBinaryConstantFold(JSContext *cx, FrameState &frame, JSOp op,
                                      FrameEntry *lhs, FrameEntry *rhs)
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
      case JSOP_MOD:
        needInt = false;
        break;

      case JSOP_RSH:
        needInt = true;
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
        ValueToECMAInt32(cx, L, &nL);
        ValueToECMAInt32(cx, R, &nR);
    } else {
        ValueToNumber(cx, L, &dL);
        ValueToNumber(cx, R, &dR);
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
        if (dR == 0) {
#ifdef XP_WIN
            if (JSDOUBLE_IS_NaN(dR))
                dL = js_NaN;
            else
#endif
            if (dL == 0 || JSDOUBLE_IS_NaN(dL))
                dL = js_NaN;
            else if (JSDOUBLE_IS_NEG(dL) != JSDOUBLE_IS_NEG(dR))
                dL = cx->runtime->negativeInfinityValue.toDouble();
            else
                dL = cx->runtime->positiveInfinityValue.toDouble();
        } else {
            dL /= dR;
        }
        break;
      case JSOP_MOD:
        if (dL == 0)
            dL = js_NaN;
        else
            dL = js_fmod(dR, dL);
        break;

      case JSOP_RSH:
        nL >>= (nR & 31);
        break;

      default:
        JS_NOT_REACHED("NYI");
        break;
    }

    Value v;
    if (needInt)
        v.setInt32(nL);
    else
        v.setNumber(dL);
    frame.popn(2);
    frame.push(v);

    return true;
}

void
mjit::Compiler::slowLoadConstantDouble(Assembler &masm,
                                       FrameEntry *fe, FPRegisterID fpreg)
{
    DoublePatch patch;
    if (fe->getKnownType() == JSVAL_TYPE_INT32)
        patch.d = (double)fe->getValue().toInt32();
    else
        patch.d = fe->getValue().toDouble();
    patch.label = masm.loadDouble(NULL, fpreg);
    patch.ool = &masm != &this->masm;
    JS_ASSERT_IF(patch.ool, &masm == &stubcc.masm);
    doubleList.append(patch);
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

void
mjit::Compiler::jsop_binary(JSOp op, VoidStub stub)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    if (tryBinaryConstantFold(cx, frame, op, lhs, rhs))
        return;

    /*
     * Bail out if there are unhandled types or ops.
     * This is temporary while ops are still being implemented.
     */
    if ((op == JSOP_MOD) ||
        (lhs->isTypeKnown() && (lhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)) ||
        (rhs->isTypeKnown() && (rhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)) 
#if defined(JS_CPU_ARM)
        /* ARM cannot detect integer overflow with multiplication. */
        || op == JSOP_MUL
#endif /* JS_CPU_ARM */
    ) {
        bool isStringResult = (op == JSOP_ADD) &&
                              (lhs->isType(JSVAL_TYPE_STRING) ||
                               rhs->isType(JSVAL_TYPE_STRING));

        prepareStubCall(Uses(2));
        stubCall(stub);
        frame.popn(2);
        if (isStringResult)
            frame.pushSyncedType(JSVAL_TYPE_STRING);
        else
            frame.pushSynced();
        return;
    }

    /* Can do int math iff there is no double constant and the op is not division. */
    bool canDoIntMath = op != JSOP_DIV &&
                        !((rhs->isTypeKnown() && rhs->getKnownType() == JSVAL_TYPE_DOUBLE) ||
                          (lhs->isTypeKnown() && lhs->getKnownType() == JSVAL_TYPE_DOUBLE));

    if (canDoIntMath)
        jsop_binary_full(lhs, rhs, op, stub);
    else
        jsop_binary_double(lhs, rhs, op, stub);
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
mjit::Compiler::loadDouble(FrameEntry *fe, FPRegisterID fpReg)
{
    MaybeJump notNumber;

    if (fe->isConstant()) {
        slowLoadConstantDouble(masm, fe, fpReg);
    } else if (!fe->isTypeKnown()) {
        frame.tempRegForType(fe);
        Jump j = frame.testDouble(Assembler::Equal, fe);
        notNumber = frame.testInt32(Assembler::NotEqual, fe);
        frame.convertInt32ToDouble(masm, fe, fpReg);
        Jump converted = masm.jump();
        j.linkTo(masm.label(), &masm);
        // CANDIDATE
        frame.loadDouble(fe, fpReg, masm);
        converted.linkTo(masm.label(), &masm);
    } else if (fe->getKnownType() == JSVAL_TYPE_INT32) {
        frame.tempRegForData(fe);
        frame.convertInt32ToDouble(masm, fe, fpReg);
    } else {
        JS_ASSERT(fe->getKnownType() == JSVAL_TYPE_DOUBLE);
        frame.loadDouble(fe, fpReg, masm);
    }

    return notNumber;
}

/*
 * This function emits a single fast-path for handling numerical arithmetic.
 * Unlike jsop_binary_full(), all integers are converted to doubles.
 */
void
mjit::Compiler::jsop_binary_double(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotNumber = loadDouble(lhs, fpLeft);
    if (lhsNotNumber.isSet())
        stubcc.linkExit(lhsNotNumber.get(), Uses(2));

    MaybeJump rhsNotNumber;
    if (frame.haveSameBacking(lhs, rhs)) {
        masm.moveDouble(fpLeft, fpRight);
    } else {
        rhsNotNumber = loadDouble(rhs, fpRight);
        if (rhsNotNumber.isSet())
            stubcc.linkExit(rhsNotNumber.get(), Uses(2));
    }

    EmitDoubleOp(op, fpRight, fpLeft, masm);
    
    MaybeJump done;
    
    /*
     * Try to convert result to integer. Skip this for 1/x or -1/x, as the
     * result is unlikely to fit in an int.
     */
    if (op == JSOP_DIV && !(lhs->isConstant() && lhs->isType(JSVAL_TYPE_INT32) &&
        abs(lhs->getValue().toInt32()) == 1)) {
        RegisterID reg = frame.allocReg();
        JumpList isDouble;
        masm.branchConvertDoubleToInt32(fpLeft, reg, isDouble, fpRight);
        
        masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), reg,
                                      frame.addressOf(lhs));
        
        frame.freeReg(reg);
        done.setJump(masm.jump());
        isDouble.linkTo(masm.label(), &masm);
    }

    masm.storeDouble(fpLeft, frame.addressOf(lhs));

    if (done.isSet())
        done.getJump().linkTo(masm.label(), &masm);

    if (lhsNotNumber.isSet() || rhsNotNumber.isSet()) {
        stubcc.leave();
        stubcc.call(stub);
    }

    frame.popn(2);
    frame.pushNumber(MaybeRegisterID());

    if (lhsNotNumber.isSet() || rhsNotNumber.isSet())
        stubcc.rejoin(Changes(1));
}

/*
 * Simpler version of jsop_binary_full() for when lhs == rhs.
 */
void
mjit::Compiler::jsop_binary_full_simple(FrameEntry *fe, JSOp op, VoidStub stub)
{
    FrameEntry *lhs = frame.peek(-2);

    /* Easiest case: known double. Don't bother conversion back yet? */
    if (fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_DOUBLE) {
        loadDouble(fe, FPRegisters::First);
        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, masm);
        frame.popn(2);
        frame.pushNumber(MaybeRegisterID());
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
        frame.loadDouble(fe, FPRegisters::First, stubcc.masm);
        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(FPRegisters::First, result);

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

#if !defined(JS_CPU_ARM)
      case JSOP_MUL:
        overflow = masm.branchMul32(Assembler::Overflow, regs.result, regs.result);
        break;
#endif

      default:
        JS_NOT_REACHED("unrecognized op");
    }
    
    JS_ASSERT(overflow.isSet());

    /*
     * Integer overflow path. Separate from the first double path, since we
     * know never to try and convert back to integer.
     */
    MaybeJump overflowDone;
    stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());
    {
        if (regs.lhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressForDataRemat(lhs));
            stubcc.masm.convertInt32ToDouble(address, FPRegisters::First);
        } else if (!lhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), FPRegisters::First);
        } else {
            slowLoadConstantDouble(stubcc.masm, lhs, FPRegisters::First);
        }

        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, stubcc.masm);

        Address address = frame.addressOf(lhs);
        stubcc.masm.storeDouble(FPRegisters::First, address);
        stubcc.masm.loadPayload(address, regs.result);

        overflowDone = stubcc.masm.jump();
    }

    /* Slow paths funnel here. */
    if (notNumber.isSet())
        notNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
    overflowDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);

    /* Slow call - use frame.sync to avoid erroneous jump repatching in stubcc. */
    frame.sync(stubcc.masm, Uses(2));
    stubcc.leave();
    stubcc.call(stub);

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result, true);

    /* Merge back OOL double paths. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());
    stubcc.linkRejoin(overflowDone.get());

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
mjit::Compiler::jsop_binary_full(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub)
{
    if (frame.haveSameBacking(lhs, rhs)) {
        jsop_binary_full_simple(lhs, op, stub);
        return;
    }

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs);

    /* Quick-test some invariants. */
    JS_ASSERT_IF(lhs->isTypeKnown(), lhs->getKnownType() == JSVAL_TYPE_INT32);
    JS_ASSERT_IF(rhs->isTypeKnown(), rhs->getKnownType() == JSVAL_TYPE_INT32);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Perform the double addition. */
    MaybeJump doublePathDone;
    if (!rhs->isTypeKnown() || lhsUnknownDone.isSet()) {
        /* If the LHS type was not known, link its path here. */
        if (lhsUnknownDone.isSet())
            lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        
        /* Perform the double operation. */
        EmitDoubleOp(op, fpRight, fpLeft, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(fpLeft, result);

        /* Load the payload into the result reg so the rejoin is safe. */
        stubcc.masm.loadPayload(result, regs.result);

        /* We'll link this back up later, at the bottom of the op. */
        doublePathDone = stubcc.masm.jump();
    }

    /* Time to do the integer path. Figure out the immutable side. */
    int32 value = 0;
    JSOp origOp = op;
    MaybeRegisterID reg;
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
            masm.neg32(regs.result);
            op = JSOP_ADD;
        }
    }

    /* Okay - good to emit the integer fast-path. */
    MaybeJump overflow, negZeroDone;
    switch (op) {
      case JSOP_ADD:
        if (reg.isSet())
            overflow = masm.branchAdd32(Assembler::Overflow, reg.reg(), regs.result);
        else
            overflow = masm.branchAdd32(Assembler::Overflow, Imm32(value), regs.result);
        break;

      case JSOP_SUB:
        if (reg.isSet())
            overflow = masm.branchSub32(Assembler::Overflow, reg.reg(), regs.result);
        else
            overflow = masm.branchSub32(Assembler::Overflow, Imm32(value), regs.result);
        break;

#if !defined(JS_CPU_ARM)
      case JSOP_MUL:
      {
        JS_ASSERT(reg.isSet());
        
        MaybeJump storeNegZero;
        bool maybeNegZero = true;
        bool hasConstant = (lhs->isConstant() || rhs->isConstant());
        
        if (hasConstant) {
            value = (lhs->isConstant() ? lhs : rhs)->getValue().toInt32();
            RegisterID nonConstReg = lhs->isConstant() ? regs.rhsData.reg() : regs.lhsData.reg();

            if (value > 0)
                maybeNegZero = false;
            else if (value < 0)
                storeNegZero = masm.branchTest32(Assembler::Zero, nonConstReg);
            else
                storeNegZero = masm.branch32(Assembler::LessThan, nonConstReg, Imm32(0));
        }
        overflow = masm.branchMul32(Assembler::Overflow, reg.reg(), regs.result);

        if (maybeNegZero) {
            if (!hasConstant) {
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
            } else {
                JS_ASSERT(storeNegZero.isSet());
                stubcc.linkExitDirect(storeNegZero.get(), stubcc.masm.label());
            }
            stubcc.masm.storeValue(DoubleValue(-0.0), frame.addressOf(lhs));
            stubcc.masm.loadPayload(frame.addressOf(lhs), regs.result);
            negZeroDone = stubcc.masm.jump();
        }
        break;
      }
#endif

      default:
        JS_NOT_REACHED("unrecognized op");
    }
    op = origOp;
    
    JS_ASSERT(overflow.isSet());

    /*
     * Integer overflow path. Separate from the first double path, since we
     * know never to try and convert back to integer.
     */
    MaybeJump overflowDone;
    stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());
    {
        if (regs.lhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressForDataRemat(lhs));
            stubcc.masm.convertInt32ToDouble(address, fpLeft);
        } else if (!lhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), fpLeft);
        } else {
            slowLoadConstantDouble(stubcc.masm, lhs, fpLeft);
        }

        if (regs.rhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressForDataRemat(rhs));
            stubcc.masm.convertInt32ToDouble(address, fpRight);
        } else if (!rhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.rhsData.reg(), fpRight);
        } else {
            slowLoadConstantDouble(stubcc.masm, rhs, fpRight);
        }

        EmitDoubleOp(op, fpRight, fpLeft, stubcc.masm);

        Address address = frame.addressOf(lhs);
        stubcc.masm.storeDouble(fpLeft, address);
        stubcc.masm.loadPayload(address, regs.result);

        overflowDone = stubcc.masm.jump();
    }

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
    stubcc.call(stub);

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result, true);

    /* Merge back OOL double paths. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());
    if (negZeroDone.isSet())
        stubcc.linkRejoin(negZeroDone.get());
    stubcc.linkRejoin(overflowDone.get());

    stubcc.rejoin(Changes(1));
}

static const uint64 DoubleNegMask = 0x8000000000000000ULL;

void
mjit::Compiler::jsop_neg()
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isTypeKnown() && fe->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET) {
        prepareStubCall(Uses(1));
        stubCall(stubs::Neg);
        frame.pop();
        frame.pushSynced();
        return;
    }

    JS_ASSERT(!fe->isConstant());

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

        FPRegisterID fpreg = frame.copyEntryIntoFPReg(fe, FPRegisters::First);

#if defined JS_CPU_X86 || defined JS_CPU_X64
        masm.loadDouble(&DoubleNegMask, FPRegisters::Second);
        masm.xorDouble(FPRegisters::Second, fpreg);
#elif defined JS_CPU_ARM
        masm.negDouble(fpreg, fpreg);
#endif

        /* Overwrite pushed frame's memory (before push). */
        masm.storeDouble(fpreg, frame.addressOf(fe));
    }

    /* Try an integer path (out-of-line). */
    MaybeJump jmpNotInt;
    MaybeJump jmpIntZero;
    MaybeJump jmpMinInt;
    MaybeJump jmpIntRejoin;
    Label lblIntPath = stubcc.masm.label();
    {
        maybeJumpIfNotInt32(stubcc.masm, jmpNotInt, fe, feTypeReg);

        /* 0 (int) -> -0 (double). */
        jmpIntZero.setJump(stubcc.masm.branch32(Assembler::Equal, reg, Imm32(0)));
        /* int32 negation on (-2147483648) yields (-2147483648). */
        jmpMinInt.setJump(stubcc.masm.branch32(Assembler::Equal, reg, Imm32(1 << 31)));

        stubcc.masm.neg32(reg);

        /* Sync back with double path. */
        stubcc.masm.storeValueFromComponents(ImmType(JSVAL_TYPE_INT32), reg,
                                             frame.addressOf(fe));

        jmpIntRejoin.setJump(stubcc.masm.jump());
    }

    frame.freeReg(reg);
    if (feTypeReg.isSet())
        frame.unpinReg(feTypeReg.reg());

    stubcc.leave();
    stubcc.call(stubs::Neg);

    frame.pop();
    frame.pushSynced();

    /* Link jumps. */
    if (jmpNotDbl.isSet())
        stubcc.linkExitDirect(jmpNotDbl.getJump(), lblIntPath);

    if (jmpNotInt.isSet())
        jmpNotInt.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpIntZero.isSet())
        jmpIntZero.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpMinInt.isSet())
        jmpMinInt.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpIntRejoin.isSet())
        stubcc.crossJump(jmpIntRejoin.getJump(), masm.label());

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_mod()
{
#if defined(JS_CPU_X86)
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);
    if ((lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (rhs->isTypeKnown() && rhs->getKnownType() != JSVAL_TYPE_INT32))
#endif
    {
        prepareStubCall(Uses(2));
        stubCall(stubs::Mod);
        frame.popn(2);
        frame.pushSynced();
        return;
    }

#if defined(JS_CPU_X86)
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
    if (!rhs->isConstant()) {
        uint32 mask = Registers::AvailRegs & ~Registers::maskReg(X86Registers::edx);
        rhsReg = frame.tempRegInMaskForData(rhs, mask);
        JS_ASSERT(rhsReg != X86Registers::edx);
    } else {
        rhsReg = frame.allocReg(Registers::AvailRegs & ~Registers::maskReg(X86Registers::edx));
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

    MaybeJump done;
    if (lhsMaybeNeg) {
        MaybeRegisterID lhsData;
        if (!lhsIsNeg)
            lhsData = frame.tempRegForData(lhs);
        Jump negZero1 = masm.branchTest32(Assembler::NonZero, X86Registers::edx);
        MaybeJump negZero2;
        if (!lhsIsNeg)
            negZero2 = masm.branchTest32(Assembler::Zero, lhsData.reg(), Imm32(0x80000000));
        /* Darn, negative 0. */
        masm.storeValue(DoubleValue(-0.0), frame.addressOf(lhs));

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
        stubcc.call(stubs::Mod);
    }

    frame.popn(2);
    frame.pushNumber(X86Registers::edx);

    if (slowPath)
        stubcc.rejoin(Changes(1));
#endif
}

void
mjit::Compiler::jsop_equality_int_string(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
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
    bool lhsString = lhs->isType(JSVAL_TYPE_STRING);
    bool rhsString = rhs->isType(JSVAL_TYPE_STRING);

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
        return;
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
        frame.syncAndKill(Registers(Registers::AvailRegs), Uses(frame.frameDepth()), Uses(2));

        /* Temporary for OOL string path. */
        RegisterID T1 = frame.allocReg();

        frame.pop();
        frame.pop();
        frame.discardFrame();

        /* Start of the slow path for equality stub call. */
        Label stubCall = stubcc.masm.label();

        JaegerSpew(JSpew_Insns, " ---- BEGIN STUB CALL CODE ---- \n");

        /* The lhs/rhs need to be synced in the stub call path. */
        frame.syncEntry(stubcc.masm, lhs, lvr);
        frame.syncEntry(stubcc.masm, rhs, rvr);

        /* Call the stub, adjusting for the two values just pushed. */
        stubcc.call(stub, frame.stackDepth() + script->nfixed + 2);

        /*
         * The stub call has no need to rejoin, since state is synced.
         * Instead, we can just test the return value.
         */
        Assembler::Condition ncond = (fused == JSOP_IFEQ)
                                   ? Assembler::Zero
                                   : Assembler::NonZero;
        Jump stubBranch =
            stubcc.masm.branchTest32(ncond, Registers::ReturnReg, Registers::ReturnReg);
        Jump stubFallthrough = stubcc.masm.jump();

        JaegerSpew(JSpew_Insns, " ---- END STUB CALL CODE ---- \n");

        /* Emit an OOL string path if both sides might be strings. */
        bool stringPath = !(lhsInt || rhsInt);
        Label missedInt = stubCall;
        Jump stringFallthrough;
        Jump stringMatched;

        if (stringPath) {
            missedInt = stubcc.masm.label();

            if (!lhsString) {
                Jump lhsFail = stubcc.masm.testString(Assembler::NotEqual, lvr.typeReg());
                lhsFail.linkTo(stubCall, &stubcc.masm);
            }
            if (!rhsString) {
                JS_ASSERT(!rhsConst);
                Jump rhsFail = stubcc.masm.testString(Assembler::NotEqual, rvr.typeReg());
                rhsFail.linkTo(stubCall, &stubcc.masm);
            }

            /* Test if lhs/rhs are atomized. */
            Imm32 atomizedFlags(JSString::FLAT | JSString::ATOMIZED);

            stubcc.masm.load32(Address(lvr.dataReg(), offsetof(JSString, mLengthAndFlags)), T1);
            stubcc.masm.and32(Imm32(JSString::TYPE_FLAGS_MASK), T1);
            Jump lhsNotAtomized = stubcc.masm.branch32(Assembler::NotEqual, T1, atomizedFlags);
            lhsNotAtomized.linkTo(stubCall, &stubcc.masm);

            if (!rhsConst) {
                stubcc.masm.load32(Address(rvr.dataReg(), offsetof(JSString, mLengthAndFlags)), T1);
                stubcc.masm.and32(Imm32(JSString::TYPE_FLAGS_MASK), T1);
                Jump rhsNotAtomized = stubcc.masm.branch32(Assembler::NotEqual, T1, atomizedFlags);
                rhsNotAtomized.linkTo(stubCall, &stubcc.masm);
            }

            if (rhsConst) {
                JSString *str = rval.toString();
                JS_ASSERT(str->isAtomized());
                stringMatched = stubcc.masm.branchPtr(cond, lvr.dataReg(), ImmPtr(str));
            } else {
                stringMatched = stubcc.masm.branchPtr(cond, lvr.dataReg(), rvr.dataReg());
            }

            stringFallthrough = stubcc.masm.jump();
        }

        Jump fast;
        if (lhsString || rhsString) {
            /* Jump straight to the OOL string path. */
            Jump jump = masm.jump();
            stubcc.linkExitDirect(jump, missedInt);
            fast = masm.jump();
        } else {
            /* Emit inline integer path. */
            if (!lhsInt) {
                Jump lhsFail = masm.testInt32(Assembler::NotEqual, lvr.typeReg());
                stubcc.linkExitDirect(lhsFail, missedInt);
            }
            if (!rhsInt) {
                if (rhsConst) {
                    Jump rhsFail = masm.jump();
                    stubcc.linkExitDirect(rhsFail, missedInt);
                } else {
                    Jump rhsFail = masm.testInt32(Assembler::NotEqual, rvr.typeReg());
                    stubcc.linkExitDirect(rhsFail, missedInt);
                }
            }

            if (rhsConst)
                fast = masm.branch32(cond, lvr.dataReg(), Imm32(rval.toInt32()));
            else
                fast = masm.branch32(cond, lvr.dataReg(), rvr.dataReg());
        }

        /* Jump from the stub call and string path fallthroughs to here. */
        stubcc.crossJump(stubFallthrough, masm.label());
        if (stringPath)
            stubcc.crossJump(stringFallthrough, masm.label());

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(fast, target, &stubBranch, stringPath ? &stringMatched : NULL);
    } else {
        /* No fusing. Compare, set, and push a boolean. */

        /* Should have filtered these out in the caller. */
        JS_ASSERT(!lhsString && !rhsString);

        /* Test the types. */
        if (!lhsInt) {
            Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
            stubcc.linkExit(lhsFail, Uses(2));
        }
        if (!rhsInt) {
            Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
            stubcc.linkExit(rhsFail, Uses(2));
        }

        stubcc.leave();
        stubcc.call(stub);

        RegisterID reg = frame.ownRegForData(lhs);

        /* x86/64's SET instruction can only take single-byte regs.*/
        RegisterID resultReg = reg;
        if (!(Registers::maskReg(reg) & Registers::SingleByteRegs))
            resultReg = frame.allocReg(Registers::SingleByteRegs);

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
}

/*
 * Emit an OOL path for a possibly double LHS, and possibly int32 or number RHS.
 */
void
mjit::Compiler::emitLeftDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                   MaybeJump &lhsNotDouble, MaybeJump &rhsNotNumber,
                                   MaybeJump &lhsUnknownDone)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    /* If the LHS is not a 32-bit integer, take OOL path. */
    Jump lhsNotInt32 = masm.testInt32(Assembler::NotEqual, regs.lhsType.reg());
    stubcc.linkExitDirect(lhsNotInt32, stubcc.masm.label());

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
        slowLoadConstantDouble(stubcc.masm, rhs, fpRight);
    else
        stubcc.masm.convertInt32ToDouble(regs.rhsData.reg(), fpRight);

    if (!rhs->isTypeKnown()) {
        /* Jump past double load, bind double type check. */
        Jump converted = stubcc.masm.jump();
        rhsIsDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Load the double. */
        frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                         rhs, fpRight, stubcc.masm);

        converted.linkTo(stubcc.masm.label(), &stubcc.masm);
    }

    /* Load the LHS. */
    frame.loadDouble(regs.lhsType.reg(), regs.lhsData.reg(),
                     lhs, fpLeft, stubcc.masm);
    lhsUnknownDone = stubcc.masm.jump();
}

/*
 * Emit an OOL path for an integer LHS, possibly double RHS.
 */
void
mjit::Compiler::emitRightDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                    MaybeJump &rhsNotNumber2)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    /* If the RHS is not a double, take OOL path. */
    Jump notInt32 = masm.testInt32(Assembler::NotEqual, regs.rhsType.reg());
    stubcc.linkExitDirect(notInt32, stubcc.masm.label());

    /* Now test if RHS is a double. */
    rhsNotNumber2 = stubcc.masm.testDouble(Assembler::NotEqual, regs.rhsType.reg());

    /* We know LHS is an integer. */
    if (lhs->isConstant())
        slowLoadConstantDouble(stubcc.masm, lhs, fpLeft);
    else
        stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), fpLeft);

    /* Load the RHS. */
    frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                     rhs, fpRight, stubcc.masm);
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

void
mjit::Compiler::jsop_relational_double(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    JS_ASSERT_IF(!target, fused != JSOP_IFEQ);

    MaybeJump lhsNotNumber = loadDouble(lhs, fpLeft);
    MaybeJump rhsNotNumber = loadDouble(rhs, fpRight);

    Assembler::DoubleCondition dblCond = DoubleCondForOp(op, fused);

    if (target) {
        if (lhsNotNumber.isSet())
            stubcc.linkExitForBranch(lhsNotNumber.get());
        if (rhsNotNumber.isSet())
            stubcc.linkExitForBranch(rhsNotNumber.get());
        stubcc.leave();
        stubcc.call(stub);

        frame.popn(2);
        frame.syncAndForgetEverything();

        Jump j = masm.branchDouble(dblCond, fpLeft, fpRight);

        /*
         * The stub call has no need to rejoin since the state is synced.
         * Instead, we can just test the return value.
         */
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump sj = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);

        /* Rejoin from the slow path. */
        Jump j2 = stubcc.masm.jump();
        stubcc.crossJump(j2, masm.label());

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(j, target, &sj);
    } else {
        if (lhsNotNumber.isSet())
            stubcc.linkExit(lhsNotNumber.get(), Uses(2));
        if (rhsNotNumber.isSet())
            stubcc.linkExit(rhsNotNumber.get(), Uses(2));
        stubcc.leave();
        stubcc.call(stub);

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
    }
}

void
mjit::Compiler::jsop_relational_self(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
#ifdef DEBUG
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    JS_ASSERT(frame.haveSameBacking(lhs, rhs));
#endif

    /* :TODO: optimize this?  */
    emitStubCmpOp(stub, target, fused);
}

/* See jsop_binary_full() for more information on how this works. */
void
mjit::Compiler::jsop_relational_full(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs, !target);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Both double paths will join here. */
    bool hasDoublePath = false;
    if (!rhs->isTypeKnown() || lhsUnknownDone.isSet())
        hasDoublePath = true;

    /* Integer path - figure out the immutable side. */
    JSOp cmpOp = op;
    int32 value = 0;
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
        switch (op) {
          case JSOP_GT:
            cmpOp = JSOP_LT;
            break;
          case JSOP_GE:
            cmpOp = JSOP_LE;
            break;
          case JSOP_LT:
            cmpOp = JSOP_GT;
            break;
          case JSOP_LE:
            cmpOp = JSOP_GE;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            break;
        }
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
            frame.sync(stubcc.masm, Uses(frame.frameDepth()));
            doubleTest = stubcc.masm.branchDouble(dblCond, fpLeft, fpRight);
            doubleFall = stubcc.masm.jump();

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
            frame.sync(stubcc.masm, Uses(frame.frameDepth()));
            stubcc.leave();
            stubcc.call(stub);
        }

        /* Forget the world, preserving data. */
        frame.pinReg(cmpReg);
        if (reg.isSet())
            frame.pinReg(reg.reg());
        
        frame.popn(2);

        frame.syncAndKillEverything();
        frame.unpinKilledReg(cmpReg);
        if (reg.isSet())
            frame.unpinKilledReg(reg.reg());
        frame.syncAndForgetEverything();
        
        /* Operands could have been reordered, so use cmpOp. */
        Assembler::Condition i32Cond;
        bool ifeq = fused == JSOP_IFEQ;
        switch (cmpOp) {
          case JSOP_GT:
            i32Cond = ifeq ? Assembler::LessThanOrEqual : Assembler::GreaterThan;
            break;
          case JSOP_GE:
            i32Cond = ifeq ? Assembler::LessThan : Assembler::GreaterThanOrEqual;
            break;
          case JSOP_LT:
            i32Cond = ifeq ? Assembler::GreaterThanOrEqual : Assembler::LessThan;
            break;
          case JSOP_LE:
            i32Cond = ifeq ? Assembler::GreaterThan : Assembler::LessThanOrEqual;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            return;
        }

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
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump j = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);

        /* Rejoin from the slow path. */
        Jump j2 = stubcc.masm.jump();
        stubcc.crossJump(j2, masm.label());

        /* :TODO: make double path invoke tracer. */
        if (hasDoublePath) {
            j.linkTo(stubcc.masm.label(), &stubcc.masm);
            doubleTest.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            j = stubcc.masm.jump();
        }

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(fast, target, &j);

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
            Jump test = stubcc.masm.branchDouble(dblCond, fpLeft, fpRight);
            stubcc.masm.move(Imm32(0), regs.result);
            Jump skip = stubcc.masm.jump();
            test.linkTo(stubcc.masm.label(), &stubcc.masm);
            stubcc.masm.move(Imm32(1), regs.result);
            skip.linkTo(stubcc.masm.label(), &stubcc.masm);
            doubleDone = stubcc.masm.jump();

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
            stubcc.call(stub);
        }

        /* Get an integer comparison condition. */
        Assembler::Condition i32Cond;
        switch (cmpOp) {
          case JSOP_GT:
            i32Cond = Assembler::GreaterThan;
            break;
          case JSOP_GE:
            i32Cond = Assembler::GreaterThanOrEqual;
            break;
          case JSOP_LT:
            i32Cond = Assembler::LessThan;
            break;
          case JSOP_LE:
            i32Cond = Assembler::LessThanOrEqual;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            return;
        }

        /* Emit the compare & set. */
        if (Registers::maskReg(regs.result) & Registers::SingleByteRegs) {
            if (reg.isSet())
                masm.set32(i32Cond, cmpReg, reg.reg(), regs.result);
            else
                masm.set32(i32Cond, cmpReg, Imm32(value), regs.result);
        } else {
            Jump j;
            if (reg.isSet())
                j = masm.branch32(i32Cond, cmpReg, reg.reg());
            else
                j = masm.branch32(i32Cond, cmpReg, Imm32(value));
            masm.move(Imm32(0), regs.result);
            Jump skip = masm.jump();
            j.linkTo(masm.label(),  &masm);
            masm.move(Imm32(1), regs.result);
            skip.linkTo(masm.label(), &masm);
        }

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, regs.result);

        if (hasDoublePath)
            stubcc.crossJump(doubleDone.get(), masm.label());
        stubcc.rejoin(Changes(1));
    }
}

