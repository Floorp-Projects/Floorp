/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCapabilities.h"

#include <inttypes.h>

#include <utility>

#include "AllocationPolicy.h"
#include "Benchmark.h"
#include "DecoderBenchmark.h"
#include "DecoderTraits.h"
#include "MediaInfo.h"
#include "MediaRecorder.h"
#include "PDMFactory.h"
#include "VPXDecoder.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/MediaCapabilitiesBinding.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "nsContentUtils.h"
#include "WindowRenderer.h"

static mozilla::LazyLogModule sMediaCapabilitiesLog("MediaCapabilities");

#define LOG(msg, ...) \
  DDMOZ_LOG(sMediaCapabilitiesLog, LogLevel::Debug, msg, ##__VA_ARGS__)

namespace mozilla::dom {

static nsCString VideoConfigurationToStr(const VideoConfiguration* aConfig) {
  if (!aConfig) {
    return nsCString();
  }

  nsCString hdrMetaType(
      aConfig->mHdrMetadataType.WasPassed()
          ? HdrMetadataTypeValues::GetString(aConfig->mHdrMetadataType.Value())
          : "?");

  nsCString colorGamut(
      aConfig->mColorGamut.WasPassed()
          ? ColorGamutValues::GetString(aConfig->mColorGamut.Value())
          : "?");

  nsCString transferFunction(aConfig->mTransferFunction.WasPassed()
                                 ? TransferFunctionValues::GetString(
                                       aConfig->mTransferFunction.Value())
                                 : "?");

  auto str = nsPrintfCString(
      "[contentType:%s width:%d height:%d bitrate:%" PRIu64
      " framerate:%lf hasAlphaChannel:%s hdrMetadataType:%s colorGamut:%s "
      "transferFunction:%s scalabilityMode:%s]",
      NS_ConvertUTF16toUTF8(aConfig->mContentType).get(), aConfig->mWidth,
      aConfig->mHeight, aConfig->mBitrate, aConfig->mFramerate,
      aConfig->mHasAlphaChannel.WasPassed()
          ? aConfig->mHasAlphaChannel.Value() ? "true" : "false"
          : "?",
      hdrMetaType.get(), colorGamut.get(), transferFunction.get(),
      aConfig->mScalabilityMode.WasPassed()
          ? NS_ConvertUTF16toUTF8(aConfig->mScalabilityMode.Value()).get()
          : "?");
  return std::move(str);
}

static nsCString AudioConfigurationToStr(const AudioConfiguration* aConfig) {
  if (!aConfig) {
    return nsCString();
  }
  auto str = nsPrintfCString(
      "[contentType:%s channels:%s bitrate:%" PRIu64 " samplerate:%d]",
      NS_ConvertUTF16toUTF8(aConfig->mContentType).get(),
      aConfig->mChannels.WasPassed()
          ? NS_ConvertUTF16toUTF8(aConfig->mChannels.Value()).get()
          : "?",
      aConfig->mBitrate.WasPassed() ? aConfig->mBitrate.Value() : 0,
      aConfig->mSamplerate.WasPassed() ? aConfig->mSamplerate.Value() : 0);
  return std::move(str);
}

static nsCString MediaCapabilitiesInfoToStr(
    const MediaCapabilitiesInfo* aInfo) {
  if (!aInfo) {
    return nsCString();
  }
  auto str = nsPrintfCString("[supported:%s smooth:%s powerEfficient:%s]",
                             aInfo->Supported() ? "true" : "false",
                             aInfo->Smooth() ? "true" : "false",
                             aInfo->PowerEfficient() ? "true" : "false");
  return std::move(str);
}

static nsCString MediaDecodingConfigurationToStr(
    const MediaDecodingConfiguration& aConfig) {
  nsCString str;
  str += "["_ns;
  if (aConfig.mVideo.WasPassed()) {
    str += "video:"_ns + VideoConfigurationToStr(&aConfig.mVideo.Value());
    if (aConfig.mAudio.WasPassed()) {
      str += " "_ns;
    }
  }
  if (aConfig.mAudio.WasPassed()) {
    str += "audio:"_ns + AudioConfigurationToStr(&aConfig.mAudio.Value());
  }
  str += "]"_ns;
  return str;
}

MediaCapabilities::MediaCapabilities(nsIGlobalObject* aParent)
    : mParent(aParent) {}

already_AddRefed<Promise> MediaCapabilities::DecodingInfo(
    const MediaDecodingConfiguration& aConfiguration, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.mVideo.WasPassed() &&
      !aConfiguration.mAudio.WasPassed()) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
        "'audio' or 'video' member of argument of "
        "MediaCapabilities.decodingInfo");
    return nullptr;
  }

  LOG("Processing %s", MediaDecodingConfigurationToStr(aConfiguration).get());

  bool supported = true;
  Maybe<MediaContainerType> videoContainer;
  Maybe<MediaContainerType> audioContainer;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.WasPassed()) {
    videoContainer = CheckVideoConfiguration(aConfiguration.mVideo.Value());
    if (!videoContainer) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }

    // We have a video configuration and it is valid. Check if it is supported.
    supported &=
        aConfiguration.mType == MediaDecodingType::File
            ? CheckTypeForFile(aConfiguration.mVideo.Value().mContentType)
            : CheckTypeForMediaSource(
                  aConfiguration.mVideo.Value().mContentType);
  }
  if (aConfiguration.mAudio.WasPassed()) {
    audioContainer = CheckAudioConfiguration(aConfiguration.mAudio.Value());
    if (!audioContainer) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &=
        aConfiguration.mType == MediaDecodingType::File
            ? CheckTypeForFile(aConfiguration.mAudio.Value().mContentType)
            : CheckTypeForMediaSource(
                  aConfiguration.mAudio.Value().mContentType);
  }

  if (!supported) {
    auto info = MakeUnique<MediaCapabilitiesInfo>(
        false /* supported */, false /* smooth */, false /* power efficient */);
    LOG("%s -> %s", MediaDecodingConfigurationToStr(aConfiguration).get(),
        MediaCapabilitiesInfoToStr(info.get()).get());
    promise->MaybeResolve(std::move(info));
    return promise.forget();
  }

  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (aConfiguration.mVideo.WasPassed()) {
    MOZ_ASSERT(videoContainer.isSome(), "configuration is valid and supported");
    auto videoTracks = DecoderTraits::GetTracksInfo(*videoContainer);
    // If the MIME type does not imply a codec, the string MUST
    // also have one and only one parameter that is named codecs with a value
    // describing a single media codec. Otherwise, it MUST contain no
    // parameters.
    if (videoTracks.Length() != 1) {
      promise->MaybeRejectWithTypeError<MSG_NO_CODECS_PARAMETER>(
          videoContainer->OriginalString());
      return promise.forget();
    }
    MOZ_DIAGNOSTIC_ASSERT(videoTracks.ElementAt(0),
                          "must contain a valid trackinfo");
    // If the type refers to an audio codec, reject now.
    if (videoTracks[0]->GetType() != TrackInfo::kVideoTrack) {
      promise
          ->MaybeRejectWithTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return promise.forget();
    }
    tracks.AppendElements(std::move(videoTracks));
  }
  if (aConfiguration.mAudio.WasPassed()) {
    MOZ_ASSERT(audioContainer.isSome(), "configuration is valid and supported");
    auto audioTracks = DecoderTraits::GetTracksInfo(*audioContainer);
    // If the MIME type does not imply a codec, the string MUST
    // also have one and only one parameter that is named codecs with a value
    // describing a single media codec. Otherwise, it MUST contain no
    // parameters.
    if (audioTracks.Length() != 1) {
      promise->MaybeRejectWithTypeError<MSG_NO_CODECS_PARAMETER>(
          audioContainer->OriginalString());
      return promise.forget();
    }
    MOZ_DIAGNOSTIC_ASSERT(audioTracks.ElementAt(0),
                          "must contain a valid trackinfo");
    // If the type refers to a video codec, reject now.
    if (audioTracks[0]->GetType() != TrackInfo::kAudioTrack) {
      promise
          ->MaybeRejectWithTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return promise.forget();
    }
    tracks.AppendElements(std::move(audioTracks));
  }

  using CapabilitiesPromise = MozPromise<MediaCapabilitiesInfo, MediaResult,
                                         /* IsExclusive = */ true>;
  nsTArray<RefPtr<CapabilitiesPromise>> promises;

  RefPtr<TaskQueue> taskQueue =
      TaskQueue::Create(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "MediaCapabilities::TaskQueue");
  for (auto&& config : tracks) {
    TrackInfo::TrackType type =
        config->IsVideo() ? TrackInfo::kVideoTrack : TrackInfo::kAudioTrack;

    MOZ_ASSERT(type == TrackInfo::kAudioTrack ||
                   videoContainer->ExtendedType().GetFramerate().isSome(),
               "framerate is a required member of VideoConfiguration");

    if (type == TrackInfo::kAudioTrack) {
      // There's no need to create an audio decoder has we only want to know if
      // such codec is supported. We do need to call the PDMFactory::Supports
      // API outside the main thread to get accurate results.
      promises.AppendElement(
          InvokeAsync(taskQueue, __func__, [config = std::move(config)]() {
            RefPtr<PDMFactory> pdm = new PDMFactory();
            SupportDecoderParams params{*config};
            if (pdm->Supports(params, nullptr /* decoder doctor */).isEmpty()) {
              return CapabilitiesPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                          __func__);
            }
            return CapabilitiesPromise::CreateAndResolve(
                MediaCapabilitiesInfo(true /* supported */, true /* smooth */,
                                      true /* power efficient */),
                __func__);
          }));
      continue;
    }

    // On Windows, the MediaDataDecoder expects to be created on a thread
    // supporting MTA, which the main thread doesn't. So we use our task queue
    // to create such decoder and perform initialization.

    RefPtr<layers::KnowsCompositor> compositor = GetCompositor();
    float frameRate =
        static_cast<float>(videoContainer->ExtendedType().GetFramerate().ref());
    const bool shouldResistFingerprinting =
        mParent->ShouldResistFingerprinting(RFPTarget::MediaCapabilities);

    // clang-format off
    promises.AppendElement(InvokeAsync(
        taskQueue, __func__,
        [taskQueue, frameRate, shouldResistFingerprinting, compositor,
         config = std::move(config)]() mutable -> RefPtr<CapabilitiesPromise> {
          // MediaDataDecoder keeps a reference to the config object, so we must
          // keep it alive until the decoder has been shutdown.
          static Atomic<uint32_t> sTrackingIdCounter(0);
          TrackingId trackingId(TrackingId::Source::MediaCapabilities,
                                sTrackingIdCounter++,
                                TrackingId::TrackAcrossProcesses::Yes);
          CreateDecoderParams params{
              *config, compositor,
              CreateDecoderParams::VideoFrameRate(frameRate),
              TrackInfo::kVideoTrack, Some(std::move(trackingId))};
          // We want to ensure that all decoder's queries are occurring only
          // once at a time as it can quickly exhaust the system resources
          // otherwise.
          static RefPtr<AllocPolicy> sVideoAllocPolicy = [&taskQueue]() {
            SchedulerGroup::Dispatch(
                NS_NewRunnableFunction(
                    "MediaCapabilities::AllocPolicy:Video", []() {
                      ClearOnShutdown(&sVideoAllocPolicy,
                                      ShutdownPhase::XPCOMShutdownThreads);
                    }));
            return new SingleAllocPolicy(TrackInfo::TrackType::kVideoTrack,
                                         taskQueue);
          }();
          return AllocationWrapper::CreateDecoder(params, sVideoAllocPolicy)
              ->Then(
                  taskQueue, __func__,
                  [taskQueue, frameRate, shouldResistFingerprinting,
                   config = std::move(config)](
                      AllocationWrapper::AllocateDecoderPromise::
                          ResolveOrRejectValue&& aValue) mutable {
                    if (aValue.IsReject()) {
                      return CapabilitiesPromise::CreateAndReject(
                          std::move(aValue.RejectValue()), __func__);
                    }
                    RefPtr<MediaDataDecoder> decoder =
                        std::move(aValue.ResolveValue());
                    // We now query the decoder to determine if it's power
                    // efficient.
                    RefPtr<CapabilitiesPromise> p = decoder->Init()->Then(
                        taskQueue, __func__,
                        [taskQueue, decoder, frameRate,
                         shouldResistFingerprinting,
                         config = std::move(config)](
                            MediaDataDecoder::InitPromise::
                                ResolveOrRejectValue&& aValue) mutable {
                          RefPtr<CapabilitiesPromise> p;
                          if (aValue.IsReject()) {
                            p = CapabilitiesPromise::CreateAndReject(
                                std::move(aValue.RejectValue()), __func__);
                          } else if (shouldResistFingerprinting) {
                            p = CapabilitiesPromise::CreateAndResolve(
                                MediaCapabilitiesInfo(true /* supported */,
                                true /* smooth */, false /* power efficient */),
                                __func__);
                          } else {
                            MOZ_ASSERT(config->IsVideo());
                            if (StaticPrefs::media_mediacapabilities_from_database()) {
                              nsAutoCString reason;
                              bool powerEfficient =
                                  decoder->IsHardwareAccelerated(reason);

                              int32_t videoFrameRate = std::clamp<int32_t>(frameRate, 1, INT32_MAX);

                              DecoderBenchmarkInfo benchmarkInfo{
                                  config->mMimeType,
                                  config->GetAsVideoInfo()->mImage.width,
                                  config->GetAsVideoInfo()->mImage.height,
                                  videoFrameRate, 8};

                              p = DecoderBenchmark::Get(benchmarkInfo)->Then(
                                  GetMainThreadSerialEventTarget(),
                                  __func__,
                                  [powerEfficient](int32_t score) {
                                    // score < 0 means no entry found.
                                    bool smooth = score < 0 || score >
                                      StaticPrefs::
                                        media_mediacapabilities_drop_threshold();
                                    return CapabilitiesPromise::
                                        CreateAndResolve(
                                            MediaCapabilitiesInfo(
                                                true, smooth,
                                                powerEfficient),
                                            __func__);
                                  },
                                  [](nsresult rv) {
                                    return CapabilitiesPromise::
                                        CreateAndReject(rv, __func__);
                                  });
                            } else if (config->GetAsVideoInfo()->mImage.height < 480) {
                              // Assume that we can do stuff at 480p or less in
                              // a power efficient manner and smoothly. If
                              // greater than 480p we assume that if the video
                              // decoding is hardware accelerated it will be
                              // smooth and power efficient, otherwise we use
                              // the benchmark to estimate
                              p = CapabilitiesPromise::CreateAndResolve(
                                  MediaCapabilitiesInfo(true, true, true),
                                  __func__);
                            } else {
                              nsAutoCString reason;
                              bool smooth = true;
                              bool powerEfficient =
                                  decoder->IsHardwareAccelerated(reason);
                              if (!powerEfficient &&
                                  VPXDecoder::IsVP9(config->mMimeType)) {
                                smooth = VP9Benchmark::IsVP9DecodeFast(
                                    true /* default */);
                                uint32_t fps =
                                    VP9Benchmark::MediaBenchmarkVp9Fps();
                                if (!smooth && fps > 0) {
                                  // The VP9 estimizer decode a 1280x720 video.
                                  // Let's adjust the result for the resolution
                                  // and frame rate of what we actually want. If
                                  // the result is twice that we need we assume
                                  // it will be smooth.
                                  const auto& videoConfig =
                                      *config->GetAsVideoInfo();
                                  double needed = ((1280.0 * 720.0) /
                                                   (videoConfig.mImage.width *
                                                    videoConfig.mImage.height) *
                                                   fps) /
                                                  frameRate;
                                  smooth = needed > 2;
                                }
                              }

                              p = CapabilitiesPromise::CreateAndResolve(
                                  MediaCapabilitiesInfo(true /* supported */,
                                                        smooth, powerEfficient),
                                  __func__);
                            }
                          }
                          MOZ_ASSERT(p.get(), "the promise has been created");
                          // Let's keep alive the decoder and the config object
                          // until the decoder has shutdown.
                          decoder->Shutdown()->Then(
                              taskQueue, __func__,
                              [taskQueue, decoder, config = std::move(config)](
                                  const ShutdownPromise::ResolveOrRejectValue&
                                      aValue) {});
                          return p;
                        });
                    return p;
                  });
        }));
    // clang-format on
  }

  auto holder = MakeRefPtr<
      DOMMozPromiseRequestHolder<CapabilitiesPromise::AllPromiseType>>(mParent);
  RefPtr<nsISerialEventTarget> targetThread;
  RefPtr<StrongWorkerRef> workerRef;

  if (NS_IsMainThread()) {
    targetThread = GetMainThreadSerialEventTarget();
  } else {
    WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(wp, "Must be called from a worker thread");
    targetThread = wp->HybridEventTarget();
    workerRef = StrongWorkerRef::Create(
        wp, "MediaCapabilities", [holder, targetThread]() {
          MOZ_ASSERT(targetThread->IsOnCurrentThread());
          holder->DisconnectIfExists();
        });
    if (NS_WARN_IF(!workerRef)) {
      // The worker is shutting down.
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  MOZ_ASSERT(targetThread);

  // this is only captured for use with the LOG macro.
  RefPtr<MediaCapabilities> self = this;

  CapabilitiesPromise::All(targetThread, promises)
      ->Then(targetThread, __func__,
             [promise, tracks = std::move(tracks), workerRef, holder,
              aConfiguration, self,
              this](CapabilitiesPromise::AllPromiseType::ResolveOrRejectValue&&
                        aValue) {
               holder->Complete();
               if (aValue.IsReject()) {
                 auto info = MakeUnique<MediaCapabilitiesInfo>(
                     false /* supported */, false /* smooth */,
                     false /* power efficient */);
                 LOG("%s -> %s",
                     MediaDecodingConfigurationToStr(aConfiguration).get(),
                     MediaCapabilitiesInfoToStr(info.get()).get());
                 promise->MaybeResolve(std::move(info));
                 return;
               }
               bool powerEfficient = true;
               bool smooth = true;
               for (auto&& capability : aValue.ResolveValue()) {
                 smooth &= capability.Smooth();
                 powerEfficient &= capability.PowerEfficient();
               }
               auto info = MakeUnique<MediaCapabilitiesInfo>(
                   true /* supported */, smooth, powerEfficient);
               LOG("%s -> %s",
                   MediaDecodingConfigurationToStr(aConfiguration).get(),
                   MediaCapabilitiesInfoToStr(info.get()).get());
               promise->MaybeResolve(std::move(info));
             })
      ->Track(*holder);

  return promise.forget();
}

already_AddRefed<Promise> MediaCapabilities::EncodingInfo(
    const MediaEncodingConfiguration& aConfiguration, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.mVideo.WasPassed() &&
      !aConfiguration.mAudio.WasPassed()) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
        "'audio' or 'video' member of argument of "
        "MediaCapabilities.encodingInfo");
    return nullptr;
  }

  bool supported = true;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.WasPassed()) {
    if (!CheckVideoConfiguration(aConfiguration.mVideo.Value())) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }
    // We have a video configuration and it is valid. Check if it is supported.
    supported &=
        CheckTypeForEncoder(aConfiguration.mVideo.Value().mContentType);
  }
  if (aConfiguration.mAudio.WasPassed()) {
    if (!CheckAudioConfiguration(aConfiguration.mAudio.Value())) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &=
        CheckTypeForEncoder(aConfiguration.mAudio.Value().mContentType);
  }

  auto info = MakeUnique<MediaCapabilitiesInfo>(supported, supported, false);
  promise->MaybeResolve(std::move(info));

  return promise.forget();
}

