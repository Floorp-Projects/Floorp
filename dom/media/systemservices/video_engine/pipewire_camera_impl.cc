/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "pipewire_camera_impl.h"

namespace mozilla {

using namespace webrtc;

auto CameraPortalImpl::Start() -> RefPtr<CameraPortalPromise> {
  MOZ_ASSERT(!mPortal);
  mPortal = std::make_unique<CameraPortal>(this);
  mPortal->Start();

  return mPromiseHolder.Ensure(__func__);
}

void CameraPortalImpl::OnCameraRequestResult(xdg_portal::RequestResponse result,
                                             int fd) {
  MOZ_ASSERT(NS_IsMainThread());
  if (result == xdg_portal::RequestResponse::kSuccess) {
    mPromiseHolder.Resolve(fd, __func__);
  } else if (result == xdg_portal::RequestResponse::kUserCancelled) {
    mPromiseHolder.Reject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
  } else {
    mPromiseHolder.Reject(NS_ERROR_FAILURE, __func__);
  }
}

VideoCaptureOptionsImpl::VideoCaptureOptionsImpl()
    : mCaptureOptions(std::make_unique<VideoCaptureOptions>()) {
  mCaptureOptions->set_allow_pipewire(true);
}

auto VideoCaptureOptionsImpl::Init(int fd)
    -> RefPtr<VideoCaptureOptionsInitPromise> {
  MOZ_ASSERT(mCaptureOptions);

  RefPtr<VideoCaptureOptionsInitPromise> promise =
      mPromiseHolder.Ensure(__func__);

  mCaptureOptions->set_pipewire_fd(fd);
  mCaptureOptions->Init(this);

  return promise;
}

std::unique_ptr<VideoCaptureOptions> VideoCaptureOptionsImpl::ReleaseOptions() {
  return std::move(mCaptureOptions);
}

void VideoCaptureOptionsImpl::OnInitialized(
    VideoCaptureOptions::Status status) {
  switch (status) {
    case VideoCaptureOptions::Status::SUCCESS:
      mPromiseHolder.Resolve(NS_OK, __func__);
      return;
    case VideoCaptureOptions::Status::UNAVAILABLE:
      mPromiseHolder.Reject(NS_ERROR_NOT_AVAILABLE, __func__);
      return;
    case VideoCaptureOptions::Status::DENIED:
      mPromiseHolder.Reject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
      return;
    default:
      mPromiseHolder.Reject(NS_ERROR_FAILURE, __func__);
      return;
  }
}

}  // namespace mozilla
