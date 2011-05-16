/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Luke Wagner <luke@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef Stack_h__
#define Stack_h__

#include "jsfun.h"

namespace js {

class StackFrame;
class FrameRegs;
class StackSegment;
class StackSpace;
class ContextStack;

class InvokeArgsGuard;
class InvokeFrameGuard;
class FrameGuard;
class ExecuteFrameGuard;
class DummyFrameGuard;
class GeneratorFrameGuard;

class ArgumentsObject;

namespace mjit { struct JITScript; }
namespace detail { struct OOMCheck; }

/*
 * VM stack layout
 *
 * SpiderMonkey uses a per-thread stack to store the activation records,
 * parameters, locals, and expression temporaries for the stack of actively
 * executing scripts, functions and generators. The per-thread stack is owned
 * by the StackSpace object stored in the thread's ThreadData.
 *
 * The per-thread stack is subdivided into contiguous segments of memory which
 * have a memory layout invariant that allows fixed offsets to be used for stack
 * access (by the JIT) as well as fast call/return. This memory layout is
 * encapsulated by a set of types that describe different regions of memory:
 * StackSegment, StackFrame, FrameRegs and CallArgs. To avoid calling into C++,
 * the JIT compiler generates code that simulates C++ stack operations.
 *
 * The memory layout of a segment looks like:
 *
 *                            current regs
 *     .------------------------------------------------------.
 *     |                  current frame                       |
 *     |   .-------------------------------------.            V
 *     |   |    initial frame                    |        FrameRegs
 *     |   |   .------------.                    |            |
 *     |   |   |            V                    V            V
 * |StackSegment| slots |StackFrame| slots |StackFrame| slots |
 *                        |      ^           |
 *           ? <----------'      `-----------'
 *                 prev               prev
 *
 * A segment starts with a fixed-size header (js::StackSegment) which logically
 * describes the segment, links it to the rest of the stack, and points to the
 * first and last frames in the segment.
 *
 * Each script activation (global or function code) is given a fixed-size header
 * (js::StackFrame) which is associated with the values (called "slots") before
 * and after it. The frame contains bookkeeping information about the activation
 * and links to the previous frame.
 *
 * The slots preceeding a (function) StackFrame in memory are the arguments of
 * the call. The slots after a StackFrame in memory are its locals followed
 * by its expression stack. There is no clean line between the arguments of a
 * frame and the expression stack of the previous frame since the top slots of
 * the expression become the arguments of a call. There are also layout
 * invariants concerning the arguments and StackFrame; see "Arguments" comment
 * in StackFrame for more details.
 *
 * The top of a segment's current frame's expression stack is pointed to by the
 * segment's "current regs", which contains the stack pointer 'sp'. In the
 * interpreter, sp is adjusted as individual values are pushed and popped from
 * the stack and the FrameRegs struct (pointed by the StackSegment) is a local
 * var of js::Interpret. JIT code simulates this by lazily updating FrameRegs
 * when calling from JIT code into the VM. Ideally, we'd like to remove all
 * dependence on FrameRegs outside the interpreter.
 *
 * A call to a native (C++) function does not push a frame. Instead, an array
 * of values (possibly from the top of a calling frame's expression stack) is
 * passed to the native. The layout of this array is abstracted by js::CallArgs.
 * Note that, between any two StackFrames there may be any number of native
 * calls, so the meaning of 'prev' is not 'directly called by'.
 *
 * An additional feature (perhaps not for much longer: bug 650361) is that
 * multiple independent "contexts" can interleave (LIFO) on a single contiguous
 * stack. "Independent" here means that neither context sees the other's frames.
 * Concretely, an embedding may enter the JS engine on cx1 and then, from a
 * native called by the JS engine, reenter the VM on cx2. Changing from cx1 to
 * cx2 causes cx1's segment to be "suspended" and a new segment started to be
 * started for cx2. These two segments are linked from the perspective of
 * StackSpace, since they are adjacent on the thread's stack, but not from the
 * perspective of cx1 and cx2. Thus, each segment has two prev-links:
 * previousInMemory and previousInContext. A context's apparent stack is
 * encapsulated and managed by the js::ContextStack object stored in JSContext.
 * ContextStack is the primary interface to the rest of the engine for pushing
 * and popping args (for js::Invoke calls) and frames.
 */

/*****************************************************************************/

class CallReceiver
{
#ifdef DEBUG
    mutable bool usedRval_;
#endif
  protected:
    Value *argv_;
    CallReceiver() {}
    CallReceiver(Value *argv) : argv_(argv) {
#ifdef DEBUG
        usedRval_ = false;
#endif
    }

  public:
    friend CallReceiver CallReceiverFromVp(Value *);
    friend CallReceiver CallReceiverFromArgv(Value *);
    Value *base() const { return argv_ - 2; }
    JSObject &callee() const { JS_ASSERT(!usedRval_); return argv_[-2].toObject(); }
    Value &calleev() const { JS_ASSERT(!usedRval_); return argv_[-2]; }
    Value &thisv() const { return argv_[-1]; }

    Value &rval() const {
#ifdef DEBUG
        usedRval_ = true;
#endif
        return argv_[-2];
    }

