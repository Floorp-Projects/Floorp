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

#include "nsSVGTransformList.h"
#include "nsSVGTransformListParser.h"
#include "nsSVGTransform.h"
#include "nsSVGMatrix.h"
#include "nsDOMError.h"
#include "prdtoa.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "nsIDOMClassInfo.h"

nsresult
nsSVGTransformList::Create(nsIDOMSVGTransformList** aResult)
{
  *aResult = new nsSVGTransformList();
  if (!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGTransformList::nsSVGTransformList()
{
}

nsSVGTransformList::~nsSVGTransformList()
{
  ReleaseTransforms();
}

void
nsSVGTransformList::ReleaseTransforms()
{
  PRInt32 count = mTransforms.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGTransform* transform = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(transform);
    if (val)
      val->RemoveObserver(this);
    NS_RELEASE(transform);
  }
  mTransforms.Clear();
}

nsIDOMSVGTransform*
nsSVGTransformList::ElementAt(PRInt32 index)
{
  return (nsIDOMSVGTransform*)mTransforms.ElementAt(index);
}

PRBool
nsSVGTransformList::AppendElement(nsIDOMSVGTransform* aElement)
{
  PRBool rv = mTransforms.AppendElement((void*)aElement);
  if (rv) {
    NS_ADDREF(aElement);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
    if (val)
      val->AddObserver(this);
  }

  return rv;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGTransformList::GetConsolidationMatrix(nsIDOMSVGTransformList *transforms)
{
  PRUint32 count;
  transforms->GetNumberOfItems(&count);

  if (!count)
    return nsnull;

  nsCOMPtr<nsIDOMSVGTransform> transform;
  nsCOMPtr<nsIDOMSVGMatrix> conmatrix;

  // single transform common case - shortcut
  if (count == 1) {
    transforms->GetItem(0, getter_AddRefs(transform));
    transform->GetMatrix(getter_AddRefs(conmatrix));
  } else {
    nsresult rv = NS_NewSVGMatrix(getter_AddRefs(conmatrix));
    NS_ENSURE_SUCCESS(rv, nsnull);
    
    nsCOMPtr<nsIDOMSVGMatrix> temp1, temp2;
    
    for (PRInt32 i = 0; i < count; ++i) {
      transforms->GetItem(i, getter_AddRefs(transform));
      transform->GetMatrix(getter_AddRefs(temp1));
      conmatrix->Multiply(temp1, getter_AddRefs(temp2));
      if (!temp2)
        return nsnull;
      conmatrix = temp2;
    }
  }

  nsIDOMSVGMatrix *_retval = nsnull;
  conmatrix.swap(_retval);
  return _retval;
}

//----------------------------------------------------------------------
// XPConnect interface list
NS_CLASSINFO_MAP_BEGIN(SVGTransformList)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMSVGTransformList)
NS_CLASSINFO_MAP_END

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGTransformList)
NS_IMPL_RELEASE(nsSVGTransformList)

NS_INTERFACE_MAP_BEGIN(nsSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransformList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGTransformList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGTransformList::SetValueString(const nsAString& aValue)
{
  // XXX: we don't implement the _exact_ BNF given in the
  // specs.

  nsresult rv = NS_OK;

  // parse transform attribute value
  nsCOMArray<nsIDOMSVGTransform> xforms;
  nsSVGTransformListParser parser(&xforms);
  rv = parser.Parse(aValue);

  if (NS_FAILED(rv)) {
    // there was a parse error.
    rv = NS_ERROR_FAILURE;
  }
  else {
    WillModify();
    ReleaseTransforms();
    PRInt32 count = xforms.Count();
    for (PRInt32 i=0; i<count; ++i) {
      AppendElement(xforms.ObjectAt(i));
    }
    DidModify();
  }

  return rv;
}

NS_IMETHODIMP
nsSVGTransformList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mTransforms.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;
  
  while (1) {
    nsIDOMSVGTransform* transform = ElementAt(i);

    nsCOMPtr<nsISVGValue> val = do_QueryInterface(transform);
    nsAutoString str;
    val->GetValueString(str);
    aValue.Append(str);

    if (++i >= count) break;

    aValue.AppendLiteral(" ");
  }
  
  return NS_OK;

}

