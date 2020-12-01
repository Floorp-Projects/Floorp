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
#include "vm/TypeInference-inl.h"

using namespace js;

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

ObjectGroup::ObjectGroup(const JSClass* clasp, TaggedProto proto,
                         JS::Realm* realm, ObjectGroupFlags initialFlags)
    : TenuredCellWithNonGCPointer(clasp),
      proto_(proto),
      realm_(realm),
      flags_(initialFlags) {
  /* Windows may not appear on prototype chains. */
  MOZ_ASSERT_IF(proto.isObject(), !IsWindow(proto.toObject()));
  MOZ_ASSERT(JS::StringIsASCII(clasp->name));

#ifdef DEBUG
  GlobalObject* global = realm->unsafeUnbarrieredMaybeGlobal();
  if (global) {
    AssertTargetIsNotGray(global);
  }
#endif
}

void ObjectGroup::setProtoUnchecked(TaggedProto proto) {
  proto_ = proto;
  MOZ_ASSERT_IF(proto_.isObject() && proto_.toObject()->isNative(),
                proto_.toObject()->isDelegate());
}

void ObjectGroup::setProto(TaggedProto proto) {
  MOZ_ASSERT(singleton());
  setProtoUnchecked(proto);
}

/////////////////////////////////////////////////////////////////////
// JSObject
/////////////////////////////////////////////////////////////////////

bool GlobalObject::shouldSplicePrototype() {
  // During bootstrapping, we need to make sure not to splice a new prototype in
  // for the global object if its __proto__ had previously been set to non-null,
  // as this will change the prototype for all other objects with the same type.
  return staticPrototype() == nullptr;
}

/* static */
bool JSObject::splicePrototype(JSContext* cx, HandleObject obj,
                               Handle<TaggedProto> proto) {
  MOZ_ASSERT(cx->compartment() == obj->compartment());

  /*
   * For singleton groups representing only a single JSObject, the proto
   * can be rearranged as needed without destroying type information for
   * the old or new types.
   */
  MOZ_ASSERT(obj->isSingleton());

  // Windows may not appear on prototype chains.
  MOZ_ASSERT_IF(proto.isObject(), !IsWindow(proto.toObject()));

#ifdef DEBUG
  const JSClass* oldClass = obj->getClass();
#endif

  if (proto.isObject()) {
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setDelegate(cx, protoObj)) {
      return false;
    }
  }

  // Force type instantiation when splicing lazy group.
  RootedObjectGroup group(cx, JSObject::getGroup(cx, obj));
  if (!group) {
    return false;
  }
  RootedObjectGroup protoGroup(cx, nullptr);
  if (proto.isObject()) {
    RootedObject protoObj(cx, proto.toObject());
    protoGroup = JSObject::getGroup(cx, protoObj);
    if (!protoGroup) {
      return false;
    }
  }

  MOZ_ASSERT(group->clasp() == oldClass,
             "splicing a prototype doesn't change a group's class");
  group->setProto(proto);
  return true;
}

/* static */
ObjectGroup* JSObject::makeLazyGroup(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(obj->hasLazyGroup());
  MOZ_ASSERT(cx->compartment() == obj->compartment());

  // Find flags which need to be specified immediately on the object.
  ObjectGroupFlags initialFlags = OBJECT_FLAG_SINGLETON;

  Rooted<TaggedProto> proto(cx, obj->taggedProto());
  ObjectGroup* group = ObjectGroupRealm::makeGroup(
      cx, obj->nonCCWRealm(), obj->getClass(), proto, initialFlags);
  if (!group) {
    return nullptr;
  }

  obj->setGroupRaw(group);

  return group;
}

/////////////////////////////////////////////////////////////////////
// ObjectGroupRealm NewTable
/////////////////////////////////////////////////////////////////////

/*
 * Entries for the per-realm set of groups which are the default
 * types to use for some prototype. An optional associated object is used which
 * allows multiple groups to be created with the same prototype. The
 * associated object may be a function (for types constructed with 'new') or a
 * type descriptor (for typed objects). These entries are also used for the set
 * of lazy groups in the realm, which use a null associated object
 * (though there are only a few of these per realm).
 */
struct ObjectGroupRealm::NewEntry {
  WeakHeapPtrObjectGroup group;

  // Note: This pointer is only used for equality and does not need a read
  // barrier.
  JSObject* associated;

  NewEntry(ObjectGroup* group, JSObject* associated)
      : group(group), associated(associated) {}

  struct Lookup {
    const JSClass* clasp;
    TaggedProto proto;
    JSObject* associated;

