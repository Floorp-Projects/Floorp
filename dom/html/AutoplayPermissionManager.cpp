/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AutoplayPermissionManager.h"
#include "mozilla/AutoplayPermissionRequest.h"

#include "nsGlobalWindowInner.h"
#include "nsISupportsImpl.h"
#include "mozilla/Logging.h"
#include "nsContentPermissionHelper.h"

extern mozilla::LazyLogModule gMediaElementLog;

#define PLAY_REQUEST_LOG(msg, ...)                                             \
  MOZ_LOG(gMediaElementLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {

RefPtr<GenericPromise>
AutoplayPermissionManager::RequestWithPrompt()
{
  // If we've already requested permission, we'll just return the promise,
  // as we don't want to show multiple permission requests at once.
  // The promise is non-exclusive, so if the request has already completed,
  // the ThenValue will run immediately.
  if (mRequestDispatched) {
    PLAY_REQUEST_LOG("AutoplayPermissionManager %p RequestWithPrompt() request "
                     "already dispatched",
                     this);
    return mPromiseHolder.Ensure(__func__);
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
  if (!window) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                           __func__);
  }

  nsCOMPtr<nsIContentPermissionRequest> request =
    AutoplayPermissionRequest::Create(nsGlobalWindowInner::Cast(window), this);
  if (!request) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR,
                                           __func__);
  }

  // Dispatch the request.
  nsCOMPtr<nsIRunnable> f = NS_NewRunnableFunction(
    "AutoplayPermissionManager::RequestWithPrompt", [window, request]() {
      dom::nsContentPermissionUtils::AskPermission(request, window);
    });
  window->EventTargetFor(TaskCategory::Other)->Dispatch(f, NS_DISPATCH_NORMAL);

  mRequestDispatched = true;
  return mPromiseHolder.Ensure(__func__);
}

AutoplayPermissionManager::AutoplayPermissionManager(
  nsGlobalWindowInner* aWindow)
  : mWindow(do_GetWeakReference(aWindow))
{
  PLAY_REQUEST_LOG("AutoplayPermissionManager %p Create()", this);
}

AutoplayPermissionManager::~AutoplayPermissionManager()
{
  // If we made a request, it should have been resolved.
  MOZ_ASSERT(!mRequestDispatched);
}

void
AutoplayPermissionManager::DenyPlayRequest()
{
  PLAY_REQUEST_LOG("AutoplayPermissionManager %p DenyPlayRequest()", this);
  mRequestDispatched = false;
  mPromiseHolder.RejectIfExists(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
}

void
AutoplayPermissionManager::ApprovePlayRequest()
{
  PLAY_REQUEST_LOG("AutoplayPermissionManager %p ApprovePlayRequest()", this);
  mRequestDispatched = false;
  mPromiseHolder.ResolveIfExists(true, __func__);
}

} // namespace mozilla
