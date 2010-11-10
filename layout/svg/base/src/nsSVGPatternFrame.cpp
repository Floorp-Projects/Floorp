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
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsGkAtoms.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsSVGTransformList.h"
#include "nsStyleContext.h"
#include "nsINameSpaceManager.h"
#include "nsISVGChildFrame.h"
#include "nsSVGMatrix.h"
#include "nsSVGRect.h"
#include "nsSVGUtils.h"
#include "nsSVGEffects.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGPatternElement.h"
#include "nsSVGGeometryFrame.h"
#include "nsSVGPatternFrame.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxPattern.h"
#include "gfxMatrix.h"


//----------------------------------------------------------------------
// Implementation

nsSVGPatternFrame::nsSVGPatternFrame(nsStyleContext* aContext) :
  nsSVGPatternFrameBase(aContext),
  mLoopFlag(PR_FALSE),
  mNoHRefURI(PR_FALSE)
{
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGPatternFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

/* virtual */ void
nsSVGPatternFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsSVGEffects::InvalidateRenderingObservers(this);
  nsSVGPatternFrameBase::DidSetStyleContext(aOldStyleContext);
}

NS_IMETHODIMP
nsSVGPatternFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                    nsIAtom*        aAttribute,
                                    PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::patternUnits ||
       aAttribute == nsGkAtoms::patternContentUnits ||
       aAttribute == nsGkAtoms::patternTransform ||
       aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::width ||
       aAttribute == nsGkAtoms::height ||
       aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox)) {
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  if (aNameSpaceID == kNameSpaceID_XLink &&
      aAttribute == nsGkAtoms::href) {
    // Blow away our reference, if any
    Properties().Delete(nsSVGEffects::HrefProperty());
    mNoHRefURI = PR_FALSE;
    // And update whoever references us
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return nsSVGPatternFrameBase::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}

#ifdef DEBUG
NS_IMETHODIMP
nsSVGPatternFrame::Init(nsIContent* aContent,
                        nsIFrame* aParent,
                        nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(aContent);
  NS_ASSERTION(patternElement, "Content is not an SVG pattern");

  return nsSVGPatternFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom*
nsSVGPatternFrame::GetType() const
{
  return nsGkAtoms::svgPatternFrame;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

// If our GetCanvasTM is getting called, we
// need to return *our current* transformation
// matrix, which depends on our units parameters
// and X, Y, Width, and Height
gfxMatrix
nsSVGPatternFrame::GetCanvasTM()
{
  if (mCTM) {
    return nsSVGUtils::ConvertSVGMatrixToThebes(mCTM);
  }

  // Do we know our rendering parent?
  if (mSource) {
    // Yes, use it!
    return mSource->GetCanvasTM();
  }

  // We get here when geometry in the <pattern> container is updated
  return gfxMatrix();
}

nsresult
nsSVGPatternFrame::PaintPattern(gfxASurface** surface,
                                gfxMatrix* patternMatrix,
                                nsIFrame *aSource,
                                float aGraphicOpacity,
                                const gfxRect *aOverrideBounds)
{
  /*
   * General approach:
   *    Set the content geometry stuff
   *    Calculate our bbox (using x,y,width,height & patternUnits &
   *                        patternTransform)
   *    Create the surface
   *    Calculate the content transformation matrix
   *    Get our children (we may need to get them from another Pattern)
   *    Call SVGPaint on all of our children
   *    Return
   */
  *surface = nsnull;

  // Get the first child of the pattern data we will render
  nsIFrame *firstKid;
  if (NS_FAILED(GetPatternFirstChild(&firstKid)))
    return NS_ERROR_FAILURE; // Either no kids or a bad reference

  /*
   * Get the content geometry information.  This is a little tricky --
   * our parent is probably a <defs>, but we are rendering in the context
   * of some geometry source.  Our content geometry information needs to
   * come from our rendering parent as opposed to our content parent.  We
   * get that information from aSource, which is passed to us from the
   * backend renderer.
   *
   * There are three "geometries" that we need:
   *   1) The bounding box for the pattern.  We use this to get the
   *      width and height for the surface, and as the return to
   *      GetBBox.
   *   2) The transformation matrix for the pattern.  This is not *quite*
   *      the same as the canvas transformation matrix that we will
   *      provide to our rendering children since we "fudge" it a little
   *      to get the renderer to handle the translations correctly for us.
   *   3) The CTM that we return to our children who make up the pattern.
   */

  // Get all of the information we need from our "caller" -- i.e.
  // the geometry that is being rendered with a pattern
  gfxRect callerBBox;
  gfxMatrix callerCTM;
  if (NS_FAILED(GetTargetGeometry(&callerCTM,
                                  &callerBBox,
                                  aSource,
                                  aOverrideBounds)))
    return NS_ERROR_FAILURE;

  // Construct the CTM that we will provide to our children when we
  // render them into the tile.
  gfxMatrix ctm = ConstructCTM(callerBBox, callerCTM, aSource);
  if (ctm.IsSingular()) {
    return NS_ERROR_FAILURE;
  }

  // Get the pattern we are going to render
  nsSVGPatternFrame *patternFrame =
    static_cast<nsSVGPatternFrame*>(firstKid->GetParent());
  patternFrame->mCTM = NS_NewSVGMatrix(ctm);

  // Get the bounding box of the pattern.  This will be used to determine
  // the size of the surface, and will also be used to define the bounding
  // box for the pattern tile.
  gfxRect bbox = GetPatternRect(callerBBox, callerCTM, aSource);

  // Get the transformation matrix that we will hand to the renderer's pattern
  // routine.
  *patternMatrix = GetPatternMatrix(bbox, callerBBox, callerCTM);

  // Now that we have all of the necessary geometries, we can
  // create our surface.
  float patternWidth = bbox.Width();
  float patternHeight = bbox.Height();

  PRBool resultOverflows;
  gfxIntSize surfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(gfxSize(patternWidth, patternHeight),
                                     &resultOverflows);

  // 0 disables rendering, < 0 is an error
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0)
    return NS_ERROR_FAILURE;

  if (resultOverflows ||
      patternWidth != surfaceSize.width ||
      patternHeight != surfaceSize.height) {
    // scale drawing to pattern surface size
    nsCOMPtr<nsIDOMSVGMatrix> tempTM, aCTM;
    NS_NewSVGMatrix(getter_AddRefs(tempTM),
                    surfaceSize.width / patternWidth, 0.0f,
                    0.0f, surfaceSize.height / patternHeight,
                    0.0f, 0.0f);
    patternFrame->mCTM->Multiply(tempTM, getter_AddRefs(aCTM));
    aCTM.swap(patternFrame->mCTM);

    // and rescale pattern to compensate
    patternMatrix->Scale(patternWidth / surfaceSize.width,
                         patternHeight / surfaceSize.height);
  }

  nsRefPtr<gfxASurface> tmpSurface =
    gfxPlatform::GetPlatform()->CreateOffscreenSurface(surfaceSize,
                                                       gfxASurface::CONTENT_COLOR_ALPHA);
  if (!tmpSurface || tmpSurface->CairoStatus())
    return NS_ERROR_FAILURE;

  nsSVGRenderState tmpState(tmpSurface);
  gfxContext* tmpContext = tmpState.GetGfxContext();

  // Fill with transparent black
  tmpContext->SetOperator(gfxContext::OPERATOR_CLEAR);
  tmpContext->Paint();
  tmpContext->SetOperator(gfxContext::OPERATOR_OVER);

  if (aGraphicOpacity != 1.0f) {
    tmpContext->Save();
    tmpContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
  }

  // OK, now render -- note that we use "firstKid", which
  // we got at the beginning because it takes care of the
  // referenced pattern situation for us

  if (aSource->IsFrameOfType(nsIFrame::eSVGGeometry)) {
    // Set the geometrical parent of the pattern we are rendering
    patternFrame->mSource = static_cast<nsSVGGeometryFrame*>(aSource);
  }

  // Delay checking NS_FRAME_DRAWING_AS_PAINTSERVER bit until here so we can
  // give back a clear surface if there's a loop
  if (!(patternFrame->GetStateBits() & NS_FRAME_DRAWING_AS_PAINTSERVER)) {
    patternFrame->AddStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
    for (nsIFrame* kid = firstKid; kid;
         kid = kid->GetNextSibling()) {
      nsSVGUtils::PaintFrameWithEffects(&tmpState, nsnull, kid);
    }
    patternFrame->RemoveStateBits(NS_FRAME_DRAWING_AS_PAINTSERVER);
  }

  patternFrame->mSource = nsnull;

  if (aGraphicOpacity != 1.0f) {
    tmpContext->PopGroupToSource();
    tmpContext->Paint(aGraphicOpacity);
    tmpContext->Restore();
  }

  // caller now owns the surface
  tmpSurface.swap(*surface);
  return NS_OK;
}

/* Will probably need something like this... */
// How do we handle the insertion of a new frame?
// We really don't want to rerender this every time,
// do we?
NS_IMETHODIMP
nsSVGPatternFrame::GetPatternFirstChild(nsIFrame **kid)
{
  // Do we have any children ourselves?
  *kid = mFrames.FirstChild();
  if (*kid)
    return NS_OK;

  // No, see if we chain to someone who does
  nsSVGPatternFrame *next = GetReferencedPattern();

  mLoopFlag = PR_TRUE;
  if (!next || next->mLoopFlag) {
    mLoopFlag = PR_FALSE;
    return NS_ERROR_FAILURE;
  }

  nsresult rv = next->GetPatternFirstChild(kid);
  mLoopFlag = PR_FALSE;
  return rv;
}

PRUint16
nsSVGPatternFrame::GetPatternUnits()
{
  // See if we need to get the value from another pattern
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::patternUnits, mContent);
  return patternElement->mEnumAttributes[nsSVGPatternElement::PATTERNUNITS].GetAnimValue();
}

PRUint16
nsSVGPatternFrame::GetPatternContentUnits()
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::patternContentUnits, mContent);
  return patternElement->mEnumAttributes[nsSVGPatternElement::PATTERNCONTENTUNITS].GetAnimValue();
}

