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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Eric J. Burley (ericb@neoplanet.com)
 *
 * Contributor(s): 
 * based heavily on David Hyatt's work on nsButtonBoxFrame
 */
#ifndef nsResizerFrame_h___
#define nsResizerFrame_h___

#include "nsTitleBarFrame.h"

class nsResizerFrame : public nsTitleBarFrame 
{

protected:
  enum eDirection {
    topleft,
    top,
	 topright,
	 left,	 
	 right,
	 bottomleft,
	 bottom,
	 bottomright
  };
  

public:
  friend nsresult NS_NewResizerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);  

  nsResizerFrame(nsIPresShell* aPresShell);

  
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus);

  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        asPrevInFlow);
  
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);

  virtual void MouseClicked (nsIPresContext* aPresContext);

protected:
	PRBool GetInitialDirection(eDirection& aDirection);
	PRBool EvalDirection(nsAutoString& aText,eDirection& aResult);

protected:
	eDirection mDirection;
	nsRect mWidgetRect;



}; // class nsResizerFrame

#endif /* nsResizerFrame_h___ */
