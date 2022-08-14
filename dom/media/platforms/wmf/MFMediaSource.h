/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIASOURCE_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIASOURCE_H

#include <mfidl.h>
#include <wrl.h>

#include "MediaInfo.h"
#include "MediaEventSource.h"
#include "MFMediaEngineExtra.h"
#include "MFMediaEngineStream.h"
#include "mozilla/Atomics.h"
#include "mozilla/EnumSet.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

// An event to indicate a need for a certain type of sample.
struct SampleRequest {
  SampleRequest(TrackInfo::TrackType aType, bool aIsEnough)
      : mType(aType), mIsEnough(aIsEnough) {}
  TrackInfo::TrackType mType;
  bool mIsEnough;
};

/**
 * MFMediaSource is a custom source for the media engine, the media engine would
 * ask the source for the characteristics and the presentation descriptor to
 * know how to react with the source. This source is also responsible to
 * dispatch events to the media engine to notify the status changes.
 *
 * https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfmediasource
 */
// TODO : support IMFTrustedInput
class MFMediaSource
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaSource, IMFRateControl, IMFRateSupport, IMFGetService> {
 public:
  MFMediaSource();
  HRESULT RuntimeClassInitialize(const Maybe<AudioInfo>& aAudio,
                                 const Maybe<VideoInfo>& aVideo);

  // Methods for IMFMediaSource
  IFACEMETHODIMP GetCharacteristics(DWORD* aCharacteristics) override;
  IFACEMETHODIMP CreatePresentationDescriptor(
      IMFPresentationDescriptor** aPresentationDescriptor) override;
  IFACEMETHODIMP Start(IMFPresentationDescriptor* aPresentationDescriptor,
                       const GUID* aGuidTimeFormat,
                       const PROPVARIANT* aStartPosition) override;
  IFACEMETHODIMP Stop() override;
  IFACEMETHODIMP Pause() override;
  IFACEMETHODIMP Shutdown() override;

  // Methods for IMFMediaEventGenerator, IMFMediaSource derives from
  // IMFMediaEventGenerator.
  IFACEMETHODIMP GetEvent(DWORD aFlags, IMFMediaEvent** aEvent) override;
  IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* aCallback,
                               IUnknown* aState) override;
  IFACEMETHODIMP EndGetEvent(IMFAsyncResult* aResult,
                             IMFMediaEvent** aEvent) override;
  IFACEMETHODIMP QueueEvent(MediaEventType aType, REFGUID aExtendedType,
                            HRESULT aStatus,
                            const PROPVARIANT* aValue) override;

  // IMFGetService
  IFACEMETHODIMP GetService(REFGUID aGuidService, REFIID aRiid,
                            LPVOID* aResult) override;

  // IMFRateSupport
  IFACEMETHODIMP GetSlowestRate(MFRATE_DIRECTION aDirection,
                                BOOL aSupportsThinning, float* aRate) override;
  IFACEMETHODIMP GetFastestRate(MFRATE_DIRECTION aDirection,
                                BOOL aSupportsThinning, float* aRate) override;
  IFACEMETHODIMP IsRateSupported(BOOL aSupportsThinning, float aNewRate,
                                 float* aSupportedRate) override;

  // IMFRateControl
  IFACEMETHODIMP SetRate(BOOL aSupportsThinning, float aRate) override;
  IFACEMETHODIMP GetRate(BOOL* aSupportsThinning, float* aRate) override;

  MFMediaEngineStream* GetAudioStream() { return mAudioStream.Get(); }
  MFMediaEngineStream* GetVideoStream() { return mVideoStream.Get(); }

  TaskQueue* GetTaskQueue() { return mTaskQueue; }

  MediaEventSource<SampleRequest>& RequestSampleEvent() {
    return mRequestSampleEvent;
  }

  // Called from the content process to notify that no more encoded data in that
  // type of track.
  void NotifyEndOfStream(TrackInfo::TrackType aType) {
    Unused << GetTaskQueue()->Dispatch(NS_NewRunnableFunction(
        "MFMediaSource::NotifyEndOfStream", [aType, self = RefPtr{this}]() {
          self->NotifyEndOfStreamInternal(aType);
        }));
  }

  // Called from the MF stream to indicate that the stream has provided last
  // encoded sample to the media engine.
  void HandleStreamEnded(TrackInfo::TrackType aType);

  enum class State {
    Initialized,
    Started,
    Stopped,
    Paused,
    Shutdowned,
  };
  State GetState() const { return mState; }

  void SetDCompSurfaceHandle(HANDLE aDCompSurfaceHandle);

 private:
  void AssertOnTaskQueue() const;
  void AssertOnMFThreadPool() const;

  void NotifyEndOfStreamInternal(TrackInfo::TrackType aType);

  bool IsSeekable() const;
  MFMediaEngineStream* GetStreamByDescriptorId(DWORD aId) const;

  // A thread-safe event queue.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-event-generators#implementing-imfmediaeventgenerator
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mMediaEventQueue;
  Microsoft::WRL::ComPtr<MFMediaEngineStream> mAudioStream;
  Microsoft::WRL::ComPtr<MFMediaEngineStream> mVideoStream;

  RefPtr<TaskQueue> mTaskQueue;

  // MFMediaEngineStream will notify us when we need more sample.
  friend class MFMediaEngineStream;
  MediaEventProducer<SampleRequest> mRequestSampleEvent;

  MediaEventListener mAudioStreamEndedListener;
  MediaEventListener mVideoStreamEndedListener;

  // This class would be run on two threads, MF thread pool and the source's
  // task queue. Following members would be used across both threads so they
  // need to be thread-safe.

  // True if the playback is ended. Use and modify on both task queue and MF
  // thread pool.
  Atomic<bool> mPresentationEnded;

  // Modify on MF thread pool, read on any threads.
  Atomic<State> mState;

  // Thread-safe members END

  // Modify and access on MF thread pool.
  float mPlaybackRate = 0.0f;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIASOURCE_H
