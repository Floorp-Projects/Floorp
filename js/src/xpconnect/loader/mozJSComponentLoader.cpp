/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   John Bandhauer <jband@netscape.com>
 *   IBM Corporation
 */

#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsILocalFile.h"
#include "nsIModule.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "mozJSComponentLoader.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"
#include "nsIObserverService.h"
#include "nsIXPCScriptable.h"
#ifndef XPCONNECT_STANDALONE
#include "nsIScriptSecurityManager.h"
#include "nsIScriptObjectOwner.h"
#endif

// For reporting errors with the console service
#include "nsIScriptError.h"
#include "nsIConsoleService.h"

const char mozJSComponentLoaderContractID[] = "@mozilla.org/moz/jsloader;1";
const char jsComponentTypeName[] = "text/javascript";

/* XXX export properly from libxpcom, for now this will let Mac build */
#ifdef RHAPSODY
extern const char fileSizeValueName[]; // = "FileSize";
extern const char lastModValueName[]; // = "LastModTimeStamp";
extern const char xpcomKeyName[]; // = "Software/Mozilla/XPCOM";
#else
const char fileSizeValueName[] = "FileSize";
const char lastModValueName[] = "LastModTimeStamp";
const char xpcomKeyName[] = "software/mozilla/XPCOM/components";
#endif

const char kJSRuntimeServiceContractID[] = "@mozilla.org/js/xpc/RuntimeService;1";
const char kXPConnectServiceContractID[] = "@mozilla.org/js/xpc/XPConnect;1";
const char kJSContextStackContractID[] =   "@mozilla.org/js/xpc/ContextStack;1";

static JSBool PR_CALLBACK
Dump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    if (!argc)
        return JS_TRUE;
    
    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;

    char *bytes = JS_GetStringBytes(str);
    bytes = nsCRT::strdup(bytes);

#ifdef XP_MAC
    for (char *c = bytes; *c; c++)
        if (*c == '\r')
            *c = '\n';
#endif
    fputs(bytes, stderr);
    nsMemory::Free(bytes);
    return JS_TRUE;
}

static JSBool PR_CALLBACK
Debug(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef DEBUG
  return Dump(cx, obj, argc, argv, rval);
#else
  return JS_TRUE;
#endif
}

static JSFunctionSpec gGlobalFun[] = {
    {"dump", Dump, 1 },
    {"debug", Debug, 1 },
    {0}
};

static JSClass gGlobalClass = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

#ifndef XPCONNECT_STANDALONE
class BackstagePass : public nsIScriptObjectPrincipal, public nsIXPCScriptable
{
public:
  NS_DECL_ISUPPORTS
  XPC_DECLARE_IXPCSCRIPTABLE
  
  NS_IMETHOD GetPrincipal(nsIPrincipal **aPrincipal) {
    NS_ADDREF(*aPrincipal = mPrincipal);
    return NS_OK;
  }

  BackstagePass(nsIPrincipal *prin) :
    mPrincipal(prin)
  {
    NS_INIT_ISUPPORTS(); 
  }

  virtual ~BackstagePass() { }

private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(BackstagePass, nsIScriptObjectPrincipal, nsIXPCScriptable);

#else

class BackstagePass : public nsIXPCScriptable
{
public:
  NS_DECL_ISUPPORTS
  XPC_DECLARE_IXPCSCRIPTABLE

  BackstagePass()
  {
    NS_INIT_ISUPPORTS(); 
  }