    void calleeHasBeenReset() const {
#ifdef DEBUG
        usedRval_ = false;
#endif
    }
};

JS_ALWAYS_INLINE CallReceiver
CallReceiverFromVp(Value *vp)
{
    return CallReceiver(vp + 2);
}

JS_ALWAYS_INLINE CallReceiver
CallReceiverFromArgv(Value *argv)
{
    return CallReceiver(argv);
}

/*****************************************************************************/

class CallArgs : public CallReceiver
{
    uintN argc_;
  protected:
    CallArgs() {}
    CallArgs(uintN argc, Value *argv) : CallReceiver(argv), argc_(argc) {}
  public:
    friend CallArgs CallArgsFromVp(uintN, Value *);
    friend CallArgs CallArgsFromArgv(uintN, Value *);
    Value &operator[](unsigned i) const { JS_ASSERT(i < argc_); return argv_[i]; }
    Value *argv() const { return argv_; }
    uintN argc() const { return argc_; }
};

JS_ALWAYS_INLINE CallArgs
CallArgsFromVp(uintN argc, Value *vp)
{
    return CallArgs(argc, vp + 2);
}

JS_ALWAYS_INLINE CallArgs
CallArgsFromArgv(uintN argc, Value *argv)
{
    return CallArgs(argc, argv);
}

/*****************************************************************************/

class StackFrame
{
  public:
    enum Flags {
        /* Primary frame type */
        GLOBAL             =        0x1,  /* frame pushed for a global script */
        FUNCTION           =        0x2,  /* frame pushed for a scripted call */
        DUMMY              =        0x4,  /* frame pushed for bookkeeping */

        /* Frame subtypes */
        EVAL               =        0x8,  /* frame pushed for eval() or debugger eval */
        DEBUGGER           =       0x10,  /* frame pushed for debugger eval */
        GENERATOR          =       0x20,  /* frame is associated with a generator */
        FLOATING_GENERATOR =       0x40,  /* frame is is in generator obj, not on stack */
        CONSTRUCTING       =       0x80,  /* frame is for a constructor invocation */

        /* Temporary frame states */
        YIELDING           =      0x100,  /* js::Interpret dispatched JSOP_YIELD */
        FINISHED_IN_INTERP =      0x200,  /* set if frame finished in Interpret() */

        /* Concerning function arguments */
        OVERRIDE_ARGS      =      0x400,  /* overridden arguments local variable */
        OVERFLOW_ARGS      =      0x800,  /* numActualArgs > numFormalArgs */
        UNDERFLOW_ARGS     =     0x1000,  /* numActualArgs < numFormalArgs */

        /* Lazy frame initialization */
        HAS_IMACRO_PC      =     0x2000,  /* frame has imacpc value available */
        HAS_CALL_OBJ       =     0x4000,  /* frame has a callobj reachable from scopeChain_ */
        HAS_ARGS_OBJ       =     0x8000,  /* frame has an argsobj in StackFrame::args */
        HAS_HOOK_DATA      =    0x10000,  /* frame has hookData_ set */
        HAS_ANNOTATION     =    0x20000,  /* frame has annotation_ set */
        HAS_RVAL           =    0x40000,  /* frame has rval_ set */
        HAS_SCOPECHAIN     =    0x80000,  /* frame has scopeChain_ set */
        HAS_PREVPC         =   0x100000   /* frame has prevpc_ set */
    };

  private:
    mutable uint32      flags_;         /* bits described by Flags */
    union {                             /* describes what code is executing in a */
        JSScript        *script;        /*   global frame */
        JSFunction      *fun;           /*   function frame, pre GetScopeChain */
    } exec;
    union {                             /* describes the arguments of a function */
        uintN           nactual;        /*   before js_GetArgsObject */
        ArgumentsObject *obj;           /*   after js_GetArgsObject */
        JSScript        *script;        /* eval has no args, but needs a script */
    } args;
    mutable JSObject    *scopeChain_;   /* current scope chain */
    StackFrame          *prev_;         /* previous cx->regs->fp */
    void                *ncode_;        /* return address for method JIT */

