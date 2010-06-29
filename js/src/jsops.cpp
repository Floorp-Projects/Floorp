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
        JSInterruptHook hook = cx->debugHooks->interruptHook;
        if (hook) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "interrupt hook");
#endif
            Value rval;
            switch (hook(cx, script, regs.pc, Jsvalify(&rval),
                         cx->debugHooks->interruptHookData)) {
              case JSTRAP_ERROR:
                goto error;
              case JSTRAP_CONTINUE:
                break;
              case JSTRAP_RETURN:
                fp->rval = rval;
                interpReturnOK = JS_TRUE;
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
            JS_ASSERT_IF(cx->throwing, status == ARECORD_ERROR);
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
            ExitTraceVisState(cx, R_ABORT);
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
BEGIN_CASE(JSOP_DEFUPVAR)
END_CASE(JSOP_DEFUPVAR)

BEGIN_CASE(JSOP_PUSH)
    PUSH_UNDEFINED();
END_CASE(JSOP_PUSH)

BEGIN_CASE(JSOP_POP)
    regs.sp--;
END_CASE(JSOP_POP)

BEGIN_CASE(JSOP_POPN)
{
    regs.sp -= GET_UINT16(regs.pc);
#ifdef DEBUG
    JS_ASSERT(fp->base() <= regs.sp);
    JSObject *obj = fp->blockChain;
    JS_ASSERT_IF(obj,
                 OBJ_BLOCK_DEPTH(cx, obj) + OBJ_BLOCK_COUNT(cx, obj)
                 <= (size_t) (regs.sp - fp->base()));
    for (obj = fp->scopeChainObj(); obj; obj = obj->getParent()) {
        Class *clasp = obj->getClass();
        if (clasp != &js_BlockClass && clasp != &js_WithClass)
            continue;
        if (obj->getPrivate() != js_FloatingFrameIfGenerator(cx, fp))
            break;
        JS_ASSERT(fp->base() + OBJ_BLOCK_DEPTH(cx, obj)
                             + ((clasp == &js_BlockClass)
                                ? OBJ_BLOCK_COUNT(cx, obj)
                                : 1)
                  <= regs.sp);
    }
#endif
}
END_CASE(JSOP_POPN)

BEGIN_CASE(JSOP_SETRVAL)
BEGIN_CASE(JSOP_POPV)
    ASSERT_NOT_THROWING(cx);
    POP_COPY_TO(fp->rval);
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
    regs.sp[-1] = fp->scopeChain;
END_CASE(JSOP_ENTERWITH)

BEGIN_CASE(JSOP_LEAVEWITH)
    JS_ASSERT(&regs.sp[-1].asNonFunObj() == fp->scopeChainObj());
    regs.sp--;
    js_LeaveWith(cx);
END_CASE(JSOP_LEAVEWITH)

BEGIN_CASE(JSOP_RETURN)
    POP_COPY_TO(fp->rval);
    /* FALL THROUGH */

BEGIN_CASE(JSOP_RETRVAL)    /* fp->rval already set */
BEGIN_CASE(JSOP_STOP)
{
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
        JS_ASSERT((uintN)(regs.sp - fp->slots()) <= script->nslots);
        regs.pc = fp->imacpc + js_CodeSpec[*fp->imacpc].length;
        fp->imacpc = NULL;
        atoms = script->atomMap.vector;
        op = JSOp(*regs.pc);
        DO_OP();
    }

    JS_ASSERT(regs.sp == fp->base());
    if ((fp->flags & JSFRAME_CONSTRUCTING) && fp->rval.isPrimitive())
        fp->rval = fp->thisv;

    interpReturnOK = true;
    if (inlineCallCount)
  inline_return:
    {
        JS_ASSERT(!fp->blockChain);
        JS_ASSERT(!js_IsActiveWithOrBlock(cx, fp->scopeChainObj(), 0));

        if (JS_LIKELY(script->staticLevel < JS_DISPLAY_SIZE))
            cx->display[script->staticLevel] = fp->displaySave;

        void *hookData = fp->hookData;
        if (JS_UNLIKELY(hookData != NULL)) {
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                hook(cx, fp, JS_FALSE, &interpReturnOK, hookData);
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

        DTrace::exitJSFun(cx, fp, fp->fun, fp->rval);

        /* Restore context version only if callee hasn't set version. */
        if (JS_LIKELY(cx->version == currentVersion)) {
            currentVersion = fp->callerVersion;
            if (currentVersion != cx->version)
                js_SetVersion(cx, currentVersion);
        }

        /*
         * If inline-constructing, replace primitive rval with the new object
         * passed in via |this|, and instrument this constructor invocation.
         */
        if (fp->flags & JSFRAME_CONSTRUCTING) {
            if (fp->rval.isPrimitive())
                fp->rval = fp->thisv;
            JS_RUNTIME_METER(cx->runtime, constructs);
        }

        JSStackFrame *down = fp->down;
        bool recursive = fp->script == down->script;

        /* Pop the frame. */
        cx->stack().popInlineFrame(cx, fp, down);

        /* Propagate return value before fp is lost. */
        regs.sp[-1] = fp->rval;

        /* Sync interpreter registers. */
        fp = cx->fp;
        script = fp->script;
        atoms = FrameAtomBase(cx, fp);

        /* Resume execution in the calling frame. */
        inlineCallCount--;
        if (JS_LIKELY(interpReturnOK)) {
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
    interpReturnOK = true;
    goto exit;
}

BEGIN_CASE(JSOP_DEFAULT)
    regs.sp--;
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTO)
{
    len = GET_JUMP_OFFSET(regs.pc);
    BRANCH(len);
}
END_CASE(JSOP_GOTO)

BEGIN_CASE(JSOP_IFEQ)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFEQ)

BEGIN_CASE(JSOP_IFNE)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond != false) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFNE)

BEGIN_CASE(JSOP_OR)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == true) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_OR)

BEGIN_CASE(JSOP_AND)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == false) {
        len = GET_JUMP_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_AND)

BEGIN_CASE(JSOP_DEFAULTX)
    regs.sp--;
    /* FALL THROUGH */
BEGIN_CASE(JSOP_GOTOX)
{
    len = GET_JUMPX_OFFSET(regs.pc);
    BRANCH(len);
}
END_CASE(JSOP_GOTOX);

BEGIN_CASE(JSOP_IFEQX)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond == false) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFEQX)

BEGIN_CASE(JSOP_IFNEX)
{
    bool cond;
    Value *_;
    POP_BOOLEAN(cx, _, cond);
    if (cond != false) {
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_IFNEX)

BEGIN_CASE(JSOP_ORX)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == true) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
}
END_CASE(JSOP_ORX)

BEGIN_CASE(JSOP_ANDX)
{
    bool cond;
    Value *vp;
    POP_BOOLEAN(cx, vp, cond);
    if (cond == JS_FALSE) {
        len = GET_JUMPX_OFFSET(regs.pc);
        PUSH_COPY(*vp);
        DO_NEXT_OP(len);
    }
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
        const Value &idval_ = regs.sp[n];                                     \
        int32_t i_;                                                           \
        if (ValueFitsInInt32(idval_, &i_) && INT_FITS_IN_JSID(i_)) {          \
            id = INT_TO_JSID(i_);                                             \
        } else {                                                              \
            if (!js_InternNonIntElementId(cx, obj, idval_, &id, &regs.sp[n])) \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define TRY_BRANCH_AFTER_COND(cond,spdec)                                     \
    JS_BEGIN_MACRO                                                            \
        JS_ASSERT(js_CodeSpec[op].length == 1);                               \
        uintN diff_ = (uintN) regs.pc[1] - (uintN) JSOP_IFEQ;                 \
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
{
    const Value &rref = regs.sp[-1];
    if (!rref.isObject()) {
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, rref, NULL);
        goto error;
    }
    JSObject *obj = &rref.asObject();
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    JSObject *obj2;
    JSProperty *prop;
    if (!obj->lookupProperty(cx, id, &obj2, &prop))
        goto error;
    bool cond = prop != NULL;
    if (prop)
        obj2->dropProperty(cx, prop);
    TRY_BRANCH_AFTER_COND(cond, 2);
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_IN)

BEGIN_CASE(JSOP_ITER)
{
    JS_ASSERT(regs.sp > fp->base());
    uintN flags = regs.pc[1];
    if (!js_ValueToIterator(cx, flags, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    JS_ASSERT(!regs.sp[-1].isPrimitive());
}
END_CASE(JSOP_ITER)

BEGIN_CASE(JSOP_MOREITER)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    bool cond;
    if (!IteratorMore(cx, &regs.sp[-2].asObject(), &cond, &regs.sp[-1]))
        goto error;
    CHECK_INTERRUPT_HANDLER();
    TRY_BRANCH_AFTER_COND(cond, 1);
    JS_ASSERT(regs.pc[1] == JSOP_IFNEX);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_MOREITER)

BEGIN_CASE(JSOP_ENDITER)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    bool ok = !!js_CloseIterator(cx, regs.sp[-1]);
    regs.sp--;
    if (!ok)
        goto error;
}
END_CASE(JSOP_ENDITER)

BEGIN_CASE(JSOP_FORARG)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    uintN slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].asObject(), &fp->argv[slot]))
        goto error;
}
END_CASE(JSOP_FORARG)

BEGIN_CASE(JSOP_FORLOCAL)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    uintN slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    JS_ASSERT(regs.sp[-1].isObject());
    if (!IteratorNext(cx, &regs.sp[-1].asObject(), &fp->slots()[slot]))
        goto error;
}
END_CASE(JSOP_FORLOCAL)

BEGIN_CASE(JSOP_FORNAME)
{
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;
    if (prop)
        obj2->dropProperty(cx, prop);
    {
        AutoValueRooter tvr(cx);
        JS_ASSERT(regs.sp[-1].isObject());
        if (!IteratorNext(cx, &regs.sp[-1].asObject(), tvr.addr()))
            goto error;
        if (!obj->setProperty(cx, id, tvr.addr()))
            goto error;
    }
}
END_CASE(JSOP_FORNAME)

BEGIN_CASE(JSOP_FORPROP)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj;
    FETCH_OBJECT(cx, -1, obj);
    {
        AutoValueRooter tvr(cx);
        JS_ASSERT(regs.sp[-2].isObject());
        if (!IteratorNext(cx, &regs.sp[-2].asObject(), tvr.addr()))
            goto error;
        if (!obj->setProperty(cx, id, tvr.addr()))
            goto error;
    }
    regs.sp--;
}
END_CASE(JSOP_FORPROP)

BEGIN_CASE(JSOP_FORELEM)
    /*
     * JSOP_FORELEM simply dups the property identifier at top of stack and
     * lets the subsequent JSOP_ENUMELEM opcode sequence handle the left-hand
     * side expression evaluation and assignment. This opcode exists solely to
     * help the decompiler.
     */
    JS_ASSERT(regs.sp - 1 >= fp->base());
    JS_ASSERT(regs.sp[-1].isObject());
    PUSH_NULL();
    if (!IteratorNext(cx, &regs.sp[-2].asObject(), &regs.sp[-1]))
        goto error;
END_CASE(JSOP_FORELEM)

BEGIN_CASE(JSOP_DUP)
{
    JS_ASSERT(regs.sp > fp->base());
    const Value &rref = regs.sp[-1];
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP)

BEGIN_CASE(JSOP_DUP2)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    const Value &lref = regs.sp[-2];
    const Value &rref = regs.sp[-1];
    PUSH_COPY(lref);
    PUSH_COPY(rref);
}
END_CASE(JSOP_DUP2)

BEGIN_CASE(JSOP_SWAP)
{
    JS_ASSERT(regs.sp - 2 >= fp->base());
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    lref.swap(rref);
}
END_CASE(JSOP_SWAP)

BEGIN_CASE(JSOP_PICK)
{
    jsint i = regs.pc[1];
    JS_ASSERT(regs.sp - (i+1) >= fp->base());
    Value lval = regs.sp[-(i+1)];
    memmove(regs.sp - (i+1), regs.sp - i, sizeof(Value)*i);
    regs.sp[-1] = lval;
}
END_CASE(JSOP_PICK)

#define NATIVE_GET(cx,obj,pobj,sprop,getHow,vp)                               \
    JS_BEGIN_MACRO                                                            \
        if (sprop->hasDefaultGetter()) {                                      \
            /* Fast path for Object instance properties. */                   \
            JS_ASSERT((sprop)->slot != SPROP_INVALID_SLOT ||                  \
                      !sprop->hasDefaultSetter());                            \
            if (((sprop)->slot != SPROP_INVALID_SLOT))                        \
                *(vp) = (pobj)->lockedGetSlot((sprop)->slot);                 \
            else                                                              \
                (vp)->setUndefined();                                         \
        } else {                                                              \
            if (!js_NativeGet(cx, obj, pobj, sprop, getHow, vp))              \
                goto error;                                                   \
        }                                                                     \
    JS_END_MACRO

