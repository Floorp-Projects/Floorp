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
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsITextContent.h"

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLChar.h"
#include "nsMathMLContainerFrame.h"

//
// nsMathMLContainerFrame implementation
//

// TODO: Proper management of ignorable whitespace 
//       and handling of non-math related frames.
// * Should math markups only enclose other math markups?
//   Why bother... we can leave them in, as hinted in the newsgroup.
//
// * Space doesn't count. Handling space is more complicated than it seems. 
//   We have to clean inside and outside:
//       <mi> a </mi> <mo> + </mo> <mi> b </mi>
//           ^ ^     ^    ^ ^     ^    ^ ^
//   *Outside* currently handled using IsOnlyWhitespace().
//   For whitespace only frames, their rect is set zero.
//
//   *Inside* not currently handled... so the user must do <mi>a</mi>
//   What to do?!
//   - Add a nsTextFrame::CompressWhitespace() *if* it is MathML content?!
//   - via CSS property? Support for "none/compress"? (white-space: normal | pre | nowrap)


// nsISupports
// =============================================================================

NS_IMETHODIMP
nsMathMLContainerFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  *aInstancePtr = nsnull;
  if (aIID.Equals(kIMathMLFrameIID)) {
    *aInstancePtr = (void*)(nsIMathMLFrame*)this;
    NS_ADDREF_THIS(); // not effectual, frames are not Refcounted 
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLContainerFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsMathMLContainerFrame::Release(void)
{
  return NS_OK;
}

/* ///////////////
 * MathML specific - Whitespace management ...
 * WHITESPACE: don't forget that whitespace doesn't count in MathML!
 * empty frames shouldn't be created in MathML, oh well that's another story.
 * =============================================================================
 */

PRBool
nsMathMLContainerFrame::IsOnlyWhitespace(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null arg");
  PRBool rv = PR_FALSE;

  // by empty frame we mean a leaf frame whose text content is empty...
  nsCOMPtr<nsIContent> aContent;
  aFrame->GetContent(getter_AddRefs(aContent));
  if (nsnull == aContent) 
    return PR_TRUE;
  PRInt32 numKids;
  aContent->ChildCount(numKids);
  if (0 == numKids) {
    nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
    if (tc.get()) tc->IsOnlyWhitespace(&rv);
  }
  return rv;
}

void
nsMathMLContainerFrame::ReflowEmptyChild(nsIPresContext& aPresContext,
                                         nsIFrame*       aFrame)
{
//  nsHTMLReflowMetrics emptySize(nsnull);
//  nsHTMLReflowState emptyReflowState(aPresContext, aReflowState, aFrame, nsSize(0,0));
//  nsresult rv = ReflowChild(aFrame, aPresContext, emptySize, emptyReflowState, aStatus);
 
  // 0-size the frame
  aFrame->SetRect(&aPresContext, nsRect(0,0,0,0));

  // 0-size the view, if any
  nsIView* view = nsnull;
  aFrame->GetView(&aPresContext, &view);
  if (view) {
    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));
    vm->ResizeView(view, 0,0);
  }
}

