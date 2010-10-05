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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef jscntxtinlines_h___
#define jscntxtinlines_h___

#include "jscntxt.h"
#include "jsparse.h"
#include "jsstaticcheck.h"
#include "jsxml.h"
#include "jsregexp.h"

inline js::RegExpStatics *
JSContext::regExpStatics()
{
    VOUCH_HAVE_STACK();
    /*
     * Whether we're on trace or not, the scope chain associated with cx->fp
     * will lead us to the appropriate global. Although cx->fp is stale on
     * trace, trace execution never crosses globals.
     */
    JS_ASSERT(hasfp());
    JSObject *global = fp()->scopeChain().getGlobal();
    js::RegExpStatics *res = js::RegExpStatics::extractFrom(global);
    return res;
}

inline bool
JSContext::ensureGeneratorStackSpace()
{
    bool ok = genStack.reserve(genStack.length() + 1);
    if (!ok)
        js_ReportOutOfMemory(this);
    return ok;
}

JSStackFrame *
JSContext::computeNextFrame(JSStackFrame *fp)
{
    JSStackFrame *next = NULL;
    for (js::StackSegment *ss = currentSegment; ; ss = ss->getPreviousInContext()) {
        JSStackFrame *end = ss->getInitialFrame()->prev();
        for (JSStackFrame *f = ss->getCurrentFrame(); f != end; next = f, f = f->prev()) {
            if (f == fp)
                return next;
        }
        if (end != ss->getPreviousInContext()->getCurrentFrame())
            next = NULL;
    }
}

