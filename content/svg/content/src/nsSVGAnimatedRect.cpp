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

#include "nsSVGAnimatedRect.h"
#include "nsSVGRect.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedRect

class nsSVGAnimatedRect : public nsIDOMSVGAnimatedRect,
                          public nsSVGValue,
                          public nsISVGValueObserver
{  
protected:
  friend nsresult NS_NewSVGAnimatedRect(nsIDOMSVGAnimatedRect** result,
                                        nsIDOMSVGRect* baseVal);

  nsSVGAnimatedRect();
  ~nsSVGAnimatedRect();
  void Init(nsIDOMSVGRect* baseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedRect interface:
  NS_DECL_NSIDOMSVGANIMATEDRECT

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     modificationType aModType);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  nsCOMPtr<nsIDOMSVGRect> mBaseVal;
};


//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedRect::nsSVGAnimatedRect()
{
}

nsSVGAnimatedRect::~nsSVGAnimatedRect()
{
  if (!mBaseVal) return;
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (!val) return;
  val->RemoveObserver(this);
}

void
nsSVGAnimatedRect::Init(nsIDOMSVGRect* baseVal)
{
  mBaseVal = baseVal;
  if (!mBaseVal) return;
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (!val) return;
  val->AddObserver(this);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedRect)
NS_IMPL_RELEASE(nsSVGAnimatedRect)

NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedRect)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedRect)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedRect)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedRect::SetValueString(const nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  return value->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAnimatedRect::GetValueString(nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  return value->GetValueString(aValue);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedRect methods:

/* readonly attribute nsIDOMSVGRect baseVal; */
NS_IMETHODIMP
nsSVGAnimatedRect::GetBaseVal(nsIDOMSVGRect * *aBaseVal)
{
  *aBaseVal = mBaseVal;
  NS_ADDREF(*aBaseVal);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGRect animVal; */
NS_IMETHODIMP
nsSVGAnimatedRect::GetAnimVal(nsIDOMSVGRect * *aAnimVal)
{
  *aAnimVal = mBaseVal;
  NS_ADDREF(*aAnimVal);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAnimatedRect::WillModifySVGObservable(nsISVGValue* observable,
                                           modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedRect::DidModifySVGObservable (nsISVGValue* observable,
                                           modificationType aModType)
{
  DidModify(aModType);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGAnimatedRect(nsIDOMSVGAnimatedRect** result,
                      nsIDOMSVGRect* baseVal)
{
  *result = nsnull;
  
  nsSVGAnimatedRect* animatedRect = new nsSVGAnimatedRect();
  if(!animatedRect) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedRect);

  animatedRect->Init(baseVal);
  
  *result = (nsIDOMSVGAnimatedRect*) animatedRect;
  
  return NS_OK;
}

