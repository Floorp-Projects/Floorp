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
CallStack::getCurrentFrame() const
{
    JS_ASSERT(inContext());
    return isSuspended() ? getSuspendedFrame() : cx->fp;
}

JS_REQUIRES_STACK inline jsval *
StackSpace::firstUnused() const
{
    CallStack *ccs = currentCallStack;
    if (!ccs)
        return base;
    if (JSContext *cx = ccs->maybeContext()) {
        if (!ccs->isSuspended())
            return cx->regs->sp;
        return ccs->getSuspendedRegs()->sp;
    }
    return ccs->getInitialArgEnd();
}

/* Inline so we don't need the friend API. */
JS_ALWAYS_INLINE void
StackSpace::assertIsCurrent(JSContext *cx) const
{
#ifdef DEBUG
    JS_ASSERT(cx == currentCallStack->maybeContext());
    JS_ASSERT(cx->getCurrentCallStack() == currentCallStack);
    cx->assertCallStacksInSync();
#endif
}

JS_ALWAYS_INLINE bool
StackSpace::ensureSpace(JSContext *maybecx, jsval *from, ptrdiff_t nvals) const
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

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrame(JSContext *cx, jsval *sp,
                           uintN nmissing, uintN nfixed) const
{
    assertIsCurrent(cx);
    JS_ASSERT(cx->hasActiveCallStack());
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
    assertIsCurrent(cx);
    JS_ASSERT(cx->hasActiveCallStack());
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
    assertIsCurrent(cx);
    JS_ASSERT(cx->hasActiveCallStack());
    JS_ASSERT(cx->fp == up && up->down == down);
    JS_ASSERT(up->savedPC == JSStackFrame::sInvalidPC);

    JSFrameRegs *regs = cx->regs;
    regs->pc = down->savedPC;
    regs->sp = up->argv - 1;
#ifdef DEBUG
    down->savedPC = JSStackFrame::sInvalidPC;
#endif
    cx->setCurrentFrame(down);
}

void
AutoIdArray::trace(JSTracer *trc) {
    JS_ASSERT(tag == IDARRAY);
    js::TraceValues(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
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

    void check(jsval v) {
        if (!JSVAL_IS_PRIMITIVE(v))
            check(JSVAL_TO_OBJECT(v));
    }

    void check(const ValueArray &arr) {
        for (size_t i = 0; i < arr.length; i++)
            check(arr.array[i]);
    }
    
    void check(JSIdArray *ida) {
        if (ida) {
            for (jsint i = 0; i < ida->length; i++)
                check(ID_TO_VALUE(ida->vector[i]));
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
callJSNative(JSContext *cx, JSNative native, JSObject *thisobj, uintN argc, jsval *argv, jsval *rval)
{
    assertSameCompartment(cx, thisobj, ValueArray(argv, argc));
    JSBool ok = native(cx, thisobj, argc, argv, rval);
    if (ok)
        assertSameCompartment(cx, *rval);
    return ok;
}

inline JSBool
callJSFastNative(JSContext *cx, JSFastNative native, uintN argc, jsval *vp)
{
    assertSameCompartment(cx, ValueArray(vp, argc + 2));
    JSBool ok = native(cx, argc, vp);
    if (ok)
        assertSameCompartment(cx, vp[0]);
    return ok;
}

inline JSBool
callJSPropertyOp(JSContext *cx, JSPropertyOp op, JSObject *obj, jsval idval, jsval *vp)
{
    assertSameCompartment(cx, obj, idval, *vp);
    JSBool ok = op(cx, obj, idval, vp);
    if (ok)
        assertSameCompartment(cx, obj, *vp);
    return ok;
}

inline JSBool
callJSPropertyOpSetter(JSContext *cx, JSPropertyOp op, JSObject *obj, jsval idval, jsval *vp)
{
    assertSameCompartment(cx, obj, idval, *vp);
    return op(cx, obj, idval, vp);
}

}

#endif /* jscntxtinlines_h___ */
