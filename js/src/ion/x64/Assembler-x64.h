/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_cpu_x64_assembler_h__
#define jsion_cpu_x64_assembler_h__

#include "ion/shared/Assembler-shared.h"
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"
#include "mozilla/Util.h"

namespace js {
namespace ion {

static const Register rax = { JSC::X86Registers::eax };
static const Register rbx = { JSC::X86Registers::ebx };
static const Register rcx = { JSC::X86Registers::ecx };
static const Register rdx = { JSC::X86Registers::edx };
static const Register rsi = { JSC::X86Registers::esi };
static const Register rdi = { JSC::X86Registers::edi };
static const Register rbp = { JSC::X86Registers::ebp };
static const Register r8  = { JSC::X86Registers::r8  };
static const Register r9  = { JSC::X86Registers::r9  };
static const Register r10 = { JSC::X86Registers::r10 };
static const Register r11 = { JSC::X86Registers::r11 };
static const Register r12 = { JSC::X86Registers::r12 };
static const Register r13 = { JSC::X86Registers::r13 };
static const Register r14 = { JSC::X86Registers::r14 };
static const Register r15 = { JSC::X86Registers::r15 };
static const Register rsp = { JSC::X86Registers::esp };

static const FloatRegister xmm0 = { JSC::X86Registers::xmm0 };
static const FloatRegister xmm1 = { JSC::X86Registers::xmm1 };
static const FloatRegister xmm2 = { JSC::X86Registers::xmm2 };
static const FloatRegister xmm3 = { JSC::X86Registers::xmm3 };
static const FloatRegister xmm4 = { JSC::X86Registers::xmm4 };
static const FloatRegister xmm5 = { JSC::X86Registers::xmm5 };
static const FloatRegister xmm6 = { JSC::X86Registers::xmm6 };
static const FloatRegister xmm7 = { JSC::X86Registers::xmm7 };
static const FloatRegister xmm8 = { JSC::X86Registers::xmm8 };
static const FloatRegister xmm9 = { JSC::X86Registers::xmm9 };
static const FloatRegister xmm10 = { JSC::X86Registers::xmm10 };
static const FloatRegister xmm11 = { JSC::X86Registers::xmm11 };
static const FloatRegister xmm12 = { JSC::X86Registers::xmm12 };
static const FloatRegister xmm13 = { JSC::X86Registers::xmm13 };
static const FloatRegister xmm14 = { JSC::X86Registers::xmm14 };
static const FloatRegister xmm15 = { JSC::X86Registers::xmm15 };

// X86-common synonyms.
static const Register eax = rax;
static const Register ebx = rbx;
static const Register ecx = rcx;
static const Register edx = rdx;
static const Register esi = rsi;
static const Register edi = rdi;
static const Register ebp = rbp;
static const Register esp = rsp;

static const Register InvalidReg = { JSC::X86Registers::invalid_reg };
static const FloatRegister InvalidFloatReg = { JSC::X86Registers::invalid_xmm };

static const Register StackPointer = rsp;
static const Register FramePointer = rbp;
static const Register JSReturnReg = rcx;
// Avoid, except for assertions.
static const Register JSReturnReg_Type = JSReturnReg;
static const Register JSReturnReg_Data = JSReturnReg;

static const Register ReturnReg = rax;
static const Register ScratchReg = r11;
static const FloatRegister ReturnFloatReg = xmm0;
static const FloatRegister ScratchFloatReg = xmm15;

static const Register ArgumentsRectifierReg = r8;
static const Register CallTempReg0 = rax;
static const Register CallTempReg1 = rdi;
static const Register CallTempReg2 = rbx;
static const Register CallTempReg3 = rcx;
static const Register CallTempReg4 = rsi;
static const Register CallTempReg5 = rdx;

// Different argument registers for WIN64
#if defined(_WIN64)
static const Register IntArgReg0 = rcx;
static const Register IntArgReg1 = rdx;
static const Register IntArgReg2 = r8;
static const Register IntArgReg3 = r9;
static const uint32_t NumIntArgRegs = 4;
static const Register IntArgRegs[NumIntArgRegs] = { rcx, rdx, r8, r9 };

static const Register CallTempNonArgRegs[] = { rax, rdi, rbx, rsi };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

static const FloatRegister FloatArgReg0 = xmm0;
static const FloatRegister FloatArgReg1 = xmm1;
static const FloatRegister FloatArgReg2 = xmm2;
static const FloatRegister FloatArgReg3 = xmm3;
static const uint32_t NumFloatArgRegs = 4;
static const FloatRegister FloatArgRegs[NumFloatArgRegs] = { xmm0, xmm1, xmm2, xmm3 };
#else
static const Register IntArgReg0 = rdi;
static const Register IntArgReg1 = rsi;
static const Register IntArgReg2 = rdx;
static const Register IntArgReg3 = rcx;
static const Register IntArgReg4 = r8;
static const Register IntArgReg5 = r9;
static const uint32_t NumIntArgRegs = 6;
static const Register IntArgRegs[NumIntArgRegs] = { rdi, rsi, rdx, rcx, r8, r9 };

static const Register CallTempNonArgRegs[] = { rax, rbx };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

static const FloatRegister FloatArgReg0 = xmm0;
static const FloatRegister FloatArgReg1 = xmm1;
static const FloatRegister FloatArgReg2 = xmm2;
static const FloatRegister FloatArgReg3 = xmm3;
static const FloatRegister FloatArgReg4 = xmm4;
static const FloatRegister FloatArgReg5 = xmm5;
static const FloatRegister FloatArgReg6 = xmm6;
static const FloatRegister FloatArgReg7 = xmm7;
static const uint32_t NumFloatArgRegs = 8;
static const FloatRegister FloatArgRegs[NumFloatArgRegs] = { xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 };
#endif

static const Register OsrFrameReg = IntArgReg3;

static const Register PreBarrierReg = rdx;

// GCC stack is aligned on 16 bytes, but we don't maintain the invariant in
// jitted code.
static const uint32_t StackAlignment = 16;
static const bool StackKeptAligned = false;

static const Scale ScalePointer = TimesEight;

class Operand
{
  public:
    enum Kind {
        REG,
        REG_DISP,
        FPREG,
        SCALE
    };

