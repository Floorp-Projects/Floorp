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
 * The js::InvokeArgumentsGuard passed to js_Invoke must come from an
 * immediately-enclosing successful call to js::StackSpace::pushInvokeArgs,
 * i.e., there must have been no un-popped pushes to cx->stack. Furthermore,
 * |args.getvp()[0]| should be the callee, |args.getvp()[1]| should be |this|,
 * and the range [args.getvp() + 2, args.getvp() + 2 + args.getArgc()) should
 * be initialized actual arguments.
 */
extern bool
Invoke(JSContext *cx, const CallArgs &args, MaybeConstruct construct = NO_CONSTRUCT);

/*
 * For calls to natives, the InvokeArgsGuard object provides a record of the
 * call for the debugger's callstack. For this to work, the InvokeArgsGuard
 * record needs to know when the call is actually active (because the
 * InvokeArgsGuard can be pushed long before and popped long after the actual
 * call, during which time many stack-observing things can happen).
 */
inline bool
Invoke(JSContext *cx, InvokeArgsGuard &args, MaybeConstruct construct = NO_CONSTRUCT)
{
    args.setActive();
    bool ok = Invoke(cx, ImplicitCast<CallArgs>(args), construct);
    args.setInactive();
    return ok;
}

/*
 * Natives like sort/forEach/replace call Invoke repeatedly with the same
 * callee, this, and number of arguments. To optimize this, such natives can
 * start an "invoke session" to factor out much of the dynamic setup logic
 * required by a normal Invoke. Usage is:
 *
 *   InvokeSessionGuard session(cx);
 *   if (!session.start(cx, callee, thisp, argc, &session))
 *     ...
 *
 *   while (...) {
 *     // write actual args (not callee, this)
 *     session[0] = ...
 *     ...
 *     session[argc - 1] = ...
 *
 *     if (!session.invoke(cx, session))
 *       ...
 *
 *     ... = session.rval();
 *   }
 *
 *   // session ended by ~InvokeSessionGuard
 */
class InvokeSessionGuard;

/*
 * "External" calls may come from C or C++ code using a JSContext on which no
 * JS is running (!cx->fp), so they may need to push a dummy StackFrame.
 */

extern bool
ExternalInvoke(JSContext *cx, const Value &thisv, const Value &fval,
               uintN argc, Value *argv, Value *rval);

extern bool
ExternalGetOrSet(JSContext *cx, JSObject *obj, jsid id, const Value &fval,
                 JSAccessMode mode, uintN argc, Value *argv, Value *rval);

/*
 * These two functions invoke a function called from a constructor context
 * (e.g. 'new'). InvokeConstructor handles the general case where a new object
 * needs to be created for/by the constructor. ConstructWithGivenThis directly
 * calls the constructor with the given 'this', hence the caller must
 * understand the semantics of the constructor call.
 */

extern JS_REQUIRES_STACK bool
InvokeConstructor(JSContext *cx, const CallArgs &args);

extern JS_REQUIRES_STACK bool
InvokeConstructorWithGivenThis(JSContext *cx, JSObject *thisobj, const Value &fval,
                               uintN argc, Value *argv, Value *rval);

extern bool
ExternalInvokeConstructor(JSContext *cx, const Value &fval, uintN argc, Value *argv,
                          Value *rval);

extern bool
ExternalExecute(JSContext *cx, JSScript *script, JSObject &scopeChain, Value *rval);

/*
 * Executes a script with the given scopeChain/this. The 'type' indicates
 * whether this is eval code or global code. To support debugging, the
 * evalFrame parameter can point to an arbitrary frame in the context's call
 * stack to simulate executing an eval in that frame.
 */
extern bool
Execute(JSContext *cx, JSScript *script, JSObject &scopeChain, const Value &thisv,
        ExecuteType type, StackFrame *evalInFrame, Value *result);

/* Flags to toggle js::Interpret() execution. */
enum InterpMode
{
    JSINTERP_NORMAL    = 0, /* interpreter is running normally */
    JSINTERP_RECORD    = 1, /* interpreter has been started to record/run traces */
    JSINTERP_SAFEPOINT = 2, /* interpreter should leave on a method JIT safe point */
    JSINTERP_PROFILE   = 3, /* interpreter should profile a loop */
    JSINTERP_REJOIN    = 4, /* as normal, but the frame has already started */
    JSINTERP_SKIP_TRAP = 5, /* as REJOIN, but skip trap at first opcode */
    JSINTERP_BAILOUT   = 6  /* interpreter is running from an Ion bailout */
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

} /* namespace js */

/*
 * JS_LONE_INTERPRET indicates that the compiler should see just the code for
 * the js_Interpret function when compiling jsinterp.cpp. The rest of the code
 * from the file should be visible only when compiling jsinvoke.cpp. It allows
 * platform builds to optimize selectively js_Interpret when the granularity
 * of the optimizations with the given compiler is a compilation unit.
 *
 * JS_STATIC_INTERPRET is the modifier for functions defined in jsinterp.cpp
 * that only js_Interpret calls. When JS_LONE_INTERPRET is true all such
 * functions are declared below.
 */
#ifndef JS_LONE_INTERPRET
# ifdef _MSC_VER
#  define JS_LONE_INTERPRET 0
# else
#  define JS_LONE_INTERPRET 1
# endif
#endif

#if !JS_LONE_INTERPRET
# define JS_STATIC_INTERPRET    static
#else
# define JS_STATIC_INTERPRET

extern JS_REQUIRES_STACK JSBool
js_EnterWith(JSContext *cx, jsint stackIndex, JSOp op, size_t oplen);

extern JS_REQUIRES_STACK void
js_LeaveWith(JSContext *cx);

/*
 * Find the results of incrementing or decrementing *vp. For pre-increments,
 * both *vp and *vp2 will contain the result on return. For post-increments,
 * vp will contain the original value converted to a number and vp2 will get
 * the result. Both vp and vp2 must be roots.
 */
extern JSBool
js_DoIncDec(JSContext *cx, const JSCodeSpec *cs, js::Value *vp, js::Value *vp2);

#endif /* JS_LONE_INTERPRET */
/*
 * Unwind block and scope chains to match the given depth. The function sets
 * fp->sp on return to stackDepth.
 */
extern JS_REQUIRES_STACK JSBool
js_UnwindScope(JSContext *cx, jsint stackDepth, JSBool normalUnwind);

extern JSBool
js_OnUnknownMethod(JSContext *cx, js::Value *vp);

extern JS_REQUIRES_STACK js::Class *
js_IsActiveWithOrBlock(JSContext *cx, JSObject *obj, int stackDepth);

#endif /* jsinterp_h___ */
