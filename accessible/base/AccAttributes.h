/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccAttributes_h_
#define AccAttributes_h_

#include "mozilla/ServoStyleConsts.h"
#include "mozilla/a11y/AccGroupInfo.h"
#include "mozilla/Variant.h"
#include "nsTHashMap.h"
#include "nsAtom.h"
#include "nsStringFwd.h"
#include "mozilla/gfx/Matrix.h"

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

  bool operator==(const FontSize& aOther) const {
    return mValue == aOther.mValue;
  }

  bool operator!=(const FontSize& aOther) const {
    return mValue != aOther.mValue;
  }
};

struct Color {
  nscolor mValue;

  bool operator==(const Color& aOther) const { return mValue == aOther.mValue; }

  bool operator!=(const Color& aOther) const { return mValue != aOther.mValue; }
};

// A special type. If an entry has a value of this type, it instructs the
// target instance of an Update to remove the entry with the same key value.
struct DeleteEntry {
  DeleteEntry() : mValue(true) {}
  bool mValue;

  bool operator==(const DeleteEntry& aOther) const { return true; }

  bool operator!=(const DeleteEntry& aOther) const { return false; }
};

class AccAttributes {
  // Warning! An AccAttributes can contain another AccAttributes. This is
  // intended for object and text attributes. However, the nested
  // AccAttributes should never itself contain another AccAttributes, nor
  // should it create a cycle. We don't do cycle collection here for
  // performance reasons, so violating this rule will cause leaks!
  using AttrValueType =
      Variant<bool, float, double, int32_t, RefPtr<nsAtom>, nsTArray<int32_t>,
              CSSCoord, FontSize, Color, DeleteEntry, UniquePtr<nsString>,
              RefPtr<AccAttributes>, uint64_t, UniquePtr<AccGroupInfo>,
              UniquePtr<gfx::Matrix4x4>, nsTArray<uint64_t>>;
  static_assert(sizeof(AttrValueType) <= 16);
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

  template <typename T, typename std::enable_if<
                            !std::is_convertible_v<T, nsString> &&
                                !std::is_convertible_v<T, AccGroupInfo*> &&
                                !std::is_convertible_v<T, gfx::Matrix4x4> &&
                                !std::is_convertible_v<T, nsAtom*>,
                            bool>::type = true>
  void SetAttribute(nsAtom* aAttrName, T&& aAttrValue) {
    mData.InsertOrUpdate(aAttrName, AsVariant(std::forward<T>(aAttrValue)));
  }

  void SetAttribute(nsAtom* aAttrName, nsString&& aAttrValue) {
    UniquePtr<nsString> value = MakeUnique<nsString>();
    *value = std::forward<nsString>(aAttrValue);
    mData.InsertOrUpdate(aAttrName, AsVariant(std::move(value)));
  }

  void SetAttribute(nsAtom* aAttrName, AccGroupInfo* aAttrValue) {
    UniquePtr<AccGroupInfo> value(aAttrValue);
    mData.InsertOrUpdate(aAttrName, AsVariant(std::move(value)));
  }

  void SetAttribute(nsAtom* aAttrName, gfx::Matrix4x4&& aAttrValue) {
    UniquePtr<gfx::Matrix4x4> value = MakeUnique<gfx::Matrix4x4>();
    *value = std::forward<gfx::Matrix4x4>(aAttrValue);
    mData.InsertOrUpdate(aAttrName, AsVariant(std::move(value)));
  }

  void SetAttributeStringCopy(nsAtom* aAttrName, nsString aAttrValue) {
    SetAttribute(aAttrName, std::move(aAttrValue));
  }

  void SetAttribute(nsAtom* aAttrName, nsAtom* aAttrValue) {
    mData.InsertOrUpdate(aAttrName, AsVariant(RefPtr<nsAtom>(aAttrValue)));
  }

  template <typename T>
  Maybe<const T&> GetAttribute(nsAtom* aAttrName) {
    if (auto value = mData.Lookup(aAttrName)) {
      if constexpr (std::is_same_v<nsString, T>) {
        if (value->is<UniquePtr<nsString>>()) {
          const T& val = *(value->as<UniquePtr<nsString>>().get());
          return SomeRef(val);
        }
      } else if constexpr (std::is_same_v<gfx::Matrix4x4, T>) {
        if (value->is<UniquePtr<gfx::Matrix4x4>>()) {
          const T& val = *(value->as<UniquePtr<gfx::Matrix4x4>>());
          return SomeRef(val);
        }
      } else {
        if (value->is<T>()) {
          const T& val = value->as<T>();
          return SomeRef(val);
        }
      }
    }
    return Nothing();
  }

  template <typename T>
  RefPtr<const T> GetAttributeRefPtr(nsAtom* aAttrName) {
    if (auto value = mData.Lookup(aAttrName)) {
      if (value->is<RefPtr<T>>()) {
        RefPtr<const T> ref = value->as<RefPtr<T>>();
        return ref;
      }
    }
    return nullptr;
  }

  // Get stringified value
  bool GetAttribute(nsAtom* aAttrName, nsAString& aAttrValue);

  bool HasAttribute(nsAtom* aAttrName) { return mData.Contains(aAttrName); }

  bool Remove(nsAtom* aAttrName) { return mData.Remove(aAttrName); }

  uint32_t Count() const { return mData.Count(); }

  // Update one instance with the entries in another. The supplied AccAttributes
  // will be emptied.
  void Update(AccAttributes* aOther);

  /**
   * Return true if all the attributes in this instance are equal to all the
   * attributes in another instance.
   */
  bool Equal(const AccAttributes* aOther) const;

  /**
   * Copy attributes from this instance to another instance.
   * This should only be used in very specific cases; e.g. merging two sets of
   * cached attributes without modifying the cache. It can only copy simple
   * value types; e.g. it can't copy array values. Attempting to copy an
   * AccAttributes with uncopyable values will cause an assertion.
   */
  void CopyTo(AccAttributes* aDest) const;

  // An entry class for our iterator.
  class Entry {
   public:
    Entry(nsAtom* aAttrName, const AttrValueType* aAttrValue)
        : mName(aAttrName), mValue(aAttrValue) {}

    nsAtom* Name() { return mName; }

    template <typename T>
    Maybe<const T&> Value() {
      if constexpr (std::is_same_v<nsString, T>) {
        if (mValue->is<UniquePtr<nsString>>()) {
          const T& val = *(mValue->as<UniquePtr<nsString>>().get());
          return SomeRef(val);
        }
      } else if constexpr (std::is_same_v<gfx::Matrix4x4, T>) {
        if (mValue->is<UniquePtr<gfx::Matrix4x4>>()) {
          const T& val = *(mValue->as<UniquePtr<gfx::Matrix4x4>>());
          return SomeRef(val);
        }
      } else {
        if (mValue->is<T>()) {
          const T& val = mValue->as<T>();
          return SomeRef(val);
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

  friend struct IPC::ParamTraits<AccAttributes*>;
};

}  // namespace a11y
}  // namespace mozilla

#endif
