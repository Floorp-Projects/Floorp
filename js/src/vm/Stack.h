/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Stack_h__
#define Stack_h__

#include "jsfun.h"
#include "jsautooplen.h"

struct JSContext;
struct JSCompartment;

extern void js_DumpStackFrame(JSContext *, js::StackFrame *);

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
class ScriptFrameIter;
class AllFramesIter;

class ArgumentsObject;
class ScopeObject;
class StaticBlockObject;

struct ScopeCoordinate;

#ifdef JS_METHODJIT
namespace mjit {
    class CallCompiler;
    class GetPropCompiler;
    struct CallSite;
    struct JITScript;
    jsbytecode *NativeToPC(JITScript *jit, void *ncode, CallSite **pinline);
    namespace ic { struct GetElementIC; }
}
typedef mjit::CallSite InlinedSite;
#else
struct InlinedSite {};
#endif
typedef size_t FrameRejoinState;

namespace detail {
    struct OOMCheck;
}

/*****************************************************************************/

/*
 * VM stack layout
 *
 * SpiderMonkey uses a per-runtime stack to store the activation records,
 * parameters, locals, and expression temporaries for the stack of actively
 * executing scripts, functions and generators.
 *
 * The stack is subdivided into contiguous segments of memory which
 * have a memory layout invariant that allows fixed offsets to be used for stack
 * access (by jit code) as well as fast call/return. This memory layout is
 * encapsulated by a set of types that describe different regions of memory.
 * This encapsulation has holes: to avoid calling into C++ from generated code,
 * JIT compilers generate code that simulates analogous operations in C++.
 *
 * A sample memory layout of a segment looks like:
 *
 *                          regs
 *       .------------------------------------------------.
 *       |                                                V
 *       |                                      fp .--FrameRegs--. sp
 *       |                                         V             V
 * |StackSegment| values |StackFrame| values |StackFrame| values |
 *                         |      ^            |
 *           ? <-----------'      `------------'
 *                 prev               prev
 *
 * A segment starts with a fixed-size header (js::StackSegment) which logically
 * describes the segment, links it to the rest of the stack, and points to the
 * end of the stack.
 *
 * Each script activation (global or function code) is given a fixed-size header
 * (js::StackFrame) which is associated with the values before and after it.
 * The frame contains bookkeeping information about the activation and links to
 * the previous frame.
 *
 * The value preceding a (function) StackFrame in memory are the arguments of
 * the call. The values after a StackFrame in memory are its locals followed by
 * its expression stack. There is no clean line between the arguments of a
 * frame and the expression stack of the previous frame since the top values of
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
 * native call are inserted anywhere there can be values. A sample memory layout
 * looks like:
 *
 *                          regs
 *       .------------------------------------------.
 *       |                                          V
 *       |                                fp .--FrameRegs--. sp
 *       |                                   V             V
 * |StackSegment| native call | values |StackFrame| values | native call |
 *       |       vp <--argc--> end                        vp <--argc--> end
 *       |           CallArgs <------------------------------ CallArgs
 *       |                                 prev                  ^
 *       `-------------------------------------------------------'
 *                                    calls
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
 * stack. "Independent" here means that each context has its own callstack.
 * Note, though, that eval-in-frame allows one context's callstack to join
 * another context's callstack. Thus, in general, the structure of calls in a
 * StackSpace is a forest.
 *
 * More concretely, an embedding may enter the JS engine on cx1 and then, from
 * a native called by the JS engine, reenter the VM on cx2. Changing from cx1
 * to cx2 causes a new segment to be started for cx2's stack on top of cx1's
 * current segment. These two segments are linked from the perspective of
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

    void setCallee(Value calleev) {
        clearUsedRval();
        this->calleev() = calleev;
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
    unsigned argc_;
  public:
    friend CallArgs CallArgsFromVp(unsigned, Value *);
    friend CallArgs CallArgsFromArgv(unsigned, Value *);
    friend CallArgs CallArgsFromSp(unsigned, Value *);
    Value &operator[](unsigned i) const { JS_ASSERT(i < argc_); return argv_[i]; }
    Value *array() const { return argv_; }
    unsigned length() const { return argc_; }
    Value *end() const { return argv_ + argc_; }
    bool hasDefined(unsigned i) const { return i < argc_ && !argv_[i].isUndefined(); }
};

JS_ALWAYS_INLINE CallArgs
CallArgsFromArgv(unsigned argc, Value *argv)
{
    CallArgs args;
    args.clearUsedRval();
    args.argv_ = argv;
    args.argc_ = argc;
    return args;
}

JS_ALWAYS_INLINE CallArgs
CallArgsFromVp(unsigned argc, Value *vp)
{
    return CallArgsFromArgv(argc, vp + 2);
}

JS_ALWAYS_INLINE CallArgs
CallArgsFromSp(unsigned argc, Value *sp)
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
    friend CallArgsList CallArgsListFromVp(unsigned, Value *, CallArgsList *);
    friend CallArgsList CallArgsListFromArgv(unsigned, Value *, CallArgsList *);
    CallArgsList *prev() const { return prev_; }
    bool active() const { return active_; }
    void setActive() { active_ = true; }
    void setInactive() { active_ = false; }
};

JS_ALWAYS_INLINE CallArgsList
CallArgsListFromArgv(unsigned argc, Value *argv, CallArgsList *prev)
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
CallArgsListFromVp(unsigned argc, Value *vp, CallArgsList *prev)
{
    return CallArgsListFromArgv(argc, vp + 2, prev);
}

/*****************************************************************************/

