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
#include "jslock.h"
#include "jsscope.h"
#include "jsxml.h"

#include "js/TemplateLib.h"

namespace js {

struct Shape;

namespace gc {

inline JSGCTraceKind
GetGCThingTraceKind(const void *thing)
{
    JS_ASSERT(thing);
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    return MapAllocToTraceKind(cell->getAllocKind());
}

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

/* Get the best kind to use when making an object with the given slot count. */
static inline AllocKind
GetGCObjectKind(size_t numSlots, bool isArray = false)
{
    extern AllocKind slotsToThingKind[];

    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT) {
        /*
         * If the object will definitely want more than the maximum number of
         * fixed slots, use zero fixed slots for arrays and the maximum for
         * other objects. Arrays do not use their fixed slots anymore when
         * they have a slots array, while other objects will continue to do so.
         */
        return isArray ? FINALIZE_OBJECT0 : FINALIZE_OBJECT16;
    }
    return slotsToThingKind[numSlots];
}

static inline AllocKind
GetGCObjectFixedSlotsKind(size_t numFixedSlots)
{
    extern AllocKind slotsToThingKind[];

    JS_ASSERT(numFixedSlots < SLOTS_TO_THING_KIND_LIMIT);
    return slotsToThingKind[numFixedSlots];
}

static inline bool
IsBackgroundAllocKind(AllocKind kind)
{
    JS_ASSERT(kind < FINALIZE_OBJECT_LIMIT);
    return kind % 2 == 1;
}

static inline AllocKind
GetBackgroundAllocKind(AllocKind kind)
{
    JS_ASSERT(!IsBackgroundAllocKind(kind));
    return (AllocKind) (kind + 1);
}

/*
 * Try to get the next larger size for an object, keeping BACKGROUND
 * consistent.
 */
static inline bool
TryIncrementAllocKind(AllocKind *kindp)
{
    size_t next = size_t(*kindp) + 2;
    if (next >= size_t(FINALIZE_OBJECT_LIMIT))
        return false;
    *kindp = AllocKind(next);
    return true;
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(AllocKind thingKind)
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
    if (cx->runtime->gcZeal() >= js::gc::ZealPokeThreshold)
        cx->runtime->gcNextScheduled = 1;
#endif
}

/*
 * Invoke ArenaOp and CellOp on every arena and cell in a compartment which
 * have the specified thing kind.
 */
template <class ArenaOp, class CellOp>
void
ForEachArenaAndCell(JSCompartment *compartment, AllocKind thingKind,
                    ArenaOp arenaOp, CellOp cellOp)
{
    size_t thingSize = Arena::thingSize(thingKind);
    ArenaHeader *aheader = compartment->arenas.getFirstArena(thingKind);

    for (; aheader; aheader = aheader->next) {
        Arena *arena = aheader->getArena();
        arenaOp(arena);
        FreeSpan firstSpan(aheader->getFirstFreeSpan());
        const FreeSpan *span = &firstSpan;

        for (uintptr_t thing = arena->thingsStart(thingKind); ; thing += thingSize) {
            JS_ASSERT(thing <= arena->thingsEnd());
            if (thing == span->first) {
                if (!span->hasNext())
                    break;
                thing = span->last;
                span = span->nextSpan();
            } else {
                Cell *t = reinterpret_cast<Cell *>(thing);
                cellOp(t);
            }
        }
    }
}

class CellIterImpl
{
    size_t firstThingOffset;
    size_t thingSize;
    ArenaHeader *aheader;
    FreeSpan firstSpan;
    const FreeSpan *span;
    uintptr_t thing;
    Cell *cell;

  protected:
    CellIterImpl() {
    }

    void init(JSCompartment *comp, AllocKind kind) {
        JS_ASSERT(comp->arenas.isSynchronizedFreeList(kind));
        firstThingOffset = Arena::firstThingOffset(kind);
        thingSize = Arena::thingSize(kind);
        aheader = comp->arenas.getFirstArena(kind);
        firstSpan.initAsEmpty();
        span = &firstSpan;
        thing = span->first;
        next();
    }

  public:
    bool done() const {
        return !cell;
    }

