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
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#include "msgCore.h"
#include "nsMsgSearchCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchSession.h"
#include "nsMsgResultElement.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsXPIDLString.h"
#include "nsIMsgSearchNotify.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsMsgLocalSearch.h"

NS_IMPL_ISUPPORTS3(nsMsgSearchSession, nsIMsgSearchSession, nsIUrlListener, nsISupportsWeakReference)

nsMsgSearchSession::nsMsgSearchSession()
{
	m_sortAttribute = nsMsgSearchAttrib::Sender;
	m_descending = PR_FALSE;
	m_idxRunningScope = 0; 
	m_parallel = PR_FALSE;
//	m_calledStartingUpdate = PR_FALSE;
	m_handlingError = PR_FALSE;
	m_pSearchParam = nsnull;
  m_searchPaused = PR_FALSE;
  NS_NewISupportsArray(getter_AddRefs(m_termList));

}

nsMsgSearchSession::~nsMsgSearchSession()
{
	DestroyResultList ();
	DestroyScopeList ();
	DestroyTermList ();

  PR_Free (m_pSearchParam);
}

/* [noscript] void AddSearchTerm (in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value, in boolean BooleanAND, in string arbitraryHeader); */
NS_IMETHODIMP
nsMsgSearchSession::AddSearchTerm(nsMsgSearchAttribValue attrib,
                                  nsMsgSearchOpValue op,
                                  nsIMsgSearchValue * value,
                                  PRBool BooleanANDp,
                                  const char *arbitraryHeader)
{
    // stupid gcc
    nsMsgSearchBooleanOperator boolOp;
    if (BooleanANDp)
        boolOp = (nsMsgSearchBooleanOperator)nsMsgSearchBooleanOp::BooleanAND;
    else
        boolOp = (nsMsgSearchBooleanOperator)nsMsgSearchBooleanOp::BooleanOR;
	nsMsgSearchTerm *pTerm = new nsMsgSearchTerm (attrib, op, value,
                                                  boolOp, arbitraryHeader);
	if (nsnull == pTerm)
		return NS_ERROR_OUT_OF_MEMORY;
	m_termList->AppendElement (pTerm);
	return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchSession::AppendTerm(nsIMsgSearchTerm *aTerm)
{
    NS_ENSURE_ARG_POINTER(aTerm);
    NS_ENSURE_TRUE(m_termList, NS_ERROR_NOT_INITIALIZED);
    return m_termList->AppendElement(aTerm);
}

NS_IMETHODIMP
nsMsgSearchSession::GetSearchTerms(nsISupportsArray **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = m_termList;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchSession::CreateTerm(nsIMsgSearchTerm **aResult)
{
    nsMsgSearchTerm *term = new nsMsgSearchTerm;
    NS_ENSURE_TRUE(term, NS_ERROR_OUT_OF_MEMORY);
    
    *aResult = NS_STATIC_CAST(nsIMsgSearchTerm*,term);
    NS_ADDREF(*aResult);
    return NS_OK;
}

/* void RegisterListener (in nsIMsgSearchNotify listener); */
NS_IMETHODIMP nsMsgSearchSession::RegisterListener(nsIMsgSearchNotify *listener)
{
  nsresult ret = NS_OK;
  if (!m_listenerList)
    ret = NS_NewISupportsArray(getter_AddRefs(m_listenerList));

  if (NS_SUCCEEDED(ret) && m_listenerList)
    m_listenerList->AppendElement(listener);
  return ret;
}

/* void UnregisterListener (in nsIMsgSearchNotify listener); */
NS_IMETHODIMP nsMsgSearchSession::UnregisterListener(nsIMsgSearchNotify *listener)
{
  if (m_listenerList)
    m_listenerList->RemoveElement(listener);
  return NS_OK;
}

/* readonly attribute long numSearchTerms; */
NS_IMETHODIMP nsMsgSearchSession::GetNumSearchTerms(PRUint32 *aNumSearchTerms)
{
  NS_ENSURE_ARG(aNumSearchTerms);
  return m_termList->Count(aNumSearchTerms);
}

/* [noscript] void GetNthSearchTerm (in long whichTerm, in nsMsgSearchAttribute attrib, in nsMsgSearchOperator op, in nsMsgSearchValue value); */
NS_IMETHODIMP
nsMsgSearchSession::GetNthSearchTerm(PRInt32 whichTerm,
                                     nsMsgSearchAttribValue attrib,
                                     nsMsgSearchOpValue op,
                                     nsIMsgSearchValue * value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* long CountSearchScopes (); */
NS_IMETHODIMP nsMsgSearchSession::CountSearchScopes(PRInt32 *_retval)
{
  NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

  /* void GetNthSearchScope (in long which, out nsMsgSearchScope scopeId, out nsIMsgFolder folder); */
NS_IMETHODIMP
nsMsgSearchSession::GetNthSearchScope(PRInt32 which,
                                      nsMsgSearchScopeValue *scopeId,
                                      nsIMsgFolder **folder)
{
  // argh, does this do an addref?
	nsMsgSearchScopeTerm *scopeTerm = (nsMsgSearchScopeTerm *) m_scopeList.SafeElementAt(which);
    if (!scopeTerm) return NS_ERROR_INVALID_ARG;
	*scopeId = scopeTerm->m_attribute;
	*folder = scopeTerm->m_folder;
  NS_IF_ADDREF(*folder);
  return NS_OK;
}

/* void AddScopeTerm (in nsMsgSearchScopeValue scope, in nsIMsgFolder folder); */
NS_IMETHODIMP
nsMsgSearchSession::AddScopeTerm(nsMsgSearchScopeValue scope,
                                 nsIMsgFolder *folder)
{
  if (scope != nsMsgSearchScope::allSearchableGroups)
  {
    NS_ASSERTION(folder, "need folder if not searching all groups");
    if (!folder)
      return NS_ERROR_NULL_POINTER;
  }

  nsMsgSearchScopeTerm *pScopeTerm = new nsMsgSearchScopeTerm(this, scope, folder);
  if (!pScopeTerm)
    return NS_ERROR_OUT_OF_MEMORY;

  m_scopeList.AppendElement(pScopeTerm);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchSession::AddDirectoryScopeTerm(nsMsgSearchScopeValue scope)
{
	nsMsgSearchScopeTerm *pScopeTerm = new nsMsgSearchScopeTerm(this, scope, nsnull);
  if (!pScopeTerm)
    return NS_ERROR_OUT_OF_MEMORY;

  m_scopeList.AppendElement(pScopeTerm);
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchSession::ClearScopes()
{
    DestroyScopeList();
    return NS_OK;
}

/* [noscript] boolean ScopeUsesCustomHeaders (in nsMsgSearchScope scope, in voidStar selection, in boolean forFilters); */
NS_IMETHODIMP
nsMsgSearchSession::ScopeUsesCustomHeaders(nsMsgSearchScopeValue scope,
                                           void * selection,
                                           PRBool forFilters,
                                           PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsStringAttribute (in nsMsgSearchAttribute attrib); */
NS_IMETHODIMP
nsMsgSearchSession::IsStringAttribute(nsMsgSearchAttribValue attrib,
                                      PRBool *_retval)
{
  NS_ENSURE_ARG(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddAllScopes (in nsMsgSearchScope attrib); */
NS_IMETHODIMP
nsMsgSearchSession::AddAllScopes(nsMsgSearchScopeValue attrib)
{
  // don't think this is needed.
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Search (); */
NS_IMETHODIMP nsMsgSearchSession::Search(nsIMsgWindow *aWindow)
{
	nsresult err = Initialize ();
    NS_ENSURE_SUCCESS(err,err);
    if (m_listenerList) {
        PRUint32 count;
        m_listenerList->Count(&count);
        for (PRUint32 i=0; i<count;i++) {
            nsCOMPtr<nsIMsgSearchNotify> listener;
            m_listenerList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchNotify),
                                           (void **)getter_AddRefs(listener));
            if (listener)
                listener->OnNewSearch();
        }
    }
  m_window = aWindow;
	if (NS_SUCCEEDED(err))
		err = BeginSearching ();
	return err;
}

/* void InterruptSearch (); */
NS_IMETHODIMP nsMsgSearchSession::InterruptSearch()
{
  if (m_window)
  {
    EnableFolderNotifications(PR_TRUE);
    while (m_idxRunningScope < m_scopeList.Count())
    {
      ReleaseFolderDBRef();
      m_idxRunningScope++;
    }
    //m_idxRunningScope = m_scopeList.Count() so it will make us not run another url
    m_window->StopUrls();
  }
  if (m_backgroundTimer)
  {
    m_backgroundTimer->Cancel();
    NotifyListenersDone(NS_OK); // ### is there a cancelled status?

    m_backgroundTimer = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchSession::PauseSearch()
{
  if (m_backgroundTimer)
  {
    m_backgroundTimer->Cancel();
    m_searchPaused = PR_TRUE;
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgSearchSession::ResumeSearch()
{
  if (m_searchPaused)
  {
    m_searchPaused = PR_FALSE;
    return StartTimer();
  }
  else
    return NS_ERROR_FAILURE;
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

NS_IMETHODIMP nsMsgSearchSession::SetWindow(nsIMsgWindow *aWindow)
{
  m_window = aWindow;
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchSession::GetWindow(nsIMsgWindow **aWindowPtr)
{
  NS_ENSURE_ARG(aWindowPtr);
  *aWindowPtr = m_window;
  NS_IF_ADDREF(*aWindowPtr);
  return NS_OK;
}

/* void OnStartRunningUrl (in nsIURI url); */
NS_IMETHODIMP nsMsgSearchSession::OnStartRunningUrl(nsIURI *url)
{
    return NS_OK;
}

/* void OnStopRunningUrl (in nsIURI url, in nsresult aExitCode); */
NS_IMETHODIMP nsMsgSearchSession::OnStopRunningUrl(nsIURI *url, nsresult aExitCode)
{
  nsCOMPtr <nsIMsgSearchAdapter> runningAdapter;

  nsresult rv = GetRunningAdapter (getter_AddRefs(runningAdapter));
  // tell the current adapter that the current url has run.
  if (NS_SUCCEEDED(rv) && runningAdapter)
  {
    runningAdapter->CurrentUrlDone(aExitCode);
    EnableFolderNotifications(PR_TRUE);
    ReleaseFolderDBRef();
  }
  m_idxRunningScope++;
  if (m_idxRunningScope < m_scopeList.Count())
    GetNextUrl();
  else
    NotifyListenersDone(aExitCode);
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

  PRUint32 numTerms;
  m_termList->Count(&numTerms);
	// Ensure that the FE has added scopes and terms to this search
	NS_ASSERTION(numTerms > 0, "no terms to search!");
	if (numTerms == 0)
		return NS_MSG_ERROR_NO_SEARCH_VALUES;

	// if we don't have any search scopes to search, return that code. 
	if (m_scopeList.Count() == 0)
		return NS_MSG_ERROR_INVALID_SEARCH_SCOPE;

  m_urlQueue.Clear(); // clear out old urls, if any.
	m_idxRunningScope = 0; 

	// If this term list (loosely specified here by the first term) should be
	// scheduled in parallel, build up a list of scopes to do the round-robin scheduling
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
  nsresult err = NS_OK;
  
  // Here's a sloppy way to start the URL, but I don't really have time to
  // unify the scheduling mechanisms. If the first scope is a newsgroup, and
  // it's not Dredd-capable, we build the URL queue. All other searches can be
  // done with one URL
  
  if (m_window)
    m_window->SetStopped(PR_FALSE);
  nsMsgSearchScopeTerm *scope = m_scopeList.ElementAt(0);
  if (scope->m_attribute == nsMsgSearchScope::news /* && !scope->m_folder->KnowsSearchNntpExtension() */ && scope->m_searchServer)
    err = BuildUrlQueue ();
  else if (scope->m_attribute == nsMsgSearchScope::onlineMail)
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
    if (adapter)
    {
		  adapter->GetEncoding(getter_Copies(url));
		  AddUrl (url);
    }
	}

	if (i > 0)
		GetNextUrl();

	return NS_OK;
}


nsresult nsMsgSearchSession::GetNextUrl()
{
  nsCString nextUrl;
  nsCOMPtr <nsIMsgMessageService> msgService;


  PRBool stopped = PR_FALSE;

  if (m_window)
    m_window->GetStopped(&stopped);
  if (stopped)
    return NS_OK;

  m_urlQueue.CStringAt(m_idxRunningScope, nextUrl);
  nsMsgSearchScopeTerm *currentTerm = GetRunningScope();
  EnableFolderNotifications(PR_FALSE);
  nsCOMPtr <nsIMsgFolder> folder = currentTerm->m_folder;
  if (folder)
  {
    nsXPIDLCString folderUri;
    folder->GetURI(getter_Copies(folderUri));
    nsresult rv = GetMessageServiceFromURI(folderUri.get(), getter_AddRefs(msgService));

    if (NS_SUCCEEDED(rv) && msgService && currentTerm)
      msgService->Search(this, m_window, currentTerm->m_folder, nextUrl.get());

	  return rv;
  }
  return NS_OK;
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
  PRBool stopped = PR_FALSE;

  searchSession->TimeSlice(&done);
  if (searchSession->m_window)
    searchSession->m_window->GetStopped(&stopped);

  if (done || stopped)
  {
    aTimer->Cancel();
    searchSession->m_backgroundTimer = nsnull;
    searchSession->NotifyListenersDone(NS_OK);
  }
}

nsresult nsMsgSearchSession::StartTimer()
{
  nsresult err;
  PRBool done;

  m_backgroundTimer = do_CreateInstance("@mozilla.org/timer;1", &err);
  m_backgroundTimer->InitWithFuncCallback(TimerCallback, (void *) this, 0, 
                                          nsITimer::TYPE_REPEATING_SLACK);
  // ### start meteors?
  return TimeSlice(&done);
}

nsresult nsMsgSearchSession::SearchWOUrls ()
{
  EnableFolderNotifications(PR_FALSE);
  return StartTimer();
}

NS_IMETHODIMP nsMsgSearchSession::GetRunningAdapter (nsIMsgSearchAdapter **aSearchAdapter)
{
  NS_ENSURE_ARG(aSearchAdapter);
	nsMsgSearchScopeTerm *scope = GetRunningScope();
	if (scope)
  {
		*aSearchAdapter = scope->m_adapter;
    NS_ADDREF(*aSearchAdapter);
    return NS_OK;
  }
	*aSearchAdapter = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchSession::AddSearchHit(nsIMsgDBHdr *header, nsIMsgFolder *folder)
{
  if (m_listenerList)
  {
    PRUint32 count;
    m_listenerList->Count(&count);
    for (PRUint32 i = 0; i < count; i++)
    {
      nsCOMPtr<nsIMsgSearchNotify> pListener;
      m_listenerList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchNotify),
                               (void **)getter_AddRefs(pListener));
      if (pListener)
        pListener->OnSearchHit(header, folder);

    }
  }
	return NS_OK;
}

nsresult nsMsgSearchSession::NotifyListenersDone(nsresult status)
{
  if (m_listenerList)
  {
    PRUint32 count;
    m_listenerList->Count(&count);
    for (PRUint32 i = 0; i < count; i++)
    {
      nsCOMPtr<nsIMsgSearchNotify> pListener;
      m_listenerList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchNotify),
                               (void **)getter_AddRefs(pListener));
      if (pListener)
        pListener->OnSearchDone(status);

    }
  }
	return NS_OK;
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
    m_resultList.Clear();
}


void nsMsgSearchSession::DestroyScopeList()
{
	nsMsgSearchScopeTerm *scope = NULL;
    PRInt32 count = m_scopeList.Count();
    
	for (PRInt32 i = count-1; i >= 0; i--)
	{
		scope = m_scopeList.ElementAt(i);
//		NS_ASSERTION (scope->IsValid(), "invalid search scope");
		delete scope;
	}
    m_scopeList.Clear();
}


void nsMsgSearchSession::DestroyTermList ()
{
    m_termList->Clear();
}

nsMsgSearchScopeTerm *nsMsgSearchSession::GetRunningScope()
{
    return (nsMsgSearchScopeTerm *) m_scopeList.SafeElementAt(m_idxRunningScope); 
}

nsresult nsMsgSearchSession::TimeSlice (PRBool *aDone)
{
  // we only do serial for now.
  return TimeSliceSerial(aDone);
}

void nsMsgSearchSession::ReleaseFolderDBRef()  
{
  nsMsgSearchScopeTerm *scope = GetRunningScope();
  if (scope)
  {
    PRBool isOpen =PR_FALSE;
    PRUint32 flags;
    nsCOMPtr <nsIMsgFolder> folder;
    scope->GetFolder(getter_AddRefs(folder));
    nsCOMPtr <nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID);
    if (mailSession && folder)
    {
      mailSession->IsFolderOpenInWindow(folder, &isOpen);
      folder->GetFlags(&flags);

      /*we don't null out the db reference for inbox because inbox is like the "main" folder
       and performance outweighs footprint */
      if (!isOpen && !(MSG_FOLDER_FLAG_INBOX & flags)) 
        folder->SetMsgDatabase(nsnull);
    }
  }
}
nsresult nsMsgSearchSession::TimeSliceSerial (PRBool *aDone)
{
	// This version of TimeSlice runs each scope term one at a time, and waits until one
	// scope term is finished before starting another one. When we're searching the local
	// disk, this is the fastest way to do it.

  NS_ENSURE_ARG(aDone);
  nsresult rv = NS_OK;
  nsMsgSearchScopeTerm *scope = GetRunningScope();
  if (scope)
  {
    rv = scope->TimeSlice (aDone);
    if (NS_FAILED(rv))
      *aDone = PR_TRUE;
    if (*aDone || NS_FAILED(rv))
    {
      EnableFolderNotifications(PR_TRUE);
      ReleaseFolderDBRef();
      m_idxRunningScope++;
      EnableFolderNotifications(PR_FALSE);

      //			if (m_idxRunningScope < m_scopeList.Count())
      //  			UpdateStatusBar (MK_MSG_SEARCH_STATUS);
    }
    *aDone = PR_FALSE;
    return rv;
  }
  else
  {
    *aDone = PR_TRUE;
    return NS_OK;
  }
}

void 
nsMsgSearchSession::EnableFolderNotifications(PRBool aEnable)
{
  nsMsgSearchScopeTerm *scope = GetRunningScope();
  if (scope)
  {
    nsCOMPtr<nsIMsgFolder> folder;
    scope->GetFolder(getter_AddRefs(folder));
    if (folder)  //enable msg count notifications
      folder->EnableNotifications(nsIMsgFolder::allMessageCountNotifications, aEnable, PR_FALSE);
  }
}

//this method is used for adding new hdrs to quick search view
NS_IMETHODIMP
nsMsgSearchSession::MatchHdr(nsIMsgDBHdr *aMsgHdr, nsIMsgDatabase *aDatabase, PRBool *aResult)
{
  nsMsgSearchScopeTerm *scope = (nsMsgSearchScopeTerm *)m_scopeList.SafeElementAt(0);
  if (scope)
  {
    if (!scope->m_adapter)
      scope->InitializeAdapter(m_termList);
    if (scope->m_adapter)
    {  
      nsXPIDLString nullCharset, folderCharset;
      scope->m_adapter->GetSearchCharsets(getter_Copies(nullCharset), getter_Copies(folderCharset));
      NS_ConvertUCS2toUTF8 charset(folderCharset.get());
      nsMsgSearchOfflineMail::MatchTermsForSearch(aMsgHdr, m_termList, charset.get(), scope, aDatabase, aResult);
    }
  }
  return NS_OK;
}

