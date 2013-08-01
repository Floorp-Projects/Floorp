/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_shared_Assembler_x86_shared_h
#define ion_shared_Assembler_x86_shared_h

#include <cstddef>

#include "assembler/assembler/X86Assembler.h"
#include "ion/shared/Assembler-shared.h"

namespace js {
namespace ion {

class AssemblerX86Shared
{
  protected:
    struct RelativePatch {
        int32_t offset;
        void *target;
        Relocation::Kind kind;

        RelativePatch(int32_t offset, void *target, Relocation::Kind kind)
          : offset(offset),
            target(target),
            kind(kind)
        { }
    };

    Vector<CodeLabel, 0, SystemAllocPolicy> codeLabels_;
    Vector<RelativePatch, 8, SystemAllocPolicy> jumps_;
    CompactBufferWriter jumpRelocations_;
    CompactBufferWriter dataRelocations_;
    CompactBufferWriter preBarriers_;
    bool enoughMemory_;

    void writeDataRelocation(const Value &val) {
        if (val.isMarkable()) {
            JS_ASSERT(static_cast<gc::Cell*>(val.toGCThing())->isTenured());
            dataRelocations_.writeUnsigned(masm.currentOffset());
        }
    }
    void writeDataRelocation(const ImmGCPtr &ptr) {
        if (ptr.value)
            dataRelocations_.writeUnsigned(masm.currentOffset());
    }
    void writePrebarrierOffset(CodeOffsetLabel label) {
        preBarriers_.writeUnsigned(label.offset());
    }

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
        NotSigned = JSC::X86Assembler::ConditionNS,
        Zero = JSC::X86Assembler::ConditionE,
        NonZero = JSC::X86Assembler::ConditionNE,
        Parity = JSC::X86Assembler::ConditionP,
        NoParity = JSC::X86Assembler::ConditionNP
    };

    // If this bit is set, the ucomisd operands have to be inverted.
    static const int DoubleConditionBitInvert = 0x10;

    // Bit set when a DoubleCondition does not map to a single x86 condition.
    // The macro assembler has to special-case these conditions.
    static const int DoubleConditionBitSpecial = 0x20;
    static const int DoubleConditionBits = DoubleConditionBitInvert | DoubleConditionBitSpecial;

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleOrdered = NoParity,
        DoubleEqual = Equal | DoubleConditionBitSpecial,
        DoubleNotEqual = NotEqual,
        DoubleGreaterThan = Above,
        DoubleGreaterThanOrEqual = AboveOrEqual,
        DoubleLessThan = Above | DoubleConditionBitInvert,
        DoubleLessThanOrEqual = AboveOrEqual | DoubleConditionBitInvert,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleUnordered = Parity,
        DoubleEqualOrUnordered = Equal,
        DoubleNotEqualOrUnordered = NotEqual | DoubleConditionBitSpecial,
        DoubleGreaterThanOrUnordered = Below | DoubleConditionBitInvert,
        DoubleGreaterThanOrEqualOrUnordered = BelowOrEqual | DoubleConditionBitInvert,
        DoubleLessThanOrUnordered = Below,
        DoubleLessThanOrEqualOrUnordered = BelowOrEqual
    };

    enum NaNCond {
        NaN_HandledByCond,
        NaN_IsTrue,
        NaN_IsFalse
    };

    // If the primary condition returned by ConditionFromDoubleCondition doesn't
    // handle NaNs properly, return NaN_IsFalse if the comparison should be
    // overridden to return false on NaN, NaN_IsTrue if it should be overridden
    // to return true on NaN, or NaN_HandledByCond if no secondary check is
    // needed.
    static inline NaNCond NaNCondFromDoubleCondition(DoubleCondition cond) {
        switch (cond) {
          case DoubleOrdered:
          case DoubleNotEqual:
          case DoubleGreaterThan:
          case DoubleGreaterThanOrEqual:
          case DoubleLessThan:
          case DoubleLessThanOrEqual:
          case DoubleUnordered:
          case DoubleEqualOrUnordered:
          case DoubleGreaterThanOrUnordered:
          case DoubleGreaterThanOrEqualOrUnordered:
          case DoubleLessThanOrUnordered:
          case DoubleLessThanOrEqualOrUnordered:
            return NaN_HandledByCond;
          case DoubleEqual:
            return NaN_IsFalse;
          case DoubleNotEqualOrUnordered:
            return NaN_IsTrue;
        }

        MOZ_ASSUME_UNREACHABLE("Unknown double condition");
    }

