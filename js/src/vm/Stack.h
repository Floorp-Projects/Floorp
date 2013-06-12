/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Stack_h__
#define Stack_h__

#include "jsfun.h"
#include "jsscript.h"
#include "ion/IonFrameIterator.h"
#include "jsautooplen.h"

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
class GeneratorFrameGuard;

class CallIter;
class ScriptFrameIter;
class AllFramesIter;

class ArgumentsObject;
class ScopeObject;
class StaticBlockObject;

struct ScopeCoordinate;

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
 * var of js::Interpret. Ideally, we'd like to remove all dependence on FrameRegs
 * outside the interpreter.
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
 * perspective of cx1 and cx2. Each independent stack is encapsulated and
 * managed by the js::ContextStack object stored in JSContext. ContextStack
 * is the primary interface to the rest of the engine for pushing and popping
 * the stack.
 */

/*****************************************************************************/

enum MaybeCheckAliasing { CHECK_ALIASING = true, DONT_CHECK_ALIASING = false };

/*****************************************************************************/

#ifdef DEBUG
extern void
CheckLocalUnaliased(MaybeCheckAliasing checkAliasing, JSScript *script,
                    StaticBlockObject *maybeBlock, unsigned i);
#endif

namespace ion {
    class BaselineFrame;
}

/* Pointer to either a StackFrame or a baseline JIT frame. */
class AbstractFramePtr
{
    uintptr_t ptr_;

  public:
    AbstractFramePtr()
      : ptr_(0)
    {}

    AbstractFramePtr(StackFrame *fp)
        : ptr_(fp ? uintptr_t(fp) | 0x1 : 0)
    {
        JS_ASSERT((uintptr_t(fp) & 1) == 0);
    }

    AbstractFramePtr(ion::BaselineFrame *fp)
      : ptr_(uintptr_t(fp))
    {
        JS_ASSERT((uintptr_t(fp) & 1) == 0);
    }

    explicit AbstractFramePtr(JSAbstractFramePtr frame)
        : ptr_(uintptr_t(frame.raw()))
    {
    }

    bool isStackFrame() const {
        return ptr_ & 0x1;
    }
    StackFrame *asStackFrame() const {
        JS_ASSERT(isStackFrame());
        StackFrame *res = (StackFrame *)(ptr_ & ~0x1);
        JS_ASSERT(res);
        return res;
    }
    bool isBaselineFrame() const {
        return ptr_ && !isStackFrame();
    }
    ion::BaselineFrame *asBaselineFrame() const {
        JS_ASSERT(isBaselineFrame());
        ion::BaselineFrame *res = (ion::BaselineFrame *)ptr_;
        JS_ASSERT(res);
        return res;
    }

    void *raw() const { return reinterpret_cast<void *>(ptr_); }

    bool operator ==(const AbstractFramePtr &other) const { return ptr_ == other.ptr_; }
    bool operator !=(const AbstractFramePtr &other) const { return ptr_ != other.ptr_; }

    operator bool() const { return !!ptr_; }

    inline JSGenerator *maybeSuspendedGenerator(JSRuntime *rt) const;

    inline JSObject *scopeChain() const;
    inline CallObject &callObj() const;
    inline bool initFunctionScopeObjects(JSContext *cx);
    inline JSCompartment *compartment() const;

    inline StaticBlockObject *maybeBlockChain() const;
    inline bool hasCallObj() const;
    inline bool isGeneratorFrame() const;
    inline bool isYielding() const;
    inline bool isFunctionFrame() const;
    inline bool isGlobalFrame() const;
    inline bool isEvalFrame() const;
    inline bool isFramePushedByExecute() const;
    inline bool isDebuggerFrame() const;

    inline JSScript *script() const;
    inline JSFunction *fun() const;
    inline JSFunction *maybeFun() const;
    inline JSFunction *callee() const;
    inline Value calleev() const;
    inline Value &thisValue() const;

    inline bool isNonEvalFunctionFrame() const;
    inline bool isNonStrictDirectEvalFrame() const;
    inline bool isStrictEvalFrame() const;

    inline unsigned numActualArgs() const;
    inline unsigned numFormalArgs() const;

    inline Value *argv() const;

    inline bool hasArgsObj() const;
    inline ArgumentsObject &argsObj() const;
    inline void initArgsObj(ArgumentsObject &argsobj) const;
    inline bool useNewType() const;

    inline bool copyRawFrameSlots(AutoValueVector *vec) const;

    inline Value &unaliasedVar(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING);
    inline Value &unaliasedLocal(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING);
    inline Value &unaliasedFormal(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING);
    inline Value &unaliasedActual(unsigned i, MaybeCheckAliasing checkAliasing = CHECK_ALIASING);

