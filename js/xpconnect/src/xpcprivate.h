/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * XPConnect allows JS code to manipulate C++ object and C++ code to manipulate
 * JS objects. JS manipulation of C++ objects tends to be significantly more
 * complex. This comment explains how it is orchestrated by XPConnect.
 *
 * For each C++ object to be manipulated in JS, there is a corresponding JS
 * object. This is called the "flattened JS object". By default, there is an
 * additional C++ object involved of type XPCWrappedNative. The XPCWrappedNative
 * holds pointers to the C++ object and the flat JS object.
 *
 * As an optimization, some C++ objects don't have XPCWrappedNatives, although
 * they still have a corresponding flattened JS object. These are called "slim
 * wrappers": all the wrapping information is stored in extra fields of the C++
 * object and the JS object. Slim wrappers are only used for DOM objects. As a
 * deoptimization, slim wrappers can be "morphed" into XPCWrappedNatives if the
 * extra fields of the XPCWrappedNative become necessary.
 *
 * All XPCWrappedNative objects belong to an XPCWrappedNativeScope. These scopes
 * are essentially in 1:1 correspondence with JS global objects. The
 * XPCWrappedNativeScope has a pointer to the JS global object. The parent of a
 * flattened JS object is, by default, the global JS object corresponding to the
 * wrapper's XPCWrappedNativeScope (the exception to this rule is when a
 * PreCreate hook asks for a different parent; see nsIXPCScriptable below).
 *
 * Some C++ objects (notably DOM objects) have information associated with them
 * that lists the interfaces implemented by these objects. A C++ object exposes
 * this information by implementing nsIClassInfo. If a C++ object implements
 * nsIClassInfo, then JS code can call its methods without needing to use
 * QueryInterface first. Typically, all instances of a C++ class share the same
 * nsIClassInfo instance. (That is, obj->QueryInterface(nsIClassInfo) returns
 * the same result for every obj of a given class.)
 *
 * XPConnect tracks nsIClassInfo information in an XPCWrappedNativeProto object.
 * A given XPCWrappedNativeScope will have one XPCWrappedNativeProto for each
 * nsIClassInfo instance being used. The XPCWrappedNativeProto has an associated
 * JS object, which is used as the prototype of all flattened JS objects created
 * for C++ objects with the given nsIClassInfo.
 *
 * Each XPCWrappedNativeProto has a pointer to its XPCWrappedNativeScope. If an
 * XPCWrappedNative wraps a C++ object with class info, then it points to its
 * XPCWrappedNativeProto. Otherwise it points to its XPCWrappedNativeScope. (The
 * pointers are smooshed together in a tagged union.) Either way it can reach
 * its scope.
 *
 * In the case of slim wrappers (where there is no XPCWrappedNative), the
 * flattened JS object has a pointer to the XPCWrappedNativeProto stored in a
 * reserved slot.
 *
 * An XPCWrappedNativeProto keeps track of the set of interfaces implemented by
 * the C++ object in an XPCNativeSet. (The list of interfaces is obtained by
 * calling a method on the nsIClassInfo.) An XPCNativeSet is a collection of
 * XPCNativeInterfaces. Each interface stores the list of members, which can be
 * methods, constants, getters, or setters.
 *
 * An XPCWrappedNative also points to an XPCNativeSet. Initially this starts out
 * the same as the XPCWrappedNativeProto's set. If there is no proto, it starts
 * out as a singleton set containing nsISupports. If JS code QI's new interfaces
 * outside of the existing set, the set will grow. All QueryInterface results
 * are cached in XPCWrappedNativeTearOff objects, which are linked off of the
 * XPCWrappedNative.
 *
 * Besides having class info, a C++ object may be "scriptable" (i.e., implement
 * nsIXPCScriptable). This allows it to implement a more DOM-like interface,
 * besides just exposing XPCOM methods and constants. An nsIXPCScriptable
 * instance has hooks that correspond to all the normal JSClass hooks. Each
 * nsIXPCScriptable instance is mirrored by an XPCNativeScriptableInfo in
 * XPConnect. These can have pointers from XPCWrappedNativeProto and
 * XPCWrappedNative (since C++ objects can have scriptable info without having
 * class info).
 *
 * Most data in an XPCNativeScriptableInfo is shared between instances. The
 * shared data is stored in an XPCNativeScriptableShared object. This type is
 * important because it holds the JSClass of the flattened JS objects with the
 * given scriptable info.
 */

/* All the XPConnect private declarations - only include locally. */

#ifndef xpcprivate_h___
#define xpcprivate_h___

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/Util.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "xpcpublic.h"
#include "jsapi.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "prprf.h"
#include "jsdbgapi.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jswrapper.h"
#include "nscore.h"
#include "nsXPCOM.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDebug.h"
#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsIClassInfoImpl.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsISupportsPrimitives.h"
#include "nsMemory.h"
#include "nsIXPConnect.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIXPCScriptable.h"
#include "nsIXPCSecurityManager.h"
#include "nsIJSRuntimeService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsXPTCUtils.h"
#include "xptinfo.h"
#include "XPCForwards.h"
#include "XPCLog.h"
#include "xpccomponents.h"
#include "xpcexception.h"
#include "xpcjsid.h"
#include "prlong.h"
#include "prmem.h"
#include "prenv.h"
#include "prclist.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsAutoJSValHolder.h"

#include "js/HashTable.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Mutex.h"

#include "nsThreadUtils.h"
#include "nsIJSContextStack.h"
#include "nsIJSEngineTelemetryStats.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIExceptionService.h"

#include "nsVariant.h"
#include "nsIPropertyBag.h"
#include "nsIProperty.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsWrapperCache.h"
#include "nsStringBuffer.h"
#include "nsDataHashtable.h"
#include "nsDeque.h"

#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

#include "nsIXPCScriptNotify.h"  // used to notify: ScriptEvaluated

#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsISecurityCheckedComponent.h"
#include "xpcObjectHelper.h"
#include "nsIThreadInternal.h"

#ifdef XP_WIN
// Nasty MS defines
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#ifdef GetClassName
#undef GetClassName
#endif
#endif /* XP_WIN */

#include "nsINode.h"

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
#define XPC_WRAPPER_MAP_SIZE                16

/***************************************************************************/
// data declarations...
extern const char XPC_CONTEXT_STACK_CONTRACTID[];
extern const char XPC_RUNTIME_CONTRACTID[];
extern const char XPC_EXCEPTION_CONTRACTID[];
extern const char XPC_CONSOLE_CONTRACTID[];
extern const char XPC_SCRIPT_ERROR_CONTRACTID[];
extern const char XPC_ID_CONTRACTID[];
extern const char XPC_XPCONNECT_CONTRACTID[];

typedef js::HashSet<JSCompartment *,
                    js::DefaultHasher<JSCompartment *>,
                    js::SystemAllocPolicy> XPCCompartmentSet;

typedef XPCCompartmentSet::Range XPCCompartmentRange;

/***************************************************************************/
// Useful macros...

#define XPC_STRING_GETTER_BODY(dest, src)                                     \
    NS_ENSURE_ARG_POINTER(dest);                                              \
    char* result;                                                             \
    if (src)                                                                  \
        result = (char*) nsMemory::Clone(src,                                 \
                                         sizeof(char)*(strlen(src)+1));       \
    else                                                                      \
        result = nullptr;                                                      \
    *dest = result;                                                           \
    return (result || !src) ? NS_OK : NS_ERROR_OUT_OF_MEMORY


#define WRAPPER_SLOTS (JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | \
                       JSCLASS_HAS_RESERVED_SLOTS(1))

// WRAPPER_MULTISLOT is defined in xpcpublic.h

#define INVALID_OBJECT ((JSObject *)1)

inline void SetSlimWrapperProto(JSObject *obj, XPCWrappedNativeProto *proto)
{
    JS_SetReservedSlot(obj, WRAPPER_MULTISLOT, PRIVATE_TO_JSVAL(proto));
}

inline XPCWrappedNativeProto* GetSlimWrapperProto(JSObject *obj)
{
    MOZ_ASSERT(IS_SLIM_WRAPPER(obj));
    const JS::Value &v = js::GetReservedSlot(obj, WRAPPER_MULTISLOT);
    return static_cast<XPCWrappedNativeProto*>(v.toPrivate());
}

// A slim wrapper is identified by having a native pointer in its reserved slot.
// This function, therefore, does the official transition from a slim wrapper to
// a non-slim wrapper.
inline void MorphMultiSlot(JSObject *obj)
{
    MOZ_ASSERT(IS_SLIM_WRAPPER(obj));
    JS_SetReservedSlot(obj, WRAPPER_MULTISLOT, JSVAL_NULL);
    MOZ_ASSERT(!IS_SLIM_WRAPPER(obj));
}

inline void SetExpandoChain(JSObject *obj, JSObject *chain)
{
    MOZ_ASSERT(IS_WN_WRAPPER(obj));
    JS_SetReservedSlot(obj, WRAPPER_MULTISLOT, JS::ObjectOrNullValue(chain));
}

inline JSObject* GetExpandoChain(JSObject *obj)
{
    MOZ_ASSERT(IS_WN_WRAPPER(obj));
    return JS_GetReservedSlot(obj, WRAPPER_MULTISLOT).toObjectOrNull();
}

/***************************************************************************/
// Auto locking support class...

// We PROMISE to never screw this up.
#ifdef _MSC_VER
#pragma warning(disable : 4355) // OK to pass "this" in member initializer
#endif

typedef mozilla::ReentrantMonitor XPCLock;

static inline void xpc_Wait(XPCLock* lock)
    {
        NS_ASSERTION(lock, "xpc_Wait called with null lock!");
        lock->Wait();
    }

