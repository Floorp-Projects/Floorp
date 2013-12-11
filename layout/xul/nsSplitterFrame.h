/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsSplitterFrame
//

#ifndef nsSplitterFrame_h__
#define nsSplitterFrame_h__


#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsSplitterFrameInner;

nsIFrame* NS_NewSplitterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsSplitterFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsSplitterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE {
    return MakeFrameName(NS_LITERAL_STRING("SplitterFrame"), aResult);
  }
#endif

  // nsIFrame overrides
  NS_IMETHOD AttributeChanged(int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType) MOZ_OVERRIDE;

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;

  NS_IMETHOD GetCursor(const nsPoint&    aPoint,
                       nsIFrame::Cursor& aCursor) MOZ_OVERRIDE;

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) MOZ_OVERRIDE;

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void GetInitialOrientation(bool& aIsHorizontal) MOZ_OVERRIDE; 

private:

  friend class nsSplitterFrameInner;
  nsSplitterFrameInner* mInner;

}; // class nsSplitterFrame

#endif
