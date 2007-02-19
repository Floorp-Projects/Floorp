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

#include "nsSVGLengthList.h"
#include "nsSVGLength.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsDOMError.h"
#include "nsReadableUtils.h"
#include "nsSVGCoordCtx.h"
#include "nsCRT.h"
#include "nsISVGValueUtils.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGLengthList

class nsSVGLengthList : public nsISVGLengthList,
                        public nsSVGValue,
                        public nsISVGValueObserver
{  
protected:
  friend nsresult NS_NewSVGLengthList(nsISVGLengthList** result);

  nsSVGLengthList();
  ~nsSVGLengthList();
//  void Init();
  
public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGLengthList interface:
  NS_DECL_NSIDOMSVGLENGTHLIST

  // nsISVGLengthList interface:
  NS_IMETHOD SetContext(nsSVGCoordCtx* context);
  
  // remainder of nsISVGValue interface:
  NS_IMETHOD SetValueString(const nsAString& aValue);
  NS_IMETHOD GetValueString(nsAString& aValue);

  // nsISVGValueObserver interface:
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     modificationType aModType);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

protected:
  // implementation helpers:
  nsISVGLength* ElementAt(PRInt32 index);
  void AppendElement(nsISVGLength* aElement);
  void RemoveElementAt(PRInt32 index);
  void InsertElementAt(nsISVGLength* aElement, PRInt32 index);
  
  void ReleaseLengths();
  
  nsAutoVoidArray mLengths;
  nsRefPtr<nsSVGCoordCtx> mContext;
};


//----------------------------------------------------------------------
// Implementation

nsSVGLengthList::nsSVGLengthList()
{
}

nsSVGLengthList::~nsSVGLengthList()
{
  ReleaseLengths();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGLengthList)
NS_IMPL_RELEASE(nsSVGLengthList)

