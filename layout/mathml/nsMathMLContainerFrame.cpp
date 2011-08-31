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
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
 *   Frederic Wang <fred.wang@free.fr>
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
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCSSAnonBoxes.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsRenderingContext.h"

#include "nsIDOMMutationEvent.h"
#include "nsFrameManager.h"
#include "nsStyleChangeList.h"

#include "nsGkAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLContainerFrame.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsDisplayList.h"
#include "nsCSSFrameConstructor.h"
#include "nsIReflowCallback.h"

using namespace mozilla;

//
// nsMathMLContainerFrame implementation
//

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLContainerFrame)

NS_QUERYFRAME_HEAD(nsMathMLContainerFrame)
  NS_QUERYFRAME_ENTRY(nsMathMLFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLContainerFrame)

// =============================================================================

// error handlers
// provide a feedback to the user when a frame with bad markup can not be rendered
nsresult
nsMathMLContainerFrame::ReflowError(nsRenderingContext& aRenderingContext,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  // clear all other flags and record that there is an error with this frame
  mEmbellishData.flags = 0;
  mPresentationData.flags = NS_MATHML_ERROR;

  ///////////////
  // Set font
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  aRenderingContext.SetFont(fm);

  // bounding metrics
  nsAutoString errorMsg; errorMsg.AssignLiteral("invalid-markup");
  mBoundingMetrics =
    aRenderingContext.GetBoundingMetrics(errorMsg.get(), errorMsg.Length());

  // reflow metrics
  aDesiredSize.ascent = fm->MaxAscent();
  nscoord descent = fm->MaxDescent();
  aDesiredSize.height = aDesiredSize.ascent + descent;
  aDesiredSize.width = mBoundingMetrics.width;

  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  return NS_OK;
}

