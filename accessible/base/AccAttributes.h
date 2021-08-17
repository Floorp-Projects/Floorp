/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccAttributes_h_
#define AccAttributes_h_

#include "mozilla/ServoStyleConsts.h"
#include "mozilla/Variant.h"
#include "nsTHashMap.h"
#include "nsAtom.h"
#include "nsStringFwd.h"

class nsVariant;

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {

namespace dom {
class Element;
}

namespace a11y {

struct FontSize {
  int32_t mValue;
};

struct Color {
  nscolor mValue;
};

class AccAttributes {
  friend struct IPC::ParamTraits<AccAttributes*>;

  using AttrValueType =
      Variant<bool, float, int32_t, RefPtr<nsAtom>, CSSCoord, FontSize, Color>;
  using AtomVariantMap = nsTHashMap<nsRefPtrHashKey<nsAtom>, AttrValueType>;

 protected:
  ~AccAttributes() = default;

 public:
  AccAttributes() = default;
  AccAttributes(AccAttributes&&) = delete;
  AccAttributes& operator=(AccAttributes&&) = delete;
  AccAttributes(const AccAttributes&) = delete;
  AccAttributes& operator=(const AccAttributes&) = delete;

  NS_INLINE_DECL_REFCOUNTING(mozilla::a11y::AccAttributes)

  template <typename T>
  void SetAttribute(nsAtom* aAttrName, const T& aAttrValue) {
    if constexpr (std::is_base_of_v<nsAtom, std::remove_pointer_t<T>>) {
      mData.InsertOrUpdate(aAttrName, AsVariant(RefPtr<nsAtom>(aAttrValue)));
    } else if constexpr (std::is_base_of_v<nsAString, T> ||
                         std::is_base_of_v<nsLiteralString, T>) {
      RefPtr<nsAtom> atomValue = NS_Atomize(aAttrValue);
      mData.InsertOrUpdate(aAttrName, AsVariant(atomValue));
    } else {
      mData.InsertOrUpdate(aAttrName, AsVariant(aAttrValue));
    }
  }

  template <typename T>
  Maybe<T> GetAttribute(nsAtom* aAttrName) {
    if (auto value = mData.Lookup(aAttrName)) {
      if constexpr (std::is_base_of_v<nsAtom, std::remove_pointer_t<T>>) {
        if (value->is<RefPtr<nsAtom>>()) {
          return Some(value->as<RefPtr<nsAtom>>().get());
        }
      } else {
        if (value->is<T>()) {
          return Some(value->as<T>());
        }
      }
    }
    return Nothing();
  }

  // Get stringified value
  bool GetAttribute(nsAtom* aAttrName, nsAString& aAttrValue);

  bool HasAttribute(nsAtom* aAttrName) { return mData.Contains(aAttrName); }

  uint32_t Count() const { return mData.Count(); }

  void Update(AccAttributes* aOther);

  // An entry class for our iterator.
  class Entry {
   public:
    Entry(nsAtom* aAttrName, const AttrValueType* aAttrValue)
        : mName(aAttrName), mValue(aAttrValue) {}

    nsAtom* Name() { return mName; }

    template <typename T>
    Maybe<T> Value() {
      if constexpr (std::is_base_of_v<nsAtom, std::remove_pointer_t<T>>) {
        if (mValue->is<RefPtr<nsAtom>>()) {
          return Some(mValue->as<RefPtr<nsAtom>>().get());
        }
      } else {
        if (mValue->is<T>()) {
          return Some(mValue->as<T>());
        }
      }
      return Nothing();
    }

    void NameAsString(nsString& aName) {
      mName->ToString(aName);
      if (aName.Find("aria-", false, 0, 1) == 0) {
        // Found 'aria-'
        aName.ReplaceLiteral(0, 5, u"");
      }
    }

    void ValueAsString(nsAString& aValueString) {
      StringFromValueAndName(mName, *mValue, aValueString);
    }

   private:
    nsAtom* mName;
    const AttrValueType* mValue;

    friend class AccAttributes;
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
  static void StringFromValueAndName(nsAtom* aAttrName,
                                     const AttrValueType& aValue,
                                     nsAString& aValueString);

  AtomVariantMap mData;
};

}  // namespace a11y
}  // namespace mozilla

#endif
