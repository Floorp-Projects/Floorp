/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBrowserElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"

#include "nsComponentManagerUtils.h"
#include "nsFrameLoader.h"
#include "nsINode.h"

#include "js/Wrapper.h"

using namespace mozilla::dom;

namespace mozilla {

bool nsBrowserElement::IsBrowserElementOrThrow(ErrorResult& aRv) {
  if (mBrowserElementAPI) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
  return false;
}

void nsBrowserElement::InitBrowserElementAPI() {
  RefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  NS_ENSURE_TRUE_VOID(frameLoader);

  if (!frameLoader->OwnerIsMozBrowserFrame()) {
    return;
  }

  if (!mBrowserElementAPI) {
    mBrowserElementAPI =
        do_CreateInstance("@mozilla.org/dom/browser-element-api;1");
    if (NS_WARN_IF(!mBrowserElementAPI)) {
      return;
    }
  }
  mBrowserElementAPI->SetFrameLoader(frameLoader);
}

void nsBrowserElement::DestroyBrowserElementFrameScripts() {
  if (!mBrowserElementAPI) {
    return;
  }
  mBrowserElementAPI->DestroyFrameScripts();
}

}  // namespace mozilla
