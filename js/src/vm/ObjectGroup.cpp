/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ObjectGroup.h"

#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"

#include "jsexn.h"

#include "builtin/DataViewObject.h"
#include "gc/FreeOp.h"
#include "gc/HashUtil.h"
#include "gc/Policy.h"
#include "gc/StoreBuffer.h"
#include "js/CharacterEncoding.h"
#include "js/friend/WindowProxy.h"  // js::IsWindow
#include "js/UniquePtr.h"
#include "vm/ArrayObject.h"
#include "vm/ErrorObject.h"
#include "vm/GlobalObject.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/RegExpObject.h"
#include "vm/Shape.h"
#include "vm/TaggedProto.h"

#include "gc/Marking-inl.h"
#include "gc/ObjectKind-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

static ObjectGroup* MakeGroup(JSContext* cx, Handle<TaggedProto> proto) {
  MOZ_ASSERT_IF(proto.isObject(),
                cx->isInsideCurrentCompartment(proto.toObject()));

  ObjectGroup* group = Allocate<ObjectGroup>(cx);
  if (!group) {
    return nullptr;
  }
  new (group) ObjectGroup(proto);

  return group;
}

ObjectGroup::ObjectGroup(TaggedProto proto)
    : TenuredCellWithNonGCPointer(nullptr), proto_(proto) {
  /* Windows may not appear on prototype chains. */
  MOZ_ASSERT_IF(proto.isObject(), !IsWindow(proto.toObject()));
}

void ObjectGroup::setProtoUncheckedDeprecated(TaggedProto proto) {
  proto_ = proto;
  MOZ_ASSERT_IF(proto_.isObject() && proto_.toObject()->is<NativeObject>(),
                proto_.toObject()->isDelegate());
}

/////////////////////////////////////////////////////////////////////
// GlobalObject
/////////////////////////////////////////////////////////////////////

bool GlobalObject::shouldSplicePrototype() {
  // During bootstrapping, we need to make sure not to splice a new prototype in
  // for the global object if its __proto__ had previously been set to non-null,
  // as this will change the prototype for all other objects with the same type.
  return staticPrototype() == nullptr;
}

/* static */
bool GlobalObject::splicePrototype(JSContext* cx, Handle<GlobalObject*> global,
                                   Handle<TaggedProto> proto) {
  MOZ_ASSERT(cx->realm() == global->realm());

  // Windows may not appear on prototype chains.
  MOZ_ASSERT_IF(proto.isObject(), !IsWindow(proto.toObject()));

  if (proto.isObject()) {
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setDelegate(cx, protoObj)) {
      return false;
    }
  }

  return JSObject::setProtoUnchecked(cx, global, proto);
}

/////////////////////////////////////////////////////////////////////
// ObjectGroupRealm NewTable
/////////////////////////////////////////////////////////////////////

/*
 * Entries for the per-realm set of groups based on prototype and class. An
 * optional associated object is used which allows multiple groups to be
 * created with the same prototype. The associated object is a type descriptor
 * (for typed objects).
 */
struct ObjectGroupRealm::NewEntry {
  WeakHeapPtrObjectGroup group;

  explicit NewEntry(ObjectGroup* group) : group(group) {}

  struct Lookup {
    TaggedProto proto;

    explicit Lookup(TaggedProto proto) : proto(proto) {}

    explicit Lookup(const NewEntry& entry)
        : proto(entry.group.unbarrieredGet()->protoDeprecated()) {}
  };

  bool needsSweep() { return IsAboutToBeFinalized(&group); }

  bool operator==(const NewEntry& other) const { return group == other.group; }
};

namespace js {
template <>
struct MovableCellHasher<ObjectGroupRealm::NewEntry> {
  using Key = ObjectGroupRealm::NewEntry;
  using Lookup = ObjectGroupRealm::NewEntry::Lookup;

  static bool hasHash(const Lookup& l) {
    return MovableCellHasher<TaggedProto>::hasHash(l.proto);
  }

  static bool ensureHash(const Lookup& l) {
    return MovableCellHasher<TaggedProto>::ensureHash(l.proto);
  }

  static inline HashNumber hash(const Lookup& lookup) {
    return MovableCellHasher<TaggedProto>::hash(lookup.proto);
  }

  static inline bool match(const ObjectGroupRealm::NewEntry& key,
                           const Lookup& lookup) {
    TaggedProto proto = key.group.unbarrieredGet()->protoDeprecated();
    return MovableCellHasher<TaggedProto>::match(proto, lookup.proto);
  }
};
}  // namespace js

class ObjectGroupRealm::NewTable
    : public JS::WeakCache<js::GCHashSet<NewEntry, MovableCellHasher<NewEntry>,
                                         SystemAllocPolicy>> {
  using Table =
      js::GCHashSet<NewEntry, MovableCellHasher<NewEntry>, SystemAllocPolicy>;
  using Base = JS::WeakCache<Table>;

 public:
  explicit NewTable(Zone* zone) : Base(zone) {}
};

/* static*/ ObjectGroupRealm& ObjectGroupRealm::getForNewObject(JSContext* cx) {
  return cx->realm()->objectGroups_;
}

