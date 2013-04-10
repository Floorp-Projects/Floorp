/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_TEXTUREPARENT_H
#define MOZILLA_LAYERS_TEXTUREPARENT_H

#include "mozilla/layers/PTextureParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "CompositableHost.h"

namespace mozilla {
namespace layers {

class TextureHost;
class CompositableHost;
class TextureInfo;

class TextureParent : public PTextureParent
{
public:
  TextureParent(const TextureInfo& aInfo, CompositableParent* aCompositable);
  virtual ~TextureParent();

  void SetTextureHost(TextureHost* aHost);

  TextureHost* GetTextureHost() const;
  CompositableHost* GetCompositableHost() const;

  const TextureInfo& GetTextureInfo() const
  {
    return mTextureInfo;
  }

  bool SurfaceTypeChanged(SurfaceDescriptor::Type aNewSurfaceType);
  void SetCurrentSurfaceType(SurfaceDescriptor::Type aNewSurfaceType);
  SurfaceDescriptor::Type GetSurfaceType() const
  {
    return mLastSurfaceType;
  }

  uint64_t GetCompositorID();

  bool EnsureTextureHost(SurfaceDescriptor::Type aSurfaceType);
private:
  TextureInfo mTextureInfo;
  RefPtr<TextureHost> mTextureHost;
  SurfaceDescriptor::Type mLastSurfaceType;
};

} // namespace
} // namespace

#endif
