/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#include "nscore.h"
#include "nsLocaleOS2.h"
#include "nsLocaleFactoryOS2.h"
#include "nsCollationOS2.h"
#include "nsDateTimeFormatOS2.h"

#include "nsLocaleCID.h"
#include "nsCollationCID.h"
#include "nsDateTimeFormatCID.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);
static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
//static NS_DEFINE_IID(kILocaleFactoryIID,NS_ILOCALEFACTORY_IID);

static NS_DEFINE_CID(kLocaleFactoryCID, NS_LOCALEFACTORY_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kCollationCID, NS_COLLATION_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

// Okay, this is very weird: for some crazy reason known only to the
// intl@netscape folks, nsILocaleFactory exists.  This makes things
// more complicated here than in other DLLs.
//
// Note that nsICollationFactory is *not* an nsIFactory.
//
// How on earth are people supposed to create nsILocale objects?
// Or call methods from nsILocaleFactory ?
// In fact this must be wrong: say I want to create an nsILocaleFactory.
// NSGetFactory() here gives the CM not a factory to create these but
// an actual nsILocaleFactory.  CreateInstance() on that then gives me
// the system locale.  What on earth is going on!

class nsLocaleDllFactory : public nsIFactory
{
 public:
   nsLocaleDllFactory( const nsCID &aClass);
   virtual ~nsLocaleDllFactory() {}

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIFactory methods
   NS_IMETHOD CreateInstance( nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

   NS_IMETHOD LockFactory( PRBool aLock) { return NS_OK; }

 protected:
   nsCID mClassID;
};

nsLocaleDllFactory::nsLocaleDllFactory( const nsCID &aClass)
{   
   NS_INIT_REFCNT();
   mClassID = aClass;
}

NS_IMPL_ISUPPORTS(nsLocaleDllFactory,nsIFactory::GetIID())

nsresult nsLocaleDllFactory::CreateInstance( nsISupports *aOuter,
                                             const nsIID &aIID,
                                             void       **aResult)
{
   if( !aResult)
      return NS_ERROR_NULL_POINTER;

   *aResult = nsnull;

   nsISupports *inst = nsnull;

   if( aIID.Equals( kICollationFactoryIID))
      inst = new nsCollationFactoryOS2;
   else if( aIID.Equals( kICollationIID))
      inst = new nsCollationOS2;
   else if( aIID.Equals( kIDateTimeFormatIID))
      inst = new nsDateTimeFormatOS2;
   else 
      return NS_NOINTERFACE;

   if( inst == nsnull)
     return NS_ERROR_OUT_OF_MEMORY;  
  
   nsresult rc = inst->QueryInterface( aIID, aResult);
  
   if( NS_FAILED(rc))
      delete inst;

   return rc;
}

// Find a factory
extern "C" NS_EXPORT nsresult NSGetFactory( nsISupports *aServiceMgr,
                                            const nsCID &aClass,
                                            const char  *aClassName,
                                            const char  *aContractID,
                                            nsIFactory **aFactory)
{
   if( !aFactory || !aServiceMgr)
      return NS_ERROR_NULL_POINTER;

   nsIFactory *fact = nsnull;
   nsresult	   rc = NS_ERROR_FAILURE;

   // first check for strange localefactory class
   if( aClass.Equals( kLocaleFactoryCID))
   {
      fact = new nsLocaleFactoryOS2;
      // XXX this next line looks a bit wrong, but hey...
      //      rc = fact->QueryInterface( kILocaleFactoryIID, (void **) aFactory);
   }
   else // something sensible
   {
      fact = new nsLocaleDllFactory( aClass);

      rc = fact->QueryInterface( nsIFactory::GetIID(), (void**)aFactory);
   }

   if( NS_FAILED(rc))
      delete fact;

   return rc;
}

// Register classes
extern "C" NS_EXPORT nsresult NSRegisterSelf( nsISupports *aServMgr,
                                              const char  *aPath)
{
   nsresult rc;
   NS_WITH_SERVICE1( nsIComponentManager, pCompMgr, aServMgr,
                     kComponentManagerCID, &rc);
   if( NS_FAILED(rc)) return rc;

   pCompMgr->RegisterComponent( kLocaleFactoryCID, NULL, NULL,
                                aPath, PR_TRUE, PR_TRUE);
   pCompMgr->RegisterComponent( kCollationFactoryCID, NULL, NULL,
                                aPath, PR_TRUE, PR_TRUE);
   pCompMgr->RegisterComponent( kCollationCID, NULL, NULL,
                                aPath, PR_TRUE, PR_TRUE);
   pCompMgr->RegisterComponent( kDateTimeFormatCID, NULL, NULL,
                                aPath, PR_TRUE, PR_TRUE);
   return rc;
}

// Deregister classes
extern "C" NS_EXPORT nsresult NSUnregisterSelf( nsISupports *aServMgr,
                                                const char  *aPath)
{
   nsresult rc;
   NS_WITH_SERVICE1( nsIComponentManager, pCompMgr, aServMgr,
                     kComponentManagerCID, &rc);
   if( NS_FAILED(rc)) return rc;

   pCompMgr->UnregisterComponent( kLocaleFactoryCID, aPath);
   pCompMgr->UnregisterComponent( kCollationFactoryCID, aPath);
   pCompMgr->UnregisterComponent( kCollationCID, aPath);
   pCompMgr->UnregisterComponent( kDateTimeFormatCID, aPath);

   return NS_OK;
}

// XXX NSCanUnload not implemented
