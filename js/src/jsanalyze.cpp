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

#include "jsinferinlines.h"
#include "jsobjinlines.h"

namespace js {
namespace analyze {

/////////////////////////////////////////////////////////////////////
// Bytecode
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void
PrintBytecode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    printf("#%u:", script->id());
    LifoAlloc lifoAlloc(1024);
    Sprinter sprinter;
    INIT_SPRINTER(cx, &sprinter, &lifoAlloc, 0);
    js_Disassemble1(cx, script, pc, pc - script->code, true, &sprinter);
    fprintf(stdout, "%s", sprinter.base);
}
#endif

/////////////////////////////////////////////////////////////////////
// Bytecode Analysis
/////////////////////////////////////////////////////////////////////

inline bool
ScriptAnalysis::addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump,
                        unsigned stackDepth)
{
    JS_ASSERT(offset < script->length);

    Bytecode *&code = codeArray[offset];
    if (!code) {
        code = cx->typeLifoAlloc().new_<Bytecode>();
        if (!code) {
            setOOM(cx);
            return false;
        }
        code->stackDepth = stackDepth;
    }
    JS_ASSERT(code->stackDepth == stackDepth);

    code->jumpTarget = true;

    if (offset < *currentOffset) {
        /* Scripts containing loops are never inlined. */
        isInlineable = false;

        /* Don't follow back edges to bytecode which has already been analyzed. */
        if (!code->analyzed) {
            if (*forwardJump == 0)
                *forwardJump = *currentOffset;
            *currentOffset = offset;
        }
    } else if (offset > *forwardJump) {
        *forwardJump = offset;
    }

    return true;
}

void
ScriptAnalysis::checkAliasedName(JSContext *cx, jsbytecode *pc)
{
    /*
     * Check to see if an accessed name aliases a local or argument in the
     * current script, and mark that local/arg as escaping. We don't need to
     * worry about marking locals/arguments in scripts this is nested in, as
     * the escaping name will be caught by the parser and the nested local/arg
     * will be marked as closed.
     */

    JSAtom *atom;
    if (JSOp(*pc) == JSOP_DEFFUN) {
        JSFunction *fun = script->getFunction(js_GetIndexFromBytecode(script, pc, 0));
        atom = fun->atom;
    } else {
        JS_ASSERT(JOF_TYPE(js_CodeSpec[*pc].format) == JOF_ATOM);
        atom = script->getAtom(js_GetIndexFromBytecode(script, pc, 0));
    }

    uintN index;
    BindingKind kind = script->bindings.lookup(cx, atom, &index);

    if (kind == ARGUMENT)
        escapedSlots[ArgSlot(index)] = true;
    else if (kind == VARIABLE)
        escapedSlots[LocalSlot(script, index)] = true;
}

