/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsAbAutoCompleteSession_h___
#define nsAbAutoCompleteSession_h___

#include "msgCore.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIMsgHeaderParser.h"
#include "nsIAbDirectory.h"
#include "nsIAbAutoCompleteSession.h"

class nsIPref;

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
    void AddToResult(const PRUnichar* pNickNameStr, 
                     const PRUnichar* pDisplayNameStr, 
                     const PRUnichar* pFirstNameStr,
                     const PRUnichar* pLastNameStr, 
                     const PRUnichar* pEmailStr, const PRUnichar* pNotes,
                     const PRUnichar* pDirName, PRBool bIsMailList, 
                     MatchType type, nsIAutoCompleteResults* results);
    PRBool CheckEntry(nsAbAutoCompleteSearchString* searchStr, const PRUnichar* nickName,const PRUnichar* displayName, 
      const PRUnichar* firstName, const PRUnichar* lastName, const PRUnichar* emailAddress, MatchType* matchType);
        
    nsCOMPtr<nsIMsgHeaderParser> mParser;
    nsString mDefaultDomain;
    PRUint32 mMatchTypeConters[LAST_MATCH_TYPE];
    PRUint32 mDefaultDomainMatchTypeCounters[LAST_MATCH_TYPE];

    // how to process the comment column, if at all.  this value
    // comes from "mail.autoComplete.commentColumn", or, if that
    // doesn't exist, defaults to 0
    //
    // 0 = none
    // 1 = name of addressbook this card came from
    // 2 = other per-addressbook format (currrently unused here)
    //
    PRInt32 mAutoCompleteCommentColumn;

private:
    #define MAX_NUMBER_OF_EMAIL_ADDRESSES     2

    nsresult SearchCards(nsIAbDirectory* directory, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results);
    nsresult SearchDirectory(const nsACString& aURI, nsAbAutoCompleteSearchString* searchStr, PRBool searchSubDirectory, nsIAutoCompleteResults* results);
    nsresult SearchPreviousResults(nsAbAutoCompleteSearchString *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteResults* results);

    nsresult SearchReplicatedLDAPDirectories(nsIPref *aPrefs, nsAbAutoCompleteSearchString* searchStr, PRBool searchSubDirectory, nsIAutoCompleteResults* results);
    nsresult NeedToSearchReplicatedLDAPDirectories(nsIPref *aPrefs, PRBool *aNeedToSearch);
    nsresult NeedToSearchLocalDirectories(nsIPref *aPrefs, PRBool *aNeedToSearch);
};


#define NS_ABAUTOCOMPLETEPARAM_IID_STR "a3093c00-1469-11d4-a449-e55aa307c5bc"
#define NS_ABAUTOCOMPLETEPARAM_IID \
  {0xa3093c00, 0x1469, 0x11d4, \
    { 0xa4, 0x49, 0xe5, 0x5a, 0xa3, 0x07, 0xc5, 0xbc }}

class nsAbAutoCompleteParam : public nsISupports
{
public:
  NS_DECL_ISUPPORTS
  
  nsAbAutoCompleteParam(const PRUnichar* nickName,
                          const PRUnichar* displayName,
                          const PRUnichar* firstName,
                          const PRUnichar* lastName,
                          const PRUnichar* emailAddress,
                          const PRUnichar* notes,
                          const PRUnichar* dirName,
                          PRBool isMailList, 
                          nsAbAutoCompleteSession::MatchType type)
  {
    const PRUnichar *empty = EmptyString().get();

    mNickName = nsCRT::strdup(nickName ? nickName : empty);
    mDisplayName = nsCRT::strdup(displayName ? displayName : empty);
    mFirstName = nsCRT::strdup(firstName ? firstName : empty);
    mLastName = nsCRT::strdup(lastName ? lastName : empty);
    mEmailAddress = nsCRT::strdup(emailAddress ? emailAddress : empty);
    mNotes = nsCRT::strdup(notes ? notes : empty);
    mDirName = nsCRT::strdup(dirName ? dirName : empty);
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
    CRTFREEIF(mDirName);
  };
  
protected:
    PRUnichar* mNickName;
    PRUnichar* mDisplayName;
    PRUnichar* mFirstName;
    PRUnichar* mLastName;
    PRUnichar* mEmailAddress;
    PRUnichar* mNotes;
    PRUnichar* mDirName;
    PRBool mIsMailList;
    nsAbAutoCompleteSession::MatchType  mType;

public:
    friend class nsAbAutoCompleteSession;
};

#endif /* nsAbAutoCompleteSession_h___ */


