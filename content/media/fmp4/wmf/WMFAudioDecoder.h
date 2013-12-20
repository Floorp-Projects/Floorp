/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WMFAudioDecoder_h_)
#define WMFAudioDecoder_h_

#include "WMF.h"
#include "MP4Reader.h"
#include "MFTDecoder.h"

namespace mozilla {

class WMFAudioDecoder : public MediaDataDecoder {
public:
  WMFAudioDecoder();

  nsresult Init(uint32_t aChannelCount,
                uint32_t aSampleRate,
                uint16_t aBitsPerSample,
                const uint8_t* aUserData,
                uint32_t aUserDataLength);

  virtual nsresult Shutdown() MOZ_OVERRIDE;

  // Inserts data into the decoder's pipeline.
  virtual DecoderStatus Input(const uint8_t* aData,
                              uint32_t aLength,
                              Microseconds aDTS,
                              Microseconds aPTS,
                              int64_t aOffsetInStream);

  // Blocks until a decoded sample is produced by the decoder.
  virtual DecoderStatus Output(nsAutoPtr<MediaData>& aOutData);

  virtual DecoderStatus Flush() MOZ_OVERRIDE;

private:


  // A helper for Output() above. This has the same interface as Output()
  // above, except that it returns DECODE_STATUS_OK and sets aOutData to
  // nullptr when all the output samples have been stripped due to having
  // negative timestamps. WMF's AAC decoder sometimes output negatively
  // timestamped samples, presumably they're the preroll samples, and we
  // strip them.
  DecoderStatus OutputNonNegativeTimeSamples(nsAutoPtr<MediaData>& aOutData);

  nsAutoPtr<MFTDecoder> mDecoder;

  uint32_t mAudioChannels;
  uint32_t mAudioBytesPerSample;
  uint32_t mAudioRate;

  // The last offset into the media resource that was passed into Input().
  // This is used to approximate the decoder's position in the media resource.
  int64_t mLastStreamOffset;

  // The offset, in audio frames, at which playback started since the
  // last discontinuity.
  int64_t mAudioFrameOffset;
  // The number of audio frames that we've played since the last
  // discontinuity.
  int64_t mAudioFrameSum;
  // True if we need to re-initialize mAudioFrameOffset and mAudioFrameSum
  // from the next audio packet we decode. This happens after a seek, since
  // WMF doesn't mark a stream as having a discontinuity after a seek(0).
  bool mMustRecaptureAudioPosition;
};



} // namespace mozilla

#endif
