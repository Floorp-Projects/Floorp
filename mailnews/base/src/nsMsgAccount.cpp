/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


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

class nsMsgAccount : public nsIMsgAccount,
                     public nsIShutdownListener
{
  
public:
  nsMsgAccount();
  virtual ~nsMsgAccount();
   
  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  NS_IMETHOD GetKey(char ** aKey);
  NS_IMETHOD SetKey(char * aKey);
  
  NS_IMETHOD GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer);
  NS_IMETHOD SetIncomingServer(nsIMsgIncomingServer * aIncomingServer);

  /* nsISupportsArray getIdentities (); */
  NS_IMETHOD GetIdentities(nsISupportsArray **_retval);

  /* attribute nsIMsgIdentity defaultIdentity; */
  NS_IMETHOD GetDefaultIdentity(nsIMsgIdentity * *aDefaultIdentity);
  NS_IMETHOD SetDefaultIdentity(nsIMsgIdentity * aDefaultIdentity);

  /* void addIdentity (in nsIMsgIdentity identity); */
  NS_IMETHOD AddIdentity(nsIMsgIdentity *identity);

  /* void removeIdentity (in nsIMsgIdentity identity); */
  NS_IMETHOD RemoveIdentity(nsIMsgIdentity *identity);

  // nsIShutdownListener

  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports *service);
  
private:
  char *m_accountKey;
  nsIPref *m_prefs;
  nsCOMPtr<nsIMsgIncomingServer> m_incomingServer;

  nsCOMPtr<nsIMsgIdentity> m_defaultIdentity;
  nsCOMPtr<nsISupportsArray> m_identities;

  nsresult getPrefService();
  nsresult createIncomingServer();
  nsresult createIdentities();
};



NS_IMPL_ADDREF(nsMsgAccount)
NS_IMPL_RELEASE(nsMsgAccount)

nsresult
nsMsgAccount::QueryInterface(const nsIID& iid, void **result)
{
  nsresult rv = NS_NOINTERFACE;
  if (!result)
    return NS_ERROR_NULL_POINTER;

  void *res = nsnull;
  if (iid.Equals(nsCOMTypeInfo<nsIMsgAccount>::GetIID()) ||
      iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    res = NS_STATIC_CAST(nsIMsgAccount*, this);
  else if (iid.Equals(nsCOMTypeInfo<nsIShutdownListener>::GetIID()))
    res = NS_STATIC_CAST(nsIShutdownListener*, this);

  if (res) {
    NS_ADDREF(this);
    *result = res;
    rv = NS_OK;
  }

  return rv;
}

nsMsgAccount::nsMsgAccount():
  m_accountKey(0),
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
  
  PR_FREEIF(m_accountKey);
  
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
                                      nsCOMTypeInfo<nsIPref>::GetIID(),
                                      (nsISupports**)&m_prefs,
                                      this);
}

NS_IMETHODIMP
nsMsgAccount::GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer)
{
  if (!aIncomingServer) return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_OK;
  // create the incoming server lazily
  if (!m_incomingServer) {
    rv = createIncomingServer();
    if (NS_FAILED(rv)) return rv;
  }
  
  if (NS_FAILED(rv)) return rv;
  if (!m_incomingServer) return NS_ERROR_UNEXPECTED;
  
  *aIncomingServer = m_incomingServer;
  NS_ADDREF(*aIncomingServer);

  return NS_OK;
}

