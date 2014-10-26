/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPAudioDecoderParent_h_
#define GMPAudioDecoderParent_h_

#include "mozilla/RefPtr.h"
#include "gmp-audio-decode.h"
#include "gmp-audio-codec.h"
#include "mozilla/gmp/PGMPAudioDecoderParent.h"
#include "GMPMessageUtils.h"
#include "GMPAudioDecoderProxy.h"

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPAudioDecoderParent MOZ_FINAL : public GMPAudioDecoderProxy
                                      , public PGMPAudioDecoderParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPAudioDecoderParent)

  explicit GMPAudioDecoderParent(GMPParent *aPlugin);

  nsresult Shutdown();

  // GMPAudioDecoderProxy
  virtual nsresult InitDecode(GMPAudioCodecType aCodecType,
                              uint32_t aChannelCount,
                              uint32_t aBitsPerChannel,
                              uint32_t aSamplesPerSecond,
                              nsTArray<uint8_t>& aExtraData,
                              GMPAudioDecoderProxyCallback* aCallback) MOZ_OVERRIDE;
  virtual nsresult Decode(GMPAudioSamplesImpl& aInput) MOZ_OVERRIDE;
  virtual nsresult Reset() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Close() MOZ_OVERRIDE;

private:
  ~GMPAudioDecoderParent();

  // PGMPAudioDecoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool RecvDecoded(const GMPAudioDecodedSampleData& aDecoded) MOZ_OVERRIDE;
  virtual bool RecvInputDataExhausted() MOZ_OVERRIDE;
  virtual bool RecvDrainComplete() MOZ_OVERRIDE;
  virtual bool RecvResetComplete() MOZ_OVERRIDE;
  virtual bool RecvError(const GMPErr& aError) MOZ_OVERRIDE;
  virtual bool Recv__delete__() MOZ_OVERRIDE;

  bool mIsOpen;
  bool mShuttingDown;
  nsRefPtr<GMPParent> mPlugin;
  GMPAudioDecoderProxyCallback* mCallback;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioDecoderParent_h_
