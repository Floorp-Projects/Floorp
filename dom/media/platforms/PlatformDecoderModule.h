/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#define PlatformDecoderModule_h_

#include "DecoderDoctorLogger.h"
#include "GMPCrashHelper.h"
#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaResult.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsTArray.h"
#include <queue>

namespace mozilla {
class TrackInfo;
class AudioInfo;
class VideoInfo;
class MediaRawData;
class DecoderDoctorDiagnostics;

namespace layers {
class ImageContainer;
} // namespace layers

namespace dom {
class RemoteDecoderModule;
}

class MediaDataDecoder;
class TaskQueue;
class CDMProxy;

static LazyLogModule sPDMLog("PlatformDecoderModule");

struct MOZ_STACK_CLASS CreateDecoderParams final
{
  explicit CreateDecoderParams(const TrackInfo& aConfig) : mConfig(aConfig) { }

  enum class Option
  {
    Default,
    LowLatency,
  };
  using OptionSet = EnumSet<Option>;

  struct UseNullDecoder
  {
    UseNullDecoder() = default;
    explicit UseNullDecoder(bool aUseNullDecoder) : mUse(aUseNullDecoder) { }
    bool mUse = false;
  };

  struct VideoFrameRate
  {
    VideoFrameRate() = default;
    explicit VideoFrameRate(float aFramerate) : mValue(aFramerate) { }
    float mValue = 0.0f;
  };

  template <typename T1, typename... Ts>
  CreateDecoderParams(const TrackInfo& aConfig, T1&& a1, Ts&&... args)
    : mConfig(aConfig)
  {
    Set(std::forward<T1>(a1), std::forward<Ts>(args)...);
  }

  const VideoInfo& VideoConfig() const
  {
    MOZ_ASSERT(mConfig.IsVideo());
    return *mConfig.GetAsVideoInfo();
  }

  const AudioInfo& AudioConfig() const
  {
    MOZ_ASSERT(mConfig.IsAudio());
    return *mConfig.GetAsAudioInfo();
  }

  layers::LayersBackend GetLayersBackend() const
  {
    if (mKnowsCompositor) {
      return mKnowsCompositor->GetCompositorBackendType();
    }
    return layers::LayersBackend::LAYERS_NONE;
  }

  const TrackInfo& mConfig;
  TaskQueue* mTaskQueue = nullptr;
  DecoderDoctorDiagnostics* mDiagnostics = nullptr;
  layers::ImageContainer* mImageContainer = nullptr;
  MediaResult* mError = nullptr;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  RefPtr<GMPCrashHelper> mCrashHelper;
  UseNullDecoder mUseNullDecoder;
  TrackInfo::TrackType mType = TrackInfo::kUndefinedTrack;
  MediaEventProducer<TrackInfo::TrackType>* mOnWaitingForKeyEvent = nullptr;
  OptionSet mOptions = OptionSet(Option::Default);
  VideoFrameRate mRate;

private:
  void Set(TaskQueue* aTaskQueue) { mTaskQueue = aTaskQueue; }
  void Set(DecoderDoctorDiagnostics* aDiagnostics)
  {
    mDiagnostics = aDiagnostics;
  }
  void Set(layers::ImageContainer* aImageContainer)
  {
    mImageContainer = aImageContainer;
  }
  void Set(MediaResult* aError) { mError = aError; }
  void Set(GMPCrashHelper* aCrashHelper) { mCrashHelper = aCrashHelper; }
  void Set(UseNullDecoder aUseNullDecoder) { mUseNullDecoder = aUseNullDecoder; }
  void Set(OptionSet aOptions) { mOptions = aOptions; }
  void Set(VideoFrameRate aRate) { mRate = aRate; }
  void Set(layers::KnowsCompositor* aKnowsCompositor)
  {
    if (aKnowsCompositor) {
      mKnowsCompositor = aKnowsCompositor;
      MOZ_ASSERT(aKnowsCompositor->IsThreadSafe());
    }
  }
  void Set(TrackInfo::TrackType aType)
  {
    mType = aType;
  }
  void Set(MediaEventProducer<TrackInfo::TrackType>* aOnWaitingForKey)
  {
    mOnWaitingForKeyEvent = aOnWaitingForKey;
  }
  template <typename T1, typename T2, typename... Ts>
  void Set(T1&& a1, T2&& a2, Ts&&... args)
  {
    Set(std::forward<T1>(a1));
    Set(std::forward<T2>(a2), std::forward<Ts>(args)...);
  }
};

// The PlatformDecoderModule interface is used by the MediaFormatReader to
// abstract access to decoders provided by various
// platforms.
// Each platform (Windows, MacOSX, Linux, B2G etc) must implement a
// PlatformDecoderModule to provide access to its decoders in order to get
// decompressed H.264/AAC from the MediaFormatReader.
//
// Decoding is asynchronous, and should be performed on the task queue
// provided if the underlying platform isn't already exposing an async API.
//
// A cross-platform decoder module that discards input and produces "blank"
// output samples exists for testing, and is created when the pref
// "media.use-blank-decoder" is true.

class PlatformDecoderModule
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformDecoderModule)

  // Perform any per-instance initialization.
  // This is called on the decode task queue.
  virtual nsresult Startup() { return NS_OK; }

  // Indicates if the PlatformDecoderModule supports decoding of aMimeType.
  virtual bool
  SupportsMimeType(const nsACString& aMimeType,
                   DecoderDoctorDiagnostics* aDiagnostics) const = 0;

  virtual bool
  Supports(const TrackInfo& aTrackInfo,
           DecoderDoctorDiagnostics* aDiagnostics) const
  {
    if (!SupportsMimeType(aTrackInfo.mMimeType, aDiagnostics)) {
      return false;
    }
    const auto videoInfo = aTrackInfo.GetAsVideoInfo();
    return !videoInfo || SupportsBitDepth(videoInfo->mBitDepth, aDiagnostics);
  }