class nsDisplayMathMLError : public nsDisplayItem {
public:
  nsDisplayMathMLError(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayMathMLError);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMathMLError() {
    MOZ_COUNT_DTOR(nsDisplayMathMLError);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("MathMLError", TYPE_MATHML_ERROR)
};

void nsDisplayMathMLError::Paint(nsDisplayListBuilder* aBuilder,
                                 nsRenderingContext* aCtx)
{
  // Set color and font ...
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(mFrame, getter_AddRefs(fm));
  aCtx->SetFont(fm);

  nsPoint pt = ToReferenceFrame();
  aCtx->SetColor(NS_RGB(255,0,0));
  aCtx->FillRect(nsRect(pt, mFrame->GetSize()));
  aCtx->SetColor(NS_RGB(255,255,255));

  nscoord ascent = aCtx->FontMetrics()->MaxAscent();

  NS_NAMED_LITERAL_STRING(errorMsg, "invalid-markup");
  aCtx->DrawString(errorMsg.get(), PRUint32(errorMsg.Length()),
                   pt.x, pt.y+ascent);
}

/* /////////////
 * nsIMathMLFrame - support methods for stretchy elements
 * =============================================================================
 */

static PRBool
IsForeignChild(const nsIFrame* aFrame)
{
  // This counts nsMathMLmathBlockFrame as a foreign child, because it
  // uses block reflow
  return !(aFrame->IsFrameOfType(nsIFrame::eMathML)) ||
    aFrame->GetType() == nsGkAtoms::blockFrame;
}

static void
DestroyHTMLReflowMetrics(void *aPropertyValue)
{
  delete static_cast<nsHTMLReflowMetrics*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(HTMLReflowMetricsProperty, DestroyHTMLReflowMetrics)

/* static */ void
nsMathMLContainerFrame::SaveReflowAndBoundingMetricsFor(nsIFrame*                  aFrame,
                                                        const nsHTMLReflowMetrics& aReflowMetrics,
                                                        const nsBoundingMetrics&   aBoundingMetrics)
{
  nsHTMLReflowMetrics *metrics = new nsHTMLReflowMetrics(aReflowMetrics);
  metrics->mBoundingMetrics = aBoundingMetrics;
  aFrame->Properties().Set(HTMLReflowMetricsProperty(), metrics);
}

// helper method to facilitate getting the reflow and bounding metrics
/* static */ void
nsMathMLContainerFrame::GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                                       nsHTMLReflowMetrics& aReflowMetrics,
                                                       nsBoundingMetrics&   aBoundingMetrics,
                                                       eMathMLFrameType*    aMathMLFrameType)
{
  NS_PRECONDITION(aFrame, "null arg");

  nsHTMLReflowMetrics *metrics = static_cast<nsHTMLReflowMetrics*>
    (aFrame->Properties().Get(HTMLReflowMetricsProperty()));

  // IMPORTANT: This function is only meant to be called in Place() methods
  // where it is assumed that SaveReflowAndBoundingMetricsFor has recorded the
  // information.
  NS_ASSERTION(metrics, "Didn't SaveReflowAndBoundingMetricsFor frame!");
  if (metrics) {
    aReflowMetrics = *metrics;
    aBoundingMetrics = metrics->mBoundingMetrics;
  }

  if (aMathMLFrameType) {
    if (!IsForeignChild(aFrame)) {
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(aFrame);
      if (mathMLFrame) {
        *aMathMLFrameType = mathMLFrame->GetMathMLFrameType();
        return;
      }
    }
    *aMathMLFrameType = eMathMLFrameType_UNKNOWN;
  }

}

void
nsMathMLContainerFrame::ClearSavedChildMetrics()
{
  nsIFrame* childFrame = mFrames.FirstChild();
  FramePropertyTable* props = PresContext()->PropertyTable();
  while (childFrame) {
    props->Delete(childFrame, HTMLReflowMetricsProperty());
    childFrame = childFrame->GetNextSibling();
  }
}

// helper to get the preferred size that a container frame should use to fire
// the stretch on its stretchy child frames.
void
nsMathMLContainerFrame::GetPreferredStretchSize(nsRenderingContext& aRenderingContext,
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
    nsHTMLReflowMetrics metrics;
    Place(aRenderingContext, PR_FALSE, metrics);
    aPreferredStretchSize = metrics.mBoundingMetrics;
  }
  else {
    // compute a size that doesn't include embellishements
    PRBool stretchAll =
      NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ||
      NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags);
    NS_ASSERTION(NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags) ||
                 stretchAll,
                 "invalid call to GetPreferredStretchSize");
    PRBool firstTime = PR_TRUE;
    nsBoundingMetrics bm, bmChild;
    nsIFrame* childFrame =
      stretchAll ? GetFirstPrincipalChild() : mPresentationData.baseFrame;
    while (childFrame) {
      // initializations in case this child happens not to be a MathML frame
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(childFrame);
      if (mathMLFrame) {
        nsEmbellishData embellishData;
        nsPresentationData presentationData;
        mathMLFrame->GetEmbellishData(embellishData);
        mathMLFrame->GetPresentationData(presentationData);
        if (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags) &&
            embellishData.direction == aStretchDirection &&
            presentationData.baseFrame) {
          // embellishements are not included, only consider the inner first child itself
          // XXXkt Does that mean the core descendent frame should be used
          // instead of the base child?
          nsIMathMLFrame* mathMLchildFrame = do_QueryFrame(presentationData.baseFrame);
          if (mathMLchildFrame) {
            mathMLFrame = mathMLchildFrame;
          }
        }
        mathMLFrame->GetBoundingMetrics(bmChild);
      }
      else {
        nsHTMLReflowMetrics unused;
        GetReflowAndBoundingMetricsFor(childFrame, unused, bmChild);
      }

      if (firstTime) {
        firstTime = PR_FALSE;
        bm = bmChild;
        if (!stretchAll) {
          // we may get here for cases such as <msup><mo>...</mo> ... </msup>,
          // or <maction>...<mo>...</mo></maction>.
          break;
        }
      }
      else {
        if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags)) {
          // if we get here, it means this is container that will stack its children
          // vertically and fire an horizontal stretch on each them. This is the case
          // for \munder, \mover, \munderover. We just sum-up the size vertically.
          bm.descent += bmChild.ascent + bmChild.descent;
          // Sometimes non-spacing marks (when width is zero) are positioned
          // to the left of the origin, but it is the distance between left
          // and right bearing that is important rather than the offsets from
          // the origin.
          if (bmChild.width == 0) {
            bmChild.rightBearing -= bmChild.leftBearing;
            bmChild.leftBearing = 0;
          }
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
nsMathMLContainerFrame::Stretch(nsRenderingContext& aRenderingContext,
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

    // Pass the stretch to the base child ...

    nsIFrame* baseFrame = mPresentationData.baseFrame;
    if (baseFrame) {
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(baseFrame);
      NS_ASSERTION(mathMLFrame, "Something is wrong somewhere");
      if (mathMLFrame) {
        PRBool stretchAll =
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ||
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(mPresentationData.flags);

        // And the trick is that the child's rect.x is still holding the descent,
        // and rect.y is still holding the ascent ...
        nsHTMLReflowMetrics childSize(aDesiredStretchSize);
        GetReflowAndBoundingMetricsFor(baseFrame, childSize, childSize.mBoundingMetrics);

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
            GetPreferredStretchSize(aRenderingContext, 
                                    stretchAll ? STRETCH_CONSIDER_EMBELLISHMENTS : 0,
                                    mEmbellishData.direction, containerSize);
          }
        }

        // do the stretching...
        mathMLFrame->Stretch(aRenderingContext,
                             mEmbellishData.direction, containerSize, childSize);
        // store the updated metrics
        SaveReflowAndBoundingMetricsFor(baseFrame, childSize,
                                        childSize.mBoundingMetrics);
        
        // Remember the siblings which were _deferred_.
        // Now that this embellished child may have changed, we need to
        // fire the stretch on its siblings using our updated size

        if (stretchAll) {

          nsStretchDirection stretchDir =
            NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(mPresentationData.flags) ?
              NS_STRETCH_DIRECTION_VERTICAL : NS_STRETCH_DIRECTION_HORIZONTAL;

          GetPreferredStretchSize(aRenderingContext, STRETCH_CONSIDER_EMBELLISHMENTS,
                                  stretchDir, containerSize);

          nsIFrame* childFrame = mFrames.FirstChild();
          while (childFrame) {
            if (childFrame != mPresentationData.baseFrame) {
              mathMLFrame = do_QueryFrame(childFrame);
              if (mathMLFrame) {
                // retrieve the metrics that was stored at the previous pass
                GetReflowAndBoundingMetricsFor(childFrame, 
                  childSize, childSize.mBoundingMetrics);
                // do the stretching...
                mathMLFrame->Stretch(aRenderingContext, stretchDir,
                                     containerSize, childSize);
                // store the updated metrics
                SaveReflowAndBoundingMetricsFor(childFrame, childSize,
                                                childSize.mBoundingMetrics);
              }
            }
            childFrame = childFrame->GetNextSibling();
          }
        }

        // re-position all our children
        nsresult rv = Place(aRenderingContext, PR_TRUE, aDesiredStretchSize);
        if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
          // Make sure the child frames get their DidReflow() calls.
          DidReflowChildren(mFrames.FirstChild());
        }

        // If our parent is not embellished, it means we are the outermost embellished
        // container and so we put the spacing, otherwise we don't include the spacing,
        // the outermost embellished container will take care of it.

        nsEmbellishData parentData;
        GetEmbellishDataFrom(mParent, parentData);
        // ensure that we are the embellished child, not just a sibling
        // (need to test coreFrame since <mfrac> resets other things)
        if (parentData.coreFrame != mEmbellishData.coreFrame) {
          // (we fetch values from the core since they may use units that depend
          // on style data, and style changes could have occurred in the core since
          // our last visit there)
          nsEmbellishData coreData;
          GetEmbellishDataFrom(mEmbellishData.coreFrame, coreData);

          mBoundingMetrics.width += coreData.leftSpace + coreData.rightSpace;
          aDesiredStretchSize.width = mBoundingMetrics.width;
          aDesiredStretchSize.mBoundingMetrics.width = mBoundingMetrics.width;

          nscoord dx = coreData.leftSpace;
          if (dx != 0) {
            mBoundingMetrics.leftBearing += dx;
            mBoundingMetrics.rightBearing += dx;
            aDesiredStretchSize.mBoundingMetrics.leftBearing += dx;
            aDesiredStretchSize.mBoundingMetrics.rightBearing += dx;

            nsIFrame* childFrame = mFrames.FirstChild();
            while (childFrame) {
              childFrame->SetPosition(childFrame->GetPosition()
                                      + nsPoint(dx, 0));
              childFrame = childFrame->GetNextSibling();
            }
          }
        }

        // Finished with these:
        ClearSavedChildMetrics();
        // Set our overflow area
        GatherAndStoreOverflow(&aDesiredStretchSize);
      }
    }
  }
  return NS_OK;
}

