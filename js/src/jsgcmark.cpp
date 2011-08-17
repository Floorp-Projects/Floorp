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

/*
 * There are two mostly separate mark paths. The first is a fast path used
 * internally in the GC. The second is a slow path used for root marking and
 * for API consumers like the cycle collector.
 *
 * The fast path uses explicit stacks. The basic marking process during a GC is
 * that all roots are pushed on to a mark stack, and then each item on the
 * stack is scanned (possibly pushing more stuff) until the stack is empty.
 *
 * PushMarkStack pushes a GC thing onto the mark stack. In some cases (shapes
 * or strings) it eagerly marks the object rather than pushing it. Popping is
 * done by the drainMarkStack method. For each thing it pops, drainMarkStack
 * calls ScanObject (or a related function).
 *
 * Most of the marking code outside jsgcmark uses functions like MarkObject,
 * MarkString, etc. These functions check if an object is in the compartment
 * currently being GCed. If it is, they call PushMarkStack. Roots are pushed
 * this way as well as pointers traversed inside trace hooks (for things like
 * js_IteratorClass). It it always valid to call a MarkX function instead of
 * PushMarkStack, although it may be slower.
 *
 * The MarkX functions also handle non-GC object traversal. In this case, they
 * call a callback for each object visited. This is a recursive process; the
 * mark stacks are not involved. These callbacks may ask for the outgoing
 * pointers to be visited. Eventually, this leads to the MarkChildren functions
 * being called. These functions duplicate much of the functionality of
 * ScanObject, but they don't push onto an explicit stack.
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
PushMarkStack(GCMarker *gcmarker, const Shape *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSShortString *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *thing);

template<typename T>
static inline void
CheckMarkedThing(JSTracer *trc, T *thing)
{
    JS_ASSERT(thing);
    JS_ASSERT(JS_IS_VALID_TRACE_KIND(GetGCThingTraceKind(thing)));
    JS_ASSERT(trc->debugPrinter || trc->debugPrintArg);
    JS_ASSERT_IF(trc->context->runtime->gcCurrentCompartment, IS_GC_MARKING_TRACER(trc));

    JS_ASSERT(!JSAtom::isStatic(thing));
    JS_ASSERT(thing->isAligned());

    JS_ASSERT(thing->compartment());
    JS_ASSERT(thing->compartment()->rt == trc->context->runtime);
}

template<typename T>
void
Mark(JSTracer *trc, T *thing)
{
    CheckMarkedThing(trc, thing);

    JSRuntime *rt = trc->context->runtime;

    JS_OPT_ASSERT_IF(rt->gcCheckCompartment,
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

/*
 * Alternative to Mark() which can be used when the thing is known to be in the
 * correct compartment (if we are in a per-compartment GC) and which is inline.
 */
template<typename T>
inline void
InlineMark(JSTracer *trc, T *thing, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);

    CheckMarkedThing(trc, thing);

    if (IS_GC_MARKING_TRACER(trc))
        PushMarkStack(static_cast<GCMarker *>(trc), thing);
    else
        trc->callback(trc, (void *)thing, GetGCThingTraceKind(thing));

#ifdef DEBUG
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
#endif
}

inline void
InlineMarkId(JSTracer *trc, jsid id, const char *name)
{
    if (JSID_IS_STRING(id)) {
        JSString *str = JSID_TO_STRING(id);
        if (!str->isStaticAtom())
            InlineMark(trc, str, name);
    } else if (JS_UNLIKELY(JSID_IS_OBJECT(id))) {
        InlineMark(trc, JSID_TO_OBJECT(id), name);
    }
}

void
MarkString(JSTracer *trc, JSString *str)
{
    JS_ASSERT(str);
    if (str->isStaticAtom())
        return;
    Mark(trc, str);
}

void
MarkString(JSTracer *trc, JSString *str, const char *name)
{
    JS_ASSERT(str);
    JS_SET_TRACING_NAME(trc, name);
    MarkString(trc, str);
}

void
MarkObject(JSTracer *trc, JSObject &obj, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(&obj);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, &obj);
}