static inline void xpc_NotifyAll(XPCLock* lock)
    {
        NS_ASSERTION(lock, "xpc_NotifyAll called with null lock!");
        lock->NotifyAll();
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

class NS_STACK_CLASS XPCAutoLock {
public:

    static XPCLock* NewLock(const char* name)
                        {return new mozilla::ReentrantMonitor(name);}
    static void     DestroyLock(XPCLock* lock)
                        {delete lock;}

    XPCAutoLock(XPCLock* lock MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mLock(lock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (mLock)
            mLock->Enter();
    }

    ~XPCAutoLock()
    {
        if (mLock) {
            mLock->Exit();
        }
    }

private:
    XPCLock*  mLock;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

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
        return nullptr;
    }
    static void operator delete(void* /*memory*/) {}
};

/************************************************/

class NS_STACK_CLASS XPCAutoUnlock {
public:
    XPCAutoUnlock(XPCLock* lock MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mLock(lock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (mLock) {
            mLock->Exit();
        }
    }

    ~XPCAutoUnlock()
    {
        if (mLock)
            mLock->Enter();
    }

private:
    XPCLock*  mLock;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

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
        return nullptr;
    }
    static void operator delete(void* /*memory*/) {}
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

// JSTRACE_XML can recursively hold on to more JSTRACE_XML objects, adding it to
// the cycle collector avoids stack overflow.
inline bool
AddToCCKind(JSGCTraceKind kind)
{
    return kind == JSTRACE_OBJECT || kind == JSTRACE_XML || kind == JSTRACE_SCRIPT;
}

class nsXPConnect : public nsIXPConnect,
                    public nsIThreadObserver,
                    public nsSupportsWeakReference,
                    public nsCycleCollectionJSRuntime,
                    public nsIJSRuntimeService,
                    public nsIThreadJSContextStack,
                    public nsIJSEngineTelemetryStats
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECT
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIJSRUNTIMESERVICE
    NS_DECL_NSIJSCONTEXTSTACK
    NS_DECL_NSITHREADJSCONTEXTSTACK
    NS_DECL_NSIJSENGINETELEMETRYSTATS

    // non-interface implementation
public:
    // These get non-addref'd pointers
    static nsXPConnect*  GetXPConnect();
    static nsXPConnect*  FastGetXPConnect() { return gSelf ? gSelf : GetXPConnect(); }
    static XPCJSRuntime* GetRuntimeInstance();
    XPCJSRuntime* GetRuntime() {return mRuntime;}

    // Gets addref'd pointer
    static nsresult GetInterfaceInfoManager(nsIInterfaceInfoSuperManager** iim,
                                            nsXPConnect* xpc = nullptr);

    static JSBool IsISupportsDescendant(nsIInterfaceInfo* info);

    nsIXPCSecurityManager* GetDefaultSecurityManager() const
    {
        // mDefaultSecurityManager is main-thread only.
        if (!NS_IsMainThread()) {
            return nullptr;
        }
        return mDefaultSecurityManager;
    }

    PRUint16 GetDefaultSecurityManagerFlags() const
        {return mDefaultSecurityManagerFlags;}

    // This returns an AddRef'd pointer. It does not do this with an 'out' param
    // only because this form is required by the generic module macro:
    // NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR
    static nsXPConnect* GetSingleton();

    // Called by module code in dll startup
    static void InitStatics() { gSelf = nullptr; gOnceAliveNowDead = false; }
    // Called by module code on dll shutdown.
    static void ReleaseXPConnectSingleton();

    virtual ~nsXPConnect();

    JSBool IsShuttingDown() const {return mShuttingDown;}

    void EnsureGCBeforeCC() { mNeedGCBeforeCC = true; }
    void ClearGCBeforeCC() { mNeedGCBeforeCC = false; }

    nsresult GetInfoForIID(const nsIID * aIID, nsIInterfaceInfo** info);
    nsresult GetInfoForName(const char * name, nsIInterfaceInfo** info);

    // nsCycleCollectionLanguageRuntime
    virtual bool NotifyLeaveMainThread();
    virtual void NotifyEnterCycleCollectionThread();
    virtual void NotifyLeaveCycleCollectionThread();
    virtual void NotifyEnterMainThread();
    virtual nsresult BeginCycleCollection(nsCycleCollectionTraversalCallback &cb);
    virtual nsresult FinishTraverse();
    virtual nsCycleCollectionParticipant *GetParticipant();
    virtual bool NeedCollect();
    virtual void Collect(PRUint32 reason);

    XPCCallContext *GetCycleCollectionContext()
    {
        return mCycleCollectionContext;
    }

    unsigned GetOutstandingRequests(JSContext* cx);

    // This returns the singleton nsCycleCollectionParticipant for JSContexts.
    static nsCycleCollectionParticipant *JSContextParticipant();

    virtual nsIPrincipal* GetPrincipal(JSObject* obj,
                                       bool allowShortCircuit) const;

    void RecordTraversal(void *p, nsISupports *s);
    virtual char* DebugPrintJSStack(bool showArgs,
                                    bool showLocals,
                                    bool showThisProps);


    static bool ReportAllJSExceptions()
    {
      return gReportAllJSExceptions > 0;
    }

    static void CheckForDebugMode(JSRuntime *rt);

protected:
    nsXPConnect();

private:
    static PRThread* FindMainThread();

private:
    // Singleton instance
    static nsXPConnect*      gSelf;
    static JSBool            gOnceAliveNowDead;

    XPCJSRuntime*            mRuntime;
    nsCOMPtr<nsIInterfaceInfoSuperManager> mInterfaceInfoManager;
    nsIXPCSecurityManager*   mDefaultSecurityManager;
    PRUint16                 mDefaultSecurityManagerFlags;
    JSBool                   mShuttingDown;
    JSBool                   mNeedGCBeforeCC;

    // nsIThreadInternal doesn't remember which observers it called
    // OnProcessNextEvent on when it gets around to calling AfterProcessNextEvent.
    // So if XPConnect gets initialized mid-event (which can happen), we'll get
    // an 'after' notification without getting an 'on' notification. If we don't
    // watch out for this, we'll do an unmatched |pop| on the context stack.
    PRUint16                   mEventDepth;
    nsAutoPtr<XPCCallContext> mCycleCollectionContext;

    typedef nsBaseHashtable<nsPtrHashKey<void>, nsISupports*, nsISupports*> ScopeSet;
    ScopeSet mScopes;
    nsCOMPtr<nsIXPCScriptable> mBackstagePass;

    static PRUint32 gReportAllJSExceptions;
    static JSBool gDebugMode;
    static JSBool gDesiredDebugMode;

public:
    static nsIScriptSecurityManager *gScriptSecurityManager;
};

/***************************************************************************/

class XPCRootSetElem
{
public:
    XPCRootSetElem()
    {
#ifdef DEBUG
        mNext = nullptr;
        mSelfp = nullptr;
#endif
    }

    ~XPCRootSetElem()
    {
        NS_ASSERTION(!mNext, "Must be unlinked");
        NS_ASSERTION(!mSelfp, "Must be unlinked");
    }

    inline XPCRootSetElem* GetNextRoot() { return mNext; }
    void AddToRootSet(XPCLock *lock, XPCRootSetElem **listHead);
    void RemoveFromRootSet(XPCLock *lock);

private:
    XPCRootSetElem *mNext;
    XPCRootSetElem **mSelfp;
};

/***************************************************************************/

// In the current xpconnect system there can only be one XPCJSRuntime.
// So, xpconnect can only be used on one JSRuntime within the process.

// no virtuals. no refcounting.
class XPCJSContextStack;
class XPCIncrementalReleaseRunnable;
class XPCJSRuntime
{
public:
    static XPCJSRuntime* newXPCJSRuntime(nsXPConnect* aXPConnect);
    static XPCJSRuntime* Get() { return nsXPConnect::GetXPConnect()->GetRuntime(); }

    JSRuntime*     GetJSRuntime() const {return mJSRuntime;}
    nsXPConnect*   GetXPConnect() const {return mXPConnect;}

    XPCJSContextStack* GetJSContextStack() {return mJSContextStack;}
    void DestroyJSContextStack();

    JSContext*     GetJSCycleCollectionContext();

    XPCCallContext*  GetCallContext() const {return mCallContext;}
    XPCCallContext*  SetCallContext(XPCCallContext* ccx)
        {XPCCallContext* old = mCallContext; mCallContext = ccx; return old;}

    jsid GetResolveName() const {return mResolveName;}
    jsid SetResolveName(jsid name)
        {jsid old = mResolveName; mResolveName = name; return old;}

    XPCWrappedNative* GetResolvingWrapper() const {return mResolvingWrapper;}
    XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w)
        {XPCWrappedNative* old = mResolvingWrapper;
         mResolvingWrapper = w; return old;}

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

    XPCCompartmentSet& GetCompartmentSet()
        {return mCompartmentSet;}

    XPCLock* GetMapLock() const {return mMapLock;}

    JSBool OnJSContextNew(JSContext* cx);

    bool DeferredRelease(nsISupports* obj);

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
        IDX_PROTO                   ,
        IDX_ITERATOR                ,
        IDX_EXPOSEDPROPS            ,
        IDX_SCRIPTONLY              ,
        IDX_BASEURIOBJECT           ,
        IDX_NODEPRINCIPAL           ,
        IDX_DOCUMENTURIOBJECT       ,
        IDX_TOTAL_COUNT // just a count of the above
    };

    jsid GetStringID(unsigned index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrIDs[index];
    }
    jsval GetStringJSVal(unsigned index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrJSVals[index];
    }
    const char* GetStringName(unsigned index) const
    {
        NS_ASSERTION(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    static void TraceBlackJS(JSTracer* trc, void* data);
    static void TraceGrayJS(JSTracer* trc, void* data);
    void TraceXPConnectRoots(JSTracer *trc);
    void AddXPConnectRoots(nsCycleCollectionTraversalCallback& cb);
    void UnmarkSkippableJSHolders();

    static void GCCallback(JSRuntime *rt, JSGCStatus status);
    static void FinalizeCallback(JSFreeOp *fop, JSFinalizeStatus status, JSBool isCompartmentGC);

    inline void AddVariantRoot(XPCTraceableVariant* variant);
    inline void AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS);
    inline void AddObjectHolderRoot(XPCJSObjectHolder* holder);

    nsresult AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
    nsresult RemoveJSHolder(void* aHolder);
    nsresult TestJSHolder(void* aHolder, bool* aRetval);

    static void SuspectWrappedNative(XPCWrappedNative *wrapper,
                                     nsCycleCollectionTraversalCallback &cb);

    void DebugDump(PRInt16 depth);

    void SystemIsBeingShutDown();

    PRThread* GetThreadRunningGC() const {return mThreadRunningGC;}

    ~XPCJSRuntime();

    nsresult GetPendingException(nsIException** aException)
    {
        if (EnsureExceptionManager())
            return mExceptionManager->GetCurrentException(aException);
        nsCOMPtr<nsIException> out = mPendingException;
        out.forget(aException);
        return NS_OK;
    }

    nsresult SetPendingException(nsIException* aException)
    {
        if (EnsureExceptionManager())
            return mExceptionManager->SetCurrentException(aException);

        mPendingException = aException;
        return NS_OK;
    }

    nsIExceptionManager* GetExceptionManager()
    {
        if (EnsureExceptionManager())
            return mExceptionManager;
        return nullptr;
    }

    bool EnsureExceptionManager()
    {
        if (mExceptionManager)
            return true;

        if (mExceptionManagerNotAvailable)
            return false;

        nsCOMPtr<nsIExceptionService> xs =
            do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
        if (xs)
            xs->GetCurrentExceptionManager(getter_AddRefs(mExceptionManager));
        if (mExceptionManager)
            return true;

        mExceptionManagerNotAvailable = true;
        return false;
    }


#ifdef XPC_CHECK_WRAPPERS_AT_SHUTDOWN
   void DEBUG_AddWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {XPCAutoLock lock(GetMapLock());
         JSDHashEntryHdr *entry =
            JS_DHashTableOperate(DEBUG_WrappedNativeHashtable,
                                 wrapper, JS_DHASH_ADD);
         if (entry) ((JSDHashEntryStub *)entry)->key = wrapper;}

   void DEBUG_RemoveWrappedNative(nsIXPConnectWrappedNative* wrapper)
        {XPCAutoLock lock(GetMapLock());
         JS_DHashTableOperate(DEBUG_WrappedNativeHashtable,
                              wrapper, JS_DHASH_REMOVE);}
private:
   JSDHashTable* DEBUG_WrappedNativeHashtable;
public:
#endif

    void AddGCCallback(JSGCCallback cb);
    void RemoveGCCallback(JSGCCallback cb);

    static void ActivityCallback(void *arg, JSBool active);

    bool NewDOMBindingsEnabled()
    {
        return gNewDOMBindingsEnabled;
    }

    bool ExperimentalBindingsEnabled()
    {
        return gExperimentalBindingsEnabled;
    }

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    AutoMarkingPtr**  GetAutoRootsAdr() {return &mAutoRoots;}

private:
    XPCJSRuntime(); // no implementation
    XPCJSRuntime(nsXPConnect* aXPConnect);

    // The caller must be holding the GC lock
    void RescheduleWatchdog(XPCContext* ccx);

    static void WatchdogMain(void *arg);

    void ReleaseIncrementally(nsTArray<nsISupports *> &array);

    static bool gNewDOMBindingsEnabled;
    static bool gExperimentalBindingsEnabled;

    static const char* mStrings[IDX_TOTAL_COUNT];
    jsid mStrIDs[IDX_TOTAL_COUNT];
    jsval mStrJSVals[IDX_TOTAL_COUNT];

    nsXPConnect*             mXPConnect;
    JSRuntime*               mJSRuntime;
    XPCJSContextStack*       mJSContextStack;
    JSContext*               mJSCycleCollectionContext;
    XPCCallContext*          mCallContext;
    AutoMarkingPtr*          mAutoRoots;
    jsid                     mResolveName;
    XPCWrappedNative*        mResolvingWrapper;
    JSObject2WrappedJSMap*   mWrappedJSMap;
    IID2WrappedJSClassMap*   mWrappedJSClassMap;
    IID2NativeInterfaceMap*  mIID2NativeInterfaceMap;
    ClassInfo2NativeSetMap*  mClassInfo2NativeSetMap;
    NativeSetMap*            mNativeSetMap;
    IID2ThisTranslatorMap*   mThisTranslatorMap;
    XPCNativeScriptableSharedMap* mNativeScriptableSharedMap;
    XPCWrappedNativeProtoMap* mDyingWrappedNativeProtoMap;
    XPCWrappedNativeProtoMap* mDetachedWrappedNativeProtoMap;
    XPCCompartmentSet        mCompartmentSet;
    XPCLock* mMapLock;
    PRThread* mThreadRunningGC;
    nsTArray<nsXPCWrappedJS*> mWrappedJSToReleaseArray;
    nsTArray<nsISupports*> mNativesToReleaseArray;
    JSBool mDoingFinalization;
    XPCRootSetElem *mVariantRoots;
    XPCRootSetElem *mWrappedJSRoots;
    XPCRootSetElem *mObjectHolderRoots;
    JSDHashTable mJSHolders;
    PRLock *mWatchdogLock;
    PRCondVar *mWatchdogWakeup;
    PRThread *mWatchdogThread;
    nsTArray<JSGCCallback> extraGCCallbacks;
    bool mWatchdogHibernating;
    PRTime mLastActiveTime; // -1 if active NOW
    XPCIncrementalReleaseRunnable *mReleaseRunnable;

    nsCOMPtr<nsIException>   mPendingException;
    nsCOMPtr<nsIExceptionManager> mExceptionManager;
    bool mExceptionManagerNotAvailable;

    friend class AutoLockWatchdog;
    friend class XPCIncrementalReleaseRunnable;
};

/***************************************************************************/
/***************************************************************************/
// XPCContext is mostly a dumb class to hold JSContext specific data and
// maps that let us find wrappers created for the given JSContext.

// no virtuals
class XPCContext
{
    friend class XPCJSRuntime;
public:
    static XPCContext* GetXPCContext(JSContext* aJSContext)
        {
            NS_ASSERTION(JS_GetSecondContextPrivate(aJSContext), "should already have XPCContext");
            return static_cast<XPCContext *>(JS_GetSecondContextPrivate(aJSContext));
        }

    XPCJSRuntime* GetRuntime() const {return mRuntime;}
    JSContext* GetJSContext() const {return mJSContext;}

    enum LangType {LANG_UNKNOWN, LANG_JS, LANG_NATIVE};

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
            if (!CallerTypeIsJavaScript())
                return nullptr;
            if (mSecurityManager) {
                if (flags & mSecurityManagerFlags)
                    return mSecurityManager;
            } else {
                nsIXPCSecurityManager* mgr;
                nsXPConnect* xpc = mRuntime->GetXPConnect();
                mgr = xpc->GetDefaultSecurityManager();
                if (mgr && (flags & xpc->GetDefaultSecurityManagerFlags()))
                    return mgr;
            }
            return nullptr;
        }

    void DebugDump(PRInt16 depth);
    void AddScope(PRCList *scope) { PR_INSERT_AFTER(scope, &mScopes); }
    void RemoveScope(PRCList *scope) { PR_REMOVE_LINK(scope); }

    ~XPCContext();

private:
    XPCContext();    // no implementation
    XPCContext(XPCJSRuntime* aRuntime, JSContext* aJSContext);

    static XPCContext* newXPCContext(XPCJSRuntime* aRuntime,
                                     JSContext* aJSContext);
private:
    XPCJSRuntime* mRuntime;
    JSContext*  mJSContext;
    nsresult mLastResult;
    nsresult mPendingResult;
    nsIXPCSecurityManager* mSecurityManager;
    nsIException* mException;
    LangType mCallingLangType;
    PRUint16 mSecurityManagerFlags;

    // A linked list of scopes to notify when we are destroyed.
    PRCList mScopes;
};

/***************************************************************************/

#define NATIVE_CALLER  XPCContext::LANG_NATIVE
#define JS_CALLER      XPCContext::LANG_JS

// class to export a JSString as an const nsAString, no refcounting :(
class XPCReadableJSStringWrapper : public nsDependentString
{
public:
    typedef nsDependentString::char_traits char_traits;

    XPCReadableJSStringWrapper(const PRUnichar *chars, size_t length) :
        nsDependentString(chars, length)
    { }

    XPCReadableJSStringWrapper() :
        nsDependentString(char_traits::sEmptyBuffer, char_traits::sEmptyBuffer)
    { SetIsVoid(true); }

    JSBool init(JSContext* aContext, JSString* str)
    {
        size_t length;
        const jschar* chars = JS_GetStringCharsZAndLength(aContext, str, &length);
        if (!chars)
            return false;

        NS_ASSERTION(IsEmpty(), "init() on initialized string");
        new(static_cast<nsDependentString *>(this)) nsDependentString(chars, length);
        return true;
    }
};

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

class XPCCallContext : public nsAXPCNativeCallContext
{
public:
    NS_IMETHOD GetCallee(nsISupports **aResult);
    NS_IMETHOD GetCalleeMethodIndex(PRUint16 *aResult);
    NS_IMETHOD GetCalleeWrapper(nsIXPConnectWrappedNative **aResult);
    NS_IMETHOD GetJSContext(JSContext **aResult);
    NS_IMETHOD GetArgc(PRUint32 *aResult);
    NS_IMETHOD GetArgvPtr(jsval **aResult);
    NS_IMETHOD GetCalleeInterface(nsIInterfaceInfo **aResult);
    NS_IMETHOD GetCalleeClassInfo(nsIClassInfo **aResult);
    NS_IMETHOD GetPreviousCallContext(nsAXPCNativeCallContext **aResult);
    NS_IMETHOD GetLanguage(PRUint16 *aResult);

    enum {NO_ARGS = (unsigned) -1};

    XPCCallContext(XPCContext::LangType callerLanguage,
                   JSContext* cx    = nullptr,
                   JSObject* obj    = nullptr,
                   JSObject* funobj = nullptr,
                   jsid id          = JSID_VOID,
                   unsigned argc       = NO_ARGS,
                   jsval *argv      = nullptr,
                   jsval *rval      = nullptr);

    virtual ~XPCCallContext();

    inline JSBool                       IsValid() const ;

