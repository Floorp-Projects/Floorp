/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineFrameInfo.h"
#include "IonSpewer.h"

#include "jsanalyze.h"
#include "jsinferinlines.h"

using namespace js;
using namespace js::ion;

bool
FrameInfo::init() {
    // One slot is always needed for this/arguments type checks.
    size_t nstack = Max(script->nslots - script->nfixed, 1);
    if (!stack.init(nstack))
        return false;

    return true;
}

void
FrameInfo::sync(StackValue *val)
{
    switch (val->kind()) {
      case StackValue::Stack:
        break;
      case StackValue::LocalSlot:
        masm.pushValue(addressOfLocal(val->localSlot()));
        break;
      case StackValue::ArgSlot:
        masm.pushValue(addressOfArg(val->argSlot()));
        break;
      case StackValue::ThisSlot:
        masm.pushValue(addressOfThis());
        break;
      case StackValue::Register:
        masm.pushValue(val->reg());
        break;
      case StackValue::Constant:
        masm.pushValue(val->constant());
        break;
      default:
        JS_NOT_REACHED("Invalid kind");
        break;
    }

    val->setStack();
}

void
FrameInfo::syncStack(uint32_t uses)
{
    JS_ASSERT(uses <= stackDepth());

    uint32_t depth = stackDepth() - uses;

    for (uint32_t i = 0; i < depth; i++) {
        StackValue *current = &stack[i];
        sync(current);
    }
}

uint32_t
FrameInfo::numUnsyncedSlots()
{
    // Start at the bottom, find the first value that's not synced.
    uint32_t i = 0;
    for (; i < stackDepth(); i++) {
        if (peek(-int32_t(i + 1))->kind() == StackValue::Stack)
            break;
    }
    return i;
}

void
FrameInfo::popValue(ValueOperand dest)
{
    StackValue *val = peek(-1);

    switch (val->kind()) {
      case StackValue::Constant:
        masm.moveValue(val->constant(), dest);
        break;
      case StackValue::LocalSlot:
        masm.loadValue(addressOfLocal(val->localSlot()), dest);
        break;
      case StackValue::ArgSlot:
        masm.loadValue(addressOfArg(val->argSlot()), dest);
        break;
      case StackValue::ThisSlot:
        masm.loadValue(addressOfThis(), dest);
        break;
      case StackValue::Stack:
        masm.popValue(dest);
        break;
      case StackValue::Register:
        masm.moveValue(val->reg(), dest);
        break;
      default:
        JS_NOT_REACHED("Invalid kind");
    }

    // masm.popValue already adjusted the stack pointer, don't do it twice.
    pop(DontAdjustStack);
}

void
FrameInfo::popRegsAndSync(uint32_t uses)
{
    // x86 has only 3 Value registers. Only support 2 regs here for now,
    // so that there's always a scratch Value register for reg -> reg
    // moves.
    JS_ASSERT(uses > 0);
    JS_ASSERT(uses <= 2);
    JS_ASSERT(uses <= stackDepth());

    syncStack(uses);

    switch (uses) {
      case 1:
        popValue(R0);
        break;
      case 2: {
        // If the second value is in R1, move it to R2 so that it's not
        // clobbered by the first popValue.
        StackValue *val = peek(-2);
        if (val->kind() == StackValue::Register && val->reg() == R1) {
            masm.moveValue(R1, R2);
            val->setRegister(R2);
        }
        popValue(R1);
        popValue(R0);
        break;
      }
      default:
        JS_NOT_REACHED("Invalid uses");
    }
}

#ifdef DEBUG
void
FrameInfo::assertValidState(jsbytecode *pc)
{
    // Check stack depth.
    analyze::Bytecode *code = script->analysis()->maybeCode(pc);
    JS_ASSERT_IF(code, stackDepth() == code->stackDepth);

    // Start at the bottom, find the first value that's not synced.
    uint32_t i = 0;
    for (; i < stackDepth(); i++) {
        if (stack[i].kind() != StackValue::Stack)
            break;
    }

    // Assert all values on top of it are also not synced.
    for (; i < stackDepth(); i++)
        JS_ASSERT(stack[i].kind() != StackValue::Stack);

    // Assert every Value register is used by at most one StackValue.
    // R2 is used as scratch register by the compiler and FrameInfo,
    // so it shouldn't be used for StackValues.
    bool usedR0 = false, usedR1 = false;

    for (i = 0; i < stackDepth(); i++) {
        if (stack[i].kind() == StackValue::Register) {
            ValueOperand reg = stack[i].reg();
            if (reg == R0) {
                JS_ASSERT(!usedR0);
                usedR0 = true;
            } else if (reg == R1) {
                JS_ASSERT(!usedR1);
                usedR1 = true;
            } else {
                JS_NOT_REACHED("Invalid register");
            }
        }
    }
}
#endif
