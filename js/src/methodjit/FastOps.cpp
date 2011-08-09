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
#include "jscntxt.h"
#include "jsemit.h"
#include "jslibmath.h"
#include "jsnum.h"
#include "jsscope.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "jstypedarrayinlines.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

typedef JSC::MacroAssembler::RegisterID RegisterID;

void
mjit::Compiler::ensureInteger(FrameEntry *fe, Uses uses)
{
    if (fe->isConstant()) {
        if (!fe->isType(JSVAL_TYPE_INT32)) {
            JS_ASSERT(fe->isType(JSVAL_TYPE_DOUBLE));
            fe->convertConstantDoubleToInt32(cx);
        }
    } else if (fe->isType(JSVAL_TYPE_DOUBLE)) {
        FPRegisterID fpreg = frame.tempFPRegForData(fe);
        FPRegisterID fptemp = frame.allocFPReg();
        RegisterID data = frame.allocReg();
        Jump truncateGuard = masm.branchTruncateDoubleToInt32(fpreg, data);

        Label syncPath = stubcc.syncExitAndJump(uses);
        stubcc.linkExitDirect(truncateGuard, stubcc.masm.label());

        /*
         * Try an OOL path to convert doubles representing integers within 2^32
         * of a signed integer, by adding/subtracting 2^32 and then trying to
         * convert to int32. This has to be an exact conversion, as otherwise
         * the truncation works incorrectly on the modified value.
         */

        stubcc.masm.zeroDouble(fptemp);
        Jump positive = stubcc.masm.branchDouble(Assembler::DoubleGreaterThan, fpreg, fptemp);
        stubcc.masm.slowLoadConstantDouble(double(4294967296.0), fptemp);
        Jump skip = stubcc.masm.jump();
        positive.linkTo(stubcc.masm.label(), &stubcc.masm);
        stubcc.masm.slowLoadConstantDouble(double(-4294967296.0), fptemp);
        skip.linkTo(stubcc.masm.label(), &stubcc.masm);

        JumpList isDouble;
        stubcc.masm.addDouble(fpreg, fptemp);
        stubcc.masm.branchConvertDoubleToInt32(fptemp, data, isDouble, Registers::FPConversionTemp);
        stubcc.crossJump(stubcc.masm.jump(), masm.label());
        isDouble.linkTo(syncPath, &stubcc.masm);

        frame.freeReg(fptemp);
        frame.learnType(fe, JSVAL_TYPE_INT32, data);
    } else if (!fe->isType(JSVAL_TYPE_INT32)) {
        FPRegisterID fptemp = frame.allocFPReg();
        RegisterID typeReg = frame.tempRegForType(fe);
        frame.pinReg(typeReg);
        RegisterID dataReg = frame.copyDataIntoReg(fe);
        frame.unpinReg(typeReg);

        Jump intGuard = masm.testInt32(Assembler::NotEqual, typeReg);

        Label syncPath = stubcc.syncExitAndJump(uses);
        stubcc.linkExitDirect(intGuard, stubcc.masm.label());

        /* Try an OOL path to truncate doubles representing int32s. */
        Jump doubleGuard = stubcc.masm.testDouble(Assembler::NotEqual, typeReg);
        doubleGuard.linkTo(syncPath, &stubcc.masm);

        frame.loadDouble(fe, fptemp, stubcc.masm);
        Jump truncateGuard = stubcc.masm.branchTruncateDoubleToInt32(fptemp, dataReg);
        truncateGuard.linkTo(syncPath, &stubcc.masm);
        stubcc.crossJump(stubcc.masm.jump(), masm.label());

        frame.freeReg(fptemp);
        frame.learnType(fe, JSVAL_TYPE_INT32, dataReg);
    }
}

void
mjit::Compiler::jsop_bitnot()
{
    FrameEntry *top = frame.peek(-1);

    /* We only want to handle integers here. */
    if (top->isNotType(JSVAL_TYPE_INT32) && top->isNotType(JSVAL_TYPE_DOUBLE)) {
        prepareStubCall(Uses(1));
        INLINE_STUBCALL(stubs::BitNot, REJOIN_FALLTHROUGH);
        frame.pop();
        frame.pushSynced(JSVAL_TYPE_INT32);
        return;
    }

    ensureInteger(top, Uses(1));

    stubcc.leave();
    OOL_STUBCALL(stubs::BitNot, REJOIN_FALLTHROUGH);

    RegisterID reg = frame.ownRegForData(top);
    masm.not32(reg);
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_bitop(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The operands we ensure are integers cannot be copied by each other. */
    frame.separateBinaryEntries(lhs, rhs);

    VoidStub stub;
    switch (op) {
      case JSOP_BITOR:
        stub = stubs::BitOr;
        break;
      case JSOP_BITAND:
        stub = stubs::BitAnd;
        break;
      case JSOP_BITXOR:
        stub = stubs::BitXor;
        break;
      case JSOP_LSH:
        stub = stubs::Lsh;
        break;
      case JSOP_RSH:
        stub = stubs::Rsh;
        break;
      case JSOP_URSH:
        stub = stubs::Ursh;
        break;
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    bool lhsIntOrDouble = !(lhs->isNotType(JSVAL_TYPE_DOUBLE) && 
                            lhs->isNotType(JSVAL_TYPE_INT32));

    /* Fast-path double to int conversion. */
    if (!lhs->isConstant() && rhs->isConstant() && lhsIntOrDouble &&
        rhs->isType(JSVAL_TYPE_INT32) && rhs->getValue().toInt32() == 0 &&
        (op == JSOP_BITOR || op == JSOP_LSH)) {
        ensureInteger(lhs, Uses(2));
        RegisterID reg = frame.ownRegForData(lhs);

        stubcc.leave();
        OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

        stubcc.rejoin(Changes(1));
        return;
    }

    /* Convert a double RHS to integer if it's constant for the test below. */
    if (rhs->isConstant() && rhs->getValue().isDouble())
        rhs->convertConstantDoubleToInt32(cx);

    /* We only want to handle integers here. */
    if ((lhs->isNotType(JSVAL_TYPE_INT32) && lhs->isNotType(JSVAL_TYPE_DOUBLE)) ||
        (rhs->isNotType(JSVAL_TYPE_INT32) && rhs->isNotType(JSVAL_TYPE_DOUBLE)) ||
        (op == JSOP_URSH && rhs->isConstant() && rhs->getValue().toInt32() % 32 == 0)) {
        prepareStubCall(Uses(2));
        INLINE_STUBCALL(stub, REJOIN_FALLTHROUGH);
        frame.popn(2);
        frame.pushSynced(op != JSOP_URSH ? JSVAL_TYPE_INT32 : knownPushedType(0));
        return;
    }

    ensureInteger(lhs, Uses(2));
    ensureInteger(rhs, Uses(2));

    if (lhs->isConstant() && rhs->isConstant()) {
        int32 L = lhs->getValue().toInt32();
        int32 R = rhs->getValue().toInt32();

        frame.popn(2);
        switch (op) {
          case JSOP_BITOR:
            frame.push(Int32Value(L | R));
            return;
          case JSOP_BITXOR:
            frame.push(Int32Value(L ^ R));
            return;
          case JSOP_BITAND:
            frame.push(Int32Value(L & R));
            return;
          case JSOP_LSH:
            frame.push(Int32Value(L << (R & 31)));
            return;
          case JSOP_RSH:
            frame.push(Int32Value(L >> (R & 31)));
            return;
          case JSOP_URSH:
          {
            uint32 unsignedL;
            ValueToECMAUint32(cx, Int32Value(L), (uint32_t*)&unsignedL);  /* Can't fail. */
            Value v = NumberValue(uint32(unsignedL >> (R & 31)));
            JS_ASSERT(v.isInt32());
            frame.push(v);
            return;
          }
          default:
            JS_NOT_REACHED("say wat");
        }
    }

    RegisterID reg;

    switch (op) {
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITAND:
      {
        /* Commutative, and we're guaranteed both are ints. */
        if (lhs->isConstant()) {
            JS_ASSERT(!rhs->isConstant());
            FrameEntry *temp = rhs;
            rhs = lhs;
            lhs = temp;
        }

        reg = frame.ownRegForData(lhs);
        if (rhs->isConstant()) {
            int32 rhsInt = rhs->getValue().toInt32();
            if (op == JSOP_BITAND)
                masm.and32(Imm32(rhsInt), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(Imm32(rhsInt), reg);
            else
                masm.or32(Imm32(rhsInt), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            Address rhsAddr = masm.payloadOf(frame.addressOf(rhs));
            if (op == JSOP_BITAND)
                masm.and32(rhsAddr, reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(rhsAddr, reg);
            else
                masm.or32(rhsAddr, reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            if (op == JSOP_BITAND)
                masm.and32(rhsReg, reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(rhsReg, reg);
            else
                masm.or32(rhsReg, reg);
        }

        break;
      }

      case JSOP_LSH:
      case JSOP_RSH:
      case JSOP_URSH:
      {
        /* Not commutative. */
        if (rhs->isConstant()) {
            RegisterID reg = frame.ownRegForData(lhs);
            int shift = rhs->getValue().toInt32() & 0x1F;

            stubcc.leave();
            OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

            if (shift) {
                if (op == JSOP_LSH)
                    masm.lshift32(Imm32(shift), reg);
                else if (op == JSOP_RSH)
                    masm.rshift32(Imm32(shift), reg);
                else
                    masm.urshift32(Imm32(shift), reg);
            }
            frame.popn(2);
            
            /* x >>> 0 may result in a double, handled above. */
            JS_ASSERT_IF(op == JSOP_URSH, shift >= 1);
            frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

            stubcc.rejoin(Changes(1));
            return;
        }
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
        /* Grosssssss! RHS _must_ be in ECX, on x86 */
        RegisterID rr = frame.tempRegInMaskForData(rhs,
                                                   Registers::maskReg(JSC::X86Registers::ecx)).reg();
#else
        RegisterID rr = frame.tempRegForData(rhs);
#endif

        if (frame.haveSameBacking(lhs, rhs)) {
            // It's okay to allocReg(). If |rr| is evicted, it won't result in
            // a load, and |rr == reg| is fine since this is (x << x).
            reg = frame.allocReg();
            if (rr != reg)
                masm.move(rr, reg);
        } else {
            frame.pinReg(rr);
            if (lhs->isConstant()) {
                reg = frame.allocReg();
                masm.move(Imm32(lhs->getValue().toInt32()), reg);
            } else {
                reg = frame.copyDataIntoReg(lhs);
            }
            frame.unpinReg(rr);
        }
        
        if (op == JSOP_LSH) {
            masm.lshift32(rr, reg);
        } else if (op == JSOP_RSH) {
            masm.rshift32(rr, reg);
        } else {
            masm.urshift32(rr, reg);
            
            Jump isNegative = masm.branch32(Assembler::LessThan, reg, Imm32(0));
            stubcc.linkExit(isNegative, Uses(2));
        }
        break;
      }

      default:
        JS_NOT_REACHED("NYI");
        return;
    }

    stubcc.leave();
    OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

    frame.pop();
    frame.pop();

    JSValueType type = knownPushedType(0);

    if (type != JSVAL_TYPE_UNKNOWN && type != JSVAL_TYPE_DOUBLE)
        frame.pushTypedPayload(type, reg);
    else if (op == JSOP_URSH)
        frame.pushNumber(reg, true);
    else
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    stubcc.rejoin(Changes(1));
}

static inline bool
CheckNullOrUndefined(FrameEntry *fe)
{
    if (!fe->isTypeKnown())
        return false;
    JSValueType type = fe->getKnownType();
    return type == JSVAL_TYPE_NULL || type == JSVAL_TYPE_UNDEFINED;
}

bool
mjit::Compiler::jsop_equality(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    bool lhsTest;
    if ((lhsTest = CheckNullOrUndefined(lhs)) || CheckNullOrUndefined(rhs)) {
        /* What's the other mask? */
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isType(JSVAL_TYPE_NULL) || test->isType(JSVAL_TYPE_UNDEFINED)) {
            return emitStubCmpOp(stub, target, fused);
        } else if (test->isTypeKnown()) {
            /* The test will not succeed, constant fold the compare. */
            bool result = GetCompareCondition(op, fused) == Assembler::NotEqual;
            frame.pop();
            frame.pop();
            if (target)
                return constantFoldBranch(target, result);
            frame.push(BooleanValue(result));
            return true;
        }

        /* The other side must be null or undefined. */
        RegisterID reg = frame.ownRegForType(test);
        frame.pop();
        frame.pop();

        /*
         * :FIXME: Easier test for undefined || null?
         * Maybe put them next to each other, subtract, do a single compare?
         */

        if (target) {
            frame.syncAndKillEverything();
            frame.freeReg(reg);

            Jump sj = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                               Registers::ReturnReg, Registers::ReturnReg);

            if ((op == JSOP_EQ && fused == JSOP_IFNE) ||
                (op == JSOP_NE && fused == JSOP_IFEQ)) {
                /*
                 * It would be easier to just have two jumpAndTrace calls here, but since
                 * each jumpAndTrace creates a TRACE IC, and since we want the bytecode
                 * to have a reference to the TRACE IC at the top of the loop, it's much
                 * better to have only one TRACE IC per loop, and hence at most one
                 * jumpAndTrace.
                 */
                Jump b1 = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
                Jump b2 = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_NULL));
                Jump j1 = masm.jump();
                b1.linkTo(masm.label(), &masm);
                b2.linkTo(masm.label(), &masm);
                Jump j2 = masm.jump();
                if (!jumpAndTrace(j2, target, &sj))
                    return false;
                j1.linkTo(masm.label(), &masm);
            } else {
                Jump j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
                Jump j2 = masm.branchPtr(Assembler::NotEqual, reg, ImmType(JSVAL_TYPE_NULL));
                if (!jumpAndTrace(j2, target, &sj))
                    return false;
                j.linkTo(masm.label(), &masm);
            }
        } else {
            Jump j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
            Jump j2 = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_NULL));
            masm.move(Imm32(op == JSOP_NE), reg);
            Jump j3 = masm.jump();
            j2.linkTo(masm.label(), &masm);
            j.linkTo(masm.label(), &masm);
            masm.move(Imm32(op == JSOP_EQ), reg);
            j3.linkTo(masm.label(), &masm);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
        }
        return true;
    }

    if (cx->typeInferenceEnabled() &&
        lhs->isType(JSVAL_TYPE_OBJECT) && rhs->isType(JSVAL_TYPE_OBJECT)) {
        /*
         * Handle equality between two objects. We have to ensure there is no
         * special equality operator on either object, if that passes then
         * this is a pointer comparison.
         */
        types::TypeSet *lhsTypes = analysis->poppedTypes(PC, 1);
        types::TypeSet *rhsTypes = analysis->poppedTypes(PC, 0);
        if (!lhsTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPECIAL_EQUALITY) &&
            !rhsTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPECIAL_EQUALITY)) {
            /* :TODO: Merge with jsop_relational_int? */
            JS_ASSERT_IF(!target, fused != JSOP_IFEQ);
            frame.forgetMismatchedObject(lhs);
            frame.forgetMismatchedObject(rhs);
            Assembler::Condition cond = GetCompareCondition(op, fused);
            if (target) {
                Jump sj = stubcc.masm.branchTest32(GetStubCompareCondition(fused),
                                                   Registers::ReturnReg, Registers::ReturnReg);
                if (!frame.syncForBranch(target, Uses(2)))
                    return false;
                RegisterID lreg = frame.tempRegForData(lhs);
                frame.pinReg(lreg);
                RegisterID rreg = frame.tempRegForData(rhs);
                frame.unpinReg(lreg);
                Jump fast = masm.branchPtr(cond, lreg, rreg);
                frame.popn(2);
                return jumpAndTrace(fast, target, &sj);
            } else {
                RegisterID result = frame.allocReg();
                RegisterID lreg = frame.tempRegForData(lhs);
                frame.pinReg(lreg);
                RegisterID rreg = frame.tempRegForData(rhs);
                frame.unpinReg(lreg);
                masm.branchValue(cond, lreg, rreg, result);

                frame.popn(2);
                frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
                return true;
            }
        }
    }

    return emitStubCmpOp(stub, target, fused);
}

