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

#include "msgCore.h"
#include "nsCOMPtr.h"
#include "nsIMsgHeaderParser.h"
#include "nsIAbDirectory.h"
#include "nsIAbAutoCompleteSession.h"


typedef struct
{
	char * userName;
	char * emailAddress;
	char * nickName;
} nsAbStubEntry;

#define MAX_ENTRIES 100


class nsAbAutoCompleteSearchString
{
public:
  nsAbAutoCompleteSearchString(const PRUnichar *uSearchString);
  virtual ~nsAbAutoCompleteSearchString();
  
  const PRUnichar*  mFullString;
  PRUint32          mFullStringLen;
  
  const PRUnichar*  mFirstPart;
  PRUint32          mFirstPartLen;
  
  const PRUnichar*  mSecondPart;
  PRUint32          mSecondPartLen;
};


class nsAbAutoCompleteSession : public nsIAbAutoCompleteSession
{
public:
	NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTOCOMPLETESESSION
    NS_DECL_NSIABAUTOCOMPLETESESSION

	nsAbAutoCompleteSession();
	virtual ~nsAbAutoCompleteSession(); 

    typedef enum
    {
        DEFAULT_MATCH           = 0,
        NICKNAME_EXACT_MATCH,
        NAME_EXACT_MATCH,
        EMAIL_EXACT_MATCH,
        NICKNAME_MATCH,
        NAME_MATCH,
        EMAIL_MATCH,
        LAST_MATCH_TYPE
    } MatchType;

protected:    
    void ResetMatchTypeConters();
    PRBool ItsADuplicate(PRUnichar* fullAddrStr, nsIAutoCompleteResults* results);
    void AddToResult(const PRUnichar* pNickNameStr, const PRUnichar* pDisplayNameStr, const PRUnichar* pFirstNameStr, const PRUnichar* pLastNameStr,
      const PRUnichar* pEmailStr, const PRUnichar* pNotes, PRBool bIsMailList, MatchType type, nsIAutoCompleteResults* results);
	  PRBool CheckEntry(nsAbAutoCompleteSearchString* searchStr, const PRUnichar* nickName,const PRUnichar* displayName, 
	    const PRUnichar* firstName, const PRUnichar* lastName, const PRUnichar* emailAddress, MatchType* matchType);
	  nsresult SearchCards(nsIAbDirectory* directory, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results);
    nsresult SearchDirectory(nsString& fileName, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results, PRBool searchSubDirectory = PR_FALSE);
    nsresult SearchPreviousResults(nsAbAutoCompleteSearchString *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteResults* results);

    nsCOMPtr<nsIMsgHeaderParser> mParser;
    nsString mDefaultDomain;
    PRUint32 mMatchTypeConters[LAST_MATCH_TYPE];
};


#define NS_ABAUTOCOMPLETEPARAM_IID_STR "a3093c00-1469-11d4-a449-e55aa307c5bc"
#define NS_ABAUTOCOMPLETEPARAM_IID \
  {0xa3093c00, 0x1469, 0x11d4, \
    { 0xa4, 0x49, 0xe5, 0x5a, 0xa3, 0x07, 0xc5, 0xbc }}

class nsAbAutoCompleteParam : public nsISupports
{
public:
	NS_DECL_ISUPPORTS
    //  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ABAUTOCOMPLETEPARAM_IID)
	
	nsAbAutoCompleteParam(const PRUnichar* nickName,
                          const PRUnichar* displayName,
                          const PRUnichar* firstName,
                          const PRUnichar* lastName,
                          const PRUnichar* emailAddress,
                          const PRUnichar* notes,
                          PRBool isMailList,
                          nsAbAutoCompleteSession::MatchType type)
	{
	  NS_INIT_REFCNT();
		mNickName = nsCRT::strdup(nickName ? nickName : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mDisplayName = nsCRT::strdup(displayName ? displayName : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mFirstName = nsCRT::strdup(firstName ? firstName : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mLastName = nsCRT::strdup(lastName ? lastName : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mEmailAddress = nsCRT::strdup(emailAddress ? emailAddress : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mNotes = nsCRT::strdup(notes ? notes : NS_STATIC_CAST(const PRUnichar*, NS_LITERAL_STRING("").get()));
		mIsMailList = isMailList;
		mType = type;
	}
	
	virtual ~nsAbAutoCompleteParam()
	{
    CRTFREEIF(mNickName);
    CRTFREEIF(mDisplayName);
    CRTFREEIF(mFirstName);
    CRTFREEIF(mLastName);
    CRTFREEIF(mEmailAddress);
    CRTFREEIF(mNotes);
	};
	
protected:
    PRUnichar* mNickName;
    PRUnichar* mDisplayName;
    PRUnichar* mFirstName;
    PRUnichar* mLastName;
    PRUnichar* mEmailAddress;
    PRUnichar* mNotes;
    PRBool mIsMailList;
    nsAbAutoCompleteSession::MatchType  mType;

public:
    friend class nsAbAutoCompleteSession;
};

#endif /* nsAbAutoCompleteSession_h___ */


