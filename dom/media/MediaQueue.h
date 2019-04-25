/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaQueue_h_)
#  define MediaQueue_h_

#  include "mozilla/RecursiveMutex.h"
#  include "mozilla/TaskQueue.h"

#  include "nsDeque.h"
#  include "MediaEventSource.h"
#  include "TimeUnits.h"

namespace mozilla {

class AudioData;

// Thread and type safe wrapper around nsDeque.
template <class T>
class MediaQueueDeallocator : public nsDequeFunctor {
  virtual void operator()(void* aObject) override {
    RefPtr<T> releaseMe = dont_AddRef(static_cast<T*>(aObject));
  }
};

template <class T>
class MediaQueue : private nsDeque {
 public:
  MediaQueue()
      : nsDeque(new MediaQueueDeallocator<T>()),
        mRecursiveMutex("mediaqueue"),
        mEndOfStream(false) {}

  ~MediaQueue() { Reset(); }

  inline size_t GetSize() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return nsDeque::GetSize();
  }

  inline void Push(T* aItem) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    MOZ_DIAGNOSTIC_ASSERT(aItem);
    NS_ADDREF(aItem);
    MOZ_ASSERT(aItem->GetEndTime() >= aItem->mTime);
    nsDeque::Push(aItem);
    mPushEvent.Notify(RefPtr<T>(aItem));
    // Pushing new data after queue has ended means that the stream is active
    // again, so we should not mark it as ended.
    if (mEndOfStream) {
      mEndOfStream = false;
    }
  }

  inline already_AddRefed<T> PopFront() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    RefPtr<T> rv = dont_AddRef(static_cast<T*>(nsDeque::PopFront()));
    if (rv) {
      mPopFrontEvent.Notify(rv);
    }
    return rv.forget();
  }

  inline already_AddRefed<T> PopBack() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    RefPtr<T> rv = dont_AddRef(static_cast<T*>(nsDeque::Pop()));
    return rv.forget();
  }

  inline RefPtr<T> PeekFront() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return static_cast<T*>(nsDeque::PeekFront());
  }

  inline RefPtr<T> PeekBack() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return static_cast<T*>(nsDeque::Peek());
  }

  void Reset() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    while (GetSize() > 0) {
      RefPtr<T> x = dont_AddRef(static_cast<T*>(nsDeque::PopFront()));
    }
    mEndOfStream = false;
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
  int64_t Duration() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (GetSize() == 0) {
      return 0;
    }
    T* last = static_cast<T*>(nsDeque::Peek());
    T* first = static_cast<T*>(nsDeque::PeekFront());
    return (last->GetEndTime() - first->mTime).ToMicroseconds();
  }

  void LockedForEach(nsDequeFunctor& aFunctor) const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    ForEach(aFunctor);
  }

  // Extracts elements from the queue into aResult, in order.
  // Elements whose start time is before aTime are ignored.
  void GetElementsAfter(int64_t aTime, nsTArray<RefPtr<T>>* aResult) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    if (GetSize() == 0) return;
    size_t i;
    for (i = GetSize() - 1; i > 0; --i) {
      T* v = static_cast<T*>(ObjectAt(i));
      if (v->GetEndTime().ToMicroseconds() < aTime) break;
    }
    // Elements less than i have a end time before aTime. It's also possible
    // that the element at i has a end time before aTime, but that's OK.
    for (; i < GetSize(); ++i) {
      RefPtr<T> elem = static_cast<T*>(ObjectAt(static_cast<size_t>(i)));
      aResult->AppendElement(elem);
    }
  }

  void GetElementsAfter(const media::TimeUnit& aTime,
                        nsTArray<RefPtr<T>>* aResult) {
    GetElementsAfter(aTime.ToMicroseconds(), aResult);
  }

  void GetFirstElements(uint32_t aMaxElements, nsTArray<RefPtr<T>>* aResult) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    for (size_t i = 0; i < aMaxElements && i < GetSize(); ++i) {
      *aResult->AppendElement() = static_cast<T*>(ObjectAt(i));
    }
  }

  uint32_t AudioFramesCount() {
    static_assert(mozilla::IsSame<T, AudioData>::value,
                  "Only usable with MediaQueue<AudioData>");
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    uint32_t frames = 0;
    for (size_t i = 0; i < GetSize(); ++i) {
      T* v = static_cast<T*>(ObjectAt(i));
      frames += v->Frames();
    }
    return frames;
  }

  MediaEventSource<RefPtr<T>>& PopFrontEvent() { return mPopFrontEvent; }

  MediaEventSource<RefPtr<T>>& PushEvent() { return mPushEvent; }

  MediaEventSource<void>& FinishEvent() { return mFinishEvent; }

 private:
  mutable RecursiveMutex mRecursiveMutex;
  MediaEventProducer<RefPtr<T>> mPopFrontEvent;
  MediaEventProducer<RefPtr<T>> mPushEvent;
  MediaEventProducer<void> mFinishEvent;
  // True when we've decoded the last frame of data in the
  // bitstream for which we're queueing frame data.
  bool mEndOfStream;
};

}  // namespace mozilla

#endif
