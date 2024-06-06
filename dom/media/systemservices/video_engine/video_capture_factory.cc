/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "video_capture_factory.h"

#include "mozilla/StaticPrefs_media.h"
#include "desktop_capture_impl.h"
#include "VideoEngine.h"

#if defined(WEBRTC_USE_PIPEWIRE)
#  include "video_engine/placeholder_device_info.h"
#endif

#if defined(WEBRTC_USE_PIPEWIRE) && defined(MOZ_ENABLE_DBUS)
#  include "mozilla/widget/AsyncDBus.h"
#endif

#include <memory>

namespace mozilla {

VideoCaptureFactory::VideoCaptureFactory() {
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
#  if defined(WEBRTC_USE_PIPEWIRE)
    // Special case when PipeWire is not initialized yet and we need to insert
    // a camera device placeholder based on camera device availability we get
    // from the camera portal
    if (!mCameraBackendInitialized && mVideoCaptureOptions->allow_pipewire()) {
      MOZ_ASSERT(mCameraAvailability != Unknown);
      deviceInfo.reset(
          new PlaceholderDeviceInfo(mCameraAvailability == Available));
      return deviceInfo;
    }
#  endif

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

VideoCaptureFactory::CreateVideoCaptureResult
VideoCaptureFactory::CreateVideoCapture(
    int32_t aModuleId, const char* aUniqueId,
    mozilla::camera::CaptureDeviceType aType) {
  CreateVideoCaptureResult result;
  if (aType == mozilla::camera::CaptureDeviceType::Camera) {
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
    result.mCapturer = webrtc::VideoCaptureFactory::Create(
        mVideoCaptureOptions.get(), aUniqueId);
#else
    result.mCapturer = webrtc::VideoCaptureFactory::Create(aUniqueId);
#endif
    return result;
  }

#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  MOZ_ASSERT("CreateVideoCapture NO DESKTOP CAPTURE IMPL ON ANDROID" ==
             nullptr);
#else
  result.mDesktopImpl =
      webrtc::DesktopCaptureImpl::Create(aModuleId, aUniqueId, aType);
  result.mCapturer =
      rtc::scoped_refptr<webrtc::VideoCaptureModule>(result.mDesktopImpl);
#endif

  return result;
}

auto VideoCaptureFactory::InitCameraBackend()
    -> RefPtr<CameraBackendInitPromise> {
  if (!mPromise) {
    mPromise = mPromiseHolder.Ensure(__func__);
#if (defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)) && !defined(WEBRTC_ANDROID)
    MOZ_ASSERT(mVideoCaptureOptions);
    mVideoCaptureOptions->Init(this);
#  if defined(WEBRTC_USE_PIPEWIRE)
    mPromise = mPromise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [this, self = RefPtr(this)](
            const CameraBackendInitPromise::ResolveOrRejectValue& aValue) {
          if (aValue.IsReject() &&
              aValue.RejectValue() != NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR) {
            // Fallback to V4L2 in case of PipeWire or camera portal failure.
            // There is nothing we need to do in order to initialize V4L2 so
            // consider the backend initialized and ready to be used.
            mVideoCaptureOptions->set_allow_pipewire(false);
            mCameraBackendInitialized = true;

            return CameraBackendInitPromise::CreateAndResolve(
                NS_OK,
                "VideoCaptureFactory::InitCameraBackend Resolve with "
                "fallback to V4L2");
          }

          return CameraBackendInitPromise::CreateAndResolveOrReject(
              aValue,
              "VideoCaptureFactory::InitCameraBackend Resolve or Reject");
        });
#  endif
#else
    mCameraBackendInitialized = true;
    mPromiseHolder.Resolve(NS_OK,
                           "VideoCaptureFactory::InitCameraBackend Resolve");
#endif
  }

  return mPromise;
}

auto VideoCaptureFactory::HasCameraDevice()
    -> RefPtr<VideoCaptureFactory::HasCameraDevicePromise> {
#if defined(WEBRTC_USE_PIPEWIRE) && defined(MOZ_ENABLE_DBUS)
  if (mVideoCaptureOptions && mVideoCaptureOptions->allow_pipewire()) {
    return widget::CreateDBusProxyForBus(
               G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
               /* aInterfaceInfo = */ nullptr, "org.freedesktop.portal.Desktop",
               "/org/freedesktop/portal/desktop",
               "org.freedesktop.portal.Camera")
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [](RefPtr<GDBusProxy>&& aProxy) {
              GVariant* variant =
                  g_dbus_proxy_get_cached_property(aProxy, "IsCameraPresent");
              if (!variant) {
                return HasCameraDevicePromise::CreateAndReject(
                    NS_ERROR_NO_INTERFACE,
                    "VideoCaptureFactory::HasCameraDevice Reject");
              }

              if (!g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
                return HasCameraDevicePromise::CreateAndReject(
                    NS_ERROR_UNEXPECTED,
                    "VideoCaptureFactory::HasCameraDevice Reject");
              }

              const bool hasCamera = g_variant_get_boolean(variant);
              g_variant_unref(variant);
              return HasCameraDevicePromise::CreateAndResolve(
                  hasCamera ? Available : NotAvailable,
                  "VideoCaptureFactory::HasCameraDevice Resolve");
            },
            [](GUniquePtr<GError>&& aError) {
              return HasCameraDevicePromise::CreateAndReject(
                  NS_ERROR_NO_INTERFACE,
                  "VideoCaptureFactory::HasCameraDevice Reject");
            });
  }
#endif
  return HasCameraDevicePromise::CreateAndReject(
      NS_ERROR_NOT_IMPLEMENTED, "VideoCaptureFactory::HasCameraDevice Reject");
}

auto VideoCaptureFactory::UpdateCameraAvailability()
    -> RefPtr<UpdateCameraAvailabilityPromise> {
  return VideoCaptureFactory::HasCameraDevice()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [this, self = RefPtr(this)](
          const HasCameraDevicePromise::ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve()) {
          mCameraAvailability = aValue.ResolveValue();

          return HasCameraDevicePromise::CreateAndResolve(
              mCameraAvailability,
              "VideoCaptureFactory::UpdateCameraAvailability Resolve");
        }

        // We want to fallback to V4L2 at this point, therefore make sure a
        // camera device is announced so we can proceed with a gUM request,
        // where we can fallback to V4L2 backend.
        mCameraAvailability = Available;

        return HasCameraDevicePromise::CreateAndReject(
            aValue.RejectValue(),
            "VideoCaptureFactory::UpdateCameraAvailability Reject");
      });
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
