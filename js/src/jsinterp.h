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
#include "jsfun.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsvalue.h"

struct JSFrameRegs
{
    STATIC_SKIP_INFERENCE
    js::Value       *sp;                  /* stack pointer */
    jsbytecode      *pc;                  /* program counter */
    JSStackFrame    *fp;                  /* active frame */
};

/* Flags to toggle js::Interpret() execution. */
enum JSInterpFlags
{
    JSINTERP_RECORD            =     0x1, /* interpreter has been started to record/run traces */
    JSINTERP_SAFEPOINT         =     0x2  /* interpreter should leave on a method JIT safe point */
};

/* Flags used in JSStackFrame::flags_ */
enum JSFrameFlags
{
    /* Primary frame type */
    JSFRAME_GLOBAL             =     0x1, /* frame pushed for a global script */
    JSFRAME_FUNCTION           =     0x2, /* frame pushed for a scripted call */
    JSFRAME_DUMMY              =     0x4, /* frame pushed for bookkeeping */

    /* Frame subtypes */
    JSFRAME_EVAL               =     0x8, /* frame pushed by js::Execute */
    JSFRAME_DEBUGGER           =    0x10, /* frame pushed by JS_EvaluateInStackFrame */
    JSFRAME_GENERATOR          =    0x20, /* frame is associated with a generator */
    JSFRAME_FLOATING_GENERATOR =    0x40, /* frame is is in generator obj, not on stack */
    JSFRAME_CONSTRUCTING       =    0x80, /* frame is for a constructor invocation */

    /* Temporary frame states */
    JSFRAME_ASSIGNING          =   0x100, /* not-JOF_ASSIGNING op is assigning */
    JSFRAME_YIELDING           =   0x200, /* js::Interpret dispatched JSOP_YIELD */
    JSFRAME_BAILED_AT_RETURN   =   0x400, /* bailed at JSOP_RETURN */

    /* Concerning function arguments */
    JSFRAME_OVERRIDE_ARGS      =  0x1000, /* overridden arguments local variable */
    JSFRAME_OVERFLOW_ARGS      =  0x2000, /* numActualArgs > numFormalArgs */
    JSFRAME_UNDERFLOW_ARGS     =  0x4000, /* numActualArgs < numFormalArgs */

    /* Lazy frame initialization */
    JSFRAME_HAS_IMACRO_PC      =   0x8000, /* frame has imacpc value available */
    JSFRAME_HAS_CALL_OBJ       =  0x10000, /* frame has a callobj reachable from scopeChain_ */
    JSFRAME_HAS_ARGS_OBJ       =  0x20000, /* frame has an argsobj in JSStackFrame::args */
    JSFRAME_HAS_HOOK_DATA      =  0x40000, /* frame has hookData_ set */
    JSFRAME_HAS_ANNOTATION     =  0x80000, /* frame has annotation_ set */
    JSFRAME_HAS_RVAL           = 0x100000, /* frame has rval_ set */
    JSFRAME_HAS_SCOPECHAIN     = 0x200000, /* frame has scopeChain_ set */
    JSFRAME_HAS_PREVPC         = 0x400000  /* frame has prevpc_ set */
};

/*
 * A stack frame is a part of a stack segment (see js::StackSegment) which is
 * on the per-thread VM stack (see js::StackSpace).
 */
struct JSStackFrame
{
  private:
    mutable uint32      flags_;         /* bits described by JSFrameFlags */
    union {                             /* describes what code is executing in a */
        JSScript        *script;        /*   global frame */
        JSFunction      *fun;           /*   function frame, pre GetScopeChain */
    } exec;
    union {                             /* describes the arguments of a function */
        uintN           nactual;        /*   pre GetArgumentsObject */
        JSObject        *obj;           /*   post GetArgumentsObject */
        JSScript        *script;        /* eval has no args, but needs a script */
    } args;
    mutable JSObject    *scopeChain_;   /* current scope chain */
    JSStackFrame        *prev_;         /* previous cx->regs->fp */
    void                *ncode_;        /* return address for method JIT */

