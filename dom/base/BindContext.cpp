/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/BrowsingContext.h"

#include "mozilla/dom/Document.h"
#include "mozilla/StaticPrefs_browser.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsPIDOMWindow.h"

namespace mozilla::dom {

bool BindContext::AllowsAutoFocus() const {
  if (!StaticPrefs::browser_autofocus()) {
    return false;
  }
  if (!InUncomposedDoc()) {
    return false;
  }
  if (mDoc.IsBeingUsedAsImage()) {
    return false;
  }
  return IsSameOriginAsTop();
}

bool BindContext::IsSameOriginAsTop() const {
  BrowsingContext* browsingContext = mDoc.GetBrowsingContext();
  if (!browsingContext) {
    return false;
  }

  nsPIDOMWindowOuter* topWindow = browsingContext->Top()->GetDOMWindow();
  if (!topWindow) {
    // If we don't have a DOMWindow, We are not in same origin.
    return false;
  }

  Document* topLevelDocument = topWindow->GetExtantDoc();
  if (!topLevelDocument) {
    return false;
  }

  return NS_SUCCEEDED(nsContentUtils::CheckSameOrigin(topLevelDocument, &mDoc));
}

}  // namespace mozilla::dom
