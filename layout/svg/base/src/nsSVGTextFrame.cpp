/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGTextFrame.h"

// Keep others in (case-insensitive) order:
#include "nsGkAtoms.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGTextElement.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGGraphicElement.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGPathElement.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGUtils.h"
#include "SVGLengthList.h"

using namespace mozilla;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGTextFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGTextFrame)

//----------------------------------------------------------------------
// nsIFrame methods
#ifdef DEBUG
NS_IMETHODIMP
nsSVGTextFrame::Init(nsIContent* aContent,
                     nsIFrame* aParent,
                     nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGTextElement> text = do_QueryInterface(aContent);
  NS_ASSERTION(text, "Content is not an SVG text");

  return nsSVGTextFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

NS_IMETHODIMP
nsSVGTextFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  if (aNameSpaceID != kNameSpaceID_None)
    return NS_OK;

  if (aAttribute == nsGkAtoms::transform) {
    nsSVGUtils::InvalidateAndScheduleReflowSVG(this);
    NotifySVGChanged(TRANSFORM_CHANGED);
  } else if (aAttribute == nsGkAtoms::x ||
             aAttribute == nsGkAtoms::y ||
             aAttribute == nsGkAtoms::dx ||
             aAttribute == nsGkAtoms::dy ||
             aAttribute == nsGkAtoms::rotate) {
    nsSVGUtils::InvalidateAndScheduleReflowSVG(this);
    NotifyGlyphMetricsChange();
  }

 return NS_OK;
}

nsIAtom *
nsSVGTextFrame::GetType() const
{
  return nsGkAtoms::svgTextFrame;
}

NS_IMETHODIMP
nsSVGTextFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists)
{
  UpdateGlyphPositioning(true);
  return nsSVGTextFrameBase::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

//----------------------------------------------------------------------
// nsSVGTextContainerFrame
PRUint32
nsSVGTextFrame::GetNumberOfChars()
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetNumberOfChars();
}

float
nsSVGTextFrame::GetComputedTextLength()
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetComputedTextLength();
}

float
nsSVGTextFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetSubStringLength(charnum, nchars);
}

PRInt32
nsSVGTextFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetCharNumAtPosition(point);
}

NS_IMETHODIMP
nsSVGTextFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetStartPositionOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetEndPositionOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetExtentOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  UpdateGlyphPositioning(false);

  return nsSVGTextFrameBase::GetRotationOfChar(charnum,  _retval);
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGTextFrame::NotifySVGChanged(PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  bool updateGlyphMetrics = false;
  
  if (aFlags & COORD_CONTEXT_CHANGED) {
    updateGlyphMetrics = true;
  }

  if (aFlags & TRANSFORM_CHANGED) {
    if (mCanvasTM && mCanvasTM->IsSingular()) {
      // We won't have calculated the glyph positions correctly
      updateGlyphMetrics = true;
    }
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nullptr;
  }

  if (updateGlyphMetrics) {
    // Ancestor changes can't affect how we render from the perspective of
    // any rendering observers that we may have, so we don't need to
    // invalidate them. We also don't need to invalidate ourself, since our
    // changed ancestor will have invalidated its entire area, which includes
    // our area.
    // For perf reasons we call this before calling NotifySVGChanged() below.
    nsSVGUtils::ScheduleReflowSVG(this);
  }

  nsSVGTextFrameBase::NotifySVGChanged(aFlags);

  if (updateGlyphMetrics) {
    // If we are positioned using percentage values we need to update our
    // position whenever our viewport's dimensions change.

    // XXX We could check here whether the text frame or any of its children
    // have any percentage co-ordinates and only update if they don't. This
    // may not be worth it as we might need to check each glyph
    NotifyGlyphMetricsChange();
  }
}

NS_IMETHODIMP
nsSVGTextFrame::PaintSVG(nsRenderingContext* aContext,
                         const nsIntRect *aDirtyRect)
{
  NS_ASSERTION(!NS_SVGDisplayListPaintingEnabled() ||
               (mState & NS_STATE_SVG_NONDISPLAY_CHILD),
               "If display lists are enabled, only painting of non-display "
               "SVG should take this code path");

  UpdateGlyphPositioning(true);
  
  return nsSVGTextFrameBase::PaintSVG(aContext, aDirtyRect);
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGTextFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  NS_ASSERTION(!NS_SVGDisplayListHitTestingEnabled() ||
               (mState & NS_STATE_SVG_NONDISPLAY_CHILD),
               "If display lists are enabled, only hit-testing of non-display "
               "SVG should take this code path");

  UpdateGlyphPositioning(true);
  
  return nsSVGTextFrameBase::GetFrameForPoint(aPoint);
}

