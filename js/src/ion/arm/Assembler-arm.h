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

#ifndef jsion_cpu_arm_assembler_h__
#define jsion_cpu_arm_assembler_h__

#include "ion/shared/Assembler-shared.h"
#include "assembler/assembler/ARMAssembler.h"
#include "ion/CompactBuffer.h"
#include "ion/IonCode.h"

namespace js {
namespace ion {
//NOTE: there are duplicates in this list!
// sometimes we want to specifically refer to the
// link register as a link register (bl lr is much
// clearer than bl r14).  HOWEVER, this register can
// easily be a gpr when it is not busy holding the return
// address.
static const Register r0 = { JSC::ARMRegisters::r0 };
static const Register r1 = { JSC::ARMRegisters::r1 };
static const Register r2 = { JSC::ARMRegisters::r2 };
static const Register r3 = { JSC::ARMRegisters::r3 };
static const Register r4 = { JSC::ARMRegisters::r4 };
static const Register r5 = { JSC::ARMRegisters::r5 };
static const Register r6 = { JSC::ARMRegisters::r6 };
static const Register r7 = { JSC::ARMRegisters::r7 };
static const Register r8 = { JSC::ARMRegisters::r8 };
static const Register r9 = { JSC::ARMRegisters::r9 };
static const Register r10 = { JSC::ARMRegisters::r10 };
static const Register r11 = { JSC::ARMRegisters::r11 };
static const Register r12 = { JSC::ARMRegisters::ip };
static const Register ip = { JSC::ARMRegisters::ip };
static const Register sp = { JSC::ARMRegisters::sp };
static const Register r14 = { JSC::ARMRegisters::lr };
static const Register lr = { JSC::ARMRegisters::lr };
static const Register pc = { JSC::ARMRegisters::pc };

static const Register InvalidReg = { JSC::ARMRegisters::invalid_reg };
static const FloatRegister InvalidFloatReg = { JSC::ARMRegisters::invalid_freg };

static const Register JSReturnReg_Type = r1;
static const Register JSReturnReg_Data = r2;
static const Register StackPointer = sp;
static const Register ReturnReg = r0;
static const FloatRegister ScratchFloatReg = { JSC::ARMRegisters::d0 };
enum Index {
    Offset = 1<<24,
    PreIndex = 1<<21 | 1 << 24,
    PostIndex = 1<<21
};

enum SetCond {
    SetCond = 1<<20,
    NoSetCond = 0
};

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
        SCALE
    };

    Kind kind_ : 2;
    int32 base_ : 5;
    Scale scale_ : 2;
    int32 disp_;
    int32 index_ : 5;

  public:
    explicit Operand(const Register &reg)
      : kind_(REG),
        base_(reg.code())
    { }
    explicit Operand(const FloatRegister &reg)
      : kind_(FPREG),
        base_(reg.code())
    { }
    explicit Operand(const Register &base, const Register &index, Scale scale, int32 disp = 0)
      : kind_(SCALE),
        base_(base.code()),
        scale_(scale),
        disp_(disp),
        index_(index.code())
    { }
    Operand(const Register &reg, int32 disp)
      : kind_(REG_DISP),
        base_(reg.code()),
        disp_(disp)
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
};

class ValueOperand
{
    Register type_;
    Register payload_;

  public:
    ValueOperand(Register type, Register payload)
      : type_(type), payload_(payload)
    { }

    Register typeReg() const {
        return type_;
    }
    Register payloadReg() const {
        return payload_;
    }
};

class Assembler
{
  protected:
    struct RelativePatch {
        int32 offset;
        void *target;
        Relocation::Kind kind;

        RelativePatch(int32 offset, void *target, Relocation::Kind kind)
          : offset(offset),
            target(target),
            kind(kind)
        { }
    };

    js::Vector<DeferredData *, 0, SystemAllocPolicy> data_;
    js::Vector<CodeLabel *, 0, SystemAllocPolicy> codeLabels_;
    js::Vector<RelativePatch, 8, SystemAllocPolicy> jumps_;
    CompactBufferWriter relocations_;
    size_t dataBytesNeeded_;

