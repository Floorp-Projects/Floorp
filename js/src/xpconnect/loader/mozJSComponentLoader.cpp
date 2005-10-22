/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributors:
 *   Mike Shaver <shaver@zeroknowledge.com>
 *   John Bandhauer <jband@netscape.com>
 *   IBM Corp.
 *   Robert Ginda <rginda@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIComponentLoader.h"
#include "nsIComponentManager.h"
#include "nsIComponentManagerObsolete.h"
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
#include "nsXPIDLString.h"
#include "nsIObserverService.h"
#include "nsIXPCScriptable.h"
#include "nsString.h"
#ifndef XPCONNECT_STANDALONE
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#endif
#include "nsIComponentLoaderManager.h"
#include "jsxdrapi.h"
#include "nsIFastLoadFileControl.h"
// For reporting errors with the console service
#include "nsIScriptError.h"
#include "nsIConsoleService.h"

static const char kJSRuntimeServiceContractID[] = "@mozilla.org/js/xpc/RuntimeService;1";
static const char kXPConnectServiceContractID[] = "@mozilla.org/js/xpc/XPConnect;1";
static const char kObserverServiceContractID[] = "@mozilla.org/observer-service;1";

/* Some platforms don't have an implementation of PR_MemMap(). */
#if !defined(XP_BEOS) && !defined(XP_OS2)
#define HAVE_PR_MEMMAP
#endif

/**
 * Buffer sizes for serialization and deserialization of scripts.
 * These should be tuned at some point.
 */
#define XPC_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XPC_DESERIALIZATION_BUFFER_SIZE (8 * 1024)

// Inactivity delay before closing our fastload file stream.
static const int kFastLoadWriteDelay = 5000;   // 5 seconds

#ifdef PR_LOGGING
// NSPR_LOG_MODULES=JSComponentLoader:5
static PRLogModuleInfo *gJSCLLog;
#endif

#define LOG(args) PR_LOG(gJSCLLog, PR_LOG_DEBUG, args)

JS_STATIC_DLL_CALLBACK(void)
Reporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    nsresult rv;

    /* Use the console service to register the error. */
    nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.  The UI can then
     * poll the service to update the JavaScript console.
     */
    nsCOMPtr<nsIScriptError> errorObject = 
        do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
    
    if (consoleService && errorObject) {
        /*
         * Got an error object; prepare appropriate-width versions of
         * various arguments to it.
         */
        nsAutoString fileUni;
        fileUni.AssignWithConversion(rep->filename);

        PRUint32 column = rep->uctokenptr - rep->uclinebuf;

        rv = errorObject->Init(NS_REINTERPRET_CAST(const PRUnichar*,
                                                   rep->ucmessage),
                               fileUni.get(),
                               NS_REINTERPRET_CAST(const PRUnichar*,
                                                   rep->uclinebuf),
                               rep->lineno, column, rep->flags,
                               "component javascript");
        if (NS_SUCCEEDED(rv)) {
            rv = consoleService->LogMessage(errorObject);
            if (NS_SUCCEEDED(rv)) {
                // We're done!  Skip return to fall thru to stderr
                // printout, for the benefit of those invoking the
                // browser with -console
                // return;
            }
        }
    }

    /*
     * If any of the above fails for some reason, fall back to
     * printing to stderr.
     */
#ifdef DEBUG
    fprintf(stderr, "JS Component Loader: %s %s:%d\n"
            "                     %s\n",
            JSREPORT_IS_WARNING(rep->flags) ? "WARNING" : "ERROR",
            rep->filename, rep->lineno,
            message ? message : "<no message>");
#endif
}

JS_STATIC_DLL_CALLBACK(JSBool)
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

    fputs(bytes, stderr);
    nsMemory::Free(bytes);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
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

class JSCLContextHelper
{
public:
    JSCLContextHelper(JSContext* cx);
    ~JSCLContextHelper();

    operator JSContext*() const {return mContext;}

    JSCLContextHelper(); // not implemnted
private:
    JSContext* mContext;
    intN       mContextThread; 
};


class JSCLAutoErrorReporterSetter
{
public:
    JSCLAutoErrorReporterSetter(JSContext* cx, JSErrorReporter reporter)
        {mContext = cx; mOldReporter = JS_SetErrorReporter(cx, reporter);}
    ~JSCLAutoErrorReporterSetter()
        {JS_SetErrorReporter(mContext, mOldReporter);} 
    JSCLAutoErrorReporterSetter(); // not implemented
private:
    JSContext* mContext;
    JSErrorReporter mOldReporter;
};

NS_IMPL_ISUPPORTS1(nsXPCFastLoadIO, nsIFastLoadFileIO)

NS_IMETHODIMP
nsXPCFastLoadIO::GetInputStream(nsIInputStream **_retval)
{
    if (! mInputStream) {
        nsCOMPtr<nsIInputStream> fileInput;
        nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInput),
                                                 mFile);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = NS_NewBufferedInputStream(getter_AddRefs(mInputStream),
                                       fileInput,
                                       XPC_DESERIALIZATION_BUFFER_SIZE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*_retval = mInputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCFastLoadIO::GetOutputStream(nsIOutputStream **_retval)
{
    if (! mOutputStream) {
        PRInt32 ioFlags = PR_WRONLY;
        if (! mInputStream) {
            ioFlags |= PR_CREATE_FILE | PR_TRUNCATE;
        }

        nsCOMPtr<nsIOutputStream> fileOutput;
        nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutput),
                                                  mFile, ioFlags, 0644);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = NS_NewBufferedOutputStream(getter_AddRefs(mOutputStream),
                                        fileOutput,
                                        XPC_SERIALIZATION_BUFFER_SIZE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*_retval = mOutputStream);
    return NS_OK;
}

