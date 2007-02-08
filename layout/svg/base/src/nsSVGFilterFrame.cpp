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
#include "nsIDocument.h"
#include "nsISVGValueUtils.h"
#include "nsSVGMatrix.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsISVGFilter.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGAnimatedInteger.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterElement.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGFilters.h"
#include "nsSVGContainerFrame.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

typedef nsSVGContainerFrame nsSVGFilterFrameBase;

class nsSVGFilterFrame : public nsSVGFilterFrameBase,
                         public nsISVGFilterFrame
{
protected:
  friend nsIFrame*
  NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext);

  NS_IMETHOD InitSVG();

public:
  nsSVGFilterFrame(nsStyleContext* aContext) : nsSVGFilterFrameBase(aContext) {}

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }

  // nsISVGFilterFrame interface:
  NS_IMETHOD FilterPaint(nsSVGRenderState *aContext,
                         nsISVGChildFrame *aTarget);
  NS_IMETHOD_(nsRect) GetInvalidationRegion(nsIFrame *aTarget);

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString &aValue) { return NS_OK; }
  NS_IMETHOD GetValueString(nsAString& aValue) { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::svgFilterFrame
   */
  virtual nsIAtom* GetType() const;

private:
  // implementation helpers
  void FilterFailCleanup(nsSVGRenderState *aContext,
                         nsISVGChildFrame *aTarget);
  
private:
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mFilterUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mPrimitiveUnits;
  nsCOMPtr<nsIDOMSVGAnimatedInteger> mFilterResX;
  nsCOMPtr<nsIDOMSVGAnimatedInteger> mFilterResY;
};

NS_INTERFACE_MAP_BEGIN(nsSVGFilterFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilterFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFilterFrameBase)

nsIFrame*
NS_NewSVGFilterFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGFilterFrame(aContext);
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
  nsIPresShell *presShell = myDoc->GetShellAt(0);
  if (!presShell) {
    NS_WARNING("no presshell");
    return NS_ERROR_FAILURE;
  }

  // Find the referenced frame
  nsIFrame *filter;
  if (!NS_SUCCEEDED(nsSVGUtils::GetReferencedFrame(&filter, aURI, aContent, presShell)))
    return NS_ERROR_FAILURE;

  nsIAtom* frameType = filter->GetType();
  if (frameType != nsGkAtoms::svgFilterFrame)
    return NS_ERROR_FAILURE;

  *aResult = (nsSVGFilterFrame *)filter;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFilterFrame::InitSVG()
{
  nsresult rv = nsSVGFilterFrameBase::InitSVG();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(mContent);
  NS_ASSERTION(filter, "wrong content element");

  filter->GetFilterUnits(getter_AddRefs(mFilterUnits));
  filter->GetPrimitiveUnits(getter_AddRefs(mPrimitiveUnits));
  filter->GetFilterResX(getter_AddRefs(mFilterResX));
  filter->GetFilterResY(getter_AddRefs(mFilterResY));

  return NS_OK;
}

void
nsSVGFilterFrame::FilterFailCleanup(nsSVGRenderState *aContext,
                                    nsISVGChildFrame *aTarget)
{
  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);
  aTarget->PaintSVG(aContext, nsnull);
}

NS_IMETHODIMP
nsSVGFilterFrame::FilterPaint(nsSVGRenderState *aContext,
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
    aTarget->PaintSVG(aContext, nsnull);
    return NS_OK;
  }

  nsIFrame *frame;
  CallQueryInterface(aTarget, &frame);

  nsCOMPtr<nsIDOMSVGMatrix> ctm = nsSVGUtils::GetCanvasTM(frame);

  float s1, s2;
  ctm->GetA(&s1);
  ctm->GetD(&s2);
#ifdef DEBUG_tor
  fprintf(stderr, "scales: %f %f\n", s1, s2);
