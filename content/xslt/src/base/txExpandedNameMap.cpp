/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    *aOldValue = nsnull;
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

    return nsnull;
}

/**
 * Removes an item, deleting it if the map owns the values
 * @param  aKey  key for item to remove
 * @return item with specified key, or null if it has been deleted
 *         or no such item exists
 */
void* txExpandedNameMap_base::removeItem(const txExpandedName& aKey)
{
    void* value = nsnull;
    PRUint32 pos = mItems.IndexOf(aKey, 0, txMapItemComparator());
    if (pos != mItems.NoIndex) {
        value = mItems[pos].mValue;
        mItems.RemoveElementAt(pos);
    }

    return value;
}