    inline bool prevUpToDate() const;
    inline void setPrevUpToDate() const;

    JSObject *evalPrevScopeChain(JSContext *cx) const;

    inline void *maybeHookData() const;
    inline void setHookData(void *data) const;
    inline Value returnValue() const;
    inline void setReturnValue(const Value &rval) const;

    inline bool hasPushedSPSFrame() const;

    inline void popBlock(JSContext *cx) const;
    inline void popWith(JSContext *cx) const;
};

class NullFramePtr : public AbstractFramePtr
{
  public:
    NullFramePtr()
      : AbstractFramePtr()
    { }
};

/*****************************************************************************/

/* Flags specified for a frame as it is constructed. */
enum InitialFrameFlags {
    INITIAL_NONE           =          0,
    INITIAL_CONSTRUCT      =       0x20, /* == StackFrame::CONSTRUCTING, asserted below */
};

enum ExecuteType {
    EXECUTE_GLOBAL         =        0x1, /* == StackFrame::GLOBAL */
    EXECUTE_DIRECT_EVAL    =        0x4, /* == StackFrame::EVAL */
    EXECUTE_INDIRECT_EVAL  =        0x5, /* == StackFrame::GLOBAL | EVAL */
    EXECUTE_DEBUG          =        0xc, /* == StackFrame::EVAL | DEBUGGER */
    EXECUTE_DEBUG_GLOBAL   =        0xd  /* == StackFrame::EVAL | DEBUGGER | GLOBAL */
};

/*****************************************************************************/

class StackFrame
{
  public:
    enum Flags {
        /* Primary frame type */
        GLOBAL             =        0x1,  /* frame pushed for a global script */
        FUNCTION           =        0x2,  /* frame pushed for a scripted call */

        /* Frame subtypes */
        EVAL               =        0x4,  /* frame pushed for eval() or debugger eval */
        DEBUGGER           =        0x8,  /* frame pushed for debugger eval */
        GENERATOR          =       0x10,  /* frame is associated with a generator */
        CONSTRUCTING       =       0x20,  /* frame is for a constructor invocation */

        /* Temporary frame states */
        YIELDING           =       0x40,  /* Interpret dispatched JSOP_YIELD */
        FINISHED_IN_INTERP =       0x80,  /* set if frame finished in Interpret() */

        /* Function prologue state */
        HAS_CALL_OBJ       =      0x100,  /* CallObject created for heavyweight fun */
        HAS_ARGS_OBJ       =      0x200,  /* ArgumentsObject created for needsArgsObj script */

        /* Lazy frame initialization */
        HAS_HOOK_DATA      =      0x400,  /* frame has hookData_ set */
        HAS_RVAL           =      0x800,  /* frame has rval_ set */
        HAS_SCOPECHAIN     =     0x1000,  /* frame has scopeChain_ set */
        HAS_PREVPC         =     0x2000,  /* frame has prevpc_ and prevInline_ set */
        HAS_BLOCKCHAIN     =     0x4000,  /* frame has blockChain_ set */

        /* Debugger state */
        PREV_UP_TO_DATE    =     0x8000,  /* see DebugScopes::updateLiveScopes */

        /* Used in tracking calls and profiling (see vm/SPSProfiler.cpp) */
        HAS_PUSHED_SPS_FRAME =  0x10000,  /* SPS was notified of enty */

        /*
         * If set, we entered one of the JITs and ScriptFrameIter should skip
         * this frame.
         */
        RUNNING_IN_JIT     =    0x20000,

