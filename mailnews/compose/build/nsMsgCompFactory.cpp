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

#include "msgCore.h"

#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "nsMsgCompCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsISmtpService.h"
#include "nsSmtpService.h"
#include "nsSmtpUrl.h"
#include "nsMsgComposeService.h"
#include "nsMsgCompose.h"
#include "nsMsgComposeFact.h"
#include "nsMsgCompFieldsFact.h"
#include "nsMsgSendFact.h"
#include "nsMsgSendLaterFact.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsMsgQuote.h"
#include "nsIMsgDraft.h"
#include "nsMsgCreate.h"    // For drafts...I know, awful file name...

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kCMsgComposeCID, NS_MSGCOMPOSE_CID);
static NS_DEFINE_CID(kCMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID);
static NS_DEFINE_CID(kCMsgSendCID, NS_MSGSEND_CID);
static NS_DEFINE_CID(kCMsgSendLaterCID, NS_MSGSENDLATER_CID);
static NS_DEFINE_CID(kCSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kCMsgComposeServiceCID, NS_MSGCOMPOSESERVICE_CID);
static NS_DEFINE_CID(kCMsgQuoteCID, NS_MSGQUOTE_CID);
static NS_DEFINE_CID(kCSmtpUrlCID, NS_SMTPURL_CID);
static NS_DEFINE_CID(kMsgDraftCID, NS_MSGDRAFT_CID);


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgComposeFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

    nsMsgComposeFactory(const nsCID &aClass,
               const char* aClassName,
               const char* aProgID,
               nsISupports*); 

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);   

  protected:
    virtual ~nsMsgComposeFactory();   

  private:  
    nsCID mClassID;
	char* mClassName;
	char* mProgID;
	nsIServiceManager* mServiceManager;
};   

nsMsgComposeFactory::nsMsgComposeFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aProgID,
                           nsISupports *compMgrSupports)   
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();

	// store a copy of the 
  compMgrSupports->QueryInterface(nsCOMTypeInfo<nsIServiceManager>::GetIID(),
                                  (void **)&mServiceManager);
}   

nsMsgComposeFactory::~nsMsgComposeFactory()   
{   
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   

	NS_IF_RELEASE(mServiceManager);
	PL_strfree(mClassName);
	PL_strfree(mProgID);
}   

nsresult nsMsgComposeFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(nsCOMTypeInfo<nsIFactory>::GetIID()))   
    *aResult = (void *)(nsIFactory*)this;   

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMsgComposeFactory)
NS_IMPL_RELEASE(nsMsgComposeFactory)

nsresult nsMsgComposeFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!

	// mscott --> I've noticed that all of the "convience" functions (NS_*) used to 
	// create components are adding a ref count to the returned component...then at the end of
	// CreateInstance we ref count again when we query interface for the desired interface. 
	// That's why we need to be careful and make sure we release the first ref count before returning...
	// So, if you add a component here and it doesn't ref count when you create it then you might
	// have a problem....
	
	if (mClassID.Equals(kCSmtpServiceCID)) 
	{
		nsSmtpService * smtpService = new nsSmtpService();
		// okay now turn around and give inst a handle on it....
    if (smtpService)
  		return smtpService->QueryInterface(kISupportsIID, aResult);
    else
      return NS_ERROR_OUT_OF_MEMORY;
	}

	// do they want a Message Compose interface ?
	else if (mClassID.Equals(kCMsgComposeCID)) 
	{
		nsMsgCompose * msgCompose = new nsMsgCompose();
		// okay now turn around and give inst a handle on it....
    if (msgCompose)
  		return msgCompose->QueryInterface(kISupportsIID, aResult);
    else
      return NS_ERROR_OUT_OF_MEMORY;
	}

	// do they want a Message Compose Fields interface ?
	else if (mClassID.Equals(kCMsgCompFieldsCID)) 
	{
		return NS_NewMsgCompFields(aIID, aResult);
	}

	// do they want a Message Send interface ?
	else if (mClassID.Equals(kCMsgSendCID)) 
	{
		return NS_NewMsgSend(aIID, aResult);
	}
	
	// do they want a Message Send Later interface ?
	else if (mClassID.Equals(kCMsgSendLaterCID)) 
	{
		return NS_NewMsgSendLater(aIID, aResult);
	}

  // do they want a Draft interface?
	else if (mClassID.Equals(kMsgDraftCID)) 
	{
		return NS_NewMsgDraft(aIID, aResult);
	}

	else if (mClassID.Equals(kCMsgComposeServiceCID)) 
	{
		nsMsgComposeService * aMsgCompService = new nsMsgComposeService();
		// okay now turn around and give inst a handle on it....
    if (aMsgCompService)
  		return aMsgCompService->QueryInterface(kISupportsIID, aResult);
    else
      return NS_ERROR_OUT_OF_MEMORY;
	}

	// Quoting anyone?
	else if (mClassID.Equals(kCMsgQuoteCID)) 
		return NS_NewMsgQuote(aIID, aResult);
	else if (mClassID.Equals(kCSmtpUrlCID))
		return NS_NewSmtpUrl(aIID, aResult);
  
	return NS_NOINTERFACE;  
}  