    static void staticAsserts() {
        // DoubleConditionBits should not interfere with x86 condition codes.
        JS_STATIC_ASSERT(!((Equal | NotEqual | Above | AboveOrEqual | Below |
                            BelowOrEqual | Parity | NoParity) & DoubleConditionBits));
    }

    AssemblerX86Shared()
      : enoughMemory_(true)
    {
    }

    static Condition InvertCondition(Condition cond);

    // Return the primary condition to test. Some primary conditions may not
    // handle NaNs properly and may therefore require a secondary condition.
    // Use NaNCondFromDoubleCondition to determine what else is needed.
    static inline Condition ConditionFromDoubleCondition(DoubleCondition cond) {
        return static_cast<Condition>(cond & ~DoubleConditionBits);
    }

    static void TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // MacroAssemblers hold onto gcthings, so they are traced by the GC.
    void trace(JSTracer *trc);

    bool oom() const {
        return masm.oom() ||
               !enoughMemory_ ||
               jumpRelocations_.oom() ||
               dataRelocations_.oom() ||
               preBarriers_.oom();
    }

    void setPrinter(Sprinter *sp) {
        masm.setPrinter(sp);
    }

    void executableCopy(void *buffer);
    void processCodeLabels(uint8_t *rawCode);
    void copyJumpRelocationTable(uint8_t *dest);
    void copyDataRelocationTable(uint8_t *dest);
    void copyPreBarrierTable(uint8_t *dest);

    bool addCodeLabel(CodeLabel label) {
        return codeLabels_.append(label);
    }
    size_t numCodeLabels() const {
        return codeLabels_.length();
    }

    // Size of the instruction stream, in bytes.
    size_t size() const {
        return masm.size();
    }
    // Size of the jump relocation table, in bytes.
    size_t jumpRelocationTableBytes() const {
        return jumpRelocations_.length();
    }
    size_t dataRelocationTableBytes() const {
        return dataRelocations_.length();
    }
    size_t preBarrierTableBytes() const {
        return preBarriers_.length();
    }
    // Size of the data table, in bytes.
    size_t bytesNeeded() const {
        return size() +
               jumpRelocationTableBytes() +
               dataRelocationTableBytes() +
               preBarrierTableBytes();
    }

