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
 *   David Anderson <danderson@mozilla.com>
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

#ifndef jsion_cpu_x64_assembler_h__
#define jsion_cpu_x64_assembler_h__

#include "ion/shared/Assembler-shared.h"

namespace js {
namespace ion {

static const Register rcx = { JSC::X86Registers::ecx };
static const Register rbp = { JSC::X86Registers::ebp };
static const Register rsp = { JSC::X86Registers::esp };

static const Register StackPointer = rsp;
static const Register JSReturnReg = rcx;

class Operand
{
  public:
    enum Kind {
        REG,
        REG_DISP,
        FPREG
    };

    Kind kind_ : 2;
    int32 base_ : 5;
    int32 disp_;

  public:
    explicit Operand(const Register &reg)
      : kind_(REG),
        base_(reg.code())
    { }
    explicit Operand(const FloatRegister &reg)
      : kind_(FPREG),
        base_(reg.code())
    { }
    Operand(const Register &reg, int32 disp)
      : kind_(REG_DISP),
        base_(reg.code()),
        disp_(disp)
    { }

    Kind kind() const {
        return kind_;
    }
    Register::Code reg() const {
        JS_ASSERT(kind() == REG);
        return (Registers::Code)base_;
    }
    Register::Code base() const {
        JS_ASSERT(kind() == REG_DISP);
        return (Registers::Code)base_;
    }
    FloatRegisters::Code fpu() const {
        JS_ASSERT(kind() == FPREG);
        return (FloatRegisters::Code)base_;
    }
    int32 disp() const {
        JS_ASSERT(kind() == REG_DISP);
        return disp_;
    }
};

} // namespace js
} // namespace ion

#include "ion/shared/Assembler-x86-shared.h"

namespace js {
namespace ion {

class Assembler : public AssemblerX86Shared
{
  public:
      void movq(ImmWord word, const Register &dest) {
          masm.movq_i64r(word.value, dest.code());
      }
      void movq(ImmGCPtr ptr, const Register &dest) {
          masm.movq_i64r(ptr.value, dest.code());
      }
      void movq(const Operand &src, const Register &dest) {
          switch (src.kind()) {
            case Operand::REG:
              masm.movq_rr(src.reg(), dest.code());
              break;
            case Operand::REG_DISP:
              masm.movq_mr(src.disp(), src.base(), dest.code());
              break;
            default:
              JS_NOT_REACHED("unexpected operand kind");
          }
      }
      void addq(Imm32 imm, const Operand &dest) {
          switch (dest.kind()) {
            case Operand::REG:
              masm.addq_ir(imm.value, dest.reg());
              break;
            case Operand::REG_DISP:
              masm.addq_im(imm.value, dest.disp(), dest.base());
              break;
            default:
              JS_NOT_REACHED("unexpected operand kind");
          }
      }
      void subq(Imm32 imm, const Register &dest) {
          masm.subq_ir(imm.value, dest.code());
      }
      void shlq(Imm32 imm, const Register &dest) {
          masm.shlq_i8r(imm.value, dest.code());
      }
      void orq(const Operand &src, const Register &dest) {
          switch (src.kind()) {
            case Operand::REG:
              masm.orq_rr(src.reg(), dest.code());
              break;
            case Operand::REG_DISP:
              masm.orq_mr(src.disp(), src.base(), dest.code());
              break;
            default:
              JS_NOT_REACHED("unexpected operand kind");
          }
      }
};

} // namespace js
} // namespace ion

#endif // jsion_cpu_x64_assembler_h__