NS_INTERFACE_MAP_BEGIN(nsSVGLengthList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLengthList)
  NS_INTERFACE_MAP_ENTRY(nsISVGLengthList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGLengthList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGLengthList::SetValueString(const nsAString& aValue)
{
  WillModify();
  
  ReleaseLengths();

  nsresult rv = NS_OK;

  char* str;
  str = ToNewCString(aValue);

  char* rest = str;
  char* token;
  const char* delimiters = ",\x20\x9\xD\xA";

  while ((token = nsCRT::strtok(rest, delimiters, &rest))) {
    nsCOMPtr<nsISVGLength> length;
    NS_NewSVGLength(getter_AddRefs(length), NS_ConvertASCIItoUTF16(token));
    if (!length) {
      rv = NS_ERROR_FAILURE;
      break;
    }
    AppendElement(length);
  }
  
  nsMemory::Free(str);
  
  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGLengthList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mLengths.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;
  
  while (1) {
    nsISVGLength* length = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(length);
    NS_ASSERTION(val, "length doesn't implement required interface");
    if (!val) continue;
    nsAutoString str;
    val->GetValueString(str);
    aValue.Append(str);

    if (++i >= count) break;

    aValue.AppendLiteral(" ");
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGLengthList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGLengthList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mLengths.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGLengthList::Clear()
{
  WillModify();
  ReleaseLengths();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGLength initialize (in nsIDOMSVGLength newItem); */
NS_IMETHODIMP nsSVGLengthList::Initialize(nsIDOMSVGLength *newItem,
                                          nsIDOMSVGLength **_retval)
{
  if (!newItem) {
    *_retval = nsnull;
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  Clear();
  return AppendItem(newItem, _retval);
}

/* nsIDOMSVGLength getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGLengthList::GetItem(PRUint32 index, nsIDOMSVGLength **_retval)
{
  if ((PRInt32)index >= mLengths.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGLength insertItemBefore (in nsIDOMSVGLength newItem, in unsigned long index); */
NS_IMETHODIMP
nsSVGLengthList::InsertItemBefore(nsIDOMSVGLength *newItem,
                                  PRUint32 index,
                                  nsIDOMSVGLength **_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!newItem)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGLengthList::InsertItemBefore");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGLength replaceItem (in nsIDOMSVGLength newItem, in unsigned long index); */
NS_IMETHODIMP
nsSVGLengthList::ReplaceItem(nsIDOMSVGLength *newItem,
                             PRUint32 index,
                             nsIDOMSVGLength **_retval)
{
  // null check when implementing - this method can be used by scripts!
  // if (!newItem)
  //   return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  NS_NOTYETIMPLEMENTED("nsSVGLengthList::ReplaceItem");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGLengthList removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGLengthList::RemoveItem(PRUint32 index, nsIDOMSVGLength **_retval)
{
  if ((PRInt32)index >= mLengths.Count()) {
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

/* nsIDOMSVGLengthList appendItem (in nsIDOMSVGLengthList newItem); */
NS_IMETHODIMP
nsSVGLengthList::AppendItem(nsIDOMSVGLength *newItem, nsIDOMSVGLength **_retval)
{
  nsCOMPtr<nsISVGLength> length = do_QueryInterface(newItem);
  if (!length) {
    *_retval = nsnull;
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  AppendElement(length);

  *_retval = newItem;
  NS_ADDREF(*_retval);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGLengthList methods:

NS_IMETHODIMP
nsSVGLengthList::SetContext(nsSVGCoordCtx *context)
{
  mContext = context;

  PRInt32 count = mLengths.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsISVGLength* length = ElementAt(i);
    length->SetContext(mContext);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGLengthList::WillModifySVGObservable(nsISVGValue* observable,
                                         modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGLengthList::DidModifySVGObservable(nsISVGValue* observable,
                                        modificationType aModType)
{
  DidModify(aModType);
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGLengthList::ReleaseLengths()
{
  WillModify();
  PRInt32 count = mLengths.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsISVGLength* length = ElementAt(i);
    length->SetContext(nsnull);
    NS_REMOVE_SVGVALUE_OBSERVER(length);
    NS_RELEASE(length);
  }
  mLengths.Clear();
  DidModify();
}

nsISVGLength*
nsSVGLengthList::ElementAt(PRInt32 index)
{
  return (nsISVGLength*)mLengths.ElementAt(index);
}

void
nsSVGLengthList::AppendElement(nsISVGLength* aElement)
{
  WillModify();
  NS_ADDREF(aElement);
  
  // The SVG specs state that 'if newItem is already in a list, it
  // is removed from its previous list before it is inserted into this
  // list':
  //  aElement->SetListOwner(this);
  
  aElement->SetContext(mContext);
  mLengths.AppendElement((void*)aElement);
  NS_ADD_SVGVALUE_OBSERVER(aElement);
  DidModify();
}

void
nsSVGLengthList::RemoveElementAt(PRInt32 index)
{
  WillModify();
  nsISVGLength* length = ElementAt(index);
  NS_ASSERTION(length, "null length");
  NS_REMOVE_SVGVALUE_OBSERVER(length);
  mLengths.RemoveElementAt(index);
  NS_RELEASE(length);
  DidModify();
}

void
nsSVGLengthList::InsertElementAt(nsISVGLength* aElement, PRInt32 index)
{
  WillModify();
  NS_ADDREF(aElement);

  // The SVG specs state that 'if newItem is already in a list, it
  // is removed from its previous list before it is inserted into this
  // list':
  //  aElement->SetListOwner(this);
  
  aElement->SetContext(mContext);
  
  mLengths.InsertElementAt((void*)aElement, index);
  NS_ADD_SVGVALUE_OBSERVER(aElement);
  DidModify();
}


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGLengthList(nsISVGLengthList** result)
{
  *result = nsnull;
  
  nsSVGLengthList* lengthList = new nsSVGLengthList();
  if (!lengthList) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(lengthList);

  *result = lengthList;
  
  return NS_OK;
}

