/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsCSSAnonBoxes.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"
#include "nsIDOMMutationEvent.h"
#include "nsFrameManager.h"
#include "nsStyleChangeList.h"

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLChar.h"
#include "nsMathMLContainerFrame.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"

NS_DEFINE_CID(kInlineFrameCID, NS_INLINE_FRAME_CID);

//
// nsMathMLContainerFrame implementation
//

// nsISupports
// =============================================================================

NS_IMPL_ADDREF_INHERITED(nsMathMLContainerFrame, nsMathMLFrame)
NS_IMPL_RELEASE_INHERITED(nsMathMLContainerFrame, nsMathMLFrame)
NS_IMPL_QUERY_INTERFACE_INHERITED1(nsMathMLContainerFrame, nsHTMLContainerFrame, nsMathMLFrame)

// =============================================================================

// error handlers
// provide a feedback to the user when a frame with bad markup can not be rendered
nsresult
nsMathMLContainerFrame::ReflowError(nsIPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv;

  // clear all other flags and record that there is an error with this frame
  mEmbellishData.flags = 0;
  mPresentationData.flags = NS_MATHML_ERROR;

  ///////////////
  // Set font
  aRenderingContext.SetFont(GetStyleFont()->mFont, nsnull);

  // bounding metrics
  nsAutoString errorMsg; errorMsg.AssignLiteral("invalid-markup");
  rv = aRenderingContext.GetBoundingMetrics(errorMsg.get(),
                                            PRUint32(errorMsg.Length()),
                                            mBoundingMetrics);
  if (NS_FAILED(rv)) {
    NS_WARNING("GetBoundingMetrics failed");
    aDesiredSize.width = aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.descent = 0;
    return NS_OK;
  }

  // reflow metrics
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(aDesiredSize.ascent);
  fm->GetMaxDescent(aDesiredSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  return NS_OK;
}

nsresult
nsMathMLContainerFrame::PaintError(nsIPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   const nsRect&        aDirtyRect,
                                   nsFramePaintLayer    aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    NS_ASSERTION(NS_MATHML_HAS_ERROR(mPresentationData.flags),
                 "There is nothing wrong with this frame!");
    // Set color and font ...
    aRenderingContext.SetFont(GetStyleFont()->mFont, nsnull);

    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.FillRect(0, 0, mRect.width, mRect.height);
    aRenderingContext.SetColor(NS_RGB(255,255,255));

    nscoord ascent;
    nsCOMPtr<nsIFontMetrics> fm;
    aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));
    fm->GetMaxAscent(ascent);

    nsAutoString errorMsg; errorMsg.AssignLiteral("invalid-markup");
    aRenderingContext.DrawString(errorMsg.get(),
                                 PRUint32(errorMsg.Length()),
                                 0, ascent);
  }
  return NS_OK;
}

/* /////////////
 * nsIMathMLFrame - support methods for stretchy elements
 * =============================================================================
 */

// helper method to facilitate getting the reflow and bounding metrics
void
nsMathMLContainerFrame::GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                                       nsHTMLReflowMetrics& aReflowMetrics,
                                                       nsBoundingMetrics&   aBoundingMetrics)
{
  NS_PRECONDITION(aFrame, "null arg");

  // IMPORTANT: This function is only meant to be called in Place() methods
  // where it is assumed that the frame's rect is still acting as place holder
  // for the frame's ascent and descent information

  nsRect rect = aFrame->GetRect();
  aReflowMetrics.descent = rect.x;
  aReflowMetrics.ascent  = rect.y;
  aReflowMetrics.width   = rect.width;
  aReflowMetrics.height  = rect.height;

  aBoundingMetrics.Clear();
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (mathMLFrame) {
    mathMLFrame->GetBoundingMetrics(aBoundingMetrics);
  }
  else { // aFrame is not a MathML frame, just return the reflow metrics
    aBoundingMetrics.descent = aReflowMetrics.descent;
    aBoundingMetrics.ascent  = aReflowMetrics.ascent;
    aBoundingMetrics.width   = aReflowMetrics.width;
    aBoundingMetrics.rightBearing = aReflowMetrics.width;
  }
}

// helper to get the preferred size that a container frame should use to fire
// the stretch on its stretchy child frames.
void
nsMathMLContainerFrame::GetPreferredStretchSize(nsIPresContext*      aPresContext,
                                                nsIRenderingContext& aRenderingContext,
                                                PRUint32             aOptions,
                                                nsStretchDirection   aStretchDirection,
                                                nsBoundingMetrics&   aPreferredStretchSize)
{
  if (aOptions & STRETCH_CONSIDER_ACTUAL_SIZE) {
    // when our actual size is ok, just use it
    aPreferredStretchSize = mBoundingMetrics;
  }
  else if (aOptions & STRETCH_CONSIDER_EMBELLISHMENTS) {
    // compute our up-to-date size using Place()
    nsHTMLReflowMetrics metrics(nsnull);
    Place(aPresContext, aRenderingContext, PR_FALSE, metrics);
    aPreferredStretchSize = metrics.mBoundingMetrics;
  }
  else {
    // compute a size that doesn't include embellishements
    NS_ASSERTION(NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags) ||
                 NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags) ||
                 NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags),
                 "invalid call to GetPreferredStretchSize");
    PRBool firstTime = PR_TRUE;
    nsBoundingMetrics bm, bmChild;
    // XXXrbs need overloaded FirstChild() and clean integration of <maction> throughout
    nsIFrame* childFrame = GetFirstChild(nsnull);
    while (childFrame) {
      // initializations in case this child happens not to be a MathML frame
      nsRect rect = childFrame->GetRect();
      bmChild.ascent = rect.y;
      bmChild.descent = rect.x;
      bmChild.width = rect.width;
      bmChild.rightBearing = rect.width;
      bmChild.leftBearing = 0;

      nsIMathMLFrame* mathMLFrame;
      childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (mathMLFrame) {
      	nsEmbellishData childData;
        mathMLFrame->GetEmbellishData(childData);
        if (NS_MATHML_IS_EMBELLISH_OPERATOR(childData.flags) &&
            childData.direction == aStretchDirection &&
            childData.nextFrame) {
          // embellishements are not included, only consider the inner first child itself
          nsIMathMLFrame* mathMLchildFrame;
          childData.nextFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLchildFrame);
          if (mathMLchildFrame) {
            mathMLFrame = mathMLchildFrame;
          }
        }
        mathMLFrame->GetBoundingMetrics(bmChild);
      }

      if (firstTime) {
        firstTime = PR_FALSE;
        bm = bmChild;
        if (!NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags) &&
            !NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags)) {
          // we may get here for cases such as <msup><mo>...</mo> ... </msup>
          break;
        }
      }
      else {
        if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags)) {
          // if we get here, it means this is container that will stack its children
          // vertically and fire an horizontal stretch on each them. This is the case
          // for \munder, \mover, \munderover. We just sum-up the size vertically.
          bm.descent += bmChild.ascent + bmChild.descent;
          if (bm.leftBearing > bmChild.leftBearing)
            bm.leftBearing = bmChild.leftBearing;
          if (bm.rightBearing < bmChild.rightBearing)
            bm.rightBearing = bmChild.rightBearing;
        }
        else if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags)) {
          // just sum-up the sizes horizontally.
          bm += bmChild;
        }
        else {
          NS_ERROR("unexpected case in GetPreferredStretchSize");
          break;
        }
      }
      childFrame = childFrame->GetNextSibling();
    }
    aPreferredStretchSize = bm;
  }
}

