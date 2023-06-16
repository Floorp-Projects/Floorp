/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private maps (hashtables). */

#ifndef xpcmaps_h___
#define xpcmaps_h___

#include "mozilla/AllocPolicy.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/HashTable.h"

#include "js/GCHashTable.h"

/***************************************************************************/
// default initial sizes for maps (hashtables)

#define XPC_JS_MAP_LENGTH 32

#define XPC_NATIVE_MAP_LENGTH 8
#define XPC_NATIVE_PROTO_MAP_LENGTH 8
#define XPC_DYING_NATIVE_PROTO_MAP_LENGTH 8
#define XPC_NATIVE_INTERFACE_MAP_LENGTH 32
#define XPC_NATIVE_SET_MAP_LENGTH 32
#define XPC_WRAPPER_MAP_LENGTH 8

/*************************/

class JSObject2WrappedJSMap {
  using Map = js::HashMap<JS::Heap<JSObject*>, nsXPCWrappedJS*,
                          js::StableCellHasher<JS::Heap<JSObject*>>,
                          InfallibleAllocPolicy>;

 public:
  JSObject2WrappedJSMap() = default;

  inline nsXPCWrappedJS* Find(JSObject* Obj) {
    MOZ_ASSERT(Obj, "bad param");
    Map::Ptr p = mTable.lookup(Obj);
    return p ? p->value() : nullptr;
  }

#ifdef DEBUG
  inline bool HasWrapper(nsXPCWrappedJS* wrapper) {
    for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
      if (iter.get().value() == wrapper) {
        return true;
      }
    }
    return false;
  }
#endif

  inline nsXPCWrappedJS* Add(JSContext* cx, nsXPCWrappedJS* wrapper) {
    MOZ_ASSERT(wrapper, "bad param");
    JSObject* obj = wrapper->GetJSObjectPreserveColor();
    Map::AddPtr p = mTable.lookupForAdd(obj);
    if (p) {
      return p->value();
    }
    if (!mTable.add(p, obj, wrapper)) {
      return nullptr;
    }
    return wrapper;
  }

  inline void Remove(nsXPCWrappedJS* wrapper) {
    MOZ_ASSERT(wrapper, "bad param");
    mTable.remove(wrapper->GetJSObjectPreserveColor());
  }

  inline uint32_t Count() { return mTable.count(); }

  inline void Dump(int16_t depth) {
    for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
      iter.get().value()->DebugDump(depth);
    }
  }

  void UpdateWeakPointersAfterGC(JSTracer* trc);

  void ShutdownMarker();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  // Report the sum of SizeOfIncludingThis() for all wrapped JS in the map.
  // Each wrapped JS is only in one map.
  size_t SizeOfWrappedJS(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  Map mTable{XPC_JS_MAP_LENGTH};
};

/*************************/

class Native2WrappedNativeMap {
  using Map = mozilla::HashMap<nsISupports*, XPCWrappedNative*,
                               mozilla::DefaultHasher<nsISupports*>,
                               mozilla::MallocAllocPolicy>;

 public:
  Native2WrappedNativeMap();

  XPCWrappedNative* Find(nsISupports* obj) const {
    MOZ_ASSERT(obj, "bad param");
    Map::Ptr ptr = mMap.lookup(obj);
    return ptr ? ptr->value() : nullptr;
  }

  XPCWrappedNative* Add(XPCWrappedNative* wrapper) {
    MOZ_ASSERT(wrapper, "bad param");
    nsISupports* obj = wrapper->GetIdentityObject();
    Map::AddPtr ptr = mMap.lookupForAdd(obj);
    MOZ_ASSERT(!ptr, "wrapper already in new scope!");
    if (ptr) {
      return ptr->value();
    }
    if (!mMap.add(ptr, obj, wrapper)) {
      return nullptr;
    }
    return wrapper;
  }

  void Clear() { mMap.clear(); }

  uint32_t Count() { return mMap.count(); }

  Map::Iterator Iter() { return mMap.iter(); }
  Map::ModIterator ModIter() { return mMap.modIter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  Map mMap;
};

/*************************/

struct IIDHasher {
  using Key = const nsIID*;
  using Lookup = Key;

  // Note this is returning the hash of the bit pattern of the first part of the
  // nsID, not the hash of the pointer to the nsID.
  static mozilla::HashNumber hash(Lookup lookup) {
    uintptr_t v;
    memcpy(&v, lookup, sizeof(v));
    return mozilla::HashGeneric(v);
  }

  static bool match(Key key, Lookup lookup) { return key->Equals(*lookup); }
};

class IID2NativeInterfaceMap {
  using Map = mozilla::HashMap<const nsIID*, XPCNativeInterface*, IIDHasher,
                               mozilla::MallocAllocPolicy>;

 public:
  IID2NativeInterfaceMap();

  XPCNativeInterface* Find(REFNSIID iid) const {
    Map::Ptr ptr = mMap.lookup(&iid);
    return ptr ? ptr->value() : nullptr;
  }

  bool AddNew(XPCNativeInterface* iface) {
    MOZ_ASSERT(iface, "bad param");
    const nsIID* iid = iface->GetIID();
    return mMap.putNew(iid, iface);
  }

  void Remove(XPCNativeInterface* iface) {
    MOZ_ASSERT(iface, "bad param");
    mMap.remove(iface->GetIID());
  }

