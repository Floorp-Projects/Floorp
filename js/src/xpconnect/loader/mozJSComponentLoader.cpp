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

#include <stdarg.h>

#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "mozilla/Module.h"
#include "nsILocalFile.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "mozJSComponentLoader.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsIObserverService.h"
#include "nsIXPCScriptable.h"
#include "nsString.h"
#ifndef XPCONNECT_STANDALONE
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIJARURI.h"
#include "nsNetUtil.h"
#endif
#include "jsxdrapi.h"
#include "jscompartment.h"
#include "jsprf.h"
// For reporting errors with the console service
#include "nsIScriptError.h"
#include "nsIConsoleService.h"
#include "nsIStorageStream.h"
#include "nsIStringStream.h"
#include "prmem.h"
#if defined(XP_WIN)
#include "nsILocalFileWin.h"
#endif
#include "xpcprivate.h"
#include "nsIResProtocolHandler.h"

#ifdef MOZ_ENABLE_LIBXUL
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#endif
#include "mozilla/Omnijar.h"

#include "jsdbgapi.h"

#include "mozilla/FunctionTimer.h"

static const char kJSRuntimeServiceContractID[] = "@mozilla.org/js/xpc/RuntimeService;1";
static const char kXPConnectServiceContractID[] = "@mozilla.org/js/xpc/XPConnect;1";
static const char kObserverServiceContractID[] = "@mozilla.org/observer-service;1";

/* Some platforms don't have an implementation of PR_MemMap(). */
#ifndef XP_OS2
#define HAVE_PR_MEMMAP
#endif

/**
 * Buffer sizes for serialization and deserialization of scripts.
 * FIXME: bug #411579 (tune this macro!) Last updated: Jan 2008
 */
#define XPC_SERIALIZATION_BUFFER_SIZE   (64 * 1024)
#define XPC_DESERIALIZATION_BUFFER_SIZE (12 * 8192)

#ifdef PR_LOGGING
// NSPR_LOG_MODULES=JSComponentLoader:5
static PRLogModuleInfo *gJSCLLog;
#endif

#define LOG(args) PR_LOG(gJSCLLog, PR_LOG_DEBUG, args)

// Components.utils.import error messages
#define ERROR_SCOPE_OBJ "%s - Second argument must be an object."
#define ERROR_NOT_PRESENT "%s - EXPORTED_SYMBOLS is not present."
#define ERROR_NOT_AN_ARRAY "%s - EXPORTED_SYMBOLS is not an array."
#define ERROR_GETTING_ARRAY_LENGTH "%s - Error getting array length of EXPORTED_SYMBOLS."
#define ERROR_ARRAY_ELEMENT "%s - EXPORTED_SYMBOLS[%d] is not a string."
#define ERROR_GETTING_SYMBOL "%s - Could not get symbol '%s'."
#define ERROR_SETTING_SYMBOL "%s - Could not set symbol '%s' on target object."

