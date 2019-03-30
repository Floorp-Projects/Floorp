/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MappedDeclarations.h"

#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"

namespace mozilla {

void MappedDeclarations::SetIdentAtomValue(nsCSSPropertyID aId,
                                           nsAtom* aValue) {
  Servo_DeclarationBlock_SetIdentStringValue(mDecl, aId, aValue);
  if (aId == eCSSProperty__x_lang) {
    // This forces the lang prefs result to be cached so that we can access them
    // off main thread during traversal.
    //
    // FIXME(emilio): Can we move mapped attribute declarations across
    // documents? Isn't this wrong in that case? This is pretty out of place
    // anyway.
    mDocument->ForceCacheLang(aValue);
  }
}

void MappedDeclarations::SetBackgroundImage(const nsAttrValue& aValue) {
  if (aValue.Type() != nsAttrValue::eURL) {
    return;
  }
  // FIXME(emilio): Going through URL parsing again seems slightly wasteful.
  nsAutoString str;
  aValue.ToString(str);
  Servo_DeclarationBlock_SetBackgroundImage(
      mDecl, &str, mDocument->DefaultStyleAttrURLData());
}

}  // namespace mozilla
