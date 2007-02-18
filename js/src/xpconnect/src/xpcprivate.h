/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Mike Shaver <shaver@mozilla.org>
 *   Mark Hammond <MarkH@ActiveState.com>
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

/* All the XPConnect private declarations - only include locally. */

#ifndef xpcprivate_h___
#define xpcprivate_h___

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "nscore.h"
#include "nsXPCOM.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsPrimitives.h"
#include "nsIGenericFactory.h"
#include "nsMemory.h"
#include "nsIXPConnect.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIXPCSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsAutoLock.h"
#include "nsXPTCUtils.h"
#include "jsapi.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "prprf.h"
#include "jsinterp.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "jsgc.h"
#include "xptinfo.h"
#include "xpcforwards.h"
#include "xpclog.h"
#include "xpccomponents.h"
#include "xpcexception.h"
#include "xpcjsid.h"
#include "prlong.h"
#include "prmem.h"
#include "prenv.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"

#include "nsThreadUtils.h"
#include "nsIJSContextStack.h"
#include "nsDeque.h"
#include "nsVoidArray.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIExceptionService.h"

#include "nsVariant.h"
#include "nsIPropertyBag.h"
#include "nsIProperty.h"
#include "nsSupportsArray.h"

#include "nsIXPCScriptNotify.h"  // used to notify: ScriptEvaluated

#ifndef XPCONNECT_STANDALONE
#define XPC_USE_SECURITY_CHECKED_COMPONENT
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#endif

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
#include "nsISecurityCheckedComponent.h"
#endif

#ifdef XPC_TOOLS_SUPPORT
#include "nsIXPCToolsProfiler.h"
#endif

#ifdef XPC_IDISPATCH_SUPPORT
// This goop was added because of EXCEPINFO in ThrowCOMError
// This include is here, because it needs to occur before the undefines below
#include <atlbase.h>
#include "oaidl.h"
// Nasty MS defines
#undef GetClassInfo
#undef GetClassName
#undef GetMessage
#endif

/***************************************************************************/
// Compile time switches for instrumentation and stuff....

// Note that one would not normally turn *any* of these on in a non-DEBUG build.

#if defined(DEBUG_jband) || defined(DEBUG_jst) || defined(DEBUG_dbradley) || defined(DEBUG_shaver_no) || defined(DEBUG_timeless)
#define DEBUG_xpc_hacker
#endif

#if defined(DEBUG_brendan)
#define DEBUG_XPCNativeWrapper 1
#endif

#ifdef DEBUG
#define XPC_DETECT_LEADING_UPPERCASE_ACCESS_ERRORS
#define XPC_CHECK_WRAPPER_THREADSAFETY
#endif

#if defined(DEBUG_xpc_hacker)
#define XPC_DUMP_AT_SHUTDOWN
#define XPC_TRACK_WRAPPER_STATS
#define XPC_TRACK_SCOPE_STATS
#define XPC_TRACK_PROTO_STATS
#define XPC_TRACK_DEFERRED_RELEASES
#define XPC_CHECK_WRAPPERS_AT_SHUTDOWN
#define XPC_REPORT_SHADOWED_WRAPPED_NATIVE_MEMBERS
#define XPC_CHECK_CLASSINFO_CLAIMS
#if defined(DEBUG_jst)
#define XPC_ASSERT_CLASSINFO_CLAIMS
#endif
//#define DEBUG_stats_jband 1
//#define XPC_REPORT_NATIVE_INTERFACE_AND_SET_FLUSHING
//#define XPC_REPORT_JSCLASS_FLUSHING
//#define XPC_TRACK_AUTOMARKINGPTR_STATS
#endif

#if defined(DEBUG_dbaron) || defined(DEBUG_bzbarsky) // only part of DEBUG_xpc_hacker!
#define XPC_DUMP_AT_SHUTDOWN
#endif

/***************************************************************************/
// conditional forward declarations....

#ifdef XPC_REPORT_SHADOWED_WRAPPED_NATIVE_MEMBERS
void DEBUG_ReportShadowedMembers(XPCNativeSet* set,
                                 XPCWrappedNative* wrapper,
                                 XPCWrappedNativeProto* proto);
#else
#define DEBUG_ReportShadowedMembers(set, wrapper, proto) ((void)0)
#endif

#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
void DEBUG_ReportWrapperThreadSafetyError(XPCCallContext& ccx,
                                          const char* msg,
                                          const XPCWrappedNative* wrapper);
void DEBUG_CheckWrapperThreadSafety(const XPCWrappedNative* wrapper);
#else
#define DEBUG_CheckWrapperThreadSafety(w) ((void)0)
#endif

/***************************************************************************/

// Defeat possible Windows macro-mangling of the name
#ifdef GetClassInfo
#undef GetClassInfo
#endif

/***************************************************************************/
// default initial sizes for maps (hashtables)

#define XPC_CONTEXT_MAP_SIZE                16
#define XPC_JS_MAP_SIZE                     64
#define XPC_JS_CLASS_MAP_SIZE               64

#define XPC_NATIVE_MAP_SIZE                 64
#define XPC_NATIVE_PROTO_MAP_SIZE           16
#define XPC_DYING_NATIVE_PROTO_MAP_SIZE     16
#define XPC_DETACHED_NATIVE_PROTO_MAP_SIZE  32
#define XPC_NATIVE_INTERFACE_MAP_SIZE       64
#define XPC_NATIVE_SET_MAP_SIZE             64
#define XPC_NATIVE_JSCLASS_MAP_SIZE         32
#define XPC_THIS_TRANSLATOR_MAP_SIZE         8
#define XPC_NATIVE_WRAPPER_MAP_SIZE         16

/***************************************************************************/
// data declarations...
extern const char* XPC_ARG_FORMATTER_FORMAT_STRINGS[]; // format strings
extern const char XPC_CONTEXT_STACK_CONTRACTID[];
extern const char XPC_RUNTIME_CONTRACTID[];
extern const char XPC_EXCEPTION_CONTRACTID[];
extern const char XPC_CONSOLE_CONTRACTID[];
extern const char XPC_SCRIPT_ERROR_CONTRACTID[];
extern const char XPC_ID_CONTRACTID[];
extern const char XPC_XPCONNECT_CONTRACTID[];

/***************************************************************************/
// useful macros...

#define XPC_STRING_GETTER_BODY(dest, src) \
    NS_ENSURE_ARG_POINTER(dest); \
    char* result; \
    if(src) \
        result = (char*) nsMemory::Clone(src, \
                                sizeof(char)*(strlen(src)+1)); \
    else \
        result = nsnull; \
    *dest = result; \
    return (result || !src) ? NS_OK : NS_ERROR_OUT_OF_MEMORY

/***************************************************************************/
// Auto locking support class...

// We PROMISE to never screw this up.
#ifdef _MSC_VER
#pragma warning(disable : 4355) // OK to pass "this" in member initializer
#endif

typedef PRMonitor XPCLock;

static inline void xpc_Wait(XPCLock* lock) 
    {
        NS_ASSERTION(lock, "xpc_Wait called with null lock!");
#ifdef DEBUG
        PRStatus result = 
#endif
        PR_Wait(lock, PR_INTERVAL_NO_TIMEOUT);
        NS_ASSERTION(PR_SUCCESS == result, "bad result from PR_Wait!");
    }

static inline void xpc_NotifyAll(XPCLock* lock) 
    {
        NS_ASSERTION(lock, "xpc_NotifyAll called with null lock!");
#ifdef DEBUG
        PRStatus result = 
#endif    
        PR_NotifyAll(lock);
        NS_ASSERTION(PR_SUCCESS == result, "bad result from PR_NotifyAll!");
    }

// This is a cloned subset of nsAutoMonitor. We want the use of a monitor -
// mostly because we need reenterability - but we also want to support passing
// a null monitor in without things blowing up. This is used for wrappers that
// are guaranteed to be used only on one thread. We avoid lock overhead by
// using a null monitor. By changing this class we can avoid having multiplte
// code paths or (conditional) manual calls to PR_{Enter,Exit}Monitor.
//
// Note that xpconnect only makes *one* monitor and *mostly* holds it locked
// only through very small critical sections.

class XPCAutoLock : public nsAutoLockBase {
public:

    static XPCLock* NewLock(const char* name)
                        {return nsAutoMonitor::NewMonitor(name);}
    static void     DestroyLock(XPCLock* lock)
                        {nsAutoMonitor::DestroyMonitor(lock);}

    XPCAutoLock(XPCLock* lock)
#ifdef DEBUG_jband
        : nsAutoLockBase(lock ? (void*) lock : (void*) this, eAutoMonitor),
#else
        : nsAutoLockBase(lock, eAutoMonitor),
#endif
          mLock(lock)
    {
        if(mLock)
            PR_EnterMonitor(mLock);
    }

    ~XPCAutoLock()
    {
        if(mLock)
        {
#ifdef DEBUG
            PRStatus status =
#endif
                PR_ExitMonitor(mLock);
            NS_ASSERTION(status == PR_SUCCESS, "PR_ExitMonitor failed");
        }
    }

private:
    XPCLock*  mLock;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an XPCAutoLock object incorrectly.
    XPCAutoLock(void) {}
    XPCAutoLock(XPCAutoLock& /*aMon*/) {}
    XPCAutoLock& operator =(XPCAutoLock& /*aMon*/) {
        return *this;
    }

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an XPCAutoLock object on the heap.
    static void* operator new(size_t /*size*/) CPP_THROW_NEW {
        return nsnull;
    }
    static void operator delete(void* /*memory*/) {}
};

/************************************************/

class XPCAutoUnlock : public nsAutoUnlockBase {
public:
    XPCAutoUnlock(XPCLock* lock)
        : nsAutoUnlockBase(lock),
          mLock(lock)
    {
        if(mLock)
        {
#ifdef DEBUG
            PRStatus status =
#endif
                PR_ExitMonitor(mLock);
            NS_ASSERTION(status == PR_SUCCESS, "PR_ExitMonitor failed");
        }
    }

    ~XPCAutoUnlock()
    {
        if(mLock)
            PR_EnterMonitor(mLock);
    }

private:
    XPCLock*  mLock;

    // Not meant to be implemented. This makes it a compiler error to
    // construct or assign an XPCAutoUnlock object incorrectly.
    XPCAutoUnlock(void) {}
    XPCAutoUnlock(XPCAutoUnlock& /*aMon*/) {}
    XPCAutoUnlock& operator =(XPCAutoUnlock& /*aMon*/) {
        return *this;
    }

    // Not meant to be implemented. This makes it a compiler error to
    // attempt to create an XPCAutoUnlock object on the heap.
    static void* operator new(size_t /*size*/) CPP_THROW_NEW {
        return nsnull;
    }
    static void operator delete(void* /*memory*/) {}
};

// A helper class to deal with temporary JS contexts. It destroys the context
// when it goes out of scope.
class XPCAutoJSContext
{
public:
    XPCAutoJSContext(JSContext *aContext, PRBool aGCOnDestroy)
        : mContext(aContext), mGCOnDestroy(aGCOnDestroy)
    {
    }

    ~XPCAutoJSContext()
    {
        if(!mContext)
            return;

        if(mGCOnDestroy)
            JS_DestroyContext(mContext);
        else
            JS_DestroyContextNoGC(mContext);
    }

    operator JSContext * () {return mContext;}

private:
    JSContext *mContext;
    PRBool mGCOnDestroy;
};

/***************************************************************************
****************************************************************************
*
* Core runtime and context classes...
*
****************************************************************************
***************************************************************************/

// We have a general rule internally that getters that return addref'd interface
// pointer generally do so using an 'out' parm. When interface pointers are
// returned as function call result values they are not addref'd. Exceptions
// to this rule are noted explicitly.

const PRBool OBJ_IS_GLOBAL = PR_TRUE;
const PRBool OBJ_IS_NOT_GLOBAL = PR_FALSE;
struct JSObjectRefcounts;

class nsXPConnect : public nsIXPConnect,
                    public nsSupportsWeakReference,
                    public nsCycleCollectionLanguageRuntime
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECT

    // non-interface implementation
public:
    // These get non-addref'd pointers
    static nsXPConnect*  GetXPConnect();
    static XPCJSRuntime* GetRuntime(nsXPConnect* xpc = nsnull);
    static XPCContext*   GetContext(JSContext* cx, nsXPConnect* xpc = nsnull);
    static nsIJSRuntimeService* GetJSRuntimeService(nsXPConnect* xpc = nsnull);

    // Gets addref'd pointer
    static nsresult GetInterfaceInfoManager(nsIInterfaceInfoSuperManager** iim,
                                            nsXPConnect* xpc = nsnull);

    // Gets addref'd pointer
    static nsresult GetContextStack(nsIThreadJSContextStack** stack,
                                    nsXPConnect* xpc = nsnull);

    static JSBool IsISupportsDescendant(nsIInterfaceInfo* info);

    nsIXPCSecurityManager* GetDefaultSecurityManager() const
        {return mDefaultSecurityManager;}

    PRUint16 GetDefaultSecurityManagerFlags() const
        {return mDefaultSecurityManagerFlags;}

    // This returns an AddRef'd pointer. It does not do this with an 'out' param
    // only because this form is required by the generic module macro:
    // NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR
    static nsXPConnect* GetSingleton();

    // Called by module code in dll startup
    static void InitStatics() { gSelf = nsnull; gOnceAliveNowDead = JS_FALSE; }
    // Called by module code on dll shutdown.
    static void ReleaseXPConnectSingleton();

    virtual ~nsXPConnect();

    JSBool IsShuttingDown() const {return mShuttingDown;}

    nsresult GetInfoForIID(const nsIID * aIID, nsIInterfaceInfo** info);
    nsresult GetInfoForName(const char * name, nsIInterfaceInfo** info);

    // from nsCycleCollectionLanguageRuntime
    nsresult BeginCycleCollection();
    nsresult Root(const nsDeque &nodes);
    nsresult Unlink(const nsDeque &nodes);
    nsresult Unroot(const nsDeque &nodes);
    nsresult Traverse(void *p, nsCycleCollectionTraversalCallback &cb);
    nsresult FinishCycleCollection();

#ifdef XPC_IDISPATCH_SUPPORT
public:
    static PRBool IsIDispatchEnabled();
#endif
protected:
    nsXPConnect();

private:
    JSBool EnsureRuntime() {return mRuntime ? JS_TRUE : CreateRuntime();}
    JSBool CreateRuntime();

    static PRThread* FindMainThread();

private:
    // Singleton instance
    static nsXPConnect*      gSelf;
    static JSBool            gOnceAliveNowDead;

    XPCJSRuntime*            mRuntime;
    nsCOMPtr<nsIInterfaceInfoSuperManager> mInterfaceInfoManager;
    nsIThreadJSContextStack* mContextStack;
    nsIXPCSecurityManager*   mDefaultSecurityManager;
    PRUint16                 mDefaultSecurityManagerFlags;
    JSBool                   mShuttingDown;
    JSObjectRefcounts*       mObjRefcounts;

#ifdef XPC_TOOLS_SUPPORT
    nsCOMPtr<nsIXPCToolsProfiler> mProfiler;
    nsCOMPtr<nsILocalFile>        mProfilerOutputFile;
#endif

};

/***************************************************************************/

// In the current xpconnect system there can only be one XPCJSRuntime.
// So, xpconnect can only be used on one JSRuntime within the process.