void
ScriptAnalysis::analyzeBytecode(JSContext *cx)
{
    JS_ASSERT(cx->compartment->activeAnalysis);
    JS_ASSERT(!ranBytecode());
    LifoAlloc &tla = cx->typeLifoAlloc();

    unsigned length = script->length;
    unsigned nargs = script->function() ? script->function()->nargs : 0;

    numSlots = TotalSlots(script);

    codeArray = tla.newArray<Bytecode*>(length);
    escapedSlots = tla.newArray<JSPackedBool>(numSlots);

    if (!codeArray || !escapedSlots) {
        setOOM(cx);
        return;
    }

    PodZero(codeArray, length);

    /*
     * Populate arg and local slots which can escape and be accessed in ways
     * other than through ARG* and LOCAL* opcodes (though arguments can still
     * be indirectly read but not written through 'arguments' properties).
     * All escaping locals are treated as having possible use-before-defs.
     */

    PodZero(escapedSlots, numSlots);

    if (script->usesEval || script->usesArguments || script->compartment()->debugMode()) {
        for (unsigned i = 0; i < nargs; i++)
            escapedSlots[ArgSlot(i)] = true;
    } else {
        for (unsigned i = 0; i < script->nClosedArgs; i++) {
            unsigned arg = script->getClosedArg(i);
            JS_ASSERT(arg < nargs);
            escapedSlots[ArgSlot(arg)] = true;
        }
    }

    if (script->usesEval || script->compartment()->debugMode()) {
        for (unsigned i = 0; i < script->nfixed; i++)
            escapedSlots[LocalSlot(script, i)] = true;
    } else {
        for (uint32_t i = 0; i < script->nClosedVars; i++) {
            unsigned local = script->getClosedVar(i);
            JS_ASSERT(local < script->nfixed);
            escapedSlots[LocalSlot(script, local)] = true;
        }
    }

    /*
     * If the script is in debug mode, JS_SetFrameReturnValue can be called at
     * any safe point.
     */
    if (cx->compartment->debugMode())
        usesReturnValue_ = true;

    bool heavyweight = script->function() && script->function()->isHeavyweight();

    isInlineable = true;
    if (script->nClosedArgs || script->nClosedVars || heavyweight ||
        script->usesEval || script->usesArguments || cx->compartment->debugMode()) {
        isInlineable = false;
    }

    modifiesArguments_ = false;
    if (script->nClosedArgs || heavyweight)
        modifiesArguments_ = true;

    canTrackVars = true;

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
    Bytecode *startcode = tla.new_<Bytecode>();
    if (!startcode) {
        setOOM(cx);
        return;
    }

    startcode->stackDepth = 0;
    codeArray[0] = startcode;

    /* Number of JOF_TYPESET opcodes we have encountered. */
    unsigned nTypeSets = 0;
    types::TypeSet *typeArray = script->types->typeArray();

    unsigned offset, nextOffset = 0;
    while (nextOffset < length) {
        offset = nextOffset;

        JS_ASSERT(forwardCatch <= forwardJump);

        /* Check if the current forward jump/try-block has finished. */
        if (forwardJump && forwardJump == offset)
            forwardJump = 0;
        if (forwardCatch && forwardCatch == offset)
            forwardCatch = 0;

        Bytecode *code = maybeCode(offset);
        jsbytecode *pc = script->code + offset;

        JSOp op = (JSOp)*pc;
        JS_ASSERT(op < JSOP_LIMIT);

        /* Immediate successor of this bytecode. */
        unsigned successorOffset = offset + GetBytecodeLength(pc);

        /*
         * Next bytecode to analyze.  This is either the successor, or is an
         * earlier bytecode if this bytecode has a loop backedge.
         */
        nextOffset = successorOffset;

        if (!code) {
            /* Haven't found a path by which this bytecode is reachable. */
            continue;
        }

        if (code->analyzed) {
            /* No need to reanalyze, see Bytecode::mergeDefines. */
            continue;
        }

        code->analyzed = true;

        if (forwardCatch)
            code->inTryBlock = true;

        if (script->hasBreakpointsAt(pc)) {
            code->safePoint = true;
            isInlineable = canTrackVars = false;
        }

        unsigned stackDepth = code->stackDepth;

        if (!forwardJump)
            code->unconditional = true;

        /*
         * Treat decompose ops as no-ops which do not adjust the stack. We will
         * pick up the stack depths as we go through the decomposed version.
         */
        if (!(js_CodeSpec[op].format & JOF_DECOMPOSE)) {
            unsigned nuses = GetUseCount(script, offset);
            unsigned ndefs = GetDefCount(script, offset);

            JS_ASSERT(stackDepth >= nuses);
            stackDepth -= nuses;
            stackDepth += ndefs;
        }

        /*
         * Assign an observed type set to each reachable JOF_TYPESET opcode.
         * This may be less than the number of type sets in the script if some
         * are unreachable, and may be greater in case the number of type sets
         * overflows a uint16. In the latter case a single type set will be
         * used for the observed types of all ops after the overflow.
         */
        if ((js_CodeSpec[op].format & JOF_TYPESET) && cx->typeInferenceEnabled()) {
            if (nTypeSets < script->nTypeSets) {
                code->observedTypes = &typeArray[nTypeSets++];
            } else {
                JS_ASSERT(nTypeSets == UINT16_MAX);
                code->observedTypes = &typeArray[nTypeSets - 1];
            }
        }

        switch (op) {

          case JSOP_RETURN:
          case JSOP_STOP:
            numReturnSites_++;
            break;

          case JSOP_SETRVAL:
          case JSOP_POPV:
            usesReturnValue_ = true;
            isInlineable = false;
            break;

          case JSOP_NAME:
          case JSOP_CALLNAME:
          case JSOP_BINDNAME:
          case JSOP_SETNAME:
          case JSOP_DELNAME:
          case JSOP_QNAMEPART:
          case JSOP_QNAMECONST:
            checkAliasedName(cx, pc);
            usesScopeChain_ = true;
            isInlineable = false;
            break;

          case JSOP_DEFFUN:
          case JSOP_DEFVAR:
          case JSOP_DEFCONST:
          case JSOP_SETCONST:
            checkAliasedName(cx, pc);
            extendsScope_ = true;
            isInlineable = canTrackVars = false;
            break;

          case JSOP_EVAL:
            extendsScope_ = true;
            isInlineable = canTrackVars = false;
            break;

          case JSOP_ENTERWITH:
            addsScopeObjects_ = true;
            isInlineable = canTrackVars = false;
            break;

          case JSOP_ENTERLET0:
          case JSOP_ENTERLET1:
          case JSOP_ENTERBLOCK:
          case JSOP_LEAVEBLOCK:
            addsScopeObjects_ = true;
            isInlineable = false;
            break;

          case JSOP_THIS:
            usesThisValue_ = true;
            break;

          case JSOP_CALL:
          case JSOP_NEW:
            /* Only consider potentially inlineable calls here. */
            hasFunctionCalls_ = true;
            break;

          case JSOP_TABLESWITCH: {
            isInlineable = false;
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            jsint low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            jsint high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump, stackDepth))
                return;
            getCode(defaultOffset).switchTarget = true;
            getCode(defaultOffset).safePoint = true;

            for (jsint i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset) {
                    if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, stackDepth))
                        return;
                }
                getCode(targetOffset).switchTarget = true;
                getCode(targetOffset).safePoint = true;
                pc2 += JUMP_OFFSET_LEN;
            }
            break;
          }

          case JSOP_LOOKUPSWITCH: {
            isInlineable = false;
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            unsigned npairs = GET_UINT16(pc2);
            pc2 += UINT16_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump, stackDepth))
                return;
            getCode(defaultOffset).switchTarget = true;
            getCode(defaultOffset).safePoint = true;

            while (npairs) {
                pc2 += INDEX_LEN;
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, stackDepth))
                    return;
                getCode(targetOffset).switchTarget = true;
                getCode(targetOffset).safePoint = true;
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
            isInlineable = false;
            JSTryNote *tn = script->trynotes()->vector;
            JSTryNote *tnlimit = tn + script->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script->mainOffset + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    /* This will overestimate try block code, for multiple catch/finally. */
                    if (catchOffset > forwardCatch)
                        forwardCatch = catchOffset;

                    if (tn->kind != JSTRY_ITER) {
                        if (!addJump(cx, catchOffset, &nextOffset, &forwardJump, stackDepth))
                            return;
                        getCode(catchOffset).exceptionEntry = true;
                        getCode(catchOffset).safePoint = true;
                    }
                }
            }
            break;
          }

          case JSOP_GETLOCAL: {
            /*
             * Watch for uses of variables not known to be defined, and mark
             * them as having possible uses before definitions.  Ignore GETLOCAL
             * followed by a POP, these are generated for, e.g. 'var x;'
             */
            jsbytecode *next = pc + JSOP_GETLOCAL_LENGTH;
            if (JSOp(*next) != JSOP_POP || jumpTarget(next)) {
                uint32_t local = GET_SLOTNO(pc);
                if (local >= script->nfixed) {
                    localsAliasStack_ = true;
                    break;
                }
            }
            break;
          }

          case JSOP_CALLLOCAL:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC:
          case JSOP_SETLOCAL: {
            uint32_t local = GET_SLOTNO(pc);
            if (local >= script->nfixed) {
                localsAliasStack_ = true;
                break;
            }
            break;
          }

          case JSOP_SETARG:
          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC:
            modifiesArguments_ = true;
            isInlineable = false;
            break;

          /* Additional opcodes which can be compiled but which can't be inlined. */
          case JSOP_ARGUMENTS:
          case JSOP_THROW:
          case JSOP_EXCEPTION:
          case JSOP_DEFLOCALFUN:
          case JSOP_DEFLOCALFUN_FC:
          case JSOP_LAMBDA:
          case JSOP_LAMBDA_FC:
          case JSOP_GETFCSLOT:
          case JSOP_CALLFCSLOT:
          case JSOP_DEBUGGER:
          case JSOP_FUNCALL:
          case JSOP_FUNAPPLY:
            isInlineable = false;
            break;

          default:
            break;
        }

        uint32_t type = JOF_TYPE(js_CodeSpec[op].format);

        /* Check basic jump opcodes, which may or may not have a fallthrough. */
        if (type == JOF_JUMP) {
            /* Some opcodes behave differently on their branching path. */
            unsigned newStackDepth = stackDepth;

            switch (op) {
              case JSOP_CASE:
                /* Case instructions do not push the lvalue back when branching. */
                newStackDepth--;
                break;

              default:;
            }

            unsigned targetOffset = offset + GET_JUMP_OFFSET(pc);
            if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, newStackDepth))
                return;
        }

        /* Handle any fallthrough from this opcode. */
        if (!BytecodeNoFallThrough(op)) {
            JS_ASSERT(successorOffset < script->length);

            Bytecode *&nextcode = codeArray[successorOffset];

            if (!nextcode) {
                nextcode = tla.new_<Bytecode>();
                if (!nextcode) {
                    setOOM(cx);
                    return;
                }
                nextcode->stackDepth = stackDepth;
            }
            JS_ASSERT(nextcode->stackDepth == stackDepth);

            if (type == JOF_JUMP)
                nextcode->jumpFallthrough = true;

            /* Treat the fallthrough of a branch instruction as a jump target. */
            if (type == JOF_JUMP)
                nextcode->jumpTarget = true;
            else
                nextcode->fallthrough = true;
        }
    }

    JS_ASSERT(!failed());
    JS_ASSERT(forwardJump == 0 && forwardCatch == 0);

    ranBytecode_ = true;
}

