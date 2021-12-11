/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSource.h"

#include "AsyncEventRunner.h"
#include "Benchmark.h"
#include "DecoderDoctorDiagnostics.h"
#include "DecoderTraits.h"
#include "MediaContainerType.h"
#include "MediaResult.h"
#include "MediaSourceDemuxer.h"
#include "MediaSourceUtils.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/mozalloc.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIRunnable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsMimeTypes.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#  include "mozilla/java/HardwareCodecCapabilityUtilsWrappers.h"
#endif

struct JSContext;
class JSObject;

mozilla::LogModule* GetMediaSourceLog() {
  static mozilla::LazyLogModule sLogModule("MediaSource");
  return sLogModule;
}

mozilla::LogModule* GetMediaSourceAPILog() {
  static mozilla::LazyLogModule sLogModule("MediaSource");
  return sLogModule;
}

#define MSE_DEBUG(arg, ...)                                              \
  DDMOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, "::%s: " arg, \
            __func__, ##__VA_ARGS__)
#define MSE_API(arg, ...)                                                   \
  DDMOZ_LOG(GetMediaSourceAPILog(), mozilla::LogLevel::Debug, "::%s: " arg, \
            __func__, ##__VA_ARGS__)

// Arbitrary limit.
static const unsigned int MAX_SOURCE_BUFFERS = 16;

namespace mozilla {

// Returns true if we should enable MSE webm regardless of preferences.
// 1. If MP4/H264 isn't supported:
//   * Windows XP
//   * Windows Vista and Server 2008 without the optional "Platform Update
//   Supplement"
//   * N/KN editions (Europe and Korea) of Windows 7/8/8.1/10 without the
//     optional "Windows Media Feature Pack"
// 2. If H264 hardware acceleration is not available.
// 3. The CPU is considered to be fast enough
static bool IsVP9Forced(DecoderDoctorDiagnostics* aDiagnostics) {
  bool mp4supported = DecoderTraits::IsMP4SupportedType(
      MediaContainerType(MEDIAMIMETYPE(VIDEO_MP4)), aDiagnostics);
  bool hwsupported = gfx::gfxVars::CanUseHardwareVideoDecoding();
#ifdef MOZ_WIDGET_ANDROID
  return !mp4supported || !hwsupported || VP9Benchmark::IsVP9DecodeFast() ||
         java::HardwareCodecCapabilityUtils::HasHWVP9(false /* aIsEncoder */);
#else
  return !mp4supported || !hwsupported || VP9Benchmark::IsVP9DecodeFast();
#endif
}

namespace dom {

static void RecordTypeForTelemetry(const nsAString& aType,
                                   nsPIDOMWindowInner* aWindow) {
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    return;
  }

  const MediaMIMEType& mimeType = containerType->Type();
  if (mimeType == MEDIAMIMETYPE(VIDEO_WEBM)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::VideoWebm);
  } else if (mimeType == MEDIAMIMETYPE(AUDIO_WEBM)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::AudioWebm);
  } else if (mimeType == MEDIAMIMETYPE(VIDEO_MP4)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::VideoMp4);
  } else if (mimeType == MEDIAMIMETYPE(AUDIO_MP4)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::AudioMp4);
  } else if (mimeType == MEDIAMIMETYPE(VIDEO_MPEG_TS)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::VideoMp2t);
  } else if (mimeType == MEDIAMIMETYPE(AUDIO_MPEG_TS)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::AudioMp2t);
  } else if (mimeType == MEDIAMIMETYPE(AUDIO_MP3)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::AudioMpeg);
  } else if (mimeType == MEDIAMIMETYPE(AUDIO_AAC)) {
    AccumulateCategorical(
        mozilla::Telemetry::LABELS_MSE_SOURCE_BUFFER_TYPE::AudioAac);
  }
}

