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
#include "nsCOMPtr.h"
#include "pratom.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsMimeEmitterCID.h"
#include "nsIMimeEmitter.h"
#include "nsMimeHtmlEmitter.h"
#include "nsMimeRawEmitter.h"
#include "nsMimeXmlEmitter.h"
#include "nsMimeXULEmitter.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kHtmlMimeEmitterCID, NS_HTML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kRawMimeEmitterCID, NS_RAW_MIME_EMITTER_CID);
static NS_DEFINE_CID(kXmlMimeEmitterCID, NS_XML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kXULMimeEmitterCID, NS_XUL_MIME_EMITTER_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMimeEmitterFactory : public nsIFactory
{   
public:
   // nsISupports methods
   NS_DECL_ISUPPORTS 

   nsMimeEmitterFactory(const nsCID &aClass,
               const char* aClassName,
               const char* aProgID,
               nsISupports*);

   // nsIFactory methods   
   NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
   NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsMimeEmitterFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mProgID;
};   

nsMimeEmitterFactory::nsMimeEmitterFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aProgID,
                           nsISupports *compMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{
	NS_INIT_REFCNT();
}   

nsMimeEmitterFactory::~nsMimeEmitterFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
	PL_strfree(mClassName);
	PL_strfree(mProgID);
}   

nsresult nsMimeEmitterFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(nsIFactory::GetIID()))   
    *aResult = (void *)(nsIFactory*)this;   

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMimeEmitterFactory)
NS_IMPL_RELEASE(nsMimeEmitterFactory)

nsresult nsMimeEmitterFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)  
{  
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	//nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a mime emitter interface ?
	if (mClassID.Equals(kHtmlMimeEmitterCID)) 
		res = NS_NewMimeHtmlEmitter(aIID, aResult);
	else if (mClassID.Equals(kRawMimeEmitterCID)) 
		res = NS_NewMimeRawEmitter(aIID, aResult);
	else if (mClassID.Equals(kXmlMimeEmitterCID)) 
		res = NS_NewMimeXmlEmitter(aIID, aResult);
	else if (mClassID.Equals(kXULMimeEmitterCID)) 
		res = NS_NewMimeXULEmitter(aIID, aResult);
	return res;  
}  

nsresult
nsMimeEmitterFactory::LockFactory(PRBool aLock)  
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

  *aFactory = new nsMimeEmitterFactory(aClass, aClassName, aProgID, aServMgr);
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
  nsresult rv = NS_OK;
  nsresult finalResult = NS_OK;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kHtmlMimeEmitterCID,
                                  "RFC822 Parser",
                                  "component://netscape/messenger/mimeemitter;type=text/html",
                                   path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) finalResult = rv;

  rv = compMgr->RegisterComponent(kRawMimeEmitterCID,
                                  "RFC822 Parser",
                                   "component://netscape/messenger/mimeemitter;type=raw",
                                   path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) finalResult = rv;

  rv = compMgr->RegisterComponent(kXmlMimeEmitterCID,
                                  "RFC822 Parser",
                                   "component://netscape/messenger/mimeemitter;type=text/xml",
                                   path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) finalResult = rv;

  rv = compMgr->RegisterComponent(kXULMimeEmitterCID,
                                  "RFC822 Parser",
                                   "component://netscape/messenger/mimeemitter;type=text/xul",
                                   path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) finalResult = rv;
  
  return finalResult;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv;
	nsresult finalResult = NS_OK;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kHtmlMimeEmitterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kRawMimeEmitterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kXmlMimeEmitterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	rv = compMgr->UnregisterComponent(kXULMimeEmitterCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	return finalResult;
}

////////////////////////////////////////////////////////////////////////////////