  uint32_t Count() { return mMap.count(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  void Trace(JSTracer* trc);

 private:
  Map mMap;
};

/*************************/

class ClassInfo2NativeSetMap {
  using Map = mozilla::HashMap<nsIClassInfo*, RefPtr<XPCNativeSet>,
                               mozilla::DefaultHasher<nsIClassInfo*>,
                               mozilla::MallocAllocPolicy>;

 public:
  ClassInfo2NativeSetMap();

  XPCNativeSet* Find(nsIClassInfo* info) const {
    auto ptr = mMap.lookup(info);
    return ptr ? ptr->value().get() : nullptr;
  }

  XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set) {
    MOZ_ASSERT(info, "bad param");
    auto ptr = mMap.lookupForAdd(info);
    if (ptr) {
      return ptr->value();
    }
    if (!mMap.add(ptr, info, set)) {
      return nullptr;
    }
    return set;
  }

  void Remove(nsIClassInfo* info) {
    MOZ_ASSERT(info, "bad param");
    mMap.remove(info);
  }

  uint32_t Count() { return mMap.count(); }

  // ClassInfo2NativeSetMap holds pointers to *some* XPCNativeSets.
  // So we don't want to count those XPCNativeSets, because they are better
  // counted elsewhere (i.e. in XPCJSContext::mNativeSetMap, which holds
  // pointers to *all* XPCNativeSets).  Hence the "Shallow".
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  Map mMap;
};

/*************************/

class ClassInfo2WrappedNativeProtoMap {
  using Map = mozilla::HashMap<nsIClassInfo*, XPCWrappedNativeProto*,
                               mozilla::DefaultHasher<nsIClassInfo*>,
                               mozilla::MallocAllocPolicy>;

 public:
  ClassInfo2WrappedNativeProtoMap();

  XPCWrappedNativeProto* Find(nsIClassInfo* info) const {
    auto ptr = mMap.lookup(info);
    return ptr ? ptr->value() : nullptr;
  }

  XPCWrappedNativeProto* Add(nsIClassInfo* info, XPCWrappedNativeProto* proto) {
    MOZ_ASSERT(info, "bad param");
    auto ptr = mMap.lookupForAdd(info);
    if (ptr) {
      return ptr->value();
    }
    if (!mMap.add(ptr, info, proto)) {
      return nullptr;
    }
    return proto;
  }

  void Clear() { mMap.clear(); }

  uint32_t Count() { return mMap.count(); }

  Map::Iterator Iter() { return mMap.iter(); }
  Map::ModIterator ModIter() { return mMap.modIter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  Map mMap;
};

/*************************/

struct NativeSetHasher {
  using Key = XPCNativeSet*;
  using Lookup = const XPCNativeSetKey*;

  static mozilla::HashNumber hash(Lookup lookup) { return lookup->Hash(); }
  static bool match(Key key, Lookup lookup);
};

class NativeSetMap {
  using Set = mozilla::HashSet<XPCNativeSet*, NativeSetHasher,
                               mozilla::MallocAllocPolicy>;

 public:
  NativeSetMap();

  XPCNativeSet* Find(const XPCNativeSetKey* key) const {
    auto ptr = mSet.lookup(key);
    return ptr ? *ptr : nullptr;
  }

  XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set) {
    MOZ_ASSERT(key, "bad param");
    MOZ_ASSERT(set, "bad param");
    auto ptr = mSet.lookupForAdd(key);
    if (ptr) {
      return *ptr;
    }
    if (!mSet.add(ptr, set)) {
      return nullptr;
    }
    return set;
  }

  bool AddNew(const XPCNativeSetKey* key, XPCNativeSet* set) {
    XPCNativeSet* set2 = Add(key, set);
    if (!set2) {
      return false;
    }
#ifdef DEBUG
    XPCNativeSetKey key2(set);
    MOZ_ASSERT(key->Hash() == key2.Hash());
    MOZ_ASSERT(set2 == set, "Should not have found an existing entry");
#endif
    return true;
  }

  void Remove(XPCNativeSet* set) {
    MOZ_ASSERT(set, "bad param");

    XPCNativeSetKey key(set);
    mSet.remove(&key);
  }

  uint32_t Count() { return mSet.count(); }

  Set::Iterator Iter() { return mSet.iter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  Set mSet;
};

/***************************************************************************/

class JSObject2JSObjectMap {
  using Map = JS::GCHashMap<JS::Heap<JSObject*>, JS::Heap<JSObject*>,
                            js::StableCellHasher<JS::Heap<JSObject*>>,
                            js::SystemAllocPolicy>;

 public:
  JSObject2JSObjectMap() = default;

  inline JSObject* Find(JSObject* key) {
    MOZ_ASSERT(key, "bad param");
    if (Map::Ptr p = mTable.lookup(key)) {
      return p->value();
    }
    return nullptr;
  }

  /* Note: If the entry already exists, return the old value. */
  inline JSObject* Add(JSContext* cx, JSObject* key, JSObject* value) {
    MOZ_ASSERT(key, "bad param");
    Map::AddPtr p = mTable.lookupForAdd(key);
    if (p) {
      JSObject* oldValue = p->value();
      p->value() = value;
      return oldValue;
    }
    if (!mTable.add(p, key, value)) {
      return nullptr;
    }
    MOZ_ASSERT(xpc::ObjectScope(key)->mWaiverWrapperMap == this);
    return value;
  }

  inline void Remove(JSObject* key) {
    MOZ_ASSERT(key, "bad param");
    mTable.remove(key);
  }

  inline uint32_t Count() { return mTable.count(); }

  void UpdateWeakPointers(JSTracer* trc) { mTable.traceWeak(trc); }

 private:
  Map mTable{XPC_WRAPPER_MAP_LENGTH};
};

#endif /* xpcmaps_h___ */