    inline nsXPConnect*                 GetXPConnect() const ;
    inline XPCJSRuntime*                GetRuntime() const ;
    inline XPCContext*                  GetXPCContext() const ;
    inline JSContext*                   GetJSContext() const ;
    inline JSBool                       GetContextPopRequired() const ;
    inline XPCContext::LangType         GetCallerLanguage() const ;
    inline XPCContext::LangType         GetPrevCallerLanguage() const ;
    inline XPCCallContext*              GetPrevCallContext() const ;

    /*
     * The 'scope for new JSObjects' will be the scope for objects created when
     * carrying out a JS/C++ call. This member is only available if HAVE_SCOPE.
     * The object passed to the ccx constructor is used as the scope for new
     * JSObjects. However, this object is also queried for a wrapper, so
     * clients that don't want a wrapper (and thus pass NULL to the ccx
     * constructor) need to manually call SetScopeForNewJSObjects.
     */
    inline JSObject*                    GetScopeForNewJSObjects() const ;
    inline void                         SetScopeForNewJSObjects(JSObject *obj) ;

    inline JSObject*                    GetFlattenedJSObject() const ;
    inline nsISupports*                 GetIdentityObject() const ;
    inline XPCWrappedNative*            GetWrapper() const ;
    inline XPCWrappedNativeProto*       GetProto() const ;

    inline JSBool                       CanGetTearOff() const ;
    inline XPCWrappedNativeTearOff*     GetTearOff() const ;

    inline XPCNativeScriptableInfo*     GetScriptableInfo() const ;
    inline JSBool                       CanGetSet() const ;
    inline XPCNativeSet*                GetSet() const ;
    inline JSBool                       CanGetInterface() const ;
    inline XPCNativeInterface*          GetInterface() const ;
    inline XPCNativeMember*             GetMember() const ;
    inline JSBool                       HasInterfaceAndMember() const ;
    inline jsid                         GetName() const ;
    inline JSBool                       GetStaticMemberIsLocal() const ;
    inline unsigned                        GetArgc() const ;
    inline jsval*                       GetArgv() const ;
    inline jsval*                       GetRetVal() const ;

    inline PRUint16                     GetMethodIndex() const ;
    inline void                         SetMethodIndex(PRUint16 index) ;

    inline JSBool   GetDestroyJSContextInDestructor() const;
    inline void     SetDestroyJSContextInDestructor(JSBool b);

    inline jsid GetResolveName() const;
    inline jsid SetResolveName(jsid name);

    inline XPCWrappedNative* GetResolvingWrapper() const;
    inline XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w);

    inline void SetRetVal(jsval val);

    void SetName(jsid name);
    void SetArgsAndResultPtr(unsigned argc, jsval *argv, jsval *rval);
    void SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                     JSBool isSetter);

    nsresult  CanCallNow();

    void SystemIsBeingShutDown();

    operator JSContext*() const {return GetJSContext();}

    XPCReadableJSStringWrapper *NewStringWrapper(const PRUnichar *str, PRUint32 len);
    void DeleteString(nsAString *string);

private:

    // no copy ctor or assignment allowed
    XPCCallContext(const XPCCallContext& r); // not implemented
    XPCCallContext& operator= (const XPCCallContext& r); // not implemented

    friend class XPCLazyCallContext;
    XPCCallContext(XPCContext::LangType callerLanguage,
                   JSContext* cx,
                   JSBool callBeginRequest,
                   JSObject* obj,
                   JSObject* flattenedJSObject,
                   XPCWrappedNative* wn,
                   XPCWrappedNativeTearOff* tearoff);

    enum WrapperInitOptions {
        WRAPPER_PASSED_TO_CONSTRUCTOR,
        INIT_SHOULD_LOOKUP_WRAPPER
    };

    void Init(XPCContext::LangType callerLanguage,
              JSBool callBeginRequest,
              JSObject* obj,
              JSObject* funobj,
              WrapperInitOptions wrapperInitOptions,
              jsid name,
              unsigned argc,
              jsval *argv,
              jsval *rval);

private:
    // posible values for mState
    enum State {
        INIT_FAILED,
        SYSTEM_SHUTDOWN,
        HAVE_CONTEXT,
        HAVE_SCOPE,
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

    XPCContext*                     mXPCContext;
    JSContext*                      mJSContext;
    JSBool                          mContextPopRequired;
    JSBool                          mDestroyJSContextInDestructor;

    XPCContext::LangType            mCallerLanguage;

    // ctor does not necessarily init the following. BEWARE!

    XPCContext::LangType            mPrevCallerLanguage;

    XPCCallContext*                 mPrevCallContext;

    JSObject*                       mScopeForNewJSObjects;
    JSObject*                       mFlattenedJSObject;
    XPCWrappedNative*               mWrapper;
    XPCWrappedNativeTearOff*        mTearOff;

    XPCNativeScriptableInfo*        mScriptableInfo;

    XPCNativeSet*                   mSet;
    XPCNativeInterface*             mInterface;
    XPCNativeMember*                mMember;

    jsid                            mName;
    JSBool                          mStaticMemberIsLocal;

    unsigned                           mArgc;
    jsval*                          mArgv;
    jsval*                          mRetVal;

    PRUint16                        mMethodIndex;

#define XPCCCX_STRING_CACHE_SIZE 2

    // String wrapper entry, holds a string, and a boolean that tells
    // whether the string is in use or not.
    //
    // NB: The string is not stored by value so that we avoid the cost of
    // construction/destruction.
    struct StringWrapperEntry
    {
        StringWrapperEntry() : mInUse(false) { }

        js::AlignedStorage2<XPCReadableJSStringWrapper> mString;
        bool mInUse;
    };

    StringWrapperEntry mScratchStrings[XPCCCX_STRING_CACHE_SIZE];
};

class XPCLazyCallContext
{
public:
    XPCLazyCallContext(XPCCallContext& ccx)
        : mCallBeginRequest(DONT_CALL_BEGINREQUEST),
          mCcx(&ccx),
          mCcxToDestroy(nullptr)
#ifdef DEBUG
          , mCx(nullptr)
          , mCallerLanguage(JS_CALLER)
          , mObj(nullptr)
          , mFlattenedJSObject(nullptr)
          , mWrapper(nullptr)
          , mTearOff(nullptr)
#endif
    {
    }
    XPCLazyCallContext(XPCContext::LangType callerLanguage, JSContext* cx,
                       JSObject* obj = nullptr,
                       JSObject* flattenedJSObject = nullptr,
                       XPCWrappedNative* wrapper = nullptr,
                       XPCWrappedNativeTearOff* tearoff = nullptr)
        : mCallBeginRequest(callerLanguage == NATIVE_CALLER ?
                            CALL_BEGINREQUEST : DONT_CALL_BEGINREQUEST),
          mCcx(nullptr),
          mCcxToDestroy(nullptr),
          mCx(cx),
          mCallerLanguage(callerLanguage),
          mObj(obj),
          mFlattenedJSObject(flattenedJSObject),
          mWrapper(wrapper),
          mTearOff(tearoff)
    {
        NS_ASSERTION(cx, "Need a JS context!");
        NS_ASSERTION(callerLanguage == NATIVE_CALLER ||
                     callerLanguage == JS_CALLER,
                     "Can't deal with unknown caller language!");
#ifdef DEBUG
        AssertContextIsTopOfStack(cx);
#endif
    }
    ~XPCLazyCallContext()
    {
        if (mCcxToDestroy)
            mCcxToDestroy->~XPCCallContext();
        else if (mCallBeginRequest == CALLED_BEGINREQUEST)
            JS_EndRequest(mCx);
    }
    void SetWrapper(XPCWrappedNative* wrapper,
                    XPCWrappedNativeTearOff* tearoff);
    void SetWrapper(JSObject* flattenedJSObject);

    JSContext *GetJSContext()
    {
        if (mCcx)
            return mCcx->GetJSContext();

        if (mCallBeginRequest == CALL_BEGINREQUEST) {
            JS_BeginRequest(mCx);
            mCallBeginRequest = CALLED_BEGINREQUEST;
        }

        return mCx;
    }
    JSObject *GetScopeForNewJSObjects() const
    {
        if (mCcx)
            return mCcx->GetScopeForNewJSObjects();

        return xpc_UnmarkGrayObject(mObj);
    }
    void SetScopeForNewJSObjects(JSObject *obj)
    {
        if (mCcx) {
            mCcx->SetScopeForNewJSObjects(obj);
            return;
        }
        NS_ABORT_IF_FALSE(!mObj, "already set!");
        mObj = obj;
    }
    JSObject *GetFlattenedJSObject() const
    {
        if (mCcx)
            return mCcx->GetFlattenedJSObject();

        return xpc_UnmarkGrayObject(mFlattenedJSObject);
    }
    XPCCallContext &GetXPCCallContext()
    {
        if (!mCcx) {
            XPCCallContext *data = mData.addr();
            mCcxToDestroy = mCcx =
                new (data) XPCCallContext(mCallerLanguage, mCx,
                                          mCallBeginRequest == CALL_BEGINREQUEST,
                                           xpc_UnmarkGrayObject(mObj),
                                           xpc_UnmarkGrayObject(mFlattenedJSObject),
                                           mWrapper,
                                          mTearOff);
            if (!mCcx->IsValid()) {
                NS_ERROR("This is not supposed to fail!");
            }
        }

        return *mCcx;
    }

private:
#ifdef DEBUG
    static void AssertContextIsTopOfStack(JSContext* cx);
#endif

    enum {
        DONT_CALL_BEGINREQUEST,
        CALL_BEGINREQUEST,
        CALLED_BEGINREQUEST
    } mCallBeginRequest;

    XPCCallContext *mCcx;
    XPCCallContext *mCcxToDestroy;
    JSContext *mCx;
    XPCContext::LangType mCallerLanguage;
    JSObject *mObj;
    JSObject *mFlattenedJSObject;
    XPCWrappedNative *mWrapper;
    XPCWrappedNativeTearOff *mTearOff;
    mozilla::AlignedStorage2<XPCCallContext> mData;
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

struct XPCWrappedNativeJSClass;
extern XPCWrappedNativeJSClass XPC_WN_NoHelper_JSClass;
extern js::Class XPC_WN_NoMods_WithCall_Proto_JSClass;
extern js::Class XPC_WN_NoMods_NoCall_Proto_JSClass;
extern js::Class XPC_WN_ModsAllowed_WithCall_Proto_JSClass;
extern js::Class XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
extern js::Class XPC_WN_Tearoff_JSClass;
extern js::Class XPC_WN_NoHelper_Proto_JSClass;

extern JSBool
XPC_WN_Equality(JSContext *cx, JSHandleObject obj, const jsval *v, JSBool *bp);

extern JSBool
XPC_WN_CallMethod(JSContext *cx, unsigned argc, jsval *vp);

extern JSBool
XPC_WN_GetterSetter(JSContext *cx, unsigned argc, jsval *vp);

extern JSBool
XPC_WN_JSOp_Enumerate(JSContext *cx, JSHandleObject obj, JSIterateOp enum_op,
                      jsval *statep, jsid *idp);

extern JSType
XPC_WN_JSOp_TypeOf_Object(JSContext *cx, JSHandleObject obj);

extern JSType
XPC_WN_JSOp_TypeOf_Function(JSContext *cx, JSHandleObject obj);

extern void
XPC_WN_JSOp_Clear(JSContext *cx, JSHandleObject obj);

extern JSObject*
XPC_WN_JSOp_ThisObject(JSContext *cx, JSHandleObject obj);

// Macros to initialize Object or Function like XPC_WN classes
#define XPC_WN_WithCall_ObjectOps                                             \
    {                                                                         \
        nullptr, /* lookupGeneric */                                           \
        nullptr, /* lookupProperty */                                          \
        nullptr, /* lookupElement */                                           \
        nullptr, /* lookupSpecial */                                           \
        nullptr, /* defineGeneric */                                           \
        nullptr, /* defineProperty */                                          \
        nullptr, /* defineElement */                                           \
        nullptr, /* defineSpecial */                                           \
        nullptr, /* getGeneric    */                                           \
        nullptr, /* getProperty    */                                          \
        nullptr, /* getElement    */                                           \
        nullptr, /* getElementIfPresent */                                     \
        nullptr, /* getSpecial    */                                           \
        nullptr, /* setGeneric    */                                           \
        nullptr, /* setProperty    */                                          \
        nullptr, /* setElement    */                                           \
        nullptr, /* setSpecial    */                                           \
        nullptr, /* getGenericAttributes  */                                   \
        nullptr, /* getAttributes  */                                          \
        nullptr, /* getElementAttributes  */                                   \
        nullptr, /* getSpecialAttributes  */                                   \
        nullptr, /* setGenericAttributes  */                                   \
        nullptr, /* setAttributes  */                                          \
        nullptr, /* setElementAttributes  */                                   \
        nullptr, /* setSpecialAttributes  */                                   \
        nullptr, /* deleteProperty */                                          \
        nullptr, /* deleteElement */                                           \
        nullptr, /* deleteSpecial */                                           \
        XPC_WN_JSOp_Enumerate,                                                \
        XPC_WN_JSOp_TypeOf_Function,                                          \
        XPC_WN_JSOp_ThisObject,                                               \
        XPC_WN_JSOp_Clear                                                     \
    }

#define XPC_WN_NoCall_ObjectOps                                               \
    {                                                                         \
        nullptr, /* lookupGeneric */                                           \
        nullptr, /* lookupProperty */                                          \
        nullptr, /* lookupElement */                                           \
        nullptr, /* lookupSpecial */                                           \
        nullptr, /* defineGeneric */                                           \
        nullptr, /* defineProperty */                                          \
        nullptr, /* defineElement */                                           \
        nullptr, /* defineSpecial */                                           \
        nullptr, /* getGeneric    */                                           \
        nullptr, /* getProperty    */                                          \
        nullptr, /* getElement    */                                           \
        nullptr, /* getElementIfPresent */                                     \
        nullptr, /* getSpecial    */                                           \
        nullptr, /* setGeneric    */                                           \
        nullptr, /* setProperty    */                                          \
        nullptr, /* setElement    */                                           \
        nullptr, /* setSpecial    */                                           \
        nullptr, /* getGenericAttributes  */                                   \
        nullptr, /* getAttributes  */                                          \
        nullptr, /* getElementAttributes  */                                   \
        nullptr, /* getSpecialAttributes  */                                   \
        nullptr, /* setGenericAttributes  */                                   \
        nullptr, /* setAttributes  */                                          \
        nullptr, /* setElementAttributes  */                                   \
        nullptr, /* setSpecialAttributes  */                                   \
        nullptr, /* deleteProperty */                                          \
        nullptr, /* deleteElement */                                           \
        nullptr, /* deleteSpecial */                                           \
        XPC_WN_JSOp_Enumerate,                                                \
        XPC_WN_JSOp_TypeOf_Object,                                            \
        XPC_WN_JSOp_ThisObject,                                               \
        XPC_WN_JSOp_Clear                                                     \
    }

// Maybe this macro should check for class->enumerate ==
// XPC_WN_Shared_Proto_Enumerate or something rather than checking for
// 4 classes?
static inline bool IS_PROTO_CLASS(js::Class *clazz)
{
    return clazz == &XPC_WN_NoMods_WithCall_Proto_JSClass ||
           clazz == &XPC_WN_NoMods_NoCall_Proto_JSClass ||
           clazz == &XPC_WN_ModsAllowed_WithCall_Proto_JSClass ||
           clazz == &XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
}

/***************************************************************************/

namespace XPCWrapper {

enum WrapperType {
    UNKNOWN         = 0,
    NONE            = 0,
    XPCNW_IMPLICIT  = 1 << 0,
    XPCNW_EXPLICIT  = 1 << 1,
    XPCNW           = (XPCNW_IMPLICIT | XPCNW_EXPLICIT),
    SJOW            = 1 << 2,
    // SJOW must be the last wrapper type that can be returned to chrome.

