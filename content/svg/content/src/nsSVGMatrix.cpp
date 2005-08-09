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
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsSVGMatrix.h"
#include "nsDOMError.h"
#include "nsSVGValue.h"
#include <math.h>
#include "nsContentUtils.h"

const double radPerDegree = 2.0*3.1415926535 / 360.0;

class nsSVGMatrix : public nsIDOMSVGMatrix,
                    public nsSVGValue
{
public:
  nsSVGMatrix(float a, float b, float c, float d, float e, float f);
  
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGMatrix interface:
  NS_DECL_NSIDOMSVGMATRIX

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
protected:
  float mA, mB, mC, mD, mE, mF;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGMatrix(nsIDOMSVGMatrix** result,
                float a, float b, float c,
                float d, float e, float f)
{
  *result = new nsSVGMatrix(a, b, c, d, e, f);
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

nsSVGMatrix::nsSVGMatrix(float a, float b, float c,
                         float d, float e, float f)
  : mA(a), mB(b), mC(c), mD(d), mE(e), mF(f)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGMatrix)
NS_IMPL_RELEASE(nsSVGMatrix)

NS_INTERFACE_MAP_BEGIN(nsSVGMatrix)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGMatrix)
//  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
//  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGMatrix)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsIDOMSVGMatrix methods:

/* attribute float a; */
NS_IMETHODIMP nsSVGMatrix::GetA(float *aA)
{
  *aA = mA;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetA(float aA)
{
  WillModify();
  mA = aA;
  DidModify();
  return NS_OK;
}

/* attribute float b; */
NS_IMETHODIMP nsSVGMatrix::GetB(float *aB)
{
  *aB = mB;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetB(float aB)
{
  WillModify();
  mB = aB;
  DidModify();
  return NS_OK;
}

/* attribute float c; */
NS_IMETHODIMP nsSVGMatrix::GetC(float *aC)
{
  *aC = mC;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetC(float aC)
{
  WillModify();
  mC = aC;
  DidModify();
  return NS_OK;
}

/* attribute float d; */
NS_IMETHODIMP nsSVGMatrix::GetD(float *aD)
{
  *aD = mD;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetD(float aD)
{
  WillModify();
  mD = aD;
  DidModify();
  return NS_OK;
}

/* attribute float e; */
NS_IMETHODIMP nsSVGMatrix::GetE(float *aE)
{
  *aE = mE;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetE(float aE)
{
  WillModify();
  mE = aE;
  DidModify();
  return NS_OK;
}

/* attribute float f; */
NS_IMETHODIMP nsSVGMatrix::GetF(float *aF)
{
  *aF = mF;
  return NS_OK;
}
NS_IMETHODIMP nsSVGMatrix::SetF(float aF)
{
  WillModify();
  mF = aF;
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGMatrix multiply (in nsIDOMSVGMatrix secondMatrix); */
NS_IMETHODIMP nsSVGMatrix::Multiply(nsIDOMSVGMatrix *secondMatrix,
                                    nsIDOMSVGMatrix **_retval)
{
  if (!secondMatrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  float sa,sb,sc,sd,se,sf;
  secondMatrix->GetA(&sa);
  secondMatrix->GetB(&sb);
  secondMatrix->GetC(&sc);
  secondMatrix->GetD(&sd);
  secondMatrix->GetE(&se);
  secondMatrix->GetF(&sf);

  return NS_NewSVGMatrix(_retval,
                         mA*sa + mC*sb,      mB*sa + mD*sb,
                         mA*sc + mC*sd,      mB*sc + mD*sd,
                         mA*se + mC*sf + mE, mB*se + mD*sf + mF);
}

/* nsIDOMSVGMatrix inverse (); */
NS_IMETHODIMP nsSVGMatrix::Inverse(nsIDOMSVGMatrix **_retval)
{
  double det = mA*mD - mC*mB;
  if (det == 0.0)
    return NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE;

  return NS_NewSVGMatrix(_retval,
                         (float)( mD/det),             (float)(-mB/det),
                         (float)(-mC/det),             (float)( mA/det),
                         (float)((mC*mF - mE*mD)/det), (float)((mE*mB - mA*mF)/det));
}

/* nsIDOMSVGMatrix translate (in float x, in float y); */
NS_IMETHODIMP nsSVGMatrix::Translate(float x, float y, nsIDOMSVGMatrix **_retval)
{
  return NS_NewSVGMatrix(_retval,
                         mA,               mB,
                         mC,               mD,
                         mA*x + mC*y + mE, mB*x + mD*y + mF);
}

/* nsIDOMSVGMatrix scale (in float scaleFactor); */
NS_IMETHODIMP nsSVGMatrix::Scale(float scaleFactor, nsIDOMSVGMatrix **_retval)
{
  return NS_NewSVGMatrix(_retval,
                         mA*scaleFactor, mB*scaleFactor,
                         mC*scaleFactor, mD*scaleFactor,
                         mE,             mF);  
}

/* nsIDOMSVGMatrix scaleNonUniform (in float scaleFactorX, in float scaleFactorY); */
NS_IMETHODIMP nsSVGMatrix::ScaleNonUniform(float scaleFactorX, float scaleFactorY, nsIDOMSVGMatrix **_retval)
{
  return NS_NewSVGMatrix(_retval,
                         mA*scaleFactorX, mB*scaleFactorX,
                         mC*scaleFactorY, mD*scaleFactorY,
                         mE,              mF);  
}

/* nsIDOMSVGMatrix rotate (in float angle); */
NS_IMETHODIMP nsSVGMatrix::Rotate(float angle, nsIDOMSVGMatrix **_retval)
{
  double ca = cos( angle*radPerDegree );
  double sa = sin( angle*radPerDegree );
  
  return NS_NewSVGMatrix(_retval,
                         (float) (mA*ca + mC*sa), (float) (mB*ca + mD*sa),
                         (float) (mC*ca - mA*sa), (float) (mD*ca - mB*sa),
                         mE,                      mF);  
}

/* nsIDOMSVGMatrix rotateFromVector (in float x, in float y); */
NS_IMETHODIMP nsSVGMatrix::RotateFromVector(float x, float y, nsIDOMSVGMatrix **_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGMatrix::RotateFromVector");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGMatrix flipX (); */
NS_IMETHODIMP nsSVGMatrix::FlipX(nsIDOMSVGMatrix **_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGMatrix::FlipX");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGMatrix flipY (); */
NS_IMETHODIMP nsSVGMatrix::FlipY(nsIDOMSVGMatrix **_retval)
{
  NS_NOTYETIMPLEMENTED("nsSVGMatrix::FlipY");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGMatrix skewX (in float angle); */
NS_IMETHODIMP nsSVGMatrix::SkewX(float angle, nsIDOMSVGMatrix **_retval)
{
  double ta = tan( angle*radPerDegree );

  return NS_NewSVGMatrix(_retval,
                         mA,                    mB,
                         (float) ( mC + mA*ta), (float) ( mD + mB*ta),
                         mE,                    mF);
}

/* nsIDOMSVGMatrix skewY (in float angle); */
NS_IMETHODIMP nsSVGMatrix::SkewY(float angle, nsIDOMSVGMatrix **_retval)
{
  double ta = tan( angle*radPerDegree );

  return NS_NewSVGMatrix(_retval,
                         (float) (mA + mC*ta), (float) (mB + mD*ta),
                         mC,                    mD,
                         mE,                    mF);
}


//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGMatrix::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("nsSVGMatrix::SetValueString");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGMatrix::GetValueString(nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("nsSVGMatrix::GetValueString");
  return NS_ERROR_NOT_IMPLEMENTED;
}
