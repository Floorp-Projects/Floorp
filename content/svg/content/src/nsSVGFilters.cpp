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
#include "nsGkAtoms.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGBoolean.h"
#include "nsIDOMSVGFilters.h"
#include "nsCOMPtr.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsIDOMSVGFilterElement.h"
#include "nsSVGEnum.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsISVGValueUtils.h"
#include "nsSVGFilters.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsStyleContext.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "gfxContext.h"
#include "gfxMatrix.h"
#include "nsIDOMSVGURIReference.h"
#include "nsImageLoadingContent.h"
#include "imgIContainer.h"
#include "nsNetUtil.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsSVGFilterElement.h"
#include "nsSVGString.h"
#include "nsSVGEffects.h"
#include "gfxUtils.h"

#if defined(XP_WIN) 
// Prevent Windows redefining LoadImage
#undef LoadImage
#endif

#define NUM_ENTRIES_IN_4x5_MATRIX 20

using namespace mozilla;
using namespace mozilla::dom;

static void
CopyDataRect(PRUint8 *aDest, const PRUint8 *aSrc, PRUint32 aStride,
             const nsIntRect& aDataRect)
{
  for (PRInt32 y = aDataRect.y; y < aDataRect.YMost(); y++) {
    memcpy(aDest + y * aStride + 4 * aDataRect.x,
           aSrc + y * aStride + 4 * aDataRect.x,
           4 * aDataRect.width);
  }
}

static void
CopyRect(const nsSVGFE::Image* aDest, const nsSVGFE::Image* aSrc, const nsIntRect& aDataRect)
{
  NS_ASSERTION(aDest->mImage->Stride() == aSrc->mImage->Stride(), "stride mismatch");
  NS_ASSERTION(aDest->mImage->GetSize() == aSrc->mImage->GetSize(), "size mismatch");
  NS_ASSERTION(nsIntRect(0, 0, aDest->mImage->Width(), aDest->mImage->Height()).Contains(aDataRect),
               "aDataRect out of bounds");

  CopyDataRect(aDest->mImage->Data(), aSrc->mImage->Data(),
               aSrc->mImage->Stride(), aDataRect);
}

//--------------------Filter Element Base Class-----------------------

nsSVGElement::LengthInfo nsSVGFE::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFE,nsSVGFEBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFE,nsSVGFEBase)

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGFE, NS_SVG_FE_CID)

NS_INTERFACE_MAP_BEGIN(nsSVGFE)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(nsSVGFE)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFE::ScaleInfo
nsSVGFE::SetupScalingFilter(nsSVGFilterInstance *aInstance,
                            const Image *aSource, const Image *aTarget,
                            const nsIntRect& aDataRect,
                            nsSVGNumberPair *aKernelUnitLength)
{
  ScaleInfo result;
  result.mRescaling = aKernelUnitLength->IsExplicitlySet();
  if (!result.mRescaling) {
    result.mSource = aSource->mImage;
    result.mTarget = aTarget->mImage;
    result.mDataRect = aDataRect;
    return result;
  }

  float kernelX = aInstance->GetPrimitiveNumber(nsSVGUtils::X,
                                                aKernelUnitLength,
                                                nsSVGNumberPair::eFirst);
  float kernelY = aInstance->GetPrimitiveNumber(nsSVGUtils::Y,
                                                aKernelUnitLength,
                                                nsSVGNumberPair::eSecond);
  if (kernelX <= 0 || kernelY <= 0)
    return result;

  PRBool overflow = PR_FALSE;
  gfxIntSize scaledSize =
    nsSVGUtils::ConvertToSurfaceSize(gfxSize(aTarget->mImage->Width() / kernelX,
                                             aTarget->mImage->Height() / kernelY),
                                     &overflow);
  // If the requested size based on the kernel unit is too big, we
  // need to bail because the effect is pixel size dependent.  Also
  // need to check if we ended up with a negative size (arithmetic
  // overflow) or zero size (large kernel unit)
  if (overflow || scaledSize.width <= 0 || scaledSize.height <= 0)
    return result;

  gfxRect r(aDataRect.x, aDataRect.y, aDataRect.width, aDataRect.height);
  r.Scale(gfxFloat(scaledSize.width)/aTarget->mImage->Width(),
          gfxFloat(scaledSize.height)/aTarget->mImage->Height());
  r.RoundOut();
  if (!gfxUtils::GfxRectToIntRect(r, &result.mDataRect))
    return result;
  
  result.mSource = new gfxImageSurface(scaledSize,
                                       gfxASurface::ImageFormatARGB32);
  result.mTarget = new gfxImageSurface(scaledSize,
                                       gfxASurface::ImageFormatARGB32);
  if (!result.mSource || result.mSource->CairoStatus() ||
      !result.mTarget || result.mTarget->CairoStatus()) {
    result.mSource = nsnull;
    result.mTarget = nsnull;
    return result;
  }
  result.mRealTarget = aTarget->mImage;

  gfxContext ctx(result.mSource);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.Scale(double(scaledSize.width) / aTarget->mImage->Width(),
            double(scaledSize.height) / aTarget->mImage->Height());
  ctx.SetSource(aSource->mImage);
  ctx.Paint();

  // mTarget was already cleared when it was created

  return result;
}

void
nsSVGFE::FinishScalingFilter(ScaleInfo *aScaleInfo)
{
  if (!aScaleInfo->mRescaling)
    return;

  gfxIntSize scaledSize = aScaleInfo->mTarget->GetSize();

  gfxContext ctx(aScaleInfo->mRealTarget);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.Scale(double(aScaleInfo->mRealTarget->Width()) / scaledSize.width,
            double(aScaleInfo->mRealTarget->Height()) / scaledSize.height);
  ctx.SetSource(aScaleInfo->mTarget);
  ctx.Paint();
}

nsIntRect
nsSVGFE::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
                           const nsSVGFilterInstance& aInstance)
{
  nsIntRect r;
  for (PRUint32 i = 0; i < aSourceBBoxes.Length(); ++i) {
    r.UnionRect(r, aSourceBBoxes[i]);
  }
  return r;
}

void
nsSVGFE::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
                                   nsTArray<nsIntRect>& aSourceBBoxes,
                                   const nsSVGFilterInstance& aInstance)
{
  for (PRUint32 i = 0; i < aSourceBBoxes.Length(); ++i) {
    aSourceBBoxes[i] = aTargetBBox;
  }
}

nsIntRect
nsSVGFE::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                           const nsSVGFilterInstance& aInstance)
{
  nsIntRect r;
  for (PRUint32 i = 0; i < aSourceChangeBoxes.Length(); ++i) {
    r.UnionRect(r, aSourceChangeBoxes[i]);
  }
  return r;
}