nsresult
nsMathMLContainerFrame::FinalizeReflow(nsRenderingContext& aRenderingContext,
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
                       (mEmbellishData.coreFrame != this && !mPresentationData.baseFrame &&
                        mEmbellishData.direction == NS_STRETCH_DIRECTION_UNSUPPORTED);
  nsresult rv = Place(aRenderingContext, placeOrigin, aDesiredSize);

  // Place() will call FinishReflowChild() when placeOrigin is true but if
  // it returns before reaching FinishReflowChild() due to errors we need
  // to fulfill the reflow protocol by calling DidReflow for the child frames
  // that still needs it here (or we may crash - bug 366012).
  // If placeOrigin is false we should reach Place() with aPlaceOrigin == true
  // through Stretch() eventually.
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags) || NS_FAILED(rv)) {
    DidReflowChildren(GetFirstPrincipalChild());
    return rv;
  }

  PRBool parentWillFireStretch = PR_FALSE;
  if (!placeOrigin) {
    // This means the rect.x and rect.y of our children were not set!!
    // Don't go without checking to see if our parent will later fire a Stretch() command
    // targeted at us. The Stretch() will cause the rect.x and rect.y to clear...
    nsIMathMLFrame* mathMLFrame = do_QueryFrame(mParent);
    if (mathMLFrame) {
      nsEmbellishData embellishData;
      nsPresentationData presentationData;
      mathMLFrame->GetEmbellishData(embellishData);
      mathMLFrame->GetPresentationData(presentationData);
      if (NS_MATHML_WILL_STRETCH_ALL_CHILDREN_VERTICALLY(presentationData.flags) ||
          NS_MATHML_WILL_STRETCH_ALL_CHILDREN_HORIZONTALLY(presentationData.flags) ||
          (NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags)
            && presentationData.baseFrame == this))
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
        GetPreferredStretchSize(aRenderingContext, 0, mEmbellishData.direction,
                                defaultSize);
      }
      Stretch(aRenderingContext, NS_STRETCH_DIRECTION_DEFAULT, defaultSize,
              aDesiredSize);
#ifdef NS_DEBUG
      {
        // The Place() call above didn't request FinishReflowChild(),
        // so let's check that we eventually did through Stretch().
        nsIFrame* childFrame = GetFirstPrincipalChild();
        for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
          NS_ASSERTION(!(childFrame->GetStateBits() & NS_FRAME_IN_REFLOW),
                       "DidReflow() was never called");
        }
      }
#endif
    }
  }

  // see if we should fix the spacing
  FixInterFrameSpacing(aDesiredSize);

  // Also return our bounding metrics
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  if (!parentWillFireStretch) {
    // Not expecting a stretch.
    // Finished with these:
    ClearSavedChildMetrics();
    // Set our overflow area.
    GatherAndStoreOverflow(&aDesiredSize);
  }

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
nsMathMLContainerFrame::PropagatePresentationDataFor(nsIFrame*       aFrame,
                                                     PRUint32        aFlagsValues,
                                                     PRUint32        aFlagsToUpdate)
{
  if (!aFrame || !aFlagsToUpdate)
    return;
  nsIMathMLFrame* mathMLFrame = do_QueryFrame(aFrame);
  if (mathMLFrame) {
    // update
    mathMLFrame->UpdatePresentationData(aFlagsValues,
                                        aFlagsToUpdate);
    // propagate using the base method to make sure that the control
    // is passed on to MathML frames that may be overloading the method
    mathMLFrame->UpdatePresentationDataFromChildAt(0, -1,
      aFlagsValues, aFlagsToUpdate);
  }
  else {
    // propagate down the subtrees
    nsIFrame* childFrame = aFrame->GetFirstPrincipalChild();
    while (childFrame) {
      PropagatePresentationDataFor(childFrame,
        aFlagsValues, aFlagsToUpdate);
      childFrame = childFrame->GetNextSibling();
    }
  }
}

