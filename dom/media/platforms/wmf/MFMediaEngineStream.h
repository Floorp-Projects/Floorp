/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H
#define DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H

#include <mfidl.h>
#include <wrl.h>

#include <queue>

#include "BlankDecoderModule.h"
#include "MediaQueue.h"
#include "PlatformDecoderModule.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "mozilla/SPSCQueue.h"

namespace mozilla {

class MFMediaSource;

/**
 * MFMediaEngineStream represents a track which would be responsible to provide
 * encoded data into the media engine. The media engine can access this stream
 * by the presentation descriptor which was acquired from the custom media
 * source.
 */
class MFMediaEngineStream
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaStream> {
 public:
  MFMediaEngineStream();
  ~MFMediaEngineStream();

  virtual nsCString GetDescriptionName() const = 0;

  HRESULT RuntimeClassInitialize(uint64_t aStreamId, const TrackInfo& aInfo,
                                 MFMediaSource* aParentSource);

  // Called by MFMediaSource.
  HRESULT Start(const PROPVARIANT* aPosition);
  HRESULT Seek(const PROPVARIANT* aPosition);
  HRESULT Stop();
  HRESULT Pause();
  void Shutdown();

  void SetSelected(bool aSelected);
  bool IsSelected() const { return mIsSelected; }
  DWORD DescriptorId() const { return mStreamDescriptorId; }

  // Methods for IMFMediaStream
  IFACEMETHODIMP GetMediaSource(IMFMediaSource** aMediaSource) override;
  IFACEMETHODIMP GetStreamDescriptor(
      IMFStreamDescriptor** aStreamDescriptor) override;
  IFACEMETHODIMP RequestSample(IUnknown* aToken) override;

  // Methods for IMFMediaEventGenerator, IMFMediaStream derives from
  // IMFMediaEventGenerator.
  IFACEMETHODIMP GetEvent(DWORD aFlags, IMFMediaEvent** aEvent) override;
  IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* aCallback,
                               IUnknown* aState) override;
  IFACEMETHODIMP EndGetEvent(IMFAsyncResult* aResult,
                             IMFMediaEvent** aEvent) override;
  IFACEMETHODIMP QueueEvent(MediaEventType aType, REFGUID aExtendedType,
                            HRESULT aStatus,
                            const PROPVARIANT* aValue) override;

  TaskQueue* GetTaskQueue() { return mTaskQueue; }

  void NotifyNewData(MediaRawData* aSample);
  void NotifyEndOfStream();

  // Return the type of the track, the result should be either audio or video.
  virtual TrackInfo::TrackType TrackType() = 0;

  RefPtr<MediaDataDecoder::FlushPromise> Flush();

  bool IsEnded() const;

  MediaEventProducer<TrackInfo::TrackType>& EndedEvent() { return mEndedEvent; }

  // True if the stream has been shutdown, it's a thread safe method.
  bool IsShutdown() const { return mIsShutdown; }

 protected:
  HRESULT GenerateStreamDescriptor(uint64_t aStreamId, const TrackInfo& aInfo);

  // Create a IMFMediaType which includes the details about the stream.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
  virtual HRESULT CreateMediaType(const TrackInfo& aInfo,
                                  IMFMediaType** aMediaType) = 0;

  // True if the stream already has enough raw data.
  virtual bool HasEnoughRawData() const = 0;

  HRESULT CreateInputSample(IMFSample** aSample);
  void ReplySampleRequestIfPossible();
  bool ShouldServeSamples() const;

  void AssertOnTaskQueue() const;
  void AssertOnMFThreadPool() const;

  // IMFMediaEventQueue is thread-safe.
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mMediaEventQueue;
  Microsoft::WRL::ComPtr<IMFStreamDescriptor> mStreamDescriptor;
  Microsoft::WRL::ComPtr<MFMediaSource> mParentSource;

  // This an unique ID retrieved from the IMFStreamDescriptor.
  DWORD mStreamDescriptorId = 0;

  RefPtr<TaskQueue> mTaskQueue;

  // This class would be run on two threads, MF thread pool and the source's
  // task queue. Following members would be used across both threads so they
  // need to be thread-safe.

  // Modify on the MF thread pool, access from any threads.
  Atomic<bool> mIsShutdown;

  // True if the stream is selected by the media source.
  // Modify on MF thread pool, access from any threads.
  Atomic<bool> mIsSelected;

  // True if the stream has received the last data.
  // Modify on the task queue, access from any threads.
  Atomic<bool> mReceivedEOS;

  // Only serve samples when the stream is already started.
  Atomic<bool> mShouldServeSmamples;

  // A thread-safe queue storing input sample.
  MediaQueue<MediaRawData> mRawDataQueue;

  // Thread-safe members END

  // Store sample request token, one token should be related with one output
  // data. It's used on the task queue only.
  std::queue<Microsoft::WRL::ComPtr<IUnknown>> mSampleRequestTokens;

  // Notify when playback reachs the end for this track.
  MediaEventProducer<TrackInfo::TrackType> mEndedEvent;
};

/**
 * This wrapper helps to dispatch task onto the stream's task queue. Its methods
 * are not thread-safe and would only be called on the IPC decoder manager
 * thread.
 */
class MFMediaEngineStreamWrapper : public MediaDataDecoder {
 public:
  MFMediaEngineStreamWrapper(MFMediaEngineStream* aStream,
                             TaskQueue* aTaskQueue,
                             const CreateDecoderParams& aParams)
      : mStream(aStream),
        mTaskQueue(aTaskQueue),
        mFakeDataCreator(new FakeDecodedDataCreator(aParams)) {
    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mTaskQueue);
    MOZ_ASSERT(mFakeDataCreator);
  }

  // Methods for MediaDataDecoder, they are all called on the remote
  // decoder manager thread.
  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;

 private:
  Microsoft::WRL::ComPtr<MFMediaEngineStream> mStream;
  // We use this to generate fake decoded outputs, as the real data is handled
  // inside the media engine. Audio output is not possible to get, the video
  // output would be output via DCOMP.
  class FakeDecodedDataCreator final {
   public:
    explicit FakeDecodedDataCreator(const CreateDecoderParams& aParams) {
      if (aParams.mConfig.IsVideo()) {
        const VideoInfo& config = aParams.VideoConfig();
        mDummyDecoder = new DummyMediaDataDecoder(
            MakeUnique<BlankVideoDataCreator>(config.mDisplay.width,
                                              config.mDisplay.height,
                                              aParams.mImageContainer),
            "blank video data decoder for media engine"_ns, aParams);
        mType = TrackInfo::TrackType::kVideoTrack;
      } else if (aParams.mConfig.IsAudio()) {
        const AudioInfo& config = aParams.AudioConfig();
        mDummyDecoder = new DummyMediaDataDecoder(
            MakeUnique<BlankAudioDataCreator>(config.mChannels, config.mRate),
            "blank audio data decoder for media engine"_ns, aParams);
        mType = TrackInfo::TrackType::kAudioTrack;
      } else {
        MOZ_ASSERT_UNREACHABLE("unexpected config type");
      }
    }
    RefPtr<MediaDataDecoder::DecodePromise> Decode(MediaRawData* aSample) {
      return mDummyDecoder->Decode(aSample);
    }
    void Flush() { Unused << mDummyDecoder->Flush(); }

    TrackInfo::TrackType Type() const { return mType; }

   private:
    RefPtr<MediaDataDecoder> mDummyDecoder;
    TrackInfo::TrackType mType;
  };
  RefPtr<TaskQueue> mTaskQueue;
  UniquePtr<FakeDecodedDataCreator> mFakeDataCreator;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H
