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
#include "nsIXPConnect.h"
#include "nsCRT.h"
#include "nsIAllocator.h"
#include "nsIRegistry.h"

const char mozJSComponentLoaderProgID[] = "moz.jsloader.1";
const char jsComponentTypeName[] = "text/javascript";

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
            *c == '\n';
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
{
    NS_INIT_REFCNT();
}

PR_STATIC_CALLBACK(PRIntn)
Release_enumerate(PLHashEntry *he, PRIntn cnt, void *arg)
{
    nsISupports *iface = (nsISupports *)he->value;
    NS_IF_RELEASE(iface);
    nsAllocator::Free((void *)he->key);
    return HT_ENUMERATE_NEXT;
}

PR_STATIC_CALLBACK(PRIntn)
RemoveRoot_enumerate(PLHashEntry *he, PRIntn cnt, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    JS_RemoveRoot(cx, &he->value);
    nsAllocator::Free((void *)he->key);
    return HT_ENUMERATE_NEXT;
}

mozJSComponentLoader::~mozJSComponentLoader()
{
    PL_HashTableEnumerateEntries(mModules, Release_enumerate, 0);
    PL_HashTableDestroy(mModules);

    if (mContext)
        PL_HashTableEnumerateEntries(mGlobals, RemoveRoot_enumerate, mContext);
    PL_HashTableDestroy(mGlobals);

    if (mContext) {
        JS_RemoveRoot(mContext, &mCompMgrWrapper);
        JS_RemoveRoot(mContext, &mSuperGlobal);
        if (mXPC)
            mXPC->AbandonJSContext(mContext);
        JS_DestroyContext(mContext);
    }
    mXPC = nsnull;
    mCompMgr = nsnull;
    mRegistry = nsnull;
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

#ifdef DEBUG_shaver
    char *cidString = aCID.ToString();
    fprintf(stderr, "mJCL::GetFactory(%s,%s,%s)\n", cidString, aLocation, aType);
    delete [] cidString;
#endif

    
    nsIModule * module = ModuleForLocation(aLocation, 0);
    if (!module) {
#ifdef DEBUG_shaver
        fprintf(stderr, "ERROR: couldn't get module for %s\n", aLocation);
#endif
        return NS_ERROR_FACTORY_NOT_LOADED;
    }
    
    nsresult rv = module->GetClassObject(mCompMgr, aCID,
                                         NS_GET_IID(nsIFactory),
                                         (void **)_retval);
#ifdef DEBUG_shaver
    fprintf(stderr, "GetClassObject %s\n", NS_FAILED(rv) ? "FAILED" : "ok");
#endif
    return rv;
}

static void
Reporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    fprintf(stderr, "mJCL: ERROR %s\n", message ? message : "<null>");
}

PR_STATIC_CALLBACK(PLHashNumber)
HashUUID(const void *key)
{
    nsID *id = (nsID *)key;
    return id->m0;
}