/////////////////////////////////////////////////////////////////////
// Lifetime Analysis
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::analyzeLifetimes(JSContext *cx)
{
    JS_ASSERT(cx->compartment->activeAnalysis && !ranLifetimes() && !failed());

    if (!ranBytecode()) {
        analyzeBytecode(cx);
        if (failed())
            return;
    }

    LifoAlloc &tla = cx->typeLifoAlloc();

    lifetimes = tla.newArray<LifetimeVariable>(numSlots);
    if (!lifetimes) {
        setOOM(cx);
        return;
    }
    PodZero(lifetimes, numSlots);

    /*
     * Variables which are currently dead. On forward branches to locations
     * where these are live, they need to be marked as live.
     */
    LifetimeVariable **saved = (LifetimeVariable **)
        cx->calloc_(numSlots * sizeof(LifetimeVariable*));
    if (!saved) {
        setOOM(cx);
        return;
    }
    unsigned savedCount = 0;

    LoopAnalysis *loop = NULL;

    uint32_t offset = script->length - 1;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);
        if (!code) {
            offset--;
            continue;
        }

        if (loop && code->safePoint)
            loop->hasSafePoints = true;

        jsbytecode *pc = script->code + offset;

        JSOp op = (JSOp) *pc;

        if (op == JSOP_LOOPHEAD && code->loop) {
            /*
             * This is the head of a loop, we need to go and make sure that any
             * variables live at the head are live at the backedge and points prior.
             * For each such variable, look for the last lifetime segment in the body
             * and extend it to the end of the loop.
             */
            JS_ASSERT(loop == code->loop);
            unsigned backedge = code->loop->backedge;
            for (unsigned i = 0; i < numSlots; i++) {
                if (lifetimes[i].lifetime)
                    extendVariable(cx, lifetimes[i], offset, backedge);
            }

            loop = loop->parent;
            JS_ASSERT_IF(loop, loop->head < offset);
        }

        /* Find the last jump target in the loop, other than the initial entry point. */
        if (loop && code->jumpTarget && offset != loop->entry && offset > loop->lastBlock)
            loop->lastBlock = offset;

        if (code->exceptionEntry) {
            DebugOnly<bool> found = false;
            JSTryNote *tn = script->trynotes()->vector;
            JSTryNote *tnlimit = tn + script->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script->mainOffset + tn->start;
                if (startOffset + tn->length == offset) {
                    /*
                     * Extend all live variables at exception entry to the start of
                     * the try block.
                     */
                    for (unsigned i = 0; i < numSlots; i++) {
                        if (lifetimes[i].lifetime)
                            ensureVariable(lifetimes[i], startOffset - 1);
                    }

                    found = true;
                    break;
                }
            }
            JS_ASSERT(found);
        }

        switch (op) {
          case JSOP_GETARG:
          case JSOP_CALLARG:
          case JSOP_GETLOCAL:
          case JSOP_CALLLOCAL:
          case JSOP_THIS: {
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (!slotEscapes(slot))
                addVariable(cx, lifetimes[slot], offset, saved, savedCount);
            break;
          }

          case JSOP_SETARG:
          case JSOP_SETLOCAL:
          case JSOP_SETLOCALPOP:
          case JSOP_DEFLOCALFUN:
          case JSOP_DEFLOCALFUN_FC: {
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (!slotEscapes(slot))
                killVariable(cx, lifetimes[slot], offset, saved, savedCount);
            break;
          }

          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC: {
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (!slotEscapes(slot)) {
                killVariable(cx, lifetimes[slot], offset, saved, savedCount);
                addVariable(cx, lifetimes[slot], offset, saved, savedCount);
            }
            break;
          }

          case JSOP_LOOKUPSWITCH:
          case JSOP_TABLESWITCH:
            /* Restore all saved variables. :FIXME: maybe do this precisely. */
            for (unsigned i = 0; i < savedCount; i++) {
                LifetimeVariable &var = *saved[i];
                var.lifetime = tla.new_<Lifetime>(offset, var.savedEnd, var.saved);
                if (!var.lifetime) {
                    cx->free_(saved);
                    setOOM(cx);
                    return;
                }
                var.saved = NULL;
                saved[i--] = saved[--savedCount];
            }
            savedCount = 0;
            break;

          case JSOP_TRY:
            for (unsigned i = 0; i < numSlots; i++) {
                LifetimeVariable &var = lifetimes[i];
                if (var.ensured) {
                    JS_ASSERT(var.lifetime);
                    if (var.lifetime->start == offset)
                        var.ensured = false;
                }
            }
            break;

          case JSOP_NEW:
          case JSOP_CALL:
          case JSOP_EVAL:
          case JSOP_FUNAPPLY:
          case JSOP_FUNCALL:
            if (loop)
                loop->hasCallsLoops = true;
            break;

          default:;
        }

        uint32_t type = JOF_TYPE(js_CodeSpec[op].format);
        if (type == JOF_JUMP) {
            /*
             * Forward jumps need to pull in all variables which are live at
             * their target offset --- the variables live before the jump are
             * the union of those live at the fallthrough and at the target.
             */
            uint32_t targetOffset = FollowBranch(cx, script, offset);

            /*
             * Watch for 'continue' statements in the loop body, which are
             * jumps to the entry offset separate from the initial jump.
             */
            if (loop && loop->entry == targetOffset && loop->entry > loop->lastBlock)
                loop->lastBlock = loop->entry;

            if (targetOffset < offset) {
                /* This is a loop back edge, no lifetime to pull in yet. */

#ifdef DEBUG
                JSOp nop = JSOp(script->code[targetOffset]);
                JS_ASSERT(nop == JSOP_LOOPHEAD);
#endif

                /*
                 * If we already have a loop, it is an outer loop and we
                 * need to prune the last block in the loop --- we do not
                 * track 'continue' statements for outer loops.
                 */
                if (loop && loop->entry > loop->lastBlock)
                    loop->lastBlock = loop->entry;

                LoopAnalysis *nloop = tla.new_<LoopAnalysis>();
                if (!nloop) {
                    cx->free_(saved);
                    setOOM(cx);
                    return;
                }
                PodZero(nloop);

                if (loop)
                    loop->hasCallsLoops = true;

                nloop->parent = loop;
                loop = nloop;

                getCode(targetOffset).loop = loop;
                loop->head = targetOffset;
                loop->backedge = offset;
                loop->lastBlock = loop->head;

                /*
                 * Find the entry jump, which will be a GOTO for 'for' or
                 * 'while' loops or a fallthrough for 'do while' loops.
                 */
                uint32_t entry = targetOffset;
                if (entry) {
                    do {
                        entry--;
                    } while (!maybeCode(entry));

                    jsbytecode *entrypc = script->code + entry;

                    if (JSOp(*entrypc) == JSOP_GOTO)
                        loop->entry = entry + GET_JUMP_OFFSET(entrypc);
                    else
                        loop->entry = targetOffset;
                } else {
                    /* Do-while loop at the start of the script. */
                    loop->entry = targetOffset;
                }
            } else {
                for (unsigned i = 0; i < savedCount; i++) {
                    LifetimeVariable &var = *saved[i];
                    JS_ASSERT(!var.lifetime && var.saved);
                    if (var.live(targetOffset)) {
                        /*
                         * Jumping to a place where this variable is live. Make a new
                         * lifetime segment for the variable.
                         */
                        var.lifetime = tla.new_<Lifetime>(offset, var.savedEnd, var.saved);
                        if (!var.lifetime) {
                            cx->free_(saved);
                            setOOM(cx);
                            return;
                        }
                        var.saved = NULL;
                        saved[i--] = saved[--savedCount];
                    } else if (loop && !var.savedEnd) {
                        /*
                         * This jump precedes the basic block which killed the variable,
                         * remember it and use it for the end of the next lifetime
                         * segment should the variable become live again. This is needed
                         * for loops, as if we wrap liveness around the loop the isLive
                         * test below may have given the wrong answer.
                         */
                        var.savedEnd = offset;
                    }
                }
            }
        }

        offset--;
    }

    cx->free_(saved);

    ranLifetimes_ = true;
}