    bool enoughMemory_;
  protected:
    JSC::ARMAssembler masm;

    typedef JSC::ARMAssembler::JmpSrc JmpSrc;
    typedef JSC::ARMAssembler::JmpDst JmpDst;

  public:
    enum Condition {
        Equal = JSC::ARMAssembler::EQ,
        NotEqual = JSC::ARMAssembler::NE,
        Above = JSC::ARMAssembler::HI,
        AboveOrEqual = JSC::ARMAssembler::CS,
        Below = JSC::ARMAssembler::CC,
        BelowOrEqual = JSC::ARMAssembler::LE,
        GreaterThan = JSC::ARMAssembler::GT,
        GreaterThanOrEqual = JSC::ARMAssembler::GE,
        LessThan = JSC::ARMAssembler::LT,
        LessThanOrEqual = JSC::ARMAssembler::LE,
        Overflow = JSC::ARMAssembler::VS,
        Signed = JSC::ARMAssembler::MI,
        Zero = JSC::ARMAssembler::EQ,
        NonZero = JSC::ARMAssembler::NE
    };

    Assembler()
      : dataBytesNeeded_(0),
        enoughMemory_(true)
    {
    }

    static Condition inverseCondition(Condition cond);

    // MacroAssemblers hold onto gcthings, so they are traced by the GC.
    void trace(JSTracer *trc);

    bool oom() const {
        return masm.oom() ||
               !enoughMemory_ ||
               relocations_.oom();
    }

    void executableCopy(void *buffer);
    void processDeferredData(IonCode *code, uint8 *data);
    void processCodeLabels(IonCode *code);
    void copyRelocationTable(uint8 *buffer);

    bool addDeferredData(DeferredData *data, size_t bytes) {
        data->setOffset(dataBytesNeeded_);
        dataBytesNeeded_ += bytes;
        if (dataBytesNeeded_ >= MAX_BUFFER_SIZE)
            return false;
        return data_.append(data);
    }

    bool addCodeLabel(CodeLabel *label) {
        return codeLabels_.append(label);
    }

    // Size of the instruction stream, in bytes.
    size_t size() const {
        return masm.size();
    }
    // Size of the relocation table, in bytes.
    size_t relocationTableSize() const {
        return relocations_.length();
    }
    // Size of the data table, in bytes.
    size_t dataSize() const {
        return dataBytesNeeded_;
    }
    size_t bytesNeeded() const {
        return size() + dataSize() + relocationTableSize();
    }

