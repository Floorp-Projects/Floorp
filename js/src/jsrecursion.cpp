/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   Andreas Gal <gal@mozilla.com>
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

#include "jsapi.h"

class RecursiveSlotMap : public SlotMap
{
  protected:
    unsigned downPostSlots;
    LIns *rval_ins;

  public:
    RecursiveSlotMap(TraceRecorder& rec, unsigned downPostSlots, LIns* rval_ins)
      : SlotMap(rec), downPostSlots(downPostSlots), rval_ins(rval_ins)
    {
    }

    JS_REQUIRES_STACK void
    adjustTypes()
    {
        /* Check if the return value should be promoted. */
        if (slots[downPostSlots].lastCheck == TypeCheck_Demote)
            rval_ins = mRecorder.lir->ins1(LIR_i2d, rval_ins);
        /* Adjust any global variables. */
        for (unsigned i = downPostSlots + 1; i < slots.length(); i++)
            adjustType(slots[i]);
    }

    JS_REQUIRES_STACK void
    adjustTail(TypeConsensus consensus)
    {
        /*
         * exit->sp_adj = ((downPostSlots + 1) * sizeof(double)) - nativeStackBase
         *
         * Store at exit->sp_adj - sizeof(double)
         */
        ptrdiff_t retOffset = downPostSlots * sizeof(double) -
                              mRecorder.tree->nativeStackBase;
        mRecorder.lir->insStore(mRecorder.addName(rval_ins, "rval_ins"),
                                 mRecorder.lirbuf->sp, retOffset, ACCSET_STACK);
    }
};

class UpRecursiveSlotMap : public RecursiveSlotMap
{
  public:
    UpRecursiveSlotMap(TraceRecorder& rec, unsigned downPostSlots, LIns* rval_ins)
      : RecursiveSlotMap(rec, downPostSlots, rval_ins)
    {
    }

    JS_REQUIRES_STACK void
    adjustTail(TypeConsensus consensus)
    {
        LirBuffer* lirbuf = mRecorder.lirbuf;
        LirWriter* lir = mRecorder.lir;

        /*
         * The native stack offset of the return value once this frame has
         * returned, is:
         *      -tree->nativeStackBase + downPostSlots * sizeof(double)
         *
         * Note, not +1, since the offset is 0-based.
         *
         * This needs to be adjusted down one frame. The amount to adjust must
         * be the amount down recursion added, which was just guarded as
         * |downPostSlots|. So the offset is:
         *
         *      -tree->nativeStackBase + downPostSlots * sizeof(double) -
         *                                   downPostSlots * sizeof(double)
         * Or:
         *      -tree->nativeStackBase
         *
         * This makes sense because this slot is just above the highest sp for
         * the down frame.
         */
        lir->insStore(rval_ins, lirbuf->sp, -mRecorder.tree->nativeStackBase, ACCSET_STACK);

        lirbuf->sp = lir->ins2(LIR_addp, lirbuf->sp,
                               lir->insImmWord(-int(downPostSlots) * sizeof(double)));
        lir->insStore(lirbuf->sp, lirbuf->state, offsetof(TracerState, sp), ACCSET_OTHER);
        lirbuf->rp = lir->ins2(LIR_addp, lirbuf->rp,
                               lir->insImmWord(-int(sizeof(FrameInfo*))));
        lir->insStore(lirbuf->rp, lirbuf->state, offsetof(TracerState, rp), ACCSET_OTHER);
    }
};

#if defined DEBUG
JS_REQUIRES_STACK void
TraceRecorder::assertDownFrameIsConsistent(VMSideExit* anchor, FrameInfo* fi)
{
    JS_ASSERT(anchor->recursive_down);
    JS_ASSERT(anchor->recursive_down->callerHeight == fi->callerHeight);

    unsigned downPostSlots = fi->callerHeight;
    JSValueType* typeMap = fi->get_typemap();

    captureStackTypes(1, typeMap);
    const JSValueType* m1 = anchor->recursive_down->get_typemap();
    for (unsigned i = 0; i < downPostSlots; i++) {
        if (m1[i] == typeMap[i])
            continue;
        if ((typeMap[i] == JSVAL_TYPE_INT32 && m1[i] == JSVAL_TYPE_DOUBLE) ||
            (typeMap[i] == JSVAL_TYPE_DOUBLE && m1[i] == JSVAL_TYPE_INT32)) {
            continue;
        }
        JS_NOT_REACHED("invalid RECURSIVE_MISMATCH exit");
    }
    JS_ASSERT(memcmp(anchor->recursive_down, fi, sizeof(FrameInfo)) == 0);
}
#endif

