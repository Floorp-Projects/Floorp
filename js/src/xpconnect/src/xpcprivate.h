/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* All the XPConnect private declarations - only include locally. */

#ifndef xpcprivate_h___
#define xpcprivate_h___

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIGenericFactory.h"
#include "nsMemory.h"
#include "nsIXPConnect.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIXPCSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsAutoLock.h"
#include "xptcall.h"
#include "jsapi.h"
#include "jshash.h"
#include "jsprf.h"
#include "jsinterp.h"
#include "jscntxt.h"
#include "jsdbgapi.h"
#include "xptinfo.h"
#include "xpcforwards.h"
#include "xpclog.h"
#include "xpccomponents.h"
#include "xpcexception.h"
#include "xpcjsid.h"
#include "prlong.h"
#include "prmem.h"
#include "prenv.h"

#include "nsIJSContextStack.h"
#include "prthread.h"
#include "nsDeque.h"

#ifdef XPCONNECT_STANDALONE
#include <math.h>
#include "nsString.h"
#else
#include "nsIScriptObjectOwner.h"   // for DOM hack in xpcconvert.cpp
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#endif

#include "nsIConsoleService.h"
#include "nsIScriptError.h"

//#define XPC_TOOLS_SUPPORT 1

#ifdef XPC_TOOLS_SUPPORT
#include "nsIXPCToolsProfiler.h"
#endif

#ifdef DEBUG
#define XPC_DETECT_LEADING_UPPERCASE_ACCESS_ERRORS 1
#endif

#ifdef DEBUG_jband
#define XPC_DUMP_AT_SHUTDOWN 1
#define XPC_CHECK_WRAPPERS_AT_SHUTDOWN 1
//#define DEBUG_stats_jband 1
#endif

/***************************************************************************/
// default initial sizes for maps (hashtables)

#define XPC_CONTEXT_MAP_SIZE        16
#define XPC_JS_MAP_SIZE             256
#define XPC_NATIVE_MAP_SIZE         256
#define XPC_JS_CLASS_MAP_SIZE       256
#define XPC_NATIVE_CLASS_MAP_SIZE   256

/***************************************************************************/
// data declarations...
extern const char XPC_ARG_FORMATTER_FORMAT_STR[]; // format string

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

class nsXPConnect : public nsIXPConnect
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECT

    // non-interface implementation
public:
    // these all return an AddRef'd object (or nsnull on failure)
    static nsXPConnect* GetXPConnect();
    static nsIInterfaceInfoManager* GetInterfaceInfoManager(nsXPConnect* xpc = nsnull);
    static nsIThreadJSContextStack* GetContextStack(nsXPConnect* xpc = nsnull);
    static XPCJSThrower* GetJSThrower(nsXPConnect* xpc = nsnull);
    static XPCJSRuntime* GetRuntime(nsXPConnect* xpc = nsnull);
    static XPCContext*  GetContext(JSContext* cx, nsXPConnect* xpc = nsnull);

    static JSBool IsISupportsDescendant(nsIInterfaceInfo* info);
    nsIXPCScriptable* GetArbitraryScriptable() {return mArbitraryScriptable;}

    nsIXPCSecurityManager* GetDefaultSecurityManager() const
        {return mDefaultSecurityManager;}
    PRUint16 GetDefaultSecurityManagerFlags() const
        {return mDefaultSecurityManagerFlags;}

    // called by module code on dll shutdown
    static void ReleaseXPConnectSingleton();
    virtual ~nsXPConnect();

protected:
    nsXPConnect();

private:
    JSBool EnsureRuntime() {return mRuntime ? JS_TRUE : CreateRuntime();}
    JSBool CreateRuntime();

private:
    // Singleton instance
    static nsXPConnect* gSelf;
    static JSBool gOnceAliveNowDead;

    XPCJSRuntime* mRuntime;
    nsIXPCScriptable* mArbitraryScriptable;
    nsIInterfaceInfoManager* mInterfaceInfoManager;
    XPCJSThrower* mThrower;
    nsIThreadJSContextStack* mContextStack;
    nsIXPCSecurityManager* mDefaultSecurityManager;
    PRUint16 mDefaultSecurityManagerFlags;
#ifdef XPC_TOOLS_SUPPORT
    nsCOMPtr<nsIXPCToolsProfiler> mProfiler;
    nsCOMPtr<nsILocalFile> mProfilerOutputFile;
#endif
};

/***************************************************************************/

// In the current xpconnect systen there can only be one XPCJSRuntime.
// So, xpconnect can only be used on one JSRuntime within the process.

// no virtuals. no refcounting.
class XPCJSRuntime
{
public:
    static XPCJSRuntime* newXPCJSRuntime(nsXPConnect* aXPConnect,
                                         nsIJSRuntimeService* aJSRuntimeService);

    JSRuntime*      GetJSRuntime() const {return mJSRuntime;}
    nsXPConnect*    GetXPConnect() const {return mXPConnect;}

    JSObject2WrappedJSMap*     GetWrappedJSMap()          const
        {return mWrappedJSMap;}
    IID2WrappedJSClassMap*     GetWrappedJSClassMap()     const
        {return mWrappedJSClassMap;}
    IID2WrappedNativeClassMap* GetWrappedNativeClassMap() const
        {return mWrappedNativeClassMap;}

