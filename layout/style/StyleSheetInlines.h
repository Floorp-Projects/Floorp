/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetInlines_h
#define mozilla_StyleSheetInlines_h

#include "mozilla/StyleSheet.h"
#include "nsINode.h"

namespace mozilla {

void StyleSheet::SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI,
                         nsIURI* aBaseURI) {
  MOZ_ASSERT(aSheetURI && aBaseURI, "null ptr");
  MOZ_ASSERT(!HasRules() && !IsComplete(),
             "Can't call SetURIs on sheets that are complete or have rules");
  StyleSheetInfo& info = Inner();
  info.mSheetURI = aSheetURI;
  info.mOriginalSheetURI = aOriginalSheetURI;
  info.mBaseURI = aBaseURI;
}

dom::ParentObject StyleSheet::GetParentObject() const {
  if (IsConstructed()) {
    return dom::ParentObject(mConstructorDocument.get());
  }
  if (mOwningNode) {
    return dom::ParentObject(mOwningNode);
  }
  return dom::ParentObject(mParentSheet);
}

}  // namespace mozilla

#endif  // mozilla_StyleSheetInlines_h