NS_IMETHODIMP
nsMathMLContainerFrame::Stretch(nsIPresContext*      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsStretchDirection   aStretchDirection,
                                nsBoundingMetrics&   aContainerSize,
                                nsHTMLReflowMetrics& aDesiredStretchSize)
{
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags)) {

    if (NS_MATHML_STRETCH_WAS_DONE(mPresentationData.flags)) {
      NS_WARNING("it is wrong to fire stretch more than once on a frame");
      return NS_OK;
    }
    mPresentationData.flags |= NS_MATHML_STRETCH_DONE;

    if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
      NS_WARNING("it is wrong to fire stretch on a erroneous frame");
      return NS_OK;
    }

    // Pass the stretch to the first non-empty child ...

    nsIFrame* childFrame = mEmbellishData.nextFrame;
    if (childFrame) {
      nsIMathMLFrame* mathMLFrame;
      childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      NS_ASSERTION(mathMLFrame, "Something is wrong somewhere");
      if (mathMLFrame) {
        PRBool stretchAll =
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ||
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags);

        // And the trick is that the child's rect.x is still holding the descent,
        // and rect.y is still holding the ascent ...
        nsHTMLReflowMetrics childSize(aDesiredStretchSize);
        GetReflowAndBoundingMetricsFor(childFrame, childSize, childSize.mBoundingMetrics);

        // See if we should downsize and confine the stretch to us...
        // XXX there may be other cases where we can downsize the stretch,
        // e.g., the first &Sum; might appear big in the following situation
        // <math xmlns='http://www.w3.org/1998/Math/MathML'>
        //   <mstyle>
        //     <msub>
        //        <msub><mo>&Sum;</mo><mfrac><mi>a</mi><mi>b</mi></mfrac></msub>
        //        <msub><mo>&Sum;</mo><mfrac><mi>a</mi><mi>b</mi></mfrac></msub>
        //      </msub>
        //   </mstyle>
        // </math>
        nsBoundingMetrics containerSize = aContainerSize;
        if (aStretchDirection != NS_STRETCH_DIRECTION_DEFAULT &&
            aStretchDirection != mEmbellishData.direction) {
          if (mEmbellishData.direction == NS_STRETCH_DIRECTION_UNSUPPORTED) {
            containerSize = childSize.mBoundingMetrics;
          }
          else {
            GetPreferredStretchSize(aPresContext, aRenderingContext, 
                                    stretchAll ? STRETCH_CONSIDER_EMBELLISHMENTS : 0,
                                    mEmbellishData.direction, containerSize);
          }
        }

        // do the stretching...
        mathMLFrame->Stretch(aPresContext, aRenderingContext,
                             mEmbellishData.direction, containerSize, childSize);

        // store the updated metrics
        childFrame->SetRect(nsRect(childSize.descent, childSize.ascent,
                                   childSize.width, childSize.height));

        // Remember the siblings which were _deferred_.
        // Now that this embellished child may have changed, we need to
        // fire the stretch on its siblings using our updated size

        if (stretchAll) {

          nsStretchDirection stretchDir =
            NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ?
              NS_STRETCH_DIRECTION_VERTICAL : NS_STRETCH_DIRECTION_HORIZONTAL;

          GetPreferredStretchSize(aPresContext, aRenderingContext, STRETCH_CONSIDER_EMBELLISHMENTS,
                                  stretchDir, containerSize);

          childFrame = mFrames.FirstChild();
          while (childFrame) {
            if (childFrame != mEmbellishData.nextFrame) {
              childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
              if (mathMLFrame) {
                // retrieve the metrics that was stored at the previous pass
                GetReflowAndBoundingMetricsFor(childFrame, 
                  childSize, childSize.mBoundingMetrics);
                // do the stretching...
                mathMLFrame->Stretch(aPresContext, aRenderingContext,
                                     stretchDir, containerSize, childSize);
                // store the updated metrics
                childFrame->SetRect(nsRect(childSize.descent, childSize.ascent,
                                           childSize.width, childSize.height));
              }
            }
            childFrame = childFrame->GetNextSibling();
          }
        }

        // re-position all our children
        Place(aPresContext, aRenderingContext, PR_TRUE, aDesiredStretchSize);

        // If our parent is not embellished, it means we are the outermost embellished
        // container and so we put the spacing, otherwise we don't include the spacing,
        // the outermost embellished container will take care of it.

        nsEmbellishData parentData;
        GetEmbellishDataFrom(mParent, parentData);
        // ensure that we are the embellished child, not just a sibling
        // (need to test coreFrame since <mfrac> resets other things)
        if (parentData.coreFrame != mEmbellishData.coreFrame) {
          // (we fetch values from the core since they may use units that depend
          // on style data, and style changes could have occured in the core since
          // our last visit there)
          nsEmbellishData coreData;
          GetEmbellishDataFrom(mEmbellishData.coreFrame, coreData);

          mBoundingMetrics.width += coreData.leftSpace + coreData.rightSpace;
          aDesiredStretchSize.width = mBoundingMetrics.width;
          aDesiredStretchSize.mBoundingMetrics.width = mBoundingMetrics.width;

          nscoord dx = coreData.leftSpace;
          if (!dx) return NS_OK;

          mBoundingMetrics.leftBearing += dx;
          mBoundingMetrics.rightBearing += dx;
          aDesiredStretchSize.mBoundingMetrics.leftBearing += dx;
          aDesiredStretchSize.mBoundingMetrics.rightBearing += dx;

          childFrame = mFrames.FirstChild();
          while (childFrame) {
            childFrame->SetPosition(childFrame->GetPosition()
				    + nsPoint(dx, 0));
            childFrame = childFrame->GetNextSibling();
          }
        }
      }
    }
  }
  return NS_OK;
}

