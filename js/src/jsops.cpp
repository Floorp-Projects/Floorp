/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/* This file needs to be included in possibly multiple places. */

#if JS_THREADED_INTERP
  interrupt:
#else /* !JS_THREADED_INTERP */
  case -1:
    JS_ASSERT(switchMask == -1);
#endif /* !JS_THREADED_INTERP */
    {
        bool moreInterrupts = false;
        JSTrapHandler handler = cx->debugHooks->interruptHandler;
        if (handler) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                js_AbortRecording(cx, "interrupt handler");
#endif
            switch (handler(cx, script, regs.pc, &rval,
                            cx->debugHooks->interruptHandlerData)) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                fp->rval = rval;
                ok = JS_TRUE;
                goto forced_return;
              case JSTRAP_THROW:
                cx->throwing = JS_TRUE;
                cx->exception = rval;
                goto error;
              default:;
            }
            moreInterrupts = true;
        }

#ifdef JS_TRACER
        if (TraceRecorder* tr = TRACE_RECORDER(cx)) {
            AbortableRecordingStatus status = tr->monitorRecording(op);
            switch (status) {
              case ARECORD_CONTINUE:
                moreInterrupts = true;
                break;
              case ARECORD_IMACRO:
                atoms = COMMON_ATOMS_START(&rt->atomState);
                op = JSOp(*regs.pc);
                DO_OP();    /* keep interrupting for op. */
                break;
              case ARECORD_ERROR:
                // The code at 'error:' aborts the recording.
                goto error;
              case ARECORD_ABORTED:
              case ARECORD_COMPLETED:
                break;
              case ARECORD_STOP:
                /* A 'stop' error should have already aborted recording. */
              default:
                JS_NOT_REACHED("Bad recording status");
            }
        }
#endif /* !JS_TRACER */

#if JS_THREADED_INTERP
#ifdef MOZ_TRACEVIS
        if (!moreInterrupts)
            js_ExitTraceVisState(cx, R_ABORT);
#endif
        jumpTable = moreInterrupts ? interruptJumpTable : normalJumpTable;
        JS_EXTENSION_(goto *normalJumpTable[op]);
#else
        switchMask = moreInterrupts ? -1 : 0;
        switchOp = intN(op);
        goto do_switch;
#endif
    }

/* No-ops for ease of decompilation. */
ADD_EMPTY_CASE(JSOP_NOP)
ADD_EMPTY_CASE(JSOP_CONDSWITCH)
ADD_EMPTY_CASE(JSOP_TRY)
ADD_EMPTY_CASE(JSOP_TRACE)
#if JS_HAS_XML_SUPPORT
ADD_EMPTY_CASE(JSOP_STARTXML)
ADD_EMPTY_CASE(JSOP_STARTXMLEXPR)
#endif
END_EMPTY_CASES

/* ADD_EMPTY_CASE is not used here as JSOP_LINENO_LENGTH == 3. */
BEGIN_CASE(JSOP_LINENO)
END_CASE(JSOP_LINENO)

BEGIN_CASE(JSOP_PUSH)
    PUSH_OPND(JSVAL_VOID);
END_CASE(JSOP_PUSH)

BEGIN_CASE(JSOP_POP)
    regs.sp--;
END_CASE(JSOP_POP)

BEGIN_CASE(JSOP_POPN)
    regs.sp -= GET_UINT16(regs.pc);
#ifdef DEBUG
    JS_ASSERT(StackBase(fp) <= regs.sp);
    obj = fp->blockChain;
    JS_ASSERT_IF(obj,
                 OBJ_BLOCK_DEPTH(cx, obj) + OBJ_BLOCK_COUNT(cx, obj)
                 <= (size_t) (regs.sp - StackBase(fp)));
    for (obj = fp->scopeChain; obj; obj = OBJ_GET_PARENT(cx, obj)) {
        clasp = OBJ_GET_CLASS(cx, obj);
        if (clasp != &js_BlockClass && clasp != &js_WithClass)
            continue;
        if (obj->getPrivate() != fp)
            break;
        JS_ASSERT(StackBase(fp) + OBJ_BLOCK_DEPTH(cx, obj)
                             + ((clasp == &js_BlockClass)
                                ? OBJ_BLOCK_COUNT(cx, obj)
                                : 1)
                  <= regs.sp);
    }
#endif
END_CASE(JSOP_POPN)

BEGIN_CASE(JSOP_SETRVAL)
BEGIN_CASE(JSOP_POPV)
    ASSERT_NOT_THROWING(cx);
    fp->rval = POP_OPND();
END_CASE(JSOP_POPV)

BEGIN_CASE(JSOP_ENTERWITH)
    if (!js_EnterWith(cx, -1))
        goto error;

    /*
     * We must ensure that different "with" blocks have different stack depth
     * associated with them. This allows the try handler search to properly
     * recover the scope chain. Thus we must keep the stack at least at the
     * current level.
     *
     * We set sp[-1] to the current "with" object to help asserting the
     * enter/leave balance in [leavewith].
     */
    regs.sp[-1] = OBJECT_TO_JSVAL(fp->scopeChain);
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(regs.sp[-1] == OBJECT_TO_JSVAL(fp->scopeChain));
    regs.sp--;
    js_LeaveWith(cx);
END_CASE(JSOP_LEAVEWITH)

BEGIN_CASE(JSOP_RETURN)
    fp->rval = POP_OPND();
    /* FALL THROUGH */

BEGIN_CASE(JSOP_RETRVAL)    /* fp->rval already set */
BEGIN_CASE(JSOP_STOP)
    /*
     * When the inlined frame exits with an exception or an error, ok will be
     * false after the inline_return label.
     */
    ASSERT_NOT_THROWING(cx);
    CHECK_BRANCH();

    if (fp->imacpc) {
        /*
         * If we are at the end of an imacro, return to its caller in the
         * current frame.
         */
        JS_ASSERT(op == JSOP_STOP);

      end_imacro:
        JS_ASSERT((uintN)(regs.sp - fp->slots) <= script->nslots);
        regs.pc = fp->imacpc + js_CodeSpec[*fp->imacpc].length;
        fp->imacpc = NULL;
        atoms = script->atomMap.vector;
        op = JSOp(*regs.pc);
        DO_OP();
    }

    JS_ASSERT(regs.sp == StackBase(fp));
    if ((fp->flags & JSFRAME_CONSTRUCTING) &&
        JSVAL_IS_PRIMITIVE(fp->rval)) {
        if (!fp->fun) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_NEW_RESULT,
                                 js_ValueToPrintableString(cx, rval));
            goto error;
        }
        fp->rval = fp->thisv;
    }
    ok = JS_TRUE;
    if (inlineCallCount)
  inline_return:
    {
        JSInlineFrame *ifp = (JSInlineFrame *) fp;
        void *hookData = ifp->hookData;

        JS_ASSERT(!fp->blockChain);
        JS_ASSERT(!js_IsActiveWithOrBlock(cx, fp->scopeChain, 0));

        if (script->staticLevel < JS_DISPLAY_SIZE)
            cx->display[script->staticLevel] = fp->displaySave;

        if (hookData) {
            JSInterpreterHook hook;
            JSBool status;

            hook = cx->debugHooks->callHook;
            if (hook) {
                /*
                 * Do not pass &ok directly as exposing the address inhibits
                 * optimizations and uninitialised warnings.
                 */
                status = ok;
                hook(cx, fp, JS_FALSE, &status, hookData);
                ok = status;
                CHECK_INTERRUPT_HANDLER();
            }
        }

        /*
         * If fp has a call object, sync values and clear the back-
         * pointer. This can happen for a lightweight function if it calls eval
         * unexpectedly (in a way that is hidden from the compiler). See bug
         * 325540.
         */
        fp->putActivationObjects(cx);

#ifdef INCLUDE_MOZILLA_DTRACE
        /* DTrace function return, inlines */
        if (JAVASCRIPT_FUNCTION_RVAL_ENABLED())
            jsdtrace_function_rval(cx, fp, fp->fun, &fp->rval);
        if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
            jsdtrace_function_return(cx, fp, fp->fun);
#endif

        /* Restore context version only if callee hasn't set version. */
        if (JS_LIKELY(cx->version == currentVersion)) {
            currentVersion = ifp->callerVersion;
            if (currentVersion != cx->version)
                js_SetVersion(cx, currentVersion);
        }

        /*
         * If inline-constructing, replace primitive rval with the new object
         * passed in via |this|, and instrument this constructor invocation.
         */
        if (fp->flags & JSFRAME_CONSTRUCTING) {
            if (JSVAL_IS_PRIMITIVE(fp->rval))
                fp->rval = fp->thisv;
            JS_RUNTIME_METER(cx->runtime, constructs);
        }

        /* Restore caller's registers. */
        regs = ifp->callerRegs;

        /* Store the return value in the caller's operand frame. */
        regs.sp -= 1 + (size_t) ifp->frame.argc;
        regs.sp[-1] = fp->rval;

        bool recursive = fp->script == fp->down->script;

        /* Restore cx->fp and release the inline frame's space. */
        cx->fp = fp = fp->down;
        JS_ASSERT(fp->regs == &ifp->callerRegs);
        fp->regs = &regs;
        JS_ARENA_RELEASE(&cx->stackPool, ifp->mark);

        /* Restore the calling script's interpreter registers. */
        script = fp->script;
        atoms = FrameAtomBase(cx, fp);

        /* Resume execution in the calling frame. */
        inlineCallCount--;
        if (JS_LIKELY(ok)) {
            JS_ASSERT(js_CodeSpec[js_GetOpcode(cx, script, regs.pc)].length
                      == JSOP_CALL_LENGTH);
            TRACE_0(LeaveFrame);
            if (!TRACE_RECORDER(cx) && recursive) {
                if (*(regs.pc + JSOP_CALL_LENGTH) == JSOP_TRACE) {
                    regs.pc += JSOP_CALL_LENGTH;
                    MONITOR_BRANCH(Record_LeaveFrame);
                    op = (JSOp)*regs.pc;
                    DO_OP();
                }
            }
            if (*(regs.pc + JSOP_CALL_LENGTH) == JSOP_TRACE ||
                *(regs.pc + JSOP_CALL_LENGTH) == JSOP_NOP) {
                JS_STATIC_ASSERT(JSOP_TRACE_LENGTH == JSOP_NOP_LENGTH);
                regs.pc += JSOP_CALL_LENGTH;
                len = JSOP_TRACE_LENGTH;
            } else {
                len = JSOP_CALL_LENGTH;
            }
            DO_NEXT_OP(len);
        }
        goto error;
    }
    goto exit;

BEGIN_CASE(JSOP_DEFAULT)
    (void) POP();
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTO)
    len = GET_JUMP_OFFSET(regs.pc);
    BRANCH(len);
END_CASE(JSOP_GOTO)

BEGIN_CASE(JSOP_IFEQ)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFEQ)

BEGIN_CASE(JSOP_IFNE)
    POP_BOOLEAN(cx, rval, cond);
    if (cond != JS_FALSE) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFNE)

BEGIN_CASE(JSOP_OR)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_TRUE) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_OPND(rval);
        DO_NEXT_OP(len);
    }
END_CASE(JSOP_OR)

BEGIN_CASE(JSOP_AND)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_OPND(rval);
        DO_NEXT_OP(len);
    }
END_CASE(JSOP_AND)

BEGIN_CASE(JSOP_DEFAULTX)
    (void) POP();
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTOX)
    len = GET_JUMPX_OFFSET(regs.pc);
    BRANCH(len);
END_CASE(JSOP_GOTOX);

BEGIN_CASE(JSOP_IFEQX)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFEQX)

BEGIN_CASE(JSOP_IFNEX)
    POP_BOOLEAN(cx, rval, cond);
    if (cond != JS_FALSE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFNEX)

BEGIN_CASE(JSOP_ORX)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_TRUE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_OPND(rval);
        DO_NEXT_OP(len);
    }
END_CASE(JSOP_ORX)

BEGIN_CASE(JSOP_ANDX)
    POP_BOOLEAN(cx, rval, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_OPND(rval);
        DO_NEXT_OP(len);
    }
END_CASE(JSOP_ANDX)

/*
 * If the index value at sp[n] is not an int that fits in a jsval, it could
 * be an object (an XML QName, AttributeName, or AnyName), but only if we are
 * compiling with JS_HAS_XML_SUPPORT.  Otherwise convert the index value to a
 * string atom id.
 */
#define FETCH_ELEMENT_ID(obj, n, id)                                          \
    JS_BEGIN_MACRO                                                            \
        jsval idval_ = FETCH_OPND(n);                                         \
        if (JSVAL_IS_INT(idval_)) {                                           \
            id = INT_JSVAL_TO_JSID(idval_);                                   \
        } else {                                                              \
            if (!js_InternNonIntElementId(cx, obj, idval_, &id))              \
                goto error;                                                   \
            regs.sp[n] = ID_TO_VALUE(id);                                     \
        }                                                                     \
    JS_END_MACRO

#define TRY_BRANCH_AFTER_COND(cond,spdec)                                     \
    JS_BEGIN_MACRO                                                            \
        uintN diff_;                                                          \
        JS_ASSERT(js_CodeSpec[op].length == 1);                               \
        diff_ = (uintN) regs.pc[1] - (uintN) JSOP_IFEQ;                       \
        if (diff_ <= 1) {                                                     \
            regs.sp -= spdec;                                                 \
            if (cond == (diff_ != 0)) {                                       \
                ++regs.pc;                                                    \
                len = GET_JUMP_OFFSET(regs.pc);                               \
                BRANCH(len);                                                  \
            }                                                                 \
            len = 1 + JSOP_IFEQ_LENGTH;                                       \
            DO_NEXT_OP(len);                                                  \
        }                                                                     \
    JS_END_MACRO

