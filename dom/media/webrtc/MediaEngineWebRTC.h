/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "AudioDeviceInfo.h"
#include "CamerasChild.h"
#include "CubebUtils.h"
#include "DOMMediaStream.h"
#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "MediaEngineSource.h"
#include "MediaEngineWrapper.h"
#include "MediaStreamGraph.h"
#include "NullTransport.h"
#include "StreamTracks.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "CubebDeviceEnumerator.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "prcvar.h"
#include "prthread.h"

// WebRTC library includes follow
// Video Engine
// conflicts with #include of scoped_ptr.h
#undef FF
#include "webrtc/modules/video_capture/video_capture_defines.h"

namespace mozilla {

class MediaEngineWebRTC : public MediaEngine {
  typedef MediaEngine Super;

 public:
  explicit MediaEngineWebRTC(MediaEnginePrefs& aPrefs);

  virtual void SetFakeDeviceChangeEvents() override;

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown() override;

  // Returns whether the host supports duplex audio stream.
  bool SupportsDuplex();

  void EnumerateDevices(uint64_t aWindowId, dom::MediaSourceEnum, MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;

 private:
  ~MediaEngineWebRTC() = default;
  void EnumerateVideoDevices(uint64_t aWindowId,
                             camera::CaptureEngine aCapEngine,
                             nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateMicrophoneDevices(uint64_t aWindowId,
                                  nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateSpeakerDevices(uint64_t aWindowId,
                               nsTArray<RefPtr<MediaDevice>>*);

  // gUM runnables can e.g. Enumerate from multiple threads
  Mutex mMutex;
  RefPtr<mozilla::CubebDeviceEnumerator> mEnumerator;
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  // This also is set in the ctor and then never changed, but we can't make it
  // const because we pass it to a function that takes bool* in the ctor.
  bool mHasTabVideoSource;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEWEBRTC_H_ */
