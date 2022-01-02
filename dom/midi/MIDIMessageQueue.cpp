/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MIDIMessageQueue.h"
#include "mozilla/dom/MIDITypes.h"

namespace mozilla::dom {

MIDIMessageQueue::MIDIMessageQueue() : mMutex("MIDIMessageQueue::mMutex") {}

class MIDIMessageTimestampComparator {
 public:
  bool Equals(const MIDIMessage& a, const MIDIMessage& b) const {
    return a.timestamp() == b.timestamp();
  }
  bool LessThan(const MIDIMessage& a, const MIDIMessage& b) const {
    return a.timestamp() < b.timestamp();
  }
};

void MIDIMessageQueue::Add(nsTArray<MIDIMessage>& aMsg) {
  MutexAutoLock lock(mMutex);
  for (auto msg : aMsg) {
    mMessageQueue.InsertElementSorted(msg, MIDIMessageTimestampComparator());
  }
}

void MIDIMessageQueue::GetMessagesBefore(TimeStamp aTimestamp,
                                         nsTArray<MIDIMessage>& aMsgQueue) {
  MutexAutoLock lock(mMutex);
  int i = 0;
  for (auto msg : mMessageQueue) {
    if (aTimestamp < msg.timestamp()) {
      break;
    }
    aMsgQueue.AppendElement(msg);
    i++;
  }
  if (i > 0) {
    mMessageQueue.RemoveElementsAt(0, i);
  }
}

void MIDIMessageQueue::GetMessages(nsTArray<MIDIMessage>& aMsgQueue) {
  MutexAutoLock lock(mMutex);
  aMsgQueue.AppendElements(mMessageQueue);
  mMessageQueue.Clear();
}

void MIDIMessageQueue::Clear() {
  MutexAutoLock lock(mMutex);
  mMessageQueue.Clear();
}

void MIDIMessageQueue::ClearAfterNow() {
  MutexAutoLock lock(mMutex);
  TimeStamp now = TimeStamp::Now();
  int i = 0;
  for (auto msg : mMessageQueue) {
    if (now < msg.timestamp()) {
      break;
    }
    i++;
  }
  if (i > 0) {
    mMessageQueue.RemoveElementsAt(0, i);
  }
}

}  // namespace mozilla::dom