  virtual ~BackstagePass() { }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(BackstagePass, nsIXPCScriptable);

#endif

XPC_IMPLEMENT_IGNORE_CREATE(BackstagePass)
XPC_IMPLEMENT_IGNORE_GETFLAGS(BackstagePass);
XPC_IMPLEMENT_FORWARD_LOOKUPPROPERTY(BackstagePass)
XPC_IMPLEMENT_FORWARD_DEFINEPROPERTY(BackstagePass)
XPC_IMPLEMENT_FORWARD_GETPROPERTY(BackstagePass)
XPC_IMPLEMENT_FORWARD_SETPROPERTY(BackstagePass)
XPC_IMPLEMENT_FORWARD_GETATTRIBUTES(BackstagePass)
XPC_IMPLEMENT_FORWARD_SETATTRIBUTES(BackstagePass)
XPC_IMPLEMENT_FORWARD_DELETEPROPERTY(BackstagePass)
XPC_IMPLEMENT_FORWARD_DEFAULTVALUE(BackstagePass)
XPC_IMPLEMENT_FORWARD_ENUMERATE(BackstagePass)
XPC_IMPLEMENT_FORWARD_CHECKACCESS(BackstagePass)
XPC_IMPLEMENT_FORWARD_CALL(BackstagePass)
XPC_IMPLEMENT_FORWARD_CONSTRUCT(BackstagePass)
XPC_IMPLEMENT_FORWARD_HASINSTANCE(BackstagePass);
XPC_IMPLEMENT_FORWARD_FINALIZE(BackstagePass)

mozJSComponentLoader::mozJSComponentLoader()
    : mCompMgr(nsnull),
      mSuperGlobal(nsnull),
      mRuntime(nsnull),
      mContext(nsnull),
      mCompMgrWrapper(nsnull),
      mModules(nsnull),
      mGlobals(nsnull),
      mXPCOMKey(0),
      mInitialized(PR_FALSE)
{
    NS_INIT_REFCNT();
}

static PRIntn PR_CALLBACK
UnrootGlobals(PLHashEntry *he, PRIntn i, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    JS_RemoveRoot(cx, &he->value);
    nsCRT::free((char *)he->key);
    return HT_ENUMERATE_REMOVE;
}

static PRIntn PR_CALLBACK
UnloadAndReleaseModules(PLHashEntry *he, PRIntn i, void *arg)
{
    nsIModule *module = NS_STATIC_CAST(nsIModule *, he->value);
    nsIComponentManager *mgr = NS_STATIC_CAST(nsIComponentManager *, arg);
    PRBool canUnload;
    if (NS_SUCCEEDED(module->CanUnload(mgr, &canUnload)) && canUnload) {
        NS_RELEASE(module);
        /* XXX need to unroot the global for the module as well */
        nsCRT::free((char *)he->key);
        return HT_ENUMERATE_REMOVE;
    }
    return HT_ENUMERATE_NEXT;
}

mozJSComponentLoader::~mozJSComponentLoader()
{
    if (mInitialized) {
        mInitialized = PR_FALSE;
        PL_HashTableEnumerateEntries(mModules, UnloadAndReleaseModules,
                                     mCompMgr);
        PL_HashTableDestroy(mModules);
        mModules = nsnull;

        PL_HashTableEnumerateEntries(mGlobals, UnrootGlobals, mContext);
        PL_HashTableDestroy(mGlobals);
        mGlobals = nsnull;

        JS_RemoveRoot(mContext, &mSuperGlobal);
        JS_RemoveRoot(mContext, &mCompMgrWrapper);
        mCompMgrWrapper = nsnull; // release wrapper so GC can release CM

        JS_DestroyContext(mContext);
        mContext = nsnull;
        mRuntimeService = nsnull;
    }
}

NS_IMPL_ISUPPORTS(mozJSComponentLoader, NS_GET_IID(nsIComponentLoader));

NS_IMETHODIMP
mozJSComponentLoader::GetFactory(const nsIID &aCID,
                                 const char *aLocation,
                                 const char *aType,
                                 nsIFactory **_retval)
{
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

#ifdef DEBUG_shaver_off
    char *cidString = aCID.ToString();
    fprintf(stderr, "mJCL::GetFactory(%s,%s,%s)\n", cidString, aLocation, aType);
    delete [] cidString;
#endif

    
    nsIModule * module = ModuleForLocation(aLocation, 0);
    if (!module) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "ERROR: couldn't get module for %s\n", aLocation);
#endif
        return NS_ERROR_FACTORY_NOT_LOADED;
    }
    
    nsresult rv = module->GetClassObject(mCompMgr, aCID,
                                         NS_GET_IID(nsIFactory),
                                         (void **)_retval);
#ifdef DEBUG_shaver_off
    fprintf(stderr, "GetClassObject %s\n", NS_FAILED(rv) ? "FAILED" : "ok");
#endif
    return rv;
}