gfxMatrix
nsSVGPatternFrame::GetPatternTransform()
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::patternTransform, mContent);

  static const gfxMatrix identityMatrix;
  if (!patternElement->mPatternTransform) {
    return identityMatrix;
  }
  nsCOMPtr<nsIDOMSVGTransformList> lTrans;
  patternElement->mPatternTransform->GetAnimVal(getter_AddRefs(lTrans));
  nsCOMPtr<nsIDOMSVGMatrix> patternTransform =
    nsSVGTransformList::GetConsolidationMatrix(lTrans);
  if (!patternTransform) {
    return identityMatrix;
  }
  return nsSVGUtils::ConvertSVGMatrixToThebes(patternTransform);
}

const nsSVGViewBox &
nsSVGPatternFrame::GetViewBox()
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::viewBox, mContent);

  return patternElement->mViewBox;
}

const nsSVGPreserveAspectRatio &
nsSVGPatternFrame::GetPreserveAspectRatio()
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::preserveAspectRatio, mContent);

  return patternElement->mPreserveAspectRatio;
}

const nsSVGLength2 *
nsSVGPatternFrame::GetX()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::x, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::X];
}

const nsSVGLength2 *
nsSVGPatternFrame::GetY()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::y, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::Y];
}

const nsSVGLength2 *
nsSVGPatternFrame::GetWidth()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::width, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::WIDTH];
}

