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
 * Contributor(s): 
 */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsDeckFrame_h___
#define nsDeckFrame_h___

#include "nsBoxFrame.h"

class nsDeckFrame : public nsBoxFrame
{
public:

  friend nsresult NS_NewDeckFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, nsIBoxLayout* aLayoutManager);

 

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);

  NS_IMETHOD DoLayout(nsBoxLayoutState& aState);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

 
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint,
                              nsFramePaintLayer aWhichLayer,    
                              nsIFrame**     aFrame);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);

  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                nsIContent*      aContent,
                nsIFrame*        aParent,
                nsIStyleContext* aContext,
                nsIFrame*        aPrevInFlow);

  NS_IMETHOD ChildrenMustHaveWidgets(PRBool& aMust);

#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const
  {
      return MakeFrameName("Deck", aResult);
  }
#endif

  nsDeckFrame(nsIPresShell* aPresShell, nsIBoxLayout* aLayout = nsnull);

protected:

  nsDeckFrame(nsIPresShell* aPresShell);

  virtual nsIBox* GetSelectedBox();
  virtual void IndexChanged(nsIPresContext* aPresContext);
  virtual PRInt32 GetSelectedIndex();
  virtual void HideBox(nsIPresContext* aPresContext, nsIBox* aBox);
  virtual void ShowBox(nsIPresContext* aPresContext, nsIBox* aBox);
  virtual nsresult CreateWidget(nsIPresContext* aPresContext, nsIBox* aBox);
  virtual nsresult CreateWidgets(nsIPresContext* aPresContext);

private:

  PRInt32 mIndex;

}; // class nsDeckFrame



#endif

