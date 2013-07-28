/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include "ImageHost.h"
#include "ContentHost.h"
#include "TiledContentHost.h"
#include "Effects.h"
#include "mozilla/layers/CompositableTransactionParent.h"

namespace mozilla {
namespace layers {

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
  RefPtr<TextureSource> source = GetDeprecatedTextureHost();

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

/* static */ TemporaryRef<CompositableHost>
CompositableHost::Create(const TextureInfo& aTextureInfo)
{
  RefPtr<CompositableHost> result;
  switch (aTextureInfo.mCompositableType) {
  case BUFFER_IMAGE_BUFFERED:
    result = new DeprecatedImageHostBuffered(aTextureInfo);
    return result;
  case BUFFER_IMAGE_SINGLE:
    result = new DeprecatedImageHostSingle(aTextureInfo);
    return result;
  case BUFFER_TILED:
    result = new TiledContentHost(aTextureInfo);
    return result;
  case BUFFER_CONTENT:
    result = new ContentHostSingleBuffered(aTextureInfo);
    return result;
  case BUFFER_CONTENT_DIRECT:
    result = new ContentHostDoubleBuffered(aTextureInfo);
    return result;
  case BUFFER_CONTENT_INC:
    result = new ContentHostIncremental(aTextureInfo);
    return result;
  default:
    MOZ_CRASH("Unknown CompositableType");
  }
}

void
CompositableHost::DumpDeprecatedTextureHost(FILE* aFile, DeprecatedTextureHost* aTexture)
{
  if (!aTexture) {
    return;
  }
  nsRefPtr<gfxImageSurface> surf = aTexture->GetAsSurface();
  if (!surf) {
    return;
  }
  surf->DumpAsDataURL(aFile ? aFile : stderr);
}

void
CompositableParent::ActorDestroy(ActorDestroyReason why)
{
  if (mHost) {
    mHost->Detach();
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
