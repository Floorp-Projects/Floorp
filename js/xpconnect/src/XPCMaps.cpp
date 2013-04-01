/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private maps (hashtables). */

#include "xpcprivate.h"

#include "js/HashTable.h"

/***************************************************************************/
// static shared...

// Note this is returning the bit pattern of the first part of the nsID, not
// the pointer to the nsID.

static JSDHashNumber
HashIIDPtrKey(JSDHashTable *table, const void *key)
{
    return *((js::HashNumber*)key);
}

static JSBool
MatchIIDPtrKey(JSDHashTable *table,
               const JSDHashEntryHdr *entry,
               const void *key)
{
    return ((const nsID*)key)->
                Equals(*((const nsID*)((JSDHashEntryStub*)entry)->key));
}

static JSDHashNumber
HashNativeKey(JSDHashTable *table, const void *key)
{
    XPCNativeSetKey* Key = (XPCNativeSetKey*) key;

    JSDHashNumber h = 0;

    XPCNativeSet*       Set;
    XPCNativeInterface* Addition;
    uint16_t            Position;

    if (Key->IsAKey()) {
        Set      = Key->GetBaseSet();
        Addition = Key->GetAddition();
        Position = Key->GetPosition();
    } else {
        Set      = (XPCNativeSet*) Key;
        Addition = nullptr;
        Position = 0;
    }

    if (!Set) {
        NS_ASSERTION(Addition, "bad key");
        // This would be an XOR like below.
        // But "0 ^ x == x". So it does not matter.
        h = (js::HashNumber) NS_PTR_TO_INT32(Addition) >> 2;
    } else {
        XPCNativeInterface** Current = Set->GetInterfaceArray();
        uint16_t count = Set->GetInterfaceCount();
        if (Addition) {
            count++;
            for (uint16_t i = 0; i < count; i++) {
                if (i == Position)
                    h ^= (js::HashNumber) NS_PTR_TO_INT32(Addition) >> 2;
                else
                    h ^= (js::HashNumber) NS_PTR_TO_INT32(*(Current++)) >> 2;
            }
        } else {
            for (uint16_t i = 0; i < count; i++)
                h ^= (js::HashNumber) NS_PTR_TO_INT32(*(Current++)) >> 2;
        }
    }

    return h;
}

/***************************************************************************/
// implement JSObject2WrappedJSMap...

void
JSObject2WrappedJSMap::FindDyingJSObjects(nsTArray<nsXPCWrappedJS*>* dying)
{
    for (Map::Range r = mTable.all(); !r.empty(); r.popFront()) {
        nsXPCWrappedJS* wrapper = r.front().value;
        NS_ASSERTION(wrapper, "found a null JS wrapper!");

        // walk the wrapper chain and find any whose JSObject is to be finalized
        while (wrapper) {
            if (wrapper->IsSubjectToFinalization() && wrapper->IsObjectAboutToBeFinalized())
                dying->AppendElement(wrapper);
            wrapper = wrapper->GetNextWrapper();
        }
    }
}

void
JSObject2WrappedJSMap::ShutdownMarker(JSRuntime* rt)
{
    for (Map::Range r = mTable.all(); !r.empty(); r.popFront()) {
        nsXPCWrappedJS* wrapper = r.front().value;
        NS_ASSERTION(wrapper, "found a null JS wrapper!");
        NS_ASSERTION(wrapper->IsValid(), "found an invalid JS wrapper!");
        wrapper->SystemIsBeingShutDown(rt);
    }
}

/***************************************************************************/
// implement Native2WrappedNativeMap...

// static
Native2WrappedNativeMap*
Native2WrappedNativeMap::newMap(int size)
{
    Native2WrappedNativeMap* map = new Native2WrappedNativeMap(size);
    if (map && map->mTable)
        return map;
    // Allocation of the map or the creation of its hash table has
    // failed. This will cause a NULL deref later when we attempt to
    // use the map, so we abort immediately to provide a more useful
    // crash stack.
    NS_RUNTIMEABORT("Ran out of memory.");
    return nullptr;
}

Native2WrappedNativeMap::Native2WrappedNativeMap(int size)
{
    mTable = JS_NewDHashTable(JS_DHashGetStubOps(), nullptr,
                              sizeof(Entry), size);
}

Native2WrappedNativeMap::~Native2WrappedNativeMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

