/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EbmlComposer.h"
#include "libmkv/EbmlIDs.h"
#include "libmkv/EbmlWriter.h"
#include "libmkv/WebMElement.h"
#include "prtime.h"

namespace mozilla {

// Timecode scale in nanoseconds
static const unsigned long TIME_CODE_SCALE = 1000000;
// The WebM header size without audio CodecPrivateData
static const int32_t DEFAULT_HEADER_SIZE = 1024;

void EbmlComposer::GenerateHeader()
{
  // Write the EBML header.
  EbmlGlobal ebml;
  // The WEbM header default size usually smaller than 1k.
  nsAutoArrayPtr<uint8_t> buffer(new uint8_t[DEFAULT_HEADER_SIZE +
                                             mCodecPrivateData.Length()]);
  ebml.buf = buffer.get();
  ebml.offset = 0;
  writeHeader(&ebml);
  {
    EbmlLoc segEbmlLoc, ebmlLocseg, ebmlLoc;
    Ebml_StartSubElement(&ebml, &segEbmlLoc, Segment);
    {
      Ebml_StartSubElement(&ebml, &ebmlLocseg, SeekHead);
      // Todo: We don't know the exact sizes of encoded data and ignore this section.
      Ebml_EndSubElement(&ebml, &ebmlLocseg);
      writeSegmentInformation(&ebml, &ebmlLoc, TIME_CODE_SCALE, 0);
      {
        EbmlLoc trackLoc;
        Ebml_StartSubElement(&ebml, &trackLoc, Tracks);
        {
          // Video
          if (mWidth > 0 && mHeight > 0) {
            writeVideoTrack(&ebml, 0x1, 0, "V_VP8",
                            mWidth, mHeight,
                            mDisplayWidth, mDisplayHeight, mFrameRate);
          }
          // Audio
          if (mCodecPrivateData.Length() > 0) {
            writeAudioTrack(&ebml, 0x2, 0x0, "A_VORBIS", mSampleFreq,
                            mChannels, mCodecPrivateData.Elements(),
                            mCodecPrivateData.Length());
          }
        }
        Ebml_EndSubElement(&ebml, &trackLoc);
      }
    }
    // The Recording length is unknow and ignore write the whole Segment element size
  }
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE + mCodecPrivateData.Length(),
             "write more data > EBML_BUFFER_SIZE");
  mClusterBuffs.AppendElement();
  mClusterBuffs.LastElement().SetLength(ebml.offset);
  memcpy(mClusterBuffs.LastElement().Elements(), ebml.buf, ebml.offset);
}

void EbmlComposer::FinishCluster()
{
  MOZ_ASSERT(mClusterLengthLoc > 0 );
  MOZ_ASSERT(mClusterHeaderIndex > 0);
  for (uint32_t i = 0; i < mClusterBuffs.Length(); i ++ ) {
    mClusterCanFlushBuffs.AppendElement()->SwapElements(mClusterBuffs[i]);
  }
  mClusterBuffs.Clear();
  EbmlGlobal ebml;
  EbmlLoc ebmlLoc;
  ebmlLoc.offset = mClusterLengthLoc;
  ebml.offset = mClusterCanFlushBuffs[mClusterHeaderIndex].Length();
  ebml.buf = mClusterCanFlushBuffs[mClusterHeaderIndex].Elements();
  Ebml_EndSubElement(&ebml, &ebmlLoc);
  mClusterHeaderIndex = 0;
  mClusterLengthLoc = 0;
}

