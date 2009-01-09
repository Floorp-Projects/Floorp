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
#include "nsIDOMSVGRect.h"
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

#ifdef DEBUG_scooter
static void printCTM(char *msg, gfxMatrix aCTM);
static void printCTM(char *msg, nsIDOMSVGMatrix *aCTM);
static void printRect(char *msg, nsIDOMSVGRect *aRect);
#endif

//----------------------------------------------------------------------
// Implementation

nsSVGPatternFrame::nsSVGPatternFrame(nsStyleContext* aContext) :
  nsSVGPatternFrameBase(aContext),
  mLoopFlag(PR_FALSE), mPaintLoopFlag(PR_FALSE),
  mNoHRefURI(PR_FALSE)
{
}

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
    DeleteProperty(nsGkAtoms::href);
    mNoHRefURI = PR_FALSE;
    // And update whoever references us
    nsSVGEffects::InvalidateRenderingObservers(this);
  }

  return nsSVGPatternFrameBase::AttributeChanged(aNameSpaceID,
                                                 aAttribute, aModType);
}

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
already_AddRefed<nsIDOMSVGMatrix>
nsSVGPatternFrame::GetCanvasTM()
{
  nsIDOMSVGMatrix *rCTM;

  if (mCTM) {
    rCTM = mCTM;
    NS_IF_ADDREF(rCTM);
  } else {
    // Do we know our rendering parent?
    if (mSource) {
      // Yes, use it!
      mSource->GetCanvasTM(&rCTM);
    } else {
      // No, return an identity
      // We get here when geometry in the <pattern> container is updated
      NS_NewSVGMatrix(&rCTM);
    }
  }
  return rCTM;
}

nsresult
nsSVGPatternFrame::PaintPattern(gfxASurface** surface,
                                gfxMatrix* patternMatrix,
                                nsSVGGeometryFrame *aSource,
                                float aGraphicOpacity)
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

  // Get our child
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
  nsSVGElement *callerContent;
  nsCOMPtr<nsIDOMSVGRect> callerBBox;
  nsCOMPtr<nsIDOMSVGMatrix> callerCTM;
  if (NS_FAILED(GetCallerGeometry(getter_AddRefs(callerCTM),
                                  getter_AddRefs(callerBBox),
                                  &callerContent, aSource)))
    return NS_ERROR_FAILURE;

  // Construct the CTM that we will provide to our children when we
  // render them into the tile.
  if (NS_FAILED(ConstructCTM(getter_AddRefs(mCTM), callerBBox, callerCTM)))
    return NS_ERROR_FAILURE;

  // Get the bounding box of the pattern.  This will be used to determine
  // the size of the surface, and will also be used to define the bounding
  // box for the pattern tile.
  nsCOMPtr<nsIDOMSVGRect> bbox;
  if (NS_FAILED(GetPatternRect(getter_AddRefs(bbox),
                               callerBBox, callerCTM,
                               callerContent)))
    return NS_ERROR_FAILURE;

  // Get the transformation matrix that we will hand to the renderer's pattern
  // routine.
  *patternMatrix = GetPatternMatrix(bbox, callerBBox, callerCTM);

#ifdef DEBUG_scooter
  printRect("Geometry Rect: ", callerBBox);
  printRect("Pattern Rect: ", bbox);
  printCTM("Pattern TM ", *patternMatrix);
  printCTM("Child TM ", mCTM);
#endif

  // Now that we have all of the necessary geometries, we can
  // create our surface.
  float patternWidth, patternHeight;
  bbox->GetWidth(&patternWidth);
  bbox->GetHeight(&patternHeight);

  PRBool resultOverflows;
  gfxIntSize surfaceSize =
    nsSVGUtils::ConvertToSurfaceSize(gfxSize(patternWidth, patternHeight),
                                     &resultOverflows);

  // 0 disables rendering, < 0 is an error
  if (surfaceSize.width <= 0 || surfaceSize.height <= 0)
    return NS_ERROR_FAILURE;

  if (resultOverflows) {
    // scale down drawing to new pattern surface size
    nsCOMPtr<nsIDOMSVGMatrix> tempTM, aCTM;
    NS_NewSVGMatrix(getter_AddRefs(tempTM),
                    surfaceSize.width / patternWidth, 0.0f,
                    0.0f, surfaceSize.height / patternHeight,
                    0.0f, 0.0f);
    mCTM->Multiply(tempTM, getter_AddRefs(aCTM));
    aCTM.swap(mCTM);

    // and magnify pattern to compensate
    patternMatrix->Scale(patternWidth / surfaceSize.width,
                         patternHeight / surfaceSize.height);
  }

