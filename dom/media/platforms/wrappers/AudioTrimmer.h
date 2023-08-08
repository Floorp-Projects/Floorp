/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(AudioTrimmer_h_)
#  define AudioTrimmer_h_

#  include "PlatformDecoderModule.h"
#  include "mozilla/Mutex.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(AudioTrimmer, MediaDataDecoder);

class AudioTrimmer final : public MediaDataDecoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioTrimmer, final);

  AudioTrimmer(already_AddRefed<MediaDataDecoder> aDecoder,
               const CreateDecoderParams& aParams)
      : mDecoder(aDecoder) {}

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  bool CanDecodeBatch() const override { return mDecoder->CanDecodeBatch(); }
  RefPtr<DecodePromise> DecodeBatch(
      nsTArray<RefPtr<MediaRawData>>&& aSamples) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;
  nsCString GetProcessName() const override;
  nsCString GetCodecName() const override;
  bool IsHardwareAccelerated(nsACString& aFailureReason) const override;
  void SetSeekThreshold(const media::TimeUnit& aTime) override;
  bool SupportDecoderRecycling() const override;
  ConversionRequired NeedsConversion() const override;

 private:
  ~AudioTrimmer() = default;

  // Apply trimming information on decoded data.
  RefPtr<DecodePromise> HandleDecodedResult(
      DecodePromise::ResolveOrRejectValue&& aValue);
  void PrepareTrimmers(MediaRawData* aRaw);
  const RefPtr<MediaDataDecoder> mDecoder;
  nsCOMPtr<nsISerialEventTarget> mThread;
  AutoTArray<Maybe<media::TimeInterval>, 2> mTrimmers;
};

}  // namespace mozilla

#endif  // AudioTrimmer_h_
