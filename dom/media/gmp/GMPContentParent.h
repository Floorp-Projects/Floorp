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

class GMPParent;
class GMPVideoDecoderParent;
class GMPVideoEncoderParent;
class ChromiumCDMParent;

class GMPContentParent final : public PGMPContentParent, public GMPSharedMem {
  friend class PGMPContentParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPContentParent)

  explicit GMPContentParent(GMPParent* aParent = nullptr);

  nsresult GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD,
                              uint32_t aDecryptorId);
  void VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder);

  nsresult GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE);
  void VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder);

  already_AddRefed<ChromiumCDMParent> GetChromiumCDM();
  void ChromiumCDMDestroyed(ChromiumCDMParent* aCDM);

  nsCOMPtr<nsISerialEventTarget> GMPEventTarget();

  // GMPSharedMem
  void CheckThread() override;

  void SetDisplayName(const nsCString& aDisplayName) {
    mDisplayName = aDisplayName;
  }
  const nsCString& GetDisplayName() { return mDisplayName; }
  void SetPluginId(const uint32_t aPluginId) { mPluginId = aPluginId; }
  uint32_t GetPluginId() const { return mPluginId; }

  class CloseBlocker {
   public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CloseBlocker)

    explicit CloseBlocker(GMPContentParent* aParent) : mParent(aParent) {
      mParent->AddCloseBlocker();
    }
    RefPtr<GMPContentParent> mParent;

   private:
    ~CloseBlocker() { mParent->RemoveCloseBlocker(); }
  };

 private:
  void AddCloseBlocker();
  void RemoveCloseBlocker();

  ~GMPContentParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  PGMPVideoDecoderParent* AllocPGMPVideoDecoderParent(
      const uint32_t& aDecryptorId) override;
  bool DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor) override;

  PGMPVideoEncoderParent* AllocPGMPVideoEncoderParent() override;
  bool DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor) override;

  void CloseIfUnused();
  // Needed because NewRunnableMethod tried to use the class that the method
  // lives on to store the receiver, but PGMPContentParent isn't refcounted.
  void Close() { PGMPContentParent::Close(); }

  nsTArray<RefPtr<GMPVideoDecoderParent>> mVideoDecoders;
  nsTArray<RefPtr<GMPVideoEncoderParent>> mVideoEncoders;
  nsTArray<RefPtr<ChromiumCDMParent>> mChromiumCDMs;
  nsCOMPtr<nsISerialEventTarget> mGMPEventTarget;
  RefPtr<GMPParent> mParent;
  nsCString mDisplayName;
  uint32_t mPluginId;
  uint32_t mCloseBlockerCount = 0;
};

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPParent_h_
