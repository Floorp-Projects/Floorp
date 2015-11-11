/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_CODEC_READER_H
#define MEDIA_CODEC_READER_H

#include <utils/threads.h>

#include <base/message_loop.h>

#include <mozilla/CheckedInt.h>
#include <mozilla/Mutex.h>
#include <mozilla/Monitor.h>

#include <nsDataHashtable.h>

#include "MediaData.h"

#include "I420ColorConverterHelper.h"
#include "MediaCodecProxy.h"
#include "MediaOmxCommonReader.h"
#include "mozilla/layers/FenceUtils.h"
#if MOZ_WIDGET_GONK && ANDROID_VERSION >= 17
#include <ui/Fence.h>
#endif

#include "MP3FrameParser.h"

namespace android {
struct ALooper;
struct AMessage;

class MOZ_EXPORT MediaExtractor;
class MOZ_EXPORT MetaData;
class MOZ_EXPORT MediaBuffer;
struct MOZ_EXPORT MediaSource;

class GonkNativeWindow;
} // namespace android

namespace mozilla {

class FlushableTaskQueue;
class MP3FrameParser;

namespace layers {
class TextureClient;
} // namespace mozilla::layers

class MediaCodecReader : public MediaOmxCommonReader
{
  typedef mozilla::layers::TextureClient TextureClient;
  typedef mozilla::layers::FenceHandle FenceHandle;
  typedef MediaOmxCommonReader::MediaResourcePromise MediaResourcePromise;

public:
  MediaCodecReader(AbstractMediaDecoder* aDecoder);
  virtual ~MediaCodecReader();

  // Release media resources they should be released in dormant state
  virtual void ReleaseMediaResources();

  // Destroys the decoding state. The reader cannot be made usable again.
  // This is different from ReleaseMediaResources() as Shutdown() is
  // irreversible, whereas ReleaseMediaResources() is reversible.
  virtual RefPtr<ShutdownPromise> Shutdown();

protected:
  // Used to retrieve some special information that can only be retrieved after
  // all contents have been continuously parsed. (ex. total duration of some
  // variable-bit-rate MP3 files.)
  virtual void NotifyDataArrivedInternal() override;
public:

  // Flush the TaskQueue, flush MediaCodec and raise the mDiscontinuity.
  virtual nsresult ResetDecode() override;

  // Disptach a DecodeVideoFrameTask to decode video data.
  virtual RefPtr<VideoDataPromise>
  RequestVideoData(bool aSkipToNextKeyframe,
                   int64_t aTimeThreshold) override;

  // Disptach a DecodeAduioDataTask to decode video data.
  virtual RefPtr<AudioDataPromise> RequestAudioData() override;

  virtual RefPtr<MediaDecoderReader::MetadataPromise> AsyncReadMetadata() override;

  // Moves the decode head to aTime microseconds. aStartTime and aEndTime
  // denote the start and end times of the media in usecs, and aCurrentTime
  // is the current playback position in microseconds.
  virtual RefPtr<SeekPromise>
  Seek(int64_t aTime, int64_t aEndTime) override;

  virtual bool IsMediaSeekable() override;

  virtual android::sp<android::MediaSource> GetAudioOffloadTrack();

  virtual bool IsAsync() const override { return true; }

protected:
  struct TrackInputCopier
  {
    virtual ~TrackInputCopier();

    virtual bool Copy(android::MediaBuffer* aSourceBuffer,
                      android::sp<android::ABuffer> aCodecBuffer);
  };

  struct Track
  {
    enum Type
    {
      kUnknown = 0,
      kAudio,
      kVideo,
    };

    Track(Type type=kUnknown);

    const Type mType;

    // pipeline parameters
    android::sp<android::MediaSource> mSource;
    bool mSourceIsStopped;
    android::sp<android::MediaCodecProxy> mCodec;
    android::Vector<android::sp<android::ABuffer> > mInputBuffers;
    android::Vector<android::sp<android::ABuffer> > mOutputBuffers;
    android::sp<android::GonkNativeWindow> mNativeWindow;
#if ANDROID_VERSION >= 21
    android::sp<android::IGraphicBufferProducer> mGraphicBufferProducer;
#endif

    // pipeline copier
    nsAutoPtr<TrackInputCopier> mInputCopier;

    // Protected by mTrackMonitor.
    // mDurationUs might be read or updated from multiple threads.
    int64_t mDurationUs;

    // playback parameters
    CheckedUint32 mInputIndex;