static void PR_CALLBACK
Reporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    nsresult rv;

    /* Use the console service to register the error. */
    nsCOMPtr<nsIConsoleService> consoleService
        (do_GetService("@mozilla.org/consoleservice;1"));

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.  The UI can then
     * poll the service to update the JavaScript console.
     */
    nsCOMPtr<nsIScriptError>
        errorObject(do_CreateInstance("@mozilla.org/scripterror;1"));
    
    if (consoleService != nsnull && errorObject != nsnull) {
        /*
         * Got an error object; prepare appropriate-width versions of
         * various arguments to it.
         */
        nsAutoString fileUni;
        fileUni.AssignWithConversion(rep->filename);

        const PRUnichar *newFileUni = fileUni.ToNewUnicode();
        
        PRUint32 column = rep->uctokenptr - rep->uclinebuf;

        rv = errorObject->Init(NS_REINTERPRET_CAST(const PRUnichar*, rep->ucmessage), newFileUni, NS_REINTERPRET_CAST(const PRUnichar*, rep->uclinebuf),
                               rep->lineno, column, rep->flags,
                               "component javascript");
        nsMemory::Free((void *)newFileUni);
        if (NS_SUCCEEDED(rv)) {
            rv = consoleService->LogMessage(errorObject);
            if (NS_SUCCEEDED(rv)) {
                // We're done!  Skip return to fall thru to stderr
                // printout, for the benefit of those invoking the
                // browser with -console.
                // return;
            }
        }
    }

    /*
     * If any of the above fails for some reason, fall back to
     * printing to stderr.
     */
    fprintf(stderr, "JS Component Loader: %s %s:%d\n"
            "                     %s\n",
            JSREPORT_IS_WARNING(rep->flags) ? "WARNING" : "ERROR",
            rep->filename, rep->lineno,
            message ? message : "<no message>");
}

NS_IMETHODIMP
mozJSComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg)
{
    mCompMgr = aCompMgr;
    nsresult rv;

    /* initialize registry handles */
    mRegistry = do_QueryInterface(aReg, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = mRegistry->GetSubtree(nsIRegistry::Common, xpcomKeyName,
                                   &mXPCOMKey);
        if (NS_FAILED(rv))
            /* if we can't get the XPCOM key, just skip all registry ops */
            mRegistry = nsnull;
    }
    return NS_OK;
}

nsresult
mozJSComponentLoader::ReallyInit()
{
    nsresult rv;

    /*
     * Get the JSRuntime from the runtime svc, if possible.
     * We keep a reference around, because it's a Bad Thing if the runtime
     * service gets shut down before we're done.  Bad!
     */
    mRuntimeService = do_GetService(kJSRuntimeServiceContractID, &rv);
    if (NS_FAILED(rv) ||
        NS_FAILED(rv = mRuntimeService->GetRuntime(&mRuntime)))
        return rv;
    
    /*
     * Get the XPConnect service.
     */
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID);
    if (!xpc)
        return NS_ERROR_FAILURE;

    mContext = JS_NewContext(mRuntime, 8192 /* pref? */);
    if (!mContext)
        return NS_ERROR_OUT_OF_MEMORY;

#ifndef XPCONNECT_STANDALONE
    NS_WITH_SERVICE(nsIScriptSecurityManager, secman, 
                   NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secman)
        return NS_ERROR_FAILURE;

    rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
    if (NS_FAILED(rv) || !mSystemPrincipal)
        return NS_ERROR_FAILURE;
#endif

    mSuperGlobal = JS_NewObject(mContext, &gGlobalClass, nsnull, nsnull);

    if (!mSuperGlobal)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!JS_InitStandardClasses(mContext, mSuperGlobal) ||
        !JS_DefineFunctions(mContext, mSuperGlobal, gGlobalFun)) {
        return NS_ERROR_FAILURE;
    }

    rv = xpc->InitClasses(mContext, mSuperGlobal);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(mContext, mSuperGlobal, mCompMgr, 
                         NS_GET_IID(nsIComponentManager),
                         getter_AddRefs(holder));

    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "WrapNative(%p,%p,nsIComponentManager) failed: %x\n",
                mContext, mCompMgr, rv);
#endif
        return rv;
    }

    rv = holder->GetJSObject(&mCompMgrWrapper);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "failed to get JSObject for comp mgr wrapper\n");
#endif
        return rv;
    }
    JS_SetErrorReporter(mContext, Reporter);

    mModules = PL_NewHashTable(16, PL_HashString, PL_CompareStrings,
                               PL_CompareValues, 0, 0);
    if (!mModules)
        return NS_ERROR_OUT_OF_MEMORY;
    mGlobals = PL_NewHashTable(16, PL_HashString, PL_CompareStrings,
                               PL_CompareValues, 0, 0);
    if (!mGlobals)
        return NS_ERROR_OUT_OF_MEMORY;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: context initialized!\n");
