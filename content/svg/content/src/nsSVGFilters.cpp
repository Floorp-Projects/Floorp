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
#include "nsSVGAnimatedNumber.h"
#include "nsSVGAnimatedString.h"
#include "nsSVGLength.h"
#include "nsSVGNumber.h"
#include "nsSVGAtoms.h"
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
#include "nsSVGNumber.h"
#include "nsSVGAnimatedNumber.h"
#include "nsISVGValueUtils.h"
#include "nsSVGFilters.h"
#include "nsSVGUtils.h"

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
    rv = AddMappedSVGValue(nsSVGAtoms::result, mResult);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGFE::WillModifySVGObservable(nsISVGValue* observable,
                                 nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  return nsSVGFEBase::WillModifySVGObservable(observable, aModType);
}

NS_IMETHODIMP
nsSVGFE::DidModifySVGObservable (nsISVGValue* observable,
                                 nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->EndBatchUpdate();
  return nsSVGFEBase::DidModifySVGObservable(observable, aModType);
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

void
nsSVGFE::DidChangeLength(PRUint8 aAttrEnum, PRBool aDoSetAttr)
{
  nsSVGFEBase::DidChangeLength(aAttrEnum, aDoSetAttr);

  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (value) {
    value->BeginBatchUpdate();
    value->EndBatchUpdate();
  }
}

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
protected:
  friend nsresult NS_NewSVGFEGaussianBlurElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
  nsSVGFEGaussianBlurElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGFEGaussianBlurElement();
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

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEGaussianBlurElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEGaussianBlurElementBase::)

  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);

protected:

  nsCOMPtr<nsIDOMSVGAnimatedNumber> mStdDeviationX;
  nsCOMPtr<nsIDOMSVGAnimatedNumber> mStdDeviationY;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
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

nsSVGFEGaussianBlurElement::~nsSVGFEGaussianBlurElement()
{
  NS_REMOVE_SVGVALUE_OBSERVER(mStdDeviationX);
  NS_REMOVE_SVGVALUE_OBSERVER(mStdDeviationY);
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
    rv = AddMappedSVGValue(nsSVGAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: stdDeviationX , #IMPLIED attrib: mStdDeviation
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mStdDeviationX), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: stdDeviationY , #IMPLIED attrib: mStdDeviation
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mStdDeviationY), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // add observers -------------------------- :
  NS_ADD_SVGVALUE_OBSERVER(mStdDeviationX);
  NS_ADD_SVGVALUE_OBSERVER(mStdDeviationY);

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEGaussianBlurElement)


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
  *aX = mStdDeviationX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber stdDeviationY; */
NS_IMETHODIMP nsSVGFEGaussianBlurElement::GetStdDeviationY(nsIDOMSVGAnimatedNumber * *aY)
{
  *aY = mStdDeviationY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  mStdDeviationX->SetBaseVal(stdDeviationX);
  mStdDeviationY->SetBaseVal(stdDeviationY);
  return NS_OK;
}

nsresult
nsSVGFEGaussianBlurElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                    nsIAtom* aPrefix, const nsAString& aValue,
                                    PRBool aNotify)
{
  nsresult rv = nsSVGFEGaussianBlurElementBase::SetAttr(aNameSpaceID, aName, aPrefix,
                                                        aValue, aNotify);

  if (aName == nsSVGAtoms::stdDeviation && aNameSpaceID == kNameSpaceID_None) {
    float stdX = 0.0f, stdY = 0.0f;
    char *str;
    str = ToNewCString(aValue);
    int num = sscanf(str, "%f %f\n", &stdX, &stdY);
    if (num == 1)
      stdY = stdX;
    mStdDeviationX->SetBaseVal(stdX);
    mStdDeviationY->SetBaseVal(stdY);
    nsMemory::Free(str);
  }

  return rv;
}

