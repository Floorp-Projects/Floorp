/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRInputSpace_h_
#define mozilla_dom_XRInputSpace_h_

#include "XRInputSpace.h"

namespace mozilla {
namespace dom {

class XRInputSpace : public XRSpace {
 public:
  explicit XRInputSpace(nsIGlobalObject* aParent, XRSession* aSession,
                        XRNativeOrigin* aNativeOrigin,
                        int32_t aControllerIndex);

  virtual bool IsPositionEmulated() const override;

 protected:
  virtual ~XRInputSpace() = default;

 private:
  int32_t mIndex;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRInputSpace_h_
