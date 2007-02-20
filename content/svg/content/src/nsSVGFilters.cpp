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

#include "nsSVGElement.h"
#include "nsSVGAnimatedString.h"
#include "nsSVGLength.h"
#include "nsGkAtoms.h"
#include "nsSVGNumber2.h"
#include "nsIDOMSVGFilters.h"
#include "nsCOMPtr.h"
#include "nsISVGFilter.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsSVGEnum.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsSVGNumberList.h"
#include "nsSVGAnimatedNumberList.h"
#include "nsISVGValueUtils.h"
#include "nsSVGFilters.h"
#include "nsSVGUtils.h"
#include "prdtoa.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsSVGAnimatedInteger.h"

nsSVGElement::LengthInfo nsSVGFE::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFE,nsSVGFEBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFE,nsSVGFEBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFE)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFE::nsSVGFE(nsINodeInfo* aNodeInfo) : nsSVGFEBase(aNodeInfo)
{
}

nsresult
nsSVGFE::Init()
{
  nsresult rv = nsSVGFEBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: result ,  #REQUIRED  attrib: result
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mResult));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::result, mResult);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

PRBool
nsSVGFE::ScanDualValueAttribute(const nsAString& aValue, nsIAtom* aAttribute,
                                nsSVGNumber2* aNum1, nsSVGNumber2* aNum2,
                                NumberInfo* aInfo1, NumberInfo* aInfo2,
                                nsAttrValue& aResult)
{
  float x = 0.0f, y = 0.0f;
  char *rest;
  PRBool parseError = PR_FALSE;

  NS_ConvertUTF16toUTF8 value(aValue);
  value.CompressWhitespace(PR_FALSE, PR_TRUE);
  const char *str = value.get();
  x = NS_STATIC_CAST(float, PR_strtod(str, &rest));
  if (str == rest) {
    //first value was illformed
    parseError = PR_TRUE;
  } else {
    if (*rest == '\0') {
      //second value was not supplied
      y = x;
    } else {
      y = NS_STATIC_CAST(float, PR_strtod(rest, &rest));
      if (*rest != '\0') {
        //second value was illformed or there was trailing content
        parseError = PR_TRUE;
      }
    }
  }

  if (parseError) {
    ReportAttributeParseFailure(GetOwnerDoc(), aAttribute, aValue);
    x = aInfo1->mDefaultValue;
    y = aInfo2->mDefaultValue;
    return PR_FALSE;
  }
  aNum1->SetBaseValue(x, this, PR_FALSE);
  aNum2->SetBaseValue(y, this, PR_FALSE);
  aResult.SetTo(aValue);
  return PR_TRUE;
}

nsSVGFilterInstance::ColorModel
nsSVGFE::GetColorModel(nsSVGFilterInstance::ColorModel::AlphaChannel aAlphaChannel)
{
  nsSVGFilterInstance::ColorModel 
    colorModel(nsSVGFilterInstance::ColorModel::LINEAR_RGB,
               aAlphaChannel);

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame)
    return colorModel;

  nsStyleContext* style = frame->GetStyleContext();
  if (style->GetStyleSVG()->mColorInterpolationFilters ==
      NS_STYLE_COLOR_INTERPOLATION_SRGB)
    colorModel.mColorSpace = nsSVGFilterInstance::ColorModel::SRGB;

  return colorModel;
}

//----------------------------------------------------------------------
// nsIDOMSVGFilterPrimitiveStandardAttributes methods

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGFE::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGFE::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGFE::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGFE::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

/* readonly attribute nsIDOMSVGAnimatedString result; */
NS_IMETHODIMP nsSVGFE::GetResult(nsIDOMSVGAnimatedString * *aResult)
{
  *aResult = mResult;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGFE::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//--------------------Filter Resource-----------------------
/**
  * nsSVGFilterResource provides functionality for managing images used by
  * filters.  PLEASE NOTE that nsSVGFilterResource should ONLY be used on the
  * stack because it has nsAutoString member.  Also note that ReleaseTarget,
  * and thus the destructor, depends on AcquireSourceImage having been called
  * one or more times before it is executed.
  */
class nsSVGFilterResource
{

public:
  nsSVGFilterResource(nsSVGFilterInstance* aInstance,
      const nsSVGFilterInstance::ColorModel &aColorModel);
  ~nsSVGFilterResource();

  /*
   * Acquires a source image for reading
   * aIn:         the name of the filter primitive to use as the source
   * aFilter:     the filter that is calling AcquireImage
   * aSourceData: out parameter - the image data of the filter primitive
   *              specified by aIn
   * aSurface:    optional out parameter - the surface of the filter
   *              primitive image specified by aIn
   */
  nsresult AcquireSourceImage(nsIDOMSVGAnimatedString* aIn,
                              nsSVGFE* aFilter, PRUint8** aSourceData,
                              cairo_surface_t** aSurface = 0);
  /*
   * Acquiring a target image will create a new surface to be used as the
   * target image.
   * aResult:     the name by which the resulting filter primitive image will
   *              be identified
   * aTargetData: out parameter - the resulting filter primitive image data
   * aSurface:    optional out parameter - the resulting filter primitive image 
   *              surface
   */
  nsresult AcquireTargetImage(nsIDOMSVGAnimatedString* aResult,
                              PRUint8** aTargetData,
                              cairo_surface_t** aSurface = 0);
  const nsRect& GetRect() {
    return mRect;
  }

  /*
   * Returns total length of data buffer in bytes
   */
  PRUint32 GetDataLength() {
    return mStride * mHeight;
  }

  /*
   * Returns the total number of bytes per row of image data
   */
  PRInt32 GetDataStride() {
    return mStride;
  }

  /*
   * Copy current sourceImage to targetImage
   */
  void CopySourceImage() { CopyImageSubregion(mTargetData, mSourceData); }

  /*
   * Copy subregion from aSrc to aDest.
   current sourceImage to targetImage
   */
  void CopyImageSubregion(PRUint8 *aDest, const PRUint8 *aSrc);

private:
  void ReleaseTarget();

  nsAutoString mInput, mResult;
  nsRect mRect;
  cairo_surface_t *mTargetImage;
  nsSVGFilterInstance* mInstance;
  PRUint8 *mSourceData, *mTargetData;
  PRUint32 mWidth, mHeight;
  PRInt32 mStride;
  nsSVGFilterInstance::ColorModel mColorModel;
};

nsSVGFilterResource::nsSVGFilterResource(nsSVGFilterInstance* aInstance,
                                         const nsSVGFilterInstance::ColorModel &aColorModel) :
  mTargetImage(nsnull),
  mInstance(aInstance),
  mSourceData(nsnull),
  mTargetData(nsnull),
  mWidth(0),
  mHeight(0),
  mStride(0),
  mColorModel(aColorModel)
{
}

nsSVGFilterResource::~nsSVGFilterResource()
{
  ReleaseTarget();
}

nsresult
nsSVGFilterResource::AcquireSourceImage(nsIDOMSVGAnimatedString* aIn,
                                        nsSVGFE* aFilter, PRUint8** aSourceData,
                                        cairo_surface_t** aSurface)
{
  aIn->GetAnimVal(mInput);

  nsRect defaultRect;
  cairo_surface_t *surface;
  mInstance->LookupImage(mInput, &surface, &defaultRect, mColorModel);
  if (!surface) {
    return NS_ERROR_FAILURE;
  }

  if (aSurface) {
    *aSurface = surface;
  }
  mInstance->GetFilterSubregion(aFilter, defaultRect, &mRect);

  mSourceData = cairo_image_surface_get_data(surface);
  mStride = cairo_image_surface_get_stride(surface);

  *aSourceData = mSourceData;
  return NS_OK;
}

nsresult
nsSVGFilterResource::AcquireTargetImage(nsIDOMSVGAnimatedString* aResult,
                                        PRUint8** aTargetData,
                                        cairo_surface_t** aSurface)
{
  aResult->GetAnimVal(mResult);
  mTargetImage = mInstance->GetImage();
  if (!mTargetImage) {
    return NS_ERROR_FAILURE;
  }

  if (aSurface) {
    *aSurface = mTargetImage;
  }
  mTargetData = cairo_image_surface_get_data(mTargetImage);
  mStride = cairo_image_surface_get_stride(mTargetImage);
  mWidth = cairo_image_surface_get_width(mTargetImage);
  mHeight = cairo_image_surface_get_height(mTargetImage);

  *aTargetData = mTargetData;
  return NS_OK;
}

void
nsSVGFilterResource::ReleaseTarget()
{
  if (!mTargetImage) {
    return;
  }
  mInstance->DefineImage(mResult, mTargetImage, mRect, mColorModel);

  // filter instance now owns the image
  cairo_surface_destroy(mTargetImage);
  mTargetImage = nsnull;
}

void
nsSVGFilterResource::CopyImageSubregion(PRUint8 *aDest, const PRUint8 *aSrc)
{
  if (!aDest || !aSrc)
    return;

  for (PRInt32 y = mRect.y; y < mRect.YMost(); y++) {
    memcpy(aDest + y * mStride + 4 * mRect.x,
           aSrc + y * mStride + 4 * mRect.x,
           4 * mRect.width);
  }
}

//---------------------Gaussian Blur------------------------

typedef nsSVGFE nsSVGFEGaussianBlurElementBase;

class nsSVGFEGaussianBlurElement : public nsSVGFEGaussianBlurElementBase,
                                   public nsIDOMSVGFEGaussianBlurElement,
                                   public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEGaussianBlurElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
  nsSVGFEGaussianBlurElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEGaussianBlurElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Gaussian
  NS_DECL_NSIDOMSVGFEGAUSSIANBLURELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEGaussianBlurElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEGaussianBlurElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEGaussianBlurElementBase::)

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { STD_DEV_X, STD_DEV_Y };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;

private:

  void BoxBlurH(PRUint8 *aInput, PRUint8 *aOutput,
                PRInt32 aStride, nsRect aRegion,
                PRUint32 leftLobe, PRUint32 rightLobe);

  void BoxBlurV(PRUint8 *aInput, PRUint8 *aOutput,
                PRInt32 aStride, nsRect aRegion,
                unsigned topLobe, unsigned bottomLobe);

  nsresult GaussianBlur(PRUint8 *aInput, PRUint8 *aOutput,
                        nsSVGFilterResource *aFilterResource,
                        float aStdX, float aStdY);

};

