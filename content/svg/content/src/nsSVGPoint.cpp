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
 *   Jonathan Watt <jonathan.watt@strath.ac.uk>
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

#include "nsSVGPoint.h"
#include "nsIDOMSVGMatrix.h"
#include "nsSVGValue.h"
#include "nsContentUtils.h"
#include "nsDOMError.h"

class nsSVGPoint : public nsIDOMSVGPoint,
                   public nsSVGValue
{
public:
  nsSVGPoint(float x, float y);

  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPoint interface:
  NS_DECL_NSIDOMSVGPOINT

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);
  
protected:
  float mX;
  float mY;
};


//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGPoint(nsIDOMSVGPoint** result, float x, float y)
{
  *result = new nsSVGPoint(x, y);
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

nsresult
NS_NewSVGPoint(nsIDOMSVGPoint** result, const gfxPoint& point)
{
  return NS_NewSVGPoint(result, float(point.x), float(point.y));
}

nsSVGPoint::nsSVGPoint(float x, float y)
    : mX(x), mY(y)
{
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGPoint)
NS_IMPL_RELEASE(nsSVGPoint)

NS_INTERFACE_MAP_BEGIN(nsSVGPoint)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPoint)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPoint)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsIDOMSVGPoint methods:

/* attribute float x; */
NS_IMETHODIMP nsSVGPoint::GetX(float *aX)
{
  *aX = mX;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPoint::SetX(float aX)
{
  WillModify();
  mX = aX;
  DidModify();
  
  return NS_OK;
}

/* attribute float y; */
NS_IMETHODIMP nsSVGPoint::GetY(float *aY)
{
  *aY = mY;
  return NS_OK;
}
NS_IMETHODIMP nsSVGPoint::SetY(float aY)
{
  WillModify();
  mY = aY;
  DidModify();
  
  return NS_OK;
}

/* nsIDOMSVGPoint matrixTransform (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP nsSVGPoint::MatrixTransform(nsIDOMSVGMatrix *matrix,
                                          nsIDOMSVGPoint **_retval)
{
  if (!matrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  float a, b, c, d, e, f;
  matrix->GetA(&a);
  matrix->GetB(&b);
  matrix->GetC(&c);
  matrix->GetD(&d);
  matrix->GetE(&e);
  matrix->GetF(&f);
  
  return NS_NewSVGPoint(_retval, a*mX + c*mY + e, b*mX + d*mY + f);
}

//----------------------------------------------------------------------
// nsISVGValue methods:
NS_IMETHODIMP
nsSVGPoint::SetValueString(const nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("nsSVGPoint::SetValueString");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGPoint::GetValueString(nsAString& aValue)
{
  NS_NOTYETIMPLEMENTED("nsSVGPoint::GetValueString");
  return NS_ERROR_NOT_IMPLEMENTED;
}



////////////////////////////////////////////////////////////////////////
// Implement a readonly version of SVGPoint
//
// We need this because attributes of some SVG interfaces *and* the objects the
// attributes refer to (including SVGPoints) are supposed to be readonly

class nsSVGReadonlyPoint : public nsSVGPoint
{
public:
  nsSVGReadonlyPoint(float x, float y)
    : nsSVGPoint(x, y)
  {
  };

  // override setters to make the whole object readonly
  NS_IMETHODIMP SetX(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetY(float) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  NS_IMETHODIMP SetValueString(const nsAString&) { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
};

nsresult
NS_NewSVGReadonlyPoint(nsIDOMSVGPoint** result, float x, float y)
{
  *result = new nsSVGReadonlyPoint(x, y);
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

