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
#include "nsSVGInteger.h"
#include "nsSVGBoolean.h"
#include "nsIDOMSVGFilters.h"
#include "nsCOMPtr.h"
#include "nsISVGFilter.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsSVGEnum.h"
#include "nsSVGNumberList.h"
#include "nsSVGAnimatedNumberList.h"
#include "nsISVGValueUtils.h"
#include "nsSVGFilters.h"
#include "nsSVGUtils.h"
#include "prdtoa.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "gfxContext.h"
#include "nsSVGLengthList.h"

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
  nsSVGFilterResource(nsSVGFE *aFilter, nsSVGFilterInstance* aInstance);
  ~nsSVGFilterResource();

  /*
   * Acquires a source image for reading
   * aIn:         the name of the filter primitive to use as the source
   * aSourceData: out parameter - the image data of the filter primitive
   *              specified by aIn
   * aSurface:    optional out parameter - the surface of the filter
   *              primitive image specified by aIn
   */
  nsresult AcquireSourceImage(nsIDOMSVGAnimatedString* aIn,
                              PRUint8** aSourceData,
                              gfxImageSurface** aSurface = 0);

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
                              gfxImageSurface** aSurface = 0);

  /*
   * The source region
   */
  const nsRect& GetSourceRegion() const {
    return mSourceRegion;
  }

  /*
   * The area affected by the filter
   */
  const nsRect& GetFilterSubregion() const {
    return mFilterSubregion;
  }

  /*
   * Returns total size of the data buffer in bytes
   */
  PRUint32 GetDataSize() const {
    return mStride * mHeight;
  }

  /*
   * Returns the total number of bytes per row of image data
   */
  PRInt32 GetDataStride() const {
    return mStride;
  }

  PRInt32 GetWidth() const {
    return mWidth;
  }

  PRInt32 GetHeight() const {
    return mHeight;
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
  nsRect mSourceRegion, mFilterSubregion;
  nsRefPtr<gfxImageSurface> mTargetImage;
  nsSVGFE *mFilter;
  nsSVGFilterInstance* mInstance;
  PRUint8 *mSourceData, *mTargetData;
  PRUint32 mWidth, mHeight;
  PRInt32 mStride;
};

nsSVGFilterResource::nsSVGFilterResource(nsSVGFE *aFilter,
                                         nsSVGFilterInstance* aInstance) :
  mTargetImage(nsnull),
  mFilter(aFilter),
  mInstance(aInstance),
  mSourceData(nsnull),
  mTargetData(nsnull),
  mWidth(0),
  mHeight(0),
  mStride(0)
{
}

nsSVGFilterResource::~nsSVGFilterResource()
{
  ReleaseTarget();
}

nsresult
nsSVGFilterResource::AcquireSourceImage(nsIDOMSVGAnimatedString* aIn,
                                        PRUint8** aSourceData,
                                        gfxImageSurface** aSurface)
{
  aIn->GetAnimVal(mInput);

  nsRefPtr<gfxImageSurface> surface;
  mInstance->LookupImage(mInput, getter_AddRefs(surface),
                         &mSourceRegion,
                         mFilter->GetColorModel(aIn));
  if (!surface) {
    return NS_ERROR_FAILURE;
  }

  mSourceData = surface->Data();
  gfxIntSize size = surface->GetSize();
  mWidth = size.width;
  mHeight = size.height;
  mStride = surface->Stride();

  mInstance->GetFilterSubregion(mFilter, 
                                mFilter->SubregionIsUnionOfRegions() ?
                                  mSourceRegion : nsRect(0, 0, mWidth, mHeight),
                                &mFilterSubregion);

  *aSourceData = mSourceData;
  if (aSurface) {
    *aSurface = nsnull;
    surface.swap(*aSurface);
  }
  return NS_OK;
}

nsresult
nsSVGFilterResource::AcquireTargetImage(nsIDOMSVGAnimatedString* aResult,
                                        PRUint8** aTargetData,
                                        gfxImageSurface** aSurface)
{
  aResult->GetAnimVal(mResult);
  mTargetImage = mInstance->GetImage();
  if (!mTargetImage) {
    return NS_ERROR_FAILURE;
  }

  mTargetData = mTargetImage->Data();

  *aTargetData = mTargetData;
  if (aSurface) {
    *aSurface = mTargetImage;
    NS_ADDREF(*aSurface);
  }
  return NS_OK;
}

void
nsSVGFilterResource::ReleaseTarget()
{
  if (!mTargetImage) {
    return;
  }
  mInstance->DefineImage(mResult,
                         mTargetImage,
                         mFilterSubregion,
                         mFilter->GetColorModel(nsnull));

  mTargetImage = nsnull;
}

void
nsSVGFilterResource::CopyImageSubregion(PRUint8 *aDest, const PRUint8 *aSrc)
{
  if (!aDest || !aSrc)
    return;

  for (PRInt32 y = mFilterSubregion.y; y < mFilterSubregion.YMost(); y++) {
    memcpy(aDest + y * mStride + 4 * mFilterSubregion.x,
           aSrc + y * mStride + 4 * mFilterSubregion.x,
           4 * mFilterSubregion.width);
  }
}

//--------------------Filter Element Base Class-----------------------

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
  x = static_cast<float>(PR_strtod(str, &rest));
  if (str == rest) {
    //first value was illformed
    parseError = PR_TRUE;
  } else {
    if (*rest == '\0') {
      //second value was not supplied
      y = x;
    } else {
      y = static_cast<float>(PR_strtod(rest, &rest));
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
  }

  aNum1->SetBaseValue(x, this, PR_FALSE);
  aNum2->SetBaseValue(y, this, PR_FALSE);

  if (parseError)
    return PR_FALSE;

  aResult.SetTo(aValue);
  return PR_TRUE;
}