void
nsSVGFE::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
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
  return GetResultImageName().ToDOMAnimatedString(aResult, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGFE::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFiltersMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFEBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGFE::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

inline static void DidAnimateAttr(Element *aFilterPrimitive)
{
  // nsSVGLeafFrame doesn't implement AttributeChanged.
  nsIFrame* frame = aFilterPrimitive->GetPrimaryFrame();
  if (frame) {
    nsSVGEffects::InvalidateRenderingObservers(frame);
  }
}

inline static void DidAnimateAttrViaParent(Element *aFilterPrimitive)
{
  // No frame, use parent's
  NS_ASSERTION(!aFilterPrimitive->GetPrimaryFrame(), "Not expecting a frame");
  nsIContent *parent = aFilterPrimitive->GetFlattenedTreeParent();
  if (parent && parent->IsElement()) {
    DidAnimateAttr(parent->AsElement());
  }
}

void
nsSVGFE::DidAnimateLength(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateNumber(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateNumberPair(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateNumberList(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateInteger(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateIntegerPair(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateEnum(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimatePreserveAspectRatio()
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateBoolean(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

void
nsSVGFE::DidAnimateString(PRUint8 aAttrEnum)
{
  DidAnimateAttr(this);
}

//---------------------Gaussian Blur------------------------

typedef nsSVGFE nsSVGFEGaussianBlurElementBase;

class nsSVGFEGaussianBlurElement : public nsSVGFEGaussianBlurElementBase,
                                   public nsIDOMSVGFEGaussianBlurElement
{
  friend nsresult NS_NewSVGFEGaussianBlurElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEGaussianBlurElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEGaussianBlurElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEGaussianBlurElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo >& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Gaussian
  NS_DECL_NSIDOMSVGFEGAUSSIANBLURELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEGaussianBlurElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEGaussianBlurElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEGaussianBlurElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { STD_DEV };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

private:
  nsresult GetDXY(PRUint32 *aDX, PRUint32 *aDY, const nsSVGFilterInstance& aInstance);
  nsIntRect InflateRectForBlur(const nsIntRect& aRect, const nsSVGFilterInstance& aInstance);

  void GaussianBlur(const Image *aSource, const Image *aTarget,
                    const nsIntRect& aDataRect,
                    PRUint32 aDX, PRUint32 aDY);
};

nsSVGElement::NumberPairInfo nsSVGFEGaussianBlurElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::stdDeviation, 0, 0 }
};

nsSVGElement::StringInfo nsSVGFEGaussianBlurElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEGaussianBlur)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEGaussianBlurElement,nsSVGFEGaussianBlurElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEGaussianBlurElement,nsSVGFEGaussianBlurElementBase)

DOMCI_NODE_DATA(SVGFEGaussianBlurElement, nsSVGFEGaussianBlurElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEGaussianBlurElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEGaussianBlurElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEGaussianBlurElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEGaussianBlurElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEGaussianBlurElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEGaussianBlurElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEGaussianBlurElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber stdDeviationX; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetStdDeviationX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(aX, nsSVGNumberPair::eFirst, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber stdDeviationY; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetStdDeviationY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(aY, nsSVGNumberPair::eSecond, this);
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  NS_ENSURE_FINITE2(stdDeviationX, stdDeviationY, NS_ERROR_ILLEGAL_VALUE);
  mNumberPairAttributes[STD_DEV].SetBaseValues(stdDeviationX, stdDeviationY, this, PR_TRUE);
  return NS_OK;
}

/**
 * We want to speed up 1/N integer divisions --- integer division is
 * often rather slow.
 * We know that our input numerators V are constrained to be <= 255*N,
 * so the result of dividing by N always fits in 8 bits.
 * So we can try approximating the division V/N as V*K/(2^24) (integer
 * division, 32-bit multiply). Dividing by 2^24 is a simple shift so it's
 * fast. The main problem is choosing a value for K; this function returns
 * K's value.
 * 
 * If the result is correct for the extrema, V=0 and V=255*N, then we'll
 * be in good shape since both the original function and our approximation
 * are linear. V=0 always gives 0 in both cases, no problem there.
 * For V=255*N, let's choose the largest K that doesn't cause overflow
 * and ensure that it gives the right answer. The constraints are
 *     (1)   255*N*K < 2^32
 * and (2)   255*N*K >= 255*(2^24)
 * 
 * From (1) we find the best value of K is floor((2^32 - 1)/(255*N)).
 * (2) tells us when this will be valid:
 *    N*floor((2^32 - 1)/(255*N)) >= 2^24
 * Now, floor(X) > X - 1, so (2) holds if
 *    N*((2^32 - 1)/(255*N) - 1) >= 2^24
 *         (2^32 - 1)/255 - 2^24 >= N
 *                             N <= 65793
 * 
 * If all that math confuses you, this should convince you:
 * > perl -e 'for($N=1;(255*$N*int(0xFFFFFFFF/(255*$N)))>>24==255;++$N){}print"$N\n"'
 * 66052
 * 
 * So this is fine for all reasonable values of N. For larger values of N
 * we may as well just use the same approximation and accept the fact that
 * the output channel values will be a little low.
 */
static PRUint32 ComputeScaledDivisor(PRUint32 aDivisor)
{
  return PR_UINT32_MAX/(255*aDivisor);
}
  
static void
BoxBlur(const PRUint8 *aInput, PRUint8 *aOutput,
        PRInt32 aStrideMinor, PRInt32 aStartMinor, PRInt32 aEndMinor,
        PRInt32 aLeftLobe, PRInt32 aRightLobe, PRBool aAlphaOnly)
{
  PRInt32 boxSize = aLeftLobe + aRightLobe + 1;
  PRInt32 scaledDivisor = ComputeScaledDivisor(boxSize);
  PRInt32 sums[4] = {0, 0, 0, 0};

  for (PRInt32 i=0; i < boxSize; i++) {
    PRInt32 pos = aStartMinor - aLeftLobe + i;
    pos = NS_MAX(pos, aStartMinor);
    pos = NS_MIN(pos, aEndMinor - 1);
#define SUM(j)     sums[j] += aInput[aStrideMinor*pos + j];
    SUM(0); SUM(1); SUM(2); SUM(3);
#undef SUM
  }

  aOutput += aStrideMinor*aStartMinor;
  if (aStartMinor + PRInt32(boxSize) <= aEndMinor) {
    const PRUint8 *lastInput = aInput + aStartMinor*aStrideMinor;
    const PRUint8 *nextInput = aInput + (aStartMinor + aRightLobe + 1)*aStrideMinor;
#define OUTPUT(j)     aOutput[j] = (sums[j]*scaledDivisor) >> 24;
#define SUM(j)        sums[j] += nextInput[j] - lastInput[j];
    // process pixels in B, G, R, A order because that's 0, 1, 2, 3 for x86
#define OUTPUT_PIXEL() \
        if (!aAlphaOnly) { OUTPUT(GFX_ARGB32_OFFSET_B); \
                           OUTPUT(GFX_ARGB32_OFFSET_G); \
                           OUTPUT(GFX_ARGB32_OFFSET_R); } \
        OUTPUT(GFX_ARGB32_OFFSET_A);
#define SUM_PIXEL() \
        if (!aAlphaOnly) { SUM(GFX_ARGB32_OFFSET_B); \
                           SUM(GFX_ARGB32_OFFSET_G); \
                           SUM(GFX_ARGB32_OFFSET_R); } \
        SUM(GFX_ARGB32_OFFSET_A);
    for (PRInt32 minor = aStartMinor;
         minor < aStartMinor + aLeftLobe;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    for (PRInt32 minor = aStartMinor + aLeftLobe;
         minor < aEndMinor - aRightLobe - 1;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    // nextInput is now aInput + aEndMinor*aStrideMinor. Set it back to
    // aInput + (aEndMinor - 1)*aStrideMinor so we read the last pixel in every
    // iteration of the next loop.
    nextInput -= aStrideMinor;
    for (PRInt32 minor = aEndMinor - aRightLobe - 1; minor < aEndMinor; minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      aOutput += aStrideMinor;
#undef SUM_PIXEL
#undef SUM
    }
  } else {
    for (PRInt32 minor = aStartMinor; minor < aEndMinor; minor++) {
      PRInt32 tmp = minor - aLeftLobe;
      PRInt32 last = NS_MAX(tmp, aStartMinor);
      PRInt32 next = NS_MIN(tmp + PRInt32(boxSize), aEndMinor - 1);

      OUTPUT_PIXEL();
#define SUM(j)     sums[j] += aInput[aStrideMinor*next + j] - \
                              aInput[aStrideMinor*last + j];
      if (!aAlphaOnly) { SUM(GFX_ARGB32_OFFSET_B);
                         SUM(GFX_ARGB32_OFFSET_G);
                         SUM(GFX_ARGB32_OFFSET_R); }
      SUM(GFX_ARGB32_OFFSET_A);
      aOutput += aStrideMinor;
#undef SUM
#undef OUTPUT_PIXEL
#undef OUTPUT
    }
  }
}

static PRUint32
GetBlurBoxSize(double aStdDev)
{
  NS_ASSERTION(aStdDev >= 0, "Negative standard deviations not allowed");

  double size = aStdDev*3*sqrt(2*M_PI)/4;
  // Doing super-large blurs accurately isn't very important.
  PRUint32 max = 1024;
  if (size > max)
    return max;
  return PRUint32(floor(size + 0.5));
}

nsresult
nsSVGFEGaussianBlurElement::GetDXY(PRUint32 *aDX, PRUint32 *aDY,
                                   const nsSVGFilterInstance& aInstance)
{
  float stdX = aInstance.GetPrimitiveNumber(nsSVGUtils::X,
                                            &mNumberPairAttributes[STD_DEV],
                                            nsSVGNumberPair::eFirst);
  float stdY = aInstance.GetPrimitiveNumber(nsSVGUtils::Y,
                                            &mNumberPairAttributes[STD_DEV],
                                            nsSVGNumberPair::eSecond);
  if (stdX < 0 || stdY < 0)
    return NS_ERROR_FAILURE;

  // If the box size is greater than twice the temporary surface size
  // in an axis, then each pixel will be set to the average of all the
  // other pixel values.
  *aDX = GetBlurBoxSize(stdX);
  *aDY = GetBlurBoxSize(stdY);
  return NS_OK;
}

static PRBool
AreAllColorChannelsZero(const nsSVGFE::Image* aTarget)
{
  return aTarget->mConstantColorChannels &&
         aTarget->mImage->GetDataSize() >= 4 &&
         (*reinterpret_cast<PRUint32*>(aTarget->mImage->Data()) & 0x00FFFFFF) == 0;
}

void
nsSVGFEGaussianBlurElement::GaussianBlur(const Image *aSource,
                                         const Image *aTarget,                                         
                                         const nsIntRect& aDataRect,
                                         PRUint32 aDX, PRUint32 aDY)
{
  NS_ASSERTION(nsIntRect(0, 0, aTarget->mImage->Width(), aTarget->mImage->Height()).Contains(aDataRect),
               "aDataRect out of bounds");

  nsAutoArrayPtr<PRUint8> tmp(new PRUint8[aTarget->mImage->GetDataSize()]);
  if (!tmp)
    return;
  memset(tmp, 0, aTarget->mImage->GetDataSize());

  PRBool alphaOnly = AreAllColorChannelsZero(aTarget);
  
  const PRUint8* sourceData = aSource->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  if (aDX == 0) {
    CopyDataRect(tmp, sourceData, stride, aDataRect);
  } else {
    PRInt32 longLobe = aDX/2;
    PRInt32 shortLobe = (aDX & 1) ? longLobe : longLobe - 1;
    for (PRInt32 major = aDataRect.y; major < aDataRect.YMost(); ++major) {
      PRInt32 ms = major*stride;
      BoxBlur(sourceData + ms, tmp + ms, 4, aDataRect.x, aDataRect.XMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, 4, aDataRect.x, aDataRect.XMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, 4, aDataRect.x, aDataRect.XMost(), longLobe, longLobe, alphaOnly);
    }
  }

  if (aDY == 0) {
    CopyDataRect(targetData, tmp, stride, aDataRect);
  } else {
    PRInt32 longLobe = aDY/2;
    PRInt32 shortLobe = (aDY & 1) ? longLobe : longLobe - 1;
    for (PRInt32 major = aDataRect.x; major < aDataRect.XMost(); ++major) {
      PRInt32 ms = major*4;
      BoxBlur(tmp + ms, targetData + ms, stride, aDataRect.y, aDataRect.YMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, stride, aDataRect.y, aDataRect.YMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, stride, aDataRect.y, aDataRect.YMost(), longLobe, longLobe, alphaOnly);
    }
  }
}

static void
InflateRectForBlurDXY(nsIntRect* aRect, PRUint32 aDX, PRUint32 aDY)
{
  aRect->Inflate(3*(aDX/2), 3*(aDY/2));
}

static void
ClearRect(gfxImageSurface* aSurface, PRInt32 aX, PRInt32 aY,
          PRInt32 aXMost, PRInt32 aYMost)
{
  NS_ASSERTION(aX <= aXMost && aY <= aYMost, "Invalid rectangle");
  NS_ASSERTION(aX >= 0 && aY >= 0 && aXMost <= aSurface->Width() && aYMost <= aSurface->Height(),
               "Rectangle out of bounds");

  if (aX == aXMost || aY == aYMost)
    return;
  for (PRInt32 y = aY; y < aYMost; ++y) {
    memset(aSurface->Data() + aSurface->Stride()*y + aX*4, 0, (aXMost - aX)*4);
  }
}

// Clip aTarget's image to its filter primitive subregion.
// aModifiedRect contains all the pixels which might not be RGBA(0,0,0,0),
// it's relative to the surface data.
static void
ClipTarget(nsSVGFilterInstance* aInstance, const nsSVGFE::Image* aTarget,
           const nsIntRect& aModifiedRect)
{
  nsIntPoint surfaceTopLeft = aInstance->GetSurfaceRect().TopLeft();

  NS_ASSERTION(aInstance->GetSurfaceRect().Contains(aModifiedRect + surfaceTopLeft),
               "Modified data area overflows the surface?");

  nsIntRect clip = aModifiedRect;
  nsSVGUtils::ClipToGfxRect(&clip,
    aTarget->mFilterPrimitiveSubregion - gfxPoint(surfaceTopLeft.x, surfaceTopLeft.y));

  ClearRect(aTarget->mImage, aModifiedRect.x, aModifiedRect.y, aModifiedRect.XMost(), clip.y);
  ClearRect(aTarget->mImage, aModifiedRect.x, clip.y, clip.x, clip.YMost());
  ClearRect(aTarget->mImage, clip.XMost(), clip.y, aModifiedRect.XMost(), clip.YMost());
  ClearRect(aTarget->mImage, aModifiedRect.x, clip.YMost(), aModifiedRect.XMost(), aModifiedRect.YMost());
}

static void
ClipComputationRectToSurface(nsSVGFilterInstance* aInstance,
                             nsIntRect* aDataRect)
{
  aDataRect->IntersectRect(*aDataRect,
          nsIntRect(nsIntPoint(0, 0), aInstance->GetSurfaceRect().Size()));
}

nsresult
nsSVGFEGaussianBlurElement::Filter(nsSVGFilterInstance* aInstance,
                                   const nsTArray<const Image*>& aSources,
                                   const Image* aTarget,
                                   const nsIntRect& rect)
{
  PRUint32 dx, dy;
  nsresult rv = GetDXY(&dx, &dy, *aInstance);
  if (NS_FAILED(rv))
    return rv;

  nsIntRect computationRect = rect;
  InflateRectForBlurDXY(&computationRect, dx, dy);
  ClipComputationRectToSurface(aInstance, &computationRect);
  GaussianBlur(aSources[0], aTarget, computationRect, dx, dy);
  ClipTarget(aInstance, aTarget, computationRect);
  return NS_OK;
}

void
nsSVGFEGaussianBlurElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEGaussianBlurElement::InflateRectForBlur(const nsIntRect& aRect,
                                               const nsSVGFilterInstance& aInstance)
{
  PRUint32 dX, dY;
  nsresult rv = GetDXY(&dX, &dY, aInstance);
  nsIntRect result = aRect;
  if (NS_SUCCEEDED(rv)) {
    InflateRectForBlurDXY(&result, dX, dY);
  }
  return result;
}

nsIntRect
nsSVGFEGaussianBlurElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return InflateRectForBlur(aSourceBBoxes[0], aInstance);
}

void
nsSVGFEGaussianBlurElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = InflateRectForBlur(aTargetBBox, aInstance);
}

nsIntRect
nsSVGFEGaussianBlurElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                              const nsSVGFilterInstance& aInstance)
{
  return InflateRectForBlur(aSourceChangeBoxes[0], aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
nsSVGFEGaussianBlurElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  NS_ARRAY_LENGTH(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEGaussianBlurElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Blend------------------------

typedef nsSVGFE nsSVGFEBlendElementBase;

class nsSVGFEBlendElement : public nsSVGFEBlendElementBase,
                            public nsIDOMSVGFEBlendElement
{
  friend nsresult NS_NewSVGFEBlendElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEBlendElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEBlendElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEBlendElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  // Blend
  NS_DECL_NSIDOMSVGFEBLENDELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEBlendElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEBlendElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEBlendElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:

  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { MODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
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

nsSVGElement::StringInfo nsSVGFEBlendElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in2, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEBlend)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEBlendElement,nsSVGFEBlendElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEBlendElement,nsSVGFEBlendElementBase)

DOMCI_NODE_DATA(SVGFEBlendElement, nsSVGFEBlendElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEBlendElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEBlendElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEBlendElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEBlendElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBlendElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEBlendElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEBlendElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEBlendElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFEBlendElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN2].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration mode; */
NS_IMETHODIMP nsSVGFEBlendElement::GetMode(nsIDOMSVGAnimatedEnumeration * *aMode)
{
  return mEnumAttributes[MODE].ToDOMAnimatedEnum(aMode, this);
}

nsresult
nsSVGFEBlendElement::Filter(nsSVGFilterInstance* aInstance,
                            const nsTArray<const Image*>& aSources,
                            const Image* aTarget,
                            const nsIntRect& rect)
{
  CopyRect(aTarget, aSources[0], rect);

  PRUint8* sourceData = aSources[1]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  PRUint16 mode = mEnumAttributes[MODE].GetAnimValue();

  for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
    for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
      PRUint32 targIndex = y * stride + 4 * x;
      PRUint32 qa = targetData[targIndex + GFX_ARGB32_OFFSET_A];
      PRUint32 qb = sourceData[targIndex + GFX_ARGB32_OFFSET_A];
      for (PRInt32 i = NS_MIN(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R);
           i <= NS_MAX(GFX_ARGB32_OFFSET_B, GFX_ARGB32_OFFSET_R); i++) {
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
            val = NS_MIN((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          case nsSVGFEBlendElement::SVG_MODE_LIGHTEN:
            val = NS_MAX((255 - qa) * cb + 255 * ca,
                         (255 - qb) * ca + 255 * cb);
            break;
          default:
            return NS_ERROR_FAILURE;
            break;
        }
        val = NS_MIN(val / 255, 255U);
        targetData[targIndex + i] =  static_cast<PRUint8>(val);
      }
      PRUint32 alpha = 255 * 255 - (255 - qa) * (255 - qb);
      FAST_DIVIDE_BY_255(targetData[targIndex + GFX_ARGB32_OFFSET_A], alpha);
    }
  }
  return NS_OK;
}

void
nsSVGFEBlendElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
nsSVGFEBlendElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEBlendElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Color Matrix------------------------

typedef nsSVGFE nsSVGFEColorMatrixElementBase;

class nsSVGFEColorMatrixElement : public nsSVGFEColorMatrixElementBase,
                                  public nsIDOMSVGFEColorMatrixElement
{
  friend nsresult NS_NewSVGFEColorMatrixElement(nsIContent **aResult,
                                                already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEColorMatrixElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEColorMatrixElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEColorMatrixElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  // Color Matrix
  NS_DECL_NSIDOMSVGFECOLORMATRIXELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEColorMatrixElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEColorMatrixElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEColorMatrixElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual PRBool OperatesOnPremultipledAlpha(PRInt32) { return PR_FALSE; }

  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  enum { VALUES };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
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

nsSVGElement::StringInfo nsSVGFEColorMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

nsSVGElement::NumberListInfo nsSVGFEColorMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::values }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEColorMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)

DOMCI_NODE_DATA(SVGFEColorMatrixElement, nsSVGFEColorMatrixElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEColorMatrixElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEColorMatrixElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEColorMatrixElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEColorMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEColorMatrixElementBase)


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEColorMatrixElement)


//----------------------------------------------------------------------
// nsSVGFEColorMatrixElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(aType, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumberList values; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetValues(nsIDOMSVGAnimatedNumberList * *aValues)
{
  *aValues = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[VALUES],
                                                     this, VALUES).get();
  return NS_OK;
}

void
nsSVGFEColorMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsresult
nsSVGFEColorMatrixElement::Filter(nsSVGFilterInstance *instance,
                                  const nsTArray<const Image*>& aSources,
                                  const Image* aTarget,
                                  const nsIntRect& rect)
{
  PRUint8* sourceData = aSources[0]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();
  const SVGNumberList &values = mNumberListAttributes[VALUES].GetAnimValue();

  if (!mNumberListAttributes[VALUES].IsExplicitlySet() &&
      (type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE)) {
    // identity matrix filter
    CopyRect(aTarget, aSources[0], rect);
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

  float colorMatrix[NUM_ENTRIES_IN_4x5_MATRIX];
  float s, c;

  switch (type) {
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX:

    if (values.Length() != NUM_ENTRIES_IN_4x5_MATRIX)
      return NS_ERROR_FAILURE;

    for(PRUint32 j = 0; j < values.Length(); j++) {
      colorMatrix[j] = values[j];
    }
    break;
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE:

    if (values.Length() != 1)
      return NS_ERROR_FAILURE;

    s = values[0];

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
  {
    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    if (values.Length() != 1)
      return NS_ERROR_FAILURE;

    float hueRotateValue = values[0];

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
  }

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
        col[i] = NS_MIN(NS_MAX(0.f, col[i]), 255.f);
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

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
nsSVGFEColorMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEColorMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
nsSVGFEColorMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  NS_ARRAY_LENGTH(sNumberListInfo));
}

//---------------------Composite------------------------

typedef nsSVGFE nsSVGFECompositeElementBase;

class nsSVGFECompositeElement : public nsSVGFECompositeElementBase,
                                public nsIDOMSVGFECompositeElement
{
  friend nsresult NS_NewSVGFECompositeElement(nsIContent **aResult,
                                              already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFECompositeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFECompositeElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFECompositeElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  // Composite
  NS_DECL_NSIDOMSVGFECOMPOSITEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFECompositeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFECompositeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFECompositeElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { K1, K2, K3, K4 };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

nsSVGElement::NumberInfo nsSVGFECompositeElement::sNumberInfo[4] =
{
  { &nsGkAtoms::k1, 0, PR_FALSE },
  { &nsGkAtoms::k2, 0, PR_FALSE },
  { &nsGkAtoms::k3, 0, PR_FALSE },
  { &nsGkAtoms::k4, 0, PR_FALSE }
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

nsSVGElement::StringInfo nsSVGFECompositeElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in2, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEComposite)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFECompositeElement,nsSVGFECompositeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFECompositeElement,nsSVGFECompositeElementBase)

DOMCI_NODE_DATA(SVGFECompositeElement, nsSVGFECompositeElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFECompositeElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFECompositeElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFECompositeElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFECompositeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFECompositeElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFECompositeElement)

//----------------------------------------------------------------------
// nsSVGFECompositeElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFECompositeElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFECompositeElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN2].ToDOMAnimatedString(aIn, this);
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
  NS_ENSURE_FINITE4(k1, k2, k3, k4, NS_ERROR_ILLEGAL_VALUE);
  mNumberAttributes[K1].SetBaseValue(k1, this, PR_TRUE);
  mNumberAttributes[K2].SetBaseValue(k2, this, PR_TRUE);
  mNumberAttributes[K3].SetBaseValue(k3, this, PR_TRUE);
  mNumberAttributes[K4].SetBaseValue(k4, this, PR_TRUE);
  return NS_OK;
}

nsresult
nsSVGFECompositeElement::Filter(nsSVGFilterInstance *instance,
                                const nsTArray<const Image*>& aSources,
                                const Image* aTarget,
                                const nsIntRect& rect)
{
  PRUint16 op = mEnumAttributes[OPERATOR].GetAnimValue();

  // Cairo does not support arithmetic operator
  if (op == nsSVGFECompositeElement::SVG_OPERATOR_ARITHMETIC) {
    float k1, k2, k3, k4;
    GetAnimatedNumberValues(&k1, &k2, &k3, &k4, nsnull);

    // Copy the first source image
    CopyRect(aTarget, aSources[0], rect);

    PRUint8* sourceData = aSources[1]->mImage->Data();
    PRUint8* targetData = aTarget->mImage->Data();
    PRUint32 stride = aTarget->mImage->Stride();

    // Blend in the second source image
    float k1Scaled = k1 / 255.0f;
    float k4Scaled = k4*255.0f;
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
        PRUint32 targIndex = y * stride + 4 * x;
        for (PRInt32 i = 0; i < 4; i++) {
          PRUint8 i1 = targetData[targIndex + i];
          PRUint8 i2 = sourceData[targIndex + i];
          float result = k1Scaled*i1*i2 + k2*i1 + k3*i2 + k4Scaled;
          targetData[targIndex + i] =
                       static_cast<PRUint8>(NS_MIN(NS_MAX(0.f, result), 255.f));
        }
      }
    }
    return NS_OK;
  }

  // Cairo supports the operation we are trying to perform

  gfxContext ctx(aTarget->mImage);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetSource(aSources[1]->mImage);
  // Ensure rendering is limited to the filter primitive subregion
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);
  ctx.Paint();

  if (op < SVG_OPERATOR_OVER || op > SVG_OPERATOR_XOR) {
    return NS_ERROR_FAILURE;
  }
  static const gfxContext::GraphicsOperator opMap[] = {
                                           gfxContext::OPERATOR_DEST,
                                           gfxContext::OPERATOR_OVER,
                                           gfxContext::OPERATOR_IN,
                                           gfxContext::OPERATOR_OUT,
                                           gfxContext::OPERATOR_ATOP,
                                           gfxContext::OPERATOR_XOR };
  ctx.SetOperator(opMap[op]);
  ctx.SetSource(aSources[0]->mImage);
  ctx.Paint();
  return NS_OK;
}

void
nsSVGFECompositeElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

nsIntRect
nsSVGFECompositeElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  PRUint16 op = mEnumAttributes[OPERATOR].GetAnimValue();

  if (op == nsSVGFECompositeElement::SVG_OPERATOR_ARITHMETIC) {
    // "arithmetic" operator can produce non-zero alpha values even where
    // all input alphas are zero, so we can actually render outside the
    // union of the source bboxes.
    // XXX we could also check that k4 is nonzero and check for other
    // cases like k1/k2 or k1/k3 zero.
    return GetMaxRect();
  }

  if (op == nsSVGFECompositeElement::SVG_OPERATOR_IN ||
      op == nsSVGFECompositeElement::SVG_OPERATOR_ATOP) {
    // We will only draw where in2 has nonzero alpha, so it's a good
    // bounding box for us
    return aSourceBBoxes[1];
  }

  // The regular Porter-Duff operators always compute zero alpha values
  // where all sources have zero alpha, so the union of their bounding
  // boxes is also a bounding box for our rendering
  return nsSVGFECompositeElementBase::ComputeTargetBBox(aSourceBBoxes, aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFECompositeElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFECompositeElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFECompositeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Component Transfer------------------------

typedef nsSVGFE nsSVGFEComponentTransferElementBase;

class nsSVGFEComponentTransferElement : public nsSVGFEComponentTransferElementBase,
                                        public nsIDOMSVGFEComponentTransferElement
{
  friend nsresult NS_NewSVGFEComponentTransferElement(nsIContent **aResult,
                                                      already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEComponentTransferElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEComponentTransferElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEComponentTransferElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  // Component Transfer
  NS_DECL_NSIDOMSVGFECOMPONENTTRANSFERELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEComponentTransferElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEComponentTransferElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEComponentTransferElementBase::)

  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual PRBool OperatesOnPremultipledAlpha(PRInt32) { return PR_FALSE; }

  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

nsSVGElement::StringInfo nsSVGFEComponentTransferElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEComponentTransfer)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)

DOMCI_NODE_DATA(SVGFEComponentTransferElement, nsSVGFEComponentTransferElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEComponentTransferElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEComponentTransferElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEComponentTransferElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEComponentTransferElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEComponentTransferElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEComponentTransferElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEComponentTransferElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP
nsSVGFEComponentTransferElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFEComponentTransferElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//--------------------------------------------

#define NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID \
    { 0xafab106d, 0xbc18, 0x4f7f, \
  { 0x9e, 0x29, 0xfe, 0xb4, 0xb0, 0x16, 0x5f, 0xf4 } }

typedef nsSVGElement nsSVGComponentTransferFunctionElementBase;

class nsSVGComponentTransferFunctionElement : public nsSVGComponentTransferFunctionElementBase
{
  friend nsresult NS_NewSVGComponentTransferFunctionElement(nsIContent **aResult,
                                                            already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGComponentTransferFunctionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGComponentTransferFunctionElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT

  virtual PRInt32 GetChannel() = 0;
  void GenerateLookupTable(PRUint8* aTable);

protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual void DidAnimateNumber(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }
  virtual EnumAttributesInfo GetEnumInfo();
  virtual void DidAnimateEnum(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }
  virtual NumberListAttributesInfo GetNumberListInfo();
  virtual void DidAnimateNumberList(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }

  // nsIDOMSVGComponentTransferFunctionElement properties:
  enum { TABLEVALUES };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];

  enum { SLOPE, INTERCEPT, AMPLITUDE, EXPONENT, OFFSET };
  nsSVGNumber2 mNumberAttributes[5];
  static NumberInfo sNumberInfo[5];

  enum { TYPE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sTypeMap[];
  static EnumInfo sEnumInfo[1];
};

nsresult
nsSVGFEComponentTransferElement::Filter(nsSVGFilterInstance *instance,
                                        const nsTArray<const Image*>& aSources,
                                        const Image* aTarget,
                                        const nsIntRect& rect)
{
  PRUint8* sourceData = aSources[0]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  PRUint8 tableR[256], tableG[256], tableB[256], tableA[256];
  for (int i=0; i<256; i++)
    tableR[i] = tableG[i] = tableB[i] = tableA[i] = i;
  PRUint8* tables[] = { tableR, tableG, tableB, tableA };
  PRUint32 count = GetChildCount();
  for (PRUint32 k = 0; k < count; k++) {
    nsRefPtr<nsSVGComponentTransferFunctionElement> child;
    CallQueryInterface(GetChildAt(k),
            (nsSVGComponentTransferFunctionElement**)getter_AddRefs(child));
    if (child) {
      child->GenerateLookupTable(tables[child->GetChannel()]);
    }
  }

  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
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
  }
  return NS_OK;
}

void
nsSVGFEComponentTransferElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsSVGElement::NumberListInfo nsSVGComponentTransferFunctionElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::tableValues }
};

nsSVGElement::NumberInfo nsSVGComponentTransferFunctionElement::sNumberInfo[5] =
{
  { &nsGkAtoms::slope,     1, PR_FALSE },
  { &nsGkAtoms::intercept, 0, PR_FALSE },
  { &nsGkAtoms::amplitude, 1, PR_FALSE },
  { &nsGkAtoms::exponent,  1, PR_FALSE },
  { &nsGkAtoms::offset,    0, PR_FALSE }
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

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGComponentTransferFunctionElement, NS_SVG_FE_COMPONENT_TRANSFER_FUNCTION_ELEMENT_CID)

NS_INTERFACE_MAP_BEGIN(nsSVGComponentTransferFunctionElement)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(nsSVGComponentTransferFunctionElement)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElementBase)


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
  *aTableValues = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[TABLEVALUES],
                                                          this, TABLEVALUES).get();
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