        /* Miscellaneous state. */
        USE_NEW_TYPE       =    0x40000   /* Use new type for constructed |this| object. */
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
    Value               rval_;          /* if HAS_RVAL, return value of the frame */
    StaticBlockObject   *blockChain_;   /* if HAS_BLOCKCHAIN, innermost let block */
    ArgumentsObject     *argsObj_;      /* if HAS_ARGS_OBJ, the call's arguments object */
    jsbytecode          *prevpc_;       /* if HAS_PREVPC, pc of previous frame*/
    void                *hookData_;     /* if HAS_HOOK_DATA, closure returned by call hook */
    AbstractFramePtr    evalInFramePrev_; /* for an eval/debugger frame, the prev frame */

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(StackFrame, rval_) % sizeof(Value) == 0);
        JS_STATIC_ASSERT(sizeof(StackFrame) % sizeof(Value) == 0);
    }

    inline void initPrev(JSContext *cx);
    void writeBarrierPost();

    /*
     * These utilities provide raw access to the values associated with a
     * StackFrame (see "VM stack layout" comment). The utilities are private
     * since they are not able to assert that only unaliased vars/formals are
     * accessed. Normal code should prefer the StackFrame::unaliased* members
     * (or FrameRegs::stackDepth for the usual "depth is at least" assertions).
     */
  public:
    Value *slots() const { return (Value *)(this + 1); }
    Value *base() const { return slots() + script()->nfixed; }
    Value *argv() const { return (Value *)this - Max(numActualArgs(), numFormalArgs()); }

  private:
    friend class FrameRegs;
    friend class ContextStack;
    friend class StackSpace;
    friend class ScriptFrameIter;
    friend class CallObject;
    friend class ClonedBlockObject;
    friend class ArgumentsObject;

    /*
     * Frame initialization, called by ContextStack operations after acquiring
     * the raw memory for the frame:
     */

    /* Used for Invoke, Interpret, trace-jit LeaveTree, and method-jit stubs. */
    void initCallFrame(JSContext *cx, JSFunction &callee,
                       JSScript *script, uint32_t nactual, StackFrame::Flags flags);

    /* Used for eval. */
    void initExecuteFrame(JSScript *script, StackFrame *prevLink, AbstractFramePtr prev,
                          FrameRegs *regs, const Value &thisv, JSObject &scopeChain,
                          ExecuteType type);

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
     */

    bool prologue(JSContext *cx);
    void epilogue(JSContext *cx);

    bool initFunctionScopeObjects(JSContext *cx);

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
     */

    bool isFunctionFrame() const {
        return !!(flags_ & FUNCTION);
    }

    bool isGlobalFrame() const {
        return !!(flags_ & GLOBAL);
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
        return flags_ & EVAL;
    }

    bool isEvalInFunction() const {
        return (flags_ & (EVAL | FUNCTION)) == (EVAL | FUNCTION);
    }

    bool isNonEvalFunctionFrame() const {
        return (flags_ & (FUNCTION | EVAL)) == FUNCTION;
    }

    inline bool isStrictEvalFrame() const {
        return isEvalFrame() && script()->strict;
    }

    bool isNonStrictEvalFrame() const {
        return isEvalFrame() && !script()->strict;
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

    AbstractFramePtr evalInFramePrev() const {
        JS_ASSERT(isEvalFrame());
        return evalInFramePrev_;
    }

    inline void resetGeneratorPrev(JSContext *cx);

    /*
     * (Unaliased) locals and arguments
     *
     * Only non-eval function frames have arguments. The arguments pushed by
     * the caller are the 'actual' arguments. The declared arguments of the
     * callee are the 'formal' arguments. When the caller passes less actual
     * arguments, missing formal arguments are padded with |undefined|.
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

    bool copyRawFrameSlots(AutoValueVector *v);

    unsigned numFormalArgs() const { JS_ASSERT(hasArgs()); return fun()->nargs; }
    unsigned numActualArgs() const { JS_ASSERT(hasArgs()); return u.nactual; }

    inline Value &canonicalActualArg(unsigned i) const;
    template <class Op>
    inline bool forEachCanonicalActualArg(Op op, unsigned start = 0, unsigned count = unsigned(-1));
    template <class Op> inline bool forEachFormalArg(Op op);

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

    JSObject *createRestParameter(JSContext *cx);

    /*
     * Scope chain
     *
     * In theory, the scope chain would contain an object for every lexical
     * scope. However, only objects that are required for dynamic lookup are
     * actually created.
     *
     * Given that a StackFrame corresponds roughly to a ES5 Execution Context
     * (ES5 10.3), StackFrame::varObj corresponds to the VariableEnvironment
     * component of a Exection Context. Intuitively, the variables object is
     * where new bindings (variables and functions) are stored. One might
     * expect that this is either the Call object or scopeChain.globalObj for
     * function or global code, respectively, however the JSAPI allows calls of
     * Execute to specify a variables object on the scope chain other than the
     * call/global object. This allows embeddings to run multiple scripts under
     * the same global, each time using a new variables object to collect and
     * discard the script's global variables.
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
     * Entering/leaving a |with| block pushes/pops an object on the scope chain.
     * Pushing uses pushOnScopeChain, popping should use popWith.
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
        return isFunctionFrame()
               ? isEvalFrame()
                 ? u.evalScript
                 : fun()->nonLazyScript()
               : exec.script;
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
    jsbytecode *prevpc() {
        JS_ASSERT(flags_ & HAS_PREVPC);
        return prevpc_;
    }

    /*
     * Function
     *
     * All function frames have an associated interpreted JSFunction. The
     * function returned by fun() and maybeFun() is not necessarily the
     * original canonical function which the frame's script was compiled
     * against.
     */

    JSFunction* fun() const {
        JS_ASSERT(isFunctionFrame());
        return exec.fun;
    }

    JSFunction* maybeFun() const {
        return isFunctionFrame() ? fun() : NULL;
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
        return argv()[-1];
    }

    JSObject &constructorThis() const {
        JS_ASSERT(hasArgs());
        return argv()[-1].toObject();
    }

    Value &thisValue() const {
        if (flags_ & (EVAL | GLOBAL))
            return ((Value *)this)[-1];
        return argv()[-1];
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
        Value &calleev = flags_ & (EVAL | GLOBAL)
                         ? ((Value *)this)[-2]
                         : argv()[-2];
        JS_ASSERT(calleev.isObjectOrNull());
        return calleev;
    }

    Value &mutableCalleev() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((Value *)this)[-2];
        return argv()[-2];
    }

    CallReceiver callReceiver() const {
        return CallReceiverFromArgv(argv());
    }

    /*
     * Frame compartment
     *
     * A stack frame's compartment is the frame's containing context's
     * compartment when the frame was pushed.
     */

    inline JSCompartment *compartment() const;

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

    bool hasPushedSPSFrame() {
        return !!(flags_ & HAS_PUSHED_SPS_FRAME);
    }

    void setPushedSPSFrame() {
        flags_ |= HAS_PUSHED_SPS_FRAME;
    }

    void unsetPushedSPSFrame() {
        flags_ &= ~HAS_PUSHED_SPS_FRAME;
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
        return argv() - 2;
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
        uint32_t mask = CONSTRUCTING;
        JS_ASSERT((flags_ & mask) != mask);
        return InitialFrameFlags(flags_ & mask);
    }

    void setConstructing() {
        flags_ |= CONSTRUCTING;
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

    bool hasCallObjUnchecked() const {
        return flags_ & HAS_CALL_OBJ;
    }

    bool hasArgsObj() const {
        JS_ASSERT(script()->needsArgsObj());
        return flags_ & HAS_ARGS_OBJ;
    }

    void setUseNewType() {
        JS_ASSERT(isConstructing());
        flags_ |= USE_NEW_TYPE;
    }
    bool useNewType() const {
        JS_ASSERT(isConstructing());
        return flags_ & USE_NEW_TYPE;
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

  public:
    void mark(JSTracer *trc);

    // Entered Baseline/Ion from the interpreter.
    bool runningInJit() const {
        return !!(flags_ & RUNNING_IN_JIT);
    }
    void setRunningInJit() {
        flags_ |= RUNNING_IN_JIT;
    }
    void clearRunningInJit() {
        flags_ &= ~RUNNING_IN_JIT;
    }
};

static const size_t VALUES_PER_STACK_FRAME = sizeof(StackFrame) / sizeof(Value);

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

inline AbstractFramePtr Valueify(JSAbstractFramePtr frame) { return AbstractFramePtr(frame); }
static inline JSAbstractFramePtr Jsvalify(AbstractFramePtr frame)   { return JSAbstractFramePtr(frame.raw()); }

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
        JS_ASSERT(fp_);
    }

    /* For ContextStack: */
    void popFrame(Value *newsp) {
        pc = fp_->prevpc();
        sp = newsp;
        fp_ = fp_->prev();
        JS_ASSERT(fp_);
    }

    /* For stubs::CompileFunction, ContextStack: */
    void prepareToRun(StackFrame &fp, JSScript *script) {
        pc = script->code;
        sp = fp.slots() + script->nfixed;
        fp_ = &fp;
    }

    void setToEndOfScript() {
        JSScript *script = fp()->script();
        sp = fp()->base();
        pc = script->code + script->length - JSOP_STOP_LENGTH;
        JS_ASSERT(*pc == JSOP_STOP);
    }
};

