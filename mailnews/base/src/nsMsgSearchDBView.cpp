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
#include "nsQuickSort.h"
#include "nsIDBFolderInfo.h"
#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"

static NS_DEFINE_CID(kMsgCopyServiceCID,    NS_MSGCOPYSERVICE_CID);
static NS_DEFINE_CID(kCopyMessageStreamListenerCID, NS_COPYMESSAGESTREAMLISTENER_CID);

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
	NS_IMPL_QUERY_BODY(nsIMsgCopyServiceListener)
NS_IMPL_QUERY_TAIL_INHERITING(nsMsgDBView)


NS_IMETHODIMP nsMsgSearchDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
	nsresult rv;
    m_folders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    m_dbToUseList = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

	rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
    NS_ENSURE_SUCCESS(rv, rv);

	if (pCount)
		*pCount = 0;
	return rv;
}

NS_IMETHODIMP nsMsgSearchDBView::Close()
{
	nsresult rv;
	PRUint32 count;

    NS_ASSERTION(m_dbToUseList, "no db used");
    //if (!m_dbToUseList) return NS_ERROR_FAILURE;

	rv = m_dbToUseList->Count(&count);
	if (NS_FAILED(rv)) return rv;
	
	for(PRUint32 i = 0; i < count; i++)
	{
		((nsIMsgDatabase*)m_dbToUseList->ElementAt(i))->RemoveListener(this);
	}

	return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::GetCellText(PRInt32 aRow, const PRUnichar * aColID, PRUnichar ** aValue)
{
  if (aColID[0] == 'l') // location
  {
    return FetchLocation(aRow, aValue);
  }
  else
    return nsMsgDBView::GetCellText(aRow, aColID, aValue);
}

nsresult nsMsgSearchDBView::FetchLocation(PRInt32 aRow, PRUnichar ** aLocationString)
{
  nsCOMPtr <nsIMsgFolder> folder;
  nsresult rv = GetFolderForViewIndex(aRow, getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = folder->GetPrettiestName(aLocationString);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
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
    return aFolder->GetMsgDatabase(nsnull, db);
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

nsresult nsMsgSearchDBView::GetFolders(nsISupportsArray **aFolders)
{
	NS_ENSURE_ARG_POINTER(aFolders);
	*aFolders = m_folders;
	NS_IF_ADDREF(*aFolders);
	
	return NS_OK;
}

NS_IMETHODIMP 
nsMsgSearchDBView::DoCommandWithFolder(nsMsgViewCommandTypeValue command, nsIMsgFolder *destFolder)
{
    mCommand = command;
    mDestFolder = getter_AddRefs(destFolder);

    return nsMsgDBView::DoCommandWithFolder(command, destFolder);
}

NS_IMETHODIMP nsMsgSearchDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
    mCommand = command;
    return nsMsgDBView::DoCommand(command);
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
nsresult nsMsgSearchDBView::RemoveByIndex(nsMsgViewIndex index)
{
    if (!IsValidIndex(index))
        return NS_MSG_INVALID_DBVIEW_INDEX;

    m_folders->RemoveElementAt(index);

    for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) mTotalIndices; i++)
    {
        mIndicesForChainedDeleteAndFile[i]--;
    }

    return nsMsgDBView::RemoveByIndex(index);
}

nsresult nsMsgSearchDBView::DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage)
{
    nsresult rv;
    InitializeGlobalsForDeleteAndFile(indices, numIndices);

    if (!deleteStorage)
        rv = ProcessChainedRequest(window, indices, numIndices);
    else
        rv = ProcessContinuousRequest(window, indices, numIndices);

    return rv;
}

nsresult 
nsMsgSearchDBView::CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder)
{
    nsresult rv;
    InitializeGlobalsForDeleteAndFile(indices, numIndices);

    rv = ProcessChainedRequest(window, indices, numIndices);

    return rv;
}

nsresult
nsMsgSearchDBView::InitializeGlobalsForDeleteAndFile(nsMsgViewIndex *indices, PRInt32 numIndices)
{
    nsMsgViewIndex *outIndices, *nextIndices;
    nextIndices = outIndices = (nsMsgViewIndex *)nsMemory::Alloc(numIndices * sizeof(nsMsgViewIndex));
    if (!outIndices) return NS_ERROR_OUT_OF_MEMORY;

    for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++) 
    {
        nextIndices[i] = indices[i];
    }

    mIndicesForChainedDeleteAndFile = outIndices;
    mTotalIndices = numIndices;
    mCurIndex = 0;

    return NS_OK;
}

