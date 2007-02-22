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

#include "nsSVGPathSegList.h"
#include "nsSVGPathSeg.h"
#include "nsSVGValue.h"
#include "nsWeakReference.h"
#include "nsVoidArray.h"
#include "nsCOMArray.h"
#include "nsDOMError.h"
#include "nsSVGPathDataParser.h"
#include "nsReadableUtils.h"
#include "nsContentUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegList

class nsSVGPathSegList : public nsIDOMSVGPathSegList,
                         public nsSVGValue,
                         public nsISVGValueObserver
{  
protected:
  friend nsresult NS_NewSVGPathSegList(nsIDOMSVGPathSegList** result);

  nsSVGPathSegList();
  ~nsSVGPathSegList();
//  void Init();

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsIDOMSVGPathSegList interface:
  NS_DECL_NSIDOMSVGPATHSEGLIST

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
  // implementation helpers:
  void AppendElement(nsIDOMSVGPathSeg* aElement);
  void RemoveElementAt(PRInt32 index);
  void InsertElementAt(nsIDOMSVGPathSeg* aElement, PRInt32 index);
  void RemoveFromCurrentList(nsIDOMSVGPathSeg*);

  void ReleaseSegments(PRBool aModify = PR_TRUE);

  nsCOMArray<nsIDOMSVGPathSeg> mSegments;
};


//----------------------------------------------------------------------
// Implementation

nsSVGPathSegList::nsSVGPathSegList()
{
}

nsSVGPathSegList::~nsSVGPathSegList()
{
  PRInt32 count = mSegments.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, mSegments.ObjectAt(i));
    seg->SetCurrentList(nsnull);
  }
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(nsSVGPathSegList)
NS_IMPL_RELEASE(nsSVGPathSegList)

NS_INTERFACE_MAP_BEGIN(nsSVGPathSegList)
  NS_INTERFACE_MAP_ENTRY(nsISVGValue)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathSegList)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPathSegList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISVGValue)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGValue methods:

NS_IMETHODIMP
nsSVGPathSegList::SetValueString(const nsAString& aValue)
{
  nsresult rv;
  
  WillModify();
  ReleaseSegments(PR_FALSE);
  nsSVGPathDataParserToDOM parser(&mSegments);
  rv = parser.Parse(aValue);

  PRInt32 count = mSegments.Count();
  for (PRInt32 i=0; i<count; ++i) {
    nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, mSegments.ObjectAt(i));
    seg->SetCurrentList(this);
  }

  if (NS_FAILED(rv)) {
    NS_ERROR("path data parse error!");
    ReleaseSegments(PR_FALSE);
  }
  DidModify();
  return rv;
}

