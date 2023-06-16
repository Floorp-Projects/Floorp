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
#include "mozilla/EnumSet.h"
#include "mozilla/TaskQueue.h"

namespace mozilla {

class MFCDMProxy;

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
class MFMediaSource : public Microsoft::WRL::RuntimeClass<
                          Microsoft::WRL::RuntimeClassFlags<
                              Microsoft::WRL::RuntimeClassType::ClassicCom>,
                          IMFMediaSource, IMFRateControl, IMFRateSupport,
                          IMFGetService, IMFTrustedInput> {
 public:
  MFMediaSource();
  ~MFMediaSource();

  HRESULT RuntimeClassInitialize(const Maybe<AudioInfo>& aAudio,
                                 const Maybe<VideoInfo>& aVideo,
                                 nsISerialEventTarget* aManagerThread);

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

  // IMFTrustedInput
  IFACEMETHODIMP GetInputTrustAuthority(DWORD aStreamId, REFIID aRiid,
                                        IUnknown** aITAOut) override;

  MFMediaEngineStream* GetAudioStream();
  MFMediaEngineStream* GetVideoStream();

  MFMediaEngineStream* GetStreamByIndentifier(DWORD aStreamId) const;

#ifdef MOZ_WMF_CDM
  void SetCDMProxy(MFCDMProxy* aCDMProxy);
#endif

  TaskQueue* GetTaskQueue() const { return mTaskQueue; }

  MediaEventSource<SampleRequest>& RequestSampleEvent() {
    return mRequestSampleEvent;
  }

  // Called from the content process to notify that no more encoded data in that
  // type of track.
  void NotifyEndOfStream(TrackInfo::TrackType aType);

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
  State GetState() const;

  void SetDCompSurfaceHandle(HANDLE aDCompSurfaceHandle, gfx::IntSize aDisplay);

  void ShutdownTaskQueue();

  bool IsEncrypted() const;

 private:
  void AssertOnManagerThread() const;
  void AssertOnMFThreadPool() const;

  void NotifyEndOfStreamInternal(TrackInfo::TrackType aType);

  bool IsSeekable() const;

  // A thread-safe event queue.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-event-generators#implementing-imfmediaeventgenerator
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mMediaEventQueue;

  // The thread used to run the engine streams' tasks.
  RefPtr<TaskQueue> mTaskQueue;

  // The thread used to run the media source's tasks.
  RefPtr<nsISerialEventTarget> mManagerThread;

  // MFMediaEngineStream will notify us when we need more sample.
  friend class MFMediaEngineStream;
  MediaEventProducer<SampleRequest> mRequestSampleEvent;

  MediaEventListener mAudioStreamEndedListener;
  MediaEventListener mVideoStreamEndedListener;

  // This class would be run/accessed on two threads, MF thread pool and the
  // manager thread. Following members could be used across threads so they need
  // to be thread-safe.

  mutable Mutex mMutex{"MFMediaEngineSource"};

  // True if the playback is ended. Use and modify on both the manager thread
  // and MF thread pool.
  bool mPresentationEnded MOZ_GUARDED_BY(mMutex);
  bool mIsAudioEnded MOZ_GUARDED_BY(mMutex);
  bool mIsVideoEnded MOZ_GUARDED_BY(mMutex);

  // Modify on MF thread pool and the manager thread, read on any threads.
  State mState MOZ_GUARDED_BY(mMutex);

  Microsoft::WRL::ComPtr<MFMediaEngineStream> mAudioStream
      MOZ_GUARDED_BY(mMutex);
  Microsoft::WRL::ComPtr<MFMediaEngineStream> mVideoStream
      MOZ_GUARDED_BY(mMutex);

  // Thread-safe members END

  // Modify and access on MF thread pool.
  float mPlaybackRate = 0.0f;

#ifdef MOZ_WMF_CDM
  RefPtr<MFCDMProxy> mCDMProxy;
#endif
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIASOURCE_H
