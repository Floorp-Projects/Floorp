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
 *   David Mandelin <dmandelin@mozilla.com>
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

#define __STDC_LIMIT_MACROS

#include "jscntxt.h"
#include "jsscope.h"
#include "jsobj.h"
#include "jslibmath.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsxml.h"
#include "jsstaticcheck.h"
#include "jsbool.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "jsiter.h"
#include "jstypes.h"
#include "methodjit/StubCalls.h"
#include "jstracer.h"
#include "jspropertycache.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"
#include "jsobjinlines.h"
#include "jscntxtinlines.h"
#include "jsatominlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

#define THROW()  \
    do {         \
        void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline); \
        f.setReturnAddress(ReturnAddressPtr(FunctionPtr(ptr))); \
        return;  \
    } while (0)

#define THROWV(v)       \
    do {                \
        void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline); \
        f.setReturnAddress(ReturnAddressPtr(FunctionPtr(ptr))); \
        return v;       \
    } while (0)

static bool
InlineReturn(JSContext *cx, JSBool ok);

static jsbytecode *
FindExceptionHandler(JSContext *cx)
{
    JSStackFrame *fp = cx->fp;
    JSScript *script = fp->script;

top:
    if (cx->throwing && script->trynotesOffset) {
        // The PC is updated before every stub call, so we can use it here.
        unsigned offset = cx->regs->pc - script->main;

        JSTryNoteArray *tnarray = script->trynotes();
        for (unsigned i = 0; i < tnarray->length; ++i) {
            JSTryNote *tn = &tnarray->vector[i];
            JS_ASSERT(offset < script->length);
            if (offset - tn->start >= tn->length)
                continue;
            if (tn->stackDepth > cx->regs->sp - fp->base())
                continue;

            jsbytecode *pc = script->main + tn->start + tn->length;
            JSBool ok = js_UnwindScope(cx, tn->stackDepth, JS_TRUE);
            JS_ASSERT(cx->regs->sp == fp->base() + tn->stackDepth);

            switch (tn->kind) {
                case JSTRY_CATCH:
                  JS_ASSERT(js_GetOpcode(cx, fp->script, pc) == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                  /* Catch cannot intercept the closing of a generator. */
                  if (JS_UNLIKELY(cx->exception.isMagic(JS_GENERATOR_CLOSING)))
                      break;
#endif

                  /*
                   * Don't clear cx->throwing to save cx->exception from GC
                   * until it is pushed to the stack via [exception] in the
                   * catch block.
                   */
                  return pc;

                case JSTRY_FINALLY:
                  /*
                   * Push (true, exception) pair for finally to indicate that
                   * [retsub] should rethrow the exception.
                   */
                  cx->regs->sp[0].setBoolean(true);
                  cx->regs->sp[1] = cx->exception;
                  cx->regs->sp += 2;
                  cx->throwing = JS_FALSE;
                  return pc;

                case JSTRY_ITER:
                {
                  /*
                   * This is similar to JSOP_ENDITER in the interpreter loop,
                   * except the code now uses the stack slot normally used by
                   * JSOP_NEXTITER, namely regs.sp[-1] before the regs.sp -= 2
                   * adjustment and regs.sp[1] after, to save and restore the
                   * pending exception.
                   */
                  AutoValueRooter tvr(cx, cx->exception);
                  JS_ASSERT(js_GetOpcode(cx, fp->script, pc) == JSOP_ENDITER);
                  cx->throwing = JS_FALSE;
                  ok = !!js_CloseIterator(cx, cx->regs->sp[-1]);
                  cx->regs->sp -= 1;
                  if (!ok)
                      goto top;
                  cx->throwing = JS_TRUE;
                  cx->exception = tvr.value();
                }
            }
        }
    }

    return NULL;
}

static inline bool
CreateFrame(VMFrame &f, uint32 flags, uint32 argc)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp;
    Value *vp = f.regs.sp - (argc + 2);
    JSObject *funobj = &vp->asObject();
    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);

    JS_ASSERT(FUN_INTERPRETED(fun));

    JSScript *newscript = fun->u.i.script;

    if (f.inlineCallCount >= JS_MAX_INLINE_CALL_COUNT) {
        js_ReportOverRecursed(cx);
        return false;
    }

    /* Allocate the frame. */
    StackSpace &stack = cx->stack();
    uintN nslots = newscript->nslots;
    uintN funargs = fun->nargs;
    Value *argv = vp + 2;
    JSStackFrame *newfp;
    if (argc < funargs) {
        uintN missing = funargs - argc;
        newfp = stack.getInlineFrame(cx, f.regs.sp, missing, nslots);
        if (!newfp)
            return false;
        for (Value *v = argv + argc, *end = v + missing; v != end; ++v)
            v->setUndefined();
    } else {
        newfp = stack.getInlineFrame(cx, f.regs.sp, 0, nslots);
        if (!newfp)
            return false;
    }

    /* Initialize the frame. */
    newfp->ncode = NULL;
    newfp->callobj = NULL;
    newfp->argsval.setNull();
    newfp->script = newscript;
    newfp->fun = fun;
    newfp->argc = argc;
    newfp->argv = vp + 2;
    newfp->rval.setUndefined();
    newfp->annotation = NULL;
    newfp->scopeChain.setObject(*funobj->getParent());
    newfp->flags = flags;
    newfp->blockChain = NULL;
    JS_ASSERT(!JSFUN_BOUND_METHOD_TEST(fun->flags));
    newfp->thisv = vp[1];
    newfp->imacpc = NULL;

    /* Push void to initialize local variables. */
    Value *newslots = newfp->slots();
    Value *newsp = newslots + fun->u.i.nvars;
    for (Value *v = newslots; v != newsp; ++v)
        v->setUndefined();

    /* Scope with a call object parented by callee's parent. */
    if (fun->isHeavyweight() && !js_GetCallObject(cx, newfp))
        return false;

    /* :TODO: Switch version if currentVersion wasn't overridden. */
    newfp->callerVersion = (JSVersion)cx->version;

    // Marker for debug support.
    if (JSInterpreterHook hook = cx->debugHooks->callHook) {
        newfp->hookData = hook(cx, fp, JS_TRUE, 0,
                               cx->debugHooks->callHookData);
        // CHECK_INTERRUPT_HANDLER();
    } else {
        newfp->hookData = NULL;
    }

    stack.pushInlineFrame(cx, fp, cx->regs->pc, newfp);

    if (newscript->staticLevel < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[newscript->staticLevel];
        newfp->displaySave = *disp;
        *disp = newfp;
    }

    return true;
}

