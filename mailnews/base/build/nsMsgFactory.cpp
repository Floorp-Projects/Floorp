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

/* Include all of the interfaces our factory can generate components for */
#include "nsIMsgRFC822Parser.h"
#include "nsMsgRFC822Parser.h"

#include "nsMsgComposeFact.h"
#include "nsMsgCompFieldsFact.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kCMsgRFC822ParserCID, NS_MSGRFC822PARSER_CID);
static NS_DEFINE_IID(kCMsgComposeCID, NS_MSGCOMPOSE_CID);
static NS_DEFINE_IID(kCMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID);


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

class nsMsgFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

    nsMsgFactory(const nsCID &aClass); 

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);   

  protected:
    virtual ~nsMsgFactory();   

  private:  
    nsCID     mClassID;
};   

nsMsgFactory::nsMsgFactory(const nsCID &aClass)   
{   
	NS_INIT_REFCNT();
	mClassID = aClass;
}   

nsMsgFactory::~nsMsgFactory()   
{   
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsMsgFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(kISupportsIID))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(kIFactoryIID))   
    *aResult = (void *)(nsIFactory*)this;   

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMsgFactory)
NS_IMPL_RELEASE(nsMsgFactory)

nsresult nsMsgFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
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
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
	}

	// do they want an Message Compose interface ?
	if (mClassID.Equals(kCMsgComposeCID))
	{
		res = NS_NewMsgCompose((nsIMsgCompose **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
	}

	// do they want an Message Compose Fields interface ?
	if (mClassID.Equals(kCMsgCompFieldsCID))
	{
		res = NS_NewMsgCompFields((nsIMsgCompFields **) &inst);
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

nsresult nsMsgFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass,
                                           nsISupports *serviceMgr,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	// If we decide to implement multiple factories in the msg.dll, then we need to check the class
	// type here and create the appropriate factory instead of always creating a nsMsgFactory...
	*aFactory = new nsMsgFactory(aClass);

	if (aFactory)
		return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