static nsresult
ReadScriptFromStream(JSContext *cx, nsIObjectInputStream *stream,
                     JSScript **script)
{
    *script = nsnull;

    PRUint32 size;
    nsresult rv = stream->Read32(&size);
    NS_ENSURE_SUCCESS(rv, rv);

    char *data;
    rv = stream->ReadBytes(size, &data);
    NS_ENSURE_SUCCESS(rv, rv);

    JSXDRState *xdr = JS_XDRNewMem(cx, JSXDR_DECODE);
    NS_ENSURE_TRUE(xdr, NS_ERROR_OUT_OF_MEMORY);

    xdr->userdata = stream;
    JS_XDRMemSetData(xdr, data, size);

    if (JS_XDRScript(xdr, script)) {
        // Update data in case ::JS_XDRScript called back into C++ code to
        // read an XPCOM object.
        //
        // In that case, the serialization process must have flushed a run
        // of counted bytes containing JS data at the point where the XPCOM
        // object starts, after which an encoding C++ callback from the JS
        // XDR code must have written the XPCOM object directly into the
        // nsIObjectOutputStream.
        //
        // The deserialization process will XDR-decode counted bytes up to
        // but not including the XPCOM object, then call back into C++ to
        // read the object, then read more counted bytes and hand them off
        // to the JSXDRState, so more JS data can be decoded.
        //
        // This interleaving of JS XDR data and XPCOM object data may occur
        // several times beneath the call to ::JS_XDRScript, above.  At the
        // end of the day, we need to free (via nsMemory) the data owned by
        // the JSXDRState.  So we steal it back, nulling xdr's buffer so it
        // doesn't get passed to ::JS_free by ::JS_XDRDestroy.

        uint32 length;
        data = NS_STATIC_CAST(char*, JS_XDRMemGetData(xdr, &length));
        if (data) {
            JS_XDRMemSetData(xdr, nsnull, 0);
        }
        JS_XDRDestroy(xdr);
    } else {
        rv = NS_ERROR_FAILURE;
    }

    // If data is null now, it must have been freed while deserializing an
    // XPCOM object (e.g., a principal) beneath ::JS_XDRScript.
    if (data) {
        nsMemory::Free(data);
    }

    return rv;
}

static nsresult
WriteScriptToStream(JSContext *cx, JSScript *script,
                    nsIObjectOutputStream *stream)
{
    JSXDRState *xdr = JS_XDRNewMem(cx, JSXDR_ENCODE);
    NS_ENSURE_TRUE(xdr, NS_ERROR_OUT_OF_MEMORY);

    xdr->userdata = stream;
    nsresult rv = NS_OK;

    if (JS_XDRScript(xdr, &script)) {
        // Get the encoded JSXDRState data and write it.  The JSXDRState owns
        // this buffer memory and will free it beneath ::JS_XDRDestroy.
        //
        // If an XPCOM object needs to be written in the midst of the JS XDR
        // encoding process, the C++ code called back from the JS engine (e.g.,
        // nsEncodeJSPrincipals in caps/src/nsJSPrincipals.cpp) will flush data
        // from the JSXDRState to aStream, then write the object, then return
        // to JS XDR code with xdr reset so new JS data is encoded at the front
        // of the xdr's data buffer.
        //
        // However many XPCOM objects are interleaved with JS XDR data in the
        // stream, when control returns here from ::JS_XDRScript, we'll have
        // one last buffer of data to write to aStream.

        uint32 size;
        const char* data = NS_REINTERPRET_CAST(const char*,
                                               JS_XDRMemGetData(xdr, &size));
        NS_ASSERTION(data, "no decoded JSXDRState data!");

        rv = stream->Write32(size);
        if (NS_SUCCEEDED(rv)) {
            rv = stream->WriteBytes(data, size);
        }
    } else {
        rv = NS_ERROR_FAILURE; // likely to be a principals serialization error
    }

    JS_XDRDestroy(xdr);
    return rv;
}

mozJSComponentLoader::mozJSComponentLoader()
    : mRuntime(nsnull),
      mContext(nsnull),
      mModules(nsnull),
      mGlobals(nsnull),
      mInitialized(PR_FALSE)
{
#ifdef PR_LOGGING
    if (!gJSCLLog) {
        gJSCLLog = PR_NewLogModule("JSComponentLoader");
    }
#endif
}

static PRIntn PR_CALLBACK
UnrootGlobals(PLHashEntry *he, PRIntn i, void *arg)
{
    JSContext *cx = (JSContext *)arg;
    JSObject *global = (JSObject *)he->value;
    JS_ClearScope(cx, global);
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
    nsresult rv = module->CanUnload(mgr, &canUnload);
    NS_ASSERTION(NS_SUCCEEDED(rv), "module CanUnload failed");
    if (NS_SUCCEEDED(rv) && canUnload) {
        NS_RELEASE(module);
        /* XXX need to unroot the global for the module as well */
        nsCRT::free((char *)he->key);
        return HT_ENUMERATE_REMOVE;
    }
    return HT_ENUMERATE_NEXT;
}

