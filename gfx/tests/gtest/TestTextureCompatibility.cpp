/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxConfig.h"
#include "gfxPlatform.h"
#include "gtest/gtest.h"
#include "MockWidget.h"
#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/RefPtr.h"
#include "TextureHelper.h"

using mozilla::gfx::Feature;
using mozilla::gfx::gfxConfig;
using mozilla::layers::BasicCompositor;
using mozilla::layers::Compositor;
using mozilla::layers::CompositorOptions;
using mozilla::layers::LayersBackend;
using mozilla::layers::TextureClient;
using mozilla::layers::TextureHost;
using mozilla::widget::CompositorWidget;
using mozilla::widget::InProcessCompositorWidget;

/**
 * This function will create the possible TextureClient and TextureHost pairs
 * according to the given backend.
 */
void
CreateTextureWithBackend(LayersBackend& aLayersBackend,
                         nsTArray<RefPtr<TextureClient>>& aTextureClients,
                         nsTArray<RefPtr<TextureHost>>& aTextureHosts)
{
  aTextureClients.AppendElement(CreateTextureClientWithBackend(aLayersBackend));

  aTextureClients.AppendElement(
    CreateYCbCrTextureClientWithBackend(aLayersBackend));

  for (uint32_t i = 0; i < aTextureClients.Length(); i++) {
    aTextureHosts.AppendElement(
      CreateTextureHostWithBackend(aTextureClients[i], aLayersBackend));
  }
}

/**
 * This will return the default list of backends that units test should run
 * against.
 */
static void
GetPlatformBackends(nsTArray<LayersBackend>& aBackends)
{
  gfxPlatform* platform = gfxPlatform::GetPlatform();
  MOZ_ASSERT(platform);

  platform->GetCompositorBackends(
    gfxConfig::IsEnabled(Feature::HW_COMPOSITING), aBackends);

  if (aBackends.IsEmpty()) {
    aBackends.AppendElement(LayersBackend::LAYERS_BASIC);
  }
}

/**
 * This function will return a BasicCompositor to caller.
 */
already_AddRefed<Compositor>
CreateBasicCompositor()
{
  RefPtr<Compositor> compositor;
  // Init the platform.
  if (gfxPlatform::GetPlatform()) {
    RefPtr<MockWidget> widget = new MockWidget(256, 256);
   CompositorOptions options;
    RefPtr<CompositorWidget> proxy
      = new InProcessCompositorWidget(options, widget);
    compositor = new BasicCompositor(nullptr, proxy);
  }
  return compositor.forget();
}

/**
 * This function checks if the textures react correctly when setting them to
 * BasicCompositor.
 */
void
CheckCompatibilityWithBasicCompositor(LayersBackend aBackends,
                                      nsTArray<RefPtr<TextureHost>>& aTextures)
{
  RefPtr<Compositor> compositor = CreateBasicCompositor();
  for (uint32_t i = 0; i < aTextures.Length(); i++) {
    if (!aTextures[i]) {
      continue;
    }
    aTextures[i]->SetTextureSourceProvider(compositor);

    // The lock function will fail if the texture is not compatible with
    // BasicCompositor.
    bool lockResult = aTextures[i]->Lock();
    if (aBackends != LayersBackend::LAYERS_BASIC) {
      EXPECT_FALSE(lockResult);
    } else {
      EXPECT_TRUE(lockResult);
    }
    if (lockResult) {
      aTextures[i]->Unlock();
    }
  }
}

TEST(Gfx, TestTextureCompatibility)
{
  nsTArray<LayersBackend> backendHints;

  GetPlatformBackends(backendHints);
  for (uint32_t i = 0; i < backendHints.Length(); i++) {
    nsTArray<RefPtr<TextureClient>> textureClients;
    nsTArray<RefPtr<TextureHost>> textureHosts;

    CreateTextureWithBackend(backendHints[i], textureClients, textureHosts);
    CheckCompatibilityWithBasicCompositor(backendHints[i], textureHosts);
  }
}
