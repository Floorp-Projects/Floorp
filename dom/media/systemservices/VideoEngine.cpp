/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoEngine.h"
#include "webrtc/video_engine/browser_capture_impl.h"
#ifdef WEBRTC_ANDROID
#include "webrtc/modules/video_capture/video_capture.h"
#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
#include "webrtc/modules/video_render/video_render.h"
#endif
#endif


namespace mozilla {
namespace camera {

#undef LOG
#undef LOG_ENABLED
mozilla::LazyLogModule gVideoEngineLog("VideoEngine");
#define LOG(args) MOZ_LOG(gVideoEngineLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gVideoEngineLog, mozilla::LogLevel::Debug)

int VideoEngine::sId = 0;

#if defined(ANDROID)
int VideoEngine::SetAndroidObjects(JavaVM* javaVM) {
  LOG((__PRETTY_FUNCTION__));

  if (webrtc::SetCaptureAndroidVM(javaVM) != 0) {
    LOG(("Could not set capture Android VM"));
    return -1;
  }
#ifdef WEBRTC_INCLUDE_INTERNAL_VIDEO_RENDER
  if (webrtc::SetRenderAndroidVM(javaVM) != 0) {
    LOG(("Could not set render Android VM"));
    return -1;
  }
#endif
  return 0;
}
#endif

void
VideoEngine::CreateVideoCapture(int32_t& id, const char* deviceUniqueIdUTF8) {
  LOG((__PRETTY_FUNCTION__));
  id = GenerateId();
  LOG(("CaptureDeviceInfo.type=%s id=%d",mCaptureDevInfo.TypeName(),id));
  CaptureEntry entry = {-1,nullptr,nullptr};

  if (mCaptureDevInfo.type == webrtc::CaptureDeviceType::Camera) {
    entry = CaptureEntry(id,
		         webrtc::VideoCaptureFactory::Create(id, deviceUniqueIdUTF8),
                         nullptr);
  } else {
#ifndef WEBRTC_ANDROID
    entry = CaptureEntry(
	      id,
	      webrtc::DesktopCaptureImpl::Create(id, deviceUniqueIdUTF8, mCaptureDevInfo.type),
              nullptr);
#else
    MOZ_ASSERT("CreateVideoCapture NO DESKTOP CAPTURE IMPL ON ANDROID" == nullptr);
#endif
  }
  mCaps.emplace(id,std::move(entry));
}

int
VideoEngine::ReleaseVideoCapture(const int32_t id) {
  bool found = false;
  WithEntry(id, [&found](CaptureEntry& cap) {
         cap.mVideoCaptureModule = nullptr;
        found = true;
   });
  return found ? 0 : (-1);
}

std::shared_ptr<webrtc::VideoCaptureModule::DeviceInfo>
VideoEngine::GetOrCreateVideoCaptureDeviceInfo() {
  if (mDeviceInfo) {
    return mDeviceInfo;
  }
  switch (mCaptureDevInfo.type) {
    case webrtc::CaptureDeviceType::Camera: {
      mDeviceInfo.reset(webrtc::VideoCaptureFactory::CreateDeviceInfo(0));
      break;
    }
    case webrtc::CaptureDeviceType::Browser: {
      mDeviceInfo.reset(webrtc::BrowserDeviceInfoImpl::CreateDeviceInfo());
      break;
    }
    // Window, Application, and Screen types are handled by DesktopCapture
    case webrtc::CaptureDeviceType::Window:
    case webrtc::CaptureDeviceType::Application:
    case webrtc::CaptureDeviceType::Screen: {
#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
      mDeviceInfo.reset(webrtc::DesktopCaptureImpl::CreateDeviceInfo(mId,mCaptureDevInfo.type));
#else
      MOZ_ASSERT("GetVideoCaptureDeviceInfo NO DESKTOP CAPTURE IMPL ON ANDROID" == nullptr);
      mDeviceInfo.reset();
#endif
      break;
    }
  }
  return mDeviceInfo;
}

void
VideoEngine::RemoveRenderer(int capnum) {
  WithEntry(capnum, [](CaptureEntry& cap) {
    cap.mVideoRender = nullptr;
  });
}

const UniquePtr<const webrtc::Config>&
VideoEngine::GetConfiguration() {
  return mConfig;
}

RefPtr<VideoEngine> VideoEngine::Create(UniquePtr<const webrtc::Config>&& aConfig) {
  LOG((__PRETTY_FUNCTION__));
  LOG(("Creating new VideoEngine with CaptureDeviceType %s",
       aConfig->Get<webrtc::CaptureDeviceInfo>().TypeName()));
  RefPtr<VideoEngine> engine(new VideoEngine(std::move(aConfig)));
  return engine;
}

VideoEngine::CaptureEntry::CaptureEntry(int32_t aCapnum,
                                        rtc::scoped_refptr<webrtc::VideoCaptureModule> aCapture,
                                        webrtc::VideoRender * aRenderer):
    mCapnum(aCapnum),
    mVideoCaptureModule(aCapture),
    mVideoRender(aRenderer)
{}

rtc::scoped_refptr<webrtc::VideoCaptureModule>
VideoEngine::CaptureEntry::VideoCapture() {
  return mVideoCaptureModule;
}

const UniquePtr<webrtc::VideoRender>&
VideoEngine::CaptureEntry::VideoRenderer() {
  if (!mVideoRender) {
     MOZ_ASSERT(mCapnum != -1);
     // Create a VideoRender on demand
     mVideoRender = UniquePtr<webrtc::VideoRender>(
         webrtc::VideoRender::CreateVideoRender(mCapnum,nullptr,false,webrtc::kRenderExternal));
   }
  return mVideoRender;
}

int32_t
VideoEngine::CaptureEntry::Capnum() const {
  return mCapnum;
}

bool VideoEngine::WithEntry(const int32_t entryCapnum,
			    const std::function<void(CaptureEntry &entry)>&& fn) {
  auto it = mCaps.find(entryCapnum);
  if (it == mCaps.end()) {
    return false;
  }
  fn(it->second);
  return true;
}

int32_t
VideoEngine::GenerateId() {
  // XXX Something better than this (a map perhaps, or a simple boolean TArray, given
  // the number in-use is O(1) normally!)
  return mId = sId++;
}

VideoEngine::VideoEngine(UniquePtr<const webrtc::Config>&& aConfig):
  mCaptureDevInfo(aConfig->Get<webrtc::CaptureDeviceInfo>()),
  mDeviceInfo(nullptr),
  mConfig(std::move(aConfig))
{
  LOG((__PRETTY_FUNCTION__));
}

}
}