void
mozJSLoaderErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep)
{
    nsresult rv;

    /* Use the console service to register the error. */
    nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);

    /*
     * Make an nsIScriptError, populate it with information from this
     * error, then log it with the console service.  The UI can then
     * poll the service to update the Error console.
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

        rv = errorObject->Init(reinterpret_cast<const PRUnichar*>
                                               (rep->ucmessage),
                               fileUni.get(),
                               reinterpret_cast<const PRUnichar*>
                                               (rep->uclinebuf),
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

static JSBool
Dump(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    if (!argc)
        return JS_TRUE;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return JS_FALSE;

    fputs(NS_ConvertUTF16toUTF8(reinterpret_cast<const PRUnichar*>(chars)).get(), stderr);
    return JS_TRUE;
}

static JSBool
Debug(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef DEBUG
    return Dump(cx, argc, vp);
#else
    return JS_TRUE;
#endif
}

static JSBool
Atob(JSContext *cx, uintN argc, jsval *vp)
{
    if (!argc)
        return JS_TRUE;

    return nsXPConnect::Base64Decode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

static JSBool
Btoa(JSContext *cx, uintN argc, jsval *vp)
{
    if (!argc)
        return JS_TRUE;

    return nsXPConnect::Base64Encode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

static JSFunctionSpec gGlobalFun[] = {
    {"dump",    Dump,   1,0},
    {"debug",   Debug,  1,0},
    {"atob",    Atob,   1,0},
    {"btoa",    Btoa,   1,0},
#ifdef MOZ_CALLGRIND
    {"startCallgrind",  js_StartCallgrind, 0,0},
    {"stopCallgrind",   js_StopCallgrind,  0,0},
    {"dumpCallgrind",   js_DumpCallgrind,  1,0},
#endif
#ifdef MOZ_VTUNE
    {"startVtune",      js_StartVtune,     1,0},
    {"stopVtune",       js_StopVtune,      0,0},
    {"pauseVtune",      js_PauseVtune,     0,0},
    {"resumeVtune",     js_ResumeVtune,    0,0},
#endif
#ifdef MOZ_TRACEVIS
    {"initEthogram",     js_InitEthogram,      0,0},
    {"shutdownEthogram", js_ShutdownEthogram,  0,0},
#endif
    {nsnull,nsnull,0,0}
};

class JSCLContextHelper
{
public:
    JSCLContextHelper(mozJSComponentLoader* loader);
    ~JSCLContextHelper() { Pop(); }

    JSContext* Pop();

    operator JSContext*() const {return mContext;}

private:
    JSContext* mContext;
    intN       mContextThread;
    nsIThreadJSContextStack* mContextStack;

    // prevent copying and assignment
    JSCLContextHelper(const JSCLContextHelper &); // not implemented
    const JSCLContextHelper& operator=(const JSCLContextHelper &); // not implemented
};


class JSCLAutoErrorReporterSetter
{
public:
    JSCLAutoErrorReporterSetter(JSContext* cx, JSErrorReporter reporter)
        {mContext = cx; mOldReporter = JS_SetErrorReporter(cx, reporter);}
    ~JSCLAutoErrorReporterSetter()
        {JS_SetErrorReporter(mContext, mOldReporter);} 
private:
    JSContext* mContext;
    JSErrorReporter mOldReporter;
    // prevent copying and assignment
    JSCLAutoErrorReporterSetter(const JSCLAutoErrorReporterSetter &); // not implemented
    const JSCLAutoErrorReporterSetter& operator=(const JSCLAutoErrorReporterSetter &); // not implemented
};

static nsresult
OutputError(JSContext *cx,
            const char *format,
            va_list ap)
{
    char *buf = JS_vsmprintf(format, ap);
    if (!buf) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS_ReportError(cx, buf);
    JS_smprintf_free(buf);

    return NS_OK;
}

static nsresult
ReportOnCaller(nsAXPCNativeCallContext *cc,
               const char *format, ...) {
    if (!cc) {
        return NS_ERROR_FAILURE;
    }
    
    va_list ap;
    va_start(ap, format);

    nsresult rv;
    JSContext *callerContext;
    rv = cc->GetJSContext(&callerContext);
    NS_ENSURE_SUCCESS(rv, rv);

    return OutputError(callerContext, format, ap);
}

static nsresult
ReportOnCaller(JSCLContextHelper &helper,
               const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    JSContext *cx = helper.Pop();
    if (!cx) {
        return NS_ERROR_FAILURE;
    }

    return OutputError(cx, format, ap);
}

#ifdef MOZ_ENABLE_LIBXUL
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

    if (!JS_XDRScript(xdr, script)) {
        rv = NS_ERROR_FAILURE;
    }

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
    data = static_cast<char*>(JS_XDRMemGetData(xdr, &length));
    if (data) {
        JS_XDRMemSetData(xdr, nsnull, 0);
    }
    JS_XDRDestroy(xdr);

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
        const char* data = reinterpret_cast<const char*>
                                           (JS_XDRMemGetData(xdr, &size));
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
#endif // MOZ_ENABLE_LIBXUL

mozJSComponentLoader::mozJSComponentLoader()
    : mRuntime(nsnull),
      mContext(nsnull),
      mInitialized(PR_FALSE)
{
    NS_ASSERTION(!sSelf, "mozJSComponentLoader should be a singleton");

#ifdef PR_LOGGING
    if (!gJSCLLog) {
        gJSCLLog = PR_NewLogModule("JSComponentLoader");
    }
#endif

    sSelf = this;
}

mozJSComponentLoader::~mozJSComponentLoader()
{
    if (mInitialized) {
        NS_ERROR("'xpcom-shutdown-loaders' was not fired before cleaning up mozJSComponentLoader");
        UnloadModules();
    }

    sSelf = nsnull;
}

mozJSComponentLoader*
mozJSComponentLoader::sSelf;

NS_IMPL_ISUPPORTS3(mozJSComponentLoader,
                   mozilla::ModuleLoader,
                   xpcIJSModuleLoader,
                   nsIObserver)
 
nsresult
mozJSComponentLoader::ReallyInit()
{
    NS_TIME_FUNCTION;
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

    mContextStack = do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
    if (NS_FAILED(rv))
        return rv;

    // Create our compilation context.
    mContext = JS_NewContext(mRuntime, 256);
    if (!mContext)
        return NS_ERROR_OUT_OF_MEMORY;

    uint32 options = JS_GetOptions(mContext);
    JS_SetOptions(mContext, options | JSOPTION_XML);

    // Always use the latest js version
    JS_SetVersion(mContext, JSVERSION_LATEST);

    // Limit C stack consumption to a reasonable 512K
    JS_SetNativeStackQuota(mContext, 512 * 1024);

#ifndef XPCONNECT_STANDALONE
    nsCOMPtr<nsIScriptSecurityManager> secman = 
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!secman)
        return NS_ERROR_FAILURE;

    rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
    if (NS_FAILED(rv) || !mSystemPrincipal)
        return NS_ERROR_FAILURE;
#endif

    if (!mModules.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;
    if (!mImports.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;
    if (!mInProgressImports.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIObserverService> obsSvc =
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->AddObserver(this, "xpcom-shutdown-loaders", PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set up localized comparison and string conversion
    xpc_LocalizeContext(mContext);

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: ReallyInit success!\n");
#endif
    mInitialized = PR_TRUE;

    return NS_OK;
}

nsresult
mozJSComponentLoader::FileKey(nsILocalFile* aFile, nsAString &aResult)
{
    nsresult rv = NS_OK;
    nsAutoString canonicalPath;

#if defined(XP_WIN)
    nsCOMPtr<nsILocalFileWin> winFile = do_QueryInterface(aFile, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    winFile->GetCanonicalPath(canonicalPath);
#else
    aFile->GetPath(canonicalPath);
#endif

    aResult = NS_LITERAL_STRING("f");
    aResult += canonicalPath;

    return rv;
}

nsresult
mozJSComponentLoader::JarKey(nsILocalFile* aFile,
                             const nsACString &aComponentPath,
                             nsAString &aResult)
{
    nsresult rv = NS_OK;
    nsAutoString canonicalPath;

#if defined(XP_WIN)
    nsCOMPtr<nsILocalFileWin> winFile = do_QueryInterface(aFile, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    winFile->GetCanonicalPath(canonicalPath);
#else
    aFile->GetPath(canonicalPath);
#endif

    aResult = NS_LITERAL_STRING("j");
    aResult += canonicalPath;
    AppendUTF8toUTF16(aComponentPath, aResult);

    return rv;
}

const mozilla::Module*
mozJSComponentLoader::LoadModule(nsILocalFile* aComponentFile)
{
    nsCOMPtr<nsIURI> uri;
    nsCAutoString spec;
    NS_GetURLSpecFromActualFile(aComponentFile, spec);

    nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
    if (NS_FAILED(rv))
        return NULL;

    nsAutoString hashstring;
    rv = FileKey(aComponentFile, hashstring);
    if (NS_FAILED(rv))
        return NULL;

    return LoadModuleImpl(aComponentFile,
                          hashstring,
                          uri);
}

const mozilla::Module*
mozJSComponentLoader::LoadModuleFromJAR(nsILocalFile *aJarFile,
                                        const nsACString &aComponentPath)
{
#if !defined(XPCONNECT_STANDALONE)
    nsresult rv;

    nsCAutoString fullSpec, fileSpec;
    NS_GetURLSpecFromActualFile(aJarFile, fileSpec);
    fullSpec = "jar:";
    fullSpec += fileSpec;
    fullSpec += "!/";
    fullSpec += aComponentPath;

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), fullSpec);
    if (NS_FAILED(rv))
        return NULL;

    nsAutoString hashstring;
    rv = JarKey(aJarFile, aComponentPath, hashstring);
    if (NS_FAILED(rv))
        return NULL;

    return LoadModuleImpl(aJarFile,
                          hashstring,
                          uri);
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

const mozilla::Module*
mozJSComponentLoader::LoadModuleImpl(nsILocalFile* aSourceFile,
                                     nsAString &aKey,
                                     nsIURI* aComponentURI)
{
    nsresult rv;

#ifdef NS_FUNCTION_TIMER
    nsCAutoString spec__("N/A");
    aComponentURI->GetSpec(spec__);
    NS_TIME_FUNCTION_FMT("%s (line %d) (file: %s)", MOZ_FUNCTION_NAME,
                         __LINE__, spec__.get());
#endif

    if (!mInitialized) {
        rv = ReallyInit();
        if (NS_FAILED(rv))
            return NULL;
    }

    ModuleEntry* mod;
    if (mModules.Get(aKey, &mod))
	return mod;

    nsAutoPtr<ModuleEntry> entry(new ModuleEntry);
    if (!entry)
        return NULL;

    rv = GlobalForLocation(aSourceFile, aComponentURI, &entry->global,
                           &entry->location, nsnull);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "GlobalForLocation failed!\n");
#endif
        return NULL;
    }

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectServiceContractID,
                                               &rv);
    if (NS_FAILED(rv))
        return NULL;

    nsCOMPtr<nsIComponentManager> cm;
    rv = NS_GetComponentManager(getter_AddRefs(cm));
    if (NS_FAILED(rv))
        return NULL;

    JSCLContextHelper cx(this);
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, entry->global))
        return NULL;

    JSObject* cm_jsobj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> cm_holder;
    rv = xpc->WrapNative(cx, entry->global, cm, 
                         NS_GET_IID(nsIComponentManager),
                         getter_AddRefs(cm_holder));

    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "WrapNative(%p,%p,nsIComponentManager) failed: %x\n",
                (void *)(JSContext*)cx, (void *)mCompMgr, rv);
#endif
        return NULL;
    }

    rv = cm_holder->GetJSObject(&cm_jsobj);
    if (NS_FAILED(rv)) {
#ifdef DEBUG_shaver
        fprintf(stderr, "GetJSObject of ComponentManager failed\n");
#endif
        return NULL;
    }

    JSObject* file_jsobj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> file_holder;
    rv = xpc->WrapNative(cx, entry->global, aSourceFile,
                         NS_GET_IID(nsIFile),
                         getter_AddRefs(file_holder));

    if (NS_FAILED(rv)) {
        return NULL;
    }

    rv = file_holder->GetJSObject(&file_jsobj);
    if (NS_FAILED(rv)) {
        return NULL;
    }

    JSCLAutoErrorReporterSetter aers(cx, mozJSLoaderErrorReporter);

    jsval NSGetFactory_val;

    if (!JS_GetProperty(cx, entry->global, "NSGetFactory", &NSGetFactory_val) ||
        JSVAL_IS_VOID(NSGetFactory_val)) {
        return NULL;
    }

    if (JS_TypeOfValue(cx, NSGetFactory_val) != JSTYPE_FUNCTION) {
        nsCAutoString spec;
        aComponentURI->GetSpec(spec);
        JS_ReportError(cx, "%s has NSGetFactory property that is not a function",
                       spec.get());
        return NULL;
    }
    
    JSObject *jsGetFactoryObj;
    if (!JS_ValueToObject(cx, NSGetFactory_val, &jsGetFactoryObj) ||
        !jsGetFactoryObj) {
        /* XXX report error properly */
        return NULL;
    }

    rv = xpc->WrapJS(cx, jsGetFactoryObj,
                     NS_GET_IID(xpcIJSGetFactory), getter_AddRefs(entry->getfactoryobj));
    if (NS_FAILED(rv)) {
        /* XXX report error properly */
#ifdef DEBUG
        fprintf(stderr, "mJCL: couldn't get nsIModule from jsval\n");
#endif
        return NULL;
    }

    // Cache this module for later
    if (!mModules.Put(aKey, entry))
        return NULL;

    // The hash owns the ModuleEntry now, forget about it
    return entry.forget();
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

