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


#include "nsMsgAccount.h"
#include "nsIMsgAccount.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "prprf.h"
#include "plstr.h"
#include "prmem.h"
#include "nsMsgBaseCID.h"
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolderCache.h"

static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

class nsMsgAccount : public nsIMsgAccount,
                     public nsIShutdownListener
{
  
public:
  nsMsgAccount();
  virtual ~nsMsgAccount();
  
  NS_DECL_ISUPPORTS

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
  NS_NewISupportsArray(getter_AddRefs(m_identities));
  NS_INIT_REFCNT();
}

nsMsgAccount::~nsMsgAccount()
{
  PR_FREEIF(m_accountKey);
  
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
  //Need to initialize this otherwise if there's already an m_incomingServer, this
  //will be unitialized.
  nsresult rv = NS_OK;

  // need to call SetKey() first!
  NS_ASSERTION(m_accountKey, "Account key not initialized.");
  if (!m_accountKey) return NS_ERROR_NOT_INITIALIZED;
  
  // create the incoming server lazily
  if (!m_incomingServer) {
    // from here, load mail.account.myaccount.server
    // and             mail.account.myaccount.identities
    // to load and create the appropriate objects
    
    //
    // Load the incoming server
    //
    // ex) mail.account.myaccount.server = "myserver"
    char *serverKeyPref = PR_smprintf("mail.account.%s.server", m_accountKey);
    char *serverKey;

    /* make sure m_prefs is valid */
    rv = getPrefService();
    if (NS_FAILED(rv)) return rv;
    
    rv = m_prefs->CopyCharPref(serverKeyPref, &serverKey);
    PR_FREEIF(serverKeyPref);

    // the server pref doesn't exist
    if (NS_FAILED(rv)) return rv;
    
#ifdef DEBUG_alecf
    printf("\t%s's server: %s\n", m_accountKey, serverKey);
#endif
    
    // ask the prefs what kind of server this is and use it to
    // create a ProgID for it
    // ex) mail.server.myserver.type = imap
    char *serverTypePref = PR_smprintf("mail.server.%s.type", serverKey);
    char *serverType;
    rv = m_prefs->CopyCharPref(serverTypePref, &serverType);
    PR_FREEIF(serverTypePref);

    // the server type doesn't exist!
    if (NS_FAILED(rv)) return rv;
    
#ifdef DEBUG_alecf
    if (NS_FAILED(rv)) {
      printf("\tCould not read pref %s\n", serverTypePref);
    } else {
      printf("\t%s's   type: %s\n", m_accountKey, serverType);
    }
#endif
    
    char *serverTypeProgID =
      PR_smprintf("component://netscape/messenger/server&type=%s", serverType);
    
    PR_FREEIF(serverType);
    
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = nsComponentManager::CreateInstance(serverTypeProgID,
                                            nsnull,
                                            nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
                                            getter_AddRefs(server));
    PR_FREEIF(serverTypeProgID);
    
    if (NS_SUCCEEDED(rv))
      rv = server->SetKey(serverKey);
    
    if (NS_SUCCEEDED(rv))
      rv = SetIncomingServer(server);

    PR_FREEIF(serverKey);
  }
  
  if (NS_FAILED(rv)) return rv;
  if (!m_incomingServer) return NS_ERROR_UNEXPECTED;
  
  *aIncomingServer = m_incomingServer;
  NS_ADDREF(*aIncomingServer);

  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::SetIncomingServer(nsIMsgIncomingServer * aIncomingServer)
{
  m_incomingServer = dont_QueryInterface(aIncomingServer);
  return NS_OK;
}

/* nsISupportsArray GetIdentities (); */
NS_IMETHODIMP
nsMsgAccount::GetIdentities(nsISupportsArray **_retval)
{
  NS_ASSERTION(m_accountKey, "Account key not initialized.");
  if (!_retval) return NS_ERROR_NULL_POINTER;
  if (!m_identities) return NS_ERROR_UNEXPECTED;
  
  *_retval = m_identities;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* attribute nsIMsgIdentity defaultIdentity; */
NS_IMETHODIMP
nsMsgAccount::GetDefaultIdentity(nsIMsgIdentity * *aDefaultIdentity)
{
  NS_ASSERTION(m_accountKey, "Account key not initialized.");
  if (!aDefaultIdentity) return NS_ERROR_NULL_POINTER;
  if (!m_defaultIdentity) return NS_ERROR_NULL_POINTER;
  
  *aDefaultIdentity = m_defaultIdentity;
  return NS_OK;
}

// todo - make sure this is in the identity array!
NS_IMETHODIMP
nsMsgAccount::SetDefaultIdentity(nsIMsgIdentity * aDefaultIdentity)
{
  NS_ASSERTION(m_accountKey, "Account key not initialized.");
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
  // for now just tread this as a Setxxx accessor
  // when this is actually implemented, don't refcount the default identity
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


  // we should make this be created lazily like incoming servers are
  //
  // Load Identities
  //
  // ex) mail.account.myaccount.identities = "joe-home,joe-work"
  char *identitiesKeyPref = PR_smprintf("mail.account.%s.identities",
                                        accountKey);
  char *identityKey = nsnull;
  rv = getPrefService();
  if (NS_SUCCEEDED(rv)) 
    rv = m_prefs->CopyCharPref(identitiesKeyPref, &identityKey);
  
  PR_FREEIF(identitiesKeyPref);

#ifdef DEBUG_alecf
  printf("%s's identities: %s\n", accountKey, identityKey);
#endif
  
  // XXX todo: iterate through identities. for now, assume just one

  nsIMsgIdentity *identity = nsnull;
  rv = nsComponentManager::CreateInstance(kMsgIdentityCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMsgIdentity>::GetIID(),
                                          (void **)&identity);

  if (NS_SUCCEEDED(rv))
    rv = identity->SetKey(identityKey);
#ifdef DEBUG_alecf
  else
    printf("\tcouldn't create %s's identity\n",identityKey);
#endif

  PR_FREEIF(identityKey);
  if (NS_SUCCEEDED(rv))
    rv = AddIdentity(identity);

  return rv;
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

 