void
MarkCrossCompartmentObject(JSTracer *trc, JSObject &obj, const char *name)
{
    JSRuntime *rt = trc->context->runtime;
    if (rt->gcCurrentCompartment && rt->gcCurrentCompartment != obj.compartment())
        return;

    MarkObject(trc, obj, name);
}

void
MarkObjectWithPrinter(JSTracer *trc, JSObject &obj, JSTraceNamePrinter printer,
		      const void *arg, size_t index)
{
    JS_ASSERT(trc);
    JS_ASSERT(&obj);
    JS_SET_TRACING_DETAILS(trc, printer, arg, index);
    Mark(trc, &obj);
}

void
MarkShape(JSTracer *trc, const Shape *shape, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(shape);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, shape);
}

#if JS_HAS_XML_SUPPORT
void
MarkXML(JSTracer *trc, JSXML *xml, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(xml);
    JS_SET_TRACING_NAME(trc, name);
    Mark(trc, xml);
}
#endif

void
PushMarkStack(GCMarker *gcmarker, JSXML *thing)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->context->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushXML(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSObject *thing)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->context->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->context->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSShortString *thing)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->context->runtime->gcCurrentCompartment);

    (void) thing->markIfUnmarked(gcmarker->getMarkColor());
}

static void
ScanShape(GCMarker *gcmarker, const Shape *shape);

void
PushMarkStack(GCMarker *gcmarker, const Shape *thing)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->context->runtime->gcCurrentCompartment);

    /* We mark shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanShape(gcmarker, thing);
}

void
MarkAtomRange(JSTracer *trc, size_t len, JSAtom **vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSAtom *atom = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            if (!atom->isStaticAtom())
                Mark(trc, atom);
        }
    }
}

void
MarkObjectRange(JSTracer *trc, size_t len, JSObject **vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSObject *obj = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, obj);
        }
    }
}

void
MarkXMLRange(JSTracer *trc, size_t len, JSXML **vec, const char *name)
{
    for (size_t i = 0; i < len; i++) {
        if (JSXML *xml = vec[i]) {
            JS_SET_TRACING_INDEX(trc, "xml_vector", i);
            Mark(trc, xml);
        }
    }
}

void
MarkId(JSTracer *trc, jsid id)
{
    if (JSID_IS_STRING(id)) {
        JSString *str = JSID_TO_STRING(id);
        if (!str->isStaticAtom())
            Mark(trc, str);
    } else if (JS_UNLIKELY(JSID_IS_OBJECT(id))) {
        Mark(trc, JSID_TO_OBJECT(id));
    }
}

void
MarkId(JSTracer *trc, jsid id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkId(trc, id);
}

void
MarkIdRange(JSTracer *trc, jsid *beg, jsid *end, const char *name)
{
    for (jsid *idp = beg; idp != end; ++idp) {
        JS_SET_TRACING_INDEX(trc, name, (idp - beg));
        MarkId(trc, *idp);
    }
}

void
MarkIdRange(JSTracer *trc, size_t len, jsid *vec, const char *name)
{
    MarkIdRange(trc, vec, vec + len, name);
}

void
MarkKind(JSTracer *trc, void *thing, uint32 kind)
{
    JS_ASSERT(thing);
    JS_ASSERT(kind == GetGCThingTraceKind(thing));
    switch (kind) {
        case JSTRACE_OBJECT:
            Mark(trc, reinterpret_cast<JSObject *>(thing));
            break;
        case JSTRACE_STRING:
            MarkString(trc, reinterpret_cast<JSString *>(thing));
            break;
        case JSTRACE_SHAPE:
            Mark(trc, reinterpret_cast<Shape *>(thing));
            break;
#if JS_HAS_XML_SUPPORT
        case JSTRACE_XML:
            Mark(trc, reinterpret_cast<JSXML *>(thing));
            break;
#endif
        default:
            JS_ASSERT(false);
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
MarkValue(JSTracer *trc, const js::Value &v, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueRaw(trc, v);
}

void
MarkCrossCompartmentValue(JSTracer *trc, const js::Value &v, const char *name)
{
    if (v.isMarkable()) {
        js::gc::Cell *cell = (js::gc::Cell *)v.toGCThing();
        unsigned kind = v.gcKind();
        if (kind == JSTRACE_STRING && ((JSString *)cell)->isStaticAtom())
            return;
        JSRuntime *rt = trc->context->runtime;
        if (rt->gcCurrentCompartment && cell->compartment() != rt->gcCurrentCompartment)
            return;

        MarkValue(trc, v, name);
    }
}

void
MarkValueRange(JSTracer *trc, const Value *beg, const Value *end, const char *name)
{
    for (const Value *vp = beg; vp < end; ++vp) {
        JS_SET_TRACING_INDEX(trc, name, vp - beg);
        MarkValueRaw(trc, *vp);
    }
}

void
MarkValueRange(JSTracer *trc, size_t len, const Value *vec, const char *name)
{
    MarkValueRange(trc, vec, vec + len, name);
}

void
MarkShapeRange(JSTracer *trc, const Shape **beg, const Shape **end, const char *name)
{
    for (const Shape **sp = beg; sp < end; ++sp) {
        JS_SET_TRACING_INDEX(trc, name, sp - beg);
        MarkShape(trc, *sp, name);
    }
}

void
MarkShapeRange(JSTracer *trc, size_t len, const Shape **vec, const char *name)
{
    MarkShapeRange(trc, vec, vec + len, name);
}

/* N.B. Assumes JS_SET_TRACING_NAME/INDEX has already been called. */
void
MarkGCThing(JSTracer *trc, void *thing, uint32 kind)
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
MarkGCThing(JSTracer *trc, void *thing, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkGCThing(trc, thing);
}

