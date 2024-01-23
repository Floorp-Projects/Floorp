/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITABLEFORWARDER
#define MOZILLA_LAYERS_COMPOSITABLEFORWARDER

#include <stdint.h>  // for int32_t, uint32_t, uint64_t
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT, MOZ_ASSERT_HELPER1
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/ipc/ProtocolUtils.h"  // for IToplevelProtocol, ProtocolID
#include "mozilla/layers/KnowsCompositor.h"  // for KnowsCompositor
#include "nsRect.h"                          // for nsIntRect
#include "nsRegion.h"                        // for nsIntRegion
#include "nsTArray.h"                        // for nsTArray

namespace mozilla {
namespace layers {
class CompositableClient;
class CompositableHandle;
class ImageContainer;
class PTextureChild;
class SurfaceDescriptorTiles;
class TextureClient;

/**
 * FwdTransactionCounter issues forwarder transaction numbers that represent a
 * sequential stream of transactions to be transported over IPDL. Since every
 * top-level protocol represents its own independently-sequenced IPDL queue,
 * transaction numbers most naturally align with top-level protocols, rather
 * than have different sub-protocols with their own independent transaction
 * numbers that can't be usefully sequenced. FwdTransactionCounter expects
 * users of it to provide themselves as proof they are a top-level protocol
 * to avoid issues.
 */
struct FwdTransactionCounter {
  explicit FwdTransactionCounter(mozilla::ipc::IToplevelProtocol* aToplevel)
      : mFwdTransactionType(aToplevel->GetProtocolId()) {}

  /**
   * The ID of the top-level protocol. This is useful to tag the source of
   * the transaction numbers in case different sources must be disambiguated.
   */
  mozilla::ipc::ProtocolId mFwdTransactionType;

  /**
   * Transaction id of ShadowLayerForwarder.
   * It is incremented by UpdateFwdTransactionId() in each BeginTransaction()
   * call.
   */
  uint64_t mFwdTransactionId = 0;
};

/**
 * A transaction is a set of changes that happenned on the content side, that
 * should be sent to the compositor side.
 * CompositableForwarder is an interface to manage a transaction of
 * compositable objetcs.
 *
 * ShadowLayerForwarder is an example of a CompositableForwarder (that can
 * additionally forward modifications of the Layer tree).
 * ImageBridgeChild is another CompositableForwarder.
 *
 * CompositableForwarder implements KnowsCompositor for simplicity as all
 * implementations of CompositableForwarder currently also implement
 * KnowsCompositor. This dependency could be split if we add new use cases.
 */
class CompositableForwarder : public KnowsCompositor {
 public:
  CompositableForwarder();
  ~CompositableForwarder();

  /**
   * Setup the IPDL actor for aCompositable to be part of layers
   * transactions.
   */
  virtual void Connect(CompositableClient* aCompositable,
                       ImageContainer* aImageContainer = nullptr) = 0;

  virtual void ReleaseCompositable(const CompositableHandle& aHandle) = 0;
  virtual bool DestroyInTransaction(PTextureChild* aTexture) = 0;

  /**
   * Tell the CompositableHost on the compositor side to remove the texture
   * from the CompositableHost.
   * This function does not delete the TextureHost corresponding to the
   * TextureClient passed in parameter.
   * When the TextureClient has TEXTURE_DEALLOCATE_CLIENT flag,
   * the transaction becomes synchronous.
   */
  virtual void RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                             TextureClient* aTexture) = 0;

  struct TimedTextureClient {
    TimedTextureClient()
        : mTextureClient(nullptr), mFrameID(0), mProducerID(0) {}

    TextureClient* mTextureClient;
    TimeStamp mTimeStamp;
    nsIntRect mPictureRect;
    int32_t mFrameID;
    int32_t mProducerID;
  };
  /**
   * Tell the CompositableHost on the compositor side what textures to use for
   * the next composition.
   */
  virtual void UseTextures(CompositableClient* aCompositable,
                           const nsTArray<TimedTextureClient>& aTextures) = 0;

  virtual void UseRemoteTexture(CompositableClient* aCompositable,
                                const RemoteTextureId aTextureId,
                                const RemoteTextureOwnerId aOwnerId,
                                const gfx::IntSize aSize,
                                const TextureFlags aFlags) = 0;

  void UpdateFwdTransactionId() {
    ++GetFwdTransactionCounter().mFwdTransactionId;
  }
  uint64_t GetFwdTransactionId() {
    return GetFwdTransactionCounter().mFwdTransactionId;
  }
  mozilla::ipc::ProtocolId GetFwdTransactionType() {
    return GetFwdTransactionCounter().mFwdTransactionType;
  }

  virtual bool InForwarderThread() = 0;

  void AssertInForwarderThread() { MOZ_ASSERT(InForwarderThread()); }

 protected:
  virtual FwdTransactionCounter& GetFwdTransactionCounter() = 0;

  nsTArray<RefPtr<TextureClient>> mTexturesToRemove;
  nsTArray<RefPtr<CompositableClient>> mCompositableClientsToRemove;
};

}  // namespace layers
}  // namespace mozilla

#endif
