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
    m_idsAreUids(PR_FALSE), m_isMove(PR_FALSE), m_srcIsPop3(PR_FALSE)
{
}

nsImapMoveCopyMsgTxn::nsImapMoveCopyMsgTxn(
	nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray, 
	const char* srcMsgIdString, nsIMsgFolder* dstFolder,
	PRBool idsAreUids, PRBool isMove,
	nsIEventQueue* eventQueue, nsIUrlListener* urlListener) :
	m_srcMsgIdString("", eOneByte), m_dstMsgIdString("", eOneByte),
    m_idsAreUids(PR_FALSE), m_isMove(PR_FALSE), m_srcIsPop3(PR_FALSE)
{
    Init(srcFolder, srcKeyArray, srcMsgIdString, dstFolder, idsAreUids,
         isMove, eventQueue, urlListener);
}

nsresult
nsImapMoveCopyMsgTxn::Init(
	nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray, 
	const char* srcMsgIdString, nsIMsgFolder* dstFolder,
	PRBool idsAreUids, PRBool isMove,
	nsIEventQueue* eventQueue, nsIUrlListener* urlListener)
{
	nsresult rv;
    m_srcMsgIdString = srcMsgIdString;
    m_idsAreUids = idsAreUids;
    m_isMove = isMove;
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
    char *uri = nsnull;
    rv = m_srcFolder->GetURI(&uri);
    nsString2 protocolType(uri, eOneByte);
    PR_FREEIF(uri);
    protocolType.SetLength(protocolType.FindChar(':'));
    // ** jt -- only do this for mailbox protocol
    if (protocolType.EqualsIgnoreCase("mailbox"))
    {
        m_srcIsPop3 = PR_TRUE;
        PRUint32 i, count = m_srcKeyArray.GetSize();
        nsCOMPtr<nsIMsgDatabase> srcDB;
        rv = m_srcFolder->GetMsgDatabase(getter_AddRefs(srcDB));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIMsgDBHdr> srcHdr;
        
        for (i=0; i<count; i++)
        {
            rv = srcDB->GetMsgHdrForKey(m_srcKeyArray.GetAt(i),
                                        getter_AddRefs(srcHdr));
            if (NS_SUCCEEDED(rv))
            {
                PRUint32 msgSize;
                rv = srcHdr->GetMessageSize(&msgSize);
                if (NS_SUCCEEDED(rv))
                    m_srcSizeArray.Add(msgSize);
            }
        }
    }
    return rv;
}

nsImapMoveCopyMsgTxn::~nsImapMoveCopyMsgTxn()
{
}

NS_IMPL_ADDREF_INHERITED(nsImapMoveCopyMsgTxn, nsMsgTxn)
NS_IMPL_RELEASE_INHERITED(nsImapMoveCopyMsgTxn, nsMsgTxn)

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (!aInstancePtr) return NS_ERROR_NULL_POINTER;

    *aInstancePtr = nsnull;

    if (aIID.Equals(nsImapMoveCopyMsgTxn::GetIID())) 
    {
        *aInstancePtr = NS_STATIC_CAST(nsImapMoveCopyMsgTxn*, this);
    }

    if (*aInstancePtr)
    {
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsMsgTxn::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::Undo(void)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIImapService, imapService, kCImapService, &rv);
	if (NS_FAILED(rv)) return rv;
	if (m_isMove)
    {
        if (m_srcIsPop3)
        {
            rv = UndoMailboxDelete();
        }
        else
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
        nsCOMPtr<nsIUrlListener> dstListener;

        dstListener = do_QueryInterface(m_dstFolder, &rv);
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
        if (m_srcIsPop3)
        {
            rv = RedoMailboxDelete();
        }
        else
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
        nsCOMPtr<nsIUrlListener> dstListener;

        dstListener = do_QueryInterface(m_dstFolder, &rv); 

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
    if (m_dstMsgIdString.Length() > 0)
        m_dstMsgIdString.Append(",");
    m_dstMsgIdString.Append((PRInt32) aKey);
    return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::UndoMailboxDelete()
{
    nsresult rv = NS_ERROR_FAILURE;
    // ** jt -- only do this for mailbox protocol
    if (m_srcIsPop3)
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
            NS_ASSERTION(oldHdr, "fatal ... cannot get old msg header\n");

            if (NS_SUCCEEDED(rv) && oldHdr)
            {
                rv = srcDB->CopyHdrFromExistingHdr(m_srcKeyArray.GetAt(i),
                                                   oldHdr,
                                                   getter_AddRefs(newHdr));
                NS_ASSERTION(newHdr, "fatal ... cannot create new header\n");
                if (NS_SUCCEEDED(rv) && newHdr)
                {
                    if (i < m_srcSizeArray.GetSize())
                        newHdr->SetMessageSize(m_srcSizeArray.GetAt(i));
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
    if (m_srcIsPop3)
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