namespace js {

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSFrameRegs *
StackSegment::getCurrentRegs() const
{
    JS_ASSERT(inContext());
    return isActive() ? cx->regs : getSuspendedRegs();
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSegment::getCurrentFrame() const
{
    return getCurrentRegs()->fp;
}

JS_REQUIRES_STACK inline Value *
StackSpace::firstUnused() const
{
    StackSegment *seg = currentSegment;
    if (!seg) {
        JS_ASSERT(invokeArgEnd == NULL);
        return base;
    }
    if (seg->inContext()) {
        Value *sp = seg->getCurrentRegs()->sp;
        if (invokeArgEnd > sp) {
            JS_ASSERT(invokeSegment == currentSegment);
            JS_ASSERT_IF(seg->maybeContext()->hasfp(),
                         invokeFrame == seg->maybeContext()->fp());
            return invokeArgEnd;
        }
        return sp;
    }
    JS_ASSERT(invokeArgEnd);
    JS_ASSERT(invokeSegment == currentSegment);
    return invokeArgEnd;
}


/* Inline so we don't need the friend API. */
JS_ALWAYS_INLINE bool
StackSpace::isCurrentAndActive(JSContext *cx) const
{
#ifdef DEBUG
    JS_ASSERT_IF(cx->getCurrentSegment(),
                 cx->getCurrentSegment()->maybeContext() == cx);
    cx->assertSegmentsInSync();
#endif
    return currentSegment &&
           currentSegment->isActive() &&
           currentSegment == cx->getCurrentSegment();
}

STATIC_POSTCONDITION(!return || ubound(from) >= nvals)
JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *maybecx, Value *from, ptrdiff_t nvals) const
{
    JS_ASSERT(from == firstUnused());
#ifdef XP_WIN
    JS_ASSERT(from <= commitEnd);
    if (JS_LIKELY(commitEnd - from >= nvals))
        goto success;
    if (end - from < nvals) {
        if (maybecx)
            js_ReportOutOfScriptQuota(maybecx);
        return false;
    }
    if (!bumpCommit(from, nvals)) {
        if (maybecx)
            js_ReportOutOfScriptQuota(maybecx);
        return false;
    }
    goto success;
#else
    if (JS_LIKELY(end - from < nvals)) {
        if (maybecx)
            js_ReportOutOfScriptQuota(maybecx);
        return false;
    }
    goto success;
#endif
  success:
#ifdef DEBUG
    memset(from, 0xde, nvals * sizeof(js::Value));
#endif
    return true;
}

JS_ALWAYS_INLINE bool
StackSpace::ensureEnoughSpaceToEnterTrace()
{
#ifdef XP_WIN
    return ensureSpace(NULL, firstUnused(), MAX_TRACE_SPACE_VALS);
#endif
    return end - firstUnused() > MAX_TRACE_SPACE_VALS;
}

JS_ALWAYS_INLINE bool
StackSpace::EnsureSpaceCheck::operator()(const StackSpace &stack, JSContext *cx,
                                         Value *from, uintN nvals)
{
    return stack.ensureSpace(cx, from, nvals);
}

JS_ALWAYS_INLINE bool
StackSpace::LimitCheck::operator()(const StackSpace &stack, JSContext *cx,
                                   Value *from, uintN nvals)
{
    JS_ASSERT(from == stack.firstUnused());
    JS_ASSERT(from < *limit);
    if (*limit - from >= ptrdiff_t(nvals))
        return true;
    if (stack.bumpCommitAndLimit(base, from, nvals, limit))
        return true;
    js_ReportOverRecursed(cx);
    return false;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
StackSpace::pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard *ag)
{
    if (JS_UNLIKELY(!isCurrentAndActive(cx)))
        return pushSegmentForInvoke(cx, argc, ag);

    Value *sp = cx->regs->sp;
    Value *start = invokeArgEnd > sp ? invokeArgEnd : sp;
    JS_ASSERT(start == firstUnused());
    uintN nvals = 2 + argc;
    if (!ensureSpace(cx, start, nvals))
        return false;

    Value *vp = start;
    Value *vpend = vp + nvals;
    MakeValueRangeGCSafe(vp, vpend);

    /* Use invokeArgEnd to root [vp, vpend) until the frame is pushed. */
    ag->prevInvokeArgEnd = invokeArgEnd;
    invokeArgEnd = vpend;
#ifdef DEBUG
    ag->prevInvokeSegment = invokeSegment;
    invokeSegment = currentSegment;
    ag->prevInvokeFrame = invokeFrame;
    invokeFrame = cx->maybefp();
#endif

    ag->cx = cx;
    ag->argv_ = vp + 2;
    ag->argc_ = argc;
    return true;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInvokeArgs(const InvokeArgsGuard &ag)
{
    if (JS_UNLIKELY(ag.seg != NULL)) {
        popSegmentForInvoke(ag);
        return;
    }

    JS_ASSERT(isCurrentAndActive(ag.cx));
    JS_ASSERT(invokeSegment == currentSegment);
    JS_ASSERT(invokeFrame == ag.cx->maybefp());
    JS_ASSERT(invokeArgEnd == ag.argv() + ag.argc());

#ifdef DEBUG
    invokeSegment = ag.prevInvokeSegment;
    invokeFrame = ag.prevInvokeFrame;
#endif
    invokeArgEnd = ag.prevInvokeArgEnd;
}

JS_ALWAYS_INLINE
InvokeArgsGuard::~InvokeArgsGuard()
{
    if (JS_UNLIKELY(!pushed()))
        return;
    cx->stack().popInvokeArgs(*this);
}

template <class Check>
JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getCallFrame(JSContext *cx, Value *firstUnused, uintN nactual,
                         JSFunction *fun, JSScript *script, uint32 *flags,
                         Check check) const
{
    JS_ASSERT(fun->script() == script);

    /* Include an extra sizeof(JSStackFrame) for the method-jit. */
    uintN nvals = VALUES_PER_STACK_FRAME + script->nslots;
    uintN nformal = fun->nargs;

    /* Maintain layout invariant: &formalArgs[0] == ((Value *)fp) - nformal. */

    if (nactual == nformal) {
        if (JS_UNLIKELY(!check(*this, cx, firstUnused, nvals)))
            return NULL;
        return reinterpret_cast<JSStackFrame *>(firstUnused);
    }

    if (nactual < nformal) {
        *flags |= JSFRAME_UNDERFLOW_ARGS;
        uintN nmissing = nformal - nactual;
        if (JS_UNLIKELY(!check(*this, cx, firstUnused, nmissing + nvals)))
            return NULL;
        SetValueRangeToUndefined(firstUnused, nmissing);
        return reinterpret_cast<JSStackFrame *>(firstUnused + nmissing);
    }

    *flags |= JSFRAME_OVERFLOW_ARGS;
    uintN ncopy = 2 + nformal;
    if (JS_UNLIKELY(!check(*this, cx, firstUnused, ncopy + nvals)))
        return NULL;
    memcpy(firstUnused, firstUnused - (2 + nactual), ncopy * sizeof(Value));
    return reinterpret_cast<JSStackFrame *>(firstUnused + ncopy);
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
StackSpace::getInvokeFrame(JSContext *cx, const CallArgs &args,
                           JSFunction *fun, JSScript *script,
                           uint32 *flags, InvokeFrameGuard *fg) const
{
    JS_ASSERT(firstUnused() == args.argv() + args.argc());

    Value *firstUnused = args.argv() + args.argc();
    fg->regs_.fp = getCallFrame(cx, firstUnused, args.argc(), fun, script, flags,
                                EnsureSpaceCheck());
    fg->regs_.sp = fg->regs_.fp->slots() + script->nfixed;
    fg->regs_.pc = script->code;

    return fg->regs_.fp != NULL;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                            InvokeFrameGuard *fg)
{
    JS_ASSERT(firstUnused() == args.argv() + args.argc());

    if (JS_UNLIKELY(!currentSegment->inContext())) {
        cx->pushSegmentAndFrame(currentSegment, fg->regs_);
    } else {
        fg->prevRegs_ = cx->regs;
        cx->setCurrentRegs(&fg->regs_);
    }

    fg->cx_ = cx;
    JS_ASSERT(isCurrentAndActive(cx));
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInvokeFrame(const InvokeFrameGuard &fg)
{
    JSContext *cx = fg.cx_;
    JSStackFrame *fp = fg.regs_.fp;

    JS_ASSERT(isCurrentAndActive(cx));
    if (JS_UNLIKELY(currentSegment->getInitialFrame() == fp)) {
        cx->popSegmentAndFrame();
    } else {
        JS_ASSERT(&fg.regs_ == cx->regs);
        JS_ASSERT(fp->prev_ == fg.prevRegs_->fp);
        JS_ASSERT(fp->prevpc() == fg.prevRegs_->pc);
        cx->setCurrentRegs(fg.prevRegs_);
    }
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE
InvokeFrameGuard::~InvokeFrameGuard()
{
    if (JS_UNLIKELY(!pushed()))
        return;
    cx_->stack().popInvokeFrame(*this);
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrame(JSContext *cx, Value *sp, uintN nactual,
                           JSFunction *fun, JSScript *script, uint32 *flags) const
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->sp == sp);

    return getCallFrame(cx, sp, nactual, fun, script, flags, EnsureSpaceCheck());
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrameWithinLimit(JSContext *cx, Value *sp, uintN nactual,
                                      JSFunction *fun, JSScript *script, uint32 *flags,
                                      JSStackFrame *base, Value **limit) const
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->sp == sp);

    return getCallFrame(cx, sp, nactual, fun, script, flags, LimitCheck(base, limit));
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInlineFrame(JSContext *cx, JSScript *script, JSStackFrame *fp,
                            JSFrameRegs *regs)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->regs == regs && script == fp->script());

    regs->fp = fp;
    regs->pc = script->code;
    regs->sp = fp->slots() + script->nfixed;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInlineFrame(JSContext *cx, JSStackFrame *prev, Value *newsp)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->fp->prev_ == prev);
    JS_ASSERT(!cx->regs->fp->hasImacropc());
    JS_ASSERT(prev->base() <= newsp && newsp <= cx->regs->fp->formalArgsEnd());

    JSFrameRegs *regs = cx->regs;
    regs->pc = prev->pc(cx, regs->fp);
    regs->fp = prev;
    regs->sp = newsp;
}

JS_ALWAYS_INLINE Value *
StackSpace::getStackLimit(JSContext *cx)
{
    Value *sp = cx->regs->sp;
    JS_ASSERT(sp == firstUnused());
    Value *limit = sp + STACK_QUOTA;

    /*
     * Try to reserve the whole STACK_QUOTA. If that fails, though, just
     * reserve the minimum required space: enough for the nslots + an
     * additional stack frame.
     */
#ifdef XP_WIN
    if (JS_LIKELY(limit <= commitEnd))
        return limit;
    if (ensureSpace(NULL /* don't report error */, sp, STACK_QUOTA))
        return limit;
    uintN minimum = cx->fp()->numSlots() + VALUES_PER_STACK_FRAME;
    return ensureSpace(cx, sp, minimum) ? sp + minimum : NULL;
#else
    if (JS_LIKELY(limit <= end))
        return limit;
    uintN minimum = cx->fp()->numSlots() + VALUES_PER_STACK_FRAME;
    return ensureSpace(cx, sp, minimum) ? sp + minimum : NULL;
#endif
}

JS_REQUIRES_STACK inline
FrameRegsIter::FrameRegsIter(JSContext *cx)
  : cx(cx)
{
    curseg = cx->getCurrentSegment();
    if (JS_UNLIKELY(!curseg || !curseg->isActive())) {
        initSlow();
        return;
    }
    JS_ASSERT(cx->regs->fp);
    curfp = cx->regs->fp;
    cursp = cx->regs->sp;
    curpc = cx->regs->pc;
    return;
}

inline FrameRegsIter &
FrameRegsIter::operator++()
{
    JSStackFrame *fp = curfp;
    JSStackFrame *prev = curfp = curfp->prev();
    if (!prev)
        return *this;

    curpc = curfp->pc(cx, fp);

    if (JS_UNLIKELY(fp == curseg->getInitialFrame())) {
        incSlow(fp, prev);
        return *this;
    }

    cursp = fp->formalArgsEnd();
    return *this;
}

class AutoNamespaceArray : protected AutoGCRooter {
  public:
    AutoNamespaceArray(JSContext *cx) : AutoGCRooter(cx, NAMESPACES) {
        array.init();
    }

