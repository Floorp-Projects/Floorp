/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include <map>                          // for _Rb_tree_iterator, map, etc
#include <utility>                      // for pair
#include "ContentHost.h"                // for ContentHostDoubleBuffered, etc
#include "Effects.h"                    // for EffectMask, Effect, etc
#include "ImageHost.h"                  // for DeprecatedImageHostBuffered, etc
#include "TiledContentHost.h"           // for TiledContentHost
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_WARNING
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "gfxPlatform.h"                // for gfxPlatform

namespace mozilla {
namespace layers {

class Matrix4x4;
class Compositor;

CompositableHost::CompositableHost(const TextureInfo& aTextureInfo)
  : mTextureInfo(aTextureInfo)
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
  if (mBackendData) {
    mBackendData->ClearData();
  }
}

void
CompositableHost::UseTextureHost(TextureHost* aTexture)
{
  if (!aTexture) {
    return;
  }
  aTexture->SetCompositor(GetCompositor());
  aTexture->SetCompositableBackendSpecificData(GetCompositableBackendSpecificData());
}

void
CompositableHost::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                            TextureHost* aTextureOnWhite)
{
  MOZ_ASSERT(aTextureOnBlack && aTextureOnWhite);
  aTextureOnBlack->SetCompositor(GetCompositor());
  aTextureOnBlack->SetCompositableBackendSpecificData(GetCompositableBackendSpecificData());
  aTextureOnWhite->SetCompositor(GetCompositor());
  aTextureOnWhite->SetCompositableBackendSpecificData(GetCompositableBackendSpecificData());
}

void
CompositableHost::RemoveTextureHost(TextureHost* aTexture)
{
  // Clear strong refrence to CompositableBackendSpecificData
  aTexture->SetCompositableBackendSpecificData(nullptr);
}

void
CompositableHost::SetCompositor(Compositor* aCompositor)
{
  mCompositor = aCompositor;
}

bool
CompositableHost::Update(const SurfaceDescriptor& aImage,
                         SurfaceDescriptor* aResult)
{
  if (!GetDeprecatedTextureHost()) {
    *aResult = aImage;
    return false;
  }
  MOZ_ASSERT(!GetDeprecatedTextureHost()->GetBuffer(),
             "This path not suitable for texture-level double buffering.");
  GetDeprecatedTextureHost()->Update(aImage);
  *aResult = aImage;
  return GetDeprecatedTextureHost()->IsValid();
}

bool
CompositableHost::AddMaskEffect(EffectChain& aEffects,
                                const gfx::Matrix4x4& aTransform,
                                bool aIs3D)
{
  RefPtr<TextureSource> source;
  RefPtr<DeprecatedTextureHost> oldHost = GetDeprecatedTextureHost();
  if (oldHost && oldHost->Lock()) {
    source = oldHost;
  } else {
    RefPtr<TextureHost> host = GetAsTextureHost();
    if (host && host->Lock()) {
      source = host->GetTextureSources();
    }
  }

  if (!source) {
    NS_WARNING("Using compositable with no texture host as mask layer");
    return false;
  }

  RefPtr<EffectMask> effect = new EffectMask(source,
                                             source->GetSize(),
                                             aTransform);
  effect->mIs3D = aIs3D;
  aEffects.mSecondaryEffects[EFFECT_MASK] = effect;
  return true;
}

void
CompositableHost::RemoveMaskEffect()
{
  RefPtr<DeprecatedTextureHost> oldHost = GetDeprecatedTextureHost();
  if (oldHost) {
    oldHost->Unlock();
  } else {
    RefPtr<TextureHost> host = GetAsTextureHost();
    if (host) {
      host->Unlock();
    }
  }
}

// implemented in TextureHostOGL.cpp
TemporaryRef<CompositableBackendSpecificData> CreateCompositableBackendSpecificDataOGL();