  public:
    void align(int alignment) {
        masm.align(alignment);
    }
    void writeCodePointer(AbsoluteLabel *label) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
        masm.jumpTablePointer(label->prev());
        label->setPrev(masm.size());
    }
    void writeDoubleConstant(double d, Label *label) {
        label->bind(masm.size());
        masm.doubleConstant(d);
    }
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
          case Operand::SCALE:
            masm.movl_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.movl_mr(src.address(), dest.code());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
          case Operand::SCALE:
            masm.movl_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.movl_rm(src.code(), dest.address());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movl(const Imm32 &imm32, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_i32r(imm32.value, dest.reg());
            break;
          case Operand::REG_DISP:
            masm.movl_i32m(imm32.value, dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movl_i32m(imm32.value, dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

    void xchgl(const Register &src, const Register &dest) {
        masm.xchgl_rr(src.code(), dest.code());
    }

    void movsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.movsd_rr(src.code(), dest.code());
    }
    void movsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movsd_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movsd_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movsd(const FloatRegister &src, const Operand &dest) {
        JS_ASSERT(HasSSE2());
        switch (dest.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.code(), dest.fpu());
            break;
          case Operand::REG_DISP:
            masm.movsd_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movsd_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movss(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movss_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movss_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movss(const FloatRegister &src, const Operand &dest) {
        JS_ASSERT(HasSSE2());
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movss_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movss_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movdqa(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movdqa_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movdqa_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movdqa(const FloatRegister &src, const Operand &dest) {
        JS_ASSERT(HasSSE2());
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movdqa_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movdqa_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cvtss2sd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.cvtss2sd_rr(src.code(), dest.code());
    }
    void cvtsd2ss(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.cvtsd2ss_rr(src.code(), dest.code());
    }
    void movzbl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movzbl_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movzbl_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movsbl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movsbl_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movsbl_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movb(const Register &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movb_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movb_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movb(const Imm32 &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movb_i8m(src.value, dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movb_i8m(src.value, dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movzwl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movzwl_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movzwl_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

    void movw(const Register &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movw_rm(src.code(), dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movw_rm(src.code(), dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movw(const Imm32 &src, const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.movw_i16m(src.value, dest.disp(), dest.base());
            break;
          case Operand::SCALE:
            masm.movw_i16m(src.value, dest.disp(), dest.base(), dest.index(), dest.scale());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void movswl(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.movswl_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.movswl_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void leal(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.leal_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.leal_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

  protected:
    JmpSrc jSrc(Condition cond, Label *label) {
        JmpSrc j = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(j, JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
        return j;
    }
    JmpSrc jmpSrc(Label *label) {
        JmpSrc j = masm.jmp();
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(j, JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
        return j;
    }

    // Comparison of EAX against the address given by a Label.
    JmpSrc cmpSrc(Label *label) {
        JmpSrc j = masm.cmp_eax();
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(j, JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
        return j;
    }

    JmpSrc jSrc(Condition cond, RepatchLabel *label) {
        JmpSrc j = masm.jCC(static_cast<JSC::X86Assembler::Condition>(cond));
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(j, JmpDst(label->offset()));
        } else {
            label->use(j.offset());
        }
        return j;
    }
    JmpSrc jmpSrc(RepatchLabel *label) {
        JmpSrc j = masm.jmp();
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(j, JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            label->use(j.offset());
        }
        return j;
    }

  public:
    void nop() { masm.nop(); }
    void j(Condition cond, Label *label) { jSrc(cond, label); }
    void jmp(Label *label) { jmpSrc(label); }
    void j(Condition cond, RepatchLabel *label) { jSrc(cond, label); }
    void jmp(RepatchLabel *label) { jmpSrc(label); }

    void jmp(const Operand &op){
        switch (op.kind()) {
          case Operand::REG_DISP:
            masm.jmp_m(op.disp(), op.base());
            break;
          case Operand::SCALE:
            masm.jmp_m(op.disp(), op.base(), op.index(), op.scale());
            break;
          case Operand::REG:
            masm.jmp_r(op.reg());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpEAX(Label *label) { cmpSrc(label); }
    void bind(Label *label) {
        JSC::MacroAssembler::Label jsclabel;
        JSC::X86Assembler::JmpDst dst(masm.label());
        if (label->used()) {
            bool more;
            JSC::X86Assembler::JmpSrc jmp(label->offset());
            do {
                JSC::X86Assembler::JmpSrc next;
                more = masm.nextJump(jmp, &next);
                masm.linkJump(jmp, dst);
                jmp = next;
            } while (more);
        }
        label->bind(dst.offset());
    }
    void bind(RepatchLabel *label) {
        JSC::MacroAssembler::Label jsclabel;
        JSC::X86Assembler::JmpDst dst(masm.label());
        if (label->used()) {
            JSC::X86Assembler::JmpSrc jmp(label->offset());
            masm.linkJump(jmp, dst);
        }
        label->bind(dst.offset());
    }
    uint32_t currentOffset() {
        return masm.label().offset();
    }

    // Re-routes pending jumps to a new label.
    void retarget(Label *label, Label *target) {
        JSC::MacroAssembler::Label jsclabel;
        if (label->used()) {
            bool more;
            JSC::X86Assembler::JmpSrc jmp(label->offset());
            do {
                JSC::X86Assembler::JmpSrc next;
                more = masm.nextJump(jmp, &next);

                if (target->bound()) {
                    // The jump can be immediately patched to the correct destination.
                    masm.linkJump(jmp, JmpDst(target->offset()));
                } else {
                    // Thread the jump list through the unpatched jump targets.
                    JmpSrc prev = JmpSrc(target->use(jmp.offset()));
                    masm.setNextJump(jmp, prev);
                }

                jmp = next;
            } while (more);
        }
        label->reset();
    }

    static void Bind(uint8_t *raw, AbsoluteLabel *label, const void *address) {
        if (label->used()) {
            intptr_t src = label->offset();
            do {
                intptr_t next = reinterpret_cast<intptr_t>(JSC::X86Assembler::getPointer(raw + src));
                JSC::X86Assembler::setPointer(raw + src, address);
                src = next;
            } while (src != AbsoluteLabel::INVALID_OFFSET);
        }
        label->bind();
    }

    void ret() {
        masm.ret();
    }
    void retn(Imm32 n) {
        // Remove the size of the return address which is included in the frame.
        masm.ret(n.value - sizeof(void *));
    }
    void call(Label *label) {
        if (label->bound()) {
            masm.linkJump(masm.call(), JmpDst(label->offset()));
        } else {
            JmpSrc j = masm.call();
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

    void breakpoint() {
        masm.int3();
    }

    static bool HasSSE2() {
        return JSC::MacroAssembler::getSSEState() >= JSC::MacroAssembler::HasSSE2;
    }
    static bool HasSSE3() {
        return JSC::MacroAssembler::getSSEState() >= JSC::MacroAssembler::HasSSE3;
    }
    static bool HasSSE41() {
        return JSC::MacroAssembler::getSSEState() >= JSC::MacroAssembler::HasSSE4_1;
    }

    // The below cmpl methods switch the lhs and rhs when it invokes the
    // macroassembler to conform with intel standard.  When calling this
    // function put the left operand on the left as you would expect.
    void cmpl(const Register &lhs, const Register &rhs) {
        masm.cmpl_rr(rhs.code(), lhs.code());
    }
    void cmpl(const Register &lhs, const Operand &rhs) {
        switch (rhs.kind()) {
          case Operand::REG:
            masm.cmpl_rr(rhs.reg(), lhs.code());
            break;
          case Operand::REG_DISP:
            masm.cmpl_mr(rhs.disp(), rhs.base(), lhs.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpl(const Register &src, Imm32 imm) {
        masm.cmpl_ir(imm.value, src.code());
    }
    void cmpl(const Operand &op, Imm32 imm) {
        switch (op.kind()) {
          case Operand::REG:
            masm.cmpl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.cmpl_im(imm.value, op.disp(), op.base());
            break;
          case Operand::SCALE:
            masm.cmpl_im(imm.value, op.disp(), op.base(), op.index(), op.scale());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.cmpl_im(imm.value, op.address());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpl(const Operand &lhs, const Register &rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.cmpl_rr(rhs.code(), lhs.reg());
            break;
          case Operand::REG_DISP:
            masm.cmpl_rm(rhs.code(), lhs.disp(), lhs.base());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.cmpl_rm(rhs.code(), lhs.address());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cmpl(const Operand &op, ImmWord imm) {
        switch (op.kind()) {
          case Operand::REG:
            masm.cmpl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.cmpl_im(imm.value, op.disp(), op.base());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.cmpl_im(imm.value, op.address());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void setCC(Condition cond, const Register &r) {
        masm.setCC_r(static_cast<JSC::X86Assembler::Condition>(cond), r.code());
    }
    void testb(const Register &lhs, const Register &rhs) {
        JS_ASSERT(GeneralRegisterSet(Registers::SingleByteRegs).has(lhs));
        JS_ASSERT(GeneralRegisterSet(Registers::SingleByteRegs).has(rhs));
        masm.testb_rr(rhs.code(), lhs.code());
    }
    void testl(const Register &lhs, const Register &rhs) {
        masm.testl_rr(rhs.code(), lhs.code());
    }
    void testl(const Register &lhs, Imm32 rhs) {
        masm.testl_i32r(rhs.value, lhs.code());
    }
    void testl(const Operand &lhs, Imm32 rhs) {
        switch (lhs.kind()) {
          case Operand::REG:
            masm.testl_i32r(rhs.value, lhs.reg());
            break;
          case Operand::REG_DISP:
            masm.testl_i32m(rhs.value, lhs.disp(), lhs.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
            break;
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
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.addl_im(imm.value, op.address());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void orl(const Register &reg, const Register &dest) {
        masm.orl_rr(reg.code(), dest.code());
    }
    void orl(Imm32 imm, const Register &reg) {
        masm.orl_ir(imm.value, reg.code());
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void xorl(const Register &src, const Register &dest) {
        masm.xorl_rr(src.code(), dest.code());
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void andl(const Register &src, const Register &dest) {
        masm.andl_rr(src.code(), dest.code());
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void imull(Imm32 imm, const Register &dest) {
        masm.imull_i32r(dest.code(), imm.value, dest.code());
    }
    void imull(const Register &src, const Register &dest) {
        masm.imull_rr(src.code(), dest.code());
    }
    void imull(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG:
            masm.imull_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.imull_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void negl(const Operand &src) {
        switch (src.kind()) {
          case Operand::REG:
            masm.negl_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.negl_m(src.disp(), src.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void negl(const Register &reg) {
        masm.negl_r(reg.code());
    }
    void notl(const Operand &src) {
        switch (src.kind()) {
          case Operand::REG:
            masm.notl_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.notl_m(src.disp(), src.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void notl(const Register &reg) {
        masm.notl_r(reg.code());
    }
    void shrl(const Imm32 imm, const Register &dest) {
        masm.shrl_i8r(imm.value, dest.code());
    }
    void shll(const Imm32 imm, const Register &dest) {
        masm.shll_i8r(imm.value, dest.code());
    }
    void sarl(const Imm32 imm, const Register &dest) {
        masm.sarl_i8r(imm.value, dest.code());
    }
    void shrl_cl(const Register &dest) {
        masm.shrl_CLr(dest.code());
    }
    void shll_cl(const Register &dest) {
        masm.shll_CLr(dest.code());
    }
    void sarl_cl(const Register &dest) {
        masm.sarl_CLr(dest.code());
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
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
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void pop(const Register &src) {
        masm.pop_r(src.code());
    }

    void pushFlags() {
        masm.push_flags();
    }
    void popFlags() {
        masm.pop_flags();
    }

#ifdef JS_CPU_X86
    void pushAllRegs() {
        masm.pusha();
    }
    void popAllRegs() {
        masm.popa();
    }
#endif

    // Zero-extend byte to 32-bit integer.
    void movzxbl(const Register &src, const Register &dest) {
        masm.movzbl_rr(src.code(), dest.code());
    }

    void cdq() {
        masm.cdq();
    }
    void idiv(Register divisor) {
        masm.idivl_r(divisor.code());
    }
    void udiv(Register divisor) {
        masm.divl_r(divisor.code());
    }

    void unpcklps(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.unpcklps_rr(src.code(), dest.code());
    }
    void pinsrd(const Register &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.pinsrd_rr(src.code(), dest.code());
    }
    void pinsrd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::REG:
            masm.pinsrd_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.pinsrd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void psrldq(Imm32 shift, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.psrldq_ir(shift.value, dest.code());
    }
    void psllq(Imm32 shift, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.psllq_ir(shift.value, dest.code());
    }
    void psrlq(Imm32 shift, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.psrlq_ir(shift.value, dest.code());
    }

    void cvtsi2sd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::REG:
            masm.cvtsi2sd_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.cvtsi2sd_mr(src.disp(), src.base(), dest.code());
            break;
          case Operand::SCALE:
            masm.cvtsi2sd_mr(src.disp(), src.base(), src.index(), src.scale(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void cvttsd2si(const FloatRegister &src, const Register &dest) {
        JS_ASSERT(HasSSE2());
        masm.cvttsd2si_rr(src.code(), dest.code());
    }
    void cvtsi2sd(const Register &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.cvtsi2sd_rr(src.code(), dest.code());
    }
    void movmskpd(const FloatRegister &src, const Register &dest) {
        JS_ASSERT(HasSSE2());
        masm.movmskpd_rr(src.code(), dest.code());
    }
    void ptest(const FloatRegister &lhs, const FloatRegister &rhs) {
        JS_ASSERT(HasSSE41());
        masm.ptest_rr(rhs.code(), lhs.code());
    }
    void ucomisd(const FloatRegister &lhs, const FloatRegister &rhs) {
        JS_ASSERT(HasSSE2());
        masm.ucomisd_rr(rhs.code(), lhs.code());
    }
    void pcmpeqw(const FloatRegister &lhs, const FloatRegister &rhs) {
        JS_ASSERT(HasSSE2());
        masm.pcmpeqw_rr(rhs.code(), lhs.code());
    }    
    void movd(const Register &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.movd_rr(src.code(), dest.code());
    }
    void movd(const FloatRegister &src, const Register &dest) {
        JS_ASSERT(HasSSE2());
        masm.movd_rr(src.code(), dest.code());
    }
    void addsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.addsd_rr(src.code(), dest.code());
    }
    void addsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.addsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.addsd_mr(src.disp(), src.base(), dest.code());
            break;
#ifdef JS_CPU_X86
          case Operand::ADDRESS:
            masm.addsd_mr(src.address(), dest.code());
            break;
#endif
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void subsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.subsd_rr(src.code(), dest.code());
    }
    void subsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.subsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.subsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void mulsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.mulsd_rr(src.code(), dest.code());
    }
    void mulsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.mulsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.mulsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void divsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.divsd_rr(src.code(), dest.code());
    }
    void divsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.divsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.divsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void xorpd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.xorpd_rr(src.code(), dest.code());
    }
    void orpd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.orpd_rr(src.code(), dest.code());
    }
    void andpd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.andpd_rr(src.code(), dest.code());
    }
    void sqrtsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.sqrtsd_rr(src.code(), dest.code());
    }
    void roundsd(const FloatRegister &src, const FloatRegister &dest,
                 JSC::X86Assembler::RoundingMode mode)
    {
        JS_ASSERT(HasSSE41());
        masm.roundsd_rr(src.code(), dest.code(), mode);
    }
    void minsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.minsd_rr(src.code(), dest.code());
    }
    void minsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.minsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.minsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void maxsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        masm.maxsd_rr(src.code(), dest.code());
    }
    void maxsd(const Operand &src, const FloatRegister &dest) {
        JS_ASSERT(HasSSE2());
        switch (src.kind()) {
          case Operand::FPREG:
            masm.maxsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.maxsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void fisttp(const Operand &dest) {
        JS_ASSERT(HasSSE3());
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.fisttp_m(dest.disp(), dest.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void fld(const Operand &dest) {
        switch (dest.kind()) {
          case Operand::REG_DISP:
            masm.fld_m(dest.disp(), dest.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }
    void fstp(const Operand &src) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            masm.fstp_m(src.disp(), src.base());
            break;
          default:
            MOZ_ASSUME_UNREACHABLE("unexpected operand kind");
        }
    }

    // Defined for compatibility with ARM's assembler
    uint32_t actualOffset(uint32_t x) {
        return x;
    }

    uint32_t actualIndex(uint32_t x) {
        return x;
    }

    void flushBuffer() {
    }

    // Patching.

    static size_t patchWrite_NearCallSize() {
        return 5;
    }
    static uintptr_t getPointer(uint8_t *instPtr) {
        uintptr_t *ptr = ((uintptr_t *) instPtr) - 1;
        return *ptr;
    }
    // Write a relative call at the start location |dataLabel|.
    // Note that this DOES NOT patch data that comes before |label|.
    static void patchWrite_NearCall(CodeLocationLabel startLabel, CodeLocationLabel target) {
        uint8_t *start = startLabel.raw();
        *start = 0xE8;
        ptrdiff_t offset = target - startLabel - patchWrite_NearCallSize();
        JS_ASSERT(int32_t(offset) == offset);
        *((int32_t *) (start + 1)) = offset;
    }

    static void patchWrite_Imm32(CodeLocationLabel dataLabel, Imm32 toWrite) {
        *((int32_t *) dataLabel.raw() - 1) = toWrite.value;
    }

    static void patchDataWithValueCheck(CodeLocationLabel data, ImmWord newData,
                                        ImmWord expectedData) {
        // The pointer given is a pointer to *after* the data.
        uintptr_t *ptr = ((uintptr_t *) data.raw()) - 1;
        JS_ASSERT(*ptr == expectedData.value);
        *ptr = newData.value;
    }
    static uint32_t nopSize() {
        return 1;
    }
    static uint8_t *nextInstruction(uint8_t *cur, uint32_t *count) {
        MOZ_ASSUME_UNREACHABLE("nextInstruction NYI on x86");
    }

    // Toggle a jmp or cmp emitted by toggledJump().
    static void ToggleToJmp(CodeLocationLabel inst) {
        uint8_t *ptr = (uint8_t *)inst.raw();
        JS_ASSERT(*ptr == 0x3D);
        *ptr = 0xE9;
    }
    static void ToggleToCmp(CodeLocationLabel inst) {
        uint8_t *ptr = (uint8_t *)inst.raw();
        JS_ASSERT(*ptr == 0xE9);
        *ptr = 0x3D;
    }
    static void ToggleCall(CodeLocationLabel inst, bool enabled) {
        uint8_t *ptr = (uint8_t *)inst.raw();
        JS_ASSERT(*ptr == 0x3D || // CMP
                  *ptr == 0xE8);  // CALL
        *ptr = enabled ? 0xE8 : 0x3D;
    }
};

} // namespace ion
} // namespace js

#endif /* ion_shared_Assembler_x86_shared_h */
