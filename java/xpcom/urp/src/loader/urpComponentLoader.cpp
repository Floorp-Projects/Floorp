/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

/*
  A bunch  of stuff was copied from mozJSComponentLoader.cpp
 */
#include <fstream.h>
#include "nsICategoryManager.h"
#include "urpComponentLoader.h"
#include "urpComponentFactory.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"    
#include "nsCRT.h"

#include "../urpLog.h"

        
const char urpComponentTypeName[] = URPCOMPONENTTYPENAME;

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

NS_IMPL_THREADSAFE_ISUPPORTS(urpComponentLoader,NS_GET_IID(nsIComponentLoader));

urpComponentLoader::urpComponentLoader() 
    : mCompMgr(NULL),
      
      mXPCOMKey(0)
{
    NS_INIT_REFCNT();
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::urpComponentLoader \n"));
}

urpComponentLoader::~urpComponentLoader() { //nb
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::~urpComponentLoader \n"));
}


/**
 * Get the factory for a given component.
 */
/* nsIFactory getFactory (in nsIIDRef aCID, in string aLocation, in string aType); */
NS_IMETHODIMP urpComponentLoader::GetFactory(const nsIID & aCID, const char *aLocation, const char *aType, nsIFactory **_retval) { 
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::GetFactory \n"));
        if (!_retval)
        return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
    char *cidString = aCID.ToString();
    fprintf(stderr, "--urpComponentLoader::GetFactory(%s,%s,%s)\n", cidString, aLocation, aType);
    delete [] cidString;
#endif
//    nsIFactory *f = new urpComponentFactory(aLocation, aCID, proxy);
    nsIFactory *f = new urpComponentFactory(aLocation, aCID);
    NS_ADDREF(f);
    *_retval = f;
    return NS_OK;
}

/**
 * Initialize the loader.
 *
 * We use nsISupports here because nsIRegistry isn't IDLized yet.
 */
/* void init (in nsIComponentManager aCompMgr, in nsISupports aRegistry); */
NS_IMETHODIMP urpComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg) {
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::Init \n"));
    nsresult rv;
    mCompMgr = aCompMgr;
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

/**
 * Called when a component of the appropriate type is registered,
 * to give the component loader an opportunity to do things like
 * annotate the registry and such.
 */
/* void onRegister (in nsIIDRef aCID, in string aType, in string aClassName, in string aContractID, in string aLocation, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP urpComponentLoader::OnRegister(const nsIID & aCID, const char *aType, const char *aClassName, const char *aContractID, const char *aLocation, PRBool aReplace, PRBool aPersist) { //nb
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::OnRegister \n"));
    return NS_OK;
}

/**
 * AutoRegister components in the given directory.
 */
NS_IMETHODIMP urpComponentLoader::AutoRegisterComponents(PRInt32 aWhen, nsIFile *aDirectory) {
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::AutoRegisterComponents \n"));
    return RegisterComponentsInDir(aWhen,aDirectory);
}

NS_IMETHODIMP urpComponentLoader::AutoUnregisterComponent(PRInt32 when,
                                                             nsIFile *component,
                                                             PRBool *unregistered) {
    //nb need to impelement
    return NS_OK;
}

nsresult urpComponentLoader::RegisterComponentsInDir(PRInt32 when, nsIFile *dir)
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

/**
 * AutoRegister the given component.
 *
 * Returns true if the component was registered, false if it couldn't
 * attempt to register the component (wrong type) and ``throws'' an
 * NS_FAILED code if there was an error during registration.
 */
/* boolean autoRegisterComponent (in long aWhen, in nsIFile aComponent); */

/* copied from mozJSComponentLoader.cpp */
NS_IMETHODIMP urpComponentLoader::AutoRegisterComponent(PRInt32 when, nsIFile *component, PRBool *registered) {
    //printf("--urpComponentLoader::AutoRegisterComponent \n");
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;
    
    const char javaExtension[] = ".urp.info";
    int javaExtensionLen = 9;
    nsXPIDLCString leafName;

    *registered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetLeafName(getter_Copies(leafName))))
        return rv;
    int len = PL_strlen(leafName);
    
    /* if it's not javaExtension return now */
    if (len < javaExtensionLen || // too short
        PL_strcasecmp(leafName + len - javaExtensionLen, javaExtension))
        return NS_OK;

    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader: registering urpComponent component %s\n",(const char *)leafName));
    rv = AttemptRegistration(component, PR_FALSE);
    if (NS_SUCCEEDED(rv))
        PR_LOG(log, PR_LOG_DEBUG, ("registered module %s\n", (const char *)leafName));
    else if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) 
        PR_LOG(log, PR_LOG_DEBUG, ("deferred module %s\n", (const char *)leafName));
    else
        PR_LOG(log, PR_LOG_DEBUG, ("failed to register %s\n", (const char *)leafName));
    *registered = (PRBool) NS_SUCCEEDED(rv);
    return NS_OK;

}


nsresult urpComponentLoader::AttemptRegistration(nsIFile *component,
                                          PRBool deferred) {
    nsXPIDLCString registryLocation;
    nsresult rv;
    nsIModule *module;

    rv = mCompMgr->RegistryLocationForSpec(component, 
                                           getter_Copies(registryLocation));
    nsXPIDLCString str;
    component->GetPath(getter_Copies(str));
    const char* location = nsCRT::strdup(str);
    ifstream in(location);
    char cidStr[500], contractid[1000], desc[1000];
    in.getline(cidStr,1000);
    in.getline(contractid,1000);
    in.getline(desc,1000);
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("%s %s %s", cidStr, contractid, desc));
    nsCID  cid;
    cid.Parse((const char *)cidStr);
    mCompMgr->RegisterComponentWithType(cid, desc, contractid, component, registryLocation, PR_TRUE, PR_TRUE, urpComponentTypeName);
    if (NS_FAILED(rv))
        return rv;
    
    /* no need to check registry data on deferred reg */
    SetRegistryInfo(registryLocation, component);
    return rv;
}