#ifdef DEBUG
void
LifetimeVariable::print() const
{
    Lifetime *segment = lifetime ? lifetime : saved;
    while (segment) {
        printf(" (%u,%u%s)", segment->start, segment->end, segment->loopTail ? ",tail" : "");
        segment = segment->next;
    }
    printf("\n");
}
#endif /* DEBUG */

inline void
ScriptAnalysis::addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                            LifetimeVariable **&saved, unsigned &savedCount)
{
    if (var.lifetime) {
        if (var.ensured)
            return;

        JS_ASSERT(offset < var.lifetime->start);
        var.lifetime->start = offset;
    } else {
        if (var.saved) {
            /* Remove from the list of saved entries. */
            for (unsigned i = 0; i < savedCount; i++) {
                if (saved[i] == &var) {
                    JS_ASSERT(savedCount);
                    saved[i--] = saved[--savedCount];
                    break;
                }
            }
        }
        var.lifetime = cx->typeLifoAlloc().new_<Lifetime>(offset, var.savedEnd, var.saved);
        if (!var.lifetime) {
            setOOM(cx);
            return;
        }
        var.saved = NULL;
    }
}

inline void
ScriptAnalysis::killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset,
                             LifetimeVariable **&saved, unsigned &savedCount)
{
    if (!var.lifetime) {
        /* Make a point lifetime indicating the write. */
        if (!var.saved)
            saved[savedCount++] = &var;
        var.saved = cx->typeLifoAlloc().new_<Lifetime>(offset, var.savedEnd, var.saved);
        if (!var.saved) {
            setOOM(cx);
            return;
        }
        var.saved->write = true;
        var.savedEnd = 0;
        return;
    }

    JS_ASSERT_IF(!var.ensured, offset < var.lifetime->start);
    unsigned start = var.lifetime->start;

    /*
     * The variable is considered to be live at the bytecode which kills it
     * (just not at earlier bytecodes). This behavior is needed by downstream
     * register allocation (see FrameState::bestEvictReg).
     */
    var.lifetime->start = offset;
    var.lifetime->write = true;

    if (var.ensured) {
        /*
         * The variable is live even before the write, due to an enclosing try
         * block. We need to split the lifetime to indicate there was a write.
         */
        var.lifetime = cx->typeLifoAlloc().new_<Lifetime>(start, offset, var.lifetime);
        if (!var.lifetime) {
            setOOM(cx);
            return;
        }
    } else {
        var.saved = var.lifetime;
        var.savedEnd = 0;
        var.lifetime = NULL;

        saved[savedCount++] = &var;
    }
}

inline void
ScriptAnalysis::extendVariable(JSContext *cx, LifetimeVariable &var,
                               unsigned start, unsigned end)
{
    JS_ASSERT(var.lifetime);
    if (var.ensured) {
        /*
         * If we are still ensured to be live, the try block must scope over
         * the loop, in which case the variable is already guaranteed to be
         * live for the entire loop.
         */
        JS_ASSERT(var.lifetime->start < start);
        return;
    }

    var.lifetime->start = start;

    /*
     * When walking backwards through loop bodies, we don't know which vars
     * are live at the loop's backedge. We save the endpoints for lifetime
     * segments which we *would* use if the variables were live at the backedge
     * and extend the variable with new lifetimes if we find the variable is
     * indeed live at the head of the loop.
     *
     * while (...) {
     *   if (x #1) { ... }
     *   ...
     *   if (... #2) { x = 0; #3}
     * }
     *
     * If x is not live after the loop, we treat it as dead in the walk and
     * make a point lifetime for the write at #3. At the beginning of that
     * basic block (#2), we save the loop endpoint; if we knew x was live in
     * the next iteration then a new lifetime would be made here. At #1 we
     * mark x live again, make a segment between the head of the loop and #1,
     * and then extend x with loop tail lifetimes from #1 to #2, and from #3
     * to the back edge.
     */

    Lifetime *segment = var.lifetime;
    while (segment && segment->start < end) {
        uint32_t savedEnd = segment->savedEnd;
        if (!segment->next || segment->next->start >= end) {
            /*
             * savedEnd is only set for variables killed in the middle of the
             * loop. Make a tail segment connecting the last use with the
             * back edge.
             */
            if (segment->end >= end) {
                /* Variable known to be live after the loop finishes. */
                break;
            }
            savedEnd = end;
        }
        JS_ASSERT(savedEnd <= end);
        if (savedEnd > segment->end) {
            Lifetime *tail = cx->typeLifoAlloc().new_<Lifetime>(savedEnd, 0, segment->next);
            if (!tail) {
                setOOM(cx);
                return;
            }
            tail->start = segment->end;
            tail->loopTail = true;

            /*
             * Clear the segment's saved end, but preserve in the tail if this
             * is the last segment in the loop and the variable is killed in an
             * outer loop before the backedge.
             */
            if (segment->savedEnd > end) {
                JS_ASSERT(savedEnd == end);
                tail->savedEnd = segment->savedEnd;
            }
            segment->savedEnd = 0;

            segment->next = tail;
            segment = tail->next;
        } else {
            JS_ASSERT(segment->savedEnd == 0);
            segment = segment->next;
        }
    }
}

