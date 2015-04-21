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

class GMPContentParent;

class GMPAudioDecoderParent final : public GMPAudioDecoderProxy
                                  , public PGMPAudioDecoderParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPAudioDecoderParent)

  explicit GMPAudioDecoderParent(GMPContentParent *aPlugin);

  nsresult Shutdown();

  // GMPAudioDecoderProxy
  virtual nsresult InitDecode(GMPAudioCodecType aCodecType,
                              uint32_t aChannelCount,
                              uint32_t aBitsPerChannel,
                              uint32_t aSamplesPerSecond,
                              nsTArray<uint8_t>& aExtraData,
                              GMPAudioDecoderCallbackProxy* aCallback) override;
  virtual nsresult Decode(GMPAudioSamplesImpl& aInput) override;
  virtual nsresult Reset() override;
  virtual nsresult Drain() override;
  virtual nsresult Close() override;

private:
  ~GMPAudioDecoderParent();

  // PGMPAudioDecoderParent
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool RecvDecoded(const GMPAudioDecodedSampleData& aDecoded) override;
  virtual bool RecvInputDataExhausted() override;
  virtual bool RecvDrainComplete() override;
  virtual bool RecvResetComplete() override;
  virtual bool RecvError(const GMPErr& aError) override;
  virtual bool RecvShutdown() override;
  virtual bool Recv__delete__() override;

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  nsRefPtr<GMPContentParent> mPlugin;
  GMPAudioDecoderCallbackProxy* mCallback;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioDecoderParent_h_