nsresult
nsMathMLContainerFrame::FinalizeReflow(nsIPresContext*      aPresContext,
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

  // The first clause caters for any non-embellished container.
  // The second clause is for a container which won't fire stretch even though it is
  // embellished, e.g., as in <mfrac><mo>...</mo> ... </mfrac>, the test is convoluted
  // because it excludes the particular case of the core <mo>...</mo> itself.
  // (<mo> needs to fire stretch on its MathMLChar in any case to initialize it)
  PRBool placeOrigin = !NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags) ||
                       (mEmbellishData.coreFrame != this && !mEmbellishData.nextFrame &&
                        mEmbellishData.direction == NS_STRETCH_DIRECTION_UNSUPPORTED);
  Place(aPresContext, aRenderingContext, placeOrigin, aDesiredSize);

  if (!placeOrigin) {
    // This means the rect.x and rect.y of our children were not set!!
    // Don't go without checking to see if our parent will later fire a Stretch() command
    // targeted at us. The Stretch() will cause the rect.x and rect.y to clear...
    PRBool parentWillFireStretch = PR_FALSE;
    nsIMathMLFrame* mathMLFrame;
    mParent->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      nsEmbellishData embellishData;
      nsPresentationData presentationData;
      mathMLFrame->GetEmbellishData(embellishData);
      mathMLFrame->GetPresentationData(presentationData);
      if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(presentationData.flags) ||
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(presentationData.flags) ||
          (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags)
            && embellishData.nextFrame == this))
      {
        parentWillFireStretch = PR_TRUE;
      }
    }
    if (!parentWillFireStretch) {
      // There is nobody who will fire the stretch for us, we do it ourselves!

      PRBool stretchAll =
        /* NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) || */
        NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags);

      nsBoundingMetrics defaultSize;
      if (mEmbellishData.coreFrame == this /* case of a bare <mo>...</mo> itself */
          || stretchAll) { /* or <mover><mo>...</mo>...</mover>, or friends */
        // use our current size as computed earlier by Place()
        defaultSize = aDesiredSize.mBoundingMetrics;
      }
      else { /* case of <msup><mo>...</mo>...</msup> or friends */
        // compute a size that doesn't include embellishments
        GetPreferredStretchSize(aPresContext, aRenderingContext, 0,
                                mEmbellishData.direction, defaultSize);
      }
      Stretch(aPresContext, aRenderingContext, NS_STRETCH_DIRECTION_DEFAULT,
              defaultSize, aDesiredSize);
    }
  }
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = aDesiredSize.width;
  }
  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  // see if we should fix the spacing
  FixInterFrameSpacing(aPresContext, aDesiredSize);

  return NS_OK;
}


/* /////////////
 * nsIMathMLFrame - support methods for scripting elements (nested frames
 * within msub, msup, msubsup, munder, mover, munderover, mmultiscripts,
 * mfrac, mroot, mtable).
 * =============================================================================
 */

