/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
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

#include "jsgcmark.h"
#include "jsprf.h"
#include "jsscope.h"
#include "jsstr.h"

#include "jsobjinlines.h"
#include "jsscopeinlines.h"

#include "vm/String-inl.h"
#include "methodjit/MethodJIT.h"

/*
 * There are two mostly separate mark paths. The first is a fast path used
 * internally in the GC. The second is a slow path used for root marking and
 * for API consumers like the cycle collector or Class::trace implementations.
 *
 * The fast path uses explicit stacks. The basic marking process during a GC is
 * that all roots are pushed on to a mark stack, and then each item on the
 * stack is scanned (possibly pushing more stuff) until the stack is empty.
 *
 * PushMarkStack pushes a GC thing onto the mark stack. In some cases (shapes
 * or strings) it eagerly marks the object rather than pushing it. Popping and
 * scanning is done by the processMarkStackTop method. For efficiency reasons
 * like tail recursion elimination that method also implements the scanning of
 * objects. For other GC things it uses helper methods.
 *
 * Most of the marking code outside jsgcmark uses functions like MarkObject,
 * MarkString, etc. These functions check if an object is in the compartment
 * currently being GCed. If it is, they call PushMarkStack. Roots are pushed
 * this way as well as pointers traversed inside trace hooks (for things like
 * IteratorClass). It it always valid to call a MarkX function instead of
 * PushMarkStack, although it may be slower.
 *
 * The MarkX functions also handle non-GC object traversal. In this case, they
 * call a callback for each object visited. This is a recursive process; the
 * mark stacks are not involved. These callbacks may ask for the outgoing
 * pointers to be visited. Eventually, this leads to the MarkChildren functions
 * being called. These functions duplicate much of the functionality of
 * scanning functions, but they don't push onto an explicit stack.
 */

using namespace js;
using namespace js::gc;

namespace js {
namespace gc {

static inline void
PushMarkStack(GCMarker *gcmarker, JSXML *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSObject *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSScript *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, const Shape *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, types::TypeObject *thing);

template<typename T>
static inline void
CheckMarkedThing(JSTracer *trc, T *thing)
{
    JS_ASSERT(thing);
    JS_ASSERT(trc->debugPrinter || trc->debugPrintArg);
    JS_ASSERT_IF(trc->runtime->gcCurrentCompartment, IS_GC_MARKING_TRACER(trc));

    JS_ASSERT(thing->isAligned());

    JS_ASSERT(thing->compartment());
    JS_ASSERT(thing->compartment()->rt == trc->runtime);
}

template<typename T>
void
Mark(JSTracer *trc, T *thing)
{
    CheckMarkedThing(trc, thing);

    JSRuntime *rt = trc->runtime;

    JS_ASSERT_IF(rt->gcCheckCompartment,
                 thing->compartment() == rt->gcCheckCompartment ||
                 thing->compartment() == rt->atomsCompartment);

    /*
     * Don't mark things outside a compartment if we are in a per-compartment
     * GC.
     */
    if (!rt->gcCurrentCompartment || thing->compartment() == rt->gcCurrentCompartment) {
        if (IS_GC_MARKING_TRACER(trc))
            PushMarkStack(static_cast<GCMarker *>(trc), thing);
        else
            trc->callback(trc, (void *)thing, GetGCThingTraceKind(thing));
    }

#ifdef DEBUG
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
#endif
}

void
MarkStringUnbarriered(JSTracer *trc, JSString *str, const char *name)
{
    JS_ASSERT(str);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, str);
}

void
MarkString(JSTracer *trc, const MarkablePtr<JSString> &str, const char *name)
{
    MarkStringUnbarriered(trc, str.value, name);
}

void
MarkAtom(JSTracer *trc, JSAtom *atom)
{
    JS_ASSERT(trc);
    JS_ASSERT(atom);
    Mark(trc, atom);
}

void
MarkAtom(JSTracer *trc, JSAtom *atom, const char *name)
{
    MarkStringUnbarriered(trc, atom, name);
}

void
MarkObjectUnbarriered(JSTracer *trc, JSObject *obj, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(obj);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, obj);
}

