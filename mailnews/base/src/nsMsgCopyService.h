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
 */

#ifndef nsMsgCopyService_h__
#define nsMsgCopyService_h__

#include "nscore.h"
#include "nsIMsgCopyService.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIMessage.h"
#include "nsIMsgWindow.h"

typedef enum _nsCopyRequestType
{
    nsCopyMessagesType = 0x0,
    nsCopyFileMessageType = 0x1,
    nsCopyFoldersType = 0x2
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
                  nsIMsgWindow *msgWindow);
    nsCopySource* AddNewCopySource(nsIMsgFolder* srcFolder);

    nsCOMPtr<nsISupports> m_srcSupport; // ui source folder or file spec
    nsCOMPtr<nsIMsgFolder> m_dstFolder;
    nsCOMPtr<nsIMsgWindow> m_msgWindow;
    nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
	nsCOMPtr<nsITransactionManager> m_txnMgr;
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

	NS_DECL_NSIMSGCOPYSERVICE

private:

    nsresult ClearRequest(nsCopyRequest* aRequest, nsresult rv);
    nsresult DoCopy(nsCopyRequest* aRequest);
    nsresult DoNextCopy();
    nsCopyRequest* FindRequest(nsISupports* aSupport, nsIMsgFolder* dstFolder);

    nsVoidArray m_copyRequests;
};


NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgCopyService(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif 
