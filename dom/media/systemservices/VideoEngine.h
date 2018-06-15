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
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/modules/video_capture/video_capture_defines.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/video_engine/desktop_capture_impl.h"
#include <memory>
#include <functional>

namespace mozilla {
namespace camera {

// Historically the video engine was part of webrtc
// it was removed (and reimplemented in Talk)
class VideoEngine
{
private:
  virtual ~VideoEngine (){};
  // Base cache expiration period
  // Note because cameras use HW plug event detection, this
  // only applies to screen based modes.
  static const int64_t kCacheExpiryPeriodMs = 2000;

public:
  VideoEngine() : mId(0) {};
  NS_INLINE_DECL_REFCOUNTING(VideoEngine)

  static already_AddRefed<VideoEngine> Create(UniquePtr<const webrtc::Config>&& aConfig);
#if defined(ANDROID)
  static int SetAndroidObjects();
#endif
  void CreateVideoCapture(int32_t& id, const char* deviceUniqueIdUTF8);

  int ReleaseVideoCapture(const int32_t id);

  // VideoEngine is responsible for any cleanup in its modules
  static void Delete(VideoEngine * engine) { }

  /** Returns an existing or creates a new new DeviceInfo.
  *   Camera info is cached to prevent repeated lengthy polling for "realness"
  *   of the hardware devices.  Other types of capture, e.g. screen share info,
  *   are cached for 1 second. This could be handled in a more elegant way in
  *   the future.
  *   @return on failure the shared_ptr will be null, otherwise it will contain
  *   a DeviceInfo.
  *   @see bug 1305212 https://bugzilla.mozilla.org/show_bug.cgi?id=1305212
  */
  std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo> GetOrCreateVideoCaptureDeviceInfo();

  const UniquePtr<const webrtc::Config>& GetConfiguration();

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
  bool WithEntry(const int32_t entryCapnum, const std::function<void(CaptureEntry &entry)>&& fn);

private:
  explicit VideoEngine(UniquePtr<const webrtc::Config>&& aConfig);
  int32_t mId;
  webrtc::CaptureDeviceInfo mCaptureDevInfo;
  std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo> mDeviceInfo;
  UniquePtr<const webrtc::Config> mConfig;
  std::map<int32_t, CaptureEntry> mCaps;
  std::map<int32_t, int32_t> mIdMap;
  // The validity period for non-camera capture device infos`
  int64_t mExpiryTimeInMs = 0;
  int32_t GenerateId();
  static int32_t sId;
};
}
}
#endif
