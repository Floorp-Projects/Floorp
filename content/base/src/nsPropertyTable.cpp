/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
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

#include "pldhash.h"
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
      PropertyListMapEntry *entry = static_cast<PropertyListMapEntry*>
                                               (PL_DHashTableOperate(&prop->mObjectValueMap, aObject,
                               PL_DHASH_LOOKUP));
      if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
        rv = aOtherTable->SetProperty(aObject, prop->mName,
                                      entry->value, prop->mDtorFunc,
                                      prop->mDtorData, prop->mTransfer);
        if (NS_FAILED(rv)) {
          DeleteAllPropertiesFor(aObject);
          aOtherTable->DeleteAllPropertiesFor(aObject);

          break;
        }

        PL_DHashTableRawRemove(&prop->mObjectValueMap, entry);
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
    PropertyListMapEntry *entry = static_cast<PropertyListMapEntry*>
      (PL_DHashTableOperate(&prop->mObjectValueMap, aObject, PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      aCallback(const_cast<void*>(aObject.get()), prop->mName, entry->value,
                 aData);
    }
  }
}

struct PropertyEnumeratorData
{
  nsIAtom* mName;
  NSPropertyFunc mCallBack;
  void* mData;
};

static PLDHashOperator
PropertyEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                   uint32_t aNumber, void* aArg)
{
  PropertyListMapEntry* entry = static_cast<PropertyListMapEntry*>(aHdr);
  PropertyEnumeratorData* data = static_cast<PropertyEnumeratorData*>(aArg);
  data->mCallBack(const_cast<void*>(entry->key), data->mName, entry->value,
                  data->mData);
  return PL_DHASH_NEXT;
}

void
nsPropertyTable::EnumerateAll(NSPropertyFunc aCallBack, void* aData)
{
  for (PropertyList* prop = mPropertyList; prop; prop = prop->mNext) {
    PropertyEnumeratorData data = { prop->mName, aCallBack, aData };
    PL_DHashTableEnumerate(&prop->mObjectValueMap, PropertyEnumerator, &data);
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
    PropertyListMapEntry *entry = static_cast<PropertyListMapEntry*>
                                             (PL_DHashTableOperate(&propertyList->mObjectValueMap, aObject,
                             PL_DHASH_LOOKUP));
    if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
      propValue = entry->value;
      if (aRemove) {
        // don't call propertyList->mDtorFunc.  That's the caller's job now.
        PL_DHashTableRawRemove(&propertyList->mObjectValueMap, entry);
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
    if (!propertyList || !propertyList->mObjectValueMap.ops) {
      delete propertyList;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    propertyList->mNext = mPropertyList;
    mPropertyList = propertyList;
  }

  // The current property value (if there is one) is replaced and the current
  // value is destroyed
  nsresult result = NS_OK;
  PropertyListMapEntry *entry = static_cast<PropertyListMapEntry*>
                                           (PL_DHashTableOperate(&propertyList->mObjectValueMap, aObject, PL_DHASH_ADD));
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
    mDtorFunc(aDtorFunc),
    mDtorData(aDtorData),
    mTransfer(aTransfer),
    mNext(nullptr)
{
  PL_DHashTableInit(&mObjectValueMap, PL_DHashGetStubOps(), this,
                    sizeof(PropertyListMapEntry), 16);
}

nsPropertyTable::PropertyList::~PropertyList()
{
  PL_DHashTableFinish(&mObjectValueMap);
}


static PLDHashOperator
DestroyPropertyEnumerator(PLDHashTable *table, PLDHashEntryHdr *hdr,
                          uint32_t number, void *arg)
{
  nsPropertyTable::PropertyList *propList =
      static_cast<nsPropertyTable::PropertyList*>(table->data);
  PropertyListMapEntry* entry = static_cast<PropertyListMapEntry*>(hdr);

  propList->mDtorFunc(const_cast<void*>(entry->key), propList->mName,
                      entry->value, propList->mDtorData);
  return PL_DHASH_NEXT;
}

void
nsPropertyTable::PropertyList::Destroy()
{
  // Enumerate any remaining object/value pairs and destroy the value object
  if (mDtorFunc)
    PL_DHashTableEnumerate(&mObjectValueMap, DestroyPropertyEnumerator,
                           nullptr);
}

bool
nsPropertyTable::PropertyList::DeletePropertyFor(nsPropertyOwner aObject)
{
  PropertyListMapEntry *entry = static_cast<PropertyListMapEntry*>
                                           (PL_DHashTableOperate(&mObjectValueMap, aObject, PL_DHASH_LOOKUP));
  if (!PL_DHASH_ENTRY_IS_BUSY(entry))
    return false;

  void* value = entry->value;
  PL_DHashTableRawRemove(&mObjectValueMap, entry);

  if (mDtorFunc)
    mDtorFunc(const_cast<void*>(aObject.get()), mName, value, mDtorData);

  return true;
}

size_t
nsPropertyTable::PropertyList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  size_t n = aMallocSizeOf(this);
  n += PL_DHashTableSizeOfExcludingThis(&mObjectValueMap, nullptr, aMallocSizeOf);
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