BEGIN_CASE(JSOP_IN)
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_PRIMITIVE(rval)) {
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, rval, NULL);
        goto error;
    }
    obj = JSVAL_TO_OBJECT(rval);
    FETCH_ELEMENT_ID(obj, -2, id);
    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        goto error;
    cond = prop != NULL;
    if (prop)
        obj2->dropProperty(cx, prop);
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    STORE_OPND(-1, BOOLEAN_TO_JSVAL(cond));
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
    JS_ASSERT(regs.sp > StackBase(fp));
    flags = regs.pc[1];
    if (!js_ValueToIterator(cx, flags, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(regs.sp[-1]));
    PUSH(JSVAL_VOID);
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_NEXTITER)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(regs.sp[-2]));
    if (!js_CallIteratorNext(cx, JSVAL_TO_OBJECT(regs.sp[-2]), &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    rval = BOOLEAN_TO_JSVAL(regs.sp[-1] != JSVAL_HOLE);
    PUSH(rval);
END_CASE(JSOP_NEXTITER)

BEGIN_CASE(JSOP_ENDITER)
    /*
     * Decrease the stack pointer even when !ok -- see comments in the
     * exception capturing code for details.
     */
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    ok = js_CloseIterator(cx, regs.sp[-2]);
    regs.sp -= 2;
    if (!ok)
        goto error;
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_FORARG)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    fp->argv[slot] = regs.sp[-1];
END_CASE(JSOP_FORARG)

BEGIN_CASE(JSOP_FORLOCAL)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    fp->slots[slot] = regs.sp[-1];
END_CASE(JSOP_FORLOCAL)

BEGIN_CASE(JSOP_FORNAME)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    LOAD_ATOM(0);
    id = ATOM_TO_JSID(atom);
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;
    if (prop)
        obj2->dropProperty(cx, prop);
    ok = obj->setProperty(cx, id, &regs.sp[-1]);
    if (!ok)
        goto error;
END_CASE(JSOP_FORNAME)

BEGIN_CASE(JSOP_FORPROP)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    LOAD_ATOM(0);
    id = ATOM_TO_JSID(atom);
    FETCH_OBJECT(cx, -1, lval, obj);
    ok = obj->setProperty(cx, id, &regs.sp[-2]);
    if (!ok)
        goto error;
    regs.sp--;
END_CASE(JSOP_FORPROP)

BEGIN_CASE(JSOP_FORELEM)
    /*
     * JSOP_FORELEM simply dups the property identifier at top of stack and
     * lets the subsequent JSOP_ENUMELEM opcode sequence handle the left-hand
     * side expression evaluation and assignment. This opcode exists solely to
     * help the decompiler.
     */
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    rval = FETCH_OPND(-1);
    PUSH(rval);
END_CASE(JSOP_FORELEM)

BEGIN_CASE(JSOP_DUP)
    JS_ASSERT(regs.sp > StackBase(fp));
    rval = FETCH_OPND(-1);
    PUSH(rval);
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    lval = FETCH_OPND(-2);
    rval = FETCH_OPND(-1);
    PUSH(lval);
    PUSH(rval);
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
    JS_ASSERT(regs.sp - 2 >= StackBase(fp));
    lval = FETCH_OPND(-2);
    rval = FETCH_OPND(-1);
    STORE_OPND(-1, lval);
    STORE_OPND(-2, rval);
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
    i = regs.pc[1];
    JS_ASSERT(regs.sp - (i+1) >= StackBase(fp));
    lval = regs.sp[-(i+1)];
    memmove(regs.sp - (i+1), regs.sp - i, sizeof(jsval)*i);
    regs.sp[-1] = lval;
END_CASE(JSOP_PICK)

#define PROPERTY_OP(n, call)                                                  \
    JS_BEGIN_MACRO                                                            \
        /* Fetch the left part and resolve it to a non-null object. */        \
        FETCH_OBJECT(cx, n, lval, obj);                                       \
                                                                              \
        /* Get or set the property. */                                        \
        if (!call)                                                            \
            goto error;                                                       \
    JS_END_MACRO

#define ELEMENT_OP(n, call)                                                   \
    JS_BEGIN_MACRO                                                            \
        /* Fetch the left part and resolve it to a non-null object. */        \
        FETCH_OBJECT(cx, n - 1, lval, obj);                                   \
                                                                              \
        /* Fetch index and convert it to id suitable for use with obj. */     \
        FETCH_ELEMENT_ID(obj, n, id);                                         \
                                                                              \
        /* Get or set the element. */                                         \
        if (!call)                                                            \
            goto error;                                                       \
    JS_END_MACRO

#define NATIVE_GET(cx,obj,pobj,sprop,getHow,vp)                               \
    JS_BEGIN_MACRO                                                            \
        if (SPROP_HAS_STUB_GETTER(sprop)) {                                   \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((sprop)->slot != SPROP_INVALID_SLOT ||                  \
                      !SPROP_HAS_STUB_SETTER(sprop));                         \
            *vp = ((sprop)->slot != SPROP_INVALID_SLOT)                       \
                  ? LOCKED_OBJ_GET_SLOT(pobj, (sprop)->slot)                  \
                  : JSVAL_VOID;                                               \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, sprop, getHow, vp))              \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_SET(cx,obj,sprop,entry,vp)                                     \
    JS_BEGIN_MACRO                                                            \
        TRACE_2(SetPropHit, entry, sprop);                                    \
        if (SPROP_HAS_STUB_SETTER(sprop) &&                                   \
            (sprop)->slot != SPROP_INVALID_SLOT &&                            \
            !OBJ_SCOPE(obj)->brandedOrHasMethodBarrier()) {                   \
            /* Fast path for, e.g., plain Object instance properties. */      \
            LOCKED_OBJ_SET_SLOT(obj, (sprop)->slot, *vp);                     \
        } else {                                                              \
            if (!js_NativeSet(cx, obj, sprop, false, vp))                     \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

/*
 * Skip the JSOP_POP typically found after a JSOP_SET* opcode, where oplen is
 * the constant length of the SET opcode sequence, and spdec is the constant
 * by which to decrease the stack pointer to pop all of the SET op's operands.
 *
 * NB: unlike macros that could conceivably be replaced by functions (ignoring
 * goto error), where a call should not have to be braced in order to expand
 * correctly (e.g., in if (cond) FOO(); else BAR()), these three macros lack
 * JS_{BEGIN,END}_MACRO brackets. They are also indented so as to align with
 * nearby opcode code.
 */
#define SKIP_POP_AFTER_SET(oplen,spdec)                                       \
            if (regs.pc[oplen] == JSOP_POP) {                                 \
                regs.sp -= spdec;                                             \
                regs.pc += oplen + JSOP_POP_LENGTH;                           \
                op = (JSOp) *regs.pc;                                         \
                DO_OP();                                                      \
            }

#define END_SET_CASE(OP)                                                      \
            SKIP_POP_AFTER_SET(OP##_LENGTH, 1);                               \
          END_CASE(OP)

#define END_SET_CASE_STORE_RVAL(OP,spdec)                                     \
            SKIP_POP_AFTER_SET(OP##_LENGTH, spdec);                           \
            rval = FETCH_OPND(-1);                                            \
            regs.sp -= (spdec) - 1;                                           \
            STORE_OPND(-1, rval);                                             \
          END_CASE(OP)

BEGIN_CASE(JSOP_SETCONST)
    LOAD_ATOM(0);
    obj = fp->varobj;
    rval = FETCH_OPND(-1);
    if (!obj->defineProperty(cx, ATOM_TO_JSID(atom), rval,
                             JS_PropertyStub, JS_PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
END_SET_CASE(JSOP_SETCONST);

#if JS_HAS_DESTRUCTURING
BEGIN_CASE(JSOP_ENUMCONSTELEM)
    rval = FETCH_OPND(-3);
    FETCH_OBJECT(cx, -2, lval, obj);
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->defineProperty(cx, id, rval,
                             JS_PropertyStub, JS_PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDNAME)
    do {
        JSPropCacheEntry *entry;

        /*
         * We can skip the property lookup for the global object. If the
         * property does not exist anywhere on the scope chain, JSOP_SETNAME
         * adds the property to the global.
         *
         * As a consequence of this optimization for the global object we run
         * its JSRESOLVE_ASSIGNING-tolerant resolve hooks only in JSOP_SETNAME,
         * after the interpreter evaluates the right- hand-side of the
         * assignment, and not here.
         *
         * This should be transparent to the hooks because the script, instead
         * of name = rhs, could have used global.name = rhs given a global
         * object reference, which also calls the hooks only after evaluating
         * the rhs. We desire such resolve hook equivalence between the two
         * forms.
         */
        obj = fp->scopeChain;
        if (!OBJ_GET_PARENT(cx, obj))
            break;
        if (JS_LIKELY(OBJ_IS_NATIVE(obj))) {
            PROPERTY_CACHE_TEST(cx, regs.pc, obj, obj2, entry, atom);
            if (!atom) {
                ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
                JS_UNLOCK_OBJ(cx, obj2);
                break;
            }
        } else {
            entry = NULL;
            LOAD_ATOM(0);
        }
        id = ATOM_TO_JSID(atom);
        obj = js_FindIdentifierBase(cx, fp->scopeChain, id);
        if (!obj)
            goto error;
    } while (0);
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_BINDNAME)

BEGIN_CASE(JSOP_IMACOP)
    JS_ASSERT(JS_UPTRDIFF(fp->imacpc, script->code) < script->length);
    op = JSOp(*fp->imacpc);
    DO_OP();

#define BITWISE_OP(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        FETCH_INT(cx, -2, i);                                                 \
        FETCH_INT(cx, -1, j);                                                 \
        i = i OP j;                                                           \
        regs.sp--;                                                            \
        STORE_INT(cx, -1, i);                                                 \
    JS_END_MACRO

BEGIN_CASE(JSOP_BITOR)
    BITWISE_OP(|);
END_CASE(JSOP_BITOR)

BEGIN_CASE(JSOP_BITXOR)
    BITWISE_OP(^);
END_CASE(JSOP_BITXOR)

BEGIN_CASE(JSOP_BITAND)
    BITWISE_OP(&);
END_CASE(JSOP_BITAND)

#define RELATIONAL_OP(OP)                                                     \
    JS_BEGIN_MACRO                                                            \
        rval = FETCH_OPND(-1);                                                \
        lval = FETCH_OPND(-2);                                                \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if ((lval & rval) & JSVAL_INT) {                                      \
            cond = JSVAL_TO_INT(lval) OP JSVAL_TO_INT(rval);                  \
        } else {                                                              \
            if (!JSVAL_IS_PRIMITIVE(lval))                                    \
                DEFAULT_VALUE(cx, -2, JSTYPE_NUMBER, lval);                   \
            if (!JSVAL_IS_PRIMITIVE(rval))                                    \
                DEFAULT_VALUE(cx, -1, JSTYPE_NUMBER, rval);                   \
            if (JSVAL_IS_STRING(lval) && JSVAL_IS_STRING(rval)) {             \
                str  = JSVAL_TO_STRING(lval);                                 \
                str2 = JSVAL_TO_STRING(rval);                                 \
                cond = js_CompareStrings(str, str2) OP 0;                     \
            } else {                                                          \
                VALUE_TO_NUMBER(cx, -2, lval, d);                             \
                VALUE_TO_NUMBER(cx, -1, rval, d2);                            \
                cond = JSDOUBLE_COMPARE(d, OP, d2, JS_FALSE);                 \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        STORE_OPND(-1, BOOLEAN_TO_JSVAL(cond));                               \
    JS_END_MACRO

/*
 * NB: These macros can't use JS_BEGIN_MACRO/JS_END_MACRO around their bodies
 * because they begin if/else chains, so callers must not put semicolons after
 * the call expressions!
 */
#if JS_HAS_XML_SUPPORT
#define XML_EQUALITY_OP(OP)                                                   \
    if ((ltmp == JSVAL_OBJECT &&                                              \
         (obj2 = JSVAL_TO_OBJECT(lval)) &&                                    \
         OBJECT_IS_XML(cx, obj2)) ||                                          \
        (rtmp == JSVAL_OBJECT &&                                              \
         (obj2 = JSVAL_TO_OBJECT(rval)) &&                                    \
         OBJECT_IS_XML(cx, obj2))) {                                          \
        if (JSVAL_IS_OBJECT(rval) && obj2 == JSVAL_TO_OBJECT(rval))           \
            rval = lval;                                                      \
        if (!js_TestXMLEquality(cx, obj2, rval, &cond))                       \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else

#define EXTENDED_EQUALITY_OP(OP)                                              \
    if (ltmp == JSVAL_OBJECT &&                                               \
        (obj2 = JSVAL_TO_OBJECT(lval)) &&                                     \
        ((clasp = OBJ_GET_CLASS(cx, obj2))->flags & JSCLASS_IS_EXTENDED)) {   \
        JSExtendedClass *xclasp;                                              \
                                                                              \
        xclasp = (JSExtendedClass *) clasp;                                   \
        if (!xclasp->equality(cx, obj2, rval, &cond))                         \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else
#else
#define XML_EQUALITY_OP(OP)             /* nothing */
#define EXTENDED_EQUALITY_OP(OP)        /* nothing */
#endif

#define EQUALITY_OP(OP, IFNAN)                                                \
    JS_BEGIN_MACRO                                                            \
        rval = FETCH_OPND(-1);                                                \
        lval = FETCH_OPND(-2);                                                \
        ltmp = JSVAL_TAG(lval);                                               \
        rtmp = JSVAL_TAG(rval);                                               \
        XML_EQUALITY_OP(OP)                                                   \
        if (ltmp == rtmp) {                                                   \
            if (ltmp == JSVAL_STRING) {                                       \
                str  = JSVAL_TO_STRING(lval);                                 \
                str2 = JSVAL_TO_STRING(rval);                                 \
                cond = js_EqualStrings(str, str2) OP JS_TRUE;                 \
            } else if (ltmp == JSVAL_DOUBLE) {                                \
                d  = *JSVAL_TO_DOUBLE(lval);                                  \
                d2 = *JSVAL_TO_DOUBLE(rval);                                  \
                cond = JSDOUBLE_COMPARE(d, OP, d2, IFNAN);                    \
            } else {                                                          \
                EXTENDED_EQUALITY_OP(OP)                                      \
                /* Handle all undefined (=>NaN) and int combinations. */      \
                cond = lval OP rval;                                          \
            }                                                                 \
        } else {                                                              \
            if (JSVAL_IS_NULL(lval) || JSVAL_IS_VOID(lval)) {                 \
                cond = (JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval)) OP 1;     \
            } else if (JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval)) {          \
                cond = 1 OP 0;                                                \
            } else {                                                          \
                if (ltmp == JSVAL_OBJECT) {                                   \
                    DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);                 \
                    ltmp = JSVAL_TAG(lval);                                   \
                } else if (rtmp == JSVAL_OBJECT) {                            \
                    DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);                 \
                    rtmp = JSVAL_TAG(rval);                                   \
                }                                                             \
                if (ltmp == JSVAL_STRING && rtmp == JSVAL_STRING) {           \
                    str  = JSVAL_TO_STRING(lval);                             \
                    str2 = JSVAL_TO_STRING(rval);                             \
                    cond = js_EqualStrings(str, str2) OP JS_TRUE;             \
                } else {                                                      \
                    VALUE_TO_NUMBER(cx, -2, lval, d);                         \
                    VALUE_TO_NUMBER(cx, -1, rval, d2);                        \
                    cond = JSDOUBLE_COMPARE(d, OP, d2, IFNAN);                \
                }                                                             \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        STORE_OPND(-1, BOOLEAN_TO_JSVAL(cond));                               \
    JS_END_MACRO

BEGIN_CASE(JSOP_EQ)
    EQUALITY_OP(==, JS_FALSE);
END_CASE(JSOP_EQ)

BEGIN_CASE(JSOP_NE)
    EQUALITY_OP(!=, JS_TRUE);
END_CASE(JSOP_NE)

#define STRICT_EQUALITY_OP(OP)                                                \
    JS_BEGIN_MACRO                                                            \
        rval = FETCH_OPND(-1);                                                \
        lval = FETCH_OPND(-2);                                                \
        cond = js_StrictlyEqual(cx, lval, rval) OP JS_TRUE;                   \
        regs.sp--;                                                            \
        STORE_OPND(-1, BOOLEAN_TO_JSVAL(cond));                               \
    JS_END_MACRO

BEGIN_CASE(JSOP_STRICTEQ)
    STRICT_EQUALITY_OP(==);
END_CASE(JSOP_STRICTEQ)

BEGIN_CASE(JSOP_STRICTNE)
    STRICT_EQUALITY_OP(!=);
END_CASE(JSOP_STRICTNE)

BEGIN_CASE(JSOP_CASE)
    STRICT_EQUALITY_OP(==);
    (void) POP();
    if (cond) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
    PUSH(lval);
END_CASE(JSOP_CASE)

BEGIN_CASE(JSOP_CASEX)
    STRICT_EQUALITY_OP(==);
    (void) POP();
    if (cond) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
    PUSH(lval);
END_CASE(JSOP_CASEX)

BEGIN_CASE(JSOP_LT)
    RELATIONAL_OP(<);
END_CASE(JSOP_LT)

BEGIN_CASE(JSOP_LE)
    RELATIONAL_OP(<=);
END_CASE(JSOP_LE)

BEGIN_CASE(JSOP_GT)
    RELATIONAL_OP(>);
END_CASE(JSOP_GT)

BEGIN_CASE(JSOP_GE)
    RELATIONAL_OP(>=);
END_CASE(JSOP_GE)

#undef EQUALITY_OP
#undef RELATIONAL_OP

#define SIGNED_SHIFT_OP(OP)                                                   \
    JS_BEGIN_MACRO                                                            \
        FETCH_INT(cx, -2, i);                                                 \
        FETCH_INT(cx, -1, j);                                                 \
        i = i OP (j & 31);                                                    \
        regs.sp--;                                                            \
        STORE_INT(cx, -1, i);                                                 \
    JS_END_MACRO

BEGIN_CASE(JSOP_LSH)
    SIGNED_SHIFT_OP(<<);
END_CASE(JSOP_LSH)

BEGIN_CASE(JSOP_RSH)
    SIGNED_SHIFT_OP(>>);
END_CASE(JSOP_RSH)

BEGIN_CASE(JSOP_URSH)
{
    uint32 u;

    FETCH_UINT(cx, -2, u);
    FETCH_INT(cx, -1, j);
    u >>= (j & 31);
    regs.sp--;
    STORE_UINT(cx, -1, u);
}
END_CASE(JSOP_URSH)

#undef BITWISE_OP
#undef SIGNED_SHIFT_OP

BEGIN_CASE(JSOP_ADD)
    rval = FETCH_OPND(-1);
    lval = FETCH_OPND(-2);
#if JS_HAS_XML_SUPPORT
    if (!JSVAL_IS_PRIMITIVE(lval) &&
        (obj2 = JSVAL_TO_OBJECT(lval), OBJECT_IS_XML(cx, obj2)) &&
        VALUE_IS_XML(cx, rval)) {
        if (!js_ConcatenateXML(cx, obj2, rval, &rval))
            goto error;
        regs.sp--;
        STORE_OPND(-1, rval);
    } else
#endif
    {
        if (!JSVAL_IS_PRIMITIVE(lval))
            DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);
        if (!JSVAL_IS_PRIMITIVE(rval))
            DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);
        if ((cond = JSVAL_IS_STRING(lval)) || JSVAL_IS_STRING(rval)) {
            if (cond) {
                str = JSVAL_TO_STRING(lval);
                str2 = js_ValueToString(cx, rval);
                if (!str2)
                    goto error;
                regs.sp[-1] = STRING_TO_JSVAL(str2);
            } else {
                str2 = JSVAL_TO_STRING(rval);
                str = js_ValueToString(cx, lval);
                if (!str)
                    goto error;
                regs.sp[-2] = STRING_TO_JSVAL(str);
            }
            str = js_ConcatStrings(cx, str, str2);
            if (!str)
                goto error;
            regs.sp--;
            STORE_OPND(-1, STRING_TO_JSVAL(str));
        } else {
            VALUE_TO_NUMBER(cx, -2, lval, d);
            VALUE_TO_NUMBER(cx, -1, rval, d2);
            d += d2;
            regs.sp--;
            STORE_NUMBER(cx, -1, d);
        }
    }
END_CASE(JSOP_ADD)

BEGIN_CASE(JSOP_CONCATN)
{
#ifdef JS_TRACER
    JS_ASSERT_IF(fp->imacpc,
                 *fp->imacpc == JSOP_CONCATN && *regs.pc == JSOP_IMACOP);

    /*
     * This instruction can be executed in three contexts. (1) is normal
     * execution. (2) is while recording, during an imacro 'imacop'.  (3) is
     * during a failed recording or when trace execution aborts during a
     * recorded imacro.
     *  1. !imacro              : N args on stack,   pc is regs.pc
     *  2. imacro && recording  : N args on stack,   pc is fp->imacpc
     *  3. imacro && !recording : N+2 args on stack, pc is fp->imacpc
     */
    bool imacro = fp->imacpc != NULL;
    bool recording = TRACE_RECORDER(cx) != NULL;
    if (imacro) {
        argc = GET_ARGC(fp->imacpc);
        if (!recording)
            js_ConcatPostImacroStackCleanup(argc, regs, NULL);
    } else {
#endif  /* JS_TRACER */
        argc = GET_ARGC(regs.pc);
#ifdef JS_TRACER
    }
#endif  /* JS_TRACER */

    JSCharBuffer buf(cx);
    for (vp = regs.sp - argc; vp < regs.sp; vp++) {
        if ((!JSVAL_IS_PRIMITIVE(*vp) &&
             !JSVAL_TO_OBJECT(*vp)->defaultValue(cx, JSTYPE_VOID, vp)) ||
            !js_ValueToCharBuffer(cx, *vp, buf)) {
            goto error;
        }
    }

    str = js_NewStringFromCharBuffer(cx, buf);
    if (!str)
        goto error;

    regs.sp -= argc - 1;
    STORE_OPND(-1, STRING_TO_JSVAL(str));

#ifdef JS_TRACER
    if (imacro) {
        /* END_CASE does pc += CONCATN_LENGTH. (IMACOP YOU IDIOT!) */
        regs.pc -= JSOP_CONCATN_LENGTH - JSOP_IMACOP_LENGTH;
    }
#endif  /* JS_TRACER */
}
END_CASE(JSOP_CONCATN)

#define BINARY_OP(OP)                                                         \
    JS_BEGIN_MACRO                                                            \
        FETCH_NUMBER(cx, -2, d);                                              \
        FETCH_NUMBER(cx, -1, d2);                                             \
        d = d OP d2;                                                          \
        regs.sp--;                                                            \
        STORE_NUMBER(cx, -1, d);                                              \
    JS_END_MACRO

BEGIN_CASE(JSOP_SUB)
    BINARY_OP(-);
END_CASE(JSOP_SUB)

BEGIN_CASE(JSOP_MUL)
    BINARY_OP(*);
END_CASE(JSOP_MUL)

BEGIN_CASE(JSOP_DIV)
    FETCH_NUMBER(cx, -1, d2);
    FETCH_NUMBER(cx, -2, d);
    regs.sp--;
    if (d2 == 0) {
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(d2))
            rval = rt->NaNValue;
        else
#endif
        if (d == 0 || JSDOUBLE_IS_NaN(d))
            rval = rt->NaNValue;
        else if (JSDOUBLE_IS_NEG(d) != JSDOUBLE_IS_NEG(d2))
            rval = rt->negativeInfinityValue;
        else
            rval = rt->positiveInfinityValue;
        STORE_OPND(-1, rval);
    } else {
        d /= d2;
        STORE_NUMBER(cx, -1, d);
    }
END_CASE(JSOP_DIV)

BEGIN_CASE(JSOP_MOD)
    FETCH_NUMBER(cx, -1, d2);
    FETCH_NUMBER(cx, -2, d);
    regs.sp--;
    if (d2 == 0) {
        STORE_OPND(-1, rt->NaNValue);
    } else {
        d = js_fmod(d, d2);
        STORE_NUMBER(cx, -1, d);
    }
END_CASE(JSOP_MOD)

BEGIN_CASE(JSOP_NOT)
    POP_BOOLEAN(cx, rval, cond);
    PUSH_OPND(BOOLEAN_TO_JSVAL(!cond));
END_CASE(JSOP_NOT)

BEGIN_CASE(JSOP_BITNOT)
    FETCH_INT(cx, -1, i);
    i = ~i;
    STORE_INT(cx, -1, i);
END_CASE(JSOP_BITNOT)

BEGIN_CASE(JSOP_NEG)
    /*
     * When the operand is int jsval, INT_FITS_IN_JSVAL(i) implies
     * INT_FITS_IN_JSVAL(-i) unless i is 0 or JSVAL_INT_MIN when the
     * results, -0.0 or JSVAL_INT_MAX + 1, are jsdouble values.
     */
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_INT(rval) &&
        rval != INT_TO_JSVAL(JSVAL_INT_MIN) &&
        (i = JSVAL_TO_INT(rval)) != 0) {
        JS_STATIC_ASSERT(!INT_FITS_IN_JSVAL(-JSVAL_INT_MIN));
        i = -i;
        JS_ASSERT(INT_FITS_IN_JSVAL(i));
        regs.sp[-1] = INT_TO_JSVAL(i);
    } else {
        if (JSVAL_IS_DOUBLE(rval)) {
            d = *JSVAL_TO_DOUBLE(rval);
        } else {
            d = js_ValueToNumber(cx, &regs.sp[-1]);
            if (JSVAL_IS_NULL(regs.sp[-1]))
                goto error;
            JS_ASSERT(JSVAL_IS_NUMBER(regs.sp[-1]) ||
                      regs.sp[-1] == JSVAL_TRUE);
        }
        d = -d;
        if (!js_NewNumberInRootedValue(cx, d, &regs.sp[-1]))
            goto error;
    }
END_CASE(JSOP_NEG)

BEGIN_CASE(JSOP_POS)
    rval = FETCH_OPND(-1);
    if (!JSVAL_IS_NUMBER(rval)) {
        d = js_ValueToNumber(cx, &regs.sp[-1]);
        rval = regs.sp[-1];
        if (JSVAL_IS_NULL(rval))
            goto error;
        if (rval == JSVAL_TRUE) {
            if (!js_NewNumberInRootedValue(cx, d, &regs.sp[-1]))
                goto error;
        } else {
            JS_ASSERT(JSVAL_IS_NUMBER(rval));
        }
    }
END_CASE(JSOP_POS)

BEGIN_CASE(JSOP_DELNAME)
    LOAD_ATOM(0);
    id = ATOM_TO_JSID(atom);
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;

    /* ECMA says to return true if name is undefined or inherited. */
    PUSH_OPND(JSVAL_TRUE);
    if (prop) {
        obj2->dropProperty(cx, prop);
        if (!obj->deleteProperty(cx, id, &regs.sp[-1]))
            goto error;
    }
END_CASE(JSOP_DELNAME)

BEGIN_CASE(JSOP_DELPROP)
    LOAD_ATOM(0);
    id = ATOM_TO_JSID(atom);
    PROPERTY_OP(-1, obj->deleteProperty(cx, id, &rval));
    STORE_OPND(-1, rval);
END_CASE(JSOP_DELPROP)

BEGIN_CASE(JSOP_DELELEM)
    ELEMENT_OP(-1, obj->deleteProperty(cx, id, &rval));
    regs.sp--;
    STORE_OPND(-1, rval);
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
    rval = FETCH_OPND(-1);
    type = JS_TypeOfValue(cx, rval);
    atom = rt->atomState.typeAtoms[type];
    STORE_OPND(-1, ATOM_KEY(atom));
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    STORE_OPND(-1, JSVAL_VOID);
END_CASE(JSOP_VOID)

BEGIN_CASE(JSOP_INCELEM)
BEGIN_CASE(JSOP_DECELEM)
BEGIN_CASE(JSOP_ELEMINC)
BEGIN_CASE(JSOP_ELEMDEC)
    /*
     * Delay fetching of id until we have the object to ensure the proper
     * evaluation order. See bug 372331.
     */
    id = 0;
    i = -2;
    goto fetch_incop_obj;

BEGIN_CASE(JSOP_INCPROP)
BEGIN_CASE(JSOP_DECPROP)
BEGIN_CASE(JSOP_PROPINC)
BEGIN_CASE(JSOP_PROPDEC)
    LOAD_ATOM(0);
    id = ATOM_TO_JSID(atom);
    i = -1;

  fetch_incop_obj:
    FETCH_OBJECT(cx, i, lval, obj);
    if (id == 0)
        FETCH_ELEMENT_ID(obj, -1, id);
    goto do_incop;

BEGIN_CASE(JSOP_INCNAME)
BEGIN_CASE(JSOP_DECNAME)
BEGIN_CASE(JSOP_NAMEINC)
BEGIN_CASE(JSOP_NAMEDEC)
{
    JSPropCacheEntry *entry;

    obj = fp->scopeChain;
    if (JS_LIKELY(OBJ_IS_NATIVE(obj))) {
        PROPERTY_CACHE_TEST(cx, regs.pc, obj, obj2, entry, atom);
        if (!atom) {
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            if (obj == obj2 && PCVAL_IS_SLOT(entry->vword)) {
                slot = PCVAL_TO_SLOT(entry->vword);
                JS_ASSERT(slot < OBJ_SCOPE(obj)->freeslot);
                rval = LOCKED_OBJ_GET_SLOT(obj, slot);
                if (JS_LIKELY(CAN_DO_FAST_INC_DEC(rval))) {
                    rtmp = rval;
                    rval += (js_CodeSpec[op].format & JOF_INC) ? 2 : -2;
                    if (!(js_CodeSpec[op].format & JOF_POST))
                        rtmp = rval;
                    LOCKED_OBJ_SET_SLOT(obj, slot, rval);
                    JS_UNLOCK_OBJ(cx, obj);
                    PUSH_OPND(rtmp);
                    len = JSOP_INCNAME_LENGTH;
                    DO_NEXT_OP(len);
                }
            }
            JS_UNLOCK_OBJ(cx, obj2);
            LOAD_ATOM(0);
        }
    } else {
        LOAD_ATOM(0);
    }
    id = ATOM_TO_JSID(atom);
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop)
        goto atom_not_defined;
    obj2->dropProperty(cx, prop);
}

do_incop:
{
    const JSCodeSpec *cs;
    jsval v;

    /*
     * We need a root to store the value to leave on the stack until
     * we have done with obj->setProperty.
     */
    PUSH_OPND(JSVAL_NULL);
    if (!obj->getProperty(cx, id, &regs.sp[-1]))
        goto error;

    cs = &js_CodeSpec[op];
    JS_ASSERT(cs->ndefs == 1);
    JS_ASSERT((cs->format & JOF_TMPSLOT_MASK) == JOF_TMPSLOT2);
    v = regs.sp[-1];
    if (JS_LIKELY(CAN_DO_FAST_INC_DEC(v))) {
        jsval incr;

        incr = (cs->format & JOF_INC) ? 2 : -2;
        if (cs->format & JOF_POST) {
            regs.sp[-1] = v + incr;
        } else {
            v += incr;
            regs.sp[-1] = v;
        }
        fp->flags |= JSFRAME_ASSIGNING;
        ok = obj->setProperty(cx, id, &regs.sp[-1]);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;

        /*
         * We must set regs.sp[-1] to v for both post and pre increments
         * as the setter overwrites regs.sp[-1].
         */
        regs.sp[-1] = v;
    } else {
        /* We need an extra root for the result. */
        PUSH_OPND(JSVAL_NULL);
        if (!js_DoIncDec(cx, cs, &regs.sp[-2], &regs.sp[-1]))
            goto error;
        fp->flags |= JSFRAME_ASSIGNING;
        ok = obj->setProperty(cx, id, &regs.sp[-1]);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;
        regs.sp--;
    }

    if (cs->nuses == 0) {
        /* regs.sp[-1] already contains the result of name increment. */
    } else {
        rtmp = regs.sp[-1];
        regs.sp -= cs->nuses;
        regs.sp[-1] = rtmp;
    }
    len = cs->length;
    DO_NEXT_OP(len);
}

{
    jsval incr, incr2;

    /* Position cases so the most frequent i++ does not need a jump. */
BEGIN_CASE(JSOP_DECARG)
    incr = -2; incr2 = -2; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGDEC)
    incr = -2; incr2 =  0; goto do_arg_incop;
BEGIN_CASE(JSOP_INCARG)
    incr =  2; incr2 =  2; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGINC)
    incr =  2; incr2 =  0;

  do_arg_incop:
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    vp = fp->argv + slot;
    goto do_int_fast_incop;