void
MarkObject(JSTracer *trc, const MarkablePtr<JSObject> &obj, const char *name)
{
    MarkObjectUnbarriered(trc, obj.value, name);
}

void
MarkScriptUnbarriered(JSTracer *trc, JSScript *script, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(script);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, script);
}

void
MarkScript(JSTracer *trc, const MarkablePtr<JSScript> &script, const char *name)
{
    MarkScriptUnbarriered(trc, script.value, name);
}

void
MarkShapeUnbarriered(JSTracer *trc, const Shape *shape, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(shape);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, shape);
}

void
MarkShape(JSTracer *trc, const MarkablePtr<const Shape> &shape, const char *name)
{
    MarkShapeUnbarriered(trc, shape.value, name);
}

void
MarkBaseShapeUnbarriered(JSTracer *trc, BaseShape *base, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(base);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, base);
}

void
MarkBaseShape(JSTracer *trc, const MarkablePtr<BaseShape> &base, const char *name)
{
    MarkBaseShapeUnbarriered(trc, base.value, name);
}

void
MarkTypeObjectUnbarriered(JSTracer *trc, types::TypeObject *type, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(type);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, type);

    /*
     * Mark parts of a type object skipped by ScanTypeObject. ScanTypeObject is
     * only used for marking tracers; for tracers with a callback, if we
     * reenter through JS_TraceChildren then MarkChildren will *not* skip these
     * members, and we don't need to handle them here.
     */
    if (IS_GC_MARKING_TRACER(trc)) {
        if (type->singleton && !type->lazy())
            MarkObject(trc, type->singleton, "type_singleton");
        if (type->interpretedFunction)
            MarkObject(trc, type->interpretedFunction, "type_function");
    }
}

void
MarkTypeObject(JSTracer *trc, const MarkablePtr<types::TypeObject> &type, const char *name)
{
    MarkTypeObjectUnbarriered(trc, type.value, name);
}

#if JS_HAS_XML_SUPPORT
void
MarkXMLUnbarriered(JSTracer *trc, JSXML *xml, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(xml);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, xml);
}

void
MarkXML(JSTracer *trc, const MarkablePtr<JSXML> &xml, const char *name)
{
    MarkXMLUnbarriered(trc, xml.value, name);
}
#endif

#define JS_SAME_COMPARTMENT_ASSERT(thing1, thing2)                      \
    JS_ASSERT((thing1)->compartment() == (thing2)->compartment())

#define JS_COMPARTMENT_ASSERT(rt, thing)                                \
    JS_ASSERT_IF((rt)->gcCurrentCompartment,                            \
                 (thing)->compartment() == (rt)->gcCurrentCompartment);

#define JS_COMPARTMENT_ASSERT_STR(rt, thing)                            \
    JS_ASSERT_IF((rt)->gcCurrentCompartment,                            \
                 (thing)->compartment() == (rt)->gcCurrentCompartment || \
                 (thing)->compartment() == (rt)->atomsCompartment);

