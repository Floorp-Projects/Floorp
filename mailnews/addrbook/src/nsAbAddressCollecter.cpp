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
 *  Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIAddressBook.h"

NS_IMPL_ISUPPORTS1(nsAbAddressCollecter, nsIAbAddressCollecter)

#define PREF_MAIL_COLLECT_ADDRESSBOOK "mail.collect_addressbook"

nsAbAddressCollecter::nsAbAddressCollecter()
{
  NS_INIT_ISUPPORTS();
}

nsAbAddressCollecter::~nsAbAddressCollecter()
{
  if (m_database) {
    m_database->Commit(nsAddrDBCommitType::kSessionCommit);
    m_database->Close(PR_FALSE);
    m_database = nsnull;
  }
}

NS_IMETHODIMP nsAbAddressCollecter::CollectUnicodeAddress(const PRUnichar * aAddress)
{
  NS_ENSURE_ARG_POINTER(aAddress);
  nsresult rv = NS_OK;

  // convert the unicode string to UTF-8...
  nsAutoString unicodeString (aAddress);
  char * utf8Version = ToNewUTF8String(unicodeString);
  if (utf8Version) {
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

  rv = OpenDatabase();
  NS_ENSURE_SUCCESS(rv, rv);

  // note that we're now setting the whole recipient list,
  // not just the pretty name of the first recipient.
  PRUint32 numAddresses;
  char *names;
  char *addresses;

  nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = pHeader->ParseHeaderAddresses(nsnull, address, &names, &addresses, &numAddresses);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to parse, so can't collect");
  if (NS_FAILED(rv))
    return NS_OK;

  char *curName = names;
  char *curAddress = addresses;
  char *excludeDomainList = nsnull;

  for (PRUint32 i = 0; i < numAddresses; i++)
  {
    nsCOMPtr <nsIAbCard> existingCard;
    nsCOMPtr <nsIAbCard> cardInstance;

    // Please DO NOT change the 3rd param of GetCardFromAttribute() call to 
    // PR_TRUE (ie, case insensitive) without reading bugs #128535 and #121478.
    rv = m_database->GetCardFromAttribute(m_directory, kPriEmailColumn, curAddress, PR_FALSE /* retain case */, getter_AddRefs(existingCard));

    if (!existingCard)
    {
      nsCOMPtr<nsIAbCard> senderCard = do_CreateInstance(NS_ABCARDPROPERTY_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv) && senderCard)
      {
        if (curName && strlen(curName) > 0)
          SetNamesForCard(senderCard, curName);
        else
        {
          nsAutoString senderFromEmail(NS_ConvertASCIItoUCS2(curAddress).get());
          PRInt32 atSignIndex = senderFromEmail.FindChar('@');
          if (atSignIndex > 0)
          {
            senderFromEmail.Truncate(atSignIndex);
            senderCard->SetDisplayName(senderFromEmail.get());
          }
        }

        senderCard->SetPrimaryEmail(NS_ConvertASCIItoUCS2(curAddress).get());

        rv = AddCardToAddressBook(senderCard);
        NS_ENSURE_SUCCESS(rv,rv);
      }
    }
    else // address is already in the AB
    {
      SetNamesForCard(existingCard, curName);
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

nsresult nsAbAddressCollecter::OpenDatabase()
{
  // check if already open
  if (m_database)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIAddrBookSession> abSession = 
           do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAddressBook> addressBook = do_GetService(NS_ADDRESSBOOK_CONTRACTID
, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = addressBook->GetAbDatabaseFromURI(m_abURI.get(), getter_AddRefs(m_database));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIRDFResource> resource;
  rv = rdfService->GetResource(m_abURI.get(), getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  m_directory = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult 
nsAbAddressCollecter::SetNamesForCard(nsIAbCard *senderCard, const char *fullName)
{
  char *firstName = nsnull;
  char *lastName = nsnull;

  senderCard->SetDisplayName(NS_ConvertUTF8toUCS2(fullName).get());

  nsresult rv = SplitFullName(fullName, &firstName, &lastName);
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
  nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get prefs");

  nsXPIDLCString prefVal;
  rv = pPref->GetCharPref(PREF_MAIL_COLLECT_ADDRESSBOOK, getter_Copies(prefVal));
  rv = adCol->SetAbURI((NS_FAILED(rv) || prefVal.IsEmpty()) ? kPersonalAddressbook : prefVal.get());
  NS_ASSERTION(NS_SUCCEEDED(rv),"failed to change collected ab");
  return 0;
}

nsresult nsAbAddressCollecter::Init(void)
{
  nsresult rv;
  nsCOMPtr<nsIPref> pPref(do_GetService(NS_PREF_CONTRACTID, &rv)); 
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = pPref->RegisterCallback(PREF_MAIL_COLLECT_ADDRESSBOOK, collectAddressBookPrefChanged, this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString prefVal;
  rv = pPref->GetCharPref(PREF_MAIL_COLLECT_ADDRESSBOOK, getter_Copies(prefVal));
  rv = SetAbURI((NS_FAILED(rv) || prefVal.IsEmpty()) ? kPersonalAddressbook : prefVal.get());
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
  if (m_database) {
    m_database->Commit(nsAddrDBCommitType::kSessionCommit);
    m_database->Close(PR_FALSE);
    m_database = nsnull;
  }
  
  m_directory = nsnull;
  m_abURI = aURI;

  nsresult rv = OpenDatabase();
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}