mozJSComponentLoader::~mozJSComponentLoader()
{
    NS_ASSERTION(!mFastLoadTimer,
                 "Fastload file should have been closed via xpcom-shutdown");
}

NS_IMPL_ISUPPORTS2(mozJSComponentLoader, nsIComponentLoader, nsIObserver)

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
    
    nsresult rv;
    nsIModule *module = ModuleForLocation(aLocation, 0, &rv);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "ERROR: couldn't get module for %s\n", aLocation);
#endif
        return rv;
    }
    
    rv = module->GetClassObject(mCompMgr, aCID, NS_GET_IID(nsIFactory),
                                (void **)_retval);
#ifdef DEBUG_shaver_off
    fprintf(stderr, "GetClassObject %s\n", NS_FAILED(rv) ? "FAILED" : "ok");
#endif
    return rv;
}

NS_IMETHODIMP
mozJSComponentLoader::Init(nsIComponentManager *aCompMgr, nsISupports *aReg)
{
    mCompMgr = aCompMgr;

    nsresult rv;
    mLoaderManager = do_QueryInterface(mCompMgr, &rv);
    if (NS_FAILED(rv))
        return rv;

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

    // Create our compilation context.
    mContext = JS_NewContext(mRuntime, 256);
    if (!mContext)
        return NS_ERROR_OUT_OF_MEMORY;

    uint32 options = JS_GetOptions(mContext);
    JS_SetOptions(mContext, options | JSOPTION_XML);

#ifndef XPCONNECT_STANDALONE
    nsCOMPtr<nsIScriptSecurityManager> secman = 
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!secman)
        return NS_ERROR_FAILURE;

    rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
    if (NS_FAILED(rv) || !mSystemPrincipal)
        return NS_ERROR_FAILURE;
#endif

    mModules = PL_NewHashTable(16, PL_HashString, PL_CompareStrings,
                               PL_CompareValues, 0, 0);
    if (!mModules)
        return NS_ERROR_OUT_OF_MEMORY;
    
    mGlobals = PL_NewHashTable(16, PL_HashString, PL_CompareStrings,
                               PL_CompareValues, 0, 0);
    if (!mGlobals)
        return NS_ERROR_OUT_OF_MEMORY;

    // Set up our fastload file
    nsCOMPtr<nsIFastLoadService> flSvc = do_GetFastLoadService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = flSvc->NewFastLoadFile("XPC", getter_AddRefs(mFastLoadFile));
    if (NS_FAILED(rv)) {
        LOG(("Could not get fastload file location\n"));
    }

    // Listen for xpcom-shutdown so that we can close out our fastload file
    // at that point (after that we can no longer create an input stream).
    nsCOMPtr<nsIObserverService> obsSvc =
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: ReallyInit success!\n");
#endif
    mInitialized = PR_TRUE;

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
    if (!mLoaderManager)
        return NS_ERROR_FAILURE;
    
    PRInt64 modDate;
    rv = component->GetLastModifiedTime(&modDate);
    if (NS_FAILED(rv))
        return rv;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "SetRegistryInfo(%s) => (%d,%d)\n", registryLocation,
            modDate, fileSize);
#endif
    return mLoaderManager->SaveFileInfo(component, registryLocation, modDate);                                                          
}

nsresult
mozJSComponentLoader::RemoveRegistryInfo(nsIFile *component, const char *registryLocation)
{
    if (!mLoaderManager)
        return NS_ERROR_FAILURE;
    
    return mLoaderManager->RemoveFileInfo(component, registryLocation);                                                          
}


PRBool
mozJSComponentLoader::HasChanged(const char *registryLocation,
                                 nsIFile *component)
{
    if (!mLoaderManager)
        return NS_ERROR_FAILURE;
    
    PRInt64 lastTime;
    component->GetLastModifiedTime(&lastTime);

    PRBool hasChanged = PR_TRUE;
    mLoaderManager->HasFileChanged(component, registryLocation, lastTime, &hasChanged);                                                          
    return hasChanged;
}

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
    nsCAutoString leafName;

    *registered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetNativeLeafName(leafName)))
        return rv;
    int len = leafName.Length();
    
    /* if it's not *.js, return now */
    if (len < jsExtensionLen || // too short
        PL_strcasecmp(leafName.get() + len - jsExtensionLen, jsExtension))
        return NS_OK;

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: registering JS component %s\n",
            leafName.get());
#endif
      
    rv = AttemptRegistration(component, PR_FALSE);
#ifdef DEBUG_shaver_off
    if (NS_SUCCEEDED(rv))
        fprintf(stderr, "registered module %s\n", leafName.get());
    else if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) 
        fprintf(stderr, "deferred module %s\n", leafName.get());
    else
        fprintf(stderr, "failed to register %s\n", leafName.get());
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
    nsCAutoString leafName;

    *unregistered = PR_FALSE;

    /* we only do files */
    PRBool isFile = PR_FALSE;
    if (NS_FAILED(rv = component->IsFile(&isFile)) || !isFile)
        return rv;

    if (NS_FAILED(rv = component->GetNativeLeafName(leafName)))
        return rv;
    int len = leafName.Length();
    
    /* if it's not *.js, return now */
    if (len < jsExtensionLen || // too short
        PL_strcasecmp(leafName.get() + len - jsExtensionLen, jsExtension))
        return NS_OK;

    rv = UnregisterComponent(component);