#define NATIVE_SET(cx,obj,sprop,entry,vp)                                     \
    JS_BEGIN_MACRO                                                            \
        TRACE_2(SetPropHit, entry, sprop);                                    \
        if (sprop->hasDefaultSetter() &&                                      \
            (sprop)->slot != SPROP_INVALID_SLOT &&                            \
            !(obj)->scope()->brandedOrHasMethodBarrier()) {                   \
            /* Fast path for, e.g., plain Object instance properties. */      \
            (obj)->lockedSetSlot((sprop)->slot, *vp);                         \
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
            {                                                                 \
                Value *newsp = regs.sp - ((spdec) - 1);                       \
                newsp[-1] = regs.sp[-1];                                      \
                regs.sp = newsp;                                              \
            }                                                                 \
          END_CASE(OP)

BEGIN_CASE(JSOP_SETCONST)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSObject *obj = fp->varobj(cx);
    const Value &ref = regs.sp[-1];
    if (!obj->defineProperty(cx, ATOM_TO_JSID(atom), ref,
                             PropertyStub, PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
}
END_SET_CASE(JSOP_SETCONST);

#if JS_HAS_DESTRUCTURING
BEGIN_CASE(JSOP_ENUMCONSTELEM)
{
    const Value &ref = regs.sp[-3];
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->defineProperty(cx, id, ref,
                             PropertyStub, PropertyStub,
                             JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        goto error;
    }
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMCONSTELEM)
#endif

BEGIN_CASE(JSOP_BINDGNAME)
    PUSH_NONFUNOBJ(*fp->scopeChainObj()->getGlobal());
END_CASE(JSOP_BINDGNAME)

BEGIN_CASE(JSOP_BINDNAME)
{
    JSObject *obj;
    do {
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
        obj = fp->scopeChainObj();
        if (!obj->getParent())
            break;

        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
        if (!atom) {
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            break;
        }

        jsid id = ATOM_TO_JSID(atom);
        obj = js_FindIdentifierBase(cx, fp->scopeChainObj(), id);
        if (!obj)
            goto error;
    } while (0);
    PUSH_NONFUNOBJ(*obj);
}
END_CASE(JSOP_BINDNAME)

BEGIN_CASE(JSOP_IMACOP)
    JS_ASSERT(JS_UPTRDIFF(fp->imacpc, script->code) < script->length);
    op = JSOp(*fp->imacpc);
    DO_OP();

#define BITWISE_OP(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ValueToECMAInt32(cx, regs.sp[-2], &i))                           \
            goto error;                                                       \
        if (!ValueToECMAInt32(cx, regs.sp[-1], &j))                           \
            goto error;                                                       \
        i = i OP j;                                                           \
        regs.sp--;                                                            \
        regs.sp[-1].setInt32(i);                                              \
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

#undef BITWISE_OP

/*
 * NB: These macros can't use JS_BEGIN_MACRO/JS_END_MACRO around their bodies
 * because they begin if/else chains, so callers must not put semicolons after
 * the call expressions!
 */
#if JS_HAS_XML_SUPPORT
#define XML_EQUALITY_OP(OP)                                                   \
    if ((lval.isNonFunObj() && lval.asObject().isXML()) ||                    \
        (rval.isNonFunObj() && rval.asObject().isXML())) {                    \
        if (!js_TestXMLEquality(cx, lval, rval, &cond))                       \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else

#define EXTENDED_EQUALITY_OP(OP)                                              \
    if (((clasp = l->getClass())->flags & JSCLASS_IS_EXTENDED) &&             \
        ((ExtendedClass *)clasp)->equality) {                                 \
        if (!((ExtendedClass *)clasp)->equality(cx, l, &rval, &cond))         \
            goto error;                                                       \
        cond = cond OP JS_TRUE;                                               \
    } else
#else
#define XML_EQUALITY_OP(OP)             /* nothing */
#define EXTENDED_EQUALITY_OP(OP)        /* nothing */
#endif

#define EQUALITY_OP(OP, IFNAN)                                                \
    JS_BEGIN_MACRO                                                            \
        Class *clasp;                                                         \
        JSBool cond;                                                          \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        XML_EQUALITY_OP(OP)                                                   \
        if (SamePrimitiveTypeOrBothObjects(lval, rval)) {                     \
            if (lval.isString()) {                                            \
                JSString *l = lval.asString(), *r = rval.asString();          \
                cond = js_EqualStrings(l, r) OP JS_TRUE;                      \
            } else if (lval.isDouble()) {                                     \
                double l = lval.asDouble(), r = rval.asDouble();              \
                cond = JSDOUBLE_COMPARE(l, OP, r, IFNAN);                     \
            } else if (lval.isObject()) {                                     \
                JSObject *l = &lval.asObject(), *r = &rval.asObject();        \
                EXTENDED_EQUALITY_OP(OP)                                      \
                cond = l OP r;                                                \
            } else {                                                          \
                cond = lval.asRawUint32() OP rval.asRawUint32();              \
            }                                                                 \
        } else {                                                              \
            if (lval.isNullOrUndefined()) {                                   \
                cond = rval.isNullOrUndefined() OP true;                      \
            } else if (rval.isNullOrUndefined()) {                            \
                cond = true OP false;                                         \
            } else {                                                          \
                if (lval.isObject())                                          \
                    DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);                 \
                if (rval.isObject())                                          \
                    DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);                 \
                if (lval.isString() && rval.isString()) {                     \
                    JSString *l = lval.asString(), *r = rval.asString();      \
                    cond = js_EqualStrings(l, r) OP JS_TRUE;                  \
                } else {                                                      \
                    double l, r;                                              \
                    if (!ValueToNumber(cx, lval, &l) ||                       \
                        !ValueToNumber(cx, rval, &r)) {                       \
                        goto error;                                           \
                    }                                                         \
                    cond = JSDOUBLE_COMPARE(l, OP, r, IFNAN);                 \
                }                                                             \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        regs.sp[-1].setBoolean(cond);                                         \
    JS_END_MACRO

BEGIN_CASE(JSOP_EQ)
    EQUALITY_OP(==, false);
END_CASE(JSOP_EQ)

BEGIN_CASE(JSOP_NE)
    EQUALITY_OP(!=, true);
END_CASE(JSOP_NE)

#undef EQUALITY_OP
#undef XML_EQUALITY_OP
#undef EXTENDED_EQUALITY_OP

#define STRICT_EQUALITY_OP(OP, COND)                                          \
    JS_BEGIN_MACRO                                                            \
        const Value &rref = regs.sp[-1];                                      \
        const Value &lref = regs.sp[-2];                                      \
        COND = StrictlyEqual(cx, lref, rref) OP true;                         \
        regs.sp--;                                                            \
    JS_END_MACRO

BEGIN_CASE(JSOP_STRICTEQ)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTEQ)

BEGIN_CASE(JSOP_STRICTNE)
{
    bool cond;
    STRICT_EQUALITY_OP(!=, cond);
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_STRICTNE)

BEGIN_CASE(JSOP_CASE)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    if (cond) {
        regs.sp--;
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_CASE)

BEGIN_CASE(JSOP_CASEX)
{
    bool cond;
    STRICT_EQUALITY_OP(==, cond);
    if (cond) {
        regs.sp--;
        len = GET_JUMPX_OFFSET(regs.pc);
        BRANCH(len);
    }
}
END_CASE(JSOP_CASEX)

#undef STRICT_EQUALITY_OP

#define RELATIONAL_OP(OP)                                                     \
    JS_BEGIN_MACRO                                                            \
        Value rval = regs.sp[-1];                                             \
        Value lval = regs.sp[-2];                                             \
        bool cond;                                                            \
        /* Optimize for two int-tagged operands (typical loop control). */    \
        if (lval.isInt32() && rval.isInt32()) {                               \
            cond = lval.asInt32() OP rval.asInt32();                          \
        } else {                                                              \
            if (lval.isObject())                                              \
                DEFAULT_VALUE(cx, -2, JSTYPE_NUMBER, lval);                   \
            if (rval.isObject())                                              \
                DEFAULT_VALUE(cx, -1, JSTYPE_NUMBER, rval);                   \
            if (lval.isString() && rval.isString()) {                         \
                JSString *l = lval.asString(), *r = rval.asString();          \
                cond = js_CompareStrings(l, r) OP 0;                          \
            } else {                                                          \
                double l, r;                                                  \
                if (!ValueToNumber(cx, lval, &l) ||                           \
                    !ValueToNumber(cx, rval, &r)) {                           \
                    goto error;                                               \
                }                                                             \
                cond = JSDOUBLE_COMPARE(l, OP, r, false);                     \
            }                                                                 \
        }                                                                     \
        TRY_BRANCH_AFTER_COND(cond, 2);                                       \
        regs.sp--;                                                            \
        regs.sp[-1].setBoolean(cond);                                         \
    JS_END_MACRO

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

#undef RELATIONAL_OP

#define SIGNED_SHIFT_OP(OP)                                                   \
    JS_BEGIN_MACRO                                                            \
        int32_t i, j;                                                         \
        if (!ValueToECMAInt32(cx, regs.sp[-2], &i))                           \
            goto error;                                                       \
        if (!ValueToECMAInt32(cx, regs.sp[-1], &j))                           \
            goto error;                                                       \
        i = i OP (j & 31);                                                    \
        regs.sp--;                                                            \
        regs.sp[-1].setInt32(i);                                              \
    JS_END_MACRO

BEGIN_CASE(JSOP_LSH)
    SIGNED_SHIFT_OP(<<);
END_CASE(JSOP_LSH)

BEGIN_CASE(JSOP_RSH)
    SIGNED_SHIFT_OP(>>);
END_CASE(JSOP_RSH)

#undef SIGNED_SHIFT_OP

BEGIN_CASE(JSOP_URSH)
{
    uint32_t u;
    if (!ValueToECMAUint32(cx, regs.sp[-2], &u))
        goto error;
    int32_t j;
    if (!ValueToECMAInt32(cx, regs.sp[-1], &j))
        goto error;

    u >>= (j & 31);

    regs.sp--;
	regs.sp[-1].setNumber(uint32(u));
}
END_CASE(JSOP_URSH)

BEGIN_CASE(JSOP_ADD)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];

    if (lval.isInt32() && rval.isInt32()) {
        int32_t l = lval.asInt32(), r = rval.asInt32();
        int32_t sum = l + r;
        regs.sp--;
        if (JS_UNLIKELY(bool((l ^ sum) & (r ^ sum) & 0x80000000)))
            regs.sp[-1].setDouble(double(l) + double(r));
        else
            regs.sp[-1].setInt32(sum);
    } else
#if JS_HAS_XML_SUPPORT
    if (lval.isNonFunObj() && lval.asObject().isXML() &&
        rval.isNonFunObj() && rval.asObject().isXML()) {
        if (!js_ConcatenateXML(cx, &lval.asObject(), &rval.asObject(), &rval))
            goto error;
        regs.sp--;
        regs.sp[-1] = rval;
    } else
#endif
    {
        if (lval.isObject()) 
            DEFAULT_VALUE(cx, -2, JSTYPE_VOID, lval);
        if (rval.isObject())
            DEFAULT_VALUE(cx, -1, JSTYPE_VOID, rval);
        bool lIsString, rIsString;
        if ((lIsString = lval.isString()) | (rIsString = rval.isString())) {
            JSString *lstr, *rstr;
            if (lIsString) {
                lstr = lval.asString();
            } else {
                lstr = js_ValueToString(cx, lval);
                if (!lstr)
                    goto error;
                regs.sp[-2].setString(lstr);
            }
            if (rIsString) {
                rstr = rval.asString();
            } else {
                rstr = js_ValueToString(cx, rval);
                if (!rstr)
                    goto error;
                regs.sp[-1].setString(rstr);
            }
            JSString *str = js_ConcatStrings(cx, lstr, rstr);
            if (!str)
                goto error;
            regs.sp--;
            regs.sp[-1].setString(str);
        } else {
            double l, r;
            if (!ValueToNumber(cx, lval, &l) || !ValueToNumber(cx, rval, &r))
                goto error;
            l += r;
            regs.sp--;
            regs.sp[-1].setNumber(l);
        }
    }
}
END_CASE(JSOP_ADD)

BEGIN_CASE(JSOP_OBJTOSTR)
{
    const Value &ref = regs.sp[-1];
    if (ref.isObject()) {
        JSString *str = js_ValueToString(cx, ref);
        if (!str)
            goto error;
        regs.sp[-1].setString(str);
    }
}
END_CASE(JSOP_OBJTOSTR)

BEGIN_CASE(JSOP_CONCATN)
{
    JSCharBuffer buf(cx);
    uintN argc = GET_ARGC(regs.pc);
    for (Value *vp = regs.sp - argc; vp < regs.sp; vp++) {
        JS_ASSERT(vp->isPrimitive());
        if (!js_ValueToCharBuffer(cx, *vp, buf))
            goto error;
    }
    JSString *str = js_NewStringFromCharBuffer(cx, buf);
    if (!str)
        goto error;
    regs.sp -= argc - 1;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_CONCATN)

#define BINARY_OP(OP)                                                         \
    JS_BEGIN_MACRO                                                            \
        double d1, d2;                                                        \
        if (!ValueToNumber(cx, regs.sp[-2], &d1) ||                           \
            !ValueToNumber(cx, regs.sp[-1], &d2)) {                           \
            goto error;                                                       \
        }                                                                     \
        double d = d1 OP d2;                                                  \
        regs.sp--;                                                            \
        regs.sp[-1].setNumber(d);                                             \
    JS_END_MACRO

BEGIN_CASE(JSOP_SUB)
    BINARY_OP(-);
END_CASE(JSOP_SUB)

BEGIN_CASE(JSOP_MUL)
    BINARY_OP(*);
END_CASE(JSOP_MUL)

#undef BINARY_OP

BEGIN_CASE(JSOP_DIV)
{
    double d1, d2;
    if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
        !ValueToNumber(cx, regs.sp[-1], &d2)) {
        goto error;
    }
    regs.sp--;
    if (d2 == 0) {
        const Value *vp;
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (JSDOUBLE_IS_NaN(d2))
            vp = &rt->NaNValue;
        else
#endif
        if (d1 == 0 || JSDOUBLE_IS_NaN(d1))
            vp = &rt->NaNValue;
        else if (JSDOUBLE_IS_NEG(d1) != JSDOUBLE_IS_NEG(d2))
            vp = &rt->negativeInfinityValue;
        else
            vp = &rt->positiveInfinityValue;
        regs.sp[-1] = *vp;
    } else {
        d1 /= d2;
        regs.sp[-1].setNumber(d1);
    }
}
END_CASE(JSOP_DIV)

BEGIN_CASE(JSOP_MOD)
{
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    int32_t l, r;
    if (lref.isInt32() && rref.isInt32() &&
        (l = lref.asInt32()) >= 0 && (r = rref.asInt32()) > 0) {
        int32_t mod = l % r;
        regs.sp--;
        regs.sp[-1].setInt32(mod);
    } else {
        double d1, d2;
        if (!ValueToNumber(cx, regs.sp[-2], &d1) ||
            !ValueToNumber(cx, regs.sp[-1], &d2)) {
            goto error;
        }
        regs.sp--;
        if (d2 == 0) {
            regs.sp[-1].setDouble(js_NaN);
        } else {
            d1 = js_fmod(d1, d2);
            regs.sp[-1].setDouble(d1);
        }
    }
}
END_CASE(JSOP_MOD)

BEGIN_CASE(JSOP_NOT)
{
    Value *_;
    bool cond;
    POP_BOOLEAN(cx, _, cond);
    PUSH_BOOLEAN(!cond);
}
END_CASE(JSOP_NOT)

BEGIN_CASE(JSOP_BITNOT)
{
    int32_t i;
    if (!ValueToECMAInt32(cx, regs.sp[-1], &i))
        goto error;
    i = ~i;
    regs.sp[-1].setInt32(i);
}
END_CASE(JSOP_BITNOT)

