/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "gtest/gtest.h"
#include "MockWidget.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/RefPtr.h"
#include "TestLayers.h"
#include "TextureHelper.h"

using mozilla::gfx::Feature;
using mozilla::gfx::gfxConfig;
using mozilla::layers::Compositor;
using mozilla::layers::CompositorOptions;
using mozilla::layers::ISurfaceAllocator;
using mozilla::layers::LayersBackend;
using mozilla::layers::TestSurfaceAllocator;
using mozilla::layers::TextureClient;
using mozilla::layers::TextureHost;
using mozilla::widget::CompositorWidget;
using mozilla::widget::InProcessCompositorWidget;

/**
 * This function will create the possible TextureClient and TextureHost pairs
 * according to the given backend.
 */
static void CreateTextureWithBackend(
    LayersBackend& aLayersBackend, ISurfaceAllocator* aDeallocator,
    nsTArray<RefPtr<TextureClient>>& aTextureClients,
    nsTArray<RefPtr<TextureHost>>& aTextureHosts) {
  aTextureClients.AppendElement(CreateTextureClientWithBackend(aLayersBackend));

  aTextureClients.AppendElement(
      CreateYCbCrTextureClientWithBackend(aLayersBackend));

  for (uint32_t i = 0; i < aTextureClients.Length(); i++) {
    aTextureHosts.AppendElement(CreateTextureHostWithBackend(
        aTextureClients[i], aDeallocator, aLayersBackend));
  }
}

/**
 * This will return the default list of backends that units test should run
 * against.
 */
static void GetPlatformBackends(nsTArray<LayersBackend>& aBackends) {
  gfxPlatform* platform = gfxPlatform::GetPlatform();
  MOZ_ASSERT(platform);

  platform->GetCompositorBackends(gfxConfig::IsEnabled(Feature::HW_COMPOSITING),
                                  aBackends);
}
TEST(Gfx, TestTextureCompatibility)
{
  nsTArray<LayersBackend> backendHints;
  RefPtr<TestSurfaceAllocator> deallocator = new TestSurfaceAllocator();

  GetPlatformBackends(backendHints);
  for (uint32_t i = 0; i < backendHints.Length(); i++) {
    nsTArray<RefPtr<TextureClient>> textureClients;
    nsTArray<RefPtr<TextureHost>> textureHosts;

    CreateTextureWithBackend(backendHints[i], deallocator, textureClients,
                             textureHosts);
  }
}
