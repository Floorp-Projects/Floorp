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

#include "nsSVGPointList.h"
#include "nsSVGPoint.h"
#include "nsDOMError.h"
#include "prdtoa.h"
#include "nsReadableUtils.h"
#include "nsTextFormatter.h"
#include "nsCRT.h"
#include "nsCOMArray.h"

nsresult
nsSVGPointList::Create(const nsAString& aValue,
                       nsISVGValue** aResult)
{
  *aResult = (nsISVGValue*) new nsSVGPointList();
  if(!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);

  (*aResult)->SetValueString(aValue);  
  return NS_OK;
}

nsresult
nsSVGPointList::Create(nsIDOMSVGPointList** aResult)
{
  *aResult = (nsIDOMSVGPointList*) new nsSVGPointList();
  if(!*aResult) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsSVGPointList::nsSVGPointList()
{
}

nsSVGPointList::~nsSVGPointList()
{
  ReleasePoints();
}

void
nsSVGPointList::ReleasePoints()
{
  WillModify();
  PRInt32 count = mPoints.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGPoint* point = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(point);
    if (val)
      val->RemoveObserver(this);
    NS_RELEASE(point);
  }
  mPoints.Clear();
  DidModify();
}

nsIDOMSVGPoint*
nsSVGPointList::ElementAt(PRInt32 index)
{
  return (nsIDOMSVGPoint*)mPoints.ElementAt(index);
}

void
nsSVGPointList::AppendElement(nsIDOMSVGPoint* aElement)
{
  WillModify();
  NS_ADDREF(aElement);
  mPoints.AppendElement((void*)aElement);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
  if (val)
    val->AddObserver(this);
  DidModify();
}

void
nsSVGPointList::RemoveElementAt(PRInt32 index)
{
  WillModify();
  nsIDOMSVGPoint* point = ElementAt(index);
  NS_ASSERTION(point, "null point");
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(point);
  if (val)
    val->RemoveObserver(this);
  mPoints.RemoveElementAt(index);
  NS_RELEASE(point);
  DidModify();
}

void
nsSVGPointList::InsertElementAt(nsIDOMSVGPoint* aElement, PRInt32 index)
{
  WillModify();
  NS_ADDREF(aElement);
  mPoints.InsertElementAt((void*)aElement, index);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
  if (val)
    val->AddObserver(this);
  DidModify();
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGPointList)
NS_IMPL_RELEASE(nsSVGPointList)

NS_INTERFACE_MAP_BEGIN(nsSVGPointList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPointList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPointList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGPointList::SetValueString(const nsAString& aValue)
{
  nsresult rv = NS_OK;

  char* str = ToNewCString(aValue);
  
  char* rest = str;
  char* token1;
  char* token2;
  const char* delimiters = ",\x20\x9\xD\xA";
  nsCOMArray<nsIDOMSVGPoint> points;
  
  while ( (token1 = nsCRT::strtok(rest, delimiters, &rest)) &&
          (token2 = nsCRT::strtok(rest, delimiters, &rest)) ) {

    char *end;
    
    double x = PR_strtod(token1, &end);
    if (*end != '\0') {
      rv = NS_ERROR_FAILURE;
      break; // parse error
    }
    double y = PR_strtod(token2, &end);
    if (*end != '\0') {
      rv = NS_ERROR_FAILURE;
      break; // parse error
    }
    
    nsCOMPtr<nsIDOMSVGPoint> point;
    nsSVGPoint::Create((float)x, (float)y, getter_AddRefs(point));
    if (!point) {
      rv = NS_ERROR_OUT_OF_MEMORY;
      break;
    }
    points.AppendObject(point);
  }

  if (token1 || NS_FAILED(rv)) {
    // there was a parse error. 
    rv = NS_ERROR_FAILURE;
  }
  else {
    WillModify();
    ReleasePoints();
    PRInt32 count = points.Count();
    for (PRInt32 i=0; i<count; ++i) {
      AppendElement(points.ObjectAt(i));
    }
    DidModify();
  }

  nsMemory::Free(str);
  
  return rv;
}

NS_IMETHODIMP
nsSVGPointList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mPoints.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;
  PRUnichar buf[48];
  
  while (1) {
    nsIDOMSVGPoint* point = ElementAt(i);
    float x, y;
    point->GetX(&x);
    point->GetY(&y);
    
    nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar), NS_LITERAL_STRING("%g,%g").get(), (double)x, (double)y);
    aValue.Append(buf);

    if (++i >= count) break;

    aValue.AppendLiteral(" ");
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPointList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGPointList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mPoints.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGPointList::Clear()
{
  WillModify();
  ReleasePoints();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGPoint initialize (in nsIDOMSVGPoint newItem); */
NS_IMETHODIMP nsSVGPointList::Initialize(nsIDOMSVGPoint *newItem, nsIDOMSVGPoint **_retval)
{
  Clear();
  return AppendItem(newItem, _retval);
}

/* nsIDOMSVGPoint getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPointList::GetItem(PRUint32 index, nsIDOMSVGPoint **_retval)
{
  if ((PRInt32)index >= mPoints.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGPoint insertItemBefore (in nsIDOMSVGPoint newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPointList::InsertItemBefore(nsIDOMSVGPoint *newItem, PRUint32 index, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPoint replaceItem (in nsIDOMSVGPoint newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPointList::ReplaceItem(nsIDOMSVGPoint *newItem, PRUint32 index, nsIDOMSVGPoint **_retval)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPoint removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPointList::RemoveItem(PRUint32 index, nsIDOMSVGPoint **_retval)
{
  if ((PRInt32)index >= mPoints.Count()) {
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

/* nsIDOMSVGPoint appendItem (in nsIDOMSVGPoint newItem); */
NS_IMETHODIMP nsSVGPointList::AppendItem(nsIDOMSVGPoint *newItem, nsIDOMSVGPoint **_retval)
{
  // XXX The SVG specs state that 'if newItem is already in a list, it
  // is removed from its previous list before it is inserted into this
  // list'. We don't do that. Should we?
  
  *_retval = newItem;
  NS_ADDREF(*_retval);
  AppendElement(newItem);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGPointList::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPointList::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}
