/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableChild.h"
#include "CompositableClient.h"

namespace mozilla {
namespace layers {

CompositableChild::CompositableChild()
 : mCompositableClient(nullptr),
   mAsyncID(0)
{
  MOZ_COUNT_CTOR(CompositableChild);
}

CompositableChild::~CompositableChild()
{
  MOZ_COUNT_DTOR(CompositableChild);
}

void
CompositableChild::Init(CompositableClient* aCompositable, uint64_t aAsyncID)
{
  mCompositableClient = aCompositable;
  mAsyncID = aAsyncID;
}

void
CompositableChild::RevokeCompositableClient()
{
  mCompositableClient = nullptr;
}

void
CompositableChild::ActorDestroy(ActorDestroyReason)
{
  if (mCompositableClient) {
    mCompositableClient->mCompositableChild = nullptr;
  }
}

} // namespace layers
} // namespace mozilla
