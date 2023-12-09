/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#  define PlatformDecoderModule_h_

#  include "DecoderDoctorLogger.h"
#  include "GMPCrashHelper.h"
#  include "MediaCodecsSupport.h"
#  include "MediaEventSource.h"
#  include "MediaInfo.h"
#  include "MediaResult.h"
#  include "mozilla/EnumSet.h"
#  include "mozilla/EnumTypeTraits.h"
#  include "mozilla/MozPromise.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/TaskQueue.h"
#  include "mozilla/layers/KnowsCompositor.h"
#  include "mozilla/layers/LayersTypes.h"
#  include "mozilla/ipc/UtilityAudioDecoder.h"
#  include "nsTArray.h"
#  include "PerformanceRecorder.h"

namespace mozilla {
class TrackInfo;
class AudioInfo;
class VideoInfo;
class MediaRawData;
class DecoderDoctorDiagnostics;

namespace layers {
class ImageContainer;
}  // namespace layers

class MediaDataDecoder;
class RemoteDecoderModule;
class CDMProxy;

static LazyLogModule sPDMLog("PlatformDecoderModule");

namespace media {

enum class Option {
  Default,
  LowLatency,
  HardwareDecoderNotAllowed,
  FullH264Parsing,
  ErrorIfNoInitializationData,  // By default frames delivered before
                                // initialization data are dropped. Pass this
                                // option to raise an error if frames are
                                // delivered before initialization data.
  DefaultPlaybackDeviceMono,    // Currently only used by Opus on RDD to avoid
                                // initialization of audio backends on RDD

  SENTINEL  // one past the last valid value
};
using OptionSet = EnumSet<Option>;

struct UseNullDecoder {
  UseNullDecoder() = default;
  explicit UseNullDecoder(bool aUseNullDecoder) : mUse(aUseNullDecoder) {}
  bool mUse = false;
};

// Do not wrap H264 decoder in a H264Converter.
struct NoWrapper {
  NoWrapper() = default;
  explicit NoWrapper(bool aDontUseWrapper) : mDontUseWrapper(aDontUseWrapper) {}
  bool mDontUseWrapper = false;
};

struct VideoFrameRate {
  VideoFrameRate() = default;
  explicit VideoFrameRate(float aFramerate) : mValue(aFramerate) {}
  float mValue = 0.0f;
};

}  // namespace media

struct CreateDecoderParams;
struct CreateDecoderParamsForAsync {
  using Option = media::Option;
  using OptionSet = media::OptionSet;
  explicit CreateDecoderParamsForAsync(const CreateDecoderParams& aParams);
  CreateDecoderParamsForAsync(CreateDecoderParamsForAsync&& aParams);

  const VideoInfo& VideoConfig() const {
    MOZ_ASSERT(mConfig->IsVideo());
    return *mConfig->GetAsVideoInfo();
  }

  const AudioInfo& AudioConfig() const {
    MOZ_ASSERT(mConfig->IsAudio());
    return *mConfig->GetAsAudioInfo();
  }

  UniquePtr<TrackInfo> mConfig;
  const RefPtr<layers::ImageContainer> mImageContainer;
  const RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  const RefPtr<GMPCrashHelper> mCrashHelper;
  const media::UseNullDecoder mUseNullDecoder;
  const media::NoWrapper mNoWrapper;
  const TrackInfo::TrackType mType = TrackInfo::kUndefinedTrack;
  std::function<MediaEventProducer<TrackInfo::TrackType>*()>
      mOnWaitingForKeyEvent;
  const OptionSet mOptions = OptionSet(Option::Default);
  const media::VideoFrameRate mRate;
  const Maybe<uint64_t> mMediaEngineId;
  const Maybe<TrackingId> mTrackingId;
};

struct MOZ_STACK_CLASS CreateDecoderParams final {
  using Option = media::Option;
  using OptionSet = media::OptionSet;
  using UseNullDecoder = media::UseNullDecoder;
  using NoWrapper = media::NoWrapper;
  using VideoFrameRate = media::VideoFrameRate;