#ifdef DEBUG_scooter
  printf("Creating %dX%d surface\n", int(surfaceSize.width), int(surfaceSize.height));
#endif

  nsRefPtr<gfxASurface> tmpSurface =
    gfxPlatform::GetPlatform()->CreateOffscreenSurface(surfaceSize,
                                                       gfxASurface::ImageFormatARGB32);
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

  // Set our geometrical parent
  mSource = aSource;

  // Delay checking mPaintLoopFlag until here so we can give back a clear
  // surface if there's a loop
  if (!mPaintLoopFlag) {
    mPaintLoopFlag = PR_TRUE;
    for (nsIFrame* kid = firstKid; kid;
         kid = kid->GetNextSibling()) {
      nsSVGUtils::PaintFrameWithEffects(&tmpState, nsnull, kid);
    }
    mPaintLoopFlag = PR_FALSE;
  }

  mSource = nsnull;

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

  gfxMatrix matrix;
  nsCOMPtr<nsIDOMSVGTransformList> lTrans;
  patternElement->mPatternTransform->GetAnimVal(getter_AddRefs(lTrans));
  nsCOMPtr<nsIDOMSVGMatrix> patternTransform =
    nsSVGTransformList::GetConsolidationMatrix(lTrans);
  if (patternTransform) {
    matrix = nsSVGUtils::ConvertSVGMatrixToThebes(patternTransform);
  }
  return matrix;
}

NS_IMETHODIMP
nsSVGPatternFrame::GetViewBox(nsIDOMSVGRect **aViewBox)
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::viewBox, mContent);

  nsCOMPtr<nsIDOMSVGAnimatedRect> viewBox;
  patternElement->GetViewBox(getter_AddRefs(viewBox));
  return viewBox->GetAnimVal(aViewBox);
}

const nsSVGPreserveAspectRatio &
nsSVGPatternFrame::GetPreserveAspectRatio()
{
  nsSVGPatternElement *patternElement =
    GetPatternWithAttr(nsGkAtoms::preserveAspectRatio, mContent);

  return patternElement->mPreserveAspectRatio;
}

nsSVGLength2 *
nsSVGPatternFrame::GetX()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::x, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::X];
}

nsSVGLength2 *
nsSVGPatternFrame::GetY()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::y, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::Y];
}

nsSVGLength2 *
nsSVGPatternFrame::GetWidth()
{
  nsSVGPatternElement *pattern = GetPatternWithAttr(nsGkAtoms::width, mContent);
  return &pattern->mLengthAttributes[nsSVGPatternElement::WIDTH];
}

nsSVGLength2 *
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

  nsSVGPaintingProperty *property =
    static_cast<nsSVGPaintingProperty*>(GetProperty(nsGkAtoms::href));

  if (!property) {
    // Fetch our pattern element's xlink:href attribute
    nsSVGPatternElement *pattern = static_cast<nsSVGPatternElement *>(mContent);
    const nsString &href = pattern->mStringAttributes[nsSVGPatternElement::HREF].GetAnimValue();
    if (href.IsEmpty()) {
      mNoHRefURI = PR_TRUE;
      return nsnull; // no URL
    }

    // Convert href to an nsIURI
    nsCOMPtr<nsIURI> targetURI;
    nsCOMPtr<nsIURI> base = mContent->GetBaseURI();
    nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(targetURI), href,
                                              mContent->GetCurrentDoc(), base);

    property = nsSVGEffects::GetPaintingProperty(targetURI, this, nsGkAtoms::href);
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

nsresult
nsSVGPatternFrame::GetPatternRect(nsIDOMSVGRect **patternRect,
                                  nsIDOMSVGRect *bbox,
                                  nsIDOMSVGMatrix *callerCTM,
                                  nsSVGElement *content)
{
  // Get our type
  PRUint16 type = GetPatternUnits();

  // We need to initialize our box
  float x,y,width,height;

  // Get the pattern x,y,width, and height
  nsSVGLength2 *tmpX, *tmpY, *tmpHeight, *tmpWidth;
  tmpX = GetX();
  tmpY = GetY();
  tmpHeight = GetHeight();
  tmpWidth = GetWidth();

  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    x = nsSVGUtils::ObjectSpace(bbox, tmpX);
    y = nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    float scale = nsSVGUtils::MaxExpansion(callerCTM);
    x = nsSVGUtils::UserSpace(content, tmpX) * scale;
    y = nsSVGUtils::UserSpace(content, tmpY) * scale;
    width = nsSVGUtils::UserSpace(content, tmpWidth) * scale;
    height = nsSVGUtils::UserSpace(content, tmpHeight) * scale;
  }

  return NS_NewSVGRect(patternRect, x, y, width, height);
}