void
EbmlComposer::WriteSimpleBlock(EncodedFrame* aFrame)
{
  EbmlGlobal ebml;
  ebml.offset = 0;

  if (aFrame->GetFrameType() == EncodedFrame::FrameType::VP8_I_FRAME && mClusterHeaderIndex > 0) {
    FinishCluster();
  }

  mClusterBuffs.AppendElement();
  mClusterBuffs.LastElement().SetLength(aFrame->GetFrameData().Length() + DEFAULT_HEADER_SIZE);
  ebml.buf = mClusterBuffs.LastElement().Elements();

  if (aFrame->GetFrameType() == EncodedFrame::FrameType::VP8_I_FRAME) {
    EbmlLoc ebmlLoc;
    Ebml_StartSubElement(&ebml, &ebmlLoc, Cluster);
    mClusterHeaderIndex = mClusterBuffs.Length() - 1; // current cluster header array index
    mClusterLengthLoc = ebmlLoc.offset;
    if (aFrame->GetFrameType() != EncodedFrame::FrameType::VORBIS_AUDIO_FRAME) {
      mClusterTimecode = aFrame->GetTimeStamp() / PR_USEC_PER_MSEC;
    }
    Ebml_SerializeUnsigned(&ebml, Timecode, mClusterTimecode);
  }

  if (aFrame->GetFrameType() != EncodedFrame::FrameType::VORBIS_AUDIO_FRAME) {
    short timeCode = aFrame->GetTimeStamp() / PR_USEC_PER_MSEC - mClusterTimecode;
    writeSimpleBlock(&ebml, 0x1, timeCode, aFrame->GetFrameType() ==
                     EncodedFrame::FrameType::VP8_I_FRAME,
                     0, 0, (unsigned char*)aFrame->GetFrameData().Elements(),
                     aFrame->GetFrameData().Length());
  } else {
    writeSimpleBlock(&ebml, 0x2, 0, false,
                     0, 0, (unsigned char*)aFrame->GetFrameData().Elements(),
                     aFrame->GetFrameData().Length());
  }
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE + aFrame->GetFrameData().Length(),
             "write more data > EBML_BUFFER_SIZE");
  mClusterBuffs.LastElement().SetLength(ebml.offset);
}

void
EbmlComposer::SetVideoConfig(uint32_t aWidth, uint32_t aHeight,
                             uint32_t aDisplayWidth, uint32_t aDisplayHeight,
                             float aFrameRate)
{
  MOZ_ASSERT(aWidth > 0, "Width should > 0");
  MOZ_ASSERT(aHeight > 0, "Height should > 0");
  MOZ_ASSERT(aDisplayWidth > 0, "DisplayWidth should > 0");
  MOZ_ASSERT(aDisplayHeight > 0, "DisplayHeight should > 0");
  MOZ_ASSERT(aFrameRate > 0, "FrameRate should > 0");
  mWidth = aWidth;
  mHeight = aHeight;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;
  mFrameRate = aFrameRate;
}

void
EbmlComposer::SetAudioConfig(uint32_t aSampleFreq, uint32_t aChannels,
                             uint32_t aBitDepth)
{
  MOZ_ASSERT(aSampleFreq > 0, "SampleFreq should > 0");
  MOZ_ASSERT(aBitDepth > 0, "BitDepth should > 0");
  MOZ_ASSERT(aChannels > 0, "Channels should > 0");
  mSampleFreq = aSampleFreq;
  mBitDepth = aBitDepth;
  mChannels = aChannels;
}

void
EbmlComposer::ExtractBuffer(nsTArray<nsTArray<uint8_t> >* aDestBufs,
                            uint32_t aFlag)
{
  if ((aFlag & ContainerWriter::FLUSH_NEEDED) && mClusterHeaderIndex > 0) {
    FinishCluster();
  }
  // aDestBufs may have some element
  for (uint32_t i = 0; i < mClusterCanFlushBuffs.Length(); i ++ ) {
    aDestBufs->AppendElement()->SwapElements(mClusterCanFlushBuffs[i]);
  }
  mClusterCanFlushBuffs.Clear();
}

EbmlComposer::EbmlComposer()
  : mClusterHeaderIndex(0)
  , mClusterLengthLoc(0)
  , mClusterTimecode(0)
  , mWidth(0)
  , mHeight(0)
  , mFrameRate(0)
  , mSampleFreq(0)
  , mBitDepth(0)
  , mChannels(0)
{}

}
