/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_VIDEO_CAPTURE_FACTORY_H_
#define MOZILLA_VIDEO_CAPTURE_FACTORY_H_

#include "modules/video_capture/video_capture_factory.h"
#include "modules/video_capture/video_capture_options.h"
#include "modules/video_capture/video_capture.h"

#include "mozilla/MozPromise.h"

namespace mozilla::camera {
enum class CaptureDeviceType;
}

namespace mozilla {
/**
 *  NOTE: This class must be accessed only on a single SerialEventTarget
 */
class VideoCaptureFactory : webrtc::VideoCaptureOptions::Callback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoCaptureFactory);

  enum CameraAvailability { Unknown, Available, NotAvailable };

  VideoCaptureFactory();

  std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo> CreateDeviceInfo(
      int32_t aId, mozilla::camera::CaptureDeviceType aType);

  rtc::scoped_refptr<webrtc::VideoCaptureModule> CreateVideoCapture(
      int32_t aModuleId, const char* aUniqueId,
      mozilla::camera::CaptureDeviceType aType);

  using CameraBackendInitPromise = MozPromise<nsresult, nsresult, false>;
  /**
   * Request to initialize webrtc::VideoCaptureOptions
   *
   * Resolves with NS_OK when VideoCaptureOptions has been properly initialized
   * or rejects with one of the possible errors. Since this is only now
   * supported by PipeWire, all the errors are PipeWire specific:
   *  1) NS_ERROR_NOT_AVAILABLE - PipeWire libraries are not available on
   *     the system
   *  2) NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR - camera access has been rejected
   *  3) NS_ERROR_FAILURE - generic error, usually a PipeWire failure
   */
  RefPtr<CameraBackendInitPromise> InitCameraBackend();

  /**
   * Updates information about camera availability
   */
  using UpdateCameraAvailabilityPromise =
      MozPromise<CameraAvailability, nsresult, true>;
  RefPtr<UpdateCameraAvailabilityPromise> UpdateCameraAvailability();

 private:
  ~VideoCaptureFactory() = default;
  // aka OnCameraBackendInitialized
  // this method override has to follow webrtc::VideoCaptureOptions::Callback
  void OnInitialized(webrtc::VideoCaptureOptions::Status status) override;

  /**
   * Resolves with true or false depending on whether there is a camera device
   * advertised by the xdg-desktop-portal (Camera portal). Rejects with one
   * of the following errors:
   *  1) NS_ERROR_NOT_IMPLEMENTED - support for the Camera portal is not
   *     implemented or enabled
   *  2) NS_ERROR_NO_INTERFACE - the camera portal is not available
   *  3) NS_ERROR_UNEXPECTED - the camera portal returned wrong value
   */
  using HasCameraDevicePromise = MozPromise<CameraAvailability, nsresult, true>;
  RefPtr<HasCameraDevicePromise> HasCameraDevice();

  std::atomic<bool> mCameraBackendInitialized = false;
  CameraAvailability mCameraAvailability = Unknown;
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
  std::unique_ptr<webrtc::VideoCaptureOptions> mVideoCaptureOptions;
#endif
  MozPromiseHolder<CameraBackendInitPromise> mPromiseHolder;
  RefPtr<CameraBackendInitPromise> mPromise;
};

}  // namespace mozilla

#endif  // MOZILLA_VIDEO_CAPTURE_FACTORY_H_