bool
mjit::Compiler::jsop_relational(JSOp op, BoolStub stub,
                                jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    /* Always slow path... */
    if ((lhs->isNotType(JSVAL_TYPE_INT32) && lhs->isNotType(JSVAL_TYPE_DOUBLE) &&
         lhs->isNotType(JSVAL_TYPE_STRING)) ||
        (rhs->isNotType(JSVAL_TYPE_INT32) && rhs->isNotType(JSVAL_TYPE_DOUBLE) &&
         rhs->isNotType(JSVAL_TYPE_STRING))) {
        if (op == JSOP_EQ || op == JSOP_NE)
            return jsop_equality(op, stub, target, fused);
        return emitStubCmpOp(stub, target, fused);
    }

    if (op == JSOP_EQ || op == JSOP_NE) {
        if ((lhs->isNotType(JSVAL_TYPE_INT32) && lhs->isNotType(JSVAL_TYPE_STRING)) ||
            (rhs->isNotType(JSVAL_TYPE_INT32) && rhs->isNotType(JSVAL_TYPE_STRING))) {
            return emitStubCmpOp(stub, target, fused);
        } else if (!target && (lhs->isType(JSVAL_TYPE_STRING) || rhs->isType(JSVAL_TYPE_STRING))) {
            return emitStubCmpOp(stub, target, fused);
        } else if (frame.haveSameBacking(lhs, rhs)) {
            return emitStubCmpOp(stub, target, fused);
        } else {
            return jsop_equality_int_string(op, stub, target, fused);
        }
    }

    if (frame.haveSameBacking(lhs, rhs)) {
        return emitStubCmpOp(stub, target, fused);
    } else if (lhs->isType(JSVAL_TYPE_STRING) || rhs->isType(JSVAL_TYPE_STRING)) {
        return emitStubCmpOp(stub, target, fused);
    } else if (lhs->isType(JSVAL_TYPE_DOUBLE) || rhs->isType(JSVAL_TYPE_DOUBLE)) {
        return jsop_relational_double(op, stub, target, fused);
    } else if (cx->typeInferenceEnabled() &&
               lhs->isType(JSVAL_TYPE_INT32) && rhs->isType(JSVAL_TYPE_INT32)) {
        return jsop_relational_int(op, target, fused);
    } else {
        return jsop_relational_full(op, stub, target, fused);
    }
}