static void
boxBlurH(PRUint8 *aInput, PRUint8 *aOutput,
         PRInt32 aStride, nsRect aRegion,
         PRUint32 leftLobe, PRUint32 rightLobe)
{
  PRInt32 boxSize = leftLobe + rightLobe + 1;

  for (PRInt32 y = aRegion.y; y < aRegion.y + aRegion.height; y++) {
    PRUint32 sums[4] = {0, 0, 0, 0};
    for (PRInt32 i=0; i < boxSize; i++) {
      PRInt32 pos = aRegion.x - leftLobe + i;
      pos = PR_MAX(pos, aRegion.x);
      pos = PR_MIN(pos, aRegion.x + aRegion.width - 1);
      sums[0] += aInput[aStride*y + 4*pos    ];
      sums[1] += aInput[aStride*y + 4*pos + 1];
      sums[2] += aInput[aStride*y + 4*pos + 2];
      sums[3] += aInput[aStride*y + 4*pos + 3];
    }
    for (PRInt32 x = aRegion.x; x < aRegion.x + aRegion.width; x++) {
      PRInt32 tmp = x - leftLobe;
      PRInt32 last = PR_MAX(tmp, aRegion.x);
      PRInt32 next = PR_MIN(tmp + boxSize, aRegion.x + aRegion.width - 1);

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

static void
boxBlurV(PRUint8 *aInput, PRUint8 *aOutput,
         PRInt32 aStride, nsRect aRegion,
         unsigned topLobe, unsigned bottomLobe)
{
  PRInt32 boxSize = topLobe + bottomLobe + 1;

  for (PRInt32 x = aRegion.x; x < aRegion.x + aRegion.width; x++) {
    PRUint32 sums[4] = {0, 0, 0, 0};
    for (PRInt32 i=0; i < boxSize; i++) {
      PRInt32 pos = aRegion.y - topLobe + i;
      pos = PR_MAX(pos, aRegion.y);
      pos = PR_MIN(pos, aRegion.y + aRegion.height - 1);
      sums[0] += aInput[aStride*pos + 4*x    ];
      sums[1] += aInput[aStride*pos + 4*x + 1];
      sums[2] += aInput[aStride*pos + 4*x + 2];
      sums[3] += aInput[aStride*pos + 4*x + 3];
    }
    for (PRInt32 y = aRegion.y; y < aRegion.y + aRegion.height; y++) {
      PRInt32 tmp = y - topLobe;
      PRInt32 last = PR_MAX(tmp, aRegion.y);
      PRInt32 next = PR_MIN(tmp + boxSize, aRegion.y + aRegion.height - 1);

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

static nsresult
gaussianBlur(PRUint8 *aInput, PRUint8 *aOutput,
             PRUint32 aLength, PRInt32 aStride, nsRect aRegion,
             float aStdX, float aStdY)
{
  if (aStdX < 0 || aStdY < 0) {
    return NS_ERROR_FAILURE;
  }

  if (aStdX == 0 || aStdY == 0) {
    memset(aOutput, 0, aLength);
    return NS_OK;
  }

  PRUint32 dX, dY;
  dX = (PRUint32) floor(aStdX * 3*sqrt(2*M_PI)/4 + 0.5);
  dY = (PRUint32) floor(aStdY * 3*sqrt(2*M_PI)/4 + 0.5);

  PRUint8 *tmp = new PRUint8[aLength];

  if (dX & 1) {
    // odd
    boxBlurH(aInput, tmp,  aStride, aRegion, dX/2, dX/2);
    boxBlurH(tmp, aOutput, aStride, aRegion, dX/2, dX/2);
    boxBlurH(aOutput, tmp, aStride, aRegion, dX/2, dX/2);
  } else {
    // even
    if (dX == 0) {
      memcpy(tmp, aInput, aLength);
    } else {
      boxBlurH(aInput, tmp,  aStride, aRegion, dX/2,     dX/2 - 1);
      boxBlurH(tmp, aOutput, aStride, aRegion, dX/2 - 1, dX/2);
      boxBlurH(aOutput, tmp, aStride, aRegion, dX/2,     dX/2);
    }
  }

  if (dY & 1) {
    // odd
    boxBlurV(tmp, aOutput, aStride, aRegion, dY/2, dY/2);
    boxBlurV(aOutput, tmp, aStride, aRegion, dY/2, dY/2);
    boxBlurV(tmp, aOutput, aStride, aRegion, dY/2, dY/2);
  } else {
    // even
    if (dY == 0) {
      memcpy(aOutput, tmp, aLength);
    } else {
      boxBlurV(tmp, aOutput, aStride, aRegion, dY/2,     dY/2 - 1);
      boxBlurV(aOutput, tmp, aStride, aRegion, dY/2 - 1, dY/2);
      boxBlurV(tmp, aOutput, aStride, aRegion, dY/2,     dY/2);
    }
  }

  delete [] tmp;
  return NS_OK;
}

static void
fixupTarget(PRUint8 *aSource, PRUint8 *aTarget,
            PRUint32 aWidth, PRUint32 aHeight,
            PRInt32 aStride, nsRect aRegion)
{
  // top
  if (aRegion.y > 0)
    for (PRInt32 y = 0; y < aRegion.y; y++)
      if (aSource)
        memcpy(aTarget + y * aStride, aSource + y * aStride, aStride);
      else
        memset(aTarget + y * aStride, 0, aStride);

  // bottom
  if (aRegion.y + aRegion.height < aHeight)
    for (PRInt32 y = aRegion.y + aRegion.height; y < aHeight; y++)
      if (aSource)
        memcpy(aTarget + y * aStride, aSource + y * aStride, aStride);
      else
        memset(aTarget + y * aStride, 0, aStride);

  // left
  if (aRegion.x > 0)
    for (PRInt32 y = aRegion.y; y < aRegion.y + aRegion.height; y++)
      memcpy(aTarget + y * aStride, aSource + y * aStride, 4 * aRegion.x);

  // right
  if (aRegion.x + aRegion.width < aWidth)
    for (PRInt32 y = aRegion.y; y < aRegion.y + aRegion.height; y++)
      if (aSource)
        memcpy(aTarget + y * aStride + 4 * (aRegion.x + aRegion.width),
               aSource + y * aStride + 4 * (aRegion.x + aRegion.width),
               4 * (aWidth - aRegion.x - aRegion.width));
      else
        memset(aTarget + y * aStride + 4 * (aRegion.x + aRegion.width),
               0,
               4 * (aWidth - aRegion.x - aRegion.width));
}

NS_IMETHODIMP
nsSVGFEGaussianBlurElement::Filter(nsSVGFilterInstance *instance)
{
  nsRect rect;
  nsIDOMSVGFilterPrimitiveStandardAttributes *attrib =
    (nsIDOMSVGFilterPrimitiveStandardAttributes *)this;

  nsRect defaultRect;
  nsAutoString input, result;
  mIn1->GetAnimVal(input);
  mResult->GetAnimVal(result);
  instance->LookupRegion(input, &defaultRect);

  instance->GetFilterSubregion(this, defaultRect, &rect);

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER GAUSS rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  nsCOMPtr<nsISVGRendererSurface> sourceImage, targetImage;
  instance->LookupImage(input, getter_AddRefs(sourceImage));
  if (!sourceImage)
    return NS_ERROR_FAILURE;

  instance->GetImage(getter_AddRefs(targetImage));
  if (!targetImage)
    return NS_ERROR_FAILURE;

  PRUint8 *sourceData, *targetData;
  PRInt32 stride;
  PRUint32 width, height, length;
  sourceImage->Lock();
  targetImage->Lock();
  sourceImage->GetData(&sourceData, &length, &stride);
  targetImage->GetData(&targetData, &length, &stride);

  sourceImage->GetWidth(&width);
  sourceImage->GetHeight(&height);

  float stdX, stdY;
  nsSVGLength2 val;

  mStdDeviationX->GetAnimVal(&stdX);
  val.Init(nsSVGUtils::X, 0xff, stdX, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  stdX = instance->GetPrimitiveLength(&val);

  mStdDeviationY->GetAnimVal(&stdY);
  val.Init(nsSVGUtils::Y, 0xff, stdY, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  stdY = instance->GetPrimitiveLength(&val);

  nsresult rv = gaussianBlur(sourceData, targetData, length, 
                             stride, rect, stdX, stdY);
  NS_ENSURE_SUCCESS(rv, rv);
  fixupTarget(sourceData, targetData, width, height, stride, rect);

  sourceImage->Unlock();
  targetImage->Unlock();

  instance->DefineImage(result, targetImage);
  instance->DefineRegion(result, rect);

  return NS_OK;
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

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEComponentTransferElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEComponentTransferElementBase::)

  // nsIContent
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);

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
    rv = AddMappedSVGValue(nsSVGAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEComponentTransferElement)

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
  nsRect rect;
  nsIDOMSVGFilterPrimitiveStandardAttributes *attrib =
    (nsIDOMSVGFilterPrimitiveStandardAttributes *)this;

  nsRect defaultRect;
  nsAutoString input, result;
  mIn1->GetAnimVal(input);
  mResult->GetAnimVal(result);
  instance->LookupRegion(input, &defaultRect);

  instance->GetFilterSubregion(this, defaultRect, &rect);

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER COMPONENT rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  nsCOMPtr<nsISVGRendererSurface> sourceImage, targetImage;
  instance->LookupImage(input, getter_AddRefs(sourceImage));
  if (!sourceImage)
    return NS_ERROR_FAILURE;

  instance->GetImage(getter_AddRefs(targetImage));
  if (!targetImage)
    return NS_ERROR_FAILURE;

  PRUint8 *sourceData, *targetData;
  PRInt32 stride;
  PRUint32 width, height, length;
  sourceImage->Lock();
  targetImage->Lock();
  sourceImage->GetData(&sourceData, &length, &stride);
  targetImage->GetData(&targetData, &length, &stride);

  sourceImage->GetWidth(&width);
  sourceImage->GetHeight(&height);

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

  for (PRInt32 y = rect.y; y < rect.y + rect.height; y++)
    for (PRInt32 x = rect.x; x < rect.x + rect.width; x++) {
      PRUint32 r, g, b, a;
      a = sourceData[y * stride + 4 * x + 3];
      if (a) {
        b = tableB[(255 * (PRUint32)sourceData[y * stride + 4 * x])     / a];
        g = tableG[(255 * (PRUint32)sourceData[y * stride + 4 * x + 1]) / a];
        r = tableR[(255 * (PRUint32)sourceData[y * stride + 4 * x + 2]) / a];
      } else 
        b = g = r = 0;
      a = tableA[a];
      targetData[y * stride + 4 * x]     = (b * a) / 255;
      targetData[y * stride + 4 * x + 1] = (g * a) / 255;
      targetData[y * stride + 4 * x + 2] = (r * a) / 255;
      targetData[y * stride + 4 * x + 3] = a;
    }

  fixupTarget(sourceData, targetData, width, height, stride, rect);

  sourceImage->Unlock();
  targetImage->Unlock();

  instance->DefineImage(result, targetImage);
  instance->DefineRegion(result, rect);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEComponentTransferElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGFEComponentTransferElement::InsertChildAt(nsIContent* aKid,
                                               PRUint32 aIndex,
                                               PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEComponentTransferElementBase::InsertChildAt(aKid,
                                                                   aIndex,
                                                                   aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
}

nsresult
nsSVGFEComponentTransferElement::AppendChildTo(nsIContent* aKid,
                                               PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEComponentTransferElementBase::AppendChildTo(aKid,
                                                                   aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
}

nsresult
nsSVGFEComponentTransferElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEComponentTransferElementBase::RemoveChildAt(aIndex,
                                                                   aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
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

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGCOMPONENTTRANSFERFUNCTIONELEMENT

protected:

  // nsIDOMSVGComponentTransferFunctionElement properties:
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mType;
  nsCOMPtr<nsIDOMSVGAnimatedNumberList>  mTableValues;
  nsCOMPtr<nsIDOMSVGAnimatedNumber>      mSlope;
  nsCOMPtr<nsIDOMSVGAnimatedNumber>      mIntercept;
  nsCOMPtr<nsIDOMSVGAnimatedNumber>      mAmplitude;
  nsCOMPtr<nsIDOMSVGAnimatedNumber>      mExponent;
  nsCOMPtr<nsIDOMSVGAnimatedNumber>      mOffset;
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
    {&nsSVGAtoms::identity,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY},
    {&nsSVGAtoms::table,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_TABLE},
    {&nsSVGAtoms::discrete,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE},
    {&nsSVGAtoms::linear,
     nsIDOMSVGComponentTransferFunctionElement::SVG_FECOMPONENTTRANSFER_TYPE_LINEAR},
    {&nsSVGAtoms::gamma,
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
    rv = AddMappedSVGValue(nsSVGAtoms::type, mType);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: tableValues, #IMPLIED attrib: tableValues
  {
    nsCOMPtr<nsIDOMSVGNumberList> values;
    rv = NS_NewSVGNumberList(getter_AddRefs(values));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedNumberList(getter_AddRefs(mTableValues), values);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::tableValues, mTableValues);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: slope ,  #IMPLIED attrib: slope
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mSlope), 1.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::slope, mSlope);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: intercept ,  #IMPLIED attrib: intercept
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mIntercept), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::intercept, mIntercept);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: amplitude ,  #IMPLIED attrib: amplitude
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mAmplitude), 1.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::amplitude, mAmplitude);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: exponent ,  #IMPLIED attrib: exponent
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mExponent), 1.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::exponent, mExponent);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: offset ,  #IMPLIED attrib: offset
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mOffset), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::offset, mOffset);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGComponentTransferFunctionElement::WillModifySVGObservable(nsISVGValue* observable,
                                 nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFEComponentTransferElement> element;
  nsCOMPtr<nsIDOMSVGFilterElement> filter;
  element = do_QueryInterface(GetParent());
  if (GetParent())
    filter = do_QueryInterface(GetParent()->GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(filter);

  if (element && filter && value)
    value->BeginBatchUpdate();
  return nsSVGComponentTransferFunctionElementBase::WillModifySVGObservable(observable, aModType);
}

NS_IMETHODIMP
nsSVGComponentTransferFunctionElement::DidModifySVGObservable (nsISVGValue* observable,
                                 nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFEComponentTransferElement> element;
  nsCOMPtr<nsIDOMSVGFilterElement> filter;
  element = do_QueryInterface(GetParent());
  if (GetParent())
    filter = do_QueryInterface(GetParent()->GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(filter);

  if (element && filter && value)
    value->EndBatchUpdate();
  return nsSVGComponentTransferFunctionElementBase::DidModifySVGObservable(observable, aModType);
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
  *aSlope = mSlope;
  NS_IF_ADDREF(*aSlope);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber intercept; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetIntercept(nsIDOMSVGAnimatedNumber * *aIntercept)
{
  *aIntercept = mIntercept;
  NS_IF_ADDREF(*aIntercept);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber amplitude; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetAmplitude(nsIDOMSVGAnimatedNumber * *aAmplitude)
{
  *aAmplitude = mAmplitude;
  NS_IF_ADDREF(*aAmplitude);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber exponent; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetExponent(nsIDOMSVGAnimatedNumber * *aExponent)
{
  *aExponent = mExponent;
  NS_IF_ADDREF(*aExponent);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber offset; */
NS_IMETHODIMP nsSVGComponentTransferFunctionElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  *aOffset = mOffset;
  NS_IF_ADDREF(*aOffset);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGComponentTransferFunctionElement::GenerateLookupTable(PRUint8 *aTable)
{
  PRUint16 type;
  mType->GetAnimVal(&type);

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
    float slope, intercept;
    mSlope->GetAnimVal(&slope);
    mIntercept->GetAnimVal(&intercept);
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
    float amplitude, exponent, offset;
    mAmplitude->GetAnimVal(&amplitude);
    mExponent->GetAnimVal(&exponent);
    mOffset->GetAnimVal(&offset);
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
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

protected:
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
NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEFuncRElement)


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
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

protected:
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
NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEFuncGElement)


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
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

protected:
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
NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEFuncBElement)


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
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGComponentTransferFunctionElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGComponentTransferFunctionElement::)

protected:
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
NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEFuncAElement)

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
  virtual ~nsSVGFEMergeElement();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // FE Base
  NS_FORWARD_NSIDOMSVGFILTERPRIMITIVESTANDARDATTRIBUTES(nsSVGFEMergeElementBase::)

  // nsISVGFilter
  NS_IMETHOD Filter(nsSVGFilterInstance *instance);
  NS_IMETHOD GetRequirements(PRUint32 *aRequirements);

  // Gaussian
  NS_DECL_NSIDOMSVGFEMERGEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeElementBase::)

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEMergeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeElementBase::)

  // nsIContent
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);