nsresult urpComponentLoader::SetRegistryInfo(const char *registryLocation,
                                      nsIFile *component)
{
    if (!mRegistry.get())
        return NS_OK;           // silent failure

    nsresult rv;
    nsRegistryKey key;

    rv = mRegistry->AddSubtreeRaw(mXPCOMKey, registryLocation, &key);
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
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("SetRegistryInfo(%s) => (%d,%d)\n", registryLocation,
            (int)modDate, (int)fileSize));
    return NS_OK;
}


PRBool urpComponentLoader::HasChanged(const char *registryLocation, nsIFile *component) {
    /* if we don't have a registry handle, force registration of component */
    if (!mRegistry)
        return PR_TRUE;

    nsRegistryKey key;
    if (NS_FAILED(mRegistry->GetSubtreeRaw(mXPCOMKey, registryLocation, &key)))
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

/**
 * Register any deferred (NS_ERROR_FACTORY_REGISTER_AGAIN) components.
 * Return registered-any-components?
 */
/* boolean registerDeferredComponents (in long aWhen); */
NS_IMETHODIMP urpComponentLoader::RegisterDeferredComponents(PRInt32 aWhen, PRBool *aRegistered) {
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::RegisterDeferredComponents \n"));
    nsresult rv;
    *aRegistered = PR_FALSE;
    PRUint32 count;
    rv = mDeferredComponents.Count(&count);
    printf("mJCL: registering deferred (%d)\n", count);
    if (NS_FAILED(rv) || !count)
        return NS_OK;
    
    for (PRUint32 i = 0; i < count; i++) {
        nsCOMPtr<nsISupports> supports;
        nsCOMPtr<nsIFile> component;

        rv = mDeferredComponents.GetElementAt(i, getter_AddRefs(supports));
        if (NS_FAILED(rv))
            continue;

        component = do_QueryInterface(supports, &rv);
        if (NS_FAILED(rv))
            continue;
        
        rv = AttemptRegistration(component, PR_TRUE /* deferred */);
        if (rv != NS_ERROR_FACTORY_REGISTER_AGAIN) {
            if (NS_SUCCEEDED(rv))
                *aRegistered = PR_TRUE;
            mDeferredComponents.RemoveElementAt(i);
        }
    }
    rv = mDeferredComponents.Count(&count);
    if (NS_SUCCEEDED(rv)) {
        if (*aRegistered)
            printf("mJCL: registered deferred, %d left\n", count);
        else
            printf("mJCL: didn't register any components, %d left\n", count);
    }
    /* are there any fatal errors? */
    return NS_OK;
}


/**
 * Unload all components that are willing.
 */
/* void unloadAll (in long aWhen); */
NS_IMETHODIMP urpComponentLoader::UnloadAll(PRInt32 aWhen) { //nb
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentLoader::UnloadAll \n"));
    return NS_OK;
}



//---------------------------------------------------------------------------------------------------

/* XXX this should all be data-driven, via NS_IMPL_GETMODULE_WITH_CATEGORIES */
static NS_METHOD
RegisterRemoteLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
		 const char *registryLocation, const char *componentType, const nsModuleComponentInfo *info)
{
    PRLogModuleInfo *log = urpLog::GetLog();
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    PR_LOG(log, PR_LOG_DEBUG, ("--URPLoader got registered\n"));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    rv = catman->AddCategoryEntry("component-loader", urpComponentTypeName,
                                    URP_COMPONENTLOADER_ContractID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
    if(NS_FAILED(rv))
	PR_LOG(log, PR_LOG_DEBUG, ("Adding of remote-comp-loader is failed\n"));
    else
	PR_LOG(log, PR_LOG_DEBUG, ("Adding of remote-comp-loader is succeseded\n"));
    nsXPIDLCString urpLoader;
    rv = catman->GetCategoryEntry("component-loader", urpComponentTypeName,
                                  getter_Copies(urpLoader));
    if (NS_FAILED(rv)) 
	PR_LOG(log, PR_LOG_DEBUG, ("didn't got category entry\n"));
    else
	PR_LOG(log, PR_LOG_DEBUG, ("got category entry\n"));

    return rv;

}

static NS_METHOD
UnregisterRemoteLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
		   const char *registryLocation, const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString urpLoader;
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("URP component loader is being unregistered\n"));
    rv = catman->GetCategoryEntry("component-loader", urpComponentTypeName,
                                  getter_Copies(urpLoader));
    if (NS_FAILED(rv)) return rv;

    // only unregister if we're the current JS component loader
    if (!strcmp(urpLoader, URP_COMPONENTLOADER_ContractID)) {
        return catman->DeleteCategoryEntry("component-loader",
					   urpComponentTypeName, PR_TRUE,
					   getter_Copies(urpLoader));
    }
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(urpComponentLoader);
static nsModuleComponentInfo components[] = {
    { "URP component loader", URP_COMPONENTLOADER_CID,
      URP_COMPONENTLOADER_ContractID, 
      urpComponentLoaderConstructor,
      RegisterRemoteLoader, UnregisterRemoteLoader }
};

NS_IMPL_NSGETMODULE("URP component loader", components);



