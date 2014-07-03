/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private maps (hashtables). */

#ifndef xpcmaps_h___
#define xpcmaps_h___

#include "mozilla/MemoryReporting.h"


// Maps...

// Note that most of the declarations for hash table entries begin with
// a pointer to something or another. This makes them look enough like
// the PLDHashEntryStub struct that the default OPs (PL_DHashGetStubOps())
// just do the right thing for most of our needs.

// no virtuals in the maps - all the common stuff inlined
// templates could be used to good effect here.

/*************************/

class JSObject2WrappedJSMap
{
    typedef js::HashMap<JSObject*, nsXPCWrappedJS*, js::PointerHasher<JSObject*, 3>,
                        js::SystemAllocPolicy> Map;

public:
    static JSObject2WrappedJSMap* newMap(int size) {
        JSObject2WrappedJSMap* map = new JSObject2WrappedJSMap();
        if (map && map->mTable.init(size))
            return map;
        delete map;
        return nullptr;
    }

    inline nsXPCWrappedJS* Find(JSObject* Obj) {
        NS_PRECONDITION(Obj,"bad param");
        Map::Ptr p = mTable.lookup(Obj);
        return p ? p->value() : nullptr;
    }

    inline nsXPCWrappedJS* Add(JSContext* cx, nsXPCWrappedJS* wrapper) {
        NS_PRECONDITION(wrapper,"bad param");
        JSObject* obj = wrapper->GetJSObjectPreserveColor();
        Map::AddPtr p = mTable.lookupForAdd(obj);
        if (p)
            return p->value();
        if (!mTable.add(p, obj, wrapper))
            return nullptr;
        JS_StoreObjectPostBarrierCallback(cx, KeyMarkCallback, obj, this);
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

    void FindDyingJSObjects(nsTArray<nsXPCWrappedJS*>* dying);

    void ShutdownMarker();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
        size_t n = mallocSizeOf(this);
        n += mTable.sizeOfExcludingThis(mallocSizeOf);
        return n;
    }

private:
    JSObject2WrappedJSMap() {}

    /*
     * This function is called during minor GCs for each key in the HashMap that
     * has been moved.
     */
    static void KeyMarkCallback(JSTracer *trc, JSObject *key, void *data) {
        JSObject2WrappedJSMap* self = static_cast<JSObject2WrappedJSMap*>(data);
        JSObject *prior = key;
        JS_CallObjectTracer(trc, &key, "XPCJSRuntime::mWrappedJSMap key");
        self->mTable.rekeyIfMoved(prior, key);
    }

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

    static Native2WrappedNativeMap* newMap(int size);

