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
 * Portions created by the Initial Developer are Copyright (C) 1998
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


#include "prprf.h"
#include "plstr.h"
#include "prmem.h"
#include "nsISupportsObsolete.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsMsgBaseCID.h"
#include "nsMsgAccount.h"
#include "nsIMsgAccount.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"

NS_IMPL_ISUPPORTS1(nsMsgAccount, nsIMsgAccount)

nsMsgAccount::nsMsgAccount()
{
}

nsMsgAccount::~nsMsgAccount()
{
}

NS_IMETHODIMP 
nsMsgAccount::Init()
{
	NS_ASSERTION(!m_identities, "don't call Init twice!");
	if (m_identities) return NS_ERROR_FAILURE;

	return createIdentities();
}

nsresult
nsMsgAccount::getPrefService() 
{
  if (m_prefs) 
    return NS_OK;

  nsresult rv;
  m_prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    m_prefs = nsnull;

  return rv;
}

NS_IMETHODIMP
nsMsgAccount::GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer)
{
  NS_ENSURE_ARG_POINTER(aIncomingServer);

  // create the incoming server lazily
  if (!m_incomingServer) {
    // ignore the error (and return null), but it's still bad so assert
    nsresult rv = createIncomingServer();
    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't lazily create the server\n");
  }
  
  NS_IF_ADDREF(*aIncomingServer = m_incomingServer);

  return NS_OK;
}

