/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRADIENTSTOPSD2D_H_
#define MOZILLA_GFX_GRADIENTSTOPSD2D_H_

#include "2D.h"

#include <d2d1.h>

namespace mozilla {
namespace gfx {

class GradientStopsD2D : public GradientStops {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsD2D, override)

  GradientStopsD2D(ID2D1GradientStopCollection* aStopCollection,
                   ID3D11Device* aDevice)
      : mStopCollection(aStopCollection), mDevice(aDevice) {}

  virtual BackendType GetBackendType() const { return BackendType::DIRECT2D; }

  bool IsValid() const final {
    return mDevice == Factory::GetDirect3D11Device();
  }

 private:
  friend class DrawTargetD2D;
  friend class DrawTargetD2D1;

  mutable RefPtr<ID2D1GradientStopCollection> mStopCollection;
  RefPtr<ID3D11Device> mDevice;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_GRADIENTSTOPSD2D_H_ */
