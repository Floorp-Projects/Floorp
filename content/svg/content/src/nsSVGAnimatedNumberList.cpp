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
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGAnimatedNumberList.h"
#include "nsSVGNumberList.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGAnimatedNumberList

class nsSVGAnimatedNumberList : public nsIDOMSVGAnimatedNumberList,
                                public nsSVGValue,
                                public nsISVGValueObserver
{  
protected:
  friend nsresult
  NS_NewSVGAnimatedNumberList(nsIDOMSVGAnimatedNumberList** result,
                              nsIDOMSVGNumberList* aBaseVal);

  nsSVGAnimatedNumberList();
  ~nsSVGAnimatedNumberList();
  nsresult Init(nsIDOMSVGNumberList* aBaseVal);
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGAnimatedNumberList interface:
  NS_DECL_NSIDOMSVGANIMATEDNUMBERLIST

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
  nsCOMPtr<nsIDOMSVGNumberList> mBaseVal;
};


//----------------------------------------------------------------------
// Implementation

nsSVGAnimatedNumberList::nsSVGAnimatedNumberList()
{
}

nsSVGAnimatedNumberList::~nsSVGAnimatedNumberList()
{
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (val)
    val->RemoveObserver(this);
}

nsresult
nsSVGAnimatedNumberList::Init(nsIDOMSVGNumberList* aBaseVal)
{
  mBaseVal = aBaseVal;
  if (!mBaseVal) return NS_ERROR_FAILURE;
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(mBaseVal);
  if (!val) return NS_ERROR_FAILURE;
  val->AddObserver(this);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGAnimatedNumberList)
NS_IMPL_RELEASE(nsSVGAnimatedNumberList)

NS_INTERFACE_MAP_BEGIN(nsSVGAnimatedNumberList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedNumberList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGAnimatedNumberList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGAnimatedNumberList::SetValueString(const nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  NS_ASSERTION(value, "value doesn't support SVGValue interfaces");
  return value->SetValueString(aValue);
}

NS_IMETHODIMP
nsSVGAnimatedNumberList::GetValueString(nsAString& aValue)
{
  nsCOMPtr<nsISVGValue> value = do_QueryInterface(mBaseVal);
  NS_ASSERTION(value, "value doesn't support SVGValue interfaces");
  return value->GetValueString(aValue);
}

//----------------------------------------------------------------------
// nsIDOMSVGAnimatedNumberList methods:

/* readonly attribute nsIDOMSVGNumberList aBaseVal; */
NS_IMETHODIMP
nsSVGAnimatedNumberList::GetBaseVal(nsIDOMSVGNumberList * *aBaseVal)
{
  *aBaseVal = mBaseVal;
  NS_ADDREF(*aBaseVal);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGNumberList animVal; */
NS_IMETHODIMP
nsSVGAnimatedNumberList::GetAnimVal(nsIDOMSVGNumberList * *aAnimVal)
{
  *aAnimVal = mBaseVal;
  NS_ADDREF(*aAnimVal);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGAnimatedNumberList::WillModifySVGObservable(nsISVGValue* observable,
                                                 modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGAnimatedNumberList::DidModifySVGObservable (nsISVGValue* observable,
                                                 modificationType aModType)
{
  DidModify(aModType);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGAnimatedNumberList(nsIDOMSVGAnimatedNumberList** result,
                            nsIDOMSVGNumberList* aBaseVal)
{
  *result = nsnull;
  
  nsSVGAnimatedNumberList* animatedNumberList = new nsSVGAnimatedNumberList();
  if (!animatedNumberList) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(animatedNumberList);

  if (NS_FAILED(animatedNumberList->Init(aBaseVal))) {
    NS_RELEASE(animatedNumberList);
    return NS_ERROR_FAILURE;
  }
  
  *result = (nsIDOMSVGAnimatedNumberList*) animatedNumberList;
  
  return NS_OK;
}