void
nsSVGTextFrame::ReflowSVG()
{
  NS_ASSERTION(nsSVGUtils::OuterSVGIsCallingReflowSVG(this),
               "This call is probably a wasteful mistake");

  NS_ABORT_IF_FALSE(!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD),
                    "ReflowSVG mechanism not designed for this");

  if (!nsSVGUtils::NeedsReflowSVG(this)) {
    NS_ASSERTION(!mPositioningDirty, "How did this happen?");
    return;
  }

  // UpdateGlyphPositioning may have been called under DOM calls and set
  // mPositioningDirty to false. We may now have better positioning, though, so
  // set it to true so that UpdateGlyphPositioning will do its work.
  mPositioningDirty = true;

  UpdateGlyphPositioning(false);

  // We only invalidate if we are dirty, if our outer-<svg> has already had its
  // initial reflow (since if it hasn't, its entire area will be invalidated
  // when it gets that initial reflow), and if our parent is not dirty (since
  // if it is, then it will invalidate its entire new area, which will include
  // our new area).
  bool invalidate = (mState & NS_FRAME_IS_DIRTY) &&
    !(GetParent()->GetStateBits() &
       (NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY));

  // With glyph positions updated, our descendants can invalidate their new
  // areas correctly:
  nsSVGTextFrameBase::ReflowSVG();

  if (invalidate) {
    // XXXSDL Let FinishAndStoreOverflow do this.
    nsSVGUtils::InvalidateBounds(this, true);
  }
}

