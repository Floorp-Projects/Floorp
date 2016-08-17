/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Private maps (hashtables). */

#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "xpcprivate.h"

#include "js/HashTable.h"

using namespace mozilla;

/***************************************************************************/
// static shared...

// Note this is returning the bit pattern of the first part of the nsID, not
// the pointer to the nsID.

static PLDHashNumber
HashIIDPtrKey(const void* key)
{
    return *((js::HashNumber*)key);
}

static bool
MatchIIDPtrKey(const PLDHashEntryHdr* entry, const void* key)
{
    return ((const nsID*)key)->
                Equals(*((const nsID*)((PLDHashEntryStub*)entry)->key));
}

static PLDHashNumber
HashNativeKey(const void* data)
{
    return static_cast<const XPCNativeSetKey*>(data)->Hash();
}

/***************************************************************************/
// implement JSObject2WrappedJSMap...

void
JSObject2WrappedJSMap::UpdateWeakPointersAfterGC(XPCJSRuntime* runtime)
{
    // Check all wrappers and update their JSObject pointer if it has been
    // moved. Release any wrappers whose weakly held JSObject has died.

    nsTArray<RefPtr<nsXPCWrappedJS>> dying;
    for (Map::Enum e(mTable); !e.empty(); e.popFront()) {
        nsXPCWrappedJS* wrapper = e.front().value();
        MOZ_ASSERT(wrapper, "found a null JS wrapper!");

        // Walk the wrapper chain and update all JSObjects.
        while (wrapper) {
#ifdef DEBUG
            if (!wrapper->IsSubjectToFinalization()) {
                // If a wrapper is not subject to finalization then it roots its
                // JS object.  If so, then it will not be about to be finalized
                // and any necessary pointer update will have already happened
                // when it was marked.
                JSObject* obj = wrapper->GetJSObjectPreserveColor();
                JSObject* prior = obj;
                JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
                MOZ_ASSERT(obj == prior);
            }
#endif
            if (wrapper->IsSubjectToFinalization()) {
                wrapper->UpdateObjectPointerAfterGC();
                if (!wrapper->GetJSObjectPreserveColor())
                    dying.AppendElement(dont_AddRef(wrapper));
            }
            wrapper = wrapper->GetNextWrapper();
        }

        // Remove or update the JSObject key in the table if necessary.
        JSObject* obj = e.front().key();
        JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
        if (!obj)
            e.removeFront();
        else
            e.front().mutableKey() = obj;
    }
}

void
JSObject2WrappedJSMap::ShutdownMarker()
{
    for (Map::Range r = mTable.all(); !r.empty(); r.popFront()) {
        nsXPCWrappedJS* wrapper = r.front().value();
        MOZ_ASSERT(wrapper, "found a null JS wrapper!");
        MOZ_ASSERT(wrapper->IsValid(), "found an invalid JS wrapper!");
        wrapper->SystemIsBeingShutDown();
    }
}

size_t
JSObject2WrappedJSMap::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mTable.sizeOfExcludingThis(mallocSizeOf);
    return n;
}

size_t
JSObject2WrappedJSMap::SizeOfWrappedJS(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = 0;
    for (Map::Range r = mTable.all(); !r.empty(); r.popFront())
        n += r.front().value()->SizeOfIncludingThis(mallocSizeOf);
    return n;
}

/***************************************************************************/
// implement Native2WrappedNativeMap...

// static
Native2WrappedNativeMap*
Native2WrappedNativeMap::newMap(int length)
{
    return new Native2WrappedNativeMap(length);
}

Native2WrappedNativeMap::Native2WrappedNativeMap(int length)
  : mTable(PLDHashTable::StubOps(), sizeof(Entry), length)
{
}

size_t
Native2WrappedNativeMap::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<Native2WrappedNativeMap::Entry*>(iter.Get());
        n += mallocSizeOf(entry->value);
    }
    return n;
}

/***************************************************************************/
// implement IID2WrappedJSClassMap...

