/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/**
 * nsPropertyTable allows a set of arbitrary key/value pairs to be stored
 * for any number of nodes, in a global hashtable rather than on the nodes
 * themselves.  Nodes can be any type of object; the hashtable keys are
 * nsIAtom pointers, and the values are void pointers.
 */

#include "nsPropertyTable.h"

#include "mozilla/MemoryReporting.h"

#include "PLDHashTable.h"
#include "nsError.h"
#include "nsIAtom.h"

struct PropertyListMapEntry : public PLDHashEntryHdr {
  const void  *key;
  void        *value;
};

//----------------------------------------------------------------------

class nsPropertyTable::PropertyList {
public:
  PropertyList(nsIAtom*           aName,
               NSPropertyDtorFunc aDtorFunc,
               void*              aDtorData,
               bool               aTransfer);
  ~PropertyList();

  // Removes the property associated with the given object, and destroys
  // the property value
  bool DeletePropertyFor(nsPropertyOwner aObject);

  // Destroy all remaining properties (without removing them)
  void Destroy();

  bool Equals(nsIAtom *aPropertyName)
  {
    return mName == aPropertyName;
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

  nsCOMPtr<nsIAtom>  mName;           // property name
  PLDHashTable       mObjectValueMap; // map of object/value pairs
  NSPropertyDtorFunc mDtorFunc;       // property specific value dtor function
  void*              mDtorData;       // pointer to pass to dtor
  bool               mTransfer;       // whether to transfer in
                                      // TransferOrDeleteAllPropertiesFor
  
  PropertyList*      mNext;
};

void
nsPropertyTable::DeleteAllProperties()
{
  while (mPropertyList) {
    PropertyList* tmp = mPropertyList;

    mPropertyList = mPropertyList->mNext;
    tmp->Destroy();
    delete tmp;
  }
}

void
nsPropertyTable::DeleteAllPropertiesFor(nsPropertyOwner aObject)
{
  for (PropertyList* prop = mPropertyList; prop; prop = prop->mNext) {
    prop->DeletePropertyFor(aObject);
  }
}

nsresult
nsPropertyTable::TransferOrDeleteAllPropertiesFor(nsPropertyOwner aObject,
                                                  nsPropertyTable *aOtherTable)
{
  nsresult rv = NS_OK;
  for (PropertyList* prop = mPropertyList; prop; prop = prop->mNext) {
    if (prop->mTransfer) {
      auto entry = static_cast<PropertyListMapEntry*>
                              (prop->mObjectValueMap.Search(aObject));
      if (entry) {
        rv = aOtherTable->SetProperty(aObject, prop->mName,
                                      entry->value, prop->mDtorFunc,
                                      prop->mDtorData, prop->mTransfer);
        if (NS_FAILED(rv)) {
          DeleteAllPropertiesFor(aObject);
          aOtherTable->DeleteAllPropertiesFor(aObject);

          break;
        }

        prop->mObjectValueMap.RemoveEntry(entry);
      }
    }
    else {
      prop->DeletePropertyFor(aObject);
    }
  }

  return rv;
}

void
nsPropertyTable::Enumerate(nsPropertyOwner aObject,
                           NSPropertyFunc aCallback, void *aData)
{
  PropertyList* prop;
  for (prop = mPropertyList; prop; prop = prop->mNext) {
    auto entry = static_cast<PropertyListMapEntry*>
                            (prop->mObjectValueMap.Search(aObject));
    if (entry) {
      aCallback(const_cast<void*>(aObject.get()), prop->mName, entry->value,
                aData);
    }
  }
}

void
nsPropertyTable::EnumerateAll(NSPropertyFunc aCallBack, void* aData)
{
  for (PropertyList* prop = mPropertyList; prop; prop = prop->mNext) {
    for (auto iter = prop->mObjectValueMap.Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<PropertyListMapEntry*>(iter.Get());
      aCallBack(const_cast<void*>(entry->key), prop->mName, entry->value,
                aData);
    }
  }
}

void*
nsPropertyTable::GetPropertyInternal(nsPropertyOwner aObject,
                                     nsIAtom    *aPropertyName,
                                     bool        aRemove,
                                     nsresult   *aResult)
{
  NS_PRECONDITION(aPropertyName && aObject, "unexpected null param");
  nsresult rv = NS_PROPTABLE_PROP_NOT_THERE;
  void *propValue = nullptr;

  PropertyList* propertyList = GetPropertyListFor(aPropertyName);
  if (propertyList) {
    auto entry = static_cast<PropertyListMapEntry*>
                            (propertyList->mObjectValueMap.Search(aObject));
    if (entry) {
      propValue = entry->value;
      if (aRemove) {
        // don't call propertyList->mDtorFunc.  That's the caller's job now.
        propertyList->mObjectValueMap.RemoveEntry(entry);
      }
      rv = NS_OK;
    }
  }

  if (aResult)
    *aResult = rv;

  return propValue;
}

nsresult
nsPropertyTable::SetPropertyInternal(nsPropertyOwner     aObject,
                                     nsIAtom            *aPropertyName,
                                     void               *aPropertyValue,
                                     NSPropertyDtorFunc  aPropDtorFunc,
                                     void               *aPropDtorData,
                                     bool                aTransfer,
                                     void              **aOldValue)
{
  NS_PRECONDITION(aPropertyName && aObject, "unexpected null param");

  PropertyList* propertyList = GetPropertyListFor(aPropertyName);

  if (propertyList) {
    // Make sure the dtor function and data and the transfer flag match
    if (aPropDtorFunc != propertyList->mDtorFunc ||
        aPropDtorData != propertyList->mDtorData ||
        aTransfer != propertyList->mTransfer) {
      NS_WARNING("Destructor/data mismatch while setting property");
      return NS_ERROR_INVALID_ARG;
    }

  } else {
    propertyList = new PropertyList(aPropertyName, aPropDtorFunc,
                                    aPropDtorData, aTransfer);
    propertyList->mNext = mPropertyList;
    mPropertyList = propertyList;
  }

  // The current property value (if there is one) is replaced and the current
  // value is destroyed
  nsresult result = NS_OK;
  auto entry = static_cast<PropertyListMapEntry*>
    (propertyList->mObjectValueMap.Add(aObject, mozilla::fallible));
  if (!entry)
    return NS_ERROR_OUT_OF_MEMORY;
  // A nullptr entry->key is the sign that the entry has just been allocated
  // for us.  If it's non-nullptr then we have an existing entry.
  if (entry->key) {
    if (aOldValue)
      *aOldValue = entry->value;
    else if (propertyList->mDtorFunc)
      propertyList->mDtorFunc(const_cast<void*>(entry->key), aPropertyName,
                              entry->value, propertyList->mDtorData);
    result = NS_PROPTABLE_PROP_OVERWRITTEN;
  }
  else if (aOldValue) {
    *aOldValue = nullptr;
  }
  entry->key = aObject;
  entry->value = aPropertyValue;

  return result;
}

nsresult
nsPropertyTable::DeleteProperty(nsPropertyOwner aObject,
                                nsIAtom    *aPropertyName)
{
  NS_PRECONDITION(aPropertyName && aObject, "unexpected null param");

  PropertyList* propertyList = GetPropertyListFor(aPropertyName);
  if (propertyList) {
    if (propertyList->DeletePropertyFor(aObject))
      return NS_OK;
  }

  return NS_PROPTABLE_PROP_NOT_THERE;
}

nsPropertyTable::PropertyList*
nsPropertyTable::GetPropertyListFor(nsIAtom* aPropertyName) const
{
  PropertyList* result;

  for (result = mPropertyList; result; result = result->mNext) {
    if (result->Equals(aPropertyName)) {
      break;
    }
  }

  return result;
}

//----------------------------------------------------------------------

nsPropertyTable::PropertyList::PropertyList(nsIAtom            *aName,
                                            NSPropertyDtorFunc  aDtorFunc,
                                            void               *aDtorData,
                                            bool                aTransfer)
  : mName(aName),
    mObjectValueMap(PLDHashTable::StubOps(), sizeof(PropertyListMapEntry)),
    mDtorFunc(aDtorFunc),
    mDtorData(aDtorData),
    mTransfer(aTransfer),
    mNext(nullptr)
{
}

nsPropertyTable::PropertyList::~PropertyList()
{
}

void
nsPropertyTable::PropertyList::Destroy()
{
  // Enumerate any remaining object/value pairs and destroy the value object.
  if (mDtorFunc) {
    for (auto iter = mObjectValueMap.Iter(); !iter.Done(); iter.Next()) {
      auto entry = static_cast<PropertyListMapEntry*>(iter.Get());
      mDtorFunc(const_cast<void*>(entry->key), mName, entry->value, mDtorData);
    }
  }
}

bool
nsPropertyTable::PropertyList::DeletePropertyFor(nsPropertyOwner aObject)
{
  auto entry =
    static_cast<PropertyListMapEntry*>(mObjectValueMap.Search(aObject));
  if (!entry)
    return false;

  void* value = entry->value;
  mObjectValueMap.RemoveEntry(entry);

  if (mDtorFunc)
    mDtorFunc(const_cast<void*>(aObject.get()), mName, value, mDtorData);

  return true;
}

size_t
nsPropertyTable::PropertyList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  n += mObjectValueMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

size_t
nsPropertyTable::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  for (PropertyList *prop = mPropertyList; prop; prop = prop->mNext) {
    n += prop->SizeOfIncludingThis(aMallocSizeOf);
  }

  return n;
}

size_t
nsPropertyTable::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

/* static */
void
nsPropertyTable::SupportsDtorFunc(void *aObject, nsIAtom *aPropertyName,
                                  void *aPropertyValue, void *aData)
{
  nsISupports *propertyValue = static_cast<nsISupports*>(aPropertyValue);
  NS_IF_RELEASE(propertyValue);
}