void
PushMarkStack(GCMarker *gcmarker, JSXML *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushXML(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSObject *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, types::TypeObject *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushType(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSScript *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    /*
     * We mark scripts directly rather than pushing on the stack as they can
     * refer to other scripts only indirectly (like via nested functions) and
     * we cannot get to deep recursion.
     */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        MarkChildren(gcmarker, thing);
}

static void
ScanShape(GCMarker *gcmarker, const Shape *shape);

void
PushMarkStack(GCMarker *gcmarker, const Shape *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    /* We mark shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanShape(gcmarker, thing);
}

static inline void
ScanBaseShape(GCMarker *gcmarker, BaseShape *base);

void
PushMarkStack(GCMarker *gcmarker, BaseShape *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);

    /* We mark base shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanBaseShape(gcmarker, thing);
}

static void
MarkAtomRange(JSTracer *trc, size_t len, JSAtom **vec, const char *name)
{
    for (uint32_t i = 0; i < len; i++) {
        if (JSAtom *atom = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, atom);
        }
    }
}

void
MarkObjectRange(JSTracer *trc, size_t len, HeapPtr<JSObject> *vec, const char *name)
{
    for (uint32_t i = 0; i < len; i++) {
        if (JSObject *obj = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, obj);
        }
    }
}

void
MarkXMLRange(JSTracer *trc, size_t len, HeapPtr<JSXML> *vec, const char *name)
{
    for (size_t i = 0; i < len; i++) {
        if (JSXML *xml = vec[i]) {
            JS_SET_TRACING_INDEX(trc, "xml_vector", i);
            Mark(trc, xml);
        }
    }
}

void
MarkIdUnbarriered(JSTracer *trc, jsid id)
{
    if (JSID_IS_STRING(id))
        Mark(trc, JSID_TO_STRING(id));
    else if (JS_UNLIKELY(JSID_IS_OBJECT(id)))
        Mark(trc, JSID_TO_OBJECT(id));
}

void
MarkIdUnbarriered(JSTracer *trc, jsid id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkIdUnbarriered(trc, id);
}

void
MarkId(JSTracer *trc, const HeapId &id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkIdUnbarriered(trc, id.get(), name);
}

void
MarkIdRangeUnbarriered(JSTracer *trc, jsid *beg, jsid *end, const char *name)
{
    for (jsid *idp = beg; idp != end; ++idp) {
        JS_SET_TRACING_INDEX(trc, name, (idp - beg));
        MarkIdUnbarriered(trc, *idp);
    }
}

void
MarkIdRangeUnbarriered(JSTracer *trc, size_t len, jsid *vec, const char *name)
{
    MarkIdRangeUnbarriered(trc, vec, vec + len, name);
}

void
MarkIdRange(JSTracer *trc, HeapId *beg, HeapId *end, const char *name)
{
    for (HeapId *idp = beg; idp != end; ++idp) {
        JS_SET_TRACING_INDEX(trc, name, (idp - beg));
        MarkIdUnbarriered(trc, *idp);
    }
}

void
MarkKind(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(thing);
    JS_ASSERT(kind == GetGCThingTraceKind(thing));
    switch (kind) {
      case JSTRACE_OBJECT:
        Mark(trc, reinterpret_cast<JSObject *>(thing));
        break;
      case JSTRACE_STRING:
        Mark(trc, reinterpret_cast<JSString *>(thing));
        break;
      case JSTRACE_SCRIPT:
        Mark(trc, static_cast<JSScript *>(thing));
        break;
      case JSTRACE_SHAPE:
        Mark(trc, reinterpret_cast<Shape *>(thing));
        break;
      case JSTRACE_BASE_SHAPE:
        Mark(trc, reinterpret_cast<BaseShape *>(thing));
        break;
      case JSTRACE_TYPE_OBJECT:
        MarkTypeObjectUnbarriered(trc, reinterpret_cast<types::TypeObject *>(thing), "type_stack");
        break;
#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        Mark(trc, static_cast<JSXML *>(thing));
        break;
#endif
    }
}

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
void
MarkValueRaw(JSTracer *trc, const js::Value &v)
{
    if (v.isMarkable()) {
        JS_ASSERT(v.toGCThing());
        return MarkKind(trc, v.toGCThing(), v.gcKind());
    }
}

void
MarkValueUnbarriered(JSTracer *trc, const js::Value &v, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueRaw(trc, v);
}

void
MarkValue(JSTracer *trc, const js::HeapValue &v, const char *name)
{
    MarkValueUnbarriered(trc, v, name);
}

void
MarkCrossCompartmentValue(JSTracer *trc, const js::HeapValue &v, const char *name)
{
    if (v.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)v.toGCThing();
        JSRuntime *rt = trc->runtime;
        if (rt->gcCurrentCompartment && cell->compartment() != rt->gcCurrentCompartment)
            return;

        MarkValue(trc, v, name);
    }
}

void
MarkValueRange(JSTracer *trc, const HeapValue *beg, const HeapValue *end, const char *name)
{
    for (const HeapValue *vp = beg; vp < end; ++vp) {
        JS_SET_TRACING_INDEX(trc, name, vp - beg);
        MarkValueRaw(trc, vp->get());
    }
}

void
MarkValueRange(JSTracer *trc, size_t len, const HeapValue *vec, const char *name)
{
    MarkValueRange(trc, vec, vec + len, name);
}

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
void
MarkGCThing(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    if (!thing)
        return;

    MarkKind(trc, thing, kind);
}

void
MarkGCThing(JSTracer *trc, void *thing)
{
    if (!thing)
        return;
    MarkKind(trc, thing, GetGCThingTraceKind(thing));
}

void
MarkGCThing(JSTracer *trc, void *thing, const char *name, size_t index)
{
    JS_SET_TRACING_INDEX(trc, name, index);
    MarkGCThing(trc, thing);
}

void
Mark(JSTracer *trc, void *thing, JSGCTraceKind kind, const char *name)
{
    JS_ASSERT(thing);
    JS_SET_TRACING_NAME(trc, name);
    MarkKind(trc, thing, kind);
}

void
MarkRoot(JSTracer *trc, JSObject *thing, const char *name)
{
    MarkObjectUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, JSString *thing, const char *name)
{
    MarkStringUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, JSScript *thing, const char *name)
{
    MarkScriptUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, const Shape *thing, const char *name)
{
    MarkShapeUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, types::TypeObject *thing, const char *name)
{
    MarkTypeObjectUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, JSXML *thing, const char *name)
{
    MarkXMLUnbarriered(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, const Value &v, const char *name)
{
    MarkValueUnbarriered(trc, v, name);
}

void
MarkRoot(JSTracer *trc, jsid id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkIdUnbarriered(trc, id);
}

void
MarkRootGCThing(JSTracer *trc, void *thing, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkGCThing(trc, thing);
}

void
MarkRootRange(JSTracer *trc, size_t len, const Shape **vec, const char *name)
{
    const Shape **end = vec + len;
    for (const Shape **sp = vec; sp < end; ++sp) {
        JS_SET_TRACING_INDEX(trc, name, sp - vec);
        MarkShapeUnbarriered(trc, *sp, name);
    }
}

void
MarkRootRange(JSTracer *trc, size_t len, JSObject **vec, const char *name)
{
    JSObject **end = vec + len;
    for (JSObject **sp = vec; sp < end; ++sp) {
        JS_SET_TRACING_INDEX(trc, name, sp - vec);
        MarkObjectUnbarriered(trc, *sp, name);
    }
}

void
MarkRootRange(JSTracer *trc, const Value *beg, const Value *end, const char *name)
{
    for (const Value *vp = beg; vp < end; ++vp) {
        JS_SET_TRACING_INDEX(trc, name, vp - beg);
        MarkValueRaw(trc, *vp);
    }
}

void
MarkRootRange(JSTracer *trc, size_t len, const Value *vec, const char *name)
{
    MarkRootRange(trc, vec, vec + len, name);
}

void
MarkRootRange(JSTracer *trc, jsid *beg, jsid *end, const char *name)
{
    MarkIdRangeUnbarriered(trc, beg, end, name);
}

void
MarkRootRange(JSTracer *trc, size_t len, jsid *vec, const char *name)
{
    MarkIdRangeUnbarriered(trc, len, vec, name);
}

static void
ScanShape(GCMarker *gcmarker, const Shape *shape)
{
  restart:
    PushMarkStack(gcmarker, shape->base());

    jsid id = shape->maybePropid();
    if (JSID_IS_STRING(id))
        PushMarkStack(gcmarker, JSID_TO_STRING(id));
    else if (JS_UNLIKELY(JSID_IS_OBJECT(id)))
        PushMarkStack(gcmarker, JSID_TO_OBJECT(id));

    shape = shape->previous();
    if (shape && shape->markIfUnmarked(gcmarker->getMarkColor()))
        goto restart;
}

static inline void
ScanBaseShape(GCMarker *gcmarker, BaseShape *base)
{
    for (;;) {
        if (base->hasGetterObject())
            PushMarkStack(gcmarker, base->getterObject());

        if (base->hasSetterObject())
            PushMarkStack(gcmarker, base->setterObject());

        if (JSObject *parent = base->getObjectParent())
            PushMarkStack(gcmarker, parent);

        if (base->isOwned()) {
            /*
             * Make sure that ScanBaseShape is not recursive so its inlining
             * is possible.
             */
            UnownedBaseShape *unowned = base->baseUnowned();
            JS_SAME_COMPARTMENT_ASSERT(base, unowned);
            if (unowned->markIfUnmarked(gcmarker->getMarkColor())) {
                base = unowned;
                continue;
            }
        }
        break;
    }
}