Maybe<MediaContainerType> MediaCapabilities::CheckVideoConfiguration(
    const VideoConfiguration& aConfig) const {
  Maybe<MediaExtendedMIMEType> container = MakeMediaExtendedMIMEType(aConfig);
  if (!container) {
    return Nothing();
  }
  // A valid video MIME type is a string that is a valid media MIME type and for
  // which the type per [RFC7231] is either video or application.
  if (!container->Type().HasVideoMajorType() &&
      !container->Type().HasApplicationMajorType()) {
    return Nothing();
  }

  // If the MIME type does not imply a codec, the string MUST also have one and
  // only one parameter that is named codecs with a value describing a single
  // media codec. Otherwise, it MUST contain no parameters.
  // TODO (nsIMOMEHeaderParam doesn't provide backend to count number of
  // parameters)

  return Some(MediaContainerType(std::move(*container)));
}

Maybe<MediaContainerType> MediaCapabilities::CheckAudioConfiguration(
    const AudioConfiguration& aConfig) const {
  Maybe<MediaExtendedMIMEType> container = MakeMediaExtendedMIMEType(aConfig);
  if (!container) {
    return Nothing();
  }
  // A valid audio MIME type is a string that is valid media MIME type and for
  // which the type per [RFC7231] is either audio or application.
  if (!container->Type().HasAudioMajorType() &&
      !container->Type().HasApplicationMajorType()) {
    return Nothing();
  }

  // If the MIME type does not imply a codec, the string MUST also have one and
  // only one parameter that is named codecs with a value describing a single
  // media codec. Otherwise, it MUST contain no parameters.
  // TODO (nsIMOMEHeaderParam doesn't provide backend to count number of
  // parameters)

  return Some(MediaContainerType(std::move(*container)));
}