protected:

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

nsSVGFEMergeElement::~nsSVGFEMergeElement()
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEMergeElement)

NS_IMETHODIMP
nsSVGFEMergeElement::Filter(nsSVGFilterInstance *instance)
{
  nsRect rect;
  nsIDOMSVGFilterPrimitiveStandardAttributes *attrib =
    (nsIDOMSVGFilterPrimitiveStandardAttributes *)this;

  nsCOMPtr<nsISVGRendererSurface> sourceImage, targetImage;
  instance->GetImage(getter_AddRefs(targetImage));
  if (!targetImage)
    return NS_ERROR_FAILURE;

  PRUint8 *sourceData, *targetData;
  PRInt32 stride;
  PRUint32 width, height, length;
  targetImage->Lock();
  targetImage->GetData(&targetData, &length, &stride);
  targetImage->GetWidth(&width);
  targetImage->GetHeight(&height);
   
  PRUint32 count = GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child = GetChildAt(i);
    nsCOMPtr<nsIDOMSVGFEMergeNodeElement> node = do_QueryInterface(child);
    if (!node) 
      continue;
    nsCOMPtr<nsIDOMSVGAnimatedString> str;
    node->GetIn1(getter_AddRefs(str));
                 
    nsRect defaultRect;
    nsAutoString input;
    str->GetAnimVal(input);
    instance->LookupRegion(input, &defaultRect);

    instance->GetFilterSubregion(this, defaultRect, &rect);

#ifdef DEBUG_tor
    fprintf(stderr, "FILTER MERGE rect: %d,%d  %dx%d\n",
            rect.x, rect.y, rect.width, rect.height);
#endif

    instance->LookupImage(input, getter_AddRefs(sourceImage));
    if (!sourceImage) {
      targetImage->Unlock();
      return NS_ERROR_FAILURE;
    }

    sourceImage->Lock();
    sourceImage->GetData(&sourceData, &length, &stride);
    
#define BLEND(target, source, alpha) \
    target = PR_MIN(255, (target * (255 - alpha))/255 + source)

    for (PRInt32 y = rect.y; y < rect.y + rect.height; y++)
      for (PRInt32 x = rect.x; x < rect.x + rect.width; x++) {
        PRUint32 a = sourceData[y * stride + 4 * x + 3];
        BLEND(targetData[y * stride + 4 * x    ],
              sourceData[y * stride + 4 * x    ], a);
        BLEND(targetData[y * stride + 4 * x + 1],
              sourceData[y * stride + 4 * x + 1], a);
        BLEND(targetData[y * stride + 4 * x + 2],
              sourceData[y * stride + 4 * x + 2], a);
        BLEND(targetData[y * stride + 4 * x + 3],
              sourceData[y * stride + 4 * x + 3], a);
      }
#undef BLEND
    
    sourceImage->Unlock();
  }

  fixupTarget(nsnull, targetData, width, height, stride, rect);

  targetImage->Unlock();

  nsAutoString result;
  mResult->GetAnimVal(result);
  instance->DefineImage(result, targetImage);
  instance->DefineRegion(result, rect);

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

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGFEMergeElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEMergeElementBase::InsertChildAt(aKid, aIndex, aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
}