#endif
    mInitialized = PR_TRUE;

    /* root last, so that we don't leak the roots in case of failure */
    JS_AddNamedRoot(mContext, &mSuperGlobal, "mJCL::mSuperGlobal");
    JS_AddNamedRoot(mContext, &mCompMgrWrapper, "mJCL::mCompMgrWrapper");
    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::AutoRegisterComponents(PRInt32 when,
                                             nsIFile *aDirectory)
{
    return RegisterComponentsInDir(when, aDirectory);
}

nsresult
mozJSComponentLoader::RegisterComponentsInDir(PRInt32 when, nsIFile *dir)
{
    nsresult rv;
    PRBool isDir;
    
    if (NS_FAILED(rv = dir->IsDirectory(&isDir)))
        return rv;
    
    if (!isDir)
        return NS_ERROR_INVALID_ARG;

    // Create a directory iterator
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = dir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    
    if (NS_FAILED(rv)) return rv;
    
   // whip through the directory to register every file
    nsIFile *dirEntry = NULL;
    PRBool more = PR_FALSE;

    rv = dirIterator->HasMoreElements(&more);
    if (NS_FAILED(rv)) return rv;
    while (more == PR_TRUE)
    {
        rv = dirIterator->GetNext((nsISupports**)&dirEntry);
        if (NS_SUCCEEDED(rv))
        {
            rv = dirEntry->IsDirectory(&isDir);
            if (NS_SUCCEEDED(rv))
            {
                if (isDir == PR_TRUE)
                {
                    // This is a directory. Grovel for components into the directory.
                    rv = RegisterComponentsInDir(when, dirEntry);
                }
                else
                {
                    PRBool registered;
                    // This is a file. Try to register it.
                    rv = AutoRegisterComponent(when, dirEntry, &registered);
                }
            }
            NS_RELEASE(dirEntry);
        }
        rv = dirIterator->HasMoreElements(&more);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


nsresult
mozJSComponentLoader::SetRegistryInfo(const char *registryLocation,
                                      nsIFile *component)
{
    nsresult rv;
    if (!mRegistry.get())
        return NS_OK;           // silent failure

    PRUint32 length = strlen(registryLocation);
    char* eRegistryLocation;
    rv = mRegistry->EscapeKey((PRUint8*)registryLocation, 1, &length, (PRUint8**)&eRegistryLocation);
    if (rv != NS_OK)
    {
        return rv;
    }
    if (eRegistryLocation == nsnull)    //  No escaping required
    eRegistryLocation = (char*)registryLocation;


    nsRegistryKey key;

    rv = mRegistry->AddSubtreeRaw(mXPCOMKey, eRegistryLocation, &key);
    if (registryLocation != eRegistryLocation)
        nsMemory::Free(eRegistryLocation);

    if (NS_FAILED(rv))
        return rv;

    PRInt64 modDate;

    if (NS_FAILED(rv = component->GetLastModificationDate(&modDate)) ||
        NS_FAILED(rv = mRegistry->SetLongLong(key, lastModValueName, &modDate)))
        return rv;

    PRInt64 fileSize;
    if (NS_FAILED(rv = component->GetFileSize(&fileSize)) ||
        NS_FAILED(rv = mRegistry->SetLongLong(key, fileSizeValueName, &fileSize)))
        return rv;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "SetRegistryInfo(%s) => (%d,%d)\n", registryLocation,
            modDate, fileSize);
#endif

    return NS_OK;
}

nsresult
mozJSComponentLoader::RemoveRegistryInfo(const char *registryLocation)
{
    if (!mRegistry.get())
        return NS_OK;           // silent failure

    nsresult rv;
    if (!mRegistry.get())
        return NS_OK;           // silent failure

    PRUint32 length = strlen(registryLocation);
    char* eRegistryLocation;
    rv = mRegistry->EscapeKey((PRUint8*)registryLocation, 1, &length, (PRUint8**)&eRegistryLocation);
    if (rv != NS_OK)
    {
        return rv;
    }
    if (eRegistryLocation == nsnull)    //  No escaping required
    eRegistryLocation = (char*)registryLocation;


    rv = mRegistry->RemoveSubtree(mXPCOMKey, eRegistryLocation);
        
    if (registryLocation != eRegistryLocation)
            nsMemory::Free(eRegistryLocation);

    return rv;
}


PRBool
mozJSComponentLoader::HasChanged(const char *registryLocation,
                                 nsIFile *component)
{

    /* if we don't have a registry handle, force registration of component */
    if (!mRegistry)
        return PR_TRUE;

    nsresult rv;
    PRUint32 length = strlen(registryLocation);
    char* eRegistryLocation;
    rv = mRegistry->EscapeKey((PRUint8*)registryLocation, 1, &length, (PRUint8**)&eRegistryLocation);
    if (rv != NS_OK)
    {
        return rv;
    }
    if (eRegistryLocation == nsnull)    //  No escaping required
        eRegistryLocation = (char*)registryLocation;

    nsRegistryKey key;
    int r = NS_FAILED(mRegistry->GetSubtreeRaw(mXPCOMKey, eRegistryLocation, &key));
    if (registryLocation != eRegistryLocation)
        nsMemory::Free(eRegistryLocation);
    if (r)
        return PR_TRUE;

    /* check modification date */
    PRInt64 regTime, lastTime;
    if (NS_FAILED(mRegistry->GetLongLong(key, lastModValueName, &regTime)))
        return PR_TRUE;
    
    if (NS_FAILED(component->GetLastModificationDate(&lastTime)) || LL_NE(lastTime, regTime))
        return PR_TRUE;

    /* check file size */
    PRInt64 regSize;
    if (NS_FAILED(mRegistry->GetLongLong(key, fileSizeValueName, &regSize)))
        return PR_TRUE;
    PRInt64 size;
    if (NS_FAILED(component->GetFileSize(&size)) || LL_NE(size,regSize) )
        return PR_TRUE;

    return PR_FALSE;
}

static const PRUnichar sJSComponentReg[] = {'R','e','g','i','s','t','e','r',
    'i','n','g',' ','J', 'S', ' ', 'c', 'o', 'm', 'p', 'o',
    'n', 'e', 'n', 't',':',' ', 0};

static const PRUnichar sJSComponentUnreg[] = {'U','n','r','e','g','i','s','t','e','r',
    'i','n','g',' ','J', 'S', ' ', 'c', 'o', 'm', 'p', 'o',
    'n', 'e', 'n', 't',':',' ', 0};

NS_IMETHODIMP
mozJSComponentLoader::AutoRegisterComponent(PRInt32 when,
                                            nsIFile *component,
                                            PRBool *registered)
{
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;

    const char jsExtension[] = ".js";
    int jsExtensionLen = 3;
    nsXPIDLCString leafName;

    *registered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetLeafName(getter_Copies(leafName))))
        return rv;
    int len = PL_strlen(leafName);
    
    /* if it's not *.js, return now */
    if (len < jsExtensionLen || // too short
        PL_strcasecmp(leafName + len - jsExtensionLen, jsExtension))
        return NS_OK;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: registering JS component %s\n",
            (const char *)leafName);