// no virtuals. no refcounting.
class XPCJSRuntime
{
public:
    static XPCJSRuntime* newXPCJSRuntime(nsXPConnect* aXPConnect,
                                         nsIJSRuntimeService* aJSRuntimeService);

    JSRuntime*     GetJSRuntime() const {return mJSRuntime;}
    nsXPConnect*   GetXPConnect() const {return mXPConnect;}

    nsIJSRuntimeService* GetJSRuntimeService() const {return mJSRuntimeService;}

    JSObject2WrappedJSMap*     GetWrappedJSMap()        const
        {return mWrappedJSMap;}

    IID2WrappedJSClassMap*     GetWrappedJSClassMap()   const
        {return mWrappedJSClassMap;}

    IID2NativeInterfaceMap* GetIID2NativeInterfaceMap() const
        {return mIID2NativeInterfaceMap;}

    ClassInfo2NativeSetMap* GetClassInfo2NativeSetMap() const
        {return mClassInfo2NativeSetMap;}

    NativeSetMap* GetNativeSetMap() const
        {return mNativeSetMap;}

    IID2ThisTranslatorMap* GetThisTranslatorMap() const
        {return mThisTranslatorMap;}

    XPCNativeScriptableSharedMap* GetNativeScriptableSharedMap() const
        {return mNativeScriptableSharedMap;}

    XPCWrappedNativeProtoMap* GetDyingWrappedNativeProtoMap() const
        {return mDyingWrappedNativeProtoMap;}

    XPCWrappedNativeProtoMap* GetDetachedWrappedNativeProtoMap() const
        {return mDetachedWrappedNativeProtoMap;}

    XPCNativeWrapperMap* GetExplicitNativeWrapperMap() const
        {return mExplicitNativeWrapperMap;}

    XPCLock* GetMapLock() const {return mMapLock;}

    XPCContext* GetXPCContext(JSContext* cx);
    XPCContext* SyncXPCContextList(JSContext* cx = nsnull);

    JSBool GetMainThreadOnlyGC() const   {return mMainThreadOnlyGC;}
    void   SetMainThreadOnlyGC(JSBool b) {mMainThreadOnlyGC = b;}
    
    JSBool GetDeferReleases() const {return mDeferReleases;}
    void   SetDeferReleases(JSBool b) 
        {/* If deferring is turned off while any are pending they'll leak! */
         NS_ASSERTION((mDeferReleases && b) || 
                      !mNativesToReleaseArray.Count(), "bad"); 
         mDeferReleases = b;}

    JSBool DeferredRelease(nsISupports* obj);

    JSBool GetDoingFinalization() const {return mDoingFinalization;}

    // Mapping of often used strings to jsid atoms that live 'forever'.
    //
    // To add a new string: add to this list and to XPCJSRuntime::mStrings
    // at the top of xpcjsruntime.cpp
    enum {
        IDX_CONSTRUCTOR             = 0 ,
        IDX_TO_STRING               ,
        IDX_TO_SOURCE               ,
        IDX_LAST_RESULT             ,
        IDX_RETURN_CODE             ,
        IDX_VALUE                   ,
        IDX_QUERY_INTERFACE         ,
        IDX_COMPONENTS              ,
        IDX_WRAPPED_JSOBJECT        ,
        IDX_OBJECT                  ,
        IDX_FUNCTION                ,
        IDX_PROTOTYPE               ,
        IDX_CREATE_INSTANCE         ,
        IDX_ITEM                    ,
#ifdef XPC_IDISPATCH_SUPPORT
        IDX_ACTIVEX_OBJECT          ,
        IDX_COM_OBJECT              ,
        IDX_ACTIVEX_SUPPORTS        ,
#endif
        IDX_TOTAL_COUNT // just a count of the above
    };

    jsid GetStringID(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrIDs[index];
    }
    jsval GetStringJSVal(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrJSVals[index];
    }
    const char* GetStringName(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    static JSBool JS_DLL_CALLBACK GCCallback(JSContext *cx, JSGCStatus status);

    void DebugDump(PRInt16 depth);

    void SystemIsBeingShutDown(XPCCallContext* ccx);

    PRThread* GetThreadRunningGC() const {return mThreadRunningGC;}

    ~XPCJSRuntime();

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
   void DEBUG_AddWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {XPCAutoLock lock(GetMapLock());
         JSDHashEntryHdr *entry =
            JS_DHashTableOperate(DEBUG_WrappedNativeHashtable,
                                 wrapper, JS_DHASH_ADD);
         if(entry) ((JSDHashEntryStub *)entry)->key = wrapper;}

   void DEBUG_RemoveWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {XPCAutoLock lock(GetMapLock());
         JS_DHashTableOperate(DEBUG_WrappedNativeHashtable,
                              wrapper, JS_DHASH_REMOVE);}
private:
   JSDHashTable* DEBUG_WrappedNativeHashtable;
public:
#endif

private:
    XPCJSRuntime(); // no implementation
    XPCJSRuntime(nsXPConnect* aXPConnect,
                 nsIJSRuntimeService* aJSRuntimeService);

    JSContext2XPCContextMap*  GetContextMap() const {return mContextMap;}
    JSBool GenerateStringIDs(JSContext* cx);
    void PurgeXPCContextList();

private:
    static const char* mStrings[IDX_TOTAL_COUNT];
    jsid mStrIDs[IDX_TOTAL_COUNT];
    jsval mStrJSVals[IDX_TOTAL_COUNT];

    nsXPConnect* mXPConnect;
    JSRuntime*  mJSRuntime;
    nsIJSRuntimeService* mJSRuntimeService; // hold this to hold the JSRuntime
    JSContext2XPCContextMap* mContextMap;
    JSObject2WrappedJSMap*   mWrappedJSMap;
    IID2WrappedJSClassMap*   mWrappedJSClassMap;
    IID2NativeInterfaceMap*  mIID2NativeInterfaceMap;
    ClassInfo2NativeSetMap*  mClassInfo2NativeSetMap;
    NativeSetMap*            mNativeSetMap;
    IID2ThisTranslatorMap*   mThisTranslatorMap;
    XPCNativeScriptableSharedMap* mNativeScriptableSharedMap;
    XPCWrappedNativeProtoMap* mDyingWrappedNativeProtoMap;
    XPCWrappedNativeProtoMap* mDetachedWrappedNativeProtoMap;
    XPCNativeWrapperMap*     mExplicitNativeWrapperMap;
    XPCLock* mMapLock;
    PRThread* mThreadRunningGC;
    nsVoidArray mWrappedJSToReleaseArray;
    nsVoidArray mNativesToReleaseArray;
    JSBool mMainThreadOnlyGC;
    JSBool mDeferReleases;
    JSBool mDoingFinalization;
};

/***************************************************************************/
/***************************************************************************/
// XPCContext is mostly a dumb class to hold JSContext specific data and
// maps that let us find wrappers created for the given JSContext.

// no virtuals
class XPCContext
{
public:
    static XPCContext* newXPCContext(XPCJSRuntime* aRuntime,
                                     JSContext* aJSContext);

    XPCJSRuntime* GetRuntime() const {return mRuntime;}
    JSContext* GetJSContext() const {return mJSContext;}

    enum LangType {LANG_UNKNOWN, LANG_JS, LANG_NATIVE};
    
    // Mark functions used by SyncXPCContextList
    void Mark()             {mMarked = (JSPackedBool) JS_TRUE;}
    void Unmark()           {mMarked = (JSPackedBool) JS_FALSE;}
    JSBool IsMarked() const {return (JSBool) mMarked;}

    LangType GetCallingLangType() const
        {
            return mCallingLangType;
        }
    LangType SetCallingLangType(LangType lt)
        {
            LangType tmp = mCallingLangType; 
            mCallingLangType = lt; 
            return tmp;
        }
    JSBool CallerTypeIsJavaScript() const 
        {
            return LANG_JS == mCallingLangType;
        }
    JSBool CallerTypeIsNative() const 
        {
            return LANG_NATIVE == mCallingLangType;
        }
    JSBool CallerTypeIsKnown() const 
        {
            return LANG_UNKNOWN != mCallingLangType;
        }

    nsresult GetException(nsIException** e)
        {
            NS_IF_ADDREF(mException);
            *e = mException;
            return NS_OK;
        }
    void SetException(nsIException* e)
        {
            NS_IF_ADDREF(e);
            NS_IF_RELEASE(mException);
            mException = e;
        }

    nsresult GetLastResult() {return mLastResult;}
    void SetLastResult(nsresult rc) {mLastResult = rc;}

    nsresult GetPendingResult() {return mPendingResult;}
    void SetPendingResult(nsresult rc) {mPendingResult = rc;}

    nsIXPCSecurityManager* GetSecurityManager() const
        {return mSecurityManager;}
    void SetSecurityManager(nsIXPCSecurityManager* aSecurityManager)
        {mSecurityManager = aSecurityManager;}

    PRUint16 GetSecurityManagerFlags() const
        {return mSecurityManagerFlags;}
    void SetSecurityManagerFlags(PRUint16 f)
        {mSecurityManagerFlags = f;}

    nsIXPCSecurityManager* GetAppropriateSecurityManager(PRUint16 flags) const
        {
            NS_ASSERTION(CallerTypeIsKnown(),"missing caller type set somewhere");
            if(!CallerTypeIsJavaScript())
                return nsnull;
            if(mSecurityManager)
            {
                if(flags & mSecurityManagerFlags)
                    return mSecurityManager;
            }
            else
            {
                nsIXPCSecurityManager* mgr;
                nsXPConnect* xpc = mRuntime->GetXPConnect();
                mgr = xpc->GetDefaultSecurityManager();
                if(mgr && (flags & xpc->GetDefaultSecurityManagerFlags()))
                    return mgr;
            }
            return nsnull;
        }

    void DebugDump(PRInt16 depth);

    ~XPCContext();

private:
    XPCContext();    // no implementation
    XPCContext(XPCJSRuntime* aRuntime, JSContext* aJSContext);

private:
    XPCJSRuntime* mRuntime;
    JSContext*  mJSContext;
    nsresult mLastResult;
    nsresult mPendingResult;
    nsIXPCSecurityManager* mSecurityManager;
    nsIException* mException;
    LangType mCallingLangType;
    PRUint16 mSecurityManagerFlags;
    JSPackedBool mMarked;
};

/***************************************************************************/

#define NATIVE_CALLER  XPCContext::LANG_NATIVE
#define JS_CALLER      XPCContext::LANG_JS

// No virtuals
// XPCCallContext is ALWAYS declared as a local variable in some function;
// i.e. instance lifetime is always controled by some C++ function returning.
//
// These things are created frequently in many places. We *intentionally* do
// not inialialize all members in order to save on construction overhead.
// Some constructor pass more valid params than others. We init what must be
// init'd and leave other members undefined. In debug builds the accessors
// use a CHECK_STATE macro to track whether or not the object is in a valid
// state to answer the question a caller might be asking. As long as this
// class is maintained correctly it can do its job without a bunch of added
// overhead from useless initializations and non-DEBUG error checking.
//
// Note that most accessors are inlined.

class XPCCallContext : public nsIXPCNativeCallContext
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCNATIVECALLCONTEXT

    enum {NO_ARGS = (uintN) -1};

    XPCCallContext(XPCContext::LangType callerLanguage,
                   JSContext* cx    = nsnull,
                   JSObject* obj    = nsnull,
                   JSObject* funobj = nsnull,
                   jsval id         = 0,
                   uintN argc       = NO_ARGS,
                   jsval *argv      = nsnull,
                   jsval *rval      = nsnull);

    virtual ~XPCCallContext();

    inline JSBool                       IsValid() const ;

    inline nsXPConnect*                 GetXPConnect() const ;
    inline XPCJSRuntime*                GetRuntime() const ;
    inline XPCPerThreadData*            GetThreadData() const ;
    inline XPCContext*                  GetXPCContext() const ;
    inline JSContext*                   GetJSContext() const ;
    inline JSContext*                   GetSafeJSContext() const ;
    inline JSBool                       GetContextPopRequired() const ;
    inline XPCContext::LangType         GetCallerLanguage() const ;
    inline XPCContext::LangType         GetPrevCallerLanguage() const ;
    inline XPCCallContext*              GetPrevCallContext() const ;

    inline JSObject*                    GetOperandJSObject() const ;
    inline JSObject*                    GetCurrentJSObject() const ;
    inline JSObject*                    GetFlattenedJSObject() const ;

    inline nsISupports*                 GetIdentityObject() const ;
    inline XPCWrappedNative*            GetWrapper() const ;

    inline JSBool                       CanGetTearOff() const ;
    inline XPCWrappedNativeTearOff*     GetTearOff() const ;

    inline XPCNativeScriptableInfo*     GetScriptableInfo() const ;
    inline JSBool                       CanGetSet() const ;
    inline XPCNativeSet*                GetSet() const ;
    inline JSBool                       CanGetInterface() const ;
    inline XPCNativeInterface*          GetInterface() const ;
    inline XPCNativeMember*             GetMember() const ;
    inline JSBool                       HasInterfaceAndMember() const ;
    inline jsval                        GetName() const ;
    inline JSBool                       GetStaticMemberIsLocal() const ;
    inline uintN                        GetArgc() const ;
    inline jsval*                       GetArgv() const ;
    inline jsval*                       GetRetVal() const ;
    inline JSBool                       GetExceptionWasThrown() const ;
    inline JSBool                       GetReturnValueWasSet() const ;

    inline PRUint16                     GetMethodIndex() const ;
    inline void                         SetMethodIndex(PRUint16 index) ;

    inline JSBool   GetDestroyJSContextInDestructor() const;
    inline void     SetDestroyJSContextInDestructor(JSBool b);

    inline jsval GetResolveName() const;
    inline jsval SetResolveName(jsval name);

    inline XPCWrappedNative* GetResolvingWrapper() const;
    inline XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w);

    inline void SetRetVal(jsval val);

    inline JSObject* GetCallee() const;
    inline void SetCallee(JSObject* callee);

    void SetName(jsval name);
    void SetArgsAndResultPtr(uintN argc, jsval *argv, jsval *rval);
    void SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                     JSBool isSetter);

    nsresult  CanCallNow();

    void SystemIsBeingShutDown();

    operator JSContext*() const {return GetJSContext();}

#ifdef XPC_IDISPATCH_SUPPORT
    /**
     * Sets the IDispatch information for the context
     * This has to be void* because of icky Microsoft macros that
     * would be introduced if we included the DispatchInterface header
     */
    void SetIDispatchInfo(XPCNativeInterface* iface, void * member);
    void* GetIDispatchMember() const { return mIDispatchMember; }
#endif
private:

    // no copy ctor or assignment allowed
    XPCCallContext(const XPCCallContext& r); // not implemented
    XPCCallContext& operator= (const XPCCallContext& r); // not implemented

private:
    // posible values for mState
    enum State {
        INIT_FAILED,
        SYSTEM_SHUTDOWN,
        HAVE_CONTEXT,
        HAVE_OBJECT,
        HAVE_NAME,
        HAVE_ARGS,
        READY_TO_CALL,
        CALL_DONE
    };

#ifdef DEBUG
inline void CHECK_STATE(int s) const {NS_ASSERTION(mState >= s, "bad state");}
#else
#define CHECK_STATE(s) ((void)0)
#endif