BEGIN_CASE(JSOP_NEG)
{
    /*
     * When the operand is int jsval, INT32_FITS_IN_JSVAL(i) implies
     * INT32_FITS_IN_JSVAL(-i) unless i is 0 or INT32_MIN when the
     * results, -0.0 or INT32_MAX + 1, are jsdouble values.
     */
    const Value &ref = regs.sp[-1];
    int32_t i;
    if (ref.isInt32() && (i = ref.asInt32()) != 0 && i != INT32_MIN) {
        i = -i;
        regs.sp[-1].setInt32(i);
    } else {
        double d;
        if (!ValueToNumber(cx, regs.sp[-1], &d))
            goto error;
        d = -d;
        regs.sp[-1].setDouble(d);
    }
}
END_CASE(JSOP_NEG)

BEGIN_CASE(JSOP_POS)
    if (!ValueToNumber(cx, &regs.sp[-1]))
        goto error;
END_CASE(JSOP_POS)

BEGIN_CASE(JSOP_DELNAME)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);
    JSObject *obj, *obj2;
    JSProperty *prop;
    if (!js_FindProperty(cx, id, &obj, &obj2, &prop))
        goto error;

    /* ECMA says to return true if name is undefined or inherited. */
    PUSH_BOOLEAN(true);
    if (prop) {
        obj2->dropProperty(cx, prop);
        if (!obj->deleteProperty(cx, id, &regs.sp[-1]))
            goto error;
    }
}
END_CASE(JSOP_DELNAME)

BEGIN_CASE(JSOP_DELPROP)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    jsid id = ATOM_TO_JSID(atom);

    JSObject *obj;
    FETCH_OBJECT(cx, -1, obj);

    Value rval;
    if (!obj->deleteProperty(cx, id, &rval))
        goto error;

    regs.sp[-1] = rval;
}
END_CASE(JSOP_DELPROP)

BEGIN_CASE(JSOP_DELELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);

    /* Get or set the element. */
    if (!obj->deleteProperty(cx, id, &regs.sp[-2]))
        goto error;

    regs.sp--;
}
END_CASE(JSOP_DELELEM)

BEGIN_CASE(JSOP_TYPEOFEXPR)
BEGIN_CASE(JSOP_TYPEOF)
{
    const Value &ref = regs.sp[-1];
    JSType type = JS_TypeOfValue(cx, Jsvalify(ref));
    JSAtom *atom = rt->atomState.typeAtoms[type];
    regs.sp[-1].setString(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_TYPEOF)

BEGIN_CASE(JSOP_VOID)
    regs.sp[-1].setUndefined();
END_CASE(JSOP_VOID)

{
    JSObject *obj;
    JSAtom *atom;
    jsid id;
    jsint i;

BEGIN_CASE(JSOP_INCELEM)
BEGIN_CASE(JSOP_DECELEM)
BEGIN_CASE(JSOP_ELEMINC)
BEGIN_CASE(JSOP_ELEMDEC)

    /*
     * Delay fetching of id until we have the object to ensure the proper
     * evaluation order. See bug 372331.
     */
    id = JSID_VOID;
    i = -2;
    goto fetch_incop_obj;

BEGIN_CASE(JSOP_INCPROP)
BEGIN_CASE(JSOP_DECPROP)
BEGIN_CASE(JSOP_PROPINC)
BEGIN_CASE(JSOP_PROPDEC)
    LOAD_ATOM(0, atom);
    id = ATOM_TO_JSID(atom);
    i = -1;

  fetch_incop_obj:
    FETCH_OBJECT(cx, i, obj);
    if (JSID_IS_VOID(id))
        FETCH_ELEMENT_ID(obj, -1, id);
    goto do_incop;

BEGIN_CASE(JSOP_INCNAME)
BEGIN_CASE(JSOP_DECNAME)
BEGIN_CASE(JSOP_NAMEINC)
BEGIN_CASE(JSOP_NAMEDEC)
BEGIN_CASE(JSOP_INCGNAME)
BEGIN_CASE(JSOP_DECGNAME)
BEGIN_CASE(JSOP_GNAMEINC)
BEGIN_CASE(JSOP_GNAMEDEC)
{
    obj = fp->scopeChainObj();
    if (js_CodeSpec[op].format & JOF_GNAME)
        obj = obj->getGlobal();

    JSObject *obj2;
    PropertyCacheEntry *entry;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (obj == obj2 && entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj->scope()->freeslot);
            Value &rref = obj->getSlotRef(slot);
            int32_t tmp;
            if (JS_LIKELY(rref.isInt32() && CanIncDecWithoutOverflow(tmp = rref.asInt32()))) {
                int32_t inc = tmp + ((js_CodeSpec[op].format & JOF_INC) ? 1 : -1);
                if (!(js_CodeSpec[op].format & JOF_POST))
                    tmp = inc;
                rref.asInt32Ref() = inc;
                PUSH_INT32(tmp);
                len = JSOP_INCNAME_LENGTH;
                DO_NEXT_OP(len);
            }
        }
        LOAD_ATOM(0, atom);
    }

    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        atomNotDefined = atom;
        goto atom_not_defined;
    }
    obj2->dropProperty(cx, prop);
}

do_incop:
{
    /*
     * We need a root to store the value to leave on the stack until
     * we have done with obj->setProperty.
     */
    PUSH_NULL();
    if (!obj->getProperty(cx, id, &regs.sp[-1]))
        goto error;

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->ndefs == 1);
    JS_ASSERT((cs->format & JOF_TMPSLOT_MASK) >= JOF_TMPSLOT2);
    Value &ref = regs.sp[-1];
    int32_t tmp;
    if (JS_LIKELY(ref.isInt32() && CanIncDecWithoutOverflow(tmp = ref.asInt32()))) {
        int incr = (cs->format & JOF_INC) ? 1 : -1;
        if (cs->format & JOF_POST)
            ref.asInt32Ref() = tmp + incr;
        else
            ref.asInt32Ref() = tmp += incr;
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &ref);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;

        /*
         * We must set regs.sp[-1] to tmp for both post and pre increments
         * as the setter overwrites regs.sp[-1].
         */
        ref.setInt32(tmp);
    } else {
        /* We need an extra root for the result. */
        PUSH_NULL();
        if (!js_DoIncDec(cx, cs, &regs.sp[-2], &regs.sp[-1]))
            goto error;
        fp->flags |= JSFRAME_ASSIGNING;
        JSBool ok = obj->setProperty(cx, id, &regs.sp[-1]);
        fp->flags &= ~JSFRAME_ASSIGNING;
        if (!ok)
            goto error;
        regs.sp--;
    }

    if (cs->nuses == 0) {
        /* regs.sp[-1] already contains the result of name increment. */
    } else {
        regs.sp[-1 - cs->nuses] = regs.sp[-1];
        regs.sp -= cs->nuses;
    }
    len = cs->length;
    DO_NEXT_OP(len);
}
}

{
    int incr, incr2;
    Value *vp;

BEGIN_CASE(JSOP_INCGLOBAL)
    incr =  1; incr2 =  1; goto do_bound_global_incop;
BEGIN_CASE(JSOP_DECGLOBAL)
    incr = -1; incr2 = -1; goto do_bound_global_incop;
BEGIN_CASE(JSOP_GLOBALINC)
    incr =  1; incr2 =  0; goto do_bound_global_incop;
BEGIN_CASE(JSOP_GLOBALDEC)
    incr = -1; incr2 =  0; goto do_bound_global_incop;

  do_bound_global_incop:
    uint32 slot;
    slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj;
    obj = fp->scopeChainObj()->getGlobal();
    vp = &obj->getSlotRef(slot);
    goto do_int_fast_incop;
END_CASE(JSOP_INCGLOBAL)

    /* Position cases so the most frequent i++ does not need a jump. */
BEGIN_CASE(JSOP_DECARG)
    incr = -1; incr2 = -1; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGDEC)
    incr = -1; incr2 =  0; goto do_arg_incop;
BEGIN_CASE(JSOP_INCARG)
    incr =  1; incr2 =  1; goto do_arg_incop;
BEGIN_CASE(JSOP_ARGINC)
    incr =  1; incr2 =  0;

  do_arg_incop:
    slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    vp = fp->argv + slot;
    goto do_int_fast_incop;

BEGIN_CASE(JSOP_DECLOCAL)
    incr = -1; incr2 = -1; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALDEC)
    incr = -1; incr2 =  0; goto do_local_incop;
BEGIN_CASE(JSOP_INCLOCAL)
    incr =  1; incr2 =  1; goto do_local_incop;
BEGIN_CASE(JSOP_LOCALINC)
    incr =  1; incr2 =  0;

  /*
   * do_local_incop comes right before do_int_fast_incop as we want to
   * avoid an extra jump for variable cases as local++ is more frequent
   * than arg++.
   */
  do_local_incop:
    slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < fp->script->nslots);
    vp = fp->slots() + slot;
    METER_SLOT_OP(op, slot);
    vp = fp->slots() + slot;

  do_int_fast_incop:
    int32_t tmp;
    if (JS_LIKELY(vp->isInt32() && CanIncDecWithoutOverflow(tmp = vp->asInt32()))) {
        vp->asInt32Ref() = tmp + incr;
        JS_ASSERT(JSOP_INCARG_LENGTH == js_CodeSpec[op].length);
        SKIP_POP_AFTER_SET(JSOP_INCARG_LENGTH, 0);
        PUSH_INT32(tmp + incr2);
    } else {
        PUSH_COPY(*vp);
        if (!js_DoIncDec(cx, &js_CodeSpec[op], &regs.sp[-1], vp))
            goto error;
    }
    len = JSOP_INCARG_LENGTH;
    JS_ASSERT(len == js_CodeSpec[op].length);
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_THIS)
    if (!fp->getThisObject(cx))
        goto error;
    PUSH_COPY(fp->thisv);
END_CASE(JSOP_THIS)

BEGIN_CASE(JSOP_UNBRANDTHIS)
{
    JSObject *obj = fp->getThisObject(cx);
    if (!obj)
        goto error;
    if (!obj->unbrand(cx))
        goto error;
}
END_CASE(JSOP_UNBRANDTHIS)

{
    JSObject *obj;
    Value *vp;
    jsint i;

BEGIN_CASE(JSOP_GETTHISPROP)
    obj = fp->getThisObject(cx);
    if (!obj)
        goto error;
    i = 0;
    PUSH_NULL();
    goto do_getprop_with_obj;

BEGIN_CASE(JSOP_GETARGPROP)
{
    i = ARGNO_LEN;
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    PUSH_COPY(fp->argv[slot]);
    goto do_getprop_body;
}

BEGIN_CASE(JSOP_GETLOCALPROP)
{
    i = SLOTNO_LEN;
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
    goto do_getprop_body;
}

BEGIN_CASE(JSOP_GETPROP)
BEGIN_CASE(JSOP_GETXPROP)
    i = 0;

  do_getprop_body:
    vp = &regs.sp[-1];

  do_getprop_with_lval:
    VALUE_TO_OBJECT(cx, vp, obj);

  do_getprop_with_obj:
    {
        Value rval;
        do {
            /*
             * We do not impose the method read barrier if in an imacro,
             * assuming any property gets it does (e.g., for 'toString'
             * from JSOP_NEW) will not be leaked to the calling script.
             */
            JSObject *aobj = js_GetProtoIfDenseArray(obj);

            PropertyCacheEntry *entry;
            JSObject *obj2;
            JSAtom *atom;
            JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
            if (!atom) {
                ASSERT_VALID_PROPERTY_CACHE_HIT(i, aobj, obj2, entry);
                if (entry->vword.isFunObj()) {
                    rval.setFunObj(entry->vword.toFunObj());
                } else if (entry->vword.isSlot()) {
                    uint32 slot = entry->vword.toSlot();
                    JS_ASSERT(slot < obj2->scope()->freeslot);
                    rval = obj2->lockedGetSlot(slot);
                } else {
                    JS_ASSERT(entry->vword.isSprop());
                    JSScopeProperty *sprop = entry->vword.toSprop();
                    NATIVE_GET(cx, obj, obj2, sprop,
                               fp->imacpc ? JSGET_NO_METHOD_BARRIER : JSGET_METHOD_BARRIER,
                               &rval);
                }
                break;
            }

            jsid id = ATOM_TO_JSID(atom);
            if (JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                ? !js_GetPropertyHelper(cx, obj, id,
                                        fp->imacpc
                                        ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                                        : JSGET_CACHE_RESULT | JSGET_METHOD_BARRIER,
                                        &rval)
                : !obj->getProperty(cx, id, &rval)) {
                goto error;
            }
        } while (0);

        regs.sp[-1] = rval;
        JS_ASSERT(JSOP_GETPROP_LENGTH + i == js_CodeSpec[op].length);
        len = JSOP_GETPROP_LENGTH + i;
    }
END_VARLEN_CASE

BEGIN_CASE(JSOP_LENGTH)
    vp = &regs.sp[-1];
    if (vp->isString()) {
        vp->setInt32(vp->asString()->length());
    } else if (vp->isObject()) {
        JSObject *obj = &vp->asObject();
        if (obj->isArray()) {
            jsuint length = obj->getArrayLength();
            regs.sp[-1].setNumber(length);
        } else if (obj->isArguments() && !obj->isArgsLengthOverridden()) {
            uint32 length = obj->getArgsLength();
            JS_ASSERT(length < INT32_MAX);
            regs.sp[-1].setInt32(int32_t(length));
        } else {
            i = -2;
            goto do_getprop_with_lval;
        }
    } else {
        i = -2;
        goto do_getprop_with_lval;
    }
END_CASE(JSOP_LENGTH)

}

