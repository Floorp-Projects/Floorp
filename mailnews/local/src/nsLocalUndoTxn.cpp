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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIMsgHdr.h"
#include "nsLocalUndoTxn.h"
#include "nsImapCore.h"
#include "nsMsgImapCID.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIEventQueueService.h"
#include "nsIMsgMailSession.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

nsLocalMoveCopyMsgTxn::nsLocalMoveCopyMsgTxn() :
    m_isMove(PR_FALSE), m_srcIsImap4(PR_FALSE)
{
	Init(nsnull, nsnull, PR_FALSE);
}

nsLocalMoveCopyMsgTxn::nsLocalMoveCopyMsgTxn(nsIMsgFolder* srcFolder,
                                             nsIMsgFolder* dstFolder,
                                             PRBool isMove) :
    m_isMove(PR_FALSE), m_srcIsImap4(PR_FALSE)
{
	Init(srcFolder, dstFolder, isMove);
}

nsLocalMoveCopyMsgTxn::~nsLocalMoveCopyMsgTxn()
{
}

NS_IMPL_ADDREF_INHERITED(nsLocalMoveCopyMsgTxn, nsMsgTxn)
NS_IMPL_RELEASE_INHERITED(nsLocalMoveCopyMsgTxn, nsMsgTxn)

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (!aInstancePtr) return NS_ERROR_NULL_POINTER;

    *aInstancePtr = nsnull;

    if (aIID.Equals(NS_GET_IID(nsLocalMoveCopyMsgTxn))) 
    {
        *aInstancePtr = NS_STATIC_CAST(nsLocalMoveCopyMsgTxn*, this);
    }

    if (*aInstancePtr)
    {
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsMsgTxn::QueryInterface(aIID, aInstancePtr);
}

nsresult
nsLocalMoveCopyMsgTxn::Init(nsIMsgFolder* srcFolder, nsIMsgFolder* dstFolder,
							PRBool isMove)
{
    nsresult rv;
    rv = SetSrcFolder(srcFolder);
    rv = SetDstFolder(dstFolder);
    m_isMove = isMove;

    mUndoFolderListener = nsnull;

    nsXPIDLCString uri;
    if (!srcFolder) return rv;
    rv = srcFolder->GetURI(getter_Copies(uri));
    nsCString protocolType(uri);
    protocolType.SetLength(protocolType.FindChar(':'));
    if (protocolType.LowerCaseEqualsLiteral("imap"))
    {
        m_srcIsImap4 = PR_TRUE;
    }
    return NS_OK;
}
nsresult 
nsLocalMoveCopyMsgTxn::GetSrcIsImap(PRBool *isImap)
{
  *isImap = m_srcIsImap4;
  return NS_OK;
}
nsresult
nsLocalMoveCopyMsgTxn::SetSrcFolder(nsIMsgFolder* srcFolder)
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (srcFolder)
          m_srcFolder = do_GetWeakReference(srcFolder, &rv);
	return rv;
}

nsresult
nsLocalMoveCopyMsgTxn::SetDstFolder(nsIMsgFolder* dstFolder)
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (dstFolder)
          m_dstFolder = do_GetWeakReference(dstFolder, &rv);
	return rv;
}

nsresult
nsLocalMoveCopyMsgTxn::AddSrcKey(nsMsgKey aKey)
{
	m_srcKeyArray.Add(aKey);
	return NS_OK;
}

nsresult
nsLocalMoveCopyMsgTxn::AddSrcStatusOffset(PRUint32 aStatusOffset)
{
	m_srcStatusOffsetArray.Add(aStatusOffset);
	return NS_OK;
}


nsresult
nsLocalMoveCopyMsgTxn::AddDstKey(nsMsgKey aKey)
{
	m_dstKeyArray.Add(aKey);
	return NS_OK;
}

