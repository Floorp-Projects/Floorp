/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OggWriter_h_
#define OggWriter_h_

#include "ContainerWriter.h"
#include <ogg/ogg.h>

namespace mozilla {
/**
 * WriteEncodedTrack inserts raw packets into Ogg stream (ogg_stream_state), and
 * GetContainerData outputs an ogg_page when enough packets have been written
 * to the Ogg stream.
 * For more details, please reference:
 * http://www.xiph.org/ogg/doc/libogg/encoding.html
 */
class OggWriter : public ContainerWriter
{
public:
  OggWriter();

  nsresult WriteEncodedTrack(const nsTArray<uint8_t>& aBuffer, int aDuration,
                             uint32_t aFlags = 0) MOZ_OVERRIDE;

  nsresult GetContainerData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                            uint32_t aFlags = 0) MOZ_OVERRIDE;

private:
  nsresult Init();

  ogg_stream_state mOggStreamState;
  ogg_page mOggPage;
  ogg_packet mPacket;
};
}
#endif
