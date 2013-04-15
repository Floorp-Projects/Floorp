/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_macro_assembler_x86_shared_h__
#define jsion_macro_assembler_x86_shared_h__

#include "mozilla/DebugOnly.h"

#ifdef JS_CPU_X86
# include "ion/x86/Assembler-x86.h"
#elif JS_CPU_X64
# include "ion/x64/Assembler-x64.h"
#endif
#include "ion/IonFrames.h"
#include "jsopcode.h"

#include "ion/IonCaches.h"

namespace js {
namespace ion {

class MacroAssemblerX86Shared : public Assembler
{
  protected:
    // Bytes pushed onto the frame by the callee; includes frameDepth_. This is
    // needed to compute offsets to stack slots while temporary space has been
    // reserved for unexpected spills or C++ function calls. It is maintained
    // by functions which track stack alignment, which for clear distinction
    // use StudlyCaps (for example, Push, Pop).
    uint32_t framePushed_;

  public:
    MacroAssemblerX86Shared()
      : framePushed_(0)
    { }

    void compareDouble(DoubleCondition cond, const FloatRegister &lhs, const FloatRegister &rhs) {
        if (cond & DoubleConditionBitInvert)
            ucomisd(rhs, lhs);
        else
            ucomisd(lhs, rhs);
    }
    void branchDouble(DoubleCondition cond, const FloatRegister &lhs,
                      const FloatRegister &rhs, Label *label)
    {
        compareDouble(cond, lhs, rhs);

        if (cond == DoubleEqual) {
            Label unordered;
            j(Parity, &unordered);
            j(Equal, label);
            bind(&unordered);
            return;
        }
        if (cond == DoubleNotEqualOrUnordered) {
            j(NotEqual, label);
            j(Parity, label);
            return;
        }

        JS_ASSERT(!(cond & DoubleConditionBitSpecial));
        j(ConditionFromDoubleCondition(cond), label);
    }

    void move32(const Imm32 &imm, const Register &dest) {
        if (imm.value == 0)
            xorl(dest, dest);
        else
            movl(imm, dest);
    }
    void move32(const Imm32 &imm, const Operand &dest) {
        movl(imm, dest);
    }
    void and32(const Imm32 &imm, const Register &dest) {
        andl(imm, dest);
    }
    void and32(const Imm32 &imm, const Address &dest) {
        andl(imm, Operand(dest));
    }
    void or32(const Imm32 &imm, const Register &dest) {
        orl(imm, dest);
    }
    void or32(const Imm32 &imm, const Address &dest) {
        orl(imm, Operand(dest));
    }
    void neg32(const Register &reg) {
        negl(reg);
    }
    void cmp32(const Register &lhs, const Imm32 &rhs) {
        cmpl(lhs, rhs);
    }
    void test32(const Register &lhs, const Register &rhs) {
        testl(lhs, rhs);
    }
    void test32(const Address &addr, Imm32 imm) {
        testl(Operand(addr), imm);
    }
    void cmp32(Register a, Register b) {
        cmpl(a, b);
    }
    void cmp32(const Operand &lhs, const Imm32 &rhs) {
        cmpl(lhs, rhs);
    }
    void cmp32(const Operand &lhs, const Register &rhs) {
        cmpl(lhs, rhs);
    }
    void add32(Register src, Register dest) {
        addl(src, dest);
    }
    void add32(Imm32 imm, Register dest) {
        addl(imm, dest);
    }
    void add32(Imm32 imm, const Address &dest) {
        addl(imm, Operand(dest));
    }
    void sub32(Imm32 imm, Register dest) {
        subl(imm, dest);
    }
    void sub32(Register src, Register dest) {
        subl(src, dest);
    }
    void xor32(Imm32 imm, Register dest) {
        xorl(imm, dest);
    }

