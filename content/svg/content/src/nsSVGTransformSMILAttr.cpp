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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "nsSVGTransformSMILAttr.h"
#include "nsSVGTransformSMILType.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGTransformList.h"
#include "nsSVGTransform.h"
#include "nsIDOMSVGTransform.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGMatrix.h"
#include "nsSMILValue.h"
#include "nsSMILNullType.h"
#include "nsISMILAnimationElement.h"
#include "nsSVGElement.h"
#include "nsISVGValue.h"
#include "prdtoa.h"
#include "prlog.h"

nsresult
nsSVGTransformSMILAttr::ValueFromString(const nsAString& aStr,
                                     const nsISMILAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     PRBool& aPreventCachingOfSandwich) const
{
  NS_ENSURE_TRUE(aSrcElement, NS_ERROR_FAILURE);
  NS_ASSERTION(aValue.IsNull(),
    "aValue should have been cleared before calling ValueFromString");

  const nsAttrValue* typeAttr = aSrcElement->GetAnimAttr(nsGkAtoms::type);
  const nsIAtom* transformType = nsGkAtoms::translate;
  if (typeAttr) {
    if (typeAttr->Type() != nsAttrValue::eAtom) {
      // Recognized values of |type| are parsed as an atom -- so if we have
      // something other than an atom, then it means our |type| was invalid.
      return NS_ERROR_FAILURE;
    }
    transformType = typeAttr->GetAtomValue();
  }

  ParseValue(aStr, transformType, aValue);
  aPreventCachingOfSandwich = PR_FALSE;
  return aValue.IsNull() ? NS_ERROR_FAILURE : NS_OK;
}

nsSMILValue
nsSVGTransformSMILAttr::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue val(&nsSVGTransformSMILType::sSingleton);

  nsIDOMSVGTransformList *list = mVal->mBaseVal.get();

  PRUint32 numItems = 0;
  list->GetNumberOfItems(&numItems);
  for (PRUint32 i = 0; i < numItems; i++) {
    nsCOMPtr<nsIDOMSVGTransform> transform;
    nsresult rv = list->GetItem(i, getter_AddRefs(transform));
    if (NS_SUCCEEDED(rv) && transform) {
      rv = AppendSVGTransformToSMILValue(transform.get(), val);
      if (NS_FAILED(rv)) {   // Appending to |val| failed (OOM?)
        val = nsSMILValue();
        break;
      }
    }
  }

  return val;
}

void
nsSVGTransformSMILAttr::ClearAnimValue()
{
  mVal->WillModify(nsISVGValue::mod_other);
  mVal->mAnimVal = nsnull;
  mVal->DidModify(nsISVGValue::mod_other);
}