void
nsSVGComponentTransferFunctionElement::GenerateLookupTable(PRUint8 *aTable)
{
  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();

  float slope, intercept, amplitude, exponent, offset;
  GetAnimatedNumberValues(&slope, &intercept, &amplitude, 
                          &exponent, &offset, nsnull);

  const SVGNumberList &tableValues =
    mNumberListAttributes[TABLEVALUES].GetAnimValue();
  PRUint32 tvLength = tableValues.Length();

  PRUint32 i;

  switch (type) {
  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
  {
    if (tableValues.Length() <= 1)
      break;

    for (i = 0; i < 256; i++) {
      PRUint32 k = (i * (tvLength - 1)) / 255;
      float v1 = tableValues[k];
      float v2 = tableValues[NS_MIN(k + 1, tvLength - 1)];
      PRInt32 val =
        PRInt32(255 * (v1 + (i/255.0f - k/float(tvLength-1))*(tvLength - 1)*(v2 - v1)));
      val = NS_MIN(255, val);
      val = NS_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
  {
    if (tableValues.Length() <= 1)
      break;

    for (i = 0; i < 256; i++) {
      PRUint32 k = (i * tvLength) / 255;
      k = NS_MIN(k, tvLength - 1);
      float v = tableValues[k];
      PRInt32 val = PRInt32(255 * v);
      val = NS_MIN(255, val);
      val = NS_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
  {
    for (i = 0; i < 256; i++) {
      PRInt32 val = PRInt32(slope * i + 255 * intercept);
      val = NS_MIN(255, val);
      val = NS_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
  {
    for (i = 0; i < 256; i++) {
      PRInt32 val = PRInt32(255 * (amplitude * pow(i / 255.0f, exponent) + offset));
      val = NS_MIN(255, val);
      val = NS_MAX(0, val);
      aTable[i] = val;
    }
    break;
  }

  case nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
  default:
    break;
  }
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberListAttributesInfo
nsSVGComponentTransferFunctionElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  NS_ARRAY_LENGTH(sNumberListInfo));
}

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
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEFuncRElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCRELEMENT

  virtual PRInt32 GetChannel() { return 0; }
  
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncRElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncRElement,nsSVGComponentTransferFunctionElement)

DOMCI_NODE_DATA(SVGFEFuncRElement, nsSVGFEFuncRElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEFuncRElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEFuncRElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGComponentTransferFunctionElement,
                           nsIDOMSVGFEFuncRElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEFuncRElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncR)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncRElement)


class nsSVGFEFuncGElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncGElement
{
  friend nsresult NS_NewSVGFEFuncGElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEFuncGElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCGELEMENT

  virtual PRInt32 GetChannel() { return 1; }

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncGElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncGElement,nsSVGComponentTransferFunctionElement)

DOMCI_NODE_DATA(SVGFEFuncGElement, nsSVGFEFuncGElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEFuncGElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEFuncGElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGComponentTransferFunctionElement,
                           nsIDOMSVGFEFuncGElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEFuncGElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncG)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncGElement)


class nsSVGFEFuncBElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncBElement
{
  friend nsresult NS_NewSVGFEFuncBElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEFuncBElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCBELEMENT

  virtual PRInt32 GetChannel() { return 2; }

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncBElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncBElement,nsSVGComponentTransferFunctionElement)

DOMCI_NODE_DATA(SVGFEFuncBElement, nsSVGFEFuncBElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEFuncBElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEFuncBElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGComponentTransferFunctionElement,
                           nsIDOMSVGFEFuncBElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEFuncBElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncB)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncBElement)


class nsSVGFEFuncAElement : public nsSVGComponentTransferFunctionElement,
                            public nsIDOMSVGFEFuncAElement
{
  friend nsresult NS_NewSVGFEFuncAElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEFuncAElement(already_AddRefed<nsINodeInfo> aNodeInfo) 
    : nsSVGComponentTransferFunctionElement(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_FORWARD_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT(nsSVGComponentTransferFunctionElement::)

  NS_DECL_NSIDOMSVGFEFUNCAELEMENT

  virtual PRInt32 GetChannel() { return 3; }

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMNODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
};

NS_IMPL_ADDREF_INHERITED(nsSVGFEFuncAElement,nsSVGComponentTransferFunctionElement)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFuncAElement,nsSVGComponentTransferFunctionElement)

DOMCI_NODE_DATA(SVGFEFuncAElement, nsSVGFEFuncAElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEFuncAElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEFuncAElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGComponentTransferFunctionElement,
                           nsIDOMSVGFEFuncAElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEFuncAElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGComponentTransferFunctionElement)

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFuncA)
NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFuncAElement)