static const char baseName[2][5] = { "gre/", "app/" };

static inline PRBool
canonicalizeBase(nsCAutoString &spec, nsACString &out, mozilla::Omnijar::Type aType)
{
    nsCAutoString base;
    nsresult rv = mozilla::Omnijar::GetURIString(aType, base);

    if (NS_FAILED(rv) || !base.Length())
        return PR_FALSE;

    if (base.Compare(spec.get(), PR_FALSE, base.Length()))
        return PR_FALSE;

    out.Append("/resource/");
    out.Append(baseName[aType]);
    out.Append(Substring(spec, base.Length()));
    return PR_TRUE;
}
/**
 * PathifyURI transforms mozilla .js uris into useful zip paths
 * to make it makes it easier to manipulate startup cache entries
 * using standard zip tools.
 * Transformations applied:
 *  * jsloader/ prefix is used to group mozJSComponentLoader cache entries in
 *    a top-level zip directory.
 *  * resource:// URIs are resolved to their corresponding file/jar URI to
 *    canonicalize resources URIs other than gre and app.
 *  * Paths under GRE or APP directory have their base path replaced with
 *    resource/gre or resource/app to avoid depending on install location.
 *  * jar:file:///path/to/file.jar!/sub/path urls are replaced with
 *    /path/to/file.jar/sub/path
 *  * .bin suffix is added to the end of the path to indicate that jsloader/ entries
 *     are binary representations of JS source.
 * For example:
 *  resource://gre/modules/XPCOMUtils.jsm or
 *  file://$GRE_DIR/modules/XPCOMUtils.jsm or
 *  jar:file://$GRE_DIR/omni.jar!/modules/XPCOMUtils.jsm become
 *     jsloader/resource/gre/modules/XPCOMUtils.jsm.bin
 *  file://$PROFILE_DIR/extensions/{uuid}/components/component.js becomes
 *     jsloader/$PROFILE_DIR/extensions/%7Buuid%7D/components/component.js.bin
 *  jar:file://$PROFILE_DIR/extensions/some.xpi!/components/component.js becomes
 *     jsloader/$PROFILE_DIR/extensions/some.xpi/components/component.js.bin
 */
