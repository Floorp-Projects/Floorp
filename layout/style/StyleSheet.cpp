/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StyleSheet.h"

#include "mozilla/ServoStyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/CSSStyleSheet.h"

namespace mozilla {

StyleSheet::StyleSheet(StyleBackendType aType)
  : mDocument(nullptr)
  , mOwningNode(nullptr)
  , mParsingMode(css::eUserSheetFeatures)
  , mType(aType)
  , mDisabled(false)
{
}

StyleSheet::StyleSheet(const StyleSheet& aCopy,
                       nsIDocument* aDocumentToUse,
                       nsINode* aOwningNodeToUse)
  : mDocument(aDocumentToUse)
  , mOwningNode(aOwningNodeToUse)
  , mParsingMode(aCopy.mParsingMode)
  , mType(aCopy.mType)
  , mDisabled(aCopy.mDisabled)
{
}

StyleSheetInfo&
StyleSheet::SheetInfo()
{
  if (IsServo()) {
    return AsServo();
  }
  return *AsGecko().mInner;
}

} // namespace mozilla