/* static */ void
nsMathMLContainerFrame::PropagatePresentationDataFromChildAt(nsIFrame*       aParentFrame,
                                                             PRInt32         aFirstChildIndex,
                                                             PRInt32         aLastChildIndex,
                                                             PRUint32        aFlagsValues,
                                                             PRUint32        aFlagsToUpdate)
{
  if (!aParentFrame || !aFlagsToUpdate)
    return;
  PRInt32 index = 0;
  nsIFrame* childFrame = aParentFrame->GetFirstPrincipalChild();
  while (childFrame) {
    if ((index >= aFirstChildIndex) &&
        ((aLastChildIndex <= 0) || ((aLastChildIndex > 0) &&
         (index <= aLastChildIndex)))) {
      PropagatePresentationDataFor(childFrame,
        aFlagsValues, aFlagsToUpdate);
    }
    index++;
    childFrame = childFrame->GetNextSibling();
  }
}

/* //////////////////
 * Frame construction
 * =============================================================================
 */


NS_IMETHODIMP
nsMathMLContainerFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  // report an error if something wrong was found in this frame
  if (NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    if (!IsVisibleForPainting(aBuilder))
      return NS_OK;

    return aLists.Content()->AppendNewToTop(
        new (aBuilder) nsDisplayMathMLError(aBuilder, this));
  }

  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = BuildDisplayListForNonBlockChildren(aBuilder, aDirtyRect, aLists,
                                           DISPLAY_CHILD_INLINE);
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  // for visual debug
  // ----------------
  // if you want to see your bounding box, make sure to properly fill
  // your mBoundingMetrics and mReference point, and set
  // mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS
  // in the Init() of your sub-class
  rv = DisplayBoundingMetrics(aBuilder, this, mReference, mBoundingMetrics, aLists);
#endif
  return rv;
}

// Note that this method re-builds the automatic data in the children -- not
// in aParentFrame itself (except for those particular operations that the
// parent frame may do in its TransmitAutomaticData()).
/* static */ void
nsMathMLContainerFrame::RebuildAutomaticDataForChildren(nsIFrame* aParentFrame)
{
  // 1. As we descend the tree, make each child frame inherit data from
  // the parent
  // 2. As we ascend the tree, transmit any specific change that we want
  // down the subtrees
  nsIFrame* childFrame = aParentFrame->GetFirstPrincipalChild();
  while (childFrame) {
    nsIMathMLFrame* childMathMLFrame = do_QueryFrame(childFrame);
    if (childMathMLFrame) {
      childMathMLFrame->InheritAutomaticData(aParentFrame);
    }
    RebuildAutomaticDataForChildren(childFrame);
    childFrame = childFrame->GetNextSibling();
  }
  nsIMathMLFrame* mathMLFrame = do_QueryFrame(aParentFrame);
  if (mathMLFrame) {
    mathMLFrame->TransmitAutomaticData();
  }
}

/* static */ nsresult
nsMathMLContainerFrame::ReLayoutChildren(nsIFrame* aParentFrame)
{
  if (!aParentFrame)
    return NS_OK;

  // walk-up to the first frame that is a MathML frame, stop if we reach <math>
  nsIFrame* frame = aParentFrame;
  while (1) {
     nsIFrame* parent = frame->GetParent();
     if (!parent || !parent->GetContent())
       break;

    // stop if it is a MathML frame
    nsIMathMLFrame* mathMLFrame = do_QueryFrame(frame);
    if (mathMLFrame)
      break;

    // stop if we reach the root <math> tag
    nsIContent* content = frame->GetContent();
    NS_ASSERTION(content, "dangling frame without a content node");
    if (!content)
      break;
    if (content->GetNameSpaceID() == kNameSpaceID_MathML &&
        content->Tag() == nsGkAtoms::math)
      break;

    // mark the frame dirty, and continue to climb up.  It's important that
    // we're NOT doing this to the frame we plan to pass to FrameNeedsReflow()
    // XXXldb Why do we need to bother with this?  Marking ancestor
    // dirty (which we do below) should do a superset of the work this
    // does.
    frame->AddStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);

    frame = parent;
  }

  // re-sync the presentation data and embellishment data of our children
  RebuildAutomaticDataForChildren(frame);

  // Ask our parent frame to reflow us
  nsIFrame* parent = frame->GetParent();
  NS_ASSERTION(parent, "No parent to pass the reflow request up to");
  if (!parent)
    return NS_OK;

  frame->PresContext()->PresShell()->
    FrameNeedsReflow(frame, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);

  return NS_OK;
}

// There are precise rules governing children of a MathML frame,
// and properties such as the scriptlevel depends on those rules.
// Hence for things to work, callers must use Append/Insert/etc wisely.