nsresult
nsSVGFE::SetupScalingFilter(nsSVGFilterInstance *aInstance,
                            nsSVGFilterResource *aResource,
                            nsIDOMSVGAnimatedString *aIn,
                            nsSVGNumber2 *aUnitX, nsSVGNumber2 *aUnitY,
                            ScaleInfo *aScaleInfo)
{
  // We need to do this even in the case where we don't need the
  // target yet because AquireTargetImage sets up some needed data in
  // the filter resource.
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  rv = aResource->AcquireSourceImage(aIn, &sourceData,
                                     getter_AddRefs(aScaleInfo->mRealSource));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aResource->AcquireTargetImage(mResult, &targetData,
                                     getter_AddRefs(aScaleInfo->mRealTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::kernelUnitLength)) {
    aScaleInfo->mRescaling = PR_TRUE;
    float kernelX, kernelY;
    nsSVGLength2 val;
    val.Init(nsSVGUtils::X, 0xff,
             aUnitX->GetAnimValue(),
             nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    kernelX = aInstance->GetPrimitiveLength(&val);
    val.Init(nsSVGUtils::Y, 0xff,
             aUnitY->GetAnimValue(),
             nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
    kernelY = aInstance->GetPrimitiveLength(&val);
#ifdef DEBUG_tor
    fprintf(stderr, "scaling kernelX/Y %f %f\n", kernelX, kernelY);
#endif
    if (kernelX <= 0 || kernelY <= 0)
      return NS_ERROR_FAILURE;

    PRBool overflow = PR_FALSE;
    gfxIntSize scaledSize =
      nsSVGUtils::ConvertToSurfaceSize(gfxSize(aResource->GetWidth() / kernelX,
                                               aResource->GetHeight() / kernelY),
                                       &overflow);
    // If the requested size based on the kernel unit is too big, we
    // need to bail because the effect is pixel size dependent.  Also
    // need to check if we ended up with a negative size (arithmetic
    // overflow) or zero size (large kernel unit)
    if (overflow || scaledSize.width <= 0 || scaledSize.height <= 0)
      return NS_ERROR_FAILURE;

#ifdef DEBUG_tor
    fprintf(stderr, "scaled size %d %d\n", scaledSize.width, scaledSize.height);
#endif
    aScaleInfo->mSource = new gfxImageSurface(scaledSize,
                                              gfxASurface::ImageFormatARGB32);
    aScaleInfo->mTarget = new gfxImageSurface(scaledSize,
                                              gfxASurface::ImageFormatARGB32);
    if (!aScaleInfo->mSource || aScaleInfo->mSource->CairoStatus() ||
        !aScaleInfo->mTarget || aScaleInfo->mTarget->CairoStatus())
      return NS_ERROR_FAILURE;

    gfxContext ctx(aScaleInfo->mSource);
    ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx.Scale(double(scaledSize.width) / aResource->GetWidth(),
              double(scaledSize.height) / aResource->GetHeight());
    ctx.SetSource(aScaleInfo->mRealSource);
    ctx.Paint();

    nsRect rect = aResource->GetFilterSubregion();

    aScaleInfo->mRect.x = rect.x * scaledSize.width / aResource->GetWidth();
    aScaleInfo->mRect.y = rect.y * scaledSize.height / aResource->GetHeight();
    aScaleInfo->mRect.width = rect.width * scaledSize.width / aResource->GetWidth();
    aScaleInfo->mRect.height = rect.height * scaledSize.height / aResource->GetHeight();
  } else {
    aScaleInfo->mRescaling = PR_FALSE;
    aScaleInfo->mRect = aResource->GetFilterSubregion();
    aScaleInfo->mSource = aScaleInfo->mRealSource;
    aScaleInfo->mTarget = aScaleInfo->mRealTarget;
  }

  return NS_OK;
}

void
nsSVGFE::FinishScalingFilter(nsSVGFilterResource *aResource,
                            ScaleInfo *aScaleInfo)
{
  if (!aScaleInfo->mRescaling)
    return;

  gfxIntSize scaledSize = aScaleInfo->mTarget->GetSize();

  gfxContext ctx(aScaleInfo->mRealTarget);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.Scale(double(aResource->GetWidth()) / scaledSize.width,
            double(aResource->GetHeight()) /scaledSize.height);
  ctx.SetSource(aScaleInfo->mTarget);
  ctx.Paint();
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

//---------------------Gaussian Blur------------------------

typedef nsSVGFE nsSVGFEGaussianBlurElementBase;

class nsSVGFEGaussianBlurElement : public nsSVGFEGaussianBlurElementBase,
                                   public nsIDOMSVGFEGaussianBlurElement,
                                   public nsISVGFilter
{
  friend nsresult NS_NewSVGFEGaussianBlurElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEGaussianBlurElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEGaussianBlurElementBase(aNodeInfo) {}
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

  PRUint8 *tmp = static_cast<PRUint8*>
                            (calloc(aFilterResource->GetDataSize(), 1));
  nsRect rect = aFilterResource->GetFilterSubregion();
#ifdef DEBUG_tor
  fprintf(stderr, "FILTER GAUSS rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

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
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

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
  friend nsresult NS_NewSVGFEBlendElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEBlendElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEBlendElementBase(aNodeInfo) {}
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

  virtual EnumAttributesInfo GetEnumInfo();

  enum { MODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sModeMap[];
  static EnumInfo sEnumInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn2;
};

nsSVGEnumMapping nsSVGFEBlendElement::sModeMap[] = {
  {&nsGkAtoms::normal, nsSVGFEBlendElement::SVG_MODE_NORMAL},
  {&nsGkAtoms::multiply, nsSVGFEBlendElement::SVG_MODE_MULTIPLY},
  {&nsGkAtoms::screen, nsSVGFEBlendElement::SVG_MODE_SCREEN},
  {&nsGkAtoms::darken, nsSVGFEBlendElement::SVG_MODE_DARKEN},
  {&nsGkAtoms::lighten, nsSVGFEBlendElement::SVG_MODE_LIGHTEN},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFEBlendElement::sEnumInfo[1] =
{
  { &nsGkAtoms::mode,
    sModeMap,
    nsSVGFEBlendElement::SVG_MODE_NORMAL
  }
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

nsresult
nsSVGFEBlendElement::Init()
{
  nsresult rv = nsSVGFEBlendElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

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
  return mEnumAttributes[MODE].ToDOMAnimatedEnum(aMode, this);
}

NS_IMETHODIMP
nsSVGFEBlendElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect rect = fr.GetFilterSubregion();
  PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER BLEND rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  fr.CopySourceImage();

  rv = fr.AcquireSourceImage(mIn2, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rect = fr.GetFilterSubregion();

  PRUint16 mode = mEnumAttributes[MODE].GetAnimValue();

  for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
    for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
      PRUint32 targIndex = y * stride + 4 * x;
      PRUint32 qa = targetData[targIndex + GFX_ARGB32_OFFSET_A];
      PRUint32 qb = sourceData[targIndex + GFX_ARGB32_OFFSET_A];
      for (PRInt32 i = PR_MIN(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R);
           i <= PR_MAX(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R); i++) {
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
        targetData[targIndex + i] =  static_cast<PRUint8>(val);
      }
      PRUint32 alpha = 255 * 255 - (255 - qa) * (255 - qb);
      FAST_DIVIDE_BY_255(targetData[targIndex + GFX_ARGB32_OFFSET_A], alpha);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEBlendElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1) | CheckStandardNames(mIn2);
  return NS_OK;
}

nsSVGElement::EnumAttributesInfo
nsSVGFEBlendElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

//---------------------Color Matrix------------------------

typedef nsSVGFE nsSVGFEColorMatrixElementBase;

class nsSVGFEColorMatrixElement : public nsSVGFEColorMatrixElementBase,
                                  public nsIDOMSVGFEColorMatrixElement,
                                  public nsISVGFilter
{
  friend nsresult NS_NewSVGFEColorMatrixElement(nsIContent **aResult,
                                                nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEColorMatrixElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEColorMatrixElementBase(aNodeInfo) {}
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
  virtual PRBool OperatesOnPremultipledAlpha() { return PR_FALSE; }

  virtual EnumAttributesInfo GetEnumInfo();

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mValues;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

nsSVGEnumMapping nsSVGFEColorMatrixElement::sTypeMap[] = {
  {&nsGkAtoms::matrix, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX},
  {&nsGkAtoms::saturate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE},
  {&nsGkAtoms::hueRotate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE},
  {&nsGkAtoms::luminanceToAlpha, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFEColorMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX
  }
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

nsresult
nsSVGFEColorMatrixElement::Init()
{
  nsresult rv = nsSVGFEColorMatrixElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

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
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(aType, this);
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

nsSVGElement::EnumAttributesInfo
nsSVGFEColorMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

//----------------------------------------------------------------------
// nsSVGElement methods

NS_IMETHODIMP
nsSVGFEColorMatrixElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();

  nsCOMPtr<nsIDOMSVGNumberList> list;
  mValues->GetAnimVal(getter_AddRefs(list));
  PRUint32 num = 0;
  if (list) {
    list->GetNumberOfItems(&num);
  }

  nsRect rect = fr.GetFilterSubregion();
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
    colorMatrix[12] = 0.072f + 0.928f * s;

    break;

  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE:

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    if (num != 1)
      return NS_ERROR_FAILURE;

    float hueRotateValue;
    list->GetItem(0, getter_AddRefs(number));
    number->GetValue(&hueRotateValue);

    c = static_cast<float>(cos(hueRotateValue * M_PI / 180));
    s = static_cast<float>(sin(hueRotateValue * M_PI / 180));

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
        col[i] =
          sourceData[targIndex + GFX_ARGB32_OFFSET_R] * colorMatrix[row + 0] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_G] * colorMatrix[row + 1] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_B] * colorMatrix[row + 2] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_A] * colorMatrix[row + 3] +
          255 *                                         colorMatrix[row + 4];
        col[i] = PR_MIN(PR_MAX(0, col[i]), 255);
      }
      targetData[targIndex + GFX_ARGB32_OFFSET_R] =
        static_cast<PRUint8>(col[0]);
      targetData[targIndex + GFX_ARGB32_OFFSET_G] =
        static_cast<PRUint8>(col[1]);
      targetData[targIndex + GFX_ARGB32_OFFSET_B] =
        static_cast<PRUint8>(col[2]);
      targetData[targIndex + GFX_ARGB32_OFFSET_A] =
        static_cast<PRUint8>(col[3]);
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
  friend nsresult NS_NewSVGFECompositeElement(nsIContent **aResult,
                                              nsINodeInfo *aNodeInfo);
protected:
  nsSVGFECompositeElement(nsINodeInfo* aNodeInfo)
    : nsSVGFECompositeElementBase(aNodeInfo) {}
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
  virtual EnumAttributesInfo GetEnumInfo();

  enum { K1, K2, K3, K4 };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn2;
};

nsSVGElement::NumberInfo nsSVGFECompositeElement::sNumberInfo[4] =
{
  { &nsGkAtoms::k1, 0 },
  { &nsGkAtoms::k2, 0 },
  { &nsGkAtoms::k3, 0 },
  { &nsGkAtoms::k4, 0 }
};

nsSVGEnumMapping nsSVGFECompositeElement::sOperatorMap[] = {
  {&nsGkAtoms::over, nsSVGFECompositeElement::SVG_OPERATOR_OVER},
  {&nsGkAtoms::in, nsSVGFECompositeElement::SVG_OPERATOR_IN},
  {&nsGkAtoms::out, nsSVGFECompositeElement::SVG_OPERATOR_OUT},
  {&nsGkAtoms::atop, nsSVGFECompositeElement::SVG_OPERATOR_ATOP},
  {&nsGkAtoms::xor_, nsSVGFECompositeElement::SVG_OPERATOR_XOR},
  {&nsGkAtoms::arithmetic, nsSVGFECompositeElement::SVG_OPERATOR_ARITHMETIC},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFECompositeElement::sEnumInfo[1] =
{
  { &nsGkAtoms::_operator,
    sOperatorMap,
    nsIDOMSVGFECompositeElement::SVG_OPERATOR_OVER
  }
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

nsresult
nsSVGFECompositeElement::Init()
{
  nsresult rv = nsSVGFECompositeElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

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
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(aOperator, this);
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
  nsRefPtr<gfxImageSurface> sourceSurface, targetSurface;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn2, &sourceData, getter_AddRefs(sourceSurface));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData,
                             getter_AddRefs(targetSurface));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 op = mEnumAttributes[OPERATOR].GetAnimValue();

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

    nsRect rect = fr.GetFilterSubregion();
    PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER COMPOSITE rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

    // Copy the first source image
    fr.CopySourceImage();

    // Blend in the second source image
    rv = fr.AcquireSourceImage(mIn1, &sourceData);
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
                       static_cast<PRUint8>(PR_MIN(PR_MAX(0, result), 255));
        }
      }
    }
    return NS_OK;
  }

  // Cairo supports the operation we are trying to perform
  gfxContext ctx(targetSurface);

  ctx.SetSource(sourceSurface);
  ctx.Paint();

  if (op < SVG_OPERATOR_OVER || op > SVG_OPERATOR_XOR) {
    return NS_ERROR_FAILURE;
  }
  gfxContext::GraphicsOperator opMap[] = { gfxContext::OPERATOR_DEST,
                                           gfxContext::OPERATOR_OVER,
                                           gfxContext::OPERATOR_IN,
                                           gfxContext::OPERATOR_OUT,
                                           gfxContext::OPERATOR_ATOP,
                                           gfxContext::OPERATOR_XOR };
  ctx.SetOperator(opMap[op]);

  rv = fr.AcquireSourceImage(mIn1, &sourceData,
                             getter_AddRefs(sourceSurface));
  NS_ENSURE_SUCCESS(rv, rv);
  ctx.SetSource(sourceSurface);
  ctx.Paint();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFECompositeElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1) | CheckStandardNames(mIn2);
  return NS_OK;
}

