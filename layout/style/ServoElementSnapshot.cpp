/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/dom/Element.h"
#include "nsIContentInlines.h"
#include "nsContentUtils.h"

namespace mozilla {

ServoElementSnapshot::ServoElementSnapshot(const Element* aElement)
  : mState(0)
  , mContains(Flags(0))
  , mIsTableBorderNonzero(false)
  , mIsMozBrowserFrame(false)
{
  MOZ_COUNT_CTOR(ServoElementSnapshot);
  mIsHTMLElementInHTMLDocument =
    aElement->IsHTMLElement() && aElement->IsInHTMLDocument();
  mIsInChromeDocument =
    nsContentUtils::IsChromeDoc(aElement->OwnerDoc());
}

ServoElementSnapshot::~ServoElementSnapshot()
{
  MOZ_COUNT_DTOR(ServoElementSnapshot);
}

void
ServoElementSnapshot::AddAttrs(Element* aElement)
{
  MOZ_ASSERT(aElement);

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
  if (aElement->MayHaveClass()) {
    mContains |= Flags::MaybeClass;
  }
}

void
ServoElementSnapshot::AddOtherPseudoClassState(Element* aElement)
{
  MOZ_ASSERT(aElement);

  if (HasOtherPseudoClassState()) {
    return;
  }

  mIsTableBorderNonzero =
    *nsCSSPseudoClasses::MatchesElement(CSSPseudoClassType::mozTableBorderNonzero,
                                        aElement);
  mIsMozBrowserFrame =
    *nsCSSPseudoClasses::MatchesElement(CSSPseudoClassType::mozBrowserFrame,
                                        aElement);

  mContains |= Flags::OtherPseudoClassState;
}

} // namespace mozilla
