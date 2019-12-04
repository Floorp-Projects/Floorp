/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBrowserElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/BrowserElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"

#include "nsComponentManagerUtils.h"
#include "nsFrameLoader.h"
#include "nsIMozBrowserFrame.h"
#include "nsINode.h"
#include "nsIPrincipal.h"

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

void nsBrowserElement::SendMouseEvent(const nsAString& aType, uint32_t aX,
                                      uint32_t aY, uint32_t aButton,
                                      uint32_t aClickCount, uint32_t aModifiers,
                                      ErrorResult& aRv) {
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SendMouseEvent(aType, aX, aY, aButton,
                                                   aClickCount, aModifiers);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void nsBrowserElement::GoBack(ErrorResult& aRv) {
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->GoBack();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void nsBrowserElement::GoForward(ErrorResult& aRv) {
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->GoForward();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void nsBrowserElement::Reload(bool aHardReload, ErrorResult& aRv) {
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Reload(aHardReload);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void nsBrowserElement::Stop(ErrorResult& aRv) {
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Stop();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<Promise> nsBrowserElement::GetCanGoBack(ErrorResult& aRv) {
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  RefPtr<Promise> p;
  nsresult rv = mBrowserElementAPI->GetCanGoBack(getter_AddRefs(p));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return p.forget();
}

already_AddRefed<Promise> nsBrowserElement::GetCanGoForward(ErrorResult& aRv) {
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  RefPtr<Promise> p;
  nsresult rv = mBrowserElementAPI->GetCanGoForward(getter_AddRefs(p));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return p.forget();
}

}  // namespace mozilla