    /* Lazily initialized */
    js::Value           rval_;          /* return value of the frame */
    jsbytecode          *prevpc_;       /* pc of previous frame*/
    jsbytecode          *imacropc_;     /* pc of macro caller */
    void                *hookData_;     /* closure returned by call hook */
    void                *annotation_;   /* perhaps remove with bug 546848 */

#if JS_BITS_PER_WORD == 32
    void                *padding;
#endif

    friend class js::StackSpace;
    friend class js::FrameRegsIter;
    friend struct JSContext;

    inline void initPrev(JSContext *cx);

  public:
    /*
     * Stack frame sort (see JSStackFrame comment above)
     *
     * A stack frame may have one of three types, which determines which
     * members of the frame may be accessed and other invariants:
     *
     *  global frame:   execution of global code or an eval in global code
     *  function frame: execution of function code or an eval in a function
     *  dummy frame:    bookkeeping frame (read: hack)
     *
     * As noted, global and function frames may optionally be 'eval frames', which
     * further restricts the stack frame members which may be used. Namely, the
     * argument-related members of function eval frames are not valid, since an eval
     * shares its containing function's arguments rather than having its own.
     */

    bool isFunctionFrame() const {
        return !!(flags_ & JSFRAME_FUNCTION);
    }

    bool isGlobalFrame() const {
        return !!(flags_ & JSFRAME_GLOBAL);
    }

    bool isDummyFrame() const {
        return !!(flags_ & JSFRAME_DUMMY);
    }

    bool isScriptFrame() const {
        return !!(flags_ & (JSFRAME_FUNCTION | JSFRAME_GLOBAL));
    }

    bool isEvalFrame() const {
        JS_ASSERT_IF(flags_ & JSFRAME_EVAL, isScriptFrame());
        return flags_ & JSFRAME_EVAL;
    }

    /*
     * Frame initialization
     *
     * After acquiring a pointer to an uninitialized stack frame on the VM
     * stack from js::StackSpace, these members are used to initialize the
     * stack frame before officially pushing the frame into the context.
     * Collecting frame initialization into a set of inline helpers allows
     * simpler reasoning and makes call-optimization easier.
     */

    /* Used for Invoke, Interpret, trace-jit LeaveTree, and method-jit stubs. */
    inline void initCallFrame(JSContext *cx, JSObject &callee, JSFunction *fun,
                              uint32 nactual, uint32 flags);

    /* Called by method-jit stubs and serve as a specification for jit-code. */
    inline void initCallFrameCallerHalf(JSContext *cx, uint32 nactual, uint32 flags);
    inline void initCallFrameEarlyPrologue(JSFunction *fun, void *ncode);
    inline void initCallFrameLatePrologue();

    /* Used for eval. */
    inline void initEvalFrame(JSContext *cx, JSScript *script, JSStackFrame *prev,
                              uint32 flags);
    inline void initGlobalFrame(JSScript *script, JSObject &chain, uint32 flags);

    /* Used when activating generators. */
    inline void stealFrameAndSlots(js::Value *vp, JSStackFrame *otherfp,
                                   js::Value *othervp, js::Value *othersp);

    /* Perhaps one fine day we will remove dummy frames. */
    inline void initDummyFrame(JSContext *cx, JSObject &chain);

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

    JSStackFrame *prev() const {
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
    jsbytecode *pc(JSContext *cx, JSStackFrame *next = NULL);

    jsbytecode *prevpc() {
        JS_ASSERT((prev_ != NULL) && (flags_ & JSFRAME_HAS_PREVPC));
        return prevpc_;
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
        return isFunctionFrame() && !isEvalFrame();
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
        return (flags_ & (JSFRAME_FUNCTION | JSFRAME_EVAL)) == JSFRAME_FUNCTION
               ? formalArgs()
               : NULL;
    }

    inline uintN numActualArgs() const;
    inline js::Value *actualArgs() const;
    inline js::Value *actualArgsEnd() const;

    inline js::Value &canonicalActualArg(uintN i) const;
    template <class Op> inline void forEachCanonicalActualArg(Op op);
    template <class Op> inline void forEachFormalArg(Op op);

    /* True if we have created an arguments object for this frame; implies hasArgs(). */
    bool hasArgsObj() const {
        return !!(flags_ & JSFRAME_HAS_ARGS_OBJ);
    }

    JSObject &argsObj() const {
        JS_ASSERT(hasArgsObj());
        JS_ASSERT(!isEvalFrame());
        return *args.obj;
    }

    JSObject *maybeArgsObj() const {
        return hasArgsObj() ? &argsObj() : NULL;
    }

    inline void setArgsObj(JSObject &obj);
    inline void clearArgsObj();

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
        if (flags_ & (JSFRAME_EVAL | JSFRAME_GLOBAL))
            return ((js::Value *)this)[-1];
        return formalArgs()[-1];
    }

