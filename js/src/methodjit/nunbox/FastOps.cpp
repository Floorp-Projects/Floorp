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
#include "jslibmath.h"
#include "jsnum.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

void
mjit::Compiler::jsop_bitnot()
{
    FrameEntry *top = frame.peek(-1);

    /* We only want to handle integers here. */
    if (top->isTypeKnown() && top->getKnownType() != JSVAL_TYPE_INT32) {
        prepareStubCall();
        stubCall(stubs::BitNot, Uses(1), Defs(1));
        frame.pop();
        frame.pushSyncedType(JSVAL_TYPE_INT32);
        return;
    }
           
    /* Test the type. */
    bool stubNeeded = false;
    if (!top->isTypeKnown()) {
        Jump intFail = frame.testInt32(Assembler::NotEqual, top);
        stubcc.linkExit(intFail);
        frame.learnType(top, JSVAL_TYPE_INT32);
        stubNeeded = true;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stubs::BitNot);
    }

    RegisterID reg = frame.ownRegForData(top);
    masm.not32(reg);
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_bitop(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

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
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    /* We only want to handle integers here. */
    if ((rhs->isTypeKnown() && rhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_INT32)) {
        prepareStubCall();
        stubCall(stub, Uses(2), Defs(1));
        frame.popn(2);
        frame.pushSyncedType(JSVAL_TYPE_INT32);
        return;
    }
           
    /* Test the types. */
    bool stubNeeded = false;
    if (!rhs->isTypeKnown()) {
        Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_TYPE_INT32);
        stubNeeded = true;
    }
    if (!lhs->isTypeKnown()) {
        Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(lhsFail);
        stubNeeded = true;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stub);
    }

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
            frame.push(Int32Value(L << R));
            return;
          case JSOP_RSH:
            frame.push(Int32Value(L >> R));
            return;
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
            if (op == JSOP_BITAND)
                masm.and32(Imm32(rhs->getValue().toInt32()), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(Imm32(rhs->getValue().toInt32()), reg);
            else
                masm.or32(Imm32(rhs->getValue().toInt32()), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            if (op == JSOP_BITAND)
                masm.and32(masm.payloadOf(frame.addressOf(rhs)), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(masm.payloadOf(frame.addressOf(rhs)), reg);
            else
                masm.or32(masm.payloadOf(frame.addressOf(rhs)), reg);
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
      {
        /* Not commutative. */
        if (rhs->isConstant()) {
            int32 shift = rhs->getValue().toInt32() & 0x1F;

            reg = frame.ownRegForData(lhs);

            if (!shift) {
                /*
                 * Just pop RHS - leave LHS. ARM can't shift by 0.
                 * Type of LHS should be learned already.
                 */
                frame.popn(2);
                frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
                if (stubNeeded)
                    stubcc.rejoin(1);
                return;
            }

            switch (op) {
              case JSOP_LSH:
                masm.lshift32(Imm32(shift), reg);
                break;
              case JSOP_RSH:
                masm.rshift32(Imm32(shift), reg);
                break;
              default:
                JS_NOT_REACHED("NYI");
            }
        } else {
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
            /* Grosssssss! RHS _must_ be in ECX, on x86 */
            RegisterID rr = frame.tempRegForData(rhs, JSC::X86Registers::ecx);
#else
            RegisterID rr = frame.tempRegForData(rhs);
#endif

            frame.pinReg(rr);
            if (lhs->isConstant()) {
                reg = frame.allocReg();
                masm.move(Imm32(lhs->getValue().toInt32()), reg);
            } else {
                reg = frame.ownRegForData(lhs);
            }
            frame.unpinReg(rr);

            switch (op) {
              case JSOP_LSH:
                masm.lshift32(rr, reg);
                break;
              case JSOP_RSH:
                masm.rshift32(rr, reg);
                break;
              default:
                JS_NOT_REACHED("NYI");
            }
        }
        break;
      }

      default:
        JS_NOT_REACHED("NYI");
        return;
    }

    frame.pop();
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(2);
}

void
mjit::Compiler::jsop_globalinc(JSOp op, uint32 index)
{
    uint32 slot = script->getGlobalSlot(index);

    bool popped = false;
    PC += JSOP_GLOBALINC_LENGTH;
    if (JSOp(*PC) == JSOP_POP && !analysis[PC].nincoming) {
        popped = true;
        PC += JSOP_POP_LENGTH;
    }

    int amt = (js_CodeSpec[op].format & JOF_INC) ? 1 : -1;
    bool post = !!(js_CodeSpec[op].format & JOF_POST);

    RegisterID data;
    RegisterID reg = frame.allocReg();
    Address addr = masm.objSlotRef(globalObj, reg, slot);
    uint32 depth = frame.stackDepth();

    if (post && !popped) {
        frame.push(addr);
        FrameEntry *fe = frame.peek(-1);
        Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
        stubcc.linkExit(notInt);
        data = frame.copyDataIntoReg(fe);
    } else {
        Jump notInt = masm.testInt32(Assembler::NotEqual, addr);
        stubcc.linkExit(notInt);
        data = frame.allocReg();
        masm.loadData32(addr, data);
    }

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), data);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), data);
    stubcc.linkExit(ovf);

    stubcc.leave();
    stubcc.masm.lea(addr, Registers::ArgReg1);
    stubcc.vpInc(op, depth);

    masm.storeData32(data, addr);

    if (!post && !popped)
        frame.pushUntypedPayload(JSVAL_TYPE_INT32, data);
    else
        frame.freeReg(data);

    frame.freeReg(reg);

    stubcc.rejoin(1);
}