//---------------------Merge------------------------

typedef nsSVGFE nsSVGFEMergeElementBase;

class nsSVGFEMergeElement : public nsSVGFEMergeElementBase,
                            public nsIDOMSVGFEMergeElement
{
  friend nsresult NS_NewSVGFEMergeElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEMergeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEMergeElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEMergeElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);

  // Merge
  NS_DECL_NSIDOMSVGFEMERGEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMergeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeElementBase::)

  // nsIContent
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

typedef nsSVGStylableElement nsSVGFEMergeNodeElementBase;

#define NS_SVG_FE_MERGE_NODE_CID \
    { 0x413687ec, 0x77fd, 0x4077, \
  { 0x9d, 0x7a, 0x97, 0x51, 0xa8, 0x4b, 0x7b, 0x40 } }

class nsSVGFEMergeNodeElement : public nsSVGFEMergeNodeElementBase,
                                public nsIDOMSVGFEMergeNodeElement
{
  friend nsresult NS_NewSVGFEMergeNodeElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEMergeNodeElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEMergeNodeElementBase(aNodeInfo) {}

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SVG_FE_MERGE_NODE_CID)

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSVGFEMERGENODEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeNodeElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMergeNodeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeNodeElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  const nsSVGString* In1() { return &mStringAttributes[IN1]; }
  
  operator nsISupports*() { return static_cast<nsIContent*>(this); }

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual StringAttributesInfo GetStringInfo();

  enum { IN1 };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};

nsSVGElement::StringInfo nsSVGFEMergeElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMerge)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMergeElement,nsSVGFEMergeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMergeElement,nsSVGFEMergeElementBase)

DOMCI_NODE_DATA(SVGFEMergeElement, nsSVGFEMergeElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEMergeElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEMergeElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEMergeElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEMergeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMergeElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMergeElement)

nsresult
nsSVGFEMergeElement::Filter(nsSVGFilterInstance *instance,
                            const nsTArray<const Image*>& aSources,
                            const Image* aTarget,
                            const nsIntRect& rect)
{
  gfxContext ctx(aTarget->mImage);
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);

  for (PRUint32 i = 0; i < aSources.Length(); i++) {
    ctx.SetSource(aSources[i]->mImage);
    ctx.Paint();
  }
  return NS_OK;
}

void
nsSVGFEMergeElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent* child = GetChildAt(i);
    nsRefPtr<nsSVGFEMergeNodeElement> node;
    CallQueryInterface(child, (nsSVGFEMergeNodeElement**)getter_AddRefs(node));
    if (node) {
      aSources.AppendElement(nsSVGStringInfo(node->In1(), node));
    }
  }
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFEMergeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Merge Node------------------------

NS_DEFINE_STATIC_IID_ACCESSOR(nsSVGFEMergeNodeElement, NS_SVG_FE_MERGE_NODE_CID)

nsSVGElement::StringInfo nsSVGFEMergeNodeElement::sStringInfo[1] =
{
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMergeNode)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMergeNodeElement,nsSVGFEMergeNodeElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMergeNodeElement,nsSVGFEMergeNodeElementBase)

DOMCI_NODE_DATA(SVGFEMergeNodeElement, nsSVGFEMergeNodeElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEMergeNodeElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGFEMergeNodeElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGFEMergeNodeElement)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(nsSVGFEMergeNodeElement)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEMergeNodeElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMergeNodeElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMergeNodeElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEMergeNodeElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEMergeNodeElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFEMergeNodeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Offset------------------------

typedef nsSVGFE nsSVGFEOffsetElementBase;

class nsSVGFEOffsetElement : public nsSVGFEOffsetElementBase,
                             public nsIDOMSVGFEOffsetElement
{
  friend nsresult NS_NewSVGFEOffsetElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEOffsetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEOffsetElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEOffsetElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Offset
  NS_DECL_NSIDOMSVGFEOFFSETELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEOffsetElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEOffsetElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEOffsetElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  nsIntPoint GetOffset(const nsSVGFilterInstance& aInstance);
  
  virtual NumberAttributesInfo GetNumberInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { DX, DY };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

nsSVGElement::NumberInfo nsSVGFEOffsetElement::sNumberInfo[2] =
{
  { &nsGkAtoms::dx, 0, PR_FALSE },
  { &nsGkAtoms::dy, 0, PR_FALSE }
};

nsSVGElement::StringInfo nsSVGFEOffsetElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEOffset)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)

DOMCI_NODE_DATA(SVGFEOffsetElement, nsSVGFEOffsetElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEOffsetElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEOffsetElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEOffsetElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEOffsetElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEOffsetElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEOffsetElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEOffsetElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
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

nsIntPoint
nsSVGFEOffsetElement::GetOffset(const nsSVGFilterInstance& aInstance)
{
  return nsIntPoint(PRInt32(aInstance.GetPrimitiveNumber(
                              nsSVGUtils::X, &mNumberAttributes[DX])),
                    PRInt32(aInstance.GetPrimitiveNumber(
                              nsSVGUtils::Y, &mNumberAttributes[DY])));
}

nsresult
nsSVGFEOffsetElement::Filter(nsSVGFilterInstance *instance,
                             const nsTArray<const Image*>& aSources,
                             const Image* aTarget,
                             const nsIntRect& rect)
{
  nsIntPoint offset = GetOffset(*instance);

  gfxContext ctx(aTarget->mImage);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  // Ensure rendering is limited to the filter primitive subregion
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);
  ctx.Translate(gfxPoint(offset.x, offset.y));
  ctx.SetSource(aSources[0]->mImage);
  ctx.Paint();

  return NS_OK;
}

void
nsSVGFEOffsetElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEOffsetElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return aSourceBBoxes[0] + GetOffset(aInstance);
}

nsIntRect
nsSVGFEOffsetElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                        const nsSVGFilterInstance& aInstance)
{
  return aSourceChangeBoxes[0] + GetOffset(aInstance);
}

void
nsSVGFEOffsetElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = aTargetBBox - GetOffset(aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEOffsetElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEOffsetElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Flood------------------------

typedef nsSVGFE nsSVGFEFloodElementBase;

class nsSVGFEFloodElement : public nsSVGFEFloodElementBase,
                            public nsIDOMSVGFEFloodElement
{
  friend nsresult NS_NewSVGFEFloodElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEFloodElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEFloodElementBase(aNodeInfo) {}

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEFloodElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  // Flood
  NS_DECL_NSIDOMSVGFEFLOODELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEFloodElementBase::)
  
  NS_FORWARD_NSIDOMNODE(nsSVGFEFloodElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEFloodElementBase::)

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual PRBool OperatesOnSRGB(nsSVGFilterInstance*,
                                PRInt32, Image*) { return PR_TRUE; }

  virtual StringAttributesInfo GetStringInfo();

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];
};
 
nsSVGElement::StringInfo nsSVGFEFloodElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFlood)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEFloodElement,nsSVGFEFloodElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEFloodElement,nsSVGFEFloodElementBase)

DOMCI_NODE_DATA(SVGFEFloodElement, nsSVGFEFloodElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEFloodElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEFloodElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEFloodElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEFloodElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEFloodElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEFloodElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEFloodElement methods

nsresult
nsSVGFEFloodElement::Filter(nsSVGFilterInstance *instance,
                            const nsTArray<const Image*>& aSources,
                            const Image* aTarget,
                            const nsIntRect& aDataRect)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->GetStyleContext();

  nscolor floodColor = style->GetStyleSVGReset()->mFloodColor;
  float floodOpacity = style->GetStyleSVGReset()->mFloodOpacity;

  gfxContext ctx(aTarget->mImage);
  ctx.SetColor(gfxRGBA(NS_GET_R(floodColor) / 255.0,
                       NS_GET_G(floodColor) / 255.0,
                       NS_GET_B(floodColor) / 255.0,
                       NS_GET_A(floodColor) / 255.0 * floodOpacity));
  ctx.Rectangle(aTarget->mFilterPrimitiveSubregion);
  ctx.Fill();
  return NS_OK;
}

nsIntRect
nsSVGFEFloodElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGFEFloodElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFEFloodElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFEFloodElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Tile------------------------

typedef nsSVGFE nsSVGFETileElementBase;

class nsSVGFETileElement : public nsSVGFETileElementBase,
                           public nsIDOMSVGFETileElement
{
  friend nsresult NS_NewSVGFETileElement(nsIContent **aResult,
                                         already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFETileElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFETileElementBase(aNodeInfo) {}

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFETileElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Tile
  NS_DECL_NSIDOMSVGFETILEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFETileElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFETileElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFETileElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual StringAttributesInfo GetStringInfo();
  
  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

nsSVGElement::StringInfo nsSVGFETileElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FETile)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFETileElement,nsSVGFETileElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFETileElement,nsSVGFETileElementBase)

