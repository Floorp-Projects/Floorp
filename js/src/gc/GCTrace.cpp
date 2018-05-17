/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JS_GC_TRACE

#include "gc/GCTrace.h"

#include <stdio.h>
#include <string.h>

#include "gc/AllocKind.h"
#include "gc/GCTraceFormat.h"
#include "js/HashTable.h"
#include "vm/JSFunction.h"
#include "vm/JSObject.h"

#include "gc/ObjectKind-inl.h"

namespace js {
namespace gc {

GCTrace gcTracer;

JS_STATIC_ASSERT(NumAllocKinds == unsigned(AllocKind::LIMIT));
JS_STATIC_ASSERT(LastObjectAllocKind == unsigned(AllocKind::OBJECT_LAST));

static FILE* gcTraceFile = nullptr;

static HashSet<const Class*, DefaultHasher<const Class*>, SystemAllocPolicy> tracedClasses;
static HashSet<const ObjectGroup*, DefaultHasher<const ObjectGroup*>, SystemAllocPolicy> tracedGroups;

static inline void
WriteWord(uint64_t data)
{
    if (gcTraceFile)
        fwrite(&data, sizeof(data), 1, gcTraceFile);
}

static inline void
TraceEvent(GCTraceEvent event, uint64_t payload = 0, uint8_t extra = 0)
{
    MOZ_ASSERT(event < GCTraceEventCount);
    MOZ_ASSERT((payload >> TracePayloadBits) == 0);
    WriteWord((uint64_t(event) << TraceEventShift) |
               (uint64_t(extra) << TraceExtraShift) | payload);
}

static inline void
TraceAddress(const void* p)
{
    TraceEvent(TraceDataAddress, uint64_t(p));
}

static inline void
TraceInt(uint32_t data)
{
    TraceEvent(TraceDataInt, data);
}

static void
TraceString(const char* string)
{
    JS_STATIC_ASSERT(sizeof(char) == 1);

    size_t length = strlen(string);
    const unsigned charsPerWord = sizeof(uint64_t);
    unsigned wordCount = (length + charsPerWord - 1) / charsPerWord;

    TraceEvent(TraceDataString, length);
    for (unsigned i = 0; i < wordCount; ++i) {
        union
        {
            uint64_t word;
            char chars[charsPerWord];
        } data;
        strncpy(data.chars, string + (i * charsPerWord), charsPerWord);
        WriteWord(data.word);
    }
}

bool
GCTrace::initTrace(GCRuntime& gc)
{
    /* This currently does not support multiple runtimes. */
    MOZ_ALWAYS_TRUE(!gcTraceFile);

    char* filename = getenv("JS_GC_TRACE");
    if (!filename)
        return true;

    if (!tracedClasses.init() || !tracedGroups.init()) {
        finishTrace();
        return false;
    }

    gcTraceFile = fopen(filename, "w");
    if (!gcTraceFile) {
        finishTrace();
        return false;
    }

    TraceEvent(TraceEventInit, 0, TraceFormatVersion);

    /* Trace information about thing sizes. */
    for (auto kind : AllAllocKinds())
        TraceEvent(TraceEventThingSize, Arena::thingSize(kind));

    return true;
}

void
GCTrace::finishTrace()
{
    if (gcTraceFile) {
        fclose(gcTraceFile);
        gcTraceFile = nullptr;
    }
    tracedClasses.finish();
    tracedGroups.finish();
}

bool
GCTrace::traceEnabled()
{
    return gcTraceFile != nullptr;
}

void
GCTrace::traceNurseryAlloc(Cell* thing, size_t size)
{
    if (thing) {
        /* We don't have AllocKind here, but we can work it out from size. */
        unsigned slots = (size - sizeof(JSObject)) / sizeof(JS::Value);
        AllocKind kind = GetBackgroundAllocKind(GetGCObjectKind(slots));
        TraceEvent(TraceEventNurseryAlloc, uint64_t(thing), uint8_t(kind));
    }
}

void
GCTrace::traceNurseryAlloc(Cell* thing, AllocKind kind)
{
    if (thing)
        TraceEvent(TraceEventNurseryAlloc, uint64_t(thing), uint8_t(kind));
}

void
GCTrace::traceTenuredAlloc(Cell* thing, AllocKind kind)
{
    if (thing)
        TraceEvent(TraceEventTenuredAlloc, uint64_t(thing), uint8_t(kind));
}

static void
MaybeTraceClass(const Class* clasp)
{
    if (tracedClasses.has(clasp))
        return;

    TraceEvent(TraceEventClassInfo, uint64_t(clasp));
    TraceString(clasp->name);
    TraceInt(clasp->flags);
    TraceInt(clasp->hasFinalize());

    MOZ_ALWAYS_TRUE(tracedClasses.put(clasp));
}

void
js::gc::GCTrace::maybeTraceGroup(ObjectGroup* group)
{
    if (tracedGroups.has(group))
        return;

    MaybeTraceClass(group->clasp());
    TraceEvent(TraceEventGroupInfo, uint64_t(group));
    TraceAddress(group->clasp());
    TraceInt(group->flagsDontCheckGeneration());

    MOZ_ALWAYS_TRUE(tracedGroups.put(group));
}

void
GCTrace::traceTypeNewScript(ObjectGroup* group)
{
    const size_t bufLength = 128;
    static char buffer[bufLength];

    JSAtom* funName = group->newScriptDontCheckGeneration()->function()->displayAtom();
    if (!funName)
        return;

    size_t length = funName->length();
    MOZ_ALWAYS_TRUE(length < bufLength);
    CopyChars(reinterpret_cast<Latin1Char*>(buffer), *funName);
    buffer[length] = 0;

    TraceEvent(TraceEventTypeNewScript, uint64_t(group));
    TraceString(buffer);
}

void
GCTrace::traceCreateObject(JSObject* object)
{
    if (!gcTraceFile)
        return;

    ObjectGroup* group = object->group();
    maybeTraceGroup(group);
    TraceEvent(TraceEventCreateObject, uint64_t(object));
    TraceAddress(group);
}

void
GCTrace::traceMinorGCStart()
{
    TraceEvent(TraceEventMinorGCStart);
}

void
GCTrace::tracePromoteToTenured(Cell* src, Cell* dst)
{
    TraceEvent(TraceEventPromoteToTenured, uint64_t(src));
    TraceAddress(dst);
}

void
GCTrace::traceMinorGCEnd()
{
    TraceEvent(TraceEventMinorGCEnd);
}

void
GCTrace::traceMajorGCStart()
{
    TraceEvent(TraceEventMajorGCStart);
}

void
GCTrace::traceTenuredFinalize(Cell* thing)
{
    if (!gcTraceFile)
        return;
    if (thing->asTenured().getAllocKind() == AllocKind::OBJECT_GROUP)
        tracedGroups.remove(static_cast<const ObjectGroup*>(thing));
    TraceEvent(TraceEventTenuredFinalize, uint64_t(thing));
}

void
GCTrace::traceMajorGCEnd()
{
    TraceEvent(TraceEventMajorGCEnd);
}

} // js
} // gc

#endif