private:
    State                           mState;

    nsXPConnect*                    mXPC;

    XPCPerThreadData*               mThreadData;
    XPCContext*                     mXPCContext;
    JSContext*                      mJSContext;
    JSBool                          mContextPopRequired;
    JSBool                          mDestroyJSContextInDestructor;

    XPCContext::LangType            mCallerLanguage;

    // ctor does not necessarily init the following. BEWARE!

    XPCContext::LangType            mPrevCallerLanguage;

    XPCCallContext*                 mPrevCallContext;

    JSObject*                       mOperandJSObject;
    JSObject*                       mCurrentJSObject;
    JSObject*                       mFlattenedJSObject;
    XPCWrappedNative*               mWrapper;
    XPCWrappedNativeTearOff*        mTearOff;

    XPCNativeScriptableInfo*        mScriptableInfo;

    XPCNativeSet*                   mSet;
    XPCNativeInterface*             mInterface;
    XPCNativeMember*                mMember;

    jsval                           mName;
    JSBool                          mStaticMemberIsLocal;

    uintN                           mArgc;
    jsval*                          mArgv;
    jsval*                          mRetVal;

    JSBool                          mExceptionWasThrown;
    JSBool                          mReturnValueWasSet;
#ifdef XPC_IDISPATCH_SUPPORT
    void*                           mIDispatchMember;
#endif
    PRUint16                        mMethodIndex;

    // If not null, this is the function object of the function we're going to
    // call.  This member only makes sense when CallerTypeIsNative() on our
    // XPCContext returns true.  We're not responsible for rooting this object;
    // whoever sets it on us needs to deal with that.
    JSObject*                       mCallee;
};


/***************************************************************************
****************************************************************************
*
* Core classes for wrapped native objects for use from JavaScript...
*
****************************************************************************
***************************************************************************/

// These are the various JSClasses and callbacks whose use that required
// visibility from more than one .cpp file.

extern JSExtendedClass XPC_WN_NoHelper_JSClass;
extern JSClass XPC_WN_NoMods_Proto_JSClass;
extern JSClass XPC_WN_ModsAllowed_Proto_JSClass;
extern JSClass XPC_WN_Tearoff_JSClass;

extern JSObjectOps * JS_DLL_CALLBACK
XPC_WN_GetObjectOpsNoCall(JSContext *cx, JSClass *clazz);

extern JSObjectOps * JS_DLL_CALLBACK
XPC_WN_GetObjectOpsWithCall(JSContext *cx, JSClass *clazz);

extern JSBool JS_DLL_CALLBACK
XPC_WN_CallMethod(JSContext *cx, JSObject *obj,
                  uintN argc, jsval *argv, jsval *vp);

extern JSBool JS_DLL_CALLBACK
XPC_WN_GetterSetter(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *vp);

extern JSBool
xpc_InitWrappedNativeJSOps();

#define IS_PROTO_CLASS(clazz)                                                 \
          ((clazz) == &XPC_WN_NoMods_Proto_JSClass ||                         \
           (clazz) == &XPC_WN_ModsAllowed_Proto_JSClass)

// Comes from xpcwrappednativeops.cpp
extern void
xpc_MarkForValidWrapper(JSContext *cx, XPCWrappedNative* wrapper, void *arg);

/***************************************************************************/

/***************************************************************************/
// XPCWrappedNativeScope is one-to-one with a JS global object.

class XPCWrappedNativeScope
{
public:

    static XPCWrappedNativeScope*
    GetNewOrUsed(XPCCallContext& ccx, JSObject* aGlobal);

    XPCJSRuntime*
    GetRuntime() const {return mRuntime;}

    Native2WrappedNativeMap*
    GetWrappedNativeMap() const {return mWrappedNativeMap;}

    ClassInfo2WrappedNativeProtoMap*
    GetWrappedNativeProtoMap() const {return mWrappedNativeProtoMap;}

    nsXPCComponents*
    GetComponents() const {return mComponents;}

    JSObject*
    GetGlobalJSObject() const {return mGlobalJSObject;}

    JSObject*
    GetPrototypeJSObject() const {return mPrototypeJSObject;}

#ifndef XPCONNECT_STANDALONE
    nsIPrincipal*
    GetPrincipal() const
    {return mScriptObjectPrincipal ?
         mScriptObjectPrincipal->GetPrincipal() : nsnull;}
#endif
    
    JSObject*
    GetPrototypeJSFunction() const {return mPrototypeJSFunction;}

    void RemoveWrappedNativeProtos();

    static XPCWrappedNativeScope*
    FindInJSObjectScope(XPCCallContext& ccx, JSObject* obj,
                        JSBool OKIfNotInitialized = JS_FALSE);

    static void
    SystemIsBeingShutDown(XPCCallContext& ccx);

    static void
    FinishedMarkPhaseOfGC(JSContext* cx, XPCJSRuntime* rt);

    static void
    FinishedFinalizationPhaseOfGC(JSContext* cx);

    static void
    MarkAllWrappedNativesAndProtos();

    static nsresult
    ClearAllWrappedNativeSecurityPolicies(XPCCallContext& ccx);

#ifdef DEBUG
    static void
    ASSERT_NoInterfaceSetsAreMarked();
#endif

    static void
    SweepAllWrappedNativeTearOffs();

    static void
    DebugDumpAllScopes(PRInt16 depth);

    void
    DebugDump(PRInt16 depth);

    JSBool
    IsValid() const {return mRuntime != nsnull;}

    void SetComponents(nsXPCComponents* aComponents);
    void SetGlobal(XPCCallContext& ccx, JSObject* aGlobal);

    static void InitStatics() { gScopes = nsnull; gDyingScopes = nsnull; }

protected:
    XPCWrappedNativeScope(XPCCallContext& ccx, JSObject* aGlobal);
    virtual ~XPCWrappedNativeScope();

    static void KillDyingScopes();

    XPCWrappedNativeScope(); // not implemented

private:
    static XPCWrappedNativeScope* gScopes;
    static XPCWrappedNativeScope* gDyingScopes;

    XPCJSRuntime*                    mRuntime;
    Native2WrappedNativeMap*         mWrappedNativeMap;
    ClassInfo2WrappedNativeProtoMap* mWrappedNativeProtoMap;
    nsXPCComponents*                 mComponents;
    XPCWrappedNativeScope*           mNext;
    // The JS global object for this scope.  If non-null, this will be the
    // default parent for the XPCWrappedNatives that have us as the scope,
    // unless a PreCreate hook overrides it.  Note that this _may_ be null (see
    // constructor).
    JSObject*                        mGlobalJSObject;
    JSObject*                        mPrototypeJSObject;
    JSObject*                        mPrototypeJSFunction;

#ifndef XPCONNECT_STANDALONE
    // The script object principal instance corresponding to our current global
    // JS object.
    // XXXbz what happens if someone calls JS_SetPrivate on mGlobalJSObject.
    // How do we deal?  Do we need to?  I suspect this isn't worth worrying
    // about, since all of our scope objects are verified as not doing that.
    nsCOMPtr<nsIScriptObjectPrincipal> mScriptObjectPrincipal;
#endif
};

/***************************************************************************/
// XPCNativeMember represents a single idl declared method, attribute or
// constant.

// Tight. No virtual methods. Can be bitwise copied (until any resolution done).

class XPCNativeMember
{
public:
    static JSBool GetCallInfo(XPCCallContext& ccx,
                              JSObject* funobj,
                              XPCNativeInterface** pInterface,
                              XPCNativeMember**    pMember);

    jsval   GetName() const {return mName;}

    PRUint16 GetIndex() const {return mIndex;}

    JSBool GetValue(XPCCallContext& ccx, XPCNativeInterface* iface, jsval* pval)
        {if(!IsResolved() && !Resolve(ccx, iface)) return JS_FALSE;
         *pval = mVal; return JS_TRUE;}

    JSBool IsMethod() const
        {return 0 != (mFlags & METHOD);}

    JSBool IsConstant() const
        {return 0 != (mFlags & CONSTANT);}

    JSBool IsAttribute() const
        {return 0 != (mFlags & GETTER);}

    JSBool IsWritableAttribute() const
        {return 0 != (mFlags & SETTER_TOO);}

    JSBool IsReadOnlyAttribute() const
        {return IsAttribute() && !IsWritableAttribute();}


    void SetName(jsval a) {mName = a;}

    void SetMethod(PRUint16 index)
        {mVal = JSVAL_NULL; mFlags = METHOD; mIndex = index;}

    void SetConstant(PRUint16 index)
        {mVal = JSVAL_NULL; mFlags = CONSTANT; mIndex = index;}

    void SetReadOnlyAttribute(PRUint16 index)
        {mVal = JSVAL_NULL; mFlags = GETTER; mIndex = index;}

    void SetWritableAttribute()
        {NS_ASSERTION(mFlags == GETTER,"bad"); mFlags = GETTER | SETTER_TOO;}

    /* default ctor - leave random contents */
    XPCNativeMember()  {MOZ_COUNT_CTOR(XPCNativeMember);}
    ~XPCNativeMember() {MOZ_COUNT_DTOR(XPCNativeMember);}

    void DealWithDyingGCThings(JSContext* cx, XPCJSRuntime* rt)
        {if(IsResolved() && JSVAL_IS_GCTHING(mVal) &&
           JS_IsAboutToBeFinalized(cx, JSVAL_TO_GCTHING(mVal)))
           {mVal = JSVAL_NULL; mFlags &= ~RESOLVED;}}

private:
    JSBool IsResolved() const {return mFlags & RESOLVED;}
    JSBool Resolve(XPCCallContext& ccx, XPCNativeInterface* iface);

    enum {
        RESOLVED    = 0x01,
        METHOD      = 0x02,
        CONSTANT    = 0x04,
        GETTER      = 0x08,
        SETTER_TOO  = 0x10
    };

private:
    // our only data...
    jsval    mName;
    jsval    mVal;
    PRUint16 mIndex;
    PRUint16 mFlags;
};

/***************************************************************************/
// XPCNativeInterface represents a single idl declared interface. This is
// primarily the set of XPCNativeMembers.

// Tight. No virtual methods.

class XPCNativeInterface
{
public:
    static XPCNativeInterface* GetNewOrUsed(XPCCallContext& ccx,
                                            const nsIID* iid);
    static XPCNativeInterface* GetNewOrUsed(XPCCallContext& ccx,
                                            nsIInterfaceInfo* info);
    static XPCNativeInterface* GetNewOrUsed(XPCCallContext& ccx,
                                            const char* name);
    static XPCNativeInterface* GetISupports(XPCCallContext& ccx);

    inline nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo.get();}
    inline jsval             GetName()          const {return mName;}

    inline const nsIID* GetIID() const;
    inline const char*  GetNameString() const;
    inline XPCNativeMember* FindMember(jsval name) const;

    inline JSBool HasAncestor(const nsIID* iid) const;

    const char* GetMemberName(XPCCallContext& ccx,
                              const XPCNativeMember* member) const;

    PRUint16 GetMemberCount() const
        {NS_ASSERTION(!IsMarked(), "bad"); return mMemberCount;}
    XPCNativeMember* GetMemberAt(PRUint16 i)
        {NS_ASSERTION(i < mMemberCount, "bad index"); return &mMembers[i];}

    inline void DealWithDyingGCThings(JSContext* cx, XPCJSRuntime* rt);

    void DebugDump(PRInt16 depth);

#define XPC_NATIVE_IFACE_MARK_FLAG ((PRUint16)JS_BIT(15)) // only high bit of 16 is set

    void Mark()     {mMemberCount |= XPC_NATIVE_IFACE_MARK_FLAG;}
    void Unmark()   {mMemberCount &= ~XPC_NATIVE_IFACE_MARK_FLAG;}
    JSBool IsMarked() const
                    {return 0 != (mMemberCount & XPC_NATIVE_IFACE_MARK_FLAG);}

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void MarkBeforeJSFinalize(JSContext*) {}
    inline void AutoMark(JSContext*) {}

    static void DestroyInstance(JSContext* cx, XPCJSRuntime* rt,
                                XPCNativeInterface* inst);

protected:
    static XPCNativeInterface* NewInstance(XPCCallContext& ccx,
                                           nsIInterfaceInfo* aInfo);

    XPCNativeInterface();   // not implemented
    XPCNativeInterface(nsIInterfaceInfo* aInfo, jsval aName)
        : mInfo(aInfo), mName(aName), mMemberCount(0)
                          {MOZ_COUNT_CTOR(XPCNativeInterface);}
    ~XPCNativeInterface() {MOZ_COUNT_DTOR(XPCNativeInterface);}

    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

    XPCNativeInterface(const XPCNativeInterface& r); // not implemented
    XPCNativeInterface& operator= (const XPCNativeInterface& r); // not implemented

private:
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    jsval                      mName;
    PRUint16          mMemberCount;
    XPCNativeMember   mMembers[1]; // always last - object sized for array
};

/***************************************************************************/
// XPCNativeSetKey is used to key a XPCNativeSet in a NativeSetMap.

class XPCNativeSetKey
{
public:
    XPCNativeSetKey(XPCNativeSet*       BaseSet  = nsnull,
                    XPCNativeInterface* Addition = nsnull,
                    PRUint16            Position = 0)
        : mIsAKey(IS_A_KEY), mPosition(Position), mBaseSet(BaseSet),
          mAddition(Addition) {}
    ~XPCNativeSetKey() {}

    XPCNativeSet*           GetBaseSet()  const {return mBaseSet;}
    XPCNativeInterface*     GetAddition() const {return mAddition;}
    PRUint16                GetPosition() const {return mPosition;}

    // This is a fun little hack...
    // We build these keys only on the stack. We use them for lookup in
    // NativeSetMap. Becasue we don't want to pay the cost of cloning a key and
    // sticking it into the hashtable, when the XPCNativeSet actually
    // gets added to the table the 'key' in the table is a pointer to the
    // set itself and not this key. Our key compare function expects to get
    // a key and a set. When we do external lookups in the map we pass in one
    // of these keys and our compare function gets passed a key and a set.
    // (see compare_NativeKeyToSet in xpcmaps.cpp). This is all well and good.
    // Except, when the table decides to resize itself. Then it tries to use
    // our compare function with the 'keys' that are in the hashtable (which are
    // really XPCNativeSet objects and not XPCNativeSetKey objects!
    //
    // So, the hack is to have the compare function assume it is getting a
    // XPCNativeSetKey pointer and call this IsAKey method. If that fails then
    // it realises that it really has a XPCNativeSet pointer and deals with that
    // fact. This is safe because we know that both of these classes have no
    // virtual methods and their first data member is a PRUint16. We are
    // confident that XPCNativeSet->mMemberCount will never be 0xffff.

    JSBool                  IsAKey() const {return mIsAKey == IS_A_KEY;}

    enum {IS_A_KEY = 0xffff};

    // Allow shallow copy

private:
    PRUint16                mIsAKey;    // must be first data member
    PRUint16                mPosition;
    XPCNativeSet*           mBaseSet;
    XPCNativeInterface*     mAddition;
};

/***************************************************************************/
// XPCNativeSet represents an ordered collection of XPCNativeInterface pointers.

