/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLsemanticsFrame_h___
#define nsMathMLsemanticsFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLSelectedFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <semantics> -- associate annotations with a MathML expression
//

class nsMathMLsemanticsFrame final : public nsMathMLSelectedFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLsemanticsFrame)

  friend nsIFrame* NS_NewMathMLsemanticsFrame(mozilla::PresShell* aPresShell,
                                              ComputedStyle* aStyle);

 protected:
  explicit nsMathMLsemanticsFrame(ComputedStyle* aStyle,
                                  nsPresContext* aPresContext)
      : nsMathMLSelectedFrame(aStyle, aPresContext, kClassID) {}
  virtual ~nsMathMLsemanticsFrame();

  nsIFrame* GetSelectedFrame() override;
};

#endif /* nsMathMLsemanticsFrame_h___ */
