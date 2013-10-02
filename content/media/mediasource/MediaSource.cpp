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
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

// Arbitrary limit.
static const unsigned int MAX_SOURCE_BUFFERS = 16;

namespace mozilla {

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
    return UnspecifiedNaN();
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
  if (!IsTypeSupportedInternal(aType, aRv)) {
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
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }
  nsRefPtr<SourceBuffer> sourceBuffer = new SourceBuffer(this, NS_ConvertUTF16toUTF8(mimeType));
  mSourceBuffers->Append(sourceBuffer);
  LOG(PR_LOG_DEBUG, ("%p AddSourceBuffer(Type=%s) -> %p", this,
                     NS_ConvertUTF16toUTF8(mimeType).get(), sourceBuffer.get()));
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
  EndOfStreamInternal(aError, aRv);
}

/* static */ bool
MediaSource::IsTypeSupported(const GlobalObject& aGlobal,
                             const nsAString& aType)
{
  ErrorResult unused;
  return IsTypeSupportedInternal(aType, unused);
}

bool
MediaSource::Attach(MediaSourceDecoder* aDecoder)
{
  LOG(PR_LOG_DEBUG, ("%p Attaching decoder %p owner %p", this, aDecoder, aDecoder->GetOwner()));
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
  LOG(PR_LOG_DEBUG, ("%p Detaching decoder %p owner %p", this, mDecoder.get(), mDecoder->GetOwner()));
  MOZ_ASSERT(mDecoder);
  mDecoder->DetachMediaSource();
  mDecoder = nullptr;
  mDuration = UnspecifiedNaN();
  mActiveSourceBuffers->Clear();
  mSourceBuffers->Clear();
  SetReadyState(MediaSourceReadyState::Closed);
}

MediaSource::MediaSource(nsPIDOMWindow* aWindow)
  : nsDOMEventTargetHelper(aWindow)
  , mDuration(UnspecifiedNaN())
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

  if ((mReadyState == MediaSourceReadyState::Closed ||
       mReadyState == MediaSourceReadyState::Ended) &&
      aState == MediaSourceReadyState::Open) {
    mReadyState = aState;
    QueueAsyncSimpleEvent("sourceopen");
    return;
  }

  if (mReadyState == MediaSourceReadyState::Open &&
      aState == MediaSourceReadyState::Ended) {
    mReadyState = aState;
    QueueAsyncSimpleEvent("sourceended");
    return;
  }

  if ((mReadyState == MediaSourceReadyState::Open ||
       mReadyState == MediaSourceReadyState::Ended) &&
      aState == MediaSourceReadyState::Closed) {
    mReadyState = aState;
    QueueAsyncSimpleEvent("sourceclose");
    return;
  }

  NS_WARNING("Invalid MediaSource readyState transition");
}

void
MediaSource::GetBuffered(TimeRanges* aRanges)
{
  if (mActiveSourceBuffers->Length() == 0) {
    return;
  }
  // TODO: Implement intersection computation.
}

void
MediaSource::DispatchSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Dispatching event %s to MediaSource", this, aName));
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
MediaSource::QueueAsyncSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Queuing event %s to MediaSource", this, aName));
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

void
MediaSource::EndOfStreamInternal(const Optional<MediaSourceEndOfStreamError>& aError, ErrorResult& aRv)
{
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

static const char* const gMediaSourceTypes[5] = {
  "video/webm",
  "audio/webm",
  "video/mp4",
  "audio/mp4",
  nullptr
};

/* static */ bool
MediaSource::IsTypeSupportedInternal(const nsAString& aType, ErrorResult& aRv)
{
  if (aType.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return false;
  }
  // TODO: Further restrict this to formats in the spec.
  nsContentTypeParser parser(aType);
  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return false;
  }
  bool found = false;
  for (uint32_t i = 0; gMediaSourceTypes[i]; ++i) {
    if (mimeType.EqualsASCII(gMediaSourceTypes[i])) {
      found = true;
      break;
    }
  }
  if (!found) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return false;
  }
  // Check aType against HTMLMediaElement list of MIME types.  Since we've
  // already restricted the container format, this acts as a specific check
  // of any specified "codecs" parameter of aType.
  if (HTMLMediaElement::GetCanPlay(aType) == CANPLAY_NO) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return false;
  }
  return true;
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

NS_IMPL_CYCLE_COLLECTION_INHERITED_2(MediaSource, nsDOMEventTargetHelper,
                                     mSourceBuffers, mActiveSourceBuffers)

NS_IMPL_ADDREF_INHERITED(MediaSource, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaSource, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MediaSource)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::MediaSource)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

} // namespace dom

} // namespace mozilla
