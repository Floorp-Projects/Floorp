/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey bytecode analysis
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Hackett <bhackett@mozilla.com>
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

#include "jsanalyze.h"
#include "jsautooplen.h"
#include "jscompartment.h"
#include "jscntxt.h"

namespace js {
namespace analyze {

/////////////////////////////////////////////////////////////////////
// Script
/////////////////////////////////////////////////////////////////////

void
Script::destroy()
{
    JS_FinishArenaPool(&pool);
}

template <typename T>
inline T *
ArenaArray(JSArenaPool &pool, unsigned count)
{
    void *v;
    JS_ARENA_ALLOCATE(v, &pool, count * sizeof(T));
    return (T *) v;
}

template <typename T>
inline T *
ArenaNew(JSArenaPool &pool)
{
    void *v;
    JS_ARENA_ALLOCATE(v, &pool, sizeof(T));
    return new (v) T();
}

/////////////////////////////////////////////////////////////////////
// Bytecode
/////////////////////////////////////////////////////////////////////

bool
Bytecode::mergeDefines(JSContext *cx, Script *script, bool initial, unsigned newDepth,
                       uint32 *newArray, unsigned newCount)
{
    if (initial) {
        /*
         * Haven't handled any incoming edges to this bytecode before.
         * Define arrays are copy on write, so just reuse the array for this bytecode.
         */
        stackDepth = newDepth;
        defineArray = newArray;
        defineCount = newCount;
        return true;
    }

    /*
     * This bytecode has multiple incoming edges, intersect the new array with any
     * variables known to be defined along other incoming edges.
     */
    if (analyzed) {
#ifdef DEBUG
        /*
         * Once analyzed, a bytecode has its full set of definitions.  There are two
         * properties we depend on to ensure this.  First, bytecode for a function
         * is emitted in topological order, and since we analyze bytecodes in the
         * order they were emitted we will have seen all incoming jumps except
         * for any loop back edges.  Second, javascript has structured control flow,
         * so loop heads dominate their bodies; the set of variables defined
         * on a back edge will be at least as large as at the head of the loop,
         * so no intersection or further analysis needs to be done.
         */
        JS_ASSERT(stackDepth == newDepth);
        for (unsigned i = 0; i < defineCount; i++) {
            bool found = false;
            for (unsigned j = 0; j < newCount; j++) {
                if (newArray[j] == defineArray[i])
                    found = true;
            }
            JS_ASSERT(found);
        }
#endif
    } else {
        JS_ASSERT(stackDepth == newDepth);
        bool owned = false;
        for (unsigned i = 0; i < defineCount; i++) {
            bool found = false;
            for (unsigned j = 0; j < newCount; j++) {
                if (newArray[j] == defineArray[i])
                    found = true;
            }
            if (!found) {
                /*
                 * Get a mutable copy of the defines.  This can end up making
                 * several copies for a bytecode if it has many incoming edges
                 * with progressively smaller sets of defined variables.
                 */
                if (!owned) {
                    uint32 *reallocArray = ArenaArray<uint32>(script->pool, defineCount);
                    if (!reallocArray) {
                        script->setOOM(cx);
                        return false;
                    }
                    memcpy(reallocArray, defineArray, defineCount * sizeof(uint32));
                    defineArray = reallocArray;
                    owned = true;
                }

                /* Swap with the last element and pop the array. */
                defineArray[i--] = defineArray[--defineCount];
            }
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// Analysis
/////////////////////////////////////////////////////////////////////

inline bool
Script::addJump(JSContext *cx, unsigned offset,
                unsigned *currentOffset, unsigned *forwardJump,
                unsigned stackDepth, uint32 *defineArray, unsigned defineCount)
{
    JS_ASSERT(offset < script->length);

    Bytecode *&bytecode = code[offset];
    bool initial = (bytecode == NULL);
    if (initial) {
        bytecode = ArenaNew<Bytecode>(pool);
        if (!bytecode) {
            setOOM(cx);
            return false;
        }
    }

    if (!bytecode->mergeDefines(cx, this, initial, stackDepth, defineArray, defineCount))
        return false;
    bytecode->jumpTarget = true;

    if (offset < *currentOffset) {
        /* Don't follow back edges to bytecode which has already been analyzed. */
        if (!bytecode->analyzed) {
            if (*forwardJump == 0)
                *forwardJump = *currentOffset;
            *currentOffset = offset;
        }
    } else if (offset > *forwardJump) {
        *forwardJump = offset;
    }

    return true;
}

inline void
Script::setLocal(uint32 local, uint32 offset)
{
    JS_ASSERT(local < localCount());
    JS_ASSERT(offset != LOCAL_CONDITIONALLY_DEFINED);

    /*
     * It isn't possible to change the point when a variable becomes unconditionally
     * defined, or to mark it as unconditionally defined after it has already been
     * marked as having a use before def.  It *is* possible to mark it as having
     * a use before def after marking it as unconditionally defined.  In a loop such as:
     *
     * while ((a = b) != 0) { x = a; }
     *
     * When walking through the body of this loop, we will first analyze the test
     * (which comes after the body in the bytecode stream) as unconditional code,
     * and mark a as definitely defined.  a is not in the define array when taking
     * the loop's back edge, so it is treated as possibly undefined when written to x.
     */
    JS_ASSERT(locals[local] == LOCAL_CONDITIONALLY_DEFINED ||
              locals[local] == offset || offset == LOCAL_USE_BEFORE_DEF);

    locals[local] = offset;
}

static inline ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    uint32 type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

static inline unsigned
GetBytecodeLength(jsbytecode *pc)
{
    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);
    JS_ASSERT(op != JSOP_TRAP);

    if (js_CodeSpec[op].length != -1)
        return js_CodeSpec[op].length;
    return js_GetVariableBytecodeLength(pc);
}

// return whether op bytecodes do not fallthrough (they may do a jump).
static inline bool
BytecodeNoFallThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_FILTER:
        return true;
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
        // these fall through indirectly, after executing a 'finally'.
        return false;
      default:
        return false;
    }
}

/* Untrap a single PC, and retrap it at scope exit. */
struct UntrapOpcode
{
    jsbytecode *pc;
    bool trap;

    UntrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
        : pc(pc), trap(JSOp(*pc) == JSOP_TRAP)
    {
        if (trap)
            *pc = JS_GetTrapOpcode(cx, script, pc);
    }

    ~UntrapOpcode()
    {
        if (trap)
            *pc = JSOP_TRAP;
    }
};

void
Script::analyze(JSContext *cx, JSScript *script)
{
    JS_ASSERT(!code && !locals);
    this->script = script;

    JS_InitArenaPool(&pool, "script_analyze", 256, 8);

    unsigned length = script->length;
    unsigned nfixed = localCount();

    code = ArenaArray<Bytecode*>(pool, length);
    locals = ArenaArray<uint32>(pool, nfixed);

    if (!code || !locals) {
        setOOM(cx);
        return;
    }

    PodZero(code, length);

    for (unsigned i = 0; i < nfixed; i++)
        locals[i] = LOCAL_CONDITIONALLY_DEFINED;

    /*
     * Treat locals as having a possible use-before-def if they could be accessed
     * by debug code or by eval, or if they could be accessed by an inner script.
     */

    if (script->usesEval || cx->compartment->debugMode()) {
        for (uint32 i = 0; i < nfixed; i++)
            setLocal(i, LOCAL_USE_BEFORE_DEF);
    }

    for (uint32 i = 0; i < script->nClosedVars; i++) {
        uint32 slot = script->getClosedVar(i);
        if (slot < nfixed)
            setLocal(slot, LOCAL_USE_BEFORE_DEF);
    }

    /*
     * If the script is in debug mode, JS_SetFrameReturnValue can be called at
     * any safe point.
     */
    if (cx->compartment->debugMode())
        usesRval = true;

    /*
     * If we are in the middle of one or more jumps, the offset of the highest
     * target jumping over this bytecode.  Includes implicit jumps from
     * try/catch/finally blocks.
     */
    unsigned forwardJump = 0;

    /*
     * If we are in the middle of a try block, the offset of the highest
     * catch/finally/enditer.
     */
    unsigned forwardCatch = 0;

    /* Fill in stack depth and definitions at initial bytecode. */
    Bytecode *startcode = ArenaNew<Bytecode>(pool);
    if (!startcode) {
        setOOM(cx);
        return;
    }

    startcode->stackDepth = 0;
    code[0] = startcode;

    unsigned offset, nextOffset = 0;
    while (nextOffset < length) {
        offset = nextOffset;

        JS_ASSERT(forwardCatch <= forwardJump);

        /* Check if the current forward jump/try-block has finished. */
        if (forwardJump && forwardJump == offset)
            forwardJump = 0;
        if (forwardCatch && forwardCatch == offset)
            forwardCatch = 0;

        Bytecode *&bytecode = code[offset];
        jsbytecode *pc = script->code + offset;

        UntrapOpcode untrap(cx, script, pc);

        JSOp op = (JSOp)*pc;
        JS_ASSERT(op < JSOP_LIMIT);

        /* Immediate successor of this bytecode. */
        unsigned successorOffset = offset + GetBytecodeLength(pc);

        /*
         * Next bytecode to analyze.  This is either the successor, or is an
         * earlier bytecode if this bytecode has a loop backedge.
         */
        nextOffset = successorOffset;

        if (!bytecode) {
            /* Haven't found a path by which this bytecode is reachable. */
            continue;
        }

        if (bytecode->analyzed) {
            /* No need to reanalyze, see Bytecode::mergeDefines. */
            continue;
        }

        bytecode->analyzed = true;

        if (forwardCatch)
            bytecode->inTryBlock = true;

        unsigned stackDepth = bytecode->stackDepth;
        uint32 *defineArray = bytecode->defineArray;
        unsigned defineCount = bytecode->defineCount;

        if (!forwardJump) {
            /*
             * There is no jump over this bytecode, nor a containing try block.
             * Either this bytecode will definitely be executed, or an exception
             * will be thrown which the script does not catch.  Either way,
             * any variables definitely defined at this bytecode will stay
             * defined throughout the rest of the script.  We just need to
             * remember the offset where the variable became unconditionally
             * defined, rather than continue to maintain it in define arrays.
             */
            for (unsigned i = 0; i < defineCount; i++) {
                uint32 local = defineArray[i];
                JS_ASSERT_IF(locals[local] != LOCAL_CONDITIONALLY_DEFINED &&
                             locals[local] != LOCAL_USE_BEFORE_DEF,
                             locals[local] <= offset);
                if (locals[local] == LOCAL_CONDITIONALLY_DEFINED)
                    setLocal(local, offset);
            }
            defineArray = bytecode->defineArray = NULL;
            defineCount = bytecode->defineCount = 0;
        }

        unsigned nuses, ndefs;
        if (js_CodeSpec[op].nuses == -1)
            nuses = js_GetVariableStackUses(op, pc);
        else
            nuses = js_CodeSpec[op].nuses;

        if (js_CodeSpec[op].ndefs == -1)
            ndefs = js_GetEnterBlockStackDefs(cx, script, pc);
        else
            ndefs = js_CodeSpec[op].ndefs;

        JS_ASSERT(stackDepth >= nuses);
        stackDepth -= nuses;
        stackDepth += ndefs;

        switch (op) {

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

          /* Watch for opcodes the method JIT doesn't compile. */
          case JSOP_GOSUB:
          case JSOP_GOSUBX:
          case JSOP_IFCANTCALLTOP:
          case JSOP_FILTER:
          case JSOP_ENDFILTER:
          case JSOP_TABLESWITCHX:
          case JSOP_LOOKUPSWITCHX:
            hadFailure = true;
            return;

          case JSOP_TABLESWITCH: {
            jsbytecode *pc2 = pc;
            unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
            pc2 += JUMP_OFFSET_LEN;
            jsint low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            jsint high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump,
                         stackDepth, defineArray, defineCount)) {
                return;
            }

            for (jsint i = low; i <= high; i++) {
                unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
                if (targetOffset != offset) {
                    if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                                 stackDepth, defineArray, defineCount)) {
                        return;
                    }
                }
                pc2 += JUMP_OFFSET_LEN;
            }
            break;
          }

          case JSOP_LOOKUPSWITCH: {
            jsbytecode *pc2 = pc;
            unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
            pc2 += JUMP_OFFSET_LEN;
            unsigned npairs = GET_UINT16(pc2);
            pc2 += UINT16_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump,
                         stackDepth, defineArray, defineCount)) {
                return;
            }

            while (npairs) {
                pc2 += INDEX_LEN;
                unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
                if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                             stackDepth, defineArray, defineCount)) {
                    return;
                }
                pc2 += JUMP_OFFSET_LEN;
                npairs--;
            }
            break;
          }

          case JSOP_TRY: {
            /*
             * Everything between a try and corresponding catch or finally is conditional.
             * Note that there is no problem with code which is skipped by a thrown
             * exception but is not caught by a later handler in the same function:
             * no more code will execute, and it does not matter what is defined.
             */
            JSTryNote *tn = script->trynotes()->vector;
            JSTryNote *tnlimit = tn + script->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script->main - script->code + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    /* This will overestimate try block code, for multiple catch/finally. */
                    if (catchOffset > forwardCatch)
                        forwardCatch = catchOffset;

                    if (tn->kind != JSTRY_ITER) {
                        if (!addJump(cx, catchOffset, &nextOffset, &forwardJump,
                                     stackDepth, defineArray, defineCount)) {
                            return;
                        }
                        code[catchOffset]->exceptionEntry = true;
                    }
                }
            }
            break;
          }

          case JSOP_GETLOCAL:
            /*
             * Watch for uses of variables not known to be defined, and mark
             * them as having possible uses before definitions.  Ignore GETLOCAL
             * followed by a POP, these are generated for, e.g. 'var x;'
             */
            if (pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP) {
                uint32 local = GET_SLOTNO(pc);
                if (local < nfixed && !localDefined(local, offset))
                    setLocal(local, LOCAL_USE_BEFORE_DEF);
            }
            break;

          case JSOP_CALLLOCAL:
          case JSOP_GETLOCALPROP:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC: {
            uint32 local = GET_SLOTNO(pc);
            if (local < nfixed && !localDefined(local, offset))
                setLocal(local, LOCAL_USE_BEFORE_DEF);
            break;
          }

          case JSOP_SETLOCAL: {
            uint32 local = GET_SLOTNO(pc);

            /*
             * The local variable may already have been marked as unconditionally
             * defined at a later point in the script, if that definition was in the
             * condition for a loop which then jumped back here.  In such cases we
             * will not treat the variable as ever being defined in the loop body
             * (see setLocal).
             */
            if (local < nfixed && locals[local] == LOCAL_CONDITIONALLY_DEFINED) {
                if (forwardJump) {
                    /* Add this local to the variables defined after this bytecode. */
                    uint32 *newArray = ArenaArray<uint32>(pool, defineCount + 1);
                    if (!newArray) {
                        setOOM(cx);
                        return;
                    }
                    if (defineCount)
                        memcpy(newArray, defineArray, defineCount * sizeof(uint32));
                    defineArray = newArray;
                    defineArray[defineCount++] = local;
                } else {
                    /* This local is unconditionally defined by this bytecode. */
                    setLocal(local, offset);
                }
            }
            break;
          }

          default:
            break;
        }

        uint32 type = JOF_TYPE(js_CodeSpec[op].format);

        /* Check basic jump opcodes, which may or may not have a fallthrough. */
        if (type == JOF_JUMP || type == JOF_JUMPX) {
            /* Some opcodes behave differently on their branching path. */
            unsigned newStackDepth;

            switch (op) {
              case JSOP_OR:
              case JSOP_AND:
              case JSOP_ORX:
              case JSOP_ANDX:
                /* OR/AND instructions push the operation result when short circuiting. */
                newStackDepth = stackDepth + 1;
                break;

              case JSOP_CASE:
              case JSOP_CASEX:
                /* Case instructions do not push the lvalue back when branching. */
                newStackDepth = stackDepth - 1;
                break;

              default:
                newStackDepth = stackDepth;
                break;
            }

            unsigned targetOffset = offset + GetJumpOffset(pc, pc);
            if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                         newStackDepth, defineArray, defineCount)) {
                return;
            }
        }

        /* Handle any fallthrough from this opcode. */
        if (!BytecodeNoFallThrough(op)) {
            JS_ASSERT(successorOffset < script->length);

            Bytecode *&nextcode = code[successorOffset];
            bool initial = (nextcode == NULL);

            if (initial) {
                nextcode = ArenaNew<Bytecode>(pool);
                if (!nextcode) {
                    setOOM(cx);
                    return;
                }
            }

            if (!nextcode->mergeDefines(cx, this, initial, stackDepth, defineArray, defineCount))
                return;
        }
    }

    JS_ASSERT(!failed());
    JS_ASSERT(forwardJump == 0 && forwardCatch == 0);
}

} /* namespace analyze */
} /* namespace js */