void
MarkGCThing(JSTracer *trc, void *thing, const char *name, size_t index)
{
    JS_SET_TRACING_INDEX(trc, name, index);
    MarkGCThing(trc, thing);
}

void
Mark(JSTracer *trc, void *thing, uint32 kind, const char *name)
{
    JS_ASSERT(thing);
    JS_SET_TRACING_NAME(trc, name);
    MarkKind(trc, thing, kind);
}

void
MarkRoot(JSTracer *trc, JSObject *thing, const char *name)
{
    MarkObject(trc, *thing, name);
}

void
MarkRoot(JSTracer *trc, JSString *thing, const char *name)
{
    MarkString(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, const Shape *thing, const char *name)
{
    MarkShape(trc, thing, name);
}

void
MarkRoot(JSTracer *trc, types::TypeObject *thing, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(thing);
    JS_SET_TRACING_NAME(trc, name);
    JSRuntime *rt = trc->context->runtime;
    if (!rt->gcCurrentCompartment || thing->compartment() == rt->gcCurrentCompartment)
        thing->trace(trc, false);
}

void
MarkRoot(JSTracer *trc, JSXML *thing, const char *name)
{
    MarkXML(trc, thing, name);
}

static void
PrintPropertyGetterOrSetter(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == PrintPropertyGetterOrSetter);
    Shape *shape = (Shape *)trc->debugPrintArg;
    jsid propid = shape->propid;
    JS_ASSERT(!JSID_IS_VOID(propid));
    const char *name = trc->debugPrintIndex ? js_setter_str : js_getter_str;

    if (JSID_IS_ATOM(propid)) {
        size_t n = PutEscapedString(buf, bufsize, JSID_TO_ATOM(propid), 0);
        if (n < bufsize)
            JS_snprintf(buf + n, bufsize - n, " %s", name);
    } else if (JSID_IS_INT(shape->propid)) {
        JS_snprintf(buf, bufsize, "%d %s", JSID_TO_INT(propid), name);
    } else {
        JS_snprintf(buf, bufsize, "<object> %s", name);
    }
}

