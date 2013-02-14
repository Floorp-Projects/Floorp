/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a Stack of cards.
 
**/

#ifndef nsStackFrame_h___
#define nsStackFrame_h___

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsStackFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewStackFrame(nsIPresShell* aPresShell,
                                    nsStyleContext* aContext);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE
  {
    return MakeFrameName(NS_LITERAL_STRING("Stack"), aResult);
  }
#endif

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) MOZ_OVERRIDE;

protected:
  nsStackFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
}; // class nsStackFrame



#endif