class XPCNativeSet
{
public:
    static XPCNativeSet* GetNewOrUsed(XPCCallContext& ccx, const nsIID* iid);
    static XPCNativeSet* GetNewOrUsed(XPCCallContext& ccx,
                                      nsIClassInfo* classInfo);
    static XPCNativeSet* GetNewOrUsed(XPCCallContext& ccx,
                                      XPCNativeSet* otherSet,
                                      XPCNativeInterface* newInterface,
                                      PRUint16 position);

    static void ClearCacheEntryForClassInfo(nsIClassInfo* classInfo);

    inline JSBool FindMember(jsval name, XPCNativeMember** pMember,
                             PRUint16* pInterfaceIndex) const;

    inline JSBool FindMember(jsval name, XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface) const;

    inline JSBool FindMember(jsval name,
                             XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface,
                             XPCNativeSet* protoSet,
                             JSBool* pIsLocal) const;

    inline JSBool HasInterface(XPCNativeInterface* aInterface) const;
    inline JSBool HasInterfaceWithAncestor(XPCNativeInterface* aInterface) const;

    inline XPCNativeInterface* FindInterfaceWithIID(const nsIID& iid) const;

    inline XPCNativeInterface* FindNamedInterface(jsval name) const;

    PRUint16 GetMemberCount() const {return mMemberCount;}
    PRUint16 GetInterfaceCount() const
        {NS_ASSERTION(!IsMarked(), "bad"); return mInterfaceCount;}
    XPCNativeInterface** GetInterfaceArray() {return mInterfaces;}

    XPCNativeInterface* GetInterfaceAt(PRUint16 i)
        {NS_ASSERTION(i < mInterfaceCount, "bad index"); return mInterfaces[i];}

    inline JSBool MatchesSetUpToInterface(const XPCNativeSet* other,
                                          XPCNativeInterface* iface) const;

#define XPC_NATIVE_SET_MARK_FLAG ((PRUint16)JS_BIT(15)) // only high bit of 16 is set

    inline void Mark();

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void MarkBeforeJSFinalize(JSContext*) {}
    inline void AutoMark(JSContext*) {}

private:
    void MarkSelfOnly() {mInterfaceCount |= XPC_NATIVE_SET_MARK_FLAG;}
public:
    void Unmark()       {mInterfaceCount &= ~XPC_NATIVE_SET_MARK_FLAG;}
    JSBool IsMarked() const
                  {return 0 != (mInterfaceCount & XPC_NATIVE_SET_MARK_FLAG);}

#ifdef DEBUG
    inline void ASSERT_NotMarked();
#endif

    void DebugDump(PRInt16 depth);

    static void DestroyInstance(XPCNativeSet* inst);

protected:
    static XPCNativeSet* NewInstance(XPCCallContext& ccx,
                                     XPCNativeInterface** array,
                                     PRUint16 count);
    static XPCNativeSet* NewInstanceMutate(XPCNativeSet*       otherSet,
                                           XPCNativeInterface* newInterface,
                                           PRUint16            position);
    XPCNativeSet()  {MOZ_COUNT_CTOR(XPCNativeSet);}
    ~XPCNativeSet() {MOZ_COUNT_DTOR(XPCNativeSet);}
    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

private:
    PRUint16                mMemberCount;
    PRUint16                mInterfaceCount;
    XPCNativeInterface*     mInterfaces[1];  // always last - object sized for array
};

/***************************************************************************/
// XPCNativeScriptableFlags is a wrapper class that holds the flags returned
// from calls to nsIXPCScriptable::GetScriptableFlags(). It has convenience
// methods to check for particular bitflags. Since we also use this class as
// a member of the gc'd class XPCNativeScriptableShared, this class holds the
// bit and exposes the inlined methods to support marking.

#define XPC_WN_SJSFLAGS_MARK_FLAG JS_BIT(31) // only high bit of 32 is set

class XPCNativeScriptableFlags
{
private:
    JSUint32 mFlags;

public:

    XPCNativeScriptableFlags(JSUint32 flags = 0) : mFlags(flags) {}

    JSUint32 GetFlags() const {return mFlags & ~XPC_WN_SJSFLAGS_MARK_FLAG;}
    void     SetFlags(JSUint32 flags) {mFlags = flags;}

    operator JSUint32() const {return GetFlags();}

    XPCNativeScriptableFlags(const XPCNativeScriptableFlags& r)
        {mFlags = r.GetFlags();}

    XPCNativeScriptableFlags& operator= (const XPCNativeScriptableFlags& r)
        {mFlags = r.GetFlags(); return *this;}

    void Mark()       {mFlags |= XPC_WN_SJSFLAGS_MARK_FLAG;}
    void Unmark()     {mFlags &= ~XPC_WN_SJSFLAGS_MARK_FLAG;}
    JSBool IsMarked() const {return 0 != (mFlags & XPC_WN_SJSFLAGS_MARK_FLAG);}

#ifdef GET_IT
#undef GET_IT
#endif
#define GET_IT(f_) const {return 0 != (mFlags & nsIXPCScriptable:: f_ );}

    JSBool WantPreCreate()                GET_IT(WANT_PRECREATE)
    JSBool WantCreate()                   GET_IT(WANT_CREATE)
    JSBool WantPostCreate()               GET_IT(WANT_POSTCREATE)
    JSBool WantAddProperty()              GET_IT(WANT_ADDPROPERTY)
    JSBool WantDelProperty()              GET_IT(WANT_DELPROPERTY)
    JSBool WantGetProperty()              GET_IT(WANT_GETPROPERTY)
    JSBool WantSetProperty()              GET_IT(WANT_SETPROPERTY)
    JSBool WantEnumerate()                GET_IT(WANT_ENUMERATE)
    JSBool WantNewEnumerate()             GET_IT(WANT_NEWENUMERATE)
    JSBool WantNewResolve()               GET_IT(WANT_NEWRESOLVE)
    JSBool WantConvert()                  GET_IT(WANT_CONVERT)
    JSBool WantFinalize()                 GET_IT(WANT_FINALIZE)
    JSBool WantCheckAccess()              GET_IT(WANT_CHECKACCESS)
    JSBool WantCall()                     GET_IT(WANT_CALL)
    JSBool WantConstruct()                GET_IT(WANT_CONSTRUCT)
    JSBool WantHasInstance()              GET_IT(WANT_HASINSTANCE)
    JSBool WantMark()                     GET_IT(WANT_MARK)
    JSBool WantEquality()                 GET_IT(WANT_EQUALITY)
    JSBool WantOuterObject()              GET_IT(WANT_OUTER_OBJECT)
    JSBool WantInnerObject()              GET_IT(WANT_INNER_OBJECT)
    JSBool UseJSStubForAddProperty()      GET_IT(USE_JSSTUB_FOR_ADDPROPERTY)
    JSBool UseJSStubForDelProperty()      GET_IT(USE_JSSTUB_FOR_DELPROPERTY)
    JSBool UseJSStubForSetProperty()      GET_IT(USE_JSSTUB_FOR_SETPROPERTY)
    JSBool DontEnumStaticProps()          GET_IT(DONT_ENUM_STATIC_PROPS)
    JSBool DontEnumQueryInterface()       GET_IT(DONT_ENUM_QUERY_INTERFACE)
    JSBool DontAskInstanceForScriptable() GET_IT(DONT_ASK_INSTANCE_FOR_SCRIPTABLE)
    JSBool ClassInfoInterfacesOnly()      GET_IT(CLASSINFO_INTERFACES_ONLY)
    JSBool AllowPropModsDuringResolve()   GET_IT(ALLOW_PROP_MODS_DURING_RESOLVE)
    JSBool AllowPropModsToPrototype()     GET_IT(ALLOW_PROP_MODS_TO_PROTOTYPE)
    JSBool DontSharePrototype()           GET_IT(DONT_SHARE_PROTOTYPE)
    JSBool DontReflectInterfaceNames()    GET_IT(DONT_REFLECT_INTERFACE_NAMES)

#undef GET_IT
};

/***************************************************************************/

// XPCNativeScriptableShared is used to hold the JSClass and the
// associated scriptable flags for XPCWrappedNatives. These are shared across
// the runtime and are garbage collected by xpconnect. We *used* to just store
// this inside the XPCNativeScriptableInfo (usually owned by instances of
// XPCWrappedNativeProto. This had two problems... It was wasteful, and it
// was a big problem when wrappers are reparented to different scopes (and
// thus different protos (the DOM does this).

class XPCNativeScriptableShared
{
public:
    const XPCNativeScriptableFlags& GetFlags() const {return mFlags;}
    JSClass*                        GetJSClass() {return &mJSClass.base;}

    XPCNativeScriptableShared(JSUint32 aFlags = 0, char* aName = nsnull)
        : mFlags(aFlags)
        {memset(&mJSClass, 0, sizeof(mJSClass));
         mJSClass.base.name = aName;  // take ownership
         MOZ_COUNT_CTOR(XPCNativeScriptableShared);}

    ~XPCNativeScriptableShared()
        {if(mJSClass.base.name)nsMemory::Free((void*)mJSClass.base.name);
         MOZ_COUNT_DTOR(XPCNativeScriptableShared);}

    char* TransferNameOwnership()
        {char* name=(char*)mJSClass.base.name; mJSClass.base.name = nsnull;
        return name;}

    void PopulateJSClass(JSBool isGlobal);

    void Mark()       {mFlags.Mark();}
    void Unmark()     {mFlags.Unmark();}
    JSBool IsMarked() const {return mFlags.IsMarked();}

private:
    XPCNativeScriptableFlags mFlags;
    JSExtendedClass          mJSClass;
};

/***************************************************************************/
// XPCNativeScriptableInfo is used to hold the nsIXPCScriptable state for a
// given class or instance.

class XPCNativeScriptableInfo
{
public:
    static XPCNativeScriptableInfo*
    Construct(XPCCallContext& ccx, JSBool isGlobal,
              const XPCNativeScriptableCreateInfo* sci);

    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mShared->GetFlags();}

    JSClass*
    GetJSClass()          {return mShared->GetJSClass();}

    XPCNativeScriptableShared*
    GetScriptableShared() {return mShared;}

    void
    SetCallback(nsIXPCScriptable* s) {mCallback = s;}

    void
    SetScriptableShared(XPCNativeScriptableShared* shared) {mShared = shared;}

    void Mark() {if(mShared) mShared->Mark();}

protected:
    XPCNativeScriptableInfo(nsIXPCScriptable* scriptable = nsnull,
                            XPCNativeScriptableShared* shared = nsnull)
        : mCallback(scriptable), mShared(shared)
                               {MOZ_COUNT_CTOR(XPCNativeScriptableInfo);}
public:
    ~XPCNativeScriptableInfo() {MOZ_COUNT_DTOR(XPCNativeScriptableInfo);}
private:

    // disable copy ctor and assignment
    XPCNativeScriptableInfo(const XPCNativeScriptableInfo& r); // not implemented
    XPCNativeScriptableInfo& operator= (const XPCNativeScriptableInfo& r); // not implemented

private:
    nsCOMPtr<nsIXPCScriptable>  mCallback;
    XPCNativeScriptableShared*  mShared;
};

/***************************************************************************/
// XPCNativeScriptableCreateInfo is used in creating new wrapper and protos.
// it abstracts out the scriptable interface pointer and the flags. After
// creation these are factored differently using XPCNativeScriptableInfo.

class XPCNativeScriptableCreateInfo
{
public:

    XPCNativeScriptableCreateInfo(const XPCNativeScriptableInfo& si)
        : mCallback(si.GetCallback()), mFlags(si.GetFlags()) {}

    XPCNativeScriptableCreateInfo(nsIXPCScriptable* callback = nsnull,
                                  XPCNativeScriptableFlags flags = 0)
        : mCallback(callback), mFlags(flags) {}

    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mFlags;}

    void
    SetCallback(nsIXPCScriptable* callback) {mCallback = callback;}

    void
    SetFlags(const XPCNativeScriptableFlags& flags)  {mFlags = flags;}

private:
    nsCOMPtr<nsIXPCScriptable>  mCallback;
    XPCNativeScriptableFlags    mFlags;
};

/***********************************************/
// XPCWrappedNativeProto hold the additional (potentially shared) wrapper data
// for XPCWrappedNative whose native objects expose nsIClassInfo.

class XPCWrappedNativeProto
{
public:
    static XPCWrappedNativeProto*
    GetNewOrUsed(XPCCallContext& ccx,
                 XPCWrappedNativeScope* Scope,
                 nsIClassInfo* ClassInfo,
                 const XPCNativeScriptableCreateInfo* ScriptableCreateInfo,
                 JSBool ForceNoSharing,
                 JSBool isGlobal);

    XPCWrappedNativeScope*
    GetScope()   const {return mScope;}

    XPCJSRuntime*
    GetRuntime() const {return mScope->GetRuntime();}

    JSObject*
    GetJSProtoObject() const {return mJSProtoObject;}

    nsIClassInfo*
    GetClassInfo()     const {return mClassInfo;}

    XPCNativeSet*
    GetSet()           const {return mSet;}

    XPCNativeScriptableInfo*
    GetScriptableInfo()   {return mScriptableInfo;}

    void**
    GetSecurityInfoAddr() {return &mSecurityInfo;}

    JSUint32
    GetClassInfoFlags() const {return mClassInfoFlags;}

#ifdef GET_IT
#undef GET_IT
#endif
#define GET_IT(f_) const {return (JSBool)(mClassInfoFlags & nsIClassInfo:: f_ );}

    JSBool ClassIsSingleton()           GET_IT(SINGLETON)
    JSBool ClassIsThreadSafe()          GET_IT(THREADSAFE)
    JSBool ClassIsMainThreadOnly()      GET_IT(MAIN_THREAD_ONLY)
    JSBool ClassIsDOMObject()           GET_IT(DOM_OBJECT)
    JSBool ClassIsPluginObject()        GET_IT(PLUGIN_OBJECT)

#undef GET_IT

#define XPC_PROTO_DONT_SHARE JS_BIT(31) // only high bit of 32 is set

    JSBool
    IsShared() const {return !(mClassInfoFlags & XPC_PROTO_DONT_SHARE);}

    XPCLock* GetLock() const
        {return ClassIsThreadSafe() ? GetRuntime()->GetMapLock() : nsnull;}

    void SetScriptableInfo(XPCNativeScriptableInfo* si)
        {NS_ASSERTION(!mScriptableInfo, "leak here!"); mScriptableInfo = si;}

    void JSProtoObjectFinalized(JSContext *cx, JSObject *obj);

    void SystemIsBeingShutDown(XPCCallContext& ccx);

    void DebugDump(PRInt16 depth);

    // This is called in the 'early' phase by AutoMarkingWrappedNativeProtoPtr.
    // 'early' meaning after JSGC_MARK_END and before JSGC_FINALIZE_END.
    // At this point in time we can still mark JSObjects in the JS gc heap.
    void MarkBeforeJSFinalize(JSContext* cx)
        {if(mJSProtoObject)
            JS_MarkGCThing(cx, mJSProtoObject, 
                           "XPCWrappedNativeProto::mJSProtoObject", nsnull);
         if(mScriptableInfo) mScriptableInfo->Mark();}
    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void AutoMark(JSContext*) {}

    // Yes, we *do* need to mark the mScriptableInfo in both cases.
    void Mark() const
        {mSet->Mark(); 
         if(mScriptableInfo) mScriptableInfo->Mark();}

#ifdef DEBUG
    void ASSERT_SetNotMarked() const {mSet->ASSERT_NotMarked();}
#endif