static void
PrintPropertyMethod(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == PrintPropertyMethod);
    Shape *shape = (Shape *)trc->debugPrintArg;
    jsid propid = shape->propid;
    JS_ASSERT(!JSID_IS_VOID(propid));

    JS_ASSERT(JSID_IS_ATOM(propid));
    size_t n = PutEscapedString(buf, bufsize, JSID_TO_ATOM(propid), 0);
    if (n < bufsize)
        JS_snprintf(buf + n, bufsize - n, " method");
}

static inline void
ScanValue(GCMarker *gcmarker, const Value &v)
{
    if (v.isMarkable()) {
        switch (v.gcKind()) {
          case JSTRACE_STRING: {
            JSString *str = (JSString *)v.toGCThing();
            if (!str->isStaticAtom())
                PushMarkStack(gcmarker, str);
            break;
          }
          case JSTRACE_OBJECT:
            PushMarkStack(gcmarker, (JSObject *)v.toGCThing());
            break;
          case JSTRACE_XML:
            PushMarkStack(gcmarker, (JSXML *)v.toGCThing());
            break;
        }
    }
}

static void
ScanShape(GCMarker *gcmarker, const Shape *shape)
{
restart:
    JSRuntime *rt = gcmarker->context->runtime;
    if (rt->gcRegenShapes)
        shape->shapeid = js_RegenerateShapeForGC(rt);

    if (JSID_IS_STRING(shape->propid)) {
        JSString *str = JSID_TO_STRING(shape->propid);
        if (!str->isStaticAtom())
            PushMarkStack(gcmarker, str);
    } else if (JS_UNLIKELY(JSID_IS_OBJECT(shape->propid))) {
        PushMarkStack(gcmarker, JSID_TO_OBJECT(shape->propid));
    }

    if (shape->hasGetterValue() && shape->getter())
        PushMarkStack(gcmarker, shape->getterObject());
    if (shape->hasSetterValue() && shape->setter())
        PushMarkStack(gcmarker, shape->setterObject());

    if (shape->isMethod())
        PushMarkStack(gcmarker, &shape->methodObject());

    shape = shape->previous();
    if (shape && shape->markIfUnmarked(gcmarker->getMarkColor()))
        goto restart;
}

static inline void
ScanRope(GCMarker *gcmarker, JSRope *rope)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 rope->compartment() == gcmarker->context->runtime->gcCurrentCompartment
                 || rope->compartment() == gcmarker->context->runtime->atomsCompartment);
    JS_ASSERT(rope->isMarked());

    JSString *leftChild = NULL;
    do {
        JSString *rightChild = rope->rightChild();

        if (rightChild->isRope()) {
            if (rightChild->markIfUnmarked())
                gcmarker->pushRope(&rightChild->asRope());
        } else {
            rightChild->asLinear().mark(gcmarker);
        }
        leftChild = rope->leftChild();

        if (leftChild->isLinear()) {
            leftChild->asLinear().mark(gcmarker);
            return;
        }
        rope = &leftChild->asRope();
    } while (leftChild->markIfUnmarked());
}

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *str)
{
    JS_ASSERT_IF(gcmarker->context->runtime->gcCurrentCompartment,
                 str->compartment() == gcmarker->context->runtime->gcCurrentCompartment
                 || str->compartment() == gcmarker->context->runtime->atomsCompartment);

    if (str->isLinear()) {
        str->asLinear().mark(gcmarker);
    } else {
        JS_ASSERT(str->isRope());
        if (str->markIfUnmarked())
            ScanRope(gcmarker, &str->asRope());
    }
}

static const uintN LARGE_OBJECT_CHUNK_SIZE = 2048;

