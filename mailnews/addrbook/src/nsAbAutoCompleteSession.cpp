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

#include "msgCore.h"
#include "nsAbAutoCompleteSession.h"
#include "nsString.h"
#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIAbDirectory.h"
#include "nsIAbCard.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsMsgBaseCID.h"
#include "nsMsgI18N.h"
#include "nsIMsgIdentity.h"
#include "prmem.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIPref.h"

NS_IMPL_ISUPPORTS2(nsAbAutoCompleteSession, nsIAbAutoCompleteSession, nsIAutoCompleteSession)

nsAbAutoCompleteSession::nsAbAutoCompleteSession()
{
    mParser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);
}


nsAbAutoCompleteSession::~nsAbAutoCompleteSession()
{
}


void nsAbAutoCompleteSession::ResetMatchTypeConters()
{
    PRInt32 i;
    for (i = 0; i < LAST_MATCH_TYPE; mMatchTypeConters[i++] = 0)
      mDefaultDomainMatchTypeCounters[i] = 0;
}

PRBool nsAbAutoCompleteSession::ItsADuplicate(PRUnichar* fullAddrStr, nsIAutoCompleteResults* results)
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> array;
    rv = results->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIEnumerator> enumerator;
        rv = array->Enumerate(getter_AddRefs(enumerator));
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIAutoCompleteItem> resultItem;
            nsAutoString valueStr;

            for (rv = enumerator->First(); NS_SUCCEEDED(rv); rv = enumerator->Next())
            {
                rv = enumerator->CurrentItem(getter_AddRefs(item));
                if (NS_SUCCEEDED(rv) && item)
                {
                    resultItem = do_QueryInterface(item, &rv);
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = resultItem->GetValue(valueStr);
                        if (NS_SUCCEEDED(rv) && !valueStr.IsEmpty())
                        {
                          if (nsDependentString(fullAddrStr).Equals(valueStr,
                                                                    nsCaseInsensitiveStringComparator()))
                            return PR_TRUE;
                        }
                    }
                }
            }
        }
    }
    
    return PR_FALSE;
}

