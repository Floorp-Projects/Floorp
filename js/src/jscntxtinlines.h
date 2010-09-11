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

inline bool
JSContext::ensureGeneratorStackSpace()
{
    bool ok = genStack.reserve(genStack.length() + 1);
    if (!ok)
        js_ReportOutOfMemory(this);
    return ok;
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

/*
 * SunSpider and v8bench have roughly an average of 9 slots per script.
 * Our heuristic for a quick over-recursion check uses a generous slot
 * count based on this estimate. We take this frame size and multiply it
 * by the old recursion limit from the interpreter.
 *
 * Worst case, if an average size script (<=9 slots) over recurses, it'll
 * effectively be the same as having increased the old inline call count
 * to <= 5,000.
 */
static const uint32 MAX_STACK_USAGE = (VALUES_PER_STACK_FRAME + 18) * JS_MAX_INLINE_CALL_COUNT;

JS_ALWAYS_INLINE Value *
StackSpace::makeStackLimit(Value *start) const
{
    Value *limit = JS_MIN(start + MAX_STACK_USAGE, end);
#ifdef XP_WIN
    limit = JS_MIN(limit, commitEnd);
#endif
    return limit;
}

JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *maybecx, Value *start, Value *from,
                        Value *& limit, uint32 nslots) const
{
    JS_ASSERT(from == firstUnused());
#ifdef XP_WIN
    /*
     * If commitEnd < limit, we're guaranteed that we reached the end of the
     * commit depth, because stackLimit is MIN(commitEnd, limit). If we did
     * reach this soft limit, check if we can bump the commit end without
     * over-recursing.
     */
    ptrdiff_t nvals = VALUES_PER_STACK_FRAME + nslots;
    if (commitEnd <= limit && from + nvals < (start + MAX_STACK_USAGE)) {
        if (!ensureSpace(maybecx, from, nvals))
            return false;

        /* Compute a new limit. */
        limit = makeStackLimit(start);

        return true;
    }
#endif
    js_ReportOverRecursed(maybecx);
    return false;
}

JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *maybecx, Value *from, ptrdiff_t nvals) const
{
    JS_ASSERT(from == firstUnused());
#ifdef XP_WIN
    JS_ASSERT(from <= commitEnd);
    if (commitEnd - from >= nvals)
        return true;
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
    return true;
#else
    if (end - from < nvals) {
        if (maybecx)
            js_ReportOutOfScriptQuota(maybecx);
        return false;
    }
    return true;
#endif
}