    bool mInputEndOfStream;
    bool mOutputEndOfStream;
    int64_t mSeekTimeUs;
    bool mFlushed; // meaningless when mSeekTimeUs is invalid.
    bool mDiscontinuity;
    RefPtr<TaskQueue> mTaskQueue;
    Monitor mTrackMonitor;

  private:
    // Forbidden
    Track(const Track &rhs) = delete;
    const Track &operator=(const Track&) = delete;
  };

  // Receive a message from MessageHandler.
  // Called on MediaCodecReader::mLooper thread.
  void onMessageReceived(const android::sp<android::AMessage>& aMessage);

  virtual bool CreateExtractor();

  virtual void HandleResourceAllocated();

  android::sp<android::MediaExtractor> mExtractor;

  MozPromiseHolder<MediaDecoderReader::MetadataPromise> mMetadataPromise;
  // XXX Remove after bug 1168008 land.
  MozPromiseRequestHolder<MediaResourcePromise> mMediaResourceRequest;
  MozPromiseHolder<MediaResourcePromise> mMediaResourcePromise;

  MozPromiseRequestHolder<android::MediaCodecProxy::CodecPromise> mVideoCodecRequest;

private:
  virtual bool HasAudio() override;
  virtual bool HasVideo() override;

  class VorbisInputCopier : public TrackInputCopier
  {
    virtual bool Copy(android::MediaBuffer* aSourceBuffer,
                      android::sp<android::ABuffer> aCodecBuffer);
  };

  struct AudioTrack : public Track
  {
    AudioTrack();
    // Protected by mTrackMonitor.
    MozPromiseHolder<AudioDataPromise> mAudioPromise;

  private:
    // Forbidden
    AudioTrack(const AudioTrack &rhs) = delete;
    const AudioTrack &operator=(const AudioTrack &rhs) = delete;
  };

  struct VideoTrack : public Track
  {
    VideoTrack();

    int32_t mWidth;
    int32_t mHeight;
    int32_t mStride;
    int32_t mSliceHeight;
    int32_t mColorFormat;
    int32_t mRotation;
    nsIntSize mFrameSize;
    nsIntRect mPictureRect;
    gfx::IntRect mRelativePictureRect;
    // Protected by mTrackMonitor.
    MozPromiseHolder<VideoDataPromise> mVideoPromise;

    RefPtr<TaskQueue> mReleaseBufferTaskQueue;
  private:
    // Forbidden
    VideoTrack(const VideoTrack &rhs) = delete;
    const VideoTrack &operator=(const VideoTrack &rhs) = delete;
  };

  struct CodecBufferInfo
  {
    CodecBufferInfo();

    android::sp<android::ABuffer> mBuffer;
    size_t mIndex;
    size_t mOffset;
    size_t mSize;
    int64_t mTimeUs;
    uint32_t mFlags;
  };

  class SignalObject
  {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SignalObject)

    SignalObject(const char* aName);
    void Wait();
    void Signal();

  protected:
    ~SignalObject();

  private:
    // Forbidden
    SignalObject() = delete;
    SignalObject(const SignalObject &rhs) = delete;
    const SignalObject &operator=(const SignalObject &rhs) = delete;

    Monitor mMonitor;
    bool mSignaled;
  };

  class ParseCachedDataRunnable : public nsRunnable
  {
  public:
    ParseCachedDataRunnable(RefPtr<MediaCodecReader> aReader,
                            const char* aBuffer,
                            uint32_t aLength,
                            int64_t aOffset,
                            RefPtr<SignalObject> aSignal);

    NS_IMETHOD Run() override;

  private:
    // Forbidden
    ParseCachedDataRunnable() = delete;
    ParseCachedDataRunnable(const ParseCachedDataRunnable &rhs) = delete;
    const ParseCachedDataRunnable &operator=(const ParseCachedDataRunnable &rhs) = delete;

    RefPtr<MediaCodecReader> mReader;
    nsAutoArrayPtr<const char> mBuffer;
    uint32_t mLength;
    int64_t mOffset;
    RefPtr<SignalObject> mSignal;
  };
  friend class ParseCachedDataRunnable;

  class ProcessCachedDataTask : public Task
  {
  public:
    ProcessCachedDataTask(RefPtr<MediaCodecReader> aReader,
                          int64_t aOffset);

    void Run() override;

  private:
    // Forbidden
    ProcessCachedDataTask() = delete;
    ProcessCachedDataTask(const ProcessCachedDataTask &rhs) = delete;
    const ProcessCachedDataTask &operator=(const ProcessCachedDataTask &rhs) = delete;

