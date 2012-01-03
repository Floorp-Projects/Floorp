/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "mozilla/Attributes.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include <stdarg.h>

#include "prlog.h"
#ifdef ANDROID
#include <android/log.h>
#endif

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsICategoryManager.h"
#include "nsIComponentManager.h"
#include "mozilla/Module.h"
#include "nsILocalFile.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "mozJSComponentLoader.h"
#include "mozJSLoaderUtils.h"
#include "nsIJSRuntimeService.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsIObserverService.h"
#include "nsIXPCScriptable.h"
#include "nsString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIJARURI.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"
#include "jsxdrapi.h"
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
#include "xpcpublic.h"
#include "nsIResProtocolHandler.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Omnijar.h"

#include "jsdbgapi.h"

#include "mozilla/FunctionTimer.h"

using namespace mozilla;
using namespace mozilla::scache;

static const char kJSRuntimeServiceContractID[] = "@mozilla.org/js/xpc/RuntimeService;1";
static const char kXPConnectServiceContractID[] = "@mozilla.org/js/xpc/XPConnect;1";
static const char kObserverServiceContractID[] = "@mozilla.org/observer-service;1";
static const char kJSCachePrefix[] = "jsloader";

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
        return true;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return false;

    size_t length;
    const jschar *chars = JS_GetStringCharsAndLength(cx, str, &length);
    if (!chars)
        return false;

    NS_ConvertUTF16toUTF8 utf8str(reinterpret_cast<const PRUnichar*>(chars));
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_INFO, "Gecko", utf8str.get());
#endif
    fputs(utf8str.get(), stdout);
    fflush(stdout);
    return true;
}

static JSBool
Debug(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef DEBUG
    return Dump(cx, argc, vp);
#else
    return true;
#endif
}

static JSBool
Atob(JSContext *cx, uintN argc, jsval *vp)
{
    if (!argc)
        return true;

    return xpc::Base64Decode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

static JSBool
Btoa(JSContext *cx, uintN argc, jsval *vp)
{
    if (!argc)
        return true;

    return xpc::Base64Encode(cx, JS_ARGV(cx, vp)[0], &JS_RVAL(cx, vp));
}

static JSBool
File(JSContext *cx, uintN argc, jsval *vp)
{
    nsresult rv;

    if (!argc) {
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }

    nsCOMPtr<nsISupports> native;
    rv = nsDOMFileFile::NewFile(getter_AddRefs(native));
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }

    nsCOMPtr<nsIJSNativeInitializer> initializer = do_QueryInterface(native);
    NS_ASSERTION(initializer, "what?");

    rv = initializer->Initialize(nsnull, cx, nsnull, argc, JS_ARGV(cx, vp));
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }

    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if (!xpc) {
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
        return false;
    }

    JSObject* glob = JS_GetGlobalForScopeChain(cx);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    jsval retval;
    rv = xpc->WrapNativeToJSVal(cx, glob, native, nsnull,
                                &NS_GET_IID(nsISupports),
                                true, &retval, nsnull);
    if (NS_FAILED(rv)) {
        XPCThrower::Throw(rv, cx);
        return false;
    }

    JS_SET_RVAL(cx, vp, retval);
    return true;
}

static JSFunctionSpec gGlobalFun[] = {
    {"dump",    Dump,   1,0},
    {"debug",   Debug,  1,0},
    {"atob",    Atob,   1,0},
    {"btoa",    Btoa,   1,0},
    {"File",    File,   1,JSFUN_CONSTRUCTOR},
    {nsnull,nsnull,0,0}
};

class JSCLContextHelper
{
public:
    JSCLContextHelper(mozJSComponentLoader* loader);
    ~JSCLContextHelper();

    void reportErrorAfterPop(char *buf);

    operator JSContext*() const {return mContext;}

private:
    JSContext* mContext;
    intN       mContextThread;
    nsIThreadJSContextStack* mContextStack;
    char*      mBuf;

    // prevent copying and assignment
    JSCLContextHelper(const JSCLContextHelper &) MOZ_DELETE;
    const JSCLContextHelper& operator=(const JSCLContextHelper &) MOZ_DELETE;
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

    JSCLAutoErrorReporterSetter(const JSCLAutoErrorReporterSetter &) MOZ_DELETE;
    const JSCLAutoErrorReporterSetter& operator=(const JSCLAutoErrorReporterSetter &) MOZ_DELETE;
};