JS_ALWAYS_INLINE bool
StackSpace::ensureEnoughSpaceToEnterTrace()
{
#ifdef XP_WIN
    return ensureSpace(NULL, firstUnused(), MAX_TRACE_SPACE_VALS);
#endif
    return end - firstUnused() > MAX_TRACE_SPACE_VALS;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
StackSpace::pushInvokeArgs(JSContext *cx, uintN argc, InvokeArgsGuard &ag)
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
    ag.prevInvokeArgEnd = invokeArgEnd;
    invokeArgEnd = vpend;
#ifdef DEBUG
    ag.prevInvokeSegment = invokeSegment;
    invokeSegment = currentSegment;
    ag.prevInvokeFrame = invokeFrame;
    invokeFrame = cx->maybefp();
#endif

    ag.cx = cx;
    ag.argv_ = vp + 2;
    ag.argc_ = argc;
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

JS_REQUIRES_STACK JS_ALWAYS_INLINE bool
StackSpace::getInvokeFrame(JSContext *cx, const CallArgs &args,
                           uintN nmissing, uintN nfixed,
                           InvokeFrameGuard &fg) const
{
    JS_ASSERT(firstUnused() == args.argv() + args.argc());

    Value *start = args.argv() + args.argc();
    ptrdiff_t nvals = nmissing + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, start, nvals))
        return false;
    fg.regs.fp = reinterpret_cast<JSStackFrame *>(start + nmissing);
    return true;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                            InvokeFrameGuard &fg)
{
    JS_ASSERT(firstUnused() == args.argv() + args.argc());

    JSStackFrame *fp = fg.regs.fp;
    JSStackFrame *down = cx->maybefp();
    fp->down = down;
    if (JS_UNLIKELY(!currentSegment->inContext())) {
        cx->pushSegmentAndFrame(currentSegment, fg.regs);
    } else {
#ifdef DEBUG
        fp->savedPC = JSStackFrame::sInvalidPC;
        JS_ASSERT(down->savedPC == JSStackFrame::sInvalidPC);
#endif
        down->savedPC = cx->regs->pc;
        fg.prevRegs = cx->regs;
        cx->setCurrentRegs(&fg.regs);
    }
    fg.cx = cx;

    JS_ASSERT(isCurrentAndActive(cx));
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInvokeFrame(const InvokeFrameGuard &fg)
{
    JSContext *cx = fg.cx;
    JSStackFrame *fp = fg.regs.fp;

    JS_ASSERT(isCurrentAndActive(cx));
    if (JS_UNLIKELY(currentSegment->getInitialFrame() == fp)) {
        cx->popSegmentAndFrame();
    } else {
        JS_ASSERT(&fg.regs == cx->regs);
        JS_ASSERT(fp->down == fg.prevRegs->fp);
        cx->setCurrentRegs(fg.prevRegs);
#ifdef DEBUG
        cx->fp()->savedPC = JSStackFrame::sInvalidPC;
#endif
    }
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE
InvokeFrameGuard::~InvokeFrameGuard()
{
    if (JS_UNLIKELY(!pushed()))
        return;
    cx->stack().popInvokeFrame(*this);
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrameUnchecked(JSContext *cx, Value *sp,
                                    uintN nmissing) const
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->sp == sp);

    JSStackFrame *fp = reinterpret_cast<JSStackFrame *>(sp + nmissing);
    return fp;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrame(JSContext *cx, Value *sp,
                           uintN nmissing, uintN nfixed) const
{
    ptrdiff_t nvals = nmissing + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, sp, nvals))
        return NULL;

    return getInlineFrameUnchecked(cx, sp, nmissing);
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInlineFrame(JSContext *cx, JSStackFrame *fp, jsbytecode *pc,
                            JSStackFrame *newfp)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->regs->fp == fp && cx->regs->pc == pc);

    fp->savedPC = pc;
    newfp->down = fp;
#ifdef DEBUG
    newfp->savedPC = JSStackFrame::sInvalidPC;
#endif
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInlineFrame(JSContext *cx, JSStackFrame *up, JSStackFrame *down)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->fp == up && up->down == down);
    JS_ASSERT(up->savedPC == JSStackFrame::sInvalidPC);
    JS_ASSERT(!up->hasIMacroPC());

    JSFrameRegs *regs = cx->regs;
    regs->fp = down;
    regs->pc = down->savedPC;
#ifdef DEBUG
    down->savedPC = JSStackFrame::sInvalidPC;
#endif
}

JS_REQUIRES_STACK inline
FrameRegsIter::FrameRegsIter(JSContext *cx)
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

inline Value *
FrameRegsIter::contiguousDownFrameSP(JSStackFrame *up)
{
    JS_ASSERT(up->argv);
    Value *sp = up->argv + up->numActualArgs();
#ifdef DEBUG
    JS_ASSERT(sp <= up->argEnd());
    JS_ASSERT(sp >= (up->down->hasScript() ? up->down->base() : up->down->slots()));
    if (up->hasFunction()) {
        uint16 nargs = up->getFunction()->nargs;
        uintN argc = up->numActualArgs();
        uintN missing = argc < nargs ? nargs - argc : 0;
        JS_ASSERT(sp == up->argEnd() - missing);
    } else {
        JS_ASSERT(sp == up->argEnd());
    }
#endif
    return sp;
}

inline FrameRegsIter &
FrameRegsIter::operator++()
{
    JSStackFrame *up = curfp;
    JSStackFrame *down = curfp = curfp->down;
    if (!down)
        return *this;

    curpc = down->savedPC;

    if (JS_UNLIKELY(up == curseg->getInitialFrame())) {
        incSlow(up, down);
        return *this;
    }

    cursp = contiguousDownFrameSP(up);
    return *this;
}

void
AutoIdArray::trace(JSTracer *trc) {
    JS_ASSERT(tag == IDARRAY);
    MarkIdRange(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
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
     * constructor to return the callee, this can be removed.
     *
     * Proxies are exceptions to both rules: they can return primitives and
     * they allow content to return the callee.
     */
    extern JSBool proxy_Construct(JSContext *, uintN, Value *);
    JS_ASSERT_IF(native != proxy_Construct,
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