static nsresult
PathifyURI(nsIURI *in, nsACString &out)
{ 
    PRBool equals;
    nsresult rv;
    nsCOMPtr<nsIURI> uri = in;
    nsCAutoString spec;

    out = "jsloader";

    // Resolve resource:// URIs. At the end of this if/else block, we
    // have both spec and uri variables identifying the same URI.
    if (NS_SUCCEEDED(in->SchemeIs("resource", &equals)) && equals) {
        nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIProtocolHandler> ph;
        rv = ioService->GetProtocolHandler("resource", getter_AddRefs(ph));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIResProtocolHandler> irph(do_QueryInterface(ph, &rv));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = irph->ResolveURI(in, spec);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = ioService->NewURI(spec, nsnull, nsnull, getter_AddRefs(uri));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        rv = in->GetSpec(spec);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!canonicalizeBase(spec, out, mozilla::Omnijar::GRE) &&
        !canonicalizeBase(spec, out, mozilla::Omnijar::APP)) {
        if (NS_SUCCEEDED(uri->SchemeIs("file", &equals)) && equals) {
            nsCOMPtr<nsIFileURL> baseFileURL;
            baseFileURL = do_QueryInterface(uri, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCAutoString path;
            rv = baseFileURL->GetPath(path);
            NS_ENSURE_SUCCESS(rv, rv);

            out.Append(path);
        } else if (NS_SUCCEEDED(uri->SchemeIs("jar", &equals)) && equals) {
            nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIURI> jarFileURI;
            rv = jarURI->GetJARFile(getter_AddRefs(jarFileURI));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIFileURL> jarFileURL;
            jarFileURL = do_QueryInterface(jarFileURI, &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCAutoString path;
            rv = jarFileURL->GetPath(path);
            NS_ENSURE_SUCCESS(rv, rv);
            out.Append(path);

            rv = jarURI->GetJAREntry(path);
            NS_ENSURE_SUCCESS(rv, rv);
            out.Append("/");
            out.Append(path);
        } else { // Very unlikely
            nsCAutoString spec;
            rv = uri->GetSpec(spec);
            NS_ENSURE_SUCCESS(rv, rv);

            out.Append("/");
            out.Append(spec);
        }
    }

    out.Append(".bin");
    return NS_OK;
}

/* static */
#ifdef MOZ_ENABLE_LIBXUL
nsresult
mozJSComponentLoader::ReadScript(StartupCache* cache, nsIURI *uri,
                                 JSContext *cx, JSScript **script)
{
    nsresult rv;
    
    nsCAutoString spec;
    rv = PathifyURI(uri, spec);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoArrayPtr<char> buf;   
    PRUint32 len;
    rv = cache->GetBuffer(spec.get(), getter_Transfers(buf), 
                          &len);
    if (NS_FAILED(rv)) {
        return rv; // don't warn since NOT_AVAILABLE is an ok error
    }

    LOG(("Found %s in startupcache\n", spec.get()));
    nsCOMPtr<nsIObjectInputStream> ois;
    rv = NS_NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(ois));
    NS_ENSURE_SUCCESS(rv, rv);
    buf.forget();

    return ReadScriptFromStream(cx, ois, script);
}

nsresult
mozJSComponentLoader::WriteScript(StartupCache* cache, JSScript *script,
                                  nsIFile *component, nsIURI *uri, JSContext *cx)
{
    nsresult rv;

    nsCAutoString spec;
    rv = PathifyURI(uri, spec);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Writing %s to startupcache\n", spec.get()));
    nsCOMPtr<nsIObjectOutputStream> oos;
    nsCOMPtr<nsIStorageStream> storageStream; 
    rv = NS_NewObjectOutputWrappedStorageStream(getter_AddRefs(oos),
                                                getter_AddRefs(storageStream));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = WriteScriptToStream(cx, script, oos);
    oos->Close();
    NS_ENSURE_SUCCESS(rv, rv);
 
    nsAutoArrayPtr<char> buf;
    PRUint32 len;
    rv = NS_NewBufferFromStorageStream(storageStream, getter_Transfers(buf), 
                                       &len);
    NS_ENSURE_SUCCESS(rv, rv);
 
    rv = cache->PutBuffer(spec.get(), buf, len);
    return rv;
}
#endif //MOZ_ENABLE_LIBXUL

nsresult
mozJSComponentLoader::GlobalForLocation(nsILocalFile *aComponentFile,
                                        nsIURI *aURI,
                                        JSObject **aGlobal,
                                        char **aLocation,
                                        jsval *exception)
{
    nsresult rv;

    JSPrincipals* jsPrincipals = nsnull;
    JSCLContextHelper cx(this);

    // preserve caller's compartment
    js::PreserveCompartment pc(cx);
    
#ifndef XPCONNECT_STANDALONE
    rv = mSystemPrincipal->GetJSPrincipals(cx, &jsPrincipals);
    NS_ENSURE_SUCCESS(rv, rv);

    JSPrincipalsHolder princHolder(mContext, jsPrincipals);
#endif

    nsCOMPtr<nsIXPCScriptable> backstagePass;
    rv = mRuntimeService->GetBackstagePass(getter_AddRefs(backstagePass));
    NS_ENSURE_SUCCESS(rv, rv);

    JSCLAutoErrorReporterSetter aers(cx, mozJSLoaderErrorReporter);

    nsCOMPtr<nsIXPConnect> xpc =
        do_GetService(kXPConnectServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure InitClassesWithNewWrappedGlobal() installs the
    // backstage pass as the global in our compilation context.
    JS_SetGlobalObject(cx, nsnull);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->InitClassesWithNewWrappedGlobal(cx, backstagePass,
                                              NS_GET_IID(nsISupports),
                                              mSystemPrincipal,
                                              nsnull,
                                              nsIXPConnect::
                                                  FLAG_SYSTEM_GLOBAL_OBJECT,
                                              getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *global;
    rv = holder->GetJSObject(&global);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, global))
        return NS_ERROR_FAILURE;

    if (!JS_DefineFunctions(cx, global, gGlobalFun) ||
        !JS_DefineProfilingFunctions(cx, global)) {
        return NS_ERROR_FAILURE;
    }

    bool realFile = false;
    // need to be extra careful checking for URIs pointing to files
    // EnsureFile may not always get called, especially on resource URIs
    // so we need to call GetFile to make sure this is a valid file
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aURI, &rv);
    nsCOMPtr<nsIFile> testFile;
    if (NS_SUCCEEDED(rv)) {
        fileURL->GetFile(getter_AddRefs(testFile));
    }
    
    if (testFile) {
        realFile = true;

        nsCOMPtr<nsIXPConnectJSObjectHolder> locationHolder;
        rv = xpc->WrapNative(cx, global, aComponentFile,
                             NS_GET_IID(nsILocalFile),
                             getter_AddRefs(locationHolder));
        NS_ENSURE_SUCCESS(rv, rv);

        JSObject *locationObj;
        rv = locationHolder->GetJSObject(&locationObj);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!JS_DefineProperty(cx, global, "__LOCATION__",
                               OBJECT_TO_JSVAL(locationObj), nsnull, nsnull, 0))
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
    rv = aURI->GetSpec(nativePath);
    NS_ENSURE_SUCCESS(rv, rv);
#endif

    JSScript *script = nsnull;

#ifdef MOZ_ENABLE_LIBXUL  
    // Before compiling the script, first check to see if we have it in
    // the startupcache.  Note: as a rule, startupcache errors are not fatal
    // to loading the script, since we can always slow-load.
    
    PRBool writeToCache = PR_FALSE;
    StartupCache* cache = StartupCache::GetSingleton();

    if (cache) {
        rv = ReadScript(cache, aURI, cx, &script);
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully loaded %s from startupcache\n", nativePath.get()));
        } else {
            // This is ok, it just means the script is not yet in the
            // cache. Could mean that the cache was corrupted and got removed,
            // but either way we're going to write this out.
            writeToCache = PR_TRUE;
        }
    }
