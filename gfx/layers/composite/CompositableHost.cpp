/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositableHost.h"
#include "ImageHost.h"
#include "ContentHost.h"
#include "TiledContentHost.h"
#include "mozilla/layers/TextureParent.h"
#include "Effects.h"
#include "mozilla/layers/CompositableTransactionParent.h"

namespace mozilla {
namespace layers {

bool
CompositableHost::Update(const SurfaceDescriptor& aImage,
                         SurfaceDescriptor* aResult)
{
  if (!GetTextureHost()) {
    *aResult = aImage;
    return false;
  }
  MOZ_ASSERT(!GetTextureHost()->GetBuffer(),
             "This path not suitable for texture-level double buffering.");
  GetTextureHost()->Update(aImage);
  *aResult = aImage;
  return GetTextureHost()->IsValid();
}

bool
CompositableHost::AddMaskEffect(EffectChain& aEffects,
                                const gfx::Matrix4x4& aTransform,
                                bool aIs3D)
{
  RefPtr<TextureSource> source = GetTextureHost();
  RefPtr<EffectMask> effect = new EffectMask(source,
                                             source->GetSize(),
                                             aTransform);
  effect->mIs3D = aIs3D;
  aEffects.mSecondaryEffects[EFFECT_MASK] = effect;
  return true;
}

/* static */ TemporaryRef<CompositableHost>
CompositableHost::Create(CompositableType aType, Compositor* aCompositor)
{
  RefPtr<CompositableHost> result;
  switch (aType) {
  case BUFFER_IMAGE_BUFFERED:
    result = new ImageHostBuffered(aCompositor, aType);
    return result;
  case BUFFER_IMAGE_SINGLE:
    result = new ImageHostSingle(aCompositor, aType);
    return result;
  case BUFFER_TILED:
    result = new TiledContentHost(aCompositor);
    return result;
  case BUFFER_CONTENT:
    result = new ContentHostSingleBuffered(aCompositor);
    return result;
  case BUFFER_CONTENT_DIRECT:
    result = new ContentHostDoubleBuffered(aCompositor);
    return result;
  default:
    MOZ_NOT_REACHED("Unknown CompositableType");
    return nullptr;
  }
}

PTextureParent*
CompositableParent::AllocPTexture(const TextureInfo& aInfo)
{
  return new TextureParent(aInfo, this);
}

bool
CompositableParent::DeallocPTexture(PTextureParent* aActor)
{
  delete aActor;
  return true;
}


CompositableParent::CompositableParent(CompositableParentManager* aMgr,
                                       CompositableType aType,
                                       uint64_t aID)
: mManager(aMgr)
, mType(aType)
, mID(aID)
, mCompositorID(0)
{
  MOZ_COUNT_CTOR(CompositableParent);
  mHost = CompositableHost::Create(aType);
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