inline void
ScriptAnalysis::ensureVariable(LifetimeVariable &var, unsigned until)
{
    JS_ASSERT(var.lifetime);

    /*
     * If we are already ensured, the current range we are trying to ensure
     * should already be included.
     */
    if (var.ensured) {
        JS_ASSERT(var.lifetime->start <= until);
        return;
    }

    JS_ASSERT(until < var.lifetime->start);
    var.lifetime->start = until;
    var.ensured = true;
}

void
ScriptAnalysis::clearAllocations()
{
    /*
     * Clear out storage used for register allocations in a compilation once
     * that compilation has finished. Register allocations are only used for
     * a single compilation.
     */
    for (unsigned i = 0; i < script->length; i++) {
        Bytecode *code = maybeCode(i);
        if (code)
            code->allocation = NULL;
    }
}

/////////////////////////////////////////////////////////////////////
// SSA Analysis
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::analyzeSSA(JSContext *cx)
{
    JS_ASSERT(cx->compartment->activeAnalysis && !ranSSA() && !failed());

    if (!ranLifetimes()) {
        analyzeLifetimes(cx);
        if (failed())
            return;
    }

    LifoAlloc &tla = cx->typeLifoAlloc();
    unsigned maxDepth = script->nslots - script->nfixed;

    /*
     * Current value of each variable and stack value. Empty for missing or
     * untracked entries, i.e. escaping locals and arguments.
     */
    SSAValue *values = (SSAValue *)
        cx->calloc_((numSlots + maxDepth) * sizeof(SSAValue));
    if (!values) {
        setOOM(cx);
        return;
    }
    struct FreeSSAValues {
        JSContext *cx;
        SSAValue *values;
        FreeSSAValues(JSContext *cx, SSAValue *values) : cx(cx), values(values) {}
        ~FreeSSAValues() { cx->free_(values); }
    } free(cx, values);

    SSAValue *stack = values + numSlots;
    uint32_t stackDepth = 0;

    for (uint32_t slot = ArgSlot(0); slot < numSlots; slot++) {
        if (trackSlot(slot))
            values[slot].initInitial(slot);
    }

    /*
     * All target offsets for forward jumps we in the middle of. We lazily add
     * pending entries at these targets for the original value of variables
     * modified before the branch rejoins.
     */
    Vector<uint32_t> branchTargets(cx);

    /*
     * Subset of branchTargets which are also exception handlers. Any value of
     * a variable modified before the target is reached is a potential value
     * at that target, along with the lazily added original value.
     */
    Vector<uint32_t> exceptionTargets(cx);

    uint32_t offset = 0;
    while (offset < script->length) {
        jsbytecode *pc = script->code + offset;
        JSOp op = (JSOp)*pc;

        uint32_t successorOffset = offset + GetBytecodeLength(pc);

        Bytecode *code = maybeCode(pc);
        if (!code) {
            offset = successorOffset;
            continue;
        }

        if (code->stackDepth > stackDepth)
            PodZero(stack + stackDepth, code->stackDepth - stackDepth);
        stackDepth = code->stackDepth;

        if (op == JSOP_LOOPHEAD && code->loop) {
            /*
             * Make sure there is a pending value array for phi nodes at the
             * loop head. We won't be able to clear these until we reach the
             * loop's back edge.
             *
             * We need phi nodes for all variables which might be modified
             * during the loop. This ensures that in the loop body we have
             * already updated state to reflect possible changes that happen
             * before the back edge, and don't need to go back and fix things
             * up when we *do* get to the back edge. This could be made lazier.
             *
             * We don't make phi nodes for values on the stack at the head of
             * the loop. These may be popped during the loop (i.e. for ITER
             * loops), but in such cases the original value is pushed back.
             */
            Vector<SlotValue> *&pending = code->pendingValues;
            if (pending) {
                removeBranchTarget(branchTargets, exceptionTargets, offset);
            } else {
                pending = cx->new_< Vector<SlotValue> >(cx);
                if (!pending) {
                    setOOM(cx);
                    return;
                }
            }

            /*
             * Make phi nodes and update state for slots which are already in
             * pending from previous branches to the loop head, and which are
             * modified in the body of the loop.
             */
            for (unsigned i = 0; i < pending->length(); i++) {
                SlotValue &v = (*pending)[i];
                if (v.slot < numSlots && liveness(v.slot).firstWrite(code->loop) != UINT32_MAX) {
                    if (v.value.kind() != SSAValue::PHI || v.value.phiOffset() < offset) {
                        SSAValue ov = v.value;
                        if (!makePhi(cx, v.slot, offset, &ov))
                            return;
                        insertPhi(cx, ov, v.value);
                        v.value = ov;
                    }
                }
                if (code->fallthrough || code->jumpFallthrough)
                    mergeValue(cx, offset, values[v.slot], &v);
                mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets);
                values[v.slot] = v.value;
            }

            /*
             * Make phi nodes for all other slots which might be modified
             * during the loop. This ensures that in the loop body we have
             * already updated state to reflect possible changes that happen
             * before the back edge, and don't need to go back and fix things
             * up when we *do* get to the back edge. This could be made lazier.
             */
            for (uint32_t slot = ArgSlot(0); slot < numSlots + stackDepth; slot++) {
                if (slot >= numSlots || !trackSlot(slot))
                    continue;
                if (liveness(slot).firstWrite(code->loop) == UINT32_MAX)
                    continue;
                if (values[slot].kind() == SSAValue::PHI && values[slot].phiOffset() == offset) {
                    /* There is already a pending entry for this slot. */
                    continue;
                }
                SSAValue ov;
                if (!makePhi(cx, slot, offset, &ov))
                    return;
                if (code->fallthrough || code->jumpFallthrough)
                    insertPhi(cx, ov, values[slot]);
                mergeBranchTarget(cx, values[slot], slot, branchTargets);
                values[slot] = ov;
                if (!pending->append(SlotValue(slot, ov))) {
                    setOOM(cx);
                    return;
                }
            }
        } else if (code->pendingValues) {
            /*
             * New values at this point from a previous jump to this bytecode.
             * If there is fallthrough from the previous instruction, merge
             * with the current state and create phi nodes where necessary,
             * otherwise replace current values with the new values.
             *
             * Catch blocks are artifically treated as having fallthrough, so
             * that values written inside the block but not subsequently
             * overwritten are picked up.
             */
            bool exception = removeBranchTarget(branchTargets, exceptionTargets, offset);
            Vector<SlotValue> *pending = code->pendingValues;
            for (unsigned i = 0; i < pending->length(); i++) {
                SlotValue &v = (*pending)[i];
                if (code->fallthrough || code->jumpFallthrough ||
                    (exception && values[v.slot].kind() != SSAValue::EMPTY)) {
                    mergeValue(cx, offset, values[v.slot], &v);
                }
                mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets);
                values[v.slot] = v.value;
            }
            freezeNewValues(cx, offset);
        }

        if (js_CodeSpec[op].format & JOF_DECOMPOSE) {
            offset = successorOffset;
            continue;
        }

        unsigned nuses = GetUseCount(script, offset);
        unsigned ndefs = GetDefCount(script, offset);
        JS_ASSERT(stackDepth >= nuses);

        unsigned xuses = ExtendedUse(pc) ? nuses + 1 : nuses;

        if (xuses) {
            code->poppedValues = tla.newArray<SSAValue>(xuses);
            if (!code->poppedValues) {
                setOOM(cx);
                return;
            }
            for (unsigned i = 0; i < nuses; i++) {
                SSAValue &v = stack[stackDepth - 1 - i];
                code->poppedValues[i] = v;
                v.clear();
            }
            if (xuses > nuses) {
                /*
                 * For SETLOCAL, INCLOCAL, etc. opcodes, add an extra popped
                 * value holding the value of the local before the op.
                 */
                uint32_t slot = GetBytecodeSlot(script, pc);
                if (trackSlot(slot))
                    code->poppedValues[nuses] = values[slot];
                else
                    code->poppedValues[nuses].clear();
            }

            if (xuses) {
                SSAUseChain *useChains = tla.newArray<SSAUseChain>(xuses);
                if (!useChains) {
                    setOOM(cx);
                    return;
                }
                PodZero(useChains, xuses);
                for (unsigned i = 0; i < xuses; i++) {
                    const SSAValue &v = code->poppedValues[i];
                    if (trackUseChain(v)) {
                        SSAUseChain *&uses = useChain(v);
                        useChains[i].popped = true;
                        useChains[i].offset = offset;
                        useChains[i].u.which = i;
                        useChains[i].next = uses;
                        uses = &useChains[i];
                    }
                }
            }
        }

        stackDepth -= nuses;

        for (unsigned i = 0; i < ndefs; i++)
            stack[stackDepth + i].initPushed(offset, i);

        unsigned xdefs = ExtendedDef(pc) ? ndefs + 1 : ndefs;
        if (xdefs) {
            code->pushedUses = tla.newArray<SSAUseChain *>(xdefs);
            if (!code->pushedUses) {
                setOOM(cx);
                return;
            }
            PodZero(code->pushedUses, xdefs);
        }

        stackDepth += ndefs;

        if (BytecodeUpdatesSlot(op)) {
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (trackSlot(slot)) {
                mergeBranchTarget(cx, values[slot], slot, branchTargets);
                mergeExceptionTarget(cx, values[slot], slot, exceptionTargets);
                values[slot].initWritten(slot, offset);
            }
        }

        switch (op) {
          case JSOP_GETARG:
          case JSOP_GETLOCAL: {
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (trackSlot(slot)) {
                /*
                 * Propagate the current value of the local to the pushed value,
                 * and remember it with an extended use on the opcode.
                 */
                stack[stackDepth - 1] = code->poppedValues[0] = values[slot];
            }
            break;
          }

          /* Short circuit ops which push back one of their operands. */

          case JSOP_MOREITER:
            stack[stackDepth - 2] = code->poppedValues[0];
            break;

          case JSOP_INITPROP:
          case JSOP_INITMETHOD:
            stack[stackDepth - 1] = code->poppedValues[1];
            break;

          case JSOP_INITELEM:
            stack[stackDepth - 1] = code->poppedValues[2];
            break;

          case JSOP_DUP:
            stack[stackDepth - 1] = stack[stackDepth - 2] = code->poppedValues[0];
            break;

          case JSOP_DUP2:
            stack[stackDepth - 1] = stack[stackDepth - 3] = code->poppedValues[0];
            stack[stackDepth - 2] = stack[stackDepth - 4] = code->poppedValues[1];
            break;

          case JSOP_SWAP:
            /* Swap is like pick 1. */
          case JSOP_PICK: {
            unsigned pickedDepth = (op == JSOP_SWAP ? 1 : pc[1]);
            stack[stackDepth - 1] = code->poppedValues[pickedDepth];
            for (unsigned i = 0; i < pickedDepth; i++)
                stack[stackDepth - 2 - i] = code->poppedValues[i];
            break;
          }

          /*
           * Switch and try blocks preserve the stack between the original op
           * and all case statements or exception/finally handlers.
           */

          case JSOP_TABLESWITCH: {
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            jsint low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            jsint high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            checkBranchTarget(cx, defaultOffset, branchTargets, values, stackDepth);

            for (jsint i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset)
                    checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth);
                pc2 += JUMP_OFFSET_LEN;
            }
            break;
          }

          case JSOP_LOOKUPSWITCH: {
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            unsigned npairs = GET_UINT16(pc2);
            pc2 += UINT16_LEN;

            checkBranchTarget(cx, defaultOffset, branchTargets, values, stackDepth);

            while (npairs) {
                pc2 += INDEX_LEN;
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth);
                pc2 += JUMP_OFFSET_LEN;
                npairs--;
            }
            break;
          }

          case JSOP_TRY: { 
            JSTryNote *tn = script->trynotes()->vector;
            JSTryNote *tnlimit = tn + script->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script->mainOffset + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    if (tn->kind != JSTRY_ITER) {
                        checkBranchTarget(cx, catchOffset, branchTargets, values, stackDepth);
                        checkExceptionTarget(cx, catchOffset, exceptionTargets);
                    }
                }
            }
            break;
          }

          case JSOP_THROW:
            mergeAllExceptionTargets(cx, values, exceptionTargets);
            break;

          default:;
        }

        uint32_t type = JOF_TYPE(js_CodeSpec[op].format);
        if (type == JOF_JUMP) {
            unsigned targetOffset = FollowBranch(cx, script, offset);
            checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth);

            /*
             * If this is a back edge, we're done with the loop and can freeze
             * the phi values at the head now.
             */
            if (targetOffset < offset)
                freezeNewValues(cx, targetOffset);
        }

        offset = successorOffset;
    }

    ranSSA_ = true;
}

