/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGLength.h"


////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedLength

class nsSVGAnimatedLength : public nsIDOMSVGAnimatedLength,
                            public nsSVGValue,
                            public nsISVGValueObserver,
                            public nsSupportsWeakReference
{
protected:
  friend nsresult NS_NewSVGAnimatedLength(nsIDOMSVGAnimatedLength** result,
                                          nsIDOMSVGLength* baseVal);
  nsSVGAnimatedLength();
  ~nsSVGAnimatedLength();
  void Init(nsIDOMSVGLength* baseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedLength interface:
  NS_DECL_NSIDOMSVGANIMATEDLENGTH

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  nsCOMPtr<nsIDOMSVGLength> mBaseVal;
};



//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedLength::nsSVGAnimatedLength()
{
}

nsSVGAnimatedLength::~nsSVGAnimatedLength()
{
  if (!mBaseVal) return;
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (!val) return;
  val->RemoveObserver(this);
}

void
nsSVGAnimatedLength::Init(nsIDOMSVGLength* baseVal)
{
  mBaseVal = baseVal;
  if (!mBaseVal) return;
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  NS_ASSERTION(val, "baseval needs to implement nsISVGValue interface");
  if (!val) return;
  val->AddObserver(this);
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedLength)
NS_IMPL_RELEASE(nsSVGAnimatedLength)


NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedLength)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedLength)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END
  
//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedLength::SetValueString(const nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  return value->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAnimatedLength::GetValueString(nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  return value->GetValueString(aValue);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedLength methods:

/* readonly attribute nsIDOMSVGLength baseVal; */
NS_IMETHODIMP
nsSVGAnimatedLength::GetBaseVal(nsIDOMSVGLength * *aBaseVal)
{
  *aBaseVal = mBaseVal;
  NS_ADDREF(*aBaseVal);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGLength animVal; */
NS_IMETHODIMP
nsSVGAnimatedLength::GetAnimVal(nsIDOMSVGLength * *aAnimVal)
{
  *aAnimVal = mBaseVal;
  NS_ADDREF(*aAnimVal);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAnimatedLength::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedLength::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions

nsresult
NS_NewSVGAnimatedLength(nsIDOMSVGAnimatedLength** aResult,
                        nsIDOMSVGLength* baseVal)
{
  *aResult = nsnull;
  
  nsSVGAnimatedLength* animatedLength = new nsSVGAnimatedLength();
  if(!animatedLength) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedLength);

//  nsCOMPtr<nsIDOMSVGLength> baseVal;
//  NS_NewSVGLength(getter_AddRefs(baseVal), owner);
  
  animatedLength->Init(baseVal);
  
  *aResult = (nsIDOMSVGAnimatedLength*) animatedLength;
  
  return NS_OK;
}
