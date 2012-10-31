/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_frameinfo_h__) && defined(JS_ION)
#define jsion_baseline_frameinfo_h__

#include "jscntxt.h"
#include "jscompartment.h"

#include "BaselineJIT.h"
#include "ion/IonMacroAssembler.h"
#include "FixedList.h"

namespace js {
namespace ion {

// FrameInfo overview.
//
// FrameInfo is used by the compiler to track values stored in the frame. This
// includes locals, arguments and stack values. Locals and arguments are always
// fully synced. Stack values can either be synced, stored as constant, stored in
// a Value register or refer to a local slot. Syncing a StackValue ensures it's
// stored on the stack, e.g. kind == Stack.
//
// To see how this works, consider the following statement:
//
//    var y = x + 9;
//
// Here two values are pushed: StackValue(LocalSlot(0)) and StackValue(Int32Value(9)).
// Only when we reach the ADD op, code is generated to load the operands directly
// into the right operand registers and sync all other stack values.
//
// For stack values, the following invariants hold (and are checked between ops):
//
// (1) If a value is synced (kind == Stack), all values below it must also be synced.
//     In other words, values with kind other than Stack can only appear on top of the
//     abstract stack.
//
// (2) When we call a stub or IC, all values still on the stack must be synced.

// Represents a value pushed on the stack. Note that StackValue is not used for
// locals or arguments since these are always fully synced.
class StackValue
{
  public:
    enum Kind {
        Constant,
        Register,
        Stack,
        LocalSlot
#ifdef DEBUG
        // In debug builds, assert Kind is initialized.
        , Uninitialized
#endif
    };

  private:
    Kind kind_;

    union {
        struct {
            Value v;
        } constant;
        struct {
            AlignedStorage2<ValueOperand> reg;
        } reg;
        struct {
            uint32_t slot;
        } local;
    } data;

  public:
    StackValue() {
        reset();
    }

    Kind kind() const {
        return kind_;
    }
    void reset() {
#ifdef DEBUG
        kind_ = Uninitialized;
#endif
    }
    Value constant() const {
        JS_ASSERT(kind_ == Constant);
        return data.constant.v;
    }
    ValueOperand reg() const {
        JS_ASSERT(kind_ == Register);
        return *data.reg.reg.addr();
    }
    uint32_t localSlot() const {
        JS_ASSERT(kind_ == LocalSlot);
        return data.local.slot;
    }

    void setConstant(const Value &v) {
        kind_ = Constant;
        data.constant.v = v;
    }
    void setRegister(const ValueOperand &val) {
        kind_ = Register;
        *data.reg.reg.addr() = val;
    }
    void setLocalSlot(uint32_t slot) {
        kind_ = LocalSlot;
        data.local.slot = slot;
    }
    void setStack() {
        kind_ = Stack;
    }
};

static const Register frameReg = ebp;
static const Register spReg = StackPointer;

static const ValueOperand R0(ecx, edx);
static const ValueOperand R1(eax, ebx);
static const ValueOperand R2(esi, edi);

class BasicFrame
{
    uint32_t dummy1;
    uint32_t dummy2;

  public:
    static inline size_t offsetOfLocal(unsigned index) {
        return sizeof(BasicFrame) + index * sizeof(Value);
    }
};

class FrameInfo
{
    JSContext *cx;
    JSScript *script;

    MacroAssembler &masm;

    FixedList<StackValue> stack;
    size_t spIndex;

  public:
    FrameInfo(JSContext *cx, JSScript *script, MacroAssembler &masm)
      : cx(cx),
        script(script),
        masm(masm),
        stack(),
        spIndex(0)
    { }

    bool init();

  private:
    inline StackValue *rawPush() {
        StackValue *val = &stack[spIndex++];
        val->reset();
        return val;
    }

    uint32_t nlocals() const {
        return script->nfixed;
    }

  public:
    inline size_t stackDepth() const {
        return spIndex;
    }
    inline StackValue *peek(int32_t index) const {
        JS_ASSERT(index < 0);
        return const_cast<StackValue *>(&stack[spIndex + index]);
    }
    inline void pop(bool adjustStack = true) {
        spIndex--;
        StackValue *popped = &stack[spIndex];

        if (adjustStack && popped->kind() == StackValue::Stack)
            masm.addPtr(Imm32(sizeof(Value)), spReg);

        // Assert when anything uses this value.
        popped->reset();
    }
    inline void popn(uint32_t n) {
        for (uint32_t i = 0; i < n; i++)
            pop();
    }
    inline void push(const Value &val) {
        StackValue *sv = rawPush();
        sv->setConstant(val);
    }
    inline void push(const ValueOperand &val) {
        StackValue *sv = rawPush();
        sv->setRegister(val);
    }
    inline void pushLocal(uint32_t local) {
        StackValue *sv = rawPush();
        sv->setLocalSlot(local);
    }
    inline Address addressOfLocal(size_t local) const {
        JS_ASSERT(local < nlocals());
        return Address(frameReg, -BasicFrame::offsetOfLocal(local));
    }
    inline Address addressOfStackValue(const StackValue *value) const {
        JS_ASSERT(value->kind() == StackValue::Stack);
        size_t slot = value - &stack[0];
        JS_ASSERT(slot < stackDepth());
        return Address(frameReg, -BasicFrame::offsetOfLocal(nlocals() + slot));
    }

    void popValue(ValueOperand dest);

    void sync(StackValue *val);
    void syncStack(uint32_t uses);
    void popRegsAndSync(uint32_t uses);

#ifdef DEBUG
    // Assert the state is valid before excuting "pc".
    void assertValidState(jsbytecode *pc);
#else
    inline void assertValidState(jsbytecode *pc) {}
#endif
};

} // namespace ion
} // namespace js

#endif

