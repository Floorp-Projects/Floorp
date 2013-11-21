/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x64_Assembler_x64_h
#define jit_x64_Assembler_x64_h

#include "mozilla/Util.h"

#include "jit/IonCode.h"
#include "jit/shared/Assembler-shared.h"

namespace js {
namespace jit {

static MOZ_CONSTEXPR_VAR Register rax = { JSC::X86Registers::eax };
static MOZ_CONSTEXPR_VAR Register rbx = { JSC::X86Registers::ebx };
static MOZ_CONSTEXPR_VAR Register rcx = { JSC::X86Registers::ecx };
static MOZ_CONSTEXPR_VAR Register rdx = { JSC::X86Registers::edx };
static MOZ_CONSTEXPR_VAR Register rsi = { JSC::X86Registers::esi };
static MOZ_CONSTEXPR_VAR Register rdi = { JSC::X86Registers::edi };
static MOZ_CONSTEXPR_VAR Register rbp = { JSC::X86Registers::ebp };
static MOZ_CONSTEXPR_VAR Register r8  = { JSC::X86Registers::r8  };
static MOZ_CONSTEXPR_VAR Register r9  = { JSC::X86Registers::r9  };
static MOZ_CONSTEXPR_VAR Register r10 = { JSC::X86Registers::r10 };
static MOZ_CONSTEXPR_VAR Register r11 = { JSC::X86Registers::r11 };
static MOZ_CONSTEXPR_VAR Register r12 = { JSC::X86Registers::r12 };
static MOZ_CONSTEXPR_VAR Register r13 = { JSC::X86Registers::r13 };
static MOZ_CONSTEXPR_VAR Register r14 = { JSC::X86Registers::r14 };
static MOZ_CONSTEXPR_VAR Register r15 = { JSC::X86Registers::r15 };
static MOZ_CONSTEXPR_VAR Register rsp = { JSC::X86Registers::esp };

static MOZ_CONSTEXPR_VAR FloatRegister xmm0 = { JSC::X86Registers::xmm0 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm1 = { JSC::X86Registers::xmm1 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm2 = { JSC::X86Registers::xmm2 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm3 = { JSC::X86Registers::xmm3 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm4 = { JSC::X86Registers::xmm4 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm5 = { JSC::X86Registers::xmm5 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm6 = { JSC::X86Registers::xmm6 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm7 = { JSC::X86Registers::xmm7 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm8 = { JSC::X86Registers::xmm8 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm9 = { JSC::X86Registers::xmm9 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm10 = { JSC::X86Registers::xmm10 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm11 = { JSC::X86Registers::xmm11 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm12 = { JSC::X86Registers::xmm12 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm13 = { JSC::X86Registers::xmm13 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm14 = { JSC::X86Registers::xmm14 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm15 = { JSC::X86Registers::xmm15 };

// X86-common synonyms.
static MOZ_CONSTEXPR_VAR Register eax = rax;
static MOZ_CONSTEXPR_VAR Register ebx = rbx;
static MOZ_CONSTEXPR_VAR Register ecx = rcx;
static MOZ_CONSTEXPR_VAR Register edx = rdx;
static MOZ_CONSTEXPR_VAR Register esi = rsi;
static MOZ_CONSTEXPR_VAR Register edi = rdi;
static MOZ_CONSTEXPR_VAR Register ebp = rbp;
static MOZ_CONSTEXPR_VAR Register esp = rsp;

static MOZ_CONSTEXPR_VAR Register InvalidReg = { JSC::X86Registers::invalid_reg };
static MOZ_CONSTEXPR_VAR FloatRegister InvalidFloatReg = { JSC::X86Registers::invalid_xmm };

static MOZ_CONSTEXPR_VAR Register StackPointer = rsp;
static MOZ_CONSTEXPR_VAR Register FramePointer = rbp;
static MOZ_CONSTEXPR_VAR Register JSReturnReg = rcx;
// Avoid, except for assertions.
static MOZ_CONSTEXPR_VAR Register JSReturnReg_Type = JSReturnReg;
static MOZ_CONSTEXPR_VAR Register JSReturnReg_Data = JSReturnReg;

static MOZ_CONSTEXPR_VAR Register ReturnReg = rax;
static MOZ_CONSTEXPR_VAR Register ScratchReg = r11;
static MOZ_CONSTEXPR_VAR Register HeapReg = r15;
static MOZ_CONSTEXPR_VAR FloatRegister ReturnFloatReg = xmm0;
static MOZ_CONSTEXPR_VAR FloatRegister ScratchFloatReg = xmm15;

// Avoid rbp, which is the FramePointer, which is unavailable in some modes.
static MOZ_CONSTEXPR_VAR Register ArgumentsRectifierReg = r8;
static MOZ_CONSTEXPR_VAR Register CallTempReg0 = rax;
static MOZ_CONSTEXPR_VAR Register CallTempReg1 = rdi;
static MOZ_CONSTEXPR_VAR Register CallTempReg2 = rbx;
static MOZ_CONSTEXPR_VAR Register CallTempReg3 = rcx;
static MOZ_CONSTEXPR_VAR Register CallTempReg4 = rsi;
static MOZ_CONSTEXPR_VAR Register CallTempReg5 = rdx;

// Different argument registers for WIN64
#if defined(_WIN64)
static MOZ_CONSTEXPR_VAR Register IntArgReg0 = rcx;
static MOZ_CONSTEXPR_VAR Register IntArgReg1 = rdx;
static MOZ_CONSTEXPR_VAR Register IntArgReg2 = r8;
static MOZ_CONSTEXPR_VAR Register IntArgReg3 = r9;
static MOZ_CONSTEXPR_VAR uint32_t NumIntArgRegs = 4;
static MOZ_CONSTEXPR_VAR Register IntArgRegs[NumIntArgRegs] = { rcx, rdx, r8, r9 };

static MOZ_CONSTEXPR_VAR Register CallTempNonArgRegs[] = { rax, rdi, rbx, rsi };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg0 = xmm0;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg1 = xmm1;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg2 = xmm2;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg3 = xmm3;
static const uint32_t NumFloatArgRegs = 4;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgRegs[NumFloatArgRegs] = { xmm0, xmm1, xmm2, xmm3 };
#else
static MOZ_CONSTEXPR_VAR Register IntArgReg0 = rdi;
static MOZ_CONSTEXPR_VAR Register IntArgReg1 = rsi;
static MOZ_CONSTEXPR_VAR Register IntArgReg2 = rdx;
static MOZ_CONSTEXPR_VAR Register IntArgReg3 = rcx;
static MOZ_CONSTEXPR_VAR Register IntArgReg4 = r8;
static MOZ_CONSTEXPR_VAR Register IntArgReg5 = r9;
static MOZ_CONSTEXPR_VAR uint32_t NumIntArgRegs = 6;
static MOZ_CONSTEXPR_VAR Register IntArgRegs[NumIntArgRegs] = { rdi, rsi, rdx, rcx, r8, r9 };

static MOZ_CONSTEXPR_VAR Register CallTempNonArgRegs[] = { rax, rbx };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg0 = xmm0;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg1 = xmm1;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg2 = xmm2;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg3 = xmm3;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg4 = xmm4;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg5 = xmm5;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg6 = xmm6;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgReg7 = xmm7;
static MOZ_CONSTEXPR_VAR uint32_t NumFloatArgRegs = 8;
static MOZ_CONSTEXPR_VAR FloatRegister FloatArgRegs[NumFloatArgRegs] = { xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7 };
#endif

class ABIArgGenerator
{
#if defined(XP_WIN)
    unsigned regIndex_;
#else
    unsigned intRegIndex_;
    unsigned floatRegIndex_;
#endif
    uint32_t stackOffset_;
    ABIArg current_;

  public:
    ABIArgGenerator();
    ABIArg next(MIRType argType);
    ABIArg &current() { return current_; }
    uint32_t stackBytesConsumedSoFar() const { return stackOffset_; }

    // Note: these registers are all guaranteed to be different
    static const Register NonArgReturnVolatileReg0;
    static const Register NonArgReturnVolatileReg1;
    static const Register NonVolatileReg;
};

static MOZ_CONSTEXPR_VAR Register OsrFrameReg = IntArgReg3;

static MOZ_CONSTEXPR_VAR Register PreBarrierReg = rdx;

// GCC stack is aligned on 16 bytes, but we don't maintain the invariant in
// jitted code.
static const uint32_t StackAlignment = 16;
static const bool StackKeptAligned = false;
static const uint32_t CodeAlignment = 8;
static const uint32_t NativeFrameSize = sizeof(void*);
static const uint32_t AlignmentAtPrologue = sizeof(void*);
static const uint32_t AlignmentMidPrologue = AlignmentAtPrologue;

static const Scale ScalePointer = TimesEight;

} // namespace jit
} // namespace js

#include "jit/shared/Assembler-x86-shared.h"

namespace js {
namespace jit {

// Return operand from a JS -> JS call.
static MOZ_CONSTEXPR_VAR ValueOperand JSReturnOperand = ValueOperand(JSReturnReg);

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
    // Each entry in this table is a jmp [rip], followed by a ud2 to hint to the
    // hardware branch predictor that there is no fallthrough, followed by the
    // eight bytes containing an immediate address. This comes out to 16 bytes.
    //    +1 byte for opcode
    //    +1 byte for mod r/m
    //    +4 bytes for rip-relative offset (2)
    //    +2 bytes for ud2 instruction
    //    +8 bytes for 64-bit address
    //
    static const uint32_t SizeOfExtendedJump = 1 + 1 + 4 + 2 + 8;
    static const uint32_t SizeOfJumpTableEntry = 16;

    uint32_t extendedJumpTable_;

    static IonCode *CodeFromJump(IonCode *code, uint8_t *jump);

  private:
    void writeRelocation(JmpSrc src, Relocation::Kind reloc);
    void addPendingJump(JmpSrc src, ImmPtr target, Relocation::Kind reloc);

  protected:
    size_t addPatchableJump(JmpSrc src, Relocation::Kind reloc);

  public:
    using AssemblerX86Shared::j;
    using AssemblerX86Shared::jmp;
    using AssemblerX86Shared::push;
    using AssemblerX86Shared::pop;

    static uint8_t *PatchableJumpAddress(IonCode *code, size_t index);
    static void PatchJumpEntry(uint8_t *entry, uint8_t *target);

    Assembler()
      : extendedJumpTable_(0)
    {
    }

    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void finish();

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
    void push(const ImmPtr &imm) {
        push(ImmWord(uintptr_t(imm.value)));
    }
    void push(const FloatRegister &src) {
        subq(Imm32(sizeof(double)), StackPointer);
        movsd(src, Address(StackPointer, 0));
    }
    CodeOffsetLabel pushWithPatch(const ImmWord &word) {
        CodeOffsetLabel label = movWithPatch(word, ScratchReg);
        push(ScratchReg);
        return label;
    }

    void pop(const FloatRegister &src) {
        movsd(Address(StackPointer, 0), src);
        addq(Imm32(sizeof(double)), StackPointer);
    }

    CodeOffsetLabel movWithPatch(const ImmWord &word, const Register &dest) {
        masm.movq_i64r(word.value, dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movWithPatch(const ImmPtr &imm, const Register &dest) {
        return movWithPatch(ImmWord(uintptr_t(imm.value)), dest);
    }

    // Load an ImmWord value into a register. Note that this instruction will
    // attempt to optimize its immediate field size. When a full 64-bit
    // immediate is needed for a relocation, use movWithPatch.
    void movq(ImmWord word, const Register &dest) {
        // Load a 64-bit immediate into a register. If the value falls into
        // certain ranges, we can use specialized instructions which have
        // smaller encodings.
        if (word.value <= UINT32_MAX) {
            // movl has a 32-bit unsigned (effectively) immediate field.
            masm.movl_i32r((uint32_t)word.value, dest.code());
        } else if ((intptr_t)word.value >= INT32_MIN && (intptr_t)word.value <= INT32_MAX) {
            // movq has a 32-bit signed immediate field.
            masm.movq_i32r((int32_t)(intptr_t)word.value, dest.code());
        } else {
            // Otherwise use movabs.
            masm.movq_i64r(word.value, dest.code());
        }
    }
    void movq(ImmPtr imm, const Register &dest) {
        movq(ImmWord(uintptr_t(imm.value)), dest);
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
          case Operand::MEM_REG_DISP:
            masm.movq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::MEM_SCALE:
            masm.movq_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          case Operand::MEM_ADDRESS32:
            masm.movq_mr(src.address(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movq(const Register &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movq_rr(src.code(), dest.reg());
            break;
          case Operand::MEM_REG_DISP:
            masm.movq_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::MEM_SCALE:
            masm.movq_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          case Operand::MEM_ADDRESS32:
            masm.movq_rm(src.code(), dest.address());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movq(Imm32 imm32, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_i32r(imm32.value, dest.reg());
            break;
          case Operand::MEM_REG_DISP:
            masm.movq_i32m(imm32.value, dest.disp(), dest.base());
            break;
          case Operand::MEM_SCALE:
            masm.movq_i32m(imm32.value, dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          case Operand::MEM_ADDRESS32:
            masm.movq_i32m(imm32.value, dest.address());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movq(const Register &src, const FloatRegister &dest) {
        masm.movq_rr(src.code(), dest.code());
    }
    void movq(const FloatRegister &src, const Register &dest) {
        masm.movq_rr(src.code(), dest.code());
    }
    void movq(const Register &src, const Register &dest) {
        masm.movq_rr(src.code(), dest.code());
    }

    void xchgq(const Register &src, const Register &dest) {
        masm.xchgq_rr(src.code(), dest.code());
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
          case Operand::MEM_REG_DISP:
            masm.addq_im(imm.value, dest.disp(), dest.base());
            break;
          case Operand::MEM_ADDRESS32:
            masm.addq_im(imm.value, dest.address());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void addq(const Register &src, const Register &dest) {
        masm.addq_rr(src.code(), dest.code());
    }
    void addq(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.addq_rr(src.reg(), dest.code());
            break;
          case Operand::MEM_REG_DISP:
            masm.addq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::MEM_ADDRESS32:
            masm.addq_mr(src.address(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

    void subq(Imm32 imm, const Register &dest) {
        masm.subq_ir(imm.value, dest.code());
    }
    void subq(const Register &src, const Register &dest) {
        masm.subq_rr(src.code(), dest.code());
    }
    void subq(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.subq_rr(src.reg(), dest.code());
            break;
          case Operand::MEM_REG_DISP:
            masm.subq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::MEM_ADDRESS32:
            masm.subq_mr(src.address(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void shlq(Imm32 imm, const Register &dest) {
        masm.shlq_i8r(imm.value, dest.code());
    }
    void shrq(Imm32 imm, const Register &dest) {
        masm.shrq_i8r(imm.value, dest.code());
    }
    void sarq(Imm32 imm, const Register &dest) {
        masm.sarq_i8r(imm.value, dest.code());
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
          case Operand::MEM_REG_DISP:
            masm.orq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::MEM_ADDRESS32:
            masm.orq_mr(src.address(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void xorq(const Register &src, const Register &dest) {
        masm.xorq_rr(src.code(), dest.code());
    }
    void xorq(Imm32 imm, const Register &dest) {
        masm.xorq_ir(imm.value, dest.code());
    }

    void mov(ImmWord word, const Register &dest) {
        // Use xor for setting registers to zero, as it is specially optimized
        // for this purpose on modern hardware. Note that it does clobber FLAGS
        // though. Use xorl instead of xorq since they are functionally
        // equivalent (32-bit instructions zero-extend their results to 64 bits)
        // and xorl has a smaller encoding.
        if (word.value == 0)
            xorl(dest, dest);
        else
            movq(word, dest);
    }
    void mov(ImmPtr imm, const Register &dest) {
        movq(imm, dest);
    }
    void mov(AsmJSImmPtr imm, const Register &dest) {
        masm.movq_i64r(-1, dest.code());
        AsmJSAbsoluteLink link(masm.currentOffset(), imm.kind());
        enoughMemory_ &= asmJSAbsoluteLinks_.append(link);
    }
    void mov(const Operand &src, const Register &dest) {
        movq(src, dest);
    }
    void mov(const Register &src, const Operand &dest) {
        movq(src, dest);
    }
    void mov(const Imm32 &imm32, const Operand &dest) {
        movq(imm32, dest);
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
    void xchg(const Register &src, const Register &dest) {
        xchgq(src, dest);
    }
    void lea(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::MEM_REG_DISP:
            masm.leaq_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::MEM_SCALE:
            masm.leaq_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexepcted operand kind");
        }
    }

    CodeOffsetLabel loadRipRelativeInt32(const Register &dest) {
        return CodeOffsetLabel(masm.movl_ripr(dest.code()).offset());
    }
    CodeOffsetLabel loadRipRelativeInt64(const Register &dest) {
        return CodeOffsetLabel(masm.movq_ripr(dest.code()).offset());
    }
    CodeOffsetLabel loadRipRelativeDouble(const FloatRegister &dest) {
        return CodeOffsetLabel(masm.movsd_ripr(dest.code()).offset());
    }
    CodeOffsetLabel storeRipRelativeInt32(const Register &dest) {
        return CodeOffsetLabel(masm.movl_rrip(dest.code()).offset());
    }
    CodeOffsetLabel storeRipRelativeDouble(const FloatRegister &dest) {
        return CodeOffsetLabel(masm.movsd_rrip(dest.code()).offset());
    }
    CodeOffsetLabel leaRipRelative(const Register &dest) {
        return CodeOffsetLabel(masm.leaq_rip(dest.code()).offset());
    }

    // The below cmpq methods switch the lhs and rhs when it invokes the
    // macroassembler to conform with intel standard.  When calling this
    // function put the left operand on the left as you would expect.
    void cmpq(const Operand &lhs, const Register &rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.cmpq_rr(rhs.code(), lhs.reg());
            break;
          case Operand::MEM_REG_DISP:
            masm.cmpq_rm(rhs.code(), lhs.disp(), lhs.base());
            break;
          case Operand::MEM_ADDRESS32:
            masm.cmpq_rm(rhs.code(), lhs.address());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpq(const Operand &lhs, Imm32 rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.cmpq_ir(rhs.value, lhs.reg());
            break;
          case Operand::MEM_REG_DISP:
            masm.cmpq_im(rhs.value, lhs.disp(), lhs.base());
            break;
          case Operand::MEM_ADDRESS32:
            masm.cmpq_im(rhs.value, lhs.address());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpq(const Register &lhs, const Operand &rhs) {
        switch (rhs.kind()) {
          case Operand::REG:
            masm.cmpq_rr(rhs.reg(), lhs.code());
            break;
          case Operand::MEM_REG_DISP:
            masm.cmpq_mr(rhs.disp(), rhs.base(), lhs.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpq(const Register &lhs, const Register &rhs) {
        masm.cmpq_rr(rhs.code(), lhs.code());
    }
    void cmpq(const Register &lhs, Imm32 rhs) {
        masm.cmpq_ir(rhs.value, lhs.code());
    }

    void testq(const Register &lhs, Imm32 rhs) {
        masm.testq_i32r(rhs.value, lhs.code());
    }
    void testq(const Register &lhs, const Register &rhs) {
        masm.testq_rr(rhs.code(), lhs.code());
    }
    void testq(const Operand &lhs, Imm32 rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.testq_i32r(rhs.value, lhs.reg());
            break;
          case Operand::MEM_REG_DISP:
            masm.testq_i32m(rhs.value, lhs.disp(), lhs.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
            break;
        }
    }

    void jmp(ImmPtr target, Relocation::Kind reloc = Relocation::HARDCODED) {
        JmpSrc src = masm.jmp();
        addPendingJump(src, target, reloc);
    }
    void j(Condition cond, ImmPtr target,
           Relocation::Kind reloc = Relocation::HARDCODED) {
        JmpSrc src = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
        addPendingJump(src, target, reloc);
    }

    void jmp(IonCode *target) {
        jmp(ImmPtr(target->raw()), Relocation::IONCODE);
    }
    void j(Condition cond, IonCode *target) {
        j(cond, ImmPtr(target->raw()), Relocation::IONCODE);
    }
    void call(IonCode *target) {
        JmpSrc src = masm.call();
        addPendingJump(src, ImmPtr(target->raw()), Relocation::IONCODE);
    }

    // Emit a CALL or CMP (nop) instruction. ToggleCall can be used to patch
    // this instruction.
    CodeOffsetLabel toggledCall(IonCode *target, bool enabled) {
        CodeOffsetLabel offset(size());
        JmpSrc src = enabled ? masm.call() : masm.cmp_eax();
        addPendingJump(src, ImmPtr(target->raw()), Relocation::IONCODE);
        JS_ASSERT(size() - offset.offset() == ToggledCallSize());
        return offset;
    }

    static size_t ToggledCallSize() {
        // Size of a call instruction.
        return 5;
    }

    // Do not mask shared implementations.
    using AssemblerX86Shared::call;

    void cvttsd2sq(const FloatRegister &src, const Register &dest) {
        masm.cvttsd2sq_rr(src.code(), dest.code());
    }
    void cvttss2sq(const FloatRegister &src, const Register &dest) {
        masm.cvttss2sq_rr(src.code(), dest.code());
    }
    void cvtsq2sd(const Register &src, const FloatRegister &dest) {
        masm.cvtsq2sd_rr(src.code(), dest.code());
    }
    void cvtsq2ss(const Register &src, const FloatRegister &dest) {
        masm.cvtsq2ss_rr(src.code(), dest.code());
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

} // namespace jit
} // namespace js

#endif /* jit_x64_Assembler_x64_h */