/* static */
void MediaSource::IsTypeSupported(const nsAString& aType,
                                  DecoderDoctorDiagnostics* aDiagnostics,
                                  ErrorResult& aRv) {
  if (aType.IsEmpty()) {
    return aRv.ThrowTypeError("Empty type");
  }

  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    return aRv.ThrowNotSupportedError("Unknown type");
  }

  if (DecoderTraits::CanHandleContainerType(*containerType, aDiagnostics) ==
      CANPLAY_NO) {
    return aRv.ThrowNotSupportedError("Can't play type");
  }

  bool hasVP9 = false;
  const MediaCodecs& codecs = containerType->ExtendedType().Codecs();
  for (const auto& codec : codecs.Range()) {
    if (IsVP9CodecString(codec)) {
      hasVP9 = true;
      break;
    }
  }

  // Now we know that this media type could be played.
  // MediaSource imposes extra restrictions, and some prefs.
  const MediaMIMEType& mimeType = containerType->Type();
  if (mimeType == MEDIAMIMETYPE("video/mp4") ||
      mimeType == MEDIAMIMETYPE("audio/mp4")) {
    if (!StaticPrefs::media_mediasource_mp4_enabled()) {
      // Don't leak information about the fact that it's pref-disabled; just act
      // like we can't play it.  Or should this throw "Unknown type"?
      return aRv.ThrowNotSupportedError("Can't play type");
    }
    if (!StaticPrefs::media_mediasource_vp9_enabled() && hasVP9 &&
        !IsVP9Forced(aDiagnostics)) {
      // Don't leak information about the fact that it's pref-disabled; just act
      // like we can't play it.  Or should this throw "Unknown type"?
      return aRv.ThrowNotSupportedError("Can't play type");
    }

    return;
  }
  if (mimeType == MEDIAMIMETYPE("video/webm")) {
    if (!StaticPrefs::media_mediasource_webm_enabled()) {
      // Don't leak information about the fact that it's pref-disabled; just act
      // like we can't play it.  Or should this throw "Unknown type"?
      return aRv.ThrowNotSupportedError("Can't play type");
    }
    if (!StaticPrefs::media_mediasource_vp9_enabled() && hasVP9 &&
        !IsVP9Forced(aDiagnostics)) {
      // Don't leak information about the fact that it's pref-disabled; just act
      // like we can't play it.  Or should this throw "Unknown type"?
      return aRv.ThrowNotSupportedError("Can't play type");
    }
    return;
  }
  if (mimeType == MEDIAMIMETYPE("audio/webm")) {
    if (!(StaticPrefs::media_mediasource_webm_enabled() ||
          StaticPrefs::media_mediasource_webm_audio_enabled())) {
      // Don't leak information about the fact that it's pref-disabled; just act
      // like we can't play it.  Or should this throw "Unknown type"?
      return aRv.ThrowNotSupportedError("Can't play type");
    }
    return;
  }

  return aRv.ThrowNotSupportedError("Type not supported in MediaSource");
}

/* static */
already_AddRefed<MediaSource> MediaSource::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<MediaSource> mediaSource = new MediaSource(window);
  return mediaSource.forget();
}

MediaSource::~MediaSource() {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("");
  if (mDecoder) {
    mDecoder->DetachMediaSource();
  }
}

SourceBufferList* MediaSource::SourceBuffers() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed,
                mSourceBuffers->IsEmpty());
  return mSourceBuffers;
}

SourceBufferList* MediaSource::ActiveSourceBuffers() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed,
                mActiveSourceBuffers->IsEmpty());
  return mActiveSourceBuffers;
}

MediaSourceReadyState MediaSource::ReadyState() {
  MOZ_ASSERT(NS_IsMainThread());
  return mReadyState;
}

double MediaSource::Duration() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mReadyState == MediaSourceReadyState::Closed) {
    return UnspecifiedNaN<double>();
  }
  MOZ_ASSERT(mDecoder);
  return mDecoder->GetDuration();
}

void MediaSource::SetDuration(double aDuration, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aDuration < 0 || IsNaN(aDuration)) {
    nsPrintfCString error("Invalid duration value %f", aDuration);
    MSE_API("SetDuration(aDuration=%f, invalid value)", aDuration);
    aRv.ThrowTypeError(error);
    return;
  }
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    MSE_API("SetDuration(aDuration=%f, invalid state)", aDuration);
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  DurationChange(aDuration, aRv);
  MSE_API("SetDuration(aDuration=%f, errorCode=%d)", aDuration,
          aRv.ErrorCodeAsInt());
}

void MediaSource::SetDuration(double aDuration) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetDuration(aDuration=%f)", aDuration);
  mDecoder->SetMediaSourceDuration(aDuration);
}