void 
nsAbAutoCompleteSession::AddToResult(const PRUnichar* pNickNameStr,
                                     const PRUnichar* pDisplayNameStr,
                                     const PRUnichar* pFirstNameStr,
                                     const PRUnichar* pLastNameStr,
                                     const PRUnichar* pEmailStr, 
                                     const PRUnichar* pNotesStr, 
                                     const PRUnichar* pDirName,
                                     PRBool bIsMailList, MatchType type,
                                     nsIAutoCompleteResults* results)
{
  nsresult rv;
  PRUnichar* fullAddrStr = nsnull;

  if (type == DEFAULT_MATCH)
  {
    if (mDefaultDomain[0] == 0)
      return;

    nsAutoString aStr(pDisplayNameStr);
    aStr.Append(PRUnichar('@'));
    aStr += mDefaultDomain;
    fullAddrStr = ToNewUnicode(aStr);
  }
  else
  {
    if (mParser)
    {
      nsXPIDLCString fullAddress;
      nsXPIDLCString utf8Email;
      if (bIsMailList)
      {
        if (pNotesStr && pNotesStr[0] != 0)
          utf8Email.Adopt(ToNewUTF8String(nsDependentString(pNotesStr)));
        else
          utf8Email.Adopt(ToNewUTF8String(nsDependentString(pDisplayNameStr)));
      }
      else
        utf8Email.Adopt(ToNewUTF8String(nsDependentString(pEmailStr)));

      mParser->MakeFullAddress(nsnull, NS_ConvertUCS2toUTF8(pDisplayNameStr).get(),
                               utf8Email, getter_Copies(fullAddress));
      if (!fullAddress.IsEmpty())
      {
        /* We need to convert back the result from UTF-8 to Unicode */
        fullAddrStr = nsCRT::strdup(NS_ConvertUTF8toUCS2(fullAddress.get()).get());
      }
    }
  
    if (!fullAddrStr)
    {
      //oops, parser problem! I will try to do my best...
      const PRUnichar * pStr = nsnull;
      if (bIsMailList)
      {
        if (pNotesStr && pNotesStr[0] != 0)
          pStr = pNotesStr;
        else
          pStr = pDisplayNameStr;
      }
      else
        pStr = pEmailStr;
      // check this so we do not get a bogus entry "someName <>"
      if (pStr && pStr[0] != 0) {
        nsAutoString aStr(pDisplayNameStr);
        aStr.AppendLiteral(" <");
        aStr += pStr;
        aStr.AppendLiteral(">");
        fullAddrStr = ToNewUnicode(aStr);
      }
      else
        fullAddrStr = nsnull;
    }
  }
    
  if (fullAddrStr && ! ItsADuplicate(fullAddrStr, results))
  {    
    nsCOMPtr<nsIAutoCompleteItem> newItem = do_CreateInstance(NS_AUTOCOMPLETEITEM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsAbAutoCompleteParam *param = new nsAbAutoCompleteParam(pNickNameStr, pDisplayNameStr, pFirstNameStr, pLastNameStr, pEmailStr, pNotesStr, pDirName, bIsMailList, type);
      NS_IF_ADDREF(param);
      newItem->SetParam(param);
      NS_IF_RELEASE(param);

      // how to process the comment column, if at all.  this value
      // comes from "mail.autoComplete.commentColumn", or, if that
      // doesn't exist, defaults to 0
      //
      // 0 = none
      // 1 = name of addressbook this card came from
      // 2 = other per-addressbook format (currrently unused here)
      //
      if (mAutoCompleteCommentColumn == 1) {
        rv = newItem->SetComment(pDirName);
        if (NS_FAILED(rv)) {
          NS_WARNING("nsAbAutoCompleteSession::AddToResult():"
                     " newItem->SetComment() failed\n");
        }
      }

      // if this isn't a default match, set the class name so we can style 
      // this cell with the local addressbook icon (or whatever)
      //
      rv = newItem->SetClassName(type == DEFAULT_MATCH ? "default-match" :
                                 "local-abook");
      if (NS_FAILED(rv)) {
        NS_WARNING("nsAbAutoCompleteSession::AddToResult():"
                   " newItem->SetClassName() failed\n");
      }

      newItem->SetValue(nsDependentString(fullAddrStr));
      nsCOMPtr<nsISupportsArray> array;
      rv = results->GetItems(getter_AddRefs(array));
      if (NS_SUCCEEDED(rv))
      {
        PRInt32 index = 0;
        PRInt32 i;
        // break off at the first of our type....
        for (i = 0; i < type; index += mMatchTypeConters[i++])
          ;        
        
        // use domain matches as the distinguisher amongst matches of the same type.
        PRInt32 insertPosition = index + mMatchTypeConters[i];
        
        if (type != DEFAULT_MATCH && !bIsMailList)
        {
          nsAutoString emailaddr(pEmailStr);
          if (FindInReadable(mDefaultDomain, emailaddr,
                             nsCaseInsensitiveStringComparator()))
          {
            // okay the match contains the default domain, we want to insert it
            // AFTER any exisiting matches of the same type which also have a domain
            // match....            
            insertPosition = index + mDefaultDomainMatchTypeCounters[type];
            mDefaultDomainMatchTypeCounters[type]++;           
          }
        }

        rv = array->InsertElementAt(newItem, insertPosition);
        if (NS_SUCCEEDED(rv))
          mMatchTypeConters[type] ++;
      }
    }
  }    
  PR_Free(fullAddrStr);
}

static PRBool CommonPrefix(const PRUnichar *aString, const PRUnichar *aSubstr, PRInt32 aSubstrLen)
{
  if (!aSubstrLen || (nsCRT::strlen(aString) < NS_STATIC_CAST(PRUint32, aSubstrLen)))
    return PR_FALSE;

  return (Substring(aString,
                    aString+aSubstrLen).Equals(Substring(aSubstr, aSubstr+aSubstrLen),
                                               nsCaseInsensitiveStringComparator()));
}


