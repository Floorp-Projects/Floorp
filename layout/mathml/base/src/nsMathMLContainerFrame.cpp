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
#include "nsIPresShell.h"
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
  if (!aContent) return PR_TRUE;
  PRInt32 numKids;
  aContent->ChildCount(numKids);
  if (0 == numKids) {
    nsCOMPtr<nsITextContent> tc(do_QueryInterface(aContent));
    if (tc.get()) tc->IsOnlyWhitespace(&rv);
  }
  return rv;
}

void
nsMathMLContainerFrame::ReflowEmptyChild(nsIPresContext* aPresContext,
                                         nsIFrame*       aFrame)
{
//  nsHTMLReflowMetrics emptySize(nsnull);
//  nsHTMLReflowState emptyReflowState(aPresContext, aReflowState, aFrame, nsSize(0,0));
//  nsresult rv = ReflowChild(aFrame, aPresContext, emptySize, emptyReflowState, aStatus);
 
  // 0-size the frame
  aFrame->SetRect(aPresContext, nsRect(0,0,0,0));

  // 0-size the view, if any
  nsIView* view = nsnull;
  aFrame->GetView(aPresContext, &view);
  if (view) {
    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));
    vm->ResizeView(view, 0,0);
  }
}

/* /////////////
 * nsIMathMLFrame - support methods for precise positioning 
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics)
{
  mBoundingMetrics = aBoundingMetrics;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::GetReference(nsPoint& aReference)
{
  aReference = mReference;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetReference(const nsPoint& aReference)
{
  mReference = aReference;
  return NS_OK;
}


/* /////////////
 * nsIMathMLFrame - support methods for stretchy elements
 * =============================================================================
 */