const nsSVGLength2 *
nsSVGPatternFrame::GetHeight()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::height, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::HEIGHT];
}

// Private (helper) methods
nsSVGPatternFrame *
nsSVGPatternFrame::GetReferencedPattern()
{
  if (mNoHRefURI)
    return nsnull;

  nsSVGPaintingProperty *property = static_cast<nsSVGPaintingProperty*>
    (Properties().Get(nsSVGEffects::HrefProperty()));

  if (!property) {
    // Fetch our pattern element's xlink:href attribute
    nsSVGPatternElement *pattern = static_cast<nsSVGPatternElement *>(mContent);
    nsAutoString href;
    pattern->mStringAttributes[nsSVGPatternElement::HREF].GetAnimValue(href, pattern);
    if (href.IsEmpty()) {
      mNoHRefURI = PR_TRUE;
      return nsnull; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property =
      nsSVGEffects::GetPaintingProperty(targetURI, this, nsSVGEffects::HrefProperty());
    if (!property)
      return nsnull;
  }

  nsIFrame *result = property->GetReferencedFrame();
  if (!result)
    return nsnull;

  nsIAtom* frameType = result->GetType();
  if (frameType != nsGkAtoms::svgPatternFrame)
    return nsnull;

  return static_cast<nsSVGPatternFrame*>(result);
}

nsSVGPatternElement *
nsSVGPatternFrame::GetPatternWithAttr(nsIAtom *aAttrName, nsIContent *aDefault)
{
  // XXX TODO: this method needs to take account of SMIL animation, since it
  // the requested attribute may be animated even if it is not set in the DOM.
  // The callers also need to be fixed up to then ask for the right thing from
  // the pattern we return! Do we neet to call mContent->FlushAnimations()?

  if (mContent->HasAttr(kNameSpaceID_None, aAttrName))
    return static_cast<nsSVGPatternElement *>(mContent);

  nsSVGPatternElement *pattern = static_cast<nsSVGPatternElement *>(aDefault);

  nsSVGPatternFrame *next = GetReferencedPattern();
  if (!next)
    return pattern;

  // Set mLoopFlag before checking mNextGrad->mLoopFlag in case we are mNextGrad
  mLoopFlag = PR_TRUE;
  // XXXjwatt: we should really send an error to the JavaScript Console here:
  NS_WARN_IF_FALSE(!next->mLoopFlag, "gradient reference loop detected "
                                     "while inheriting attribute!");
  if (!next->mLoopFlag)
    pattern = next->GetPatternWithAttr(aAttrName, aDefault);
  mLoopFlag = PR_FALSE;

  return pattern;
}

// -------------------------------------------------------------------------
// Helper functions
// -------------------------------------------------------------------------

gfxRect
nsSVGPatternFrame::GetPatternRect(const gfxRect &aTargetBBox,
                                  const gfxMatrix &aTargetCTM,
                                  nsIFrame *aTarget)
{
  // Get our type
  PRUint16 type = GetPatternUnits();

  // We need to initialize our box
  float x,y,width,height;

  // Get the pattern x,y,width, and height
  const nsSVGLength2 *tmpX, *tmpY, *tmpHeight, *tmpWidth;
  tmpX = GetX();
  tmpY = GetY();
  tmpHeight = GetHeight();
  tmpWidth = GetWidth();

  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = nsSVGUtils::ObjectSpace(aTargetBBox, tmpX);
    y = nsSVGUtils::ObjectSpace(aTargetBBox, tmpY);
    width = nsSVGUtils::ObjectSpace(aTargetBBox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(aTargetBBox, tmpHeight);
  } else {
    float scale = nsSVGUtils::MaxExpansion(aTargetCTM);
    x = nsSVGUtils::UserSpace(aTarget, tmpX) * scale;
    y = nsSVGUtils::UserSpace(aTarget, tmpY) * scale;
    width = nsSVGUtils::UserSpace(aTarget, tmpWidth) * scale;
    height = nsSVGUtils::UserSpace(aTarget, tmpHeight) * scale;
  }

  return gfxRect(x, y, width, height);
}

gfxMatrix
nsSVGPatternFrame::ConstructCTM(const gfxRect &callerBBox,
                                const gfxMatrix &callerCTM,
                                nsIFrame *aTarget)
{
  gfxMatrix tCTM;
  nsSVGSVGElement *ctx = nsnull;
  nsIContent* targetContent = aTarget->GetContent();

  // The objectBoundingBox conversion must be handled in the CTM:
  if (GetPatternContentUnits() ==
      nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    tCTM.Scale(callerBBox.Width(), callerBBox.Height());
  } else {
    if (targetContent->IsSVG()) {
      ctx = static_cast<nsSVGElement*>(targetContent)->GetCtx();
    }
    float scale = nsSVGUtils::MaxExpansion(callerCTM);
    tCTM.Scale(scale, scale);
  }

  nsSVGPatternElement *patternElement =
    static_cast<nsSVGPatternElement*>(mContent);
  gfxMatrix tm;
  const nsSVGViewBoxRect viewBox = GetViewBox().GetAnimValue();

  if (viewBox.height > 0.0f && viewBox.width > 0.0f) {
    float viewportWidth, viewportHeight, refX, refY;
    if (targetContent->IsSVG()) {
      // If we're dealing with an SVG target only retrieve the context once.
      // Calling the nsIFrame* variant of GetAnimValue would look it up on
      // every call.
      viewportWidth = GetWidth()->GetAnimValue(ctx);
      viewportHeight = GetHeight()->GetAnimValue(ctx);
      refX = GetX()->GetAnimValue(ctx);
      refY = GetY()->GetAnimValue(ctx);
    } else {
      // No SVG target, call the nsIFrame* variant of GetAnimValue.
      viewportWidth = GetWidth()->GetAnimValue(aTarget);
      viewportHeight = GetHeight()->GetAnimValue(aTarget);
      refX = GetX()->GetAnimValue(aTarget);
      refY = GetY()->GetAnimValue(aTarget);
    }
    gfxMatrix viewBoxTM = nsSVGUtils::GetViewBoxTransform(patternElement,
                                                          viewportWidth, viewportHeight,
                                                          viewBox.x, viewBox.y,
                                                          viewBox.width, viewBox.height,
                                                          GetPreserveAspectRatio());

    gfxPoint ref = viewBoxTM.Transform(gfxPoint(refX, refY));

    tm = viewBoxTM * gfxMatrix().Translate(gfxPoint(-ref.x, -ref.y));
  }
  return tm * tCTM;
}

gfxMatrix
nsSVGPatternFrame::GetPatternMatrix(const gfxRect &bbox,
                                    const gfxRect &callerBBox,
                                    const gfxMatrix &callerCTM)
{
  // Get the pattern transform
  gfxMatrix patternTransform = GetPatternTransform();

  // We really want the pattern matrix to handle translations
  float minx = bbox.X();
  float miny = bbox.Y();

  PRUint16 type = GetPatternContentUnits();
  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    minx += callerBBox.X();
    miny += callerBBox.Y();
  }

  float scale = 1.0f / nsSVGUtils::MaxExpansion(callerCTM);
  patternTransform.Scale(scale, scale);
  patternTransform.Translate(gfxPoint(minx, miny));

  return patternTransform;
}