/*****************************************************************************/

class StackSegment
{
    JSContext *cx_;

    /* Previous segment within same context stack. */
    StackSegment *const prevInContext_;

    /* Previous segment sequentially in memory. */
    StackSegment *const prevInMemory_;

    /* Execution registers for most recent script in this segment (or null). */
    FrameRegs *regs_;

    /* End of CallArgs pushed by pushInvokeArgs. */
    Value *invokeArgsEnd_;

#if JS_BITS_PER_WORD == 32
    /*
     * Ensure StackSegment is Value-aligned. Protected to silence Clang warning
     * about unused private fields.
     */
  protected:
    uint32_t padding_;
#endif

  public:
    StackSegment(JSContext *cx,
                 StackSegment *prevInContext,
                 StackSegment *prevInMemory,
                 FrameRegs *regs)
      : cx_(cx),
        prevInContext_(prevInContext),
        prevInMemory_(prevInMemory),
        regs_(regs),
        invokeArgsEnd_(NULL)
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

    JSContext *cx() const {
        return cx_;
    }

    StackSegment *prevInContext() const {
        return prevInContext_;
    }

    StackSegment *prevInMemory() const {
        return prevInMemory_;
    }

    void repointRegs(FrameRegs *regs) {
        regs_ = regs;
    }

