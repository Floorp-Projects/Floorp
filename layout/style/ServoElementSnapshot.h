/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoElementSnapshot_h
#define mozilla_ServoElementSnapshot_h

#include "mozilla/EventStates.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/dom/BorrowedAttrInfo.h"
#include "mozilla/dom/Element.h"
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsChangeHint.h"
#include "nsGkAtoms.h"
#include "nsAtom.h"

namespace mozilla {

/**
 * A structure representing a single attribute name and value.
 *
 * This is pretty similar to the private nsAttrAndChildArray::InternalAttr.
 */
struct ServoAttrSnapshot
{
  nsAttrName mName;
  nsAttrValue mValue;

  ServoAttrSnapshot(const nsAttrName& aName, const nsAttrValue& aValue)
    : mName(aName)
    , mValue(aValue)
  {
  }
};

/**
 * A bitflags enum class used to determine what data does a ServoElementSnapshot
 * contains.
 */
enum class ServoElementSnapshotFlags : uint8_t
{
  State = 1 << 0,
  Attributes = 1 << 1,
  Id = 1 << 2,
  MaybeClass = 1 << 3,
  OtherPseudoClassState = 1 << 4,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ServoElementSnapshotFlags)

/**
 * This class holds all non-tree-structural state of an element that might be
 * used for selector matching eventually.
 *
 * This means the attributes, and the element state, such as :hover, :active,
 * etc...
 */
class ServoElementSnapshot
{
  typedef dom::BorrowedAttrInfo BorrowedAttrInfo;
  typedef dom::Element Element;
  typedef EventStates::ServoType ServoStateType;

public:
  typedef ServoElementSnapshotFlags Flags;

  explicit ServoElementSnapshot(const Element* aElement);

  ~ServoElementSnapshot()
  {
    MOZ_COUNT_DTOR(ServoElementSnapshot);
  }

  bool HasAttrs() const { return HasAny(Flags::Attributes); }

  bool HasState() const { return HasAny(Flags::State); }

  bool HasOtherPseudoClassState() const
  {
    return HasAny(Flags::OtherPseudoClassState);
  }

  /**
   * Captures the given state (if not previously captured).
   */
  void AddState(EventStates aState)
  {
    if (!HasAny(Flags::State)) {
      mState = aState.ServoValue();
      mContains |= Flags::State;
    }
  }

  /**
   * Captures the given element attributes (if not previously captured).
   *
   * The attribute name and namespace are used to note which kind of attribute
   * has changed.
   */
  void AddAttrs(Element* aElement,
                int32_t aNameSpaceID,
                nsAtom* aAttribute)
  {
    if (aNameSpaceID == kNameSpaceID_None) {
      if (aAttribute == nsGkAtoms::_class) {
        mClassAttributeChanged = true;
      } else if (aAttribute == nsGkAtoms::id) {
        mIdAttributeChanged = true;
      } else {
        mOtherAttributeChanged = true;
      }
    } else {
      mOtherAttributeChanged = true;
    }

    if (HasAttrs()) {
      return;
    }

    uint32_t attrCount = aElement->GetAttrCount();
    const nsAttrName* attrName;
    for (uint32_t i = 0; i < attrCount; ++i) {
      attrName = aElement->GetAttrNameAt(i);
      const nsAttrValue* attrValue =
        aElement->GetParsedAttr(attrName->LocalName(), attrName->NamespaceID());
      mAttrs.AppendElement(ServoAttrSnapshot(*attrName, *attrValue));
    }
    mContains |= Flags::Attributes;
    if (aElement->HasID()) {
      mContains |= Flags::Id;
    }
    if (const nsAttrValue* classValue = aElement->GetClasses()) {
      mClass = *classValue;
      mContains |= Flags::MaybeClass;
    }
  }

  /**
   * Captures some other pseudo-class matching state not included in
   * EventStates.
   */
  void AddOtherPseudoClassState(Element* aElement);

  /**
   * Needed methods for attribute matching.
   */
  BorrowedAttrInfo GetAttrInfoAt(uint32_t aIndex) const
  {
    MOZ_ASSERT(HasAttrs());
    if (aIndex >= mAttrs.Length()) {
      return BorrowedAttrInfo(nullptr, nullptr);
    }
    return BorrowedAttrInfo(&mAttrs[aIndex].mName, &mAttrs[aIndex].mValue);
  }

  const nsAttrValue* GetParsedAttr(nsAtom* aLocalName) const
  {
    return GetParsedAttr(aLocalName, kNameSpaceID_None);
  }

  const nsAttrValue* GetParsedAttr(nsAtom* aLocalName,
                                   int32_t aNamespaceID) const
  {
    MOZ_ASSERT(HasAttrs());
    uint32_t i, len = mAttrs.Length();
    if (aNamespaceID == kNameSpaceID_None) {
      // This should be the common case so lets make an optimized loop
      for (i = 0; i < len; ++i) {
        if (mAttrs[i].mName.Equals(aLocalName)) {
          return &mAttrs[i].mValue;
        }
      }

      return nullptr;
    }

    for (i = 0; i < len; ++i) {
      if (mAttrs[i].mName.Equals(aLocalName, aNamespaceID)) {
        return &mAttrs[i].mValue;
      }
    }

    return nullptr;
  }

  const nsAttrValue* GetClasses() const
  {
    MOZ_ASSERT(HasAttrs());
    return &mClass;
  }

  bool IsInChromeDocument() const { return mIsInChromeDocument; }
  bool SupportsLangAttr() const { return mSupportsLangAttr; }

  bool HasAny(Flags aFlags) const { return bool(mContains & aFlags); }

  bool IsTableBorderNonzero() const
  {
    MOZ_ASSERT(HasOtherPseudoClassState());
    return mIsTableBorderNonzero;
  }

  bool IsMozBrowserFrame() const
  {
    MOZ_ASSERT(HasOtherPseudoClassState());
    return mIsMozBrowserFrame;
  }

private:
  // TODO: Profile, a 1 or 2 element AutoTArray could be worth it, given we know
  // we're dealing with attribute changes when we take snapshots of attributes,
  // though it can be wasted space if we deal with a lot of state-only
  // snapshots.
  nsTArray<ServoAttrSnapshot> mAttrs;
  nsAttrValue mClass;
  ServoStateType mState;
  Flags mContains;
  bool mIsHTMLElementInHTMLDocument : 1;
  bool mIsInChromeDocument : 1;
  bool mSupportsLangAttr : 1;
  bool mIsTableBorderNonzero : 1;
  bool mIsMozBrowserFrame : 1;
  bool mClassAttributeChanged : 1;
  bool mIdAttributeChanged : 1;
  bool mOtherAttributeChanged : 1;
};

} // namespace mozilla

#endif
