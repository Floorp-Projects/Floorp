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
 * Copyright (C) 1998, 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h" // for precompiled headers
#include "nsMsgImapCID.h"
#include "nsIMsgHdr.h"
#include "nsImapUndoTxn.h"
#include "nsXPIDLString.h"
#include "nsIIMAPHostSessionList.h"
#include "nsIMsgIncomingServer.h"


static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);


nsImapMoveCopyMsgTxn::nsImapMoveCopyMsgTxn() :
    m_idsAreUids(PR_FALSE), m_isMove(PR_FALSE), m_srcIsPop3(PR_FALSE)
{
}

nsImapMoveCopyMsgTxn::nsImapMoveCopyMsgTxn(
	nsIMsgFolder* srcFolder, nsMsgKeyArray* srcKeyArray, 
	const char* srcMsgIdString, nsIMsgFolder* dstFolder,
	PRBool idsAreUids, PRBool isMove,
	nsIEventQueue* eventQueue, nsIUrlListener* urlListener) :
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
	NS_NewISupportsArray(getter_AddRefs(m_srcHdrs));
    m_srcMsgIdString = srcMsgIdString;
    m_idsAreUids = idsAreUids;
    m_isMove = isMove;
	m_srcFolder = getter_AddRefs(NS_GetWeakReference(srcFolder));
	m_dstFolder = getter_AddRefs(NS_GetWeakReference(dstFolder));
	m_eventQueue = do_QueryInterface(eventQueue, &rv);
	if (urlListener)
		m_urlListener = do_QueryInterface(urlListener, &rv);
	m_srcKeyArray.CopyArray(srcKeyArray);
	m_dupKeyArray.CopyArray(srcKeyArray);
    nsXPIDLCString uri;
    rv = srcFolder->GetURI(getter_Copies(uri));
    nsCString protocolType(uri);
    protocolType.SetLength(protocolType.FindChar(':'));
    // ** jt -- only do this for mailbox protocol
    if (protocolType.EqualsIgnoreCase("mailbox"))
    {
        m_srcIsPop3 = PR_TRUE;
        PRUint32 i, count = m_srcKeyArray.GetSize();
        nsCOMPtr<nsIMsgDatabase> srcDB;
        rv = srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIMsgDBHdr> srcHdr;
        nsCOMPtr<nsIMsgDBHdr> copySrcHdr;
		nsMsgKey pseudoKey;
	
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
				if (isMove)
				{
                    srcDB->GetNextPseudoMsgKey(&pseudoKey);
                    pseudoKey--;
				    m_dupKeyArray.SetAt(i,pseudoKey);
                    rv = srcDB->CopyHdrFromExistingHdr(pseudoKey,
                                                       srcHdr, PR_FALSE,
                                                       getter_AddRefs(copySrcHdr));
				    if (NS_SUCCEEDED(rv)) 
                    {
				      nsCOMPtr<nsISupports> supports = do_QueryInterface(copySrcHdr);
				      m_srcHdrs->AppendElement(supports);
					}
				}
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

    if (aIID.Equals(NS_GET_IID(nsImapMoveCopyMsgTxn))) 
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
nsImapMoveCopyMsgTxn::UndoTransaction(void)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIImapService> imapService(do_GetService(kCImapService, &rv));
	if (NS_FAILED(rv)) return rv;
	if (m_isMove || !m_dstFolder)
    {
        if (m_srcIsPop3)
        {
            rv = UndoMailboxDelete();
            if (NS_FAILED(rv)) return rv;
        }
        else
        {
            nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
            if (NS_FAILED(rv) || !srcFolder) return rv;
            nsCOMPtr<nsIUrlListener> srcListener =
                do_QueryInterface(srcFolder, &rv);
            if (NS_FAILED(rv)) return rv;
            // ** make sure we are in the selected state; use lite select
            // folder so we won't hit performance hard
            rv = imapService->LiteSelectFolder(m_eventQueue, srcFolder,
                                               srcListener, nsnull);
            if (NS_FAILED(rv)) return rv;
            rv = imapService->SubtractMessageFlags(
                m_eventQueue, srcFolder, srcListener, nsnull,
                m_srcMsgIdString.get(), kImapMsgDeletedFlag,
                m_idsAreUids);
            if (NS_FAILED(rv)) return rv;
            if (DeleteIsMoveToTrash(srcFolder))
                rv = imapService->GetHeaders(m_eventQueue, srcFolder,
                                             srcListener, nsnull,
                                             m_srcMsgIdString.get(),
                                             PR_TRUE); 
        }
    }
    if (m_dstKeyArray.GetSize() > 0)
    {
        nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
        if (NS_FAILED(rv) || !dstFolder) return rv;

        nsCOMPtr<nsIUrlListener> dstListener;

        dstListener = do_QueryInterface(dstFolder, &rv);
        if (NS_FAILED(rv)) return rv;
        // ** make sire we are in the selected state; use lite select folder
        // so we won't hit preformace hard
        rv = imapService->LiteSelectFolder(m_eventQueue, dstFolder,
                                           dstListener, nsnull);
        if (NS_FAILED(rv)) return rv;
        rv = imapService->AddMessageFlags(m_eventQueue, dstFolder,
                                          dstListener, nsnull,
                                          m_dstMsgIdString.get(),
                                          kImapMsgDeletedFlag,
                                          m_idsAreUids);
    }
	return rv;
}

