/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SpiderMonkey JaegerMonkey implementation
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2002-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsautooplen.h"

#include "frontend/BytecodeEmitter.h"

namespace js {

static inline PropertyName *
GetNameFromBytecode(JSContext *cx, jsbytecode *pc, JSOp op, const JSCodeSpec &cs)
{
    if (op == JSOP_LENGTH)
        return cx->runtime->atomState.lengthAtom;

    // The method JIT's implementation of instanceof contains an internal lookup
    // of the prototype property.
    if (op == JSOP_INSTANCEOF)
        return cx->runtime->atomState.classPrototypeAtom;

    JSScript *script = cx->stack.currentScriptWithDiagnostics();
    PropertyName *name;
    GET_NAME_FROM_BYTECODE(script, pc, 0, name);
    return name;
}

class BytecodeRange {
  public:
    BytecodeRange(JSScript *script)
      : script(script), pc(script->code), end(pc + script->length) {}
    bool empty() const { return pc == end; }
    jsbytecode *frontPC() const { return pc; }
    JSOp frontOpcode() const { return JSOp(*pc); }
    size_t frontOffset() const { return pc - script->code; }
    void popFront() { pc += GetBytecodeLength(pc); }

  private:
    JSScript *script;
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
