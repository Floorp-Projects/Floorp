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

#include "nsMsgCopyService.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsMsgKeyArray.h"
#include "nspr.h"
#include "nsIMessage.h"
#include "nsIMsgFolder.h"

#ifdef XP_PC
#include <windows.h>
#endif 


typedef enum _nsCopyRequestType
{
    nsCopyMessagesType = 0x0,
    nsCopyFileMessageType = 0x1
} nsCopyRequestType;

class nsCopyRequest;

class nsCopySource
{
public:
    nsCopySource();
    nsCopySource(nsIMsgFolder* srcFolder);
    ~nsCopySource();
    void AddMessage(nsIMessage* aMsg);

    nsCOMPtr<nsIMsgFolder> m_msgFolder;
    nsCOMPtr<nsISupportsArray> m_messageArray;
    PRBool m_processed;
};

class nsCopyRequest 
{
public:
    nsCopyRequest();
    ~nsCopyRequest();

    nsresult Init(nsCopyRequestType type, nsISupports* aSupport,
                  nsIMsgFolder* dstFolder,
                  PRBool bVal, nsIMsgCopyServiceListener* listener,
                  nsITransactionManager* txnMgr);
    nsCopySource* AddNewCopySource(nsIMsgFolder* srcFolder);

    nsCOMPtr<nsISupports> m_srcSupport; // ui source folder or file spec
    nsCOMPtr<nsIMsgFolder> m_dstFolder;
    nsCOMPtr<nsITransactionManager> m_txnMgr;
    nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
    nsCopyRequestType m_requestType;
    PRBool m_isMoveOrDraftOrTemplate;
    PRBool m_processed;
    nsVoidArray m_copySourceArray; // array of nsCopySource
};

class nsMsgCopyService : public nsIMsgCopyService
{
public:
	nsMsgCopyService();
	virtual ~nsMsgCopyService();
	
	NS_DECL_ISUPPORTS 

	// nsIMsgCopyService interface
	NS_IMETHOD CopyMessages(nsIMsgFolder* srcFolder, /* UI src foler */
							nsISupportsArray* messages,
							nsIMsgFolder* dstFolder,
							PRBool isMove,
                            nsIMsgCopyServiceListener* listener,
							nsITransactionManager* txnMgr);

	NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec,
                               nsIMsgFolder* dstFolder,
                               nsIMessage* msgToReplace,
                               PRBool isDraftOrTemplate,
                               nsIMsgCopyServiceListener* listener,
                               nsITransactionManager* txnMgr);

	NS_IMETHOD NotifyCompletion(nsISupports* aSupport, /* store src folder */
								nsIMsgFolder* dstFolder,
                                nsresult result);


private:

    nsresult ClearRequest(nsCopyRequest* aRequest, nsresult rv);
    nsresult DoCopy(nsCopyRequest* aRequest);
    nsresult DoNextCopy();
    nsCopyRequest* FindRequest(nsISupports* aSupport, nsIMsgFolder* dstFolder);

    nsVoidArray m_copyRequests;
};

// ******************** nsCopySource ******************
// 
nsCopySource::nsCopySource() : m_processed(PR_FALSE)
{
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(m_messageArray));
}

nsCopySource::nsCopySource(nsIMsgFolder* srcFolder) :
    m_processed(PR_FALSE)
{
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(m_messageArray));
    m_msgFolder = do_QueryInterface(srcFolder, &rv);
}

nsCopySource::~nsCopySource()
{
}

void nsCopySource::AddMessage(nsIMessage* aMsg)
{
	nsCOMPtr<nsISupports> supports(do_QueryInterface(aMsg));
	if(supports)
		m_messageArray->AppendElement(supports);
}

// ************ nsCopyRequest *****************
// 
nsCopyRequest::nsCopyRequest() :
    m_requestType(nsCopyMessagesType),
    m_isMoveOrDraftOrTemplate(PR_FALSE),
    m_processed(PR_FALSE)
{
}