nsSVGElement::NumberInfo nsSVGFEGaussianBlurElement::sNumberInfo[2] =
{
  { &nsGkAtoms::stdDeviation, 0 },
  { &nsGkAtoms::stdDeviation, 0 }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEGaussianBlur)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEGaussianBlurElement,nsSVGFEGaussianBlurElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEGaussianBlurElement,nsSVGFEGaussianBlurElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEGaussianBlurElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEGaussianBlurElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEGaussianBlurElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEGaussianBlurElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEGaussianBlurElement::nsSVGFEGaussianBlurElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEGaussianBlurElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEGaussianBlurElement::Init()
{
  nsresult rv = nsSVGFEGaussianBlurElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEGaussianBlurElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEGaussianBlurElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber stdDeviationX; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetStdDeviationX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberAttributes[STD_DEV_X].ToDOMAnimatedNumber(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber stdDeviationY; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetStdDeviationY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberAttributes[STD_DEV_Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  mNumberAttributes[STD_DEV_X].SetBaseValue(stdDeviationX, this, PR_TRUE);
  mNumberAttributes[STD_DEV_Y].SetBaseValue(stdDeviationY, this, PR_TRUE);
  return NS_OK;
}

PRBool
nsSVGFEGaussianBlurElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                           const nsAString& aValue,
                                           nsAttrValue& aResult)
{
  if (aName == nsGkAtoms::stdDeviation && aNameSpaceID == kNameSpaceID_None) {
    return ScanDualValueAttribute(aValue, nsGkAtoms::stdDeviation,
                                  &mNumberAttributes[STD_DEV_X],
                                  &mNumberAttributes[STD_DEV_Y],
                                  &sNumberInfo[STD_DEV_X],
                                  &sNumberInfo[STD_DEV_Y],
                                  aResult);
  }
  return nsSVGFEGaussianBlurElementBase::ParseAttribute(aNameSpaceID, aName,
                                                        aValue, aResult);
}

void
nsSVGFEGaussianBlurElement::BoxBlurH(PRUint8 *aInput, PRUint8 *aOutput,
                                     PRInt32 aStride, nsRect aRegion,
                                     PRUint32 leftLobe, PRUint32 rightLobe)
{
  PRInt32 boxSize = leftLobe + rightLobe + 1;

  for (PRInt32 y = aRegion.y; y < aRegion.YMost(); y++) {
    PRUint32 sums[4] = {0, 0, 0, 0};
    for (PRInt32 i=0; i < boxSize; i++) {
      PRInt32 pos = aRegion.x - leftLobe + i;
      pos = PR_MAX(pos, aRegion.x);
      pos = PR_MIN(pos, aRegion.XMost() - 1);
      sums[0] += aInput[aStride*y + 4*pos    ];
      sums[1] += aInput[aStride*y + 4*pos + 1];
      sums[2] += aInput[aStride*y + 4*pos + 2];
      sums[3] += aInput[aStride*y + 4*pos + 3];
    }
    for (PRInt32 x = aRegion.x; x < aRegion.XMost(); x++) {
      PRInt32 tmp = x - leftLobe;
      PRInt32 last = PR_MAX(tmp, aRegion.x);
      PRInt32 next = PR_MIN(tmp + boxSize, aRegion.XMost() - 1);

      aOutput[aStride*y + 4*x    ] = sums[0]/boxSize;
      aOutput[aStride*y + 4*x + 1] = sums[1]/boxSize;
      aOutput[aStride*y + 4*x + 2] = sums[2]/boxSize;
      aOutput[aStride*y + 4*x + 3] = sums[3]/boxSize;

      sums[0] += aInput[aStride*y + 4*next    ] -
                 aInput[aStride*y + 4*last    ];
      sums[1] += aInput[aStride*y + 4*next + 1] -
                 aInput[aStride*y + 4*last + 1];
      sums[2] += aInput[aStride*y + 4*next + 2] -
                 aInput[aStride*y + 4*last + 2];
      sums[3] += aInput[aStride*y + 4*next + 3] -
                 aInput[aStride*y + 4*last + 3];
    }
  }
}

void
nsSVGFEGaussianBlurElement::BoxBlurV(PRUint8 *aInput, PRUint8 *aOutput,
                                     PRInt32 aStride, nsRect aRegion,
                                     unsigned topLobe, unsigned bottomLobe)
{
  PRInt32 boxSize = topLobe + bottomLobe + 1;

  for (PRInt32 x = aRegion.x; x < aRegion.XMost(); x++) {
    PRUint32 sums[4] = {0, 0, 0, 0};
    for (PRInt32 i=0; i < boxSize; i++) {
      PRInt32 pos = aRegion.y - topLobe + i;
      pos = PR_MAX(pos, aRegion.y);
      pos = PR_MIN(pos, aRegion.YMost() - 1);
      sums[0] += aInput[aStride*pos + 4*x    ];
      sums[1] += aInput[aStride*pos + 4*x + 1];
      sums[2] += aInput[aStride*pos + 4*x + 2];
      sums[3] += aInput[aStride*pos + 4*x + 3];
    }
    for (PRInt32 y = aRegion.y; y < aRegion.YMost(); y++) {
      PRInt32 tmp = y - topLobe;
      PRInt32 last = PR_MAX(tmp, aRegion.y);
      PRInt32 next = PR_MIN(tmp + boxSize, aRegion.YMost() - 1);

      aOutput[aStride*y + 4*x    ] = sums[0]/boxSize;
      aOutput[aStride*y + 4*x + 1] = sums[1]/boxSize;
      aOutput[aStride*y + 4*x + 2] = sums[2]/boxSize;
      aOutput[aStride*y + 4*x + 3] = sums[3]/boxSize;

      sums[0] += aInput[aStride*next + 4*x    ] -
                 aInput[aStride*last + 4*x    ];
      sums[1] += aInput[aStride*next + 4*x + 1] -
                 aInput[aStride*last + 4*x + 1];
      sums[2] += aInput[aStride*next + 4*x + 2] -
                 aInput[aStride*last + 4*x + 2];
      sums[3] += aInput[aStride*next + 4*x + 3] -
                 aInput[aStride*last + 4*x + 3];
    }
  }
}

nsresult
nsSVGFEGaussianBlurElement::GaussianBlur(PRUint8 *aInput, PRUint8 *aOutput,
                                         nsSVGFilterResource *aFilterResource,
                                         float aStdX, float aStdY)
{
  if (aStdX < 0 || aStdY < 0) {
    return NS_ERROR_FAILURE;
  }

  if (aStdX == 0 || aStdY == 0) {
    return NS_OK;
  }

  PRUint32 dX, dY;
  dX = (PRUint32) floor(aStdX * 3*sqrt(2*M_PI)/4 + 0.5);
  dY = (PRUint32) floor(aStdY * 3*sqrt(2*M_PI)/4 + 0.5);

  PRUint8 *tmp = NS_STATIC_CAST(PRUint8*,
                                calloc(aFilterResource->GetDataLength(), 1));
  nsRect rect = aFilterResource->GetRect();
  PRUint32 stride = aFilterResource->GetDataStride();

  if (dX & 1) {
    // odd
    BoxBlurH(aInput, tmp,  stride, rect, dX/2, dX/2);
    BoxBlurH(tmp, aOutput, stride, rect, dX/2, dX/2);
    BoxBlurH(aOutput, tmp, stride, rect, dX/2, dX/2);
  } else {
    // even
    if (dX == 0) {
      aFilterResource->CopyImageSubregion(tmp, aInput);
    } else {
      BoxBlurH(aInput, tmp,  stride, rect, dX/2,     dX/2 - 1);
      BoxBlurH(tmp, aOutput, stride, rect, dX/2 - 1, dX/2);
      BoxBlurH(aOutput, tmp, stride, rect, dX/2,     dX/2);
    }
  }

  if (dY & 1) {
    // odd
    BoxBlurV(tmp, aOutput, stride, rect, dY/2, dY/2);
    BoxBlurV(aOutput, tmp, stride, rect, dY/2, dY/2);
    BoxBlurV(tmp, aOutput, stride, rect, dY/2, dY/2);
  } else {
    // even
    if (dY == 0) {
      aFilterResource->CopyImageSubregion(aOutput, tmp);
    } else {
      BoxBlurV(tmp, aOutput, stride, rect, dY/2,     dY/2 - 1);
      BoxBlurV(aOutput, tmp, stride, rect, dY/2 - 1, dY/2);
      BoxBlurV(tmp, aOutput, stride, rect, dY/2,     dY/2);
    }
  }

  free(tmp);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_tor
  nsRect rect = fr.GetRect();
  fprintf(stderr, "FILTER GAUSS rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  float stdX, stdY;
  nsSVGLength2 val;

  GetAnimatedNumberValues(&stdX, &stdY, nsnull);
  val.Init(nsSVGUtils::X, 0xff, stdX, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  stdX = instance->GetPrimitiveLength(&val);

  val.Init(nsSVGUtils::Y, 0xff, stdY, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  stdY = instance->GetPrimitiveLength(&val);

  return GaussianBlur(sourceData, targetData, &fr, stdX, stdY);
}

static PRUint32
CheckStandardNames(nsIDOMSVGAnimatedString *aStr)
{
  nsAutoString str;
  aStr->GetAnimVal(str);

  if (str.EqualsLiteral("SourceGraphic"))
    return NS_FE_SOURCEGRAPHIC;
  if (str.EqualsLiteral("SourceAlpha"))
    return NS_FE_SOURCEALPHA;
  if (str.EqualsLiteral("BackgroundImage"))
    return NS_FE_BACKGROUNDIMAGE;
  if (str.EqualsLiteral("BackgroundAlpha"))
    return NS_FE_BACKGROUNDALPHA;
  if (str.EqualsLiteral("FillPaint"))
    return NS_FE_FILLPAINT;
  if (str.EqualsLiteral("StrokePaint"))
    return NS_FE_STROKEPAINT;
  return 0;
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEGaussianBlurElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------Blend------------------------

typedef nsSVGFE nsSVGFEBlendElementBase;

class nsSVGFEBlendElement : public nsSVGFEBlendElementBase,
                            public nsIDOMSVGFEBlendElement,
                            public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEBlendElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEBlendElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEBlendElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Blend
  NS_DECL_NSIDOMSVGFEBLENDELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEBlendElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEBlendElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEBlendElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn2;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMode;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEBlend)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEBlendElement,nsSVGFEBlendElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEBlendElement,nsSVGFEBlendElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEBlendElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEBlendElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEBlendElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBlendElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEBlendElement::nsSVGFEBlendElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEBlendElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEBlendElement::Init()
{
  nsresult rv = nsSVGFEBlendElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  static struct nsSVGEnumMapping gModeTypes[] = {
    {&nsGkAtoms::normal, nsSVGFEBlendElement::SVG_MODE_NORMAL},
    {&nsGkAtoms::multiply, nsSVGFEBlendElement::SVG_MODE_MULTIPLY},
    {&nsGkAtoms::screen, nsSVGFEBlendElement::SVG_MODE_SCREEN},
    {&nsGkAtoms::darken, nsSVGFEBlendElement::SVG_MODE_DARKEN},
    {&nsGkAtoms::lighten, nsSVGFEBlendElement::SVG_MODE_LIGHTEN},
    {nsnull, 0}
  };

  // Create mapped properties:
  // DOM property: mode, #IMPLIED attrib: mode
  {
    nsCOMPtr<nsISVGEnum> modes;
    rv = NS_NewSVGEnum(getter_AddRefs(modes),
                       nsSVGFEBlendElement::SVG_MODE_NORMAL,
                       gModeTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mMode), modes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::mode, mMode);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  // DOM property: in2 , #IMPLIED attrib: in2
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn2));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in2, mIn2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEBlendElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEBlendElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEBlendElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFEBlendElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn2;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration mode; */
NS_IMETHODIMP nsSVGFEBlendElement::GetMode(nsIDOMSVGAnimatedEnumeration * *aMode)
{
  *aMode = mMode;
  NS_IF_ADDREF(*aMode);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEBlendElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();
  PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER BLEND rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  fr.CopySourceImage();

  rv = fr.AcquireSourceImage(mIn2, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rect = fr.GetRect();

  PRUint16 mode;
  mMode->GetAnimVal(&mode);

  for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
    for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
      PRUint32 targIndex = y * stride + 4 * x;
      PRUint32 qa = targetData[targIndex + 3];
      PRUint32 qb = sourceData[targIndex + 3];
      for (PRInt32 i = 0; i < 3; i++) {
        PRUint32 ca = targetData[targIndex + i];
        PRUint32 cb = sourceData[targIndex + i];
        PRUint32 val;
        switch (mode) {
          case nsSVGFEBlendElement::SVG_MODE_NORMAL:
            val = (255 - qa) * cb + 255 * ca;
            break;
          case nsSVGFEBlendElement::SVG_MODE_MULTIPLY:
            val = ((255 - qa) * cb + (255 - qb + cb) * ca);
            break;
          case nsSVGFEBlendElement::SVG_MODE_SCREEN:
            val = 255 * (cb + ca) - ca * cb;
            break;
          case nsSVGFEBlendElement::SVG_MODE_DARKEN:
            val = PR_MIN((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          case nsSVGFEBlendElement::SVG_MODE_LIGHTEN:
            val = PR_MAX((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          default:
            return NS_ERROR_FAILURE;
            break;
        }
        val = PR_MIN(val / 255, 255);
        targetData[targIndex + i] =  NS_STATIC_CAST(PRUint8, val);
      }
      PRUint32 alpha = 255 * 255 - (255 - qa) * (255 - qb);
      FAST_DIVIDE_BY_255(targetData[targIndex + 3], alpha);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEBlendElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//---------------------Color Matrix------------------------

typedef nsSVGFE nsSVGFEColorMatrixElementBase;

class nsSVGFEColorMatrixElement : public nsSVGFEColorMatrixElementBase,
                                  public nsIDOMSVGFEColorMatrixElement,
                                  public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEColorMatrixElement(nsIContent **aResult,
                                                nsINodeInfo *aNodeInfo);
  nsSVGFEColorMatrixElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEColorMatrixElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Color Matrix
  NS_DECL_NSIDOMSVGFECOLORMATRIXELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEColorMatrixElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEColorMatrixElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEColorMatrixElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  //virtual NumberAttributesInfo GetNumberInfo();

  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mType;
  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mValues;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEColorMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEColorMatrixElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEColorMatrixElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEColorMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEColorMatrixElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEColorMatrixElement::nsSVGFEColorMatrixElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEColorMatrixElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEColorMatrixElement::Init()
{
  nsresult rv = nsSVGFEColorMatrixElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  static struct nsSVGEnumMapping gTypes[] = {
    {&nsGkAtoms::matrix, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX},
    {&nsGkAtoms::saturate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE},
    {&nsGkAtoms::hueRotate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE},
    {&nsGkAtoms::luminanceToAlpha, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA},
    {nsnull, 0}
  };

  // Create mapped properties:
  // DOM property: type, #IMPLIED attrib: type
  {
    nsCOMPtr<nsISVGEnum> types;
    rv = NS_NewSVGEnum(getter_AddRefs(types),
                       nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX,
                       gTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mType), types);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::type, mType);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: values, #IMPLIED attrib: values
  {
    nsCOMPtr<nsIDOMSVGNumberList> values;
    rv = NS_NewSVGNumberList(getter_AddRefs(values));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedNumberList(getter_AddRefs(mValues), values);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::values, mValues);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEColorMatrixElement)


//----------------------------------------------------------------------
// nsSVGFEColorMatrixElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  *aType = mType;
  NS_IF_ADDREF(*aType);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList values; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetValues(nsIDOMSVGAnimatedNumberList * *aValues)
{
  *aValues = mValues;
  NS_IF_ADDREF(*aValues);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEColorMatrixElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

NS_IMETHODIMP
nsSVGFEColorMatrixElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::UNPREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 type;
  mType->GetAnimVal(&type);

  nsCOMPtr<nsIDOMSVGNumberList> list;
  mValues->GetAnimVal(getter_AddRefs(list));
  PRUint32 num = 0;
  if (list) {
    list->GetNumberOfItems(&num);
  }

  nsRect rect = fr.GetRect();
  PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER COLOR MATRIX rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::values) &&
      (type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE)) {
    // identity matrix filter
    fr.CopySourceImage();
    return NS_OK;
  }

  static const float identityMatrix[] = 
    { 1, 0, 0, 0, 0,
      0, 1, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 1, 0 };

  static const float luminanceToAlphaMatrix[] = 
    { 0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0.2125f, 0.7154f, 0.0721f, 0, 0 };

  nsCOMPtr<nsIDOMSVGNumber> number;
  float colorMatrix[20];
  float s, c;

  switch (type) {
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX:

    if (num != 20)
      return NS_ERROR_FAILURE;

    for(PRUint32 j = 0; j < num; j++) {
      list->GetItem(j, getter_AddRefs(number));
      number->GetValue(&colorMatrix[j]);
    }
    break;
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE:

    if (num != 1)
      return NS_ERROR_FAILURE;

    list->GetItem(0, getter_AddRefs(number));
    number->GetValue(&s);

    if (s > 1 || s < 0)
      return NS_ERROR_FAILURE;

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    colorMatrix[0] = 0.213f + 0.787f * s;
    colorMatrix[1] = 0.715f - 0.715f * s;
    colorMatrix[2] = 0.072f - 0.072f * s;

    colorMatrix[5] = 0.213f - 0.213f * s;
    colorMatrix[6] = 0.715f + 0.285f * s;
    colorMatrix[7] = 0.072f - 0.072f * s;

    colorMatrix[10] = 0.213f - 0.213f * s;
    colorMatrix[11] = 0.715f - 0.715f * s;
    colorMatrix[12] = 0.715f - 0.715f * s;

    break;

  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE:

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    if (num != 1)
      return NS_ERROR_FAILURE;

    float hueRotateValue;
    list->GetItem(0, getter_AddRefs(number));
    number->GetValue(&hueRotateValue);

    c = NS_STATIC_CAST(float, cos(hueRotateValue * M_PI / 180));
    s = NS_STATIC_CAST(float, sin(hueRotateValue * M_PI / 180));

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    colorMatrix[0] = 0.213f + 0.787f * c - 0.213f * s;
    colorMatrix[1] = 0.715f - 0.715f * c - 0.715f * s;
    colorMatrix[2] = 0.072f - 0.072f * c + 0.928f * s;

    colorMatrix[5] = 0.213f - 0.213f * c + 0.143f * s;
    colorMatrix[6] = 0.715f + 0.285f * c + 0.140f * s;
    colorMatrix[7] = 0.072f - 0.072f * c - 0.283f * s;

    colorMatrix[10] = 0.213f - 0.213f * c - 0.787f * s;
    colorMatrix[11] = 0.715f - 0.715f * c + 0.715f * s;
    colorMatrix[12] = 0.072f + 0.928f * c + 0.072f * s;

    break;

  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA:

    memcpy(colorMatrix, luminanceToAlphaMatrix, sizeof(colorMatrix));
    break;

  default:
    return NS_ERROR_FAILURE;
  }

  for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
    for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
      PRUint32 targIndex = y * stride + 4 * x;

      float col[4];
      for (int i = 0, row = 0; i < 4; i++, row += 5) {
        col[i] = sourceData[targIndex + 2] * colorMatrix[row + 0] +
                 sourceData[targIndex + 1] * colorMatrix[row + 1] +
                 sourceData[targIndex + 0] * colorMatrix[row + 2] +
                 sourceData[targIndex + 3] * colorMatrix[row + 3] +
                 255 *                       colorMatrix[row + 4];
        col[i] = PR_MIN(PR_MAX(0, col[i]), 255);
      }
      targetData[targIndex + 2] = NS_STATIC_CAST(PRUint8, col[0]);
      targetData[targIndex + 1] = NS_STATIC_CAST(PRUint8, col[1]);
      targetData[targIndex + 0] = NS_STATIC_CAST(PRUint8, col[2]);
      targetData[targIndex + 3] = NS_STATIC_CAST(PRUint8, col[3]);
    }
  }
  return NS_OK;
}

//---------------------Composite------------------------

typedef nsSVGFE nsSVGFECompositeElementBase;

class nsSVGFECompositeElement : public nsSVGFECompositeElementBase,
                                public nsIDOMSVGFECompositeElement,
                                public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFECompositeElement(nsIContent **aResult,
                                              nsINodeInfo *aNodeInfo);
  nsSVGFECompositeElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFECompositeElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Composite
  NS_DECL_NSIDOMSVGFECOMPOSITEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFECompositeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFECompositeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFECompositeElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { K1, K2, K3, K4 };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn2;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mOperator;
};

nsSVGElement::NumberInfo nsSVGFECompositeElement::sNumberInfo[4] =
{
  { &nsGkAtoms::k1, 0 },
  { &nsGkAtoms::k2, 0 },
  { &nsGkAtoms::k3, 0 },
  { &nsGkAtoms::k4, 0 }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEComposite)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFECompositeElement,nsSVGFECompositeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFECompositeElement,nsSVGFECompositeElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFECompositeElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFECompositeElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFECompositeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFECompositeElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFECompositeElement::nsSVGFECompositeElement(nsINodeInfo *aNodeInfo)
  : nsSVGFECompositeElementBase(aNodeInfo)
{
}

nsresult
nsSVGFECompositeElement::Init()
{
  nsresult rv = nsSVGFECompositeElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  static struct nsSVGEnumMapping gOperatorTypes[] = {
    {&nsGkAtoms::over, nsSVGFECompositeElement::SVG_OPERATOR_OVER},
    {&nsGkAtoms::in, nsSVGFECompositeElement::SVG_OPERATOR_IN},
    {&nsGkAtoms::out, nsSVGFECompositeElement::SVG_OPERATOR_OUT},
    {&nsGkAtoms::atop, nsSVGFECompositeElement::SVG_OPERATOR_ATOP},
    {&nsGkAtoms::xor_, nsSVGFECompositeElement::SVG_OPERATOR_XOR},
    {&nsGkAtoms::arithmetic, nsSVGFECompositeElement::SVG_OPERATOR_ARITHMETIC},
    {nsnull, 0}
  };
  
  // Create mapped properties:
  // DOM property: operator, #IMPLIED attrib: operator
  {
    nsCOMPtr<nsISVGEnum> operators;
    rv = NS_NewSVGEnum(getter_AddRefs(operators),
                       nsIDOMSVGFECompositeElement::SVG_OPERATOR_OVER,
                       gOperatorTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mOperator), operators);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::_operator, mOperator);
    NS_ENSURE_SUCCESS(rv,rv);
  }
    
  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: in2 , #IMPLIED attrib: in2
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn2));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in2, mIn2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFECompositeElement)