size_t
Native2WrappedNativeMap::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mTable ? JS_DHashTableSizeOfIncludingThis(mTable, SizeOfEntryExcludingThis, mallocSizeOf) : 0;
    return n;
}

/* static */ size_t
Native2WrappedNativeMap::SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr,
                                                  JSMallocSizeOfFun mallocSizeOf, void *)
{
    return mallocSizeOf(((Native2WrappedNativeMap::Entry*)hdr)->value);
}

/***************************************************************************/
// implement IID2WrappedJSClassMap...

struct JSDHashTableOps IID2WrappedJSClassMap::Entry::sOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    HashIIDPtrKey,
    MatchIIDPtrKey,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub
};

// static
IID2WrappedJSClassMap*
IID2WrappedJSClassMap::newMap(int size)
{
    IID2WrappedJSClassMap* map = new IID2WrappedJSClassMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

IID2WrappedJSClassMap::IID2WrappedJSClassMap(int size)
{
    mTable = JS_NewDHashTable(&Entry::sOps, nullptr, sizeof(Entry), size);
}

IID2WrappedJSClassMap::~IID2WrappedJSClassMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}


/***************************************************************************/
// implement IID2NativeInterfaceMap...

struct JSDHashTableOps IID2NativeInterfaceMap::Entry::sOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    HashIIDPtrKey,
    MatchIIDPtrKey,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub
};

// static
IID2NativeInterfaceMap*
IID2NativeInterfaceMap::newMap(int size)
{
    IID2NativeInterfaceMap* map = new IID2NativeInterfaceMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

IID2NativeInterfaceMap::IID2NativeInterfaceMap(int size)
{
    mTable = JS_NewDHashTable(&Entry::sOps, nullptr, sizeof(Entry), size);
}

IID2NativeInterfaceMap::~IID2NativeInterfaceMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

size_t
IID2NativeInterfaceMap::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mTable ? JS_DHashTableSizeOfIncludingThis(mTable, SizeOfEntryExcludingThis, mallocSizeOf) : 0;
    return n;
}

/* static */ size_t
IID2NativeInterfaceMap::SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr,
                                                 JSMallocSizeOfFun mallocSizeOf, void *)
{
    XPCNativeInterface *iface = ((IID2NativeInterfaceMap::Entry*)hdr)->value;
    return iface->SizeOfIncludingThis(mallocSizeOf);
}

/***************************************************************************/
// implement ClassInfo2NativeSetMap...

// static
ClassInfo2NativeSetMap*
ClassInfo2NativeSetMap::newMap(int size)
{
    ClassInfo2NativeSetMap* map = new ClassInfo2NativeSetMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

ClassInfo2NativeSetMap::ClassInfo2NativeSetMap(int size)
{
    mTable = JS_NewDHashTable(JS_DHashGetStubOps(), nullptr,
                              sizeof(Entry), size);
}

ClassInfo2NativeSetMap::~ClassInfo2NativeSetMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

size_t
ClassInfo2NativeSetMap::ShallowSizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    // The second arg is NULL because this is a "shallow" measurement of the map.
    n += mTable ? JS_DHashTableSizeOfIncludingThis(mTable, NULL, mallocSizeOf) : 0;
    return n;
}

/***************************************************************************/
// implement ClassInfo2WrappedNativeProtoMap...

// static
ClassInfo2WrappedNativeProtoMap*
ClassInfo2WrappedNativeProtoMap::newMap(int size)
{
    ClassInfo2WrappedNativeProtoMap* map = new ClassInfo2WrappedNativeProtoMap(size);
    if (map && map->mTable)
        return map;
    // Allocation of the map or the creation of its hash table has
    // failed. This will cause a NULL deref later when we attempt to
    // use the map, so we abort immediately to provide a more useful
    // crash stack.
    NS_RUNTIMEABORT("Ran out of memory.");
    return nullptr;
}

ClassInfo2WrappedNativeProtoMap::ClassInfo2WrappedNativeProtoMap(int size)
{
    mTable = JS_NewDHashTable(JS_DHashGetStubOps(), nullptr,
                              sizeof(Entry), size);
}

ClassInfo2WrappedNativeProtoMap::~ClassInfo2WrappedNativeProtoMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

size_t
ClassInfo2WrappedNativeProtoMap::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mTable ? JS_DHashTableSizeOfIncludingThis(mTable, SizeOfEntryExcludingThis, mallocSizeOf) : 0;
    return n;
}

