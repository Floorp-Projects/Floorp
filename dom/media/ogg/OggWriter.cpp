/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OggWriter.h"
#include "prtime.h"
#include "mozilla/ProfilerLabels.h"

#define LOG(args, ...)

namespace mozilla {

OggWriter::OggWriter()
    : ContainerWriter(), mOggStreamState(), mOggPage(), mPacket() {
  if (NS_FAILED(Init())) {
    LOG("ERROR! Fail to initialize the OggWriter.");
  }
}

OggWriter::~OggWriter() {
  if (mInitialized) {
    ogg_stream_clear(&mOggStreamState);
  }
  // mPacket's data was always owned by us, no need to ogg_packet_clear.
}

nsresult OggWriter::Init() {
  MOZ_ASSERT(!mInitialized);

  // The serial number (serialno) should be a random number, for the current
  // implementation where the output file contains only a single stream, this
  // serialno is used to differentiate between files.
  srand(static_cast<unsigned>(PR_Now()));
  int rc = ogg_stream_init(&mOggStreamState, rand());

  mPacket.b_o_s = 1;
  mPacket.e_o_s = 0;
  mPacket.granulepos = 0;
  mPacket.packet = nullptr;
  mPacket.packetno = 0;
  mPacket.bytes = 0;

  mInitialized = (rc == 0);

  return (rc == 0) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

nsresult OggWriter::WriteEncodedTrack(
    const nsTArray<RefPtr<EncodedFrame>>& aData, uint32_t aFlags) {
  AUTO_PROFILER_LABEL("OggWriter::WriteEncodedTrack", OTHER);

  uint32_t len = aData.Length();
  for (uint32_t i = 0; i < len; i++) {
    if (aData[i]->mFrameType != EncodedFrame::OPUS_AUDIO_FRAME) {
      LOG("[OggWriter] wrong encoded data type!");
      return NS_ERROR_FAILURE;
    }

    // only pass END_OF_STREAM on the last frame!
    nsresult rv = WriteEncodedData(
        *aData[i]->mFrameData, aData[i]->mDuration,
        i < len - 1 ? (aFlags & ~ContainerWriter::END_OF_STREAM) : aFlags);
    if (NS_FAILED(rv)) {
      LOG("%p Failed to WriteEncodedTrack!", this);
      return rv;
    }
  }
  return NS_OK;
}

nsresult OggWriter::WriteEncodedData(const nsTArray<uint8_t>& aBuffer,
                                     int aDuration, uint32_t aFlags) {
  if (!mInitialized) {
    LOG("[OggWriter] OggWriter has not initialized!");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!ogg_stream_eos(&mOggStreamState),
             "No data can be written after eos has marked.");

  // Set eos flag to true, and once the eos is written to a packet, there must
  // not be anymore pages after a page has marked as eos.
  if (aFlags & ContainerWriter::END_OF_STREAM) {
    LOG("[OggWriter] Set e_o_s flag to true.");
    mPacket.e_o_s = 1;
  }

  mPacket.packet = const_cast<uint8_t*>(aBuffer.Elements());
  mPacket.bytes = aBuffer.Length();
  mPacket.granulepos += aDuration;

  // 0 returned on success. -1 returned in the event of internal error.
  // The data in the packet is copied into the internal storage managed by the
  // mOggStreamState, so we are free to alter the contents of mPacket after
  // this call has returned.
  int rc = ogg_stream_packetin(&mOggStreamState, &mPacket);
  if (rc < 0) {
    LOG("[OggWriter] Failed in ogg_stream_packetin! (%d).", rc);
    return NS_ERROR_FAILURE;
  }

  if (mPacket.b_o_s) {
    mPacket.b_o_s = 0;
  }
  mPacket.packetno++;
  mPacket.packet = nullptr;

  return NS_OK;
}

void OggWriter::ProduceOggPage(nsTArray<nsTArray<uint8_t>>* aOutputBufs) {
  aOutputBufs->AppendElement();
  aOutputBufs->LastElement().SetLength(mOggPage.header_len + mOggPage.body_len);
  memcpy(aOutputBufs->LastElement().Elements(), mOggPage.header,
         mOggPage.header_len);
  memcpy(aOutputBufs->LastElement().Elements() + mOggPage.header_len,
         mOggPage.body, mOggPage.body_len);
}

nsresult OggWriter::GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                                     uint32_t aFlags) {
  int rc = -1;
  AUTO_PROFILER_LABEL("OggWriter::GetContainerData", OTHER);
  // Generate the oggOpus Header
  if (aFlags & ContainerWriter::GET_HEADER) {
    OpusMetadata* meta = static_cast<OpusMetadata*>(mMetadata.get());
    NS_ASSERTION(meta, "should have meta data");
    NS_ASSERTION(meta->GetKind() == TrackMetadataBase::METADATA_OPUS,
                 "should have Opus meta data");

    nsresult rv = WriteEncodedData(meta->mIdHeader, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rc = ogg_stream_flush(&mOggStreamState, &mOggPage);
    NS_ENSURE_TRUE(rc > 0, NS_ERROR_FAILURE);
    ProduceOggPage(aOutputBufs);

    rv = WriteEncodedData(meta->mCommentHeader, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rc = ogg_stream_flush(&mOggStreamState, &mOggPage);
    NS_ENSURE_TRUE(rc > 0, NS_ERROR_FAILURE);

    // Force generate a page even if the amount of packet data is not enough.
    // Usually do so after a header packet.

    ProduceOggPage(aOutputBufs);
  }

  // return value 0 means insufficient data has accumulated to fill a page, or
  // an internal error has occurred.
  while (ogg_stream_pageout(&mOggStreamState, &mOggPage) != 0) {
    ProduceOggPage(aOutputBufs);
  }

  if (aFlags & ContainerWriter::FLUSH_NEEDED) {
    // return value 0 means no packet to put into a page, or an internal error.
    if (ogg_stream_flush(&mOggStreamState, &mOggPage) != 0) {
      ProduceOggPage(aOutputBufs);
    }
    mIsWritingComplete = true;
  }

  // We always return NS_OK here since it's OK to call this without having
  // enough data to fill a page. It's the more common case compared to internal
  // errors, and we cannot distinguish the two.
  return NS_OK;
}

nsresult OggWriter::SetMetadata(
    const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) {
  MOZ_ASSERT(aMetadata.Length() == 1);
  MOZ_ASSERT(aMetadata[0]);

  AUTO_PROFILER_LABEL("OggWriter::SetMetadata", OTHER);

  if (aMetadata[0]->GetKind() != TrackMetadataBase::METADATA_OPUS) {
    LOG("wrong meta data type!");
    return NS_ERROR_FAILURE;
  }
  // Validate each field of METADATA
  mMetadata = static_cast<OpusMetadata*>(aMetadata[0].get());
  if (mMetadata->mIdHeader.Length() == 0) {
    LOG("miss mIdHeader!");
    return NS_ERROR_FAILURE;
  }
  if (mMetadata->mCommentHeader.Length() == 0) {
    LOG("miss mCommentHeader!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla

#undef LOG