DOMCI_NODE_DATA(SVGFETileElement, nsSVGFETileElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFETileElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFETileElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFETileElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFETileElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFETileElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFETileElement)


//----------------------------------------------------------------------
// nsSVGFETileElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFETileElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

void
nsSVGFETileElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFETileElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

void
nsSVGFETileElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // Just assume we need the entire source bounding box, so do nothing.
}

nsIntRect
nsSVGFETileElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                      const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

static PRInt32 WrapInterval(PRInt32 aVal, PRInt32 aMax)
{
  aVal = aVal % aMax;
  return aVal < 0 ? aMax + aVal : aVal;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsresult
nsSVGFETileElement::Filter(nsSVGFilterInstance *instance,
                           const nsTArray<const Image*>& aSources,
                           const Image* aTarget,
                           const nsIntRect& rect)
{
  // XXX This code depends on the surface rect containing the filter
  // primitive subregion. ComputeTargetBBox, ComputeNeededSourceBBoxes
  // and ComputeChangeBBox are all pessimal, so that will normally be OK,
  // but nothing clips mFilterPrimitiveSubregion so this should be changed.

  nsIntRect tile;
  PRBool res = gfxUtils::GfxRectToIntRect(aSources[0]->mFilterPrimitiveSubregion, &tile);

  NS_ENSURE_TRUE(res, NS_ERROR_FAILURE); // asserts on failure (not 
  if (tile.IsEmpty())
    return NS_OK;

  const nsIntRect &surfaceRect = instance->GetSurfaceRect();
  if (!tile.Intersects(surfaceRect)) {
    // nothing to draw
    return NS_OK;
  }

  // Get it into surface space
  tile -= surfaceRect.TopLeft();

  PRUint8* sourceData = aSources[0]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  // the offset to add to our x/y coordinates (which are relative to the
  // temporary surface data) to get coordinates relative to the origin
  // of the tile
  nsIntPoint offset(-tile.x + tile.width, -tile.y + tile.height);
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    PRUint32 tileY = tile.y + WrapInterval(y + offset.y, tile.height);
    if (tileY < (PRUint32)surfaceRect.height) {
      for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
        PRUint32 tileX = tile.x + WrapInterval(x + offset.x, tile.width);
        if (tileX < (PRUint32)surfaceRect.width) {
          *(PRUint32*)(targetData + y * stride + 4 * x) =
            *(PRUint32*)(sourceData + tileY * stride + 4 * tileX);
        }
      }
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFETileElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Turbulence------------------------

typedef nsSVGFE nsSVGFETurbulenceElementBase;

class nsSVGFETurbulenceElement : public nsSVGFETurbulenceElementBase,
                                 public nsIDOMSVGFETurbulenceElement
{
  friend nsresult NS_NewSVGFETurbulenceElement(nsIContent **aResult,
                                               already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFETurbulenceElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFETurbulenceElementBase(aNodeInfo) {}

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFETurbulenceElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  // Turbulence
  NS_DECL_NSIDOMSVGFETURBULENCEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFETurbulenceElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFETurbulenceElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFETurbulenceElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { SEED }; // floating point seed?!
  nsSVGNumber2 mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { BASE_FREQ };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { OCTAVES };
  nsSVGInteger mIntegerAttributes[1];
  static IntegerInfo sIntegerInfo[1];

  enum { TYPE, STITCHTILES };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sTypeMap[];
  static nsSVGEnumMapping sStitchTilesMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

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

nsSVGElement::NumberInfo nsSVGFETurbulenceElement::sNumberInfo[1] =
{
  { &nsGkAtoms::seed, 0, PR_FALSE }
};

nsSVGElement::NumberPairInfo nsSVGFETurbulenceElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::baseFrequency, 0, 0 }
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

nsSVGElement::StringInfo nsSVGFETurbulenceElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FETurbulence)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFETurbulenceElement,nsSVGFETurbulenceElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFETurbulenceElement,nsSVGFETurbulenceElementBase)

DOMCI_NODE_DATA(SVGFETurbulenceElement, nsSVGFETurbulenceElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFETurbulenceElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFETurbulenceElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFETurbulenceElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFETurbulenceElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFETurbulenceElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFETurbulenceElement)

//----------------------------------------------------------------------
// nsIDOMSVGFETurbulenceElement methods

/* readonly attribute nsIDOMSVGAnimatedNumber baseFrequencyX; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetBaseFrequencyX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(aX, nsSVGNumberPair::eFirst, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber baseFrequencyY; */
NS_IMETHODIMP nsSVGFETurbulenceElement::GetBaseFrequencyY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(aY, nsSVGNumberPair::eSecond, this);
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

nsresult
nsSVGFETurbulenceElement::Filter(nsSVGFilterInstance *instance,
                                 const nsTArray<const Image*>& aSources,
                                 const Image* aTarget,
                                 const nsIntRect& rect)
{
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  nsIntRect filterSubregion(PRInt32(aTarget->mFilterPrimitiveSubregion.X()),
                            PRInt32(aTarget->mFilterPrimitiveSubregion.Y()),
                            PRInt32(aTarget->mFilterPrimitiveSubregion.Width()),
                            PRInt32(aTarget->mFilterPrimitiveSubregion.Height()));

  float fX = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eFirst);
  float fY = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eSecond);
  float seed = mNumberAttributes[OCTAVES].GetAnimValue();
  PRInt32 octaves = mIntegerAttributes[OCTAVES].GetAnimValue();
  PRUint16 type = mEnumAttributes[TYPE].GetAnimValue();
  PRUint16 stitch = mEnumAttributes[STITCHTILES].GetAnimValue();

  InitSeed((PRInt32)seed);

  // XXXroc this makes absolutely no sense to me.
  float filterX = instance->GetFilterRect().X();
  float filterY = instance->GetFilterRect().Y();
  float filterWidth = instance->GetFilterRect().Width();
  float filterHeight = instance->GetFilterRect().Height();

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
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRInt32 targIndex = y * stride + x * 4;
      double point[2];
      point[0] = filterX + (filterWidth * (x + instance->GetSurfaceRect().x)) / (filterSubregion.width - 1);
      point[1] = filterY + (filterHeight * (y + instance->GetSurfaceRect().y)) / (filterSubregion.height - 1);

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
        col[i] = NS_MIN(col[i], 255.f);
        col[i] = NS_MAX(col[i], 0.f);
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

nsIntRect
nsSVGFETurbulenceElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFETurbulenceElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
nsSVGFETurbulenceElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                 NS_ARRAY_LENGTH(sNumberPairInfo));
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

nsSVGElement::StringAttributesInfo
nsSVGFETurbulenceElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Morphology------------------------

typedef nsSVGFE nsSVGFEMorphologyElementBase;

class nsSVGFEMorphologyElement : public nsSVGFEMorphologyElementBase,
                                 public nsIDOMSVGFEMorphologyElement
{
  friend nsresult NS_NewSVGFEMorphologyElement(nsIContent **aResult,
                                               already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEMorphologyElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEMorphologyElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEMorphologyElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Morphology
  NS_DECL_NSIDOMSVGFEMORPHOLOGYELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMorphologyElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEMorphologyElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMorphologyElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  void GetRXY(PRInt32 *aRX, PRInt32 *aRY, const nsSVGFilterInstance& aInstance);
  nsIntRect InflateRect(const nsIntRect& aRect, const nsSVGFilterInstance& aInstance);

  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { RADIUS };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { OPERATOR };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sOperatorMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

nsSVGElement::NumberPairInfo nsSVGFEMorphologyElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::radius, 0, 0 }
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

nsSVGElement::StringInfo nsSVGFEMorphologyElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMorphology)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)

DOMCI_NODE_DATA(SVGFEMorphologyElement, nsSVGFEMorphologyElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEMorphologyElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEMorphologyElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEMorphologyElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEMorphologyElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMorphologyElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMorphologyElement)


//----------------------------------------------------------------------
// nsSVGFEMorphologyElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration operator; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetOperator(nsIDOMSVGAnimatedEnumeration * *aOperator)
{
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(aOperator, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusX; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(aX, nsSVGNumberPair::eFirst, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusY; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(aY, nsSVGNumberPair::eSecond, this);
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::SetRadius(float rx, float ry)
{
  NS_ENSURE_FINITE2(rx, ry, NS_ERROR_ILLEGAL_VALUE);
  mNumberPairAttributes[RADIUS].SetBaseValues(rx, ry, this, PR_TRUE);
  return NS_OK;
}

void
nsSVGFEMorphologyElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEMorphologyElement::InflateRect(const nsIntRect& aRect,
                                      const nsSVGFilterInstance& aInstance)
{
  PRInt32 rx, ry;
  GetRXY(&rx, &ry, aInstance);
  nsIntRect result = aRect;
  result.Inflate(NS_MAX(0, rx), NS_MAX(0, ry));
  return result;
}

nsIntRect
nsSVGFEMorphologyElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return InflateRect(aSourceBBoxes[0], aInstance);
}

void
nsSVGFEMorphologyElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = InflateRect(aTargetBBox, aInstance);
}

nsIntRect
nsSVGFEMorphologyElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                            const nsSVGFilterInstance& aInstance)
{
  return InflateRect(aSourceChangeBoxes[0], aInstance);
}

#define MORPHOLOGY_EPSILON 0.0001

void
nsSVGFEMorphologyElement::GetRXY(PRInt32 *aRX, PRInt32 *aRY,
                                 const nsSVGFilterInstance& aInstance)
{
  // Subtract an epsilon here because we don't want a value that's just
  // slightly larger than an integer to round up to the next integer; it's
  // probably meant to be the integer it's close to, modulo machine precision
  // issues.
  *aRX = NSToIntCeil(aInstance.GetPrimitiveNumber(nsSVGUtils::X,
                                                  &mNumberPairAttributes[RADIUS],
                                                  nsSVGNumberPair::eFirst) -
                     MORPHOLOGY_EPSILON);
  *aRY = NSToIntCeil(aInstance.GetPrimitiveNumber(nsSVGUtils::Y,
                                                  &mNumberPairAttributes[RADIUS],
                                                  nsSVGNumberPair::eSecond) -
                     MORPHOLOGY_EPSILON);
}

nsresult
nsSVGFEMorphologyElement::Filter(nsSVGFilterInstance *instance,
                                 const nsTArray<const Image*>& aSources,
                                 const Image* aTarget,
                                 const nsIntRect& rect)
{
  PRInt32 rx, ry;
  GetRXY(&rx, &ry, *instance);

  if (rx < 0 || ry < 0) {
    // XXX nsSVGUtils::ReportToConsole()
    return NS_OK;
  }
  if (rx == 0 && ry == 0) {
    return NS_OK;
  }

  PRUint8* sourceData = aSources[0]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();
  PRUint32 xExt[4], yExt[4];  // X, Y indices of RGBA extrema
  PRUint8 extrema[4];         // RGBA magnitude of extrema
  PRUint16 op = mEnumAttributes[OPERATOR].GetAnimValue();

  /* Scan the kernel for each pixel to determine max/min RGBA values.  Note that
   * as we advance in the x direction, each kernel overlaps the previous kernel.
   * Thus, we can avoid iterating over the entire kernel by comparing the
   * leading edge of the new kernel against the extrema found in the previous
   * kernel.   We must still scan the entire kernel if the previous extrema do
   * not fall within the current kernel or if we are starting a new row.
   */
  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    PRUint32 startY = NS_MAX(0, y - ry);
    // We need to read pixels not just in 'rect', which is limited to
    // the dirty part of our filter primitive subregion, but all pixels in
    // the given radii from the source surface, so use the surface size here.
    PRUint32 endY = NS_MIN(y + ry, instance->GetSurfaceHeight() - 1);
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint32 startX = NS_MAX(0, x - rx);
      PRUint32 endX = NS_MIN(x + rx, instance->GetSurfaceWidth() - 1);
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

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
nsSVGFEMorphologyElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  NS_ARRAY_LENGTH(sNumberPairInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEMorphologyElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEMorphologyElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------Convolve Matrix------------------------

typedef nsSVGFE nsSVGFEConvolveMatrixElementBase;

class nsSVGFEConvolveMatrixElement : public nsSVGFEConvolveMatrixElementBase,
                                     public nsIDOMSVGFEConvolveMatrixElement
{
  friend nsresult NS_NewSVGFEConvolveMatrixElement(nsIContent **aResult,
                                                   already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEConvolveMatrixElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEConvolveMatrixElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEConvolveMatrixElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // Color Matrix
  NS_DECL_NSIDOMSVGFECONVOLVEMATRIXELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEConvolveMatrixElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEConvolveMatrixElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEConvolveMatrixElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual PRBool OperatesOnPremultipledAlpha(PRInt32) {
    return !mBooleanAttributes[PRESERVEALPHA].GetAnimValue();
  }

  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual IntegerAttributesInfo GetIntegerInfo();
  virtual IntegerPairAttributesInfo GetIntegerPairInfo();
  virtual BooleanAttributesInfo GetBooleanInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();
  virtual NumberListAttributesInfo GetNumberListInfo();

  enum { DIVISOR, BIAS };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];

  enum { KERNEL_UNIT_LENGTH };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { TARGET_X, TARGET_Y };
  nsSVGInteger mIntegerAttributes[2];
  static IntegerInfo sIntegerInfo[2];

  enum { ORDER };
  nsSVGIntegerPair mIntegerPairAttributes[1];
  static IntegerPairInfo sIntegerPairInfo[1];

  enum { PRESERVEALPHA };
  nsSVGBoolean mBooleanAttributes[1];
  static BooleanInfo sBooleanInfo[1];

  enum { EDGEMODE };
  nsSVGEnum mEnumAttributes[1];
  static nsSVGEnumMapping sEdgeModeMap[];
  static EnumInfo sEnumInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  enum { KERNELMATRIX };
  SVGAnimatedNumberList mNumberListAttributes[1];
  static NumberListInfo sNumberListInfo[1];
};

nsSVGElement::NumberInfo nsSVGFEConvolveMatrixElement::sNumberInfo[2] =
{
  { &nsGkAtoms::divisor, 1, PR_FALSE },
  { &nsGkAtoms::bias, 0, PR_FALSE }
};

nsSVGElement::NumberPairInfo nsSVGFEConvolveMatrixElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::IntegerInfo nsSVGFEConvolveMatrixElement::sIntegerInfo[2] =
{
  { &nsGkAtoms::targetX, 0 },
  { &nsGkAtoms::targetY, 0 }
};

nsSVGElement::IntegerPairInfo nsSVGFEConvolveMatrixElement::sIntegerPairInfo[1] =
{
  { &nsGkAtoms::order, 3, 3 }
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

nsSVGElement::StringInfo nsSVGFEConvolveMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

nsSVGElement::NumberListInfo nsSVGFEConvolveMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::kernelMatrix }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEConvolveMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEConvolveMatrixElement,nsSVGFEConvolveMatrixElementBase)

DOMCI_NODE_DATA(SVGFEConvolveMatrixElement, nsSVGFEConvolveMatrixElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEConvolveMatrixElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEConvolveMatrixElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEConvolveMatrixElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEConvolveMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEConvolveMatrixElementBase)


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEConvolveMatrixElement)

