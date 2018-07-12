/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/JumpList.h"

#include "vm/BytecodeUtil.h"

using namespace js;
using namespace js::frontend;

void
JumpList::push(jsbytecode* code, ptrdiff_t jumpOffset)
{
    SET_JUMP_OFFSET(&code[jumpOffset], offset - jumpOffset);
    offset = jumpOffset;
}

void
JumpList::patchAll(jsbytecode* code, JumpTarget target)
{
    ptrdiff_t delta;
    for (ptrdiff_t jumpOffset = offset; jumpOffset != -1; jumpOffset += delta) {
        jsbytecode* pc = &code[jumpOffset];
        MOZ_ASSERT(IsJumpOpcode(JSOp(*pc)) || JSOp(*pc) == JSOP_LABEL);
        delta = GET_JUMP_OFFSET(pc);
        MOZ_ASSERT(delta < 0);
        ptrdiff_t span = target.offset - jumpOffset;
        SET_JUMP_OFFSET(pc, span);
    }
}