MOZ_ALWAYS_INLINE ObjectGroup* ObjectGroupRealm::DefaultNewGroupCache::lookup(
    const JSClass* clasp, TaggedProto proto) {
  if (group_ && group_->protoDeprecated() == proto) {
    return group_;
  }
  return nullptr;
}

/* static */
ObjectGroup* ObjectGroup::defaultNewGroup(JSContext* cx, const JSClass* clasp,
                                          TaggedProto proto) {
  MOZ_ASSERT(clasp);
  MOZ_ASSERT_IF(proto.isObject(),
                cx->isInsideCurrentCompartment(proto.toObject()));

  ObjectGroupRealm& groups = ObjectGroupRealm::getForNewObject(cx);

  if (ObjectGroup* group = groups.defaultNewGroupCache.lookup(clasp, proto)) {
    return group;
  }

  gc::AutoSuppressGC suppressGC(cx);

  ObjectGroupRealm::NewTable*& table = groups.defaultNewTable;

  if (!table) {
    table = cx->new_<ObjectGroupRealm::NewTable>(cx->zone());
    if (!table) {
      return nullptr;
    }
  }

  if (proto.isObject() && !proto.toObject()->isDelegate()) {
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setDelegate(cx, protoObj)) {
      return nullptr;
    }
  }

  ObjectGroupRealm::NewTable::AddPtr p =
      table->lookupForAdd(ObjectGroupRealm::NewEntry::Lookup(proto));
  if (p) {
    ObjectGroup* group = p->group;
    MOZ_ASSERT(group->protoDeprecated() == proto);
    groups.defaultNewGroupCache.put(group);
    return group;
  }

  Rooted<TaggedProto> protoRoot(cx, proto);
  ObjectGroup* group = MakeGroup(cx, protoRoot);
  if (!group) {
    return nullptr;
  }

  if (!table->add(p, ObjectGroupRealm::NewEntry(group))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  groups.defaultNewGroupCache.put(group);
  return group;
}

static bool AddPlainObjectProperties(JSContext* cx, HandlePlainObject obj,
                                     IdValuePair* properties,
                                     size_t nproperties) {
  RootedId propid(cx);
  RootedValue value(cx);

  for (size_t i = 0; i < nproperties; i++) {
    propid = properties[i].id;
    value = properties[i].value;
    if (!NativeDefineDataProperty(cx, obj, propid, value, JSPROP_ENUMERATE)) {
      return false;
    }
  }

  return true;
}

PlainObject* js::NewPlainObjectWithProperties(JSContext* cx,
                                              IdValuePair* properties,
                                              size_t nproperties,
                                              NewObjectKind newKind) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
  RootedPlainObject obj(
      cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind, newKind));
  if (!obj || !AddPlainObjectProperties(cx, obj, properties, nproperties)) {
    return nullptr;
  }
  return obj;
}

/////////////////////////////////////////////////////////////////////
// ObjectGroupRealm
/////////////////////////////////////////////////////////////////////

ObjectGroupRealm::~ObjectGroupRealm() { js_delete(defaultNewTable); }

void ObjectGroupRealm::addSizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf, size_t* realmTables) {
  if (defaultNewTable) {
    *realmTables += defaultNewTable->sizeOfIncludingThis(mallocSizeOf);
  }
}

void ObjectGroupRealm::clearTables() {
  if (defaultNewTable) {
    defaultNewTable->clear();
  }
  defaultNewGroupCache.purge();
}

void ObjectGroupRealm::fixupNewTableAfterMovingGC(NewTable* table) {
  /*
   * Each entry's hash depends on the object's prototype and we can't tell
   * whether that has been moved or not in sweepNewObjectGroupTable().
   */
  if (table) {
    for (NewTable::Enum e(*table); !e.empty(); e.popFront()) {
      NewEntry& entry = e.mutableFront();

      ObjectGroup* group = entry.group.unbarrieredGet();
      if (IsForwarded(group)) {
        group = Forwarded(group);
        entry.group.set(group);
      }
      TaggedProto proto = group->protoDeprecated();
      if (proto.isObject() && IsForwarded(proto.toObject())) {
        proto = TaggedProto(Forwarded(proto.toObject()));
        // Update the group's proto here so that we are able to lookup
        // entries in this table before all object pointers are updated.
        group->protoDeprecated() = proto;
      }
    }
  }
}

#ifdef JSGC_HASH_TABLE_CHECKS

void ObjectGroupRealm::checkNewTableAfterMovingGC(NewTable* table) {
  /*
   * Assert that nothing points into the nursery or needs to be relocated, and
   * that the hash table entries are discoverable.
   */
  if (!table) {
    return;
  }

  for (auto r = table->all(); !r.empty(); r.popFront()) {
    NewEntry entry = r.front();
    CheckGCThingAfterMovingGC(entry.group.unbarrieredGet());
    TaggedProto proto = entry.group.unbarrieredGet()->protoDeprecated();
    if (proto.isObject()) {
      CheckGCThingAfterMovingGC(proto.toObject());
    }

    auto ptr = table->lookup(NewEntry::Lookup(entry));
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &r.front());
  }
}

#endif  // JSGC_HASH_TABLE_CHECKS

JS::ubi::Node::Size JS::ubi::Concrete<js::ObjectGroup>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  Size size = js::gc::Arena::thingSize(get().asTenured().getAllocKind());
  return size;
}
