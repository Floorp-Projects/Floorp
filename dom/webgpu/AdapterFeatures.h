/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_AdapterFeatures_H_
#define GPU_AdapterFeatures_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {
class Adapter;

class AdapterFeatures final : public nsWrapperCache, public ChildOf<Adapter> {
 public:
  GPU_DECL_CYCLE_COLLECTION(AdapterFeatures)
  GPU_DECL_JS_WRAP(AdapterFeatures)

  explicit AdapterFeatures(Adapter* const aParent);

 private:
  ~AdapterFeatures() = default;
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_AdapterFeatures_H_