BEGIN_CASE(JSOP_DECLOCAL)
    incr = -2; incr2 = -2; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALDEC)
    incr = -2; incr2 =  0; goto do_local_incop;
BEGIN_CASE(JSOP_INCLOCAL)
    incr =  2; incr2 =  2; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALINC)
    incr =  2; incr2 =  0;

  /*
   * do_local_incop comes right before do_int_fast_incop as we want to
   * avoid an extra jump for variable cases as local++ is more frequent
   * than arg++.
   */
  do_local_incop:
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    vp = fp->slots + slot;
    METER_SLOT_OP(op, slot);
    vp = fp->slots + slot;

  do_int_fast_incop:
    rval = *vp;
    if (JS_LIKELY(CAN_DO_FAST_INC_DEC(rval))) {
        *vp = rval + incr;
        JS_ASSERT(JSOP_INCARG_LENGTH == js_CodeSpec[op].length);
        SKIP_POP_AFTER_SET(JSOP_INCARG_LENGTH, 0);
        PUSH_OPND(rval + incr2);
    } else {
        PUSH_OPND(rval);
        if (!js_DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-1], vp))
            goto error;
    }
    len = JSOP_INCARG_LENGTH;
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

/* NB: This macro doesn't use JS_BEGIN_MACRO/JS_END_MACRO around its body. */
#define FAST_GLOBAL_INCREMENT_OP(SLOWOP,INCR,INCR2)                           \
    op2 = SLOWOP;                                                             \
    incr = INCR;                                                              \
    incr2 = INCR2;                                                            \
    goto do_global_incop

{
    jsval incr, incr2;

BEGIN_CASE(JSOP_DECGVAR)
    FAST_GLOBAL_INCREMENT_OP(JSOP_DECNAME, -2, -2);
BEGIN_CASE(JSOP_GVARDEC)
    FAST_GLOBAL_INCREMENT_OP(JSOP_NAMEDEC, -2,  0);
BEGIN_CASE(JSOP_INCGVAR)
    FAST_GLOBAL_INCREMENT_OP(JSOP_INCNAME,  2,  2);
BEGIN_CASE(JSOP_GVARINC)
    FAST_GLOBAL_INCREMENT_OP(JSOP_NAMEINC,  2,  0);

#undef FAST_GLOBAL_INCREMENT_OP

  do_global_incop:
    JS_ASSERT((js_CodeSpec[op].format & JOF_TMPSLOT_MASK) ==
              JOF_TMPSLOT2);
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    lval = fp->slots[slot];
    if (JSVAL_IS_NULL(lval)) {
        op = op2;
        DO_OP();
    }
    slot = JSVAL_TO_INT(lval);
    rval = OBJ_GET_SLOT(cx, fp->varobj, slot);
    if (JS_LIKELY(CAN_DO_FAST_INC_DEC(rval))) {
        PUSH_OPND(rval + incr2);
        rval += incr;
    } else {
        PUSH_OPND(rval);
        PUSH_OPND(JSVAL_NULL);  /* Extra root */
        if (!js_DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-2], &regs.sp[-1]))
            goto error;
        rval = regs.sp[-1];
        --regs.sp;
    }
    OBJ_SET_SLOT(cx, fp->varobj, slot, rval);
    len = JSOP_INCGVAR_LENGTH;  /* all gvar incops are same length */
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

#define COMPUTE_THIS(cx, fp, obj)                                             \
    JS_BEGIN_MACRO                                                            \
        if (!(obj = js_ComputeThisForFrame(cx, fp)))                          \
            goto error;                                                       \
    JS_END_MACRO

BEGIN_CASE(JSOP_THIS)
    COMPUTE_THIS(cx, fp, obj);
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_THIS)

BEGIN_CASE(JSOP_GETTHISPROP)
    i = 0;
    COMPUTE_THIS(cx, fp, obj);
    PUSH(JSVAL_NULL);
    goto do_getprop_with_obj;

#undef COMPUTE_THIS

BEGIN_CASE(JSOP_GETARGPROP)
    i = ARGNO_LEN;
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    PUSH_OPND(fp->argv[slot]);
    goto do_getprop_body;

BEGIN_CASE(JSOP_GETLOCALPROP)
    i = SLOTNO_LEN;
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_OPND(fp->slots[slot]);
    goto do_getprop_body;

BEGIN_CASE(JSOP_GETPROP)
BEGIN_CASE(JSOP_GETXPROP)
    i = 0;

  do_getprop_body:
    lval = FETCH_OPND(-1);

  do_getprop_with_lval:
    VALUE_TO_OBJECT(cx, -1, lval, obj);

  do_getprop_with_obj:
    do {
        JSObject *aobj;
        JSPropCacheEntry *entry;

        /*
         * We do not impose the method read barrier if in an imacro,
         * assuming any property gets it does (e.g., for 'toString'
         * from JSOP_NEW) will not be leaked to the calling script.
         */
        aobj = js_GetProtoIfDenseArray(cx, obj);
        if (JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)) {
            PROPERTY_CACHE_TEST(cx, regs.pc, aobj, obj2, entry, atom);
            if (!atom) {
                ASSERT_VALID_PROPERTY_CACHE_HIT(i, aobj, obj2, entry);
                if (PCVAL_IS_OBJECT(entry->vword)) {
                    rval = PCVAL_OBJECT_TO_JSVAL(entry->vword);
                } else if (PCVAL_IS_SLOT(entry->vword)) {
                    slot = PCVAL_TO_SLOT(entry->vword);
                    JS_ASSERT(slot < OBJ_SCOPE(obj2)->freeslot);
                    rval = LOCKED_OBJ_GET_SLOT(obj2, slot);
                } else {
                    JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
                    sprop = PCVAL_TO_SPROP(entry->vword);
                    NATIVE_GET(cx, obj, obj2, sprop,
                               fp->imacpc ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                               &rval);
                }
                JS_UNLOCK_OBJ(cx, obj2);
                break;
            }
        } else {
            entry = NULL;
            if (i < 0)
                atom = rt->atomState.lengthAtom;
            else
                LOAD_ATOM(i);
        }
        id = ATOM_TO_JSID(atom);
        if (entry
            ? !js_GetPropertyHelper(cx, obj, id,
                                    fp->imacpc
                                    ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                                    : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER,
                                    &rval)
            : !obj->getProperty(cx, id, &rval)) {
            goto error;
        }
    } while (0);

    STORE_OPND(-1, rval);
    JS_ASSERT(JSOP_GETPROP_LENGTH + i == js_CodeSpec[op].length);
    len = JSOP_GETPROP_LENGTH + i;
END_VARLEN_CASE

BEGIN_CASE(JSOP_LENGTH)
    lval = FETCH_OPND(-1);
    if (JSVAL_IS_STRING(lval)) {
        str = JSVAL_TO_STRING(lval);
        regs.sp[-1] = INT_TO_JSVAL(str->length());
    } else if (!JSVAL_IS_PRIMITIVE(lval) &&
               (obj = JSVAL_TO_OBJECT(lval), OBJ_IS_ARRAY(cx, obj))) {
        jsuint length;

        /*
         * We know that the array is created with only its 'length' private
         * data in a fixed slot at JSSLOT_ARRAY_LENGTH. See also
         * JSOP_ARRAYPUSH, far below.
         */
        length = obj->fslots[JSSLOT_ARRAY_LENGTH];
        if (length <= JSVAL_INT_MAX) {
            regs.sp[-1] = INT_TO_JSVAL(length);
        } else if (!js_NewDoubleInRootedValue(cx, (jsdouble) length,
                                              &regs.sp[-1])) {
            goto error;
        }
    } else {
        i = -2;
        goto do_getprop_with_lval;
    }
END_CASE(JSOP_LENGTH)

