/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Seth Spitzer <sspitzer@netscape.com>
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
#include "nsIAddressBook.h"

NS_IMPL_ISUPPORTS1(nsAbAddressCollecter, nsIAbAddressCollecter)

#define PREF_MAIL_COLLECT_ADDRESSBOOK "mail.collect_addressbook"

nsAbAddressCollecter::nsAbAddressCollecter()
{
}

nsAbAddressCollecter::~nsAbAddressCollecter()
{
  if (m_database) {
    m_database->Commit(nsAddrDBCommitType::kSessionCommit);
    m_database->Close(PR_FALSE);
    m_database = nsnull;
  }
}

NS_IMETHODIMP nsAbAddressCollecter::CollectUnicodeAddress(const PRUnichar *aAddress, PRBool aCreateCard, PRUint32 aSendFormat)
{
  NS_ENSURE_ARG_POINTER(aAddress);
  // convert the unicode string to UTF-8...
  nsresult rv = CollectAddress(NS_ConvertUCS2toUTF8(aAddress).get(), aCreateCard, aSendFormat);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsAbAddressCollecter::GetCardFromAttribute(const char *aName, const char *aValue, nsIAbCard **aCard)
{
  NS_ENSURE_ARG_POINTER(aCard);
  return m_database->GetCardFromAttribute(m_directory, aName, aValue, PR_FALSE /* retain case */, aCard);
}

NS_IMETHODIMP nsAbAddressCollecter::CollectAddress(const char *aAddress, PRBool aCreateCard, PRUint32 aSendFormat)
{
  // note that we're now setting the whole recipient list,
  // not just the pretty name of the first recipient.
  PRUint32 numAddresses;
  char *names;
  char *addresses;

  nsresult rv;
  nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = pHeader->ParseHeaderAddresses(nsnull, aAddress, &names, &addresses, &numAddresses);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to parse, so can't collect");
  if (NS_FAILED(rv))
    return NS_OK;

  char *curName = names;
  char *curAddress = addresses;
  char *excludeDomainList = nsnull;

  for (PRUint32 i = 0; i < numAddresses; i++)
  {
    nsXPIDLCString unquotedName;
    rv = pHeader->UnquotePhraseOrAddr(curName, PR_FALSE, getter_Copies(unquotedName));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to unquote name");
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr <nsIAbCard> existingCard;
    nsCOMPtr <nsIAbCard> cardInstance;

    // Please DO NOT change the 3rd param of GetCardFromAttribute() call to 
    // PR_TRUE (ie, case insensitive) without reading bugs #128535 and #121478.
    rv = GetCardFromAttribute(kPriEmailColumn, curAddress, getter_AddRefs(existingCard));
    if (!existingCard && aCreateCard)
    {
      nsCOMPtr<nsIAbCard> senderCard = do_CreateInstance(NS_ABCARDPROPERTY_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv) && senderCard)
      {
        PRBool modifiedCard;
        rv = SetNamesForCard(senderCard, unquotedName.get(), &modifiedCard);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set names");

        rv = AutoCollectScreenName(senderCard, curAddress, &modifiedCard);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set screenname");

        rv = senderCard->SetPrimaryEmail(NS_ConvertASCIItoUCS2(curAddress).get());
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set email");

        if (aSendFormat != nsIAbPreferMailFormat::unknown)
        {
          rv = senderCard->SetPreferMailFormat(aSendFormat);
          NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remember preferred mail format");
        }

        rv = AddCardToAddressBook(senderCard);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to add card");
      }
    }
    else if (existingCard) { 
      // address is already in the AB, so update the names
      PRBool setNames = PR_FALSE;
      rv = SetNamesForCard(existingCard, unquotedName.get(), &setNames);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set names");

      PRBool setScreenName = PR_FALSE; 
      rv = AutoCollectScreenName(existingCard, curAddress, &setScreenName);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to set screen name");

      PRBool setPreferMailFormat = PR_FALSE; 
      if (aSendFormat != nsIAbPreferMailFormat::unknown)
      {
        PRUint32 currentFormat;
        rv = existingCard->GetPreferMailFormat(&currentFormat);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get preferred mail format");

        // we only want to update the AB if the current format is unknown
        if (currentFormat == nsIAbPreferMailFormat::unknown) 
        {
          rv = existingCard->SetPreferMailFormat(aSendFormat);
          NS_ASSERTION(NS_SUCCEEDED(rv), "failed to remember preferred mail format");
          setPreferMailFormat = PR_TRUE;
        }
      }

      if (setScreenName || setNames || setPreferMailFormat)
        existingCard->EditCardToDatabase(m_abURI.get());
    }

    curName += strlen(curName) + 1;
    curAddress += strlen(curAddress) + 1;
  } 

  PR_FREEIF(addresses);
  PR_FREEIF(names);
  PR_FREEIF(excludeDomainList);
  return NS_OK;
}

