/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceHelpers.h"
#include "MacIOSurfaceImage.h"
#include "gfxPlatform.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/MacIOSurfaceTextureClientOGL.h"
#include "mozilla/UniquePtr.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

TextureClient*
MacIOSurfaceImage::GetTextureClient(KnowsCompositor* aForwarder)
{
  if (!mTextureClient) {
    BackendType backend = BackendType::NONE;
    mTextureClient = TextureClient::CreateWithData(
      MacIOSurfaceTextureData::Create(mSurface, backend),
      TextureFlags::DEFAULT,
      aForwarder->GetTextureForwarder()
    );
  }
  return mTextureClient;
}

already_AddRefed<SourceSurface>
MacIOSurfaceImage::GetAsSourceSurface()
{
  return CreateSourceSurfaceFromMacIOSurface(mSurface);
}
