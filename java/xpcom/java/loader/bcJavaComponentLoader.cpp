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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

/*
  A bunch  of stuff was copied from mozJSComponentLoader.cpp
 */
#include "nsICategoryManager.h"
#include "bcJavaComponentLoader.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"    
#include "nsCRT.h"
#include "bcJavaModule.h"


        
const char javaComponentTypeName[] = JAVACOMPONENTTYPENAME;

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

NS_IMPL_ISUPPORTS(bcJavaComponentLoader,NS_GET_IID(nsIComponentLoader));

bcJavaComponentLoader::bcJavaComponentLoader() 
    : mCompMgr(NULL),
      
      mXPCOMKey(0)
{
    NS_INIT_REFCNT();
    printf("--bcJavaComponentLoader::bcJavaComponentLoader \n");
}

bcJavaComponentLoader::~bcJavaComponentLoader() { //nb
    printf("--bcJavaComponentLoader::~bcJavaComponentLoader \n");
}


/**
 * Get the factory for a given component.
 */
/* nsIFactory getFactory (in nsIIDRef aCID, in string aLocation, in string aType); */
NS_IMETHODIMP bcJavaComponentLoader::GetFactory(const nsIID & aCID, const char *aLocation, const char *aType, nsIFactory **_retval) { 
    printf("--bcJavaComponentLoader::GetFactory \n");
        if (!_retval)
        return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
    char *cidString = aCID.ToString();
    fprintf(stderr, "--bcJavaComponentLoader::GetFactory(%s,%s,%s)\n", cidString, aLocation, aType);
    delete [] cidString;
#endif
    nsIModule * module = ModuleForLocation(aLocation, 0);
    if (!module) {
#ifdef DEBUG
        fprintf(stderr, "ERROR: couldn't get module for %s\n", aLocation);
#endif
        return NS_ERROR_FACTORY_NOT_LOADED;
    }
    
    nsresult rv = module->GetClassObject(mCompMgr, aCID,
                                         NS_GET_IID(nsIFactory),
                                         (void **)_retval);
#ifdef DEBUG
    fprintf(stderr, "GetClassObject %s\n", NS_FAILED(rv) ? "FAILED" : "ok");
#endif
    return rv;
}

/**
 * Initialize the loader.
 *
 * We use nsISupports here because nsIRegistry isn't IDLized yet.
 */
/* void init (in nsIComponentManager aCompMgr, in nsISupports aRegistry); */
NS_IMETHODIMP bcJavaComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg) {
    printf("--bcJavaComponentLoader::Init \n");
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
NS_IMETHODIMP bcJavaComponentLoader::OnRegister(const nsIID & aCID, const char *aType, const char *aClassName, const char *aContractID, const char *aLocation, PRBool aReplace, PRBool aPersist) { //nb
    printf("--bcJavaComponentLoader::OnRegister \n");
    return NS_OK;
}

/**
 * AutoRegister components in the given directory.
 */
NS_IMETHODIMP bcJavaComponentLoader::AutoRegisterComponents(PRInt32 aWhen, nsIFile *aDirectory) {
    printf("--bcJavaComponentLoader::AutoRegisterComponents \n");
    return RegisterComponentsInDir(aWhen,aDirectory);
}

NS_IMETHODIMP bcJavaComponentLoader::AutoUnregisterComponent(PRInt32 when,
                                                             nsIFile *component,
                                                             PRBool *unregistered) {
    //nb need to impelement
    return NS_OK;
}

nsresult bcJavaComponentLoader::RegisterComponentsInDir(PRInt32 when, nsIFile *dir)
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
NS_IMETHODIMP bcJavaComponentLoader::AutoRegisterComponent(PRInt32 when, nsIFile *component, PRBool *registered) {
    //printf("--bcJavaComponentLoader::AutoRegisterComponent \n");
    nsresult rv;
    if (!registered)
        return NS_ERROR_NULL_POINTER;
    
    const char javaExtension[] = ".jar.info";
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

    printf("--bcJavaComponentLoader: registering bcJavaComponent component %s\n",(const char *)leafName);
    rv = AttemptRegistration(component, PR_FALSE);
    if (NS_SUCCEEDED(rv))
        printf("registered module %s\n", (const char *)leafName);
    else if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) 
        printf("deferred module %s\n", (const char *)leafName);
    else
        printf("failed to register %s\n", (const char *)leafName);
    *registered = (PRBool) NS_SUCCEEDED(rv);
    return NS_OK;

}


