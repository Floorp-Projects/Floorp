/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

// Note this is returning the hash of the bit pattern of the first part of the
// nsID, not the hash of the pointer to the nsID.

static PLDHashNumber HashIIDPtrKey(const void* key) {
  uintptr_t v;
  memcpy(&v, key, sizeof(v));
  return HashGeneric(v);
}

static bool MatchIIDPtrKey(const PLDHashEntryHdr* entry, const void* key) {
  return ((const nsID*)key)
      ->Equals(*((const nsID*)((PLDHashEntryStub*)entry)->key));
}

static PLDHashNumber HashNativeKey(const void* data) {
  return static_cast<const XPCNativeSetKey*>(data)->Hash();
}

/***************************************************************************/
// implement JSObject2WrappedJSMap...

void JSObject2WrappedJSMap::UpdateWeakPointersAfterGC() {
  // Check all wrappers and update their JSObject pointer if it has been
  // moved. Release any wrappers whose weakly held JSObject has died.

  nsTArray<RefPtr<nsXPCWrappedJS>> dying;
  for (auto iter = mTable.modIter(); !iter.done(); iter.next()) {
    nsXPCWrappedJS* wrapper = iter.get().value();
    MOZ_ASSERT(wrapper, "found a null JS wrapper!");

    // Walk the wrapper chain and update all JSObjects.
    while (wrapper) {
      if (wrapper->IsSubjectToFinalization()) {
        wrapper->UpdateObjectPointerAfterGC();
        if (!wrapper->GetJSObjectPreserveColor()) {
          dying.AppendElement(dont_AddRef(wrapper));
        }
      }
      wrapper = wrapper->GetNextWrapper();
    }

    // Remove or update the JSObject key in the table if necessary.
    JSObject* obj = iter.get().key().unbarrieredGet();
    JS_UpdateWeakPointerAfterGCUnbarriered(&obj);
    if (!obj) {
      iter.remove();
    } else {
      iter.get().mutableKey() = obj;
    }
  }
}

void JSObject2WrappedJSMap::ShutdownMarker() {
  for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
    nsXPCWrappedJS* wrapper = iter.get().value();
    MOZ_ASSERT(wrapper, "found a null JS wrapper!");
    MOZ_ASSERT(wrapper->IsValid(), "found an invalid JS wrapper!");
    wrapper->SystemIsBeingShutDown();
  }
}

size_t JSObject2WrappedJSMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mTable.shallowSizeOfExcludingThis(mallocSizeOf);
  return n;
}

size_t JSObject2WrappedJSMap::SizeOfWrappedJS(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = 0;
  for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
    n += iter.get().value()->SizeOfIncludingThis(mallocSizeOf);
  }
  return n;
}

/***************************************************************************/
// implement Native2WrappedNativeMap...

// static
Native2WrappedNativeMap* Native2WrappedNativeMap::newMap(int length) {
  return new Native2WrappedNativeMap(length);
}

Native2WrappedNativeMap::Native2WrappedNativeMap(int length)
    : mTable(PLDHashTable::StubOps(), sizeof(Entry), length) {}

size_t Native2WrappedNativeMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<Native2WrappedNativeMap::Entry*>(iter.Get());
    n += mallocSizeOf(entry->value);
  }
  return n;
}

/***************************************************************************/
// implement IID2NativeInterfaceMap...

const struct PLDHashTableOps IID2NativeInterfaceMap::Entry::sOps = {
    HashIIDPtrKey, MatchIIDPtrKey, PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub};

// static
IID2NativeInterfaceMap* IID2NativeInterfaceMap::newMap(int length) {
  return new IID2NativeInterfaceMap(length);
}

IID2NativeInterfaceMap::IID2NativeInterfaceMap(int length)
    : mTable(&Entry::sOps, sizeof(Entry), length) {}