    bool isEmpty() const {
        return !regs_;
    }

    bool contains(const StackFrame *fp) const;
    bool contains(const FrameRegs *regs) const;

    StackFrame *computeNextFrame(const StackFrame *fp, size_t maxDepth) const;

    Value *end() const;

    FrameRegs *pushRegs(FrameRegs &regs);
    void popRegs(FrameRegs *regs);

    Value *invokeArgsEnd() const {
        return invokeArgsEnd_;
    }
    void pushInvokeArgsEnd(Value *end, Value **prev) {
        *prev = invokeArgsEnd_;
        invokeArgsEnd_ = end;
    }
    void popInvokeArgsEnd(Value *prev) {
        invokeArgsEnd_ = prev;
    }
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

    inline bool ensureSpace(JSContext *cx, MaybeReportError report,
                            Value *from, ptrdiff_t nvals) const;
    JS_FRIEND_API(bool) ensureSpaceSlow(JSContext *cx, MaybeReportError report,
                                        Value *from, ptrdiff_t nvals) const;

    StackSegment &findContainingSegment(const StackFrame *target) const;

    bool containsFast(StackFrame *fp) {
        return (Value *)fp >= base_ && (Value *)fp <= trustedEnd_;
    }

    void markFrame(JSTracer *trc, StackFrame *fp, Value *slotsEnd);

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

    /* Called during GC: mark segments, frames, and slots under firstUnused. */
    void mark(JSTracer *trc);

    /* Called during GC: sets active flag on compartments with active frames. */
    void markActiveCompartments();

    /*
     * On Windows, report the committed size; on *nix, we report the resident
     * size (which means that if part of the stack is swapped to disk, we say
     * it's shrunk).
     */
    JS_FRIEND_API(size_t) sizeOf();

#ifdef DEBUG
    /* Only used in assertion of debuggers API. */
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
                       MaybeExtend extend, bool *pushedSeg);

    inline StackFrame *
    getCallFrame(JSContext *cx, MaybeReportError report, const CallArgs &args,
                 JSFunction *fun, HandleScript script, StackFrame::Flags *pflags) const;

    /* Make pop* functions private since only called by guard classes. */
    void popSegment();
    friend class InvokeArgsGuard;
    void popInvokeArgs(const InvokeArgsGuard &iag);
    friend class FrameGuard;
    void popFrame(const FrameGuard &fg);
    friend class GeneratorFrameGuard;
    void popGeneratorFrame(const GeneratorFrameGuard &gfg);

    friend class ScriptFrameIter;

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
     * hence this query has little semantic meaning past "you can call fp()".
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

    /*** Stack manipulation ***/

    /*
     * pushInvokeArgs allocates |argc + 2| rooted values that will be passed as
     * the arguments to Invoke. A single allocation can be used for multiple
     * Invoke calls. The InvokeArgsGuard passed to Invoke must come from
     * an immediately-enclosing (stack-wise) call to pushInvokeArgs.
     */
    bool pushInvokeArgs(JSContext *cx, unsigned argc, InvokeArgsGuard *ag,
                        MaybeReportError report = REPORT_ERROR);

    StackFrame *pushInvokeFrame(JSContext *cx, MaybeReportError report,
                                const CallArgs &args, JSFunction *fun,
                                InitialFrameFlags initial, FrameGuard *fg);

    /* Called by Invoke for a scripted function call. */
    bool pushInvokeFrame(JSContext *cx, const CallArgs &args,
                         InitialFrameFlags initial, InvokeFrameGuard *ifg);

    /* Called by Execute for execution of eval or global code. */
    bool pushExecuteFrame(JSContext *cx, HandleScript script, const Value &thisv,
                          HandleObject scopeChain, ExecuteType type,
                          AbstractFramePtr evalInFrame, ExecuteFrameGuard *efg);

    /*
     * Called by SendToGenerator to resume a yielded generator. In addition to
     * pushing a frame onto the VM stack, this function copies over the
     * floating frame stored in 'gen'. When 'gfg' is destroyed, the destructor
     * will copy the frame back to the floating frame.
     */
    bool pushGeneratorFrame(JSContext *cx, JSGenerator *gen, GeneratorFrameGuard *gfg);

    /*
     * An "inline frame" may only be pushed from within the top, active
     * segment. This is the case for calls made inside mjit code and Interpret.
     * The 'stackLimit' overload updates 'stackLimit' if it changes.
     */
    bool pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                         HandleFunction callee, HandleScript script,
                         InitialFrameFlags initial,
                         MaybeReportError report = REPORT_ERROR);
    bool pushInlineFrame(JSContext *cx, FrameRegs &regs, const CallArgs &args,
                         HandleFunction callee, HandleScript script,
                         InitialFrameFlags initial, Value **stackLimit);
    void popInlineFrame(FrameRegs &regs);

    /*
     * Get the topmost script and optional pc on the stack. By default, this
     * function only returns a JSScript in the current compartment, returning
     * NULL if the current script is in a different compartment. This behavior
     * can be overridden by passing ALLOW_CROSS_COMPARTMENT.
     */
    enum MaybeAllowCrossCompartment {
        DONT_ALLOW_CROSS_COMPARTMENT = false,
        ALLOW_CROSS_COMPARTMENT = true
    };
    inline JSScript *currentScript(jsbytecode **pc = NULL,
                                   MaybeAllowCrossCompartment = DONT_ALLOW_CROSS_COMPARTMENT) const;

    /* Get the scope chain for the topmost scripted call on the stack. */
    inline HandleObject currentScriptedScopeChain() const;

    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * As an optimization, the interpreter/mjit can operate on a local
     * FrameRegs instance repoint the ContextStack to this local instance.
     */
    inline void repointRegs(FrameRegs *regs) { JS_ASSERT(hasfp()); seg_->repointRegs(regs); }
};