    inline JSObject *computeThisObject(JSContext *cx);

    /*
     * Callee
     *
     * Only function frames have a callee. An eval frame in a function has the
     * same caller as its containing function frame.
     */

    js::Value &calleeValue() const {
        JS_ASSERT(isFunctionFrame());
        if (isEvalFrame())
            return ((js::Value *)this)[-2];
        return formalArgs()[-2];
    }

    JSObject &callee() const {
        JS_ASSERT(isFunctionFrame());
        return calleeValue().toObject();
    }

    JSObject *maybeCallee() const {
        return isFunctionFrame() ? &callee() : NULL;
    }

    /*
     * getValidCalleeObject is a fallible getter to compute the correct callee
     * function object, which may require deferred cloning due to the JSObject
     * methodReadBarrier. For a non-function frame, return true with *vp set
     * from calleeValue, which may not be an object (it could be undefined).
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
     */

    JSObject &scopeChain() const {
        JS_ASSERT_IF(!(flags_ & JSFRAME_HAS_SCOPECHAIN), isFunctionFrame());
        if (!(flags_ & JSFRAME_HAS_SCOPECHAIN)) {
            scopeChain_ = callee().getParent();
            flags_ |= JSFRAME_HAS_SCOPECHAIN;
        }
        return *scopeChain_;
    }

    bool hasCallObj() const {
        return !!(flags_ & JSFRAME_HAS_CALL_OBJ);
    }

    inline JSObject &callObj() const;
    inline JSObject *maybeCallObj() const;
    inline void setScopeChainNoCallObj(JSObject &obj);
    inline void setScopeChainAndCallObj(JSObject &obj);
    inline void clearCallObj();

    /*
     * Imacropc
     *
     * A frame's IMacro pc is the bytecode address when an imacro started
     * executing (guaranteed non-null). An imacro does not push a frame, so
     * when the imacro finishes, the frame's IMacro pc becomes the current pc.
     */

    bool hasImacropc() const {
        return flags_ & JSFRAME_HAS_IMACRO_PC;
    }

    jsbytecode *imacropc() const {
        JS_ASSERT(hasImacropc());
        return imacropc_;
    }

    jsbytecode *maybeImacropc() const {
        return hasImacropc() ? imacropc() : NULL;
    }

    void clearImacropc() {
        flags_ &= ~JSFRAME_HAS_IMACRO_PC;
    }

    void setImacropc(jsbytecode *pc) {
        JS_ASSERT(pc);
        JS_ASSERT(!(flags_ & JSFRAME_HAS_IMACRO_PC));
        imacropc_ = pc;
        flags_ |= JSFRAME_HAS_IMACRO_PC;
    }

    /* Annotation (will be removed after bug 546848) */

    void* annotation() const {
        return (flags_ & JSFRAME_HAS_ANNOTATION) ? annotation_ : NULL;
    }

    void setAnnotation(void *annot) {
        flags_ |= JSFRAME_HAS_ANNOTATION;
        annotation_ = annot;
    }

    /* Debugger hook data */

    bool hasHookData() const {
        return !!(flags_ & JSFRAME_HAS_HOOK_DATA);
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
        flags_ |= JSFRAME_HAS_HOOK_DATA;
    }

    /* Return value */

    const js::Value& returnValue() {
        if (!(flags_ & JSFRAME_HAS_RVAL))
            rval_.setUndefined();
        return rval_;
    }

    void markReturnValue() {
        flags_ |= JSFRAME_HAS_RVAL;
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
        return !!(flags_ & JSFRAME_GENERATOR);
    }

    bool isFloatingGenerator() const {
        JS_ASSERT_IF(flags_ & JSFRAME_FLOATING_GENERATOR, isGeneratorFrame());
        return !!(flags_ & JSFRAME_FLOATING_GENERATOR);
    }

