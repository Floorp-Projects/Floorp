/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectGroup_h
#define vm_ObjectGroup_h

#include "jsfriendapi.h"

#include "ds/IdValuePair.h"
#include "gc/Allocator.h"
#include "gc/Barrier.h"
#include "gc/GCProbes.h"
#include "js/CharacterEncoding.h"
#include "js/GCHashTable.h"
#include "js/TypeDecls.h"
#include "js/UbiNode.h"
#include "vm/TaggedProto.h"

namespace js {

class ObjectGroupRealm;
class PlainObject;

/*
 * The NewObjectKind allows an allocation site to specify the lifetime
 * requirements that must be fixed at allocation time.
 */
enum NewObjectKind {
  /* This is the default. Most objects are generic. */
  GenericObject,

  /*
   * Objects which will not benefit from being allocated in the nursery
   * (e.g. because they are known to have a long lifetime) may be allocated
   * with this kind to place them immediately into the tenured generation.
   */
  TenuredObject
};

// Note: for now just store an int* in the CellHeader. We can't store the Realm*
// because it's an incomplete type and we can't include vm/Realm.h here. This
// class will be removed shortly anyway.
class ObjectGroup : public gc::TenuredCellWithNonGCPointer<int> {
  /* Prototype shared by objects in this group. */
  GCPtr<TaggedProto> proto_;  // set by constructor

#ifndef JS_64BIT
  // Temporary padding to respect MinCellSize.
  uint64_t padding_ = 0;
#endif

  // END OF PROPERTIES

 private:
  static inline uint32_t offsetOfProto() {
    return offsetof(ObjectGroup, proto_);
  }

  friend class gc::GCRuntime;

  // See JSObject::offsetOfGroup() comment.
  friend class js::jit::MacroAssembler;

 public:
  inline explicit ObjectGroup(TaggedProto proto);

  const GCPtr<TaggedProto>& proto() const { return proto_; }

  GCPtr<TaggedProto>& proto() { return proto_; }

  void setProtoUnchecked(TaggedProto proto);

  /* Helpers */

  void traceChildren(JSTracer* trc);

  void finalize(JSFreeOp* fop) {
    // Nothing to do.
  }

  static const JS::TraceKind TraceKind = JS::TraceKind::ObjectGroup;

  static ObjectGroup* defaultNewGroup(JSContext* cx, const JSClass* clasp,
                                      TaggedProto proto);
};

// Structure used to manage the groups in a realm.
class ObjectGroupRealm {
 private:
  class NewTable;

 private:
  // Set of default 'new' groups in the realm.
  NewTable* defaultNewTable = nullptr;

  // This cache is purged on GC.
  class DefaultNewGroupCache {
    ObjectGroup* group_;

   public:
    DefaultNewGroupCache() { purge(); }

    void purge() { group_ = nullptr; }
    void put(ObjectGroup* group) { group_ = group; }

    MOZ_ALWAYS_INLINE ObjectGroup* lookup(const JSClass* clasp,
                                          TaggedProto proto);
  } defaultNewGroupCache = {};

  // END OF PROPERTIES

 private:
  friend class ObjectGroup;

 public:
  struct NewEntry;

  ObjectGroupRealm() = default;
  ~ObjectGroupRealm();

  ObjectGroupRealm(ObjectGroupRealm&) = delete;
  void operator=(ObjectGroupRealm&) = delete;

  static ObjectGroupRealm& getForNewObject(JSContext* cx);

  void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              size_t* realmTables);

  void clearTables();

  void purge() { defaultNewGroupCache.purge(); }

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkTablesAfterMovingGC() {
    checkNewTableAfterMovingGC(defaultNewTable);
  }
#endif

  void fixupTablesAfterMovingGC() {
    fixupNewTableAfterMovingGC(defaultNewTable);
  }

 private:
#ifdef JSGC_HASH_TABLE_CHECKS
  void checkNewTableAfterMovingGC(NewTable* table);
#endif

  void fixupNewTableAfterMovingGC(NewTable* table);
};

PlainObject* NewPlainObjectWithProperties(JSContext* cx,
                                          IdValuePair* properties,
                                          size_t nproperties,
                                          NewObjectKind newKind);

}  // namespace js

// JS::ubi::Nodes can point to object groups; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {

template <>
class Concrete<js::ObjectGroup> : TracerConcrete<js::ObjectGroup> {
 protected:
  explicit Concrete(js::ObjectGroup* ptr)
      : TracerConcrete<js::ObjectGroup>(ptr) {}

 public:
  static void construct(void* storage, js::ObjectGroup* ptr) {
    new (storage) Concrete(ptr);
  }

  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};

}  // namespace ubi
}  // namespace JS

#endif /* vm_ObjectGroup_h */
