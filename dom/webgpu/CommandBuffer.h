/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandBuffer_H_
#define GPU_CommandBuffer_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
class HTMLCanvasElement;
}  // namespace dom
namespace webgpu {

class Device;

class CommandBuffer final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandBuffer)
  GPU_DECL_JS_WRAP(CommandBuffer)

  CommandBuffer(Device* const aParent, RawId aId,
                const WeakPtr<dom::HTMLCanvasElement>& aTargetCanvasElement);

  Maybe<RawId> Commit();

 private:
  CommandBuffer() = delete;
  ~CommandBuffer();
  void Cleanup();

  const RawId mId;
  const WeakPtr<dom::HTMLCanvasElement> mTargetCanvasElement;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandBuffer_H_