JS_REQUIRES_STACK VMSideExit*
TraceRecorder::downSnapshot(FrameInfo* downFrame)
{
    JS_ASSERT(!pendingSpecializedNative);

    /* Build the typemap the exit will have. Note extra stack slot for return value. */
    unsigned downPostSlots = downFrame->callerHeight;
    unsigned ngslots = tree->globalSlots->length();
    unsigned exitTypeMapLen = downPostSlots + 1 + ngslots;
    JSValueType* exitTypeMap = (JSValueType*)alloca(sizeof(JSValueType) * exitTypeMapLen);
    JSValueType* typeMap = downFrame->get_typemap();


    /* Add stack slots. */
    for (unsigned i = 0; i < downPostSlots; i++)
        exitTypeMap[i] = typeMap[i];

    /* Add the return type. */
    JS_ASSERT_IF(*cx->regs->pc != JSOP_RETURN, *cx->regs->pc == JSOP_STOP);
    if (*cx->regs->pc == JSOP_RETURN)
        exitTypeMap[downPostSlots] = determineSlotType(&stackval(-1));
    else
        exitTypeMap[downPostSlots] = JSVAL_TYPE_UNDEFINED;

    /* Add global types. */
    determineGlobalTypes(&exitTypeMap[downPostSlots + 1]);

    VMSideExit* exit = (VMSideExit*)
        traceMonitor->traceAlloc->alloc(sizeof(VMSideExit) + sizeof(JSValueType) * exitTypeMapLen);

    PodZero(exit);
    exit->from = fragment;
    exit->calldepth = 0;
    JS_ASSERT(unsigned(exit->calldepth) == callDepth);
    exit->numGlobalSlots = ngslots;
    exit->numStackSlots = downPostSlots + 1;
    exit->numStackSlotsBelowCurrentFrame = cx->fp()->down->argv ?
        nativeStackOffset(&cx->fp()->argv[-2]) / sizeof(double) : 0;
    exit->exitType = UNSTABLE_LOOP_EXIT;
    exit->block = cx->fp()->down->maybeBlockChain();
    exit->pc = downFrame->pc + JSOP_CALL_LENGTH;
    exit->imacpc = NULL;
    exit->sp_adj = ((downPostSlots + 1) * sizeof(double)) - tree->nativeStackBase;
    exit->rp_adj = exit->calldepth * sizeof(FrameInfo*);
    exit->nativeCalleeWord = 0;
    exit->lookupFlags = js_InferFlags(cx, 0);
    memcpy(exit->fullTypeMap(), exitTypeMap, sizeof(JSValueType) * exitTypeMapLen);
#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif
    return exit;
}

