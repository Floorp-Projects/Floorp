/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsanalyze.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/PodOperations.h"

#include "jscompartment.h"
#include "jscntxt.h"

#include "jsanalyzeinlines.h"
#include "jsinferinlines.h"
#include "jsopcodeinlines.h"

using namespace js;
using namespace js::analyze;

using mozilla::DebugOnly;
using mozilla::PodCopy;
using mozilla::PodZero;

/////////////////////////////////////////////////////////////////////
// Bytecode
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void
analyze::PrintBytecode(JSContext *cx, HandleScript script, jsbytecode *pc)
{
    printf("#%u:", script->id());
    Sprinter sprinter(cx);
    if (!sprinter.init())
        return;
    js_Disassemble1(cx, script, pc, pc - script->code, true, &sprinter);
    fprintf(stdout, "%s", sprinter.string());
}
#endif

/////////////////////////////////////////////////////////////////////
// Bytecode Analysis
/////////////////////////////////////////////////////////////////////

inline bool
ScriptAnalysis::addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump, unsigned *forwardLoop,
                        unsigned stackDepth)
{
    JS_ASSERT(offset < script_->length);

    Bytecode *&code = codeArray[offset];
    if (!code) {
        code = cx->analysisLifoAlloc().new_<Bytecode>();
        if (!code) {
            setOOM(cx);
            return false;
        }
        code->stackDepth = stackDepth;
    }
    JS_ASSERT(code->stackDepth == stackDepth);

    code->jumpTarget = true;

    if (offset < *currentOffset) {
        hasLoops_ = true;

        if (code->analyzed) {
            /*
             * Backedge in a do-while loop, the body has been analyzed. Rewalk
             * the body to set inLoop bits.
             */
            for (unsigned i = offset; i <= *currentOffset; i++) {
                Bytecode *code = maybeCode(i);
                if (code)
                    code->inLoop = true;
            }
        } else {
            /*
             * Backedge in a while/for loop, whose body has not been analyzed
             * due to a lack of fallthrough at the loop head. Roll back the
             * offset to analyze the body.
             */
            if (*forwardJump == 0)
                *forwardJump = *currentOffset;
            if (*forwardLoop == 0)
                *forwardLoop = *currentOffset;
            *currentOffset = offset;
        }
    } else if (offset > *forwardJump) {
        *forwardJump = offset;
    }

    return true;
}