/* static */ size_t
ClassInfo2WrappedNativeProtoMap::SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr,
                                                          JSMallocSizeOfFun mallocSizeOf, void *)
{
    return mallocSizeOf(((ClassInfo2WrappedNativeProtoMap::Entry*)hdr)->value);
}

/***************************************************************************/
// implement NativeSetMap...

JSBool
NativeSetMap::Entry::Match(JSDHashTable *table,
                           const JSDHashEntryHdr *entry,
                           const void *key)
{
    XPCNativeSetKey* Key = (XPCNativeSetKey*) key;

    // See the comment in the XPCNativeSetKey declaration in xpcprivate.h.
    if (!Key->IsAKey()) {
        XPCNativeSet* Set1 = (XPCNativeSet*) key;
        XPCNativeSet* Set2 = ((Entry*)entry)->key_value;

        if (Set1 == Set2)
            return true;

        uint16_t count = Set1->GetInterfaceCount();
        if (count != Set2->GetInterfaceCount())
            return false;

        XPCNativeInterface** Current1 = Set1->GetInterfaceArray();
        XPCNativeInterface** Current2 = Set2->GetInterfaceArray();
        for (uint16_t i = 0; i < count; i++) {
            if (*(Current1++) != *(Current2++))
                return false;
        }

        return true;
    }

    XPCNativeSet*       SetInTable = ((Entry*)entry)->key_value;
    XPCNativeSet*       Set        = Key->GetBaseSet();
    XPCNativeInterface* Addition   = Key->GetAddition();

    if (!Set) {
        // This is a special case to deal with the invariant that says:
        // "All sets have exactly one nsISupports interface and it comes first."
        // See XPCNativeSet::NewInstance for details.
        //
        // Though we might have a key that represents only one interface, we
        // know that if that one interface were contructed into a set then
        // it would end up really being a set with two interfaces (except for
        // the case where the one interface happened to be nsISupports).

        return ((SetInTable->GetInterfaceCount() == 1 &&
                 SetInTable->GetInterfaceAt(0) == Addition) ||
                (SetInTable->GetInterfaceCount() == 2 &&
                 SetInTable->GetInterfaceAt(1) == Addition));
    }

    if (!Addition && Set == SetInTable)
        return true;

    uint16_t count = Set->GetInterfaceCount() + (Addition ? 1 : 0);
    if (count != SetInTable->GetInterfaceCount())
        return false;

    uint16_t Position = Key->GetPosition();
    XPCNativeInterface** CurrentInTable = SetInTable->GetInterfaceArray();
    XPCNativeInterface** Current = Set->GetInterfaceArray();
    for (uint16_t i = 0; i < count; i++) {
        if (Addition && i == Position) {
            if (Addition != *(CurrentInTable++))
                return false;
        } else {
            if (*(Current++) != *(CurrentInTable++))
                return false;
        }
    }

    return true;
}

struct JSDHashTableOps NativeSetMap::Entry::sOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    HashNativeKey,
    Match,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub
};

// static
NativeSetMap*
NativeSetMap::newMap(int size)
{
    NativeSetMap* map = new NativeSetMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

NativeSetMap::NativeSetMap(int size)
{
    mTable = JS_NewDHashTable(&Entry::sOps, nullptr, sizeof(Entry), size);
}

NativeSetMap::~NativeSetMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

size_t
NativeSetMap::SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf)
{
    size_t n = 0;
    n += mallocSizeOf(this);
    n += mTable ? JS_DHashTableSizeOfIncludingThis(mTable, SizeOfEntryExcludingThis, mallocSizeOf) : 0;
    return n;
}

/* static */ size_t
NativeSetMap::SizeOfEntryExcludingThis(JSDHashEntryHdr *hdr, JSMallocSizeOfFun mallocSizeOf, void *)
{
    XPCNativeSet *set = ((NativeSetMap::Entry*)hdr)->key_value;
    return set->SizeOfIncludingThis(mallocSizeOf);
}

/***************************************************************************/
// implement IID2ThisTranslatorMap...

JSBool
IID2ThisTranslatorMap::Entry::Match(JSDHashTable *table,
                                    const JSDHashEntryHdr *entry,
                                    const void *key)
{
    return ((const nsID*)key)->Equals(((Entry*)entry)->key);
}

void
IID2ThisTranslatorMap::Entry::Clear(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    NS_IF_RELEASE(((Entry*)entry)->value);
    memset(entry, 0, table->entrySize);
}

