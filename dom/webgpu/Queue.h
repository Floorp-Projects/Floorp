/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Queue_H_
#define GPU_Queue_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class CommandBuffer;
class Device;
class Fence;

class Queue final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Queue)
  GPU_DECL_JS_WRAP(Queue)

 private:
  Queue() = delete;
  virtual ~Queue();

 public:
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Queue_H_
