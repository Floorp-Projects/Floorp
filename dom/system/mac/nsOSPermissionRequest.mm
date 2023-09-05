/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsOSPermissionRequest.h"

#include "mozilla/dom/Promise.h"
#include "nsCocoaUtils.h"

using namespace mozilla;

using mozilla::dom::Promise;

NS_IMETHODIMP
nsOSPermissionRequest::GetAudioCapturePermissionState(uint16_t* aAudio) {
  MOZ_ASSERT(aAudio);
  return nsCocoaUtils::GetAudioCapturePermissionState(*aAudio);
}

NS_IMETHODIMP
nsOSPermissionRequest::GetVideoCapturePermissionState(uint16_t* aVideo) {
  MOZ_ASSERT(aVideo);
  return nsCocoaUtils::GetVideoCapturePermissionState(*aVideo);
}

NS_IMETHODIMP
nsOSPermissionRequest::GetScreenCapturePermissionState(uint16_t* aScreen) {
  MOZ_ASSERT(aScreen);
  return nsCocoaUtils::GetScreenCapturePermissionState(*aScreen);
}

NS_IMETHODIMP
nsOSPermissionRequest::RequestVideoCapturePermission(JSContext* aCx,
                                                     Promise** aPromiseOut) {
  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsCocoaUtils::RequestVideoCapturePermission(promiseHandle);
  promiseHandle.forget(aPromiseOut);
  return rv;
}

NS_IMETHODIMP
nsOSPermissionRequest::RequestAudioCapturePermission(JSContext* aCx,
                                                     Promise** aPromiseOut) {
  RefPtr<Promise> promiseHandle;
  nsresult rv = GetPromise(aCx, promiseHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = nsCocoaUtils::RequestAudioCapturePermission(promiseHandle);
  promiseHandle.forget(aPromiseOut);
  return rv;
}

NS_IMETHODIMP
nsOSPermissionRequest::MaybeRequestScreenCapturePermission() {
  return nsCocoaUtils::MaybeRequestScreenCapturePermission();
}
