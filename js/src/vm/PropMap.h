/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_PropMap_h
#define vm_PropMap_h

#include "gc/Cell.h"
#include "js/UbiNode.h"

namespace js {

class DictionaryPropMap;
class SharedPropMap;
class LinkedPropMap;
class CompactPropMap;
class NormalPropMap;

class PropMap : public gc::TenuredCellWithFlags {
 public:
  static constexpr size_t Capacity = 8;

 protected:
  // XXX temporary
  uint64_t padding1;
  uint64_t padding2;

  static_assert(gc::CellFlagBitsReservedForGC == 3,
                "PropMap must reserve enough bits for Cell");
  // Flags word, stored in the cell header. Note that this hides the
  // Cell::flags() method.
  uintptr_t flags() const { return headerFlagsField(); }

 public:
  static const JS::TraceKind TraceKind = JS::TraceKind::PropMap;

  void traceChildren(JSTracer* trc);
};

class SharedPropMap : public PropMap {
 public:
  void fixupAfterMovingGC();
  void sweep(JSFreeOp* fop);
  void finalize(JSFreeOp* fop);
};

class CompactPropMap final : public SharedPropMap {};

class LinkedPropMap final : public PropMap {};

class NormalPropMap final : public SharedPropMap {};

class DictionaryPropMap final : public PropMap {
 public:
  void fixupAfterMovingGC();
  void sweep(JSFreeOp* fop);
  void finalize(JSFreeOp* fop);
};

}  // namespace js

// JS::ubi::Nodes can point to PropMaps; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::PropMap> : TracerConcrete<js::PropMap> {
 protected:
  explicit Concrete(js::PropMap* ptr) : TracerConcrete<js::PropMap>(ptr) {}

 public:
  static void construct(void* storage, js::PropMap* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif  // vm_PropMap_h