SVGBBox
nsSVGTextFrame::GetBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                    PRUint32 aFlags)
{
  UpdateGlyphPositioning(true);

  return nsSVGTextFrameBase::GetBBoxContribution(aToBBoxUserspace, aFlags);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGTextFrame::GetCanvasTM(PRUint32 aFor)
{
  if (!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }

  if (!mCanvasTM) {
    NS_ASSERTION(mParent, "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    nsSVGGraphicElement *content = static_cast<nsSVGGraphicElement*>(mContent);

    gfxMatrix tm =
      content->PrependLocalTransformsTo(parent->GetCanvasTM(aFor));

    mCanvasTM = new gfxMatrix(tm);
  }

  return *mCanvasTM;
}

//----------------------------------------------------------------------
//

static void
MarkDirtyBitsOnDescendants(nsIFrame *aFrame)
{
  // Do not skip marking of aFrame or any of its descendants if they have
  // the NS_FRAME_IS_DIRTY set, because some of their descendants may not
  // have it set, and we need all descendants to be dirty.
  if (aFrame->GetStateBits() & (NS_FRAME_FIRST_REFLOW)) {
    // Nothing to do if our outer-<svg> hasn't yet had its initial reflow.
    return;
  }
  nsIFrame* kid = aFrame->GetFirstPrincipalChild();
  while (kid) {
    nsISVGChildFrame* svgkid = do_QueryFrame(kid);
    if (svgkid) {
      MarkDirtyBitsOnDescendants(kid);
      kid->AddStateBits(NS_FRAME_IS_DIRTY);
    }
    kid = kid->GetNextSibling();
  }
}

void
nsSVGTextFrame::NotifyGlyphMetricsChange()
{
  // NotifySVGChanged isn't appropriate here, so we just mark our descendants
  // as fully dirty to get ReflowSVG() called on them:
  MarkDirtyBitsOnDescendants(this);

  nsSVGUtils::InvalidateAndScheduleReflowSVG(this);

  mPositioningDirty = true;
}

void
nsSVGTextFrame::SetWhitespaceHandling(nsSVGGlyphFrame *aFrame)
{
  SetWhitespaceCompression();

  nsSVGGlyphFrame* firstFrame = aFrame;
  bool trimLeadingWhitespace = true;
  nsSVGGlyphFrame* lastNonWhitespaceFrame = aFrame;

  // If the previous frame ended with whitespace
  // then display of leading whitespace should be suppressed
  // when we are compressing whitespace.
  while (aFrame) {
    if (!aFrame->IsAllWhitespace()) {
      lastNonWhitespaceFrame = aFrame;
    }

    aFrame->SetTrimLeadingWhitespace(trimLeadingWhitespace);
    trimLeadingWhitespace = aFrame->EndsWithWhitespace();

    aFrame = aFrame->GetNextGlyphFrame();
  }

  // When there is only whitespace left we need to trim off
  // the end of the last frame that isn't entirely whitespace.
  // Making sure that we reset earlier frames as they may once
  // have been the last non-whitespace frame.
  aFrame = firstFrame;
  while (aFrame != lastNonWhitespaceFrame) {
    aFrame->SetTrimTrailingWhitespace(false);
    aFrame = aFrame->GetNextGlyphFrame();
  }

  // We're at the last non-whitespace frame so trim off the end
  // and make sure we set one of the trim bits so that any
  // further whitespace is compressed to nothing
  while (aFrame) {
    aFrame->SetTrimTrailingWhitespace(true);
    aFrame = aFrame->GetNextGlyphFrame();
  }
}

void
nsSVGTextFrame::UpdateGlyphPositioning(bool aForceGlobalTransform)
{
  if (!mPositioningDirty)
    return;

  mPositioningDirty = false;

  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node)
    return;

  nsSVGGlyphFrame *frame, *firstFrame;

  firstFrame = node->GetFirstGlyphFrame();
  if (!firstFrame) {
    return;
  }

  SetWhitespaceHandling(firstFrame);

  BuildPositionList(0, 0);

  gfxPoint ctp(0.0, 0.0);

  // loop over chunks
  while (firstFrame) {
    nsSVGTextPathFrame *textPath = firstFrame->FindTextPathParent();

    nsTArray<float> effectiveXList, effectiveYList;
    firstFrame->GetEffectiveXY(firstFrame->GetNumberOfChars(),
                               effectiveXList, effectiveYList);
    if (!effectiveXList.IsEmpty()) ctp.x = effectiveXList[0];
    if (!textPath && !effectiveYList.IsEmpty()) ctp.y = effectiveYList[0];

    // check for startOffset on textPath
    if (textPath) {
      if (!textPath->GetPathFrame()) {
        // invalid text path, give up
        return;
      }
      ctp.x = textPath->GetStartOffset();
    }

    // determine x offset based on text_anchor:
  
    PRUint8 anchor = firstFrame->GetTextAnchor();

    /**
     * XXXsmontagu: The SVG spec is very vague as to how 'text-anchor'
     *  interacts with bidirectional text. It says:
     *
     *   "For scripts that are inherently right to left such as Hebrew and
     *   Arabic [text-anchor: start] is equivalent to right alignment."
     * and
     *   "For scripts that are inherently right to left such as Hebrew and
     *   Arabic, [text-anchor: end] is equivalent to left alignment.
     *
     * It's not clear how this should be implemented in terms of defined
     * properties, i.e. how one should determine that a particular element
     * contains a script that is inherently right to left.
     *
     * The code below follows http://www.w3.org/TR/SVGTiny12/text.html#TextAnchorProperty
     * and swaps the values of text-anchor: end and  text-anchor: start
     * whenever the 'direction' property is rtl.
     *
     * This is probably the "right" thing to do, but other browsers don't do it,
     * so I am leaving it inside #if 0 for now for interoperability.
     *
     * See also XXXsmontagu comments in nsSVGGlyphFrame::EnsureTextRun
     */
#if 0
    if (GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      if (anchor == NS_STYLE_TEXT_ANCHOR_END) {
        anchor = NS_STYLE_TEXT_ANCHOR_START;
      } else if (anchor == NS_STYLE_TEXT_ANCHOR_START) {
        anchor = NS_STYLE_TEXT_ANCHOR_END;
      }
    }
#endif

    float chunkLength = 0.0f;
    if (anchor != NS_STYLE_TEXT_ANCHOR_START) {
      // need to get the total chunk length
    
      frame = firstFrame;
      while (frame) {
        chunkLength += frame->GetAdvance(aForceGlobalTransform);
        frame = frame->GetNextGlyphFrame();
        if (frame && frame->IsAbsolutelyPositioned())
          break;
      }
    }

    if (anchor == NS_STYLE_TEXT_ANCHOR_MIDDLE)
      ctp.x -= chunkLength/2.0f;
    else if (anchor == NS_STYLE_TEXT_ANCHOR_END)
      ctp.x -= chunkLength;
  
    // set position of each frame in this chunk:
  
    frame = firstFrame;
    while (frame) {

      frame->SetGlyphPosition(&ctp, aForceGlobalTransform);
      frame = frame->GetNextGlyphFrame();
      if (frame && frame->IsAbsolutelyPositioned())
        break;
    }
    firstFrame = frame;
  }
}
