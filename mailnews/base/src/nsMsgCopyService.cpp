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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsMsgCopyService.h"
#include "nsMsgKeyArray.h"
#include "nsCOMArray.h"
#include "nspr.h"
#include "nsIFileSpec.h"
#include "nsIMsgFolderNotificationService.h"
#include "nsMsgBaseCID.h"

// ******************** nsCopySource ******************
// 

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

nsCopyRequest::nsCopyRequest() :
    m_requestType(nsCopyMessagesType),
    m_isMoveOrDraftOrTemplate(PR_FALSE),
    m_processed(PR_FALSE),
    m_newMsgFlags(0)
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
      delete ncs;
  }
}

nsresult
nsCopyRequest::Init(nsCopyRequestType type, nsISupports* aSupport,
                    nsIMsgFolder* dstFolder,
                    PRBool bVal, PRUint32 newMsgFlags, nsIMsgCopyServiceListener* listener,
                    nsIMsgWindow* msgWindow, PRBool allowUndo)
{
  nsresult rv = NS_OK;
  m_requestType = type;
  m_srcSupport = aSupport;
  m_dstFolder = dstFolder;
  m_isMoveOrDraftOrTemplate = bVal;
  m_allowUndo = allowUndo;
  m_newMsgFlags = newMsgFlags;
  if (listener)
      m_listener = listener;
  if (msgWindow)
  {
    m_msgWindow = msgWindow;
    if (m_allowUndo)
			msgWindow->GetTransactionManager(getter_AddRefs(m_txnMgr));
	}
  if (type == nsCopyFoldersType)
  {
    // To support multiple copy folder operations to the same destination, we 
    // need to save the leaf name of the src file spec so that FindRequest() is
    // able to find the right request when copy finishes.
    nsCOMPtr<nsIMsgFolder> srcFolder = do_QueryInterface(aSupport, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsXPIDLString folderName;
    rv = srcFolder->GetName(getter_Copies(folderName));
    NS_ENSURE_SUCCESS(rv, rv);
    m_dstFolderName = folderName;
  }
  
  return rv;
}

nsCopySource*
nsCopyRequest::AddNewCopySource(nsIMsgFolder* srcFolder)
{
  nsCopySource* newSrc = new nsCopySource(srcFolder);
  if (newSrc)
  {
      m_copySourceArray.AppendElement((void*) newSrc);
      if (srcFolder == m_dstFolder)
        newSrc->m_processed = PR_TRUE;
  }
  return newSrc;
}

// ************* nsMsgCopyService ****************
// 


nsMsgCopyService::nsMsgCopyService()
{
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
    // Send notifications to nsIGlobalMsgFolderNotificationService
    
    if (aRequest->m_requestType == nsCopyFoldersType)
    {
      nsCOMPtr <nsIMsgFolderNotificationService> notifier = do_GetService(NS_MSGNOTIFICATIONSERVICE_CONTRACTID);
      if (notifier)
      {
        PRBool hasListeners;
        notifier->GetHasListeners(&hasListeners);
        if (hasListeners)
        {
          nsCOMPtr <nsISupportsArray> supportsArray = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
          if (supportsArray)
          {
            // Iterate over the copy sources and append their message arrays to this supports array
            // or in the case of folders, the source folder.
            PRInt32 cnt, i;
            cnt =  aRequest->m_copySourceArray.Count();
            for (i=0; i < cnt; i++)
            {
              nsCopySource *copySource = (nsCopySource*) aRequest->m_copySourceArray.ElementAt(i);
              supportsArray->AppendElement(copySource->m_msgFolder);
            }
            notifier->NotifyItemMoveCopyCompleted(aRequest->m_isMoveOrDraftOrTemplate, supportsArray, aRequest->m_dstFolder);
          }
        }
      }
    }
    
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
nsMsgCopyService::QueueRequest(nsCopyRequest* aRequest, PRBool *aCopyImmediately)
{
  NS_ENSURE_ARG_POINTER(aRequest);
  NS_ENSURE_ARG_POINTER(aCopyImmediately);
  *aCopyImmediately = PR_TRUE;
  nsCopyRequest* copyRequest;

  PRInt32 cnt, i;
  cnt = m_copyRequests.Count();
  for (i=0; i < cnt; i++)
  {
    copyRequest = (nsCopyRequest*) m_copyRequests.ElementAt(i);
    if (aRequest->m_requestType == nsCopyFoldersType)
    {
      // For copy folder, see if both destination folder (root)
      // (ie, Local Folder) and folder name (ie, abc) are the same.
      if (copyRequest->m_dstFolderName == aRequest->m_dstFolderName &&
          copyRequest->m_dstFolder.get() == aRequest->m_dstFolder.get())
      {
        *aCopyImmediately = PR_FALSE;
        break;
      }
    }
    else if (copyRequest->m_dstFolder.get() == aRequest->m_dstFolder.get())  //if dst are same and we already have a request, we cannot copy immediately
    {
      *aCopyImmediately = PR_FALSE;
      break;
    }
  }
  return NS_OK;
}
  
nsresult 
nsMsgCopyService::DoCopy(nsCopyRequest* aRequest)
{
  NS_ENSURE_ARG(aRequest);
  PRBool copyImmediately;
  QueueRequest(aRequest, &copyImmediately);
  m_copyRequests.AppendElement((void*) aRequest);
  if (copyImmediately) // if there wasn't another request for this dest folder then we can copy immediately
    return DoNextCopy();

  return NS_OK;
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
    nsCOMArray<nsIMsgFolder> activeTargets;

    // ** jt -- always FIFO
    for (i=0; i < cnt; i++)
    {
      copyRequest = (nsCopyRequest*) m_copyRequests.ElementAt(i);
      copySource = nsnull;
      scnt = copyRequest->m_copySourceArray.Count();
      if (!copyRequest->m_processed)
      {
        // if the target folder of this request already has an active 
        // copy request, skip this request for now.
        if (activeTargets.IndexOfObject(copyRequest->m_dstFolder) != kNotFound)
        {
          copyRequest = nsnull;
          continue;
        }
        if (scnt <= 0) goto found; // must be CopyFileMessage
        for (j=0; j < scnt; j++)
        {
          copySource = (nsCopySource*) copyRequest->m_copySourceArray.ElementAt(j);
          if (!copySource->m_processed)
            goto found;
        }
        if (j >= scnt) // all processed set the value
          copyRequest->m_processed = PR_TRUE;
      }
      else // keep track of folders actively getting copied to.
        activeTargets.AppendObject(copyRequest->m_dstFolder);
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
              // If it's a copy folder operation and the destination
              // folder already exists, CopyFolder() returns an error w/o sending
              // a completion notification, so clear it here.
              if (NS_FAILED(rv))
                ClearRequest(copyRequest, rv);

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
                    aMessage = do_QueryElementAt(copySource->m_messageArray,
                                                 0, &rv);
                    copySource->m_processed = PR_TRUE;
                }
                copyRequest->m_processed = PR_TRUE;
                rv = copyRequest->m_dstFolder->CopyFileMessage
                    (aSpec, aMessage,
                     copyRequest->m_isMoveOrDraftOrTemplate,
                     copyRequest->m_newMsgFlags,
                     copyRequest->m_msgWindow,
                     copyRequest->m_listener);
            }
          }
      }
    }
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
    if (copyRequest->m_requestType == nsCopyFoldersType)
    {
        // If the src is different then check next request. 
        if (copyRequest->m_srcSupport.get() != aSupport)
        {
          copyRequest = nsnull;
          continue;
        }

        // See if the parent of the copied folder is the same as the one when the request was made.
        // Note if the destination folder is already a server folder then no need to get parent.
        nsCOMPtr <nsIMsgFolder> parentMsgFolder;
        nsresult rv = NS_OK;
        PRBool isServer=PR_FALSE;
        dstFolder->GetIsServer(&isServer);
        if (!isServer)
          rv = dstFolder->GetParentMsgFolder(getter_AddRefs(parentMsgFolder));
        if ((NS_FAILED(rv)) || (!parentMsgFolder && !isServer) || (copyRequest->m_dstFolder.get() != parentMsgFolder))
        {
          copyRequest = nsnull;
          continue;
        }

        // Now checks if the folder name is the same.
        nsXPIDLString folderName;
        rv = dstFolder->GetName(getter_Copies(folderName));
        if (NS_FAILED(rv))
        {
          copyRequest = nsnull;
          continue;
        }

        if (copyRequest->m_dstFolderName == folderName)
          break;
    }
    else if (copyRequest->m_srcSupport.get() == aSupport &&
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
  NS_ENSURE_ARG_POINTER(srcFolder);
  NS_ENSURE_ARG_POINTER(messages);
  NS_ENSURE_ARG_POINTER(dstFolder);

  nsCopyRequest* copyRequest;
  nsCopySource* copySource = nsnull;
  nsCOMPtr<nsISupportsArray> msgArray;
  PRUint32 cnt;
  nsCOMPtr<nsIMsgDBHdr> msg;
  nsCOMPtr<nsIMsgFolder> curFolder;
  nsCOMPtr<nsISupports> aSupport;
  nsresult rv;
    
  // XXX TODO 
  // JUNK MAIL RELATED
  // make sure dest folder exists
  // and has proper flags, before we start copying?

  copyRequest = new nsCopyRequest();
  if (!copyRequest) 
    return NS_ERROR_OUT_OF_MEMORY;

  aSupport = do_QueryInterface(srcFolder, &rv);

  rv = copyRequest->Init(nsCopyMessagesType, aSupport, dstFolder, isMove, 
                        0 /* new msg flags, not used */, listener, 
                         window, allowUndo);
  if (NS_FAILED(rv)) 
    goto done;

  rv = NS_NewISupportsArray(getter_AddRefs(msgArray));
  if (NS_FAILED(rv)) 
    goto done;

  messages->Count(&cnt);

  // duplicate the message array so we could sort the messages by it's
  // folder easily
  msgArray->AppendElements(messages);

  rv = msgArray->Count(&cnt);
  if (NS_FAILED(rv)) 
    goto done;

  while (cnt-- > 0)
  {
    msg = do_QueryElementAt(msgArray, cnt, &rv);

    if (NS_FAILED(rv)) 
      goto done;

    rv = msg->GetFolder(getter_AddRefs(curFolder));

    if (NS_FAILED(rv)) 
      goto done;
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
    isMove, 0 /* new msg flags, not used */ , listener, window, PR_FALSE);
  NS_ENSURE_SUCCESS(rv,rv);
  
  curFolder = do_QueryInterface(support, &rv);
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
                                  PRUint32 aMsgFlags,
                                  nsIMsgCopyServiceListener* listener,
                                  nsIMsgWindow* window)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  nsCopyRequest* copyRequest;
  nsCopySource* copySource = nsnull;
  nsCOMPtr<nsISupports> fileSupport;
  nsCOMPtr<nsITransactionManager> txnMgr;

  NS_ENSURE_ARG_POINTER(fileSpec);
  NS_ENSURE_ARG_POINTER(dstFolder);

  if (window)
    window->GetTransactionManager(getter_AddRefs(txnMgr));
  copyRequest = new nsCopyRequest();
  if (!copyRequest) return rv;
  fileSupport = do_QueryInterface(fileSpec, &rv);
  if (NS_FAILED(rv)) goto done;

  rv = copyRequest->Init(nsCopyFileMessageType, fileSupport, dstFolder,
                         isDraft, aMsgFlags, listener, window, PR_FALSE);
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
  nsCopyRequest* copyRequest = nsnull;
  do
  {
    // loop for copy requests, because if we do a cross server folder copy,
    // we'll have a copy request for the folder copy, which will in turn
    // generate a copy request for the messages in the folder, which
    // will have the same src support.
    copyRequest = FindRequest(aSupport, dstFolder);

    if (copyRequest)
    {
      // check if this copy request is done by making sure all the
      // sources have been processed.
      nsCopySource* copySource = nsnull;
      PRInt32 sourceIndex, sourceCount;
      sourceCount = copyRequest->m_copySourceArray.Count();
      for (sourceIndex = 0; sourceIndex < sourceCount; sourceIndex++)
      {
        if (!((nsCopySource*)
            copyRequest->m_copySourceArray.ElementAt(sourceIndex))->m_processed)
            break;
      }
      // if all sources processed, mark the request as processed
      if (sourceIndex >= sourceCount) 
        copyRequest->m_processed = PR_TRUE;
    // if this request is done, or failed, clear it.
      if (copyRequest->m_processed || NS_FAILED(result))
        ClearRequest(copyRequest, result);
      else 
        break;
    }
    else
      break;
  }
  while (copyRequest);

  return DoNextCopy();
}