    /* Lazily initialized */
    js::Value           rval_;          /* return value of the frame */
    jsbytecode          *prevpc_;       /* pc of previous frame*/
    jsbytecode          *imacropc_;     /* pc of macro caller */
    void                *hookData_;     /* closure returned by call hook */
    void                *annotation_;   /* perhaps remove with bug 546848 */

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) % sizeof(js::Value) == 0);
        JS_STATIC_ASSERT(sizeof(StackFrame) % sizeof(js::Value) == 0);
    }

    inline void initPrev(JSContext *cx);
    jsbytecode *prevpcSlow();

  public:
    /*
     * Frame initialization
     *
     * After acquiring a pointer to an uninitialized stack frame on the VM
     * stack from StackSpace, these members are used to initialize the stack
     * frame before officially pushing the frame into the context.
     */

    /* Used for Invoke, Interpret, trace-jit LeaveTree, and method-jit stubs. */
    inline void initCallFrame(JSContext *cx, JSObject &callee, JSFunction *fun,
                              uint32 nactual, uint32 flags);

    /* Used for SessionInvoke. */
    inline void resetInvokeCallFrame();

    /* Called by method-jit stubs and serve as a specification for jit-code. */
    inline void initCallFrameCallerHalf(JSContext *cx, uint32 flags, void *ncode);
    inline void initCallFrameEarlyPrologue(JSFunction *fun, uint32 nactual);
    inline void initCallFrameLatePrologue();

    /* Used for eval. */
    inline void initEvalFrame(JSContext *cx, JSScript *script, StackFrame *prev,
                              uint32 flags);
    inline void initGlobalFrame(JSScript *script, JSObject &chain, StackFrame *prev,
                                uint32 flags);

    /* Used when activating generators. */
    inline void stealFrameAndSlots(js::Value *vp, StackFrame *otherfp,
                                   js::Value *othervp, js::Value *othersp);

    /* Perhaps one fine day we will remove dummy frames. */
    inline void initDummyFrame(JSContext *cx, JSObject &chain);

    /*
     * Stack frame type
     *
     * A stack frame may have one of three types, which determines which
     * members of the frame may be accessed and other invariants:
     *
     *  global frame:   execution of global code or an eval in global code
     *  function frame: execution of function code or an eval in a function
     *  dummy frame:    bookkeeping frame (to be removed in bug 625199)
     */

    bool isFunctionFrame() const {
        return !!(flags_ & FUNCTION);
    }

    bool isGlobalFrame() const {
        return !!(flags_ & GLOBAL);
    }

    bool isDummyFrame() const {
        return !!(flags_ & DUMMY);
    }

    bool isScriptFrame() const {
        bool retval = !!(flags_ & (FUNCTION | GLOBAL));
        JS_ASSERT(retval == !isDummyFrame());
        return retval;
    }

    /*
     * Eval frames
     *
     * As noted above, global and function frames may optionally be 'eval
     * frames'. Eval code shares its parent's arguments which means that the
     * arg-access members of StackFrame may not be used for eval frames.
     * Search for 'hasArgs' below for more details.
     *
     * A further sub-classification of eval frames is whether the frame was
     * pushed for an ES5 strict-mode eval().
     */

    bool isEvalFrame() const {
        JS_ASSERT_IF(flags_ & EVAL, isScriptFrame());
        return flags_ & EVAL;
    }

    bool isNonEvalFunctionFrame() const {
        return (flags_ & (FUNCTION | EVAL)) == FUNCTION;
    }

    inline bool isStrictEvalFrame() const {
        return isEvalFrame() && script()->strictModeCode;
    }

    bool isNonStrictEvalFrame() const {
        return isEvalFrame() && !script()->strictModeCode;
    }

    /*
     * Previous frame
     *
     * A frame's 'prev' frame is either null or the previous frame pointed to
     * by cx->regs->fp when this frame was pushed. Often, given two prev-linked
     * frames, the next-frame is a function or eval that was called by the
     * prev-frame, but not always: the prev-frame may have called a native that
     * reentered the VM through JS_CallFunctionValue on the same context
     * (without calling JS_SaveFrameChain) which pushed the next-frame. Thus,
     * 'prev' has little semantic meaning and basically just tells the VM what
     * to set cx->regs->fp to when this frame is popped.
     */

    StackFrame *prev() const {
        return prev_;
    }

    inline void resetGeneratorPrev(JSContext *cx);

    /*
     * Frame slots
     *
     * A frame's 'slots' are the fixed slots associated with the frame (like
     * local variables) followed by an expression stack holding temporary
     * values. A frame's 'base' is the base of the expression stack.
     */

    js::Value *slots() const {
        return (js::Value *)(this + 1);
    }

    js::Value *base() const {
        return slots() + script()->nfixed;
    }

    js::Value &varSlot(uintN i) {
        JS_ASSERT(i < script()->nfixed);
        JS_ASSERT_IF(maybeFun(), i < script()->bindings.countVars());
        return slots()[i];
    }

    /*
     * Script
     *
     * All function and global frames have an associated JSScript which holds
     * the bytecode being executed for the frame.
     */

    /*
     * Get the frame's current bytecode, assuming |this| is in |cx|.
     * next is frame whose prev == this, NULL if not known or if this == cx->fp().
     */
    jsbytecode *pc(JSContext *cx, StackFrame *next = NULL);

    jsbytecode *prevpc() {
        if (flags_ & HAS_PREVPC)
            return prevpc_;
        return prevpcSlow();
    }

    JSScript *script() const {
        JS_ASSERT(isScriptFrame());
        return isFunctionFrame()
               ? isEvalFrame() ? args.script : fun()->script()
               : exec.script;
    }

    JSScript *functionScript() const {
        JS_ASSERT(isFunctionFrame());
        return isEvalFrame() ? args.script : fun()->script();
    }

    JSScript *globalScript() const {
        JS_ASSERT(isGlobalFrame());
        return exec.script;
    }

    JSScript *maybeScript() const {
        return isScriptFrame() ? script() : NULL;
    }

    size_t numFixed() const {
        return script()->nfixed;
    }

    size_t numSlots() const {
        return script()->nslots;
    }

    size_t numGlobalVars() const {
        JS_ASSERT(isGlobalFrame());
        return exec.script->nfixed;
    }

    /*
     * Function
     *
     * All function frames have an associated interpreted JSFunction.
     */

    JSFunction* fun() const {
        JS_ASSERT(isFunctionFrame());
        return exec.fun;
    }

    JSFunction* maybeFun() const {
        return isFunctionFrame() ? fun() : NULL;
    }

    /*
     * Arguments
     *
     * Only non-eval function frames have arguments. A frame follows its
     * arguments contiguously in memory. The arguments pushed by the caller are
     * the 'actual' arguments. The declared arguments of the callee are the
     * 'formal' arguments. When the caller passes less or equal actual
     * arguments, the actual and formal arguments are the same array (but with
     * different extents). When the caller passes too many arguments, the
     * formal subset of the actual arguments is copied onto the top of the
     * stack. This allows the engine to maintain a jit-time constant offset of
     * arguments from the frame pointer. Since the formal subset of the actual
     * arguments is potentially on the stack twice, it is important for all
     * reads/writes to refer to the same canonical memory location.
     *
     * An arguments object (the object returned by the 'arguments' keyword) is
     * lazily created, so a given function frame may or may not have one.
     */

    /* True if this frame has arguments. Contrast with hasArgsObj. */
    bool hasArgs() const {
        return isNonEvalFunctionFrame();
    }

    uintN numFormalArgs() const {
        JS_ASSERT(hasArgs());
        return fun()->nargs;
    }

    js::Value &formalArg(uintN i) const {
        JS_ASSERT(i < numFormalArgs());
        return formalArgs()[i];
    }

    js::Value *formalArgs() const {
        JS_ASSERT(hasArgs());
        return (js::Value *)this - numFormalArgs();
    }

    js::Value *formalArgsEnd() const {
        JS_ASSERT(hasArgs());
        return (js::Value *)this;
    }

    js::Value *maybeFormalArgs() const {
        return (flags_ & (FUNCTION | EVAL)) == FUNCTION
               ? formalArgs()
               : NULL;
    }

    inline uintN numActualArgs() const;
    inline js::Value *actualArgs() const;
    inline js::Value *actualArgsEnd() const;

    inline js::Value &canonicalActualArg(uintN i) const;
    template <class Op> inline bool forEachCanonicalActualArg(Op op);
    template <class Op> inline bool forEachFormalArg(Op op);

    inline void clearMissingArgs();

    bool hasArgsObj() const {
        return !!(flags_ & HAS_ARGS_OBJ);
    }

    ArgumentsObject &argsObj() const {
        JS_ASSERT(hasArgsObj());
        JS_ASSERT(!isEvalFrame());
        return *args.obj;
    }

    ArgumentsObject *maybeArgsObj() const {
        return hasArgsObj() ? &argsObj() : NULL;
    }

    inline void setArgsObj(ArgumentsObject &obj);

    /*
     * This value
     *
     * Every frame has a this value although, until 'this' is computed, the
     * value may not be the semantically-correct 'this' value.
     *
     * The 'this' value is stored before the formal arguments for function
     * frames and directly before the frame for global frames. The *Args
     * members assert !isEvalFrame(), so we implement specialized inline
     * methods for accessing 'this'. When the caller has static knowledge that
     * a frame is a function or global frame, 'functionThis' and 'globalThis',
     * respectively, allow more efficient access.
     */

    js::Value &functionThis() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((js::Value *)this)[-1];
        return formalArgs()[-1];
    }

    JSObject &constructorThis() const {
        JS_ASSERT(hasArgs());
        return formalArgs()[-1].toObject();
    }

    js::Value &globalThis() const {
        JS_ASSERT(isGlobalFrame());
        return ((js::Value *)this)[-1];
    }

    js::Value &thisValue() const {
        if (flags_ & (EVAL | GLOBAL))
            return ((js::Value *)this)[-1];
        return formalArgs()[-1];
    }

    /*
     * Callee
     *
     * Only function frames have a callee. An eval frame in a function has the
     * same caller as its containing function frame.
     */

    js::Value &calleev() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((js::Value *)this)[-2];
        return formalArgs()[-2];
    }

    JSObject &callee() const {
        JS_ASSERT(isFunctionFrame());
        return calleev().toObject();
    }

    JSObject *maybeCallee() const {
        return isFunctionFrame() ? &callee() : NULL;
    }

    js::CallReceiver callReceiver() const {
        return js::CallReceiverFromArgv(formalArgs());
    }

    /*
     * getValidCalleeObject is a fallible getter to compute the correct callee
     * function object, which may require deferred cloning due to the JSObject
     * methodReadBarrier. For a non-function frame, return true with *vp set
     * from calleev, which may not be an object (it could be undefined).
     */
    bool getValidCalleeObject(JSContext *cx, js::Value *vp);

    /*
     * Scope chain
     *
     * Every frame has a scopeChain which, when traversed via the 'parent' link
     * to the root, indicates the current global object. A 'call object' is a
     * node on a scope chain representing a function's activation record. A
     * call object is used for dynamically-scoped name lookup and lexically-
     * scoped upvar access. The call object holds the values of locals and
     * arguments when a function returns (and its stack frame is popped). For
     * performance reasons, call objects are created lazily for 'lightweight'
     * functions, i.e., functions which are not statically known to require a
     * call object. Thus, a given function frame may or may not have a call
     * object. When a function does have a call object, it is found by walking
     * up the scope chain until the first call object. Thus, it is important,
     * when setting the scope chain, to indicate whether the new scope chain
     * contains a new call object and thus changes the 'hasCallObj' state.
     *
     * NB: 'fp->hasCallObj()' implies that fp->callObj() needs to be 'put' when
     * the frame is popped. Since the scope chain of a non-strict eval frame
     * contains the call object of the parent (function) frame, it is possible
     * to have:
     *   !fp->hasCall() && fp->scopeChain().isCall()
     */

    JSObject &scopeChain() const {
        JS_ASSERT_IF(!(flags_ & HAS_SCOPECHAIN), isFunctionFrame());
        if (!(flags_ & HAS_SCOPECHAIN)) {
            scopeChain_ = callee().getParent();
            flags_ |= HAS_SCOPECHAIN;
        }
        return *scopeChain_;
    }

    bool hasCallObj() const {
        bool ret = !!(flags_ & HAS_CALL_OBJ);
        JS_ASSERT_IF(ret, !isNonStrictEvalFrame());
        return ret;
    }

    inline JSObject &callObj() const;
    inline void setScopeChainNoCallObj(JSObject &obj);
    inline void setScopeChainWithOwnCallObj(JSObject &obj);

    /*
     * NB: putActivationObjects does not mark activation objects as having been
     * put (since the frame is about to be popped).
     */
    inline void putActivationObjects();
    inline void markActivationObjectsAsPut();

    /*
     * Frame compartment
     *
     * A stack frame's compartment is the frame's containing context's
     * compartment when the frame was pushed.
     */

    JSCompartment *compartment() const {
        JS_ASSERT_IF(isScriptFrame(), scopeChain().compartment() == script()->compartment);
        return scopeChain().compartment();
    }

    /*
     * Imacropc
     *
     * A frame's IMacro pc is the bytecode address when an imacro started
     * executing (guaranteed non-null). An imacro does not push a frame, so
     * when the imacro finishes, the frame's IMacro pc becomes the current pc.
     */

    bool hasImacropc() const {
        return flags_ & HAS_IMACRO_PC;
    }

    jsbytecode *imacropc() const {
        JS_ASSERT(hasImacropc());
        return imacropc_;
    }

    jsbytecode *maybeImacropc() const {
        return hasImacropc() ? imacropc() : NULL;
    }

    void clearImacropc() {
        flags_ &= ~HAS_IMACRO_PC;
    }

    void setImacropc(jsbytecode *pc) {
        JS_ASSERT(pc);
        JS_ASSERT(!(flags_ & HAS_IMACRO_PC));
        imacropc_ = pc;
        flags_ |= HAS_IMACRO_PC;
    }

    /* Annotation (will be removed after bug 546848) */

    void* annotation() const {
        return (flags_ & HAS_ANNOTATION) ? annotation_ : NULL;
    }

    void setAnnotation(void *annot) {
        flags_ |= HAS_ANNOTATION;
        annotation_ = annot;
    }

    /* Debugger hook data */

    bool hasHookData() const {
        return !!(flags_ & HAS_HOOK_DATA);
    }

    void* hookData() const {
        JS_ASSERT(hasHookData());
        return hookData_;
    }

    void* maybeHookData() const {
        return hasHookData() ? hookData_ : NULL;
    }

    void setHookData(void *v) {
        hookData_ = v;
        flags_ |= HAS_HOOK_DATA;
    }

    /* Return value */

    const js::Value &returnValue() {
        if (!(flags_ & HAS_RVAL))
            rval_.setUndefined();
        return rval_;
    }

    void markReturnValue() {
        flags_ |= HAS_RVAL;
    }

    void setReturnValue(const js::Value &v) {
        rval_ = v;
        markReturnValue();
    }

    void clearReturnValue() {
        rval_.setUndefined();
        markReturnValue();
    }

    /* Native-code return address */

    void *nativeReturnAddress() const {
        return ncode_;
    }

    void setNativeReturnAddress(void *addr) {
        ncode_ = addr;
    }

    void **addressOfNativeReturnAddress() {
        return &ncode_;
    }

    /*
     * Generator-specific members
     *
     * A non-eval function frame may optionally be the activation of a
     * generator. For the most part, generator frames act like ordinary frames.
     * For exceptions, see js_FloatingFrameIfGenerator.
     */

    bool isGeneratorFrame() const {
        return !!(flags_ & GENERATOR);
    }

    bool isFloatingGenerator() const {
        JS_ASSERT_IF(flags_ & FLOATING_GENERATOR, isGeneratorFrame());
        return !!(flags_ & FLOATING_GENERATOR);
    }

    void initFloatingGenerator() {
        JS_ASSERT(!(flags_ & GENERATOR));
        flags_ |= (GENERATOR | FLOATING_GENERATOR);
    }

    void unsetFloatingGenerator() {
        flags_ &= ~FLOATING_GENERATOR;
    }

    void setFloatingGenerator() {
        flags_ |= FLOATING_GENERATOR;
    }

    /*
     * js::Execute pushes both global and function frames (since eval() in a
     * function pushes a frame with isFunctionFrame() && isEvalFrame()). Most
     * code should not care where a frame was pushed, but if it is necessary to
     * pick out frames pushed by js::Execute, this is the right query:
     */

    bool isFramePushedByExecute() const {
        return !!(flags_ & (GLOBAL | EVAL));
    }

    /*
     * Other flags
     */

    bool isConstructing() const {
        return !!(flags_ & CONSTRUCTING);
    }

    uint32 isConstructingFlag() const {
        JS_ASSERT(isFunctionFrame());
        JS_ASSERT((flags_ & ~(CONSTRUCTING | FUNCTION)) == 0);
        return flags_;
    }

    bool isDebuggerFrame() const {
        return !!(flags_ & DEBUGGER);
    }

    bool isDirectEvalOrDebuggerFrame() const {
        return (flags_ & (EVAL | DEBUGGER)) && !(flags_ & GLOBAL);
    }

    bool hasOverriddenArgs() const {
        return !!(flags_ & OVERRIDE_ARGS);
    }

    bool hasOverflowArgs() const {
        return !!(flags_ & OVERFLOW_ARGS);
    }

    void setOverriddenArgs() {
        flags_ |= OVERRIDE_ARGS;
    }

    bool isYielding() {
        return !!(flags_ & YIELDING);
    }

    void setYielding() {
        flags_ |= YIELDING;
    }

    void clearYielding() {
        flags_ &= ~YIELDING;
    }

    void setFinishedInInterpreter() {
        flags_ |= FINISHED_IN_INTERP;
    }

    bool finishedInInterpreter() const {
        return !!(flags_ & FINISHED_IN_INTERP);
    }