PRBool
nsAbAutoCompleteSession::CheckEntry(nsAbAutoCompleteSearchString* searchStr,
                                    const PRUnichar* nickName,
                                    const PRUnichar* displayName,
                                    const PRUnichar* firstName,
                                    const PRUnichar* lastName,
                                    const PRUnichar* emailAddress,
                                    MatchType* matchType)
{
  const PRUnichar * fullString;
  PRUint32 fullStringLen;
  
  if (searchStr->mFirstPartLen > 0 && searchStr->mSecondPartLen == 0)
  {
    fullString = searchStr->mFirstPart;
    fullStringLen = searchStr->mFirstPartLen;
  }
  else
  {
    fullString = searchStr->mFullString;
    fullStringLen = searchStr->mFullStringLen;
  }

  nsDependentString fullStringStr(fullString, fullStringLen);
  
  // First check for a Nickname exact match
  if (nickName &&
      fullStringStr.Equals(nsDependentString(nickName),
                           nsCaseInsensitiveStringComparator()))
  {
    *matchType = NICKNAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a display Name exact match
  if (displayName &&
      fullStringStr.Equals(nsDependentString(displayName),
                           nsCaseInsensitiveStringComparator()))
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a fisrt Name exact match
  if (firstName &&
      fullStringStr.Equals(nsDependentString(firstName),
                           nsCaseInsensitiveStringComparator()))
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a last Name exact match
  if (lastName &&
      fullStringStr.Equals(nsDependentString(lastName),
                           nsCaseInsensitiveStringComparator()))
  {
    *matchType = NAME_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a Email exact match
  if (emailAddress &&
      fullStringStr.Equals(nsDependentString(emailAddress),
                           nsCaseInsensitiveStringComparator()))
  {
    *matchType = EMAIL_EXACT_MATCH;
    return PR_TRUE;
  }

  // Then check for a NickName partial match
  if (nickName && CommonPrefix(nickName, fullString, fullStringLen))
  {
    *matchType = NICKNAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a display Name partial match
  if (displayName && CommonPrefix(displayName, fullString, fullStringLen))
  {
    *matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a first Name partial match
  if (firstName && CommonPrefix(firstName, fullString, fullStringLen))
  {
    *matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a last Name partial match
  if (lastName && CommonPrefix(lastName, fullString, fullStringLen))
  {
    *matchType = NAME_MATCH;
    return PR_TRUE;
  }

  // Then check for a Email partial match
  if (emailAddress && CommonPrefix(emailAddress, fullString, fullStringLen))
  {
    *matchType = EMAIL_MATCH;
    return PR_TRUE;
  }
  
  //If we have a muti-part search string, look for a partial match with first name and last name or reverse
  if (searchStr->mFirstPartLen && searchStr->mSecondPartLen)
  {
    if (((firstName && CommonPrefix(firstName, searchStr->mFirstPart, searchStr->mFirstPartLen)) &&
        (lastName && CommonPrefix(lastName, searchStr->mSecondPart, searchStr->mSecondPartLen))) ||
        
        
        ((lastName && CommonPrefix(lastName, searchStr->mFirstPart, searchStr->mFirstPartLen)) &&
        (firstName && CommonPrefix(firstName, searchStr->mSecondPart, searchStr->mSecondPartLen)))
        )
    {
      *matchType = NAME_MATCH;
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult nsAbAutoCompleteSession::SearchCards(nsIAbDirectory* directory, nsAbAutoCompleteSearchString* searchStr, nsIAutoCompleteResults* results)
{
  nsresult rv;    
  nsCOMPtr<nsIEnumerator> cardsEnumerator;
  nsCOMPtr<nsIAbCard> card;
  PRInt32 i;
  
  rv = directory->GetChildCards(getter_AddRefs(cardsEnumerator));
  if (NS_SUCCEEDED(rv) && cardsEnumerator)
  {
    nsCOMPtr<nsISupports> item;
    for (rv = cardsEnumerator->First(); NS_SUCCEEDED(rv); rv = cardsEnumerator->Next())
    {
      rv = cardsEnumerator->CurrentItem(getter_AddRefs(item));
      if (NS_SUCCEEDED(rv))
      {
        card = do_QueryInterface(item, &rv);
        if (NS_SUCCEEDED(rv))
        {
          // Skip if it's not a normal card (ie, they can't be added as members).
          PRBool isNormal;
          rv = card->GetIsANormalCard(&isNormal);
          if (NS_FAILED(rv) || !isNormal)
            continue;

          nsXPIDLString pEmailStr[MAX_NUMBER_OF_EMAIL_ADDRESSES]; //[0]=primary email, [1]=secondary email (no available with mailing list)
          nsXPIDLString pDisplayNameStr;
          nsXPIDLString pFirstNameStr;
          nsXPIDLString pLastNameStr;
          nsXPIDLString pNickNameStr;
          nsXPIDLString pNotesStr;
          PRBool bIsMailList;

          rv = card->GetIsMailList(&bIsMailList);
          if (NS_FAILED(rv))
            continue;
          if (bIsMailList)
          {
            rv = card->GetNotes(getter_Copies(pNotesStr));
            if (NS_FAILED(rv))
              continue;
          }
          else
          {
            for (i = 0 ; i < MAX_NUMBER_OF_EMAIL_ADDRESSES; i ++)
            {
              switch (i)
              {
                case 0: rv = card->GetPrimaryEmail(getter_Copies(pEmailStr[i]));  break;
                case 1: rv = card->GetSecondEmail(getter_Copies(pEmailStr[i]));   break;
                default: return NS_ERROR_FAILURE;
              }
              if (NS_FAILED(rv))
                continue;

              // Don't bother with card without an email address
              if (pEmailStr[i].IsEmpty())
                continue;

              //...and does it looks like a valid address?
              if (pEmailStr[i].FindChar('@') <= 0)
                pEmailStr[i].SetLength(0);
            }
            if (pEmailStr[0].IsEmpty() && pEmailStr[1].IsEmpty())
              continue;
          }
            
            //Now, retrive the user name and nickname
          rv = card->GetDisplayName(getter_Copies(pDisplayNameStr));
          if (NS_FAILED(rv))
              continue;
          rv = card->GetFirstName(getter_Copies(pFirstNameStr));
          if (NS_FAILED(rv))
              continue;
          rv = card->GetLastName(getter_Copies(pLastNameStr));
          if (NS_FAILED(rv))
              continue;
          rv = card->GetNickName(getter_Copies(pNickNameStr));
          if (NS_FAILED(rv))
              continue;

          // in the address book a mailing list does not have an email address field. However,
          // we do "fix up" mailing lists in the UI sometimes to look like "My List <My List>"
          // if we are looking up an address and we are comparing it to a mailing list to see if it is a match
          // instead of just looking for an exact match on "My List", hijack the unused email address field 
          // and use that to test against "My List <My List>"
          if (bIsMailList)
            mParser->MakeFullAddressWString (pDisplayNameStr, pDisplayNameStr, getter_Copies(pEmailStr[0]));  
            
          for (i = 0 ; i < MAX_NUMBER_OF_EMAIL_ADDRESSES; i ++)
          {
            if (!bIsMailList && pEmailStr[i].IsEmpty())
              continue;

            MatchType matchType;
            if (CheckEntry(searchStr, pNickNameStr.get(), 
                                      pDisplayNameStr.get(), 
                                      pFirstNameStr.get(), 
                                      pLastNameStr.get(), pEmailStr[i].get(), 
                                      &matchType))
            {
              nsXPIDLString pDirName;
              if (mAutoCompleteCommentColumn == 1)
              {
                rv = directory->GetDirName(getter_Copies(pDirName));
                if (NS_FAILED(rv))
                  continue;
              }

              AddToResult(pNickNameStr.get(), pDisplayNameStr.get(), 
                          pFirstNameStr.get(), pLastNameStr.get(), 
                          pEmailStr[i].get(), pNotesStr.get(), 
                          pDirName.get(), bIsMailList, matchType, 
                          results);
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsAbAutoCompleteSession::NeedToSearchLocalDirectories(nsIPref *aPrefs, PRBool *aNeedToSearch)
{
  NS_ENSURE_ARG_POINTER(aPrefs);
  NS_ENSURE_ARG_POINTER(aNeedToSearch);

  nsresult rv = aPrefs->GetBoolPref("mail.enable_autocomplete", aNeedToSearch);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}
  
nsresult
nsAbAutoCompleteSession::NeedToSearchReplicatedLDAPDirectories(nsIPref *aPrefs, PRBool *aNeedToSearch)
{
  NS_ENSURE_ARG_POINTER(aPrefs);
  NS_ENSURE_ARG_POINTER(aNeedToSearch);

  // first check if the user is set up to do LDAP autocompletion
  nsresult rv = aPrefs->GetBoolPref("ldap_2.autoComplete.useDirectory", aNeedToSearch);
  NS_ENSURE_SUCCESS(rv, rv);

  // no need to search if not set up for LDAP autocompletion
  if (!*aNeedToSearch)
    return NS_OK;

  // now see if we are offline, if we are we need to search the
  // replicated directory
  nsCOMPtr <nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  rv = ioService->GetOffline(aNeedToSearch);
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult 
nsAbAutoCompleteSession::SearchReplicatedLDAPDirectories(nsIPref *aPref, nsAbAutoCompleteSearchString* searchStr, PRBool searchSubDirectory, nsIAutoCompleteResults* results)
{
  NS_ENSURE_ARG_POINTER(aPref);

  nsXPIDLCString prefName;
  nsresult rv = aPref->CopyCharPref("ldap_2.autoComplete.directoryServer", getter_Copies(prefName));
  NS_ENSURE_SUCCESS(rv,rv);

  if (prefName.IsEmpty())
    return NS_OK;
    
  // use the prefName to get the fileName pref
  nsCAutoString fileNamePref;
  fileNamePref = prefName + NS_LITERAL_CSTRING(".filename");

  nsXPIDLCString fileName;
  rv = aPref->CopyCharPref(fileNamePref.get(), getter_Copies(fileName));
  NS_ENSURE_SUCCESS(rv,rv);

  // if there is no fileName, bail out now.
  if (fileName.IsEmpty())
    return NS_OK;

  // use the fileName to create the URI for the replicated directory
  nsCAutoString URI;
  URI = NS_LITERAL_CSTRING("moz-abmdbdirectory://") + fileName;

  // and then search the replicated directory
  return SearchDirectory(URI, searchStr, searchSubDirectory, results);
}

nsresult nsAbAutoCompleteSession::SearchDirectory(const nsACString& aURI, nsAbAutoCompleteSearchString* searchStr, PRBool searchSubDirectory, nsIAutoCompleteResults* results)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIRDFService> rdfService(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsIRDFResource> resource;
    rv = rdfService->GetResource(aURI, getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    // query interface 
    nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    
    // when autocompleteing against directories, 
    // we only want to match against certain directories
    // we ask the directory if it wants to be used
    // for local autocompleting.
    PRBool searchDuringLocalAutocomplete;
    rv = directory->GetSearchDuringLocalAutocomplete(&searchDuringLocalAutocomplete);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!searchDuringLocalAutocomplete)
      return NS_OK;

    if (!aURI.EqualsLiteral(kAllDirectoryRoot))
        rv = SearchCards(directory, searchStr, results);
    
    if (!searchSubDirectory)
        return rv;
  
    nsCOMPtr<nsISimpleEnumerator> subDirectories;
    if (NS_SUCCEEDED(directory->GetChildNodes(getter_AddRefs(subDirectories))) && subDirectories)
    {
        nsCOMPtr<nsISupports> item;
        PRBool hasMore;
        while (NS_SUCCEEDED(rv = subDirectories->HasMoreElements(&hasMore)) && hasMore)
        {
            if (NS_SUCCEEDED(subDirectories->GetNext(getter_AddRefs(item))))
            {
              directory = do_QueryInterface(item, &rv);
              if (NS_SUCCEEDED(rv))
              {
                nsCOMPtr<nsIRDFResource> subResource(do_QueryInterface(item, &rv));
                if (NS_SUCCEEDED(rv))
                {
                    nsXPIDLCString URI;
                    subResource->GetValue(getter_Copies(URI));
                    rv = SearchDirectory(URI, searchStr, PR_TRUE, results);
                }
              }
            }
        }
    }
    return rv;
}

nsresult nsAbAutoCompleteSession::SearchPreviousResults(nsAbAutoCompleteSearchString *searchStr, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteResults* results)
{
    if (!previousSearchResult)
        return NS_ERROR_NULL_POINTER;
        
    nsXPIDLString prevSearchString;
    nsresult rv;

    rv = previousSearchResult->GetSearchString(getter_Copies(prevSearchString));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!(const PRUnichar*)prevSearchString || ((const PRUnichar*)prevSearchString)[0] == 0)
        return NS_ERROR_FAILURE;
    
    PRUint32 prevSearchStrLen = nsCRT::strlen(prevSearchString);
    if (searchStr->mFullStringLen < prevSearchStrLen ||
        CommonPrefix(searchStr->mFullString, prevSearchString, prevSearchStrLen))
        return NS_ERROR_ABORT;

    nsCOMPtr<nsISupportsArray> array;
    rv = previousSearchResult->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv))
    {
        PRUint32 nbrOfItems;
        PRUint32 i;
        PRUint32 pos;
        
        rv = array->Count(&nbrOfItems);
        if (NS_FAILED(rv) || nbrOfItems <= 0)
            return NS_ERROR_FAILURE;
        
        nsCOMPtr<nsISupports> item;
        nsCOMPtr<nsIAutoCompleteItem> resultItem;
        nsAbAutoCompleteParam *param;

        for (i = 0, pos = 0; i < nbrOfItems; i ++, pos ++)
        {
            rv = array->QueryElementAt(pos, NS_GET_IID(nsIAutoCompleteItem),
                                           getter_AddRefs(resultItem));
            NS_ENSURE_SUCCESS(rv, rv);
              
            rv = resultItem->GetParam(getter_AddRefs(item));
            NS_ENSURE_SUCCESS(rv, rv);
            if (!item)
                return NS_ERROR_FAILURE;

            param = (nsAbAutoCompleteParam *)(void *)item;
            
            MatchType matchType;
            if (CheckEntry(searchStr, param->mNickName, param->mDisplayName,  param->mFirstName,  param->mLastName, param->mEmailAddress, &matchType))
                AddToResult(param->mNickName, param->mDisplayName, 
                            param->mFirstName, param->mLastName, 
                            param->mEmailAddress, param->mNotes, 
                            param->mDirName, param->mIsMailList, matchType,
                            results);
        }
        return NS_OK;
    }

    return NS_ERROR_ABORT;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnStartLookup(const PRUnichar *uSearchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    nsresult rv = NS_OK;
    
    if (!listener)
        return NS_ERROR_NULL_POINTER;
    
    PRBool enableLocalAutocomplete;
    PRBool enableReplicatedLDAPAutocomplete;

    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv); 
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NeedToSearchLocalDirectories(prefs, &enableLocalAutocomplete);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = NeedToSearchReplicatedLDAPDirectories(prefs, &enableReplicatedLDAPAutocomplete);
    NS_ENSURE_SUCCESS(rv,rv);

    if (uSearchString[0] == 0 || (!enableLocalAutocomplete && !enableReplicatedLDAPAutocomplete))
    {
        listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
        return NS_OK;
    }

    // figure out what we're supposed to do about the comment column, and 
    // remember it for when the results start coming back
    //
    rv = prefs->GetIntPref("mail.autoComplete.commentColumn", 
                           &mAutoCompleteCommentColumn);
    if (NS_FAILED(rv)) {
      mAutoCompleteCommentColumn = 0;
    }


    // strings with @ signs or commas (commas denote multiple names) should be ignored for 
    // autocomplete purposes
    PRInt32 i;
    for (i = nsCRT::strlen(uSearchString) - 1; i >= 0; i --)
        if (uSearchString[i] == '@' || uSearchString[i] == ',')
        {
            listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
            return NS_OK;
        }
        
    nsAbAutoCompleteSearchString searchStrings(uSearchString);
    
    ResetMatchTypeConters();       
    nsCOMPtr<nsIAutoCompleteResults> results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      if (NS_FAILED(SearchPreviousResults(&searchStrings, previousSearchResult, results)))
      {
        nsresult rv1,rv2;

        if (enableLocalAutocomplete) {
          rv1 = SearchDirectory(NS_LITERAL_CSTRING(kAllDirectoryRoot), &searchStrings,
                                PR_TRUE, results);
          NS_ASSERTION(NS_SUCCEEDED(rv1), "searching all local directories failed");
        }
        else 
          rv1 = NS_OK;

        if (enableReplicatedLDAPAutocomplete) {
          rv2 = SearchReplicatedLDAPDirectories(prefs, &searchStrings, PR_TRUE, results);
          NS_ASSERTION(NS_SUCCEEDED(rv2), "searching all replicated LDAP directories failed");
        }
        else
          rv2 = NS_OK;
        
        // only bail out if both failed.  otherwise, we have some results we can use
        if (NS_FAILED(rv1) && NS_FAILED(rv2))
          rv = NS_ERROR_FAILURE;
        else 
          rv = NS_OK;
    }
                
    AutoCompleteStatus status = nsIAutoCompleteStatus::failed;
    if (NS_SUCCEEDED(rv) && results)
    {
        PRBool addedDefaultItem = PR_FALSE;

        results->SetSearchString(uSearchString);
        results->SetDefaultItemIndex(-1);
        if (mDefaultDomain[0] != 0)
        {
            PRUnichar emptyStr = 0;
            AddToResult(&emptyStr, uSearchString, &emptyStr, &emptyStr, 
                        &emptyStr, &emptyStr, &emptyStr, PR_FALSE, 
                        DEFAULT_MATCH, results);
            addedDefaultItem = PR_TRUE;
        }

        nsCOMPtr<nsISupportsArray> array;
        rv = results->GetItems(getter_AddRefs(array));
        if (NS_SUCCEEDED(rv))
        {
          //If we have more than a match (without counting the default item), we don't
          //want to auto complete the user input therefore set the default item index to -1

          PRUint32 nbrOfItems;
          rv = array->Count(&nbrOfItems);
          if (NS_SUCCEEDED(rv))
            if (nbrOfItems == 0)
              status = nsIAutoCompleteStatus::noMatch;
            else
            {
              status = nsIAutoCompleteStatus::matchFound;
              if (addedDefaultItem)
              {
                // If we have at least one REAL match then make it the default item. If we don't have any matches,
                // just the default domain, then don't install a default item index on the widget.
                results->SetDefaultItemIndex(nbrOfItems > 1 ? 1 : -1);
              }
              else
                results->SetDefaultItemIndex(0);  
            }
        }
    }
    listener->OnAutoComplete(results, status);
    
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnStopLookup()
{
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::OnAutoComplete(const PRUnichar *searchString, nsIAutoCompleteResults *previousSearchResult, nsIAutoCompleteListener *listener)
{
    return OnStartLookup(searchString, previousSearchResult, listener);
}

NS_IMETHODIMP nsAbAutoCompleteSession::GetDefaultDomain(PRUnichar * *aDefaultDomain)
{
    if (!aDefaultDomain)
        return NS_ERROR_NULL_POINTER;

    *aDefaultDomain = ToNewUnicode(mDefaultDomain);
    return NS_OK;
}

NS_IMETHODIMP nsAbAutoCompleteSession::SetDefaultDomain(const PRUnichar * aDefaultDomain)
{
    mDefaultDomain = aDefaultDomain;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsAbAutoCompleteParam, nsISupports)

nsAbAutoCompleteSearchString::nsAbAutoCompleteSearchString(const PRUnichar *uSearchString)
{
  mFullString = nsCRT::strdup(uSearchString);
  mFullStringLen = nsCRT::strlen(mFullString);
  
  PRUint32 i;
  PRUnichar * aPtr;
  for (i = 0, aPtr = (PRUnichar*)mFullString; i < mFullStringLen; i ++, aPtr ++)
  {
    if (*aPtr == ' ')
    {
      mFirstPart = nsCRT::strndup(mFullString, i);
      mFirstPartLen = i;
      mSecondPart = nsCRT::strdup(++aPtr);
      mSecondPartLen = mFullStringLen - i - 1;
      return;
    }
  }
  
  /* If we did not find a space in the search string, initialize the first and second part as null */
  mFirstPart = nsnull;
  mFirstPartLen = 0;
  mSecondPart = nsnull;
  mSecondPartLen = 0;
}

nsAbAutoCompleteSearchString::~nsAbAutoCompleteSearchString()
{
  if (mFullString)
     nsCRT::free((PRUnichar*)mFullString);
  if (mFirstPart)
     nsCRT::free((PRUnichar*)mFirstPart);
  if (mSecondPart)
     nsCRT::free((PRUnichar*)mSecondPart);
}
