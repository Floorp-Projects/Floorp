/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include <map>                          // for _Rb_tree_iterator, map, etc
#include <utility>                      // for pair
#include "ContentHost.h"                // for ContentHostDoubleBuffered, etc
#include "Effects.h"                    // for EffectMask, Effect, etc
#include "gfxUtils.h"
#include "ImageHost.h"                  // for ImageHostBuffered, etc
#include "TiledContentHost.h"           // for TiledContentHost
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "nsDebug.h"                    // for NS_WARNING
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "mozilla/layers/PCompositableParent.h"
#include "IPDLActor.h"

namespace mozilla {

using namespace gfx;

namespace layers {

class Compositor;

/**
 * IPDL actor used by CompositableHost to match with its corresponding
 * CompositableClient on the content side.
 *
 * CompositableParent is owned by the IPDL system. It's deletion is triggered
 * by either the CompositableChild's deletion, or by the IPDL communication
 * going down.
 */
class CompositableParent : public ParentActor<PCompositableParent>
{
public:
  CompositableParent(CompositableParentManager* aMgr, const TextureInfo& aTextureInfo)
  {
    MOZ_COUNT_CTOR(CompositableParent);
    mHost = CompositableHost::Create(aTextureInfo);
  }

  ~CompositableParent()
  {
    MOZ_COUNT_DTOR(CompositableParent);
  }

  virtual void Destroy() override
  {
    if (mHost) {
      mHost->Detach(nullptr, CompositableHost::FORCE_DETACH);
    }
  }

  RefPtr<CompositableHost> mHost;
};

CompositableHost::CompositableHost(const TextureInfo& aTextureInfo)
  : mTextureInfo(aTextureInfo)
  , mCompositorID(0)
  , mCompositor(nullptr)
  , mLayer(nullptr)
  , mFlashCounter(0)
  , mAttached(false)
  , mKeepAttached(false)
{
  MOZ_COUNT_CTOR(CompositableHost);
}

CompositableHost::~CompositableHost()
{
  MOZ_COUNT_DTOR(CompositableHost);
}

PCompositableParent*
CompositableHost::CreateIPDLActor(CompositableParentManager* aMgr,
                                  const TextureInfo& aTextureInfo)
{
  return new CompositableParent(aMgr, aTextureInfo);
}

bool
CompositableHost::DestroyIPDLActor(PCompositableParent* aActor)
{
  delete aActor;
  return true;
}

CompositableHost*
CompositableHost::FromIPDLActor(PCompositableParent* aActor)
{
  MOZ_ASSERT(aActor);
  return static_cast<CompositableParent*>(aActor)->mHost;
}

void
CompositableHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures)
{
  if (GetCompositor()) {
    for (auto& texture : aTextures) {
      texture.mTexture->SetCompositor(GetCompositor());
    }
  }
}

void
CompositableHost::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                            TextureHost* aTextureOnWhite)
{
  MOZ_ASSERT(aTextureOnBlack && aTextureOnWhite);
  if (GetCompositor()) {
    aTextureOnBlack->SetCompositor(GetCompositor());
    aTextureOnWhite->SetCompositor(GetCompositor());
  }
}

void
CompositableHost::RemoveTextureHost(TextureHost* aTexture)
{}

void
CompositableHost::SetCompositor(Compositor* aCompositor)
{
  MOZ_ASSERT(aCompositor);
  mCompositor = aCompositor;
}

bool
CompositableHost::AddMaskEffect(EffectChain& aEffects,
                                const gfx::Matrix4x4& aTransform)
{
  CompositableTextureSourceRef source;
  RefPtr<TextureHost> host = GetAsTextureHost();

  if (!host) {
    NS_WARNING("Using compositable with no valid TextureHost as mask");
    return false;
  }

  if (!host->Lock()) {
    NS_WARNING("Failed to lock the mask texture");
    return false;
  }

  if (!host->BindTextureSource(source)) {
    NS_WARNING("The TextureHost was successfully locked but can't provide a TextureSource");
    host->Unlock();
    return false;
  }
  MOZ_ASSERT(source);

  RefPtr<EffectMask> effect = new EffectMask(source,
                                             source->GetSize(),
                                             aTransform);
  aEffects.mSecondaryEffects[EffectTypes::MASK] = effect;
  return true;
}

void
CompositableHost::RemoveMaskEffect()
{
  RefPtr<TextureHost> host = GetAsTextureHost();
  if (host) {
    host->Unlock();
  }
}

/* static */ already_AddRefed<CompositableHost>
CompositableHost::Create(const TextureInfo& aTextureInfo)
{
  RefPtr<CompositableHost> result;
  switch (aTextureInfo.mCompositableType) {
  case CompositableType::IMAGE_BRIDGE:
    NS_ERROR("Cannot create an image bridge compositable this way");
    break;
  case CompositableType::CONTENT_TILED:
    result = new TiledContentHost(aTextureInfo);
    break;
  case CompositableType::IMAGE:
    result = new ImageHost(aTextureInfo);
    break;
  case CompositableType::CONTENT_SINGLE:
    result = new ContentHostSingleBuffered(aTextureInfo);
    break;
  case CompositableType::CONTENT_DOUBLE:
    result = new ContentHostDoubleBuffered(aTextureInfo);
    break;
  default:
    NS_ERROR("Unknown CompositableType");
  }
  return result.forget();
}

void
CompositableHost::DumpTextureHost(std::stringstream& aStream, TextureHost* aTexture)
{
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  if (!dSurf) {
    return;
  }
  aStream << gfxUtils::GetAsDataURI(dSurf).get();
}

void
CompositableHost::ReceivedDestroy(PCompositableParent* aActor)
{
  static_cast<CompositableParent*>(aActor)->RecvDestroy();
}

} // namespace layers
} // namespace mozilla