    ~XPCWrappedNativeProto();

protected:
    // disable copy ctor and assignment
    XPCWrappedNativeProto(const XPCWrappedNativeProto& r); // not implemented
    XPCWrappedNativeProto& operator= (const XPCWrappedNativeProto& r); // not implemented

    // hide ctor
    XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                          nsIClassInfo* ClassInfo,
                          PRUint32 ClassInfoFlags,
                          XPCNativeSet* Set);

    JSBool Init(XPCCallContext& ccx, JSBool isGlobal,
                const XPCNativeScriptableCreateInfo* scriptableCreateInfo);

private:
#if defined(DEBUG_xpc_hacker) || defined(DEBUG)
    static PRInt32 gDEBUG_LiveProtoCount;
#endif

private:
    XPCWrappedNativeScope*   mScope;
    JSObject*                mJSProtoObject;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    PRUint32                 mClassInfoFlags;
    XPCNativeSet*            mSet;
    void*                    mSecurityInfo;
    XPCNativeScriptableInfo* mScriptableInfo;
};


/***********************************************/
// XPCWrappedNativeTearOff represents the info needed to make calls to one
// interface on the underlying native object of a XPCWrappedNative.

class XPCWrappedNativeTearOff
{
public:
    JSBool IsAvailable() const {return mInterface == nsnull;}
    JSBool IsReserved()  const {return mInterface == (XPCNativeInterface*)1;}
    JSBool IsValid()     const {return !IsAvailable() && !IsReserved();}
    void   SetReserved()       {mInterface = (XPCNativeInterface*)1;}

    XPCNativeInterface* GetInterface() const {return mInterface;}
    nsISupports*        GetNative()    const {return mNative;}
    JSObject*           GetJSObject()  const;
    void SetInterface(XPCNativeInterface*  Interface) {mInterface = Interface;}
    void SetNative(nsISupports*  Native)              {mNative = Native;}
    void SetJSObject(JSObject*  JSObj);

    void JSObjectFinalized() {SetJSObject(nsnull);}

    XPCWrappedNativeTearOff()
        : mInterface(nsnull), mNative(nsnull), mJSObject(nsnull) {}
    ~XPCWrappedNativeTearOff();

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void MarkBeforeJSFinalize(JSContext*) {}
    inline void AutoMark(JSContext*) {}

    void Mark()       {mJSObject = (JSObject*)(((jsword)mJSObject) | 1);}
    void Unmark()     {mJSObject = (JSObject*)(((jsword)mJSObject) & ~1);}
    JSBool IsMarked() const {return (JSBool)(((jsword)mJSObject) & 1);}

#ifdef XPC_IDISPATCH_SUPPORT
    enum JSObject_flags
    {
        IDISPATCH_BIT = 2,
        JSOBJECT_MASK = 3
    };
    void                SetIDispatch(JSContext* cx);
    JSBool              IsIDispatch() const;
    XPCDispInterface*   GetIDispatchInfo() const;
#endif
private:
    XPCWrappedNativeTearOff(const XPCWrappedNativeTearOff& r); // not implemented
    XPCWrappedNativeTearOff& operator= (const XPCWrappedNativeTearOff& r); // not implemented

private:
    XPCNativeInterface* mInterface;
    nsISupports*        mNative;
    JSObject*           mJSObject;
};

/***********************************************/
// XPCWrappedNativeTearOffChunk is a collections of XPCWrappedNativeTearOff
// objects. It lets us allocate a set of XPCWrappedNativeTearOff objects and
// link the sets - rather than only having the option of linking single
// XPCWrappedNativeTearOff objects.
//
// The value of XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK can be tuned at buildtime
// to balance between the code of allocations of additional chunks and the waste
// of space for ununsed XPCWrappedNativeTearOff objects.

#define XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK 1

class XPCWrappedNativeTearOffChunk
{
friend class XPCWrappedNative;
private:
    XPCWrappedNativeTearOffChunk() : mNextChunk(nsnull) {}
    ~XPCWrappedNativeTearOffChunk() {delete mNextChunk;}

private:
    XPCWrappedNativeTearOff mTearOffs[XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK];
    XPCWrappedNativeTearOffChunk* mNextChunk;
};

/***************************************************************************/
// XPCWrappedNative the wrapper around one instance of a native xpcom object
// to be used from JavaScript.

class XPCWrappedNative : public nsIXPConnectWrappedNative
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDNATIVE
    NS_DECL_CYCLE_COLLECTION_CLASS(XPCWrappedNative)

#ifndef XPCONNECT_STANDALONE
    virtual nsIPrincipal* GetObjectPrincipal() const;
#endif

    JSBool
    IsValid() const {return nsnull != mFlatJSObject;}

#define XPC_SCOPE_TAG     ((jsword)0x1)
#define XPC_SCOPE_WORD(s) ((jsword)(s))

    static inline JSBool
    IsTaggedScope(XPCWrappedNativeScope* s)
        {return XPC_SCOPE_WORD(s) & XPC_SCOPE_TAG;}

    static inline XPCWrappedNativeScope*
    TagScope(XPCWrappedNativeScope* s)
        {NS_ASSERTION(!IsTaggedScope(s), "bad pointer!");
         return (XPCWrappedNativeScope*)(XPC_SCOPE_WORD(s) | XPC_SCOPE_TAG);}

    static inline XPCWrappedNativeScope*
    UnTagScope(XPCWrappedNativeScope* s)
        {return (XPCWrappedNativeScope*)(XPC_SCOPE_WORD(s) & ~XPC_SCOPE_TAG);}

    JSBool
    HasProto() const {return !IsTaggedScope(mMaybeScope);}

    XPCWrappedNativeProto*
    GetProto() const {return HasProto() ? mMaybeProto : nsnull;}

    XPCWrappedNativeScope*
    GetScope() const {return HasProto() ?
                           mMaybeProto->GetScope() : UnTagScope(mMaybeScope);}

    nsISupports*
    GetIdentityObject() const {return mIdentity;}

    JSObject*
    GetFlatJSObject()   const {return mFlatJSObject;}

    XPCLock*
    GetLock() const {return IsValid() && HasProto() ?
                                mMaybeProto->GetLock() : nsnull;}

    XPCNativeSet*
    GetSet() const {XPCAutoLock al(GetLock()); return mSet;}

private:
    void
    SetSet(XPCNativeSet* set) {XPCAutoLock al(GetLock()); mSet = set;}
public:

    XPCNativeScriptableInfo*
    GetScriptableInfo() const {return mScriptableInfo;}

    nsIXPCScriptable*      // call this wrong and you deserve to crash
    GetScriptableCallback() const  {return mScriptableInfo->GetCallback();}

    void**
    GetSecurityInfoAddr() {return HasProto() ?
                                   mMaybeProto->GetSecurityInfoAddr() : nsnull;}

    nsIClassInfo*
    GetClassInfo() const {return IsValid() && HasProto() ?
                            mMaybeProto->GetClassInfo() : nsnull;}

    JSBool
    HasSharedProto() const {return IsValid() && HasProto() &&
                            mMaybeProto->IsShared();}

    JSBool
    HasMutatedSet() const {return IsValid() &&
                                  (!HasProto() ||
                                   GetSet() != mMaybeProto->GetSet());}

    XPCJSRuntime*
    GetRuntime() const {XPCWrappedNativeScope* scope = GetScope();
                        return scope ? scope->GetRuntime() : nsnull;}

    static nsresult
    GetNewOrUsed(XPCCallContext& ccx,
                 nsISupports* Object,
                 XPCWrappedNativeScope* Scope,
                 XPCNativeInterface* Interface,
                 JSBool isGlobal,
                 XPCWrappedNative** wrapper);

public:
    static nsresult
    GetUsedOnly(XPCCallContext& ccx,
                nsISupports* Object,
                XPCWrappedNativeScope* Scope,
                XPCNativeInterface* Interface,
                XPCWrappedNative** wrapper);

    static XPCWrappedNative*
    GetWrappedNativeOfJSObject(JSContext* cx, JSObject* obj,
                               JSObject* funobj = nsnull,
                               JSObject** pobj2 = nsnull,
                               XPCWrappedNativeTearOff** pTearOff = nsnull);

    static nsresult
    ReparentWrapperIfFound(XPCCallContext& ccx,
                           XPCWrappedNativeScope* aOldScope,
                           XPCWrappedNativeScope* aNewScope,
                           JSObject* aNewParent,
                           nsISupports* aCOMObj,
                           XPCWrappedNative** aWrapper);

    void FlatJSObjectFinalized(JSContext *cx, JSObject *obj);

    void SystemIsBeingShutDown(XPCCallContext& ccx);

#ifdef XPC_DETECT_LEADING_UPPERCASE_ACCESS_ERRORS
    // This will try to find a member that is of the form "camelCased"
    // but was accessed from JS using "CamelCased". This is here to catch
    // mistakes caused by the confusion magnet that JS methods are by
    // convention 'foo' while C++ members are by convention 'Foo'.
    static void
    HandlePossibleNameCaseError(XPCCallContext& ccx,
                                XPCNativeSet* set,
                                XPCNativeInterface* iface,
                                jsval name);
    static void
    HandlePossibleNameCaseError(JSContext* cx,
                                XPCNativeSet* set,
                                XPCNativeInterface* iface,
                                jsval name);

#define  HANDLE_POSSIBLE_NAME_CASE_ERROR(context, set, iface, name) \
    XPCWrappedNative::HandlePossibleNameCaseError(context, set, iface, name)
#else
#define  HANDLE_POSSIBLE_NAME_CASE_ERROR(context, set, iface, name) ((void)0)
#endif

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    static JSBool CallMethod(XPCCallContext& ccx,
                             CallMode mode = CALL_METHOD);

    static JSBool GetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_GETTER);}

    static JSBool SetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_SETTER);}

    inline JSBool HasInterfaceNoQI(XPCNativeInterface* aInterface);
    inline JSBool HasInterfaceNoQI(const nsIID& iid);

    XPCWrappedNativeTearOff* LocateTearOff(XPCCallContext& ccx,
                                           XPCNativeInterface* aInterface);
    XPCWrappedNativeTearOff* FindTearOff(XPCCallContext& ccx,
                                         XPCNativeInterface* aInterface,
                                         JSBool needJSObject = JS_FALSE,
                                         nsresult* pError = nsnull);
    void Mark() const
    {
        mSet->Mark();
        if(mScriptableInfo) mScriptableInfo->Mark();
        if(HasProto()) mMaybeProto->Mark();
    }

    // Yes, we *do* need to mark the mScriptableInfo in both cases.
    inline void MarkBeforeJSFinalize(JSContext* cx)
    {
        if(mScriptableInfo) mScriptableInfo->Mark();
        if(HasProto()) mMaybeProto->MarkBeforeJSFinalize(cx);
        if(mNativeWrapper)
        {
            JS_MarkGCThing(cx, mNativeWrapper, 
                           "XPCWrappedNative::mNativeWrapper", nsnull);
        }
    }

    inline void AutoMark(JSContext* cx)
    {
        // If this got called, we're being kept alive by someone who really
        // needs us alive and whole.  Do not let our mFlatJSObject go away.
        // This is the only time we should be marking our mFlatJSObject;
        // normally we just go away quietly when it does.  Be careful not to
        // mark the bogus JSVAL_ONE value we can have during init, though.
        if(mFlatJSObject && mFlatJSObject != (JSObject*)JSVAL_ONE)
        {
            ::JS_MarkGCThing(cx, mFlatJSObject,
                             "XPCWrappedNative::mFlatJSObject", nsnull);
        }
    }

#ifdef DEBUG
    void ASSERT_SetsNotMarked() const
        {mSet->ASSERT_NotMarked();
         if(HasProto()){mMaybeProto->ASSERT_SetNotMarked();}}

    int DEBUG_CountOfTearoffChunks() const
        {int i = 0; const XPCWrappedNativeTearOffChunk* to;
         for(to = &mFirstChunk; to; to = to->mNextChunk) {i++;} return i;}
#endif

    inline void SweepTearOffs();

    // Returns a string that shuld be free'd using JS_smprintf_free (or null).
    char* ToString(XPCCallContext& ccx,
                   XPCWrappedNativeTearOff* to = nsnull) const;

    static nsresult GatherProtoScriptableCreateInfo(
                        nsIClassInfo* classInfo,
                        XPCNativeScriptableCreateInfo* sciProto);

    JSBool HasExternalReference() const {return mRefCnt > 1;}

    JSObject* GetNativeWrapper()              { return mNativeWrapper; }
    void      SetNativeWrapper(JSObject *obj) { mNativeWrapper = obj; }

    // Make ctor and dtor protected (rather than private) to placate nsCOMPtr.
protected:
    XPCWrappedNative(); // not implemented

    // This ctor is used if this object will have a proto.
    XPCWrappedNative(nsISupports* aIdentity,
                     XPCWrappedNativeProto* aProto);

    // This ctor is used if this object will NOT have a proto.
    XPCWrappedNative(nsISupports* aIdentity,
                     XPCWrappedNativeScope* aScope,
                     XPCNativeSet* aSet);

    virtual ~XPCWrappedNative();

private:
    JSBool Init(XPCCallContext& ccx, JSObject* parent, JSBool isGlobal,
                const XPCNativeScriptableCreateInfo* sci);

    JSBool ExtendSet(XPCCallContext& ccx, XPCNativeInterface* aInterface);

    nsresult InitTearOff(XPCCallContext& ccx,
                         XPCWrappedNativeTearOff* aTearOff,
                         XPCNativeInterface* aInterface,
                         JSBool needJSObject);

    JSBool InitTearOffJSObject(XPCCallContext& ccx,
                                XPCWrappedNativeTearOff* to);

public:
    static nsresult GatherScriptableCreateInfo(
                        nsISupports* obj,
                        nsIClassInfo* classInfo,
                        XPCNativeScriptableCreateInfo* sciProto,
                        XPCNativeScriptableCreateInfo* sciWrapper);

private:
    union
    {
        XPCWrappedNativeScope*   mMaybeScope;
        XPCWrappedNativeProto*   mMaybeProto;
    };
    XPCNativeSet*                mSet;
    JSObject*                    mFlatJSObject;
    XPCNativeScriptableInfo*     mScriptableInfo;
    XPCWrappedNativeTearOffChunk mFirstChunk;
    JSObject*                    mNativeWrapper;

public:
    nsCOMPtr<nsIThread>          mThread; // Don't want to overload _mOwningThread
};

/***************************************************************************
****************************************************************************
*
* Core classes for wrapped JSObject for use from native code...
*
****************************************************************************
***************************************************************************/

// this interfaces exists so we can refcount nsXPCWrappedJSClass
// {2453EBA0-A9B8-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_CLASS_IID  \
{ 0x2453eba0, 0xa9b8, 0x11d2,               \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedJSClass : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)
    NS_IMETHOD DebugDump(PRInt16 depth) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPCWrappedJSClass,
                              NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)

/*************************/
// nsXPCWrappedJSClass represents the sharable factored out common code and
// data for nsXPCWrappedJS instances for the same interface type.