/*****************************************************************************/

class InvokeArgsGuard : public JS::CallArgs
{
    friend class ContextStack;
    ContextStack *stack_;
    Value *prevInvokeArgsEnd_;
    bool pushedSeg_;
    void setPushed(ContextStack &stack) { JS_ASSERT(!pushed()); stack_ = &stack; }
  public:
    InvokeArgsGuard() : CallArgs(), stack_(NULL), prevInvokeArgsEnd_(NULL), pushedSeg_(false) {}
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

template <>
struct DefaultHasher<AbstractFramePtr> {
    typedef AbstractFramePtr Lookup;

    static js::HashNumber hash(const Lookup &key) {
        return size_t(key.raw());
    }

    static bool match(const AbstractFramePtr &k, const Lookup &l) {
        return k == l;
    }
};

/*****************************************************************************/

class InterpreterActivation;

namespace ion {
    class JitActivation;
};

// A JSRuntime's stack consists of a linked list of activations. Every activation
// contains a number of scripted frames that are either running in the interpreter
// (InterpreterActivation) or JIT code (JitActivation). The frames inside a single
// activation are contiguous: whenever C++ calls back into JS, a new activation is
// pushed.
//
// Every activation is tied to a single JSContext and JSCompartment. This means we
// can construct a given context's stack by skipping activations belonging to other
// contexts.
class Activation
{
  protected:
    JSContext *cx_;
    JSCompartment *compartment_;
    Activation *prev_;
    bool active_;

    // Counter incremented by JS_SaveFrameChain on the top-most activation and
    // decremented by JS_RestoreFrameChain. If > 0, ScriptFrameIter should stop
    // iterating when it reaches this activation (if GO_THROUGH_SAVED is not
    // set).
    size_t savedFrameChain_;

    enum Kind { Interpreter, Jit };
    Kind kind_;

    inline Activation(JSContext *cx, Kind kind_, bool active = true);
    inline ~Activation();

  public:
    JSContext *cx() const {
        return cx_;
    }
    JSCompartment *compartment() const {
        return compartment_;
    }
    Activation *prev() const {
        return prev_;
    }
    bool isActive() const {
        return active_;
    }
    void setActive(bool active = true) {
        active_ = active;
    }

    bool isInterpreter() const {
        return kind_ == Interpreter;
    }
    bool isJit() const {
        return kind_ == Jit;
    }

    InterpreterActivation *asInterpreter() const {
        JS_ASSERT(isInterpreter());
        return (InterpreterActivation *)this;
    }
    ion::JitActivation *asJit() const {
        JS_ASSERT(isJit());
        return (ion::JitActivation *)this;
    }

    void saveFrameChain() {
        savedFrameChain_++;
    }
    void restoreFrameChain() {
        JS_ASSERT(savedFrameChain_ > 0);
        savedFrameChain_--;
    }
    bool hasSavedFrameChain() const {
        return savedFrameChain_ > 0;
    }

