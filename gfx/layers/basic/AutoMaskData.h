/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_AUTOMASKDATA_H_
#define GFX_AUTOMASKDATA_H_

#include "gfxASurface.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor

namespace mozilla {
namespace layers {

/**
 * Drawing with a mask requires a mask surface and a transform.
 *
 * This helper class manages the gfxASurface
 * logic.
 */
class MOZ_STACK_CLASS AutoMaskData {
public:
  AutoMaskData() { }
  ~AutoMaskData() { }

  /**
   * Construct this out of either a gfxASurface or a
   * SurfaceDescriptor.  Construct() must only be called once.
   * GetSurface() and GetTransform() must not be called until this has
   * been constructed.
   */

  void Construct(const gfx::Matrix& aTransform,
                 gfxASurface* aSurface);

  /** The returned surface can't escape the scope of |this|. */
  gfxASurface* GetSurface();
  const gfx::Matrix& GetTransform();

private:
  bool IsConstructed();

  gfx::Matrix mTransform;
  nsRefPtr<gfxASurface> mSurface;

  AutoMaskData(const AutoMaskData&) MOZ_DELETE;
  AutoMaskData& operator=(const AutoMaskData&) MOZ_DELETE;
};

class MOZ_STACK_CLASS AutoMoz2DMaskData {
public:
  AutoMoz2DMaskData() { }
  ~AutoMoz2DMaskData() { }

  void Construct(const gfx::Matrix& aTransform,
                 gfx::SourceSurface* aSurface)
  {
    MOZ_ASSERT(!IsConstructed());
    mTransform = aTransform;
    mSurface = aSurface;
  }

  gfx::SourceSurface* GetSurface()
  {
    MOZ_ASSERT(IsConstructed());
    return mSurface.get();
  }

  const gfx::Matrix& GetTransform()
  {
    MOZ_ASSERT(IsConstructed());
    return mTransform;
  }

private:
  bool IsConstructed()
  {
    return !!mSurface;
  }

  gfx::Matrix mTransform;
  RefPtr<gfx::SourceSurface> mSurface;

  AutoMoz2DMaskData(const AutoMoz2DMaskData&) MOZ_DELETE;
  AutoMoz2DMaskData& operator=(const AutoMoz2DMaskData&) MOZ_DELETE;
};

}
}

#endif // GFX_AUTOMASKDATA_H_
