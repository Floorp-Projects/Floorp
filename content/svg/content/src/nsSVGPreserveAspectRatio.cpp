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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is
 * Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Watt <jonathan.watt@strath.ac.uk> (original author)
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

#include "nsSVGPreserveAspectRatio.h"
#include "nsWhitespaceTokenizer.h"
#ifdef MOZ_SMIL
#include "nsSMILValue.h"
#include "SMILEnumType.h"
#endif // MOZ_SMIL

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// nsSVGPreserveAspectRatio class

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  nsSVGPreserveAspectRatio::DOMBaseVal, mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  nsSVGPreserveAspectRatio::DOMAnimVal, mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  nsSVGPreserveAspectRatio::DOMAnimPAspectRatio, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGPreserveAspectRatio::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGPreserveAspectRatio::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGPreserveAspectRatio::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGPreserveAspectRatio::DOMAnimVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGPreserveAspectRatio::DOMAnimPAspectRatio)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGPreserveAspectRatio::DOMAnimPAspectRatio)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGPreserveAspectRatio::DOMBaseVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPreserveAspectRatio)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGPreserveAspectRatio::DOMAnimVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPreserveAspectRatio)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGPreserveAspectRatio::DOMAnimPAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedPreserveAspectRatio)
NS_INTERFACE_MAP_END

/* Implementation */

static const char *sAlignStrings[] =
  { "none", "xMinYMin", "xMidYMin", "xMaxYMin", "xMinYMid", "xMidYMid",
    "xMaxYMid", "xMinYMax", "xMidYMax", "xMaxYMax" };

static const char *sMeetOrSliceStrings[] = { "meet", "slice" };

static PRUint16
GetAlignForString(const nsAString &aAlignString)
{
  for (PRUint32 i = 0 ; i < NS_ARRAY_LENGTH(sAlignStrings) ; i++) {
    if (aAlignString.EqualsASCII(sAlignStrings[i])) {
      return (i + nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE);
    }
  }

  return nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN;
}

static void
GetAlignString(nsAString& aAlignString, PRUint16 aAlign)
{
  NS_ASSERTION(
    aAlign >= nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
    aAlign <= nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX,
    "Unknown align");

  aAlignString.AssignASCII(
    sAlignStrings[aAlign -
                  nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE]);
}

static PRUint16
GetMeetOrSliceForString(const nsAString &aMeetOrSlice)
{
  for (PRUint32 i = 0 ; i < NS_ARRAY_LENGTH(sMeetOrSliceStrings) ; i++) {
    if (aMeetOrSlice.EqualsASCII(sMeetOrSliceStrings[i])) {
      return (i + nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET);
    }
  }

  return nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN;
}

static void
GetMeetOrSliceString(nsAString& aMeetOrSliceString, PRUint16 aMeetOrSlice)
{
  NS_ASSERTION(
    aMeetOrSlice >= nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
    aMeetOrSlice <= nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE,
    "Unknown meetOrSlice");

  aMeetOrSliceString.AssignASCII(
    sMeetOrSliceStrings[aMeetOrSlice -
                        nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET]);
}

