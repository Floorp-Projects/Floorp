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

// this file implements the nsMsgFilter interface 

#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgHdr.h"
#include "nsMsgFilterList.h"    // for kFileVersion
#include "nsMsgFilter.h"
#include "nsMsgUtils.h"
#include "nsFileStream.h"
#include "nsMsgLocalSearch.h"
#include "nsMsgSearchTerm.h"
#include "nsXPIDLString.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsMsgSearchValue.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "nsMsgI18N.h"
#include "nsIImportService.h"
#include "nsISupportsObsolete.h"
#include "nsIOutputStream.h"
#include "nsEscape.h"

static const char *kImapPrefix = "//imap:";
static const char *kWhitespace = "\b\t\r\n ";

nsMsgRuleAction::nsMsgRuleAction()
{
}

nsMsgRuleAction::~nsMsgRuleAction()
{
}

NS_IMPL_ISUPPORTS1(nsMsgRuleAction, nsIMsgRuleAction)

NS_IMPL_GETSET(nsMsgRuleAction, Type, nsMsgRuleActionType, m_type)

NS_IMETHODIMP nsMsgRuleAction::SetPriority(nsMsgPriorityValue aPriority)
{
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::ChangePriority, 
                NS_ERROR_ILLEGAL_VALUE);
  m_priority = aPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgRuleAction::GetPriority(nsMsgPriorityValue *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::ChangePriority,
                 NS_ERROR_ILLEGAL_VALUE);
  *aResult = m_priority;
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgRuleAction::SetLabel(nsMsgLabelValue aLabel)
{
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::Label,
                 NS_ERROR_ILLEGAL_VALUE);
  m_label = aLabel;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgRuleAction::GetLabel(nsMsgLabelValue *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::Label, NS_ERROR_ILLEGAL_VALUE);
  *aResult = m_label;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgRuleAction::SetTargetFolderUri(const char *aUri)
{
  NS_ENSURE_ARG_POINTER(aUri);
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::MoveToFolder,
                 NS_ERROR_ILLEGAL_VALUE);
  m_folderUri = aUri;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgRuleAction::GetTargetFolderUri(char** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::MoveToFolder,
                 NS_ERROR_ILLEGAL_VALUE);
  *aResult = ToNewCString(m_folderUri);
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgRuleAction::SetJunkScore(PRInt32 aJunkScore)
{
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::JunkScore && aJunkScore >= 0 && aJunkScore <= 100,
                 NS_ERROR_ILLEGAL_VALUE);
  m_junkScore = aJunkScore;
  return NS_OK;
}

NS_IMETHODIMP
nsMsgRuleAction::GetJunkScore(PRInt32 *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_type == nsMsgFilterAction::JunkScore, NS_ERROR_ILLEGAL_VALUE);
  *aResult = m_junkScore;
  return NS_OK;
}

nsMsgFilter::nsMsgFilter():
    m_temporary(PR_FALSE),
    m_unparseable(PR_FALSE),
    m_filterList(nsnull)
{
  NS_NewISupportsArray(getter_AddRefs(m_termList));
  NS_NewISupportsArray(getter_AddRefs(m_actionList));

  m_type = nsMsgFilterType::InboxRule;
}

nsMsgFilter::~nsMsgFilter()
{
}

NS_IMPL_ISUPPORTS1(nsMsgFilter, nsIMsgFilter)

NS_IMPL_GETSET(nsMsgFilter, FilterType, nsMsgFilterTypeType, m_type)
NS_IMPL_GETSET(nsMsgFilter, Enabled, PRBool, m_enabled)
NS_IMPL_GETSET(nsMsgFilter, Temporary, PRBool, m_temporary)
NS_IMPL_GETSET(nsMsgFilter, Unparseable, PRBool, m_unparseable)

