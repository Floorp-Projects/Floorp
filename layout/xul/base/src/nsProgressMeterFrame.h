/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
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
#include "nsCOMPtr.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"

#include "prtypes.h"

class nsIPresContext;
class nsIStyleContext;

class nsProgressMeterFrame : public nsLeafFrame
{
public:
  friend nsresult NS_NewProgressMeterFrame(nsIFrame*& aNewFrame);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  
    // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute,
                              PRInt32 aHint);

  NS_IMETHOD  ReResolveStyleContext ( nsIPresContext* aPresContext, 
                                      nsIStyleContext* aParentContext,
                                      PRInt32 aParentChange,
                                      nsStyleChangeList* aChangeList,
                                      PRInt32* aLocalChange) ;


  virtual void animate();

  virtual void Reflow(nsIPresContext* aPresContext);

  virtual void Redraw(nsIPresContext* aPresContext);

protected:
  nsProgressMeterFrame();
  virtual ~nsProgressMeterFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  virtual void CalcSize(nsIPresContext& aPresContext, int& width, int& height);

  virtual void  PaintBar ( nsIPresContext& aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& rect, 
							float progress,
							nscolor color);


  virtual void PaintBarStripped(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext, 
                                           const nsRect& rect, nscolor color);

  virtual void PaintBarSolid(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext, 
                                           const nsRect& rect, nscolor color, float skew);


  virtual nscolor BrightenBy(nscolor color, PRUint8 amount);
  virtual PRUint8 GetBrightness(nscolor c);
  virtual nsRect TransformXtoY(const nsRect& rect);
  virtual nsRect TransformYtoX(const nsRect& rect);

  virtual void RefreshStyleContext(nsIPresContext* aPresContext,
                            nsIAtom *         aNewContentPseudo,
                            nsCOMPtr<nsIStyleContext>* aCurrentStyle,
                            nsIContent *      aContent,
                            nsIStyleContext*  aParentStyle) ;


    // pass-by-value not allowed for a coordinator because it corresponds 1-to-1
    // with an element in the UI.
  nsProgressMeterFrame ( const nsProgressMeterFrame& aFrame ) ;	            // DO NOT IMPLEMENT
  nsProgressMeterFrame& operator= ( const nsProgressMeterFrame& aFrame ) ;  // DO NOT IMPLEMENT

private:
 
  void setProgress(nsAutoString progress);
  void setAlignment(nsAutoString alignment);
  void setMode(nsAutoString mode);
  void setSize(nsAutoString s, int& size, PRBool& isPercent);

  nsCOMPtr<nsIStyleContext>    mBarStyle;
  float   mProgress;
  PRBool  mHorizontal;
  PRBool  mUndetermined;
  int     mStripeOffset;
}; // class nsProgressMeterFrame