// helper to let the update of presentation data pass through
// a subtree that may contain non-mathml container frames
/* static */ void
nsMathMLContainerFrame::PropagatePresentationDataFor(nsIPresContext* aPresContext,
                                                     nsIFrame*       aFrame,
                                                     PRInt32         aScriptLevelIncrement,
                                                     PRUint32        aFlagsValues,
                                                     PRUint32        aFlagsToUpdate)
{
  if (!aFlagsToUpdate && !aScriptLevelIncrement)
    return;
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (mathMLFrame) {
    // update
    mathMLFrame->UpdatePresentationData(aPresContext,
      aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    // propagate using the base method to make sure that the control
    // is passed on to MathML frames that may be overloading the method
    mathMLFrame->UpdatePresentationDataFromChildAt(aPresContext,
      0, -1, aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
  }
  else {
    // propagate down the subtrees
    nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);
    while (childFrame) {
      PropagatePresentationDataFor(aPresContext, childFrame,
        aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
      childFrame = childFrame->GetNextSibling();
    }
  }
}

/* static */ void
nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                                             nsIFrame*       aParentFrame,
                                                             PRInt32         aFirstChildIndex,
                                                             PRInt32         aLastChildIndex,
                                                             PRInt32         aScriptLevelIncrement,
                                                             PRUint32        aFlagsValues,
                                                             PRUint32        aFlagsToUpdate)
{
  if (!aFlagsToUpdate && !aScriptLevelIncrement)
    return;
  PRInt32 index = 0;
  nsIFrame* childFrame = aParentFrame->GetFirstChild(nsnull);
  while (childFrame) {
    if ((index >= aFirstChildIndex) &&
        ((aLastChildIndex <= 0) || ((aLastChildIndex > 0) &&
         (index <= aLastChildIndex)))) {
      PropagatePresentationDataFor(aPresContext, childFrame,
        aScriptLevelIncrement, aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame = childFrame->GetNextSibling();
  }
}

// helper to let the scriptstyle re-resolution pass through
// a subtree that may contain non-mathml container frames.
// This function is *very* expensive. Unfortunately, there isn't much
// to do about it at the moment. For background on the problem @see 
// http://groups.google.com/groups?selm=3A9192B5.D22B6C38%40maths.uq.edu.au
/* static */ void
nsMathMLContainerFrame::PropagateScriptStyleFor(nsIPresContext* aPresContext,
                                                nsIFrame*       aFrame,
                                                PRInt32         aParentScriptLevel)
{
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (mathMLFrame) {
    // we will re-resolve our style data based on our current scriptlevel
    nsPresentationData presentationData;
    mathMLFrame->GetPresentationData(presentationData);
    PRInt32 gap = presentationData.scriptLevel - aParentScriptLevel;

    // since we are a MathML frame, our current scriptlevel becomes
    // the one to use when we will propagate the recursion
    aParentScriptLevel = presentationData.scriptLevel;

    nsStyleContext* oldStyleContext = aFrame->GetStyleContext();
    nsStyleContext* parentContext = oldStyleContext->GetParent();

    nsIContent* content = aFrame->GetContent();
    if (!gap) {
      // unset any -moz-math-font-size attribute without notifying that we want a reflow
      content->UnsetAttr(kNameSpaceID_None, nsMathMLAtoms::fontsize, PR_FALSE);
    }
    else {
      // By default scriptminsize=8pt and scriptsizemultiplier=0.71
      nscoord scriptminsize = NSIntPointsToTwips(NS_MATHML_SCRIPTMINSIZE);
      float scriptsizemultiplier = NS_MATHML_SCRIPTSIZEMULTIPLIER;
#if 0
       // XXX Bug 44201
       // user-supplied scriptminsize and scriptsizemultiplier that are
       // restricted to particular elements are not supported because our
       // css rules are fixed in mathml.css and are applicable to all elements.

       // see if there is a scriptminsize attribute on a <mstyle> that wraps us
       if (NS_CONTENT_ATTR_HAS_VALUE ==
           GetAttribute(nsnull, presentationData.mstyle,
                        nsMathMLAtoms::scriptminsize_, fontsize)) {
         nsCSSValue cssValue;
         if (ParseNumericValue(fontsize, cssValue)) {
           nsCSSUnit unit = cssValue.GetUnit();
           if (eCSSUnit_Number == unit)
             scriptminsize = nscoord(float(scriptminsize) * cssValue.GetFloatValue());
           else if (eCSSUnit_Percent == unit)
             scriptminsize = nscoord(float(scriptminsize) * cssValue.GetPercentValue());
           else if (eCSSUnit_Null != unit)
             scriptminsize = CalcLength(aPresContext, mStyleContext, cssValue);
         }
       }
#endif

      // figure out the incremental factor
      nsAutoString fontsize;
      if (0 > gap) { // the size is going to be increased
        if (gap < NS_MATHML_CSS_NEGATIVE_SCRIPTLEVEL_LIMIT)
          gap = NS_MATHML_CSS_NEGATIVE_SCRIPTLEVEL_LIMIT;
        gap = -gap;
        scriptsizemultiplier = 1.0f / scriptsizemultiplier;
        fontsize.AssignLiteral("-");
      }
      else { // the size is going to be decreased
        if (gap > NS_MATHML_CSS_POSITIVE_SCRIPTLEVEL_LIMIT)
          gap = NS_MATHML_CSS_POSITIVE_SCRIPTLEVEL_LIMIT;
        fontsize.AssignLiteral("+");
      }
      fontsize.AppendInt(gap, 10);
      // we want to make sure that the size will stay readable
      const nsStyleFont* font = parentContext->GetStyleFont();
      nscoord newFontSize = font->mFont.size;
      while (0 < gap--) {
        newFontSize = (nscoord)((float)(newFontSize) * scriptsizemultiplier);
      }
      if (newFontSize <= scriptminsize) {
        fontsize.AssignLiteral("scriptminsize");
      }

      // set the -moz-math-font-size attribute without notifying that we want a reflow
      content->SetAttr(kNameSpaceID_None, nsMathMLAtoms::fontsize,
                       fontsize, PR_FALSE);
    }

    // now, re-resolve the style contexts in our subtree
    nsFrameManager *fm = aPresContext->FrameManager();
    nsStyleChangeList changeList;
    fm->ComputeStyleChangeFor(aFrame, &changeList, NS_STYLE_HINT_NONE);
#ifdef DEBUG
    // Use the parent frame to make sure we catch in-flows and such
    nsIFrame* parentFrame = aFrame->GetParent();
    fm->DebugVerifyStyleTree(parentFrame ? parentFrame : aFrame);
#endif
  }

  // recurse down the subtrees for changes that may arise deep down
  nsIFrame* childFrame = aFrame->GetFirstChild(nsnull);
  while (childFrame) {
    childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      // propagate using the base method to make sure that the control
      // is passed on to MathML frames that may be overloading the method
      mathMLFrame->ReResolveScriptStyle(aPresContext, aParentScriptLevel);
    }
    else {
      PropagateScriptStyleFor(aPresContext, childFrame, aParentScriptLevel);
    }
    childFrame = childFrame->GetNextSibling();
  }
}


/* //////////////////
 * Frame construction
 * =============================================================================
 */

// We use this to wrap non-MathML frames so that foreign elements (e.g.,
// html:img) can mix better with other surrounding MathML markups.
// Currently we only wrap nsInlineFrames because problems were observed only
// in the presence of such frames. By construction, a foreign frame wrapper
// has one and only one child, and the life of the wrapper is bound to the
// life of that unique child. Not all child list operations are applicable
// with a wrapper. One must either use the parent (or the unique child)
// for such operations (@see nsMathMLForeignFrameWrapper).
nsresult
nsMathMLContainerFrame::WrapForeignFrames(nsIPresContext* aPresContext)
{
  nsIFrame* next = mFrames.FirstChild();
  nsFrameManager *frameManager = aPresContext->FrameManager();

  while (next) {
    nsIFrame* child = next;
    next = next->GetNextSibling();
    nsInlineFrame* inlineFrame;
    child->QueryInterface(kInlineFrameCID, (void**)&inlineFrame);
    if (inlineFrame) {
      // create a new wrapper frame to wrap this child
      nsIFrame* wrapper;
      nsresult rv = NS_NewMathMLForeignFrameWrapper(aPresContext->PresShell(),
						    &wrapper);
      if (NS_FAILED(rv)) return rv;
      nsRefPtr<nsStyleContext> newStyleContext;
      newStyleContext = aPresContext->StyleSet()->
	ResolvePseudoStyleFor(mContent,
			      nsCSSAnonBoxes::mozAnonymousBlock,
			      mStyleContext);
      rv = wrapper->Init(aPresContext, mContent, this, newStyleContext, nsnull);
      if (NS_FAILED(rv)) {
        wrapper->Destroy(aPresContext);
        return rv;
      }
      mFrames.ReplaceFrame(aPresContext, this, child, wrapper, PR_FALSE);
      child->SetParent(wrapper);
      child->SetNextSibling(nsnull);
      frameManager->ReParentStyleContext(child, newStyleContext);
      wrapper->SetInitialChildList(aPresContext, nsnull, child);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Paint(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer,
                              PRUint32             aFlags)
{
  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

  // report an error if something wrong was found in this frame
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    return PaintError(aPresContext, aRenderingContext,
                      aDirtyRect, aWhichLayer);
  }

  // Paint inline element backgrounds in the foreground layer (bug 36710).
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
  }

  PaintDecorationsAndChildren(aPresContext, aRenderingContext, aDirtyRect,
                              aWhichLayer, PR_FALSE, aFlags);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // for visual debug
  // ----------------
  // if you want to see your bounding box, make sure to properly fill
  // your mBoundingMetrics and mReference point, and set
  // mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS
  // in the Init() of your sub-class

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer &&
      NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags))
  {
    aRenderingContext.SetColor(NS_RGB(0,0,255));

    nscoord x = mReference.x + mBoundingMetrics.leftBearing;
    nscoord y = mReference.y - mBoundingMetrics.ascent;
    nscoord w = mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing;
    nscoord h = mBoundingMetrics.ascent + mBoundingMetrics.descent;

    aRenderingContext.DrawRect(x,y,w,h);
  }
#endif
  return NS_OK;
}

// This method is called in a top-down manner, as we descend the frame tree
// during its construction
NS_IMETHODIMP
nsMathMLContainerFrame::Init(nsIPresContext*  aPresContext,
                             nsIContent*      aContent,
                             nsIFrame*        aParent,
                             nsStyleContext*  aContext,
                             nsIFrame*        aPrevInFlow)
{
  MapAttributesIntoCSS(aPresContext, aContent);

  // let the base class do its Init()
  return nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // ...We will build our automatic MathML data once the entire <math>...</math>
  // tree is constructed.
}

// This method is called in a bottom-up manner, as we ascend the frame tree
// after its construction
NS_IMETHODIMP
nsMathMLContainerFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                            nsIAtom*        aListName,
                                            nsIFrame*       aChildList)
{
  // First, let the base class do its job
  nsresult rv = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // Next, since we are an inline frame, and since we are a container, we have to
  // be very careful with the way we treat our children. Things look okay when
  // all of our children are only MathML frames. But there are problems if one of
  // our children happens to be an nsInlineFrame, e.g., from generated content such
  // as :before { content: open-quote } or :after { content: close-quote }
  // The code asserts during reflow (in nsLineLayout::BeginSpan)
  // Also there are problems when our children are hybrid, e.g., from html markups.
  // In short, the nsInlineFrame class expects a number of *invariants* that are not
  // met when we mix things.

  // So wrap foreign children in nsMathMLForeignFrameWrapper frames
  WrapForeignFrames(aPresContext);
  return rv;

  // ...We will build our automatic MathML data once the entire <math>...</math>
  // tree is constructed.
}