    XOW             = 1 << 3,
    COW             = 1 << 4,
    SOW             = 1 << 5
};

}

/***************************************************************************/
// XPCWrappedNativeScope is one-to-one with a JS global object.

class XPCWrappedNativeScope : public PRCList
{
public:

    static XPCWrappedNativeScope*
    GetNewOrUsed(XPCCallContext& ccx, JSObject* aGlobal, nsISupports* aNative = nullptr);

    XPCJSRuntime*
    GetRuntime() const {return mRuntime;}

    Native2WrappedNativeMap*
    GetWrappedNativeMap() const {return mWrappedNativeMap;}

    ClassInfo2WrappedNativeProtoMap*
    GetWrappedNativeProtoMap(JSBool aMainThreadOnly) const
        {return aMainThreadOnly ?
                mMainThreadWrappedNativeProtoMap :
                mWrappedNativeProtoMap;}

    nsXPCComponents*
    GetComponents() const {return mComponents;}

    JSObject*
    GetGlobalJSObject() const
        {return xpc_UnmarkGrayObject(mGlobalJSObject);}

    JSObject*
    GetGlobalJSObjectPreserveColor() const {return mGlobalJSObject;}

    JSObject*
    GetPrototypeJSObject() const
        {return xpc_UnmarkGrayObject(mPrototypeJSObject);}

    JSObject*
    GetPrototypeJSObjectPreserveColor() const {return mPrototypeJSObject;}

    // Getter for the prototype that we use for wrappers that have no
    // helper.
    JSObject*
    GetPrototypeNoHelper(XPCCallContext& ccx);

    nsIPrincipal*
    GetPrincipal() const
    {return mScriptObjectPrincipal ?
         mScriptObjectPrincipal->GetPrincipal() : nullptr;}

    void RemoveWrappedNativeProtos();

    static XPCWrappedNativeScope*
    FindInJSObjectScope(JSContext* cx, JSObject* obj,
                        JSBool OKIfNotInitialized = false,
                        XPCJSRuntime* runtime = nullptr);

    static XPCWrappedNativeScope*
    FindInJSObjectScope(XPCCallContext& ccx, JSObject* obj,
                        JSBool OKIfNotInitialized = false)
    {
        return FindInJSObjectScope(ccx, obj, OKIfNotInitialized,
                                   ccx.GetRuntime());
    }

    static void
    SystemIsBeingShutDown();

    static void
    TraceWrappedNativesInAllScopes(JSTracer* trc, XPCJSRuntime* rt);

    void TraceSelf(JSTracer *trc) {
        JSObject *obj = GetGlobalJSObjectPreserveColor();
        MOZ_ASSERT(obj);
        JS_CALL_OBJECT_TRACER(trc, obj, "XPCWrappedNativeScope::mGlobalJSObject");

        JSObject *proto = GetPrototypeJSObjectPreserveColor();
        if (proto)
            JS_CALL_OBJECT_TRACER(trc, proto, "XPCWrappedNativeScope::mPrototypeJSObject");
    }

    static void
    SuspectAllWrappers(XPCJSRuntime* rt, nsCycleCollectionTraversalCallback &cb);

    static void
    StartFinalizationPhaseOfGC(JSFreeOp *fop, XPCJSRuntime* rt);

    static void
    FinishedFinalizationPhaseOfGC();

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

    static size_t
    SizeOfAllScopesIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    size_t
    SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

    JSBool
    IsValid() const {return mRuntime != nullptr;}

    static JSBool
    IsDyingScope(XPCWrappedNativeScope *scope);

    void SetComponents(nsXPCComponents* aComponents);
    nsXPCComponents *GetComponents();
    void SetGlobal(XPCCallContext& ccx, JSObject* aGlobal, nsISupports* aNative);

    static void InitStatics() { gScopes = nullptr; gDyingScopes = nullptr; }

    XPCContext *GetContext() { return mContext; }
    void ClearContext() { mContext = nullptr; }

    nsDataHashtable<nsDepCharHashKey, JSObject*>& GetCachedDOMPrototypes()
    {
        return mCachedDOMPrototypes;
    }

    static XPCWrappedNativeScope *GetNativeScope(JSObject *obj)
    {
        MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_XPCONNECT_GLOBAL);

        const js::Value &v = js::GetObjectSlot(obj, JSCLASS_GLOBAL_SLOT_COUNT);
        return v.isUndefined()
               ? nullptr
               : static_cast<XPCWrappedNativeScope *>(v.toPrivate());
    }
    void TraceDOMPrototypes(JSTracer *trc);

    JSBool NewDOMBindingsEnabled()
    {
        return mNewDOMBindingsEnabled;
    }

    JSBool ExperimentalBindingsEnabled()
    {
        return mExperimentalBindingsEnabled;
    }

protected:
    XPCWrappedNativeScope(XPCCallContext& ccx, JSObject* aGlobal, nsISupports* aNative);
    virtual ~XPCWrappedNativeScope();

    static void KillDyingScopes();

    XPCWrappedNativeScope(); // not implemented

private:
    static XPCWrappedNativeScope* gScopes;
    static XPCWrappedNativeScope* gDyingScopes;

    XPCJSRuntime*                    mRuntime;
    Native2WrappedNativeMap*         mWrappedNativeMap;
    ClassInfo2WrappedNativeProtoMap* mWrappedNativeProtoMap;
    ClassInfo2WrappedNativeProtoMap* mMainThreadWrappedNativeProtoMap;
    nsRefPtr<nsXPCComponents>        mComponents;
    XPCWrappedNativeScope*           mNext;
    // The JS global object for this scope.  If non-null, this will be the
    // default parent for the XPCWrappedNatives that have us as the scope,
    // unless a PreCreate hook overrides it.  Note that this _may_ be null (see
    // constructor).
    js::ObjectPtr                    mGlobalJSObject;

    // Cached value of Object.prototype
    js::ObjectPtr                    mPrototypeJSObject;
    // Prototype to use for wrappers with no helper.
    JSObject*                        mPrototypeNoHelper;

    XPCContext*                      mContext;

    // The script object principal instance corresponding to our current global
    // JS object.
    // XXXbz what happens if someone calls JS_SetPrivate on mGlobalJSObject.
    // How do we deal?  Do we need to?  I suspect this isn't worth worrying
    // about, since all of our scope objects are verified as not doing that.
    nsIScriptObjectPrincipal* mScriptObjectPrincipal;

    nsDataHashtable<nsDepCharHashKey, JSObject*> mCachedDOMPrototypes;

    JSBool mNewDOMBindingsEnabled;
    JSBool mExperimentalBindingsEnabled;
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

    jsid   GetName() const {return mName;}

    PRUint16 GetIndex() const {return mIndex;}

    JSBool GetConstantValue(XPCCallContext& ccx, XPCNativeInterface* iface,
                            jsval* pval)
        {NS_ASSERTION(IsConstant(),
                      "Only call this if you're sure this is a constant!");
         return Resolve(ccx, iface, nullptr, pval);}

    JSBool NewFunctionObject(XPCCallContext& ccx, XPCNativeInterface* iface,
                             JSObject *parent, jsval* pval);

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


    void SetName(jsid a) {mName = a;}

    void SetMethod(PRUint16 index)
        {mFlags = METHOD; mIndex = index;}

    void SetConstant(PRUint16 index)
        {mFlags = CONSTANT; mIndex = index;}

    void SetReadOnlyAttribute(PRUint16 index)
        {mFlags = GETTER; mIndex = index;}

    void SetWritableAttribute()
        {NS_ASSERTION(mFlags == GETTER,"bad"); mFlags = GETTER | SETTER_TOO;}

    /* default ctor - leave random contents */
    XPCNativeMember()  {MOZ_COUNT_CTOR(XPCNativeMember);}
    ~XPCNativeMember() {MOZ_COUNT_DTOR(XPCNativeMember);}

private:
    JSBool Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                   JSObject *parent, jsval *vp);

    enum {
        METHOD      = 0x01,
        CONSTANT    = 0x02,
        GETTER      = 0x04,
        SETTER_TOO  = 0x08
    };

private:
    // our only data...
    jsid     mName;
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
    inline jsid              GetName()          const {return mName;}

    inline const nsIID* GetIID() const;
    inline const char*  GetNameString() const;
    inline XPCNativeMember* FindMember(jsid name) const;

    inline JSBool HasAncestor(const nsIID* iid) const;

    PRUint16 GetMemberCount() const {
        return mMemberCount;
    }
    XPCNativeMember* GetMemberAt(PRUint16 i) {
        NS_ASSERTION(i < mMemberCount, "bad index");
        return &mMembers[i];
    }

    void DebugDump(PRInt16 depth);

#define XPC_NATIVE_IFACE_MARK_FLAG ((PRUint16)JS_BIT(15)) // only high bit of 16 is set

    void Mark() {
        mMarked = 1;
    }

    void Unmark() {
        mMarked = 0;
    }

    bool IsMarked() const {
        return mMarked != 0;
    }

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void TraceJS(JSTracer* trc) {}
    inline void AutoTrace(JSTracer* trc) {}

    static void DestroyInstance(XPCNativeInterface* inst);

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

  protected:
    static XPCNativeInterface* NewInstance(XPCCallContext& ccx,
                                           nsIInterfaceInfo* aInfo);

    XPCNativeInterface();   // not implemented
    XPCNativeInterface(nsIInterfaceInfo* aInfo, jsid aName)
      : mInfo(aInfo), mName(aName), mMemberCount(0), mMarked(0)
    {
        MOZ_COUNT_CTOR(XPCNativeInterface);
    }
    ~XPCNativeInterface() {
        MOZ_COUNT_DTOR(XPCNativeInterface);
    }

    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

    XPCNativeInterface(const XPCNativeInterface& r); // not implemented
    XPCNativeInterface& operator= (const XPCNativeInterface& r); // not implemented

private:
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    jsid                       mName;
    PRUint16                   mMemberCount : 15;
    PRUint16                   mMarked : 1;
    XPCNativeMember            mMembers[1]; // always last - object sized for array
};

/***************************************************************************/
// XPCNativeSetKey is used to key a XPCNativeSet in a NativeSetMap.

