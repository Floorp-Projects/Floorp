/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_
#define MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_

#include "prcvar.h"
#include "prthread.h"
#include "nsIThread.h"
#include "nsIRunnable.h"

#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "DOMMediaStream.h"
#include "nsDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"

#include "VideoUtils.h"
#include "MediaEngineCameraVideoSource.h"
#include "VideoSegment.h"
#include "AudioSegment.h"
#include "StreamBuffer.h"
#include "MediaStreamGraph.h"

#include "MediaEngineWrapper.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"

// WebRTC library includes follow
#include "webrtc/common.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "CamerasChild.h"

#include "NullTransport.h"

namespace webrtc {
class I420VideoFrame;
}

namespace mozilla {

/**
 * The WebRTC implementation of the MediaEngine interface.
 */
class MediaEngineRemoteVideoSource : public MediaEngineCameraVideoSource,
                                     public webrtc::ExternalRenderer
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // ExternalRenderer
  int FrameSizeChange(unsigned int w, unsigned int h,
                      unsigned int streams) override;
  int DeliverFrame(unsigned char* buffer,
                   size_t size,
                   uint32_t time_stamp,
                   int64_t ntp_time,
                   int64_t render_time,
                   void *handle) override;
  // XXX!!!! FIX THIS
  int DeliverI420Frame(const webrtc::I420VideoFrame& webrtc_frame) override { return 0; };
  bool IsTextureSupported() override { return false; };

  // MediaEngineCameraVideoSource
  MediaEngineRemoteVideoSource(int aIndex, mozilla::camera::CaptureEngine aCapEngine,
                               dom::MediaSourceEnum aMediaSource,
                               const char* aMonitorName = "RemoteVideo.Monitor");

  nsresult Allocate(const dom::MediaTrackConstraints& aConstraints,
                    const MediaEnginePrefs& aPrefs,
                    const nsString& aDeviceId) override;
  nsresult Deallocate() override;;
  nsresult Start(SourceMediaStream*, TrackID) override;
  nsresult Stop(SourceMediaStream*, TrackID) override;
  nsresult Restart(const dom::MediaTrackConstraints& aConstraints,
                   const MediaEnginePrefs &aPrefs,
                   const nsString& aDeviceId) override;
  void NotifyPull(MediaStreamGraph* aGraph,
                  SourceMediaStream* aSource,
                  TrackID aId,
                  StreamTime aDesiredTime) override;
  dom::MediaSourceEnum GetMediaSource() const override {
    return mMediaSource;
  }

  bool ChooseCapability(const dom::MediaTrackConstraints &aConstraints,
                        const MediaEnginePrefs &aPrefs,
                        const nsString& aDeviceId) override;

  void Refresh(int aIndex);

  void Shutdown() override;

protected:
  ~MediaEngineRemoteVideoSource() { Shutdown(); }

private:
  // Initialize the needed Video engine interfaces.
  void Init();
  size_t NumCapabilities() override;
  void GetCapability(size_t aIndex, webrtc::CaptureCapability& aOut) override;

  dom::MediaSourceEnum mMediaSource; // source of media (camera | application | screen)
  mozilla::camera::CaptureEngine mCapEngine;
};

}

#endif /* MEDIAENGINE_REMOTE_VIDEO_SOURCE_H_ */