nsresult
nsSVGTransformSMILAttr::SetAnimValue(const nsSMILValue& aValue)
{
  if (aValue.mType != &nsSVGTransformSMILType::sSingleton) {
    NS_WARNING("Unexpected SMIL Type");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  // Create the anim value if necessary
  if (!mVal->mAnimVal) {
    rv = nsSVGTransformList::Create(getter_AddRefs(mVal->mAnimVal));
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // Do a minimal update on the anim value and if anything fails, set the anim
  // value to null so that calls to nsSVGAnimatedTransformList::GetAnimVal will
  // return the base value instead.
  rv = UpdateFromSMILValue(mVal->mAnimVal, aValue);
  if (NS_FAILED(rv)) {
    mVal->mAnimVal = nsnull;
  }
  NS_ENSURE_SUCCESS(rv,rv);

  mSVGElement->DidAnimateTransform();
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGTransformSMILAttr::ParseValue(const nsAString& aSpec,
                                   const nsIAtom* aTransformType,
                                   nsSMILValue& aResult)
{
  NS_ASSERTION(aResult.IsNull(), "Unexpected type for SMIL value");

  // nsSVGSMILTransform constructor should be expecting array with 3 params
  PR_STATIC_ASSERT(nsSVGSMILTransform::NUM_SIMPLE_PARAMS == 3);

  float params[3] = { 0.f };
  PRInt32 numParsed = ParseParameterList(aSpec, params, 3);
  nsSVGSMILTransform::TransformType transformType;

  if (aTransformType == nsGkAtoms::translate) {
    // tx [ty=0]
    if (numParsed != 1 && numParsed != 2)
      return;
    transformType = nsSVGSMILTransform::TRANSFORM_TRANSLATE;
  } else if (aTransformType == nsGkAtoms::scale) {
    // sx [sy=sx]
    if (numParsed != 1 && numParsed != 2)
      return;
    if (numParsed == 1) {
      params[1] = params[0];
    }
    transformType = nsSVGSMILTransform::TRANSFORM_SCALE;
  } else if (aTransformType == nsGkAtoms::rotate) {
    // r [cx=0 cy=0]
    if (numParsed != 1 && numParsed != 3)
      return;
    transformType = nsSVGSMILTransform::TRANSFORM_ROTATE;
  } else if (aTransformType == nsGkAtoms::skewX) {
    // x-angle
    if (numParsed != 1)
      return;
    transformType = nsSVGSMILTransform::TRANSFORM_SKEWX;
  } else if (aTransformType == nsGkAtoms::skewY) {
    // y-angle
    if (numParsed != 1)
      return;
    transformType = nsSVGSMILTransform::TRANSFORM_SKEWY;
  } else {
    return;
  }

  nsSMILValue val(&nsSVGTransformSMILType::sSingleton);
  nsSVGSMILTransform transform(transformType, params);
  if (NS_FAILED(nsSVGTransformSMILType::AppendTransform(transform, val))) {
    return;
  }

  // Success! Populate our outparam with parsed value.
  aResult.Swap(val);
}

inline PRBool
IsSpace(const char c)
{
  return (c == 0x9 || c == 0xA || c == 0xD || c == 0x20);
}

inline void
SkipWsp(nsACString::const_iterator& aIter,
        const nsACString::const_iterator& aIterEnd)
{
  while (aIter != aIterEnd && IsSpace(*aIter))
    ++aIter;
}

PRInt32
nsSVGTransformSMILAttr::ParseParameterList(const nsAString& aSpec,
                                           float* aVars,
                                           PRInt32 aNVars)
{
  NS_ConvertUTF16toUTF8 spec(aSpec);

  nsACString::const_iterator start, end;
  spec.BeginReading(start);
  spec.EndReading(end);

  SkipWsp(start, end);

  int numArgsFound = 0;

  while (start != end) {
    char const *arg = start.get();
    char *argend;
    float f = float(PR_strtod(arg, &argend));
    if (arg == argend || argend > end.get() || !NS_FloatIsFinite(f))
      return -1;

    if (numArgsFound < aNVars) {
      aVars[numArgsFound] = f;
    }

    start.advance(argend - arg);
    numArgsFound++;

    SkipWsp(start, end);
    if (*start == ',') {
      ++start;
      SkipWsp(start, end);
    }
  }

  return numArgsFound;
}

nsresult
nsSVGTransformSMILAttr::AppendSVGTransformToSMILValue(
  nsIDOMSVGTransform* aTransform, nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &nsSVGTransformSMILType::sSingleton,
               "Unexpected type for SMIL value");

  PRUint16 svgTransformType = nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX;
  aTransform->GetType(&svgTransformType);

  nsCOMPtr<nsIDOMSVGMatrix> matrix;
  nsresult rv = aTransform->GetMatrix(getter_AddRefs(matrix));
  if (NS_FAILED(rv) || !matrix)
    return NS_ERROR_FAILURE;

  // nsSVGSMILTransform constructor should be expecting array with 3 params
  PR_STATIC_ASSERT(nsSVGSMILTransform::NUM_SIMPLE_PARAMS == 3);
  float params[3] = { 0.f };
  nsSVGSMILTransform::TransformType transformType;

  switch (svgTransformType)
  {
    case nsIDOMSVGTransform::SVG_TRANSFORM_TRANSLATE:
      {
        matrix->GetE(&params[0]);
        matrix->GetF(&params[1]);
        transformType = nsSVGSMILTransform::TRANSFORM_TRANSLATE;
      }
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SCALE:
      {
        matrix->GetA(&params[0]);
        matrix->GetD(&params[1]);
        transformType = nsSVGSMILTransform::TRANSFORM_SCALE;
      }
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_ROTATE:
      {
        /*
         * Unfortunately the SVG 1.1 DOM API for transforms doesn't allow us to
         * query the center of rotation so we do some dirty casting to make up
         * for it.
         */
        nsSVGTransform* svgTransform = static_cast<nsSVGTransform*>(aTransform);
        svgTransform->GetAngle(&params[0]);
        svgTransform->GetRotationOrigin(params[1], params[2]);
        transformType = nsSVGSMILTransform::TRANSFORM_ROTATE;
      }
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWX:
      {
        aTransform->GetAngle(&params[0]);
        transformType = nsSVGSMILTransform::TRANSFORM_SKEWX;
      }
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_SKEWY:
      {
        aTransform->GetAngle(&params[0]);
        transformType = nsSVGSMILTransform::TRANSFORM_SKEWY;
      }
      break;

    case nsIDOMSVGTransform::SVG_TRANSFORM_MATRIX:
      {
        // nsSVGSMILTransform constructor for TRANSFORM_MATRIX type should be
        // expecting array with 6 params
        PR_STATIC_ASSERT(nsSVGSMILTransform::NUM_STORED_PARAMS == 6);
        float mx[6];
        matrix->GetA(&mx[0]);
        matrix->GetB(&mx[1]);
        matrix->GetC(&mx[2]);
        matrix->GetD(&mx[3]);
        matrix->GetE(&mx[4]);
        matrix->GetF(&mx[5]);
        return nsSVGTransformSMILType::AppendTransform(nsSVGSMILTransform(mx),
                                                       aValue);
      }

    case nsIDOMSVGTransform::SVG_TRANSFORM_UNKNOWN:
      // If it's 'unknown', it's probably not initialised, so just skip it.
      return NS_OK;

    default:
      NS_WARNING("Trying to convert unrecognised SVG transform type");
      return NS_ERROR_FAILURE;
  }

  NS_ABORT_IF_FALSE(transformType != nsSVGSMILTransform::TRANSFORM_MATRIX,
                    "generalized matrix case should have returned above");

  return nsSVGTransformSMILType::
    AppendTransform(nsSVGSMILTransform(transformType, params), aValue);
}

nsresult
nsSVGTransformSMILAttr::UpdateFromSMILValue(
  nsIDOMSVGTransformList* aTransformList, const nsSMILValue& aValue)
{
  PRUint32 svgLength = -1;
  aTransformList->GetNumberOfItems(&svgLength);

  nsSVGTransformSMILType* type = &nsSVGTransformSMILType::sSingleton;
  PRUint32 smilLength = type->GetNumTransforms(aValue);

  nsresult rv = NS_OK;

  for (PRUint32 i = 0; i < smilLength; i++) {
    nsCOMPtr<nsIDOMSVGTransform> transform;
    if (i < svgLength) {
      // Get the transform to update
      rv = aTransformList->GetItem(i, getter_AddRefs(transform));
      NS_ENSURE_SUCCESS(rv,rv);
    } else {
      // Append another transform to the list
      nsresult rv = NS_NewSVGTransform(getter_AddRefs(transform));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr<nsIDOMSVGTransform> result;
      rv = aTransformList->AppendItem(transform, getter_AddRefs(result));
      NS_ENSURE_SUCCESS(rv,rv);
    }
    // Set the value
    const nsSVGSMILTransform* smilTransform = type->GetTransformAt(i, aValue);
    rv = GetSVGTransformFromSMILValue(*smilTransform, transform);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // Trim excess elements
  while (svgLength > smilLength) {
    nsCOMPtr<nsIDOMSVGTransform> removed;
    rv = aTransformList->RemoveItem(--svgLength, getter_AddRefs(removed));
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

nsresult
nsSVGTransformSMILAttr::GetSVGTransformFromSMILValue(
    const nsSVGSMILTransform& aSMILTransform,
    nsIDOMSVGTransform* aSVGTransform)
{
  switch (aSMILTransform.mTransformType)
  {
    case nsSVGSMILTransform::TRANSFORM_TRANSLATE:
      return aSVGTransform->SetTranslate(aSMILTransform.mParams[0],
                                         aSMILTransform.mParams[1]);

    case nsSVGSMILTransform::TRANSFORM_SCALE:
      return aSVGTransform->SetScale(aSMILTransform.mParams[0],
                                   aSMILTransform.mParams[1]);

    case nsSVGSMILTransform::TRANSFORM_ROTATE:
      return aSVGTransform->SetRotate(aSMILTransform.mParams[0],
                                      aSMILTransform.mParams[1],
                                      aSMILTransform.mParams[2]);

    case nsSVGSMILTransform::TRANSFORM_SKEWX:
      return aSVGTransform->SetSkewX(aSMILTransform.mParams[0]);

    case nsSVGSMILTransform::TRANSFORM_SKEWY:
      return aSVGTransform->SetSkewY(aSMILTransform.mParams[0]);

    case nsSVGSMILTransform::TRANSFORM_MATRIX:
    {
      nsCOMPtr<nsIDOMSVGMatrix> svgMatrix;
      nsresult rv =
        NS_NewSVGMatrix(getter_AddRefs(svgMatrix),
                        aSMILTransform.mParams[0],
                        aSMILTransform.mParams[1],
                        aSMILTransform.mParams[2],
                        aSMILTransform.mParams[3],
                        aSMILTransform.mParams[4],
                        aSMILTransform.mParams[5]);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ABORT_IF_FALSE(svgMatrix,
                        "NS_NewSVGMatrix succeeded, so it should have "
                        "given us a non-null result");
      return aSVGTransform->SetMatrix(svgMatrix);
    }
    default:
      NS_WARNING("Unexpected transform type");
      return NS_ERROR_FAILURE;
  }
}