#ifdef DEBUG
    /* Poison scopeChain value set before a frame is flushed. */
    static JSObject *const sInvalidScopeChain;
#endif

  public:
    /* Public, but only for JIT use: */

    static size_t offsetOfFlags() {
        return offsetof(StackFrame, flags_);
    }

    static size_t offsetOfExec() {
        return offsetof(StackFrame, exec);
    }

    void *addressOfArgs() {
        return &args;
    }

    static size_t offsetOfScopeChain() {
        return offsetof(StackFrame, scopeChain_);
    }

    JSObject **addressOfScopeChain() {
        JS_ASSERT(flags_ & HAS_SCOPECHAIN);
        return &scopeChain_;
    }

    static size_t offsetOfPrev() {
        return offsetof(StackFrame, prev_);
    }

    static size_t offsetOfReturnValue() {
        return offsetof(StackFrame, rval_);
    }

    static ptrdiff_t offsetOfNcode() {
        return offsetof(StackFrame, ncode_);
    }

    static ptrdiff_t offsetOfCallee(JSFunction *fun) {
        JS_ASSERT(fun != NULL);
        return -(fun->nargs + 2) * sizeof(js::Value);
    }

    static ptrdiff_t offsetOfThis(JSFunction *fun) {
        return fun == NULL
               ? -1 * ptrdiff_t(sizeof(js::Value))
               : -(fun->nargs + 1) * ptrdiff_t(sizeof(js::Value));
    }

    static ptrdiff_t offsetOfFormalArg(JSFunction *fun, uintN i) {
        JS_ASSERT(i < fun->nargs);
        return (-(int)fun->nargs + i) * sizeof(js::Value);
    }

    static size_t offsetOfFixed(uintN i) {
        return sizeof(StackFrame) + i * sizeof(js::Value);
    }