nsresult
nsMsgAccount::createIncomingServer()
{
  if (!(const char*)m_accountKey) return NS_ERROR_NOT_INITIALIZED;
  // from here, load mail.account.myaccount.server
  // Load the incoming server
  //
  // ex) mail.account.myaccount.server = "myserver"

  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  // get the "server" pref
  nsCAutoString serverKeyPref("mail.account.");
  serverKeyPref += m_accountKey;
  serverKeyPref += ".server";
  nsXPIDLCString serverKey;
  rv = m_prefs->GetCharPref(serverKeyPref.get(), getter_Copies(serverKey));
  if (NS_FAILED(rv)) return rv;
    
#ifdef DEBUG_alecf
  printf("\t%s's server: %s\n", (const char*)m_accountKey, (const char*)serverKey);
#endif

  // get the servertype
  // ex) mail.server.myserver.type = imap
  nsCAutoString serverTypePref("mail.server.");
  serverTypePref.Append(serverKey);
  serverTypePref += ".type";
  
  nsXPIDLCString serverType;
  rv = m_prefs->GetCharPref(serverTypePref.get(), getter_Copies(serverType));

  // the server type doesn't exist, use "generic"
  if (NS_FAILED(rv)) {
    serverType.Adopt(nsCRT::strdup("generic"));
  }
    
    
  // get the server from the account manager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
    
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = accountManager->GetIncomingServer(serverKey, getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // store the server in this structure
  m_incomingServer = server;
  accountManager->NotifyServerLoaded(server);

  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::SetIncomingServer(nsIMsgIncomingServer * aIncomingServer)
{
  nsresult rv;
  
  nsXPIDLCString key;
  rv = aIncomingServer->GetKey(getter_Copies(key));
  
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString serverPrefName("mail.account.");
    serverPrefName.Append(m_accountKey);
    serverPrefName.Append(".server");
    m_prefs->SetCharPref(serverPrefName.get(), key);
  }

  m_incomingServer = aIncomingServer;

  PRBool serverValid;
  (void) aIncomingServer->GetValid(&serverValid);
  // only notify server loaded if server is valid so 
  // account manager only gets told about finished accounts.
  if (serverValid) 
  {
  nsCOMPtr<nsIMsgAccountManager> accountManager =
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    accountManager->NotifyServerLoaded(aIncomingServer);
  }
  return NS_OK;
}

/* nsISupportsArray GetIdentities (); */
NS_IMETHODIMP
nsMsgAccount::GetIdentities(nsISupportsArray **_retval)
{
  if (!_retval) return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(m_identities,"you never called Init()");
  if (!m_identities) return NS_ERROR_FAILURE;

  *_retval = m_identities;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/*
 * set up the m_identities array
 * do not call this more than once or we'll leak.
 */
nsresult
nsMsgAccount::createIdentities()
{
  NS_ASSERTION(!m_identities, "only call createIdentities() once!");
  if (m_identities) return NS_ERROR_FAILURE;

  NS_ENSURE_TRUE((const char*)m_accountKey, NS_ERROR_NOT_INITIALIZED);
  
  NS_NewISupportsArray(getter_AddRefs(m_identities));

  // get the pref
  // ex) mail.account.myaccount.identities = "joe-home,joe-work"
  nsCAutoString identitiesKeyPref("mail.account.");
  identitiesKeyPref.Append(m_accountKey);
  identitiesKeyPref.Append(".identities");
  
  nsXPIDLCString identityKey;
  nsresult rv;
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = m_prefs->GetCharPref(identitiesKeyPref.get(), getter_Copies(identityKey));

  if (NS_FAILED(rv)) return rv;
  if (identityKey.IsEmpty())    // not an error if no identities, but 
    return NS_OK;               // nsCRT::strtok will be unhappy
  
#ifdef DEBUG_alecf
  printf("%s's identities: %s\n",
         (const char*)m_accountKey,
         (const char*)identityKey);
#endif
  
  // get the server from the account manager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  // const-casting because nsCRT::strtok whacks the string,
  // but safe because identityKey is a copy
  char* newStr;
  char* rest = identityKey.BeginWriting();
  char* token = nsCRT::strtok(rest, ",", &newStr);

  // temporaries used inside the loop
  nsCOMPtr<nsIMsgIdentity> identity;
  nsCAutoString key;

  // iterate through id1,id2, etc
  while (token) {
    key = token;
    key.StripWhitespace();
    
    // create the account
    rv = accountManager->GetIdentity(key.get(), getter_AddRefs(identity));
    if (NS_SUCCEEDED(rv)) {
      // ignore error from addIdentityInternal() - if it fails, it fails.
      rv = addIdentityInternal(identity);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Couldn't create identity");
    }

    // advance to next key, if any
    token = nsCRT::strtok(newStr, ",", &newStr);
  }
    
  return rv;
}


/* attribute nsIMsgIdentity defaultIdentity; */
NS_IMETHODIMP
nsMsgAccount::GetDefaultIdentity(nsIMsgIdentity * *aDefaultIdentity)
{
  if (!aDefaultIdentity) return NS_ERROR_NULL_POINTER;
  nsresult rv;
  if (!m_identities) {
    rv = Init();
    if (NS_FAILED(rv)) return rv;
  }

  nsISupports* idsupports;
  rv = m_identities->GetElementAt(0, &idsupports);
  if (NS_FAILED(rv)) return rv;
  
  if (idsupports) {
      rv = idsupports->QueryInterface(NS_GET_IID(nsIMsgIdentity),
                                          (void **)aDefaultIdentity);
      NS_RELEASE(idsupports);
  }
  return rv;
}

// todo - make sure this is in the identity array!
NS_IMETHODIMP
nsMsgAccount::SetDefaultIdentity(nsIMsgIdentity * aDefaultIdentity)
{
  NS_ASSERTION(m_identities,"you never called Init()");
  if (!m_identities) return NS_ERROR_FAILURE;  
  
  NS_ASSERTION(m_identities->IndexOf(aDefaultIdentity) != -1, "Where did that identity come from?!");
  if (m_identities->IndexOf(aDefaultIdentity) == -1)
    return NS_ERROR_UNEXPECTED;
  
  m_defaultIdentity = aDefaultIdentity;
  return NS_OK;
}

// add the identity to m_identities, but don't fiddle with the
// prefs. The assumption here is that the pref for this identity is
// already set.
nsresult
nsMsgAccount::addIdentityInternal(nsIMsgIdentity *identity)
{
  NS_ASSERTION(m_identities,"you never called Init()");
  if (!m_identities) return NS_ERROR_FAILURE;  

  return m_identities->AppendElement(identity);
}


/* void addIdentity (in nsIMsgIdentity identity); */
NS_IMETHODIMP
nsMsgAccount::AddIdentity(nsIMsgIdentity *identity)
{
  // hack hack - need to add this to the list of identities.
  // for now just treat this as a Setxxx accessor
  // when this is actually implemented, don't refcount the default identity
  nsresult rv;
  
  nsXPIDLCString key;
  rv = identity->GetKey(getter_Copies(key));

  if (NS_SUCCEEDED(rv)) {

    nsCAutoString identitiesKeyPref("mail.account.");
    identitiesKeyPref.Append(m_accountKey);
    identitiesKeyPref.Append(".identities");
      
    nsXPIDLCString identityList;
    m_prefs->GetCharPref(identitiesKeyPref.get(),
                         getter_Copies(identityList));

    nsCAutoString newIdentityList(identityList);
    
    nsCAutoString testKey;      // temporary to strip whitespace
    PRBool foundIdentity = PR_FALSE; // if the input identity is found

    // nsCRT::strtok will be unhappy with an empty string
    if (!identityList.IsEmpty()) {
      
      // const-casting because nsCRT::strtok whacks the string,
      // but safe because identityList is a copy
      char *newStr;
      char *rest = identityList.BeginWriting();
      char *token = nsCRT::strtok(rest, ",", &newStr);
      
      // look for the identity key that we're adding
      while (token) {
        testKey = token;
        testKey.StripWhitespace();

        if (testKey.Equals(key))
          foundIdentity = PR_TRUE;

        token = nsCRT::strtok(newStr, ",", &newStr);
      }
    }

    // if it didn't already exist, append it
    if (!foundIdentity) {
      if (newIdentityList.IsEmpty())
        newIdentityList = key;
      else {
        newIdentityList.Append(',');
        newIdentityList.Append(key);
      }
    }
    
    m_prefs->SetCharPref(identitiesKeyPref.get(), newIdentityList.get());
  }

  // now add it to the in-memory list
  rv = addIdentityInternal(identity);
  
  if (!m_defaultIdentity)
    SetDefaultIdentity(identity);
  
  return rv;
}

/* void removeIdentity (in nsIMsgIdentity identity); */
NS_IMETHODIMP
nsMsgAccount::RemoveIdentity(nsIMsgIdentity * aIdentity)
{
  NS_ENSURE_TRUE(m_identities, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aIdentity);

  PRUint32 count =0;
  m_identities->Count(&count);

  NS_ENSURE_TRUE(count > 1, NS_ERROR_FAILURE); // you must have at least one identity

  nsXPIDLCString key;
  nsresult rv = aIdentity->GetKey(getter_Copies(key));

  // remove our identity
  m_identities->RemoveElement(aIdentity);
  count--;

  // clear out the actual pref values associated with the identity
  aIdentity->ClearAllValues();

  // if we just deleted the default identity, clear it out so we pick a new one
  if (m_defaultIdentity == aIdentity)
    m_defaultIdentity = nsnull;

  // now rebuild the identity pref
  nsCAutoString identitiesKeyPref("mail.account.");
  identitiesKeyPref.Append(m_accountKey);
  identitiesKeyPref.Append(".identities");
      
  nsCAutoString newIdentityList;

  // iterate over the remaining identities
  for (PRUint32 index = 0; index < count; index++)
  {
    nsCOMPtr<nsIMsgIdentity> identity = do_QueryElementAt(m_identities, index, &rv);
    if (identity)
    {
      identity->GetKey(getter_Copies(key));

      if (!index)
        newIdentityList = key;
      else
      {
        newIdentityList.Append(',');
        newIdentityList.Append(key);
      }
    }
  }

  m_prefs->SetCharPref(identitiesKeyPref.get(), newIdentityList.get());
 
  return rv;
}

NS_IMPL_GETTER_STR(nsMsgAccount::GetKey, m_accountKey)

nsresult
nsMsgAccount::SetKey(const char *accountKey)
{
  if (!accountKey) return NS_ERROR_NULL_POINTER;

  nsresult rv=NS_OK;

  // need the prefs service to do anything
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  *(char**)getter_Copies(m_accountKey) = PL_strdup(accountKey);
  
  return Init();
}

NS_IMETHODIMP
nsMsgAccount::ToString(PRUnichar **aResult)
{
  nsAutoString val(NS_LITERAL_STRING("[nsIMsgAccount: "));
  val.AppendWithConversion(m_accountKey);
  val.AppendLiteral("]");
  *aResult = ToNewUnicode(val);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::ClearAllValues()
{
    nsresult rv;
    nsCAutoString rootPref("mail.account.");
    rootPref += m_accountKey;

    rv = getPrefService();
    if (NS_FAILED(rv))
        return rv;

    PRUint32 cntChild, i;
    char **childArray;
 
    rv = m_prefs->GetChildList(rootPref.get(), &cntChild, &childArray);
    if (NS_SUCCEEDED(rv)) {
        for (i = 0; i < cntChild; i++)
            m_prefs->ClearUserPref(childArray[i]);

        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(cntChild, childArray);
    }

    return rv;
}