class nsXPCWrappedJSClass : public nsIXPCWrappedJSClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(PRInt16 depth);
public:

    static nsresult
    GetNewOrUsed(XPCCallContext& ccx,
                 REFNSIID aIID,
                 nsXPCWrappedJSClass** clazz);

    REFNSIID GetIID() const {return mIID;}
    XPCJSRuntime* GetRuntime() const {return mRuntime;}
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    const char* GetInterfaceName();

    static JSBool IsWrappedJS(nsISupports* aPtr);

    NS_IMETHOD DelegatedQueryInterface(nsXPCWrappedJS* self, REFNSIID aIID,
                                       void** aInstancePtr);

    JSObject* GetRootJSObject(XPCCallContext& ccx, JSObject* aJSObj);

    NS_IMETHOD CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                          const XPTMethodDescriptor* info,
                          nsXPTCMiniVariant* params);

    JSObject*  CallQueryInterfaceOnJSObject(XPCCallContext& ccx,
                                            JSObject* jsobj, REFNSIID aIID);

    static nsresult BuildPropertyEnumerator(XPCCallContext& ccx,
                                            JSObject* aJSObj,
                                            nsISimpleEnumerator** aEnumerate);

    static nsresult GetNamedPropertyAsVariant(XPCCallContext& ccx, 
                                              JSObject* aJSObj,
                                              jsval aName, 
                                              nsIVariant** aResult);

    virtual ~nsXPCWrappedJSClass();

    static nsresult CheckForException(XPCCallContext & ccx,
                                      const char * aPropertyName,
                                      const char * anInterfaceName);
private:
    nsXPCWrappedJSClass();   // not implemented
    nsXPCWrappedJSClass(XPCCallContext& ccx, REFNSIID aIID,
                        nsIInterfaceInfo* aInfo);

    JSObject*  NewOutObject(JSContext* cx);

    JSBool IsReflectable(uint16 i) const
        {return (JSBool)(mDescriptors[i/32] & (1 << (i%32)));}
    void SetReflectable(uint16 i, JSBool b)
        {if(b) mDescriptors[i/32] |= (1 << (i%32));
         else mDescriptors[i/32] &= ~(1 << (i%32));}

    enum SizeMode {GET_SIZE, GET_LENGTH};

    JSBool GetArraySizeFromParam(JSContext* cx,
                                 const XPTMethodDescriptor* method,
                                 const nsXPTParamInfo& param,
                                 uint16 methodIndex,
                                 uint8 paramIndex,
                                 SizeMode mode,
                                 nsXPTCMiniVariant* params,
                                 JSUint32* result);

    JSBool GetInterfaceTypeFromParam(JSContext* cx,
                                     const XPTMethodDescriptor* method,
                                     const nsXPTParamInfo& param,
                                     uint16 methodIndex,
                                     const nsXPTType& type,
                                     nsXPTCMiniVariant* params,
                                     nsID* result);

    void CleanupPointerArray(const nsXPTType& datum_type,
                             JSUint32 array_count,
                             void** arrayp);

    void CleanupPointerTypeObject(const nsXPTType& type,
                                  void** pp);

private:
    XPCJSRuntime* mRuntime;
    nsIInterfaceInfo* mInfo;
    char* mName;
    nsIID mIID;
    uint32* mDescriptors;
};

/*************************/
// nsXPCWrappedJS is a wrapper for a single JSObject for use from native code.
// nsXPCWrappedJS objects are chained together to represent the various
// interface on the single underlying (possibly aggregate) JSObject.

class nsXPCWrappedJS : protected nsAutoXPTCStub,
                       public nsIXPConnectWrappedJS,
                       public nsSupportsWeakReference,
                       public nsIPropertyBag
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDJS
    NS_DECL_NSISUPPORTSWEAKREFERENCE
    NS_DECL_NSIPROPERTYBAG
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXPCWrappedJS,
                                             nsIXPConnectWrappedJS)

    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const XPTMethodDescriptor *info,
                          nsXPTCMiniVariant* params);

    /*
    * This is rarely called directly. Instead one usually calls
    * XPCConvert::JSObject2NativeInterface which will handles cases where the
    * JS object is already a wrapped native or a DOM object.
    */

    static nsresult
    GetNewOrUsed(XPCCallContext& ccx,
                 JSObject* aJSObj,
                 REFNSIID aIID,
                 nsISupports* aOuter,
                 nsXPCWrappedJS** wrapper);

    nsISomeInterface* GetXPTCStub() { return mXPTCStub; }
    JSObject* GetJSObject() const {return mJSObj;}
    nsXPCWrappedJSClass*  GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}
    nsXPCWrappedJS* GetRootWrapper() const {return mRoot;}
    nsXPCWrappedJS* GetNextWrapper() const {return mNext;}

    nsXPCWrappedJS* Find(REFNSIID aIID);
    nsXPCWrappedJS* FindInherited(REFNSIID aIID);

    JSBool IsValid() const {return mJSObj != nsnull;}
    void SystemIsBeingShutDown(JSRuntime* rt);

    // This is used by XPCJSRuntime::GCCallback to find wrappers that no
    // longer root their JSObject and are only still alive because they
    // were being used via nsSupportsWeakReference at the time when their
    // last (outside) reference was released. Wrappers that fit into that
    // category are only deleted when we see that their corresponding JSObject
    // is to be finalized.
    JSBool IsSubjectToFinalization() const {return IsValid() && mRefCnt == 1;}

    JSBool IsAggregatedToNative() const {return mRoot->mOuter != nsnull;}
    nsISupports* GetAggregatedNativeObject() const {return mRoot->mOuter;}

    virtual ~nsXPCWrappedJS();
protected:
    nsXPCWrappedJS();   // not implemented
    nsXPCWrappedJS(XPCCallContext& ccx,
                   JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root,
                   nsISupports* aOuter);

private:
    JSObject* mJSObj;
    nsXPCWrappedJSClass* mClass;
    nsXPCWrappedJS* mRoot;
    nsXPCWrappedJS* mNext;
    nsISupports* mOuter;    // only set in root
#ifdef GC_MARK_DEBUG
    char *mGCRootName;
#endif
};

/***************************************************************************/

class XPCJSObjectHolder : public nsIXPConnectJSObjectHolder
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER

    // non-interface implementation

public:
    static XPCJSObjectHolder* newHolder(JSContext* cx, JSObject* obj);

    virtual ~XPCJSObjectHolder();

private:
    XPCJSObjectHolder(JSContext* cx, JSObject* obj);
    XPCJSObjectHolder(); // not implemented

    JSRuntime* mRuntime;
    JSObject* mJSObj;
};

/***************************************************************************
****************************************************************************
*
* All manner of utility classes follow...
*
****************************************************************************
***************************************************************************/

class xpcProperty : public nsIProperty
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROPERTY

  xpcProperty(const PRUnichar* aName, PRUint32 aNameLen, nsIVariant* aValue);
  virtual ~xpcProperty() {}

private:
    nsString             mName;
    nsCOMPtr<nsIVariant> mValue;
};

class xpcPropertyBagEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    xpcPropertyBagEnumerator(PRUint32 count);
    virtual ~xpcPropertyBagEnumerator() {}

    JSBool AppendElement(nsISupports* element);

private:
    nsSupportsArray mArray;
    PRUint32        mIndex;
    PRUint32        mCount;
};

/***************************************************************************/
// data conversion

// class here just for static methods
class XPCConvert
{
public:
    static JSBool IsMethodReflectable(const XPTMethodDescriptor& info);

    /**
     * Convert a native object into a jsval.
     *
     * @param ccx the context for the whole procedure
     * @param d [out] the resulting jsval
     * @param s the native object we're working with
     * @param type the type of object that s is
     * @param iid the interface of s that we want
     * @param scope the default scope to put on the new JSObject's __parent__
     *        chain
     * @param pErr [out] relevant error code, if any.
     */    
    static JSBool NativeData2JS(XPCCallContext& ccx, jsval* d, const void* s,
                                const nsXPTType& type, const nsID* iid,
                                JSObject* scope, nsresult* pErr);

    static JSBool JSData2Native(XPCCallContext& ccx, void* d, jsval s,
                                const nsXPTType& type,
                                JSBool useAllocator, const nsID* iid,
                                nsresult* pErr);

    /**
     * Convert a native nsISupports into a JSObject.
     *
     * @param ccx the context for the whole procedure
     * @param dest [out] the resulting JSObject
     * @param src the native object we're working with
     * @param iid the interface of src that we want
     * @param scope the default scope to put on the new JSObject's __parent__
     *        chain
     * @param allowNativeWrapper if true, this method may wrap the resulting
     *        JSObject in an XPCNativeWrapper and return that, as needed.
     * @param pErr [out] relevant error code, if any.
     */
    static JSBool NativeInterface2JSObject(XPCCallContext& ccx,
                                           nsIXPConnectJSObjectHolder** dest,
                                           nsISupports* src,
                                           const nsID* iid,
                                           JSObject* scope,
                                           PRBool allowNativeWrapper,
                                           PRBool isGlobal,
                                           nsresult* pErr);

    static JSBool GetNativeInterfaceFromJSObject(XPCCallContext& ccx,
                                                 void** dest, JSObject* src,
                                                 const nsID* iid, 
                                                 nsresult* pErr);
    static JSBool JSObject2NativeInterface(XPCCallContext& ccx,
                                           void** dest, JSObject* src,
                                           const nsID* iid,
                                           nsISupports* aOuter,
                                           nsresult* pErr);

    /**
     * Convert a native array into a jsval.
     *
     * @param ccx the context for the whole procedure
     * @param d [out] the resulting jsval
     * @param s the native array we're working with
     * @param type the type of objects in the array
     * @param iid the interface of each object in the array that we want
     * @param count the number of items in the array
     * @param scope the default scope to put on the new JSObjects' __parent__
     *        chain
     * @param pErr [out] relevant error code, if any.
     */    
    static JSBool NativeArray2JS(XPCCallContext& ccx,
                                 jsval* d, const void** s,
                                 const nsXPTType& type, const nsID* iid,
                                 JSUint32 count, JSObject* scope,
                                 nsresult* pErr);

    static JSBool JSArray2Native(XPCCallContext& ccx, void** d, jsval s,
                                 JSUint32 count, JSUint32 capacity,
                                 const nsXPTType& type,
                                 JSBool useAllocator, const nsID* iid,
                                 uintN* pErr);

    static JSBool NativeStringWithSize2JS(XPCCallContext& ccx,
                                          jsval* d, const void* s,
                                          const nsXPTType& type,
                                          JSUint32 count,
                                          nsresult* pErr);

    static JSBool JSStringWithSize2Native(XPCCallContext& ccx, void* d, jsval s,
                                          JSUint32 count, JSUint32 capacity,
                                          const nsXPTType& type,
                                          JSBool useAllocator,
                                          uintN* pErr);

    static nsresult JSValToXPCException(XPCCallContext& ccx,
                                        jsval s,
                                        const char* ifaceName,
                                        const char* methodName,
                                        nsIException** exception);

    static nsresult JSErrorToXPCException(XPCCallContext& ccx,
                                          const char* message,
                                          const char* ifaceName,
                                          const char* methodName,
                                          const JSErrorReport* report,
                                          nsIException** exception);

    static nsresult ConstructException(nsresult rv, const char* message,
                                       const char* ifaceName,
                                       const char* methodName,
                                       nsISupports* data,                                       
                                       nsIException** exception);

    static void RemoveXPCOMUCStringFinalizer();

private:
    XPCConvert(); // not implemented

};

/***************************************************************************/

// class to export a JSString as an const nsAString, no refcounting :(
class XPCReadableJSStringWrapper : public nsDependentString
{
public:
    typedef nsDependentString::char_traits char_traits;

    XPCReadableJSStringWrapper(PRUnichar *chars, size_t length) :
        nsDependentString(chars, length)
    { }

    XPCReadableJSStringWrapper() :
        nsDependentString(char_traits::sEmptyBuffer, char_traits::sEmptyBuffer)
    { SetIsVoid(PR_TRUE); }
};

// readable string conversions, static methods only
class XPCStringConvert
{
public:

    static JSString *ReadableToJSString(JSContext *cx,
                                        const nsAString &readable);

    static XPCReadableJSStringWrapper *JSStringToReadable(JSString *str);

    static void ShutdownDOMStringFinalizer();

private:
    XPCStringConvert();         // not implemented
};

extern JSBool JS_DLL_CALLBACK
XPC_JSArgumentFormatter(JSContext *cx, const char *format,
                        JSBool fromJS, jsval **vpp, va_list *app);


/***************************************************************************/
// code for throwing exceptions into JS

class XPCThrower
{
public:
    static void Throw(nsresult rv, JSContext* cx);
    static void Throw(nsresult rv, XPCCallContext& ccx);
    static void ThrowBadResult(nsresult rv, nsresult result, XPCCallContext& ccx);
    static void ThrowBadParam(nsresult rv, uintN paramNum, XPCCallContext& ccx);
#ifdef XPC_IDISPATCH_SUPPORT
    static void ThrowCOMError(JSContext* cx, unsigned long COMErrorCode, 
                              nsresult rv = NS_ERROR_XPC_COM_ERROR,
                              const EXCEPINFO * exception = nsnull);
#endif
    static JSBool SetVerbosity(JSBool state)
        {JSBool old = sVerbose; sVerbose = state; return old;}

private:
    static void Verbosify(XPCCallContext& ccx,
                          char** psz, PRBool own);

    static void BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz);
    static JSBool ThrowExceptionObject(JSContext* cx, nsIException* e);
    static JSBool CheckForPendingException(nsresult result, XPCCallContext &ccx);

private:
    static JSBool sVerbose;
};


/***************************************************************************/

class XPCJSStack
{
public:
    static nsresult
    CreateStack(JSContext* cx, nsIStackFrame** stack);

    static nsresult
    CreateStackFrameLocation(PRUint32 aLanguage,
                             const char* aFilename,
                             const char* aFunctionName,
                             PRInt32 aLineNumber,
                             nsIStackFrame* aCaller,
                             nsIStackFrame** stack);
private:
    XPCJSStack();   // not implemented
};

/***************************************************************************/

class nsXPCException :
            public nsIXPCException
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCEXCEPTION_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXCEPTION
    NS_DECL_NSIXPCEXCEPTION
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

    static nsresult NewException(const char *aMessage,
                                 nsresult aResult,
                                 nsIStackFrame *aLocation,
                                 nsISupports *aData,
                                 nsIException** exception);

    static JSBool NameAndFormatForNSResult(nsresult rv,
                                           const char** name,
                                           const char** format);

    static void* IterateNSResults(nsresult* rv,
                                  const char** name,
                                  const char** format,
                                  void** iterp);

    static PRUint32 GetNSResultCount();

    nsXPCException();
    virtual ~nsXPCException();

    static void InitStatics() { sEverMadeOneFromFactory = JS_FALSE; }

protected:
    void Reset();
private:
    char*           mMessage;
    nsresult        mResult;
    char*           mName;
    nsIStackFrame*  mLocation;
    nsISupports*    mData;
    char*           mFilename;
    int             mLineNumber;
    nsIException*   mInner;
    PRBool          mInitialized;

    static JSBool sEverMadeOneFromFactory;
};

/***************************************************************************/
/*
* nsJSID implements nsIJSID. It is also used by nsJSIID and nsJSCID as a
* member (as a hidden implementaion detail) to which they delegate many calls.
*/

extern JSBool xpc_InitJSxIDClassObjects();
extern void xpc_DestroyJSxIDClassObjects();