void
mjit::Compiler::jsop_not()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isConstant()) {
        const Value &v = top->getValue();
        frame.pop();
        frame.push(BooleanValue(!js_ValueToBoolean(v)));
        return;
    }

    if (top->isTypeKnown()) {
        JSValueType type = top->getKnownType();
        switch (type) {
          case JSVAL_TYPE_INT32:
          {
            RegisterID data = frame.allocReg(Registers::SingleByteRegs).reg();
            if (frame.shouldAvoidDataRemat(top))
                masm.loadPayload(frame.addressOf(top), data);
            else
                masm.move(frame.tempRegForData(top), data);

            masm.set32(Assembler::Equal, data, Imm32(0), data);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, data);
            break;
          }

          case JSVAL_TYPE_BOOLEAN:
          {
            RegisterID reg = frame.ownRegForData(top);

            masm.xor32(Imm32(1), reg);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
            break;
          }

          case JSVAL_TYPE_OBJECT:
          {
            RegisterID reg = frame.allocReg();
            masm.move(Imm32(0), reg);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
            break;
          }

          default:
          {
            prepareStubCall(Uses(1));
            INLINE_STUBCALL(stubs::ValueToBoolean, REJOIN_NONE);

            RegisterID reg = Registers::ReturnReg;
            frame.takeReg(reg);
            masm.xor32(Imm32(1), reg);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
            break;
          }
        }

        return;
    }

    RegisterID data = frame.allocReg(Registers::SingleByteRegs).reg();
    if (frame.shouldAvoidDataRemat(top))
        masm.loadPayload(frame.addressOf(top), data);
    else
        masm.move(frame.tempRegForData(top), data);
    RegisterID type = frame.tempRegForType(top);
    Label syncTarget = stubcc.syncExitAndJump(Uses(1));


    /* Inline path is for booleans. */
    Jump jmpNotBool = masm.testBoolean(Assembler::NotEqual, type);
    masm.xor32(Imm32(1), data);


    /* OOL path is for int + object. */
    Label lblMaybeInt32 = stubcc.masm.label();

    Jump jmpNotInt32 = stubcc.masm.testInt32(Assembler::NotEqual, type);
    stubcc.masm.set32(Assembler::Equal, data, Imm32(0), data);
    Jump jmpInt32Exit = stubcc.masm.jump();

    Label lblMaybeObject = stubcc.masm.label();
    Jump jmpNotObject = stubcc.masm.testPrimitive(Assembler::Equal, type);
    stubcc.masm.move(Imm32(0), data);
    Jump jmpObjectExit = stubcc.masm.jump();


    /* Rejoin location. */
    Label lblRejoin = masm.label();

    /* Patch up jumps. */
    stubcc.linkExitDirect(jmpNotBool, lblMaybeInt32);

    jmpNotInt32.linkTo(lblMaybeObject, &stubcc.masm);
    stubcc.crossJump(jmpInt32Exit, lblRejoin);

    jmpNotObject.linkTo(syncTarget, &stubcc.masm);
    stubcc.crossJump(jmpObjectExit, lblRejoin);
    

    /* Leave. */
    stubcc.leave();
    OOL_STUBCALL(stubs::Not, REJOIN_FALLTHROUGH);

    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, data);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_typeof()
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isTypeKnown()) {
        JSRuntime *rt = cx->runtime;

        JSAtom *atom = NULL;
        switch (fe->getKnownType()) {
          case JSVAL_TYPE_STRING:
            atom = rt->atomState.typeAtoms[JSTYPE_STRING];
            break;
          case JSVAL_TYPE_UNDEFINED:
            atom = rt->atomState.typeAtoms[JSTYPE_VOID];
            break;
          case JSVAL_TYPE_NULL:
            atom = rt->atomState.typeAtoms[JSTYPE_OBJECT];
            break;
          case JSVAL_TYPE_OBJECT:
            atom = NULL;
            break;
          case JSVAL_TYPE_BOOLEAN:
            atom = rt->atomState.typeAtoms[JSTYPE_BOOLEAN];
            break;
          default:
            atom = rt->atomState.typeAtoms[JSTYPE_NUMBER];
            break;
        }

        if (atom) {
            frame.pop();
            frame.push(StringValue(atom));
            return;
        }
    }

    JSOp fused = JSOp(PC[JSOP_TYPEOF_LENGTH]);
    if (fused == JSOP_STRING && !fe->isTypeKnown()) {
        JSOp op = JSOp(PC[JSOP_TYPEOF_LENGTH + JSOP_STRING_LENGTH]);

        if (op == JSOP_STRICTEQ || op == JSOP_EQ || op == JSOP_STRICTNE || op == JSOP_NE) {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC + JSOP_TYPEOF_LENGTH));
            JSRuntime *rt = cx->runtime;
            JSValueType type = JSVAL_TYPE_BOXED;
            Assembler::Condition cond = (op == JSOP_STRICTEQ || op == JSOP_EQ)
                                        ? Assembler::Equal
                                        : Assembler::NotEqual;
            
            if (atom == rt->atomState.typeAtoms[JSTYPE_VOID]) {
                type = JSVAL_TYPE_UNDEFINED;
            } else if (atom == rt->atomState.typeAtoms[JSTYPE_STRING]) {
                type = JSVAL_TYPE_STRING;
            } else if (atom == rt->atomState.typeAtoms[JSTYPE_BOOLEAN]) {
                type = JSVAL_TYPE_BOOLEAN;
            } else if (atom == rt->atomState.typeAtoms[JSTYPE_NUMBER]) {
                type = JSVAL_TYPE_INT32;

                /* JSVAL_TYPE_DOUBLE is 0x0 and JSVAL_TYPE_INT32 is 0x1, use <= or > to match both */
                cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
            }

            if (type != JSVAL_TYPE_BOXED) {
                PC += JSOP_STRING_LENGTH;;
                PC += JSOP_EQ_LENGTH;

                RegisterID result = frame.allocReg(Registers::SingleByteRegs).reg();

#if defined JS_NUNBOX32
                if (frame.shouldAvoidTypeRemat(fe))
                    masm.set32(cond, masm.tagOf(frame.addressOf(fe)), ImmType(type), result);
                else
                    masm.set32(cond, frame.tempRegForType(fe), ImmType(type), result);
#elif defined JS_PUNBOX64
                masm.setPtr(cond, frame.tempRegForType(fe), ImmType(type), result);
#endif

                frame.pop();
                frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
                return;
            }
        }
    }

    prepareStubCall(Uses(1));
    INLINE_STUBCALL(stubs::TypeOf, REJOIN_NONE);
    frame.pop();
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_STRING, Registers::ReturnReg);
}

bool
mjit::Compiler::booleanJumpScript(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    MaybeRegisterID type;
    MaybeRegisterID data;

    if (!fe->isTypeKnown() && !frame.shouldAvoidTypeRemat(fe))
        type.setReg(frame.copyTypeIntoReg(fe));
    if (!fe->isType(JSVAL_TYPE_DOUBLE))
        data.setReg(frame.copyDataIntoReg(fe));

    frame.syncAndForgetEverything();

    Assembler::Condition cond = (op == JSOP_IFNE || op == JSOP_OR)
                                ? Assembler::NonZero
                                : Assembler::Zero;
    Assembler::Condition ncond = (op == JSOP_IFNE || op == JSOP_OR)
                                 ? Assembler::Zero
                                 : Assembler::NonZero;

    /* Inline path: Boolean guard + call script. */
    MaybeJump jmpNotBool;
    MaybeJump jmpNotExecScript;
    if (type.isSet()) {
        jmpNotBool.setJump(masm.testBoolean(Assembler::NotEqual, type.reg()));
    } else {
        if (!fe->isTypeKnown()) {
            jmpNotBool.setJump(masm.testBoolean(Assembler::NotEqual,
                                                frame.addressOf(fe)));
        } else if (fe->isNotType(JSVAL_TYPE_BOOLEAN) &&
                   fe->isNotType(JSVAL_TYPE_INT32)) {
            jmpNotBool.setJump(masm.jump());
        }
    }

    /* 
     * TODO: We don't need the second jump if
     * jumpInScript() can go from ool path to inline path.
     */
    if (!fe->isType(JSVAL_TYPE_DOUBLE))
        jmpNotExecScript.setJump(masm.branchTest32(ncond, data.reg(), data.reg()));
    Label lblExecScript = masm.label();
    Jump j = masm.jump();


    /* OOL path: Conversion to boolean. */
    MaybeJump jmpCvtExecScript;
    MaybeJump jmpCvtRejoin;
    Label lblCvtPath = stubcc.masm.label();

    if (!fe->isTypeKnown() ||
        !(fe->isType(JSVAL_TYPE_BOOLEAN) || fe->isType(JSVAL_TYPE_INT32))) {
        /* Note: this cannot overwrite slots holding loop invariants. */
        stubcc.masm.infallibleVMCall(JS_FUNC_TO_DATA_PTR(void *, stubs::ValueToBoolean),
                                     frame.totalDepth());

        jmpCvtExecScript.setJump(stubcc.masm.branchTest32(cond, Registers::ReturnReg,
                                                          Registers::ReturnReg));
        jmpCvtRejoin.setJump(stubcc.masm.jump());
    }

    /* Rejoin tag. */
    Label lblAfterScript = masm.label();

    /* Patch up jumps. */
    if (jmpNotBool.isSet())
        stubcc.linkExitDirect(jmpNotBool.getJump(), lblCvtPath);
    if (jmpNotExecScript.isSet())
        jmpNotExecScript.getJump().linkTo(lblAfterScript, &masm);

    if (jmpCvtExecScript.isSet())
        stubcc.crossJump(jmpCvtExecScript.getJump(), lblExecScript);
    if (jmpCvtRejoin.isSet())
        stubcc.crossJump(jmpCvtRejoin.getJump(), lblAfterScript);

    frame.pop();

    return jumpAndTrace(j, target);
}

bool
mjit::Compiler::jsop_ifneq(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant()) {
        JSBool b = js_ValueToBoolean(fe->getValue());

        frame.pop();

        if (op == JSOP_IFEQ)
            b = !b;
        if (b) {
            if (!frame.syncForBranch(target, Uses(0)))
                return false;
            if (!jumpAndTrace(masm.jump(), target))
                return false;
        } else {
            if (target < PC && !finishLoop(target))
                return false;
        }
        return true;
    }

    return booleanJumpScript(op, target);
}

bool
mjit::Compiler::jsop_andor(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant()) {
        JSBool b = js_ValueToBoolean(fe->getValue());
        
        /* Short-circuit. */
        if ((op == JSOP_OR && b == JS_TRUE) ||
            (op == JSOP_AND && b == JS_FALSE)) {
            if (!frame.syncForBranch(target, Uses(0)))
                return false;
            if (!jumpAndTrace(masm.jump(), target))
                return false;
        }

        frame.pop();
        return true;
    }

    return booleanJumpScript(op, target);
}

bool
mjit::Compiler::jsop_localinc(JSOp op, uint32 slot)
{
    restoreVarType();

    types::TypeSet *types = pushedTypeSet(0);
    JSValueType type = types ? types->getKnownTypeTag(cx) : JSVAL_TYPE_UNKNOWN;

    int amt = (op == JSOP_LOCALINC || op == JSOP_INCLOCAL) ? 1 : -1;

    if (!analysis->incrementInitialValueObserved(PC)) {
        // Before: 
        // After:  V
        frame.pushLocal(slot);

        // Before: V
        // After:  V 1
        frame.push(Int32Value(-amt));

        // Note, SUB will perform integer conversion for us.
        // Before: V 1
        // After:  N+1
        if (!jsop_binary(JSOP_SUB, stubs::Sub, type, types))
            return false;

        // Before: N+1
        // After:  N+1
        frame.storeLocal(slot, analysis->popGuaranteed(PC));
    } else {
        // Before:
        // After: V
        frame.pushLocal(slot);

        // Before: V
        // After:  N
        jsop_pos();

        // Before: N
        // After:  N N
        frame.dup();

        // Before: N N
        // After:  N N 1
        frame.push(Int32Value(amt));

        // Before: N N 1
        // After:  N N+1
        if (!jsop_binary(JSOP_ADD, stubs::Add, type, types))
            return false;

        // Before: N N+1
        // After:  N N+1
        frame.storeLocal(slot, true);

        // Before: N N+1
        // After:  N
        frame.pop();
    }

    updateVarType();
    return true;
}

bool
mjit::Compiler::jsop_arginc(JSOp op, uint32 slot)
{
    restoreVarType();

    types::TypeSet *types = pushedTypeSet(0);
    JSValueType type = types ? types->getKnownTypeTag(cx) : JSVAL_TYPE_UNKNOWN;

    int amt = (op == JSOP_ARGINC || op == JSOP_INCARG) ? 1 : -1;

    if (!analysis->incrementInitialValueObserved(PC)) {
        // Before: 
        // After:  V
        frame.pushArg(slot);

        // Before: V
        // After:  V 1
        frame.push(Int32Value(-amt));

        // Note, SUB will perform integer conversion for us.
        // Before: V 1
        // After:  N+1
        if (!jsop_binary(JSOP_SUB, stubs::Sub, type, types))
            return false;

        // Before: N+1
        // After:  N+1
        frame.storeArg(slot, analysis->popGuaranteed(PC));
    } else {
        // Before:
        // After: V
        frame.pushArg(slot);

        // Before: V
        // After:  N
        jsop_pos();

        // Before: N
        // After:  N N
        frame.dup();

        // Before: N N
        // After:  N N 1
        frame.push(Int32Value(amt));

        // Before: N N 1
        // After:  N N+1
        if (!jsop_binary(JSOP_ADD, stubs::Add, type, types))
            return false;

        // Before: N N+1
        // After:  N N+1
        frame.storeArg(slot, true);

        // Before: N N+1
        // After:  N
        frame.pop();
    }

    updateVarType();
    return true;
}

