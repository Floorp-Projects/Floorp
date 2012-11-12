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
#include "BaselineIC.h"
#include "FixedList.h"

#if defined(JS_CPU_X86)
# include "x86/BaselineCompiler-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/BaselineCompiler-x64.h"
#else
#error "CPU Not Supported"
#endif

namespace js {
namespace ion {

#define OPCODE_LIST(_)         \
    _(JSOP_NOP)                \
    _(JSOP_POP)                \
    _(JSOP_DUP)                \
    _(JSOP_GOTO)               \
    _(JSOP_IFNE)               \
    _(JSOP_POS)                \
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
    _(JSOP_GT)                 \
    _(JSOP_GETLOCAL)           \
    _(JSOP_SETLOCAL)           \
    _(JSOP_GETARG)             \
    _(JSOP_SETARG)             \
    _(JSOP_RETURN)             \
    _(JSOP_STOP)

class BaselineCompiler : public BaselineCompilerSpecific
{
    FixedList<Label> labels_;
    Label return_;

    Label *labelOf(jsbytecode *pc) {
        return &labels_[pc - script->code];
    }

  public:
    BaselineCompiler(JSContext *cx, JSScript *script);
    bool init();

    MethodStatus compile();

  private:
    MethodStatus emitBody();

    bool emitPrologue();
    bool emitEpilogue();

    void storeValue(const StackValue *source, const Address &dest,
                    const ValueOperand &scratch);

#define EMIT_OP(op) bool emit_##op();
    OPCODE_LIST(EMIT_OP)
#undef EMIT_OP

    // Handles JSOP_LT, JSOP_GT, and friends
    bool emitCompare();
};

} // namespace ion
} // namespace js

#endif

