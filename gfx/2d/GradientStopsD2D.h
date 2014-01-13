/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRADIENTSTOPSD2D_H_
#define MOZILLA_GFX_GRADIENTSTOPSD2D_H_

#include "2D.h"

#include <d2d1.h>

namespace mozilla {
namespace gfx {

class GradientStopsD2D : public GradientStops
{
public:
  GradientStopsD2D(ID2D1GradientStopCollection *aStopCollection)
    : mStopCollection(aStopCollection)
  {}

  virtual BackendType GetBackendType() const { return BackendType::DIRECT2D; }

private:
  friend class DrawTargetD2D;
  friend class DrawTargetD2D1;

  mutable RefPtr<ID2D1GradientStopCollection> mStopCollection;
};

}
}

#endif /* MOZILLA_GFX_GRADIENTSTOPSD2D_H_ */