BEGIN_CASE(JSOP_CALLPROP)
{
    JSObject *aobj;
    JSPropCacheEntry *entry;

    lval = FETCH_OPND(-1);
    if (!JSVAL_IS_PRIMITIVE(lval)) {
        obj = JSVAL_TO_OBJECT(lval);
    } else {
        if (JSVAL_IS_STRING(lval)) {
            i = JSProto_String;
        } else if (JSVAL_IS_NUMBER(lval)) {
            i = JSProto_Number;
        } else if (JSVAL_IS_BOOLEAN(lval)) {
            i = JSProto_Boolean;
        } else {
            JS_ASSERT(JSVAL_IS_NULL(lval) || JSVAL_IS_VOID(lval));
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            goto error;
        }

        if (!js_GetClassPrototype(cx, NULL, INT_TO_JSID(i), &obj))
            goto error;
    }

    aobj = js_GetProtoIfDenseArray(cx, obj);
    if (JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)) {
        PROPERTY_CACHE_TEST(cx, regs.pc, aobj, obj2, entry, atom);
        if (!atom) {
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, aobj, obj2, entry);
            if (PCVAL_IS_OBJECT(entry->vword)) {
                rval = PCVAL_OBJECT_TO_JSVAL(entry->vword);
            } else if (PCVAL_IS_SLOT(entry->vword)) {
                slot = PCVAL_TO_SLOT(entry->vword);
                JS_ASSERT(slot < OBJ_SCOPE(obj2)->freeslot);
                rval = LOCKED_OBJ_GET_SLOT(obj2, slot);
            } else {
                JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
                sprop = PCVAL_TO_SPROP(entry->vword);
                NATIVE_GET(cx, obj, obj2, sprop, JSGET_NO_METHOD_BARRIER, &rval);
            }
            JS_UNLOCK_OBJ(cx, obj2);
            STORE_OPND(-1, rval);
            PUSH_OPND(lval);
            goto end_callprop;
        }
    } else {
        entry = NULL;
        LOAD_ATOM(0);
    }

    /*
     * Cache miss: use the immediate atom that was loaded for us under
     * PROPERTY_CACHE_TEST.
     */
    id = ATOM_TO_JSID(atom);
    PUSH(JSVAL_NULL);
    if (!JSVAL_IS_PRIMITIVE(lval)) {
        if (!js_GetMethod(cx, obj, id,
                          entry
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            goto error;
        }
        STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
        STORE_OPND(-2, rval);
    } else {
        JS_ASSERT(obj->map->ops->getProperty == js_GetProperty);
        if (!js_GetPropertyHelper(cx, obj, id,
                                  JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                  &rval)) {
            goto error;
        }
        STORE_OPND(-1, lval);
        STORE_OPND(-2, rval);
    }

  end_callprop:
    /* Wrap primitive lval in object clothing if necessary. */
    if (JSVAL_IS_PRIMITIVE(lval)) {
        /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
        if (!VALUE_IS_FUNCTION(cx, rval) ||
            (obj = JSVAL_TO_OBJECT(rval),
             fun = GET_FUNCTION_PRIVATE(cx, obj),
             !PRIMITIVE_THIS_TEST(fun, lval))) {
            if (!js_PrimitiveToObject(cx, &regs.sp[-1]))
                goto error;
        }
    }
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(JSVAL_IS_VOID(rval))) {
        LOAD_ATOM(0);
        regs.sp[-2] = ATOM_KEY(atom);
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    }
#endif
}
END_CASE(JSOP_CALLPROP)

BEGIN_CASE(JSOP_SETNAME)
BEGIN_CASE(JSOP_SETPROP)
BEGIN_CASE(JSOP_SETMETHOD)
    rval = FETCH_OPND(-1);
    JS_ASSERT_IF(op == JSOP_SETMETHOD, VALUE_IS_FUNCTION(cx, rval));
    lval = FETCH_OPND(-2);
    JS_ASSERT_IF(op == JSOP_SETNAME, !JSVAL_IS_PRIMITIVE(lval));
    VALUE_TO_OBJECT(cx, -2, lval, obj);

    do {
        JSPropCacheEntry *entry;

        entry = NULL;
        atom = NULL;
        if (JS_LIKELY(obj->map->ops->setProperty == js_SetProperty)) {
            JSPropertyCache *cache = &JS_PROPERTY_CACHE(cx);
            uint32 kshape = OBJ_SHAPE(obj);

            /*
             * Open-code PROPERTY_CACHE_TEST, specializing for two important
             * set-property cases. First:
             *
             *   function f(a, b, c) {
             *     var o = {p:a, q:b, r:c};
             *     return o;
             *   }
             *
             * or similar real-world cases, which evolve a newborn native
             * object predicatably through some bounded number of property
             * additions. And second:
             *
             *   o.p = x;
             *
             * in a frequently executed method or loop body, where p will
             * (possibly after the first iteration) always exist in native
             * object o.
             */
            entry = &cache->table[PROPERTY_CACHE_HASH_PC(regs.pc, kshape)];
            PCMETER(cache->pctestentry = entry);
            PCMETER(cache->tests++);
            PCMETER(cache->settests++);
            if (entry->kpc == regs.pc && entry->kshape == kshape) {
                JS_ASSERT(PCVCAP_TAG(entry->vcap) <= 1);
                if (JS_LOCK_OBJ_IF_SHAPE(cx, obj, kshape)) {
                    JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
                    sprop = PCVAL_TO_SPROP(entry->vword);
                    JS_ASSERT(!(sprop->attrs & JSPROP_READONLY));
                    JS_ASSERT_IF(!(sprop->attrs & JSPROP_SHARED),
                                 PCVCAP_TAG(entry->vcap) == 0);

                    JSScope *scope = OBJ_SCOPE(obj);
                    JS_ASSERT(!scope->sealed());

                    /*
                     * Fastest path: check whether the cached sprop is already
                     * in scope and call NATIVE_SET and break to get out of the
                     * do-while(0). But we can call NATIVE_SET only if obj owns
                     * scope or sprop is shared.
                     */
                    bool checkForAdd;
                    if (sprop->attrs & JSPROP_SHARED) {
                        if (PCVCAP_TAG(entry->vcap) == 0 ||
                            ((obj2 = OBJ_GET_PROTO(cx, obj)) &&
                             OBJ_IS_NATIVE(obj2) &&
                             OBJ_SHAPE(obj2) == PCVCAP_SHAPE(entry->vcap))) {
                            goto fast_set_propcache_hit;
                        }

                        /* The cache entry doesn't apply. vshape mismatch. */
                        checkForAdd = false;
                    } else if (scope->owned()) {
                        if (sprop == scope->lastProperty() || scope->hasProperty(sprop)) {
                          fast_set_propcache_hit:
                            PCMETER(cache->pchits++);
                            PCMETER(cache->setpchits++);
                            NATIVE_SET(cx, obj, sprop, entry, &rval);
                            JS_UNLOCK_SCOPE(cx, scope);
                            break;
                        }
                        checkForAdd =
                            !(sprop->attrs & JSPROP_SHARED) &&
                            sprop->parent == scope->lastProperty();
                    } else {
                        scope = js_GetMutableScope(cx, obj);
                        if (!scope) {
                            JS_UNLOCK_OBJ(cx, obj);
                            goto error;
                        }
                        checkForAdd = !sprop->parent;
                    }

                    if (checkForAdd &&
                        SPROP_HAS_STUB_SETTER(sprop) &&
                        (slot = sprop->slot) == scope->freeslot) {
                        /*
                         * Fast path: adding a plain old property that was once
                         * at the frontier of the property tree, whose slot is
                         * next to claim among the allocated slots in obj,
                         * where scope->table has not been created yet.
                         *
                         * We may want to remove hazard conditions above and
                         * inline compensation code here, depending on
                         * real-world workloads.
                         */
                        JS_ASSERT(!(obj->getClass()->flags &
                                    JSCLASS_SHARE_ALL_PROPERTIES));

                        PCMETER(cache->pchits++);
                        PCMETER(cache->addpchits++);

                        /*
                         * Beware classes such as Function that use the
                         * reserveSlots hook to allocate a number of reserved
                         * slots that may vary with obj.
                         */
                        if (slot < STOBJ_NSLOTS(obj) &&
                            !OBJ_GET_CLASS(cx, obj)->reserveSlots) {
                            ++scope->freeslot;
                        } else {
                            if (!js_AllocSlot(cx, obj, &slot)) {
                                JS_UNLOCK_SCOPE(cx, scope);
                                goto error;
                            }
                        }

                        /*
                         * If this obj's number of reserved slots differed, or
                         * if something created a hash table for scope, we must
                         * pay the price of JSScope::putProperty.
                         *
                         * If slot does not match the cached sprop's slot,
                         * update the cache entry in the hope that obj and
                         * other instances with the same number of reserved
                         * slots are now "hot".
                         */
                        if (slot != sprop->slot || scope->table) {
                            JSScopeProperty *sprop2 =
                                scope->putProperty(cx, sprop->id,
                                                   sprop->getter, sprop->setter,
                                                   slot, sprop->attrs,
                                                   sprop->flags, sprop->shortid);
                            if (!sprop2) {
                                js_FreeSlot(cx, obj, slot);
                                JS_UNLOCK_SCOPE(cx, scope);
                                goto error;
                            }
                            if (sprop2 != sprop) {
                                PCMETER(cache->slotchanges++);
                                JS_ASSERT(slot != sprop->slot &&
                                          slot == sprop2->slot &&
                                          sprop2->id == sprop->id);
                                entry->vword = SPROP_TO_PCVAL(sprop2);
                            }
                            sprop = sprop2;
                        } else {
                            scope->extend(cx, sprop);
                        }

                        /*
                         * No method change check here because here we are
                         * adding a new property, not updating an existing
                         * slot's value that might contain a method of a
                         * branded scope.
                         */
                        TRACE_2(SetPropHit, entry, sprop);
                        LOCKED_OBJ_SET_SLOT(obj, slot, rval);
                        JS_UNLOCK_SCOPE(cx, scope);

                        /*
                         * Purge the property cache of the id we may have just
                         * shadowed in obj's scope and proto chains. We do this
                         * after unlocking obj's scope to avoid lock nesting.
                         */
                        js_PurgeScopeChain(cx, obj, sprop->id);
                        break;
                    }
                    JS_UNLOCK_SCOPE(cx, scope);
                    PCMETER(cache->setpcmisses++);
                }
            }

            atom = js_FullTestPropertyCache(cx, regs.pc, &obj, &obj2,
                                            &entry);
            if (atom) {
                PCMETER(cache->misses++);
                PCMETER(cache->setmisses++);
            } else {
                ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
                sprop = NULL;
                if (obj == obj2) {
                    JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
                    sprop = PCVAL_TO_SPROP(entry->vword);
                    JS_ASSERT(!(sprop->attrs & JSPROP_READONLY));
                    JS_ASSERT(!OBJ_SCOPE(obj2)->sealed());
                    NATIVE_SET(cx, obj, sprop, entry, &rval);
                }
                JS_UNLOCK_OBJ(cx, obj2);
                if (sprop)
                    break;
            }
        }

        if (!atom)
            LOAD_ATOM(0);
        id = ATOM_TO_JSID(atom);
        if (entry) {
            uintN defineHow = (op == JSOP_SETMETHOD)
                              ? JSDNP_CACHE_RESULT | JSDNP_SET_METHOD
                              : JSDNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rval))
                goto error;
        } else {
            if (!obj->setProperty(cx, id, &rval))
                goto error;
            ABORT_RECORDING(cx, "Non-native set");
        }
    } while (0);
END_SET_CASE_STORE_RVAL(JSOP_SETPROP, 2);

BEGIN_CASE(JSOP_GETELEM)
    /* Open-coded ELEMENT_OP optimized for strings and dense arrays. */
    lval = FETCH_OPND(-2);
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_STRING(lval) && JSVAL_IS_INT(rval)) {
        str = JSVAL_TO_STRING(lval);
        i = JSVAL_TO_INT(rval);
        if ((size_t)i < str->length()) {
            str = JSString::getUnitString(cx, str, size_t(i));
            if (!str)
                goto error;
            rval = STRING_TO_JSVAL(str);
            goto end_getelem;
        }
    }

    VALUE_TO_OBJECT(cx, -2, lval, obj);
    if (JSVAL_IS_INT(rval)) {
        if (OBJ_IS_DENSE_ARRAY(cx, obj)) {
            jsuint length;

            length = js_DenseArrayCapacity(obj);
            i = JSVAL_TO_INT(rval);
            if ((jsuint)i < length &&
                i < obj->fslots[JSSLOT_ARRAY_LENGTH]) {
                rval = obj->dslots[i];
                if (rval != JSVAL_HOLE)
                    goto end_getelem;

                /* Reload rval from the stack in the rare hole case. */
                rval = FETCH_OPND(-1);
            }
        }
        id = INT_JSVAL_TO_JSID(rval);
    } else {
        if (!js_InternNonIntElementId(cx, obj, rval, &id))
            goto error;
    }

    if (!obj->getProperty(cx, id, &rval))
        goto error;
  end_getelem:
    regs.sp--;
    STORE_OPND(-1, rval);
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_CALLELEM)
    ELEMENT_OP(-1, js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &rval));
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(JSVAL_IS_VOID(rval))) {
        regs.sp[-2] = regs.sp[-1];
        regs.sp[-1] = OBJECT_TO_JSVAL(obj);
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    } else
#endif
    {
        STORE_OPND(-2, rval);
        STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
    }
END_CASE(JSOP_CALLELEM)

BEGIN_CASE(JSOP_SETELEM)
    rval = FETCH_OPND(-1);
    FETCH_OBJECT(cx, -3, lval, obj);
    FETCH_ELEMENT_ID(obj, -2, id);
    do {
        if (OBJ_IS_DENSE_ARRAY(cx, obj) && JSID_IS_INT(id)) {
            jsuint length;

            length = js_DenseArrayCapacity(obj);
            i = JSID_TO_INT(id);
            if ((jsuint)i < length) {
                if (obj->dslots[i] == JSVAL_HOLE) {
                    if (js_PrototypeHasIndexedProperties(cx, obj))
                        break;
                    if (i >= obj->fslots[JSSLOT_ARRAY_LENGTH])
                        obj->fslots[JSSLOT_ARRAY_LENGTH] = i + 1;
                    obj->fslots[JSSLOT_ARRAY_COUNT]++;
                }
                obj->dslots[i] = rval;
                goto end_setelem;
            }
        }
    } while (0);
    if (!obj->setProperty(cx, id, &rval))
        goto error;
  end_setelem:
END_SET_CASE_STORE_RVAL(JSOP_SETELEM, 3)

BEGIN_CASE(JSOP_ENUMELEM)
    /* Funky: the value to set is under the [obj, id] pair. */
    rval = FETCH_OPND(-3);
    FETCH_OBJECT(cx, -2, lval, obj);
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->setProperty(cx, id, &rval))
        goto error;
    regs.sp -= 3;
END_CASE(JSOP_ENUMELEM)

BEGIN_CASE(JSOP_NEW)
    /* Get immediate argc and find the constructor function. */
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (2 + argc);
    JS_ASSERT(vp >= StackBase(fp));

    /*
     * Assign lval, obj, and fun exactly as the code at inline_call: expects to
     * find them, to avoid nesting a js_Interpret call via js_InvokeConstructor.
     */
    lval = *vp;
    if (VALUE_IS_FUNCTION(cx, lval)) {
        obj = JSVAL_TO_OBJECT(lval);
        fun = GET_FUNCTION_PRIVATE(cx, obj);
        if (FUN_INTERPRETED(fun)) {
            /* Root as we go using vp[1]. */
            if (!obj->getProperty(cx,
                                  ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                                  &vp[1])) {
                goto error;
            }
            rval = vp[1];
            obj2 = js_NewObject(cx, &js_ObjectClass,
                                JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : NULL,
                                OBJ_GET_PARENT(cx, obj));
            if (!obj2)
                goto error;

            if (fun->u.i.script->isEmpty()) {
                *vp = OBJECT_TO_JSVAL(obj2);
                regs.sp = vp + 1;
                goto end_new;
            }

            vp[1] = OBJECT_TO_JSVAL(obj2);
            flags = JSFRAME_CONSTRUCTING;
            goto inline_call;
        }
    }

    if (!js_InvokeConstructor(cx, argc, JS_FALSE, vp))
        goto error;
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    TRACE_0(NativeCallComplete);

  end_new:
END_CASE(JSOP_NEW)

