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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsISVGViewportAxis.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsISVGViewportRect.h"
#include "nsSVGNumber.h"
#include "nsISVGValue.h"
#include "nsSVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsISVGValueUtils.h"
#include "nsDOMError.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////
// nsSVGViewportAxis : helper class implementing nsISVGViewportAxis

class nsSVGViewportAxis : public nsISVGViewportAxis
                      // ,public nsISVGValueObserver,
                      //  public nsSupportsWeakReference
{
protected:
  friend nsresult NS_NewSVGViewportAxis(nsISVGViewportAxis **result,
                                        nsIDOMSVGNumber* scale,
                                        nsIDOMSVGNumber* length);
  nsSVGViewportAxis();
  nsresult Init(nsIDOMSVGNumber* scale, nsIDOMSVGNumber* length);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGViewportAxis methods:
  NS_IMETHOD GetMillimeterPerPixel(nsIDOMSVGNumber **scale);
  NS_IMETHOD GetLength(nsIDOMSVGNumber **length);
  
  // nsISVGValueObserver
  // NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  // NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

private:
  nsCOMPtr<nsIDOMSVGNumber> mScale;
  nsCOMPtr<nsIDOMSVGNumber> mLength;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGViewportAxis(nsISVGViewportAxis **result, nsIDOMSVGNumber* scale,
                      nsIDOMSVGNumber* length)
{
  *result = nsnull;
  nsSVGViewportAxis *va =  new nsSVGViewportAxis();
  NS_ENSURE_TRUE(va, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(va);
  
  nsresult rv = va->Init(scale, length);

  if (NS_FAILED(rv)) {
    NS_RELEASE(va);
    return rv;
  }

  *result = va;
  
  return rv;
}

nsSVGViewportAxis::nsSVGViewportAxis()
{
}


nsresult
nsSVGViewportAxis::Init(nsIDOMSVGNumber* scale,
                        nsIDOMSVGNumber* length)
{
  mScale = scale;
  mLength = length;

  return NS_OK;
}
  
//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGViewportAxis)
NS_IMPL_RELEASE(nsSVGViewportAxis)

NS_INTERFACE_MAP_BEGIN(nsSVGViewportAxis)
  NS_INTERFACE_MAP_ENTRY(nsISVGViewportAxis)
//  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
//  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGViewportAxis)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGViewportAxis methods:

NS_IMETHODIMP
nsSVGViewportAxis::GetMillimeterPerPixel(nsIDOMSVGNumber **scale)
{
  *scale = mScale;
  NS_ADDREF(*scale);
  return NS_OK;
}


NS_IMETHODIMP
nsSVGViewportAxis::GetLength(nsIDOMSVGNumber **length)
{
  *length = mLength;
  NS_ADDREF(*length);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

// NS_IMETHODIMP
// nsSVGViewportAxis::WillModifySVGObservable(nsISVGValue* observable)
// {
//   return NS_OK;
// }

// NS_IMETHODIMP
// nsSVGViewportAxis::DidModifySVGObservable(nsISVGValue* observable)
// {
//   // we only listen in on changes to the length
//   UpdateLength();
//   return NS_OK;
// }


////////////////////////////////////////////////////////////////////////
// nsSVGViewportRect


class nsSVGViewportRect : public nsISVGViewportRect,
                          public nsSVGValue,
                          public nsISVGValueObserver,
                          public nsSupportsWeakReference
{
protected:
  friend nsresult
  NS_NewSVGViewportRect(nsISVGViewportRect **result,
                        nsIDOMSVGNumber* scalex,
                        nsIDOMSVGNumber* scaley,
                        nsIDOMSVGNumber* lengthx,
                        nsIDOMSVGNumber* lengthy);
  nsSVGViewportRect();
  ~nsSVGViewportRect();
  nsresult Init(nsIDOMSVGNumber* scalex,
                nsIDOMSVGNumber* scaley,
                nsIDOMSVGNumber* lengthx,
                nsIDOMSVGNumber* lengthy);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGViewportRect interface:
  NS_IMETHOD GetXAxis(nsISVGViewportAxis **xaxis);
  NS_IMETHOD GetYAxis(nsISVGViewportAxis **yaxis);
  NS_IMETHOD GetUnspecifiedAxis(nsISVGViewportAxis **axis);
  
  // nsIDOMSVGRect interface:
  NS_DECL_NSIDOMSVGRECT

  // nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

protected:
  void UpdateGenericAxisScale();
  void UpdateGenericAxisLength();
  
private:
  virtual void OnDidModify();
  
  nsCOMPtr<nsISVGViewportAxis> mAxisX;
  nsCOMPtr<nsISVGViewportAxis> mAxisY;
  nsCOMPtr<nsISVGViewportAxis> mAxisUnspecified;
};


//----------------------------------------------------------------------
// implementation:

nsresult
NS_NewSVGViewportRect(nsISVGViewportRect **result,
                      nsIDOMSVGNumber* scalex,
                      nsIDOMSVGNumber* scaley,
                      nsIDOMSVGNumber* lengthx,
                      nsIDOMSVGNumber* lengthy)
{
  *result = nsnull;
  
  nsSVGViewportRect *vr = new nsSVGViewportRect();
  if (!vr) return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(vr);
  nsresult rv = vr->Init(scalex, scaley, lengthx, lengthy);
  if (NS_FAILED(rv)) {
    NS_RELEASE(vr);
    return rv;
  }

  *result = vr;
  return NS_OK;
      
}

nsSVGViewportRect::nsSVGViewportRect()
{
}

nsSVGViewportRect::~nsSVGViewportRect()
{
  if (mAxisX) {
    nsCOMPtr<nsIDOMSVGNumber> l;
    mAxisX->GetLength(getter_AddRefs(l));
    NS_REMOVE_SVGVALUE_OBSERVER(l);
    
    nsCOMPtr<nsIDOMSVGNumber> s;
    mAxisX->GetMillimeterPerPixel(getter_AddRefs(s));
    NS_REMOVE_SVGVALUE_OBSERVER(s);
  }

  if (mAxisY) {
    nsCOMPtr<nsIDOMSVGNumber> l;
    mAxisY->GetLength(getter_AddRefs(l));
    NS_REMOVE_SVGVALUE_OBSERVER(l);
    
    nsCOMPtr<nsIDOMSVGNumber> s;
    mAxisY->GetMillimeterPerPixel(getter_AddRefs(s));
    NS_REMOVE_SVGVALUE_OBSERVER(s);
  }
}

nsresult
nsSVGViewportRect::Init(nsIDOMSVGNumber* scalex, nsIDOMSVGNumber* scaley,
                        nsIDOMSVGNumber* lengthx, nsIDOMSVGNumber* lengthy)
{
  nsresult rv;
  
  rv = NS_NewSVGViewportAxis(getter_AddRefs(mAxisX), scalex,lengthx);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = NS_NewSVGViewportAxis(getter_AddRefs(mAxisY), scaley,lengthy);
  NS_ENSURE_SUCCESS(rv,rv);

  {
    nsCOMPtr<nsIDOMSVGNumber> sqrt_scale;
    rv = NS_NewSVGNumber(getter_AddRefs(sqrt_scale));
    NS_ENSURE_SUCCESS(rv,rv);
    nsCOMPtr<nsIDOMSVGNumber> sqrt_length;
    rv = NS_NewSVGNumber(getter_AddRefs(sqrt_length));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGViewportAxis(getter_AddRefs(mAxisUnspecified),
                               sqrt_scale,sqrt_length);
    NS_ENSURE_SUCCESS(rv,rv);
    UpdateGenericAxisScale();
    UpdateGenericAxisLength();
  }

  NS_ADD_SVGVALUE_OBSERVER(scalex);
  NS_ADD_SVGVALUE_OBSERVER(scaley);
  NS_ADD_SVGVALUE_OBSERVER(lengthx);
  NS_ADD_SVGVALUE_OBSERVER(lengthy);
  
  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGViewportRect)
NS_IMPL_RELEASE(nsSVGViewportRect)

NS_INTERFACE_MAP_BEGIN(nsSVGViewportRect)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGRect)
  NS_INTERFACE_MAP_ENTRY(nsISVGViewportRect)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGRect)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGViewportRect methods:
NS_IMETHODIMP
nsSVGViewportRect::GetXAxis(nsISVGViewportAxis **xaxis)
{
  *xaxis = mAxisX;
  NS_ADDREF(*xaxis);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGViewportRect::GetYAxis(nsISVGViewportAxis **yaxis)
{
  *yaxis = mAxisY;
  NS_ADDREF(*yaxis);
  return NS_OK;  
}

NS_IMETHODIMP
nsSVGViewportRect::GetUnspecifiedAxis(nsISVGViewportAxis **axis)
{
  *axis = mAxisUnspecified;
  NS_ADDREF(*axis);
  return NS_OK;
}



//----------------------------------------------------------------------
// nsIDOMSVGRect methods:

/* attribute float x; */
NS_IMETHODIMP
nsSVGViewportRect::GetX(float *aX)
{
  *aX = 0.0f;
  return NS_OK;
}
NS_IMETHODIMP
nsSVGViewportRect::SetX(float aX)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; // XXX return different error
}

/* attribute float y; */
NS_IMETHODIMP
nsSVGViewportRect::GetY(float *aY)
{
  *aY = 0.0f;
  return NS_OK;
}
NS_IMETHODIMP
nsSVGViewportRect::SetY(float aY)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

/* attribute float width; */
NS_IMETHODIMP
nsSVGViewportRect::GetWidth(float *aWidth)
{
  nsCOMPtr<nsIDOMSVGNumber> l;
  mAxisX->GetLength(getter_AddRefs(l));
  return l->GetValue(aWidth);
}
NS_IMETHODIMP
nsSVGViewportRect::SetWidth(float aWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

/* attribute float height; */
NS_IMETHODIMP
nsSVGViewportRect::GetHeight(float *aHeight)
{
  nsCOMPtr<nsIDOMSVGNumber> l;
  mAxisY->GetLength(getter_AddRefs(l));
  return l->GetValue(aHeight);
}
NS_IMETHODIMP
nsSVGViewportRect::SetHeight(float aHeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGViewportRect::SetValueString(const nsAString& aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGViewportRect::GetValueString(nsAString& aValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGViewportRect::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGViewportRect::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

//----------------------------------------------------------------------
// helpers:

void nsSVGViewportRect::UpdateGenericAxisLength()
{
  float x;
  {
    nsCOMPtr<nsIDOMSVGNumber> l_x;
    mAxisX->GetLength(getter_AddRefs(l_x));
    l_x->GetValue(&x);
  }

  float y;
  {
    nsCOMPtr<nsIDOMSVGNumber> l_y;
    mAxisY->GetLength(getter_AddRefs(l_y));
    l_y->GetValue(&y);
  }
  
  nsCOMPtr<nsIDOMSVGNumber> l_sqrt;  
  mAxisUnspecified->GetLength (getter_AddRefs(l_sqrt));
  l_sqrt->SetValue((float)sqrt((x*x + y*y)/2.0f));
}

void nsSVGViewportRect::UpdateGenericAxisScale()  
{
  float x;
  {
    nsCOMPtr<nsIDOMSVGNumber> s_x;
    mAxisX->GetMillimeterPerPixel(getter_AddRefs(s_x));
    s_x->GetValue(&x);
  }

  float y;
  {
    nsCOMPtr<nsIDOMSVGNumber> s_y;
    mAxisY->GetMillimeterPerPixel(getter_AddRefs(s_y));
    s_y->GetValue(&y);
  }
  
  nsCOMPtr<nsIDOMSVGNumber> s_sqrt;
  mAxisUnspecified->GetMillimeterPerPixel(getter_AddRefs(s_sqrt));
  s_sqrt->SetValue((float)sqrt(x*x + y*y));
}

void nsSVGViewportRect::OnDidModify()
{
  UpdateGenericAxisLength();
  UpdateGenericAxisScale();
}
