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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSVGFilterFrame.h"
#include "nsSVGFilterInstance.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsISVGValue.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsISVGValueUtils.h"
#include "nsISVGGeometrySource.h"
#include "nsSVGMatrix.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererCanvas.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGFilter.h"
#include "nsSVGLength.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsINameSpaceManager.h"
#include "nsSVGAtoms.h"
#include "nsSVGDefsFrame.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGValue.h"
#include "nsSVGPoint.h"
#include "nsIDOMSVGAnimatedInteger.h"
#include "nsSVGUtils.h"

class nsSVGFilterFrame : public nsSVGDefsFrame,
                         public nsSVGValue,
                         public nsISVGFilterFrame
{
protected:
  friend nsresult
  NS_NewSVGFilterFrame(nsIPresShell* aPresShell,
                       nsIContent* aContent,
                       nsIFrame** aNewFrame);

  virtual ~nsSVGFilterFrame();
  NS_IMETHOD InitSVG();

public:
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGFilterFrame interface:
  NS_IMETHOD FilterPaint(nsISVGRendererCanvas *aCanvas,
                         nsISVGChildFrame *aTarget);
  NS_IMETHOD GetInvalidationRegion(nsIFrame *aTarget,
                                   nsISVGRendererRegion **aRegion);

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString &aValue) { return NS_OK; }
  NS_IMETHOD GetValueString(nsAString& aValue) { return NS_ERROR_NOT_IMPLEMENTED; }

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable, 
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable(nsISVGValue* observable, 
                                    nsISVGValue::modificationType aModType);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgFilterFrame
   */
  virtual nsIAtom* GetType() const;

private:
  // implementation helpers
  void FilterFailCleanup(nsISVGRendererCanvas *aCanvas,
                         nsISVGChildFrame *aTarget);
  
private:
  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
  nsCOMPtr<nsIDOMSVGLength> mWidth;
  nsCOMPtr<nsIDOMSVGLength> mHeight;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mFilterUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mPrimitiveUnits;
  nsCOMPtr<nsIDOMSVGAnimatedInteger> mFilterResX;
  nsCOMPtr<nsIDOMSVGAnimatedInteger> mFilterResY;
};

NS_INTERFACE_MAP_BEGIN(nsSVGFilterFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilterFrame)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END_INHERITING(nsSVGDefsFrame)

nsresult
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  nsSVGFilterFrame* it = new (aPresShell) nsSVGFilterFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsresult
NS_GetSVGFilterFrame(nsISVGFilterFrame **aResult,
                     nsIURI *aURI, nsIContent *aContent)
{
  *aResult = nsnull;

  // Get the PresShell
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("No document for this content!");
    return NS_ERROR_FAILURE;
  }
  nsIPresShell *aPresShell = myDoc->GetShellAt(0);

  // Get the URI Spec
  nsCAutoString uriSpec;
  aURI->GetSpec(uriSpec);

  // Find the referenced frame
  nsIFrame *filter;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&filter,
                                                   uriSpec, aContent, aPresShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = filter->GetType();
  if (frameType != nsLayoutAtoms::svgFilterFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGFilterFrame *)filter;
  return NS_OK;
}