  private:
    Activation(const Activation &other) MOZ_DELETE;
    void operator=(const Activation &other) MOZ_DELETE;
};

class InterpreterFrameIterator;

class InterpreterActivation : public Activation
{
    friend class js::InterpreterFrameIterator;

    StackFrame *const entry_; // Entry frame for this activation.
    StackFrame *current_;     // The most recent frame.
    FrameRegs &regs_;

  public:
    inline InterpreterActivation(JSContext *cx, StackFrame *entry, FrameRegs &regs);
    inline ~InterpreterActivation();

    void pushFrame(StackFrame *frame) {
        JS_ASSERT(frame->script()->compartment() == compartment_);
        current_ = frame;
    }
    void popFrame(StackFrame *frame) {
        JS_ASSERT(current_ == frame);
        JS_ASSERT(current_ != entry_);

        current_ = frame->prev();
        JS_ASSERT(current_);
    }
    StackFrame *current() const {
        JS_ASSERT(current_);
        return current_;
    }
};

// Iterates over a runtime's activation list.
class ActivationIterator
{
    uint8_t *jitTop_;

  protected:
    Activation *activation_;

  private:
    void settle();

  public:
    explicit ActivationIterator(JSRuntime *rt);

    ActivationIterator &operator++();

    Activation *activation() const {
        return activation_;
    }
    uint8_t *jitTop() const {
        JS_ASSERT(activation_->isJit());
        return jitTop_;
    }
    bool done() const {
        return activation_ == NULL;
    }
};

namespace ion {

// A JitActivation is used for frames running in Baseline or Ion.
class JitActivation : public Activation
{
    uint8_t *prevIonTop_;
    JSContext *prevIonJSContext_;
    bool firstFrameIsConstructing_;

  public:
    JitActivation(JSContext *cx, bool firstFrameIsConstructing, bool active = true);
    ~JitActivation();

    uint8_t *prevIonTop() const {
        return prevIonTop_;
    }
    JSCompartment *compartment() const {
        return compartment_;
    }
    bool firstFrameIsConstructing() const {
        return firstFrameIsConstructing_;
    }
};

// A filtering of the ActivationIterator to only stop at JitActivations.
class JitActivationIterator : public ActivationIterator
{
    void settle() {
        while (!done() && !activation_->isJit())
            ActivationIterator::operator++();
    }

  public:
    explicit JitActivationIterator(JSRuntime *rt)
      : ActivationIterator(rt)
    {
        settle();
    }

    JitActivationIterator &operator++() {
        ActivationIterator::operator++();
        settle();
        return *this;
    }

    // Returns the bottom and top addresses of the current JitActivation.
    void jitStackRange(uintptr_t *&min, uintptr_t *&end);
};

} // namespace ion

// Iterates over the frames of a single InterpreterActivation.
class InterpreterFrameIterator
{
    InterpreterActivation *activation_;
    StackFrame *fp_;
    jsbytecode *pc_;

  public:
    explicit InterpreterFrameIterator(InterpreterActivation *activation)
      : activation_(activation),
        fp_(NULL),
        pc_(NULL)
    {
        if (activation) {
            fp_ = activation->current();
            pc_ = activation->regs_.pc;
        }
    }

    StackFrame *frame() const {
        JS_ASSERT(!done());
        return fp_;
    }
    jsbytecode *pc() const {
        JS_ASSERT(!done());
        return pc_;
    }

    InterpreterFrameIterator &operator++();

    bool done() const {
        return fp_ == NULL;
    }
};

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
class ScriptFrameIter
{
  public:
    enum SavedOption { STOP_AT_SAVED, GO_THROUGH_SAVED };
    enum ContextOption { CURRENT_CONTEXT, ALL_CONTEXTS };
    enum State { DONE, SCRIPTED, JIT };

    /*
     * Unlike ScriptFrameIter itself, ScriptFrameIter::Data can be allocated on
     * the heap, so this structure should not contain any GC things.
     */
    struct Data
    {
        PerThreadData *perThread_;
        JSContext    *cx_;
        SavedOption  savedOption_;
        ContextOption contextOption_;

        State        state_;

        jsbytecode   *pc_;

        InterpreterFrameIterator interpFrames_;
        ActivationIterator activations_;

#ifdef JS_ION
        ion::IonFrameIterator ionFrames_;
#endif

        Data(JSContext *cx, PerThreadData *perThread, SavedOption savedOption,
             ContextOption contextOption);
        Data(const Data &other);
    };

    friend class ContextStack;
    friend class ::JSBrokenFrameIterator;
  private:
    Data data_;
#ifdef JS_ION
    ion::InlineFrameIterator ionInlineFrames_;
#endif

