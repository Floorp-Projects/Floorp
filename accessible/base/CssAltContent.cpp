/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CssAltContent.h"
#include "nsIContent.h"
#include "nsIFrame.h"

namespace mozilla::a11y {

CssAltContent::CssAltContent(nsIContent* aContent) {
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return;
  }
  if (!frame->IsReplaced()) {
    return;
  }
  // Check if this is for a pseudo-element.
  if (aContent->IsInNativeAnonymousSubtree()) {
    nsIContent* parent = aContent->GetParent();
    if (parent && (parent->IsGeneratedContentContainerForBefore() ||
                   parent->IsGeneratedContentContainerForAfter() ||
                   parent->IsGeneratedContentContainerForMarker())) {
      // We need the frame from the pseudo-element to get the content style.
      frame = parent->GetPrimaryFrame();
      if (!frame) {
        return;
      }
    }
  }
  mItems = frame->StyleContent()->AltContentItems();
}

void CssAltContent::AppendToString(nsAString& aOut) {
  // There can be multiple alt text items.
  for (const auto& item : mItems) {
    if (item.IsString()) {
      aOut.Append(NS_ConvertUTF8toUTF16(item.AsString().AsString()));
    }
  }
}

}  // namespace mozilla::a11y