NS_IMETHODIMP
nsSVGPathSegList::GetValueString(nsAString& aValue)
{
  aValue.Truncate();

  PRInt32 count = mSegments.Count();

  if (count<=0) return NS_OK;

  PRInt32 i = 0;

  while (1) {
    nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, mSegments.ObjectAt(i));

    nsAutoString str;
    seg->GetValueString(str);
    aValue.Append(str);

    if (++i >= count) break;

    aValue.AppendLiteral(" ");
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPathSegList methods:

/* readonly attribute unsigned long numberOfItems; */
NS_IMETHODIMP nsSVGPathSegList::GetNumberOfItems(PRUint32 *aNumberOfItems)
{
  *aNumberOfItems = mSegments.Count();
  return NS_OK;
}

/* void clear (); */
NS_IMETHODIMP nsSVGPathSegList::Clear()
{
  WillModify();
  ReleaseSegments();
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGPathSeg initialize (in nsIDOMSVGPathSeg newItem); */
NS_IMETHODIMP nsSVGPathSegList::Initialize(nsIDOMSVGPathSeg *newItem,
                                           nsIDOMSVGPathSeg **_retval)
{
  if (!newItem) {
    *_retval = nsnull;
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }
  Clear();
  return AppendItem(newItem, _retval);
}

/* nsIDOMSVGPathSeg getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::GetItem(PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  if (index >= NS_STATIC_CAST(PRUint32, mSegments.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = mSegments.ObjectAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGPathSeg insertItemBefore (in nsIDOMSVGPathSeg newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::InsertItemBefore(nsIDOMSVGPathSeg *newItem,
                                                 PRUint32 index,
                                                 nsIDOMSVGPathSeg **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  if (index >= NS_STATIC_CAST(PRUint32, mSegments.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  InsertElementAt(newItem, index);
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* nsIDOMSVGPathSeg replaceItem (in nsIDOMSVGPathSeg newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::ReplaceItem(nsIDOMSVGPathSeg *newItem,
                                            PRUint32 index,
                                            nsIDOMSVGPathSeg **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  if (index >= NS_STATIC_CAST(PRUint32, mSegments.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  InsertElementAt(newItem, index);
  RemoveElementAt(index+1);
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* nsIDOMSVGPathSeg removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::RemoveItem(PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  if (index >= NS_STATIC_CAST(PRUint32, mSegments.Count())) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval = mSegments.ObjectAt(index);
  NS_ADDREF(*_retval);
  WillModify();
  RemoveElementAt(index);
  DidModify();
  return NS_OK;
}

/* nsIDOMSVGPathSeg appendItem (in nsIDOMSVGPathSeg newItem); */
NS_IMETHODIMP nsSVGPathSegList::AppendItem(nsIDOMSVGPathSeg *newItem,
                                           nsIDOMSVGPathSeg **_retval)
{
  *_retval = newItem;
  if (!newItem)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  AppendElement(newItem);
  NS_ADDREF(*_retval);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGValueObserver methods

NS_IMETHODIMP
nsSVGPathSegList::WillModifySVGObservable(nsISVGValue* observable,
                                          modificationType aModType)
{
  // This method is not yet used

  NS_NOTYETIMPLEMENTED("nsSVGPathSegList::WillModifySVGObservable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGPathSegList::DidModifySVGObservable (nsISVGValue* observable,
                                          modificationType aModType)
{
  // This method is not yet used

  NS_NOTYETIMPLEMENTED("nsSVGPathSegList::DidModifySVGObservable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGPathSegList::ReleaseSegments(PRBool aModify)
{
  if (aModify) {
    WillModify();
  }
  PRInt32 count = mSegments.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, mSegments.ObjectAt(i));
    seg->SetCurrentList(nsnull);
  }
  mSegments.Clear();
  if (aModify) {
    DidModify();
  }
}

void
nsSVGPathSegList::AppendElement(nsIDOMSVGPathSeg* aElement)
{
  WillModify();
  // XXX: we should only remove an item from its current list if we
  // successfully added it to this list
  RemoveFromCurrentList(aElement);
  mSegments.AppendObject(aElement);
  nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, aElement);
  seg->SetCurrentList(this);
  DidModify();
}

void
nsSVGPathSegList::RemoveElementAt(PRInt32 index)
{
  WillModify();
  nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, mSegments.ObjectAt(index));
  seg->SetCurrentList(nsnull);
  mSegments.RemoveObjectAt(index);
  DidModify();
}

void
nsSVGPathSegList::InsertElementAt(nsIDOMSVGPathSeg* aElement, PRInt32 index)
{
  WillModify();
  // XXX: we should only remove an item from its current list if we
  // successfully added it to this list
  RemoveFromCurrentList(aElement);
  mSegments.InsertObjectAt(aElement, index);
  nsSVGPathSeg* seg = NS_STATIC_CAST(nsSVGPathSeg*, aElement);
  seg->SetCurrentList(this);
  DidModify();
}

// The SVG specs state that 'if newItem is already in a list, it
// is removed from its previous list before it is inserted into this
// list'.  This is a helper function to do that.
void 
nsSVGPathSegList::RemoveFromCurrentList(nsIDOMSVGPathSeg* aSeg)
{
  nsSVGPathSeg *theSeg = NS_STATIC_CAST(nsSVGPathSeg*, aSeg);
  nsCOMPtr<nsISVGValue> currentList = theSeg->GetCurrentList();
  if (currentList) {
    // aSeg's current list must be cast back to a nsSVGPathSegList*
    nsSVGPathSegList* otherSegList = NS_STATIC_CAST(nsSVGPathSegList*, currentList.get());
    PRInt32 ix = otherSegList->mSegments.IndexOfObject(theSeg);
    if (ix != -1) { 
      otherSegList->RemoveElementAt(ix); 
    }
    theSeg->SetCurrentList(nsnull);
  }
}

////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewSVGPathSegList(nsIDOMSVGPathSegList** result)
{
  *result = nsnull;
  
  nsSVGPathSegList* pathSegList = new nsSVGPathSegList();
  if (!pathSegList) return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(pathSegList);

  // pathSegList->Init();

  *result = (nsIDOMSVGPathSegList*) pathSegList;

  return NS_OK;
}

