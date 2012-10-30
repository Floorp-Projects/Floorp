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

class StackValue {
  public:
    enum Kind {
        Constant,
        Register,
        Stack,
        LocalSlot
#ifdef DEBUG
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
            //XXX: allow using ValueOperand here.
            struct Register r1;
            struct Register r2;
        } reg;
        struct {
            uint32_t local;
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
        return ValueOperand(data.reg.r1, data.reg.r2);
    }
    uint32_t localSlot() const {
        JS_ASSERT(kind_ == LocalSlot);
        return data.local.local;
    }

    void setConstant(const Value &v) {
        kind_ = Constant;
        data.constant.v = v;
    }
    void setRegister(const ValueOperand &val) {
        kind_ = Register;
        data.reg.r1 = val.typeReg();
        data.reg.r2 = val.payloadReg();
    }
    void setLocalSlot(uint32_t local) {
        kind_ = LocalSlot;
        data.local.local = local;
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

    size_t stackDepth() const {
        return spIndex;
    }
    inline StackValue *peek(int32_t index) const {
        JS_ASSERT(index < 0);
        return const_cast<StackValue *>(&stack[spIndex + index]);
    }
    inline void pop() {
        --spIndex;
        StackValue *val = &stack[spIndex];

        if (val->kind() == StackValue::Stack) {
            JS_NOT_REACHED("NYI");
        }
    }
    inline void popn(uint32_t n) {
        for (uint32_t i = 0; i < n; i++)
            pop();
    }
    inline StackValue *rawPush() {
        StackValue *val = &stack[spIndex++];
        val->reset();
        return val;
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
        JS_ASSERT(local < script->nfixed);
        return Address(frameReg, -BasicFrame::offsetOfLocal(local));
    }

    void ensureInRegister(StackValue *val, ValueOperand dest, ValueOperand scratch);

    void sync(StackValue *val);
    void syncStack(uint32_t uses);
    void popRegsAndSync(uint32_t uses);
};

} // namespace ion
} // namespace js

#endif