nsSVGFilterFrame::~nsSVGFilterFrame()
{
  WillModify();
  // Notify the world that we're dying
  DidModify(mod_die);

  NS_REMOVE_SVGVALUE_OBSERVER(mX);
  NS_REMOVE_SVGVALUE_OBSERVER(mY);
  NS_REMOVE_SVGVALUE_OBSERVER(mWidth);
  NS_REMOVE_SVGVALUE_OBSERVER(mHeight);
  NS_REMOVE_SVGVALUE_OBSERVER(mFilterUnits);
  NS_REMOVE_SVGVALUE_OBSERVER(mPrimitiveUnits);
  NS_REMOVE_SVGVALUE_OBSERVER(mFilterResX);
  NS_REMOVE_SVGVALUE_OBSERVER(mFilterResY);
  NS_REMOVE_SVGVALUE_OBSERVER(mContent);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:
NS_IMETHODIMP
nsSVGFilterFrame::WillModifySVGObservable(nsISVGValue* observable,
                                          modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}
                                                                                
NS_IMETHODIMP
nsSVGFilterFrame::DidModifySVGObservable(nsISVGValue* observable, 
                                         nsISVGValue::modificationType aModType)
{
  // Something we depend on was modified -- pass it on!
  DidModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFilterFrame::InitSVG()
{
  nsresult rv = nsSVGDefsFrame::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(mContent);
  NS_ASSERTION(filter, "wrong content element");

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    filter->GetX(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no X");
    if (!mX) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mX);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    filter->GetY(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no Y");
    if (!mY) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mY);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    filter->GetWidth(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no Width");
    if (!mWidth) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mWidth);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    filter->GetHeight(getter_AddRefs(length));
    length->GetBaseVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no Height");
    if (!mHeight) return NS_ERROR_FAILURE;
    NS_ADD_SVGVALUE_OBSERVER(mHeight);
  }

  filter->GetFilterUnits(getter_AddRefs(mFilterUnits));
  NS_ADD_SVGVALUE_OBSERVER(mFilterUnits);

  filter->GetPrimitiveUnits(getter_AddRefs(mPrimitiveUnits));
  NS_ADD_SVGVALUE_OBSERVER(mPrimitiveUnits);

  filter->GetFilterResX(getter_AddRefs(mFilterResX));
  NS_ADD_SVGVALUE_OBSERVER(mFilterResX);

  filter->GetFilterResY(getter_AddRefs(mFilterResY));
  NS_ADD_SVGVALUE_OBSERVER(mFilterResY);

  NS_ADD_SVGVALUE_OBSERVER(mContent);

  return NS_OK;
}

void
nsSVGFilterFrame::FilterFailCleanup(nsISVGRendererCanvas *aCanvas,
                                    nsISVGChildFrame *aTarget)
{
  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);
  nsRect dummyRect;
  aTarget->PaintSVG(aCanvas, dummyRect, PR_TRUE);
}