nsresult nsMsgComposeFactory::LockFactory(PRBool aLock)  
{  
	if (aLock) 
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount); 
	
	return NS_OK;
}  

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	*aFactory = new nsMsgComposeFactory(aClass, aClassName, aProgID, aServMgr);
	if (aFactory)
		return (*aFactory)->QueryInterface(nsCOMTypeInfo<nsIFactory>::GetIID(),
									   (void**)aFactory);
		else
			return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv = NS_OK;
	nsresult finalResult = NS_OK;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	// register the message compose factory
	rv = compMgr->RegisterComponent(kCSmtpServiceCID,
										"SMTP Service", nsnull,
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;
	
	rv = compMgr->RegisterComponent(kCSmtpUrlCID,
										"Smtp url",
										nsnull,
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kCMsgComposeServiceCID,
										"Message Compose Service",
										"component://netscape/messengercompose",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kCMsgComposeCID,
										"Message Compose",
										"component://netscape/messengercompose/compose",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kCMsgCompFieldsCID,
										"Message Compose Fields",
										"component://netscape/messengercompose/composefields",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kCMsgSendCID,
										"Message Compose Send",
										"component://netscape/messengercompose/send",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

    rv = compMgr->RegisterComponent(kCMsgSendLaterCID,
										"Message Compose Send Later",
										"Xcomponent://netscape/messengercompose/sendlater",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

    rv = compMgr->RegisterComponent(kCSmtpServiceCID,
										"Message Compose SMTP Service",
										"Xcomponent://netscape/messengercompose/smtp",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->RegisterComponent(kCSmtpServiceCID,  
                                    "SMTP Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "mailto",
                                    path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) finalResult = rv;

  
    rv = compMgr->RegisterComponent(kCMsgQuoteCID,
										"Message Quoting",
										"Xcomponent://netscape/messengercompose/smtp",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

  // For Drafts...
  rv = compMgr->RegisterComponent(kMsgDraftCID,
										"Message Drafts",
										"Xcomponent://netscape/messengercompose/drafts",
										path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) finalResult = rv;

  return finalResult;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult finalResult = NS_OK;
	nsresult rv = NS_OK;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kCMsgComposeServiceCID, path);
	if (NS_FAILED(rv))finalResult = rv;

	rv = compMgr->UnregisterComponent(kCMsgComposeCID, path);
	if (NS_FAILED(rv))finalResult = rv;

	rv = compMgr->UnregisterComponent(kCMsgCompFieldsCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kCMsgSendCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kCMsgSendLaterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kCSmtpServiceCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kCSmtpUrlCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kCMsgQuoteCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

  rv = compMgr->UnregisterComponent(kMsgDraftCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	return finalResult;
}
