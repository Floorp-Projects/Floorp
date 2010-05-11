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
#include "jsxml.h"

#include "jsobjinlines.h"

namespace js {

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
CallStack::getCurrentFrame() const
{
    JS_ASSERT(inContext());
    return isSuspended() ? getSuspendedFrame() : cx->fp;
}

JS_REQUIRES_STACK inline Value *
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

#ifdef DEBUG
/* Inline so we don't need the friend API. */
JS_ALWAYS_INLINE bool
StackSpace::isCurrent(JSContext *cx) const
{
    JS_ASSERT(cx == currentCallStack->maybeContext());
    JS_ASSERT(cx->getCurrentCallStack() == currentCallStack);
    JS_ASSERT(cx->callStackInSync());
    return true;
}
#endif

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
    return ensureSpace(NULL, firstUnused(), sMaxJSValsNeededForTrace);
#endif
    return end - firstUnused() > sMaxJSValsNeededForTrace;
}

JS_ALWAYS_INLINE void
StackSpace::popInvokeArgs(JSContext *cx, Value *vp)
{
    JS_ASSERT(!currentCallStack->inContext());
    currentCallStack = currentCallStack->getPreviousInThread();
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE JSStackFrame *
StackSpace::getInlineFrame(JSContext *cx, Value *sp,
                           uintN nmissing, uintN nslots) const
{
    JS_ASSERT(isCurrent(cx) && cx->hasActiveCallStack());
    JS_ASSERT(cx->regs->sp == sp);

    ptrdiff_t nvals = nmissing + ValuesPerStackFrame + nslots;
    if (!ensureSpace(cx, sp, nvals))
        return NULL;

    JSStackFrame *fp = reinterpret_cast<JSStackFrame *>(sp + nmissing);
    return fp;
}

JS_REQUIRES_STACK JS_ALWAYS_INLINE void
StackSpace::pushInlineFrame(JSContext *cx, JSStackFrame *fp, jsbytecode *pc,
                            JSStackFrame *newfp)
{
    JS_ASSERT(isCurrent(cx) && cx->hasActiveCallStack());
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
    JS_ASSERT(isCurrent(cx) && cx->hasActiveCallStack());
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

/*
 * InvokeArgsGuard is used outside the JS engine. To simplify symbol visibility
 * issues, force InvokeArgsGuard members inline:
 */
JS_ALWAYS_INLINE
InvokeArgsGuard::InvokeArgsGuard()
  : cx(NULL), cs(NULL), vp(NULL)
{}

JS_ALWAYS_INLINE
InvokeArgsGuard::InvokeArgsGuard(Value *vp, uintN argc)
  : cx(NULL), cs(NULL), vp(vp), argc(argc)
{}

JS_ALWAYS_INLINE
InvokeArgsGuard::~InvokeArgsGuard()
{
    if (!cs)
        return;
    JS_ASSERT(cs == cx->stack().getCurrentCallStack());
    cx->stack().popInvokeArgs(cx, vp);
}

void
AutoIdArray::trace(JSTracer *trc) {
    JS_ASSERT(tag == IDARRAY);
    TraceIds(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
}

class AutoNamespaces : protected AutoGCRooter {
  protected:
    AutoNamespaces(JSContext *cx) : AutoGCRooter(cx, NAMESPACES) {
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  public:
    JSXMLArray array;
};

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        JS_SET_TRACING_NAME(trc, "js::AutoValueRooter.val");
        CallGCMarkerIfGCThing(trc, static_cast<AutoValueRooter *>(this)->val);
        return;

      case SPROP:
        static_cast<AutoScopePropertyRooter *>(this)->sprop->trace(trc);
        return;

      case WEAKROOTS:
        static_cast<AutoSaveWeakRoots *>(this)->savedRoots.mark(trc);
        return;

      case COMPILER:
        static_cast<JSCompiler *>(this)->trace(trc);
        return;

      case SCRIPT:
        if (JSScript *script = static_cast<AutoScriptRooter *>(this)->script)
            js_TraceScript(trc, script);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        TraceIds(trc, ida->length, ida->vector, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropertyDescriptorArray &descriptors =
            static_cast<AutoDescriptorArray *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropertyDescriptor &desc = descriptors[i];

            JS_SET_TRACING_NAME(trc, "PropertyDescriptor::value");
            CallGCMarkerIfGCThing(trc, desc.value);
            JS_SET_TRACING_NAME(trc, "PropertyDescriptor::get");
            CallGCMarkerIfGCThing(trc, desc.get);
            JS_SET_TRACING_NAME(trc, "PropertyDescriptor::set");
            CallGCMarkerIfGCThing(trc, desc.set);
            js_TraceId(trc, desc.id);
        }
        return;
      }

      case NAMESPACES: {
        JSXMLArray &array = static_cast<AutoNamespaces *>(this)->array;
        TraceObjectVector(trc, reinterpret_cast<JSObject **>(array.vector), array.length);
        array.cursors->trace(trc);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj) {
            JS_SET_TRACING_NAME(trc, "js::AutoObjectRooter.obj");
            CallGCMarker(trc, obj, JSTRACE_OBJECT);
        }
        return;

      case ID:
        JS_SET_TRACING_NAME(trc, "js::AutoIdRooter.val");
        CallGCMarkerIfGCThing(trc, static_cast<AutoIdRooter *>(this)->idval);
        return;

      case VECTOR: {
        Vector<Value, 8> &vector = static_cast<AutoValueVector *>(this)->vector;
        TraceValues(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    TraceValues(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

}  /* namespace js */

#endif /* jscntxtinlines_h___ */