nsresult
nsSVGFEMergeElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEMergeElementBase::AppendChildTo(aKid, aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
}

nsresult
nsSVGFEMergeElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsIDOMSVGFilterElement> filter = do_QueryInterface(GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(GetParent());
  if (filter && value)
    value->BeginBatchUpdate();
  nsresult rv = nsSVGFEMergeElementBase::RemoveChildAt(aIndex, aNotify);
  if (filter && value)
    value->EndBatchUpdate();

  return rv;
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

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMSVGFEMERGENODEELEMENT

  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGFEMergeNodeElementBase::)

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEMergeNodeElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEMergeNodeElementBase::)

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
    rv = AddMappedSVGValue(nsSVGAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEMergeNodeElement)

NS_IMETHODIMP
nsSVGFEMergeNodeElement::WillModifySVGObservable(nsISVGValue* observable,
                                                 nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFEMergeElement> element;
  nsCOMPtr<nsIDOMSVGFilterElement> filter;
  element = do_QueryInterface(GetParent());
  if (GetParent())
    filter = do_QueryInterface(GetParent()->GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(filter);

  if (element && filter && value)
    value->BeginBatchUpdate();
  return nsSVGFEMergeNodeElementBase::WillModifySVGObservable(observable, aModType);
}

NS_IMETHODIMP
nsSVGFEMergeNodeElement::DidModifySVGObservable(nsISVGValue* observable,
                                                nsISVGValue::modificationType aModType)
{
  nsCOMPtr<nsIDOMSVGFEMergeElement> element;
  nsCOMPtr<nsIDOMSVGFilterElement> filter;
  element = do_QueryInterface(GetParent());
  if (GetParent())
    filter = do_QueryInterface(GetParent()->GetParent());
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(filter);

  if (element && filter && value)
    value->EndBatchUpdate();
  return nsSVGFEMergeNodeElementBase::DidModifySVGObservable(observable, aModType);
}

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
  virtual ~nsSVGFEOffsetElement();
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

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEOffsetElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEOffsetElementBase::)