nsresult
nsSVGPatternFrame::GetTargetGeometry(gfxMatrix *aCTM,
                                     gfxRect *aBBox,
                                     nsIFrame *aTarget,
                                     const gfxRect *aOverrideBounds)
{
  *aBBox = aOverrideBounds ? *aOverrideBounds : nsSVGUtils::GetBBox(aTarget);

  // Sanity check
  PRUint16 type = GetPatternUnits();
  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    if (aBBox->Width() <= 0 || aBBox->Height() <= 0) {
      return NS_ERROR_FAILURE;
    }
  }

  // Get the transformation matrix from our calling geometry
  *aCTM = nsSVGUtils::GetCanvasTM(aTarget);

  // OK, now fix up the bounding box to reflect user coordinates
  // We handle device unit scaling in pattern matrix
  {
    float scale = nsSVGUtils::MaxExpansion(*aCTM);
    if (scale <= 0) {
      return NS_ERROR_FAILURE;
    }
    aBBox->Scale(scale);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGPaintServerFrame methods:

already_AddRefed<gfxPattern>
nsSVGPatternFrame::GetPaintServerPattern(nsIFrame *aSource,
                                         float aGraphicOpacity,
                                         const gfxRect *aOverrideBounds)
{
  if (aGraphicOpacity == 0.0f) {
    nsRefPtr<gfxPattern> pattern = new gfxPattern(gfxRGBA(0, 0, 0, 0));
    return pattern.forget();
  }

  // Paint it!
  nsRefPtr<gfxASurface> surface;
  gfxMatrix pMatrix;
  nsresult rv = PaintPattern(getter_AddRefs(surface), &pMatrix,
                             aSource, aGraphicOpacity, aOverrideBounds);

  if (NS_FAILED(rv)) {
    return nsnull;
  }

  if (pMatrix.IsSingular()) {
    return nsnull;
  }

  pMatrix.Invert();

  nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);

  if (!pattern || pattern->CairoStatus())
    return nsnull;

  pattern->SetMatrix(pMatrix);
  pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
  return pattern.forget();
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(nsIPresShell*   aPresShell,
                                nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGPatternFrame(aContext);
}