// nsIMsgCopyServiceListener methods
nsresult
nsMsgSearchDBView::OnStartCopy()
{
#ifdef NS_DEBUG
    printf("nsIMsgCopyServiceListener::OnStartCopy()\n");
#endif

    return NS_OK;
}

nsresult
nsMsgSearchDBView::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
    printf("nsIMsgCopyServiceListener::OnProgress() - COPY\n");
#endif

    return NS_OK;
}
  
nsresult
nsMsgSearchDBView::OnStopCopy(nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_SUCCEEDED(aStatus))
    {
        if (mCurIndex != 0) 
        {
            int tmpSize = mTotalIndices - mCurIndex;
            nsMsgViewIndex* tmpIndices = (PRUint32*)PR_Malloc(tmpSize*sizeof(PRUint32)); 
            nsCRT::memcpy(tmpIndices, &mIndicesForChainedDeleteAndFile[mCurIndex], tmpSize*sizeof(PRUint32)); 

            PR_Free(mIndicesForChainedDeleteAndFile);

            mIndicesForChainedDeleteAndFile = tmpIndices;
            mTotalIndices -= mCurIndex;
            mCurIndex = 0;

            ProcessChainedRequest(mMsgWindow, mIndicesForChainedDeleteAndFile, mTotalIndices);
        }
    }

    return rv;
}

nsresult
nsMsgSearchDBView::SetMessageKey(PRUint32 aMessageKey)
{
	return NS_OK;
}

nsresult
nsMsgSearchDBView::GetMessageId(nsCString* aMessageId)
{
	return NS_OK;
}
// end nsIMsgCopyServiceListener methods

nsresult nsMsgSearchDBView::ProcessChainedRequest(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsISupportsArray> messageArray;
    NS_NewISupportsArray(getter_AddRefs(messageArray));

    nsCOMPtr<nsIMsgCopyServiceListener> copyServListener;//= do_QueryInterface(this);
    rv = this->QueryInterface(NS_GET_IID(nsIMsgCopyServiceListener), getter_AddRefs(copyServListener));

    // add error checking.
    nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(indices[0]));
    nsCOMPtr<nsIMsgFolder> curFolder;
    if(curSupports)
    {
        curFolder = do_QueryInterface(curSupports);
    }

    nsCOMPtr <nsIMsgDatabase> dbToUse;
    if (curFolder)
    {
        nsCOMPtr <nsIDBFolderInfo> folderInfo;
        rv = curFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
        NS_ENSURE_SUCCESS(rv,rv);
        dbToUse->AddListener(this);

        nsCOMPtr <nsISupports> tmpSupports = do_QueryInterface(dbToUse);
        m_dbToUseList->AppendElement(tmpSupports);
    }

    // error checking again.
    PRBool myFlag = PR_FALSE;

    for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++)
    {
        nsCOMPtr <nsISupports> supports = getter_AddRefs(m_folders->ElementAt(indices[index]));
        if(supports)
        {
            nsCOMPtr<nsIMsgFolder> currentFolder = do_QueryInterface(supports);
            if (currentFolder)
            {
                nsXPIDLCString curUri;
                nsXPIDLCString currentFolderUri;

                curFolder->GetURI(getter_Copies(curUri));
                currentFolder->GetURI(getter_Copies(currentFolderUri));

                if (nsCRT::strcmp(curUri, currentFolderUri) == 0)
                {
                    nsMsgKey key = m_keys.GetAt(indices[index]);
                    nsCOMPtr <nsIMsgDBHdr> msgHdr;
                    rv = dbToUse->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
                    NS_ENSURE_SUCCESS(rv,rv);

                    if (msgHdr)
                        messageArray->AppendElement(msgHdr);

                    if ((nsMsgViewIndex)index < (nsMsgViewIndex) (numIndices-1))
                        continue;
                }
                else
                {
                    mCurIndex = index;
                    break;
                }
            }
        }
    }

    // called for delete with trash, copy and move
    if (mCommand == nsMsgViewCommandType::deleteMsg)
        curFolder->DeleteMessages(messageArray, window, PR_FALSE /* delete storage */, PR_FALSE /* is move*/, copyServListener);
    else if (mCommand == nsMsgViewCommandType::moveMessages)
        mDestFolder->CopyMessages(curFolder, messageArray, PR_TRUE /* isMove */, window, copyServListener, PR_FALSE /* is folder */);
    else if (mCommand == nsMsgViewCommandType::copyMessages)
        mDestFolder->CopyMessages(curFolder, messageArray, PR_FALSE /* isMove */, window, copyServListener, PR_FALSE /* is folder */);
    return rv;
}

