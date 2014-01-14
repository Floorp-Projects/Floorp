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
class AACTrackMetadata;
class AVCTrackMetadata;
class ISOMediaWriterRunnable;

class ISOMediaWriter : public ContainerWriter
{
public:
  nsresult WriteEncodedTrack(const EncodedFrameContainer &aData,
                             uint32_t aFlags = 0) MOZ_OVERRIDE;

  nsresult GetContainerData(nsTArray<nsTArray<uint8_t>>* aOutputBufs,
                            uint32_t aFlags = 0) MOZ_OVERRIDE;

  nsresult SetMetadata(TrackMetadataBase* aMetadata) MOZ_OVERRIDE;

  ISOMediaWriter(uint32_t aType);
  ~ISOMediaWriter();

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
