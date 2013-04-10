/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureParent.h"
#include "mozilla/layers/Compositor.h"
#include "CompositableHost.h"
#include "ShadowLayerParent.h"
#include "LayerManagerComposite.h"
#include "Compositor.h"
#include "mozilla/layers/CompositableTransactionParent.h"

namespace mozilla {
namespace layers {

TextureParent::TextureParent(const TextureInfo& aInfo,
                             CompositableParent* aCompositable)
: mTextureInfo(aInfo)
, mLastSurfaceType(SurfaceDescriptor::Tnull_t)
{
  MOZ_COUNT_CTOR(TextureParent);
}

TextureParent::~TextureParent()
{
  MOZ_COUNT_DTOR(TextureParent);
}

bool
TextureParent::EnsureTextureHost(SurfaceDescriptor::Type aSurfaceType)
{
  if (!SurfaceTypeChanged(aSurfaceType)) {
    return false;
  }

  MOZ_ASSERT(!mTextureHost || mTextureHost->GetIPDLActor() == this);

  CompositableParent* compParent = static_cast<CompositableParent*>(Manager());
  CompositableHost* compositable = compParent->GetCompositableHost();

  mTextureHost = TextureHost::CreateTextureHost(aSurfaceType,
                                                mTextureInfo.mTextureHostFlags,
                                                mTextureInfo.mTextureFlags);
  mTextureHost->SetTextureParent(this);
  compositable->AddTextureHost(mTextureHost,
                               compParent->GetCompositableManager());

  mLastSurfaceType = aSurfaceType;

  Compositor* compositor = compositable->GetCompositor();
  if (compositor) {
    mTextureHost->SetCompositor(compositor);
  }
  return true;
}

void TextureParent::SetTextureHost(TextureHost* aHost)
{
  MOZ_ASSERT(!mTextureHost || mTextureHost == aHost);
  mTextureHost = aHost;
}

CompositableHost* TextureParent::GetCompositableHost() const
{
  CompositableParent* actor = static_cast<CompositableParent*>(Manager());
  return actor->GetCompositableHost();
}

uint64_t
TextureParent::GetCompositorID()
{
  return static_cast<CompositableParent*>(Manager())->GetCompositorID();
}


TextureHost* TextureParent::GetTextureHost() const
{
  return mTextureHost;
}

bool TextureParent::SurfaceTypeChanged(SurfaceDescriptor::Type aNewSurfaceType)
{
  return mLastSurfaceType != aNewSurfaceType;
}

void TextureParent::SetCurrentSurfaceType(SurfaceDescriptor::Type aNewSurfaceType)
{
  mLastSurfaceType = aNewSurfaceType;
}


} // namespace
} // namespace