static inline void
FixVMFrame(VMFrame &f, JSStackFrame *fp)
{
    f.inlineCallCount++;
    f.fp->ncode = f.scriptedReturn;
    JS_ASSERT(f.fp == fp->down);
    f.fp = fp;
}

static inline bool
InlineCall(VMFrame &f, uint32 flags, void **pret, uint32 argc)
{
    if (!CreateFrame(f, flags, argc))
        return false;

    JSContext *cx = f.cx;
    JSStackFrame *fp = cx->fp;
    JSScript *script = fp->script;
    if (cx->options & JSOPTION_METHODJIT) {
        if (!script->ncode) {
            if (mjit::TryCompile(cx, script, fp->fun, fp->scopeChainObj()) == Compile_Error)
                return false;
        }
        JS_ASSERT(script->ncode);
        if (script->ncode != JS_UNJITTABLE_METHOD) {
            FixVMFrame(f, fp);
            *pret = script->nmap[-1];
            return true;
        }
    }

    f.regs.pc = script->code;
    f.regs.sp = fp->base();

    bool ok = !!Interpret(cx);
    InlineReturn(cx, JS_TRUE);

    *pret = NULL;
    return ok;
}

static bool
InlineReturn(JSContext *cx, JSBool ok)
{
    JSStackFrame *fp = cx->fp;

    JS_ASSERT(!fp->blockChain);
    JS_ASSERT(!js_IsActiveWithOrBlock(cx, fp->scopeChainObj(), 0));

    if (fp->script->staticLevel < JS_DISPLAY_SIZE)
        cx->display[fp->script->staticLevel] = fp->displaySave;

    // Marker for debug support.
    void *hookData = fp->hookData;
    if (JS_UNLIKELY(hookData != NULL)) {
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
            ok = (status == JS_TRUE);
            // CHECK_INTERRUPT_HANDLER();
        }
    }

    fp->putActivationObjects(cx);

    /* :TODO: version stuff */

    if (fp->flags & JSFRAME_CONSTRUCTING && fp->rval.isPrimitive())
        fp->rval = fp->thisv;

    cx->stack().popInlineFrame(cx, fp, fp->down);
    cx->regs->sp[-1] = fp->rval;

    return ok;
}