nsSVGElement::EnumAttributesInfo
nsSVGFECompositeElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
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
  friend nsresult NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                                      nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEComponentTransferElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEComponentTransferElementBase(aNodeInfo) {}
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
  virtual PRBool OperatesOnPremultipledAlpha() { return PR_FALSE; }

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
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetFilterSubregion();

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
      targetData[targIndex + GFX_ARGB32_OFFSET_B] =
        tableB[sourceData[targIndex + GFX_ARGB32_OFFSET_B]];
      targetData[targIndex + GFX_ARGB32_OFFSET_G] =
        tableG[sourceData[targIndex + GFX_ARGB32_OFFSET_G]];
      targetData[targIndex + GFX_ARGB32_OFFSET_R] =
        tableR[sourceData[targIndex + GFX_ARGB32_OFFSET_R]];
      targetData[targIndex + GFX_ARGB32_OFFSET_A] =
        tableA[sourceData[targIndex + GFX_ARGB32_OFFSET_A]];
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
  friend nsresult NS_NewSVGComponentTransferFunctionElement(nsIContent **aResult,
                                                            nsINodeInfo *aNodeInfo);
protected:
  nsSVGComponentTransferFunctionElement(nsINodeInfo* aNodeInfo)
    : nsSVGComponentTransferFunctionElementBase(aNodeInfo) {}
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT

protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();

  // nsIDOMSVGComponentTransferFunctionElement properties:
  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mTableValues;

  enum { SLOPE, INTERCEPT, AMPLITUDE, EXPONENT, OFFSET };
  nsSVGNumber2 mNumberAttributes[5];
  static NumberInfo sNumberInfo[5];

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];
};

nsSVGElement::NumberInfo nsSVGComponentTransferFunctionElement::sNumberInfo[5] =
{
  { &nsGkAtoms::slope,     0 },
  { &nsGkAtoms::intercept, 0 },
  { &nsGkAtoms::amplitude, 0 },
  { &nsGkAtoms::exponent,  0 },
  { &nsGkAtoms::offset,    0 }
};

nsSVGEnumMapping nsSVGComponentTransferFunctionElement::sTypeMap[] = {
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

nsSVGElement::EnumInfo nsSVGComponentTransferFunctionElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY
  }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGComponentTransferFunctionElement,nsSVGComponentTransferFunctionElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGComponentTransferFunctionElement,nsSVGComponentTransferFunctionElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGComponentTransferFunctionElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElementBase)

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGComponentTransferFunctionElement::Init()
{
  nsresult rv = nsSVGComponentTransferFunctionElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

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
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(aType, this);
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
  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();

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

nsSVGElement::EnumAttributesInfo
nsSVGComponentTransferFunctionElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::NumberAttributesInfo
nsSVGComponentTransferFunctionElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

class nsSVGFEFuncRElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncRElement
{
  friend nsresult NS_NewSVGFEFuncRElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
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
  friend nsresult NS_NewSVGFEFuncGElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
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
  friend nsresult NS_NewSVGFEFuncBElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
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
  friend nsresult NS_NewSVGFEFuncAElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
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
  friend nsresult NS_NewSVGFEMergeElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEMergeElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEMergeElementBase(aNodeInfo) {}

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
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMergeElement)

NS_IMETHODIMP
nsSVGFEMergeElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsRefPtr<gfxImageSurface> sourceSurface, targetSurface;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireTargetImage(mResult, &targetData,
                             getter_AddRefs(targetSurface));
  NS_ENSURE_SUCCESS(rv, rv);

  gfxContext ctx(targetSurface);

  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child = GetChildAt(i);
    nsCOMPtr<nsIDOMSVGFEMergeNodeElement> node = do_QueryInterface(child);
    if (!node)
      continue;
    nsCOMPtr<nsIDOMSVGAnimatedString> str;
    node->GetIn1(getter_AddRefs(str));

    rv = fr.AcquireSourceImage(str, &sourceData, getter_AddRefs(sourceSurface));
    NS_ENSURE_SUCCESS(rv, rv);

    ctx.SetSource(sourceSurface);
    ctx.Paint();
  }
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
  friend nsresult NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEMergeNodeElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEMergeNodeElementBase(aNodeInfo) {}
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
  friend nsresult NS_NewSVGFEOffsetElement(nsIContent **aResult,
                                           nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEOffsetElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEOffsetElementBase(aNodeInfo) {}
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
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetFilterSubregion();

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

  if (abs(offsetX) >= rect.width || abs(offsetY) >= rect.height)
    return NS_OK;

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
  friend nsresult NS_NewSVGFEFloodElement(nsIContent **aResult,
                                          nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEFloodElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEFloodElementBase(aNodeInfo) {}
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
  virtual PRBool OperatesOnSRGB(nsIDOMSVGAnimatedString*) { return PR_TRUE; }

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
  nsRefPtr<gfxImageSurface> targetSurface;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData,
                             getter_AddRefs(targetSurface));
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetFilterSubregion();

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->GetStyleContext();

  nscolor floodColor = style->GetStyleSVGReset()->mFloodColor;
  float floodOpacity = style->GetStyleSVGReset()->mFloodOpacity;

  gfxContext ctx(targetSurface);
  ctx.SetColor(gfxRGBA(NS_GET_R(floodColor) / 255.0,
                       NS_GET_G(floodColor) / 255.0,
                       NS_GET_B(floodColor) / 255.0,
                       NS_GET_A(floodColor) / 255.0 * floodOpacity));
  ctx.Rectangle(gfxRect(rect.x, rect.y, rect.width, rect.height));
  ctx.Fill();
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

//---------------------Tile------------------------

typedef nsSVGFE nsSVGFETileElementBase;

class nsSVGFETileElement : public nsSVGFETileElementBase,
                           public nsIDOMSVGFETileElement,
                           public nsISVGFilter
{
  friend nsresult NS_NewSVGFETileElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
protected:
  nsSVGFETileElement(nsINodeInfo* aNodeInfo)
    : nsSVGFETileElementBase(aNodeInfo) {}
  nsresult Init();

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFETileElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Tile
  NS_DECL_NSIDOMSVGFETILEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFETileElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFETileElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFETileElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  //virtual NumberAttributesInfo GetNumberInfo();

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FETile)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFETileElement,nsSVGFETileElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFETileElement,nsSVGFETileElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFETileElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFETileElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFETileElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFETileElementBase)

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGFETileElement::Init()
{
  nsresult rv = nsSVGFETileElementBase::Init();
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


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFETileElement)


//----------------------------------------------------------------------
// nsSVGFETileElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFETileElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFETileElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