static void
ScanObject(GCMarker *gcmarker, JSObject *obj)
{
    if (obj->isNewborn())
        return;

    types::TypeObject *type = obj->gctype();
    if (type != &types::emptyTypeObject && !type->isMarked())
        type->trace(gcmarker, /* weak = */ true);

    if (JSObject *parent = obj->getParent())
        PushMarkStack(gcmarker, parent);
    if (!obj->isDenseArray() && obj->newType && !obj->newType->isMarked())
        obj->newType->trace(gcmarker);

    Class *clasp = obj->getClass();
    if (clasp->trace) {
        if (obj->isDenseArray() && obj->getDenseArrayInitializedLength() > LARGE_OBJECT_CHUNK_SIZE) {
            if (!gcmarker->largeStack.push(LargeMarkItem(obj))) {
                clasp->trace(gcmarker, obj);
            }
        } else {
            clasp->trace(gcmarker, obj);
        }
    }

    if (obj->isNative()) {
        js::Shape *shape = obj->lastProp;
        PushMarkStack(gcmarker, shape);

        if (gcmarker->context->runtime->gcRegenShapes) {
            /* We need to regenerate our shape if hasOwnShape(). */
            uint32 newShape = shape->shapeid;
            if (obj->hasOwnShape()) {
                newShape = js_RegenerateShapeForGC(gcmarker->context->runtime);
                JS_ASSERT(newShape != shape->shapeid);
            }
            obj->objShape = newShape;
        }

        uint32 nslots = obj->slotSpan();
        JS_ASSERT(obj->slotSpan() <= obj->numSlots());
        if (nslots > LARGE_OBJECT_CHUNK_SIZE) {
            if (gcmarker->largeStack.push(LargeMarkItem(obj)))
                return;
        }

        obj->scanSlots(gcmarker);
    }
}

static bool
ScanLargeObject(GCMarker *gcmarker, LargeMarkItem &item)
{
    JSObject *obj = item.obj;

    uintN start = item.markpos;
    uintN stop;
    uint32 capacity;
    if (obj->isDenseArray()) {
        capacity = obj->getDenseArrayInitializedLength();
        stop = JS_MIN(start + LARGE_OBJECT_CHUNK_SIZE, capacity);
        for (uintN i=stop; i>start; i--)
            ScanValue(gcmarker, obj->getDenseArrayElement(i-1));
    } else {
        JS_ASSERT(obj->isNative());
        capacity = obj->slotSpan();
        stop = JS_MIN(start + LARGE_OBJECT_CHUNK_SIZE, capacity);
        for (uintN i=stop; i>start; i--)
            ScanValue(gcmarker, obj->nativeGetSlot(i-1));
    }

    if (stop == capacity)
        return true;

    item.markpos += LARGE_OBJECT_CHUNK_SIZE;
    return false;
}

void
MarkChildren(JSTracer *trc, JSObject *obj)
{
    /* If obj has no map, it must be a newborn. */
    if (obj->isNewborn())
        return;

    types::TypeObject *type = obj->gctype();
    if (type != &types::emptyTypeObject)
        type->trace(trc, /* weak = */ true);

    /* Trace universal (ops-independent) members. */
    if (!obj->isDenseArray() && obj->newType)
        obj->newType->trace(trc);
    if (JSObject *parent = obj->getParent())
        MarkObject(trc, *parent, "parent");

    Class *clasp = obj->getClass();
    if (clasp->trace)
        clasp->trace(trc, obj);

    if (obj->isNative()) {
        MarkShape(trc, obj->lastProp, "shape");

        JS_ASSERT(obj->slotSpan() <= obj->numSlots());
        uint32 nslots = obj->slotSpan();
        for (uint32 i = 0; i < nslots; i++) {
            JS_SET_TRACING_DETAILS(trc, js_PrintObjectSlotName, obj, i);
            MarkValueRaw(trc, obj->nativeGetSlot(i));
        }
    }
}

void
MarkChildren(JSTracer *trc, JSString *str)
{
    if (str->isDependent()) {
        MarkString(trc, str->asDependent().base(), "base");
    } else if (str->isRope()) {
        JSRope &rope = str->asRope();
        MarkString(trc, rope.leftChild(), "left child");
        MarkString(trc, rope.rightChild(), "right child");
    }
}

void
MarkChildren(JSTracer *trc, const Shape *shape)
{
restart:
    MarkId(trc, shape->propid, "propid");

    if (shape->hasGetterValue() && shape->getter())
        MarkObjectWithPrinter(trc, *shape->getterObject(), PrintPropertyGetterOrSetter, shape, 0);
    if (shape->hasSetterValue() && shape->setter())
        MarkObjectWithPrinter(trc, *shape->setterObject(), PrintPropertyGetterOrSetter, shape, 1);

    if (shape->isMethod())
        MarkObjectWithPrinter(trc, shape->methodObject(), PrintPropertyMethod, shape, 0);

    shape = shape->previous();
    if (shape)
        goto restart;
}

