/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_DISPLAY_PRESENTATION_H
#define GFX_VR_DISPLAY_PRESENTATION_H

#include "mozilla/RefPtr.h"
#include "mozilla/dom/VRDisplayBinding.h"

namespace mozilla {
namespace dom {
class XRWebGLLayer;
}
namespace gfx {
class VRDisplayClient;
class VRLayerChild;

class VRDisplayPresentation final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRDisplayPresentation)

 public:
  VRDisplayPresentation(VRDisplayClient* aDisplayClient,
                        const nsTArray<dom::VRLayer>& aLayers, uint32_t aGroup);
  void UpdateLayers(const nsTArray<mozilla::dom::VRLayer>& aLayers);
  void UpdateXRWebGLLayer(dom::XRWebGLLayer* aLayer);
  void SubmitFrame();
  void GetDOMLayers(nsTArray<dom::VRLayer>& result);
  uint32_t GetGroup() const;

 private:
  ~VRDisplayPresentation();
  void CreateLayers();
  void DestroyLayers();

  RefPtr<VRDisplayClient> mDisplayClient;
  nsTArray<dom::VRLayer> mDOMLayers;
  nsTArray<RefPtr<VRLayerChild>> mLayers;
  uint32_t mGroup;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_DISPLAY_PRESENTAITON_H */
