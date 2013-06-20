/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsopcodeinlines_h
#define jsopcodeinlines_h

#include "jsautooplen.h"

#include "frontend/SourceNotes.h"

namespace js {

static inline unsigned
GetDefCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;

    /*
     * Add an extra pushed value for OR/AND opcodes, so that they are included
     * in the pushed array of stack values for type inference.
     */
    switch (JSOp(*pc)) {
      case JSOP_OR:
      case JSOP_AND:
        return 1;
      case JSOP_PICK:
        /*
         * Pick pops and pushes how deep it looks in the stack + 1
         * items. i.e. if the stack were |a b[2] c[1] d[0]|, pick 2
         * would pop b, c, and d to rearrange the stack to |a c[0]
         * d[1] b[2]|.
         */
        return (pc[1] + 1);
      default:
        return StackDefs(script, pc);
    }
}

static inline unsigned
GetUseCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;

    if (JSOp(*pc) == JSOP_PICK)
        return (pc[1] + 1);
    if (js_CodeSpec[*pc].nuses == -1)
        return StackUses(script, pc);
    return js_CodeSpec[*pc].nuses;
}

static inline bool
IsJumpOpcode(JSOp op)
{
    uint32_t type = JOF_TYPE(js_CodeSpec[op].format);

    /*
     * LABEL opcodes have type JOF_JUMP but are no-ops, don't treat them as
     * jumps to avoid degrading precision.
     */
    return type == JOF_JUMP && op != JSOP_LABEL;
}

static inline bool
BytecodeFallsThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_DEFAULT:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
        return false;
      case JSOP_GOSUB:
        /* These fall through indirectly, after executing a 'finally'. */
        return true;
      default:
        return true;
    }
}

class BytecodeRange {
  public:
    BytecodeRange(JSContext *cx, JSScript *script)
      : script(cx, script), pc(script->code), end(pc + script->length) {}
    bool empty() const { return pc == end; }
    jsbytecode *frontPC() const { return pc; }
    JSOp frontOpcode() const { return JSOp(*pc); }
    size_t frontOffset() const { return pc - script->code; }
    void popFront() { pc += GetBytecodeLength(pc); }

  private:
    RootedScript script;
    jsbytecode *pc, *end;
};

class SrcNoteLineScanner
{
    /* offset of the current JSOp in the bytecode */
    ptrdiff_t offset;

    /* next src note to process */
    jssrcnote *sn;

    /* line number of the current JSOp */
    uint32_t lineno;

    /*
     * Is the current op the first one after a line change directive? Note that
     * multiple ops may be "first" if a line directive is used to return to a
     * previous line (eg, with a for loop increment expression.)
     */
    bool lineHeader;

public:
    SrcNoteLineScanner(jssrcnote *sn, uint32_t lineno)
        : offset(0), sn(sn), lineno(lineno)
    {
    }

    /*
     * This is called repeatedly with always-advancing relpc values. The src
     * notes are tuples of <PC offset from prev src note, type, args>. Scan
     * through, updating the lineno, until the next src note is for a later
     * bytecode.
     *
     * When looking at the desired PC offset ('relpc'), the op is first in that
     * line iff there is a SRC_SETLINE or SRC_NEWLINE src note for that exact
     * bytecode.
     *
     * Note that a single bytecode may have multiple line-modifying notes (even
     * though only one should ever be needed.)
     */
    void advanceTo(ptrdiff_t relpc) {
        // Must always advance! If the same or an earlier PC is erroneously
        // passed in, we will already be past the relevant src notes
        JS_ASSERT_IF(offset > 0, relpc > offset);

        // Next src note should be for after the current offset
        JS_ASSERT_IF(offset > 0, SN_IS_TERMINATOR(sn) || SN_DELTA(sn) > 0);

        // The first PC requested is always considered to be a line header
        lineHeader = (offset == 0);

        if (SN_IS_TERMINATOR(sn))
            return;

        ptrdiff_t nextOffset;
        while ((nextOffset = offset + SN_DELTA(sn)) <= relpc && !SN_IS_TERMINATOR(sn)) {
            offset = nextOffset;
            SrcNoteType type = (SrcNoteType) SN_TYPE(sn);
            if (type == SRC_SETLINE || type == SRC_NEWLINE) {
                if (type == SRC_SETLINE)
                    lineno = js_GetSrcNoteOffset(sn, 0);
                else
                    lineno++;

                if (offset == relpc)
                    lineHeader = true;
            }

            sn = SN_NEXT(sn);
        }
    }

    bool isLineHeader() const {
        return lineHeader;
    }

    uint32_t getLine() const { return lineno; }
};

}

#endif /* jsopcodeinlines_h */
