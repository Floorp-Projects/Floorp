/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
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
#include "nsMsgUtils.h"
#include "nsITreeColumns.h"

nsMsgSearchDBView::nsMsgSearchDBView()
{
  // don't try to display messages for the search pane.
  mSuppressMsgDisplay = PR_TRUE;
}

nsMsgSearchDBView::~nsMsgSearchDBView()
{	
}

NS_IMPL_ISUPPORTS_INHERITED3(nsMsgSearchDBView, nsMsgDBView, nsIMsgDBView, nsIMsgCopyServiceListener, nsIMsgSearchNotify)

NS_IMETHODIMP nsMsgSearchDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
    nsresult rv;
    m_folders = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
    NS_ENSURE_SUCCESS(rv, rv);

    if (pCount)
      *pCount = 0;
    m_folder = nsnull;
    return rv;
}

NS_IMETHODIMP
nsMsgSearchDBView::CopyDBView(nsMsgDBView *aNewMsgDBView, nsIMessenger *aMessengerInstance, 
                                       nsIMsgWindow *aMsgWindow, nsIMsgDBViewCommandUpdater *aCmdUpdater)
{
  nsMsgDBView::CopyDBView(aNewMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);
  nsMsgSearchDBView* newMsgDBView = (nsMsgSearchDBView *) aNewMsgDBView;

  // now copy all of our private member data
  newMsgDBView->mDestFolder = mDestFolder;
  newMsgDBView->mCommand = mCommand;
  newMsgDBView->mTotalIndices = mTotalIndices;
  newMsgDBView->mCurIndex = mCurIndex; 

  if (m_folders)
    m_folders->Clone(getter_AddRefs(newMsgDBView->m_folders));

  if (m_hdrsForEachFolder)
    m_hdrsForEachFolder->Clone(getter_AddRefs(newMsgDBView->m_hdrsForEachFolder));

  if (m_copyListenerList)
    m_copyListenerList->Clone(getter_AddRefs(newMsgDBView->m_copyListenerList));

  if (m_uniqueFoldersSelected)
    m_uniqueFoldersSelected->Clone(getter_AddRefs(newMsgDBView->m_uniqueFoldersSelected));


  PRInt32 count = m_dbToUseList.Count(); 
  for(PRInt32 i = 0; i < count; i++)
  {
    newMsgDBView->m_dbToUseList.AppendObject(m_dbToUseList[i]);
    // register the new view with the database so it gets notifications
    m_dbToUseList[i]->AddListener(newMsgDBView);
  }

  // nsUInt32Array* mTestIndices;

  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::Close()
{
  PRInt32 count = m_dbToUseList.Count();
  
  for(PRInt32 i = 0; i < count; i++)
    m_dbToUseList[i]->RemoveListener(this);

  m_dbToUseList.Clear();

  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aValue)
{
  const PRUnichar* colID;
  aCol->GetIdConst(&colID);
  if (colID[0] == 'l' && colID[1] == 'o') // location, need to check for "lo" not just "l" to avoid "label" column
  {
    // XXX fix me by converting Fetch* to take an nsAString& parameter
    nsXPIDLString valueText;
    nsresult rv = FetchLocation(aRow, getter_Copies(valueText));
    aValue.Assign(valueText);
    return rv;
  }
  else
    return nsMsgDBView::GetCellText(aRow, aCol, aValue);
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

nsresult nsMsgSearchDBView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool /*ensureListed*/)
{
   return NS_OK;
}

nsresult nsMsgSearchDBView::GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr)
{
  nsresult rv = NS_MSG_INVALID_DBVIEW_INDEX;
  nsCOMPtr<nsIMsgFolder> folder = do_QueryElementAt(m_folders, index);
  if (folder)
    {
      nsCOMPtr <nsIMsgDatabase> db;
      rv = folder->GetMsgDatabase(mMsgWindow, getter_AddRefs(db));
      NS_ENSURE_SUCCESS(rv, rv);
      if (db)
        rv = db->GetMsgHdrForKey(m_keys.GetAt(index), msgHdr);
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
  if (m_folders->IndexOf(supports) < 0 ) //do this just for new folder
  {
    nsCOMPtr<nsIMsgDatabase> dbToUse;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    folder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
    if (dbToUse)
    {
      dbToUse->AddListener(this);
      m_dbToUseList.AppendObject(dbToUse);
    }
  }

  m_folders->AppendElement(supports);
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  aMsgHdr->GetMessageKey(&msgKey);
  aMsgHdr->GetFlags(&msgFlags);
  m_keys.Add(msgKey);
  m_levels.Add(0);
  m_flags.Add(msgFlags);

  // this needs to be called after we add the key, since RowCountChanged() will call our GetRowCount()
  if (mTree)
    mTree->RowCountChanged(GetSize() - 1, 1);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnSearchDone(nsresult status)
{
  //we want to set imap delete model once the search is over because setting next
  //message after deletion will happen before deleting the message and search scope
  //can change with every search.
  mDeleteModel = nsMsgImapDeleteModels::MoveToTrash;  //set to default in case it is non-imap folder
  nsCOMPtr <nsIMsgFolder> curFolder = do_QueryElementAt(m_folders, 0);
  if (curFolder)   
    GetImapDeleteModel(curFolder);
  return NS_OK;
}


// for now also acts as a way of resetting the search datasource
NS_IMETHODIMP
nsMsgSearchDBView::OnNewSearch()
{
  PRInt32 oldSize = GetSize();

  PRInt32 count = m_dbToUseList.Count();
  for(PRInt32 j = 0; j < count; j++) 
    m_dbToUseList[j]->RemoveListener(this);

  m_dbToUseList.Clear();

  m_folders->Clear();
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();

  // needs to happen after we remove the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree) 
    mTree->RowCountChanged(0, -oldSize);

//    mSearchResults->Clear();
    return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *instigator)
{
  m_dbToUseList.RemoveObject(NS_STATIC_CAST(nsIMsgDatabase *, instigator));
  return nsMsgDBView::OnAnnouncerGoingAway(instigator);
}

nsresult nsMsgSearchDBView::GetFolders(nsISupportsArray **aFolders)
{
  NS_ENSURE_ARG_POINTER(aFolders);
  NS_IF_ADDREF(*aFolders = m_folders);
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgSearchDBView::DoCommandWithFolder(nsMsgViewCommandTypeValue command, nsIMsgFolder *destFolder)
{
    mCommand = command;
    mDestFolder = destFolder;

    return nsMsgDBView::DoCommandWithFolder(command, destFolder);
}

NS_IMETHODIMP nsMsgSearchDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
  mCommand = command;
  if (command == nsMsgViewCommandType::deleteMsg || command == nsMsgViewCommandType::deleteNoTrash
    || command == nsMsgViewCommandType::selectAll)
    return nsMsgDBView::DoCommand(command);
  nsresult rv = NS_OK;
  nsUInt32Array selection;
  GetSelectedIndices(&selection);

  nsMsgViewIndex *indices = selection.GetData();
  PRInt32 numIndices = selection.GetSize();

  // we need to break apart the selection by folders, and then call
  // ApplyCommandToIndices with the command and the indices in the
  // selection that are from that folder.

  nsUInt32Array *indexArrays;
  PRInt32 numArrays;
  rv = PartitionSelectionByFolder(indices, numIndices, &indexArrays, &numArrays);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRInt32 folderIndex = 0; folderIndex < numArrays; folderIndex++)
  {
    rv = ApplyCommandToIndices(command, indexArrays[folderIndex].GetData(), indexArrays[folderIndex].GetSize());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
nsresult nsMsgSearchDBView::RemoveByIndex(nsMsgViewIndex index)
{
    if (!IsValidIndex(index))
        return NS_MSG_INVALID_DBVIEW_INDEX;

    m_folders->RemoveElementAt(index);
    
    return nsMsgDBView::RemoveByIndex(index);
}

nsresult nsMsgSearchDBView::DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage)
{
    nsresult rv;
    GetFoldersAndHdrsForSelection(indices, numIndices);
    if (mDeleteModel != nsMsgImapDeleteModels::MoveToTrash)
      deleteStorage = PR_TRUE;
    if (!deleteStorage)
      rv = ProcessRequestsInOneFolder(window);
    else
      rv = ProcessRequestsInAllFolders(window);
    return rv;
}

nsresult 
nsMsgSearchDBView::CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder)
{
    nsresult rv;
    GetFoldersAndHdrsForSelection(indices, numIndices);

    rv = ProcessRequestsInOneFolder(window);

    return rv;
}

nsresult
nsMsgSearchDBView::PartitionSelectionByFolder(nsMsgViewIndex *indices, PRInt32 numIndices, nsUInt32Array **indexArrays, PRInt32 *numArrays)
{
  nsresult rv = NS_OK; 
  nsCOMPtr <nsISupportsArray> uniqueFoldersSelected = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
  mCurIndex = 0;

  //Build unique folder list based on headers selected by the user
  for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++)
  {
     nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(indices[i]));
     if ( uniqueFoldersSelected->IndexOf(curSupports) < 0)
       uniqueFoldersSelected->AppendElement(curSupports); 
  }

  PRUint32 numFolders =0; 
  rv = uniqueFoldersSelected->Count(&numFolders);   //group the headers selected by each folder 
  *indexArrays = new nsUInt32Array[numFolders];
  *numArrays = numFolders;
  NS_ENSURE_TRUE(*indexArrays, NS_ERROR_OUT_OF_MEMORY);
  for (PRUint32 folderIndex=0; folderIndex < numFolders; folderIndex++)
  {
     nsCOMPtr <nsIMsgFolder> curFolder =
         do_QueryElementAt(uniqueFoldersSelected, folderIndex, &rv);
     for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++) 
     {
       nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryElementAt(m_folders,
                                                             indices[i], &rv);
       if (NS_SUCCEEDED(rv) && msgFolder && msgFolder == curFolder) 
          (*indexArrays)[folderIndex].Add(indices[i]);
     }
  }
  return rv;

}

