/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineVideoDecoder_h_
#define WidevineVideoDecoder_h_

#include "stddef.h"
#include "content_decryption_module.h"
#include "gmp-api/gmp-video-decode.h"
#include "gmp-api/gmp-video-host.h"
#include "MediaData.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "WidevineDecryptor.h"
#include "WidevineVideoFrame.h"
#include <map>
#include <deque>

namespace mozilla {

class WidevineVideoDecoder : public GMPVideoDecoder {
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WidevineVideoDecoder)

  WidevineVideoDecoder(GMPVideoHost* aVideoHost,
                       RefPtr<CDMWrapper> aCDMWrapper);
  void InitDecode(const GMPVideoCodec& aCodecSettings,
                  const uint8_t* aCodecSpecific,
                  uint32_t aCodecSpecificLength,
                  GMPVideoDecoderCallback* aCallback,
                  int32_t aCoreCount) override;
  void Decode(GMPVideoEncodedFrame* aInputFrame,
              bool aMissingFrames,
              const uint8_t* aCodecSpecificInfo,
              uint32_t aCodecSpecificInfoLength,
              int64_t aRenderTimeMs = -1) override;
  void Reset() override;
  void Drain() override;
  void DecodingComplete() override;

private:

  ~WidevineVideoDecoder();

  cdm::ContentDecryptionModule_8* CDM() const {
    // CDM should only be accessed before 'DecodingComplete'.
    MOZ_ASSERT(mCDMWrapper);
    // CDMWrapper ensure the CDM is non-null, no need to check again.
    return mCDMWrapper->GetCDM();
  }

  bool ReturnOutput(WidevineVideoFrame& aFrame);
  void CompleteReset();

  GMPVideoHost* mVideoHost;
  RefPtr<CDMWrapper> mCDMWrapper;
  RefPtr<MediaByteBuffer> mExtraData;
  RefPtr<MediaByteBuffer> mAnnexB;
  GMPVideoDecoderCallback* mCallback;
  std::map<uint64_t, uint64_t> mFrameDurations;
  bool mSentInput;
  // Frames waiting on allocation
  std::deque<WidevineVideoFrame> mFrameAllocationQueue;
  // Number of calls of ReturnOutput currently in progress.
  int32_t mReturnOutputCallDepth;
  // If we're waiting to drain. Used to prevent drain completing while
  // ReturnOutput calls are still on the stack.
  bool mDrainPending;
  // If a reset is being performed. Used to track if ReturnOutput should
  // dump current frame.
  bool mResetInProgress;
};

} // namespace mozilla

#endif // WidevineVideoDecoder_h_
