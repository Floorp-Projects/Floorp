/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 */

#ifndef nsMathMLmactionFrame_h___
#define nsMathMLmactionFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"
#include "nslog.h"

NS_IMPL_LOG(nsMathMLmactionFrameLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLmactionFrameLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLmactionFrameLog)

//
// <maction> -- bind actions to a subexpression
//

//#define DEBUG_mouse 1

#if DEBUG_mouse
#define MOUSE(_msg) PRINTF("maction:%p MOUSE: "#_msg" ...\n", this);
#else
#define MOUSE(_msg)
#endif

class nsMathMLmactionFrame : public nsMathMLContainerFrame,
                             public nsIDOMMouseListener {
public:
  friend nsresult NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  GetFrameForPoint(nsIPresContext*   aPresContext,
                   const nsPoint&    aPoint,
                   nsFramePaintLayer aWhichLayer,
                   nsIFrame**        aFrame);

  NS_IMETHOD
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  // nsIDOMMouseListener methods
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent)  { MOUSE(down) return NS_OK; }
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent) { MOUSE(up) return NS_OK; }
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);// { MOUSE(click) return NS_OK; }
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { MOUSE(dblclik) return NS_OK; }
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);// { MOUSE(over) return NS_OK; }
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);// { MOUSE(out) return NS_OK; }

  // nsIDOMEventListener methods
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent)  { MOUSE(event); return NS_OK; }

protected:
  nsMathMLmactionFrame();
  virtual ~nsMathMLmactionFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nsIPresContext* mPresContext;
  PRInt32         mActionType;
  PRInt32         mChildCount;
  PRInt32         mSelection;
  nsIFrame*       mSelectedFrame;
  nsString        mRestyle;

  // helper to return the frame for the attribute selection="number"
  nsIFrame* 
  GetSelectedFrame();

  // helper to display a message on the status bar
  static nsresult
  ShowStatus(nsIPresContext* aPresContext, 
             nsString&       aStatusMsg);
};

#endif /* nsMathMLmactionFrame_h___ */
