/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include "msgCore.h"
#include "nsMsgSearchDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"

nsMsgSearchDBView::nsMsgSearchDBView()
{
  /* member initializers and constructor code */
  // don't try to display messages for the search pane.
  mSupressMsgDisplay = PR_TRUE;
}

nsMsgSearchDBView::~nsMsgSearchDBView()
{
  /* destructor code */
}

NS_IMPL_ADDREF_INHERITED(nsMsgSearchDBView, nsMsgDBView)
NS_IMPL_RELEASE_INHERITED(nsMsgSearchDBView, nsMsgDBView)
NS_IMPL_QUERY_HEAD(nsMsgSearchDBView)
    NS_IMPL_QUERY_BODY(nsIMsgSearchNotify)
NS_IMPL_QUERY_TAIL_INHERITING(nsMsgDBView)


NS_IMETHODIMP nsMsgSearchDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
	nsresult rv;
  m_folders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

	rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
    NS_ENSURE_SUCCESS(rv, rv);

	if (pCount)
		*pCount = 0;
	return rv;
}

NS_IMETHODIMP nsMsgSearchDBView::GetCellText(PRInt32 aRow, const PRUnichar * aColID, PRUnichar ** aValue)
{
  nsresult rv;
  if (aColID[0] == 'l') // location
  {
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = GetMsgHdrForViewIndex(aRow, getter_AddRefs(msgHdr));
    NS_ENSURE_SUCCESS(rv, rv);
    return FetchLocation(msgHdr, aValue);
  }
  else
    return nsMsgDBView::GetCellText(aRow, aColID, aValue);

}

nsresult nsMsgSearchDBView::FetchLocation(nsIMsgDBHdr * aHdr, PRUnichar ** aSizeString)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgSearchDBView::GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr)
{
  nsresult rv;
  nsCOMPtr <nsISupports> supports = getter_AddRefs(m_folders->ElementAt(index));
	if(supports)
	{
		nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(supports);
    if (folder)
    {
      nsCOMPtr <nsIMsgDatabase> db;
      rv = folder->GetMsgDatabase(mMsgWindow, getter_AddRefs(db));
      NS_ENSURE_SUCCESS(rv, rv);
      if (db)
        rv = db->GetMsgHdrForKey(m_keys.GetAt(index), msgHdr);
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgSearchDBView::GetFolderForViewIndex(nsMsgViewIndex index, nsIMsgFolder **aFolder)
{
  return m_folders->QueryElementAt(index, NS_GET_IID(nsIMsgFolder), (void **) aFolder);
}

nsresult nsMsgSearchDBView::GetDBForViewIndex(nsMsgViewIndex index, nsIMsgDatabase **db)
{
  nsCOMPtr <nsIMsgFolder> aFolder;
  GetFolderForViewIndex(index, getter_AddRefs(aFolder));
  if (aFolder)
    return aFolder->GetMsgDatabase(nsnull, getter_AddRefs(db));
  else
    return NS_MSG_INVALID_DBVIEW_INDEX;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(folder);
  nsCOMPtr <nsISupports> supports = do_QueryInterface(folder);

  m_folders->AppendElement(supports);

  nsresult rv = NS_OK;
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  aMsgHdr->GetMessageKey(&msgKey);
  aMsgHdr->GetFlags(&msgFlags);
  m_keys.Add(msgKey);
  m_levels.Add(0);
  m_flags.Add(msgFlags);
  if (mOutliner)
    mOutliner->RowCountChanged(m_keys.GetSize() - 1, 1);

  return rv;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnSearchDone(nsresult status)
{
  return NS_OK;
}


// for now also acts as a way of resetting the search datasource
NS_IMETHODIMP
nsMsgSearchDBView::OnNewSearch()
{
  if (mOutliner)
  {
    PRInt32 viewSize = m_keys.GetSize();
    mOutliner->RowCountChanged(0, -viewSize); // all rows gone.
  }
  m_folders->Clear();
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();
//    mSearchResults->Clear();
    return NS_OK;
}