static inline JSObject *
InlineConstruct(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    JSObject *funobj = &vp[0].asObject();
    JS_ASSERT(funobj->isFunction());

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!funobj->getProperty(cx, id, &vp[1]))
        return NULL;

    JSObject *proto = vp[1].isObject() ? &vp[1].asObject() : NULL;
    return NewObject(cx, &js_ObjectClass, proto, funobj->getParent());
}

void * JS_FASTCALL
stubs::SlowCall(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    JSObject *obj;
    if (IsFunctionObject(*vp, &obj)) {
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, obj);

        if (fun->isInterpreted()) {
            void *ret;

            if (fun->u.i.script->isEmpty()) {
                vp->setUndefined();
                f.regs.sp = vp + 1;
                return NULL;
            }

            if (!InlineCall(f, 0, &ret, argc))
                THROWV(NULL);

            return ret;
        }

        if (fun->isFastNative()) {
            FastNative fn = (FastNative)fun->u.n.native;
            if (!fn(cx, argc, vp))
                THROWV(NULL);
            return NULL;
        }
    }

    if (!Invoke(f.cx, InvokeArgsGuard(vp, argc), 0))
        THROWV(NULL);

    return NULL;
}

void * JS_FASTCALL
stubs::SlowNew(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    JSObject *obj;
    if (IsFunctionObject(*vp, &obj)) {
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, obj);

        if (fun->isInterpreted()) {
            JSScript *script = fun->u.i.script;
            JSObject *obj2 = InlineConstruct(f, argc);
            if (!obj2)
                THROWV(NULL);

            if (script->isEmpty()) {
                vp[0].setObject(*obj2);
                return NULL;
            }

            void *ret;
            if (!InlineCall(f, JSFRAME_CONSTRUCTING, &ret, argc))
                THROWV(NULL);

            return ret;
        }
    }

    if (!InvokeConstructor(cx, InvokeArgsGuard(vp, argc), JS_TRUE))
        THROWV(NULL);

    return NULL;
}

static inline bool
CreateLightFrame(VMFrame &f, uint32 flags, uint32 argc)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp;
    Value *vp = f.regs.sp - (argc + 2);
    JSObject *funobj = &vp->asObject();
    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);

    JS_ASSERT(FUN_INTERPRETED(fun));

    JSScript *newscript = fun->u.i.script;

    if (f.inlineCallCount >= JS_MAX_INLINE_CALL_COUNT) {
        js_ReportOverRecursed(cx);
        return false;
    }

    /* Allocate the frame. */
    StackSpace &stack = cx->stack();
    uintN nslots = newscript->nslots;
    uintN funargs = fun->nargs;
    Value *argv = vp + 2;
    JSStackFrame *newfp;
    if (argc < funargs) {
        uintN missing = funargs - argc;
        newfp = stack.getInlineFrame(cx, f.regs.sp, missing, nslots);
        if (!newfp)
            return false;
        for (Value *v = argv + argc, *end = v + missing; v != end; ++v)
            v->setUndefined();
    } else {
        newfp = stack.getInlineFrame(cx, f.regs.sp, 0, nslots);
        if (!newfp)
            return false;
    }

    /* Initialize the frame. */
    newfp->ncode = NULL;
    newfp->callobj = NULL;
    newfp->argsval.setNull();
    newfp->script = newscript;
    newfp->fun = fun;
    newfp->argc = argc;
    newfp->argv = vp + 2;
    newfp->rval.setUndefined();
    newfp->annotation = NULL;
    newfp->scopeChain.setObject(*funobj->getParent());
    newfp->flags = flags;
    newfp->blockChain = NULL;
    JS_ASSERT(!JSFUN_BOUND_METHOD_TEST(fun->flags));
    newfp->thisv = vp[1];
    newfp->imacpc = NULL;
    newfp->hookData = NULL;