static inline bool
IsCacheableSetElem(FrameEntry *obj, FrameEntry *id, FrameEntry *value)
{
    if (obj->isNotType(JSVAL_TYPE_OBJECT))
        return false;
    if (id->isNotType(JSVAL_TYPE_INT32))
        return false;
    if (id->isConstant()) {
        if (id->getValue().toInt32() < 0)
            return false;
        if (id->getValue().toInt32() + 1 < 0)  // watch for overflow in hole paths
            return false;
    }

    // obj[obj] * is not allowed, since it will never optimize.
    // obj[id] = id is allowed.
    // obj[id] = obj is allowed.
    if (obj->hasSameBacking(id))
        return false;

    return true;
}

void
mjit::Compiler::jsop_setelem_dense()
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *value = frame.peek(-1);

    // We might not know whether this is an object, but if it is an object we
    // know it is a dense array.
    if (!obj->isTypeKnown()) {
        Jump guard = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(guard, Uses(3));
    }

    // Test for integer index.
    if (!id->isTypeKnown()) {
        Jump guard = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(guard, Uses(3));
    }

    // Allocate registers.

    ValueRemat vr;
    frame.pinEntry(value, vr, /* breakDouble = */ false);

    Int32Key key = id->isConstant()
                 ? Int32Key::FromConstant(id->getValue().toInt32())
                 : Int32Key::FromRegister(frame.tempRegForData(id));
    bool pinKey = !key.isConstant() && !frame.haveSameBacking(id, value);
    if (pinKey)
        frame.pinReg(key.reg());

    // Register to hold the computed slots pointer for the object. If we can
    // hoist the initialized length check, we make the slots pointer loop
    // invariant and never access the object itself.
    RegisterID slotsReg;
    analyze::CrossSSAValue objv(a->inlineIndex, analysis->poppedValue(PC, 2));
    analyze::CrossSSAValue indexv(a->inlineIndex, analysis->poppedValue(PC, 1));
    bool hoisted = loop && id->isType(JSVAL_TYPE_INT32) &&
        loop->hoistArrayLengthCheck(DENSE_ARRAY, objv, indexv);

    if (hoisted) {
        FrameEntry *slotsFe = loop->invariantArraySlots(objv);
        slotsReg = frame.tempRegForData(slotsFe);

        frame.unpinEntry(vr);
        if (pinKey)
            frame.unpinReg(key.reg());
    } else {
        // Get a register for the object which we can clobber.
        RegisterID objReg;
        if (frame.haveSameBacking(obj, value)) {
            objReg = frame.allocReg();
            masm.move(vr.dataReg(), objReg);
        } else if (frame.haveSameBacking(obj, id)) {
            objReg = frame.allocReg();
            masm.move(key.reg(), objReg);
        } else {
            objReg = frame.copyDataIntoReg(obj);
        }

        frame.unpinEntry(vr);
        if (pinKey)
            frame.unpinReg(key.reg());

        // Make an OOL path for setting exactly the initialized length.
        Label syncTarget = stubcc.syncExitAndJump(Uses(3));

        Jump initlenGuard = masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                                  objReg, key, Assembler::BelowOrEqual);
        stubcc.linkExitDirect(initlenGuard, stubcc.masm.label());

        // Recheck for an exact initialized length. :TODO: would be nice to
        // reuse the condition bits from the previous test.
        Jump exactlenGuard = stubcc.masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                                          objReg, key, Assembler::NotEqual);
        exactlenGuard.linkTo(syncTarget, &stubcc.masm);

        // Check array capacity.
        Jump capacityGuard = stubcc.masm.guardArrayExtent(offsetof(JSObject, capacity),
                                                          objReg, key, Assembler::BelowOrEqual);
        capacityGuard.linkTo(syncTarget, &stubcc.masm);

        // Bump the index for setting the array length.  The above guard
        // ensures this won't overflow, due to NSLOTS_LIMIT.
        stubcc.masm.bumpKey(key, 1);

        // Update the initialized length.
        stubcc.masm.storeKey(key, Address(objReg, offsetof(JSObject, initializedLength)));

        // Update the array length if needed.
        Jump lengthGuard = stubcc.masm.guardArrayExtent(offsetof(JSObject, privateData),
                                                        objReg, key, Assembler::AboveOrEqual);
        stubcc.masm.storeKey(key, Address(objReg, offsetof(JSObject, privateData)));
        lengthGuard.linkTo(stubcc.masm.label(), &stubcc.masm);

        // Restore the index.
        stubcc.masm.bumpKey(key, -1);

        // Rejoin with the inline path.
        Jump initlenExit = stubcc.masm.jump();
        stubcc.crossJump(initlenExit, masm.label());

        masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
        slotsReg = objReg;
    }

    // Fully store the value. :TODO: don't need to do this in the non-initlen case
    // if the array is packed and monomorphic.
    if (key.isConstant())
        masm.storeValue(vr, Address(slotsReg, key.index() * sizeof(Value)));
    else
        masm.storeValue(vr, BaseIndex(slotsReg, key.reg(), masm.JSVAL_SCALE));

    stubcc.leave();
    OOL_STUBCALL(STRICT_VARIANT(stubs::SetElem), REJOIN_FALLTHROUGH);

    if (!hoisted)
        frame.freeReg(slotsReg);
    frame.shimmy(2);
    stubcc.rejoin(Changes(2));
}

#ifdef JS_METHODJIT_TYPED_ARRAY
void
mjit::Compiler::convertForTypedArray(int atype, ValueRemat *vr, bool *allocated)
{
    FrameEntry *value = frame.peek(-1);
    bool floatArray = (atype == TypedArray::TYPE_FLOAT32 ||
                       atype == TypedArray::TYPE_FLOAT64);
    *allocated = false;

    if (value->isConstant()) {
        Value v = value->getValue();
        if (floatArray) {
            double d = v.isDouble() ? v.toDouble() : v.toInt32();
            *vr = ValueRemat::FromConstant(DoubleValue(d));
        } else {
            int i32;
            if (v.isInt32()) {
                i32 = v.toInt32();
                if (atype == TypedArray::TYPE_UINT8_CLAMPED)
                    i32 = ClampIntForUint8Array(i32);
            } else {
                i32 = (atype == TypedArray::TYPE_UINT8_CLAMPED)
                    ? js_TypedArray_uint8_clamp_double(v.toDouble())
                    : js_DoubleToECMAInt32(v.toDouble());
            }
            *vr = ValueRemat::FromConstant(Int32Value(i32));
        }
    } else {
        if (floatArray) {
            FPRegisterID fpReg;
            MaybeJump notNumber = loadDouble(value, &fpReg, allocated);
            if (notNumber.isSet())
                stubcc.linkExit(notNumber.get(), Uses(3));

            if (atype == TypedArray::TYPE_FLOAT32) {
                if (!*allocated) {
                    frame.pinReg(fpReg);
                    FPRegisterID newFpReg = frame.allocFPReg();
                    masm.convertDoubleToFloat(fpReg, newFpReg);
                    frame.unpinReg(fpReg);
                    fpReg = newFpReg;
                    *allocated = true;
                } else {
                    masm.convertDoubleToFloat(fpReg, fpReg);
                }
            }
            *vr = ValueRemat::FromFPRegister(fpReg);
        } else {
            /*
             * Allocate a register with the following properties:
             * 1) For byte arrays the value must be in a byte register.
             * 2) For Uint8ClampedArray the register must be writable.
             * 3) If the value is definitely int32 (and the array is not
             *    Uint8ClampedArray) we don't have to allocate a new register.
             * 4) If id and value have the same backing (e.g. arr[i] = i) and
             *    we need a byte register, we have to allocate a new register
             *    because we've already pinned a key register and can't use
             *    tempRegInMaskForData.
             */
            MaybeRegisterID reg;
            bool needsByteReg = (atype == TypedArray::TYPE_INT8 ||
                                 atype == TypedArray::TYPE_UINT8 ||
                                 atype == TypedArray::TYPE_UINT8_CLAMPED);
            FrameEntry *id = frame.peek(-2);
            if (!value->isType(JSVAL_TYPE_INT32) || atype == TypedArray::TYPE_UINT8_CLAMPED ||
                (needsByteReg && frame.haveSameBacking(id, value))) {
                // Pin value so that we don't evict it.
                MaybeRegisterID dataReg;
                if (value->mightBeType(JSVAL_TYPE_INT32) && !frame.haveSameBacking(id, value)) {
                    dataReg = frame.tempRegForData(value);
                    frame.pinReg(dataReg.reg());
                }

                // x86 has 4 single byte registers. Worst case we've pinned 3
                // registers, one for each of object, key and value. This means
                // there must be at least one single byte register available.
                if (needsByteReg)
                    reg = frame.allocReg(Registers::SingleByteRegs).reg();
                else
                    reg = frame.allocReg();
                *allocated = true;
                if (dataReg.isSet())
                    frame.unpinReg(dataReg.reg());
            } else {
                if (needsByteReg)
                    reg = frame.tempRegInMaskForData(value, Registers::SingleByteRegs).reg();
                else
                    reg = frame.tempRegForData(value);
            }

            MaybeJump intDone;
            if (value->mightBeType(JSVAL_TYPE_INT32)) {
                // Check if the value is an integer.
                MaybeJump notInt;
                if (!value->isTypeKnown()) {
                    JS_ASSERT(*allocated);
                    notInt = frame.testInt32(Assembler::NotEqual, value);
                }

                if (*allocated)
                    masm.move(frame.tempRegForData(value), reg.reg());

                if (atype == TypedArray::TYPE_UINT8_CLAMPED)
                    masm.clampInt32ToUint8(reg.reg());

                if (notInt.isSet()) {
                    intDone = masm.jump();
                    notInt.get().linkTo(masm.label(), &masm);
                }
            }
            if (value->mightBeType(JSVAL_TYPE_DOUBLE)) {
                // Check if the value is a double.
                if (!value->isTypeKnown()) {
                    Jump notNumber = frame.testDouble(Assembler::NotEqual, value);
                    stubcc.linkExit(notNumber, Uses(3));
                }

                // Load value in fpReg.
                FPRegisterID fpReg;
                if (value->isTypeKnown()) {
                    fpReg = frame.tempFPRegForData(value);
                } else {
                    fpReg = frame.allocFPReg();
                    frame.loadDouble(value, fpReg, masm);
                }

                // Convert double to integer.
                if (atype == TypedArray::TYPE_UINT8_CLAMPED) {
                    if (value->isTypeKnown())
                        frame.pinReg(fpReg);
                    FPRegisterID fpTemp = frame.allocFPReg();
                    if (value->isTypeKnown())
                        frame.unpinReg(fpReg);
                    masm.clampDoubleToUint8(fpReg, fpTemp, reg.reg());
                    frame.freeReg(fpTemp);
                } else {
                    Jump j = masm.branchTruncateDoubleToInt32(fpReg, reg.reg());
                    stubcc.linkExit(j, Uses(3));
                }
                if (!value->isTypeKnown())
                    frame.freeReg(fpReg);
            }
            if (intDone.isSet())
                intDone.get().linkTo(masm.label(), &masm);
            *vr = ValueRemat::FromKnownType(JSVAL_TYPE_INT32, reg.reg());
        }
    }
}