NS_IMETHODIMP nsMsgFilter::GetFilterName(PRUnichar **name)
{
  NS_ENSURE_ARG_POINTER(name);  
  *name = ToNewUnicode(m_filterName);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetFilterName(const PRUnichar *name)
{
  m_filterName.Assign(name);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetFilterDesc(char **description)
{
  NS_ENSURE_ARG_POINTER(description);
  *description = ToNewCString(m_description);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetFilterDesc(const char *description)
{
  m_description.Assign(description);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetUnparsedBuffer(char **unparsedBuffer)
{
  NS_ENSURE_ARG_POINTER(unparsedBuffer);
  *unparsedBuffer = ToNewCString(m_unparsedBuffer);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetUnparsedBuffer(const char *unparsedBuffer)
{
  m_unparsedBuffer.Assign(unparsedBuffer);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::AddTerm(     
                                   nsMsgSearchAttribValue attrib,    /* attribute for this term          */
                                   nsMsgSearchOpValue op,         /* operator e.g. opContains           */
                                   nsIMsgSearchValue *value,        /* value e.g. "Dogbert"               */
                                  PRBool BooleanAND, 	    /* PR_TRUE if AND is the boolean operator.
                                                            PR_FALSE if OR is the boolean operators */
                                  const char * arbitraryHeader)  /* arbitrary header specified by user.
                                  ignored unless attrib = attribOtherHeader */
{
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::AppendTerm(nsIMsgSearchTerm * aTerm)
{
    NS_ENSURE_TRUE(aTerm, NS_ERROR_NULL_POINTER);
    
    return m_termList->AppendElement(NS_STATIC_CAST(nsISupports*,aTerm));
}

NS_IMETHODIMP
nsMsgFilter::CreateTerm(nsIMsgSearchTerm **aResult)
{
    nsMsgSearchTerm *term = new nsMsgSearchTerm;
    NS_ENSURE_TRUE(term, NS_ERROR_OUT_OF_MEMORY);

    *aResult = NS_STATIC_CAST(nsIMsgSearchTerm*,term);
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgFilter::CreateAction(nsIMsgRuleAction **aAction)
{
  NS_ENSURE_ARG_POINTER(aAction);
  nsMsgRuleAction *action = new nsMsgRuleAction;
  NS_ENSURE_TRUE(action, NS_ERROR_OUT_OF_MEMORY);

  *aAction = NS_STATIC_CAST(nsIMsgRuleAction*,action);
  NS_ADDREF(*aAction);
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgFilter::GetSortedActionList(nsISupportsArray *actionList)
{
  NS_ENSURE_ARG_POINTER(actionList);
  PRUint32 numActions;
  nsresult err = m_actionList->Count(&numActions);
  NS_ENSURE_SUCCESS(err, err);

  for (PRUint32 index =0; index < numActions; index++)
  {
    nsCOMPtr<nsIMsgRuleAction> action;
    err = m_actionList->QueryElementAt(index, NS_GET_IID(nsIMsgRuleAction), (void **)getter_AddRefs(action));
    if (!action)
      continue;
 
    nsMsgRuleActionType actionType;
    action->GetType(&actionType);
    if (actionType == nsMsgFilterAction::MoveToFolder)  //we always want MoveToFolder action to be last
      actionList->AppendElement(action);
    else
      actionList->InsertElementAt(action,0);
  }
  return err;
}

NS_IMETHODIMP
nsMsgFilter::AppendAction(nsIMsgRuleAction *aAction)
{
  return m_actionList->AppendElement(NS_STATIC_CAST(nsISupports*,aAction));
}

NS_IMETHODIMP
nsMsgFilter::GetActionAt(PRInt32 aIndex, nsIMsgRuleAction **aAction)
{
  NS_ENSURE_ARG_POINTER(aAction);
  return m_actionList->QueryElementAt(aIndex, NS_GET_IID(nsIMsgRuleAction), 
                                       (void **) aAction);
}

NS_IMETHODIMP
nsMsgFilter::GetActionList(nsISupportsArray **actionList)
{
  NS_IF_ADDREF(*actionList = m_actionList);
  return NS_OK;
}

NS_IMETHODIMP  //for editing a filter
nsMsgFilter::ClearActionList()
{
  return m_actionList->Clear();
}

NS_IMETHODIMP nsMsgFilter::GetTerm(PRInt32 termIndex, 
                                   nsMsgSearchAttribValue *attrib,    /* attribute for this term          */
                                   nsMsgSearchOpValue *op,         /* operator e.g. opContains           */
                                   nsIMsgSearchValue **value,         /* value e.g. "Dogbert"               */
                                   PRBool *booleanAnd, /* PR_TRUE if AND is the boolean operator. PR_FALSE if OR is the boolean operator */
                                   char ** arbitraryHeader) /* arbitrary header specified by user.ignore unless attrib = attribOtherHeader */
{
  nsresult rv;
  nsCOMPtr<nsIMsgSearchTerm> term;
  rv = m_termList->QueryElementAt(termIndex, NS_GET_IID(nsIMsgSearchTerm),
                                    (void **)getter_AddRefs(term));
  if (NS_SUCCEEDED(rv) && term)
  {
    if(attrib)
      term->GetAttrib(attrib);
    if(op)
      term->GetOp(op);
    if(value)
      term->GetValue(value);
    if(booleanAnd)
      term->GetBooleanAnd(booleanAnd);
    if (attrib && arbitraryHeader 
        && *attrib > nsMsgSearchAttrib::OtherHeader 
        && *attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)
      term->GetArbitraryHeader(arbitraryHeader);
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetSearchTerms(nsISupportsArray **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_IF_ADDREF(*aResult = m_termList);
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetSearchTerms(nsISupportsArray *aSearchList)
{
    m_termList = aSearchList;
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetScope(nsIMsgSearchScopeTerm *aResult)
{
    m_scope = aResult;
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetScope(nsIMsgSearchScopeTerm **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_IF_ADDREF(*aResult = m_scope);
    return NS_OK;
}

#define LOG_ENTRY_START_TAG "<p>\n"
#define LOG_ENTRY_START_TAG_LEN (strlen(LOG_ENTRY_START_TAG))
#define LOG_ENTRY_END_TAG "</p>\n"
#define LOG_ENTRY_END_TAG_LEN (strlen(LOG_ENTRY_END_TAG))

NS_IMETHODIMP nsMsgFilter::LogRuleHit(nsIMsgRuleAction *aFilterAction, nsIMsgDBHdr *aMsgHdr)
{
    nsCOMPtr <nsIOutputStream> logStream;
    nsresult rv = m_filterList->GetLogStream(getter_AddRefs(logStream));
    NS_ENSURE_SUCCESS(rv,rv);
  
    PRTime date;
    char dateStr[40];	/* 30 probably not enough */
    nsMsgRuleActionType actionType;
    
    nsXPIDLCString author;
    nsXPIDLCString subject;
    nsXPIDLString filterName;

    GetFilterName(getter_Copies(filterName));
    aFilterAction->GetType(&actionType);
    rv = aMsgHdr->GetDate(&date);
    PRExplodedTime exploded;
    PR_ExplodeTime(date, PR_LocalTimeParameters, &exploded);
    // XXX Temporary until logging is fully localized
    PR_FormatTimeUSEnglish(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M:%S", &exploded);

    aMsgHdr->GetAuthor(getter_Copies(author));
    aMsgHdr->GetSubject(getter_Copies(subject));

    nsCString buffer;
    // this is big enough to hold a log entry.  
    // do this so we avoid growing and copying as we append to the log.
    buffer.SetCapacity(512);  
    
    buffer = "Applied filter \"";
    buffer +=  NS_ConvertUCS2toUTF8(filterName).get();
    buffer +=  "\" to message from ";
    buffer +=  (const char*)author;
    buffer +=  " - ";
    buffer +=  (const char *)subject;
    buffer +=  " at ";
    buffer +=  dateStr;
    buffer +=  "\n";
    const char *actionStr = GetActionStr(actionType);
    buffer +=  "Action = ";
    buffer +=  actionStr;
    buffer +=  " ";
        
    if (actionType == nsMsgFilterAction::MoveToFolder) {
      nsXPIDLCString actionFolderUri;
      aFilterAction->GetTargetFolderUri(getter_Copies(actionFolderUri));
      buffer += actionFolderUri.get();
    } 
         
    buffer += "\n";
    if (actionType == nsMsgFilterAction::MoveToFolder) {
      nsXPIDLCString msgId;
      aMsgHdr->GetMessageId(getter_Copies(msgId));
      buffer += " id = ";
      buffer += (const char*)msgId;
      buffer += "\n";
    }
    
    PRUint32 writeCount;

    rv = logStream->Write(LOG_ENTRY_START_TAG, LOG_ENTRY_START_TAG_LEN, &writeCount);
    NS_ENSURE_SUCCESS(rv,rv);
    NS_ASSERTION(writeCount == LOG_ENTRY_START_TAG_LEN, "failed to write out start log tag");

    // html escape the log for security reasons.
    // we don't want some to send us a message with a subject with
    // html tags, especially <script>
    char *escapedBuffer = nsEscapeHTML(buffer.get());
    if (!escapedBuffer)
      return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 escapedBufferLen = strlen(escapedBuffer);
    rv = logStream->Write(escapedBuffer, escapedBufferLen, &writeCount);
    PR_Free(escapedBuffer);
    NS_ENSURE_SUCCESS(rv,rv);
    NS_ASSERTION(writeCount == escapedBufferLen, "failed to write out log hit");
 
    rv = logStream->Write(LOG_ENTRY_END_TAG, LOG_ENTRY_END_TAG_LEN, &writeCount);
    NS_ENSURE_SUCCESS(rv,rv);
    NS_ASSERTION(writeCount == LOG_ENTRY_END_TAG_LEN, "failed to write out end log tag");
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::MatchHdr(nsIMsgDBHdr	*msgHdr, nsIMsgFolder *folder, nsIMsgDatabase *db, 
									const char *headers, PRUint32 headersSize, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(folder);
  // use offlineMail because
  nsMsgSearchScopeTerm* scope = new nsMsgSearchScopeTerm(nsnull, nsMsgSearchScope::offlineMail, folder);
  if (!scope) return NS_ERROR_OUT_OF_MEMORY;
  
  nsXPIDLCString folderCharset;
  folder->GetCharset(getter_Copies(folderCharset));
  nsresult rv = nsMsgSearchOfflineMail::MatchTermsForFilter(msgHdr, m_termList,
                  folderCharset.get(),  scope,  db,  headers,  headersSize, pResult);
  delete scope;
  return rv;
}

void
nsMsgFilter::SetFilterList(nsIMsgFilterList *filterList)
{
  // doesn't hold a ref.
  m_filterList = filterList;
}

nsresult
nsMsgFilter::GetFilterList(nsIMsgFilterList **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_IF_ADDREF(*aResult = m_filterList);
    return NS_OK;
}

void nsMsgFilter::SetFilterScript(nsCString *fileName) 
{
  m_scriptFileName = *fileName;
}

nsresult nsMsgFilter::ConvertMoveToFolderValue(nsIMsgRuleAction *filterAction, nsCString &moveValue)
{
  NS_ENSURE_ARG_POINTER(filterAction);
  PRInt16 filterVersion = kFileVersion;
  if (m_filterList)
      m_filterList->GetVersion(&filterVersion);
  if (filterVersion <= k60Beta1Version)
  {
    nsCOMPtr <nsIImportService> impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
    NS_ASSERTION(impSvc, "cannot get importService");
    nsCOMPtr <nsIMsgFolder> rootFolder;
    nsXPIDLCString folderUri;

    m_filterList->GetFolder(getter_AddRefs(rootFolder));

	  // if relative path starts with kImap, this is a move to folder on the same server
    if (moveValue.Find(kImapPrefix) == 0)
    {
      PRInt32 prefixLen = PL_strlen(kImapPrefix);
      nsCAutoString originalServerPath;
      moveValue.Mid(originalServerPath, prefixLen, moveValue.Length() - prefixLen);
      if ( filterVersion == k45Version && impSvc)
      {
        nsAutoString unicodeStr;
        impSvc->SystemStringToUnicode(originalServerPath.get(), unicodeStr);
        nsresult rv = CopyUTF16toMUTF7(unicodeStr, originalServerPath);
        NS_ENSURE_SUCCESS(rv, rv); 
      }

      nsCOMPtr <nsIMsgFolder> destIFolder;
      if (rootFolder)
      {
        rootFolder->FindSubFolder(originalServerPath, getter_AddRefs(destIFolder));
        if (destIFolder)
        {
          destIFolder->GetURI(getter_Copies(folderUri));
          filterAction->SetTargetFolderUri(folderUri);
          moveValue.Assign(folderUri);
        }
      }
    }
	  else
    {
      // start off leaving the value the same.
      filterAction->SetTargetFolderUri(moveValue.get());
      nsresult rv = NS_OK;
      nsCOMPtr <nsIMsgFolder> localMailRoot;
      rootFolder->GetURI(getter_Copies(folderUri));
      // if the root folder is not imap, than the local mail root is the server root.
      // otherwise, it's the migrated local folders.
      if (nsCRT::strncmp("imap:", folderUri, 5))
      {
        localMailRoot = rootFolder;
      }
      else
      {
        nsCOMPtr<nsIMsgAccountManager> accountManager = 
                 do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
        {
          nsCOMPtr <nsIMsgIncomingServer> server; 
          rv = accountManager->GetLocalFoldersServer(getter_AddRefs(server)); 
          if (NS_SUCCEEDED(rv) && server)
          {
            rv = server->GetRootFolder(getter_AddRefs(localMailRoot));
          }
        }
      }
      if (NS_SUCCEEDED(rv) && localMailRoot)
      {
        nsXPIDLCString localRootURI;
        nsCOMPtr <nsIMsgFolder> destIMsgFolder;
        nsCOMPtr <nsIMsgFolder> localMailRootMsgFolder = do_QueryInterface(localMailRoot);
        localMailRoot->GetURI(getter_Copies(localRootURI));
        nsCString destFolderUri;
        destFolderUri.Assign( localRootURI);
        // need to remove ".sbd" from moveValue, and perhaps escape it.
        moveValue.ReplaceSubstring(".sbd/", "/");

#if defined(XP_MAC) || defined(XP_MACOSX)
        char *unescapedMoveValue = ToNewCString(moveValue);
        nsUnescape(unescapedMoveValue);
        moveValue.Assign(unescapedMoveValue);
        nsCRT::free(unescapedMoveValue);
#endif
        destFolderUri.Append('/');
        if ( filterVersion == k45Version && impSvc)
        {
          nsAutoString unicodeStr;
          impSvc->SystemStringToUnicode(moveValue.get(), unicodeStr);
          rv = NS_MsgEscapeEncodeURLPath(unicodeStr, moveValue);
        }
        destFolderUri.Append(moveValue);
        localMailRootMsgFolder->GetChildWithURI (destFolderUri.get(), PR_TRUE, PR_FALSE /*caseInsensitive*/, getter_AddRefs(destIMsgFolder));

        if (destIMsgFolder)
        {
          destIMsgFolder->GetURI(getter_Copies(folderUri));
		      filterAction->SetTargetFolderUri(folderUri);
          moveValue.Assign(folderUri);
        }
      }
    }
  }
  else
    filterAction->SetTargetFolderUri(moveValue.get());
    
  return NS_OK;
	// set m_action.m_value.m_folderUri
}

nsresult nsMsgFilter::SaveToTextFile(nsIOFileStream *aStream)
{
  NS_ENSURE_ARG_POINTER(aStream);
  if (m_unparseable)
  {
    //we need to trim leading whitespaces before filing out
    m_unparsedBuffer.Trim(kWhitespace, PR_TRUE /*leadingCharacters*/, PR_FALSE /*trailingCharacters*/);
    *aStream << m_unparsedBuffer.get();
    return NS_OK;
  }
  nsresult err = m_filterList->WriteWstrAttr(nsIMsgFilterList::attribName, m_filterName.get(), aStream);
  err = m_filterList->WriteBoolAttr(nsIMsgFilterList::attribEnabled, m_enabled, aStream);
  err = m_filterList->WriteStrAttr(nsIMsgFilterList::attribDescription, m_description.get(), aStream);
  err = m_filterList->WriteIntAttr(nsIMsgFilterList::attribType, m_type, aStream);
  if (IsScript())
    err = m_filterList->WriteStrAttr(nsIMsgFilterList::attribScriptFile, m_scriptFileName.get(), aStream);
  else
    err = SaveRule(aStream);
  return err;
}

nsresult nsMsgFilter::SaveRule(nsIOFileStream *aStream)
{
  nsresult err = NS_OK;
  nsCOMPtr<nsIMsgFilterList> filterList;
  GetFilterList(getter_AddRefs(filterList));
  nsCAutoString	actionFilingStr;
   
  PRUint32 numActions;
  err = m_actionList->Count(&numActions);
  NS_ENSURE_SUCCESS(err, err);

  for (PRUint32 index =0; index < numActions; index++)
  {
    nsCOMPtr<nsIMsgRuleAction> action;
    err = m_actionList->QueryElementAt(index, NS_GET_IID(nsIMsgRuleAction), (void **)getter_AddRefs(action));
    if (!action)
      continue;
 
    nsMsgRuleActionType actionType;
    action->GetType(&actionType);
    GetActionFilingStr(actionType, actionFilingStr);
  
    err = filterList->WriteStrAttr(nsIMsgFilterList::attribAction, actionFilingStr.get(), aStream);
    NS_ENSURE_SUCCESS(err, err);
  
    switch(actionType)
    {
      case nsMsgFilterAction::MoveToFolder:
      {
        nsXPIDLCString imapTargetString;
        action->GetTargetFolderUri(getter_Copies(imapTargetString));
        err = filterList->WriteStrAttr(nsIMsgFilterList::attribActionValue, imapTargetString.get(), aStream);
      }
      break;
      case nsMsgFilterAction::ChangePriority:
      {
        nsMsgPriorityValue priorityValue;
        action->GetPriority(&priorityValue);
        nsAutoString priority;
        NS_MsgGetUntranslatedPriorityName (priorityValue, &priority);
        nsCAutoString cStr;
        cStr.AssignWithConversion(priority);
        err = filterList->WriteStrAttr(nsIMsgFilterList::attribActionValue, cStr.get(), aStream);
      }
      break;
      case nsMsgFilterAction::Label:
      {
        nsMsgLabelValue label;
        action->GetLabel(&label);
        err = filterList->WriteIntAttr(nsIMsgFilterList::attribActionValue, label, aStream);
      }
      break;
      case nsMsgFilterAction::JunkScore:
      {
        PRInt32 junkScore;
        action->GetJunkScore(&junkScore);
        err = filterList->WriteIntAttr(nsIMsgFilterList::attribActionValue, junkScore, aStream);
      }
      default:
        break;
    }
  }
  // and here the fun begins - file out term list...
  PRUint32 searchIndex;
  nsCAutoString  condition;
  PRUint32 count;
  m_termList->Count(&count);
  for (searchIndex = 0; searchIndex < count && NS_SUCCEEDED(err);
  searchIndex++)
  {
    nsCAutoString	stream;
    
    nsCOMPtr<nsIMsgSearchTerm> term;
    m_termList->QueryElementAt(searchIndex, NS_GET_IID(nsIMsgSearchTerm),
      (void **)getter_AddRefs(term));
    if (!term)
      continue;
    
    if (condition.Length() > 1)
      condition += ' ';
    
    PRBool booleanAnd;
    term->GetBooleanAnd(&booleanAnd);
    if (booleanAnd)
      condition += "AND (";
    else
      condition += "OR (";
    
    nsresult searchError = term->EnStreamNew(stream);
    if (NS_FAILED(searchError))
    {
      err = searchError;
      break;
    }
    
    condition += stream;
    condition += ')';
  }
  if (NS_SUCCEEDED(err))
    err = filterList->WriteStrAttr(nsIMsgFilterList::attribCondition, condition.get(), aStream);
  return err;
}

// for each action, this table encodes the filterTypes that support the action.
struct RuleActionsTableEntry
{
	nsMsgRuleActionType	action;
	nsMsgFilterTypeType		supportedTypes;
	PRInt32				xp_strIndex;
	const char			*actionFilingStr;	/* used for filing out filters, don't translate! */
};

static struct RuleActionsTableEntry ruleActionsTable[] =
{
  { nsMsgFilterAction::MoveToFolder,    nsMsgFilterType::Inbox, 0,  "Move to folder"},
  { nsMsgFilterAction::ChangePriority,  nsMsgFilterType::Inbox, 0,  "Change priority"},
  { nsMsgFilterAction::Delete,          nsMsgFilterType::All,   0,  "Delete"},
  { nsMsgFilterAction::MarkRead,        nsMsgFilterType::All,   0,  "Mark read"},
  { nsMsgFilterAction::KillThread,      nsMsgFilterType::All,   0,  "Ignore thread"},
  { nsMsgFilterAction::WatchThread,     nsMsgFilterType::All,   0,  "Watch thread"},
  { nsMsgFilterAction::MarkFlagged,     nsMsgFilterType::All,   0,  "Mark flagged"},
  { nsMsgFilterAction::Label,           nsMsgFilterType::All,   0,  "Label"},
  { nsMsgFilterAction::Reply,           nsMsgFilterType::All,   0,  "Reply"},
  { nsMsgFilterAction::Forward,         nsMsgFilterType::All,   0,  "Forward"},
  { nsMsgFilterAction::StopExecution,   nsMsgFilterType::All,   0,  "Stop execution"},
  { nsMsgFilterAction::DeleteFromPop3Server, nsMsgFilterType::All,   0, "Delete from Pop3 server"},
  { nsMsgFilterAction::LeaveOnPop3Server, nsMsgFilterType::All,   0, "Leave on Pop3 server"},
  { nsMsgFilterAction::JunkScore, nsMsgFilterType::All,   0, "JunkScore"},
  { nsMsgFilterAction::FetchBodyFromPop3Server, nsMsgFilterType::All,   0, "Fetch body from Pop3Server"},
};

const char *nsMsgFilter::GetActionStr(nsMsgRuleActionType action)
{
  int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);
  
  for (int i = 0; i < numActions; i++)
  {
    if (action == ruleActionsTable[i].action)
      return ruleActionsTable[i].actionFilingStr; 
  }
  return "";
}
/*static */nsresult nsMsgFilter::GetActionFilingStr(nsMsgRuleActionType action, nsCString &actionStr)
{
  int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);
  
  for (int i = 0; i < numActions; i++)
  {
    if (action == ruleActionsTable[i].action)
    {
      actionStr = ruleActionsTable[i].actionFilingStr;
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}


nsMsgRuleActionType nsMsgFilter::GetActionForFilingStr(nsCString &actionStr)
{
  int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);
  
  for (int i = 0; i < numActions; i++)
  {
    if (actionStr.Equals(ruleActionsTable[i].actionFilingStr))
      return ruleActionsTable[i].action;
  }
  return nsMsgFilterAction::None;
}

PRInt16
nsMsgFilter::GetVersion()
{
    if (!m_filterList) return 0;
    PRInt16 version;
    m_filterList->GetVersion(&version);
    return version;
}

#ifdef DEBUG
void nsMsgFilter::Dump()
{
  nsCAutoString s;
  CopyUCS2toASCII(m_filterName, s);
  printf("filter %s type = %c desc = %s\n", s.get(), m_type + '0', m_description.get());
}
#endif

