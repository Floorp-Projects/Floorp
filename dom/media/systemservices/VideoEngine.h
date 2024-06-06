/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_VideoEngine_h
#define mozilla_VideoEngine_h

#include "MediaEngine.h"
#include "VideoFrameUtils.h"
#include "mozilla/media/MediaUtils.h"
#include "modules/video_capture/video_capture.h"
#include <memory>
#include <functional>

namespace mozilla {
class VideoCaptureFactory;
}

namespace mozilla::camera {

enum class CaptureDeviceType { Camera, Screen, Window, Browser };

struct CaptureDeviceInfo {
  CaptureDeviceType type;

  CaptureDeviceInfo() : type(CaptureDeviceType::Camera) {}
  explicit CaptureDeviceInfo(CaptureDeviceType t) : type(t) {}

  const char* TypeName() const {
    switch (type) {
      case CaptureDeviceType::Camera: {
        return "Camera";
      }
      case CaptureDeviceType::Screen: {
        return "Screen";
      }
      case CaptureDeviceType::Window: {
        return "Window";
      }
      case CaptureDeviceType::Browser: {
        return "Browser";
      }
    }
    assert(false);
    return "UNKOWN-CaptureDeviceType!";
  }
};

// Historically the video engine was part of webrtc
// it was removed (and reimplemented in Talk)
class VideoEngine {
 private:
  virtual ~VideoEngine();

  // Base cache expiration period
  // Note because cameras use HW plug event detection, this
  // only applies to screen based modes.
  static const int64_t kCacheExpiryPeriodMs = 2000;

 public:
  NS_INLINE_DECL_REFCOUNTING(VideoEngine)

  static already_AddRefed<VideoEngine> Create(
      const CaptureDeviceType& aCaptureDeviceType,
      RefPtr<VideoCaptureFactory> aVideoCaptureFactory);
#if defined(ANDROID)
  static int SetAndroidObjects();
#endif
  /** Returns a non-negative capture identifier or -1 on failure.
   */
  int32_t CreateVideoCapture(const char* aDeviceUniqueIdUTF8);

  int ReleaseVideoCapture(const int32_t aId);

  // VideoEngine is responsible for any cleanup in its modules
  static void Delete(VideoEngine* aEngine) {}

  /** Returns an existing or creates a new new DeviceInfo.
   *   Camera info is cached to prevent repeated lengthy polling for "realness"
   *   of the hardware devices.  Other types of capture, e.g. screen share info,
   *   are cached for 1 second. This could be handled in a more elegant way in
   *   the future.
   *   @return on failure the shared_ptr will be null, otherwise it will contain
   *   a DeviceInfo.
   *   @see bug 1305212 https://bugzilla.mozilla.org/show_bug.cgi?id=1305212
   */
  std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo>
  GetOrCreateVideoCaptureDeviceInfo();

  /**
   * Destroys existing DeviceInfo.
   *  The DeviceInfo will be recreated the next time it is needed.
   */
  void ClearVideoCaptureDeviceInfo();

  class CaptureEntry {
   public:
    CaptureEntry(int32_t aCapnum,
                 rtc::scoped_refptr<webrtc::VideoCaptureModule> aCapture);
    int32_t Capnum() const;
    rtc::scoped_refptr<webrtc::VideoCaptureModule> VideoCapture();

   private:
    int32_t mCapnum;
    rtc::scoped_refptr<webrtc::VideoCaptureModule> mVideoCaptureModule;
    friend class VideoEngine;
  };

  // Returns true iff an entry for capnum exists
  bool WithEntry(const int32_t entryCapnum,
                 const std::function<void(CaptureEntry& entry)>&& fn);

 private:
  VideoEngine(const CaptureDeviceType& aCaptureDeviceType,
              RefPtr<VideoCaptureFactory> aVideoCaptureFactory);
  int32_t mId;
  const CaptureDeviceInfo mCaptureDevInfo;
  RefPtr<VideoCaptureFactory> mVideoCaptureFactory;
  std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo> mDeviceInfo;
  std::map<int32_t, CaptureEntry> mCaps;
  std::map<int32_t, int32_t> mIdMap;
  // The validity period for non-camera capture device infos`
  webrtc::Timestamp mExpiryTime = webrtc::Timestamp::Micros(0);
  int32_t GenerateId();
};
}  // namespace mozilla::camera
#endif
