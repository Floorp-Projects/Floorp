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

#include "nsIMessenger.h"
#include "nsMessenger.h"

#include "nsMsgGroupRecord.h"

#include "nsMsgAppCore.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsIMsgRFC822Parser.h"
#include "nsMsgRFC822Parser.h"

#include "nsIUrlListenerManager.h"
#include "nsUrlListenerManager.h"
#include "nsMsgMailSession.h"
#include "nsMessageViewDataSource.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

static NS_DEFINE_CID(kCMessengerCID, NS_MESSENGER_CID);
static NS_DEFINE_CID(kCMessengerBootstrapCID, NS_MESSENGERBOOTSTRAP_CID);

static NS_DEFINE_CID(kCMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID);
static NS_DEFINE_CID(kCMsgFolderEventCID, NS_MSGFOLDEREVENT_CID);

static NS_DEFINE_CID(kCMsgAppCoreCID, NS_MSGAPPCORE_CID);
static NS_DEFINE_CID(kCMsgGroupRecordCID, NS_MSGGROUPRECORD_CID);

static NS_DEFINE_CID(kCMessageViewDataSourceCID, NS_MESSAGEVIEWDATASOURCE_CID);


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
  nsIServiceManager* mServiceManager;
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

  // store a copy of the 
  compMgrSupports->QueryInterface(nsIServiceManager::GetIID(),
                                  (void **)&mServiceManager);
}   

nsMsgFactory::~nsMsgFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  
  NS_IF_RELEASE(mServiceManager);
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
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want an RFC822 Parser interface ?
	if (mClassID.Equals(kCMsgRFC822ParserCID)) 
	{
		res = NS_NewRFC822Parser((nsIMsgRFC822Parser **) &inst);
		if (NS_FAILED(res))  // was there a problem creating the object ?
		  return res;   
	}
	else if (mClassID.Equals(kCMsgFolderEventCID)) 
	{
		NS_NOTREACHED("hello? what happens here?");
		return NS_OK;
	}
	else if (mClassID.Equals(kCMessengerBootstrapCID)) 
	{
		res = NS_NewMessengerBootstrap((nsIAppShellService**)&inst,
                                   mServiceManager);
		if (NS_FAILED(res)) return res;
	}
	else if (mClassID.Equals(kCMessengerCID)) 
	{
		res = NS_NewMessenger((nsIMessenger**)&inst);
		if (NS_FAILED(res)) return res;
	}
#if 0                           // not implemented yet
  else if (mClassID.Equals(kCMsgGroupRecordCID))
  {
    res = NS_NewMsgGroupRecord((nsIMsgGroupRecord **)&inst);
    if (NS_FAILED(res)) return res;
  }
#endif
	else if (mClassID.Equals(kCUrlListenerManagerCID))
	{
		nsUrlListenerManager * listener = nsnull;
		listener = new nsUrlListenerManager();
		if (listener == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		listener->QueryInterface(nsIUrlListenerManager::GetIID(), (void **) &inst);
	}
	else if (mClassID.Equals(kCMsgMailSessionCID))
	{
		nsMsgMailSession * session = new nsMsgMailSession();
		if (session == nsnull)
			return NS_ERROR_OUT_OF_MEMORY;
		session->QueryInterface(nsIMsgMailSession::GetIID(),  (void **) &inst);		

	}
	else if (mClassID.Equals(kCMsgAppCoreCID)) 
	{
		res = NS_NewMsgAppCore((nsIDOMMsgAppCore **)&inst);
		if (NS_FAILED(res)) return res;
	}
	else if (mClassID.Equals(kCMessageViewDataSourceCID))
	{
		inst = NS_STATIC_CAST(nsIRDFCompositeDataSource*, new nsMessageViewDataSource());

	}
	// End of checking the interface ID code....
	if (inst) 
	{
		// so we now have the class that supports the desired interface...we need to turn around and
		// query for our desired interface.....
		res = inst->QueryInterface(aIID, aResult);
		if (res != NS_OK)  // if the query interface failed for some reason, then the object did not get ref counted...delete it.
			delete inst; 
	}
	else
		res = NS_ERROR_OUT_OF_MEMORY;

  return res;  
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

  rv = compMgr->RegisterComponent(kCMsgRFC822ParserCID,
                                       "RFC822 Parser",
                                       nsnull,
                                       path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMessengerCID,
                                       "Netscape Messenger",
                                       "component://netscape/messenger/application",
                                       path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  
  rv = compMgr->RegisterComponent(kCMessengerBootstrapCID,
                                       "Netscape Messenger Bootstrapper",
                                       "component://netscape/messenger",
                                       path,
                                       PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kCMsgAppCoreCID,
                                  "Messenger AppCore",
                                  "component://netscape/appcores/messenger",
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

	rv = compMgr->RegisterComponent(kCMessageViewDataSourceCID, 
                                  "Mail/News Message View Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "mail-messageview",
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
  rv = compMgr->UnregisterComponent(kCMsgRFC822ParserCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kCMessengerCID, path);
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
  rv = compMgr->UnregisterComponent(kCMessageViewDataSourceCID, path);
  if(NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

