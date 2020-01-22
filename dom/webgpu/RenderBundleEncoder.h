/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_RenderBundleEncoder_H_
#define GPU_RenderBundleEncoder_H_

#include "ObjectModel.h"
#include "RenderEncoderBase.h"

namespace mozilla {
namespace webgpu {

class Device;
class RenderBundle;

class RenderBundleEncoder final : public RenderEncoderBase,
                                  public ChildOf<Device> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(RenderBundleEncoder,
                                                         RenderEncoderBase)
  GPU_DECL_JS_WRAP(RenderBundleEncoder)

  RenderBundleEncoder() = delete;

 private:
  ~RenderBundleEncoder() = default;

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_RenderBundleEncoder_H_