NS_IMETHODIMP
nsImapMoveCopyMsgTxn::RedoTransaction(void)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIImapService> imapService(do_GetService(kCImapService, &rv));
	if (NS_FAILED(rv)) return rv;
	if (m_isMove)
    {
        if (m_srcIsPop3)
        {
            rv = RedoMailboxDelete();
            if (NS_FAILED(rv)) return rv;
        }
        else
        {
            nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
            if (NS_FAILED(rv) || !srcFolder) return rv;
            nsCOMPtr<nsIUrlListener> srcListener =
                do_QueryInterface(srcFolder, &rv); 
            if (NS_FAILED(rv)) return rv;
            // ** make sire we are in the selected state; use lite select
            // folder so we won't hit preformace hard
            rv = imapService->LiteSelectFolder(m_eventQueue, srcFolder,
                                               srcListener, nsnull);
            if (NS_FAILED(rv)) return rv;

            rv = imapService->AddMessageFlags(m_eventQueue, srcFolder,
                                              srcListener, nsnull,
                                              m_srcMsgIdString.get(),
                                              kImapMsgDeletedFlag,
                                              m_idsAreUids);
        }
    }
    if (m_dstKeyArray.GetSize() > 0)
    {
        nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
        if (NS_FAILED(rv) || !dstFolder) return rv;

        nsCOMPtr<nsIUrlListener> dstListener;

        dstListener = do_QueryInterface(dstFolder, &rv); 
        if (NS_FAILED(rv)) return rv;
        // ** make sire we are in the selected state; use lite select
        // folder so we won't hit preformace hard
        rv = imapService->LiteSelectFolder(m_eventQueue, dstFolder,
                                           dstListener, nsnull);
        if (NS_FAILED(rv)) return rv;
        rv = imapService->SubtractMessageFlags(m_eventQueue, dstFolder,
                                               dstListener, nsnull,
                                               m_dstMsgIdString.get(),
                                               kImapMsgDeletedFlag,
                                               m_idsAreUids);
        if (NS_FAILED(rv)) return rv;
        if (DeleteIsMoveToTrash(dstFolder))
            rv = imapService->GetHeaders(m_eventQueue, dstFolder,
                                         dstListener, nsnull,
                                         m_dstMsgIdString.get(),
                                         PR_TRUE);
    }
	return rv;
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
    m_dstMsgIdString.AppendInt((PRInt32) aKey);
    return NS_OK;
}

nsresult
nsImapMoveCopyMsgTxn::UndoMailboxDelete()
{
    nsresult rv = NS_ERROR_FAILURE;
    // ** jt -- only do this for mailbox protocol
    if (m_srcIsPop3)
    {
        nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
        if (NS_FAILED(rv) || !srcFolder) return rv;

        nsCOMPtr<nsIMsgFolder> dstFolder = do_QueryReferent(m_dstFolder, &rv);
        if (NS_FAILED(rv) || !dstFolder) return rv;

        nsCOMPtr<nsIMsgDatabase> srcDB;
        nsCOMPtr<nsIMsgDatabase> dstDB;
        rv = srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
        if (NS_FAILED(rv)) return rv;
        rv = dstFolder->GetMsgDatabase(nsnull, getter_AddRefs(dstDB));
        if (NS_FAILED(rv)) return rv;
        
        PRUint32 count = m_srcKeyArray.GetSize();
        PRUint32 i;
        nsCOMPtr<nsIMsgDBHdr> oldHdr;
        nsCOMPtr<nsIMsgDBHdr> newHdr;
		nsCOMPtr<nsISupports> aSupport;
        for (i=0; i<count; i++)
        {
           aSupport = getter_AddRefs(m_srcHdrs->ElementAt(i));
           oldHdr = do_QueryInterface(aSupport);
           NS_ASSERTION(oldHdr, "fatal ... cannot get old msg header\n");
           rv = srcDB->CopyHdrFromExistingHdr(m_srcKeyArray.GetAt(i),
                                              oldHdr,PR_TRUE,
                                              getter_AddRefs(newHdr));
           NS_ASSERTION(newHdr, "fatal ... cannot create new header\n");
			
           if (NS_SUCCEEDED(rv) && newHdr)
           {
		        if (i < m_srcSizeArray.GetSize())
                newHdr->SetMessageSize(m_srcSizeArray.GetAt(i));
                srcDB->UndoDelete(newHdr);
		   }
        }
        srcDB->SetSummaryValid(PR_TRUE);
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
        nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryReferent(m_srcFolder, &rv);
        if (NS_FAILED(rv) || !srcFolder) return rv;
        rv = srcFolder->GetMsgDatabase(nsnull, getter_AddRefs(srcDB));
        if (NS_SUCCEEDED(rv))
        {
            srcDB->DeleteMessages(&m_srcKeyArray, nsnull);
            srcDB->SetSummaryValid(PR_TRUE);
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

PRBool nsImapMoveCopyMsgTxn::DeleteIsMoveToTrash(nsIMsgFolder *folder)
{
    nsresult rv;
    PRBool retVal = PR_FALSE;
    nsCOMPtr<nsIMsgIncomingServer> server;
    nsXPIDLCString serverKey;
    if (!folder) return PR_FALSE;
    rv = folder->GetServer(getter_AddRefs(server));
    if (NS_FAILED(rv)) return PR_FALSE;
    rv = server->GetKey(getter_Copies(serverKey));
    if (NS_FAILED(rv) || !serverKey) return PR_FALSE;
    
    nsCOMPtr<nsIImapHostSessionList> hostSession = 
             do_GetService(kCImapHostSessionList, &rv);
    if (NS_FAILED(rv) || !hostSession) return PR_FALSE;
    rv = hostSession->GetDeleteIsMoveToTrashForHost((const char*) serverKey,
                                                    retVal);
    return retVal;
}
