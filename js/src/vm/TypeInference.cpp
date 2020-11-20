/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/TypeInference-inl.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Sprintf.h"

#include <algorithm>
#include <new>
#include <utility>

#include "jsapi.h"

#include "gc/HashUtil.h"
#include "jit/BaselineIC.h"
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "jit/JitZone.h"
#include "js/MemoryMetrics.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "js/UniquePtr.h"
#include "util/DiagnosticAssertions.h"
#include "util/Poison.h"
#include "vm/FrameIter.h"  // js::AllScriptFramesIter
#include "vm/HelperThreads.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Opcodes.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Printer.h"
#include "vm/Shape.h"
#include "vm/Time.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ObjectGroup-inl.h"  // JSObject::setSingleton

using namespace js;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodArrayZero;
using mozilla::PodCopy;

using js::jit::JitScript;

bool js::ClassCanHaveExtraProperties(const JSClass* clasp) {
  return clasp->getResolve() || clasp->getOpsLookupProperty() ||
         clasp->getOpsGetProperty() || IsTypedArrayClass(clasp);
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

static inline void AssertGCStateForSweep(Zone* zone) {
  MOZ_ASSERT(zone->isGCSweepingOrCompacting());

  // IsAboutToBeFinalized doesn't work right on tenured objects when called
  // during a minor collection.
  MOZ_ASSERT(!JS::RuntimeHeapIsMinorCollecting());
}

/* static */
void JitScript::sweepTypes(const js::AutoSweepJitScript& sweep, Zone* zone) {
  MOZ_ASSERT(typesGeneration() != zone->types.generation);
  setTypesGeneration(zone->types.generation);

  AssertGCStateForSweep(zone);
}

TypeZone::TypeZone(Zone* zone)
    : zone_(zone),
      typeLifoAlloc_(zone, (size_t)TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      currentCompilationId_(zone),
      generation(zone, 0),
      sweepTypeLifoAlloc(zone, (size_t)TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
      sweepingTypes(zone, false),
      keepJitScripts(zone, false),
      activeAnalysis(zone, nullptr) {}

TypeZone::~TypeZone() {
  MOZ_RELEASE_ASSERT(!sweepingTypes);
  MOZ_ASSERT(!keepJitScripts);
}

void TypeZone::beginSweep() {
  MOZ_ASSERT(zone()->isGCSweepingOrCompacting());
  MOZ_ASSERT(
      !zone()->runtimeFromMainThread()->gc.storeBuffer().hasTypeSetPointers());

  // Clear the analysis pool, but don't release its data yet. While sweeping
  // types any live data will be allocated into the pool.
  sweepTypeLifoAlloc.ref().steal(&typeLifoAlloc());

  generation = !generation;
}

void TypeZone::endSweep(JSRuntime* rt) {
  rt->gc.queueAllLifoBlocksForFree(&sweepTypeLifoAlloc.ref());
}

JS::ubi::Node::Size JS::ubi::Concrete<js::ObjectGroup>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = js::gc::Arena::thingSize(get().asTenured().getAllocKind());
  return size;
}
