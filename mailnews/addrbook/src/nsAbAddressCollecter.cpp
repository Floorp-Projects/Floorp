/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsReadableUtils.h"
#include "prmem.h"
  
NS_IMPL_ISUPPORTS1(nsAbAddressCollecter, nsIAbAddressCollecter)

#define PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT "mail.collect_email_address_enable_size_limit"
#define PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT "mail.collect_email_address_size_limit"

nsAbAddressCollecter::nsAbAddressCollecter()
{
	NS_INIT_ISUPPORTS();

	m_maxCABsize = -1;
	m_sizeLimitEnabled = PR_FALSE;
}

nsAbAddressCollecter::~nsAbAddressCollecter()
{
	if (m_historyAB)
	{
		m_historyAB->Commit(nsAddrDBCommitType::kSessionCommit);
		m_historyAB->Close(PR_FALSE);
		m_historyAB = nsnull;
	}
}

NS_IMETHODIMP nsAbAddressCollecter::CollectUnicodeAddress(const PRUnichar * aAddress)
{
  NS_ENSURE_ARG(aAddress);
  nsresult rv = NS_OK;

  // convert the unicode string to UTF-8...
  nsAutoString unicodeString (aAddress);
  char * utf8Version = ToNewUTF8String(unicodeString);
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

  nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
  NS_ENSURE_SUCCESS(rv, rv);

	if (!m_historyAB)
	{
		rv = OpenHistoryAB(getter_AddRefs(m_historyAB));
		if (NS_FAILED(rv) || !m_historyAB)
			return rv;
	}
	// note that we're now setting the whole recipient list,
	// not just the pretty name of the first recipient.
	PRUint32 numAddresses;
	char	*names;
	char	*addresses;

  nsresult res = NS_OK;
	nsCOMPtr<nsIMsgHeaderParser>  pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);

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
				nsCOMPtr <nsIAbCard> cardInstance;

        // Please DO NOT change the 3rd param of GetCardFromAttribute() call to 
        // PR_TRUE (ie, case insensitive) without reading bugs #128535 and #121478.
				rv = m_historyAB->GetCardFromAttribute(m_historyDirectory, kPriEmailColumn, curAddress, PR_FALSE /* retain case */, getter_AddRefs(existingCard));

				if (!existingCard)
				{
					nsCOMPtr<nsIAbCard> senderCard = do_CreateInstance(NS_ABCARDPROPERTY_CONTRACTID, &rv);
					if (NS_SUCCEEDED(rv) && senderCard)
					{
						if (curName && strlen(curName) > 0)
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
								senderCard->SetDisplayName(senderFromEmail.get());
							}
						}
						nsAutoString email; email.AssignWithConversion(curAddress);
						senderCard->SetPrimaryEmail(email.get());

						rv = AddCardToCollectedAddressBook(senderCard);
            NS_ENSURE_SUCCESS(rv,rv);
					}
				}
				else //address is already in the CAB
				{
					if (m_sizeLimitEnabled) 
					{
            // XXX todo
            // there has to be a better way to do this, without deleting
            // and adding the card.  perhaps using modified time
            // this doesn't seem like the best way to do LRU
						m_historyAB->DeleteCard(existingCard, PR_TRUE /* notify */);
						SetNamesForCard(existingCard, curName);
						//append it to the bottom.
            rv = AddCardToCollectedAddressBook(existingCard);
            NS_ENSURE_SUCCESS(rv,rv);
					}
					else
					{
						SetNamesForCard(existingCard, curName);
						existingCard->EditCardToDatabase(kCollectedAddressbookUri);
					}
				}

				if (m_sizeLimitEnabled) 
				{
					PRUint32 count = 0;
					rv = m_historyAB->GetCardCount( &count );

					if( count > (PRUint32)m_maxCABsize )
						rv = m_historyAB->RemoveExtraCardsInCab(count, m_maxCABsize);

				} //if m_sizeLimitEnabled
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

	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
		(*dbPath) += kCollectedAddressbook;

		nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
		         do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
                {
			rv = addrDBFactory->Open(dbPath, PR_TRUE, aDatabase, PR_TRUE);
                        if (NS_FAILED(rv))
                        {
                          // blow away corrupt db's
                          dbPath->Delete(PR_FALSE);
                        }
                }
		delete dbPath;
	}
	nsCOMPtr<nsIRDFService> rdfService(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr <nsIRDFResource> resource;
	rv = rdfService->GetResource(kCollectedAddressbookUri, getter_AddRefs(resource));
	NS_ENSURE_SUCCESS(rv, rv);

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

                // XXX todo, fix this leak
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

	senderCard->SetDisplayName(NS_ConvertUTF8toUCS2(fullName).get());

	nsresult rv = SplitFullName (fullName, &firstName, &lastName);
	if (NS_SUCCEEDED(rv))
	{
		senderCard->SetFirstName(NS_ConvertUTF8toUCS2(firstName).get());
	  
    if (lastName)
      senderCard->SetLastName(NS_ConvertUTF8toUCS2(lastName).get());
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
nsAbAddressCollecter::collectEmailAddressEnableSizeLimitPrefChanged(const char *newpref, void *data)
{
	nsresult rv;
	nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) data;
	nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
	if(NS_FAILED(pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, &adCol->m_sizeLimitEnabled))){
		adCol->m_sizeLimitEnabled = PR_TRUE;
	}
	return 0;
}

int PR_CALLBACK 
nsAbAddressCollecter::collectEmailAddressSizeLimitPrefChanged(const char *newpref, void *data)
{
	nsresult rv;
	nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) data;
	nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
	if(NS_FAILED(pPref->GetIntPref(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, &adCol->m_maxCABsize))){
		adCol->m_maxCABsize = 0;
	}
	return 0;
}

nsresult nsAbAddressCollecter::Init(void)
{
	nsresult rv;
	nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
	NS_ENSURE_SUCCESS(rv,rv);
  
	rv = pPref->RegisterCallback(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, collectEmailAddressEnableSizeLimitPrefChanged, this);
	NS_ENSURE_SUCCESS(rv,rv);
  
  rv = pPref->RegisterCallback(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, collectEmailAddressSizeLimitPrefChanged, this);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = pPref->GetBoolPref(PREF_COLLECT_EMAIL_ADDRESS_ENABLE_SIZE_LIMIT, &m_sizeLimitEnabled);
	NS_ENSURE_SUCCESS(rv,rv);

  rv = pPref->GetIntPref(PREF_COLLECT_EMAIL_ADDRESS_SIZE_LIMIT, &m_maxCABsize);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAbAddressCollecter::AddCardToCollectedAddressBook(nsIAbCard *card)
{
  NS_ENSURE_ARG_POINTER(card);

  nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(kCollectedAddressbookUri, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIAbCard> addedCard;
	rv = directory->AddCard(card, getter_AddRefs(addedCard));
  NS_ENSURE_SUCCESS(rv,rv);
	return rv;
}