    ~AutoNamespaceArray() {
        array.finish(context);
    }

    uint32 length() const { return array.length; }

  public:
    friend void AutoGCRooter::trace(JSTracer *trc);

    JSXMLArray array;
};

#ifdef DEBUG
class CompartmentChecker
{
  private:
    JSContext *context;
    JSCompartment *compartment;

  public:
    explicit CompartmentChecker(JSContext *cx) : context(cx), compartment(cx->compartment) {
        check(cx->hasfp() ? JS_GetGlobalForScopeChain(cx) : cx->globalObject);
        VOUCH_DOES_NOT_REQUIRE_STACK();
    }

    /*
     * Set a breakpoint here (break js::CompartmentChecker::fail) to debug
     * compartment mismatches.
     */
    static void fail(JSCompartment *c1, JSCompartment *c2) {
#ifdef DEBUG_jorendorff
        printf("*** Compartment mismatch %p vs. %p\n", (void *) c1, (void *) c2);
        // JS_NOT_REACHED("compartment mismatch");
#endif
    }

    void check(JSCompartment *c) {
        if (c && c != context->runtime->defaultCompartment) {
            if (!compartment)
                compartment = c;
            else if (c != compartment)
                fail(compartment, c);
        }
    }

    void check(JSPrincipals *) { /* nothing for now */ }

