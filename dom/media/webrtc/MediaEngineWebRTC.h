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
#include "cubeb/cubeb.h"
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

// This class implements a cache for accessing the audio device list. It can be
// accessed on any thread.
class CubebDeviceEnumerator final
{
public:
  CubebDeviceEnumerator();
  ~CubebDeviceEnumerator();
  // This method returns a list of all the input and output audio devices
  // available on this machine.
  // This method is safe to call from all threads.
  void EnumerateAudioInputDevices(nsTArray<RefPtr<AudioDeviceInfo>>& aOutDevices);
  // From a cubeb device id, return the info for this device, if it's still a
  // valid id, or nullptr otherwise.
  // This method is safe to call from any thread.
  already_AddRefed<AudioDeviceInfo>
  DeviceInfoFromID(CubebUtils::AudioDeviceID aID);

protected:

  // Static function called by cubeb when the audio input device list changes
  // (i.e. when a new device is made available, or non-available). This
  // re-binds to the MediaEngineWebRTC that instantiated this
  // CubebDeviceEnumerator, and simply calls `AudioDeviceListChanged` below.
  static void AudioDeviceListChanged_s(cubeb* aContext, void* aUser);
  // Invalidates the cached audio input device list, can be called on any
  // thread.
  void AudioDeviceListChanged();

private:
  // Synchronize access to mDevices
  Mutex mMutex;
  nsTArray<RefPtr<AudioDeviceInfo>> mDevices;
  // If mManualInvalidation is true, then it is necessary to query the device
  // list each time instead of relying on automatic invalidation of the cache by
  // cubeb itself. Set in the constructor and then can be access on any thread.
  bool mManualInvalidation;
};

class MediaEngineWebRTC : public MediaEngine
{
  typedef MediaEngine Super;
public:
  explicit MediaEngineWebRTC(MediaEnginePrefs& aPrefs);

  virtual void SetFakeDeviceChangeEvents() override;

  // Clients should ensure to clean-up sources video/audio sources
  // before invoking Shutdown on this class.
  void Shutdown() override;

  // Returns whether the host supports duplex audio stream.
  bool SupportsDuplex();

  void EnumerateDevices(uint64_t aWindowId,
                        dom::MediaSourceEnum,
                        MediaSinkEnum,
                        nsTArray<RefPtr<MediaDevice>>*) override;
  void ReleaseResourcesForWindow(uint64_t aWindowId) override;
private:
  ~MediaEngineWebRTC() = default;
  void EnumerateVideoDevices(uint64_t aWindowId,
                             dom::MediaSourceEnum,
                             nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateMicrophoneDevices(uint64_t aWindowId,
                                  nsTArray<RefPtr<MediaDevice>>*);
  void EnumerateSpeakerDevices(uint64_t aWindowId,
                               nsTArray<RefPtr<MediaDevice> >*);

  // gUM runnables can e.g. Enumerate from multiple threads
  Mutex mMutex;
  UniquePtr<mozilla::CubebDeviceEnumerator> mEnumerator;
  const bool mDelayAgnostic;
  const bool mExtendedFilter;
  // This also is set in the ctor and then never changed, but we can't make it
  // const because we pass it to a function that takes bool* in the ctor.
  bool mHasTabVideoSource;

  // Maps WindowID to a map of device uuid to their MediaEngineSource,
  // separately for audio and video.
  nsClassHashtable<nsUint64HashKey,
                    nsRefPtrHashtable<nsStringHashKey,
                                      MediaEngineSource>> mVideoSources;
  nsClassHashtable<nsUint64HashKey,
                    nsRefPtrHashtable<nsStringHashKey,
                                      MediaEngineSource>> mAudioSources;
};

}

#endif /* NSMEDIAENGINEWEBRTC_H_ */
