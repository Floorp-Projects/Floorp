/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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

    if (aIID.Equals(nsLocalMoveCopyMsgTxn::GetIID())) 
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
nsLocalMoveCopyMsgTxn::SetSrcFolder(nsIMsgFolder* srcFolder)
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (srcFolder)
		m_srcFolder = do_QueryInterface(srcFolder, &rv);
	return rv;
}

nsresult
nsLocalMoveCopyMsgTxn::SetDstFolder(nsIMsgFolder* dstFolder)
{
	nsresult rv = NS_ERROR_NULL_POINTER;
	if (dstFolder)
		m_dstFolder = do_QueryInterface(dstFolder, &rv);
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
nsLocalMoveCopyMsgTxn::SetUndoString(nsString *aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	m_undoString = *aString;
	return NS_OK;
}

nsresult
nsLocalMoveCopyMsgTxn::SetRedoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	m_redoString = *aString;
	return NS_OK;
}

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::GetUndoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	*aString = m_undoString;
	return NS_OK;
}

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::GetRedoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	*aString = m_redoString;
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
        NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
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
                msgIds.Append((PRInt32) keyArray.GetAt(i));
            }
            nsCOMPtr<nsIEventQueue> eventQueue;
            NS_WITH_SERVICE(nsIEventQueueService, pEventQService,
                            kEventQueueServiceCID, &rv);  
            if (NS_SUCCEEDED(rv) && pEventQService)
            {
                pEventQService->GetThreadEventQueue(PR_GetCurrentThread(),
                                              getter_AddRefs(eventQueue));
                if (eventQueue)
                {
                    if (addFlag)
                        rv =imapService->AddMessageFlags(eventQueue, folder,
                                                        urlListener, nsnull,
                                                        msgIds.GetBuffer(),
                                                        kImapMsgDeletedFlag,
                                                        PR_TRUE);
                    else
                        rv = imapService->SubtractMessageFlags(eventQueue,
                                                              folder,
                                                         urlListener, nsnull,
                                                         msgIds.GetBuffer(),
                                                         kImapMsgDeletedFlag,
                                                         PR_TRUE);
                    if (NS_SUCCEEDED(rv))
                        imapService->SelectFolder(eventQueue, folder,
                                                  urlListener, nsnull, nsnull);
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
nsLocalMoveCopyMsgTxn::Undo()
{
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIMsgDatabase> srcDB;
    nsCOMPtr<nsIMsgDatabase> dstDB;
    
    rv = m_srcFolder->GetMsgDatabase(getter_AddRefs(srcDB));
    if(NS_FAILED(rv)) return rv;
    rv = m_dstFolder->GetMsgDatabase(getter_AddRefs(dstDB));
    if (NS_FAILED(rv)) return rv;

    PRUint32 count = m_srcKeyArray.GetSize();
    PRUint32 i;
    nsCOMPtr<nsIMsgDBHdr> oldHdr;
    nsCOMPtr<nsIMsgDBHdr> newHdr;

    if (m_isMove)
    {
        if (m_srcIsImap4)
        {
            rv = UndoImapDeleteFlag(m_srcFolder, m_srcKeyArray, PR_FALSE);
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
                                                       oldHdr,
                                                       getter_AddRefs(newHdr));
                    NS_ASSERTION(newHdr, 
                                 "fatal ... cannot create new msg header\n");
                    if (NS_SUCCEEDED(rv) && newHdr)
                        srcDB->UndoDelete(newHdr);
                }
            }
        }
        srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
    }

    dstDB->DeleteMessages(&m_dstKeyArray, nsnull);
    dstDB->Commit(nsMsgDBCommitType::kLargeCommit);

    return rv;
}

NS_IMETHODIMP
nsLocalMoveCopyMsgTxn::Redo()
{
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIMsgDatabase> srcDB;
    nsCOMPtr<nsIMsgDatabase> dstDB;
    
    rv = m_srcFolder->GetMsgDatabase(getter_AddRefs(srcDB));
    if(NS_FAILED(rv)) return rv;
    rv = m_dstFolder->GetMsgDatabase(getter_AddRefs(dstDB));
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
                                               oldHdr,
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
    dstDB->Commit(nsMsgDBCommitType::kLargeCommit);

    if (m_isMove)
    {
        if (m_srcIsImap4)
        {
            rv = UndoImapDeleteFlag(m_srcFolder, m_srcKeyArray, PR_TRUE);
        }
        else
        {
            rv = srcDB->DeleteMessages(&m_srcKeyArray, nsnull);
            srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
        }
    }

    return rv;
}
