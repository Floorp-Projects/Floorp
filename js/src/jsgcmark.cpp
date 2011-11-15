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
 * IteratorClass). It it always valid to call a MarkX function instead of
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
MarkObjectWithPrinterUnbarriered(JSTracer *trc, JSObject *obj, JSTraceNamePrinter printer,
                                 const void *arg, size_t index)
{
    JS_ASSERT(trc);
    JS_ASSERT(obj);
    JS_SET_TRACING_DETAILS(trc, printer, arg, index);
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
MarkTypeObjectUnbarriered(JSTracer *trc, types::TypeObject *type, const char *name)
{
    JS_ASSERT(trc);
    JS_ASSERT(type);
    JS_SET_TRACING_NAME(trc, name);
    if (type == &types::emptyTypeObject)
        return;
    Mark(trc, type);

    /*
     * Mark parts of a type object skipped by ScanTypeObject. ScanTypeObject is
     * only used for marking tracers; for tracers with a callback, if we
     * reenter through JS_TraceChildren then MarkChildren will *not* skip these
     * members, and we don't need to handle them here.
     */
    if (IS_GC_MARKING_TRACER(trc)) {
        if (type->singleton)
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

void
PushMarkStack(GCMarker *gcmarker, JSXML *thing)
{
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushXML(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSObject *thing)
{
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing)
{
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

void
PushMarkStack(GCMarker *gcmarker, types::TypeObject *thing)
{
    JS_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushType(thing);
}

void
PushMarkStack(GCMarker *gcmarker, JSScript *thing)
{
    JS_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                 thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

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
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     thing->compartment() == gcmarker->runtime->gcCurrentCompartment);

    /* We mark shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanShape(gcmarker, thing);
}

static void
MarkAtomRange(JSTracer *trc, size_t len, JSAtom **vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSAtom *atom = vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            Mark(trc, atom);
        }
    }
}

void
MarkObjectRange(JSTracer *trc, size_t len, HeapPtr<JSObject> *vec, const char *name)
{
    for (uint32 i = 0; i < len; i++) {
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
PrintPropertyId(char *buf, size_t bufsize, jsid propid, const char *label)
{
    JS_ASSERT(!JSID_IS_VOID(propid));
    if (JSID_IS_ATOM(propid)) {
        size_t n = PutEscapedString(buf, bufsize, JSID_TO_ATOM(propid), 0);
        if (n < bufsize)
            JS_snprintf(buf + n, bufsize - n, " %s", label);
    } else if (JSID_IS_INT(propid)) {
        JS_snprintf(buf, bufsize, "%d %s", JSID_TO_INT(propid), label);
    } else {
        JS_snprintf(buf, bufsize, "<object> %s", label);
    }
}

static void
PrintPropertyGetterOrSetter(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == PrintPropertyGetterOrSetter);
    Shape *shape = (Shape *)trc->debugPrintArg;
    PrintPropertyId(buf, bufsize, shape->propid,
                    trc->debugPrintIndex ? js_setter_str : js_getter_str); 
}

static void
PrintPropertyMethod(JSTracer *trc, char *buf, size_t bufsize)
{
    JS_ASSERT(trc->debugPrinter == PrintPropertyMethod);
    Shape *shape = (Shape *)trc->debugPrintArg;
    PrintPropertyId(buf, bufsize, shape->propid, " method");
}

static inline void
ScanValue(GCMarker *gcmarker, const Value &v)
{
    if (v.isMarkable()) {
        JSGCTraceKind kind = v.gcKind();
        if (kind == JSTRACE_STRING) {
            PushMarkStack(gcmarker, v.toString());
        } else {
            JS_ASSERT(kind == JSTRACE_OBJECT);
            PushMarkStack(gcmarker, &v.toObject());
        }
    }
}

static void
ScanShape(GCMarker *gcmarker, const Shape *shape)
{
restart:
    JSRuntime *rt = gcmarker->runtime;
    if (rt->gcRegenShapes)
        shape->shapeid = js_RegenerateShapeForGC(rt);

    if (JSID_IS_STRING(shape->propid))
        PushMarkStack(gcmarker, JSID_TO_STRING(shape->propid));
    else if (JS_UNLIKELY(JSID_IS_OBJECT(shape->propid)))
        PushMarkStack(gcmarker, JSID_TO_OBJECT(shape->propid));

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
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     rope->compartment() == gcmarker->runtime->gcCurrentCompartment
                     || rope->compartment() == gcmarker->runtime->atomsCompartment);
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
    JS_OPT_ASSERT_IF(gcmarker->runtime->gcCurrentCompartment,
                     str->compartment() == gcmarker->runtime->gcCurrentCompartment
                     || str->compartment() == gcmarker->runtime->atomsCompartment);

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

    types::TypeObject *type = obj->typeFromGC();
    if (type != &types::emptyTypeObject)
        PushMarkStack(gcmarker, type);

    if (JSObject *parent = obj->getParent())
        PushMarkStack(gcmarker, parent);

    /*
     * Call the trace hook if necessary, and check for a newType on objects
     * which are not dense arrays (dense arrays have trace hooks).
     */
    Class *clasp = obj->getClass();
    if (clasp->trace) {
        if (clasp == &ArrayClass) {
            if (obj->getDenseArrayInitializedLength() > LARGE_OBJECT_CHUNK_SIZE) {
                if (!gcmarker->largeStack.push(LargeMarkItem(obj)))
                    clasp->trace(gcmarker, obj);
            } else {
                clasp->trace(gcmarker, obj);
            }
        } else {
            if (obj->newType)
                PushMarkStack(gcmarker, obj->newType);
            clasp->trace(gcmarker, obj);
        }
    } else {
        if (obj->newType)
            PushMarkStack(gcmarker, obj->newType);
    }

    if (obj->isNative()) {
        js::Shape *shape = obj->lastProp;
        PushMarkStack(gcmarker, shape);

        if (gcmarker->runtime->gcRegenShapes) {
            /* We need to regenerate our shape if hasOwnShape(). */
            uint32 newShape = shape->shapeid;
            if (obj->hasOwnShape()) {
                newShape = js_RegenerateShapeForGC(gcmarker->runtime);
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

    MarkTypeObject(trc, obj->typeFromGC(), "type");

    /* Trace universal (ops-independent) members. */
    if (!obj->isDenseArray() && obj->newType)
        MarkTypeObject(trc, obj->newType, "new_type");
    if (obj->parent)
        MarkObject(trc, obj->parent, "parent");

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

#ifdef JS_CRASH_DIAGNOSTICS
    JSRuntime *rt = trc->runtime;
    JS_OPT_ASSERT_IF(rt->gcCheckCompartment, script->compartment() == rt->gcCheckCompartment);
#endif

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

    if (!script->isCachedEval && script->globalObject)
        MarkObject(trc, script->globalObject, "object");

    if (IS_GC_MARKING_TRACER(trc) && script->filename)
        js_MarkScriptFilename(script->filename);

    script->bindings.trace(trc);

    if (script->types)
        script->types->trace(trc);
}

void
MarkChildren(JSTracer *trc, const Shape *shape)
{
restart:
    MarkId(trc, shape->propid, "propid");

    if (shape->hasGetterValue() && shape->getter())
        MarkObjectWithPrinterUnbarriered(trc, shape->getterObject(),
                                         PrintPropertyGetterOrSetter, shape, 0);
    if (shape->hasSetterValue() && shape->setter())
        MarkObjectWithPrinterUnbarriered(trc, shape->setterObject(),
                                         PrintPropertyGetterOrSetter, shape, 1);

    if (shape->isMethod())
        MarkObjectWithPrinterUnbarriered(trc, &shape->methodObject(),
                                         PrintPropertyMethod, shape, 0);

    shape = shape->previous();
    if (shape)
        goto restart;
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

    if (type->emptyShapes) {
        for (unsigned i = 0; i < FINALIZE_OBJECT_LIMIT; i++) {
            if (type->emptyShapes[i])
                PushMarkStack(gcmarker, type->emptyShapes[i]);
        }
    }

    if (type->proto)
        PushMarkStack(gcmarker, type->proto);

    if (type->newScript) {
        PushMarkStack(gcmarker, type->newScript->fun);
        PushMarkStack(gcmarker, type->newScript->shape);
    }

    /*
     * Don't need to trace singleton or functionScript, an object with this
     * type must have already been traced and it will also hold a reference
     * on the script (singleton and functionScript types cannot be the newType
     * of another object). Attempts to mark type objects directly must use
     * MarkTypeObject, which will itself mark these extra bits.
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

    if (type->emptyShapes) {
        for (unsigned i = 0; i < FINALIZE_OBJECT_LIMIT; i++) {
            if (type->emptyShapes[i])
                MarkShape(trc, type->emptyShapes[i], "empty_shape");
        }
    }

    if (type->proto)
        MarkObject(trc, type->proto, "type_proto");

    if (type->singleton)
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

void
GCMarker::drainMarkStack()
{
    JSRuntime *rt = runtime;
    rt->gcCheckCompartment = rt->gcCurrentCompartment;

    while (!isMarkStackEmpty()) {
        while (!ropeStack.isEmpty())
            ScanRope(this, ropeStack.pop());

        while (!objStack.isEmpty())
            ScanObject(this, objStack.pop());

        while (!typeStack.isEmpty())
            ScanTypeObject(this, typeStack.pop());

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

inline void
JSObject::scanSlots(GCMarker *gcmarker)
{
    /*
     * Scan the fixed slots and the dynamic slots separately, to avoid
     * branching inside nativeGetSlot().
     */
    JS_ASSERT(slotSpan() <= numSlots());
    unsigned i, nslots = slotSpan();
    if (slots) {
        unsigned nfixed = numFixedSlots();
        if (nslots > nfixed) {
            HeapValue *vp = fixedSlots();
            for (i = 0; i < nfixed; i++, vp++)
                ScanValue(gcmarker, *vp);
            vp = slots;
            for (; i < nslots; i++, vp++)
                ScanValue(gcmarker, *vp);
            return;
        }
    }
    JS_ASSERT(nslots <= numFixedSlots());
    HeapValue *vp = fixedSlots();
    for (i = 0; i < nslots; i++, vp++)
        ScanValue(gcmarker, *vp);
}