static inline void
ScanLinearString(GCMarker *gcmarker, JSLinearString *str)
{
    JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);
    JS_ASSERT(str->isMarked());

    /*
     * Add extra asserts to confirm the static type to detect incorrect string
     * mutations.
     */
    JS_ASSERT(str->JSString::isLinear());
    while (str->isDependent()) {
        str = str->asDependent().base();
        JS_ASSERT(str->JSString::isLinear());
        JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);
        if (!str->markIfUnmarked())
            break;
    }
}

/*
 * The function tries to scan the whole rope tree using the marking stack as
 * temporary storage. If that becomes full, the unscanned ropes are added to
 * the delayed marking list. When the function returns, the marking stack is
 * at the same depth as it was on entry. This way we avoid using tags when
 * pushing ropes to the stack as ropes never leaks to other users of the
 * stack. This also assumes that a rope can only point to other ropes or
 * linear strings, it cannot refer to GC things of other types.
 */
static void
ScanRope(GCMarker *gcmarker, JSRope *rope)
{
    uintptr_t *savedTos = gcmarker->stack.tos;
    for (;;) {
        JS_ASSERT(GetGCThingTraceKind(rope) == JSTRACE_STRING);
        JS_ASSERT(rope->JSString::isRope());
        JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, rope);
        JS_ASSERT(rope->isMarked());
        JSRope *next = NULL;

        JSString *right = rope->rightChild();
        if (right->markIfUnmarked()) {
            if (right->isLinear())
                ScanLinearString(gcmarker, &right->asLinear());
            else
                next = &right->asRope();
        }

        JSString *left = rope->leftChild();
        if (left->markIfUnmarked()) {
            if (left->isLinear()) {
                ScanLinearString(gcmarker, &left->asLinear());
            } else {
                /*
                 * When both children are ropes, set aside the right one to
                 * scan it later.
                 */
                if (next && !gcmarker->stack.push(reinterpret_cast<uintptr_t>(next)))
                    gcmarker->delayMarkingChildren(next);
                next = &left->asRope();
            }
        }
        if (next) {
            rope = next;
        } else if (savedTos != gcmarker->stack.tos) {
            JS_ASSERT(savedTos < gcmarker->stack.tos);
            rope = reinterpret_cast<JSRope *>(gcmarker->stack.pop());
        } else {
            break;
        }
    }
    JS_ASSERT(savedTos == gcmarker->stack.tos);
 }