//----------------------------------------------------------------------
// nsIDOMSVGTransformList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGTransformList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mTransforms.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGTransformList::Clear()
{
  WillModify();
  ReleaseTransforms();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGTransform initialize (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP nsSVGTransformList::Initialize(nsIDOMSVGTransform *newItem,
                                             nsIDOMSVGTransform **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsSVGValueAutoNotifier autonotifier(this);

  ReleaseTransforms();
  if (!AppendElement(newItem)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::GetItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  if (index >= NS_STATIC_CAST(PRUint32, mTransforms.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform insertItemBefore (in nsIDOMSVGTransform newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::InsertItemBefore(nsIDOMSVGTransform *newItem,
                                                   PRUint32 index,
                                                   nsIDOMSVGTransform **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsSVGValueAutoNotifier autonotifier(this);

  PRUint32 count = mTransforms.Count();

  if (!mTransforms.InsertElementAt((void*)newItem, (index < count)? index: count)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(newItem);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(newItem);
  if (val)
    val->AddObserver(this);

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform replaceItem (in nsIDOMSVGTransform newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::ReplaceItem(nsIDOMSVGTransform *newItem,
                                              PRUint32 index,
                                              nsIDOMSVGTransform **_retval)
{
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  *_retval = nsnull;

  nsSVGValueAutoNotifier autonotifier(this);

  if (index >= PRUint32(mTransforms.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  nsIDOMSVGTransform* oldItem = ElementAt(index);

  if (!mTransforms.ReplaceElementAt((void*)newItem, index)) {
    NS_NOTREACHED("removal of element failed");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISVGValue> val = do_QueryInterface(oldItem);
  if (val)
    val->RemoveObserver(this);
  NS_RELEASE(oldItem);
  val = do_QueryInterface(newItem);
  if (val)
    val->AddObserver(this);
  NS_ADDREF(newItem);

  *_retval = newItem;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGTransformList::RemoveItem(PRUint32 index, nsIDOMSVGTransform **_retval)
{
  nsSVGValueAutoNotifier autonotifier(this);

  if (index >= NS_STATIC_CAST(PRUint32, mTransforms.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval = ElementAt(index);

  if (!mTransforms.RemoveElementAt(index)) {
    NS_NOTREACHED("removal of element failed");
    *_retval = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsISVGValue> val = do_QueryInterface(*_retval);
  if (val)
    val->RemoveObserver(this);

  // don't NS_ADDREF(*_retval)
  return NS_OK;
}

/* nsIDOMSVGTransform appendItem (in nsIDOMSVGTransform newItem); */
NS_IMETHODIMP nsSVGTransformList::AppendItem(nsIDOMSVGTransform *newItem,
                                             nsIDOMSVGTransform **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsSVGValueAutoNotifier autonotifier(this);

  if (!AppendElement(newItem)) {
    *_retval = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGTransform createSVGTransformFromMatrix (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
nsSVGTransformList::CreateSVGTransformFromMatrix(nsIDOMSVGMatrix *matrix,
                                                 nsIDOMSVGTransform **_retval)
{
  if (!matrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  nsresult rv = NS_NewSVGTransform(_retval);
  if (NS_FAILED(rv))
    return rv;

  (*_retval)->SetMatrix(matrix);
  return NS_OK;
}

/* nsIDOMSVGTransform consolidate (); */
NS_IMETHODIMP nsSVGTransformList::Consolidate(nsIDOMSVGTransform **_retval)
{
  // Note we don't want WillModify/DidModify since nothing really changes

  *_retval = nsnull;

  PRInt32 count = mTransforms.Count();
  if (count==0) return NS_OK;
  if (count==1) {
    *_retval = ElementAt(0);
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMSVGMatrix> conmatrix = GetConsolidationMatrix(this);
  if (!conmatrix)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIDOMSVGTransform> consolidation;
  nsresult rv = CreateSVGTransformFromMatrix(conmatrix,
                                             getter_AddRefs(consolidation));
  if (NS_FAILED(rv))
    return rv;

  ReleaseTransforms();
  if (!AppendElement(consolidation))
    return NS_ERROR_OUT_OF_MEMORY;

  *_retval = consolidation;
  NS_ADDREF(*_retval);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGTransformList::WillModifySVGObservable(nsISVGValue* observable,
                                            modificationType aModType)
{
  WillModify(aModType);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTransformList::DidModifySVGObservable (nsISVGValue* observable,
                                            modificationType aModType)
{
  DidModify(aModType);
  return NS_OK;
}
