/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsMsgSearchSession_h___
#define nsMsgSearchSession_h___

#include "nscore.h"
#include "nsMsgSearchCore.h"
#include "nsIMsgSearchSession.h"
#include "nsIUrlListener.h"
#include "nsIFolderListener.h"
#include "nsIMsgWindow.h"
#include "nsITimer.h"
#include "nsMsgSearchArray.h"
#include "nsISupportsArray.h"
#include "nsWeakReference.h"

class nsMsgSearchAdapter;

class nsMsgSearchSession : public nsIMsgSearchSession, public nsIUrlListener, public nsIFolderListener, public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSEARCHSESSION
  NS_DECL_NSIURLLISTENER
  NS_DECL_NSIFOLDERLISTENER

  nsMsgSearchSession();
  virtual ~nsMsgSearchSession();

protected:

  nsCOMPtr <nsIMsgWindow> m_window;

	nsresult Initialize();
  nsresult StartTimer();
	nsresult TimeSlice (PRBool *aDone);
	nsMsgSearchScopeTerm *GetRunningScope();
	void			StopRunning();
	nsresult BeginSearching();
	nsresult BuildUrlQueue ();
	nsresult AddUrl(const char *url);
	nsresult SearchWOUrls ();
	nsresult GetNextUrl();
  nsresult NotifyListenersDone(nsresult status);

	nsMsgSearchScopeTermArray m_scopeList;
	nsCOMPtr <nsISupportsArray> m_termList;
    nsCOMPtr <nsISupportsArray> m_listenerList;
    nsCOMPtr <nsISupportsArray> m_folderListenerList;

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
  PRBool m_searchPaused;


};

#endif
