/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCTrace_h
#define gc_GCTrace_h

#include "gc/Heap.h"

namespace js {

class ObjectGroup;

namespace gc {

/*
 * Tracing code is declared within this class, so the class can be a friend of
 * something and access private details used for tracing.
 */
class GCTrace {
  public:
    GCTrace() { };

#ifdef JS_GC_TRACE

    MOZ_MUST_USE bool initTrace(GCRuntime& gc);
    void finishTrace();
    bool traceEnabled();
    void traceNurseryAlloc(Cell* thing, size_t size);
    void traceNurseryAlloc(Cell* thing, AllocKind kind);
    void traceTenuredAlloc(Cell* thing, AllocKind kind);
    void traceCreateObject(JSObject* object);
    void traceMinorGCStart();
    void tracePromoteToTenured(Cell* src, Cell* dst);
    void traceMinorGCEnd();
    void traceMajorGCStart();
    void traceTenuredFinalize(Cell* thing);
    void traceMajorGCEnd();
    void traceTypeNewScript(js::ObjectGroup* group);

  private:
    FILE* gcTraceFile = nullptr;

    HashSet<const Class*, DefaultHasher<const Class*>, SystemAllocPolicy> tracedClasses;
    HashSet<const ObjectGroup*, DefaultHasher<const ObjectGroup*>, SystemAllocPolicy> tracedGroups;

    void maybeTraceClass(const Class* clasp);
    void maybeTraceGroup(ObjectGroup* group);

#else

    MOZ_MUST_USE bool initTrace(GCRuntime& gc) { return true; }
    void finishTrace() {}
    bool traceEnabled() { return false; }
    void traceNurseryAlloc(Cell* thing, size_t size) {}
    void traceNurseryAlloc(Cell* thing, AllocKind kind) {}
    void traceTenuredAlloc(Cell* thing, AllocKind kind) {}
    void traceCreateObject(JSObject* object) {}
    void traceMinorGCStart() {}
    void tracePromoteToTenured(Cell* src, Cell* dst) {}
    void traceMinorGCEnd() {}
    void traceMajorGCStart() {}
    void traceTenuredFinalize(Cell* thing) {}
    void traceMajorGCEnd() {}
    void traceTypeNewScript(js::ObjectGroup* group) {}

#endif
}; /* GCTrace */

extern GCTrace gcTracer;

} /* namespace gc */
} /* namespace js */

#endif
