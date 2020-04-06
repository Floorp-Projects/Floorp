/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRNativeOriginLocal_h_
#define mozilla_dom_XRNativeOriginLocal_h_

#include "gfxVR.h"

namespace mozilla {
namespace gfx {
class VRDisplayClient;
}
namespace dom {

class XRNativeOriginLocal : public XRNativeOrigin {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(XRNativeOriginLocal, override)
  explicit XRNativeOriginLocal(gfx::VRDisplayClient* aDisplay);

  gfx::PointDouble3D GetPosition() override;

 private:
  ~XRNativeOriginLocal() = default;
  RefPtr<gfx::VRDisplayClient> mDisplay;
  gfx::PointDouble3D mInitialPosition;
  bool mInitialPositionValid;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRNativeOriginLocal_h_