  explicit CreateDecoderParams(const TrackInfo& aConfig) : mConfig(aConfig) {}
  CreateDecoderParams(const CreateDecoderParams& aParams) = default;

  MOZ_IMPLICIT CreateDecoderParams(const CreateDecoderParamsForAsync& aParams)
      : mConfig(*aParams.mConfig),
        mImageContainer(aParams.mImageContainer),
        mKnowsCompositor(aParams.mKnowsCompositor),
        mCrashHelper(aParams.mCrashHelper),
        mUseNullDecoder(aParams.mUseNullDecoder),
        mNoWrapper(aParams.mNoWrapper),
        mType(aParams.mType),
        mOnWaitingForKeyEvent(aParams.mOnWaitingForKeyEvent),
        mOptions(aParams.mOptions),
        mRate(aParams.mRate),
        mMediaEngineId(aParams.mMediaEngineId),
        mTrackingId(aParams.mTrackingId) {}

  template <typename T1, typename... Ts>
  CreateDecoderParams(const TrackInfo& aConfig, T1&& a1, Ts&&... args)
      : mConfig(aConfig) {
    Set(std::forward<T1>(a1), std::forward<Ts>(args)...);
  }

  template <typename T1, typename... Ts>
  CreateDecoderParams(const CreateDecoderParams& aParams, T1&& a1, Ts&&... args)
      : CreateDecoderParams(aParams) {
    Set(std::forward<T1>(a1), std::forward<Ts>(args)...);
  }

  const VideoInfo& VideoConfig() const {
    MOZ_ASSERT(mConfig.IsVideo());
    return *mConfig.GetAsVideoInfo();
  }

  const AudioInfo& AudioConfig() const {
    MOZ_ASSERT(mConfig.IsAudio());
    return *mConfig.GetAsAudioInfo();
  }

  layers::LayersBackend GetLayersBackend() const {
    if (mKnowsCompositor) {
      return mKnowsCompositor->GetCompositorBackendType();
    }
    return layers::LayersBackend::LAYERS_NONE;
  }

  // CreateDecoderParams is a MOZ_STACK_CLASS, it is only used to
  // simplify the passing of arguments to Create*Decoder.
  // It is safe to use references and raw pointers.
  const TrackInfo& mConfig;
  layers::ImageContainer* mImageContainer = nullptr;
  MediaResult* mError = nullptr;
  layers::KnowsCompositor* mKnowsCompositor = nullptr;
  GMPCrashHelper* mCrashHelper = nullptr;
  media::UseNullDecoder mUseNullDecoder;
  media::NoWrapper mNoWrapper;
  TrackInfo::TrackType mType = TrackInfo::kUndefinedTrack;
  std::function<MediaEventProducer<TrackInfo::TrackType>*()>
      mOnWaitingForKeyEvent;
  OptionSet mOptions = OptionSet(Option::Default);
  media::VideoFrameRate mRate;
  // Used on Windows when the MF media engine playback is enabled.
  Maybe<uint64_t> mMediaEngineId;
  Maybe<TrackingId> mTrackingId;