    Kind kind_ : 3;
    int32_t base_ : 5;
    Scale scale_ : 3;
    int32_t disp_;
    int32_t index_ : 5;

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
    explicit Operand(const BaseIndex &address)
      : kind_(SCALE),
        base_(address.base.code()),
        scale_(address.scale),
        disp_(address.offset),
        index_(address.index.code())
    { }
    Operand(Register base, Register index, Scale scale, int32_t disp = 0)
      : kind_(SCALE),
        base_(base.code()),
        scale_(scale),
        disp_(disp),
        index_(index.code())
    { }
    Operand(Register reg, int32_t disp)
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
    int32_t disp() const {
        JS_ASSERT(kind() == REG_DISP || kind() == SCALE);
        return disp_;
    }
};

} // namespace ion
} // namespace js

#include "ion/shared/Assembler-x86-shared.h"

namespace js {
namespace ion {

// Return operand from a JS -> JS call.
static const ValueOperand JSReturnOperand = ValueOperand(JSReturnReg);

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
    static const uint32_t SizeOfExtendedJump = 1 + 1 + 4 + 8;
    static const uint32_t SizeOfJumpTableEntry = 16;

    uint32_t extendedJumpTable_;

    static IonCode *CodeFromJump(IonCode *code, uint8_t *jump);

  private:
    void writeRelocation(JmpSrc src, Relocation::Kind reloc);
    void addPendingJump(JmpSrc src, void *target, Relocation::Kind reloc);

  protected:
    size_t addPatchableJump(JmpSrc src, Relocation::Kind reloc);

  public:
    using AssemblerX86Shared::j;
    using AssemblerX86Shared::jmp;
    using AssemblerX86Shared::push;

    static uint8_t *PatchableJumpAddress(IonCode *code, size_t index);
    static void PatchJumpEntry(uint8_t *entry, uint8_t *target);

    Assembler()
      : extendedJumpTable_(0)
    {
    }

    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush();

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8_t *buffer);

    // Actual assembly emitting functions.

    void push(const ImmGCPtr ptr) {
        movq(ptr, ScratchReg);
        push(ScratchReg);
    }
    void push(const ImmWord ptr) {
        // We often end up with ImmWords that actually fit into int32.
        // Be aware of the sign extension behavior.
        if (ptr.value <= INT32_MAX) {
            push(Imm32(ptr.value));
        } else {
            movq(ptr, ScratchReg);
            push(ScratchReg);
        }
    }
    void push(const FloatRegister &src) {
        subq(Imm32(sizeof(void*)), StackPointer);
        movsd(src, Operand(StackPointer, 0));
    }
    CodeOffsetLabel pushWithPatch(const ImmWord &word) {
        movq(word, ScratchReg);
        CodeOffsetLabel label = masm.currentOffset();
        push(ScratchReg);
        return label;
    }