/* Get a phi node's capacity for a given length. */
static inline unsigned
PhiNodeCapacity(unsigned length)
{
    if (length <= 4)
        return 4;

    unsigned log2;
    JS_FLOOR_LOG2(log2, length - 1);
    return 1 << (log2 + 1);
}

bool
ScriptAnalysis::makePhi(JSContext *cx, uint32_t slot, uint32_t offset, SSAValue *pv)
{
    SSAPhiNode *node = cx->typeLifoAlloc().new_<SSAPhiNode>();
    SSAValue *options = cx->typeLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(0));
    if (!node || !options) {
        setOOM(cx);
        return false;
    }
    node->slot = slot;
    node->options = options;
    pv->initPhi(offset, node);
    return true;
}

void
ScriptAnalysis::insertPhi(JSContext *cx, SSAValue &phi, const SSAValue &v)
{
    JS_ASSERT(phi.kind() == SSAValue::PHI);
    SSAPhiNode *node = phi.phiNode();

    /*
     * Filter dupes inserted into small nodes to keep things clean and avoid
     * extra type constraints, but don't bother on large phi nodes to avoid
     * quadratic behavior.
     */
    if (node->length <= 8) {
        for (unsigned i = 0; i < node->length; i++) {
            if (v == node->options[i])
                return;
        }
    }

    if (trackUseChain(v)) {
        SSAUseChain *&uses = useChain(v);

        SSAUseChain *use = cx->typeLifoAlloc().new_<SSAUseChain>();
        if (!use) {
            setOOM(cx);
            return;
        }

        use->popped = false;
        use->offset = phi.phiOffset();
        use->u.phi = node;
        use->next = uses;
        uses = use;
    }

    if (node->length < PhiNodeCapacity(node->length)) {
        node->options[node->length++] = v;
        return;
    }

    SSAValue *newOptions =
        cx->typeLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(node->length + 1));
    if (!newOptions) {
        setOOM(cx);
        return;
    }

    PodCopy(newOptions, node->options, node->length);
    node->options = newOptions;
    node->options[node->length++] = v;
}

