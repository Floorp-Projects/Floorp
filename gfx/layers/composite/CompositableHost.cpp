/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include <map>        // for _Rb_tree_iterator, map, etc
#include <utility>    // for pair
#include "Effects.h"  // for EffectMask, Effect, etc
#include "gfxUtils.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"     // for TextureHost, etc
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsDebug.h"          // for NS_WARNING
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR, etc
#include "gfxPlatform.h"      // for gfxPlatform
#include "IPDLActor.h"

namespace mozilla {

using namespace gfx;

namespace layers {

class Compositor;

CompositableHost::CompositableHost(const TextureInfo& aTextureInfo)
    : mTextureInfo(aTextureInfo) {
  MOZ_COUNT_CTOR(CompositableHost);
}

CompositableHost::~CompositableHost() { MOZ_COUNT_DTOR(CompositableHost); }

void CompositableHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures) {
}

void CompositableHost::RemoveTextureHost(TextureHost* aTexture) {}

/* static */
already_AddRefed<CompositableHost> CompositableHost::Create(
    const TextureInfo& aTextureInfo) {
  RefPtr<CompositableHost> result;
  switch (aTextureInfo.mCompositableType) {
    case CompositableType::IMAGE:
      result = new WebRenderImageHost(aTextureInfo);
      break;
    default:
      NS_ERROR("Unknown CompositableType");
  }
  return result.forget();
}

void CompositableHost::DumpTextureHost(std::stringstream& aStream,
                                       TextureHost* aTexture) {
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    return;
  }
  aStream << gfxUtils::GetAsDataURI(dSurf).get();
}

}  // namespace layers
}  // namespace mozilla