already_AddRefed<SourceBuffer> MediaSource::AddSourceBuffer(
    const nsAString& aType, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  DecoderDoctorDiagnostics diagnostics;
  IsTypeSupported(aType, &diagnostics, aRv);
  RecordTypeForTelemetry(aType, GetOwner());
  bool supported = !aRv.Failed();
  diagnostics.StoreFormatDiagnostics(
      GetOwner() ? GetOwner()->GetExtantDoc() : nullptr, aType, supported,
      __func__);
  MSE_API("AddSourceBuffer(aType=%s)%s", NS_ConvertUTF16toUTF8(aType).get(),
          supported ? "" : " [not supported]");
  if (!supported) {
    return nullptr;
  }
  if (mSourceBuffers->Length() >= MAX_SOURCE_BUFFERS) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return nullptr;
  }
  if (mReadyState != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  Maybe<MediaContainerType> containerType = MakeMediaContainerType(aType);
  if (!containerType) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }
  RefPtr<SourceBuffer> sourceBuffer = new SourceBuffer(this, *containerType);
  mSourceBuffers->Append(sourceBuffer);
  DDLINKCHILD("sourcebuffer[]", sourceBuffer.get());
  MSE_DEBUG("sourceBuffer=%p", sourceBuffer.get());
  return sourceBuffer.forget();
}

RefPtr<MediaSource::ActiveCompletionPromise> MediaSource::SourceBufferIsActive(
    SourceBuffer* aSourceBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  mActiveSourceBuffers->ClearSimple();
  bool initMissing = false;
  bool found = false;
  for (uint32_t i = 0; i < mSourceBuffers->Length(); i++) {
    SourceBuffer* sourceBuffer = mSourceBuffers->IndexedGetter(i, found);
    MOZ_ALWAYS_TRUE(found);
    if (sourceBuffer == aSourceBuffer) {
      mActiveSourceBuffers->Append(aSourceBuffer);
    } else if (sourceBuffer->IsActive()) {
      mActiveSourceBuffers->AppendSimple(sourceBuffer);
    } else {
      // Some source buffers haven't yet received an init segment.
      // There's nothing more we can do at this stage.
      initMissing = true;
    }
  }
  if (initMissing || !mDecoder) {
    return ActiveCompletionPromise::CreateAndResolve(true, __func__);
  }

  mDecoder->NotifyInitDataArrived();

  // Add our promise to the queue.
  // It will be resolved once the HTMLMediaElement modifies its readyState.
  MozPromiseHolder<ActiveCompletionPromise> holder;
  RefPtr<ActiveCompletionPromise> promise = holder.Ensure(__func__);
  mCompletionPromises.AppendElement(std::move(holder));
  return promise;
}

void MediaSource::CompletePendingTransactions() {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Resolving %u promises", unsigned(mCompletionPromises.Length()));
  for (auto& promise : mCompletionPromises) {
    promise.Resolve(true, __func__);
  }
  mCompletionPromises.Clear();
}

void MediaSource::RemoveSourceBuffer(SourceBuffer& aSourceBuffer,
                                     ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  SourceBuffer* sourceBuffer = &aSourceBuffer;
  MSE_API("RemoveSourceBuffer(aSourceBuffer=%p)", sourceBuffer);
  if (!mSourceBuffers->Contains(sourceBuffer)) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  sourceBuffer->AbortBufferAppend();
  // TODO:
  // abort stream append loop (if running)

  // TODO:
  // For all sourceBuffer audioTracks, videoTracks, textTracks:
  //     set sourceBuffer to null
  //     remove sourceBuffer video, audio, text Tracks from MediaElement tracks
  //     remove sourceBuffer video, audio, text Tracks and fire "removetrack" at
  //     affected lists fire "removetrack" at modified MediaElement track lists
  // If removed enabled/selected, fire "change" at affected MediaElement list.
  if (mActiveSourceBuffers->Contains(sourceBuffer)) {
    mActiveSourceBuffers->Remove(sourceBuffer);
  }
  mSourceBuffers->Remove(sourceBuffer);
  DDUNLINKCHILD(sourceBuffer);
  // TODO: Free all resources associated with sourceBuffer
}

