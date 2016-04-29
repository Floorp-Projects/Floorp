/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentParent_h_
#define GMPContentParent_h_

#include "mozilla/gmp/PGMPContentParent.h"
#include "GMPSharedMemManager.h"
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
  void CheckThread() override;

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
  uint32_t GetPluginId() const
  {
    return mPluginId;
  }

private:
  ~GMPContentParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  PGMPVideoDecoderParent* AllocPGMPVideoDecoderParent() override;
  bool DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor) override;

  PGMPVideoEncoderParent* AllocPGMPVideoEncoderParent() override;
  bool DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor) override;

  PGMPDecryptorParent* AllocPGMPDecryptorParent() override;
  bool DeallocPGMPDecryptorParent(PGMPDecryptorParent* aActor) override;

  PGMPAudioDecoderParent* AllocPGMPAudioDecoderParent() override;
  bool DeallocPGMPAudioDecoderParent(PGMPAudioDecoderParent* aActor) override;

  void CloseIfUnused();
  // Needed because NS_NewRunnableMethod tried to use the class that the method
  // lives on to store the receiver, but PGMPContentParent isn't refcounted.
  void Close()
  {
    PGMPContentParent::Close();
  }

  nsTArray<RefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<RefPtr<GMPVideoEncoderParent>> mVideoEncoders;
  nsTArray<RefPtr<GMPDecryptorParent>> mDecryptors;
  nsTArray<RefPtr<GMPAudioDecoderParent>> mAudioDecoders;
  nsCOMPtr<nsIThread> mGMPThread;
  RefPtr<GMPParent> mParent;
  nsCString mDisplayName;
  uint32_t mPluginId;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPParent_h_