 private:
  void Set(layers::ImageContainer* aImageContainer) {
    mImageContainer = aImageContainer;
  }
  void Set(MediaResult* aError) { mError = aError; }
  void Set(GMPCrashHelper* aCrashHelper) { mCrashHelper = aCrashHelper; }
  void Set(UseNullDecoder aUseNullDecoder) {
    mUseNullDecoder = aUseNullDecoder;
  }
  void Set(NoWrapper aNoWrapper) { mNoWrapper = aNoWrapper; }
  void Set(const OptionSet& aOptions) { mOptions = aOptions; }
  void Set(VideoFrameRate aRate) { mRate = aRate; }
  void Set(layers::KnowsCompositor* aKnowsCompositor) {
    if (aKnowsCompositor) {
      mKnowsCompositor = aKnowsCompositor;
      MOZ_ASSERT(aKnowsCompositor->IsThreadSafe());
    }
  }
  void Set(TrackInfo::TrackType aType) { mType = aType; }
  void Set(std::function<MediaEventProducer<TrackInfo::TrackType>*()>&&
               aOnWaitingForKey) {
    mOnWaitingForKeyEvent = std::move(aOnWaitingForKey);
  }
  void Set(const std::function<MediaEventProducer<TrackInfo::TrackType>*()>&
               aOnWaitingForKey) {
    mOnWaitingForKeyEvent = aOnWaitingForKey;
  }
  void Set(const Maybe<uint64_t>& aMediaEngineId) {
    mMediaEngineId = aMediaEngineId;
  }
  void Set(const Maybe<TrackingId>& aTrackingId) { mTrackingId = aTrackingId; }
  void Set(const CreateDecoderParams& aParams) {
    // Set all but mTrackInfo;
    mImageContainer = aParams.mImageContainer;
    mError = aParams.mError;
    mKnowsCompositor = aParams.mKnowsCompositor;
    mCrashHelper = aParams.mCrashHelper;
    mUseNullDecoder = aParams.mUseNullDecoder;
    mNoWrapper = aParams.mNoWrapper;
    mType = aParams.mType;
    mOnWaitingForKeyEvent = aParams.mOnWaitingForKeyEvent;
    mOptions = aParams.mOptions;
    mRate = aParams.mRate;
    mMediaEngineId = aParams.mMediaEngineId;
    mTrackingId = aParams.mTrackingId;
  }
  template <typename T1, typename T2, typename... Ts>
  void Set(T1&& a1, T2&& a2, Ts&&... args) {
    Set(std::forward<T1>(a1));
    Set(std::forward<T2>(a2), std::forward<Ts>(args)...);
  }
};

struct MOZ_STACK_CLASS SupportDecoderParams final {
  using Option = media::Option;
  using OptionSet = media::OptionSet;
  using UseNullDecoder = media::UseNullDecoder;
  using NoWrapper = media::NoWrapper;
  using VideoFrameRate = media::VideoFrameRate;

  explicit SupportDecoderParams(const TrackInfo& aConfig) : mConfig(aConfig) {}

  explicit SupportDecoderParams(const CreateDecoderParams& aParams)
      : mConfig(aParams.mConfig),
        mError(aParams.mError),
        mKnowsCompositor(aParams.mKnowsCompositor),
        mUseNullDecoder(aParams.mUseNullDecoder),
        mNoWrapper(aParams.mNoWrapper),
        mOptions(aParams.mOptions),
        mRate(aParams.mRate),
        mMediaEngineId(aParams.mMediaEngineId) {}

  template <typename T1, typename... Ts>
  SupportDecoderParams(const TrackInfo& aConfig, T1&& a1, Ts&&... args)
      : mConfig(aConfig) {
    Set(std::forward<T1>(a1), std::forward<Ts>(args)...);
  }

  const nsCString& MimeType() const { return mConfig.mMimeType; }

  const TrackInfo& mConfig;
  DecoderDoctorDiagnostics* mDiagnostics = nullptr;
  MediaResult* mError = nullptr;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  UseNullDecoder mUseNullDecoder;
  NoWrapper mNoWrapper;
  OptionSet mOptions = OptionSet(Option::Default);
  VideoFrameRate mRate;
  Maybe<uint64_t> mMediaEngineId;

 private:
  void Set(DecoderDoctorDiagnostics* aDiagnostics) {
    mDiagnostics = aDiagnostics;
  }
  void Set(MediaResult* aError) { mError = aError; }
  void Set(media::UseNullDecoder aUseNullDecoder) {
    mUseNullDecoder = aUseNullDecoder;
  }
  void Set(media::NoWrapper aNoWrapper) { mNoWrapper = aNoWrapper; }
  void Set(const media::OptionSet& aOptions) { mOptions = aOptions; }
  void Set(media::VideoFrameRate aRate) { mRate = aRate; }
  void Set(layers::KnowsCompositor* aKnowsCompositor) {
    if (aKnowsCompositor) {
      mKnowsCompositor = aKnowsCompositor;
      MOZ_ASSERT(aKnowsCompositor->IsThreadSafe());
    }
  }
  void Set(const Maybe<uint64_t>& aMediaEngineId) {
    mMediaEngineId = aMediaEngineId;
  }

