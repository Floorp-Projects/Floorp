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

#ifndef TRANSFRMX_EXPANDEDNAMEMAP_H
#define TRANSFRMX_EXPANDEDNAMEMAP_H

#include "txError.h"
#include "txXMLUtils.h"
#include "nsTArray.h"

class txExpandedNameMap_base {
protected:
    /**
     * Adds an item, if an item with this key already exists an error is
     * returned
     * @param  aKey   key for item to add
     * @param  aValue value of item to add
     * @return errorcode
     */
    nsresult addItem(const txExpandedName& aKey, void* aValue);

    /**
     * Sets an item, if an item with this key already exists it is overwritten
     * with the new value
     * @param  aKey   key for item to set
     * @param  aValue value of item to set
     * @return errorcode
     */
    nsresult setItem(const txExpandedName& aKey, void* aValue,
                     void** aOldValue);

    /**
     * Gets an item
     * @param  aKey  key for item to get
     * @return item with specified key, or null if no such item exists
     */
    void* getItem(const txExpandedName& aKey) const;

    /**
     * Removes an item, deleting it if the map owns the values
     * @param  aKey  key for item to remove
     * @return item with specified key, or null if it has been deleted
     *         or no such item exists
     */
    void* removeItem(const txExpandedName& aKey);

    /**
     * Clears the items
     */
    void clearItems()
    {
        mItems.Clear();
    }

    class iterator_base {
    public:
        iterator_base(txExpandedNameMap_base& aMap)
            : mMap(aMap),
              mCurrentPos(PRUint32(-1))
        {
        }

        bool next()
        {
            return ++mCurrentPos < mMap.mItems.Length();
        }

        const txExpandedName key()
        {
            NS_ASSERTION(mCurrentPos >= 0 &&
                         mCurrentPos < mMap.mItems.Length(),
                         "invalid position in txExpandedNameMap::iterator");
            return txExpandedName(mMap.mItems[mCurrentPos].mNamespaceID,
                                  mMap.mItems[mCurrentPos].mLocalName);
        }

    protected:
        void* itemValue()
        {
            NS_ASSERTION(mCurrentPos >= 0 &&
                         mCurrentPos < mMap.mItems.Length(),
                         "invalid position in txExpandedNameMap::iterator");
            return mMap.mItems[mCurrentPos].mValue;
        }

    private:
        txExpandedNameMap_base& mMap;
        PRUint32 mCurrentPos;
    };
    
    friend class iterator_base;

    friend class txMapItemComparator;
    struct MapItem {
        PRInt32 mNamespaceID;
        nsCOMPtr<nsIAtom> mLocalName;
        void* mValue;
    };
    
    nsTArray<MapItem> mItems;
};

template<class E>
class txExpandedNameMap : public txExpandedNameMap_base
{
public:
    nsresult add(const txExpandedName& aKey, E* aValue)
    {
        return addItem(aKey, (void*)aValue);
    }

    nsresult set(const txExpandedName& aKey, E* aValue)
    {
        void* oldValue;
        return setItem(aKey, (void*)aValue, &oldValue);
    }

    E* get(const txExpandedName& aKey) const
    {
        return (E*)getItem(aKey);
    }

    E* remove(const txExpandedName& aKey)
    {
        return (E*)removeItem(aKey);
    }

    void clear()
    {
        clearItems();
    }

    class iterator : public iterator_base
    {
    public:
        iterator(txExpandedNameMap& aMap)
            : iterator_base(aMap)
        {
        }

        E* value()
        {
            return (E*)itemValue();
        }
    };
};

template<class E>
class txOwningExpandedNameMap : public txExpandedNameMap_base
{
public:
    ~txOwningExpandedNameMap()
    {
        clear();
    }

    nsresult add(const txExpandedName& aKey, E* aValue)
    {
        return addItem(aKey, (void*)aValue);
    }

    nsresult set(const txExpandedName& aKey, E* aValue)
    {
        nsAutoPtr<E> oldValue;
        return setItem(aKey, (void*)aValue, getter_Transfers(oldValue));
    }

    E* get(const txExpandedName& aKey) const
    {
        return (E*)getItem(aKey);
    }

    void remove(const txExpandedName& aKey)
    {
        delete (E*)removeItem(aKey);
    }

    void clear()
    {
        PRUint32 i, len = mItems.Length();
        for (i = 0; i < len; ++i) {
            delete (E*)mItems[i].mValue;
        }
        clearItems();
    }

    class iterator : public iterator_base
    {
    public:
        iterator(txOwningExpandedNameMap& aMap)
            : iterator_base(aMap)
        {
        }

        E* value()
        {
            return (E*)itemValue();
        }
    };
};

#endif //TRANSFRMX_EXPANDEDNAMEMAP_H
