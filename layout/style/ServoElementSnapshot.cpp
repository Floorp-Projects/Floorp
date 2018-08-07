/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoElementSnapshot.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/dom/Element.h"
#include "nsIContentInlines.h"
#include "nsContentUtils.h"

namespace mozilla {

ServoElementSnapshot::ServoElementSnapshot(const Element& aElement)
  : mState(0)
  , mContains(Flags(0))
  , mIsTableBorderNonzero(false)
  , mIsMozBrowserFrame(false)
  , mClassAttributeChanged(false)
  , mIdAttributeChanged(false)
  , mOtherAttributeChanged(false)
{
  MOZ_COUNT_CTOR(ServoElementSnapshot);
  MOZ_ASSERT(NS_IsMainThread());
  mIsHTMLElementInHTMLDocument =
    aElement.IsHTMLElement() && aElement.IsInHTMLDocument();
  mIsInChromeDocument = nsContentUtils::IsChromeDoc(aElement.OwnerDoc());
  mSupportsLangAttr = aElement.SupportsLangAttr();
}

void
ServoElementSnapshot::AddOtherPseudoClassState(const Element& aElement)
{
  if (HasOtherPseudoClassState()) {
    return;
  }

  mIsTableBorderNonzero = Gecko_IsTableBorderNonzero(&aElement);
  mIsMozBrowserFrame = Gecko_IsBrowserFrame(&aElement);

  mContains |= Flags::OtherPseudoClassState;
}

} // namespace mozilla
