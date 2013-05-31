/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLsemanticsFrame_h___
#define nsMathMLsemanticsFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLSelectedFrame.h"

//
// <semantics> -- associate annotations with a MathML expression
//

class nsMathMLsemanticsFrame : public nsMathMLSelectedFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLsemanticsFrame(nsIPresShell* aPresShell,
                                              nsStyleContext* aContext);

protected:
  nsMathMLsemanticsFrame(nsStyleContext* aContext) :
    nsMathMLSelectedFrame(aContext) {}
  virtual ~nsMathMLsemanticsFrame();

  nsIFrame* GetSelectedFrame() MOZ_OVERRIDE;
};

#endif /* nsMathMLsemanticsFrame_h___ */
