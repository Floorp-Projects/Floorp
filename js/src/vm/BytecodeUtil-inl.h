/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_BytecodeUtil_inl_h
#define vm_BytecodeUtil_inl_h

#include "vm/BytecodeUtil.h"

#include "frontend/SourceNotes.h"
#include "vm/JSScript.h"

namespace js {

static inline unsigned
GetDefCount(jsbytecode* pc)
{
    /*
     * Add an extra pushed value for OR/AND opcodes, so that they are included
     * in the pushed array of stack values for type inference.
     */
    switch (JSOp(*pc)) {
      case JSOP_OR:
      case JSOP_AND:
        return 1;
      case JSOP_PICK:
      case JSOP_UNPICK:
        /*
         * Pick pops and pushes how deep it looks in the stack + 1
         * items. i.e. if the stack were |a b[2] c[1] d[0]|, pick 2
         * would pop b, c, and d to rearrange the stack to |a c[0]
         * d[1] b[2]|.
         */
        return pc[1] + 1;
      default:
        return StackDefs(pc);
    }
}

static inline unsigned
GetUseCount(jsbytecode* pc)
{
    if (JSOp(*pc) == JSOP_PICK || JSOp(*pc) == JSOP_UNPICK)
        return pc[1] + 1;

    return StackUses(pc);
}

static inline JSOp
ReverseCompareOp(JSOp op)
{
    switch (op) {
      case JSOP_GT:
        return JSOP_LT;
      case JSOP_GE:
        return JSOP_LE;
      case JSOP_LT:
        return JSOP_GT;
      case JSOP_LE:
        return JSOP_GE;
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
        return op;
      default:
        MOZ_CRASH("unrecognized op");
    }
}

static inline JSOp
NegateCompareOp(JSOp op)
{
    switch (op) {
      case JSOP_GT:
        return JSOP_LE;
      case JSOP_GE:
        return JSOP_LT;
      case JSOP_LT:
        return JSOP_GE;
      case JSOP_LE:
        return JSOP_GT;
      case JSOP_EQ:
        return JSOP_NE;
      case JSOP_NE:
        return JSOP_EQ;
      case JSOP_STRICTNE:
        return JSOP_STRICTEQ;
      case JSOP_STRICTEQ:
        return JSOP_STRICTNE;
      default:
        MOZ_CRASH("unrecognized op");
    }
}

class BytecodeRange {
  public:
    BytecodeRange(JSContext* cx, JSScript* script)
      : script(cx, script), pc(script->code()), end(pc + script->length())
    {}
    bool empty() const { return pc == end; }
    jsbytecode* frontPC() const { return pc; }
    JSOp frontOpcode() const { return JSOp(*pc); }
    size_t frontOffset() const { return script->pcToOffset(pc); }
    void popFront() { pc += GetBytecodeLength(pc); }

  private:
    RootedScript script;
    jsbytecode* pc;
    jsbytecode* end;
};

class BytecodeRangeWithPosition : private BytecodeRange
{
  public:
    using BytecodeRange::empty;
    using BytecodeRange::frontPC;
    using BytecodeRange::frontOpcode;
    using BytecodeRange::frontOffset;

    BytecodeRangeWithPosition(JSContext* cx, JSScript* script)
      : BytecodeRange(cx, script), lineno(script->lineno()), column(0),
        sn(script->notes()), snpc(script->code()), isEntryPoint(false),
        wasArtifactEntryPoint(false)
    {
        if (!SN_IS_TERMINATOR(sn))
            snpc += SN_DELTA(sn);
        updatePosition();
        while (frontPC() != script->main())
            popFront();

        if (frontOpcode() != JSOP_JUMPTARGET)
            isEntryPoint = true;
        else
            wasArtifactEntryPoint =  true;
    }

    void popFront() {
        BytecodeRange::popFront();
        if (empty())
            isEntryPoint = false;
        else
            updatePosition();

        // The following conditions are handling artifacts introduced by the
        // bytecode emitter, such that we do not add breakpoints on empty
        // statements of the source code of the user.
        if (wasArtifactEntryPoint) {
            wasArtifactEntryPoint = false;
            isEntryPoint = true;
        }

        if (isEntryPoint && frontOpcode() == JSOP_JUMPTARGET) {
            wasArtifactEntryPoint = isEntryPoint;
            isEntryPoint = false;
        }
    }

    size_t frontLineNumber() const { return lineno; }
    size_t frontColumnNumber() const { return column; }

    // Entry points are restricted to bytecode offsets that have an
    // explicit mention in the line table.  This restriction avoids a
    // number of failing cases caused by some instructions not having
    // sensible (to the user) line numbers, and it is one way to
    // implement the idea that the bytecode emitter should tell the
    // debugger exactly which offsets represent "interesting" (to the
    // user) places to stop.
    bool frontIsEntryPoint() const { return isEntryPoint; }

  private:
    void updatePosition() {
        // Determine the current line number by reading all source notes up to
        // and including the current offset.
        jsbytecode *lastLinePC = nullptr;
        while (!SN_IS_TERMINATOR(sn) && snpc <= frontPC()) {
            SrcNoteType type = SN_TYPE(sn);
            if (type == SRC_COLSPAN) {
                ptrdiff_t colspan = SN_OFFSET_TO_COLSPAN(GetSrcNoteOffset(sn, 0));
                MOZ_ASSERT(ptrdiff_t(column) + colspan >= 0);
                column += colspan;
                lastLinePC = snpc;
            } else if (type == SRC_SETLINE) {
                lineno = size_t(GetSrcNoteOffset(sn, 0));
                column = 0;
                lastLinePC = snpc;
            } else if (type == SRC_NEWLINE) {
                lineno++;
                column = 0;
                lastLinePC = snpc;
            }

            sn = SN_NEXT(sn);
            snpc += SN_DELTA(sn);
        }
        isEntryPoint = lastLinePC == frontPC();
    }

    size_t lineno;
    size_t column;
    jssrcnote* sn;
    jsbytecode* snpc;
    bool isEntryPoint;
    bool wasArtifactEntryPoint;
};

} // namespace js

#endif /* vm_BytecodeUtil_inl_h */
