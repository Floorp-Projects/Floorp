/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SupportedFeatures_H_
#define GPU_SupportedFeatures_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

#include <unordered_set>

namespace mozilla {
class ErrorResult;
namespace dom {
enum class GPUFeatureName : uint8_t;
}  // namespace dom
}  // namespace mozilla

namespace mozilla::webgpu {
class Adapter;

class SupportedFeatures final : public nsWrapperCache, public ChildOf<Adapter> {
 public:
  GPU_DECL_CYCLE_COLLECTION(SupportedFeatures)
  GPU_DECL_JS_WRAP(SupportedFeatures)

  explicit SupportedFeatures(Adapter* const aParent);

  void Add(dom::GPUFeatureName, ErrorResult&);
  const auto& Features() const { return mFeatures; }

 private:
  ~SupportedFeatures() = default;
  void Cleanup() {}

  std::unordered_set<dom::GPUFeatureName> mFeatures;
};

}  // namespace mozilla::webgpu

#endif  // GPU_SupportedFeatures_H_
