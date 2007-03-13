/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
 *   Mark Banner <mark@standard8.demon.co.uk>
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
#include "nsAbOutlookDirectory.h"
#include "nsAbWinHelper.h"

#include "nsIRDFService.h"

#include "nsAbBaseCID.h"
#include "nsIAbCard.h"
#include "nsAbOutlookCard.h"
#include "nsXPIDLString.h"
#include "nsAbDirectoryQuery.h"
#include "nsIAbBooleanExpression.h"
#include "nsIAddressBook.h"
#include "nsIAddrBookSession.h"
#include "nsIAbMDBDirectory.h"
#include "nsAbQueryStringToExpression.h"
#include "nsAbUtils.h"
#include "nsIProxyObjectManager.h"
#include "nsEnumeratorUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsCRT.h"
#include "prlog.h"
#include "prthread.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gAbOutlookDirectoryLog
    = PR_NewLogModule("nsAbOutlookDirectoryLog");
#endif

#define PRINTF(args) PR_LOG(gAbOutlookDirectoryLog, PR_LOG_DEBUG, args)


// Class for the int key
class nsIntegerKey : public nsHashKey
{
public:
    nsIntegerKey(PRInt32 aInteger) 
        : mInteger(aInteger) {
    }
    virtual ~nsIntegerKey(void) {}
    
    PRUint32 HashCode(void) const { return NS_STATIC_CAST(PRUint32, mInteger) ; }
    PRBool Equals(const nsHashKey *aKey) const { 
        return aKey && NS_REINTERPRET_CAST(const nsIntegerKey *, aKey)->mInteger == mInteger ; 
    }
    nsHashKey *Clone(void) const { return new nsIntegerKey(mInteger) ; }
    
protected:
    PRInt32 mInteger ;
    
private:
} ;

nsAbOutlookDirectory::nsAbOutlookDirectory(void)
: nsAbDirectoryRDFResource(), nsAbDirProperty(), nsAbDirSearchListenerContext(), 
mQueryThreads(16, PR_TRUE), mCurrentQueryId(0), mSearchContext(-1), 
mAbWinType(nsAbWinType_Unknown), mMapiData(nsnull)
{
    mMapiData = new nsMapiEntry ;
    mProtector = PR_NewLock() ;
}

nsAbOutlookDirectory::~nsAbOutlookDirectory(void)
{
    if (mMapiData) { delete mMapiData ; }
    if (mProtector) { PR_DestroyLock(mProtector) ; }
}

NS_IMPL_ISUPPORTS_INHERITED3(nsAbOutlookDirectory, nsAbDirectoryRDFResource, 
                             nsIAbDirectory, nsIAbDirectoryQuery, nsIAbDirectorySearch)

// nsIRDFResource method
NS_IMETHODIMP nsAbOutlookDirectory::Init(const char *aUri)
{
  nsresult retCode = nsAbDirectoryRDFResource::Init(aUri);

  NS_ENSURE_SUCCESS(retCode, retCode);

  // We need to ensure  that the m_DirPrefId is initialized properly
  nsCAutoString uri(aUri);

  // Mailing lists don't have their own prefs.
  if (m_DirPrefId.IsEmpty() && (uri.Find("MailList") == kNotFound))
  {
    // Find the first ? (of the search params) if there is one.
    // We know we can start at the end of the moz-aboutlookdirectory:// because
    // that's the URI we should have been passed.
    PRInt32 searchCharLocation = uri.FindChar('?', kOutlookDirSchemeLength);

    // Get just the basic uri without the search params.
    if (searchCharLocation != kNotFound)
      uri.Left(uri, searchCharLocation);

    // Get the pref servers and the address book directory branch
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch(NS_LITERAL_CSTRING(PREF_LDAP_SERVER_TREE_NAME ".").get(),
                                getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, rv);

    char** childArray;
    PRUint32 childCount, i;
    PRInt32 dotOffset;
    nsXPIDLCString childValue;
    nsDependentCString child;

    rv = prefBranch->GetChildList("", &childCount, &childArray);
    NS_ENSURE_SUCCESS(rv, rv);

    for (i = 0; i < childCount; ++i)
    {
      child.Assign(childArray[i]);

      if (StringEndsWith(child, NS_LITERAL_CSTRING(".uri")) &&
          NS_SUCCEEDED(prefBranch->GetCharPref(child.get(),
                                               getter_Copies(childValue))) &&
          childValue == uri)
      {
        dotOffset = child.RFindChar('.');
        if (dotOffset != -1)
        {
          nsCAutoString prefName;
          child.Left(prefName, dotOffset);
          m_DirPrefId.AssignLiteral(PREF_LDAP_SERVER_TREE_NAME ".");
          m_DirPrefId.Append(prefName);
        }
      }
    }
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(childCount, childArray);

    NS_ASSERTION(!m_DirPrefId.IsEmpty(),
                 "Error, Could not set m_DirPrefId in nsAbOutlookDirectory::Init");
  }

  nsCAutoString entry;
  nsCAutoString stub;

  mAbWinType = getAbWinType(kOutlookDirectoryScheme, mURINoQuery.get(), stub, entry);
  if (mAbWinType == nsAbWinType_Unknown) {
    PRINTF(("Huge problem URI=%s.\n", mURINoQuery));
    return NS_ERROR_INVALID_ARG;
  }
  nsAbWinHelperGuard mapiAddBook (mAbWinType);
  nsString prefix;
  nsAutoString unichars;
  ULONG objectType = 0;

  if (!mapiAddBook->IsOK())
    return NS_ERROR_FAILURE;

  mMapiData->Assign(entry);
  if (!mapiAddBook->GetPropertyLong(*mMapiData, PR_OBJECT_TYPE, objectType)) {
    PRINTF(("Cannot get type.\n"));
    return NS_ERROR_FAILURE;
  }
  if (!mapiAddBook->GetPropertyUString(*mMapiData, PR_DISPLAY_NAME_W, unichars)) {
    PRINTF(("Cannot get name.\n"));
    return NS_ERROR_FAILURE;
  }

  if (mAbWinType == nsAbWinType_Outlook)
    prefix.AssignLiteral("OP ");
  else
    prefix.AssignLiteral("OE ");
  prefix.Append(unichars);
  SetDirName(prefix.get());

  if (objectType == MAPI_DISTLIST) {
    SetDirName(unichars.get());
    SetIsMailList(PR_TRUE);
  }
  else
    SetIsMailList(PR_FALSE);

  return UpdateAddressList();
}

// nsIAbDirectory methods

