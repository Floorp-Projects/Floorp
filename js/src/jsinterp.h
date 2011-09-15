/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
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

#ifndef jsinterp_h___
#define jsinterp_h___
/*
 * JS interpreter interface.
 */
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsvalue.h"

#include "vm/Stack.h"

namespace js {

extern JSObject *
GetBlockChain(JSContext *cx, StackFrame *fp);

extern JSObject *
GetBlockChainFast(JSContext *cx, StackFrame *fp, JSOp op, size_t oplen);

extern JSObject *
GetScopeChain(JSContext *cx);

/*
 * Refresh and return fp->scopeChain.  It may be stale if block scopes are
 * active but not yet reflected by objects in the scope chain.  If a block
 * scope contains a with, eval, XML filtering predicate, or similar such
 * dynamically scoped construct, then compile-time block scope at fp->blocks
 * must reflect at runtime.
 */
extern JSObject *
GetScopeChain(JSContext *cx, StackFrame *fp);

extern JSObject *
GetScopeChainFast(JSContext *cx, StackFrame *fp, JSOp op, size_t oplen);

/*
 * Report an error that the this value passed as |this| in the given arguments
 * vector is not compatible with the specified class.
 */
void
ReportIncompatibleMethod(JSContext *cx, Value *vp, Class *clasp);

/*
 * Given a context and a vector of [callee, this, args...] for a function
 * whose JSFUN_PRIMITIVE_THIS flag is set, set |*v| to the primitive value
 * of |this|. If |this| is an object, insist that it be an instance of the
 * appropriate wrapper class for T, and set |*v| to its private slot value.
 * If |this| is a primitive, unbox it into |*v| if it's of the required
 * type, and throw an error otherwise.
 */
template <typename T>
bool GetPrimitiveThis(JSContext *cx, Value *vp, T *v);

/*
 * ScriptPrologue/ScriptEpilogue must be called in pairs. ScriptPrologue
 * must be called before the script executes. ScriptEpilogue must be called
 * after the script returns or exits via exception.
 */

inline bool
ScriptPrologue(JSContext *cx, StackFrame *fp, JSScript *script);

inline bool
ScriptEpilogue(JSContext *cx, StackFrame *fp, bool ok);

/*
 * It is not valid to call ScriptPrologue when a generator is resumed or to
 * call ScriptEpilogue when a generator yields. However, the debugger still
 * needs LIFO notification of generator start/stop. This pair of functions does
 * the right thing based on the state of 'fp'.
 */

inline bool
ScriptPrologueOrGeneratorResume(JSContext *cx, StackFrame *fp);

inline bool
ScriptEpilogueOrGeneratorYield(JSContext *cx, StackFrame *fp, bool ok);

/* Implemented in jsdbgapi: */

extern void
ScriptDebugPrologue(JSContext *cx, StackFrame *fp);

extern bool
ScriptDebugEpilogue(JSContext *cx, StackFrame *fp, bool ok);

/*
 * For a given |call|, convert null/undefined |this| into the global object for
 * the callee and replace other primitives with boxed versions. This assumes
 * that call.callee() is not strict mode code. This is the special/slow case of
 * ComputeThis.
 */
extern bool
BoxNonStrictThis(JSContext *cx, const CallReceiver &call);

/*
 * Ensure that fp->thisValue() is the correct value of |this| for the scripted
 * call represented by |fp|. ComputeThis is necessary because fp->thisValue()
 * may be set to 'undefined' when 'this' should really be the global object (as
 * an optimization to avoid global-this computation).
 */
inline bool
ComputeThis(JSContext *cx, StackFrame *fp);

enum MaybeConstruct {
    NO_CONSTRUCT = INITIAL_NONE,
    CONSTRUCT = INITIAL_CONSTRUCT
};

/*
 * InvokeKernel assumes that the given args have been pushed on the top of the
 * VM stack. Additionally, if 'args' is contained in a CallArgsList, that they
 * have already been marked 'active'.
 */
extern bool
InvokeKernel(JSContext *cx, const CallArgs &args, MaybeConstruct construct = NO_CONSTRUCT);

/*
 * Invoke assumes that 'args' has been pushed (via ContextStack::pushInvokeArgs)
 * and is currently at the top of the VM stack.
 */
inline bool
Invoke(JSContext *cx, InvokeArgsGuard &args, MaybeConstruct construct = NO_CONSTRUCT)
{
    args.setActive();
    bool ok = InvokeKernel(cx, args, construct);
    args.setInactive();
    return ok;
}

/*
 * This Invoke overload places the least requirements on the caller: it may be
 * called at any time and it takes care of copying the given callee, this, and
 * arguments onto the stack.
 */
extern bool
Invoke(JSContext *cx, const Value &thisv, const Value &fval, uintN argc, Value *argv,
       Value *rval);

/*
 * This helper takes care of the infinite-recursion check necessary for
 * getter/setter calls.
 */
extern bool
InvokeGetterOrSetter(JSContext *cx, JSObject *obj, const Value &fval, uintN argc, Value *argv,
                     Value *rval);

/*
 * InvokeConstructor* implement a function call from a constructor context
 * (e.g. 'new') handling the the creation of the new 'this' object.
 */
extern JS_REQUIRES_STACK bool
InvokeConstructorKernel(JSContext *cx, const CallArgs &args);

/* See the InvokeArgsGuard overload of Invoke. */
inline bool
InvokeConstructor(JSContext *cx, InvokeArgsGuard &args)
{
    args.setActive();
    bool ok = InvokeConstructorKernel(cx, ImplicitCast<CallArgs>(args));
    args.setInactive();
    return ok;
}

/* See the fval overload of Invoke. */
extern bool
InvokeConstructor(JSContext *cx, const Value &fval, uintN argc, Value *argv, Value *rval);

/*
 * InvokeConstructorWithGivenThis directly calls the constructor with the given
 * 'this'; the caller must choose the semantically correct 'this'.
 */
extern JS_REQUIRES_STACK bool
InvokeConstructorWithGivenThis(JSContext *cx, JSObject *thisobj, const Value &fval,
                               uintN argc, Value *argv, Value *rval);

/*
 * Executes a script with the given scopeChain/this. The 'type' indicates
 * whether this is eval code or global code. To support debugging, the
 * evalFrame parameter can point to an arbitrary frame in the context's call
 * stack to simulate executing an eval in that frame.
 */
extern bool
ExecuteKernel(JSContext *cx, JSScript *script, JSObject &scopeChain, const Value &thisv,
              ExecuteType type, StackFrame *evalInFrame, Value *result);

/* Execute a script with the given scopeChain as global code. */
extern bool
Execute(JSContext *cx, JSScript *script, JSObject &scopeChain, Value *rval);

/* Flags to toggle js::Interpret() execution. */
enum InterpMode
{
    JSINTERP_NORMAL    = 0, /* interpreter is running normally */
    JSINTERP_RECORD    = 1, /* interpreter has been started to record/run traces */
    JSINTERP_SAFEPOINT = 2, /* interpreter should leave on a method JIT safe point */
    JSINTERP_PROFILE   = 3, /* interpreter should profile a loop */
    JSINTERP_REJOIN    = 4, /* as normal, but the frame has already started */
    JSINTERP_SKIP_TRAP = 5  /* as REJOIN, but skip trap at first opcode */
};

/*
 * Execute the caller-initialized frame for a user-defined script or function
 * pointed to by cx->fp until completion or error.
 */
extern JS_REQUIRES_STACK JS_NEVER_INLINE bool
Interpret(JSContext *cx, StackFrame *stopFp, InterpMode mode = JSINTERP_NORMAL);

extern JS_REQUIRES_STACK bool
RunScript(JSContext *cx, JSScript *script, StackFrame *fp);

extern bool
CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs);

