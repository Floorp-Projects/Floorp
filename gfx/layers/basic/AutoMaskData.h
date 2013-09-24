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
 * Sometimes the mask surface is a direct gfxASurface, but other times
 * it's a SurfaceDescriptor.  For SurfaceDescriptor, we need to use a
 * scoped AutoOpenSurface to get a gfxASurface for the
 * SurfaceDescriptor.
 *
 * This helper class manages the gfxASurface-or-SurfaceDescriptor
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

  void Construct(const gfxMatrix& aTransform,
                 gfxASurface* aSurface);

  void Construct(const gfxMatrix& aTransform,
                 const SurfaceDescriptor& aSurface);

  /** The returned surface can't escape the scope of |this|. */
  gfxASurface* GetSurface();
  const gfxMatrix& GetTransform();

private:
  bool IsConstructed();

  gfxMatrix mTransform;
  nsRefPtr<gfxASurface> mSurface;
  Maybe<AutoOpenSurface> mSurfaceOpener;

  AutoMaskData(const AutoMaskData&) MOZ_DELETE;
  AutoMaskData& operator=(const AutoMaskData&) MOZ_DELETE;
};

}
}

#endif // GFX_AUTOMASKDATA_H_