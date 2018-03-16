/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZInputBridgeChild.h"

namespace mozilla {
namespace layers {

APZInputBridgeChild::APZInputBridgeChild()
  : mDestroyed(false)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

APZInputBridgeChild::~APZInputBridgeChild()
{
}

void
APZInputBridgeChild::Destroy()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mDestroyed) {
    return;
  }

  Send__delete__(this);
  mDestroyed = true;
}

void
APZInputBridgeChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mDestroyed = true;
}

} // namespace layers
} // namespace mozilla
