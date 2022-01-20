/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "AudioDeviceInfo.h"
#include "CubebUtils.h"
#include "DOMMediaStream.h"
#include "MediaEngine.h"
#include "MediaEnginePrefs.h"
#include "MediaEngineSource.h"
#include "MediaEngineWrapper.h"
#include "MediaTrackGraph.h"
#include "NullTransport.h"
#include "VideoSegment.h"
#include "VideoUtils.h"
#include "CubebDeviceEnumerator.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "prcvar.h"
#include "prthread.h"

// WebRTC library includes follow
#include "modules/video_capture/video_capture_defines.h"

namespace mozilla {

class MediaEngineWebRTC : public MediaEngine {
 public:
  MediaEngineWebRTC();

  // Enable periodic fake "devicechange" event. Must always be called from the
  // same thread, and must be disabled before shutdown.
  void SetFakeDeviceChangeEventsEnabled(bool aEnable) override;

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown() override;

  void EnumerateDevices(dom::MediaSourceEnum, MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  RefPtr<MediaEngineSource> CreateSource(const MediaDevice* aDevice) override;

  MediaEventSource<void>& DeviceListChangeEvent() override {
    return mDeviceListChangeEvent;
  }
  bool IsFake() const override { return false; }

 private:
  ~MediaEngineWebRTC() = default;
  void EnumerateVideoDevices(dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateMicrophoneDevices(nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateSpeakerDevices(nsTArray<RefPtr<MediaDevice>>*);

  void DeviceListChanged() { mDeviceListChangeEvent.Notify(); }

  static void FakeDeviceChangeEventTimerTick(nsITimer* aTimer, void* aClosure);

  MediaEventListener mCameraListChangeListener;
  MediaEventListener mMicrophoneListChangeListener;
  MediaEventListener mSpeakerListChangeListener;
  MediaEventProducer<void> mDeviceListChangeEvent;
  nsCOMPtr<nsITimer> mFakeDeviceChangeEventTimer;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEWEBRTC_H_ */
