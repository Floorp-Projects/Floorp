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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsAbAutoCompleteSession_h___
#define nsAbAutoCompleteSession_h___

#include "nsIAutoCompleteSession.h"
#include "nsIAutoCompleteListener.h"
#include "nsCOMPtr.h"
#include "msgCore.h"


typedef struct
{
	char * userName;
	char * emailAddress;
} nsAbStubEntry;

#define MAX_ENTRIES 100

class nsAbAutoCompleteSession : public nsIAutoCompleteSession
{
public:
	NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTOCOMPLETESESSION
	nsAbAutoCompleteSession();
	virtual ~nsAbAutoCompleteSession();

protected:
  nsresult InitializeTable();
  nsresult PopulateTableWithAB(nsIEnumerator * aABCards); // enumerates through the cards and adds them to the table
  
  nsCOMPtr<nsIAutoCompleteListener> m_resultListener;
  PRBool	m_tableInitialized;
  nsAbStubEntry m_searchNameCompletionEntryTable[MAX_ENTRIES];
  PRInt32   m_numEntries;
  nsString	m_domain;
};

// factory method
extern nsresult NS_NewAbAutoCompleteSession(const nsIID &aIID, void ** aInstancePtrResult);

#endif /* nsAbAutoCompleteSession_h___ */