NS_IMETHODIMP
nsSVGFETileElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRect rect = fr.GetFilterSubregion();
  PRInt32 stride = fr.GetDataStride();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER TILE rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif
  nsRect tile = fr.GetSourceRegion();

  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    PRUint32 tileY = tile.y + (y - tile.y + tile.height) % tile.height;
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint32 tileX = tile.x + (x - tile.x + tile.width) % tile.width;
      *(PRUint32*)(targetData + y * stride + 4 * x) =
        *(PRUint32*)(sourceData + tileY * stride + 4 * tileX);
    }
  }

  return NS_OK;
}


//---------------------Turbulence------------------------

typedef nsSVGFE nsSVGFETurbulenceElementBase;

class nsSVGFETurbulenceElement : public nsSVGFETurbulenceElementBase,
                                 public nsIDOMSVGFETurbulenceElement,
                                 public nsISVGFilter
{
  friend nsresult NS_NewSVGFETurbulenceElement(nsIContent **aResult,
                                               nsINodeInfo *aNodeInfo);
protected:
  nsSVGFETurbulenceElement(nsINodeInfo* aNodeInfo)
    : nsSVGFETurbulenceElementBase(aNodeInfo) {}

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

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
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual EnumAttributesInfo GetEnumInfo();

  enum { BASE_FREQ_X, BASE_FREQ_Y, SEED}; // floating point seed?!
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];

  enum { OCTAVES };
  nsSVGInteger mIntegerAttributes[1];
  static IntegerInfo sIntegerInfo[1];

  enum { STITCHTILES, TYPE };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sStitchTilesMap[];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[2];

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

  void InitSeed(PRInt32 aSeed);
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

nsSVGElement::IntegerInfo nsSVGFETurbulenceElement::sIntegerInfo[1] =
{
  { &nsGkAtoms::numOctaves, 1 }
};

nsSVGEnumMapping nsSVGFETurbulenceElement::sTypeMap[] = {
  {&nsGkAtoms::fractalNoise,
   nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_FRACTALNOISE},
  {&nsGkAtoms::turbulence,
   nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_TURBULENCE},
  {nsnull, 0}
};

