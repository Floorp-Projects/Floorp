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

#include "pratom.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsRepository.h"

#include "nsINetPlugin.h"
#include "nsRepository.h"
#include "plugin_inst.h"
#include "net.h"

/* 
 * Include all of the headers/defines for interfaces the libmime factory can 
 * generate components for
 */
#include "nsRFC822toHTMLStreamConverter.h"
static   NS_DEFINE_CID(kCMimeRFC822HTMLConverterCID, NS_RFC822_HTML_STREAM_CONVERTER_CID);

#include "nsMimeObjectClassAccess.h"
static   NS_DEFINE_CID(kCMimeMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID);

#include "nsMimeHeaderConverter.h"
static   NS_DEFINE_CID(kCMimeHeaderConverterCID, NS_MIME_HEADER_CONVERTER_CID);

// These are necessary for the new stream converter/plugin interface...
static   NS_DEFINE_IID(kINetPluginIID,      NS_INET_PLUGIN_IID);
static   NS_DEFINE_CID(kINetPluginCID,      NS_INET_PLUGIN_CID);
static   NS_DEFINE_CID(kINetPluginMIMECID,  NS_INET_PLUGIN_MIME_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_LockCount = 0;
static PRInt32 g_InstanceCount = 0;

class nsMimeFactory : public nsINetPlugin
{   
  public:
    // nsISupports methods
    NS_DECL_ISUPPORTS 
    
    nsMimeFactory(const nsCID &aClass); 
    
    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);
  
    ////////////////////////////////////////////////////////////////////////////
    // from nsINetPlugin:
    NS_IMETHOD Initialize(void);
    NS_IMETHOD Shutdown(void);
    NS_IMETHOD GetMIMEDescription(const char* *result);
 
  protected:
    virtual    ~nsMimeFactory();   
  
  private:  
    nsCID      mClassID;
};   

nsMimeFactory::nsMimeFactory(const nsCID &aClass)   
{   
	NS_INIT_REFCNT();
	mClassID = aClass;
}   

nsMimeFactory::~nsMimeFactory()   
{   
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsMimeFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support three interfaces....nsISupports, nsFactory and nsINetPlugin.....
  if (aIID.Equals(::nsISupports::GetIID()))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(nsIFactory::GetIID()))   
    *aResult = (void *)(nsIFactory*)this;
  else if (aIID.Equals(nsINetPlugin::GetIID()))   
    *aResult = (void *)(nsINetPlugin*)this; 

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMimeFactory)
NS_IMPL_RELEASE(nsMimeFactory)

nsresult nsMimeFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!

  // ADD NEW CLASSES HERE!!!	
	// do they want an an RFC822 - HTML interface ?
	if (mClassID.Equals(kCMimeRFC822HTMLConverterCID))
	{
		res = NS_NewRFC822HTMLConverter((nsIStreamConverter **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
	}
  else if (mClassID.Equals(kCMimeMimeObjectClassAccessCID))
  {
    res = NS_NewMimeObjectClassAccess((nsIMimeObjectClassAccess **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
  }
  else if (mClassID.Equals(kCMimeHeaderConverterCID))
  {
    res = NS_NewMimeHeaderConverter((nsIMimeHeaderConverter **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
  }
  else if (mClassID.Equals(kINetPluginMIMECID))
  {
    res = NS_NewMimePluginInstance((MimePluginInstance **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;	  	  
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

nsresult nsMimeFactory::LockFactory(PRBool aLock)  
{  
  if (aLock) {
    PR_AtomicIncrement(&g_LockCount);
  } else {
    PR_AtomicDecrement(&g_LockCount);
  }

  return NS_OK;
}  

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	// If we decide to implement multiple factories in the mime.dll, then we need to check the class
	// type here and create the appropriate factory instead of always creating a nsMimeFactory...
	*aFactory = new nsMimeFactory(aClass);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::GetIID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}


extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* serviceMgr, const char *path)
{
  printf("*** Mime being registered\n");
  nsRepository::RegisterComponent(kCMimeMimeObjectClassAccessCID, NULL, NULL, path, 
                                PR_TRUE, PR_TRUE);
  nsRepository::RegisterComponent(kCMimeRFC822HTMLConverterCID, NULL, NULL, path, 
                                PR_TRUE, PR_TRUE);
  nsRepository::RegisterComponent(kCMimeHeaderConverterCID, NULL, NULL, path, 
                                PR_TRUE, PR_TRUE);
                                
  nsRepository::RegisterComponent(kINetPluginMIMECID, NULL, PROGRAM_ID, path, 
                                  PR_TRUE, PR_TRUE);
  return NS_OK;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* serviceMgr, const char *path)
{
  printf("*** Mime being unregistered\n");
  nsRepository::UnregisterComponent(kCMimeMimeObjectClassAccessCID, path);
  nsRepository::UnregisterComponent(kCMimeRFC822HTMLConverterCID, path);
  nsRepository::UnregisterComponent(kCMimeHeaderConverterCID, path);
  
	return nsRepository::UnregisterComponent(kINetPluginMIMECID, path);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// nsINetPlugin methods
////////////////////////////////////////////////////////////////////////////
NS_METHOD
nsMimeFactory::Initialize(void)
{
    return NS_OK;
}

NS_METHOD
nsMimeFactory::Shutdown(void)
{
    return NS_OK;
}

NS_METHOD
nsMimeFactory::GetMIMEDescription(const char* *result)
{
    return NS_OK;
}

