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
// nsSplitterFrame
//

#ifndef nsSplitterFrame_h__
#define nsSplitterFrame_h__


#include "nsBoxFrame.h"
#include "nsIAnonymousContentCreator.h"

class nsISupportsArray;
class nsSplitterFrameInner;

nsresult NS_NewSplitterFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;

class nsSplitterFrame : public nsBoxFrame, public nsIAnonymousContentCreator
{
public:
  nsSplitterFrame(nsIPresShell* aPresShell);
  virtual ~nsSplitterFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("SplitterFrame", aResult);
  }
#endif


  // nsIFrame overrides
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                                    nsIContent*      aContent,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aContext,
                                    nsIFrame*        aPrevInFlow);

  NS_IMETHOD GetCursor(nsIPresContext* aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);

  // nsIAnonymousContentCreator
  NS_IMETHOD  CreateAnonymousContent(nsIPresContext* aPresContext,
                                     nsISupportsArray& aAnonymousItems);
  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame) { if (aFrame) *aFrame = nsnull; return NS_ERROR_FAILURE; }

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return NS_OK; }

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

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  virtual PRBool GetInitialOrientation(PRBool& aIsHorizontal); 

private:

  friend class nsSplitterFrameInner;
  nsSplitterFrameInner* mInner;
  // XXX Hack
  nsIPresContext* mPresContext;  // weak reference

}; // class nsSplitterFrame

#endif
