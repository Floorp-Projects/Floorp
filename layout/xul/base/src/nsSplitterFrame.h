/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//
// nsSplitterFrame
//

#ifndef nsSplitterFrame_h__
#define nsSplitterFrame_h__


#include "nsBoxFrame.h"
#include "nsIAnonymousContentCreator.h"

class nsISupportsArray;
class nsSplitterFrameImpl;

nsresult NS_NewSplitterFrame(nsIFrame** aResult) ;

class nsSplitterFrame : public nsBoxFrame, public nsIAnonymousContentCreator
{
public:
  nsSplitterFrame();
  ~nsSplitterFrame();

    // nsIFrame overrides
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("SplitterFrame", aResult);
  }

 
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD  Init(nsIPresContext&  aPresContext,
                                    nsIContent*      aContent,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aContext,
                                    nsIFrame*        aPrevInFlow);

  // nsIAnonymousContentCreator
  NS_IMETHOD  CreateAnonymousContent(nsISupportsArray& aAnonymousItems);
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return NS_OK; }

   NS_IMETHOD HandlePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsIPresContext& aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleDrag(nsIPresContext& aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleRelease(nsIPresContext& aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus&  aEventStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);


private:

  friend class nsSplitterFrameImpl;
  nsSplitterFrameImpl* mImpl;

}; // class nsSplitterFrame

#endif