    PRLock* GetMapLock() const {return mMapLock;}

    XPCContext* GetXPCContext(JSContext* cx);
    XPCContext* SyncXPCContextList(JSContext* cx = nsnull);


    // Mapping of often used strings to jsid atoms that live 'forever'.
    //
    // To add a new string: add to this list and to XPCJSRuntime::mStrings
    // at the top of xpcjsruntime.cpp
    enum {
        IDX_CONSTRUCTOR             = 0 ,
        IDX_TO_STRING               ,
        IDX_LAST_RESULT             ,
        IDX_RETURN_CODE             ,
        IDX_VALUE                   ,
        IDX_QUERY_INTERFACE         ,
        IDX_COMPONENTS              ,
        IDX_WRAPPED_JSOBJECT        ,
        IDX_TOTAL_COUNT // just a count of the above
    };

    jsid GetStringID(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrIDs[index];
    }
    const char* GetStringName(uintN index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    void DebugDump(PRInt16 depth);

    ~XPCJSRuntime();

#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
   void DEBUG_AddWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {JS_HashTableAdd(DEBUG_WrappedNativeHashtable, wrapper, wrapper);}

   void DEBUG_RemoveWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {JS_HashTableRemove(DEBUG_WrappedNativeHashtable, wrapper);}

   private:                            
   JSHashTable *DEBUG_WrappedNativeHashtable;
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

    nsXPConnect* mXPConnect;
    JSRuntime*  mJSRuntime;
    nsIJSRuntimeService* mJSRuntimeService; // hold this to hold the JSRuntime
    JSContext2XPCContextMap* mContextMap;
    JSObject2WrappedJSMap* mWrappedJSMap;
    IID2WrappedJSClassMap* mWrappedJSClassMap;
    IID2WrappedNativeClassMap* mWrappedNativeClassMap;
    PRLock* mMapLock;
};

/***************************************************************************/

// internal 'dumb' class to let us quickly build CallContexts (see below)
struct NativeCallContextData
{
    // no ctor or dtor so we have random state at creation.

    void init(nsISupports*               acallee,
              uint16                     aindex,
              nsIXPConnectWrappedNative* awrapper,
              JSContext*                 acx,
              PRUint32                   aargc,
              jsval*                     aargv,
              jsval*                     aretvalp)
    {
        this->callee    = acallee;
        this->index     = aindex;
        this->wrapper   = awrapper;
        this->cx        = acx;
        this->argc      = aargc;
        this->argv      = aargv;
        this->retvalp   = aretvalp;
        this->threw     = JS_FALSE;
        this->retvalSet = JS_FALSE;
    }

    nsISupports*               callee;
    nsIInterfaceInfo*          info;
    uint16                     index;
    nsIXPConnectWrappedNative* wrapper;
    JSContext*                 cx;
    PRUint32                   argc;
    jsval*                     argv;
    jsval*                     retvalp;
    JSBool                     threw;
    JSBool                     retvalSet;
};

class nsXPCNativeCallContext : public nsIXPCNativeCallContext
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCNATIVECALLCONTEXT

public:
    // non-interface implementation

    nsXPCNativeCallContext();
    virtual ~nsXPCNativeCallContext();

    NativeCallContextData* SetData(NativeCallContextData* aData) 
        {NativeCallContextData* temp = mData; mData = aData; return temp;}
private:
    NativeCallContextData* mData;
};

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

    LangType GetCallingLangType() const 
        {return mCallingLangType;}
    LangType SetCallingLangType(LangType lt) 
        {LangType tmp = mCallingLangType; mCallingLangType = lt; return tmp;}
    JSBool CallerTypeIsJavaScript() const {return LANG_JS == mCallingLangType;}
    JSBool CallerTypeIsNative() const {return LANG_NATIVE == mCallingLangType;}
    JSBool CallerTypeIsKnown() const {return LANG_UNKNOWN != mCallingLangType;}

    nsIXPCException* GetException()
        {
            NS_IF_ADDREF(mException);
            return mException;
        }
    void SetException(nsIXPCException* e)
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

    nsXPCNativeCallContext* GetNativeCallContext()
        {return &mNativeCallContext;}

    nsIXPCSecurityManager* GetAppropriateSecurityManager(PRUint16 flags) const
        {
            NS_WARN_IF_FALSE(CallerTypeIsKnown(),"missing caller type set somewhere");
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
    PRUint16 mSecurityManagerFlags;
    nsIXPCException* mException;
    nsXPCNativeCallContext mNativeCallContext;
    LangType mCallingLangType;
};

/***************************************************************************/
// A class to put on the stack when we are entering xpconnect from an entry
// point where the JSContext is known. This pushs and pops the given context
// with the nsThreadJSContextStack service as this object goes into and out
// of scope. It is optimized to not push/pop the cx if it is already on top
// of the stack.

class AutoPushJSContext
{
public:
    AutoPushJSContext(JSContext *cx, nsXPConnect* xpc = nsnull);
    ~AutoPushJSContext();
private:
    AutoPushJSContext();    // no implementation

private:
    nsIThreadJSContextStack* mContextStack;
#ifdef DEBUG
    JSContext* mDebugCX;
#endif
};