#ifdef JS_METHODJIT
    js::mjit::JITScript *jit() {
        return script()->getJIT(isConstructing());
    }
#endif

    void methodjitStaticAsserts();
};

static const size_t VALUES_PER_STACK_FRAME = sizeof(StackFrame) / sizeof(Value);

inline StackFrame *          Valueify(JSStackFrame *fp) { return (StackFrame *)fp; }
static inline JSStackFrame * Jsvalify(StackFrame *fp)   { return (JSStackFrame *)fp; }

/*****************************************************************************/

class FrameRegs
{
  public:
    Value *sp;
    jsbytecode *pc;
  private:
    StackFrame *fp_;
  public:
    StackFrame *fp() const { return fp_; }

    /* For jit use (need constant): */
    static const size_t offsetOfFp = 2 * sizeof(void *);
    static void staticAssert() {
        JS_STATIC_ASSERT(offsetOfFp == offsetof(FrameRegs, fp_));
    }

    /* For generator: */
    void rebaseFromTo(StackFrame *from, StackFrame *to) {
        fp_ = to;
        sp = to->slots() + (sp - from->slots());
    }

    /* For ContextStack: */
    void popFrame(Value *newsp) {
        pc = fp_->prevpc();
        sp = newsp;
        fp_ = fp_->prev();
    }

