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
#include "BaselineFrameInfo.h"
#include "BaselineIC.h"
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

class BaselineCompiler
{
    JSContext *cx;
    JSScript *script;
    jsbytecode *pc;
    MacroAssembler masm;
    FixedList<Label> labels_;
    Label return_;

    FrameInfo frame;
    js::Vector<CacheData, 16, SystemAllocPolicy> caches_;

    Label *labelOf(jsbytecode *pc) {
        return &labels_[pc - script->code];
    }

  public:
    BaselineCompiler(JSContext *cx, JSScript *script);
    bool init();

    MethodStatus compile();

  private:
    bool allocateCache(const BaseCache &cache) {
        JS_ASSERT(cache.data.pc == pc);
        return caches_.append(cache.data);
    }

    MethodStatus emitBody();

    bool emitPrologue();
    bool emitEpilogue();

    void storeValue(const StackValue *source, const Address &dest,
                    const ValueOperand &scratch);

#define EMIT_OP(op) bool emit_##op();
    OPCODE_LIST(EMIT_OP)
#undef EMIT_OP
};

} // namespace ion
} // namespace js

#endif