#endif
      
    rv = AttemptRegistration(component, PR_FALSE);
#ifdef DEBUG_shaver
    if (NS_SUCCEEDED(rv))
        fprintf(stderr, "registered module %s\n", (const char *)leafName);
    else if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) 
        fprintf(stderr, "deferred module %s\n", (const char *)leafName);
    else
        fprintf(stderr, "failed to register %s\n", (const char *)leafName);
#endif    
    *registered = (PRBool) NS_SUCCEEDED(rv);
    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::AutoUnregisterComponent(PRInt32 when,
                                              nsIFile *component,
                                              PRBool *unregistered)
{
    nsresult rv;
    if (!unregistered)
        return NS_ERROR_NULL_POINTER;

    const char jsExtension[] = ".js";
    int jsExtensionLen = 3;
    nsXPIDLCString leafName;

    *unregistered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetLeafName(getter_Copies(leafName))))
        return rv;
    int len = PL_strlen(leafName);
    
    /* if it's not *.js, return now */
    if (len < jsExtensionLen || // too short
        PL_strcasecmp(leafName + len - jsExtensionLen, jsExtension))
        return NS_OK;

    rv = UnregisterComponent(component);
#ifdef DEBUG_dp
    if (NS_SUCCEEDED(rv))
        fprintf(stderr, "unregistered module %s\n", (const char *)leafName);
    else
        fprintf(stderr, "failed to unregister %s\n", (const char *)leafName);
#endif    
    *unregistered = (PRBool) NS_SUCCEEDED(rv);
    return NS_OK;
}

