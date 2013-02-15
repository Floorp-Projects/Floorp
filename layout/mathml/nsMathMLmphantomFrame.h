/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmphantomFrame_h___
#define nsMathMLmphantomFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mphantom> -- make content invisible but preserve its size
//

class nsMathMLmphantomFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmphantomFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent);

  NS_IMETHOD
  TransmitAutomaticData() {
    return TransmitAutomaticDataForMrowLikeElement();
  }

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) MOZ_OVERRIDE { return NS_OK; }

protected:
  nsMathMLmphantomFrame(nsStyleContext* aContext)
    : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmphantomFrame();
};

#endif /* nsMathMLmphantomFrame_h___ */