nsSVGEnumMapping nsSVGFETurbulenceElement::sStitchTilesMap[] = {
  {&nsGkAtoms::stitch,
   nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_STITCH},
  {&nsGkAtoms::noStitch,
   nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_NOSTITCH},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFETurbulenceElement::sEnumInfo[2] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    nsIDOMSVGFETurbulenceElement::SVG_TURBULENCE_TYPE_TURBULENCE
  },
  { &nsGkAtoms::stitchTiles,
    sStitchTilesMap,
    nsIDOMSVGFETurbulenceElement::SVG_STITCHTYPE_NOSTITCH
  }
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
  return mIntegerAttributes[OCTAVES].ToDOMAnimatedInteger(aNum, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber seed; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetSeed(nsIDOMSVGAnimatedNumber * *aSeed)
{
  return mNumberAttributes[SEED].ToDOMAnimatedNumber(aSeed, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration stitchTiles; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetStitchTiles(nsIDOMSVGAnimatedEnumeration * *aStitch)
{
  return mEnumAttributes[STITCHTILES].ToDOMAnimatedEnum(aStitch, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(aType, this);
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
  nsSVGFilterResource fr(this, instance);

  nsIDOMSVGAnimatedString* sourceGraphic = nsnull;
  rv = NS_NewSVGAnimatedString(&sourceGraphic);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = fr.AcquireSourceImage(sourceGraphic, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetFilterSubregion();

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER TURBULENCE rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  float fX, fY, seed;
  PRInt32 octaves = mIntegerAttributes[OCTAVES].GetAnimValue();
  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();
  PRUint16 stitch = mEnumAttributes[STITCHTILES].GetAnimValue();

  GetAnimatedNumberValues(&fX, &fY, &seed, nsnull);

  InitSeed((PRInt32)seed);

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

      targetData[targIndex + GFX_ARGB32_OFFSET_B] = b;
      targetData[targIndex + GFX_ARGB32_OFFSET_G] = g;
      targetData[targIndex + GFX_ARGB32_OFFSET_R] = r;
      targetData[targIndex + GFX_ARGB32_OFFSET_A] = a;
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
nsSVGFETurbulenceElement::InitSeed(PRInt32 aSeed)
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

nsSVGElement::IntegerAttributesInfo
nsSVGFETurbulenceElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               NS_ARRAY_LENGTH(sIntegerInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFETurbulenceElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

//---------------------Morphology------------------------

typedef nsSVGFE nsSVGFEMorphologyElementBase;

class nsSVGFEMorphologyElement : public nsSVGFEMorphologyElementBase,
                                 public nsIDOMSVGFEMorphologyElement,
                                 public nsISVGFilter
{
  friend nsresult NS_NewSVGFEMorphologyElement(nsIContent **aResult,
                                               nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEMorphologyElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEMorphologyElementBase(aNodeInfo) {}
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
  virtual EnumAttributesInfo GetEnumInfo();

  enum { RADIUS_X, RADIUS_Y };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

nsSVGElement::NumberInfo nsSVGFEMorphologyElement::sNumberInfo[2] =
{
  { &nsGkAtoms::radius, 0 },
  { &nsGkAtoms::radius, 0 }
};

nsSVGEnumMapping nsSVGFEMorphologyElement::sOperatorMap[] = {
  {&nsGkAtoms::erode, nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE},
  {&nsGkAtoms::dilate, nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFEMorphologyElement::sEnumInfo[1] =
{
  { &nsGkAtoms::_operator,
    sOperatorMap,
    nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE
  }
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

nsresult
nsSVGFEMorphologyElement::Init()
{
  nsresult rv = nsSVGFEMorphologyElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

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
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(aOperator, this);
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

nsSVGElement::EnumAttributesInfo
nsSVGFEMorphologyElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::Filter(nsSVGFilterInstance *instance)
{
  nsresult rv;
  PRUint8 *sourceData, *targetData;
  nsSVGFilterResource fr(this, instance);

  rv = fr.AcquireSourceImage(mIn1, &sourceData);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = fr.AcquireTargetImage(mResult, &targetData);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRect rect = fr.GetFilterSubregion();

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
  PRUint16 op = mEnumAttributes[OPERATOR].GetAnimValue();

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

//---------------------Convolve Matrix------------------------

typedef nsSVGFE nsSVGFEConvolveMatrixElementBase;

class nsSVGFEConvolveMatrixElement : public nsSVGFEConvolveMatrixElementBase,
                                     public nsIDOMSVGFEConvolveMatrixElement,
                                     public nsISVGFilter
{
  friend nsresult NS_NewSVGFEConvolveMatrixElement(nsIContent **aResult,
                                                   nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEConvolveMatrixElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEConvolveMatrixElementBase(aNodeInfo) {}
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEConvolveMatrixElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Color Matrix
  NS_DECL_NSIDOMSVGFECONVOLVEMATRIXELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEConvolveMatrixElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEConvolveMatrixElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEConvolveMatrixElementBase::)

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual PRBool OperatesOnPremultipledAlpha() {
    return !mBooleanAttributes[PRESERVEALPHA].GetAnimValue();
  }

  virtual NumberAttributesInfo GetNumberInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual BooleanAttributesInfo GetBooleanInfo();
  virtual EnumAttributesInfo GetEnumInfo();

  enum { DIVISOR, BIAS, KERNEL_UNIT_LENGTH_X, KERNEL_UNIT_LENGTH_Y };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { ORDER_X, ORDER_Y, TARGET_X, TARGET_Y };
  nsSVGInteger mIntegerAttributes[4];
  static IntegerInfo sIntegerInfo[4];

  enum { PRESERVEALPHA };
  nsSVGBoolean mBooleanAttributes[1];
  static BooleanInfo sBooleanInfo[1];

  enum { EDGEMODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sEdgeModeMap[];
  static EnumInfo sEnumInfo[1];

  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mKernelMatrix;

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

nsSVGElement::NumberInfo nsSVGFEConvolveMatrixElement::sNumberInfo[4] =
{
  { &nsGkAtoms::divisor, 1 },
  { &nsGkAtoms::bias, 0 },
  { &nsGkAtoms::kernelUnitLength, 0 },
  { &nsGkAtoms::kernelUnitLength, 0 }
};

nsSVGElement::IntegerInfo nsSVGFEConvolveMatrixElement::sIntegerInfo[4] =
{
  { &nsGkAtoms::order, 0 },
  { &nsGkAtoms::order, 0 },
  { &nsGkAtoms::targetX, 0 },
  { &nsGkAtoms::targetY, 0 }
};

nsSVGElement::BooleanInfo nsSVGFEConvolveMatrixElement::sBooleanInfo[1] =
{
  { &nsGkAtoms::preserveAlpha, PR_FALSE }
};

nsSVGEnumMapping nsSVGFEConvolveMatrixElement::sEdgeModeMap[] = {
  {&nsGkAtoms::duplicate, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE},
  {&nsGkAtoms::wrap, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_WRAP},
  {&nsGkAtoms::none, nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_NONE},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFEConvolveMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::edgeMode,
    sEdgeModeMap,
    nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEConvolveMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEConvolveMatrixElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEConvolveMatrixElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEConvolveMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEConvolveMatrixElementBase)

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGFEConvolveMatrixElement::Init()
{
  nsresult rv = nsSVGFEConvolveMatrixElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: kernelMarix, #IMPLIED attrib: kernelMatrix
  {
    nsCOMPtr<nsIDOMSVGNumberList> values;
    rv = NS_NewSVGNumberList(getter_AddRefs(values));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedNumberList(getter_AddRefs(mKernelMatrix), values);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::kernelMatrix, mKernelMatrix);
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

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEConvolveMatrixElement)

//----------------------------------------------------------------------
// nsSVGFEConvolveMatrixElement methods

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderX(nsIDOMSVGAnimatedInteger * *aOrderX)
{
  return mIntegerAttributes[ORDER_X].ToDOMAnimatedInteger(aOrderX, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderY(nsIDOMSVGAnimatedInteger * *aOrderY)
{
  return mIntegerAttributes[ORDER_Y].ToDOMAnimatedInteger(aOrderY, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetKernelMatrix(nsIDOMSVGAnimatedNumberList * *aKernelMatrix)
{
  *aKernelMatrix = mKernelMatrix;
  NS_IF_ADDREF(*aKernelMatrix);
  return NS_OK;
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetTargetX(nsIDOMSVGAnimatedInteger * *aTargetX)
{
  return mIntegerAttributes[TARGET_X].ToDOMAnimatedInteger(aTargetX, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetTargetY(nsIDOMSVGAnimatedInteger * *aTargetY)
{
  return mIntegerAttributes[TARGET_Y].ToDOMAnimatedInteger(aTargetY, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetEdgeMode(nsIDOMSVGAnimatedEnumeration * *aEdgeMode)
{
  return mEnumAttributes[EDGEMODE].ToDOMAnimatedEnum(aEdgeMode, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetPreserveAlpha(nsIDOMSVGAnimatedBoolean * *aPreserveAlpha)
{
  return mBooleanAttributes[PRESERVEALPHA].ToDOMAnimatedBoolean(aPreserveAlpha, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetDivisor(nsIDOMSVGAnimatedNumber **aDivisor)
{
  return mNumberAttributes[DIVISOR].ToDOMAnimatedNumber(aDivisor, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetBias(nsIDOMSVGAnimatedNumber **aBias)
{
  return mNumberAttributes[BIAS].ToDOMAnimatedNumber(aBias, this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_X].ToDOMAnimatedNumber(aKernelX,
                                                                     this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_Y].ToDOMAnimatedNumber(aKernelY,
                                                                     this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

PRBool
nsSVGFEConvolveMatrixElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                             const nsAString& aValue,
                                             nsAttrValue& aResult)
{
  if (aName == nsGkAtoms::order && aNameSpaceID == kNameSpaceID_None) {
    PRInt32 x = 0, y = 0;
    char *rest;
    PRBool parseError = PR_FALSE;

    NS_ConvertUTF16toUTF8 value(aValue);
    value.CompressWhitespace(PR_FALSE, PR_TRUE);
    const char *str = value.get();
    x = strtol(str, &rest, 10);
    if (str == rest) {
      //first value was illformed
      parseError = PR_TRUE;
    } else {
      if (*rest == '\0') {
        //second value was not supplied
        y = x;
      } else {
        y = strtol(rest, &rest, 10);
        if (*rest != '\0') {
          //second value was illformed or there was trailing content
          parseError = PR_TRUE;
        }
      }
    }

    if (parseError) {
      ReportAttributeParseFailure(GetOwnerDoc(), aName, aValue);
      x = sIntegerInfo[ORDER_X].mDefaultValue;
      y = sIntegerInfo[ORDER_Y].mDefaultValue;
    }

    mIntegerAttributes[ORDER_X].SetBaseValue(x, this, PR_FALSE);
    mIntegerAttributes[ORDER_Y].SetBaseValue(y, this, PR_FALSE);

    if (parseError)
      return PR_FALSE;

    aResult.SetTo(aValue);
    return PR_TRUE;
  }

  if (aName == nsGkAtoms::kernelUnitLength && aNameSpaceID == kNameSpaceID_None) {
    return ScanDualValueAttribute(aValue, nsGkAtoms::kernelUnitLength,
                                  &mNumberAttributes[KERNEL_UNIT_LENGTH_X],
                                  &mNumberAttributes[KERNEL_UNIT_LENGTH_Y],
                                  &sNumberInfo[KERNEL_UNIT_LENGTH_X],
                                  &sNumberInfo[KERNEL_UNIT_LENGTH_Y],
                                  aResult);
  }

  return nsSVGFEConvolveMatrixElementBase::ParseAttribute(aNameSpaceID, aName,
                                                          aValue, aResult);
}

#define BOUND(val, min, max) \
  PR_MIN(PR_MAX(val, min), max)

#define WRAP(val, min, max) \
  (val < 0 ? (val + val % max) : val % max)

static void
ConvolvePixel(const PRUint8 *aSourceData,
              PRUint8 *aTargetData,
              PRInt32 aWidth, PRInt32 aHeight,
              PRInt32 aStride,
              PRInt32 aX, PRInt32 aY,
              PRUint16 aEdgeMode,
              const float *aKernel,
              float aDivisor, float aBias,
              PRBool aPreserveAlpha,
              PRInt32 aOrderX, PRInt32 aOrderY,
              PRInt32 aTargetX, PRInt32 aTargetY)
{
  float sum[4] = {0, 0, 0, 0};
  aBias *= 255;
  PRInt32 offsets[4] = {GFX_ARGB32_OFFSET_R,
                        GFX_ARGB32_OFFSET_G,
                        GFX_ARGB32_OFFSET_B,
                        GFX_ARGB32_OFFSET_A } ;
  PRInt32 channels = aPreserveAlpha ? 3 : 4;

  for (PRInt32 y = 0; y < aOrderY; y++) {
    PRInt32 sampleY = aY + y - aTargetY;
    PRBool overscanY = sampleY < 0 || sampleY >= aHeight;
    for (PRInt32 x = 0; x < aOrderX; x++) {
      PRInt32 sampleX = aX + x - aTargetX;
      PRBool overscanX = sampleX < 0 || sampleX >= aWidth;
      for (PRInt32 i = 0; i < channels; i++) {
        if (overscanY || overscanX) {
          switch (aEdgeMode) {
            case nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_DUPLICATE:
              sum[i] +=
                aSourceData[BOUND(sampleY, 0, aHeight - 1) * aStride +
                            BOUND(sampleX, 0, aWidth - 1) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            case nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_WRAP:
              sum[i] +=
                aSourceData[WRAP(sampleY, 0, aHeight) * aStride +
                            WRAP(sampleX, 0, aWidth) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            default:
              break;
          }
        } else {
          sum[i] +=
            aSourceData[sampleY * aStride + 4 * sampleX + offsets[i]] *
            aKernel[aOrderX * y + x];
        }
      }
    }
  }
  for (PRInt32 i = 0; i < channels; i++) {
    aTargetData[aY * aStride + 4 * aX + offsets[i]] =
      BOUND(static_cast<PRInt32>(sum[i] / aDivisor + aBias * 255), 0, 255);
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A] =
      aSourceData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A];
  }
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::Filter(nsSVGFilterInstance *instance)
{
  nsCOMPtr<nsIDOMSVGNumberList> list;
  mKernelMatrix->GetAnimVal(getter_AddRefs(list));
  PRUint32 num = 0;
  if (list) {
    list->GetNumberOfItems(&num);
  }

  PRInt32 orderX, orderY;
  PRInt32 targetX, targetY;
  GetAnimatedIntegerValues(&orderX, &orderY, &targetX, &targetY, nsnull);

  if (orderX <= 0 || orderY <= 0 ||
      static_cast<PRUint32>(orderX * orderY) != num) {
    return NS_ERROR_FAILURE;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::targetX)) {
    if (targetX < 0 || targetX >= orderX)
      return NS_ERROR_FAILURE;
  } else {
    targetX = orderX / 2;
  }
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::targetY)) {
    if (targetY < 0 || targetY >= orderY)
      return NS_ERROR_FAILURE;
  } else {
    targetY = orderY / 2;
  }

  if (orderX > NS_SVG_OFFSCREEN_MAX_DIMENSION ||
      orderY > NS_SVG_OFFSCREEN_MAX_DIMENSION)
    return NS_ERROR_FAILURE;
  nsAutoPtr<float> kernel(new float[orderX * orderY]);
  if (!kernel)
    return NS_ERROR_FAILURE;
  for (PRUint32 i = 0; i < num; i++) {
    nsCOMPtr<nsIDOMSVGNumber> number;
    list->GetItem(i, getter_AddRefs(number));
    // svg specification flips the kernel from what one might expect
    number->GetValue(&kernel[num - 1 - i]);
  }

  float divisor;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::divisor)) {
    divisor = mNumberAttributes[DIVISOR].GetAnimValue();
    if (divisor == 0)
      return NS_ERROR_FAILURE;
  } else {
    divisor = kernel[0];
    for (PRUint32 i = 1; i < num; i++)
      divisor += kernel[i];
    if (divisor == 0)
      divisor = 1;
  }

  nsSVGFilterResource fr(this, instance);

  ScaleInfo info;
  nsresult rv = SetupScalingFilter(instance, &fr, mIn1,
                                   &mNumberAttributes[KERNEL_UNIT_LENGTH_X],
                                   &mNumberAttributes[KERNEL_UNIT_LENGTH_Y],
                                   &info);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_tor
  {
    nsRect rect = fr.GetFilterSubregion();

    fprintf(stderr, "FILTER CONVOLVE MATRIX rect: %d,%d  %dx%d\n",
            rect.x, rect.y, rect.width, rect.height);
  }
#endif

  PRUint16 edgeMode = mEnumAttributes[EDGEMODE].GetAnimValue();
  PRBool preserveAlpha = mBooleanAttributes[PRESERVEALPHA].GetAnimValue();

  float bias = 0;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::bias)) {
    bias = mNumberAttributes[BIAS].GetAnimValue();
  }

  nsRect rect = info.mRect;

  PRInt32 stride = info.mSource->Stride();
  PRInt32 width = info.mSource->GetSize().width;
  PRInt32 height = info.mSource->GetSize().height;

  PRUint8 *sourceData = info.mSource->Data();
  PRUint8 *targetData = info.mTarget->Data();

  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      ConvolvePixel(sourceData, targetData,
                    width, height, stride,
                    x, y,
                    edgeMode, kernel, divisor, bias, preserveAlpha,
                    orderX, orderY, targetX, targetY);
    }
  }

  FinishScalingFilter(&fr, &info);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEConvolveMatrixElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::IntegerAttributesInfo
nsSVGFEConvolveMatrixElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               NS_ARRAY_LENGTH(sIntegerInfo));
}

nsSVGElement::BooleanAttributesInfo
nsSVGFEConvolveMatrixElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(mBooleanAttributes, sBooleanInfo,
                               NS_ARRAY_LENGTH(sBooleanInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEConvolveMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

//---------------------DistantLight------------------------

typedef nsSVGElement nsSVGFEDistantLightElementBase;

class nsSVGFEDistantLightElement : public nsSVGFEDistantLightElementBase,
                                   public nsIDOMSVGFEDistantLightElement
{
  friend nsresult NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEDistantLightElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEDistantLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFEDISTANTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDistantLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFEDistantLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEDistantLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { AZIMUTH, ELEVATION };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDistantLight)

nsSVGElement::NumberInfo nsSVGFEDistantLightElement::sNumberInfo[2] =
{
  { &nsGkAtoms::azimuth,   0 },
  { &nsGkAtoms::elevation, 0 }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDistantLightElement,nsSVGFEDistantLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDistantLightElement,nsSVGFEDistantLightElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEDistantLightElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEDistantLightElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEDistantLightElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDistantLightElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDistantLightElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEDistantLightElement methods

NS_IMETHODIMP
nsSVGFEDistantLightElement::GetAzimuth(nsIDOMSVGAnimatedNumber **aAzimuth)
{
  return mNumberAttributes[AZIMUTH].ToDOMAnimatedNumber(aAzimuth,
                                                        this);
}

NS_IMETHODIMP
nsSVGFEDistantLightElement::GetElevation(nsIDOMSVGAnimatedNumber **aElevation)
{
  return mNumberAttributes[ELEVATION].ToDOMAnimatedNumber(aElevation,
                                                          this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEDistantLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------PointLight------------------------

typedef nsSVGElement nsSVGFEPointLightElementBase;

class nsSVGFEPointLightElement : public nsSVGFEPointLightElementBase,
                                 public nsIDOMSVGFEPointLightElement
{
  friend nsresult NS_NewSVGFEPointLightElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEPointLightElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEPointLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFEPOINTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEPointLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFEPointLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEPointLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { X, Y, Z };
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEPointLight)

nsSVGElement::NumberInfo nsSVGFEPointLightElement::sNumberInfo[3] =
{
  { &nsGkAtoms::x, 0 },
  { &nsGkAtoms::y, 0 },
  { &nsGkAtoms::z, 0 }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEPointLightElement,nsSVGFEPointLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEPointLightElement,nsSVGFEPointLightElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEPointLightElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEPointLightElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEPointLightElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEPointLightElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEPointLightElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEPointLightElement methods

NS_IMETHODIMP
nsSVGFEPointLightElement::GetX(nsIDOMSVGAnimatedNumber **aX)
{
  return mNumberAttributes[X].ToDOMAnimatedNumber(aX, this);
}

NS_IMETHODIMP
nsSVGFEPointLightElement::GetY(nsIDOMSVGAnimatedNumber **aY)
{
  return mNumberAttributes[Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFEPointLightElement::GetZ(nsIDOMSVGAnimatedNumber **aZ)
{
  return mNumberAttributes[Z].ToDOMAnimatedNumber(aZ, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEPointLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//---------------------SpotLight------------------------

typedef nsSVGElement nsSVGFESpotLightElementBase;

class nsSVGFESpotLightElement : public nsSVGFESpotLightElementBase,
                                public nsIDOMSVGFESpotLightElement
{
  friend nsresult NS_NewSVGFESpotLightElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
protected:
  nsSVGFESpotLightElement(nsINodeInfo* aNodeInfo)
    : nsSVGFESpotLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFESPOTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFESpotLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFESpotLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFESpotLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual NumberAttributesInfo GetNumberInfo();

  enum { X, Y, Z, POINTS_AT_X, POINTS_AT_Y, POINTS_AT_Z,
         SPECULAR_EXPONENT, LIMITING_CONE_ANGLE };
  nsSVGNumber2 mNumberAttributes[8];
  static NumberInfo sNumberInfo[8];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpotLight)

nsSVGElement::NumberInfo nsSVGFESpotLightElement::sNumberInfo[8] =
{
  { &nsGkAtoms::x, 0 },
  { &nsGkAtoms::y, 0 },
  { &nsGkAtoms::z, 0 },
  { &nsGkAtoms::pointsAtX, 0 },
  { &nsGkAtoms::pointsAtY, 0 },
  { &nsGkAtoms::pointsAtZ, 0 },
  { &nsGkAtoms::specularExponent, 0 },
  { &nsGkAtoms::limitingConeAngle, 0 }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFESpotLightElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFESpotLightElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFESpotLightElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFESpotLightElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFESpotLightElement)

//----------------------------------------------------------------------
// nsIDOMSVGFESpotLightElement methods

NS_IMETHODIMP
nsSVGFESpotLightElement::GetX(nsIDOMSVGAnimatedNumber **aX)
{
  return mNumberAttributes[X].ToDOMAnimatedNumber(aX, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetY(nsIDOMSVGAnimatedNumber **aY)
{
  return mNumberAttributes[Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetZ(nsIDOMSVGAnimatedNumber **aZ)
{
  return mNumberAttributes[Z].ToDOMAnimatedNumber(aZ, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtX(nsIDOMSVGAnimatedNumber **aX)
{
  return mNumberAttributes[POINTS_AT_X].ToDOMAnimatedNumber(aX, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtY(nsIDOMSVGAnimatedNumber **aY)
{
  return mNumberAttributes[POINTS_AT_Y].ToDOMAnimatedNumber(aY, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetPointsAtZ(nsIDOMSVGAnimatedNumber **aZ)
{
  return mNumberAttributes[POINTS_AT_Z].ToDOMAnimatedNumber(aZ, this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetSpecularExponent(nsIDOMSVGAnimatedNumber **aExponent)
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(aExponent,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpotLightElement::GetLimitingConeAngle(nsIDOMSVGAnimatedNumber **aAngle)
{
  return mNumberAttributes[LIMITING_CONE_ANGLE].ToDOMAnimatedNumber(aAngle,
                                                                    this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFESpotLightElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

//------------------------------------------------------------

typedef nsSVGFE nsSVGFELightingElementBase;

class nsSVGFELightingElement : public nsSVGFELightingElementBase,
                               public nsISVGFilter
{
protected:
  nsSVGFELightingElement(nsINodeInfo* aNodeInfo)
    : nsSVGFELightingElementBase(aNodeInfo) {}
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFELightingElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFELightingElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFELightingElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFELightingElementBase::)

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual PRBool ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
protected:
  virtual void
  LightPixel(const float *N, const float *L,
             nscolor color, PRUint8 *targetData) = 0;

  virtual NumberAttributesInfo GetNumberInfo();

  enum { SURFACE_SCALE, DIFFUSE_CONSTANT, SPECULAR_CONSTANT, SPECULAR_EXPONENT,
         KERNEL_UNIT_LENGTH_X, KERNEL_UNIT_LENGTH_Y };
  nsSVGNumber2 mNumberAttributes[6];
  static NumberInfo sNumberInfo[6];

  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
};

nsSVGElement::NumberInfo nsSVGFELightingElement::sNumberInfo[6] =
{
  { &nsGkAtoms::surfaceScale, 1 },
  { &nsGkAtoms::diffuseConstant, 1 },
  { &nsGkAtoms::specularConstant, 1 },
  { &nsGkAtoms::specularExponent, 1 },
  { &nsGkAtoms::kernelUnitLength, 0 },
  { &nsGkAtoms::kernelUnitLength, 0 }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFELightingElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFELightingElementBase)

//----------------------------------------------------------------------
// Implementation

nsresult
nsSVGFELightingElement::Init()
{
  nsresult rv = nsSVGFELightingElementBase::Init();
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

NS_IMETHODIMP_(PRBool)
nsSVGFELightingElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sLightingEffectsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFELightingElementBase::IsAttributeMapped(name);
}

PRBool
nsSVGFELightingElement::ParseAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                       const nsAString& aValue,
                                       nsAttrValue& aResult)
{
  if (aName == nsGkAtoms::kernelUnitLength && aNameSpaceID == kNameSpaceID_None) {
    return ScanDualValueAttribute(aValue, nsGkAtoms::radius,
                                  &mNumberAttributes[KERNEL_UNIT_LENGTH_X],
                                  &mNumberAttributes[KERNEL_UNIT_LENGTH_Y],
                                  &sNumberInfo[KERNEL_UNIT_LENGTH_X],
                                  &sNumberInfo[KERNEL_UNIT_LENGTH_Y],
                                  aResult);
  }
  return nsSVGFELightingElementBase::ParseAttribute(aNameSpaceID, aName,
                                                      aValue, aResult);
}

NS_IMETHODIMP
nsSVGFELightingElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

nsSVGElement::NumberAttributesInfo
nsSVGFELightingElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

#define DOT(a,b) (a[0] * b[0] + a[1] * b[1] + a[2] * b[2])
#define NORMALIZE(vec) \
  PR_BEGIN_MACRO \
    float norm = sqrt(DOT(vec, vec)); \
    vec[0] /= norm; \
    vec[1] /= norm; \
    vec[2] /= norm; \
  PR_END_MACRO

static PRInt32
Convolve3x3(const PRUint8 *index, PRInt32 stride,
            const PRInt8 kernel[3][3])
{
  PRInt32 sum = 0;
  for (PRInt32 y = 0; y < 3; y++) {
    for (PRInt32 x = 0; x < 3; x++) {
      PRInt8 k = kernel[y][x];
      if (k)
        sum += k * index[4 * (x - 1) + stride * (y - 1)];
    }
  }
  return sum;
}

static void
GenerateNormal(float *N, const PRUint8 *data, PRInt32 stride, nsRect rect,
               PRInt32 x, PRInt32 y, float surfaceScale)
{
  // See this for source of constants:
  //   http://www.w3.org/TR/SVG11/filters.html#feDiffuseLighting
  static const PRInt8 Kx[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2,  2}, { 0, -1,  1} },
        { {  0,  0,  0}, {-2,  0,  2}, {-1,  0,  1} },
        { {  0,  0,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0, -1,  1} },
        { { -1,  0,  1}, {-2,  0,  2}, {-1,  0,  1} },
        { { -1,  1,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0,  0,  0} },
        { { -1,  0,  1}, {-2,  0,  2}, { 0,  0,  0} },
        { { -1,  1,  0}, {-2,  2,  0}, { 0,  0,  0} } } };
  static const PRInt8 Ky[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2, -1}, { 0,  2,  1} },
        { {  0,  0,  0}, {-1, -2, -1}, { 1,  2,  1} },
        { {  0,  0,  0}, {-1, -2,  1}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  0,  0}, { 0,  2,  1} },
        { { -1, -2, -1}, { 0,  0,  0}, { 1,  2,  1} },
        { { -1, -2,  0}, { 0,  0,  0}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  2,  1}, { 0,  0,  0} },
        { { -1, -2, -1}, { 1,  2,  1}, { 0,  0,  0} },
        { { -1, -2,  0}, { 1,  2,  0}, { 0,  0,  0} } } };
  static const float FACTORx[3][3] =
    { { 2.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0 },
      { 1.0 / 2.0, 1.0 / 4.0, 1.0 / 2.0 },
      { 2.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0 } };
  static const float FACTORy[3][3] =
    { { 2.0 / 3.0, 1.0 / 2.0, 2.0 / 3.0 },
      { 1.0 / 3.0, 1.0 / 4.0, 1.0 / 3.0 },
      { 2.0 / 3.0, 1.0 / 2.0, 2.0 / 3.0 } };

  PRInt8 xflag, yflag;
  if (x == 0) {
    xflag = 0;
  } else if (x == rect.width - 1) {
    xflag = 2;
  } else {
    xflag = 1;
  }
  if (y == 0) {
    yflag = 0;
  } else if (y == rect.height - 1) {
    yflag = 2;
  } else {
    yflag = 1;
  }

  const PRUint8 *index = data + y * stride + 4 * x + GFX_ARGB32_OFFSET_A;

  N[0] = -surfaceScale * FACTORx[yflag][xflag] *
    Convolve3x3(index, stride, Kx[yflag][xflag]);
  N[1] = -surfaceScale * FACTORy[yflag][xflag] *
    Convolve3x3(index, stride, Ky[yflag][xflag]);
  N[2] = 255;
  NORMALIZE(N);
}

NS_IMETHODIMP
nsSVGFELightingElement::Filter(nsSVGFilterInstance *instance)
{
  nsSVGFilterResource fr(this, instance);

  ScaleInfo info;
  nsresult rv = SetupScalingFilter(instance, &fr, mIn1,
                                   &mNumberAttributes[KERNEL_UNIT_LENGTH_X],
                                   &mNumberAttributes[KERNEL_UNIT_LENGTH_Y],
                                   &info);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_tor
  {
    nsRect rect = fr.GetFilterSubregion();
    fprintf(stderr, "FILTER LIGHTING rect: %d,%d  %dx%d\n",
            rect.x, rect.y, rect.width, rect.height);
  }
#endif

  nsCOMPtr<nsIDOMSVGFEDistantLightElement> distantLight;
  nsCOMPtr<nsIDOMSVGFEPointLightElement> pointLight;
  nsCOMPtr<nsIDOMSVGFESpotLightElement> spotLight;

  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->GetStyleContext();

  nscolor lightColor = style->GetStyleSVGReset()->mLightingColor;

  // find specified light
  PRUint32 count = GetChildCount();
  for (PRUint32 k = 0; k < count; k++) {
    nsCOMPtr<nsIContent> child = GetChildAt(k);
    distantLight = do_QueryInterface(child);
    pointLight = do_QueryInterface(child);
    spotLight = do_QueryInterface(child);
    if (distantLight || pointLight || spotLight)
      break;
  }

  if (!distantLight && !pointLight && !spotLight)
    return NS_ERROR_FAILURE;

  const float radPerDeg = M_PI/180.0;

  float L[3];
  if (distantLight) {
    float azimuth, elevation;
    static_cast<nsSVGFEDistantLightElement*>
      (distantLight.get())->GetAnimatedNumberValues(&azimuth,
                                                    &elevation,
                                                    nsnull);
    L[0] = cos(azimuth * radPerDeg) * cos(elevation * radPerDeg);
    L[1] = sin(azimuth * radPerDeg) * cos(elevation * radPerDeg);
    L[2] = sin(elevation * radPerDeg);
  }
  float lightPos[3], pointsAt[3], specularExponent, cosConeAngle;
  if (pointLight) {
    static_cast<nsSVGFEPointLightElement*>
      (pointLight.get())->GetAnimatedNumberValues(lightPos,
                                                  lightPos + 1,
                                                  lightPos + 2,
                                                  nsnull);
  }
  if (spotLight) {
    float limitingConeAngle;
    static_cast<nsSVGFESpotLightElement*>
      (spotLight.get())->GetAnimatedNumberValues(lightPos,
                                                 lightPos + 1,
                                                 lightPos + 2,
                                                 pointsAt,
                                                 pointsAt + 1,
                                                 pointsAt + 2,
                                                 &specularExponent,
                                                 &limitingConeAngle,
                                                 nsnull);
    nsCOMPtr<nsIContent> spot = do_QueryInterface(spotLight);
    if (spot->HasAttr(kNameSpaceID_None, nsGkAtoms::limitingConeAngle)) {
      cosConeAngle = PR_MAX(cos(limitingConeAngle * radPerDeg), 0);
    } else {
      cosConeAngle = 0;
    }
  }

  float surfaceScale = mNumberAttributes[SURFACE_SCALE].GetAnimValue();

  nsRect rect = info.mRect;
  PRInt32 stride = info.mSource->Stride();
  PRUint8 *sourceData = info.mSource->Data();
  PRUint8 *targetData = info.mTarget->Data();

  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRInt32 index = y * stride + x * 4;

      float N[3];
      GenerateNormal(N, sourceData, stride, rect, x, y, surfaceScale);

      if (pointLight || spotLight) {
        float Z =
          surfaceScale * sourceData[index + GFX_ARGB32_OFFSET_A] / 255;

        L[0] = lightPos[0] - x;
        L[1] = lightPos[1] - y;
        L[2] = lightPos[2] - Z;
        NORMALIZE(L);
      }

      nscolor color;

      if (spotLight) {
        float S[3];
        S[0] = pointsAt[0] - lightPos[0];
        S[1] = pointsAt[1] - lightPos[1];
        S[2] = pointsAt[2] - lightPos[2];
        NORMALIZE(S);
        float dot = -DOT(L, S);
        if (dot < cosConeAngle) {
          color = NS_RGB(0, 0, 0);
        } else {
          float tmp = pow(dot, specularExponent);
          color = NS_RGB(PRUint8(NS_GET_R(lightColor) * tmp),
                         PRUint8(NS_GET_G(lightColor) * tmp),
                         PRUint8(NS_GET_B(lightColor) * tmp));
        }
      } else {
        color = lightColor;
      }

      LightPixel(N, L, color, targetData + index);
    }
  }

  FinishScalingFilter(&fr, &info);

  return NS_OK;
}


//---------------------DiffuseLighting------------------------

typedef nsSVGFELightingElement nsSVGFEDiffuseLightingElementBase;

class nsSVGFEDiffuseLightingElement : public nsSVGFEDiffuseLightingElementBase,
                                      public nsIDOMSVGFEDiffuseLightingElement
{
  friend nsresult NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                                    nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEDiffuseLightingElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEDiffuseLightingElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // DiffuseLighting
  NS_DECL_NSIDOMSVGFEDIFFUSELIGHTINGELEMENT

  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFEDiffuseLightingElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEDiffuseLightingElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, PRUint8 *targetData);

};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDiffuseLighting)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFEDiffuseLightingElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFEDiffuseLightingElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFEDiffuseLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDiffuseLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDiffuseLightingElement)

//----------------------------------------------------------------------
// nsSVGFEDiffuseLightingElement methods

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetSurfaceScale(nsIDOMSVGAnimatedNumber **aScale)
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(aScale,
                                                              this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetDiffuseConstant(nsIDOMSVGAnimatedNumber **aConstant)
{
  return mNumberAttributes[DIFFUSE_CONSTANT].ToDOMAnimatedNumber(aConstant,
                                                              this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_X].ToDOMAnimatedNumber(aKernelX,
                                                                     this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_Y].ToDOMAnimatedNumber(aKernelY,
                                                                     this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

void
nsSVGFEDiffuseLightingElement::LightPixel(const float *N, const float *L,
                                          nscolor color, PRUint8 *targetData)
{
  float diffuseNL =
    mNumberAttributes[DIFFUSE_CONSTANT].GetAnimValue() * DOT(N, L);

  if (diffuseNL > 0) {
    targetData[GFX_ARGB32_OFFSET_B] =
      PR_MIN(PRUint32(diffuseNL * NS_GET_B(color)), 255);
    targetData[GFX_ARGB32_OFFSET_G] =
      PR_MIN(PRUint32(diffuseNL * NS_GET_G(color)), 255);
    targetData[GFX_ARGB32_OFFSET_R] =
      PR_MIN(PRUint32(diffuseNL * NS_GET_R(color)), 255);
  } else {
    targetData[GFX_ARGB32_OFFSET_B] = 0;
    targetData[GFX_ARGB32_OFFSET_G] = 0;
    targetData[GFX_ARGB32_OFFSET_R] = 0;
  }

  targetData[GFX_ARGB32_OFFSET_A] = 255;
}

//---------------------SpecularLighting------------------------

typedef nsSVGFELightingElement nsSVGFESpecularLightingElementBase;

class nsSVGFESpecularLightingElement : public nsSVGFESpecularLightingElementBase,
                                       public nsIDOMSVGFESpecularLightingElement
{
  friend nsresult NS_NewSVGFESpecularLightingElement(nsIContent **aResult,
                                               nsINodeInfo *aNodeInfo);
protected:
  nsSVGFESpecularLightingElement(nsINodeInfo* aNodeInfo)
    : nsSVGFESpecularLightingElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // DiffuseLighting
  NS_DECL_NSIDOMSVGFESPECULARLIGHTINGELEMENT

  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFESpecularLightingElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFESpecularLightingElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFESpecularLightingElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFESpecularLightingElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);

protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, PRUint8 *targetData);

};

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpecularLighting)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFESpecularLightingElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterPrimitiveStandardAttributes)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFESpecularLightingElement)
  NS_INTERFACE_MAP_ENTRY(nsISVGFilter)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFESpecularLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFESpecularLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFESpecularLightingElement)

//----------------------------------------------------------------------
// nsSVGFESpecularLightingElement methods

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  *aIn = mIn1;
  NS_IF_ADDREF(*aIn);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSurfaceScale(nsIDOMSVGAnimatedNumber **aScale)
{
  return mNumberAttributes[SURFACE_SCALE].ToDOMAnimatedNumber(aScale,
                                                              this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSpecularConstant(nsIDOMSVGAnimatedNumber **aConstant)
{
  return mNumberAttributes[SPECULAR_CONSTANT].ToDOMAnimatedNumber(aConstant,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetSpecularExponent(nsIDOMSVGAnimatedNumber **aExponent)
{
  return mNumberAttributes[SPECULAR_EXPONENT].ToDOMAnimatedNumber(aExponent,
                                                                  this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetKernelUnitLengthX(nsIDOMSVGAnimatedNumber **aKernelX)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_X].ToDOMAnimatedNumber(aKernelX,
                                                                     this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberAttributes[KERNEL_UNIT_LENGTH_Y].ToDOMAnimatedNumber(aKernelY,
                                                                     this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

NS_IMETHODIMP
nsSVGFESpecularLightingElement::Filter(nsSVGFilterInstance *instance)
{
  float specularExponent = mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue();

  // specification defined range (15.22)
  if (specularExponent < 1 || specularExponent > 128)
    return NS_ERROR_FAILURE;

  return nsSVGFESpecularLightingElementBase::Filter(instance);
}


void
nsSVGFESpecularLightingElement::LightPixel(const float *N, const float *L,
                                           nscolor color, PRUint8 *targetData)
{
  float H[3];
  H[0] = L[0];
  H[1] = L[1];
  H[2] = L[2] + 1;
  NORMALIZE(H);

  float kS = mNumberAttributes[SPECULAR_CONSTANT].GetAnimValue();
  float dotNH = DOT(N, H);

  if (dotNH > 0 && kS > 0) {
    float specularNH =
      kS * pow(dotNH, mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue());

    targetData[GFX_ARGB32_OFFSET_B] =
      PR_MIN(PRUint32(specularNH * NS_GET_B(color)), 255);
    targetData[GFX_ARGB32_OFFSET_G] =
      PR_MIN(PRUint32(specularNH * NS_GET_G(color)), 255);
    targetData[GFX_ARGB32_OFFSET_R] =
      PR_MIN(PRUint32(specularNH * NS_GET_R(color)), 255);

    targetData[GFX_ARGB32_OFFSET_A] =
      PR_MAX(targetData[GFX_ARGB32_OFFSET_B],
             PR_MAX(targetData[GFX_ARGB32_OFFSET_G],
                    targetData[GFX_ARGB32_OFFSET_R]));
  } else {
    targetData[GFX_ARGB32_OFFSET_B] = 0;
    targetData[GFX_ARGB32_OFFSET_G] = 0;
    targetData[GFX_ARGB32_OFFSET_R] = 0;
    targetData[GFX_ARGB32_OFFSET_A] = 255;
  }
}

//---------------------UnimplementedMOZ------------------------

typedef nsSVGFE nsSVGFEUnimplementedMOZElementBase;

class nsSVGFEUnimplementedMOZElement : public nsSVGFEUnimplementedMOZElementBase,
                                       public nsIDOMSVGFEUnimplementedMOZElement,
                                       public nsISVGFilter
{
  friend nsresult NS_NewSVGFEUnimplementedMOZElement(nsIContent **aResult,
                                                     nsINodeInfo *aNodeInfo);
protected:
  nsSVGFEUnimplementedMOZElement(nsINodeInfo* aNodeInfo)
    : nsSVGFEUnimplementedMOZElementBase(aNodeInfo) {}

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
