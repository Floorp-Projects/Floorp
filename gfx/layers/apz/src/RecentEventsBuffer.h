/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RecentEventsBuffer_h
#define mozilla_layers_RecentEventsBuffer_h

#include <deque>

#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {
/**
 * RecentEventsBuffer: maintains an age constrained buffer of events
 *
 * Intended for use with elements of type InputData, but the only requirement
 * is a member "mTimeStamp" of type TimeStamp
 */
template <typename Event>
class RecentEventsBuffer {
 public:
  explicit RecentEventsBuffer(TimeDuration maxAge);

  void push(Event event);
  void clear();

  typedef typename std::deque<Event>::size_type size_type;
  size_type size() { return mBuffer.size(); }

  // Delegate to container for iterators
  typedef typename std::deque<Event>::iterator iterator;
  typedef typename std::deque<Event>::const_iterator const_iterator;
  iterator begin() { return mBuffer.begin(); }
  iterator end() { return mBuffer.end(); }
  const_iterator cbegin() const { return mBuffer.cbegin(); }
  const_iterator cend() const { return mBuffer.cend(); }

  // Also delegate for front/back
  typedef typename std::deque<Event>::reference reference;
  typedef typename std::deque<Event>::const_reference const_reference;
  reference front() { return mBuffer.front(); }
  reference back() { return mBuffer.back(); }
  const_reference front() const { return mBuffer.front(); }
  const_reference back() const { return mBuffer.back(); }

 private:
  TimeDuration mMaxAge;
  std::deque<Event> mBuffer;
};

template <typename Event>
RecentEventsBuffer<Event>::RecentEventsBuffer(TimeDuration maxAge)
    : mMaxAge(maxAge), mBuffer() {}

template <typename Event>
void RecentEventsBuffer<Event>::push(Event event) {
  // Events must be pushed in chronological order
  MOZ_ASSERT(mBuffer.empty() || mBuffer.back().mTimeStamp <= event.mTimeStamp);

  mBuffer.push_back(event);

  // Flush all events older than the given lifetime
  TimeStamp bound = event.mTimeStamp - mMaxAge;
  while (!mBuffer.empty()) {
    if (mBuffer.front().mTimeStamp >= bound) {
      break;
    }
    mBuffer.pop_front();
  }
}

template <typename Event>
void RecentEventsBuffer<Event>::clear() {
  mBuffer.clear();
}

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_RecentEventsBuffer_h
