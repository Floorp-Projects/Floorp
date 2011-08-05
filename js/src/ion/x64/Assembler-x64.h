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
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"

namespace js {
namespace ion {

static const Register rax = { JSC::X86Registers::eax };
static const Register rcx = { JSC::X86Registers::ecx };
static const Register rdx = { JSC::X86Registers::edx };
static const Register r8  = { JSC::X86Registers::r8  };
static const Register r9  = { JSC::X86Registers::r9  };
static const Register r10 = { JSC::X86Registers::r10 };
static const Register r11 = { JSC::X86Registers::r11 };
static const Register r12 = { JSC::X86Registers::r12 };
static const Register r13 = { JSC::X86Registers::r13 };
static const Register r14 = { JSC::X86Registers::r14 };
static const Register r15 = { JSC::X86Registers::r15 };
static const Register rdi = { JSC::X86Registers::edi };
static const Register rsi = { JSC::X86Registers::esi };
static const Register rbx = { JSC::X86Registers::ebx };
static const Register rbp = { JSC::X86Registers::ebp };
static const Register rsp = { JSC::X86Registers::esp };

static const Register InvalidReg = { JSC::X86Registers::invalid_reg };
static const FloatRegister InvalidFloatReg = { JSC::X86Registers::invalid_xmm };

static const Register StackPointer = rsp;
static const Register JSReturnReg = rcx;

// Different argument registers for WIN64
#if defined(_WIN64)
static const Register ArgReg1 = rcx;
static const Register ArgReg2 = rdx;
static const Register ArgReg3 = r8;
static const Register ArgReg4 = r9;
#else
static const Register ArgReg1 = rdi;
static const Register ArgReg2 = rsi;
static const Register ArgReg3 = rdx;
static const Register ArgReg4 = rcx;
#endif

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
    // x64 jumps may need extra bits of relocation, because a jump may extend
    // beyond the signed 32-bit range. To account for this we add an extended
    // jump table at the bottom of the instruction stream, and if a jump
    // overflows its range, it will redirect here.
    //
    // In our relocation table, we store two offsets instead of one: the offset
    // to the original jump, and an offset to the extended jump if we will need
    // to use it instead. The offsets are stored as:
    //    [unsigned] Unsigned offset to short jump, from the start of the code.
    //    [unsigned] Unsigned offset to the extended jump, from the start of
    //               the jump table, in units of SizeOfJumpTableEntry.
    //
    // The start of the relocation table contains the offset from the code
    // buffer to the start of the extended jump table.
    //
    // Each entry in this table is a jmp [rip], where the next eight bytes
    // contain an immediate address. This comes out to 14 bytes, which we pad
    // to 16.
    //    +1 byte for opcode
    //    +1 byte for mod r/m
    //    +4 bytes for rip-relative offset (0)
    //    +8 bytes for 64-bit address
    //
    static const uint32 SizeOfExtendedJump = 1 + 1 + 4 + 8;
    static const uint32 SizeOfJumpTableEntry = 16;

    uint32 extendedJumpTable_;

    static IonCode *CodeFromJump(IonCode *code, uint8 *jump);

  private:
    void writeRelocation(JmpSrc src);
    void addPendingJump(JmpSrc src, void *target, Relocation::Kind reloc);

  public:
    using AssemblerX86Shared::j;
    using AssemblerX86Shared::jmp;

    Assembler()
      : extendedJumpTable_(0)
    {
    }

    static void TraceRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush();

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8 *buffer);

    // Actual assembly emitting functions.

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
    void movq(const Register &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movq_rr(src.code(), dest.reg());
            break;
          case Operand::REG_DISP:
            masm.movq_rm(src.code(), dest.disp(), dest.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void movqsd(const Register &src, const FloatRegister &dest) {
        masm.movq_rr(src.code(), dest.code());
    }
    void movqsd(const FloatRegister &src, const Register &dest) {
        masm.movq_rr(src.code(), dest.code());
    }

    void addq(Imm32 imm, const Register &dest) {
        masm.addq_ir(imm.value, dest.code());
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
    void addq(const Register &src, const Register &dest) {
        masm.addq_rr(src.code(), dest.code());
    }

    void subq(Imm32 imm, const Register &dest) {
        masm.subq_ir(imm.value, dest.code());
    }
    void subq(const Register &src, const Register &dest) {
        masm.subq_rr(src.code(), dest.code());
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

    void mov(const Imm32 &imm32, const Register &dest) {
        movq(ImmWord(imm32.value), dest);
    }
    void mov(const Operand &src, const Register &dest) {
        movq(src, dest);
    }
    void mov(const Register &src, const Operand &dest) {
        movq(src, dest);
    }
    void mov(const Register &src, const Register &dest) {
        masm.movq_rr(src.code(), dest.code());
    }
    // The below cmpq methods switch the lhs and rhs when it invokes the
    // macroassembler to conform with intel standard.  When calling this
    // function put the left operand on the left as you would expect.
    void cmpq(const Operand &lhs, const Register &rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.cmpq_rr(rhs.code(), lhs.reg());
            break;
          case Operand::REG_DISP:
            masm.cmpq_rm(rhs.code(), lhs.disp(), lhs.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void cmpq(const Register &lhs, const Operand &rhs) {
        switch (rhs.kind()) {
          case Operand::REG:
            masm.cmpq_rr(rhs.reg(), lhs.code());
            break;
          case Operand::REG_DISP:
            masm.cmpq_mr(rhs.disp(), rhs.base(), lhs.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void cmpq(const Register &lhs, const Register &rhs) {
        masm.cmpq_rr(rhs.code(), lhs.code());
    }

    void jmp(void *target, Relocation::Kind reloc) {
        JmpSrc src = masm.jmp();
        addPendingJump(src, target, reloc);
    }
    void j(Condition cond, void *target, Relocation::Kind reloc) {
        JmpSrc src = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
        addPendingJump(src, target, reloc);
    }
};

} // namespace js
} // namespace ion

#endif // jsion_cpu_x64_assembler_h__

