/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootAccessibleWrap.h"

#include "Compatibility.h"
#include "mozilla/PresShell.h"
#include "nsCoreUtils.h"
#include "nsWinUtils.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Constructor/destructor

RootAccessibleWrap::RootAccessibleWrap(dom::Document* aDocument,
                                       PresShell* aPresShell)
    : RootAccessible(aDocument, aPresShell) {}

RootAccessibleWrap::~RootAccessibleWrap() {}

////////////////////////////////////////////////////////////////////////////////
// RootAccessible

void RootAccessibleWrap::DocumentActivated(DocAccessible* aDocument) {
  // This check will never work with e10s enabled, in other words, as of
  // Firefox 57.
  if (Compatibility::IsDolphin() &&
      nsCoreUtils::IsTopLevelContentDocInProcess(aDocument->DocumentNode())) {
    MOZ_ASSERT(XRE_IsParentProcess());
    uint32_t count = mChildDocuments.Length();
    for (uint32_t idx = 0; idx < count; idx++) {
      DocAccessible* childDoc = mChildDocuments[idx];
      HWND childDocHWND = static_cast<HWND>(childDoc->GetNativeWindow());
      if (childDoc != aDocument)
        nsWinUtils::HideNativeWindow(childDocHWND);
      else
        nsWinUtils::ShowNativeWindow(childDocHWND);
    }
  }
}
