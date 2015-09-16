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
#include "mozilla/dom/SourceBufferBinding.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/mozalloc.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nscore.h"
#include "SourceBufferContentManager.h"
#include "mozilla/Monitor.h"

class JSObject;
struct JSContext;

namespace mozilla {

class ErrorResult;
class MediaByteBuffer;
template <typename T> class AsyncEventRunner;
class TrackBuffersManager;

namespace dom {

class TimeRanges;

class SourceBufferAttributes {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SourceBufferAttributes);
  explicit SourceBufferAttributes(bool aGenerateTimestamp)
    : mGenerateTimestamps(aGenerateTimestamp)
    , mMonitor("SourceBufferAttributes")
    , mAppendWindowStart(0)
    , mAppendWindowEnd(PositiveInfinity<double>())
    , mAppendMode(SourceBufferAppendMode::Segments)
    , mApparentTimestampOffset(0)
  {}

  double GetAppendWindowStart()
  {
    MonitorAutoLock mon(mMonitor);
    return mAppendWindowStart;
  }

  double GetAppendWindowEnd()
  {
    MonitorAutoLock mon(mMonitor);
    return mAppendWindowEnd;
  }

  void SetAppendWindowStart(double aWindowStart)
  {
    MonitorAutoLock mon(mMonitor);
    mAppendWindowStart = aWindowStart;
  }

  void SetAppendWindowEnd(double aWindowEnd)
  {
    MonitorAutoLock mon(mMonitor);
    mAppendWindowEnd = aWindowEnd;
  }

  double GetApparentTimestampOffset()
  {
    MonitorAutoLock mon(mMonitor);
    return mApparentTimestampOffset;
  }

  void SetApparentTimestampOffset(double aTimestampOffset)
  {
    MonitorAutoLock mon(mMonitor);
    mApparentTimestampOffset = aTimestampOffset;
    mTimestampOffset = media::TimeUnit::FromSeconds(aTimestampOffset);
  }

  media::TimeUnit GetTimestampOffset()
  {
    MonitorAutoLock mon(mMonitor);
    return mTimestampOffset;
  }

  void SetTimestampOffset(media::TimeUnit& aTimestampOffset)
  {
    MonitorAutoLock mon(mMonitor);
    mTimestampOffset = aTimestampOffset;
    mApparentTimestampOffset = aTimestampOffset.ToSeconds();
  }

  SourceBufferAppendMode GetAppendMode()
  {
    MonitorAutoLock mon(mMonitor);
    return mAppendMode;
  }

  void SetAppendMode(SourceBufferAppendMode aAppendMode)
  {
    MonitorAutoLock mon(mMonitor);
    mAppendMode = aAppendMode;
  }

  // mGenerateTimestamp isn't mutable once the source buffer has been constructed
  // We don't need a monitor to protect it across threads.
  const bool mGenerateTimestamps;

private:
  ~SourceBufferAttributes() {};

  // Monitor protecting all members below.
  Monitor mMonitor;
  double mAppendWindowStart;
  double mAppendWindowEnd;
  SourceBufferAppendMode mAppendMode;
  double mApparentTimestampOffset;
  media::TimeUnit mTimestampOffset;
};

class SourceBuffer final : public DOMEventTargetHelper
{
public:
  /** WebIDL Methods. */
  SourceBufferAppendMode Mode() const
  {
    return mAttributes->GetAppendMode();
  }

  void SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv);

  bool Updating() const
  {
    return mUpdating;
  }

  already_AddRefed<TimeRanges> GetBuffered(ErrorResult& aRv);
  media::TimeIntervals GetTimeIntervals();

  double TimestampOffset() const
  {
    return mAttributes->GetApparentTimestampOffset();
  }

  void SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv);

  double AppendWindowStart() const
  {
    return mAttributes->GetAppendWindowStart();
  }

  void SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv);

  double AppendWindowEnd() const
  {
    return mAttributes->GetAppendWindowEnd();
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

  // Evict data in the source buffer in the given time range.
  void Evict(double aStart, double aEnd);

  double GetBufferedStart();
  double GetBufferedEnd();

  // Runs the range removal algorithm as defined by the MSE spec.
  void RangeRemoval(double aStart, double aEnd);

  bool IsActive() const
  {
    return mActive;
  }

#if defined(DEBUG)
  void Dump(const char* aPath);
#endif

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

  // If the media segment contains data beyond the current duration,
  // then run the duration change algorithm with new duration set to the
  // maximum of the current duration and the group end timestamp.
  void CheckEndTime();

  // Shared implementation of AppendBuffer overloads.
  void AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv);
  void BufferAppend(uint32_t aAppendID);

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

  void AppendDataCompletedWithSuccess(bool aHasActiveTracks);
  void AppendDataErrored(nsresult aError);

  nsRefPtr<MediaSource> mMediaSource;

  uint32_t mEvictionThreshold;

  nsRefPtr<SourceBufferContentManager> mContentManager;
  nsRefPtr<SourceBufferAttributes> mAttributes;

  bool mUpdating;

  mozilla::Atomic<bool> mActive;

  // Each time mUpdating is set to true, mUpdateID will be incremented.
  // This allows for a queued AppendData task to identify if it was earlier
  // aborted and another AppendData queued.
  uint32_t mUpdateID;
  int64_t mReportedOffset;

  MozPromiseRequestHolder<SourceBufferContentManager::AppendPromise> mPendingAppend;
  const nsCString mType;
};

} // namespace dom

} // namespace mozilla

#endif /* mozilla_dom_SourceBuffer_h_ */