#ifdef DEBUG_dp
    if (NS_SUCCEEDED(rv))
        fprintf(stderr, "unregistered module %s\n", leafName.get());
    else
        fprintf(stderr, "failed to unregister %s\n", leafName.get());
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
    
    // what I want to do here is QI for a Component Registration Manager.  Since this 
    // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
    nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &rv);
    if (obsoleteManager)
        rv = obsoleteManager->RegistryLocationForSpec(component, 
                                                      getter_Copies(registryLocation));
    if (NS_FAILED(rv))
        return rv;
    
    /* no need to check registry data on deferred reg */
    if (!deferred && !HasChanged(registryLocation, component))
        return NS_OK;
    
    nsIModule *module = ModuleForLocation(registryLocation, component, &rv);
    if (NS_FAILED(rv)) {
        SetRegistryInfo(registryLocation, component);
        return rv;
    }

    // Notify observers, if any, of autoregistration work
    nsCOMPtr<nsIObserverService> observerService = 
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIServiceManager> mgr;
    rv = NS_GetServiceManager(getter_AddRefs(mgr));
    NS_ENSURE_SUCCESS(rv, rv);

    // this string can't come from a string bundle, because we
    // don't have string bundles yet.
    NS_ConvertASCIItoUCS2 fileName("(no name)");

    // get the file name
    if (component) {
        component->GetLeafName(fileName);
    }

    // this string can't come from a string bundle, because we
    // don't have string bundles yet.
    const nsPromiseFlatString &msg =
        PromiseFlatString(NS_LITERAL_STRING("Registering JS component ") +
                          fileName);

    observerService->NotifyObservers(mgr,
                                     NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                     msg.get());
    
    rv = module->RegisterSelf(mCompMgr, component, registryLocation,
                              MOZJSCOMPONENTLOADER_TYPE_NAME);
    if (rv == NS_ERROR_FACTORY_REGISTER_AGAIN) {
        if (!deferred) {
            mDeferredComponents.AppendElement(component);
        }
        /*
         * we don't enter in the registry because we may want to
         * try again on a later autoreg, in case a dependency has
         * become available. 
         */
    } else {
        SetRegistryInfo(registryLocation, component);
    }

    return rv;
}

nsresult
mozJSComponentLoader::UnregisterComponent(nsIFile *component)
{
    nsXPIDLCString registryLocation;
    nsresult rv;
    
    // what I want to do here is QI for a Component Registration Manager.  Since this 
    // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
    nsCOMPtr<nsIComponentManagerObsolete> obsoleteManager = do_QueryInterface(mCompMgr, &rv);
    if (obsoleteManager)
        rv = obsoleteManager->RegistryLocationForSpec(component, 
                                                      getter_Copies(registryLocation));
    if (NS_FAILED(rv))
        return rv;
    
    nsIModule *module = ModuleForLocation(registryLocation, component, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // Notify observers, if any, of autoregistration work
    nsCOMPtr<nsIObserverService> observerService = 
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIServiceManager> mgr;
    rv = NS_GetServiceManager(getter_AddRefs(mgr));
    NS_ENSURE_SUCCESS(rv, rv);

    const nsAFlatString &msg = NS_LITERAL_STRING("Unregistering JS component");
    observerService->NotifyObservers(mgr,
                                     NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID,
                                     msg.get());
    
    rv = module->UnregisterSelf(mCompMgr, component, registryLocation);
    if (NS_SUCCEEDED(rv)) {
        // Remove any autoreg specific info. Ignore error.
        RemoveRegistryInfo(component, registryLocation);
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
#ifdef DEBUG_shaver_off
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

#ifdef DEBUG_shaver_off
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


nsIModule*
mozJSComponentLoader::ModuleForLocation(const char *registryLocation,
                                        nsIFile *component, nsresult *status)
{
    nsresult rv;
    if (!mInitialized) {
        rv = ReallyInit();
        if (NS_FAILED(rv)) {
            *status = rv;
            return nsnull;
        }
    }

    PLHashNumber hash = PL_HashString(registryLocation);
    PLHashEntry **hep = PL_HashTableRawLookup(mModules, hash,
                                              registryLocation);
    PLHashEntry *he = *hep;
    if (he) {
        *status = NS_OK;
        return NS_STATIC_CAST(nsIModule*, he->value);
    }

    JSObject *global;
    rv = GlobalForLocation(registryLocation, component, &global);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "GlobalForLocation failed!\n");
#endif
        *status = rv;
        return nsnull;
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID,
                                               &rv);
    if (NS_FAILED(rv)) {
        *status = rv;
        return nsnull;
    }

    JSCLContextHelper cx(mContext);

    JSObject* cm_jsobj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> cm_holder;
    rv = xpc->WrapNative(cx, global, mCompMgr, 
                         NS_GET_IID(nsIComponentManager),
                         getter_AddRefs(cm_holder));

    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "WrapNative(%p,%p,nsIComponentManager) failed: %x\n",
                (void *)(JSContext*)cx, (void *)mCompMgr, rv);
#endif
        *status = rv;
        return nsnull;
    }

    rv = cm_holder->GetJSObject(&cm_jsobj);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "GetJSObject of ComponentManager failed\n");
