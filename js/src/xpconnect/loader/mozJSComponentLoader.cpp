/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 */

#include "prlog.h"
#include "nsIComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIModule.h"
#include "mozJSComponentLoader.h"
#include "nsIGenericFactory.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"
#include "nsCRT.h"
#include "nsIAllocator.h"
#include "nsIRegistry.h"
#include "nsXPIDLString.h"

const char mozJSComponentLoaderProgID[] = "moz.jsloader.1";
const char jsComponentTypeName[] = "text/javascript";
/* XXX export properly from libxpcom, for now this will let Mac build */
const char fileSizeValueName[] = "FileSize";
const char lastModValueName[] = "LastModTimeStamp";
const char xpcomKeyName[] = "Software/Mozilla/XPCOM";

const char kJSRuntimeServiceProgID[] = "nsJSRuntimeService";
const char kXPConnectServiceProgID[] = "nsIXPConnect";
const char kJSContextStackProgID[] =   "nsThreadJSContextStack";

static JSBool
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
    nsAllocator::Free(bytes);
    return JS_TRUE;
}

static JSFunctionSpec gGlobalFun[] = {
    {"dump", Dump, 1 },
    {0}
};

static JSClass gGlobalClass = {
    "global", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

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

static PR_CALLBACK PRIntn
UnrootGlobals(PLHashEntry *he, PRIntn i, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    JS_RemoveRoot(cx, &he->value);
    nsCRT::free((char *)he->key);
    return HT_ENUMERATE_REMOVE;
}

static PR_CALLBACK PRIntn
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
        mCompMgrWrapper = nsnull; // release wrapper so GC can release CM
        mXPC->AbandonJSContext(mContext);

        JS_DestroyContext(mContext);
        mContext = nsnull;
        mXPC = nsnull;
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

static void
Reporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    fprintf(stderr, "JS Component Loader: ERROR %s:%d\n"
            "                     %s\n", rep->filename, rep->lineno,
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
    mRuntimeService = do_GetService(kJSRuntimeServiceProgID, &rv);
    if (NS_FAILED(rv) ||
        NS_FAILED(rv = mRuntimeService->GetRuntime(&mRuntime)))
        return rv;
    
    /*
     * Get the XPConnect service.
     */
    mXPC = do_GetService(kXPConnectServiceProgID, &rv);
    if (NS_FAILED(rv))
        return rv;

    mContext = JS_NewContext(mRuntime, 8192 /* pref? */);
    if (!mContext)
        return NS_ERROR_OUT_OF_MEMORY;

    mSuperGlobal = JS_NewObject(mContext, &gGlobalClass, NULL, NULL);
    if (!mSuperGlobal)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!JS_InitStandardClasses(mContext, mSuperGlobal) ||
        !JS_DefineFunctions(mContext, mSuperGlobal, gGlobalFun))
        return NS_ERROR_FAILURE;

    rv = mXPC->InitJSContext(mContext, mSuperGlobal, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;
    
    nsCOMPtr<nsIXPConnectWrappedNative> wrappedCM;
    rv = mXPC->WrapNative(mContext, mCompMgr, NS_GET_IID(nsIComponentManager),
                          getter_AddRefs(wrappedCM));
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "WrapNative(%p,%p,nsIComponentManager) failed: %x\n",
                mContext, mCompMgr, rv);
#endif
        return rv;
    }

    rv = wrappedCM->GetJSObject(&mCompMgrWrapper);
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
                                             nsIFileSpec *aDirectory)
{
    return RegisterComponentsInDir(when, aDirectory);
}

nsresult
mozJSComponentLoader::RegisterComponentsInDir(PRInt32 when, nsIFileSpec *dir)
{
    nsresult rv;
    PRBool isDir;
    
    if (NS_FAILED(rv = dir->IsDirectory(&isDir)))
        return rv;
    
    if (!isDir)
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIDirectoryIterator> dirIterator;
    rv = mCompMgr->CreateInstanceByProgID(NS_DIRECTORYITERATOR_PROGID, NULL,
                                  NS_GET_IID(nsIDirectoryIterator),
                                  getter_AddRefs(dirIterator));
    if (NS_FAILED(rv))
        return rv;
    if (NS_FAILED(rv = dirIterator->Init(dir, PR_FALSE)))
        return rv;
    
    nsCOMPtr<nsIFileSpec> dirEntry;
    PRBool more;
    if (NS_FAILED(rv = dirIterator->Exists(&more)))
        return rv;
    while(more) {
        rv = dirIterator->GetCurrentSpec(getter_AddRefs(dirEntry));
        if (NS_FAILED(rv))
            return rv;
        
        if (NS_FAILED(rv = dirEntry->IsDirectory(&isDir)))
            return rv;

        if (!isDir) {
            PRBool registered;
            rv = AutoRegisterComponent(when, dirEntry, &registered);
        } else {
            rv = RegisterComponentsInDir(when, dirEntry);
        }

        if (NS_FAILED(rv = dirIterator->Next()))
            return rv;
        if (NS_FAILED(rv = dirIterator->Exists(&more)))
            return rv;
    }

    return NS_OK;
}