nsresult nsMsgSearchDBView::ProcessContinuousRequest(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsISupportsArray> messageArray;
    NS_NewISupportsArray(getter_AddRefs(messageArray));

    nsCOMPtr<nsIMsgCopyServiceListener> copyServListener;//= do_QueryInterface(this);
    rv = this->QueryInterface(NS_GET_IID(nsIMsgCopyServiceListener), getter_AddRefs(copyServListener));

    // add error checking.
    nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(indices[0]));
    nsCOMPtr<nsIMsgFolder> curFolder;
    if(curSupports)
    {
        curFolder = do_QueryInterface(curSupports);
    }

    nsCOMPtr <nsIMsgDatabase> dbToUse;
    if (curFolder)
    {
        nsCOMPtr <nsIDBFolderInfo> folderInfo;
        rv = curFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
        NS_ENSURE_SUCCESS(rv,rv);
        dbToUse->AddListener(this);

        nsCOMPtr <nsISupports> tmpSupports = do_QueryInterface(dbToUse);
        m_dbToUseList->AppendElement(tmpSupports);
    }

    // error checking again.
    PRBool myFlag = PR_FALSE;

    for (nsMsgViewIndex index = 0; index < (nsMsgViewIndex) numIndices; index++)
    {
        nsCOMPtr <nsISupports> supports = getter_AddRefs(m_folders->ElementAt(indices[index]));
        if(supports)
        {
            nsCOMPtr<nsIMsgFolder> currentFolder = do_QueryInterface(supports);
            if (currentFolder)
            {
                nsXPIDLCString curUri;
                nsXPIDLCString currentFolderUri;

                curFolder->GetURI(getter_Copies(curUri));
                currentFolder->GetURI(getter_Copies(currentFolderUri));

                if (nsCRT::strcmp(curUri, currentFolderUri) == 0)
                {
                    nsMsgKey key = m_keys.GetAt(indices[index]);
                    nsCOMPtr <nsIMsgDBHdr> msgHdr;
                    rv = dbToUse->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
                    NS_ENSURE_SUCCESS(rv,rv);

                    if (msgHdr)
                        messageArray->AppendElement(msgHdr);
    
                    if ((nsMsgViewIndex)index < (nsMsgViewIndex) (numIndices-1))
                        continue;
                }
                else
                    myFlag = PR_TRUE;


                if (mCommand == nsMsgViewCommandType::deleteMsg)
                    curFolder->DeleteMessages(messageArray, window, PR_TRUE /* delete storage */, PR_FALSE /* is move*/, copyServListener);
                else if (mCommand == nsMsgViewCommandType::moveMessages)
                    mDestFolder->CopyMessages(curFolder, messageArray, PR_FALSE /* isMove */, window, copyServListener, PR_FALSE /* is folder */);

                // clean messages array
                // add element
                // define new curfolder
                if (myFlag) 
                {
                    curFolder = currentFolder;

                    messageArray->Clear();

                    nsCOMPtr <nsIDBFolderInfo> folderInfo;
                    rv = curFolder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
                    NS_ENSURE_SUCCESS(rv,rv);
                    dbToUse->AddListener(this);

                    nsCOMPtr <nsISupports> tmpSupports = do_QueryInterface(dbToUse);
                    m_dbToUseList->AppendElement(tmpSupports);					

                    nsMsgKey key = m_keys.GetAt(indices[index]);
                    nsCOMPtr <nsIMsgDBHdr> msgHdr;
                    rv = dbToUse->GetMsgHdrForKey(key, getter_AddRefs(msgHdr));
                    NS_ENSURE_SUCCESS(rv,rv);

                    if (msgHdr)
                        messageArray->AppendElement(msgHdr);

                    if ((nsMsgViewIndex)index == (nsMsgViewIndex) (numIndices-1)) 
                    {
                        // called only for shift delete
                        curFolder->DeleteMessages(messageArray, window, PR_TRUE /* delete storage */, PR_FALSE /* is move*/, copyServListener);
                    }
                }
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
    nsresult rv;

    nsMsgKeyArray preservedSelection;
    SaveSelection(&preservedSelection);
    PRInt32 rowCountBeforeSort = GetSize();

    rv = nsMsgDBView::Sort(sortType,sortOrder);

    // the sort may have changed the number of rows
    // before we restore the selection, tell the outliner
    // do this before we call restore selection
    // this is safe when there is no selection. 
    rv = AdjustRowCount(rowCountBeforeSort, GetSize());

    RestoreSelection(&preservedSelection);
    if (mOutliner) mOutliner->Invalidate();

    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}
