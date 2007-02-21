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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Karsten Düsterloh <mnyromyr@tprac.de>
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

// this file implements the nsMsgFilterService interface 

#include "msgCore.h"
#include "nsMsgFilterService.h"
#include "nsFileStream.h"
#include "nsMsgFilterList.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIPrompt.h"
#include "nsIDocShell.h"
#include "nsIMsgWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringBundle.h"
#include "nsIMsgSearchNotify.h"
#include "nsIUrlListener.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
#include "nsIDBFolderInfo.h"
#include "nsIRDFService.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgCopyService.h"
#include "nsIOutputStream.h"
#include "nsIMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsMsgUtils.h"

NS_IMPL_ISUPPORTS1(nsMsgFilterService, nsIMsgFilterService)

nsMsgFilterService::nsMsgFilterService()
{
}

nsMsgFilterService::~nsMsgFilterService()
{
}

NS_IMETHODIMP nsMsgFilterService::OpenFilterList(nsIFileSpec *filterFile, nsIMsgFolder *rootFolder, nsIMsgWindow *aMsgWindow, nsIMsgFilterList **resultFilterList)
{
	nsresult ret = NS_OK;

    nsFileSpec filterSpec;
    filterFile->GetFileSpec(&filterSpec);
	nsIOFileStream *fileStream = new nsIOFileStream(filterSpec);
	if (!fileStream)
		return NS_ERROR_OUT_OF_MEMORY;

	nsMsgFilterList *filterList = new nsMsgFilterList();
	if (!filterList)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(filterList);
    filterList->SetFolder(rootFolder);
    
    // temporarily tell the filter where it's file path is
    filterList->SetDefaultFile(filterFile);
    
    PRUint32 size;
    ret = filterFile->GetFileSize(&size);
	if (NS_SUCCEEDED(ret) && size > 0)
		ret = filterList->LoadTextFilters(fileStream);
  fileStream->close();
  delete fileStream;
  fileStream =nsnull;
	if (NS_SUCCEEDED(ret))
  {
		*resultFilterList = filterList;
        PRInt16 version;
        filterList->GetVersion(&version);
    if (version != kFileVersion)
    {

      SaveFilterList(filterList, filterFile);
    }
  }
	else
  {
    NS_RELEASE(filterList);
    if (ret == NS_MSG_FILTER_PARSE_ERROR && aMsgWindow)
    {
      ret = BackUpFilterFile(filterFile, aMsgWindow);
      NS_ENSURE_SUCCESS(ret, ret);
      ret = filterFile->Truncate(0);
      NS_ENSURE_SUCCESS(ret, ret);
      return OpenFilterList(filterFile, rootFolder, aMsgWindow, resultFilterList);
    }
    else if(ret == NS_MSG_CUSTOM_HEADERS_OVERFLOW && aMsgWindow)
      ThrowAlertMsg("filterCustomHeaderOverflow", aMsgWindow);
    else if(ret == NS_MSG_INVALID_CUSTOM_HEADER && aMsgWindow)
      ThrowAlertMsg("invalidCustomHeader", aMsgWindow);
  }
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CloseFilterList(nsIMsgFilterList *filterList)
{
	//NS_ASSERTION(PR_FALSE,"CloseFilterList doesn't do anything yet");
	return NS_OK;
}

/* save without deleting */
NS_IMETHODIMP	nsMsgFilterService::SaveFilterList(nsIMsgFilterList *filterList, nsIFileSpec *filterFile)
{
  NS_ENSURE_ARG_POINTER(filterFile);
  NS_ENSURE_ARG_POINTER(filterList);

  nsresult ret = NS_OK;
  nsCOMPtr <nsIFileSpec> tmpFiltersFile;
  nsCOMPtr <nsIFileSpec> realFiltersFile;
  nsCOMPtr <nsIFileSpec> parentDir;

  ret = GetSpecialDirectoryWithFileName(NS_OS_TEMP_DIR,
                                        "tmprules.dat",
                                        getter_AddRefs(tmpFiltersFile));

  NS_ASSERTION(NS_SUCCEEDED(ret),"writing filters file: failed to append filename");
  if (NS_FAILED(ret)) 
    return ret;

  ret = tmpFiltersFile->MakeUnique();  //need a unique tmp file to prevent dataloss in multiuser environment
  NS_ENSURE_SUCCESS(ret, ret);

  nsFileSpec tmpFileSpec;
  tmpFiltersFile->GetFileSpec(&tmpFileSpec);

	nsIOFileStream *tmpFileStream = nsnull;
  
  if (NS_SUCCEEDED(ret))
    ret = filterFile->GetParent(getter_AddRefs(parentDir));

  if (NS_SUCCEEDED(ret))
    tmpFileStream = new nsIOFileStream(tmpFileSpec);
	if (!tmpFileStream)
		return NS_ERROR_OUT_OF_MEMORY;
  ret = filterList->SaveToFile(tmpFileStream);
  tmpFileStream->close();
  delete tmpFileStream;
  tmpFileStream = nsnull;
  
  if (NS_SUCCEEDED(ret))
  {
    // can't move across drives
    ret = tmpFiltersFile->CopyToDir(parentDir);
    if (NS_SUCCEEDED(ret))
    {
      filterFile->Delete(PR_FALSE);
      nsXPIDLCString tmpFileName;
      tmpFiltersFile->GetLeafName(getter_Copies(tmpFileName));
      parentDir->AppendRelativeUnixPath(tmpFileName.get());
      nsXPIDLCString finalLeafName;
      filterFile->GetLeafName(getter_Copies(finalLeafName));
      if (!finalLeafName.IsEmpty())
        parentDir->Rename(finalLeafName);
      else // fall back to msgFilterRules.dat
        parentDir->Rename("msgFilterRules.dat");

      tmpFiltersFile->Delete(PR_FALSE);
    }

  }
  NS_ASSERTION(NS_SUCCEEDED(ret), "error opening/saving filter list");
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CancelFilterList(nsIMsgFilterList *filterList)
{ 
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgFilterService::BackUpFilterFile(nsIFileSpec *aFilterFile, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  AlertBackingUpFilterFile(aMsgWindow);
  aFilterFile->CloseStream();

  nsCOMPtr<nsILocalFile> localFilterFile;
  nsFileSpec filterFileSpec;
  aFilterFile->GetFileSpec(&filterFileSpec);
  rv = NS_FileSpecToIFile(&filterFileSpec, getter_AddRefs(localFilterFile));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsILocalFile> localParentDir;
  nsCOMPtr <nsIFileSpec> parentDir;
  rv = aFilterFile->GetParent(getter_AddRefs(parentDir));
  NS_ENSURE_SUCCESS(rv,rv);

  nsFileSpec parentDirSpec;
  parentDir->GetFileSpec(&parentDirSpec);

  rv = NS_FileSpecToIFile(&parentDirSpec, getter_AddRefs(localParentDir));
  NS_ENSURE_SUCCESS(rv,rv);

  //if back-up file exists delete the back up file otherwise copy fails. 
  nsCOMPtr <nsILocalFile> backupFile;
  rv = NS_FileSpecToIFile(&parentDirSpec, getter_AddRefs(backupFile));
  NS_ENSURE_SUCCESS(rv,rv);
  backupFile->AppendNative(NS_LITERAL_CSTRING("rulesbackup.dat"));
  PRBool exists;
  backupFile->Exists(&exists);
  if (exists)
    backupFile->Remove(PR_FALSE);

  return localFilterFile->CopyToNative(localParentDir, NS_LITERAL_CSTRING("rulesbackup.dat"));
}

nsresult nsMsgFilterService::AlertBackingUpFilterFile(nsIMsgWindow *aMsgWindow)
{
  return ThrowAlertMsg("filterListBackUpMsg", aMsgWindow);
}

nsresult //Do not use this routine if you have to call it very often because it creates a new bundle each time
nsMsgFilterService::GetStringFromBundle(const char *aMsgName, PRUnichar **aResult)
{ 
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aResult);
  nsCOMPtr <nsIStringBundle> bundle;
  rv = GetFilterStringBundle(getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
    rv=bundle->GetStringFromName(NS_ConvertASCIItoUTF16(aMsgName).get(), aResult);
  return rv;
  
}

nsresult
nsMsgFilterService::GetFilterStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv=NS_OK;
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService =
         do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/filter.properties",
                                 getter_AddRefs(bundle));
  NS_IF_ADDREF(*aBundle = bundle);
  return rv;
}

nsresult
nsMsgFilterService::ThrowAlertMsg(const char*aMsgName, nsIMsgWindow *aMsgWindow)
{
  nsXPIDLString alertString;
  nsresult rv = GetStringFromBundle(aMsgName, getter_Copies(alertString));
  if (NS_SUCCEEDED(rv) && alertString && aMsgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    aMsgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && alertString)
        dialog->Alert(nsnull, alertString);
    }
  }
  return rv;
}