// Note that this method re-builds the automatic data in the children -- not
// in aParentFrame itself (except for those particular operations that the
// parent frame may do in its TransmitAutomaticData()).
/* static */ void
nsMathMLContainerFrame::RebuildAutomaticDataForChildren(nsIPresContext* aPresContext,
                                                        nsIFrame*       aParentFrame)
{
  // 1. As we descend the tree, make each child frame inherit data from
  // the parent
  // 2. As we ascend the tree, transmit any specific change that we want
  // down the subtrees
  nsIFrame* childFrame = aParentFrame->GetFirstChild(nsnull);
  while (childFrame) {
    nsIMathMLFrame* childMathMLFrame;
    childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&childMathMLFrame);
    if (childMathMLFrame) {
      childMathMLFrame->InheritAutomaticData(aPresContext, aParentFrame);
    }
    RebuildAutomaticDataForChildren(aPresContext, childFrame);
    childFrame = childFrame->GetNextSibling();
  }
  nsIMathMLFrame* mathMLFrame;
  aParentFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (mathMLFrame) {
    mathMLFrame->TransmitAutomaticData(aPresContext);
  }
}

/* static */ nsresult
nsMathMLContainerFrame::ReLayoutChildren(nsIPresContext* aPresContext,
                                         nsIFrame*       aParentFrame)
{
  // walk-up to the first frame that is a MathML frame, stop if we reach <math>
  PRInt32 parentScriptLevel = 0;
  nsIFrame* frame = aParentFrame;
  while (frame) {
    // stop if it is a MathML frame
    nsIMathMLFrame* mathMLFrame;
    frame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      nsPresentationData parentData;
      mathMLFrame->GetPresentationData(parentData);
      parentScriptLevel = parentData.scriptLevel;
      break;
    }
    // stop if we reach the root <math> tag
    nsIContent* content = frame->GetContent();
    NS_ASSERTION(content, "dangling frame without a content node");
    if (!content)
      return NS_ERROR_FAILURE;

    if (content->Tag() == nsMathMLAtoms::math) {
      break;
    }
    // mark the frame dirty, and continue to climb up
    frame->AddStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
    frame = frame->GetParent();
  }
  NS_ASSERTION(frame, "bad MathML markup - could not find the top <math> element");
  if (!frame)
    return NS_OK;

  // re-sync the presentation data and embellishment data of our children
#ifdef DEBUG_rbs
  nsEmbellishData oldData;
  GetEmbellishDataFrom(frame, oldData);
#endif

  RebuildAutomaticDataForChildren(aPresContext, frame);

#ifdef DEBUG_rbs
  nsEmbellishData newData;
  GetEmbellishDataFrom(frame, newData);
  NS_ASSERTION((NS_MATHML_IS_EMBELLISH_OPERATOR(oldData.flags) == 
                NS_MATHML_IS_EMBELLISH_OPERATOR(newData.flags)) && 
               (oldData.coreFrame == newData.coreFrame),
               "embellished hierarchy has to be rebuilt too");