nsresult
mozJSComponentLoader::AttemptRegistration(nsIFile *component,
                                          PRBool deferred)
{
    nsXPIDLCString registryLocation;
    nsresult rv;
    nsIModule *module;

    rv = mCompMgr->RegistryLocationForSpec(component, 
                                           getter_Copies(registryLocation));
    if (NS_FAILED(rv))
        return rv;
    
    /* no need to check registry data on deferred reg */
    if (!deferred && !HasChanged(registryLocation, component))
        goto out;
    
    module = ModuleForLocation(registryLocation, component);
    if (!module)
        goto out;
    
    {
      // Notify observers, if any, of autoregistration work
      NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsIServiceManager *mgr;    // NO COMPtr as we dont release the service manager
        rv = nsServiceManager::GetGlobalServiceManager(&mgr);
        if (NS_SUCCEEDED(rv))
        {
          // this string can't come from a string bundle, because we don't have string
          // bundles yet.
          NS_ConvertASCIItoUCS2 statusMsg("Registering JS component ");            
          NS_ConvertASCIItoUCS2 fileName("(no name)");

          // get the file name
          if (component)
          {
            nsXPIDLCString leafName;
            component->GetLeafName(getter_Copies(leafName));
            fileName.AssignWithConversion(leafName);
          }
          statusMsg.Append(fileName);
          
          (void) observerService->Notify(mgr,
              NS_ConvertASCIItoUCS2(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID).GetUnicode(),
              statusMsg.GetUnicode());
        }
      }
    }
    
    rv = module->RegisterSelf(mCompMgr, component, registryLocation,
                              jsComponentTypeName);
    if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) {
        if (!deferred)
            mDeferredComponents.AppendElement(component);
        /*
         * we don't enter in the registry because we may want to
         * try again on a later autoreg, in case a dependency has
         * become available. 
         */
    } else{ 
 out:
        SetRegistryInfo(registryLocation, component);
    }

    return rv;
}

nsresult
mozJSComponentLoader::UnregisterComponent(nsIFile *component)
{
    nsXPIDLCString registryLocation;
    nsresult rv;
    nsIModule *module;

    rv = mCompMgr->RegistryLocationForSpec(component, 
                                           getter_Copies(registryLocation));
    if (NS_FAILED(rv))
        return rv;
    
    module = ModuleForLocation(registryLocation, component);
    if (!module)
        return NS_ERROR_FAILURE;
    
    {
      // Notify observers, if any, of autoregistration work
      NS_WITH_SERVICE (nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
      {
        nsIServiceManager *mgr;    // NO COMPtr as we dont release the service manager
        rv = nsServiceManager::GetGlobalServiceManager(&mgr);
        if (NS_SUCCEEDED(rv))
        {
          (void) observerService->Notify(mgr,
              NS_ConvertASCIItoUCS2(NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID).GetUnicode(),
              NS_ConvertASCIItoUCS2("Unregistering JS component").GetUnicode());
        }
      }
    }
    
    rv = module->UnregisterSelf(mCompMgr, component, registryLocation);

    if (NS_SUCCEEDED(rv))
    {
        // Remove any autoreg specific info. Ignore error.
        RemoveRegistryInfo(registryLocation);
    }
        
    return rv;
}


nsresult
mozJSComponentLoader::RegisterDeferredComponents(PRInt32 aWhen,
                                                 PRBool *aRegistered)
{
    nsresult rv;
    *aRegistered = PR_FALSE;

    PRUint32 count;
    rv = mDeferredComponents.Count(&count);
#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: registering deferred (%d)\n", count);
#endif
    if (NS_FAILED(rv) || !count)
        return NS_OK;
    
    for (PRUint32 i = 0; i < count; i++) {
        nsCOMPtr<nsIFile> component;
        rv = mDeferredComponents.QueryElementAt(i, NS_GET_IID(nsIFile), getter_AddRefs(component));
        if (NS_FAILED(rv))
            continue;

        rv = AttemptRegistration(component, PR_TRUE /* deferred */);
        if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
            if (NS_SUCCEEDED(rv))
                *aRegistered = PR_TRUE;
            mDeferredComponents.RemoveElementAt(i);
        }
    }

#ifdef DEBUG_shaver
    rv = mDeferredComponents.Count(&count);
    if (NS_SUCCEEDED(rv)) {
        if (*aRegistered)
            fprintf(stderr, "mJCL: registered deferred, %d left\n",
                    count);
        else
            fprintf(stderr, "mJCL: didn't register any components, %d left\n",
                    count);
    }
#endif
    /* are there any fatal errors? */
    return NS_OK;
}