// this class is used to run filters after the fact, i.e., after new mail has been downloaded from the server.
// It can do the following:
// 1. Apply a single imap or pop3 filter on a single folder.
// 2. Apply multiple filters on a single imap or pop3 folder.
// 3. Apply a single filter on multiple imap or pop3 folders in the same account.
// 4. Apply multiple filters on multiple imap or pop3 folders in the same account.
// This will be called from the front end js code in the case of the apply filters to folder menu code, 
// and from the filter dialog js code with the run filter now command.


// this class holds the list of filters and folders, and applies them in turn, first iterating
// over all the filters on one folder, and then advancing to the next folder and repeating. 
// For each filter,we take the filter criteria and create a search term list. Then, we execute the search.
// We are a search listener so that we can build up the list of search hits. 
// Then, when the search is done, we will apply the filter action(s) en-masse, so, for example, if the action is a move, 
// we calls one method to move all the messages to the destination folder. Or, mark all the messages read.
// In the case of imap operations, or imap/local  moves, the action will be asynchronous, so we'll need to be a url listener 
// as well, and kick off the next filter when the action completes.
class nsMsgFilterAfterTheFact : public nsIUrlListener, public nsIMsgSearchNotify, public nsIMsgCopyServiceListener
{
public:
  nsMsgFilterAfterTheFact(nsIMsgWindow *aMsgWindow, nsIMsgFilterList *aFilterList, nsISupportsArray *aFolderList);
  virtual ~nsMsgFilterAfterTheFact();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLLISTENER
  NS_DECL_NSIMSGSEARCHNOTIFY
  NS_DECL_NSIMSGCOPYSERVICELISTENER

