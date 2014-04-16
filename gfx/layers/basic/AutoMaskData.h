/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_AUTOMASKDATA_H_
#define GFX_AUTOMASKDATA_H_

#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor

namespace mozilla {
namespace layers {

/**
 * Drawing with a mask requires a mask surface and a transform.
 *
 * This helper class manages the SourceSurface logic.
 */
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
