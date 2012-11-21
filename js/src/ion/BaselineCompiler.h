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
# include "arm/BaselineCompiler-arm.h"
#endif

namespace js {
namespace ion {

#define OPCODE_LIST(_)         \
    _(JSOP_NOP)                \
    _(JSOP_LABEL)              \
    _(JSOP_NOTEARG)            \
    _(JSOP_POP)                \
    _(JSOP_DUP)                \
    _(JSOP_GOTO)               \
    _(JSOP_IFEQ)               \
    _(JSOP_IFNE)               \
    _(JSOP_AND)                \
    _(JSOP_OR)                 \
    _(JSOP_POS)                \
    _(JSOP_LOOPHEAD)           \
    _(JSOP_LOOPENTRY)          \
    _(JSOP_VOID)               \
    _(JSOP_UNDEFINED)          \
    _(JSOP_HOLE)               \
    _(JSOP_NULL)               \
    _(JSOP_TRUE)               \
    _(JSOP_FALSE)              \
    _(JSOP_ZERO)               \
    _(JSOP_ONE)                \
    _(JSOP_INT8)               \
    _(JSOP_INT32)              \
    _(JSOP_UINT16)             \
    _(JSOP_UINT24)             \
    _(JSOP_DOUBLE)             \
    _(JSOP_STRING)             \
    _(JSOP_ADD)                \
    _(JSOP_LT)                 \
    _(JSOP_GT)                 \
    _(JSOP_GETELEM)            \
    _(JSOP_SETELEM)            \
    _(JSOP_GETLOCAL)           \
    _(JSOP_CALLLOCAL)          \
    _(JSOP_SETLOCAL)           \
    _(JSOP_GETARG)             \
    _(JSOP_CALLARG)            \
    _(JSOP_SETARG)             \
    _(JSOP_CALL)               \
    _(JSOP_FUNCALL)            \
    _(JSOP_FUNAPPLY)           \
    _(JSOP_NEW)                \
    _(JSOP_RETURN)             \
    _(JSOP_STOP)

class BaselineCompiler : public BaselineCompilerSpecific
{
    FixedList<Label> labels_;
    HeapLabel *return_;

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
    bool emitToBoolean();
    bool emitTest(bool branchIfTrue);
    bool emitAndOr(bool branchIfTrue);
    bool emitCall();
};

} // namespace ion
} // namespace js

#endif

