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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"  // for pre-compiled headers

#include "nsIServiceManager.h"

#include "nsIAbCard.h"
#include "nsAbBaseCID.h"
#include "nsAbAddressCollecter.h"
#include "nsIPref.h"
#include "nsIAddrBookSession.h"
#include "nsIMsgHeaderParser.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
  
  // For the new pref API's
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAbCardPropertyCID, NS_ABCARDPROPERTY_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS(nsAbAddressCollecter, NS_GET_IID(nsIAbAddressCollecter));

static const char *PREF_COLLECT_EMAIL_ADDRESS = "mail.collect_email_address";
static const char *PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT = "mail.collect_email_address_enable_size_limit";
static const char *PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT = "mail.collect_email_address_size_limit";
#define OVERSIZED_CAB_LIMIT 2

nsAbAddressCollecter::nsAbAddressCollecter()
{
	NS_INIT_REFCNT();
	maxCABsize = -1;
	collectAddresses = -1;
	sizeLimitEnabled = -1;

	//set up the pref callbacks:
	registerPrefCallbacks();
}

nsAbAddressCollecter::~nsAbAddressCollecter()
{
	if (m_historyAB)
	{
		m_historyAB->Commit(kSessionCommit);
		m_historyAB->Close(PR_FALSE);
		m_historyAB = null_nsCOMPtr();
	}
}

NS_IMETHODIMP nsAbAddressCollecter::CollectUnicodeAddress(const PRUnichar * aAddress)
{
  NS_ENSURE_ARG(aAddress);
  nsresult rv = NS_OK;

  // convert the unicode string to UTF-8...
  nsAutoString unicodeString (aAddress);
  char * utf8Version = unicodeString.ToNewUTF8String();
  if (utf8Version)
  {
    rv = CollectAddress(utf8Version);
    Recycle(utf8Version);
  }

  return rv;
}