BEGIN_CASE(JSOP_CALLPROP)
{
    Value lval = regs.sp[-1];

    Value objv;
    if (lval.isObject()) {
        objv = lval;
    } else {
        JSProtoKey protoKey;
        if (lval.isString()) {
            protoKey = JSProto_String;
        } else if (lval.isNumber()) {
            protoKey = JSProto_Number;
        } else if (lval.isBoolean()) {
            protoKey = JSProto_Boolean;
        } else {
            JS_ASSERT(lval.isNull() || lval.isUndefined());
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            goto error;
        }
        JSObject *pobj;
        if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
            goto error;
        objv.setNonFunObj(*pobj);
    }

    JSObject *aobj = js_GetProtoIfDenseArray(&objv.asObject());
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, aobj, obj2, entry);
        if (entry->vword.isFunObj()) {
            rval.setFunObj(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            NATIVE_GET(cx, &objv.asObject(), obj2, sprop, JSGET_NO_METHOD_BARRIER, &rval);
        }
        regs.sp[-1] = rval;
        PUSH_COPY(lval);
        goto end_callprop;
    }

    /*
     * Cache miss: use the immediate atom that was loaded for us under
     * PropertyCache::test.
     */
    jsid id;
    id = ATOM_TO_JSID(atom);

    PUSH_NULL();
    if (lval.isObject()) {
        if (!js_GetMethod(cx, &objv.asObject(), id,
                          JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            goto error;
        }
        regs.sp[-1] = objv;
        regs.sp[-2] = rval;
    } else {
        JS_ASSERT(objv.asObject().map->ops->getProperty == js_GetProperty);
        if (!js_GetPropertyHelper(cx, &objv.asObject(), id,
                                  JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                  &rval)) {
            goto error;
        }
        regs.sp[-1] = lval;
        regs.sp[-2] = rval;
    }

  end_callprop:
    /* Wrap primitive lval in object clothing if necessary. */
    if (lval.isPrimitive()) {
        /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
        if (!rval.isFunObj() ||
            !PrimitiveThisTest(GET_FUNCTION_PRIVATE(cx, &rval.asFunObj()), lval)) {
            if (!js_PrimitiveToObject(cx, &regs.sp[-1]))
                goto error;
        }
    }
#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isUndefined())) {
        LOAD_ATOM(0, atom);
        regs.sp[-2].setString(ATOM_TO_STRING(atom));
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    }
#endif
}
END_CASE(JSOP_CALLPROP)

BEGIN_CASE(JSOP_UNBRAND)
    JS_ASSERT(regs.sp - fp->slots() >= 1);
    if (!regs.sp[-1].asObject().unbrand(cx))
        goto error;
END_CASE(JSOP_UNBRAND)

BEGIN_CASE(JSOP_SETGNAME)
BEGIN_CASE(JSOP_SETNAME)
BEGIN_CASE(JSOP_SETPROP)
BEGIN_CASE(JSOP_SETMETHOD)
{
    Value &rref = regs.sp[-1];
    JS_ASSERT_IF(op == JSOP_SETMETHOD, rref.isFunObj());
    Value &lref = regs.sp[-2];
    JS_ASSERT_IF(op == JSOP_SETNAME, lref.isObject());
    JSObject *obj;
    VALUE_TO_OBJECT(cx, &lref, obj);

    JS_ASSERT_IF(op == JSOP_SETGNAME, obj == fp->scopeChainObj()->getGlobal());

    do {
        PropertyCache *cache = &JS_PROPERTY_CACHE(cx);

        /*
         * Probe the property cache, specializing for two important
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
        PropertyCacheEntry *entry;
        JSObject *obj2;
        JSAtom *atom;
        if (cache->testForSet(cx, regs.pc, obj, &entry, &obj2, &atom)) {
            /*
             * Fast property cache hit, only partially confirmed by
             * testForSet. We know that the entry applies to regs.pc and
             * that obj's shape matches.
             *
             * The entry predicts either a new property to be added
             * directly to obj by this set, or on an existing "own"
             * property, or on a prototype property that has a setter.
             */
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            JS_ASSERT_IF(sprop->isDataDescriptor(), sprop->writable());
            JS_ASSERT_IF(sprop->hasSlot(), entry->vcapTag() == 0);

            JSScope *scope = obj->scope();
            JS_ASSERT(!scope->sealed());

            /*
             * Fastest path: check whether the cached sprop is already
             * in scope and call NATIVE_SET and break to get out of the
             * do-while(0). But we can call NATIVE_SET only if obj owns
             * scope or sprop is shared.
             */
            bool checkForAdd;
            if (!sprop->hasSlot()) {
                if (entry->vcapTag() == 0 ||
                    ((obj2 = obj->getProto()) &&
                     obj2->isNative() &&
                     obj2->shape() == entry->vshape())) {
                    goto fast_set_propcache_hit;
                }

                /* The cache entry doesn't apply. vshape mismatch. */
                checkForAdd = false;
            } else if (!scope->isSharedEmpty()) {
                if (sprop == scope->lastProperty() || scope->hasProperty(sprop)) {
                  fast_set_propcache_hit:
                    PCMETER(cache->pchits++);
                    PCMETER(cache->setpchits++);
                    NATIVE_SET(cx, obj, sprop, entry, &rref);
                    break;
                }
                checkForAdd = sprop->hasSlot() && sprop->parent == scope->lastProperty();
            } else {
                /*
                 * We check that cx own obj here and will continue to
                 * own it after js_GetMutableScope returns so we can
                 * continue to skip JS_UNLOCK_OBJ calls.
                 */
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                scope = js_GetMutableScope(cx, obj);
                JS_ASSERT(CX_OWNS_OBJECT_TITLE(cx, obj));
                if (!scope)
                    goto error;
                checkForAdd = !sprop->parent;
            }

            uint32 slot;
            if (checkForAdd &&
                entry->vshape() == rt->protoHazardShape &&
                sprop->hasDefaultSetter() &&
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
                PCMETER(cache->pchits++);
                PCMETER(cache->addpchits++);

                /*
                 * Beware classes such as Function that use the
                 * reserveSlots hook to allocate a number of reserved
                 * slots that may vary with obj.
                 */
                if (slot < obj->numSlots() &&
                    !obj->getClass()->reserveSlots) {
                    ++scope->freeslot;
                } else {
                    if (!js_AllocSlot(cx, obj, &slot))
                        goto error;
                }

                /*
                 * If this obj's number of reserved slots differed, or
                 * if something created a hash table for scope, we must
                 * pay the price of JSScope::putProperty.
                 *
                 * (A reserveSlots hook can cause scopes of the same
                 * shape to have different freeslot values. This is
                 * what causes the slot != sprop->slot case. See
                 * js_GetMutableScope.)
                 */
                if (slot != sprop->slot || scope->table) {
                    JSScopeProperty *sprop2 =
                        scope->putProperty(cx, sprop->id,
                                           sprop->getter(), sprop->setter(),
                                           slot, sprop->attributes(),
                                           sprop->getFlags(), sprop->shortid);
                    if (!sprop2) {
                        js_FreeSlot(cx, obj, slot);
                        goto error;
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
                obj->lockedSetSlot(slot, rref);

                /*
                 * Purge the property cache of the id we may have just
                 * shadowed in obj's scope and proto chains. We do this
                 * after unlocking obj's scope to avoid lock nesting.
                 */
                js_PurgeScopeChain(cx, obj, sprop->id);
                break;
            }
            PCMETER(cache->setpcmisses++);
            atom = NULL;
        } else if (!atom) {
            /*
             * Slower property cache hit, fully confirmed by testForSet (in
             * the slow path, via fullTest).
             */
            ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
            JSScopeProperty *sprop = NULL;
            if (obj == obj2) {
                sprop = entry->vword.toSprop();
                JS_ASSERT(sprop->writable());
                JS_ASSERT(!obj2->scope()->sealed());
                NATIVE_SET(cx, obj, sprop, entry, &rref);
            }
            if (sprop)
                break;
        }

        if (!atom)
            LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);
        if (entry && JS_LIKELY(obj->map->ops->setProperty == js_SetProperty)) {
            uintN defineHow;
            if (op == JSOP_SETMETHOD)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_SET_METHOD;
            else if (op == JSOP_SETNAME)
                defineHow = JSDNP_CACHE_RESULT | JSDNP_UNQUALIFIED;
            else
                defineHow = JSDNP_CACHE_RESULT;
            if (!js_SetPropertyHelper(cx, obj, id, defineHow, &rref))
                goto error;
        } else {
            if (!obj->setProperty(cx, id, &rref))
                goto error;
            ABORT_RECORDING(cx, "Non-native set");
        }
    } while (0);
}
END_SET_CASE_STORE_RVAL(JSOP_SETPROP, 2);

BEGIN_CASE(JSOP_GETELEM)
{
    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    if (lref.isString() && rref.isInt32()) {
        JSString *str = lref.asString();
        int32_t i = rref.asInt32();
        if (size_t(i) < str->length()) {
            str = JSString::getUnitString(cx, str, size_t(i));
            if (!str)
                goto error;
            regs.sp--;
            regs.sp[-1].setString(str);
            len = JSOP_GETELEM_LENGTH;
            DO_NEXT_OP(len);
        }
    }

    JSObject *obj;
    VALUE_TO_OBJECT(cx, &lref, obj);

    const Value *copyFrom;
    Value rval;
    jsid id;
    if (rref.isInt32()) {
        int32_t i = rref.asInt32();
        if (obj->isDenseArray()) {
            jsuint idx = jsuint(i);

            if (idx < obj->getArrayLength() &&
                idx < obj->getDenseArrayCapacity()) {
                copyFrom = obj->addressOfDenseArrayElement(idx);
                if (!copyFrom->isMagic())
                    goto end_getelem;

                /* Reload retval from the stack in the rare hole case. */
                copyFrom = &regs.sp[-1];
            }
        } else if (obj->isArguments()
#ifdef JS_TRACER
                   && !GetArgsPrivateNative(obj)
#endif
                  ) {
            uint32 arg = uint32(i);

            if (arg < obj->getArgsLength()) {
                JSStackFrame *afp = (JSStackFrame *) obj->getPrivate();
                if (afp) {
                    copyFrom = &afp->argv[arg];
                    goto end_getelem;
                }

                copyFrom = obj->addressOfArgsElement(arg);
                if (!copyFrom->isMagic())
                    goto end_getelem;
                copyFrom = &regs.sp[-1];
            }
        }
        if (JS_LIKELY(INT_FITS_IN_JSID(i)))
            id = INT_TO_JSID(i);
        else
            goto intern_big_int;
    } else {
      intern_big_int:
        if (!js_InternNonIntElementId(cx, obj, rref, &id))
            goto error;
    }

    if (!obj->getProperty(cx, id, &rval))
        goto error;
    copyFrom = &rval;

  end_getelem:
    regs.sp--;
    regs.sp[-1] = *copyFrom;
}
END_CASE(JSOP_GETELEM)

BEGIN_CASE(JSOP_CALLELEM)
{
    /* Fetch the left part and resolve it to a non-null object. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);

    /* Fetch index and convert it to id suitable for use with obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);

    /* Get or set the element. */
    if (!js_GetMethod(cx, obj, id, JSGET_NO_METHOD_BARRIER, &regs.sp[-2]))
        goto error;

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(regs.sp[-2].isUndefined())) {
        regs.sp[-2] = regs.sp[-1];
        regs.sp[-1].setObject(*obj);
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            goto error;
    } else
#endif
    {
        regs.sp[-1].setObject(*obj);
    }
}
END_CASE(JSOP_CALLELEM)

BEGIN_CASE(JSOP_SETELEM)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -3, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    do {
        if (obj->isDenseArray() && JSID_IS_INT(id)) {
            jsuint length = obj->getDenseArrayCapacity();
            jsint i = JSID_TO_INT(id);
            if ((jsuint)i < length) {
                if (obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
                    if (js_PrototypeHasIndexedProperties(cx, obj))
                        break;
                    if ((jsuint)i >= obj->getArrayLength())
                        obj->setDenseArrayLength(i + 1);
                    obj->incDenseArrayCountBy(1);
                }
                obj->setDenseArrayElement(i, regs.sp[-1]);
                goto end_setelem;
            }
        }
    } while (0);
    if (!obj->setProperty(cx, id, &regs.sp[-1]))
        goto error;
  end_setelem:;
}
END_SET_CASE_STORE_RVAL(JSOP_SETELEM, 3)

BEGIN_CASE(JSOP_ENUMELEM)
{
    /* Funky: the value to set is under the [obj, id] pair. */
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsid id;
    FETCH_ELEMENT_ID(obj, -1, id);
    if (!obj->setProperty(cx, id, &regs.sp[-3]))
        goto error;
    regs.sp -= 3;
}
END_CASE(JSOP_ENUMELEM)

{
    JSFunction *fun;
    JSObject *obj;
    uintN flags;
    uintN argc;
    Value *vp;

BEGIN_CASE(JSOP_NEW)
{
    /* Get immediate argc and find the constructor function. */
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (2 + argc);
    JS_ASSERT(vp >= fp->base());

    /*
     * Assign lval, obj, and fun exactly as the code at inline_call: expects to
     * find them, to avoid nesting a js_Interpret call via js_InvokeConstructor.
     */
    if (vp[0].isFunObj()) {
        obj = &vp[0].asFunObj();
        fun = GET_FUNCTION_PRIVATE(cx, obj);
        if (fun->isInterpreted()) {
            /* Root as we go using vp[1]. */
            if (!obj->getProperty(cx,
                                  ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom),
                                  &vp[1])) {
                goto error;
            }
            JSObject *proto = vp[1].isObject() ? &vp[1].asObject() : NULL;
            JSObject *obj2 = NewObject(cx, &js_ObjectClass, proto, obj->getParent());
            if (!obj2)
                goto error;

            if (fun->u.i.script->isEmpty()) {
                vp[0].setNonFunObj(*obj2);
                regs.sp = vp + 1;
                goto end_new;
            }

            vp[1].setNonFunObj(*obj2);
            flags = JSFRAME_CONSTRUCTING;
            goto inline_call;
        }
    }

    if (!InvokeConstructor(cx, InvokeArgsGuard(vp, argc), JS_FALSE))
        goto error;
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    TRACE_0(NativeCallComplete);

  end_new:;
}
END_CASE(JSOP_NEW)

