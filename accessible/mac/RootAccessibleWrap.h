/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture,
 * see http://lxr.mozilla.org/seamonkey/source/accessible/accessible-docs.html
 */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "RootAccessible.h"

namespace mozilla {

class PresShell;

namespace a11y {

class RootAccessibleWrap : public RootAccessible {
 public:
  RootAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);
  virtual ~RootAccessibleWrap();

  Class GetNativeType();

  // let's our native accessible get in touch with the
  // native cocoa view that is our accessible parent.
  void GetNativeWidget(void** aOutView);
};

}  // namespace a11y
}  // namespace mozilla

#endif