static inline void
ScanString(GCMarker *gcmarker, JSString *str)
{
    if (str->isLinear())
        ScanLinearString(gcmarker, &str->asLinear());
    else
        ScanRope(gcmarker, &str->asRope());
}

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *str)
{
    JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);

    /*
     * As string can only refer to other strings we fully scan its GC graph
     * using the explicit stack when navigating the rope tree to avoid
     * dealing with strings on the stack in drainMarkStack.
     */
    if (str->markIfUnmarked())
        ScanString(gcmarker, str);
}

static inline void
PushValueArray(GCMarker *gcmarker, JSObject* obj, HeapValue *start, HeapValue *end)
{
    JS_ASSERT(start <= end);
    uintptr_t tagged = reinterpret_cast<uintptr_t>(obj) | GCMarker::ValueArrayTag;
    uintptr_t startAddr = reinterpret_cast<uintptr_t>(start);
    uintptr_t endAddr = reinterpret_cast<uintptr_t>(end);

    /* Push in the reverse order so obj will be on top. */
    if (!gcmarker->stack.push(endAddr, startAddr, tagged)) {
        /*
         * If we cannot push the array, we trigger delay marking for the whole
         * object.
         */
        gcmarker->delayMarkingChildren(obj);
    }
}

void
MarkChildren(JSTracer *trc, JSObject *obj)
{
    MarkTypeObject(trc, obj->typeFromGC(), "type");

    Shape *shape = obj->lastProperty();
    MarkShapeUnbarriered(trc, shape, "shape");

    Class *clasp = shape->getObjectClass();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (shape->isNative()) {
        uint32_t nslots = obj->slotSpan();
        for (uint32_t i = 0; i < nslots; i++) {
            JS_SET_TRACING_DETAILS(trc, js_PrintObjectSlotName, obj, i);
            MarkValueRaw(trc, obj->nativeGetSlot(i));
        }
    }
}