PR_STATIC_CALLBACK(PRIntn)
CompareUUID(const void *v1, const void *v2)
{
    nsID *a = (nsID *)v1, *b = (nsID *)v2;
    return a->Equals(*b);
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

    NS_WITH_SERVICE(nsIJSRuntimeService, rtsvc, "nsJSRuntimeService", &rv);
    // get the JSRuntime from the runtime svc, if possible
    if (NS_FAILED(rv))
        return rv;
    
    if (NS_FAILED(rv = rtsvc->GetRuntime(&mRuntime)))
#ifdef DEBUG_shaver
        fprintf(stderr, "mJCL: runtime initialized!\n");
#endif
    
    mContext = JS_NewContext(mRuntime, 8192 /* pref? */);
    if (!mContext)
        return NS_ERROR_OUT_OF_MEMORY;

    mSuperGlobal = JS_NewObject(mContext, &gGlobalClass, NULL, NULL);
    if (!mSuperGlobal)
        return NS_ERROR_OUT_OF_MEMORY;
    JS_AddNamedRoot(mContext, &mSuperGlobal, "mJCL::mSuperGlobal");

    if (!JS_InitStandardClasses(mContext, mSuperGlobal) ||
        !JS_DefineFunctions(mContext, mSuperGlobal, gGlobalFun))
        return NS_ERROR_FAILURE;

    /* get the XPC service. */
    NS_WITH_SERVICE(nsIXPConnect, xpcsvc, "nsIXPConnect", &rv);
    if (NS_FAILED(rv))
        return rv;
    mXPC = xpcsvc;

    rv = mXPC->InitJSContext(mContext, mSuperGlobal, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIXPConnectWrappedNative> wrappedCM;
    rv = mXPC->WrapNative(mContext, mCompMgr, NS_GET_IID(nsIComponentManager),
                          getter_AddRefs(wrappedCM));
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "failed to wrap comp mgr as nXPCWN\n");
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
    JS_AddNamedRoot(mContext, &mCompMgrWrapper, "mJCL::mCompMgrWrapper");
    JS_SetErrorReporter(mContext, Reporter);

    mModules = PL_NewHashTable(16, HashUUID, CompareUUID, PL_CompareValues,
                               0, 0);
    if (!mModules)
        return NS_ERROR_OUT_OF_MEMORY;
    mGlobals = PL_NewHashTable(16, PL_HashString, PL_CompareStrings,
                               PL_CompareValues, 0, 0);
    if (!mGlobals)
        return NS_ERROR_OUT_OF_MEMORY;

#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: context initialized!\n");
#endif
    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::AutoRegisterComponents(PRInt32 when,
                                             nsIFileSpec *aDirectory)
{
    char *nativePath = NULL;
    nsresult rv = aDirectory->GetNativePath(&nativePath);
    if (NS_FAILED(rv))
        return rv;

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
    if (!mRegistry)
        return NS_OK;           // silent failure

    nsresult rv;
    nsIRegistry::Key key;
    if (NS_FAILED(rv = mRegistry->GetSubtreeRaw(mXPCOMKey, registryLocation,
                                                &key)))
        return rv;

    PRUint32 modDate;
    if (NS_FAILED(rv = component->GetModDate(&modDate)) ||
        NS_FAILED(rv = mRegistry->SetInt(key, lastModValueName, modDate)))
        return rv;

    PRUint32 fileSize;
    if (NS_FAILED(rv = component->GetFileSize(&fileSize)) ||
        NS_FAILED(rv = mRegistry->SetInt(key, fileSizeValueName, fileSize)))
        return rv;

    return NS_OK;
}

PRBool
mozJSComponentLoader::HasChanged(const char *registryLocation,
                                 nsIFileSpec *component)
{

    /* if we don't have a registry handle, force registration of component */
    if (!mRegistry)
        return PR_TRUE;

    nsIRegistry::Key key;
    if (NS_FAILED(mRegistry->GetSubtreeRaw(mXPCOMKey, registryLocation, &key)))
        return PR_TRUE;

    /* check modification date */
    int32 regTime;
    if (NS_FAILED(mRegistry->GetInt(key, lastModValueName, &regTime)))
        return PR_TRUE;
    PRBool changed;
    if (NS_FAILED(component->ModDateChanged(regTime, &changed)) || changed)
        return PR_TRUE;

    /* check file size */
    int32 regSize;
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

    const char jsExtension[] = ".cjs";
    int jsExtensionLen = 4;
    char *nativePath = nsnull, *leafName = nsnull, *registryLocation = nsnull;
    nsIModule *module;

    *registered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetLeafName(&leafName)))
        return rv;
    int len = PL_strlen(leafName);
    
    /* if it's not *.cjs, return now */
    if (len < jsExtensionLen || // too short
        PL_strcasecmp(leafName + len - jsExtensionLen, jsExtension))
        goto out;

#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: registering JS component %s\n", leafName);
#endif

    rv = mCompMgr->RegistryLocationForSpec(component, &registryLocation);
    if (NS_FAILED(rv))
        goto out;
    
    if (!HasChanged(registryLocation, component)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "mJCL: %s hasn't changed\n", registryLocation);
#endif
        goto out;
    }

    module = ModuleForLocation(registryLocation, component);

    rv = module->RegisterSelf(mCompMgr, component, registryLocation);
    if (NS_FAILED(rv)) {
        fprintf(stderr, "module->RegisterSelf failed(%x)\n", rv);
        goto out;
    }

    SetRegistryInfo(registryLocation, component);

#ifdef DEBUG_shaver
    fprintf(stderr, "added module %p for %s\n", module, registryLocation);
#endif    
    registryLocation = 0;       // prevent free below
    *registered = PR_TRUE;

 out:
    if (registryLocation)
        nsAllocator::Free(registryLocation);
    if (nativePath)
        nsAllocator::Free(nativePath);
    if (leafName)
        nsAllocator::Free(leafName);
    return NS_OK;

}

nsIModule *
mozJSComponentLoader::ModuleForLocation(const char *registryLocation,
                                        nsIFileSpec *component)
{
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

    jsval argv[2], retval;
    argv[0] = OBJECT_TO_JSVAL(mCompMgrWrapper);
    argv[1] = STRING_TO_JSVAL(JS_NewStringCopyZ(mContext, registryLocation));
    if (!JS_CallFunctionName(mContext, obj, "NSGetModule", 2, argv,
                             &retval)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "mJCL: NSGetModule failed for %s\n",
                registryLocation);
#endif
        return nsnull;
    }

#ifdef DEBUG_shaver
    JSString *s = JS_ValueToString(mContext, retval);
    fprintf(stderr, "mJCL: %s::NSGetModule returned %s\n",
            registryLocation, JS_GetStringBytes(s));