static JS_REQUIRES_STACK Value *
DownFrameSP(JSContext *cx)
{
    FrameRegsIter i(cx);
    ++i;
    JS_ASSERT(i.fp() == cx->fp()->down);
    return i.sp();
}

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::upRecursion()
{
    JS_ASSERT((JSOp)*cx->fp()->down->savedPC == JSOP_CALL);
    JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, cx->fp()->down->getScript(),
              cx->fp()->down->savedPC)].length == JSOP_CALL_LENGTH);

    JS_ASSERT(callDepth == 0);

    /*
     * If some operation involving interpreter frame slurping failed, go to
     * that code right away, and don't bother with emitting the up-recursive
     * guards again.
     */
    if (anchor && (anchor->exitType == RECURSIVE_EMPTY_RP_EXIT ||
        anchor->exitType == RECURSIVE_SLURP_MISMATCH_EXIT ||
        anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT)) {
        return slurpDownFrames(cx->fp()->down->savedPC);
    }

    jsbytecode* return_pc = cx->fp()->down->savedPC;
    jsbytecode* recursive_pc = return_pc + JSOP_CALL_LENGTH;

    /*
     * It is possible that the down frame isn't the same at runtime. It's not
     * enough to guard on the PC, since the typemap could be different as well.
     * To deal with this, guard that the FrameInfo on the callstack is 100%
     * identical.
     *
     * Note that though the counted slots is called "downPostSlots", this is
     * the number of slots after the CALL instruction has theoretically popped
     * callee/this/argv, but before the return value is pushed. This is
     * intended since the FrameInfo pushed by down recursion would not have
     * the return value yet. Instead, when closing the loop, the return value
     * becomes the sole stack type that deduces type stability.
     */
    unsigned totalSlots = NativeStackSlots(cx, 1);
    unsigned downPostSlots = totalSlots - NativeStackSlots(cx, 0);
    FrameInfo* fi = (FrameInfo*)alloca(sizeof(FrameInfo) + totalSlots * sizeof(JSValueType));
    fi->block = NULL;
    fi->pc = (jsbytecode*)return_pc;
    fi->imacpc = NULL;

    /*
     * Need to compute this from the down frame, since the stack could have
     * moved on this one.
     */
    fi->spdist = DownFrameSP(cx) - cx->fp()->down->slots();
    JS_ASSERT(cx->fp()->numActualArgs() == cx->fp()->down->numActualArgs());
    fi->set_argc(uint16(cx->fp()->numActualArgs()), false);
    fi->callerHeight = downPostSlots;
    fi->callerArgc = cx->fp()->down->numActualArgs();

    if (anchor && anchor->exitType == RECURSIVE_MISMATCH_EXIT) {
        /*
         * Case 0: Anchoring off a RECURSIVE_MISMATCH guard. Guard on this FrameInfo.
         * This is always safe because this point is only reached on simple "call myself"
         * recursive functions.
         */
#if defined DEBUG
        assertDownFrameIsConsistent(anchor, fi);
#endif
        fi = anchor->recursive_down;
    } else if (recursive_pc != fragment->root->ip) {
        /*
         * Case 1: Guess that down-recursion has to started back out, infer types
         * from the down frame.
         */
        captureStackTypes(1, fi->get_typemap());
    } else {
        /* Case 2: Guess that up-recursion is backing out, infer types from our Tree. */
        JS_ASSERT(tree->nStackTypes == downPostSlots + 1);
        JSValueType* typeMap = fi->get_typemap();
        for (unsigned i = 0; i < downPostSlots; i++)
            typeMap[i] = tree->typeMap[i];
    }

    fi = traceMonitor->frameCache->memoize(fi);

    /*
     * Guard that there are more recursive frames. If coming from an anchor
     * where this was already computed, don't bother doing it again.
     */
    if (!anchor || anchor->exitType != RECURSIVE_MISMATCH_EXIT) {
        VMSideExit* exit = snapshot(RECURSIVE_EMPTY_RP_EXIT);

        /* Guard that rp >= sr + 1 */
        guard(true,
              lir->ins2(LIR_gep, lirbuf->rp,
                        lir->ins2(LIR_addp,
                                  lir->insLoad(LIR_ldp, lirbuf->state,
                                               offsetof(TracerState, sor), ACCSET_OTHER),
                                  INS_CONSTWORD(sizeof(FrameInfo*)))),
              exit);
    }

    debug_only_printf(LC_TMRecorder, "guardUpRecursive fragment->root=%p fi=%p\n", (void*)fragment->root, (void*)fi);

    /* Guard that the FrameInfo above is the same FrameInfo pointer. */
    VMSideExit* exit = snapshot(RECURSIVE_MISMATCH_EXIT);
    LIns* prev_rp = lir->insLoad(LIR_ldp, lirbuf->rp, -int32_t(sizeof(FrameInfo*)), ACCSET_RSTACK);
    guard(true, lir->ins2(LIR_eqp, prev_rp, INS_CONSTPTR(fi)), exit);

    /*
     * Now it's time to try and close the loop. Get a special exit that points
     * at the down frame, after the return has been propagated up.
     */
    exit = downSnapshot(fi);

    LIns* rval_ins;
    if (*cx->regs->pc == JSOP_RETURN) {
        JS_ASSERT(!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT);
        rval_ins = get(&stackval(-1));
        JS_ASSERT(rval_ins);
    } else {
        rval_ins = INS_UNDEFINED();
    }

    JSValueType returnType = exit->stackTypeMap()[downPostSlots];
    if (returnType == JSVAL_TYPE_INT32) {
        JS_ASSERT(*cx->regs->pc == JSOP_RETURN);
        JS_ASSERT(determineSlotType(&stackval(-1)) == JSVAL_TYPE_INT32);
        JS_ASSERT(isPromoteInt(rval_ins));
        rval_ins = demote(lir, rval_ins);
    }

    UpRecursiveSlotMap slotMap(*this, downPostSlots, rval_ins);
    for (unsigned i = 0; i < downPostSlots; i++)
        slotMap.addSlot(exit->stackType(i));
    if (*cx->regs->pc == JSOP_RETURN)
        slotMap.addSlot(&stackval(-1));
    else
        slotMap.addSlot(JSVAL_TYPE_UNDEFINED);
    VisitGlobalSlots(slotMap, cx, *tree->globalSlots);
    if (recursive_pc == (jsbytecode*)fragment->root->ip) {
        debug_only_print0(LC_TMTracer, "Compiling up-recursive loop...\n");
    } else {
        debug_only_print0(LC_TMTracer, "Compiling up-recursive branch...\n");
        exit->exitType = RECURSIVE_UNLINKED_EXIT;
        exit->recursive_pc = recursive_pc;
    }
    JS_ASSERT(tree->recursion != Recursion_Disallowed);
    if (tree->recursion != Recursion_Detected)
        tree->recursion = Recursion_Unwinds;
    return closeLoop(slotMap, exit);
}