nsCopyRequest::~nsCopyRequest()
{
    PRInt32 j;
    nsCopySource* ncs;
    
    j = m_copySourceArray.Count();
    while(j-- > 0)
    {
        ncs = (nsCopySource*) m_copySourceArray.ElementAt(j);
        m_copySourceArray.RemoveElementAt(j);
        delete ncs;
    }
}

nsresult
nsCopyRequest::Init(nsCopyRequestType type, nsISupports* aSupport,
                    nsIMsgFolder* dstFolder,
                    PRBool bVal, nsIMsgCopyServiceListener* listener,
                    nsITransactionManager* txnMgr)
{
    nsresult rv = NS_OK;
    m_requestType = type;
    m_srcSupport = do_QueryInterface(aSupport, &rv);
    if (NS_FAILED(rv)) return rv;
    m_dstFolder = do_QueryInterface(dstFolder, &rv);
    if (NS_FAILED(rv)) return rv;
    m_isMoveOrDraftOrTemplate = bVal;
    if (listener)
        m_listener = do_QueryInterface(listener, &rv);
    if (txnMgr)
        m_txnMgr = do_QueryInterface(txnMgr, &rv);
    return rv;
}

nsCopySource*
nsCopyRequest::AddNewCopySource(nsIMsgFolder* srcFolder)
{
    nsCopySource* newSrc = new nsCopySource(srcFolder);
    if (newSrc)
        m_copySourceArray.AppendElement((void*) newSrc);
    return newSrc;
}

// ************* nsMsgCopyService ****************
// 

nsMsgCopyService::nsMsgCopyService()
{
    NS_INIT_REFCNT();
}

nsMsgCopyService::~nsMsgCopyService()
{
    PRInt32 i;
    nsCopyRequest* copyRequest;
    
    i = m_copyRequests.Count();

    while(i-- > 0)
    {
        copyRequest = (nsCopyRequest*) m_copyRequests.ElementAt(i);
        ClearRequest(copyRequest, NS_ERROR_FAILURE);
    }
}

                              
nsresult
nsMsgCopyService::ClearRequest(nsCopyRequest* aRequest, nsresult rv)
{
    PR_CEnterMonitor(this);

    if (aRequest)
    {
        // undo stuff
        if (aRequest->m_copySourceArray.Count() > 1 && 
            aRequest->m_txnMgr)
            aRequest->m_txnMgr->EndBatch();
            
        m_copyRequests.RemoveElement(aRequest);
        if (aRequest->m_listener)
            aRequest->m_listener->OnStopCopy(rv);
        delete aRequest;
    }
    
    PR_CExitMonitor(this);

    return rv;
}

nsresult 
nsMsgCopyService::DoCopy(nsCopyRequest* aRequest)
{
    nsresult rv = NS_ERROR_NULL_POINTER;

    if (aRequest)
    {
        PR_CEnterMonitor(this);
        m_copyRequests.AppendElement((void*) aRequest);
        PR_CExitMonitor(this);
        rv = DoNextCopy();
    }

    return rv;
}