enum MaybeCheckAliasing { CHECK_ALIASING = true, DONT_CHECK_ALIASING = false };

/*****************************************************************************/

/* Flags specified for a frame as it is constructed. */
enum InitialFrameFlags {
    INITIAL_NONE           =          0,
    INITIAL_CONSTRUCT      =       0x40, /* == StackFrame::CONSTRUCTING, asserted below */
    INITIAL_LOWERED        =   0x200000  /* == StackFrame::LOWERED_CALL_APPLY, asserted below */
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
        CONSTRUCTING       =       0x40,  /* frame is for a constructor invocation */

        /* Temporary frame states */
        YIELDING           =       0x80,  /* Interpret dispatched JSOP_YIELD */
        FINISHED_IN_INTERP =      0x100,  /* set if frame finished in Interpret() */

        /* Function arguments */
        OVERFLOW_ARGS      =      0x200,  /* numActualArgs > numFormalArgs */
        UNDERFLOW_ARGS     =      0x400,  /* numActualArgs < numFormalArgs */

        /* Function prologue state */
        HAS_CALL_OBJ       =      0x800,  /* CallObject created for heavyweight fun */
        HAS_ARGS_OBJ       =     0x1000,  /* ArgumentsObject created for needsArgsObj script */
        HAS_NESTING        =     0x2000,  /* NestingPrologue called for frame */

        /* Lazy frame initialization */
        HAS_HOOK_DATA      =     0x4000,  /* frame has hookData_ set */
        HAS_ANNOTATION     =     0x8000,  /* frame has annotation_ set */
        HAS_RVAL           =    0x10000,  /* frame has rval_ set */
        HAS_SCOPECHAIN     =    0x20000,  /* frame has scopeChain_ set */
        HAS_PREVPC         =    0x40000,  /* frame has prevpc_ and prevInline_ set */
        HAS_BLOCKCHAIN     =    0x80000,  /* frame has blockChain_ set */

        /* Method JIT state */
        DOWN_FRAMES_EXPANDED = 0x100000,  /* inlining in down frames has been expanded */
        LOWERED_CALL_APPLY   = 0x200000,  /* Pushed by a lowered call/apply */

        /* Debugger state */
        PREV_UP_TO_DATE    =   0x400000   /* see DebugScopes::updateLiveScopes */
    };

  private:
    mutable uint32_t    flags_;         /* bits described by Flags */
    union {                             /* describes what code is executing in a */
        JSScript        *script;        /*   global frame */
        JSFunction      *fun;           /*   function frame, pre GetScopeChain */
    } exec;
    union {                             /* describes the arguments of a function */
        unsigned        nactual;        /*   for non-eval frames */
        JSScript        *evalScript;    /*   the script of an eval-in-function */
    } u;
    mutable JSObject    *scopeChain_;   /* if HAS_SCOPECHAIN, current scope chain */
    StackFrame          *prev_;         /* if HAS_PREVPC, previous cx->regs->fp */
    void                *ncode_;        /* for a jit frame, return address for method JIT */
    Value               rval_;          /* if HAS_RVAL, return value of the frame */
    StaticBlockObject   *blockChain_;   /* if HAS_BLOCKCHAIN, innermost let block */
    ArgumentsObject     *argsObj_;      /* if HAS_ARGS_OBJ, the call's arguments object */
    jsbytecode          *prevpc_;       /* if HAS_PREVPC, pc of previous frame*/
    InlinedSite         *prevInline_;   /* for a jit frame, inlined site in previous frame */
    void                *hookData_;     /* if HAS_HOOK_DATA, closure returned by call hook */
    void                *annotation_;   /* if HAS_ANNOTATION, perhaps remove with bug 546848 */
    FrameRejoinState    rejoin_;        /* for a jit frame rejoining the interpreter
                                         * from JIT code, state at rejoin. */

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) % sizeof(Value) == 0);
        JS_STATIC_ASSERT(sizeof(StackFrame) % sizeof(Value) == 0);
    }

    inline void initPrev(JSContext *cx);
    jsbytecode *prevpcSlow(InlinedSite **pinlined);
    void writeBarrierPost();

    /*
     * These utilities provide raw access to the values associated with a
     * StackFrame (see "VM stack layout" comment). The utilities are private
     * since they are not able to assert that only unaliased vars/formals are
     * accessed. Normal code should prefer the StackFrame::unaliased* members
     * (or FrameRegs::stackDepth for the usual "depth is at least" assertions).
     */
    Value *slots() const { return (Value *)(this + 1); }
    Value *base() const { return slots() + script()->nfixed; }
    Value *formals() const { return (Value *)this - fun()->nargs; }
    Value *actuals() const { return formals() - (flags_ & OVERFLOW_ARGS ? 2 + u.nactual : 0); }

    friend class FrameRegs;
    friend class ContextStack;
    friend class StackSpace;
    friend class StackIter;
    friend class CallObject;
    friend class ClonedBlockObject;
    friend class ArgumentsObject;
    friend void ::js_DumpStackFrame(JSContext *, StackFrame *);
    friend void ::js_ReportIsNotFunction(JSContext *, const js::Value *, unsigned);