class SlurpInfo
{
public:
    unsigned curSlot;
    JSValueType* typeMap;
    VMSideExit* exit;
    unsigned slurpFailSlot;
};

/*
 * The three types of anchors that can cause this type of trace to be built are:
 *   RECURSIVE_SLURP_MISMATCH_EXIT
 *   RECURSIVE_SLURP_FAIL_EXIT
 *   RECURSIVE_EMPTY_RP_EXIT
 *
 * EMPTY_RP means that recursion is unwinding, but there are no more frames.
 * This triggers a "slurp trace" to be built. A slurp trace does three things:
 *   1) Checks to see if cx->fp returns to the same point the recursive trace
 *      is trying to unwind to.
 *   2) Pops the inline frame cx->fp, such that cx->fp is now cx->fp->down.
 *   3) Converts the new top-frame slots/sp into the tracer frame.
 *
 * SLURP_MISMATCH means that while trying to convert an interpreter frame,
 * it is owned by the same script, but does not return to the same pc. At this
 * point the frame has not been popped yet.
 *
 * SLURP_FAIL means that the interpreter frame has been popped, the return
 * value has been written to the native stack, but there was a type mismatch
 * while unboxing the interpreter slots.
 */
JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::slurpDownFrames(jsbytecode* return_pc)
{
    /* Missing - no go */
    if (cx->fp()->numActualArgs() != cx->fp()->numFormalArgs())
        RETURN_STOP_A("argc != nargs");

    LIns* argv_ins;
    unsigned frameDepth;
    unsigned downPostSlots;

    FrameRegsIter i(cx);
    LIns* fp_ins = addName(entryFrameIns(), "fp");

    /*
     * When first emitting slurp code, do so against the down frame. After
     * popping the interpreter frame, it is illegal to resume here, as the
     * down frame has been moved up. So all this code should be skipped if
     * anchoring off such an exit.
     */
    if (!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT) {
        fp_ins = addName(lir->insLoad(LIR_ldp, fp_ins, offsetof(JSStackFrame, down), ACCSET_OTHER),
                         "downFp");
        ++i;

        argv_ins = addName(lir->insLoad(LIR_ldp, fp_ins, offsetof(JSStackFrame, argv), ACCSET_OTHER),
                           "argv");

        /* If recovering from a SLURP_MISMATCH, all of this is unnecessary. */
        if (!anchor || anchor->exitType != RECURSIVE_SLURP_MISMATCH_EXIT) {
            /* fp->down should not be NULL. */
            guard(false, lir->insEqP_0(fp_ins), RECURSIVE_LOOP_EXIT);

            /* fp->down->argv should not be NULL. */
            guard(false, lir->insEqP_0(argv_ins), RECURSIVE_LOOP_EXIT);

            /*
             * Guard on the script being the same. This might seem unnecessary,
             * but it lets the recursive loop end cleanly if it doesn't match.
             * With only the pc check, it is harder to differentiate between
             * end-of-recursion and recursion-returns-to-different-pc.
             */
            guard(true,
                  lir->ins2(LIR_eqp,
                            addName(lir->insLoad(LIR_ldp, fp_ins,
                                                 JSStackFrame::offsetScript(), ACCSET_OTHER),
                                    "script"),
                            INS_CONSTPTR(cx->fp()->down->getScript())),
                  RECURSIVE_LOOP_EXIT);
        }

        /* fp->down->savedPC should be == pc. */
        guard(true,
              lir->ins2(LIR_eqp,
                        addName(lir->insLoad(LIR_ldp, fp_ins, offsetof(JSStackFrame, savedPC),  
                                             ACCSET_OTHER),
                                "savedPC"),
                        INS_CONSTPTR(return_pc)),
              RECURSIVE_SLURP_MISMATCH_EXIT);

        /* fp->down->argc should be == argc. */
        guard(true,
              lir->ins2(LIR_eqi,
                        addName(lir->insLoad(LIR_ldi, fp_ins, JSStackFrame::offsetNumActualArgs(),
                                             ACCSET_OTHER),
                                "argc"),
                        INS_CONST(cx->fp()->numActualArgs())),
              MISMATCH_EXIT);

        /* Pop the interpreter frame. */
        LIns* args[] = { lirbuf->state, cx_ins };
        guard(false, lir->insEqI_0(lir->insCall(&js_PopInterpFrame_ci, args)), MISMATCH_EXIT);

        /* Compute slots for the down frame. */
        downPostSlots = NativeStackSlots(cx, 1) - NativeStackSlots(cx, 0);
        frameDepth = 1;
    } else {
        /* Note: loading argv from fp, not fp->down. */
        argv_ins = addName(lir->insLoad(LIR_ldp, fp_ins, offsetof(JSStackFrame, argv), ACCSET_OTHER),
                           "argv");

        /* Slots for this frame, minus the return value. */
        downPostSlots = NativeStackSlots(cx, 0) - 1;
        frameDepth = 0;
    }

    /*
     * This is a special exit used as a template for the stack-slurping code.
     * LeaveTree will ignore all but the final slot, which contains the return
     * value. The slurpSlot variable keeps track of the last slot that has been
     * unboxed, as to avoid re-unboxing when taking a SLURP_FAIL exit.
     */
    unsigned numGlobalSlots = tree->globalSlots->length();
    unsigned safeSlots = NativeStackSlots(cx, frameDepth) + 1 + numGlobalSlots;
    jsbytecode* recursive_pc = return_pc + JSOP_CALL_LENGTH;
    VMSideExit* exit = (VMSideExit*)
        traceMonitor->traceAlloc->alloc(sizeof(VMSideExit) + sizeof(JSValueType) * safeSlots);
    PodZero(exit);
    exit->pc = (jsbytecode*)recursive_pc;
    exit->from = fragment;
    exit->exitType = RECURSIVE_SLURP_FAIL_EXIT;
    exit->numStackSlots = downPostSlots + 1;
    exit->numGlobalSlots = numGlobalSlots;
    exit->sp_adj = ((downPostSlots + 1) * sizeof(double)) - tree->nativeStackBase;
    exit->recursive_pc = recursive_pc;

    /*
     * Build the exit typemap. This may capture extra types, but they are
     * thrown away.
     */
    JSValueType* typeMap = exit->stackTypeMap();
    jsbytecode* oldpc = cx->regs->pc;
    cx->regs->pc = exit->pc;
    captureStackTypes(frameDepth, typeMap);
    cx->regs->pc = oldpc;
    if (!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT) {
        JS_ASSERT_IF(*cx->regs->pc != JSOP_RETURN, *cx->regs->pc == JSOP_STOP);
        if (*cx->regs->pc == JSOP_RETURN)
            typeMap[downPostSlots] = determineSlotType(&stackval(-1));
        else
            typeMap[downPostSlots] = JSVAL_TYPE_UNDEFINED;
    } else {
        typeMap[downPostSlots] = anchor->stackTypeMap()[anchor->numStackSlots - 1];
    }
    determineGlobalTypes(&typeMap[exit->numStackSlots]);
#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif

    /*
     * Return values are tricky because there are two cases. Anchoring off a
     * slurp failure (the second case) means the return value has already been
     * moved. However it can still be promoted to link trees together, so we
     * load it from the new location.
     *
     * In all other cases, the return value lives in the tracker and it can be
     * grabbed safely.
     */
    LIns* rval_ins;
    intptr_t offset = exit->sp_adj - sizeof(double);
    JSValueType returnType = exit->stackTypeMap()[downPostSlots];

    if (!anchor || anchor->exitType != RECURSIVE_SLURP_FAIL_EXIT) {
        /*
         * It is safe to read cx->regs->pc here because the frame hasn't
         * been popped yet. We're guaranteed to have a return or stop.
         */
        JSOp op = JSOp(*cx->regs->pc);
        JS_ASSERT(op == JSOP_RETURN || op == JSOP_STOP);

        if (op == JSOP_RETURN) {
            rval_ins = get(&stackval(-1));
            if (returnType == JSVAL_TYPE_INT32) {
                JS_ASSERT(determineSlotType(&stackval(-1)) == JSVAL_TYPE_INT32);
                JS_ASSERT(isPromoteInt(rval_ins));
                rval_ins = demote(lir, rval_ins);
            }
        } else {
            rval_ins = INS_UNDEFINED();
        }

        /*
         * The return value must be written out early, before slurping can fail,
         * otherwise it will not be available when there's a type mismatch.
         */
        lir->insStore(rval_ins, lirbuf->sp, offset, ACCSET_STACK);
    } else {
        switch (returnType)
        {
          case JSVAL_TYPE_BOOLEAN:
          case JSVAL_TYPE_UNDEFINED:
          case JSVAL_TYPE_INT32:
            rval_ins = lir->insLoad(LIR_ldi, lirbuf->sp, offset, ACCSET_STACK);
            break;
          case JSVAL_TYPE_DOUBLE:
            rval_ins = lir->insLoad(LIR_ldd, lirbuf->sp, offset, ACCSET_STACK);
            break;
          case JSVAL_TYPE_FUNOBJ:
          case JSVAL_TYPE_NONFUNOBJ:
          case JSVAL_TYPE_STRING:
          case JSVAL_TYPE_NULL:
            rval_ins = lir->insLoad(LIR_ldp, lirbuf->sp, offset, ACCSET_STACK);
            break;
          default:
            JS_NOT_REACHED("unknown type");
            RETURN_STOP_A("unknown type"); 
        }
    }

    /* Slurp */
    SlurpInfo info;
    info.curSlot = 0;
    info.exit = exit;
    info.typeMap = typeMap;
    info.slurpFailSlot = (anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT) ?
                         anchor->slurpFailSlot : 0;

    JSStackFrame *const fp = i.fp();

    /* callee */
    slurpSlot(argv_ins, -2 * ptrdiff_t(sizeof(Value)), &fp->argv[-2], &info);
    /* this */
    slurpSlot(argv_ins, -1 * ptrdiff_t(sizeof(Value)), &fp->argv[-1], &info);
    /* args[0..n] */
    for (unsigned i = 0; i < JS_MAX(fp->numActualArgs(), fp->numFormalArgs()); i++)
        slurpSlot(argv_ins, i * sizeof(Value), &fp->argv[i], &info);
    /* argsobj */
    slurpFrameObjPtrSlot(fp_ins, JSStackFrame::offsetArgsObj(), fp->addressArgsObj(), &info);
    /* scopeChain */
    slurpFrameObjPtrSlot(fp_ins, JSStackFrame::offsetScopeChain(), fp->addressScopeChain(), &info);
    /* vars */
    LIns* slots_ins = addName(lir->ins2(LIR_addp, fp_ins, INS_CONSTWORD(sizeof(JSStackFrame))),
                              "slots");
    for (unsigned i = 0; i < fp->getFixedCount(); i++)
        slurpSlot(slots_ins, i * sizeof(Value), &fp->slots()[i], &info);
    /* stack vals */
    unsigned nfixed = fp->getFixedCount();
    Value* stack = fp->base();
    LIns* stack_ins = addName(lir->ins2(LIR_addp,
                                        slots_ins,
                                        INS_CONSTWORD(nfixed * sizeof(Value))),
                              "stackBase");

    size_t limit = size_t(i.sp() - fp->base());
    if (anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT)
        limit--;
    else
        limit -= fp->numFormalArgs() + 2;
    for (size_t i = 0; i < limit; i++)
        slurpSlot(stack_ins, i * sizeof(Value), &stack[i], &info);

    JS_ASSERT(info.curSlot == downPostSlots);

    /* Jump back to the start */
    exit = copy(exit);
    exit->exitType = UNSTABLE_LOOP_EXIT;
#if defined JS_JIT_SPEW
    TreevisLogExit(cx, exit);
#endif

    RecursiveSlotMap slotMap(*this, downPostSlots, rval_ins);
    for (unsigned i = 0; i < downPostSlots; i++)
        slotMap.addSlot(typeMap[i]);
    if (*cx->regs->pc == JSOP_RETURN)
        slotMap.addSlot(&stackval(-1), typeMap[downPostSlots]);
    else
        slotMap.addSlot(JSVAL_TYPE_UNDEFINED);
    VisitGlobalSlots(slotMap, cx, *tree->globalSlots);
    debug_only_print0(LC_TMTracer, "Compiling up-recursive slurp...\n");
    exit = copy(exit);
    if (exit->recursive_pc == fragment->root->ip)
        exit->exitType = UNSTABLE_LOOP_EXIT;
    else
        exit->exitType = RECURSIVE_UNLINKED_EXIT;
    debug_only_printf(LC_TMTreeVis, "TREEVIS CHANGEEXIT EXIT=%p TYPE=%s\n", (void*)exit,
                      getExitName(exit->exitType));
    JS_ASSERT(tree->recursion >= Recursion_Unwinds);
    return closeLoop(slotMap, exit);
}