    void initFloatingGenerator() {
        JS_ASSERT(!(flags_ & JSFRAME_GENERATOR));
        flags_ |= (JSFRAME_GENERATOR | JSFRAME_FLOATING_GENERATOR);
    }

    void unsetFloatingGenerator() {
        flags_ &= ~JSFRAME_FLOATING_GENERATOR;
    }

    void setFloatingGenerator() {
        flags_ |= JSFRAME_FLOATING_GENERATOR;
    }

    /*
     * Other flags
     */

    bool isConstructing() const {
        return !!(flags_ & JSFRAME_CONSTRUCTING);
    }

    uint32 isConstructingFlag() const {
        JS_ASSERT(isFunctionFrame());
        JS_ASSERT((flags_ & ~(JSFRAME_CONSTRUCTING | JSFRAME_FUNCTION)) == 0);
        return flags_;
    }

    bool isDebuggerFrame() const {
        return !!(flags_ & JSFRAME_DEBUGGER);
    }

    bool isEvalOrDebuggerFrame() const {
        return !!(flags_ & (JSFRAME_EVAL | JSFRAME_DEBUGGER));
    }

    bool hasOverriddenArgs() const {
        return !!(flags_ & JSFRAME_OVERRIDE_ARGS);
    }

    bool hasOverflowArgs() const {
        return !!(flags_ & JSFRAME_OVERFLOW_ARGS);
    }

    void setOverriddenArgs() {
        flags_ |= JSFRAME_OVERRIDE_ARGS;
    }

    bool isAssigning() const {
        return !!(flags_ & JSFRAME_ASSIGNING);
    }

    void setAssigning() {
        flags_ |= JSFRAME_ASSIGNING;
    }

    void clearAssigning() {
        flags_ &= ~JSFRAME_ASSIGNING;
    }

    bool isYielding() {
        return !!(flags_ & JSFRAME_YIELDING);
    }

    void setYielding() {
        flags_ |= JSFRAME_YIELDING;
    }

    void clearYielding() {
        flags_ &= ~JSFRAME_YIELDING;
    }

    bool isBailedAtReturn() const {
        return flags_ & JSFRAME_BAILED_AT_RETURN;
    }

    void setBailedAtReturn() {
        flags_ |= JSFRAME_BAILED_AT_RETURN;
    }

    /*
     * Variables object accessors
     *
     * A stack frame's 'varobj' refers to the 'variables object' (ES3 term)
     * associated with the Execution Context's VariableEnvironment (ES5 10.3).
     *
     * To compute the frame's varobj, the caller must supply the segment
     * containing the frame (see js::StackSegment comment). As an abbreviation,
     * the caller may pass the context if the frame is contained in that
     * context's active segment.
     */

    inline JSObject &varobj(js::StackSegment *seg) const;
    inline JSObject &varobj(JSContext *cx) const;

    /* Access to privates from the jits. */

    static size_t offsetOfFlags() {
        return offsetof(JSStackFrame, flags_);
    }

    static size_t offsetOfExec() {
        return offsetof(JSStackFrame, exec);
    }

    void *addressOfArgs() {
        return &args;
    }

    static size_t offsetOfScopeChain() {
        return offsetof(JSStackFrame, scopeChain_);
    }

    JSObject **addressOfScopeChain() {
        JS_ASSERT(flags_ & JSFRAME_HAS_SCOPECHAIN);
        return &scopeChain_;
    }

    static size_t offsetOfPrev() {
        return offsetof(JSStackFrame, prev_);
    }

    static size_t offsetOfReturnValue() {
        return offsetof(JSStackFrame, rval_);
    }

    static ptrdiff_t offsetOfncode() {
        return offsetof(JSStackFrame, ncode_);
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
        return sizeof(JSStackFrame) + i * sizeof(js::Value);
    }

    /* Workaround for static asserts on private members. */

    void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(JSStackFrame, rval_) % sizeof(js::Value) == 0);
        JS_STATIC_ASSERT(sizeof(JSStackFrame) % sizeof(js::Value) == 0);
    }

    void methodjitStaticAsserts();

#ifdef DEBUG
    /* Poison scopeChain value set before a frame is flushed. */
    static JSObject *const sInvalidScopeChain;
#endif
};

