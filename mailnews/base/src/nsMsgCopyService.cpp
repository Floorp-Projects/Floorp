/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998, 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgCopyService.h"
#include "nsVoidArray.h"
#include "nsMsgKeyArray.h"
#include "nspr.h"
#include "nsIFileSpec.h"

// ******************** nsCopySource ******************
// 

MOZ_DECL_CTOR_COUNTER(nsCopySource)

nsCopySource::nsCopySource() : m_processed(PR_FALSE)
{
	MOZ_COUNT_CTOR(nsCopySource);
  nsresult rv;
  rv = NS_NewISupportsArray(getter_AddRefs(m_messageArray));
}

nsCopySource::nsCopySource(nsIMsgFolder* srcFolder) :
    m_processed(PR_FALSE)
{
	MOZ_COUNT_CTOR(nsCopySource);
  nsresult rv;
  rv = NS_NewISupportsArray(getter_AddRefs(m_messageArray));
  m_msgFolder = do_QueryInterface(srcFolder, &rv);
}

nsCopySource::~nsCopySource()
{
	MOZ_COUNT_DTOR(nsCopySource);
}

void nsCopySource::AddMessage(nsIMsgDBHdr* aMsg)
{
	nsCOMPtr<nsISupports> supports(do_QueryInterface(aMsg));
	if(supports)
		m_messageArray->AppendElement(supports);
}

// ************ nsCopyRequest *****************
// 

MOZ_DECL_CTOR_COUNTER(nsCopyRequest)

nsCopyRequest::nsCopyRequest() :
    m_requestType(nsCopyMessagesType),
    m_isMoveOrDraftOrTemplate(PR_FALSE),
    m_processed(PR_FALSE)
{
	MOZ_COUNT_CTOR(nsCopyRequest);
}

nsCopyRequest::~nsCopyRequest()
{
 	MOZ_COUNT_DTOR(nsCopyRequest);

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
                    nsIMsgWindow* msgWindow, PRBool allowUndo)
{
  nsresult rv = NS_OK;
  m_requestType = type;
  m_srcSupport = aSupport;
  m_dstFolder = dstFolder;
  m_isMoveOrDraftOrTemplate = bVal;
  m_allowUndo = allowUndo;
  if (listener)
      m_listener = listener;
  if (msgWindow)
	{
    m_msgWindow = msgWindow;
    if (m_allowUndo)
		msgWindow->GetTransactionManager(getter_AddRefs(m_txnMgr));
	}
  
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
  if (aRequest)
  {
    // undo stuff
    if (aRequest->m_allowUndo && aRequest->m_copySourceArray.Count() > 1 && 
        aRequest->m_txnMgr)
        aRequest->m_txnMgr->EndBatch();
        
    m_copyRequests.RemoveElement(aRequest);
    if (aRequest->m_listener)
        aRequest->m_listener->OnStopCopy(rv);
    delete aRequest;
  }
  
  return rv;
}

nsresult 
nsMsgCopyService::DoCopy(nsCopyRequest* aRequest)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (aRequest)
  {
      m_copyRequests.AppendElement((void*) aRequest);
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
                   copyRequest->m_msgWindow, copyRequest->m_listener, PR_FALSE, copyRequest->m_allowUndo);   //isFolder operation PR_FALSE
                                                              
          }
          else if (copyRequest->m_requestType == nsCopyFoldersType )
		  {
			  copySource->m_processed = PR_TRUE;
              rv = copyRequest->m_dstFolder->CopyFolder
                  (copySource->m_msgFolder,
                   copyRequest->m_isMoveOrDraftOrTemplate,
                   copyRequest->m_msgWindow, copyRequest->m_listener);
		  }
          else if (copyRequest->m_requestType == nsCopyFileMessageType)
          {
            nsCOMPtr<nsIFileSpec> aSpec(do_QueryInterface(copyRequest->m_srcSupport, &rv));
            if (NS_SUCCEEDED(rv))
            {
                // ** in case of saving draft/template; the very first
                // time we may not have the original message to replace
                // with; if we do we shall have an instance of copySource
                nsCOMPtr<nsIMsgDBHdr> aMessage;
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
                     copyRequest->m_msgWindow,
                     copyRequest->m_listener);
            }
          }
      }
    }
    // Don't clear copy request in failure case - notify completion should do that.
    return rv;
}

nsCopyRequest*
nsMsgCopyService::FindRequest(nsISupports* aSupport,
                              nsIMsgFolder* dstFolder)
{
  nsCopyRequest* copyRequest = nsnull;
  PRInt32 cnt, i;

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

  return copyRequest;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMsgCopyService, nsIMsgCopyService)