nsresult
nsMsgCopyService::DoNextCopy()
{
    nsresult rv = NS_OK;
    nsCopyRequest* copyRequest = nsnull;
    nsCopySource* copySource = nsnull;
    PRInt32 i, j, cnt, scnt;
    PR_CEnterMonitor(this);

    cnt = m_copyRequests.Count();
    if (cnt > 0)
    {
        // ** jt -- always FIFO
        for (i=0; i < cnt; i++)
        {
            copyRequest = (nsCopyRequest*) m_copyRequests.ElementAt(i);
            scnt = copyRequest->m_copySourceArray.Count();
            if (!copyRequest->m_processed)
            {
                if (scnt <= 0) goto found; // must be CopyFileMessage
                for (j=0; j < scnt; j++)
                {
                    copySource = (nsCopySource*)
                        copyRequest->m_copySourceArray.ElementAt(j);
                    if (!copySource->m_processed) goto found;
                }
                if (j >= scnt) // all processed set the value
                    copyRequest->m_processed = PR_TRUE;
            }
        }
    found:
        if (copyRequest && !copyRequest->m_processed)
        {
            if (copyRequest->m_listener)
                copyRequest->m_listener->OnStartCopy();
            if (copyRequest->m_requestType == nsCopyMessagesType &&
                copySource)
            {
                copySource->m_processed = PR_TRUE;
                rv = copyRequest->m_dstFolder->CopyMessages
                    (copySource->m_msgFolder, copySource->m_messageArray,
                     copyRequest->m_isMoveOrDraftOrTemplate,
                     copyRequest->m_txnMgr, copyRequest->m_listener);
                                                                
            }
            else if (copyRequest->m_requestType == nsCopyFileMessageType)
            {
                nsCOMPtr<nsIFileSpec>
                    aSpec(do_QueryInterface(copyRequest->m_srcSupport, &rv));
                if (NS_SUCCEEDED(rv))
                {
                    // ** in case of saving draft/template; the very first
                    // time we may not have the original message to replace
                    // with; if we do we shall have an instance of copySource
                    nsCOMPtr<nsIMessage> aMessage;
                    if (copySource)
                    {
                        nsCOMPtr<nsISupports> aSupport;
                        aSupport =
                            getter_AddRefs(copySource->m_messageArray->ElementAt(0));
                        aMessage = do_QueryInterface(aSupport, &rv);
                        copySource->m_processed = PR_TRUE;
                    }
                    copyRequest->m_processed = PR_TRUE;
                    rv = copyRequest->m_dstFolder->CopyFileMessage
                        (aSpec, aMessage,
                         copyRequest->m_isMoveOrDraftOrTemplate,
                         copyRequest->m_txnMgr,
                         copyRequest->m_listener);
                }
            }
        }
    }
    
    PR_CExitMonitor(this);
    if (NS_FAILED(rv))
        ClearRequest(copyRequest, rv);
    return rv;
}

nsCopyRequest*
nsMsgCopyService::FindRequest(nsISupports* aSupport,
                              nsIMsgFolder* dstFolder)
{
    nsCopyRequest* copyRequest = nsnull;
    PRInt32 cnt, i;

    PR_CEnterMonitor(this);

    cnt = m_copyRequests.Count();
    for (i=0; i < cnt; i++)
    {
        copyRequest = (nsCopyRequest*) m_copyRequests.ElementAt(i);
        if (copyRequest->m_srcSupport.get() == aSupport &&
            copyRequest->m_dstFolder.get() == dstFolder)
            break;
        else
            copyRequest = nsnull;
    }

    PR_CExitMonitor(this);
    return copyRequest;
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsMsgCopyService, nsCOMTypeInfo<nsIMsgCopyService>::GetIID())

