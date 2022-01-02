/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OggWriter_h_
#define OggWriter_h_

#include "ContainerWriter.h"
#include "OpusTrackEncoder.h"
#include <ogg/ogg.h>

namespace mozilla {
/**
 * WriteEncodedTrack inserts raw packets into Ogg stream (ogg_stream_state), and
 * GetContainerData outputs an ogg_page when enough packets have been written
 * to the Ogg stream.
 * For more details, please reference:
 * http://www.xiph.org/ogg/doc/libogg/encoding.html
 */
class OggWriter : public ContainerWriter {
 public:
  OggWriter();
  ~OggWriter();

  // Write frames into the ogg container. aFlags should be set to END_OF_STREAM
  // for the final set of frames.
  nsresult WriteEncodedTrack(const nsTArray<RefPtr<EncodedFrame>>& aData,
                             uint32_t aFlags = 0) override;

  nsresult GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                            uint32_t aFlags = 0) override;

  // Check metadata type integrity and reject unacceptable track encoder.
  nsresult SetMetadata(
      const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) override;

 private:
  nsresult Init();

  nsresult WriteEncodedData(const nsTArray<uint8_t>& aBuffer, int aDuration,
                            uint32_t aFlags = 0);

  void ProduceOggPage(nsTArray<nsTArray<uint8_t>>* aOutputBufs);
  // Store the Medatata from track encoder
  RefPtr<OpusMetadata> mMetadata;

  ogg_stream_state mOggStreamState;
  ogg_page mOggPage;
  ogg_packet mPacket;
};

}  // namespace mozilla

#endif