static inline bool
CheckNullOrUndefined(FrameEntry *fe)
{
    if (!fe->isTypeKnown())
        return false;
    JSValueType type = fe->getKnownType();
    return type == JSVAL_TYPE_NULL || type == JSVAL_TYPE_UNDEFINED;
}

void
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

        if (test->isTypeKnown()) {
            emitStubCmpOp(stub, target, fused);
            return;
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
            frame.forgetEverything();

            if ((op == JSOP_EQ && fused == JSOP_IFNE) ||
                (op == JSOP_NE && fused == JSOP_IFEQ)) {
                Jump j = masm.branch32(Assembler::Equal, reg, ImmTag(JSVAL_TAG_UNDEFINED));
                jumpInScript(j, target);
                j = masm.branch32(Assembler::Equal, reg, ImmTag(JSVAL_TAG_NULL));
                jumpInScript(j, target);
            } else {
                Jump j = masm.branch32(Assembler::Equal, reg, ImmTag(JSVAL_TAG_UNDEFINED));
                Jump j2 = masm.branch32(Assembler::NotEqual, reg, ImmTag(JSVAL_TAG_NULL));
                jumpInScript(j2, target);
                j.linkTo(masm.label(), &masm);
            }
        } else {
            Jump j = masm.branch32(Assembler::Equal, reg, ImmTag(JSVAL_TAG_UNDEFINED));
            Jump j2 = masm.branch32(Assembler::Equal, reg, ImmTag(JSVAL_TAG_NULL));
            masm.move(Imm32(op == JSOP_NE), reg);
            Jump j3 = masm.jump();
            j2.linkTo(masm.label(), &masm);
            j.linkTo(masm.label(), &masm);
            masm.move(Imm32(op == JSOP_EQ), reg);
            j3.linkTo(masm.label(), &masm);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
        }
        return;
    }

    emitStubCmpOp(stub, target, fused);
}