#endif

  nsSVGElement *target = NS_STATIC_CAST(nsSVGElement*, frame->GetContent());

  aTarget->SetMatrixPropagation(PR_FALSE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);

  PRUint16 type;
  mFilterUnits->GetAnimVal(&type);

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;
  aTarget->GetBBox(getter_AddRefs(bbox));

  nsSVGFilterElement *filter = NS_STATIC_CAST(nsSVGFilterElement*, mContent);
  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
  tmpX = &filter->mLengthAttributes[nsSVGFilterElement::X];
  tmpY = &filter->mLengthAttributes[nsSVGFilterElement::Y];
  tmpWidth = &filter->mLengthAttributes[nsSVGFilterElement::WIDTH];
  tmpHeight = &filter->mLengthAttributes[nsSVGFilterElement::HEIGHT];

  if (type == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    if (!bbox)
      return NS_OK;

    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, tmpX);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    x = nsSVGUtils::UserSpace(target, tmpX);
    y = nsSVGUtils::UserSpace(target, tmpY);
    width = nsSVGUtils::UserSpace(target, tmpWidth);
    height = nsSVGUtils::UserSpace(target, tmpHeight);
  }
  
  PRInt32 filterResX = PRInt32(s1 * width + 0.5);
  PRInt32 filterResY = PRInt32(s2 * height + 0.5);

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::filterRes)) {
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
  nsRefPtr<gfxImageSurface> tmpSurface =
    new gfxImageSurface(gfxIntSize(filterResX, filterResY), gfxASurface::ImageFormatARGB32);
  if (!tmpSurface) {
    FilterFailCleanup(aContext, aTarget);
    return NS_OK;
  }

  gfxContext tmpContext(tmpSurface);
  nsSVGRenderState tmpState(&tmpContext);

  memset(tmpSurface->Data(), 0, tmpSurface->GetSize().height * tmpSurface->Stride());
  aTarget->PaintSVG(&tmpState, nsnull);

  mPrimitiveUnits->GetAnimVal(&type);
  nsSVGFilterInstance instance(target, bbox,
                               x, y, width, height,
                               filterResX, filterResY,
                               type);
  nsSVGFilterInstance::ColorModel 
    colorModel(nsSVGFilterInstance::ColorModel::SRGB,
               nsSVGFilterInstance::ColorModel::PREMULTIPLIED);

  if (requirements & NS_FE_SOURCEALPHA) {
    cairo_surface_t *alpha =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 filterResX, filterResY);

    if (!alpha) {
      FilterFailCleanup(aContext, aTarget);
      return NS_OK;
    }

    PRUint8 *data = tmpSurface->Data();
    PRUint8 *alphaData = cairo_image_surface_get_data(alpha);
    PRUint32 stride = tmpSurface->Stride();

    for (PRUint32 yy=0; yy<filterResY; yy++)
      for (PRUint32 xx=0; xx<filterResX; xx++) {
        alphaData[stride*yy + 4*xx]     = 0;
        alphaData[stride*yy + 4*xx + 1] = 0;
        alphaData[stride*yy + 4*xx + 2] = 0;
        alphaData[stride*yy + 4*xx + 3] = data[stride*yy + 4*xx + 3];
    }

    instance.DefineImage(NS_LITERAL_STRING("SourceAlpha"), alpha,
                         nsRect(0, 0, filterResX, filterResY), colorModel);
  }

  // this always needs to be defined last because the default image
  // for the first filter element is supposed to be SourceGraphic
  instance.DefineImage(NS_LITERAL_STRING("SourceGraphic"),
                       tmpSurface->CairoSurface(),
                       nsRect(0, 0, filterResX, filterResY), colorModel);

  for (PRUint32 k=0; k<count; ++k) {
    nsIContent* child = mContent->GetChildAt(k);

    nsCOMPtr<nsISVGFilter> filter = do_QueryInterface(child);
    if (filter && NS_FAILED(filter->Filter(&instance))) {
      FilterFailCleanup(aContext, aTarget);
      return NS_OK;
    }
  }

  cairo_surface_t *filterResult = nsnull;
  nsRect filterRect;
  nsRefPtr<gfxASurface> resultSurface;

  instance.LookupImage(NS_LITERAL_STRING(""),
                       &filterResult, &filterRect, colorModel);

  if (filterResult)
    resultSurface = gfxASurface::Wrap(filterResult);
  if (!resultSurface) {
    FilterFailCleanup(aContext, aTarget);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGMatrix> scale, fini;
  NS_NewSVGMatrix(getter_AddRefs(scale),
                  width/filterResX, 0.0f,
                  0.0f, height/filterResY,
                  x, y);

  ctm->Multiply(scale, getter_AddRefs(fini));

  nsSVGUtils::CompositeSurfaceMatrix(aContext->GetGfxContext(),
                                     resultSurface, fini, 1.0);

  aTarget->SetOverrideCTM(nsnull);
  aTarget->SetMatrixPropagation(PR_TRUE);
  aTarget->NotifyCanvasTMChanged(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP_(nsRect)
nsSVGFilterFrame::GetInvalidationRegion(nsIFrame *aTarget)
{
  nsSVGElement *targetContent =
    NS_STATIC_CAST(nsSVGElement*, aTarget->GetContent());
  nsISVGChildFrame *svg;

  nsCOMPtr<nsIDOMSVGMatrix> ctm = nsSVGUtils::GetCanvasTM(aTarget);

  CallQueryInterface(aTarget, &svg);

  PRUint16 type;
  mFilterUnits->GetAnimVal(&type);

  float x, y, width, height;
  nsCOMPtr<nsIDOMSVGRect> bbox;

  svg->SetMatrixPropagation(PR_FALSE);
  svg->NotifyCanvasTMChanged(PR_TRUE);

  svg->GetBBox(getter_AddRefs(bbox));

  svg->SetMatrixPropagation(PR_TRUE);
  svg->NotifyCanvasTMChanged(PR_TRUE);

  nsSVGFilterElement *filter = NS_STATIC_CAST(nsSVGFilterElement*, mContent);
  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;
  tmpX = &filter->mLengthAttributes[nsSVGFilterElement::X];
  tmpY = &filter->mLengthAttributes[nsSVGFilterElement::Y];
  tmpWidth = &filter->mLengthAttributes[nsSVGFilterElement::WIDTH];
  tmpHeight = &filter->mLengthAttributes[nsSVGFilterElement::HEIGHT];

  if (type == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    if (!bbox)
      return nsRect();

    bbox->GetX(&x);
    x += nsSVGUtils::ObjectSpace(bbox, tmpX);
    bbox->GetY(&y);
    y += nsSVGUtils::ObjectSpace(bbox, tmpY);
    width = nsSVGUtils::ObjectSpace(bbox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(bbox, tmpHeight);
  } else {
    x = nsSVGUtils::UserSpace(targetContent, tmpX);
    y = nsSVGUtils::UserSpace(targetContent, tmpY);
    width = nsSVGUtils::UserSpace(targetContent, tmpWidth);
    height = nsSVGUtils::UserSpace(targetContent, tmpHeight);
  }

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

  return nsSVGUtils::ToBoundingPixelRect(xmin, ymin, xmax, ymax);
}

nsIAtom *
nsSVGFilterFrame::GetType() const
{
  return nsGkAtoms::svgFilterFrame;
}

// ----------------------------------------------------------------
// nsSVGFilterInstance

float
nsSVGFilterInstance::GetPrimitiveLength(nsSVGLength2 *aLength)
{
  float value;
  if (mPrimitiveUnits == nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX)
    value = nsSVGUtils::ObjectSpace(mTargetBBox, aLength);
  else
    value = nsSVGUtils::UserSpace(mTarget, aLength);

  switch (aLength->GetCtxType()) {
  case nsSVGUtils::X:
    return value * mFilterResX / mFilterWidth;
  case nsSVGUtils::Y:
    return value * mFilterResY / mFilterHeight;
  case nsSVGUtils::XY:
  default:
    return value *
      sqrt(float(mFilterResX * mFilterResX + mFilterResY * mFilterResY)) /
      sqrt(mFilterWidth * mFilterWidth + mFilterHeight * mFilterHeight);
  }
}

void
nsSVGFilterInstance::GetFilterSubregion(
  nsIContent *aFilter,
  nsRect defaultRegion,
  nsRect *result)
{
  nsSVGFE *fE = NS_STATIC_CAST(nsSVGFE*, aFilter);
  nsSVGLength2 *tmpX, *tmpY, *tmpWidth, *tmpHeight;

  tmpX = &fE->mLengthAttributes[nsSVGFE::X];
  tmpY = &fE->mLengthAttributes[nsSVGFE::Y];
  tmpWidth = &fE->mLengthAttributes[nsSVGFE::WIDTH];
  tmpHeight = &fE->mLengthAttributes[nsSVGFE::HEIGHT];

  float x, y, width, height;

  if (mPrimitiveUnits == 
      nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX) {
    x      = nsSVGUtils::ObjectSpace(mTargetBBox, tmpX);
    y      = nsSVGUtils::ObjectSpace(mTargetBBox, tmpY);
    width  = nsSVGUtils::ObjectSpace(mTargetBBox, tmpWidth);
    height = nsSVGUtils::ObjectSpace(mTargetBBox, tmpHeight);
  } else {
    x      = nsSVGUtils::UserSpace(mTarget, tmpX);
    y      = nsSVGUtils::UserSpace(mTarget, tmpY);
    width  = nsSVGUtils::UserSpace(mTarget, tmpWidth);
    height = nsSVGUtils::UserSpace(mTarget, tmpHeight);
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
  if (!content->HasAttr(kNameSpaceID_None, nsGkAtoms::x))
    region.x = defaultRegion.x;
  if (!content->HasAttr(kNameSpaceID_None, nsGkAtoms::y))
    region.y = defaultRegion.y;
  if (!content->HasAttr(kNameSpaceID_None, nsGkAtoms::width))
    region.width = defaultRegion.width;
  if (!content->HasAttr(kNameSpaceID_None, nsGkAtoms::height))
    region.height = defaultRegion.height;

  result->IntersectRect(filter, region);

#ifdef DEBUG_tor
  fprintf(stderr, "GFS[3]: %d %d %d %d\n",
          result->x, result->y, result->width, result->height);
#endif
}

cairo_surface_t *
nsSVGFilterInstance::GetImage()
{
  return cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                    mFilterResX, mFilterResY);
}

void
nsSVGFilterInstance::LookupImage(const nsAString &aName,
                                 cairo_surface_t **aImage,
                                 nsRect *aRegion,
                                 const ColorModel &aRequiredColorModel)
{
  ImageEntry *entry;

  if (aName.IsEmpty())
    entry = mLastImage;
  else
    mImageDictionary.Get(aName, &entry);

  if (entry) {
    *aImage = entry->mImage;
    *aRegion = entry->mRegion;

    if (aRequiredColorModel == entry->mColorModel)
      return;

    // convert image to desired format
    PRUint8 *data = cairo_image_surface_get_data(entry->mImage);
    PRInt32 stride = cairo_image_surface_get_stride(entry->mImage);

    if (entry->mColorModel.mAlphaChannel == ColorModel::PREMULTIPLIED)
      nsSVGUtils::UnPremultiplyImageDataAlpha(data, stride, entry->mRegion);

    if (aRequiredColorModel.mColorSpace != entry->mColorModel.mColorSpace) {

      if (aRequiredColorModel.mColorSpace == ColorModel::LINEAR_RGB)
        nsSVGUtils::ConvertImageDataToLinearRGB(data, stride, entry->mRegion);
      else
        nsSVGUtils::ConvertImageDataFromLinearRGB(data, stride, entry->mRegion);
    }
    if (aRequiredColorModel.mAlphaChannel == ColorModel::PREMULTIPLIED)
      nsSVGUtils::PremultiplyImageDataAlpha(data, stride, entry->mRegion);

    entry->mColorModel = aRequiredColorModel;

  } else {
    *aImage = nsnull;
    aRegion->Empty();
  }
}

void
nsSVGFilterInstance::DefineImage(const nsAString &aName,
                                 cairo_surface_t *aImage,
                                 const nsRect &aRegion,
                                 const ColorModel &aColorModel)
{
  ImageEntry *entry = new ImageEntry(aImage, aRegion, aColorModel);

  if (entry)
    mImageDictionary.Put(aName, entry);

  mLastImage = entry;
}
