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

static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

class nsMsgAccount : public nsIMsgAccount {
  
public:
  nsMsgAccount();
  virtual ~nsMsgAccount();
  
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetKey(char ** aKey);
  NS_IMETHOD SetKey(char * aKey);
  
  NS_IMETHOD GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer);
  NS_IMETHOD SetIncomingServer(nsIMsgIncomingServer * aIncomingServer);

  /* nsIEnumerator getIdentities (); */
  NS_IMETHOD getIdentities(nsIEnumerator **_retval);

  /* attribute nsIMsgIdentity defaultIdentity; */
  NS_IMETHOD GetDefaultIdentity(nsIMsgIdentity * *aDefaultIdentity);
  NS_IMETHOD SetDefaultIdentity(nsIMsgIdentity * aDefaultIdentity);

  /* void addIdentity (in nsIMsgIdentity identity); */
  NS_IMETHOD addIdentity(nsIMsgIdentity *identity);

  /* void removeIdentity (in nsIMsgIdentity identity); */
  NS_IMETHOD removeIdentity(nsIMsgIdentity *identity);

private:
  char *m_accountKey;
  nsIPref *m_prefs;
  nsIMsgIncomingServer *m_incomingServer;

  nsIMsgIdentity *m_defaultIdentity;
};



NS_IMPL_ISUPPORTS(nsMsgAccount, GetIID())

nsMsgAccount::nsMsgAccount():
  m_accountKey(0),
  m_prefs(0),
  m_incomingServer(0),
  m_defaultIdentity(0)
{
  NS_INIT_REFCNT();
  
}

nsMsgAccount::~nsMsgAccount()
{
  PR_FREEIF(m_accountKey);
  NS_RELEASE(m_incomingServer);
  
  // in the future, the default identity should not be refcounted
  NS_RELEASE(m_defaultIdentity);
}


NS_IMETHODIMP
nsMsgAccount::GetIncomingServer(nsIMsgIncomingServer * *aIncomingServer)
{
  if (!aIncomingServer) return NS_ERROR_NULL_POINTER;
  nsresult rv;

  // need to call SetKey() first!
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
    
    nsIMsgIncomingServer *server;
    rv = nsComponentManager::CreateInstance(serverTypeProgID,
                                            nsnull,
                                            nsIMsgIncomingServer::GetIID(),
                                            (void **)&server);
    PR_FREEIF(serverTypeProgID);
    
#ifdef DEBUG_alecf
    if (NS_SUCCEEDED(rv)) {
      printf("Created a %s server\n", serverType);
    } else {
      printf("Could not create a %s server\n", serverType);
    }
#endif
    
    if (NS_SUCCEEDED(rv))
      rv = server->SetKey(serverKey);
    
    if (NS_SUCCEEDED(rv))
      rv = SetIncomingServer(server);

    PR_FREEIF(serverKey);
  }
  
  *aIncomingServer = m_incomingServer;

  // addref for the caller
  NS_ADDREF(m_incomingServer);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::SetIncomingServer(nsIMsgIncomingServer * aIncomingServer)
{
  if (aIncomingServer != m_incomingServer) {
    NS_IF_RELEASE(m_incomingServer);
    m_incomingServer = aIncomingServer;
    if (m_incomingServer) NS_ADDREF(m_incomingServer);
  }
  return NS_OK;
}

/* nsIEnumerator getIdentities (); */
NS_IMETHODIMP
nsMsgAccount::getIdentities(nsIEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIMsgIdentity defaultIdentity; */
NS_IMETHODIMP
nsMsgAccount::GetDefaultIdentity(nsIMsgIdentity * *aDefaultIdentity)
{
  if (!aDefaultIdentity) return NS_ERROR_NULL_POINTER;
  if (!m_defaultIdentity) return NS_ERROR_NULL_POINTER;
  
  *aDefaultIdentity = m_defaultIdentity;
  NS_ADDREF(*aDefaultIdentity);
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccount::SetDefaultIdentity(nsIMsgIdentity * aDefaultIdentity)
{
  if (aDefaultIdentity != m_defaultIdentity) {
    NS_IF_RELEASE(m_defaultIdentity);
    m_defaultIdentity = aDefaultIdentity;
    if (m_defaultIdentity) NS_ADDREF(m_defaultIdentity);
  }
  return NS_OK;
}

/* void addIdentity (in nsIMsgIdentity identity); */
NS_IMETHODIMP
nsMsgAccount::addIdentity(nsIMsgIdentity *identity)
{
  // hack hack - need to add this to the list of identities.
  // for now just tread this as a Setxxx accessor
  // when this is actually implemented, don't refcount the default identity
  SetDefaultIdentity(identity);
  return NS_OK;
}

/* void removeIdentity (in nsIMsgIdentity identity); */
NS_IMETHODIMP
nsMsgAccount::removeIdentity(nsIMsgIdentity *identity)
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
  if (!m_prefs)
    rv = nsServiceManager::GetService(kPrefServiceCID,
                                      nsIPref::GetIID(),
                                      (nsISupports**)&m_prefs);
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
  rv = m_prefs->CopyCharPref(identitiesKeyPref, &identityKey);
  PR_FREEIF(identitiesKeyPref);

#ifdef DEBUG_alecf
  printf("%s's identities: %s\n", accountKey, identityKey);
#endif
  
  // XXX todo: iterate through identities. for now, assume just one

  nsIMsgIdentity *identity = nsnull;
  rv = nsComponentManager::CreateInstance(kMsgIdentityCID,
                                          nsnull,
                                          nsIMsgIdentity::GetIID(),
                                          (void **)&identity);

  if (NS_SUCCEEDED(rv))
    rv = identity->SetKey(identityKey);
#ifdef DEBUG_alecf
  else
    printf("\tcouldn't create %s's identity\n",identityKey);
#endif

  PR_FREEIF(identityKey);
  if (NS_SUCCEEDED(rv))
    rv = addIdentity(identity);

  return rv;
}

nsresult
NS_NewMsgAccount(const nsIID& iid, void **result)
{
  if (!result) return NS_ERROR_NULL_POINTER;
  
  nsMsgAccount *account = new nsMsgAccount;

  return account->QueryInterface(iid, result);
}

 
