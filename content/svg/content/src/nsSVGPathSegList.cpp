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
#include "nsDOMError.h"
#include "nsSVGPathDataParser.h"
#include "nsReadableUtils.h"

////////////////////////////////////////////////////////////////////////
// nsSVGPathSegList

class nsSVGPathSegList : public nsIDOMSVGPathSegList,
                         public nsSVGValue,
                         public nsISVGValueObserver,
                         public nsSupportsWeakReference
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
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);
  
  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

protected:
  // implementation helpers:
  nsIDOMSVGPathSeg* ElementAt(PRInt32 index);
  void AppendElement(nsIDOMSVGPathSeg* aElement);
  void RemoveElementAt(PRInt32 index);
  void InsertElementAt(nsIDOMSVGPathSeg* aElement, PRInt32 index);
  
  void ReleaseSegments();
  
  nsVoidArray mSegments;
};


//----------------------------------------------------------------------
// Implementation

nsSVGPathSegList::nsSVGPathSegList()
{
}

nsSVGPathSegList::~nsSVGPathSegList()
{
  ReleaseSegments();
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

  char *str = ToNewCString(aValue);

  nsVoidArray data;
  nsSVGPathDataParser parser(&data);
  rv = parser.Parse(str);
  
  if (NS_SUCCEEDED(rv)) {
    WillModify();
    ReleaseSegments();
    mSegments = data;
    PRInt32 count = mSegments.Count();
    for (PRInt32 i=0; i<count; ++i) {
      nsIDOMSVGPathSeg* seg = ElementAt(i);
      nsCOMPtr<nsISVGValue> val = do_QueryInterface(seg);
      if (val)
        val->AddObserver(this);
    }
    DidModify();
  }
  else {
    NS_ERROR("path data parse error!");    
    PRInt32 count = data.Count();
    for (PRInt32 i=0; i<count; ++i) {
      nsIDOMSVGPathSeg* seg = (nsIDOMSVGPathSeg*)data.ElementAt(i);
      NS_RELEASE(seg);
    }
  }
  
  nsMemory::Free(str);
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
    nsIDOMSVGPathSeg* seg = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(seg);
    NS_ASSERTION(val, "path segment doesn't implement required interface");
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
NS_IMETHODIMP nsSVGPathSegList::Initialize(nsIDOMSVGPathSeg *newItem, nsIDOMSVGPathSeg **_retval)
{
  Clear();
  return AppendItem(newItem, _retval);
}

/* nsIDOMSVGPathSeg getItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::GetItem(PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  if ((PRInt32)index >= mSegments.Count()) {
    *_retval = nsnull;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval  = ElementAt(index);
  NS_ADDREF(*_retval);
  return NS_OK;
}

/* nsIDOMSVGPathSeg insertItemBefore (in nsIDOMSVGPathSeg newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::InsertItemBefore(nsIDOMSVGPathSeg *newItem, PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPathSeg replaceItem (in nsIDOMSVGPathSeg newItem, in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::ReplaceItem(nsIDOMSVGPathSeg *newItem, PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIDOMSVGPathSeg removeItem (in unsigned long index); */
NS_IMETHODIMP nsSVGPathSegList::RemoveItem(PRUint32 index, nsIDOMSVGPathSeg **_retval)
{
  if ((PRInt32)index >= mSegments.Count()) {
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

/* nsIDOMSVGPathSeg appendItem (in nsIDOMSVGPathSeg newItem); */
NS_IMETHODIMP nsSVGPathSegList::AppendItem(nsIDOMSVGPathSeg *newItem, nsIDOMSVGPathSeg **_retval)
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
nsSVGPathSegList::WillModifySVGObservable(nsISVGValue* observable)
{
  WillModify();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGPathSegList::DidModifySVGObservable (nsISVGValue* observable)
{
  DidModify();
  return NS_OK;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGPathSegList::ReleaseSegments()
{
  WillModify();
  PRInt32 count = mSegments.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsIDOMSVGPathSeg* seg = ElementAt(i);
    nsCOMPtr<nsISVGValue> val = do_QueryInterface(seg);
    if (val)
      val->RemoveObserver(this);
    NS_RELEASE(seg);
  }
  mSegments.Clear();
  DidModify();
}

nsIDOMSVGPathSeg*
nsSVGPathSegList::ElementAt(PRInt32 index)
{
  return (nsIDOMSVGPathSeg*)mSegments.ElementAt(index);
}

void
nsSVGPathSegList::AppendElement(nsIDOMSVGPathSeg* aElement)
{
  WillModify();
  NS_ADDREF(aElement);
  mSegments.AppendElement((void*)aElement);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
  if (val)
    val->AddObserver(this);
  DidModify();
}

void
nsSVGPathSegList::RemoveElementAt(PRInt32 index)
{
  WillModify();
  nsIDOMSVGPathSeg* seg = ElementAt(index);
  NS_ASSERTION(seg, "null pathsegment");
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(seg);
  if (val)
    val->RemoveObserver(this);
  mSegments.RemoveElementAt(index);
  NS_RELEASE(seg);
  DidModify();
}

void
nsSVGPathSegList::InsertElementAt(nsIDOMSVGPathSeg* aElement, PRInt32 index)
{
  WillModify();
  NS_ADDREF(aElement);
  mSegments.InsertElementAt((void*)aElement, index);
  nsCOMPtr<nsISVGValue> val = do_QueryInterface(aElement);
  if (val)
    val->AddObserver(this);
  DidModify();
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

