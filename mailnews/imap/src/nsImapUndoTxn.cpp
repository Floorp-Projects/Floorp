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

#include "msgCore.h" // for precompiled headers
#include "nsMsgImapCID.h"
#include "nsImapUndoTxn.h"


static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);

nsImapMoveCopyMsgTxn::nsImapMoveCopyMsgTxn() :
    m_srcMsgIdString("", eOneByte), m_dstMsgIdString("", eOneByte),
    m_idsAreUids(PR_FALSE), m_isMove(PR_FALSE)
{
}

nsImapMoveCopyMsgTxn::nsImapMoveCopyMsgTxn(
	nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray, 
	const char* srcMsgIdString, nsIMsgFolder* dstFolder,
	PRBool idsAreUids, PRBool isMove,
	nsIEventQueue* eventQueue, nsIUrlListener* urlListener) :
	m_srcMsgIdString(srcMsgIdString, eOneByte), 
	m_dstMsgIdString("", eOneByte), m_idsAreUids(idsAreUids), m_isMove(isMove)
{
	nsresult rv;
	m_srcFolder = do_QueryInterface(srcFolder, &rv);
	m_dstFolder = do_QueryInterface(dstFolder, &rv);
	m_eventQueue = do_QueryInterface(eventQueue, &rv);
	if (urlListener)
		m_urlListener = do_QueryInterface(urlListener, &rv);
	m_srcKeyArray.CopyArray(srcKeyArray);
	if (srcKeyArray->GetSize() > 1)
	{
		if (isMove)
		{
			m_undoString = "Undo Move Messages";
			m_redoString = "Redo Move Messages";
		}
		else
		{
			m_undoString = "Undo Copy Messages";
			m_redoString = "Redo Copy Messages";
		}
	}
	else
	{
		if (isMove)
		{
			m_undoString = "Undo Move Message";
			m_redoString = "Redo Move Message";
		}
		else
		{
			m_undoString = "Undo Copy Message";
			m_redoString = "Redo Copy Message";
		}
	}
}