NS_IMETHODIMP
nsMsgCopyService::CopyMessages(nsIMsgFolder* srcFolder, /* UI src foler */
                               nsISupportsArray* messages,
                               nsIMsgFolder* dstFolder,
                               PRBool isMove,
                               nsIMsgCopyServiceListener* listener,
                               nsITransactionManager* txnMgr)
{
    nsCopyRequest* copyRequest;
    nsCopySource* copySource = nsnull;
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsISupportsArray> msgArray;
    PRUint32 i, cnt;
    nsCOMPtr<nsIMessage> msg;
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsCOMPtr<nsISupports> aSupport;

    if (!srcFolder || !messages || !dstFolder) return rv;

    copyRequest = new nsCopyRequest();
    if (!copyRequest) return rv;
    aSupport = do_QueryInterface(srcFolder, &rv);

    rv = copyRequest->Init(nsCopyMessagesType, aSupport, dstFolder, 
                           isMove, listener, txnMgr);
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewISupportsArray(getter_AddRefs(msgArray));
    if (NS_FAILED(rv)) goto done;

    messages->Count(&cnt);

    // duplicate the message array so we could sort the messages by it's
    // folder easily
    for (i=0; i<cnt; i++)
    {
        aSupport = getter_AddRefs(messages->ElementAt(i));
        msgArray->AppendElement(aSupport);
    }

    rv = msgArray->Count(&rv);
    if (NS_FAILED(rv)) goto done;

    while (cnt-- > 0)
    {
        aSupport = getter_AddRefs(msgArray->ElementAt(cnt));
        msg = do_QueryInterface(aSupport, &rv);
        if (NS_FAILED(rv)) goto done;

        rv = msg->GetMsgFolder(getter_AddRefs(curFolder));
        if (NS_FAILED(rv)) goto done;
        if (!copySource)
        {
            copySource = copyRequest->AddNewCopySource(curFolder);
            if (!copySource)
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
                goto done;
            }
        }

        if (curFolder == copySource->m_msgFolder)
        {
            copySource->AddMessage(msg);
            msgArray->RemoveElementAt(cnt);
        }

        if (cnt == 0)
        {
            rv = msgArray->Count(&cnt);
            if (cnt > 0)
                copySource = nsnull; // * force to create a new one and
                                     // * continue grouping the messages
        }
    }

    // undo stuff
    if (NS_SUCCEEDED(rv) && copyRequest->m_copySourceArray.Count() > 1 &&
        copyRequest->m_txnMgr)
        copyRequest->m_txnMgr->BeginBatch();

done:
    
    if (NS_FAILED(rv))
        delete copyRequest;
    else
        rv = DoCopy(copyRequest);
    
    msgArray->Clear();

    return rv;
}

NS_IMETHODIMP
nsMsgCopyService::CopyFileMessage(nsIFileSpec* fileSpec,
                                  nsIMsgFolder* dstFolder,
                                  nsIMessage* msgToReplace,
                                  PRBool isDraft,
                                  nsIMsgCopyServiceListener* listener,
                                  nsITransactionManager* txnMgr)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCopyRequest* copyRequest;
    nsCopySource* copySource = nsnull;
    nsCOMPtr<nsISupports> aSupport;

    if (!fileSpec || !dstFolder) return rv;
    copyRequest = new nsCopyRequest();
    if (!copyRequest) return rv;
    aSupport = do_QueryInterface(fileSpec, &rv);
    if (NS_FAILED(rv)) goto done;

    rv = copyRequest->Init(nsCopyFileMessageType, aSupport, dstFolder,
                           isDraft, listener, txnMgr);
    if (NS_FAILED(rv)) goto done;

    if (msgToReplace)
    {
        copySource = copyRequest->AddNewCopySource(dstFolder);
        if (!copySource)
        {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        copySource->AddMessage(msgToReplace);
    }

done:
    if (NS_FAILED(rv))
    {
        delete copyRequest;
    }
    else
    {
        rv = DoCopy(copyRequest);
    }

    return rv;
}

NS_IMETHODIMP
nsMsgCopyService::NotifyCompletion(nsISupports* aSupport,
                                   nsIMsgFolder* dstFolder,
                                   nsresult result)
{
    nsresult rv;
    rv = DoNextCopy();
    nsCopyRequest* copyRequest = FindRequest(aSupport, dstFolder);
    if (copyRequest && copyRequest->m_processed)
        ClearRequest(copyRequest, result);
    return rv;
}

nsresult
NS_NewMsgCopyService(const nsIID& iid, void **result)
{
	nsMsgCopyService* copyService;
	if (!result) return NS_ERROR_NULL_POINTER;
	copyService = new nsMsgCopyService();
	if (!copyService) return NS_ERROR_OUT_OF_MEMORY;
	return copyService->QueryInterface(iid, result);
}
