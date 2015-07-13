/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOMediaWriter_h_
#define ISOMediaWriter_h_

#include "ContainerWriter.h"
#include "nsIRunnable.h"

namespace mozilla {

class ISOControl;
class FragmentBuffer;

class ISOMediaWriter : public ContainerWriter
{
public:
  // Generate an fragmented MP4 stream, ISO/IEC 14496-12.
  // Brand names in 'ftyp' box are 'isom' and 'mp42'.
  const static uint32_t TYPE_FRAG_MP4 = 1 << 0;

  // Generate an fragmented 3GP stream, 3GPP TS 26.244,
  // '5.4.3 Basic profile'.
  // Brand names in 'ftyp' box are '3gp9' and 'isom'.
  const static uint32_t TYPE_FRAG_3GP = 1 << 1;

  // aType is the combination of CREATE_AUDIO_TRACK and CREATE_VIDEO_TRACK.
  // It is a hint to muxer that the output streaming contains audio, video
  // or both.
  //
  // aHint is one of the value in TYPE_XXXXXXXX. It is a hint to muxer what kind
  // of ISO format should be generated.
  ISOMediaWriter(uint32_t aType, uint32_t aHint = TYPE_FRAG_MP4);
  ~ISOMediaWriter();

  // ContainerWriter methods
  nsresult WriteEncodedTrack(const EncodedFrameContainer &aData,
                             uint32_t aFlags = 0) override;

  nsresult GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                            uint32_t aFlags = 0) override;

  nsresult SetMetadata(TrackMetadataBase* aMetadata) override;

protected:
  /**
   * The state of each state will generate one or more blob.
   * Each blob will be a moov, moof, moof... until receiving EOS.
   * The generated sequence is:
   *
   *   moov -> moof -> moof -> ... -> moof -> moof
   *
   * Following is the details of each state.
   *   MUXING_HEAD:
   *     It collects the metadata to generate a moov. The state transits to
   *     MUXING_HEAD after output moov blob.
   *
   *   MUXING_FRAG:
   *     It collects enough audio/video data to generate a fragment blob. This
   *     will be repeated until END_OF_STREAM and then transiting to MUXING_DONE.
   *
   *   MUXING_DONE:
   *     End of ISOMediaWriter life cycle.
   */
  enum MuxState {
    MUXING_HEAD,
    MUXING_FRAG,
    MUXING_DONE,
  };

private:
  nsresult RunState();

  // True if one of following conditions hold:
  //   1. Audio/Video accumulates enough data to generate a moof.
  //   2. Get EOS signal.
  //   aEOS will be assigned to true if it gets EOS signal.
  bool ReadyToRunState(bool& aEOS);

  // The main class to generate and iso box. Its life time is same as
  // ISOMediaWriter and deleted only if ISOMediaWriter is destroyed.
  nsAutoPtr<ISOControl> mControl;

  // Buffers to keep audio/video data frames, they are created when metadata is
  // received. Only one instance for each media type is allowed and they will be
  // deleted only if ISOMediaWriter is destroyed.
  nsAutoPtr<FragmentBuffer> mAudioFragmentBuffer;
  nsAutoPtr<FragmentBuffer> mVideoFragmentBuffer;

  MuxState mState;

  // A flag to indicate the output buffer is ready to blob out.
  bool mBlobReady;

  // Combination of Audio_Track or Video_Track.
  uint32_t mType;
};

} // namespace mozilla

#endif // ISOMediaWriter_h_
