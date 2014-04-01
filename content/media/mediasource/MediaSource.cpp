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
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/HTMLMediaElement.h"
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
PRLogModuleInfo* gMediaSourceLog;
#define MSE_DEBUG(...) PR_LOG(gMediaSourceLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#endif

// Arbitrary limit.
static const unsigned int MAX_SOURCE_BUFFERS = 16;

namespace mozilla {

static const char* const gMediaSourceTypes[6] = {
  "video/webm",
  "audio/webm",
  "video/mp4",
  "audio/mp4",
  "audio/mpeg",
  nullptr
};

static nsresult
IsTypeSupported(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
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

SourceBufferList*
MediaSource::SourceBuffers()
{
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed, mSourceBuffers->IsEmpty());
  return mSourceBuffers;
}

SourceBufferList*
MediaSource::ActiveSourceBuffers()
{
  MOZ_ASSERT_IF(mReadyState == MediaSourceReadyState::Closed, mActiveSourceBuffers->IsEmpty());
  return mActiveSourceBuffers;
}

MediaSourceReadyState
MediaSource::ReadyState()
{
  return mReadyState;
}

double
MediaSource::Duration()
{
  if (mReadyState == MediaSourceReadyState::Closed) {
    return UnspecifiedNaN<double>();
  }
  return mDuration;
}

void
MediaSource::SetDuration(double aDuration, ErrorResult& aRv)
{
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
  nsresult rv = mozilla::IsTypeSupported(aType);
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
  mSourceBuffers->Append(sourceBuffer);
  MSE_DEBUG("%p AddSourceBuffer(Type=%s) -> %p", this,
            NS_ConvertUTF16toUTF8(mimeType).get(), sourceBuffer.get());
  return sourceBuffer.forget();
}

void
MediaSource::RemoveSourceBuffer(SourceBuffer& aSourceBuffer, ErrorResult& aRv)
{
  SourceBuffer* sourceBuffer = &aSourceBuffer;
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
  if (mReadyState != MediaSourceReadyState::Open ||
      mSourceBuffers->AnyUpdating()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  SetReadyState(MediaSourceReadyState::Ended);
  mSourceBuffers->Ended();
  if (!aError.WasPassed()) {
    // TODO:
    // Run duration change algorithm.
    // DurationChange(highestDurationOfSourceBuffers, aRv);
    // if (aRv.Failed()) {
    //   return;
    // }
    // Notify media element that all data is now available.
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
  return NS_SUCCEEDED(mozilla::IsTypeSupported(aType));
}

bool
MediaSource::Attach(MediaSourceDecoder* aDecoder)
{
  MSE_DEBUG("%p Attaching decoder %p owner %p", this, aDecoder, aDecoder->GetOwner());
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
  MSE_DEBUG("%p Detaching decoder %p owner %p", this, mDecoder.get(), mDecoder->GetOwner());
  MOZ_ASSERT(mDecoder);
  mDecoder->DetachMediaSource();
  mDecoder = nullptr;
  mDuration = UnspecifiedNaN<double>();
  mActiveSourceBuffers->Clear();
  mSourceBuffers->Clear();
  SetReadyState(MediaSourceReadyState::Closed);
}

MediaSource::MediaSource(nsPIDOMWindow* aWindow)
  : DOMEventTargetHelper(aWindow)
  , mDuration(UnspecifiedNaN<double>())
  , mDecoder(nullptr)
  , mReadyState(MediaSourceReadyState::Closed)
{
  mSourceBuffers = new SourceBufferList(this);
  mActiveSourceBuffers = new SourceBufferList(this);

#ifdef PR_LOGGING
  if (!gMediaSourceLog) {
    gMediaSourceLog = PR_NewLogModule("MediaSource");
  }
#endif
}

void
MediaSource::SetReadyState(MediaSourceReadyState aState)
{
  MOZ_ASSERT(aState != mReadyState);

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
  MSE_DEBUG("%p Dispatching event %s to MediaSource", this, aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
MediaSource::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("%p Queuing event %s to MediaSource", this, aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<MediaSource>(this, aName);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void
MediaSource::DurationChange(double aNewDuration, ErrorResult& aRv)
{
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

nsPIDOMWindow*
MediaSource::GetParentObject() const
{
  return GetOwner();
}

JSObject*
MediaSource::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MediaSourceBinding::Wrap(aCx, aScope, this);
}

void
MediaSource::NotifyEvicted(double aStart, double aEnd)
{
  // Cycle through all SourceBuffers and tell them to evict data in
  // the given range.
  mSourceBuffers->Evict(aStart, aEnd);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(MediaSource, DOMEventTargetHelper,
                                     mSourceBuffers, mActiveSourceBuffers)

NS_IMPL_ADDREF_INHERITED(MediaSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaSource, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaSource)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::MediaSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom

} // namespace mozilla
