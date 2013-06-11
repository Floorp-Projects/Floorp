/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContainerWriter_h_
#define ContainerWriter_h_

#include "nsTArray.h"
#include "nsAutoPtr.h"

namespace mozilla {
/**
 * ContainerWriter packs encoded track data into a specific media container.
 */
class ContainerWriter {
public:
  ContainerWriter()
    : mInitialized(false)
  {}
  virtual ~ContainerWriter() {}

  enum {
    END_OF_STREAM = 1 << 0
  };

  /**
   * Writes encoded track data from aBuffer to a packet, and insert this packet
   * into the internal stream of container writer. aDuration is the playback
   * duration of this packet in number of samples. aFlags is true with
   * END_OF_STREAM if this is the last packet of track.
   * Currently, WriteEncodedTrack doesn't support multiple tracks.
   */
  virtual nsresult WriteEncodedTrack(const nsTArray<uint8_t>& aBuffer,
                                     int aDuration, uint32_t aFlags = 0) = 0;

  enum {
    FLUSH_NEEDED = 1 << 0
  };

  /**
   * Copies the final container data to a buffer if it has accumulated enough
   * packets from WriteEncodedTrack. This buffer of data is appended to
   * aOutputBufs, and existing elements of aOutputBufs should not be modified.
   * aFlags is true with FLUSH_NEEDED will force OggWriter to flush an ogg page
   * even it is not full, and copy these container data to a buffer for
   * aOutputBufs to append.
   */
  virtual nsresult GetContainerData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                                    uint32_t aFlags = 0) = 0;

protected:
  bool mInitialized;
};
}
#endif