#endif

  // re-resolve the style data to sync any change of script sizes
  nsIFrame* childFrame = aParentFrame->GetFirstChild(nsnull);
  while (childFrame) {
    nsIMathMLFrame* mathMLFrame;
    childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
    if (mathMLFrame) {
      // propagate using the base method to make sure that the control
      // is passed on to MathML frames that may be overloading the method
      mathMLFrame->ReResolveScriptStyle(aPresContext, parentScriptLevel);
    }
    else {
      PropagateScriptStyleFor(aPresContext, childFrame, parentScriptLevel);
    }
    childFrame = childFrame->GetNextSibling();
  }

  // Ask our parent frame to reflow us
  return frame->ReflowDirtyChild(aPresContext->PresShell(), nsnull);
}

// There are precise rules governing children of a MathML frame,
// and properties such as the scriptlevel depends on those rules.
// Hence for things to work, callers must use Append/Insert/etc wisely.

nsresult
nsMathMLContainerFrame::ChildListChanged(nsIPresContext* aPresContext,
                                         PRInt32         aModType)
{
  if (aModType != nsIDOMMutationEvent::REMOVAL) {
    // wrap any new foreign child that may have crept in
    WrapForeignFrames(aPresContext);
  }
  return ReLayoutChildren(aPresContext, this);
}

NS_IMETHODIMP
nsMathMLContainerFrame::AppendFrames(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aFrameList)
{
  if (aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aFrameList) {
    mFrames.AppendFrames(this, aFrameList);
    return ChildListChanged(aPresContext, nsIDOMMutationEvent::ADDITION);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::InsertFrames(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aPrevFrame,
                                     nsIFrame*       aFrameList)
{
  if (aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aFrameList) {
    // Insert frames after aPrevFrame
    mFrames.InsertFrames(this, aPrevFrame, aFrameList);
    return ChildListChanged(aPresContext, nsIDOMMutationEvent::ADDITION);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::RemoveFrame(nsIPresContext* aPresContext,
                                    nsIPresShell&   aPresShell,
                                    nsIAtom*        aListName,
                                    nsIFrame*       aOldFrame)
{
  if (aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  // remove the child frame
  mFrames.DestroyFrame(aPresContext, aOldFrame);
  return ChildListChanged(aPresContext, nsIDOMMutationEvent::REMOVAL);
}

NS_IMETHODIMP
nsMathMLContainerFrame::ReplaceFrame(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame,
                                     nsIFrame*       aNewFrame)
{
  if (aListName || !aOldFrame || !aNewFrame) {
    return NS_ERROR_INVALID_ARG;
  }
  // Replace the old frame with the new frame in the list
  mFrames.ReplaceFrame(aPresContext, this, aOldFrame, aNewFrame, PR_TRUE);

  return ChildListChanged(aPresContext, nsIDOMMutationEvent::MODIFICATION);
}

NS_IMETHODIMP
nsMathMLContainerFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent*     aChild,
                                         PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType)
{
  if (aAttribute == nsMathMLAtoms::mathcolor_      ||
      aAttribute == nsMathMLAtoms::color_          ||
      aAttribute == nsMathMLAtoms::mathsize_       ||
      aAttribute == nsMathMLAtoms::fontsize_       ||
      aAttribute == nsMathMLAtoms::fontfamily_     ||
      aAttribute == nsMathMLAtoms::mathbackground_ ||
      aAttribute == nsMathMLAtoms::background_) {
    MapAttributesIntoCSS(aPresContext, this);
  }

  return ReflowDirtyChild(aPresContext->PresShell(), nsnull);
}

// We are an inline frame, so we handle dirty request like nsInlineFrame
NS_IMETHODIMP
nsMathMLContainerFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  // The inline container frame does not handle the reflow
  // request.  It passes it up to its parent container.

  // If you don't already have dirty children,
  if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    if (mParent) {
      // Record that you are dirty and have dirty children
      mState |= NS_FRAME_IS_DIRTY;
      mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

      // Pass the reflow request up to the parent
      mParent->ReflowDirtyChild(aPresShell, (nsIFrame*) this);
    }
    else {
      NS_ASSERTION(0, "No parent to pass the reflow request up to.");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Reflow(nsIPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  nsresult rv;
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  // See if this is an incremental reflow
  if (aReflowState.reason == eReflowReason_Incremental) {
#ifdef MATHML_NOISY_INCREMENTAL_REFLOW
printf("nsMathMLContainerFrame::Reflow:IncrementalReflow received by: ");
nsFrame::ListTag(stdout, this);
printf("\n");
#endif
  }

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.mComputeMEW,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                     childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) return rv;

    // At this stage, the origin points of the children have no use, so we will use the
    // origins as placeholders to store the child's ascent and descent. Later on,
    // we should set the origins so as to overwrite what we are storing there now.
    childFrame->SetRect(nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                               childDesiredSize.width, childDesiredSize.height));
    childFrame = childFrame->GetNextSibling();
  }

  /////////////
  // If we are a container which is entitled to stretch its children, then we
  // ask our stretchy children to stretch themselves

  // The stretching of siblings of an embellished child is _deferred_ until
  // after finishing the stretching of the embellished child - bug 117652

  if (!NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags) &&
      (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ||
       NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags))) {

    // get the stretchy direction
    nsStretchDirection stretchDir =
      NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) 
      ? NS_STRETCH_DIRECTION_VERTICAL 
      : NS_STRETCH_DIRECTION_HORIZONTAL;

    // what size should we use to stretch our stretchy children
    // We don't use STRETCH_CONSIDER_ACTUAL_SIZE -- because our size is not known yet
    // We don't use STRETCH_CONSIDER_EMBELLISHMENTS -- because we don't want to
    // include them in the caculations of the size of stretchy elements
    nsBoundingMetrics containerSize;
    GetPreferredStretchSize(aPresContext, *aReflowState.rendContext, 0,
                            stretchDir, containerSize);

    // fire the stretch on each child
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      nsIMathMLFrame* mathMLFrame;
      childFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
      if (mathMLFrame) {
        // retrieve the metrics that was stored at the previous pass
        GetReflowAndBoundingMetricsFor(childFrame,
          childDesiredSize, childDesiredSize.mBoundingMetrics);

        mathMLFrame->Stretch(aPresContext, *aReflowState.rendContext,
                             stretchDir, containerSize, childDesiredSize);
        // store the updated metrics
        childFrame->SetRect(nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                   childDesiredSize.width, childDesiredSize.height));
      }
      childFrame = childFrame->GetNextSibling();
    }
  }

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = childDesiredSize.mMaxElementWidth;
  }

  /////////////
  // Place children now by re-adjusting the origins to align the baselines
  FinalizeReflow(aPresContext, *aReflowState.rendContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// For MathML, the 'type' will be used to determine the spacing between frames
// Subclasses can override this method to return a 'type' that will give
// them a particular spacing
nsIAtom*
nsMathMLContainerFrame::GetType() const
{
  // see if this is an embellished operator (mapped to 'Op' in TeX)
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags) &&
      mEmbellishData.coreFrame) {
    return mEmbellishData.coreFrame->GetType();
  }

  // everything else is a schematta element (mapped to 'Inner' in TeX)
  return nsMathMLAtoms::schemataMathMLFrame;
}

