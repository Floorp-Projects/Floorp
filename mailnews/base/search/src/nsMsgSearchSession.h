/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsMsgSearchSession_h___
#define nsMsgSearchSession_h___

#include "nscore.h"
#include "nsMsgSearchCore.h"
#include "nsIMsgSearchSession.h"
#include "nsIUrlListener.h"
#include "nsIMsgWindow.h"
#include "nsITimer.h"

class nsMsgSearchAdapter;

class nsMsgSearchSession : public nsIMsgSearchSession, public nsIUrlListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSEARCHSESSION
  NS_DECL_NSIURLLISTENER

  nsMsgSearchSession();
  virtual ~nsMsgSearchSession();

protected:

  nsCOMPtr <nsIMsgWindow> m_window;

	nsresult Initialize();
	nsresult TimeSlice (PRBool *aDone);
	nsMsgSearchAdapter *GetRunningAdapter ();
	nsMsgSearchScopeTerm *GetRunningScope();
	void			StopRunning();
	nsresult BeginSearching();
	nsresult BuildUrlQueue ();
	nsresult AddUrl(const char *url);
	nsresult SearchWOUrls ();
	nsresult GetNextUrl();

	nsMsgSearchScopeTermArray m_scopeList;
	nsMsgSearchTermArray m_termList;
	nsMsgResultArray m_resultList;

	void DestroyTermList ();
	void DestroyScopeList ();
	void DestroyResultList ();

  static void TimerCallback(nsITimer *aTimer, void *aClosure);
	// support for searching multiple scopes in serial
	nsresult TimeSliceSerial (PRBool *aDone);
	nsresult TimeSliceParallel ();

	nsMsgSearchAttribValue m_sortAttribute;
	PRBool m_descending;
	// support for searching multiple scopes in parallel
	PRBool m_parallel;
	PRInt32 m_idxRunningScope;
	nsMsgSearchScopeTermArray m_parallelScopes;
	nsMsgSearchType m_searchType;
	void *m_pSearchParam;
	PRBool m_handlingError;
  nsCStringArray m_urlQueue;
  nsCOMPtr <nsITimer> m_backgroundTimer;


};

#endif
