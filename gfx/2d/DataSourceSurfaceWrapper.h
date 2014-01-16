/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DATASOURCESURFACEWRAPPER_H_
#define MOZILLA_GFX_DATASOURCESURFACEWRAPPER_H_

#include "2D.h"

namespace mozilla {
namespace gfx {

// Wraps a DataSourceSurface and forwards all methods except for GetType(),
// from which it always returns SurfaceType::DATA.
class DataSourceSurfaceWrapper : public DataSourceSurface
{
public:
  DataSourceSurfaceWrapper(DataSourceSurface *aSurface)
   : mSurface(aSurface)
  {}

  virtual SurfaceType GetType() const MOZ_OVERRIDE { return SurfaceType::DATA; }

  virtual uint8_t *GetData() MOZ_OVERRIDE { return mSurface->GetData(); }
  virtual int32_t Stride() MOZ_OVERRIDE { return mSurface->Stride(); }
  virtual IntSize GetSize() const MOZ_OVERRIDE { return mSurface->GetSize(); }
  virtual SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mSurface->GetFormat(); }
  virtual bool IsValid() const MOZ_OVERRIDE { return mSurface->IsValid(); }

private:
  RefPtr<DataSourceSurface> mSurface;
};

}
}

#endif /* MOZILLA_GFX_DATASOURCESURFACEWRAPPER_H_ */
