/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_client_CompositableChild_h
#define mozilla_gfx_layers_client_CompositableChild_h

#include <stdint.h>
#include "IPDLActor.h"
#include "mozilla/Mutex.h"
#include "mozilla/layers/PCompositableChild.h"

namespace mozilla {
namespace layers {

class CompositableClient;
class AsyncCompositableChild;

/**
 * IPDL actor used by CompositableClient to match with its corresponding
 * CompositableHost on the compositor side.
 *
 * CompositableChild is owned by a CompositableClient.
 */
class CompositableChild : public PCompositableChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositableChild)

  static PCompositableChild* CreateActor();
  static void DestroyActor(PCompositableChild* aChild);

  void Init(CompositableClient* aCompositable);
  virtual void RevokeCompositableClient();

  virtual void ActorDestroy(ActorDestroyReason) override;

  virtual RefPtr<CompositableClient> GetCompositableClient();

  virtual AsyncCompositableChild* AsAsyncCompositableChild() {
    return nullptr;
  }

  // These should only be called on the IPDL thread.
  bool IsConnected() const;
  bool CanSend() const {
    return mCanSend;
  }

protected:
  CompositableChild();
  virtual ~CompositableChild();

protected:
  CompositableClient* mCompositableClient;
  bool mCanSend;
};

// This CompositableChild can be used off the main thread.
class AsyncCompositableChild final : public CompositableChild
{
public:
  static PCompositableChild* CreateActor(uint64_t aAsyncID);

  void RevokeCompositableClient() override;
  RefPtr<CompositableClient> GetCompositableClient() override;

  void ActorDestroy(ActorDestroyReason) override;

  AsyncCompositableChild* AsAsyncCompositableChild() override {
    return this;
  }

  uint64_t GetAsyncID() const {
    return mAsyncID;
  }

protected:
  explicit AsyncCompositableChild(uint64_t aAsyncID);
  ~AsyncCompositableChild() override;

private:
  Mutex mLock;
  uint64_t mAsyncID;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_client_CompositableChild_h