void
mjit::Compiler::jsop_setelem_typed(int atype)
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *value = frame.peek(-1);

    // We might not know whether this is an object, but if it is an object we
    // know it's a typed array.
    if (!obj->isTypeKnown()) {
        Jump guard = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(guard, Uses(3));
    }

    // Test for integer index.
    if (!id->isTypeKnown()) {
        Jump guard = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(guard, Uses(3));
    }

    // Pin value.
    ValueRemat vr;
    frame.pinEntry(value, vr, /* breakDouble = */ false);

    // Allocate and pin object and key regs.
    Int32Key key = id->isConstant()
                 ? Int32Key::FromConstant(id->getValue().toInt32())
                 : Int32Key::FromRegister(frame.tempRegForData(id));

    bool pinKey = !key.isConstant() && !frame.haveSameBacking(id, value);
    if (pinKey)
        frame.pinReg(key.reg());

    analyze::CrossSSAValue objv(a->inlineIndex, analysis->poppedValue(PC, 1));
    analyze::CrossSSAValue indexv(a->inlineIndex, analysis->poppedValue(PC, 0));
    bool hoisted = loop && id->isType(JSVAL_TYPE_INT32) &&
        loop->hoistArrayLengthCheck(TYPED_ARRAY, objv, indexv);

    RegisterID objReg;
    if (hoisted) {
        FrameEntry *slotsFe = loop->invariantArraySlots(objv);
        objReg = frame.tempRegForData(slotsFe);
        frame.pinReg(objReg);
    } else {
        objReg = frame.copyDataIntoReg(obj);

        // Bounds check.
        Jump lengthGuard = masm.guardArrayExtent(TypedArray::lengthOffset(),
                                                 objReg, key, Assembler::BelowOrEqual);
        stubcc.linkExit(lengthGuard, Uses(3));

        // Load the array's packed data vector.
        masm.loadPtr(Address(objReg, TypedArray::dataOffset()), objReg);
    }

    // Unpin value so that convertForTypedArray can assign a new data
    // register using tempRegInMaskForData.
    frame.unpinEntry(vr);

    // Make sure key is pinned.
    if (frame.haveSameBacking(id, value)) {
        frame.pinReg(key.reg());
        pinKey = true;
    }
    JS_ASSERT(pinKey == !id->isConstant());

    bool allocated;
    convertForTypedArray(atype, &vr, &allocated);

    // Store the value.
    masm.storeToTypedArray(atype, objReg, key, vr);
    if (allocated) {
        if (vr.isFPRegister())
            frame.freeReg(vr.fpReg());
        else
            frame.freeReg(vr.dataReg());
    }
    if (pinKey)
        frame.unpinReg(key.reg());
    if (hoisted)
        frame.unpinReg(objReg);
    else
        frame.freeReg(objReg);

    stubcc.leave();
    OOL_STUBCALL(STRICT_VARIANT(stubs::SetElem), REJOIN_FALLTHROUGH);

    frame.shimmy(2);
    stubcc.rejoin(Changes(2));
}
#endif /* JS_METHODJIT_TYPED_ARRAY */

bool
mjit::Compiler::jsop_setelem(bool popGuaranteed)
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *value = frame.peek(-1);

    if (!IsCacheableSetElem(obj, id, value) || monitored(PC)) {
        jsop_setelem_slow();
        return true;
    }

    frame.forgetMismatchedObject(obj);

    // If the object is definitely a dense array or a typed array we can generate
    // code directly without using an inline cache.
    if (cx->typeInferenceEnabled() && id->mightBeType(JSVAL_TYPE_INT32)) {
        types::TypeSet *types = analysis->poppedTypes(PC, 2);

        if (!types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY) &&
            !arrayPrototypeHasIndexedProperty()) {
            // Inline dense array path.
            jsop_setelem_dense();
            return true;
        }

#ifdef JS_METHODJIT_TYPED_ARRAY
        if ((value->mightBeType(JSVAL_TYPE_INT32) || value->mightBeType(JSVAL_TYPE_DOUBLE)) &&
            !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY)) {
            // Inline typed array path.
            int atype = types->getTypedArrayType(cx);
            if (atype != TypedArray::TYPE_MAX) {
                jsop_setelem_typed(atype);
                return true;
            }
        }
#endif
    }

    SetElementICInfo ic = SetElementICInfo(JSOp(*PC));

    // One by one, check if the most important stack entries have registers,
    // and if so, pin them. This is to avoid spilling and reloading from the
    // stack as we incrementally allocate other registers.
    MaybeRegisterID pinnedValueType = frame.maybePinType(value);
    MaybeRegisterID pinnedValueData = frame.maybePinData(value);

    // Pin |obj| if it doesn't share a backing with |value|.
    MaybeRegisterID pinnedObjData;
    if (!obj->hasSameBacking(value))
        pinnedObjData = frame.maybePinData(obj);

    // Pin |id| if it doesn't share a backing with |value|.
    MaybeRegisterID pinnedIdData;
    if (!id->hasSameBacking(value))
        pinnedIdData = frame.maybePinData(id);

    // Note: The fact that |obj| and |value|, or |id| and |value| can be
    // copies, is a little complicated, but it is safe. Explanations
    // follow at each point. Keep in mind two points:
    //  1) maybePin() never allocates a register, it only pins if a register
    //     already existed.
    //  2) tempRegForData() will work fine on a pinned register.
 
    // Guard that the object is an object.
    if (!obj->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(j, Uses(3));
    }

    // Guard that the id is int32.
    if (!id->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(j, Uses(3));
    }

    // Grab a register for the object. It's safe to unpin |obj| because it
    // won't have been pinned if it shares a backing with |value|. However,
    // it would not be safe to copyDataIntoReg() if the value was pinned,
    // since this could evict the register. So we special case.
    frame.maybeUnpinReg(pinnedObjData);
    if (obj->hasSameBacking(value) && pinnedValueData.isSet()) {
        ic.objReg = frame.allocReg();
        masm.move(pinnedValueData.reg(), ic.objReg);
    } else {
        ic.objReg = frame.copyDataIntoReg(obj);
    }

    // pinEntry() will ensure pinned registers for |value|. To avoid a
    // double-pin assert, first unpin any registers that |value| had.
    frame.maybeUnpinReg(pinnedValueType);
    frame.maybeUnpinReg(pinnedValueData);
    frame.pinEntry(value, ic.vr);

    // Store rematerialization information about the key. This is the final
    // register we allocate, and thus it can use tempRegForData() without
    // the worry of being spilled. Once again, this is safe even if |id|
    // shares a backing with |value|, because tempRegForData() will work on
    // the pinned register, and |pinnedIdData| will not double-pin.
    frame.maybeUnpinReg(pinnedIdData);
    if (id->isConstant())
        ic.key = Int32Key::FromConstant(id->getValue().toInt32());
    else
        ic.key = Int32Key::FromRegister(frame.tempRegForData(id));

    // Unpin the value since register allocation is complete.
    frame.unpinEntry(ic.vr);

    // Now it's also safe to grab remat info for obj (all exits that can
    // generate stubs must have the same register state).
    ic.objRemat = frame.dataRematInfo(obj);

    // All patchable guards must occur after this point.
    RESERVE_IC_SPACE(masm);
    ic.fastPathStart = masm.label();

    // Create the common out-of-line sync block, taking care to link previous
    // guards here after.
    RESERVE_OOL_SPACE(stubcc.masm);
    ic.slowPathStart = stubcc.syncExit(Uses(3));

    // Guard obj is a dense array.
    ic.claspGuard = masm.testObjClass(Assembler::NotEqual, ic.objReg, &js_ArrayClass);
    stubcc.linkExitDirect(ic.claspGuard, ic.slowPathStart);

    // Guard in range of initialized length.
    Jump initlenGuard = masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                              ic.objReg, ic.key, Assembler::BelowOrEqual);
    stubcc.linkExitDirect(initlenGuard, ic.slowPathStart);

    // Load the dynamic slots vector.
    masm.loadPtr(Address(ic.objReg, offsetof(JSObject, slots)), ic.objReg);

    // Guard there's no hole, then store directly to the slot.
    if (ic.key.isConstant()) {
        Address slot(ic.objReg, ic.key.index() * sizeof(Value));
        ic.holeGuard = masm.guardNotHole(slot);
        masm.storeValue(ic.vr, slot);
    } else {
        BaseIndex slot(ic.objReg, ic.key.reg(), Assembler::JSVAL_SCALE);
        ic.holeGuard = masm.guardNotHole(slot);
        masm.storeValue(ic.vr, slot);
    }
    stubcc.linkExitDirect(ic.holeGuard, ic.slowPathStart);

    stubcc.leave();
#if defined JS_POLYIC
    passICAddress(&ic);
    ic.slowPathCall = OOL_STUBCALL(STRICT_VARIANT(ic::SetElement), REJOIN_FALLTHROUGH);
#else
    OOL_STUBCALL(STRICT_VARIANT(stubs::SetElem), REJOIN_FALLTHROUGH);