#ifdef JS_HAS_XML_SUPPORT
void
MarkChildren(JSTracer *trc, JSXML *xml)
{
    js_TraceXML(trc, xml);
}
#endif

} /* namespace gc */

void
GCMarker::drainMarkStack()
{
    JSRuntime *rt = context->runtime;
    rt->gcCheckCompartment = rt->gcCurrentCompartment;

    while (!isMarkStackEmpty()) {
        while (!ropeStack.isEmpty())
            ScanRope(this, ropeStack.pop());

        while (!objStack.isEmpty())
            ScanObject(this, objStack.pop());

        while (!xmlStack.isEmpty())
            MarkChildren(this, xmlStack.pop());

        if (!largeStack.isEmpty()) {
            LargeMarkItem &item = largeStack.peek();
            if (ScanLargeObject(this, item))
                largeStack.pop();
        }

        if (isMarkStackEmpty()) {
            /*
             * Mark children of things that caused too deep recursion during the above
             * tracing. Don't do this until we're done with everything else.
             */
            markDelayedChildren();
        }
    }

    rt->gcCheckCompartment = NULL;
}

} /* namespace js */

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, uint32 kind)
{
    switch (kind) {
      case JSTRACE_OBJECT:
	MarkChildren(trc, (JSObject *)thing);
        break;

      case JSTRACE_STRING:
	MarkChildren(trc, (JSString *)thing);
        break;

      case JSTRACE_SHAPE:
	MarkChildren(trc, (js::Shape *)thing);
        break;

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        MarkChildren(trc, (JSXML *)thing);
        break;
#endif
    }
}

inline void
JSObject::scanSlots(GCMarker *gcmarker)
{
    /*
     * Scan the fixed slots and the dynamic slots separately, to avoid
     * branching inside nativeGetSlot().
     */
    JS_ASSERT(slotSpan() <= numSlots());
    uint32 nslots = slotSpan();
    uint32 nfixed = numFixedSlots();
    uint32 i;
    for (i = 0; i < nslots && i < nfixed; i++)
        ScanValue(gcmarker, fixedSlots()[i]);
    for (; i < nslots; i++)
        ScanValue(gcmarker, slots[i - nfixed]);
}

void
js::types::TypeObject::trace(JSTracer *trc, bool weak)
{
    /*
     * Only mark types if the Mark/Sweep GC is running; the bit won't be
     * cleared by the cycle collector. Also, don't mark for weak references
     * from singleton JS objects, as if there are no outstanding refs we will
     * destroy the type object and revert the JS object to a lazy type.
     */
    if (IS_GC_MARKING_TRACER(trc) && (!weak || !singleton)) {
        JS_ASSERT_IF(trc->context->runtime->gcCurrentCompartment,
                     compartment() == trc->context->runtime->gcCurrentCompartment);
        markIfUnmarked(static_cast<GCMarker *>(trc)->getMarkColor());
    }

#ifdef DEBUG
    InlineMarkId(trc, name_, "type_name");
#endif

    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop)
            InlineMarkId(trc, prop->id, "type_prop");
    }

    if (emptyShapes) {
        int count = FINALIZE_OBJECT_LAST - FINALIZE_OBJECT0 + 1;
        for (int i = 0; i < count; i++) {
            if (emptyShapes[i])
                InlineMark(trc, emptyShapes[i], "empty_shape");
        }
    }

    if (proto)
        InlineMark(trc, proto, "type_proto");

    if (singleton)
        InlineMark(trc, singleton, "type_singleton");

    if (newScript) {
        js_TraceScript(trc, newScript->script, NULL);
        InlineMark(trc, newScript->shape, "new_shape");
    }

    if (functionScript)
        js_TraceScript(trc, functionScript, NULL);
}