    void branch32(Condition cond, const Operand &lhs, const Register &rhs, Label *label) {
        cmpl(lhs, rhs);
        j(cond, label);
    }
    void branch32(Condition cond, const Operand &lhs, Imm32 rhs, Label *label) {
        cmpl(lhs, rhs);
        j(cond, label);
    }
    void branch32(Condition cond, const Address &lhs, const Register &rhs, Label *label) {
        cmpl(Operand(lhs), rhs);
        j(cond, label);
    }
    void branch32(Condition cond, const Address &lhs, Imm32 imm, Label *label) {
        cmpl(Operand(lhs), imm);
        j(cond, label);
    }
    void branch32(Condition cond, const Register &lhs, Imm32 imm, Label *label) {
        cmpl(lhs, imm);
        j(cond, label);
    }
    void branch32(Condition cond, const Register &lhs, const Register &rhs, Label *label) {
        cmpl(lhs, rhs);
        j(cond, label);
    }
    void branchTest32(Condition cond, const Register &lhs, const Register &rhs, Label *label) {
        testl(lhs, rhs);
        j(cond, label);
    }
    void branchTest32(Condition cond, const Register &lhs, Imm32 imm, Label *label) {
        testl(lhs, imm);
        j(cond, label);
    }
    void branchTest32(Condition cond, const Address &address, Imm32 imm, Label *label) {
        testl(Operand(address), imm);
        j(cond, label);
    }

    // The following functions are exposed for use in platform-shared code.
    template <typename T>
    void Push(const T &t) {
        push(t);
        framePushed_ += STACK_SLOT_SIZE;
    }
    void Push(const FloatRegister &t) {
        push(t);
        framePushed_ += sizeof(double);
    }
    CodeOffsetLabel PushWithPatch(const ImmWord &word) {
        framePushed_ += sizeof(word.value);
        return pushWithPatch(word);
    }

    void Pop(const Register &reg) {
        pop(reg);
        framePushed_ -= STACK_SLOT_SIZE;
    }
    void implicitPop(uint32_t args) {
        JS_ASSERT(args % STACK_SLOT_SIZE == 0);
        framePushed_ -= args;
    }
    uint32_t framePushed() const {
        return framePushed_;
    }
    void setFramePushed(uint32_t framePushed) {
        framePushed_ = framePushed;
    }

    void jump(Label *label) {
        jmp(label);
    }
    void jump(RepatchLabel *label) {
        jmp(label);
    }
    void jump(Register reg) {
        jmp(Operand(reg));
    }

