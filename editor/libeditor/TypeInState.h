/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TypeInState_h
#define mozilla_TypeInState_h

#include "mozilla/EditorDOMPoint.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nscore.h"

// Workaround for windows headers
#ifdef SetProp
#  undef SetProp
#endif

class nsAtom;
class nsINode;

namespace mozilla {
namespace dom {
class Selection;
}  // namespace dom

struct PropItem {
  nsAtom* tag;
  nsAtom* attr;
  nsString value;

  PropItem() : tag(nullptr), attr(nullptr) { MOZ_COUNT_CTOR(PropItem); }
  PropItem(nsAtom* aTag, nsAtom* aAttr, const nsAString& aValue)
      : tag(aTag),
        attr(aAttr != nsGkAtoms::_empty ? aAttr : nullptr),
        value(aValue) {
    MOZ_COUNT_CTOR(PropItem);
  }
  MOZ_COUNTED_DTOR(PropItem)
};

class StyleCache final {
 public:
  StyleCache() = delete;
  StyleCache(nsStaticAtom* aTag, nsStaticAtom* aAttribute,
             const nsAString& aValue)
      : mTag(aTag), mAttribute(aAttribute), mValue(aValue) {
    MOZ_ASSERT(mTag);
  }

  nsStaticAtom* Tag() const { return mTag; }
  nsStaticAtom* GetAttribute() const { return mAttribute; }
  const nsString& Value() const { return mValue; }

 private:
  nsStaticAtom* mTag;
  nsStaticAtom* mAttribute;
  nsString mValue;
};

class MOZ_STACK_CLASS AutoStyleCacheArray final
    : public AutoTArray<StyleCache, 19> {
 public:
  index_type IndexOf(const nsStaticAtom* aTag,
                     const nsStaticAtom* aAttribute) const {
    for (index_type index = 0; index < Length(); ++index) {
      const StyleCache& styleCache = ElementAt(index);
      if (styleCache.Tag() == aTag && styleCache.GetAttribute() == aAttribute) {
        return index;
      }
    }
    return NoIndex;
  }
};

class TypeInState final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TypeInState)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(TypeInState)

  TypeInState();
  void Reset();

  nsresult UpdateSelState(dom::Selection* aSelection);

  void OnSelectionChange(dom::Selection& aSelection, int16_t aReason);

  void SetProp(nsAtom* aProp, nsAtom* aAttr, const nsAString& aValue);

  void ClearAllProps();
  void ClearProp(nsAtom* aProp, nsAtom* aAttr);

  /**
   * TakeClearProperty() hands back next property item on the clear list.
   * Caller assumes ownership of PropItem and must delete it.
   */
  UniquePtr<PropItem> TakeClearProperty();

  /**
   * TakeSetProperty() hands back next property item on the set list.
   * Caller assumes ownership of PropItem and must delete it.
   */
  UniquePtr<PropItem> TakeSetProperty();

  /**
   * TakeRelativeFontSize() hands back relative font value, which is then
   * cleared out.
   */
  int32_t TakeRelativeFontSize();

  void GetTypingState(bool& isSet, bool& theSetting, nsAtom* aProp,
                      nsAtom* aAttr = nullptr, nsString* outValue = nullptr);

  static bool FindPropInList(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue,
                             nsTArray<PropItem*>& aList, int32_t& outIndex);

 protected:
  virtual ~TypeInState();

  void RemovePropFromSetList(nsAtom* aProp, nsAtom* aAttr);
  void RemovePropFromClearedList(nsAtom* aProp, nsAtom* aAttr);
  bool IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue);
  bool IsPropSet(nsAtom* aProp, nsAtom* aAttr, nsAString* outValue,
                 int32_t& outIndex);
  bool IsPropCleared(nsAtom* aProp, nsAtom* aAttr);
  bool IsPropCleared(nsAtom* aProp, nsAtom* aAttr, int32_t& outIndex);

  nsTArray<PropItem*> mSetArray;
  nsTArray<PropItem*> mClearedArray;
  EditorDOMPoint mLastSelectionPoint;
  int32_t mRelativeFontSize;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_TypeInState_h
