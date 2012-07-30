/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpandedNameMap.h"
#include "txCore.h"

class txMapItemComparator
{
  public:
    bool Equals(const txExpandedNameMap_base::MapItem& aItem,
                  const txExpandedName& aKey) const {
      return aItem.mNamespaceID == aKey.mNamespaceID &&
             aItem.mLocalName == aKey.mLocalName;
    }
};

/**
 * Adds an item, if an item with this key already exists an error is
 * returned
 * @param  aKey   key for item to add
 * @param  aValue value of item to add
 * @return errorcode
 */
nsresult txExpandedNameMap_base::addItem(const txExpandedName& aKey,
                                         void* aValue)
{
    PRUint32 pos = mItems.IndexOf(aKey, 0, txMapItemComparator());
    if (pos != mItems.NoIndex) {
        return NS_ERROR_XSLT_ALREADY_SET;
    }

    MapItem* item = mItems.AppendElement();
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    item->mNamespaceID = aKey.mNamespaceID;
    item->mLocalName = aKey.mLocalName;
    item->mValue = aValue;

    return NS_OK;
}

/**
 * Sets an item, if an item with this key already exists it is overwritten
 * with the new value
 * @param  aKey   key for item to set
 * @param  aValue value of item to set
 * @return errorcode
 */
nsresult txExpandedNameMap_base::setItem(const txExpandedName& aKey,
                                         void* aValue,
                                         void** aOldValue)
{
    *aOldValue = nullptr;
    PRUint32 pos = mItems.IndexOf(aKey, 0, txMapItemComparator());
    if (pos != mItems.NoIndex) {
        *aOldValue = mItems[pos].mValue;
        mItems[pos].mValue = aValue;
        
        return NS_OK;
    }

    MapItem* item = mItems.AppendElement();
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    item->mNamespaceID = aKey.mNamespaceID;
    item->mLocalName = aKey.mLocalName;
    item->mValue = aValue;

    return NS_OK;
}

/**
 * Gets an item
 * @param  aKey  key for item to get
 * @return item with specified key, or null if no such item exists
 */
void* txExpandedNameMap_base::getItem(const txExpandedName& aKey) const
{
    PRUint32 pos = mItems.IndexOf(aKey, 0, txMapItemComparator());
    if (pos != mItems.NoIndex) {
        return mItems[pos].mValue;
    }

    return nullptr;
}

/**
 * Removes an item, deleting it if the map owns the values
 * @param  aKey  key for item to remove
 * @return item with specified key, or null if it has been deleted
 *         or no such item exists
 */
void* txExpandedNameMap_base::removeItem(const txExpandedName& aKey)
{
    void* value = nullptr;
    PRUint32 pos = mItems.IndexOf(aKey, 0, txMapItemComparator());
    if (pos != mItems.NoIndex) {
        value = mItems[pos].mValue;
        mItems.RemoveElementAt(pos);
    }

    return value;
}
