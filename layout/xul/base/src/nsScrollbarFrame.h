/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// nsScrollbarFrame
//

#ifndef nsScrollbarFrame_h__
#define nsScrollbarFrame_h__


#include "nsBoxFrame.h"
#include "nsIScrollbarFrame.h"

class nsISupportsArray;
class nsIScrollbarMediator;

nsresult NS_NewScrollbarFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;

class nsScrollbarFrame : public nsBoxFrame, public nsIScrollbarFrame
{
public:
    nsScrollbarFrame(nsIPresShell* aShell):nsBoxFrame(aShell), mScrollbarMediator(nsnull) {}

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ScrollbarFrame", aResult);
  }
#endif

  // nsIFrame overrides
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

   NS_IMETHOD HandlePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsIPresContext* aPresContext,
                         nsGUIEvent *    aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleDrag(nsIPresContext* aPresContext,
                        nsGUIEvent *    aEvent,
                        nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleRelease(nsIPresContext* aPresContext,
                           nsGUIEvent *    aEvent,
                           nsEventStatus*  aEventStatus);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow);


  // nsIScrollbarFrame
  NS_IMETHOD SetScrollbarMediator(nsIScrollbarMediator* aMediator) { mScrollbarMediator = aMediator; return NS_OK; };
  NS_IMETHOD GetScrollbarMediator(nsIScrollbarMediator** aResult) { *aResult = mScrollbarMediator; return NS_OK; };

private:
  nsIScrollbarMediator* mScrollbarMediator;
}; // class nsScrollbarFrame

#endif
