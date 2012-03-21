/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgcmark_h___
#define jsgcmark_h___

#include "jsgc.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jslock.h"

#include "gc/Barrier.h"
#include "js/TemplateLib.h"

namespace js {
namespace gc {

/*** Object Marking ***/

/*
 * These functions expose marking functionality for all of the different GC
 * thing kinds. For each GC thing, there are several variants. As an example,
 * these are the variants generated for JSObject. They are listed from most to
 * least desirable for use:
 *
 * MarkObject(JSTracer *trc, const HeapPtr<JSObject> &thing, const char *name);
 *     This function should be used for marking JSObjects, in preference to all
 *     others below. Use it when you have HeapPtr<JSObject>, which
 *     automatically implements write barriers.
 *
 * MarkObjectRoot(JSTracer *trc, JSObject *thing, const char *name);
 *     This function is only valid during the root marking phase of GC (i.e.,
 *     when MarkRuntime is on the stack).
 *
 * MarkObjectUnbarriered(JSTracer *trc, JSObject *thing, const char *name);
 *     Like MarkObject, this function can be called at any time. It is more
 *     forgiving, since it doesn't demand a HeapPtr as an argument. Its use
 *     should always be accompanied by a comment explaining how write barriers
 *     are implemented for the given field.
 *
 * Additionally, the functions MarkObjectRange and MarkObjectRootRange are
 * defined for marking arrays of object pointers.
 */
#define DeclMarker(base, type)                                                                    \
void Mark##base(JSTracer *trc, HeapPtr<type> *thing, const char *name);                           \
void Mark##base##Root(JSTracer *trc, type **thingp, const char *name);                            \
void Mark##base##Unbarriered(JSTracer *trc, type **thingp, const char *name);                     \
void Mark##base##Range(JSTracer *trc, size_t len, HeapPtr<type> *thing, const char *name);        \
void Mark##base##RootRange(JSTracer *trc, size_t len, type **thing, const char *name);

DeclMarker(BaseShape, BaseShape)
DeclMarker(BaseShape, UnownedBaseShape)
DeclMarker(Object, ArgumentsObject)
DeclMarker(Object, GlobalObject)
DeclMarker(Object, JSObject)
DeclMarker(Object, JSFunction)
DeclMarker(Script, JSScript)
DeclMarker(Shape, Shape)
DeclMarker(String, JSAtom)
DeclMarker(String, JSString)
DeclMarker(String, JSFlatString)
DeclMarker(String, JSLinearString)
DeclMarker(TypeObject, types::TypeObject)
#if JS_HAS_XML_SUPPORT
DeclMarker(XML, JSXML)
#endif

/*** Externally Typed Marking ***/

/*
 * Note: this must only be called by the GC and only when we are tracing through
 * MarkRoots. It is explicitly for ConservativeStackMarking and should go away
 * after we transition to exact rooting.
 */
void
MarkKind(JSTracer *trc, void **thingp, JSGCTraceKind kind);

void
MarkGCThingRoot(JSTracer *trc, void **thingp, const char *name);

/*** ID Marking ***/

void
MarkId(JSTracer *trc, HeapId *id, const char *name);

void
MarkIdRoot(JSTracer *trc, jsid *id, const char *name);

void
MarkIdRange(JSTracer *trc, size_t len, js::HeapId *vec, const char *name);

void
MarkIdRootRange(JSTracer *trc, size_t len, jsid *vec, const char *name);

/*** Value Marking ***/

void
MarkValue(JSTracer *trc, HeapValue *v, const char *name);

void
MarkValueRange(JSTracer *trc, size_t len, HeapValue *vec, const char *name);

void
MarkValueRoot(JSTracer *trc, Value *v, const char *name);

void
MarkValueRootRange(JSTracer *trc, size_t len, Value *vec, const char *name);

inline void
MarkValueRootRange(JSTracer *trc, Value *begin, Value *end, const char *name)
{
    MarkValueRootRange(trc, end - begin, begin, name);
}

/*** Slot Marking ***/

void
MarkSlot(JSTracer *trc, HeapSlot *s, const char *name);

void
MarkArraySlots(JSTracer *trc, size_t len, HeapSlot *vec, const char *name);

void
MarkObjectSlots(JSTracer *trc, JSObject *obj, uint32_t start, uint32_t nslots);

/*
 * Mark a value that may be in a different compartment from the compartment
 * being GC'd. (Although it won't be marked if it's in the wrong compartment.)
 */
void
MarkCrossCompartmentSlot(JSTracer *trc, HeapSlot *s, const char *name);


/*** Special Cases ***/

/*
 * The unioned HeapPtr stored in script->globalObj needs special treatment to
 * typecheck correctly.
 */
void
MarkObject(JSTracer *trc, HeapPtr<GlobalObject, JSScript *> *thingp, const char *name);

/* Direct value access used by the write barriers and the methodjit. */
void
MarkValueUnbarriered(JSTracer *trc, Value *v, const char *name);

/*
 * MarkChildren<JSObject> is exposed solely for preWriteBarrier on
 * JSObject::TradeGuts. It should not be considered external interface.
 */
void
MarkChildren(JSTracer *trc, JSObject *obj);

/*
 * Trace through the shape and any shapes it contains to mark
 * non-shape children. This is exposed to the JS API as
 * JS_TraceShapeCycleCollectorChildren.
 */
void
MarkCycleCollectorChildren(JSTracer *trc, Shape *shape);

void
PushArena(GCMarker *gcmarker, ArenaHeader *aheader);

/*** Generic ***/

/*
 * The Mark() functions interface should only be used by code that must be
 * templated.  Other uses should use the more specific, type-named functions.
 */

inline void
Mark(JSTracer *trc, HeapValue *v, const char *name)
{
    MarkValue(trc, v, name);
}

inline void
Mark(JSTracer *trc, HeapPtr<JSObject> *o, const char *name)
{
    MarkObject(trc, o, name);
}

inline void
Mark(JSTracer *trc, HeapPtr<JSXML> *xml, const char *name)
{
    MarkXML(trc, xml, name);
}

inline bool
IsMarked(const Value &v)
{
    if (v.isMarkable())
        return !IsAboutToBeFinalized(v);
    return true;
}

inline bool
IsMarked(Cell *cell)
{
    return !IsAboutToBeFinalized(cell);
}

} /* namespace gc */

void
TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind);

void
CallTracer(JSTracer *trc, void *thing, JSGCTraceKind kind);

} /* namespace js */

#endif