class ImportFrameSlotsVisitor : public SlotVisitorBase
{
    TraceRecorder &mRecorder;
public:
    ImportFrameSlotsVisitor(TraceRecorder &recorder) : mRecorder(recorder)
    {}

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitStackSlots(Value *vp, size_t count, JSStackFrame* fp) {
        /* N.B. vp may point to a JSObject*. */
        for (size_t i = 0; i < count; ++i)
            mRecorder.get(vp++);
        return true;
    }

    JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
    visitFrameObjPtr(JSObject **p, JSStackFrame* fp) {
        /* visitStackSlots only uses the address of its argument. */
        return visitStackSlots((Value *)p, 1, fp);
    }
};

JS_REQUIRES_STACK AbortableRecordingStatus
TraceRecorder::downRecursion()
{
    JSStackFrame* fp = cx->fp();
    JSScript *script = fp->getScript();
    if ((jsbytecode*)fragment->ip < script->code ||
        (jsbytecode*)fragment->ip >= script->code + script->length) {
        RETURN_STOP_A("inner recursive call must compile first");
    }

    /* Adjust the stack by the budget the down-frame needs. */
    int slots = NativeStackSlots(cx, 1) - NativeStackSlots(cx, 0);
    JS_ASSERT(unsigned(slots) ==
              NativeStackSlots(cx, 1) - fp->numActualArgs() - 2 - fp->getFixedCount() - 2);

    /* Guard that there is enough stack space. */
    JS_ASSERT(tree->maxNativeStackSlots >= tree->nativeStackBase / sizeof(double));
    int guardSlots = slots + tree->maxNativeStackSlots -
                     tree->nativeStackBase / sizeof(double);
    LIns* sp_top = lir->ins2(LIR_addp, lirbuf->sp, lir->insImmWord(guardSlots * sizeof(double)));
    guard(true, lir->ins2(LIR_ltp, sp_top, eos_ins), OOM_EXIT);

    /* Guard that there is enough call stack space. */
    LIns* rp_top = lir->ins2(LIR_addp, lirbuf->rp,
                             lir->insImmWord((tree->maxCallDepth + 1) * sizeof(FrameInfo*)));
    guard(true, lir->ins2(LIR_ltp, rp_top, eor_ins), OOM_EXIT);

    /*
     * For every slot in the new frame that is not in the tracker, create a load
     * in the tracker. This is necessary because otherwise snapshot() will see
     * missing imports and use the down frame, rather than the new frame.
     * This won't affect performance because the loads will be killed if not
     * used.
     */
    ImportFrameSlotsVisitor visitor(*this);
    VisitStackSlots(visitor, cx, callDepth);

    /* Add space for a new JIT frame. */
    lirbuf->sp = lir->ins2(LIR_addp, lirbuf->sp, lir->insImmWord(slots * sizeof(double)));
    lir->insStore(lirbuf->sp, lirbuf->state, offsetof(TracerState, sp), ACCSET_OTHER);
    lirbuf->rp = lir->ins2(LIR_addp, lirbuf->rp, lir->insImmWord(sizeof(FrameInfo*)));
    lir->insStore(lirbuf->rp, lirbuf->state, offsetof(TracerState, rp), ACCSET_OTHER);
    --callDepth;
    clearCurrentFrameSlotsFromTracker(nativeFrameTracker);

    /*
     * If the callee and caller have identical call sites, this is a down-
     * recursive loop. Otherwise something special happened. For example, a
     * recursive call that is unwinding could nest back down recursively again.
     * In this case, we build a fragment that ideally we'll never invoke
     * directly, but link from a down-recursive branch. The UNLINKED_EXIT tells
     * closeLoop() that the peer trees should match the recursive pc, not the
     * tree pc.
     */
    VMSideExit* exit;
    if ((jsbytecode*)fragment->root->ip == script->code)
        exit = snapshot(UNSTABLE_LOOP_EXIT);
    else
        exit = snapshot(RECURSIVE_UNLINKED_EXIT);
    exit->recursive_pc = script->code;
    debug_only_print0(LC_TMTracer, "Compiling down-recursive function call.\n");
    JS_ASSERT(tree->recursion != Recursion_Disallowed);
    tree->recursion = Recursion_Detected;
    return closeLoop(exit);
}