nsresult
mozJSComponentLoader::SetRegistryInfo(const char *registryLocation,
                                      nsIFileSpec *component)
{
    if (!mRegistry.get())
        return NS_OK;           // silent failure

    nsresult rv;
    nsRegistryKey key;
    rv = mRegistry->GetSubtreeRaw(mXPCOMKey, registryLocation,
                                  &key);

    if (rv == NS_ERROR_REG_NOT_FOUND)
        rv = mRegistry->AddSubtreeRaw(mXPCOMKey, registryLocation, &key);

    if (NS_FAILED(rv))
        return rv;

    PRUint32 modDate;
    if (NS_FAILED(rv = component->GetModDate(&modDate)) ||
        NS_FAILED(rv = mRegistry->SetInt(key, lastModValueName, modDate)))
        return rv;

    PRUint32 fileSize;
    if (NS_FAILED(rv = component->GetFileSize(&fileSize)) ||
        NS_FAILED(rv = mRegistry->SetInt(key, fileSizeValueName, fileSize)))
        return rv;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "SetRegistryInfo(%s) => (%d,%d)\n", registryLocation,
            modDate, fileSize);
#endif

    return NS_OK;
}

PRBool
mozJSComponentLoader::HasChanged(const char *registryLocation,
                                 nsIFileSpec *component)
{

    /* if we don't have a registry handle, force registration of component */
    if (!mRegistry)
        return PR_TRUE;

    nsRegistryKey key;
    if (NS_FAILED(mRegistry->GetSubtreeRaw(mXPCOMKey, registryLocation, &key)))
        return PR_TRUE;

    /* check modification date */
    PRInt32 regTime;
    if (NS_FAILED(mRegistry->GetInt(key, lastModValueName, &regTime)))
        return PR_TRUE;
    PRBool changed;
    if (NS_FAILED(component->ModDateChanged(regTime, &changed)) || changed)
        return PR_TRUE;

    /* check file size */
    PRInt32 regSize;
    if (NS_FAILED(mRegistry->GetInt(key, fileSizeValueName, &regSize)))
        return PR_TRUE;
    PRUint32 size = 0;
    if (NS_FAILED(component->GetFileSize(&size)) || ((int32)size != regSize))
        return PR_TRUE;

    return PR_FALSE;
}

NS_IMETHODIMP
mozJSComponentLoader::AutoRegisterComponent(PRInt32 when,
                                            nsIFileSpec *component,
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

nsresult
mozJSComponentLoader::AttemptRegistration(nsIFileSpec *component,
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
        nsCOMPtr<nsISupports> supports;
        nsCOMPtr<nsIFileSpec> component;

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
                                        nsIFileSpec *component)
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

    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, cxstack, kJSContextStackProgID, &rv);
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

    if (NS_FAILED(mXPC->WrapJS(mContext, jsModuleObj, NS_GET_IID(nsIModule),
                               (nsISupports **)&module))) {
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
                                        nsIFileSpec *component)
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
    
    obj = JS_NewObject(mContext, &gGlobalClass, mSuperGlobal, NULL);
    if (!obj)
        return nsnull;

    if (!component) {
        if (NS_FAILED(mCompMgr->SpecForRegistryLocation(aLocation,
                                                        &component)))
            return nsnull;
        needRelease = PR_TRUE;
    }

    nsresult rv;
    NS_WITH_SERVICE(nsIJSContextStack, cxstack, kJSContextStackProgID, &rv);
    if (NS_FAILED(rv) ||
        NS_FAILED(cxstack->Push(mContext)))
        return nsnull;

    char *location;             // declare before first jump to out:
    jsval retval;
    nsXPIDLCString nativePath;
    /* XXX MAC: we use strings as file paths, *sigh* */
    component->GetNativePath(getter_Copies(nativePath));
    JSScript *script = JS_CompileFile(mContext, obj, (const char *)nativePath);
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
                                 const char *aClassName, const char *aProgID,
                                 const char *aLocation,
                                 PRBool aReplace, PRBool aPersist)

{
#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: registered %s/%s in %s\n", aClassName, aProgID,
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

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader)

static nsModuleComponentInfo components[] = {
    { "JS component loader", MOZJSCOMPONENTLOADER_CID,
      mozJSComponentLoaderProgID, mozJSComponentLoaderConstructor }
};

NS_IMPL_NSGETMODULE("mozJSComponentLoader", components)
