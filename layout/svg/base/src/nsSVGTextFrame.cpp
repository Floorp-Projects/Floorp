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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsIDOMSVGTextElement.h"
#include "nsSVGTextFrame.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGLengthList.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGAnimatedNumber.h"
#include "nsISVGGlyphFragmentNode.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsIDOMSVGRect.h"
#include "nsISVGTextContentMetrics.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsGkAtoms.h"
#include "nsSVGTextPathFrame.h"
#include "nsSVGPathElement.h"
#include "nsSVGUtils.h"
#include "nsSVGGraphicElement.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGTextFrame(aContext);
}

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
    // transform has changed

    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;

    nsSVGUtils::NotifyChildrenOfSVGChange(this, TRANSFORM_CHANGED);
   
  } else if (aAttribute == nsGkAtoms::x ||
             aAttribute == nsGkAtoms::y ||
             aAttribute == nsGkAtoms::dx ||
             aAttribute == nsGkAtoms::dy) {
    NotifyGlyphMetricsChange();
  }

 return NS_OK;
}

nsIAtom *
nsSVGTextFrame::GetType() const
{
  return nsGkAtoms::svgTextFrame;
}

//----------------------------------------------------------------------
// nsISVGTextContentMetrics
NS_IMETHODIMP
nsSVGTextFrame::GetNumberOfChars(PRInt32 *_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetNumberOfChars(_retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetComputedTextLength(float *_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetComputedTextLength(_retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetSubStringLength(charnum, nchars, _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetStartPositionOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetEndPositionOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetExtentOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetRotationOfChar(charnum,  _retval);
}

NS_IMETHODIMP
nsSVGTextFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  UpdateGlyphPositioning(PR_FALSE);

  return nsSVGTextFrameBase::GetCharNumAtPosition(point,  _retval);
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGTextFrame::NotifySVGChanged(PRUint32 aFlags)
{
  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nsnull;
  }

  if (aFlags & COORD_CONTEXT_CHANGED) {
    // If we are positioned using percentage values we need to update our
    // position whenever our viewport's dimensions change.

    // XXX We could check here whether the text frame or any of its children
    // have any percentage co-ordinates and only update if they don't. This
    // may not be worth it as we might need to check each glyph
    NotifyGlyphMetricsChange();
  }

  nsSVGTextFrameBase::NotifySVGChanged(aFlags);
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawSuspended()
{
  mMetricsState = suspended;

  return nsSVGTextFrameBase::NotifyRedrawSuspended();
}

NS_IMETHODIMP
nsSVGTextFrame::NotifyRedrawUnsuspended()
{
  mMetricsState = unsuspended;
  UpdateGlyphPositioning(PR_FALSE);
  return nsSVGTextFrameBase::NotifyRedrawUnsuspended();
}

NS_IMETHODIMP
nsSVGTextFrame::PaintSVG(nsSVGRenderState* aContext,
                         const nsIntRect *aDirtyRect)
{
  UpdateGlyphPositioning(PR_TRUE);
  
  return nsSVGTextFrameBase::PaintSVG(aContext, aDirtyRect);
}

NS_IMETHODIMP_(nsIFrame*)
nsSVGTextFrame::GetFrameForPoint(const nsPoint &aPoint)
{
  UpdateGlyphPositioning(PR_TRUE);
  
  return nsSVGTextFrameBase::GetFrameForPoint(aPoint);
}

NS_IMETHODIMP
nsSVGTextFrame::UpdateCoveredRegion()
{
  UpdateGlyphPositioning(PR_TRUE);
  
  return nsSVGTextFrameBase::UpdateCoveredRegion();
}

NS_IMETHODIMP
nsSVGTextFrame::InitialUpdate()
{
  nsresult rv = nsSVGTextFrameBase::InitialUpdate();
  
  UpdateGlyphPositioning(PR_FALSE);

  return rv;
}  

NS_IMETHODIMP
nsSVGTextFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  UpdateGlyphPositioning(PR_TRUE);

  return nsSVGTextFrameBase::GetBBox(_retval);
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGTextFrame::GetCanvasTM()
{
  if (!GetMatrixPropagation()) {
    nsIDOMSVGMatrix *retval;
    NS_NewSVGMatrix(&retval);
    return retval;
  }

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = static_cast<nsSVGContainerFrame*>
                                                     (mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsSVGGraphicElement *element =
      static_cast<nsSVGGraphicElement*>(mContent);
    nsCOMPtr<nsIDOMSVGMatrix> localTM = element->GetLocalTransformMatrix();
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

//----------------------------------------------------------------------
//

void
nsSVGTextFrame::NotifyGlyphMetricsChange()
{
  mPositioningDirty = PR_TRUE;
  UpdateGlyphPositioning(PR_FALSE);
}

static void
GetSingleValue(nsISVGGlyphFragmentLeaf *fragment,
               nsIDOMSVGLengthList *list, float *val)
{
  if (!list)
    return;

  PRUint32 count = 0;
  list->GetNumberOfItems(&count);
#ifdef DEBUG
  if (count > 1)
    NS_WARNING("multiple lengths for x/y attributes on <text> elements not implemented yet!");
#endif
  if (count) {
    nsCOMPtr<nsIDOMSVGLength> length;
    list->GetItem(0, getter_AddRefs(length));
    length->GetValue(val);
  }
}

void
nsSVGTextFrame::UpdateGlyphPositioning(PRBool aForceGlobalTransform)
{
  if (mMetricsState == suspended || !mPositioningDirty)
    return;

  SetWhitespaceHandling();

  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node) return;

  mPositioningDirty = PR_FALSE;

  // we'll align every fragment in this chunk on the dominant-baseline:
  // XXX should actually inspect 'alignment-baseline' for each fragment
  
  PRUint8 baseline;
  switch(GetStyleSVGReset()->mDominantBaseline) {
    case NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_TEXT_BEFORE_EDGE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_TEXT_AFTER_EDGE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_MIDDLE:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_MIDDLE;
      break;
    case NS_STYLE_DOMINANT_BASELINE_CENTRAL:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_CENTRAL;
      break;
    case NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_MATHEMATICAL;
      break;
    case NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_IDEOGRAPHC;
      break;
    case NS_STYLE_DOMINANT_BASELINE_HANGING:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_HANGING;
      break;
    case NS_STYLE_DOMINANT_BASELINE_AUTO:
    case NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT:
    case NS_STYLE_DOMINANT_BASELINE_ALPHABETIC:
    default:
      baseline = nsISVGGlyphFragmentLeaf::BASELINE_ALPHABETIC;
      break;
  }

  nsISVGGlyphFragmentLeaf *fragment, *firstFragment;

  firstFragment = node->GetFirstGlyphFragment();
  if (!firstFragment) {
    return;
  }

  float x = 0, y = 0;

  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetX();
    GetSingleValue(firstFragment, list, &x);
  }
  {
    nsCOMPtr<nsIDOMSVGLengthList> list = GetY();
    GetSingleValue(firstFragment, list, &y);
  }

  // loop over chunks
  while (firstFragment) {
    {
      nsCOMPtr<nsIDOMSVGLengthList> list = firstFragment->GetX();
      GetSingleValue(firstFragment, list, &x);
    }
    {
      nsCOMPtr<nsIDOMSVGLengthList> list = firstFragment->GetY();
      GetSingleValue(firstFragment, list, &y);
    }

    // check for startOffset on textPath
    nsSVGTextPathFrame *textPath = firstFragment->FindTextPathParent();
    if (textPath) {
      if (!textPath->GetPathFrame()) {
        // invalid text path, give up
        return;
      }
      x = textPath->GetStartOffset();
    }

    // determine x offset based on text_anchor:
  
    PRUint8 anchor = firstFragment->GetTextAnchor();

    float chunkLength = 0.0f;
    if (anchor != NS_STYLE_TEXT_ANCHOR_START) {
      // need to get the total chunk length
    
      fragment = firstFragment;
      while (fragment) {
        float dx = 0.0f;
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDx();
        GetSingleValue(fragment, list, &dx);
        chunkLength += dx + fragment->GetAdvance(aForceGlobalTransform);
        fragment = fragment->GetNextGlyphFragment();
        if (fragment && fragment->IsAbsolutelyPositioned())
          break;
      }
    }

    if (anchor == NS_STYLE_TEXT_ANCHOR_MIDDLE)
      x -= chunkLength/2.0f;
    else if (anchor == NS_STYLE_TEXT_ANCHOR_END)
      x -= chunkLength;
  
    // set position of each fragment in this chunk:
  
    fragment = firstFragment;
    while (fragment) {

      float dx = 0.0f, dy = 0.0f;
      {
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDx();
        GetSingleValue(fragment, list, &dx);
      }
      {
        nsCOMPtr<nsIDOMSVGLengthList> list = fragment->GetDy();
        GetSingleValue(fragment, list, &dy);
      }

      float baseline_offset =
        fragment->GetBaselineOffset(baseline, aForceGlobalTransform);
      fragment->SetGlyphPosition(x + dx, y + dy - baseline_offset);

      x += dx + fragment->GetAdvance(aForceGlobalTransform);
      y += dy;
      fragment = fragment->GetNextGlyphFragment();
      if (fragment && fragment->IsAbsolutelyPositioned())
        break;
    }
    firstFragment = fragment;
  }
}