class XPCNativeSetKey
{
public:
    XPCNativeSetKey(XPCNativeSet*       BaseSet  = nullptr,
                    XPCNativeInterface* Addition = nullptr,
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

    // This generates a union set.
    //
    // If preserveFirstSetOrder is true, the elements from |firstSet| come first,
    // followed by any non-duplicate items from |secondSet|. If false, the same
    // algorithm is applied; but if we detect that |secondSet| is a superset of
    // |firstSet|, we return |secondSet| without worrying about whether the
    // ordering might differ from |firstSet|.
    static XPCNativeSet* GetNewOrUsed(XPCCallContext& ccx,
                                      XPCNativeSet* firstSet,
                                      XPCNativeSet* secondSet,
                                      bool preserveFirstSetOrder);

    static void ClearCacheEntryForClassInfo(nsIClassInfo* classInfo);

    inline JSBool FindMember(jsid name, XPCNativeMember** pMember,
                             PRUint16* pInterfaceIndex) const;

    inline JSBool FindMember(jsid name, XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface) const;

    inline JSBool FindMember(jsid name,
                             XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface,
                             XPCNativeSet* protoSet,
                             JSBool* pIsLocal) const;

    inline JSBool HasInterface(XPCNativeInterface* aInterface) const;
    inline JSBool HasInterfaceWithAncestor(XPCNativeInterface* aInterface) const;
    inline JSBool HasInterfaceWithAncestor(const nsIID* iid) const;

    inline XPCNativeInterface* FindInterfaceWithIID(const nsIID& iid) const;

    inline XPCNativeInterface* FindNamedInterface(jsid name) const;

    PRUint16 GetMemberCount() const {
        return mMemberCount;
    }
    PRUint16 GetInterfaceCount() const {
        return mInterfaceCount;
    }
    XPCNativeInterface **GetInterfaceArray() {
        return mInterfaces;
    }

    XPCNativeInterface* GetInterfaceAt(PRUint16 i)
        {NS_ASSERTION(i < mInterfaceCount, "bad index"); return mInterfaces[i];}

    inline JSBool MatchesSetUpToInterface(const XPCNativeSet* other,
                                          XPCNativeInterface* iface) const;

#define XPC_NATIVE_SET_MARK_FLAG ((PRUint16)JS_BIT(15)) // only high bit of 16 is set

    inline void Mark();

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void TraceJS(JSTracer* trc) {}
    inline void AutoTrace(JSTracer* trc) {}

  private:
    void MarkSelfOnly() {
        mMarked = 1;
    }

  public:
    void Unmark() {
        mMarked = 0;
    }
    bool IsMarked() const {
        return !!mMarked;
    }

#ifdef DEBUG
    inline void ASSERT_NotMarked();
#endif

    void DebugDump(PRInt16 depth);

    static void DestroyInstance(XPCNativeSet* inst);

    size_t SizeOfIncludingThis(nsMallocSizeOfFun mallocSizeOf);

  protected:
    static XPCNativeSet* NewInstance(XPCCallContext& ccx,
                                     XPCNativeInterface** array,
                                     PRUint16 count);
    static XPCNativeSet* NewInstanceMutate(XPCNativeSet*       otherSet,
                                           XPCNativeInterface* newInterface,
                                           PRUint16            position);
    XPCNativeSet()
      : mMemberCount(0), mInterfaceCount(0), mMarked(0)
    {
        MOZ_COUNT_CTOR(XPCNativeSet);
    }
    ~XPCNativeSet() {
        MOZ_COUNT_DTOR(XPCNativeSet);
    }
    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

  private:
    PRUint16                mMemberCount;
    PRUint16                mInterfaceCount : 15;
    PRUint16                mMarked : 1;
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
    uint32_t mFlags;

public:

    XPCNativeScriptableFlags(uint32_t flags = 0) : mFlags(flags) {}

    uint32_t GetFlags() const {return mFlags & ~XPC_WN_SJSFLAGS_MARK_FLAG;}
    void     SetFlags(uint32_t flags) {mFlags = flags;}

    operator uint32_t() const {return GetFlags();}

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
    JSBool WantEquality()                 GET_IT(WANT_EQUALITY)
    JSBool WantOuterObject()              GET_IT(WANT_OUTER_OBJECT)
    JSBool UseJSStubForAddProperty()      GET_IT(USE_JSSTUB_FOR_ADDPROPERTY)
    JSBool UseJSStubForDelProperty()      GET_IT(USE_JSSTUB_FOR_DELPROPERTY)
    JSBool UseJSStubForSetProperty()      GET_IT(USE_JSSTUB_FOR_SETPROPERTY)
    JSBool DontEnumStaticProps()          GET_IT(DONT_ENUM_STATIC_PROPS)
    JSBool DontEnumQueryInterface()       GET_IT(DONT_ENUM_QUERY_INTERFACE)
    JSBool DontAskInstanceForScriptable() GET_IT(DONT_ASK_INSTANCE_FOR_SCRIPTABLE)
    JSBool ClassInfoInterfacesOnly()      GET_IT(CLASSINFO_INTERFACES_ONLY)
    JSBool AllowPropModsDuringResolve()   GET_IT(ALLOW_PROP_MODS_DURING_RESOLVE)
    JSBool AllowPropModsToPrototype()     GET_IT(ALLOW_PROP_MODS_TO_PROTOTYPE)
    JSBool IsGlobalObject()               GET_IT(IS_GLOBAL_OBJECT)
    JSBool DontReflectInterfaceNames()    GET_IT(DONT_REFLECT_INTERFACE_NAMES)
    JSBool UseStubEqualityHook()          GET_IT(USE_STUB_EQUALITY_HOOK)

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

// We maintain the invariant that every JSClass for which ext.isWrappedNative
// is true is a contained in an instance of this struct, and can thus be cast
// to it.
struct XPCWrappedNativeJSClass
{
    js::Class base;
    PRUint32 interfacesBitmap;
};

class XPCNativeScriptableShared
{
public:
    const XPCNativeScriptableFlags& GetFlags() const {return mFlags;}
    PRUint32                        GetInterfacesBitmap() const
        {return mJSClass.interfacesBitmap;}
    JSClass*                        GetJSClass()
        {return Jsvalify(&mJSClass.base);}
    JSClass*                        GetSlimJSClass()
        {if (mCanBeSlim) return GetJSClass(); return nullptr;}

    XPCNativeScriptableShared(uint32_t aFlags, char* aName,
                              PRUint32 interfacesBitmap)
        : mFlags(aFlags),
          mCanBeSlim(false)
        {memset(&mJSClass, 0, sizeof(mJSClass));
         mJSClass.base.name = aName;  // take ownership
         mJSClass.interfacesBitmap = interfacesBitmap;
         MOZ_COUNT_CTOR(XPCNativeScriptableShared);}

    ~XPCNativeScriptableShared()
        {if (mJSClass.base.name)nsMemory::Free((void*)mJSClass.base.name);
         MOZ_COUNT_DTOR(XPCNativeScriptableShared);}

    char* TransferNameOwnership()
        {char* name=(char*)mJSClass.base.name; mJSClass.base.name = nullptr;
        return name;}

    void PopulateJSClass();

    void Mark()       {mFlags.Mark();}
    void Unmark()     {mFlags.Unmark();}
    JSBool IsMarked() const {return mFlags.IsMarked();}

private:
    XPCNativeScriptableFlags mFlags;
    XPCWrappedNativeJSClass  mJSClass;
    JSBool                   mCanBeSlim;
};

/***************************************************************************/
// XPCNativeScriptableInfo is used to hold the nsIXPCScriptable state for a
// given class or instance.

class XPCNativeScriptableInfo
{
public:
    static XPCNativeScriptableInfo*
    Construct(XPCCallContext& ccx, const XPCNativeScriptableCreateInfo* sci);

    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mShared->GetFlags();}

    PRUint32
    GetInterfacesBitmap() const {return mShared->GetInterfacesBitmap();}

    JSClass*
    GetJSClass()          {return mShared->GetJSClass();}

    JSClass*
    GetSlimJSClass()      {return mShared->GetSlimJSClass();}

    XPCNativeScriptableShared*
    GetScriptableShared() {return mShared;}

    void
    SetCallback(nsIXPCScriptable* s) {mCallback = s;}
    void
    SetCallback(already_AddRefed<nsIXPCScriptable> s) {mCallback = s;}

    void
    SetScriptableShared(XPCNativeScriptableShared* shared) {mShared = shared;}

    void Mark() {
        if (mShared)
            mShared->Mark();
    }

    void TraceJS(JSTracer *trc) {}
    void AutoTrace(JSTracer *trc) {}

protected:
    XPCNativeScriptableInfo(nsIXPCScriptable* scriptable = nullptr,
                            XPCNativeScriptableShared* shared = nullptr)
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

class NS_STACK_CLASS XPCNativeScriptableCreateInfo
{
public:

    XPCNativeScriptableCreateInfo(const XPCNativeScriptableInfo& si)
        : mCallback(si.GetCallback()), mFlags(si.GetFlags()),
          mInterfacesBitmap(si.GetInterfacesBitmap()) {}

    XPCNativeScriptableCreateInfo(already_AddRefed<nsIXPCScriptable> callback,
                                  XPCNativeScriptableFlags flags,
                                  PRUint32 interfacesBitmap)
        : mCallback(callback), mFlags(flags),
          mInterfacesBitmap(interfacesBitmap) {}

    XPCNativeScriptableCreateInfo()
        : mFlags(0), mInterfacesBitmap(0) {}


    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mFlags;}

    PRUint32
    GetInterfacesBitmap() const     {return mInterfacesBitmap;}

    void
    SetCallback(already_AddRefed<nsIXPCScriptable> callback)
        {mCallback = callback;}

    void
    SetFlags(const XPCNativeScriptableFlags& flags)  {mFlags = flags;}

    void
    SetInterfacesBitmap(PRUint32 interfacesBitmap)
        {mInterfacesBitmap = interfacesBitmap;}

private:
    nsCOMPtr<nsIXPCScriptable>  mCallback;
    XPCNativeScriptableFlags    mFlags;
    PRUint32                    mInterfacesBitmap;
};

/***********************************************/
// XPCWrappedNativeProto hold the additional shared wrapper data
// for XPCWrappedNative whose native objects expose nsIClassInfo.

#define UNKNOWN_OFFSETS ((QITableEntry*)1)

class XPCWrappedNativeProto
{
public:
    static XPCWrappedNativeProto*
    GetNewOrUsed(XPCCallContext& ccx,
                 XPCWrappedNativeScope* scope,
                 nsIClassInfo* classInfo,
                 const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                 QITableEntry* offsets = UNKNOWN_OFFSETS,
                 bool callPostCreatePrototype = true);

    XPCWrappedNativeScope*
    GetScope()   const {return mScope;}

    XPCJSRuntime*
    GetRuntime() const {return mScope->GetRuntime();}

    JSObject*
    GetJSProtoObject() const {return xpc_UnmarkGrayObject(mJSProtoObject);}

    nsIClassInfo*
    GetClassInfo()     const {return mClassInfo;}

    XPCNativeSet*
    GetSet()           const {return mSet;}

    XPCNativeScriptableInfo*
    GetScriptableInfo()   {return mScriptableInfo;}

    void**
    GetSecurityInfoAddr() {return &mSecurityInfo;}

    uint32_t
    GetClassInfoFlags() const {return mClassInfoFlags;}

    QITableEntry*
    GetOffsets()
    {
        return InitedOffsets() ? mOffsets : nullptr;
    }
    QITableEntry*
    GetOffsetsMasked()
    {
        return mOffsets;
    }
    void
    CacheOffsets(nsISupports* identity)
    {
        static NS_DEFINE_IID(kThisPtrOffsetsSID, NS_THISPTROFFSETS_SID);

#ifdef DEBUG
        if (InitedOffsets() && mOffsets) {
            QITableEntry* offsets;
            identity->QueryInterface(kThisPtrOffsetsSID, (void**)&offsets);
            NS_ASSERTION(offsets == mOffsets,
                         "We can't deal with objects that have the same "
                         "classinfo but different offset tables.");
        }
#endif

        if (!InitedOffsets()) {
            if (mClassInfoFlags & nsIClassInfo::CONTENT_NODE) {
                identity->QueryInterface(kThisPtrOffsetsSID, (void**)&mOffsets);
            } else {
                mOffsets = nullptr;
            }
        }
    }

#ifdef GET_IT
#undef GET_IT
#endif
#define GET_IT(f_) const {return !!(mClassInfoFlags & nsIClassInfo:: f_ );}

    JSBool ClassIsSingleton()           GET_IT(SINGLETON)
    JSBool ClassIsThreadSafe()          GET_IT(THREADSAFE)
    JSBool ClassIsMainThreadOnly()      GET_IT(MAIN_THREAD_ONLY)
    JSBool ClassIsDOMObject()           GET_IT(DOM_OBJECT)
    JSBool ClassIsPluginObject()        GET_IT(PLUGIN_OBJECT)

#undef GET_IT

    XPCLock* GetLock() const
        {return ClassIsThreadSafe() ? GetRuntime()->GetMapLock() : nullptr;}

    void SetScriptableInfo(XPCNativeScriptableInfo* si)
        {NS_ASSERTION(!mScriptableInfo, "leak here!"); mScriptableInfo = si;}

    bool CallPostCreatePrototype(XPCCallContext& ccx);
    void JSProtoObjectFinalized(js::FreeOp *fop, JSObject *obj);

    void SystemIsBeingShutDown();

    void DebugDump(PRInt16 depth);

    void TraceSelf(JSTracer *trc) {
        if (mJSProtoObject)
            JS_CALL_OBJECT_TRACER(trc, mJSProtoObject, "XPCWrappedNativeProto::mJSProtoObject");
    }

    void TraceInside(JSTracer *trc) {
        if (JS_IsGCMarkingTracer(trc)) {
            mSet->Mark();
            if (mScriptableInfo)
                mScriptableInfo->Mark();
        }

        GetScope()->TraceSelf(trc);
    }

    void TraceJS(JSTracer *trc) {
        TraceSelf(trc);
        TraceInside(trc);
    }

    void WriteBarrierPre(JSRuntime* rt)
    {
        if (js::IsIncrementalBarrierNeeded(rt) && mJSProtoObject)
            mJSProtoObject.writeBarrierPre(rt);
    }

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void AutoTrace(JSTracer* trc) {}

    // Yes, we *do* need to mark the mScriptableInfo in both cases.
    void Mark() const
        {mSet->Mark();
         if (mScriptableInfo) mScriptableInfo->Mark();}

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
                          XPCNativeSet* Set,
                          QITableEntry* offsets);

    JSBool Init(XPCCallContext& ccx,
                const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                bool callPostCreatePrototype);

private:
#if defined(DEBUG_xpc_hacker) || defined(DEBUG)
    static PRInt32 gDEBUG_LiveProtoCount;
#endif

private:
    bool
    InitedOffsets()
    {
        return mOffsets != UNKNOWN_OFFSETS;
    }

    XPCWrappedNativeScope*   mScope;
    js::ObjectPtr            mJSProtoObject;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    PRUint32                 mClassInfoFlags;
    XPCNativeSet*            mSet;
    void*                    mSecurityInfo;
    XPCNativeScriptableInfo* mScriptableInfo;
    QITableEntry*            mOffsets;
};

class xpcObjectHelper;
extern JSBool ConstructSlimWrapper(XPCCallContext &ccx,
                                   xpcObjectHelper &aHelper,
                                   XPCWrappedNativeScope* xpcScope,
                                   jsval *rval);
extern JSBool MorphSlimWrapper(JSContext *cx, JSObject *obj);

/***********************************************/
// XPCWrappedNativeTearOff represents the info needed to make calls to one
// interface on the underlying native object of a XPCWrappedNative.

class XPCWrappedNativeTearOff
{
public:
    JSBool IsAvailable() const {return mInterface == nullptr;}
    JSBool IsReserved()  const {return mInterface == (XPCNativeInterface*)1;}
    JSBool IsValid()     const {return !IsAvailable() && !IsReserved();}
    void   SetReserved()       {mInterface = (XPCNativeInterface*)1;}

    XPCNativeInterface* GetInterface() const {return mInterface;}
    nsISupports*        GetNative()    const {return mNative;}
    JSObject*           GetJSObject();
    JSObject*           GetJSObjectPreserveColor() const;
    void SetInterface(XPCNativeInterface*  Interface) {mInterface = Interface;}
    void SetNative(nsISupports*  Native)              {mNative = Native;}
    void SetJSObject(JSObject*  JSObj);

    void JSObjectFinalized() {SetJSObject(nullptr);}

    XPCWrappedNativeTearOff()
        : mInterface(nullptr), mNative(nullptr), mJSObject(nullptr) {}
    ~XPCWrappedNativeTearOff();

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void TraceJS(JSTracer* trc) {}
    inline void AutoTrace(JSTracer* trc) {}

    void Mark()       {mJSObject = (JSObject*)(intptr_t(mJSObject) | 1);}
    void Unmark()     {mJSObject = (JSObject*)(intptr_t(mJSObject) & ~1);}
    bool IsMarked() const {return !!(intptr_t(mJSObject) & 1);}

private:
    XPCWrappedNativeTearOff(const XPCWrappedNativeTearOff& r) MOZ_DELETE;
    XPCWrappedNativeTearOff& operator= (const XPCWrappedNativeTearOff& r) MOZ_DELETE;

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
    XPCWrappedNativeTearOffChunk() : mNextChunk(nullptr) {}
    ~XPCWrappedNativeTearOffChunk() {delete mNextChunk;}

private:
    XPCWrappedNativeTearOff mTearOffs[XPC_WRAPPED_NATIVE_TEAROFFS_PER_CHUNK];
    XPCWrappedNativeTearOffChunk* mNextChunk;
};

void *xpc_GetJSPrivate(JSObject *obj);

/***************************************************************************/
// XPCWrappedNative the wrapper around one instance of a native xpcom object
// to be used from JavaScript.

class XPCWrappedNative : public nsIXPConnectWrappedNative
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDNATIVE
    // No need to unlink the JS objects, if the XPCWrappedNative will be cycle
    // collected then its mFlatJSObject will be cycle collected too and
    // finalization of the mFlatJSObject will unlink the js objects (see
    // XPC_WN_NoHelper_Finalize and FlatJSObjectFinalized).
    // We also give XPCWrappedNative empty Root/Unroot methods, to avoid
    // root/unrooting the JS objects from addrefing/releasing the
    // XPCWrappedNative during unlinking, which would make the JS objects
    // uncollectable to the JS GC.
    class NS_CYCLE_COLLECTION_INNERCLASS
     : public nsXPCOMCycleCollectionParticipant
    {
      NS_DECL_CYCLE_COLLECTION_CLASS_BODY_NO_UNLINK(XPCWrappedNative,
                                                    XPCWrappedNative)
      static NS_METHOD RootImpl(void *p) { return NS_OK; }
      static NS_METHOD UnlinkImpl(void *p);
      static NS_METHOD UnrootImpl(void *p) { return NS_OK; }
    };
    NS_CYCLE_COLLECTION_PARTICIPANT_INSTANCE
    NS_DECL_CYCLE_COLLECTION_UNMARK_PURPLE_STUB(XPCWrappedNative)

    nsIPrincipal* GetObjectPrincipal() const;

    JSBool
    IsValid() const {return nullptr != mFlatJSObject;}