NS_IMETHODIMP
nsMsgCopyService::CopyMessages(nsIMsgFolder* srcFolder, /* UI src folder */
                               nsISupportsArray* messages,
                               nsIMsgFolder* dstFolder,
                               PRBool isMove,
                               nsIMsgCopyServiceListener* listener,
                               nsIMsgWindow* window,
                               PRBool allowUndo)
{
    nsCopyRequest* copyRequest;
    nsCopySource* copySource = nsnull;
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsISupportsArray> msgArray;
    PRUint32 i, cnt;
    nsCOMPtr<nsIMsgDBHdr> msg;
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsCOMPtr<nsISupports> aSupport;

    if (!srcFolder || !messages || !dstFolder) return rv;

    copyRequest = new nsCopyRequest();
    if (!copyRequest) return rv;
    aSupport = do_QueryInterface(srcFolder, &rv);

    rv = copyRequest->Init(nsCopyMessagesType, aSupport, dstFolder, 
                           isMove, listener, window, allowUndo);
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewISupportsArray(getter_AddRefs(msgArray));
    if (NS_FAILED(rv)) goto done;

    messages->Count(&cnt);
    copySource = copyRequest->AddNewCopySource(srcFolder);

    for (i=0; i<cnt; i++)
    {
      aSupport = getter_AddRefs(messages->ElementAt(i));
      msg = do_QueryInterface(aSupport, &rv);
      copySource->AddMessage(msg);
    }
    // undo stuff
    if (NS_SUCCEEDED(rv) && copyRequest->m_allowUndo && copyRequest->m_copySourceArray.Count() > 1 &&
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
nsMsgCopyService::CopyFolders( nsISupportsArray* folders,
                               nsIMsgFolder* dstFolder,
                               PRBool isMove,
                               nsIMsgCopyServiceListener* listener,
                               nsIMsgWindow* window)
{
    nsCopyRequest* copyRequest;
    nsCopySource* copySource = nsnull;
    nsresult rv = NS_ERROR_NULL_POINTER;
    PRUint32 cnt;
    nsCOMPtr<nsIFolder> folder;
    nsCOMPtr<nsIMsgFolder> curFolder;
    nsCOMPtr<nsISupports> support;

    if (!folders || !dstFolder) return rv;

	rv = folders->Count(&cnt);   //if cnt is zero it cannot to get this point, will be detected earlier
	if ( cnt > 1)
	     NS_ASSERTION((NS_SUCCEEDED(rv)),"More than one folders to copy");
	
    support = getter_AddRefs(folders->ElementAt(0));

    copyRequest = new nsCopyRequest();
 	if (!copyRequest) return NS_ERROR_OUT_OF_MEMORY;
       
    rv = copyRequest->Init(nsCopyFoldersType, support, dstFolder, 
                           isMove, listener, window, PR_FALSE);
    NS_ENSURE_SUCCESS(rv,rv);

    folder = do_QueryInterface(support, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    curFolder = do_QueryInterface(folder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
        
    copySource = copyRequest->AddNewCopySource(curFolder);
    if (!copySource)
        rv = NS_ERROR_OUT_OF_MEMORY;
	    
    if (NS_FAILED(rv))
	{
        delete copyRequest;
		NS_ENSURE_SUCCESS(rv, rv);
	}
    else
        rv = DoCopy(copyRequest);
    
   return rv;
}

NS_IMETHODIMP
nsMsgCopyService::CopyFileMessage(nsIFileSpec* fileSpec,
                                  nsIMsgFolder* dstFolder,
                                  nsIMsgDBHdr* msgToReplace,
                                  PRBool isDraft,
                                  nsIMsgCopyServiceListener* listener,
	                               nsIMsgWindow* window)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    nsCopyRequest* copyRequest;
    nsCopySource* copySource = nsnull;
    nsCOMPtr<nsISupports> aSupport;
	  nsCOMPtr<nsITransactionManager> txnMgr;

    if (!fileSpec || !dstFolder) return rv;

	if (window)
		window->GetTransactionManager(getter_AddRefs(txnMgr));
  copyRequest = new nsCopyRequest();
  if (!copyRequest) return rv;
  aSupport = do_QueryInterface(fileSpec, &rv);
  if (NS_FAILED(rv)) goto done;

  rv = copyRequest->Init(nsCopyFileMessageType, aSupport, dstFolder,
                         isDraft, listener, window, PR_FALSE);
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
  {
    ClearRequest(copyRequest, result);
  }

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