#ifdef JS_METHODJIT
    friend class mjit::CallCompiler;
    friend class mjit::GetPropCompiler;
    friend struct mjit::ic::GetElementIC;
#endif

    /*
     * Frame initialization, called by ContextStack operations after acquiring
     * the raw memory for the frame:
     */

    /* Used for Invoke, Interpret, trace-jit LeaveTree, and method-jit stubs. */
    void initCallFrame(JSContext *cx, JSFunction &callee,
                       JSScript *script, uint32_t nactual, StackFrame::Flags flags);

    /* Used for getFixupFrame (for FixupArity). */
    void initFixupFrame(StackFrame *prev, StackFrame::Flags flags, void *ncode, unsigned nactual);

    /* Used for eval. */
    void initExecuteFrame(JSScript *script, StackFrame *prev, FrameRegs *regs,
                          const Value &thisv, JSObject &scopeChain, ExecuteType type);

    /* Perhaps one fine day we will remove dummy frames. */
    void initDummyFrame(JSContext *cx, JSObject &chain);

  public:
    /*
     * Frame prologue/epilogue
     *
     * Every stack frame must have 'prologue' called before executing the
     * first op and 'epilogue' called after executing the last op and before
     * popping the frame (whether the exit is exceptional or not).
     *
     * For inline JS calls/returns, it is easy to call the prologue/epilogue
     * exactly once. When calling JS from C++, Invoke/Execute push the stack
     * frame but do *not* call the prologue/epilogue. That means Interpret
     * must call the prologue/epilogue for the entry frame. This scheme
     * simplifies jit compilation.
     *
     * An important corner case is what happens when an error occurs (OOM,
     * over-recursed) after pushing the stack frame but before 'prologue' is
     * called or completes fully. To simplify usage, 'epilogue' does not assume
     * 'prologue' has completed and handles all the intermediate state details.
     *
     * The 'newType' option indicates whether the constructed 'this' value (if
     * there is one) should be given a new singleton type.
     */

    bool prologue(JSContext *cx, bool newType);
    void epilogue(JSContext *cx);

    /* Subsets of 'prologue' called from jit code. */
    inline bool jitHeavyweightFunctionPrologue(JSContext *cx);
    inline void jitTypeNestingPrologue(JSContext *cx);
    bool jitStrictEvalPrologue(JSContext *cx);

    /* Initialize local variables of newly-pushed frame. */
    void initVarsToUndefined();

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

    bool isDirectEvalFrame() const {
        return isEvalFrame() && script()->staticLevel > 0;
    }

    bool isNonStrictDirectEvalFrame() const {
        return isNonStrictEvalFrame() && isDirectEvalFrame();
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
     * (Unaliased) locals and arguments
     *
     * Only non-eval function frames have arguments. The arguments pushed by
     * the caller are the 'actual' arguments. The declared arguments of the
     * callee are the 'formal' arguments. When the caller passes less or equal
     * actual arguments, the actual and formal arguments are the same array
     * (but with different extents). When the caller passes too many arguments,
     * the formal subset of the actual arguments is copied onto the top of the
     * stack. This allows the engine to maintain a jit-time constant offset of
     * arguments from the frame pointer. Since the formal subset of the actual
     * arguments is potentially on the stack twice, it is important for all
     * reads/writes to refer to the same canonical memory location. This is
     * abstracted by the unaliased{Formal,Actual} methods.
     *
     * When a local/formal variable is "aliased" (accessed by nested closures,
     * dynamic scope operations, or 'arguments), the canonical location for
     * that value is the slot of an activation object (scope or arguments).
     * Currently, all variables are given slots in *both* the stack frame and
     * heap objects, even though, as just described, only one should ever be
     * accessed. Thus, it is up to the code performing an access to access the
     * correct value. These functions assert that accesses to stack values are
     * unaliased. For more about canonical values locations.
     */

    inline Value &unaliasedVar(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);
    inline Value &unaliasedLocal(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);

    bool hasArgs() const { return isNonEvalFunctionFrame(); }
    inline Value &unaliasedFormal(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);
    inline Value &unaliasedActual(unsigned i, MaybeCheckAliasing = CHECK_ALIASING);
    template <class Op> inline void forEachUnaliasedActual(Op op);

    inline unsigned numFormalArgs() const;
    inline unsigned numActualArgs() const;

    /*
     * Arguments object
     *
     * If a non-eval function has script->needsArgsObj, an arguments object is
     * created in the prologue and stored in the local variable for the
     * 'arguments' binding (script->argumentsLocal). Since this local is
     * mutable, the arguments object can be overwritten and we can "lose" the
     * arguments object. Thus, StackFrame keeps an explicit argsObj_ field so
     * that the original arguments object is always available.
     */

    ArgumentsObject &argsObj() const;
    void initArgsObj(ArgumentsObject &argsobj);

    inline JSObject *createRestParameter(JSContext *cx);

    /*
     * Scope chain
     *
     * In theory, the scope chain would contain an object for every lexical
     * scope. However, only objects that are required for dynamic lookup are
     * actually created.
     *
     * Given that a (non-dummy) StackFrame corresponds roughly to a ES5
     * Execution Context (ES5 10.3), StackFrame::varObj corresponds to the
     * VariableEnvironment component of a Exection Context. Intuitively, the
     * variables object is where new bindings (variables and functions) are
     * stored. One might expect that this is either the Call object or
     * scopeChain.globalObj for function or global code, respectively, however
     * the JSAPI allows calls of Execute to specify a variables object on the
     * scope chain other than the call/global object. This allows embeddings to
     * run multiple scripts under the same global, each time using a new
     * variables object to collect and discard the script's global variables.
     */

    inline HandleObject scopeChain() const;

    inline ScopeObject &aliasedVarScope(ScopeCoordinate sc) const;
    inline GlobalObject &global() const;
    inline CallObject &callObj() const;
    inline JSObject &varObj();

    inline void pushOnScopeChain(ScopeObject &scope);
    inline void popOffScopeChain();

    /*
     * Block chain
     *
     * Entering/leaving a let (or exception) block may do 1 or 2 things: First,
     * a static block object (created at compiled time and stored in the
     * script) is pushed on StackFrame::blockChain. Second, if the static block
     * may be cloned to hold the dynamic values if this is needed for dynamic
     * scope access. A clone is created for a static block iff
     * StaticBlockObject::needsClone.
     */

    bool hasBlockChain() const {
        return (flags_ & HAS_BLOCKCHAIN) && blockChain_;
    }

    StaticBlockObject *maybeBlockChain() {
        return (flags_ & HAS_BLOCKCHAIN) ? blockChain_ : NULL;
    }

    StaticBlockObject &blockChain() const {
        JS_ASSERT(hasBlockChain());
        return *blockChain_;
    }

    bool pushBlock(JSContext *cx, StaticBlockObject &block);
    void popBlock(JSContext *cx);

    /*
     * With 
     *
     * Entering/leaving a with (or E4X filter) block pushes/pops an object 
     * on the scope chain. Pushing uses pushOnScopeChain, popping should use
     * popWith.
     */

    void popWith(JSContext *cx);

    /*
     * Script
     *
     * All function and global frames have an associated JSScript which holds
     * the bytecode being executed for the frame. This script/bytecode does
     * not reflect any inlining that has been performed by the method JIT.
     * If other frames were inlined into this one, the script/pc reflect the
     * point of the outermost call. Inlined frame invariants:
     *
     * - Inlined frames have the same scope chain as the outer frame.
     * - Inlined frames have the same strictness as the outer frame.
     * - Inlined frames can only make calls to other JIT frames associated with
     *   the same VMFrame. Other calls force expansion of the inlined frames.
     */

    JSScript *script() const {
        JS_ASSERT(isScriptFrame());
        return isFunctionFrame()
               ? isEvalFrame() ? u.evalScript : fun()->script()
               : exec.script;
    }

    JSScript *maybeScript() const {
        return isScriptFrame() ? script() : NULL;
    }

    /*
     * Get the frame's current bytecode, assuming 'this' is in 'stack'. Beware,
     * as the name implies, pcQuadratic can lead to quadratic behavior in loops
     * such as:
     *
     *   for ( ...; fp; fp = fp->prev())
     *     ... fp->pcQuadratic(cx->stack);
     *
     * This can be avoided in three ways:
     *  - use ScriptFrameIter, it has O(1) iteration
     *  - if you know the next frame (i.e., next s.t. next->prev == fp
     *  - pcQuadratic will only iterate maxDepth frames (before giving up and
     *    returning fp->script->code), making it O(1), but incorrect.
     */

    jsbytecode *pcQuadratic(const ContextStack &stack, size_t maxDepth = SIZE_MAX);

    /* Return the previous frame's pc. Unlike pcQuadratic, this is O(1). */
    jsbytecode *prevpc(InlinedSite **pinlined = NULL) {
        if (flags_ & HAS_PREVPC) {
            if (pinlined)
                *pinlined = prevInline_;
            return prevpc_;
        }
        return prevpcSlow(pinlined);
    }

    InlinedSite *prevInline() {
        JS_ASSERT(flags_ & HAS_PREVPC);
        return prevInline_;
    }

    /*
     * Function
     *
     * All function frames have an associated interpreted JSFunction. The
     * function returned by fun() and maybeFun() is not necessarily the
     * original canonical function which the frame's script was compiled
     * against. To get this function, use maybeScriptFunction().
     */

    JSFunction* fun() const {
        JS_ASSERT(isFunctionFrame());
        return exec.fun;
    }

    JSFunction* maybeFun() const {
        return isFunctionFrame() ? fun() : NULL;
    }

    JSFunction* maybeScriptFunction() const {
        if (!isFunctionFrame())
            return NULL;
        const StackFrame *fp = this;
        while (fp->isEvalFrame())
            fp = fp->prev();
        return fp->script()->function();
    }

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
     * a frame is a function, 'functionThis' allows more efficient access.
     */

    Value &functionThis() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((Value *)this)[-1];
        return formals()[-1];
    }

    JSObject &constructorThis() const {
        JS_ASSERT(hasArgs());
        return formals()[-1].toObject();
    }

    Value &thisValue() const {
        if (flags_ & (EVAL | GLOBAL))
            return ((Value *)this)[-1];
        return formals()[-1];
    }

    /*
     * Callee
     *
     * Only function frames have a callee. An eval frame in a function has the
     * same callee as its containing function frame. maybeCalleev can be used
     * to return a value that is either the callee object (for function frames) or
     * null (for global frames).
     */

    JSFunction &callee() const {
        JS_ASSERT(isFunctionFrame());
        return *calleev().toObject().toFunction();
    }

    const Value &calleev() const {
        JS_ASSERT(isFunctionFrame());
        return mutableCalleev();
    }

    const Value &maybeCalleev() const {
        JS_ASSERT(isScriptFrame());
        Value &calleev = flags_ & (EVAL | GLOBAL)
                         ? ((Value *)this)[-2]
                         : formals()[-2];
        JS_ASSERT(calleev.isObjectOrNull());
        return calleev;
    }

    Value &mutableCalleev() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((Value *)this)[-2];
        return formals()[-2];
    }

    CallReceiver callReceiver() const {
        return CallReceiverFromArgv(formals());
    }

    /*
     * Frame compartment
     *
     * A stack frame's compartment is the frame's containing context's
     * compartment when the frame was pushed.
     */

    inline JSCompartment *compartment() const;

    /* Annotation (will be removed after bug 546848) */

    void* annotation() const {
        return (flags_ & HAS_ANNOTATION) ? annotation_ : NULL;
    }

    void setAnnotation(void *annot) {
        flags_ |= HAS_ANNOTATION;
        annotation_ = annot;
    }

    /* JIT rejoin state */

    FrameRejoinState rejoin() const {
        return rejoin_;
    }

    void setRejoin(FrameRejoinState state) {
        rejoin_ = state;
    }

    /* Down frame expansion state */

    void setDownFramesExpanded() {
        flags_ |= DOWN_FRAMES_EXPANDED;
    }

    bool downFramesExpanded() {
        return !!(flags_ & DOWN_FRAMES_EXPANDED);
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

    bool hasReturnValue() const {
        return !!(flags_ & HAS_RVAL);
    }

    Value &returnValue() {
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
     * A "generator" frame is a function frame associated with a generator.
     * Since generators are not executed LIFO, the VM copies a single abstract
     * generator frame back and forth between the LIFO VM stack (when the
     * generator is active) and a snapshot stored in JSGenerator (when the
     * generator is inactive). A generator frame is comprised of a StackFrame
     * structure and the values that make up the arguments, locals, and
     * expression stack. The layout in the JSGenerator snapshot matches the
     * layout on the stack (see the "VM stack layout" comment above).
     */

    bool isGeneratorFrame() const {
        bool ret = flags_ & GENERATOR;
        JS_ASSERT_IF(ret, isNonEvalFunctionFrame());
        return ret;
    }

    void initGeneratorFrame() const {
        JS_ASSERT(!isGeneratorFrame());
        JS_ASSERT(isNonEvalFunctionFrame());
        flags_ |= GENERATOR;
    }

    Value *generatorArgsSnapshotBegin() const {
        JS_ASSERT(isGeneratorFrame());
        return actuals() - 2;
    }

    Value *generatorArgsSnapshotEnd() const {
        JS_ASSERT(isGeneratorFrame());
        return (Value *)this;
    }

    Value *generatorSlotsSnapshotBegin() const {
        JS_ASSERT(isGeneratorFrame());
        return (Value *)(this + 1);
    }

    enum TriggerPostBarriers {
        DoPostBarrier = true,
        NoPostBarrier = false
    };
    template <TriggerPostBarriers doPostBarrier>
    void copyFrameAndValues(JSContext *cx, Value *vp, StackFrame *otherfp,
                            const Value *othervp, Value *othersp);

    JSGenerator *maybeSuspendedGenerator(JSRuntime *rt);

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

    InitialFrameFlags initialFlags() const {
        JS_STATIC_ASSERT((int)INITIAL_NONE == 0);
        JS_STATIC_ASSERT((int)INITIAL_CONSTRUCT == (int)CONSTRUCTING);
        JS_STATIC_ASSERT((int)INITIAL_LOWERED == (int)LOWERED_CALL_APPLY);
        uint32_t mask = CONSTRUCTING | LOWERED_CALL_APPLY;
        JS_ASSERT((flags_ & mask) != mask);
        return InitialFrameFlags(flags_ & mask);
    }

    bool isConstructing() const {
        return !!(flags_ & CONSTRUCTING);
    }

    /*
     * These two queries should not be used in general: the presence/absence of
     * the call/args object is determined by the static(ish) properties of the
     * JSFunction/JSScript. These queries should only be performed when probing
     * a stack frame that may be in the middle of the prologue (during which
     * time the call/args object are created).
     */

    bool hasCallObj() const {
        JS_ASSERT(isStrictEvalFrame() || fun()->isHeavyweight());
        return flags_ & HAS_CALL_OBJ;
    }

    bool hasArgsObj() const {
        JS_ASSERT(script()->needsArgsObj());
        return flags_ & HAS_ARGS_OBJ;
    }

    /*
     * The method JIT call/apply optimization can erase Function.{call,apply}
     * invocations from the stack and push the callee frame directly. The base
     * of these frames will be offset by one value, however, which the
     * interpreter needs to account for if it ends up popping the frame.
     */
    bool loweredCallOrApply() const {
        return !!(flags_ & LOWERED_CALL_APPLY);
    }

    bool isDebuggerFrame() const {
        return !!(flags_ & DEBUGGER);
    }

    bool prevUpToDate() const {
        return !!(flags_ & PREV_UP_TO_DATE);
    }

    void setPrevUpToDate() {
        flags_ |= PREV_UP_TO_DATE;
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

  public:
    /* Public, but only for JIT use: */

    inline void resetInlinePrev(StackFrame *prevfp, jsbytecode *prevpc);
    inline void initInlineFrame(JSFunction *fun, StackFrame *prevfp, jsbytecode *prevpc);

    static size_t offsetOfFlags() {
        return offsetof(StackFrame, flags_);
    }

    static size_t offsetOfExec() {
        return offsetof(StackFrame, exec);
    }

    static size_t offsetOfNumActual() {
        return offsetof(StackFrame, u.nactual);
    }

    static size_t offsetOfScopeChain() {
        return offsetof(StackFrame, scopeChain_);
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

    static ptrdiff_t offsetOfArgsObj() {
        return offsetof(StackFrame, argsObj_);
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

    static ptrdiff_t offsetOfFormalArg(JSFunction *fun, unsigned i) {
        JS_ASSERT(i < fun->nargs);
        return (-(int)fun->nargs + i) * sizeof(Value);
    }

    static size_t offsetOfFixed(unsigned i) {
        return sizeof(StackFrame) + i * sizeof(Value);
    }

#ifdef JS_METHODJIT
    inline mjit::JITScript *jit();
#endif

    void methodjitStaticAsserts();

  public:
    void mark(JSTracer *trc);
};

static const size_t VALUES_PER_STACK_FRAME = sizeof(StackFrame) / sizeof(Value);

static inline unsigned
ToReportFlags(InitialFrameFlags initial)
{
    return unsigned(initial & StackFrame::CONSTRUCTING);
}

static inline StackFrame::Flags
ToFrameFlags(InitialFrameFlags initial)
{
    return StackFrame::Flags(initial);
}

static inline InitialFrameFlags
InitialFrameFlagsFromConstructing(bool b)
{
    return b ? INITIAL_CONSTRUCT : INITIAL_NONE;
}

static inline bool
InitialFrameFlagsAreConstructing(InitialFrameFlags initial)
{
    return !!(initial & INITIAL_CONSTRUCT);
}

static inline bool
InitialFrameFlagsAreLowered(InitialFrameFlags initial)
{
    return !!(initial & INITIAL_LOWERED);
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
    InlinedSite *inlined_;
    StackFrame *fp_;
  public:
    StackFrame *fp() const { return fp_; }
    InlinedSite *inlined() const { return inlined_; }

    /* For jit use (need constant): */
    static const size_t offsetOfFp = 3 * sizeof(void *);
    static const size_t offsetOfInlined = 2 * sizeof(void *);
    static void staticAssert() {
        JS_STATIC_ASSERT(offsetOfFp == offsetof(FrameRegs, fp_));
        JS_STATIC_ASSERT(offsetOfInlined == offsetof(FrameRegs, inlined_));
    }
    void clearInlined() { inlined_ = NULL; }

    unsigned stackDepth() const {
        JS_ASSERT(sp >= fp_->base());
        return sp - fp_->base();
    }

    Value *spForStackDepth(unsigned depth) const {
        JS_ASSERT(fp_->script()->nfixed + depth <= fp_->script()->nslots);
        return fp_->base() + depth;
    }

    /* For generator: */
    void rebaseFromTo(const FrameRegs &from, StackFrame &to) {
        fp_ = &to;
        sp = to.slots() + (from.sp - from.fp_->slots());
        pc = from.pc;
        inlined_ = from.inlined_;
        JS_ASSERT(fp_);
    }

    /* For ContextStack: */
    void popFrame(Value *newsp) {
        pc = fp_->prevpc(&inlined_);
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

    /* For InternalInterpret: */
    void restorePartialFrame(Value *newfp) {
        fp_ = (StackFrame *) newfp;
    }

    /* For EnterMethodJIT: */
    void refreshFramePointer(StackFrame *fp) {
        fp_ = fp;
    }

    /* For stubs::CompileFunction, ContextStack: */
    void prepareToRun(StackFrame &fp, JSScript *script) {
        pc = script->code;
        sp = fp.slots() + script->nfixed;
        fp_ = &fp;
        inlined_ = NULL;
    }

    void setToEndOfScript() {
        JSScript *script = fp()->script();
        sp = fp()->base();
        pc = script->code + script->length - JSOP_STOP_LENGTH;
        JS_ASSERT(*pc == JSOP_STOP);
    }

    /* For pushDummyFrame: */
    void initDummyFrame(StackFrame &fp) {
        pc = NULL;
        sp = fp.slots();
        fp_ = &fp;
        inlined_ = NULL;
    }

    /* For expandInlineFrames: */
    void expandInline(StackFrame *innerfp, jsbytecode *innerpc) {
        pc = innerpc;
        fp_ = innerfp;
        inlined_ = NULL;
    }

#ifdef JS_METHODJIT
    /* For LimitCheck: */
    void updateForNcode(mjit::JITScript *jit, void *ncode) {
        pc = mjit::NativeToPC(jit, ncode, &inlined_);
    }
#endif
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

    jsbytecode *maybepc() const {
        return regs_ ? regs_->pc : NULL;
    }

    CallArgsList &calls() const {
        JS_ASSERT(calls_);
        return *calls_;
    }

    CallArgsList *maybeCalls() const {
        return calls_;
    }

    Value *callArgv() const {
        return calls_->array();
    }

    Value *maybeCallArgv() const {
        return calls_ ? calls_->array() : NULL;
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

    StackFrame *computeNextFrame(const StackFrame *fp, size_t maxDepth) const;

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

    bool containsFast(StackFrame *fp) {
        return (Value *)fp >= base_ && (Value *)fp <= trustedEnd_;
    }

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
    static const unsigned ARGS_LENGTH_MAX = CAPACITY_VALS - (2 * BUFFER_VALS);

    /* See stack layout comment in Stack.h. */
    inline Value *firstUnused() const { return seg_ ? seg_->end() : base_; }

    StackSegment &containingSegment(const StackFrame *target) const;

    /*
     * Extra space to reserve on the stack for method JIT frames, beyond the
     * frame's nslots. This may be used for inlined stack frames, slots storing
     * loop invariant code, or to reserve space for pushed callee frames. Note
     * that this space should be reserved when pushing interpreter frames as
     * well, so that we don't need to check the stack when entering the method
     * JIT at loop heads or safe points.
     */
    static const size_t STACK_JIT_EXTRA = (/*~VALUES_PER_STACK_FRAME*/ 8 + 18) * 10;

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
    bool tryBumpLimit(JSContext *cx, Value *from, unsigned nvals, Value **limit);

    /* Called during GC: mark segments, frames, and slots under firstUnused. */
    void mark(JSTracer *trc);
    void markFrameValues(JSTracer *trc, StackFrame *fp, Value *slotsEnd, jsbytecode *pc);

    /* Called during GC: sets active flag on compartments with active frames. */
    void markActiveCompartments();

    /* We only report the committed size;  uncommitted size is uninteresting. */
    JS_FRIEND_API(size_t) sizeOfCommitted();

#ifdef DEBUG
    bool containsSlow(StackFrame *fp);
#endif
};

/*****************************************************************************/

class ContextStack
{
    StackSegment *seg_;
    StackSpace *const space_;
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
    Value *ensureOnTop(JSContext *cx, MaybeReportError report, unsigned nvars,
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
     * executing. Note that JS_SaveFrameChain does not factor into this definition.
     */
    bool empty() const                { return !seg_; }

    /*
     * Return whether there has been at least one frame pushed since the most
     * recent call to JS_SaveFrameChain. Note that natives do not have frames
     * and dummy frames are frames that do not represent script execution hence
     * this query has little semantic meaning past "you can call fp()".
     */
    inline bool hasfp() const { return seg_ && seg_->maybeRegs(); }

    /*
     * Return the most recent script activation's registers with the same
     * caveat as hasfp regarding JS_SaveFrameChain.
     */
    inline FrameRegs *maybeRegs() const { return seg_ ? seg_->maybeRegs() : NULL; }
    inline StackFrame *maybefp() const { return seg_ ? seg_->maybefp() : NULL; }

    /* Faster alternatives to maybe* functions. */
    inline FrameRegs &regs() const { JS_ASSERT(hasfp()); return seg_->regs(); }
    inline StackFrame *fp() const { JS_ASSERT(hasfp()); return seg_->fp(); }

    /* The StackSpace currently hosting this ContextStack. */
    StackSpace &space() const { return *space_; }

    /* Return whether the given frame is in this context's stack. */
    bool containsSlow(const StackFrame *target) const;

    /*** Stack manipulation ***/

    /*
     * pushInvokeArgs allocates |argc + 2| rooted values that will be passed as
     * the arguments to Invoke. A single allocation can be used for multiple
     * Invoke calls. The InvokeArgumentsGuard passed to Invoke must come from
     * an immediately-enclosing (stack-wise) call to pushInvokeArgs.
     */
    bool pushInvokeArgs(JSContext *cx, unsigned argc, InvokeArgsGuard *ag);

    /* Called by Invoke for a scripted function call. */
    bool pushInvokeFrame(JSContext *cx, const CallArgs &args,
                         InitialFrameFlags initial, InvokeFrameGuard *ifg);

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
                         JSFunction &callee, JSScript *script,
                         InitialFrameFlags initial);
    bool pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                         JSFunction &callee, JSScript *script,
                         InitialFrameFlags initial, Value **stackLimit);
    void popInlineFrame(FrameRegs &regs);

    /* Pop a partially-pushed frame after hitting the limit before throwing. */
    void popFrameAfterOverflow();

    /* Get the topmost script and optional pc on the stack. */
    inline JSScript *currentScript(jsbytecode **pc = NULL) const;

    /* Get the scope chain for the topmost scripted call on the stack. */
    inline HandleObject currentScriptedScopeChain() const;

    /*
     * Called by the methodjit for an arity mismatch. Arity mismatch can be
     * hot, so getFixupFrame avoids doing call setup performed by jit code when
     * FixupArity returns.
     */
    StackFrame *getFixupFrame(JSContext *cx, MaybeReportError report,
                              const CallArgs &args, JSFunction *fun, JSScript *script,
                              void *ncode, InitialFrameFlags initial, Value **stackLimit);

    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * As an optimization, the interpreter/mjit can operate on a local
     * FrameRegs instance repoint the ContextStack to this local instance.
     */
    inline void repointRegs(FrameRegs *regs) { JS_ASSERT(hasfp()); seg_->repointRegs(regs); }

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
 * Iterate through the callstack (following fp->prev) of the given context.
 * Each element of said callstack can either be the execution of a script
 * (scripted function call, global code, eval code, debugger code) or the
 * invocation of a (C++) native. Example usage:
 *
 *   for (Stackiter i(cx); !i.done(); ++i) {
 *     if (i.isScript()) {
 *       ... i.fp() ... i.sp() ... i.pc()
 *     } else {
 *       JS_ASSERT(i.isNativeCall());
 *       ... i.args();
 *     }
 *   }
 *
 * The SavedOption parameter additionally lets the iterator continue through
 * breaks in the callstack (from JS_SaveFrameChain). The default is to stop.
 */
class StackIter
{
    friend class ContextStack;
    JSContext    *maybecx_;
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
    JSScript     *script_;
    CallArgs     args_;

    void poisonRegs();
    void popFrame();
    void popCall();
    void settleOnNewSegment();
    void settleOnNewState();
    void startOnSegment(StackSegment *seg);

  public:
    StackIter(JSContext *cx, SavedOption = STOP_AT_SAVED);
    StackIter(JSRuntime *rt, StackSegment &seg);

    bool done() const { return state_ == DONE; }
    StackIter &operator++();

    bool operator==(const StackIter &rhs) const;
    bool operator!=(const StackIter &rhs) const { return !(*this == rhs); }

    bool isScript() const { JS_ASSERT(!done()); return state_ == SCRIPTED; }
    bool isImplicitNativeCall() const {
        JS_ASSERT(!done());
        return state_ == IMPLICIT_NATIVE;
    }
    bool isNativeCall() const {
        JS_ASSERT(!done());
        return state_ == NATIVE || state_ == IMPLICIT_NATIVE;
    }

    bool isFunctionFrame() const;
    bool isEvalFrame() const;
    bool isNonEvalFunctionFrame() const;
    bool isConstructing() const;

    StackFrame *fp() const { JS_ASSERT(isScript()); return fp_; }
    Value      *sp() const { JS_ASSERT(isScript()); return sp_; }
    jsbytecode *pc() const { JS_ASSERT(isScript()); return pc_; }
    JSScript   *script() const { JS_ASSERT(isScript()); return script_; }
    JSFunction *callee() const;
    Value       calleev() const;
    Value       thisv() const;

    CallArgs nativeArgs() const { JS_ASSERT(isNativeCall()); return args_; }
};

/* A filtering of the StackIter to only stop at scripts. */
class ScriptFrameIter : public StackIter
{
    void settle() {
        while (!done() && !isScript())
            StackIter::operator++();
    }

  public:
    ScriptFrameIter(JSContext *cx, StackIter::SavedOption opt = StackIter::STOP_AT_SAVED)
        : StackIter(cx, opt) { settle(); }

    ScriptFrameIter &operator++() { StackIter::operator++(); settle(); return *this; }
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
    void settle();
    StackSegment *seg_;
    StackFrame *fp_;
};

}  /* namespace js */
#endif /* Stack_h__ */