size_t IID2NativeInterfaceMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
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
bool ClassInfo2NativeSetMap::Entry::Match(const PLDHashEntryHdr* aEntry,
                                          const void* aKey) {
  return static_cast<const Entry*>(aEntry)->key == aKey;
}

// static
void ClassInfo2NativeSetMap::Entry::Clear(PLDHashTable* aTable,
                                          PLDHashEntryHdr* aEntry) {
  auto entry = static_cast<Entry*>(aEntry);
  NS_RELEASE(entry->value);

  entry->key = nullptr;
  entry->value = nullptr;
}

const PLDHashTableOps ClassInfo2NativeSetMap::Entry::sOps = {
    PLDHashTable::HashVoidPtrKeyStub, Match, PLDHashTable::MoveEntryStub, Clear,
    nullptr};

// static
ClassInfo2NativeSetMap* ClassInfo2NativeSetMap::newMap(int length) {
  return new ClassInfo2NativeSetMap(length);
}

ClassInfo2NativeSetMap::ClassInfo2NativeSetMap(int length)
    : mTable(&ClassInfo2NativeSetMap::Entry::sOps, sizeof(Entry), length) {}

size_t ClassInfo2NativeSetMap::ShallowSizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) {
  size_t n = mallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  return n;
}

/***************************************************************************/
// implement ClassInfo2WrappedNativeProtoMap...

// static
ClassInfo2WrappedNativeProtoMap* ClassInfo2WrappedNativeProtoMap::newMap(
    int length) {
  return new ClassInfo2WrappedNativeProtoMap(length);
}

ClassInfo2WrappedNativeProtoMap::ClassInfo2WrappedNativeProtoMap(int length)
    : mTable(PLDHashTable::StubOps(), sizeof(Entry), length) {}

size_t ClassInfo2WrappedNativeProtoMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry =
        static_cast<ClassInfo2WrappedNativeProtoMap::Entry*>(iter.Get());
    n += mallocSizeOf(entry->value);
  }
  return n;
}

/***************************************************************************/
// implement NativeSetMap...

bool NativeSetMap::Entry::Match(const PLDHashEntryHdr* entry, const void* key) {
  auto Key = static_cast<const XPCNativeSetKey*>(key);
  XPCNativeSet* SetInTable = ((Entry*)entry)->key_value;
  XPCNativeSet* Set = Key->GetBaseSet();
  XPCNativeInterface* Addition = Key->GetAddition();

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

  if (!Addition && Set == SetInTable) {
    return true;
  }

  uint16_t count = Set->GetInterfaceCount();
  if (count + (Addition ? 1 : 0) != SetInTable->GetInterfaceCount()) {
    return false;
  }

  XPCNativeInterface** CurrentInTable = SetInTable->GetInterfaceArray();
  XPCNativeInterface** Current = Set->GetInterfaceArray();
  for (uint16_t i = 0; i < count; i++) {
    if (*(Current++) != *(CurrentInTable++)) {
      return false;
    }
  }
  return !Addition || Addition == *(CurrentInTable++);
}

const struct PLDHashTableOps NativeSetMap::Entry::sOps = {
    HashNativeKey, Match, PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub};

// static
NativeSetMap* NativeSetMap::newMap(int length) {
  return new NativeSetMap(length);
}

NativeSetMap::NativeSetMap(int length)
    : mTable(&Entry::sOps, sizeof(Entry), length) {}

size_t NativeSetMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mTable.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<NativeSetMap::Entry*>(iter.Get());
    n += entry->key_value->SizeOfIncludingThis(mallocSizeOf);
  }
  return n;
}

/***************************************************************************/
// implement XPCWrappedNativeProtoMap...

// static
XPCWrappedNativeProtoMap* XPCWrappedNativeProtoMap::newMap(int length) {
  return new XPCWrappedNativeProtoMap(length);
}

XPCWrappedNativeProtoMap::XPCWrappedNativeProtoMap(int length)
    : mTable(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub), length) {}

/***************************************************************************/