    void check(JSObject *obj) {
        if (obj)
            check(obj->getCompartment(context));
    }

    void check(const js::Value &v) {
        if (v.isObject())
            check(&v.toObject());
    }

    void check(jsval v) {
        check(Valueify(v));
    }

    void check(const ValueArray &arr) {
        for (size_t i = 0; i < arr.length; i++)
            check(arr.array[i]);
    }

    void check(const JSValueArray &arr) {
        for (size_t i = 0; i < arr.length; i++)
            check(arr.array[i]);
    }

    void check(jsid id) {
        if (JSID_IS_OBJECT(id))
            check(JSID_TO_OBJECT(id));
    }
    
    void check(JSIdArray *ida) {
        if (ida) {
            for (jsint i = 0; i < ida->length; i++) {
                if (JSID_IS_OBJECT(ida->vector[i]))
                    check(ida->vector[i]);
            }
        }
    }

    void check(JSScript *script) {
        if (script && script->u.object)
            check(script->u.object);
    }

    void check(JSString *) { /* nothing for now */ }
};

#endif

/*
 * Don't perform these checks when called from a finalizer. The checking
 * depends on other objects not having been swept yet.
 */
#define START_ASSERT_SAME_COMPARTMENT()                                       \
    if (cx->runtime->gcRunning)                                               \
        return;                                                               \
    CompartmentChecker c(cx)

template <class T1> inline void
assertSameCompartment(JSContext *cx, T1 t1)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
#endif
}