  nsresult  AdvanceToNextFolder();  // kicks off the process
protected:
  nsresult  RunNextFilter();
  nsresult  ApplyFilter();
  nsresult  OnEndExecution(nsresult executionStatus); // do what we have to do to cleanup.
  PRBool    ContinueExecutionPrompt();
  nsresult  DisplayConfirmationPrompt(nsIMsgWindow *msgWindow, const PRUnichar *confirmString, PRBool *confirmed);
  nsCOMPtr <nsIMsgWindow>     m_msgWindow;
  nsCOMPtr <nsIMsgFilterList> m_filters;
  nsCOMPtr <nsISupportsArray> m_folders;
  nsCOMPtr <nsIMsgFolder>     m_curFolder;
  nsCOMPtr <nsIMsgDatabase>   m_curFolderDB;
  nsCOMPtr <nsIMsgFilter>     m_curFilter;
  PRUint32                    m_curFilterIndex;
  PRUint32                    m_curFolderIndex;
  PRUint32                    m_numFilters;
  PRUint32                    m_numFolders;
  nsMsgKeyArray               m_searchHits;
  nsCOMPtr <nsISupportsArray> m_searchHitHdrs;
  nsCOMPtr <nsIMsgSearchSession> m_searchSession;
};

NS_IMPL_ISUPPORTS3(nsMsgFilterAfterTheFact, nsIUrlListener, nsIMsgSearchNotify, nsIMsgCopyServiceListener)

