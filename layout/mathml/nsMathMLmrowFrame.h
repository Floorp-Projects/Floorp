/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmrowFrame_h___
#define nsMathMLmrowFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mrow> -- horizontally group any number of subexpressions 
//

class nsMathMLmrowFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmrowFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) MOZ_OVERRIDE;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) MOZ_OVERRIDE;

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE {
    return TransmitAutomaticDataForMrowLikeElement();
  }

protected:
  nsMathMLmrowFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmrowFrame();
};

#endif /* nsMathMLmrowFrame_h___ */
