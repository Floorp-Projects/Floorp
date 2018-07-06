/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoplayPermissionRequest.h"
#include "mozilla/AutoplayPermissionManager.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gMediaElementLog;

#define PLAY_REQUEST_LOG(msg, ...)                                             \
  MOZ_LOG(gMediaElementLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {

NS_IMPL_ISUPPORTS(AutoplayPermissionRequest, nsIContentPermissionRequest)

AutoplayPermissionRequest::AutoplayPermissionRequest(
  AutoplayPermissionManager* aManager,
  nsGlobalWindowInner* aWindow,
  nsIPrincipal* aNodePrincipal,
  nsIEventTarget* aMainThreadTarget)
  : mManager(aManager)
  , mWindow(do_GetWeakReference(aWindow))
  , mNodePrincipal(aNodePrincipal)
  , mMainThreadTarget(aMainThreadTarget)
  , mRequester(new dom::nsContentPermissionRequester(aWindow))
{
  MOZ_ASSERT(mNodePrincipal);
}

AutoplayPermissionRequest::~AutoplayPermissionRequest()
{
  Cancel();
}

NS_IMETHODIMP
AutoplayPermissionRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);

  nsCOMPtr<nsIPrincipal> principal = mNodePrincipal;
  principal.forget(aRequestingPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::GetTypes(nsIArray** aTypes)
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
AutoplayPermissionRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
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
AutoplayPermissionRequest::GetElement(dom::Element** aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::GetIsHandlingUserInput(bool* aIsHandlingUserInput)
{
  NS_ENSURE_ARG_POINTER(aIsHandlingUserInput);
  *aIsHandlingUserInput = false;
  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::Cancel()
{
  if (mManager) {
    mManager->DenyPlayRequest();
  }
  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::Allow(JS::HandleValue aChoices)
{
  if (mManager) {
    mManager->ApprovePlayRequest();
  }
  return NS_OK;
}

NS_IMETHODIMP
AutoplayPermissionRequest::GetRequester(
  nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);

  return NS_OK;
}

already_AddRefed<AutoplayPermissionRequest>
AutoplayPermissionRequest::Create(nsGlobalWindowInner* aWindow,
                                  AutoplayPermissionManager* aManager)
{
  if (!aWindow || !aWindow->GetPrincipal() ||
      !aWindow->EventTargetFor(TaskCategory::Other)) {
    return nullptr;
  }
  RefPtr<AutoplayPermissionRequest> request =
    new AutoplayPermissionRequest(aManager,
                                  aWindow,
                                  aWindow->GetPrincipal(),
                                  aWindow->EventTargetFor(TaskCategory::Other));
  PLAY_REQUEST_LOG("AutoplayPermissionRequest %p Create()", request.get());
  return request.forget();
}

} // namespace mozilla
