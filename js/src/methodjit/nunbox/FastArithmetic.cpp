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

typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

static inline bool
JSOpBinaryTryConstantFold(JSContext *cx, FrameState &frame, JSOp op, FrameEntry *lhs, FrameEntry *rhs)
{
    if (!lhs->isConstant() || !rhs->isConstant())
        return false;

    const Value &L = lhs->getValue();
    const Value &R = rhs->getValue();

    if (!L.isPrimitive() || !R.isPrimitive() ||
        (op == JSOP_ADD && (L.isString() || R.isString()))) {
        return false;
    }

    double dL, dR;
    ValueToNumber(cx, L, &dL);
    ValueToNumber(cx, R, &dR);

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

      default:
        JS_NOT_REACHED("NYI");
        break;
    }

    Value v;
    v.setNumber(dL);
    frame.popn(2);
    frame.push(v);

    return true;
}

/*
 * TODO: Replace with fast constant loading.
 * This is not part of the FrameState because fast constant loading
 * is inherently a Compiler task, and this code should not be used
 * except temporarily.
 */
void
mjit::Compiler::slowLoadConstantDouble(Assembler &masm,
                                       FrameEntry *fe, FPRegisterID fpreg)
{
    jsval_layout jv;
    if (fe->getKnownType() == JSVAL_TYPE_INT32)
        jv.asDouble = (double)fe->getValue().toInt32();
    else
        jv.asDouble = fe->getValue().toDouble();

    masm.storeLayout(jv, frame.addressOf(fe));
    masm.loadDouble(frame.addressOf(fe), fpreg);
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

    if (JSOpBinaryTryConstantFold(cx, frame, op, lhs, rhs))
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
                              ((lhs->isTypeKnown() && lhs->getKnownType() == JSVAL_TYPE_STRING) ||
                               (rhs->isTypeKnown() && rhs->getKnownType() == JSVAL_TYPE_STRING));
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

mjit::Compiler::MaybeJump
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

    MaybeJump rhsNotNumber;
    if (frame.haveSameBacking(lhs, rhs))
        masm.moveDouble(fpLeft, fpRight);
    else
        rhsNotNumber = loadDouble(rhs, fpRight);

    EmitDoubleOp(op, fpRight, fpLeft, masm);
    masm.storeDouble(fpLeft, frame.addressOf(lhs));

    if (lhsNotNumber.isSet())
        stubcc.linkExit(lhsNotNumber.get(), Uses(2));
    if (rhsNotNumber.isSet())
        stubcc.linkExit(rhsNotNumber.get(), Uses(2));

    stubcc.leave();
    stubcc.call(stub);

    frame.popn(2);
    frame.pushNumber(MaybeRegisterID());

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
            Address address = masm.payloadOf(frame.addressOf(lhs));
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

    /* Slow call. */
    stubcc.syncExit(Uses(2));
    stubcc.leave();
    stubcc.call(stub);

    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(lhs));

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result);

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

    MaybeJump lhsNotDouble;
    MaybeJump rhsNotNumber;
    MaybeJump lhsUnknownDone;
    if (!lhs->isTypeKnown()) {
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
            frame.loadDouble(rhs, fpRight, stubcc.masm);

            converted.linkTo(stubcc.masm.label(), &stubcc.masm);
        }

        /* Load the LHS. */
        frame.loadDouble(lhs, fpLeft, stubcc.masm);
        lhsUnknownDone = stubcc.masm.jump();
    }

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown()) {
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
        frame.loadDouble(rhs, fpRight, stubcc.masm);
    }

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
    MaybeJump overflow;
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
        JS_ASSERT(reg.isSet());
        overflow = masm.branchMul32(Assembler::Overflow, reg.reg(), regs.result);
        break;
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
            Address address = masm.payloadOf(frame.addressOf(lhs));
            stubcc.masm.convertInt32ToDouble(address, fpLeft);
        } else if (!lhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), fpLeft);
        } else {
            slowLoadConstantDouble(stubcc.masm, lhs, fpLeft);
        }

        if (regs.rhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressOf(rhs));
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

    /* Slow call. */
    stubcc.syncExit(Uses(2));
    stubcc.leave();
    stubcc.call(stub);

    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(lhs));

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result);

    /* Merge back OOL double paths. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());
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

    /* 
     * Load type information into register.
     */
    MaybeRegisterID feTypeReg;
    if (!fe->isTypeKnown() && !frame.shouldAvoidTypeRemat(fe)) {
        /* Safe because only one type is loaded. */
        feTypeReg.setReg(frame.tempRegForType(fe));

        /* Don't get clobbered by copyDataIntoReg(). */
        frame.pinReg(feTypeReg.reg());
    }

    /*
     * If copyDataIntoReg() is called here, syncAllRegs() is not required.
     * If called from the int path, the double and int states would
     * need to be explicitly synced in a currently unsupported manner.
     *
     * This function call can call allocReg() and clobber any register.
     * (It is also useful to think about the interplay between this
     *  code being in the int path, versus stub call syncing..)
     */
    RegisterID reg = frame.copyDataIntoReg(masm, fe);
    Label feSyncTarget = stubcc.syncExitAndJump(Uses(1));

    /* Try a double path (inline). */
    MaybeJump jmpNotDbl;
    {
        maybeJumpIfNotDouble(masm, jmpNotDbl, fe, feTypeReg);

        FPRegisterID fpreg = frame.copyEntryIntoFPReg(fe, FPRegisters::First);

#ifdef JS_CPU_X86
        masm.loadDouble(&DoubleNegMask, FPRegisters::Second);
        masm.xorDouble(FPRegisters::Second, fpreg);
#else
        masm.negDouble(fpreg, fpreg);
#endif

        /* Overwrite pushed frame's memory (before push). */
        masm.storeDouble(fpreg, frame.addressOf(fe));
    }

    /* Try an integer path (out-of-line). */
    MaybeJump jmpNotInt;
    MaybeJump jmpIntZero;
    MaybeJump jmpIntRejoin;
    Label lblIntPath = stubcc.masm.label();
    {
        maybeJumpIfNotInt32(stubcc.masm, jmpNotInt, fe, feTypeReg);

        /* 0 (int) -> -0 (double). */
        jmpIntZero.setJump(stubcc.masm.branch32(Assembler::Equal, reg, Imm32(0)));

        stubcc.masm.neg32(reg);

        /* Sync back with double path. */
        stubcc.masm.storePayload(reg, frame.addressOf(fe));
        stubcc.masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(fe));

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
    if (jmpIntRejoin.isSet())
        stubcc.crossJump(jmpIntRejoin.getJump(), masm.label());

    stubcc.rejoin(Changes(1));
}