static float
GetLengthValue(nsSVGLength2 *aLength)
{
  return aLength->GetAnimValue(static_cast<nsSVGSVGElement*>(nsnull));
}

nsresult
nsSVGPatternFrame::ConstructCTM(nsIDOMSVGMatrix **aCTM,
                                nsIDOMSVGRect *callerBBox,
                                nsIDOMSVGMatrix *callerCTM)
{
  nsCOMPtr<nsIDOMSVGMatrix> tCTM, tempTM;

  // Begin by handling the objectBoundingBox conversion since
  // this must be handled in the CTM
  PRUint16 type = GetPatternContentUnits();

  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    // Use the bounding box
    float width, height;
    callerBBox->GetWidth(&width);
    callerBBox->GetHeight(&height);
    NS_NewSVGMatrix(getter_AddRefs(tCTM), width, 0.0f, 0.0f,
                    height, 0.0f, 0.0f);
  } else {
    float scale = nsSVGUtils::MaxExpansion(callerCTM);
    NS_NewSVGMatrix(getter_AddRefs(tCTM), scale, 0, 0, scale, 0, 0);
  }

  // Do we have a viewbox?
  nsCOMPtr<nsIDOMSVGRect> viewRect;
  GetViewBox(getter_AddRefs(viewRect));

  // See if we really have something
  float viewBoxX, viewBoxY, viewBoxHeight, viewBoxWidth;
  viewRect->GetX(&viewBoxX);
  viewRect->GetY(&viewBoxY);
  viewRect->GetHeight(&viewBoxHeight);
  viewRect->GetWidth(&viewBoxWidth);
  if (viewBoxHeight > 0.0f && viewBoxWidth > 0.0f) {

    float viewportWidth = GetLengthValue(GetWidth());
    float viewportHeight = GetLengthValue(GetHeight());
    float refX = GetLengthValue(GetX());
    float refY = GetLengthValue(GetY());

    tempTM = nsSVGUtils::GetViewBoxTransform(viewportWidth, viewportHeight,
                                             viewBoxX + refX, viewBoxY + refY,
                                             viewBoxWidth, viewBoxHeight,
                                             GetPreserveAspectRatio(),
                                             PR_TRUE);

  } else {
    // No viewBox, construct from the (modified) parent matrix
    NS_NewSVGMatrix(getter_AddRefs(tempTM));
  }
  tCTM->Multiply(tempTM, aCTM);
  return NS_OK;
}

gfxMatrix
nsSVGPatternFrame::GetPatternMatrix(nsIDOMSVGRect *bbox,
                                    nsIDOMSVGRect *callerBBox,
                                    nsIDOMSVGMatrix *callerCTM)
{
  // Get the pattern transform
  gfxMatrix patternTransform = GetPatternTransform();

  // We really want the pattern matrix to handle translations
  float minx, miny;
  bbox->GetX(&minx);
  bbox->GetY(&miny);

  PRUint16 type = GetPatternContentUnits();
  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    float x, y;
    callerBBox->GetX(&x);
    callerBBox->GetY(&y);
    minx += x;
    miny += y;
  }

  float scale = 1.0f / nsSVGUtils::MaxExpansion(callerCTM);
  patternTransform.Scale(scale, scale);
  patternTransform.Translate(gfxPoint(minx, miny));

  return patternTransform;
}

