/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_macro_assembler_x64_h__
#define jsion_macro_assembler_x64_h__

#include "ion/shared/MacroAssembler-x86-shared.h"
#include "ion/MoveResolver.h"
#include "ion/IonFrames.h"
#include "jsnum.h"

namespace js {
namespace ion {

struct ImmShiftedTag : public ImmWord
{
    ImmShiftedTag(JSValueShiftedTag shtag)
      : ImmWord((uintptr_t)shtag)
    { }

    ImmShiftedTag(JSValueType type)
      : ImmWord(uintptr_t(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type))))
    { }
};

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag tag)
      : Imm32(tag)
    { }
};

class MacroAssemblerX64 : public MacroAssemblerX86Shared
{
    // Number of bytes the stack is adjusted inside a call to C. Calls to C may
    // not be nested.
    bool inCall_;
    uint32_t args_;
    uint32_t passedIntArgs_;
    uint32_t passedFloatArgs_;
    uint32_t stackForCall_;
    bool dynamicAlignment_;
    bool enoughMemory_;

    void setupABICall(uint32_t arg);

  protected:
    MoveResolver moveResolver_;

  public:
    using MacroAssemblerX86Shared::call;
    using MacroAssemblerX86Shared::Push;
    using MacroAssemblerX86Shared::callWithExitFrame;

    enum Result {
        GENERAL,
        DOUBLE
    };

    typedef MoveResolver::MoveOperand MoveOperand;
    typedef MoveResolver::Move Move;

    MacroAssemblerX64()
      : inCall_(false),
        enoughMemory_(true)
    {
    }

    bool oom() const {
        return MacroAssemblerX86Shared::oom() || !enoughMemory_;
    }

    /////////////////////////////////////////////////////////////////
    // X64 helpers.
    /////////////////////////////////////////////////////////////////
    void call(ImmWord target) {
        movq(target, rax);
        call(rax);
    }

    // Refers to the upper 32 bits of a 64-bit Value operand.
    // On x86_64, the upper 32 bits do not necessarily only contain the type.
    Operand ToUpper32(Operand base) {
        switch (base.kind()) {
          case Operand::REG_DISP:
            return Operand(Register::FromCode(base.base()), base.disp() + 4);

          case Operand::SCALE:
            return Operand(Register::FromCode(base.base()), Register::FromCode(base.index()),
                           base.scale(), base.disp() + 4);

          default:
            JS_NOT_REACHED("unexpected operand kind");
            return base; // Silence GCC warning.
        }
    }
    static inline Operand ToUpper32(const Address &address) {
        return Operand(address.base, address.offset + 4);
    }
    static inline Operand ToUpper32(const BaseIndex &address) {
        return Operand(address.base, address.index, address.scale, address.offset + 4);
    }

    uint32_t Upper32Of(JSValueShiftedTag tag) {
        union { // Implemented in this way to appease MSVC++.
            uint64_t tag;
            struct {
                uint32_t lo32;
                uint32_t hi32;
            } s;
        } e;
        e.tag = tag;
        return e.s.hi32;
    }

    JSValueShiftedTag GetShiftedTag(JSValueType type) {
        return (JSValueShiftedTag)JSVAL_TYPE_TO_SHIFTED_TAG(type);
    }

