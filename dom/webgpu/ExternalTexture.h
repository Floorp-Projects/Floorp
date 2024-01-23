/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExternalTexture_H_
#define ExternalTexture_H_

#include "mozilla/gfx/Point.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {

namespace ipc {
class Shmem;
}

namespace webgpu {

// A texture that can be used by the WebGPU implementation but is created and
// owned by Gecko
class ExternalTexture {
 public:
  static UniquePtr<ExternalTexture> Create(
      const uint32_t aWidth, const uint32_t aHeight,
      const struct ffi::WGPUTextureFormat aFormat,
      const ffi::WGPUTextureUsages aUsage);

  ExternalTexture(const uint32_t aWidth, const uint32_t aHeight,
                  const struct ffi::WGPUTextureFormat aFormat,
                  const ffi::WGPUTextureUsages aUsage);
  virtual ~ExternalTexture();

  virtual void* GetExternalTextureHandle() { return nullptr; }

  virtual Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor(
      Maybe<gfx::FenceInfo>& aFenceInfo) = 0;

  virtual void GetSnapshot(const ipc::Shmem& aDestShmem,
                           const gfx::IntSize& aSize) {}

  gfx::IntSize GetSize() { return gfx::IntSize(mWidth, mHeight); }

  void SetSubmissionIndex(uint64_t aSubmissionIndex);
  uint64_t GetSubmissionIndex() const { return mSubmissionIndex; }

  const uint32_t mWidth;
  const uint32_t mHeight;
  const struct ffi::WGPUTextureFormat mFormat;
  const ffi::WGPUTextureUsages mUsage;

 protected:
  uint64_t mSubmissionIndex = 0;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ExternalTexture_H_
