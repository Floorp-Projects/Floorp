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

#include "nsLocalUndoTxn.h"

static NS_DEFINE_CID(kMailboxServiceCID, NS_IMAILBOXSERVICE_IID);

nsLocalMoveCopyMsgTxn::nsLocalMoveCopyMsgTxn()
{
	Init(nsnull, nsnull, PR_FALSE);
}

nsLocalMoveCopyMsgTxn::nsLocalMoveCopyMsgTxn(nsIMsgFolder* srcFolder,
											nsIMsgFolder* dstFolder,
											PRBool isMove)
{
	Init(srcFolder, dstFolder, isMove);
}

nsLocalMoveCopyMsgTxn::~nsLocalMoveCopyMsgTxn()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsLocalMoveCopyMsgTxn, nsMsgTxn, nsLocalMoveCopyMsgTxn)

nsresult
nsLocalMoveCopyMsgTxn::Init(nsIMsgFolder* srcFolder, nsIMsgFolder* dstFolder,
							PRBool isMove)
{
	nsresult rv;
	rv = SetSrcFolder(srcFolder);
	rv = SetDstFolder(dstFolder);
	m_isMove = isMove;
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
    {
        PRUint32 count = m_srcKeyArray.GetSize();
        PRUint32 i;
        nsCOMPtr<nsIMsgDBHdr> oldHdr;
        nsCOMPtr<nsIMsgDBHdr> newHdr;
        for (i=0; i<count; i++)
        {
            rv = dstDB->GetMsgHdrForKey(m_dstKeyArray.GetAt(i), 
                                        getter_AddRefs(oldHdr));
            if (m_isMove && NS_SUCCEEDED(rv))
            {
                rv = srcDB->CopyHdrFromExistingHdr(m_srcKeyArray.GetAt(i),
                                                   oldHdr,
                                                   getter_AddRefs(newHdr));
                if (NS_SUCCEEDED(rv))
                    srcDB->UndoDelete(newHdr);
                srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
            }
            if (oldHdr)
            {
                rv = dstDB->DeleteHeader(oldHdr, nsnull, PR_TRUE, PR_TRUE);
                dstDB->Commit(nsMsgDBCommitType::kLargeCommit);
            }
        }
    }
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
    {
        PRUint32 count = m_srcKeyArray.GetSize();
        PRUint32 i;
        nsCOMPtr<nsIMsgDBHdr> oldHdr;
        nsCOMPtr<nsIMsgDBHdr> newHdr;
        for (i=0; i<count; i++)
        {
            rv = srcDB->GetMsgHdrForKey(m_srcKeyArray.GetAt(i), 
                                        getter_AddRefs(oldHdr));
            if (NS_SUCCEEDED(rv))
            {
                rv = dstDB->CopyHdrFromExistingHdr(m_dstKeyArray.GetAt(i),
                                                   oldHdr,
                                                   getter_AddRefs(newHdr));
                if (NS_SUCCEEDED(rv))
                    dstDB->UndoDelete(newHdr);
                dstDB->Commit(nsMsgDBCommitType::kLargeCommit);
            }
            if (m_isMove && oldHdr)
            {
                rv = srcDB->DeleteHeader(oldHdr, nsnull, PR_TRUE, PR_TRUE);
                srcDB->Commit(nsMsgDBCommitType::kLargeCommit);
            }
        }
    }
    return rv;
}