void
mjit::Compiler::jsop_relational(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    /* Always slow path... */
    if ((rhs->isTypeKnown() && rhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_INT32)) {
        if (op == JSOP_EQ || op == JSOP_NE)
            jsop_equality(op, stub, target, fused);
        else
            emitStubCmpOp(stub, target, fused);
        return;
    }

    /* Test the types. */
    if (!rhs->isTypeKnown()) {
        Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_TYPE_INT32);
    }
    if (!lhs->isTypeKnown()) {
        Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(lhsFail);
    }

    Assembler::Condition cond;
    switch (op) {
      case JSOP_LT:
        cond = Assembler::LessThan;
        break;
      case JSOP_LE:
        cond = Assembler::LessThanOrEqual;
        break;
      case JSOP_GT:
        cond = Assembler::GreaterThan;
        break;
      case JSOP_GE:
        cond = Assembler::GreaterThanOrEqual;
        break;
      case JSOP_EQ:
        cond = Assembler::Equal;
        break;
      case JSOP_NE:
        cond = Assembler::NotEqual;
        break;
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    /* Swap the LHS and RHS if it makes register allocation better... or possible. */
    bool swapped = false;
    if (lhs->isConstant() ||
        (frame.shouldAvoidDataRemat(lhs) && !rhs->isConstant())) {
        FrameEntry *temp = rhs;
        rhs = lhs;
        lhs = temp;
        swapped = true;

        switch (cond) {
          case Assembler::LessThan:
            cond = Assembler::GreaterThan;
            break;
          case Assembler::LessThanOrEqual:
            cond = Assembler::GreaterThanOrEqual;
            break;
          case Assembler::GreaterThan:
            cond = Assembler::LessThan;
            break;
          case Assembler::GreaterThanOrEqual:
            cond = Assembler::LessThanOrEqual;
            break;
          case Assembler::Equal: /* fall through */
          case Assembler::NotEqual:
            /* Equal and NotEqual are commutative. */
            break;
          default:
            JS_NOT_REACHED("wat");
            break;
        }
    }

    stubcc.leave();
    stubcc.call(stub);

    if (target) {
        /* We can do a little better when we know the opcode is fused. */
        RegisterID lr = frame.ownRegForData(lhs);
        
        /* Initialize stuff to quell GCC warnings. */
        bool rhsConst;
        int32 rval = 0;
        RegisterID rr = Registers::ReturnReg;
        if (!(rhsConst = rhs->isConstant()))
            rr = frame.ownRegForData(rhs);
        else
            rval = rhs->getValue().toInt32();

        frame.pop();
        frame.pop();

        /*
         * Note: this resets the regster allocator, so rr and lr don't need
         * to be freed. We're not going to touch the frame.
         */
        frame.forgetEverything();

        /* Invert the test for IFEQ. */
        if (fused == JSOP_IFEQ) {
            switch (cond) {
              case Assembler::LessThan:
                cond = Assembler::GreaterThanOrEqual;
                break;
              case Assembler::LessThanOrEqual:
                cond = Assembler::GreaterThan;
                break;
              case Assembler::GreaterThan:
                cond = Assembler::LessThanOrEqual;
                break;
              case Assembler::GreaterThanOrEqual:
                cond = Assembler::LessThan;
                break;
              case Assembler::Equal:
                cond = Assembler::NotEqual;
                break;
              case Assembler::NotEqual:
                cond = Assembler::Equal;
                break;
              default:
                JS_NOT_REACHED("hello");
            }
        }

        Jump j;
        if (!rhsConst)
            j = masm.branch32(cond, lr, rr);
        else
            j = masm.branch32(cond, lr, Imm32(rval));

        jumpInScript(j, target);

        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");
        /*
         * The stub call has no need to rejoin, since state is synced.
         * Instead, we can just test the return value.
         */
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        j = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
        stubcc.jumpInScript(j, target);

        /* Rejoin unnecessary - state is flushed. */
        j = stubcc.masm.jump();
        stubcc.crossJump(j, masm.label());
        JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");
    } else {
        /* No fusing. Compare, set, and push a boolean. */

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
        stubcc.rejoin(1);
    }
}

void
mjit::Compiler::jsop_objtostr()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isTypeKnown() && top->getKnownType() == JSVAL_TYPE_OBJECT) {
        prepareStubCall();
        stubCall(stubs::ObjToStr, Uses(1), Defs(1));
        frame.pop();
        frame.pushSynced();
        return;
    }

    if (top->isTypeKnown())
        return;

    frame.giveOwnRegs(top);

    Jump isObj = frame.testPrimitive(Assembler::NotEqual, top);
    stubcc.linkExit(isObj);

    stubcc.leave();
    stubcc.call(stubs::ObjToStr);

    stubcc.rejoin(1);
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
            RegisterID data = frame.allocReg(Registers::SingleByteRegs);
            if (frame.shouldAvoidDataRemat(top))
                masm.loadData32(frame.addressOf(top), data);
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
            frame.pop();
            frame.push(BooleanValue(false));
            break;
          }

          default:
          {
            prepareStubCall();
            stubCall(stubs::ValueToBoolean, Uses(0), Defs(0));

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

    RegisterID data = frame.allocReg(Registers::SingleByteRegs);
    if (frame.shouldAvoidDataRemat(top))
        masm.loadData32(frame.addressOf(top), data);
    else
        masm.move(frame.tempRegForData(top), data);
    RegisterID type = frame.tempRegForType(top);
    Label syncTarget = stubcc.syncExitAndJump();


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
    stubcc.call(stubs::Not);

    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, data);

    stubcc.rejoin(1);
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
            frame.push(StringValue(ATOM_TO_STRING(atom)));
            return;
        }
    }

    prepareStubCall();
    stubCall(stubs::TypeOf, Uses(1), Defs(1));
    frame.pop();
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_STRING, Registers::ReturnReg);
}