#define AUTO_PUSH_JSCONTEXT(cx) AutoPushJSContext _AutoPushJSContext(cx)
#define AUTO_PUSH_JSCONTEXT2(cx,xpc) AutoPushJSContext _AutoPushJSContext(cx,xpc)

/***************************************************************************/
// A class to put on the stack when we are entering xpconnect from an entry
// point where we need to use a JSContext that is 'compatible' with the one 
// indicated. 'Compatible' means that the JSContext is from the same JSRuntime
// and is either already the top JSContext in the nsThreadJSContextStack or
// is a JSContext that xpconnect manages on a per thread basis for use when
// the nsThreadJSContextStack is empty. If the top JSContext on the 
// nsThreadJSContextStack hails from a JSRuntime that is different from the 
// JSRuntime of the indicated JSContext then the JSContext returned from
// AutoPushCompatibleJSContext::GetJSContext() will be nsnull to indicate 
// failure.
// 
// In practice this class is used when we will be calling to a wrapped JS 
// object.

class AutoPushCompatibleJSContext
{
public:
    AutoPushCompatibleJSContext(JSRuntime* rt, nsXPConnect* xpc = nsnull);
    ~AutoPushCompatibleJSContext();
    JSContext* GetJSContext() const {return mCX;}

private:
    AutoPushCompatibleJSContext();    // no implementation

private:
    nsIThreadJSContextStack* mContextStack;
    JSContext* mCX;
};

/***************************************************************************/
// This class is used to track whether xpconnect was entered from JavaScript
// or native code. This information is used to determine whther or not to call
// security hooks; i.e. the nsIXPCSecurityManager need not be called to protect
// xpconnect activities initiated by native code. Instances of this class are
// instatiated as auto objects at the various critical entry points of 
// xpconnect. On entry they set a variable in the XPCContext to track the 
// caller type and then restore the previous value upon destruction (when the 
// scope is exited).

class AutoPushCallingLangType
{
public:
    AutoPushCallingLangType(JSContext* cx, XPCContext::LangType type);
    AutoPushCallingLangType(XPCContext* xpcc, XPCContext::LangType type);
    ~AutoPushCallingLangType();

    XPCContext* GetXPCContext() const 
        {return mXPCContext;}
    JSContext*  GetJSContext() const 
        {return mXPCContext ? nsnull : mXPCContext->GetJSContext();}

private:
    void ctorCommon(XPCContext::LangType type)
    {
        if(mXPCContext)
        {
            mOldCallingLangType = mXPCContext->SetCallingLangType(type);
#ifdef DEBUG
            mDebugPushedCallingLangType = type;
#endif
        }
    }

private:
    XPCContext* mXPCContext;
    XPCContext::LangType mOldCallingLangType;
#ifdef DEBUG
    XPCContext::LangType mDebugPushedCallingLangType;
#endif
};

#define SET_CALLER_JAVASCRIPT(_context) \
    AutoPushCallingLangType _APCLT(_context, XPCContext::LANG_JS)

#define SET_CALLER_NATIVE(_context) \
    AutoPushCallingLangType _APCLT(_context, XPCContext::LANG_NATIVE)

/***************************************************************************/
// this interfaces exists so we can refcount nsXPCWrappedNativeScope

// {EA984010-97EA-11d3-BB2A-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_SCOPE_IID  \
{ 0xea984010, 0x97ea, 0x11d3, \
    { 0xbb, 0x2a, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedNativeScope : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_NATIVE_SCOPE_IID)
    NS_IMETHOD DebugDump(PRInt16 depth) = 0;
};

/*************************/
// Each nsXPCWrappedNative has a reference to one of these scope objects.
// An instance of nsXPCWrappedNativeScope is associated with a global object 
// using nsIXPConnect::InitClasses. At that same time a Components object is 
// attached to that global object. The scope object for any given JSObject is 
// found by doing a name lookup for the 'Components' object. When we are asked 
// to build a wrapper around a native object we look for a scope object and 
// from it look for an existing wrapper around the given native object. Thus, 
// each wrapped native object exposing a given interface can have zero or one 
// wrapper for each scope.

class nsXPCWrappedNativeScope : public nsIXPCWrappedNativeScope
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(PRInt16 depth);
public:
    XPCJSRuntime* 
    GetRuntime() const {return mRuntime;}

    Native2WrappedNativeMap* 
    GetWrappedNativeMap() const {return mWrappedNativeMap;}

    nsXPCComponents*
    GetComponents() const {return mComponents;}

    static nsXPCWrappedNativeScope* 
    FindInJSObjectScope(XPCContext* xpcc, JSObject* obj);

    static void 
    SystemIsBeingShutDown();

    static void
    DebugDumpAllScopes(PRInt16 depth);

    JSBool IsValid() const {return mRuntime != nsnull;}

    nsXPCWrappedNativeScope(XPCContext* xpcc, nsXPCComponents* comp);
    virtual ~nsXPCWrappedNativeScope();
private:
    nsXPCWrappedNativeScope(); // not implemented

private:
    static nsXPCWrappedNativeScope* gScopes;

    XPCJSRuntime*             mRuntime;
    Native2WrappedNativeMap*  mWrappedNativeMap;
    nsXPCComponents*          mComponents;
    nsXPCWrappedNativeScope*  mNext;
};

