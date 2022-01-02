/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmmultiscriptsFrame_h___
#define nsMathMLmmultiscriptsFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base
// <msub> -- attach a subscript to a base
// <msubsup> -- attach a subscript-superscript pair to a base
// <msup> -- attach a superscript to a base
//

class nsMathMLmmultiscriptsFrame final : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmmultiscriptsFrame)

  friend nsIFrame* NS_NewMathMLmmultiscriptsFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

  NS_IMETHOD
  TransmitAutomaticData() override;

  virtual nsresult Place(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                         ReflowOutput& aDesiredSize) override;

  static nsresult PlaceMultiScript(nsPresContext* aPresContext,
                                   DrawTarget* aDrawTarget, bool aPlaceOrigin,
                                   ReflowOutput& aDesiredSize,
                                   nsMathMLContainerFrame* aForFrame,
                                   nscoord aUserSubScriptShift,
                                   nscoord aUserSupScriptShift,
                                   float aFontSizeInflation);

  uint8_t ScriptIncrement(nsIFrame* aFrame) override;

 protected:
  explicit nsMathMLmmultiscriptsFrame(ComputedStyle* aStyle,
                                      nsPresContext* aPresContext)
      : nsMathMLContainerFrame(aStyle, aPresContext, kClassID) {}
  virtual ~nsMathMLmmultiscriptsFrame();
};

#endif /* nsMathMLmmultiscriptsFrame_h___ */