    Lookup(const JSClass* clasp, TaggedProto proto, JSObject* associated)
        : clasp(clasp), proto(proto), associated(associated) {
      MOZ_ASSERT(clasp);
      MOZ_ASSERT_IF(associated && associated->is<JSFunction>(),
                    clasp == &PlainObject::class_);
    }

    explicit Lookup(const NewEntry& entry)
        : clasp(entry.group.unbarrieredGet()->clasp()),
          proto(entry.group.unbarrieredGet()->proto()),
          associated(entry.associated) {
      MOZ_ASSERT_IF(associated && associated->is<JSFunction>(),
                    clasp == &PlainObject::class_);
    }
  };

  bool needsSweep() {
    return IsAboutToBeFinalized(&group) ||
           (associated && IsAboutToBeFinalizedUnbarriered(&associated));
  }

  bool operator==(const NewEntry& other) const {
    return group == other.group && associated == other.associated;
  }
};

namespace js {
template <>
struct MovableCellHasher<ObjectGroupRealm::NewEntry> {
  using Key = ObjectGroupRealm::NewEntry;
  using Lookup = ObjectGroupRealm::NewEntry::Lookup;

  static bool hasHash(const Lookup& l) {
    return MovableCellHasher<TaggedProto>::hasHash(l.proto) &&
           MovableCellHasher<JSObject*>::hasHash(l.associated);
  }

  static bool ensureHash(const Lookup& l) {
    return MovableCellHasher<TaggedProto>::ensureHash(l.proto) &&
           MovableCellHasher<JSObject*>::ensureHash(l.associated);
  }

  static inline HashNumber hash(const Lookup& lookup) {
    HashNumber hash = MovableCellHasher<TaggedProto>::hash(lookup.proto);
    hash = mozilla::AddToHash(
        hash, MovableCellHasher<JSObject*>::hash(lookup.associated));
    return mozilla::AddToHash(hash, mozilla::HashGeneric(lookup.clasp));
  }