nsresult
nsLocalMoveCopyMsgTxn::AddDstMsgSize(PRUint32 msgSize)
{
    m_dstSizeArray.Add(msgSize);
    return NS_OK;
}

nsresult
nsLocalMoveCopyMsgTxn::UndoImapDeleteFlag(nsIMsgFolder* folder, 
                                          nsMsgKeyArray& keyArray,
                                          PRBool deleteFlag)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (m_srcIsImap4)
    {
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(NS_IMAPSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIUrlListener> urlListener;
            nsCString msgIds;
            PRUint32 i, count = keyArray.GetSize();

            urlListener = do_QueryInterface(folder, &rv);

            for (i=0; i < count; i++)
            {
                if (!msgIds.IsEmpty())
                    msgIds.Append(',');
                msgIds.AppendInt((PRInt32) keyArray.GetAt(i));
            }
            nsCOMPtr<nsIEventQueue> eventQueue;
            nsCOMPtr<nsIEventQueueService> pEventQService = 
                     do_GetService(kEventQueueServiceCID, &rv);
            if (NS_SUCCEEDED(rv) && pEventQService)
            {
                pEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                              getter_AddRefs(eventQueue));
                if (eventQueue)
                {
                    // This is to make sure that we are in the selected state
                    // when executing the imap url; we don't want to load the
                    // folder so use lite select to do the trick
                    rv = imapService->LiteSelectFolder(eventQueue, folder,
                                                       urlListener, nsnull);
                    if (!deleteFlag)
                        rv =imapService->AddMessageFlags(eventQueue, folder,
                                                        urlListener, nsnull,
                                                        msgIds.get(),
                                                        kImapMsgDeletedFlag,
                                                        PR_TRUE);
                    else
                        rv = imapService->SubtractMessageFlags(eventQueue,
                                                              folder,
                                                         urlListener, nsnull,
                                                         msgIds.get(),
                                                         kImapMsgDeletedFlag,
                                                         PR_TRUE);
                    if (NS_SUCCEEDED(rv) && m_msgWindow)
                        folder->UpdateFolder(m_msgWindow);
                }
            }
        }
        rv = NS_OK; // always return NS_OK to indicate that the src is imap
    }
    else
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::UndoTransaction()
{
    nsresult rv;
    nsCOMPtr<nsIMsgDatabase> dstDB;
    
    nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
    if (NS_FAILED(rv) || !dstFolder) return rv;
    nsCOMPtr<nsIMsgLocalMailFolder> dstlocalMailFolder = do_QueryReferent(m_dstFolder, &rv);
    if (NS_FAILED(rv) || !dstlocalMailFolder) return rv;
		dstlocalMailFolder->GetDatabaseWOReparse(getter_AddRefs(dstDB));

    if (!dstDB)
    {
      mUndoFolderListener = new nsLocalUndoFolderListener(this, dstFolder);
      if (!mUndoFolderListener)
        return NS_ERROR_OUT_OF_MEMORY; 
      NS_ADDREF(mUndoFolderListener);
      
      nsCOMPtr<nsIMsgMailSession> mailSession = 
        do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = mailSession->AddFolderListener(mUndoFolderListener, nsIFolderListener::event);
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = dstFolder->GetMsgDatabase(nsnull, getter_AddRefs(dstDB));
      NS_ENSURE_SUCCESS(rv,rv);
    }
    else
    {
      rv = UndoTransactionInternal();
    }
    return rv;
}

