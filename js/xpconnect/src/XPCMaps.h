/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private maps (hashtables). */

#ifndef xpcmaps_h___
#define xpcmaps_h___

#include "mozilla/MemoryReporting.h"

#include "js/GCHashTable.h"

// Maps...

// Note that most of the declarations for hash table entries begin with
// a pointer to something or another. This makes them look enough like
// the PLDHashEntryStub struct that the default ops (PLDHashTable::StubOps())
// just do the right thing for most of our needs.

// no virtuals in the maps - all the common stuff inlined
// templates could be used to good effect here.

/*************************/

class JSObject2WrappedJSMap {
  using Map = js::HashMap<JS::Heap<JSObject*>, nsXPCWrappedJS*,
                          js::MovableCellHasher<JS::Heap<JSObject*>>,
                          InfallibleAllocPolicy>;

 public:
  static JSObject2WrappedJSMap* newMap(int length) {
    return new JSObject2WrappedJSMap(length);
  }

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

  void UpdateWeakPointersAfterGC();

  void ShutdownMarker();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

  // Report the sum of SizeOfIncludingThis() for all wrapped JS in the map.
  // Each wrapped JS is only in one map.
  size_t SizeOfWrappedJS(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  explicit JSObject2WrappedJSMap(size_t length) : mTable(length) {}

  Map mTable;
};

/*************************/

class Native2WrappedNativeMap {
 public:
  struct Entry : public PLDHashEntryHdr {
    nsISupports* key;
    XPCWrappedNative* value;
  };

  static Native2WrappedNativeMap* newMap(int length);

  inline XPCWrappedNative* Find(nsISupports* Obj) const {
    MOZ_ASSERT(Obj, "bad param");
    auto entry = static_cast<Entry*>(mTable.Search(Obj));
    return entry ? entry->value : nullptr;
  }