  static inline bool match(const ObjectGroupRealm::NewEntry& key,
                           const Lookup& lookup) {
    if (key.group.unbarrieredGet()->clasp() != lookup.clasp) {
      return false;
    }

    TaggedProto proto = key.group.unbarrieredGet()->proto();
    if (!MovableCellHasher<TaggedProto>::match(proto, lookup.proto)) {
      return false;
    }

    return MovableCellHasher<JSObject*>::match(key.associated,
                                               lookup.associated);
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

/* static*/ ObjectGroupRealm& ObjectGroupRealm::get(const ObjectGroup* group) {
  return group->realm()->objectGroups_;
}

/* static*/ ObjectGroupRealm& ObjectGroupRealm::getForNewObject(JSContext* cx) {
  return cx->realm()->objectGroups_;
}

MOZ_ALWAYS_INLINE ObjectGroup* ObjectGroupRealm::DefaultNewGroupCache::lookup(
    const JSClass* clasp, TaggedProto proto, JSObject* associated) {
  if (group_ && associated_ == associated && group_->proto() == proto &&
      group_->clasp() == clasp) {
    return group_;
  }
  return nullptr;
}

/* static */
ObjectGroup* ObjectGroup::defaultNewGroup(JSContext* cx, const JSClass* clasp,
                                          TaggedProto proto,
                                          JSObject* associated) {
  MOZ_ASSERT(clasp);
  MOZ_ASSERT_IF(associated, proto.isObject());
  MOZ_ASSERT_IF(proto.isObject(),
                cx->isInsideCurrentCompartment(proto.toObject()));

  if (associated && !associated->is<TypeDescr>()) {
    associated = nullptr;
  }

  if (associated) {
    MOZ_ASSERT(associated->is<TypeDescr>());
    if (!IsTypedObjectClass(clasp)) {
      // This can happen when we call Reflect.construct with a TypeDescr as
      // newTarget argument. We're not creating a TypedObject in this case, so
      // don't set the TypeDescr on the group.
      associated = nullptr;
    }
  }

  ObjectGroupRealm& groups = ObjectGroupRealm::getForNewObject(cx);

  if (ObjectGroup* group =
          groups.defaultNewGroupCache.lookup(clasp, proto, associated)) {
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

  ObjectGroupRealm::NewTable::AddPtr p = table->lookupForAdd(
      ObjectGroupRealm::NewEntry::Lookup(clasp, proto, associated));
  if (p) {
    ObjectGroup* group = p->group;
    MOZ_ASSERT(group->clasp() == clasp);
    MOZ_ASSERT(group->proto() == proto);
    groups.defaultNewGroupCache.put(group, associated);
    return group;
  }

  ObjectGroupFlags initialFlags = 0;

  Rooted<TaggedProto> protoRoot(cx, proto);
  ObjectGroup* group = ObjectGroupRealm::makeGroup(cx, cx->realm(), clasp,
                                                   protoRoot, initialFlags);
  if (!group) {
    return nullptr;
  }

  if (!table->add(p, ObjectGroupRealm::NewEntry(group, associated))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  if (associated) {
    group->setTypeDescr(&associated->as<TypeDescr>());
  }

  groups.defaultNewGroupCache.put(group, associated);
  return group;
}

/* static */
ObjectGroup* ObjectGroup::lazySingletonGroup(JSContext* cx,
                                             ObjectGroupRealm& realm,
                                             JS::Realm* objectRealm,
                                             const JSClass* clasp,
                                             TaggedProto proto) {
  MOZ_ASSERT_IF(proto.isObject(),
                cx->compartment() == proto.toObject()->compartment());

  ObjectGroupRealm::NewTable*& table = realm.lazyTable;

  if (!table) {
    table = cx->new_<ObjectGroupRealm::NewTable>(cx->zone());
    if (!table) {
      return nullptr;
    }
  }

  ObjectGroupRealm::NewTable::AddPtr p = table->lookupForAdd(
      ObjectGroupRealm::NewEntry::Lookup(clasp, proto, nullptr));
  if (p) {
    ObjectGroup* group = p->group;
    MOZ_ASSERT(group->lazy());

    return group;
  }

  gc::AutoSuppressGC suppressGC(cx);

  Rooted<TaggedProto> protoRoot(cx, proto);
  ObjectGroup* group = ObjectGroupRealm::makeGroup(
      cx, objectRealm, clasp, protoRoot,
      OBJECT_FLAG_SINGLETON | OBJECT_FLAG_LAZY_SINGLETON);
  if (!group) {
    return nullptr;
  }

  if (!table->add(p, ObjectGroupRealm::NewEntry(group, nullptr))) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return group;
}

inline const JSClass* GetClassForProtoKey(JSProtoKey key) {
  switch (key) {
    case JSProto_Null:
    case JSProto_Object:
      return &PlainObject::class_;
    case JSProto_Array:
      return &ArrayObject::class_;

    case JSProto_Int8Array:
    case JSProto_Uint8Array:
    case JSProto_Int16Array:
    case JSProto_Uint16Array:
    case JSProto_Int32Array:
    case JSProto_Uint32Array:
    case JSProto_Float32Array:
    case JSProto_Float64Array:
    case JSProto_Uint8ClampedArray:
    case JSProto_BigInt64Array:
    case JSProto_BigUint64Array:
      return &TypedArrayObject::classes[key - JSProto_Int8Array];

    default:
      // We only expect to see plain objects, arrays, and typed arrays here.
      MOZ_CRASH("Bad proto key");
  }
}

/* static */
ObjectGroup* ObjectGroup::defaultNewGroup(JSContext* cx, JSProtoKey key) {
  JSObject* proto = nullptr;
  if (key != JSProto_Null) {
    proto = GlobalObject::getOrCreatePrototype(cx, key);
    if (!proto) {
      return nullptr;
    }
  }
  return defaultNewGroup(cx, GetClassForProtoKey(key), TaggedProto(proto));
}

/* static */
ArrayObject* ObjectGroup::newArrayObject(JSContext* cx, const Value* vp,
                                         size_t length, NewObjectKind newKind,
                                         NewArrayKind arrayKind) {
  MOZ_ASSERT(newKind != SingletonObject);

  // If we are making a copy on write array, don't try to adjust the group as
  // getOrFixupCopyOnWriteObject will do this before any objects are copied
  // from this one.
  if (arrayKind == NewArrayKind::CopyOnWrite) {
    ArrayObject* obj = NewDenseCopiedArray(cx, length, vp, nullptr, newKind);
    if (!obj || !ObjectElements::MakeElementsCopyOnWrite(cx, obj)) {
      return nullptr;
    }
    return obj;
  }

  return NewDenseCopiedArray(cx, length, vp, nullptr, newKind);
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

/* static */
JSObject* ObjectGroup::newPlainObject(JSContext* cx, IdValuePair* properties,
                                      size_t nproperties,
                                      NewObjectKind newKind) {
  return NewPlainObjectWithProperties(cx, properties, nproperties, newKind);
}

/* static */
ObjectGroup* ObjectGroup::allocationSiteGroup(
    JSContext* cx, JSScript* scriptArg, jsbytecode* pc, JSProtoKey kind,
    HandleObject protoArg /* = nullptr */) {
  MOZ_ASSERT_IF(protoArg, kind == JSProto_Array);
  MOZ_ASSERT(cx->realm() == scriptArg->realm());

  if (protoArg) {
    return defaultNewGroup(cx, GetClassForProtoKey(kind),
                           TaggedProto(protoArg));
  }
  return defaultNewGroup(cx, kind);
}

/* static */
ObjectGroup* ObjectGroup::callingAllocationSiteGroup(JSContext* cx,
                                                     JSProtoKey key,
                                                     HandleObject proto) {
  MOZ_ASSERT_IF(proto, key == JSProto_Array);

  if (proto) {
    return defaultNewGroup(cx, GetClassForProtoKey(key), TaggedProto(proto));
  }
  return defaultNewGroup(cx, key);
}

/* static */
ArrayObject* ObjectGroup::getOrFixupCopyOnWriteObject(JSContext* cx,
                                                      HandleScript script,
                                                      jsbytecode* pc) {
  MOZ_CRASH("TODO(no-TI): remove");
}

/* static */
ArrayObject* ObjectGroup::getCopyOnWriteObject(JSScript* script,
                                               jsbytecode* pc) {
  // getOrFixupCopyOnWriteObject should already have been called for
  // script/pc, ensuring that the template object has a group with the
  // COPY_ON_WRITE flag. We don't assert this here, due to a corner case
  // where this property doesn't hold. See jsop_newarray_copyonwrite in
  // IonBuilder.
  ArrayObject* obj = &script->getObject(pc)->as<ArrayObject>();
  MOZ_ASSERT(obj->denseElementsAreCopyOnWrite());

  return obj;
}

/////////////////////////////////////////////////////////////////////
// ObjectGroupRealm
/////////////////////////////////////////////////////////////////////

ObjectGroupRealm::~ObjectGroupRealm() {
  js_delete(defaultNewTable);
  js_delete(lazyTable);
  stringSplitStringGroup = nullptr;
}

/* static */
ObjectGroup* ObjectGroupRealm::makeGroup(
    JSContext* cx, Realm* realm, const JSClass* clasp,
    Handle<TaggedProto> proto, ObjectGroupFlags initialFlags /* = 0 */) {
  MOZ_ASSERT_IF(proto.isObject(),
                cx->isInsideCurrentCompartment(proto.toObject()));

  ObjectGroup* group = Allocate<ObjectGroup>(cx);
  if (!group) {
    return nullptr;
  }
  new (group) ObjectGroup(clasp, proto, realm, initialFlags);

  return group;
}

/* static */
ObjectGroup* ObjectGroupRealm::getStringSplitStringGroup(JSContext* cx) {
  ObjectGroupRealm& groups = ObjectGroupRealm::getForNewObject(cx);

  ObjectGroup* group = groups.stringSplitStringGroup.get();
  if (group) {
    return group;
  }

  // The following code is a specialized version of the code
  // for ObjectGroup::allocationSiteGroup().

  JSObject* proto = GlobalObject::getOrCreateArrayPrototype(cx, cx->global());
  if (!proto) {
    return nullptr;
  }
  Rooted<TaggedProto> tagged(cx, TaggedProto(proto));

  group = makeGroup(cx, cx->realm(), &ArrayObject::class_, tagged);
  if (!group) {
    return nullptr;
  }

  groups.stringSplitStringGroup.set(group);
  return group;
}

void ObjectGroupRealm::addSizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf, size_t* allocationSiteTables,
    size_t* arrayObjectGroupTables, size_t* plainObjectGroupTables,
    size_t* realmTables) {
  // TODO(no-TI): remove unused arguments.

  if (defaultNewTable) {
    *realmTables += defaultNewTable->sizeOfIncludingThis(mallocSizeOf);
  }

  if (lazyTable) {
    *realmTables += lazyTable->sizeOfIncludingThis(mallocSizeOf);
  }
}

void ObjectGroupRealm::clearTables() {
  if (defaultNewTable) {
    defaultNewTable->clear();
  }
  if (lazyTable) {
    lazyTable->clear();
  }
  defaultNewGroupCache.purge();
}

void ObjectGroupRealm::traceWeak(JSTracer* trc) {
  if (stringSplitStringGroup) {
    JS::GCPolicy<WeakHeapPtrObjectGroup>::traceWeak(trc,
                                                    &stringSplitStringGroup);
  }
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
      TaggedProto proto = group->proto();
      if (proto.isObject() && IsForwarded(proto.toObject())) {
        proto = TaggedProto(Forwarded(proto.toObject()));
        // Update the group's proto here so that we are able to lookup
        // entries in this table before all object pointers are updated.
        group->proto() = proto;
      }
      if (entry.associated && IsForwarded(entry.associated)) {
        entry.associated = Forwarded(entry.associated);
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
    TaggedProto proto = entry.group.unbarrieredGet()->proto();
    if (proto.isObject()) {
      CheckGCThingAfterMovingGC(proto.toObject());
    }
    CheckGCThingAfterMovingGC(entry.associated);

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
