/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#ifdef JS_GC_TRACE

JS_STATIC_ASSERT(NumAllocKinds == unsigned(AllocKind::LIMIT));
JS_STATIC_ASSERT(LastObjectAllocKind == unsigned(AllocKind::OBJECT_LAST));

static inline void
WriteWord(FILE *file, uint64_t data)
{
    if (file)
        fwrite(&data, sizeof(data), 1, file);
}

static inline void
TraceEvent(FILE *file, GCTraceEvent event, uint64_t payload = 0,
    uint8_t extra = 0)
{
    MOZ_ASSERT(event < GCTraceEventCount);
    MOZ_ASSERT((payload >> TracePayloadBits) == 0);
    WriteWord(file, (uint64_t(event) << TraceEventShift) |
               (uint64_t(extra) << TraceExtraShift) | payload);
}

static inline void
TraceAddress(FILE *file, const void* p)
{
    TraceEvent(file, TraceDataAddress, uint64_t(p));
}

static inline void
TraceInt(FILE *file, uint32_t data)
{
    TraceEvent(file, TraceDataInt, data);
}

static void
TraceString(FILE *file, const char* string)
{
    JS_STATIC_ASSERT(sizeof(char) == 1);

    size_t length = strlen(string);
    const unsigned charsPerWord = sizeof(uint64_t);
    unsigned wordCount = (length + charsPerWord - 1) / charsPerWord;

    TraceEvent(file, TraceDataString, length);
    for (unsigned i = 0; i < wordCount; ++i) {
        union
        {
            uint64_t word;
            char chars[charsPerWord];
        } data;
        strncpy(data.chars, string + (i * charsPerWord), charsPerWord);
        WriteWord(file, data.word);
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

    TraceEvent(gcTraceFile, TraceEventInit, 0, TraceFormatVersion);

    /* Trace information about thing sizes. */
    for (auto kind : AllAllocKinds())
        TraceEvent(gcTraceFile, TraceEventThingSize, Arena::thingSize(kind));

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
        TraceEvent(gcTraceFile, TraceEventNurseryAlloc, uint64_t(thing),
            uint8_t(kind));
    }
}

void
GCTrace::traceNurseryAlloc(Cell* thing, AllocKind kind)
{
    if (thing) {
        TraceEvent(gcTraceFile, TraceEventNurseryAlloc, uint64_t(thing),
            uint8_t(kind));
    }
}

void
GCTrace::traceTenuredAlloc(Cell* thing, AllocKind kind)
{
    if (thing) {
        TraceEvent(gcTraceFile, TraceEventTenuredAlloc, uint64_t(thing),
            uint8_t(kind));
    }
}

void
js::gc::GCTrace::maybeTraceClass(const Class* clasp)
{
    if (tracedClasses.has(clasp))
        return;

    TraceEvent(gcTraceFile, TraceEventClassInfo, uint64_t(clasp));
    TraceString(gcTraceFile, clasp->name);
    TraceInt(gcTraceFile, clasp->flags);
    TraceInt(gcTraceFile, clasp->hasFinalize());

    MOZ_ALWAYS_TRUE(tracedClasses.put(clasp));
}

void
js::gc::GCTrace::maybeTraceGroup(ObjectGroup* group)
{
    if (tracedGroups.has(group))
        return;

    maybeTraceClass(group->clasp());
    TraceEvent(gcTraceFile, TraceEventGroupInfo, uint64_t(group));
    TraceAddress(gcTraceFile, group->clasp());
    TraceInt(gcTraceFile, group->flagsDontCheckGeneration());

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

    TraceEvent(gcTraceFile, TraceEventTypeNewScript, uint64_t(group));
    TraceString(gcTraceFile, buffer);
}

void
GCTrace::traceCreateObject(JSObject* object)
{
    if (!gcTraceFile)
        return;

    ObjectGroup* group = object->group();
    maybeTraceGroup(group);
    TraceEvent(gcTraceFile, TraceEventCreateObject, uint64_t(object));
    TraceAddress(gcTraceFile, group);
}

void
GCTrace::traceMinorGCStart()
{
    TraceEvent(gcTraceFile, TraceEventMinorGCStart);
}

void
GCTrace::tracePromoteToTenured(Cell* src, Cell* dst)
{
    TraceEvent(gcTraceFile, TraceEventPromoteToTenured, uint64_t(src));
    TraceAddress(gcTraceFile, dst);
}

void
GCTrace::traceMinorGCEnd()
{
    TraceEvent(gcTraceFile, TraceEventMinorGCEnd);
}

void
GCTrace::traceMajorGCStart()
{
    TraceEvent(gcTraceFile, TraceEventMajorGCStart);
}

void
GCTrace::traceTenuredFinalize(Cell* thing)
{
    if (!gcTraceFile)
        return;
    if (thing->asTenured().getAllocKind() == AllocKind::OBJECT_GROUP)
        tracedGroups.remove(static_cast<const ObjectGroup*>(thing));
    TraceEvent(gcTraceFile, TraceEventTenuredFinalize, uint64_t(thing));
}

void
GCTrace::traceMajorGCEnd()
{
    TraceEvent(gcTraceFile, TraceEventMajorGCEnd);
}

#endif

} // js
} // gc