//----------------------------------------------------------------------
// nsSVGFEConvolveMatrixElement methods

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderX(nsIDOMSVGAnimatedInteger * *aOrderX)
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(aOrderX, nsSVGIntegerPair::eFirst, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetOrderY(nsIDOMSVGAnimatedInteger * *aOrderY)
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(aOrderY, nsSVGIntegerPair::eSecond, this);
}

NS_IMETHODIMP nsSVGFEConvolveMatrixElement::GetKernelMatrix(nsIDOMSVGAnimatedNumberList * *aKernelMatrix)
{
  *aKernelMatrix = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[KERNELMATRIX],
                                                           this, KERNELMATRIX).get();
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
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFEConvolveMatrixElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
                                                                       this);
}

void
nsSVGFEConvolveMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEConvolveMatrixElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  // XXX A more precise box is possible when 'bias' is zero and 'edgeMode' is
  // 'none', but it requires analysis of 'kernelUnitLength', 'order' and
  // 'targetX/Y', so it's quite a lot of work. Don't do it for now.
  return GetMaxRect();
}

void
nsSVGFEConvolveMatrixElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // XXX Precise results are possible but we're going to skip that work
  // for now. Do nothing, which means the needed-box remains the
  // source's output bounding box.
}

nsIntRect
nsSVGFEConvolveMatrixElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                                const nsSVGFilterInstance& aInstance)
{
  // XXX Precise results are possible but we're going to skip that work
  // for now.
  return GetMaxRect();
}

static PRInt32 BoundInterval(PRInt32 aVal, PRInt32 aMax)
{
  aVal = NS_MAX(aVal, 0);
  return NS_MIN(aVal, aMax - 1);
}

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
                aSourceData[BoundInterval(sampleY, aHeight) * aStride +
                            BoundInterval(sampleX, aWidth) * 4 + offsets[i]] *
                aKernel[aOrderX * y + x];
              break;
            case nsSVGFEConvolveMatrixElement::SVG_EDGEMODE_WRAP:
              sum[i] +=
                aSourceData[WrapInterval(sampleY, aHeight) * aStride +
                            WrapInterval(sampleX, aWidth) * 4 + offsets[i]] *
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
      BoundInterval(static_cast<PRInt32>(sum[i] / aDivisor + aBias), 256);
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A] =
      aSourceData[aY * aStride + 4 * aX + GFX_ARGB32_OFFSET_A];
  }
}

nsresult
nsSVGFEConvolveMatrixElement::Filter(nsSVGFilterInstance *instance,
                                     const nsTArray<const Image*>& aSources,
                                     const Image* aTarget,
                                     const nsIntRect& rect)
{
  const SVGNumberList &kernelMatrix =
    mNumberListAttributes[KERNELMATRIX].GetAnimValue();
  PRUint32 kmLength = kernelMatrix.Length();

  PRInt32 orderX = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eFirst);
  PRInt32 orderY = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eSecond);

  if (orderX <= 0 || orderY <= 0 ||
      static_cast<PRUint32>(orderX * orderY) != kmLength) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 targetX, targetY;
  GetAnimatedIntegerValues(&targetX, &targetY, nsnull);

  if (mIntegerAttributes[TARGET_X].IsExplicitlySet()) {
    if (targetX < 0 || targetX >= orderX)
      return NS_ERROR_FAILURE;
  } else {
    targetX = orderX / 2;
  }
  if (mIntegerAttributes[TARGET_Y].IsExplicitlySet()) {
    if (targetY < 0 || targetY >= orderY)
      return NS_ERROR_FAILURE;
  } else {
    targetY = orderY / 2;
  }

  if (orderX > NS_SVG_OFFSCREEN_MAX_DIMENSION ||
      orderY > NS_SVG_OFFSCREEN_MAX_DIMENSION)
    return NS_ERROR_FAILURE;
  nsAutoArrayPtr<float> kernel(new float[orderX * orderY]);
  if (!kernel)
    return NS_ERROR_FAILURE;
  for (PRUint32 i = 0; i < kmLength; i++) {
    kernel[kmLength - 1 - i] = kernelMatrix[i];
  }

  float divisor;
  if (mNumberAttributes[DIVISOR].IsExplicitlySet()) {
    divisor = mNumberAttributes[DIVISOR].GetAnimValue();
    if (divisor == 0)
      return NS_ERROR_FAILURE;
  } else {
    divisor = kernel[0];
    for (PRUint32 i = 1; i < kmLength; i++)
      divisor += kernel[i];
    if (divisor == 0)
      divisor = 1;
  }

  ScaleInfo info = SetupScalingFilter(instance, aSources[0], aTarget, rect,
                                      &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);
  if (!info.mTarget)
    return NS_ERROR_FAILURE;

  PRUint16 edgeMode = mEnumAttributes[EDGEMODE].GetAnimValue();
  PRBool preserveAlpha = mBooleanAttributes[PRESERVEALPHA].GetAnimValue();

  float bias = mNumberAttributes[BIAS].GetAnimValue();

  const nsIntRect& dataRect = info.mDataRect;
  PRInt32 stride = info.mSource->Stride();
  PRInt32 width = info.mSource->GetSize().width;
  PRInt32 height = info.mSource->GetSize().height;
  PRUint8 *sourceData = info.mSource->Data();
  PRUint8 *targetData = info.mTarget->Data();

  for (PRInt32 y = dataRect.y; y < dataRect.YMost(); y++) {
    for (PRInt32 x = dataRect.x; x < dataRect.XMost(); x++) {
      ConvolvePixel(sourceData, targetData,
                    width, height, stride,
                    x, y,
                    edgeMode, kernel, divisor, bias, preserveAlpha,
                    orderX, orderY, targetX, targetY);
    }
  }

  FinishScalingFilter(&info);

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

nsSVGElement::NumberPairAttributesInfo
nsSVGFEConvolveMatrixElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  NS_ARRAY_LENGTH(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
nsSVGFEConvolveMatrixElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               NS_ARRAY_LENGTH(sIntegerInfo));
}

nsSVGElement::IntegerPairAttributesInfo
nsSVGFEConvolveMatrixElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(mIntegerPairAttributes, sIntegerPairInfo,
                                   NS_ARRAY_LENGTH(sIntegerPairInfo));
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

nsSVGElement::StringAttributesInfo
nsSVGFEConvolveMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
nsSVGFEConvolveMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  NS_ARRAY_LENGTH(sNumberListInfo));
}

//---------------------DistantLight------------------------

typedef nsSVGElement nsSVGFEDistantLightElementBase;

class nsSVGFEDistantLightElement : public nsSVGFEDistantLightElementBase,
                                   public nsIDOMSVGFEDistantLightElement
{
  friend nsresult NS_NewSVGFEDistantLightElement(nsIContent **aResult,
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEDistantLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEDistantLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFEDISTANTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDistantLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFEDistantLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEDistantLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual void DidAnimateNumber(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }

  enum { AZIMUTH, ELEVATION };
  nsSVGNumber2 mNumberAttributes[2];
  static NumberInfo sNumberInfo[2];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDistantLight)

nsSVGElement::NumberInfo nsSVGFEDistantLightElement::sNumberInfo[2] =
{
  { &nsGkAtoms::azimuth,   0, PR_FALSE },
  { &nsGkAtoms::elevation, 0, PR_FALSE }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDistantLightElement,nsSVGFEDistantLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDistantLightElement,nsSVGFEDistantLightElementBase)

DOMCI_NODE_DATA(SVGFEDistantLightElement, nsSVGFEDistantLightElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEDistantLightElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGFEDistantLightElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFEDistantLightElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEDistantLightElement)
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
                                                 already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEPointLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEPointLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFEPOINTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEPointLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFEPointLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEPointLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual void DidAnimateNumber(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }

  enum { X, Y, Z };
  nsSVGNumber2 mNumberAttributes[3];
  static NumberInfo sNumberInfo[3];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEPointLight)

nsSVGElement::NumberInfo nsSVGFEPointLightElement::sNumberInfo[3] =
{
  { &nsGkAtoms::x, 0, PR_FALSE },
  { &nsGkAtoms::y, 0, PR_FALSE },
  { &nsGkAtoms::z, 0, PR_FALSE }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEPointLightElement,nsSVGFEPointLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEPointLightElement,nsSVGFEPointLightElementBase)

DOMCI_NODE_DATA(SVGFEPointLightElement, nsSVGFEPointLightElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEPointLightElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGFEPointLightElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFEPointLightElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEPointLightElement)
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
                                              already_AddRefed<nsINodeInfo> aNodeInfo);
  friend class nsSVGFELightingElement;
protected:
  nsSVGFESpotLightElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFESpotLightElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGFESPOTLIGHTELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFESpotLightElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFESpotLightElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFESpotLightElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual NumberAttributesInfo GetNumberInfo();
  virtual void DidAnimateNumber(PRUint8 aAttrEnum) {
    DidAnimateAttrViaParent(this);
  }

  enum { X, Y, Z, POINTS_AT_X, POINTS_AT_Y, POINTS_AT_Z,
         SPECULAR_EXPONENT, LIMITING_CONE_ANGLE };
  nsSVGNumber2 mNumberAttributes[8];
  static NumberInfo sNumberInfo[8];
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpotLight)

nsSVGElement::NumberInfo nsSVGFESpotLightElement::sNumberInfo[8] =
{
  { &nsGkAtoms::x, 0, PR_FALSE },
  { &nsGkAtoms::y, 0, PR_FALSE },
  { &nsGkAtoms::z, 0, PR_FALSE },
  { &nsGkAtoms::pointsAtX, 0, PR_FALSE },
  { &nsGkAtoms::pointsAtY, 0, PR_FALSE },
  { &nsGkAtoms::pointsAtZ, 0, PR_FALSE },
  { &nsGkAtoms::specularExponent, 1, PR_FALSE },
  { &nsGkAtoms::limitingConeAngle, 0, PR_FALSE }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpotLightElement,nsSVGFESpotLightElementBase)

DOMCI_NODE_DATA(SVGFESpotLightElement, nsSVGFESpotLightElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFESpotLightElement)
  NS_NODE_INTERFACE_TABLE4(nsSVGFESpotLightElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFESpotLightElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFESpotLightElement)
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

class nsSVGFELightingElement : public nsSVGFELightingElementBase
{
protected:
  nsSVGFELightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFELightingElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFELightingElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  // XXX shouldn't we have ComputeTargetBBox here, since the output can
  // extend beyond the bounds of the inputs thanks to the convolution kernel?
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFELightingElementBase::)
  NS_FORWARD_NSIDOMNODE(nsSVGFELightingElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFELightingElementBase::)

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

protected:
  virtual void
  LightPixel(const float *N, const float *L,
             nscolor color, PRUint8 *targetData) = 0;

  virtual NumberAttributesInfo GetNumberInfo();
  virtual NumberPairAttributesInfo GetNumberPairInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { SURFACE_SCALE, DIFFUSE_CONSTANT, SPECULAR_CONSTANT, SPECULAR_EXPONENT };
  nsSVGNumber2 mNumberAttributes[4];
  static NumberInfo sNumberInfo[4];

  enum { KERNEL_UNIT_LENGTH };
  nsSVGNumberPair mNumberPairAttributes[1];
  static NumberPairInfo sNumberPairInfo[1];

  enum { RESULT, IN1 };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];
};