#endif

    ic.fastPathRejoin = masm.label();

    // When generating typed array stubs, it may be necessary to call
    // js_DoubleToECMAInt32(), which would clobber registers. To deal with
    // this, we tell the IC exactly which registers need to be saved
    // across calls.
    ic.volatileMask = frame.regsInUse();

    // If the RHS will be popped, and doesn't overlap any live values, then
    // there's no need to save it across calls. Note that this is not true of
    // |obj| or |key|, which will be used to compute the LHS reference for
    // assignment.
    //
    // Note that the IC wants to clobber |vr.dataReg| to convert for typed
    // arrays. If this clobbering is necessary, we must preserve dataReg,
    // even if it's not in a volatile register.
    if (popGuaranteed &&
        !ic.vr.isConstant() &&
        !value->isCopy() &&
        !frame.haveSameBacking(value, obj) &&
        !frame.haveSameBacking(value, id))
    {
        ic.volatileMask &= ~Registers::maskReg(ic.vr.dataReg());
        if (!ic.vr.isTypeKnown())
            ic.volatileMask &= ~Registers::maskReg(ic.vr.typeReg());
    } else if (!ic.vr.isConstant()) {
        ic.volatileMask |= Registers::maskReg(ic.vr.dataReg());
    }

    frame.freeReg(ic.objReg);
    frame.shimmy(2);
    stubcc.rejoin(Changes(2));

#if defined JS_POLYIC
    if (!setElemICs.append(ic))
        return false;
#endif

    return true;
}

static inline bool
IsCacheableGetElem(FrameEntry *obj, FrameEntry *id)
{
    if (id->isTypeKnown() &&
        !(id->getKnownType() == JSVAL_TYPE_INT32
#if defined JS_POLYIC
          || id->getKnownType() == JSVAL_TYPE_STRING
#endif
         )) {
        return false;
    }

    if (id->isTypeKnown() && id->getKnownType() == JSVAL_TYPE_INT32 && id->isConstant() &&
        id->getValue().toInt32() < 0) {
        return false;
    }

    // obj[obj] is not allowed, since it will never optimize.
    if (obj->hasSameBacking(id))
        return false;

    return true;
}

void
mjit::Compiler::jsop_getelem_dense(bool isPacked)
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *id = frame.peek(-1);

    // We might not know whether this is an object, but if it is an object we
    // know it is a dense array.
    if (!obj->isTypeKnown()) {
        Jump guard = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(guard, Uses(2));
    }

    // Test for integer index.
    if (!id->isTypeKnown()) {
        Jump guard = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(guard, Uses(2));
    }

    JSValueType type = knownPushedType(0);

    // Allocate registers.

    // If we know the result of the GETELEM may be undefined, then misses on the
    // initialized length or hole checks can just produce an undefined value.
    // We checked in the caller that prototypes do not have indexed properties.
    bool allowUndefined = mayPushUndefined(0);

    analyze::CrossSSAValue objv(a->inlineIndex, analysis->poppedValue(PC, 1));
    analyze::CrossSSAValue indexv(a->inlineIndex, analysis->poppedValue(PC, 0));
    bool hoisted = loop && id->isType(JSVAL_TYPE_INT32) &&
        loop->hoistArrayLengthCheck(DENSE_ARRAY, objv, indexv);

    // Get a register with either the object or its slots, depending on whether
    // we are hoisting the bounds check.
    RegisterID baseReg;
    if (hoisted) {
        FrameEntry *slotsFe = loop->invariantArraySlots(objv);
        baseReg = frame.tempRegForData(slotsFe);
    } else {
        baseReg = frame.tempRegForData(obj);
    }
    frame.pinReg(baseReg);

    Int32Key key = id->isConstant()
                 ? Int32Key::FromConstant(id->getValue().toInt32())
                 : Int32Key::FromRegister(frame.tempRegForData(id));
    bool pinKey = !key.isConstant() && key.reg() != baseReg;
    if (pinKey)
        frame.pinReg(key.reg());

    RegisterID dataReg = frame.allocReg();

    MaybeRegisterID typeReg;
    if (type == JSVAL_TYPE_UNKNOWN || type == JSVAL_TYPE_DOUBLE || hasTypeBarriers(PC))
        typeReg = frame.allocReg();

    // Guard on the array's initialized length.
    MaybeJump initlenGuard;
    if (!hoisted) {
        initlenGuard = masm.guardArrayExtent(offsetof(JSObject, initializedLength),
                                             baseReg, key, Assembler::BelowOrEqual);
    }

    frame.unpinReg(baseReg);
    if (pinKey)
        frame.unpinReg(key.reg());

    RegisterID slotsReg;
    if (hoisted) {
        slotsReg = baseReg;
    } else {
        if (!allowUndefined)
            stubcc.linkExit(initlenGuard.get(), Uses(2));
        masm.loadPtr(Address(baseReg, offsetof(JSObject, slots)), dataReg);
        slotsReg = dataReg;
    }

    // Get the slot, skipping the hole check if the array is known to be packed.
    Jump holeCheck;
    if (key.isConstant()) {
        Address slot(slotsReg, key.index() * sizeof(Value));
        holeCheck = masm.fastArrayLoadSlot(slot, !isPacked, typeReg, dataReg);
    } else {
        JS_ASSERT(key.reg() != dataReg);
        BaseIndex slot(slotsReg, key.reg(), masm.JSVAL_SCALE);
        holeCheck = masm.fastArrayLoadSlot(slot, !isPacked, typeReg, dataReg);
    }

    if (!isPacked && !allowUndefined)
        stubcc.linkExit(holeCheck, Uses(2));

    stubcc.leave();
    OOL_STUBCALL(stubs::GetElem, REJOIN_FALLTHROUGH);

    frame.popn(2);

    BarrierState barrier;
    if (typeReg.isSet()) {
        frame.pushRegs(typeReg.reg(), dataReg, type);
        barrier = testBarrier(typeReg.reg(), dataReg, false);
    } else {
        frame.pushTypedPayload(type, dataReg);
    }

    stubcc.rejoin(Changes(2));

    if (allowUndefined) {
        if (!hoisted)
            stubcc.linkExitDirect(initlenGuard.get(), stubcc.masm.label());
        if (!isPacked)
            stubcc.linkExitDirect(holeCheck, stubcc.masm.label());
        JS_ASSERT(type == JSVAL_TYPE_UNKNOWN || type == JSVAL_TYPE_UNDEFINED);
        if (type == JSVAL_TYPE_UNDEFINED)
            stubcc.masm.loadValuePayload(UndefinedValue(), dataReg);
        else
            stubcc.masm.loadValueAsComponents(UndefinedValue(), typeReg.reg(), dataReg);
        stubcc.linkRejoin(stubcc.masm.jump());
    }

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);
}

void
mjit::Compiler::jsop_getelem_args()
{
    FrameEntry *id = frame.peek(-1);

    // Test for integer index.
    if (!id->isTypeKnown()) {
        Jump guard = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(guard, Uses(2));
    }

    // Allocate registers.

    analyze::CrossSSAValue indexv(a->inlineIndex, analysis->poppedValue(PC, 0));
    bool hoistedLength = loop && id->isType(JSVAL_TYPE_INT32) &&
        loop->hoistArgsLengthCheck(indexv);
    FrameEntry *actualsFe = loop ? loop->invariantArguments() : NULL;

    Int32Key key = id->isConstant()
                 ? Int32Key::FromConstant(id->getValue().toInt32())
                 : Int32Key::FromRegister(frame.tempRegForData(id));
    if (!key.isConstant())
        frame.pinReg(key.reg());

    RegisterID dataReg = frame.allocReg();
    RegisterID typeReg = frame.allocReg();

    if (!key.isConstant())
        frame.unpinReg(key.reg());

    // Guard on nactual.
    if (!hoistedLength) {
        Address nactualAddr(JSFrameReg, StackFrame::offsetOfArgs());
        MaybeJump rangeGuard;
        if (key.isConstant()) {
            JS_ASSERT(key.index() >= 0);
            rangeGuard = masm.branch32(Assembler::BelowOrEqual, nactualAddr, Imm32(key.index()));
        } else {
            rangeGuard = masm.branch32(Assembler::BelowOrEqual, nactualAddr, key.reg());
        }
        stubcc.linkExit(rangeGuard.get(), Uses(2));
    }

    RegisterID actualsReg;
    if (actualsFe) {
        actualsReg = frame.tempRegForData(actualsFe);
    } else {
        actualsReg = dataReg;
        masm.loadFrameActuals(outerScript->function(), actualsReg);
    }

    if (key.isConstant()) {
        Address arg(actualsReg, key.index() * sizeof(Value));
        masm.loadValueAsComponents(arg, typeReg, dataReg);
    } else {
        JS_ASSERT(key.reg() != dataReg);
        BaseIndex arg(actualsReg, key.reg(), masm.JSVAL_SCALE);
        masm.loadValueAsComponents(arg, typeReg, dataReg);
    }

    stubcc.leave();
    OOL_STUBCALL(stubs::GetElem, REJOIN_FALLTHROUGH);

    frame.popn(2);
    frame.pushRegs(typeReg, dataReg, knownPushedType(0));
    BarrierState barrier = testBarrier(typeReg, dataReg, false);

    stubcc.rejoin(Changes(2));

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);
}