template <class T1, class T2> inline void
assertSameCompartment(JSContext *cx, T1 t1, T2 t2)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
#endif
}

template <class T1, class T2, class T3> inline void
assertSameCompartment(JSContext *cx, T1 t1, T2 t2, T3 t3)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
#endif
}

template <class T1, class T2, class T3, class T4> inline void
assertSameCompartment(JSContext *cx, T1 t1, T2 t2, T3 t3, T4 t4)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
    c.check(t4);
#endif
}

template <class T1, class T2, class T3, class T4, class T5> inline void
assertSameCompartment(JSContext *cx, T1 t1, T2 t2, T3 t3, T4 t4, T5 t5)
{
#ifdef DEBUG
    START_ASSERT_SAME_COMPARTMENT();
    c.check(t1);
    c.check(t2);
    c.check(t3);
    c.check(t4);
    c.check(t5);
#endif
}

#undef START_ASSERT_SAME_COMPARTMENT

STATIC_PRECONDITION_ASSUME(ubound(vp) >= argc + 2)
JS_ALWAYS_INLINE bool
CallJSNative(JSContext *cx, js::Native native, uintN argc, js::Value *vp)
{
#ifdef DEBUG
    JSBool alreadyThrowing = cx->throwing;
#endif
    assertSameCompartment(cx, ValueArray(vp, argc + 2));
    JSBool ok = native(cx, argc, vp);
    if (ok) {
        assertSameCompartment(cx, vp[0]);
        JS_ASSERT_IF(!alreadyThrowing, !cx->throwing);
    }
    return ok;
}

STATIC_PRECONDITION(ubound(vp) >= argc + 2)
JS_ALWAYS_INLINE bool
CallJSNativeConstructor(JSContext *cx, js::Native native, uintN argc, js::Value *vp)
{
#ifdef DEBUG
    JSObject *callee = &vp[0].toObject();
#endif

    JS_ASSERT(vp[1].isMagic());
    if (!CallJSNative(cx, native, argc, vp))
        return false;

    /*
     * Native constructors must return non-primitive values on success.
     * Although it is legal, if a constructor returns the callee, there is a
     * 99.9999% chance it is a bug. If any valid code actually wants the
     * constructor to return the callee, the assertion can be removed or
     * (another) conjunct can be added to the antecedent.
     *
     * Proxies are exceptions to both rules: they can return primitives and
     * they allow content to return the callee.
     *
     * (new Object(Object)) returns the callee.
     */
    extern JSBool proxy_Construct(JSContext *, uintN, Value *);
    JS_ASSERT_IF(native != proxy_Construct &&
                 (!callee->isFunction() || callee->getFunctionPrivate()->u.n.clasp != &js_ObjectClass),
                 !vp->isPrimitive() && callee != &vp[0].toObject());

    return true;
}

JS_ALWAYS_INLINE bool
CallJSPropertyOp(JSContext *cx, js::PropertyOp op, JSObject *obj, jsid id, js::Value *vp)
{
    assertSameCompartment(cx, obj, id, *vp);
    JSBool ok = op(cx, obj, id, vp);
    if (ok)
        assertSameCompartment(cx, obj, *vp);
    return ok;
}

JS_ALWAYS_INLINE bool
CallJSPropertyOpSetter(JSContext *cx, js::PropertyOp op, JSObject *obj, jsid id, js::Value *vp)
{
    assertSameCompartment(cx, obj, id, *vp);
    return op(cx, obj, id, vp);
}

}  /* namespace js */

#endif /* jscntxtinlines_h___ */
