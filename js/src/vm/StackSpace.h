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

#ifndef StackSpace_h__
#define StackSpace_h__

#include "jsprvtd.h"

namespace js {

/* Forward declarations. */
class FrameGuard;
class DummyFrameGuard;
class ExecuteFrameGuard;
class GeneratorFrameGuard;

/* Flags specified for a frame as it is constructed. */
enum InitialFrameFlags {
    INITIAL_NONE           =          0,
    INITIAL_CONSTRUCT      =       0x80, /* == StackFrame::CONSTRUCTING, asserted in Stack.h */
    INITIAL_LOWERED        =   0x200000  /* == StackFrame::LOWERED_CALL_APPLY, asserted in Stack.h */
};

enum ExecuteType {
    EXECUTE_GLOBAL         =        0x1, /* == StackFrame::GLOBAL */
    EXECUTE_DIRECT_EVAL    =        0x8, /* == StackFrame::EVAL */
    EXECUTE_INDIRECT_EVAL  =        0x9, /* == StackFrame::GLOBAL | EVAL */
    EXECUTE_DEBUG          =       0x18  /* == StackFrame::EVAL | DEBUGGER */
};

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

    /* See stack layout comment in Stack.h. */
    inline Value *firstUnused() const;

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
    bool tryBumpLimit(JSContext *cx, Value *from, uintN nvals, Value **limit);

    /* Called during GC: mark segments, frames, and slots under firstUnused. */
    void mark(JSTracer *trc);

    /* We only report the committed size;  uncommitted size is uninteresting. */
    JS_FRIEND_API(size_t) sizeOfCommitted();
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
                 JSFunction *fun, JSScript *script, /*StackFrame::Flags*/ uint32 *pflags) const;

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
    inline bool hasfp() const;

    /*
     * Return the most recent script activation's registers with the same
     * caveat as hasfp regarding JS_SaveFrameChain.
     */
    inline FrameRegs *maybeRegs() const;
    inline StackFrame *maybefp() const;

    /* Faster alternatives to maybe* functions. */
    inline FrameRegs &regs() const;
    inline StackFrame *fp() const;

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
    inline JSObject *currentScriptedScopeChain() const;

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
                              void *ncode, InitialFrameFlags initial, Value **stackLimit);

    bool saveFrameChain();
    void restoreFrameChain();

    /*
     * As an optimization, the interpreter/mjit can operate on a local
     * FrameRegs instance repoint the ContextStack to this local instance.
     */
    inline void repointRegs(FrameRegs *regs);

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

} /* namespace js */

#endif /* StackSpace_h__ */