    /* For FixupArity: */
    void popPartialFrame(Value *newsp) {
        sp = newsp;
        fp_ = fp_->prev();
    }

    /* For stubs::CompileFunction, ContextStack: */
    void prepareToRun(StackFrame *fp, JSScript *script) {
        pc = script->code;
        sp = fp->slots() + script->nfixed;
        fp_ = fp;
    }

    /* For pushDummyFrame: */
    void initDummyFrame(StackFrame *fp) {
        pc = NULL;
        sp = fp->slots();
        fp_ = fp;
    }
};

/*****************************************************************************/

struct StackOverride
{
    Value         *top;
#ifdef DEBUG
    StackSegment  *seg;
    StackFrame    *frame;
#endif
};

/*****************************************************************************/

class StackSpace
{
    Value         *base_;
#ifdef XP_WIN
    mutable Value *commitEnd_;
#endif
    Value *end_;
    StackSegment  *seg_;
    StackOverride override_;

    static const size_t CAPACITY_VALS  = 512 * 1024;
    static const size_t CAPACITY_BYTES = CAPACITY_VALS * sizeof(Value);
    static const size_t COMMIT_VALS    = 16 * 1024;
    static const size_t COMMIT_BYTES   = COMMIT_VALS * sizeof(Value);

    static void staticAsserts() {
        JS_STATIC_ASSERT(CAPACITY_VALS % COMMIT_VALS == 0);
    }

#ifdef XP_WIN
    JS_FRIEND_API(bool) bumpCommit(JSContext *maybecx, Value *from, ptrdiff_t nvals) const;
#endif

    friend class ContextStack;
    friend struct detail::OOMCheck;
    inline bool ensureSpace(JSContext *maybecx, Value *from, ptrdiff_t nvals) const;
    void pushSegment(StackSegment &seg);
    void popSegment();
    inline void pushOverride(Value *top, StackOverride *prev);
    inline void popOverride(const StackOverride &prev);

  public:
    StackSpace();
    bool init();
    ~StackSpace();

    /* See stack layout comment above. */
    StackSegment *currentSegment() const { return seg_; }
    Value *firstUnused() const;

    /* Optimization of firstUnused when currentSegment() is known active. */
    inline Value *activeFirstUnused() const;

    /* Get the segment containing the target frame. */
    StackSegment &containingSegment(const StackFrame *target) const;

    /*
     * Retrieve the 'variables object' (ES3 term) associated with the given
     * frame's Execution Context's VariableEnvironment (ES5 10.3).
     */
    JSObject &varObjForFrame(const StackFrame *fp);

    /*
     * LeaveTree requires stack allocation to rebuild the stack. There is no
     * good way to handle an OOM for these allocations, so this function checks
     * that OOM cannot occur using the size of the TraceNativeStorage as a
     * conservative upper bound.
     */
    inline bool ensureEnoughSpaceToEnterTrace();

    /*
     * If we let infinite recursion go until it hit the end of the contiguous
     * stack, it would take a long time. As a heuristic, we kill scripts which
     * go deeper than MAX_INLINE_CALLS. Note: this heuristic only applies to a
     * single activation of the VM. If a script reenters, the call count gets
     * reset. This is ok because we will quickly hit the C recursion limit.
     */
    static const size_t MAX_INLINE_CALLS = 3000;

    /*
     * SunSpider and v8bench have roughly an average of 9 slots per script. Our
     * heuristic for a quick over-recursion check uses a generous slot count
     * based on this estimate. We take this frame size and multiply it by the
     * old recursion limit from the interpreter. Worst case, if an average size
     * script (<=9 slots) over recurses, it'll effectively be the same as having
     * increased the old inline call count to <= 5,000.
     */
    static const size_t STACK_QUOTA = MAX_INLINE_CALLS * (VALUES_PER_STACK_FRAME + 18);

    /*
     * In the mjit, we'd like to collapse two "overflow" checks into one:
     *  - the MAX_INLINE_CALLS check (see above comment)
     *  - the stack OOM check (or, on Windows, the commit/OOM check) This
     * function produces a 'limit' pointer that satisfies both these checks.
     * (The STACK_QUOTA comment explains how this limit simulates checking
     * MAX_INLINE_CALLS.) This limit is guaranteed to have at least enough space
     * for cx->fp()->nslots() plus an extra stack frame (which is the min
     * requirement for entering mjit code) or else an error is reported and NULL
     * is returned. When the stack grows past the returned limit, the script may
     * still be within quota, but more memory needs to be committed. This is
     * handled by bumpLimitWithinQuota.
     */
    inline Value *getStackLimit(JSContext *cx);