#define XPC_SCOPE_WORD(s)   (intptr_t(s))
#define XPC_SCOPE_MASK      (intptr_t(0x3))
#define XPC_SCOPE_TAG       (intptr_t(0x1))
#define XPC_WRAPPER_EXPIRED (intptr_t(0x2))

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

    inline JSBool
    IsWrapperExpired() const
        {return XPC_SCOPE_WORD(mMaybeScope) & XPC_WRAPPER_EXPIRED;}

    JSBool
    HasProto() const {return !IsTaggedScope(mMaybeScope);}

    XPCWrappedNativeProto*
    GetProto() const
        {return HasProto() ?
         (XPCWrappedNativeProto*)
         (XPC_SCOPE_WORD(mMaybeProto) & ~XPC_SCOPE_MASK) : nullptr;}

    void SetProto(XPCWrappedNativeProto* p);

    XPCWrappedNativeScope*
    GetScope() const
        {return GetProto() ? GetProto()->GetScope() :
         (XPCWrappedNativeScope*)
         (XPC_SCOPE_WORD(mMaybeScope) & ~XPC_SCOPE_MASK);}

    nsISupports*
    GetIdentityObject() const {return mIdentity;}

    /**
     * This getter clears the gray bit before handing out the JSObject which
     * means that the object is guaranteed to be kept alive past the next CC.
     */
    JSObject*
    GetFlatJSObject() const
        {if (mFlatJSObject != INVALID_OBJECT)
             xpc_UnmarkGrayObject(mFlatJSObject);
         return mFlatJSObject;}

    /**
     * This getter does not change the color of the JSObject meaning that the
     * object returned is not guaranteed to be kept alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    JSObject*
    GetFlatJSObjectPreserveColor() const {return mFlatJSObject;}

    XPCLock*
    GetLock() const {return IsValid() && HasProto() ?
                                GetProto()->GetLock() : nullptr;}

    XPCNativeSet*
    GetSet() const {XPCAutoLock al(GetLock()); return mSet;}

    void
    SetSet(XPCNativeSet* set) {XPCAutoLock al(GetLock()); mSet = set;}

private:
    inline void
    ExpireWrapper()
        {mMaybeScope = (XPCWrappedNativeScope*)
                       (XPC_SCOPE_WORD(mMaybeScope) | XPC_WRAPPER_EXPIRED);}

public:

    XPCNativeScriptableInfo*
    GetScriptableInfo() const {return mScriptableInfo;}

    nsIXPCScriptable*      // call this wrong and you deserve to crash
    GetScriptableCallback() const  {return mScriptableInfo->GetCallback();}

    void**
    GetSecurityInfoAddr() {return HasProto() ?
                                   GetProto()->GetSecurityInfoAddr() : nullptr;}

    nsIClassInfo*
    GetClassInfo() const {return IsValid() && HasProto() ?
                            GetProto()->GetClassInfo() : nullptr;}

    JSBool
    HasMutatedSet() const {return IsValid() &&
                                  (!HasProto() ||
                                   GetSet() != GetProto()->GetSet());}

    XPCJSRuntime*
    GetRuntime() const {XPCWrappedNativeScope* scope = GetScope();
                        return scope ? scope->GetRuntime() : nullptr;}

    static nsresult
    WrapNewGlobal(XPCCallContext &ccx, xpcObjectHelper &nativeHelper,
                  nsIPrincipal *principal, bool initStandardClasses,
                  XPCWrappedNative **wrappedGlobal);

    static nsresult
    GetNewOrUsed(XPCCallContext& ccx,
                 xpcObjectHelper& helper,
                 XPCWrappedNativeScope* Scope,
                 XPCNativeInterface* Interface,
                 XPCWrappedNative** wrapper);

    static nsresult
    Morph(XPCCallContext& ccx,
          JSObject* existingJSObject,
          XPCNativeInterface* Interface,
          nsWrapperCache *cache,
          XPCWrappedNative** resultWrapper);

public:
    static nsresult
    GetUsedOnly(XPCCallContext& ccx,
                nsISupports* Object,
                XPCWrappedNativeScope* Scope,
                XPCNativeInterface* Interface,
                XPCWrappedNative** wrapper);

    // If pobj2 is not null and *pobj2 is not null after the call then *pobj2
    // points to an object for which IS_SLIM_WRAPPER_OBJECT is true.
    // cx is null when invoked from the marking phase of the GC. In this case
    // fubobj must be null as well.
    static XPCWrappedNative*
    GetWrappedNativeOfJSObject(JSContext* cx, JSObject* obj,
                               JSObject* funobj = nullptr,
                               JSObject** pobj2 = nullptr,
                               XPCWrappedNativeTearOff** pTearOff = nullptr);
    static XPCWrappedNative*
    GetAndMorphWrappedNativeOfJSObject(JSContext* cx, JSObject* obj)
    {
        JSObject *obj2 = nullptr;
        XPCWrappedNative* wrapper =
            GetWrappedNativeOfJSObject(cx, obj, nullptr, &obj2);
        if (wrapper || !obj2)
            return wrapper;

        NS_ASSERTION(IS_SLIM_WRAPPER(obj2),
                     "Hmm, someone changed GetWrappedNativeOfJSObject?");
        SLIM_LOG_WILL_MORPH(cx, obj2);
        return MorphSlimWrapper(cx, obj2) ?
               (XPCWrappedNative*)xpc_GetJSPrivate(obj2) :
               nullptr;
    }

    static nsresult
    ReparentWrapperIfFound(XPCCallContext& ccx,
                           XPCWrappedNativeScope* aOldScope,
                           XPCWrappedNativeScope* aNewScope,
                           JSObject* aNewParent,
                           nsISupports* aCOMObj,
                           XPCWrappedNative** aWrapper);

    // Returns the wrapper corresponding to the parent of our mFlatJSObject.
    //
    // If the parent does not have a WN, or if there is no parent, null is
    // returned.
    XPCWrappedNative *GetParentWrapper();

    bool IsOrphan();
    nsresult RescueOrphans(XPCCallContext& ccx);

    void FlatJSObjectFinalized();

    void SystemIsBeingShutDown();

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    static JSBool CallMethod(XPCCallContext& ccx,
                             CallMode mode = CALL_METHOD);

    static JSBool GetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_GETTER);}

    static JSBool SetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_SETTER);}

    inline JSBool HasInterfaceNoQI(const nsIID& iid);

    XPCWrappedNativeTearOff* LocateTearOff(XPCCallContext& ccx,
                                           XPCNativeInterface* aInterface);
    XPCWrappedNativeTearOff* FindTearOff(XPCCallContext& ccx,
                                         XPCNativeInterface* aInterface,
                                         JSBool needJSObject = false,
                                         nsresult* pError = nullptr);
    void Mark() const
    {
        mSet->Mark();
        if (mScriptableInfo) mScriptableInfo->Mark();
        if (HasProto()) GetProto()->Mark();
    }

    // Yes, we *do* need to mark the mScriptableInfo in both cases.
    inline void TraceInside(JSTracer *trc) {
        if (JS_IsGCMarkingTracer(trc)) {
            mSet->Mark();
            if (mScriptableInfo)
                mScriptableInfo->Mark();
        }
        if (HasProto())
            GetProto()->TraceSelf(trc);
        else
            GetScope()->TraceSelf(trc);
        JSObject* wrapper = GetWrapperPreserveColor();
        if (wrapper)
            JS_CALL_OBJECT_TRACER(trc, wrapper, "XPCWrappedNative::mWrapper");
        if (mScriptableInfo &&
            (mScriptableInfo->GetJSClass()->flags & JSCLASS_XPCONNECT_GLOBAL))
        {
            TraceXPCGlobal(trc, mFlatJSObject);
        }
    }

    void TraceJS(JSTracer *trc) {
        TraceInside(trc);
    }

    void TraceSelf(JSTracer *trc) {
        // If this got called, we're being kept alive by someone who really
        // needs us alive and whole.  Do not let our mFlatJSObject go away.
        // This is the only time we should be tracing our mFlatJSObject,
        // normally somebody else is doing that. Be careful not to trace the
        // bogus INVALID_OBJECT value we can have during init, though.
        if (mFlatJSObject && mFlatJSObject != INVALID_OBJECT) {
            JS_CALL_OBJECT_TRACER(trc, mFlatJSObject,
                                  "XPCWrappedNative::mFlatJSObject");
        }
    }

    void AutoTrace(JSTracer *trc) {
        TraceSelf(trc);
    }

#ifdef DEBUG
    void ASSERT_SetsNotMarked() const
        {mSet->ASSERT_NotMarked();
         if (HasProto()){GetProto()->ASSERT_SetNotMarked();}}

    int DEBUG_CountOfTearoffChunks() const
        {int i = 0; const XPCWrappedNativeTearOffChunk* to;
         for (to = &mFirstChunk; to; to = to->mNextChunk) {i++;} return i;}
#endif

    inline void SweepTearOffs();

    // Returns a string that shuld be free'd using JS_smprintf_free (or null).
    char* ToString(XPCCallContext& ccx,
                   XPCWrappedNativeTearOff* to = nullptr) const;

    static void GatherProtoScriptableCreateInfo(nsIClassInfo* classInfo,
                                                XPCNativeScriptableCreateInfo& sciProto);

    JSBool HasExternalReference() const {return mRefCnt > 1;}

    JSBool NeedsSOW() { return !!(mWrapperWord & NEEDS_SOW); }
    void SetNeedsSOW() { mWrapperWord |= NEEDS_SOW; }
    JSBool NeedsCOW() { return !!(mWrapperWord & NEEDS_COW); }
    void SetNeedsCOW() { mWrapperWord |= NEEDS_COW; }

    JSObject* GetWrapperPreserveColor() const
        {return (JSObject*)(mWrapperWord & (size_t)~(size_t)FLAG_MASK);}

    JSObject* GetWrapper()
    {
        JSObject* wrapper = GetWrapperPreserveColor();
        if (wrapper) {
            xpc_UnmarkGrayObject(wrapper);
            // Call this to unmark mFlatJSObject.
            GetFlatJSObject();
        }
        return wrapper;
    }
    void SetWrapper(JSObject *obj)
    {
        js::IncrementalReferenceBarrier(GetWrapperPreserveColor());
        intptr_t newval = intptr_t(obj) | (mWrapperWord & FLAG_MASK);
        mWrapperWord = newval;
    }

    // Returns the relevant same-compartment security if applicable, or
    // mFlatJSObject otherwise.
    //
    // This takes care of checking mWrapperWord to see if we already have such
    // a wrapper.
    JSObject *GetSameCompartmentSecurityWrapper(JSContext *cx);

    void NoteTearoffs(nsCycleCollectionTraversalCallback& cb);

    QITableEntry* GetOffsets()
    {
        if (!HasProto() || !GetProto()->ClassIsDOMObject())
            return nullptr;

        XPCWrappedNativeProto* proto = GetProto();
        QITableEntry* offsets = proto->GetOffsets();
        if (!offsets) {
            static NS_DEFINE_IID(kThisPtrOffsetsSID, NS_THISPTROFFSETS_SID);
            mIdentity->QueryInterface(kThisPtrOffsetsSID, (void**)&offsets);
        }
        return offsets;
    }

    // Make ctor and dtor protected (rather than private) to placate nsCOMPtr.
protected:
    XPCWrappedNative(); // not implemented

    // This ctor is used if this object will have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports> aIdentity,
                     XPCWrappedNativeProto* aProto);

    // This ctor is used if this object will NOT have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports> aIdentity,
                     XPCWrappedNativeScope* aScope,
                     XPCNativeSet* aSet);

    virtual ~XPCWrappedNative();
    void Destroy();

    void UpdateScriptableInfo(XPCNativeScriptableInfo *si);

private:
    enum {
        NEEDS_SOW = JS_BIT(0),
        NEEDS_COW = JS_BIT(1),
        FLAG_MASK = JS_BITMASK(3)
    };

private:

    JSBool Init(XPCCallContext& ccx, JSObject* parent, const XPCNativeScriptableCreateInfo* sci);
    JSBool Init(XPCCallContext &ccx, JSObject *existingJSObject);
    JSBool FinishInit(XPCCallContext &ccx);

    JSBool ExtendSet(XPCCallContext& ccx, XPCNativeInterface* aInterface);

    nsresult InitTearOff(XPCCallContext& ccx,
                         XPCWrappedNativeTearOff* aTearOff,
                         XPCNativeInterface* aInterface,
                         JSBool needJSObject);

    JSBool InitTearOffJSObject(XPCCallContext& ccx,
                               XPCWrappedNativeTearOff* to);

public:
    static const XPCNativeScriptableCreateInfo& GatherScriptableCreateInfo(nsISupports* obj,
                                                                           nsIClassInfo* classInfo,
                                                                           XPCNativeScriptableCreateInfo& sciProto,
                                                                           XPCNativeScriptableCreateInfo& sciWrapper);

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
    intptr_t                     mWrapperWord;
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
#define NS_IXPCONNECT_WRAPPED_JS_CLASS_IID                                    \
{ 0x2453eba0, 0xa9b8, 0x11d2,                                                 \
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

    NS_IMETHOD CallMethod(nsXPCWrappedJS* wrapper, uint16_t methodIndex,
                          const XPTMethodDescriptor* info,
                          nsXPTCMiniVariant* params);

    JSObject*  CallQueryInterfaceOnJSObject(XPCCallContext& ccx,
                                            JSObject* jsobj, REFNSIID aIID);

    static nsresult BuildPropertyEnumerator(XPCCallContext& ccx,
                                            JSObject* aJSObj,
                                            nsISimpleEnumerator** aEnumerate);

    static nsresult GetNamedPropertyAsVariant(XPCCallContext& ccx,
                                              JSObject* aJSObj,
                                              const nsAString& aName,
                                              nsIVariant** aResult);

    virtual ~nsXPCWrappedJSClass();

    static nsresult CheckForException(XPCCallContext & ccx,
                                      const char * aPropertyName,
                                      const char * anInterfaceName,
                                      bool aForceReport);
private:
    nsXPCWrappedJSClass();   // not implemented
    nsXPCWrappedJSClass(XPCCallContext& ccx, REFNSIID aIID,
                        nsIInterfaceInfo* aInfo);

    JSObject*  NewOutObject(JSContext* cx, JSObject* scope);

    JSBool IsReflectable(uint16_t i) const
        {return (JSBool)(mDescriptors[i/32] & (1 << (i%32)));}
    void SetReflectable(uint16_t i, JSBool b)
        {if (b) mDescriptors[i/32] |= (1 << (i%32));
         else mDescriptors[i/32] &= ~(1 << (i%32));}

    JSBool GetArraySizeFromParam(JSContext* cx,
                                 const XPTMethodDescriptor* method,
                                 const nsXPTParamInfo& param,
                                 uint16_t methodIndex,
                                 uint8_t paramIndex,
                                 nsXPTCMiniVariant* params,
                                 uint32_t* result);

    JSBool GetInterfaceTypeFromParam(JSContext* cx,
                                     const XPTMethodDescriptor* method,
                                     const nsXPTParamInfo& param,
                                     uint16_t methodIndex,
                                     const nsXPTType& type,
                                     nsXPTCMiniVariant* params,
                                     nsID* result);

    void CleanupPointerArray(const nsXPTType& datum_type,
                             uint32_t array_count,
                             void** arrayp);

    void CleanupPointerTypeObject(const nsXPTType& type,
                                  void** pp);

private:
    XPCJSRuntime* mRuntime;
    nsIInterfaceInfo* mInfo;
    char* mName;
    nsIID mIID;
    uint32_t* mDescriptors;
};

/*************************/
// nsXPCWrappedJS is a wrapper for a single JSObject for use from native code.
// nsXPCWrappedJS objects are chained together to represent the various
// interface on the single underlying (possibly aggregate) JSObject.

