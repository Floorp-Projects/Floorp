/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/GeckoBindings.h"
#include "mozilla/dom/Element.h"
#include "nsIContentInlines.h"
#include "nsContentUtils.h"

namespace mozilla {

ServoElementSnapshot::ServoElementSnapshot(const Element& aElement)
    : mState(0),
      mContains(Flags(0)),
      mIsTableBorderNonzero(false),
      mIsSelectListBox(false),
      mClassAttributeChanged(false),
      mIdAttributeChanged(false) {
  MOZ_COUNT_CTOR(ServoElementSnapshot);
  MOZ_ASSERT(NS_IsMainThread());
  mIsInChromeDocument = nsContentUtils::IsChromeDoc(aElement.OwnerDoc());
  mSupportsLangAttr = aElement.SupportsLangAttr();
}

void ServoElementSnapshot::AddOtherPseudoClassState(const Element& aElement) {
  if (HasOtherPseudoClassState()) {
    return;
  }

  mIsTableBorderNonzero = Gecko_IsTableBorderNonzero(&aElement);
  mIsSelectListBox = Gecko_IsSelectListBox(&aElement);

  mContains |= Flags::OtherPseudoClassState;
}

void ServoElementSnapshot::AddAttrs(const Element& aElement,
                                    int32_t aNameSpaceID, nsAtom* aAttribute) {
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::_class) {
      if (mClassAttributeChanged) {
        return;
      }
      mClassAttributeChanged = true;
    } else if (aAttribute == nsGkAtoms::id) {
      if (mIdAttributeChanged) {
        return;
      }
      mIdAttributeChanged = true;
    }
  }

  if (!mChangedAttrNames.Contains(aAttribute)) {
    mChangedAttrNames.AppendElement(aAttribute);
  }

  if (HasAttrs()) {
    return;
  }

  uint32_t attrCount = aElement.GetAttrCount();
  mAttrs.SetCapacity(attrCount);
  for (uint32_t i = 0; i < attrCount; ++i) {
    const BorrowedAttrInfo info = aElement.GetAttrInfoAt(i);
    MOZ_ASSERT(info);
    mAttrs.AppendElement(AttrArray::InternalAttr{*info.mName, *info.mValue});
  }

  mContains |= Flags::Attributes;
  if (aElement.HasID()) {
    mContains |= Flags::Id;
  }

  if (const nsAttrValue* classValue = aElement.GetClasses()) {
    // FIXME(emilio): It's pretty unfortunate that this is only relevant for
    // SVG, yet it's a somewhat expensive copy. We should be able to do
    // better!
    mClass = *classValue;
    mContains |= Flags::MaybeClass;
  }
}

}  // namespace mozilla
