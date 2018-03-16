/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeParent.h"

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/IAPZCTreeManager.h"

namespace mozilla {
namespace layers {

APZInputBridgeParent::APZInputBridgeParent(const uint64_t& aLayersId)
{
  MOZ_ASSERT(XRE_IsGPUProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mTreeManager = CompositorBridgeParent::GetAPZCTreeManager(aLayersId);
  MOZ_ASSERT(mTreeManager);
}

APZInputBridgeParent::~APZInputBridgeParent()
{
}

void
APZInputBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // We shouldn't need it after this
  mTreeManager = nullptr;
}

} // namespace layers
} // namespace mozilla
