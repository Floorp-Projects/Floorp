/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsMsgGroupRecord.h"

#include "nsMsgAppCore.h"

/* Include all of the interfaces our factory can generate components for */

#include "nsIUrlListenerManager.h"
#include "nsUrlListenerManager.h"
#include "nsMsgMailSession.h"
#include "nsMsgAccount.h"
#include "nsMsgAccountManager.h"
#include "nsMsgIdentity.h"
#include "nsMessageViewDataSource.h"
#include "nsMsgFolderDataSource.h"
#include "nsMsgMessageDataSource.h"

#include "nsMsgAccountManagerDS.h"
#include "nsMsgAccountDataSource.h"
#include "nsMsgServerDataSource.h"
#include "nsMsgIdentityDataSource.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

static NS_DEFINE_CID(kCMessengerBootstrapCID, NS_MESSENGERBOOTSTRAP_CID);

static NS_DEFINE_CID(kCMsgFolderEventCID, NS_MSGFOLDEREVENT_CID);

static NS_DEFINE_CID(kCMsgAppCoreCID, NS_MSGAPPCORE_CID);
static NS_DEFINE_CID(kCMsgGroupRecordCID, NS_MSGGROUPRECORD_CID);

static NS_DEFINE_CID(kMailNewsFolderDataSourceCID, NS_MAILNEWSFOLDERDATASOURCE_CID);
static NS_DEFINE_CID(kMailNewsMessageDataSourceCID, NS_MAILNEWSMESSAGEDATASOURCE_CID);
static NS_DEFINE_CID(kCMessageViewDataSourceCID, NS_MESSAGEVIEWDATASOURCE_CID);

// account manager stuff
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);

// account manager RDF stuff
static NS_DEFINE_CID(kMsgAccountManagerDataSourceCID, NS_MSGACCOUNTMANAGERDATASOURCE_CID);
static NS_DEFINE_CID(kMsgAccountDataSourceCID, NS_MSGACCOUNTDATASOURCE_CID);
static NS_DEFINE_CID(kMsgIdentityDataSourceCID, NS_MSGIDENTITYDATASOURCE_CID);
static NS_DEFINE_CID(kMsgServerDataSourceCID, NS_MSGSERVERDATASOURCE_CID);


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

  nsMsgFactory(const nsCID &aClass,
               const char* aClassName,
               const char* aProgID,
               nsISupports*);

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
  NS_IMETHOD LockFactory(PRBool aLock);   

protected:
  virtual ~nsMsgFactory();   

  nsCID mClassID;
  char* mClassName;
  char* mProgID;
};   

nsMsgFactory::nsMsgFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aProgID,
                           nsISupports *compMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{
	NS_INIT_REFCNT();

}   

nsMsgFactory::~nsMsgFactory()   
{

	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  
	PL_strfree(mClassName);
	PL_strfree(mProgID);
}   

nsresult
nsMsgFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(::nsISupports::GetIID()))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(nsIFactory::GetIID()))   
    *aResult = (void *)(nsIFactory*)this;   

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMsgFactory)
NS_IMPL_RELEASE(nsMsgFactory)

nsresult
nsMsgFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)  
{  
	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
  if (mClassID.Equals(kCMsgFolderEventCID)) 
	{
		NS_NOTREACHED("hello? what happens here?");
		return NS_OK;
	}
	else if (mClassID.Equals(kCMessengerBootstrapCID)) 
	{
		return NS_NewMessengerBootstrap(aIID, aResult);
	}
	else if (mClassID.Equals(kCUrlListenerManagerCID))
	{
		nsUrlListenerManager * listener = nsnull;
		listener = new nsUrlListenerManager();
		if (listener == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		else
			return listener->QueryInterface(aIID, aResult);
	}
	else if (mClassID.Equals(kCMsgMailSessionCID))
	{
		nsMsgMailSession * session = new nsMsgMailSession();
		if (session == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		else
			return session->QueryInterface(aIID,  aResult);		
	}
	else if (mClassID.Equals(kCMsgAppCoreCID)) 
	{
		return NS_NewMsgAppCore(aIID, aResult);
	}

  else if (mClassID.Equals(kMsgAccountManagerCID))
  {
    return NS_NewMsgAccountManager(aIID, aResult);
  }
  
  else if (mClassID.Equals(kMsgAccountCID))
  {
    return NS_NewMsgAccount(aIID, aResult);
  }
  
  else if (mClassID.Equals(kMsgIdentityCID)) {
    nsMsgIdentity* identity = new nsMsgIdentity();
    return identity->QueryInterface(aIID, aResult);
  }
	else if (mClassID.Equals(kMailNewsFolderDataSourceCID)) 
	{
		nsresult rv;
		nsMsgFolderDataSource * folderDataSource = new nsMsgFolderDataSource();
		if (folderDataSource)
			rv = folderDataSource->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && folderDataSource)
			delete folderDataSource;
		return rv;
	}
	else if (mClassID.Equals(kMailNewsMessageDataSourceCID)) 
	{
		nsresult rv;
		nsMsgMessageDataSource * messageDataSource = new nsMsgMessageDataSource();
		if (messageDataSource)
			rv = messageDataSource->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && messageDataSource)
			delete messageDataSource;
			
		return rv;
	}
 	else if (mClassID.Equals(kCMessageViewDataSourceCID))
	{
		nsMessageViewDataSource * msgView = new nsMessageViewDataSource();
		if (msgView)
			return msgView->QueryInterface(aIID, aResult);
		else
			return NS_ERROR_OUT_OF_MEMORY;
	}

  // account manager RDF datasources
  else if (mClassID.Equals(kMsgAccountManagerDataSourceCID)) {
    return NS_NewMsgAccountManagerDataSource(aIID, aResult);
  }
  else if (mClassID.Equals(kMsgAccountDataSourceCID)) {
    return NS_NewMsgAccountDataSource(aIID, aResult);
  }
  else if (mClassID.Equals(kMsgIdentityDataSourceCID)) {
    return NS_NewMsgIdentityDataSource(aIID, aResult);
  }
  else if (mClassID.Equals(kMsgServerDataSourceCID)) {
    return NS_NewMsgServerDataSource(aIID, aResult);
  }
	
	return NS_NOINTERFACE;  
}  

