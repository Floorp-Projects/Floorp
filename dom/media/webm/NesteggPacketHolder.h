/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(NesteggPacketHolder_h_)
#define NesteggPacketHolder_h_

#include <stdint.h>
#include "nsAutoRef.h"
#include "nestegg/nestegg.h"

namespace mozilla {

// Holds a nestegg_packet, and its file offset. This is needed so we
// know the offset in the file we've played up to, in order to calculate
// whether it's likely we can play through to the end without needing
// to stop to buffer, given the current download rate.
class NesteggPacketHolder {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NesteggPacketHolder)
  NesteggPacketHolder() : mPacket(nullptr), mOffset(-1), mTimestamp(-1), mIsKeyframe(false) {}

  bool Init(nestegg_packet* aPacket, int64_t aOffset, unsigned aTrack, bool aIsKeyframe)
  {
    uint64_t timestamp_ns;
    if (nestegg_packet_tstamp(aPacket, &timestamp_ns) == -1) {
      return false;
    }

    // We store the timestamp as signed microseconds so that it's easily
    // comparable to other timestamps we have in the system.
    mTimestamp = timestamp_ns / 1000;
    mPacket = aPacket;
    mOffset = aOffset;
    mTrack = aTrack;
    mIsKeyframe = aIsKeyframe;

    return true;
  }

  nestegg_packet* Packet() { MOZ_ASSERT(IsInitialized()); return mPacket; }
  int64_t Offset() { MOZ_ASSERT(IsInitialized()); return mOffset; }
  int64_t Timestamp() { MOZ_ASSERT(IsInitialized()); return mTimestamp; }
  unsigned Track() { MOZ_ASSERT(IsInitialized()); return mTrack; }
  bool IsKeyframe() { MOZ_ASSERT(IsInitialized()); return mIsKeyframe; }

private:
  ~NesteggPacketHolder()
  {
    nestegg_free_packet(mPacket);
  }

  bool IsInitialized() { return mOffset >= 0; }

  nestegg_packet* mPacket;

  // Offset in bytes. This is the offset of the end of the Block
  // which contains the packet.
  int64_t mOffset;

  // Packet presentation timestamp in microseconds.
  int64_t mTimestamp;

  // Track ID.
  unsigned mTrack;

  // Does this packet contain a keyframe?
  bool mIsKeyframe;

  // Copy constructor and assignment operator not implemented. Don't use them!
  NesteggPacketHolder(const NesteggPacketHolder &aOther);
  NesteggPacketHolder& operator= (NesteggPacketHolder const& aOther);
};

} // namespace mozilla

#endif