nsresult bcJavaComponentLoader::AttemptRegistration(nsIFile *component,
                                          PRBool deferred) {
    nsXPIDLCString registryLocation;
    nsresult rv;
    nsIModule *module;

    rv = mCompMgr->RegistryLocationForSpec(component, 
                                           getter_Copies(registryLocation));
    if (NS_FAILED(rv))
        return rv;
    
    /* no need to check registry data on deferred reg */
    if (deferred || HasChanged(registryLocation, component)) {
	module = ModuleForLocation(registryLocation, component);
	if (module) {
	    rv = module->RegisterSelf(mCompMgr, component, registryLocation,
				      javaComponentTypeName);
	    if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) {
		mDeferredComponents.AppendElement(component);
		/*
		 * we don't enter in the registry because we may want to
		 * try again on a later autoreg, in case a dependency has
		 * become available. 
		 */
		return rv;
	    }
	}
    }
    SetRegistryInfo(registryLocation, component);
    return rv;
}

nsresult bcJavaComponentLoader::SetRegistryInfo(const char *registryLocation,
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
    printf("SetRegistryInfo(%s) => (%d,%d)\n", registryLocation,
            (int)modDate, (int)fileSize);
    return NS_OK;
}


PRBool bcJavaComponentLoader::HasChanged(const char *registryLocation, nsIFile *component) {
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

nsIModule * bcJavaComponentLoader::ModuleForLocation(const char *registryLocation, nsIFile *component) {
    nsStringKey key((const PRUnichar *)registryLocation); //nb can I do this?
    nsIModule *res = NULL;
    res = (nsIModule*)mModules.Get(&key);
    PRBool needRelease = PR_FALSE;
    if (res) {
	return res;
    }
    if (!component) {
        if (NS_FAILED(mCompMgr->SpecForRegistryLocation(registryLocation, &component)))
            return NULL;
        needRelease = PR_TRUE;
    }
    res = new bcJavaModule(registryLocation, component);
    if (needRelease) {
        NS_IF_RELEASE(component);
    }

    if (res) {
        mModules.Put(&key,res);
    }
    return res;
}
/**
 * Register any deferred (NS_ERROR_FACTORY_REGISTER_AGAIN) components.
 * Return registered-any-components?
 */
/* boolean registerDeferredComponents (in long aWhen); */
NS_IMETHODIMP bcJavaComponentLoader::RegisterDeferredComponents(PRInt32 aWhen, PRBool *aRegistered) {
    printf("--bcJavaComponentLoader::RegisterDeferredComponents \n");
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
NS_IMETHODIMP bcJavaComponentLoader::UnloadAll(PRInt32 aWhen) { //nb
    printf("--bcJavaComponentLoader::UnloadAll \n");
    return NS_OK;
}



//---------------------------------------------------------------------------------------------------

/* XXX this should all be data-driven, via NS_IMPL_GETMODULE_WITH_CATEGORIES */
static NS_METHOD
RegisterJavaLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
		 const char *registryLocation, const char *componentType)
{
    printf("--JavaLoader got registered\n");
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("component-loader", javaComponentTypeName,
                                    BC_JAVACOMPONENTLOADER_ContractID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));

}

static NS_METHOD
UnregisterJavaLoader(nsIComponentManager *aCompMgr, nsIFile *aPath,
		   const char *registryLocation)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString javaLoader;
    rv = catman->GetCategoryEntry("component-loader", javaComponentTypeName,
                                  getter_Copies(javaLoader));
    if (NS_FAILED(rv)) return rv;

    // only unregister if we're the current JS component loader
    if (!strcmp(javaLoader, BC_JAVACOMPONENTLOADER_ContractID)) {
        return catman->DeleteCategoryEntry("component-loader",
					   javaComponentTypeName, PR_TRUE,
                                           getter_Copies(javaLoader));
    }
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(bcJavaComponentLoader);
static nsModuleComponentInfo components[] = {
    { "Java component loader", BC_JAVACOMPONENTLOADER_CID,
      BC_JAVACOMPONENTLOADER_ContractID, 
      bcJavaComponentLoaderConstructor,
      RegisterJavaLoader, UnregisterJavaLoader }
};

NS_IMPL_NSGETMODULE("Java component loader", components);



