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

struct JSContext;
struct JSCompartment;

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

class CallIter;
class FrameRegsIter;
class AllFramesIter;

class ArgumentsObject;

namespace mjit { struct JITScript; }

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
 * access (by jit code) as well as fast call/return. This memory layout is
 * encapsulated by a set of types that describe different regions of memory.
 * This encapsulation has holes: to avoid calling into C++ from generated code,
 * JIT compilers generate code that simulates analogous operations in C++.
 *
 * A sample memory layout of a segment looks like:
 *
 *                          regs
 *       .---------------------------------------------.
 *       |                                             V
 *       |                                   fp .--FrameRegs--. sp
 *       |                                      V             V
 * |StackSegment| slots |StackFrame| slots |StackFrame| slots |
 *                        |      ^           |
 *           ? <----------'      `-----------'
 *                 prev               prev
 *
 * A segment starts with a fixed-size header (js::StackSegment) which logically
 * describes the segment, links it to the rest of the stack, and points to the
 * end of the stack.
 *
 * Each script activation (global or function code) is given a fixed-size header
 * (js::StackFrame) which is associated with the values (called "slots") before
 * and after it. The frame contains bookkeeping information about the activation
 * and links to the previous frame.
 *
 * The slots preceding a (function) StackFrame in memory are the arguments of
 * the call. The slots after a StackFrame in memory are its locals followed by
 * its expression stack. There is no clean line between the arguments of a
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
 * of values is passed to the native. The layout of this array is abstracted by
 * js::CallArgs. With respect to the StackSegment layout above, the args to a
 * native call are inserted anywhere there can be slots. A sample memory layout
 * looks like:
 *
 *                          regs
 *       .----------------------------------------.
 *       |                                        V
 *       |                              fp .--FrameRegs--. sp
 *       |                                 V             V
 * |StackSegment| native call | slots |StackFrame| slots | native call |
 *       |     vp <--argc--> end                        vp <--argc--> end
 *       |         CallArgs <------------------------------ CallArgs
 *       |                               prev                  ^
 *       `-----------------------------------------------------'
 *                                  calls
 *
 * Here there are two native calls on the stack. The start of each native arg
 * range is recorded by a CallArgs element which is prev-linked like stack
 * frames. Note that, in full generality, native and scripted calls can
 * interleave arbitrarily. Thus, the end of a segment is the maximum of its
 * current frame and its current native call. Similarly, the top of the entire
 * thread stack is the end of its current segment.
 *
 * Note that, between any two StackFrames there may be any number
 * of native calls, so the meaning of 'prev' is not 'directly called by'.
 *
 * An additional feature (perhaps not for much longer: bug 650361) is that
 * multiple independent "contexts" can interleave (LIFO) on a single contiguous
 * stack. "Independent" here means that neither context sees the other's
 * frames. Concretely, an embedding may enter the JS engine on cx1 and then,
 * from a native called by the JS engine, reenter the VM on cx2. Changing from
 * cx1 to cx2 causes a new segment to be started for cx2's stack on top of
 * cx1's current segment. These two segments are linked from the perspective of
 * StackSpace, since they are adjacent on the thread's stack, but not from the
 * perspective of cx1 and cx2. Thus, each segment has two links: prevInMemory
 * and prevInContext. Each independent stack is encapsulated and managed by
 * the js::ContextStack object stored in JSContext. ContextStack is the primary
 * interface to the rest of the engine for pushing and popping the stack.
 */

/*****************************************************************************/

class CallReceiver
{
  protected:
#ifdef DEBUG
    mutable bool usedRval_;
    void setUsedRval() const { usedRval_ = true; }
    void clearUsedRval() const { usedRval_ = false; }
#else
    void setUsedRval() const {}
    void clearUsedRval() const {}
#endif
    Value *argv_;
  public:
    friend CallReceiver CallReceiverFromVp(Value *);
    friend CallReceiver CallReceiverFromArgv(Value *);
    Value *base() const { return argv_ - 2; }
    JSObject &callee() const { JS_ASSERT(!usedRval_); return argv_[-2].toObject(); }
    Value &calleev() const { JS_ASSERT(!usedRval_); return argv_[-2]; }
    Value &thisv() const { return argv_[-1]; }

    Value &rval() const {
        setUsedRval();
        return argv_[-2];
    }

    Value *spAfterCall() const {
        setUsedRval();
        return argv_ - 1;
    }

    void calleeHasBeenReset() const {
        clearUsedRval();
    }
};

JS_ALWAYS_INLINE CallReceiver
CallReceiverFromArgv(Value *argv)
{
    CallReceiver receiver;
    receiver.clearUsedRval();
    receiver.argv_ = argv;
    return receiver;
}

JS_ALWAYS_INLINE CallReceiver
CallReceiverFromVp(Value *vp)
{
    return CallReceiverFromArgv(vp + 2);
}

/*****************************************************************************/

class CallArgs : public CallReceiver
{
  protected:
    uintN argc_;
  public:
    friend CallArgs CallArgsFromVp(uintN, Value *);
    friend CallArgs CallArgsFromArgv(uintN, Value *);
    friend CallArgs CallArgsFromSp(uintN, Value *);
    Value &operator[](unsigned i) const { JS_ASSERT(i < argc_); return argv_[i]; }
    Value *argv() const { return argv_; }
    uintN argc() const { return argc_; }
    Value *end() const { return argv_ + argc_; }
};

JS_ALWAYS_INLINE CallArgs
CallArgsFromArgv(uintN argc, Value *argv)
{
    CallArgs args;
    args.clearUsedRval();
    args.argv_ = argv;
    args.argc_ = argc;
    return args;
}

JS_ALWAYS_INLINE CallArgs
CallArgsFromVp(uintN argc, Value *vp)
{
    return CallArgsFromArgv(argc, vp + 2);
}

JS_ALWAYS_INLINE CallArgs
CallArgsFromSp(uintN argc, Value *sp)
{
    return CallArgsFromArgv(argc, sp - argc);
}

/*****************************************************************************/

/*
 * For calls to natives, the InvokeArgsGuard object provides a record of the
 * call for the debugger's callstack. For this to work, the InvokeArgsGuard
 * record needs to know when the call is actually active (because the
 * InvokeArgsGuard can be pushed long before and popped long after the actual
 * call, during which time many stack-observing things can happen).
 */
class CallArgsList : public CallArgs
{
    friend class StackSegment;
    CallArgsList *prev_;
    bool active_;
  public:
    friend CallArgsList CallArgsListFromVp(uintN, Value *, CallArgsList *);
    friend CallArgsList CallArgsListFromArgv(uintN, Value *, CallArgsList *);
    CallArgsList *prev() const { return prev_; }
    bool active() const { return active_; }
    void setActive() { active_ = true; }
    void setInactive() { active_ = false; }
};

JS_ALWAYS_INLINE CallArgsList
CallArgsListFromArgv(uintN argc, Value *argv, CallArgsList *prev)
{
    CallArgsList args;
#ifdef DEBUG
    args.usedRval_ = false;
#endif
    args.argv_ = argv;
    args.argc_ = argc;
    args.prev_ = prev;
    args.active_ = false;
    return args;
}

JS_ALWAYS_INLINE CallArgsList
CallArgsListFromVp(uintN argc, Value *vp, CallArgsList *prev)
{
    return CallArgsListFromArgv(argc, vp + 2, prev);
}

/*****************************************************************************/

enum MaybeConstruct {
    NO_CONSTRUCT           =          0, /* == false */
    CONSTRUCT              =       0x80  /* == StackFrame::CONSTRUCTING, asserted below */
};

enum ExecuteType {
    EXECUTE_GLOBAL         =        0x1, /* == StackFrame::GLOBAL */
    EXECUTE_DIRECT_EVAL    =        0x8, /* == StackFrame::EVAL */
    EXECUTE_INDIRECT_EVAL  =        0x9, /* == StackFrame::GLOBAL | EVAL */
    EXECUTE_DEBUG          =       0x18  /* == StackFrame::EVAL | DEBUGGER */
};

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
    Value               rval_;          /* return value of the frame */
    jsbytecode          *prevpc_;       /* pc of previous frame*/
    jsbytecode          *imacropc_;     /* pc of macro caller */
    void                *hookData_;     /* closure returned by call hook */
    void                *annotation_;   /* perhaps remove with bug 546848 */

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) % sizeof(Value) == 0);
        JS_STATIC_ASSERT(sizeof(StackFrame) % sizeof(Value) == 0);
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
    void initCallFrame(JSContext *cx, JSObject &callee, JSFunction *fun,
                       JSScript *script, uint32 nactual, StackFrame::Flags flags);

    /* Used for SessionInvoke. */
    void resetCallFrame(JSScript *script);

    /* Called by jit stubs and serve as a specification for jit-code. */
    void initJitFrameCallerHalf(StackFrame *prev, StackFrame::Flags flags, void *ncode);
    void initJitFrameEarlyPrologue(JSFunction *fun, uint32 nactual);
    bool initJitFrameLatePrologue(JSContext *cx, Value **limit);

    /* Used for eval. */
    void initExecuteFrame(JSScript *script, StackFrame *prev, FrameRegs *regs,
                          const Value &thisv, JSObject &scopeChain, ExecuteType type);

    /* Used when activating generators. */
    void stealFrameAndSlots(Value *vp, StackFrame *otherfp, Value *othervp, Value *othersp);

    /* Perhaps one fine day we will remove dummy frames. */
    void initDummyFrame(JSContext *cx, JSObject &chain);

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

    bool isEvalInFunction() const {
        return (flags_ & (EVAL | FUNCTION)) == (EVAL | FUNCTION);
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

    Value *slots() const {
        return (Value *)(this + 1);
    }

    Value *base() const {
        return slots() + script()->nfixed;
    }

    Value &varSlot(uintN i) {
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
     *
     * Beware, as the name implies, pcQuadratic can lead to quadratic behavior
     * in loops such as:
     *
     *   for ( ...; fp; fp = fp->prev())
     *     ... fp->pcQuadratic(cx);
     *
     * For such situations, prefer FrameRegsIter; its amortized O(1).
     *
     *   When I get to the bottom I go back to the top of the stack
     *   Where I stop and I turn and I go right back
     *   Till I get to the bottom and I see you again...
     */
    jsbytecode *pcQuadratic(JSContext *cx) const;

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

    Value &formalArg(uintN i) const {
        JS_ASSERT(i < numFormalArgs());
        return formalArgs()[i];
    }

    Value *formalArgs() const {
        JS_ASSERT(hasArgs());
        return (Value *)this - numFormalArgs();
    }

    Value *formalArgsEnd() const {
        JS_ASSERT(hasArgs());
        return (Value *)this;
    }

    Value *maybeFormalArgs() const {
        return (flags_ & (FUNCTION | EVAL)) == FUNCTION
               ? formalArgs()
               : NULL;
    }

    inline uintN numActualArgs() const;
    inline Value *actualArgs() const;
    inline Value *actualArgsEnd() const;

    inline Value &canonicalActualArg(uintN i) const;
    template <class Op>
    inline bool forEachCanonicalActualArg(Op op, uintN start = 0, uintN count = uintN(-1));
    template <class Op> inline bool forEachFormalArg(Op op);

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

    Value &functionThis() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((Value *)this)[-1];
        return formalArgs()[-1];
    }

    JSObject &constructorThis() const {
        JS_ASSERT(hasArgs());
        return formalArgs()[-1].toObject();
    }

    Value &globalThis() const {
        JS_ASSERT(isGlobalFrame());
        return ((Value *)this)[-1];
    }

    Value &thisValue() const {
        if (flags_ & (EVAL | GLOBAL))
            return ((Value *)this)[-1];
        return formalArgs()[-1];
    }

    /*
     * Callee
     *
     * Only function frames have a callee. An eval frame in a function has the
     * same caller as its containing function frame. maybeCalleev can be used
     * to return a value that is either caller object (for function frames) or
     * null (for global frames).
     */

    JSObject &callee() const {
        JS_ASSERT(isFunctionFrame());
        return calleev().toObject();
    }

    const Value &calleev() const {
        JS_ASSERT(isFunctionFrame());
        return mutableCalleev();
    }

    const Value &maybeCalleev() const {
        JS_ASSERT(isScriptFrame());
        Value &calleev = flags_ & (EVAL | GLOBAL)
                         ? ((Value *)this)[-2]
                         : formalArgs()[-2];
        JS_ASSERT(calleev.isObjectOrNull());
        return calleev;
    }

    /*
     * Beware! Ad hoc changes can corrupt the stack layout; the callee should
     * only be changed to something that is equivalent to the current callee in
     * terms of numFormalArgs etc. Prefer overwriteCallee since it checks.
     */
    void overwriteCallee(JSObject &newCallee) {
        JS_ASSERT(callee().getFunctionPrivate() == newCallee.getFunctionPrivate());
        mutableCalleev().setObject(newCallee);
    }

    Value &mutableCalleev() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((Value *)this)[-2];
        return formalArgs()[-2];
    }

    /*
     * Compute the callee function for this stack frame, cloning if needed to
     * implement the method read barrier.  If this is not a function frame,
     * set *vp to null.
     */
    bool getValidCalleeObject(JSContext *cx, Value *vp);

    CallReceiver callReceiver() const {
        return CallReceiverFromArgv(formalArgs());
    }

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
     * Variables object
     *
     * Given that a (non-dummy) StackFrame corresponds roughly to a ES5
     * Execution Context (ES5 10.3), StackFrame::varObj corresponds to the
     * VariableEnvironment component of a Exection Context. Intuitively, the
     * variables object is where new bindings (variables and functions) are
     * stored. One might expect that this is either the callObj or
     * scopeChain.globalObj for function or global code, respectively, however
     * the JSAPI allows calls of Execute to specify a variables object on the
     * scope chain other than the call/global object. This allows embeddings to
     * run multiple scripts under the same global, each time using a new
     * variables object to collect and discard the script's global variables.
     */

    JSObject &varObj() {
        JSObject *obj = &scopeChain();
        while (!obj->isVarObj())
            obj = obj->getParent();
        return *obj;
    }

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

    const Value &returnValue() {
        if (!(flags_ & HAS_RVAL))
            rval_.setUndefined();
        return rval_;
    }

    void markReturnValue() {
        flags_ |= HAS_RVAL;
    }

    void setReturnValue(const Value &v) {
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

    MaybeConstruct isConstructing() const {
        JS_STATIC_ASSERT((int)CONSTRUCT == (int)CONSTRUCTING);
        JS_STATIC_ASSERT((int)NO_CONSTRUCT == 0);
        return MaybeConstruct(flags_ & CONSTRUCTING);
    }

    bool isDebuggerFrame() const {
        return !!(flags_ & DEBUGGER);
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

    static size_t offsetOfArgs() {
        return offsetof(StackFrame, args);
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
        return -(fun->nargs + 2) * sizeof(Value);
    }

    static ptrdiff_t offsetOfThis(JSFunction *fun) {
        return fun == NULL
               ? -1 * ptrdiff_t(sizeof(Value))
               : -(fun->nargs + 1) * ptrdiff_t(sizeof(Value));
    }

    static ptrdiff_t offsetOfFormalArg(JSFunction *fun, uintN i) {
        JS_ASSERT(i < fun->nargs);
        return (-(int)fun->nargs + i) * sizeof(Value);
    }

    static size_t offsetOfFixed(uintN i) {
        return sizeof(StackFrame) + i * sizeof(Value);
    }

#ifdef JS_METHODJIT
    mjit::JITScript *jit() {
        return script()->getJIT(isConstructing());
    }
#endif

    void methodjitStaticAsserts();
};

static const size_t VALUES_PER_STACK_FRAME = sizeof(StackFrame) / sizeof(Value);

static inline uintN
ToReportFlags(MaybeConstruct construct)
{
    return uintN(construct);
}

static inline StackFrame::Flags
ToFrameFlags(MaybeConstruct construct)
{
    JS_STATIC_ASSERT((int)CONSTRUCT == (int)StackFrame::CONSTRUCTING);
    JS_STATIC_ASSERT((int)NO_CONSTRUCT == 0);
    return StackFrame::Flags(construct);
}

static inline MaybeConstruct
MaybeConstructFromBool(bool b)
{
    return b ? CONSTRUCT : NO_CONSTRUCT;
}

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
    void rebaseFromTo(const FrameRegs &from, StackFrame &to) {
        fp_ = &to;
        sp = to.slots() + (from.sp - from.fp_->slots());
        pc = from.pc;
        JS_ASSERT(fp_);
    }

    /* For ContextStack: */
    void popFrame(Value *newsp) {
        pc = fp_->prevpc();
        sp = newsp;
        fp_ = fp_->prev();
        JS_ASSERT(fp_);
    }

    /* For FixupArity: */
    void popPartialFrame(Value *newsp) {
        sp = newsp;
        fp_ = fp_->prev();
        JS_ASSERT(fp_);
    }

    /* For stubs::CompileFunction, ContextStack: */
    void prepareToRun(StackFrame &fp, JSScript *script) {
        pc = script->code;
        sp = fp.slots() + script->nfixed;
        fp_ = &fp;
        JS_ASSERT(fp_);
    }

    /* For pushDummyFrame: */
    void initDummyFrame(StackFrame &fp) {
        pc = NULL;
        sp = fp.slots();
        fp_ = &fp;
        JS_ASSERT(fp_);
    }
};

/*****************************************************************************/

class StackSegment
{
    /* Previous segment within same context stack. */
    StackSegment *const prevInContext_;

    /* Previous segment sequentially in memory. */
    StackSegment *const prevInMemory_;

    /* Execution registers for most recent script in this segment (or null). */
    FrameRegs *regs_;

    /* Call args for most recent native call in this segment (or null). */
    CallArgsList *calls_;

  public:
    StackSegment(StackSegment *prevInContext,
                 StackSegment *prevInMemory,
                 FrameRegs *regs,
                 CallArgsList *calls)
      : prevInContext_(prevInContext),
        prevInMemory_(prevInMemory),
        regs_(regs),
        calls_(calls)
    {}

    /* A segment is followed in memory by the arguments of the first call. */

    Value *slotsBegin() const {
        return (Value *)(this + 1);
    }

    /* Accessors. */

    FrameRegs &regs() const {
        JS_ASSERT(regs_);
        return *regs_;
    }

    FrameRegs *maybeRegs() const {
        return regs_;
    }

    StackFrame *fp() const {
        return regs_->fp();
    }

    StackFrame *maybefp() const {
        return regs_ ? regs_->fp() : NULL;
    }

    CallArgsList &calls() const {
        JS_ASSERT(calls_);
        return *calls_;
    }

    CallArgsList *maybeCalls() const {
        return calls_;
    }

    Value *callArgv() const {
        return calls_->argv();
    }

    Value *maybeCallArgv() const {
        return calls_ ? calls_->argv() : NULL;
    }

    StackSegment *prevInContext() const {
        return prevInContext_;
    }

    StackSegment *prevInMemory() const {
        return prevInMemory_;
    }

    void repointRegs(FrameRegs *regs) {
        JS_ASSERT_IF(regs, regs->fp());
        regs_ = regs;
    }

    bool isEmpty() const {
        return !calls_ && !regs_;
    }

    bool contains(const StackFrame *fp) const;
    bool contains(const FrameRegs *regs) const;
    bool contains(const CallArgsList *call) const;

    StackFrame *computeNextFrame(const StackFrame *fp) const;

    Value *end() const;

    FrameRegs *pushRegs(FrameRegs &regs);
    void popRegs(FrameRegs *regs);
    void pushCall(CallArgsList &callList);
    void pointAtCall(CallArgsList &callList);
    void popCall();

    /* For jit access: */

    static const size_t offsetOfRegs() { return offsetof(StackSegment, regs_); }
};

static const size_t VALUES_PER_STACK_SEGMENT = sizeof(StackSegment) / sizeof(Value);
JS_STATIC_ASSERT(sizeof(StackSegment) % sizeof(Value) == 0);

/*****************************************************************************/

class StackSpace
{
    StackSegment  *seg_;
    Value         *base_;
    mutable Value *conservativeEnd_;
#ifdef XP_WIN
    mutable Value *commitEnd_;
#endif
    Value         *defaultEnd_;
    Value         *trustedEnd_;

    void assertInvariants() const {
        JS_ASSERT(base_ <= conservativeEnd_);
#ifdef XP_WIN
        JS_ASSERT(conservativeEnd_ <= commitEnd_);
        JS_ASSERT(commitEnd_ <= trustedEnd_);
#endif
        JS_ASSERT(conservativeEnd_ <= defaultEnd_);
        JS_ASSERT(defaultEnd_ <= trustedEnd_);
    }

    /* The total number of values/bytes reserved for the stack. */
    static const size_t CAPACITY_VALS  = 512 * 1024;
    static const size_t CAPACITY_BYTES = CAPACITY_VALS * sizeof(Value);

    /* How much of the stack is initially committed. */
    static const size_t COMMIT_VALS    = 16 * 1024;
    static const size_t COMMIT_BYTES   = COMMIT_VALS * sizeof(Value);

    /* How much space is reserved at the top of the stack for trusted JS. */
    static const size_t BUFFER_VALS    = 16 * 1024;
    static const size_t BUFFER_BYTES   = BUFFER_VALS * sizeof(Value);

    static void staticAsserts() {
        JS_STATIC_ASSERT(CAPACITY_VALS % COMMIT_VALS == 0);
    }

    friend class AllFramesIter;
    friend class ContextStack;
    friend class StackFrame;

    /*
     * Except when changing compartment (see pushDummyFrame), the 'dest'
     * parameter of ensureSpace is cx->compartment. Ideally, we'd just pass
     * this directly (and introduce a helper that supplies cx->compartment when
     * no 'dest' is given). For some compilers, this really hurts performance,
     * so, instead, a trivially sinkable magic constant is used to indicate
     * that dest should be cx->compartment.
     */
    static const size_t CX_COMPARTMENT = 0xc;

    inline bool ensureSpace(JSContext *cx, MaybeReportError report,
                            Value *from, ptrdiff_t nvals,
                            JSCompartment *dest = (JSCompartment *)CX_COMPARTMENT) const;
    JS_FRIEND_API(bool) ensureSpaceSlow(JSContext *cx, MaybeReportError report,
                                        Value *from, ptrdiff_t nvals,
                                        JSCompartment *dest) const;

    StackSegment &findContainingSegment(const StackFrame *target) const;

  public:
    StackSpace();
    bool init();
    ~StackSpace();

    /*
     * Maximum supported value of arguments.length. This bounds the maximum
     * number of arguments that can be supplied to Function.prototype.apply.
     * This value also bounds the number of elements parsed in an array
     * initialiser.
     *
     * Since arguments are copied onto the stack, the stack size is the
     * limiting factor for this constant. Use the max stack size (available to
     * untrusted code) with an extra buffer so that, after such an apply, the
     * callee can do a little work without OOMing.
     */
    static const uintN ARGS_LENGTH_MAX = CAPACITY_VALS - (2 * BUFFER_VALS);

    /* See stack layout comment above. */
    Value *firstUnused() const { return seg_ ? seg_->end() : base_; }

#ifdef JS_TRACER
    /*
     * LeaveTree requires stack allocation to rebuild the stack. There is no
     * good way to handle an OOM for these allocations, so this function checks
     * that OOM cannot occur using the size of the TraceNativeStorage as a
     * conservative upper bound.
     *
     * Despite taking a 'cx', this function does not report an error if it
     * returns 'false'.
     */
    inline bool ensureEnoughSpaceToEnterTrace(JSContext *cx);
#endif

    /*
     * Return a limit against which jit code can check for. This limit is not
     * necessarily the end of the stack since we lazily commit stack memory on
     * some platforms. Thus, when the stack limit is exceeded, the caller should
     * use tryBumpLimit to attempt to increase the stack limit by committing
     * more memory. If the stack is truly exhausted, tryBumpLimit will report an
     * error and return NULL.
     *
     * An invariant of the methodjit is that there is always space to push a
     * frame on top of the current frame's expression stack (which can be at
     * most script->nslots deep). getStackLimit ensures that the returned limit
     * does indeed have this required space and reports an error and returns
     * NULL if this reserve space cannot be allocated.
     */
    inline Value *getStackLimit(JSContext *cx, MaybeReportError report);
    bool tryBumpLimit(JSContext *cx, Value *from, uintN nvals, Value **limit);

    /* Called during GC: mark segments, frames, and slots under firstUnused. */
    void mark(JSTracer *trc);

    /* We only report the committed size;  uncommitted size is uninteresting. */
    JS_FRIEND_API(size_t) committedSize();
};

/*****************************************************************************/

class ContextStack
{
    StackSegment *seg_;
    StackSpace *space_;
    JSContext *cx_;

    /*
     * Return whether this ContextStack is at the top of the contiguous stack.
     * This is a precondition for extending the current segment by pushing
     * stack frames or overrides etc.
     *
     * NB: Just because a stack is onTop() doesn't mean there is necessarily
     * a frame pushed on the stack. For this, use hasfp().
     */
    bool onTop() const;

#ifdef DEBUG
    void assertSpaceInSync() const;
#else
    void assertSpaceInSync() const {}
#endif

    /* Implementation details of push* public interface. */
    StackSegment *pushSegment(JSContext *cx);
    enum MaybeExtend { CAN_EXTEND = true, CANT_EXTEND = false };
    Value *ensureOnTop(JSContext *cx, MaybeReportError report, uintN nvars,
                       MaybeExtend extend, bool *pushedSeg,
                       JSCompartment *dest = (JSCompartment *)StackSpace::CX_COMPARTMENT);

    inline StackFrame *
    getCallFrame(JSContext *cx, MaybeReportError report, const CallArgs &args,
                 JSFunction *fun, JSScript *script, StackFrame::Flags *pflags) const;

    /* Make pop* functions private since only called by guard classes. */
    void popSegment();
    friend class InvokeArgsGuard;
    void popInvokeArgs(const InvokeArgsGuard &iag);
    friend class FrameGuard;
    void popFrame(const FrameGuard &fg);
    friend class GeneratorFrameGuard;
    void popGeneratorFrame(const GeneratorFrameGuard &gfg);

    friend class StackIter;

  public:
    ContextStack(JSContext *cx);
    ~ContextStack();

    /*** Stack accessors ***/

    /*
     * A context's stack is "empty" if there are no scripts or natives
     * executing. Note that JS_SaveFrameChain does factor into this definition.
     */
    bool empty() const                { return !seg_; }

    /*
     * Return whether there has been at least one frame pushed since the most
     * recent call to JS_SaveFrameChain. Note that natives do not have frames
     * and dummy frames are frames that do not represent script execution hence
     * this query has little semantic meaning past "you can call fp()".
     */
    bool hasfp() const                { return seg_ && seg_->maybeRegs(); }

    /*
     * Return the most recent script activation's registers with the same
     * caveat as hasfp regarding JS_SaveFrameChain.
     */
    FrameRegs *maybeRegs() const      { return seg_ ? seg_->maybeRegs() : NULL; }
    StackFrame *maybefp() const       { return seg_ ? seg_->maybefp() : NULL; }

    /* Faster alternatives to maybe* functions. */
    FrameRegs &regs() const           { JS_ASSERT(hasfp()); return seg_->regs(); }
    StackFrame *fp() const            { JS_ASSERT(hasfp()); return seg_->fp(); }

    /* The StackSpace currently hosting this ContextStack. */
    StackSpace &space() const    { assertSpaceInSync(); return *space_; }

    /* Return whether the given frame is in this context's stack. */
    bool containsSlow(const StackFrame *target) const;

    /*** Stack manipulation ***/

    /*
     * pushInvokeArgs allocates |argc + 2| rooted values that will be passed as
     * the arguments to Invoke. A single allocation can be used for multiple
     * Invoke calls. The InvokeArgumentsGuard passed to Invoke must come from
     * an immediately-enclosing (stack-wise) call to pushInvokeArgs.
     */
    bool pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard *ag);

    /* Called by Invoke for a scripted function call. */
    bool pushInvokeFrame(JSContext *cx, const CallArgs &args,
                         MaybeConstruct construct, InvokeFrameGuard *ifg);

    /* Called by Execute for execution of eval or global code. */
    bool pushExecuteFrame(JSContext *cx, JSScript *script, const Value &thisv,
                          JSObject &scopeChain, ExecuteType type,
                          StackFrame *evalInFrame, ExecuteFrameGuard *efg);

    /*
     * Called by SendToGenerator to resume a yielded generator. In addition to
     * pushing a frame onto the VM stack, this function copies over the
     * floating frame stored in 'gen'. When 'gfg' is destroyed, the destructor
     * will copy the frame back to the floating frame.
     */
    bool pushGeneratorFrame(JSContext *cx, JSGenerator *gen, GeneratorFrameGuard *gfg);

    /*
     * When changing the compartment of a cx, it is necessary to immediately
     * change the scope chain to a global in the right compartment since any
     * amount of general VM code can run before the first scripted frame is
     * pushed (if at all). This is currently and hackily accomplished by
     * pushing a "dummy frame" with the correct scope chain. On success, this
     * function will change the compartment to 'scopeChain.compartment()' and
     * push a dummy frame for 'scopeChain'. On failure, nothing is changed.
     */
    bool pushDummyFrame(JSContext *cx, JSCompartment *dest, JSObject &scopeChain, DummyFrameGuard *dfg);

    /*
     * An "inline frame" may only be pushed from within the top, active
     * segment. This is the case for calls made inside mjit code and Interpret.
     * The 'stackLimit' overload updates 'stackLimit' if it changes.
     */
    bool pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                         JSObject &callee, JSFunction *fun, JSScript *script,
                         MaybeConstruct construct);
    bool pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                         JSObject &callee, JSFunction *fun, JSScript *script,
                         MaybeConstruct construct, Value **stackLimit);
    void popInlineFrame(FrameRegs &regs);

    /* Pop a partially-pushed frame after hitting the limit before throwing. */
    void popFrameAfterOverflow();

    /*
     * Called by the methodjit for an arity mismatch. Arity mismatch can be
     * hot, so getFixupFrame avoids doing call setup performed by jit code when
     * FixupArity returns. In terms of work done:
     *
     *   getFixupFrame = pushInlineFrame -
     *                   (fp->initJitFrameLatePrologue + regs->prepareToRun)
     */
    StackFrame *getFixupFrame(JSContext *cx, MaybeReportError report,
                              const CallArgs &args, JSFunction *fun, JSScript *script,
                              void *ncode, MaybeConstruct construct, Value **stackLimit);

    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * As an optimization, the interpreter/mjit can operate on a local
     * FrameRegs instance repoint the ContextStack to this local instance.
     */
    void repointRegs(FrameRegs *regs) { JS_ASSERT(hasfp()); seg_->repointRegs(regs); }

    /*** For JSContext: ***/

    /*
     * To avoid indirection, ContextSpace caches a pointer to the StackSpace.
     * This must be kept coherent with cx->thread->data.space by calling
     * 'threadReset' whenver cx->thread changes.
     */
    void threadReset();

    /*** For jit compiler: ***/

    static size_t offsetOfSeg() { return offsetof(ContextStack, seg_); }
};

/*****************************************************************************/

class InvokeArgsGuard : public CallArgsList
{
    friend class ContextStack;
    ContextStack *stack_;
    bool pushedSeg_;
    void setPushed(ContextStack &stack) { JS_ASSERT(!pushed()); stack_ = &stack; }
  public:
    InvokeArgsGuard() : CallArgsList(), stack_(NULL), pushedSeg_(false) {}
    ~InvokeArgsGuard() { if (pushed()) stack_->popInvokeArgs(*this); }
    bool pushed() const { return !!stack_; }
    void pop() { stack_->popInvokeArgs(*this); stack_ = NULL; }
};

class FrameGuard
{
  protected:
    friend class ContextStack;
    ContextStack *stack_;
    bool pushedSeg_;
    FrameRegs regs_;
    FrameRegs *prevRegs_;
    void setPushed(ContextStack &stack) { stack_ = &stack; }
  public:
    FrameGuard() : stack_(NULL), pushedSeg_(false) {}
    ~FrameGuard() { if (pushed()) stack_->popFrame(*this); }
    bool pushed() const { return !!stack_; }
    void pop() { stack_->popFrame(*this); stack_ = NULL; }

    StackFrame *fp() const { return regs_.fp(); }
};

class InvokeFrameGuard : public FrameGuard
{};

class ExecuteFrameGuard : public FrameGuard
{};

class DummyFrameGuard : public FrameGuard
{};

class GeneratorFrameGuard : public FrameGuard
{
    friend class ContextStack;
    JSGenerator *gen_;
    Value *stackvp_;
  public:
    ~GeneratorFrameGuard() { if (pushed()) stack_->popGeneratorFrame(*this); }
};

/*****************************************************************************/

/*
 * Iterate through the callstack of the given context. Each element of said
 * callstack can either be the execution of a script (scripted function call,
 * global code, eval code, debugger code) or the invocation of a (C++) native.
 * Example usage:
 *
 *   for (Stackiter i(cx); !i.done(); ++i) {
 *     if (i.isScript()) {
 *       ... i.fp() ... i.sp() ... i.pc()
 *     } else {
 *       JS_ASSERT(i.isNativeCall());
 *       ... i.args();
 *     }
 *
 * The SavedOption parameter additionally lets the iterator continue through
 * breaks in the callstack (from JS_SaveFrameChain). The default is to stop.
 */
class StackIter
{
    friend class ContextStack;
    JSContext    *cx_;
  public:
    enum SavedOption { STOP_AT_SAVED, GO_THROUGH_SAVED };
  private:
    SavedOption  savedOption_;

    enum State { DONE, SCRIPTED, NATIVE, IMPLICIT_NATIVE };
    State        state_;

    StackFrame   *fp_;
    CallArgsList *calls_;

    StackSegment *seg_;
    Value        *sp_;
    jsbytecode   *pc_;
    CallArgs     args_;

    void poisonRegs();
    void popFrame();
    void popCall();
    void settleOnNewSegment();
    void settleOnNewState();
    void startOnSegment(StackSegment *seg);

  public:
    StackIter(JSContext *cx, SavedOption = STOP_AT_SAVED);

    bool done() const { return state_ == DONE; }
    StackIter &operator++();

    bool operator==(const StackIter &rhs) const;
    bool operator!=(const StackIter &rhs) const { return !(*this == rhs); }

    bool isScript() const { JS_ASSERT(!done()); return state_ == SCRIPTED; }
    StackFrame *fp() const { JS_ASSERT(!done() && isScript()); return fp_; }
    Value      *sp() const { JS_ASSERT(!done() && isScript()); return sp_; }
    jsbytecode *pc() const { JS_ASSERT(!done() && isScript()); return pc_; }

    bool isNativeCall() const { JS_ASSERT(!done()); return state_ != SCRIPTED; }
    CallArgs nativeArgs() const { JS_ASSERT(!done() && isNativeCall()); return args_; }
};

/* A filtering of the StackIter to only stop at scripts. */
class FrameRegsIter
{
    StackIter iter_;

    void settle() {
        while (!iter_.done() && !iter_.isScript())
            ++iter_;
    }

  public:
    FrameRegsIter(JSContext *cx, StackIter::SavedOption opt = StackIter::STOP_AT_SAVED)
        : iter_(cx, opt) { settle(); }

    bool done() const { return iter_.done(); }
    FrameRegsIter &operator++() { ++iter_; settle(); return *this; }

    bool operator==(const FrameRegsIter &rhs) const { return iter_ == rhs.iter_; }
    bool operator!=(const FrameRegsIter &rhs) const { return iter_ != rhs.iter_; }

    StackFrame *fp() const { return iter_.fp(); }
    Value      *sp() const { return iter_.sp(); }
    jsbytecode *pc() const { return iter_.pc(); }
};

/*****************************************************************************/

/*
 * Blindly iterate over all frames in the current thread's stack. These frames
 * can be from different contexts and compartments, so beware.
 */
class AllFramesIter
{
  public:
    AllFramesIter(StackSpace &space);

    bool done() const { return fp_ == NULL; }
    AllFramesIter& operator++();

    StackFrame *fp() const { return fp_; }

  private:
    StackSegment *seg_;
    StackFrame *fp_;
};

}  /* namespace js */

#endif /* Stack_h__ */