nsresult
nsSVGPreserveAspectRatio::ToDOMBaseVal(nsIDOMSVGPreserveAspectRatio **aResult,
                                       nsSVGElement *aSVGElement)
{
  *aResult = new DOMBaseVal(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

nsresult
nsSVGPreserveAspectRatio::ToDOMAnimVal(nsIDOMSVGPreserveAspectRatio **aResult,
                                       nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimVal(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

static nsresult
ToPreserveAspectRatio(const nsAString &aString,
                      nsSVGPreserveAspectRatio::PreserveAspectRatio *aValue)
{
  if (aString.IsEmpty() || NS_IsAsciiWhitespace(aString[0])) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsWhitespaceTokenizer tokenizer(aString);
  if (!tokenizer.hasMoreTokens()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  const nsAString &token = tokenizer.nextToken();

  nsresult rv;
  nsSVGPreserveAspectRatio::PreserveAspectRatio val;

  val.SetDefer(token.EqualsLiteral("defer"));

  if (val.GetDefer()) {
    if (!tokenizer.hasMoreTokens()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    rv = val.SetAlign(GetAlignForString(tokenizer.nextToken()));
  } else {
    rv = val.SetAlign(GetAlignForString(token));
  }

  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (tokenizer.hasMoreTokens()) {
    rv = val.SetMeetOrSlice(GetMeetOrSliceForString(tokenizer.nextToken()));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  } else {
    val.SetMeetOrSlice(nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET);
  }

  if (tokenizer.hasMoreTokens()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  *aValue = val;
  return NS_OK;
}

nsresult
nsSVGPreserveAspectRatio::SetBaseValueString(const nsAString &aValueAsString,
                                             nsSVGElement *aSVGElement,
                                             PRBool aDoSetAttr)
{
  PreserveAspectRatio val;
  nsresult res = ToPreserveAspectRatio(aValueAsString, &val);
  if (NS_FAILED(res)) {
    return res;
  }

  mBaseVal = val;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
#ifdef MOZ_SMIL
  else {
    aSVGElement->AnimationNeedsResample();
  }
#endif

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under nsGenericElement::SetAttr,
  // which takes care of notifying.
  return NS_OK;
}

void
nsSVGPreserveAspectRatio::GetBaseValueString(nsAString & aValueAsString)
{
  nsAutoString tmpString;

  aValueAsString.Truncate();

  if (mBaseVal.mDefer) {
    aValueAsString.AppendLiteral("defer ");
  }

  GetAlignString(tmpString, mBaseVal.mAlign);
  aValueAsString.Append(tmpString);

  if (mBaseVal.mAlign !=
      nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE) {

    aValueAsString.AppendLiteral(" ");
    GetMeetOrSliceString(tmpString, mBaseVal.mMeetOrSlice);
    aValueAsString.Append(tmpString);
  }
}

nsresult
nsSVGPreserveAspectRatio::SetBaseAlign(PRUint16 aAlign,
                                       nsSVGElement *aSVGElement)
{
  nsresult rv = mBaseVal.SetAlign(aAlign);
  NS_ENSURE_SUCCESS(rv, rv);

  mAnimVal.mAlign = mBaseVal.mAlign;
  aSVGElement->DidChangePreserveAspectRatio(PR_TRUE);
#ifdef MOZ_SMIL
  if (mIsAnimated) {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  
  return NS_OK;
}

nsresult
nsSVGPreserveAspectRatio::SetBaseMeetOrSlice(PRUint16 aMeetOrSlice,
                                             nsSVGElement *aSVGElement)
{
  nsresult rv = mBaseVal.SetMeetOrSlice(aMeetOrSlice);
  NS_ENSURE_SUCCESS(rv, rv);

  mAnimVal.mMeetOrSlice = mBaseVal.mMeetOrSlice;
  aSVGElement->DidChangePreserveAspectRatio(PR_TRUE);
#ifdef MOZ_SMIL
  if (mIsAnimated) {
    aSVGElement->AnimationNeedsResample();
  }
#endif
  
  return NS_OK;
}

void
nsSVGPreserveAspectRatio::SetAnimValue(PRUint64 aPackedValue, nsSVGElement *aSVGElement)
{
  mAnimVal.SetDefer(((aPackedValue & 0xff0000) >> 16) ? PR_TRUE : PR_FALSE);
  mAnimVal.SetAlign(PRUint16((aPackedValue & 0xff00) >> 8));
  mAnimVal.SetMeetOrSlice(PRUint16(aPackedValue & 0xff));
  mIsAnimated = PR_TRUE;
  aSVGElement->DidAnimatePreserveAspectRatio();
}

nsresult
nsSVGPreserveAspectRatio::ToDOMAnimatedPreserveAspectRatio(
  nsIDOMSVGAnimatedPreserveAspectRatio **aResult,
  nsSVGElement *aSVGElement)
{
  *aResult = new DOMAnimPAspectRatio(this, aSVGElement);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

#ifdef MOZ_SMIL
nsISMILAttr*
nsSVGPreserveAspectRatio::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILPreserveAspectRatio(this, aSVGElement);
}

static PRUint64
PackPreserveAspectRatio(const nsSVGPreserveAspectRatio::PreserveAspectRatio& par)
{
  // All preserveAspectRatio values are enum values (do not interpolate), so we
  // can safely collate them and treat them as a single enum as for SMIL.
  PRUint64 packed = 0;
  packed |= PRUint64(par.GetDefer() ? 1 : 0) << 16;
  packed |= PRUint64(par.GetAlign()) << 8;
  packed |= PRUint64(par.GetMeetOrSlice());
  return packed;
}

nsresult
nsSVGPreserveAspectRatio::SMILPreserveAspectRatio
                        ::ValueFromString(const nsAString& aStr,
                                          const nsISMILAnimationElement* /*aSrcElement*/,
                                          nsSMILValue& aValue) const
{
  PreserveAspectRatio par;
  nsresult res = ToPreserveAspectRatio(aStr, &par);
  NS_ENSURE_SUCCESS(res, res);

  nsSMILValue val(&SMILEnumType::sSingleton);
  val.mU.mUint = PackPreserveAspectRatio(par);
  aValue = val;
  return NS_OK;
}

nsSMILValue
nsSVGPreserveAspectRatio::SMILPreserveAspectRatio::GetBaseValue() const
{
  nsSMILValue val(&SMILEnumType::sSingleton);
  val.mU.mUint = PackPreserveAspectRatio(mVal->GetBaseValue());
  return val;
}

void
nsSVGPreserveAspectRatio::SMILPreserveAspectRatio::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->SetAnimValue(PackPreserveAspectRatio(mVal->GetBaseValue()), mSVGElement);
    mVal->mIsAnimated = PR_FALSE;
  }
}

nsresult
nsSVGPreserveAspectRatio::SMILPreserveAspectRatio::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SMILEnumType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILEnumType::sSingleton) {
    mVal->SetAnimValue(aValue.mU.mUint, mSVGElement);
  }
  return NS_OK;
}
#endif // MOZ_SMIL
