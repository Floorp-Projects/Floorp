/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MappedDeclarationsBuilder.h"

#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"

namespace mozilla {

void MappedDeclarationsBuilder::SetIdentAtomValue(nsCSSPropertyID aId,
                                                  nsAtom* aValue) {
  Servo_DeclarationBlock_SetIdentStringValue(&EnsureDecls(), aId, aValue);
  if (aId == eCSSProperty__x_lang) {
    // This forces the lang prefs result to be cached so that we can access them
    // off main thread during traversal.
    //
    // FIXME(emilio): Can we move mapped attribute declarations across
    // documents? Isn't this wrong in that case? This is pretty out of place
    // anyway.
    mDocument.ForceCacheLang(aValue);
  }
}

void MappedDeclarationsBuilder::SetBackgroundImage(const nsAttrValue& aValue) {
  if (aValue.Type() != nsAttrValue::eURL) {
    return;
  }
  nsAutoString str;
  aValue.ToString(str);
  nsAutoCString utf8;
  CopyUTF16toUTF8(str, utf8);
  Servo_DeclarationBlock_SetBackgroundImage(
      &EnsureDecls(), &utf8, mDocument.DefaultStyleAttrURLData());
}

}  // namespace mozilla