NS_IMETHODIMP nsAbAddressCollecter::CollectAddress(const char *address)
{
	nsresult rv;
	//collectAddresses = PR_TRUE;
	//sizeLimitEnabled = PR_TRUE;

	NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
	if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;

	if(collectAddresses == -1){
		rv = pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS, &collectAddresses);
		if (!NS_SUCCEEDED(rv) || !collectAddresses)
			return rv;
	}

	if(sizeLimitEnabled == -1){
		rv = pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, &sizeLimitEnabled);
		if (!NS_SUCCEEDED(rv))
			return rv;
	}

	//maxCABsize = 20;
	if(sizeLimitEnabled && maxCABsize == -1){
		PRInt32 max = 0;
    rv = pPref->GetIntPref(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, &max);
		if (!NS_SUCCEEDED(rv))
			return rv;
		maxCABsize = max;
	}

	if (!m_historyAB)
	{
		rv = OpenHistoryAB(getter_AddRefs(m_historyAB));
		if (!NS_SUCCEEDED(rv) || !m_historyAB)
			return rv;
	}
	// note that we're now setting the whole recipient list,
	// not just the pretty name of the first recipient.
	PRUint32 numAddresses;
	char	*names;
	char	*addresses;

  nsresult res = NS_OK;
	nsCOMPtr<nsIMsgHeaderParser>  pHeader = do_GetService(kMsgHeaderParserCID, &res);

	if (NS_FAILED(res)) return res;

	nsresult ret = pHeader->ParseHeaderAddresses (nsnull, address, &names, &addresses, &numAddresses);
	if (ret == NS_OK)
	{
		char *curName = names;
		char *curAddress = addresses;
		char *excludeDomainList = nsnull;

		for (PRUint32 i = 0; i < numAddresses; i++)
		{
			PRBool exclude;

			rv = IsDomainExcluded(curAddress, pPref, &exclude);
			if (NS_SUCCEEDED(rv) && !exclude)
			{
				nsCOMPtr <nsIAbCard> existingCard;

				rv = m_historyAB->GetCardForEmailAddress(m_historyDirectory, curAddress, getter_AddRefs(existingCard));

				if (!existingCard)
				{
					nsCOMPtr<nsIAbCard> senderCard;
					rv = nsComponentManager::CreateInstance(kAbCardPropertyCID, nsnull, NS_GET_IID(nsIAbCard), getter_AddRefs(senderCard));
					if (NS_SUCCEEDED(rv) && senderCard)
					{
						if (curName && nsCRT::strlen(curName) > 0)
						{
							SetNamesForCard(senderCard, curName);
						}
						else
						{
							nsAutoString senderFromEmail; senderFromEmail.AssignWithConversion(curAddress);
							PRInt32 atSignIndex = senderFromEmail.FindChar('@');
							if (atSignIndex > 0)
							{
								senderFromEmail.Truncate(atSignIndex);
								senderCard->SetDisplayName((PRUnichar*)senderFromEmail.GetUnicode());
							}
						}
						nsAutoString email; email.AssignWithConversion(curAddress);
						senderCard->SetPrimaryEmail((PRUnichar*)email.GetUnicode());
						senderCard->AddCardToDatabase(kCollectedAddressbookUri);
					}
				}
				else //address is already in the CAB
				{
					SetNamesForCard(existingCard, curName);
					existingCard->EditCardToDatabase(kCollectedAddressbookUri);

					if(sizeLimitEnabled) {
						//remove card from ab, and
						m_historyAB->DeleteCard( existingCard, PR_FALSE );
						SetNamesForCard(existingCard, curName);
						//append it to the bottom.
						existingCard->AddCardToDatabase(kCollectedAddressbookUri);
					}
				}

				if(sizeLimitEnabled) {
					PRUint32 count = 0;

					rv = m_historyAB->GetCardCount( &count );

					if( (count > (PRUint32)maxCABsize) && (count <= (PRUint32)maxCABsize + OVERSIZED_CAB_LIMIT) ){
						nsCOMPtr<nsISupports> obj = nsnull;
						nsIEnumerator *cardEnum = nsnull;
							
						m_historyAB->EnumerateCards(m_historyDirectory, &cardEnum);

						cardEnum->First();

						while( --count >= (PRUint32) maxCABsize ){
							cardEnum->CurrentItem(getter_AddRefs(obj));

							existingCard = do_QueryInterface(obj, &rv);

							m_historyAB->DeleteCard(existingCard, PR_FALSE);

							cardEnum->Next();
						}

						if(cardEnum)
							delete cardEnum;
					} 
				} //if sizeLimitEnabled
	
			}
			curName += strlen(curName) + 1;
			curAddress += strlen(curAddress) + 1;
		} //for
		PR_FREEIF(addresses);
		PR_FREEIF(names);
		PR_FREEIF(excludeDomainList);
	}

	return NS_OK;
}


nsresult nsAbAddressCollecter::OpenHistoryAB(nsIAddrDatabase **aDatabase)
{
	if (!aDatabase)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;

	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
		(*dbPath) += kCollectedAddressbook;

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, aDatabase, PR_TRUE);
		delete dbPath;
	}
	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if (NS_FAILED(rv)) 
		return rv;

	nsCOMPtr <nsIRDFResource> resource;
	rv = rdfService->GetResource(kCollectedAddressbookUri, getter_AddRefs(resource));
	if (NS_FAILED(rv)) 
		return rv;

	// query interface 
	m_historyDirectory = do_QueryInterface(resource, &rv);
	return rv;
}

nsresult nsAbAddressCollecter::IsDomainExcluded(const char *address, nsIPref *pPref, PRBool *bExclude)
{
	if (!bExclude)
		return NS_ERROR_NULL_POINTER;

	*bExclude = PR_FALSE;

	nsXPIDLCString excludedDomainList;
	nsresult rv = pPref->CopyCharPref("mail.address_collection_ignore_domain_list",
                                   getter_Copies(excludedDomainList));

	if (NS_FAILED(rv) || !excludedDomainList || !excludedDomainList[0]) 
		return NS_OK;

	nsCAutoString incomingDomain(address);
	PRInt32 atSignIndex = incomingDomain.RFindChar('@');
	if (atSignIndex > 0)
	{
		incomingDomain.Cut(0, atSignIndex + 1);

		char *token = nsnull;
		char *rest = NS_CONST_CAST(char*,(const char*)excludedDomainList);
		nsCAutoString str;

		token = nsCRT::strtok(rest, ",", &rest);
		while (token && *token) 
		{
			str = token;
			str.StripWhitespace();

			if (!str.IsEmpty()) 
			{
				if (str.Equals(incomingDomain))
				{
					*bExclude = PR_TRUE;
					break;
				}
			}
			str = "";
			token = nsCRT::strtok(rest, ",", &rest);
		}
	}
	return rv;
}