    template<typename T> T *get() const {
        JS_ASSERT(!done());
        return static_cast<T *>(cell);
    }

    Cell *getCell() const {
        JS_ASSERT(!done());
        return cell;
    }

    void next() {
        for (;;) {
            if (thing != span->first)
                break;
            if (JS_LIKELY(span->hasNext())) {
                thing = span->last + thingSize;
                span = span->nextSpan();
                break;
            }
            if (!aheader) {
                cell = NULL;
                return;
            }
            firstSpan = aheader->getFirstFreeSpan();
            span = &firstSpan;
            thing = aheader->arenaAddress() | firstThingOffset;
            aheader = aheader->next;
        }
        cell = reinterpret_cast<Cell *>(thing);
        thing += thingSize;
    }
};

class CellIterUnderGC : public CellIterImpl {

  public:
    CellIterUnderGC(JSCompartment *comp, AllocKind kind) {
        JS_ASSERT(comp->rt->gcRunning);
        init(comp, kind);
    }
};

/*
 * When using the iterator outside the GC the caller must ensure that no GC or
 * allocations of GC things are possible and that the background finalization
 * for the given thing kind is not enabled or is done.
 */
class CellIter: public CellIterImpl
{
    ArenaLists *lists;
    AllocKind kind;
#ifdef DEBUG
    size_t *counter;
#endif
  public:
    CellIter(JSContext *cx, JSCompartment *comp, AllocKind kind)
      : lists(&comp->arenas),
        kind(kind) {
#ifdef JS_THREADSAFE
        JS_ASSERT(comp->arenas.doneBackgroundFinalize(kind));
#endif
        if (lists->isSynchronizedFreeList(kind)) {
            lists = NULL;
        } else {
            JS_ASSERT(!comp->rt->gcRunning);
            lists->copyFreeListToArena(kind);
        }
#ifdef DEBUG
        counter = &JS_THREAD_DATA(cx)->noGCOrAllocationCheck;
        ++*counter;
#endif
        init(comp, kind);
    }

    ~CellIter() {
#ifdef DEBUG
        JS_ASSERT(*counter > 0);
        --*counter;
#endif
        if (lists)
            lists->clearFreeListInArena(kind);
    }
};

/* Signatures for ArenaOp and CellOp above. */

inline void EmptyArenaOp(Arena *arena) {}
inline void EmptyCellOp(Cell *t) {}

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
NewGCThing(JSContext *cx, js::gc::AllocKind kind, size_t thingSize)
{
    JS_ASSERT(thingSize == js::gc::Arena::thingSize(kind));
#ifdef JS_THREADSAFE
    JS_ASSERT_IF((cx->compartment == cx->runtime->atomsCompartment),
                 kind == js::gc::FINALIZE_STRING || kind == js::gc::FINALIZE_SHORT_STRING);
#endif
    JS_ASSERT(!cx->runtime->gcRunning);
    JS_ASSERT(!JS_THREAD_DATA(cx)->noGCOrAllocationCheck);

#ifdef JS_GC_ZEAL
    if (cx->runtime->needZealousGC())
        js::gc::RunDebugGC(cx);
#endif

    JSCompartment *comp = cx->compartment;
    void *t = comp->arenas.allocateFromFreeList(kind, thingSize);
    if (!t)
        t = js::gc::ArenaLists::refillFreeList(cx, kind);
    return static_cast<T *>(t);
}

inline JSObject *
js_NewGCObject(JSContext *cx, js::gc::AllocKind kind)
{
    JS_ASSERT(kind >= js::gc::FINALIZE_OBJECT0 && kind < js::gc::FINALIZE_OBJECT_LIMIT);
    JSObject *obj = NewGCThing<JSObject>(cx, kind, js::gc::Arena::thingSize(kind));
    if (obj)
        obj->earlyInit(js::gc::GetGCKindSlots(kind));
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
    if (fun)
        fun->earlyInit(JSObject::FUN_CLASS_RESERVED_SLOTS);
    return fun;
}

inline JSScript *
js_NewGCScript(JSContext *cx)
{
    return NewGCThing<JSScript>(cx, js::gc::FINALIZE_SCRIPT, sizeof(JSScript));
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
