/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaSource.h"

#include "AsyncEventRunner.h"
#include "DecoderTraits.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/mozalloc.h"
#include "nsContentTypeParser.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsPIDOMWindow.h"
#include "nsStringGlue.h"
#include "nsThreadUtils.h"
#include "prlog.h"

struct JSContext;
class JSObject;

#ifdef PR_LOGGING
PRLogModuleInfo* GetMediaSourceLog()
{
  static PRLogModuleInfo* sLogModule;
  if (!sLogModule) {
    sLogModule = PR_NewLogModule("MediaSource");
  }
  return sLogModule;
}

PRLogModuleInfo* GetMediaSourceAPILog()
{
  static PRLogModuleInfo* sLogModule;
  if (!sLogModule) {
    sLogModule = PR_NewLogModule("MediaSource");
  }
  return sLogModule;
}

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_API(...)
#endif

// Arbitrary limit.
static const unsigned int MAX_SOURCE_BUFFERS = 16;

namespace mozilla {

static const char* const gMediaSourceTypes[6] = {
  "video/webm",
  "audio/webm",
// XXX: Disabled other codecs temporarily to allow WebM testing.  For now, set
// the developer-only media.mediasource.ignore_codecs pref to true to test other
// codecs, and expect things to be broken.
#if 0
  "video/mp4",
  "audio/mp4",
  "audio/mpeg",
#endif
  nullptr
};

static nsresult
IsTypeSupported(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }
  if (Preferences::GetBool("media.mediasource.ignore_codecs", false)) {
    return NS_OK;
  }
  // TODO: Further restrict this to formats in the spec.
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  bool found = false;
  for (uint32_t i = 0; gMediaSourceTypes[i]; ++i) {
    if (mimeType.EqualsASCII(gMediaSourceTypes[i])) {
      found = true;
      break;
    }
  }
  if (!found) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  // Check aType against HTMLMediaElement list of MIME types.  Since we've
  // already restricted the container format, this acts as a specific check
  // of any specified "codecs" parameter of aType.
  if (dom::HTMLMediaElement::GetCanPlay(aType) == CANPLAY_NO) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }
  return NS_OK;
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
  MSE_API("MediaSource(%p)::~MediaSource()", this);
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
  return mDuration;
}