nsresult
nsMsgSearchDBView::GetFoldersAndHdrsForSelection(nsMsgViewIndex *indices, PRInt32 numIndices)
{
  nsresult rv = NS_OK; 
  mCurIndex = 0;
                    //initialize and clear from the last usage
  if (!m_uniqueFoldersSelected)
  {
    m_uniqueFoldersSelected = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv); 
  }
  else
    m_uniqueFoldersSelected->Clear();
  
  if (!m_hdrsForEachFolder)
  {
    m_hdrsForEachFolder = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else
    m_hdrsForEachFolder->Clear();

  //Build unique folder list based on headers selected by the user
  for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++)
  {
     nsCOMPtr <nsISupports> curSupports = getter_AddRefs(m_folders->ElementAt(indices[i]));
     if ( m_uniqueFoldersSelected->IndexOf(curSupports) < 0)
       m_uniqueFoldersSelected->AppendElement(curSupports); 
  }

  PRUint32 numFolders =0; 
  rv = m_uniqueFoldersSelected->Count(&numFolders);   //group the headers selected by each folder 
  NS_ENSURE_SUCCESS(rv,rv);
  for (PRUint32 folderIndex=0; folderIndex < numFolders; folderIndex++)
  {
     nsCOMPtr <nsIMsgFolder> curFolder =
         do_QueryElementAt(m_uniqueFoldersSelected, folderIndex, &rv);
     nsCOMPtr <nsISupportsArray> msgHdrsForOneFolder;
     NS_NewISupportsArray(getter_AddRefs(msgHdrsForOneFolder));
     for (nsMsgViewIndex i = 0; i < (nsMsgViewIndex) numIndices; i++) 
     {
       nsCOMPtr <nsIMsgFolder> msgFolder = do_QueryElementAt(m_folders,
                                                             indices[i], &rv);
       if (NS_SUCCEEDED(rv) && msgFolder && msgFolder == curFolder) 
       {
          nsCOMPtr<nsIMsgDBHdr> msgHdr; 
          rv = GetMsgHdrForViewIndex(indices[i],getter_AddRefs(msgHdr));
          NS_ENSURE_SUCCESS(rv,rv);
          nsCOMPtr <nsISupports> hdrSupports = do_QueryInterface(msgHdr);
          msgHdrsForOneFolder->AppendElement(hdrSupports);
       }
     }
     nsCOMPtr <nsISupports> supports = do_QueryInterface(msgHdrsForOneFolder, &rv);
     if (NS_SUCCEEDED(rv) && supports)
       m_hdrsForEachFolder->AppendElement(supports);
  }
  return rv;


}

