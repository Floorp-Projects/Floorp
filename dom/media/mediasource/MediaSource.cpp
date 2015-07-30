/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSource.h"

#include "AsyncEventRunner.h"
#include "DecoderTraits.h"
#include "MediaSourceUtils.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/mozalloc.h"
#include "nsContentTypeParser.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEffectiveTLDService.h"
#include "nsIRunnable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include "nsServiceManagerUtils.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

struct JSContext;
class JSObject;

PRLogModuleInfo* GetMediaSourceLog()
{
  static PRLogModuleInfo* sLogModule = nullptr;
  if (!sLogModule) {
    sLogModule = PR_NewLogModule("MediaSource");
  }
  return sLogModule;
}

PRLogModuleInfo* GetMediaSourceAPILog()
{
  static PRLogModuleInfo* sLogModule = nullptr;
  if (!sLogModule) {
    sLogModule = PR_NewLogModule("MediaSource");
  }
  return sLogModule;
}

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSource(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_API(arg, ...) MOZ_LOG(GetMediaSourceAPILog(), mozilla::LogLevel::Debug, ("MediaSource(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

// Arbitrary limit.
static const unsigned int MAX_SOURCE_BUFFERS = 16;

namespace mozilla {

static const char* const gMediaSourceTypes[6] = {
  "video/mp4",
  "audio/mp4",
  "video/webm",
  "audio/webm",
  nullptr
};

static nsresult
IsTypeSupported(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }
  NS_ConvertUTF16toUTF8 mimeTypeUTF8(mimeType);

  nsAutoString codecs;
  bool hasCodecs = NS_SUCCEEDED(parser.GetParameter("codecs", codecs));

  for (uint32_t i = 0; gMediaSourceTypes[i]; ++i) {
    if (mimeType.EqualsASCII(gMediaSourceTypes[i])) {
      if (DecoderTraits::IsMP4Type(mimeTypeUTF8)) {
        if (!Preferences::GetBool("media.mediasource.mp4.enabled", false)) {
          return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        }
        if (hasCodecs &&
            DecoderTraits::CanHandleCodecsType(mimeTypeUTF8.get(),
                                               codecs) == CANPLAY_NO) {
          return NS_ERROR_DOM_INVALID_STATE_ERR;
        }
        return NS_OK;
      } else if (DecoderTraits::IsWebMType(mimeTypeUTF8)) {
        if (!Preferences::GetBool("media.mediasource.webm.enabled", false)) {
          return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
        }
        if (hasCodecs &&
            DecoderTraits::CanHandleCodecsType(mimeTypeUTF8.get(),
                                               codecs) == CANPLAY_NO) {
          return NS_ERROR_DOM_INVALID_STATE_ERR;
        }
        return NS_OK;
      }
    }
  }

  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

namespace dom {

/* static */ already_AddRefed<MediaSource>
MediaSource::Constructor(const GlobalObject& aGlobal,
                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<MediaSource> mediaSource = new MediaSource(window);
  return mediaSource.forget();
}

MediaSource::~MediaSource()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("");
  if (mDecoder) {
    mDecoder->DetachMediaSource();
  }
}

SourceBufferList*
MediaSource::SourceBuffers()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed, mSourceBuffers->IsEmpty());
  return mSourceBuffers;
}

SourceBufferList*
MediaSource::ActiveSourceBuffers()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed, mActiveSourceBuffers->IsEmpty());
  return mActiveSourceBuffers;
}

MediaSourceReadyState
MediaSource::ReadyState()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mReadyState;
}

double
MediaSource::Duration()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mReadyState == MediaSourceReadyState::Closed) {
    return UnspecifiedNaN<double>();
  }
  MOZ_ASSERT(mDecoder);
  return mDecoder->GetMediaSourceDuration();
}

void
MediaSource::SetDuration(double aDuration, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetDuration(aDuration=%f, ErrorResult)", aDuration);
  if (aDuration < 0 || IsNaN(aDuration)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  SetDuration(aDuration, MSRangeRemovalAction::RUN);
}

void
MediaSource::SetDuration(double aDuration, MSRangeRemovalAction aAction)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetDuration(aDuration=%f)", aDuration);
  mDecoder->SetMediaSourceDuration(aDuration, aAction);
}

