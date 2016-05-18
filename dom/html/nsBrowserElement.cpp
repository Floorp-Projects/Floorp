/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBrowserElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BrowserElementBinding.h"
#include "mozilla/dom/BrowserElementAudioChannel.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/ToJSValue.h"

#include "AudioChannelService.h"

#include "mozIApplication.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsFrameLoader.h"
#include "nsIAppsService.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMElement.h"
#include "nsIMozBrowserFrame.h"
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
  bool isMozBrowserOrApp;
  nsCOMPtr<nsIFrameLoader> frameLoader = GetFrameLoader();
  NS_ENSURE_TRUE_VOID(frameLoader);
  nsresult rv = frameLoader->GetOwnerIsMozBrowserOrAppFrame(&isMozBrowserOrApp);
  NS_ENSURE_SUCCESS_VOID(rv);
  rv = frameLoader->GetOwnerIsWidget(&mOwnerIsWidget);
  NS_ENSURE_SUCCESS_VOID(rv);

  if (!isMozBrowserOrApp) {
    return;
  }

  if (!mBrowserElementAPI) {
    mBrowserElementAPI = do_CreateInstance("@mozilla.org/dom/browser-element-api;1");
    if (NS_WARN_IF(!mBrowserElementAPI)) {
      return;
    }
  }
  mBrowserElementAPI->SetFrameLoader(frameLoader);
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
  aRv.MightThrowJSException();
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.StealExceptionFromJSContext(cx);
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
nsBrowserElement::FindAll(const nsAString& aSearchString,
                          BrowserFindCaseSensitivity aCaseSensitivity,
                          ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  uint32_t caseSensitivity;
  if (aCaseSensitivity == BrowserFindCaseSensitivity::Case_insensitive) {
    caseSensitivity = nsIBrowserElementAPI::FIND_CASE_INSENSITIVE;
  } else {
    caseSensitivity = nsIBrowserElementAPI::FIND_CASE_SENSITIVE;
  }

  nsresult rv = mBrowserElementAPI->FindAll(aSearchString, caseSensitivity);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
nsBrowserElement::FindNext(BrowserFindDirection aDirection,
                          ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  uint32_t direction;
  if (aDirection == BrowserFindDirection::Backward) {
    direction = nsIBrowserElementAPI::FIND_BACKWARD;
  } else {
    direction = nsIBrowserElementAPI::FIND_FORWARD;
  }

  nsresult rv = mBrowserElementAPI->FindNext(direction);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
nsBrowserElement::ClearMatch(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->ClearMatch();

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
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

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->SetInputMethodActive(aIsActive,
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
nsBrowserElement::GetAllowedAudioChannels(
                 nsTArray<RefPtr<BrowserElementAudioChannel>>& aAudioChannels,
                 ErrorResult& aRv)
{
  aAudioChannels.Clear();

  // If empty, it means that this is the first call of this method.
  if (mBrowserElementAudioChannels.IsEmpty()) {
    nsCOMPtr<nsIFrameLoader> frameLoader = GetFrameLoader();
    if (NS_WARN_IF(!frameLoader)) {
      return;
    }

    bool isMozBrowserOrApp;
    aRv = frameLoader->GetOwnerIsMozBrowserOrAppFrame(&isMozBrowserOrApp);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    if (!isMozBrowserOrApp) {
      return;
    }

    nsCOMPtr<nsIDOMElement> frameElement;
    aRv = frameLoader->GetOwnerElement(getter_AddRefs(frameElement));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(frameElement);

    nsCOMPtr<nsIDOMDocument> doc;
    aRv = frameElement->GetOwnerDocument(getter_AddRefs(doc));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(doc);

    nsCOMPtr<mozIDOMWindowProxy> win;
    aRv = doc->GetDefaultView(getter_AddRefs(win));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(win);

    auto* window = nsPIDOMWindowOuter::From(win);
    nsPIDOMWindowInner* innerWindow = window->GetCurrentInnerWindow();

    nsCOMPtr<nsIMozBrowserFrame> mozBrowserFrame =
      do_QueryInterface(frameElement);
    if (NS_WARN_IF(!mozBrowserFrame)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    nsAutoString manifestURL;
    aRv = mozBrowserFrame->GetAppManifestURL(manifestURL);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    nsCOMPtr<mozIApplication> parentApp;
    aRv = GetParentApplication(getter_AddRefs(parentApp));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
            ("nsBrowserElement, GetAllowedAudioChannels, this = %p\n", this));

    GenerateAllowedAudioChannels(innerWindow, frameLoader, mBrowserElementAPI,
                                 manifestURL, parentApp,
                                 mBrowserElementAudioChannels, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  aAudioChannels.AppendElements(mBrowserElementAudioChannels);
}

/* static */ void
nsBrowserElement::GenerateAllowedAudioChannels(
                 nsPIDOMWindowInner* aWindow,
                 nsIFrameLoader* aFrameLoader,
                 nsIBrowserElementAPI* aAPI,
                 const nsAString& aManifestURL,
                 mozIApplication* aParentApp,
                 nsTArray<RefPtr<BrowserElementAudioChannel>>& aAudioChannels,
                 ErrorResult& aRv)
{
  MOZ_ASSERT(aAudioChannels.IsEmpty());

  nsCOMPtr<nsIAppsService> appsService =
    do_GetService("@mozilla.org/AppsService;1");
  if (NS_WARN_IF(!appsService)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<mozIApplication> app;
  aRv = appsService->GetAppByManifestURL(aManifestURL, getter_AddRefs(app));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // Normal is always allowed.
  nsTArray<RefPtr<BrowserElementAudioChannel>> channels;

  RefPtr<BrowserElementAudioChannel> ac =
    BrowserElementAudioChannel::Create(aWindow, aFrameLoader, aAPI,
                                       AudioChannel::Normal,
                                       aManifestURL, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  channels.AppendElement(ac);

  if (app) {
    const nsAttrValue::EnumTable* audioChannelTable =
      AudioChannelService::GetAudioChannelTable();

    bool allowed;
    nsAutoCString permissionName;

    for (uint32_t i = 0; audioChannelTable && audioChannelTable[i].tag; ++i) {
      permissionName.AssignASCII("audio-channel-");
      permissionName.AppendASCII(audioChannelTable[i].tag);

      // In case of nested iframes we want to check if the parent has the
      // permission to use this AudioChannel.
      if (aParentApp) {
        aRv = aParentApp->HasPermission(permissionName.get(), &allowed);
        if (NS_WARN_IF(aRv.Failed())) {
          return;
        }

        if (!allowed) {
          continue;
        }
      }

      aRv = app->HasPermission(permissionName.get(), &allowed);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      if (allowed) {
        RefPtr<BrowserElementAudioChannel> ac =
          BrowserElementAudioChannel::Create(aWindow, aFrameLoader, aAPI,
                                             (AudioChannel)audioChannelTable[i].value,
                                             aManifestURL, aRv);
        if (NS_WARN_IF(aRv.Failed())) {
          return;
        }

        channels.AppendElement(ac);
      }
    }
  }

  aAudioChannels.SwapElements(channels);
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetMuted(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetMuted(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::Mute(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Mute();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::Unmute(ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->Unmute();

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetVolume(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);
  NS_ENSURE_TRUE(IsNotWidgetOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetVolume(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

void
nsBrowserElement::SetVolume(float aVolume, ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));
  NS_ENSURE_TRUE_VOID(IsNotWidgetOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SetVolume(aVolume);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

void
nsBrowserElement::SetNFCFocus(bool aIsFocus,
                              ErrorResult& aRv)
{
  NS_ENSURE_TRUE_VOID(IsBrowserElementOrThrow(aRv));

  nsresult rv = mBrowserElementAPI->SetNFCFocus(aIsFocus);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
}

already_AddRefed<DOMRequest>
nsBrowserElement::ExecuteScript(const nsAString& aScript,
                                const BrowserElementExecuteScriptOptions& aOptions,
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
  aRv.MightThrowJSException();
  if (!ToJSValue(cx, aOptions, &options)) {
    aRv.StealExceptionFromJSContext(cx);
    return nullptr;
  }

  nsresult rv = mBrowserElementAPI->ExecuteScript(aScript, options, getter_AddRefs(req));

  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_INVALID_ARG) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    } else {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    }
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetStructuredData(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetStructuredData(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}

already_AddRefed<DOMRequest>
nsBrowserElement::GetWebManifest(ErrorResult& aRv)
{
  NS_ENSURE_TRUE(IsBrowserElementOrThrow(aRv), nullptr);

  nsCOMPtr<nsIDOMDOMRequest> req;
  nsresult rv = mBrowserElementAPI->GetWebManifest(getter_AddRefs(req));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  return req.forget().downcast<DOMRequest>();
}



} // namespace mozilla