protected:
  PlatformDecoderModule() { }
  virtual ~PlatformDecoderModule() { }

  friend class H264Converter;
  friend class PDMFactory;
  friend class dom::RemoteDecoderModule;
  friend class EMEDecoderModule;

  // Indicates if the PlatformDecoderModule supports decoding of aBitDepth.
  // Should override this method when the platform can support bitDepth != 8.
  virtual bool SupportsBitDepth(const uint8_t aBitDepth,
                                DecoderDoctorDiagnostics* aDiagnostics) const
  {
    return aBitDepth == 8;
  }

  // Creates a Video decoder. The layers backend is passed in so that
  // decoders can determine whether hardware accelerated decoding can be used.
  // Asynchronous decoding of video should be done in runnables dispatched
  // to aVideoTaskQueue. If the task queue isn't needed, the decoder should
  // not hold a reference to it.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // Returns nullptr if the decoder can't be created.
  // It is safe to store a reference to aConfig.
  // This is called on the decode task queue.
  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) = 0;

  // Creates an Audio decoder with the specified properties.
  // Asynchronous decoding of audio should be done in runnables dispatched to
  // aAudioTaskQueue. If the task queue isn't needed, the decoder should
  // not hold a reference to it.
  // Returns nullptr if the decoder can't be created.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // It is safe to store a reference to aConfig.
  // This is called on the decode task queue.
  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) = 0;
};

DDLoggedTypeDeclName(MediaDataDecoder);

