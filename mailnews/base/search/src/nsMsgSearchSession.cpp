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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchSession.h"
#include "nsMsgResultElement.h"
#include "nsMsgSearchTerm.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS2(nsMsgSearchSession, nsIMsgSearchSession, nsIUrlListener)

nsMsgSearchSession::nsMsgSearchSession()
{
  NS_INIT_ISUPPORTS();
	m_sortAttribute = nsMsgSearchAttrib::Sender;
	m_descending = PR_FALSE;
	m_idxRunningScope = 0; 
	m_parallel = PR_FALSE;
//	m_calledStartingUpdate = PR_FALSE;
	m_handlingError = PR_FALSE;
	m_pSearchParam = nsnull;

}

nsMsgSearchSession::~nsMsgSearchSession()
{
	DestroyResultList ();
	DestroyScopeList ();
	DestroyTermList ();

    PR_FREEIF (m_pSearchParam);
}

/* [noscript] void AddSearchTerm (in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value, in boolean BooleanAND, in string arbitraryHeader); */
NS_IMETHODIMP nsMsgSearchSession::AddSearchTerm(nsMsgSearchAttribute attrib, nsMsgSearchOperator op, nsMsgSearchValue * value, PRBool BooleanAND, const char *arbitraryHeader)
{
	nsMsgSearchTerm *pTerm = new nsMsgSearchTerm (attrib, op, value, nsMsgSearchBooleanOp::BooleanAND, arbitraryHeader);
	if (nsnull == pTerm)
		return NS_ERROR_OUT_OF_MEMORY;
	m_termList.AppendElement (pTerm);
	return NS_OK;
}

/* readonly attribute long numSearchTerms; */
NS_IMETHODIMP nsMsgSearchSession::GetNumSearchTerms(PRInt32 *aNumSearchTerms)
{
  NS_ENSURE_ARG(aNumSearchTerms);
  *aNumSearchTerms = m_termList.Count();
  return NS_OK;
}

