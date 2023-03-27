/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A simple frame class for XUL frames that are leafs on the tree but need
// background / border painting, and for some reason or another need special
// code (like event handling code) which we haven't ported to the DOM.
//
// This should generally not be used for new frame classes.

#ifndef mozilla_SimpleXULLeafFrame_h
#define mozilla_SimpleXULLeafFrame_h

#include "nsLeafFrame.h"

namespace mozilla {

// Shared class for thumb and scrollbar buttons.
class SimpleXULLeafFrame : public nsLeafFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(SimpleXULLeafFrame)

  // TODO: Look at appearance instead maybe?
  nscoord GetIntrinsicISize() override { return 0; }
  nscoord GetIntrinsicBSize() override { return 0; }

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;
  explicit SimpleXULLeafFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext, ClassID aClassID)
      : nsLeafFrame(aStyle, aPresContext, aClassID) {}

  explicit SimpleXULLeafFrame(ComputedStyle* aStyle,
                              nsPresContext* aPresContext)
      : SimpleXULLeafFrame(aStyle, aPresContext, kClassID) {}

  friend nsIFrame* NS_NewSimpleXULLeafFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);
};

}  // namespace mozilla

#endif
