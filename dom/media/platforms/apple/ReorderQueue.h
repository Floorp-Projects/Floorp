/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Queue for ordering decoded video frames by presentation time.
// Decoders often return frames out of order, which we need to
// buffer so we can forward them in correct presentation order.

#ifndef mozilla_ReorderQueue_h
#define mozilla_ReorderQueue_h

#include <MediaData.h>
#include <nsTPriorityQueue.h>

namespace mozilla {

struct ReorderQueueComparator
{
  bool LessThan(MediaData* const& a, MediaData* const& b) const
  {
    return a->mTime < b->mTime;
  }
};

typedef nsTPriorityQueue<RefPtr<MediaData>, ReorderQueueComparator> ReorderQueue;

} // namespace mozilla

#endif // mozilla_ReorderQueue_h