const struct PLDHashTableOps IID2WrappedJSClassMap::Entry::sOps =
{
    HashIIDPtrKey,
    MatchIIDPtrKey,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
};

// static
IID2WrappedJSClassMap*
IID2WrappedJSClassMap::newMap(int length)
{
    return new IID2WrappedJSClassMap(length);
}

IID2WrappedJSClassMap::IID2WrappedJSClassMap(int length)
  : mTable(&Entry::sOps, sizeof(Entry), length)
{
}

/***************************************************************************/
// implement IID2NativeInterfaceMap...

const struct PLDHashTableOps IID2NativeInterfaceMap::Entry::sOps =
{
    HashIIDPtrKey,
    MatchIIDPtrKey,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
};

// static
IID2NativeInterfaceMap*
IID2NativeInterfaceMap::newMap(int length)
{
    return new IID2NativeInterfaceMap(length);
}

IID2NativeInterfaceMap::IID2NativeInterfaceMap(int length)
  : mTable(&Entry::sOps, sizeof(Entry), length)
{
}

size_t
IID2NativeInterfaceMap::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<IID2NativeInterfaceMap::Entry*>(iter.Get());
        n += entry->value->SizeOfIncludingThis(mallocSizeOf);
    }
    return n;
}

/***************************************************************************/
// implement ClassInfo2NativeSetMap...

// static
ClassInfo2NativeSetMap*
ClassInfo2NativeSetMap::newMap(int length)
{
    return new ClassInfo2NativeSetMap(length);
}

ClassInfo2NativeSetMap::ClassInfo2NativeSetMap(int length)
  : mTable(PLDHashTable::StubOps(), sizeof(Entry), length)
{
}

size_t
ClassInfo2NativeSetMap::ShallowSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    size_t n = mallocSizeOf(this);
    n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
    return n;
}

/***************************************************************************/
// implement ClassInfo2WrappedNativeProtoMap...

// static
ClassInfo2WrappedNativeProtoMap*
ClassInfo2WrappedNativeProtoMap::newMap(int length)
{
    return new ClassInfo2WrappedNativeProtoMap(length);
}

ClassInfo2WrappedNativeProtoMap::ClassInfo2WrappedNativeProtoMap(int length)
  : mTable(PLDHashTable::StubOps(), sizeof(Entry), length)
{
}

size_t
ClassInfo2WrappedNativeProtoMap::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(iter.Get());
        n += mallocSizeOf(entry->value);
    }
    return n;
}

/***************************************************************************/
// implement NativeSetMap...

bool
NativeSetMap::Entry::Match(const PLDHashEntryHdr* entry, const void* key)
{
    auto Key = static_cast<const XPCNativeSetKey*>(key);
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

        return (SetInTable->GetInterfaceCount() == 1 &&
                SetInTable->GetInterfaceAt(0) == Addition) ||
               (SetInTable->GetInterfaceCount() == 2 &&
                SetInTable->GetInterfaceAt(1) == Addition);
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

const struct PLDHashTableOps NativeSetMap::Entry::sOps =
{
    HashNativeKey,
    Match,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
};

// static
NativeSetMap*
NativeSetMap::newMap(int length)
{
    return new NativeSetMap(length);
}

NativeSetMap::NativeSetMap(int length)
  : mTable(&Entry::sOps, sizeof(Entry), length)
{
}

size_t
NativeSetMap::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
    for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
        auto entry = static_cast<NativeSetMap::Entry*>(iter.Get());
        n += entry->key_value->SizeOfIncludingThis(mallocSizeOf);
    }
    return n;
}

/***************************************************************************/
// implement IID2ThisTranslatorMap...

bool
IID2ThisTranslatorMap::Entry::Match(const PLDHashEntryHdr* entry,
                                    const void* key)
{
    return ((const nsID*)key)->Equals(((Entry*)entry)->key);
}

void
IID2ThisTranslatorMap::Entry::Clear(PLDHashTable* table, PLDHashEntryHdr* entry)
{
    static_cast<Entry*>(entry)->value = nullptr;
    memset(entry, 0, table->EntrySize());
}