/* /////////////
 * nsIMathMLFrame - support methods for stretchy elements and scripting 
 * elements (nested frames within msub, msup, msubsup, munder, mover, 
 * munderover, mmultiscripts, mfrac, mroot, mtable).
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::Stretch(nsIPresContext&    aPresContext,
                                nsStretchDirection aStretchDirection,
                                nsCharMetrics&     aContainerSize,
                                nsCharMetrics&     aDesiredStretchSize)
{
  return NS_OK; // the Stretch() is only implemented by <mo> and its nsMathMLChar
}

NS_IMETHODIMP
nsMathMLContainerFrame::GetPresentationData(PRInt32* aScriptLevel, 
                                            PRBool*  aDisplayStyle)
{
  NS_PRECONDITION(aScriptLevel && aDisplayStyle, "null arg");
  *aScriptLevel = mScriptLevel;
  *aDisplayStyle = mDisplayStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::UpdatePresentationData(PRInt32 aScriptLevelIncrement, 
                                               PRBool  aDisplayStyle)
{
  mScriptLevel += aScriptLevelIncrement;
  mDisplayStyle = aDisplayStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::UpdatePresentationDataFromChildAt(PRInt32 aIndex, 
                                                          PRInt32 aScriptLevelIncrement,
                                                          PRBool  aDisplayStyle)
{
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      if (0 >= aIndex--) {
     	nsIMathMLFrame* aMathMLFrame = nsnull;
        nsresult rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
        if (NS_SUCCEEDED(rv) && nsnull != aMathMLFrame) {
          // update
      	  aMathMLFrame->UpdatePresentationData(aScriptLevelIncrement, aDisplayStyle);
          // propagate down the subtrees
          aMathMLFrame->UpdatePresentationDataFromChildAt(0, aScriptLevelIncrement, aDisplayStyle);
      	}
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}

// helper method to alter the style context
// This method is used for switching the font to a subscript/superscript font in
// mfrac, msub, msup, msubsup, munder, mover, munderover, mmultiscripts 

NS_IMETHODIMP
nsMathMLContainerFrame::InsertScriptLevelStyleContext(nsIPresContext& aPresContext)
{
  nsresult rv = NS_OK;
  nsIFrame* nextFrame = mFrames.FirstChild();
  while (nsnull != nextFrame && NS_SUCCEEDED(rv)) { 	
    nsIFrame* childFrame = nextFrame;
    rv = nextFrame->GetNextSibling(&nextFrame);
    if (!IsOnlyWhitespace(childFrame) && NS_SUCCEEDED(rv)) {

      // see if the child frame implements the nsIMathMLFrame interface
      nsIMathMLFrame* aMathMLFrame;
      rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
      if (nsnull != aMathMLFrame && NS_SUCCEEDED(rv)) {

        // get the scriptlevel of the child
        PRInt32 childLevel;
        PRBool childDisplayStyle;
        aMathMLFrame->GetPresentationData(&childLevel, &childDisplayStyle);

        // Iteration to set a style context for the script level font.
        // Wow, here is what is happening: the style system requires that any style context
        // *must* be uniquely associated to a frame. So we insert as many frames as needed
        // to scale-down (or scale-up) the fontsize.

        PRInt32 gap = childLevel - mScriptLevel;
        if (0 != gap) {
          nsCOMPtr<nsIContent> childContent;
          childFrame->GetContent(getter_AddRefs(childContent));

          nsIFrame* firstFrame = nsnull;
          nsIFrame* lastFrame = this;
          nsIStyleContext* lastStyleContext = mStyleContext;
          nsCOMPtr<nsIStyleContext> newStyleContext;

          nsAutoString fontSize = (0 < gap)
                                ? ":-moz-math-font-size-smaller"
                                : ":-moz-math-font-size-larger";
          nsCOMPtr<nsIAtom> fontAtom(getter_AddRefs(NS_NewAtom(fontSize)));
          if (0 > gap) gap = -gap; // absolute value
          while (0 < gap--) {

            aPresContext.ResolvePseudoStyleContextFor(childContent, fontAtom, lastStyleContext,
                                                      PR_FALSE, getter_AddRefs(newStyleContext));          
            if (newStyleContext && newStyleContext.get() != lastStyleContext) {
              // create a new frame and append it as sole child of the last created frame
              nsIFrame* newFrame = nsnull;
              NS_NewMathMLContainerFrame(&newFrame);
              NS_ASSERTION(newFrame, "Failed to create new frame");

              newFrame->Init(aPresContext, childContent, lastFrame, newStyleContext, nsnull);

              if (nsnull == firstFrame) {
                firstFrame = newFrame; 
              }
              if (this != lastFrame) {
                lastFrame->SetInitialChildList(aPresContext, nsnull, newFrame);
              }
              lastStyleContext = newStyleContext;
              lastFrame = newFrame;    
            }
            else {
              break;
            }
          }
          if (nsnull != firstFrame) { // at least one new frame was created
            mFrames.ReplaceFrame(this, childFrame, firstFrame);
            childFrame->SetParent(lastFrame);
            childFrame->SetNextSibling(nsnull);
            aPresContext.ReParentStyleContext(childFrame, lastStyleContext);
            lastFrame->SetInitialChildList(aPresContext, nsnull, childFrame);
          }
        }
      }
    }
  }
  return rv;
}


/* //////////////////
 * Frame construction
 * =============================================================================
 */
 