#endif
        *status = rv;
        return nsnull;
    }

    JSCLAutoErrorReporterSetter aers(cx, Reporter);

    jsval argv[2], retval, NSGetModule_val;

    if (!JS_GetProperty(cx, global, "NSGetModule", &NSGetModule_val) ||
        JSVAL_IS_VOID(NSGetModule_val)) {
        *status = NS_ERROR_FAILURE;
        return nsnull;
    }

    if (JS_TypeOfValue(cx, NSGetModule_val) != JSTYPE_FUNCTION) {
        JS_ReportError(cx, "%s has NSGetModule property that is not a function",
                       registryLocation);
        *status = NS_ERROR_FAILURE;
        return nsnull;
    }
    
    argv[0] = OBJECT_TO_JSVAL(cm_jsobj);
    argv[1] = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, registryLocation));
    if (!JS_CallFunctionValue(cx, global, NSGetModule_val, 2, argv, &retval)) {
        *status = NS_ERROR_FAILURE;
        return nsnull;
    }

#ifdef DEBUG_shaver_off
    JSString *s = JS_ValueToString(cx, retval);
    fprintf(stderr, "mJCL: %s::NSGetModule returned %s\n",
            registryLocation, JS_GetStringBytes(s));
#endif

    JSObject *jsModuleObj;
    if (!JS_ValueToObject(cx, retval, &jsModuleObj)) {
        /* XXX report error properly */
#ifdef DEBUG
        fprintf(stderr, "mJCL: couldn't convert %s's nsIModule to obj\n",
                registryLocation);
#endif
        *status = NS_ERROR_FAILURE;
        return nsnull;
    }

    nsIModule *module;
    rv = xpc->WrapJS(cx, jsModuleObj, NS_GET_IID(nsIModule), (void**)&module);
    if (NS_FAILED(rv)) {
        /* XXX report error properly */
#ifdef DEBUG
        fprintf(stderr, "mJCL: couldn't get nsIModule from jsval\n");
#endif
        *status = rv;
        return nsnull;
    }

    /* we hand our reference to the hash table, it'll be released much later */
    he = PL_HashTableRawAdd(mModules, hep, hash,
                            nsCRT::strdup(registryLocation), module);

    return module;
}

// Some stack based classes for cleaning up on early return
#ifdef HAVE_PR_MEMMAP
class FileAutoCloser
{
 public:
    explicit FileAutoCloser(PRFileDesc *file) : mFile(file) {}
    ~FileAutoCloser() { PR_Close(mFile); }
 private:
    PRFileDesc *mFile;
};

class FileMapAutoCloser
{
 public:
    explicit FileMapAutoCloser(PRFileMap *map) : mMap(map) {}
    ~FileMapAutoCloser() { PR_CloseFileMap(mMap); }
 private:
    PRFileMap *mMap;
};
#endif

class JSPrincipalsHolder
{
 public:
    JSPrincipalsHolder(JSContext *cx, JSPrincipals *principals)
        : mCx(cx), mPrincipals(principals) {}
    ~JSPrincipalsHolder() { JSPRINCIPALS_DROP(mCx, mPrincipals); }
 private:
    JSContext *mCx;
    JSPrincipals *mPrincipals;
};

class JSScriptHolder
{
 public:
    JSScriptHolder(JSContext *cx, JSScript *script)
        : mCx(cx), mScript(script) {}
    ~JSScriptHolder() { ::JS_DestroyScript(mCx, mScript); }
 private:
    JSContext *mCx;
    JSScript *mScript;
};

class FastLoadStateHolder
{
 public:
    explicit FastLoadStateHolder(nsIFastLoadService *service);
    ~FastLoadStateHolder() { pop(); }

    void pop();

 private:
    nsCOMPtr<nsIFastLoadService> mService;
    nsCOMPtr<nsIFastLoadFileIO> mIO;
    nsCOMPtr<nsIObjectInputStream> mInputStream;
    nsCOMPtr<nsIObjectOutputStream> mOutputStream;
};

FastLoadStateHolder::FastLoadStateHolder(nsIFastLoadService *service)
{
    if (!service)
        return;

    mService = service;
    service->GetFileIO(getter_AddRefs(mIO));
    service->GetInputStream(getter_AddRefs(mInputStream));
    service->GetOutputStream(getter_AddRefs(mOutputStream));
}

void
FastLoadStateHolder::pop()
{
    if (!mService)
        return;

    mService->SetFileIO(mIO);
    mService->SetInputStream(mInputStream);
    mService->SetOutputStream(mOutputStream);

    mService = nsnull;
}

/* static */
void
mozJSComponentLoader::CloseFastLoad(nsITimer *timer, void *closure)
{
    NS_STATIC_CAST(mozJSComponentLoader*, closure)->CloseFastLoad();
}

void
mozJSComponentLoader::CloseFastLoad()
{
    // Close our fastload streams
    LOG(("Closing fastload file\n"));
    if (mFastLoadOutput) {
        nsresult rv = mFastLoadOutput->Close();
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIFastLoadService> flSvc = do_GetFastLoadService(&rv);
            if (NS_SUCCEEDED(rv)) {
                flSvc->CacheChecksum(mFastLoadFile, mFastLoadOutput);
            }
        }
        mFastLoadOutput = nsnull;
    }
    if (mFastLoadInput) {
        mFastLoadInput->Close();
        mFastLoadInput = nsnull;
    }

    mFastLoadIO = nsnull;
    mFastLoadTimer = nsnull;
}