    /////////////////////////////////////////////////////////////////
    // X86/X64-common interface.
    /////////////////////////////////////////////////////////////////
    void storeValue(ValueOperand val, Operand dest) {
        movq(val.valueReg(), dest);
    }
    void storeValue(ValueOperand val, const Address &dest) {
        storeValue(val, Operand(dest));
    }
    template <typename T>
    void storeValue(JSValueType type, Register reg, const T &dest) {
        // Value types with 32-bit payloads can be emitted as two 32-bit moves.
        if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN) {
            movl(reg, Operand(dest));
            movl(Imm32(Upper32Of(GetShiftedTag(type))), ToUpper32(Operand(dest)));
        } else {
            boxValue(type, reg, ScratchReg);
            movq(ScratchReg, Operand(dest));
        }
    }
    template <typename T>
    void storeValue(const Value &val, const T &dest) {
        jsval_layout jv = JSVAL_TO_IMPL(val);
        movq(ImmWord(jv.asBits), ScratchReg);
        if (val.isMarkable())
            writeDataRelocation(val);
        movq(ScratchReg, Operand(dest));
    }
    void storeValue(ValueOperand val, BaseIndex dest) {
        storeValue(val, Operand(dest));
    }
    void loadValue(Operand src, ValueOperand val) {
        movq(src, val.valueReg());
    }
    void loadValue(Address src, ValueOperand val) {
        loadValue(Operand(src), val);
    }
    void loadValue(const BaseIndex &src, ValueOperand val) {
        loadValue(Operand(src), val);
    }
    void tagValue(JSValueType type, Register payload, ValueOperand dest) {
        JS_ASSERT(dest.valueReg() != ScratchReg);
        if (payload != dest.valueReg())
            movq(payload, dest.valueReg());
        movq(ImmShiftedTag(type), ScratchReg);
        orq(Operand(ScratchReg), dest.valueReg());
    }
    void pushValue(ValueOperand val) {
        push(val.valueReg());
    }
    void Push(const ValueOperand &val) {
        pushValue(val);
        framePushed_ += sizeof(Value);
    }
    void popValue(ValueOperand val) {
        pop(val.valueReg());
    }
    void pushValue(const Value &val) {
        jsval_layout jv = JSVAL_TO_IMPL(val);
        push(ImmWord(jv.asBits));
    }
    void pushValue(JSValueType type, Register reg) {
        boxValue(type, reg, ScratchReg);
        push(ScratchReg);
    }
    void pushValue(const Address &addr) {
        push(Operand(addr));
    }

    void moveValue(const Value &val, const Register &dest) {
        jsval_layout jv = JSVAL_TO_IMPL(val);
        movq(ImmWord(jv.asPtr), dest);
        writeDataRelocation(val);
    }
    void moveValue(const Value &src, const ValueOperand &dest) {
        moveValue(src, dest.valueReg());
    }
    void moveValue(const ValueOperand &src, const ValueOperand &dest) {
        if (src.valueReg() != dest.valueReg())
            movq(src.valueReg(), dest.valueReg());
    }
    void boxValue(JSValueType type, Register src, Register dest) {
        JS_ASSERT(src != dest);

        JSValueShiftedTag tag = (JSValueShiftedTag)JSVAL_TYPE_TO_SHIFTED_TAG(type);
        movq(ImmShiftedTag(tag), dest);

        // Integers must be treated specially, since the top 32 bits of the
        // register may be filled, we can't clobber the tag bits. This can
        // happen when instructions automatically sign-extend their result.
        // To account for this, we clear the top bits of the register, which
        // is safe since those bits aren't required.
        if (type == JSVAL_TYPE_INT32 || type == JSVAL_TYPE_BOOLEAN)
            movl(src, src);
        orq(src, dest);
    }

    Condition testUndefined(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_UNDEFINED));
        return cond;
    }
    Condition testInt32(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_INT32));
        return cond;
    }
    Condition testBoolean(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_BOOLEAN));
        return cond;
    }
    Condition testNull(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_NULL));
        return cond;
    }
    Condition testString(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_STRING));
        return cond;
    }
    Condition testObject(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_OBJECT));
        return cond;
    }
    Condition testDouble(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, Imm32(JSVAL_TAG_MAX_DOUBLE));
        return cond == Equal ? BelowOrEqual : Above;
    }
    Condition testNumber(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, Imm32(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET));
        return cond == Equal ? BelowOrEqual : Above;
    }
    Condition testGCThing(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, Imm32(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET));
        return cond == Equal ? AboveOrEqual : Below;
    }

    Condition testMagic(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_MAGIC));
        return cond;
    }
    Condition testError(Condition cond, const Register &tag) {
        return testMagic(cond, tag);
    }
    Condition testPrimitive(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET));
        return cond == Equal ? Below : AboveOrEqual;
    }

    Condition testUndefined(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testUndefined(cond, ScratchReg);
    }
    Condition testInt32(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testInt32(cond, ScratchReg);
    }
    Condition testBoolean(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testBoolean(cond, ScratchReg);
    }
    Condition testDouble(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testDouble(cond, ScratchReg);
    }
    Condition testNumber(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testNumber(cond, ScratchReg);
    }
    Condition testNull(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testNull(cond, ScratchReg);
    }
    Condition testString(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testString(cond, ScratchReg);
    }
    Condition testObject(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testObject(cond, ScratchReg);
    }
    Condition testGCThing(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testGCThing(cond, ScratchReg);
    }
    Condition testGCThing(Condition cond, const Address &src) {
        splitTag(src, ScratchReg);
        return testGCThing(cond, ScratchReg);
    }
    Condition testGCThing(Condition cond, const BaseIndex &src) {
        splitTag(src, ScratchReg);
        return testGCThing(cond, ScratchReg);
    }
    Condition testMagic(Condition cond, const Address &src) {
        splitTag(src, ScratchReg);
        return testMagic(cond, ScratchReg);
    }
    Condition testMagic(Condition cond, const BaseIndex &src) {
        splitTag(src, ScratchReg);
        return testMagic(cond, ScratchReg);
    }
    Condition testPrimitive(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testPrimitive(cond, ScratchReg);
    }

    Condition isMagic(Condition cond, const ValueOperand &src, JSWhyMagic why) {
        uint64_t magic = MagicValue(why).asRawBits();
        cmpPtr(src.valueReg(), ImmWord(magic));
        return cond;
    }

    void cmpPtr(const Register &lhs, const ImmWord rhs) {
        JS_ASSERT(lhs != ScratchReg);
        movq(rhs, ScratchReg);
        cmpq(lhs, ScratchReg);
    }
    void cmpPtr(const Register &lhs, const ImmGCPtr rhs) {
        JS_ASSERT(lhs != ScratchReg);
        movq(rhs, ScratchReg);
        cmpq(lhs, ScratchReg);
    }
    void cmpPtr(const Operand &lhs, const ImmGCPtr rhs) {
        movq(rhs, ScratchReg);
        cmpq(lhs, ScratchReg);
    }
    void cmpPtr(const Operand &lhs, const ImmWord rhs) {
        movq(rhs, ScratchReg);
        cmpq(lhs, ScratchReg);
    }
    void cmpPtr(const Address &lhs, const ImmGCPtr rhs) {
        cmpPtr(Operand(lhs), rhs);
    }
    void cmpPtr(const Address &lhs, const ImmWord rhs) {
        cmpPtr(Operand(lhs), rhs);
    }
    void cmpPtr(const Operand &lhs, const Register &rhs) {
        cmpq(lhs, rhs);
    }
    void cmpPtr(const Address &lhs, const Register &rhs) {
        cmpPtr(Operand(lhs), rhs);
    }
    void cmpPtr(const Register &lhs, const Register &rhs) {
        return cmpq(lhs, rhs);
    }
    void testPtr(const Register &lhs, const Register &rhs) {
        testq(lhs, rhs);
    }

    Condition testNegativeZero(const FloatRegister &reg, const Register &scratch);

    /////////////////////////////////////////////////////////////////
    // Common interface.
    /////////////////////////////////////////////////////////////////
    void reserveStack(uint32_t amount) {
        if (amount)
            subq(Imm32(amount), StackPointer);
        framePushed_ += amount;
    }
    void freeStack(uint32_t amount) {
        JS_ASSERT(amount <= framePushed_);
        if (amount)
            addq(Imm32(amount), StackPointer);
        framePushed_ -= amount;
    }
    void freeStack(Register amount) {
        addq(amount, StackPointer);
    }

    void addPtr(const Register &src, const Register &dest) {
        addq(src, dest);
    }
    void addPtr(Imm32 imm, const Register &dest) {
        addq(imm, dest);
    }
    void addPtr(Imm32 imm, const Address &dest) {
        addq(imm, Operand(dest));
    }
    void addPtr(ImmWord imm, const Register &dest) {
        JS_ASSERT(dest != ScratchReg);
        movq(imm, ScratchReg);
        addq(ScratchReg, dest);
    }
    void subPtr(Imm32 imm, const Register &dest) {
        subq(imm, dest);
    }
    void subPtr(const Register &src, const Register &dest) {
        subq(src, dest);
    }

    // Specialization for AbsoluteAddress.
    void branchPtr(Condition cond, const AbsoluteAddress &addr, const Register &ptr, Label *label) {
        JS_ASSERT(ptr != ScratchReg);
        movq(ImmWord(addr.addr), ScratchReg);
        branchPtr(cond, Operand(ScratchReg, 0x0), ptr, label);
    }

    template <typename T>
    void branchPrivatePtr(Condition cond, T lhs, ImmWord ptr, Label *label) {
        branchPtr(cond, lhs, ImmWord(ptr.value >> 1), label);
    }

    template <typename T, typename S>
    void branchPtr(Condition cond, T lhs, S ptr, Label *label) {
        cmpPtr(Operand(lhs), ptr);
        j(cond, label);
    }

    CodeOffsetJump jumpWithPatch(RepatchLabel *label) {
        JmpSrc src = jmpSrc(label);
        return CodeOffsetJump(size(), addPatchableJump(src, Relocation::HARDCODED));
    }
    template <typename S, typename T>
    CodeOffsetJump branchPtrWithPatch(Condition cond, S lhs, T ptr, RepatchLabel *label) {
        cmpPtr(lhs, ptr);
        JmpSrc src = jSrc(cond, label);
        return CodeOffsetJump(size(), addPatchableJump(src, Relocation::HARDCODED));
    }
    void branchPtr(Condition cond, Register lhs, Register rhs, Label *label) {
        cmpPtr(lhs, rhs);
        j(cond, label);
    }
    void branchTestPtr(Condition cond, Register lhs, Register rhs, Label *label) {
        testq(lhs, rhs);
        j(cond, label);
    }
    void decBranchPtr(Condition cond, const Register &lhs, Imm32 imm, Label *label) {
        subPtr(imm, lhs);
        j(cond, label);
    }

    void movePtr(const Register &src, const Register &dest) {
        movq(src, dest);
    }
    void movePtr(ImmWord imm, Register dest) {
        movq(imm, dest);
    }
    void movePtr(ImmGCPtr imm, Register dest) {
        movq(imm, dest);
    }
    void loadPtr(const AbsoluteAddress &address, Register dest) {
        movq(ImmWord(address.addr), ScratchReg);
        movq(Operand(ScratchReg, 0x0), dest);
    }
    void loadPtr(const Address &address, Register dest) {
        movq(Operand(address), dest);
    }
    void loadPtr(const BaseIndex &src, Register dest) {
        movq(Operand(src), dest);
	}
    void loadPrivate(const Address &src, Register dest) {
        loadPtr(src, dest);
        shlq(Imm32(1), dest);
    }
    void storePtr(ImmWord imm, const Address &address) {
        movq(imm, ScratchReg);
        movq(ScratchReg, Operand(address));
    }
    void storePtr(ImmGCPtr imm, const Address &address) {
        movq(imm, ScratchReg);
        movq(ScratchReg, Operand(address));
    }
    void storePtr(Register src, const Address &address) {
        movq(src, Operand(address));
    }
    void storePtr(const Register &src, const AbsoluteAddress &address) {
        movq(ImmWord(address.addr), ScratchReg);
        movq(src, Operand(ScratchReg, 0x0));
    }
    void rshiftPtr(Imm32 imm, Register dest) {
        shrq(imm, dest);
    }
    void lshiftPtr(Imm32 imm, Register dest) {
        shlq(imm, dest);
    }
    void orPtr(Imm32 imm, Register dest) {
        orq(imm, dest);
    }

    void splitTag(Register src, Register dest) {
        if (src != dest)
            movq(src, dest);
        shrq(Imm32(JSVAL_TAG_SHIFT), dest);
    }
    void splitTag(const ValueOperand &operand, const Register &dest) {
        splitTag(operand.valueReg(), dest);
    }
    void splitTag(const Address &operand, const Register &dest) {
        movq(Operand(operand), dest);
        shrq(Imm32(JSVAL_TAG_SHIFT), dest);
    }
    void splitTag(const BaseIndex &operand, const Register &dest) {
        movq(Operand(operand), dest);
        shrq(Imm32(JSVAL_TAG_SHIFT), dest);
    }

    // Extracts the tag of a value and places it in ScratchReg.
    Register splitTagForTest(const ValueOperand &value) {
        splitTag(value, ScratchReg);
        return ScratchReg;
    }
    void cmpTag(const ValueOperand &operand, ImmTag tag) {
        Register reg = splitTagForTest(operand);
        cmpl(Operand(reg), tag);
    }

    void branchTestUndefined(Condition cond, Register tag, Label *label) {
        cond = testUndefined(cond, tag);
        j(cond, label);
    }
    void branchTestInt32(Condition cond, Register tag, Label *label) {
        cond = testInt32(cond, tag);
        j(cond, label);
    }
    void branchTestDouble(Condition cond, Register tag, Label *label) {
        cond = testDouble(cond, tag);
        j(cond, label);
    }
    void branchTestBoolean(Condition cond, Register tag, Label *label) {
        cond = testBoolean(cond, tag);
        j(cond, label);
    }
    void branchTestNull(Condition cond, Register tag, Label *label) {
        cond = testNull(cond, tag);
        j(cond, label);
    }
    void branchTestString(Condition cond, Register tag, Label *label) {
        cond = testString(cond, tag);
        j(cond, label);
    }
    void branchTestObject(Condition cond, Register tag, Label *label) {
        cond = testObject(cond, tag);
        j(cond, label);
    }
    void branchTestNumber(Condition cond, Register tag, Label *label) {
        cond = testNumber(cond, tag);
        j(cond, label);
    }

    // x64 can test for certain types directly from memory when the payload
    // of the type is limited to 32 bits. This avoids loading into a register,
    // accesses half as much memory, and removes a right-shift.
    void branchTestUndefined(Condition cond, const Operand &operand, Label *label) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(ToUpper32(operand), Imm32(Upper32Of(GetShiftedTag(JSVAL_TYPE_UNDEFINED))));
        j(cond, label);
    }
    void branchTestInt32(Condition cond, const Operand &operand, Label *label) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(ToUpper32(operand), Imm32(Upper32Of(GetShiftedTag(JSVAL_TYPE_INT32))));
        j(cond, label);
    }
    void branchTestBoolean(Condition cond, const Operand &operand, Label *label) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(ToUpper32(operand), Imm32(Upper32Of(GetShiftedTag(JSVAL_TYPE_BOOLEAN))));
        j(cond, label);
    }
    void branchTestNull(Condition cond, const Operand &operand, Label *label) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(ToUpper32(operand), Imm32(Upper32Of(GetShiftedTag(JSVAL_TYPE_NULL))));
        j(cond, label);
    }

    // Perform a type-test on a full Value loaded into a register.
    // Clobbers the ScratchReg.
    void branchTestUndefined(Condition cond, const ValueOperand &src, Label *label) {
        cond = testUndefined(cond, src);
        j(cond, label);
    }
    void branchTestInt32(Condition cond, const ValueOperand &src, Label *label) {
        splitTag(src, ScratchReg);
        branchTestInt32(cond, ScratchReg, label);
    }
    void branchTestBoolean(Condition cond, const ValueOperand &src, Label *label) {
        splitTag(src, ScratchReg);
        branchTestBoolean(cond, ScratchReg, label);
    }
    void branchTestDouble(Condition cond, const ValueOperand &src, Label *label) {
        cond = testDouble(cond, src);
        j(cond, label);
    }
    void branchTestNull(Condition cond, const ValueOperand &src, Label *label) {
        cond = testNull(cond, src);
        j(cond, label);
    }
    void branchTestString(Condition cond, const ValueOperand &src, Label *label) {
        cond = testString(cond, src);
        j(cond, label);
    }
    void branchTestObject(Condition cond, const ValueOperand &src, Label *label) {
        cond = testObject(cond, src);
        j(cond, label);
    }
    void branchTestNumber(Condition cond, const ValueOperand &src, Label *label) {
        cond = testNumber(cond, src);
        j(cond, label);
    }
    template <typename T>
    void branchTestGCThing(Condition cond, const T &src, Label *label) {
        cond = testGCThing(cond, src);
        j(cond, label);
    }
    template <typename T>
    void branchTestPrimitive(Condition cond, const T &t, Label *label) {
        cond = testPrimitive(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestMagic(Condition cond, const T &t, Label *label) {
        cond = testMagic(cond, t);
        j(cond, label);
    }
    Condition testMagic(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testMagic(cond, ScratchReg);
    }
    Condition testError(Condition cond, const ValueOperand &src) {
        return testMagic(cond, src);
    }
    void branchTestValue(Condition cond, const ValueOperand &value, const Value &v, Label *label) {
        JS_ASSERT(value.valueReg() != ScratchReg);
        moveValue(v, ScratchReg);
        cmpq(value.valueReg(), ScratchReg);
        j(cond, label);
    }

    void boxDouble(const FloatRegister &src, const ValueOperand &dest) {
        movqsd(src, dest.valueReg());
    }
    void boxNonDouble(JSValueType type, const Register &src, const ValueOperand &dest) {
        JS_ASSERT(src != dest.valueReg());
        boxValue(type, src, dest.valueReg());
    }

    // Note that the |dest| register here may be ScratchReg, so we shouldn't
    // use it.
    void unboxInt32(const ValueOperand &src, const Register &dest) {
        movl(Operand(src.valueReg()), dest);
    }
    void unboxInt32(const Operand &src, const Register &dest) {
        movl(src, dest);
    }
    void unboxInt32(const Address &src, const Register &dest) {
        unboxInt32(Operand(src), dest);
    }

    void unboxArgObjMagic(const ValueOperand &src, const Register &dest) {
        unboxArgObjMagic(Operand(src.valueReg()), dest);
    }
    void unboxArgObjMagic(const Operand &src, const Register &dest) {
        xorq(dest, dest);
    }
    void unboxArgObjMagic(const Address &src, const Register &dest) {
        unboxArgObjMagic(Operand(src), dest);
    }

    void unboxBoolean(const ValueOperand &src, const Register &dest) {
        movl(Operand(src.valueReg()), dest);
    }
    void unboxBoolean(const Operand &src, const Register &dest) {
        movl(src, dest);
    }
    void unboxBoolean(const Address &src, const Register &dest) {
        unboxBoolean(Operand(src), dest);
    }

    void unboxDouble(const ValueOperand &src, const FloatRegister &dest) {
        movqsd(src.valueReg(), dest);
    }
    void unboxDouble(const Operand &src, const FloatRegister &dest) {
        lea(src, ScratchReg);
        movqsd(ScratchReg, dest);
    }
    void unboxPrivate(const ValueOperand &src, const Register dest) {
        movq(src.valueReg(), dest);
        shlq(Imm32(1), dest);
    }

    void notBoolean(const ValueOperand &val) {
        xorq(Imm32(1), val.valueReg());
    }

    // Unbox any non-double value into dest. Prefer unboxInt32 or unboxBoolean
    // instead if the source type is known.
    void unboxNonDouble(const ValueOperand &src, const Register &dest) {
        if (src.valueReg() == dest) {
            movq(ImmWord(JSVAL_PAYLOAD_MASK), ScratchReg);
            andq(ScratchReg, dest);
        } else {
            movq(ImmWord(JSVAL_PAYLOAD_MASK), dest);
            andq(src.valueReg(), dest);
        }
    }
    void unboxNonDouble(const Operand &src, const Register &dest) {
        // Explicitly permits |dest| to be used in |src|.
        JS_ASSERT(dest != ScratchReg);
        movq(ImmWord(JSVAL_PAYLOAD_MASK), ScratchReg);
        movq(src, dest);
        andq(ScratchReg, dest);
    }

    void unboxString(const ValueOperand &src, const Register &dest) { unboxNonDouble(src, dest); }
    void unboxString(const Operand &src, const Register &dest) { unboxNonDouble(src, dest); }

    void unboxObject(const ValueOperand &src, const Register &dest) { unboxNonDouble(src, dest); }
    void unboxObject(const Operand &src, const Register &dest) { unboxNonDouble(src, dest); }

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address &address, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        loadPtr(address, ScratchReg);
        unboxObject(ValueOperand(ScratchReg), scratch);
        return scratch;
    }
    Register extractObject(const ValueOperand &value, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        unboxObject(value, scratch);
        return scratch;
    }
    Register extractInt32(const ValueOperand &value, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        unboxInt32(value, scratch);
        return scratch;
    }
    Register extractTag(const Address &address, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        loadPtr(address, scratch);
        splitTag(scratch, scratch);
        return scratch;
    }
    Register extractTag(const ValueOperand &value, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        splitTag(value, scratch);
        return scratch;
    }

    void unboxValue(const ValueOperand &src, AnyRegister dest) {
        if (dest.isFloat()) {
            Label notInt32, end;
            branchTestInt32(Assembler::NotEqual, src, &notInt32);
            cvtsi2sd(src.valueReg(), dest.fpu());
            jump(&end);
            bind(&notInt32);
            unboxDouble(src, dest.fpu());
            bind(&end);
        } else {
            unboxNonDouble(src, dest.gpr());
        }
    }

    // These two functions use the low 32-bits of the full value register.
    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(Operand(operand.valueReg()), dest);
    }
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(Operand(operand.valueReg()), dest);
    }

    void loadConstantDouble(double d, const FloatRegister &dest) {
        union DoublePun {
            uint64_t u;
            double d;
        } pun;
        pun.d = d;
        if (!maybeInlineDouble(pun.u, dest)) {
            movq(ImmWord(pun.u), ScratchReg);
            movqsd(ScratchReg, dest);
        }
    }
    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        loadConstantDouble(*dp, dest);
    }

    Condition testInt32Truthy(bool truthy, const ValueOperand &operand) {
        testl(operand.valueReg(), operand.valueReg());
        return truthy ? NonZero : Zero;
    }
    void branchTestBooleanTruthy(bool truthy, const ValueOperand &operand, Label *label) {
        testl(operand.valueReg(), operand.valueReg());
        j(truthy ? NonZero : Zero, label);
    }
    // This returns the tag in ScratchReg.
    Condition testStringTruthy(bool truthy, const ValueOperand &value) {
        unboxString(value, ScratchReg);

        Operand lengthAndFlags(ScratchReg, JSString::offsetOfLengthAndFlags());
        movq(lengthAndFlags, ScratchReg);
        shrq(Imm32(JSString::LENGTH_SHIFT), ScratchReg);
        testq(ScratchReg, ScratchReg);
        return truthy ? Assembler::NonZero : Assembler::Zero;
    }


    void loadInt32OrDouble(const Operand &operand, const FloatRegister &dest) {
        Label notInt32, end;
        branchTestInt32(Assembler::NotEqual, operand, &notInt32);
        cvtsi2sd(operand, dest);
        jump(&end);
        bind(&notInt32);
        movsd(operand, dest);
        bind(&end);
    }

    template <typename T>
    void loadUnboxedValue(const T &src, MIRType type, AnyRegister dest) {
        if (dest.isFloat())
            loadInt32OrDouble(Operand(src), dest.fpu());
        else if (type == MIRType_Int32 || type == MIRType_Boolean)
            movl(Operand(src), dest.gpr());
        else
            unboxNonDouble(Operand(src), dest.gpr());
    }

    void loadInstructionPointerAfterCall(const Register &dest) {
        movq(Operand(StackPointer, 0x0), dest);
    }

    void convertUInt32ToDouble(const Register &src, const FloatRegister &dest) {
        cvtsq2sd(src, dest);
    }

    void inc64(AbsoluteAddress dest) {
        mov(ImmWord(dest.addr), ScratchReg);
        addPtr(Imm32(1), Address(ScratchReg, 0));
    }

    // Setup a call to C/C++ code, given the number of general arguments it
    // takes. Note that this only supports cdecl.
    //
    // In order for alignment to work correctly, the MacroAssembler must have a
    // consistent view of the stack displacement. It is okay to call "push"
    // manually, however, if the stack alignment were to change, the macro
    // assembler should be notified before starting a call.
    void setupAlignedABICall(uint32_t args);

    // Sets up an ABI call for when the alignment is not known. This may need a
    // scratch register.
    void setupUnalignedABICall(uint32_t args, const Register &scratch);

    // Arguments must be assigned to a C/C++ call in order. They are moved
    // in parallel immediately before performing the call. This process may
    // temporarily use more stack, in which case esp-relative addresses will be
    // automatically adjusted. It is extremely important that esp-relative
    // addresses are computed *after* setupABICall(). Furthermore, no
    // operations should be emitted while setting arguments.
    void passABIArg(const MoveOperand &from);
    void passABIArg(const Register &reg);
    void passABIArg(const FloatRegister &reg);

  private:
    void callWithABIPre(uint32_t *stackAdjust);
    void callWithABIPost(uint32_t stackAdjust, Result result);

  public:
    // Emits a call to a C/C++ function, resolving all argument moves.
    void callWithABI(void *fun, Result result = GENERAL);
    void callWithABI(Address fun, Result result = GENERAL);

    void handleException();

    void makeFrameDescriptor(Register frameSizeReg, FrameType type) {
        shlq(Imm32(FRAMESIZE_SHIFT), frameSizeReg);
        orq(Imm32(type), frameSizeReg);
    }

    // Save an exit frame (which must be aligned to the stack pointer) to
    // ThreadData::ionTop.
    void linkExitFrame() {
        mov(ImmWord(GetIonContext()->compartment->rt), ScratchReg);
        mov(StackPointer, Operand(ScratchReg, offsetof(JSRuntime, ionTop)));
    }

    void callWithExitFrame(IonCode *target, Register dynStack) {
        addPtr(Imm32(framePushed()), dynStack);
        makeFrameDescriptor(dynStack, IonFrame_OptimizedJS);
        Push(dynStack);
        call(target);
    }

    void enterOsr(Register calleeToken, Register code) {
        push(Imm32(0)); // num actual args.
        push(calleeToken);
        push(Imm32(MakeFrameDescriptor(0, IonFrame_Osr)));
        call(code);
        addq(Imm32(sizeof(uintptr_t) * 2), rsp);
    }
};

typedef MacroAssemblerX64 MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_x64_h__

