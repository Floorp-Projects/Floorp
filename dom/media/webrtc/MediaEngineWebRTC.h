/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIAENGINEWEBRTC_H_
#define MEDIAENGINEWEBRTC_H_

#include "MediaEngine.h"
#include "MediaEventSource.h"
#include "MediaEngineSource.h"
#include "nsTArray.h"
#include "CubebDeviceEnumerator.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"

namespace mozilla {

class MediaEngineWebRTC : public MediaEngine {
 public:
  MediaEngineWebRTC();

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

  MediaEventListener mCameraListChangeListener;
  MediaEventListener mMicrophoneListChangeListener;
  MediaEventListener mSpeakerListChangeListener;
  MediaEventProducer<void> mDeviceListChangeEvent;
};

}  // namespace mozilla

#endif /* NSMEDIAENGINEWEBRTC_H_ */
