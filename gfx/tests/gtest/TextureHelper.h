/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <vector>

#include "gfxPlatform.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/RefPtr.h"

using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace layers {

/**
 * Create a YCbCrTextureClient according to the given backend.
 */
static already_AddRefed<TextureClient>
CreateYCbCrTextureClientWithBackend(LayersBackend aLayersBackend)
{
  // TODO: Create TextureClient.
  return nullptr;
}

/**
 * Create a TextureClient according to the given backend.
 */
static already_AddRefed<TextureClient>
CreateTextureClientWithBackend(LayersBackend aLayersBackend)
{
  // TODO: Create TextureClient.
  return nullptr;
}

/**
 * Create a TextureHost according to the given TextureClient.
 */
already_AddRefed<TextureHost>
CreateTextureHostWithBackend(TextureClient* aClient, LayersBackend& aLayersBackend)
{
  if (!aClient) {
    return nullptr;
  }

  // client serialization
  SurfaceDescriptor descriptor;
  RefPtr<TextureHost> textureHost;

  aClient->ToSurfaceDescriptor(descriptor);

  return TextureHost::Create(descriptor, nullptr, aLayersBackend,
                             aClient->GetFlags());
}

} // namespace layers
} // namespace mozilla