class nsXPCWrappedJS : protected nsAutoXPTCStub,
                       public nsIXPConnectWrappedJS,
                       public nsSupportsWeakReference,
                       public nsIPropertyBag,
                       public XPCRootSetElem
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDJS
    NS_DECL_NSISUPPORTSWEAKREFERENCE
    NS_DECL_NSIPROPERTYBAG

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXPCWrappedJS, nsIXPConnectWrappedJS)
    NS_DECL_CYCLE_COLLECTION_UNMARK_PURPLE_STUB(nsXPCWrappedJS)

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

    /**
     * This getter clears the gray bit before handing out the JSObject which
     * means that the object is guaranteed to be kept alive past the next CC.
     */
    JSObject* GetJSObject() const {return xpc_UnmarkGrayObject(mJSObj);}

    /**
     * This getter does not change the color of the JSObject meaning that the
     * object returned is not guaranteed to be kept alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    JSObject* GetJSObjectPreserveColor() const {return mJSObj;}

    nsXPCWrappedJSClass*  GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}
    nsXPCWrappedJS* GetRootWrapper() const {return mRoot;}
    nsXPCWrappedJS* GetNextWrapper() const {return mNext;}

    nsXPCWrappedJS* Find(REFNSIID aIID);
    nsXPCWrappedJS* FindInherited(REFNSIID aIID);

    JSBool IsValid() const {return mJSObj != nullptr;}
    void SystemIsBeingShutDown(JSRuntime* rt);

    // This is used by XPCJSRuntime::GCCallback to find wrappers that no
    // longer root their JSObject and are only still alive because they
    // were being used via nsSupportsWeakReference at the time when their
    // last (outside) reference was released. Wrappers that fit into that
    // category are only deleted when we see that their corresponding JSObject
    // is to be finalized.
    JSBool IsSubjectToFinalization() const {return IsValid() && mRefCnt == 1;}

    JSBool IsAggregatedToNative() const {return mRoot->mOuter != nullptr;}
    nsISupports* GetAggregatedNativeObject() const {return mRoot->mOuter;}

    void SetIsMainThreadOnly() {
        MOZ_ASSERT(mMainThread);
        mMainThreadOnly = true;
    }
    bool IsMainThreadOnly() const {return mMainThreadOnly;}

    void TraceJS(JSTracer* trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);

    virtual ~nsXPCWrappedJS();
protected:
    nsXPCWrappedJS();   // not implemented
    nsXPCWrappedJS(XPCCallContext& ccx,
                   JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root,
                   nsISupports* aOuter);

   void Unlink();

private:
    JSObject* mJSObj;
    nsXPCWrappedJSClass* mClass;
    nsXPCWrappedJS* mRoot;
    nsXPCWrappedJS* mNext;
    nsISupports* mOuter;    // only set in root
    bool mMainThread;
    bool mMainThreadOnly;
};

/***************************************************************************/

class XPCJSObjectHolder : public nsIXPConnectJSObjectHolder,
                          public XPCRootSetElem
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER

    // non-interface implementation

public:
    static XPCJSObjectHolder* newHolder(XPCCallContext& ccx, JSObject* obj);

    virtual ~XPCJSObjectHolder();

    void TraceJS(JSTracer *trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);

private:
    XPCJSObjectHolder(XPCCallContext& ccx, JSObject* obj);
    XPCJSObjectHolder(); // not implemented

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

/***************************************************************************/
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
     * @param scope the default scope to put on the new JSObject's parent
     *        chain
     * @param pErr [out] relevant error code, if any.
     */
    static JSBool NativeData2JS(XPCCallContext& ccx, jsval* d, const void* s,
                                const nsXPTType& type, const nsID* iid,
                                nsresult* pErr)
    {
        XPCLazyCallContext lccx(ccx);
        return NativeData2JS(lccx, d, s, type, iid, pErr);
    }
    static JSBool NativeData2JS(XPCLazyCallContext& lccx, jsval* d,
                                const void* s, const nsXPTType& type,
                                const nsID* iid, nsresult* pErr);

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
     * @param iid the interface of src that we want (may be null)
     * @param Interface the interface of src that we want
     * @param cache the wrapper cache for src (may be null, in which case src
     *              will be QI'ed to get the cache)
     * @param allowNativeWrapper if true, this method may wrap the resulting
     *        JSObject in an XPCNativeWrapper and return that, as needed.
     * @param pErr [out] relevant error code, if any.
     * @param src_is_identity optional performance hint. Set to true only
     *                        if src is the identity pointer.
     */
    static JSBool NativeInterface2JSObject(XPCCallContext& ccx,
                                           jsval* d,
                                           nsIXPConnectJSObjectHolder** dest,
                                           xpcObjectHelper& aHelper,
                                           const nsID* iid,
                                           XPCNativeInterface** Interface,
                                           bool allowNativeWrapper,
                                           nsresult* pErr)
    {
        XPCLazyCallContext lccx(ccx);
        return NativeInterface2JSObject(lccx, d, dest, aHelper, iid, Interface,
                                        allowNativeWrapper, pErr);
    }
    static JSBool NativeInterface2JSObject(XPCLazyCallContext& lccx,
                                           jsval* d,
                                           nsIXPConnectJSObjectHolder** dest,
                                           xpcObjectHelper& aHelper,
                                           const nsID* iid,
                                           XPCNativeInterface** Interface,
                                           bool allowNativeWrapper,
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
    static JSBool GetISupportsFromJSObject(JSObject* obj, nsISupports** iface);

    /**
     * Convert a native array into a jsval.
     *
     * @param ccx the context for the whole procedure
     * @param d [out] the resulting jsval
     * @param s the native array we're working with
     * @param type the type of objects in the array
     * @param iid the interface of each object in the array that we want
     * @param count the number of items in the array
     * @param scope the default scope to put on the new JSObjects' parent chain
     * @param pErr [out] relevant error code, if any.
     */
    static JSBool NativeArray2JS(XPCLazyCallContext& ccx,
                                 jsval* d, const void** s,
                                 const nsXPTType& type, const nsID* iid,
                                 uint32_t count, nsresult* pErr);

    static JSBool JSArray2Native(XPCCallContext& ccx, void** d, jsval s,
                                 uint32_t count, const nsXPTType& type,
                                 const nsID* iid, nsresult* pErr);

    static JSBool JSTypedArray2Native(XPCCallContext& ccx,
                                      void** d,
                                      JSObject* jsarray,
                                      uint32_t count,
                                      const nsXPTType& type,
                                      nsresult* pErr);

    static JSBool NativeStringWithSize2JS(JSContext* cx,
                                          jsval* d, const void* s,
                                          const nsXPTType& type,
                                          uint32_t count,
                                          nsresult* pErr);

    static JSBool JSStringWithSize2Native(XPCCallContext& ccx, void* d, jsval s,
                                          uint32_t count, const nsXPTType& type,
                                          nsresult* pErr);

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
                                       nsIException** exception,
                                       JSContext* cx,
                                       jsval *jsExceptionPtr);

private:
    XPCConvert(); // not implemented

};

/***************************************************************************/

// readable string conversions, static methods only
class XPCStringConvert
{
public:

    // If the string shares the readable's buffer, that buffer will
    // get assigned to *sharedBuffer.  Otherwise null will be
    // assigned.
    static jsval ReadableToJSVal(JSContext *cx, const nsAString &readable,
                                 nsStringBuffer** sharedBuffer);

private:
    XPCStringConvert();         // not implemented
};

/***************************************************************************/
// code for throwing exceptions into JS

class XPCThrower
{
public:
    static void Throw(nsresult rv, JSContext* cx);
    static void Throw(nsresult rv, XPCCallContext& ccx);
    static void ThrowBadResult(nsresult rv, nsresult result, XPCCallContext& ccx);
    static void ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx);
    static JSBool SetVerbosity(JSBool state)
        {JSBool old = sVerbose; sVerbose = state; return old;}

    static void BuildAndThrowException(JSContext* cx, nsresult rv, const char* sz);
    static JSBool CheckForPendingException(nsresult result, JSContext *cx);

private:
    static void Verbosify(XPCCallContext& ccx,
                          char** psz, bool own);

    static JSBool ThrowExceptionObject(JSContext* cx, nsIException* e);

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
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCEXCEPTION_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIEXCEPTION
    NS_DECL_NSIXPCEXCEPTION

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

    static void InitStatics() { sEverMadeOneFromFactory = false; }

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
    bool            mInitialized;

    nsAutoJSValHolder mThrownJSVal;

    static JSBool sEverMadeOneFromFactory;
};

/***************************************************************************/
/*
* nsJSID implements nsIJSID. It is also used by nsJSIID and nsJSCID as a
* member (as a hidden implementaion detail) to which they delegate many calls.
*/

// Initialization is done on demand, and calling the destructor below is always
// safe.
extern void xpc_DestroyJSxIDClassObjects();

class nsJSID : public nsIJSID
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_JS_ID_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSID

    bool InitWithName(const nsID& id, const char *nameString);
    bool SetName(const char* name);
    void   SetNameToNoString()
        {NS_ASSERTION(!mName, "name already set"); mName = gNoString;}
    bool NameIsSet() const {return nullptr != mName;}
    const nsID& ID() const {return mID;}
    bool IsValid() const {return !mID.Equals(GetInvalidIID());}

    static nsJSID* NewID(const char* str);
    static nsJSID* NewID(const nsID& id);

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

class nsJSIID : public nsIJSIID,
                public nsIXPCScriptable,
                public nsISecurityCheckedComponent
{
public:
    NS_DECL_ISUPPORTS

    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

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
struct XPCJSContextInfo {
    XPCJSContextInfo(JSContext* aCx) :
        cx(aCx),
        savedFrameChain(false),
        suspendDepth(0)
    {}
    JSContext* cx;

    // Whether the frame chain was saved
    bool savedFrameChain;

    // Greater than 0 if a request was suspended.
    unsigned suspendDepth;
};

class XPCJSContextStack
{
public:
    XPCJSContextStack()
      : mSafeJSContext(NULL)
      , mOwnSafeJSContext(NULL)
    { }

    virtual ~XPCJSContextStack();

    uint32_t Count()
    {
        return mStack.Length();
    }

    JSContext *Peek()
    {
        return mStack.IsEmpty() ? NULL : mStack[mStack.Length() - 1].cx;
    }

    JSContext *Pop();
    bool Push(JSContext *cx);
    JSContext *GetSafeJSContext();

#ifdef DEBUG
    bool DEBUG_StackHasJSContext(JSContext *cx);
#endif

    const InfallibleTArray<XPCJSContextInfo>* GetStack()
    { return &mStack; }

private:
    AutoInfallibleTArray<XPCJSContextInfo, 16> mStack;
    JSContext*  mSafeJSContext;
    JSContext*  mOwnSafeJSContext;
};

/***************************************************************************/

#define NS_XPC_JSCONTEXT_STACK_ITERATOR_CID                                   \
{ 0x05bae29d, 0x8aef, 0x486d,                                                 \
  { 0x84, 0xaa, 0x53, 0xf4, 0x8f, 0x14, 0x68, 0x11 } }

class nsXPCJSContextStackIterator MOZ_FINAL : public nsIJSContextStackIterator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSCONTEXTSTACKITERATOR

private:
    const InfallibleTArray<XPCJSContextInfo> *mStack;
    PRUint32 mPosition;
};

/***************************************************************************/
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
// 'Components' object

class nsXPCComponents : public nsIXPCComponents,
                        public nsIXPCScriptable,
                        public nsIClassInfo,
                        public nsISecurityCheckedComponent
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

public:
    static JSBool
    AttachComponentsObject(XPCCallContext& ccx,
                           XPCWrappedNativeScope* aScope,
                           JSObject* aGlobal);

    void SystemIsBeingShutDown() {ClearMembers();}

    virtual ~nsXPCComponents();

private:
    nsXPCComponents(XPCWrappedNativeScope* aScope);
    void ClearMembers();

private:
    friend class XPCWrappedNativeScope;
    XPCWrappedNativeScope*          mScope;
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

extern JSObject*
xpc_NewIDObject(JSContext *cx, JSObject* jsobj, const nsID& aID);

extern const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj);

extern JSBool
xpc_JSObjectIsID(JSContext *cx, JSObject* obj);

/***************************************************************************/
// in xpcdebug.cpp

extern JSBool
xpc_DumpJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals,
                JSBool showThisProps);

// Return a newly-allocated string containing a representation of the
// current JS stack.  It is the *caller's* responsibility to free this
// string with JS_smprintf_free().
extern char*
xpc_PrintJSStack(JSContext* cx, JSBool showArgs, JSBool showLocals,
                 JSBool showThisProps);

extern JSBool
xpc_DumpEvalInJSStackFrame(JSContext* cx, uint32_t frameno, const char* text);

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
    PRUint64 mOuterWindowID;
    PRUint64 mInnerWindowID;
    PRInt64 mTimeStamp;
};

/******************************************************************************
 * Handles pre/post script processing and the setting/resetting the error
 * reporter
 */