void
mjit::Compiler::jsop_localinc(JSOp op, uint32 slot, bool popped)
{
    bool post = (op == JSOP_LOCALINC || op == JSOP_LOCALDEC);
    int32 amt = (op == JSOP_INCLOCAL || op == JSOP_LOCALINC) ? 1 : -1;
    uint32 depth = frame.stackDepth();

    frame.pushLocal(slot);

    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant() && fe->getValue().isPrimitive()) {
        Value v = fe->getValue();
        double d;
        ValueToNumber(cx, v, &d);
        d += amt;
        v.setNumber(d);
        frame.push(v);
        frame.storeLocal(slot);
        frame.pop();
        return;
    }

    if (post && !popped) {
        frame.dup();
        fe = frame.peek(-1);
    }

    if (!fe->isTypeKnown() || fe->getKnownType() != JSVAL_TYPE_INT32) {
        /* :TODO: do something smarter for the known-type-is-bad case. */
        if (fe->isTypeKnown()) {
            Jump j = masm.jump();
            stubcc.linkExit(j);
        } else {
            Jump intFail = frame.testInt32(Assembler::NotEqual, fe);
            stubcc.linkExit(intFail);
        }
    }

    RegisterID reg = frame.ownRegForData(fe);
    frame.pop();

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), reg);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), reg);
    stubcc.linkExit(ovf);

    /* Note, stub call will push original value again no matter what. */
    stubcc.leave();
    stubcc.masm.addPtr(Imm32(sizeof(Value) * slot + sizeof(JSStackFrame)),
                       JSFrameReg,
                       Registers::ArgReg1);
    stubcc.vpInc(op, depth);

    frame.pushUntypedPayload(JSVAL_TYPE_INT32, reg, true, true);
    frame.storeLocal(slot, post || popped, false);

    if (post || popped)
        frame.pop();

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_arginc(JSOp op, uint32 slot, bool popped)
{
    int amt = (js_CodeSpec[op].format & JOF_INC) ? 1 : -1;
    bool post = !!(js_CodeSpec[op].format & JOF_POST);
    uint32 depth = frame.stackDepth();

    jsop_getarg(slot);
    if (post && !popped)
        frame.dup();

    FrameEntry *fe = frame.peek(-1);
    Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
    stubcc.linkExit(notInt);

    RegisterID reg = frame.ownRegForData(fe);
    frame.pop();

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), reg);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), reg);
    stubcc.linkExit(ovf);

    Address argv(JSFrameReg, offsetof(JSStackFrame, argv));

    stubcc.leave();
    stubcc.masm.loadPtr(argv, Registers::ArgReg1);
    stubcc.masm.addPtr(Imm32(sizeof(Value) * slot), Registers::ArgReg1, Registers::ArgReg1);
    stubcc.vpInc(op, depth);

    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
    fe = frame.peek(-1);

    reg = frame.allocReg();
    masm.loadPtr(argv, reg);
    Address address = Address(reg, slot * sizeof(Value));
    frame.storeTo(fe, address, popped);
    frame.freeReg(reg);

    if (post || popped)
        frame.pop();
    else
        frame.forgetType(fe);

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_setelem()
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *fe = frame.peek(-1);

    if ((obj->isTypeKnown() && obj->getKnownType() != JSVAL_TYPE_OBJECT) ||
        (id->isTypeKnown() && id->getKnownType() != JSVAL_TYPE_INT32) ||
        (id->isConstant() && id->getValue().toInt32() < 0)) {
        jsop_setelem_slow();
        return;
    }

    /* id.isInt32() */
    if (!id->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(j);
    }

    /* obj.isObject() */
    if (!obj->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(j);
    }

    /* obj.isDenseArray() */
    RegisterID objReg = frame.copyDataIntoReg(obj);
    Jump guardDense = masm.branchPtr(Assembler::NotEqual,
                                      Address(objReg, offsetof(JSObject, clasp)),
                                      ImmPtr(&js_ArrayClass));
    stubcc.linkExit(guardDense);

    /* dslots non-NULL */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Jump guardSlots = masm.branchTestPtr(Assembler::Zero, objReg, objReg);
    stubcc.linkExit(guardSlots);

    /* guard within capacity */
    if (id->isConstant()) {
        Jump inRange = masm.branch32(Assembler::LessThanOrEqual,
                                     masm.payloadOf(Address(objReg, -int(sizeof(Value)))),
                                     Imm32(id->getValue().toInt32()));
        stubcc.linkExit(inRange);

        /* guard not a hole */
        Address slot(objReg, id->getValue().toInt32() * sizeof(Value));
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmTag(JSVAL_TAG_MAGIC));
        stubcc.linkExit(notHole);

        stubcc.leave();
        stubcc.call(stubs::SetElem);

        /* Infallible, start killing everything. */
        frame.eviscerate(obj);
        frame.eviscerate(id);

        /* Perform the store. */
        if (fe->isConstant()) {
            masm.storeValue(fe->getValue(), slot);
        } else {
            masm.storeData32(frame.tempRegForData(fe), slot);
            if (fe->isTypeKnown())
                masm.storeTypeTag(ImmTag(fe->getKnownTag()), slot);
            else
                masm.storeTypeTag(frame.tempRegForType(fe), slot);
        }
    } else {
        RegisterID idReg = frame.copyDataIntoReg(id);
        Jump inRange = masm.branch32(Assembler::AboveOrEqual,
                                     idReg,
                                     masm.payloadOf(Address(objReg, -int(sizeof(Value)))));
        stubcc.linkExit(inRange);

        /* guard not a hole */
        BaseIndex slot(objReg, idReg, Assembler::JSVAL_SCALE);
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmTag(JSVAL_TAG_MAGIC));
        stubcc.linkExit(notHole);

        stubcc.leave();
        stubcc.call(stubs::SetElem);

        /* Infallible, start killing everything. */
        frame.eviscerate(obj);
        frame.eviscerate(id);

        /* Perform the store. */
        if (fe->isConstant()) {
            masm.storeValue(fe->getValue(), slot);
        } else {
            masm.storeData32(frame.tempRegForData(fe), slot);
            if (fe->isTypeKnown())
                masm.storeTypeTag(ImmTag(fe->getKnownTag()), slot);
            else
                masm.storeTypeTag(frame.tempRegForType(fe), slot);
        }

        frame.freeReg(idReg);
    }
    frame.freeReg(objReg);

    frame.shimmy(2);
    stubcc.rejoin(0);
}