already_AddRefed<SourceBuffer>
MediaSource::AddSourceBuffer(const nsAString& aType, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mozilla::IsTypeSupported(aType);
  MSE_API("AddSourceBuffer(aType=%s)%s",
          NS_ConvertUTF16toUTF8(aType).get(),
          rv == NS_OK ? "" : " [not supported]");
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
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
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }
  nsRefPtr<SourceBuffer> sourceBuffer = new SourceBuffer(this, NS_ConvertUTF16toUTF8(mimeType));
  if (!sourceBuffer) {
    aRv.Throw(NS_ERROR_FAILURE); // XXX need a better error here
    return nullptr;
  }
  mSourceBuffers->Append(sourceBuffer);
  MSE_DEBUG("sourceBuffer=%p", sourceBuffer.get());
  return sourceBuffer.forget();
}

void
MediaSource::SourceBufferIsActive(SourceBuffer* aSourceBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  mActiveSourceBuffers->ClearSimple();
  bool found = false;
  for (uint32_t i = 0; i < mSourceBuffers->Length(); i++) {
    SourceBuffer* sourceBuffer = mSourceBuffers->IndexedGetter(i, found);
    MOZ_ALWAYS_TRUE(found);
    if (sourceBuffer == aSourceBuffer) {
      mActiveSourceBuffers->Append(aSourceBuffer);
    } else if (sourceBuffer->IsActive()) {
      mActiveSourceBuffers->AppendSimple(sourceBuffer);
    }
  }
}

void
MediaSource::RemoveSourceBuffer(SourceBuffer& aSourceBuffer, ErrorResult& aRv)
{
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
  //     remove sourceBuffer video, audio, text Tracks and fire "removetrack" at affected lists
  //     fire "removetrack" at modified MediaElement track lists
  // If removed enabled/selected, fire "change" at affected MediaElement list.
  if (mActiveSourceBuffers->Contains(sourceBuffer)) {
    mActiveSourceBuffers->Remove(sourceBuffer);
  }
  mSourceBuffers->Remove(sourceBuffer);
  // TODO: Free all resources associated with sourceBuffer
}

void
MediaSource::EndOfStream(const Optional<MediaSourceEndOfStreamError>& aError, ErrorResult& aRv)
{
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
    mDecoder->SetMediaSourceDuration(mSourceBuffers->GetHighestBufferedEndTime(),
                                     MSRangeRemovalAction::SKIP);
    if (aRv.Failed()) {
      return;
    }
    // Notify reader that all data is now available.
    mDecoder->Ended(true);
    return;
  }
  switch (aError.Value()) {
  case MediaSourceEndOfStreamError::Network:
    // TODO: If media element has a readyState of:
    //   HAVE_NOTHING -> run resource fetch algorithm
    // > HAVE_NOTHING -> run "interrupted" steps of resource fetch
    break;
  case MediaSourceEndOfStreamError::Decode:
    // TODO: If media element has a readyState of:
    //   HAVE_NOTHING -> run "unsupported" steps of resource fetch
    // > HAVE_NOTHING -> run "corrupted" steps of resource fetch
    break;
  default:
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  }
}

/* static */ bool
MediaSource::IsTypeSupported(const GlobalObject&, const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mozilla::IsTypeSupported(aType);
#define this nullptr
  MSE_API("IsTypeSupported(aType=%s)%s ",
          NS_ConvertUTF16toUTF8(aType).get(), rv == NS_OK ? "OK" : "[not supported]");
#undef this // don't ever remove this line !
  return NS_SUCCEEDED(rv);
}