namespace js {

static const size_t VALUES_PER_STACK_FRAME = sizeof(JSStackFrame) / sizeof(Value);

} /* namespace js */


extern JSObject *
js_GetBlockChain(JSContext *cx, JSStackFrame *fp);

extern JSObject *
js_GetBlockChainFast(JSContext *cx, JSStackFrame *fp, JSOp op, size_t oplen);

/*
 * Refresh and return fp->scopeChain.  It may be stale if block scopes are
 * active but not yet reflected by objects in the scope chain.  If a block
 * scope contains a with, eval, XML filtering predicate, or similar such
 * dynamically scoped construct, then compile-time block scope at fp->blocks
 * must reflect at runtime.
 */
extern JSObject *
js_GetScopeChain(JSContext *cx, JSStackFrame *fp);

extern JSObject *
js_GetScopeChainFast(JSContext *cx, JSStackFrame *fp, JSOp op, size_t oplen);

/*
 * Given a context and a vector of [callee, this, args...] for a function that
 * was specified with a JSFUN_THISP_PRIMITIVE flag, get the primitive value of
 * |this| into *thisvp. In doing so, if |this| is an object, insist it is an
 * instance of clasp and extract its private slot value to return via *thisvp.
 *
 * NB: this function loads and uses *vp before storing *thisvp, so the two may
 * alias the same Value.
 */
extern JSBool
js_GetPrimitiveThis(JSContext *cx, js::Value *vp, js::Class *clasp,
                    const js::Value **vpp);

namespace js {

inline void
PutActivationObjects(JSContext *cx, JSStackFrame *fp);

/*
 * For a call with arguments argv including argv[-1] (nominal |this|) and
 * argv[-2] (callee) replace null |this| with callee's parent and replace
 * primitive values with the equivalent wrapper objects. argv[-1] must
 * not be JSVAL_VOID or an activation object.
 */
extern bool
ComputeThisFromArgv(JSContext *cx, js::Value *argv);

JS_ALWAYS_INLINE JSObject *
ComputeThisFromVp(JSContext *cx, js::Value *vp)
{
    extern bool ComputeThisFromArgv(JSContext *, js::Value *);
    return ComputeThisFromArgv(cx, vp + 2) ? &vp[1].toObject() : NULL;
}

JS_ALWAYS_INLINE bool
ComputeThisFromVpInPlace(JSContext *cx, js::Value *vp)
{
    extern bool ComputeThisFromArgv(JSContext *, js::Value *);
    return ComputeThisFromArgv(cx, vp + 2);
}

JS_ALWAYS_INLINE bool
PrimitiveThisTest(JSFunction *fun, const Value &v)
{
    uint16 flags = fun->flags;
    return (v.isString() && !!(flags & JSFUN_THISP_STRING)) ||
           (v.isNumber() && !!(flags & JSFUN_THISP_NUMBER)) ||
           (v.isBoolean() && !!(flags & JSFUN_THISP_BOOLEAN));
}

/*
 * Abstracts the layout of the stack passed to natives from the engine and from
 * natives to js::Invoke.
 */
struct CallArgs
{
    Value *argv_;
    uintN argc_;
  protected:
    CallArgs() {}
    CallArgs(Value *argv, uintN argc) : argv_(argv), argc_(argc) {}
  public:
    Value *base() const { return argv_ - 2; }
    Value &callee() const { return argv_[-2]; }
    Value &thisv() const { return argv_[-1]; }
    Value &operator[](unsigned i) const { JS_ASSERT(i < argc_); return argv_[i]; }
    Value *argv() const { return argv_; }
    uintN argc() const { return argc_; }
    Value &rval() const { return argv_[-2]; }