extern bool
StrictlyEqual(JSContext *cx, const Value &lval, const Value &rval, JSBool *equal);

extern bool
LooselyEqual(JSContext *cx, const Value &lval, const Value &rval, JSBool *equal);

/* === except that NaN is the same as NaN and -0 is not the same as +0. */
extern bool
SameValue(JSContext *cx, const Value &v1, const Value &v2, JSBool *same);

extern JSType
TypeOfValue(JSContext *cx, const Value &v);

extern JSBool
HasInstance(JSContext *cx, JSObject *obj, const js::Value *v, JSBool *bp);

extern bool
ValueToId(JSContext *cx, const Value &v, jsid *idp);

/*
 * @param closureLevel      The static level of the closure that the cookie
 *                          pertains to.
 * @param cookie            Level amount is a "skip" (delta) value from the
 *                          closure level.
 * @return  The value of the upvar.
 */
extern const Value &
GetUpvar(JSContext *cx, uintN level, UpvarCookie cookie);

/* Search the call stack for the nearest frame with static level targetLevel. */
extern StackFrame *
FindUpvarFrame(JSContext *cx, uintN targetLevel);

/*
 * A linked list of the |FrameRegs regs;| variables belonging to all
 * js::Interpret C++ frames on this thread's stack.
 *
 * Note that this is *not* a list of all JS frames running under the
 * interpreter; that would include inlined frames, whose FrameRegs are
 * saved in various pieces in various places. Rather, this lists each
 * js::Interpret call's live 'regs'; when control returns to that call, it
 * will resume execution with this 'regs' instance.
 *
 * When Debugger puts a script in single-step mode, all js::Interpret
 * invocations that might be presently running that script must have
 * interrupts enabled. It's not practical to simply check
 * script->stepModeEnabled() at each point some callee could have changed
 * it, because there are so many places js::Interpret could possibly cause
 * JavaScript to run: each place an object might be coerced to a primitive
 * or a number, for example. So instead, we simply expose a list of the
 * 'regs' those frames are using, and let Debugger tweak the affected
 * js::Interpret frames when an onStep handler is established.
 *
 * Elements of this list are allocated within the js::Interpret stack
 * frames themselves; the list is headed by this thread's js::ThreadData.
 */
class InterpreterFrames {
  public:
    class InterruptEnablerBase {
      public:
        virtual void enableInterrupts() const = 0;
    };

    InterpreterFrames(JSContext *cx, FrameRegs *regs, const InterruptEnablerBase &enabler);
    ~InterpreterFrames();

    /* If this js::Interpret frame is running |script|, enable interrupts. */
    void enableInterruptsIfRunning(JSScript *script) {
        if (script == regs->fp()->script())
            enabler.enableInterrupts();
    }

    InterpreterFrames *older;

  private:
    JSContext *context;
    FrameRegs *regs;
    const InterruptEnablerBase &enabler;
};

/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
extern bool
UnwindScope(JSContext *cx, jsint stackDepth, JSBool normalUnwind);

extern bool
OnUnknownMethod(JSContext *cx, js::Value *vp);

extern bool
IsActiveWithOrBlock(JSContext *cx, JSObject &obj, int stackDepth);

}  /* namespace js */

#endif /* jsinterp_h___ */
