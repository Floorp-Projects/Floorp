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

#ifndef jsgcinlines_h___
#define jsgcinlines_h___

#include "jsgc.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsscope.h"
#include "jsxml.h"

#include "jslock.h"
#include "jstl.h"

inline bool
JSAtom::isUnitString(const void *ptr)
{
    jsuword delta = reinterpret_cast<jsuword>(ptr) -
                    reinterpret_cast<jsuword>(unitStaticTable);
    if (delta >= UNIT_STATIC_LIMIT * sizeof(JSString))
        return false;

    /* If ptr points inside the static array, it must be well-aligned. */
    JS_ASSERT(delta % sizeof(JSString) == 0);
    return true;
}

inline bool
JSAtom::isLength2String(const void *ptr)
{
    jsuword delta = reinterpret_cast<jsuword>(ptr) -
                    reinterpret_cast<jsuword>(length2StaticTable);
    if (delta >= NUM_SMALL_CHARS * NUM_SMALL_CHARS * sizeof(JSString))
        return false;

    /* If ptr points inside the static array, it must be well-aligned. */
    JS_ASSERT(delta % sizeof(JSString) == 0);
    return true;
}

inline bool
JSAtom::isHundredString(const void *ptr)
{
    jsuword delta = reinterpret_cast<jsuword>(ptr) -
                    reinterpret_cast<jsuword>(hundredStaticTable);
    if (delta >= NUM_HUNDRED_STATICS * sizeof(JSString))
        return false;

    /* If ptr points inside the static array, it must be well-aligned. */
    JS_ASSERT(delta % sizeof(JSString) == 0);
    return true;
}

inline bool
JSAtom::isStatic(const void *ptr)
{
    return isUnitString(ptr) || isLength2String(ptr) || isHundredString(ptr);
}

namespace js {

struct Shape;

namespace gc {

inline uint32
GetGCThingTraceKind(const void *thing)
{
    JS_ASSERT(thing);
    if (JSAtom::isStatic(thing))
        return JSTRACE_STRING;
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    return GetFinalizableTraceKind(cell->arenaHeader()->getThingKind());
}

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

/* Get the best kind to use when making an object with the given slot count. */
static inline FinalizeKind
GetGCObjectKind(size_t numSlots)
{
    extern FinalizeKind slotsToThingKind[];

    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return FINALIZE_OBJECT0;
    return slotsToThingKind[numSlots];
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(FinalizeKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
        return 0;
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
        return 2;
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
        return 4;
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
        return 8;
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
        return 12;
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        return 16;
      default:
        JS_NOT_REACHED("Bad object finalize kind");
        return 0;
    }
}

static inline void
GCPoke(JSContext *cx, Value oldval)
{
    /*
     * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
     * loading oldval.  XXX remove implied force, fix jsinterp.c's "second arg
     * ignored", etc.
     */
#if 1
    cx->runtime->gcPoke = JS_TRUE;
#else
    cx->runtime->gcPoke = oldval.isGCThing();
#endif

#ifdef JS_GC_ZEAL
    /* Schedule a GC to happen "soon" after a GC poke. */
    if (cx->runtime->gcZeal())
        cx->runtime->gcNextScheduled = 1;
#endif
}

} /* namespace gc */
} /* namespace js */

/*
 * Allocates a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */

template <typename T>
inline T *
NewGCThing(JSContext *cx, unsigned thingKind, size_t thingSize)
{
    JS_ASSERT(thingKind < js::gc::FINALIZE_LIMIT);
    JS_ASSERT(thingSize == js::gc::GCThingSizeMap[thingKind]);
#ifdef JS_THREADSAFE
    JS_ASSERT_IF((cx->compartment == cx->runtime->atomsCompartment),
                 (thingKind == js::gc::FINALIZE_STRING) ||
                 (thingKind == js::gc::FINALIZE_SHORT_STRING));
#endif
    JS_ASSERT(!cx->runtime->gcRunning);

#ifdef JS_GC_ZEAL
    if (cx->runtime->needZealousGC())
        js::gc::RunDebugGC(cx);
#endif

    js::gc::Cell *cell = cx->compartment->freeLists.getNext(thingKind, thingSize);
    return static_cast<T *>(cell ? cell : js::gc::RefillFinalizableFreeList(cx, thingKind));
}

inline JSObject *
js_NewGCObject(JSContext *cx, js::gc::FinalizeKind kind)
{
    JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind <= js::gc::FINALIZE_OBJECT_LAST);
    JSObject *obj = NewGCThing<JSObject>(cx, kind, js::gc::GCThingSizeMap[kind]);
    if (obj) {
        obj->capacity = js::gc::GetGCKindSlots(kind);
        obj->lastProp = NULL; /* Stops obj from being scanned until initializated. */
    }
    return obj;
}

inline JSString *
js_NewGCString(JSContext *cx)
{
    return NewGCThing<JSString>(cx, js::gc::FINALIZE_STRING, sizeof(JSString));
}

inline JSShortString *
js_NewGCShortString(JSContext *cx)
{
    return NewGCThing<JSShortString>(cx, js::gc::FINALIZE_SHORT_STRING, sizeof(JSShortString));
}

inline JSExternalString *
js_NewGCExternalString(JSContext *cx)
{
    return NewGCThing<JSExternalString>(cx, js::gc::FINALIZE_EXTERNAL_STRING,
                                        sizeof(JSExternalString));
}

inline JSFunction*
js_NewGCFunction(JSContext *cx)
{
    JSFunction *fun = NewGCThing<JSFunction>(cx, js::gc::FINALIZE_FUNCTION, sizeof(JSFunction));
    if (fun) {
        fun->capacity = JSObject::FUN_CLASS_RESERVED_SLOTS;
        fun->lastProp = NULL; /* Stops fun from being scanned until initializated. */
    }
    return fun;
}

inline js::Shape *
js_NewGCShape(JSContext *cx)
{
    return NewGCThing<js::Shape>(cx, js::gc::FINALIZE_SHAPE, sizeof(js::Shape));
}

#if JS_HAS_XML_SUPPORT
extern JSXML *
js_NewGCXML(JSContext *cx);
#endif

#endif /* jsgcinlines_h___ */