/* static */ bool
MediaSource::Enabled(JSContext* cx, JSObject* aGlobal)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Don't use aGlobal across Preferences stuff, which the static
  // analysis thinks can GC.
  JS::Rooted<JSObject*> global(cx, aGlobal);

  bool enabled = Preferences::GetBool("media.mediasource.enabled");
  if (!enabled) {
    return false;
  }

  // Check whether it's enabled everywhere or just whitelisted sites.
  bool restrict = Preferences::GetBool("media.mediasource.whitelist", false);
  if (!restrict) {
    return true;
  }

  // We want to restrict to YouTube only.
  // We define that as the origin being *.youtube.com.
  // We also support *.youtube-nocookie.com
  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(global);
  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(principal->GetURI(getter_AddRefs(uri))) || !uri) {
    return false;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldServ =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(tldServ, false);

  nsAutoCString eTLDplusOne;
   if (NS_FAILED(tldServ->GetBaseDomain(uri, 0, eTLDplusOne))) {
     return false;
   }

   return eTLDplusOne.EqualsLiteral("youtube.com") ||
          eTLDplusOne.EqualsLiteral("youtube-nocookie.com") ||
          eTLDplusOne.EqualsLiteral("netflix.com") ||
          eTLDplusOne.EqualsLiteral("dailymotion.com") ||
          eTLDplusOne.EqualsLiteral("dmcdn.net");
}

bool
MediaSource::Attach(MediaSourceDecoder* aDecoder)
{
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

void
MediaSource::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("mDecoder=%p owner=%p",
            mDecoder.get(), mDecoder ? mDecoder->GetOwner() : nullptr);
  if (!mDecoder) {
    MOZ_ASSERT(mReadyState == MediaSourceReadyState::Closed);
    MOZ_ASSERT(mActiveSourceBuffers->IsEmpty() && mSourceBuffers->IsEmpty());
    return;
  }
  mMediaElement = nullptr;
  mFirstSourceBufferInitialized = false;
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

MediaSource::MediaSource(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mDecoder(nullptr)
  , mPrincipal(nullptr)
  , mReadyState(MediaSourceReadyState::Closed)
  , mFirstSourceBufferInitialized(false)
{
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

void
MediaSource::SetReadyState(MediaSourceReadyState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState != mReadyState);
  MSE_DEBUG("SetReadyState(aState=%d) mReadyState=%d", aState, mReadyState);

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

void
MediaSource::DispatchSimpleEvent(const char* aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Dispatch event '%s'", aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
MediaSource::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("Queuing event '%s'", aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<MediaSource>(this, aName);
  NS_DispatchToMainThread(event);
}

void
MediaSource::DurationChange(double aOldDuration, double aNewDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("DurationChange(aOldDuration=%f, aNewDuration=%f)", aOldDuration, aNewDuration);

  if (aNewDuration < aOldDuration) {
    // Remove all buffered data from aNewDuration.
    mSourceBuffers->RangeRemoval(aNewDuration, PositiveInfinity<double>());
  }
  // TODO: If partial audio frames/text cues exist, clamp duration based on mSourceBuffers.
}

void
MediaSource::NotifyEvicted(double aStart, double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("NotifyEvicted(aStart=%f, aEnd=%f)", aStart, aEnd);
  // Cycle through all SourceBuffers and tell them to evict data in
  // the given range.
  mSourceBuffers->Evict(aStart, aEnd);
}

void
MediaSource::QueueInitializationEvent()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mFirstSourceBufferInitialized) {
    return;
  }
  mFirstSourceBufferInitialized = true;
  MSE_DEBUG("");
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &MediaSource::InitializationEvent);
  NS_DispatchToMainThread(task);
}

void
MediaSource::InitializationEvent()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("");
  if (mDecoder) {
    mDecoder->PrepareReaderInitialization();
  }
}

#if defined(DEBUG)
void
MediaSource::Dump(const char* aPath)
{
  char buf[255];
  PR_snprintf(buf, sizeof(buf), "%s/mediasource-%p", aPath, this);
  PR_MkDir(buf, 0700);

  if (mSourceBuffers) {
    mSourceBuffers->Dump(buf);
  }
}
#endif

void
MediaSource::GetMozDebugReaderData(nsAString& aString)
{
  mDecoder->GetMozDebugReaderData(aString);
}

nsPIDOMWindow*
MediaSource::GetParentObject() const
{
  return GetOwner();
}

JSObject*
MediaSource::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MediaSourceBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaSource, DOMEventTargetHelper,
                                   mMediaElement,
                                   mSourceBuffers, mActiveSourceBuffers)

NS_IMPL_ADDREF_INHERITED(MediaSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaSource, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaSource)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::MediaSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

#undef MSE_DEBUG
#undef MSE_API

} // namespace dom

} // namespace mozilla