/***************************************************************************/
// code for throwing exceptions into JS

class XPCJSThrower
{
public:
    void ThrowBadResultException(nsresult rv,
                                 JSContext* cx,
                                 nsXPCWrappedNativeClass* clazz,
                                 const XPCNativeMemberDescriptor* desc,
                                 nsresult result);

    void ThrowBadParamException(nsresult rv,
                                JSContext* cx,
                                nsXPCWrappedNativeClass* clazz,
                                const XPCNativeMemberDescriptor* desc,
                                uintN paramNum);

    void ThrowException(nsresult rv,
                        JSContext* cx,
                        nsXPCWrappedNativeClass* clazz = nsnull,
                        const XPCNativeMemberDescriptor* desc = nsnull);

    XPCJSThrower(JSBool Verbose = JS_FALSE);
    ~XPCJSThrower();

private:
    void Verbosify(JSContext* cx,
                   nsXPCWrappedNativeClass* clazz,
                   const XPCNativeMemberDescriptor* desc,
                   char** psz, PRBool own);

    char* BuildCallerString(JSContext* cx);
    void BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz);
    JSBool ThrowExceptionObject(JSContext* cx, nsIXPCException* e);

private:
    JSBool mVerbose;
};


/***************************************************************************/
/***************************************************************************/

// this interfaces exists so we can refcount nsXPCWrappedJSClass
// {2453EBA0-A9B8-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_CLASS_IID  \
{ 0x2453eba0, 0xa9b8, 0x11d2,               \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedJSClass : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)
    NS_IMETHOD DebugDump(PRInt16 depth) = 0;
};

/*************************/

class nsXPCWrappedJSClass : public nsIXPCWrappedJSClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(PRInt16 depth);
public:

    static nsXPCWrappedJSClass* GetNewOrUsedClass(XPCJSRuntime* rt,
                                                  REFNSIID aIID);
    REFNSIID GetIID() const {return mIID;}
    XPCJSRuntime* GetJSRuntime() const {return mRuntime;}
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    const char* GetInterfaceName();

    static JSBool InitClasses(XPCContext* xpcc, JSObject* aGlobalJSObj);
    static JSBool IsWrappedJS(nsISupports* aPtr);

    NS_IMETHOD DelegatedQueryInterface(nsXPCWrappedJS* self, REFNSIID aIID,
                                       void** aInstancePtr);

    JSObject* GetRootJSObject(JSObject* aJSObj);

    NS_IMETHOD CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                        const nsXPTMethodInfo* info,
                        nsXPTCMiniVariant* params);

    virtual ~nsXPCWrappedJSClass();
private:
    nsXPCWrappedJSClass();   // not implemented
    nsXPCWrappedJSClass(XPCJSRuntime* rt, REFNSIID aIID,
                        nsIInterfaceInfo* aInfo);

    JSObject*  NewOutObject(JSContext* cx);

    JSObject*  CallQueryInterfaceOnJSObject(JSObject* jsobj, REFNSIID aIID);

    JSBool IsReflectable(uint16 i) const
        {return (JSBool)(mDescriptors[i/32] & (1 << (i%32)));}
    void SetReflectable(uint16 i, JSBool b)
        {if(b) mDescriptors[i/32] |= (1 << (i%32));
         else mDescriptors[i/32] &= ~(1 << (i%32));}

    enum SizeMode {GET_SIZE, GET_LENGTH};

    JSBool GetArraySizeFromParam(JSContext* cx,
                                 const nsXPTMethodInfo* method,
                                 const nsXPTParamInfo& param,
                                 uint16 methodIndex,
                                 uint8 paramIndex,
                                 SizeMode mode,
                                 nsXPTCMiniVariant* params,
                                 JSUint32* result);

    JSBool GetInterfaceTypeFromParam(JSContext* cx,
                                     const nsXPTMethodInfo* method,
                                     const nsXPTParamInfo& param,
                                     uint16 methodIndex,
                                     const nsXPTType& type,
                                     nsXPTCMiniVariant* params,
                                     JSBool* iidIsOwned,
                                     nsID** result);

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

class nsXPCWrappedJS : public nsXPTCStubBase, public nsIXPConnectWrappedJS
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDJS

    // Note that both nsXPTCStubBase and nsIXPConnectWrappedJS declare
    // a GetInterfaceInfo methos\d with the same sig. So, the declaration
    // for it here comes from the NS_DECL_NSIXPCONNECTWRAPPEDJS macro

    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* params);

    /* 
    * This is rarely called directly. Instead one usually calls
    * XPCConvert::JSObject2NativeInterface which will handles cases where the
    * JS object is already a wrapped native or a DOM object. 
    */
    static nsXPCWrappedJS* GetNewOrUsedWrapper(XPCContext* xpcc,
                                               JSObject* aJSObj,
                                               REFNSIID aIID);

    JSObject* GetJSObject() const {return mJSObj;}
    nsXPCWrappedJSClass*  GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}
    nsXPCWrappedJS* GetRootWrapper() const {return mRoot;}

    nsXPCWrappedJS* Find(REFNSIID aIID);

    JSBool IsValid() const {return mJSObj != nsnull;}
    void SystemIsBeingShutDown();

    virtual ~nsXPCWrappedJS();