static nsresult
ReportOnCaller(JSContext *callerContext,
               const char *format, ...) {
    if (!callerContext) {
        return NS_ERROR_FAILURE;
    }

    va_list ap;
    va_start(ap, format);

    char *buf = JS_vsmprintf(format, ap);
    if (!buf) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    JS_ReportError(callerContext, buf);
    JS_smprintf_free(buf);

    return NS_OK;
}

static nsresult
ReportOnCaller(JSCLContextHelper &helper,
               const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    char *buf = JS_vsmprintf(format, ap);
    if (!buf) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    helper.reportErrorAfterPop(buf);

    return NS_OK;
}

mozJSComponentLoader::mozJSComponentLoader()
    : mRuntime(nsnull),
      mContext(nsnull),
      mInitialized(false)
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

    uint32_t options = JS_GetOptions(mContext);
    JS_SetOptions(mContext, options | JSOPTION_XML);

    // Always use the latest js version
    JS_SetVersion(mContext, JSVERSION_LATEST);

    // Limit C stack consumption to a reasonable 512K
    JS_SetNativeStackQuota(mContext, 512 * 1024);

    nsCOMPtr<nsIScriptSecurityManager> secman =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!secman)
        return NS_ERROR_FAILURE;

    rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
    if (NS_FAILED(rv) || !mSystemPrincipal)
        return NS_ERROR_FAILURE;

    if (!mModules.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;
    if (!mImports.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;
    if (!mInProgressImports.Init(32))
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIObserverService> obsSvc =
        do_GetService(kObserverServiceContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->AddObserver(this, "xpcom-shutdown-loaders", false);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set up localized comparison and string conversion
    xpc_LocalizeContext(mContext);

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: ReallyInit success!\n");
#endif
    mInitialized = true;

    return NS_OK;
}

const mozilla::Module*
mozJSComponentLoader::LoadModule(FileLocation &aFile)
{
    nsCOMPtr<nsILocalFile> file = aFile.GetBaseFile();

    nsCString spec;
    aFile.GetURIString(spec);

    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
    if (NS_FAILED(rv))
        return NULL;

#ifdef NS_FUNCTION_TIMER
    NS_TIME_FUNCTION_FMT("%s (line %d) (file: %s)", MOZ_FUNCTION_NAME,
                         __LINE__, spec.get());
#endif

    if (!mInitialized) {
        rv = ReallyInit();
        if (NS_FAILED(rv))
            return NULL;
    }

    ModuleEntry* mod;
    if (mModules.Get(spec, &mod))
	return mod;

    nsAutoPtr<ModuleEntry> entry(new ModuleEntry);
    if (!entry)
        return NULL;

    rv = GlobalForLocation(file, uri, &entry->global,
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
    rv = xpc->WrapNative(cx, entry->global, file,
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
        uri->GetSpec(spec);
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
    if (!mModules.Put(spec, entry))
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
#else
class ANSIFileAutoCloser
{
 public:
    explicit ANSIFileAutoCloser(FILE *file) : mFile(file) {}
    ~ANSIFileAutoCloser() { fclose(mFile); }
 private:
    FILE *mFile;
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

    JS_AbortIfWrongThread(JS_GetRuntime(cx));

    // preserve caller's compartment
    js::AutoPreserveCompartment pc(cx);

    rv = mSystemPrincipal->GetJSPrincipals(cx, &jsPrincipals);
    NS_ENSURE_SUCCESS(rv, rv);

    JSPrincipalsHolder princHolder(mContext, jsPrincipals);

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
    rv = aURI->GetSpec(nativePath);
    NS_ENSURE_SUCCESS(rv, rv);

    // Expose the URI from which the script was imported through a special
    // variable that we insert into the JSM.
    JSString *exposedUri = JS_NewStringCopyN(cx, nativePath.get(), nativePath.Length());
    if (!JS_DefineProperty(cx, global, "__URI__",
                           STRING_TO_JSVAL(exposedUri), nsnull, nsnull, 0))
        return NS_ERROR_FAILURE;


    JSScript *script = nsnull;

    // Before compiling the script, first check to see if we have it in
    // the startupcache.  Note: as a rule, startupcache errors are not fatal
    // to loading the script, since we can always slow-load.

    bool writeToCache = false;
    StartupCache* cache = StartupCache::GetSingleton();

    nsCAutoString cachePath(kJSCachePrefix);
    rv = PathifyURI(aURI, cachePath);
    NS_ENSURE_SUCCESS(rv, rv);

    if (cache) {
        rv = ReadCachedScript(cache, cachePath, cx, &script);
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully loaded %s from startupcache\n", nativePath.get()));
        } else {
            // This is ok, it just means the script is not yet in the
            // cache. Could mean that the cache was corrupted and got removed,
            // but either way we're going to write this out.
            writeToCache = true;
        }
    }

    if (!script) {
        // The script wasn't in the cache , so compile it now.
        LOG(("Slow loading %s\n", nativePath.get()));

        // If |exception| is non-null, then our caller wants us to propagate
        // any exceptions out to our caller. Ensure that the engine doesn't
        // eagerly report the exception.
        uint32_t oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_NO_SCRIPT_RVAL |
                      (exception ? JSOPTION_DONT_REPORT_UNCAUGHT : 0));

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

            script = JS_CompileScriptForPrincipalsVersion(cx, global, jsPrincipals, buf, fileSize32, nativePath.get(), 1,
                                                          JSVERSION_LATEST);

            PR_MemUnmap(buf, fileSize32);

#else  /* HAVE_PR_MEMMAP */

            /**
             * No memmap implementation, so fall back to 
             * reading in the file
             */

            FILE *fileHandle;
            rv = aComponentFile->OpenANSIFileDesc("r", &fileHandle);
            if (NS_FAILED(rv)) {
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FILE_NOT_FOUND;
            }

            // Ensure file fclose
            ANSIFileAutoCloser fileCloser(fileHandle);

            PRInt64 len;
            rv = aComponentFile->GetFileSize(&len);
            if (NS_FAILED(rv) || len < 0) {
                NS_WARNING("Failed to get file size");
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FAILURE;
            }

            char *buf = (char *) malloc(len * sizeof(char));
            if (!buf) {
                JS_SetOptions(cx, oldopts);
                return NS_ERROR_FAILURE;
            }

            size_t rlen = fread(buf, 1, len, fileHandle);
            if (rlen != (PRUint64)len) {
                free(buf);
                JS_SetOptions(cx, oldopts);
                NS_WARNING("Failed to read file");
                return NS_ERROR_FAILURE;
            }
            script = JS_CompileScriptForPrincipalsVersion(cx, global, jsPrincipals, buf, rlen, nativePath.get(), 1,
                                                          JSVERSION_LATEST);

            free(buf);

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

            script = JS_CompileScriptForPrincipalsVersion(cx, global, jsPrincipals, buf, bytesRead, nativePath.get(), 1,
                                                          JSVERSION_LATEST);
        }
        // Propagate the exception, if one exists. Also, don't leave the stale
        // exception on this context.
        JS_SetOptions(cx, oldopts);
        if (!script && exception) {
            JS_GetPendingException(cx, exception);
            JS_ClearPendingException(cx);
        }
    }

    if (!script) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: script compilation of %s FAILED\n",
                nativePath.get());
#endif
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_shaver_off
    fprintf(stderr, "mJCL: compiled JS component %s\n",
            nativePath.get());
#endif

    if (writeToCache) {
        // We successfully compiled the script, so cache it.
        rv = WriteCachedScript(cache, cachePath, cx, script);

        // Don't treat failure to write as fatal, since we might be working
        // with a read-only cache.
        if (NS_SUCCEEDED(rv)) {
            LOG(("Successfully wrote to cache\n"));
        } else {
            LOG(("Failed to write to cache\n"));
        }
    }

    // Assign aGlobal here so that it's available to recursive imports.
    // See bug 384168.
    *aGlobal = global;

    uint32_t oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts |
                  (exception ? JSOPTION_DONT_REPORT_UNCAUGHT : 0));
    bool ok = JS_ExecuteScriptVersion(cx, global, script, NULL, JSVERSION_LATEST);
    JS_SetOptions(cx, oldopts);

    if (!ok) {
#ifdef DEBUG_shaver_off
        fprintf(stderr, "mJCL: failed to execute %s\n", nativePath.get());
#endif
        if (exception) {
            JS_GetPendingException(cx, exception);
            JS_ClearPendingException(cx);
        }
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
mozJSComponentLoader::ClearModules(const nsACString& key, ModuleEntry*& entry, void* cx)
{
    entry->Clear();
    return PL_DHASH_REMOVE;
}

void
mozJSComponentLoader::UnloadModules()
{
    mInitialized = false;

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

NS_IMETHODIMP
mozJSComponentLoader::Import(const nsACString& registryLocation,
                             const JS::Value& targetObj,
                             JSContext* cx,
                             PRUint8 optionalArgc,
                             JS::Value* retval)
{
    NS_TIME_FUNCTION_FMT("%s (line %d) (file: %s)", MOZ_FUNCTION_NAME,
                         __LINE__, registryLocation.BeginReading());

    JSAutoRequest ar(cx);

    JSObject *targetObject = nsnull;

    if (optionalArgc) {
        // The caller passed in the optional second argument. Get it.
        if (!JSVAL_IS_OBJECT(targetObj)) {
            return ReportOnCaller(cx, ERROR_SCOPE_OBJ,
                                  PromiseFlatCString(registryLocation).get());
        }
        targetObject = JSVAL_TO_OBJECT(targetObj);
    } else {
        // Our targetObject is the caller's global object. Find it by
        // walking the calling object's parent chain.
        nsresult rv;
        nsCOMPtr<nsIXPConnect> xpc =
            do_GetService(kXPConnectServiceContractID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAXPCNativeCallContext *cc = nsnull;
        rv = xpc->GetCurrentNativeCallContext(&cc);
        NS_ENSURE_SUCCESS(rv, rv);

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
    nsresult rv = ImportInto(registryLocation, targetObject, cx, &globalObj);

    if (globalObj && !JS_WrapObject(cx, &globalObj)) {
        NS_ERROR("can't wrap return value");
        return NS_ERROR_FAILURE;
    }

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
    JSContext *callercx;
    nsresult rv = cc->GetJSContext(&callercx);
    NS_ENSURE_SUCCESS(rv, rv);
    return ImportInto(aLocation, targetObj, callercx, _retval);
}

nsresult
mozJSComponentLoader::ImportInto(const nsACString & aLocation,
                                 JSObject * targetObj,
                                 JSContext * callercx,
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
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIURI> baseURI;
        while (jarURI) {
            jarURI->GetJARFile(getter_AddRefs(baseURI));
            jarURI = do_QueryInterface(baseURI, &rv);
        }
        baseFileURL = do_QueryInterface(baseURI, &rv);
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

    nsCAutoString key;
    rv = resolvedURI->GetSpec(key);
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
                LOG(("%s] from %s\n", logBuffer.get(),
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
mozJSComponentLoader::Unload(const nsACString & aLocation)
{
    nsresult rv;

    if (!mInitialized) {
        return NS_OK;
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

    nsCAutoString key;
    rv = resolvedURI->GetSpec(key);
    NS_ENSURE_SUCCESS(rv, rv);

    ModuleEntry* mod;
    if (mImports.Get(key, &mod)) {
        mImports.Remove(key);
    }

    return NS_OK;
}

NS_IMETHODIMP
mozJSComponentLoader::Observe(nsISupports *subject, const char *topic,
                              const PRUnichar *data)
{
    if (!strcmp(topic, "xpcom-shutdown-loaders")) {
        UnloadModules();
    } else {
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
      mContextStack(loader->mContextStack),
      mBuf(nsnull)
{
    mContextStack->Push(mContext);
    mContextThread = JS_GetContextThread(mContext);
    if (mContextThread) {
        JS_BeginRequest(mContext);
    }
}

JSCLContextHelper::~JSCLContextHelper()
{
    if (mContextStack) {
        if (mContextThread) {
            JS_EndRequest(mContext);
        }

        mContextStack->Pop(nsnull);

        JSContext* cx = nsnull;
        mContextStack->Peek(&cx);

        mContextStack = nsnull;

        if (cx && mBuf) {
            JS_ReportError(cx, mBuf);
        }
    }

    if (mBuf) {
        JS_smprintf_free(mBuf);
    }
}

void
JSCLContextHelper::reportErrorAfterPop(char *buf)
{
    NS_ASSERTION(!mBuf, "Already called reportErrorAfterPop");
    mBuf = buf;
}
