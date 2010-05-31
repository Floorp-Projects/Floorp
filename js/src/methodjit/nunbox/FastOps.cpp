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
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

void
mjit::Compiler::jsop_bindname(uint32 index)
{
    RegisterID reg = frame.allocReg();
    masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, scopeChain)), reg);

    Address address(reg, offsetof(JSObject, fslots) + JSSLOT_PARENT * sizeof(jsval));

    Jump j = masm.branch32(Assembler::NotEqual, masm.payloadOf(address), Imm32(0));

    stubcc.linkExit(j);
    stubcc.leave();
    stubcc.call(stubs::BindName);

    frame.pushTypedPayload(JSVAL_MASK32_NONFUNOBJ, reg);

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_bitop(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* We only want to handle integers here. */
    if ((rhs->isTypeKnown() && rhs->getTypeTag() != JSVAL_MASK32_INT32) ||
        (lhs->isTypeKnown() && lhs->getTypeTag() != JSVAL_MASK32_INT32)) {
        prepareStubCall();
        stubCall(stubs::BitAnd, Uses(2), Defs(1));
        frame.popn(2);
        frame.pushSyncedType(JSVAL_MASK32_INT32);
        return;
    }
           
    /* Test the types. */
    if (!rhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(rhs);
        Jump rhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_MASK32_INT32);
    }
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        Jump lhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(lhsFail);
    }

    stubcc.leave();
    stubcc.call(stubs::BitAnd);

    if (lhs->isConstant() && rhs->isConstant()) {
        int32 L = lhs->getValue().asInt32();
        int32 R = rhs->getValue().asInt32();

        frame.popn(2);
        switch (op) {
          case JSOP_BITAND:
            frame.push(Value(Int32Tag(L & R)));
            return;

          default:
            JS_NOT_REACHED("say wat");
        }
    }

    RegisterID reg;

    switch (op) {
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
            masm.and32(Imm32(rhs->getValue().asInt32()), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            masm.and32(masm.payloadOf(frame.addressOf(rhs)), reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            masm.and32(rhsReg, reg);
        }

        break;
      }

      default:
        JS_NOT_REACHED("NYI");
        return;
    }

    frame.pop();
    frame.pop();
    frame.pushTypedPayload(JSVAL_MASK32_INT32, reg);

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

    if (post && !popped) {
        frame.push(addr);
        FrameEntry *fe = frame.peek(-1);
        Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
        stubcc.linkExit(notInt);
        data = frame.copyData(fe);
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
    stubcc.vpInc(op, post && !popped);

    masm.storeData32(data, addr);

    if (!post && !popped)
        frame.pushUntypedPayload(JSVAL_MASK32_INT32, data);
    else
        frame.freeReg(data);

    frame.freeReg(reg);

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_relational(JSOp op, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Test the types. */
    if (!rhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(rhs);
        Jump rhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_MASK32_INT32);
    }
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        Jump lhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(lhsFail);
    }
}