nsresult
nsMathMLContainerFrame::ChildListChanged(PRInt32 aModType)
{
  // If this is an embellished frame we need to rebuild the
  // embellished hierarchy by walking-up to the parent of the
  // outermost embellished container.
  nsIFrame* frame = this;
  if (mEmbellishData.coreFrame) {
    nsIFrame* parent = mParent;
    nsEmbellishData embellishData;
    for ( ; parent; frame = parent, parent = parent->GetParent()) {
      GetEmbellishDataFrom(parent, embellishData);
      if (embellishData.coreFrame != mEmbellishData.coreFrame)
        break;

      // Important: do not do this to the frame we plan to pass to
      // ReLayoutChildren
      // XXXldb Why do we need to bother with this?  Marking ancestor
      // dirty (which we do below) should do a superset of the work this
      // does.
      frame->AddStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
  return ReLayoutChildren(frame);
}

NS_IMETHODIMP
nsMathMLContainerFrame::AppendFrames(ChildListID     aListID,
                                     nsFrameList&    aFrameList)
{
  if (aListID != kPrincipalList) {
    return NS_ERROR_INVALID_ARG;
  }
  mFrames.AppendFrames(this, aFrameList);
  return ChildListChanged(nsIDOMMutationEvent::ADDITION);
}

NS_IMETHODIMP
nsMathMLContainerFrame::InsertFrames(ChildListID     aListID,
                                     nsIFrame*       aPrevFrame,
                                     nsFrameList&    aFrameList)
{
  if (aListID != kPrincipalList) {
    return NS_ERROR_INVALID_ARG;
  }
  // Insert frames after aPrevFrame
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);
  return ChildListChanged(nsIDOMMutationEvent::ADDITION);
}

NS_IMETHODIMP
nsMathMLContainerFrame::RemoveFrame(ChildListID     aListID,
                                    nsIFrame*       aOldFrame)
{
  if (aListID != kPrincipalList) {
    return NS_ERROR_INVALID_ARG;
  }
  // remove the child frame
  mFrames.DestroyFrame(aOldFrame);
  return ChildListChanged(nsIDOMMutationEvent::REMOVAL);
}

NS_IMETHODIMP
nsMathMLContainerFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aModType)
{
  // XXX Since they are numerous MathML attributes that affect layout, and
  // we can't check all of them here, play safe by requesting a reflow.
  // XXXldb This should only do work for attributes that cause changes!
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);

  return NS_OK;
}

void
nsMathMLContainerFrame::GatherAndStoreOverflow(nsHTMLReflowMetrics* aMetrics)
{
  // nsIFrame::FinishAndStoreOverflow likes the overflow area to include the
  // frame rectangle.
  aMetrics->SetOverflowAreasToDesiredBounds();

  // All non-child-frame content such as nsMathMLChars (and most child-frame
  // content) is included in mBoundingMetrics.
  nsRect boundingBox(mBoundingMetrics.leftBearing,
                     aMetrics->ascent - mBoundingMetrics.ascent,
                     mBoundingMetrics.rightBearing - mBoundingMetrics.leftBearing,
                     mBoundingMetrics.ascent + mBoundingMetrics.descent);

  // REVIEW: Maybe this should contribute only to visual overflow
  // and not scrollable?
  aMetrics->mOverflowAreas.UnionAllWith(boundingBox);

  // mBoundingMetrics does not necessarily include content of <mpadded>
  // elements whose mBoundingMetrics may not be representative of the true
  // bounds, and doesn't include the CSS2 outline rectangles of children, so
  // make such to include child overflow areas.
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    ConsiderChildOverflow(aMetrics->mOverflowAreas, childFrame);
    childFrame = childFrame->GetNextSibling();
  }

  FinishAndStoreOverflow(aMetrics);
}

nsresult 
nsMathMLContainerFrame::ReflowChild(nsIFrame*                aChildFrame,
                                    nsPresContext*           aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aReflowState,
                                    nsReflowStatus&          aStatus)
{
  // Having foreign/hybrid children, e.g., from html markups, is not defined by
  // the MathML spec. But it can happen in practice, e.g., <html:img> allows us
  // to do some cool demos... or we may have a child that is an nsInlineFrame
  // from a generated content such as :before { content: open-quote } or 
  // :after { content: close-quote }. Unfortunately, the other frames out-there
  // may expect their own invariants that are not met when we mix things.
  // Hence we do not claim their support, but we will nevertheless attempt to keep
  // them in the flow, if we can get their desired size. We observed that most
  // frames may be reflowed generically, but nsInlineFrames need extra care.

#ifdef DEBUG
  nsInlineFrame* inlineFrame = do_QueryFrame(aChildFrame);
  NS_ASSERTION(!inlineFrame, "Inline frames should be wrapped in blocks");
#endif
  
  nsresult rv = nsHTMLContainerFrame::
         ReflowChild(aChildFrame, aPresContext, aDesiredSize, aReflowState,
                     0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);

  if (NS_FAILED(rv))
    return rv;

  if (aDesiredSize.ascent == nsHTMLReflowMetrics::ASK_FOR_BASELINE) {
    // This will be suitable for inline frames, which are wrapped in a block.
    if(!nsLayoutUtils::GetLastLineBaseline(aChildFrame,
                                           &aDesiredSize.ascent)) {
      // We don't expect any other block children so just place the frame on
      // the baseline instead of going through DidReflow() and
      // GetBaseline().  This is what nsFrame::GetBaseline() will do anyway.
      aDesiredSize.ascent = aDesiredSize.height;
    }
  }
  if (IsForeignChild(aChildFrame)) {
    // use ComputeTightBounds API as aDesiredSize.mBoundingMetrics is not set.
    nsRect r = aChildFrame->ComputeTightBounds(aReflowState.rendContext->ThebesContext());
    aDesiredSize.mBoundingMetrics.leftBearing = r.x;
    aDesiredSize.mBoundingMetrics.rightBearing = r.XMost();
    aDesiredSize.mBoundingMetrics.ascent = aDesiredSize.ascent - r.y;
    aDesiredSize.mBoundingMetrics.descent = r.YMost() - aDesiredSize.ascent;
    aDesiredSize.mBoundingMetrics.width = aDesiredSize.width;
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLContainerFrame::Reflow(nsPresContext*           aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus&          aStatus)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.mBoundingMetrics = nsBoundingMetrics();

  /////////////
  // Reflow children
  // Asking each child to cache its bounding metrics

  nsReflowStatus childStatus;
  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsHTMLReflowMetrics childDesiredSize(aDesiredSize.mFlags);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    nsresult rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                              childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) {
      // Call DidReflow() for the child frames we successfully did reflow.
      DidReflowChildren(mFrames.FirstChild(), childFrame);
      return rv;
    }

    SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                    childDesiredSize.mBoundingMetrics);
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
    GetPreferredStretchSize(*aReflowState.rendContext, 0, stretchDir,
                            containerSize);

    // fire the stretch on each child
    childFrame = mFrames.FirstChild();
    while (childFrame) {
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(childFrame);
      if (mathMLFrame) {
        // retrieve the metrics that was stored at the previous pass
        nsHTMLReflowMetrics childDesiredSize;
        GetReflowAndBoundingMetricsFor(childFrame,
          childDesiredSize, childDesiredSize.mBoundingMetrics);

        mathMLFrame->Stretch(*aReflowState.rendContext, stretchDir,
                             containerSize, childDesiredSize);
        // store the updated metrics
        SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                        childDesiredSize.mBoundingMetrics);
      }
      childFrame = childFrame->GetNextSibling();
    }
  }

  /////////////
  // Place children now by re-adjusting the origins to align the baselines
  FinalizeReflow(*aReflowState.rendContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

/* virtual */ nscoord
nsMathMLContainerFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  result = GetIntrinsicWidth(aRenderingContext);
  return result;
}