  inline XPCWrappedNative* Add(XPCWrappedNative* wrapper) {
    MOZ_ASSERT(wrapper, "bad param");
    nsISupports* obj = wrapper->GetIdentityObject();
    MOZ_ASSERT(!Find(obj), "wrapper already in new scope!");
    auto entry = static_cast<Entry*>(mTable.Add(obj, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key) {
      return entry->value;
    }
    entry->key = obj;
    entry->value = wrapper;
    return wrapper;
  }

  inline void Remove(XPCWrappedNative* wrapper) {
    MOZ_ASSERT(wrapper, "bad param");
#ifdef DEBUG
    XPCWrappedNative* wrapperInMap = Find(wrapper->GetIdentityObject());
    MOZ_ASSERT(!wrapperInMap || wrapperInMap == wrapper,
               "About to remove a different wrapper with the same "
               "nsISupports identity! This will most likely cause serious "
               "problems!");
#endif
    mTable.Remove(wrapper->GetIdentityObject());
  }

  inline void Clear() { mTable.Clear(); }

  inline uint32_t Count() { return mTable.EntryCount(); }

  PLDHashTable::Iterator Iter() { return mTable.Iter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  Native2WrappedNativeMap();  // no implementation
  explicit Native2WrappedNativeMap(int size);

 private:
  PLDHashTable mTable;
};

/*************************/

class IID2NativeInterfaceMap {
 public:
  struct Entry : public PLDHashEntryHdr {
    const nsIID* key;
    XPCNativeInterface* value;

    static const struct PLDHashTableOps sOps;
  };

  static IID2NativeInterfaceMap* newMap(int length);

  inline XPCNativeInterface* Find(REFNSIID iid) const {
    auto entry = static_cast<Entry*>(mTable.Search(&iid));
    return entry ? entry->value : nullptr;
  }

  inline XPCNativeInterface* Add(XPCNativeInterface* iface) {
    MOZ_ASSERT(iface, "bad param");
    const nsIID* iid = iface->GetIID();
    auto entry = static_cast<Entry*>(mTable.Add(iid, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key) {
      return entry->value;
    }
    entry->key = iid;
    entry->value = iface;
    return iface;
  }

  inline void Remove(XPCNativeInterface* iface) {
    MOZ_ASSERT(iface, "bad param");
    mTable.Remove(iface->GetIID());
  }

  inline uint32_t Count() { return mTable.EntryCount(); }

  PLDHashTable::Iterator Iter() { return mTable.Iter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  IID2NativeInterfaceMap();  // no implementation
  explicit IID2NativeInterfaceMap(int size);

 private:
  PLDHashTable mTable;
};

/*************************/

class ClassInfo2NativeSetMap {
 public:
  struct Entry : public PLDHashEntryHdr {
    nsIClassInfo* key;
    XPCNativeSet* value;  // strong reference
    static const PLDHashTableOps sOps;

   private:
    static bool Match(const PLDHashEntryHdr* aEntry, const void* aKey);
    static void Clear(PLDHashTable* aTable, PLDHashEntryHdr* aEntry);
  };

  static ClassInfo2NativeSetMap* newMap(int length);

  inline XPCNativeSet* Find(nsIClassInfo* info) const {
    auto entry = static_cast<Entry*>(mTable.Search(info));
    return entry ? entry->value : nullptr;
  }

  inline XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set) {
    MOZ_ASSERT(info, "bad param");
    auto entry = static_cast<Entry*>(mTable.Add(info, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key) {
      return entry->value;
    }
    entry->key = info;
    NS_ADDREF(entry->value = set);
    return set;
  }

  inline void Remove(nsIClassInfo* info) {
    MOZ_ASSERT(info, "bad param");
    mTable.Remove(info);
  }

  inline uint32_t Count() { return mTable.EntryCount(); }

  // ClassInfo2NativeSetMap holds pointers to *some* XPCNativeSets.
  // So we don't want to count those XPCNativeSets, because they are better
  // counted elsewhere (i.e. in XPCJSContext::mNativeSetMap, which holds
  // pointers to *all* XPCNativeSets).  Hence the "Shallow".
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  ClassInfo2NativeSetMap();  // no implementation
  explicit ClassInfo2NativeSetMap(int size);

 private:
  PLDHashTable mTable;
};

/*************************/

class ClassInfo2WrappedNativeProtoMap {
 public:
  struct Entry : public PLDHashEntryHdr {
    nsIClassInfo* key;
    XPCWrappedNativeProto* value;
  };

  static ClassInfo2WrappedNativeProtoMap* newMap(int length);

  inline XPCWrappedNativeProto* Find(nsIClassInfo* info) const {
    auto entry = static_cast<Entry*>(mTable.Search(info));
    return entry ? entry->value : nullptr;
  }

  inline XPCWrappedNativeProto* Add(nsIClassInfo* info,
                                    XPCWrappedNativeProto* proto) {
    MOZ_ASSERT(info, "bad param");
    auto entry = static_cast<Entry*>(mTable.Add(info, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key) {
      return entry->value;
    }
    entry->key = info;
    entry->value = proto;
    return proto;
  }

  inline void Remove(nsIClassInfo* info) {
    MOZ_ASSERT(info, "bad param");
    mTable.Remove(info);
  }

  inline void Clear() { mTable.Clear(); }

  inline uint32_t Count() { return mTable.EntryCount(); }

  PLDHashTable::Iterator Iter() { return mTable.Iter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  ClassInfo2WrappedNativeProtoMap();  // no implementation
  explicit ClassInfo2WrappedNativeProtoMap(int size);

 private:
  PLDHashTable mTable;
};

/*************************/

class NativeSetMap {
 public:
  struct Entry : public PLDHashEntryHdr {
    XPCNativeSet* key_value;

    static bool Match(const PLDHashEntryHdr* entry, const void* key);

    static const struct PLDHashTableOps sOps;
  };

  static NativeSetMap* newMap(int length);

  inline XPCNativeSet* Find(XPCNativeSetKey* key) const {
    auto entry = static_cast<Entry*>(mTable.Search(key));
    return entry ? entry->key_value : nullptr;
  }

  inline XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set) {
    MOZ_ASSERT(key, "bad param");
    MOZ_ASSERT(set, "bad param");
    auto entry = static_cast<Entry*>(mTable.Add(key, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key_value) {
      return entry->key_value;
    }
    entry->key_value = set;
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

  inline void Remove(XPCNativeSet* set) {
    MOZ_ASSERT(set, "bad param");

    XPCNativeSetKey key(set);
    mTable.Remove(&key);
  }

  inline uint32_t Count() { return mTable.EntryCount(); }

  PLDHashTable::Iterator Iter() { return mTable.Iter(); }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

 private:
  NativeSetMap();  // no implementation
  explicit NativeSetMap(int size);

 private:
  PLDHashTable mTable;
};

/***************************************************************************/

class XPCWrappedNativeProtoMap {
 public:
  typedef PLDHashEntryStub Entry;

  static XPCWrappedNativeProtoMap* newMap(int length);

  inline XPCWrappedNativeProto* Add(XPCWrappedNativeProto* proto) {
    MOZ_ASSERT(proto, "bad param");
    auto entry =
        static_cast<PLDHashEntryStub*>(mTable.Add(proto, mozilla::fallible));
    if (!entry) {
      return nullptr;
    }
    if (entry->key) {
      return (XPCWrappedNativeProto*)entry->key;
    }
    entry->key = proto;
    return proto;
  }

  inline void Remove(XPCWrappedNativeProto* proto) {
    MOZ_ASSERT(proto, "bad param");
    mTable.Remove(proto);
  }

  inline uint32_t Count() { return mTable.EntryCount(); }

  PLDHashTable::Iterator Iter() { return mTable.Iter(); }

 private:
  XPCWrappedNativeProtoMap();  // no implementation
  explicit XPCWrappedNativeProtoMap(int size);

 private:
  PLDHashTable mTable;
};

/***************************************************************************/

class JSObject2JSObjectMap {
  using Map = JS::GCHashMap<JS::Heap<JSObject*>, JS::Heap<JSObject*>,
                            js::MovableCellHasher<JS::Heap<JSObject*>>,
                            js::SystemAllocPolicy>;

 public:
  static JSObject2JSObjectMap* newMap(int length) {
    return new JSObject2JSObjectMap(length);
  }

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

  void Sweep() { mTable.sweep(); }

 private:
  explicit JSObject2JSObjectMap(size_t length) : mTable(length) {}

  Map mTable;
};

#endif /* xpcmaps_h___ */