void MediaSource::EndOfStream(
    const Optional<MediaSourceEndOfStreamError>& aError, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("EndOfStream(aError=%d)",
          aError.WasPassed() ? uint32_t(aError.Value()) : 0);
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  SetReadyState(MediaSourceReadyState::Ended);
  mSourceBuffers->Ended();
  if (!aError.WasPassed()) {
    DurationChange(mSourceBuffers->GetHighestBufferedEndTime(), aRv);
    // Notify reader that all data is now available.
    mDecoder->Ended(true);
    return;
  }
  switch (aError.Value()) {
    case MediaSourceEndOfStreamError::Network:
      mDecoder->NetworkError(MediaResult(NS_ERROR_FAILURE, "MSE network"));
      break;
    case MediaSourceEndOfStreamError::Decode:
      mDecoder->DecodeError(NS_ERROR_DOM_MEDIA_FATAL_ERR);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE(
          "Someone added a MediaSourceReadyState value and didn't handle it "
          "here");
      break;
  }
}

void MediaSource::EndOfStream(const MediaResult& aError) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("EndOfStream(aError=%s)", aError.ErrorName().get());

  SetReadyState(MediaSourceReadyState::Ended);
  mSourceBuffers->Ended();
  mDecoder->DecodeError(aError);
}

/* static */
bool MediaSource::IsTypeSupported(const GlobalObject& aOwner,
                                  const nsAString& aType) {
  MOZ_ASSERT(NS_IsMainThread());
  DecoderDoctorDiagnostics diagnostics;
  IgnoredErrorResult rv;
  IsTypeSupported(aType, &diagnostics, rv);
  bool supported = !rv.Failed();
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aOwner.GetAsSupports());
  RecordTypeForTelemetry(aType, window);
  diagnostics.StoreFormatDiagnostics(window ? window->GetExtantDoc() : nullptr,
                                     aType, supported, __func__);
  MOZ_LOG(GetMediaSourceAPILog(), mozilla::LogLevel::Debug,
          ("MediaSource::%s: IsTypeSupported(aType=%s) %s", __func__,
           NS_ConvertUTF16toUTF8(aType).get(),
           supported ? "OK" : "[not supported]"));
  return supported;
}

void MediaSource::SetLiveSeekableRange(double aStart, double aEnd,
                                       ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // 1. If the readyState attribute is not "open" then throw an
  // InvalidStateError exception and abort these steps.
  if (mReadyState != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // 2. If start is negative or greater than end, then throw a TypeError
  // exception and abort these steps.
  if (aStart < 0 || aStart > aEnd) {
    aRv.ThrowTypeError("Invalid start value");
    return;
  }

  // 3. Set live seekable range to be a new normalized TimeRanges object
  // containing a single range whose start position is start and end position is
  // end.
  mLiveSeekableRange =
      Some(media::TimeInterval(media::TimeUnit::FromSeconds(aStart),
                               media::TimeUnit::FromSeconds(aEnd)));
}

void MediaSource::ClearLiveSeekableRange(ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // 1. If the readyState attribute is not "open" then throw an
  // InvalidStateError exception and abort these steps.
  if (mReadyState != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // 2. If live seekable range contains a range, then set live seekable range to
  // be a new empty TimeRanges object.
  mLiveSeekableRange.reset();
}

bool MediaSource::Attach(MediaSourceDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Attach(aDecoder=%p) owner=%p", aDecoder, aDecoder->GetOwner());
  MOZ_ASSERT(aDecoder);
  MOZ_ASSERT(aDecoder->GetOwner());
  if (mReadyState != MediaSourceReadyState::Closed) {
    return false;
  }
  MOZ_ASSERT(!mMediaElement);
  mMediaElement = aDecoder->GetOwner()->GetMediaElement();
  MOZ_ASSERT(!mDecoder);
  mDecoder = aDecoder;
  mDecoder->AttachMediaSource(this);
  SetReadyState(MediaSourceReadyState::Open);
  return true;
}

void MediaSource::Detach() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(mCompletionPromises.IsEmpty());
  MSE_DEBUG("mDecoder=%p owner=%p", mDecoder.get(),
            mDecoder ? mDecoder->GetOwner() : nullptr);
  if (!mDecoder) {
    MOZ_ASSERT(mReadyState == MediaSourceReadyState::Closed);
    MOZ_ASSERT(mActiveSourceBuffers->IsEmpty() && mSourceBuffers->IsEmpty());
    return;
  }
  mMediaElement = nullptr;
  SetReadyState(MediaSourceReadyState::Closed);
  if (mActiveSourceBuffers) {
    mActiveSourceBuffers->Clear();
  }
  if (mSourceBuffers) {
    mSourceBuffers->Clear();
  }
  mDecoder->DetachMediaSource();
  mDecoder = nullptr;
}

MediaSource::MediaSource(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mDecoder(nullptr),
      mPrincipal(nullptr),
      mAbstractMainThread(
          GetOwnerGlobal()->AbstractMainThreadFor(TaskCategory::Other)),
      mReadyState(MediaSourceReadyState::Closed) {
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers = new SourceBufferList(this);
  mActiveSourceBuffers = new SourceBufferList(this);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  if (sop) {
    mPrincipal = sop->GetPrincipal();
  }

  MSE_API("MediaSource(aWindow=%p) mSourceBuffers=%p mActiveSourceBuffers=%p",
          aWindow, mSourceBuffers.get(), mActiveSourceBuffers.get());
}

void MediaSource::SetReadyState(MediaSourceReadyState aState) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState != mReadyState);
  MSE_DEBUG("SetReadyState(aState=%" PRIu32 ") mReadyState=%" PRIu32,
            static_cast<uint32_t>(aState), static_cast<uint32_t>(mReadyState));

  MediaSourceReadyState oldState = mReadyState;
  mReadyState = aState;

  if (mReadyState == MediaSourceReadyState::Open &&
      (oldState == MediaSourceReadyState::Closed ||
       oldState == MediaSourceReadyState::Ended)) {
    QueueAsyncSimpleEvent("sourceopen");
    if (oldState == MediaSourceReadyState::Ended) {
      // Notify reader that more data may come.
      mDecoder->Ended(false);
    }
    return;
  }

  if (mReadyState == MediaSourceReadyState::Ended &&
      oldState == MediaSourceReadyState::Open) {
    QueueAsyncSimpleEvent("sourceended");
    return;
  }

  if (mReadyState == MediaSourceReadyState::Closed &&
      (oldState == MediaSourceReadyState::Open ||
       oldState == MediaSourceReadyState::Ended)) {
    QueueAsyncSimpleEvent("sourceclose");
    return;
  }

  NS_WARNING("Invalid MediaSource readyState transition");
}

