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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * Jonas Sicking. All Rights Reserved.
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

#ifndef TRANSFRMX_EXPANDEDNAMEMAP_H
#define TRANSFRMX_EXPANDEDNAMEMAP_H

#include "txError.h"
#include "XMLUtils.h"

class TxObject;

class txExpandedNameMap {
public:
    txExpandedNameMap(MBool aOwnsValues);
    
    ~txExpandedNameMap();
    
    /*
     * Adds an item, if an item with this key already exists an error is
     * returned
     * @param  aKey   key for item to add
     * @param  aValue value of item to add
     * @return errorcode
     */
    nsresult add(const txExpandedName& aKey, TxObject* aValue);

    /*
     * Sets an item, if an item with this key already exists it is overwritten
     * with the new value
     * @param  aKey   key for item to set
     * @param  aValue value of item to set
     * @return errorcode
     */
    nsresult set(const txExpandedName& aKey, TxObject* aValue);

    /*
     * Gets an item
     * @param  aKey  key for item to get
     * @return item with specified key, or null if no such item exists
     */
    TxObject* get(const txExpandedName& aKey);

    class iterator {
    public:
        iterator(txExpandedNameMap& aMap) : mMap(aMap),
                                            mCurrentPos(-1)
        {
        }

        MBool next()
        {
            return ++mCurrentPos < mMap.mItemCount;
        }

        const txExpandedName key()
        {
            NS_ASSERTION(mCurrentPos >= 0 && mCurrentPos < mMap.mItemCount,
                         "invalid position in txExpandedNameMap::iterator");
            return txExpandedName(mMap.mItems[mCurrentPos].mNamespaceID,
                                  mMap.mItems[mCurrentPos].mLocalName);
        }

        TxObject* value()
        {
            NS_ASSERTION(mCurrentPos >= 0 && mCurrentPos < mMap.mItemCount,
                         "invalid position in txExpandedNameMap::iterator");
            return mMap.mItems[mCurrentPos].mValue;
        }

    private:
        txExpandedNameMap& mMap;
        int mCurrentPos;
    };
    
    friend class iterator;

private:
    struct MapItem {
        PRInt32 mNamespaceID;
        txAtom* mLocalName;
        TxObject* mValue;
    };
    
    MapItem* mItems;
    int mItemCount;
    MBool mOwnsValues;
};

#endif //TRANSFRMX_EXPANDEDNAMEMAP_H