/* virtual */ nscoord
nsMathMLContainerFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  result = GetIntrinsicWidth(aRenderingContext);
  return result;
}

/* virtual */ nscoord
nsMathMLContainerFrame::GetIntrinsicWidth(nsRenderingContext* aRenderingContext)
{
  // Get child widths
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    // XXX This includes margin while Reflow currently doesn't consider
    // margin, so we may end up with too much space, but, with stretchy
    // characters, this is an approximation anyway.
    nscoord width =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext, childFrame,
                                           nsLayoutUtils::PREF_WIDTH);

    nsHTMLReflowMetrics childDesiredSize;
    childDesiredSize.width = width;
    childDesiredSize.mBoundingMetrics.width = width;
    // TODO: we need nsIFrame::GetIntrinsicHBounds() for better values here.
    childDesiredSize.mBoundingMetrics.leftBearing = 0;
    childDesiredSize.mBoundingMetrics.rightBearing = width;

    SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                    childDesiredSize.mBoundingMetrics);

    childFrame = childFrame->GetNextSibling();
  }

  // Measure
  nsHTMLReflowMetrics desiredSize;
  nsresult rv = MeasureForWidth(*aRenderingContext, desiredSize);
  if (NS_FAILED(rv)) {
    ReflowError(*aRenderingContext, desiredSize);
  }

  ClearSavedChildMetrics();

  return desiredSize.width;
}

/* virtual */ nsresult
nsMathMLContainerFrame::MeasureForWidth(nsRenderingContext& aRenderingContext,
                                        nsHTMLReflowMetrics& aDesiredSize)
{
  return Place(aRenderingContext, PR_FALSE, aDesiredSize);
}


// see spacing table in Chapter 18, TeXBook (p.170)
// Our table isn't quite identical to TeX because operators have 
// built-in values for lspace & rspace in the Operator Dictionary.
static PRInt32 kInterFrameSpacingTable[eMathMLFrameType_COUNT][eMathMLFrameType_COUNT] =
{
  // in units of muspace.
  // upper half of the byte is set if the
  // spacing is not to be used for scriptlevel > 0

  /*           Ord  OpOrd OpInv OpUsr Inner Italic Upright */
  /*Ord  */   {0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00},
  /*OpOrd*/   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  /*OpInv*/   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  /*OpUsr*/   {0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01},
  /*Inner*/   {0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01},
  /*Italic*/  {0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01},
  /*Upright*/ {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01}
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
                     eMathMLFrameType  aFirstFrameType,
                     eMathMLFrameType  aSecondFrameType,
                     eMathMLFrameType* aFromFrameType, // IN/OUT
                     PRInt32*          aCarrySpace)    // IN/OUT
{
  eMathMLFrameType firstType = aFirstFrameType;
  eMathMLFrameType secondType = aSecondFrameType;

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

    firstType = *aFromFrameType;

    // But... the invisible operator that we encountered earlier could
    // be sitting between italic and upright identifiers, e.g.,
    //
    // 1. <mi>sin</mi> <mo>&ApplyFunction;</mo> <mi>x</mi>
    // 2. <mi>x</mi> <mo>&InvisibileTime;</mo> <mi>sin</mi>
    //
    // the trick to get the inter-space in either situation
    // is to promote "<mi>sin</mi><mo>&ApplyFunction;</mo>" and
    // "<mo>&InvisibileTime;</mo><mi>sin</mi>" to user-defined operators...
    if (firstType == eMathMLFrameType_UprightIdentifier) {
      firstType = eMathMLFrameType_OperatorUserDefined;
    }
    else if (secondType == eMathMLFrameType_UprightIdentifier) {
      secondType = eMathMLFrameType_OperatorUserDefined;
    }

    GET_INTERSPACE(aScriptLevel, firstType, secondType, space);

    // Now, we have two values: the computed space and the space that
    // has been carried forward until now. Which value do we pick?
    // If the second type is an operator (e.g., fence), it already has
    // built-in lspace & rspace, so we let them win. Otherwise we pick
    // the max between the two values that we have.
    if (secondType != eMathMLFrameType_OperatorOrdinary &&
        space < *aCarrySpace)
      space = *aCarrySpace;

    // reset everything now that the carry-forward is done
    *aFromFrameType = eMathMLFrameType_UNKNOWN;
    *aCarrySpace = 0;
  }

  return space;
}

static nscoord GetThinSpace(const nsStyleFont* aStyleFont)
{
  return NSToCoordRound(float(aStyleFont->mFont.size)*float(3) / float(18));
}