inline void
ScriptAnalysis::mergeValue(JSContext *cx, uint32_t offset, const SSAValue &v, SlotValue *pv)
{
    /* Make sure that v is accounted for in the pending value or phi value at pv. */
    JS_ASSERT(v.kind() != SSAValue::EMPTY && pv->value.kind() != SSAValue::EMPTY);

    if (v == pv->value)
        return;

    if (pv->value.kind() != SSAValue::PHI || pv->value.phiOffset() < offset) {
        SSAValue ov = pv->value;
        if (makePhi(cx, pv->slot, offset, &pv->value)) {
            insertPhi(cx, pv->value, v);
            insertPhi(cx, pv->value, ov);
        }
        return;
    }

    JS_ASSERT(pv->value.phiOffset() == offset);
    insertPhi(cx, pv->value, v);
}

void
ScriptAnalysis::checkPendingValue(JSContext *cx, const SSAValue &v, uint32_t slot,
                                  Vector<SlotValue> *pending)
{
    JS_ASSERT(v.kind() != SSAValue::EMPTY);

    for (unsigned i = 0; i < pending->length(); i++) {
        if ((*pending)[i].slot == slot)
            return;
    }

    if (!pending->append(SlotValue(slot, v)))
        setOOM(cx);
}

void
ScriptAnalysis::checkBranchTarget(JSContext *cx, uint32_t targetOffset,
                                  Vector<uint32_t> &branchTargets,
                                  SSAValue *values, uint32_t stackDepth)
{
    unsigned targetDepth = getCode(targetOffset).stackDepth;
    JS_ASSERT(targetDepth <= stackDepth);

    /*
     * If there is already an active branch to target, make sure its pending
     * values reflect any changes made since the first branch. Otherwise, add a
     * new pending branch and determine its pending values lazily.
     */
    Vector<SlotValue> *&pending = getCode(targetOffset).pendingValues;
    if (pending) {
        for (unsigned i = 0; i < pending->length(); i++) {
            SlotValue &v = (*pending)[i];
            mergeValue(cx, targetOffset, values[v.slot], &v);
        }
    } else {
        pending = cx->new_< Vector<SlotValue> >(cx);
        if (!pending || !branchTargets.append(targetOffset)) {
            setOOM(cx);
            return;
        }
    }

    /*
     * Make sure there is a pending entry for each value on the stack.
     * The number of stack entries at join points is usually zero, and
     * we don't want to look at the active branches while popping and
     * pushing values in each opcode.
     */
    for (unsigned i = 0; i < targetDepth; i++) {
        uint32_t slot = StackSlot(script, i);
        checkPendingValue(cx, values[slot], slot, pending);
    }
}

void
ScriptAnalysis::checkExceptionTarget(JSContext *cx, uint32_t catchOffset,
                                     Vector<uint32_t> &exceptionTargets)
{
    /*
     * The catch offset will already be in the branch targets, just check
     * whether this is already a known exception target.
     */
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        if (exceptionTargets[i] == catchOffset)
            return;
    }
    if (!exceptionTargets.append(catchOffset))
        setOOM(cx);
}

void
ScriptAnalysis::mergeBranchTarget(JSContext *cx, const SSAValue &value, uint32_t slot,
                                  const Vector<uint32_t> &branchTargets)
{
    if (slot >= numSlots) {
        /*
         * There is no need to lazily check that there are pending values at
         * branch targets for slots on the stack, these are added to pending
         * eagerly.
         */
        return;
    }

    JS_ASSERT(trackSlot(slot));

    /*
     * Before changing the value of a variable, make sure the old value is
     * marked at the target of any branches jumping over the current opcode.
     */
    for (unsigned i = 0; i < branchTargets.length(); i++) {
        Vector<SlotValue> *pending = getCode(branchTargets[i]).pendingValues;
        checkPendingValue(cx, value, slot, pending);
    }
}

void
ScriptAnalysis::mergeExceptionTarget(JSContext *cx, const SSAValue &value, uint32_t slot,
                                     const Vector<uint32_t> &exceptionTargets)
{
    JS_ASSERT(trackSlot(slot));

    /*
     * Update the value at exception targets with the value of a variable
     * before it is overwritten. Unlike mergeBranchTarget, this is done whether
     * or not the overwritten value is the value of the variable at the
     * original branch. Values for a variable which are written after the
     * try block starts and overwritten before it is finished can still be
     * seen at exception handlers via exception paths.
     */
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        unsigned offset = exceptionTargets[i];
        Vector<SlotValue> *pending = getCode(offset).pendingValues;

        bool duplicate = false;
        for (unsigned i = 0; i < pending->length(); i++) {
            if ((*pending)[i].slot == slot) {
                duplicate = true;
                SlotValue &v = (*pending)[i];
                mergeValue(cx, offset, value, &v);
                break;
            }
        }

        if (!duplicate && !pending->append(SlotValue(slot, value)))
            setOOM(cx);
    }
}

