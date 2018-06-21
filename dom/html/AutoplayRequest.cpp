/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AutoplayRequest.h"

#include "nsGlobalWindowInner.h"
#include "nsISupportsImpl.h"
#include "mozilla/Logging.h"
#include "nsContentPermissionHelper.h"

extern mozilla::LazyLogModule gMediaElementLog;

#define PLAY_REQUEST_LOG(msg, ...)                                             \
  MOZ_LOG(gMediaElementLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {

NS_IMPL_ISUPPORTS(AutoplayRequest, nsIContentPermissionRequest)

AutoplayRequest::AutoplayRequest(nsGlobalWindowInner* aWindow,
                                 nsIPrincipal* aNodePrincipal,
                                 nsIEventTarget* aMainThreadTarget)
  : mWindow(do_GetWeakReference(aWindow))
  , mNodePrincipal(aNodePrincipal)
  , mMainThreadTarget(aMainThreadTarget)
  , mRequester(new dom::nsContentPermissionRequester(aWindow))
{
  MOZ_RELEASE_ASSERT(mNodePrincipal);
}

AutoplayRequest::~AutoplayRequest() {}

already_AddRefed<AutoplayRequest>
AutoplayRequest::Create(nsGlobalWindowInner* aWindow)
{
  if (!aWindow || !aWindow->GetPrincipal() ||
      !aWindow->EventTargetFor(TaskCategory::Other)) {
    return nullptr;
  }
  RefPtr<AutoplayRequest> request =
    new AutoplayRequest(aWindow,
                        aWindow->GetPrincipal(),
                        aWindow->EventTargetFor(TaskCategory::Other));
  PLAY_REQUEST_LOG("AutoplayRequest %p Create()", request.get());
  return request.forget();
}

NS_IMETHODIMP
AutoplayRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);

  nsCOMPtr<nsIPrincipal> principal = mNodePrincipal;
  principal.forget(aRequestingPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::GetTypes(nsIArray** aTypes)
{
  NS_ENSURE_ARG_POINTER(aTypes);

  nsTArray<nsString> emptyOptions;
  return dom::nsContentPermissionUtils::CreatePermissionArray(
    NS_LITERAL_CSTRING("autoplay-media"),
    NS_LITERAL_CSTRING("unused"),
    emptyOptions,
    aTypes);
}

NS_IMETHODIMP
AutoplayRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  if (!window) {
    return NS_ERROR_FAILURE;
  }
  window.forget(aRequestingWindow);

  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::GetElement(dom::Element** aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::GetIsHandlingUserInput(bool* aIsHandlingUserInput)
{
  NS_ENSURE_ARG_POINTER(aIsHandlingUserInput);
  *aIsHandlingUserInput = false;
  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::Cancel()
{
  MOZ_ASSERT(mRequestDispatched);
  PLAY_REQUEST_LOG("AutoplayRequest %p Cancel()", this);
  mRequestDispatched = false;
  mPromiseHolder.RejectIfExists(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(mRequestDispatched);
  PLAY_REQUEST_LOG("AutoplayRequest %p Allow()", this);
  mRequestDispatched = false;
  mPromiseHolder.ResolveIfExists(true, __func__);
  return NS_OK;
}

NS_IMETHODIMP
AutoplayRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);

  return NS_OK;
}

RefPtr<GenericPromise>
AutoplayRequest::RequestWithPrompt()
{
  // If we've already requested permission, we'll just return the promise,
  // as we don't want to show multiple permission requests at once.
  // The promise is non-exclusive, so if the request has already completed,
  // the ThenValue will run immediately.
  if (mRequestDispatched) {
    PLAY_REQUEST_LOG(
      "AutoplayRequest %p RequestWithPrompt() request already dispatched",
      this);
    return mPromiseHolder.Ensure(__func__);
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  if (!window) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                           __func__);
  }
  nsCOMPtr<nsIContentPermissionRequest> request = do_QueryInterface(this);
  MOZ_RELEASE_ASSERT(request);
  nsCOMPtr<nsIRunnable> f = NS_NewRunnableFunction(
    "AutoplayRequest::RequestWithPrompt", [window, request]() {
      dom::nsContentPermissionUtils::AskPermission(request, window);
    });
  mMainThreadTarget->Dispatch(f, NS_DISPATCH_NORMAL);

  mRequestDispatched = true;
  return mPromiseHolder.Ensure(__func__);
}

} // namespace mozilla
