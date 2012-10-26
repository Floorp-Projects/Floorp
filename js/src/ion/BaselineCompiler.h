/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(jsion_baseline_compiler_h__) && defined(JS_ION)
#define jsion_baseline_compiler_h__

#include "jscntxt.h"
#include "jscompartment.h"
#include "IonCode.h"
#include "jsinfer.h"
#include "jsinterp.h"

#include "BaselineJIT.h"
#include "ion/IonMacroAssembler.h"
#include "FixedList.h"

namespace js {
namespace ion {

#define OPCODE_LIST(_)         \
    _(JSOP_NOP)                \
    _(JSOP_POP)                \
    _(JSOP_GOTO)               \
    _(JSOP_IFNE)               \
    _(JSOP_LOOPHEAD)           \
    _(JSOP_LOOPENTRY)          \
    _(JSOP_ZERO)               \
    _(JSOP_ONE)                \
    _(JSOP_INT8)               \
    _(JSOP_INT32)              \
    _(JSOP_UINT16)             \
    _(JSOP_UINT24)             \
    _(JSOP_ADD)                \
    _(JSOP_LT)                 \
    _(JSOP_GETLOCAL)           \
    _(JSOP_SETLOCAL)           \
    _(JSOP_RETURN)             \
    _(JSOP_STOP)

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

class FrameState
{
    JSContext *cx;
    JSScript *script;

    MacroAssembler &masm;

    FixedList<StackValue> stack;
    size_t spIndex;

  public:
    FrameState(JSContext *cx, JSScript *script, MacroAssembler &masm)
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

class BaselineCompiler
{
    JSContext *cx;
    JSScript *script;
    jsbytecode *pc;
    MacroAssembler masm;
    Label *labels_;
    Label return_;

    FrameState frame;
    js::Vector<CacheData, 16, SystemAllocPolicy> caches_;

    Label *labelOf(jsbytecode *pc) const {
        return labels_ + (pc - script->code);
    }

  public:
    BaselineCompiler(JSContext *cx, JSScript *script);
    bool init();

    bool allocateCache(const BaseCache &cache) {
        JS_ASSERT(cache.data.pc == pc);
        return caches_.append(cache.data);
    }

    MethodStatus compile();
    MethodStatus emitBody();

    bool emitPrologue();
    bool emitEpilogue();

    void loadValue(const StackValue *source, const ValueOperand &dest);
    void storeValue(const StackValue *source, const Address &dest);

#define EMIT_OP(op) bool emit_##op();
    OPCODE_LIST(EMIT_OP)
#undef EMIT_OP
};

} // namespace ion
} // namespace js

#endif

