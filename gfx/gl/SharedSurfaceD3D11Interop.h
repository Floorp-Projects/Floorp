/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_D3D11_INTEROP_H_
#define SHARED_SURFACE_D3D11_INTEROP_H_

#include <d3d11.h>
#include <windows.h>
#include "SharedSurface.h"

namespace mozilla {
namespace gl {

class DXInterop2Device;
class GLContext;
class WGLLibrary;

class SharedSurface_D3D11Interop final : public SharedSurface {
 public:
  struct Data final {
    const RefPtr<DXInterop2Device> interop;
    HANDLE lockHandle;
    RefPtr<ID3D11Texture2D> texD3D;
    HANDLE dxgiHandle;
    UniquePtr<Renderbuffer> interopRb;
    UniquePtr<MozFramebuffer> interopFbIfNeedsIndirect;
  };
  const Data mData;
  const bool mNeedsFinish;

 private:
  bool mLockedForGL = false;

 public:
  static UniquePtr<SharedSurface_D3D11Interop> Create(const SharedSurfaceDesc&,
                                                      DXInterop2Device*);

 private:
  SharedSurface_D3D11Interop(const SharedSurfaceDesc&,
                             UniquePtr<MozFramebuffer>&& fbForDrawing, Data&&);

 public:
  virtual ~SharedSurface_D3D11Interop();

  void LockProdImpl() override {}
  void UnlockProdImpl() override {}

  void ProducerAcquireImpl() override;
  void ProducerReleaseImpl() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;
};

class SurfaceFactory_D3D11Interop : public SurfaceFactory {
 public:
  const RefPtr<DXInterop2Device> mInterop;

  static UniquePtr<SurfaceFactory_D3D11Interop> Create(GLContext& gl);

 protected:
  SurfaceFactory_D3D11Interop(const PartialSharedSurfaceDesc&,
                              DXInterop2Device* interop);

 public:
  virtual ~SurfaceFactory_D3D11Interop();

 protected:
  UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    return SharedSurface_D3D11Interop::Create(desc, mInterop);
  }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_D3D11_INTEROP_H_ */