#endif

    if (!script) {
        // The script wasn't in the cache , so compile it now.
        LOG(("Slow loading %s\n", nativePath.get()));

        // If |exception| is non-null, then our caller wants us to propagate
        // any exceptions out to our caller. Ensure that the engine doesn't
        // eagerly report the exception.
        uint32 oldopts = 0;
        if (exception) {
            oldopts = JS_GetOptions(cx);
            JS_SetOptions(cx, oldopts | JSOPTION_DONT_REPORT_UNCAUGHT);
        }

        if (realFile) {
#ifdef HAVE_PR_MEMMAP
            PRInt64 fileSize;
            rv = aComponentFile->GetFileSize(&fileSize);
            if (NS_FAILED(rv)) {
                JS_SetOptions(cx, oldopts);
                return rv;
            }

            PRInt64 maxSize;
            LL_UI2L(maxSize, PR_UINT32_MAX);
            if (LL_CMP(fileSize, >, maxSize)) {
                NS_ERROR("file too large");
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FAILURE;
            }

            PRFileDesc *fileHandle;
            rv = aComponentFile->OpenNSPRFileDesc(PR_RDONLY, 0, &fileHandle);
            if (NS_FAILED(rv)) {
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FILE_NOT_FOUND;
            }

            // Make sure the file is closed, no matter how we return.
            FileAutoCloser fileCloser(fileHandle);

            PRFileMap *map = PR_CreateFileMap(fileHandle, fileSize,
                                              PR_PROT_READONLY);
            if (!map) {
                NS_ERROR("Failed to create file map");
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FAILURE;
            }

            // Make sure the file map is closed, no matter how we return.
            FileMapAutoCloser mapCloser(map);

            PRUint32 fileSize32;
            LL_L2UI(fileSize32, fileSize);

            char *buf = static_cast<char*>(PR_MemMap(map, 0, fileSize32));
            if (!buf) {
                NS_WARNING("Failed to map file");
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FAILURE;
            }

            script = JS_CompileScriptForPrincipalsVersion(
              cx, global, jsPrincipals, buf, fileSize32, nativePath.get(), 1,
              JSVERSION_LATEST);

            PR_MemUnmap(buf, fileSize32);

#else  /* HAVE_PR_MEMMAP */

            /**
             * No memmap implementation, so fall back to using
             * JS_CompileFileHandleForPrincipals().
             */

            FILE *fileHandle;
            rv = aComponentFile->OpenANSIFileDesc("r", &fileHandle);
            if (NS_FAILED(rv)) {
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FILE_NOT_FOUND;
            }

            script = JS_CompileFileHandleForPrincipalsVersion(
              cx, global, nativePath.get(), fileHandle, jsPrincipals, JSVERSION_LATEST);

            /* JS will close the filehandle after compilation is complete. */
#endif /* HAVE_PR_MEMMAP */
        } else {
            nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIChannel> scriptChannel;
            rv = ioService->NewChannelFromURI(aURI, getter_AddRefs(scriptChannel));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIInputStream> scriptStream;
            rv = scriptChannel->Open(getter_AddRefs(scriptStream));
            NS_ENSURE_SUCCESS(rv, rv);

            PRUint32 len, bytesRead;

            rv = scriptStream->Available(&len);
            NS_ENSURE_SUCCESS(rv, rv);
            if (!len)
                return NS_ERROR_FAILURE;

            /* malloc an internal buf the size of the file */
            nsAutoArrayPtr<char> buf(new char[len + 1]);
            if (!buf)
                return NS_ERROR_OUT_OF_MEMORY;

            /* read the file in one swoop */
            rv = scriptStream->Read(buf, len, &bytesRead);
            if (bytesRead != len)
                return NS_BASE_STREAM_OSERROR;

            buf[len] = '\0';

            script = JS_CompileScriptForPrincipalsVersion(
              cx, global, jsPrincipals, buf, bytesRead, nativePath.get(), 1,
              JSVERSION_LATEST);
        }
        // Propagate the exception, if one exists. Also, don't leave the stale
        // exception on this context.
        // NB: The caller must stick exception into a rooted slot (probably on
        // its context) as soon as possible to avoid GC hazards.
        if (exception) {
            JS_SetOptions(cx, oldopts);
            if (!script) {
                JS_GetPendingException(cx, exception);
                JS_ClearPendingException(cx);
            }
        }
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

    // Flag this script as a system script
    // FIXME: BUG 346139: We actually want to flag this exact filename, not
    // anything that starts with this filename... Maybe we need a way to do
    // that?  On the other hand, the fact that this is in our components dir
    // means that if someone snuck a malicious file into this dir we're screwed
    // anyway...  So maybe flagging as a prefix is fine.
    xpc->FlagSystemFilenamePrefix(nativePath.get(), PR_TRUE);

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: compiled JS component %s\n",
            nativePath.get());