private:
    nsXPCWrappedJS();   // not implemented
    nsXPCWrappedJS(XPCContext* xpcc,
                   JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root);

private:
    JSObject* mJSObj;
    nsXPCWrappedJSClass* mClass;
    nsXPCWrappedJS* mRoot;
    nsXPCWrappedJS* mNext;
};

/***************************************************************************/

// nsXPCWrappedNativeClass maintains an array of these things
class XPCNativeMemberDescriptor
{
private:
    enum {
        // these are all bitwise flags!
        NMD_CONSTANT    = 0x0, // categories...
        NMD_METHOD      = 0x1,
        NMD_ATTRIB_RO   = 0x2,
        NMD_ATTRIB_RW   = 0x3,
        NMD_CAT_MASK    = 0x3  // & mask for the categories above
        // any new bits start at 0x04
    };

public:
    jsid            id;     /* hashed name for quick JS property lookup */
    uintN           index;  /* in InterfaceInfo for const, method, and get */
    uintN           index2; /* in InterfaceInfo for set */
    intN            argc;
private:
    uint16          flags;
public:

    JSBool IsConstant() const  {return (flags & NMD_CAT_MASK) == NMD_CONSTANT;}
    JSBool IsMethod() const    {return (flags & NMD_CAT_MASK) == NMD_METHOD;}
    JSBool IsAttribute() const {return (JSBool)((flags & NMD_CAT_MASK) & NMD_ATTRIB_RO);}
    JSBool IsWritableAttribute() const {return (flags & NMD_CAT_MASK) == NMD_ATTRIB_RW;}
    JSBool IsReadOnlyAttribute() const {return (flags & NMD_CAT_MASK) == NMD_ATTRIB_RO;}

    void SetConstant()          {flags=(flags&~NMD_CAT_MASK)|NMD_CONSTANT;}
    void SetMethod()            {flags=(flags&~NMD_CAT_MASK)|NMD_METHOD;}
    void SetReadOnlyAttribute() {flags=(flags&~NMD_CAT_MASK)|NMD_ATTRIB_RO;}
    void SetWritableAttribute() {flags=(flags&~NMD_CAT_MASK)|NMD_ATTRIB_RW;}

    XPCNativeMemberDescriptor();
};

/*************************/

// this interfaces exists just so we can refcount nsXPCWrappedNativeClass
// {C9E36280-954A-11d2-BA5A-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID \
{ 0xc9e36280, 0x954a, 0x11d2,                   \
  { 0xba, 0x5a, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class nsIXPCWrappedNativeClass : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_NATIVE_CLASS_IID)
    NS_IMETHOD DebugDump(PRInt16 depth) = 0;
};

/*************************/

class nsXPCWrappedNativeClass : public nsIXPCWrappedNativeClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(PRInt16 depth);
public:
    static nsXPCWrappedNativeClass* GetNewOrUsedClass(XPCContext* xpcc,
                                                      REFNSIID aIID,
                                                      nsresult* pErr);
                                                      
    REFNSIID GetIID() const {return mIID;}
    const char* GetInterfaceName();
    const char* GetMemberName(const XPCNativeMemberDescriptor* desc) const;
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    XPCJSRuntime* GetRuntime() const {return mRuntime;}
    nsIXPCScriptable* GetArbitraryScriptable() const
        {return mRuntime->GetXPConnect()->GetArbitraryScriptable();}

    JSObject* NewInstanceJSObject(XPCContext* xpcc,
                                  JSObject* aGlobalObject,
                                  nsXPCWrappedNative* self);
    static nsXPCWrappedNative* GetWrappedNativeOfJSObject(JSContext* cx,
                                                          JSObject* jsobj);

    int GetMemberCount() const {return mMemberCount;}
    const XPCNativeMemberDescriptor* GetMemberDescriptor(uint16 i) const
    {
        NS_PRECONDITION(i < mMemberCount,"bad index");
        return mDescriptors ? &mDescriptors[i] : nsnull;
    }

    const XPCNativeMemberDescriptor* LookupMemberByID(jsid id) const;

#ifdef XPC_DETECT_LEADING_UPPERCASE_ACCESS_ERRORS
    // This will try to find a member that is of the form "camelCased"
    // but was accessed from JS using "CamelCased". This is here to catch
    // mistakes caused by the confusion magnet that JS methods are by 
    // convention 'foo' while C++ members are by convention 'Foo'.
    void HandlePossibleNameCaseError(JSContext* cx, jsid id);

#define  HANDLE_POSSIBLE_NAME_CASE_ERROR(_cx, _clazz,_id) \
                    _clazz->HandlePossibleNameCaseError(_cx, _id)