NS_IMETHODIMP
nsSVGFilterFrame::FilterPaint(nsISVGRendererCanvas *aCanvas,
                              nsISVGChildFrame *aTarget)
{
  nsCOMPtr<nsIDOMSVGFilterElement> aFilter = do_QueryInterface(mContent);
  NS_ASSERTION(aFilter, "Wrong content element (not filter)");

  PRBool unimplementedFilter = PR_FALSE;
  PRUint32 requirements = 0;
  PRUint32 count = mContent->GetChildCount();
  for (PRUint32 i=0; i<count; ++i) {
    nsIContent* child = mContent->GetChildAt(i);

    nsCOMPtr<nsISVGFilter> filter = do_QueryInterface(child);
    if (filter) {
      PRUint32 tmp;
      filter->GetRequirements(&tmp);
      requirements |= tmp;
    }

    nsCOMPtr<nsIDOMSVGFEUnimplementedMOZElement> unimplemented;
    unimplemented = do_QueryInterface(child);
    if (unimplemented)
      unimplementedFilter = PR_TRUE;
  }

  // check for source requirements or filter elements that we don't support yet
  if (requirements & ~(NS_FE_SOURCEGRAPHIC | NS_FE_SOURCEALPHA) ||
      unimplementedFilter) {
#ifdef DEBUG_tor
    if (requirements & ~(NS_FE_SOURCEGRAPHIC | NS_FE_SOURCEALPHA))
      fprintf(stderr, "FilterFrame: unimplemented source requirement\n");
    if (unimplementedFilter)
      fprintf(stderr, "FilterFrame: unimplemented filter element\n");
#endif
    nsRect dummyRect;
    aTarget->PaintSVG(aCanvas, dummyRect, PR_TRUE);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGMatrix> ctm;

  nsISVGContainerFrame *aContainer;
  CallQueryInterface(aTarget, &aContainer);
  if (aContainer)
    ctm = aContainer->GetCanvasTM();
  else {
    nsISVGGeometrySource *aSource;
    CallQueryInterface(aTarget, &aSource);
    if (aSource)
      aSource->GetCanvasTM(getter_AddRefs(ctm));
  }

  float s1, s2;
  ctm->GetA(&s1);
  ctm->GetD(&s2);
#ifdef DEBUG_tor
  fprintf(stderr, "scales: %f %f\n", s1, s2);
#endif

  nsIFrame *frame;
  CallQueryInterface(aTarget, &frame);
  nsIContent *target = frame->GetContent();

  aTarget->SetMatrixPropagation(PR_FALSE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);

  PRUint16 type;
  mFilterUnits->GetAnimVal(&type);

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;
  aTarget->GetBBox(getter_AddRefs(bbox));

  if (type == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, mX, nsSVGUtils::X);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, mY, nsSVGUtils::Y);
    width = nsSVGUtils::ObjectSpace(bbox, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::ObjectSpace(bbox, mHeight, nsSVGUtils::Y);
  } else {
    x = nsSVGUtils::UserSpace(target, mX, nsSVGUtils::X);
    y = nsSVGUtils::UserSpace(target, mY, nsSVGUtils::Y);
    width = nsSVGUtils::UserSpace(target, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::UserSpace(target, mHeight, nsSVGUtils::Y);
  }
  
  PRInt32 filterResX = PRInt32(s1 * width + 0.5);
  PRInt32 filterResY = PRInt32(s2 * height + 0.5);

  if (mContent->HasAttr(kNameSpaceID_None, nsSVGAtoms::filterRes)) {
    mFilterResX->GetAnimVal(&filterResX);
    mFilterResY->GetAnimVal(&filterResY);
  }

  // filterRes = 0 disables rendering, < 0 is error
  if (filterResX <= 0.0f || filterResY <= 0.0f)
    return NS_OK;

#ifdef DEBUG_tor
  fprintf(stderr, "filter bbox: %f,%f  %fx%f\n", x, y, width, height);
  fprintf(stderr, "filterRes: %u %u\n", filterResX, filterResY);
#endif

  nsCOMPtr<nsIDOMSVGMatrix> filterTransform;
  NS_NewSVGMatrix(getter_AddRefs(filterTransform),
                  filterResX/width,  0.0f,
                  0.0f,              filterResY/height,
                  -x*filterResX/width,                 -y*filterResY/height);
  aTarget->SetOverrideCTM(filterTransform);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);

  // paint the target geometry
  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  nsCOMPtr<nsISVGRenderer> renderer;
  nsCOMPtr<nsISVGRendererSurface> surface;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  renderer->CreateSurface(filterResX, filterResY, getter_AddRefs(surface));

  if (!surface) {
    FilterFailCleanup(aCanvas, aTarget);
    return NS_OK;
  }

  aCanvas->PushSurface(surface);
  nsRect dummyRect;
  aTarget->PaintSVG(aCanvas, dummyRect, PR_TRUE);
  aCanvas->PopSurface();

  mPrimitiveUnits->GetAnimVal(&type);
  nsSVGFilterInstance instance(renderer, target, bbox,
                               x, y, width, height,
                               filterResX, filterResY,
                               type);

  if (requirements & NS_FE_SOURCEALPHA) {
    nsCOMPtr<nsISVGRendererSurface> alpha;
    renderer->CreateSurface(filterResX, filterResY, getter_AddRefs(alpha));

    if (!alpha) {
      FilterFailCleanup(aCanvas, aTarget);
      return NS_OK;
    }

    PRUint8 *data, *alphaData;
    PRUint32 length;
    PRInt32 stride;
    surface->Lock();
    alpha->Lock();
    surface->GetData(&data, &length, &stride);
    alpha->GetData(&alphaData, &length, &stride);

    for (PRUint32 yy=0; yy<filterResY; yy++)
      for (PRUint32 xx=0; xx<filterResX; xx++) {
        alphaData[stride*yy + 4*xx]     = 0;
        alphaData[stride*yy + 4*xx + 1] = 0;
        alphaData[stride*yy + 4*xx + 2] = 0;
        alphaData[stride*yy + 4*xx + 3] = data[stride*yy + 4*xx + 3];
    }

    surface->Unlock();
    alpha->Unlock();

    instance.DefineImage(NS_LITERAL_STRING("SourceAlpha"), alpha);
    instance.DefineRegion(NS_LITERAL_STRING("SourceAlpha"), 
                          nsRect(0, 0, filterResX, filterResY));
  }

  // this always needs to be defined last because the default image
  // for the first filter element is supposed to be SourceGraphic
  instance.DefineImage(NS_LITERAL_STRING("SourceGraphic"), surface);
  instance.DefineRegion(NS_LITERAL_STRING("SourceGraphic"), 
                        nsRect(0, 0, filterResX, filterResY));

  for (PRUint32 k=0; k<count; ++k) {
    nsresult rv;
    nsIContent* child = mContent->GetChildAt(k);

    nsCOMPtr<nsISVGFilter> filter = do_QueryInterface(child);
    if (filter)
      rv = filter->Filter(&instance);
    if (NS_FAILED(rv)) {
      FilterFailCleanup(aCanvas, aTarget);
      return NS_OK;
    }
  }

  nsCOMPtr<nsISVGRendererSurface> filterResult;
  instance.LookupImage(NS_LITERAL_STRING(""), getter_AddRefs(filterResult));

  nsCOMPtr<nsIDOMSVGMatrix> scale, fini;
  NS_NewSVGMatrix(getter_AddRefs(scale),
                  width/filterResX, 0.0f,
                  0.0f, height/filterResY,
                  x, y);

  ctm->Multiply(scale, getter_AddRefs(fini));

  aCanvas->CompositeSurfaceMatrix(filterResult, fini, 1.0);

  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGFilterFrame::GetInvalidationRegion(nsIFrame *aTarget,
                                        nsISVGRendererRegion **aRegion)
{
  nsIContent *targetContent = aTarget->GetContent();
  nsISVGChildFrame *svg;

  nsCOMPtr<nsIDOMSVGMatrix> ctm;

  nsISVGContainerFrame *aContainer;
  CallQueryInterface(aTarget, &aContainer);
  if (aContainer)
    ctm = aContainer->GetCanvasTM();
  else {
    nsISVGGeometrySource *source;
    CallQueryInterface(aTarget, &source);
    if (source)
      source->GetCanvasTM(getter_AddRefs(ctm));
  }

  CallQueryInterface(aTarget, &svg);

  svg->SetMatrixPropagation(PR_FALSE);
  svg->NotifyCanvasTMChanged(PR_TRUE);

  PRUint16 type;
  mFilterUnits->GetAnimVal(&type);

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;
  svg->GetBBox(getter_AddRefs(bbox));

  if (type == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, mX, nsSVGUtils::X);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, mY, nsSVGUtils::Y);
    width = nsSVGUtils::ObjectSpace(bbox, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::ObjectSpace(bbox, mHeight, nsSVGUtils::Y);
  } else {
    x = nsSVGUtils::UserSpace(targetContent, mX, nsSVGUtils::X);
    y = nsSVGUtils::UserSpace(targetContent, mY, nsSVGUtils::Y);
    width = nsSVGUtils::UserSpace(targetContent, mWidth, nsSVGUtils::X);
    height = nsSVGUtils::UserSpace(targetContent, mHeight, nsSVGUtils::Y);
  }

  svg->SetMatrixPropagation(PR_TRUE);
  svg->NotifyCanvasTMChanged(PR_TRUE);

#ifdef DEBUG_tor
  fprintf(stderr, "invalidate box: %f,%f %fx%f\n", x, y, width, height);
#endif

  // transform back
  float xx[4], yy[4];
  xx[0] = x;          yy[0] = y;
  xx[1] = x + width;  yy[1] = y;
  xx[2] = x + width;  yy[2] = y + height;
  xx[3] = x;          yy[3] = y + height;

  nsSVGUtils::TransformPoint(ctm, &xx[0], &yy[0]);
  nsSVGUtils::TransformPoint(ctm, &xx[1], &yy[1]);
  nsSVGUtils::TransformPoint(ctm, &xx[2], &yy[2]);
  nsSVGUtils::TransformPoint(ctm, &xx[3], &yy[3]);

  float xmin, xmax, ymin, ymax;
  xmin = xmax = xx[0];
  ymin = ymax = yy[0];
  for (int i=1; i<4; i++) {
    if (xx[i] < xmin)
      xmin = xx[i];
    if (yy[i] < ymin)
      ymin = yy[i];
    if (xx[i] > xmax)
      xmax = xx[i];
    if (yy[i] > ymax)
      ymax = yy[i];
  }

#ifdef DEBUG_tor
  fprintf(stderr, "xform bound: %f %f  %f %f\n", xmin, ymin, xmax, ymax);
#endif

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  renderer->CreateRectRegion(xmin, ymin, xmax - xmin, ymax - ymin, aRegion);

  return NS_OK;
}

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsLayoutAtoms::svgFilterFrame;
}

