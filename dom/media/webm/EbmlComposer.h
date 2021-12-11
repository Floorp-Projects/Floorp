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
  EbmlComposer() = default;
  /*
   * Assign the parameters which header requires. These can be called multiple
   * times to change paramter values until GenerateHeader() is called, when this
   * becomes illegal to call again.
   */
  void SetVideoConfig(uint32_t aWidth, uint32_t aHeight, uint32_t aDisplayWidth,
                      uint32_t aDisplayHeight);
  void SetAudioConfig(uint32_t aSampleFreq, uint32_t aChannels);
  /*
   * Set the CodecPrivateData for writing in header.
   */
  void SetAudioCodecPrivateData(nsTArray<uint8_t>& aBufs) {
    mCodecPrivateData.AppendElements(aBufs);
  }
  /*
   * Generate the whole WebM header with the configured tracks, and make
   * available to ExtractBuffer. Must only be called once.
   */
  void GenerateHeader();
  /*
   * Insert media encoded buffer into muxer and it would be package
   * into SimpleBlock. If no cluster is opened, new cluster will start for
   * writing. Frames passed to this function should already have any codec delay
   * applied.
   */
  nsresult WriteSimpleBlock(EncodedFrame* aFrame);
  /*
   * Get valid cluster data.
   */
  void ExtractBuffer(nsTArray<nsTArray<uint8_t>>* aDestBufs,
                     uint32_t aFlag = 0);

 private:
  // True once we have written the first cluster header. We cannot serialize any
  // P-frames until this is true, since we start a new cluster every I-frame.
  bool mHasWrittenCluster = false;
  // The timecode of the cluster.
  uint64_t mCurrentClusterTimecode = 0;

  // Written data to be flushed out by ExtractBuffer().
  nsTArray<nsTArray<uint8_t>> mBuffer;

  // True when Metadata has been serialized into mBuffer.
  bool mMetadataFinished = false;

  // Video configuration
  int mWidth = 0;
  int mHeight = 0;
  int mDisplayWidth = 0;
  int mDisplayHeight = 0;
  bool mHasVideo = false;

  // Audio configuration
  float mSampleFreq = 0;
  int mChannels = 0;
  bool mHasAudio = false;
  // Audio codec specific header data.
  nsTArray<uint8_t> mCodecPrivateData;
};

}  // namespace mozilla

#endif