BEGIN_CASE(JSOP_CALL)
BEGIN_CASE(JSOP_EVAL)
BEGIN_CASE(JSOP_APPLY)
{
    argc = GET_ARGC(regs.pc);
    vp = regs.sp - (argc + 2);

    if (vp->isFunObj()) {
        obj = &vp->asFunObj();
        fun = GET_FUNCTION_PRIVATE(cx, obj);

        /* Clear frame flags since this is not a constructor call. */
        flags = 0;
        if (FUN_INTERPRETED(fun))
      inline_call:
        {
            JSScript *newscript = fun->u.i.script;
            if (JS_UNLIKELY(newscript->isEmpty())) {
                vp->setUndefined();
                regs.sp = vp + 1;
                goto end_call;
            }

            /* Restrict recursion of lightweight functions. */
            if (JS_UNLIKELY(inlineCallCount >= JS_MAX_INLINE_CALL_COUNT)) {
                js_ReportOverRecursed(cx);
                goto error;
            }

            /*
             * Get pointer to new frame/slots, without changing global state.
             * Initialize missing args if there are any.
             */
            StackSpace &stack = cx->stack();
            uintN nfixed = newscript->nslots;
            uintN funargs = fun->nargs;
            JSStackFrame *newfp;
            if (argc < funargs) {
                uintN missing = funargs - argc;
                newfp = stack.getInlineFrame(cx, regs.sp, missing, nfixed);
                if (!newfp)
                    goto error;
                SetValueRangeToUndefined(regs.sp, missing);
            } else {
                newfp = stack.getInlineFrame(cx, regs.sp, 0, nfixed);
                if (!newfp)
                    goto error;
            }

            /* Initialize stack frame. */
            newfp->callobj = NULL;
            newfp->setArgsObj(NULL);
            newfp->script = newscript;
            newfp->fun = fun;
            newfp->argc = argc;
            newfp->argv = vp + 2;
            newfp->rval.setUndefined();
            newfp->annotation = NULL;
            newfp->setScopeChainObj(obj->getParent());
            newfp->flags = flags;
            newfp->blockChain = NULL;
            if (JS_LIKELY(newscript->staticLevel < JS_DISPLAY_SIZE)) {
                JSStackFrame **disp = &cx->display[newscript->staticLevel];
                newfp->displaySave = *disp;
                *disp = newfp;
            }
            JS_ASSERT(!JSFUN_BOUND_METHOD_TEST(fun->flags));
            newfp->thisv = vp[1];
            newfp->imacpc = NULL;

            /* Push void to initialize local variables. */
            Value *newsp = newfp->base();
            SetValueRangeToUndefined(newfp->slots(), newsp);

            /* Scope with a call object parented by callee's parent. */
            if (fun->isHeavyweight() && !js_GetCallObject(cx, newfp))
                goto error;

            /* Switch version if currentVersion wasn't overridden. */
            newfp->callerVersion = (JSVersion) cx->version;
            if (JS_LIKELY(cx->version == currentVersion)) {
                currentVersion = (JSVersion) newscript->version;
                if (JS_UNLIKELY(currentVersion != cx->version))
                    js_SetVersion(cx, currentVersion);
            }

            /* Push the frame. */
            stack.pushInlineFrame(cx, fp, regs.pc, newfp);

            /* Initializer regs after pushInlineFrame snapshots pc. */
            regs.pc = newscript->code;
            regs.sp = newsp;

            /* Import into locals. */
            JS_ASSERT(newfp == cx->fp);
            fp = newfp;
            script = newscript;
            atoms = script->atomMap.vector;

            /* Call the debugger hook if present. */
            if (JSInterpreterHook hook = cx->debugHooks->callHook) {
                fp->hookData = hook(cx, fp, JS_TRUE, 0,
                                    cx->debugHooks->callHookData);
                CHECK_INTERRUPT_HANDLER();
            } else {
                fp->hookData = NULL;
            }

            inlineCallCount++;
            JS_RUNTIME_METER(rt, inlineCalls);

            DTrace::enterJSFun(cx, fp, fun, fp->down, fp->argc, fp->argv);

#ifdef JS_TRACER
            if (TraceRecorder *tr = TRACE_RECORDER(cx)) {
                AbortableRecordingStatus status = tr->record_EnterFrame(inlineCallCount);
                RESTORE_INTERP_VARS();
                if (StatusAbortsRecorderIfActive(status)) {
                    if (TRACE_RECORDER(cx)) {
                        JS_ASSERT(TRACE_RECORDER(cx) == tr);
                        AbortRecording(cx, "record_EnterFrame failed");
                    }
                    if (status == ARECORD_ERROR)
                        goto error;
                }
            } else if (fp->script == fp->down->script &&
                       *fp->down->savedPC == JSOP_CALL &&
                       *regs.pc == JSOP_TRACE) {
                MONITOR_BRANCH(Record_EnterFrame);
            }
#endif

#ifdef JS_METHODJIT
            /*
             * :FIXME: try to method jit - take this out once we're more
             * complete.
             */
            if (!TRACE_RECORDER(cx)) {
                JSObject *scope = newfp->scopeChainObj();
                mjit::CompileStatus status = mjit::CanMethodJIT(cx, newscript, fun, scope);
                if (status == mjit::Compile_Error)
                    goto error;
                if (status == mjit::Compile_Okay) {
                    if (!mjit::JaegerShot(cx))
                        goto error;
                    interpReturnOK = true;
                    goto inline_return;
                }
            }
#endif

            /* Load first op and dispatch it (safe since JSOP_STOP). */
            op = (JSOp) *regs.pc;
            DO_OP();
        }

        if (fun->flags & JSFUN_FAST_NATIVE) {
            DTrace::enterJSFun(cx, NULL, fun, fp, argc, vp + 2, vp);

            JS_ASSERT(fun->u.n.extra == 0);
            JS_ASSERT(vp[1].isObjectOrNull() || PrimitiveThisTest(fun, vp[1]));
            JSBool ok = ((FastNative) fun->u.n.native)(cx, argc, vp);
            DTrace::exitJSFun(cx, NULL, fun, *vp, vp);
            regs.sp = vp + 1;
            if (!ok)
                goto error;
            TRACE_0(NativeCallComplete);
            goto end_call;
        }
    }

    bool ok;
    ok = Invoke(cx, InvokeArgsGuard(vp, argc), 0);
    regs.sp = vp + 1;
    CHECK_INTERRUPT_HANDLER();
    if (!ok)
        goto error;
    JS_RUNTIME_METER(rt, nonInlineCalls);
    TRACE_0(NativeCallComplete);

  end_call:;
}
END_CASE(JSOP_CALL)
}

BEGIN_CASE(JSOP_SETCALL)
{
    uintN argc = GET_ARGC(regs.pc);
    Value *vp = regs.sp - argc - 2;
    JSBool ok = Invoke(cx, InvokeArgsGuard(vp, argc), 0);
    if (ok)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
    goto error;
}
END_CASE(JSOP_SETCALL)

BEGIN_CASE(JSOP_GETGNAME)
BEGIN_CASE(JSOP_CALLGNAME)
BEGIN_CASE(JSOP_NAME)
BEGIN_CASE(JSOP_CALLNAME)
{
    JSObject *obj = fp->scopeChainObj();
    if (op == JSOP_GETGNAME || op == JSOP_CALLGNAME)
        obj = obj->getGlobal();

    JSScopeProperty *sprop;
    Value rval;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, obj, obj2, entry, atom);
    if (!atom) {
        ASSERT_VALID_PROPERTY_CACHE_HIT(0, obj, obj2, entry);
        if (entry->vword.isFunObj()) {
            PUSH_FUNOBJ(entry->vword.toFunObj());
            goto do_push_obj_if_call;
        }

        if (entry->vword.isSlot()) {
            uintN slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            PUSH_COPY(obj2->lockedGetSlot(slot));
            goto do_push_obj_if_call;
        }

        JS_ASSERT(entry->vword.isSprop());
        sprop = entry->vword.toSprop();
        goto do_native_get;
    }

    jsid id;
    id = ATOM_TO_JSID(atom);
    JSProperty *prop;
    if (!js_FindPropertyHelper(cx, id, true, &obj, &obj2, &prop))
        goto error;
    if (!prop) {
        /* Kludge to allow (typeof foo == "undefined") tests. */
        JSOp op2 = js_GetOpcode(cx, script, regs.pc + JSOP_NAME_LENGTH);
        if (op2 == JSOP_TYPEOF) {
            PUSH_UNDEFINED();
            len = JSOP_NAME_LENGTH;
            DO_NEXT_OP(len);
        }
        atomNotDefined = atom;
        goto atom_not_defined;
    }

    /* Take the slow path if prop was not found in a native object. */
    if (!obj->isNative() || !obj2->isNative()) {
        obj2->dropProperty(cx, prop);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    } else {
        sprop = (JSScopeProperty *)prop;
  do_native_get:
        NATIVE_GET(cx, obj, obj2, sprop, JSGET_METHOD_BARRIER, &rval);
        JS_UNLOCK_OBJ(cx, obj2);
    }

    PUSH_COPY(rval);

  do_push_obj_if_call:
    /* obj must be on the scope chain, thus not a function. */
    if (op == JSOP_CALLNAME || op == JSOP_CALLGNAME)
        PUSH_NONFUNOBJ(*obj);
}
END_CASE(JSOP_NAME)

BEGIN_CASE(JSOP_UINT16)
    PUSH_INT32((int32_t) GET_UINT16(regs.pc));
END_CASE(JSOP_UINT16)

BEGIN_CASE(JSOP_UINT24)
    PUSH_INT32((int32_t) GET_UINT24(regs.pc));
END_CASE(JSOP_UINT24)

BEGIN_CASE(JSOP_INT8)
    PUSH_INT32(GET_INT8(regs.pc));
END_CASE(JSOP_INT8)

BEGIN_CASE(JSOP_INT32)
    PUSH_INT32(GET_INT32(regs.pc));
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
{
    JS_ASSERT(!fp->imacpc);
    JS_ASSERT(size_t(atoms - script->atomMap.vector) <= script->atomMap.length);
    double dbl;
    LOAD_DOUBLE(0, dbl);
    PUSH_DOUBLE(dbl);
}
END_CASE(JSOP_DOUBLE)

BEGIN_CASE(JSOP_STRING)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_STRING)

BEGIN_CASE(JSOP_OBJECT)
{
    JSObject *obj;
    LOAD_OBJECT(0, obj);
    /* Only XML and RegExp objects are emitted. */
    PUSH_NONFUNOBJ(*obj);
}
END_CASE(JSOP_OBJECT)

BEGIN_CASE(JSOP_REGEXP)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc. ES5 finally fixed this bad old ES3 design flaw which was
     * flouted by many browser-based implementations.
     *
     * We avoid the js_GetScopeChain call here and pass fp->scopeChain as
     * js_GetClassPrototype uses the latter only to locate the global.
     */
    jsatomid index = GET_FULL_INDEX(0);
    JSObject *proto;
    if (!js_GetClassPrototype(cx, fp->scopeChainObj(), JSProto_RegExp, &proto))
        goto error;
    JS_ASSERT(proto);
    JSObject *obj = js_CloneRegExpObject(cx, script->getRegExp(index), proto);
    if (!obj)
        goto error;
    PUSH_NONFUNOBJ(*obj);
}
END_CASE(JSOP_REGEXP)

BEGIN_CASE(JSOP_ZERO)
    PUSH_INT32(0);
END_CASE(JSOP_ZERO)

BEGIN_CASE(JSOP_ONE)
    PUSH_INT32(1);
END_CASE(JSOP_ONE)

BEGIN_CASE(JSOP_NULL)
    PUSH_NULL();
END_CASE(JSOP_NULL)

BEGIN_CASE(JSOP_FALSE)
    PUSH_BOOLEAN(false);
END_CASE(JSOP_FALSE)

BEGIN_CASE(JSOP_TRUE)
    PUSH_BOOLEAN(true);
END_CASE(JSOP_TRUE)