BEGIN_CASE(JSOP_CALL)
BEGIN_CASE(JSOP_EVAL)
BEGIN_CASE(JSOP_APPLY)
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (argc + 2);

    lval = *vp;
    if (VALUE_IS_FUNCTION(cx, lval)) {
        obj = JSVAL_TO_OBJECT(lval);
        fun = GET_FUNCTION_PRIVATE(cx, obj);

        /* Clear frame flags since this is not a constructor call. */
        flags = 0;
        if (FUN_INTERPRETED(fun))
      inline_call:
        {
            uintN nframeslots, nvars, missing;
            JSArena *a;
            jsuword nbytes;
            void *newmark;
            jsval *newsp;
            JSInlineFrame *newifp;
            JSInterpreterHook hook;

            script = fun->u.i.script;
            if (script->isEmpty()) {
                script = fp->script;
                *vp = JSVAL_VOID;
                regs.sp = vp + 1;
                goto end_call;
            }

            /* Restrict recursion of lightweight functions. */
            if (inlineCallCount >= JS_MAX_INLINE_CALL_COUNT) {
                js_ReportOverRecursed(cx);
                script = fp->script;
                goto error;
            }

            /* Compute the total number of stack slots needed by fun. */
            nframeslots = JS_HOWMANY(sizeof(JSInlineFrame), sizeof(jsval));
            atoms = script->atomMap.vector;
            nbytes = (nframeslots + script->nslots) * sizeof(jsval);

            /* Allocate missing expected args adjacent to actuals. */
            a = cx->stackPool.current;
            newmark = (void *) a->avail;
            if (fun->nargs <= argc) {
                missing = 0;
            } else {
                newsp = vp + 2 + fun->nargs;
                JS_ASSERT(newsp > regs.sp);
                if ((jsuword) newsp <= a->limit) {
                    if ((jsuword) newsp > a->avail)
                        a->avail = (jsuword) newsp;
                    jsval *argsp = newsp;
                    do {
                        *--argsp = JSVAL_VOID;
                    } while (argsp != regs.sp);
                    missing = 0;
                } else {
                    missing = fun->nargs - argc;
                    nbytes += (2 + fun->nargs) * sizeof(jsval);
                }
            }

            /* Allocate the inline frame with its slots and operands. */
            if (a->avail + nbytes <= a->limit) {
                newsp = (jsval *) a->avail;
                a->avail += nbytes;
                JS_ASSERT(missing == 0);
            } else {
                JS_ARENA_ALLOCATE_CAST(newsp, jsval *, &cx->stackPool,
                                       nbytes);
                if (!newsp) {
                    js_ReportOutOfScriptQuota(cx);
                    goto bad_inline_call;
                }

                /*
                 * Move args if the missing ones overflow arena a, then push
                 * undefined for the missing args.
                 */
                if (missing) {
                    memcpy(newsp, vp, (2 + argc) * sizeof(jsval));
                    vp = newsp;
                    newsp = vp + 2 + argc;
                    do {
                        *newsp++ = JSVAL_VOID;
                    } while (--missing != 0);
                }
            }

            /* Claim space for the stack frame and initialize it. */
            newifp = (JSInlineFrame *) newsp;
            newsp += nframeslots;
            newifp->frame.callobj = NULL;
            newifp->frame.argsobj = NULL;
            newifp->frame.varobj = NULL;
            newifp->frame.script = script;
            newifp->frame.fun = fun;
            newifp->frame.argc = argc;
            newifp->frame.argv = vp + 2;
            newifp->frame.rval = JSVAL_VOID;
            newifp->frame.down = fp;
            newifp->frame.annotation = NULL;
            newifp->frame.scopeChain = parent = OBJ_GET_PARENT(cx, obj);
            newifp->frame.flags = flags;
            newifp->frame.dormantNext = NULL;
            newifp->frame.blockChain = NULL;
            if (script->staticLevel < JS_DISPLAY_SIZE) {
                JSStackFrame **disp = &cx->display[script->staticLevel];
                newifp->frame.displaySave = *disp;
                *disp = &newifp->frame;
            }
            newifp->mark = newmark;

            /* Compute the 'this' parameter now that argv is set. */
            JS_ASSERT(!JSFUN_BOUND_METHOD_TEST(fun->flags));
            newifp->frame.thisv = vp[1];

            newifp->frame.regs = NULL;
            newifp->frame.imacpc = NULL;
            newifp->frame.slots = newsp;

            /* Push void to initialize local variables. */
            nvars = fun->u.i.nvars;
            while (nvars--)
                *newsp++ = JSVAL_VOID;

            /* Scope with a call object parented by callee's parent. */
            if (JSFUN_HEAVYWEIGHT_TEST(fun->flags) &&
                !js_GetCallObject(cx, &newifp->frame)) {
                goto bad_inline_call;
            }

            /* Switch version if currentVersion wasn't overridden. */
            newifp->callerVersion = (JSVersion) cx->version;
            if (JS_LIKELY(cx->version == currentVersion)) {
                currentVersion = (JSVersion) script->version;
                if (currentVersion != cx->version)
                    js_SetVersion(cx, currentVersion);
            }

            /* Push the frame and set interpreter registers. */
            newifp->callerRegs = regs;
            fp->regs = &newifp->callerRegs;
            regs.sp = newsp;
            regs.pc = script->code;
            newifp->frame.regs = &regs;
            cx->fp = fp = &newifp->frame;

            /* Call the debugger hook if present. */
            hook = cx->debugHooks->callHook;
            if (hook) {
                newifp->hookData = hook(cx, &newifp->frame, JS_TRUE, 0,
                                        cx->debugHooks->callHookData);
                CHECK_INTERRUPT_HANDLER();
            } else {
                newifp->hookData = NULL;
            }

            inlineCallCount++;
            JS_RUNTIME_METER(rt, inlineCalls);

#ifdef INCLUDE_MOZILLA_DTRACE
            /* DTrace function entry, inlines */
            if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
                jsdtrace_function_entry(cx, fp, fun);
            if (JAVASCRIPT_FUNCTION_INFO_ENABLED())
                jsdtrace_function_info(cx, fp, fp->down, fun);
            if (JAVASCRIPT_FUNCTION_ARGS_ENABLED())
                jsdtrace_function_args(cx, fp, fun, fp->argc, fp->argv);
#endif

#ifdef JS_TRACER
            if (TRACE_RECORDER(cx)) {
                TRACE_1(EnterFrame, inlineCallCount);
                RESTORE_INTERP_VARS();
            } else if (fp->script == fp->down->script &&
                       *fp->down->regs->pc == JSOP_CALL &&
                       *fp->regs->pc == JSOP_TRACE) {
                MONITOR_BRANCH(Record_EnterFrame);
            }
#endif

            /* Load first op and dispatch it (safe since JSOP_STOP). */
            op = (JSOp) *regs.pc;
            DO_OP();

          bad_inline_call:
            JS_ASSERT(fp->regs == &regs);
            script = fp->script;
            atoms = script->atomMap.vector;
            js_FreeRawStack(cx, newmark);
            goto error;
        }

        if (fun->flags & JSFUN_FAST_NATIVE) {
#ifdef INCLUDE_MOZILLA_DTRACE
            /* DTrace function entry, non-inlines */
            if (VALUE_IS_FUNCTION(cx, lval)) {
                if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
                    jsdtrace_function_entry(cx, NULL, fun);
                if (JAVASCRIPT_FUNCTION_INFO_ENABLED())
                    jsdtrace_function_info(cx, NULL, fp, fun);
                if (JAVASCRIPT_FUNCTION_ARGS_ENABLED())
                    jsdtrace_function_args(cx, fp, fun, argc, vp+2);
            }
#endif

            JS_ASSERT(fun->u.n.extra == 0);
            JS_ASSERT(JSVAL_IS_OBJECT(vp[1]) ||
                      PRIMITIVE_THIS_TEST(fun, vp[1]));
            ok = ((JSFastNative) fun->u.n.native)(cx, argc, vp);
#ifdef INCLUDE_MOZILLA_DTRACE
            if (VALUE_IS_FUNCTION(cx, lval)) {
                if (JAVASCRIPT_FUNCTION_RVAL_ENABLED())
                    jsdtrace_function_rval(cx, NULL, fun, vp);
                if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
                    jsdtrace_function_return(cx, NULL, fun);
            }
#endif
            regs.sp = vp + 1;
            if (!ok) {
                /*
                 * If we are executing the JSOP_NEXTITER imacro and a
                 * Stopiteration exception is raised, transform it into a
                 * JSVAL_HOLE return value.  The tracer generates equivalent
                 * code by calling CatchStopIteration_tn.
                 */
                if (fp->imacpc && *fp->imacpc == JSOP_NEXTITER &&
                    cx->throwing && js_ValueIsStopIteration(cx->exception)) {
                    // pc may point to JSOP_DUP here due to bug 474854.
                    JS_ASSERT(*regs.pc == JSOP_CALL ||
                              *regs.pc == JSOP_DUP);
                    cx->throwing = JS_FALSE;
                    cx->exception = JSVAL_VOID;
                    regs.sp[-1] = JSVAL_HOLE;
                } else {
                    goto error;
                }
            }
            TRACE_0(NativeCallComplete);
            goto end_call;
        }
    }

    ok = js_Invoke(cx, argc, vp, 0);
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    if (!ok)
        goto error;
    JS_RUNTIME_METER(rt, nonInlineCalls);
    TRACE_0(NativeCallComplete);

  end_call:
END_CASE(JSOP_CALL)

BEGIN_CASE(JSOP_SETCALL)
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - argc - 2;
    if (js_Invoke(cx, argc, vp, 0))
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    goto error;
END_CASE(JSOP_SETCALL)

BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    JSPropCacheEntry *entry;

    obj = fp->scopeChain;
    if (JS_LIKELY(OBJ_IS_NATIVE(obj))) {
        PROPERTY_CACHE_TEST(cx, regs.pc, obj, obj2, entry, atom);
        if (!atom) {
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            if (PCVAL_IS_OBJECT(entry->vword)) {
                rval = PCVAL_OBJECT_TO_JSVAL(entry->vword);
                JS_UNLOCK_OBJ(cx, obj2);
                goto do_push_rval;
            }

            if (PCVAL_IS_SLOT(entry->vword)) {
                slot = PCVAL_TO_SLOT(entry->vword);
                JS_ASSERT(slot < OBJ_SCOPE(obj2)->freeslot);
                rval = LOCKED_OBJ_GET_SLOT(obj2, slot);
                JS_UNLOCK_OBJ(cx, obj2);
                goto do_push_rval;
            }

            JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
            sprop = PCVAL_TO_SPROP(entry->vword);
            goto do_native_get;
        }
    } else {
        LOAD_ATOM(0);
    }

    id = ATOM_TO_JSID(atom);
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        endpc = script->code + script->length;
        op2 = js_GetOpcode(cx, script, regs.pc + JSOP_NAME_LENGTH);
        if (op2 == JSOP_TYPEOF) {
            PUSH_OPND(JSVAL_VOID);
            len = JSOP_NAME_LENGTH;
            DO_NEXT_OP(len);
        }
        goto atom_not_defined;
    }

    /* Take the slow path if prop was not found in a native object. */
    if (!OBJ_IS_NATIVE(obj) || !OBJ_IS_NATIVE(obj2)) {
        obj2->dropProperty(cx, prop);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    } else {
        sprop = (JSScopeProperty *)prop;
  do_native_get:
        NATIVE_GET(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, &rval);
        obj2->dropProperty(cx, (JSProperty *) sprop);
    }

  do_push_rval:
    PUSH_OPND(rval);
    if (op == JSOP_CALLNAME)
        PUSH_OPND(OBJECT_TO_JSVAL(obj));
}
END_CASE(JSOP_NAME)

BEGIN_CASE(JSOP_UINT16)
    i = (jsint) GET_UINT16(regs.pc);
    rval = INT_TO_JSVAL(i);
    PUSH_OPND(rval);
END_CASE(JSOP_UINT16)

BEGIN_CASE(JSOP_UINT24)
    i = (jsint) GET_UINT24(regs.pc);
    rval = INT_TO_JSVAL(i);
    PUSH_OPND(rval);
END_CASE(JSOP_UINT24)

BEGIN_CASE(JSOP_INT8)
    i = GET_INT8(regs.pc);
    rval = INT_TO_JSVAL(i);
    PUSH_OPND(rval);
END_CASE(JSOP_INT8)

BEGIN_CASE(JSOP_INT32)
    i = GET_INT32(regs.pc);
    rval = INT_TO_JSVAL(i);
    PUSH_OPND(rval);
END_CASE(JSOP_INT32)

BEGIN_CASE(JSOP_INDEXBASE)
    /*
     * Here atoms can exceed script->atomMap.length as we use atoms as a
     * segment register for object literals as well.
     */
    atoms += GET_INDEXBASE(regs.pc);
END_CASE(JSOP_INDEXBASE)

BEGIN_CASE(JSOP_INDEXBASE1)
BEGIN_CASE(JSOP_INDEXBASE2)
BEGIN_CASE(JSOP_INDEXBASE3)
    atoms += (op - JSOP_INDEXBASE1 + 1) << 16;
END_CASE(JSOP_INDEXBASE3)

BEGIN_CASE(JSOP_RESETBASE0)
BEGIN_CASE(JSOP_RESETBASE)
    atoms = script->atomMap.vector;
END_CASE(JSOP_RESETBASE)

BEGIN_CASE(JSOP_DOUBLE)
    JS_ASSERT(!fp->imacpc);
    JS_ASSERT(size_t(atoms - script->atomMap.vector) < script->atomMap.length);
    /* FALL THROUGH */

BEGIN_CASE(JSOP_STRING)
    LOAD_ATOM(0);
    PUSH_OPND(ATOM_KEY(atom));
END_CASE(JSOP_DOUBLE)

BEGIN_CASE(JSOP_OBJECT)
    LOAD_OBJECT(0);
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_OBJECT)

BEGIN_CASE(JSOP_REGEXP)
{
    JSObject *funobj;

    /*
     * Push a regexp object for the atom mapped by the bytecode at pc, cloning
     * the literal's regexp object if necessary, to simulate in the
     * pre-compile/execute-later case what ECMA specifies for the
     * compile-and-go case: that scanning each regexp literal creates a single
     * corresponding RegExp object.
     *
     * To support pre-compilation transparently, we must handle the case where
     * a regexp object literal is used in a different global at execution time
     * from the global with which it was scanned at compile time.  We do this
     * by re-wrapping the JSRegExp private data struct with a cloned object
     * having the right prototype and parent, and having its own lastIndex
     * property value storage.
     *
     * Unlike JSOP_DEFFUN and other prolog bytecodes that may clone literal
     * objects, we don't want to pay a script prolog execution price for all
     * regexp literals in a script (many may not be used by a particular
     * execution of that script, depending on control flow), so we initialize
     * lazily here.
     *
     * XXX This code is specific to regular expression objects.  If we need a
     * similar op for other kinds of object literals, we should push cloning
     * down under JSObjectOps and reuse code here.
     */
    index = GET_FULL_INDEX(0);
    JS_ASSERT(index < script->regexps()->length);

    slot = index;
    if (fp->fun) {
        /*
         * We're in function code, not global or eval code (in eval code,
         * JSOP_REGEXP is never emitted). The cloned funobj contains
         * script->regexps()->length reserved slots for the cloned regexps; see
         * fun_reserveSlots, jsfun.c.
         */
        funobj = JSVAL_TO_OBJECT(fp->argv[-2]);
        slot += JSCLASS_RESERVED_SLOTS(&js_FunctionClass);
        if (script->upvarsOffset != 0)
            slot += script->upvars()->length;
        if (!JS_GetReservedSlot(cx, funobj, slot, &rval))
            goto error;
        if (JSVAL_IS_VOID(rval))
            rval = JSVAL_NULL;
    } else {
        /*
         * We're in global code. The code generator reserved a slot for the
         * regexp among script->nfixed slots. All such slots are initialized to
         * null, not void, for faster testing in JSOP_*GVAR cases. To simplify
         * index calculations we count regexps in the reverse order down from
         * script->nslots - 1.
         */
        JS_ASSERT(slot < script->nfixed);
        slot = script->nfixed - slot - 1;
        rval = fp->slots[slot];
#ifdef __GNUC__
        funobj = NULL;  /* suppress bogus gcc warnings */
#endif
    }

    if (JSVAL_IS_NULL(rval)) {
        /* Compute the current global object in obj2. */
        obj2 = fp->scopeChain;
        while ((parent = OBJ_GET_PARENT(cx, obj2)) != NULL)
            obj2 = parent;

        /*
         * If obj's parent is not obj2, we must clone obj so that it has the
         * right parent, and therefore, the right prototype.
         *
         * Yes, this means we assume that the correct RegExp.prototype to which
         * regexp instances (including literals) delegate can be distinguished
         * solely by the instance's parent, which was set to the parent of the
         * RegExp constructor function object when the instance was created.
         * In other words,
         *
         *   (/x/.__parent__ == RegExp.__parent__) implies
         *   (/x/.__proto__ == RegExp.prototype)
         *
         * (unless you assign a different object to RegExp.prototype at
         * runtime, in which case, ECMA doesn't specify operation, and you get
         * what you deserve).
         *
         * This same coupling between instance parent and constructor parent
         * turns up everywhere (see jsobj.c's FindClassObject,
         * js_ConstructObject, and js_NewObject).  It's fundamental to the
         * design of the language when you consider multiple global objects and
         * separate compilation and execution, even though it is not specified
         * fully in ECMA.
         */
        obj = script->getRegExp(index);
        if (OBJ_GET_PARENT(cx, obj) != obj2) {
            obj = js_CloneRegExpObject(cx, obj, obj2);
            if (!obj)
                goto error;
        }
        rval = OBJECT_TO_JSVAL(obj);

        /* Store the regexp object value in its cloneIndex slot. */
        if (fp->fun) {
            if (!JS_SetReservedSlot(cx, funobj, slot, rval))
                goto error;
        } else {
            fp->slots[slot] = rval;
        }
    }

    PUSH_OPND(rval);
}
END_CASE(JSOP_REGEXP)

BEGIN_CASE(JSOP_ZERO)
    PUSH_OPND(JSVAL_ZERO);
END_CASE(JSOP_ZERO)

BEGIN_CASE(JSOP_ONE)
    PUSH_OPND(JSVAL_ONE);
END_CASE(JSOP_ONE)

BEGIN_CASE(JSOP_NULL)
    PUSH_OPND(JSVAL_NULL);
END_CASE(JSOP_NULL)

BEGIN_CASE(JSOP_FALSE)
    PUSH_OPND(JSVAL_FALSE);
END_CASE(JSOP_FALSE)

BEGIN_CASE(JSOP_TRUE)
    PUSH_OPND(JSVAL_TRUE);
END_CASE(JSOP_TRUE)

BEGIN_CASE(JSOP_TABLESWITCH)
    pc2 = regs.pc;
    len = GET_JUMP_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    rval = POP_OPND();
    if (JSVAL_IS_INT(rval)) {
        i = JSVAL_TO_INT(rval);
    } else if (JSVAL_IS_DOUBLE(rval) && *JSVAL_TO_DOUBLE(rval) == 0) {
        /* Treat -0 (double) as 0. */
        i = 0;
    } else {
        DO_NEXT_OP(len);
    }

    pc2 += JUMP_OFFSET_LEN;
    low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
        off = (jsint) GET_JUMP_OFFSET(pc2);
        if (off)
            len = off;
    }
END_VARLEN_CASE

BEGIN_CASE(JSOP_TABLESWITCHX)
    pc2 = regs.pc;
    len = GET_JUMPX_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    rval = POP_OPND();
    if (JSVAL_IS_INT(rval)) {
        i = JSVAL_TO_INT(rval);
    } else if (JSVAL_IS_DOUBLE(rval) && *JSVAL_TO_DOUBLE(rval) == 0) {
        /* Treat -0 (double) as 0. */
        i = 0;
    } else {
        DO_NEXT_OP(len);
    }

    pc2 += JUMPX_OFFSET_LEN;
    low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMPX_OFFSET_LEN * i;
        off = (jsint) GET_JUMPX_OFFSET(pc2);
        if (off)
            len = off;
    }