  template <typename T1, typename T2, typename... Ts>
  void Set(T1&& a1, T2&& a2, Ts&&... args) {
    Set(std::forward<T1>(a1));
    Set(std::forward<T2>(a2), std::forward<Ts>(args)...);
  }
};

// Used for IPDL serialization.
// The 'value' have to be the biggest enum from CreateDecoderParams::Option.
template <>
struct MaxEnumValue<::mozilla::CreateDecoderParams::Option> {
  static constexpr unsigned int value =
      static_cast<unsigned int>(CreateDecoderParams::Option::SENTINEL);
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

class PlatformDecoderModule {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformDecoderModule)

  // Perform any per-instance initialization.
  // This is called on the decode task queue.
  virtual nsresult Startup() { return NS_OK; }

  // Indicates if the PlatformDecoderModule supports decoding of aMimeType,
  // and whether or not hardware-accelerated decoding is supported.
  // The answer to both SupportsMimeType and Supports doesn't guarantee that
  // creation of a decoder will actually succeed.
  virtual media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const = 0;

  virtual media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const {
    const TrackInfo& trackInfo = aParams.mConfig;
    const media::DecodeSupportSet support =
        SupportsMimeType(trackInfo.mMimeType, aDiagnostics);

    // Bail early if we don't support this format at all
    if (support.isEmpty()) {
      return support;
    }

    const auto* videoInfo = trackInfo.GetAsVideoInfo();

    if (!videoInfo) {
      // No video stream = software decode only
      return media::DecodeSupport::SoftwareDecode;
    }

    // Check whether we support the desired color depth
    if (!SupportsColorDepth(videoInfo->mColorDepth, aDiagnostics)) {
      return media::DecodeSupportSet{};
    }
    return support;
  }

  using CreateDecoderPromise = MozPromise<RefPtr<MediaDataDecoder>, MediaResult,
                                          /* IsExclusive = */ true>;

 protected:
  PlatformDecoderModule() = default;
  virtual ~PlatformDecoderModule() = default;

  friend class MediaChangeMonitor;
  friend class PDMFactory;
  friend class EMEDecoderModule;
  friend class RemoteDecoderModule;

  // Indicates if the PlatformDecoderModule supports decoding of aColorDepth.
  // Should override this method when the platform can support color depth != 8.
  virtual bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const {
    return aColorDepth == gfx::ColorDepth::COLOR_8;
  }

  // Creates a Video decoder. The layers backend is passed in so that
  // decoders can determine whether hardware accelerated decoding can be used.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // Returns nullptr if the decoder can't be created.
  // It is not safe to store a reference to aParams or aParams.mConfig as the
  // object isn't guaranteed to live after the call.
  // CreateVideoDecoder may need to make additional checks if the
  // CreateDecoderParams argument is actually supported and return nullptr if
  // not to allow for fallback PDMs to be tried.
  virtual already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) = 0;

  // Creates an Audio decoder with the specified properties.
  // Returns nullptr if the decoder can't be created.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // It is not safe to store a reference to aParams or aParams.mConfig as the
  // object isn't guaranteed to live after the call.
  // CreateAudioDecoder may need to make additional checks if the
  // CreateDecoderParams argument is actually supported and return nullptr if
  // not to allow for fallback PDMs to be tried.
  virtual already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) = 0;

  // Asychronously create a decoder.
  virtual RefPtr<CreateDecoderPromise> AsyncCreateDecoder(
      const CreateDecoderParams& aParams);
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
// Decoding is done asynchronously.
class MediaDataDecoder : public DecoderDoctorLifeLogger<MediaDataDecoder> {
 protected:
  virtual ~MediaDataDecoder() = default;

