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

#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGPreserveAspectRatio.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedPreserveAspectRatio

class nsSVGAnimatedPreserveAspectRatio : public nsIDOMSVGAnimatedPreserveAspectRatio,
                                         public nsSVGValue,
                                         public nsISVGValueObserver,
                                         public nsSupportsWeakReference
{
protected:
  friend nsresult NS_NewSVGAnimatedPreserveAspectRatio(
                                 nsIDOMSVGAnimatedPreserveAspectRatio** result,
                                 nsIDOMSVGPreserveAspectRatio* baseVal);

  nsSVGAnimatedPreserveAspectRatio();
  ~nsSVGAnimatedPreserveAspectRatio();
  void Init(nsIDOMSVGPreserveAspectRatio* baseVal);

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedPreserveAspectRatio interface:
  NS_DECL_NSIDOMSVGANIMATEDPRESERVEASPECTRATIO

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

protected:
  nsCOMPtr<nsIDOMSVGPreserveAspectRatio> mBaseVal;
};


//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedPreserveAspectRatio::nsSVGAnimatedPreserveAspectRatio()
{
}

nsSVGAnimatedPreserveAspectRatio::~nsSVGAnimatedPreserveAspectRatio()
{
  nsCOMPtr<nsISVGValue> val( do_QueryInterface(mBaseVal) );
  if (!val) return;
  val->RemoveObserver(this);
}

void
nsSVGAnimatedPreserveAspectRatio::Init(nsIDOMSVGPreserveAspectRatio* aBaseVal)
{
  mBaseVal = aBaseVal;
  nsCOMPtr<nsISVGValue> val( do_QueryInterface(mBaseVal) );
  NS_ASSERTION(val, "baseval needs to implement nsISVGValue interface");
  if (!val) return;
  val->AddObserver(this);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedPreserveAspectRatio)
NS_IMPL_RELEASE(nsSVGAnimatedPreserveAspectRatio)

NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::SetValueString(const nsAString& aValue)
{
  nsresult rv;
  nsCOMPtr<nsISVGValue> val( do_QueryInterface(mBaseVal, &rv) );
  return NS_FAILED(rv)? rv: val->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::GetValueString(nsAString& aValue)
{
  nsresult rv;
  nsCOMPtr<nsISVGValue> val( do_QueryInterface(mBaseVal, &rv) );
  return NS_FAILED(rv)? rv: val->GetValueString(aValue);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedPreserveAspectRatio methods:

/* readonly attribute nsIDOMSVGPreserveAspectRatio baseVal; */
NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::GetBaseVal(nsIDOMSVGPreserveAspectRatio** aBaseVal)
{
  *aBaseVal = mBaseVal;
  NS_ADDREF(*aBaseVal);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGPreserveAspectRatio animVal; */
NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::GetAnimVal(nsIDOMSVGPreserveAspectRatio** aAnimVal)
{
  *aAnimVal = mBaseVal;
  NS_ADDREF(*aAnimVal);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedPreserveAspectRatio::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGAnimatedPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio** result,
                                     nsIDOMSVGPreserveAspectRatio* baseVal)
{
  *result = nsnull;

  if (!baseVal) {
    NS_ERROR("need baseVal");
    return NS_ERROR_FAILURE;
  }

  nsSVGAnimatedPreserveAspectRatio* animatedPreserveAspectRatio = new nsSVGAnimatedPreserveAspectRatio();
  if (!animatedPreserveAspectRatio)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(animatedPreserveAspectRatio);
  animatedPreserveAspectRatio->Init(baseVal);
  *result = (nsIDOMSVGAnimatedPreserveAspectRatio*) animatedPreserveAspectRatio;

  return NS_OK;
}