#endif

#ifdef MOZ_ENABLE_LIBXUL
    if (writeToCache) {
        // We successfully compiled the script, so cache it. 
        rv = WriteScript(cache, script, aComponentFile, aURI, cx);

        // Don't treat failure to write as fatal, since we might be working
        // with a read-only cache.
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully wrote to cache\n"));
        } else {
            LOG(("Failed to write to cache\n"));
        }
    }
#endif

    // Assign aGlobal here so that it's available to recursive imports.
    // See bug 384168.
    *aGlobal = global;

    jsval retval;
    if (!JS_ExecuteScriptVersion(cx, global, script, &retval, JSVERSION_LATEST)) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: failed to execute %s\n", nativePath.get());
#endif
        *aGlobal = nsnull;
        return NS_ERROR_FAILURE;
    }

    /* Freed when we remove from the table. */
    *aLocation = ToNewCString(nativePath);
    if (!*aLocation) {
        *aGlobal = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS_AddNamedObjectRoot(cx, aGlobal, *aLocation);
    return NS_OK;
}

/* static */ PLDHashOperator
mozJSComponentLoader::ClearModules(const nsAString& key, ModuleEntry*& entry, void* cx)
{
    entry->Clear();
    return PL_DHASH_REMOVE;
}
    
void
mozJSComponentLoader::UnloadModules()
{
    mInitialized = PR_FALSE;

    mInProgressImports.Clear();
    mImports.Clear();

    mModules.Enumerate(ClearModules, NULL);

    // Destroying our context will force a GC.
    JS_DestroyContext(mContext);
    mContext = nsnull;

    mRuntimeService = nsnull;
    mContextStack = nsnull;
#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: UnloadAll(%d)\n", aWhen);
#endif
}

/* [JSObject] import (in AUTF8String registryLocation,
                      [optional] in JSObject targetObj ); */
