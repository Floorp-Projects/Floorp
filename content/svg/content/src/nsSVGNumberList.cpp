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
 * Scooter Morris
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
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

#include "nsSVGNumberList.h"
#include "nsSVGNumber.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsDOMError.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsISVGValueUtils.h"
#include "prdtoa.h"

////////////////////////////////////////////////////////////////////////
// nsSVGNumberList

class nsSVGNumberList : public nsIDOMSVGNumberList,
                        public nsSVGValue,
                        public nsISVGValueObserver,
                        public nsSupportsWeakReference
{  
protected:
  friend nsresult NS_NewSVGNumberList(nsIDOMSVGNumberList** result);

  nsSVGNumberList();
  ~nsSVGNumberList();
//  void Init();
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGNumberList interface:
  NS_DECL_NSIDOMSVGNUMBERLIST

  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:
  // implementation helpers:
  nsIDOMSVGNumber* ElementAt(PRInt32 index);
  void AppendElement(nsIDOMSVGNumber* aElement);
  void RemoveElementAt(PRInt32 index);
  nsresult InsertElementAt(nsIDOMSVGNumber* aElement, PRInt32 index);
  
  void ReleaseNumbers();
  
  nsAutoVoidArray mNumbers;
};


//----------------------------------------------------------------------
// Implementation

nsSVGNumberList::nsSVGNumberList()
{
}

nsSVGNumberList::~nsSVGNumberList()
{
  ReleaseNumbers();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGNumberList)
NS_IMPL_RELEASE(nsSVGNumberList)

NS_INTERFACE_MAP_BEGIN(nsSVGNumberList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGNumberList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGNumberList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGNumberList::SetValueString(const nsAString& aValue)
{
  WillModify();
  ReleaseNumbers();

  nsresult rv = NS_OK;

  char* str;
  str = ToNewCString(aValue);

  char* rest = str;
  char* token;
  const char* delimiters = ", \t\r\n";

  while ((token = nsCRT::strtok(rest, delimiters, &rest))) {
    char *left;
    double value = PR_strtod(token, &left);
    if (token!=left) {
      nsCOMPtr<nsIDOMSVGNumber> number;
      NS_NewSVGNumber(getter_AddRefs(number), value);
      if (!number) {
        rv = NS_ERROR_FAILURE;
        break;
      }
      AppendElement(number);
    }
  }
  
  nsMemory::Free(str);
  
  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGNumberList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mNumbers.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;
  
  while (1) {
    nsIDOMSVGNumber* number = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(number);
    NS_ASSERTION(val, "number doesn't implement required interface");
    if (!val) continue;
    nsAutoString str;
    val->GetValueString(str);
    aValue.Append(str);

    if (++i >= count) break;

    aValue.Append(NS_LITERAL_STRING(" "));
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGNumberList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGNumberList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mNumbers.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGNumberList::Clear()
{
  WillModify();
  ReleaseNumbers();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGNumber initialize (in nsIDOMSVGNumber newItem); */
NS_IMETHODIMP nsSVGNumberList::Initialize(nsIDOMSVGNumber *newItem, nsIDOMSVGNumber **_retval)
{
  Clear();
  return AppendItem(newItem, _retval);
}

/* nsIDOMSVGNumber getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGNumberList::GetItem(PRUint32 index, nsIDOMSVGNumber **_retval)
{
  if ((PRInt32)index >= mNumbers.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGNumber insertItemBefore (in nsIDOMSVGNumber newItem, in unsigned long index); */
NS_IMETHODIMP
nsSVGNumberList::InsertItemBefore(nsIDOMSVGNumber *newItem, PRUint32 index, nsIDOMSVGNumber **_retval)
{
  *_retval = newItem;

  if (!newItem) return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  PRInt32 idx = index;
  PRInt32 count = mNumbers.Count();

  if (!InsertElementAt(newItem, (idx < count)? idx: count)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGNumber replaceItem (in nsIDOMSVGNumber newItem, in unsigned long index); */
NS_IMETHODIMP
nsSVGNumberList::ReplaceItem(nsIDOMSVGNumber *newItem, PRUint32 index, nsIDOMSVGNumber **_retval)
{
  nsresult rv = RemoveItem(index, _retval);
  if (NS_FAILED(rv))
    return rv;

  return InsertElementAt(newItem, index);
}

/* nsIDOMSVGNumberList removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGNumberList::RemoveItem(PRUint32 index, nsIDOMSVGNumber **_retval)
{
  if ((PRInt32)index >= mNumbers.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval = ElementAt(index);
  NS_ADDREF(*_retval);
  WillModify();
  RemoveElementAt(index);
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGNumberList appendItem (in nsIDOMSVGNumberList newItem); */
NS_IMETHODIMP
nsSVGNumberList::AppendItem(nsIDOMSVGNumber *newItem, nsIDOMSVGNumber **_retval)
{
  nsCOMPtr<nsIDOMSVGNumber> number = do_QueryInterface(newItem);
  if (!number) {
    NS_ERROR("number doesn't implement required interface");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }
  AppendElement(number);

  *_retval = newItem;
  NS_ADDREF(*_retval);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGNumberList::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGNumberList::DidModifySVGObservable(nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGNumberList::ReleaseNumbers()
{
  WillModify();
  PRInt32 count = mNumbers.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGNumber* number = ElementAt(i);
    NS_REMOVE_SVGVALUE_OBSERVER(number);
    NS_RELEASE(number);
  }
  mNumbers.Clear();
  DidModify();
}

nsIDOMSVGNumber*
nsSVGNumberList::ElementAt(PRInt32 index)
{
  return (nsIDOMSVGNumber*)mNumbers.ElementAt(index);
}

void
nsSVGNumberList::AppendElement(nsIDOMSVGNumber* aElement)
{
  WillModify();
  NS_ADDREF(aElement);
  
  // The SVG specs state that 'if newItem is already in a list, it
  // is removed from its previous list before it is inserted into this
  // list':
  //  aElement->SetListOwner(this);
  
  mNumbers.AppendElement((void*)aElement);
  NS_ADD_SVGVALUE_OBSERVER(aElement);
  DidModify();
}

void
nsSVGNumberList::RemoveElementAt(PRInt32 index)
{
  WillModify();
  nsIDOMSVGNumber* number = ElementAt(index);
  NS_ASSERTION(number, "null number");
  NS_REMOVE_SVGVALUE_OBSERVER(number);
  mNumbers.RemoveElementAt(index);
  NS_RELEASE(number);
  DidModify();
}

nsresult
nsSVGNumberList::InsertElementAt(nsIDOMSVGNumber* aElement, PRInt32 index)
{
  nsresult rv;
  WillModify();
  NS_ADDREF(aElement);

  // The SVG specs state that 'if newItem is already in a list, it
  // is removed from its previous list before it is inserted into this
  // list':
  //  aElement->SetListOwner(this);
  
  if (!NS_FAILED(rv = mNumbers.InsertElementAt((void*)aElement, index)))
    NS_ADD_SVGVALUE_OBSERVER(aElement);
  DidModify();
  return rv;
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGNumberList(nsIDOMSVGNumberList** result)
{
  *result = nsnull;
  
  nsSVGNumberList* numberList = new nsSVGNumberList();
  if (!numberList) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(numberList);

  *result = numberList;
  
  return NS_OK;
}