#endif

    JSObject *jsModuleObj;
    if (!JS_ValueToObject(mContext, retval, &jsModuleObj)) {
        /* XXX report error properly */
        fprintf(stderr, "mJCL: couldn't convert %s's nsIModule to obj\n",
                registryLocation);
        return nsnull;
    }

    nsIModule *module;
    if (NS_FAILED(mXPC->WrapJS(mContext, jsModuleObj, NS_GET_IID(nsIModule),
                               (nsISupports **)&module))) {
        /* XXX report error properly */
        fprintf(stderr, "mJCL: couldn't get nsIModule from jsval\n");
        return nsnull;
    }

    /* we hand our reference to the hash table, it'll be released much later */
    he = PL_HashTableRawAdd(mModules, hep, hash, registryLocation, module);
    return module;
}

JSObject *
mozJSComponentLoader::GlobalForLocation(const char *aLocation,
                                        nsIFileSpec *component)
{

    PRBool needRelease = PR_FALSE;
    PLHashNumber hash = PL_HashString(aLocation);
    PLHashEntry **hep = PL_HashTableRawLookup(mGlobals, hash,
                                              (void *)aLocation);
    PLHashEntry *he = *hep;
    if (he)
        return (JSObject *)he->value;
    
    JSObject *obj = JS_NewObject(mContext, &gGlobalClass, mSuperGlobal,
                                 mSuperGlobal);
    if (!obj)
        return nsnull;

    if (!component) {
        if (NS_FAILED(mCompMgr->SpecForRegistryLocation(aLocation,
                                                        &component)))
            return nsnull;
        needRelease = PR_TRUE;
    }

    jsval retval;
    char *nativePath;
    /* XXX MAC: we use strings as file paths, *sigh* */
    component->GetNativePath(&nativePath);
    JSScript *script = JS_CompileFile(mContext, obj, nativePath);
    if (!script) {
#ifdef DEBUG_shaver
        fprintf(stderr, "mJCL: script compilation of %s FAILED\n",
                nativePath);
#endif
        goto out;
    }

#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: compiled JS component %s\n", nativePath);
#endif

    if (!JS_ExecuteScript(mContext, obj, script, &retval)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "mJCL: failed to execute %s\n", nativePath);
#endif
        goto out;
    }

    he = PL_HashTableRawAdd(mGlobals, hep, hash, aLocation, obj);
    JS_AddNamedRoot(mContext, &he->value, aLocation);

 out:
    if (needRelease)
        NS_RELEASE(component);
    if (nativePath)
        nsAllocator::Free(nativePath);
    return obj;
}

NS_IMETHODIMP
mozJSComponentLoader::OnRegister(const nsIID &aCID, const char *aType,
                                 const char *aClassName, const char *aProgID,
                                 const char *aLocation,
                                 PRBool aReplace, PRBool aPersist)

{
#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: registered %s/%s in %s\n", aClassName, aProgID,
            aLocation);
#endif
    return NS_OK;
}    

NS_IMETHODIMP
mozJSComponentLoader::UnloadAll(PRInt32 aWhen)
{
#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: UnloadAll(%d)\n", aWhen);
#endif
    return NS_OK;
}

static nsresult
CreateJSComponentLoader(nsISupports *aOuter, const nsIID &iid, void **result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    *result = 0;
    nsISupports *inst = 0;

    if (iid.Equals(NS_GET_IID(nsIComponentLoader))) {
        inst = (nsISupports *)new mozJSComponentLoader();
    } else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(iid, result);
    if (NS_FAILED(rv)) {
        delete inst;
    }
    return rv;
}

static NS_DEFINE_CID(kmozJSComponentLoaderCID, MOZJSCOMPONENTLOADER_CID);

extern "C" nsresult
NSGetFactory(nsISupports* servMgr, const nsCID &aClass, const char *aClassName,
             const char *aProgID, nsIFactory **aFactory)
{
    if (!aFactory)
        return NS_ERROR_NULL_POINTER;

    if (!aClass.Equals(kmozJSComponentLoaderCID))
        return NS_ERROR_NO_INTERFACE;

    nsIGenericFactory *factory;

    nsresult rv = NS_NewGenericFactory(&factory, CreateJSComponentLoader);
    
    if (!factory)
        return NS_ERROR_OUT_OF_MEMORY;
    if (NS_SUCCEEDED(rv))
        *aFactory = factory;

    return rv;
}

extern "C" nsresult
NSRegisterSelf(nsISupports *aServMgr, const char *aLocation)
{
    nsresult rv;
#ifdef DEBUG_shaver
    fprintf(stderr, "mJCL: registering component loader\n");
#endif

    static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = compMgr->RegisterComponent(kmozJSComponentLoaderCID,
                                    "JavaScript Component Loader",
                                    mozJSComponentLoaderProgID, aLocation, PR_TRUE,
                                    PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    return compMgr->RegisterComponentLoader(jsComponentTypeName,
                                            mozJSComponentLoaderProgID,
                                            PR_TRUE);
}