// nsIMsgCopyServiceListener methods

NS_IMETHODIMP
nsMsgSearchDBView::OnStartCopy()
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDBView::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

// believe it or not, these next two are msgcopyservice listener methods!
NS_IMETHODIMP
nsMsgSearchDBView::SetMessageKey(PRUint32 aMessageKey)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchDBView::GetMessageId(nsCString* aMessageId)
{
  return NS_OK;
}
  
NS_IMETHODIMP
nsMsgSearchDBView::OnStopCopy(nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_SUCCEEDED(aStatus))
    {
        mCurIndex++;
        PRUint32 numFolders =0;
        rv = m_uniqueFoldersSelected->Count(&numFolders);
        if ( mCurIndex < numFolders)
          ProcessRequestsInOneFolder(mMsgWindow);
    }

    return rv;
}

// end nsIMsgCopyServiceListener methods

nsresult nsMsgSearchDBView::ProcessRequestsInOneFolder(nsIMsgWindow *window)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIMsgFolder> curFolder =
        do_QueryElementAt(m_uniqueFoldersSelected, mCurIndex);
    NS_ASSERTION(curFolder, "curFolder is null");
    nsCOMPtr <nsISupportsArray> messageArray =
        do_QueryElementAt(m_hdrsForEachFolder, mCurIndex);
    NS_ASSERTION(messageArray, "messageArray is null");

    // called for delete with trash, copy and move
    if (mCommand == nsMsgViewCommandType::deleteMsg)
        curFolder->DeleteMessages(messageArray, window, PR_FALSE /* delete storage */, PR_FALSE /* is move*/, this, PR_FALSE /*allowUndo*/);
    else 
    {
      NS_ASSERTION(!(curFolder == mDestFolder), "The source folder and the destination folder are the same");
      if (NS_SUCCEEDED(rv) && curFolder != mDestFolder)
      {
         nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
         if (NS_SUCCEEDED(rv))
         {
           if (mCommand == nsMsgViewCommandType::moveMessages)
             copyService->CopyMessages(curFolder, messageArray, mDestFolder, PR_TRUE /* isMove */, this, window, PR_FALSE /*allowUndo*/);
           else if (mCommand == nsMsgViewCommandType::copyMessages)
             copyService->CopyMessages(curFolder, messageArray, mDestFolder, PR_FALSE /* isMove */, this, window, PR_FALSE /*allowUndo*/);
         }
      }
    }
    return rv;
}

