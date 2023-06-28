/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_OutOfMemoryError_H_
#define GPU_OutOfMemoryError_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
class GlobalObject;
}  // namespace dom
namespace webgpu {
class Device;

class OutOfMemoryError final : public nsWrapperCache, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(OutOfMemoryError)
  GPU_DECL_JS_WRAP(OutOfMemoryError)

  explicit OutOfMemoryError(const RefPtr<Device>& aParent)
      : ChildOf<Device>(aParent) {}

 private:
  virtual ~OutOfMemoryError();
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_OutOfMemoryError_H_