bool MediaCapabilities::CheckTypeForMediaSource(const nsAString& aType) {
  IgnoredErrorResult rv;
  MediaSource::IsTypeSupported(aType, nullptr /* DecoderDoctorDiagnostics */,
                               rv);

  return !rv.Failed();
}

bool MediaCapabilities::CheckTypeForFile(const nsAString& aType) {
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    return false;
  }

  return DecoderTraits::CanHandleContainerType(
             *containerType, nullptr /* DecoderDoctorDiagnostics */) !=
         CANPLAY_NO;
}

bool MediaCapabilities::CheckTypeForEncoder(const nsAString& aType) {
  return MediaRecorder::IsTypeSupported(aType);
}

already_AddRefed<layers::KnowsCompositor> MediaCapabilities::GetCompositor() {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
  if (NS_WARN_IF(!window)) {
    return nullptr;
  }

  nsCOMPtr<Document> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  WindowRenderer* renderer = nsContentUtils::WindowRendererForDocument(doc);
  if (NS_WARN_IF(!renderer)) {
    return nullptr;
  }
  RefPtr<layers::KnowsCompositor> knows = renderer->AsKnowsCompositor();
  if (NS_WARN_IF(!knows)) {
    return nullptr;
  }
  return knows->GetForMedia().forget();
}

bool MediaCapabilities::Enabled(JSContext* aCx, JSObject* aGlobal) {
  return StaticPrefs::media_media_capabilities_enabled();
}

JSObject* MediaCapabilities::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return MediaCapabilities_Binding::Wrap(aCx, this, aGivenProto);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaCapabilities)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MediaCapabilities)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MediaCapabilities)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MediaCapabilities, mParent)

// MediaCapabilitiesInfo
bool MediaCapabilitiesInfo::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
    JS::MutableHandle<JSObject*> aReflector) {
  return MediaCapabilitiesInfo_Binding::Wrap(aCx, this, aGivenProto,
                                             aReflector);
}

}  // namespace mozilla::dom