NS_IMETHODIMP
nsMathMLContainerFrame::Stretch(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsStretchDirection   aStretchDirection,
                                nsStretchMetrics&    aContainerSize,
                                nsStretchMetrics&    aDesiredStretchSize)
{
  nsresult rv = NS_OK;
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {

    if (NS_MATHML_STRETCH_WAS_DONE(mEmbellishData.flags)) {
      printf("WARNING *** it is wrong to fire stretch more than once on a frame...\n");
//      NS_ASSERTION(PR_FALSE,"Stretch() was fired more than once on a frame!");
      return NS_OK;
    }
    mEmbellishData.flags |= NS_MATHML_STRETCH_DONE;


    // Pass the stretch to the first non-empty child ...

    nsIFrame* childFrame = mEmbellishData.firstChild;
    NS_ASSERTION(childFrame, "Something is wrong somewhere");

    if (childFrame) {
      nsIMathMLFrame* aMathMLFrame = nsnull;
      rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Something is wrong somewhere");
      if (NS_SUCCEEDED(rv) && aMathMLFrame) {
        nsHTMLReflowMetrics aReflowMetrics(nsnull);

        nsRect rect;
        childFrame->GetRect(rect);
        // And the trick is that rect.x is still holding the descent, and rect.y 
        // is still holding the ascent ...
        nsStretchMetrics childSize(rect.x, rect.y, rect.width, rect.height);
        nsStretchMetrics container(aContainerSize);

        if (aStretchDirection != NS_STRETCH_DIRECTION_DEFAULT && aStretchDirection != mEmbellishData.direction) {
          // change the direction and confine the stretch to us
#if 0
          GetDesiredStretchSize(aPresContext, aRenderingContext, container);
#endif
          GetRect(rect);
          container = nsStretchMetrics(rect.x, rect.y, rect.width, rect.height);
        }

        aMathMLFrame->Stretch(aPresContext, aRenderingContext,
                              mEmbellishData.direction, container, childSize);
        childFrame->SetRect(aPresContext,
                            nsRect(childSize.descent, childSize.ascent, 
                                   childSize.width, childSize.height));

        // We now have one child that may have changed, re-position all our children
        Place(aPresContext, aRenderingContext, PR_TRUE, aReflowMetrics);

        // Prepare the metrics to be returned
        aDesiredStretchSize = nsStretchMetrics(aReflowMetrics);
        aDesiredStretchSize.leftSpace = childSize.leftSpace;
        aDesiredStretchSize.rightSpace = childSize.rightSpace;

        // If our parent is not embellished, it means we are the outermost embellished
        // container and so we put the spacing, otherwise we don't include the spacing,
        // the outermost embellished container will take care of it.

        if (!IsEmbellishOperator(mParent)) {
          nsStyleFont font;
          mStyleContext->GetStyle(eStyleStruct_Font, font);
          nscoord em = NSToCoordRound(float(font.mFont.size));

          // cache these values
          mEmbellishData.leftSpace = nscoord( em * aDesiredStretchSize.leftSpace );
          mEmbellishData.rightSpace = nscoord( em * aDesiredStretchSize.rightSpace );

          aDesiredStretchSize.width += nscoord( (aDesiredStretchSize.leftSpace + aDesiredStretchSize.rightSpace) * em );
          nscoord dx = nscoord( aDesiredStretchSize.leftSpace * em );        
          if (0 == dx) return NS_OK;
    
          nsPoint origin;
          childFrame = mFrames.FirstChild();
          while (childFrame) {
            childFrame->GetOrigin(origin);
            childFrame->MoveTo(aPresContext, origin.x + dx, origin.y);
            childFrame->GetNextSibling(&childFrame);
          }
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::FinalizeReflow(PRInt32              aDirection,
                                       nsIPresContext*      aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       nsHTMLReflowMetrics& aDesiredSize)
{
  // During reflow, we use rect.x and rect.y as placeholders for the child's ascent
  // and descent in expectation of a stretch command. Hence we need to ensure that
  // a stretch command will actually be fired later on, after exiting from our
  // reflow. If the stretch is not fired, the rect.x, and rect.y will remain
  // with inappropriate data causing children to be improperly positioned.
  // This helper method checks to see if our parent will fire a stretch command
  // targeted at us. If not, we go ahead and fire an involutive stretch on 
  // ourselves. This will clear all the rect.x and rect.y, and return our
  // desired size.


  // First, complete the post-reflow hook.
  // We use the information in our children rectangles to position them.
  // If placeOrigin==false, then Place() will not touch rect.x, and rect.y.
  // They will still be holding the ascent and descent for each child.
  PRBool placeOrigin = !NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags);
  Place(aPresContext, aRenderingContext, placeOrigin, aDesiredSize);

  if (!placeOrigin) {
    // This means the rect.x and rect.y of our children were not set!! 
    // Don't go without checking to see if our parent will later fire a Stretch() command
    // targeted at us. The Stretch() will cause the rect.x and rect.y to clear...
    PRBool parentWillFireStretch = PR_FALSE;
    nsEmbellishData parentData;
    nsIMathMLFrame* aMathMLFrame = nsnull;
    nsresult rv = mParent->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
    if (NS_SUCCEEDED(rv) && aMathMLFrame) {
      aMathMLFrame->GetEmbellishData(parentData);
      if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN(parentData.flags) ||
         (NS_MATHML_WILL_STRETCH_FIRST_CHILD(parentData.flags) && 
          parentData.firstChild == this)) {
        parentWillFireStretch = PR_TRUE;
      }
    }
    if (!parentWillFireStretch) {
      // There is nobody who will fire the stretch for us, we do it ourselves!

       // BEGIN of GETTING THE STRETCH SIZE
       // What is the size that we should use to stretch our stretchy children ????

// 1) With this code, vertical stretching works. But horizontal stretching 
// does not work when the firstChild happens to be the core embellished mo...
//      nsRect rect;
//      nsIFrame* childFrame = mEmbellishData.firstChild;
//      NS_ASSERTION(childFrame, "Something is wrong somewhere");
//      childFrame->GetRect(rect);
//      nsStretchMetrics curSize(rect.x, rect.y, rect.width, rect.height);


// 2) With this code, horizontal stretching works. But vertical stretching
// is done in some cases where frames could have simply been kept as is.
      nsStretchMetrics curSize(aDesiredSize);


// 3) With this code, we should get appropriate size when it is done !!
//      GetDesiredSize(aDirection, aPresContext, aRenderingContext, curSize);

      // XXX It is not clear if a direction should be imposed. 
      // With the default direction, the MathMLChar will attempt to stretch 
      // in its preferred direction.

      nsStretchMetrics newSize(curSize);
      Stretch(aPresContext, aRenderingContext, NS_STRETCH_DIRECTION_DEFAULT, 
              curSize, newSize);

      aDesiredSize.width = newSize.width; 
      aDesiredSize.height = newSize.height;
      aDesiredSize.ascent = newSize.ascent; 
      aDesiredSize.descent = newSize.descent;
    }
  }
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  return NS_OK;
}

// This is the method used to set the frame as an embellished container.
// It checks if the first (non-empty) child is embellished. Hence, calls 
// must be bottom-up. The method must only be called from within frames who are 
// entitled to be potential embellished operators as per the MathML REC. 
NS_IMETHODIMP
nsMathMLContainerFrame::EmbellishOperator()
{
  // Get the first non-empty child
  nsIFrame* firstChild = mFrames.FirstChild();
  while (firstChild) {
    if (!IsOnlyWhitespace(firstChild)) break;
    firstChild->GetNextSibling(&firstChild);
  }
  if (firstChild && IsEmbellishOperator(firstChild)) {
    // Cache the first child
    mEmbellishData.flags |= NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.firstChild = firstChild;
    // Cache also the inner-most embellished frame at the core of the hierarchy
    nsIMathMLFrame* aMathMLFrame = nsnull;
    nsresult rv = firstChild->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
    nsEmbellishData embellishData;
    aMathMLFrame->GetEmbellishData(embellishData);
    mEmbellishData.core = embellishData.core;
    mEmbellishData.direction = embellishData.direction;
  }
  else {
    mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_OPERATOR;
    mEmbellishData.firstChild = nsnull;
    mEmbellishData.core = nsnull;
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::GetEmbellishData(nsEmbellishData& aEmbellishData)
{
  aEmbellishData = mEmbellishData;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::SetEmbellishData(const nsEmbellishData& aEmbellishData)
{
  mEmbellishData = aEmbellishData;
  return NS_OK;
}

PRBool
nsMathMLContainerFrame::IsEmbellishOperator(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null arg");
  if (!aFrame) return PR_FALSE;
  nsIMathMLFrame* aMathMLFrame = nsnull;
  nsresult rv = aFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
  if (NS_FAILED(rv) || !aMathMLFrame) return PR_FALSE;
  nsEmbellishData aEmbellishData;
  aMathMLFrame->GetEmbellishData(aEmbellishData);
  return NS_MATHML_IS_EMBELLISH_OPERATOR(aEmbellishData.flags);
}

/* /////////////
 * nsIMathMLFrame - support methods for scripting elements (nested frames
 * within msub, msup, msubsup, munder, mover, munderover, mmultiscripts, 
 * mfrac, mroot, mtable).
 * =============================================================================
 */

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
nsMathMLContainerFrame::InsertScriptLevelStyleContext(nsIPresContext* aPresContext)
{
  nsresult rv = NS_OK;
  nsIFrame* nextFrame = mFrames.FirstChild();
  while (nsnull != nextFrame && NS_SUCCEEDED(rv)) { 	
    nsIFrame* childFrame = nextFrame;
    rv = nextFrame->GetNextSibling(&nextFrame);
    if (!IsOnlyWhitespace(childFrame) && NS_SUCCEEDED(rv)) {

      // see if the child frame implements the nsIMathMLFrame interface
      nsIMathMLFrame* aMathMLFrame = nsnull;
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

          nsCOMPtr<nsIPresShell> shell;
          aPresContext->GetShell(getter_AddRefs(shell));

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

            aPresContext->ResolvePseudoStyleContextFor(childContent, fontAtom, lastStyleContext,
                                                       PR_FALSE, getter_AddRefs(newStyleContext));          
            if (newStyleContext && newStyleContext.get() != lastStyleContext) {
              // create a new wrapper frame and append it as sole child of the last created frame
              nsIFrame* newFrame = nsnull;
              NS_NewMathMLWrapperFrame(shell, &newFrame);
              NS_ASSERTION(newFrame, "Failed to create new frame");
              if (!newFrame) break;
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
            aPresContext->ReParentStyleContext(childFrame, lastStyleContext);
            lastFrame->SetInitialChildList(aPresContext, nsnull, childFrame);

            // if the child was an embellished operator,
            // make the whole list embellished as well
            nsEmbellishData embellishData;
            aMathMLFrame->GetEmbellishData(embellishData);
            if (0 != embellishData.flags && nsnull != embellishData.firstChild) {
              do { // walk the hierarchy in a bottom-up manner
                rv= lastFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
                NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
                if (NS_FAILED(rv) || !aMathMLFrame) break;
                embellishData.firstChild = childFrame;
                aMathMLFrame->SetEmbellishData(embellishData);
                childFrame = lastFrame;
                lastFrame->GetParent(&lastFrame);
              } while (lastFrame != this);
            }
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
nsMathMLContainerFrame::Init(nsIPresContext*  aPresContext,
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
  mCompressed = PR_FALSE; // for compatibility with TeX rendering
  mScriptSpace = 0; // = 0.5 pt in plain TeX

  mEmbellishData.flags = 0;
  mEmbellishData.firstChild = nsnull;

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
nsMathMLContainerFrame::SetInitialChildList(nsIPresContext* aPresContext,
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
      if (NS_SUCCEEDED(rv) && inlineFrame) {
        // create a new anonymous block frame to wrap this child...
        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        nsIFrame* anonymous;
        rv = NS_NewBlockFrame(shell, &anonymous);
        if (NS_FAILED(rv))
          return rv;    
        nsCOMPtr<nsIStyleContext> newStyleContext;
        aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::mozAnonymousBlock, 
                                                   mStyleContext, PR_FALSE, 
                                                   getter_AddRefs(newStyleContext));
        rv = anonymous->Init(aPresContext, mContent, this, newStyleContext, nsnull);
        if (NS_FAILED(rv)) {
          anonymous->Destroy(aPresContext);
          return rv;
        }
        mFrames.ReplaceFrame(this, child, anonymous);
        child->SetParent(anonymous);
        child->SetNextSibling(nsnull);
        aPresContext->ReParentStyleContext(child, newStyleContext);
        anonymous->SetInitialChildList(aPresContext, nsnull, child);
      }
    }
  }

  return rv;
}

#if 0
// helper method to compute the desired size of a frame
NS_IMETHODIMP
nsMathMLContainerFrame::GetDesiredStretchSize(nsIPresContext*      aPresContext,
                                              nsIRenderingContext& aRenderingContext,
                                              nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;
  nsRect rect;
  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      if (IsEmbellishOperator(childFrame)) {
        // We have encountered an embellished operator...
        // It is treated as if the embellishments were not there!
        nsIMathMLFrame* aMathMLFrame = nsnull;
        rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
        NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {
          nsEmbellishData embellishData;
          aMathMLFrame->GetEmbellishData(embellishData);
          embellishData.core->GetRect(rect);
        }
      }
      if (0 == aDirection) { // for horizontal positioning of child frames like in mrow
        aDesiredSize.width += rect.width;
        if (aDesiredSize.ascent < rect.y) aDesiredSize.ascent = rect.y;
        if (aDesiredSize.descent < rect.x) aDesiredSize.descent = rect.x;
      }
      else if (1 == aDirection) { // for vertical positioning of child frames like in mfrac, munder
        if (0 == count) { // it is the ascent and descent of the 'base' that is returned by defaul
          aDesiredSize.ascent = rect.y;
          aDesiredSize.descent = rect.x;
        }
        if (aDesiredSize.width < rect.width) aDesiredSize.width = rect.width;
      }
      count++;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  
  // Check if embellished operators didn't have (non-empty) siblings...
  if (0 < count && 0 == aDirection && 0 == aDesiredSize.height) {
    // Return the font ascent and font descent
    nsCOMPtr<nsIFontMetrics> fm;
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    aPresContext->GetMetricsFor(font.mFont, getter_AddRefs(fm));
    fm->GetMaxAscent(aDesiredSize.ascent);
    fm->GetMaxDescent(aDesiredSize.descent);
    aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  }
  if (0 < count && 1 == aDirection && 0 == aDesiredSize.width) {
    // Return em
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    aDesiredSize.width = NSToCoordRound(float(font.mFont.size));
  }
  return NS_OK;
}
#endif


// helper function to reflow token elements
// note that mBoundingMetrics is computed here
nsresult
nsMathMLContainerFrame::ReflowTokenFor(nsIFrame*                aFrame,
                                       nsIPresContext*          aPresContext,
                                       nsHTMLReflowMetrics&     aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aFrame, "null arg");
  nsresult rv = NS_OK;

  // ask our children to compute their bounding metrics 
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  nsBoundingMetrics bm;
  PRInt32 count = 0; 
  nsIFrame* childFrame;
  aFrame->FirstChild(nsnull, &childFrame);
  while (childFrame) {
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);      
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = NS_STATIC_CAST(nsMathMLContainerFrame*, 
                          aFrame)->ReflowChild(childFrame, 
                                               aPresContext, childDesiredSize,
                                               childReflowState, aStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
      if (NS_FAILED(rv)) return rv;

      // origins are used as placeholders to store the child's ascent and descent.
      childFrame->SetRect(aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));
      // compute and cache the bounding metrics
      if (0 == count)
        bm  = childDesiredSize.mBoundingMetrics;
      else 
        bm += childDesiredSize.mBoundingMetrics;

      count++;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  NS_STATIC_CAST(nsMathMLContainerFrame*, aFrame)->SetBoundingMetrics(bm);

  // Place and size children
  NS_STATIC_CAST(nsMathMLContainerFrame*, 
                 aFrame)->FinalizeReflow(0, aPresContext, *aReflowState.rendContext, 
                                         aDesiredSize);
  return NS_OK;
}

// helper function to place token elements
// mBoundingMetrics is computed at the ReflowToken pass, it is
// not computed here because if our children may be text frames that
// do not implement the GetBoundingMetrics() interface.
nsresult
nsMathMLContainerFrame::PlaceTokenFor(nsIFrame*            aFrame,
                                      nsIPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      PRBool               aPlaceOrigin,  
                                      nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
 
  nsRect rect;
  nsIFrame* childFrame;
  aFrame->FirstChild(nsnull, &childFrame);
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      aDesiredSize.width += rect.width;
      if (aDesiredSize.descent < rect.x) aDesiredSize.descent = rect.x;
      if (aDesiredSize.ascent < rect.y) aDesiredSize.ascent = rect.y;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  
  if (aPlaceOrigin) {
    nscoord dy;
    nscoord dx = 0;

    aFrame->FirstChild(nsnull, &childFrame);
    while (childFrame) {
      childFrame->GetRect(rect);

      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = rect.height;

      // Place and size the child
      dy = aDesiredSize.ascent - rect.y;
      NS_STATIC_CAST(nsMathMLContainerFrame*, 
                     aFrame)->FinishReflowChild(childFrame, aPresContext, 
                                                childSize, dx, dy, 0);
      dx += rect.width;
      childFrame->GetNextSibling(&childFrame);
    }
  }
  return NS_OK;
}

// helper function to reflow all children
NS_IMETHODIMP
nsMathMLContainerFrame::ReflowChildren(PRInt32                  aDirection,
                                       nsIPresContext*          aPresContext,
                                       nsHTMLReflowMetrics&     aDesiredSize,
                                       const nsHTMLReflowState& aReflowState,
                                       nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aDirection==0 || aDirection==1, "Unknown direction");

  nsresult rv = NS_OK;
  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  // Asks the child to cache its bounding metrics
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize, 
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);

  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;

  nscoord count = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);      
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                       childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) return rv;

      // At this stage, the origin points of the children have no use, so we will use the
      // origins as placeholders to store the child's ascent and descent. Later on,
      // we should set the origins so as to overwrite what we are storing there now.
      childFrame->SetRect(aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));

      if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN(mEmbellishData.flags) && 
          IsEmbellishOperator(childFrame)) {
        // We have encountered an embellished operator...
        // It is treated as if the embellishments were not there!
        nsIMathMLFrame* aMathMLFrame = nsnull;
        rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
        NS_ASSERTION(NS_SUCCEEDED(rv) && aMathMLFrame, "Mystery!");
        if (NS_SUCCEEDED(rv) && aMathMLFrame) {

          nsEmbellishData embellishData;
          aMathMLFrame->GetEmbellishData(embellishData);
          nsRect rect;
          embellishData.core->GetRect(rect);
          childDesiredSize.descent = rect.x;
          childDesiredSize.ascent = rect.y;
          childDesiredSize.width = rect.width;
          childDesiredSize.height = rect.height;

#if 0
          nsIRenderingContext& renderingContext = *aReflowState.rendContext;
          nsStretchMetrics stretchSize;
          aMathMLFrame->GetDesiredStretchSize(aPresContext, renderingContext, stretchSize);
          childDesiredSize.descent = stretchSize.descent;
          childDesiredSize.ascent = stretchSize.ascent;
          childDesiredSize.width = stretchSize.width;
          childDesiredSize.height = stretchSize.height;
#endif
        }
      }
      if (0 == aDirection) { // for horizontal positioning of child frames like in mrow
        aDesiredSize.width += childDesiredSize.width;
        if (aDesiredSize.ascent < childDesiredSize.ascent) aDesiredSize.ascent = childDesiredSize.ascent;
        if (aDesiredSize.descent < childDesiredSize.descent) aDesiredSize.descent = childDesiredSize.descent;
      }
      else if (1 == aDirection) { // for vertical positioning of child frames like in mfrac, munder
        if (0 == count) { // it is the ascent and descent of the 'base' that is returned by defaul
          aDesiredSize.ascent = childDesiredSize.ascent;
          aDesiredSize.descent = childDesiredSize.descent;
        }
        if (aDesiredSize.width < childDesiredSize.width) aDesiredSize.width = childDesiredSize.width;
      }
      count++;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  // Check if embellished operators didn't have (non-empty) siblings...
  if (0 < count && 0 == aDirection && 0 == aDesiredSize.height) {
    // Return the font ascent and font descent
    nsCOMPtr<nsIFontMetrics> fm;
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    aPresContext->GetMetricsFor(font.mFont, getter_AddRefs(fm));
    fm->GetMaxAscent(aDesiredSize.ascent);
    fm->GetMaxDescent(aDesiredSize.descent);
    aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  }
  if (0 < count && 1 == aDirection && 0 == aDesiredSize.width) {
    // Return em
    nsStyleFont font;
    mStyleContext->GetStyle(eStyleStruct_Font, font);
    aDesiredSize.width = NSToCoordRound(float(font.mFont.size));
  }

  return NS_OK;
}

// helper function to stretch all children
NS_IMETHODIMP
nsMathMLContainerFrame::StretchChildren(nsIPresContext*      aPresContext,
                                        nsIRenderingContext& aRenderingContext,
                                        nsStretchDirection   aStretchDirection,
                                        nsStretchMetrics&    aContainerSize)
{
  nsRect rect;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (mEmbellishData.firstChild == childFrame) {
      // Skip this child... because:
      // If we are here it means we are an embellished container and
      // for now, we don't touch our embellished child frame.
      // Its stretch will be handled separatedly when we receive
      // stretch command fired by our parent frame.
    }
    else {
      nsIMathMLFrame* aMathMLFrame;
      nsresult rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
      if (NS_SUCCEEDED(rv) && aMathMLFrame) {
        // retrieve the metrics that was stored at the ReflowChildren pass
        childFrame->GetRect(rect);
        nsStretchMetrics childSize(rect.x, rect.y, rect.width, rect.height);
        aMathMLFrame->Stretch(aPresContext, aRenderingContext, 
                              aStretchDirection, aContainerSize, childSize);
        // store the updated metrics
        childFrame->SetRect(aPresContext,
                            nsRect(childSize.descent, childSize.ascent,
                                   childSize.width, childSize.height));
      }
    }
    childFrame->GetNextSibling(&childFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  /////////////
  // Reflow children
  ReflowChildren(0, aPresContext, aDesiredSize, aReflowState, aStatus);

  /////////////
  // If we are a container which is entitled to stretch its children, then we 
  // ask our stretchy children to stretch themselves

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;

  if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN(mEmbellishData.flags)) {
    nsStretchMetrics containerSize(aDesiredSize);
    nsStretchDirection stretchDir = NS_STRETCH_DIRECTION_VERTICAL;

    StretchChildren(aPresContext, renderingContext, stretchDir, containerSize);
  }

  /////////////
  // Place children now by re-adjusting the origins to align the baselines
  FinalizeReflow(0, aPresContext, renderingContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Place(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aPlaceOrigin,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
 
  PRInt32 count = 0; 
  nsRect rect;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);

      aDesiredSize.width += rect.width;
      if (aDesiredSize.descent < rect.x) aDesiredSize.descent = rect.x;
      if (aDesiredSize.ascent < rect.y) aDesiredSize.ascent = rect.y;

      // Compute and cache our bounding metrics
      nsBoundingMetrics bm;
      if (NS_FAILED(GetBoundingMetricsFor(childFrame, bm))) {
        bm.ascent  =  rect.y;
        bm.descent = -rect.x;
        bm.width   =  rect.width;
      }
      if (0 == count)   
        mBoundingMetrics  = bm;
      else
        mBoundingMetrics += bm;

      count++;
    }
    childFrame->GetNextSibling(&childFrame);
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  
  if (aPlaceOrigin) {
    nscoord dy;
    nscoord dx = 0;
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      childFrame->GetRect(rect);

      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = rect.height;

      // Place and size the child
      dy = aDesiredSize.ascent - rect.y;
      FinishReflowChild(childFrame, aPresContext, childSize, dx, dy, 0);

      dx += rect.width;
      childFrame->GetNextSibling(&childFrame);
    }
  }
  return NS_OK;
}


