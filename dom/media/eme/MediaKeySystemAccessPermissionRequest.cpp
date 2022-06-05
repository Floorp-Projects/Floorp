/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeySystemAccessPermissionRequest.h"

#include "nsGlobalWindowInner.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaKeySystemAccessPermissionRequest,
                                   ContentPermissionRequestBase)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    MediaKeySystemAccessPermissionRequest, ContentPermissionRequestBase)

/* static */
already_AddRefed<MediaKeySystemAccessPermissionRequest>
MediaKeySystemAccessPermissionRequest::Create(nsPIDOMWindowInner* aWindow) {
  // Could conceivably be created off main thread then used on main thread
  // later. If we really need to do that at some point we could relax this
  // assert.
  AssertIsOnMainThread();
  if (!aWindow) {
    return nullptr;
  }

  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(aWindow);
  if (!innerWindow->GetPrincipal()) {
    return nullptr;
  }

  RefPtr<MediaKeySystemAccessPermissionRequest> request =
      new MediaKeySystemAccessPermissionRequest(innerWindow);
  return request.forget();
}

MediaKeySystemAccessPermissionRequest::MediaKeySystemAccessPermissionRequest(
    nsGlobalWindowInner* aWindow)
    : ContentPermissionRequestBase(aWindow->GetPrincipal(), aWindow,
                                   "media.eme.require-app-approval"_ns,
                                   "media-key-system-access"_ns) {}

MediaKeySystemAccessPermissionRequest::
    ~MediaKeySystemAccessPermissionRequest() {
  AssertIsOnMainThread();
  // If a request has not been serviced by the time it is destroyed, treat it
  // as if the request was denied.
  Cancel();
}

already_AddRefed<MediaKeySystemAccessPermissionRequest::RequestPromise>
MediaKeySystemAccessPermissionRequest::GetPromise() {
  return mPromiseHolder.Ensure(__func__);
}

nsresult MediaKeySystemAccessPermissionRequest::Start() {
  // Check test prefs to see if we should short circuit. We want to do this
  // before checking the cached value so we can have pref changes take effect
  // without refreshing the page.
  MediaKeySystemAccessPermissionRequest::PromptResult promptResult =
      CheckPromptPrefs();
  if (promptResult ==
      MediaKeySystemAccessPermissionRequest::PromptResult::Granted) {
    return Allow(JS::UndefinedHandleValue);
  }
  if (promptResult ==
      MediaKeySystemAccessPermissionRequest::PromptResult::Denied) {
    return Cancel();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMETHODIMP
MediaKeySystemAccessPermissionRequest::Allow(JS::Handle<JS::Value> aChoices) {
  AssertIsOnMainThread();
  mPromiseHolder.ResolveIfExists(true, __func__);
  return NS_OK;
}

NS_IMETHODIMP
MediaKeySystemAccessPermissionRequest::Cancel() {
  AssertIsOnMainThread();
  mPromiseHolder.RejectIfExists(false, __func__);
  return NS_OK;
}

}  // namespace mozilla::dom