    RefPtr<MediaCodecReader> mReader;
    int64_t mOffset;
  };
  friend class ProcessCachedDataTask;

  // Forbidden
  MediaCodecReader() = delete;
  const MediaCodecReader& operator=(const MediaCodecReader& rhs) = delete;

  bool ReallocateExtractorResources();
  void ReleaseCriticalResources();
  void ReleaseResources();

  bool CreateLooper();
  void DestroyLooper();

  void DestroyExtractor();

  bool CreateMediaSources();
  void DestroyMediaSources();

  RefPtr<MediaResourcePromise> CreateMediaCodecs();
  bool CreateMediaCodec(android::sp<android::ALooper>& aLooper,
                        Track& aTrack,
                        bool& aIsWaiting);
  static bool ConfigureMediaCodec(Track& aTrack);
  void DestroyMediaCodecs();
  static void DestroyMediaCodec(Track& aTrack);

  bool CreateTaskQueues();
  void ShutdownTaskQueues();
  void DecodeVideoFrameTask(int64_t aTimeThreshold);
  void DecodeVideoFrameSync(int64_t aTimeThreshold);
  void DecodeAudioDataTask();
  void DecodeAudioDataSync();
  void DispatchVideoTask(int64_t aTimeThreshold);
  void DispatchAudioTask();
  inline bool CheckVideoResources() {
    return (HasVideo() && mVideoTrack.mSource != nullptr &&
            mVideoTrack.mTaskQueue);
  }

  inline bool CheckAudioResources() {
    return (HasAudio() && mAudioTrack.mSource != nullptr &&
            mAudioTrack.mTaskQueue);
  }

  bool TriggerIncrementalParser();

  bool UpdateDuration();
  bool UpdateAudioInfo();
  bool UpdateVideoInfo();

  android::status_t FlushCodecData(Track& aTrack);
  android::status_t FillCodecInputData(Track& aTrack);
  android::status_t GetCodecOutputData(Track& aTrack,
                                       CodecBufferInfo& aBuffer,
                                       int64_t aThreshold,
                                       const TimeStamp& aTimeout);
  bool EnsureCodecFormatParsed(Track& aTrack);

  uint8_t* GetColorConverterBuffer(int32_t aWidth, int32_t aHeight);
  void ClearColorConverterBuffer();

  int64_t ProcessCachedData(int64_t aOffset,
                            RefPtr<SignalObject> aSignal);
  bool ParseDataSegment(const char* aBuffer,
                        uint32_t aLength,
                        int64_t aOffset);

  static void TextureClientRecycleCallback(TextureClient* aClient,
                                           void* aClosure);
  void TextureClientRecycleCallback(TextureClient* aClient);
  void WaitFenceAndReleaseOutputBuffer();

  void ReleaseRecycledTextureClients();

  void ReleaseAllTextureClients();

  android::sp<android::ALooper> mLooper;
  android::sp<android::MetaData> mMetaData;

  Mutex mTextureClientIndexesLock;
  nsDataHashtable<nsPtrHashKey<TextureClient>, size_t> mTextureClientIndexes;

  // media tracks
  AudioTrack mAudioTrack;
  VideoTrack mVideoTrack;
  AudioTrack mAudioOffloadTrack; // only Track::mSource is valid

  // color converter
  android::I420ColorConverterHelper mColorConverter;
  nsAutoArrayPtr<uint8_t> mColorConverterBuffer;
  size_t mColorConverterBufferSize;

  // incremental parser
  Monitor mParserMonitor;
  bool mParseDataFromCache;
  int64_t mNextParserPosition;
  int64_t mParsedDataLength;
  nsAutoPtr<MP3FrameParser> mMP3FrameParser;
  // mReleaseIndex corresponding to a graphic buffer, and the mReleaseFence is
  // the graohic buffer's fence. We must wait for the fence signaled by
  // compositor, otherwise we will see the flicker because the HW decoder and
  // compositor use the buffer concurrently.
  struct ReleaseItem {
    ReleaseItem(size_t aIndex, const FenceHandle& aFence)
    : mReleaseIndex(aIndex)
    , mReleaseFence(aFence) {}
    size_t mReleaseIndex;
    FenceHandle mReleaseFence;
  };
  nsTArray<ReleaseItem> mPendingReleaseItems;

  NotifyDataArrivedFilter mFilter;
};

} // namespace mozilla

#endif // MEDIA_CODEC_READER_H