void
ScriptAnalysis::mergeAllExceptionTargets(JSContext *cx, SSAValue *values,
                                         const Vector<uint32_t> &exceptionTargets)
{
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        Vector<SlotValue> *pending = getCode(exceptionTargets[i]).pendingValues;
        for (unsigned i = 0; i < pending->length(); i++) {
            const SlotValue &v = (*pending)[i];
            if (trackSlot(v.slot))
                mergeExceptionTarget(cx, values[v.slot], v.slot, exceptionTargets);
        }
    }
}

bool
ScriptAnalysis::removeBranchTarget(Vector<uint32_t> &branchTargets,
                                   Vector<uint32_t> &exceptionTargets,
                                   uint32_t offset)
{
    bool exception = false;
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        if (exceptionTargets[i] == offset) {
            exceptionTargets[i] = branchTargets.back();
            exceptionTargets.popBack();
            exception = true;
            break;
        }
    }
    for (unsigned i = 0; i < branchTargets.length(); i++) {
        if (branchTargets[i] == offset) {
            branchTargets[i] = branchTargets.back();
            branchTargets.popBack();
            return exception;
        }
    }
    JS_ASSERT(OOM());
    return exception;
}

void
ScriptAnalysis::freezeNewValues(JSContext *cx, uint32_t offset)
{
    Bytecode &code = getCode(offset);

    Vector<SlotValue> *pending = code.pendingValues;
    code.pendingValues = NULL;

    unsigned count = pending->length();
    if (count == 0) {
        cx->delete_(pending);
        return;
    }

    code.newValues = cx->typeLifoAlloc().newArray<SlotValue>(count + 1);
    if (!code.newValues) {
        setOOM(cx);
        return;
    }

    for (unsigned i = 0; i < count; i++)
        code.newValues[i] = (*pending)[i];
    code.newValues[count].slot = 0;
    code.newValues[count].value.clear();

    cx->delete_(pending);
}

CrossSSAValue
CrossScriptSSA::foldValue(const CrossSSAValue &cv)
{
    const Frame &frame = getFrame(cv.frame);
    const SSAValue &v = cv.v;

    JSScript *parentScript = NULL;
    ScriptAnalysis *parentAnalysis = NULL;
    if (frame.parent != INVALID_FRAME) {
        parentScript = getFrame(frame.parent).script;
        parentAnalysis = parentScript->analysis();
    }

    if (v.kind() == SSAValue::VAR && v.varInitial() && parentScript) {
        uint32_t slot = v.varSlot();
        if (slot >= ArgSlot(0) && slot < LocalSlot(frame.script, 0)) {
            uint32_t argc = GET_ARGC(frame.parentpc);
            SSAValue argv = parentAnalysis->poppedValue(frame.parentpc, argc - 1 - (slot - ArgSlot(0)));
            return foldValue(CrossSSAValue(frame.parent, argv));
        }
    }

    if (v.kind() == SSAValue::PUSHED) {
        jsbytecode *pc = frame.script->code + v.pushedOffset();

        switch (JSOp(*pc)) {
          case JSOP_THIS:
            if (parentScript) {
                uint32_t argc = GET_ARGC(frame.parentpc);
                SSAValue thisv = parentAnalysis->poppedValue(frame.parentpc, argc);
                return foldValue(CrossSSAValue(frame.parent, thisv));
            }
            break;

          case JSOP_CALL: {
            /*
             * If there is a single inline callee with a single return site,
             * propagate back to that.
             */
            JSScript *callee = NULL;
            uint32_t calleeFrame = INVALID_FRAME;
            for (unsigned i = 0; i < numFrames(); i++) {
                if (iterFrame(i).parent == cv.frame && iterFrame(i).parentpc == pc) {
                    if (callee)
                        return cv;  /* Multiple callees */
                    callee = iterFrame(i).script;
                    calleeFrame = iterFrame(i).index;
                }
            }
            if (callee && callee->analysis()->numReturnSites() == 1) {
                ScriptAnalysis *analysis = callee->analysis();
                uint32_t offset = 0;
                while (offset < callee->length) {
                    jsbytecode *pc = callee->code + offset;
                    if (analysis->maybeCode(pc) && JSOp(*pc) == JSOP_RETURN)
                        return foldValue(CrossSSAValue(calleeFrame, analysis->poppedValue(pc, 0)));
                    offset += GetBytecodeLength(pc);
                }
            }
            break;
          }

          case JSOP_TOID: {
            /*
             * TOID acts as identity for integers, so to get better precision
             * we should propagate its popped values forward if it acted as
             * identity.
             */
            ScriptAnalysis *analysis = frame.script->analysis();
            SSAValue toidv = analysis->poppedValue(pc, 0);
            if (analysis->getValueTypes(toidv)->getKnownTypeTag(cx) == JSVAL_TYPE_INT32)
                return foldValue(CrossSSAValue(cv.frame, toidv));
            break;
          }

          default:;
        }
    }

    return cv;
}

#ifdef DEBUG

void
ScriptAnalysis::printSSA(JSContext *cx)
{
    AutoEnterAnalysis enter(cx);

    printf("\n");

    for (unsigned offset = 0; offset < script->length; offset++) {
        Bytecode *code = maybeCode(offset);
        if (!code)
            continue;

        jsbytecode *pc = script->code + offset;

        PrintBytecode(cx, script, pc);

        SlotValue *newv = code->newValues;
        if (newv) {
            while (newv->slot) {
                if (newv->value.kind() != SSAValue::PHI || newv->value.phiOffset() != offset) {
                    newv++;
                    continue;
                }
                printf("  phi ");
                newv->value.print();
                printf(" [");
                for (unsigned i = 0; i < newv->value.phiLength(); i++) {
                    if (i)
                        printf(",");
                    newv->value.phiValue(i).print();
                }
                printf("]\n");
                newv++;
            }
        }

        unsigned nuses = GetUseCount(script, offset);
        unsigned xuses = ExtendedUse(pc) ? nuses + 1 : nuses;

        for (unsigned i = 0; i < xuses; i++) {
            printf("  popped%d: ", i);
            code->poppedValues[i].print();
            printf("\n");
        }
    }

    printf("\n"); 
}

void
SSAValue::print() const
{
    switch (kind()) {

      case EMPTY:
        printf("empty");
        break;

      case PUSHED:
        printf("pushed:%05u#%u", pushedOffset(), pushedIndex());
        break;

      case VAR:
        if (varInitial())
            printf("initial:%u", varSlot());
        else
            printf("write:%05u", varOffset());
        break;

      case PHI:
        printf("phi:%05u#%u", phiOffset(), phiSlot());
        break;

      default:
        JS_NOT_REACHED("Bad kind");
    }
}

void
ScriptAnalysis::assertMatchingDebugMode()
{
    JS_ASSERT(!!script->compartment()->debugMode() == !!originalDebugMode_);
}

#endif  /* DEBUG */

} /* namespace analyze */
} /* namespace js */