void
MarkChildren(JSTracer *trc, JSString *str)
{
    /*
     * We use custom barriers in JSString, so it's safe to use unbarriered
     * marking here.
     */
    if (str->isDependent()) {
        MarkStringUnbarriered(trc, str->asDependent().base(), "base");
    } else if (str->isRope()) {
        JSRope &rope = str->asRope();
        MarkStringUnbarriered(trc, rope.leftChild(), "left child");
        MarkStringUnbarriered(trc, rope.rightChild(), "right child");
    }
}


void
MarkChildren(JSTracer *trc, JSScript *script)
{
    CheckScript(script, NULL);

    JS_ASSERT_IF(trc->runtime->gcCheckCompartment,
                 script->compartment() == trc->runtime->gcCheckCompartment);

    MarkAtomRange(trc, script->natoms, script->atoms, "atoms");

    if (JSScript::isValidOffset(script->objectsOffset)) {
        JSObjectArray *objarray = script->objects();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (JSScript::isValidOffset(script->regexpsOffset)) {
        JSObjectArray *objarray = script->regexps();
        MarkObjectRange(trc, objarray->length, objarray->vector, "objects");
    }

    if (JSScript::isValidOffset(script->constOffset)) {
        JSConstArray *constarray = script->consts();
        MarkValueRange(trc, constarray->length, constarray->vector, "consts");
    }

    if (script->function())
        MarkObjectUnbarriered(trc, script->function(), "function");

    if (!script->isCachedEval && script->globalObject)
        MarkObject(trc, script->globalObject, "object");

    if (IS_GC_MARKING_TRACER(trc) && script->filename)
        js_MarkScriptFilename(script->filename);

    script->bindings.trace(trc);

    if (script->types)
        script->types->trace(trc);

    if (script->hasAnyBreakpointsOrStepMode())
        script->markTrapClosures(trc);
}

void
MarkChildren(JSTracer *trc, const Shape *shape)
{
    MarkBaseShapeUnbarriered(trc, shape->base(), "base");
    MarkIdUnbarriered(trc, shape->maybePropid(), "propid");
    if (shape->previous())
        MarkShape(trc, shape->previous(), "parent");
}

inline void
MarkBaseShapeGetterSetter(JSTracer *trc, BaseShape *base)
{
    if (base->hasGetterObject())
        MarkObjectUnbarriered(trc, base->getterObject(), "getter");
    if (base->hasSetterObject())
        MarkObjectUnbarriered(trc, base->setterObject(), "setter");
}

void
MarkChildren(JSTracer *trc, BaseShape *base)
{
    MarkBaseShapeGetterSetter(trc, base);
    if (base->isOwned())
        MarkBaseShapeUnbarriered(trc, base->baseUnowned(), "base");

    if (JSObject *parent = base->getObjectParent())
        MarkObjectUnbarriered(trc, parent, "parent");
}

/*
 * This function is used by the cycle collector to trace through the
 * children of a BaseShape (and its baseUnowned(), if any). The cycle
 * collector does not directly care about BaseShapes, so only the
 * getter, setter, and parent are marked. Furthermore, the parent is
 * marked only if it isn't the same as prevParent, which will be
 * updated to the current shape's parent.
 */
inline void
MarkCycleCollectorChildren(JSTracer *trc, BaseShape *base, JSObject **prevParent)
{
    JS_ASSERT(base);

    MarkBaseShapeGetterSetter(trc, base);

    JSObject *parent = base->getObjectParent();
    if (parent && parent != *prevParent) {
        MarkObjectUnbarriered(trc, parent, "parent");
        *prevParent = parent;
    }

    // An owned base shape has the same parent, getter and setter as
    // its baseUnowned().
#ifdef DEBUG
    if (base->isOwned()) {
        UnownedBaseShape *unowned = base->baseUnowned();
        JS_ASSERT_IF(base->hasGetterObject(), base->getterObject() == unowned->getterObject());
        JS_ASSERT_IF(base->hasSetterObject(), base->setterObject() == unowned->setterObject());
        JS_ASSERT(base->getObjectParent() == unowned->getObjectParent());
    }
#endif
}

/*
 * This function is used by the cycle collector to trace through a
 * shape. The cycle collector does not care about shapes or base
 * shapes, so those are not marked. Instead, any shapes or base shapes
 * that are encountered have their children marked. Stack space is
 * bounded. If two shapes in a row have the same parent pointer, the
 * parent pointer will only be marked once.
 */
void
MarkCycleCollectorChildren(JSTracer *trc, const Shape *shape)
{
    JSObject *prevParent = NULL;
    do {
        MarkCycleCollectorChildren(trc, shape->base(), &prevParent);
        MarkIdUnbarriered(trc, shape->maybePropid(), "propid");
        shape = shape->previous();
    } while (shape);
}

static void
ScanTypeObject(GCMarker *gcmarker, types::TypeObject *type)
{
    if (!type->singleton) {
        unsigned count = type->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            types::Property *prop = type->getProperty(i);
            if (prop && JSID_IS_STRING(prop->id))
                PushMarkStack(gcmarker, JSID_TO_STRING(prop->id));
        }
    }

    if (type->proto)
        PushMarkStack(gcmarker, type->proto);

    if (type->newScript) {
        PushMarkStack(gcmarker, type->newScript->fun);
        PushMarkStack(gcmarker, type->newScript->shape);
    }

    if (type->interpretedFunction)
        PushMarkStack(gcmarker, type->interpretedFunction);

    /*
     * Don't need to trace singleton, an object with this type must have
     * already been traced and it will also hold a reference on the script
     * (singleton and functionScript types cannot be the newType of another
     * object). Attempts to mark type objects directly must use MarkTypeObject,
     * which will itself mark these extra bits.
     */
}

