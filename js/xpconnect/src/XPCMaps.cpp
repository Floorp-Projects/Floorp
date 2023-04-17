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
// implement JSObject2WrappedJSMap...

void JSObject2WrappedJSMap::UpdateWeakPointersAfterGC(JSTracer* trc) {
  // Check all wrappers and update their JSObject pointer if it has been
  // moved. Release any wrappers whose weakly held JSObject has died.

  nsTArray<RefPtr<nsXPCWrappedJS>> dying;
  for (auto iter = mTable.modIter(); !iter.done(); iter.next()) {
    nsXPCWrappedJS* wrapper = iter.get().value();
    MOZ_ASSERT(wrapper, "found a null JS wrapper!");

    // There's no need to walk the entire chain, because only the root can be
    // subject to finalization due to the double release behavior in Release.
    // See the comment at the top of XPCWrappedJS.cpp about nsXPCWrappedJS
    // lifetime.
    if (wrapper && wrapper->IsSubjectToFinalization()) {
      wrapper->UpdateObjectPointerAfterGC(trc);
      if (!wrapper->GetJSObjectPreserveColor()) {
        dying.AppendElement(dont_AddRef(wrapper));
      }
    }

    // Remove or update the JSObject key in the table if necessary.
    if (!JS_UpdateWeakPointerAfterGC(trc, &iter.get().mutableKey())) {
      iter.remove();
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

Native2WrappedNativeMap::Native2WrappedNativeMap()
    : mMap(XPC_NATIVE_MAP_LENGTH) {}

size_t Native2WrappedNativeMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mMap.shallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mMap.iter(); !iter.done(); iter.next()) {
    n += mallocSizeOf(iter.get().value());
  }
  return n;
}

/***************************************************************************/
// implement IID2NativeInterfaceMap...

IID2NativeInterfaceMap::IID2NativeInterfaceMap()
    : mMap(XPC_NATIVE_INTERFACE_MAP_LENGTH) {}

size_t IID2NativeInterfaceMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mMap.shallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mMap.iter(); !iter.done(); iter.next()) {
    n += iter.get().value()->SizeOfIncludingThis(mallocSizeOf);
  }
  return n;
}

/***************************************************************************/
// implement ClassInfo2NativeSetMap...

ClassInfo2NativeSetMap::ClassInfo2NativeSetMap()
    : mMap(XPC_NATIVE_SET_MAP_LENGTH) {}

size_t ClassInfo2NativeSetMap::ShallowSizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) {
  size_t n = mallocSizeOf(this);
  n += mMap.shallowSizeOfExcludingThis(mallocSizeOf);
  return n;
}

/***************************************************************************/
// implement ClassInfo2WrappedNativeProtoMap...

ClassInfo2WrappedNativeProtoMap::ClassInfo2WrappedNativeProtoMap()
    : mMap(XPC_NATIVE_PROTO_MAP_LENGTH) {}

size_t ClassInfo2WrappedNativeProtoMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mMap.shallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mMap.iter(); !iter.done(); iter.next()) {
    n += mallocSizeOf(iter.get().value());
  }
  return n;
}

/***************************************************************************/
// implement NativeSetMap...

bool NativeSetHasher::match(Key key, Lookup lookup) {
  // The |key| argument is for the existing table entry and |lookup| is the
  // value passed by the caller that is being compared with it.
  XPCNativeSet* SetInTable = key;
  XPCNativeSet* Set = lookup->GetBaseSet();
  XPCNativeInterface* Addition = lookup->GetAddition();

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

NativeSetMap::NativeSetMap() : mSet(XPC_NATIVE_SET_MAP_LENGTH) {}

size_t NativeSetMap::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);
  n += mSet.shallowSizeOfExcludingThis(mallocSizeOf);
  for (auto iter = mSet.iter(); !iter.done(); iter.next()) {
    n += iter.get()->SizeOfIncludingThis(mallocSizeOf);
  }
  return n;
}

/***************************************************************************/
