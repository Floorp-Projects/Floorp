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
 * IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Rowley <tor@acm.org> (original author)
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

#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsISVGEnum.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedEnumeration

class nsSVGAnimatedEnumeration : public nsIDOMSVGAnimatedEnumeration,
                                 public nsSVGValue,
                                 public nsISVGValueObserver,
                                 public nsSupportsWeakReference
{
protected:
  friend nsresult NS_NewSVGAnimatedEnumeration(nsIDOMSVGAnimatedEnumeration** result,
                                               nsISVGEnum* aBaseVal);
  nsSVGAnimatedEnumeration();
  ~nsSVGAnimatedEnumeration();
  nsresult Init(nsISVGEnum* aBaseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedEnumeration interface:
  NS_DECL_NSIDOMSVGANIMATEDENUMERATION

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  nsCOMPtr<nsISVGEnum> mBaseVal;
};



//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedEnumeration::nsSVGAnimatedEnumeration()
{
}

nsSVGAnimatedEnumeration::~nsSVGAnimatedEnumeration()
{
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (val) val->RemoveObserver(this);
}

nsresult
nsSVGAnimatedEnumeration::Init(nsISVGEnum* aBaseVal)
{
  mBaseVal = aBaseVal;
  if (!mBaseVal) return NS_ERROR_FAILURE;
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  NS_ASSERTION(val, "baseval needs to implement nsISVGValue interface");
  if (!val) return NS_ERROR_FAILURE;
  val->AddObserver(this);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedEnumeration)
NS_IMPL_RELEASE(nsSVGAnimatedEnumeration)


NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedEnumeration)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END
  
//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedEnumeration::SetValueString(const nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  NS_ASSERTION(value, "svg animated enumeration base value has wrong interface!");
  return value->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAnimatedEnumeration::GetValueString(nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  NS_ASSERTION(value, "svg animated enumeration base value has wrong interface!");
  return value->GetValueString(aValue);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedEnumeration methods:

/* attribute unsigned short baseVal; */
NS_IMETHODIMP
nsSVGAnimatedEnumeration::GetBaseVal(PRUint16 *aBaseVal)
{
  mBaseVal->GetIntegerValue(*aBaseVal);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedEnumeration::SetBaseVal(PRUint16 aBaseVal)
{
  return mBaseVal->SetIntegerValue(aBaseVal);
}

/* readonly attribute unsigned short animVal; */
NS_IMETHODIMP
nsSVGAnimatedEnumeration::GetAnimVal(PRUint16 *aAnimVal)
{
  mBaseVal->GetIntegerValue(*aAnimVal);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAnimatedEnumeration::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedEnumeration::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedEnumeration(nsIDOMSVGAnimatedEnumeration** aResult,
                             nsISVGEnum* aBaseVal)
{
  *aResult = nsnull;
  
  nsSVGAnimatedEnumeration* animatedEnum = new nsSVGAnimatedEnumeration();
  if (!animatedEnum) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedEnum);

  nsresult rv = animatedEnum->Init(aBaseVal);
  
  *aResult = (nsIDOMSVGAnimatedEnumeration*) animatedEnum;
  
  return rv;
}


