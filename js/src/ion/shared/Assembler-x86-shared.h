/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
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

#ifndef jsion_assembler_x86_shared__
#define jsion_assembler_x86_shared__

#include "assembler/assembler/X86Assembler.h"

namespace js {
namespace ion {

class AssemblerX86Shared
{
  protected:
    JSC::X86Assembler masm;

    typedef JSC::X86Assembler::JmpSrc JmpSrc;
    typedef JSC::X86Assembler::JmpDst JmpDst;

  public:
    enum Condition {
        Equal = JSC::X86Assembler::ConditionE,
        NotEqual = JSC::X86Assembler::ConditionNE,
        Above = JSC::X86Assembler::ConditionA,
        AboveOrEqual = JSC::X86Assembler::ConditionAE,
        Below = JSC::X86Assembler::ConditionB,
        BelowOrEqual = JSC::X86Assembler::ConditionBE,
        GreaterThan = JSC::X86Assembler::ConditionG,
        GreaterThanOrEqual = JSC::X86Assembler::ConditionGE,
        LessThan = JSC::X86Assembler::ConditionL,
        LessThanOrEqual = JSC::X86Assembler::ConditionLE,
        Overflow = JSC::X86Assembler::ConditionO,
        Signed = JSC::X86Assembler::ConditionS,
        Zero = JSC::X86Assembler::ConditionE,
        NonZero = JSC::X86Assembler::ConditionNE
    };

    bool oom() const {
        return masm.oom();
    }
    size_t size() const {
        return masm.size();
    }
    void executableCopy(void *buffer) {
        masm.executableCopy(buffer);
    }

  public:
    void movl(const Imm32 &imm32, const Register &dest) {
        masm.movl_i32r(imm32.value, dest.code());
    }
    void movl(const Register &src, const Register &dest) {
        masm.movl_rr(src.code(), dest.code());
    }
    void movl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.movl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void movl(const Register &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_rr(src.code(), dest.reg());
            break;
          case Operand::REG_DISP:
            masm.movl_rm(src.code(), dest.disp(), dest.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }

    void movsd(const Operand &src, const FloatRegister &dest) {
        switch (src.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void movsd(const FloatRegister &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.code(), dest.fpu());
            break;
          case Operand::REG_DISP:
            masm.movsd_rm(src.code(), dest.disp(), dest.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }

    void j(Condition cond, Label *label) {
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond)), JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc j = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
    }
    void jmp(Label *label) {
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(masm.jmp(), JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc j = masm.jmp();
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
    }
    void bind(Label *label) {
        JSC::MacroAssembler::Label jsclabel;
        if (label->used()) {
            bool more;
            JSC::X86Assembler::JmpSrc jmp(label->offset());
            do {
                JSC::X86Assembler::JmpSrc next;
                more = masm.nextJump(jmp, &next);
                masm.linkJump(jmp, masm.label());
                jmp = next;
            } while (more);
        }
        label->bind(masm.label().offset());
    }

    void ret() {
        masm.ret();
    }
    void call(const Register &reg) {
        masm.call(reg.code());
    }
    void call(const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.call(op.reg());
            break;
          case Operand::REG_DISP:
            masm.call_m(op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }

    void breakpoint() {
        masm.int3();
    }

    // The below cmpq methods switch the lhs and rhs when it invokes the
    // macroassembler to conform with intel standard.  When calling this
    // function put the left operand on the left as you would expect.
    void cmpl(const Register &lhs, const Register &rhs) {
        masm.cmpl_rr(rhs.code(), lhs.code());
    }
    void cmpl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.cmpl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.cmpl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void addl(Imm32 imm, const Register &dest) {
        masm.addl_ir(imm.value, dest.code());
    }
    void addl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.addl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.addl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void subl(Imm32 imm, const Register &dest) {
        masm.subl_ir(imm.value, dest.code());
    }
    void subl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.subl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.subl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void addl(const Register &src, const Register &dest) {
        masm.addl_rr(src.code(), dest.code());
    }
    void subl(const Register &src, const Register &dest) {
        masm.subl_rr(src.code(), dest.code());
    }
    void subl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.subl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.subl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void orl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.orl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.orl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void xorl(Imm32 imm, const Register &reg) {
        masm.xorl_ir(imm.value, reg.code());
    }
    void xorl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.xorl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.xorl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void andl(Imm32 imm, const Register &dest) {
        masm.andl_ir(imm.value, dest.code());
    }
    void andl(Imm32 imm, const Operand &op) {
        switch (op.kind()) {
          case Operand::REG:
            masm.andl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.andl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void addl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.addl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.addl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void orl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.orl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.orl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void xorl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.xorl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.xorl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void andl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.andl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.andl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }

    void shrl(const Imm32 imm, const Register &dest) {
        masm.shrl_i8r(imm.value, dest.code());
    }
    void shll(const Imm32 imm, const Register &dest) {
        masm.shll_i8r(imm.value, dest.code());
    }

    void push(const Imm32 imm) {
        masm.push_i32(imm.value);
    }
    void push(const Operand &src) {
        switch (src.kind()) {
          case Operand::REG:
            masm.push_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.push_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void push(const Register &src) {
        masm.push_r(src.code());
    }
    void pop(const Operand &src) {
        switch (src.kind()) {
          case Operand::REG:
            masm.pop_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.pop_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    void pop(const Register &src) {
        masm.pop_r(src.code());
    }
};

} // namespace ion
} // namespace js

#endif // jsion_assembler_x86_shared__

