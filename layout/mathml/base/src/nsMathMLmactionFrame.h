/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMathMLmactionFrame_h___
#define nsMathMLmactionFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <maction> -- bind actions to a subexpression
//

//#define DEBUG_mouse 1

#if DEBUG_mouse
#define MOUSE(_msg) printf("maction:%p MOUSE: "#_msg" ...\n", this);
#else
#define MOUSE(_msg)
#endif

class nsMathMLmactionFrame : public nsMathMLContainerFrame,
                             public nsIDOMMouseListener {
public:
  friend nsresult NS_NewMathMLmactionFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Init(nsPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  GetFrameForPoint(nsPresContext*   aPresContext,
                   const nsPoint&    aPoint,
                   nsFramePaintLayer aWhichLayer,
                   nsIFrame**        aFrame);

  NS_IMETHOD
  Paint(nsPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

  NS_IMETHOD
  Place(nsPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  // nsIDOMMouseListener methods
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent)  { MOUSE(down) return NS_OK; }
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent) { MOUSE(up) return NS_OK; }
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);// { MOUSE(click) return NS_OK; }
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent) { MOUSE(dblclik) return NS_OK; }
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);// { MOUSE(over) return NS_OK; }
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);// { MOUSE(out) return NS_OK; }

  // nsIDOMEventListener methods
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)  { MOUSE(event); return NS_OK; }

protected:
  nsMathMLmactionFrame();
  virtual ~nsMathMLmactionFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nsPresContext* mPresContext;
  PRInt32         mActionType;
  PRInt32         mChildCount;
  PRInt32         mSelection;
  nsIFrame*       mSelectedFrame;
  nsString        mRestyle;
  PRBool          mWasRestyled;

  // helper to return the frame for the attribute selection="number"
  nsIFrame* 
  GetSelectedFrame();

  // helper to display a message on the status bar
  static nsresult
  ShowStatus(nsPresContext* aPresContext, 
             nsString&       aStatusMsg);
};

#endif /* nsMathMLmactionFrame_h___ */