  public:
    void align(int alignment) {
#if 0
        masm.align(alignment);
#endif
    }
    void movl(const Imm32 &imm32, const Register &dest) {
#if 0
        masm.movl_i32r(imm32.value, dest.code());
#endif
    }
    void movl(const Register &src, const Register &dest) {
#if 0
        masm.movl_rr(src.code(), dest.code());
#endif
    }
    void movl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.movl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void movl(const Register &src, const Operand &dest) {
#if 0
        switch (dest.kind()) {
          case Operand::REG:
            masm.movl_rr(src.code(), dest.reg());
            break;
          case Operand::REG_DISP:
            masm.movl_rm(src.code(), dest.disp(), dest.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }

    void movsd(const FloatRegister &src, const FloatRegister &dest) {
#if 0
        masm.movsd_rr(src.code(), dest.code());
#endif
    }
    void movsd(const Operand &src, const FloatRegister &dest) {
#if 0
        switch (src.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.fpu(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.movsd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void movsd(const FloatRegister &src, const Operand &dest) {
#if 0
        switch (dest.kind()) {
          case Operand::FPREG:
            masm.movsd_rr(src.code(), dest.fpu());
            break;
          case Operand::REG_DISP:
            masm.movsd_rm(src.code(), dest.disp(), dest.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }

    void j(Condition cond, Label *label) {
#if 0
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(masm.jCC(static_cast<JSC::ARMAssembler::Condition>(cond)), JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc j = masm.jCC(static_cast<JSC::ARMAssembler::Condition>(cond));
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
#endif
    }
    void jmp(Label *label) {
#if 0
        if (label->bound()) {
            // The jump can be immediately patched to the correct destination.
            masm.linkJump(masm.jmp(), JmpDst(label->offset()));
        } else {
            // Thread the jump list through the unpatched jump targets.
            JmpSrc j = masm.jmp();
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
#endif
    }
    void jmp(const Operand &op){
#if 0
        switch (op.kind()) {
          case Operand::SCALE:
            masm.jmp_m(op.disp(), op.base(), op.index(), op.scale());
            break;
          case Operand::REG:
            masm.jmp_r(op.reg());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void bind(Label *label) {
#if 0
        JSC::MacroAssembler::Label jsclabel;
        if (label->used()) {
            bool more;
            JSC::ARMAssembler::JmpSrc jmp(label->offset());
            do {
                JSC::ARMAssembler::JmpSrc next;
                more = masm.nextJump(jmp, &next);
                masm.linkJump(jmp, masm.label());
                jmp = next;
            } while (more);
        }
        label->bind(masm.label().offset());
#endif
    }

    static void Bind(IonCode *code, AbsoluteLabel *label, const void *address) {
#if 0
        uint8 *raw = code->raw();
        if (label->used()) {
            intptr_t src = label->offset();
            do {
                intptr_t next = reinterpret_cast<intptr_t>(JSC::ARMAssembler::getPointer(raw + src));
                JSC::ARMAssembler::setPointer(raw + src, address);
                src = next;
            } while (src != AbsoluteLabel::INVALID_OFFSET);
        }
        JS_ASSERT(((uint8 *)address - raw) >= 0 && ((uint8 *)address - raw) < INT_MAX);
        label->bind();
#endif
    }

    void ret() {
#if 0
        masm.ret();
#endif
    }
    void call(Label *label) {
#if 0
        if (label->bound()) {
            masm.linkJump(masm.call(), JmpDst(label->offset()));
        } else {
            JmpSrc j = masm.call();
            JmpSrc prev = JmpSrc(label->use(j.offset()));
            masm.setNextJump(j, prev);
        }
#endif
    }
    void call(const Register &reg) {
#if 0
        masm.call(reg.code());
#endif
    }
    void call(const Operand &op) {
#if 0
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
#endif
    }

    void breakpoint() {
#if 0
        masm.int3();
#endif
    }

    // The below cmpl methods switch the lhs and rhs when it invokes the
    // macroassembler to conform with intel standard.  When calling this
    // function put the left operand on the left as you would expect.
    void cmpl(const Register &lhs, const Register &rhs) {
#if 0
        masm.cmpl_rr(rhs.code(), lhs.code());
#endif
    }
    void cmpl(Imm32 imm, const Register &reg) {
#if 0
        masm.cmpl_ir(imm.value, reg.code());
#endif
    }
    void cmpl(const Register &lhs, const Operand &rhs) {
#if 0
        switch (rhs.kind()) {
          case Operand::REG:
            masm.cmpl_rr(rhs.reg(), lhs.code());
            break;
          case Operand::REG_DISP:
            masm.cmpl_mr(rhs.disp(), rhs.base(), lhs.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void cmpl(const Operand &op, Imm32 imm) {
#if 0
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
#endif
    }
    void testl(const Register &lhs, const Register &rhs) {
#if 0
        masm.testl_rr(rhs.code(), lhs.code());
#endif
    }
    void testl(Imm32 lhs, const Register &rhs) {
#if 0
        masm.testl_i32r(lhs.value, rhs.code());
#endif
    }

    void addl(Imm32 imm, const Register &dest) {
#if 0
        masm.addl_ir(imm.value, dest.code());
#endif
    }
    void addl(Imm32 imm, const Operand &op) {
#if 0
        switch (op.kind()) {
          case Operand::REG:
            masm.addl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.addl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void subl(Imm32 imm, const Register &dest) {
#if 0
        masm.subl_ir(imm.value, dest.code());
#endif
    }
    void subl(Imm32 imm, const Operand &op) {
#if 0
        switch (op.kind()) {
          case Operand::REG:
            masm.subl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.subl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void addl(const Register &src, const Register &dest) {
#if 0
        masm.addl_rr(src.code(), dest.code());
#endif
    }
    void subl(const Register &src, const Register &dest) {
#if 0
        masm.subl_rr(src.code(), dest.code());
#endif
    }
    void subl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.subl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.subl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void orl(Imm32 imm, const Register &reg) {
#if 0
        masm.orl_ir(imm.value, reg.code());
#endif
    }
    void orl(Imm32 imm, const Operand &op) {
#if 0
        switch (op.kind()) {
          case Operand::REG:
            masm.orl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.orl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void xorl(Imm32 imm, const Register &reg) {
#if 0
        masm.xorl_ir(imm.value, reg.code());
#endif
    }
    void xorl(Imm32 imm, const Operand &op) {
#if 0
        switch (op.kind()) {
          case Operand::REG:
            masm.xorl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.xorl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void andl(Imm32 imm, const Register &dest) {
#if 0
        masm.andl_ir(imm.value, dest.code());
#endif
    }
    void andl(Imm32 imm, const Operand &op) {
#if 0
        switch (op.kind()) {
          case Operand::REG:
            masm.andl_ir(imm.value, op.reg());
            break;
          case Operand::REG_DISP:
            masm.andl_im(imm.value, op.disp(), op.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void addl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.addl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.addl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void orl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.orl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.orl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void xorl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.xorl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.xorl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void andl(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.andl_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.andl_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void imull(Imm32 imm, const Register &dest) {
#if 0
        masm.imull_i32r(dest.code(), imm.value, dest.code());
#endif
    }
    void imull(const Register &src, const Register &dest) {
#if 0
        masm.imull_rr(src.code(), dest.code());
#endif
    }
    void imull(const Operand &src, const Register &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.imull_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.imull_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void negl(const Operand &src) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.negl_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.negl_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
#endif
    }
    void notl(const Operand &src) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.notl_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.notl_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
#endif
    }

    void shrl(const Imm32 imm, const Register &dest) {
#if 0
        masm.shrl_i8r(imm.value, dest.code());
#endif
    }
    void shll(const Imm32 imm, const Register &dest) {
#if 0
        masm.shll_i8r(imm.value, dest.code());
#endif
    }

    void push(const Imm32 imm) {
#if 0
        masm.push_i32(imm.value);
#endif
    }
    void push(const Operand &src) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.push_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.push_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void push(const Register &src) {
        masm.push_r(src.code());
    }
    void pop(const Operand &src) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.pop_r(src.reg());
            break;
          case Operand::REG_DISP:
            masm.pop_m(src.disp(), src.base());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void pop(const Register &src) {
#if 0
        masm.pop_r(src.code());
#endif
    }

    void unpcklps(const FloatRegister &src, const FloatRegister &dest) {
#if 0
        masm.unpcklps_rr(src.code(), dest.code());
#endif
    }
    void pinsrd(const Register &src, const FloatRegister &dest) {
#if 0
        masm.pinsrd_rr(src.code(), dest.code());
#endif
    }
    void pinsrd(const Operand &src, const FloatRegister &dest) {
#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.pinsrd_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.pinsrd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void psrlq(Imm32 shift, const FloatRegister &dest) {
#if 0
        masm.psrldq_rr(dest.code(), shift.value);
#endif
    }

    void cvtsi2sd(const Operand &src, const FloatRegister &dest) {

#if 0
        switch (src.kind()) {
          case Operand::REG:
            masm.cvtsi2sd_rr(src.reg(), dest.code());
            break;
          case Operand::REG_DISP:
            masm.cvtsi2sd_mr(src.disp(), src.base(), dest.code());
            break;
          default:
            JS_NOT_REACHED("unexpected operand kind");
        }
#endif
    }
    void cvttsd2si(const FloatRegister &src, const Register &dest) {
        JS_NOT_REACHED("cvttsd2si NYI");
#if 0
        masm.cvttsd2si_rr(src.code(), dest.code());
#endif
    }
    void cvtsi2sd(const Register &src, const FloatRegister &dest) {
        JS_NOT_REACHED("cvtsi2sd cvtsi2sd NYI");
#if 0
        masm.cvtsi2sd_rr(src.code(), dest.code());
#endif
    }
    void movmskpd(const FloatRegister &src, const Register &dest) {
        JS_NOT_REACHED("movmskpd NYI");
#if 0
        masm.movmskpd_rr(src.code(), dest.code());
#endif
    }
    void ptest(const FloatRegister &lhs, const FloatRegister &rhs) {
        JS_NOT_REACHED("ptest NYI");
#if 0
        JS_ASSERT(HasSSE41());
        masm.ptest_rr(rhs.code(), lhs.code());
#endif
    }
    void ucomisd(const FloatRegister &lhs, const FloatRegister &rhs) {
        JS_NOT_REACHED("ucomisd NYI");
#if 0
        masm.ucomisd_rr(rhs.code(), lhs.code());
#endif
    }
    void movd(const Register &src, const FloatRegister &dest) {
        JS_NOT_REACHED("movd NYI");
#if 0
        masm.movd_rr(src.code(), dest.code());
#endif
    }
    void movd(const FloatRegister &src, const Register &dest) {
        JS_NOT_REACHED("movd NYI");
#if 0
        masm.movd_rr(src.code(), dest.code());
#endif
    }
    void addsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_NOT_REACHED("addsd NYI");
#if 0
        masm.addsd_rr(src.code(), dest.code());
#endif
    }
    void mulsd(const FloatRegister &src, const FloatRegister &dest) {
        JS_NOT_REACHED("mulsd NYI");
#if 0
        masm.mulsd_rr(src.code(), dest.code());
#endif
    }
    void xorpd(const FloatRegister &src, const FloatRegister &dest) {
        JS_NOT_REACHED("xorpd NYI");
#if 0
        masm.xorpd_rr(src.code(), dest.code());
#endif
    }

    void writeRelocation(JmpSrc src) {
        relocations_.writeUnsigned(src.offset());
    }
    void addPendingJump(JmpSrc src, void *target, Relocation::Kind kind) {
        enoughMemory_ &= jumps_.append(RelativePatch(src.offset(), target, kind));
        if (kind == Relocation::CODE)
            writeRelocation(src);
    }

  public:
    static void TraceRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader);

    // The buffer is about to be linked, make sure any constant pools or excess
    // bookkeeping has been flushed to the instruction stream.
    void flush() { }

    // Copy the assembly code to the given buffer, and perform any pending
    // relocations relying on the target address.
    void executableCopy(uint8 *buffer);

    // Actual assembly emitting functions.

    void movl(const ImmGCPtr &ptr, const Register &dest) {
    }

    void mov(const Imm32 &imm32, const Register &dest) {
    }
    void mov(const Operand &src, const Register &dest) {
    }
    void mov(const Register &src, const Operand &dest) {
    }
    void mov(AbsoluteLabel *label, const Register &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
    }
    void lea(const Operand &src, const Register &dest) {
        switch (src.kind()) {
          case Operand::REG_DISP:
            break;
          case Operand::SCALE:
            break;
          default:
            JS_NOT_REACHED("unexepcted operand kind");
        }
    }
    // convert what to what now?
    void cvttsd2s(const FloatRegister &src, const Register &dest) {
    }

    void jmp(void *target, Relocation::Kind reloc) {
#if 0
        JmpSrc src = masm.branch();
        addPendingJump(src, target, reloc);
#endif
    }
    void j(Condition cond, void *target, Relocation::Kind reloc) {
#if 0
        JmpSrc src = masm.branch(cond);
        addPendingJump(src, target, reloc);
#endif
    }

    void movsd(const double *dp, const FloatRegister &dest) {
    }
    void movsd(AbsoluteLabel *label, const FloatRegister &dest) {
        JS_ASSERT(!label->bound());
        // Thread the patch list through the unpatched address word in the
        // instruction stream.
    }
    void startDataTransferM(bool isLoad, Register rm, bool update = false) {}
    void transferReg(Register rn) {}
    void finishDataTransfer() {}
};

static const uint32 NumArgRegs = 4;

static inline bool
GetArgReg(uint32 arg, Register *out)
{
    switch (arg) {
    case 0: *out = r0;
        return true;
    default:
        return false;
    }
}

} // namespace ion
} // namespace js

#endif // jsion_cpu_arm_assembler_h__