void
ScriptAnalysis::analyzeBytecode(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);
    JS_ASSERT(!ranBytecode());
    LifoAlloc &alloc = cx->analysisLifoAlloc();

    numSlots = TotalSlots(script_);

    unsigned length = script_->length;
    codeArray = alloc.newArray<Bytecode*>(length);
    escapedSlots = alloc.newArray<bool>(numSlots);

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
     * Conservatively use 'argumentsHasVarBinding' instead of 'needsArgsObj'
     * (needsArgsObj requires SSA which requires escapedSlots). Lastly, the
     * debugger can access any local at any time. Even though debugger
     * reads/writes are monitored by the DebugScopeProxy, this monitoring
     * updates the flow-insensitive type sets, so we cannot use SSA.
     */

    PodZero(escapedSlots, numSlots);

    bool allVarsAliased = script_->compartment()->debugMode();
    bool allArgsAliased = allVarsAliased || script_->argumentsHasVarBinding();

    RootedScript script(cx, script_);
    for (BindingIter bi(script); bi; bi++) {
        if (bi->kind() == ARGUMENT)
            escapedSlots[ArgSlot(bi.frameIndex())] = allArgsAliased || bi->aliased();
        else
            escapedSlots[LocalSlot(script_, bi.frameIndex())] = allVarsAliased || bi->aliased();
    }

    /*
     * If the script is in debug mode, JS_SetFrameReturnValue can be called at
     * any safe point.
     */
    if (cx->compartment()->debugMode())
        usesReturnValue_ = true;

    bool heavyweight = script_->function() && script_->function()->isHeavyweight();

    isIonInlineable = true;
    if (heavyweight || cx->compartment()->debugMode())
        isIonInlineable = false;

    modifiesArguments_ = false;
    if (heavyweight)
        modifiesArguments_ = true;

    canTrackVars = true;

    /*
     * If we are in the middle of one or more jumps, the offset of the highest
     * target jumping over this bytecode.  Includes implicit jumps from
     * try/catch/finally blocks.
     */
    unsigned forwardJump = 0;

    /* If we are in the middle of a loop, the offset of the highest backedge. */
    unsigned forwardLoop = 0;

    /*
     * If we are in the middle of a try block, the offset of the highest
     * catch/finally/enditer.
     */
    unsigned forwardCatch = 0;

    /* Fill in stack depth and definitions at initial bytecode. */
    Bytecode *startcode = alloc.new_<Bytecode>();
    if (!startcode) {
        setOOM(cx);
        return;
    }

    startcode->stackDepth = 0;
    codeArray[0] = startcode;

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
        jsbytecode *pc = script_->code + offset;

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

        /*
         * Update info about bytecodes inside loops, which may have been
         * analyzed before the backedge was seen.
         */
        if (forwardLoop) {
            code->inLoop = true;
            if (forwardLoop <= offset)
                forwardLoop = 0;
        }

        if (code->analyzed) {
            /* No need to reanalyze, see Bytecode::mergeDefines. */
            continue;
        }

        code->analyzed = true;

        if (forwardCatch)
            code->inTryBlock = true;

        if (script_->hasBreakpointsAt(pc)) {
            code->safePoint = true;
            canTrackVars = false;
            isIonInlineable = false;
        }

        unsigned stackDepth = code->stackDepth;

        if (!forwardJump)
            code->unconditional = true;

        unsigned nuses = GetUseCount(script_, offset);
        unsigned ndefs = GetDefCount(script_, offset);

        JS_ASSERT(stackDepth >= nuses);
        stackDepth -= nuses;
        stackDepth += ndefs;

        switch (op) {

          case JSOP_RETURN:
          case JSOP_STOP:
            numReturnSites_++;
            break;

          case JSOP_SETRVAL:
          case JSOP_POPV:
            usesReturnValue_ = true;
            isIonInlineable = false;
            break;

          case JSOP_NAME:
          case JSOP_CALLNAME:
          case JSOP_BINDNAME:
          case JSOP_SETNAME:
          case JSOP_DELNAME:
          case JSOP_GETALIASEDVAR:
          case JSOP_CALLALIASEDVAR:
          case JSOP_SETALIASEDVAR:
          case JSOP_LAMBDA:
            usesScopeChain_ = true;
            break;

          case JSOP_DEFFUN:
          case JSOP_DEFVAR:
          case JSOP_DEFCONST:
          case JSOP_SETCONST:
            usesScopeChain_ = true; // Requires access to VarObj via ScopeChain.
            canTrackVars = false;
            isIonInlineable = false;
            break;

          case JSOP_EVAL:
            canTrackVars = false;
            isIonInlineable = false;
            break;

          case JSOP_ENTERWITH:
            canTrackVars = false;
            isIonInlineable = false;
            break;

          case JSOP_ENTERLET0:
          case JSOP_ENTERLET1:
          case JSOP_ENTERBLOCK:
          case JSOP_LEAVEBLOCK:
            isIonInlineable = false;
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
            isIonInlineable = false;
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            int32_t low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            int32_t high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
                return;
            getCode(defaultOffset).safePoint = true;

            for (int32_t i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset) {
                    if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
                        return;
                }
                getCode(targetOffset).safePoint = true;
                pc2 += JUMP_OFFSET_LEN;
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
            isIonInlineable = false;
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    /* This will overestimate try block code, for multiple catch/finally. */
                    if (catchOffset > forwardCatch)
                        forwardCatch = catchOffset;

                    if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP) {
                        if (!addJump(cx, catchOffset, &nextOffset, &forwardJump, &forwardLoop, stackDepth))
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
                if (local >= script_->nfixed) {
                    localsAliasStack_ = true;
                    break;
                }
            }
            break;
          }

          case JSOP_CALLLOCAL:
          case JSOP_SETLOCAL: {
            uint32_t local = GET_SLOTNO(pc);
            if (local >= script_->nfixed) {
                localsAliasStack_ = true;
                break;
            }
            break;
          }

          case JSOP_SETARG:
            modifiesArguments_ = true;
            break;

          case JSOP_GETPROP:
          case JSOP_CALLPROP:
          case JSOP_LENGTH:
          case JSOP_GETELEM:
          case JSOP_CALLELEM:
            numPropertyReads_++;
            break;

          case JSOP_THROW:
          case JSOP_EXCEPTION:
          case JSOP_DEBUGGER:
            isIonInlineable = false;
            break;

          /* Additional opcodes which can be both compiled both normally and inline. */
          case JSOP_ARGUMENTS:
          case JSOP_FUNCALL:
          case JSOP_FUNAPPLY:
          case JSOP_CALLEE:
          case JSOP_NOP:
          case JSOP_UNDEFINED:
          case JSOP_GOTO:
          case JSOP_DEFAULT:
          case JSOP_IFEQ:
          case JSOP_IFNE:
          case JSOP_ITERNEXT:
          case JSOP_DUP:
          case JSOP_DUP2:
          case JSOP_SWAP:
          case JSOP_PICK:
          case JSOP_BITOR:
          case JSOP_BITXOR:
          case JSOP_BITAND:
          case JSOP_LT:
          case JSOP_LE:
          case JSOP_GT:
          case JSOP_GE:
          case JSOP_EQ:
          case JSOP_NE:
          case JSOP_LSH:
          case JSOP_RSH:
          case JSOP_URSH:
          case JSOP_ADD:
          case JSOP_SUB:
          case JSOP_MUL:
          case JSOP_DIV:
          case JSOP_MOD:
          case JSOP_NOT:
          case JSOP_BITNOT:
          case JSOP_NEG:
          case JSOP_POS:
          case JSOP_DELPROP:
          case JSOP_DELELEM:
          case JSOP_TYPEOF:
          case JSOP_TYPEOFEXPR:
          case JSOP_VOID:
          case JSOP_TOID:
          case JSOP_SETELEM:
          case JSOP_IMPLICITTHIS:
          case JSOP_DOUBLE:
          case JSOP_STRING:
          case JSOP_ZERO:
          case JSOP_ONE:
          case JSOP_NULL:
          case JSOP_FALSE:
          case JSOP_TRUE:
          case JSOP_OR:
          case JSOP_AND:
          case JSOP_CASE:
          case JSOP_STRICTEQ:
          case JSOP_STRICTNE:
          case JSOP_ITER:
          case JSOP_MOREITER:
          case JSOP_ENDITER:
          case JSOP_POP:
          case JSOP_GETARG:
          case JSOP_CALLARG:
          case JSOP_BINDGNAME:
          case JSOP_UINT16:
          case JSOP_NEWINIT:
          case JSOP_NEWARRAY:
          case JSOP_NEWOBJECT:
          case JSOP_ENDINIT:
          case JSOP_INITPROP:
          case JSOP_INITELEM:
          case JSOP_INITELEM_ARRAY:
          case JSOP_SETPROP:
          case JSOP_IN:
          case JSOP_INSTANCEOF:
          case JSOP_LINENO:
          case JSOP_ENUMELEM:
          case JSOP_CONDSWITCH:
          case JSOP_LABEL:
          case JSOP_RETRVAL:
          case JSOP_GETGNAME:
          case JSOP_CALLGNAME:
          case JSOP_GETINTRINSIC:
          case JSOP_SETINTRINSIC:
          case JSOP_BINDINTRINSIC:
          case JSOP_CALLINTRINSIC:
          case JSOP_SETGNAME:
          case JSOP_REGEXP:
          case JSOP_OBJECT:
          case JSOP_UINT24:
          case JSOP_GETXPROP:
          case JSOP_INT8:
          case JSOP_INT32:
          case JSOP_HOLE:
          case JSOP_LOOPHEAD:
          case JSOP_LOOPENTRY:
          case JSOP_NOTEARG:
          case JSOP_REST:
            break;

          default:
            isIonInlineable = false;
            break;
        }

        bool jump = IsJumpOpcode(op);

        /* Check basic jump opcodes, which may or may not have a fallthrough. */
        if (jump) {
            /* Case instructions do not push the lvalue back when branching. */
            unsigned newStackDepth = stackDepth;
            if (op == JSOP_CASE)
                newStackDepth--;

            unsigned targetOffset = offset + GET_JUMP_OFFSET(pc);
            if (!addJump(cx, targetOffset, &nextOffset, &forwardJump, &forwardLoop, newStackDepth))
                return;

            if (op == JSOP_CASE || op == JSOP_DEFAULT)
                getCode(targetOffset).safePoint = true;
        }

        /* Handle any fallthrough from this opcode. */
        if (BytecodeFallsThrough(op)) {
            JS_ASSERT(successorOffset < script_->length);

            Bytecode *&nextcode = codeArray[successorOffset];

            if (!nextcode) {
                nextcode = alloc.new_<Bytecode>();
                if (!nextcode) {
                    setOOM(cx);
                    return;
                }
                nextcode->stackDepth = stackDepth;
            }
            JS_ASSERT(nextcode->stackDepth == stackDepth);

            if (jump)
                nextcode->jumpFallthrough = true;

            /* Treat the fallthrough of a branch instruction as a jump target. */
            if (jump)
                nextcode->jumpTarget = true;
            else
                nextcode->fallthrough = true;
        }
    }

    JS_ASSERT(!failed());
    JS_ASSERT(forwardJump == 0 && forwardLoop == 0 && forwardCatch == 0);

    ranBytecode_ = true;

    /*
     * Always ensure that a script's arguments usage has been analyzed before
     * entering the script. This allows the functionPrologue to ensure that
     * arguments are always created eagerly which simplifies interp logic.
     */
    if (!script_->analyzedArgsUsage())
        analyzeSSA(cx);
}

