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
// nsPolygonFrame
//

#ifndef nsPolygonFrame_h__
#define nsPolygonFrame_h__


#include "nsLeafFrame.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsISVGFrame.h"

class nsString;
class nsIPresContext;

nsresult NS_NewPolygonFrame(nsIPresShell* aPresShell, nsIFrame** aResult) ;


// XXX - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// This should NOT be derived from nsLeafFrame
// we really want to create our own container class from the nsIFrame
// interface and not derive from any HTML Frames
// XXX - !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
class nsPolygonFrame : public nsLeafFrame, public nsISVGFrame
{
public:
  nsPolygonFrame();
  virtual ~nsPolygonFrame();

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsIPresContext*          aCX,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
   // nsISVGFrame
  NS_IMETHOD GetXY(nscoord* aX, nscoord* aY) { *aX = mX; *aY = mY; return NS_OK; }

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("PolygonFrame", aResult);
  }
#endif

  // nsIFrame overrides
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  nsresult HandleMouseDownEvent(nsIPresContext* aPresContext,
                                nsGUIEvent*     aEvent,
                                nsEventStatus*  aEventStatus);

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  NS_IMETHOD RenderPoints(nsIRenderingContext& aRenderingContext,
                          const nsPoint aPoints[], PRInt32 aNumPoints);
  
  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType, 
                              PRInt32         aHint);
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue);

protected:
  NS_IMETHOD GetPoints();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize) ;


  nsVoidArray mPoints;

  nsPoint *  mPnts;
  PRInt32    mNumPnts;
  nscoord    mX;
  nscoord    mY;

  nsIPresContext* mPresContext;  // XXX: Remove the need to cache the pres context.

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
}; // class nsPolygonFrame

#endif