protected:

  nsCOMPtr<nsIDOMSVGAnimatedNumber> mDx;
  nsCOMPtr<nsIDOMSVGAnimatedNumber> mDy;
  nsCOMPtr<nsIDOMSVGAnimatedString> mIn1;
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

nsSVGFEOffsetElement::~nsSVGFEOffsetElement()
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
    rv = AddMappedSVGValue(nsSVGAtoms::in, mIn1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: dx , #IMPLIED attrib: dx
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mDx), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::dx, mDx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: dy , #IMPLIED attrib: dy
  {
    rv = NS_NewSVGAnimatedNumber(getter_AddRefs(mDy), 0.0f);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::dy, mDy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEOffsetElement)


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
  *aDx = mDx;
  NS_IF_ADDREF(*aDx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumber dy; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetDy(nsIDOMSVGAnimatedNumber * *aDy)
{
  *aDy = mDy;
  NS_IF_ADDREF(*aDy);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEOffsetElement::Filter(nsSVGFilterInstance *instance)
{
  nsRect rect;
  nsIDOMSVGFilterPrimitiveStandardAttributes *attrib =
    (nsIDOMSVGFilterPrimitiveStandardAttributes *)this;

  nsRect defaultRect;
  nsAutoString input, result;
  mIn1->GetAnimVal(input);
  mResult->GetAnimVal(result);
  instance->LookupRegion(input, &defaultRect);

  instance->GetFilterSubregion(this, defaultRect, &rect);

#ifdef DEBUG_tor
  fprintf(stderr, "FILTER OFFSET rect: %d,%d  %dx%d\n",
          rect.x, rect.y, rect.width, rect.height);
#endif

  nsCOMPtr<nsISVGRendererSurface> sourceImage, targetImage;
  instance->LookupImage(input, getter_AddRefs(sourceImage));
  if (!sourceImage)
    return NS_ERROR_FAILURE;

  instance->GetImage(getter_AddRefs(targetImage));
  if (!targetImage)
    return NS_ERROR_FAILURE;

  PRUint8 *sourceData, *targetData;
  PRInt32 stride;
  PRUint32 width, height, length;
  sourceImage->Lock();
  targetImage->Lock();
  sourceImage->GetData(&sourceData, &length, &stride);
  targetImage->GetData(&targetData, &length, &stride);

  sourceImage->GetWidth(&width);
  sourceImage->GetHeight(&height);

  PRInt32 offsetX, offsetY;
  float flt;
  nsSVGLength2 val;

  mDx->GetAnimVal(&flt);
  val.Init(nsSVGUtils::X, 0xff, flt, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  offsetX = (PRInt32) instance->GetPrimitiveLength(&val);

  mDy->GetAnimVal(&flt);
  val.Init(nsSVGUtils::Y, 0xff, flt, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER);
  offsetY = (PRInt32) instance->GetPrimitiveLength(&val);

  for (PRInt32 y = rect.y; y < rect.y + rect.height; y++) {
    PRInt32 targetRow = y + offsetY;
    if (targetRow < rect.y || targetRow >= rect.y + rect.height)
      continue;

    PRInt32 targetColumn = rect.x + offsetX;
    if (targetColumn < rect.x)
      memcpy(targetData + stride * targetRow + 4 * rect.x,
             sourceData + stride * y - 4 * offsetX,
             4 * (rect.width + offsetX));
    else
      memcpy(targetData + stride * targetRow + 4 * targetColumn,
             sourceData + stride * y + 4 * rect.x,
             4 * (rect.width - offsetX));
  }

  fixupTarget(sourceData, targetData, width, height, stride, rect);

  sourceImage->Unlock();
  targetImage->Unlock();

  instance->DefineImage(result, targetImage);
  instance->DefineRegion(result, rect);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGFEOffsetElement::GetRequirements(PRUint32 *aRequirements)
{
  *aRequirements = CheckStandardNames(mIn1);
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
  virtual ~nsSVGFEUnimplementedMOZElement();

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

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGFEUnimplementedMOZElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGFEUnimplementedMOZElementBase::)

protected:

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

nsSVGFEUnimplementedMOZElement::~nsSVGFEUnimplementedMOZElement()
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGFEUnimplementedMOZElement)

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
