/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OpusDecoder_h_)
#define OpusDecoder_h_

#include "OpusParser.h"
#include "PlatformDecoderModule.h"

#include "mozilla/Maybe.h"
#include "nsAutoPtr.h"

namespace mozilla {

class OpusDataDecoder : public MediaDataDecoder
{
public:
  OpusDataDecoder(const AudioInfo& aConfig,
                  TaskQueue* aTaskQueue,
                  MediaDataDecoderCallback* aCallback);
  ~OpusDataDecoder();

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "opus audio decoder";
  }

  // Return true if mimetype is Opus
  static bool IsOpus(const nsACString& aMimeType);

private:
  enum DecodeError {
    DECODE_SUCCESS,
    DECODE_ERROR,
    FATAL_ERROR
  };

  nsresult DecodeHeader(const unsigned char* aData, size_t aLength);

  void ProcessDecode(MediaRawData* aSample);
  DecodeError DoDecode(MediaRawData* aSample);
  void ProcessDrain();

  const AudioInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;

  // Opus decoder state
  nsAutoPtr<OpusParser> mOpusParser;
  OpusMSDecoder* mOpusDecoder;

  uint16_t mSkip;        // Samples left to trim before playback.
  bool mDecodedHeader;

  // Opus padding should only be discarded on the final packet.  Once this
  // is set to true, if the reader attempts to decode any further packets it
  // will raise an error so we can indicate that the file is invalid.
  bool mPaddingDiscarded;
  int64_t mFrames;
  Maybe<int64_t> mLastFrameTime;
  uint8_t mMappingTable[MAX_AUDIO_CHANNELS]; // Channel mapping table.

  Atomic<bool> mIsFlushing;
};

} // namespace mozilla
#endif