/////////////////////////////////////////////////////////////////////
// Lifetime Analysis
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::analyzeLifetimes(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis && !ranLifetimes() && !failed());

    if (!ranBytecode()) {
        analyzeBytecode(cx);
        if (failed())
            return;
    }

    LifoAlloc &alloc = cx->analysisLifoAlloc();

    lifetimes = alloc.newArray<LifetimeVariable>(numSlots);
    if (!lifetimes) {
        setOOM(cx);
        return;
    }
    PodZero(lifetimes, numSlots);

    /*
     * Variables which are currently dead. On forward branches to locations
     * where these are live, they need to be marked as live.
     */
    LifetimeVariable **saved = cx->pod_calloc<LifetimeVariable*>(numSlots);
    if (!saved) {
        setOOM(cx);
        return;
    }
    unsigned savedCount = 0;

    LoopAnalysis *loop = NULL;

    uint32_t offset = script_->length - 1;
    while (offset < script_->length) {
        Bytecode *code = maybeCode(offset);
        if (!code) {
            offset--;
            continue;
        }

        if (loop && code->safePoint)
            loop->hasSafePoints = true;

        jsbytecode *pc = script_->code + offset;

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
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset + tn->start;
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
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (!slotEscapes(slot))
                addVariable(cx, lifetimes[slot], offset, saved, savedCount);
            break;
          }

          case JSOP_SETARG:
          case JSOP_SETLOCAL: {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (!slotEscapes(slot))
                killVariable(cx, lifetimes[slot], offset, saved, savedCount);
            break;
          }

          case JSOP_TABLESWITCH:
            /* Restore all saved variables. :FIXME: maybe do this precisely. */
            for (unsigned i = 0; i < savedCount; i++) {
                LifetimeVariable &var = *saved[i];
                var.lifetime = alloc.new_<Lifetime>(offset, var.savedEnd, var.saved);
                if (!var.lifetime) {
                    js_free(saved);
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

          case JSOP_LOOPENTRY:
            getCode(offset).loop = loop;
            break;

          default:;
        }

        if (IsJumpOpcode(op)) {
            /*
             * Forward jumps need to pull in all variables which are live at
             * their target offset --- the variables live before the jump are
             * the union of those live at the fallthrough and at the target.
             */
            uint32_t targetOffset = FollowBranch(cx, script_, offset);

            /*
             * Watch for 'continue' statements in the loop body, which are
             * jumps to the entry offset separate from the initial jump.
             */
            if (loop && loop->entry == targetOffset && loop->entry > loop->lastBlock)
                loop->lastBlock = loop->entry;

            if (targetOffset < offset) {
                /* This is a loop back edge, no lifetime to pull in yet. */

#ifdef DEBUG
                JSOp nop = JSOp(script_->code[targetOffset]);
                JS_ASSERT(nop == JSOP_LOOPHEAD);
#endif

                /*
                 * If we already have a loop, it is an outer loop and we
                 * need to prune the last block in the loop --- we do not
                 * track 'continue' statements for outer loops.
                 */
                if (loop && loop->entry > loop->lastBlock)
                    loop->lastBlock = loop->entry;

                LoopAnalysis *nloop = alloc.new_<LoopAnalysis>();
                if (!nloop) {
                    js_free(saved);
                    setOOM(cx);
                    return;
                }
                PodZero(nloop);

                if (loop) {
                    loop->hasCallsLoops = true;
                    nloop->depth = loop->depth + 1;
                }

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

                    jsbytecode *entrypc = script_->code + entry;

                    if (JSOp(*entrypc) == JSOP_GOTO)
                        loop->entry = entry + GET_JUMP_OFFSET(entrypc);
                    else
                        loop->entry = targetOffset;
                } else {
                    /* Do-while loop at the start of the script. */
                    loop->entry = targetOffset;
                }
                JS_ASSERT(script_->code[loop->entry] == JSOP_LOOPHEAD ||
                          script_->code[loop->entry] == JSOP_LOOPENTRY);
            } else {
                for (unsigned i = 0; i < savedCount; i++) {
                    LifetimeVariable &var = *saved[i];
                    JS_ASSERT(!var.lifetime && var.saved);
                    if (var.live(targetOffset)) {
                        /*
                         * Jumping to a place where this variable is live. Make a new
                         * lifetime segment for the variable.
                         */
                        var.lifetime = alloc.new_<Lifetime>(offset, var.savedEnd, var.saved);
                        if (!var.lifetime) {
                            js_free(saved);
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

    js_free(saved);

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
        var.lifetime = cx->analysisLifoAlloc().new_<Lifetime>(offset, var.savedEnd, var.saved);
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
        Lifetime *lifetime = cx->analysisLifoAlloc().new_<Lifetime>(offset, var.savedEnd, var.saved);
        if (!lifetime) {
            setOOM(cx);
            return;
        }
        if (!var.saved)
            saved[savedCount++] = &var;
        var.saved = lifetime;
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
         * We set the new interval's savedEnd to 0, since it will always be
         * adjacent to the old interval, so it never needs to be extended.
         */
        var.lifetime = cx->analysisLifoAlloc().new_<Lifetime>(start, 0, var.lifetime);
        if (!var.lifetime) {
            setOOM(cx);
            return;
        }
        var.lifetime->end = offset;
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
     * Consider this code:
     *
     *   while (...) { (#1)
     *       use x;    (#2)
     *       ...
     *       x = ...;  (#3)
     *       ...
     *   }             (#4)
     *
     * Just before analyzing the while statement, there would be a live range
     * from #1..#2 and a "point range" at #3. The job of extendVariable is to
     * create a new live range from #3..#4.
     *
     * However, more extensions may be required if the definition of x is
     * conditional. Consider the following.
     *
     *   while (...) {     (#1)
     *       use x;        (#2)
     *       ...
     *       if (...)      (#5)
     *           x = ...;  (#3)
     *       ...
     *   }                 (#4)
     *
     * Assume that x is not used after the loop. Then, before extendVariable is
     * run, the live ranges would be the same as before (#1..#2 and #3..#3). We
     * still need to create a range from #3..#4. But, since the assignment at #3
     * may never run, we also need to create a range from #2..#3. This is done
     * as follows.
     *
     * Each time we create a Lifetime, we store the start of the most recently
     * seen sequence of conditional code in the Lifetime's savedEnd field. So,
     * when creating the Lifetime at #2, we set the Lifetime's savedEnd to
     * #5. (The start of the most recent conditional is cached in each
     * variable's savedEnd field.) Consequently, extendVariable is able to
     * create a new interval from #2..#5 using the savedEnd field of the
     * existing #1..#2 interval.
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
            Lifetime *tail = cx->analysisLifoAlloc().new_<Lifetime>(savedEnd, 0, segment->next);
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

/////////////////////////////////////////////////////////////////////
// SSA Analysis
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::analyzeSSA(JSContext *cx)
{
    JS_ASSERT(cx->compartment()->activeAnalysis && !ranSSA() && !failed());

    if (!ranLifetimes()) {
        analyzeLifetimes(cx);
        if (failed())
            return;
    }

    LifoAlloc &alloc = cx->analysisLifoAlloc();
    unsigned maxDepth = script_->nslots - script_->nfixed;

    /*
     * Current value of each variable and stack value. Empty for missing or
     * untracked entries, i.e. escaping locals and arguments.
     */
    SSAValueInfo *values = cx->pod_calloc<SSAValueInfo>(numSlots + maxDepth);
    if (!values) {
        setOOM(cx);
        return;
    }
    struct FreeSSAValues {
        SSAValueInfo *values;
        FreeSSAValues(SSAValueInfo *values) : values(values) {}
        ~FreeSSAValues() { js_free(values); }
    } free(values);

    SSAValueInfo *stack = values + numSlots;
    uint32_t stackDepth = 0;

    for (uint32_t slot = ArgSlot(0); slot < numSlots; slot++) {
        if (trackSlot(slot))
            values[slot].v.initInitial(slot);
    }

    /*
     * All target offsets for forward jumps we have seen (including ones whose
     * target we have advanced past). We lazily add pending entries at these
     * targets for the original value of variables modified before the branch
     * rejoins.
     */
    Vector<uint32_t> branchTargets(cx);

    /*
     * Subset of branchTargets which are exception handlers at future offsets.
     * Any new value of a variable modified before the target is reached is a
     * potential value at that target, along with the lazy original value.
     */
    Vector<uint32_t> exceptionTargets(cx);

    uint32_t offset = 0;
    while (offset < script_->length) {
        jsbytecode *pc = script_->code + offset;
        JSOp op = (JSOp)*pc;

        uint32_t successorOffset = offset + GetBytecodeLength(pc);

        Bytecode *code = maybeCode(pc);
        if (!code) {
            offset = successorOffset;
            continue;
        }

        if (code->exceptionEntry) {
            /* Remove from exception targets list, which reflects only future targets. */
            for (size_t i = 0; i < exceptionTargets.length(); i++) {
                if (exceptionTargets[i] == offset) {
                    exceptionTargets[i] = exceptionTargets.back();
                    exceptionTargets.popBack();
                    break;
                }
            }
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
            if (!pending) {
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
                    if (v.value.kind() != SSAValue::PHI || v.value.phiOffset() != offset) {
                        JS_ASSERT(v.value.phiOffset() < offset);
                        SSAValue ov = v.value;
                        if (!makePhi(cx, v.slot, offset, &ov))
                            return;
                        insertPhi(cx, ov, v.value);
                        v.value = ov;
                    }
                }
                if (code->fallthrough || code->jumpFallthrough)
                    mergeValue(cx, offset, values[v.slot].v, &v);
                mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets, offset - 1);
                values[v.slot].v = v.value;
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
                if (values[slot].v.kind() == SSAValue::PHI && values[slot].v.phiOffset() == offset) {
                    /* There is already a pending entry for this slot. */
                    continue;
                }
                SSAValue ov;
                if (!makePhi(cx, slot, offset, &ov))
                    return;
                if (code->fallthrough || code->jumpFallthrough)
                    insertPhi(cx, ov, values[slot].v);
                mergeBranchTarget(cx, values[slot], slot, branchTargets, offset - 1);
                values[slot].v = ov;
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
            bool exception = getCode(offset).exceptionEntry;
            Vector<SlotValue> *pending = code->pendingValues;
            for (unsigned i = 0; i < pending->length(); i++) {
                SlotValue &v = (*pending)[i];
                if (code->fallthrough || code->jumpFallthrough ||
                    (exception && values[v.slot].v.kind() != SSAValue::EMPTY)) {
                    mergeValue(cx, offset, values[v.slot].v, &v);
                }
                mergeBranchTarget(cx, values[v.slot], v.slot, branchTargets, offset);
                values[v.slot].v = v.value;
            }
            freezeNewValues(cx, offset);
        }

        unsigned nuses = GetUseCount(script_, offset);
        unsigned ndefs = GetDefCount(script_, offset);
        JS_ASSERT(stackDepth >= nuses);

        unsigned xuses = ExtendedUse(pc) ? nuses + 1 : nuses;

        if (xuses) {
            code->poppedValues = alloc.newArray<SSAValue>(xuses);
            if (!code->poppedValues) {
                setOOM(cx);
                return;
            }
            for (unsigned i = 0; i < nuses; i++) {
                SSAValue &v = stack[stackDepth - 1 - i].v;
                code->poppedValues[i] = v;
                v.clear();
            }
            if (xuses > nuses) {
                /*
                 * For SETLOCAL, etc. opcodes, add an extra popped value
                 * holding the value of the local before the op.
                 */
                uint32_t slot = GetBytecodeSlot(script_, pc);
                if (trackSlot(slot))
                    code->poppedValues[nuses] = values[slot].v;
                else
                    code->poppedValues[nuses].clear();
            }

            if (xuses) {
                SSAUseChain *useChains = alloc.newArray<SSAUseChain>(xuses);
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
            stack[stackDepth + i].v.initPushed(offset, i);

        unsigned xdefs = ExtendedDef(pc) ? ndefs + 1 : ndefs;
        if (xdefs) {
            code->pushedUses = alloc.newArray<SSAUseChain *>(xdefs);
            if (!code->pushedUses) {
                setOOM(cx);
                return;
            }
            PodZero(code->pushedUses, xdefs);
        }

        stackDepth += ndefs;

        if (BytecodeUpdatesSlot(op)) {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (trackSlot(slot)) {
                mergeBranchTarget(cx, values[slot], slot, branchTargets, offset);
                mergeExceptionTarget(cx, values[slot].v, slot, exceptionTargets);
                values[slot].v.initWritten(slot, offset);
            }
        }

        switch (op) {
          case JSOP_GETARG:
          case JSOP_GETLOCAL: {
            uint32_t slot = GetBytecodeSlot(script_, pc);
            if (trackSlot(slot)) {
                /*
                 * Propagate the current value of the local to the pushed value,
                 * and remember it with an extended use on the opcode.
                 */
                stack[stackDepth - 1].v = code->poppedValues[0] = values[slot].v;
            }
            break;
          }

          /* Short circuit ops which push back one of their operands. */

          case JSOP_MOREITER:
            stack[stackDepth - 2].v = code->poppedValues[0];
            break;

          case JSOP_INITPROP:
          case JSOP_INITPROP_GETTER:
          case JSOP_INITPROP_SETTER:
            stack[stackDepth - 1].v = code->poppedValues[1];
            break;

          case JSOP_SPREAD:
          case JSOP_INITELEM_INC:
            stack[stackDepth - 2].v = code->poppedValues[2];
            break;

          case JSOP_INITELEM_ARRAY:
            stack[stackDepth - 1].v = code->poppedValues[1];
            break;

          case JSOP_INITELEM:
          case JSOP_INITELEM_GETTER:
          case JSOP_INITELEM_SETTER:
            stack[stackDepth - 1].v = code->poppedValues[2];
            break;

          case JSOP_DUP:
            stack[stackDepth - 1].v = stack[stackDepth - 2].v = code->poppedValues[0];
            break;

          case JSOP_DUP2:
            stack[stackDepth - 1].v = stack[stackDepth - 3].v = code->poppedValues[0];
            stack[stackDepth - 2].v = stack[stackDepth - 4].v = code->poppedValues[1];
            break;

          case JSOP_SWAP:
            /* Swap is like pick 1. */
          case JSOP_PICK: {
            unsigned pickedDepth = (op == JSOP_SWAP ? 1 : pc[1]);
            stack[stackDepth - 1].v = code->poppedValues[pickedDepth];
            for (unsigned i = 0; i < pickedDepth; i++)
                stack[stackDepth - 2 - i].v = code->poppedValues[i];
            break;
          }

          /*
           * Switch and try blocks preserve the stack between the original op
           * and all case statements or exception/finally handlers.
           */

          case JSOP_TABLESWITCH: {
            unsigned defaultOffset = offset + GET_JUMP_OFFSET(pc);
            jsbytecode *pc2 = pc + JUMP_OFFSET_LEN;
            int32_t low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            int32_t high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            for (int32_t i = low; i <= high; i++) {
                unsigned targetOffset = offset + GET_JUMP_OFFSET(pc2);
                if (targetOffset != offset)
                    checkBranchTarget(cx, targetOffset, branchTargets, values, stackDepth);
                pc2 += JUMP_OFFSET_LEN;
            }

            checkBranchTarget(cx, defaultOffset, branchTargets, values, stackDepth);
            break;
          }

          case JSOP_TRY: {
            JSTryNote *tn = script_->trynotes()->vector;
            JSTryNote *tnlimit = tn + script_->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script_->mainOffset + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP) {
                        checkBranchTarget(cx, catchOffset, branchTargets, values, stackDepth);
                        checkExceptionTarget(cx, catchOffset, exceptionTargets);
                    }
                }
            }
            break;
          }

          case JSOP_THROW:
          case JSOP_RETURN:
          case JSOP_STOP:
          case JSOP_RETRVAL:
            mergeAllExceptionTargets(cx, values, exceptionTargets);
            break;

          default:;
        }

        if (IsJumpOpcode(op)) {
            unsigned targetOffset = FollowBranch(cx, script_, offset);
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

    /*
     * Now that we have full SSA information for the script, analyze whether
     * we can avoid creating the arguments object.
     */
    if (!script_->analyzedArgsUsage())
        script_->setNeedsArgsObj(needsArgsObj(cx));
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
    SSAPhiNode *node = cx->analysisLifoAlloc().new_<SSAPhiNode>();
    SSAValue *options = cx->analysisLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(0));
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

        SSAUseChain *use = cx->analysisLifoAlloc().new_<SSAUseChain>();
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
        cx->analysisLifoAlloc().newArray<SSAValue>(PhiNodeCapacity(node->length + 1));
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
                                  SSAValueInfo *values, uint32_t stackDepth)
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
            mergeValue(cx, targetOffset, values[v.slot].v, &v);
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
        uint32_t slot = StackSlot(script_, i);
        checkPendingValue(cx, values[slot].v, slot, pending);
    }
}

void
ScriptAnalysis::checkExceptionTarget(JSContext *cx, uint32_t catchOffset,
                                     Vector<uint32_t> &exceptionTargets)
{
    JS_ASSERT(getCode(catchOffset).exceptionEntry);

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
ScriptAnalysis::mergeBranchTarget(JSContext *cx, SSAValueInfo &value, uint32_t slot,
                                  const Vector<uint32_t> &branchTargets, uint32_t currentOffset)
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
     * Only look at new branch targets which have appeared since the last time
     * the variable was written.
     */
    for (int i = branchTargets.length() - 1; i >= value.branchSize; i--) {
        if (branchTargets[i] <= currentOffset)
            continue;

        const Bytecode &code = getCode(branchTargets[i]);

        Vector<SlotValue> *pending = code.pendingValues;
        checkPendingValue(cx, value.v, slot, pending);
    }

    value.branchSize = branchTargets.length();
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
ScriptAnalysis::mergeAllExceptionTargets(JSContext *cx, SSAValueInfo *values,
                                         const Vector<uint32_t> &exceptionTargets)
{
    for (unsigned i = 0; i < exceptionTargets.length(); i++) {
        Vector<SlotValue> *pending = getCode(exceptionTargets[i]).pendingValues;
        for (unsigned i = 0; i < pending->length(); i++) {
            const SlotValue &v = (*pending)[i];
            if (trackSlot(v.slot))
                mergeExceptionTarget(cx, values[v.slot].v, v.slot, exceptionTargets);
        }
    }
}

void
ScriptAnalysis::freezeNewValues(JSContext *cx, uint32_t offset)
{
    Bytecode &code = getCode(offset);

    Vector<SlotValue> *pending = code.pendingValues;
    code.pendingValues = NULL;

    unsigned count = pending->length();
    if (count == 0) {
        js_delete(pending);
        return;
    }

    code.newValues = cx->analysisLifoAlloc().newArray<SlotValue>(count + 1);
    if (!code.newValues) {
        setOOM(cx);
        return;
    }

    for (unsigned i = 0; i < count; i++)
        code.newValues[i] = (*pending)[i];
    code.newValues[count].slot = 0;
    code.newValues[count].value.clear();

    js_delete(pending);
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx, SeenVector &seen, const SSAValue &v)
{
    /*
     * trackUseChain is false for initial values of variables, which
     * cannot hold the script's arguments object.
     */
    if (!trackUseChain(v))
        return false;

    for (unsigned i = 0; i < seen.length(); i++) {
        if (v == seen[i])
            return false;
    }
    if (!seen.append(v)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return true;
    }

    SSAUseChain *use = useChain(v);
    while (use) {
        if (needsArgsObj(cx, seen, use))
            return true;
        use = use->next;
    }

    return false;
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx, SeenVector &seen, SSAUseChain *use)
{
    if (!use->popped)
        return needsArgsObj(cx, seen, SSAValue::PhiValue(use->offset, use->u.phi));

    jsbytecode *pc = script_->code + use->offset;
    JSOp op = JSOp(*pc);

    if (op == JSOP_POP || op == JSOP_POPN)
        return false;

    /* We can read the frame's arguments directly for f.apply(x, arguments). */
    if (op == JSOP_FUNAPPLY && GET_ARGC(pc) == 2 && use->u.which == 0)
        return false;

    /* arguments[i] can read fp->canonicalActualArg(i) directly. */
    if (op == JSOP_GETELEM && use->u.which == 1)
        return false;

    /* arguments.length length can read fp->numActualArgs() directly. */
    if (op == JSOP_LENGTH)
        return false;

    /* Allow assignments to non-closed locals (but not arguments). */

    if (op == JSOP_SETLOCAL) {
        uint32_t slot = GetBytecodeSlot(script_, pc);
        if (!trackSlot(slot))
            return true;
        return needsArgsObj(cx, seen, SSAValue::PushedValue(use->offset, 0)) ||
               needsArgsObj(cx, seen, SSAValue::WrittenVar(slot, use->offset));
    }

    if (op == JSOP_GETLOCAL)
        return needsArgsObj(cx, seen, SSAValue::PushedValue(use->offset, 0));

    return true;
}

bool
ScriptAnalysis::needsArgsObj(JSContext *cx)
{
    JS_ASSERT(script_->argumentsHasVarBinding());

    /*
     * Always construct arguments objects when in debug mode and for generator
     * scripts (generators can be suspended when speculation fails).
     */
    if (cx->compartment()->debugMode() || script_->isGenerator)
        return true;

    /*
     * If the script has dynamic name accesses which could reach 'arguments',
     * the parser will already have checked to ensure there are no explicit
     * uses of 'arguments' in the function. If there are such uses, the script
     * will be marked as definitely needing an arguments object.
     *
     * New accesses on 'arguments' can occur through 'eval' or the debugger
     * statement. In the former case, we will dynamically detect the use and
     * mark the arguments optimization as having failed.
     */
    if (script_->bindingsAccessedDynamically)
        return false;

    /*
     * Since let variables and are not tracked, we cannot soundly perform this
     * analysis in their presence.
     */
    if (localsAliasStack())
        return true;

    /*
     * If a script has explicit mentions of 'arguments' and formals which may
     * be stored as part of a call object, don't use lazy arguments. The
     * compiler can then assume that accesses through arguments[i] will be on
     * unaliased variables.
     */
    if (script_->funHasAnyAliasedFormal)
        return true;

    unsigned pcOff = script_->argumentsBytecode() - script_->code;

    SeenVector seen(cx);
    return needsArgsObj(cx, seen, SSAValue::PushedValue(pcOff, 0));
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
            if (analysis->getValueTypes(toidv)->getKnownTypeTag() == JSVAL_TYPE_INT32)
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
    types::AutoEnterAnalysis enter(cx);

    printf("\n");

    RootedScript script(cx, script_);
    for (unsigned offset = 0; offset < script_->length; offset++) {
        Bytecode *code = maybeCode(offset);
        if (!code)
            continue;

        jsbytecode *pc = script_->code + offset;

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

        unsigned nuses = GetUseCount(script_, offset);
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
    JS_ASSERT(!!script_->compartment()->debugMode() == !!originalDebugMode_);
}

#endif  /* DEBUG */
