/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_client_CompositableChild_h
#define mozilla_gfx_layers_client_CompositableChild_h

#include <stdint.h>
#include "IPDLActor.h"
#include "mozilla/layers/PCompositableChild.h"

namespace mozilla {
namespace layers {

class CompositableClient;

/**
 * IPDL actor used by CompositableClient to match with its corresponding
 * CompositableHost on the compositor side.
 *
 * CompositableChild is owned by a CompositableClient.
 */
class CompositableChild : public ChildActor<PCompositableChild>
{
public:
  CompositableChild();
  virtual ~CompositableChild();

  void Init(CompositableClient* aCompositable, uint64_t aAsyncID);
  void RevokeCompositableClient();

  void ActorDestroy(ActorDestroyReason) override;

  CompositableClient* GetCompositableClient() {
    return mCompositableClient;
  }
  uint64_t GetAsyncID() const {
    return mAsyncID;
  }

private:
  CompositableClient* mCompositableClient;
  uint64_t mAsyncID;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_client_CompositableChild_h