nsresult 
nsLocalMoveCopyMsgTxn::UndoTransactionInternal()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mUndoFolderListener)
    {
      nsCOMPtr<nsIMsgMailSession> mailSession = 
        do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv); 
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = mailSession->RemoveFolderListener(mUndoFolderListener);
      NS_ENSURE_SUCCESS(rv,rv);
      
      NS_RELEASE(mUndoFolderListener);
      mUndoFolderListener = nsnull;
    }

    nsCOMPtr<nsIMsgDatabase> srcDB;
    nsCOMPtr<nsIMsgDatabase> dstDB;
    nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
    if (NS_FAILED(rv) || !srcFolder) return rv;
    
    nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
    if (NS_FAILED(rv) || !dstFolder) return rv;
    
    rv = srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
    if(NS_FAILED(rv)) return rv;

    rv = dstFolder->GetMsgDatabase(nsnull, getter_AddRefs(dstDB));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count = m_srcKeyArray.GetSize();
    PRUint32 i;
    nsCOMPtr<nsIMsgDBHdr> oldHdr;
    nsCOMPtr<nsIMsgDBHdr> newHdr;

    // protect against a bogus undo txn without any source keys
    // see bug #179856 for details
    NS_ASSERTION(count, "no source keys");
    if (!count)
      return NS_ERROR_UNEXPECTED;

    if (m_isMove)
    {
        if (m_srcIsImap4)
        {
            PRBool deleteFlag = PR_TRUE;  //message has been deleted -we are trying to undo it
            CheckForToggleDelete(srcFolder, m_srcKeyArray.GetAt(0), &deleteFlag); //there could have been a toggle.
            rv = UndoImapDeleteFlag(srcFolder, m_srcKeyArray, deleteFlag);
        }
        else
        {
            nsCOMPtr<nsISupportsArray> srcMessages;
            NS_NewISupportsArray(getter_AddRefs(srcMessages));
            nsCOMPtr <nsISupports> msgSupports;
            for (i=0; i<count; i++)
            {
                rv = dstDB->GetMsgHdrForKey(m_dstKeyArray.GetAt(i), 
                                            getter_AddRefs(oldHdr));
                NS_ASSERTION(oldHdr, "fatal ... cannot get old msg header\n");
                if (NS_SUCCEEDED(rv) && oldHdr)
                {
                    rv = srcDB->CopyHdrFromExistingHdr(m_srcKeyArray.GetAt(i),
                                                       oldHdr, PR_TRUE,
                                                       getter_AddRefs(newHdr));
                    NS_ASSERTION(newHdr, 
                                 "fatal ... cannot create new msg header\n");
                    if (NS_SUCCEEDED(rv) && newHdr)
                    {
											 newHdr->SetStatusOffset(m_srcStatusOffsetArray.GetAt(i));
                       srcDB->UndoDelete(newHdr);
                       msgSupports = do_QueryInterface(newHdr);
                       srcMessages->AppendElement(msgSupports);
                    }
                }
            }
            nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(srcFolder);
            if (localFolder)
              localFolder->MarkMsgsOnPop3Server(srcMessages, POP3_NONE /*deleteMsgs*/);
        }
        srcDB->SetSummaryValid(PR_TRUE);
        srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
    }

    dstDB->DeleteMessages(&m_dstKeyArray, nsnull);
    dstDB->SetSummaryValid(PR_TRUE);
    dstDB->Commit(nsMsgDBCommitType::kLargeCommit);

    return rv;
}

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::RedoTransaction()
{
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIMsgDatabase> srcDB;
    nsCOMPtr<nsIMsgDatabase> dstDB;

    nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
    if (NS_FAILED(rv) || !srcFolder) return rv;

    nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
    if (NS_FAILED(rv) || !dstFolder) return rv;
    
    rv = srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
    if(NS_FAILED(rv)) return rv;
    rv = dstFolder->GetMsgDatabase(nsnull, getter_AddRefs(dstDB));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count = m_srcKeyArray.GetSize();
    PRUint32 i;
    nsCOMPtr<nsIMsgDBHdr> oldHdr;
    nsCOMPtr<nsIMsgDBHdr> newHdr;

    nsCOMPtr<nsISupportsArray> srcMessages;
    NS_NewISupportsArray(getter_AddRefs(srcMessages));
    nsCOMPtr <nsISupports> msgSupports;
    
    for (i=0; i<count; i++)
    {
        rv = srcDB->GetMsgHdrForKey(m_srcKeyArray.GetAt(i), 
                                    getter_AddRefs(oldHdr));
        NS_ASSERTION(oldHdr, "fatal ... cannot get old msg header\n");

        if (NS_SUCCEEDED(rv) && oldHdr)
        {
            msgSupports =do_QueryInterface(oldHdr);
            srcMessages->AppendElement(msgSupports);
            
            rv = dstDB->CopyHdrFromExistingHdr(m_dstKeyArray.GetAt(i),
                                               oldHdr, PR_TRUE,
                                               getter_AddRefs(newHdr));
            NS_ASSERTION(newHdr, "fatal ... cannot get new msg header\n");
            if (NS_SUCCEEDED(rv) && newHdr)
            {
                if (m_dstSizeArray.GetSize() > i)
                    rv = newHdr->SetMessageSize(m_dstSizeArray.GetAt(i));
                dstDB->UndoDelete(newHdr);
            }
        }
    }
    dstDB->SetSummaryValid(PR_TRUE);
    dstDB->Commit(nsMsgDBCommitType::kLargeCommit);

    if (m_isMove)
    {
        if (m_srcIsImap4)
        {
            // protect against a bogus undo txn without any source keys
            // see bug #179856 for details
            NS_ASSERTION(m_srcKeyArray.GetSize(), "no source keys");
            if (!m_srcKeyArray.GetSize())
              return NS_ERROR_UNEXPECTED;
          
            PRBool deleteFlag = PR_FALSE; //message is un-deleted- we are trying to redo
            CheckForToggleDelete(srcFolder, m_srcKeyArray.GetAt(0), &deleteFlag); // there could have been a toggle
            rv = UndoImapDeleteFlag(srcFolder, m_srcKeyArray, deleteFlag);
        }
        else
        {
            nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(srcFolder);
            if (localFolder)
              localFolder->MarkMsgsOnPop3Server(srcMessages, POP3_DELETE /*deleteMsgs*/);

            rv = srcDB->DeleteMessages(&m_srcKeyArray, nsnull);
            srcDB->SetSummaryValid(PR_TRUE);
            srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
        }
    }

    return rv;
}

