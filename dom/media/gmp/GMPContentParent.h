/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPContentParent_h_
#define GMPContentParent_h_

#include "mozilla/gmp/PGMPContentParent.h"
#include "GMPSharedMemManager.h"
#include "GMPNativeTypes.h"
#include "nsISupportsImpl.h"

namespace mozilla::gmp {

class GMPContentParentCloseBlocker;
class GMPParent;
class GMPVideoDecoderParent;
class GMPVideoEncoderParent;
class ChromiumCDMParent;

/**
 * This class allows the parent/content processes to create GMP decoder/encoder
 * objects in the GMP process.
 */
class GMPContentParent final : public PGMPContentParent, public GMPSharedMem {
  friend class PGMPContentParent;

 public:
  // Mark AddRef and Release as `final`, as they overload pure virtual
  // implementations in PGMPContentParent.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPContentParent, final)

  explicit GMPContentParent(GMPParent* aParent = nullptr);

  nsresult GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD);
  void VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder);

  nsresult GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE);
  void VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder);

  already_AddRefed<ChromiumCDMParent> GetChromiumCDM(
      const nsCString& aKeySystem);
  void ChromiumCDMDestroyed(ChromiumCDMParent* aCDM);

  nsCOMPtr<nsISerialEventTarget> GMPEventTarget();

  // GMPSharedMem
  void CheckThread() override;

  void SetDisplayName(const nsCString& aDisplayName) {
    mDisplayName = aDisplayName;
  }
  const nsCString& GetDisplayName() const { return mDisplayName; }
  void SetPluginId(const uint32_t aPluginId) { mPluginId = aPluginId; }
  uint32_t GetPluginId() const { return mPluginId; }
  void SetPluginType(GMPPluginType aPluginType) { mPluginType = aPluginType; }
  GMPPluginType GetPluginType() const { return mPluginType; }

 private:
  friend class GMPContentParentCloseBlocker;

  void AddCloseBlocker();
  void RemoveCloseBlocker();

  ~GMPContentParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

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
  GMPPluginType mPluginType = GMPPluginType::Unknown;
  uint32_t mCloseBlockerCount = 0;
};

class GMPContentParentCloseBlocker final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GMPContentParentCloseBlocker)

  explicit GMPContentParentCloseBlocker(GMPContentParent* aParent)
      : mParent(aParent), mEventTarget(aParent->GMPEventTarget()) {
    MOZ_ASSERT(mEventTarget);
    mParent->AddCloseBlocker();
  }

  void Destroy();

  RefPtr<GMPContentParent> mParent;

 private:
  nsCOMPtr<nsISerialEventTarget> mEventTarget;

  ~GMPContentParentCloseBlocker() { Destroy(); }
};

}  // namespace mozilla::gmp

#endif  // GMPParent_h_
