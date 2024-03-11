/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaQueue_h_)
#  define MediaQueue_h_

#  include <type_traits>

#  include "mozilla/RecursiveMutex.h"
#  include "mozilla/TaskQueue.h"

#  include "nsDeque.h"
#  include "MediaEventSource.h"
#  include "TimeUnits.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#  define QLOG(msg, ...)                       \
    MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
            ("MediaQueue=%p " msg, this, ##__VA_ARGS__))

class AudioData;
class VideoData;
class EncodedFrame;

template <typename T>
struct TimestampAdjustmentTrait {
  static const bool mValue = false;
};

template <>
struct TimestampAdjustmentTrait<AudioData> {
  static const bool mValue = true;
};

template <>
struct TimestampAdjustmentTrait<VideoData> {
  static const bool mValue = true;
};

template <typename T>
struct NonTimestampAdjustmentTrait {
  static const bool mValue = !TimestampAdjustmentTrait<T>::mValue;
};

template <typename T>
struct DurationTypeTrait {
  using type = media::TimeUnit;
};

template <>
struct DurationTypeTrait<EncodedFrame> {
  using type = uint64_t;
};

template <class T>
class MediaQueue : private nsRefPtrDeque<T> {
 public:
  explicit MediaQueue(bool aEnablePreciseDuration = false)
      : nsRefPtrDeque<T>(),
        mRecursiveMutex("mediaqueue"),
        mEndOfStream(false),
        mEnablePreciseDuration(aEnablePreciseDuration) {}

  ~MediaQueue() { Reset(); }

  inline size_t GetSize() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return nsRefPtrDeque<T>::GetSize();
  }

  template <typename U,
            std::enable_if_t<TimestampAdjustmentTrait<U>::mValue, bool> = true>
  inline void AdjustTimeStampIfNeeded(U* aItem) {
    static_assert(std::is_same_v<U, AudioData> || std::is_same_v<U, VideoData>);
    if (mOffset != media::TimeUnit::Zero()) {
      const auto prev = aItem->mTime, prevEndTime = aItem->GetEndTime();
      aItem->mTime += mOffset;
      if (!aItem->mTime.IsValid()) {
        NS_WARNING("Reverting timestamp adjustment due to sample overflow!");
        aItem->mTime = prev;
      } else {
        QLOG("adjusted %s sample [%" PRId64 ",%" PRId64 "] -> [%" PRId64
             ",%" PRId64 "]",
             std::is_same_v<U, AudioData> ? "audio" : "video",
             prev.ToMicroseconds(), prevEndTime.ToMicroseconds(),
             aItem->mTime.ToMicroseconds(),
             aItem->GetEndTime().ToMicroseconds());
      }
    }
  }

  template <typename U, std::enable_if_t<NonTimestampAdjustmentTrait<U>::mValue,
                                         bool> = true>
  inline void AdjustTimeStampIfNeeded(U* aItem) {}

  enum class TimestampAdjustment {
    Enable,
    Disable,
  };
  inline void PushFront(
      T* aItem, TimestampAdjustment aIsEnabled = TimestampAdjustment::Enable) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (aIsEnabled == TimestampAdjustment::Enable) {
      AdjustTimeStampIfNeeded(aItem);
    }
    nsRefPtrDeque<T>::PushFront(aItem);
    AddDurationToPreciseDuration(aItem);
  }

  inline void Push(T* aItem) {
    MOZ_DIAGNOSTIC_ASSERT(aItem);
    Push(do_AddRef(aItem));
  }

  inline void Push(already_AddRefed<T> aItem) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    T* item = aItem.take();

    MOZ_DIAGNOSTIC_ASSERT(item);
    MOZ_DIAGNOSTIC_ASSERT(item->GetEndTime() >= item->mTime);
    AdjustTimeStampIfNeeded(item);
    nsRefPtrDeque<T>::Push(dont_AddRef(item));
    AddDurationToPreciseDuration(item);
    mPushEvent.Notify(RefPtr<T>(item));

    // Pushing new data after queue has ended means that the stream is active
    // again, so we should not mark it as ended.
    if (mEndOfStream) {
      mEndOfStream = false;
    }
  }

  inline already_AddRefed<T> PopFront() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    RefPtr<T> rv = nsRefPtrDeque<T>::PopFront();
    if (rv) {
      MOZ_DIAGNOSTIC_ASSERT(rv->GetEndTime() >= rv->mTime);
      SubtractDurationFromPreciseDuration(rv);
      mPopFrontEvent.Notify(RefPtr<T>(rv));
    }
    return rv.forget();
  }

  inline already_AddRefed<T> PopBack() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    RefPtr<T> rv = nsRefPtrDeque<T>::Pop();
    if (rv) {
      MOZ_DIAGNOSTIC_ASSERT(rv->GetEndTime() >= rv->mTime);
      SubtractDurationFromPreciseDuration(rv);
    }
    return rv.forget();
  }

  inline RefPtr<T> PeekFront() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return nsRefPtrDeque<T>::PeekFront();
  }

  inline RefPtr<T> PeekBack() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return nsRefPtrDeque<T>::Peek();
  }

  void Reset() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    nsRefPtrDeque<T>::Erase();
    SetOffset(media::TimeUnit::Zero());
    mEndOfStream = false;
    ResetPreciseDuration();
  }

  bool AtEndOfStream() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return GetSize() == 0 && mEndOfStream;
  }

  // Returns true if the media queue has had its last item added to it.
  // This happens when the media stream has been completely decoded. Note this
  // does not mean that the corresponding stream has finished playback.
  bool IsFinished() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mEndOfStream;
  }

  // Informs the media queue that it won't be receiving any more items.
  void Finish() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (!mEndOfStream) {
      mEndOfStream = true;
      mFinishEvent.Notify();
    }
  }

  // Returns the approximate number of microseconds of items in the queue.
  int64_t Duration() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (GetSize() == 0) {
      return 0;
    }
    T* last = nsRefPtrDeque<T>::Peek();
    T* first = nsRefPtrDeque<T>::PeekFront();
    return (last->GetEndTime() - first->mTime).ToMicroseconds();
  }

  // Return a precise duration if the feature is enabled. Otherwise, return -1.
  int64_t PreciseDuration() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return GetPreciseDuration();
  }

  void LockedForEach(nsDequeFunctor<T>& aFunctor) const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    nsRefPtrDeque<T>::ForEach(aFunctor);
  }

  // Fill aResult with the elements which end later than the given time aTime.
  void GetElementsAfter(const media::TimeUnit& aTime,
                        nsTArray<RefPtr<T>>* aResult) {
    GetElementsAfterStrict(aTime.ToMicroseconds(), aResult);
  }

  void GetFirstElements(uint32_t aMaxElements, nsTArray<RefPtr<T>>* aResult) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    for (size_t i = 0; i < aMaxElements && i < GetSize(); ++i) {
      *aResult->AppendElement() = nsRefPtrDeque<T>::ObjectAt(i);
    }
  }

  uint32_t AudioFramesCount() {
    static_assert(std::is_same_v<T, AudioData>,
                  "Only usable with MediaQueue<AudioData>");
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    uint32_t frames = 0;
    for (size_t i = 0; i < GetSize(); ++i) {
      T* v = nsRefPtrDeque<T>::ObjectAt(i);
      frames += v->Frames();
    }
    return frames;
  }

  bool SetOffset(const media::TimeUnit& aOffset) {
    if (!aOffset.IsValid()) {
      QLOG("Invalid offset!");
      return false;
    }
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    mOffset = aOffset;
    QLOG("Set media queue offset %" PRId64, mOffset.ToMicroseconds());
    return true;
  }

  media::TimeUnit GetOffset() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mOffset;
  }

  MediaEventSource<RefPtr<T>>& PopFrontEvent() { return mPopFrontEvent; }

  MediaEventSource<RefPtr<T>>& PushEvent() { return mPushEvent; }

  MediaEventSource<void>& FinishEvent() { return mFinishEvent; }

 private:
  // Extracts elements from the queue into aResult, in order.
  // Elements whose end time is before or equal to aTime are ignored.
  void GetElementsAfterStrict(int64_t aTime, nsTArray<RefPtr<T>>* aResult) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (GetSize() == 0) return;
    size_t i;
    for (i = GetSize() - 1; i > 0; --i) {
      T* v = nsRefPtrDeque<T>::ObjectAt(i);
      if (v->GetEndTime().ToMicroseconds() < aTime) break;
    }
    for (; i < GetSize(); ++i) {
      RefPtr<T> elem = nsRefPtrDeque<T>::ObjectAt(i);
      if (elem->GetEndTime().ToMicroseconds() > aTime) {
        aResult->AppendElement(elem);
      }
    }
  }

  mutable RecursiveMutex mRecursiveMutex MOZ_UNANNOTATED;
  MediaEventProducer<RefPtr<T>> mPopFrontEvent;
  MediaEventProducer<RefPtr<T>> mPushEvent;
  MediaEventProducer<void> mFinishEvent;
  // True when we've decoded the last frame of data in the
  // bitstream for which we're queueing frame data.
  bool mEndOfStream;
  // This offset will be added to any data pushed into the queue. We use it when
  // the media queue starts receiving looped data, which timestamp needs to be
  // modified.
  media::TimeUnit mOffset;

  inline void AddDurationToPreciseDuration(T* aItem) {
    if (!mEnablePreciseDuration) {
      return;
    }
    if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                 media::TimeUnit> ||
                  std::is_same_v<typename DurationTypeTrait<T>::type,
                                 uint64_t>) {
      mPreciseDuration += aItem->mDuration;
    }
  }

  inline void SubtractDurationFromPreciseDuration(T* aItem) {
    if (!mEnablePreciseDuration) {
      return;
    }
    if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                 media::TimeUnit> ||
                  std::is_same_v<typename DurationTypeTrait<T>::type,
                                 uint64_t>) {
      mPreciseDuration -= aItem->mDuration;
    }
  }

  inline void ResetPreciseDuration() {
    if (!mEnablePreciseDuration) {
      return;
    }
    if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                 media::TimeUnit>) {
      mPreciseDuration = media::TimeUnit::Zero();
    } else if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                        uint64_t>) {
      mPreciseDuration = 0;
    }
  }

  inline int64_t GetPreciseDuration() const {
    if (mEnablePreciseDuration) {
      if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                   media::TimeUnit>) {
        return mPreciseDuration.ToMicroseconds();
      } else if constexpr (std::is_same_v<typename DurationTypeTrait<T>::type,
                                          uint64_t>) {
        return mPreciseDuration;
      }
    }
    return -1;
  }

  typename DurationTypeTrait<T>::type mPreciseDuration;
  const bool mEnablePreciseDuration = false;
};

}  // namespace mozilla

#  undef QLOG

#endif