#ifdef JS_METHODJIT_TYPED_ARRAY
void
mjit::Compiler::jsop_getelem_typed(int atype)
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *id = frame.peek(-1);

    // We might not know whether this is an object, but if it's an object we
    // know it is a typed array.
    if (!obj->isTypeKnown()) {
        Jump guard = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(guard, Uses(2));
    }

    // Test for integer index.
    if (!id->isTypeKnown()) {
        Jump guard = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(guard, Uses(2));
    }

    // Load object and key.
    Int32Key key = id->isConstant()
                 ? Int32Key::FromConstant(id->getValue().toInt32())
                 : Int32Key::FromRegister(frame.tempRegForData(id));
    if (!key.isConstant())
        frame.pinReg(key.reg());

    analyze::CrossSSAValue objv(a->inlineIndex, analysis->poppedValue(PC, 1));
    analyze::CrossSSAValue indexv(a->inlineIndex, analysis->poppedValue(PC, 0));
    bool hoisted = loop && id->isType(JSVAL_TYPE_INT32) &&
        loop->hoistArrayLengthCheck(TYPED_ARRAY, objv, indexv);

    RegisterID objReg;
    if (hoisted) {
        FrameEntry *slotsFe = loop->invariantArraySlots(objv);
        objReg = frame.tempRegForData(slotsFe);
        frame.pinReg(objReg);
    } else {
        objReg = frame.copyDataIntoReg(obj);

        // Bounds check.
        Jump lengthGuard = masm.guardArrayExtent(TypedArray::lengthOffset(),
                                                 objReg, key, Assembler::BelowOrEqual);
        stubcc.linkExit(lengthGuard, Uses(2));

        // Load the array's packed data vector.
        masm.loadPtr(Address(objReg, TypedArray::dataOffset()), objReg);
    }

    // We can load directly into an FP-register if the following conditions
    // are met:
    // 1) The array is an Uint32Array or a float array (loadFromTypedArray
    //    can't load into an FP-register for other arrays).
    // 2) The result is definitely a double (the result type set can include
    //    other types after reading out-of-bound values).
    AnyRegisterID dataReg;
    MaybeRegisterID typeReg, tempReg;
    JSValueType type = knownPushedType(0);
    bool maybeReadFloat = (atype == TypedArray::TYPE_FLOAT32 ||
                           atype == TypedArray::TYPE_FLOAT64 ||
                           atype == TypedArray::TYPE_UINT32);
    if (maybeReadFloat && type == JSVAL_TYPE_DOUBLE) {
        dataReg = frame.allocFPReg();
        // Need an extra reg to convert uint32 to double.
        if (atype == TypedArray::TYPE_UINT32)
            tempReg = frame.allocReg();
    } else {
        dataReg = frame.allocReg();
        // loadFromTypedArray expects a type register for Uint32Array or
        // float arrays. Also allocate a type register if the result may not
        // be int32 (due to reading out-of-bound values) or if there's a
        // type barrier.
        if (maybeReadFloat || type != JSVAL_TYPE_INT32)
            typeReg = frame.allocReg();
    }

    // Load value from the array.
    masm.loadFromTypedArray(atype, objReg, key, typeReg, dataReg, tempReg);

    if (hoisted)
        frame.unpinReg(objReg);
    else
        frame.freeReg(objReg);
    if (!key.isConstant())
        frame.unpinReg(key.reg());
    if (tempReg.isSet())
        frame.freeReg(tempReg.reg());

    stubcc.leave();
    OOL_STUBCALL(stubs::GetElem, REJOIN_FALLTHROUGH);

    frame.popn(2);

    BarrierState barrier;
    if (dataReg.isFPReg()) {
        frame.pushDouble(dataReg.fpreg());
    } else if (typeReg.isSet()) {
        frame.pushRegs(typeReg.reg(), dataReg.reg(), knownPushedType(0));
        if (hasTypeBarriers(PC))
            barrier = testBarrier(typeReg.reg(), dataReg.reg(), false);
    } else {
        JS_ASSERT(type == JSVAL_TYPE_INT32);
        frame.pushTypedPayload(JSVAL_TYPE_INT32, dataReg.reg());
    }
    stubcc.rejoin(Changes(2));

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);
}
#endif /* JS_METHODJIT_TYPED_ARRAY */

bool
mjit::Compiler::jsop_getelem(bool isCall)
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *id = frame.peek(-1);

    if (!IsCacheableGetElem(obj, id)) {
        if (isCall)
            jsop_callelem_slow();
        else
            jsop_getelem_slow();
        return true;
    }

    // If the object is definitely an arguments object, a dense array or a typed array
    // we can generate code directly without using an inline cache.
    if (cx->typeInferenceEnabled() && id->mightBeType(JSVAL_TYPE_INT32) && !isCall) {
        types::TypeSet *types = analysis->poppedTypes(PC, 1);
        if (types->isLazyArguments(cx) && !outerScript->analysis()->modifiesArguments()) {
            // Inline arguments path.
            jsop_getelem_args();
            return true;
        }

        if (obj->mightBeType(JSVAL_TYPE_OBJECT) &&
            !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY) &&
            !arrayPrototypeHasIndexedProperty()) {
            // Inline dense array path.
            bool packed = !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
            jsop_getelem_dense(packed);
            return true;
        }

#ifdef JS_METHODJIT_TYPED_ARRAY
        if (obj->mightBeType(JSVAL_TYPE_OBJECT) &&
            !types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY) &&
            pushedTypeSet(0)->baseFlags() != 0) {
            // Inline typed array path.
            int atype = types->getTypedArrayType(cx);
            if (atype != TypedArray::TYPE_MAX) {
                jsop_getelem_typed(atype);
                return true;
            }
        }
#endif
    }

    frame.forgetMismatchedObject(obj);

    GetElementICInfo ic = GetElementICInfo(JSOp(*PC));

    // Pin the top of the stack to avoid spills, before allocating registers.
    MaybeRegisterID pinnedIdData = frame.maybePinData(id);
    MaybeRegisterID pinnedIdType = frame.maybePinType(id);

    MaybeJump objTypeGuard;
    if (!obj->isTypeKnown()) {
        // Test the type of the object without spilling the payload.
        MaybeRegisterID pinnedObjData = frame.maybePinData(obj);
        Jump guard = frame.testObject(Assembler::NotEqual, obj);
        frame.maybeUnpinReg(pinnedObjData);

        // Create a sync path, which we'll rejoin manually later. This is safe
        // as long as the IC does not build a stub; it won't, because |obj|
        // won't be an object. If we extend this IC to support strings, all
        // that needs to change is a little code movement.
        stubcc.linkExit(guard, Uses(2));
        objTypeGuard = stubcc.masm.jump();
    }

    // Get a mutable register for the object. This will be the data reg.
    ic.objReg = frame.copyDataIntoReg(obj);

    // For potential dense array calls, grab an extra reg to save the
    // outgoing object.
    MaybeRegisterID thisReg;
    if (isCall && id->mightBeType(JSVAL_TYPE_INT32)) {
        thisReg = frame.allocReg();
        masm.move(ic.objReg, thisReg.reg());
    }

    // Get a mutable register for pushing the result type. We kill two birds
    // with one stone by making sure, if the key type is not known, to be loaded
    // into this register. In this case it is both an input and an output.
    frame.maybeUnpinReg(pinnedIdType);
    if (id->isConstant() || id->isTypeKnown())
        ic.typeReg = frame.allocReg();
    else
        ic.typeReg = frame.copyTypeIntoReg(id);

    // Fill in the id value.
    frame.maybeUnpinReg(pinnedIdData);
    if (id->isConstant()) {
        ic.id = ValueRemat::FromConstant(id->getValue());
    } else {
        RegisterID dataReg = frame.tempRegForData(id);
        if (id->isTypeKnown())
            ic.id = ValueRemat::FromKnownType(id->getKnownType(), dataReg);
        else
            ic.id = ValueRemat::FromRegisters(ic.typeReg, dataReg);
    }

    RESERVE_IC_SPACE(masm);
    ic.fastPathStart = masm.label();

    // Note: slow path here is safe, since the frame will not be modified.
    RESERVE_OOL_SPACE(stubcc.masm);
    ic.slowPathStart = stubcc.masm.label();
    frame.sync(stubcc.masm, Uses(2));

    if (id->mightBeType(JSVAL_TYPE_INT32)) {
        // Always test the type first (see comment in PolyIC.h).
        if (!id->isTypeKnown()) {
            ic.typeGuard = masm.testInt32(Assembler::NotEqual, ic.typeReg);
            stubcc.linkExitDirect(ic.typeGuard.get(), ic.slowPathStart);
        }

        // Guard on the clasp.
        ic.claspGuard = masm.testObjClass(Assembler::NotEqual, ic.objReg, &js_ArrayClass);
        stubcc.linkExitDirect(ic.claspGuard, ic.slowPathStart);

        Int32Key key = id->isConstant()
                       ? Int32Key::FromConstant(id->getValue().toInt32())
                       : Int32Key::FromRegister(ic.id.dataReg());

        Assembler::FastArrayLoadFails fails =
            masm.fastArrayLoad(ic.objReg, key, ic.typeReg, ic.objReg);

        // Store the object back to sp[-1] for calls. This must occur after
        // all guards because otherwise sp[-1] will be clobbered.
        if (isCall) {
            Address thisSlot = frame.addressOf(id);
            masm.storeValueFromComponents(ImmType(JSVAL_TYPE_OBJECT), thisReg.reg(), thisSlot);
            frame.freeReg(thisReg.reg());
        }

        stubcc.linkExitDirect(fails.rangeCheck, ic.slowPathStart);
        stubcc.linkExitDirect(fails.holeCheck, ic.slowPathStart);
    } else {
        // The type is known to not be dense-friendly ahead of time, so always
        // fall back to a slow path.
        ic.claspGuard = masm.jump();
        stubcc.linkExitDirect(ic.claspGuard, ic.slowPathStart);
    }

    stubcc.leave();
    if (objTypeGuard.isSet())
        objTypeGuard.get().linkTo(stubcc.masm.label(), &stubcc.masm);
#ifdef JS_POLYIC
    passICAddress(&ic);
    if (isCall)
        ic.slowPathCall = OOL_STUBCALL(ic::CallElement, REJOIN_FALLTHROUGH);
    else
        ic.slowPathCall = OOL_STUBCALL(ic::GetElement, REJOIN_FALLTHROUGH);
#else
    if (isCall)
        ic.slowPathCall = OOL_STUBCALL(stubs::CallElem, REJOIN_FALLTHROUGH);
    else
        ic.slowPathCall = OOL_STUBCALL(stubs::GetElem, REJOIN_FALLTHROUGH);
#endif

    ic.fastPathRejoin = masm.label();

    frame.popn(2);
    frame.pushRegs(ic.typeReg, ic.objReg, knownPushedType(0));
    BarrierState barrier = testBarrier(ic.typeReg, ic.objReg, false);
    if (isCall)
        frame.pushSynced(knownPushedType(1));

    stubcc.rejoin(Changes(isCall ? 2 : 1));

#ifdef JS_POLYIC
    if (!getElemICs.append(ic))
        return false;
#endif

    finishBarrier(barrier, REJOIN_FALLTHROUGH, isCall ? 1 : 0);
    return true;
}

static inline bool
ReallySimpleStrictTest(FrameEntry *fe)
{
    if (!fe->isTypeKnown())
        return false;
    JSValueType type = fe->getKnownType();
    return type == JSVAL_TYPE_NULL || type == JSVAL_TYPE_UNDEFINED;
}

static inline bool
BooleanStrictTest(FrameEntry *fe)
{
    return fe->isConstant() && fe->getKnownType() == JSVAL_TYPE_BOOLEAN;
}

