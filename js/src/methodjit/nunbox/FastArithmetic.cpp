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
                dL = cx->runtime->negativeInfinityValue.asDouble();
            else
                dL = cx->runtime->positiveInfinityValue.asDouble();
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

void
mjit::Compiler::jsop_binary_intmath(JSOp op, RegisterID *returnReg, MaybeJump &jmpOverflow)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* 
     * One of the values should not be a constant.
     * If there is a constant, force it to be in rhs.
     */
    bool swapped = false;
    if (lhs->isConstant()) {
        JS_ASSERT(!rhs->isConstant());
        swapped = true;
        FrameEntry *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    RegisterID reg = Registers::ReturnReg;
    reg = frame.copyDataIntoReg(lhs);
    if (swapped && op == JSOP_SUB) {
        masm.neg32(reg);
        op = JSOP_ADD;
    }

    Jump fail;
    switch(op) {
      case JSOP_ADD:
        if (rhs->isConstant()) {
            fail = masm.branchAdd32(Assembler::Overflow,
                                    Imm32(rhs->getValue().asInt32()), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            fail = masm.branchAdd32(Assembler::Overflow,
                                    frame.addressOf(rhs), reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            fail = masm.branchAdd32(Assembler::Overflow,
                                    rhsReg, reg);
        }
        break;

      case JSOP_SUB:
        if (rhs->isConstant()) {
            fail = masm.branchSub32(Assembler::Overflow,
                                    Imm32(rhs->getValue().asInt32()), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            fail = masm.branchSub32(Assembler::Overflow,
                                    frame.addressOf(rhs), reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            fail = masm.branchSub32(Assembler::Overflow,
                                    rhsReg, reg);
        }
        break;

#if !defined(JS_CPU_ARM)
      case JSOP_MUL:
        if (rhs->isConstant()) {
            RegisterID rhsReg = frame.copyConstantIntoReg(rhs);
            fail = masm.branchMul32(Assembler::Overflow,
                                    rhsReg, reg);
            frame.freeReg(rhsReg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            fail = masm.branchMul32(Assembler::Overflow,
                                    frame.addressOf(rhs), reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            fail = masm.branchMul32(Assembler::Overflow,
                                    rhsReg, reg);
        }
        break;
#endif /* JS_CPU_ARM */

      default:
        JS_NOT_REACHED("unhandled int32 op.");
        break;
    }
    
    *returnReg = reg;
    jmpOverflow.setJump(fail);
}

void
mjit::Compiler::jsop_binary_dblmath(JSOp op, FPRegisterID rfp, FPRegisterID lfp)
{
    switch (op) {
      case JSOP_ADD:
        stubcc.masm.addDouble(rfp, lfp);
        break;
      case JSOP_SUB:
        stubcc.masm.subDouble(rfp, lfp);
        break;
      case JSOP_MUL:
        stubcc.masm.mulDouble(rfp, lfp);
        break;
      case JSOP_DIV:
        stubcc.masm.divDouble(rfp, lfp);
        break;
      default:
        JS_NOT_REACHED("unhandled double op.");
        break;
    }
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
    jsdpun u;
    if (fe->getKnownType() == JSVAL_TYPE_INT32)
        u.d = (double)fe->getValue().asInt32();
    else
        u.d = fe->getValue().asDouble();

    masm.storeData32(Imm32(u.s.lo), frame.addressOf(fe));
    masm.storeTypeTag(ImmTag(JSValueTag(u.s.hi)), frame.addressOf(fe));
    masm.loadDouble(frame.addressOf(fe), fpreg);
}

void
mjit::Compiler::maybeJumpIfNotInt32(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                                    MaybeRegisterID &mreg)
{
    if (!fe->isTypeKnown()) {
        if (mreg.isSet())
            mj.setJump(masm.testInt32(Assembler::NotEqual, mreg.getReg()));
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
            mj.setJump(masm.testDouble(Assembler::NotEqual, mreg.getReg()));
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
    /* Predict the address of the returned FrameEntry. */
    FrameEntry *returnFe = lhs;

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
        prepareStubCall();
        stubCall(stub, Uses(2), Defs(1));
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

    frame.syncAllRegs(Registers::AvailRegs);

    /*
     * Since both frame-syncing/stub-call code and the double
     * fast-path are emitted into the secondary buffer, we attempt
     * to minimize the number of jumps by forcing all fast-path exits
     * to be synced prior to generating any code for doubles.
     * 
     * The required registers for type-checking are 'pre-allocated' here
     * by invoking register allocation logic. This logic works only if
     * the registers are used in the order that they are allocated,
     * and only if success is guaranteed after the type-checking step.
     *
     * Fast-paths for ints and doubles cannot use type information:
     * ints can be converted to doubles in case of overflow, and
     * doubles can be converted to ints in case of roundness.
     * It is therefore not possible to optimize away a type check
     * unless a value's type is known to be constant.
     */
    MaybeRegisterID rhsTypeReg;
    bool rhsTypeRegNeedsLoad = false;
    if (!rhs->isTypeKnown() && !frame.shouldAvoidTypeRemat(rhs)) {
        rhsTypeRegNeedsLoad = !frame.peekTypeInRegister(rhs);
        rhsTypeReg.setReg(frame.predictRegForType(rhs));
    }
    Label rhsSyncTarget = stubcc.syncExitAndJump();
    
    MaybeRegisterID lhsTypeReg;
    bool lhsTypeRegNeedsLoad = false;
    if (!lhs->isTypeKnown() && !frame.shouldAvoidTypeRemat(lhs)) {
        lhsTypeRegNeedsLoad = !frame.peekTypeInRegister(lhs);
        lhsTypeReg.setReg(frame.predictRegForType(lhs));
    }
    Label lhsSyncTarget = rhsSyncTarget;
    if (!rhsTypeReg.isSet() || lhsTypeReg.isSet())
        lhsSyncTarget = stubcc.syncExitAndJump();


    /* Reassigned by jsop_binary_intmath() */
    RegisterID returnReg = Registers::ReturnReg;

    /*
     * The conversion path logic is slightly redundant because
     * we hold the invariant that the lhsTypeReg's type cannot be
     * loaded until all uses of the rhsTypeReg are finished.
     * This invariant helps prevent reloading type data
     * in the case of rhsTypeReg == lhsTypeReg.
     */
    FPRegisterID rfp = FPRegisters::First;
    FPRegisterID lfp = FPRegisters::Second;

    /*
     * Conversion Path 2: rhs known double; lhs not double.
     * Jumped to from double path if lhs not double.
     * Partially jumped to from above conversion path.
     */
    MaybeJump jmpCvtPath2;
    MaybeJump jmpCvtPath2NotInt;
    Label lblCvtPath2 = stubcc.masm.label();
    {
        /* Don't bother emitting double conversion for a known double. */
        if (!lhs->isTypeKnown() || lhs->getKnownType() != JSVAL_TYPE_DOUBLE) {
            maybeJumpIfNotInt32(stubcc.masm, jmpCvtPath2NotInt, lhs, lhsTypeReg);

            if (!lhs->isConstant())
                frame.convertInt32ToDouble(stubcc.masm, lhs, lfp);
            else
                slowLoadConstantDouble(stubcc.masm, lhs, lfp);

            jmpCvtPath2.setJump(stubcc.masm.jump());
        }
    }
    
    /*
     * Conversion Path 3: rhs int; lhs not int.
     * Jumped to from int path if lhs not int.
     */
    MaybeJump jmpCvtPath3;
    MaybeJump jmpCvtPath3NotDbl;
    Label lblCvtPath3 = stubcc.masm.label();
    {
        /* Don't bother emitting double checking code for a known int. */
        if (!lhs->isTypeKnown() || lhs->getKnownType() != JSVAL_TYPE_INT32) {
            maybeJumpIfNotDouble(stubcc.masm, jmpCvtPath3NotDbl, lhs, lhsTypeReg);

            if (!rhs->isConstant())
                frame.convertInt32ToDouble(stubcc.masm, rhs, rfp);
            else
                slowLoadConstantDouble(stubcc.masm, rhs, rfp);

            frame.copyEntryIntoFPReg(stubcc.masm, lhs, lfp);
            jmpCvtPath3.setJump(stubcc.masm.jump());
        }
    }

    /* Double exits */
    MaybeJump jmpRhsNotDbl;
    MaybeJump jmpLhsNotDbl;
    Jump jmpDblRejoin;
    /* Double joins */
    Label lblDblRhsTest = stubcc.masm.label();
    Label lblDblDoMath;
    {
        /* Try a double fastpath. */

        /* rhsTypeReg must have already been loaded by the int path. */
        maybeJumpIfNotDouble(stubcc.masm, jmpRhsNotDbl, rhs, rhsTypeReg);
        frame.copyEntryIntoFPReg(stubcc.masm, rhs, rfp);

        if (lhsTypeRegNeedsLoad)
            frame.emitLoadTypeTag(stubcc.masm, lhs, lhsTypeReg.getReg());
        maybeJumpIfNotDouble(stubcc.masm, jmpLhsNotDbl, lhs, lhsTypeReg);
        frame.copyEntryIntoFPReg(stubcc.masm, lhs, lfp);

        lblDblDoMath = stubcc.masm.label();
        jsop_binary_dblmath(op, rfp, lfp);

        /*
         * Double syncing/rejoining code is emitted after the integer path,
         * so that returnReg is set in the FrameState. This is a hack.
         */
    }


    /*
     * Integer code must come after double code: tempRegForType(),
     * used by jsop_binary_intmath(), changes compiler state.
     */

    /* Integer exits */
    MaybeJump jmpRhsNotInt;
    MaybeJump jmpLhsNotInt;
    MaybeJump jmpOverflow;
    MaybeJump jmpIntDiv;
    {
        /* Try an integer fastpath. */
        if (rhsTypeRegNeedsLoad)
            frame.emitLoadTypeTag(rhs, rhsTypeReg.getReg());
        maybeJumpIfNotInt32(masm, jmpRhsNotInt, rhs, rhsTypeReg);

        if (lhsTypeRegNeedsLoad)
            frame.emitLoadTypeTag(lhs, lhsTypeReg.getReg());
        maybeJumpIfNotInt32(masm, jmpLhsNotInt, lhs, lhsTypeReg);

        /*
         * Only perform integer math if there is no constant
         * double value forcing type conversion.
         */
        if (canDoIntMath)
            jsop_binary_intmath(op, &returnReg, jmpOverflow);

        if (op == JSOP_DIV)
           jmpIntDiv.setJump(masm.jump()); 
    }
    

    /* Sync/rejoin double path. */
    {
        /*
         * Since there does not exist a floating-point register allocator,
         * the calculated value must be synced to memory.
         */
        stubcc.masm.storeDouble(lfp, frame.addressOf(returnFe));
        if (canDoIntMath)
            stubcc.masm.loadData32(frame.addressOf(returnFe), returnReg);

        jmpDblRejoin = stubcc.masm.jump();
    }

    /*
     * Conversion Path 1: rhs known int; lhs known int.
     * Jumped to on overflow, or for division conversion.
     *
     * TODO: We can reuse instructions from Conversion Path 2
     * to handle conversion for the lhs. However, this involves
     * fighting the FrameState, which is currently unadvised.
     */
    MaybeJump jmpCvtPath1;
    Label lblCvtPath1 = stubcc.masm.label();
    {
        if (canDoIntMath || op == JSOP_DIV) {
            /*
             * TODO: Constants should be handled by rip/PC-relative
             * addressing on x86_64/ARM, and absolute addressing
             * on x86. This requires coercing masm into emitting
             * constants into the instruction stream.
             * Such an approach is safe because each conversion
             * path is directly jumped to: no code can fall through.
             */
            if (!lhs->isConstant())
                frame.convertInt32ToDouble(stubcc.masm, lhs, lfp);
            else
                slowLoadConstantDouble(stubcc.masm, lhs, lfp);

            if (!rhs->isConstant())
                frame.convertInt32ToDouble(stubcc.masm, rhs, rfp);
            else
                slowLoadConstantDouble(stubcc.masm, rhs, rfp);

            jmpCvtPath1.setJump(stubcc.masm.jump());
        }
    }


    /* Patch up jumps. */
    if (jmpRhsNotInt.isSet())
        stubcc.linkExitDirect(jmpRhsNotInt.getJump(), lblDblRhsTest);
    if (jmpLhsNotInt.isSet())
        stubcc.linkExitDirect(jmpLhsNotInt.getJump(), lblCvtPath3);
    if (jmpOverflow.isSet())
        stubcc.linkExitDirect(jmpOverflow.getJump(), lblCvtPath1);
    if (jmpIntDiv.isSet())
        stubcc.linkExitDirect(jmpIntDiv.getJump(), lblCvtPath1);

    if (jmpRhsNotDbl.isSet())
        jmpRhsNotDbl.getJump().linkTo(rhsSyncTarget, &stubcc.masm);
    if (jmpLhsNotDbl.isSet())
        jmpLhsNotDbl.getJump().linkTo(lblCvtPath2, &stubcc.masm);

    if (jmpCvtPath1.isSet())
        jmpCvtPath1.getJump().linkTo(lblDblDoMath, &stubcc.masm);
    if (jmpCvtPath2.isSet())
        jmpCvtPath2.getJump().linkTo(lblDblDoMath, &stubcc.masm);
    if (jmpCvtPath2NotInt.isSet())
        jmpCvtPath2NotInt.getJump().linkTo(lhsSyncTarget, &stubcc.masm);
    if (jmpCvtPath3.isSet())
        jmpCvtPath3.getJump().linkTo(lblDblDoMath, &stubcc.masm);
    if (jmpCvtPath3NotDbl.isSet())
        jmpCvtPath3NotDbl.getJump().linkTo(lhsSyncTarget, &stubcc.masm);
    
        
    /* Leave. */
    stubcc.leave();
    stubcc.call(stub);

    frame.popn(2);
    if (canDoIntMath)
        frame.pushUntypedPayload(JSVAL_TYPE_INT32, returnReg);
    else
        frame.pushSynced();

    stubcc.crossJump(jmpDblRejoin, masm.label());
    stubcc.rejoin(1);
}

static const uint64 DoubleNegMask = 0x8000000000000000ULL;

void
mjit::Compiler::jsop_neg()
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isTypeKnown() && fe->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET) {
        prepareStubCall();
        stubCall(stubs::Neg, Uses(1), Defs(1));
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
        frame.pinReg(feTypeReg.getReg());
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
    Label feSyncTarget = stubcc.syncExitAndJump();

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
        stubcc.masm.storeData32(reg, frame.addressOf(fe));
        stubcc.masm.storeTypeTag(ImmTag(JSVAL_TAG_INT32), frame.addressOf(fe));

        jmpIntRejoin.setJump(stubcc.masm.jump());
    }

    frame.freeReg(reg);
    if (feTypeReg.isSet())
        frame.unpinReg(feTypeReg.getReg());

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

    stubcc.rejoin(1);
}

