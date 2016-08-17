/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
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

class JSObject2WrappedJSMap
{
    using Map = js::HashMap<JS::Heap<JSObject*>,
                            nsXPCWrappedJS*,
                            js::MovableCellHasher<JS::Heap<JSObject*>>,
                            js::SystemAllocPolicy>;

public:
    static JSObject2WrappedJSMap* newMap(int length) {
        JSObject2WrappedJSMap* map = new JSObject2WrappedJSMap();
        if (!map->mTable.init(length)) {
            // This is a decent estimate of the size of the hash table's
            // entry storage. The |2| is because on average the capacity is
            // twice the requested length.
            NS_ABORT_OOM(length * 2 * sizeof(Map::Entry));
        }
        return map;
    }

    inline nsXPCWrappedJS* Find(JSObject* Obj) {
        NS_PRECONDITION(Obj,"bad param");
        Map::Ptr p = mTable.lookup(Obj);
        return p ? p->value() : nullptr;
    }

#ifdef DEBUG
    inline bool HasWrapper(nsXPCWrappedJS* wrapper) {
        for (auto r = mTable.all(); !r.empty(); r.popFront()) {
            if (r.front().value() == wrapper)
                return true;
        }
        return false;
    }
#endif

    inline nsXPCWrappedJS* Add(JSContext* cx, nsXPCWrappedJS* wrapper) {
        NS_PRECONDITION(wrapper,"bad param");
        JSObject* obj = wrapper->GetJSObjectPreserveColor();
        Map::AddPtr p = mTable.lookupForAdd(obj);
        if (p)
            return p->value();
        if (!mTable.add(p, obj, wrapper))
            return nullptr;
        return wrapper;
    }

    inline void Remove(nsXPCWrappedJS* wrapper) {
        NS_PRECONDITION(wrapper,"bad param");
        mTable.remove(wrapper->GetJSObjectPreserveColor());
    }

    inline uint32_t Count() {return mTable.count();}

    inline void Dump(int16_t depth) {
        for (Map::Range r = mTable.all(); !r.empty(); r.popFront())
            r.front().value()->DebugDump(depth);
    }

    void UpdateWeakPointersAfterGC(XPCJSRuntime* runtime);