nsIModule *
mozJSComponentLoader::ModuleForLocation(const char *registryLocation,
                                        nsIFile *component)
{
    nsIModule *module = nsnull;
    if (!mInitialized &&
        NS_FAILED(ReallyInit()))
        return nsnull;

    PLHashNumber hash = PL_HashString(registryLocation);
    PLHashEntry **hep = PL_HashTableRawLookup(mModules, hash,
                                              (void *)registryLocation);
    PLHashEntry *he = *hep;
    if (he)
        return (nsIModule *)he->value;

    JSObject *obj = GlobalForLocation(registryLocation, component);
    if (!obj) {
#ifdef DEBUG_shaver
        fprintf(stderr, "GlobalForLocation failed!\n");
#endif
        return nsnull;
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID);
    if (!xpc)
        return nsnull;

    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, cxstack, kJSContextStackContractID, &rv);
    if (NS_FAILED(rv) ||
        NS_FAILED(cxstack->Push(mContext)))
        return nsnull;

    /* from here on, return via out: to pop from cxstack */

    jsval argv[2], retval;
    argv[0] = OBJECT_TO_JSVAL(mCompMgrWrapper);
    argv[1] = STRING_TO_JSVAL(JS_NewStringCopyZ(mContext, registryLocation));
    if (!JS_CallFunctionName(mContext, obj, "NSGetModule", 2, argv,
                             &retval)) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: NSGetModule failed for %s\n",
                registryLocation);
#endif
        goto out;
    }

#ifdef DEBUG_shaver_off
    JSString *s = JS_ValueToString(mContext, retval);
    fprintf(stderr, "mJCL: %s::NSGetModule returned %s\n",
            registryLocation, JS_GetStringBytes(s));
#endif

    JSObject *jsModuleObj;
    if (!JS_ValueToObject(mContext, retval, &jsModuleObj)) {
        /* XXX report error properly */
        fprintf(stderr, "mJCL: couldn't convert %s's nsIModule to obj\n",
                registryLocation);
        goto out;
    }

    if (NS_FAILED(xpc->WrapJS(mContext, jsModuleObj, NS_GET_IID(nsIModule),
                              (void **)&module))) {
        /* XXX report error properly */
        fprintf(stderr, "mJCL: couldn't get nsIModule from jsval\n");
        goto out;
    }

    /* we hand our reference to the hash table, it'll be released much later */
    he = PL_HashTableRawAdd(mModules, hep, hash,
                            nsCRT::strdup(registryLocation), module);
 out:
    JSContext *cx;
    cxstack->Pop(&cx);
    NS_ASSERTION(cx == mContext, "JS context stack push/pop mismatch!");
    return module;
}

JSObject *
mozJSComponentLoader::GlobalForLocation(const char *aLocation,
                                        nsIFile *component)
{
    JSObject *obj = nsnull;
    PRBool needRelease = PR_FALSE;
    PLHashNumber hash = PL_HashString(aLocation);
    PLHashEntry **hep = PL_HashTableRawLookup(mGlobals, hash,
                                              (void *)aLocation);
    PLHashEntry *he = *hep;
    if (he)
        return (JSObject *)he->value;

    if (!mInitialized &&
        NS_FAILED(ReallyInit()))
        return nsnull;
    
    nsresult rv;
    JSPrincipals* jsPrincipals = nsnull;

#ifndef XPCONNECT_STANDALONE
    nsCOMPtr<nsIScriptObjectPrincipal> backstagePass =
      new BackstagePass(mSystemPrincipal);

    rv = mSystemPrincipal->GetJSPrincipals(&jsPrincipals);
    if (NS_FAILED(rv) || !jsPrincipals)
      return nsnull;

#else
    nsCOMPtr<nsISupports> backstagePass = new BackstagePass();
#endif

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID);
    if (!xpc)
      return nsnull;

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(mContext, mSuperGlobal, backstagePass,
                         NS_GET_IID(nsISupports),
                         getter_AddRefs(holder));
    if (NS_FAILED(rv))
        return nsnull;
    
    rv = holder->GetJSObject(&obj);
    if (NS_FAILED(rv))
        return nsnull;

    /* fix the scope and prototype links for cloning and scope containment */
    if (!JS_SetPrototype(mContext, obj, mSuperGlobal) ||
        !JS_SetParent(mContext, obj, nsnull))
      return nsnull;

    if (!component) {
        if (NS_FAILED(mCompMgr->SpecForRegistryLocation(aLocation,
                                                        &component)))
            return nsnull;
        needRelease = PR_TRUE;
    }

    NS_WITH_SERVICE(nsIJSContextStack, cxstack, kJSContextStackContractID, &rv);
    if (NS_FAILED(rv) ||
        NS_FAILED(cxstack->Push(mContext)))
        return nsnull;
    
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(component);

    if (!localFile)
        return nsnull;

    char *location;             // declare before first jump to out:
    jsval retval;
    nsXPIDLCString displayPath;
    FILE* fileHandle;
    
    localFile->GetPath(getter_Copies(displayPath));   
    rv = localFile->OpenANSIFileDesc("r", &fileHandle);
    if (NS_FAILED(rv))
        return nsnull;

    JSScript *script = 
        JS_CompileFileHandleForPrincipals(mContext, obj,
                                          (const char *)displayPath, 
                                          fileHandle, jsPrincipals);
    
    /* JS will close the filehandle after compilation is complete. */

    if (!script) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: script compilation of %s FAILED\n",
                nativePath);
