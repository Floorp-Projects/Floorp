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
 * Portions created by the Initial Developer are Copyright (C) 1998, 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"
#include "nsIMsgHdr.h"
#include "nsLocalUndoTxn.h"
#include "nsImapCore.h"
#include "nsMsgImapCID.h"
#include "nsIImapService.h"
#include "nsIUrlListener.h"
#include "nsIEventQueueService.h"

static NS_DEFINE_CID(kMailboxServiceCID, NS_IMAILBOXSERVICE_IID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
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

    char *uri = nsnull;
    if (!srcFolder) return rv;
    rv = srcFolder->GetURI(&uri);
    nsCString protocolType(uri);
    PR_FREEIF(uri);
    protocolType.SetLength(protocolType.FindChar(':'));
    if (protocolType.EqualsIgnoreCase("imap"))
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
          m_srcFolder = getter_AddRefs(NS_GetWeakReference(srcFolder, &rv));
	return rv;
}

nsresult
nsLocalMoveCopyMsgTxn::SetDstFolder(nsIMsgFolder* dstFolder)
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (dstFolder)
          m_dstFolder = getter_AddRefs(NS_GetWeakReference(dstFolder, &rv));
	return rv;
}

nsresult
nsLocalMoveCopyMsgTxn::AddSrcKey(nsMsgKey aKey)
{
	m_srcKeyArray.Add(aKey);
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
                                          PRBool addFlag)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (m_srcIsImap4)
    {
        nsCOMPtr<nsIImapService> imapService = 
                 do_GetService(kCImapService, &rv);
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsIUrlListener> urlListener;
            nsCString msgIds;
            PRUint32 i, count = keyArray.GetSize();

            urlListener = do_QueryInterface(folder, &rv);

            for (i=0; i < count; i++)
            {
                if (msgIds.Length() > 0)
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
                    if (addFlag)
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

    if (m_isMove)
    {
        if (m_srcIsImap4)
        {
            rv = UndoImapDeleteFlag(srcFolder, m_srcKeyArray, PR_FALSE);
        }
        else
        {
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
                        srcDB->UndoDelete(newHdr);
                }
            }
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
    
    
    for (i=0; i<count; i++)
    {
        rv = srcDB->GetMsgHdrForKey(m_srcKeyArray.GetAt(i), 
                                    getter_AddRefs(oldHdr));
        NS_ASSERTION(oldHdr, "fatal ... cannot get old msg header\n");

        if (NS_SUCCEEDED(rv) && oldHdr)
        {
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
            rv = UndoImapDeleteFlag(srcFolder, m_srcKeyArray, PR_TRUE);
        }
        else
        {
            rv = srcDB->DeleteMessages(&m_srcKeyArray, nsnull);
            srcDB->SetSummaryValid(PR_TRUE);
            srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
        }
    }

    return rv;
}