void
mjit::Compiler::jsop_getelem()
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *id = frame.peek(-1);

    if ((obj->isTypeKnown() && obj->getKnownType() != JSVAL_TYPE_OBJECT) ||
        (id->isTypeKnown() && id->getKnownType() != JSVAL_TYPE_INT32) ||
        (id->isConstant() && id->getValue().toInt32() < 0)) {
        jsop_getelem_slow();
        return;
    }

    /* id.isInt32() */
    if (!id->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(j);
    }

    /* obj.isNonFunObj() */
    if (!obj->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(j);
    }

    /* obj.isDenseArray() */
    RegisterID objReg = frame.copyDataIntoReg(obj);
    Jump guardDense = masm.branchPtr(Assembler::NotEqual,
                                      Address(objReg, offsetof(JSObject, clasp)),
                                      ImmPtr(&js_ArrayClass));
    stubcc.linkExit(guardDense);

    /* dslots non-NULL */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Jump guardSlots = masm.branchTestPtr(Assembler::Zero, objReg, objReg);
    stubcc.linkExit(guardSlots);

    /* guard within capacity */
    if (id->isConstant()) {
        Jump inRange = masm.branch32(Assembler::LessThanOrEqual,
                                     masm.payloadOf(Address(objReg, -int(sizeof(Value)))),
                                     Imm32(id->getValue().toInt32()));
        stubcc.linkExit(inRange);

        /* guard not a hole */
        Address slot(objReg, id->getValue().toInt32() * sizeof(Value));
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmTag(JSVAL_TAG_MAGIC));
        stubcc.linkExit(notHole);

        stubcc.leave();
        stubcc.call(stubs::GetElem);

        frame.popn(2);
        frame.freeReg(objReg);
        frame.push(slot);
    } else {
        RegisterID idReg = frame.copyDataIntoReg(id);
        Jump inRange = masm.branch32(Assembler::AboveOrEqual,
                                     idReg,
                                     masm.payloadOf(Address(objReg, -int(sizeof(Value)))));
        stubcc.linkExit(inRange);

        /* guard not a hole */
        BaseIndex slot(objReg, idReg, Assembler::JSVAL_SCALE);
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmTag(JSVAL_TAG_MAGIC));
        stubcc.linkExit(notHole);

        stubcc.leave();
        stubcc.call(stubs::GetElem);

        frame.popn(2);

        RegisterID reg = frame.allocReg();
        masm.loadTypeTag(slot, reg);
        masm.loadData32(slot, idReg);
        frame.freeReg(objReg);
        frame.pushRegs(reg, idReg);
    }

    stubcc.rejoin(0);
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

    /* Comparison against undefined or null is super easy. */
    bool lhsTest;
    if ((lhsTest = ReallySimpleStrictTest(lhs)) || ReallySimpleStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isTypeKnown()) {
            FrameEntry *known = lhsTest ? lhs : rhs;
            frame.popn(2);
            frame.push(BooleanValue((test->getKnownType() == known->getKnownType()) ==
                                  (op == JSOP_STRICTEQ)));
            return;
        }

        FrameEntry *known = lhsTest ? lhs : rhs;
        JSValueTag mask = known->getKnownTag();

        /* This is only true if the other side is |null|. */
        RegisterID result = frame.allocReg(Registers::SingleByteRegs);
        if (frame.shouldAvoidTypeRemat(test))
            masm.set32(cond, masm.tagOf(frame.addressOf(test)), Imm32(mask), result);
        else
            masm.set32(cond, frame.tempRegForType(test), Imm32(mask), result);
        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Hardcoded booleans are easy too. */
    if ((lhsTest = BooleanStrictTest(lhs)) || BooleanStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isTypeKnown() && test->getKnownType() != JSVAL_TYPE_BOOLEAN) {
            frame.popn(2);
            frame.push(BooleanValue(op == JSOP_STRICTNE));
            return;
        }

        if (test->isConstant()) {
            frame.popn(2);
            const Value &L = lhs->getValue();
            const Value &R = rhs->getValue();
            frame.push(BooleanValue((L.toBoolean() == R.toBoolean()) == (op == JSOP_STRICTEQ)));
            return;
        }

        RegisterID result = frame.allocReg(Registers::SingleByteRegs);
        
        /* Is the other side boolean? */
        Jump notBoolean;
        if (!test->isTypeKnown())
           notBoolean = frame.testBoolean(Assembler::NotEqual, test);

        /* Do a dynamic test. */
        bool val = lhsTest ? lhs->getValue().toBoolean() : rhs->getValue().toBoolean();
        if (frame.shouldAvoidDataRemat(test))
            masm.set32(cond, masm.payloadOf(frame.addressOf(test)), Imm32(val), result);
        else
            masm.set32(cond, frame.tempRegForData(test), Imm32(val), result);

        if (!test->isTypeKnown()) {
            Jump done = masm.jump();
            notBoolean.linkTo(masm.label(), &masm);
            masm.move(Imm32((op == JSOP_STRICTNE)), result);
            done.linkTo(masm.label(), &masm);
        }

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    prepareStubCall();
    if (op == JSOP_STRICTEQ)
        stubCall(stubs::StrictEq, Uses(2), Defs(1));
    else
        stubCall(stubs::StrictNe, Uses(2), Defs(1));
    frame.popn(2);
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
}

void
mjit::Compiler::jsop_pos()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isTypeKnown()) {
        if (top->getKnownType() <= JSVAL_TYPE_INT32)
            return;
        prepareStubCall();
        stubCall(stubs::Pos, Uses(1), Defs(1));
        frame.pop();
        frame.pushSynced();
        return;
    }

    frame.giveOwnRegs(top);

    Jump j;
    if (frame.shouldAvoidTypeRemat(top))
        j = masm.testNumber(Assembler::NotEqual, frame.addressOf(top));
    else
        j = masm.testNumber(Assembler::NotEqual, frame.tempRegForType(top));
    stubcc.linkExit(j);

    stubcc.leave();
    stubcc.call(stubs::Pos);

    stubcc.rejoin(1);
}