    bool computeThis(JSContext *cx) const {
        return ComputeThisFromArgv(cx, argv_);
    }
};

/*
 * The js::InvokeArgumentsGuard passed to js_Invoke must come from an
 * immediately-enclosing successful call to js::StackSpace::pushInvokeArgs,
 * i.e., there must have been no un-popped pushes to cx->stack(). Furthermore,
 * |args.getvp()[0]| should be the callee, |args.getvp()[1]| should be |this|,
 * and the range [args.getvp() + 2, args.getvp() + 2 + args.getArgc()) should
 * be initialized actual arguments.
 */
extern JS_REQUIRES_STACK bool
Invoke(JSContext *cx, const CallArgs &args, uint32 flags);

/*
 * Consolidated js_Invoke flags simply rename certain JSFRAME_* flags, so that
 * we can share bits stored in JSStackFrame.flags and passed to:
 *
 *   js_Invoke
 *   js_InternalInvoke
 *   js_ValueToFunction
 *   js_ValueToFunctionObject
 *   js_ValueToCallableObject
 *   js_ReportIsNotFunction
 *
 * See jsfun.h for the latter four and flag renaming macros.
 */
#define JSINVOKE_CONSTRUCT      JSFRAME_CONSTRUCTING

/*
 * Mask to isolate construct and iterator flags for use with jsfun.h functions.
 */
#define JSINVOKE_FUNFLAGS       JSINVOKE_CONSTRUCT

/*
 * "External" calls may come from C or C++ code using a JSContext on which no
 * JS is running (!cx->fp), so they may need to push a dummy JSStackFrame.
 */

extern bool
ExternalInvoke(JSContext *cx, const Value &thisv, const Value &fval,
               uintN argc, Value *argv, Value *rval);

static JS_ALWAYS_INLINE bool
ExternalInvoke(JSContext *cx, JSObject *obj, const Value &fval,
               uintN argc, Value *argv, Value *rval)
{
    return ExternalInvoke(cx, ObjectOrNullValue(obj), fval, argc, argv, rval);
}

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

/*
 * Executes a script with the given scope chain in the context of the given
 * frame.
 */
extern JS_FORCES_STACK bool
Execute(JSContext *cx, JSObject *chain, JSScript *script,
        JSStackFrame *prev, uintN flags, Value *result);

/*
 * Execute the caller-initialized frame for a user-defined script or function
 * pointed to by cx->fp until completion or error.
 */
extern JS_REQUIRES_STACK JS_NEVER_INLINE bool
Interpret(JSContext *cx, JSStackFrame *stopFp, uintN inlineCallCount = 0, uintN interpFlags = 0);

extern JS_REQUIRES_STACK bool
RunScript(JSContext *cx, JSScript *script, JSStackFrame *fp);

#define JSPROP_INITIALIZER 0x100   /* NB: Not a valid property attribute. */

extern bool
CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                   JSObject **objp, JSProperty **propp);

extern bool
StrictlyEqual(JSContext *cx, const Value &lval, const Value &rval);

/* === except that NaN is the same as NaN and -0 is not the same as +0. */
extern bool
SameValue(const Value &v1, const Value &v2, JSContext *cx);

extern JSType
TypeOfValue(JSContext *cx, const Value &v);

inline bool
InstanceOf(JSContext *cx, JSObject *obj, Class *clasp, Value *argv)
{
    if (obj && obj->getClass() == clasp)
        return true;
    extern bool InstanceOfSlow(JSContext *, JSObject *, Class *, Value *);
    return InstanceOfSlow(cx, obj, clasp, argv);
}

extern JSBool
HasInstance(JSContext *cx, JSObject *obj, const js::Value *v, JSBool *bp);

inline void *
GetInstancePrivate(JSContext *cx, JSObject *obj, Class *clasp, Value *argv)
{
    if (!InstanceOf(cx, obj, clasp, argv))
        return NULL;
    return obj->getPrivate();
}

extern bool
ValueToId(JSContext *cx, const Value &v, jsid *idp);

/*
 * @param closureLevel      The static level of the closure that the cookie
 *                          pertains to.
 * @param cookie            Level amount is a "skip" (delta) value from the
 *                          closure level.
 * @return  The value of the upvar.
 */
extern const js::Value &
GetUpvar(JSContext *cx, uintN level, js::UpvarCookie cookie);

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

#define JS_MAX_INLINE_CALL_COUNT 3000

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

/*
 * Opcode tracing helper. When len is not 0, cx->fp->regs->pc[-len] gives the
 * previous opcode.
 */
extern JS_REQUIRES_STACK void
js_TraceOpcode(JSContext *cx);

/*
 * JS_OPMETER helper functions.
 */
extern void
js_MeterOpcodePair(JSOp op1, JSOp op2);

extern void
js_MeterSlotOpcode(JSOp op, uint32 slot);

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