//----------------------------------------------------------------------
// nsSVGFECompositeElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFECompositeElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFECompositeElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn2;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}


/* readonly attribute nsIDOMSVGAnimatedEnumeration operator; */
NS_IMETHODIMP nsSVGFECompositeElement::GetOperator(nsIDOMSVGAnimatedEnumeration * *aOperator)
{
  *aOperator = mOperator;
  NS_IF_ADDREF(*aOperator);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber K1; */
NS_IMETHODIMP nsSVGFECompositeElement::GetK1(nsIDOMSVGAnimatedNumber * *aK1)
{
  return mNumberAttributes[K1].ToDOMAnimatedNumber(aK1, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber K2; */
NS_IMETHODIMP nsSVGFECompositeElement::GetK2(nsIDOMSVGAnimatedNumber * *aK2)
{
  return mNumberAttributes[K2].ToDOMAnimatedNumber(aK2, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber K3; */
NS_IMETHODIMP nsSVGFECompositeElement::GetK3(nsIDOMSVGAnimatedNumber * *aK3)
{
  return mNumberAttributes[K3].ToDOMAnimatedNumber(aK3, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber K4; */
NS_IMETHODIMP nsSVGFECompositeElement::GetK4(nsIDOMSVGAnimatedNumber * *aK4)
{
  return mNumberAttributes[K4].ToDOMAnimatedNumber(aK4, this);
}

NS_IMETHODIMP
nsSVGFECompositeElement::SetK(float k1, float k2, float k3, float k4)
{
  mNumberAttributes[K1].SetBaseValue(k1, this, PR_TRUE);
  mNumberAttributes[K2].SetBaseValue(k2, this, PR_TRUE);
  mNumberAttributes[K3].SetBaseValue(k3, this, PR_TRUE);
  mNumberAttributes[K4].SetBaseValue(k4, this, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFECompositeElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  cairo_surface_t *sourceSurface, *targetSurface;

  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));
  rv = fr.AcquireSourceImage(mIn2, this, &sourceData, &sourceSurface);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData, &targetSurface);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 op;
  mOperator->GetAnimVal(&op);

  // Cairo does not support arithmetic operator
  if (op == nsSVGFECompositeElement::SVG_OPERATOR_ARITHMETIC) {
    float k1, k2, k3, k4;
    nsSVGLength2 val;
    GetAnimatedNumberValues(&k1, &k2, &k3, &k4, nsnull);
    val.Init(nsSVGUtils::XY, 0xff, k1, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    k1 = instance->GetPrimitiveLength(&val);
    val.Init(nsSVGUtils::XY, 0xff, k2, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    k2 = instance->GetPrimitiveLength(&val);
    val.Init(nsSVGUtils::XY, 0xff, k3, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    k3 = instance->GetPrimitiveLength(&val);
    val.Init(nsSVGUtils::XY, 0xff, k4, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    k4 = instance->GetPrimitiveLength(&val);

    nsRect rect = fr.GetRect();
    PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER COMPOSITE rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

    // Copy the first source image
    fr.CopySourceImage();

    // Blend in the second source image
    rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
    NS_ENSURE_SUCCESS(rv, rv);
    float k1Scaled = k1 / 255.0f;
    float k4Scaled = k4 / 255.0f;
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
        PRUint32 targIndex = y * stride + 4 * x;
        for (PRInt32 i = 0; i < 4; i++) {
          PRUint8 i2 = targetData[targIndex + i];
          PRUint8 i1 = sourceData[targIndex + i];
          float result = k1Scaled*i1*i2 + k2*i1 + k3*i2 + k4Scaled;
          targetData[targIndex + i] =
                       NS_STATIC_CAST(PRUint8, PR_MIN(PR_MAX(0, result), 255));
        }
      }
    }
    return NS_OK;
  }

  // Cairo supports the operation we are trying to perform
  cairo_t *cr = cairo_create(targetSurface);
  if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
    cairo_destroy(cr);
    return NS_ERROR_FAILURE;
  }

  cairo_set_source_surface(cr, sourceSurface, 0, 0);
  cairo_paint(cr);

  if (op < SVG_OPERATOR_OVER || op > SVG_OPERATOR_XOR) {
    cairo_destroy(cr);
    return NS_ERROR_FAILURE;
  }
  cairo_operator_t opMap[] = { CAIRO_OPERATOR_DEST, CAIRO_OPERATOR_OVER,
                               CAIRO_OPERATOR_IN, CAIRO_OPERATOR_OUT,
                               CAIRO_OPERATOR_ATOP, CAIRO_OPERATOR_XOR };
  cairo_set_operator(cr, opMap[op]);

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData, &sourceSurface);
  NS_ENSURE_SUCCESS(rv, rv);
  cairo_set_source_surface(cr, sourceSurface, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFECompositeElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFECompositeElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------Component Transfer------------------------

typedef nsSVGFE nsSVGFEComponentTransferElementBase;

class nsSVGFEComponentTransferElement : public nsSVGFEComponentTransferElementBase,
                                        public nsIDOMSVGFEComponentTransferElement,
                                        public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                                      nsINodeInfo *aNodeInfo);
  nsSVGFEComponentTransferElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEComponentTransferElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Component Transfer
  NS_DECL_NSIDOMSVGFECOMPONENTTRANSFERELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEComponentTransferElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEComponentTransferElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEComponentTransferElementBase::)

  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEComponentTransfer)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEComponentTransferElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEComponentTransferElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEComponentTransferElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEComponentTransferElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEComponentTransferElement::nsSVGFEComponentTransferElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEComponentTransferElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEComponentTransferElement::Init()
{
  nsresult rv = nsSVGFEComponentTransferElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEComponentTransferElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEComponentTransferElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP
nsSVGFEComponentTransferElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEComponentTransferElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance, 
                         GetColorModel(nsSVGFilterInstance::ColorModel::UNPREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER COMPONENT rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  PRUint8 tableR[256], tableG[256], tableB[256], tableA[256];
  for (int i=0; i<256; i++)
    tableR[i] = tableG[i] = tableB[i] = tableA[i] = i;
  PRUint32 count = GetChildCount();
  for (PRUint32 k = 0; k < count; k++) {
    nsCOMPtr<nsIContent> child = GetChildAt(k);
    nsCOMPtr<nsIDOMSVGFEFuncRElement> elementR = do_QueryInterface(child);
    nsCOMPtr<nsIDOMSVGFEFuncGElement> elementG = do_QueryInterface(child);
    nsCOMPtr<nsIDOMSVGFEFuncBElement> elementB = do_QueryInterface(child);
    nsCOMPtr<nsIDOMSVGFEFuncAElement> elementA = do_QueryInterface(child);
    if (elementR)
      elementR->GenerateLookupTable(tableR);
    if (elementG)
      elementG->GenerateLookupTable(tableG);
    if (elementB)
      elementB->GenerateLookupTable(tableB);
    if (elementA)
      elementA->GenerateLookupTable(tableA);
  }

  PRInt32 stride = fr.GetDataStride();
  for (PRInt32 y = rect.y; y < rect.YMost(); y++)
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRInt32 targIndex = y * stride + x * 4;
      targetData[targIndex] = tableB[sourceData[targIndex]];
      targetData[targIndex + 1] = tableG[sourceData[targIndex + 1]];
      targetData[targIndex + 2] = tableR[sourceData[targIndex + 2]];
      targetData[targIndex + 3] = tableA[sourceData[targIndex + 3]];
    }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEComponentTransferElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//--------------------------------------------

typedef nsSVGElement nsSVGComponentTransferFunctionElementBase;

class nsSVGComponentTransferFunctionElement : public nsSVGComponentTransferFunctionElementBase
{
protected:
  friend nsresult NS_NewSVGComponentTransferFunctionElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGComponentTransferFunctionElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  // nsIDOMSVGComponentTransferFunctionElement properties:
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mType;
  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mTableValues;

  enum { SLOPE, INTERCEPT, AMPLITUDE, EXPONENT, OFFSET };
  nsSVGNumber2 mNumberAttributes[5];
  static NumberInfo sNumberInfo[5];

};

nsSVGElement::NumberInfo nsSVGComponentTransferFunctionElement::sNumberInfo[5] =
{
  { &nsGkAtoms::slope,     0 },
  { &nsGkAtoms::intercept, 0 },
  { &nsGkAtoms::amplitude, 0 },
  { &nsGkAtoms::exponent,  0 },
  { &nsGkAtoms::offset,    0 }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGComponentTransferFunctionElement,nsSVGComponentTransferFunctionElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGComponentTransferFunctionElement,nsSVGComponentTransferFunctionElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGComponentTransferFunctionElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGComponentTransferFunctionElement::nsSVGComponentTransferFunctionElement(nsINodeInfo* aNodeInfo)
  : nsSVGComponentTransferFunctionElementBase(aNodeInfo)
{
}

nsresult
nsSVGComponentTransferFunctionElement::Init()
{
  nsresult rv;

  // enumeration mappings
  static struct nsSVGEnumMapping gComponentTransferTypes[] = {
    {&nsGkAtoms::identity,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY},
    {&nsGkAtoms::table,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_TABLE},
    {&nsGkAtoms::discrete,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE},
    {&nsGkAtoms::linear,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_LINEAR},
    {&nsGkAtoms::gamma,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_GAMMA},
    {nsnull, 0}
  };

  // Create mapped properties:

  // DOM property: type, #IMPLIED attrib: type
  {
    nsCOMPtr<nsISVGEnum> types;
    rv = NS_NewSVGEnum(getter_AddRefs(types),
                       nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY,
                       gComponentTransferTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mType), types);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::type, mType);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: tableValues, #IMPLIED attrib: tableValues
  {
    nsCOMPtr<nsIDOMSVGNumberList> values;
    rv = NS_NewSVGNumberList(getter_AddRefs(values));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedNumberList(getter_AddRefs(mTableValues), values);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::tableValues, mTableValues);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGComponentTransferFunctionElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  *aType = mType;
  NS_IF_ADDREF(*aType);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList tableValues; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetTableValues(nsIDOMSVGAnimatedNumberList * *aTableValues)
{
  *aTableValues = mTableValues;
  NS_IF_ADDREF(*aTableValues);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber slope; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetSlope(nsIDOMSVGAnimatedNumber * *aSlope)
{
  return mNumberAttributes[SLOPE].ToDOMAnimatedNumber(aSlope, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber intercept; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetIntercept(nsIDOMSVGAnimatedNumber * *aIntercept)
{
  return mNumberAttributes[INTERCEPT].ToDOMAnimatedNumber(aIntercept, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber amplitude; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetAmplitude(nsIDOMSVGAnimatedNumber * *aAmplitude)
{
  return mNumberAttributes[AMPLITUDE].ToDOMAnimatedNumber(aAmplitude, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber exponent; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetExponent(nsIDOMSVGAnimatedNumber * *aExponent)
{
  return mNumberAttributes[EXPONENT].ToDOMAnimatedNumber(aExponent, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber offset; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  return mNumberAttributes[OFFSET].ToDOMAnimatedNumber(aOffset, this);
}

NS_IMETHODIMP
nsSVGComponentTransferFunctionElement::GenerateLookupTable(PRUint8 *aTable)
{
  PRUint16 type;
  mType->GetAnimVal(&type);

  float slope, intercept, amplitude, exponent, offset;
  GetAnimatedNumberValues(&slope, &intercept, &amplitude, 
                          &exponent, &offset, nsnull);

  PRUint32 i;

  switch (type) {
  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
  {
    nsCOMPtr<nsIDOMSVGNumberList> list;
    nsCOMPtr<nsIDOMSVGNumber> number;
    mTableValues->GetAnimVal(getter_AddRefs(list));
    PRUint32 num = 0;
    if (list)
      list->GetNumberOfItems(&num);
    if (num <= 1)
      break;

    for (i = 0; i < 256; i++) {
      PRInt32 k = (i * (num - 1)) / 255;
      float v1, v2;
      list->GetItem(k, getter_AddRefs(number));
      number->GetValue(&v1);
      list->GetItem(PR_MIN(k + 1, num - 1), getter_AddRefs(number));
      number->GetValue(&v2);
      PRInt32 val =
        PRInt32(255 * (v1 + (i/255.0f - k/float(num-1))*(num - 1)*(v2 - v1)));
      val = PR_MIN(255, val);
      val = PR_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
  {
    nsCOMPtr<nsIDOMSVGNumberList> list;
    nsCOMPtr<nsIDOMSVGNumber> number;
    mTableValues->GetAnimVal(getter_AddRefs(list));
    PRUint32 num = 0;
    if (list)
      list->GetNumberOfItems(&num);
    if (num <= 1)
      break;

    for (i = 0; i < 256; i++) {
      PRInt32 k = (i * num) / 255;
      k = PR_MIN(k, num - 1);
      float v;
      list->GetItem(k, getter_AddRefs(number));
      number->GetValue(&v);
      PRInt32 val = PRInt32(255 * v);
      val = PR_MIN(255, val);
      val = PR_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
  {
    for (i = 0; i < 256; i++) {
      PRInt32 val = PRInt32(slope * i + 255 * intercept);
      val = PR_MIN(255, val);
      val = PR_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
  {
    for (i = 0; i < 256; i++) {
      PRInt32 val = PRInt32(255 * (amplitude * pow(i / 255.0f, exponent) + offset));
      val = PR_MIN(255, val);
      val = PR_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
  default:
    break;
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGComponentTransferFunctionElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

class nsSVGFEFuncRElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncRElement
{
protected:
  friend nsresult NS_NewSVGFEFuncRElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEFuncRElement(nsINodeInfo* aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCRELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncRElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncRElement,nsSVGComponentTransferFunctionElement)

NS_INTERFACE_MAP_BEGIN(nsSVGFEFuncRElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGComponentTransferFunctionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEFuncRElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEFuncRElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncR)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncRElement)


class nsSVGFEFuncGElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncGElement
{
protected:
  friend nsresult NS_NewSVGFEFuncGElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEFuncGElement(nsINodeInfo* aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCGELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncGElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncGElement,nsSVGComponentTransferFunctionElement)

NS_INTERFACE_MAP_BEGIN(nsSVGFEFuncGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGComponentTransferFunctionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEFuncGElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEFuncGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncG)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncGElement)


class nsSVGFEFuncBElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncBElement
{
protected:
  friend nsresult NS_NewSVGFEFuncBElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEFuncBElement(nsINodeInfo* aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCBELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncBElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncBElement,nsSVGComponentTransferFunctionElement)

NS_INTERFACE_MAP_BEGIN(nsSVGFEFuncBElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGComponentTransferFunctionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEFuncBElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEFuncBElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncB)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncBElement)


class nsSVGFEFuncAElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncAElement
{
protected:
  friend nsresult NS_NewSVGFEFuncAElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEFuncAElement(nsINodeInfo* aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCAELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncAElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncAElement,nsSVGComponentTransferFunctionElement)

NS_INTERFACE_MAP_BEGIN(nsSVGFEFuncAElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGComponentTransferFunctionElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEFuncAElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEFuncAElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncA)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncAElement)

//---------------------Merge------------------------

typedef nsSVGFE nsSVGFEMergeElementBase;

class nsSVGFEMergeElement : public nsSVGFEMergeElementBase,
                            public nsIDOMSVGFEMergeElement,
                            public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEMergeElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEMergeElement(nsINodeInfo* aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEMergeElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Merge
  NS_DECL_NSIDOMSVGFEMERGEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMergeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeElementBase::)

  // nsIContent

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMerge)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMergeElement,nsSVGFEMergeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMergeElement,nsSVGFEMergeElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEMergeElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEMergeElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEMergeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMergeElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEMergeElement::nsSVGFEMergeElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEMergeElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMergeElement)

NS_IMETHODIMP
nsSVGFEMergeElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  cairo_surface_t *sourceSurface, *targetSurface;

  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));
  rv = fr.AcquireTargetImage(mResult, &targetData, &targetSurface);
  NS_ENSURE_SUCCESS(rv, rv);

  cairo_t *cr = cairo_create(targetSurface);
  if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
    cairo_destroy(cr);
    return NS_ERROR_FAILURE;
  }

  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child = GetChildAt(i);
    nsCOMPtr<nsIDOMSVGFEMergeNodeElement> node = do_QueryInterface(child);
    if (!node)
      continue;
    nsCOMPtr<nsIDOMSVGAnimatedString> str;
    node->GetIn1(getter_AddRefs(str));

    rv = fr.AcquireSourceImage(str, this, &sourceData, &sourceSurface);
    NS_ENSURE_SUCCESS(rv, rv);

    cairo_set_source_surface(cr, sourceSurface, 0, 0);
    cairo_paint(cr);
  }
  cairo_destroy(cr);
  return NS_OK;
}


NS_IMETHODIMP
nsSVGFEMergeElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = 0;

  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child = GetChildAt(i);
    nsCOMPtr<nsIDOMSVGFEMergeNodeElement> node = do_QueryInterface(child);
    if (node) {
      nsCOMPtr<nsIDOMSVGAnimatedString> str;
      node->GetIn1(getter_AddRefs(str));
      *aRequirements |= CheckStandardNames(str);
    }
  }

  return NS_OK;
}

//---------------------Merge Node------------------------

typedef nsSVGStylableElement nsSVGFEMergeNodeElementBase;

class nsSVGFEMergeNodeElement : public nsSVGFEMergeNodeElementBase,
                                public nsIDOMSVGFEMergeNodeElement
{
protected:
  friend nsresult NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEMergeNodeElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSVGFEMERGENODEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeNodeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMergeNodeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeNodeElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMergeNode)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMergeNodeElement,nsSVGFEMergeNodeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMergeNodeElement,nsSVGFEMergeNodeElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEMergeNodeElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEMergeNodeElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEMergeNodeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMergeNodeElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEMergeNodeElement::nsSVGFEMergeNodeElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEMergeNodeElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEMergeNodeElement::Init()
{
  nsresult rv = nsSVGFEMergeNodeElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMergeNodeElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEMergeNodeElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEMergeNodeElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}


//---------------------Offset------------------------

typedef nsSVGFE nsSVGFEOffsetElementBase;

class nsSVGFEOffsetElement : public nsSVGFEOffsetElementBase,
                             public nsIDOMSVGFEOffsetElement,
                             public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEOffsetElement(nsIContent **aResult,
                                           nsINodeInfo *aNodeInfo);
  nsSVGFEOffsetElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEOffsetElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Offset
  NS_DECL_NSIDOMSVGFEOFFSETELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEOffsetElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEOffsetElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEOffsetElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { DX, DY };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

nsSVGElement::NumberInfo nsSVGFEOffsetElement::sNumberInfo[2] =
{
  { &nsGkAtoms::dx, 0 },
  { &nsGkAtoms::dy, 0 }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEOffset)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEOffsetElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEOffsetElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEOffsetElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEOffsetElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEOffsetElement::nsSVGFEOffsetElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEOffsetElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEOffsetElement::Init()
{
  nsresult rv = nsSVGFEOffsetElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEOffsetElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEOffsetElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber dx; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetDx(nsIDOMSVGAnimatedNumber * *aDx)
{
  return mNumberAttributes[DX].ToDOMAnimatedNumber(aDx, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber dy; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetDy(nsIDOMSVGAnimatedNumber * *aDy)
{
  return mNumberAttributes[DY].ToDOMAnimatedNumber(aDy, this);
}

NS_IMETHODIMP
nsSVGFEOffsetElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER OFFSET rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  PRInt32 offsetX, offsetY;
  float fltX, fltY;
  nsSVGLength2 val;

  GetAnimatedNumberValues(&fltX, &fltY, nsnull);
  val.Init(nsSVGUtils::X, 0xff, fltX, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  offsetX = (PRInt32) instance->GetPrimitiveLength(&val);

  val.Init(nsSVGUtils::Y, 0xff, fltY, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  offsetY = (PRInt32) instance->GetPrimitiveLength(&val);

  PRInt32 stride = fr.GetDataStride();
  PRInt32 targetColumn = rect.x + offsetX;
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    PRInt32 targetRow = y + offsetY;
    if (targetRow < rect.y || targetRow >= rect.YMost())
      continue;

    if (targetColumn < rect.x)
      memcpy(targetData + stride * targetRow + 4 * rect.x,
             sourceData + stride * y - 4 * offsetX,
             4 * (rect.width + offsetX));
    else
      memcpy(targetData + stride * targetRow + 4 * targetColumn,
             sourceData + stride * y + 4 * rect.x,
             4 * (rect.width - offsetX));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEOffsetElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEOffsetElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------Flood------------------------

typedef nsSVGFE nsSVGFEFloodElementBase;

class nsSVGFEFloodElement : public nsSVGFEFloodElementBase,
                            public nsIDOMSVGFEFloodElement,
                            public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEFloodElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEFloodElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEFloodElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Flood
  NS_DECL_NSIDOMSVGFEFLOODELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEFloodElementBase::)
  
  NS_FORWARD_NSIDOMNODE(nsSVGFEFloodElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEFloodElementBase::)

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFlood)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEFloodElement,nsSVGFEFloodElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFloodElement,nsSVGFEFloodElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEFloodElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEFloodElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEFloodElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEFloodElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEFloodElement::nsSVGFEFloodElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEFloodElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEFloodElement::Init()
{
  nsresult rv = nsSVGFEFloodElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFloodElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEFloodElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEFloodElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEFloodElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  cairo_surface_t *targetSurface;
  // flood colour is sRGB
  nsSVGFilterInstance::ColorModel
    colorModel(nsSVGFilterInstance::ColorModel::SRGB, 
               nsSVGFilterInstance::ColorModel::PREMULTIPLIED);
  nsSVGFilterResource fr(instance, colorModel);

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData, &targetSurface);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->GetStyleContext();

  nscolor floodColor = style->GetStyleSVGReset()->mFloodColor;
  float floodOpacity = style->GetStyleSVGReset()->mFloodOpacity;

  cairo_t *cr = cairo_create(targetSurface);
  if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
    cairo_destroy(cr);
    return NS_ERROR_FAILURE;
  }
  cairo_set_source_rgba(cr, 
                        NS_GET_R(floodColor) / 255.0,
                        NS_GET_G(floodColor) / 255.0,
                        NS_GET_B(floodColor) / 255.0,
                        floodOpacity);
  cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
  cairo_fill(cr);
  cairo_destroy(cr);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEFloodElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGFEFloodElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFEFloodElementBase::IsAttributeMapped(name);
}

//---------------------Turbulence------------------------

typedef nsSVGFE nsSVGFETurbulenceElementBase;

class nsSVGFETurbulenceElement : public nsSVGFETurbulenceElementBase,
                                 public nsIDOMSVGFETurbulenceElement,
                                 public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFETurbulenceElement(nsIContent **aResult,
                                               nsINodeInfo *aNodeInfo);
  nsSVGFETurbulenceElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFETurbulenceElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Turbulence
  NS_DECL_NSIDOMSVGFETURBULENCEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFETurbulenceElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFETurbulenceElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFETurbulenceElementBase::)

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { BASE_FREQ_X, BASE_FREQ_Y, SEED}; // floating point seed?!
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];

  nsCOMPtr<nsIDOMSVGAnimatedInteger> mNumOctaves;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mStitchTiles;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mType;

private:

  /* The turbulence calculation code is an adapted version of what
     appears in the SVG 1.1 specification:
         http://www.w3.org/TR/SVG11/filters.html#feTurbulence
  */

  /* Produces results in the range [1, 2**31 - 2].
     Algorithm is: r = (a * r) mod m
     where a = 16807 and m = 2**31 - 1 = 2147483647
     See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
     To test: the algorithm should produce the result 1043618065
     as the 10,000th generated number if the original seed is 1.
  */
#define RAND_M 2147483647	/* 2**31 - 1 */
#define RAND_A 16807		/* 7**5; primitive root of m */
#define RAND_Q 127773		/* m / a */
#define RAND_R 2836		/* m % a */

  PRInt32 SetupSeed(PRInt32 aSeed) {
    if (aSeed <= 0)
      aSeed = -(aSeed % (RAND_M - 1)) + 1;
    if (aSeed > RAND_M - 1)
      aSeed = RAND_M - 1;
    return aSeed;
  }

  PRUint32 Random(PRUint32 aSeed) {
    PRInt32 result = RAND_A * (aSeed % RAND_Q) - RAND_R * (aSeed / RAND_Q);
    if (result <= 0)
      result += RAND_M;
    return result;
  }
#undef RAND_M
#undef RAND_A
#undef RAND_Q
#undef RAND_R

  const static int sBSize = 0x100;
  const static int sBM = 0xff;
  const static int sPerlinN = 0x1000;
  const static int sNP = 12;			/* 2^PerlinN */
  const static int sNM = 0xfff;

  PRInt32 mLatticeSelector[sBSize + sBSize + 2];
  double mGradient[4][sBSize + sBSize + 2][2];
  struct StitchInfo {
    int mWidth;			// How much to subtract to wrap for stitching.
    int mHeight;
    int mWrapX;			// Minimum value to wrap.
    int mWrapY;
  };

  void Init(PRInt32 aSeed);
  double Noise2(int aColorChannel, double aVec[2], StitchInfo *aStitchInfo);
  double
  Turbulence(int aColorChannel, double *aPoint, double aBaseFreqX,
             double aBaseFreqY, int aNumOctaves, PRBool aFractalSum,
             PRBool aDoStitching, double aTileX, double aTileY,
             double aTileWidth, double aTileHeight);
};

nsSVGElement::NumberInfo nsSVGFETurbulenceElement::sNumberInfo[3] =
{
  { &nsGkAtoms::baseFrequency, 0 },
  { &nsGkAtoms::baseFrequency, 0 },
  { &nsGkAtoms::seed, 0 }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FETurbulence)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFETurbulenceElement,nsSVGFETurbulenceElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFETurbulenceElement,nsSVGFETurbulenceElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFETurbulenceElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFETurbulenceElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFETurbulenceElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFETurbulenceElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFETurbulenceElement::nsSVGFETurbulenceElement(nsINodeInfo *aNodeInfo)
  : nsSVGFETurbulenceElementBase(aNodeInfo)
{
}

nsresult
nsSVGFETurbulenceElement::Init()
{
  nsresult rv = nsSVGFETurbulenceElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // enumeration mappings
  static struct nsSVGEnumMapping gTurbulenceTypes[] = {
    {&nsGkAtoms::fractalNoise,
     nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_FRACTALNOISE},
    {&nsGkAtoms::turbulence,
     nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_TURBULENCE},
    {nsnull, 0}
  };

  static struct nsSVGEnumMapping gStitchTypes[] = {
    {&nsGkAtoms::stitch,
     nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_STITCH},
    {&nsGkAtoms::noStitch,
     nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_NOSTITCH},
    {nsnull, 0}
  };

  // Create mapped properties:

  // DOM property: stitchTiles, #IMPLIED attrib: stitchTiles
  {
    nsCOMPtr<nsISVGEnum> stitch;
    rv = NS_NewSVGEnum(getter_AddRefs(stitch),
                       nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_NOSTITCH,
                       gStitchTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mStitchTiles), stitch);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::stitchTiles, mStitchTiles);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: type, #IMPLIED attrib: type
  {
    nsCOMPtr<nsISVGEnum> types;
    rv = NS_NewSVGEnum(getter_AddRefs(types),
                       nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_TURBULENCE,
                       gTurbulenceTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mType), types);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::type, mType);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: numOctaves ,  #IMPLIED attrib: numOctaves
  {
    rv = NS_NewSVGAnimatedInteger(getter_AddRefs(mNumOctaves), 1);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::numOctaves, mNumOctaves);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFETurbulenceElement)

//----------------------------------------------------------------------
// nsIDOMSVGFETurbulenceElement methods

/* readonly attribute nsIDOMSVGAnimatedNumber baseFrequencyX; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetBaseFrequencyX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberAttributes[BASE_FREQ_X].ToDOMAnimatedNumber(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber baseFrequencyY; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetBaseFrequencyY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberAttributes[BASE_FREQ_Y].ToDOMAnimatedNumber(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedInteger numOctaves; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetNumOctaves(nsIDOMSVGAnimatedInteger * *aNum)
{
  *aNum = mNumOctaves;
  NS_IF_ADDREF(*aNum);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber seed; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetSeed(nsIDOMSVGAnimatedNumber * *aSeed)
{
  return mNumberAttributes[SEED].ToDOMAnimatedNumber(aSeed, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration stitchTiles; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetStitchTiles(nsIDOMSVGAnimatedEnumeration * *aStitch)
{
  *aStitch = mStitchTiles;
  NS_IF_ADDREF(*aStitch);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  *aType = mType;
  NS_IF_ADDREF(*aType);
  return NS_OK;
}

PRBool
nsSVGFETurbulenceElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                         const nsAString& aValue,
                                         nsAttrValue& aResult)
{
  if (aName == nsGkAtoms::baseFrequency && aNameSpaceID == kNameSpaceID_None) {
    return ScanDualValueAttribute(aValue, nsGkAtoms::baseFrequency,
                                  &mNumberAttributes[BASE_FREQ_X],
                                  &mNumberAttributes[BASE_FREQ_Y],
                                  &sNumberInfo[BASE_FREQ_X],
                                  &sNumberInfo[BASE_FREQ_Y],
                                  aResult);
  }
  return nsSVGFETurbulenceElementBase::ParseAttribute(aNameSpaceID, aName,
                                                      aValue, aResult);
}

NS_IMETHODIMP
nsSVGFETurbulenceElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));

  nsIDOMSVGAnimatedString* sourceGraphic = nsnull;
  rv = NS_NewSVGAnimatedString(&sourceGraphic);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = fr.AcquireSourceImage(sourceGraphic, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER TURBULENCE rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  float fX, fY, seed;
  PRInt32 octaves;
  PRUint16 type, stitch;

  GetAnimatedNumberValues(&fX, &fY, &seed, nsnull);
  mNumOctaves->GetAnimVal(&octaves);
  mStitchTiles->GetAnimVal(&stitch);
  mType->GetAnimVal(&type);

  Init((PRInt32)seed);

  float filterX, filterY, filterWidth, filterHeight;
  instance->GetFilterBox(&filterX, &filterY, &filterWidth, &filterHeight);

  PRBool doStitch = PR_FALSE;
  if (stitch == nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_STITCH) {
    doStitch = PR_TRUE;

    float lowFreq, hiFreq;

    lowFreq = floor(filterWidth * fX) / filterWidth;
    hiFreq = ceil(filterWidth * fX) / filterWidth;
    if (fX / lowFreq < hiFreq / fX)
      fX = lowFreq;
    else
      fX = hiFreq;

    lowFreq = floor(filterHeight * fY) / filterHeight;
    hiFreq = ceil(filterHeight * fY) / filterHeight;
    if (fY / lowFreq < hiFreq / fY)
      fY = lowFreq;
    else
      fY = hiFreq;
  }
  PRInt32 stride = fr.GetDataStride();
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRInt32 targIndex = y * stride + x * 4;
      double point[2];
      point[0] = filterX + (filterWidth * x ) / (rect.width - 1);
      point[1] = filterY + (filterHeight * y) / (rect.height - 1);

      float col[4];
      if (type == nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_TURBULENCE) {
        for (int i = 0; i < 4; i++)
          col[i] = Turbulence(i, point, fX, fY, octaves, PR_FALSE,
                              doStitch, filterX, filterY, filterWidth, filterHeight) * 255;
      } else {
        for (int i = 0; i < 4; i++)
          col[i] = (Turbulence(i, point, fX, fY, octaves, PR_TRUE,
                               doStitch, filterX, filterY, filterWidth, filterHeight) * 255 + 255) / 2;
      }
      for (int i = 0; i < 4; i++) {
        col[i] = PR_MIN(col[i], 255);
        col[i] = PR_MAX(col[i], 0);
      }

      PRUint8 r, g, b, a;
      a = PRUint8(col[3]);
      FAST_DIVIDE_BY_255(r, unsigned(col[0]) * a);
      FAST_DIVIDE_BY_255(g, unsigned(col[1]) * a);
      FAST_DIVIDE_BY_255(b, unsigned(col[2]) * a);

      targetData[targIndex    ] = b;
      targetData[targIndex + 1] = g;
      targetData[targIndex + 2] = r;
      targetData[targIndex + 3] = a;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGFETurbulenceElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = 0;
  return NS_OK;
}

void
nsSVGFETurbulenceElement::Init(PRInt32 aSeed)
{
  double s;
  int i, j, k;
  aSeed = SetupSeed(aSeed);
  for (k = 0; k < 4; k++) {
    for (i = 0; i < sBSize; i++) {
      mLatticeSelector[i] = i;
      for (j = 0; j < 2; j++) {
        mGradient[k][i][j] =
          (double) (((aSeed =
                      Random(aSeed)) % (sBSize + sBSize)) - sBSize) / sBSize;
      }
      s = double (sqrt
                  (mGradient[k][i][0] * mGradient[k][i][0] +
                   mGradient[k][i][1] * mGradient[k][i][1]));
      mGradient[k][i][0] /= s;
      mGradient[k][i][1] /= s;
    }
  }
  while (--i) {
    k = mLatticeSelector[i];
    mLatticeSelector[i] = mLatticeSelector[j =
                                           (aSeed =
                                            Random(aSeed)) % sBSize];
    mLatticeSelector[j] = k;
  }
  for (i = 0; i < sBSize + 2; i++) {
    mLatticeSelector[sBSize + i] = mLatticeSelector[i];
    for (k = 0; k < 4; k++)
      for (j = 0; j < 2; j++)
        mGradient[k][sBSize + i][j] = mGradient[k][i][j];
  }
}

#define S_CURVE(t) ( t * t * (3. - 2. * t) )
#define LERP(t, a, b) ( a + t * (b - a) )
double
nsSVGFETurbulenceElement::Noise2(int aColorChannel, double aVec[2],
                                 StitchInfo *aStitchInfo)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  double rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  register long i, j;
  t = aVec[0] + sPerlinN;
  bx0 = (int) t;
  bx1 = bx0 + 1;
  rx0 = t - (int) t;
  rx1 = rx0 - 1.0f;
  t = aVec[1] + sPerlinN;
  by0 = (int) t;
  by1 = by0 + 1;
  ry0 = t - (int) t;
  ry1 = ry0 - 1.0f;
  // If stitching, adjust lattice points accordingly.
  if (aStitchInfo != NULL) {
    if (bx0 >= aStitchInfo->mWrapX)
      bx0 -= aStitchInfo->mWidth;
    if (bx1 >= aStitchInfo->mWrapX)
      bx1 -= aStitchInfo->mWidth;
    if (by0 >= aStitchInfo->mWrapY)
      by0 -= aStitchInfo->mHeight;
    if (by1 >= aStitchInfo->mWrapY)
      by1 -= aStitchInfo->mHeight;
  }
  bx0 &= sBM;
  bx1 &= sBM;
  by0 &= sBM;
  by1 &= sBM;
  i = mLatticeSelector[bx0];
  j = mLatticeSelector[bx1];
  b00 = mLatticeSelector[i + by0];
  b10 = mLatticeSelector[j + by0];
  b01 = mLatticeSelector[i + by1];
  b11 = mLatticeSelector[j + by1];
  sx = double (S_CURVE(rx0));
  sy = double (S_CURVE(ry0));
  q = mGradient[aColorChannel][b00];
  u = rx0 * q[0] + ry0 * q[1];
  q = mGradient[aColorChannel][b10];
  v = rx1 * q[0] + ry0 * q[1];
  a = LERP(sx, u, v);
  q = mGradient[aColorChannel][b01];
  u = rx0 * q[0] + ry1 * q[1];
  q = mGradient[aColorChannel][b11];
  v = rx1 * q[0] + ry1 * q[1];
  b = LERP(sx, u, v);
  return LERP(sy, a, b);
}
#undef S_CURVE
#undef LERP

double
nsSVGFETurbulenceElement::Turbulence(int aColorChannel, double *aPoint,
                                     double aBaseFreqX, double aBaseFreqY,
                                     int aNumOctaves, PRBool aFractalSum,
                                     PRBool aDoStitching,
                                     double aTileX, double aTileY,
                                     double aTileWidth, double aTileHeight)
{
  StitchInfo stitch;
  StitchInfo *stitchInfo = NULL; // Not stitching when NULL.
  // Adjust the base frequencies if necessary for stitching.
  if (aDoStitching) {
    // When stitching tiled turbulence, the frequencies must be adjusted
    // so that the tile borders will be continuous.
    if (aBaseFreqX != 0.0) {
      double loFreq = double (floor(aTileWidth * aBaseFreqX)) / aTileWidth;
      double hiFreq = double (ceil(aTileWidth * aBaseFreqX)) / aTileWidth;
      if (aBaseFreqX / loFreq < hiFreq / aBaseFreqX)
        aBaseFreqX = loFreq;
      else
        aBaseFreqX = hiFreq;
    }
    if (aBaseFreqY != 0.0) {
      double loFreq = double (floor(aTileHeight * aBaseFreqY)) / aTileHeight;
      double hiFreq = double (ceil(aTileHeight * aBaseFreqY)) / aTileHeight;
      if (aBaseFreqY / loFreq < hiFreq / aBaseFreqY)
        aBaseFreqY = loFreq;
      else
        aBaseFreqY = hiFreq;
    }
    // Set up initial stitch values.
    stitchInfo = &stitch;
    stitch.mWidth = int (aTileWidth * aBaseFreqX + 0.5f);
    stitch.mWrapX = int (aTileX * aBaseFreqX + sPerlinN + stitch.mWidth);
    stitch.mHeight = int (aTileHeight * aBaseFreqY + 0.5f);
    stitch.mWrapY = int (aTileY * aBaseFreqY + sPerlinN + stitch.mHeight);
  }
  double sum = 0.0f;
  double vec[2];
  vec[0] = aPoint[0] * aBaseFreqX;
  vec[1] = aPoint[1] * aBaseFreqY;
  double ratio = 1;
  for (int octave = 0; octave < aNumOctaves; octave++) {
    if (aFractalSum)
      sum += double (Noise2(aColorChannel, vec, stitchInfo) / ratio);
    else
      sum += double (fabs(Noise2(aColorChannel, vec, stitchInfo)) / ratio);
    vec[0] *= 2;
    vec[1] *= 2;
    ratio *= 2;
    if (stitchInfo != NULL) {
      // Update stitch values. Subtracting sPerlinN before the multiplication
      // and adding it afterward simplifies to subtracting it once.
      stitch.mWidth *= 2;
      stitch.mWrapX = 2 * stitch.mWrapX - sPerlinN;
      stitch.mHeight *= 2;
      stitch.mWrapY = 2 * stitch.mWrapY - sPerlinN;
    }
  }
  return sum;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFETurbulenceElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------Morphology------------------------

typedef nsSVGFE nsSVGFEMorphologyElementBase;

class nsSVGFEMorphologyElement : public nsSVGFEMorphologyElementBase,
                                 public nsIDOMSVGFEMorphologyElement,
                                 public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEMorphologyElement(nsIContent **aResult,
                                               nsINodeInfo *aNodeInfo);
  nsSVGFEMorphologyElement(nsINodeInfo* aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEMorphologyElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Morphology
  NS_DECL_NSIDOMSVGFEMORPHOLOGYELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMorphologyElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMorphologyElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMorphologyElementBase::)

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { RADIUS_X, RADIUS_Y };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mOperator;
};

nsSVGElement::NumberInfo nsSVGFEMorphologyElement::sNumberInfo[2] =
{
  { &nsGkAtoms::radius, 0 },
  { &nsGkAtoms::radius, 0 }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMorphology)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEMorphologyElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEMorphologyElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEMorphologyElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMorphologyElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEMorphologyElement::nsSVGFEMorphologyElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEMorphologyElementBase(aNodeInfo)
{
}

nsresult
nsSVGFEMorphologyElement::Init()
{
  nsresult rv = nsSVGFEMorphologyElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  static struct nsSVGEnumMapping gOperatorTypes[] = {
    {&nsGkAtoms::erode, nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE},
    {&nsGkAtoms::dilate, nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE},
    {nsnull, 0}
  };

  // Create mapped properties:
  // DOM property: operator, #IMPLIED attrib: operator
  {
    nsCOMPtr<nsISVGEnum> operators;
    rv = NS_NewSVGEnum(getter_AddRefs(operators),
                       nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE,
                       gOperatorTypes);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mOperator), operators);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::_operator, mOperator);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: in1 , #IMPLIED attrib: in
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mIn1));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMorphologyElement)


//----------------------------------------------------------------------
// nsSVGFEMorphologyElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration operator; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetOperator(nsIDOMSVGAnimatedEnumeration * *aOperator)
{
  *aOperator = mOperator;
  NS_IF_ADDREF(*aOperator);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusX; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberAttributes[RADIUS_X].ToDOMAnimatedNumber(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusY; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberAttributes[RADIUS_Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::SetRadius(float rx, float ry)
{
  mNumberAttributes[RADIUS_X].SetBaseValue(rx, this, PR_TRUE);
  mNumberAttributes[RADIUS_Y].SetBaseValue(ry, this, PR_TRUE);
  return NS_OK;
}

PRBool
nsSVGFEMorphologyElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                         const nsAString& aValue,
                                         nsAttrValue& aResult)
{
  if (aName == nsGkAtoms::radius && aNameSpaceID == kNameSpaceID_None) {
    return ScanDualValueAttribute(aValue, nsGkAtoms::radius,
                                  &mNumberAttributes[RADIUS_X],
                                  &mNumberAttributes[RADIUS_Y],
                                  &sNumberInfo[RADIUS_X],
                                  &sNumberInfo[RADIUS_Y],
                                  aResult);
  }
  return nsSVGFEMorphologyElementBase::ParseAttribute(aNameSpaceID, aName,
                                                      aValue, aResult);
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEMorphologyElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(instance,
                         GetColorModel(nsSVGFilterInstance::ColorModel::PREMULTIPLIED));

  rv = fr.AcquireSourceImage(mIn1, this, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetRect();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER MORPH rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  float rx, ry;
  nsSVGLength2 val;

  GetAnimatedNumberValues(&rx, &ry, nsnull);
  val.Init(nsSVGUtils::X, 0xff, rx, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  rx = instance->GetPrimitiveLength(&val);
  val.Init(nsSVGUtils::Y, 0xff, ry, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  ry = instance->GetPrimitiveLength(&val);

  PRInt32 stride = fr.GetDataStride();
  PRUint32 xExt[4], yExt[4];  // X, Y indices of RGBA extrema
  PRUint8 extrema[4];         // RGBA magnitude of extrema
  PRUint16 op;
  mOperator->GetAnimVal(&op);

  if (rx == 0 && ry == 0) {
    fr.CopySourceImage();
    return NS_OK;
  }
  /* Scan the kernel for each pixel to determine max/min RGBA values.  Note that
   * as we advance in the x direction, each kernel overlaps the previous kernel.
   * Thus, we can avoid iterating over the entire kernel by comparing the
   * leading edge of the new kernel against the extrema found in the previous
   * kernel.   We must still scan the entire kernel if the previous extrema do
   * not fall within the current kernel or if we are starting a new row.
   */
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    PRUint32 startY = PR_MAX(0, (int)(y - ry));
    PRUint32 endY = PR_MIN((int)(y + ry), rect.YMost() - 1);
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint32 startX = PR_MAX(0, (int)(x - rx));
      PRUint32 endX = PR_MIN((int)(x + rx), rect.XMost() - 1);
      PRUint32 targIndex = y * stride + 4 * x;

      // We need to scan the entire kernel
      if (x == rect.x || xExt[0]  <= startX || xExt[1] <= startX ||
          xExt[2] <= startX || xExt[3] <= startX) {
        PRUint32 i;
        for (i = 0; i < 4; i++) {
          extrema[i] = sourceData[targIndex + i];
        }
        for (PRUint32 y1 = startY; y1 <= endY; y1++) {
          for (PRUint32 x1 = startX; x1 <= endX; x1++) {
            for (i = 0; i < 4; i++) {
              PRUint8 pixel = sourceData[y1 * stride + 4 * x1 + i];
              if ((extrema[i] >= pixel &&
                   op == nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE) ||
                  (extrema[i] <= pixel &&
                   op == nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE)) {
                extrema[i] = pixel;
                xExt[i] = x1;
                yExt[i] = y1;
              }
            }
          }
        }
      } else { // We only need to look at the newest column
        for (PRUint32 y1 = startY; y1 <= endY; y1++) {
          for (PRUint32 i = 0; i < 4; i++) {
            PRUint8 pixel = sourceData[y1 * stride + 4 * endX + i];
            if ((extrema[i] >= pixel &&
                 op == nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE) ||
                (extrema[i] <= pixel &&
                 op == nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE)) {
                extrema[i] = pixel;
                xExt[i] = endX;
                yExt[i] = y1;
            }
          }
        }
      }
      targetData[targIndex  ] = extrema[0];
      targetData[targIndex+1] = extrema[1];
      targetData[targIndex+2] = extrema[2];
      targetData[targIndex+3] = extrema[3];
    }
  }
  return NS_OK;
}

//---------------------UnimplementedMOZ------------------------

typedef nsSVGFE nsSVGFEUnimplementedMOZElementBase;

class nsSVGFEUnimplementedMOZElement : public nsSVGFEUnimplementedMOZElementBase,
                                       public nsIDOMSVGFEUnimplementedMOZElement,
                                       public nsISVGFilter
{
protected:
  friend nsresult NS_NewSVGFEUnimplementedMOZElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
  nsSVGFEUnimplementedMOZElement(nsINodeInfo* aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEUnimplementedMOZElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Gaussian
  NS_DECL_NSIDOMSVGFEMERGEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEUnimplementedMOZElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEUnimplementedMOZElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEUnimplementedMOZElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEUnimplementedMOZ)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEUnimplementedMOZElement,nsSVGFEUnimplementedMOZElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEUnimplementedMOZElement,nsSVGFEUnimplementedMOZElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEUnimplementedMOZElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEUnimplementedMOZElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEUnimplementedMOZElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEUnimplementedMOZElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEUnimplementedMOZElement::nsSVGFEUnimplementedMOZElement(nsINodeInfo *aNodeInfo)
  : nsSVGFEUnimplementedMOZElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEUnimplementedMOZElement)

NS_IMETHODIMP
nsSVGFEUnimplementedMOZElement::Filter(nsSVGFilterInstance *instance)
{
  /* should never be called */
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsSVGFEUnimplementedMOZElement::GetRequirements(PRUint32 *aRequirements)
{
  /* should never be called */
  return NS_ERROR_NOT_IMPLEMENTED;
}