// MediaDataDecoder is the interface exposed by decoders created by the
// PlatformDecoderModule's Create*Decoder() functions. The type of
// media data that the decoder accepts as valid input and produces as
// output is determined when the MediaDataDecoder is created.
//
// Unless otherwise noted, all functions are only called on the decode task
// queue.  An exception is the MediaDataDecoder in
// MediaFormatReader::IsVideoAccelerated() for which all calls (Init(),
// IsHardwareAccelerated(), and Shutdown()) are from the main thread.
//
// Don't block inside these functions, unless it's explicitly noted that you
// should (like in Flush()).
//
// Decoding is done asynchronously. Any async work can be done on the
// TaskQueue passed into the PlatformDecoderModules's Create*Decoder()
// function. This may not be necessary for platforms with async APIs
// for decoding.
class MediaDataDecoder : public DecoderDoctorLifeLogger<MediaDataDecoder>
{
protected:
  virtual ~MediaDataDecoder() { }

public:
  typedef TrackInfo::TrackType TrackType;
  typedef nsTArray<RefPtr<MediaData>> DecodedData;
  typedef MozPromise<TrackType, MediaResult, /* IsExclusive = */ true>
    InitPromise;
  typedef MozPromise<DecodedData, MediaResult, /* IsExclusive = */ true>
    DecodePromise;
  typedef MozPromise<bool, MediaResult, /* IsExclusive = */ true> FlushPromise;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDecoder)

  // Initialize the decoder. The decoder should be ready to decode once
  // promise resolves. The decoder should do any initialization here, rather
  // than in its constructor or PlatformDecoderModule::Create*Decoder(),
  // so that if the MediaFormatReader needs to shutdown during initialization,
  // it can call Shutdown() to cancel this operation. Any initialization
  // that requires blocking the calling thread in this function *must*
  // be done here so that it can be canceled by calling Shutdown()!
  virtual RefPtr<InitPromise> Init() = 0;

  // Inserts a sample into the decoder's decode pipeline. The DecodePromise will
  // be resolved with the decoded MediaData. In case the decoder needs more
  // input, the DecodePromise may be resolved with an empty array of samples to
  // indicate that Decode should be called again before a MediaData is returned.
  virtual RefPtr<DecodePromise> Decode(MediaRawData* aSample) = 0;

  // Causes all complete samples in the pipeline that can be decoded to be
  // output. If the decoder can't produce samples from the current output,
  // it drops the input samples. The decoder may be holding onto samples
  // that are required to decode samples that it expects to get in future.
  // This is called when the demuxer reaches end of stream.
  // This function is asynchronous.
  // The MediaDataDecoder shall resolve the pending DecodePromise with drained
  // samples. Drain will be called multiple times until the resolved
  // DecodePromise is empty which indicates that there are no more samples to
  // drain.
  virtual RefPtr<DecodePromise> Drain() = 0;

  // Causes all samples in the decoding pipeline to be discarded. When this
  // promise resolves, the decoder must be ready to accept new data for
  // decoding. This function is called when the demuxer seeks, before decoding
  // resumes after the seek. The current DecodePromise if any shall be rejected
  // with NS_ERROR_DOM_MEDIA_CANCELED
  virtual RefPtr<FlushPromise> Flush() = 0;

  // Cancels all init/decode/drain operations, and shuts down the decoder. The
  // platform decoder should clean up any resources it's using and release
  // memory etc. The shutdown promise will be resolved once the decoder has
  // completed shutdown. The reader calls Flush() before calling Shutdown(). The
  // reader will delete the decoder once the promise is resolved.
  // The ShutdownPromise must only ever be resolved.
  virtual RefPtr<ShutdownPromise> Shutdown() = 0;

  // Called from the state machine task queue or main thread. Decoder needs to
  // decide whether or not hardware acceleration is supported after creating.
  // It doesn't need to call Init() before calling this function.
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const
  {
    return false;
  }

  // Return the name of the MediaDataDecoder, only used for decoding.
  // May be accessed in a non thread-safe fashion.
  virtual nsCString GetDescriptionName() const = 0;

  // Set a hint of seek target time to decoder. Decoder will drop any decoded
  // data which pts is smaller than this value. This threshold needs to be clear
  // after reset decoder.
  // Decoder may not honor this value. However, it'd be better that
  // video decoder implements this API to improve seek performance.
  // Note: it should be called before Input() or after Flush().
  virtual void SetSeekThreshold(const media::TimeUnit& aTime) { }

  // When playing adaptive playback, recreating an Android video decoder will
  // cause the transition not smooth during resolution change.
  // Reuse the decoder if the decoder support recycling.
  // Currently, only Android video decoder will return true.
  virtual bool SupportDecoderRecycling() const { return false; }

  enum class ConversionRequired
  {
    kNeedNone = 0,
    kNeedAVCC = 1,
    kNeedAnnexB = 2,
  };

  // Indicates that the decoder requires a specific format.
  // The demuxed data will be converted accordingly before feeding it to
  // Decode().
  virtual ConversionRequired NeedsConversion() const
  {
    return ConversionRequired::kNeedNone;
  }
};

} // namespace mozilla

#endif
