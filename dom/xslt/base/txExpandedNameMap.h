/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_EXPANDEDNAMEMAP_H
#define TRANSFRMX_EXPANDEDNAMEMAP_H

#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsError.h"
#include "txExpandedName.h"
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
  nsresult setItem(const txExpandedName& aKey, void* aValue, void** aOldValue);

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
  void clearItems() { mItems.Clear(); }

  class iterator_base {
   public:
    explicit iterator_base(txExpandedNameMap_base& aMap)
        : mMap(aMap), mCurrentPos(uint32_t(-1)) {}

    bool next() { return ++mCurrentPos < mMap.mItems.Length(); }

    const txExpandedName key() {
      NS_ASSERTION(mCurrentPos < mMap.mItems.Length(),
                   "invalid position in txExpandedNameMap::iterator");
      return txExpandedName(mMap.mItems[mCurrentPos].mNamespaceID,
                            mMap.mItems[mCurrentPos].mLocalName);
    }

   protected:
    void* itemValue() {
      NS_ASSERTION(mCurrentPos < mMap.mItems.Length(),
                   "invalid position in txExpandedNameMap::iterator");
      return mMap.mItems[mCurrentPos].mValue;
    }

   private:
    txExpandedNameMap_base& mMap;
    uint32_t mCurrentPos;
  };

  friend class iterator_base;

  friend class txMapItemComparator;
  struct MapItem {
    int32_t mNamespaceID;
    RefPtr<nsAtom> mLocalName;
    void* mValue;
  };

  nsTArray<MapItem> mItems;
};

template <class E>
class txExpandedNameMap : public txExpandedNameMap_base {
 public:
  nsresult add(const txExpandedName& aKey, E* aValue) {
    return addItem(aKey, (void*)aValue);
  }

  nsresult set(const txExpandedName& aKey, E* aValue) {
    void* oldValue;
    return setItem(aKey, (void*)aValue, &oldValue);
  }

  E* get(const txExpandedName& aKey) const { return (E*)getItem(aKey); }

  E* remove(const txExpandedName& aKey) { return (E*)removeItem(aKey); }

  void clear() { clearItems(); }

  class iterator : public iterator_base {
   public:
    explicit iterator(txExpandedNameMap& aMap) : iterator_base(aMap) {}

    E* value() { return (E*)itemValue(); }
  };
};

template <class E>
class txOwningExpandedNameMap : public txExpandedNameMap_base {
 public:
  ~txOwningExpandedNameMap() { clear(); }

  nsresult add(const txExpandedName& aKey, E* aValue) {
    return addItem(aKey, (void*)aValue);
  }

  nsresult set(const txExpandedName& aKey, E* aValue) {
    mozilla::UniquePtr<E> oldValue;
    return setItem(aKey, (void*)aValue, getter_Transfers(oldValue));
  }

  E* get(const txExpandedName& aKey) const { return (E*)getItem(aKey); }

  void remove(const txExpandedName& aKey) { delete (E*)removeItem(aKey); }

  void clear() {
    uint32_t i, len = mItems.Length();
    for (i = 0; i < len; ++i) {
      delete (E*)mItems[i].mValue;
    }
    clearItems();
  }

  class iterator : public iterator_base {
   public:
    explicit iterator(txOwningExpandedNameMap& aMap) : iterator_base(aMap) {}

    E* value() { return (E*)itemValue(); }
  };
};

#endif  // TRANSFRMX_EXPANDEDNAMEMAP_H