// ----------------------------------------------------------------
// nsSVGFilterInstance

float
nsSVGFilterInstance::GetPrimitiveX(nsIDOMSVGLength *aLength)
{
  float value;
  if (mPrimitiveUnits == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX)
    value = nsSVGUtils::ObjectSpace(mTargetBBox, aLength, nsSVGUtils::X);
  else
    value = nsSVGUtils::UserSpace(mTarget, aLength, nsSVGUtils::X);

  return value * mFilterResX / mFilterWidth;
}

float
nsSVGFilterInstance::GetPrimitiveY(nsIDOMSVGLength *aLength)
{
  float value;
  if (mPrimitiveUnits == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX)
    value = nsSVGUtils::ObjectSpace(mTargetBBox, aLength, nsSVGUtils::Y);
  else
    value = nsSVGUtils::UserSpace(mTarget, aLength, nsSVGUtils::Y);

  return value * mFilterResY / mFilterHeight;
}

float
nsSVGFilterInstance::GetPrimitiveXY(nsIDOMSVGLength *aLength)
{
  float value;
  if (mPrimitiveUnits == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX)
    value = nsSVGUtils::ObjectSpace(mTargetBBox, aLength, nsSVGUtils::XY);
  else
    value = nsSVGUtils::UserSpace(mTarget, aLength, nsSVGUtils::XY);

  return value *
    sqrt(float(mFilterResX * mFilterResX + mFilterResY * mFilterResY)) /
    sqrt(mFilterWidth * mFilterWidth + mFilterHeight * mFilterHeight);
}