//==========================
// *BEWARE* of the wrapper frame!
// This is the frame that is inserted to alter the style context of
// scripting elements. What this means is that looking for your parent
// or your siblings with (possible several) wrapper frames around you
// can make you wonder what is going on. For example, the direct parent
// of the subscript within <msub> is not <msub>, but instead the wrapper
// frame that was insterted to alter the style context of the subscript!
// You will seldom need to find out who is exactly your parent. You should
// first rethink your code to see if you can avoid finding who is your parent. 
// Be careful, there are wrapper frames all over the place, and probably 
// one or many are wrapping you if you are in a position where the
// scriptlevel is non zero.

nsresult
NS_NewMathMLWrapperFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLWrapperFrame* it = new (aPresShell) nsMathMLWrapperFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLWrapperFrame::nsMathMLWrapperFrame()
{
}

nsMathMLWrapperFrame::~nsMathMLWrapperFrame()
{
}

// For Reflow() and Place(), pretend we are a token to re-use code
NS_IMETHODIMP
nsMathMLWrapperFrame::Reflow(nsIPresContext*          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  return ReflowTokenFor(this, aPresContext, aDesiredSize,
                        aReflowState, aStatus);
}

NS_IMETHODIMP
nsMathMLWrapperFrame::Place(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceTokenFor(this, aPresContext, aRenderingContext,
                       aPlaceOrigin, aDesiredSize);
}