nsresult
nsMsgAccount::createIncomingServer()
{
  if (!m_accountKey) return NS_ERROR_NOT_INITIALIZED;
  // from here, load mail.account.myaccount.server
  // Load the incoming server
  //
  // ex) mail.account.myaccount.server = "myserver"

  nsresult rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  // get the "server" pref
  char *serverKeyPref = PR_smprintf("mail.account.%s.server", m_accountKey);
  nsXPIDLCString serverKey;
  rv = m_prefs->CopyCharPref(serverKeyPref, getter_Copies(serverKey));
  PR_FREEIF(serverKeyPref);
  if (NS_FAILED(rv)) return rv;
    
#ifdef DEBUG_alecf
  printf("\t%s's server: %s\n", m_accountKey, (const char*)serverKey);
#endif

  // get the servertype
  // ex) mail.server.myserver.type = imap
  char *serverTypePref = PR_smprintf("mail.server.%s.type",
                                     (const char*)serverKey);
  nsXPIDLCString serverType;
  rv = m_prefs->CopyCharPref(serverTypePref, getter_Copies(serverType));
  PR_FREEIF(serverTypePref);

  // the server type doesn't exist, use "generic"
  if (NS_FAILED(rv)) {
    serverType.Copy("generic");
    return rv;
  }
    
#ifdef DEBUG_alecf
  if (NS_FAILED(rv)) {
    printf("\tCould not read pref %s\n", serverTypePref);
  } else {
    printf("\t%s's   type: %s\n", m_accountKey, (const char*)serverType);
  }
#endif
    
  // get the server from the account manager
  NS_WITH_SERVICE(nsIMsgMailSession,session, NS_MSGMAILSESSION_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIMsgAccountManager> am;
  rv = session->GetAccountManager(getter_AddRefs(am));
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = am->GetIncomingServer(serverKey, getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;
  
#ifdef DEBUG_alecf
  printf("%s loaded.\n", m_accountKey);
#endif
  // store the server in this structure
  m_incomingServer = server;

  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::SetIncomingServer(nsIMsgIncomingServer * aIncomingServer)
{
  nsresult rv;
  
  nsXPIDLCString key;
  rv = aIncomingServer->GetKey(getter_Copies(key));
  
  if (NS_SUCCEEDED(rv)) {
    char* serverPrefName =
      PR_smprintf("mail.account.%s.server", m_accountKey);
    m_prefs->SetCharPref(serverPrefName, key);
    PR_smprintf_free(serverPrefName);
  }

  m_incomingServer = dont_QueryInterface(aIncomingServer);
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

  NS_ASSERTION(m_accountKey, "Account key not initialized.");
  if (!m_accountKey) return NS_ERROR_NOT_INITIALIZED;
  
  NS_NewISupportsArray(getter_AddRefs(m_identities));

  // get the pref
  // ex) mail.account.myaccount.identities = "joe-home,joe-work"
  char *identitiesKeyPref = PR_smprintf("mail.account.%s.identities",
                                        m_accountKey);
  nsXPIDLCString identityKey;
  nsresult rv;
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;
  
  rv = m_prefs->CopyCharPref(identitiesKeyPref, getter_Copies(identityKey));
  PR_FREEIF(identitiesKeyPref);
  if (NS_FAILED(rv)) return rv;
  
#ifdef DEBUG_alecf
  printf("%s's identities: %s\n", m_accountKey, (const char*)identityKey);
#endif
  
  // get the server from the account manager
  NS_WITH_SERVICE(nsIMsgMailSession,session, NS_MSGMAILSESSION_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
    
  nsCOMPtr<nsIMsgAccountManager> am;
  rv = session->GetAccountManager(getter_AddRefs(am));
  if (NS_FAILED(rv)) return rv;
    
  // XXX todo: iterate through identities. for now, assume just one
  nsCOMPtr<nsIMsgIdentity> identity;
  rv = am->GetIdentity(identityKey, getter_AddRefs(identity));
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
  
  rv = idsupports->QueryInterface(NS_GET_IID(nsIMsgIdentity),
                                  (void **)aDefaultIdentity);
  NS_RELEASE(idsupports);
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
    char *identitiesKeyPref = PR_smprintf("mail.account.%s.identities",
                                          m_accountKey);
    m_prefs->SetCharPref(identitiesKeyPref, key);
    PR_smprintf_free(identitiesKeyPref);
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
nsMsgAccount::SetKey(char *accountKey)
{
  if (!accountKey) return NS_ERROR_NULL_POINTER;

  nsresult rv=NS_OK;

  // need the prefs service to do anything
  rv = getPrefService();
  if (NS_FAILED(rv)) return rv;

  m_accountKey = PL_strdup(accountKey);
  
  return Init();
}

/* called if the prefs service goes offline */
NS_IMETHODIMP
nsMsgAccount::OnShutdown(const nsCID& aClass, nsISupports *service)
{
  if (aClass.Equals(kPrefServiceCID)) {
    if (m_prefs) nsServiceManager::ReleaseService(kPrefServiceCID, m_prefs);
    m_prefs = nsnull;
  }

  return NS_OK;
}

nsresult
NS_NewMsgAccount(const nsIID& iid, void **result)
{
  if (!result) return NS_ERROR_NULL_POINTER;
  
  nsMsgAccount *account = new nsMsgAccount;
  if (!account) return NS_ERROR_OUT_OF_MEMORY;
  
  return account->QueryInterface(iid, result);
}

 