class nsJSID : public nsIJSID
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_JS_ID_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSID

    PRBool InitWithName(const nsID& id, const char *nameString);
    PRBool SetName(const char* name);
    void   SetNameToNoString()
        {NS_ASSERTION(!mName, "name already set"); mName = gNoString;}
    PRBool NameIsSet() const {return nsnull != mName;}
    const nsID* GetID() const {return &mID;}

    PRBool IsValid() const {return !mID.Equals(GetInvalidIID());}

    static nsJSID* NewID(const char* str);

    nsJSID();
    virtual ~nsJSID();
protected:

    void Reset();
    const nsID& GetInvalidIID() const;

protected:
    static char gNoString[];
    nsID    mID;
    char*   mNumber;
    char*   mName;
};

// nsJSIID

class nsJSIID : public nsIJSIID, public nsIXPCScriptable
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    NS_DECL_ISUPPORTS

    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID
    NS_DECL_NSIXPCSCRIPTABLE
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

    static nsJSIID* NewID(nsIInterfaceInfo* aInfo);

    nsJSIID(nsIInterfaceInfo* aInfo);
    nsJSIID(); // not implemented
    virtual ~nsJSIID();

private:
    nsCOMPtr<nsIInterfaceInfo> mInfo;
};

// nsJSCID

class nsJSCID : public nsIJSCID, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS

    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSCID
    NS_DECL_NSIXPCSCRIPTABLE

    static nsJSCID* NewID(const char* str);

    nsJSCID();
    virtual ~nsJSCID();

private:
    void ResolveName();

private:
    nsJSID mDetails;
};


/***************************************************************************/
// XPCJSContextStack is not actually an xpcom object, but xpcom calls are
// delegated to it as an implementation detail.

class XPCJSContextStack
{
public:
    NS_DECL_NSIJSCONTEXTSTACK
    NS_DECL_NSITHREADJSCONTEXTSTACK

    XPCJSContextStack();
    virtual ~XPCJSContextStack();

#ifdef DEBUG
    JSBool DEBUG_StackHasJSContext(JSContext*  aJSContext);
#endif

    const nsDeque &GetStack()
    { return mStack; }

private:
    void SyncJSContexts();

private:
    nsDeque     mStack;
    JSContext*  mSafeJSContext;

    // If non-null, we own it; same as mSafeJSContext if SetSafeJSContext
    // not called.
    JSContext*  mOwnSafeJSContext;
};

/***************************************************************************/

#define NS_XPC_JSCONTEXT_STACK_ITERATOR_CID \
{ 0x05bae29d, 0x8aef, 0x486d, \
  { 0x84, 0xaa, 0x53, 0xf4, 0x8f, 0x14, 0x68, 0x11 } }

class nsXPCJSContextStackIterator : public nsIJSContextStackIterator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSCONTEXTSTACKITERATOR

private:
    // XXX These don't really want to be pointers.
    nsAutoPtr<nsDequeIterator> mIterator;
};

/**************************************************************/
// All of our thread local storage.

#define BAD_TLS_INDEX ((PRUint32) -1)

class XPCPerThreadData
{
public:
    // Get the instance of this object for the current thread
    static XPCPerThreadData* GetData();
    static void CleanupAllThreads();

    ~XPCPerThreadData();

    nsresult GetException(nsIException** aException)
    {
        if(EnsureExceptionManager())
            return mExceptionManager->GetCurrentException(aException);

        NS_IF_ADDREF(mException);
        *aException = mException;
        return NS_OK;
    }

    nsresult SetException(nsIException* aException)
    {
        if(EnsureExceptionManager())
            return mExceptionManager->SetCurrentException(aException);

        NS_IF_ADDREF(aException);
        NS_IF_RELEASE(mException);
        mException = aException;
        return NS_OK;
    }

    nsIExceptionManager* GetExceptionManager()
    {
        if(EnsureExceptionManager())
            return mExceptionManager;
        return nsnull;
    }

    JSBool EnsureExceptionManager()
    {
        if(mExceptionManager)
            return JS_TRUE;

        if(mExceptionManagerNotAvailable)
            return JS_FALSE;

        nsCOMPtr<nsIExceptionService> xs =
            do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
        if(xs)
            xs->GetCurrentExceptionManager(&mExceptionManager);
        if(mExceptionManager)
            return JS_TRUE;

        mExceptionManagerNotAvailable = JS_TRUE;
        return JS_FALSE;
    }

    XPCJSContextStack* GetJSContextStack() {return mJSContextStack;}

    XPCCallContext*  GetCallContext() const {return mCallContext;}
    XPCCallContext*  SetCallContext(XPCCallContext* ccx)
        {XPCCallContext* old = mCallContext; mCallContext = ccx; return old;}

    jsval GetResolveName() const {return mResolveName;}
    jsval SetResolveName(jsval name)
        {jsval old = mResolveName; mResolveName = name; return old;}

    XPCWrappedNative* GetResolvingWrapper() const {return mResolvingWrapper;}
    XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w)
        {XPCWrappedNative* old = mResolvingWrapper;
         mResolvingWrapper = w; return old;}

    void Cleanup();
    void ReleaseNatives();

    PRBool IsValid() const {return mJSContextStack != nsnull;}

    static PRLock* GetLock() {return gLock;}
    // Must be called with the threads locked.
    static XPCPerThreadData* IterateThreads(XPCPerThreadData** iteratorp);

    XPCContext* GetRecentXPCContext(JSContext* cx) const
        {return cx == mMostRecentJSContext ? mMostRecentXPCContext : nsnull;}

    void SetRecentContext(JSContext* cx, XPCContext* xpcc)
        {mMostRecentJSContext = cx; mMostRecentXPCContext = xpcc;}

    void ClearRecentContext()
        {mMostRecentJSContext = nsnull; mMostRecentXPCContext = nsnull;}

    AutoMarkingPtr**  GetAutoRootsAdr() {return &mAutoRoots;}

    void MarkAutoRootsBeforeJSFinalize(JSContext* cx);
    void MarkAutoRootsAfterJSFinalize();

    jsuword GetStackLimit() const { return mStackLimit; }

    static void InitStatics()
        { gLock = nsnull; gThreads = nsnull; gTLSIndex = BAD_TLS_INDEX; }

#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
    JSUint32  IncrementWrappedNativeThreadsafetyReportDepth()
        {return ++mWrappedNativeThreadsafetyReportDepth;}
    void      ClearWrappedNativeThreadsafetyReportDepth()
        {mWrappedNativeThreadsafetyReportDepth = 0;}
#endif

private:
    XPCPerThreadData();

private:
    XPCJSContextStack*   mJSContextStack;
    XPCPerThreadData*    mNextThread;
    XPCCallContext*      mCallContext;
    jsval                mResolveName;
    XPCWrappedNative*    mResolvingWrapper;

    JSContext*           mMostRecentJSContext;
    XPCContext*          mMostRecentXPCContext;

    nsIExceptionManager* mExceptionManager;
    nsIException*        mException;
    JSBool               mExceptionManagerNotAvailable;
    AutoMarkingPtr*      mAutoRoots;

    jsuword              mStackLimit;

#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
    JSUint32             mWrappedNativeThreadsafetyReportDepth;
#endif
    PRThread*            mThread;
    nsVoidArray          mNativesToReleaseArray;

    static PRLock*           gLock;
    static XPCPerThreadData* gThreads;
    static PRUintn           gTLSIndex;
};

/**************************************************************/

#define NS_XPC_THREAD_JSCONTEXT_STACK_CID  \
{ 0xff8c4d10, 0x3194, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class nsXPCThreadJSContextStackImpl : public nsIThreadJSContextStack,
                                      public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSCONTEXTSTACK
    NS_DECL_NSITHREADJSCONTEXTSTACK

    // This returns and AddRef'd pointer. It does not do this with an out param
    // only because this form  is required by generic module macro:
    // NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR
    static nsXPCThreadJSContextStackImpl* GetSingleton();

    static void InitStatics() { gXPCThreadJSContextStack = nsnull; }
    static void FreeSingleton();

    nsXPCThreadJSContextStackImpl();
    virtual ~nsXPCThreadJSContextStackImpl();

private:
    XPCJSContextStack* GetStackForCurrentThread()
        {XPCPerThreadData* data = XPCPerThreadData::GetData();
         return data ? data->GetJSContextStack() : nsnull;}

    static nsXPCThreadJSContextStackImpl* gXPCThreadJSContextStack;
    friend class nsXPCJSContextStackIterator;
};

/***************************************************************************/
#define NS_JS_RUNTIME_SERVICE_CID \
{0xb5e65b52, 0x1dd1, 0x11b2, \
    { 0xae, 0x8f, 0xf0, 0x92, 0x8e, 0xd8, 0x84, 0x82 }}

#ifndef XPCONNECT_STANDALONE
#include "nsIScriptSecurityManager.h"

class BackstagePass : public nsIScriptObjectPrincipal,
                      public nsIXPCScriptable,
                      public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  virtual nsIPrincipal* GetPrincipal() {
    return mPrincipal;
  }

  BackstagePass(nsIPrincipal *prin) :
    mPrincipal(prin)
  {
  }

  virtual ~BackstagePass() { }

private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

#else

class BackstagePass : public nsIXPCScriptable, public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  BackstagePass()
  {
  }

  virtual ~BackstagePass() { }
};

#endif

class nsJSRuntimeServiceImpl : public nsIJSRuntimeService,
                               public nsSupportsWeakReference
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSRUNTIMESERVICE

    // This returns an AddRef'd pointer. It does not do this with an out param
    // only because this form  is required by generic module macro:
    // NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR
    static nsJSRuntimeServiceImpl* GetSingleton();

    static void FreeSingleton();

    nsJSRuntimeServiceImpl();
    virtual ~nsJSRuntimeServiceImpl();

    static void InitStatics() { gJSRuntimeService = nsnull; }
 protected:
    JSRuntime *mRuntime;
    static nsJSRuntimeServiceImpl* gJSRuntimeService;
    nsCOMPtr<nsIXPCScriptable> mBackstagePass;
};

/***************************************************************************/
// 'Components' object

class nsXPCComponents : public nsIXPCComponents,
                        public nsIXPCScriptable,
                        public nsIClassInfo
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
                      , public nsISecurityCheckedComponent
#endif
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

public:
    static JSBool
    AttachNewComponentsObject(XPCCallContext& ccx,
                              XPCWrappedNativeScope* aScope,
                              JSObject* aGlobal);

    void SystemIsBeingShutDown() {ClearMembers();}

    virtual ~nsXPCComponents();

private:
    nsXPCComponents();
    void ClearMembers();

private:
    nsXPCComponents_Interfaces*     mInterfaces;
    nsXPCComponents_InterfacesByID* mInterfacesByID;
    nsXPCComponents_Classes*        mClasses;
    nsXPCComponents_ClassesByID*    mClassesByID;
    nsXPCComponents_Results*        mResults;
    nsXPCComponents_ID*             mID;
    nsXPCComponents_Exception*      mException;
    nsXPCComponents_Constructor*    mConstructor;
    nsXPCComponents_Utils*          mUtils;
};

/***************************************************************************/

class nsXPCComponents_Interfaces :
            public nsIScriptableInterfaces,
            public nsIXPCScriptable,
            public nsIClassInfo
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEINTERFACES
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

public:
    nsXPCComponents_Interfaces();
    virtual ~nsXPCComponents_Interfaces();

private:
    nsCOMPtr<nsIInterfaceInfoManager> mManager;
};


/***************************************************************************/

extern JSObject*
xpc_NewIDObject(JSContext *cx, JSObject* jsobj, const nsID& aID);

extern nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj);

extern JSBool
xpc_JSObjectIsID(JSContext *cx, JSObject* obj);

/***************************************************************************/
// in xpcdebug.cpp

extern JSBool
xpc_DumpJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals,
                JSBool showThisProps);

extern JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, JSUint32 frameno, const char* text);

extern JSBool
xpc_DumpJSObject(JSObject* obj);

extern JSBool
xpc_InstallJSDebuggerKeywordHandler(JSRuntime* rt);

/***************************************************************************/

// Definition of nsScriptError, defined here because we lack a place to put
// XPCOM objects associated with the JavaScript engine.
class nsScriptError : public nsIScriptError {
public:
    nsScriptError();

    virtual ~nsScriptError();

  // TODO - do something reasonable on getting null from these babies.

    NS_DECL_ISUPPORTS
    NS_DECL_NSICONSOLEMESSAGE
    NS_DECL_NSISCRIPTERROR

private:
    nsString mMessage;
    nsString mSourceName;
    PRUint32 mLineNumber;
    nsString mSourceLine;
    PRUint32 mColumnNumber;
    PRUint32 mFlags;
    nsCString mCategory;
};

/***************************************************************************/
// XXX allowing for future notifications to XPCCallContext

class AutoJSRequest
{
public:
    AutoJSRequest(XPCCallContext& aCCX)
      : mCCX(aCCX), mCX(aCCX.GetJSContext()) {BeginRequest();}
    ~AutoJSRequest() {EndRequest();}

    void EndRequest() {
        if(mCX) {
            JS_EndRequest(mCX);
            mCX = nsnull;
        }
    }
private:
    void BeginRequest() {
        if(JS_GetContextThread(mCX))
            JS_BeginRequest(mCX);
        else
            mCX = nsnull;
    }
private:
    XPCCallContext& mCCX;
    JSContext* mCX;
};

class AutoJSSuspendRequest
{
public:
    AutoJSSuspendRequest(XPCCallContext& aCCX)
      : mCCX(aCCX), mCX(aCCX.GetJSContext()) {SuspendRequest();}
    ~AutoJSSuspendRequest() {ResumeRequest();}

    void ResumeRequest() {
        if(mCX) {
            JS_ResumeRequest(mCX, mDepth);
            mCX = nsnull;
        }
    }
private:
    void SuspendRequest() {
        if(JS_GetContextThread(mCX))
            mDepth = JS_SuspendRequest(mCX);
        else
            mCX = nsnull;
    }
private:
    XPCCallContext& mCCX;
    JSContext* mCX;
    jsrefcount mDepth;
};

class AutoJSSuspendRequestWithNoCallContext
{
public:
    AutoJSSuspendRequestWithNoCallContext(JSContext *aCX)
      : mCX(aCX) {SuspendRequest();}
    ~AutoJSSuspendRequestWithNoCallContext() {ResumeRequest();}

    void ResumeRequest() {
        if(mCX) {
            JS_ResumeRequest(mCX, mDepth);
            mCX = nsnull;
        }
    }
private:
    void SuspendRequest() {
        if(JS_GetContextThread(mCX))
            mDepth = JS_SuspendRequest(mCX);
        else
            mCX = nsnull;
    }
private:
    JSContext* mCX;
    jsrefcount mDepth;
};

/*****************************************/

class AutoJSRequestWithNoCallContext
{
public:
    AutoJSRequestWithNoCallContext(JSContext* aCX) : mCX(aCX) {BeginRequest();}
    ~AutoJSRequestWithNoCallContext() {EndRequest();}

    void EndRequest() {
        if(mCX) {
            JS_EndRequest(mCX);
            mCX = nsnull;
        }
    }
private:
    void BeginRequest() {
        if(JS_GetContextThread(mCX))
            JS_BeginRequest(mCX);
        else
            mCX = nsnull;
    }
private:
    JSContext* mCX;
};

