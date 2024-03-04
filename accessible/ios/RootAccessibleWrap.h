/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* For documentation of the accessibility architecture,
 * see https://firefox-source-docs.mozilla.org/accessible/index.html
 */

#ifndef mozilla_a11y_RootAccessibleWrap_h__
#define mozilla_a11y_RootAccessibleWrap_h__

#include "RootAccessible.h"

struct CGRect;

namespace mozilla {

class PresShell;

namespace a11y {

/**
 * iOS specific functionality for the node at a root of the accessibility
 * tree: see the RootAccessible superclass for further details.
 */
class RootAccessibleWrap : public RootAccessible {
 public:
  RootAccessibleWrap(dom::Document* aDocument, PresShell* aPresShell);
  virtual ~RootAccessibleWrap() = default;

  // Lets our native accessible get in touch with the
  // native cocoa view that is our accessible parent.
  void GetNativeWidget(void** aOutView);

  CGRect DevPixelsRectToUIKit(const LayoutDeviceIntRect& aRect);
};

}  // namespace a11y
}  // namespace mozilla

#endif