class nsMathMLContainerFrame::RowChildFrameIterator {
public:
  explicit RowChildFrameIterator(nsMathMLContainerFrame* aParentFrame) :
    mParentFrame(aParentFrame),
    mChildFrame(aParentFrame->mFrames.FirstChild()),
    mX(0),
    mCarrySpace(0),
    mFromFrameType(eMathMLFrameType_UNKNOWN)
  {
    if (!mChildFrame)
      return;

    InitMetricsForChild();
    // Remove left correction in <msqrt> because the sqrt glyph itself is
    // there first.
    if (mParentFrame->GetContent()->Tag() == nsGkAtoms::msqrt_) {
      mX = 0;
    }
  }

  RowChildFrameIterator& operator++()
  {
    // add child size + italic correction
    mX += mSize.mBoundingMetrics.width + mItalicCorrection;

    mChildFrame = mChildFrame->GetNextSibling();
    if (!mChildFrame)
      return *this;

    eMathMLFrameType prevFrameType = mChildFrameType;
    InitMetricsForChild();

    // add inter frame spacing
    const nsStyleFont* font = mParentFrame->GetStyleFont();
    nscoord space =
      GetInterFrameSpacing(font->mScriptLevel,
                           prevFrameType, mChildFrameType,
                           &mFromFrameType, &mCarrySpace);
    mX += space * GetThinSpace(font);
    return *this;
  }

  nsIFrame* Frame() const { return mChildFrame; }
  nscoord X() const { return mX; }
  const nsHTMLReflowMetrics& ReflowMetrics() const { return mSize; }
  nscoord Ascent() const { return mSize.ascent; }
  nscoord Descent() const { return mSize.height - mSize.ascent; }
  const nsBoundingMetrics& BoundingMetrics() const {
    return mSize.mBoundingMetrics;
  }

private:
  const nsMathMLContainerFrame* mParentFrame;
  nsIFrame* mChildFrame;
  nsHTMLReflowMetrics mSize;
  nscoord mX;

  nscoord mItalicCorrection;
  eMathMLFrameType mChildFrameType;
  PRInt32 mCarrySpace;
  eMathMLFrameType mFromFrameType;

  void InitMetricsForChild()
  {
    GetReflowAndBoundingMetricsFor(mChildFrame, mSize, mSize.mBoundingMetrics,
                                   &mChildFrameType);
    nscoord leftCorrection;
    GetItalicCorrection(mSize.mBoundingMetrics, leftCorrection,
                        mItalicCorrection);
    // add left correction -- this fixes the problem of the italic 'f'
    // e.g., <mo>q</mo> <mi>f</mi> <mo>I</mo> 
    mX += leftCorrection;
  }
};

/* virtual */ nsresult
nsMathMLContainerFrame::Place(nsRenderingContext& aRenderingContext,
                              PRBool               aPlaceOrigin,
                              nsHTMLReflowMetrics& aDesiredSize)
{
  // This is needed in case this frame is empty (i.e., no child frames)
  mBoundingMetrics = nsBoundingMetrics();

  RowChildFrameIterator child(this);
  nscoord ascent = 0, descent = 0;
  while (child.Frame()) {
    if (descent < child.Descent())
      descent = child.Descent();
    if (ascent < child.Ascent())
      ascent = child.Ascent();
    // add the child size
    mBoundingMetrics.width = child.X();
    mBoundingMetrics += child.BoundingMetrics();
    ++child;
  }
  // Add the italic correction at the end (including the last child).
  // This gives a nice gap between math and non-math frames, and still
  // gives the same math inter-spacing in case this frame connects to
  // another math frame
  mBoundingMetrics.width = child.X();

  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.height = ascent + descent;
  aDesiredSize.ascent = ascent;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children

  if (aPlaceOrigin) {
    PositionRowChildFrames(0, aDesiredSize.ascent);
  }

  return NS_OK;
}

void
nsMathMLContainerFrame::PositionRowChildFrames(nscoord aOffsetX,
                                               nscoord aBaseline)
{
  RowChildFrameIterator child(this);
  while (child.Frame()) {
    nscoord dx = aOffsetX + child.X();
    nscoord dy = aBaseline - child.Ascent();
    FinishReflowChild(child.Frame(), PresContext(), nsnull,
                      child.ReflowMetrics(), dx, dy, 0);
    ++child;
  }
}

class ForceReflow : public nsIReflowCallback {
public:
  virtual PRBool ReflowFinished() {
    return PR_TRUE;
  }
  virtual void ReflowCallbackCanceled() {}
};

// We only need one of these so we just make it a static global, no need
// to dynamically allocate/destroy it.
static ForceReflow gForceReflow;

void
nsMathMLContainerFrame::SetIncrementScriptLevel(PRInt32 aChildIndex, PRBool aIncrement)
{
  nsIFrame* child = PrincipalChildList().FrameAt(aChildIndex);
  if (!child)
    return;
  nsIContent* content = child->GetContent();
  if (!content->IsMathML())
    return;
  nsMathMLElement* element = static_cast<nsMathMLElement*>(content);

  if (element->GetIncrementScriptLevel() == aIncrement)
    return;

  // XXXroc this does a ContentStatesChanged, is it safe to call here? If
  // not we should do it in a post-reflow callback.
  element->SetIncrementScriptLevel(aIncrement, PR_TRUE);
  PresContext()->PresShell()->PostReflowCallback(&gForceReflow);
}

// helpers to fix the inter-spacing when <math> is the only parent
// e.g., it fixes <math> <mi>f</mi> <mo>q</mo> <mi>f</mi> <mo>I</mo> </math>