nsresult
nsSVGPatternFrame::GetCallerGeometry(nsIDOMSVGMatrix **aCTM,
                                     nsIDOMSVGRect **aBBox,
                                     nsSVGElement **aContent,
                                     nsSVGGeometryFrame *aSource)
{
  *aCTM = nsnull;
  *aBBox = nsnull;
  *aContent = nsnull;

  // Make sure the callerContent is an SVG element.  If we are attempting
  // to paint a pattern for text, then the content will be the #text, so we
  // actually want the parent, which should be the <svg:text> or <svg:tspan>
  // element.
  nsIAtom *callerType = aSource->GetType();
  if (callerType ==  nsGkAtoms::svgGlyphFrame) {
    *aContent = static_cast<nsSVGElement*>
                           (aSource->GetContent()->GetParent());
  } else {
    *aContent = static_cast<nsSVGElement*>(aSource->GetContent());
  }
  NS_ASSERTION(aContent,"Caller does not have any content!");
  if (!aContent)
    return NS_ERROR_FAILURE;

  // Get the calling geometry's bounding box.  This
  // will be in *device coordinates*
  nsISVGChildFrame *callerSVGFrame;
  if (callerType == nsGkAtoms::svgGlyphFrame)
    CallQueryInterface(aSource->GetParent(), &callerSVGFrame);
  else
    CallQueryInterface(aSource, &callerSVGFrame);

  callerSVGFrame->SetMatrixPropagation(PR_FALSE);
  callerSVGFrame->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                                   nsISVGChildFrame::TRANSFORM_CHANGED );
  callerSVGFrame->GetBBox(aBBox);
  callerSVGFrame->SetMatrixPropagation(PR_TRUE);
  callerSVGFrame->NotifySVGChanged(nsISVGChildFrame::SUPPRESS_INVALIDATION |
                                   nsISVGChildFrame::TRANSFORM_CHANGED);

  // Sanity check
  PRUint16 type = GetPatternUnits();
  if (type == nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
    float width, height;
    (*aBBox)->GetWidth(&width);
    (*aBBox)->GetHeight(&height);
    if (width <= 0 || height <= 0) {
      return NS_ERROR_FAILURE;
    }
  }

  // Get the transformation matrix from our calling geometry
  aSource->GetCanvasTM(aCTM);

  // OK, now fix up the bounding box to reflect user coordinates
  // We handle device unit scaling in pattern matrix
  {
    float x, y, width, height;
    (*aBBox)->GetX(&x);
    (*aBBox)->GetY(&y);
    (*aBBox)->GetWidth(&width);
    (*aBBox)->GetHeight(&height);
    float scale = nsSVGUtils::MaxExpansion(*aCTM);
#ifdef DEBUG_scooter
    fprintf(stderr, "pattern scale %f\n", scale);
    fprintf(stderr, "x,y,width,height: %f %f %f %f\n", x, y, width, height);
#endif
    x *= scale;
    y *= scale;
    width *= scale;
    height *= scale;
    (*aBBox)->SetX(x);
    (*aBBox)->SetY(y);
    (*aBBox)->SetWidth(width);
    (*aBBox)->SetHeight(height);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGPaintServerFrame methods:

PRBool
nsSVGPatternFrame::SetupPaintServer(gfxContext *aContext,
                                    nsSVGGeometryFrame *aSource,
                                    float aGraphicOpacity)
{
  if (aGraphicOpacity == 0.0f) {
    aContext->SetColor(gfxRGBA(0, 0, 0, 0));
    return PR_TRUE;
  }

  gfxMatrix matrix = aContext->CurrentMatrix();

  // Paint it!
  nsRefPtr<gfxASurface> surface;
  gfxMatrix pMatrix;
  aContext->IdentityMatrix();
  nsresult rv = PaintPattern(getter_AddRefs(surface), &pMatrix,
                             aSource, aGraphicOpacity);

  aContext->SetMatrix(matrix);
  if (NS_FAILED(rv)) {
    return PR_FALSE;
  }

  if (pMatrix.IsSingular()) {
    return PR_FALSE;
  }

  pMatrix.Invert();

  nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);

  if (!pattern || pattern->CairoStatus())
    return PR_FALSE;

  pattern->SetMatrix(pMatrix);
  pattern->SetExtend(gfxPattern::EXTEND_REPEAT);

  aContext->SetPattern(pattern);

  return PR_TRUE;
}

// -------------------------------------------------------------------------
// Public functions
// -------------------------------------------------------------------------

nsIFrame* NS_NewSVGPatternFrame(nsIPresShell*   aPresShell,
                                nsIContent*     aContent,
                                nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGPatternElement> patternElement = do_QueryInterface(aContent);
  if (!patternElement) {
    NS_ERROR("Can't create frame! Content is not an SVG pattern");
    return nsnull;
  }

  return new (aPresShell) nsSVGPatternFrame(aContext);
}

#ifdef DEBUG_scooter
static void printCTM(char *msg, gfxMatrix aCTM)
{
  printf("%s {%f,%f,%f,%f,%f,%f}\n", msg,
         aCTM.xx, aCTM.yx, aCTM.xy, aCTM.yy, aCTM.x0, aCTM.y0);
}

static void printCTM(char *msg, nsIDOMSVGMatrix *aCTM)
{
  float a,b,c,d,e,f;
  aCTM->GetA(&a);
  aCTM->GetB(&b);
  aCTM->GetC(&c);
  aCTM->GetD(&d);
  aCTM->GetE(&e);
  aCTM->GetF(&f);
  printf("%s {%f,%f,%f,%f,%f,%f}\n",msg,a,b,c,d,e,f);
}

static void printRect(char *msg, nsIDOMSVGRect *aRect)
{
  float x,y,width,height;
  aRect->GetX(&x);
  aRect->GetY(&y);
  aRect->GetWidth(&width);
  aRect->GetHeight(&height);
  printf("%s {%f,%f,%f,%f}\n",msg,x,y,width,height);
}
#endif