{
BEGIN_CASE(JSOP_TABLESWITCH)
{
    jsbytecode *pc2 = regs.pc;
    len = GET_JUMP_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    const Value &rref = *--regs.sp;
    int32_t i;
    if (rref.isInt32()) {
        i = rref.asInt32();
    } else if (rref.isDouble() && rref.asDouble() == 0) {
        /* Treat -0 (double) as 0. */
        i = 0;
    } else {
        DO_NEXT_OP(len);
    }

    pc2 += JUMP_OFFSET_LEN;
    jsint low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
        jsint off = (jsint) GET_JUMP_OFFSET(pc2);
        if (off)
            len = off;
    }
}
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_TABLESWITCHX)
{
    jsbytecode *pc2 = regs.pc;
    len = GET_JUMPX_OFFSET(pc2);

    /*
     * ECMAv2+ forbids conversion of discriminant, so we will skip to the
     * default case if the discriminant isn't already an int jsval.  (This
     * opcode is emitted only for dense jsint-domain switches.)
     */
    const Value &rref = *--regs.sp;
    int32_t i;
    if (rref.isInt32()) {
        i = rref.asInt32();
    } else if (rref.isDouble() && rref.asDouble() == 0) {
        /* Treat -0 (double) as 0. */
        i = 0;
    } else {
        DO_NEXT_OP(len);
    }

    pc2 += JUMPX_OFFSET_LEN;
    jsint low = GET_JUMP_OFFSET(pc2);
    pc2 += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc2);

    i -= low;
    if ((jsuint)i < (jsuint)(high - low + 1)) {
        pc2 += JUMP_OFFSET_LEN + JUMPX_OFFSET_LEN * i;
        jsint off = (jsint) GET_JUMPX_OFFSET(pc2);
        if (off)
            len = off;
    }
}
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_LOOKUPSWITCHX)
{
    jsint off;
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
    jsbytecode *pc2 = regs.pc;

    Value lval = regs.sp[-1];
    regs.sp--;

    if (!lval.isPrimitive())
        goto end_lookup_switch;

    pc2 += off;
    jsint npairs;
    npairs = (jsint) GET_UINT16(pc2);
    pc2 += UINT16_LEN;
    JS_ASSERT(npairs);  /* empty switch uses JSOP_TABLESWITCH */

    bool match;
#define SEARCH_PAIRS(MATCH_CODE)                                              \
    for (;;) {                                                                \
        Value rval = script->getConst(GET_INDEX(pc2));                        \
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

    if (lval.isString()) {
        JSString *str = lval.asString();
        JSString *str2;
        SEARCH_PAIRS(
            match = (rval.isString() &&
                     ((str2 = rval.asString()) == str ||
                      js_EqualStrings(str2, str)));
        )
    } else if (lval.isNumber()) {
        double ldbl = lval.asNumber();
        SEARCH_PAIRS(
            match = rval.isNumber() && ldbl == rval.asNumber();
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
}
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_TRAP)
{
    Value rval;
    JSTrapStatus status = JS_HandleTrap(cx, script, regs.pc, Jsvalify(&rval));
    switch (status) {
      case JSTRAP_ERROR:
        goto error;
      case JSTRAP_RETURN:
        fp->rval = rval;
        interpReturnOK = JS_TRUE;
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
    JS_ASSERT(rval.isInt32());
    op = (JSOp) rval.asInt32();
    JS_ASSERT((uintN)op < (uintN)JSOP_LIMIT);
    DO_OP();
}

BEGIN_CASE(JSOP_ARGUMENTS)
{
    Value rval;
    if (!js_GetArgsValue(cx, fp, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGUMENTS)

BEGIN_CASE(JSOP_ARGSUB)
{
    jsid id = INT_TO_JSID(GET_ARGNO(regs.pc));
    Value rval;
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGSUB)

BEGIN_CASE(JSOP_ARGCNT)
{
    jsid id = ATOM_TO_JSID(rt->atomState.lengthAtom);
    Value rval;
    if (!js_GetArgsProperty(cx, fp, id, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_ARGCNT)

BEGIN_CASE(JSOP_GETARG)
BEGIN_CASE(JSOP_CALLARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    PUSH_COPY(fp->argv[slot]);
    if (op == JSOP_CALLARG)
        PUSH_NULL();
}
END_CASE(JSOP_GETARG)

BEGIN_CASE(JSOP_SETARG)
{
    uint32 slot = GET_ARGNO(regs.pc);
    JS_ASSERT(slot < fp->fun->nargs);
    METER_SLOT_OP(op, slot);
    fp->argv[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETARG)

BEGIN_CASE(JSOP_GETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
}
END_CASE(JSOP_GETLOCAL)

BEGIN_CASE(JSOP_CALLLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    PUSH_COPY(fp->slots()[slot]);
    PUSH_NULL();
}
END_CASE(JSOP_CALLLOCAL)

BEGIN_CASE(JSOP_SETLOCAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    JS_ASSERT(slot < script->nslots);
    fp->slots()[slot] = regs.sp[-1];
}
END_SET_CASE(JSOP_SETLOCAL)

BEGIN_CASE(JSOP_GETUPVAR)
BEGIN_CASE(JSOP_CALLUPVAR)
{
    JSUpvarArray *uva = script->upvars();

    uintN index = GET_UINT16(regs.pc);
    JS_ASSERT(index < uva->length);

    const Value &rval = js_GetUpvar(cx, script->staticLevel, uva->vector[index]);
    PUSH_COPY(rval);

    if (op == JSOP_CALLUPVAR)
        PUSH_NULL();
}
END_CASE(JSOP_GETUPVAR)

BEGIN_CASE(JSOP_GETUPVAR_DBG)
BEGIN_CASE(JSOP_CALLUPVAR_DBG)
{
    JSFunction *fun = fp->fun;
    JS_ASSERT(FUN_KIND(fun) == JSFUN_INTERPRETED);
    JS_ASSERT(fun->u.i.wrapper);

    /* Scope for tempPool mark and local names allocation in it. */
    JSObject *obj, *obj2;
    JSProperty *prop;
    jsid id;
    JSAtom *atom;
    {
        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jsuword *names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
        if (!names)
            goto error;

        uintN index = fun->countArgsAndVars() + GET_UINT16(regs.pc);
        atom = JS_LOCAL_NAME_TO_ATOM(names[index]);
        id = ATOM_TO_JSID(atom);

        JSBool ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (!ok)
            goto error;
    }

    if (!prop) {
        atomNotDefined = atom;
        goto atom_not_defined;
    }

    /* Minimize footprint with generic code instead of NATIVE_GET. */
    obj2->dropProperty(cx, prop);
    Value *vp = regs.sp;
    PUSH_NULL();
    if (!obj->getProperty(cx, id, vp))
        goto error;

    if (op == JSOP_CALLUPVAR_DBG)
        PUSH_NULL();
}
END_CASE(JSOP_GETUPVAR_DBG)

BEGIN_CASE(JSOP_GETDSLOT)
BEGIN_CASE(JSOP_CALLDSLOT)
{
    JS_ASSERT(fp->argv);
    JSObject *obj = &fp->argv[-2].asObject();
    JS_ASSERT(obj);
    JS_ASSERT(obj->dslots);

    uintN index = GET_UINT16(regs.pc);
    JS_ASSERT(JS_INITIAL_NSLOTS + index < obj->dslots[-1].asPrivateUint32());
    JS_ASSERT_IF(obj->scope()->object == obj,
                 JS_INITIAL_NSLOTS + index < obj->scope()->freeslot);

    PUSH_COPY(obj->dslots[index]);
    if (op == JSOP_CALLDSLOT)
        PUSH_NULL();
}
END_CASE(JSOP_GETDSLOT)

BEGIN_CASE(JSOP_GETGLOBAL)
BEGIN_CASE(JSOP_CALLGLOBAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = fp->scopeChainObj()->getGlobal();
    JS_ASSERT(slot < obj->scope()->freeslot);
    PUSH_COPY(obj->getSlot(slot));
    if (op == JSOP_CALLGLOBAL)
        PUSH_NULL();
}
END_CASE(JSOP_GETGLOBAL)

BEGIN_CASE(JSOP_FORGLOBAL)
{
    Value rval;
    if (!IteratorNext(cx, &regs.sp[-1].asObject(), &rval))
        goto error;
    PUSH_COPY(rval);
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = fp->scopeChainObj()->getGlobal();
    JS_ASSERT(slot < obj->scope()->freeslot);
    JS_LOCK_OBJ(cx, obj);
    {
        JSScope *scope = obj->scope();
        if (!scope->methodWriteBarrier(cx, slot, rval)) {
            JS_UNLOCK_SCOPE(cx, scope);
            goto error;
        }
        obj->lockedSetSlot(slot, rval);
        JS_UNLOCK_SCOPE(cx, scope);
    }
    regs.sp--;
}
END_CASE(JSOP_FORGLOBAL)

BEGIN_CASE(JSOP_SETGLOBAL)
{
    uint32 slot = GET_SLOTNO(regs.pc);
    slot = script->getGlobalSlot(slot);
    JSObject *obj = fp->scopeChainObj()->getGlobal();
    JS_ASSERT(slot < obj->scope()->freeslot);
    {
        JS_LOCK_OBJ(cx, obj);
        JSScope *scope = obj->scope();
        if (!scope->methodWriteBarrier(cx, slot, regs.sp[-1])) {
            JS_UNLOCK_SCOPE(cx, scope);
            goto error;
        }
        obj->lockedSetSlot(slot, regs.sp[-1]);
        JS_UNLOCK_SCOPE(cx, scope);
    }
}
END_SET_CASE(JSOP_SETGLOBAL)

BEGIN_CASE(JSOP_DEFCONST)
BEGIN_CASE(JSOP_DEFVAR)
{
    uint32 index = GET_INDEX(regs.pc);
    JSAtom *atom = atoms[index];

    /*
     * index is relative to atoms at this point but for global var
     * code below we need the absolute value.
     */
    index += atoms - script->atomMap.vector;
    JSObject *obj = fp->varobj(cx);
    JS_ASSERT(obj->map->ops->defineProperty == js_DefineProperty);
    uintN attrs = JSPROP_ENUMERATE;
    if (!(fp->flags & JSFRAME_EVAL))
        attrs |= JSPROP_PERMANENT;
    if (op == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    /* Lookup id in order to check for redeclaration problems. */
    jsid id = ATOM_TO_JSID(atom);
    JSProperty *prop = NULL;
    JSObject *obj2;
    if (op == JSOP_DEFVAR) {
        /*
         * Redundant declaration of a |var|, even one for a non-writable
         * property like |undefined| in ES5, does nothing.
         */
        if (!obj->lookupProperty(cx, id, &obj2, &prop))
            goto error;
    } else {
        if (!CheckRedeclaration(cx, obj, id, attrs, &obj2, &prop))
            goto error;
    }

    /* Bind a variable only if it's not yet defined. */
    if (!prop) {
        if (!js_DefineNativeProperty(cx, obj, id, Value(UndefinedTag()), PropertyStub, PropertyStub,
                                     attrs, 0, 0, &prop)) {
            goto error;
        }
        JS_ASSERT(prop);
        obj2 = obj;
    }

    obj2->dropProperty(cx, prop);
}
END_CASE(JSOP_DEFVAR)

BEGIN_CASE(JSOP_DEFFUN)
{
    PropertyOp getter, setter;
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
    JSFunction *fun;
    LOAD_FUNCTION(0);
    JSObject *obj = FUN_OBJECT(fun);

    JSObject *obj2;
    if (FUN_NULL_CLOSURE(fun)) {
        /*
         * Even a null closure needs a parent for principals finding.
         * FIXME: bug 476950, although debugger users may also demand some kind
         * of scope link for debugger-assisted eval-in-frame.
         */
        obj2 = fp->scopeChainObj();
    } else {
        JS_ASSERT(!FUN_FLAT_CLOSURE(fun));

        /*
         * Inline js_GetScopeChain a bit to optimize for the case of a
         * top-level function.
         */
        if (!fp->blockChain) {
            obj2 = fp->scopeChainObj();
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
    if (obj->getParent() != obj2) {
        obj = CloneFunctionObject(cx, fun, obj2);
        if (!obj)
            goto error;
    }

    /*
     * Protect obj from any GC hiding below JSObject::setProperty or
     * JSObject::defineProperty.  All paths from here must flow through the
     * fp->scopeChain code below the parent->defineProperty call.
     */
    MUST_FLOW_THROUGH("restore_scope");
    fp->setScopeChainObj(obj);

    Value rval = FunObjTag(*obj);

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    uintN attrs = (fp->flags & JSFRAME_EVAL)
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * Load function flags that are also property attributes.  Getters and
     * setters do not need a slot, their value is stored elsewhere in the
     * property itself, not in obj slots.
     */
    getter = setter = PropertyStub;
    uintN flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        /* Function cannot be both getter a setter. */
        JS_ASSERT(flags == JSPROP_GETTER || flags == JSPROP_SETTER);
        attrs |= flags | JSPROP_SHARED;
        rval.setUndefined();
        if (flags == JSPROP_GETTER)
            getter = CastAsPropertyOp(obj);
        else
            setter = CastAsPropertyOp(obj);
    }

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    JSObject *parent = fp->varobj(cx);
    JS_ASSERT(parent);

    /*
     * Check for a const property of the same name -- or any kind of property
     * if executing with the strict option.  We check here at runtime as well
     * as at compile-time, to handle eval as well as multiple HTML script tags.
     */
    jsid id = ATOM_TO_JSID(fun->atom);
    prop = NULL;
    JSBool ok = CheckRedeclaration(cx, parent, id, attrs, &pobj, &prop);
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
            parent->getClass() == &js_CallClass &&
            (old = ((JSScopeProperty *) prop)->attributes(),
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
    /* Restore fp->scopeChain now that obj is defined in fp->callobj. */
    fp->setScopeChainObj(obj2);
    if (!ok)
        goto error;
}
END_CASE(JSOP_DEFFUN)

BEGIN_CASE(JSOP_DEFFUN_FC)
BEGIN_CASE(JSOP_DEFFUN_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = (op == JSOP_DEFFUN_FC)
                    ? js_NewFlatClosure(cx, fun)
                    : js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    Value rval = FunObjTag(*obj);

    uintN attrs = (fp->flags & JSFRAME_EVAL)
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    uintN flags = JSFUN_GSFLAG2ATTR(fun->flags);
    if (flags) {
        attrs |= flags | JSPROP_SHARED;
        rval.setUndefined();
    }

    JSObject *parent = fp->varobj(cx);
    JS_ASSERT(parent);

    jsid id = ATOM_TO_JSID(fun->atom);
    JSBool ok = CheckRedeclaration(cx, parent, id, attrs, NULL, NULL);
    if (ok) {
        if (attrs == JSPROP_ENUMERATE) {
            JS_ASSERT(fp->flags & JSFRAME_EVAL);
            ok = parent->setProperty(cx, id, &rval);
        } else {
            JS_ASSERT(attrs & JSPROP_PERMANENT);

            ok = parent->defineProperty(cx, id, rval,
                                        (flags & JSPROP_GETTER)
                                        ? CastAsPropertyOp(obj)
                                        : PropertyStub,
                                        (flags & JSPROP_SETTER)
                                        ? CastAsPropertyOp(obj)
                                        : PropertyStub,
                                        attrs);
        }
    }

    if (!ok)
        goto error;
}
END_CASE(JSOP_DEFFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN)
{
    /*
     * Define a local function (i.e., one nested at the top level of another
     * function), parented by the current scope chain, stored in a local
     * variable slot that the compiler allocated.  This is an optimization over
     * JSOP_DEFFUN that avoids requiring a call object for the outer function's
     * activation.
     */
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!FUN_FLAT_CLOSURE(fun));
    JSObject *obj = FUN_OBJECT(fun);

    if (FUN_NULL_CLOSURE(fun)) {
        obj = CloneFunctionObject(cx, fun, fp->scopeChainObj());
        if (!obj)
            goto error;
    } else {
        JSObject *parent = js_GetScopeChain(cx, fp);
        if (!parent)
            goto error;

        if (obj->getParent() != parent) {
#ifdef JS_TRACER
            if (TRACE_RECORDER(cx))
                AbortRecording(cx, "DEFLOCALFUN for closure");
#endif
            obj = CloneFunctionObject(cx, fun, parent);
            if (!obj)
                goto error;
        }
    }

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots()[slot].setFunObj(*obj);
}
END_CASE(JSOP_DEFLOCALFUN)

BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    uint32 slot = GET_SLOTNO(regs.pc);
    TRACE_2(DefLocalFunSetSlot, slot, obj);

    fp->slots()[slot].setFunObj(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_FC)

BEGIN_CASE(JSOP_DEFLOCALFUN_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(SLOTNO_LEN);

    JSObject *obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    uint32 slot = GET_SLOTNO(regs.pc);
    fp->slots()[slot].setFunObj(*obj);
}
END_CASE(JSOP_DEFLOCALFUN_DBGFC)

BEGIN_CASE(JSOP_LAMBDA)
{
    /* Load the specified function object literal. */
    JSFunction *fun;
    LOAD_FUNCTION(0);
    JSObject *obj = FUN_OBJECT(fun);

    /* do-while(0) so we can break instead of using a goto. */
    do {
        JSObject *parent;
        if (FUN_NULL_CLOSURE(fun)) {
            parent = fp->scopeChainObj();

            if (obj->getParent() == parent) {
                op = JSOp(regs.pc[JSOP_LAMBDA_LENGTH]);

                /*
                 * Optimize ({method: function () { ... }, ...}) and
                 * this.method = function () { ... }; bytecode sequences.
                 */
                if (op == JSOP_SETMETHOD) {
#ifdef DEBUG
                    JSOp op2 = JSOp(regs.pc[JSOP_LAMBDA_LENGTH + JSOP_SETMETHOD_LENGTH]);
                    JS_ASSERT(op2 == JSOP_POP || op2 == JSOP_POPV);
#endif

                    const Value &lref = regs.sp[-1];
                    if (lref.isObject() &&
                        lref.asObject().getClass() == &js_ObjectClass) {
                        break;
                    }
                } else if (op == JSOP_INITMETHOD) {
#ifdef DEBUG
                    const Value &lref = regs.sp[-1];
                    JS_ASSERT(lref.isObject());
                    JSObject *obj2 = &lref.asObject();
                    JS_ASSERT(obj2->getClass() == &js_ObjectClass);
                    JS_ASSERT(obj2->scope()->object == obj2);
#endif
                    break;
                }
            }
        } else {
            parent = js_GetScopeChain(cx, fp);
            if (!parent)
                goto error;
        }

        obj = CloneFunctionObject(cx, fun, parent);
        if (!obj)
            goto error;
    } while (0);

    PUSH_FUNOBJ(*obj);
}
END_CASE(JSOP_LAMBDA)

BEGIN_CASE(JSOP_LAMBDA_FC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_FUNOBJ(*obj);
}
END_CASE(JSOP_LAMBDA_FC)

BEGIN_CASE(JSOP_LAMBDA_DBGFC)
{
    JSFunction *fun;
    LOAD_FUNCTION(0);

    JSObject *obj = js_NewDebuggableFlatClosure(cx, fun);
    if (!obj)
        goto error;

    PUSH_FUNOBJ(*obj);
}
END_CASE(JSOP_LAMBDA_DBGFC)

BEGIN_CASE(JSOP_CALLEE)
    PUSH_COPY(fp->argv[-2]);
END_CASE(JSOP_CALLEE)

BEGIN_CASE(JSOP_GETTER)
BEGIN_CASE(JSOP_SETTER)
{
  do_getter_setter:
    JSOp op2 = (JSOp) *++regs.pc;
    jsid id;
    Value rval;
    jsint i;
    JSObject *obj;
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
      {
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
        rval = regs.sp[-1];
        i = -1;
        goto gs_pop_lval;
      }
      case JSOP_SETELEM:
        rval = regs.sp[-1];
        id = JSID_VOID;
        i = -2;
      gs_pop_lval:
        FETCH_OBJECT(cx, i - 1, obj);
        break;

      case JSOP_INITPROP:
      {
        JS_ASSERT(regs.sp - fp->base() >= 2);
        rval = regs.sp[-1];
        i = -1;
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        id = ATOM_TO_JSID(atom);
        goto gs_get_lval;
      }
      default:
        JS_ASSERT(op2 == JSOP_INITELEM);

        JS_ASSERT(regs.sp - fp->base() >= 3);
        rval = regs.sp[-1];
        id = JSID_VOID;
        i = -2;
      gs_get_lval:
      {
        const Value &lref = regs.sp[i-1];
        JS_ASSERT(lref.isObject());
        obj = &lref.asObject();
        break;
      }
    }

    /* Ensure that id has a type suitable for use with obj. */
    if (JSID_IS_VOID(id))
        FETCH_ELEMENT_ID(obj, i, id);

    if (!js_IsCallable(rval)) {
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
    Value rtmp;
    uintN attrs;
    if (!obj->checkAccess(cx, id, JSACC_WATCH, &rtmp, &attrs))
        goto error;

    PropertyOp getter, setter;
    if (op == JSOP_GETTER) {
        getter = CastAsPropertyOp(&rval.asObject());
        setter = PropertyStub;
        attrs = JSPROP_GETTER;
    } else {
        getter = PropertyStub;
        setter = CastAsPropertyOp(&rval.asObject());
        attrs = JSPROP_SETTER;
    }
    attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

    /* Check for a readonly or permanent property of the same name. */
    if (!CheckRedeclaration(cx, obj, id, attrs, NULL, NULL))
        goto error;

    if (!obj->defineProperty(cx, id, Value(UndefinedTag()), getter, setter, attrs))
        goto error;

    regs.sp += i;
    if (js_CodeSpec[op2].ndefs > js_CodeSpec[op2].nuses) {
        JS_ASSERT(js_CodeSpec[op2].ndefs == js_CodeSpec[op2].nuses + 1);
        regs.sp[-1] = rval;
    }
    len = js_CodeSpec[op2].length;
    DO_NEXT_OP(len);
}

BEGIN_CASE(JSOP_HOLE)
    PUSH_HOLE();
END_CASE(JSOP_HOLE)

BEGIN_CASE(JSOP_NEWARRAY)
{
    len = GET_UINT16(regs.pc);
    cx->assertValidStackDepth(len);
    JSObject *obj = js_NewArrayObject(cx, len, regs.sp - len, JS_TRUE);
    if (!obj)
        goto error;
    regs.sp -= len - 1;
    regs.sp[-1].setNonFunObj(*obj);
}
END_CASE(JSOP_NEWARRAY)

BEGIN_CASE(JSOP_NEWINIT)
{
    jsint i = GET_INT8(regs.pc);
    JS_ASSERT(i == JSProto_Array || i == JSProto_Object);
    JSObject *obj;
    if (i == JSProto_Array) {
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
    } else {
        obj = NewObject(cx, &js_ObjectClass, NULL, NULL);
        if (!obj)
            goto error;

        if (regs.pc[JSOP_NEWINIT_LENGTH] != JSOP_ENDINIT) {
            JS_LOCK_OBJ(cx, obj);
            JSScope *scope = js_GetMutableScope(cx, obj);
            if (!scope) {
                JS_UNLOCK_OBJ(cx, obj);
                goto error;
            }

            /*
             * We cannot assume that js_GetMutableScope above creates a scope
             * owned by cx and skip JS_UNLOCK_SCOPE. A new object debugger
             * hook may add properties to the newly created object, suspend
             * the current request and share the object with other threads.
             */
            JS_UNLOCK_SCOPE(cx, scope);
        }
    }

    PUSH_NONFUNOBJ(*obj);
    CHECK_INTERRUPT_HANDLER();
}
END_CASE(JSOP_NEWINIT)

BEGIN_CASE(JSOP_ENDINIT)
{
    /* Re-set the newborn root to the top of this object tree. */
    JS_ASSERT(regs.sp - fp->base() >= 1);
    const Value &lref = regs.sp[-1];
    JS_ASSERT(lref.isObject());
    cx->weakRoots.finalizableNewborns[FINALIZE_OBJECT] = &lref.asObject();
}
END_CASE(JSOP_ENDINIT)

BEGIN_CASE(JSOP_INITPROP)
BEGIN_CASE(JSOP_INITMETHOD)
{
    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.sp - fp->base() >= 2);
    Value rval = regs.sp[-1];

    /* Load the object being initialized into lval/obj. */
    JSObject *obj = &regs.sp[-2].asObject();
    JS_ASSERT(obj->isNative());
    JS_ASSERT(!obj->getClass()->reserveSlots);

    JSScope *scope = obj->scope();

    /*
     * Probe the property cache. 
     *
     * We can not assume that the object created by JSOP_NEWINIT is still
     * single-threaded as the debugger can access it from other threads.
     * So check first.
     *
     * On a hit, if the cached sprop has a non-default setter, it must be
     * __proto__. If sprop->parent != scope->lastProperty(), there is a
     * repeated property name. The fast path does not handle these two cases.
     */
    PropertyCacheEntry *entry;
    JSScopeProperty *sprop;
    if (CX_OWNS_OBJECT_TITLE(cx, obj) &&
        JS_PROPERTY_CACHE(cx).testForInit(rt, regs.pc, obj, scope, &sprop, &entry) &&
        sprop->hasDefaultSetter() &&
        sprop->parent == scope->lastProperty())
    {
        /* Fast path. Property cache hit. */
        uint32 slot = sprop->slot;
        JS_ASSERT(slot == scope->freeslot);
        if (slot < obj->numSlots()) {
            ++scope->freeslot;
        } else {
            if (!js_AllocSlot(cx, obj, &slot))
                goto error;
            JS_ASSERT(slot == sprop->slot);
        }

        JS_ASSERT(!scope->lastProperty() ||
                  scope->shape == scope->lastProperty()->shape);
        if (scope->table) {
            JSScopeProperty *sprop2 =
                scope->addProperty(cx, sprop->id, sprop->getter(), sprop->setter(), slot,
                                   sprop->attributes(), sprop->getFlags(), sprop->shortid);
            if (!sprop2) {
                js_FreeSlot(cx, obj, slot);
                goto error;
            }
            JS_ASSERT(sprop2 == sprop);
        } else {
            JS_ASSERT(!scope->isSharedEmpty());
            scope->extend(cx, sprop);
        }

        /*
         * No method change check here because here we are adding a new
         * property, not updating an existing slot's value that might
         * contain a method of a branded scope.
         */
        TRACE_2(SetPropHit, entry, sprop);
        obj->lockedSetSlot(slot, rval);
    } else {
        PCMETER(JS_PROPERTY_CACHE(cx).inipcmisses++);

        /* Get the immediate property name into id. */
        JSAtom *atom;
        LOAD_ATOM(0, atom);
        jsid id = ATOM_TO_JSID(atom);

        /* Set the property named by obj[id] to rval. */
        if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
            goto error;

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
    }

    /* Common tail for property cache hit and miss cases. */
    regs.sp--;
}
END_CASE(JSOP_INITPROP);

BEGIN_CASE(JSOP_INITELEM)
{
    /* Pop the element's value into rval. */
    JS_ASSERT(regs.sp - fp->base() >= 3);
    const Value &rref = regs.sp[-1];

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    JSObject *obj = &lref.asObject();

    /* Fetch id now that we have obj. */
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);

    /*
     * Check for property redeclaration strict warning (we may be in an object
     * initialiser, not an array initialiser).
     */
    if (!CheckRedeclaration(cx, obj, id, JSPROP_INITIALIZER, NULL, NULL))
        goto error;

    /*
     * If rref is a hole, do not call JSObject::defineProperty. In this case,
     * obj must be an array, so if the current op is the last element
     * initialiser, set the array length to one greater than id.
     */
    if (rref.isMagic(JS_ARRAY_HOLE)) {
        JS_ASSERT(obj->isArray());
        JS_ASSERT(JSID_IS_INT(id));
        JS_ASSERT(jsuint(JSID_TO_INT(id)) < JS_ARGS_LENGTH_MAX);
        if (js_GetOpcode(cx, script, regs.pc + JSOP_INITELEM_LENGTH) == JSOP_ENDINIT &&
            !js_SetLengthProperty(cx, obj, (jsuint) (JSID_TO_INT(id) + 1))) {
            goto error;
        }
    } else {
        if (!obj->defineProperty(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
            goto error;
    }
    regs.sp -= 2;
}
END_CASE(JSOP_INITELEM)

#if JS_HAS_SHARP_VARS

BEGIN_CASE(JSOP_DEFSHARP)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    const Value &lref = fp->slots()[slot];
    JSObject *obj;
    if (lref.isObject()) {
        obj = &lref.asObject();
    } else {
        JS_ASSERT(lref.isUndefined());
        obj = js_NewArrayObject(cx, 0, NULL);
        if (!obj)
            goto error;
        fp->slots()[slot].setNonFunObj(*obj);
    }
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    jsid id = INT_TO_JSID(i);
    const Value &rref = regs.sp[-1];
    if (rref.isPrimitive()) {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_DEF, numBuf);
        goto error;
    }
    if (!obj->defineProperty(cx, id, rref, NULL, NULL, JSPROP_ENUMERATE))
        goto error;
}
END_CASE(JSOP_DEFSHARP)

BEGIN_CASE(JSOP_USESHARP)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    const Value &lref = fp->slots()[slot];
    jsint i = (jsint) GET_UINT16(regs.pc + UINT16_LEN);
    Value rval;
    if (lref.isUndefined()) {
        rval.setUndefined();
    } else {
        JSObject *obj = &fp->slots()[slot].asObject();
        jsid id = INT_TO_JSID(i);
        if (!obj->getProperty(cx, id, &rval))
            goto error;
    }
    if (!rval.isObjectOrNull()) {
        char numBuf[12];

        JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_SHARP_USE, numBuf);
        goto error;
    }
    PUSH_COPY(rval);
}
END_CASE(JSOP_USESHARP)

BEGIN_CASE(JSOP_SHARPINIT)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < fp->script->nfixed);
    Value *vp = &fp->slots()[slot];
    Value rval = vp[1];

    /*
     * We peek ahead safely here because empty initialisers get zero
     * JSOP_SHARPINIT ops, and non-empty ones get two: the first comes
     * immediately after JSOP_NEWINIT followed by one or more property
     * initialisers; and the second comes directly before JSOP_ENDINIT.
     */
    if (regs.pc[JSOP_SHARPINIT_LENGTH] != JSOP_ENDINIT) {
        rval.setInt32(rval.isUndefined() ? 1 : rval.asInt32() + 1);
    } else {
        JS_ASSERT(rval.isInt32());
        rval.asInt32Ref() -= 1;
        if (rval.asInt32() == 0)
            vp[0].setUndefined();
    }
    vp[1] = rval;
}
END_CASE(JSOP_SHARPINIT)

#endif /* JS_HAS_SHARP_VARS */

{
BEGIN_CASE(JSOP_GOSUB)
    PUSH_BOOLEAN(false);
    jsint i = (regs.pc - script->main) + JSOP_GOSUB_LENGTH;
    PUSH_INT32(i);
    len = GET_JUMP_OFFSET(regs.pc);
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_GOSUBX)
    PUSH_BOOLEAN(false);
    jsint i = (regs.pc - script->main) + JSOP_GOSUBX_LENGTH;
    len = GET_JUMPX_OFFSET(regs.pc);
    PUSH_INT32(i);
END_VARLEN_CASE
}

{
BEGIN_CASE(JSOP_RETSUB)
    /* Pop [exception or hole, retsub pc-index]. */
    Value rval, lval;
    POP_COPY_TO(rval);
    POP_COPY_TO(lval);
    JS_ASSERT(lval.isBoolean());
    if (lval.asBoolean()) {
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
    JS_ASSERT(rval.isInt32());
    len = rval.asInt32();
    regs.pc = script->main;
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_EXCEPTION)
    JS_ASSERT(cx->throwing);
    PUSH_COPY(cx->exception);
    cx->throwing = JS_FALSE;
    CHECK_BRANCH();
END_CASE(JSOP_EXCEPTION)

BEGIN_CASE(JSOP_FINALLY)
    CHECK_BRANCH();
END_CASE(JSOP_FINALLY)

BEGIN_CASE(JSOP_THROWING)
    JS_ASSERT(!cx->throwing);
    cx->throwing = JS_TRUE;
    POP_COPY_TO(cx->exception);
END_CASE(JSOP_THROWING)

BEGIN_CASE(JSOP_THROW)
    JS_ASSERT(!cx->throwing);
    CHECK_BRANCH();
    cx->throwing = JS_TRUE;
    POP_COPY_TO(cx->exception);
    /* let the code at error try to catch the exception. */
    goto error;

BEGIN_CASE(JSOP_SETLOCALPOP)
{
    /*
     * The stack must have a block with at least one local slot below the
     * exception object.
     */
    JS_ASSERT((size_t) (regs.sp - fp->base()) >= 2);
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(slot + 1 < script->nslots);
    POP_COPY_TO(fp->slots()[slot]);
}
END_CASE(JSOP_SETLOCALPOP)

BEGIN_CASE(JSOP_IFPRIMTOP)
    /*
     * If the top of stack is of primitive type, jump to our target. Otherwise
     * advance to the next opcode.
     */
    JS_ASSERT(regs.sp > fp->base());
    if (regs.sp[-1].isPrimitive()) {
        len = GET_JUMP_OFFSET(regs.pc);
        BRANCH(len);
    }
END_CASE(JSOP_IFPRIMTOP)

BEGIN_CASE(JSOP_PRIMTOP)
    JS_ASSERT(regs.sp > fp->base());
    if (regs.sp[-1].isObject()) {
        jsint i = GET_INT8(regs.pc);
        js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO, -2, regs.sp[-2], NULL,
                             (i == JSTYPE_VOID) ? "primitive type" : JS_TYPE_STR(i));
        goto error;
    }
END_CASE(JSOP_PRIMTOP)

BEGIN_CASE(JSOP_OBJTOP)
    if (regs.sp[-1].isPrimitive()) {
        js_ReportValueError(cx, GET_UINT16(regs.pc), -1, regs.sp[-1], NULL);
        goto error;
    }
END_CASE(JSOP_OBJTOP)

BEGIN_CASE(JSOP_INSTANCEOF)
{
    const Value &rref = regs.sp[-1];
    JSObject *obj;
    if (rref.isPrimitive() ||
        !(obj = &rref.asObject())->map->ops->hasInstance) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                            -1, rref, NULL);
        goto error;
    }
    const Value &lref = regs.sp[-2];
    JSBool cond = JS_FALSE;
    if (!obj->map->ops->hasInstance(cx, obj, &lref, &cond))
        goto error;
    regs.sp--;
    regs.sp[-1].setBoolean(cond);
}
END_CASE(JSOP_INSTANCEOF)

#if JS_HAS_DEBUGGER_KEYWORD
BEGIN_CASE(JSOP_DEBUGGER)
{
    JSDebuggerHandler handler = cx->debugHooks->debuggerHandler;
    if (handler) {
        Value rval;
        switch (handler(cx, script, regs.pc, Jsvalify(&rval), cx->debugHooks->debuggerHandlerData)) {
        case JSTRAP_ERROR:
            goto error;
        case JSTRAP_CONTINUE:
            break;
        case JSTRAP_RETURN:
            fp->rval = rval;
            interpReturnOK = JS_TRUE;
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
{
    Value rval;
    POP_COPY_TO(rval);
    if (!js_SetDefaultXMLNamespace(cx, rval))
        goto error;
}
END_CASE(JSOP_DEFXMLNS)

BEGIN_CASE(JSOP_ANYNAME)
{
    jsid id;
    if (!js_GetAnyName(cx, &id))
        goto error;
    PUSH_COPY(IdToValue(id));
}
END_CASE(JSOP_ANYNAME)

BEGIN_CASE(JSOP_QNAMEPART)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    PUSH_STRING(ATOM_TO_STRING(atom));
}
END_CASE(JSOP_QNAMEPART)

BEGIN_CASE(JSOP_QNAMECONST)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    Value rval = StringTag(ATOM_TO_STRING(atom));
    Value lval = regs.sp[-1];
    JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_QNAMECONST)

BEGIN_CASE(JSOP_QNAME)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
    if (!obj)
        goto error;
    regs.sp--;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_QNAME)

BEGIN_CASE(JSOP_TOATTRNAME)
{
    Value rval;
    rval = regs.sp[-1];
    if (!js_ToAttributeName(cx, &rval))
        goto error;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_TOATTRNAME)

BEGIN_CASE(JSOP_TOATTRVAL)
{
    Value rval;
    rval = regs.sp[-1];
    JS_ASSERT(rval.isString());
    JSString *str = js_EscapeAttributeValue(cx, rval.asString(), JS_FALSE);
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_TOATTRVAL)

BEGIN_CASE(JSOP_ADDATTRNAME)
BEGIN_CASE(JSOP_ADDATTRVAL)
{
    Value rval = regs.sp[-1];
    Value lval = regs.sp[-2];
    JSString *str = lval.asString();
    JSString *str2 = rval.asString();
    str = js_AddAttributePart(cx, op == JSOP_ADDATTRNAME, str, str2);
    if (!str)
        goto error;
    regs.sp--;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_ADDATTRNAME)

BEGIN_CASE(JSOP_BINDXMLNAME)
{
    Value lval;
    lval = regs.sp[-1];
    JSObject *obj;
    jsid id;
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    regs.sp[-1].setObjectOrNull(obj);
    PUSH_COPY(IdToValue(id));
}
END_CASE(JSOP_BINDXMLNAME)

BEGIN_CASE(JSOP_SETXMLNAME)
{
    JSObject *obj = &regs.sp[-3].asObject();
    Value rval = regs.sp[-1];
    jsid id;
    FETCH_ELEMENT_ID(obj, -2, id);
    if (!obj->setProperty(cx, id, &rval))
        goto error;
    rval = regs.sp[-1];
    regs.sp -= 2;
    regs.sp[-1] = rval;
}
END_CASE(JSOP_SETXMLNAME)

BEGIN_CASE(JSOP_CALLXMLNAME)
BEGIN_CASE(JSOP_XMLNAME)
{
    Value lval = regs.sp[-1];
    JSObject *obj;
    jsid id;
    if (!js_FindXMLProperty(cx, lval, &obj, &id))
        goto error;
    Value rval;
    if (!obj->getProperty(cx, id, &rval))
        goto error;
    regs.sp[-1] = rval;
    if (op == JSOP_CALLXMLNAME)
        PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLNAME)

BEGIN_CASE(JSOP_DESCENDANTS)
BEGIN_CASE(JSOP_DELDESC)
{
    JSObject *obj;
    FETCH_OBJECT(cx, -2, obj);
    jsval rval = Jsvalify(regs.sp[-1]);
    if (!js_GetXMLDescendants(cx, obj, rval, &rval))
        goto error;

    if (op == JSOP_DELDESC) {
        regs.sp[-1] = Valueify(rval);   /* set local root */
        if (!js_DeleteXMLListElements(cx, JSVAL_TO_OBJECT(rval)))
            goto error;
        rval = JSVAL_TRUE;                  /* always succeed */
    }

    regs.sp--;
    regs.sp[-1] = Valueify(rval);
}
END_CASE(JSOP_DESCENDANTS)

{
BEGIN_CASE(JSOP_FILTER)
    /*
     * We push the hole value before jumping to [enditer] so we can detect the
     * first iteration and direct js_StepXMLListFilter to initialize filter's
     * state.
     */
    PUSH_HOLE();
    len = GET_JUMP_OFFSET(regs.pc);
    JS_ASSERT(len > 0);
END_VARLEN_CASE
}

BEGIN_CASE(JSOP_ENDFILTER)
{
    bool cond = !regs.sp[-1].isMagic();
    if (cond) {
        /* Exit the "with" block left from the previous iteration. */
        js_LeaveWith(cx);
    }
    if (!js_StepXMLListFilter(cx, cond))
        goto error;
    if (!regs.sp[-1].isNull()) {
        /*
         * Decrease sp after EnterWith returns as we use sp[-1] there to root
         * temporaries.
         */
        JS_ASSERT(IsXML(regs.sp[-1]));
        if (!js_EnterWith(cx, -2))
            goto error;
        regs.sp--;
        len = GET_JUMP_OFFSET(regs.pc);
        JS_ASSERT(len < 0);
        BRANCH(len);
    }
    regs.sp--;
}
END_CASE(JSOP_ENDFILTER);

BEGIN_CASE(JSOP_TOXML)
{
    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXML)

BEGIN_CASE(JSOP_TOXMLLIST)
{
    Value rval = regs.sp[-1];
    JSObject *obj = js_ValueToXMLListObject(cx, rval);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_TOXMLLIST)

BEGIN_CASE(JSOP_XMLTAGEXPR)
{
    Value rval = regs.sp[-1];
    JSString *str = js_ValueToString(cx, rval);
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_XMLTAGEXPR)

BEGIN_CASE(JSOP_XMLELTEXPR)
{
    Value rval = regs.sp[-1];
    JSString *str;
    if (IsXML(rval)) {
        str = js_ValueToXMLString(cx, rval);
    } else {
        str = js_ValueToString(cx, rval);
        if (str)
            str = js_EscapeElementValue(cx, str);
    }
    if (!str)
        goto error;
    regs.sp[-1].setString(str);
}
END_CASE(JSOP_XMLELTEXPR)

BEGIN_CASE(JSOP_XMLOBJECT)
{
    JSObject *obj;
    LOAD_OBJECT(0, obj);
    obj = js_CloneXMLObject(cx, obj);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLOBJECT)

BEGIN_CASE(JSOP_XMLCDATA)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_TEXT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCDATA)

