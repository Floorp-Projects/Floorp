/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OpusDecoder_h_)
#define OpusDecoder_h_

#include "OpusParser.h"
#include "PlatformDecoderModule.h"

#include "nsAutoPtr.h"

namespace mozilla {

class OpusDataDecoder : public MediaDataDecoder
{
public:
  OpusDataDecoder(const AudioInfo& aConfig,
                  FlushableTaskQueue* aTaskQueue,
                  MediaDataDecoderCallback* aCallback);
  ~OpusDataDecoder();

  nsresult Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;

  // Return true if mimetype is Opus
  static bool IsOpus(const nsACString& aMimeType);

private:
  nsresult DecodeHeader(const unsigned char* aData, size_t aLength);

  void Decode (MediaRawData* aSample);
  int DoDecode (MediaRawData* aSample);
  void DoDrain ();

  const AudioInfo& mInfo;
  RefPtr<FlushableTaskQueue> mTaskQueue;
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
};

} // namespace mozilla
#endif
