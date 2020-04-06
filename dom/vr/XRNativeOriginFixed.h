/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRNativeOriginFixed_h_
#define mozilla_dom_XRNativeOriginFixed_h_

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class XRNativeOriginFixed : public XRNativeOrigin {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(XRNativeOriginFixed, override)
  explicit XRNativeOriginFixed(const gfx::PointDouble3D& aPosition);

  gfx::PointDouble3D GetPosition() override;

 private:
  ~XRNativeOriginFixed() = default;

  gfx::PointDouble3D mPosition;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRNativeOriginFixed_h_