END_VARLEN_CASE

BEGIN_CASE(JSOP_LOOKUPSWITCHX)
    off = JUMPX_OFFSET_LEN;
    goto do_lookup_switch;

BEGIN_CASE(JSOP_LOOKUPSWITCH)
    off = JUMP_OFFSET_LEN;

  do_lookup_switch:
    /*
     * JSOP_LOOKUPSWITCH and JSOP_LOOKUPSWITCHX are never used if any atom
     * index in it would exceed 64K limit.
     */
    JS_ASSERT(!fp->imacpc);
    JS_ASSERT(atoms == script->atomMap.vector);
    pc2 = regs.pc;
    lval = POP_OPND();

    if (!JSVAL_IS_NUMBER(lval) &&
        !JSVAL_IS_STRING(lval) &&
        !JSVAL_IS_BOOLEAN(lval)) {
        goto end_lookup_switch;
    }

    pc2 += off;
    npairs = (jsint) GET_UINT16(pc2);
    pc2 += UINT16_LEN;
    JS_ASSERT(npairs);  /* empty switch uses JSOP_TABLESWITCH */

#define SEARCH_PAIRS(MATCH_CODE)                                              \
    for (;;) {                                                                \
        JS_ASSERT(GET_INDEX(pc2) < script->atomMap.length);                   \
        atom = atoms[GET_INDEX(pc2)];                                         \
        rval = ATOM_KEY(atom);                                                \
        MATCH_CODE                                                            \
        pc2 += INDEX_LEN;                                                     \
        if (match)                                                            \
            break;                                                            \
        pc2 += off;                                                           \
        if (--npairs == 0) {                                                  \
            pc2 = regs.pc;                                                    \
            break;                                                            \
        }                                                                     \
    }

    if (JSVAL_IS_STRING(lval)) {
        str = JSVAL_TO_STRING(lval);
        SEARCH_PAIRS(
            match = (JSVAL_IS_STRING(rval) &&
                     ((str2 = JSVAL_TO_STRING(rval)) == str ||
                      js_EqualStrings(str2, str)));
        )
    } else if (JSVAL_IS_DOUBLE(lval)) {
        d = *JSVAL_TO_DOUBLE(lval);
        SEARCH_PAIRS(
            match = (JSVAL_IS_DOUBLE(rval) &&
                     *JSVAL_TO_DOUBLE(rval) == d);
        )
    } else {
        SEARCH_PAIRS(
            match = (lval == rval);
        )
    }
#undef SEARCH_PAIRS

  end_lookup_switch:
    len = (op == JSOP_LOOKUPSWITCH)
          ? GET_JUMP_OFFSET(pc2)
          : GET_JUMPX_OFFSET(pc2);
END_VARLEN_CASE

BEGIN_CASE(JSOP_TRAP)
{
    JSTrapStatus status;

    status = JS_HandleTrap(cx, script, regs.pc, &rval);
    switch (status) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_RETURN:
        fp->rval = rval;
        ok = JS_TRUE;
        goto forced_return;
      case JSTRAP_THROW:
        cx->throwing = JS_TRUE;
        cx->exception = rval;
        goto error;
      default:
        break;
    }
    JS_ASSERT(status == JSTRAP_CONTINUE);
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(JSVAL_IS_INT(rval));
    op = (JSOp) JSVAL_TO_INT(rval);
    JS_ASSERT((uintN)op < (uintN)JSOP_LIMIT);
    DO_OP();
}

BEGIN_CASE(JSOP_ARGUMENTS)
    if (!js_GetArgsValue(cx, fp, &rval))
        goto error;
    PUSH_OPND(rval);
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_ARGSUB)
    id = INT_TO_JSID(GET_ARGNO(regs.pc));
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_OPND(rval);
END_CASE(JSOP_ARGSUB)

BEGIN_CASE(JSOP_ARGCNT)
    id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_OPND(rval);
END_CASE(JSOP_ARGCNT)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    PUSH_OPND(fp->argv[slot]);
    if (op == JSOP_CALLARG)
        PUSH_OPND(JSVAL_NULL);
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    vp = &fp->argv[slot];
    *vp = FETCH_OPND(-1);
END_SET_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_OPND(fp->slots[slot]);
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_CALLLOCAL)
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_OPND(fp->slots[slot]);
    PUSH_OPND(JSVAL_NULL);
END_CASE(JSOP_CALLLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    vp = &fp->slots[slot];
    *vp = FETCH_OPND(-1);
END_SET_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETUPVAR)
BEGIN_CASE(JSOP_CALLUPVAR)
{
    JSUpvarArray *uva = script->upvars();

    index = GET_UINT16(regs.pc);
    JS_ASSERT(index < uva->length);

    rval = js_GetUpvar(cx, script->staticLevel, uva->vector[index]);
    PUSH_OPND(rval);

    if (op == JSOP_CALLUPVAR)
        PUSH_OPND(JSVAL_NULL);
}
END_CASE(JSOP_GETUPVAR)

BEGIN_CASE(JSOP_GETUPVAR_DBG)
BEGIN_CASE(JSOP_CALLUPVAR_DBG)
    fun = fp->fun;
    JS_ASSERT(FUN_KIND(fun) == JSFUN_INTERPRETED);
    JS_ASSERT(fun->u.i.wrapper);

    /* Scope for tempPool mark and local names allocation in it. */
    {
        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jsuword *names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
        if (!names)
            goto error;

        index = fun->countArgsAndVars() + GET_UINT16(regs.pc);
        atom = JS_LOCAL_NAME_TO_ATOM(names[index]);
        id = ATOM_TO_JSID(atom);

        ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (!ok)
            goto error;
    }

    if (!prop)
        goto atom_not_defined;

    /* Minimize footprint with generic code instead of NATIVE_GET. */
    obj2->dropProperty(cx, prop);
    vp = regs.sp;
    PUSH_OPND(JSVAL_NULL);
    if (!obj->getProperty(cx, id, vp))
        goto error;

    if (op == JSOP_CALLUPVAR_DBG)
        PUSH_OPND(JSVAL_NULL);
END_CASE(JSOP_GETUPVAR_DBG)

BEGIN_CASE(JSOP_GETDSLOT)
BEGIN_CASE(JSOP_CALLDSLOT)
    JS_ASSERT(fp->argv);
    obj = JSVAL_TO_OBJECT(fp->argv[-2]);
    JS_ASSERT(obj);
    JS_ASSERT(obj->dslots);

    index = GET_UINT16(regs.pc);
    JS_ASSERT(JS_INITIAL_NSLOTS + index < jsatomid(obj->dslots[-1]));
    JS_ASSERT_IF(OBJ_SCOPE(obj)->object == obj,
                 JS_INITIAL_NSLOTS + index < OBJ_SCOPE(obj)->freeslot);

    PUSH_OPND(obj->dslots[index]);
    if (op == JSOP_CALLDSLOT)
        PUSH_OPND(JSVAL_NULL);
END_CASE(JSOP_GETDSLOT)

BEGIN_CASE(JSOP_GETGVAR)
BEGIN_CASE(JSOP_CALLGVAR)
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    lval = fp->slots[slot];
    if (JSVAL_IS_NULL(lval)) {
        op = (op == JSOP_GETGVAR) ? JSOP_NAME : JSOP_CALLNAME;
        DO_OP();
    }
    obj = fp->varobj;
    slot = JSVAL_TO_INT(lval);
    rval = OBJ_GET_SLOT(cx, obj, slot);
    PUSH_OPND(rval);
    if (op == JSOP_CALLGVAR)
        PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_GETGVAR)

BEGIN_CASE(JSOP_SETGVAR)
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < GlobalVarCount(fp));
    METER_SLOT_OP(op, slot);
    rval = FETCH_OPND(-1);
    obj = fp->varobj;
    lval = fp->slots[slot];
    if (JSVAL_IS_NULL(lval)) {
        /*
         * Inline-clone and deoptimize JSOP_SETNAME code here because
         * JSOP_SETGVAR has arity 1: [rval], not arity 2: [obj, rval]
         * as JSOP_SETNAME does, where [obj] is due to JSOP_BINDNAME.
         */
#ifdef JS_TRACER
        if (TRACE_RECORDER(cx))
            js_AbortRecording(cx, "SETGVAR with NULL slot");
#endif
        LOAD_ATOM(0);
        id = ATOM_TO_JSID(atom);
        if (!obj->setProperty(cx, id, &rval))
            goto error;
    } else {
        slot = JSVAL_TO_INT(lval);
        JS_LOCK_OBJ(cx, obj);
        JSScope *scope = OBJ_SCOPE(obj);
        if (!scope->methodWriteBarrier(cx, slot, rval)) {
            JS_UNLOCK_SCOPE(cx, scope);
            goto error;
        }
        LOCKED_OBJ_SET_SLOT(obj, slot, rval);
        JS_UNLOCK_SCOPE(cx, scope);
    }
END_SET_CASE(JSOP_SETGVAR)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
    index = GET_INDEX(regs.pc);
    atom = atoms[index];

    /*
     * index is relative to atoms at this point but for global var
     * code below we need the absolute value.
     */
    index += atoms - script->atomMap.vector;
    obj = fp->varobj;
    JS_ASSERT(obj->map->ops->defineProperty == js_DefineProperty);
    attrs = JSPROP_ENUMERATE;
    if (!(fp->flags & JSFRAME_EVAL))
        attrs |= JSPROP_PERMANENT;
    if (op == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    /* Lookup id in order to check for redeclaration problems. */
    id = ATOM_TO_JSID(atom);
    prop = NULL;
    if (!js_CheckRedeclaration(cx, obj, id, attrs, &obj2, &prop))
        goto error;

    /* Bind a variable only if it's not yet defined. */
    if (!prop) {
        if (!js_DefineNativeProperty(cx, obj, id, JSVAL_VOID, JS_PropertyStub, JS_PropertyStub,
                                     attrs, 0, 0, &prop)) {
            goto error;
        }
        JS_ASSERT(prop);
        obj2 = obj;
    }

    /*
     * Try to optimize a property we either just created, or found
     * directly in the global object, that is permanent, has a slot,
     * and has stub getter and setter, into a "fast global" accessed
     * by the JSOP_*GVAR opcodes.
     */
    if (!fp->fun &&
        index < GlobalVarCount(fp) &&
        obj2 == obj &&
        OBJ_IS_NATIVE(obj)) {
        sprop = (JSScopeProperty *) prop;
        if ((sprop->attrs & JSPROP_PERMANENT) &&
            SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj)) &&
            SPROP_HAS_STUB_GETTER_OR_IS_METHOD(sprop) &&
            SPROP_HAS_STUB_SETTER(sprop)) {
            /*
             * Fast globals use frame variables to map the global
             * name's atom index to the permanent fp->varobj slot
             * number, tagged as a jsval. The atom index for the
             * global's name literal is identical to its variable
             * index.
             */
            fp->slots[index] = INT_TO_JSVAL(sprop->slot);
        }
    }

    obj2->dropProperty(cx, prop);
END_CASE(JSOP_DEFVAR)

BEGIN_CASE(JSOP_DEFFUN)
{
    JSPropertyOp getter, setter;
    bool doSet;
    JSObject *pobj;
    JSProperty *prop;
    uint32 old;

    /*
     * A top-level function defined in Global or Eval code (see ECMA-262
     * Ed. 3), or else a SpiderMonkey extension: a named function statement in
     * a compound statement (not at the top statement level of global code, or
     * at the top level of a function body).
     */
    LOAD_FUNCTION(0);
    obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        /*
         * Even a null closure needs a parent for principals finding.
         * FIXME: bug 476950, although debugger users may also demand some kind
         * of scope link for debugger-assisted eval-in-frame.
         */
        obj2 = fp->scopeChain;
    } else {
        JS_ASSERT(!FUN_FLAT_CLOSURE(fun));

        /*
         * Inline js_GetScopeChain a bit to optimize for the case of a
         * top-level function.
         */
        if (!fp->blockChain) {
            obj2 = fp->scopeChain;
        } else {
            obj2 = js_GetScopeChain(cx, fp);
            if (!obj2)
                goto error;
        }
    }

    /*
     * If static link is not current scope, clone fun's object to link to the
     * current scope via parent. We do this to enable sharing of compiled
     * functions among multiple equivalent scopes, amortizing the cost of
     * compilation over a number of executions.  Examples include XUL scripts
     * and event handlers shared among Firefox or other Mozilla app chrome
     * windows, and user-defined JS functions precompiled and then shared among
     * requests in server-side JS.
     */
    if (OBJ_GET_PARENT(cx, obj) != obj2) {
        obj = js_CloneFunctionObject(cx, fun, obj2);
        if (!obj)
            goto error;
    }

    /*
     * Protect obj from any GC hiding below JSObject::setProperty or
     * JSObject::defineProperty.  All paths from here must flow through the
     * fp->scopeChain code below the parent->defineProperty call.
     */
    MUST_FLOW_THROUGH("restore_scope");
    fp->scopeChain = obj;

    rval = OBJECT_TO_JSVAL(obj);

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    attrs = (fp->flags & JSFRAME_EVAL)
            ? JSPROP_ENUMERATE
            : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * Load function flags that are also property attributes.  Getters and
     * setters do not need a slot, their value is stored elsewhere in the
     * property itself, not in obj slots.
     */
    getter = setter = JS_PropertyStub;
    flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        /* Function cannot be both getter a setter. */
        JS_ASSERT(flags == JSPROP_GETTER || flags == JSPROP_SETTER);
        attrs |= flags | JSPROP_SHARED;
        rval = JSVAL_VOID;
        if (flags == JSPROP_GETTER)
            getter = js_CastAsPropertyOp(obj);
        else
            setter = js_CastAsPropertyOp(obj);
    }

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    parent = fp->varobj;
    JS_ASSERT(parent);

    /*
     * Check for a const property of the same name -- or any kind of property
     * if executing with the strict option.  We check here at runtime as well
     * as at compile-time, to handle eval as well as multiple HTML script tags.
     */
    id = ATOM_TO_JSID(fun->atom);
    prop = NULL;
    ok = js_CheckRedeclaration(cx, parent, id, attrs, &pobj, &prop);
    if (!ok)
        goto restore_scope;

    /*
     * We deviate from 10.1.2 in ECMA 262 v3 and under eval use for function
     * declarations JSObject::setProperty, not JSObject::defineProperty, to
     * preserve the JSOP_PERMANENT attribute of existing properties and make
     * sure that such properties cannot be deleted.
     *
     * We also use JSObject::setProperty for the existing properties of Call
     * objects with matching attributes to preserve the native getters and
     * setters that store the value of the property in the interpreter frame,
     * see bug 467495.
     */
    doSet = (attrs == JSPROP_ENUMERATE);
    JS_ASSERT_IF(doSet, fp->flags & JSFRAME_EVAL);
    if (prop) {
        if (parent == pobj &&
            OBJ_GET_CLASS(cx, parent) == &js_CallClass &&
            (old = ((JSScopeProperty *) prop)->attrs,
             !(old & (JSPROP_GETTER|JSPROP_SETTER)) &&
             (old & (JSPROP_ENUMERATE|JSPROP_PERMANENT)) == attrs)) {
            /*
             * js_CheckRedeclaration must reject attempts to add a getter or
             * setter to an existing property without a getter or setter.
             */
            JS_ASSERT(!(attrs & ~(JSPROP_ENUMERATE|JSPROP_PERMANENT)));
            JS_ASSERT(!(old & JSPROP_READONLY));
            doSet = JS_TRUE;
        }
        pobj->dropProperty(cx, prop);
    }
    ok = doSet
         ? parent->setProperty(cx, id, &rval)
         : parent->defineProperty(cx, id, rval, getter, setter, attrs);

  restore_scope:
    /* Restore fp->scopeChain now that obj is defined in fp->varobj. */
    fp->scopeChain = obj2;
    if (!ok)
        goto error;
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_DEFFUN_FC)
BEGIN_CASE(JSOP_DEFFUN_DBGFC)
    LOAD_FUNCTION(0);

    obj = (op == JSOP_DEFFUN_FC)
          ? js_NewFlatClosure(cx, fun)
          : js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;
    rval = OBJECT_TO_JSVAL(obj);

    attrs = (fp->flags & JSFRAME_EVAL)
            ? JSPROP_ENUMERATE
            : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        attrs |= flags | JSPROP_SHARED;
        rval = JSVAL_VOID;
    }

    parent = fp->varobj;
    JS_ASSERT(parent);

    id = ATOM_TO_JSID(fun->atom);
    ok = js_CheckRedeclaration(cx, parent, id, attrs, NULL, NULL);
    if (ok) {
        if (attrs == JSPROP_ENUMERATE) {
            JS_ASSERT(fp->flags & JSFRAME_EVAL);
            ok = parent->setProperty(cx, id, &rval);
        } else {
            JS_ASSERT(attrs & JSPROP_PERMANENT);

            ok = parent->defineProperty(cx, id, rval,
                                        (flags & JSPROP_GETTER)
                                        ? js_CastAsPropertyOp(obj)
                                        : JS_PropertyStub,
                                        (flags & JSPROP_SETTER)
                                        ? js_CastAsPropertyOp(obj)
                                        : JS_PropertyStub,
                                        attrs);
        }
    }

    if (!ok)
        goto error;
END_CASE(JSOP_DEFFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN)
    /*
     * Define a local function (i.e., one nested at the top level of another
     * function), parented by the current scope chain, stored in a local
     * variable slot that the compiler allocated.  This is an optimization over
     * JSOP_DEFFUN that avoids requiring a call object for the outer function's
     * activation.
     */
    LOAD_FUNCTION(SLOTNO_LEN);
    JS_ASSERT(FUN_INTERPRETED(fun));
    JS_ASSERT(!FUN_FLAT_CLOSURE(fun));
    obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        obj = js_CloneFunctionObject(cx, fun, fp->scopeChain);
        if (!obj)
            goto error;
    } else {
        parent = js_GetScopeChain(cx, fp);
        if (!parent)
            goto error;

        if (OBJ_GET_PARENT(cx, obj) != parent) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                js_AbortRecording(cx, "DEFLOCALFUN for closure");
#endif
            obj = js_CloneFunctionObject(cx, fun, parent);
            if (!obj)
                goto error;
        }
    }

    slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots[slot] = OBJECT_TO_JSVAL(obj);