const struct PLDHashTableOps IID2ThisTranslatorMap::Entry::sOps =
{
    HashIIDPtrKey,
    Match,
    PLDHashTable::MoveEntryStub,
    Clear
};

// static
IID2ThisTranslatorMap*
IID2ThisTranslatorMap::newMap(int length)
{
    return new IID2ThisTranslatorMap(length);
}

IID2ThisTranslatorMap::IID2ThisTranslatorMap(int length)
  : mTable(&Entry::sOps, sizeof(Entry), length)
{
}

/***************************************************************************/

PLDHashNumber
XPCNativeScriptableSharedMap::Entry::Hash(const void* key)
{
    PLDHashNumber h;
    const unsigned char* s;

    XPCNativeScriptableShared* obj =
        (XPCNativeScriptableShared*) key;

    // hash together the flags and the classname string, ignore the interfaces
    // bitmap since it's very rare that it's different when flags and classname
    // are the same.

    h = (PLDHashNumber) obj->GetFlags();
    for (s = (const unsigned char*) obj->GetJSClass()->name; *s != '\0'; s++)
        h = RotateLeft(h, 4) ^ *s;
    return h;
}

bool
XPCNativeScriptableSharedMap::Entry::Match(const PLDHashEntryHdr* entry,
                                           const void* key)
{
    XPCNativeScriptableShared* obj1 =
        ((XPCNativeScriptableSharedMap::Entry*) entry)->key;

    XPCNativeScriptableShared* obj2 =
        (XPCNativeScriptableShared*) key;

    // match the flags and the classname string

    if (obj1->GetFlags() != obj2->GetFlags())
        return false;

    const char* name1 = obj1->GetJSClass()->name;
    const char* name2 = obj2->GetJSClass()->name;

    if (!name1 || !name2)
        return name1 == name2;

    return 0 == strcmp(name1, name2);
}

const struct PLDHashTableOps XPCNativeScriptableSharedMap::Entry::sOps =
{
    Hash,
    Match,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub
};

// static
XPCNativeScriptableSharedMap*
XPCNativeScriptableSharedMap::newMap(int length)
{
    return new XPCNativeScriptableSharedMap(length);
}

XPCNativeScriptableSharedMap::XPCNativeScriptableSharedMap(int length)
  : mTable(&Entry::sOps, sizeof(Entry), length)
{
}

bool
XPCNativeScriptableSharedMap::GetNewOrUsed(uint32_t flags,
                                           char* name,
                                           XPCNativeScriptableInfo* si)
{
    NS_PRECONDITION(name,"bad param");
    NS_PRECONDITION(si,"bad param");

    RefPtr<XPCNativeScriptableShared> key =
        new XPCNativeScriptableShared(flags, name, /* populate = */ false);
    auto entry = static_cast<Entry*>(mTable.Add(key, fallible));
    if (!entry)
        return false;

    RefPtr<XPCNativeScriptableShared> shared = entry->key;

    // XXX: this XPCNativeScriptableShared is heap-allocated, which means the
    // js::Class it contains is also heap-allocated. This causes problems for
    // memory reporting. See the comment above the BaseShape case in
    // StatsCellCallback() in js/src/vm/MemoryMetrics.cpp.
    //
    // When the code below is removed (bug 1265271) and there are no longer any
    // heap-allocated js::Class instances, the disabled code in
    // StatsCellCallback() should be reinstated.
    //
    if (!shared) {
        shared = new XPCNativeScriptableShared(flags, key->TransferNameOwnership(),
                                               /* populate = */ true);
        entry->key = shared;
    }
    si->SetScriptableShared(shared.forget());
    return true;
}

/***************************************************************************/
// implement XPCWrappedNativeProtoMap...

// static
XPCWrappedNativeProtoMap*
XPCWrappedNativeProtoMap::newMap(int length)
{
    return new XPCWrappedNativeProtoMap(length);
}

XPCWrappedNativeProtoMap::XPCWrappedNativeProtoMap(int length)
  : mTable(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub), length)
{
}

/***************************************************************************/
