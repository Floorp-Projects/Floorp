/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
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

#include "BytecodeAnalyzer.h"
#include "jsautooplen.h"
#include "jsemit.h"
#include "Retcon.h"

using namespace js;

BytecodeAnalyzer::~BytecodeAnalyzer()
{
    cx->free(ops);
}

bool
BytecodeAnalyzer::addEdge(jsbytecode *pc, int32 offset, uint32 stackDepth)
{
    uint32 idx = (uint32)((pc + offset) - script->code);

    JS_ASSERT_IF(ops[idx].visited || ops[idx].nincoming,
                 ops[idx].stackDepth == stackDepth);

    if (!ops[idx].visited && !doList.append(pc + offset))
        return false;

    ops[idx].stackDepth = stackDepth;
    ops[idx].nincoming++;

    return true;
}

bool
BytecodeAnalyzer::analyze(uint32 index)
{
    mjit::AutoScriptRetrapper trapper(cx, script);
    jsbytecode *pc = doList[index];
    uint32 stackDepth = ops[pc - script->code].stackDepth;

    for (;;) {
        JSOp op = JSOp(pc[0]);
        OpcodeStatus &status = ops[pc - script->code];

        if (status.visited)
            return true;

        status.visited = true;
        status.stackDepth = stackDepth;

        if (op == JSOP_TRAP) {
            status.trap = true;
            if (!trapper.untrap(pc))
                return false;
            op = JSOp(pc[0]);
        }

        uint32 nuses, ndefs;
        if (js_CodeSpec[op].nuses == -1)
            nuses = js_GetVariableStackUses(op, pc);
        else
            nuses = js_CodeSpec[op].nuses;

        if (js_CodeSpec[op].ndefs == -1)
            ndefs = js_GetEnterBlockStackDefs(cx, script, pc);
        else
            ndefs = js_CodeSpec[op].ndefs;

        JS_ASSERT(nuses <= stackDepth);
        stackDepth -= nuses;
        stackDepth += ndefs;

        uint32 offs;
        jsbytecode *newpc;
        switch (op) {
          case JSOP_TRAP:
            return false;

          case JSOP_SETRVAL:
          case JSOP_POPV:
            usesRval = true;
            break;

          case JSOP_NAME:
          case JSOP_CALLNAME:
          case JSOP_BINDNAME:
          case JSOP_SETNAME:
          case JSOP_DELNAME:
          case JSOP_INCNAME:
          case JSOP_DECNAME:
          case JSOP_NAMEINC:
          case JSOP_NAMEDEC:
            usesScope = true;
            break;

          case JSOP_DEFAULT:
          case JSOP_GOTO:
            offs = (pc + JSOP_GOTO_LENGTH) - script->code;
            if (!ops[offs].visited && ops[offs].nincoming && !doList.append(pc + JSOP_GOTO_LENGTH))
                return false;
            pc += GET_JUMP_OFFSET(pc);
            ops[pc - script->code].nincoming++;
            continue;

          case JSOP_DEFAULTX:
          case JSOP_GOTOX:
            offs = (pc + JSOP_GOTOX_LENGTH) - script->code;
            if (!ops[offs].visited && ops[offs].nincoming && !doList.append(pc + JSOP_GOTOX_LENGTH))
                return false;
            pc += GET_JUMPX_OFFSET(pc);
            ops[pc - script->code].nincoming++;
            continue;

          case JSOP_IFEQ:
          case JSOP_IFNE:
            if (!addEdge(pc, GET_JUMP_OFFSET(pc), stackDepth))
                return false;
            break;

          case JSOP_OR:
          case JSOP_AND:
            /* If the jump is taken, the condition is pushed. */
            if (!addEdge(pc, GET_JUMP_OFFSET(pc), stackDepth + 1))
                return false;
            break;

          case JSOP_IFEQX:
          case JSOP_IFNEX:
            if (!addEdge(pc, GET_JUMPX_OFFSET(pc), stackDepth))
                return false;
            break;

          case JSOP_ORX:
          case JSOP_ANDX:
            if (!addEdge(pc, GET_JUMPX_OFFSET(pc), stackDepth + 1))
                return false;
            break;

          case JSOP_CASE:
            /* If the jump is taken, the extra value is not pushed. */
            if (!addEdge(pc, GET_JUMP_OFFSET(pc), stackDepth - 1))
                return false;
            break;

          case JSOP_CASEX:
            /* If the jump is taken, the extra value is not pushed. */
            if (!addEdge(pc, GET_JUMPX_OFFSET(pc), stackDepth - 1))
                return false;
            break;

          case JSOP_GOSUB:
          case JSOP_GOSUBX:
          case JSOP_IFPRIMTOP:
          case JSOP_FILTER:
          case JSOP_ENDFILTER:
          case JSOP_TABLESWITCHX:
          case JSOP_LOOKUPSWITCHX:
            return false;

          case JSOP_TABLESWITCH:
          {
            jsint def = GET_JUMP_OFFSET(pc);
            if (!addEdge(pc, def, stackDepth))
                return false;

            newpc = pc + JUMP_OFFSET_LEN;
            jsint low = GET_JUMP_OFFSET(newpc);
            newpc += JUMP_OFFSET_LEN;
            jsint high = GET_JUMP_OFFSET(newpc);
            newpc += JUMP_OFFSET_LEN;
            uint32 ncases = (uint32)(high - low + 1);

            for (uint32 i = 0; i < ncases; i++) {
                jsint offs = GET_JUMP_OFFSET(newpc);
                newpc += JUMP_OFFSET_LEN;
                if (!offs)
                    offs = def;
                if (!addEdge(pc, offs, stackDepth))
                    return false;
            }
            pc = newpc + 1;

            break;
          }

          case JSOP_LOOKUPSWITCH:
          {
            if (!addEdge(pc, GET_JUMP_OFFSET(pc), stackDepth))
                return false;

            newpc = pc + JUMP_OFFSET_LEN;
            uint32 npairs = GET_UINT16(newpc);
            newpc += UINT16_LEN;

            JS_ASSERT(npairs);
            for (uint32 i = 0; i < npairs; i++) {
                newpc += INDEX_LEN ;
                if (!addEdge(pc, GET_JUMP_OFFSET(newpc), stackDepth))
                    return false;
                newpc += JUMP_OFFSET_LEN;
            }
            pc = newpc + 1;

            break;
          }

          case JSOP_RETRVAL:
          case JSOP_RETURN:
          {
            /*
             * If there is nothing incoming, just leave.
             * This is to defeat the emitter doing things like:
             *   leaveblock 1
             *   retrval
             *   leaveblock 1
             * (see testNullCallee in trace-tests)
             */
            JS_ASSERT(js_CodeSpec[op].length == 1);
            uint32 offs = (pc + 1) - script->code;
            if (ops[offs].visited || !ops[offs].nincoming)
                return true;

            /* Otherwise, restore the stack depth and continue. */
            stackDepth = ops[offs].stackDepth;
            break;
          }

          case JSOP_THROW:
            /* Control flow stops here. */
            return true;

          case JSOP_STOP:
            JS_ASSERT(uint32(pc - script->code) + 1 == script->length);
            return true;

          default:
#ifdef DEBUG
            uint32 type = JOF_TYPE(js_CodeSpec[op].format);
            JS_ASSERT(type != JOF_JUMP && type != JOF_JUMPX);
#endif
            break;
        }

        if (js_CodeSpec[op].length != -1)
            pc += js_CodeSpec[op].length;
    }
}

bool
BytecodeAnalyzer::analyze()
{
    ops = (OpcodeStatus *)cx->malloc(sizeof(OpcodeStatus) * script->length);
    if (!ops)
        return false;
    memset(ops, 0, sizeof(OpcodeStatus) * script->length);

    if (!doList.append(script->code))
        return false;

    if (script->trynotesOffset) {
        JSTryNoteArray *tnarray = script->trynotes();
        for (unsigned i = 0; i < tnarray->length; ++i) {
            JSTryNote &tn = tnarray->vector[i];
            unsigned pcstart = script->main + tn.start - script->code;
            unsigned pcoff = pcstart + tn.length;

            for (unsigned j = pcstart; j < pcoff; j++)
                ops[j].inTryBlock = true;

            if (tn.kind == JSTRY_ITER)
                continue;
            
            ops[pcoff].exceptionEntry = true;
            ops[pcoff].nincoming = 1;
            ops[pcoff].stackDepth = tn.stackDepth;
            if (!doList.append(script->code + pcoff))
                return false;
        }
    }

    for (size_t i = 0; i < doList.length(); i++) {
        if (ops[doList[i] - script->code].visited)
            continue;
        if (!analyze(i))
            return false;
    }

    return true;
}