nsresult
mozJSComponentLoader::StartFastLoad(nsIFastLoadService *flSvc)
{
    if (!mFastLoadFile || !flSvc) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    // Now set our IO object as current, and create our streams.
    if (!mFastLoadIO) {
        mFastLoadIO = new nsXPCFastLoadIO(mFastLoadFile);
        NS_ENSURE_TRUE(mFastLoadIO, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = flSvc->SetFileIO(mFastLoadIO);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mFastLoadInput && !mFastLoadOutput) {
        // First time accessing the fastload file
        PRBool exists;
        mFastLoadFile->Exists(&exists);
        if (exists) {
            LOG(("trying to use existing fastload file\n"));

            nsCOMPtr<nsIInputStream> input;
            rv = mFastLoadIO->GetInputStream(getter_AddRefs(input));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = flSvc->NewInputStream(input, getter_AddRefs(mFastLoadInput));
            if (NS_SUCCEEDED(rv)) {
                LOG(("opened fastload file for reading\n"));

                nsCOMPtr<nsIFastLoadReadControl>
                    readControl(do_QueryInterface(mFastLoadInput));
                if (readControl) {
                    // Verify checksum, using the FastLoadService's
                    // checksum cache to avoid computing more than once
                    // per session.
                    PRUint32 checksum;
                    rv = readControl->GetChecksum(&checksum);
                    if (NS_SUCCEEDED(rv)) {
                        PRUint32 verified;
                        rv = flSvc->ComputeChecksum(mFastLoadFile,
                                                    readControl, &verified);
                        if (NS_SUCCEEDED(rv) && verified != checksum) {
                            LOG(("Incorrect checksum detected"));
                            rv = NS_ERROR_FAILURE;
                        }
                    }
                }
            }
            if (NS_FAILED(rv)) {
                LOG(("Invalid fastload file detected, removing it\n"));
                if (mFastLoadInput) {
                    mFastLoadInput->Close();
                    mFastLoadInput = nsnull;
                } else {
                    input->Close();
                }
                mFastLoadIO->SetInputStream(nsnull);
                mFastLoadFile->Remove(PR_FALSE);
                exists = PR_FALSE;
            }
        }

        if (!exists) {
            LOG(("Creating new fastload file\n"));

            nsCOMPtr<nsIOutputStream> output;
            rv = mFastLoadIO->GetOutputStream(getter_AddRefs(output));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = flSvc->NewOutputStream(output,
                                        getter_AddRefs(mFastLoadOutput));
            if (NS_FAILED(rv)) {
                LOG(("Fatal error, could not create fastload file\n"));

                if (mFastLoadOutput) {
                    mFastLoadOutput->Close();
                    mFastLoadOutput = nsnull;
                } else {
                    output->Close();
                }
                mFastLoadIO->SetOutputStream(nsnull);
                mFastLoadFile->Remove(PR_FALSE);
                return rv;
            }
        }
    }

    flSvc->SetInputStream(mFastLoadInput);
    flSvc->SetOutputStream(mFastLoadOutput);

    // Start our update timer.  This allows us to keep the stream open
    // when many components are loaded in succession, but close it once
    // there has been a period of inactivity.

    if (!mFastLoadTimer) {
        mFastLoadTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mFastLoadTimer->InitWithFuncCallback(&mozJSComponentLoader::CloseFastLoad,
                                                  this,
                                                  kFastLoadWriteDelay,
                                                  nsITimer::TYPE_ONE_SHOT);
    } else {
        rv = mFastLoadTimer->SetDelay(kFastLoadWriteDelay);
    }

    return rv;
}

nsresult
mozJSComponentLoader::ReadScript(nsIFastLoadService *flSvc,
                                 const char *nativePath, nsIURI *uri,
                                 JSContext *cx, JSScript **script)
{
    NS_ASSERTION(flSvc, "fastload not initialized");

    nsresult rv = flSvc->StartMuxedDocument(uri, nativePath,
                                            nsIFastLoadService::NS_FASTLOAD_READ);
    if (NS_FAILED(rv)) {
        return rv; // don't warn since NOT_AVAILABLE is an ok error
    }

    LOG(("Found %s in fastload file\n", nativePath));

    nsCOMPtr<nsIURI> oldURI;
    rv = flSvc->SelectMuxedDocument(uri, getter_AddRefs(oldURI));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(mFastLoadInput,
                 "FASTLOAD_READ should only succeed with an input stream");

    rv = ReadScriptFromStream(cx, mFastLoadInput, script);
    if (NS_SUCCEEDED(rv)) {
        rv = flSvc->EndMuxedDocument(uri);
    }

    return rv;
}

nsresult
mozJSComponentLoader::WriteScript(nsIFastLoadService *flSvc, JSScript *script,
                                  nsIFile *component, const char *nativePath,
                                  nsIURI *uri, JSContext *cx)
{
    NS_ASSERTION(flSvc, "fastload not initialized");
    nsresult rv;

    if (!mFastLoadOutput) {
        // Trying to read a URI that was not in the fastload file will have
        // created an output stream for us.  But, if we haven't tried to
        // load anything that was missing, it will still be null.
        rv = flSvc->GetOutputStream(getter_AddRefs(mFastLoadOutput));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ASSERTION(mFastLoadOutput, "must have an output stream here");

    LOG(("Writing %s to fastload\n", nativePath));
    rv = flSvc->AddDependency(component);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = flSvc->StartMuxedDocument(uri, nativePath,
                                   nsIFastLoadService::NS_FASTLOAD_WRITE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> oldURI;
    rv = flSvc->SelectMuxedDocument(uri, getter_AddRefs(oldURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = WriteScriptToStream(cx, script, mFastLoadOutput);
    NS_ENSURE_SUCCESS(rv, rv);

    return flSvc->EndMuxedDocument(uri);
}

nsresult
mozJSComponentLoader::GlobalForLocation(const char *aLocation,
                                        nsIFile *aComponent,
                                        JSObject **aGlobal)
{
    nsresult rv;
    if (!mInitialized) {
        rv = ReallyInit();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    PLHashNumber hash = PL_HashString(aLocation);
    PLHashEntry **hep = PL_HashTableRawLookup(mGlobals, hash, aLocation);
    PLHashEntry *he = *hep;
    if (he) {
        *aGlobal = NS_STATIC_CAST(JSObject*, he->value);
        return NS_OK;
    }

    *aGlobal = nsnull;

    JSPrincipals* jsPrincipals = nsnull;
    JSCLContextHelper cx(mContext);

#ifndef XPCONNECT_STANDALONE
    rv = mSystemPrincipal->GetJSPrincipals(cx, &jsPrincipals);
    NS_ENSURE_SUCCESS(rv, rv);

    JSPrincipalsHolder princHolder(mContext, jsPrincipals);
#endif

    nsCOMPtr<nsIXPCScriptable> backstagePass;
    rv = mRuntimeService->GetBackstagePass(getter_AddRefs(backstagePass));
    NS_ENSURE_SUCCESS(rv, rv);

    JSCLAutoErrorReporterSetter aers(cx, Reporter);

    nsCOMPtr<nsIXPConnect> xpc =
        do_GetService(kXPConnectServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure InitClassesWithNewWrappedGlobal() installs the
    // backstage pass as the global in our compilation context.
    JS_SetGlobalObject(cx, nsnull);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->InitClassesWithNewWrappedGlobal(cx, backstagePass,
                                              NS_GET_IID(nsISupports),
                                              nsIXPConnect::
                                                  FLAG_SYSTEM_GLOBAL_OBJECT,
                                              getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *global;
    rv = holder->GetJSObject(&global);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineFunctions(cx, global, gGlobalFun)) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIFile> component = aComponent;
    if (!component) {
        // what I want to do here is QI for a Component Registration Manager.  Since this 
        // has not been invented yet, QI to the obsolete manager.  Kids, don't do this at home.
        nsCOMPtr<nsIComponentManagerObsolete> mgr = do_QueryInterface(mCompMgr,
                                                                      &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mgr->SpecForRegistryLocation(aLocation,
                                          getter_AddRefs(component));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(component, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIXPConnectJSObjectHolder> locationHolder;
    rv = xpc->WrapNative(cx, global, localFile,
                         NS_GET_IID(nsILocalFile),
                         getter_AddRefs(locationHolder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *locationObj;
    rv = locationHolder->GetJSObject(&locationObj);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineProperty(cx, global, "__LOCATION__",
                           OBJECT_TO_JSVAL(locationObj), nsnull, nsnull, 0)) {
        return NS_ERROR_FAILURE;
    }

    nsCAutoString nativePath;
    // Quick hack to unbust XPCONNECT_STANDALONE.
    // This leaves the jsdebugger with a non-URL pathname in the 
    // XPCONNECT_STANDALONE case - but at least it builds and runs otherwise.
    // See: http://bugzilla.mozilla.org/show_bug.cgi?id=121438
#ifdef XPCONNECT_STANDALONE
    localFile->GetNativePath(nativePath);
#else
    NS_GetURLSpecFromFile(localFile, nativePath);
#endif

    // Before compiling the script, first check to see if we have it in
    // the fastload file.  Note: as a rule, fastload errors are not fatal
    // to loading the script, since we can always slow-load.
    nsCOMPtr<nsIFastLoadService> flSvc = do_GetFastLoadService(&rv);

    // Save the old state and restore it upon return
    FastLoadStateHolder flState(flSvc);
    PRBool fastLoading = PR_FALSE;

    if (NS_SUCCEEDED(rv)) {
        rv = StartFastLoad(flSvc);
        if (NS_SUCCEEDED(rv)) {
            fastLoading = PR_TRUE;
        }
    }

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), nativePath);
    NS_ENSURE_SUCCESS(rv, rv);

    JSScript *script = nsnull;

    if (fastLoading) {
        rv = ReadScript(flSvc, nativePath.get(), uri, cx, &script);
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully loaded %s from fastload\n", nativePath.get()));
            fastLoading = PR_FALSE; // no need to write out the script
        } else if (rv == NS_ERROR_NOT_AVAILABLE) {
            // This is ok, it just means the script is not yet in the
            // fastload file.
            rv = NS_OK;
        } else {
            LOG(("Failed to deserialize %s\n", nativePath.get()));

            // Remove the fastload file, it may be corrupted.
            LOG(("Invalid fastload file detected, removing it\n"));
            nsCOMPtr<nsIObjectOutputStream> objectOutput;
            flSvc->GetOutputStream(getter_AddRefs(objectOutput));
            if (objectOutput) {
                flSvc->SetOutputStream(nsnull);
                objectOutput->Close();
            }
            nsCOMPtr<nsIObjectInputStream> objectInput;
            flSvc->GetInputStream(getter_AddRefs(objectInput));
            if (objectInput) {
                flSvc->SetInputStream(nsnull);
                objectInput->Close();
            }
            if (mFastLoadFile) {
                mFastLoadFile->Remove(PR_FALSE);
            }
            fastLoading = PR_FALSE;
        }
    }


    if (!script || NS_FAILED(rv)) {
        // The script wasn't in the fastload cache, so compile it now.
        LOG(("Slow loading %s\n", nativePath.get()));

#ifdef HAVE_PR_MEMMAP
        PRInt64 fileSize;
        localFile->GetFileSize(&fileSize);

        PRInt64 maxSize;
        LL_UI2L(maxSize, PR_UINT32_MAX);
        if (LL_CMP(fileSize, >, maxSize)) {
            NS_ERROR("file too large");
            return NS_ERROR_FAILURE;
        }

        PRFileDesc *fileHandle;
        rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fileHandle);
        NS_ENSURE_SUCCESS(rv, rv);

        // Make sure the file is closed, no matter how we return.
        FileAutoCloser fileCloser(fileHandle);

        PRFileMap *map = PR_CreateFileMap(fileHandle, fileSize,
                                          PR_PROT_READONLY);
        if (!map) {
            NS_ERROR("Failed to create file map");
            return NS_ERROR_FAILURE;
        }

        // Make sure the file map is closed, no matter how we return.
        FileMapAutoCloser mapCloser(map);

        PRUint32 fileSize32;
        LL_L2UI(fileSize32, fileSize);

        char *buf = NS_STATIC_CAST(char*, PR_MemMap(map, 0, fileSize32));
        if (!buf) {
            NS_ERROR("Failed to map file");
            return NS_ERROR_FAILURE;
        }

        script = JS_CompileScriptForPrincipals(cx, global,
                                               jsPrincipals,
                                               buf, fileSize32,
                                               nativePath.get(), 0);
        PR_MemUnmap(buf, fileSize32);

#else  /* HAVE_PR_MEMMAP */

        /**
         * No memmap implementation, so fall back to using
         * JS_CompileFileHandleForPrincipals().
         */

        FILE *fileHandle;
        rv = localFile->OpenANSIFileDesc("r", &fileHandle);
        NS_ENSURE_SUCCESS(rv, rv);

        script = JS_CompileFileHandleForPrincipals(cx, global,
                                                   nativePath.get(),
                                                   fileHandle, jsPrincipals);

        /* JS will close the filehandle after compilation is complete. */

#endif /* HAVE_PR_MEMMAP */
    }

    if (!script) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: script compilation of %s FAILED\n",
                nativePath.get());
#endif
        return NS_ERROR_FAILURE;
    }

    // Ensure that we clean up the script on return.
    JSScriptHolder scriptHolder(cx, script);

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: compiled JS component %s\n",
            nativePath.get());
#endif

    if (fastLoading) {
        // We successfully compiled the script, so cache it in fastload.
        rv = WriteScript(flSvc, script, component, nativePath.get(), uri, cx);

        // Don't treat failure to write as fatal, since we might be working
        // with a read-only fastload file.
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully wrote to fastload\n"));
        } else {
            LOG(("Failed to write to fastload\n"));
        }
    }

    // Restore the old state of the fastload service.
    flState.pop();

    jsval retval;
    if (!JS_ExecuteScript(cx, global, script, &retval)) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: failed to execute %s\n", nativePath.get());
#endif
        return NS_ERROR_FAILURE;
    }

    *aGlobal = global;

    /* Freed when we remove from the table. */
    char *location = nsCRT::strdup(aLocation);
    NS_ENSURE_TRUE(location, NS_ERROR_OUT_OF_MEMORY);

    he = PL_HashTableRawAdd(mGlobals, hep, hash, location, global);
    JS_AddNamedRoot(cx, &he->value, location);
    return NS_OK;
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
        mInitialized = PR_FALSE;

        // stabilize the component manager, etc.
        nsCOMPtr<nsIComponentManager> kungFuDeathGrip = mCompMgr;

        PL_HashTableEnumerateEntries(mModules, 
                                     UnloadAndReleaseModules,
                                     mCompMgr);
        PL_HashTableDestroy(mModules);
        mModules = nsnull;

        PL_HashTableEnumerateEntries(mGlobals, UnrootGlobals, mContext);
        PL_HashTableDestroy(mGlobals);
        mGlobals = nsnull;

        // Destroying our context will force a GC.
        JS_DestroyContext(mContext);
        mContext = nsnull;

        mRuntimeService = nsnull;
    }

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: UnloadAll(%d)\n", aWhen);
#endif

    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::Observe(nsISupports *subject, const char *topic,
                              const PRUnichar *data)
{
    NS_ASSERTION(!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID),
                 "Unexpected observer topic");

    if (mFastLoadTimer) {
        mFastLoadTimer->Cancel();
    }

    CloseFastLoad();
    return NS_OK;
}

//----------------------------------------------------------------------

JSCLContextHelper::JSCLContextHelper(JSContext *cx)
    : mContext(cx), mContextThread(0)
{
    mContextThread = JS_GetContextThread(mContext);
    if (mContextThread) {
        JS_BeginRequest(mContext);
    } 
}

JSCLContextHelper::~JSCLContextHelper()
{
    JS_ClearNewbornRoots(mContext);
    if (mContextThread)
        JS_EndRequest(mContext);
}        

//----------------------------------------------------------------------

/* XXX this should all be data-driven, via NS_IMPL_GETMODULE_WITH_CATEGORIES */
