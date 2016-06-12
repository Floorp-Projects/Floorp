/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SourceBuffer_h_
#define mozilla_dom_SourceBuffer_h_

#include "mozilla/MozPromise.h"
#include "MediaSource.h"
#include "js/RootingAPI.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SourceBufferBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/mozalloc.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nscore.h"
#include "TrackBuffersManager.h"
#include "SourceBufferTask.h"

class JSObject;
struct JSContext;

namespace mozilla {

class ErrorResult;
class MediaByteBuffer;
template <typename T> class AsyncEventRunner;

namespace dom {

class TimeRanges;

class SourceBuffer final : public DOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  SourceBufferAppendMode Mode() const
  {
    return mCurrentAttributes.GetAppendMode();
  }

  void SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv);

  bool Updating() const
  {
    return mUpdating;
  }

  TimeRanges* GetBuffered(ErrorResult& aRv);
  media::TimeIntervals GetTimeIntervals();

  double TimestampOffset() const
  {
    return mCurrentAttributes.GetApparentTimestampOffset();
  }

  void SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv);

  double AppendWindowStart() const
  {
    return mCurrentAttributes.GetAppendWindowStart();
  }

  void SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv);

  double AppendWindowEnd() const
  {
    return mCurrentAttributes.GetAppendWindowEnd();
  }

  void SetAppendWindowEnd(double aAppendWindowEnd, ErrorResult& aRv);

  void AppendBuffer(const ArrayBuffer& aData, ErrorResult& aRv);
  void AppendBuffer(const ArrayBufferView& aData, ErrorResult& aRv);

  void Abort(ErrorResult& aRv);
  void AbortBufferAppend();

  void Remove(double aStart, double aEnd, ErrorResult& aRv);
  /** End WebIDL Methods. */

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SourceBuffer, DOMEventTargetHelper)

  SourceBuffer(MediaSource* aMediaSource, const nsACString& aType);

  MediaSource* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // Notify the SourceBuffer that it has been detached from the
  // MediaSource's sourceBuffer list.
  void Detach();
  bool IsAttached() const
  {
    return mMediaSource != nullptr;
  }

  void Ended();

  double GetBufferedStart();
  double GetBufferedEnd();

  // Runs the range removal algorithm as defined by the MSE spec.
  void RangeRemoval(double aStart, double aEnd);

  bool IsActive() const
  {
    return mActive;
  }

private:
  ~SourceBuffer();

  friend class AsyncEventRunner<SourceBuffer>;
  friend class BufferAppendRunnable;
  friend class mozilla::TrackBuffersManager;
  void DispatchSimpleEvent(const char* aName);
  void QueueAsyncSimpleEvent(const char* aName);

  // Update mUpdating and fire the appropriate events.
  void StartUpdating();
  void StopUpdating();
  void AbortUpdating();
  void ResetParserState();

  // If the media segment contains data beyond the current duration,
  // then run the duration change algorithm with new duration set to the
  // maximum of the current duration and the group end timestamp.
  void CheckEndTime();

  // Shared implementation of AppendBuffer overloads.
  void AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv);

  // Implement the "Append Error Algorithm".
  // Will call endOfStream() with "decode" error if aDecodeError is true.
  // 3.5.3 Append Error Algorithm
  // http://w3c.github.io/media-source/#sourcebuffer-append-error
  void AppendError(bool aDecoderError);

  // Implements the "Prepare Append Algorithm". Returns MediaByteBuffer object
  // on success or nullptr (with aRv set) on error.
  already_AddRefed<MediaByteBuffer> PrepareAppend(const uint8_t* aData,
                                                  uint32_t aLength,
                                                  ErrorResult& aRv);

  void AppendDataCompletedWithSuccess(SourceBufferTask::AppendBufferResult aResult);
  void AppendDataErrored(nsresult aError);

  RefPtr<MediaSource> mMediaSource;

  RefPtr<TrackBuffersManager> mTrackBuffersManager;
  SourceBufferAttributes mCurrentAttributes;

  bool mUpdating;

  mozilla::Atomic<bool> mActive;

  MozPromiseRequestHolder<SourceBufferTask::AppendPromise> mPendingAppend;
  const nsCString mType;

  RefPtr<TimeRanges> mBuffered;
};

} // namespace dom

} // namespace mozilla

#endif /* mozilla_dom_SourceBuffer_h_ */