void
MediaSource::SetDuration(double aDuration, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("MediaSource(%p)::SetDuration(aDuration=%f)", this, aDuration);
  if (aDuration < 0 || IsNaN(aDuration)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  DurationChange(aDuration, aRv);
}

already_AddRefed<SourceBuffer>
MediaSource::AddSourceBuffer(const nsAString& aType, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mozilla::IsTypeSupported(aType);
  MSE_API("MediaSource(%p)::AddSourceBuffer(aType=%s)%s",
          this, NS_ConvertUTF16toUTF8(aType).get(),
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
  nsRefPtr<SourceBuffer> sourceBuffer = SourceBuffer::Create(this, NS_ConvertUTF16toUTF8(mimeType));
  if (!sourceBuffer) {
    aRv.Throw(NS_ERROR_FAILURE); // XXX need a better error here
    return nullptr;
  }
  mSourceBuffers->Append(sourceBuffer);
  mActiveSourceBuffers->Append(sourceBuffer);
  MSE_DEBUG("MediaSource(%p)::AddSourceBuffer() sourceBuffer=%p", this, sourceBuffer.get());
  return sourceBuffer.forget();
}

void
MediaSource::RemoveSourceBuffer(SourceBuffer& aSourceBuffer, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  SourceBuffer* sourceBuffer = &aSourceBuffer;
  MSE_API("MediaSource(%p)::RemoveSourceBuffer(aSourceBuffer=%p)", this, sourceBuffer);
  if (!mSourceBuffers->Contains(sourceBuffer)) {
    aRv.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }
  if (sourceBuffer->Updating()) {
    // TODO:
    // abort stream append loop (if running)
    // set updating to false
    // fire "abort" at sourceBuffer
    // fire "updateend" at sourceBuffer
  }
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
  MSE_API("MediaSource(%p)::EndOfStream(aError=%d)",
          this, aError.WasPassed() ? uint32_t(aError.Value()) : 0);
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  SetReadyState(MediaSourceReadyState::Ended);
  mSourceBuffers->Ended();
  if (!aError.WasPassed()) {
    DurationChange(mSourceBuffers->GetHighestBufferedEndTime(), aRv);
    if (aRv.Failed()) {
      return;
    }
    // TODO:
    //   Notify media element that all data is now available.
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
  MSE_API("MediaSource::IsTypeSupported(aType=%s)%s",
          NS_ConvertUTF16toUTF8(aType).get(), rv == NS_OK ? "" : " [not supported]");
  return NS_SUCCEEDED(rv);
}

bool
MediaSource::Attach(MediaSourceDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("MediaSource(%p)::Attach(aDecoder=%p) owner=%p", this, aDecoder, aDecoder->GetOwner());
  MOZ_ASSERT(aDecoder);
  if (mReadyState != MediaSourceReadyState::Closed) {
    return false;
  }
  mDecoder = aDecoder;
  mDecoder->AttachMediaSource(this);
  SetReadyState(MediaSourceReadyState::Open);
  return true;
}

void
MediaSource::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("MediaSource(%p)::Detach() mDecoder=%p owner=%p",
            this, mDecoder.get(), mDecoder->GetOwner());
  MOZ_ASSERT(mDecoder);
  mDecoder->DetachMediaSource();
  mDecoder = nullptr;
  mDuration = UnspecifiedNaN<double>();
  mActiveSourceBuffers->Clear();
  mSourceBuffers->Clear();
  SetReadyState(MediaSourceReadyState::Closed);
}

void
MediaSource::GetBuffered(TimeRanges* aBuffered)
{
  if (mActiveSourceBuffers->IsEmpty()) {
    return;
  }

  nsTArray<nsRefPtr<TimeRanges>> ranges;
  for (uint32_t i = 0; i < mActiveSourceBuffers->Length(); ++i) {
    bool found;
    SourceBuffer* sourceBuffer = mActiveSourceBuffers->IndexedGetter(i, found);

    ErrorResult dummy;
    *ranges.AppendElement() = sourceBuffer->GetBuffered(dummy);
  }

  double highestEndTime = mActiveSourceBuffers->GetHighestBufferedEndTime();
  if (highestEndTime <= 0) {
    return;
  }

  MOZ_ASSERT(aBuffered->Length() == 0);
  aBuffered->Add(0, highestEndTime);

  for (uint32_t i = 0; i < ranges.Length(); ++i) {
    if (mReadyState == MediaSourceReadyState::Ended) {
      ranges[i]->Add(ranges[i]->GetEndTime(), highestEndTime);
    }

    aBuffered->Intersection(ranges[i]);
  }

  MSE_DEBUG("MediaSource(%p)::GetBuffered start=%f end=%f length=%u",
            this, aBuffered->GetStartTime(), aBuffered->GetEndTime(), aBuffered->Length());
}

MediaSource::MediaSource(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mDuration(UnspecifiedNaN<double>())
  , mDecoder(nullptr)
  , mReadyState(MediaSourceReadyState::Closed)
  , mWaitForDataMonitor("MediaSource.WaitForData.Monitor")
{
  MOZ_ASSERT(NS_IsMainThread());
  mSourceBuffers = new SourceBufferList(this);
  mActiveSourceBuffers = new SourceBufferList(this);
  MSE_API("MediaSource(%p)::MediaSource(aWindow=%p) mSourceBuffers=%p mActiveSourceBuffers=%p",
          this, aWindow, mSourceBuffers.get(), mActiveSourceBuffers.get());
}

void
MediaSource::SetReadyState(MediaSourceReadyState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aState != mReadyState);
  MSE_DEBUG("MediaSource(%p)::SetReadyState(aState=%d) mReadyState=%d", this, aState, mReadyState);

  MediaSourceReadyState oldState = mReadyState;
  mReadyState = aState;

  if (mReadyState == MediaSourceReadyState::Open &&
      (oldState == MediaSourceReadyState::Closed ||
       oldState == MediaSourceReadyState::Ended)) {
    QueueAsyncSimpleEvent("sourceopen");
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
  MSE_API("MediaSource(%p) Dispatch event '%s'", this, aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
MediaSource::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("MediaSource(%p) Queuing event '%s'", this, aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<MediaSource>(this, aName);
  NS_DispatchToMainThread(event);
}

void
MediaSource::DurationChange(double aNewDuration, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("MediaSource(%p)::DurationChange(aNewDuration=%f)", this, aNewDuration);
  if (mDuration == aNewDuration) {
    return;
  }
  double oldDuration = mDuration;
  mDuration = aNewDuration;
  if (aNewDuration < oldDuration) {
    mSourceBuffers->Remove(aNewDuration, oldDuration, aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  // TODO: If partial audio frames/text cues exist, clamp duration based on mSourceBuffers.
  // TODO: Update media element's duration and run element's duration change algorithm.
}

void
MediaSource::NotifyEvicted(double aStart, double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("MediaSource(%p)::NotifyEvicted(aStart=%f, aEnd=%f)", this, aStart, aEnd);
  // Cycle through all SourceBuffers and tell them to evict data in
  // the given range.
  mSourceBuffers->Evict(aStart, aEnd);
}

void
MediaSource::WaitForData()
{
  MSE_DEBUG("MediaSource(%p)::WaitForData()", this);
  MonitorAutoLock mon(mWaitForDataMonitor);
  mon.Wait();
}

void
MediaSource::NotifyGotData()
{
  MSE_DEBUG("MediaSource(%p)::NotifyGotData()", this);
  MonitorAutoLock mon(mWaitForDataMonitor);
  mon.NotifyAll();
}

nsPIDOMWindow*
MediaSource::GetParentObject() const
{
  return GetOwner();
}

JSObject*
MediaSource::WrapObject(JSContext* aCx)
{
  return MediaSourceBinding::Wrap(aCx, this);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(MediaSource, DOMEventTargetHelper,
                                   mSourceBuffers, mActiveSourceBuffers)

NS_IMPL_ADDREF_INHERITED(MediaSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaSource, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaSource)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::MediaSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom

} // namespace mozilla