    void ShutdownMarker();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    // Report the sum of SizeOfIncludingThis() for all wrapped JS in the map.
    // Each wrapped JS is only in one map.
    size_t SizeOfWrappedJS(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    JSObject2WrappedJSMap() {}

    Map mTable;
};

/*************************/

class Native2WrappedNativeMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        nsISupports*      key;
        XPCWrappedNative* value;
    };

    static Native2WrappedNativeMap* newMap(int length);

    inline XPCWrappedNative* Find(nsISupports* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        auto entry = static_cast<Entry*>(mTable.Search(Obj));
        return entry ? entry->value : nullptr;
    }

    inline XPCWrappedNative* Add(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        nsISupports* obj = wrapper->GetIdentityObject();
        MOZ_ASSERT(!Find(obj), "wrapper already in new scope!");
        auto entry = static_cast<Entry*>(mTable.Add(obj, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return entry->value;
        entry->key = obj;
        entry->value = wrapper;
        return wrapper;
    }

    inline void Remove(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
#ifdef DEBUG
        XPCWrappedNative* wrapperInMap = Find(wrapper->GetIdentityObject());
        MOZ_ASSERT(!wrapperInMap || wrapperInMap == wrapper,
                   "About to remove a different wrapper with the same "
                   "nsISupports identity! This will most likely cause serious "
                   "problems!");
#endif
        mTable.Remove(wrapper->GetIdentityObject());
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    Native2WrappedNativeMap();    // no implementation
    explicit Native2WrappedNativeMap(int size);

private:
    PLDHashTable mTable;
};

/*************************/

class IID2WrappedJSClassMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        const nsIID*         key;
        nsXPCWrappedJSClass* value;

        static const struct PLDHashTableOps sOps;
    };

    static IID2WrappedJSClassMap* newMap(int length);

    inline nsXPCWrappedJSClass* Find(REFNSIID iid)
    {
        auto entry = static_cast<Entry*>(mTable.Search(&iid));
        return entry ? entry->value : nullptr;
    }

    inline nsXPCWrappedJSClass* Add(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        const nsIID* iid = &clazz->GetIID();
        auto entry = static_cast<Entry*>(mTable.Add(iid, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return entry->value;
        entry->key = iid;
        entry->value = clazz;
        return clazz;
    }

    inline void Remove(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        mTable.Remove(&clazz->GetIID());
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

#ifdef DEBUG
    PLDHashTable::Iterator Iter() { return mTable.Iter(); }
#endif

private:
    IID2WrappedJSClassMap();    // no implementation
    explicit IID2WrappedJSClassMap(int size);
private:
    PLDHashTable mTable;
};

/*************************/

class IID2NativeInterfaceMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        const nsIID*        key;
        XPCNativeInterface* value;

        static const struct PLDHashTableOps sOps;
    };

    static IID2NativeInterfaceMap* newMap(int length);

    inline XPCNativeInterface* Find(REFNSIID iid)
    {
        auto entry = static_cast<Entry*>(mTable.Search(&iid));
        return entry ? entry->value : nullptr;
    }

    inline XPCNativeInterface* Add(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        const nsIID* iid = iface->GetIID();
        auto entry = static_cast<Entry*>(mTable.Add(iid, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return entry->value;
        entry->key = iid;
        entry->value = iface;
        return iface;
    }

    inline void Remove(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        mTable.Remove(iface->GetIID());
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    IID2NativeInterfaceMap();    // no implementation
    explicit IID2NativeInterfaceMap(int size);

private:
    PLDHashTable mTable;
};

/*************************/

class ClassInfo2NativeSetMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        nsIClassInfo* key;
        XPCNativeSet* value;
    };

    static ClassInfo2NativeSetMap* newMap(int length);

    inline XPCNativeSet* Find(nsIClassInfo* info)
    {
        auto entry = static_cast<Entry*>(mTable.Search(info));
        return entry ? entry->value : nullptr;
    }

    inline XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set)
    {
        NS_PRECONDITION(info,"bad param");
        auto entry = static_cast<Entry*>(mTable.Add(info, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return entry->value;
        entry->key = info;
        entry->value = set;
        return set;
    }

    inline void Remove(nsIClassInfo* info)
    {
        NS_PRECONDITION(info,"bad param");
        mTable.Remove(info);
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

    // ClassInfo2NativeSetMap holds pointers to *some* XPCNativeSets.
    // So we don't want to count those XPCNativeSets, because they are better
    // counted elsewhere (i.e. in XPCJSRuntime::mNativeSetMap, which holds
    // pointers to *all* XPCNativeSets).  Hence the "Shallow".
    size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

private:
    ClassInfo2NativeSetMap();    // no implementation
    explicit ClassInfo2NativeSetMap(int size);
private:
    PLDHashTable mTable;
};

/*************************/

class ClassInfo2WrappedNativeProtoMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        nsIClassInfo*          key;
        XPCWrappedNativeProto* value;
    };

    static ClassInfo2WrappedNativeProtoMap* newMap(int length);

    inline XPCWrappedNativeProto* Find(nsIClassInfo* info)
    {
        auto entry = static_cast<Entry*>(mTable.Search(info));
        return entry ? entry->value : nullptr;
    }

    inline XPCWrappedNativeProto* Add(nsIClassInfo* info, XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(info,"bad param");
        auto entry = static_cast<Entry*>(mTable.Add(info, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return entry->value;
        entry->key = info;
        entry->value = proto;
        return proto;
    }

    inline void Remove(nsIClassInfo* info)
    {
        NS_PRECONDITION(info,"bad param");
        mTable.Remove(info);
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    ClassInfo2WrappedNativeProtoMap();    // no implementation
    explicit ClassInfo2WrappedNativeProtoMap(int size);

private:
    PLDHashTable mTable;
};

/*************************/

class NativeSetMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        XPCNativeSet* key_value;

        static bool
        Match(const PLDHashEntryHdr* entry, const void* key);

        static const struct PLDHashTableOps sOps;
    };

    static NativeSetMap* newMap(int length);

    inline XPCNativeSet* Find(XPCNativeSetKey* key)
    {
        auto entry = static_cast<Entry*>(mTable.Search(key));
        return entry ? entry->key_value : nullptr;
    }

    inline XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set)
    {
        MOZ_ASSERT(key, "bad param");
        MOZ_ASSERT(set, "bad param");
        auto entry = static_cast<Entry*>(mTable.Add(key, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key_value)
            return entry->key_value;
        entry->key_value = set;
        return set;
    }

    inline void Remove(XPCNativeSet* set)
    {
        MOZ_ASSERT(set, "bad param");

        XPCNativeSetKey key(set, nullptr, 0);
        mTable.Remove(&key);
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    NativeSetMap();    // no implementation
    explicit NativeSetMap(int size);

private:
    PLDHashTable mTable;
};

/***************************************************************************/

class IID2ThisTranslatorMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        nsIID                                  key;
        nsCOMPtr<nsIXPCFunctionThisTranslator> value;

        static bool
        Match(const PLDHashEntryHdr* entry, const void* key);

        static void
        Clear(PLDHashTable* table, PLDHashEntryHdr* entry);

        static const struct PLDHashTableOps sOps;
    };

    static IID2ThisTranslatorMap* newMap(int length);

    inline nsIXPCFunctionThisTranslator* Find(REFNSIID iid)
    {
        auto entry = static_cast<Entry*>(mTable.Search(&iid));
        if (!entry) {
            return nullptr;
        }
        return entry->value;
    }

    inline nsIXPCFunctionThisTranslator* Add(REFNSIID iid,
                                             nsIXPCFunctionThisTranslator* obj)
    {
        auto entry = static_cast<Entry*>(mTable.Add(&iid, mozilla::fallible));
        if (!entry)
            return nullptr;
        entry->value = obj;
        entry->key = iid;
        return obj;
    }

    inline void Remove(REFNSIID iid)
    {
        mTable.Remove(&iid);
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

private:
    IID2ThisTranslatorMap();    // no implementation
    explicit IID2ThisTranslatorMap(int size);
private:
    PLDHashTable mTable;
};

/***************************************************************************/

class XPCNativeScriptableSharedMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        // This is a weak reference that will be cleared
        // in the XPCNativeScriptableShared destructor.
        XPCNativeScriptableShared* key;

        static PLDHashNumber
        Hash(const void* key);

        static bool
        Match(const PLDHashEntryHdr* entry, const void* key);

        static const struct PLDHashTableOps sOps;
    };

    static XPCNativeScriptableSharedMap* newMap(int length);

    bool GetNewOrUsed(uint32_t flags, char* name, XPCNativeScriptableInfo* si);

    inline uint32_t Count() { return mTable.EntryCount(); }

    void Remove(XPCNativeScriptableShared* key) { mTable.Remove(key); }

private:
    XPCNativeScriptableSharedMap();    // no implementation
    explicit XPCNativeScriptableSharedMap(int size);
private:
    PLDHashTable mTable;
};

/***************************************************************************/

class XPCWrappedNativeProtoMap
{
public:
    typedef PLDHashEntryStub Entry;

    static XPCWrappedNativeProtoMap* newMap(int length);

    inline XPCWrappedNativeProto* Add(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        auto entry = static_cast<PLDHashEntryStub*>
                                (mTable.Add(proto, mozilla::fallible));
        if (!entry)
            return nullptr;
        if (entry->key)
            return (XPCWrappedNativeProto*) entry->key;
        entry->key = proto;
        return proto;
    }

    inline void Remove(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        mTable.Remove(proto);
    }

    inline uint32_t Count() { return mTable.EntryCount(); }

    PLDHashTable::Iterator Iter() { return mTable.Iter(); }

private:
    XPCWrappedNativeProtoMap();    // no implementation
    explicit XPCWrappedNativeProtoMap(int size);
private:
    PLDHashTable mTable;
};

/***************************************************************************/

class JSObject2JSObjectMap
{
    using Map = JS::GCHashMap<JS::Heap<JSObject*>,
                              JS::Heap<JSObject*>,
                              js::MovableCellHasher<JS::Heap<JSObject*>>,
                              js::SystemAllocPolicy>;

public:
    static JSObject2JSObjectMap* newMap(int length) {
        JSObject2JSObjectMap* map = new JSObject2JSObjectMap();
        if (!map->mTable.init(length)) {
            // This is a decent estimate of the size of the hash table's
            // entry storage. The |2| is because on average the capacity is
            // twice the requested length.
            NS_ABORT_OOM(length * 2 * sizeof(Map::Entry));
        }
        return map;
    }

    inline JSObject* Find(JSObject* key) {
        NS_PRECONDITION(key, "bad param");
        if (Map::Ptr p = mTable.lookup(key))
            return p->value();
        return nullptr;
    }

    /* Note: If the entry already exists, return the old value. */
    inline JSObject* Add(JSContext* cx, JSObject* key, JSObject* value) {
        NS_PRECONDITION(key,"bad param");
        Map::AddPtr p = mTable.lookupForAdd(key);
        if (p)
            return p->value();
        if (!mTable.add(p, key, value))
            return nullptr;
        MOZ_ASSERT(xpc::CompartmentPrivate::Get(key)->scope->mWaiverWrapperMap == this);
        return value;
    }

    inline void Remove(JSObject* key) {
        NS_PRECONDITION(key,"bad param");
        mTable.remove(key);
    }

    inline uint32_t Count() { return mTable.count(); }

    void Sweep() {
        mTable.sweep();
    }

private:
    JSObject2JSObjectMap() {}

    Map mTable;
};

#endif /* xpcmaps_h___ */