#else
#define  HANDLE_POSSIBLE_NAME_CASE_ERROR(_cx, _clazz,_id) ((void)0)
#endif

    static JSBool GetConstantAsJSVal(JSContext *cx,
                                     nsIInterfaceInfo* iinfo,
                                     PRUint16 index,
                                     jsval* vp);

    JSBool GetConstantAsJSVal(JSContext* cx,
                              nsXPCWrappedNative* wrapper,
                              const XPCNativeMemberDescriptor* desc,
                              jsval* vp)
        {return GetConstantAsJSVal(cx, mInfo, desc->index, vp);}

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    JSBool CallWrappedMethod(JSContext* cx,
                             nsXPCWrappedNative* wrapper,
                             const XPCNativeMemberDescriptor* desc,
                             CallMode callMode,
                             uintN argc, jsval *argv, jsval *vp);

    JSBool GetAttributeAsJSVal(JSContext* cx,
                               nsXPCWrappedNative* wrapper,
                               const XPCNativeMemberDescriptor* desc,
                               jsval* vp)
    {
        return CallWrappedMethod(cx, wrapper, desc, CALL_GETTER, 0, nsnull, vp);
    }

    JSBool SetAttributeFromJSVal(JSContext* cx,
                                 nsXPCWrappedNative* wrapper,
                                 const XPCNativeMemberDescriptor* desc,
                                 jsval* vp)
    {
        return CallWrappedMethod(cx, wrapper, desc, CALL_SETTER, 1, vp, nsnull);
    }

    JSObject* NewFunObj(JSContext *cx, JSObject *obj,
                        const XPCNativeMemberDescriptor* desc);

    JSBool DynamicEnumerate(nsXPCWrappedNative* wrapper,
                            nsIXPCScriptable* ds,
                            nsIXPCScriptable* as,
                            JSContext *cx, JSObject *obj,
                            JSIterateOp enum_op,
                            jsval *statep, jsid *idp);

    JSBool StaticEnumerate(nsXPCWrappedNative* wrapper,
                           JSIterateOp enum_op,
                           jsval *statep, jsid *idp);

    virtual ~nsXPCWrappedNativeClass();

private:
    nsXPCWrappedNativeClass();   // not implemented
    nsXPCWrappedNativeClass(XPCContext* xpcc, 
                            REFNSIID aIID,
                            nsIInterfaceInfo* aInfo);

    void ThrowBadResultException(JSContext* cx,
                                 const XPCNativeMemberDescriptor* desc,
                                 nsresult result)
        {nsXPConnect::GetJSThrower()->
                ThrowBadResultException(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE,
                                        cx, this, desc, result);}

    void ThrowBadParamException(uintN errNum,
                                JSContext* cx,
                                const XPCNativeMemberDescriptor* desc,
                                uintN paramNum)
        {nsXPConnect::GetJSThrower()->
                ThrowBadParamException(errNum, cx, this, desc, paramNum);}

    void ThrowException(uintN errNum,
                        JSContext* cx,
                        const XPCNativeMemberDescriptor* desc)
        {nsXPConnect::GetJSThrower()->
                ThrowException(errNum, cx, this, desc);}

    JSBool BuildMemberDescriptors(XPCContext* xpcc);
    void  DestroyMemberDescriptors();

    
    enum SizeMode {GET_SIZE, GET_LENGTH};

    JSBool GetArraySizeFromParam(JSContext* cx, 
                                 const nsXPTMethodInfo* method,
                                 const XPCNativeMemberDescriptor* desc,
                                 const nsXPTParamInfo& param,
                                 uint16 vtblIndex,
                                 uint8 paramIndex,
                                 SizeMode mode,
                                 nsXPTCVariant* dispatchParams,
                                 JSUint32* result);

    JSBool GetInterfaceTypeFromParam(JSContext* cx, 
                                     const nsXPTMethodInfo* method,
                                     const XPCNativeMemberDescriptor* desc,
                                     const nsXPTParamInfo& param,
                                     uint16 vtblIndex,
                                     uint8 paramIndex,
                                     const nsXPTType& datum_type,
                                     nsXPTCVariant* dispatchParams,
                                     nsID** result);

private:
    XPCJSRuntime* mRuntime;
    nsIID mIID;
    char* mName;
    nsIInterfaceInfo* mInfo;
    int mMemberCount;
    XPCNativeMemberDescriptor* mDescriptors;
};

/*************************/

class nsXPCWrappedNative : public nsIXPConnectWrappedNative
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDNATIVE

    // non-interface implementation

public:
    /* 
    *This is rarely called directly. Instead one usually calls
    * XPCConvert::NativeInterface2JSObject which will handles cases where the
    * xpcom object is already a wrapped JS or a DOM object. 
    */
    static nsXPCWrappedNative* 
    GetNewOrUsedWrapper(XPCContext* xpcc,
                        nsXPCWrappedNativeScope* aScope,
                        JSObject* aGlobalObject,
                        nsISupports* aObj,
                        REFNSIID aIID,
                        nsresult* pErr);
    nsISupports* GetNative() const {return mObj;}
    JSObject* GetJSObject() const {return mJSObj;}
    nsXPCWrappedNativeClass* GetClass() const {return mClass;}
    nsXPCWrappedNativeScope* GetScope() const {return mScope;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}

    nsIXPCScriptable* GetDynamicScriptable() const
        {return mRoot->mDynamicScriptable;}

    JSUint32 GetDynamicScriptableFlags() const
        {return mRoot->mDynamicScriptableFlags;}

    nsIXPCScriptable* GetArbitraryScriptable() const
        {return GetClass()->GetArbitraryScriptable();}

    JSBool IsValid() const {return mObj != nsnull;}
    void SystemIsBeingShutDown();

    void JSObjectFinalized(JSContext *cx, JSObject *obj);

    virtual ~nsXPCWrappedNative();