void
MarkChildren(JSTracer *trc, types::TypeObject *type)
{
    if (!type->singleton) {
        unsigned count = type->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            types::Property *prop = type->getProperty(i);
            if (prop)
                MarkId(trc, prop->id, "type_prop");
        }
    }

    if (type->proto)
        MarkObject(trc, type->proto, "type_proto");

    if (type->singleton && !type->lazy())
        MarkObject(trc, type->singleton, "type_singleton");

    if (type->newScript) {
        MarkObject(trc, type->newScript->fun, "type_new_function");
        MarkShape(trc, type->newScript->shape, "type_new_shape");
    }

    if (type->interpretedFunction)
        MarkObject(trc, type->interpretedFunction, "type_function");
}

#ifdef JS_HAS_XML_SUPPORT
void
MarkChildren(JSTracer *trc, JSXML *xml)
{
    js_TraceXML(trc, xml);
}
#endif

} /* namespace gc */

inline void
GCMarker::processMarkStackTop()
{
    /*
     * The function uses explicit goto and implements the scanning of the
     * object directly. It allows to eliminate the tail recursion and
     * significantly improve the marking performance, see bug 641025.
     */
    HeapValue *vp, *end;
    JSObject *obj;

    uintptr_t addr = stack.pop();
    uintptr_t tag = addr & StackTagMask;
    addr &= ~StackTagMask;

    if (tag == ValueArrayTag) {
        JS_STATIC_ASSERT(ValueArrayTag == 0);
        JS_ASSERT(!(addr & Cell::CellMask));
        obj = reinterpret_cast<JSObject *>(addr);
        uintptr_t addr2 = stack.pop();
        uintptr_t addr3 = stack.pop();
        JS_ASSERT(addr2 <= addr3);
        JS_ASSERT((addr3 - addr2) % sizeof(Value) == 0);
        vp = reinterpret_cast<HeapValue *>(addr2);
        end = reinterpret_cast<HeapValue *>(addr3);
        goto scan_value_array;
    }

    if (tag == ObjectTag) {
        obj = reinterpret_cast<JSObject *>(addr);
        goto scan_obj;
    }

    if (tag == TypeTag) {
        ScanTypeObject(this, reinterpret_cast<types::TypeObject *>(addr));
    } else {
        JS_ASSERT(tag == XmlTag);
        MarkChildren(this, reinterpret_cast<JSXML *>(addr));
    }
    return;

  scan_value_array:
    JS_ASSERT(vp <= end);
    while (vp != end) {
        const Value &v = *vp++;
        if (v.isString()) {
            JSString *str = v.toString();
            if (str->markIfUnmarked())
                ScanString(this, str);
        } else if (v.isObject()) {
            JSObject *obj2 = &v.toObject();
            if (obj2->markIfUnmarked(getMarkColor())) {
                PushValueArray(this, obj, vp, end);
                obj = obj2;
                goto scan_obj;
            }
        }
    }
    return;

  scan_obj:
    {
        types::TypeObject *type = obj->typeFromGC();
        PushMarkStack(this, type);

        js::Shape *shape = obj->lastProperty();
        PushMarkStack(this, shape);

        /* Call the trace hook if necessary. */
        Class *clasp = shape->getObjectClass();
        if (clasp->trace) {
            if (clasp == &ArrayClass) {
                JS_ASSERT(!shape->isNative());
                vp = obj->getDenseArrayElements();
                end = vp + obj->getDenseArrayInitializedLength();
                goto scan_value_array;
            }
            clasp->trace(this, obj);
        }

        if (!shape->isNative())
            return;

        unsigned nslots = obj->slotSpan();
        vp = obj->fixedSlots();
        if (obj->slots) {
            unsigned nfixed = obj->numFixedSlots();
            if (nslots > nfixed) {
                PushValueArray(this, obj, vp, vp + nfixed);
                vp = obj->slots;
                end = vp + (nslots - nfixed);
                goto scan_value_array;
            }
        }
        JS_ASSERT(nslots <= obj->numFixedSlots());
        end = vp + nslots;
        goto scan_value_array;
    }
}

