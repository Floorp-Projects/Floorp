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
#include "nsMsgBaseCID.h"

#ifdef XP_PC
#include <windows.h>
#endif 

static NS_DEFINE_CID(kMsgCopyServiceCID, NS_MSGCOPYSERVICE_CID);
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
							nsITransactionManager* txnMgr);

	NS_IMETHOD CopyMessage(nsIFileSpec* fileSpec,
						   nsIMsgFolder* dstFolder,
						   nsITransactionManager* txnMgr);

	NS_IMETHOD NotifyCompletion(nsISupports* aSupport, /* store src folder */
								nsIMsgFolder* dstFolder);

    NS_IMETHOD RegisterListener(nsIMsgCopyServiceListener* aListener,
                                nsISupports* aSupport, /* src folder or file */
                                                       /* spec */
                                nsIMsgFolder* dstFolder,
                                nsISupports* listenerData);
    NS_IMETHOD UnregisterListener(nsIMsgCopyServiceListener* theListener);
};

nsMsgCopyService::nsMsgCopyService()
{
	NS_INIT_REFCNT();
}

nsMsgCopyService::~nsMsgCopyService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsMsgCopyService, nsIMsgCopyService::GetIID());

NS_IMETHODIMP
nsMsgCopyService::CopyMessages(nsIMsgFolder* srcFolder, /* UI src foler */
                               nsISupportsArray* messages,
                               nsIMsgFolder* dstFolder,
                               PRBool isMove,
                               nsITransactionManager* txnMgr)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgCopyService::CopyMessage(nsIFileSpec* fileSpec,
                              nsIMsgFolder* dstFolder,
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

NS_IMETHODIMP
nsMsgCopyService::RegisterListener(nsIMsgCopyServiceListener* aListener,
                                   nsISupports* aSupport,
                                   nsIMsgFolder* dstFolder,
                                   nsISupports* listenerData)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMsgCopyService::UnregisterListener(nsIMsgCopyServiceListener* theListener)
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