    void movq(ImmWord word, const Register &dest) {
        masm.movq_i64r(word.value, dest.code());
    }
    void movq(ImmGCPtr ptr, const Register &dest) {
        masm.movq_i64r(ptr.value, dest.code());
        writeDataRelocation(ptr);
    }
    void movq(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.movq_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movq_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
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
          case Operand::SCALE:
            masm.movq_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
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
    void movq(const Register &src, const Register &dest) {
        masm.movq_rr(src.code(), dest.code());
    }

    void andq(const Register &src, const Register &dest) {
        masm.andq_rr(src.code(), dest.code());
    }
    void andq(Imm32 imm, const Register &dest) {
        masm.andq_ir(imm.value, dest.code());
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
    void shrq(Imm32 imm, const Register &dest) {
        masm.shrq_i8r(imm.value, dest.code());
    }
    void orq(Imm32 imm, const Register &dest) {
        masm.orq_ir(imm.value, dest.code());
    }
    void orq(const Register &src, const Register &dest) {
        masm.orq_rr(src.code(), dest.code());
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
    void xorq(const Register &src, const Register &dest) {
        masm.xorq_rr(src.code(), dest.code());
    }

    void mov(ImmWord word, const Register &dest) {
        movq(word, dest);
    }
    void mov(const Imm32 &imm32, const Register &dest) {
        movl(imm32, dest);
    }
    void mov(const Operand &src, const Register &dest) {
        movq(src, dest);
    }
    void mov(const Register &src, const Operand &dest) {
        movq(src, dest);
    }
    void mov(const Register &src, const Register &dest) {
        movq(src, dest);
    }
    void mov(AbsoluteLabel *label, const Register &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
        masm.movq_i64r(label->prev(), dest.code());
        label->setPrev(masm.size());
    }
    void lea(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.leaq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.leaq_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
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
    void cmpq(const Operand &lhs, Imm32 rhs) {
        switch (lhs.kind()) {
          case Operand::REG_DISP:
            masm.cmpq_im(rhs.value, lhs.disp(), lhs.base());
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
    void cmpq(Imm32 lhs, const Register &rhs) {
        masm.cmpq_ir(lhs.value, rhs.code());
    }
    
    void testq(const Register &lhs, Imm32 rhs) {
        masm.testq_i32r(rhs.value, lhs.code());
    }
    void testq(const Register &lhs, const Register &rhs) {
        masm.testq_rr(rhs.code(), lhs.code());
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

    // Do not mask shared implementations.
    using AssemblerX86Shared::call;

    void cvttsd2sq(const FloatRegister &src, const Register &dest) {
        masm.cvttsd2sq_rr(src.code(), dest.code());
    }
    void cvttsd2s(const FloatRegister &src, const Register &dest) {
        cvttsd2sq(src, dest);
    }
    void cvtsq2sd(const Register &src, const FloatRegister &dest) {
        masm.cvtsq2sd_rr(src.code(), dest.code());
    }
};

static inline void
PatchJump(CodeLocationJump jump, CodeLocationLabel label)
{
    if (JSC::X86Assembler::canRelinkJump(jump.raw(), label.raw())) {
        JSC::X86Assembler::setRel32(jump.raw(), label.raw());
    } else {
        JSC::X86Assembler::setRel32(jump.raw(), jump.jumpTableEntry());
        Assembler::PatchJumpEntry(jump.jumpTableEntry(), label.raw());
    }
}

static inline bool
GetIntArgReg(uint32_t intArg, uint32_t floatArg, Register *out)
{
#if defined(_WIN64)
    uint32_t arg = intArg + floatArg;
#else
    uint32_t arg = intArg;
#endif
    if (arg >= NumIntArgRegs)
        return false;
    *out = IntArgRegs[arg];
    return true;
}

// Get a register in which we plan to put a quantity that will be used as an
// integer argument.  This differs from GetIntArgReg in that if we have no more
// actual argument registers to use we will fall back on using whatever
// CallTempReg* don't overlap the argument registers, and only fail once those
// run out too.
static inline bool
GetTempRegForIntArg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
    if (GetIntArgReg(usedIntArgs, usedFloatArgs, out))
        return true;
    // Unfortunately, we have to assume things about the point at which
    // GetIntArgReg returns false, because we need to know how many registers it
    // can allocate.
#if defined(_WIN64)
    uint32_t arg = usedIntArgs + usedFloatArgs;
#else
    uint32_t arg = usedIntArgs;
#endif
    arg -= NumIntArgRegs;
    if (arg >= NumCallTempNonArgRegs)
        return false;
    *out = CallTempNonArgRegs[arg];
    return true;
}

static inline bool
GetFloatArgReg(uint32_t intArg, uint32_t floatArg, FloatRegister *out)
{
#if defined(_WIN64)
    uint32_t arg = intArg + floatArg;
#else
    uint32_t arg = floatArg;
#endif
    if (floatArg >= NumFloatArgRegs)
        return false;
    *out = FloatArgRegs[arg];
    return true;
}

} // namespace ion
} // namespace js

#endif // jsion_cpu_x64_assembler_h__