    inline XPCWrappedNative* Find(nsISupports* Obj)
    {
        NS_PRECONDITION(Obj,"bad param");
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, Obj, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline XPCWrappedNative* Add(XPCWrappedNative* wrapper)
    {
        NS_PRECONDITION(wrapper,"bad param");
        nsISupports* obj = wrapper->GetIdentityObject();
        MOZ_ASSERT(!Find(obj), "wrapper already in new scope!");
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, obj, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, wrapper->GetIdentityObject(), PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    ~Native2WrappedNativeMap();
private:
    Native2WrappedNativeMap();    // no implementation
    Native2WrappedNativeMap(int size);

    static size_t SizeOfEntryExcludingThis(PLDHashEntryHdr *hdr, mozilla::MallocSizeOf mallocSizeOf, void *);

private:
    PLDHashTable *mTable;
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

    static IID2WrappedJSClassMap* newMap(int size);

    inline nsXPCWrappedJSClass* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, &iid, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline nsXPCWrappedJSClass* Add(nsXPCWrappedJSClass* clazz)
    {
        NS_PRECONDITION(clazz,"bad param");
        const nsIID* iid = &clazz->GetIID();
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, iid, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, &clazz->GetIID(), PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    ~IID2WrappedJSClassMap();
private:
    IID2WrappedJSClassMap();    // no implementation
    IID2WrappedJSClassMap(int size);
private:
    PLDHashTable *mTable;
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

    static IID2NativeInterfaceMap* newMap(int size);

    inline XPCNativeInterface* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, &iid, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline XPCNativeInterface* Add(XPCNativeInterface* iface)
    {
        NS_PRECONDITION(iface,"bad param");
        const nsIID* iid = iface->GetIID();
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, iid, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, iface->GetIID(), PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    ~IID2NativeInterfaceMap();
private:
    IID2NativeInterfaceMap();    // no implementation
    IID2NativeInterfaceMap(int size);

    static size_t SizeOfEntryExcludingThis(PLDHashEntryHdr *hdr, mozilla::MallocSizeOf mallocSizeOf, void *);

private:
    PLDHashTable *mTable;
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

    static ClassInfo2NativeSetMap* newMap(int size);

    inline XPCNativeSet* Find(nsIClassInfo* info)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, info, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline XPCNativeSet* Add(nsIClassInfo* info, XPCNativeSet* set)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, info, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, info, PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    // ClassInfo2NativeSetMap holds pointers to *some* XPCNativeSets.
    // So we don't want to count those XPCNativeSets, because they are better
    // counted elsewhere (i.e. in XPCJSRuntime::mNativeSetMap, which holds
    // pointers to *all* XPCNativeSets).  Hence the "Shallow".
    size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    ~ClassInfo2NativeSetMap();
private:
    ClassInfo2NativeSetMap();    // no implementation
    ClassInfo2NativeSetMap(int size);
private:
    PLDHashTable *mTable;
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

    static ClassInfo2WrappedNativeProtoMap* newMap(int size);

    inline XPCWrappedNativeProto* Find(nsIClassInfo* info)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, info, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline XPCWrappedNativeProto* Add(nsIClassInfo* info, XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(info,"bad param");
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, info, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, info, PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    ~ClassInfo2WrappedNativeProtoMap();
private:
    ClassInfo2WrappedNativeProtoMap();    // no implementation
    ClassInfo2WrappedNativeProtoMap(int size);

    static size_t SizeOfEntryExcludingThis(PLDHashEntryHdr *hdr, mozilla::MallocSizeOf mallocSizeOf, void *);

private:
    PLDHashTable *mTable;
};

/*************************/

class NativeSetMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        XPCNativeSet* key_value;

        static bool
        Match(PLDHashTable *table,
              const PLDHashEntryHdr *entry,
              const void *key);

        static const struct PLDHashTableOps sOps;
    };

    static NativeSetMap* newMap(int size);

    inline XPCNativeSet* Find(XPCNativeSetKey* key)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, key, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->key_value;
    }

    inline XPCNativeSet* Add(const XPCNativeSetKey* key, XPCNativeSet* set)
    {
        NS_PRECONDITION(key,"bad param");
        NS_PRECONDITION(set,"bad param");
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, key, PL_DHASH_ADD);
        if (!entry)
            return nullptr;
        if (entry->key_value)
            return entry->key_value;
        entry->key_value = set;
        return set;
    }

    inline XPCNativeSet* Add(XPCNativeSet* set)
    {
        XPCNativeSetKey key(set, nullptr, 0);
        return Add(&key, set);
    }

    inline void Remove(XPCNativeSet* set)
    {
        NS_PRECONDITION(set,"bad param");

        XPCNativeSetKey key(set, nullptr, 0);
        PL_DHashTableOperate(mTable, &key, PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    ~NativeSetMap();
private:
    NativeSetMap();    // no implementation
    NativeSetMap(int size);

    static size_t SizeOfEntryExcludingThis(PLDHashEntryHdr *hdr, mozilla::MallocSizeOf mallocSizeOf, void *);

private:
    PLDHashTable *mTable;
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
        Match(PLDHashTable *table,
              const PLDHashEntryHdr *entry,
              const void *key);

        static void
        Clear(PLDHashTable *table, PLDHashEntryHdr *entry);

        static const struct PLDHashTableOps sOps;
    };

    static IID2ThisTranslatorMap* newMap(int size);

    inline nsIXPCFunctionThisTranslator* Find(REFNSIID iid)
    {
        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, &iid, PL_DHASH_LOOKUP);
        if (PL_DHASH_ENTRY_IS_FREE(entry))
            return nullptr;
        return entry->value;
    }

    inline nsIXPCFunctionThisTranslator* Add(REFNSIID iid,
                                             nsIXPCFunctionThisTranslator* obj)
    {

        Entry* entry = (Entry*)
            PL_DHashTableOperate(mTable, &iid, PL_DHASH_ADD);
        if (!entry)
            return nullptr;
        entry->value = obj;
        entry->key = iid;
        return obj;
    }

    inline void Remove(REFNSIID iid)
    {
        PL_DHashTableOperate(mTable, &iid, PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    ~IID2ThisTranslatorMap();
private:
    IID2ThisTranslatorMap();    // no implementation
    IID2ThisTranslatorMap(int size);
private:
    PLDHashTable *mTable;
};

/***************************************************************************/

class XPCNativeScriptableSharedMap
{
public:
    struct Entry : public PLDHashEntryHdr
    {
        XPCNativeScriptableShared* key;

        static PLDHashNumber
        Hash(PLDHashTable *table, const void *key);

        static bool
        Match(PLDHashTable *table,
              const PLDHashEntryHdr *entry,
              const void *key);

        static const struct PLDHashTableOps sOps;
    };

    static XPCNativeScriptableSharedMap* newMap(int size);

    bool GetNewOrUsed(uint32_t flags, char* name, uint32_t interfacesBitmap,
                      XPCNativeScriptableInfo* si);

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    ~XPCNativeScriptableSharedMap();
private:
    XPCNativeScriptableSharedMap();    // no implementation
    XPCNativeScriptableSharedMap(int size);
private:
    PLDHashTable *mTable;
};

/***************************************************************************/

class XPCWrappedNativeProtoMap
{
public:
    static XPCWrappedNativeProtoMap* newMap(int size);

    inline XPCWrappedNativeProto* Add(XPCWrappedNativeProto* proto)
    {
        NS_PRECONDITION(proto,"bad param");
        PLDHashEntryStub* entry = (PLDHashEntryStub*)
            PL_DHashTableOperate(mTable, proto, PL_DHASH_ADD);
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
        PL_DHashTableOperate(mTable, proto, PL_DHASH_REMOVE);
    }

    inline uint32_t Count() {return mTable->entryCount;}
    inline uint32_t Enumerate(PLDHashEnumerator f, void *arg)
        {return PL_DHashTableEnumerate(mTable, f, arg);}

    ~XPCWrappedNativeProtoMap();
private:
    XPCWrappedNativeProtoMap();    // no implementation
    XPCWrappedNativeProtoMap(int size);
private:
    PLDHashTable *mTable;
};

/***************************************************************************/

class JSObject2JSObjectMap
{
    typedef js::HashMap<JSObject *, JS::Heap<JSObject *>, js::PointerHasher<JSObject *, 3>,
                        js::SystemAllocPolicy> Map;

public:
    static JSObject2JSObjectMap* newMap(int size) {
        JSObject2JSObjectMap* map = new JSObject2JSObjectMap();
        if (map && map->mTable.init(size))
            return map;
        delete map;
        return nullptr;
    }

    inline JSObject* Find(JSObject* key) {
        NS_PRECONDITION(key, "bad param");
        if (Map::Ptr p = mTable.lookup(key))
            return p->value();
        return nullptr;
    }

    /* Note: If the entry already exists, return the old value. */
    inline JSObject* Add(JSContext *cx, JSObject *key, JSObject *value) {
        NS_PRECONDITION(key,"bad param");
        Map::AddPtr p = mTable.lookupForAdd(key);
        if (p)
            return p->value();
        if (!mTable.add(p, key, value))
            return nullptr;
        MOZ_ASSERT(xpc::CompartmentPrivate::Get(key)->scope->mWaiverWrapperMap == this);
        JS_StoreObjectPostBarrierCallback(cx, KeyMarkCallback, key, this);
        return value;
    }

    inline void Remove(JSObject* key) {
        NS_PRECONDITION(key,"bad param");
        mTable.remove(key);
    }

    inline uint32_t Count() { return mTable.count(); }

    void Sweep() {
        for (Map::Enum e(mTable); !e.empty(); e.popFront()) {
            JSObject *updated = e.front().key();
            if (JS_IsAboutToBeFinalizedUnbarriered(&updated) || JS_IsAboutToBeFinalized(&e.front().value()))
                e.removeFront();
            else if (updated != e.front().key())
                e.rekeyFront(updated);
        }
    }

    void Reparent(JSContext *aCx, JSObject *aNewInnerArg) {
        JS::RootedObject aNewInner(aCx, aNewInnerArg);
        for (Map::Enum e(mTable); !e.empty(); e.popFront()) {
            /*
             * We reparent wrappers that have as their parent an inner window
             * whose outer has the new inner window as its current inner.
             */
            JS::RootedObject wrapper(aCx, e.front().value());
            JS::RootedObject parent(aCx, JS_GetParent(wrapper));
            JS::RootedObject outer(aCx, JS_ObjectToOuterObject(aCx, parent));
            if (outer) {
                JSObject *inner = JS_ObjectToInnerObject(aCx, outer);
                if (inner == aNewInner && inner != parent)
                    JS_SetParent(aCx, wrapper, aNewInner);
            } else {
                JS_ClearPendingException(aCx);
            }
        }
    }

private:
    JSObject2JSObjectMap() {}

    /*
     * This function is called during minor GCs for each key in the HashMap that
     * has been moved.
     */
    static void KeyMarkCallback(JSTracer *trc, JSObject *key, void *data) {
        /*
         * To stop the barriers on the values of mTable firing while we are
         * marking the store buffer, we cast the table to one that is
         * binary-equivatlent but without the barriers, and update that.
         */
        typedef js::HashMap<JSObject *, JSObject *, js::PointerHasher<JSObject *, 3>,
                            js::SystemAllocPolicy> UnbarrieredMap;
        JSObject2JSObjectMap *self = static_cast<JSObject2JSObjectMap *>(data);
        UnbarrieredMap &table = reinterpret_cast<UnbarrieredMap &>(self->mTable);

        JSObject *prior = key;
        JS_CallObjectTracer(trc, &key, "XPCWrappedNativeScope::mWaiverWrapperMap key");
        table.rekeyIfMoved(prior, key);
    }

    Map mTable;
};

#endif /* xpcmaps_h___ */