nsMsgFilterAfterTheFact::nsMsgFilterAfterTheFact(nsIMsgWindow *aMsgWindow, nsIMsgFilterList *aFilterList, nsISupportsArray *aFolderList)
{
  m_curFilterIndex = m_curFolderIndex = 0;
  m_msgWindow = aMsgWindow;
  m_filters = aFilterList;
  m_folders = aFolderList;
  m_filters->GetFilterCount(&m_numFilters);
  m_folders->Count(&m_numFolders);

  NS_ADDREF(this); // we own ourselves, and will release ourselves when execution is done.

  NS_NewISupportsArray(getter_AddRefs(m_searchHitHdrs));
}

nsMsgFilterAfterTheFact::~nsMsgFilterAfterTheFact()
{
}

// do what we have to do to cleanup.
nsresult nsMsgFilterAfterTheFact::OnEndExecution(nsresult executionStatus)
{
  if (m_searchSession)
    m_searchSession->UnregisterListener(this);

  if (m_filters)
    (void)m_filters->FlushLogIfNecessary();

  Release(); // release ourselves.
  return executionStatus;
}

nsresult nsMsgFilterAfterTheFact::RunNextFilter()
{
  nsresult rv;
  if (m_curFilterIndex >= m_numFilters)
    return AdvanceToNextFolder();

  rv = m_filters->GetFilterAt(m_curFilterIndex++, getter_AddRefs(m_curFilter));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsISupportsArray> searchTerms;
  rv = m_curFilter->GetSearchTerms(getter_AddRefs(searchTerms));
  NS_ENSURE_SUCCESS(rv, rv);
  if (m_searchSession)
    m_searchSession->UnregisterListener(this);
  m_searchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMsgSearchScopeValue searchScope = nsMsgSearchScope::offlineMail;
  PRUint32 termCount;
  searchTerms->Count(&termCount);
  for (PRUint32 termIndex = 0; termIndex < termCount; termIndex++)
  {
    nsCOMPtr <nsIMsgSearchTerm> term;
    rv = searchTerms->QueryElementAt(termIndex, NS_GET_IID(nsIMsgSearchTerm), getter_AddRefs(term));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = m_searchSession->AppendTerm(term);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  m_searchSession->RegisterListener(this);

  rv = m_searchSession->AddScopeTerm(searchScope, m_curFolder);
  NS_ENSURE_SUCCESS(rv, rv);
  // it's possible that this error handling will need to be rearranged when mscott lands the UI for
  // doing filters based on sender in PAB, because we can't do that for IMAP. I believe appending the
  // search term will fail, or the Search itself will fail synchronously. In that case, we'll
  // have to ignore the filter, I believe. Ultimately, we'd like to re-work the search backend
  // so that it can do this.
  return m_searchSession->Search(m_msgWindow);
}

nsresult nsMsgFilterAfterTheFact::AdvanceToNextFolder()
{
  if (m_curFolderIndex >= m_numFolders)
    return OnEndExecution(NS_OK);

  nsresult rv = m_folders->QueryElementAt(m_curFolderIndex++, NS_GET_IID(nsIMsgFolder), getter_AddRefs(m_curFolder));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  rv = m_curFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(m_curFolderDB));
  if (rv == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING || rv == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
  {
    nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(m_curFolder, &rv);
    if (NS_SUCCEEDED(rv) && localFolder)
      return localFolder->ParseFolder(m_msgWindow, this);
  }
  return RunNextFilter();
}

NS_IMETHODIMP nsMsgFilterAfterTheFact::OnStartRunningUrl(nsIURI *aUrl)
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterAfterTheFact::OnStopRunningUrl(nsIURI *aUrl, nsresult aExitCode)
{
  PRBool continueExecution = NS_SUCCEEDED(aExitCode);
  if (!continueExecution)
    continueExecution = ContinueExecutionPrompt();

  return (continueExecution) ? RunNextFilter() : OnEndExecution(aExitCode);
}

NS_IMETHODIMP nsMsgFilterAfterTheFact::OnSearchHit(nsIMsgDBHdr *header, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG_POINTER(header);
  nsMsgKey msgKey;
  header->GetMessageKey(&msgKey);
  m_searchHits.Add(msgKey);
  m_searchHitHdrs->AppendElement(header);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterAfterTheFact::OnSearchDone(nsresult status)
{
  nsresult rv = status;
  PRBool continueExecution = NS_SUCCEEDED(status);
  if (!continueExecution)
    continueExecution = ContinueExecutionPrompt();

  if (continueExecution)
    return (m_searchHits.GetSize() > 0) ? ApplyFilter() : RunNextFilter();
  else
    return OnEndExecution(rv);
}

NS_IMETHODIMP nsMsgFilterAfterTheFact::OnNewSearch()
{
  m_searchHits.RemoveAll();
  m_searchHitHdrs->Clear();
  return NS_OK;
}

nsresult nsMsgFilterAfterTheFact::ApplyFilter()
{
  nsresult rv = NS_OK;
  PRBool applyMoreActions = PR_TRUE;
  if (m_curFilter && m_curFolder)
  {
    // we're going to log the filter actions before firing them because some actions are async
    PRBool loggingEnabled = PR_FALSE;
    if (m_filters)
      (void)m_filters->GetLoggingEnabled(&loggingEnabled);

    nsCOMPtr<nsISupportsArray> actionList;
    rv = NS_NewISupportsArray(getter_AddRefs(actionList));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = m_curFilter->GetSortedActionList(actionList);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 numActions;
    actionList->Count(&numActions);

    for (PRUint32 actionIndex =0; actionIndex < numActions && applyMoreActions; actionIndex++)
    {
      nsCOMPtr<nsIMsgRuleAction> filterAction;
      actionList->QueryElementAt(actionIndex, NS_GET_IID(nsIMsgRuleAction), (void **)getter_AddRefs(filterAction));
      nsMsgRuleActionType actionType;
      if (filterAction)
        filterAction->GetType(&actionType);
      else
        continue;
      
      nsXPIDLCString actionTargetFolderUri;
      if (actionType == nsMsgFilterAction::MoveToFolder ||
          actionType == nsMsgFilterAction::CopyToFolder)
      {
        filterAction->GetTargetFolderUri(getter_Copies(actionTargetFolderUri));
        if (actionTargetFolderUri.IsEmpty())
        {
          NS_ASSERTION(PR_FALSE, "actionTargetFolderUri is empty");
          continue;
        }
      }

      if (loggingEnabled) 
      {
          for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
          {
            nsCOMPtr <nsIMsgDBHdr> msgHdr;
            m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
            if (msgHdr)
              (void)m_curFilter->LogRuleHit(filterAction, msgHdr); 
          }
      }
      // all actions that pass "this" as a listener in order to chain filter execution
      // when the action is finished need to return before reaching the bottom of this
      // routine, because we run the next filter at the end of this routine.
      switch (actionType)
      {
      case nsMsgFilterAction::Delete:
        // we can't pass ourselves in as a copy service listener because the copy service
        // listener won't get called in several situations (e.g., the delete model is imap delete)
        // and we rely on the listener getting called to continue the filter application.
        // This means we're going to end up firing off the delete, and then subsequently 
        // issuing a search for the next filter, which will block until the delete finishes.
        m_curFolder->DeleteMessages(m_searchHitHdrs, m_msgWindow, PR_FALSE, PR_FALSE, nsnull, PR_FALSE /*allow Undo*/ );
        //if we are deleting then we couldn't care less about applying remaining filter actions
        applyMoreActions = PR_FALSE;
        break;
      case nsMsgFilterAction::MoveToFolder:
      case nsMsgFilterAction::CopyToFolder:
      {
        // if moving or copying to a different file, do it.
        nsXPIDLCString uri;
        rv = m_curFolder->GetURI(getter_Copies(uri));

        if ((const char*)actionTargetFolderUri &&
            nsCRT::strcmp(uri, actionTargetFolderUri))
        {
          nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
          nsCOMPtr<nsIRDFResource> res;
          rv = rdf->GetResource(actionTargetFolderUri, getter_AddRefs(res));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIMsgFolder> destIFolder(do_QueryInterface(res, &rv));
          NS_ENSURE_SUCCESS(rv, rv);

          PRBool canFileMessages = PR_TRUE;
          nsCOMPtr<nsIMsgFolder> parentFolder;
          destIFolder->GetParent(getter_AddRefs(parentFolder));
          if (parentFolder)
            destIFolder->GetCanFileMessages(&canFileMessages);
          if (!parentFolder || !canFileMessages)
          {
            m_curFilter->SetEnabled(PR_FALSE);
            destIFolder->ThrowAlertMsg("filterDisabled",m_msgWindow);
            // we need to explicitly save the filter file.
            m_filters->SaveToDefaultFile();
            // In the case of applying multiple filters
            // we might want to remove the filter from the list, but 
            // that's a bit evil since we really don't know that we own
            // the list. Disabling it doesn't do a lot of good since
            // we still apply disabled filters. Currently, we don't
            // have any clients that apply filters to multiple folders,
            // so this might be the edge case of an edge case.
            return RunNextFilter();
          }
          nsCOMPtr<nsIMsgCopyService> copyService = do_GetService(NS_MSGCOPYSERVICE_CONTRACTID, &rv);
          if (copyService)
            return copyService->CopyMessages(m_curFolder, m_searchHitHdrs, destIFolder, actionType == nsMsgFilterAction::MoveToFolder, this, m_msgWindow, PR_FALSE);
        }
        //we have already moved the hdrs so we can't apply more actions
        if (actionType == nsMsgFilterAction::MoveToFolder)
          applyMoreActions = PR_FALSE;
      }
        
        break;
      case nsMsgFilterAction::MarkRead:
          // crud, no listener support here - we'll probably just need to go on and apply
          // the next filter, and, in the imap case, rely on multiple connection and url
          // queueing to stay out of trouble
          m_curFolder->MarkMessagesRead(m_searchHitHdrs, PR_TRUE);
        break;
      case nsMsgFilterAction::MarkFlagged:
        m_curFolder->MarkMessagesFlagged(m_searchHitHdrs, PR_TRUE);
        break;
      case nsMsgFilterAction::KillThread:
      case nsMsgFilterAction::WatchThread:
        {
          for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
          {
            nsCOMPtr <nsIMsgDBHdr> msgHdr;
            m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
            if (msgHdr)
            {
              nsCOMPtr <nsIMsgThread> msgThread;
              nsMsgKey threadKey;
              m_curFolderDB->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(msgThread));
              if (msgThread)
              {
                msgThread->GetThreadKey(&threadKey);
                if (actionType == nsMsgFilterAction::KillThread)
                  m_curFolderDB->MarkThreadIgnored(msgThread, threadKey, PR_TRUE, nsnull);
                else
                  m_curFolderDB->MarkThreadWatched(msgThread, threadKey, PR_TRUE, nsnull);
              }
            }
          }
        }
        break;
      case nsMsgFilterAction::ChangePriority:
          {
              nsMsgPriorityValue filterPriority;
              filterAction->GetPriority(&filterPriority);
              for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
              {
                nsCOMPtr <nsIMsgDBHdr> msgHdr;
                m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
                if (msgHdr)
                  msgHdr->SetPriority(filterPriority);
              }
          }
        break;
      case nsMsgFilterAction::Label:
        {
            nsMsgLabelValue filterLabel;
            filterAction->GetLabel(&filterLabel);
            m_curFolder->SetLabelForMessages(m_searchHitHdrs, filterLabel);
        }
        break;
      case nsMsgFilterAction::AddTag:
        {
            nsXPIDLCString keyword;
            filterAction->GetStrValue(getter_Copies(keyword));
            m_curFolder->AddKeywordToMessages(m_searchHitHdrs, keyword.get());
        }
        break;
      case nsMsgFilterAction::JunkScore:
      {
        nsCAutoString junkScoreStr;
        PRInt32 junkScore;
        filterAction->GetJunkScore(&junkScore);
        junkScoreStr.AppendInt(junkScore);
        m_curFolder->SetJunkScoreForMessages(m_searchHitHdrs, junkScoreStr.get());
        break;
      }
      case nsMsgFilterAction::Forward:
        {
          nsXPIDLCString forwardTo;
          filterAction->GetStrValue(getter_Copies(forwardTo));
          nsCOMPtr <nsIMsgIncomingServer> server;
          rv = m_curFolder->GetServer(getter_AddRefs(server));
          NS_ENSURE_SUCCESS(rv, rv);
          if (!forwardTo.IsEmpty())
          {
            nsCOMPtr <nsIMsgComposeService> compService = do_GetService (NS_MSGCOMPOSESERVICE_CONTRACTID) ;
            if (compService)
            {
              for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
              {
                nsCOMPtr <nsIMsgDBHdr> msgHdr;
                m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
                if (msgHdr)
                {
                  nsAutoString forwardStr;
                  forwardStr.AssignWithConversion(forwardTo.get());
                  rv = compService->ForwardMessage(forwardStr, msgHdr, m_msgWindow, server);
                }
              }
            }
          }
        }
        break;
      case nsMsgFilterAction::Reply:
        {
          nsXPIDLCString replyTemplateUri;
          filterAction->GetStrValue(getter_Copies(replyTemplateUri));
          nsCOMPtr <nsIMsgIncomingServer> server;
          rv = m_curFolder->GetServer(getter_AddRefs(server));
          NS_ENSURE_SUCCESS(rv, rv);
          if (!replyTemplateUri.IsEmpty())
          {
            nsCOMPtr <nsIMsgComposeService> compService = do_GetService (NS_MSGCOMPOSESERVICE_CONTRACTID) ;
            if (compService)
            {
              for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
              {
                nsCOMPtr <nsIMsgDBHdr> msgHdr;
                m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
                if (msgHdr)
                  rv = compService->ReplyWithTemplate(msgHdr, replyTemplateUri, m_msgWindow, server);
              }
            }
          }
        }
        break;
      case nsMsgFilterAction::DeleteFromPop3Server:
        {
          nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(m_curFolder);
          if (localFolder)
          {
            // This action ignores the deleteMailLeftOnServer preference
            localFolder->MarkMsgsOnPop3Server(m_searchHitHdrs, POP3_FORCE_DEL);

            nsCOMPtr <nsISupportsArray> partialMsgs;
            // Delete the partial headers. They're useless now
            // that the server copy is being deleted.
            for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
            {
              nsCOMPtr <nsIMsgDBHdr> msgHdr;
              m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
              if (msgHdr)
              {
                PRUint32 flags;
                msgHdr->GetFlags(&flags);
                if (flags & MSG_FLAG_PARTIAL)
                {
                  if (!partialMsgs)
                    partialMsgs = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
                  NS_ENSURE_SUCCESS(rv, rv);
                  partialMsgs->AppendElement(msgHdr);
                }
              }
            }
            if (partialMsgs)
              m_curFolder->DeleteMessages(partialMsgs, m_msgWindow, PR_TRUE, PR_FALSE, nsnull, PR_FALSE);
          }
        }
        break;
      case nsMsgFilterAction::FetchBodyFromPop3Server:
        {
          nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(m_curFolder);
          if (localFolder)
          {
            nsCOMPtr<nsISupportsArray> messages = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
            NS_ENSURE_SUCCESS(rv, rv);
            for (PRUint32 msgIndex = 0; msgIndex < m_searchHits.GetSize(); msgIndex++)
            {
              nsCOMPtr <nsIMsgDBHdr> msgHdr;
              m_searchHitHdrs->QueryElementAt(msgIndex, NS_GET_IID(nsIMsgDBHdr), getter_AddRefs(msgHdr));
              if (msgHdr)
              {
                PRUint32 flags = 0;
                msgHdr->GetFlags(&flags);
                if (flags & MSG_FLAG_PARTIAL)
                {
                  nsCOMPtr<nsISupports> iSupports = do_QueryInterface(msgHdr);
                  messages->AppendElement(iSupports);
                }
              }
            }
            PRUint32 msgsToFetch;
            messages->Count(&msgsToFetch);
            if (msgsToFetch > 0)
              m_curFolder->DownloadMessagesForOffline(messages, m_msgWindow);
          }
        }
        break;

      case nsMsgFilterAction::StopExecution:
      {
        // don't apply any more filters
        applyMoreActions = PR_FALSE; 
      }
      break;

      default:
        break;
      }
    }
  }
  if (applyMoreActions)
    rv = RunNextFilter();

  return rv;
}

NS_IMETHODIMP nsMsgFilterService::GetTempFilterList(nsIMsgFolder *aFolder, nsIMsgFilterList **aFilterList)
{
  NS_ENSURE_ARG_POINTER(aFilterList);
  nsMsgFilterList *filterList = new nsMsgFilterList;
  NS_ENSURE_TRUE(filterList, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aFilterList = filterList);
  (*aFilterList)->SetFolder(aFolder);
  filterList->m_temporaryList = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterService::ApplyFiltersToFolders(nsIMsgFilterList *aFilterList, nsISupportsArray *aFolders, nsIMsgWindow *aMsgWindow)
{
  nsMsgFilterAfterTheFact *filterExecutor = new nsMsgFilterAfterTheFact(aMsgWindow, aFilterList, aFolders);
  if (filterExecutor)
    return filterExecutor->AdvanceToNextFolder();
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

/* void OnStartCopy (); */
NS_IMETHODIMP nsMsgFilterAfterTheFact::OnStartCopy()
{
  return NS_OK;
}

/* void OnProgress (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP nsMsgFilterAfterTheFact::OnProgress(PRUint32 aProgress, PRUint32 aProgressMax)
{
  return NS_OK;
}

/* void SetMessageKey (in PRUint32 aKey); */
NS_IMETHODIMP nsMsgFilterAfterTheFact::SetMessageKey(PRUint32 /* aKey */)
{
  return NS_OK;
}

/* [noscript] void GetMessageId (in nsCString aMessageId); */
NS_IMETHODIMP nsMsgFilterAfterTheFact::GetMessageId(nsCString * /* aMessageId */)
{
  return NS_OK;
}

/* void OnStopCopy (in nsresult aStatus); */
NS_IMETHODIMP nsMsgFilterAfterTheFact::OnStopCopy(nsresult aStatus)
{
  PRBool continueExecution = NS_SUCCEEDED(aStatus);
  if (!continueExecution)
    continueExecution = ContinueExecutionPrompt();
  return  (continueExecution) ?  RunNextFilter() : OnEndExecution(aStatus);
}

PRBool nsMsgFilterAfterTheFact::ContinueExecutionPrompt()
{
  PRBool returnVal = PR_FALSE;
  nsresult rv;
  nsCOMPtr <nsIStringBundle> bundle;
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/filter.properties",
                                 getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle)
  {
    nsXPIDLString filterName;
    m_curFilter->GetFilterName(getter_Copies(filterName));
    nsXPIDLString formatString;
    nsXPIDLString confirmText;
    const PRUnichar *formatStrings[] =
    {
      filterName
    };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("continueFilterExecution").get(),
                                      formatStrings, 1, getter_Copies(confirmText));
    if (NS_SUCCEEDED(rv))
    {
      rv = DisplayConfirmationPrompt(m_msgWindow, confirmText, &returnVal);
    }
  }
  return returnVal;
}
nsresult
nsMsgFilterAfterTheFact::DisplayConfirmationPrompt(nsIMsgWindow *msgWindow, const PRUnichar *confirmString, PRBool *confirmed)
{
  nsresult rv=NS_OK;
  if (msgWindow)
  {
    nsCOMPtr <nsIDocShell> docShell;
    msgWindow->GetRootDocShell(getter_AddRefs(docShell));
    if (docShell)
    {
      nsCOMPtr<nsIPrompt> dialog(do_GetInterface(docShell));
      if (dialog && confirmString)
        dialog->Confirm(nsnull, confirmString, confirmed);
    }
  }
  return rv;
}
