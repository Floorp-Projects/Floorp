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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIMsgAccountManager.h"
#include "nsIPop3IncomingServer.h"

#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static nsresult printIdentity(nsIMsgIdentity *);
static nsresult printAccount(nsIMsgAccount *);
static nsresult printIncomingServer(nsIMsgIncomingServer*);


/* I tried to rip this from webshell/tests/viewer/nsSetupRegistry.cpp and 
   then realized it was futile.
   this is SO ugly, but I'm not going to copy stuff from this file,
   it's too big
*/
#include "../../../../webshell/tests/viewer/nsSetupRegistry.cpp"

int main() {

  nsresult rv;
  NS_SetupRegistry();

  printf("Kicking off prefs\n");
  static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
  nsIPref *prefs;
  // start up prefs
  rv = nsServiceManager::GetService(kPrefServiceCID,
                                    nsIPref::GetIID(),
                                    (nsISupports**)&prefs);

  if (NS_FAILED(rv)) {
    printf("Couldn't start prefs\n");
    exit(1);
  }

  rv = prefs->Startup("prefs50.js");
  if (NS_FAILED(rv)) printf("Couldn't read prefs\n");

  nsIMsgAccountManager *accountManager;
  rv = nsComponentManager::CreateInstance(kMsgAccountManagerCID,
                                          nsnull,
                                          nsIMsgAccountManager::GetIID(),
                                          (void **)&accountManager);

  if (NS_FAILED(rv)) {
    printf("Couldn't create the account manager. Check that autoregistration is working\n");
    return 1;
  }


  rv = accountManager->LoadAccounts();
  if (NS_FAILED(rv)) {
    printf("Error loading accounts\n");
    return 1;
  }
  
  nsIMsgAccount *account;
  rv = accountManager->GetDefaultAccount(&account);

  if (NS_FAILED(rv)) {
    printf("Error getting default account\n");
    return 1;
  }

  rv = printAccount(account);
  
  if (NS_FAILED(rv)) {
    printf("Error %8.8X printing account\n", rv);
    return 1;
  }

}


static nsresult
printAccount(nsIMsgAccount *account)
{
  nsresult rv;

  if (!account) {
    printf("No account\n");
    return NS_ERROR_UNEXPECTED;
  }
  
  printf("Incoming Server data:\n");
  nsIMsgIncomingServer *server;
  rv = account->GetIncomingServer(&server);
  if (NS_FAILED(rv)) {
    printf("No incoming server\n");
    //    return rv;
  } else {
  
  rv = printIncomingServer(server);
  //  if (NS_FAILED(rv)) return rv;
  }

  printf("Identity data:\n");
  //nsIEnumerator *identities;
  nsIMsgIdentity *identity;
  
  //rv = account->getIdentities(&identities);
  //  if (NS_FAILED(rv)) return rv;
  rv = account->GetDefaultIdentity(&identity);
  if (NS_FAILED(rv)) {
    printf("No Identity\n");
    return rv;
  }

  rv = printIdentity(identity);

  if (NS_FAILED(rv)) {
    printf("Error printing identity\n");
    return rv;
  }
  return rv;
};

static nsresult
printIdentity(nsIMsgIdentity *identity)
{
  if (!identity) return NS_ERROR_NULL_POINTER;
  char *value;
  nsresult rv;
  rv = identity->GetIdentityName(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tID Name: %s\n", value);

  rv = identity->GetFullName(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tFullName: %s\n", value);

  rv = identity->GetEmail(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tEmail: %s\n", value);
  
  rv = identity->GetReplyTo(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tReplyTo: %s\n", value);

  rv = identity->GetOrganization(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tOrganization: %s\n", value);
  
  rv = identity->GetSmtpHostname(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tSmtpHostname: %s\n", value);

  rv = identity->GetSmtpUsername(&value);  
  if (NS_SUCCEEDED(rv) && value) printf("\tSmtpUsername: %s\n", value);

  return rv;
}


static nsresult
printIncomingServer(nsIMsgIncomingServer *server)
{
  if (!server) return NS_ERROR_NULL_POINTER;
  char *value;
  nsresult rv;

  value=nsnull;
  rv = server->GetPrettyName(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tPrettyName: %s\n", value);

  value=nsnull;
  rv = server->GetHostName(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tHostName: %s\n", value);

  value=nsnull;
  rv = server->GetUserName(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tUserName: %s\n", value);

  value=nsnull;
  rv = server->GetPassword(&value);
  if (NS_SUCCEEDED(rv) && value) printf("\tPassword: %s\n", value);  

  /* see if it's a pop server */
  nsIPop3IncomingServer *popServer=nsnull;
  rv = server->QueryInterface(nsIPop3IncomingServer::GetIID(),
                              (void **)&popServer);
  
  if (NS_SUCCEEDED(rv) && popServer) {
    printf("\tThis is a pop3 server!\n");
    value=nsnull;
    rv = popServer->GetRootFolderPath(&value);
    if (NS_SUCCEEDED(rv) && value) printf("\t\tPop3 root folder path: %s\n",
                                          value);
    
  } else {
    printf("Not a pop3 server\n");
  }
  
  return NS_OK;
}
