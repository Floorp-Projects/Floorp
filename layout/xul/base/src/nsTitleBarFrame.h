/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTitleBarFrame_h___
#define nsTitleBarFrame_h___

#include "nsBoxFrame.h"

class nsTitleBarFrame : public nsBoxFrame  
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);  

  nsTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus);

  virtual void MouseClicked(nsPresContext* aPresContext, nsGUIEvent* aEvent);

  void UpdateMouseThrough() { AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER); }

protected:
	bool mTrackingMouseMove;	
	nsIntPoint mLastPoint;

}; // class nsTitleBarFrame

#endif /* nsTitleBarFrame_h___ */
