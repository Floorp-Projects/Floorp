/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author:
 *   Seth Spitzer <sspitzer@netscape.com>
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
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAbView.h"
#include "nsISupports.h"
#include "nsIRDFService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIAbCard.h"

NS_IMPL_ADDREF(nsAbView)
NS_IMPL_RELEASE(nsAbView)

NS_INTERFACE_MAP_BEGIN(nsAbView)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAbView)
   NS_INTERFACE_MAP_ENTRY(nsIAbView)
   NS_INTERFACE_MAP_ENTRY(nsIOutlinerView)
NS_INTERFACE_MAP_END

nsAbView::nsAbView()
{
  NS_INIT_ISUPPORTS();

  mMailListAtom = getter_AddRefs(NS_NewAtom("MailList"));
}


nsAbView::~nsAbView()
{
  nsIAbCard *card;

  PRInt32 i = mCards.Count();
  while(i-- > 0)
  {
      card = (nsIAbCard*) mCards.ElementAt(i);
      NS_IF_RELEASE(card);
      mCards.RemoveElementAt(i);
  }
}

NS_IMETHODIMP nsAbView::Init(const char *aURI)
{
  nsresult rv;

  mURI = aURI;

  nsCOMPtr <nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(aURI, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAbDirectory> directory = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
    
  rv = EnumerateCards(directory);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult nsAbView::EnumerateCards(nsIAbDirectory* directory)
{
  nsresult rv;    
  nsCOMPtr<nsIEnumerator> cardsEnumerator;
  nsCOMPtr<nsIAbCard> card;

  rv = directory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator)
  {
		nsCOMPtr<nsISupports> item;
	  for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next())
	  {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv))
      {
        nsCOMPtr <nsIAbCard> card = do_QueryInterface(item);
        nsIAbCard *c = card;
        NS_IF_ADDREF(c);
        rv = mCards.AppendElement((void *)c);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to append card");
      }
    }
  }

  // XXX hard code, default sortby name
  rv = SortBy(NS_LITERAL_STRING("DisplayName").get());
  return NS_OK;
}

NS_IMETHODIMP nsAbView::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mCards.Count();
  return NS_OK;
}

NS_IMETHODIMP nsAbView::GetSelection(nsIOutlinerSelection * *aSelection)
{
  *aSelection = mOutlinerSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP nsAbView::SetSelection(nsIOutlinerSelection * aSelection)
{
  mOutlinerSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP nsAbView::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
    return NS_OK;
}

NS_IMETHODIMP nsAbView::GetCellProperties(PRInt32 row, const PRUnichar *colID, nsISupportsArray *properties)
{
  // this is only for the DisplayName column (ignore Department)
  if (colID[0] != 'D' || colID[1] != 'i')
    return NS_OK;

  nsIAbCard *card = (nsIAbCard *)(mCards.ElementAt(row));

  PRBool isMailList = PR_FALSE;
  nsresult rv = card->GetIsMailList(&isMailList);
  NS_ENSURE_SUCCESS(rv,rv);

  if (isMailList)
    properties->AppendElement(mMailListAtom);  

  return NS_OK;
}

NS_IMETHODIMP nsAbView::GetColumnProperties(const PRUnichar *colID, nsIDOMElement *colElt, nsISupportsArray *properties)
{
    return NS_OK;
}

NS_IMETHODIMP nsAbView::IsContainer(PRInt32 index, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP nsAbView::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsAbView::IsSorted(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::CanDropOn(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::Drop(PRInt32 row, PRInt32 orientation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP nsAbView::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP nsAbView::GetCellText(PRInt32 row, const PRUnichar *colID, PRUnichar **_retval)
{
  nsIAbCard *card = (nsIAbCard *)(mCards.ElementAt(row));

  nsresult rv = card->GetCardUnicharValue(colID, _retval);
  NS_ENSURE_SUCCESS(rv,rv);

  return rv;
}

NS_IMETHODIMP nsAbView::SetOutliner(nsIOutlinerBoxObject *outliner)
{
  mOutliner = outliner;
  return NS_OK;
}

NS_IMETHODIMP nsAbView::ToggleOpenState(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::CycleHeader(const PRUnichar *colID, nsIDOMElement *elt)
{
  nsresult rv = SortBy(colID);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = NS_ERROR_UNEXPECTED;
  if (mOutliner)
    rv = mOutliner->Invalidate();
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsAbView::SelectionChanged()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::CycleCell(PRInt32 row, const PRUnichar *colID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::IsEditable(PRInt32 row, const PRUnichar *colID, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::SetCellText(PRInt32 row, const PRUnichar *colID, const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::PerformAction(const PRUnichar *action)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::PerformActionOnCell(const PRUnichar *action, PRInt32 row, const PRUnichar *colID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbView::GetCardFromRow(PRInt32 row, nsIAbCard **aCard)
{
  *aCard = (nsIAbCard *)(mCards.ElementAt(row));
  NS_IF_ADDREF(*aCard);
  return NS_OK;
}

static int PR_CALLBACK
inplaceSortCallback(const void *data1, const void *data2, void *privateData)
{
  nsIAbCard *card1 = (nsIAbCard *)data1;
  nsIAbCard *card2 = (nsIAbCard *)data2;

  nsXPIDLString val1;
  nsXPIDLString val2;

  const PRUnichar *colID = (const PRUnichar *)privateData;
  // XXX fix me, do this with const to avoid the strcpy
  nsresult rv = card1->GetCardUnicharValue(colID, getter_Copies(val1));
  NS_ASSERTION(NS_SUCCEEDED(rv), "get value 1 failed");

  rv = card2->GetCardUnicharValue(colID, getter_Copies(val2));
  NS_ASSERTION(NS_SUCCEEDED(rv), "get value 2 failed");

  PRInt32 sortValue = nsCRT::strcmp(val1.get(), val2.get());

  // if we've got a value, or this is the "PrimaryEmail" column (ignore "PagerNumber")
  // return now.  we ignore "PrimaryEmail", as we use that for our secondary sort
  if (sortValue || (colID[0] == 'P' && colID[1] == 'r'))
    return sortValue;
  
  // secondary sort is always email address.
  // XXX fix me, do this with const to avoid the strcpy
  rv = card1->GetCardUnicharValue(NS_LITERAL_STRING("PrimaryEmail").get(), getter_Copies(val1));
  NS_ASSERTION(NS_SUCCEEDED(rv), "get value 1 failed");

  rv = card2->GetCardUnicharValue(NS_LITERAL_STRING("PrimaryEmail").get(), getter_Copies(val2));
  NS_ASSERTION(NS_SUCCEEDED(rv), "get value 2 failed");
  
  return nsCRT::strcmp(val1.get(), val2.get());
}


nsresult nsAbView::SortBy(const PRUnichar *colID)
{
  // if we are sorting by how we are already sorted, just reverse
  if (!nsCRT::strcmp(mSortedColumn.get(),colID)) {
    PRInt32 count = mCards.Count();
    PRInt32 halfPoint = count / 2;
    for (PRInt32 i=0; i < halfPoint; i++) {
      mCards.MoveElement(i, count - i - 1);
    }
  }
  else {
    mCards.Sort(inplaceSortCallback, (void *)colID);
    mSortedColumn = colID;
  }

  return NS_OK;
}