#if JS_BITS_PER_WORD == 32
JS_REQUIRES_STACK inline LIns*
TraceRecorder::slurpDoubleSlot(LIns* addr_ins, ptrdiff_t offset, VMSideExit* exit)
{
    LIns* tag_ins = lir->insLoad(LIR_ldi, addr_ins, offset + sTagOffset, ACCSET_OTHER);
    return unbox_number_as_double(addr_ins, offset, tag_ins, exit, ACCSET_OTHER);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::slurpObjectSlot(LIns* addr_ins, ptrdiff_t offset, JSValueType type, VMSideExit* exit)
{
    LIns* tag_ins = lir->insLoad(LIR_ldi, addr_ins, offset + sTagOffset, ACCSET_OTHER);
    return unbox_object(addr_ins, offset, tag_ins, type, exit, ACCSET_OTHER);
}

JS_REQUIRES_STACK inline LIns*
TraceRecorder::slurpNonDoubleObjectSlot(LIns* addr_ins, ptrdiff_t offset, JSValueType type, VMSideExit* exit)
{
    LIns* tag_ins = lir->insLoad(LIR_ldi, addr_ins, offset + sTagOffset, ACCSET_OTHER);
    return unbox_non_double_object(addr_ins, offset, tag_ins, type, exit, ACCSET_OTHER);
}
#elif JS_BITS_PER_WORD == 64
JS_REQUIRES_STACK inline LIns*
TraceRecorder::slurpDoubleSlot(LIns* addr_ins, ptrdiff_t offset, VMSideExit* exit)
{
    LIns* v_ins = lir->insLoad(LIR_ldq, addr_ins, offset, ACCSET_OTHER);
    return unbox_number_as_double(v_ins, exit);
}

JS_REQUIRES_STACK LIns*
TraceRecorder::slurpObjectSlot(LIns* addr_ins, ptrdiff_t offset, JSValueType type, VMSideExit* exit)
{
    LIns* v_ins = lir->insLoad(LIR_ldq, addr_ins, offset, ACCSET_OTHER);
    return unbox_object(v_ins, type, exit);
}

JS_REQUIRES_STACK inline LIns*
TraceRecorder::slurpNonDoubleObjectSlot(LIns* addr_ins, ptrdiff_t offset, JSValueType type, VMSideExit* exit)
{
    LIns* v_ins = lir->insLoad(LIR_ldq, addr_ins, offset, ACCSET_OTHER);
    return unbox_non_double_object(v_ins, type, exit);
}
#endif

JS_REQUIRES_STACK inline LIns*
TraceRecorder::slurpSlot(LIns* addr_ins, ptrdiff_t offset, Value* vp, VMSideExit* exit)
{
    if (exit->slurpType == JSVAL_TYPE_DOUBLE)
        return slurpDoubleSlot(addr_ins, offset, exit);
    if (exit->slurpType == JSVAL_TYPE_FUNOBJ || exit->slurpType == JSVAL_TYPE_NONFUNOBJ)
        return slurpObjectSlot(addr_ins, offset, exit->slurpType, exit);
    JSValueType type = exit->slurpType;
    return slurpNonDoubleObjectSlot(addr_ins, offset, type, exit);
}

JS_REQUIRES_STACK void
TraceRecorder::slurpSlot(LIns* addr_ins, ptrdiff_t offset, Value* vp, SlurpInfo* info)
{
    /* Don't re-read slots that aren't needed. */
    if (info->curSlot < info->slurpFailSlot) {
        info->curSlot++;
        return;
    }
    VMSideExit* exit = copy(info->exit);
    exit->slurpFailSlot = info->curSlot;
    exit->slurpType = info->typeMap[info->curSlot];

    /* Make sure that we don't try and record infinity branches */
    JS_ASSERT_IF(anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT &&
                 info->curSlot == info->slurpFailSlot,
                 anchor->slurpType != exit->slurpType);

    LIns* val = slurpSlot(addr_ins, offset, vp, exit);
    lir->insStore(val,
                  lirbuf->sp,
                  -tree->nativeStackBase + ptrdiff_t(info->curSlot) * sizeof(double),
                  ACCSET_STACK);
    info->curSlot++;
}

JS_REQUIRES_STACK void
TraceRecorder::slurpFrameObjPtrSlot(LIns* addr_ins, ptrdiff_t offset, JSObject** p, SlurpInfo* info)
{
    /* Don't re-read slots that aren't needed. */
    if (info->curSlot < info->slurpFailSlot) {
        info->curSlot++;
        return;
    }
    VMSideExit* exit = copy(info->exit);
    exit->slurpFailSlot = info->curSlot;
    exit->slurpType = info->typeMap[info->curSlot];

    /* Make sure that we don't try and record infinity branches */
    JS_ASSERT_IF(anchor && anchor->exitType == RECURSIVE_SLURP_FAIL_EXIT &&
                 info->curSlot == info->slurpFailSlot,
                 anchor->slurpType != exit->slurpType);

    LIns *val;
    LIns *ptr_val = lir->insLoad(LIR_ldp, addr_ins, offset, ACCSET_OTHER);
    LIns *ptr_is_null_ins = lir->insEqP_0(ptr_val);
    if (exit->slurpType == JSVAL_TYPE_NULL) {
        guard(true, ptr_is_null_ins, exit);
        val = INS_NULL();
    } else {
        JS_ASSERT(exit->slurpType == JSVAL_TYPE_NONFUNOBJ);
        guard(false, ptr_is_null_ins, exit);
        val = ptr_val;
    }

    lir->insStore(val,
                  lirbuf->sp,
                  -tree->nativeStackBase + ptrdiff_t(info->curSlot) * sizeof(double),
                  ACCSET_STACK);
    info->curSlot++;
}