enum eMathMLFrameType {
  eMathMLFrameType_UNKNOWN = -1,
  eMathMLFrameType_Ordinary,
  eMathMLFrameType_OperatorVisible,
  eMathMLFrameType_OperatorInvisible,
  eMathMLFrameType_Inner
};

// see spacing table in Chapter 18, TeXBook (p.170)
// Our table isn't quite identical to TeX because operators have 
// built-in values for lspace & rspace in the Operator Dictionary.
static PRInt32 kInterFrameSpacingTable[4][4] =
{
  // in units of muspace.
  // upper half of the byte is set if the
  // spacing is not to be used for scriptlevel > 0
  /*          Ord   OpVis OpInv Inner */
  /*Ord  */  {0x01, 0x00, 0x01, 0x01},
  /*OpVis*/  {0x00, 0x00, 0x00, 0x00},
  /*OpInv*/  {0x01, 0x00, 0x00, 0x01},
  /*Inner*/  {0x01, 0x00, 0x01, 0x01}
};

#define GET_INTERSPACE(scriptlevel_, frametype1_, frametype2_, space_)  \
   /* no space if there is a frame that we know nothing about */        \
   if (frametype1_ == eMathMLFrameType_UNKNOWN ||                       \
       frametype2_ == eMathMLFrameType_UNKNOWN)                         \
    space_ = 0;                                                         \
  else {                                                                \
    space_ = kInterFrameSpacingTable[frametype1_][frametype2_];         \
    space_ = (scriptlevel_ > 0 && (space_ & 0xF0))                      \
      ? 0 /* spacing is disabled */                                     \
      : space_ & 0x0F;                                                  \
  }                                                                     \

#define MAP_FRAMETYPE(atomtype_, enumtype_)                             \
  if (atomtype_ == nsMathMLAtoms::ordinaryMathMLFrame)                  \
    enumtype_ = eMathMLFrameType_Ordinary;                              \
  else if (atomtype_ == nsMathMLAtoms::schemataMathMLFrame)             \
    enumtype_ = eMathMLFrameType_Inner;                                 \
  else if (atomtype_ == nsMathMLAtoms::operatorVisibleMathMLFrame)      \
    enumtype_ = eMathMLFrameType_OperatorVisible;                       \
  else if (atomtype_ == nsMathMLAtoms::operatorInvisibleMathMLFrame)    \
    enumtype_ = eMathMLFrameType_OperatorInvisible;                     \
  else                                                                  \
    enumtype_ = eMathMLFrameType_UNKNOWN;