NS_IMETHODIMP nsAbOutlookDirectory::GetURI(nsACString &aURI)
{
  nsresult rv = GetStringValue("uri", EmptyCString(), aURI);

  if (aURI.IsEmpty())
  {
    rv = GetFileName(aURI);
    NS_ENSURE_SUCCESS(rv, rv);

    aURI.Insert(kMDBDirectoryRoot, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP nsAbOutlookDirectory::GetChildNodes(nsISimpleEnumerator **aNodes)
{
    if (!aNodes) { return NS_ERROR_NULL_POINTER ; }
    *aNodes = nsnull ;
    nsCOMPtr<nsISupportsArray> nodeList ;
    nsresult retCode = NS_OK ;
    
    if (mIsQueryURI) {
        NS_NewISupportsArray(getter_AddRefs(nodeList)) ;
    }
    else {
        retCode = GetChildNodes(getter_AddRefs(nodeList)) ;
    }
    if (NS_SUCCEEDED(retCode))
      retCode = NS_NewArrayEnumerator(aNodes, nodeList);
    return retCode;
}

NS_IMETHODIMP nsAbOutlookDirectory::GetChildCards(nsISimpleEnumerator **aCards)
{
    if (!aCards) { return NS_ERROR_NULL_POINTER ; }
    *aCards = nsnull ;
    nsCOMPtr<nsISupportsArray> cardList ;
    nsresult retCode ;
    
    mCardList.Reset() ;
    if (mIsQueryURI) {
        retCode = StartSearch() ;
        NS_NewISupportsArray(getter_AddRefs(cardList)) ;
    }
    else {
        retCode = GetChildCards(getter_AddRefs(cardList), nsnull) ;
    }
    if (NS_SUCCEEDED(retCode)) {
        // Fill the results array and update the card list
        // Also update the address list and notify any changes.
        PRUint32 nbCards = 0 ;
        nsCOMPtr<nsISupports> element ;
        
        NS_NewArrayEnumerator(aCards, cardList);
        cardList->Count(&nbCards) ;
        for (PRUint32 i = 0 ; i < nbCards ; ++ i) {
            cardList->GetElementAt(i, getter_AddRefs(element)) ;
            nsVoidKey newKey (NS_STATIC_CAST(void *, element)) ;
            nsCOMPtr<nsISupports> oldElement = mCardList.Get(&newKey) ;

            if (!oldElement) {
                // We are dealing with a new element (probably directly
                // added from Outlook), we may need to sync m_AddressList
                mCardList.Put(&newKey, element) ;
                nsCOMPtr<nsIAbCard> card (do_QueryInterface(element, &retCode)) ;

                NS_ENSURE_SUCCESS(retCode, retCode) ;
                PRBool isMailList = PR_FALSE ;

                retCode = card->GetIsMailList(&isMailList) ;
                NS_ENSURE_SUCCESS(retCode, retCode) ;
                if (isMailList) {
                    // We can have mailing lists only in folder, 
                    // we must add the directory to m_AddressList
                    nsXPIDLCString mailListUri ;

                    retCode = card->GetMailListURI(getter_Copies(mailListUri)) ;
                    NS_ENSURE_SUCCESS(retCode, retCode) ;
                    nsCOMPtr<nsIRDFResource> resource ;

                    retCode = gRDFService->GetResource(mailListUri, getter_AddRefs(resource)) ;
                    NS_ENSURE_SUCCESS(retCode, retCode) ;
                    nsCOMPtr<nsIAbDirectory> mailList(do_QueryInterface(resource, &retCode)) ;
                    
                    NS_ENSURE_SUCCESS(retCode, retCode) ;
                    m_AddressList->AppendElement(mailList) ;
                    NotifyItemAddition(mailList) ;
                }
                else if (m_IsMailList) {
                    m_AddressList->AppendElement(card) ;
                    NotifyItemAddition(card) ;
                }
            }
            else {
                NS_ASSERTION(oldElement == element, "Different card stored") ;
            }
        }
    }
    return retCode ;
}

NS_IMETHODIMP nsAbOutlookDirectory::HasCard(nsIAbCard *aCard, PRBool *aHasCard)
{
    if (!aCard || !aHasCard) { return NS_ERROR_NULL_POINTER ; }
    nsVoidKey key (NS_STATIC_CAST(void *, aCard)) ;

    *aHasCard = mCardList.Exists(&key) ;
    return NS_OK ;
}

NS_IMETHODIMP nsAbOutlookDirectory::HasDirectory(nsIAbDirectory *aDirectory, PRBool *aHasDirectory)
{
    if (!aDirectory || !aHasDirectory ) { return NS_ERROR_NULL_POINTER ; }
    *aHasDirectory = (m_AddressList->IndexOf(aDirectory) >= 0) ;
    return NS_OK ;
}

static nsresult ExtractEntryFromUri(nsIRDFResource *aResource, nsCString &aEntry,
                                    const char *aPrefix)
{
    aEntry.Truncate() ;
    if (!aPrefix ) { return NS_ERROR_NULL_POINTER ; }
    nsXPIDLCString uri ;
    nsresult retCode = aResource->GetValue(getter_Copies(uri)) ;
    
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCAutoString stub ;
    nsAbWinType objType = getAbWinType(aPrefix, uri.get(), stub, aEntry) ;

    return NS_OK ;
}

static nsresult ExtractCardEntry(nsIAbCard *aCard, nsCString& aEntry)
{
    aEntry.Truncate() ;
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsIRDFResource> resource (do_QueryInterface(aCard, &retCode)) ;
    
    // Receiving a non-RDF card is accepted
    if (NS_FAILED(retCode)) { return NS_OK ; }
    return ExtractEntryFromUri(resource, aEntry, kOutlookCardScheme) ;
}

static nsresult ExtractDirectoryEntry(nsIAbDirectory *aDirectory, nsCString& aEntry)
{
    aEntry.Truncate() ;
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsIRDFResource> resource (do_QueryInterface(aDirectory, &retCode)) ;
    
    // Receiving a non-RDF directory is accepted
    if (NS_FAILED(retCode)) { return NS_OK ; }
    return ExtractEntryFromUri(resource, aEntry, kOutlookDirectoryScheme) ;
}

NS_IMETHODIMP nsAbOutlookDirectory::DeleteCards(nsISupportsArray *aCardList)
{
    if (mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    if (!aCardList) { return NS_ERROR_NULL_POINTER ; }
    PRUint32 nbCards = 0 ;
    nsresult retCode = NS_OK ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    
    retCode = aCardList->Count(&nbCards) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    PRUint32 i = 0 ;
    nsCOMPtr<nsISupports> element ;
    nsCAutoString entryString ;
    nsMapiEntry cardEntry ;

    for (i = 0 ; i < nbCards ; ++ i) {
        retCode = aCardList->GetElementAt(i, getter_AddRefs(element)) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        nsCOMPtr<nsIAbCard> card (do_QueryInterface(element, &retCode)) ;
        
        NS_ENSURE_SUCCESS(retCode, retCode) ;

        retCode = ExtractCardEntry(card, entryString) ;
        if (NS_SUCCEEDED(retCode) && !entryString.IsEmpty()) {

            cardEntry.Assign(entryString) ;
            if (!mapiAddBook->DeleteEntry(*mMapiData, cardEntry)) {
                PRINTF(("Cannot delete card %s.\n", entryString.get())) ;
            }
            else {
                nsVoidKey key (NS_STATIC_CAST(void *, element)) ;
                
                mCardList.Remove(&key) ;
                if (m_IsMailList) { m_AddressList->RemoveElement(element) ; }
                retCode = NotifyItemDeletion(element) ;
                NS_ENSURE_SUCCESS(retCode, retCode) ;
            }
        }
        else {
            PRINTF(("Card doesn't belong in this directory.\n")) ;
        }
    }
    return NS_OK ;
}

NS_IMETHODIMP nsAbOutlookDirectory::ModifyDirectory(nsIAbDirectory *directory, nsIAbDirectoryProperties *aProperties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsAbOutlookDirectory::DeleteDirectory(nsIAbDirectory *aDirectory)
{
    if (mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    if (!aDirectory) { return NS_ERROR_NULL_POINTER ; }
    nsresult retCode = NS_OK ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;
    nsCAutoString entryString ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    retCode = ExtractDirectoryEntry(aDirectory, entryString) ;
    if (NS_SUCCEEDED(retCode) && !entryString.IsEmpty()) {
        nsMapiEntry directoryEntry ;

        directoryEntry.Assign(entryString) ;
        if (!mapiAddBook->DeleteEntry(*mMapiData, directoryEntry)) {
            PRINTF(("Cannot delete directory %s.\n", entryString.get())) ;
        }
        else {
            m_AddressList->RemoveElement(aDirectory) ;
            retCode = NotifyItemDeletion(aDirectory) ;
            NS_ENSURE_SUCCESS(retCode, retCode) ;
        }
    }
    else {
        PRINTF(("Directory doesn't belong to this folder.\n")) ;
    }
    return retCode ;
}

NS_IMETHODIMP nsAbOutlookDirectory::AddCard(nsIAbCard *aData, nsIAbCard **addedCard)
{
    if (mIsQueryURI)
      return NS_ERROR_NOT_IMPLEMENTED;

    NS_ENSURE_ARG_POINTER(aData);

    nsresult retCode = NS_OK ;
    PRBool hasCard = PR_FALSE ;
    
    retCode = HasCard(aData, &hasCard) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (hasCard) {
        PRINTF(("Has card.\n")) ;
        NS_IF_ADDREF(*addedCard = aData);
        return NS_OK ; 
    }
    retCode = CreateCard(aData, addedCard) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsVoidKey newKey (NS_STATIC_CAST(void *, *addedCard)) ;
    
    mCardList.Put(&newKey, *addedCard) ;
    if (m_IsMailList) { m_AddressList->AppendElement(*addedCard) ; }
    NotifyItemAddition(*addedCard) ;
    return retCode ;
}

NS_IMETHODIMP nsAbOutlookDirectory::DropCard(nsIAbCard *aData, PRBool needToCopyCard)
{
    nsCOMPtr <nsIAbCard> addedCard;
    return AddCard(aData, getter_AddRefs(addedCard));
}

NS_IMETHODIMP nsAbOutlookDirectory::AddMailList(nsIAbDirectory *aMailList)
{
    if (mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    if (!aMailList) { return NS_ERROR_NULL_POINTER ; }
    if (m_IsMailList) { return NS_OK ; }
    nsresult retCode = NS_OK ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;
    nsCAutoString entryString ;
    nsMapiEntry newEntry ;
    PRBool didCopy = PR_FALSE ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    retCode = ExtractDirectoryEntry(aMailList, entryString) ;
    if (NS_SUCCEEDED(retCode) && !entryString.IsEmpty()) {
        nsMapiEntry sourceEntry ;

        sourceEntry.Assign(entryString) ;
        mapiAddBook->CopyEntry(*mMapiData, sourceEntry, newEntry) ;
    }
    if (newEntry.mByteCount == 0) {
        if (!mapiAddBook->CreateDistList(*mMapiData, newEntry)) {
            return NS_ERROR_FAILURE ; 
        }
    }
    else { didCopy = PR_TRUE ; }
    newEntry.ToString(entryString) ;
    nsCAutoString uri ;

    buildAbWinUri(kOutlookDirectoryScheme, mAbWinType, uri) ;
    uri.Append(entryString) ;
    nsCOMPtr<nsIRDFResource> resource ;
    retCode = gRDFService->GetResource(uri, getter_AddRefs(resource)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCOMPtr<nsIAbDirectory> newList(do_QueryInterface(resource, &retCode)) ;
    
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!didCopy) {
        retCode = newList->CopyMailList(aMailList) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        retCode = newList->EditMailListToDatabase(mURINoQuery.get(), nsnull) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
    }
    m_AddressList->AppendElement(newList) ;
    NotifyItemAddition(newList) ;
    return retCode ;
}

NS_IMETHODIMP nsAbOutlookDirectory::EditMailListToDatabase(const char *aUri, nsIAbCard *listCard)
{
    if (mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    nsresult retCode = NS_OK ;
    nsXPIDLString name ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    retCode = GetDirName(getter_Copies(name)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!mapiAddBook->SetPropertyUString(*mMapiData, PR_DISPLAY_NAME_W, name.get())) {
        return NS_ERROR_FAILURE ;
    }
    retCode = CommitAddressList() ;
    return retCode ;
}

struct OutlookTableAttr
{
    const char *mOuterName ;
    ULONG mMapiProp ;
} ;

/*static const OutlookTableAttr OutlookTableStringToProp [] = 
{
{"FirstName", PR_GIVEN_NAME_W},
{"LastName", PR_SURNAME_W},
{"DisplayName", PR_DISPLAY_NAME_W},
{"NickName", PR_NICKNAME_W},
{"PrimaryEmail", PR_EMAIL_ADDRESS_W},
{"WorkPhone", PR_BUSINESS_TELEPHONE_NUMBER_W},
{"HomePhone", PR_HOME_TELEPHONE_NUMBER_W},
{"FaxNumber", PR_BUSINESS_FAX_NUMBER_W},
{"PagerNumber", PR_PAGER_TELEPHONE_NUMBER_W},
{"CellularNumber", PR_MOBILE_TELEPHONE_NUMBER_W},
{"HomeAddress", PR_HOME_ADDRESS_STREET_W},
{"HomeCity", PR_HOME_ADDRESS_CITY_W},
{"HomeState", PR_HOME_ADDRESS_STATE_OR_PROVINCE_W},
{"HomeZipCode", PR_HOME_ADDRESS_POSTAL_CODE_W},
{"HomeCountry", PR_HOME_ADDRESS_COUNTRY_W},
{"WorkAddress", PR_BUSINESS_ADDRESS_STREET_W}, 
{"WorkCity", PR_BUSINESS_ADDRESS_CITY_W},
{"WorkState", PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE_W},
{"WorkZipCode", PR_BUSINESS_ADDRESS_POSTAL_CODE_W},
{"WorkCountry", PR_BUSINESS_ADDRESS_COUNTRY_W},
{"JobTitle", PR_TITLE_W},
{"Department", PR_DEPARTMENT_NAME_W},
{"Company", PR_COMPANY_NAME_W},
{"WebPage1", PR_BUSINESS_HOME_PAGE_W},
{"WebPage2", PR_PERSONAL_HOME_PAGE_W},
// For the moment, we don't support querying on the birthday
// sub-elements.
#if 0
{"BirthYear", PR_BIRTHDAY},
{"BirthMonth", PR_BIRTHDAY}, 
{"BirthDay", PR_BIRTHDAY},
#endif // 0
{"Notes", PR_COMMENT_W}
} ;*/

// Here, we are forced to use the Ascii versions of the properties
// instead of the widechar ones, because the content restriction
// operators do not work on unicode strings in mapi.
static const OutlookTableAttr OutlookTableStringToProp [] = 
{
    // replace "PrimaryEmail" with kPriEmailColumn etc.
    {"FirstName", PR_GIVEN_NAME_A},
    {"LastName", PR_SURNAME_A},
    {"DisplayName", PR_DISPLAY_NAME_A},
    {"NickName", PR_NICKNAME_A},
    {"PrimaryEmail", PR_EMAIL_ADDRESS_A},
    {"WorkPhone", PR_BUSINESS_TELEPHONE_NUMBER_A},
    {"HomePhone", PR_HOME_TELEPHONE_NUMBER_A},
    {"FaxNumber", PR_BUSINESS_FAX_NUMBER_A},
    {"PagerNumber", PR_PAGER_TELEPHONE_NUMBER_A},
    {"CellularNumber", PR_MOBILE_TELEPHONE_NUMBER_A},
    {"HomeAddress", PR_HOME_ADDRESS_STREET_A},
    {"HomeCity", PR_HOME_ADDRESS_CITY_A},
    {"HomeState", PR_HOME_ADDRESS_STATE_OR_PROVINCE_A},
    {"HomeZipCode", PR_HOME_ADDRESS_POSTAL_CODE_A},
    {"HomeCountry", PR_HOME_ADDRESS_COUNTRY_A},
    {"WorkAddress", PR_BUSINESS_ADDRESS_STREET_A}, 
    {"WorkCity", PR_BUSINESS_ADDRESS_CITY_A},
    {"WorkState", PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE_A},
    {"WorkZipCode", PR_BUSINESS_ADDRESS_POSTAL_CODE_A},
    {"WorkCountry", PR_BUSINESS_ADDRESS_COUNTRY_A},
    {"JobTitle", PR_TITLE_A},
    {"Department", PR_DEPARTMENT_NAME_A},
    {"Company", PR_COMPANY_NAME_A},
    {"WebPage1", PR_BUSINESS_HOME_PAGE_A},
    {"WebPage2", PR_PERSONAL_HOME_PAGE_A},
    // For the moment, we don't support querying on the birthday
    // sub-elements.
#if 0
    {"BirthYear", PR_BIRTHDAY},
    {"BirthMonth", PR_BIRTHDAY}, 
    {"BirthDay", PR_BIRTHDAY},
#endif // 0
    {"Notes", PR_COMMENT_A}
} ;

static const PRUint32 OutlookTableNbProps = sizeof(OutlookTableStringToProp) /
                                            sizeof(OutlookTableStringToProp [0]) ;

static ULONG findPropertyTag(const char *aName) {
    PRUint32 i = 0 ;
    
    for (i = 0 ; i < OutlookTableNbProps ; ++ i) {
        if (nsCRT::strcmp(aName, OutlookTableStringToProp [i].mOuterName) == 0) {
            return OutlookTableStringToProp [i].mMapiProp ;
        }
    }
    return 0 ;
}

static nsresult BuildRestriction(nsIAbBooleanConditionString *aCondition, 
                                 SRestriction& aRestriction,
                                 PRBool& aSkipItem)
{
    if (!aCondition) { return NS_ERROR_NULL_POINTER ; }
    aSkipItem = PR_FALSE ;
    nsAbBooleanConditionType conditionType = 0 ;
    nsresult retCode = NS_OK ;
    nsXPIDLCString name ;
    nsXPIDLString value ;
    ULONG propertyTag = 0 ;
    nsCAutoString valueAscii ;
    
    retCode = aCondition->GetCondition(&conditionType) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = aCondition->GetName(getter_Copies(name)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = aCondition->GetValue(getter_Copies(value)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    valueAscii.AssignWithConversion(value.get()) ;
    propertyTag = findPropertyTag(name.get()) ;
    if (propertyTag == 0) {
        aSkipItem = PR_TRUE ;
        return retCode ;
    }
    switch (conditionType) {
    case nsIAbBooleanConditionTypes::Exists :
        aRestriction.rt = RES_EXIST ;
        aRestriction.res.resExist.ulPropTag = propertyTag ;
        break ;
    case nsIAbBooleanConditionTypes::DoesNotExist :
        aRestriction.rt = RES_NOT ;
        aRestriction.res.resNot.lpRes = new SRestriction ;
        aRestriction.res.resNot.lpRes->rt = RES_EXIST ;
        aRestriction.res.resNot.lpRes->res.resExist.ulPropTag = propertyTag ;
        break ;
    case nsIAbBooleanConditionTypes::Contains :
        aRestriction.rt = RES_CONTENT ;
        aRestriction.res.resContent.ulFuzzyLevel = FL_SUBSTRING | FL_LOOSE ;
        aRestriction.res.resContent.ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp = new SPropValue ;
        aRestriction.res.resContent.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::DoesNotContain :
        aRestriction.rt = RES_NOT ;
        aRestriction.res.resNot.lpRes = new SRestriction ;
        aRestriction.res.resNot.lpRes->rt = RES_CONTENT ;
        aRestriction.res.resNot.lpRes->res.resContent.ulFuzzyLevel = FL_SUBSTRING | FL_LOOSE ;
        aRestriction.res.resNot.lpRes->res.resContent.ulPropTag = propertyTag ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp = new SPropValue ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::Is :
        aRestriction.rt = RES_CONTENT ;
        aRestriction.res.resContent.ulFuzzyLevel = FL_FULLSTRING | FL_LOOSE ;
        aRestriction.res.resContent.ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp = new SPropValue ;
        aRestriction.res.resContent.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::IsNot :
        aRestriction.rt = RES_NOT ;
        aRestriction.res.resNot.lpRes = new SRestriction ;
        aRestriction.res.resNot.lpRes->rt = RES_CONTENT ;
        aRestriction.res.resNot.lpRes->res.resContent.ulFuzzyLevel = FL_FULLSTRING | FL_LOOSE ;
        aRestriction.res.resNot.lpRes->res.resContent.ulPropTag = propertyTag ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp = new SPropValue ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resNot.lpRes->res.resContent.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::BeginsWith :
        aRestriction.rt = RES_CONTENT ;
        aRestriction.res.resContent.ulFuzzyLevel = FL_PREFIX | FL_LOOSE ;
        aRestriction.res.resContent.ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp = new SPropValue ;
        aRestriction.res.resContent.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resContent.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::EndsWith :
        // This condition should be implemented through regular expressions,
        // but MAPI doesn't match them correctly.
#if 0
        aRestriction.rt = RES_PROPERTY ;
        aRestriction.res.resProperty.relop = RELOP_RE ;
        aRestriction.res.resProperty.ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp = new SPropValue ;
        aRestriction.res.resProperty.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
#else
        aSkipItem = PR_TRUE ;
#endif // 0
        break ;
    case nsIAbBooleanConditionTypes::SoundsLike :
        // This condition cannot be implemented in MAPI.
        aSkipItem = PR_TRUE ;
        break ;
    case nsIAbBooleanConditionTypes::RegExp :
        // This condition should be implemented this way, but the following
        // code will never match (through MAPI's fault).
#if 0
        aRestriction.rt = RES_PROPERTY ;
        aRestriction.res.resProperty.relop = RELOP_RE ;
        aRestriction.res.resProperty.ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp = new SPropValue ;
        aRestriction.res.resProperty.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
#else
        aSkipItem = PR_TRUE ;
#endif // 0
        break ;
    case nsIAbBooleanConditionTypes::LessThan :
        aRestriction.rt = RES_PROPERTY ;
        aRestriction.res.resProperty.relop = RELOP_LT ;
        aRestriction.res.resProperty.ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp = new SPropValue ;
        aRestriction.res.resProperty.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    case nsIAbBooleanConditionTypes::GreaterThan :
        aRestriction.rt = RES_PROPERTY ;
        aRestriction.res.resProperty.relop = RELOP_GT ;
        aRestriction.res.resProperty.ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp = new SPropValue ;
        aRestriction.res.resProperty.lpProp->ulPropTag = propertyTag ;
        aRestriction.res.resProperty.lpProp->Value.lpszA = nsCRT::strdup(valueAscii.get()) ;
        break ;
    default :
        aSkipItem = PR_TRUE ;
        break ;
    }
    return retCode ;
}

static nsresult BuildRestriction(nsIAbBooleanExpression *aLevel, 
                                 SRestriction& aRestriction)
{
    if (!aLevel) { return NS_ERROR_NULL_POINTER ; }
    aRestriction.rt = RES_COMMENT ;
    nsresult retCode = NS_OK ;
    nsAbBooleanOperationType operationType = 0 ;
    PRUint32 nbExpressions = 0 ;
    nsCOMPtr<nsISupportsArray> expressions ;
    
    retCode = aLevel->GetOperation(&operationType) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = aLevel->GetExpressions(getter_AddRefs(expressions)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = expressions->Count(&nbExpressions) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (nbExpressions == 0) { 
        PRINTF(("Error, no expressions.\n")) ;
        return NS_OK ;
    }
    if (operationType == nsIAbBooleanOperationTypes::NOT && nbExpressions != 1) {
        PRINTF(("Error, unary operation NOT with multiple operands.\n")) ;
        return NS_OK ;
    }
    LPSRestriction restrictionArray = new SRestriction [nbExpressions] ;
    PRUint32 realNbExpressions = 0 ;
    PRBool skipItem = PR_FALSE ;
    PRUint32 i = 0 ;
    nsCOMPtr<nsISupports> element ;

    for (i = 0 ; i < nbExpressions ; ++ i) {
        retCode = expressions->GetElementAt(i, getter_AddRefs(element)) ;
        if (NS_SUCCEEDED(retCode)) {
            nsCOMPtr<nsIAbBooleanConditionString> condition (do_QueryInterface(element, &retCode)) ;
            
            if (NS_SUCCEEDED(retCode)) {
                retCode = BuildRestriction(condition, *restrictionArray, skipItem) ;
                if (NS_SUCCEEDED(retCode)) {
                    if (!skipItem) { ++ restrictionArray ; ++ realNbExpressions ; }
                }
                else { PRINTF(("Cannot build restriction for item %d %08x.\n", i, retCode)) ; }
            }
            else { 
                nsCOMPtr<nsIAbBooleanExpression> subExpression (do_QueryInterface(element, &retCode)) ;

                if (NS_SUCCEEDED(retCode)) {
                    retCode = BuildRestriction(subExpression, *restrictionArray) ;
                    if (NS_SUCCEEDED(retCode)) {
                        if (restrictionArray->rt != RES_COMMENT) {
                            ++ restrictionArray ; ++ realNbExpressions ; 
                        }
                    }
                }
                else { PRINTF(("Cannot get interface for item %d %08x.\n", i, retCode)) ; }
            }
        }
        else { PRINTF(("Cannot get item %d %08x.\n", i, retCode)) ; }
    }
    restrictionArray -= realNbExpressions ;
    if (realNbExpressions > 1) {
        if (operationType == nsIAbBooleanOperationTypes::OR) {
            aRestriction.rt = RES_OR ;
            aRestriction.res.resOr.lpRes = restrictionArray ;
            aRestriction.res.resOr.cRes = realNbExpressions ;
        }
        else if (operationType == nsIAbBooleanOperationTypes::AND) {
            aRestriction.rt = RES_AND ;
            aRestriction.res.resAnd.lpRes = restrictionArray ;
            aRestriction.res.resAnd.cRes = realNbExpressions ;
        }
        else {
            PRINTF(("Unsupported operation %d.\n", operationType)) ;
        }
    }
    else if (realNbExpressions == 1) {
        if (operationType == nsIAbBooleanOperationTypes::NOT) {
            aRestriction.rt = RES_NOT ;
            // This copy is to ensure that every NOT restriction is being 
            // allocated by new and not new[] (see destruction of restriction)
            aRestriction.res.resNot.lpRes = new SRestriction ;
            memcpy(aRestriction.res.resNot.lpRes, restrictionArray, sizeof(SRestriction)) ;
        }
        else {
            // Case where the restriction array is redundant,
            // we need to fill the restriction directly.
            memcpy(&aRestriction, restrictionArray, sizeof(SRestriction)) ;
        }
        delete [] restrictionArray ;
    }
    if (aRestriction.rt == RES_COMMENT) {
        // This means we haven't really built any useful expression
        delete [] restrictionArray ;
    }
    return NS_OK ;
}

static nsresult BuildRestriction(nsIAbDirectoryQueryArguments *aArguments, 
                                 SRestriction& aRestriction)
{
    if (!aArguments) { return NS_ERROR_NULL_POINTER ; }
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsIAbBooleanExpression> booleanQuery ;

    retCode = aArguments->GetExpression(getter_AddRefs(booleanQuery)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = BuildRestriction(booleanQuery, aRestriction) ;
    return retCode ;
}

static void DestroyRestriction(SRestriction& aRestriction)
{
    switch(aRestriction.rt) {
    case RES_AND :
    case RES_OR :
        {
            for (ULONG i = 0 ; i < aRestriction.res.resAnd.cRes ; ++ i) {
                DestroyRestriction(aRestriction.res.resAnd.lpRes [i]) ;
            }
            delete [] aRestriction.res.resAnd.lpRes ;
        }
        break ;
    case RES_COMMENT :
        break ;
    case RES_CONTENT :
        if (PROP_TYPE(aRestriction.res.resContent.ulPropTag) == PT_UNICODE) {
            nsCRT::free(aRestriction.res.resContent.lpProp->Value.lpszW) ;
        }
        else if (PROP_TYPE(aRestriction.res.resContent.ulPropTag) == PT_STRING8) {
            nsCRT::free(aRestriction.res.resContent.lpProp->Value.lpszA) ;
        }
        delete aRestriction.res.resContent.lpProp ;
        break ;
    case RES_EXIST :
        break ;
    case RES_NOT :
        DestroyRestriction(*aRestriction.res.resNot.lpRes) ;
        delete aRestriction.res.resNot.lpRes ;
        break ;
    case RES_BITMASK :
    case RES_COMPAREPROPS :
        break ;
    case RES_PROPERTY :
        if (PROP_TYPE(aRestriction.res.resProperty.ulPropTag) == PT_UNICODE) {
            nsCRT::free(aRestriction.res.resProperty.lpProp->Value.lpszW) ;
        }
        else if (PROP_TYPE(aRestriction.res.resProperty.ulPropTag) == PT_STRING8) {
            nsCRT::free(aRestriction.res.resProperty.lpProp->Value.lpszA) ;
        }
        delete aRestriction.res.resProperty.lpProp ;
    case RES_SIZE :
    case RES_SUBRESTRICTION :
        break ;
    }
}

nsresult FillPropertyValues(nsIAbCard *aCard, nsIAbDirectoryQueryArguments *aArguments, 
                            nsISupportsArray **aValues)
{
    if (!aCard || !aArguments || !aValues ) { 
        return NS_ERROR_NULL_POINTER ; 
    }
    *aValues = nsnull ;
    CharPtrArrayGuard properties ;
    nsCOMPtr<nsISupportsArray> values ;
    nsresult retCode = NS_OK ;
    
    retCode = aArguments->GetReturnProperties(properties.GetSizeAddr(), properties.GetArrayAddr()) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    PRUint32 i = 0 ;
    nsAbDirectoryQueryPropertyValue *newValue = nsnull ;
	nsCOMPtr<nsIAbDirectoryQueryPropertyValue> propertyValue ;

    for (i = 0 ; i < properties.GetSize() ; ++ i) {
        const char* cPropName = properties[i] ;
        newValue = nsnull ;
		
    if (!nsCRT::strcmp(cPropName, "card:nsIAbCard")) {
			nsCOMPtr<nsISupports> bogusInterface (do_QueryInterface(aCard, &retCode)) ;

			NS_ENSURE_SUCCESS(retCode, retCode) ;
			newValue = new nsAbDirectoryQueryPropertyValue (cPropName, bogusInterface) ;
		}
    else {
			nsXPIDLString value ;

			retCode = aCard->GetCardValue(cPropName, getter_Copies(value)) ;
			NS_ENSURE_SUCCESS(retCode, retCode) ;
            if (!value.IsEmpty()) {
                newValue = new nsAbDirectoryQueryPropertyValue(cPropName, value.get()) ;
            }
		}
        if (newValue) {
			if (!values) {
				NS_NewISupportsArray(getter_AddRefs(values)) ;
			}
			propertyValue = newValue ;
			values->AppendElement (propertyValue) ;
        }
    }
    *aValues = values ;
    NS_ADDREF(*aValues) ;
    return retCode ;
}

struct QueryThreadArgs 
{
    nsAbOutlookDirectory *mThis ;
    nsCOMPtr<nsIAbDirectoryQueryArguments> mArguments ;
    nsCOMPtr<nsIAbDirectoryQueryResultListener> mListener ;
    PRInt32 mResultLimit ;
    PRInt32 mTimeout ;
    PRInt32 mThreadId ;
} ;

static void QueryThreadFunc(void *aArguments)
{
    QueryThreadArgs *arguments = NS_REINTERPRET_CAST(QueryThreadArgs *, aArguments) ;

    if (!aArguments) { return ; }
    arguments->mThis->ExecuteQuery(arguments->mArguments, arguments->mListener,
                                   arguments->mResultLimit, arguments->mTimeout,
                                   arguments->mThreadId) ;
    delete arguments ;
}

nsresult nsAbOutlookDirectory::DoQuery(nsIAbDirectoryQueryArguments *aArguments,
                                       nsIAbDirectoryQueryResultListener *aListener,
                                       PRInt32 aResultLimit, PRInt32 aTimeout,
                                       PRInt32 *aReturnValue) 
{
    if (!aArguments || !aListener || !aReturnValue)  { 
        return NS_ERROR_NULL_POINTER ; 
    }
    *aReturnValue = -1 ;
    
    QueryThreadArgs *threadArgs = new QueryThreadArgs ;
    PRThread *newThread = nsnull ;

    if (!threadArgs) { 
        return NS_ERROR_OUT_OF_MEMORY ;
    }
    threadArgs->mThis = this ;
    threadArgs->mArguments = aArguments ;
    threadArgs->mListener = aListener ;
    threadArgs->mResultLimit = aResultLimit ; 
    threadArgs->mTimeout = aTimeout ;
    PR_Lock(mProtector) ;
    *aReturnValue = ++ mCurrentQueryId ;
    PR_Unlock(mProtector) ;
    threadArgs->mThreadId = *aReturnValue ;
    newThread = PR_CreateThread(PR_USER_THREAD,
                                QueryThreadFunc,
                                threadArgs,
                                PR_PRIORITY_NORMAL,
                                PR_GLOBAL_THREAD,
                                PR_UNJOINABLE_THREAD,
                                0) ;
    if (!newThread ) {
        delete threadArgs ;
        return NS_ERROR_OUT_OF_MEMORY ;
    }
    nsIntegerKey newKey(*aReturnValue) ;
    
    mQueryThreads.Put(&newKey, newThread) ;
    return NS_OK ;
}

nsresult nsAbOutlookDirectory::StopQuery(PRInt32 aContext)
{
    nsIntegerKey contextKey(aContext) ;
    PRThread *queryThread = NS_REINTERPRET_CAST(PRThread *, mQueryThreads.Get(&contextKey)) ;

    if (queryThread) { 
        PR_Interrupt(queryThread) ; 
        mQueryThreads.Remove(&contextKey) ;
    }
    return NS_OK ;
}

// nsIAbDirectorySearch methods
NS_IMETHODIMP nsAbOutlookDirectory::StartSearch(void)
{
    if (!mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    nsresult retCode = NS_OK ;
    
    retCode = StopSearch() ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    mCardList.Reset() ;

    nsCOMPtr<nsIAbBooleanExpression> expression ;

    nsCOMPtr<nsIAbDirectoryQueryArguments> arguments = do_CreateInstance(NS_ABDIRECTORYQUERYARGUMENTS_CONTRACTID,&retCode);
    NS_ENSURE_SUCCESS(retCode, retCode);

    retCode = nsAbQueryStringToExpression::Convert(mQueryString.get (), getter_AddRefs(expression)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = arguments->SetExpression(expression) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    
    const char *arr = "card:nsIAbCard";
    retCode = arguments->SetReturnProperties(1, &arr);
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = arguments->SetQuerySubDirectories(PR_TRUE) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCOMPtr<nsIAbDirectoryQueryResultListener> proxyListener;

    retCode = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                     NS_GET_IID(nsIAbDirectoryQueryResultListener),
                     NS_STATIC_CAST(nsIAbDirectoryQueryResultListener *, new nsAbDirSearchListener(this)),
                     NS_PROXY_SYNC | NS_PROXY_ALWAYS,
                     getter_AddRefs(proxyListener));
    NS_ENSURE_SUCCESS(retCode, retCode) ;

    return DoQuery(arguments, proxyListener, -1, 0, &mSearchContext) ;
}

NS_IMETHODIMP nsAbOutlookDirectory::StopSearch(void) 
{
    if (!mIsQueryURI) { return NS_ERROR_NOT_IMPLEMENTED ; }
    return StopQuery(mSearchContext) ;
}

// nsAbDirSearchListenerContext
nsresult nsAbOutlookDirectory::OnSearchFinished(PRInt32 aResult)
{
    return NS_OK ;
}

nsresult nsAbOutlookDirectory::OnSearchFoundCard(nsIAbCard *aCard) 
{
    nsVoidKey newKey (NS_STATIC_CAST(void *, aCard)) ;
    nsresult retCode = NS_OK ;
    
    mCardList.Put(&newKey, aCard) ;
    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &retCode);
    if (NS_SUCCEEDED(retCode)) { abSession->NotifyDirectoryItemAdded(this, aCard) ; }
    return retCode ;
}

nsresult nsAbOutlookDirectory::ExecuteQuery(nsIAbDirectoryQueryArguments *aArguments,
                                            nsIAbDirectoryQueryResultListener *aListener,
                                            PRInt32 aResultLimit, PRInt32 aTimeout,
                                            PRInt32 aThreadId) 

{
    if (!aArguments || !aListener) { 
        return NS_ERROR_NULL_POINTER ; 
    }
    SRestriction arguments ;
    nsresult retCode = NS_OK ;
    
    retCode = BuildRestriction(aArguments, arguments) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCOMPtr<nsISupportsArray> resultsArray ;
    PRUint32 nbResults = 0 ;
    
    retCode = GetChildCards(getter_AddRefs(resultsArray), 
                            arguments.rt == RES_COMMENT ? nsnull : &arguments) ;
    DestroyRestriction(arguments) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = resultsArray->Count(&nbResults) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCOMPtr<nsIAbDirectoryQueryResult> result ;
    nsAbDirectoryQueryResult *newResult = nsnull ;

    if (aResultLimit > 0 && nbResults > NS_STATIC_CAST(PRUint32, aResultLimit)) { 
        nbResults = NS_STATIC_CAST(PRUint32, aResultLimit) ; 
    }
    PRUint32 i = 0 ;
    nsCOMPtr<nsISupports> element ;
    nsCOMPtr<nsISupportsArray> propertyValues ;
    
    for (i = 0 ; i < nbResults ; ++ i) {
        retCode = resultsArray->GetElementAt(i, getter_AddRefs(element)) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        nsCOMPtr<nsIAbCard> card (do_QueryInterface(element, &retCode)) ;
        
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        FillPropertyValues(card, aArguments, getter_AddRefs(propertyValues)) ;
        newResult = new nsAbDirectoryQueryResult(0, aArguments,
                                                 nsIAbDirectoryQueryResult::queryResultMatch, 
                                                 propertyValues) ;
        if (!newResult) { return NS_ERROR_OUT_OF_MEMORY ; }
        result = newResult ;
        aListener->OnQueryItem(result) ;
    }
    nsIntegerKey hashKey(aThreadId) ;
    
    mQueryThreads.Remove(&hashKey) ;
    newResult = new nsAbDirectoryQueryResult(0, aArguments,
                                             nsIAbDirectoryQueryResult::queryResultComplete, 
                                             0) ;
    result = newResult ;
    aListener->OnQueryItem(result) ;
    return retCode ;
}
nsresult nsAbOutlookDirectory::GetChildCards(nsISupportsArray **aCards, 
                                             void *aRestriction)
{
    if (!aCards) { return NS_ERROR_NULL_POINTER ; }
    *aCards = nsnull ;
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsISupportsArray> cards ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;
    nsMapiEntryArray cardEntries ;
    LPSRestriction restriction = (LPSRestriction) aRestriction ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    retCode = NS_NewISupportsArray(getter_AddRefs(cards)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!mapiAddBook->GetCards(*mMapiData, restriction, cardEntries)) {
        PRINTF(("Cannot get cards.\n")) ;
        return NS_ERROR_FAILURE ;
    }
    nsCAutoString entryId ;
    nsCAutoString uriName ;
    nsCOMPtr<nsIRDFResource> resource ;
    nsCOMPtr <nsIAbCard> childCard;
        
    for (ULONG card = 0 ; card < cardEntries.mNbEntries ; ++ card) {
        cardEntries.mEntries [card].ToString(entryId) ;
        buildAbWinUri(kOutlookCardScheme, mAbWinType, uriName) ;
        uriName.Append(entryId) ;
        childCard = do_CreateInstance(NS_ABOUTLOOKCARD_CONTRACTID, &retCode);
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        resource = do_QueryInterface(childCard, &retCode) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        retCode = resource->Init(uriName.get()) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        cards->AppendElement(childCard) ;
    }
    *aCards = cards ;
    NS_ADDREF(*aCards) ;
    return retCode ;
}

nsresult nsAbOutlookDirectory::GetChildNodes(nsISupportsArray **aNodes)
{
    if (!aNodes) { return NS_ERROR_NULL_POINTER ; }
    *aNodes = nsnull ;
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsISupportsArray> nodes ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;
    nsMapiEntryArray nodeEntries ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    retCode = NS_NewISupportsArray(getter_AddRefs(nodes)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!mapiAddBook->GetNodes(*mMapiData, nodeEntries)) {
        PRINTF(("Cannot get nodes.\n")) ;
        return NS_ERROR_FAILURE ;
    }
    nsCAutoString entryId ;
    nsCAutoString uriName ;
    nsCOMPtr <nsIRDFResource> resource ;

    for (ULONG node = 0 ; node < nodeEntries.mNbEntries ; ++ node) {
        nodeEntries.mEntries [node].ToString(entryId) ;
        buildAbWinUri(kOutlookDirectoryScheme, mAbWinType, uriName) ;
        uriName.Append(entryId) ;
        retCode = gRDFService->GetResource(uriName, getter_AddRefs(resource)) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        nodes->AppendElement(resource) ;
    }
    *aNodes = nodes ;
    NS_ADDREF(*aNodes) ;
    return retCode ;
}

nsresult nsAbOutlookDirectory::NotifyItemDeletion(nsISupports *aItem) 
{
    nsresult retCode = NS_OK ;
    
    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &retCode);
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = abSession->NotifyDirectoryItemDeleted(this, aItem) ;
    return retCode ;
}

nsresult nsAbOutlookDirectory::NotifyItemAddition(nsISupports *aItem) 
{
    nsresult retCode = NS_OK ;
    
    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &retCode);
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = abSession->NotifyDirectoryItemAdded(this, aItem) ;
    return retCode ;
}

// This is called from EditMailListToDatabase.
// We got m_AddressList containing the list of cards the mailing
// list is supposed to contain at the end.
nsresult nsAbOutlookDirectory::CommitAddressList(void)
{
    nsresult retCode = NS_OK ;
    nsCOMPtr<nsISupportsArray> oldList ;
    PRUint32 nbCards = 0 ;
    PRUint32 i = 0 ;
    
    if (!m_IsMailList) { 
        PRINTF(("We are not in a mailing list, no commit can be done.\n")) ;
        return NS_ERROR_UNEXPECTED ;
    }
    retCode = GetChildCards(getter_AddRefs(oldList), nsnull) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = m_AddressList->Count(&nbCards) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    nsCOMPtr<nsISupports> element ;
    nsCOMPtr<nsIAbCard> newCard ;

    for (i = 0 ; i < nbCards ; ++ i) {
        retCode = m_AddressList->GetElementAt(i, getter_AddRefs(element)) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        if (!oldList->RemoveElement(element)) {
            // The entry was not already there
            nsCOMPtr<nsIAbCard> card (do_QueryInterface(element, &retCode)) ;
            
            NS_ENSURE_SUCCESS(retCode, retCode) ;

            retCode = CreateCard(card, getter_AddRefs(newCard)) ;
            NS_ENSURE_SUCCESS(retCode, retCode) ;
            m_AddressList->ReplaceElementAt(newCard, i) ;
        }
    }
    retCode = DeleteCards(oldList) ;
    return retCode ;
}

nsresult nsAbOutlookDirectory::UpdateAddressList(void)
{
    nsresult retCode = S_OK ;
    
    if (m_IsMailList) {
        retCode = GetChildCards(getter_AddRefs(m_AddressList), nsnull) ; 
    }
    else { 
        retCode = GetChildNodes(getter_AddRefs(m_AddressList)) ; 
    }
    return retCode ;
}

nsresult nsAbOutlookDirectory::CreateCard(nsIAbCard *aData, nsIAbCard **aNewCard) 
{
    if (!aData || !aNewCard) { return NS_ERROR_NULL_POINTER ; }
    *aNewCard = nsnull ;
    nsresult retCode = NS_OK ;
    nsAbWinHelperGuard mapiAddBook (mAbWinType) ;
    nsMapiEntry newEntry ;
    nsCAutoString entryString ;
    PRBool didCopy = PR_FALSE ;

    if (!mapiAddBook->IsOK()) { return NS_ERROR_FAILURE ; }
    // If we get a RDF resource and it maps onto an Outlook card uri,
    // we simply copy the contents of the Outlook card.
    retCode = ExtractCardEntry(aData, entryString) ;
    if (NS_SUCCEEDED(retCode) && !entryString.IsEmpty()) {
        nsMapiEntry sourceEntry ;
        
        
        sourceEntry.Assign(entryString) ;
        if (m_IsMailList) {
            // In the case of a mailing list, we can use the address
            // as a direct template to build the new one (which is done
            // by CopyEntry).
            mapiAddBook->CopyEntry(*mMapiData, sourceEntry, newEntry) ;
            didCopy = PR_TRUE ;
        }
        else {
            // Else, we have to create a temporary address and copy the
            // source into it. Yes it's silly.
            mapiAddBook->CreateEntry(*mMapiData, newEntry) ;
        }
    }
    // If this approach doesn't work, well we're back to creating and copying.
    if (newEntry.mByteCount == 0) {
        // In the case of a mailing list, we cannot directly create a new card,
        // we have to create a temporary one in a real folder (to be able to use
        // templates) and then copy it to the mailing list.
        if (m_IsMailList) {
            nsMapiEntry parentEntry ;
            nsMapiEntry temporaryEntry ;

            if (!mapiAddBook->GetDefaultContainer(parentEntry)) {
                return NS_ERROR_FAILURE ;
            }
            if (!mapiAddBook->CreateEntry(parentEntry, temporaryEntry)) {
                return NS_ERROR_FAILURE ;
            }
            if (!mapiAddBook->CopyEntry(*mMapiData, temporaryEntry, newEntry)) {
                return NS_ERROR_FAILURE ;
            }
            if (!mapiAddBook->DeleteEntry(parentEntry, temporaryEntry)) {
                return NS_ERROR_FAILURE ;
            }
        }
        // If we're on a real address book folder, we can directly create an
        // empty card.
        else if (!mapiAddBook->CreateEntry(*mMapiData, newEntry)) {
            return NS_ERROR_FAILURE ;
        }
    }
    newEntry.ToString(entryString) ;
    nsCAutoString uri ;

    buildAbWinUri(kOutlookCardScheme, mAbWinType, uri) ;
    uri.Append(entryString) ;
    nsCOMPtr<nsIRDFResource> resource ;
    
    nsCOMPtr<nsIAbCard> newCard = do_CreateInstance(NS_ABOUTLOOKCARD_CONTRACTID, &retCode);
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    resource = do_QueryInterface(newCard, &retCode) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    retCode = resource->Init(uri.get()) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!didCopy) {
        retCode = newCard->Copy(aData) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
        retCode = ModifyCard(newCard) ;
        NS_ENSURE_SUCCESS(retCode, retCode) ;
    }
    *aNewCard = newCard ;
    NS_ADDREF(*aNewCard) ;
    return retCode ;
}

static void UnicodeToWord(const PRUnichar *aUnicode, WORD& aWord)
{
    aWord = 0 ;
    if (aUnicode == nsnull || *aUnicode == 0) { return ; }
    PRInt32 errorCode = 0 ;
    nsAutoString unichar (aUnicode) ;

    aWord = NS_STATIC_CAST(WORD, unichar.ToInteger(&errorCode)) ;
    if (errorCode != 0) {
        PRINTF(("Error conversion string %S: %08x.\n", unichar.get(), errorCode)) ;
    }
}

#define PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST "mail.addr_book.lastnamefirst"

NS_IMETHODIMP nsAbOutlookDirectory::ModifyCard(nsIAbCard *aModifiedCard)
{
  NS_ENSURE_ARG_POINTER(aModifiedCard);

  nsresult retCode = NS_OK;
  nsXPIDLString *properties = nsnull;
  nsAutoString utility;
  nsAbWinHelperGuard mapiAddBook(mAbWinType);

  if (!mapiAddBook->IsOK())
    return NS_ERROR_FAILURE;

  // First, all the standard properties in one go
  properties = new nsXPIDLString[index_LastProp];
  if (!properties) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aModifiedCard->GetFirstName(getter_Copies(properties[index_FirstName]));
  aModifiedCard->GetLastName(getter_Copies(properties[index_LastName]));
  // This triple search for something to put in the name
  // is because in the case of a mailing list edition in 
  // Mozilla, the display name will not be provided, and 
  // MAPI doesn't allow that, so we fall back on an optional
  // name, and when all fails, on the email address.
  aModifiedCard->GetDisplayName(getter_Copies(properties[index_DisplayName]));
  if (*properties[index_DisplayName].get() == 0) {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    PRInt32 format;
    rv = prefBranch->GetIntPref(PREF_MAIL_ADDR_BOOK_LASTNAMEFIRST, &format);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIAddrBookSession> abSession =
      do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = abSession->GenerateNameFromCard(aModifiedCard, format, getter_Copies(properties [index_DisplayName]));
    NS_ENSURE_SUCCESS(rv,rv);

    if (*properties[index_DisplayName].get() == 0) {
      aModifiedCard->GetPrimaryEmail(getter_Copies(properties[index_DisplayName]));
    }
  }
  aModifiedCard->SetDisplayName(properties[index_DisplayName]);
  aModifiedCard->GetNickName(getter_Copies(properties[index_NickName]));
  aModifiedCard->GetPrimaryEmail(getter_Copies(properties[index_EmailAddress]));
  aModifiedCard->GetWorkPhone(getter_Copies(properties[index_WorkPhoneNumber]));
  aModifiedCard->GetHomePhone(getter_Copies(properties[index_HomePhoneNumber]));
  aModifiedCard->GetFaxNumber(getter_Copies(properties[index_WorkFaxNumber]));
  aModifiedCard->GetPagerNumber(getter_Copies(properties[index_PagerNumber]));
  aModifiedCard->GetCellularNumber(getter_Copies(properties[index_MobileNumber]));
  aModifiedCard->GetHomeCity(getter_Copies(properties[index_HomeCity]));
  aModifiedCard->GetHomeState(getter_Copies(properties[index_HomeState]));
  aModifiedCard->GetHomeZipCode(getter_Copies(properties[index_HomeZip]));
  aModifiedCard->GetHomeCountry(getter_Copies(properties[index_HomeCountry]));
  aModifiedCard->GetWorkCity(getter_Copies(properties[index_WorkCity]));
  aModifiedCard->GetWorkState(getter_Copies(properties[index_WorkState]));
  aModifiedCard->GetWorkZipCode(getter_Copies(properties[index_WorkZip]));
  aModifiedCard->GetWorkCountry(getter_Copies(properties[index_WorkCountry]));
  aModifiedCard->GetJobTitle(getter_Copies(properties[index_JobTitle]));
  aModifiedCard->GetDepartment(getter_Copies(properties[index_Department]));
  aModifiedCard->GetCompany(getter_Copies(properties[index_Company]));
  aModifiedCard->GetWebPage1(getter_Copies(properties[index_WorkWebPage]));
  aModifiedCard->GetWebPage2(getter_Copies(properties[index_HomeWebPage]));
  aModifiedCard->GetNotes(getter_Copies(properties[index_Comments]));
  if (!mapiAddBook->SetPropertiesUString(*mMapiData, OutlookCardMAPIProps,
                                         index_LastProp, properties)) {
    PRINTF(("Cannot set general properties.\n")) ;
  }

  delete [] properties;
  nsXPIDLString unichar;
  nsXPIDLString unichar2;
  WORD year = 0;
  WORD month = 0;
  WORD day = 0;

  aModifiedCard->GetHomeAddress(getter_Copies(unichar));
  aModifiedCard->GetHomeAddress2(getter_Copies(unichar2));

  utility.Assign(unichar.get());
  if (!utility.IsEmpty())
    utility.AppendWithConversion(CRLF);

  utility.Append(unichar2.get());
  if (!mapiAddBook->SetPropertyUString(*mMapiData, PR_HOME_ADDRESS_STREET_W, utility.get())) {
    PRINTF(("Cannot set home address.\n")) ;
  }

  aModifiedCard->GetWorkAddress(getter_Copies(unichar));
  aModifiedCard->GetWorkAddress2(getter_Copies(unichar2));

  utility.Assign(unichar.get());
  if (!utility.IsEmpty())
    utility.AppendWithConversion(CRLF);

  utility.Append(unichar2.get());
  if (!mapiAddBook->SetPropertyUString(*mMapiData, PR_BUSINESS_ADDRESS_STREET_W, utility.get())) {
    PRINTF(("Cannot set work address.\n")) ;
  }

  aModifiedCard->GetBirthYear(getter_Copies(unichar));
  UnicodeToWord(unichar.get(), year);
  aModifiedCard->GetBirthMonth(getter_Copies(unichar));
  UnicodeToWord(unichar.get(), month);
  aModifiedCard->GetBirthDay(getter_Copies(unichar));
  UnicodeToWord(unichar.get(), day);
  if (!mapiAddBook->SetPropertyDate(*mMapiData, PR_BIRTHDAY, year, month, day)) {
    PRINTF(("Cannot set date.\n")) ;
  }

  return retCode;
}