#if 0
    /* :TODO: Switch version if currentVersion wasn't overridden. */
    newfp->callerVersion = (JSVersion)cx->version;
#endif

#ifdef DEBUG
    newfp->savedPC = JSStackFrame::sInvalidPC;
#endif
    newfp->down = fp;
    fp->savedPC = f.regs.pc;
    cx->setCurrentFrame(newfp);

    if (newscript->staticLevel < JS_DISPLAY_SIZE) {
        JSStackFrame **disp = &cx->display[newscript->staticLevel];
        newfp->displaySave = *disp;
        *disp = newfp;
    }

    return true;
}

/*
 * stubs::Call is guaranteed to be called on a scripted call with JIT'd code.
 */
void * JS_FASTCALL
stubs::Call(VMFrame &f, uint32 argc)
{
    if (!CreateLightFrame(f, 0, argc))
        THROWV(NULL);

    FixVMFrame(f, f.cx->fp);

    return f.fp->script->ncode;
}

/*
 * stubs::New is guaranteed to be called on a scripted call with JIT'd code.
 */
void * JS_FASTCALL
stubs::New(VMFrame &f, uint32 argc)
{
    JSObject *obj = InlineConstruct(f, argc);
    if (!obj)
        THROWV(NULL);

    f.regs.sp[-int(argc + 1)].setObject(*obj);
    if (!CreateLightFrame(f, JSFRAME_CONSTRUCTING, argc))
        THROWV(NULL);

    FixVMFrame(f, f.cx->fp);

    return f.fp->script->ncode;
}

void JS_FASTCALL
stubs::PutCallObject(VMFrame &f)
{
    JS_ASSERT(f.fp->callobj);
    js_PutCallObject(f.cx, f.fp);
    JS_ASSERT(f.fp->argsval.isNull());
}

void JS_FASTCALL
stubs::PutArgsObject(VMFrame &f)
{
    JS_ASSERT(f.fp->argsval.isObject());
    js_PutArgsObject(f.cx, f.fp);
}

void JS_FASTCALL
stubs::CopyThisv(VMFrame &f)
{
    JS_ASSERT(f.fp->flags & JSFRAME_CONSTRUCTING);
    if (f.fp->rval.isPrimitive())
        f.fp->rval = f.fp->thisv;
}

extern "C" void *
js_InternalThrow(VMFrame &f)
{
    JSContext *cx = f.cx;

    // Make sure sp is up to date.
    JS_ASSERT(cx->regs == &f.regs);

    jsbytecode *pc = NULL;
    for (;;) {
        pc = FindExceptionHandler(cx);
        if (pc)
            break;

        // If |f.inlineCallCount == 0|, then we are on the 'topmost' frame (where
        // topmost means the first frame called into through js_Interpret). In this
        // case, we still unwind, but we shouldn't return from a JS function, because
        // we're not in a JS function.
        bool lastFrame = (f.inlineCallCount == 0);
        js_UnwindScope(cx, 0, cx->throwing);
        if (lastFrame)
            break;

        JS_ASSERT(f.regs.sp == cx->regs->sp);
        InlineReturn(f.cx, JS_FALSE);
        f.inlineCallCount--;
        f.fp = cx->fp;
        f.scriptedReturn = cx->fp->ncode;
    }

    JS_ASSERT(f.regs.sp == cx->regs->sp);

    if (!pc) {
        *f.oldRegs = f.regs;
        f.cx->setCurrentRegs(f.oldRegs);
        return NULL;
    }

    return cx->fp->script->pcToNative(pc);
}

void JS_FASTCALL
stubs::GetCallObject(VMFrame &f)
{
    JS_ASSERT(f.fp->fun->isHeavyweight());
    if (!js_GetCallObject(f.cx, f.fp))
        THROW();
}

