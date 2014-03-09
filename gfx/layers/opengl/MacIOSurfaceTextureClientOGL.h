/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H
#define MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H

#include "mozilla/layers/TextureClientOGL.h"

class MacIOSurface;

namespace mozilla {
namespace layers {

class MacIOSurfaceTextureClientOGL : public TextureClient
{
public:
  MacIOSurfaceTextureClientOGL(TextureFlags aFlags);

  virtual ~MacIOSurfaceTextureClientOGL();

  void InitWith(MacIOSurface* aSurface);

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE;

  virtual bool IsAllocated() const MOZ_OVERRIDE { return !!mSurface; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const;

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

protected:
  RefPtr<MacIOSurface> mSurface;
  bool mIsLocked;
};

}
}

#endif // MOZILLA_GFX_MACIOSURFACETEXTURECLIENTOGL_H