void
nsSVGFilterInstance::GetFilterSubregion(
  nsIDOMSVGFilterPrimitiveStandardAttributes *aFilter,
  nsRect defaultRegion,
  nsRect *result)
{
  nsCOMPtr<nsIDOMSVGLength> svgX, svgY, svgWidth, svgHeight;
  nsCOMPtr<nsIDOMSVGAnimatedLength> val;
  aFilter->GetX(getter_AddRefs(val));
  val->GetBaseVal(getter_AddRefs(svgX));
  aFilter->GetY(getter_AddRefs(val));
  val->GetBaseVal(getter_AddRefs(svgY));
  aFilter->GetWidth(getter_AddRefs(val));
  val->GetBaseVal(getter_AddRefs(svgWidth));
  aFilter->GetHeight(getter_AddRefs(val));
  val->GetBaseVal(getter_AddRefs(svgHeight));

  float x, y, width, height;

  if (mPrimitiveUnits == 
      nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    x      = nsSVGUtils::ObjectSpace(mTargetBBox, svgX, nsSVGUtils::X);
    y      = nsSVGUtils::ObjectSpace(mTargetBBox, svgY, nsSVGUtils::Y);
    width  = nsSVGUtils::ObjectSpace(mTargetBBox, svgWidth, nsSVGUtils::X);
    height = nsSVGUtils::ObjectSpace(mTargetBBox, svgHeight, nsSVGUtils::Y);
  } else {
    x      = nsSVGUtils::UserSpace(mTarget, svgX, nsSVGUtils::X);
    y      = nsSVGUtils::UserSpace(mTarget, svgY, nsSVGUtils::Y);
    width  = nsSVGUtils::UserSpace(mTarget, svgWidth, nsSVGUtils::X);
    height = nsSVGUtils::UserSpace(mTarget, svgHeight, nsSVGUtils::Y);
  }

#ifdef DEBUG_tor
  fprintf(stderr, "GFS[1]: %f %f %f %f\n", x, y, width, height);
#endif

  nsRect filter, region;

  filter.x = 0;
  filter.y = 0;
  filter.width = mFilterResX;
  filter.height = mFilterResY;

  region.x      = (x - mFilterX) * mFilterResX / mFilterWidth;
  region.y      = (y - mFilterY) * mFilterResY / mFilterHeight;
  region.width  =          width * mFilterResX / mFilterWidth;
  region.height =         height * mFilterResY / mFilterHeight;

#ifdef DEBUG_tor
  fprintf(stderr, "GFS[2]: %d %d %d %d\n",
          region.x, region.y, region.width, region.height);
#endif

  nsCOMPtr<nsIContent> content = do_QueryInterface(aFilter);
  if (!content->HasAttr(kNameSpaceID_None, nsSVGAtoms::x))
    region.x = defaultRegion.x;
  if (!content->HasAttr(kNameSpaceID_None, nsSVGAtoms::y))
    region.y = defaultRegion.y;
  if (!content->HasAttr(kNameSpaceID_None, nsSVGAtoms::width))
    region.width = defaultRegion.width;
  if (!content->HasAttr(kNameSpaceID_None, nsSVGAtoms::height))
    region.height = defaultRegion.height;

  result->IntersectRect(filter, region);

#ifdef DEBUG_tor
  fprintf(stderr, "GFS[3]: %d %d %d %d\n",
          result->x, result->y, result->width, result->height);
#endif
}

void
nsSVGFilterInstance::GetImage(nsISVGRendererSurface **result)
{
  mRenderer->CreateSurface(mFilterResX, mFilterResY, result);
}

void
nsSVGFilterInstance::LookupImage(const nsAString &aName, 
                                 nsISVGRendererSurface **aImage)
{
  if (aName.IsEmpty()) {
    *aImage = mLastImage;
    NS_IF_ADDREF(*aImage);
  } else
    mImageDictionary.Get(aName, aImage);
}

void
nsSVGFilterInstance::DefineImage(const nsAString &aName, 
                                 nsISVGRendererSurface *aImage)
{
  mImageDictionary.Put(aName, aImage);
  mLastImage = aImage;
}

void
nsSVGFilterInstance::LookupRegion(const nsAString &aName, 
                                  nsRect *aRect)
{
  if (aName.IsEmpty())
    *aRect = mLastRegion;
  else
    mRegionDictionary.Get(aName, aRect);
}

void
nsSVGFilterInstance::DefineRegion(const nsAString &aName, 
                                  nsRect aRect)
{
  mRegionDictionary.Put(aName, aRect);
  mLastRegion = aRect;
}
