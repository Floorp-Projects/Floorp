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

class MFMediaEngineVideoStream;
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

  virtual nsCString GetCodecName() const = 0;

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

  void NotifyEndOfStream() {
    Microsoft::WRL::ComPtr<MFMediaEngineStream> self = this;
    Unused << mTaskQueue->Dispatch(NS_NewRunnableFunction(
        "MFMediaEngineStream::NotifyEndOfStream",
        [self]() { self->NotifyEndOfStreamInternal(); }));
  }

  // Return the type of the track, the result should be either audio or video.
  virtual TrackInfo::TrackType TrackType() = 0;

  RefPtr<MediaDataDecoder::FlushPromise> Flush();

  MediaEventProducer<TrackInfo::TrackType>& EndedEvent() { return mEndedEvent; }

  // True if the stream has been shutdown, it's a thread safe method.
  bool IsShutdown() const { return mIsShutdown; }

  virtual MFMediaEngineVideoStream* AsVideoStream() { return nullptr; }

  RefPtr<MediaDataDecoder::DecodePromise> OutputData(
      RefPtr<MediaRawData> aSample);

  virtual RefPtr<MediaDataDecoder::DecodePromise> Drain();

  virtual MediaDataDecoder::ConversionRequired NeedsConversion() const {
    return MediaDataDecoder::ConversionRequired::kNeedNone;
  }

  virtual bool IsEncrypted() const = 0;

 protected:
  HRESULT GenerateStreamDescriptor(
      Microsoft::WRL::ComPtr<IMFMediaType>& aMediaType);

  // Create a IMFMediaType which includes the details about the stream.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
  virtual HRESULT CreateMediaType(const TrackInfo& aInfo,
                                  IMFMediaType** aMediaType) = 0;

  // True if the stream already has enough raw data.
  virtual bool HasEnoughRawData() const = 0;

  HRESULT CreateInputSample(IMFSample** aSample);
  void ReplySampleRequestIfPossible();
  bool ShouldServeSamples() const;

  void NotifyNewData(MediaRawData* aSample);
  void NotifyEndOfStreamInternal();

  virtual bool IsEnded() const;

  // Overwrite this method if inherited class needs to perform clean up on the
  // task queue when the stream gets shutdowned.
  virtual void ShutdownCleanUpOnTaskQueue(){};

  // Inherited class must implement this method to return decoded data. it
  // should uses `mRawDataQueueForGeneratingOutput` to generate output.
  virtual already_AddRefed<MediaData> OutputDataInternal() = 0;

  void SendRequestSampleEvent(bool aIsEnough);

  HRESULT AddEncryptAttributes(IMFSample* aSample,
                               const CryptoSample& aCryptoConfig);

  void AssertOnTaskQueue() const;
  void AssertOnMFThreadPool() const;

  // IMFMediaEventQueue is thread-safe.
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mMediaEventQueue;
  Microsoft::WRL::ComPtr<IMFStreamDescriptor> mStreamDescriptor;
  Microsoft::WRL::ComPtr<MFMediaSource> mParentSource;

  // This an unique ID retrieved from the IMFStreamDescriptor.
  DWORD mStreamDescriptorId = 0;

  // A unique ID assigned by MFMediaSource, which won't be changed after first
  // assignment.
  uint64_t mStreamId = 0;

  RefPtr<TaskQueue> mTaskQueue;

  // This class would be run on three threads, MF thread pool, the source's
  // task queue and MediaPDecoder (wrapper thread). Following members would be
  // used across both threads so they need to be thread-safe.

  // Modify on the MF thread pool, access from any threads.
  Atomic<bool> mIsShutdown;

  // True if the stream is selected by the media source.
  // Modify on MF thread pool, access from any threads.
  Atomic<bool> mIsSelected;

  // A thread-safe queue storing input samples, which provides samples to the
  // media engine.
  MediaQueue<MediaRawData> mRawDataQueueForFeedingEngine;

  // A thread-safe queue storing input samples, which would be used to generate
  // decoded data.
  MediaQueue<MediaRawData> mRawDataQueueForGeneratingOutput;

  // Thread-safe members END

  // Store sample request token, one token should be related with one output
  // data. It's used on the task queue only.
  std::queue<Microsoft::WRL::ComPtr<IUnknown>> mSampleRequestTokens;

  // Notify when playback reachs the end for this track.
  MediaEventProducer<TrackInfo::TrackType> mEndedEvent;

  // True if the stream has received the last data, but it could be reset if the
  // stream starts delivering more data. Used on the task queue only.
  bool mReceivedEOS;
};

/**
 * This wrapper helps to dispatch task onto the stream's task queue. Its methods
 * are not thread-safe and would only be called on the IPC decoder manager
 * thread.
 */
class MFMediaEngineStreamWrapper final : public MediaDataDecoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFMediaEngineStreamWrapper, final);

  MFMediaEngineStreamWrapper(MFMediaEngineStream* aStream,
                             TaskQueue* aTaskQueue,
                             const CreateDecoderParams& aParams)
      : mStream(aStream), mTaskQueue(aTaskQueue) {
    MOZ_ASSERT(mStream);
    MOZ_ASSERT(mTaskQueue);
  }

  // Methods for MediaDataDecoder, they are all called on the remote
  // decoder manager thread.
  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;
  nsCString GetCodecName() const override;
  ConversionRequired NeedsConversion() const override;

 private:
  ~MFMediaEngineStreamWrapper() = default;

  Microsoft::WRL::ComPtr<MFMediaEngineStream> mStream;
  RefPtr<TaskQueue> mTaskQueue;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFMEDIAENGINESTREAM_H
