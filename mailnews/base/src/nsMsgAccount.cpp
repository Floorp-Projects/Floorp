/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "prprf.h"
#include "plstr.h"
#include "prmem.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIPref.h"
#include "nsMsgBaseCID.h"
#include "nsMsgAccount.h"
#include "nsIMsgAccount.h"
#include "nsIMsgFolderCache.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"

static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_ISUPPORTS1(nsMsgAccount, nsIMsgAccount)

nsMsgAccount::nsMsgAccount():
  m_prefs(0),
  m_incomingServer(null_nsCOMPtr()),
  m_defaultIdentity(null_nsCOMPtr())
{

  NS_INIT_REFCNT();
}

nsMsgAccount::~nsMsgAccount()
{

  // release of servers an identites happen automatically
  // thanks to nsCOMPtrs and nsISupportsArray
  if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);  
  
}

NS_IMETHODIMP 
nsMsgAccount::Init()
{
	NS_ASSERTION(!m_identities, "don't call Init twice!");
	if (m_identities) return NS_ERROR_FAILURE;

	return createIdentities();
}

nsresult
nsMsgAccount::getPrefService() {

  if (m_prefs) return NS_OK;
  
  return nsServiceManager::GetService(kPrefServiceCID,
                                      NS_GET_IID(nsIPref),
                                      (nsISupports**)&m_prefs);
}

NS_IMETHODIMP
nsMsgAccount::GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer)
{
  NS_ENSURE_ARG_POINTER(aIncomingServer);

  nsresult rv = NS_OK;
  // create the incoming server lazily
  if (!m_incomingServer) {
    // ignore the error (and return null), but it's still bad so assert
    rv = createIncomingServer();
    NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't lazily create the server\n");
  }
  
  
  *aIncomingServer = m_incomingServer;
  NS_IF_ADDREF(*aIncomingServer);

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
  rv = m_prefs->CopyCharPref(serverKeyPref, getter_Copies(serverKey));
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
  rv = m_prefs->CopyCharPref(serverTypePref, getter_Copies(serverType));

  // the server type doesn't exist, use "generic"
  if (NS_FAILED(rv)) {
    serverType.Adopt(nsCRT::strdup("generic"));
  }
    
#ifdef DEBUG_alecf
  if (NS_FAILED(rv)) {
    printf("\tCould not read pref %s\n", (const char*)serverTypePref);
  } else {
    printf("\t%s's   type: %s\n",
           (const char*)m_accountKey, (const char*)serverType);
  }
#endif
    
  // get the server from the account manager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
    
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = accountManager->GetIncomingServer(serverKey, getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  
#ifdef DEBUG_alecf
  printf("%s loaded.\n", (const char*)m_accountKey);
#endif
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
    m_prefs->SetCharPref(serverPrefName, key);
  }

  m_incomingServer = dont_QueryInterface(aIncomingServer);

  nsCOMPtr<nsIMsgAccountManager> accountManager =
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
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
  
  rv = m_prefs->CopyCharPref(identitiesKeyPref, getter_Copies(identityKey));

  if (NS_FAILED(rv)) return rv;
  
#ifdef DEBUG_alecf
  printf("%s's identities: %s\n",
         (const char*)m_accountKey,
         (const char*)identityKey);
#endif
  
  // get the server from the account manager
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
    
    
  // XXX todo: iterate through identities. for now, assume just one
  nsCOMPtr<nsIMsgIdentity> identity;
  rv = accountManager->GetIdentity(identityKey, getter_AddRefs(identity));
  if (NS_FAILED(rv)) return rv;

  rv = AddIdentity(identity);

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
  
  m_defaultIdentity = dont_QueryInterface(aDefaultIdentity);
  return NS_OK;
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
    
    m_prefs->SetCharPref(identitiesKeyPref, key);
  }
  
  NS_ASSERTION(m_identities,"you never called Init()");
  if (!m_identities) return NS_ERROR_FAILURE;  

  m_identities->AppendElement(identity);
  if (!m_defaultIdentity)
    SetDefaultIdentity(identity);
  
  return NS_OK;
}

/* void removeIdentity (in nsIMsgIdentity identity); */
NS_IMETHODIMP
nsMsgAccount::RemoveIdentity(nsIMsgIdentity *identity)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_GETTER_STR(nsMsgAccount::GetKey, m_accountKey);

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
  nsAutoString val; val.AssignWithConversion("[nsIMsgAccount: ");
  val.AppendWithConversion(m_accountKey);
  val.AppendWithConversion("]");
  *aResult = val.ToNewUnicode();
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::ClearAllValues()
{
    nsresult rv;
    nsCAutoString rootPref("mail.account.");
    rootPref += m_accountKey;

    rv = m_prefs->EnumerateChildren(rootPref, clearPrefEnum, (void *)m_prefs);

    return rv;
}

void
nsMsgAccount::clearPrefEnum(const char *aPref, void *aClosure)
{
    nsIPref *prefs = (nsIPref *)aClosure;
    prefs->ClearUserPref(aPref);
}
