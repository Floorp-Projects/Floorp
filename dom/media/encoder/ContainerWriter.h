/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContainerWriter_h_
#define ContainerWriter_h_

#include "nsTArray.h"
#include "EncodedFrame.h"
#include "TrackMetadataBase.h"

namespace mozilla {
/**
 * ContainerWriter packs encoded track data into a specific media container.
 */
class ContainerWriter {
 public:
  ContainerWriter() : mInitialized(false), mIsWritingComplete(false) {}
  virtual ~ContainerWriter() {}
  // Mapping to DOMMediaStream::TrackTypeHints
  enum {
    CREATE_AUDIO_TRACK = 1 << 0,
    CREATE_VIDEO_TRACK = 1 << 1,
  };
  enum { END_OF_STREAM = 1 << 0 };

  /**
   * Writes encoded track data from aData into the internal stream of container
   * writer. aFlags is used to signal the impl of different conditions
   * such as END_OF_STREAM. Each impl may handle different flags, and should be
   * documented accordingly. Currently, WriteEncodedTrack doesn't support
   * explicit track specification, though each impl may provide logic to
   * allocate frames into different tracks.
   */
  virtual nsresult WriteEncodedTrack(
      const nsTArray<RefPtr<EncodedFrame>>& aData, uint32_t aFlags = 0) = 0;

  /**
   * Stores the metadata for all given tracks to the muxer.
   *
   * This method checks the integrity of aMetadata.
   * If the metadata isn't well formatted, this method returns NS_ERROR_FAILURE.
   * If the metadata is well formatted, it stores the metadata and returns
   * NS_OK.
   */
  virtual nsresult SetMetadata(
      const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) = 0;

  /**
   * Indicate if the writer has finished to output data
   */
  virtual bool IsWritingComplete() { return mIsWritingComplete; }

  enum { FLUSH_NEEDED = 1 << 0, GET_HEADER = 1 << 1 };

  /**
   * Copies the final container data to a buffer if it has accumulated enough
   * packets from WriteEncodedTrack. This buffer of data is appended to
   * aOutputBufs, and existing elements of aOutputBufs should not be modified.
   * aFlags is true with FLUSH_NEEDED will force OggWriter to flush an ogg page
   * even it is not full, and copy these container data to a buffer for
   * aOutputBufs to append.
   */
  virtual nsresult GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                                    uint32_t aFlags = 0) = 0;

 protected:
  bool mInitialized;
  bool mIsWritingComplete;
};

}  // namespace mozilla

#endif