struct JSDHashTableOps IID2ThisTranslatorMap::Entry::sOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    HashIIDPtrKey,
    Match,
    JS_DHashMoveEntryStub,
    Clear,
    JS_DHashFinalizeStub
};

// static
IID2ThisTranslatorMap*
IID2ThisTranslatorMap::newMap(int size)
{
    IID2ThisTranslatorMap* map = new IID2ThisTranslatorMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

IID2ThisTranslatorMap::IID2ThisTranslatorMap(int size)
{
    mTable = JS_NewDHashTable(&Entry::sOps, nullptr, sizeof(Entry), size);
}

IID2ThisTranslatorMap::~IID2ThisTranslatorMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

/***************************************************************************/

JSDHashNumber
XPCNativeScriptableSharedMap::Entry::Hash(JSDHashTable *table, const void *key)
{
    JSDHashNumber h;
    const unsigned char *s;

    XPCNativeScriptableShared* obj =
        (XPCNativeScriptableShared*) key;

    // hash together the flags and the classname string, ignore the interfaces
    // bitmap since it's very rare that it's different when flags and classname
    // are the same.

    h = (JSDHashNumber) obj->GetFlags();
    for (s = (const unsigned char*) obj->GetJSClass()->name; *s != '\0'; s++)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;
    return h;
}

JSBool
XPCNativeScriptableSharedMap::Entry::Match(JSDHashTable *table,
                                           const JSDHashEntryHdr *entry,
                                           const void *key)
{
    XPCNativeScriptableShared* obj1 =
        ((XPCNativeScriptableSharedMap::Entry*) entry)->key;

    XPCNativeScriptableShared* obj2 =
        (XPCNativeScriptableShared*) key;

    // match the flags, the classname string and the interfaces bitmap

    if (obj1->GetFlags() != obj2->GetFlags() ||
        obj1->GetInterfacesBitmap() != obj2->GetInterfacesBitmap())
        return false;

    const char* name1 = obj1->GetJSClass()->name;
    const char* name2 = obj2->GetJSClass()->name;

    if (!name1 || !name2)
        return name1 == name2;

    return 0 == strcmp(name1, name2);
}

struct JSDHashTableOps XPCNativeScriptableSharedMap::Entry::sOps =
{
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    Hash,
    Match,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub
};

// static
XPCNativeScriptableSharedMap*
XPCNativeScriptableSharedMap::newMap(int size)
{
    XPCNativeScriptableSharedMap* map =
        new XPCNativeScriptableSharedMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

XPCNativeScriptableSharedMap::XPCNativeScriptableSharedMap(int size)
{
    mTable = JS_NewDHashTable(&Entry::sOps, nullptr, sizeof(Entry), size);
}

XPCNativeScriptableSharedMap::~XPCNativeScriptableSharedMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

JSBool
XPCNativeScriptableSharedMap::GetNewOrUsed(uint32_t flags,
                                           char* name,
                                           uint32_t interfacesBitmap,
                                           XPCNativeScriptableInfo* si)
{
    NS_PRECONDITION(name,"bad param");
    NS_PRECONDITION(si,"bad param");

    XPCNativeScriptableShared key(flags, name, interfacesBitmap);
    Entry* entry = (Entry*)
        JS_DHashTableOperate(mTable, &key, JS_DHASH_ADD);
    if (!entry)
        return false;

    XPCNativeScriptableShared* shared = entry->key;

    if (!shared) {
        entry->key = shared =
            new XPCNativeScriptableShared(flags, key.TransferNameOwnership(),
                                          interfacesBitmap);
        if (!shared)
            return false;
        shared->PopulateJSClass();
    }
    si->SetScriptableShared(shared);
    return true;
}

/***************************************************************************/
// implement XPCWrappedNativeProtoMap...

// static
XPCWrappedNativeProtoMap*
XPCWrappedNativeProtoMap::newMap(int size)
{
    XPCWrappedNativeProtoMap* map = new XPCWrappedNativeProtoMap(size);
    if (map && map->mTable)
        return map;
    delete map;
    return nullptr;
}

XPCWrappedNativeProtoMap::XPCWrappedNativeProtoMap(int size)
{
    mTable = JS_NewDHashTable(JS_DHashGetStubOps(), nullptr,
                              sizeof(JSDHashEntryStub), size);
}

XPCWrappedNativeProtoMap::~XPCWrappedNativeProtoMap()
{
    if (mTable)
        JS_DHashTableDestroy(mTable);
}

/***************************************************************************/