NS_IMPL_ISUPPORTS1(nsLocalUndoFolderListener, nsIFolderListener)

nsLocalUndoFolderListener::nsLocalUndoFolderListener(nsLocalMoveCopyMsgTxn *aTxn, nsIMsgFolder *aFolder)
{
  mTxn = aTxn;
  mFolder = aFolder;
}

nsLocalUndoFolderListener::~nsLocalUndoFolderListener()
{
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemAdded(nsIRDFResource *parentItem, nsISupports *item)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemRemoved(nsIRDFResource *parentItem, nsISupports *item)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemPropertyChanged(nsIRDFResource *item, nsIAtom *property, const char *oldValue, const char *newValue)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemIntPropertyChanged(nsIRDFResource *item, nsIAtom *property, PRInt32 oldValue, PRInt32 newValue)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemBoolPropertyChanged(nsIRDFResource *item, nsIAtom *property, PRBool oldValue, PRBool newValue)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemUnicharPropertyChanged(nsIRDFResource *item, nsIAtom *property, const PRUnichar *oldValue, const PRUnichar *newValue)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
    return NS_OK;
}

NS_IMETHODIMP nsLocalUndoFolderListener::OnItemEvent(nsIMsgFolder *item, nsIAtom *event)
{
  nsCOMPtr <nsIAtom> folderLoadedAtom = do_GetAtom("FolderLoaded");
  nsCOMPtr <nsIMsgFolder> itemFolder = do_QueryInterface(item);
  if (mTxn && mFolder && folderLoadedAtom == event && item == mFolder)
    return mTxn->UndoTransactionInternal();

  return NS_ERROR_FAILURE;
}
