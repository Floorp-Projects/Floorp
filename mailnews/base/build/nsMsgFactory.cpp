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
#include "nsRepository.h"
#include "rdf.h"
#include "nsCRT.h"

#include "nsMessenger.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsIMsgRFC822Parser.h"
#include "nsMsgRFC822Parser.h"

#include "nsIUrlListenerManager.h"
#include "nsUrlListenerManager.h"

static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

static NS_DEFINE_CID(kCMessengerCID, NS_MESSENGER_CID);
static NS_DEFINE_CID(kCMessengerBootstrapCID, NS_MESSENGER_CID);

static NS_DEFINE_CID(kCMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID);
static NS_DEFINE_CID(kCMsgFolderEventCID, NS_MSGFOLDEREVENT_CID);

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
                           nsISupports *serviceMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{
	NS_INIT_REFCNT();

  // store a copy of the 
  serviceMgrSupports->QueryInterface(nsIServiceManager::GetIID(),
                                     (void **)&mServiceManager);
}   

nsMsgFactory::~nsMsgFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  
  NS_IF_RELEASE(mServiceManager);
  delete[] mClassName;
  delete[] mProgID;
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
	else if (mClassID.Equals(kCUrlListenerManagerCID))
	{
		nsUrlListenerManager * listener = nsnull;
		listener = new nsUrlListenerManager();
		if (listener) // we need to pick up a ref cnt...
			listener->QueryInterface(nsIUrlListenerManager::GetIID(), (void **) &inst);
	}

	// End of checking the interface ID code....
	if (inst) 
	{
		// so we now have the class that supports the desired interface...we need to turn around and
		// query for our desired interface.....
		res = inst->QueryInterface(aIID, aResult);
		NS_RELEASE(inst);
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
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

  *aFactory = new nsMsgFactory(aClass, aClassName, aProgID, serviceMgr);
  if (aFactory)
    return (*aFactory)->QueryInterface(nsIFactory::GetIID(),
                                       (void**)aFactory);
  else
    return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr) 
{
	return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* serviceMgr, const char* path)
{
  nsresult rv;

  // register the message folder factory
  rv = nsRepository::RegisterComponent(kCMsgFolderEventCID, 
                                       nsnull, nsnull,
                                       path, PR_TRUE, PR_TRUE);

  rv = nsRepository::RegisterComponent(kCUrlListenerManagerCID, nsnull, nsnull,
									   path, PR_TRUE, PR_TRUE);

  rv = nsRepository::RegisterComponent(kCMsgRFC822ParserCID, nsnull, nsnull,
									   path, PR_TRUE, PR_TRUE);
  
  rv = nsRepository::RegisterComponent(kCMessengerCID,
                                       "Netscape Messenger",
                                       "component://netscape/messenger",
                                       path,
                                       PR_TRUE, PR_TRUE);

  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* serviceMgr, const char* path)
{
  nsresult rv;

  rv = nsRepository::UnregisterComponent(kCUrlListenerManagerCID, path);
  rv = nsRepository::UnregisterComponent(kCMsgRFC822ParserCID, path);
  rv = nsRepository::UnregisterComponent(kCMessengerCID, path);
  rv = nsRepository::UnregisterComponent(kCMsgFolderEventCID, path);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////

