/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozXMLTermFactory.cpp: XPCOM factory for mozIXMLTermShell, mozILineTerm

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nspr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"

#include "mozXMLT.h"
#include "mozLineTerm.h"
#include "mozIXMLTermShell.h"

static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kXMLTermShellCID,     MOZXMLTERMSHELL_CID);
static NS_DEFINE_CID(kLineTermCID,         MOZLINETERM_CID);

class XMLTermFactory : public nsIFactory
{
public:
  XMLTermFactory(const nsCID &aClass, const char* className, const char* contractID);

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

protected:
  virtual ~XMLTermFactory();

protected:
  nsCID       mClassID;
  const char* mClassName;
  const char* mContractID;
};

/////////////////////////////////////////////////////////////////////////
// mozXMLTermFactory implementation
/////////////////////////////////////////////////////////////////////////

// Globals, useful to check if safe to unload module
static PRInt32 gLockCnt = 0; 
static PRInt32 gInstanceCnt = 0; 

XMLTermFactory::XMLTermFactory(const nsCID &aClass, 
                               const char* className,
                               const char* contractID):
  mClassID(aClass),
  mClassName(className),
  mContractID(contractID)
{
  // Zero reference counter
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&gInstanceCnt);
}


XMLTermFactory::~XMLTermFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  PR_AtomicDecrement(&gInstanceCnt);
}


NS_IMETHODIMP
XMLTermFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  // Always NULL result, in case of failure
  *aResult = nsnull;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = NS_STATIC_CAST(nsISupports*, this);

  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = NS_STATIC_CAST(nsIFactory*, this);

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}


NS_IMPL_ADDREF(XMLTermFactory);
NS_IMPL_RELEASE(XMLTermFactory);


NS_IMETHODIMP
XMLTermFactory::CreateInstance(nsISupports *aOuter,
                               const nsIID &aIID,
                               void **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;

  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  *aResult = nsnull;

  nsresult rv;

  nsISupports *inst = nsnull;
  if (mClassID.Equals(kXMLTermShellCID)) {
    if (NS_FAILED(rv = NS_NewXMLTermShell((mozIXMLTermShell**) &inst)))
      return rv;

  } else if (mClassID.Equals(kLineTermCID)) {
    if (NS_FAILED(rv = NS_NewLineTerm((mozILineTerm**) &inst)))
      return rv;

  } else {
    return NS_ERROR_NO_INTERFACE;
  }

  if (! inst)
    return NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
    // We didn't get the right interface.
    NS_ERROR("didn't support the interface you wanted");
  }

  NS_IF_RELEASE(inst);
  return rv;
}


NS_IMETHODIMP
XMLTermFactory::LockFactory(PRBool aLock)
{ 
  if (aLock) { 
    PR_AtomicIncrement(&gLockCnt); 
  } else { 
    PR_AtomicDecrement(&gLockCnt); 
  } 

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////
// Exported functions for loading, registering, unregistering, and unloading
////////////////////////////////////////////////////////////////////////////

// Return approptiate factory to the caller
extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
  static PRBool ltermInitialized = PR_FALSE;
  static PRBool xmltermInitialized = PR_FALSE;

  if (!ltermInitialized) {
    // Initialize all LINETERM operations
    // (This initialization needs to be done at factory creation time;
    //  trying to do it earlier, i.e., at registration time,
    //  does not work ... something to do with loading of static global
    //  variables.)

    int messageLevel = 8;
    char* debugStr = (char*) PR_GetEnv("LTERM_DEBUG");

    if (debugStr && (strlen(debugStr) == 1)) {
      messageLevel = 98;
      debugStr = nsnull;
    }

    int result = lterm_init(0);
    if (result == 0) {
      tlog_set_level(LTERM_TLOG_MODULE, messageLevel, debugStr);
    }
    ltermInitialized = PR_TRUE;

    char* logStr = (char*) PR_GetEnv("LTERM_LOG");
    if (logStr && (strlen(logStr) > 0)) {
      // Enable LineTerm logging
      mozLineTerm::mLoggingEnabled = PR_TRUE;
    }
  }

  if (aClass.Equals(kXMLTermShellCID) && !xmltermInitialized) {
    // Set initial debugging message level for XMLterm
    int messageLevel = 8;
    char* debugStr = (char*) PR_GetEnv("XMLT_DEBUG");
                      
    if (debugStr && (strlen(debugStr) == 1)) {
      messageLevel = 98;
      debugStr = nsnull;
    }

    tlog_set_level(XMLT_TLOG_MODULE, messageLevel, debugStr);
    xmltermInitialized = PR_TRUE;
  }

  if (!aFactory)
    return NS_ERROR_NULL_POINTER;

  *aFactory = nsnull;

  XMLTermFactory* factory = new XMLTermFactory(aClass, aClassName,
                                               aContractID);
  if (factory == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(factory);
  *aFactory = factory;
  return NS_OK;
}


extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
  nsresult result;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &result));
  if (NS_FAILED(result)) return result;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &result);
  if (NS_FAILED(result)) return result;

  printf("Registering lineterm interface\n");

  result = compMgr->RegisterComponent(kLineTermCID,
                                      "LineTerm Component",
                                      "@mozilla.org/xmlterm/lineterm;1",
                                      aPath, PR_TRUE, PR_TRUE);

  if (NS_FAILED(result)) return result;

  printf("Registering xmlterm shell interface\n");

  result = compMgr->RegisterComponent(kXMLTermShellCID,
                                      "XMLTerm Shell Component",
                                      "@mozilla.org/xmlterm/xmltermshell;1",
                                      aPath, PR_TRUE, PR_TRUE);

  if (NS_FAILED(result)) return result;

  return NS_OK;
}


extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult result;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &result));
  if (NS_FAILED(result)) return result;

  NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &result);
  if (NS_FAILED(result)) return result;

  result = compMgr->UnregisterComponent(kLineTermCID, aPath);
  if (NS_FAILED(result)) return result;

  result = compMgr->UnregisterComponent(kXMLTermShellCID, aPath);
  if (NS_FAILED(result)) return result;

  return NS_OK;
}
