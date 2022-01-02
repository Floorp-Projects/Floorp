/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSPermissionRequestBase.h"

#include "mozilla/dom/Promise.h"

using namespace mozilla;

using mozilla::dom::Promise;

NS_IMPL_ISUPPORTS(nsOSPermissionRequestBase, nsIOSPermissionRequest,
                  nsISupportsWeakReference)

NS_IMETHODIMP
nsOSPermissionRequestBase::GetMediaCapturePermissionState(
    uint16_t* aCamera, uint16_t* aMicrophone) {
  nsresult rv = GetVideoCapturePermissionState(aCamera);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return GetAudioCapturePermissionState(aMicrophone);
}

NS_IMETHODIMP
nsOSPermissionRequestBase::GetAudioCapturePermissionState(uint16_t* aAudio) {
  MOZ_ASSERT(aAudio);
  *aAudio = PERMISSION_STATE_AUTHORIZED;
  return NS_OK;
}

NS_IMETHODIMP
nsOSPermissionRequestBase::GetVideoCapturePermissionState(uint16_t* aVideo) {
  MOZ_ASSERT(aVideo);
  *aVideo = PERMISSION_STATE_AUTHORIZED;
  return NS_OK;
}

NS_IMETHODIMP
nsOSPermissionRequestBase::GetScreenCapturePermissionState(uint16_t* aScreen) {
  MOZ_ASSERT(aScreen);
  *aScreen = PERMISSION_STATE_AUTHORIZED;
  return NS_OK;
}

nsresult nsOSPermissionRequestBase::GetPromise(JSContext* aCx,
                                               RefPtr<Promise>& aPromiseOut) {
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!globalObject)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  aPromiseOut = Promise::Create(globalObject, result);
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsOSPermissionRequestBase::RequestVideoCapturePermission(
    JSContext* aCx, Promise** aPromiseOut) {
  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  promiseHandle->MaybeResolve(true /* access authorized */);
  promiseHandle.forget(aPromiseOut);
  return NS_OK;
}

NS_IMETHODIMP
nsOSPermissionRequestBase::RequestAudioCapturePermission(
    JSContext* aCx, Promise** aPromiseOut) {
  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  promiseHandle->MaybeResolve(true /* access authorized */);
  promiseHandle.forget(aPromiseOut);
  return NS_OK;
}

NS_IMETHODIMP
nsOSPermissionRequestBase::MaybeRequestScreenCapturePermission() {
  return NS_OK;
}
