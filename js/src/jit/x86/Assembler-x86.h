/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_Assembler_x86_h
#define jit_x86_Assembler_x86_h

#include "mozilla/Util.h"

#include "assembler/assembler/X86Assembler.h"
#include "jit/CompactBuffer.h"
#include "jit/IonCode.h"
#include "jit/shared/Assembler-shared.h"

namespace js {
namespace jit {

static MOZ_CONSTEXPR_VAR Register eax = { JSC::X86Registers::eax };
static MOZ_CONSTEXPR_VAR Register ecx = { JSC::X86Registers::ecx };
static MOZ_CONSTEXPR_VAR Register edx = { JSC::X86Registers::edx };
static MOZ_CONSTEXPR_VAR Register ebx = { JSC::X86Registers::ebx };
static MOZ_CONSTEXPR_VAR Register esp = { JSC::X86Registers::esp };
static MOZ_CONSTEXPR_VAR Register ebp = { JSC::X86Registers::ebp };
static MOZ_CONSTEXPR_VAR Register esi = { JSC::X86Registers::esi };
static MOZ_CONSTEXPR_VAR Register edi = { JSC::X86Registers::edi };

static MOZ_CONSTEXPR_VAR FloatRegister xmm0 = { JSC::X86Registers::xmm0 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm1 = { JSC::X86Registers::xmm1 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm2 = { JSC::X86Registers::xmm2 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm3 = { JSC::X86Registers::xmm3 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm4 = { JSC::X86Registers::xmm4 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm5 = { JSC::X86Registers::xmm5 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm6 = { JSC::X86Registers::xmm6 };
static MOZ_CONSTEXPR_VAR FloatRegister xmm7 = { JSC::X86Registers::xmm7 };

static MOZ_CONSTEXPR_VAR Register InvalidReg = { JSC::X86Registers::invalid_reg };
static MOZ_CONSTEXPR_VAR FloatRegister InvalidFloatReg = { JSC::X86Registers::invalid_xmm };

static MOZ_CONSTEXPR_VAR Register JSReturnReg_Type = ecx;
static MOZ_CONSTEXPR_VAR Register JSReturnReg_Data = edx;
static MOZ_CONSTEXPR_VAR Register StackPointer = esp;
static MOZ_CONSTEXPR_VAR Register FramePointer = ebp;
static MOZ_CONSTEXPR_VAR Register ReturnReg = eax;
static MOZ_CONSTEXPR_VAR FloatRegister ReturnFloatReg = xmm0;
static MOZ_CONSTEXPR_VAR FloatRegister ScratchFloatReg = xmm7;

static MOZ_CONSTEXPR_VAR Register ArgumentsRectifierReg = esi;
static MOZ_CONSTEXPR_VAR Register CallTempReg0 = edi;
static MOZ_CONSTEXPR_VAR Register CallTempReg1 = eax;
static MOZ_CONSTEXPR_VAR Register CallTempReg2 = ebx;
static MOZ_CONSTEXPR_VAR Register CallTempReg3 = ecx;
static MOZ_CONSTEXPR_VAR Register CallTempReg4 = esi;
static MOZ_CONSTEXPR_VAR Register CallTempReg5 = edx;
static MOZ_CONSTEXPR_VAR Register CallTempReg6 = ebp;

// We have no arg regs, so our NonArgRegs are just our CallTempReg*
static MOZ_CONSTEXPR_VAR Register CallTempNonArgRegs[] = { edi, eax, ebx, ecx, esi, edx };
static const uint32_t NumCallTempNonArgRegs =
    mozilla::ArrayLength(CallTempNonArgRegs);

class ABIArgGenerator
{
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

static MOZ_CONSTEXPR_VAR Register OsrFrameReg = edx;
static MOZ_CONSTEXPR_VAR Register PreBarrierReg = edx;

// GCC stack is aligned on 16 bytes, but we don't maintain the invariant in
// jitted code.
#if defined(__GNUC__)
static const uint32_t StackAlignment = 16;
#else
static const uint32_t StackAlignment = 4;
#endif
static const bool StackKeptAligned = false;
static const uint32_t CodeAlignment = 8;
static const uint32_t NativeFrameSize = sizeof(void*);
static const uint32_t AlignmentAtPrologue = sizeof(void*);
static const uint32_t AlignmentMidPrologue = AlignmentAtPrologue;
struct ImmTag : public Imm32
{
    ImmTag(JSValueTag mask)
      : Imm32(int32_t(mask))
    { }
};

struct ImmType : public ImmTag
{
    ImmType(JSValueType type)
      : ImmTag(JSVAL_TYPE_TO_TAG(type))
    { }
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
    int32_t index_ : 5;
    Scale scale_ : 3;
    int32_t base_;
    int32_t disp_;

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
        index_(address.index.code()),
        scale_(address.scale),
        base_(address.base.code()),
        disp_(address.offset)
    { }
    Operand(Register base, Register index, Scale scale, int32_t disp = 0)
      : kind_(SCALE),
        index_(index.code()),
        scale_(scale),
        base_(base.code()),
        disp_(disp)
    { }
    Operand(Register reg, int32_t disp)
      : kind_(REG_DISP),
        base_(reg.code()),
        disp_(disp)
    { }
    explicit Operand(const AbsoluteAddress &address)
      : kind_(ADDRESS),
        base_(reinterpret_cast<int32_t>(address.addr))
    { }
    explicit Operand(const void *address)
      : kind_(ADDRESS),
        base_(reinterpret_cast<int32_t>(address))
    { }

    Address toAddress() {
        JS_ASSERT(kind() == REG_DISP);
        return Address(Register::FromCode(base()), disp());
    }

    BaseIndex toBaseIndex() {
        JS_ASSERT(kind() == SCALE);
        return BaseIndex(Register::FromCode(base()), Register::FromCode(index()), scale(), disp());
    }

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
    int32_t disp() const {
        JS_ASSERT(kind() == REG_DISP || kind() == SCALE);
        return disp_;
    }
    void *address() const {
        JS_ASSERT(kind() == ADDRESS);
        return reinterpret_cast<void *>(base_);
    }
};

} // namespace jit
} // namespace js

#include "jit/shared/Assembler-x86-shared.h"

namespace js {
namespace jit {

static inline void
PatchJump(CodeLocationJump jump, CodeLocationLabel label)
{
#ifdef DEBUG
    // Assert that we're overwriting a jump instruction, either:
    //   0F 80+cc <imm32>, or
    //   E9 <imm32>
    unsigned char *x = (unsigned char *)jump.raw() - 5;
    JS_ASSERT(((*x >= 0x80 && *x <= 0x8F) && *(x - 1) == 0x0F) ||
              (*x == 0xE9));
#endif
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
    using AssemblerX86Shared::movss;
    using AssemblerX86Shared::retarget;
    using AssemblerX86Shared::cmpl;
    using AssemblerX86Shared::call;
    using AssemblerX86Shared::push;
    using AssemblerX86Shared::pop;

    static void TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8_t *buffer);

    // Actual assembly emitting functions.

    void push(const ImmGCPtr &ptr) {
        push(Imm32(ptr.value));
        writeDataRelocation(ptr);
    }
    void push(const ImmWord imm) {
        push(Imm32(imm.value));
    }
    void push(const FloatRegister &src) {
        subl(Imm32(sizeof(double)), StackPointer);
        movsd(src, Operand(StackPointer, 0));
    }

    CodeOffsetLabel pushWithPatch(const ImmWord &word) {
        push(Imm32(word.value));
        return masm.currentOffset();
    }

    void pop(const FloatRegister &src) {
        movsd(Operand(StackPointer, 0), src);
        addl(Imm32(sizeof(double)), StackPointer);
    }

    CodeOffsetLabel movWithPatch(const ImmWord &word, const Register &dest) {
        movl(Imm32(word.value), dest);
        return masm.currentOffset();
    }

    void movl(const ImmGCPtr &ptr, const Register &dest) {
        masm.movl_i32r(ptr.value, dest.code());
        writeDataRelocation(ptr);
    }
    void movl(const ImmGCPtr &ptr, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_i32r(ptr.value, dest.reg());
            writeDataRelocation(ptr);
            break;
          case Operand::REG_DISP:
            masm.movl_i32m(ptr.value, dest.disp(), dest.base());
            writeDataRelocation(ptr);
            break;
          case Operand::SCALE:
            masm.movl_i32m(ptr.value, dest.disp(), dest.base(), dest.index(), dest.scale());
            writeDataRelocation(ptr);
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
    void mov(Imm32 imm, const Operand &dest) {
        movl(imm, dest);
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
    void xchg(const Register &src, const Register &dest) {
        xchgl(src, dest);
    }
    void lea(const Operand &src, const Register &dest) {
        return leal(src, dest);
    }

    void cmpl(const Register src, ImmWord ptr) {
        masm.cmpl_ir(ptr.value, src.code());
    }
    void cmpl(const Register src, ImmGCPtr ptr) {
        masm.cmpl_ir(ptr.value, src.code());
        writeDataRelocation(ptr);
    }
    void cmpl(const Register &lhs, const Register &rhs) {
        masm.cmpl_rr(rhs.code(), lhs.code());
    }
    void cmpl(const Operand &op, ImmGCPtr imm) {
        switch (op.kind()) {
          case Operand::REG:
            masm.cmpl_ir_force32(imm.value, op.reg());
            writeDataRelocation(imm);
            break;
          case Operand::REG_DISP:
            masm.cmpl_im_force32(imm.value, op.disp(), op.base());
            writeDataRelocation(imm);
            break;
          case Operand::ADDRESS:
            masm.cmpl_im(imm.value, op.address());
            writeDataRelocation(imm);
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    CodeOffsetLabel cmplWithPatch(const Register &lhs, Imm32 rhs) {
        masm.cmpl_ir_force32(rhs.value, lhs.code());
        return masm.currentOffset();
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

    // Emit a CALL or CMP (nop) instruction. ToggleCall can be used to patch
    // this instruction.
    CodeOffsetLabel toggledCall(IonCode *target, bool enabled) {
        CodeOffsetLabel offset(size());
        JmpSrc src = enabled ? masm.call() : masm.cmp_eax();
        addPendingJump(src, target->raw(), Relocation::IONCODE);
        JS_ASSERT(size() - offset.offset() == ToggledCallSize());
        return offset;
    }

    static size_t ToggledCallSize() {
        // Size of a call instruction.
        return 5;
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
        JS_ASSERT(HasSSE2());
        masm.movsd_mr((const void *)dp, dest.code());
    }

    void movss(const float *dp, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.movss_mr((const void *)dp, dest.code());
    }

    // Move a 32-bit immediate into a register where the immediate can be
    // patched.
    CodeOffsetLabel movlWithPatch(Imm32 imm, Register dest) {
        masm.movl_i32r(imm.value, dest.code());
        return masm.currentOffset();
    }

    // Load from *addr where addr can be patched.
    CodeOffsetLabel movlWithPatch(void *addr, Register dest) {
        masm.movl_mr(addr, dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movsdWithPatch(void *addr, FloatRegister dest) {
        JS_ASSERT(HasSSE2());
        masm.movsd_mr(addr, dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movssWithPatch(void *addr, FloatRegister dest) {
        JS_ASSERT(HasSSE2());
        masm.movss_mr(addr, dest.code());
        return masm.currentOffset();
    }

    // Store to *addr where addr can be patched
    CodeOffsetLabel movlWithPatch(Register src, void *addr) {
        masm.movl_rm(src.code(), addr);
        return masm.currentOffset();
    }
    CodeOffsetLabel movsdWithPatch(FloatRegister dest, void *addr) {
        JS_ASSERT(HasSSE2());
        masm.movsd_rm(dest.code(), addr);
        return masm.currentOffset();
    }
    CodeOffsetLabel movssWithPatch(FloatRegister dest, void *addr) {
        JS_ASSERT(HasSSE2());
        masm.movss_rm(dest.code(), addr);
        return masm.currentOffset();
    }

    // Load from *(base + disp32) where disp32 can be patched.
    CodeOffsetLabel movsblWithPatch(Address src, Register dest) {
        masm.movsbl_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movzblWithPatch(Address src, Register dest) {
        masm.movzbl_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movswlWithPatch(Address src, Register dest) {
        masm.movswl_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movzwlWithPatch(Address src, Register dest) {
        masm.movzwl_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movlWithPatch(Address src, Register dest) {
        masm.movl_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movssWithPatch(Address src, FloatRegister dest) {
        JS_ASSERT(HasSSE2());
        masm.movss_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movsdWithPatch(Address src, FloatRegister dest) {
        JS_ASSERT(HasSSE2());
        masm.movsd_mr_disp32(src.offset, src.base.code(), dest.code());
        return masm.currentOffset();
    }

    // Store to *(base + disp32) where disp32 can be patched.
    CodeOffsetLabel movbWithPatch(Register src, Address dest) {
        masm.movb_rm_disp32(src.code(), dest.offset, dest.base.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movwWithPatch(Register src, Address dest) {
        masm.movw_rm_disp32(src.code(), dest.offset, dest.base.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movlWithPatch(Register src, Address dest) {
        masm.movl_rm_disp32(src.code(), dest.offset, dest.base.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movssWithPatch(FloatRegister src, Address dest) {
        JS_ASSERT(HasSSE2());
        masm.movss_rm_disp32(src.code(), dest.offset, dest.base.code());
        return masm.currentOffset();
    }
    CodeOffsetLabel movsdWithPatch(FloatRegister src, Address dest) {
        JS_ASSERT(HasSSE2());
        masm.movsd_rm_disp32(src.code(), dest.offset, dest.base.code());
        return masm.currentOffset();
    }

    // Load from *(addr + index*scale) where addr can be patched.
    CodeOffsetLabel movlWithPatch(void *addr, Register index, Scale scale, Register dest) {
        masm.movl_mr(addr, index.code(), scale, dest.code());
        return masm.currentOffset();
    }
};

// Get a register in which we plan to put a quantity that will be used as an
// integer argument.  This differs from GetIntArgReg in that if we have no more
// actual argument registers to use we will fall back on using whatever
// CallTempReg* don't overlap the argument registers, and only fail once those
// run out too.
static inline bool
GetTempRegForIntArg(uint32_t usedIntArgs, uint32_t usedFloatArgs, Register *out)
{
    if (usedIntArgs >= NumCallTempNonArgRegs)
        return false;
    *out = CallTempNonArgRegs[usedIntArgs];
    return true;
}

} // namespace jit
} // namespace js

#endif /* jit_x86_Assembler_x86_h */
