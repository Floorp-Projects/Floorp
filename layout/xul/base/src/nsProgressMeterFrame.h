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

  Eric D Vaughan.

  A simple progress meter. 

  Attributes:

  value: A number between 0% adn 100%
  align: horizontal, or vertical
  mode: determined, undetermined (one shows progress other shows animated candy cane)

  Style:

  Bar gets its color from the color style
  Alternating stripes can be set with the seudo style:

	:PROGRESSMETER-STRIPE {
		color: gray
	}

**/

#include "nsLeafFrame.h"
#include "nsColor.h"

#include "prtypes.h"
#include "nsIBox.h"

class nsIPresContext;
class nsIStyleContext;

class nsProgressMeterFrame : public nsLeafFrame, nsIBox
{
public:
  friend nsresult NS_NewProgressMeterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release(void) { return NS_OK; }
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr); 
  NS_IMETHOD GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize);

  NS_IMETHOD InvalidateCache(nsIFrame* aChild);

  NS_IMETHOD SetDebug(nsIPresContext* aPresContext, PRBool aDebug) { return NS_OK; }


    // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD  Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);


  virtual void Reflow(nsIPresContext* aPresContext);

  virtual void Redraw(nsIPresContext* aPresContext);

protected:
  nsProgressMeterFrame();
  virtual ~nsProgressMeterFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  virtual void CalcSize(nsIPresContext* aPresContext, int& width, int& height);

  virtual void  PaintBar ( nsIPresContext* aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& rect, 
							float progress,
							nscolor color);


    // pass-by-value not allowed for a coordinator because it corresponds 1-to-1
    // with an element in the UI.
  nsProgressMeterFrame ( const nsProgressMeterFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsProgressMeterFrame& operator= ( const nsProgressMeterFrame& aFrame ) ;  // DO NOT IMPLEMENT

private:
 
  void setProgress(nsAutoString progress);
  void setAlignment(nsAutoString alignment);
  void setMode(nsAutoString mode);

  float   mProgress;
  PRBool  mHorizontal;
  PRBool  mUndetermined;
}; // class nsProgressMeterFrame
