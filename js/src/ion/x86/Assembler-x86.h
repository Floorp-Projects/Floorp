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

#ifndef jsion_cpu_x86_assembler_h__
#define jsion_cpu_x86_assembler_h__

#include "ion/shared/Assembler-shared.h"
#include "assembler/assembler/X86Assembler.h"
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"

namespace js {
namespace ion {

static const Register eax = { JSC::X86Registers::eax };
static const Register ecx = { JSC::X86Registers::ecx };
static const Register edx = { JSC::X86Registers::edx };
static const Register ebx = { JSC::X86Registers::ebx };
static const Register esp = { JSC::X86Registers::esp };
static const Register ebp = { JSC::X86Registers::ebp };
static const Register esi = { JSC::X86Registers::esi };
static const Register edi = { JSC::X86Registers::edi };

static const Register InvalidReg = { JSC::X86Registers::invalid_reg };
static const FloatRegister InvalidFloatReg = { JSC::X86Registers::invalid_xmm };

static const Register JSReturnReg_Type = ecx;
static const Register JSReturnReg_Data = edx;
static const Register StackPointer = esp;
static const Register ReturnReg = eax;
static const FloatRegister ScratchFloatReg = { JSC::X86Registers::xmm7 };

static const Register ArgumentsRectifierReg = esi;
static const Register CallTempReg1 = edi;
static const Register CallTempReg2 = eax;

static const Register OsrFrameReg = edx;

// GCC stack is aligned on 16 bytes, but we don't maintain the invariant in
// jitted code.
static const uint32 StackAlignment = 16;
static const bool StackKeptAligned = false;

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32(mask))
    { }
};

struct ImmType : public ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSVAL_TYPE_TO_TAG(type))
    { }
};

enum Scale {
    TimesOne,
    TimesTwo,
    TimesFour,
    TimesEight
};

static const Scale ScalePointer = TimesFour;

class Operand
{
  public:
    enum Kind {
        REG,
        REG_DISP,
        FPREG,
        SCALE,
        ADDRESS
    };

    Kind kind_ : 4;
    int32 index_ : 5;
    Scale scale_ : 3;
    int32 base_;
    int32 disp_;

  public:
    explicit Operand(Register reg)
      : kind_(REG),
        base_(reg.code())
    { }
    explicit Operand(FloatRegister reg)
      : kind_(FPREG),
        base_(reg.code())
    { }
    explicit Operand(const Address &address)
      : kind_(REG_DISP),
        base_(address.base.code()),
        disp_(address.offset)
    { }
    Operand(Register base, Register index, Scale scale, int32 disp = 0)
      : kind_(SCALE),
        index_(index.code()),
        scale_(scale),
        base_(base.code()),
        disp_(disp)
    { }
    Operand(Register reg, int32 disp)
      : kind_(REG_DISP),
        base_(reg.code()),
        disp_(disp)
    { }
    explicit Operand(void *address)
      : kind_(ADDRESS),
        base_(reinterpret_cast<int32>(address))
    { }

    Kind kind() const {
        return kind_;
    }
    Registers::Code reg() const {
        JS_ASSERT(kind() == REG);
        return (Registers::Code)base_;
    }
    Registers::Code base() const {
        JS_ASSERT(kind() == REG_DISP || kind() == SCALE);
        return (Registers::Code)base_;
    }
    Registers::Code index() const {
        JS_ASSERT(kind() == SCALE);
        return (Registers::Code)index_;
    }
    Scale scale() const {
        JS_ASSERT(kind() == SCALE);
        return scale_;
    }
    FloatRegisters::Code fpu() const {
        JS_ASSERT(kind() == FPREG);
        return (FloatRegisters::Code)base_;
    }
    int32 disp() const {
        JS_ASSERT(kind() == REG_DISP || kind() == SCALE);
        return disp_;
    }
    void *address() const {
        JS_ASSERT(kind() == ADDRESS);
        return reinterpret_cast<void *>(base_);
    }
};

} // namespace ion
} // namespace js

#include "ion/shared/Assembler-x86-shared.h"

namespace js {
namespace ion {

static inline void
PatchJump(CodeLocationJump jump, CodeLocationLabel label)
{
    JSC::X86Assembler::setRel32(jump.raw(), label.raw());
}

// Return operand from a JS -> JS call.
static const ValueOperand JSReturnOperand = ValueOperand(JSReturnReg_Type, JSReturnReg_Data);

class Assembler : public AssemblerX86Shared
{
    void writeRelocation(JmpSrc src) {
        jumpRelocations_.writeUnsigned(src.offset());
    }
    void addPendingJump(JmpSrc src, void *target, Relocation::Kind kind) {
        enoughMemory_ &= jumps_.append(RelativePatch(src.offset(), target, kind));
        if (kind == Relocation::IONCODE)
            writeRelocation(src);
    }

