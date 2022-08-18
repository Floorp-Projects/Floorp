/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Texture_H_
#define GPU_Texture_H_

#include "mozilla/WeakPtr.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/webgpu/WebGPUTypes.h"

namespace mozilla {
namespace dom {
struct GPUTextureDescriptor;
struct GPUTextureViewDescriptor;
enum class GPUTextureFormat : uint8_t;
}  // namespace dom

namespace webgpu {
namespace ffi {
struct WGPUTextureViewDescriptor;
}  // namespace ffi

class CanvasContext;
class Device;
class TextureView;

class Texture final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Texture)
  GPU_DECL_JS_WRAP(Texture)

  Texture(Device* const aParent, RawId aId,
          const dom::GPUTextureDescriptor& aDesc);
  Device* GetParentDevice() { return mParent; }
  const RawId mId;
  const dom::GPUTextureFormat mFormat;
  const Maybe<uint8_t> mBytesPerBlock;

  WeakPtr<CanvasContext> mTargetContext;

 private:
  virtual ~Texture();
  void Cleanup();

 public:
  already_AddRefed<TextureView> CreateView(
      const dom::GPUTextureViewDescriptor& aDesc);
  void Destroy();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