nsresult nsMsgSearchDBView::ProcessRequestsInAllFolders(nsIMsgWindow *window)
{
    PRUint32 numFolders =0;
    nsresult rv = m_uniqueFoldersSelected->Count(&numFolders);
    NS_ENSURE_SUCCESS(rv,rv);
    for (PRUint32 folderIndex=0; folderIndex < numFolders; folderIndex++)
    {
       nsCOMPtr<nsIMsgFolder> curFolder =
           do_QueryElementAt(m_uniqueFoldersSelected, folderIndex);
       NS_ASSERTION (curFolder, "curFolder is null");

       nsCOMPtr <nsISupportsArray> messageArray =
           do_QueryElementAt(m_hdrsForEachFolder, folderIndex);
       NS_ASSERTION(messageArray, "messageArray is null");

       curFolder->DeleteMessages(messageArray, window, PR_TRUE /* delete storage */, PR_FALSE /* is move*/, nsnull/*copyServListener*/, PR_FALSE /*allowUndo*/ );
    }

    return NS_OK;
}

NS_IMETHODIMP nsMsgSearchDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
    nsresult rv;
    PRInt32 rowCountBeforeSort = GetSize();

    if (!rowCountBeforeSort)
        return NS_OK;

    nsMsgKey preservedKey;
    nsMsgKeyArray preservedSelection;
    SaveAndClearSelection(&preservedKey, &preservedSelection);

    rv = nsMsgDBView::Sort(sortType,sortOrder);

    // the sort may have changed the number of rows
    // before we restore the selection, tell the tree
    // do this before we call restore selection
    // this is safe when there is no selection. 
    rv = AdjustRowCount(rowCountBeforeSort, GetSize());

    RestoreSelection(preservedKey, &preservedSelection);
    if (mTree) mTree->Invalidate();

    NS_ENSURE_SUCCESS(rv,rv);
    return rv;
}


// if nothing selected, return an NS_ERROR
NS_IMETHODIMP
nsMsgSearchDBView::GetHdrForFirstSelectedMessage(nsIMsgDBHdr **hdr)
{
  NS_ENSURE_ARG_POINTER(hdr);
  PRInt32 index;
  if (!mTreeSelection)
    return NS_ERROR_NULL_POINTER;
  nsresult rv = mTreeSelection->GetCurrentIndex(&index);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = GetMsgHdrForViewIndex(index, hdr);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult
nsMsgSearchDBView::GetFolderFromMsgURI(const char *aMsgURI, nsIMsgFolder **aFolder)
{
  nsCOMPtr <nsIMsgMessageService> msgMessageService;
  nsresult rv = GetMessageServiceFromURI(aMsgURI, getter_AddRefs(msgMessageService));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = msgMessageService->MessageURIToMsgHdr(aMsgURI, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv,rv);
  
  return msgHdr->GetFolder(aFolder);
}

