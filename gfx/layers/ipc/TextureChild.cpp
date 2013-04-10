/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureChild.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {

CompositableClient*
TextureChild::GetCompositableClient()
{
  return static_cast<CompositableChild*>(Manager())->GetCompositableClient();
}

void
TextureChild::Destroy()
{
  if (mTextureClient) {
  	mTextureClient->SetIPDLActor(nullptr);
  	mTextureClient = nullptr;
  }
  Send__delete__(this);
}

} // namespace
} // namespace