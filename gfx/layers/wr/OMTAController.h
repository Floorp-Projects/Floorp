/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_OMTAController_h
#define mozilla_layers_OMTAController_h

#include <unordered_map>

#include "mozilla/layers/LayersTypes.h"  // for LayersId
#include "nsISerialEventTarget.h"
#include "nsISupportsImpl.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace layers {

/**
 * This class just delegates the jank animations notification to the compositor
 * thread from the sampler thread.
 */
class OMTAController final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OMTAController)

 public:
  explicit OMTAController(LayersId aRootLayersId)
      : mRootLayersId(aRootLayersId) {}

  using JankedAnimations =
      std::unordered_map<LayersId, nsTArray<uint64_t>, LayersId::HashFn>;
  void NotifyJankedAnimations(JankedAnimations&& aJankedAnimations) const;

 private:
  ~OMTAController() = default;

  LayersId mRootLayersId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_OMTAController_h