END_CASE(JSOP_DEFLOCALFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
    LOAD_FUNCTION(SLOTNO_LEN);

    obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots[slot] = OBJECT_TO_JSVAL(obj);
END_CASE(JSOP_DEFLOCALFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN_DBGFC)
    LOAD_FUNCTION(SLOTNO_LEN);

    obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    slot = GET_SLOTNO(regs.pc);
    fp->slots[slot] = OBJECT_TO_JSVAL(obj);
END_CASE(JSOP_DEFLOCALFUN_DBGFC)

BEGIN_CASE(JSOP_LAMBDA)
    /* Load the specified function object literal. */
    LOAD_FUNCTION(0);
    obj = FUN_OBJECT(fun);

    /* do-while(0) so we can break instead of using a goto. */
    do {
        if (FUN_NULL_CLOSURE(fun)) {
            parent = fp->scopeChain;

            if (OBJ_GET_PARENT(cx, obj) == parent) {
                op = JSOp(regs.pc[JSOP_LAMBDA_LENGTH]);

                /*
                 * Optimize ({method: function () { ... }, ...}) and
                 * this.method = function () { ... }; bytecode sequences.
                 */
                if (op == JSOP_SETMETHOD) {
#ifdef DEBUG
                    op2 = JSOp(regs.pc[JSOP_LAMBDA_LENGTH + JSOP_SETMETHOD_LENGTH]);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif

                    lval = FETCH_OPND(-1);
                    if (JSVAL_IS_OBJECT(lval) &&
                        (obj2 = JSVAL_TO_OBJECT(lval)) &&
                        OBJ_GET_CLASS(cx, obj2) == &js_ObjectClass) {
                        break;
                    }
                } else if (op == JSOP_INITMETHOD) {
                    lval = FETCH_OPND(-1);
                    JS_ASSERT(!JSVAL_IS_PRIMITIVE(lval));
                    obj2 = JSVAL_TO_OBJECT(lval);
                    JS_ASSERT(OBJ_GET_CLASS(cx, obj2) == &js_ObjectClass);
                    JS_ASSERT(OBJ_SCOPE(obj2)->object == obj2);
                    break;
                }
            }
        } else {
            parent = js_GetScopeChain(cx, fp);
            if (!parent)
                goto error;
        }

        obj = js_CloneFunctionObject(cx, fun, parent);
        if (!obj)
            goto error;
    } while (0);

    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_LAMBDA_FC)
    LOAD_FUNCTION(0);

    obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_LAMBDA_FC)

BEGIN_CASE(JSOP_LAMBDA_DBGFC)
    LOAD_FUNCTION(0);

    obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_LAMBDA_DBGFC)

BEGIN_CASE(JSOP_CALLEE)
    PUSH_OPND(fp->argv[-2]);
END_CASE(JSOP_CALLEE)

#if JS_HAS_GETTER_SETTER
BEGIN_CASE(JSOP_GETTER)
BEGIN_CASE(JSOP_SETTER)
  do_getter_setter:
    op2 = (JSOp) *++regs.pc;
    switch (op2) {
      case JSOP_INDEXBASE:
        atoms += GET_INDEXBASE(regs.pc);
        regs.pc += JSOP_INDEXBASE_LENGTH - 1;
        goto do_getter_setter;
      case JSOP_INDEXBASE1:
      case JSOP_INDEXBASE2:
      case JSOP_INDEXBASE3:
        atoms += (op2 - JSOP_INDEXBASE1 + 1) << 16;
        goto do_getter_setter;

      case JSOP_SETNAME:
      case JSOP_SETPROP:
        LOAD_ATOM(0);
        id = ATOM_TO_JSID(atom);
        rval = FETCH_OPND(-1);
        i = -1;
        goto gs_pop_lval;

      case JSOP_SETELEM:
        rval = FETCH_OPND(-1);
        id = 0;
        i = -2;
      gs_pop_lval:
        FETCH_OBJECT(cx, i - 1, lval, obj);
        break;

      case JSOP_INITPROP:
        JS_ASSERT(regs.sp - StackBase(fp) >= 2);
        rval = FETCH_OPND(-1);
        i = -1;
        LOAD_ATOM(0);
        id = ATOM_TO_JSID(atom);
        goto gs_get_lval;

      default:
        JS_ASSERT(op2 == JSOP_INITELEM);

        JS_ASSERT(regs.sp - StackBase(fp) >= 3);
        rval = FETCH_OPND(-1);
        id = 0;
        i = -2;
      gs_get_lval:
        lval = FETCH_OPND(i-1);
        JS_ASSERT(JSVAL_IS_OBJECT(lval));
        obj = JSVAL_TO_OBJECT(lval);
        break;
    }

    /* Ensure that id has a type suitable for use with obj. */
    if (id == 0)
        FETCH_ELEMENT_ID(obj, i, id);

    if (JS_TypeOfValue(cx, rval) != JSTYPE_FUNCTION) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_GETTER_OR_SETTER,
                             (op == JSOP_GETTER)
                             ? js_getter_str
                             : js_setter_str);
        goto error;
    }

    /*
     * Getters and setters are just like watchpoints from an access control
     * point of view.
     */
    if (!obj->checkAccess(cx, id, JSACC_WATCH, &rtmp, &attrs))
        goto error;

    if (op == JSOP_GETTER) {
        getter = js_CastAsPropertyOp(JSVAL_TO_OBJECT(rval));
        setter = JS_PropertyStub;
        attrs = JSPROP_GETTER;
    } else {
        getter = JS_PropertyStub;
        setter = js_CastAsPropertyOp(JSVAL_TO_OBJECT(rval));
        attrs = JSPROP_SETTER;
    }
    attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

    /* Check for a readonly or permanent property of the same name. */
    if (!js_CheckRedeclaration(cx, obj, id, attrs, NULL, NULL))
        goto error;

    if (!obj->defineProperty(cx, id, JSVAL_VOID, getter, setter, attrs))
        goto error;

    regs.sp += i;
    if (js_CodeSpec[op2].ndefs > js_CodeSpec[op2].nuses) {
        JS_ASSERT(js_CodeSpec[op2].ndefs == js_CodeSpec[op2].nuses + 1);
        STORE_OPND(-1, rval);
    }
    len = js_CodeSpec[op2].length;
    DO_NEXT_OP(len);
#endif /* JS_HAS_GETTER_SETTER */

BEGIN_CASE(JSOP_HOLE)
    PUSH_OPND(JSVAL_HOLE);
END_CASE(JSOP_HOLE)

BEGIN_CASE(JSOP_NEWARRAY)
    len = GET_UINT16(regs.pc);
    cx->fp->assertValidStackDepth(len);
    obj = js_NewArrayObject(cx, len, regs.sp - len, JS_TRUE);
    if (!obj)
        goto error;
    regs.sp -= len - 1;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_NEWARRAY)

BEGIN_CASE(JSOP_NEWINIT)
    i = GET_INT8(regs.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);
    if (i == JSProto_Array) {
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
    } else {
        obj = js_NewObject(cx, &js_ObjectClass, NULL, NULL);
        if (!obj)
            goto error;

        if (regs.pc[JSOP_NEWINIT_LENGTH] != JSOP_ENDINIT) {
            JS_LOCK_OBJ(cx, obj);
            JSScope *scope = js_GetMutableScope(cx, obj);
            if (!scope) {
                JS_UNLOCK_OBJ(cx, obj);
                goto error;
            }
            JS_UNLOCK_SCOPE(cx, scope);
        }
    }

    PUSH_OPND(OBJECT_TO_JSVAL(obj));
    CHECK_INTERRUPT_HANDLER();
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_ENDINIT)
    /* Re-set the newborn root to the top of this object tree. */
    JS_ASSERT(regs.sp - StackBase(fp) >= 1);
    lval = FETCH_OPND(-1);
    JS_ASSERT(JSVAL_IS_OBJECT(lval));
    cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = JSVAL_TO_OBJECT(lval);
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
BEGIN_CASE(JSOP_INITMETHOD)
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - StackBase(fp) >= 2);
    rval = FETCH_OPND(-1);

    /* Load the object being initialized into lval/obj. */
    lval = FETCH_OPND(-2);
    obj = JSVAL_TO_OBJECT(lval);
    JS_ASSERT(OBJ_IS_NATIVE(obj));
    JS_ASSERT(!OBJ_GET_CLASS(cx, obj)->reserveSlots);
    JS_ASSERT(!(obj->getClass()->flags & JSCLASS_SHARE_ALL_PROPERTIES));

    do {
        JSScope *scope;
        uint32 kshape;
        JSPropertyCache *cache;
        JSPropCacheEntry *entry;

        JS_LOCK_OBJ(cx, obj);
        scope = OBJ_SCOPE(obj);
        // FIXME: bug 513291 -- uncomment this assertion and remove the
        //        (!scope->owned()) => js_GetMutableScope code further
        //        below.
        // JS_ASSERT(scope->object == obj);
        JS_ASSERT(!scope->sealed());
        kshape = scope->shape;
        cache = &JS_PROPERTY_CACHE(cx);
        entry = &cache->table[PROPERTY_CACHE_HASH_PC(regs.pc, kshape)];
        PCMETER(cache->pctestentry = entry);
        PCMETER(cache->tests++);
        PCMETER(cache->initests++);

        if (entry->kpc == regs.pc &&
            entry->kshape == kshape &&
            PCVCAP_SHAPE(entry->vcap) == rt->protoHazardShape) {
            JS_ASSERT(PCVCAP_TAG(entry->vcap) == 0);

            PCMETER(cache->pchits++);
            PCMETER(cache->inipchits++);

            JS_ASSERT(PCVAL_IS_SPROP(entry->vword));
            sprop = PCVAL_TO_SPROP(entry->vword);
            JS_ASSERT(!(sprop->attrs & JSPROP_READONLY));

            /*
             * If this property has a non-stub setter, it must be __proto__,
             * __parent__, or another "shared prototype" built-in. Force a miss
             * to save code size here and let the standard code path take care
             * of business.
             */
            if (!SPROP_HAS_STUB_SETTER(sprop))
                goto do_initprop_miss;

            if (!scope->owned()) {
                scope = js_GetMutableScope(cx, obj);
                if (!scope) {
                    JS_UNLOCK_OBJ(cx, obj);
                    goto error;
                }
            }

            /*
             * Detect a repeated property name and force a miss to share the
             * strict warning code and consolidate all the complexity managed
             * by JSScope::addProperty.
             */
            if (sprop->parent != scope->lastProperty())
                goto do_initprop_miss;

            /*
             * Otherwise this entry must be for a direct property of obj, not a
             * proto-property, and there cannot have been any deletions of
             * prior properties.
             */
            JS_ASSERT(!scope->inDictionaryMode());
            JS_ASSERT_IF(scope->table, !scope->hasProperty(sprop));

            slot = sprop->slot;
            JS_ASSERT(slot == scope->freeslot);
            if (slot < STOBJ_NSLOTS(obj)) {
                ++scope->freeslot;
            } else {
                if (!js_AllocSlot(cx, obj, &slot)) {
                    JS_UNLOCK_SCOPE(cx, scope);
                    goto error;
                }
                JS_ASSERT(slot == sprop->slot);
            }

            JS_ASSERT(!scope->lastProperty() ||
                      scope->shape == scope->lastProperty()->shape);
            if (scope->table) {
                JSScopeProperty *sprop2 =
                    scope->addDataProperty(cx, sprop->id, slot, sprop->attrs);
                if (!sprop2) {
                    js_FreeSlot(cx, obj, slot);
                    JS_UNLOCK_SCOPE(cx, scope);
                    goto error;
                }
                JS_ASSERT(sprop2 == sprop);
            } else {
                JS_ASSERT(scope->owned());
                scope->extend(cx, sprop);
            }

            /*
             * No method change check here because here we are adding a new
             * property, not updating an existing slot's value that might
             * contain a method of a branded scope.
             */
            TRACE_2(SetPropHit, entry, sprop);
            LOCKED_OBJ_SET_SLOT(obj, slot, rval);
            JS_UNLOCK_SCOPE(cx, scope);
            break;
        }

      do_initprop_miss:
        PCMETER(cache->inipcmisses++);
        JS_UNLOCK_SCOPE(cx, scope);

        /* Get the immediate property name into id. */
        LOAD_ATOM(0);
        id = ATOM_TO_JSID(atom);

        /* Set the property named by obj[id] to rval. */
        if (!js_CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER,
                                   NULL, NULL)) {
            goto error;
        }

        uintN defineHow = (op == JSOP_INITMETHOD)
                          ? JSDNP_CACHE_RESULT | JSDNP_SET_METHOD
                          : JSDNP_CACHE_RESULT;
        if (!(JS_UNLIKELY(atom == cx->runtime->atomState.protoAtom)
              ? js_SetPropertyHelper(cx, obj, id, defineHow, &rval)
              : js_DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                        JSPROP_ENUMERATE, 0, 0, NULL,
                                        defineHow))) {
            goto error;
        }
    } while (0);

    /* Common tail for property cache hit and miss cases. */
    regs.sp--;
END_CASE(JSOP_INITPROP);

BEGIN_CASE(JSOP_INITELEM)
    /* Pop the element's value into rval. */
    JS_ASSERT(regs.sp - StackBase(fp) >= 3);
    rval = FETCH_OPND(-1);

    /* Find the object being initialized at top of stack. */
    lval = FETCH_OPND(-3);
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(lval));
    obj = JSVAL_TO_OBJECT(lval);

    /* Fetch id now that we have obj. */
    FETCH_ELEMENT_ID(obj, -2, id);

    /*
     * Check for property redeclaration strict warning (we may be in an object
     * initialiser, not an array initialiser).
     */
    if (!js_CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
        goto error;

    /*
     * If rval is a hole, do not call JSObject::defineProperty. In this case,
     * obj must be an array, so if the current op is the last element
     * initialiser, set the array length to one greater than id.
     */
    if (rval == JSVAL_HOLE) {
        JS_ASSERT(OBJ_IS_ARRAY(cx, obj));
        JS_ASSERT(JSID_IS_INT(id));
        JS_ASSERT(jsuint(JSID_TO_INT(id)) < JS_ARGS_LENGTH_MAX);
        if (js_GetOpcode(cx, script, regs.pc + JSOP_INITELEM_LENGTH) == JSOP_ENDINIT &&
            !js_SetLengthProperty(cx, obj, (jsuint) (JSID_TO_INT(id) + 1))) {
            goto error;
        }
    } else {
        if (!obj->defineProperty(cx, id, rval, NULL, NULL, JSPROP_ENUMERATE))
            goto error;
    }
    regs.sp -= 2;
END_CASE(JSOP_INITELEM)

#if JS_HAS_SHARP_VARS

BEGIN_CASE(JSOP_DEFSHARP)
    slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    lval = fp->slots[slot];
    if (!JSVAL_IS_PRIMITIVE(lval)) {
        obj = JSVAL_TO_OBJECT(lval);
    } else {
        JS_ASSERT(JSVAL_IS_VOID(lval));
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
        fp->slots[slot] = OBJECT_TO_JSVAL(obj);
    }
    i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    id = INT_TO_JSID(i);
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_PRIMITIVE(rval)) {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_DEF, numBuf);
        goto error;
    }
    if (!obj->defineProperty(cx, id, rval, NULL, NULL, JSPROP_ENUMERATE))
        goto error;
END_CASE(JSOP_DEFSHARP)

BEGIN_CASE(JSOP_USESHARP)
    slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    lval = fp->slots[slot];
    i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    if (JSVAL_IS_VOID(lval)) {
        rval = JSVAL_VOID;
    } else {
        obj = JSVAL_TO_OBJECT(fp->slots[slot]);
        id = INT_TO_JSID(i);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    }
    if (!JSVAL_IS_OBJECT(rval)) {
        char numBuf[12];

        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_USE, numBuf);
        goto error;
    }
    PUSH_OPND(rval);
END_CASE(JSOP_USESHARP)

BEGIN_CASE(JSOP_SHARPINIT)
    slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    vp = &fp->slots[slot];
    rval = vp[1];

    /*
     * We peek ahead safely here because empty initialisers get zero
     * JSOP_SHARPINIT ops, and non-empty ones get two: the first comes
     * immediately after JSOP_NEWINIT followed by one or more property
     * initialisers; and the second comes directly before JSOP_ENDINIT.
     */
    if (regs.pc[JSOP_SHARPINIT_LENGTH] != JSOP_ENDINIT) {
        rval = JSVAL_IS_VOID(rval) ? JSVAL_ONE : rval + 2;
    } else {
        JS_ASSERT(JSVAL_IS_INT(rval));
        rval -= 2;
        if (rval == JSVAL_ZERO)
            vp[0] = JSVAL_VOID;
    }
    vp[1] = rval;
END_CASE(JSOP_SHARPINIT)

#endif /* JS_HAS_SHARP_VARS */

BEGIN_CASE(JSOP_GOSUB)
    PUSH(JSVAL_FALSE);
    i = (regs.pc - script->main) + JSOP_GOSUB_LENGTH;
    PUSH(INT_TO_JSVAL(i));
    len = GET_JUMP_OFFSET(regs.pc);
END_VARLEN_CASE

BEGIN_CASE(JSOP_GOSUBX)
    PUSH(JSVAL_FALSE);
    i = (regs.pc - script->main) + JSOP_GOSUBX_LENGTH;
    len = GET_JUMPX_OFFSET(regs.pc);
    PUSH(INT_TO_JSVAL(i));
END_VARLEN_CASE

BEGIN_CASE(JSOP_RETSUB)
    /* Pop [exception or hole, retsub pc-index]. */
    rval = POP();
    lval = POP();
    JS_ASSERT(JSVAL_IS_BOOLEAN(lval));
    if (JSVAL_TO_BOOLEAN(lval)) {
        /*
         * Exception was pending during finally, throw it *before* we adjust
         * pc, because pc indexes into script->trynotes.  This turns out not to
         * be necessary, but it seems clearer.  And it points out a FIXME:
         * 350509, due to Igor Bukanov.
         */
        cx->throwing = JS_TRUE;
        cx->exception = rval;
        goto error;
    }
    JS_ASSERT(JSVAL_IS_INT(rval));
    len = JSVAL_TO_INT(rval);
    regs.pc = script->main;