 public:
  using TrackType = TrackInfo::TrackType;
  using DecodedData = nsTArray<RefPtr<MediaData>>;
  using InitPromise = MozPromise<TrackType, MediaResult, true>;
  using DecodePromise = MozPromise<DecodedData, MediaResult, true>;
  using FlushPromise = MozPromise<bool, MediaResult, true>;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Initialize the decoder. The decoder should be ready to decode once
  // the promise resolves. The decoder should do any initialization here, rather
  // than in its constructor or PlatformDecoderModule::Create*Decoder(),
  // so that if the MediaFormatReader needs to shutdown during initialization,
  // it can call Shutdown() to cancel this operation. Any initialization
  // that requires blocking the calling thread in this function *must*
  // be done here so that it can be canceled by calling Shutdown()!
  // Methods Decode, DecodeBatch, Drain, Flush, Shutdown are guaranteed to be
  // called on the thread where Init() first ran.
  virtual RefPtr<InitPromise> Init() = 0;

  // Inserts a sample into the decoder's decode pipeline. The DecodePromise will
  // be resolved with the decoded MediaData. In case the decoder needs more
  // input, the DecodePromise may be resolved with an empty array of samples to
  // indicate that Decode should be called again before a MediaData is returned.
  virtual RefPtr<DecodePromise> Decode(MediaRawData* aSample) = 0;

  // This could probably be implemented as a wrapper that takes a
  // generic MediaDataDecoder and manages batching as needed.  For now
  // only AudioTrimmer with RemoteMediaDataDecoder supports batch
  // decoding.
  // Inserts an array of samples into the decoder's decode pipeline. The
  // DecodePromise will be resolved with the decoded MediaData. In case
  // the decoder needs more input, the DecodePromise may be resolved
  // with an empty array of samples to indicate that Decode should be
  // called again before a MediaData is returned.
  virtual bool CanDecodeBatch() const { return false; }
  virtual RefPtr<DecodePromise> DecodeBatch(
      nsTArray<RefPtr<MediaRawData>>&& aSamples) {
    MOZ_CRASH("DecodeBatch not implemented yet");
    return MediaDataDecoder::DecodePromise::CreateAndReject(
        NS_ERROR_DOM_MEDIA_DECODE_ERR, __func__);
  }

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
  // Shutdown() may not be called if init hasn't been called first. It is
  // possible under some circumstances for the decoder to be deleted without
  // Init having been called first.
  virtual RefPtr<ShutdownPromise> Shutdown() = 0;

  // Called from the state machine task queue or main thread. Decoder needs to
  // decide whether or not hardware acceleration is supported after creating.
  // It doesn't need to call Init() before calling this function.
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const {
    return false;
  }

  // Return the name of the MediaDataDecoder, only used for decoding.
  // May be accessed in a non thread-safe fashion.
  virtual nsCString GetDescriptionName() const = 0;

  virtual nsCString GetProcessName() const {
    nsCString rv = nsCString(XRE_GetProcessTypeString());
    if (XRE_IsUtilityProcess()) {
      rv += "+"_ns + mozilla::ipc::GetChildAudioActorName();
    }
    return rv;
  };
  virtual nsCString GetCodecName() const = 0;

  // Set a hint of seek target time to decoder. Decoder will drop any decoded
  // data which pts is smaller than this value. This threshold needs to be clear
  // after reset decoder. To clear it explicitly, call this method with
  // TimeUnit::Invalid().
  // Decoder may not honor this value. However, it'd be better that
  // video decoder implements this API to improve seek performance.
  // Note: it should be called before Input() or after Flush().
  virtual void SetSeekThreshold(const media::TimeUnit& aTime) {}

  // When playing adaptive playback, recreating an Android video decoder will
  // cause the transition not smooth during resolution change.
  // Reuse the decoder if the decoder support recycling.
  // Currently, only Android video decoder will return true.
  virtual bool SupportDecoderRecycling() const { return false; }

  enum class ConversionRequired {
    kNeedNone = 0,
    kNeedAVCC = 1,
    kNeedAnnexB = 2,
  };

  // Indicates that the decoder requires a specific format.
  // The demuxed data will be converted accordingly before feeding it to
  // Decode().
  virtual ConversionRequired NeedsConversion() const {
    return ConversionRequired::kNeedNone;
  }
};

}  // namespace mozilla

#endif