NS_IMETHODIMP
mozJSComponentLoader::Import(const nsACString & registryLocation)
{
    // This function should only be called from JS.
    nsresult rv;

    NS_TIME_FUNCTION_FMT("%s (line %d) (file: %s)", MOZ_FUNCTION_NAME,
                         __LINE__, registryLocation.BeginReading());

    nsCOMPtr<nsIXPConnect> xpc =
        do_GetService(kXPConnectServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAXPCNativeCallContext *cc = nsnull;
    rv = xpc->GetCurrentNativeCallContext(&cc);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
    {
    // ensure that we are being call from JS, from this method
    nsCOMPtr<nsIInterfaceInfo> info;
    rv = cc->GetCalleeInterface(getter_AddRefs(info));
    NS_ENSURE_SUCCESS(rv, rv);
    nsXPIDLCString name;
    info->GetName(getter_Copies(name));
    NS_ASSERTION(nsCRT::strcmp("nsIXPCComponents_Utils", name.get()) == 0,
                 "Components.utils.import must only be called from JS.");
    PRUint16 methodIndex;
    const nsXPTMethodInfo *methodInfo;
    rv = info->GetMethodInfoForName("import", &methodIndex, &methodInfo);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint16 calleeIndex;
    rv = cc->GetCalleeMethodIndex(&calleeIndex);
    NS_ASSERTION(calleeIndex == methodIndex,
                 "Components.utils.import called from another utils method.");
    }
#endif

    JSContext *cx = nsnull;
    rv = cc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoRequest ar(cx);

    JSObject *targetObject = nsnull;

    PRUint32 argc = 0;
    rv = cc->GetArgc(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc > 1) {
        // The caller passed in the optional second argument. Get it.
        jsval *argv = nsnull;
        rv = cc->GetArgvPtr(&argv);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!JSVAL_IS_OBJECT(argv[1])) {
            return ReportOnCaller(cc, ERROR_SCOPE_OBJ,
                                  PromiseFlatCString(registryLocation).get());
        }
        targetObject = JSVAL_TO_OBJECT(argv[1]);
    } else {
        // Our targetObject is the caller's global object. Find it by
        // walking the calling object's parent chain.

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        rv = cc->GetCalleeWrapper(getter_AddRefs(wn));
        NS_ENSURE_SUCCESS(rv, rv);
        
        wn->GetJSObject(&targetObject);
        if (!targetObject) {
            NS_ERROR("null calling object");
            return NS_ERROR_FAILURE;
        }

        targetObject = JS_GetGlobalForObject(cx, targetObject);
    }
 
    JSAutoEnterCompartment ac;
    if (targetObject && !ac.enter(cx, targetObject)) {
        NS_ERROR("can't enter compartment");
        return NS_ERROR_FAILURE;
    }

    JSObject *globalObj = nsnull;
    rv = ImportInto(registryLocation, targetObject, cc, &globalObj);

    if (globalObj && !JS_WrapObject(cx, &globalObj)) {
        NS_ERROR("can't wrap return value");
        return NS_ERROR_FAILURE;
    }

    jsval *retval = nsnull;
    cc->GetRetValPtr(&retval);
    if (retval)
        *retval = OBJECT_TO_JSVAL(globalObj);

    return rv;
}

/* [noscript] JSObjectPtr importInto(in AUTF8String registryLocation,
                                     in JSObjectPtr targetObj); */
