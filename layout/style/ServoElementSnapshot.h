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
#include "nsAttrName.h"
#include "nsAttrValue.h"
#include "nsChangeHint.h"
#include "nsIAtom.h"

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

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
  ~ServoElementSnapshot();

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
   */
  void AddAttrs(Element* aElement);

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

  const nsAttrValue* GetParsedAttr(nsIAtom* aLocalName) const
  {
    return GetParsedAttr(aLocalName, kNameSpaceID_None);
  }

  const nsAttrValue* GetParsedAttr(nsIAtom* aLocalName,
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

  bool IsInChromeDocument() const
  {
    return mIsInChromeDocument;
  }

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
  ServoStateType mState;
  Flags mContains;
  bool mIsHTMLElementInHTMLDocument : 1;
  bool mIsInChromeDocument : 1;
  bool mIsTableBorderNonzero : 1;
  bool mIsMozBrowserFrame : 1;
};

} // namespace mozilla

#endif
