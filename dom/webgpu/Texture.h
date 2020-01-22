/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Texture_H_
#define GPU_Texture_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
struct GPUTextureViewDescriptor;
}  // namespace dom

namespace webgpu {

class Device;
class TextureView;

class Texture final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Texture)
  GPU_DECL_JS_WRAP(Texture)

 private:
  Texture() = delete;
  virtual ~Texture();
  void Cleanup() {}

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Texture_H_
