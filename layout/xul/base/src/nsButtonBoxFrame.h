/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsButtonBoxFrame_h___
#define nsButtonBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsButtonBoxFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewButtonBoxFrame(nsIPresShell* aPresShell);

  nsButtonBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
    :nsBoxFrame(aPresShell, aContext, false) {
    UpdateMouseThrough();
  }

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual void MouseClicked (nsPresContext* aPresContext, nsGUIEvent* aEvent)
  { DoMouseClick(aEvent, false); }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("ButtonBoxFrame"), aResult);
  }
#endif

  /**
   * Our implementation of MouseClicked. 
   * @param aTrustEvent if true and aEvent as null, then assume the event was trusted
   */
  void DoMouseClick(nsGUIEvent* aEvent, bool aTrustEvent);
  void UpdateMouseThrough() MOZ_OVERRIDE { AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER); }
}; // class nsButtonBoxFrame

#endif /* nsButtonBoxFrame_h___ */