/***************************************************************************/
class AutoJSErrorAndExceptionEater
{
public:
    AutoJSErrorAndExceptionEater(JSContext* aCX)
        : mCX(aCX),
          mOldErrorReporter(JS_SetErrorReporter(mCX, nsnull)),
          mOldExceptionState(JS_SaveExceptionState(mCX)) {}
    ~AutoJSErrorAndExceptionEater()
    {
        JS_SetErrorReporter(mCX, mOldErrorReporter);
        JS_RestoreExceptionState(mCX, mOldExceptionState);
    }
private:
    JSContext*        mCX;
    JSErrorReporter   mOldErrorReporter;
    JSExceptionState* mOldExceptionState;
};

/******************************************************************************
 * Handles pre/post script processing and the setting/resetting the error
 * reporter
 */
class AutoScriptEvaluate
{
public:
    /**
     * Saves the JSContext as well as initializing our state
     * @param cx The JSContext, this can be null, we don't do anything then
     */
    AutoScriptEvaluate(JSContext * cx)
         : mJSContext(cx), mState(0), mEvaluated(PR_FALSE), 
           mContextHasThread(0) {}

    /**
     * Does the pre script evaluation and sets the error reporter if given
     * This function should only be called once, and will assert if called
     * more than once
     * @param errorReporter the error reporter callback function to set
     */

    void StartEvaluating(JSErrorReporter errorReporter = nsnull);
    /**
     * Does the post script evaluation and resets the error reporter
     */
    ~AutoScriptEvaluate();
private:
    JSContext* mJSContext;
    JSExceptionState* mState;
    JSErrorReporter mOldErrorReporter;
    PRBool mEvaluated;
    jsword mContextHasThread;

    // No copying or assignment allowed
    AutoScriptEvaluate(const AutoScriptEvaluate &);
    AutoScriptEvaluate & operator =(const AutoScriptEvaluate &);
};

/***************************************************************************/
class AutoResolveName
{
public:
    AutoResolveName(XPCCallContext& ccx, jsval name)
        : mTLS(ccx.GetThreadData()),
          mOld(mTLS->SetResolveName(name)),
          mCheck(name) {}
    ~AutoResolveName()
        {
#ifdef DEBUG
            jsval old = 
#endif
            mTLS->SetResolveName(mOld);
            NS_ASSERTION(old == mCheck, "Bad Nesting!");
        }

private:
    XPCPerThreadData* mTLS;
    jsval mOld;
    jsval mCheck;
};

/***************************************************************************/
class XPCMarkableJSVal
{
public:
    XPCMarkableJSVal(jsval val) : mVal(val), mValPtr(&mVal) {}
    XPCMarkableJSVal(jsval *pval) : mVal(0), mValPtr(pval) {}
    ~XPCMarkableJSVal() {}
    void Mark() {}
    void MarkBeforeJSFinalize(JSContext* cx)
        {if(JSVAL_IS_GCTHING(*mValPtr))
            JS_MarkGCThing(cx, JSVAL_TO_GCTHING(*mValPtr), 
                           "XPCMarkableJSVal", nsnull);}
    void AutoMark(JSContext*) {}
private:
    XPCMarkableJSVal(); // not implemented    
    jsval  mVal;
    jsval* mValPtr;
}; 

/***************************************************************************/
// AutoMarkingPtr is the base class for the various AutoMarking pointer types 
// below. This system allows us to temporarily protect instances of our garbage 
// collected types after they are constructed but before they are safely 
// attached to other rooted objects.
// This base class has pure virtual support for marking. 

class AutoMarkingPtr
{
public:
    AutoMarkingPtr(XPCCallContext& ccx)
        : mNext(nsnull), mTLS(ccx.GetThreadData()) {Link();}

    virtual ~AutoMarkingPtr() {Unlink();}
    
    void Link() 
        {if(!mTLS) return;
         AutoMarkingPtr** list = mTLS->GetAutoRootsAdr(); 
         mNext = *list; *list = this;}

    void Unlink() 
        {if(!mTLS) return;
         AutoMarkingPtr** cur = mTLS->GetAutoRootsAdr(); 
         while(*cur != this) {
            NS_ASSERTION(*cur, "This object not in list!");
            cur = &(*cur)->mNext;
         }
         *cur = mNext;
         mTLS = nsnull;
        }

    AutoMarkingPtr* GetNext() {return mNext;}
    
    virtual void MarkBeforeJSFinalize(JSContext* cx) = 0;
    virtual void MarkAfterJSFinalize() = 0;

protected:
    AutoMarkingPtr* mNext;
    XPCPerThreadData* mTLS;
};

// More joy of macros...

#define DEFINE_AUTO_MARKING_PTR_TYPE(class_, type_)                          \
class class_ : public AutoMarkingPtr                                         \
{                                                                            \
public:                                                                      \
    class_ (XPCCallContext& ccx, type_ * ptr = nsnull)                       \
        : AutoMarkingPtr(ccx), mPtr(ptr) {}                                  \
    virtual ~ class_ () {}                                                   \
                                                                             \
    virtual void MarkBeforeJSFinalize(JSContext* cx)                         \
        {if(mPtr) {                                                          \
           mPtr->MarkBeforeJSFinalize(cx);                                   \
           mPtr->AutoMark(cx);                                               \
         }                                                                   \
         if(mNext) mNext->MarkBeforeJSFinalize(cx);}                         \
                                                                             \
    virtual void MarkAfterJSFinalize()                                       \
        {if(mPtr) mPtr->Mark();                                              \
         if(mNext) mNext->MarkAfterJSFinalize();}                            \
                                                                             \
    type_ * get()        const  {return mPtr;}                               \
    operator type_ *()   const  {return mPtr;}                               \
    type_ * operator->() const  {return mPtr;}                               \
                                                                             \
    class_ & operator =(type_ * p)                                           \
        {mPtr = p; return *this;}                                            \
                                                                             \
protected:                                                                   \
    type_ * mPtr;                                                            \
};

// Use the macro above to define our AutoMarking types...

DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingNativeInterfacePtr, XPCNativeInterface)
DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingNativeSetPtr, XPCNativeSet)
DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingWrappedNativePtr, XPCWrappedNative)
DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingWrappedNativeTearOffPtr, XPCWrappedNativeTearOff)
DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingWrappedNativeProtoPtr, XPCWrappedNativeProto)
DEFINE_AUTO_MARKING_PTR_TYPE(AutoMarkingJSVal, XPCMarkableJSVal)
                                    
#define DEFINE_AUTO_MARKING_ARRAY_PTR_TYPE(class_, type_)                    \
class class_ : public AutoMarkingPtr                                         \
{                                                                            \
public:                                                                      \
    class_ (XPCCallContext& ccx)                                             \
        : AutoMarkingPtr(ccx), mPtr(nsnull), mCount(0) {}                    \
    class_ (XPCCallContext& ccx, type_** aPtr, PRUint32 aCount,              \
            PRBool aClear = PR_FALSE)                                        \
        : AutoMarkingPtr(ccx), mPtr(aPtr), mCount(aCount)                    \
    {                                                                        \
        if(!mPtr) mCount = 0;                                                \
        else if(aClear) memset(mPtr, 0, mCount*sizeof(type_*));              \
    }                                                                        \
    virtual ~ class_ () {}                                                   \
                                                                             \
    virtual void MarkBeforeJSFinalize(JSContext* cx)                         \
    {                                                                        \
        for(PRUint32 i = 0; i < mCount; ++i)                                 \
        {                                                                    \
            type_* cur = mPtr[i];                                            \
            if(cur)                                                          \
            {                                                                \
                cur->MarkBeforeJSFinalize(cx);                               \
                cur->AutoMark(cx);                                           \
            }                                                                \
        }                                                                    \
        if(mNext) mNext->MarkBeforeJSFinalize(cx);                           \
    }                                                                        \
                                                                             \
    virtual void MarkAfterJSFinalize()                                       \
    {                                                                        \
        for(PRUint32 i = 0; i < mCount; ++i)                                 \
        {                                                                    \
            type_* cur = mPtr[i];                                            \
            if(cur)                                                          \
                cur->Mark();                                                 \
        }                                                                    \
        if(mNext) mNext->MarkAfterJSFinalize();                              \
    }                                                                        \
                                                                             \
    type_ ** get()       const  {return mPtr;}                               \
    operator type_ **()  const  {return mPtr;}                               \
    type_ ** operator->() const  {return mPtr;}                              \
                                                                             \
    class_ & operator =(const class_ & inst)                                 \
        {mPtr = inst.mPtr; mCount = inst.mCount; return *this;}              \
                                                                             \
protected:                                                                   \
    type_ ** mPtr;                                                           \
    PRUint32 mCount;                                                         \
};

DEFINE_AUTO_MARKING_ARRAY_PTR_TYPE(AutoMarkingNativeInterfacePtrArrayPtr,
                                   XPCNativeInterface)
    
// Note: It looked like I would need one of these AutoMarkingPtr types for
// XPCNativeScriptableInfo in order to manage marking its 
// XPCNativeScriptableShared member during construction. But AFAICT we build
// these and bind them to rooted things so immediately that this just is not
// needed.

#define AUTO_MARK_JSVAL_HELPER2(tok, line) tok##line
#define AUTO_MARK_JSVAL_HELPER(tok, line) AUTO_MARK_JSVAL_HELPER2(tok, line)

#define AUTO_MARK_JSVAL(ccx, val)                                            \
    XPCMarkableJSVal AUTO_MARK_JSVAL_HELPER(_val_,__LINE__)(val);            \
    AutoMarkingJSVal AUTO_MARK_JSVAL_HELPER(_automarker_,__LINE__)           \
    (ccx, &AUTO_MARK_JSVAL_HELPER(_val_,__LINE__))

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
/***************************************************************************/
// Allocates a string that grants all access ("AllAccess")

extern char* xpc_CloneAllAccess();
/***************************************************************************/
// Returns access if wideName is in list

extern char * xpc_CheckAccessList(const PRUnichar* wideName, const char* list[]);
#endif

/***************************************************************************/
// in xpcvariant.cpp...

// {1809FD50-91E8-11d5-90F9-0010A4E73D9A}
#define XPCVARIANT_IID \
    {0x1809fd50, 0x91e8, 0x11d5, \
      { 0x90, 0xf9, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a } }

class XPCVariant : public nsIVariant
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIVARIANT
    // If this class ever implements nsIWritableVariant, take special care with
    // the case when mJSVal is JSVAL_STRING, since we don't own the data in
    // that case.

    // We #define and iid so that out module local code can use QI to detect 
    // if a given nsIVariant is in fact an XPCVariant. 
    NS_DECLARE_STATIC_IID_ACCESSOR(XPCVARIANT_IID)

    static XPCVariant* newVariant(XPCCallContext& ccx, jsval aJSVal);

    jsval GetJSVal() const {return mJSVal;}

    XPCVariant(JSRuntime* aJSRuntime);

    /**
     * Convert a variant into a jsval.
     *
     * @param ccx the context for the whole procedure
     * @param variant the variant to convert
     * @param scope the default scope to put on the new JSObject's __parent__
     *        chain
     * @param pErr [out] relevant error code, if any.
     * @param pJSVal [out] the resulting jsval.
     */    
    static JSBool VariantDataToJS(XPCCallContext& ccx, 
                                  nsIVariant* variant,
                                  JSObject* scope, nsresult* pErr,
                                  jsval* pJSVal);

protected:
    virtual ~XPCVariant();

    JSBool InitializeData(XPCCallContext& ccx);

protected:
    nsDiscriminatedUnion mData;
    jsval                mJSVal;

    // For faster GC-thing locking and unlocking
    JSRuntime*           mJSRuntime;
};

NS_DEFINE_STATIC_IID_ACCESSOR(XPCVariant, XPCVARIANT_IID)

/***************************************************************************/
#ifndef XPCONNECT_STANDALONE

#define PRINCIPALHOLDER_IID \
{0xbf109f49, 0xf94a, 0x43d8, {0x93, 0xdb, 0xe4, 0x66, 0x49, 0xc5, 0xd9, 0x7d}}

class PrincipalHolder : public nsIScriptObjectPrincipal
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(PRINCIPALHOLDER_IID)

    PrincipalHolder(nsIPrincipal *holdee)
        : mHoldee(holdee)
    {
    }
    virtual ~PrincipalHolder() { }

    NS_DECL_ISUPPORTS

    nsIPrincipal *GetPrincipal();

private:
    nsCOMPtr<nsIPrincipal> mHoldee;
};

NS_DEFINE_STATIC_IID_ACCESSOR(PrincipalHolder, PRINCIPALHOLDER_IID)

#endif /* !XPCONNECT_STANDALONE */

/***************************************************************************/
// Utilities

JSBool xpc_IsReportableErrorCode(nsresult code);

JSObject* xpc_CloneJSFunction(XPCCallContext &ccx, JSObject *funobj,
                              JSObject *parent);

#ifndef XPCONNECT_STANDALONE

// Helper for creating a sandbox object to use for evaluating
// untrusted code completely separated from all other code in the
// system using xpc_EvalInSandbox(). Takes the JSContext on which to
// do setup etc on, puts the sandbox object in *vp (which must be
// rooted by the caller), and uses the principal that's either
// directly passed in prinOrSop or indirectly as an
// nsIScriptObjectPrincipal holding the principal. If no principal is
// reachable through prinOrSop, a new null principal will be created
// and used.
nsresult
xpc_CreateSandboxObject(JSContext * cx, jsval * vp, nsISupports *prinOrSop);

// Helper for evaluating scripts in a sandbox object created with
// xpc_CreateSandboxObject(). The caller is responsible of ensuring
// that *rval doesn't get collected during the call or usage after the
// call. This helper will use filename and lineNo for error reporting,
// and if no filename is provided it will use the codebase from the
// principal and line number 1 as a fallback.
nsresult
xpc_EvalInSandbox(JSContext *cx, JSObject *sandbox, const nsAString& source,
                  const char *filename, PRInt32 lineNo, jsval *rval);
#endif /* !XPCONNECT_STANDALONE */

/***************************************************************************/
// Inlined utilities.

inline JSBool
xpc_ForcePropertyResolve(JSContext* cx, JSObject* obj, jsval idval);

inline jsval
GetRTStringByIndex(JSContext *cx, uintN index);

nsISupports *
XPC_GetIdentityObject(JSContext *cx, JSObject *obj);

PRBool
IsXPCSafeJSObjectWrapperClass(JSClass *clazz);

JSObject *
XPC_SJOW_GetUnsafeObject(JSContext *cx, JSObject *obj);

JSBool
XPC_SJOW_Construct(JSContext *cx, JSObject *obj, uintN, jsval *argv,
                   jsval *rval);

PRBool
XPC_SJOW_AttachNewConstructorObject(XPCCallContext &ccx,
                                    JSObject *aGlobalObject);

#ifdef XPC_IDISPATCH_SUPPORT
// IDispatch specific classes
#include "XPCDispPrivate.h"
#endif

/***************************************************************************/
// Inlines use the above - include last.

#include "xpcinlines.h"

/***************************************************************************/
// Maps have inlines that use the above - include last.

#include "xpcmaps.h"

/***************************************************************************/

#endif /* xpcprivate_h___ */
