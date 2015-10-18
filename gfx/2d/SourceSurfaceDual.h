/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
     
#ifndef MOZILLA_GFX_SOURCESURFACEDUAL_H_
#define MOZILLA_GFX_SOURCESURFACEDUAL_H_
     
#include "2D.h"
     
namespace mozilla {
namespace gfx {

class DualSurface;
class DualPattern;

class SourceSurfaceDual : public SourceSurface
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(SourceSurfaceDual)
  SourceSurfaceDual(DrawTarget *aDTA, DrawTarget *aDTB)
    : mA(aDTA->Snapshot())
    , mB(aDTB->Snapshot())
  { }

  virtual SurfaceType GetType() const { return SurfaceType::DUAL_DT; }
  virtual IntSize GetSize() const { return mA->GetSize(); }
  virtual SurfaceFormat GetFormat() const { return mA->GetFormat(); }

  // This is implemented for debugging purposes only (used by dumping
  // client-side textures for paint dumps), for which we don't care about
  // component alpha, so we just use the first of the two surfaces.
  virtual already_AddRefed<DataSourceSurface> GetDataSurface() {
    return mA->GetDataSurface();
  }
private:
  friend class DualSurface;
  friend class DualPattern;

  RefPtr<SourceSurface> mA;
  RefPtr<SourceSurface> mB;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SOURCESURFACEDUAL_H_ */