static nscoord
GetInterFrameSpacingFor(PRInt32         aScriptLevel,
                        nsIFrame*       aParentFrame,
                        nsIFrame*       aChildFrame)
{
  nsIFrame* childFrame = aParentFrame->GetFirstPrincipalChild();
  if (!childFrame || aChildFrame == childFrame)
    return 0;

  PRInt32 carrySpace = 0;
  eMathMLFrameType fromFrameType = eMathMLFrameType_UNKNOWN;
  eMathMLFrameType prevFrameType = eMathMLFrameType_UNKNOWN;
  eMathMLFrameType childFrameType = nsMathMLFrame::GetMathMLFrameTypeFor(childFrame);
  childFrame = childFrame->GetNextSibling();
  while (childFrame) {
    prevFrameType = childFrameType;
    childFrameType = nsMathMLFrame::GetMathMLFrameTypeFor(childFrame);
    nscoord space = GetInterFrameSpacing(aScriptLevel,
      prevFrameType, childFrameType, &fromFrameType, &carrySpace);
    if (aChildFrame == childFrame) {
      // get thinspace
      nsStyleContext* parentContext = aParentFrame->GetStyleContext();
      nscoord thinSpace = GetThinSpace(parentContext->GetStyleFont());
      // we are done
      return space * thinSpace;
    }
    childFrame = childFrame->GetNextSibling();
  }

  NS_NOTREACHED("child not in the childlist of its parent");
  return 0;
}

nscoord
nsMathMLContainerFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = 0;
  nsIContent* parentContent = mParent->GetContent();
  if (NS_UNLIKELY(!parentContent)) {
    return 0;
  }
  nsIAtom *parentTag = parentContent->Tag();
  if (parentContent->GetNameSpaceID() == kNameSpaceID_MathML && 
      (parentTag == nsGkAtoms::math || parentTag == nsGkAtoms::mtd_)) {
    gap = GetInterFrameSpacingFor(GetStyleFont()->mScriptLevel, mParent, this);
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
  return gap;
}

/* static */ void
nsMathMLContainerFrame::DidReflowChildren(nsIFrame* aFirst, nsIFrame* aStop)

{
  if (NS_UNLIKELY(!aFirst))
    return;

  for (nsIFrame* frame = aFirst;
       frame != aStop;
       frame = frame->GetNextSibling()) {
    NS_ASSERTION(frame, "aStop isn't a sibling");
    if (frame->GetStateBits() & NS_FRAME_IN_REFLOW) {
      // finish off principal descendants, too
      nsIFrame* grandchild = frame->GetFirstPrincipalChild();
      if (grandchild)
        DidReflowChildren(grandchild, nsnull);

      frame->DidReflow(frame->PresContext(), nsnull,
                       NS_FRAME_REFLOW_FINISHED);
    }
  }
}

// helper used by mstyle, mphantom, mpadded and mrow in their implementations
// of TransmitAutomaticData().
nsresult
nsMathMLContainerFrame::TransmitAutomaticDataForMrowLikeElement()
{
  //
  // One loop to check both conditions below:
  //
  // 1) whether all the children of the mrow-like element are space-like.
  //
  //   The REC defines the following elements to be "space-like":
  //   * an mstyle, mphantom, or mpadded element, all of whose direct
  //     sub-expressions are space-like;
  //   * an mrow all of whose direct sub-expressions are space-like.
  //
  // 2) whether all but one child of the mrow-like element are space-like and
  //    this non-space-like child is an embellished operator.
  //
  //   The REC defines the following elements to be embellished operators:
  //   * one of the elements mstyle, mphantom, or mpadded, such that an mrow
  //     containing the same arguments would be an embellished operator;
  //   * an mrow whose arguments consist (in any order) of one embellished
  //     operator and zero or more space-like elements.
  //
  nsIFrame *childFrame, *baseFrame;
  PRBool embellishedOpFound = PR_FALSE;
  nsEmbellishData embellishData;
  
  for (childFrame = GetFirstPrincipalChild();
       childFrame;
       childFrame = childFrame->GetNextSibling()) {
    nsIMathMLFrame* mathMLFrame = do_QueryFrame(childFrame);
    if (!mathMLFrame) break;
    if (!mathMLFrame->IsSpaceLike()) {
      if (embellishedOpFound) break;
      baseFrame = childFrame;
      GetEmbellishDataFrom(baseFrame, embellishData);
      if (!NS_MATHML_IS_EMBELLISH_OPERATOR(embellishData.flags)) break;
      embellishedOpFound = PR_TRUE;
    }
  }

  if (!childFrame) {
    // we successfully went to the end of the loop. This means that one of
    // condition 1) or 2) holds.
    if (!embellishedOpFound) {
      // the mrow-like element is space-like.
      mPresentationData.flags |= NS_MATHML_SPACE_LIKE;
    } else {
      // the mrow-like element is an embellished operator.
      // let the state of the embellished operator found bubble to us.
      mPresentationData.baseFrame = baseFrame;
      mEmbellishData = embellishData;
    }
  }

  if (childFrame || !embellishedOpFound) {
    // The element is not embellished operator
    mPresentationData.baseFrame = nsnull;
    mEmbellishData.flags = 0;
    mEmbellishData.coreFrame = nsnull;
    mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
    mEmbellishData.leftSpace = 0;
    mEmbellishData.rightSpace = 0;
  }

  if (childFrame || embellishedOpFound) {
    // The element is not space-like
    mPresentationData.flags &= ~NS_MATHML_SPACE_LIKE;
  }

  return NS_OK;
}

//==========================

nsIFrame*
NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                           PRUint32 aFlags)
{
  nsMathMLmathBlockFrame* it = new (aPresShell) nsMathMLmathBlockFrame(aContext);
  if (it) {
    it->SetFlags(aFlags);
  }
  return it;
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmathBlockFrame)

nsIFrame*
NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmathInlineFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmathInlineFrame)