BEGIN_CASE(JSOP_XMLCOMMENT)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_COMMENT, NULL, str);
    if (!obj)
        goto error;
    PUSH_OBJECT(*obj);
}
END_CASE(JSOP_XMLCOMMENT)

BEGIN_CASE(JSOP_XMLPI)
{
    JSAtom *atom;
    LOAD_ATOM(0, atom);
    JSString *str = ATOM_TO_STRING(atom);
    Value rval = regs.sp[-1];
    JSString *str2 = rval.asString();
    JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_PROCESSING_INSTRUCTION, str, str2);
    if (!obj)
        goto error;
    regs.sp[-1].setObject(*obj);
}
END_CASE(JSOP_XMLPI)

BEGIN_CASE(JSOP_GETFUNNS)
{
    Value rval;
    if (!js_GetFunctionNamespace(cx, &rval))
        goto error;
    PUSH_COPY(rval);
}
END_CASE(JSOP_GETFUNNS)
#endif /* JS_HAS_XML_SUPPORT */

BEGIN_CASE(JSOP_ENTERBLOCK)
{
    JSObject *obj;
    LOAD_OBJECT(0, obj);
    JS_ASSERT(!OBJ_IS_CLONED_BLOCK(obj));
    JS_ASSERT(fp->base() + OBJ_BLOCK_DEPTH(cx, obj) == regs.sp);
    Value *vp = regs.sp + OBJ_BLOCK_COUNT(cx, obj);
    JS_ASSERT(regs.sp < vp);
    JS_ASSERT(vp <= fp->slots() + script->nslots);
    SetValueRangeToUndefined(regs.sp, vp);
    regs.sp = vp;

#ifdef DEBUG
    JS_ASSERT(fp->blockChain == obj->getParent());

    /*
     * The young end of fp->scopeChain may omit blocks if we haven't closed
     * over them, but if there are any closure blocks on fp->scopeChain, they'd
     * better be (clones of) ancestors of the block we're entering now;
     * anything else we should have popped off fp->scopeChain when we left its
     * static scope.
     */
    JSObject *obj2 = fp->scopeChainObj();
    Class *clasp;
    while ((clasp = obj2->getClass()) == &js_WithClass)
        obj2 = obj2->getParent();
    if (clasp == &js_BlockClass &&
        obj2->getPrivate() == js_FloatingFrameIfGenerator(cx, fp)) {
        JSObject *youngestProto = obj2->getProto();
        JS_ASSERT(!OBJ_IS_CLONED_BLOCK(youngestProto));
        JSObject *parent = obj;
        while ((parent = parent->getParent()) != youngestProto)
            JS_ASSERT(parent);
    }
#endif

    fp->blockChain = obj;
}
END_CASE(JSOP_ENTERBLOCK)

BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
BEGIN_CASE(JSOP_LEAVEBLOCK)
{
#ifdef DEBUG
    JS_ASSERT(fp->blockChain->getClass() == &js_BlockClass);
    uintN blockDepth = OBJ_BLOCK_DEPTH(cx, fp->blockChain);

    JS_ASSERT(blockDepth <= StackDepth(script));
#endif
    /*
     * If we're about to leave the dynamic scope of a block that has been
     * cloned onto fp->scopeChain, clear its private data, move its locals from
     * the stack into the clone, and pop it off the chain.
     */
    JSObject *obj = fp->scopeChainObj();
    if (obj->getProto() == fp->blockChain) {
        JS_ASSERT(obj->getClass() == &js_BlockClass);
        if (!js_PutBlockObject(cx, JS_TRUE))
            goto error;
    }

    /* Pop the block chain, too.  */
    fp->blockChain = fp->blockChain->getParent();

    /* Move the result of the expression to the new topmost stack slot. */
    Value *vp = NULL;  /* silence GCC warnings */
    if (op == JSOP_LEAVEBLOCKEXPR)
        vp = &regs.sp[-1];
    regs.sp -= GET_UINT16(regs.pc);
    if (op == JSOP_LEAVEBLOCKEXPR) {
        JS_ASSERT(fp->base() + blockDepth == regs.sp - 1);
        regs.sp[-1] = *vp;
    } else {
        JS_ASSERT(fp->base() + blockDepth == regs.sp);
    }
}
END_CASE(JSOP_LEAVEBLOCK)

#if JS_HAS_GENERATORS
BEGIN_CASE(JSOP_GENERATOR)
{
    ASSERT_NOT_THROWING(cx);
    regs.pc += JSOP_GENERATOR_LENGTH;
    JSObject *obj = js_NewGenerator(cx);
    if (!obj)
        goto error;
    JS_ASSERT(!fp->callobj && !fp->argsObj());
    fp->rval.setNonFunObj(*obj);
    interpReturnOK = true;
    if (inlineCallCount != 0)
        goto inline_return;
    goto exit;
}

BEGIN_CASE(JSOP_YIELD)
    ASSERT_NOT_THROWING(cx);
    if (cx->generatorFor(fp)->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD,
                            JSDVG_SEARCH_STACK, fp->argv[-2], NULL);
        goto error;
    }
    fp->rval = regs.sp[-1];
    fp->flags |= JSFRAME_YIELDING;
    regs.pc += JSOP_YIELD_LENGTH;
    interpReturnOK = JS_TRUE;
    goto exit;

BEGIN_CASE(JSOP_ARRAYPUSH)
{
    uint32 slot = GET_UINT16(regs.pc);
    JS_ASSERT(script->nfixed <= slot);
    JS_ASSERT(slot < script->nslots);
    JSObject *obj = &fp->slots()[slot].asObject();
    if (!js_ArrayCompPush(cx, obj, regs.sp[-1]))
        goto error;
    regs.sp--;
}
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
