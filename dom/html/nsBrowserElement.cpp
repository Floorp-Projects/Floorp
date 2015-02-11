/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBrowserElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BrowserElementBinding.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsFrameLoader.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMElement.h"
#include "nsINode.h"
#include "nsIPrincipal.h"

using namespace mozilla::dom;

namespace mozilla {

bool
nsBrowserElement::IsBrowserElementOrThrow(ErrorResult& aRv)
{
  if (mBrowserElementAPI) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
  return false;
}

bool
nsBrowserElement::IsNotWidgetOrThrow(ErrorResult& aRv)
{
  if (!mOwnerIsWidget) {
    return true;
  }
  aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
  return false;
}

void
nsBrowserElement::InitBrowserElementAPI()
{
  bool isBrowserOrApp;
  nsCOMPtr<nsIFrameLoader> frameLoader = GetFrameLoader();
  NS_ENSURE_TRUE_VOID(frameLoader);
  nsresult rv = frameLoader->GetOwnerIsBrowserOrAppFrame(&isBrowserOrApp);
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = frameLoader->GetOwnerIsWidget(&mOwnerIsWidget);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!isBrowserOrApp) {
    return;
  }

  mBrowserElementAPI = do_CreateInstance("@mozilla.org/dom/browser-element-api;1");
  if (mBrowserElementAPI) {
    mBrowserElementAPI->SetFrameLoader(frameLoader);
  }
}

void
nsBrowserElement::SetVisible(bool aVisible, ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SetVisible(aVisible);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetVisible(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetVisible(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::SetActive(bool aVisible, ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SetActive(aVisible);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

bool
nsBrowserElement::GetActive(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), false);

  bool isActive;
  nsresult rv = mBrowserElementAPI->GetActive(&isActive);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  return isActive;
}

void
nsBrowserElement::SendMouseEvent(const nsAString& aType,
                                 uint32_t aX,
                                 uint32_t aY,
                                 uint32_t aButton,
                                 uint32_t aClickCount,
                                 uint32_t aModifiers,
                                 ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SendMouseEvent(aType,
                                                   aX,
                                                   aY,
                                                   aButton,
                                                   aClickCount,
                                                   aModifiers);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::SendTouchEvent(const nsAString& aType,
                                 const Sequence<uint32_t>& aIdentifiers,
                                 const Sequence<int32_t>& aXs,
                                 const Sequence<int32_t>& aYs,
                                 const Sequence<uint32_t>& aRxs,
                                 const Sequence<uint32_t>& aRys,
                                 const Sequence<float>& aRotationAngles,
                                 const Sequence<float>& aForces,
                                 uint32_t aCount,
                                 uint32_t aModifiers,
                                 ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  if (aIdentifiers.Length() != aCount ||
      aXs.Length() != aCount ||
      aYs.Length() != aCount ||
      aRxs.Length() != aCount ||
      aRys.Length() != aCount ||
      aRotationAngles.Length() != aCount ||
      aForces.Length() != aCount) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsresult rv = mBrowserElementAPI->SendTouchEvent(aType,
                                                   aIdentifiers.Elements(),
                                                   aXs.Elements(),
                                                   aYs.Elements(),
                                                   aRxs.Elements(),
                                                   aRys.Elements(),
                                                   aRotationAngles.Elements(),
                                                   aForces.Elements(),
                                                   aCount,
                                                   aModifiers);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::GoBack(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->GoBack();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::GoForward(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->GoForward();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::Reload(bool aHardReload, ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Reload(aHardReload);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::Stop(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Stop();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::Download(const nsAString& aUrl,
                           const BrowserElementDownloadOptions& aOptions,
                           ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedObj = do_QueryInterface(mBrowserElementAPI);
  MOZ_ASSERT(wrappedObj, "Failed to get wrapped JS from XPCOM component.");
  AutoJSAPI jsapi;
  jsapi.Init(wrappedObj->GetJSObject());
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  nsresult rv = mBrowserElementAPI->Download(aUrl, options, getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::PurgeHistory(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->PurgeHistory(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetScreenshot(uint32_t aWidth,
                                uint32_t aHeight,
                                const nsAString& aMimeType,
                                ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetScreenshot(aWidth, aHeight, aMimeType,
                                                  getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (rv == NS_ERROR_INVALID_ARG) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    } else {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::Zoom(float aZoom, ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Zoom(aZoom);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetCanGoBack(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetCanGoBack(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetCanGoForward(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetCanGoForward(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetContentDimensions(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetContentDimensions(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::AddNextPaintListener(BrowserElementNextPaintEventCallback& aListener,
                                       ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  CallbackObjectHolder<BrowserElementNextPaintEventCallback,
                       nsIBrowserElementNextPaintListener> holder(&aListener);
  nsCOMPtr<nsIBrowserElementNextPaintListener> listener = holder.ToXPCOMCallback();

  nsresult rv = mBrowserElementAPI->AddNextPaintListener(listener);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::RemoveNextPaintListener(BrowserElementNextPaintEventCallback& aListener,
                                          ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  CallbackObjectHolder<BrowserElementNextPaintEventCallback,
                       nsIBrowserElementNextPaintListener> holder(&aListener);
  nsCOMPtr<nsIBrowserElementNextPaintListener> listener = holder.ToXPCOMCallback();

  nsresult rv = mBrowserElementAPI->RemoveNextPaintListener(listener);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::SetInputMethodActive(bool aIsActive,
                                       ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  nsCOMPtr<nsIFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsCOMPtr<nsIDOMElement> ownerElement;
  nsresult rv = frameLoader->GetOwnerElement(getter_AddRefs(ownerElement));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(ownerElement);
  nsCOMPtr<nsIPrincipal> principal = node->NodePrincipal();
  if (!nsContentUtils::IsExactSitePermAllow(principal, "input-manage")) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIDOMDOMRequest> req;
  rv = mBrowserElementAPI->SetInputMethodActive(aIsActive,
                                                getter_AddRefs(req));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (rv == NS_ERROR_INVALID_ARG) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    } else {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::SetNFCFocus(bool aIsFocus,
                              ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsRefPtr<nsFrameLoader> frameLoader = GetFrameLoader();
  if (!frameLoader) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCOMPtr<nsIDOMElement> ownerElement;
  nsresult rv = frameLoader->GetOwnerElement(getter_AddRefs(ownerElement));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(ownerElement);
  nsCOMPtr<nsIPrincipal> principal = node->NodePrincipal();
  if (!nsContentUtils::IsExactSitePermAllow(principal, "nfc-manager")) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  rv = mBrowserElementAPI->SetNFCFocus(aIsFocus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

} // namespace mozilla
