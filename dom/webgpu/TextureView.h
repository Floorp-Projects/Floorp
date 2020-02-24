/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_TextureView_H_
#define GPU_TextureView_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class TextureView final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(TextureView)
  GPU_DECL_JS_WRAP(TextureView)

  const RawId mId;

 private:
  TextureView() = delete;
  virtual ~TextureView();
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_TextureView_H_