private:
    nsXPCWrappedNative();    // not implemented
    nsXPCWrappedNative(XPCContext* xpcc,
                       nsISupports* aObj,
                       nsXPCWrappedNativeScope* aScope,
                       JSObject* aGlobalObject,
                       nsXPCWrappedNativeClass* aClass,
                       nsXPCWrappedNative* root);

    nsXPCWrappedNative* Find(REFNSIID aIID);

private:
    nsISupports* mObj;
    JSObject* mJSObj;
    nsXPCWrappedNativeClass* mClass;
    nsXPCWrappedNativeScope* mScope;
    nsIXPCScriptable* mDynamicScriptable;   // only set in root!
    JSUint32 mDynamicScriptableFlags;   // only set in root!
    nsXPCWrappedNative* mRoot;
    nsXPCWrappedNative* mNext;
    nsIXPConnectFinalizeListener* mFinalizeListener;
};

/*************************/

class nsXPCJSObjectHolder : public nsIXPConnectJSObjectHolder
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER

    // non-interface implementation

public:
    static nsXPCJSObjectHolder* newHolder(JSContext* cx, JSObject* obj);

    virtual ~nsXPCJSObjectHolder();

private:
    nsXPCJSObjectHolder(JSContext* cx, JSObject* obj);
    nsXPCJSObjectHolder(); // not implemented

    JSRuntime* mRuntime;
    JSObject* mJSObj;
};

/***************************************************************************/
// data convertion

// class here just for static methods
class XPCConvert
{
public:
    static JSBool IsMethodReflectable(const nsXPTMethodInfo& info);

    static JSBool NativeData2JS(JSContext* cx, jsval* d, const void* s,
                                const nsXPTType& type, const nsID* iid,
                                JSObject* scope, nsresult* pErr);

    static JSBool JSData2Native(JSContext* cx, void* d, jsval s,
                                const nsXPTType& type,
                                JSBool useAllocator, const nsID* iid,
                                nsresult* pErr);

    static JSBool NativeInterface2JSObject(JSContext* cx,
                                           nsIXPConnectJSObjectHolder** dest,
                                           nsISupports* src,
                                           const nsID* iid,
                                           JSObject* scope, nsresult* pErr);

    static JSBool JSObject2NativeInterface(JSContext* cx,
                                           void** dest, JSObject* src,
                                           const nsID* iid, nsresult* pErr);

    static JSBool NativeArray2JS(JSContext* cx,
                                 jsval* d, const void** s,
                                 const nsXPTType& type, const nsID* iid,
                                 JSUint32 count, JSObject* scope, 
                                 nsresult* pErr);

    static JSBool JSArray2Native(JSContext* cx, void** d, jsval s,
                                 JSUint32 count, JSUint32 capacity,
                                 const nsXPTType& type,
                                 JSBool useAllocator, const nsID* iid,
                                 uintN* pErr);

    static JSBool NativeStringWithSize2JS(JSContext* cx,
                                          jsval* d, const void* s,
                                          const nsXPTType& type, 
                                          JSUint32 count,
                                          nsresult* pErr);

    static JSBool JSStringWithSize2Native(JSContext* cx, void* d, jsval s,
                                          JSUint32 count, JSUint32 capacity,
                                          const nsXPTType& type,
                                          JSBool useAllocator,
                                          uintN* pErr);

    static nsIXPCException* JSValToXPCException(JSContext* cx,
                                                jsval s,
                                                const char* ifaceName,
                                                const char* methodName);

    static nsIXPCException* JSErrorToXPCException(JSContext* cx,
                                                  const char* message,
                                                  const char* ifaceName,
                                                  const char* methodName,
                                                  const JSErrorReport* report);

    static nsIXPCException* ConstructException(nsresult rv, const char* message,
                               const char* ifaceName, const char* methodName,
                               nsISupports* data);

private:
    XPCConvert(); // not implemented
};

extern JSBool JS_DLL_CALLBACK
XPC_JSArgumentFormatter(JSContext *cx, const char *format,
                        JSBool fromJS, jsval **vpp, va_list *app);

/***************************************************************************/

class nsXPCArbitraryScriptable : public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    nsXPCArbitraryScriptable();
};

/***************************************************************************/

class XPCJSStack
{
public:
    static nsIJSStackFrameLocation* CreateStack(JSContext* cx);

    static nsIJSStackFrameLocation* CreateStackFrameLocation(
                                        JSBool isJSFrame,
                                        const char* aFilename,
                                        const char* aFunctionName,
                                        PRInt32 aLineNumber,
                                        nsIJSStackFrameLocation* aCaller);
private:
    XPCJSStack();   // not implemented
};

/***************************************************************************/

class nsXPCException : public nsIXPCException
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCEXCEPTION_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCEXCEPTION

    static nsXPCException* NewException(const char *aMessage,
                                        nsresult aResult,
                                        nsIJSStackFrameLocation *aLocation,
                                        nsISupports *aData);

    static JSBool NameAndFormatForNSResult(nsresult rv,
                                           const char** name,
                                           const char** format);
    static void* IterateNSResults(nsresult* rv,
                                  const char** name,
                                  const char** format,
                                  void** iterp);
    nsXPCException();
    virtual ~nsXPCException();

