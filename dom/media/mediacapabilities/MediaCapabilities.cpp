/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaCapabilities.h"
#include "DecoderTraits.h"
#include "Layers.h"
#include "MediaInfo.h"
#include "MediaRecorder.h"
#include "PDMFactory.h"
#include "mozilla/Move.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/MediaCapabilitiesBinding.h"
#include "mozilla/dom/MediaSource.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

MediaCapabilities::MediaCapabilities(nsIGlobalObject* aParent)
  : mParent(aParent)
{
}

already_AddRefed<Promise>
MediaCapabilities::DecodingInfo(
  const MediaDecodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.IsAnyMemberPresent() ||
      (!aConfiguration.mVideo.IsAnyMemberPresent() &&
       !aConfiguration.mAudio.IsAnyMemberPresent())) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
      NS_LITERAL_STRING("'audio' or 'video'"));
    return nullptr;
  }

  bool supported = true;
  Maybe<MediaContainerType> videoContainer;
  Maybe<MediaContainerType> audioContainer;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.IsAnyMemberPresent()) {
    videoContainer = CheckVideoConfiguration(aConfiguration.mVideo);
    if (!videoContainer) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }

    // We have a video configuration and it is valid. Check if it is supported.
    supported &=
      aConfiguration.mType == MediaDecodingType::File
        ? CheckTypeForFile(aConfiguration.mVideo.mContentType)
        : CheckTypeForMediaSource(aConfiguration.mVideo.mContentType);
  }
  if (aConfiguration.mAudio.IsAnyMemberPresent()) {
    audioContainer = CheckAudioConfiguration(aConfiguration.mAudio);
    if (!audioContainer) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &=
      aConfiguration.mType == MediaDecodingType::File
        ? CheckTypeForFile(aConfiguration.mAudio.mContentType)
        : CheckTypeForMediaSource(aConfiguration.mAudio.mContentType);
  }

  if (!supported) {
    auto info = MakeUnique<MediaCapabilitiesInfo>(
      false /* supported */, false /* smooth */, false /* power efficient */);
    promise->MaybeResolve(std::move(info));
    return promise.forget();
  }

  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (aConfiguration.mVideo.IsAnyMemberPresent()) {
    MOZ_ASSERT(videoContainer.isSome(), "configuration is valid and supported");
    auto videoTracks = DecoderTraits::GetTracksInfo(*videoContainer);
    // If the MIME type does not imply a codec, the string MUST
    // also have one and only one parameter that is named codecs with a value
    // describing a single media codec. Otherwise, it MUST contain no
    // parameters.
    if (videoTracks.Length() != 1) {
      promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
      return promise.forget();
    }
    MOZ_DIAGNOSTIC_ASSERT(videoTracks.ElementAt(0),
                          "must contain a valid trackinfo");
    tracks.AppendElements(std::move(videoTracks));
  }
  if (aConfiguration.mAudio.IsAnyMemberPresent()) {
    MOZ_ASSERT(audioContainer.isSome(), "configuration is valid and supported");
    auto audioTracks = DecoderTraits::GetTracksInfo(*audioContainer);
    // If the MIME type does not imply a codec, the string MUST
    // also have one and only one parameter that is named codecs with a value
    // describing a single media codec. Otherwise, it MUST contain no
    // parameters.
    if (audioTracks.Length() != 1) {
      promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
      return promise.forget();
    }
    MOZ_DIAGNOSTIC_ASSERT(audioTracks.ElementAt(0),
                          "must contain a valid trackinfo");
    tracks.AppendElements(std::move(audioTracks));
  }

  typedef MozPromise<MediaCapabilitiesInfo,
                     MediaResult,
                     /* IsExclusive = */ true>
    CapabilitiesPromise;
  nsTArray<RefPtr<CapabilitiesPromise>> promises;

  RefPtr<TaskQueue> taskQueue =
    new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                  "MediaCapabilities::TaskQueue");
  for (auto&& config : tracks) {
    TrackInfo::TrackType type =
      config->IsVideo() ? TrackInfo::kVideoTrack : TrackInfo::kAudioTrack;

    MOZ_ASSERT(type == TrackInfo::kAudioTrack ||
                 videoContainer->ExtendedType().GetFramerate().isSome(),
               "framerate is a required member of VideoConfiguration");

    if (type == TrackInfo::kAudioTrack) {
      // There's no need to create an audio decoder has we only want to know if
      // such codec is supported
      RefPtr<PDMFactory> pdm = new PDMFactory();
      if (!pdm->Supports(*config, nullptr /* decoder doctor */)) {
        auto info =
          MakeUnique<MediaCapabilitiesInfo>(false /* supported */,
                                            false /* smooth */,
                                            false /* power efficient */);
        promise->MaybeResolve(std::move(info));
        return promise.forget();
      }
      // We can assume that if we could create the decoder, then we can play it.
      // We report that we can play it smoothly and in an efficient fashion.
      promises.AppendElement(CapabilitiesPromise::CreateAndResolve(
        MediaCapabilitiesInfo(
          true /* supported */, true /* smooth */, true /* power efficient */),
        __func__));
      continue;
    }

    // On Windows, the MediaDataDecoder expects to be created on a thread
    // supporting MTA, which the main thread doesn't. So we use our task queue
    // to create such decoder and perform initialization.

    RefPtr<layers::KnowsCompositor> compositor = GetCompositor();
    double frameRate = videoContainer->ExtendedType().GetFramerate().ref();
    promises.AppendElement(InvokeAsync(
      taskQueue,
      __func__,
      [taskQueue, frameRate, compositor, config = std::move(config)]() mutable
      -> RefPtr<CapabilitiesPromise> {
        // MediaDataDecoder keeps a reference to the config object, so we must
        // keep it alive until the decoder has been shutdown.
        CreateDecoderParams params{ *config,
                                    taskQueue,
                                    compositor,
                                    CreateDecoderParams::VideoFrameRate(
                                      frameRate),
#ifdef XP_WIN
                                    CreateDecoderParams::NoWrapper(true),
#endif
                                    TrackInfo::kVideoTrack };

        RefPtr<PDMFactory> pdm = new PDMFactory();
        RefPtr<MediaDataDecoder> decoder = pdm->CreateDecoder(params);
        if (!decoder) {
          return CapabilitiesPromise::CreateAndReject(
            MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Can't create decoder"),
            __func__);
        }
        // We now query the decoder to determine if it's power efficient.
        return decoder->Init()->Then(
          taskQueue,
          __func__,
          [taskQueue, decoder, config = std::move(config)](
            const MediaDataDecoder::InitPromise::ResolveOrRejectValue&
              aValue) mutable {
            RefPtr<CapabilitiesPromise> p;
            if (aValue.IsReject()) {
              p = CapabilitiesPromise::CreateAndReject(aValue.RejectValue(),
                                                       __func__);
            } else {
              MOZ_ASSERT(config->IsVideo());
              nsAutoCString reason;
              bool powerEfficient = decoder->IsHardwareAccelerated(reason);
              // TODO use benchmark result to narrow smoothness;
              p = CapabilitiesPromise::CreateAndResolve(
                MediaCapabilitiesInfo(
                  true /* supported */, true /* smooth */, powerEfficient),
                __func__);
            }
            MOZ_ASSERT(p.get(), "the promise has been created");
            // Let's keep alive the decoder and the config object until the
            // decoder has shutdown.
            decoder->Shutdown()->Then(
              taskQueue,
              __func__,
              [taskQueue, decoder, config = std::move(config)](
                const ShutdownPromise::ResolveOrRejectValue& aValue) {});
            return p;
          });
      }));
  }

  auto holder =
    MakeRefPtr<DOMMozPromiseRequestHolder<CapabilitiesPromise::AllPromiseType>>(
      mParent);
  RefPtr<nsISerialEventTarget> targetThread;
  RefPtr<StrongWorkerRef> workerRef;

  if (NS_IsMainThread()) {
    targetThread = mParent->AbstractMainThreadFor(TaskCategory::Other);
  } else {
    WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(wp, "Must be called from a worker thread");
    targetThread = wp->HybridEventTarget();
    RefPtr<StrongWorkerRef> strongWorkerRef = StrongWorkerRef::Create(
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

  CapabilitiesPromise::All(targetThread, promises)
    ->Then(targetThread,
           __func__,
           [promise, tracks = std::move(tracks), workerRef, holder](
             const CapabilitiesPromise::AllPromiseType::ResolveOrRejectValue&
               aValue) {
             holder->Complete();
             if (aValue.IsReject()) {
               auto info =
                 MakeUnique<MediaCapabilitiesInfo>(false /* supported */,
                                                   false /* smooth */,
                                                   false /* power efficient */);
               promise->MaybeResolve(std::move(info));
               return;
             }
             bool powerEfficient = true;
             for (auto&& capability : aValue.ResolveValue()) {
               powerEfficient &= capability.PowerEfficient();
             }
             auto info = MakeUnique<MediaCapabilitiesInfo>(
               true /* supported */, true /* smooth */, powerEfficient);
             promise->MaybeResolve(std::move(info));
           })
    ->Track(*holder);

  return promise.forget();
}

already_AddRefed<Promise>
MediaCapabilities::EncodingInfo(
  const MediaEncodingConfiguration& aConfiguration,
  ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(mParent, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // If configuration is not a valid MediaConfiguration, return a Promise
  // rejected with a TypeError.
  if (!aConfiguration.IsAnyMemberPresent() ||
      (!aConfiguration.mVideo.IsAnyMemberPresent() &&
       !aConfiguration.mAudio.IsAnyMemberPresent())) {
    aRv.ThrowTypeError<MSG_MISSING_REQUIRED_DICTIONARY_MEMBER>(
      NS_LITERAL_STRING("'audio' or 'video'"));
    return nullptr;
  }

  bool supported = true;

  // If configuration.video is present and is not a valid video configuration,
  // return a Promise rejected with a TypeError.
  if (aConfiguration.mVideo.IsAnyMemberPresent()) {
    if (!CheckVideoConfiguration(aConfiguration.mVideo)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_VIDEO_CONFIGURATION>();
      return nullptr;
    }
    // We have a video configuration and it is valid. Check if it is supported.
    supported &= CheckTypeForEncoder(aConfiguration.mVideo.mContentType);
  }
  if (aConfiguration.mAudio.IsAnyMemberPresent()) {
    if (!CheckAudioConfiguration(aConfiguration.mAudio)) {
      aRv.ThrowTypeError<MSG_INVALID_MEDIA_AUDIO_CONFIGURATION>();
      return nullptr;
    }
    // We have an audio configuration and it is valid. Check if it is supported.
    supported &= CheckTypeForEncoder(aConfiguration.mAudio.mContentType);
  }

  auto info = MakeUnique<MediaCapabilitiesInfo>(supported, supported, false);
  promise->MaybeResolve(std::move(info));

  return promise.forget();
}

Maybe<MediaContainerType>
MediaCapabilities::CheckVideoConfiguration(
  const VideoConfiguration& aConfig) const
{
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

Maybe<MediaContainerType>
MediaCapabilities::CheckAudioConfiguration(
  const AudioConfiguration& aConfig) const
{
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

bool
MediaCapabilities::CheckTypeForMediaSource(const nsAString& aType)
{
  return NS_SUCCEEDED(MediaSource::IsTypeSupported(
    aType, nullptr /* DecoderDoctorDiagnostics */));
}

bool
MediaCapabilities::CheckTypeForFile(const nsAString& aType)
{
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    return false;
  }

  return DecoderTraits::CanHandleContainerType(
           *containerType, nullptr /* DecoderDoctorDiagnostics */) !=
         CANPLAY_NO;
}

bool
MediaCapabilities::CheckTypeForEncoder(const nsAString& aType)
{
  return MediaRecorder::IsTypeSupported(aType);
}

already_AddRefed<layers::KnowsCompositor>
MediaCapabilities::GetCompositor()
{
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
  if (NS_WARN_IF(!window)) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return nullptr;
  }
  RefPtr<layers::LayerManager> layerManager =
    nsContentUtils::LayerManagerForDocument(doc);
  if (NS_WARN_IF(!layerManager)) {
    return nullptr;
  }
  RefPtr<layers::KnowsCompositor> knows = layerManager->AsKnowsCompositor();
  if (NS_WARN_IF(!knows)) {
    return nullptr;
  }
  return knows->GetForMedia().forget();
}

bool
MediaCapabilities::Enabled(JSContext* aCx, JSObject* aGlobal)
{
  return StaticPrefs::MediaCapabilitiesEnabled();
}

JSObject*
MediaCapabilities::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
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
bool
MediaCapabilitiesInfo::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto,
                                  JS::MutableHandle<JSObject*> aReflector)
{
  return MediaCapabilitiesInfo_Binding::Wrap(
    aCx, this, aGivenProto, aReflector);
}

} // namespace dom
} // namespace mozilla