/* [noscript] void GetNthSearchTerm (in long whichTerm, in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value); */
NS_IMETHODIMP nsMsgSearchSession::GetNthSearchTerm(PRInt32 whichTerm, nsMsgSearchAttribute attrib, nsMsgSearchOperator op, nsMsgSearchValue * value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long MSG_CountSearchScopes (); */
NS_IMETHODIMP nsMsgSearchSession::MSG_CountSearchScopes(PRInt32 *_retval)
{
  NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] voidStar MSG_GetNthSearchScope (in long which, in nsMsgSearchScope scopeId); */
NS_IMETHODIMP nsMsgSearchSession::MSG_GetNthSearchScope(PRInt32 which, nsMsgSearchScope *scopeId, void * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddScopeTerm (in nsMsgSearchScope attrib, in nsIMsgFolder folder); */
NS_IMETHODIMP nsMsgSearchSession::AddScopeTerm(nsMsgSearchScopeAttribute attrib, nsIMsgFolder *folder)
{
  if (attrib != nsMsgSearchScope::AllSearchableGroups)
	{
		NS_ASSERTION(folder, "need folder if not searching all groups");
		if (!folder)
			return NS_ERROR_NULL_POINTER;
	}

	nsresult err = NS_OK;

  if (attrib == nsMsgSearchScope::MailFolder)
	{
#if 0
		// It's legal to have a folderInfo which is only a directory, but has no
		// mail folder or summary file. However, such a folderInfo isn't a legal 
		// scopeTerm, so turn it away here
		if (mailFolder && !XP_Stat(mailFolder->GetPathname(), &fileStat, xpMailFolder) && S_ISDIR(fileStat.st_mode))
			err = SearchError_InvalidFolder;

		// IMAP folders can have a \NOSELECT flag which means that they can't
		// ever be opened. Since we have to SELECT a folder in order to search
		// it, we'll just omit this folder from the list of search scopes
		MSG_IMAPFolderInfoMail *imapFolder = folder->GetIMAPFolderInfoMail();
		if (imapFolder && !imapFolder->GetCanIOpenThisFolder())
			return NS_OK;
#endif
	}

  if ((attrib == nsMsgSearchScope::Newsgroup || attrib == nsMsgSearchScope::OfflineNewsgroup) /* && folder->IsNews() */)
	{
#if 0
		// Even unsubscribed newsgroups have a folderInfo, so filter them
		// out here, adding only the newsgroups we are subscribed to
		if (!newsFolder->IsSubscribed())
			return NS_OK;

		// It would be nice if the FEs did this, but I guess no one knows
		// that offline news searching is supposed to work
		if (NET_IsOffline())
      attrib = nsMsgSearchScope::OfflineNewsgroup;
#endif
	}

  if (attrib == nsMsgSearchScope::AllSearchableGroups)
	{
#if 0
		// Try to be flexible about what we get here. It could be a news group,
		// news host, or NULL, which uses the default host.
		if (folder == nsnull)
		{
			// I dunno how much of this can be NULL, so I'm not assuming anything
			MSG_NewsHost *host = NULL;
			msg_HostTable *table = m_pane->GetMaster()->GetHostTable();
			if (table)
			{
				host = table->GetDefaultHost(FALSE /*###tw*/);
				if (host)
					folder = host->GetHostInfo();
			}
		}
		else
		{
			switch (folder->GetType())
			{
			case FOLDER_CONTAINERONLY:
				break; // this is what we want -- nothing to do 
			case FOLDER_NEWSGROUP:
			case FOLDER_CATEGORYCONTAINER:
				{
					MSG_NewsHost *host = ((MSG_FolderInfoNews*) folder)->GetHost();
					folder = host->GetHostInfo();
				}
		    default:
			  break;
			}
		}
#endif
	}

	if (NS_SUCCEEDED(err))
	{
		nsMsgSearchScopeTerm *pScope = new nsMsgSearchScopeTerm (this, attrib, folder);
		if (pScope)
			m_scopeList.AppendElement (pScope);
		else
			err = NS_ERROR_OUT_OF_MEMORY;
	}

	return err;
}

/* [noscript] void AddLdapScope (in nsMsgDIRServer server); */
NS_IMETHODIMP nsMsgSearchSession::AddLdapScope(nsMsgDIRServer * server)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] boolean ScopeUsesCustomHeaders (in nsMsgSearchScope scope, in voidStar selection, in boolean forFilters); */
NS_IMETHODIMP nsMsgSearchSession::ScopeUsesCustomHeaders(nsMsgSearchScopeAttribute scope, void * selection, PRBool forFilters, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsStringAttribute (in nsMsgSearchAttribute attrib); */
NS_IMETHODIMP nsMsgSearchSession::IsStringAttribute(nsMsgSearchAttribute attrib, PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddAllScopes (in nsMsgSearchScope attrib); */
NS_IMETHODIMP nsMsgSearchSession::AddAllScopes(nsMsgSearchScopeAttribute attrib)
{
  // don't think this is needed.
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Search (); */
NS_IMETHODIMP nsMsgSearchSession::Search(nsIMsgWindow *aWindow)
{
	nsresult err = Initialize ();
  m_window = aWindow;
	if (NS_SUCCEEDED(err))
		err = BeginSearching ();
	return err;
}

/* void InterruptSearch (); */
NS_IMETHODIMP nsMsgSearchSession::InterruptSearch()
{
    return NS_OK;
}

/* [noscript] readonly attribute voidStar searchParam; */
NS_IMETHODIMP nsMsgSearchSession::GetSearchParam(void * *aSearchParam)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsMsgSearchType searchType; */
NS_IMETHODIMP nsMsgSearchSession::GetSearchType(nsMsgSearchType * *aSearchType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] nsMsgSearchType SetSearchParam (in nsMsgSearchType type, in voidStar param); */
NS_IMETHODIMP nsMsgSearchSession::SetSearchParam(nsMsgSearchType *type, void * param, nsMsgSearchType **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long numResults; */
NS_IMETHODIMP nsMsgSearchSession::GetNumResults(PRInt32 *aNumResults)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnStartRunningUrl (in nsIURI url); */
NS_IMETHODIMP nsMsgSearchSession::OnStartRunningUrl(nsIURI *url)
{
    return NS_OK;
}

/* void OnStopRunningUrl (in nsIURI url, in nsresult aExitCode); */
NS_IMETHODIMP nsMsgSearchSession::OnStopRunningUrl(nsIURI *url, nsresult aExitCode)
{
  GetNextUrl();
    return NS_OK;
}


nsresult nsMsgSearchSession::Initialize()
{
	// Loop over scope terms, initializing an adapter per term. This 
	// architecture is necessitated by two things: 
	// 1. There might be more than one kind of adapter per if online 
	//    *and* offline mail mail folders are selected, or if newsgroups
	//    belonging to Dredd *and* INN are selected
	// 2. Most of the protocols are only capable of searching one scope at a
	//    time, so we'll do each scope in a separate adapter on the client

	nsMsgSearchScopeTerm *scopeTerm = nsnull;
	nsresult err = NS_OK;

	// Ensure that the FE has added scopes and terms to this search
	NS_ASSERTION(m_termList.Count() > 0, "no terms to search!");
	if (m_termList.Count() == 0)
		return NS_MSG_ERROR_NO_SEARCH_VALUES;

	// if we don't have any search scopes to search, return that code. 
	if (m_scopeList.Count() == 0)
		return NS_MSG_ERROR_INVALID_SEARCH_SCOPE;

	// If this term list (loosely specified here by the first term) should be
	// scheduled in parallel, build up a list of scopes to do the round-robin scheduling
	scopeTerm = m_scopeList.ElementAt(0);

	for (int i = 0; i < m_scopeList.Count() && NS_SUCCEEDED(err); i++)
	{
		scopeTerm = m_scopeList.ElementAt(i);
		// NS_ASSERTION(scopeTerm->IsValid());

		err = scopeTerm->InitializeAdapter (m_termList);

//		if (scopeTerm->m_folder->GetType() == FOLDER_MAIL)
//			m_offlineProgressTotal += scopeTerm->m_folder->GetTotalMessages();
	}

	return err;
}

nsresult nsMsgSearchSession::BeginSearching()
{
	nsresult err = NS_OK;;

	// Here's a sloppy way to start the URL, but I don't really have time to
	// unify the scheduling mechanisms. If the first scope is a newsgroup, and
	// it's not Dredd-capable, we build the URL queue. All other searches can be
	// done with one URL

	nsMsgSearchScopeTerm *scope = m_scopeList.ElementAt(0);
	if (scope->m_attribute == nsMsgSearchScope::Newsgroup /* && !scope->m_folder->KnowsSearchNntpExtension() */ && scope->m_searchServer)
		err = BuildUrlQueue ();
	else if (scope->m_attribute == nsMsgSearchScope::MailFolder && !scope->IsOfflineMail())
		err = BuildUrlQueue ();
	else
		err = SearchWOUrls();

	return err;
}


nsresult nsMsgSearchSession::BuildUrlQueue ()
{
	PRInt32 i;
	for (i = 0; i < m_scopeList.Count(); i++)
	{
		nsCOMPtr <nsIMsgSearchAdapter> adapter = do_QueryInterface((m_scopeList.ElementAt(i))->m_adapter);
		nsXPIDLCString url;
		adapter->GetEncoding(getter_Copies(url));
		AddUrl (url);
	}

	if (i > 0)
		GetNextUrl();

	return NS_OK;
}


nsresult nsMsgSearchSession::GetNextUrl()
{
  nsCString nextUrl;
  nsCOMPtr <nsIMsgMessageService> msgService;

  m_urlQueue.CStringAt(0, nextUrl);
  nsresult rv = GetMessageServiceFromURI(nextUrl.GetBuffer(), getter_AddRefs(msgService));
  nsMsgSearchScopeTerm *currentTerm = GetRunningScope();

  if (NS_SUCCEEDED(rv) && msgService && currentTerm)
    msgService->Search(this, m_window, currentTerm->m_folder, nextUrl.GetBuffer());

	return rv;

}

nsresult nsMsgSearchSession::AddUrl(const char *url)
{
  nsCString urlCString(url);
  m_urlQueue.AppendCString(urlCString);
  return NS_OK;
}

/* static */ void nsMsgSearchSession::TimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsMsgSearchSession *searchSession = (nsMsgSearchSession *) aClosure;
  PRBool done;
  searchSession->TimeSlice(&done);
  if (done)
    aTimer->Cancel();
}


nsresult nsMsgSearchSession::SearchWOUrls ()
{
	nsresult err = NS_OK;
  PRBool done;

	NS_NewTimer(getter_AddRefs(m_backgroundTimer));
  m_backgroundTimer->Init(TimerCallback, (void *) this, 0, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
  // ### start meteors?
  err = TimeSlice(&done);
#if 0
	if (!m_urlStruct)
		m_urlStruct = NET_CreateURLStruct ("search-libmsg:", NET_DONT_RELOAD);

	if (m_urlStruct)
	{
		// Set the internal_url flag so just in case someone else happens to have
		// a search-libmsg URL, it won't fire my code, and surely crash.
		m_urlStruct->internal_url = TRUE;

		// Initiate the asynchronous search
		int getUrlErr = m_pane->GetURL (m_urlStruct, FALSE);
		if (getUrlErr)
			err = (MSG_SearchError) -1; //###phil impedance mismatch
		else 
			if (!XP_STRNCMP(m_urlStruct->address, "news:", 5) || !XP_STRNCMP(m_urlStruct->address, "snews:", 6))
				BeginCylonMode();
	}
	else
		err = NS_ERROR_OUT_OF_MEMORY;
#endif
	return err;
}


NS_IMETHODIMP nsMsgSearchSession::AddResultElement (nsMsgResultElement *element)
{
	NS_ASSERTION(element, "no null elements");

	m_resultList.AppendElement (element);

	return NS_OK;
}


void nsMsgSearchSession::DestroyResultList ()
{
	nsMsgResultElement *result = nsnull;
	for (int i = 0; i < m_resultList.Count(); i++)
	{
		result = m_resultList.ElementAt(i);
//		NS_ASSERTION (result->IsValid(), "invalid search result");
		delete result;
	}
}


void nsMsgSearchSession::DestroyScopeList()
{
	nsMsgSearchScopeTerm *scope = NULL;
	for (int i = 0; i < m_scopeList.Count(); i++)
	{
		scope = m_scopeList.ElementAt(i);
//		NS_ASSERTION (scope->IsValid(), "invalid search scope");
		delete scope;
	}
}


void nsMsgSearchSession::DestroyTermList ()
{
	nsMsgSearchTerm *term = NULL;
	for (int i = 0; i < m_termList.Count(); i++)
	{
		term = m_termList.ElementAt(i);
//		NS_ASSERTION (term->IsValid(), "invalid search term");
		delete term;
	}
}

nsMsgSearchScopeTerm *nsMsgSearchSession::GetRunningScope()
{
	if (m_idxRunningScope < m_scopeList.Count())
		return m_scopeList.ElementAt (m_idxRunningScope); 
	return nsnull;

}

nsresult nsMsgSearchSession::TimeSlice (PRBool *aDone)
{
  // we only do serial for now.
  return TimeSliceSerial(aDone);
}

nsresult nsMsgSearchSession::TimeSliceSerial (PRBool *aDone)
{
	// This version of TimeSlice runs each scope term one at a time, and waits until one
	// scope term is finished before starting another one. When we're searching the local
	// disk, this is the fastest way to do it.

  NS_ENSURE_ARG(aDone);

	nsMsgSearchScopeTerm *scope = GetRunningScope();
	if (scope)
	{
		nsresult err = scope->TimeSlice (aDone);
		if (*aDone)
		{
			m_idxRunningScope++;
//			if (m_idxRunningScope < m_scopeList.Count())
//  			UpdateStatusBar (MK_MSG_SEARCH_STATUS);
		}
    *aDone = PR_FALSE;
		return NS_OK;
	}
	else
	{
    *aDone = PR_TRUE;
		return NS_OK;
	}
}