    void convertInt32ToDouble(const Register &src, const FloatRegister &dest) {
        cvtsi2sd(Operand(src), dest);
    }
    void convertInt32ToDouble(const Address &src, FloatRegister dest) {
        cvtsi2sd(Operand(src), dest);
    }
    Condition testDoubleTruthy(bool truthy, const FloatRegister &reg) {
        xorpd(ScratchFloatReg, ScratchFloatReg);
        ucomisd(ScratchFloatReg, reg);
        return truthy ? NonZero : Zero;
    }
    void branchTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail) {
        JS_STATIC_ASSERT(INT_MIN == int(0x80000000));
        cvttsd2si(src, dest);
        cmpl(dest, Imm32(INT_MIN));
        j(Assembler::Equal, fail);
    }
    void load8ZeroExtend(const Address &src, const Register &dest) {
        movzbl(Operand(src), dest);
    }
    void load8ZeroExtend(const BaseIndex &src, const Register &dest) {
        movzbl(Operand(src), dest);
    }
    void load8SignExtend(const Address &src, const Register &dest) {
        movxbl(Operand(src), dest);
    }
    void load8SignExtend(const BaseIndex &src, const Register &dest) {
        movxbl(Operand(src), dest);
    }
    template <typename S, typename T>
    void store8(const S &src, const T &dest) {
        movb(src, Operand(dest));
    }
    void load16ZeroExtend(const Address &src, const Register &dest) {
        movzwl(Operand(src), dest);
    }
    void load16ZeroExtend(const BaseIndex &src, const Register &dest) {
        movzwl(Operand(src), dest);
    }
    template <typename S, typename T>
    void store16(const S &src, const T &dest) {
        movw(src, Operand(dest));
    }
    void load16SignExtend(const Address &src, const Register &dest) {
        movxwl(Operand(src), dest);
    }
    void load16SignExtend(const BaseIndex &src, const Register &dest) {
        movxwl(Operand(src), dest);
    }
    void load32(const Address &address, Register dest) {
        movl(Operand(address), dest);
    }
    void load32(const BaseIndex &src, Register dest) {
        movl(Operand(src), dest);
    }
    void load32(const Operand &src, Register dest) {
        movl(src, dest);
    }
    template <typename S, typename T>
    void store32(const S &src, const T &dest) {
        movl(src, Operand(dest));
    }
    void loadDouble(const Address &src, FloatRegister dest) {
        movsd(Operand(src), dest);
    }
    void loadDouble(const BaseIndex &src, FloatRegister dest) {
        movsd(Operand(src), dest);
    }
    void loadDouble(const Operand &src, FloatRegister dest) {
        movsd(src, dest);
    }
    void storeDouble(FloatRegister src, const Address &dest) {
        movsd(src, Operand(dest));
    }
    void storeDouble(FloatRegister src, const BaseIndex &dest) {
        movsd(src, Operand(dest));
    }
    void storeDouble(FloatRegister src, const Operand &dest) {
        movsd(src, dest);
    }
    void zeroDouble(FloatRegister reg) {
        xorpd(reg, reg);
    }
    void negateDouble(FloatRegister reg) {
        // From MacroAssemblerX86Shared::maybeInlineDouble
        pcmpeqw(ScratchFloatReg, ScratchFloatReg);
        psllq(Imm32(63), ScratchFloatReg);

        // XOR the float in a float register with -0.0.
        xorpd(ScratchFloatReg, reg); // s ^ 0x80000000000000
    }
    void addDouble(FloatRegister src, FloatRegister dest) {
        addsd(src, dest);
    }
    void subDouble(FloatRegister src, FloatRegister dest) {
        subsd(src, dest);
    }
    void mulDouble(FloatRegister src, FloatRegister dest) {
        mulsd(src, dest);
    }
    void divDouble(FloatRegister src, FloatRegister dest) {
        divsd(src, dest);
    }
    void convertDoubleToFloat(const FloatRegister &src, const FloatRegister &dest) {
        cvtsd2ss(src, dest);
    }
    void loadFloatAsDouble(const Register &src, FloatRegister dest) {
        movd(src, dest);
        cvtss2sd(dest, dest);
    }
    void loadFloatAsDouble(const Address &src, FloatRegister dest) {
        movss(Operand(src), dest);
        cvtss2sd(dest, dest);
    }
    void loadFloatAsDouble(const BaseIndex &src, FloatRegister dest) {
        movss(Operand(src), dest);
        cvtss2sd(dest, dest);
    }
    void loadFloatAsDouble(const Operand &src, FloatRegister dest) {
        movss(src, dest);
        cvtss2sd(dest, dest);
    }
    void storeFloat(FloatRegister src, const Address &dest) {
        movss(src, Operand(dest));
    }
    void storeFloat(FloatRegister src, const BaseIndex &dest) {
        movss(src, Operand(dest));
    }

    // Checks whether a double is representable as a 32-bit integer. If so, the
    // integer is written to the output register. Otherwise, a bailout is taken to
    // the given snapshot. This function overwrites the scratch float register.
    void convertDoubleToInt32(FloatRegister src, Register dest, Label *fail,
                              bool negativeZeroCheck = true)
    {
        cvttsd2si(src, dest);
        cvtsi2sd(dest, ScratchFloatReg);
        ucomisd(src, ScratchFloatReg);
        j(Assembler::Parity, fail);
        j(Assembler::NotEqual, fail);

        // Check for -0
        if (negativeZeroCheck) {
            Label notZero;
            testl(dest, dest);
            j(Assembler::NonZero, &notZero);

            if (Assembler::HasSSE41()) {
                ptest(src, src);
                j(Assembler::NonZero, fail);
            } else {
                // bit 0 = sign of low double
                // bit 1 = sign of high double
                movmskpd(src, dest);
                andl(Imm32(1), dest);
                j(Assembler::NonZero, fail);
            }

            bind(&notZero);
        }
    }

    void clampIntToUint8(Register src, Register dest) {
        Label inRange, done;
        branchTest32(Assembler::Zero, src, Imm32(0xffffff00), &inRange);
        {
            Label negative;
            branchTest32(Assembler::Signed, src, src, &negative);
            {
                movl(Imm32(255), dest);
                jump(&done);
            }
            bind(&negative);
            {
                xorl(dest, dest);
                jump(&done);
            }
        }
        bind(&inRange);
        if (src != dest)
            movl(src, dest);
        bind(&done);
    }