NS_IMETHODIMP
nsMathMLContainerFrame::Init(nsIPresContext&  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsIStyleContext* aContext,
                             nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  // first, let the base class do its Init()
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  
  // now, if our parent implements the nsIMathMLFrame interface, we inherit
  // its scriptlevel and displaystyle. If the parent later wishes to increment
  // with other values, it will do so in its SetInitialChildList() method.
 
  mScriptLevel = 0;
  mDisplayStyle = PR_TRUE;
 
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsresult res = aParent->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
  if (NS_SUCCEEDED(res) && nsnull != aMathMLFrame) {
    PRInt32 aScriptLevel = 0; 
    PRBool aDisplayStyle = PR_TRUE;
    aMathMLFrame->GetPresentationData(&aScriptLevel, &aDisplayStyle);
    UpdatePresentationData(aScriptLevel, aDisplayStyle);
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                            nsIAtom*        aListName,
                                            nsIFrame*       aChildList)
{
  // First, let the base class do its job
  nsresult rv;
  rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // Next, since we are an inline frame, and since we are a container, we have to
  // be very careful with the way we treat our children. Things look okay when
  // all of our children are only MathML frames. But there are problems if one of
  // our children happens to be an nsInlineFrame, e.g., from generated content such
  // as :before { content: open-quote } or :after { content: close-quote }
  // The code asserts during reflow (in nsLineLayout::BeginSpan)
  // Also there are problems when our children are hybrid, e.g., from html markups.
  // In short, the nsInlineFrame class expects a number of *invariants* that are not
  // met when we mix things.

  // So what we do here is to wrap children that happen to be nsInlineFrames in
  // anonymous block frames.
  // XXX Question: Do we have to handle Insert/Remove/Append on behalf of
  //     these anonymous blocks?
  //     Note: By construction, our anonymous blocks have only one child.

  nsIFrame* next = mFrames.FirstChild();
  while (next) {
    nsIFrame* child = next;
    rv = next->GetNextSibling(&next);
    if (!IsOnlyWhitespace(child)) {
      nsInlineFrame* inlineFrame = nsnull;
      rv = child->QueryInterface(nsInlineFrame::kInlineFrameCID, (void**)&inlineFrame);  
      if (inlineFrame) {
        // create a new anonymous block frame to wrap this child...
        nsIFrame* anonymous;
        rv = NS_NewBlockFrame(&anonymous);
        if (NS_FAILED(rv))
          return rv;    
        nsCOMPtr<nsIStyleContext> newStyleContext;
        aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::mozAnonymousBlock, 
                                                  mStyleContext, PR_FALSE, 
                                                  getter_AddRefs(newStyleContext));
        rv = anonymous->Init(aPresContext, mContent, this, newStyleContext, nsnull);
        if (NS_FAILED(rv)) {
          anonymous->Destroy(aPresContext);
          delete anonymous;
          return rv;
        }
        mFrames.ReplaceFrame(this, child, anonymous);
        child->SetParent(anonymous);
        child->SetNextSibling(nsnull);
        aPresContext.ReParentStyleContext(child, newStyleContext);
        anonymous->SetInitialChildList(aPresContext, nsnull, child);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Reflow(nsIPresContext&          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  nsReflowStatus childStatus;
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  nsRect rect;

  aStatus = NS_FRAME_NOT_COMPLETE;
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;

  //////////////////
  // Reflow Children  
  
  nsIFrame* childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
  	
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState, childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }

      // At this stage, the origin points of the children have no use, so we will use the
      // origins as placeholders to store the child's ascent and descent. Before return,
      // we should set the origins so as to overwrite what we are storing there now.
      childFrame->SetRect(&aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                          childDesiredSize.width, childDesiredSize.height));

      aDesiredSize.width += childDesiredSize.width;
      if (aDesiredSize.ascent < childDesiredSize.ascent) {
        aDesiredSize.ascent = childDesiredSize.ascent;
      }
      if (aDesiredSize.descent < childDesiredSize.descent) {
        aDesiredSize.descent = childDesiredSize.descent;
      }
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  /////////////
  // Ask stretchy children to stretch themselves
  
  nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;
  nsCharMetrics parentSize(aDesiredSize);
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
    
  childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
  	
    // retrieve the metrics that was stored at the previous pass
    childFrame->GetRect(rect);
    nsCharMetrics childSize(rect.x, rect.y, rect.width, rect.height);

    //////////
    // Stretch ...
    // Only directed at frames that implement the nsIMathMLFrame interface
    nsIMathMLFrame* aMathMLFrame;
    rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && nsnull != aMathMLFrame) {
      aMathMLFrame->Stretch(aPresContext, stretchDir, parentSize, childSize);
      // store the updated metrics
      childFrame->SetRect(&aPresContext,
                          nsRect(childSize.descent, childSize.ascent,
                                 childSize.width, childSize.height));
    }
 
    aDesiredSize.width += childSize.width;
    if (aDesiredSize.ascent < childSize.ascent) {
      aDesiredSize.ascent = childSize.ascent;
    }
    if (aDesiredSize.descent < childSize.descent) {
      aDesiredSize.descent = childSize.descent;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  /////////////
  // Place Children now by re-adjusting the origins to align the baselines

  nsPoint offset(0,0);
  childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
    childFrame->GetRect(rect);
    offset.y = aDesiredSize.ascent - rect.y;
    childFrame->MoveTo(&aPresContext, offset.x,offset.y);
    offset.x += rect.width;
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}