    void popActivation();
    void popInterpreterFrame();
#ifdef JS_ION
    void nextJitFrame();
    void popJitFrame();
#endif
    void settleOnActivation();

  public:
    ScriptFrameIter(JSContext *cx, SavedOption = STOP_AT_SAVED);
    ScriptFrameIter(JSContext *cx, ContextOption, SavedOption);
    ScriptFrameIter(const ScriptFrameIter &iter);
    ScriptFrameIter(const Data &data);

    bool done() const { return data_.state_ == DONE; }
    ScriptFrameIter &operator++();

    Data *copyData() const;

    JSCompartment *compartment() const;

    JSScript *script() const {
        JS_ASSERT(!done());
        if (data_.state_ == SCRIPTED)
            return interpFrame()->script();
#ifdef JS_ION
        JS_ASSERT(data_.state_ == JIT);
        if (data_.ionFrames_.isOptimizedJS())
            return ionInlineFrames_.script();
        return data_.ionFrames_.script();
#else
        return NULL;
#endif
    }
    bool isJit() const {
        JS_ASSERT(!done());
        return data_.state_ == JIT;
    }

    bool isIon() const {
#ifdef JS_ION
        return isJit() && data_.ionFrames_.isOptimizedJS();
#else
        return false;
#endif
    }

    bool isBaseline() const {
#ifdef JS_ION
        return isJit() && data_.ionFrames_.isBaselineJS();
#else
        return false;
#endif
    }

    bool isFunctionFrame() const;
    bool isGlobalFrame() const;
    bool isEvalFrame() const;
    bool isNonEvalFunctionFrame() const;
    bool isGeneratorFrame() const;
    bool isConstructing() const;

    bool hasArgs() const { return isNonEvalFunctionFrame(); }

    AbstractFramePtr abstractFramePtr() const;

    /*
     * When entering IonMonkey, the top interpreter frame (pushed by the caller)
     * is kept on the stack as bookkeeping (with runningInIon() set). The
     * contents of the frame are ignored by Ion code (and GC) and thus
     * immediately become garbage and must not be touched directly.
     */
    StackFrame *interpFrame() const {
        JS_ASSERT(data_.state_ == SCRIPTED);
        return data_.interpFrames_.frame();
    }

    Activation *activation() const { return data_.activations_.activation(); }

    jsbytecode *pc() const { JS_ASSERT(!done()); return data_.pc_; }
    void        updatePcQuadratic();
    JSFunction *callee() const;
    Value       calleev() const;
    unsigned    numActualArgs() const;
    unsigned    numFormalArgs() const { return script()->function()->nargs; }
    Value       unaliasedActual(unsigned i, MaybeCheckAliasing = CHECK_ALIASING) const;

    JSObject   *scopeChain() const;
    CallObject &callObj() const;

    bool        hasArgsObj() const;
    ArgumentsObject &argsObj() const;

    // Ensure that thisv is correct, see ComputeThis.
    bool        computeThis() const;
    Value       thisv() const;

    Value       returnValue() const;
    void        setReturnValue(const Value &v);

    JSFunction *maybeCallee() const {
        return isFunctionFrame() ? callee() : NULL;
    }

    // These are only valid for the top frame.
    size_t      numFrameSlots() const;
    Value       frameSlotValue(size_t index) const;

    template <class Op>
    inline void ionForEachCanonicalActualArg(JSContext *cx, Op op);
};

/* A filtering of the ScriptFrameIter to only stop at non-self-hosted scripts. */
class NonBuiltinScriptFrameIter : public ScriptFrameIter
{
    void settle() {
        while (!done() && script()->selfHosted)
            ScriptFrameIter::operator++();
    }

  public:
    NonBuiltinScriptFrameIter(JSContext *cx, ScriptFrameIter::SavedOption opt = ScriptFrameIter::STOP_AT_SAVED)
      : ScriptFrameIter(cx, opt) { settle(); }

    NonBuiltinScriptFrameIter(const ScriptFrameIter::Data &data)
      : ScriptFrameIter(data)
    {}

    NonBuiltinScriptFrameIter &operator++() { ScriptFrameIter::operator++(); settle(); return *this; }
};

/*
 * Blindly iterate over all frames in the current thread's stack. These frames
 * can be from different contexts and compartments, so beware.
 */
class AllFramesIter : public ScriptFrameIter
{
  public:
    AllFramesIter(JSContext *cx)
      : ScriptFrameIter(cx, ScriptFrameIter::ALL_CONTEXTS, ScriptFrameIter::GO_THROUGH_SAVED)
    {}
};

}  /* namespace js */
#endif /* Stack_h__ */
