/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kMsgHeaderParserCID,		NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS(nsAbAddressCollecter, nsCOMTypeInfo<nsIAbAddressCollecter>::GetIID());

nsAbAddressCollecter::nsAbAddressCollecter()
{
	NS_INIT_REFCNT();
}

nsAbAddressCollecter::~nsAbAddressCollecter()
{
	if (m_historyAB)
	{
		m_historyAB->Commit(kSessionCommit);
		m_historyAB->Close(PR_FALSE);
	}
}

NS_IMETHODIMP nsAbAddressCollecter::CollectAddress(const char *address)
{
	nsresult rv;
	PRBool collectAddresses = PR_TRUE;

    NS_WITH_SERVICE(nsIPref, pPref, kPrefCID, &rv); 
    if (NS_FAILED(rv) || !pPref) 
		return NS_ERROR_FAILURE;

    rv = pPref->GetBoolPref("mail.collect_email_address", &collectAddresses);
	if (!NS_SUCCEEDED(rv) || !collectAddresses)
		return rv;

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

	nsCOMPtr<nsIMsgHeaderParser>  pHeader;

	nsresult res = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                                    NULL, nsIMsgHeaderParser::GetIID(), 
                                                    (void **) getter_AddRefs(pHeader)); 

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
					rv = nsComponentManager::CreateInstance(kAbCardPropertyCID, nsnull, nsCOMTypeInfo<nsIAbCard>::GetIID(), getter_AddRefs(senderCard));
					if (NS_SUCCEEDED(rv) && senderCard)
					{
						if (curName && nsCRT::strlen(curName) > 0)
						{
							SetNamesForCard(senderCard, curName);
						}
						else
						{
							nsCAutoString senderFromEmail(curAddress);
							PRInt32 atSignIndex = senderFromEmail.FindChar('@');
							if (atSignIndex > 0)
							{
								senderFromEmail.Truncate(atSignIndex + 1);
								senderCard->SetDisplayName(senderFromEmail);
							}
						}
						senderCard->SetPrimaryEmail(curAddress);
						senderCard->AddCardToDatabase("abdirectory://history.mab");
					}
				}
				else
				{
					SetNamesForCard(existingCard, curName);
					existingCard->EditCardToDatabase("abdirectory://history.mab");
				}
			}
			curName += strlen(curName) + 1;
			curAddress += strlen(curAddress) + 1;
		}
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
		(*dbPath) += "history.mab";

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, aDatabase, PR_TRUE);
	}
	NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if (NS_FAILED(rv)) 
		return rv;

	nsCOMPtr <nsIRDFResource> resource;
	rv = rdfService->GetResource("abdirectory://history.mab", getter_AddRefs(resource));
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

	char *displayName = nsnull;

	nsresult rv = senderCard->GetFirstName(&firstName);
	if (NS_SUCCEEDED(rv) && firstName)
	{
	}
	senderCard->SetDisplayName((char *) fullName);
	rv = SplitFullName (fullName, &firstName, &lastName);
	if (NS_SUCCEEDED(rv))
	{
		senderCard->SetFirstName(firstName);
		senderCard->SetLastName(lastName);
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