    /*
     * Try to bump the limit, staying within |base + STACK_QUOTA|, by
     * committing more pages of the contiguous stack.
     *  base: the frame on which execution started
     *  from: the current top of the stack
     *  nvals: requested space above 'from'
     *  *limit: receives bumped new limit
     */
    bool bumpLimitWithinQuota(JSContext *maybecx, StackFrame *base, Value *from, uintN nvals, Value **limit) const;

    /*
     * Raise the given limit without considering quota.
     * See comment in BumpStackFull.
     */
    bool bumpLimit(JSContext *cx, StackFrame *base, Value *from, uintN nvals, Value **limit) const;

    /* Called during GC: mark segments, frames, and slots under firstUnused. */
    void mark(JSTracer *trc);
};

/*****************************************************************************/

class ContextStack
{
    FrameRegs *regs_;
    StackSegment *seg_;
    StackSpace *space_;
    JSContext *cx_;

    /*
     * This is the collecting-point for code that wants to know when there is
     * no JS active. Note that "no JS active" does not mean the stack is empty
     * because of JS_(Save|Restore)FrameChain. If code really wants to know
     * when the stack is empty, test |cx->stack.empty()|.
     */
    void notifyIfNoCodeRunning();

    /*
     * Return whether this ContextStack is running code at the top of the
     * contiguous stack. This is a precondition for extending the current
     * segment by pushing stack frames or overrides etc.
     */
    inline bool isCurrentAndActive() const;

#ifdef DEBUG
    void assertSegmentsInSync() const;
    void assertSpaceInSync() const;
#else
    void assertSegmentsInSync() const {}
    void assertSpaceInSync() const {}
#endif

    friend class FrameGuard;
    bool getSegmentAndFrame(JSContext *cx, uintN vplen, uintN nslots,
                            FrameGuard *frameGuard) const;
    void pushSegmentAndFrame(FrameRegs &regs, FrameGuard *frameGuard);
    void pushSegmentAndFrameImpl(FrameRegs &regs, StackSegment &seg);
    void popSegmentAndFrame();
    void popSegmentAndFrameImpl();

    template <class Check>
    inline StackFrame *getCallFrame(JSContext *cx, Value *sp, uintN nactual,
                                    JSFunction *fun, JSScript *script, uint32 *pflags,
                                    Check check) const;

    friend class InvokeArgsGuard;
    bool pushInvokeArgsSlow(JSContext *cx, uintN argc, InvokeArgsGuard *argsGuard);
    void popInvokeArgsSlow(const InvokeArgsGuard &argsGuard);
    inline void popInvokeArgs(const InvokeArgsGuard &argsGuard);

    friend class InvokeFrameGuard;
    void pushInvokeFrameSlow(InvokeFrameGuard *frameGuard);
    void popInvokeFrameSlow(const InvokeFrameGuard &frameGuard);
    inline void popInvokeFrame(const InvokeFrameGuard &frameGuard);

  public:
    ContextStack(JSContext *cx);
    ~ContextStack();

    /*
     * A context is "empty" if it has no code, running or suspended, on its
     * stack. Running code can be stopped (via JS_SaveFrameChain) which leads
     * to the state |!cx->empty() && cx->running()|.
     */
    bool empty() const           { JS_ASSERT_IF(regs_, seg_); return !seg_; }
    bool running() const         { JS_ASSERT_IF(regs_, regs_->fp()); return !!regs_; }

    /* Current regs of the current segment (see VM stack layout comment). */
    FrameRegs &regs() const      { JS_ASSERT(regs_); return *regs_; }

    /* Convenience helpers. */
    FrameRegs *maybeRegs() const { return regs_; }
    StackFrame *fp() const       { return regs_->fp(); }
    StackFrame *maybefp() const  { return regs_ ? regs_->fp() : NULL; }

    /* The StackSpace currently hosting this ContextStack. */
    StackSpace &space() const    { assertSpaceInSync(); return *space_; }

    /*
     * To avoid indirection, ContextSpace caches a pointers to the StackSpace.
     * This must be kept coherent with cx->thread->data.space by calling
     * 'threadReset' whenver cx->thread changes.
     */
    void threadReset();

    /*
     * As an optimization, the interpreter/mjit can operate on a local
     * FrameRegs instance repoint the ContextStack to this local instance.
     */
    void repointRegs(FrameRegs *regs) {
        JS_ASSERT_IF(regs, regs->fp());
        regs_ = regs;
    }

    /* Return the current segment, which may or may not be active. */
    js::StackSegment *currentSegment() const {
        assertSegmentsInSync();
        return seg_;
    }

    /* This is an optimization of StackSpace::varObjForFrame. */
    inline JSObject &currentVarObj() const;

    /* Search the call stack for the nearest frame with static level targetLevel. */
    inline StackFrame *findFrameAtLevel(uintN targetLevel) const;

#ifdef DEBUG
    /* Return whether the given frame is in this context's stack. */
    bool contains(const StackFrame *fp) const;
#endif

    /* Mark the top segment as suspended, without pushing a new one. */
    void saveActiveSegment();

    /* Undoes calls to suspendActiveSegment. */
    void restoreSegment();

    /*
     * For the five sets of stack operations below:
     *  - The boolean-valued functions call js_ReportOutOfScriptQuota on OOM.
     *  - The "get*Frame" functions do not change any global state, they just
     *    check OOM and return pointers to an uninitialized frame with the
     *    requested missing arguments/slots. Only once the "push*Frame"
     *    function has been called is global state updated. Thus, between
     *    "get*Frame" and "push*Frame", the frame and slots are unrooted.
     *  - Functions taking "*Guard" arguments will use the guard's destructor
     *    to pop the stack. The caller must ensure the guard has the
     *    appropriate lifetime.
     */