class NS_STACK_CLASS AutoScriptEvaluate
{
public:
    /**
     * Saves the JSContext as well as initializing our state
     * @param cx The JSContext, this can be null, we don't do anything then
     */
    AutoScriptEvaluate(JSContext * cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
         : mJSContext(cx), mState(0), mErrorReporterSet(false),
           mEvaluated(false), mContextHasThread(0) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    /**
     * Does the pre script evaluation and sets the error reporter if given
     * This function should only be called once, and will assert if called
     * more than once
     * @param errorReporter the error reporter callback function to set
     */

    bool StartEvaluating(JSObject *scope, JSErrorReporter errorReporter = nullptr);
    /**
     * Does the post script evaluation and resets the error reporter
     */
    ~AutoScriptEvaluate();
private:
    JSContext* mJSContext;
    JSExceptionState* mState;
    bool mErrorReporterSet;
    bool mEvaluated;
    intptr_t mContextHasThread;
    JSAutoEnterCompartment mEnterCompartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    // No copying or assignment allowed
    AutoScriptEvaluate(const AutoScriptEvaluate &) MOZ_DELETE;
    AutoScriptEvaluate & operator =(const AutoScriptEvaluate &) MOZ_DELETE;
};

/***************************************************************************/
class NS_STACK_CLASS AutoResolveName
{
public:
    AutoResolveName(XPCCallContext& ccx, jsid name
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
          mOld(XPCJSRuntime::Get()->SetResolveName(name))
#ifdef DEBUG
          ,mCheck(name)
#endif
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoResolveName()
        {
#ifdef DEBUG
            jsid old =
#endif
            XPCJSRuntime::Get()->SetResolveName(mOld);
            NS_ASSERTION(old == mCheck, "Bad Nesting!");
        }

private:
    jsid mOld;
#ifdef DEBUG
    jsid mCheck;
#endif
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/***************************************************************************/
class XPCMarkableJSVal
{
public:
    XPCMarkableJSVal(jsval val) : mVal(val), mValPtr(&mVal) {}
    XPCMarkableJSVal(jsval *pval) : mVal(JSVAL_VOID), mValPtr(pval) {}
    ~XPCMarkableJSVal() {}
    void Mark() {}
    void TraceJS(JSTracer* trc)
    {
        JS_CALL_VALUE_TRACER(trc, *mValPtr, "XPCMarkableJSVal");
    }
    void AutoTrace(JSTracer* trc) {}
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
    AutoMarkingPtr(XPCCallContext& ccx) {
        mRoot = XPCJSRuntime::Get()->GetAutoRootsAdr();
        mNext = *mRoot;
        *mRoot = this;
    }

    virtual ~AutoMarkingPtr() {
        if (mRoot) {
            MOZ_ASSERT(*mRoot == this);
            *mRoot = mNext;
        }
    }

    void TraceJSAll(JSTracer* trc) {
        for (AutoMarkingPtr *cur = this; cur; cur = cur->mNext)
            cur->TraceJS(trc);
    }

    void MarkAfterJSFinalizeAll() {
        for (AutoMarkingPtr *cur = this; cur; cur = cur->mNext)
            cur->MarkAfterJSFinalize();
    }

  protected:
    virtual void TraceJS(JSTracer* trc) = 0;
    virtual void MarkAfterJSFinalize() = 0;

  private:
    AutoMarkingPtr** mRoot;
    AutoMarkingPtr* mNext;
};

template<class T>
class TypedAutoMarkingPtr : public AutoMarkingPtr
{
  public:
    TypedAutoMarkingPtr(XPCCallContext& ccx) : AutoMarkingPtr(ccx), mPtr(nullptr) {}
    TypedAutoMarkingPtr(XPCCallContext& ccx, T* ptr) : AutoMarkingPtr(ccx), mPtr(ptr) {}

    T* get() const { return mPtr; }
    operator T *() const { return mPtr; }
    T* operator->() const { return mPtr; }

    TypedAutoMarkingPtr<T>& operator =(T* ptr) { mPtr = ptr; return *this; }

  protected:
    virtual void TraceJS(JSTracer* trc)
    {
        if (mPtr) {
            mPtr->TraceJS(trc);
            mPtr->AutoTrace(trc);
        }
    }

    virtual void MarkAfterJSFinalize()
    {
        if (mPtr)
            mPtr->Mark();
    }

  private:
    T* mPtr;
};

typedef TypedAutoMarkingPtr<XPCNativeInterface> AutoMarkingNativeInterfacePtr;
typedef TypedAutoMarkingPtr<XPCNativeSet> AutoMarkingNativeSetPtr;
typedef TypedAutoMarkingPtr<XPCWrappedNative> AutoMarkingWrappedNativePtr;
typedef TypedAutoMarkingPtr<XPCWrappedNativeTearOff> AutoMarkingWrappedNativeTearOffPtr;
typedef TypedAutoMarkingPtr<XPCWrappedNativeProto> AutoMarkingWrappedNativeProtoPtr;
typedef TypedAutoMarkingPtr<XPCMarkableJSVal> AutoMarkingJSVal;
typedef TypedAutoMarkingPtr<XPCNativeScriptableInfo> AutoMarkingNativeScriptableInfoPtr;

template<class T>
class ArrayAutoMarkingPtr : public AutoMarkingPtr
{
  public:
    ArrayAutoMarkingPtr(XPCCallContext& ccx)
      : AutoMarkingPtr(ccx), mPtr(nullptr), mCount(0) {}
    ArrayAutoMarkingPtr(XPCCallContext& ccx, T** ptr, PRUint32 count, bool clear)
      : AutoMarkingPtr(ccx), mPtr(ptr), mCount(count)
    {
        if (!mPtr) mCount = 0;
        else if (clear) memset(mPtr, 0, mCount*sizeof(T*));
    }

    T** get() const { return mPtr; }
    operator T **() const { return mPtr; }
    T** operator->() const { return mPtr; }

    ArrayAutoMarkingPtr<T>& operator =(const ArrayAutoMarkingPtr<T> &other)
    {
        mPtr = other.mPtr;
        mCount = other.mCount;
        return *this;
    }

  protected:
    virtual void TraceJS(JSTracer* trc)
    {
        for (PRUint32 i = 0; i < mCount; i++) {
            if (mPtr[i]) {
                mPtr[i]->TraceJS(trc);
                mPtr[i]->AutoTrace(trc);
            }
        }
    }

    virtual void MarkAfterJSFinalize()
    {
        for (PRUint32 i = 0; i < mCount; i++) {
            if (mPtr[i])
                mPtr[i]->Mark();
        }
    }

  private:
    T** mPtr;
    PRUint32 mCount;
};

typedef ArrayAutoMarkingPtr<XPCNativeInterface> AutoMarkingNativeInterfacePtrArrayPtr;

#define AUTO_MARK_JSVAL_HELPER2(tok, line) tok##line
#define AUTO_MARK_JSVAL_HELPER(tok, line) AUTO_MARK_JSVAL_HELPER2(tok, line)

#define AUTO_MARK_JSVAL(ccx, val)                                             \
    XPCMarkableJSVal AUTO_MARK_JSVAL_HELPER(_val_,__LINE__)(val);             \
    AutoMarkingJSVal AUTO_MARK_JSVAL_HELPER(_automarker_,__LINE__)            \
    (ccx, &AUTO_MARK_JSVAL_HELPER(_val_,__LINE__))

/***************************************************************************/
// Allocates a string that grants all access ("AllAccess")

extern char* xpc_CloneAllAccess();
/***************************************************************************/
// Returns access if wideName is in list

extern char * xpc_CheckAccessList(const PRUnichar* wideName, const char* list[]);

/***************************************************************************/
// in xpcvariant.cpp...

// {1809FD50-91E8-11d5-90F9-0010A4E73D9A}
#define XPCVARIANT_IID                                                        \
    {0x1809fd50, 0x91e8, 0x11d5,                                              \
      { 0x90, 0xf9, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a } }

// {DC524540-487E-4501-9AC7-AAA784B17C1C}
#define XPCVARIANT_CID                                                        \
    {0xdc524540, 0x487e, 0x4501,                                              \
      { 0x9a, 0xc7, 0xaa, 0xa7, 0x84, 0xb1, 0x7c, 0x1c } }

class XPCVariant : public nsIVariant
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIVARIANT
    NS_DECL_CYCLE_COLLECTION_CLASS(XPCVariant)

    // If this class ever implements nsIWritableVariant, take special care with
    // the case when mJSVal is JSVAL_STRING, since we don't own the data in
    // that case.

    // We #define and iid so that out module local code can use QI to detect
    // if a given nsIVariant is in fact an XPCVariant.
    NS_DECLARE_STATIC_IID_ACCESSOR(XPCVARIANT_IID)

    static XPCVariant* newVariant(XPCCallContext& ccx, jsval aJSVal);

    /**
     * This getter clears the gray bit before handing out the jsval if the jsval
     * represents a JSObject. That means that the object is guaranteed to be
     * kept alive past the next CC.
     */
    jsval GetJSVal() const
        {if (!JSVAL_IS_PRIMITIVE(mJSVal))
             xpc_UnmarkGrayObject(JSVAL_TO_OBJECT(mJSVal));
         return mJSVal;}

    /**
     * This getter does not change the color of the jsval (if it represents a
     * JSObject) meaning that the value returned is not guaranteed to be kept
     * alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    jsval GetJSValPreserveColor() const {return mJSVal;}

    XPCVariant(XPCCallContext& ccx, jsval aJSVal);

    /**
     * Convert a variant into a jsval.
     *
     * @param ccx the context for the whole procedure
     * @param variant the variant to convert
     * @param scope the default scope to put on the new JSObject's parent chain
     * @param pErr [out] relevant error code, if any.
     * @param pJSVal [out] the resulting jsval.
     */
    static JSBool VariantDataToJS(XPCLazyCallContext& lccx,
                                  nsIVariant* variant,
                                  nsresult* pErr, jsval* pJSVal);

    bool IsPurple()
    {
        return mRefCnt.IsPurple();
    }

    void RemovePurple()
    {
        mRefCnt.RemovePurple();
    }

    void SetCCGeneration(PRUint32 aGen)
    {
        mCCGeneration = aGen;
    }

    PRUint32 CCGeneration() { return mCCGeneration; }
protected:
    virtual ~XPCVariant() { }

    JSBool InitializeData(XPCCallContext& ccx);

protected:
    nsDiscriminatedUnion mData;
    jsval                mJSVal;
    bool                 mReturnRawObject : 1;
    PRUint32             mCCGeneration : 31;
};

NS_DEFINE_STATIC_IID_ACCESSOR(XPCVariant, XPCVARIANT_IID)

class XPCTraceableVariant: public XPCVariant,
                           public XPCRootSetElem
{
public:
    XPCTraceableVariant(XPCCallContext& ccx, jsval aJSVal)
        : XPCVariant(ccx, aJSVal)
    {
        ccx.GetRuntime()->AddVariantRoot(this);
    }

    virtual ~XPCTraceableVariant();

    void TraceJS(JSTracer* trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);
};

/***************************************************************************/

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

/***************************************************************************/
// Utilities

inline void *
xpc_GetJSPrivate(JSObject *obj)
{
    return js::GetObjectPrivate(obj);
}

namespace xpc {
struct SandboxOptions {
    SandboxOptions()
        : wantXrays(true)
        , wantComponents(true)
        , wantXHRConstructor(false)
        , proto(NULL)
    { }

    bool wantXrays;
    bool wantComponents;
    bool wantXHRConstructor;
    JSObject* proto;
    nsCString sandboxName;
};
}

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
xpc_CreateSandboxObject(JSContext * cx, jsval * vp, nsISupports *prinOrSop,
                        xpc::SandboxOptions& options);
// Helper for evaluating scripts in a sandbox object created with
// xpc_CreateSandboxObject(). The caller is responsible of ensuring
// that *rval doesn't get collected during the call or usage after the
// call. This helper will use filename and lineNo for error reporting,
// and if no filename is provided it will use the codebase from the
// principal and line number 1 as a fallback. if returnStringOnly is
// true, then the result in *rval, or the exception in cx->exception
// will be coerced into strings. If an exception is thrown converting
// an exception to a string, evalInSandbox will return an NS_ERROR_*
// result, and cx->exception will be empty.
nsresult
xpc_EvalInSandbox(JSContext *cx, JSObject *sandbox, const nsAString& source,
                  const char *filename, PRInt32 lineNo,
                  JSVersion jsVersion, bool returnStringOnly, jsval *rval);

/***************************************************************************/
// Inlined utilities.

inline JSBool
xpc_ForcePropertyResolve(JSContext* cx, JSObject* obj, jsid id);

inline jsid
GetRTIdByIndex(JSContext *cx, unsigned index);

// Wrapper for JS_NewObject to mark the new object as system when parent is
// also a system object. If uniqueType is specified then a new type object will
// be created which is used only by the result, so that its property types
// will be tracked precisely.
inline JSObject*
xpc_NewSystemInheritingJSObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                                bool uniqueType, JSObject *parent);

nsISupports *
XPC_GetIdentityObject(JSContext *cx, JSObject *obj);

namespace xpc {

class CompartmentPrivate
{
public:
    typedef nsTHashtable<nsPtrHashKey<JSObject> > DOMExpandoMap;

    CompartmentPrivate(bool wantXrays)
        : wantXrays(wantXrays)
    {
        MOZ_COUNT_CTOR(xpc::CompartmentPrivate);
    }

    ~CompartmentPrivate();

    bool wantXrays;
    nsAutoPtr<JSObject2JSObjectMap> waiverWrapperMap;
    nsAutoPtr<DOMExpandoMap> domExpandoMap;

    bool RegisterDOMExpandoObject(JSObject *expando) {
        if (!domExpandoMap) {
            domExpandoMap = new DOMExpandoMap();
            domExpandoMap->Init(8);
        }
        return domExpandoMap->PutEntry(expando, mozilla::fallible_t());
    }
    void RemoveDOMExpandoObject(JSObject *expando) {
        if (domExpandoMap)
            domExpandoMap->RemoveEntry(expando);
    }

    const nsACString& GetLocation() {
        if (locationURI) {
            if (NS_FAILED(locationURI->GetSpec(location)))
                location = NS_LITERAL_CSTRING("<unknown location>");
            locationURI = nullptr;
        }
        return location;
    }
    void SetLocation(const nsACString& aLocation) {
        if (aLocation.IsEmpty())
            return;
        if (!location.IsEmpty() || locationURI)
            return;
        location = aLocation;
    }
    void SetLocation(nsIURI *aLocationURI) {
        if (!aLocationURI)
            return;
        if (!location.IsEmpty() || locationURI)
            return;
        locationURI = aLocationURI;
    }

private:
    nsCString location;
    nsCOMPtr<nsIURI> locationURI;
};

inline CompartmentPrivate*
GetCompartmentPrivate(JSCompartment *compartment)
{
    MOZ_ASSERT(compartment);
    void *priv = JS_GetCompartmentPrivate(compartment);
    return static_cast<CompartmentPrivate*>(priv);
}

inline CompartmentPrivate*
GetCompartmentPrivate(JSObject *object)
{
    MOZ_ASSERT(object);
    JSCompartment *compartment = js::GetObjectCompartment(object);

    MOZ_ASSERT(compartment);
    return GetCompartmentPrivate(compartment);
}

}

/***************************************************************************/
// Inlines use the above - include last.

#include "XPCInlines.h"

/***************************************************************************/
// Maps have inlines that use the above - include last.

#include "XPCMaps.h"

/***************************************************************************/

#endif /* xpcprivate_h___ */