/* static */ TemporaryRef<CompositableHost>
CompositableHost::Create(const TextureInfo& aTextureInfo)
{
  RefPtr<CompositableHost> result;
  switch (aTextureInfo.mCompositableType) {
  case BUFFER_IMAGE_SINGLE:
    result = new DeprecatedImageHostSingle(aTextureInfo);
    break;
  case BUFFER_IMAGE_BUFFERED:
    result = new DeprecatedImageHostBuffered(aTextureInfo);
    break;
  case BUFFER_BRIDGE:
    MOZ_CRASH("Cannot create an image bridge compositable this way");
    break;
  case BUFFER_CONTENT:
    result = new DeprecatedContentHostSingleBuffered(aTextureInfo);
    break;
  case BUFFER_CONTENT_DIRECT:
    result = new DeprecatedContentHostDoubleBuffered(aTextureInfo);
    break;
  case BUFFER_CONTENT_INC:
    result = new ContentHostIncremental(aTextureInfo);
    break;
  case BUFFER_TILED:
  case BUFFER_SIMPLE_TILED:
    result = new TiledContentHost(aTextureInfo);
    break;
  case COMPOSITABLE_IMAGE:
    result = new ImageHost(aTextureInfo);
    break;
  case COMPOSITABLE_CONTENT_SINGLE:
    result = new ContentHostSingleBuffered(aTextureInfo);
    break;
  case COMPOSITABLE_CONTENT_DOUBLE:
    result = new ContentHostDoubleBuffered(aTextureInfo);
    break;
  default:
    MOZ_CRASH("Unknown CompositableType");
  }
  // We know that Tiled buffers don't use the compositable backend-specific
  // data, so don't bother creating it.
  if (result && aTextureInfo.mCompositableType != BUFFER_TILED) {
    RefPtr<CompositableBackendSpecificData> data = CreateCompositableBackendSpecificDataOGL();
    result->SetCompositableBackendSpecificData(data);
  }
  return result;
}

#ifdef MOZ_DUMP_PAINTING
void
CompositableHost::DumpDeprecatedTextureHost(FILE* aFile, DeprecatedTextureHost* aTexture)
{
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  gfxPlatform *platform = gfxPlatform::GetPlatform();
  RefPtr<gfx::DrawTarget> dt = platform->CreateDrawTargetForData(dSurf->GetData(),
                                                                 dSurf->GetSize(),
                                                                 dSurf->Stride(),
                                                                 dSurf->GetFormat());
  nsRefPtr<gfxASurface> surf = platform->GetThebesSurfaceForDrawTarget(dt);
  if (!surf) {
    return;
  }
  surf->DumpAsDataURL(aFile ? aFile : stderr);
}

void
CompositableHost::DumpTextureHost(FILE* aFile, TextureHost* aTexture)
{
  if (!aTexture) {
    return;
  }
  RefPtr<gfx::DataSourceSurface> dSurf = aTexture->GetAsSurface();
  gfxPlatform *platform = gfxPlatform::GetPlatform();
  RefPtr<gfx::DrawTarget> dt = platform->CreateDrawTargetForData(dSurf->GetData(),
                                                                 dSurf->GetSize(),
                                                                 dSurf->Stride(),
                                                                 dSurf->GetFormat());
  nsRefPtr<gfxASurface> surf = platform->GetThebesSurfaceForDrawTarget(dt);
  if (!surf) {
    return;
  }
  surf->DumpAsDataURL(aFile ? aFile : stderr);
}
#endif

void
CompositableParent::ActorDestroy(ActorDestroyReason why)
{
  if (mHost) {
    mHost->Detach(nullptr, CompositableHost::FORCE_DETACH);
  }
}

CompositableParent::CompositableParent(CompositableParentManager* aMgr,
                                       const TextureInfo& aTextureInfo,
                                       uint64_t aID)
: mManager(aMgr)
, mType(aTextureInfo.mCompositableType)
, mID(aID)
, mCompositorID(0)
{
  MOZ_COUNT_CTOR(CompositableParent);
  mHost = CompositableHost::Create(aTextureInfo);
  if (aID) {
    CompositableMap::Set(aID, this);
  }
}

CompositableParent::~CompositableParent()
{
  MOZ_COUNT_DTOR(CompositableParent);
  CompositableMap::Erase(mID);
}

namespace CompositableMap {

typedef std::map<uint64_t, CompositableParent*> CompositableMap_t;
static CompositableMap_t* sCompositableMap = nullptr;
bool IsCreated() {
  return sCompositableMap != nullptr;
}
CompositableParent* Get(uint64_t aID)
{
  if (!IsCreated() || aID == 0) {
    return nullptr;
  }
  CompositableMap_t::iterator it = sCompositableMap->find(aID);
  if (it == sCompositableMap->end()) {
    return nullptr;
  }
  return it->second;
}
void Set(uint64_t aID, CompositableParent* aParent)
{
  if (!IsCreated() || aID == 0) {
    return;
  }
  (*sCompositableMap)[aID] = aParent;
}
void Erase(uint64_t aID)
{
  if (!IsCreated() || aID == 0) {
    return;
  }
  CompositableMap_t::iterator it = sCompositableMap->find(aID);
  if (it != sCompositableMap->end()) {
    sCompositableMap->erase(it);
  }
}
void Clear()
{
  if (!IsCreated()) {
    return;
  }
  sCompositableMap->clear();
}
void Create()
{
  if (sCompositableMap == nullptr) {
    sCompositableMap = new CompositableMap_t;
  }
}
void Destroy()
{
  Clear();
  delete sCompositableMap;
  sCompositableMap = nullptr;
}

} // namespace CompositableMap

} // namespace layers
} // namespace mozilla