    /*
     * pushInvokeArgs allocates |argc + 2| rooted values that will be passed as
     * the arguments to Invoke. A single allocation can be used for multiple
     * Invoke calls. The InvokeArgumentsGuard passed to Invoke must come from
     * an immediately-enclosing (stack-wise) call to pushInvokeArgs.
     */
    bool pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard *ag);

    /* These functions are called inside Invoke, not Invoke clients. */
    inline StackFrame *
    getInvokeFrame(JSContext *cx, const CallArgs &args,
                   JSFunction *fun, JSScript *script, uint32 *flags,
                   InvokeFrameGuard *frameGuard) const;
    void pushInvokeFrame(const CallArgs &args,
                         InvokeFrameGuard *frameGuard);

    /* These functions are called inside Execute, not Execute clients. */
    bool getExecuteFrame(JSContext *cx, JSScript *script,
                         ExecuteFrameGuard *frameGuard) const;
    void pushExecuteFrame(JSObject *initialVarObj,
                          ExecuteFrameGuard *frameGuard);

    /* These functions are called inside SendToGenerator. */
    bool getGeneratorFrame(JSContext *cx, uintN vplen, uintN nslots,
                           GeneratorFrameGuard *frameGuard);
    void pushGeneratorFrame(FrameRegs &regs,
                            GeneratorFrameGuard *frameGuard);

    /* Pushes a StackFrame::isDummyFrame. */
    bool pushDummyFrame(JSContext *cx, JSObject &scopeChain,
                        DummyFrameGuard *frameGuard);

    /*
     * An "inline frame" may only be pushed from within the top, active
     * segment. This is the case for calls made inside mjit code and Interpret.
     * The *WithinLimit variant stays within the stack quota using the given
     * limit (see StackSpace::getStackLimit).
     */
    inline StackFrame *
    getInlineFrame(JSContext *cx, Value *sp, uintN nactual,
                   JSFunction *fun, JSScript *script, uint32 *flags) const;
    inline StackFrame *
    getInlineFrameWithinLimit(JSContext *cx, Value *sp, uintN nactual,
                              JSFunction *fun, JSScript *script, uint32 *flags,
                              StackFrame *base, Value **limit) const;
    inline void pushInlineFrame(JSScript *script, StackFrame *fp, FrameRegs &regs);
    inline void popInlineFrame();

    /* For jit use: */
    static size_t offsetOfRegs() { return offsetof(ContextStack, regs_); }
};

/*****************************************************************************/

class InvokeArgsGuard : public CallArgs
{
    friend class ContextStack;
    ContextStack     *stack_;  /* null implies nothing pushed */
    StackSegment     *seg_;    /* null implies no segment pushed */
    StackOverride    prevOverride_;
  public:
    InvokeArgsGuard() : stack_(NULL), seg_(NULL) {}
    ~InvokeArgsGuard();
    bool pushed() const { return stack_ != NULL; }
};

/*
 * This type can be used to call Invoke when the arguments have already been
 * pushed onto the stack as part of normal execution.
 */
struct InvokeArgsAlreadyOnTheStack : CallArgs
{
    InvokeArgsAlreadyOnTheStack(uintN argc, Value *vp) : CallArgs(argc, vp + 2) {}
};

class InvokeFrameGuard

{
    friend class ContextStack;
    ContextStack *stack_;  /* null implies nothing pushed */
    FrameRegs regs_;
    FrameRegs *prevRegs_;
  public:
    InvokeFrameGuard() : stack_(NULL) {}
    ~InvokeFrameGuard();
    bool pushed() const { return stack_ != NULL; }
    void pop();
    StackFrame *fp() const { return regs_.fp(); }
};

/* Reusable base; not for direct use. */
class FrameGuard
{
    friend class ContextStack;
    ContextStack *stack_;  /* null implies nothing pushed */
    StackSegment *seg_;
    Value *vp_;
    StackFrame *fp_;
  public:
    FrameGuard() : stack_(NULL), vp_(NULL), fp_(NULL) {}
    ~FrameGuard();
    bool pushed() const { return stack_ != NULL; }
    StackSegment *segment() const { return seg_; }
    Value *vp() const { return vp_; }
    StackFrame *fp() const { return fp_; }
};

class ExecuteFrameGuard : public FrameGuard
{
    friend class ContextStack;
    FrameRegs regs_;
};

class DummyFrameGuard : public FrameGuard
{
    friend class ContextStack;
    FrameRegs regs_;
};

class GeneratorFrameGuard : public FrameGuard
{};

/*****************************************************************************/

/*
 * While |cx->fp|'s pc/sp are available in |cx->regs|, to compute the saved
 * value of pc/sp for any other frame, it is necessary to know about that
 * frame's next-frame. This iterator maintains this information when walking
 * a chain of stack frames starting at |cx->fp|.
 *
 * Usage:
 *   for (FrameRegsIter i(cx); !i.done(); ++i)
 *     ... i.fp() ... i.sp() ... i.pc()
 */
class FrameRegsIter
{
    JSContext    *cx_;
    StackSegment *seg_;
    StackFrame   *fp_;
    Value        *sp_;
    jsbytecode   *pc_;

    void initSlow();
    void incSlow(StackFrame *oldfp);

  public:
    inline FrameRegsIter(JSContext *cx);

    bool done() const { return fp_ == NULL; }
    inline FrameRegsIter &operator++();

    StackFrame *fp() const { return fp_; }
    Value *sp() const { return sp_; }
    jsbytecode *pc() const { return pc_; }
};

/*
 * Utility class for iteration over all active stack frames.
 */
class AllFramesIter
{
public:
    AllFramesIter(JSContext *cx);

    bool done() const { return fp_ == NULL; }
    AllFramesIter& operator++();

    StackFrame *fp() const { return fp_; }

private:
    StackSegment *seg_;
    StackFrame *fp_;
};

}  /* namespace js */

#endif /* Stack_h__ */