nsSVGElement::NumberInfo nsSVGFELightingElement::sNumberInfo[4] =
{
  { &nsGkAtoms::surfaceScale, 1, PR_FALSE },
  { &nsGkAtoms::diffuseConstant, 1, PR_FALSE },
  { &nsGkAtoms::specularConstant, 1, PR_FALSE },
  { &nsGkAtoms::specularExponent, 1, PR_FALSE }
};

nsSVGElement::NumberPairInfo nsSVGFELightingElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::StringInfo nsSVGFELightingElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFELightingElement) 
NS_INTERFACE_MAP_END_INHERITING(nsSVGFELightingElementBase)

//----------------------------------------------------------------------
// Implementation

NS_IMETHODIMP_(PRBool)
nsSVGFELightingElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sLightingEffectsMap
  };

  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFELightingElementBase::IsAttributeMapped(name);
}

void
nsSVGFELightingElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

void
nsSVGFELightingElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // XXX lighting can depend on more than the target area, because
  // of the kernels it uses. We could compute something precise here
  // but just leave it and assume we use the entire source bounding box.
}

nsIntRect
nsSVGFELightingElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                          const nsSVGFilterInstance& aInstance)
{
  // XXX be conservative for now
  return GetMaxRect();
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
GenerateNormal(float *N, const PRUint8 *data, PRInt32 stride,
               PRInt32 surfaceWidth, PRInt32 surfaceHeight,
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
  } else if (x == surfaceWidth - 1) {
    xflag = 2;
  } else {
    xflag = 1;
  }
  if (y == 0) {
    yflag = 0;
  } else if (y == surfaceHeight - 1) {
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

nsresult
nsSVGFELightingElement::Filter(nsSVGFilterInstance *instance,
                               const nsTArray<const Image*>& aSources,
                               const Image* aTarget,
                               const nsIntRect& rect)
{
  ScaleInfo info = SetupScalingFilter(instance, aSources[0], aTarget, rect,
                                      &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);
  if (!info.mTarget)
    return NS_ERROR_FAILURE;

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
  float lightPos[3], pointsAt[3], specularExponent;
  float cosConeAngle = 0;
  if (pointLight) {
    static_cast<nsSVGFEPointLightElement*>
      (pointLight.get())->GetAnimatedNumberValues(lightPos,
                                                  lightPos + 1,
                                                  lightPos + 2,
                                                  nsnull);
    instance->ConvertLocation(lightPos);
  }
  if (spotLight) {
    float limitingConeAngle;
    nsSVGFESpotLightElement* spot = 
      static_cast<nsSVGFESpotLightElement*>(spotLight.get());
    spot->GetAnimatedNumberValues(lightPos,
                                  lightPos + 1,
                                  lightPos + 2,
                                  pointsAt,
                                  pointsAt + 1,
                                  pointsAt + 2,
                                  &specularExponent,
                                  &limitingConeAngle,
                                  nsnull);
    instance->ConvertLocation(lightPos);
    instance->ConvertLocation(pointsAt);

    if (spot->mNumberAttributes[nsSVGFESpotLightElement::LIMITING_CONE_ANGLE].
                                  IsExplicitlySet()) {
      cosConeAngle = NS_MAX<double>(cos(limitingConeAngle * radPerDeg), 0.0);
    }
  }

  float surfaceScale = mNumberAttributes[SURFACE_SCALE].GetAnimValue();

  const nsIntRect& dataRect = info.mDataRect;
  PRInt32 stride = info.mSource->Stride();
  PRUint8 *sourceData = info.mSource->Data();
  PRUint8 *targetData = info.mTarget->Data();
  PRInt32 surfaceWidth = info.mSource->Width();
  PRInt32 surfaceHeight = info.mSource->Height();
  
  for (PRInt32 y = dataRect.y; y < dataRect.YMost(); y++) {
    for (PRInt32 x = dataRect.x; x < dataRect.XMost(); x++) {
      PRInt32 index = y * stride + x * 4;

      float N[3];
      GenerateNormal(N, sourceData, stride, surfaceWidth, surfaceHeight,
                     x, y, surfaceScale);

      if (pointLight || spotLight) {
        gfxPoint pt = instance->FilterSpaceToUserSpace(
                gfxPoint(x + instance->GetSurfaceRect().x,
                         y + instance->GetSurfaceRect().y));
        float Z = surfaceScale * sourceData[index + GFX_ARGB32_OFFSET_A] / 255;

        L[0] = lightPos[0] - pt.x;
        L[1] = lightPos[1] - pt.y;
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

  FinishScalingFilter(&info);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFELightingElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
nsSVGFELightingElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  NS_ARRAY_LENGTH(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFELightingElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//---------------------DiffuseLighting------------------------

typedef nsSVGFELightingElement nsSVGFEDiffuseLightingElementBase;

class nsSVGFEDiffuseLightingElement : public nsSVGFEDiffuseLightingElementBase,
                                      public nsIDOMSVGFEDiffuseLightingElement
{
  friend nsresult NS_NewSVGFEDiffuseLightingElement(nsIContent **aResult,
                                                    already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFEDiffuseLightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
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

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, PRUint8 *targetData);

};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDiffuseLighting)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDiffuseLightingElement,nsSVGFEDiffuseLightingElementBase)

DOMCI_NODE_DATA(SVGFEDiffuseLightingElement, nsSVGFEDiffuseLightingElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEDiffuseLightingElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEDiffuseLightingElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEDiffuseLightingElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEDiffuseLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDiffuseLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDiffuseLightingElement)

//----------------------------------------------------------------------
// nsSVGFEDiffuseLightingElement methods

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
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
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFEDiffuseLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
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
      NS_MIN(PRUint32(diffuseNL * NS_GET_B(color)), 255U);
    targetData[GFX_ARGB32_OFFSET_G] =
      NS_MIN(PRUint32(diffuseNL * NS_GET_G(color)), 255U);
    targetData[GFX_ARGB32_OFFSET_R] =
      NS_MIN(PRUint32(diffuseNL * NS_GET_R(color)), 255U);
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
                                               already_AddRefed<nsINodeInfo> aNodeInfo);
protected:
  nsSVGFESpecularLightingElement(already_AddRefed<nsINodeInfo> aNodeInfo)
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

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual void LightPixel(const float *N, const float *L,
                          nscolor color, PRUint8 *targetData);

};

NS_IMPL_NS_NEW_SVG_ELEMENT(FESpecularLighting)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFESpecularLightingElement,nsSVGFESpecularLightingElementBase)

DOMCI_NODE_DATA(SVGFESpecularLightingElement, nsSVGFESpecularLightingElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFESpecularLightingElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFESpecularLightingElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFESpecularLightingElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFESpecularLightingElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFESpecularLightingElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFESpecularLightingElement)

//----------------------------------------------------------------------
// nsSVGFESpecularLightingElement methods

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
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
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelX,
                                                                       nsSVGNumberPair::eFirst,
                                                                       this);
}

NS_IMETHODIMP
nsSVGFESpecularLightingElement::GetKernelUnitLengthY(nsIDOMSVGAnimatedNumber **aKernelY)
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(aKernelY,
                                                                       nsSVGNumberPair::eSecond,
                                                                       this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsresult
nsSVGFESpecularLightingElement::Filter(nsSVGFilterInstance *instance,
                                       const nsTArray<const Image*>& aSources,
                                       const Image* aTarget,
                                       const nsIntRect& rect)
{
  float specularExponent = mNumberAttributes[SPECULAR_EXPONENT].GetAnimValue();

  // specification defined range (15.22)
  if (specularExponent < 1 || specularExponent > 128)
    return NS_ERROR_FAILURE;

  return nsSVGFESpecularLightingElementBase::Filter(instance, aSources, aTarget, rect);
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
      NS_MIN(PRUint32(specularNH * NS_GET_B(color)), 255U);
    targetData[GFX_ARGB32_OFFSET_G] =
      NS_MIN(PRUint32(specularNH * NS_GET_G(color)), 255U);
    targetData[GFX_ARGB32_OFFSET_R] =
      NS_MIN(PRUint32(specularNH * NS_GET_R(color)), 255U);

    targetData[GFX_ARGB32_OFFSET_A] =
      NS_MAX(targetData[GFX_ARGB32_OFFSET_B],
             NS_MAX(targetData[GFX_ARGB32_OFFSET_G],
                    targetData[GFX_ARGB32_OFFSET_R]));
  } else {
    targetData[GFX_ARGB32_OFFSET_B] = 0;
    targetData[GFX_ARGB32_OFFSET_G] = 0;
    targetData[GFX_ARGB32_OFFSET_R] = 0;
    targetData[GFX_ARGB32_OFFSET_A] = 255;
  }
}

//---------------------Image------------------------

typedef nsSVGFE nsSVGFEImageElementBase;

class nsSVGFEImageElement : public nsSVGFEImageElementBase,
                            public nsIDOMSVGFEImageElement,
                            public nsIDOMSVGURIReference,
                            public nsImageLoadingContent
{
protected:
  friend nsresult NS_NewSVGFEImageElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGFEImageElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsSVGFEImageElement();

public:
  virtual PRBool SubregionIsUnionOfRegions() { return PR_FALSE; }

  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEImageElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);

  NS_DECL_NSIDOMSVGFEIMAGEELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEImageElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEImageElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEImageElementBase::)

  // nsIContent
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString* aValue, PRBool aNotify);
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual nsEventStates IntrinsicState() const;

  // imgIDecoderObserver
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver
  NS_IMETHOD FrameChanged(imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);
  // imgIContainerObserver
  NS_IMETHOD OnStartContainer(imgIRequest *aRequest,
                              imgIContainer *aContainer);

  void MaybeLoadSVGImage();

  virtual nsXPCClassInfo* GetClassInfo();
private:
  // Invalidate users of the filter containing this element.
  void Invalidate();

  nsresult LoadSVGImage(PRBool aForce, PRBool aNotify);

protected:
  virtual PRBool OperatesOnSRGB(nsSVGFilterInstance*,
                                PRInt32, Image*) { return PR_TRUE; }

  virtual SVGAnimatedPreserveAspectRatio *GetPreserveAspectRatio();
  virtual StringAttributesInfo GetStringInfo();
  virtual void DidAnimateString(PRUint8 aAttrEnum);

  enum { RESULT, HREF };
  nsSVGString mStringAttributes[2];
  static StringInfo sStringInfo[2];

  SVGAnimatedPreserveAspectRatio mPreserveAspectRatio;
};

nsSVGElement::StringInfo nsSVGFEImageElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEImage)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEImageElement,nsSVGFEImageElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEImageElement,nsSVGFEImageElementBase)

DOMCI_NODE_DATA(SVGFEImageElement, nsSVGFEImageElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEImageElement)
  NS_NODE_INTERFACE_TABLE8(nsSVGFEImageElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEImageElement, nsIDOMSVGURIReference,
                           imgIDecoderObserver, nsIImageLoadingContent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEImageElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEImageElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFEImageElement::nsSVGFEImageElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGFEImageElementBase(aNodeInfo)
{
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

nsSVGFEImageElement::~nsSVGFEImageElement()
{
  DestroyImageLoadingContent();
}

//----------------------------------------------------------------------

nsresult
nsSVGFEImageElement::LoadSVGImage(PRBool aForce, PRBool aNotify)
{
  // resolve href attribute
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();

  nsAutoString href;
  mStringAttributes[HREF].GetAnimValue(href, this);
  href.Trim(" \t\n\r");

  if (baseURI && !href.IsEmpty())
    NS_MakeAbsoluteURI(href, href, baseURI);

  // Make sure we don't get in a recursive death-spiral
  nsIDocument* doc = GetOwnerDoc();
  if (doc) {
    nsCOMPtr<nsIURI> hrefAsURI;
    if (NS_SUCCEEDED(StringToURI(href, doc, getter_AddRefs(hrefAsURI)))) {
      PRBool isEqual;
      if (NS_SUCCEEDED(hrefAsURI->Equals(baseURI, &isEqual)) && isEqual) {
        // Image URI matches our URI exactly! Bail out.
        return NS_OK;
      }
    }
  }

  return LoadImage(href, aForce, aNotify);
}

//----------------------------------------------------------------------
// nsIContent methods:

NS_IMETHODIMP_(PRBool)
nsSVGFEImageElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGraphicsMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGFEImageElementBase::IsAttributeMapped(name);
}

nsresult
nsSVGFEImageElement::AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                  const nsAString* aValue, PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_XLink && aName == nsGkAtoms::href) {
    if (aValue) {
      LoadSVGImage(PR_TRUE, aNotify);
    } else {
      CancelImageRequests(aNotify);
    }
  }

  return nsSVGFEImageElementBase::AfterSetAttr(aNamespaceID, aName,
                                               aValue, aNotify);
}