protected:
    void Reset();
private:
    char*                       mMessage;
    nsresult                    mResult;
    char*                       mName;
    nsIJSStackFrameLocation*    mLocation;
    nsISupports*                mData;
    PRBool                      mInitialized;
};

/***************************************************************************/

extern JSClass WrappedNative_class;
extern JSClass WrappedNativeWithCall_class;

extern JSBool JS_DLL_CALLBACK
WrappedNative_CallMethod(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv, jsval *vp);

extern JSBool xpc_InitWrappedNativeJSOps();

/***************************************************************************/
/*
* nsJSID implements nsIJSID. It is also used by nsJSIID and nsJSCID as a
* member (as a hidden implementaion detail) to which they delegate many calls.
*/

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
{
public:
    NS_DECL_ISUPPORTS

    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID
    XPC_DECLARE_IXPCSCRIPTABLE

    static nsJSIID* NewID(const char* str);

    nsJSIID();
    virtual ~nsJSIID();

private:
    void ResolveName();

    void FillCache(JSContext *cx, JSObject *obj,
                   nsIXPConnectWrappedNative *wrapper,
                   nsIXPCScriptable *arbitrary);

private:
    nsJSID mDetails;
    JSBool mCacheFilled;
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
    XPC_DECLARE_IXPCSCRIPTABLE

    static nsJSCID* NewID(const char* str);

    nsJSCID();
    virtual ~nsJSCID();

private:
    void ResolveName();

private:
    nsJSID mDetails;
};


/***************************************************************************/

// All of our thread local storage.

class xpcPerThreadData
{
public:
    // Get the instance of this object for the current thread
    static xpcPerThreadData* GetData();
    static void CleanupAllThreads();

    ~xpcPerThreadData();

    nsIXPCException* GetException();
    void             SetException(nsIXPCException* aException);

    JSContext*       GetSafeJSContext();
    nsresult         SetSafeJSContext(JSContext *cx);
    nsDeque*         GetJSContextStack();

    PRBool IsValid() const;

    void Cleanup();

private:
    xpcPerThreadData();

private:
    nsIXPCException*  mException;
    nsDeque*          mJSContextStack;
    JSContext*        mSafeJSContext;

    // If if non-null, we own it; same as mSafeJSContext if SetSafeJSContext
    // not called.
    JSContext*        mOwnSafeJSContext;
    xpcPerThreadData* mNextThread;

    static PRLock*           gLock;
    static xpcPerThreadData* gThreads;
    static PRUintn           gTLSIndex;

};

/**************************************************************/

#define NS_XPC_THREAD_JSCONTEXT_STACK_CID  \
{ 0xff8c4d10, 0x3194, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

class nsXPCThreadJSContextStackImpl : public nsIThreadJSContextStack
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSCONTEXTSTACK
    NS_DECL_NSITHREADJSCONTEXTSTACK

    static nsXPCThreadJSContextStackImpl* GetSingleton();
    static void FreeSingleton();

    nsXPCThreadJSContextStackImpl();
    virtual ~nsXPCThreadJSContextStackImpl();

private:
    nsDeque* GetStackForCurrentThread()
        {xpcPerThreadData* data = xpcPerThreadData::GetData();
         return data ? data->GetJSContextStack() : nsnull;}
};

/***************************************************************************/
#define NS_JS_RUNTIME_SERVICE_CID \
{0xb5e65b52, 0x1dd1, 0x11b2, \
    { 0xae, 0x8f, 0xf0, 0x92, 0x8e, 0xd8, 0x84, 0x82 }}

class nsJSRuntimeServiceImpl : public nsIJSRuntimeService
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSRUNTIMESERVICE
        
    static nsJSRuntimeServiceImpl* GetSingleton();
    static void FreeSingleton();

    nsJSRuntimeServiceImpl();
    virtual ~nsJSRuntimeServiceImpl();
 protected:
    JSRuntime *mRuntime;
};

/***************************************************************************/
// 'Components' object

class nsXPCComponents : public nsIXPCComponents, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS
    XPC_DECLARE_IXPCSCRIPTABLE

public:
    static JSBool
    AttachNewComponentsObject(XPCContext* xpcc, 
                              JSObject* obj);

    virtual ~nsXPCComponents();

private:
    nsXPCComponents();

private:
    nsXPCComponents_Interfaces*  mInterfaces;
    nsXPCComponents_Classes*     mClasses;
    nsXPCComponents_ClassesByID* mClassesByID;
    nsXPCComponents_Results*     mResults;
    nsXPCComponents_ID*          mID;
    nsXPCComponents_Exception*   mException;
    nsXPCComponents_Constructor* mConstructor;
    PRBool                       mCreating;
};

/***************************************************************************/

extern JSObject*
xpc_NewIDObject(JSContext *cx, JSObject* jsobj, const nsID& aID);

extern nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj);

/***************************************************************************/
// in xpcdebug.cpp

extern JSBool 
xpc_DumpJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals, 
                JSBool showThisProps);

extern JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, JSUint32 frameno, const char* text);

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
// the include of declarations of the maps comes last because they have
// inlines which call methods on classes above.

#include "xpcmaps.h"

#endif /* xpcprivate_h___ */