END_VARLEN_CASE

BEGIN_CASE(JSOP_EXCEPTION)
    JS_ASSERT(cx->throwing);
    PUSH(cx->exception);
    cx->throwing = JS_FALSE;
    CHECK_BRANCH();
END_CASE(JSOP_EXCEPTION)

BEGIN_CASE(JSOP_FINALLY)
    CHECK_BRANCH();
END_CASE(JSOP_FINALLY)

BEGIN_CASE(JSOP_THROWING)
    JS_ASSERT(!cx->throwing);
    cx->throwing = JS_TRUE;
    cx->exception = POP_OPND();
END_CASE(JSOP_THROWING)

BEGIN_CASE(JSOP_THROW)
    JS_ASSERT(!cx->throwing);
    CHECK_BRANCH();
    cx->throwing = JS_TRUE;
    cx->exception = POP_OPND();
    /* let the code at error try to catch the exception. */
    goto error;

BEGIN_CASE(JSOP_SETLOCALPOP)
    /*
     * The stack must have a block with at least one local slot below the
     * exception object.
     */
    JS_ASSERT((size_t) (regs.sp - StackBase(fp)) >= 2);
    slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < script->nslots);
    fp->slots[slot] = POP_OPND();
END_CASE(JSOP_SETLOCALPOP)

BEGIN_CASE(JSOP_IFPRIMTOP)
    /*
     * If the top of stack is of primitive type, jump to our target. Otherwise
     * advance to the next opcode.
     */
    JS_ASSERT(regs.sp > StackBase(fp));
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_PRIMITIVE(rval)) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFPRIMTOP)

BEGIN_CASE(JSOP_PRIMTOP)
    JS_ASSERT(regs.sp > StackBase(fp));
    lval = FETCH_OPND(-1);
    i = GET_INT8(regs.pc);
    if (!JSVAL_IS_PRIMITIVE(lval)) {
        lval = FETCH_OPND(-2);
        js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO, -2, lval, NULL,
                             (i == JSTYPE_VOID) ? "primitive type" : JS_TYPE_STR(i));
        goto error;
    }
END_CASE(JSOP_PRIMTOP)

BEGIN_CASE(JSOP_OBJTOP)
    lval = FETCH_OPND(-1);
    if (JSVAL_IS_PRIMITIVE(lval)) {
        js_ReportValueError(cx, GET_UINT16(regs.pc), -1, lval, NULL);
        goto error;
    }
END_CASE(JSOP_OBJTOP)

BEGIN_CASE(JSOP_INSTANCEOF)
    rval = FETCH_OPND(-1);
    if (JSVAL_IS_PRIMITIVE(rval) ||
        !(obj = JSVAL_TO_OBJECT(rval))->map->ops->hasInstance) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                            -1, rval, NULL);
        goto error;
    }
    lval = FETCH_OPND(-2);
    cond = JS_FALSE;
    if (!obj->map->ops->hasInstance(cx, obj, lval, &cond))
        goto error;
    regs.sp--;
    STORE_OPND(-1, BOOLEAN_TO_JSVAL(cond));
END_CASE(JSOP_INSTANCEOF)

#if JS_HAS_DEBUGGER_KEYWORD
BEGIN_CASE(JSOP_DEBUGGER)
{
    JSTrapHandler handler = cx->debugHooks->debuggerHandler;
    if (handler) {
        switch (handler(cx, script, regs.pc, &rval, cx->debugHooks->debuggerHandlerData)) {
        case JSTRAP_ERROR:
            goto error;
        case JSTRAP_CONTINUE:
            break;
        case JSTRAP_RETURN:
            fp->rval = rval;
            ok = JS_TRUE;
            goto forced_return;
        case JSTRAP_THROW:
            cx->throwing = JS_TRUE;
            cx->exception = rval;
            goto error;
        default:;
        }
        CHECK_INTERRUPT_HANDLER();
    }
}
END_CASE(JSOP_DEBUGGER)
#endif /* JS_HAS_DEBUGGER_KEYWORD */

#if JS_HAS_XML_SUPPORT
BEGIN_CASE(JSOP_DEFXMLNS)
    rval = POP();
    if (!js_SetDefaultXMLNamespace(cx, rval))
        goto error;
END_CASE(JSOP_DEFXMLNS)

BEGIN_CASE(JSOP_ANYNAME)
    if (!js_GetAnyName(cx, &rval))
        goto error;
    PUSH_OPND(rval);
END_CASE(JSOP_ANYNAME)

BEGIN_CASE(JSOP_QNAMEPART)
    LOAD_ATOM(0);
    PUSH_OPND(ATOM_KEY(atom));
END_CASE(JSOP_QNAMEPART)

BEGIN_CASE(JSOP_QNAMECONST)
    LOAD_ATOM(0);
    rval = ATOM_KEY(atom);
    lval = FETCH_OPND(-1);
    obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_QNAMECONST)

BEGIN_CASE(JSOP_QNAME)
    rval = FETCH_OPND(-1);
    lval = FETCH_OPND(-2);
    obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp--;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_QNAME)

BEGIN_CASE(JSOP_TOATTRNAME)
    rval = FETCH_OPND(-1);
    if (!js_ToAttributeName(cx, &rval))
        goto error;
    STORE_OPND(-1, rval);
END_CASE(JSOP_TOATTRNAME)

BEGIN_CASE(JSOP_TOATTRVAL)
    rval = FETCH_OPND(-1);
    JS_ASSERT(JSVAL_IS_STRING(rval));
    str = js_EscapeAttributeValue(cx, JSVAL_TO_STRING(rval), JS_FALSE);
    if (!str)
        goto error;
    STORE_OPND(-1, STRING_TO_JSVAL(str));
END_CASE(JSOP_TOATTRVAL)

BEGIN_CASE(JSOP_ADDATTRNAME)
BEGIN_CASE(JSOP_ADDATTRVAL)
    rval = FETCH_OPND(-1);
    lval = FETCH_OPND(-2);
    str = JSVAL_TO_STRING(lval);
    str2 = JSVAL_TO_STRING(rval);
    str = js_AddAttributePart(cx, op == JSOP_ADDATTRNAME, str, str2);
    if (!str)
        goto error;
    regs.sp--;
    STORE_OPND(-1, STRING_TO_JSVAL(str));
END_CASE(JSOP_ADDATTRNAME)

BEGIN_CASE(JSOP_BINDXMLNAME)
    lval = FETCH_OPND(-1);
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
    PUSH_OPND(ID_TO_VALUE(id));
END_CASE(JSOP_BINDXMLNAME)

BEGIN_CASE(JSOP_SETXMLNAME)
    obj = JSVAL_TO_OBJECT(FETCH_OPND(-3));
    rval = FETCH_OPND(-1);
    FETCH_ELEMENT_ID(obj, -2, id);
    if (!obj->setProperty(cx, id, &rval))
        goto error;
    rval = FETCH_OPND(-1);
    regs.sp -= 2;
    STORE_OPND(-1, rval);
END_CASE(JSOP_SETXMLNAME)

BEGIN_CASE(JSOP_CALLXMLNAME)
BEGIN_CASE(JSOP_XMLNAME)
    lval = FETCH_OPND(-1);
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    if (!obj->getProperty(cx, id, &rval))
        goto error;
    STORE_OPND(-1, rval);
    if (op == JSOP_CALLXMLNAME)
        PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_XMLNAME)

BEGIN_CASE(JSOP_DESCENDANTS)
BEGIN_CASE(JSOP_DELDESC)
    FETCH_OBJECT(cx, -2, lval, obj);
    rval = FETCH_OPND(-1);
    if (!js_GetXMLDescendants(cx, obj, rval, &rval))
        goto error;

    if (op == JSOP_DELDESC) {
        regs.sp[-1] = rval;          /* set local root */
        if (!js_DeleteXMLListElements(cx, JSVAL_TO_OBJECT(rval)))
            goto error;
        rval = JSVAL_TRUE;      /* always succeed */
    }

    regs.sp--;
    STORE_OPND(-1, rval);
END_CASE(JSOP_DESCENDANTS)

BEGIN_CASE(JSOP_FILTER)
    /*
     * We push the hole value before jumping to [enditer] so we can detect the
     * first iteration and direct js_StepXMLListFilter to initialize filter's
     * state.
     */
    PUSH_OPND(JSVAL_HOLE);
    len = GET_JUMP_OFFSET(regs.pc);
    JS_ASSERT(len > 0);
END_VARLEN_CASE

BEGIN_CASE(JSOP_ENDFILTER)
    cond = (regs.sp[-1] != JSVAL_HOLE);
    if (cond) {
        /* Exit the "with" block left from the previous iteration. */
        js_LeaveWith(cx);
    }
    if (!js_StepXMLListFilter(cx, cond))
        goto error;
    if (regs.sp[-1] != JSVAL_NULL) {
        /*
         * Decrease sp after EnterWith returns as we use sp[-1] there to root
         * temporaries.
         */
        JS_ASSERT(VALUE_IS_XML(cx, regs.sp[-1]));
        if (!js_EnterWith(cx, -2))
            goto error;
        regs.sp--;
        len = GET_JUMP_OFFSET(regs.pc);
        JS_ASSERT(len < 0);
        BRANCH(len);
    }
    regs.sp--;
END_CASE(JSOP_ENDFILTER);

BEGIN_CASE(JSOP_TOXML)
    rval = FETCH_OPND(-1);
    obj = js_ValueToXMLObject(cx, rval);
    if (!obj)
        goto error;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_TOXML)

BEGIN_CASE(JSOP_TOXMLLIST)
    rval = FETCH_OPND(-1);
    obj = js_ValueToXMLListObject(cx, rval);
    if (!obj)
        goto error;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_TOXMLLIST)

BEGIN_CASE(JSOP_XMLTAGEXPR)
    rval = FETCH_OPND(-1);
    str = js_ValueToString(cx, rval);
    if (!str)
        goto error;
    STORE_OPND(-1, STRING_TO_JSVAL(str));
END_CASE(JSOP_XMLTAGEXPR)

BEGIN_CASE(JSOP_XMLELTEXPR)
    rval = FETCH_OPND(-1);
    if (VALUE_IS_XML(cx, rval)) {
        str = js_ValueToXMLString(cx, rval);
    } else {
        str = js_ValueToString(cx, rval);
        if (str)
            str = js_EscapeElementValue(cx, str);
    }
    if (!str)
        goto error;
    STORE_OPND(-1, STRING_TO_JSVAL(str));
END_CASE(JSOP_XMLELTEXPR)

BEGIN_CASE(JSOP_XMLOBJECT)
    LOAD_OBJECT(0);
    obj = js_CloneXMLObject(cx, obj);
    if (!obj)
        goto error;
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_XMLOBJECT)

BEGIN_CASE(JSOP_XMLCDATA)
    LOAD_ATOM(0);
    str = ATOM_TO_STRING(atom);
    obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_TEXT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_XMLCDATA)

BEGIN_CASE(JSOP_XMLCOMMENT)
    LOAD_ATOM(0);
    str = ATOM_TO_STRING(atom);
    obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_COMMENT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OPND(OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_XMLCOMMENT)

BEGIN_CASE(JSOP_XMLPI)
    LOAD_ATOM(0);
    str = ATOM_TO_STRING(atom);
    rval = FETCH_OPND(-1);
    str2 = JSVAL_TO_STRING(rval);
    obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_PROCESSING_INSTRUCTION, str, str2);
    if (!obj)
        goto error;
    STORE_OPND(-1, OBJECT_TO_JSVAL(obj));
END_CASE(JSOP_XMLPI)

BEGIN_CASE(JSOP_GETFUNNS)
    if (!js_GetFunctionNamespace(cx, &rval))
        goto error;
    PUSH_OPND(rval);
END_CASE(JSOP_GETFUNNS)
#endif /* JS_HAS_XML_SUPPORT */

BEGIN_CASE(JSOP_ENTERBLOCK)
    LOAD_OBJECT(0);
    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(obj));
    JS_ASSERT(StackBase(fp) + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= fp->slots + script->nslots);
    while (regs.sp < vp) {
        STORE_OPND(0, JSVAL_VOID);
        regs.sp++;
    }

#ifdef DEBUG
    JS_ASSERT(fp->blockChain == OBJ_GET_PARENT(cx, obj));

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    obj2 = fp->scopeChain;
    while ((clasp = OBJ_GET_CLASS(cx, obj2)) == &js_WithClass)
        obj2 = OBJ_GET_PARENT(cx, obj2);
    if (clasp == &js_BlockClass &&
        obj2->getPrivate() == fp) {
        JSObject *youngestProto = OBJ_GET_PROTO(cx, obj2);
        JS_ASSERT(!OBJ_IS_CLONED_BLOCK(youngestProto));
        parent = obj;
        while ((parent = OBJ_GET_PARENT(cx, parent)) != youngestProto)
            JS_ASSERT(parent);
    }
#endif

    fp->blockChain = obj;
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
BEGIN_CASE(JSOP_LEAVEBLOCK)
{
#ifdef DEBUG
    JS_ASSERT(OBJ_GET_CLASS(cx, fp->blockChain) == &js_BlockClass);
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, fp->blockChain);

    JS_ASSERT(blockDepth <= StackDepth(script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    obj = fp->scopeChain;
    if (OBJ_GET_PROTO(cx, obj) == fp->blockChain) {
        JS_ASSERT (OBJ_GET_CLASS(cx, obj) == &js_BlockClass);
        if (!js_PutBlockObject(cx, JS_TRUE))
            goto error;
    }

    /* Pop the block chain, too.  */
    fp->blockChain = OBJ_GET_PARENT(cx, fp->blockChain);

    /* Move the result of the expression to the new topmost stack slot. */
    if (op == JSOP_LEAVEBLOCKEXPR)
        rval = FETCH_OPND(-1);
    regs.sp -= GET_UINT16(regs.pc);
    if (op == JSOP_LEAVEBLOCKEXPR) {
        JS_ASSERT(StackBase(fp) + blockDepth == regs.sp - 1);
        STORE_OPND(-1, rval);
    } else {
        JS_ASSERT(StackBase(fp) + blockDepth == regs.sp);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

BEGIN_CASE(JSOP_CALLBUILTIN)
#ifdef JS_TRACER
    obj = js_GetBuiltinFunction(cx, GET_INDEX(regs.pc));
    if (!obj)
        goto error;
    rval = FETCH_OPND(-1);
    PUSH_OPND(rval);
    STORE_OPND(-2, OBJECT_TO_JSVAL(obj));
#else
    goto bad_opcode;  /* This is an imacro-only opcode. */
#endif
END_CASE(JSOP_CALLBUILTIN)

#if JS_HAS_GENERATORS
BEGIN_CASE(JSOP_GENERATOR)
    ASSERT_NOT_THROWING(cx);
    regs.pc += JSOP_GENERATOR_LENGTH;
    obj = js_NewGenerator(cx, fp);
    if (!obj)
        goto error;
    JS_ASSERT(!fp->callobj && !fp->argsobj);
    fp->rval = OBJECT_TO_JSVAL(obj);
    ok = JS_TRUE;
    if (inlineCallCount != 0)
        goto inline_return;
    goto exit;

BEGIN_CASE(JSOP_YIELD)
    ASSERT_NOT_THROWING(cx);
    if (FRAME_TO_GENERATOR(fp)->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD,
                            JSDVG_SEARCH_STACK, fp->argv[-2], NULL);
        goto error;
    }
    fp->rval = FETCH_OPND(-1);
    fp->flags |= JSFRAME_YIELDING;
    regs.pc += JSOP_YIELD_LENGTH;
    ok = JS_TRUE;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
    slot = GET_UINT16(regs.pc);
    JS_ASSERT(script->nfixed <= slot);
    JS_ASSERT(slot < script->nslots);
    lval = fp->slots[slot];
    obj  = JSVAL_TO_OBJECT(lval);
    rval = FETCH_OPND(-1);
    if (!js_ArrayCompPush(cx, obj, rval))
        goto error;
    regs.sp--;
END_CASE(JSOP_ARRAYPUSH)
#endif /* JS_HAS_GENERATORS */

#if JS_THREADED_INTERP
  L_JSOP_BACKPATCH:
  L_JSOP_BACKPATCH_POP:

# if !JS_HAS_GENERATORS
  L_JSOP_GENERATOR:
  L_JSOP_YIELD:
  L_JSOP_ARRAYPUSH:
# endif

# if !JS_HAS_SHARP_VARS
  L_JSOP_DEFSHARP:
  L_JSOP_USESHARP:
  L_JSOP_SHARPINIT:
# endif

# if !JS_HAS_DESTRUCTURING
  L_JSOP_ENUMCONSTELEM:
# endif

# if !JS_HAS_XML_SUPPORT
  L_JSOP_CALLXMLNAME:
  L_JSOP_STARTXMLEXPR:
  L_JSOP_STARTXML:
  L_JSOP_DELDESC:
  L_JSOP_GETFUNNS:
  L_JSOP_XMLPI:
  L_JSOP_XMLCOMMENT:
  L_JSOP_XMLCDATA:
  L_JSOP_XMLOBJECT:
  L_JSOP_XMLELTEXPR:
  L_JSOP_XMLTAGEXPR:
  L_JSOP_TOXMLLIST:
  L_JSOP_TOXML:
  L_JSOP_ENDFILTER:
  L_JSOP_FILTER:
  L_JSOP_DESCENDANTS:
  L_JSOP_XMLNAME:
  L_JSOP_SETXMLNAME:
  L_JSOP_BINDXMLNAME:
  L_JSOP_ADDATTRVAL:
  L_JSOP_ADDATTRNAME:
  L_JSOP_TOATTRVAL:
  L_JSOP_TOATTRNAME:
  L_JSOP_QNAME:
  L_JSOP_QNAMECONST:
  L_JSOP_QNAMEPART:
  L_JSOP_ANYNAME:
  L_JSOP_DEFXMLNS:
# endif
#endif /* !JS_THREADED_INTERP */
