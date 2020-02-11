/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderBundleEncoder_H_
#define GPU_RenderBundleEncoder_H_

#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;
class RenderBundle;

class RenderBundleEncoder final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(RenderBundleEncoder)
  GPU_DECL_JS_WRAP(RenderBundleEncoder)

  RenderBundleEncoder() = delete;

 private:
  ~RenderBundleEncoder() = default;
  void Cleanup() {}

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_RenderBundleEncoder_H_