  public:
    using AssemblerX86Shared::movl;
    using AssemblerX86Shared::j;
    using AssemblerX86Shared::jmp;
    using AssemblerX86Shared::movsd;
    using AssemblerX86Shared::retarget;
    using AssemblerX86Shared::cmpl;
    using AssemblerX86Shared::call;
    using AssemblerX86Shared::push;

    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush() { }

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8 *buffer);

    // Actual assembly emitting functions.

    void push(const ImmGCPtr &ptr) {
        push(Imm32(ptr.value));
        writeDataRelocation(masm.currentOffset());
    }
    void push(const FloatRegister &src) {
        subl(Imm32(sizeof(double)), StackPointer);
        movsd(src, Operand(StackPointer, 0));
    }

    void movl(const ImmGCPtr &ptr, const Register &dest) {
        masm.movl_i32r(ptr.value, dest.code());
        writeDataRelocation(masm.currentOffset());
    }
    void movl(const ImmGCPtr &ptr, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_i32r(ptr.value, dest.reg());
            writeDataRelocation(masm.currentOffset());
            break;
          case Operand::REG_DISP:
            masm.movl_i32m(ptr.value, dest.disp(), dest.base());
            writeDataRelocation(masm.currentOffset());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void movl(ImmWord imm, Register dest) {
        masm.movl_i32r(imm.value, dest.code());
    }
    void mov(ImmWord imm, Register dest) {
        movl(imm, dest);
    }
    void mov(Imm32 imm, Register dest) {
        movl(imm, dest);
    }
    void mov(const Operand &src, const Register &dest) {
        movl(src, dest);
    }
    void mov(const Register &src, const Operand &dest) {
        movl(src, dest);
    }
    void mov(AbsoluteLabel *label, const Register &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
        masm.movl_i32r(label->prev(), dest.code());
        label->setPrev(masm.size());
    }
    void mov(const Register &src, const Register &dest) {
        movl(src, dest);
    }
    void lea(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.leal_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.leal_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }
    void cvttsd2s(const FloatRegister &src, const Register &dest) {
        cvttsd2si(src, dest);
    }

    void cmpl(const Register src, ImmWord ptr) {
        masm.cmpl_ir(ptr.value, src.code());
    }
    void cmpl(const Register src, ImmGCPtr ptr) {
        masm.cmpl_ir(ptr.value, src.code());
        writeDataRelocation(masm.currentOffset());
    }
    void cmpl(const Operand &op, ImmGCPtr imm) {
        switch (op.kind()) {
          case Operand::REG:
            masm.cmpl_ir_force32(imm.value, op.reg());
            writeDataRelocation(masm.currentOffset());
            break;
          case Operand::REG_DISP:
            masm.cmpl_im_force32(imm.value, op.disp(), op.base());
            writeDataRelocation(masm.currentOffset());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
    }

    void jmp(void *target, Relocation::Kind reloc = Relocation::HARDCODED) {
        JmpSrc src = masm.jmp();
        addPendingJump(src, target, reloc);
    }
    void j(Condition cond, void *target,
           Relocation::Kind reloc = Relocation::HARDCODED) {
        JmpSrc src = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
        addPendingJump(src, target, reloc);
    }

    void jmp(IonCode *target) {
        jmp(target->raw(), Relocation::IONCODE);
    }
    void j(Condition cond, IonCode *target) {
        j(cond, target->raw(), Relocation::IONCODE);
    }
    void call(IonCode *target) {
        JmpSrc src = masm.call();
        addPendingJump(src, target->raw(), Relocation::IONCODE);
    }
    void call(ImmWord target) {
        JmpSrc src = masm.call();
        addPendingJump(src, target.asPointer(), Relocation::HARDCODED);
    }

    // Re-routes pending jumps to an external target, flushing the label in the
    // process.
    void retarget(Label *label, void *target, Relocation::Kind reloc) {
        JSC::MacroAssembler::Label jsclabel;
        if (label->used()) {
            bool more;
            JSC::X86Assembler::JmpSrc jmp(label->offset());
            do {
                JSC::X86Assembler::JmpSrc next;
                more = masm.nextJump(jmp, &next);
                addPendingJump(jmp, target, reloc);
                jmp = next;
            } while (more);
        }
        label->reset();
    }

    void movsd(const double *dp, const FloatRegister &dest) {
        masm.movsd_mr((const void *)dp, dest.code());
    }
    void movsd(AbsoluteLabel *label, const FloatRegister &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
        masm.movsd_mr(reinterpret_cast<void *>(label->prev()), dest.code());
        label->setPrev(masm.size());
    }
};

static const uint32 NumArgRegs = 0;

static inline bool
GetArgReg(uint32 arg, Register *out)
{
    return false;
}

static inline uint32
GetArgStackDisp(uint32 arg)
{
    return arg * STACK_SLOT_SIZE;
}

} // namespace ion
} // namespace js

#endif // jsion_cpu_x86_assembler_h__