nsresult
nsMsgFactory::LockFactory(PRBool aLock)  
{  
	if (aLock)
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount);

	return NS_OK;
}  

////////////////////////////////////////////////////////////////////////////////

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

  *aFactory = new nsMsgFactory(aClass, aClassName, aProgID, aServMgr);
  if (aFactory)
    return (*aFactory)->QueryInterface(nsIFactory::GetIID(),
                                       (void**)aFactory);
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
	return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  // register the message folder factory
  rv = compMgr->RegisterComponent(kCMsgFolderEventCID, 
                                       "Folder Event",
                                       nsnull,
                                       path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCUrlListenerManagerCID,
                                       "UrlListenerManager",
                                       nsnull,
                                       path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMessengerBootstrapCID,
                                       "Netscape Messenger Bootstrapper",
                                       "component://netscape/messenger",
                                       path,
                                       PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMsgAppCoreCID,
                                  "Messenger AppCore",
                                  "component://netscape/appcores/messenger",
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kMsgAccountManagerCID,
                                  "Messenger Account Manager",
                                  "component://netscape/messenger/account-manager",
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kMsgAccountCID,
                                  "Messenger User Account",
                                  "component://netscape/messenger/account",
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kMsgIdentityCID,
                                  "Messenger User Identity",
                                  "component://netscape/messenger/identity",
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  
#if 0
  rv = compMgr->RegisterComponent(kCMsgGroupRecordCID,
                                       nsnull,
                                       nsnull,
                                       path,
                                       PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
#endif
  rv = compMgr->RegisterComponent(kCMsgMailSessionCID,
                                  "Mail Session",
                                  nsnull,
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  // register our RDF datasources:
  rv = compMgr->RegisterComponent(kMailNewsFolderDataSourceCID, 
                                  "Mail/News Folder Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "mailnewsfolders",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  // register our RDF datasources:
  rv = compMgr->RegisterComponent(kMailNewsMessageDataSourceCID, 
                                  "Mail/News Message Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "mailnewsmessages",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMessageViewDataSourceCID, 
                                  "Mail/News Message View Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "mail-messageview",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kMsgAccountManagerDataSourceCID,
                                  "Mail/News Account Manager Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "msgaccountmanager",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kMsgAccountDataSourceCID,
                                  "Mail/News Account Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "msgaccounts",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kMsgIdentityDataSourceCID,
                                  "Mail/News Identity Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "msgidentities",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kMsgServerDataSourceCID,
                                  "Mail/News Server Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "msgservers",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  
                                  
#ifdef NS_DEBUG
  printf("mailnews registering from %s\n",path);
#endif

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kCUrlListenerManagerCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kCMessengerBootstrapCID, path);
  if (NS_FAILED(rv)) goto done;
#if 0
  rv = compMgr->UnregisterComponent(kCMsgGroupRecordCID, path);
  if (NS_FAILED(rv)) goto done;
#endif
  rv = compMgr->UnregisterComponent(kCMsgFolderEventCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kCMsgMailSessionCID, path);
  if(NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kMailNewsFolderDataSourceCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kMailNewsMessageDataSourceCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kCMessageViewDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;

  // Account Manager RDF stuff
  rv = compMgr->UnregisterComponent(kMsgAccountManagerDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kMsgAccountDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kMsgIdentityDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kMsgServerDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