void
mjit::Compiler::jsop_stricteq(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    Assembler::Condition cond = (op == JSOP_STRICTEQ) ? Assembler::Equal : Assembler::NotEqual;

    /*
     * NB: x64 can do full-Value comparisons. This is beneficial
     * to do if the payload/type are not yet in registers.
     */

    /* Constant-fold. */
    if (lhs->isConstant() && rhs->isConstant()) {
        JSBool b;
        StrictlyEqual(cx, lhs->getValue(), rhs->getValue(), &b);
        frame.popn(2);
        frame.push(BooleanValue((op == JSOP_STRICTEQ) ? b : !b));
        return;
    }

    if (frame.haveSameBacking(lhs, rhs)) {
        /* False iff NaN. */
        if (lhs->isTypeKnown() && lhs->isNotType(JSVAL_TYPE_DOUBLE)) {
            frame.popn(2);
            frame.push(BooleanValue(op == JSOP_STRICTEQ));
            return;
        }

        if (lhs->isType(JSVAL_TYPE_DOUBLE))
            frame.forgetKnownDouble(lhs);

        /* Assume NaN is either in canonical form or has the sign bit set (by jsop_neg). */
        RegisterID result = frame.allocReg(Registers::SingleByteRegs).reg();
        RegisterID treg = frame.copyTypeIntoReg(lhs);

        Assembler::Condition oppositeCond = (op == JSOP_STRICTEQ) ? Assembler::NotEqual : Assembler::Equal;

        /* Ignore the sign bit. */
        masm.lshiftPtr(Imm32(1), treg);
#ifndef JS_CPU_X64
        static const int ShiftedCanonicalNaNType = 0x7FF80000 << 1;
#ifdef JS_CPU_SPARC
        /* On Sparc the result 0/0 is 0x7FFFFFFF not 0x7FF80000 */
        masm.and32(Imm32(ShiftedCanonicalNaNType), treg);
#endif
        masm.setPtr(oppositeCond, treg, Imm32(ShiftedCanonicalNaNType), result);
#else
        static const void *ShiftedCanonicalNaNType = (void *)(0x7FF8000000000000 << 1);
        masm.move(ImmPtr(ShiftedCanonicalNaNType), Registers::ScratchReg);
        masm.setPtr(oppositeCond, treg, Registers::ScratchReg, result);
#endif
        frame.freeReg(treg);

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Comparison against undefined or null is super easy. */
    bool lhsTest;
    if ((lhsTest = ReallySimpleStrictTest(lhs)) || ReallySimpleStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;
        FrameEntry *known = lhsTest ? lhs : rhs;
        RegisterID result = frame.allocReg(Registers::SingleByteRegs).reg();

        if (test->isTypeKnown()) {
            masm.move(Imm32((test->getKnownType() == known->getKnownType()) ==
                            (op == JSOP_STRICTEQ)), result);
            frame.popn(2);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
            return;
        }

        /* This is only true if the other side is |null|. */
#ifndef JS_CPU_X64
        JSValueTag mask = known->getKnownTag();
        if (frame.shouldAvoidTypeRemat(test))
            masm.set32(cond, masm.tagOf(frame.addressOf(test)), Imm32(mask), result);
        else
            masm.set32(cond, frame.tempRegForType(test), Imm32(mask), result);
#else
        RegisterID maskReg = frame.allocReg();
        masm.move(ImmTag(known->getKnownTag()), maskReg);

        RegisterID r = frame.tempRegForType(test);
        masm.setPtr(cond, r, maskReg, result);

        frame.freeReg(maskReg);
#endif
        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Hardcoded booleans are easy too. */
    if ((lhsTest = BooleanStrictTest(lhs)) || BooleanStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isTypeKnown() && test->isNotType(JSVAL_TYPE_BOOLEAN)) {
            RegisterID result = frame.allocReg(Registers::SingleByteRegs).reg();
            frame.popn(2);

            masm.move(Imm32(op == JSOP_STRICTNE), result);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
            return;
        }

        if (test->isConstant()) {
            frame.popn(2);
            const Value &L = lhs->getValue();
            const Value &R = rhs->getValue();
            frame.push(BooleanValue((L.toBoolean() == R.toBoolean()) == (op == JSOP_STRICTEQ)));
            return;
        }

        RegisterID data = frame.copyDataIntoReg(test);

        RegisterID result = data;
        if (!(Registers::maskReg(data) & Registers::SingleByteRegs))
            result = frame.allocReg(Registers::SingleByteRegs).reg();
        
        Jump notBoolean;
        if (!test->isTypeKnown())
           notBoolean = frame.testBoolean(Assembler::NotEqual, test);

        /* Do a dynamic test. */
        bool val = lhsTest ? lhs->getValue().toBoolean() : rhs->getValue().toBoolean();
        masm.set32(cond, data, Imm32(val), result);

        if (!test->isTypeKnown()) {
            Jump done = masm.jump();
            notBoolean.linkTo(masm.label(), &masm);
            masm.move(Imm32((op == JSOP_STRICTNE)), result);
            done.linkTo(masm.label(), &masm);
        }

        if (data != result)
            frame.freeReg(data);

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Is it impossible that both Values are ints? */
    if ((lhs->isTypeKnown() && lhs->isNotType(JSVAL_TYPE_INT32)) ||
        (rhs->isTypeKnown() && rhs->isNotType(JSVAL_TYPE_INT32))) {
        prepareStubCall(Uses(2));

        if (op == JSOP_STRICTEQ)
            INLINE_STUBCALL(stubs::StrictEq, REJOIN_NONE);
        else
            INLINE_STUBCALL(stubs::StrictNe, REJOIN_NONE);

        frame.popn(2);
        frame.pushSynced(JSVAL_TYPE_BOOLEAN);
        return;
    }

#if !defined JS_CPU_ARM && !defined JS_CPU_SPARC
    /* Try an integer fast-path. */
    bool needStub = false;
    if (!lhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(j, Uses(2));
        needStub = true;
    }

    if (!rhs->isTypeKnown() && !frame.haveSameBacking(lhs, rhs)) {
        Jump j = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
        needStub = true;
    }

    FrameEntry *test  = lhs->isConstant() ? rhs : lhs;
    FrameEntry *other = lhs->isConstant() ? lhs : rhs;

    /* ReturnReg is safely usable with set32, since %ah can be accessed. */
    RegisterID resultReg = Registers::ReturnReg;
    frame.takeReg(resultReg);
    RegisterID testReg = frame.tempRegForData(test);
    frame.pinReg(testReg);

    JS_ASSERT(resultReg != testReg);

    /* Set boolean in resultReg. */
    if (other->isConstant()) {
        masm.set32(cond, testReg, Imm32(other->getValue().toInt32()), resultReg);
    } else if (frame.shouldAvoidDataRemat(other)) {
        masm.set32(cond, testReg, frame.addressOf(other), resultReg);
    } else {
        RegisterID otherReg = frame.tempRegForData(other);

        JS_ASSERT(otherReg != resultReg);
        JS_ASSERT(otherReg != testReg);

        masm.set32(cond, testReg, otherReg, resultReg);
    }

    frame.unpinReg(testReg);

    if (needStub) {
        stubcc.leave();
        if (op == JSOP_STRICTEQ)
            OOL_STUBCALL(stubs::StrictEq, REJOIN_NONE);
        else
            OOL_STUBCALL(stubs::StrictNe, REJOIN_NONE);
    }

    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, resultReg);

    if (needStub)
        stubcc.rejoin(Changes(1));
#else
    /* TODO: Port set32() logic to ARM. */
    prepareStubCall(Uses(2));

    if (op == JSOP_STRICTEQ)
        INLINE_STUBCALL(stubs::StrictEq, REJOIN_NONE);
    else
        INLINE_STUBCALL(stubs::StrictNe, REJOIN_NONE);

    frame.popn(2);
    frame.pushSynced(JSVAL_TYPE_BOOLEAN);
    return;
#endif
}

void
mjit::Compiler::jsop_pos()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isTypeKnown()) {
        if (top->getKnownType() <= JSVAL_TYPE_INT32)
            return;
        prepareStubCall(Uses(1));
        INLINE_STUBCALL(stubs::Pos, REJOIN_POS);
        frame.pop();
        frame.pushSynced(knownPushedType(0));
        return;
    }

    frame.giveOwnRegs(top);

    Jump j;
    if (frame.shouldAvoidTypeRemat(top))
        j = masm.testNumber(Assembler::NotEqual, frame.addressOf(top));
    else
        j = masm.testNumber(Assembler::NotEqual, frame.tempRegForType(top));
    stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    OOL_STUBCALL(stubs::Pos, REJOIN_POS);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_initmethod()
{
#ifdef DEBUG
    FrameEntry *obj = frame.peek(-2);
#endif
    JSAtom *atom = script->getAtom(fullAtomIndex(PC));

    /* Initializers with INITMETHOD are not fast yet. */
    JS_ASSERT(!frame.extra(obj).initObject);

    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    INLINE_STUBCALL(stubs::InitMethod, REJOIN_FALLTHROUGH);
}

void
mjit::Compiler::jsop_initprop()
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *fe = frame.peek(-1);
    JSAtom *atom = script->getAtom(fullAtomIndex(PC));

    JSObject *baseobj = frame.extra(obj).initObject;

    if (!baseobj || monitored(PC)) {
        prepareStubCall(Uses(2));
        masm.move(ImmPtr(atom), Registers::ArgReg1);
        INLINE_STUBCALL(stubs::InitProp, REJOIN_FALLTHROUGH);
        return;
    }

    JSObject *holder;
    JSProperty *prop = NULL;
#ifdef DEBUG
    bool res =
#endif
    LookupPropertyWithFlags(cx, baseobj, ATOM_TO_JSID(atom),
                            JSRESOLVE_QUALIFIED, &holder, &prop);
    JS_ASSERT(res && prop && holder == baseobj);

    RegisterID objReg = frame.copyDataIntoReg(obj);

    /* Perform the store. */
    Shape *shape = (Shape *) prop;
    Address address = masm.objPropAddress(baseobj, objReg, shape->slot);
    frame.storeTo(fe, address);
    frame.freeReg(objReg);
}

void
mjit::Compiler::jsop_initelem()
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *fe = frame.peek(-1);

    /*
     * The initialized index is always a constant, but we won't remember which
     * constant if there are branches inside the code computing the initializer
     * expression (e.g. the expression uses the '?' operator).  Slow path those
     * cases, as well as those where INITELEM is used on an object initializer
     * or a non-fast array initializer.
     */
    if (!id->isConstant() || !frame.extra(obj).initArray) {
        JSOp next = JSOp(PC[JSOP_INITELEM_LENGTH]);

        prepareStubCall(Uses(3));
        masm.move(Imm32(next == JSOP_ENDINIT ? 1 : 0), Registers::ArgReg1);
        INLINE_STUBCALL(stubs::InitElem, REJOIN_FALLTHROUGH);
        return;
    }

    int32 idx = id->getValue().toInt32();

    RegisterID objReg = frame.copyDataIntoReg(obj);

    if (cx->typeInferenceEnabled()) {
        /* Update the initialized length. */
        masm.store32(Imm32(idx + 1), Address(objReg, offsetof(JSObject, initializedLength)));
    }

    /* Perform the store. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
    frame.storeTo(fe, Address(objReg, idx * sizeof(Value)));
    frame.freeReg(objReg);
}