NS_IMETHODIMP
mozJSComponentLoader::ImportInto(const nsACString & aLocation,
                                 JSObject * targetObj,
                                 nsAXPCNativeCallContext * cc,
                                 JSObject * *_retval)
{
    nsresult rv;
    *_retval = nsnull;

    if (!mInitialized) {
        rv = ReallyInit();
        NS_ENSURE_SUCCESS(rv, rv);
    }
    
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the URI.
    nsCOMPtr<nsIURI> resURI;
    rv = ioService->NewURI(aLocation, nsnull, nsnull, getter_AddRefs(resURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // figure out the resolved URI
    nsCOMPtr<nsIChannel> scriptChannel;
    rv = ioService->NewChannelFromURI(resURI, getter_AddRefs(scriptChannel));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_INVALID_ARG);

    nsCOMPtr<nsIURI> resolvedURI;
    rv = scriptChannel->GetURI(getter_AddRefs(resolvedURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // get the JAR if there is one
    nsCOMPtr<nsIJARURI> jarURI;
    jarURI = do_QueryInterface(resolvedURI, &rv);
    nsCOMPtr<nsIFileURL> baseFileURL;
    nsCAutoString jarEntry;
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIURI> baseURI;
        rv = jarURI->GetJARFile(getter_AddRefs(baseURI));
        NS_ENSURE_SUCCESS(rv, rv);

        baseFileURL = do_QueryInterface(baseURI, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        jarURI->GetJAREntry(jarEntry);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        baseFileURL = do_QueryInterface(resolvedURI, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIFile> sourceFile;
    rv = baseFileURL->GetFile(getter_AddRefs(sourceFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> sourceLocalFile;
    sourceLocalFile = do_QueryInterface(sourceFile, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString key;
    if (jarEntry.IsEmpty()) {
        rv = FileKey(sourceLocalFile, key);
    } else {
        rv = JarKey(sourceLocalFile, jarEntry, key);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    ModuleEntry* mod;
    nsAutoPtr<ModuleEntry> newEntry;
    if (!mImports.Get(key, &mod) && !mInProgressImports.Get(key, &mod)) {
        newEntry = new ModuleEntry;
        if (!newEntry || !mInProgressImports.Put(key, newEntry))
            return NS_ERROR_OUT_OF_MEMORY;

        jsval exception = JSVAL_VOID;
        rv = GlobalForLocation(sourceLocalFile, resURI, &newEntry->global,
                               &newEntry->location, &exception);

        mInProgressImports.Remove(key);

        if (NS_FAILED(rv)) {
            *_retval = nsnull;

            if (!JSVAL_IS_VOID(exception)) {
                // An exception was thrown during compilation. Propagate it
                // out to our caller so they can report it.
                JSContext *callercx;
                cc->GetJSContext(&callercx);
                JS_SetPendingException(callercx, exception);
                return NS_OK;
            }

            // Something failed, but we don't know what it is, guess.
            return NS_ERROR_FILE_NOT_FOUND;
        }

        mod = newEntry;
    }

    NS_ASSERTION(mod->global, "Import table contains entry with no global");
    *_retval = mod->global;

    jsval symbols;
    if (targetObj) {
        JSCLContextHelper cxhelper(this);

        JSAutoEnterCompartment ac;
        if (!ac.enter(mContext, mod->global))
            return NS_ERROR_FAILURE;

        if (!JS_GetProperty(mContext, mod->global,
                            "EXPORTED_SYMBOLS", &symbols)) {
            return ReportOnCaller(cxhelper, ERROR_NOT_PRESENT,
                                  PromiseFlatCString(aLocation).get());
        }

        JSObject *symbolsObj = nsnull;
        if (!JSVAL_IS_OBJECT(symbols) ||
            !(symbolsObj = JSVAL_TO_OBJECT(symbols)) ||
            !JS_IsArrayObject(mContext, symbolsObj)) {
            return ReportOnCaller(cxhelper, ERROR_NOT_AN_ARRAY,
                                  PromiseFlatCString(aLocation).get());
        }

        // Iterate over symbols array, installing symbols on targetObj:

        jsuint symbolCount = 0;
        if (!JS_GetArrayLength(mContext, symbolsObj, &symbolCount)) {
            return ReportOnCaller(cxhelper, ERROR_GETTING_ARRAY_LENGTH,
                                  PromiseFlatCString(aLocation).get());
        }

#ifdef DEBUG
        nsCAutoString logBuffer;
#endif

        for (jsuint i = 0; i < symbolCount; ++i) {
            jsval val;
            jsid symbolId;

            if (!JS_GetElement(mContext, symbolsObj, i, &val) ||
                !JSVAL_IS_STRING(val) ||
                !JS_ValueToId(mContext, val, &symbolId)) {
                return ReportOnCaller(cxhelper, ERROR_ARRAY_ELEMENT,
                                      PromiseFlatCString(aLocation).get(), i);
            }

            if (!JS_GetPropertyById(mContext, mod->global, symbolId, &val)) {
                JSAutoByteString bytes(mContext, JSID_TO_STRING(symbolId));
                if (!bytes)
                    return NS_ERROR_FAILURE;
                return ReportOnCaller(cxhelper, ERROR_GETTING_SYMBOL,
                                      PromiseFlatCString(aLocation).get(),
                                      bytes.ptr());
            }

            JSAutoEnterCompartment target_ac;

            if (!target_ac.enter(mContext, targetObj) ||
                !JS_WrapValue(mContext, &val) ||
                !JS_SetPropertyById(mContext, targetObj, symbolId, &val)) {
                JSAutoByteString bytes(mContext, JSID_TO_STRING(symbolId));
                if (!bytes)
                    return NS_ERROR_FAILURE;
                return ReportOnCaller(cxhelper, ERROR_SETTING_SYMBOL,
                                      PromiseFlatCString(aLocation).get(),
                                      bytes.ptr());
            }
#ifdef DEBUG
            if (i == 0) {
                logBuffer.AssignLiteral("Installing symbols [ ");
            }
            JSAutoByteString bytes(mContext, JSID_TO_STRING(symbolId));
            if (!!bytes)
                logBuffer.Append(bytes.ptr());
            logBuffer.AppendLiteral(" ");
            if (i == symbolCount - 1) {
                LOG(("%s] from %s\n", PromiseFlatCString(logBuffer).get(),
                                      PromiseFlatCString(aLocation).get()));
            }
#endif
        }
    }

    // Cache this module for later
    if (newEntry) {
        if (!mImports.Put(key, newEntry))
            return NS_ERROR_OUT_OF_MEMORY;
        newEntry.forget();
    }
    
    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::Observe(nsISupports *subject, const char *topic,
                              const PRUnichar *data)
{
    if (!strcmp(topic, "xpcom-shutdown-loaders")) {
        UnloadModules();
    }
    else {
        NS_ERROR("Unexpected observer topic.");
    }

    return NS_OK;
}

/* static */ already_AddRefed<nsIFactory>
mozJSComponentLoader::ModuleEntry::GetFactory(const mozilla::Module& module,
                                              const mozilla::Module::CIDEntry& entry)
{
    const ModuleEntry& self = static_cast<const ModuleEntry&>(module);
    NS_ASSERTION(self.getfactoryobj, "Handing out an uninitialized module?");

    nsCOMPtr<nsIFactory> f;
    nsresult rv = self.getfactoryobj->Get(*entry.cid, getter_AddRefs(f));
    if (NS_FAILED(rv))
        return NULL;

    return f.forget();
}

//----------------------------------------------------------------------

JSCLContextHelper::JSCLContextHelper(mozJSComponentLoader *loader)
    : mContext(loader->mContext), mContextThread(0),
      mContextStack(loader->mContextStack)
{
    mContextStack->Push(mContext);
    mContextThread = JS_GetContextThread(mContext);
    if (mContextThread) {
        JS_BeginRequest(mContext);
    } 
}

// Pops the context that was pushed and then returns the context that is now at
// the top of the stack.
JSContext*
JSCLContextHelper::Pop()
{
    JSContext* cx = nsnull;
    if (mContextStack) {
        JS_ClearNewbornRoots(mContext);
        if (mContextThread) {
            JS_EndRequest(mContext);
        }

        mContextStack->Pop(nsnull);
        mContextStack->Peek(&cx);
        mContextStack = nsnull;
    }
    return cx;
}