void
nsSVGFEImageElement::MaybeLoadSVGImage()
{
  if (mStringAttributes[HREF].IsExplicitlySet() &&
      (NS_FAILED(LoadSVGImage(PR_FALSE, PR_TRUE)) ||
       !LoadingEnabled())) {
    CancelImageRequests(PR_TRUE);
  }
}

nsresult
nsSVGFEImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsSVGFEImageElementBase::BindToTree(aDocument, aParent,
                                                    aBindingParent,
                                                    aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mStringAttributes[HREF].IsExplicitlySet()) {
    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
    nsContentUtils::AddScriptRunner(
      NS_NewRunnableMethod(this, &nsSVGFEImageElement::MaybeLoadSVGImage));
  }

  return rv;
}

nsEventStates
nsSVGFEImageElement::IntrinsicState() const
{
  return nsSVGFEImageElementBase::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEImageElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGFEImageElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGFEImageElement methods

nsresult
nsSVGFEImageElement::Filter(nsSVGFilterInstance *instance,
                            const nsTArray<const Image*>& aSources,
                            const Image* aTarget,
                            const nsIntRect& rect)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;

  nsCOMPtr<imgIRequest> currentRequest;
  GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
             getter_AddRefs(currentRequest));

  nsCOMPtr<imgIContainer> imageContainer;
  if (currentRequest)
    currentRequest->GetImage(getter_AddRefs(imageContainer));

  nsRefPtr<gfxASurface> currentFrame;
  if (imageContainer)
    imageContainer->GetFrame(imgIContainer::FRAME_CURRENT,
                             imgIContainer::FLAG_SYNC_DECODE,
                             getter_AddRefs(currentFrame));

  // We need to wrap the surface in a pattern to have somewhere to set the
  // graphics filter.
  nsRefPtr<gfxPattern> thebesPattern;
  if (currentFrame)
    thebesPattern = new gfxPattern(currentFrame);

  if (thebesPattern) {
    thebesPattern->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(frame));

    PRInt32 nativeWidth, nativeHeight;
    imageContainer->GetWidth(&nativeWidth);
    imageContainer->GetHeight(&nativeHeight);

    const gfxRect& filterSubregion = aTarget->mFilterPrimitiveSubregion;

    gfxMatrix viewBoxTM =
      nsSVGUtils::GetViewBoxTransform(this,
                                      filterSubregion.Width(), filterSubregion.Height(),
                                      0,0, nativeWidth, nativeHeight,
                                      mPreserveAspectRatio);

    gfxMatrix xyTM = gfxMatrix().Translate(gfxPoint(filterSubregion.X(), filterSubregion.Y()));

    gfxMatrix TM = viewBoxTM * xyTM;
    
    nsRefPtr<gfxContext> ctx = new gfxContext(aTarget->mImage);
    nsSVGUtils::CompositePatternMatrix(ctx, thebesPattern, TM, nativeWidth, nativeHeight, 1.0);
  }

  return NS_OK;
}

nsIntRect
nsSVGFEImageElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  // XXX can do better here ... we could check what we know of the source
  // image bounds and compute an accurate bounding box for the filter
  // primitive result.
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

SVGAnimatedPreserveAspectRatio *
nsSVGFEImageElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringAttributesInfo
nsSVGFEImageElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

void
nsSVGFEImageElement::DidAnimateString(PRUint8 aAttrEnum)
{
  if (aAttrEnum == HREF) {
    LoadSVGImage(PR_TRUE, PR_FALSE);
    return;
  }

  nsSVGFEImageElementBase::DidAnimateString(aAttrEnum);
}

//----------------------------------------------------------------------
// imgIDecoderObserver methods

NS_IMETHODIMP
nsSVGFEImageElement::OnStopDecode(imgIRequest *aRequest,
                                  nsresult status,
                                  const PRUnichar *statusArg)
{
  nsresult rv =
    nsImageLoadingContent::OnStopDecode(aRequest, status, statusArg);
  Invalidate();
  return rv;
}

NS_IMETHODIMP
nsSVGFEImageElement::FrameChanged(imgIContainer *aContainer,
                                  const nsIntRect *aDirtyRect)
{
  nsresult rv =
    nsImageLoadingContent::FrameChanged(aContainer, aDirtyRect);
  Invalidate();
  return rv;
}

NS_IMETHODIMP
nsSVGFEImageElement::OnStartContainer(imgIRequest *aRequest,
                                      imgIContainer *aContainer)
{
  nsresult rv =
    nsImageLoadingContent::OnStartContainer(aRequest, aContainer);

  // Request a decode
  NS_ABORT_IF_FALSE(aContainer, "who sent the notification then?");
  aContainer->RequestDecode();

  // We have a size - invalidate
  Invalidate();
  return rv;
}

//----------------------------------------------------------------------
// helper methods

void
nsSVGFEImageElement::Invalidate()
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  if (filter) {
    static_cast<nsSVGFilterElement*>(GetParent())->Invalidate();
  }
}

//---------------------Displacement------------------------

typedef nsSVGFE nsSVGFEDisplacementMapElementBase;

class nsSVGFEDisplacementMapElement : public nsSVGFEDisplacementMapElementBase,
                                      public nsIDOMSVGFEDisplacementMapElement
{
protected:
  friend nsresult NS_NewSVGFEDisplacementMapElement(nsIContent **aResult,
                                                    already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGFEDisplacementMapElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsSVGFEDisplacementMapElementBase(aNodeInfo) {}

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEDisplacementMapElementBase::)

  virtual nsresult Filter(nsSVGFilterInstance* aInstance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect);
  virtual nsSVGString& GetResultImageName() { return mStringAttributes[RESULT]; }
  virtual void GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources);
  virtual nsIntRect ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance);
  virtual void ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance);
  virtual nsIntRect ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
          const nsSVGFilterInstance& aInstance);

  // DisplacementMap
  NS_DECL_NSIDOMSVGFEDISPLACEMENTMAPELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEDisplacementMapElementBase::)

  NS_FORWARD_NSIDOMNODE(nsSVGFEDisplacementMapElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEDisplacementMapElementBase::)

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();
protected:
  virtual PRBool OperatesOnSRGB(nsSVGFilterInstance* aInstance,
                                PRInt32 aInput, Image* aImage) {
    switch (aInput) {
    case 0:
      return aImage->mColorModel.mColorSpace == ColorModel::SRGB;
    case 1:
      return nsSVGFEDisplacementMapElementBase::OperatesOnSRGB(aInstance,
                                                               aInput, aImage);
    default:
      NS_ERROR("Will not give correct output color model");
      return PR_FALSE;
    }
  }
  virtual PRBool OperatesOnPremultipledAlpha(PRInt32 aInput) {
    return !(aInput == 1);
  }

  virtual NumberAttributesInfo GetNumberInfo();
  virtual EnumAttributesInfo GetEnumInfo();
  virtual StringAttributesInfo GetStringInfo();

  enum { SCALE };
  nsSVGNumber2 mNumberAttributes[1];
  static NumberInfo sNumberInfo[1];

  enum { CHANNEL_X, CHANNEL_Y };
  nsSVGEnum mEnumAttributes[2];
  static nsSVGEnumMapping sChannelMap[];
  static EnumInfo sEnumInfo[2];

  enum { RESULT, IN1, IN2 };
  nsSVGString mStringAttributes[3];
  static StringInfo sStringInfo[3];
};

nsSVGElement::NumberInfo nsSVGFEDisplacementMapElement::sNumberInfo[1] =
{
  { &nsGkAtoms::scale, 0, PR_FALSE },
};

nsSVGEnumMapping nsSVGFEDisplacementMapElement::sChannelMap[] = {
  {&nsGkAtoms::R, nsSVGFEDisplacementMapElement::SVG_CHANNEL_R},
  {&nsGkAtoms::G, nsSVGFEDisplacementMapElement::SVG_CHANNEL_G},
  {&nsGkAtoms::B, nsSVGFEDisplacementMapElement::SVG_CHANNEL_B},
  {&nsGkAtoms::A, nsSVGFEDisplacementMapElement::SVG_CHANNEL_A},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGFEDisplacementMapElement::sEnumInfo[2] =
{
  { &nsGkAtoms::xChannelSelector,
    sChannelMap,
    nsSVGFEDisplacementMapElement::SVG_CHANNEL_A
  },
  { &nsGkAtoms::yChannelSelector,
    sChannelMap,
    nsSVGFEDisplacementMapElement::SVG_CHANNEL_A
  }
};

nsSVGElement::StringInfo nsSVGFEDisplacementMapElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in, kNameSpaceID_None, PR_TRUE },
  { &nsGkAtoms::in2, kNameSpaceID_None, PR_TRUE }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEDisplacementMap)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEDisplacementMapElement,nsSVGFEDisplacementMapElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEDisplacementMapElement,nsSVGFEDisplacementMapElementBase)

DOMCI_NODE_DATA(SVGFEDisplacementMapElement, nsSVGFEDisplacementMapElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEDisplacementMapElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEDisplacementMapElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEDisplacementMapElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEDisplacementMapElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEDisplacementMapElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEDisplacementMapElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEDisplacementMapElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedString in2; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetIn2(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN2].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber scale; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetScale(nsIDOMSVGAnimatedNumber * *aScale)
{
  return mNumberAttributes[SCALE].ToDOMAnimatedNumber(aScale, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration xChannelSelector; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetXChannelSelector(nsIDOMSVGAnimatedEnumeration * *aChannel)
{
  return mEnumAttributes[CHANNEL_X].ToDOMAnimatedEnum(aChannel, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration yChannelSelector; */
NS_IMETHODIMP nsSVGFEDisplacementMapElement::GetYChannelSelector(nsIDOMSVGAnimatedEnumeration * *aChannel)
{
  return mEnumAttributes[CHANNEL_Y].ToDOMAnimatedEnum(aChannel, this);
}

nsresult
nsSVGFEDisplacementMapElement::Filter(nsSVGFilterInstance *instance,
                                      const nsTArray<const Image*>& aSources,
                                      const Image* aTarget,
                                      const nsIntRect& rect)
{
  float scale = instance->GetPrimitiveNumber(nsSVGUtils::XY,
                                             &mNumberAttributes[SCALE]);
  if (scale == 0.0f) {
    CopyRect(aTarget, aSources[0], rect);
    return NS_OK;
  }

  PRInt32 width = instance->GetSurfaceWidth();
  PRInt32 height = instance->GetSurfaceHeight();

  PRUint8* sourceData = aSources[0]->mImage->Data();
  PRUint8* displacementData = aSources[1]->mImage->Data();
  PRUint8* targetData = aTarget->mImage->Data();
  PRUint32 stride = aTarget->mImage->Stride();

  static const PRUint16 channelMap[5] = {
                             0,
                             GFX_ARGB32_OFFSET_R,
                             GFX_ARGB32_OFFSET_G,
                             GFX_ARGB32_OFFSET_B,
                             GFX_ARGB32_OFFSET_A };
  PRUint16 xChannel = channelMap[mEnumAttributes[CHANNEL_X].GetAnimValue()];
  PRUint16 yChannel = channelMap[mEnumAttributes[CHANNEL_Y].GetAnimValue()];

  double scaleOver255 = scale / 255.0;
  double scaleAdjustment = 0.5 - 0.5 * scale;

  for (PRInt32 y = rect.y; y < rect.YMost(); y++) {
    for (PRInt32 x = rect.x; x < rect.XMost(); x++) {
      PRUint32 targIndex = y * stride + 4 * x;
      // At some point we might want to replace this with a bilinear sample.
      PRInt32 sourceX = x +
        NSToIntFloor(scaleOver255 * displacementData[targIndex + xChannel] +
                scaleAdjustment);
      PRInt32 sourceY = y +
        NSToIntFloor(scaleOver255 * displacementData[targIndex + yChannel] +
                scaleAdjustment);
      if (sourceX < 0 || sourceX >= width ||
          sourceY < 0 || sourceY >= height) {
        *(PRUint32*)(targetData + targIndex) = 0;
      } else {
        *(PRUint32*)(targetData + targIndex) =
          *(PRUint32*)(sourceData + sourceY * stride + 4 * sourceX);
      }
    }
  }
  return NS_OK;
}

void
nsSVGFEDisplacementMapElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

nsIntRect
nsSVGFEDisplacementMapElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
          const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement, and then return mIn1's bounds
  // adjusted for the maximum displacement
  return GetMaxRect();
}

void
nsSVGFEDisplacementMapElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  // in2 contains the displacements, which we read for each target pixel
  aSourceBBoxes[1] = aTargetBBox;
  // XXX to figure out which parts of 'in' we might read, we could
  // do some analysis of 'scale' to figure out the maximum displacement.
  // For now, just leave aSourceBBoxes[0] alone, i.e. assume we use its
  // entire output bounding box.
  // If we change this, we need to change coordinate assumptions above
}

nsIntRect
nsSVGFEDisplacementMapElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                                 const nsSVGFilterInstance& aInstance)
{
  // XXX we could do something clever here involving analysis of 'scale'
  // to figure out the maximum displacement
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEDisplacementMapElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              NS_ARRAY_LENGTH(sNumberInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEDisplacementMapElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEDisplacementMapElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}