#endif
        obj = nsnull;
        goto out;
    }

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: compiled JS component %s\n", nativePath);
#endif

    if (!JS_ExecuteScript(mContext, obj, script, &retval)) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: failed to execute %s\n", nativePath);
#endif
        obj = nsnull;
        goto out;
    }

    /* freed when we remove from the table */
    location = nsCRT::strdup(aLocation);
    he = PL_HashTableRawAdd(mGlobals, hep, hash, location, obj);
    JS_AddNamedRoot(mContext, &he->value, location);

 out:
    if (script)
        JS_DestroyScript(mContext, script);
    JSContext *cx;
    rv = cxstack->Pop(&cx);
    NS_ASSERTION(NS_SUCCEEDED(rv) && cx == mContext,
                 "JS context stack push/pop mismatch!");
    if (needRelease)
        NS_RELEASE(component);
    return obj;
}

NS_IMETHODIMP
mozJSComponentLoader::OnRegister(const nsIID &aCID, const char *aType,
                                 const char *aClassName, const char *aContractID,
                                 const char *aLocation,
                                 PRBool aReplace, PRBool aPersist)

{
#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: registered %s/%s in %s\n", aClassName, aContractID,
            aLocation);
#endif
    return NS_OK;
}    

NS_IMETHODIMP
mozJSComponentLoader::UnloadAll(PRInt32 aWhen)
{
    if (mInitialized) {

        // stabilize the component manager, etc.
        nsCOMPtr<nsIComponentManager> kungFuDeathGrip = mCompMgr;

        PL_HashTableEnumerateEntries(mModules, UnloadAndReleaseModules,
                                     mCompMgr);
        JS_MaybeGC(mContext);
    }

#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: UnloadAll(%d)\n", aWhen);
#endif

    return NS_OK;
}

//----------------------------------------------------------------------

/* XXX this should all be data-driven, via NS_IMPL_GETMODULE_WITH_CATEGORIES */
static NS_METHOD
RegisterJSLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
                 const char *registryLocation, const char *componentType)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("component-loader", jsComponentTypeName,
                                    mozJSComponentLoaderContractID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD
UnregisterJSLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
                   const char *registryLocation)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString jsLoader;
    rv = catman->GetCategoryEntry("component-loader", jsComponentTypeName,
                                  getter_Copies(jsLoader));
    if (NS_FAILED(rv)) return rv;

    // only unregister if we're the current JS component loader
    if (!strcmp(jsLoader, mozJSComponentLoaderContractID)) {
        return catman->DeleteCategoryEntry("component-loader",
                                           jsComponentTypeName, PR_TRUE,
                                           getter_Copies(jsLoader));
    }
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader);

static nsModuleComponentInfo components[] = {
    { "JS component loader", MOZJSCOMPONENTLOADER_CID,
      mozJSComponentLoaderContractID, mozJSComponentLoaderConstructor,
      RegisterJSLoader, UnregisterJSLoader }
};

NS_IMPL_NSGETMODULE("JS component loader", components);
