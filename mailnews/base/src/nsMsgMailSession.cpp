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

#include "msgCore.h" // for pre-compiled headers
#include "nsIMsgIdentity.h"
#include "nsIMsgAccountManager.h"
#include "nsIPop3IncomingServer.h"
#include "nsMsgMailSession.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"

NS_IMPL_ISUPPORTS(nsMsgMailSession, nsIMsgMailSession::GetIID());

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kPop3IncomingServerCID, NS_POP3INCOMINGSERVER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
    

nsMsgMailSession::nsMsgMailSession():
  m_accountManager(0)
{
	NS_INIT_REFCNT();

    nsresult rv;
    /* kick of the prefs now, we'll need them for the account manager */
    
    nsIPref* prefs;
    rv = nsServiceManager::GetService(kPrefCID,
                                      nsIPref::GetIID(),
                                      (nsISupports**)&prefs);
    if (NS_FAILED(rv)) return;
    
    if (prefs && NS_SUCCEEDED(rv))
      rv = prefs->Startup("prefs50.js");
    
    (void)nsServiceManager::ReleaseService(kPrefCID, prefs);

    rv = nsComponentManager::CreateInstance(kMsgAccountManagerCID,
                                            NULL,
                                            nsIMsgAccountManager::GetIID(),
                                            (void **)&m_accountManager);
    if (NS_SUCCEEDED(rv))
      m_accountManager->LoadAccounts();

}

nsMsgMailSession::~nsMsgMailSession()
{
	NS_IF_RELEASE(m_accountManager);
}


// nsIMsgMailSession
nsresult nsMsgMailSession::GetCurrentIdentity(nsIMsgIdentity ** aIdentity)
{
  nsresult rv=NS_ERROR_UNEXPECTED;
  nsIMsgAccount *defaultAccount;

  if (m_accountManager)
    rv = m_accountManager->GetDefaultAccount(&defaultAccount);
  if (NS_FAILED(rv)) return rv;
  
  rv = defaultAccount->GetDefaultIdentity(aIdentity);
  
  NS_IF_RELEASE(defaultAccount);

  return rv;
}

nsresult nsMsgMailSession::GetCurrentServer(nsIMsgIncomingServer ** aServer)
{
  nsresult rv=NS_ERROR_UNEXPECTED;
  nsIMsgAccount *defaultAccount;
  if (m_accountManager)
    rv = m_accountManager->GetDefaultAccount(&defaultAccount);

  if (NS_FAILED(rv)) return rv;

  rv = defaultAccount->GetIncomingServer(aServer);

  NS_IF_RELEASE(defaultAccount);

  return rv;
}