    bool maybeInlineDouble(uint64_t u, const FloatRegister &dest) {
        // This implements parts of "13.4 Generating constants" of
        // "2. Optimizing subroutines in assembly language" by Agner Fog.
        switch (u) {
          case 0x0000000000000000ULL: // 0.0
            xorpd(dest, dest);
            break;
          case 0x8000000000000000ULL: // -0.0
            pcmpeqw(dest, dest);
            psllq(Imm32(63), dest);
            break;
          case 0x3fe0000000000000ULL: // 0.5
            pcmpeqw(dest, dest);
            psllq(Imm32(55), dest);
            psrlq(Imm32(2), dest);
            break;
          case 0x3ff0000000000000ULL: // 1.0
            pcmpeqw(dest, dest);
            psllq(Imm32(54), dest);
            psrlq(Imm32(2), dest);
            break;
          case 0x3ff8000000000000ULL: // 1.5
            pcmpeqw(dest, dest);
            psllq(Imm32(53), dest);
            psrlq(Imm32(2), dest);
            break;
          case 0x4000000000000000ULL: // 2.0
            pcmpeqw(dest, dest);
            psllq(Imm32(63), dest);
            psrlq(Imm32(1), dest);
            break;
          case 0xc000000000000000ULL: // -2.0
            pcmpeqw(dest, dest);
            psllq(Imm32(62), dest);
            break;
          default:
            return false;
        }
        return true;
    }

    void emitSet(Assembler::Condition cond, const Register &dest,
                 Assembler::NaNCond ifNaN = Assembler::NaN_Unexpected) {
        if (GeneralRegisterSet(Registers::SingleByteRegs).has(dest)) {
            // If the register we're defining is a single byte register,
            // take advantage of the setCC instruction
            setCC(cond, dest);
            movzxbl(dest, dest);

            if (ifNaN != Assembler::NaN_Unexpected) {
                Label noNaN;
                j(Assembler::NoParity, &noNaN);
                if (ifNaN == Assembler::NaN_IsTrue)
                    movl(Imm32(1), dest);
                else
                    xorl(dest, dest);
                bind(&noNaN);
            }
        } else {
            Label end;
            Label ifFalse;

            if (ifNaN == Assembler::NaN_IsFalse)
                j(Assembler::Parity, &ifFalse);
            movl(Imm32(1), dest);
            j(cond, &end);
            if (ifNaN == Assembler::NaN_IsTrue)
                j(Assembler::Parity, &end);
            bind(&ifFalse);
            xorl(dest, dest);

            bind(&end);
        }
    }

    // Emit a JMP that can be toggled to a CMP. See ToggleToJmp(), ToggleToCmp().
    CodeOffsetLabel toggledJump(Label *label) {
        CodeOffsetLabel offset(size());
        jump(label);
        return offset;
    }

    template <typename T>
    void computeEffectiveAddress(const T &address, Register dest) {
        lea(Operand(address), dest);
    }

    // Builds an exit frame on the stack, with a return address to an internal
    // non-function. Returns offset to be passed to markSafepointAt().
    bool buildFakeExitFrame(const Register &scratch, uint32_t *offset) {
        mozilla::DebugOnly<uint32_t> initialDepth = framePushed();

        CodeLabel cl;
        mov(cl.dest(), scratch);

        uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);
        Push(Imm32(descriptor));
        Push(scratch);

        bind(cl.src());
        *offset = currentOffset();

        JS_ASSERT(framePushed() == initialDepth + IonExitFrameLayout::Size());
        return addCodeLabel(cl);
    }

    bool buildOOLFakeExitFrame(void *fakeReturnAddr) {
        uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);
        Push(Imm32(descriptor));
        Push(ImmWord(fakeReturnAddr));
        return true;
    }

    void callWithExitFrame(IonCode *target) {
        uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);
        Push(Imm32(descriptor));
        call(target);
    }
    void callIon(const Register &callee) {
        call(callee);
    }

    void checkStackAlignment() {
        // Exists for ARM compatibility.
    }

    CodeOffsetLabel labelForPatch() {
        return CodeOffsetLabel(size());
    }
    
    void abiret() {
        ret();
    }
};

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_x86_shared_h__

