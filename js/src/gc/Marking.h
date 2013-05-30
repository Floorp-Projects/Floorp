/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_marking_h___
#define gc_marking_h___

#include "jsgc.h"
#include "jscntxt.h"
#include "jslock.h"

#include "gc/Barrier.h"
#include "gc/Nursery.h"
#include "js/TemplateLib.h"
#include "ion/IonCode.h"

extern "C" {
struct JSContext;
class JSFunction;
class JSObject;
class JSScript;
}

class JSAtom;
class JSLinearString;

namespace js {

class ArgumentsObject;
class ArrayBufferObject;
class BaseShape;
class GlobalObject;
class UnownedBaseShape;
class Shape;

template<class, typename> class HeapPtr;

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
 *
 * The following functions are provided to test whether a GC thing is marked
 * under different circumstances:
 *
 * IsObjectAboutToBeFinalized(JSObject **thing);
 *     This function is indended to be used in code used to sweep GC things.  It
 *     indicates whether the object will will be finialized in the current group
 *     of compartments being swept.  Note that this will return false for any
 *     object not in the group of compartments currently being swept, as even if
 *     it is unmarked it may still become marked before it is swept.
 *
 * IsObjectMarked(JSObject **thing);
 *     This function is indended to be used in rare cases in code used to mark
 *     GC things.  It indicates whether the object is currently marked.
 */
#define DeclMarker(base, type)                                                                    \
void Mark##base(JSTracer *trc, EncapsulatedPtr<type> *thing, const char *name);                   \
void Mark##base##Root(JSTracer *trc, type **thingp, const char *name);                            \
void Mark##base##Unbarriered(JSTracer *trc, type **thingp, const char *name);                     \
void Mark##base##Range(JSTracer *trc, size_t len, HeapPtr<type> *thing, const char *name);        \
void Mark##base##RootRange(JSTracer *trc, size_t len, type **thing, const char *name);            \
bool Is##base##Marked(type **thingp);                                                             \
bool Is##base##Marked(EncapsulatedPtr<type> *thingp);                                             \
bool Is##base##AboutToBeFinalized(type **thingp);                                                 \
bool Is##base##AboutToBeFinalized(EncapsulatedPtr<type> *thingp);

DeclMarker(BaseShape, BaseShape)
DeclMarker(BaseShape, UnownedBaseShape)
DeclMarker(IonCode, ion::IonCode)
DeclMarker(Object, ArgumentsObject)
DeclMarker(Object, ArrayBufferObject)
DeclMarker(Object, DebugScopeObject)
DeclMarker(Object, GlobalObject)
DeclMarker(Object, JSObject)
DeclMarker(Object, JSFunction)
DeclMarker(Object, ScopeObject)
DeclMarker(Script, JSScript)
DeclMarker(LazyScript, LazyScript)
DeclMarker(Shape, Shape)
DeclMarker(String, JSAtom)
DeclMarker(String, JSString)
DeclMarker(String, JSFlatString)
DeclMarker(String, JSLinearString)
DeclMarker(String, PropertyName)
DeclMarker(TypeObject, types::TypeObject)

#undef DeclMarker

/* Return true if the pointer is NULL, or if it is a tagged pointer to NULL. */
JS_ALWAYS_INLINE bool
IsNullTaggedPointer(void *p)
{
    return uintptr_t(p) < 32;
}

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

void
MarkGCThingUnbarriered(JSTracer *trc, void **thingp, const char *name);

/*** ID Marking ***/

void
MarkId(JSTracer *trc, EncapsulatedId *id, const char *name);

void
MarkIdRoot(JSTracer *trc, jsid *id, const char *name);

void
MarkIdUnbarriered(JSTracer *trc, jsid *id, const char *name);

void
MarkIdRange(JSTracer *trc, size_t len, HeapId *vec, const char *name);

void
MarkIdRootRange(JSTracer *trc, size_t len, jsid *vec, const char *name);

/*** Value Marking ***/

void
MarkValue(JSTracer *trc, EncapsulatedValue *v, const char *name);

void
MarkValueRange(JSTracer *trc, size_t len, EncapsulatedValue *vec, const char *name);

inline void
MarkValueRange(JSTracer *trc, HeapValue *begin, HeapValue *end, const char *name)
{
    return MarkValueRange(trc, end - begin, begin, name);
}

void
MarkValueRoot(JSTracer *trc, Value *v, const char *name);

void
MarkThingOrValueUnbarriered(JSTracer *trc, uintptr_t *word, const char *name);

void
MarkValueRootRange(JSTracer *trc, size_t len, Value *vec, const char *name);

inline void
MarkValueRootRange(JSTracer *trc, Value *begin, Value *end, const char *name)
{
    MarkValueRootRange(trc, end - begin, begin, name);
}

void
MarkValueRootRangeMaybeNullPayload(JSTracer *trc, size_t len, Value *vec, const char *name);

void
MarkTypeRoot(JSTracer *trc, types::Type *v, const char *name);

bool
IsValueMarked(Value *v);

bool
IsValueAboutToBeFinalized(Value *v);

/*** Slot Marking ***/

void
MarkSlot(JSTracer *trc, HeapSlot *s, const char *name);

void
MarkArraySlots(JSTracer *trc, size_t len, HeapSlot *vec, const char *name);

void
MarkObjectSlots(JSTracer *trc, JSObject *obj, uint32_t start, uint32_t nslots);

void
MarkCrossCompartmentObjectUnbarriered(JSTracer *trc, JSObject *src, JSObject **dst_obj,
                                      const char *name);

void
MarkCrossCompartmentScriptUnbarriered(JSTracer *trc, JSObject *src, JSScript **dst_script,
                                      const char *name);

/*
 * Mark a value that may be in a different compartment from the compartment
 * being GC'd. (Although it won't be marked if it's in the wrong compartment.)
 */
void
MarkCrossCompartmentSlot(JSTracer *trc, JSObject *src, HeapSlot *dst_slot, const char *name);


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
Mark(JSTracer *trc, EncapsulatedValue *v, const char *name)
{
    MarkValue(trc, v, name);
}

inline void
Mark(JSTracer *trc, EncapsulatedPtrObject *o, const char *name)
{
    MarkObject(trc, o, name);
}

inline void
Mark(JSTracer *trc, EncapsulatedPtrScript *o, const char *name)
{
    MarkScript(trc, o, name);
}

inline void
Mark(JSTracer *trc, HeapPtr<ion::IonCode> *code, const char *name)
{
    MarkIonCode(trc, code, name);
}

/* For use by WeakMap's HashKeyRef instantiation. */
inline void
Mark(JSTracer *trc, JSObject **objp, const char *name)
{
    MarkObjectUnbarriered(trc, objp, name);
}

/* For use by Debugger::WeakMap's proxiedScopes HashKeyRef instantiation. */
inline void
Mark(JSTracer *trc, ScopeObject **obj, const char *name)
{
    MarkObjectUnbarriered(trc, obj, name);
}

bool
IsCellMarked(Cell **thingp);

bool
IsCellAboutToBeFinalized(Cell **thing);

inline bool
IsMarked(EncapsulatedValue *v)
{
    if (!v->isMarkable())
        return true;
    return IsValueMarked(v->unsafeGet());
}

inline bool
IsMarked(EncapsulatedPtrObject *objp)
{
    return IsObjectMarked(objp);
}

inline bool
IsMarked(EncapsulatedPtrScript *scriptp)
{
    return IsScriptMarked(scriptp);
}

inline bool
IsAboutToBeFinalized(EncapsulatedValue *v)
{
    if (!v->isMarkable())
        return false;
    return IsValueAboutToBeFinalized(v->unsafeGet());
}

inline bool
IsAboutToBeFinalized(EncapsulatedPtrObject *objp)
{
    return IsObjectAboutToBeFinalized(objp);
}

inline bool
IsAboutToBeFinalized(EncapsulatedPtrScript *scriptp)
{
    return IsScriptAboutToBeFinalized(scriptp);
}

#ifdef JS_ION
/* Nonsense to get WeakCache to work with new Marking semantics. */

inline bool
IsAboutToBeFinalized(const js::ion::VMFunction **vmfunc)
{
    /*
     * Preserves entries in the WeakCache<VMFunction, IonCode>
     * iff the IonCode has been marked.
     */
    return true;
}

inline bool
IsAboutToBeFinalized(ReadBarriered<js::ion::IonCode> code)
{
    return IsIonCodeAboutToBeFinalized(code.unsafeGet());
}
#endif

inline Cell *
ToMarkable(const Value &v)
{
    if (v.isMarkable())
        return (Cell *)v.toGCThing();
    return NULL;
}

inline Cell *
ToMarkable(Cell *cell)
{
    return cell;
}

inline JSGCTraceKind
TraceKind(const Value &v)
{
    JS_ASSERT(v.isMarkable());
    if (v.isObject())
        return JSTRACE_OBJECT;
    return JSTRACE_STRING;
}

inline JSGCTraceKind
TraceKind(JSObject *obj)
{
    return JSTRACE_OBJECT;
}

inline JSGCTraceKind
TraceKind(JSScript *script)
{
    return JSTRACE_SCRIPT;
}

inline JSGCTraceKind
TraceKind(LazyScript *lazy)
{
    return JSTRACE_LAZY_SCRIPT;
}

} /* namespace gc */

void
TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind);

} /* namespace js */

#endif /* gc_marking_h___ */
