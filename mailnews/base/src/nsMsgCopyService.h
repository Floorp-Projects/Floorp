/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMsgCopyService_h__
#define nsMsgCopyService_h__

#include "nscore.h"
#include "nsIMsgCopyService.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIMsgHdr.h"
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
    void AddMessage(nsIMsgDBHdr* aMsg);

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
                  nsIMsgWindow *msgWindow, PRBool allowUndo);
    nsCopySource* AddNewCopySource(nsIMsgFolder* srcFolder);

    nsCOMPtr<nsISupports> m_srcSupport; // ui source folder or file spec
    nsCOMPtr<nsIMsgFolder> m_dstFolder;
    nsCOMPtr<nsIMsgWindow> m_msgWindow;
    nsCOMPtr<nsIMsgCopyServiceListener> m_listener;
	nsCOMPtr<nsITransactionManager> m_txnMgr;
    nsCopyRequestType m_requestType;
    PRBool m_isMoveOrDraftOrTemplate;
    PRBool m_allowUndo;
    PRBool m_processed;
    nsString m_dstFolderName;      // used for copy folder.
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
    nsresult QueueRequest(nsCopyRequest* aRequest, PRBool *aCopyImmediately);

    nsVoidArray m_copyRequests;
};


#endif 
