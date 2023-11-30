/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "video_capture_factory.h"

#include "mozilla/StaticPrefs_media.h"
#include "desktop_capture_impl.h"
#include "VideoEngine.h"

#include <memory>

namespace mozilla {

VideoCaptureFactory::VideoCaptureFactory() : mCameraBackendInitialized(false) {
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
  mVideoCaptureOptions = std::make_unique<webrtc::VideoCaptureOptions>();
  // In case pipewire is enabled, this acts as a fallback and can be always
  // enabled.
  mVideoCaptureOptions->set_allow_v4l2(true);
  bool allowPipeWire = false;
#  if defined(WEBRTC_USE_PIPEWIRE)
  allowPipeWire =
      mozilla::StaticPrefs::media_webrtc_camera_allow_pipewire_AtStartup();
  mVideoCaptureOptions->set_allow_pipewire(allowPipeWire);
#  endif
  if (!allowPipeWire) {
    // V4L2 backend can and should be initialized right away since there are no
    // permissions involved
    InitCameraBackend();
  }
#endif
}

std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo>
VideoCaptureFactory::CreateDeviceInfo(
    int32_t aId, mozilla::camera::CaptureDeviceType aType) {
  if (aType == mozilla::camera::CaptureDeviceType::Camera) {
    std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo> deviceInfo;
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
    deviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo(
        mVideoCaptureOptions.get()));
#else
    deviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo());
#endif
    return deviceInfo;
  }

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  MOZ_ASSERT("CreateDeviceInfo NO DESKTOP CAPTURE IMPL ON ANDROID" == nullptr);
  return nullptr;
#else
  return webrtc::DesktopCaptureImpl::CreateDeviceInfo(aId, aType);
#endif
}

rtc::scoped_refptr<webrtc::VideoCaptureModule>
VideoCaptureFactory::CreateVideoCapture(
    int32_t aModuleId, const char* aUniqueId,
    mozilla::camera::CaptureDeviceType aType) {
  if (aType == mozilla::camera::CaptureDeviceType::Camera) {
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
    return webrtc::VideoCaptureFactory::Create(mVideoCaptureOptions.get(),
                                               aUniqueId);
#else
    return webrtc::VideoCaptureFactory::Create(aUniqueId);
#endif
  }

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  MOZ_ASSERT("CreateVideoCapture NO DESKTOP CAPTURE IMPL ON ANDROID" ==
             nullptr);
  return nullptr;
#else
  return rtc::scoped_refptr<webrtc::VideoCaptureModule>(
      webrtc::DesktopCaptureImpl::Create(aModuleId, aUniqueId, aType));
#endif
}

auto VideoCaptureFactory::InitCameraBackend()
    -> RefPtr<CameraBackendInitPromise> {
  if (!mPromise) {
    mPromise = mPromiseHolder.Ensure(__func__);
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
    MOZ_ASSERT(mVideoCaptureOptions);
    mVideoCaptureOptions->Init(this);
#else
    mCameraBackendInitialized = true;
    mPromiseHolder.Resolve(NS_OK,
                           "VideoCaptureFactory::InitCameraBackend Resolve");
#endif
  }

  return mPromise;
}

void VideoCaptureFactory::OnInitialized(
    webrtc::VideoCaptureOptions::Status status) {
  switch (status) {
    case webrtc::VideoCaptureOptions::Status::SUCCESS:
      mCameraBackendInitialized = true;
      mPromiseHolder.Resolve(NS_OK, __func__);
      return;
    case webrtc::VideoCaptureOptions::Status::UNAVAILABLE:
      mPromiseHolder.Reject(NS_ERROR_NOT_AVAILABLE, __func__);
      return;
    case webrtc::VideoCaptureOptions::Status::DENIED:
      mPromiseHolder.Reject(NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR, __func__);
      return;
    default:
      mPromiseHolder.Reject(NS_ERROR_FAILURE, __func__);
      return;
  }
}

}  // namespace mozilla
