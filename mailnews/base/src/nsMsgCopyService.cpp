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
#include "prmon.h"

#ifdef XP_PC
#include <windows.h>
#endif 

typedef struct _nsMsgStore
{
    nsCOMPtr<nsIMsgFolder> msgFolder;
    nsMsgKeyArray keyArray;
} nsMsgStore;

typedef struct _nsCopyRequest 
{
    nsCOMPtr<nsISupports> srcSupport; // a nsIMsgFolder or nsFileSpec
    nsCOMPtr<nsIMsgFolder> dstFolder;
    nsCOMPtr<nsITransactionManager> txnMgr;
    nsCOMPtr<nsIMsgCopyServiceListener> listener;
    nsCOMPtr<nsISupports> listenerData;
    PRBool isMove;
    nsVoidArray msgStoreArray; // array of nsMsgStore
} nsCopyRequest;

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
                            nsISupports* listenerData,
							nsITransactionManager* txnMgr);

	NS_IMETHOD CopyFileMessage(nsIFileSpec* fileSpec,
                               nsIMsgFolder* dstFolder,
                               nsIMessage* msgToReplace,
                               PRBool isDraft,
                               nsIMsgCopyServiceListener* listener,
                               nsISupports* listenerData,
                               nsITransactionManager* txnMgr);

	NS_IMETHOD NotifyCompletion(nsISupports* aSupport, /* store src folder */
								nsIMsgFolder* dstFolder);


private:

    nsresult ClearRequest(nsCopyRequest* aRequest, nsresult rv);
    void RemoveAll();
    nsVoidArray m_copyRequests;
};

nsMsgCopyService::nsMsgCopyService()
{
    NS_INIT_REFCNT();
}

nsMsgCopyService::~nsMsgCopyService()
{
    RemoveAll();
}

nsresult
nsMsgCopyService::ClearRequest(nsCopyRequest* aRequest, nsresult rv)
{
    nsMsgStore* nms;
    PRInt32 j;

    if (aRequest)
    {
        j = aRequest->msgStoreArray.Count();
        while(j-- > 0)
        {
            nms = (nsMsgStore*) aRequest->msgStoreArray.ElementAt(j);
            aRequest->msgStoreArray.RemoveElementAt(j);
            delete nms;
        }
        m_copyRequests.RemoveElement(aRequest);
        if (aRequest->listener)
            aRequest->listener->OnStopCopy(rv, aRequest->listenerData);
        delete aRequest;
    }
    return rv;
}

void
nsMsgCopyService::RemoveAll()
{
    PRInt32 i;
    nsCopyRequest* ncr;
    
    i = m_copyRequests.Count();

    while(i-- > 0)
    {
        ncr = (nsCopyRequest*) m_copyRequests.ElementAt(i);
        ClearRequest(ncr, NS_ERROR_FAILURE);
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsMsgCopyService, nsIMsgCopyService::GetIID())

NS_IMETHODIMP
nsMsgCopyService::CopyMessages(nsIMsgFolder* srcFolder, /* UI src foler */
                               nsISupportsArray* messages,
                               nsIMsgFolder* dstFolder,
                               PRBool isMove,
                               nsIMsgCopyServiceListener* listener,
                               nsISupports* listenerData,
                               nsITransactionManager* txnMgr)
{
#if 0
    nsCopyRequest* ncr;
    nsMsgStore* nms;
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsVoidArray msgArray;
    PRUint32 i, cnt;
    nsCOMPtr<nsIMessage> msg;

    if (!srcFolder || !messages || !dstFolder) return rv;

    ncr = new nsCopyRequest;
    if (!ncr) return rv;

    nms = new nsMsgStore;

    cnt = messages->Count();

    // duplicate the message array so we could sort the messages by it's
    // folder easily
    for (i=0; i<cnt; i++)
        msgArray.AppendElement((void*) messages->ElementAt(i));
#endif 
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgCopyService::CopyFileMessage(nsIFileSpec* fileSpec,
                                  nsIMsgFolder* dstFolder,
                                  nsIMessage* msgToReplace,
                                  PRBool isDraft,
                                  nsIMsgCopyServiceListener* listener,
                                  nsISupports* listenerData,
                                  nsITransactionManager* txnMgr)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgCopyService::NotifyCompletion(nsISupports* aSupport,
                                   nsIMsgFolder* dstFolder)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
