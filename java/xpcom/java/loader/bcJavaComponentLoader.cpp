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
#include "bcJavaComponentLoader.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"    
#include "nsCRT.h"
#include "bcJavaModule.h"


/*********************************************************************************/

//#include "bcIJavaSample.h"
#include "unistd.h"
#include "signal.h"


/********************************************************************************/
        
const char javaComponentTypeName[] = JAVACOMPONENTTYPENAME;
extern const char xpcomKeyName[];

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
/* void onRegister (in nsIIDRef aCID, in string aType, in string aClassName, in string aProgID, in string aLocation, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP bcJavaComponentLoader::OnRegister(const nsIID & aCID, const char *aType, const char *aClassName, const char *aProgID, const char *aLocation, PRBool aReplace, PRBool aPersist) { //nb
    printf("--bcJavaComponentLoader::OnRegister \n");
    return NS_OK;
}

/**
 * AutoRegister components in the given directory.
 */
/* void autoRegisterComponents (in long aWhen, in nsIFile aDirectory); */
NS_IMETHODIMP bcJavaComponentLoader::AutoRegisterComponents(PRInt32 aWhen, nsIFile *aDirectory) {
    //printf("--bcJavaComponentLoader::AutoRegisterComponents \n");
    return RegisterComponentsInDir(aWhen,aDirectory);
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
            modDate, fileSize);
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
    nsStringKey key(registryLocation);
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



NS_GENERIC_FACTORY_CONSTRUCTOR(bcJavaComponentLoader)
    
static  nsModuleComponentInfo components[] =
{
    {
        "Java Component Loader",
        BC_JAVACOMPONENTLOADER_CID,
	BC_JAVACOMPONENTLOADER_PROGID,
        bcJavaComponentLoaderConstructor
    }
};


/* copied-and-pasted from mozJSComponentLoader */
#include "nsHashtable.h"
class bcJavaComponentLoaderModule : public nsIModule
{
public:
    bcJavaComponentLoaderModule(const char *moduleName, PRUint32 componentCount,
                nsModuleComponentInfo *components);
    virtual ~bcJavaComponentLoaderModule();
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool                      mInitialized;
    const char*                 mModuleName;
    PRUint32                    mComponentCount;
    nsModuleComponentInfo*      mComponents;
    nsSupportsHashtable         mFactories;
};

bcJavaComponentLoaderModule::bcJavaComponentLoaderModule(const char* moduleName, PRUint32 componentCount,
                         nsModuleComponentInfo* aComponents)
    : mInitialized(PR_FALSE), 
      mModuleName(moduleName),
      mComponentCount(componentCount),
      mComponents(aComponents),
      mFactories(8, PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

bcJavaComponentLoaderModule::~bcJavaComponentLoaderModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS1(bcJavaComponentLoaderModule, nsIModule)

// Perform our one-time intialization for this module
nsresult
bcJavaComponentLoaderModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
bcJavaComponentLoaderModule::Shutdown()
{
    // Release the factory objects
    mFactories.Reset();
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
bcJavaComponentLoaderModule::GetClassObject(nsIComponentManager *aCompMgr,
                                const nsCID& aClass,
                                const nsIID& aIID,
                                void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsIDKey key(aClass);
    nsCOMPtr<nsIGenericFactory> fact = getter_AddRefs(NS_REINTERPRET_CAST(nsIGenericFactory *, mFactories.Get(&key)));
    if (fact == nsnull) {
        nsModuleComponentInfo* desc = mComponents;
        for (PRUint32 i = 0; i < mComponentCount; i++) {
            if (desc->mCID.Equals(aClass)) {
                rv = NS_NewGenericFactory(getter_AddRefs(fact), desc->mConstructor);
                if (NS_FAILED(rv)) return rv;

                (void)mFactories.Put(&key, fact);
                goto found;
            }
            desc++;
        }
        // not found in descriptions
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsGenericModule %s: unable to create factory for %s\n", mModuleName, cs);
        nsCRT::free(cs);
#endif
        // XXX put in stop-gap so that we don't search for this one again
		return NS_ERROR_FACTORY_NOT_REGISTERED;
    }
  found:    
    rv = fact->QueryInterface(aIID, r_classObj);
    return rv;
}

NS_IMETHODIMP
bcJavaComponentLoaderModule::RegisterSelf(nsIComponentManager *aCompMgr,
                              nsIFile* aPath,
                              const char* registryLocation,
                              const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering %s components (all right -- an almost-generic module!)\n", mModuleName);
#endif

    nsModuleComponentInfo* cp = mComponents;
    for (PRUint32 i = 0; i < mComponentCount; i++) {
        rv = aCompMgr->RegisterComponentSpec(cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsGenericModule %s: unable to register %s component => %x\n",
                   mModuleName, cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }
    printf("JavaComponentLoaderModule::RegisterSelf \n");
    return aCompMgr->RegisterComponentLoader(javaComponentTypeName,
					     BC_JAVACOMPONENTLOADER_PROGID,
                                             PR_TRUE);
}

NS_IMETHODIMP
bcJavaComponentLoaderModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFile* aPath,
                            const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering %s components (all right -- an almost-generic module!)\n", mModuleName);
#endif
    nsModuleComponentInfo* cp = mComponents;
    for (PRUint32 i = 0; i < mComponentCount; i++) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsGenericModule %s: unable to unregister %s component => %x\n",
                   mModuleName, cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
bcJavaComponentLoaderModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    printf("--bcJavaComponentLoaderModule::CanUnload\n");
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_TRUE;
    return NS_OK;
}

NS_EXPORT nsresult
NS_NewJavaComponentLoaderModule(const char* moduleName,
               PRUint32 componentCount,
               nsModuleComponentInfo* aComponents,
               nsIModule* *result)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(result, "Null argument");

    // Create and initialize the module instance
    bcJavaComponentLoaderModule *m =  new bcJavaComponentLoaderModule(moduleName, componentCount, aComponents);
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)result);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    return rv;
}

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *compMgr,
                                          nsIFile *location,
                                          nsIModule** result)
{
    return NS_NewJavaComponentLoaderModule("bcJavaComponentLoaderModule",
                          sizeof(components) / sizeof(components[0]),
                          components, result);
}