void
GCMarker::drainMarkStack()
{
    JSRuntime *rt = runtime;
    rt->gcCheckCompartment = rt->gcCurrentCompartment;

    for (;;) {
        while (!stack.isEmpty())
            processMarkStackTop();
        if (!hasDelayedChildren())
            break;

        /*
         * Mark children of things that caused too deep recursion during the
         * above tracing. Don't do this until we're done with everything
         * else.
         */
        markDelayedChildren();
    }

    rt->gcCheckCompartment = NULL;
}

void
TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    switch (kind) {
      case JSTRACE_OBJECT:
        MarkChildren(trc, static_cast<JSObject *>(thing));
        break;

      case JSTRACE_STRING:
        MarkChildren(trc, static_cast<JSString *>(thing));
        break;

      case JSTRACE_SCRIPT:
        MarkChildren(trc, static_cast<JSScript *>(thing));
        break;

      case JSTRACE_SHAPE:
        MarkChildren(trc, static_cast<Shape *>(thing));
        break;

      case JSTRACE_BASE_SHAPE:
        MarkChildren(trc, static_cast<BaseShape *>(thing));
        break;

      case JSTRACE_TYPE_OBJECT:
        MarkChildren(trc, (types::TypeObject *)thing);
        break;

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        MarkChildren(trc, static_cast<JSXML *>(thing));
        break;
#endif
    }
}

void
CallTracer(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(thing);
    MarkKind(trc, thing, kind);
}

} /* namespace js */
