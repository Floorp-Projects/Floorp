/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccAttributes_h_
#define AccAttributes_h_

#include "nsTHashMap.h"
#include "nsAtom.h"

namespace mozilla {

namespace a11y {

class AccAttributes {
  using AtomVariantMap = nsTHashMap<nsRefPtrHashKey<nsAtom>, nsString>;

 protected:
  ~AccAttributes() = default;

 public:
  AccAttributes() = default;
  AccAttributes(AccAttributes&&) = delete;
  AccAttributes& operator=(AccAttributes&&) = delete;
  AccAttributes(const AccAttributes&) = delete;
  AccAttributes& operator=(const AccAttributes&) = delete;

  NS_INLINE_DECL_REFCOUNTING(mozilla::a11y::AccAttributes)

  // Set values using atom keys
  void SetAttribute(nsAtom* aAttrName, const nsAString& aAttrValue);
  void SetAttribute(nsAtom* aAttrName, const nsAtom* aAttrValue);

  // Set values using string keys
  void SetAttribute(const nsAString& aAttrName, const nsAString& aAttrValue);

  // Get stringified value
  bool GetAttribute(nsAtom* aAttrName, nsAString& aAttrValue);

  bool HasAttribute(nsAtom* aAttrName) { return mData.Contains(aAttrName); }

  uint32_t Count() const { return mData.Count(); }

  // An entry class for our iterator.
  class Entry {
   public:
    Entry(nsAtom* aAttrName, const nsString* aAttrValue)
        : mName(aAttrName), mValue(aAttrValue) {}

    nsAtom* Name() { return mName; }

    void NameAsString(nsAString& aName) { mName->ToString(aName); }

    void ValueAsString(nsAString& aValueString) {
      aValueString.Assign(*mValue);
    }

   private:
    nsAtom* mName;
    const nsString* mValue;
  };

  class Iterator {
   public:
    explicit Iterator(AtomVariantMap::iterator aIter) : mHashIterator(aIter) {}

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator(Iterator&&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = delete;

    bool operator!=(const Iterator& aOther) const {
      return mHashIterator != aOther.mHashIterator;
    }

    Iterator& operator++() {
      mHashIterator++;
      return *this;
    }

    Entry operator*() {
      auto& entry = *mHashIterator;
      return Entry(entry.GetKey(), &entry.GetData());
    }

   private:
    AtomVariantMap::iterator mHashIterator;
  };

  friend class Iterator;

  Iterator begin() { return Iterator(mData.begin()); }
  Iterator end() { return Iterator(mData.end()); }

 private:
  AtomVariantMap mData;
};

}  // namespace a11y
}  // namespace mozilla

#endif
