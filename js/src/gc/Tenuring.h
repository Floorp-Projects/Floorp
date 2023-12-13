/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Tenuring_h
#define gc_Tenuring_h

#include "mozilla/Maybe.h"

#include "gc/AllocKind.h"
#include "js/GCAPI.h"
#include "js/TracingAPI.h"
#include "util/Text.h"

namespace js {

class NativeObject;
class Nursery;
class PlainObject;

namespace wasm {
class AnyRef;
}  // namespace wasm

namespace gc {

class RelocationOverlay;
class StringRelocationOverlay;

template <typename Key>
struct DeduplicationStringHasher {
  using Lookup = Key;
  static inline HashNumber hash(const Lookup& lookup);
  static MOZ_ALWAYS_INLINE bool match(const Key& key, const Lookup& lookup);
};

class TenuringTracer final : public JSTracer {
  Nursery& nursery_;

  // Amount of data moved to the tenured generation during collection.
  size_t tenuredSize = 0;
  // Number of cells moved to the tenured generation.
  size_t tenuredCells = 0;

  // These lists are threaded through the Nursery using the space from
  // already moved things. The lists are used to fix up the moved things and
  // to find things held live by intra-Nursery pointers.
  gc::RelocationOverlay* objHead = nullptr;
  gc::StringRelocationOverlay* stringHead = nullptr;

  using StringDeDupSet =
      HashSet<JSString*, DeduplicationStringHasher<JSString*>,
              SystemAllocPolicy>;

  // deDupSet is emplaced at the beginning of the nursery collection and reset
  // at the end of the nursery collection. It can also be reset during nursery
  // collection when out of memory to insert new entries.
  mozilla::Maybe<StringDeDupSet> stringDeDupSet;

#define DEFINE_ON_EDGE_METHOD(name, type, _1, _2) \
  void on##name##Edge(type** thingp, const char* name) override;
  JS_FOR_EACH_TRACEKIND(DEFINE_ON_EDGE_METHOD)
#undef DEFINE_ON_EDGE_METHOD

 public:
  TenuringTracer(JSRuntime* rt, Nursery* nursery);

  Nursery& nursery() { return nursery_; }

  // Move all objects and everything they can reach to the tenured heap. Called
  // after all roots have been traced.
  void collectToObjectFixedPoint();

  // Move all strings and all strings they can reach to the tenured heap, and
  // additionally do any fixups for when strings are pointing into memory that
  // was deduplicated. Called after collectToObjectFixedPoint().
  void collectToStringFixedPoint();

  size_t getTenuredSize() const;
  size_t getTenuredCells() const;

  void traverse(JS::Value* thingp);
  void traverse(wasm::AnyRef* thingp);

  // The store buffers need to be able to call these directly.
  void traceObject(JSObject* obj);
  void traceObjectSlots(NativeObject* nobj, uint32_t start, uint32_t end);
  void traceSlots(JS::Value* vp, uint32_t nslots);
  void traceString(JSString* str);
  void traceBigInt(JS::BigInt* bi);

 private:
  // The dependent string chars needs to be relocated if the base which it's
  // using chars from has been deduplicated.
  template <typename CharT>
  void relocateDependentStringChars(JSDependentString* tenuredDependentStr,
                                    JSLinearString* baseOrRelocOverlay,
                                    size_t* offset,
                                    bool* rootBaseNotYetForwarded,
                                    JSLinearString** rootBase);

  inline void insertIntoObjectFixupList(gc::RelocationOverlay* entry);
  inline void insertIntoStringFixupList(gc::StringRelocationOverlay* entry);

  template <typename T>
  inline T* allocTenured(JS::Zone* zone, gc::AllocKind kind);
  JSString* allocTenuredString(JSString* src, JS::Zone* zone,
                               gc::AllocKind dstKind);

  inline JSObject* movePlainObjectToTenured(PlainObject* src);
  JSObject* moveToTenuredSlow(JSObject* src);
  JSString* moveToTenured(JSString* src);
  JS::BigInt* moveToTenured(JS::BigInt* src);

  size_t moveElementsToTenured(NativeObject* dst, NativeObject* src,
                               gc::AllocKind dstKind);
  size_t moveSlotsToTenured(NativeObject* dst, NativeObject* src);
  size_t moveStringToTenured(JSString* dst, JSString* src,
                             gc::AllocKind dstKind);
  size_t moveBigIntToTenured(JS::BigInt* dst, JS::BigInt* src,
                             gc::AllocKind dstKind);

  void traceSlots(JS::Value* vp, JS::Value* end);
};

}  // namespace gc
}  // namespace js

#endif  // gc_Tenuring_h