nsImapMoveCopyMsgTxn::~nsImapMoveCopyMsgTxn()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsImapMoveCopyMsgTxn, nsMsgTxn,	nsImapMoveCopyMsgTxn)

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::Undo(void)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
	if (NS_FAILED(rv)) return rv;
	if (m_isMove)
    {
        rv = UndoMailboxDelete();
        if (NS_FAILED(rv))
        {
            nsCOMPtr<nsIUrlListener> srcListener =
                do_QueryInterface(m_srcFolder, &rv);
            rv = imapService->SubtractMessageFlags(
                m_eventQueue, m_srcFolder, srcListener, nsnull,
                m_srcMsgIdString.GetBuffer(), kImapMsgDeletedFlag,
                m_idsAreUids);
            if (NS_SUCCEEDED(rv))
                rv = imapService->SelectFolder(m_eventQueue, m_srcFolder,
                                               srcListener, nsnull);
        }
    }
    if (m_dstKeyArray.GetSize() > 0)
    {
        nsCOMPtr<nsIUrlListener> dstListener = do_QueryInterface(m_dstFolder,
                                                                 &rv);
        rv = imapService->AddMessageFlags(m_eventQueue, m_dstFolder,
                                          dstListener, nsnull,
                                          m_dstMsgIdString.GetBuffer(),
                                          kImapMsgDeletedFlag,
                                          m_idsAreUids);
        if (NS_SUCCEEDED(rv))
            rv = imapService->SelectFolder(m_eventQueue, m_dstFolder,
                                           dstListener, nsnull);
    }
	return rv;
}

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::Redo(void)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
	if (NS_FAILED(rv)) return rv;
	if (m_isMove)
    {
        rv = RedoMailboxDelete();
        if (NS_FAILED(rv))
        {
            nsCOMPtr<nsIUrlListener> srcListener =
                do_QueryInterface(m_srcFolder, &rv); 
            rv = imapService->AddMessageFlags(m_eventQueue, m_srcFolder,
                                              srcListener, nsnull,
                                              m_srcMsgIdString.GetBuffer(),
                                              kImapMsgDeletedFlag,
                                              m_idsAreUids);
            if (NS_SUCCEEDED(rv))
                rv = imapService->SelectFolder(m_eventQueue, m_srcFolder,
                                               srcListener, nsnull);
        }
    }
    if (m_dstKeyArray.GetSize() > 0)
    {
        nsCOMPtr<nsIUrlListener> dstListener = do_QueryInterface(m_dstFolder,
                                                                 &rv); 
        rv = imapService->SubtractMessageFlags(m_eventQueue, m_dstFolder,
                                               dstListener, nsnull,
                                               m_dstMsgIdString.GetBuffer(),
                                               kImapMsgDeletedFlag,
                                               m_idsAreUids);
        if(NS_SUCCEEDED(rv))
            rv = imapService->SelectFolder(m_eventQueue, m_dstFolder,
                                           dstListener, nsnull);
    }
	return rv;
}

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::GetUndoString(nsString* aString)
{
	if (!aString)
		return NS_ERROR_NULL_POINTER;
	*aString = m_undoString;
	return NS_OK;
}

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::GetRedoString(nsString* aString)
{
	if (!aString)
		return NS_ERROR_NULL_POINTER;
	*aString = m_redoString;
	return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::SetUndoString(nsString *aString)
{
	if (!aString)
		return NS_ERROR_NULL_POINTER;
	m_undoString = *aString;
	return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::SetRedoString(nsString* aString)
{
	if (!aString) return NS_ERROR_NULL_POINTER;
	m_redoString = *aString;
	return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::SetCopyResponseUid(nsMsgKeyArray* aKeyArray,
										 const char* aMsgIdString)
{
	if (!aKeyArray || !aMsgIdString) return NS_ERROR_NULL_POINTER;
	m_dstKeyArray.CopyArray(aKeyArray);
	m_dstMsgIdString = aMsgIdString;
	if (m_dstMsgIdString.Last() == ']')
	{
		PRInt32 len = m_dstMsgIdString.Length();
		m_dstMsgIdString.SetLength(len - 1);
	}
	return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::GetSrcKeyArray(nsMsgKeyArray& srcKeyArray)
{
    srcKeyArray.CopyArray(&m_srcKeyArray);
    return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::GetDstKeyArray(nsMsgKeyArray& dstKeyArray)
{
    dstKeyArray.CopyArray(&m_dstKeyArray);
    return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::AddDstKey(nsMsgKey aKey)
{
    m_dstKeyArray.Add(aKey);
    return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::UndoMailboxDelete()
{
    nsresult rv = NS_ERROR_FAILURE;
    char *uri = nsnull;
    rv = m_srcFolder->GetURI(&uri);
    nsString2 protocolType(uri, eOneByte);
    PR_FREEIF(uri);
    protocolType.SetLength(protocolType.Find(':'));
    // ** jt -- only do this for mailbox protocol
    if (protocolType.EqualsIgnoreCase("mailbox"))
    {
        nsCOMPtr<nsIMsgDatabase> srcDB;
        nsCOMPtr<nsIMsgDatabase> dstDB;
        rv = m_srcFolder->GetMsgDatabase(getter_AddRefs(srcDB));
        if (NS_FAILED(rv)) return rv;
        rv = m_dstFolder->GetMsgDatabase(getter_AddRefs(dstDB));
        if (NS_FAILED(rv)) return rv;
        
        PRUint32 count = m_srcKeyArray.GetSize();
        PRUint32 i;
        nsCOMPtr<nsIMsgDBHdr> oldHdr;
        nsCOMPtr<nsIMsgDBHdr> newHdr;
        for (i=0; i<count; i++)
        {
            rv = dstDB->GetMsgHdrForKey(m_dstKeyArray.GetAt(i),
                                        getter_AddRefs(oldHdr));
            if (NS_SUCCEEDED(rv))
            {
                srcDB->CopyHdrFromExistingHdr(m_srcKeyArray.GetAt(i),
                                              oldHdr,
                                              getter_AddRefs(newHdr));
                if (NS_SUCCEEDED(rv))
                {
                    srcDB->UndoDelete(newHdr);
                }
            }
        }
        srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
        return NS_OK; // always return NS_OK
    }
    else
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}


nsresult
nsImapMoveCopyMsgTxn::RedoMailboxDelete()
{
    nsresult rv = NS_ERROR_FAILURE;
    char *uri = nsnull;
    rv = m_srcFolder->GetURI(&uri);
    nsString2 protocolType(uri, eOneByte);
    PR_FREEIF(uri);
    protocolType.SetLength(protocolType.Find(':'));
    // ** jt -- only do this for mailbox protocol
    if (protocolType.EqualsIgnoreCase("mailbox"))
    {
        nsCOMPtr<nsIMsgDatabase> srcDB;
        rv = m_srcFolder->GetMsgDatabase(getter_AddRefs(srcDB));
        if (NS_SUCCEEDED(rv))
        {
            srcDB->DeleteMessages(&m_srcKeyArray, nsnull);
            srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
        }
        return NS_OK; // always return NS_OK
    }
    else
    {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}