nsresult nsAbAddressCollecter::AutoCollectScreenName(nsIAbCard *aCard, const char *aEmail, PRBool *aModifiedCard)
{
  NS_ENSURE_ARG_POINTER(aCard);
  NS_ENSURE_ARG_POINTER(aEmail);
  NS_ENSURE_ARG_POINTER(aModifiedCard);

  *aModifiedCard = PR_FALSE;

  nsXPIDLString screenName;
  nsresult rv = aCard->GetAimScreenName(getter_Copies(screenName));
  NS_ENSURE_SUCCESS(rv,rv);

  // don't override existing screennames
  if (!screenName.IsEmpty())
    return NS_OK;

  const char *atPos = strchr(aEmail, '@');
  
  if (!atPos)
    return NS_OK;
  
  const char *domain = atPos + 1;
 
  if (!domain)
    return NS_OK;
 
  // username in 
  // username@aol.com (America Online)
  // username@cs.com (Compuserve)
  // username@netscape.net (Netscape webmail)
  // are all AIM screennames.  autocollect that info.
  if (strcmp(domain,"aol.com") && 
      strcmp(domain,"cs.com") && 
      strcmp(domain,"netscape.net"))
    return NS_OK;

  NS_ConvertASCIItoUTF16 userName(Substring(aEmail, atPos));

  rv = aCard->SetAimScreenName(userName.get());
  NS_ENSURE_SUCCESS(rv,rv);
  
  *aModifiedCard = PR_TRUE;
  return rv;
}


nsresult 
nsAbAddressCollecter::SetNamesForCard(nsIAbCard *senderCard, const char *fullName, PRBool *aModifiedCard)
{
  char *firstName = nsnull;
  char *lastName = nsnull;
  *aModifiedCard = PR_FALSE;

  nsXPIDLString displayName;
  nsresult rv = senderCard->GetDisplayName(getter_Copies(displayName));
  NS_ENSURE_SUCCESS(rv,rv);

  // we already have a display name, so don't do anything
  if (!displayName.IsEmpty())
    return NS_OK;

  senderCard->SetDisplayName(NS_ConvertUTF8toUCS2(fullName).get());
  *aModifiedCard = PR_TRUE;

  rv = SplitFullName(fullName, &firstName, &lastName);
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

nsresult nsAbAddressCollecter::SplitFullName(const char *fullName, char **firstName, char **lastName)
{
  if (fullName)
  {
    *firstName = nsCRT::strdup(fullName);
    if (!*firstName)
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
      *lastName = nsCRT::strdup(plastName);
    }
  }

  return NS_OK;
}

int PR_CALLBACK 
nsAbAddressCollecter::collectAddressBookPrefChanged(const char *aNewpref, void *aData)
{
  nsresult rv;
  nsAbAddressCollecter *adCol = (nsAbAddressCollecter *) aData;
  nsCOMPtr<nsIPref> pPref = do_GetService(NS_PREF_CONTRACTID, &rv); 
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get prefs");

  nsXPIDLCString prefVal;
  rv = pPref->GetCharPref(PREF_MAIL_COLLECT_ADDRESSBOOK, getter_Copies(prefVal));
  rv = adCol->SetAbURI((NS_FAILED(rv) || prefVal.IsEmpty()) ? kPersonalAddressbookUri : prefVal.get());
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to change collected ab");
  return 0;
}

nsresult nsAbAddressCollecter::Init(void)
{
  nsresult rv;
  nsCOMPtr<nsIPref> pPref = do_GetService(NS_PREF_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = pPref->RegisterCallback(PREF_MAIL_COLLECT_ADDRESSBOOK, collectAddressBookPrefChanged, this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString prefVal;
  rv = pPref->GetCharPref(PREF_MAIL_COLLECT_ADDRESSBOOK, getter_Copies(prefVal));
  rv = SetAbURI((NS_FAILED(rv) || prefVal.IsEmpty()) ? kPersonalAddressbookUri : prefVal.get());
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

nsresult nsAbAddressCollecter::AddCardToAddressBook(nsIAbCard *card)
{
  NS_ENSURE_ARG_POINTER(card);

  nsCOMPtr <nsIAbCard> addedCard;
  nsresult rv = m_directory->AddCard(card, getter_AddRefs(addedCard));
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

nsresult nsAbAddressCollecter::SetAbURI(const char *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  if (!strcmp(aURI,m_abURI.get()))
    return NS_OK;

  if (m_database) {
    m_database->Commit(nsAddrDBCommitType::kSessionCommit);
    m_database->Close(PR_FALSE);
    m_database = nsnull;
  }
  
  m_directory = nsnull;
  m_abURI = aURI;

  nsresult rv;
  nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = addressBook->GetAbDatabaseFromURI(m_abURI.get(), getter_AddRefs(m_database));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(m_abURI, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  m_directory = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}