nsresult nsAbAddressCollecter::SetNamesForCard(nsIAbCard *senderCard, const char *fullName)
{
	char *firstName = nsnull;
	char *lastName = nsnull;
	PRUnichar *unicodeStr = nsnull;


	INTL_ConvertToUnicode((const char *)fullName, nsCRT::strlen(fullName), (void**)&unicodeStr);
	senderCard->SetDisplayName(unicodeStr);
	PR_Free(unicodeStr);
	nsresult rv = SplitFullName (fullName, &firstName, &lastName);
	if (NS_SUCCEEDED(rv))
	{
		INTL_ConvertToUnicode((const char *)firstName, nsCRT::strlen(firstName), (void**)&unicodeStr);
		senderCard->SetFirstName(unicodeStr);
		PR_Free(unicodeStr);
        if (lastName) {
            INTL_ConvertToUnicode((const char *)lastName, nsCRT::strlen(lastName), (void**)&unicodeStr);
            senderCard->SetLastName(unicodeStr);
            PR_Free(unicodeStr);
        }
	}
	PR_FREEIF(firstName);
	PR_FREEIF(lastName);
	return rv;
}

nsresult nsAbAddressCollecter::SplitFullName (const char *fullName, char **firstName, char **lastName)
{
    if (fullName)
    {
        *firstName = nsCRT::strdup(fullName);
        if (NULL == *firstName)
            return NS_ERROR_OUT_OF_MEMORY;

        char *plastSpace = *firstName;
        char *walkName = *firstName;
        char *plastName = nsnull;

        while (walkName && *walkName)
        {
            if (*walkName == ' ')
            {
                plastSpace = walkName;
                plastName = plastSpace + 1;
            }
            
            walkName++;
        }

        if (plastName) 
        {
            *plastSpace = '\0';
            *lastName = nsCRT::strdup (plastName);
        }
    }

    return NS_OK;
}

int PR_CALLBACK 
nsAbAddressCollecter::collectEmailAddressPrefChanged(const char *newpref, void *data){
	nsresult rv;
	nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) data;
	NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
	if(NS_FAILED(pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS, &adCol->collectAddresses))){
		adCol->collectAddresses = PR_TRUE;
	}

	return 0;
}

int PR_CALLBACK 
nsAbAddressCollecter::collectEmailAddressEnableSizeLimitPrefChanged(const char *newpref, void *data){
	nsresult rv;
	nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) data;
	NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
	if(NS_FAILED(pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, &adCol->sizeLimitEnabled))){
		adCol->sizeLimitEnabled = PR_TRUE;
	}

	
	return 0;
}

int PR_CALLBACK 
nsAbAddressCollecter::collectEmailAddressSizeLimitPrefChanged(const char *newpref, void *data){
	nsresult rv;
	PRInt32 max = 0;
	nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) data;
	NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
	if(NS_FAILED(pPref->GetIntPref(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, &max))){
		adCol->maxCABsize = 200;
	} else 
		adCol->maxCABsize = max;

	return 0;
}

void nsAbAddressCollecter::registerPrefCallbacks(void){
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
	pPref->RegisterCallback(PREF_COLLECT_EMAIL_ADDRESS, collectEmailAddressPrefChanged, this);
	pPref->RegisterCallback(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, collectEmailAddressEnableSizeLimitPrefChanged, this);
	pPref->RegisterCallback(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, collectEmailAddressSizeLimitPrefChanged, this);

}