void MediaSource::DispatchSimpleEvent(const char* aName) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Dispatch event '%s'", aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void MediaSource::QueueAsyncSimpleEvent(const char* aName) {
  MSE_DEBUG("Queuing event '%s'", aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<MediaSource>(this, aName);
  mAbstractMainThread->Dispatch(event.forget());
}

void MediaSource::DurationChange(double aNewDuration, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("DurationChange(aNewDuration=%f)", aNewDuration);

  // 1. If the current value of duration is equal to new duration, then return.
  if (mDecoder->GetDuration() == aNewDuration) {
    return;
  }

  // 2. If new duration is less than the highest starting presentation timestamp
  // of any buffered coded frames for all SourceBuffer objects in sourceBuffers,
  // then throw an InvalidStateError exception and abort these steps.
  if (aNewDuration < mSourceBuffers->HighestStartTime()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // 3. Let highest end time be the largest track buffer ranges end time across
  // all the track buffers across all SourceBuffer objects in sourceBuffers.
  double highestEndTime = mSourceBuffers->HighestEndTime();
  // 4. If new duration is less than highest end time, then
  //    4.1 Update new duration to equal highest end time.
  aNewDuration = std::max(aNewDuration, highestEndTime);

  // 5. Update the media duration to new duration and run the HTMLMediaElement
  // duration change algorithm.
  mDecoder->SetMediaSourceDuration(aNewDuration);
}

already_AddRefed<Promise> MediaSource::MozDebugReaderData(ErrorResult& aRv) {
  // Creating a JS promise
  nsPIDOMWindowInner* win = GetOwner();
  if (!win) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  RefPtr<Promise> domPromise = Promise::Create(win->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(domPromise);
  MediaSourceDecoderDebugInfo info;
  mDecoder->GetDebugInfo(info);
  domPromise->MaybeResolve(info);
  return domPromise.forget();
}

nsPIDOMWindowInner* MediaSource::GetParentObject() const { return GetOwner(); }

JSObject* MediaSource::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return MediaSource_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaSource, DOMEventTargetHelper,
                                   mMediaElement, mSourceBuffers,
                                   mActiveSourceBuffers)

NS_IMPL_ADDREF_INHERITED(MediaSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaSource, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaSource)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::MediaSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

#undef MSE_DEBUG
#undef MSE_API

}  // namespace dom

}  // namespace mozilla
