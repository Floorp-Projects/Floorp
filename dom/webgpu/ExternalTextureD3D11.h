/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ExternalTextureD3D11_H_
#define GPU_ExternalTextureD3D11_H_

#include "mozilla/webgpu/ExternalTexture.h"

struct ID3D11Texture2D;

namespace mozilla {

namespace webgpu {

class ExternalTextureD3D11 final : public ExternalTexture {
 public:
  static UniquePtr<ExternalTextureD3D11> Create(
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  ExternalTextureD3D11(const uint32_t aWidth, const uint32_t aHeight,
                       const struct ffi::WGPUTextureFormat aFormat,
                       const ffi::WGPUTextureUsages aUsage,
                       RefPtr<ID3D11Texture2D> aTexture);
  virtual ~ExternalTextureD3D11();

  void* GetExternalTextureHandle() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;

  void GetSnapshot(const ipc::Shmem& aDestShmem,
                   const gfx::IntSize& aSize) override;

 protected:
  RefPtr<ID3D11Texture2D> mTexture;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
