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

//
// a helper frame class to wrap non-MathML frames so that foreign elements 
// (e.g., html:img) can mix better with other surrounding MathML markups
//

#ifndef nsMathMLForeignFrameWrapper_h___
#define nsMathMLForeignFrameWrapper_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

class nsMathMLForeignFrameWrapper : public nsBlockFrame,
                                    public nsMathMLFrame {
public:
  friend nsresult NS_NewMathMLForeignFrameWrapper(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS_INHERITED

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate)
  {
    nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(aPresContext, this,
      aFirstIndex, aLastIndex, aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext* aPresContext,
                       PRInt32         aParentScriptLevel)
  {
    nsMathMLContainerFrame::PropagateScriptStyleFor(aPresContext, this, aParentScriptLevel);
    return NS_OK;
  }

  // overloaded nsBlockFrame methods

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

#ifdef NS_DEBUG
  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv = nsBlockFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    // cannot use mFrames{.FirstChild()|.etc} since the block code doesn't set mFrames
    nsFrameList frameList(aChildList);
    NS_ASSERTION(frameList.FirstChild() && frameList.GetLength() == 1,
                 "there must be one and only one child frame");
    return rv;
  }
#endif

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  // we are just a wrapper and these methods shouldn't be called
  NS_IMETHOD
  AppendFrames(nsIPresContext* aPresContext,
               nsIPresShell&   aPresShell,
               nsIAtom*        aListName,
               nsIFrame*       aFrameList)
  {
    NS_NOTREACHED("unsupported operation");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  InsertFrames(nsIPresContext* aPresContext,
               nsIPresShell&   aPresShell,
               nsIAtom*        aListName,
               nsIFrame*       aPrevFrame,
               nsIFrame*       aFrameList)
  {
    NS_NOTREACHED("unsupported operation");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // need special care here because the base class implementation treats this
  // as two operations: remove & insert; In our case, removing the child will
  // remove us too... so we have to operate from our parent's perspective
  NS_IMETHOD
  ReplaceFrame(nsIPresContext* aPresContext,
               nsIPresShell&   aPresShell,
               nsIAtom*        aListName,
               nsIFrame*       aOldFrame,
               nsIFrame*       aNewFrame)
  {
    nsresult rv = mParent->ReplaceFrame(aPresContext, aPresShell, aListName, this, aNewFrame);
    // XXX the usage of ReplaceFrame() vs. ReplaceFrameAndDestroy() is
    // XXX ambiguous - see bug 122748. The style system doesn't call ReplaceFrame()
    // XXX and that's why nobody seems to have been biten by the ambiguity yet
    aOldFrame->Destroy(aPresContext);
    return rv;
  }

  // Our life is bound to the life of our unique child.
  // When our child goes away, we ask our parent to delete us
  NS_IMETHOD
  RemoveFrame(nsIPresContext* aPresContext,
              nsIPresShell&   aPresShell,
              nsIAtom*        aListName,
              nsIFrame*       aOldFrame)
  {
    return mParent->RemoveFrame(aPresContext, aPresShell, aListName, this);
  }

protected:
  nsMathMLForeignFrameWrapper() {}
  virtual ~nsMathMLForeignFrameWrapper() {}
};

#endif /* nsMathMLForeignFrameWrapper_h___ */
