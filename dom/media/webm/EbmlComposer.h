/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EbmlComposer_h_
#define EbmlComposer_h_
#include "nsTArray.h"
#include "ContainerWriter.h"

namespace mozilla {

/*
 * A WebM muxer helper for package the valid WebM format.
 */
class EbmlComposer {
public:
  EbmlComposer();
  /*
   * Assign the parameter which header required.
   */
  void SetVideoConfig(uint32_t aWidth, uint32_t aHeight, uint32_t aDisplayWidth,
                      uint32_t aDisplayHeight, float aFrameRate);

  void SetAudioConfig(uint32_t aSampleFreq, uint32_t aChannels,
                      uint32_t bitDepth);
  /*
   * Set the CodecPrivateData for writing in header.
   */
  void SetAudioCodecPrivateData(nsTArray<uint8_t>& aBufs)
  {
    mCodecPrivateData.AppendElements(aBufs);
  }
  /*
   * Generate the whole WebM header and output to mBuff.
   */
  void GenerateHeader();
  /*
   * Insert media encoded buffer into muxer and it would be package
   * into SimpleBlock. If no cluster is opened, new cluster will start for writing.
   */
  void WriteSimpleBlock(EncodedFrame* aFrame);
  /*
   * Get valid cluster data.
   */
  void ExtractBuffer(nsTArray<nsTArray<uint8_t> >* aDestBufs,
                     uint32_t aFlag = 0);
private:
  // Move the metadata data to mClusterCanFlushBuffs.
  void FinishMetadata();
  // Close current cluster and move data to mClusterCanFlushBuffs.
  void FinishCluster();
  // The temporary storage for cluster data.
  nsTArray<nsTArray<uint8_t> > mClusterBuffs;
  // The storage which contain valid cluster data.
  nsTArray<nsTArray<uint8_t> > mClusterCanFlushBuffs;

  // Indicate the data types in mClusterBuffs.
  enum {
    FLUSH_NONE = 0,
    FLUSH_METADATA = 1 << 0,
    FLUSH_CLUSTER = 1 << 1
  };
  uint32_t mFlushState;
  // Indicate the cluster header index in mClusterBuffs.
  uint32_t mClusterHeaderIndex;
  // The cluster length position.
  uint64_t mClusterLengthLoc;
  // Audio codec specific header data.
  nsTArray<uint8_t> mCodecPrivateData;

  // The timecode of the cluster.
  uint64_t mClusterTimecode;

  // Video configuration
  int mWidth;
  int mHeight;
  int mDisplayWidth;
  int mDisplayHeight;
  float mFrameRate;
  // Audio configuration
  float mSampleFreq;
  int mBitDepth;
  int mChannels;
};

} // namespace mozilla

#endif