// This function computes the inter-space between two frames. However, 
// since invisible operators need special treatment, the inter-space may
// be delayed when an invisible operator is encountered. In this case,
// the function will carry the inter-space forward until it is determined
// that it can be applied properly (i.e., until we encounter a visible
// frame where to decide whether to accept or reject the inter-space).
// aFromFrameType: remembers the frame when the carry-forward initiated.
// aCarrySpace: keeps track of the inter-space that is delayed.
// @returns: current inter-space (which is 0 when the true inter-space is
// delayed -- and thus has no effect since the frame is invisible anyway).
static nscoord
GetInterFrameSpacing(PRInt32           aScriptLevel,
                     nsIAtom*          aFirstFrameType,
                     nsIAtom*          aSecondFrameType,
                     eMathMLFrameType* aFromFrameType, // IN/OUT
                     PRInt32*          aCarrySpace)    // IN/OUT
{
  eMathMLFrameType firstType, secondType;
  MAP_FRAMETYPE(aFirstFrameType, firstType);
  MAP_FRAMETYPE(aSecondFrameType, secondType);

  PRInt32 space;
  GET_INTERSPACE(aScriptLevel, firstType, secondType, space);

  // feedback control to avoid the inter-space to be added when not necessary
  if (secondType == eMathMLFrameType_OperatorInvisible) {
    // see if we should start to carry the space forward until we
    // encounter a visible frame
    if (*aFromFrameType == eMathMLFrameType_UNKNOWN) {
      *aFromFrameType = firstType;
      *aCarrySpace = space;
    }
    // keep carrying *aCarrySpace forward, while returning 0 for this stage
    space = 0;
  }
  else if (*aFromFrameType != eMathMLFrameType_UNKNOWN) {
    // no carry-forward anymore, get the real inter-space between
    // the two frames of interest
    GET_INTERSPACE(aScriptLevel, *aFromFrameType, secondType, space);

    // Now, we have two values: the computed space and the space that
    // has been carried forward until now. Which value do we pick?
    // If the second type is an operator (e.g., fence), it already has
    // built-in lspace & rspace, so we let them win. Otherwise we pick
    // the max between the two values that we have.
    if (secondType != eMathMLFrameType_OperatorVisible &&
        space < *aCarrySpace)
      space = *aCarrySpace;

    // reset everything now that the carry-forward is done
    *aFromFrameType = eMathMLFrameType_UNKNOWN;
    *aCarrySpace = 0;
  }

  return space;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Place(nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aPlaceOrigin,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  // these are needed in case this frame is empty (i.e., we don't enter the loop)
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  mBoundingMetrics.Clear();

  // cache away thinspace
  const nsStyleFont* font = GetStyleFont();
  nscoord thinSpace = NSToCoordRound(float(font->mFont.size)*float(3) / float(18));

  PRInt32 count = 0;
  PRInt32 carrySpace = 0;
  eMathMLFrameType fromFrameType = eMathMLFrameType_UNKNOWN;
  nsHTMLReflowMetrics childSize (nsnull);
  nsBoundingMetrics bmChild;
  nscoord leftCorrection = 0, italicCorrection = 0;
  nsIAtom* prevFrameType = nsnull;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsIAtom* childFrameType = childFrame->GetType();
    GetReflowAndBoundingMetricsFor(childFrame, childSize, bmChild);
    GetItalicCorrection(bmChild, leftCorrection, italicCorrection);
    if (0 == count) {
      aDesiredSize.ascent = childSize.ascent;
      aDesiredSize.descent = childSize.descent;
      mBoundingMetrics = bmChild;
      // update to include the left correction
      // but leave <msqrt> alone because the sqrt glyph itself is there first

      if (mContent->Tag() == nsMathMLAtoms::msqrt_)
        leftCorrection = 0;
      else
        mBoundingMetrics.leftBearing += leftCorrection;
    }
    else {
      if (aDesiredSize.descent < childSize.descent)
        aDesiredSize.descent = childSize.descent;
      if (aDesiredSize.ascent < childSize.ascent)
        aDesiredSize.ascent = childSize.ascent;
      // add inter frame spacing
      nscoord space = GetInterFrameSpacing(mPresentationData.scriptLevel,
        prevFrameType, childFrameType, &fromFrameType, &carrySpace);
      mBoundingMetrics.width += space * thinSpace;
      // add the child size
      mBoundingMetrics += bmChild;
    }
    count++;
    prevFrameType = childFrameType;
    // add left correction -- this fixes the problem of the italic 'f'
    // e.g., <mo>q</mo> <mi>f</mi> <mo>I</mo> 
    mBoundingMetrics.width += leftCorrection;
    mBoundingMetrics.rightBearing += leftCorrection;
    // add the italic correction at the end (including the last child).
    // this gives a nice gap between math and non-math frames, and still
    // gives the same math inter-spacing in case this frame connects to
    // another math frame
    mBoundingMetrics.width += italicCorrection;

    childFrame = childFrame->GetNextSibling();
  }
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children

  if (aPlaceOrigin) {
    count = 0;
    nscoord dx = 0, dy = 0;
    italicCorrection = 0;
    carrySpace = 0;
    fromFrameType = eMathMLFrameType_UNKNOWN;
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      nsIAtom* childFrameType = childFrame->GetType();
      GetReflowAndBoundingMetricsFor(childFrame, childSize, bmChild);
      GetItalicCorrection(bmChild, leftCorrection, italicCorrection);
      dy = aDesiredSize.ascent - childSize.ascent;
      if (0 == count) {
        // for <msqrt>, the sqrt glyph itself is there first

        if (mContent->Tag() == nsMathMLAtoms::msqrt_)
          leftCorrection = 0;
      }
      else {
        // add inter frame spacing
        nscoord space = GetInterFrameSpacing(mPresentationData.scriptLevel,
          prevFrameType, childFrameType, &fromFrameType, &carrySpace);
        dx += space * thinSpace;
      }
      count++;
      prevFrameType = childFrameType;
      // add left correction
      dx += leftCorrection;
      FinishReflowChild(childFrame, aPresContext, nsnull, childSize, dx, dy, 0);
      // add child size + italic correction
      dx += bmChild.width + italicCorrection;
      childFrame = childFrame->GetNextSibling();
    }
  }

  return NS_OK;
}

// helpers to fix the inter-spacing when <math> is the only parent
// e.g., it fixes <math> <mi>f</mi> <mo>q</mo> <mi>f</mi> <mo>I</mo> </math>

static nscoord
GetInterFrameSpacingFor(nsIPresContext* aPresContext,
                        PRInt32         aScriptLevel,
                        nsIFrame*       aParentFrame,
                        nsIFrame*       aChildFrame)
{
  nsIFrame* childFrame = aParentFrame->GetFirstChild(nsnull);
  if (!childFrame || aChildFrame == childFrame)
    return 0;

  PRInt32 carrySpace = 0;
  eMathMLFrameType fromFrameType = eMathMLFrameType_UNKNOWN;
  nsIAtom* childFrameType = childFrame->GetType();
  nsIAtom* prevFrameType = nsnull;
  childFrame = childFrame->GetNextSibling();
  while (childFrame) {
    prevFrameType = childFrameType;
    childFrameType = childFrame->GetType();
    nscoord space = GetInterFrameSpacing(aScriptLevel,
      prevFrameType, childFrameType, &fromFrameType, &carrySpace);
    if (aChildFrame == childFrame) {
      // get thinspace
      nsStyleContext* parentContext = aParentFrame->GetStyleContext();
      const nsStyleFont* font = parentContext->GetStyleFont();
      nscoord thinSpace = NSToCoordRound(float(font->mFont.size)*float(3) / float(18));
      // we are done
      return space * thinSpace;
    }
    childFrame = childFrame->GetNextSibling();
  }

  NS_NOTREACHED("child not in the childlist of its parent");
  return 0;
}

nsresult
nsMathMLContainerFrame::FixInterFrameSpacing(nsIPresContext*      aPresContext,
                                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsIContent* parentContent = mParent->GetContent();
  nsIAtom *parentTag = parentContent->Tag();
  if (parentTag == nsMathMLAtoms::math ||
      parentTag == nsMathMLAtoms::mtd_) {
    nscoord gap = GetInterFrameSpacingFor(aPresContext,
      mPresentationData.scriptLevel, mParent, this);
    // add our own italic correction
    nscoord leftCorrection = 0, italicCorrection = 0;
    GetItalicCorrection(mBoundingMetrics, leftCorrection, italicCorrection);
    gap += leftCorrection;
    // see if we should shift our children to account for the correction
    if (gap) {
      nsIFrame* childFrame = mFrames.FirstChild();
      while (childFrame) {
        childFrame->SetPosition(childFrame->GetPosition() + nsPoint(gap, 0));
        childFrame = childFrame->GetNextSibling();
      }
      mBoundingMetrics.leftBearing += gap;
      mBoundingMetrics.rightBearing += gap;
      mBoundingMetrics.width += gap;
      aDesiredSize.width += gap;
    }
    mBoundingMetrics.width += italicCorrection;
    aDesiredSize.width += italicCorrection;
  }
  return NS_OK;
}



//==========================

nsresult
NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmathBlockFrame* it = new (aPresShell) nsMathMLmathBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

//==========================

nsresult
NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmathInlineFrame* it = new (aPresShell) nsMathMLmathInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}
