/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentParent_h_
#define GMPContentParent_h_

#include "mozilla/gmp/PGMPContentParent.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace gmp {

class GMPAudioDecoderParent;
class GMPDecryptorParent;
class GMPParent;
class GMPVideoDecoderParent;
class GMPVideoEncoderParent;

class GMPContentParent final : public PGMPContentParent,
                               public GMPSharedMem
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPContentParent)

  explicit GMPContentParent(GMPParent* aParent = nullptr);

  nsresult GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD);
  void VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder);

  nsresult GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE);
  void VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder);

  nsresult GetGMPDecryptor(GMPDecryptorParent** aGMPKS);
  void DecryptorDestroyed(GMPDecryptorParent* aSession);

  nsresult GetGMPAudioDecoder(GMPAudioDecoderParent** aGMPAD);
  void AudioDecoderDestroyed(GMPAudioDecoderParent* aDecoder);

  nsIThread* GMPThread();

  // GMPSharedMem
  virtual void CheckThread() override;

  void SetDisplayName(const nsCString& aDisplayName)
  {
    mDisplayName = aDisplayName;
  }
  const nsCString& GetDisplayName()
  {
    return mDisplayName;
  }
  void SetPluginId(const uint32_t aPluginId)
  {
    mPluginId = aPluginId;
  }
  const uint32_t GetPluginId()
  {
    return mPluginId;
  }

private:
  ~GMPContentParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PGMPVideoDecoderParent* AllocPGMPVideoDecoderParent() override;
  virtual bool DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor) override;

  virtual PGMPVideoEncoderParent* AllocPGMPVideoEncoderParent() override;
  virtual bool DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor) override;

  virtual PGMPDecryptorParent* AllocPGMPDecryptorParent() override;
  virtual bool DeallocPGMPDecryptorParent(PGMPDecryptorParent* aActor) override;

  virtual PGMPAudioDecoderParent* AllocPGMPAudioDecoderParent() override;
  virtual bool DeallocPGMPAudioDecoderParent(PGMPAudioDecoderParent* aActor) override;

  void CloseIfUnused();
  // Needed because NS_NewRunnableMethod tried to use the class that the method
  // lives on to store the receiver, but PGMPContentParent isn't refcounted.
  void Close()
  {
    PGMPContentParent::Close();
  }

  nsTArray<nsRefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<nsRefPtr<GMPVideoEncoderParent>> mVideoEncoders;
  nsTArray<nsRefPtr<GMPDecryptorParent>> mDecryptors;
  nsTArray<nsRefPtr<GMPAudioDecoderParent>> mAudioDecoders;
  nsCOMPtr<nsIThread> mGMPThread;
  nsRefPtr<GMPParent> mParent;
  nsCString mDisplayName;
  uint32_t mPluginId;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_
