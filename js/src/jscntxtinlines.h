/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSegment::getCurrentFrame() const
{
    JS_ASSERT(inContext());
    return isActive() ? cx->fp : getSuspendedFrame();
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSFrameRegs *
StackSegment::getCurrentRegs() const
{
    JS_ASSERT(inContext());
    return isActive() ? cx->regs : getSuspendedRegs();
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
            JS_ASSERT_IF(seg->maybeContext()->fp, invokeFrame == seg->maybeContext()->fp);
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
    invokeFrame = cx->fp;
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
    JS_ASSERT(invokeFrame == ag.cx->fp);
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
    fg.fp = reinterpret_cast<JSStackFrame *>(start + nmissing);
    return true;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInvokeFrame(JSContext *cx, const CallArgs &args,
                            InvokeFrameGuard &fg)
{
    JS_ASSERT(firstUnused() == args.argv() + args.argc());

    JSStackFrame *fp = fg.fp;
    JSStackFrame *down = cx->fp;
    fp->down = down;
    if (JS_UNLIKELY(!currentSegment->inContext())) {
        cx->pushSegmentAndFrame(currentSegment, fp, fg.regs);
    } else {
#ifdef DEBUG
        fp->savedPC = JSStackFrame::sInvalidPC;
        JS_ASSERT(down->savedPC == JSStackFrame::sInvalidPC);
#endif
        down->savedPC = cx->regs->pc;
        cx->setCurrentFrame(fp);
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
    JSStackFrame *fp = fg.fp;

    JS_ASSERT(isCurrentAndActive(cx));
    if (JS_UNLIKELY(currentSegment->getInitialFrame() == fp)) {
        cx->popSegmentAndFrame();
    } else {
        JS_ASSERT(fp == cx->fp);
        JS_ASSERT(&fg.regs == cx->regs);
        cx->setCurrentFrame(fp->down);
        cx->setCurrentRegs(fg.prevRegs);
#ifdef DEBUG
        cx->fp->savedPC = JSStackFrame::sInvalidPC;
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
StackSpace::getInlineFrame(JSContext *cx, Value *sp,
                           uintN nmissing, uintN nfixed) const
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->regs->sp == sp);

    ptrdiff_t nvals = nmissing + VALUES_PER_STACK_FRAME + nfixed;
    if (!ensureSpace(cx, sp, nvals))
        return NULL;

    JSStackFrame *fp = reinterpret_cast<JSStackFrame *>(sp + nmissing);
    return fp;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInlineFrame(JSContext *cx, JSStackFrame *fp, jsbytecode *pc,
                            JSStackFrame *newfp)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->fp == fp && cx->regs->pc == pc);

    fp->savedPC = pc;
    newfp->down = fp;
#ifdef DEBUG
    newfp->savedPC = JSStackFrame::sInvalidPC;
#endif
    cx->setCurrentFrame(newfp);
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::popInlineFrame(JSContext *cx, JSStackFrame *up, JSStackFrame *down)
{
    JS_ASSERT(isCurrentAndActive(cx));
    JS_ASSERT(cx->hasActiveSegment());
    JS_ASSERT(cx->fp == up && up->down == down);
    JS_ASSERT(up->savedPC == JSStackFrame::sInvalidPC);

    JSFrameRegs *regs = cx->regs;
    regs->pc = down->savedPC;
#ifdef DEBUG
    down->savedPC = JSStackFrame::sInvalidPC;
#endif
    cx->setCurrentFrame(down);
}

JS_REQUIRES_STACK inline
FrameRegsIter::FrameRegsIter(JSContext *cx)
{
    curseg = cx->getCurrentSegment();
    if (JS_UNLIKELY(!curseg || !curseg->isActive()))
        initSlow();
    JS_ASSERT(cx->fp);
    curfp = cx->fp;
    cursp = cx->regs->sp;
    curpc = cx->regs->pc;
    return;
}

inline Value *
FrameRegsIter::contiguousDownFrameSP(JSStackFrame *up)
{
    JS_ASSERT(up->argv);
    Value *sp = up->argv + up->argc;
#ifdef DEBUG
    JS_ASSERT(sp <= up->argEnd());
    JS_ASSERT(sp >= (up->down->script ? up->down->base() : up->down->slots()));
    if (up->fun) {
        uint16 nargs = up->fun->nargs;
        uintN argc = up->argc;
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
        check(cx->fp ? JS_GetGlobalForScopeChain(cx) : cx->globalObject);
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

inline JSBool
callJSNative(JSContext *cx, js::Native native, JSObject *thisobj, uintN argc, js::Value *argv, js::Value *rval)
{
    assertSameCompartment(cx, thisobj, ValueArray(argv, argc));
    JSBool ok = native(cx, thisobj, argc, argv, rval);
    if (ok)
        assertSameCompartment(cx, *rval);
    return ok;
}

inline JSBool
callJSFastNative(JSContext *cx, js::FastNative native, uintN argc, js::Value *vp)
{
    assertSameCompartment(cx, ValueArray(vp, argc + 2));
    JSBool ok = native(cx, argc, vp);
    if (ok)
        assertSameCompartment(cx, vp[0]);
    return ok;
}

inline JSBool
callJSPropertyOp(JSContext *cx, js::PropertyOp op, JSObject *obj, jsid id, js::Value *vp)
{
    assertSameCompartment(cx, obj, id, *vp);
    JSBool ok = op(cx, obj, id, vp);
    if (ok)
        assertSameCompartment(cx, obj, *vp);
    return ok;
}

inline JSBool
callJSPropertyOpSetter(JSContext *cx, js::PropertyOp op, JSObject *obj, jsid id, js::Value *vp)
{
    assertSameCompartment(cx, obj, id, *vp);
    return op(cx, obj, id, vp);
}

}  /* namespace js */

#endif /* jscntxtinlines_h___ */
