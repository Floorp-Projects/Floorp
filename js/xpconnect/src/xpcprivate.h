/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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
 * nsIXPCScriptable instance can have pointers from XPCWrappedNativeProto and
 * XPCWrappedNative (since C++ objects can have scriptable info without having
 * class info).
 */

/* All the XPConnect private declarations - only include locally. */

#ifndef xpcprivate_h___
#define xpcprivate_h___

#include "mozilla/Alignment.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/dom/ScriptSettings.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xpcpublic.h"
#include "js/TracingAPI.h"
#include "js/WeakMapPtr.h"
#include "PLDHashTable.h"
#include "nscore.h"
#include "nsXPCOM.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
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
#include "nsIXPCScriptable.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsXPTCUtils.h"
#include "xptinfo.h"
#include "XPCForwards.h"
#include "XPCLog.h"
#include "xpccomponents.h"
#include "xpcexception.h"
#include "xpcjsid.h"
#include "prenv.h"
#include "prcvar.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"

#include "MainThreadUtils.h"

#include "nsIConsoleService.h"
#include "nsIException.h"

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

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptObjectPrincipal.h"
#include "xpcObjectHelper.h"

#include "SandboxPrivate.h"
#include "BackstagePass.h"
#include "nsAXPCNativeCallContext.h"

#ifdef XP_WIN
// Nasty MS defines
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#ifdef GetClassName
#undef GetClassName
#endif
#endif /* XP_WIN */

/***************************************************************************/
// default initial sizes for maps (hashtables)

#define XPC_JS_MAP_LENGTH                       32
#define XPC_JS_CLASS_MAP_LENGTH                 32

#define XPC_NATIVE_MAP_LENGTH                    8
#define XPC_NATIVE_PROTO_MAP_LENGTH              8
#define XPC_DYING_NATIVE_PROTO_MAP_LENGTH        8
#define XPC_NATIVE_INTERFACE_MAP_LENGTH         32
#define XPC_NATIVE_SET_MAP_LENGTH               32
#define XPC_THIS_TRANSLATOR_MAP_LENGTH           4
#define XPC_WRAPPER_MAP_LENGTH                   8

/***************************************************************************/
// data declarations...
extern const char XPC_CONTEXT_STACK_CONTRACTID[];
extern const char XPC_EXCEPTION_CONTRACTID[];
extern const char XPC_CONSOLE_CONTRACTID[];
extern const char XPC_SCRIPT_ERROR_CONTRACTID[];
extern const char XPC_ID_CONTRACTID[];
extern const char XPC_XPCONNECT_CONTRACTID[];

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

// If IS_WN_CLASS for the JSClass of an object is true, the object is a
// wrappednative wrapper, holding the XPCWrappedNative in its private slot.
static inline bool IS_WN_CLASS(const js::Class* clazz)
{
    return clazz->isWrappedNative();
}

static inline bool IS_WN_REFLECTOR(JSObject* obj)
{
    return IS_WN_CLASS(js::GetObjectClass(obj));
}

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

class nsXPConnect final : public nsIXPConnect
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECT

    // non-interface implementation
public:
    // These get non-addref'd pointers
    static nsXPConnect* XPConnect()
    {
        // Do a release-mode assert that we're not doing anything significant in
        // XPConnect off the main thread. If you're an extension developer hitting
        // this, you need to change your code. See bug 716167.
        if (!MOZ_LIKELY(NS_IsMainThread()))
            MOZ_CRASH();

        return gSelf;
    }

    static XPCJSRuntime* GetRuntimeInstance();

    static bool IsISupportsDescendant(nsIInterfaceInfo* info);

    static nsIScriptSecurityManager* SecurityManager()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(gScriptSecurityManager);
        return gScriptSecurityManager;
    }

    static nsIPrincipal* SystemPrincipal()
    {
        MOZ_ASSERT(NS_IsMainThread());
        MOZ_ASSERT(gSystemPrincipal);
        return gSystemPrincipal;
    }

    // This returns an AddRef'd pointer. It does not do this with an 'out' param
    // only because this form is required by the generic module macro:
    // NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR
    static nsXPConnect* GetSingleton();

    // Called by module code in dll startup
    static void InitStatics();
    // Called by module code on dll shutdown.
    static void ReleaseXPConnectSingleton();

    bool IsShuttingDown() const {return mShuttingDown;}

    nsresult GetInfoForIID(const nsIID * aIID, nsIInterfaceInfo** info);
    nsresult GetInfoForName(const char * name, nsIInterfaceInfo** info);

    virtual nsIPrincipal* GetPrincipal(JSObject* obj,
                                       bool allowShortCircuit) const override;

    void RecordTraversal(void* p, nsISupports* s);
    virtual char* DebugPrintJSStack(bool showArgs,
                                    bool showLocals,
                                    bool showThisProps) override;

protected:
    virtual ~nsXPConnect();

    nsXPConnect();

private:
    // Singleton instance
    static nsXPConnect*             gSelf;
    static bool                     gOnceAliveNowDead;

    XPCJSRuntime*                   mRuntime;
    bool                            mShuttingDown;

public:
    static nsIScriptSecurityManager* gScriptSecurityManager;
    static nsIPrincipal* gSystemPrincipal;
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
        MOZ_ASSERT(!mNext, "Must be unlinked");
        MOZ_ASSERT(!mSelfp, "Must be unlinked");
    }

    inline XPCRootSetElem* GetNextRoot() { return mNext; }
    void AddToRootSet(XPCRootSetElem** listHead);
    void RemoveFromRootSet();

private:
    XPCRootSetElem* mNext;
    XPCRootSetElem** mSelfp;
};

/***************************************************************************/

// In the current xpconnect system there can only be one XPCJSContext.
// So, xpconnect can only be used on one JSContext within the process.

class WatchdogManager;

enum WatchdogTimestampCategory
{
    TimestampContextStateChange = 0,
    TimestampWatchdogWakeup,
    TimestampWatchdogHibernateStart,
    TimestampWatchdogHibernateStop,
    TimestampCount
};

class AsyncFreeSnowWhite;

template <class StringType>
class ShortLivedStringBuffer
{
public:
    StringType* Create()
    {
        for (uint32_t i = 0; i < ArrayLength(mStrings); ++i) {
            if (!mStrings[i]) {
                mStrings[i].emplace();
                return mStrings[i].ptr();
            }
        }

        // All our internal string wrappers are used, allocate a new string.
        return new StringType();
    }

    void Destroy(StringType* string)
    {
        for (uint32_t i = 0; i < ArrayLength(mStrings); ++i) {
            if (mStrings[i] && mStrings[i].ptr() == string) {
                // One of our internal strings is no longer in use, mark
                // it as such and free its data.
                mStrings[i].reset();
                return;
            }
        }

        // We're done with a string that's not one of our internal
        // strings, delete it.
        delete string;
    }

    ~ShortLivedStringBuffer()
    {
#ifdef DEBUG
        for (uint32_t i = 0; i < ArrayLength(mStrings); ++i) {
            MOZ_ASSERT(!mStrings[i], "Short lived string still in use");
        }
#endif
    }

private:
    mozilla::Maybe<StringType> mStrings[2];
};

class XPCJSContext final : public mozilla::CycleCollectedJSContext
{
public:
    static void InitTLS();
    static XPCJSContext* NewXPCJSContext(XPCJSContext* aPrimaryContext);
    static XPCJSContext* Get();

    XPCJSRuntime* Runtime() const;

    mozilla::CycleCollectedJSRuntime* CreateRuntime(JSContext* aCx) override;

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

    bool JSContextInitialized(JSContext* cx);

    virtual void BeforeProcessTask(bool aMightBlock) override;
    virtual void AfterProcessTask(uint32_t aNewRecursionDepth) override;

    ~XPCJSContext();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    AutoMarkingPtr** GetAutoRootsAdr() {return &mAutoRoots;}

    nsresult GetPendingResult() { return mPendingResult; }
    void SetPendingResult(nsresult rv) { mPendingResult = rv; }

    PRTime GetWatchdogTimestamp(WatchdogTimestampCategory aCategory);

    static void ActivityCallback(void* arg, bool active);
    static bool InterruptCallback(JSContext* cx);

    // Mapping of often used strings to jsid atoms that live 'forever'.
    //
    // To add a new string: add to this list and to XPCJSContext::mStrings
    // at the top of XPCJSContext.cpp
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
        IDX_EVAL                    ,
        IDX_CONTROLLERS             ,
        IDX_REALFRAMEELEMENT        ,
        IDX_LENGTH                  ,
        IDX_NAME                    ,
        IDX_UNDEFINED               ,
        IDX_EMPTYSTRING             ,
        IDX_FILENAME                ,
        IDX_LINENUMBER              ,
        IDX_COLUMNNUMBER            ,
        IDX_STACK                   ,
        IDX_MESSAGE                 ,
        IDX_LASTINDEX               ,
        IDX_TOTAL_COUNT // just a count of the above
    };

    inline JS::HandleId GetStringID(unsigned index) const;
    inline const char* GetStringName(unsigned index) const;

    ShortLivedStringBuffer<nsString> mScratchStrings;
    ShortLivedStringBuffer<nsCString> mScratchCStrings;

private:
    XPCJSContext();

    MOZ_IS_CLASS_INIT
    nsresult Initialize(XPCJSContext* aPrimaryContext);

    XPCCallContext*          mCallContext;
    AutoMarkingPtr*          mAutoRoots;
    jsid                     mResolveName;
    XPCWrappedNative*        mResolvingWrapper;
    RefPtr<WatchdogManager>  mWatchdogManager;

    // If we spend too much time running JS code in an event handler, then we
    // want to show the slow script UI. The timeout T is controlled by prefs. We
    // invoke the interrupt callback once after T/2 seconds and set
    // mSlowScriptSecondHalf to true. After another T/2 seconds, we invoke the
    // interrupt callback again. Since mSlowScriptSecondHalf is now true, it
    // shows the slow script UI. The reason we invoke the callback twice is to
    // ensure that putting the computer to sleep while running a script doesn't
    // cause the UI to be shown. If the laptop goes to sleep during one of the
    // timeout periods, the script still has the other T/2 seconds to complete
    // before the slow script UI is shown.
    bool mSlowScriptSecondHalf;

    // mSlowScriptCheckpoint is set to the time when:
    // 1. We started processing the current event, or
    // 2. mSlowScriptSecondHalf was set to true
    // (whichever comes later). We use it to determine whether the interrupt
    // callback needs to do anything.
    mozilla::TimeStamp mSlowScriptCheckpoint;
    // Accumulates total time we actually waited for telemetry
    mozilla::TimeDuration mSlowScriptActualWait;
    bool mTimeoutAccumulated;

    // mPendingResult is used to implement Components.returnCode.  Only really
    // meaningful while calling through XPCWrappedJS.
    nsresult mPendingResult;

    friend class XPCJSRuntime;
    friend class Watchdog;
    friend class AutoLockWatchdog;
};

class XPCJSRuntime final : public mozilla::CycleCollectedJSRuntime
{
public:
    static XPCJSRuntime* Get();

    void RemoveWrappedJS(nsXPCWrappedJS* wrapper);
    void AssertInvalidWrappedJSNotInTable(nsXPCWrappedJS* wrapper) const;

    JSObject2WrappedJSMap* GetMultiCompartmentWrappedJSMap() const
        {return mWrappedJSMap;}

    IID2WrappedJSClassMap* GetWrappedJSClassMap() const
        {return mWrappedJSClassMap;}

    IID2NativeInterfaceMap* GetIID2NativeInterfaceMap() const
        {return mIID2NativeInterfaceMap;}

    ClassInfo2NativeSetMap* GetClassInfo2NativeSetMap() const
        {return mClassInfo2NativeSetMap;}

    NativeSetMap* GetNativeSetMap() const
        {return mNativeSetMap;}

    IID2ThisTranslatorMap* GetThisTranslatorMap() const
        {return mThisTranslatorMap;}

    XPCWrappedNativeProtoMap* GetDyingWrappedNativeProtoMap() const
        {return mDyingWrappedNativeProtoMap;}

    bool InitializeStrings(JSContext* cx);

    virtual bool
    DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                          char (&aName)[72]) const override;
    virtual bool
    NoteCustomGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                                   nsCycleCollectionTraversalCallback& aCb) const override;

    /**
     * Infrastructure for classes that need to defer part of the finalization
     * until after the GC has run, for example for objects that we don't want to
     * destroy during the GC.
     */

public:
    bool GetDoingFinalization() const {return mDoingFinalization;}

    JS::HandleId GetStringID(unsigned index) const
    {
        MOZ_ASSERT(index < XPCJSContext::IDX_TOTAL_COUNT, "index out of range");
        // fromMarkedLocation() is safe because the string is interned.
        return JS::HandleId::fromMarkedLocation(&mStrIDs[index]);
    }
    JS::HandleValue GetStringJSVal(unsigned index) const
    {
        MOZ_ASSERT(index < XPCJSContext::IDX_TOTAL_COUNT, "index out of range");
        // fromMarkedLocation() is safe because the string is interned.
        return JS::HandleValue::fromMarkedLocation(&mStrJSVals[index]);
    }
    const char* GetStringName(unsigned index) const
    {
        MOZ_ASSERT(index < XPCJSContext::IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    virtual bool UsefulToMergeZones() const override;
    void TraceNativeBlackRoots(JSTracer* trc) override;
    void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) override;
    void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& cb) override;
    void UnmarkSkippableJSHolders();
    void PrepareForForgetSkippable() override;
    void BeginCycleCollectionCallback() override;
    void EndCycleCollectionCallback(mozilla::CycleCollectorResults& aResults) override;
    void DispatchDeferredDeletion(bool aContinuation, bool aPurge = false) override;

    void CustomGCCallback(JSGCStatus status) override;
    void CustomOutOfMemoryCallback() override;
    void OnLargeAllocationFailure();
    static void GCSliceCallback(JSContext* cx,
                                JS::GCProgress progress,
                                const JS::GCDescription& desc);
    static void DoCycleCollectionCallback(JSContext* cx);
    static void FinalizeCallback(JSFreeOp* fop,
                                 JSFinalizeStatus status,
                                 bool isZoneGC,
                                 void* data);
    static void WeakPointerZonesCallback(JSContext* cx, void* data);
    static void WeakPointerCompartmentCallback(JSContext* cx, JSCompartment* comp, void* data);

    inline void AddVariantRoot(XPCTraceableVariant* variant);
    inline void AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS);
    inline void AddObjectHolderRoot(XPCJSObjectHolder* holder);

    void DebugDump(int16_t depth);

    bool GCIsRunning() const {return mGCIsRunning;}

    ~XPCJSRuntime();

    void AddGCCallback(xpcGCCallback cb);
    void RemoveGCCallback(xpcGCCallback cb);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    JSObject* UnprivilegedJunkScope() { return mUnprivilegedJunkScope; }
    JSObject* PrivilegedJunkScope() { return mPrivilegedJunkScope; }
    JSObject* CompilationScope() { return mCompilationScope; }

    void InitSingletonScopes();
    void DeleteSingletonScopes();

private:
    explicit XPCJSRuntime(JSContext* aCx);

    MOZ_IS_CLASS_INIT
    void Initialize(JSContext* cx);
    void Shutdown(JSContext* cx) override;

    void ReleaseIncrementally(nsTArray<nsISupports*>& array);

    static const char* const mStrings[XPCJSContext::IDX_TOTAL_COUNT];
    jsid mStrIDs[XPCJSContext::IDX_TOTAL_COUNT];
    JS::Value mStrJSVals[XPCJSContext::IDX_TOTAL_COUNT];

    JSObject2WrappedJSMap*   mWrappedJSMap;
    IID2WrappedJSClassMap*   mWrappedJSClassMap;
    IID2NativeInterfaceMap*  mIID2NativeInterfaceMap;
    ClassInfo2NativeSetMap*  mClassInfo2NativeSetMap;
    NativeSetMap*            mNativeSetMap;
    IID2ThisTranslatorMap*   mThisTranslatorMap;
    XPCWrappedNativeProtoMap* mDyingWrappedNativeProtoMap;
    bool mGCIsRunning;
    nsTArray<nsISupports*> mNativesToReleaseArray;
    bool mDoingFinalization;
    XPCRootSetElem* mVariantRoots;
    XPCRootSetElem* mWrappedJSRoots;
    XPCRootSetElem* mObjectHolderRoots;
    nsTArray<xpcGCCallback> extraGCCallbacks;
    JS::GCSliceCallback mPrevGCSliceCallback;
    JS::DoCycleCollectionCallback mPrevDoCycleCollectionCallback;
    JS::PersistentRootedObject mUnprivilegedJunkScope;
    JS::PersistentRootedObject mPrivilegedJunkScope;
    JS::PersistentRootedObject mCompilationScope;
    RefPtr<AsyncFreeSnowWhite> mAsyncSnowWhiteFreer;

    friend class XPCJSContext;
    friend class XPCIncrementalReleaseRunnable;
};

inline JS::HandleId
XPCJSContext::GetStringID(unsigned index) const
{
    return Runtime()->GetStringID(index);
}

inline const char*
XPCJSContext::GetStringName(unsigned index) const
{
    return Runtime()->GetStringName(index);
}

/***************************************************************************/

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

class MOZ_STACK_CLASS XPCCallContext final : public nsAXPCNativeCallContext
{
public:
    NS_IMETHOD GetCallee(nsISupports** aResult);
    NS_IMETHOD GetCalleeMethodIndex(uint16_t* aResult);
    NS_IMETHOD GetJSContext(JSContext** aResult);
    NS_IMETHOD GetArgc(uint32_t* aResult);
    NS_IMETHOD GetArgvPtr(JS::Value** aResult);
    NS_IMETHOD GetCalleeInterface(nsIInterfaceInfo** aResult);
    NS_IMETHOD GetCalleeClassInfo(nsIClassInfo** aResult);
    NS_IMETHOD GetPreviousCallContext(nsAXPCNativeCallContext** aResult);

    enum {NO_ARGS = (unsigned) -1};

    explicit XPCCallContext(JSContext* cx,
                            JS::HandleObject obj    = nullptr,
                            JS::HandleObject funobj = nullptr,
                            JS::HandleId id         = JSID_VOIDHANDLE,
                            unsigned argc           = NO_ARGS,
                            JS::Value* argv         = nullptr,
                            JS::Value* rval         = nullptr);

    virtual ~XPCCallContext();

    inline bool                         IsValid() const ;

    inline XPCJSContext*                GetContext() const ;
    inline JSContext*                   GetJSContext() const ;
    inline bool                         GetContextPopRequired() const ;
    inline XPCCallContext*              GetPrevCallContext() const ;

    inline JSObject*                    GetFlattenedJSObject() const ;
    inline nsISupports*                 GetIdentityObject() const ;
    inline XPCWrappedNative*            GetWrapper() const ;
    inline XPCWrappedNativeProto*       GetProto() const ;

    inline bool                         CanGetTearOff() const ;
    inline XPCWrappedNativeTearOff*     GetTearOff() const ;

    inline nsIXPCScriptable*            GetScriptable() const ;
    inline bool                         CanGetSet() const ;
    inline XPCNativeSet*                GetSet() const ;
    inline bool                         CanGetInterface() const ;
    inline XPCNativeInterface*          GetInterface() const ;
    inline XPCNativeMember*             GetMember() const ;
    inline bool                         HasInterfaceAndMember() const ;
    inline jsid                         GetName() const ;
    inline bool                         GetStaticMemberIsLocal() const ;
    inline unsigned                     GetArgc() const ;
    inline JS::Value*                   GetArgv() const ;
    inline JS::Value*                   GetRetVal() const ;

    inline uint16_t                     GetMethodIndex() const ;
    inline void                         SetMethodIndex(uint16_t index) ;

    inline jsid GetResolveName() const;
    inline jsid SetResolveName(JS::HandleId name);

    inline XPCWrappedNative* GetResolvingWrapper() const;
    inline XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w);

    inline void SetRetVal(const JS::Value& val);

    void SetName(jsid name);
    void SetArgsAndResultPtr(unsigned argc, JS::Value* argv, JS::Value* rval);
    void SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                     bool isSetter);

    nsresult  CanCallNow();

    void SystemIsBeingShutDown();

    operator JSContext*() const {return GetJSContext();}

private:

    // no copy ctor or assignment allowed
    XPCCallContext(const XPCCallContext& r) = delete;
    XPCCallContext& operator= (const XPCCallContext& r) = delete;

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
inline void CHECK_STATE(int s) const {MOZ_ASSERT(mState >= s, "bad state");}
#else
#define CHECK_STATE(s) ((void)0)
#endif

private:
    JSAutoRequest                   mAr;
    State                           mState;

    RefPtr<nsXPConnect>           mXPC;

    XPCJSContext*                   mXPCJSContext;
    JSContext*                      mJSContext;

    // ctor does not necessarily init the following. BEWARE!

    XPCCallContext*                 mPrevCallContext;

    XPCWrappedNative*               mWrapper;
    XPCWrappedNativeTearOff*        mTearOff;

    nsCOMPtr<nsIXPCScriptable>      mScriptable;

    RefPtr<XPCNativeSet>            mSet;
    RefPtr<XPCNativeInterface>      mInterface;
    XPCNativeMember*                mMember;

    JS::RootedId                    mName;
    bool                            mStaticMemberIsLocal;

    unsigned                        mArgc;
    JS::Value*                      mArgv;
    JS::Value*                      mRetVal;

    uint16_t                        mMethodIndex;
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

extern const js::Class XPC_WN_NoHelper_JSClass;
extern const js::Class XPC_WN_NoMods_Proto_JSClass;
extern const js::Class XPC_WN_ModsAllowed_Proto_JSClass;
extern const js::Class XPC_WN_Tearoff_JSClass;
#define XPC_WN_TEAROFF_RESERVED_SLOTS 1
#define XPC_WN_TEAROFF_FLAT_OBJECT_SLOT 0
extern const js::Class XPC_WN_NoHelper_Proto_JSClass;

extern bool
XPC_WN_CallMethod(JSContext* cx, unsigned argc, JS::Value* vp);

extern bool
XPC_WN_GetterSetter(JSContext* cx, unsigned argc, JS::Value* vp);

// Maybe this macro should check for class->enumerate ==
// XPC_WN_Shared_Proto_Enumerate or something rather than checking for
// 4 classes?
static inline bool IS_PROTO_CLASS(const js::Class* clazz)
{
    return clazz == &XPC_WN_NoMods_Proto_JSClass ||
           clazz == &XPC_WN_ModsAllowed_Proto_JSClass;
}

typedef js::HashSet<size_t,
                    js::DefaultHasher<size_t>,
                    js::SystemAllocPolicy> InterpositionWhitelist;

struct InterpositionWhitelistPair {
    nsIAddonInterposition* interposition;
    InterpositionWhitelist whitelist;
};

typedef nsTArray<InterpositionWhitelistPair> InterpositionWhitelistArray;

/***************************************************************************/
// XPCWrappedNativeScope is one-to-one with a JS global object.

class nsIAddonInterposition;
class nsXPCComponentsBase;
class XPCWrappedNativeScope final
{
public:

    XPCJSRuntime*
    GetRuntime() const {return XPCJSRuntime::Get();}

    Native2WrappedNativeMap*
    GetWrappedNativeMap() const {return mWrappedNativeMap;}

    ClassInfo2WrappedNativeProtoMap*
    GetWrappedNativeProtoMap() const {return mWrappedNativeProtoMap;}

    nsXPCComponentsBase*
    GetComponents() const {return mComponents;}

    // Forces the creation of a privileged |Components| object, even in
    // content scopes. This will crash if used outside of automation.
    void
    ForcePrivilegedComponents();

    bool AttachComponentsObject(JSContext* aCx);

    // Returns the JS object reflection of the Components object.
    bool
    GetComponentsJSObject(JS::MutableHandleObject obj);

    JSObject*
    GetGlobalJSObject() const {
        return mGlobalJSObject;
    }

    JSObject*
    GetGlobalJSObjectPreserveColor() const {return mGlobalJSObject.unbarrieredGet();}

    nsIPrincipal*
    GetPrincipal() const {
        JSCompartment* c = js::GetObjectCompartment(mGlobalJSObject);
        return nsJSPrincipals::get(JS_GetCompartmentPrincipals(c));
    }

    JSObject*
    GetExpandoChain(JS::HandleObject target);

    bool
    SetExpandoChain(JSContext* cx, JS::HandleObject target, JS::HandleObject chain);

    static void
    SystemIsBeingShutDown();

    static void
    TraceWrappedNativesInAllScopes(JSTracer* trc);

    void TraceSelf(JSTracer* trc) {
        MOZ_ASSERT(mGlobalJSObject);
        mGlobalJSObject.trace(trc, "XPCWrappedNativeScope::mGlobalJSObject");
    }

    void TraceInside(JSTracer* trc) {
        if (mContentXBLScope)
            mContentXBLScope.trace(trc, "XPCWrappedNativeScope::mXBLScope");
        for (size_t i = 0; i < mAddonScopes.Length(); i++)
            mAddonScopes[i].trace(trc, "XPCWrappedNativeScope::mAddonScopes");
        if (mXrayExpandos.initialized())
            mXrayExpandos.trace(trc);
    }

    static void
    SuspectAllWrappers(nsCycleCollectionNoteRootCallback& cb);

    static void
    SweepAllWrappedNativeTearOffs();

    static void
    UpdateWeakPointersInAllScopesAfterGC();

    void
    UpdateWeakPointersAfterGC();

    static void
    KillDyingScopes();

    static void
    DebugDumpAllScopes(int16_t depth);

    void
    DebugDump(int16_t depth);

    struct ScopeSizeInfo {
        explicit ScopeSizeInfo(mozilla::MallocSizeOf mallocSizeOf)
            : mMallocSizeOf(mallocSizeOf),
              mScopeAndMapSize(0),
              mProtoAndIfaceCacheSize(0)
        {}

        mozilla::MallocSizeOf mMallocSizeOf;
        size_t mScopeAndMapSize;
        size_t mProtoAndIfaceCacheSize;
    };

    static void
    AddSizeOfAllScopesIncludingThis(ScopeSizeInfo* scopeSizeInfo);

    void
    AddSizeOfIncludingThis(ScopeSizeInfo* scopeSizeInfo);

    static bool
    IsDyingScope(XPCWrappedNativeScope* scope);

    typedef js::HashMap<JSAddonId*,
                        nsCOMPtr<nsIAddonInterposition>,
                        js::PointerHasher<JSAddonId*, 3>,
                        js::SystemAllocPolicy> InterpositionMap;

    typedef js::HashSet<JSAddonId*,
                        js::PointerHasher<JSAddonId*, 3>,
                        js::SystemAllocPolicy> AddonSet;

    // Gets the appropriate scope object for XBL in this scope. The context
    // must be same-compartment with the global upon entering, and the scope
    // object is wrapped into the compartment of the global.
    JSObject* EnsureContentXBLScope(JSContext* cx);

    JSObject* EnsureAddonScope(JSContext* cx, JSAddonId* addonId);

    XPCWrappedNativeScope(JSContext* cx, JS::HandleObject aGlobal);

    nsAutoPtr<JSObject2JSObjectMap> mWaiverWrapperMap;

    bool IsContentXBLScope() { return mIsContentXBLScope; }
    bool AllowContentXBLScope();
    bool UseContentXBLScope() { return mUseContentXBLScope; }
    void ClearContentXBLScope() { mContentXBLScope = nullptr; }

    bool IsAddonScope() { return mIsAddonScope; }

    bool HasInterposition() { return mInterposition; }
    nsCOMPtr<nsIAddonInterposition> GetInterposition();

    static bool SetAddonInterposition(JSContext* cx,
                                      JSAddonId* addonId,
                                      nsIAddonInterposition* interp);

    static InterpositionWhitelist* GetInterpositionWhitelist(nsIAddonInterposition* interposition);
    static bool UpdateInterpositionWhitelist(JSContext* cx,
                                             nsIAddonInterposition* interposition);

    void SetAddonCallInterposition() { mHasCallInterpositions = true; }
    bool HasCallInterposition() { return mHasCallInterpositions; };

    static bool AllowCPOWsInAddon(JSContext* cx, JSAddonId* addonId, bool allow);

protected:
    virtual ~XPCWrappedNativeScope();

    XPCWrappedNativeScope() = delete;

private:
    class ClearInterpositionsObserver final : public nsIObserver {
        ~ClearInterpositionsObserver() {}

      public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER
    };

    static XPCWrappedNativeScope* gScopes;
    static XPCWrappedNativeScope* gDyingScopes;

    static bool                      gShutdownObserverInitialized;
    static InterpositionMap*         gInterpositionMap;
    static AddonSet*                 gAllowCPOWAddonSet;

    static InterpositionWhitelistArray* gInterpositionWhitelists;

    Native2WrappedNativeMap*         mWrappedNativeMap;
    ClassInfo2WrappedNativeProtoMap* mWrappedNativeProtoMap;
    RefPtr<nsXPCComponentsBase>    mComponents;
    XPCWrappedNativeScope*           mNext;
    // The JS global object for this scope.  If non-null, this will be the
    // default parent for the XPCWrappedNatives that have us as the scope,
    // unless a PreCreate hook overrides it.  Note that this _may_ be null (see
    // constructor).
    JS::ObjectPtr                    mGlobalJSObject;

    // XBL Scope. This is is a lazily-created sandbox for non-system scopes.
    // EnsureContentXBLScope() decides whether it needs to be created or not.
    // This reference is wrapped into the compartment of mGlobalJSObject.
    JS::ObjectPtr                    mContentXBLScope;

    // Lazily created sandboxes for addon code.
    nsTArray<JS::ObjectPtr>          mAddonScopes;

    // This is a service that will be use to interpose on some property accesses on
    // objects from other scope, for add-on compatibility reasons.
    nsCOMPtr<nsIAddonInterposition>  mInterposition;

    // If this flag is set, we intercept function calls on vanilla JS function objects
    // from this scope if the caller scope has mInterposition set.
    bool mHasCallInterpositions;

    JS::WeakMapPtr<JSObject*, JSObject*> mXrayExpandos;

    bool mIsContentXBLScope;
    bool mIsAddonScope;

    // For remote XUL domains, we run all XBL in the content scope for compat
    // reasons (though we sometimes pref this off for automation). We separately
    // track the result of this decision (mAllowContentXBLScope), from the decision
    // of whether to actually _use_ an XBL scope (mUseContentXBLScope), which depends
    // on the type of global and whether the compartment is system principal
    // or not.
    //
    // This distinction is useful primarily because, if true, we know that we
    // have no way of distinguishing XBL script from content script for the
    // given scope. In these (unsupported) situations, we just always claim to
    // be XBL.
    bool mAllowContentXBLScope;
    bool mUseContentXBLScope;
};

/***************************************************************************/
// Slots we use for our functions
#define XPC_FUNCTION_NATIVE_MEMBER_SLOT 0
#define XPC_FUNCTION_PARENT_OBJECT_SLOT 1

/***************************************************************************/
// XPCNativeMember represents a single idl declared method, attribute or
// constant.

// Tight. No virtual methods. Can be bitwise copied (until any resolution done).

class XPCNativeMember final
{
public:
    static bool GetCallInfo(JSObject* funobj,
                            RefPtr<XPCNativeInterface>* pInterface,
                            XPCNativeMember** pMember);

    jsid   GetName() const {return mName;}

    uint16_t GetIndex() const {return mIndex;}

    bool GetConstantValue(XPCCallContext& ccx, XPCNativeInterface* iface,
                          JS::Value* pval)
        {MOZ_ASSERT(IsConstant(),
                    "Only call this if you're sure this is a constant!");
         return Resolve(ccx, iface, nullptr, pval);}

    bool NewFunctionObject(XPCCallContext& ccx, XPCNativeInterface* iface,
                           JS::HandleObject parent, JS::Value* pval);

    bool IsMethod() const
        {return 0 != (mFlags & METHOD);}

    bool IsConstant() const
        {return 0 != (mFlags & CONSTANT);}

    bool IsAttribute() const
        {return 0 != (mFlags & GETTER);}

    bool IsWritableAttribute() const
        {return 0 != (mFlags & SETTER_TOO);}

    bool IsReadOnlyAttribute() const
        {return IsAttribute() && !IsWritableAttribute();}


    void SetName(jsid a) {mName = a;}

    void SetMethod(uint16_t index)
        {mFlags = METHOD; mIndex = index;}

    void SetConstant(uint16_t index)
        {mFlags = CONSTANT; mIndex = index;}

    void SetReadOnlyAttribute(uint16_t index)
        {mFlags = GETTER; mIndex = index;}

    void SetWritableAttribute()
        {MOZ_ASSERT(mFlags == GETTER,"bad"); mFlags = GETTER | SETTER_TOO;}

    static uint16_t GetMaxIndexInInterface()
        {return (1<<12) - 1;}

    inline XPCNativeInterface* GetInterface() const;

    void SetIndexInInterface(uint16_t index)
        {mIndexInInterface = index;}

    /* default ctor - leave random contents */
    XPCNativeMember()  {MOZ_COUNT_CTOR(XPCNativeMember);}
    ~XPCNativeMember() {MOZ_COUNT_DTOR(XPCNativeMember);}

private:
    bool Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                 JS::HandleObject parent, JS::Value* vp);

    enum {
        METHOD      = 0x01,
        CONSTANT    = 0x02,
        GETTER      = 0x04,
        SETTER_TOO  = 0x08
        // If you add a flag here, you may need to make mFlags wider and either
        // make mIndexInInterface narrower (and adjust
        // XPCNativeInterface::NewInstance accordingly) or make this object
        // bigger.
    };

private:
    // our only data...
    jsid     mName;
    uint16_t mIndex;
    // mFlags needs to be wide enogh to hold the flags in the above enum.
    uint16_t mFlags : 4;
    // mIndexInInterface is the index of this in our XPCNativeInterface's
    // mMembers.  In theory our XPCNativeInterface could have as many as 2^15-1
    // members (since mMemberCount is 15-bit) but in practice we prevent
    // creation of XPCNativeInterfaces which have more than 2^12 members.
    // If the width of this field changes, update GetMaxIndexInInterface.
    uint16_t mIndexInInterface : 12;
} JS_HAZ_NON_GC_POINTER; // Only stores a pinned string

/***************************************************************************/
// XPCNativeInterface represents a single idl declared interface. This is
// primarily the set of XPCNativeMembers.

// Tight. No virtual methods.

class XPCNativeInterface final
{
  public:
    NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(XPCNativeInterface,
                                            DestroyInstance(this))

    static already_AddRefed<XPCNativeInterface> GetNewOrUsed(const nsIID* iid);
    static already_AddRefed<XPCNativeInterface> GetNewOrUsed(nsIInterfaceInfo* info);
    static already_AddRefed<XPCNativeInterface> GetNewOrUsed(const char* name);
    static already_AddRefed<XPCNativeInterface> GetISupports();

    inline nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo.get();}
    inline jsid              GetName()          const {return mName;}

    inline const nsIID* GetIID() const;
    inline const char*  GetNameString() const;
    inline XPCNativeMember* FindMember(jsid name) const;

    inline bool HasAncestor(const nsIID* iid) const;
    static inline size_t OffsetOfMembers();

    uint16_t GetMemberCount() const {
        return mMemberCount;
    }
    XPCNativeMember* GetMemberAt(uint16_t i) {
        MOZ_ASSERT(i < mMemberCount, "bad index");
        return &mMembers[i];
    }

    void DebugDump(int16_t depth);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

  protected:
    static already_AddRefed<XPCNativeInterface> NewInstance(nsIInterfaceInfo* aInfo);

    XPCNativeInterface() = delete;
    XPCNativeInterface(nsIInterfaceInfo* aInfo, jsid aName)
      : mInfo(aInfo), mName(aName), mMemberCount(0)
    {}
    ~XPCNativeInterface();

    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

    XPCNativeInterface(const XPCNativeInterface& r) = delete;
    XPCNativeInterface& operator= (const XPCNativeInterface& r) = delete;

    static void DestroyInstance(XPCNativeInterface* inst);

private:
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    jsid                       mName;
    uint16_t                   mMemberCount;
    XPCNativeMember            mMembers[1]; // always last - object sized for array
};

/***************************************************************************/
// XPCNativeSetKey is used to key a XPCNativeSet in a NativeSetMap.
// It represents a new XPCNativeSet we are considering constructing, without
// requiring that the set actually be built.

class MOZ_STACK_CLASS XPCNativeSetKey final
{
public:
    // This represents an existing set |baseSet|.
    explicit XPCNativeSetKey(XPCNativeSet* baseSet)
        : mBaseSet(baseSet), mAddition(nullptr)
    {
        MOZ_ASSERT(baseSet);
    }

    // This represents a new set containing only nsISupports and
    // |addition|.
    explicit XPCNativeSetKey(XPCNativeInterface* addition)
        : mBaseSet(nullptr), mAddition(addition)
    {
        MOZ_ASSERT(addition);
    }

    // This represents the existing set |baseSet| with the interface
    // |addition| inserted after existing interfaces. |addition| must
    // not already be present in |baseSet|.
    explicit XPCNativeSetKey(XPCNativeSet* baseSet,
                             XPCNativeInterface* addition);
    ~XPCNativeSetKey() {}

    XPCNativeSet* GetBaseSet() const {return mBaseSet;}
    XPCNativeInterface* GetAddition() const {return mAddition;}

    PLDHashNumber Hash() const;

    // Allow shallow copy

private:
    RefPtr<XPCNativeSet> mBaseSet;
    RefPtr<XPCNativeInterface> mAddition;
};

/***************************************************************************/
// XPCNativeSet represents an ordered collection of XPCNativeInterface pointers.

class XPCNativeSet final
{
  public:
    NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(XPCNativeSet,
                                            DestroyInstance(this))

    static already_AddRefed<XPCNativeSet> GetNewOrUsed(const nsIID* iid);
    static already_AddRefed<XPCNativeSet> GetNewOrUsed(nsIClassInfo* classInfo);
    static already_AddRefed<XPCNativeSet> GetNewOrUsed(XPCNativeSetKey* key);

    // This generates a union set.
    //
    // If preserveFirstSetOrder is true, the elements from |firstSet| come first,
    // followed by any non-duplicate items from |secondSet|. If false, the same
    // algorithm is applied; but if we detect that |secondSet| is a superset of
    // |firstSet|, we return |secondSet| without worrying about whether the
    // ordering might differ from |firstSet|.
    static already_AddRefed<XPCNativeSet> GetNewOrUsed(XPCNativeSet* firstSet,
                                                       XPCNativeSet* secondSet,
                                                       bool preserveFirstSetOrder);

    static void ClearCacheEntryForClassInfo(nsIClassInfo* classInfo);

    inline bool FindMember(jsid name, XPCNativeMember** pMember,
                           uint16_t* pInterfaceIndex) const;

    inline bool FindMember(jsid name, XPCNativeMember** pMember,
                           RefPtr<XPCNativeInterface>* pInterface) const;

    inline bool FindMember(JS::HandleId name,
                           XPCNativeMember** pMember,
                           RefPtr<XPCNativeInterface>* pInterface,
                           XPCNativeSet* protoSet,
                           bool* pIsLocal) const;

    inline bool HasInterface(XPCNativeInterface* aInterface) const;
    inline bool HasInterfaceWithAncestor(XPCNativeInterface* aInterface) const;
    inline bool HasInterfaceWithAncestor(const nsIID* iid) const;

    inline XPCNativeInterface* FindInterfaceWithIID(const nsIID& iid) const;

    inline XPCNativeInterface* FindNamedInterface(jsid name) const;

    uint16_t GetMemberCount() const {
        return mMemberCount;
    }
    uint16_t GetInterfaceCount() const {
        return mInterfaceCount;
    }
    XPCNativeInterface** GetInterfaceArray() {
        return mInterfaces;
    }

    XPCNativeInterface* GetInterfaceAt(uint16_t i)
        {MOZ_ASSERT(i < mInterfaceCount, "bad index"); return mInterfaces[i];}

    inline bool MatchesSetUpToInterface(const XPCNativeSet* other,
                                          XPCNativeInterface* iface) const;

    void DebugDump(int16_t depth);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

  protected:
    static already_AddRefed<XPCNativeSet> NewInstance(nsTArray<RefPtr<XPCNativeInterface>>&& array);
    static already_AddRefed<XPCNativeSet> NewInstanceMutate(XPCNativeSetKey* key);

    XPCNativeSet()
      : mMemberCount(0), mInterfaceCount(0)
    {}
    ~XPCNativeSet();
    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

    static void DestroyInstance(XPCNativeSet* inst);

  private:
    uint16_t                mMemberCount;
    uint16_t                mInterfaceCount;
    // Always last - object sized for array.
    // These are strong references.
    XPCNativeInterface*     mInterfaces[1];
};

/***********************************************/
// XPCWrappedNativeProto hold the additional shared wrapper data
// for XPCWrappedNative whose native objects expose nsIClassInfo.

class XPCWrappedNativeProto final
{
public:
    static XPCWrappedNativeProto*
    GetNewOrUsed(XPCWrappedNativeScope* scope,
                 nsIClassInfo* classInfo,
                 nsIXPCScriptable* scriptable,
                 bool callPostCreatePrototype = true);

    XPCWrappedNativeScope*
    GetScope()   const {return mScope;}

    XPCJSRuntime*
    GetRuntime() const {return mScope->GetRuntime();}

    JSObject*
    GetJSProtoObject() const { return mJSProtoObject; }

    JSObject*
    GetJSProtoObjectPreserveColor() const { return mJSProtoObject.unbarrieredGet(); }

    nsIClassInfo*
    GetClassInfo()     const {return mClassInfo;}

    XPCNativeSet*
    GetSet()           const {return mSet;}

    nsIXPCScriptable*
    GetScriptable() const { return mScriptable; }

    bool CallPostCreatePrototype();
    void JSProtoObjectFinalized(js::FreeOp* fop, JSObject* obj);
    void JSProtoObjectMoved(JSObject* obj, const JSObject* old);

    void SystemIsBeingShutDown();

    void DebugDump(int16_t depth);

    void TraceSelf(JSTracer* trc) {
        if (mJSProtoObject)
            mJSProtoObject.trace(trc, "XPCWrappedNativeProto::mJSProtoObject");
    }

    void TraceInside(JSTracer* trc) {
        GetScope()->TraceSelf(trc);
    }

    void TraceJS(JSTracer* trc) {
        TraceSelf(trc);
        TraceInside(trc);
    }

    void WriteBarrierPre(JSContext* cx)
    {
        if (JS::IsIncrementalBarrierNeeded(cx) && mJSProtoObject)
            mJSProtoObject.writeBarrierPre(cx);
    }

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    void Mark() const {}
    inline void AutoTrace(JSTracer* trc) {}

    ~XPCWrappedNativeProto();

protected:
    // disable copy ctor and assignment
    XPCWrappedNativeProto(const XPCWrappedNativeProto& r) = delete;
    XPCWrappedNativeProto& operator= (const XPCWrappedNativeProto& r) = delete;

    // hide ctor
    XPCWrappedNativeProto(XPCWrappedNativeScope* Scope,
                          nsIClassInfo* ClassInfo,
                          already_AddRefed<XPCNativeSet>&& Set);

    bool Init(nsIXPCScriptable* scriptable, bool callPostCreatePrototype);

private:
#ifdef DEBUG
    static int32_t gDEBUG_LiveProtoCount;
#endif

private:
    XPCWrappedNativeScope*   mScope;
    JS::ObjectPtr            mJSProtoObject;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    RefPtr<XPCNativeSet>     mSet;
    nsCOMPtr<nsIXPCScriptable> mScriptable;
};

/***********************************************/
// XPCWrappedNativeTearOff represents the info needed to make calls to one
// interface on the underlying native object of a XPCWrappedNative.

class XPCWrappedNativeTearOff final
{
public:
    bool IsAvailable() const {return mInterface == nullptr;}
    bool IsReserved()  const {return mInterface == (XPCNativeInterface*)1;}
    bool IsValid()     const {return !IsAvailable() && !IsReserved();}
    void   SetReserved()       {mInterface = (XPCNativeInterface*)1;}

    XPCNativeInterface* GetInterface() const {return mInterface;}
    nsISupports*        GetNative()    const {return mNative;}
    JSObject*           GetJSObject();
    JSObject*           GetJSObjectPreserveColor() const;
    void SetInterface(XPCNativeInterface*  Interface) {mInterface = Interface;}
    void SetNative(nsISupports*  Native)              {mNative = Native;}
    already_AddRefed<nsISupports> TakeNative() { return mNative.forget(); }
    void SetJSObject(JSObject*  JSObj);

    void JSObjectFinalized() {SetJSObject(nullptr);}
    void JSObjectMoved(JSObject* obj, const JSObject* old);

    XPCWrappedNativeTearOff()
        : mInterface(nullptr), mJSObject(nullptr)
    {
        MOZ_COUNT_CTOR(XPCWrappedNativeTearOff);
    }
    ~XPCWrappedNativeTearOff();

    // NOP. This is just here to make the AutoMarkingPtr code compile.
    inline void TraceJS(JSTracer* trc) {}
    inline void AutoTrace(JSTracer* trc) {}

    void Mark()       {mJSObject.setFlags(1);}
    void Unmark()     {mJSObject.unsetFlags(1);}
    bool IsMarked() const {return mJSObject.hasFlag(1);}

    XPCWrappedNativeTearOff* AddTearOff()
    {
        MOZ_ASSERT(!mNextTearOff);
        mNextTearOff = mozilla::MakeUnique<XPCWrappedNativeTearOff>();
        return mNextTearOff.get();
    }

    XPCWrappedNativeTearOff* GetNextTearOff() {return mNextTearOff.get();}

private:
    XPCWrappedNativeTearOff(const XPCWrappedNativeTearOff& r) = delete;
    XPCWrappedNativeTearOff& operator= (const XPCWrappedNativeTearOff& r) = delete;

private:
    XPCNativeInterface* mInterface;
    // mNative is an nsRefPtr not an nsCOMPtr because it may not be the canonical
    // nsISupports pointer.
    RefPtr<nsISupports> mNative;
    JS::TenuredHeap<JSObject*> mJSObject;
    mozilla::UniquePtr<XPCWrappedNativeTearOff> mNextTearOff;
};


/***************************************************************************/
// XPCWrappedNative the wrapper around one instance of a native xpcom object
// to be used from JavaScript.

class XPCWrappedNative final : public nsIXPConnectWrappedNative
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDNATIVE

    NS_DECL_CYCLE_COLLECTION_CLASS(XPCWrappedNative)

    nsIPrincipal* GetObjectPrincipal() const;

    bool
    IsValid() const { return mFlatJSObject.hasFlag(FLAT_JS_OBJECT_VALID); }

#define XPC_SCOPE_WORD(s)   (intptr_t(s))
#define XPC_SCOPE_MASK      (intptr_t(0x3))
#define XPC_SCOPE_TAG       (intptr_t(0x1))
#define XPC_WRAPPER_EXPIRED (intptr_t(0x2))

    static inline bool
    IsTaggedScope(XPCWrappedNativeScope* s)
        {return XPC_SCOPE_WORD(s) & XPC_SCOPE_TAG;}

    static inline XPCWrappedNativeScope*
    TagScope(XPCWrappedNativeScope* s)
        {MOZ_ASSERT(!IsTaggedScope(s), "bad pointer!");
         return (XPCWrappedNativeScope*)(XPC_SCOPE_WORD(s) | XPC_SCOPE_TAG);}

    static inline XPCWrappedNativeScope*
    UnTagScope(XPCWrappedNativeScope* s)
        {return (XPCWrappedNativeScope*)(XPC_SCOPE_WORD(s) & ~XPC_SCOPE_TAG);}

    inline bool
    IsWrapperExpired() const
        {return XPC_SCOPE_WORD(mMaybeScope) & XPC_WRAPPER_EXPIRED;}

    bool
    HasProto() const {return !IsTaggedScope(mMaybeScope);}

    XPCWrappedNativeProto*
    GetProto() const
        {return HasProto() ?
         (XPCWrappedNativeProto*)
         (XPC_SCOPE_WORD(mMaybeProto) & ~XPC_SCOPE_MASK) : nullptr;}

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
    JSObject* GetFlatJSObject() const { return mFlatJSObject; }

    /**
     * This getter does not change the color of the JSObject meaning that the
     * object returned is not guaranteed to be kept alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    JSObject*
    GetFlatJSObjectPreserveColor() const {
        return mFlatJSObject.unbarrieredGetPtr();
    }

    XPCNativeSet*
    GetSet() const {return mSet;}

    void
    SetSet(already_AddRefed<XPCNativeSet> set) {mSet = set;}

    static XPCWrappedNative* Get(JSObject* obj) {
        MOZ_ASSERT(IS_WN_REFLECTOR(obj));
        return (XPCWrappedNative*)js::GetObjectPrivate(obj);
    }

private:
    inline void
    ExpireWrapper()
        {mMaybeScope = (XPCWrappedNativeScope*)
                       (XPC_SCOPE_WORD(mMaybeScope) | XPC_WRAPPER_EXPIRED);}

public:

    nsIXPCScriptable*
    GetScriptable() const { return mScriptable; }

    nsIClassInfo*
    GetClassInfo() const {return IsValid() && HasProto() ?
                            GetProto()->GetClassInfo() : nullptr;}

    bool
    HasMutatedSet() const {return IsValid() &&
                                  (!HasProto() ||
                                   GetSet() != GetProto()->GetSet());}

    XPCJSRuntime*
    GetRuntime() const {XPCWrappedNativeScope* scope = GetScope();
                        return scope ? scope->GetRuntime() : nullptr;}

    static nsresult
    WrapNewGlobal(xpcObjectHelper& nativeHelper,
                  nsIPrincipal* principal, bool initStandardClasses,
                  JS::CompartmentOptions& aOptions,
                  XPCWrappedNative** wrappedGlobal);

    static nsresult
    GetNewOrUsed(xpcObjectHelper& helper,
                 XPCWrappedNativeScope* Scope,
                 XPCNativeInterface* Interface,
                 XPCWrappedNative** wrapper);

    static nsresult
    GetUsedOnly(nsISupports* Object,
                XPCWrappedNativeScope* Scope,
                XPCNativeInterface* Interface,
                XPCWrappedNative** wrapper);

    void FlatJSObjectFinalized();
    void FlatJSObjectMoved(JSObject* obj, const JSObject* old);

    void SystemIsBeingShutDown();

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    static bool CallMethod(XPCCallContext& ccx,
                           CallMode mode = CALL_METHOD);

    static bool GetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_GETTER);}

    static bool SetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_SETTER);}

    inline bool HasInterfaceNoQI(const nsIID& iid);

    XPCWrappedNativeTearOff* FindTearOff(XPCNativeInterface* aInterface,
                                         bool needJSObject = false,
                                         nsresult* pError = nullptr);
    XPCWrappedNativeTearOff* FindTearOff(const nsIID& iid);

    void Mark() const {}

    inline void TraceInside(JSTracer* trc) {
        if (HasProto())
            GetProto()->TraceSelf(trc);
        else
            GetScope()->TraceSelf(trc);

        JSObject* obj = mFlatJSObject.unbarrieredGetPtr();
        if (obj && JS_IsGlobalObject(obj)) {
            xpc::TraceXPCGlobal(trc, obj);
        }
    }

    void TraceJS(JSTracer* trc) {
        TraceInside(trc);
    }

    void TraceSelf(JSTracer* trc) {
        // If this got called, we're being kept alive by someone who really
        // needs us alive and whole.  Do not let our mFlatJSObject go away.
        // This is the only time we should be tracing our mFlatJSObject,
        // normally somebody else is doing that.
        JS::TraceEdge(trc, &mFlatJSObject, "XPCWrappedNative::mFlatJSObject");
    }

    static void Trace(JSTracer* trc, JSObject* obj);

    void AutoTrace(JSTracer* trc) {
        TraceSelf(trc);
    }

    inline void SweepTearOffs();

    // Returns a string that shuld be free'd using JS_smprintf_free (or null).
    char* ToString(XPCWrappedNativeTearOff* to = nullptr) const;

    static nsIXPCScriptable* GatherProtoScriptable(nsIClassInfo* classInfo);

    bool HasExternalReference() const {return mRefCnt > 1;}

    void Suspect(nsCycleCollectionNoteRootCallback& cb);
    void NoteTearoffs(nsCycleCollectionTraversalCallback& cb);

    // Make ctor and dtor protected (rather than private) to placate nsCOMPtr.
protected:
    XPCWrappedNative() = delete;

    // This ctor is used if this object will have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                     XPCWrappedNativeProto* aProto);

    // This ctor is used if this object will NOT have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                     XPCWrappedNativeScope* aScope,
                     already_AddRefed<XPCNativeSet>&& aSet);

    virtual ~XPCWrappedNative();
    void Destroy();

private:
    enum {
        // Flags bits for mFlatJSObject:
        FLAT_JS_OBJECT_VALID = JS_BIT(0)
    };

    bool Init(nsIXPCScriptable* scriptable);
    bool FinishInit();

    bool ExtendSet(XPCNativeInterface* aInterface);

    nsresult InitTearOff(XPCWrappedNativeTearOff* aTearOff,
                         XPCNativeInterface* aInterface,
                         bool needJSObject);

    bool InitTearOffJSObject(XPCWrappedNativeTearOff* to);

public:
    static void GatherScriptable(nsISupports* obj,
                                 nsIClassInfo* classInfo,
                                 nsIXPCScriptable** scrProto,
                                 nsIXPCScriptable** scrWrapper);

private:
    union
    {
        XPCWrappedNativeScope* mMaybeScope;
        XPCWrappedNativeProto* mMaybeProto;
    };
    RefPtr<XPCNativeSet> mSet;
    JS::TenuredHeap<JSObject*> mFlatJSObject;
    nsCOMPtr<nsIXPCScriptable> mScriptable;
    XPCWrappedNativeTearOff mFirstTearOff;
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
    NS_IMETHOD DebugDump(int16_t depth) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPCWrappedJSClass,
                              NS_IXPCONNECT_WRAPPED_JS_CLASS_IID)

/*************************/
// nsXPCWrappedJSClass represents the sharable factored out common code and
// data for nsXPCWrappedJS instances for the same interface type.

class nsXPCWrappedJSClass final : public nsIXPCWrappedJSClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(int16_t depth) override;
public:

    static already_AddRefed<nsXPCWrappedJSClass>
    GetNewOrUsed(JSContext* cx,
                 REFNSIID aIID,
                 bool allowNonScriptable = false);

    REFNSIID GetIID() const {return mIID;}
    XPCJSRuntime* GetRuntime() const {return mRuntime;}
    nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo;}
    const char* GetInterfaceName();

    static bool IsWrappedJS(nsISupports* aPtr);

    NS_IMETHOD DelegatedQueryInterface(nsXPCWrappedJS* self, REFNSIID aIID,
                                       void** aInstancePtr);

    JSObject* GetRootJSObject(JSContext* cx, JSObject* aJSObj);

    NS_IMETHOD CallMethod(nsXPCWrappedJS* wrapper, uint16_t methodIndex,
                          const XPTMethodDescriptor* info,
                          nsXPTCMiniVariant* params);

    JSObject*  CallQueryInterfaceOnJSObject(JSContext* cx,
                                            JSObject* jsobj, REFNSIID aIID);

    static nsresult BuildPropertyEnumerator(XPCCallContext& ccx,
                                            JSObject* aJSObj,
                                            nsISimpleEnumerator** aEnumerate);

    static nsresult GetNamedPropertyAsVariant(XPCCallContext& ccx,
                                              JSObject* aJSObj,
                                              const nsAString& aName,
                                              nsIVariant** aResult);

private:
    // aSyntheticException, if not null, is the exception we should be using.
    // If null, look for an exception on the JSContext hanging off the
    // XPCCallContext.
    static nsresult CheckForException(XPCCallContext & ccx,
                                      mozilla::dom::AutoEntryScript& aes,
                                      const char * aPropertyName,
                                      const char * anInterfaceName,
                                      nsIException* aSyntheticException = nullptr);
    virtual ~nsXPCWrappedJSClass();

    nsXPCWrappedJSClass() = delete;
    nsXPCWrappedJSClass(JSContext* cx, REFNSIID aIID,
                        nsIInterfaceInfo* aInfo);

    bool IsReflectable(uint16_t i) const
        {return (bool)(mDescriptors[i/32] & (1 << (i%32)));}
    void SetReflectable(uint16_t i, bool b)
        {if (b) mDescriptors[i/32] |= (1 << (i%32));
         else mDescriptors[i/32] &= ~(1 << (i%32));}

    bool GetArraySizeFromParam(JSContext* cx,
                               const XPTMethodDescriptor* method,
                               const nsXPTParamInfo& param,
                               uint16_t methodIndex,
                               uint8_t paramIndex,
                               nsXPTCMiniVariant* params,
                               uint32_t* result) const;

    bool GetInterfaceTypeFromParam(JSContext* cx,
                                   const XPTMethodDescriptor* method,
                                   const nsXPTParamInfo& param,
                                   uint16_t methodIndex,
                                   const nsXPTType& type,
                                   nsXPTCMiniVariant* params,
                                   nsID* result) const;

    static void CleanupPointerArray(const nsXPTType& datum_type,
                                    uint32_t array_count,
                                    void** arrayp);

    static void CleanupPointerTypeObject(const nsXPTType& type,
                                         void** pp);

    void CleanupOutparams(JSContext* cx, uint16_t methodIndex, const nsXPTMethodInfo* info,
                          nsXPTCMiniVariant* nativeParams, bool inOutOnly, uint8_t n) const;

private:
    XPCJSRuntime* mRuntime;
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    char* mName;
    nsIID mIID;
    uint32_t* mDescriptors;
};

/*************************/
// nsXPCWrappedJS is a wrapper for a single JSObject for use from native code.
// nsXPCWrappedJS objects are chained together to represent the various
// interface on the single underlying (possibly aggregate) JSObject.

class nsXPCWrappedJS final : protected nsAutoXPTCStub,
                             public nsIXPConnectWrappedJSUnmarkGray,
                             public nsSupportsWeakReference,
                             public nsIPropertyBag,
                             public XPCRootSetElem
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDJS
    NS_DECL_NSIXPCONNECTWRAPPEDJSUNMARKGRAY
    NS_DECL_NSISUPPORTSWEAKREFERENCE
    NS_DECL_NSIPROPERTYBAG

    NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(nsXPCWrappedJS, nsIXPConnectWrappedJS)

    NS_IMETHOD CallMethod(uint16_t methodIndex,
                          const XPTMethodDescriptor* info,
                          nsXPTCMiniVariant* params) override;

    /*
    * This is rarely called directly. Instead one usually calls
    * XPCConvert::JSObject2NativeInterface which will handles cases where the
    * JS object is already a wrapped native or a DOM object.
    */

    static nsresult
    GetNewOrUsed(JS::HandleObject aJSObj,
                 REFNSIID aIID,
                 nsXPCWrappedJS** wrapper);

    nsISomeInterface* GetXPTCStub() { return mXPTCStub; }

    /**
     * This getter does not change the color of the JSObject meaning that the
     * object returned is not guaranteed to be kept alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    JSObject* GetJSObjectPreserveColor() const { return mJSObj.unbarrieredGet(); }

    // Returns true if the wrapper chain contains references to multiple
    // compartments. If the wrapper chain contains references to multiple
    // compartments, then it must be registered on the XPCJSContext. Otherwise,
    // it should be registered in the CompartmentPrivate for the compartment of
    // the root's JS object. This will only return correct results when called
    // on the root wrapper and will assert if not called on a root wrapper.
    bool IsMultiCompartment() const;

    nsXPCWrappedJSClass*  GetClass() const {return mClass;}
    REFNSIID GetIID() const {return GetClass()->GetIID();}
    nsXPCWrappedJS* GetRootWrapper() const {return mRoot;}
    nsXPCWrappedJS* GetNextWrapper() const {return mNext;}

    nsXPCWrappedJS* Find(REFNSIID aIID);
    nsXPCWrappedJS* FindInherited(REFNSIID aIID);
    nsXPCWrappedJS* FindOrFindInherited(REFNSIID aIID) {
        nsXPCWrappedJS* wrapper = Find(aIID);
        if (wrapper)
            return wrapper;
        return FindInherited(aIID);
    }

    bool IsRootWrapper() const { return mRoot == this; }
    bool IsValid() const { return bool(mJSObj); }
    void SystemIsBeingShutDown();

    // These two methods are used by JSObject2WrappedJSMap::FindDyingJSObjects
    // to find non-rooting wrappers for dying JS objects. See the top of
    // XPCWrappedJS.cpp for more details.
    bool IsSubjectToFinalization() const {return IsValid() && mRefCnt == 1;}
    void UpdateObjectPointerAfterGC() {JS_UpdateWeakPointerAfterGC(&mJSObj);}

    bool IsAggregatedToNative() const {return mRoot->mOuter != nullptr;}
    nsISupports* GetAggregatedNativeObject() const {return mRoot->mOuter;}
    void SetAggregatedNativeObject(nsISupports* aNative) {
        MOZ_ASSERT(aNative);
        if (mRoot->mOuter) {
            MOZ_ASSERT(mRoot->mOuter == aNative,
                       "Only one aggregated native can be set");
            return;
        }
        mRoot->mOuter = aNative;
    }

    void TraceJS(JSTracer* trc);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    virtual ~nsXPCWrappedJS();
protected:
    nsXPCWrappedJS() = delete;
    nsXPCWrappedJS(JSContext* cx,
                   JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root,
                   nsresult* rv);

    bool CanSkip();
    void Destroy();
    void Unlink();

private:
    JSCompartment* Compartment() const {
        return js::GetObjectCompartment(mJSObj.unbarrieredGet());
    }

    JS::Heap<JSObject*> mJSObj;
    RefPtr<nsXPCWrappedJSClass> mClass;
    nsXPCWrappedJS* mRoot;    // If mRoot != this, it is an owning pointer.
    nsXPCWrappedJS* mNext;
    nsCOMPtr<nsISupports> mOuter;    // only set in root
};

/***************************************************************************/

class XPCJSObjectHolder final : public nsIXPConnectJSObjectHolder,
                                public XPCRootSetElem
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER

    // non-interface implementation

public:
    void TraceJS(JSTracer* trc);

    explicit XPCJSObjectHolder(JSObject* obj);

private:
    virtual ~XPCJSObjectHolder();

    XPCJSObjectHolder() = delete;

    JS::Heap<JSObject*> mJSObj;
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

  xpcProperty(const char16_t* aName, uint32_t aNameLen, nsIVariant* aValue);

private:
    virtual ~xpcProperty() {}

    nsString             mName;
    nsCOMPtr<nsIVariant> mValue;
};

/***************************************************************************/
// class here just for static methods
class XPCConvert
{
public:
    static bool IsMethodReflectable(const XPTMethodDescriptor& info);

    /**
     * Convert a native object into a JS::Value.
     *
     * @param d [out] the resulting JS::Value
     * @param s the native object we're working with
     * @param type the type of object that s is
     * @param iid the interface of s that we want
     * @param scope the default scope to put on the new JSObject's parent
     *        chain
     * @param pErr [out] relevant error code, if any.
     */

    static bool NativeData2JS(JS::MutableHandleValue d,
                              const void* s, const nsXPTType& type,
                              const nsID* iid, nsresult* pErr);

    static bool JSData2Native(void* d, JS::HandleValue s,
                              const nsXPTType& type,
                              const nsID* iid,
                              nsresult* pErr);

    /**
     * Convert a native nsISupports into a JSObject.
     *
     * @param dest [out] the resulting JSObject
     * @param src the native object we're working with
     * @param iid the interface of src that we want (may be null)
     * @param cache the wrapper cache for src (may be null, in which case src
     *              will be QI'ed to get the cache)
     * @param allowNativeWrapper if true, this method may wrap the resulting
     *        JSObject in an XPCNativeWrapper and return that, as needed.
     * @param pErr [out] relevant error code, if any.
     * @param src_is_identity optional performance hint. Set to true only
     *                        if src is the identity pointer.
     */
    static bool NativeInterface2JSObject(JS::MutableHandleValue d,
                                         nsIXPConnectJSObjectHolder** dest,
                                         xpcObjectHelper& aHelper,
                                         const nsID* iid,
                                         bool allowNativeWrapper,
                                         nsresult* pErr);

    static bool GetNativeInterfaceFromJSObject(void** dest, JSObject* src,
                                               const nsID* iid,
                                               nsresult* pErr);
    static bool JSObject2NativeInterface(void** dest, JS::HandleObject src,
                                         const nsID* iid,
                                         nsISupports* aOuter,
                                         nsresult* pErr);

    // Note - This return the XPCWrappedNative, rather than the native itself,
    // for the WN case. You probably want UnwrapReflectorToISupports.
    static bool GetISupportsFromJSObject(JSObject* obj, nsISupports** iface);

    /**
     * Convert a native array into a JS::Value.
     *
     * @param d [out] the resulting JS::Value
     * @param s the native array we're working with
     * @param type the type of objects in the array
     * @param iid the interface of each object in the array that we want
     * @param count the number of items in the array
     * @param scope the default scope to put on the new JSObjects' parent chain
     * @param pErr [out] relevant error code, if any.
     */
    static bool NativeArray2JS(JS::MutableHandleValue d, const void** s,
                               const nsXPTType& type, const nsID* iid,
                               uint32_t count, nsresult* pErr);

    static bool JSArray2Native(void** d, JS::HandleValue s,
                               uint32_t count, const nsXPTType& type,
                               const nsID* iid, nsresult* pErr);

    static bool JSTypedArray2Native(void** d,
                                    JSObject* jsarray,
                                    uint32_t count,
                                    const nsXPTType& type,
                                    nsresult* pErr);

    static bool NativeStringWithSize2JS(JS::MutableHandleValue d, const void* s,
                                        const nsXPTType& type,
                                        uint32_t count,
                                        nsresult* pErr);

    static bool JSStringWithSize2Native(void* d, JS::HandleValue s,
                                        uint32_t count, const nsXPTType& type,
                                        nsresult* pErr);

    static nsresult JSValToXPCException(JS::MutableHandleValue s,
                                        const char* ifaceName,
                                        const char* methodName,
                                        nsIException** exception);

    static nsresult ConstructException(nsresult rv, const char* message,
                                       const char* ifaceName,
                                       const char* methodName,
                                       nsISupports* data,
                                       nsIException** exception,
                                       JSContext* cx,
                                       JS::Value* jsExceptionPtr);

private:
    XPCConvert() = delete;

};

/***************************************************************************/
// code for throwing exceptions into JS

class nsXPCException;

class XPCThrower
{
public:
    static void Throw(nsresult rv, JSContext* cx);
    static void Throw(nsresult rv, XPCCallContext& ccx);
    static void ThrowBadResult(nsresult rv, nsresult result, XPCCallContext& ccx);
    static void ThrowBadParam(nsresult rv, unsigned paramNum, XPCCallContext& ccx);
    static bool SetVerbosity(bool state)
        {bool old = sVerbose; sVerbose = state; return old;}

    static bool CheckForPendingException(nsresult result, JSContext* cx);

private:
    static void Verbosify(XPCCallContext& ccx,
                          char** psz, bool own);

private:
    static bool sVerbose;
};

/***************************************************************************/

class nsXPCException
{
public:
    static bool NameAndFormatForNSResult(nsresult rv,
                                         const char** name,
                                         const char** format);

    static const void* IterateNSResults(nsresult* rv,
                                        const char** name,
                                        const char** format,
                                        const void** iterp);

    static uint32_t GetNSResultCount();
};

/***************************************************************************/
/*
* nsJSID implements nsIJSID. It is also used by nsJSIID and nsJSCID as a
* member (as a hidden implementaion detail) to which they delegate many calls.
*/

// Initialization is done on demand, and calling the destructor below is always
// safe.
extern void xpc_DestroyJSxIDClassObjects();

class nsJSID final : public nsIJSID
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_JS_ID_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSID

    bool InitWithName(const nsID& id, const char* nameString);
    bool SetName(const char* name);
    void   SetNameToNoString()
        {MOZ_ASSERT(!mName, "name already set"); mName = const_cast<char*>(gNoString);}
    bool NameIsSet() const {return nullptr != mName;}
    const nsID& ID() const {return mID;}
    bool IsValid() const {return !mID.Equals(GetInvalidIID());}

    static already_AddRefed<nsJSID> NewID(const char* str);
    static already_AddRefed<nsJSID> NewID(const nsID& id);

    nsJSID();

    void Reset();
    const nsID& GetInvalidIID() const;

protected:
    virtual ~nsJSID();
    static const char gNoString[];
    nsID    mID;
    char*   mNumber;
    char*   mName;
};


// nsJSIID

class nsJSIID : public nsIJSIID,
                public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS

    // we manually delegate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID
    NS_DECL_NSIXPCSCRIPTABLE

    static already_AddRefed<nsJSIID> NewID(nsIInterfaceInfo* aInfo);

    explicit nsJSIID(nsIInterfaceInfo* aInfo);
    nsJSIID() = delete;

private:
    virtual ~nsJSIID();

    nsCOMPtr<nsIInterfaceInfo> mInfo;
};

// nsJSCID

class nsJSCID : public nsIJSCID, public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS

    // we manually delegate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSCID
    NS_DECL_NSIXPCSCRIPTABLE

    static already_AddRefed<nsJSCID> NewID(const char* str);

    nsJSCID();

private:
    virtual ~nsJSCID();

    void ResolveName();

private:
    RefPtr<nsJSID> mDetails;
};


/***************************************************************************/
// 'Components' object implementations. nsXPCComponentsBase has the
// less-privileged stuff that we're willing to expose to XBL.

class nsXPCComponentsBase : public nsIXPCComponentsBase
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTSBASE

public:
    void SystemIsBeingShutDown() { ClearMembers(); }

    XPCWrappedNativeScope* GetScope() { return mScope; }

protected:
    virtual ~nsXPCComponentsBase();

    explicit nsXPCComponentsBase(XPCWrappedNativeScope* aScope);
    virtual void ClearMembers();

    XPCWrappedNativeScope*                   mScope;

    // Unprivileged members from nsIXPCComponentsBase.
    RefPtr<nsXPCComponents_Interfaces>     mInterfaces;
    RefPtr<nsXPCComponents_InterfacesByID> mInterfacesByID;
    RefPtr<nsXPCComponents_Results>        mResults;

    friend class XPCWrappedNativeScope;
};

class nsXPCComponents : public nsXPCComponentsBase,
                        public nsIXPCComponents
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_FORWARD_NSIXPCCOMPONENTSBASE(nsXPCComponentsBase::)
    NS_DECL_NSIXPCCOMPONENTS

protected:
    explicit nsXPCComponents(XPCWrappedNativeScope* aScope);
    virtual ~nsXPCComponents();
    virtual void ClearMembers() override;

    // Privileged members added by nsIXPCComponents.
    RefPtr<nsXPCComponents_Classes>     mClasses;
    RefPtr<nsXPCComponents_ClassesByID> mClassesByID;
    RefPtr<nsXPCComponents_ID>          mID;
    RefPtr<nsXPCComponents_Exception>   mException;
    RefPtr<nsXPCComponents_Constructor> mConstructor;
    RefPtr<nsXPCComponents_Utils>       mUtils;

    friend class XPCWrappedNativeScope;
};


/***************************************************************************/

extern JSObject*
xpc_NewIDObject(JSContext* cx, JS::HandleObject jsobj, const nsID& aID);

extern const nsID*
xpc_JSObjectToID(JSContext* cx, JSObject* obj);

extern bool
xpc_JSObjectIsID(JSContext* cx, JSObject* obj);

/***************************************************************************/
// in XPCDebug.cpp

extern bool
xpc_DumpJSStack(bool showArgs, bool showLocals, bool showThisProps);

// Return a newly-allocated string containing a representation of the
// current JS stack.
extern JS::UniqueChars
xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                 bool showThisProps);

/******************************************************************************
 * Handles pre/post script processing.
 */
class MOZ_RAII AutoScriptEvaluate
{
public:
    /**
     * Saves the JSContext as well as initializing our state
     * @param cx The JSContext, this can be null, we don't do anything then
     */
    explicit AutoScriptEvaluate(JSContext * cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
         : mJSContext(cx), mEvaluated(false) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    /**
     * Does the pre script evaluation.
     * This function should only be called once, and will assert if called
     * more than once
     */

    bool StartEvaluating(JS::HandleObject scope);

    /**
     * Does the post script evaluation.
     */
    ~AutoScriptEvaluate();
private:
    JSContext* mJSContext;
    mozilla::Maybe<JS::AutoSaveExceptionState> mState;
    bool mEvaluated;
    mozilla::Maybe<JSAutoCompartment> mAutoCompartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    // No copying or assignment allowed
    AutoScriptEvaluate(const AutoScriptEvaluate&) = delete;
    AutoScriptEvaluate & operator =(const AutoScriptEvaluate&) = delete;
};

/***************************************************************************/
class MOZ_RAII AutoResolveName
{
public:
    AutoResolveName(XPCCallContext& ccx, JS::HandleId name
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(ccx.GetContext())
        , mOld(ccx, mContext->SetResolveName(name))
#ifdef DEBUG
        , mCheck(ccx, name)
#endif
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoResolveName()
    {
        mozilla::DebugOnly<jsid> old =
            mContext->SetResolveName(mOld);
        MOZ_ASSERT(old == mCheck, "Bad Nesting!");
    }

private:
    XPCJSContext* mContext;
    JS::RootedId mOld;
#ifdef DEBUG
    JS::RootedId mCheck;
#endif
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
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
    explicit AutoMarkingPtr(JSContext* cx) {
        mRoot = XPCJSContext::Get()->GetAutoRootsAdr();
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
        for (AutoMarkingPtr* cur = this; cur; cur = cur->mNext)
            cur->TraceJS(trc);
    }

    void MarkAfterJSFinalizeAll() {
        for (AutoMarkingPtr* cur = this; cur; cur = cur->mNext)
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
    explicit TypedAutoMarkingPtr(JSContext* cx) : AutoMarkingPtr(cx), mPtr(nullptr) {}
    TypedAutoMarkingPtr(JSContext* cx, T* ptr) : AutoMarkingPtr(cx), mPtr(ptr) {}

    T* get() const { return mPtr; }
    operator T*() const { return mPtr; }
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

typedef TypedAutoMarkingPtr<XPCWrappedNative> AutoMarkingWrappedNativePtr;
typedef TypedAutoMarkingPtr<XPCWrappedNativeTearOff> AutoMarkingWrappedNativeTearOffPtr;
typedef TypedAutoMarkingPtr<XPCWrappedNativeProto> AutoMarkingWrappedNativeProtoPtr;

/***************************************************************************/
namespace xpc {
// Allocates a string that grants all access ("AllAccess")
char*
CloneAllAccess();

// Returns access if wideName is in list
char*
CheckAccessList(const char16_t* wideName, const char* const list[]);
} /* namespace xpc */

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

    static already_AddRefed<XPCVariant> newVariant(JSContext* cx, const JS::Value& aJSVal);

    /**
     * This getter clears the gray bit before handing out the Value if the Value
     * represents a JSObject. That means that the object is guaranteed to be
     * kept alive past the next CC.
     */
    JS::Value GetJSVal() const {
        return mJSVal;
    }

    /**
     * This getter does not change the color of the Value (if it represents a
     * JSObject) meaning that the value returned is not guaranteed to be kept
     * alive past the next CC.
     *
     * This should only be called if you are certain that the return value won't
     * be passed into a JS API function and that it won't be stored without
     * being rooted (or otherwise signaling the stored value to the CC).
     */
    JS::Value GetJSValPreserveColor() const { return mJSVal.unbarrieredGet(); }

    XPCVariant(JSContext* cx, const JS::Value& aJSVal);

    /**
     * Convert a variant into a JS::Value.
     *
     * @param ccx the context for the whole procedure
     * @param variant the variant to convert
     * @param scope the default scope to put on the new JSObject's parent chain
     * @param pErr [out] relevant error code, if any.
     * @param pJSVal [out] the resulting jsval.
     */
    static bool VariantDataToJS(nsIVariant* variant,
                                nsresult* pErr, JS::MutableHandleValue pJSVal);

    bool IsPurple()
    {
        return mRefCnt.IsPurple();
    }

    void RemovePurple()
    {
        mRefCnt.RemovePurple();
    }

    void SetCCGeneration(uint32_t aGen)
    {
        mCCGeneration = aGen;
    }

    uint32_t CCGeneration() { return mCCGeneration; }
protected:
    virtual ~XPCVariant() { }

    bool InitializeData(JSContext* cx);

protected:
    nsDiscriminatedUnion mData;
    JS::Heap<JS::Value>  mJSVal;
    bool                 mReturnRawObject : 1;
    uint32_t             mCCGeneration : 31;
};

NS_DEFINE_STATIC_IID_ACCESSOR(XPCVariant, XPCVARIANT_IID)

class XPCTraceableVariant: public XPCVariant,
                           public XPCRootSetElem
{
public:
    XPCTraceableVariant(JSContext* cx, const JS::Value& aJSVal)
        : XPCVariant(cx, aJSVal)
    {
        nsXPConnect::GetRuntimeInstance()->AddVariantRoot(this);
    }

    virtual ~XPCTraceableVariant();

    void TraceJS(JSTracer* trc);
};

/***************************************************************************/
// Utilities

inline void*
xpc_GetJSPrivate(JSObject* obj)
{
    return js::GetObjectPrivate(obj);
}

inline JSContext*
xpc_GetSafeJSContext()
{
    return XPCJSContext::Get()->Context();
}

namespace xpc {

JSAddonId*
NewAddonId(JSContext* cx, const nsACString& id);

// JSNatives to expose atob and btoa in various non-DOM XPConnect scopes.
bool
Atob(JSContext* cx, unsigned argc, JS::Value* vp);

bool
Btoa(JSContext* cx, unsigned argc, JS::Value* vp);

// Helper function that creates a JSFunction that wraps a native function that
// forwards the call to the original 'callable'.
class FunctionForwarderOptions;
bool
NewFunctionForwarder(JSContext* cx, JS::HandleId id, JS::HandleObject callable,
                     FunctionForwarderOptions& options, JS::MutableHandleValue vp);

// Old fashioned xpc error reporter. Try to use JS_ReportError instead.
nsresult
ThrowAndFail(nsresult errNum, JSContext* cx, bool* retval);

struct GlobalProperties {
    GlobalProperties() {
      mozilla::PodZero(this);

    }
    bool Parse(JSContext* cx, JS::HandleObject obj);
    bool DefineInXPCComponents(JSContext* cx, JS::HandleObject obj);
    bool DefineInSandbox(JSContext* cx, JS::HandleObject obj);
    bool CSS : 1;
    bool indexedDB : 1;
    bool XMLHttpRequest : 1;
    bool TextDecoder : 1;
    bool TextEncoder : 1;
    bool URL : 1;
    bool URLSearchParams : 1;
    bool atob : 1;
    bool btoa : 1;
    bool Blob : 1;
    bool Directory : 1;
    bool File : 1;
    bool crypto : 1;
    bool rtcIdentityProvider : 1;
    bool fetch : 1;
    bool caches : 1;
    bool fileReader: 1;
    bool messageChannel: 1;
private:
    bool Define(JSContext* cx, JS::HandleObject obj);
};

// Infallible.
already_AddRefed<nsIXPCComponents_utils_Sandbox>
NewSandboxConstructor();

// Returns true if class of 'obj' is SandboxClass.
bool
IsSandbox(JSObject* obj);

class MOZ_STACK_CLASS OptionsBase {
public:
    explicit OptionsBase(JSContext* cx = xpc_GetSafeJSContext(),
                         JSObject* options = nullptr)
        : mCx(cx)
        , mObject(cx, options)
    { }

    virtual bool Parse() = 0;

protected:
    bool ParseValue(const char* name, JS::MutableHandleValue prop, bool* found = nullptr);
    bool ParseBoolean(const char* name, bool* prop);
    bool ParseObject(const char* name, JS::MutableHandleObject prop);
    bool ParseJSString(const char* name, JS::MutableHandleString prop);
    bool ParseString(const char* name, nsCString& prop);
    bool ParseString(const char* name, nsString& prop);
    bool ParseId(const char* name, JS::MutableHandleId id);
    bool ParseUInt32(const char* name, uint32_t* prop);

    JSContext* mCx;
    JS::RootedObject mObject;
};

class MOZ_STACK_CLASS SandboxOptions : public OptionsBase {
public:
    explicit SandboxOptions(JSContext* cx = xpc_GetSafeJSContext(),
                            JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , wantXrays(true)
        , allowWaivers(true)
        , wantComponents(true)
        , wantExportHelpers(false)
        , isWebExtensionContentScript(false)
        , waiveInterposition(false)
        , proto(cx)
        , addonId(cx)
        , writeToGlobalPrototype(false)
        , sameZoneAs(cx)
        , freshZone(false)
        , invisibleToDebugger(false)
        , discardSource(false)
        , metadata(cx)
        , userContextId(0)
        , originAttributes(cx)
    { }

    virtual bool Parse();

    bool wantXrays;
    bool allowWaivers;
    bool wantComponents;
    bool wantExportHelpers;
    bool isWebExtensionContentScript;
    bool waiveInterposition;
    JS::RootedObject proto;
    nsCString sandboxName;
    JS::RootedString addonId;
    bool writeToGlobalPrototype;
    JS::RootedObject sameZoneAs;
    bool freshZone;
    bool invisibleToDebugger;
    bool discardSource;
    GlobalProperties globalProperties;
    JS::RootedValue metadata;
    uint32_t userContextId;
    JS::RootedObject originAttributes;

protected:
    bool ParseGlobalProperties();
};

class MOZ_STACK_CLASS CreateObjectInOptions : public OptionsBase {
public:
    explicit CreateObjectInOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                   JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , defineAs(cx, JSID_VOID)
    { }

    virtual bool Parse() { return ParseId("defineAs", &defineAs); }

    JS::RootedId defineAs;
};

class MOZ_STACK_CLASS ExportFunctionOptions : public OptionsBase {
public:
    explicit ExportFunctionOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                   JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , defineAs(cx, JSID_VOID)
        , allowCrossOriginArguments(false)
    { }

    virtual bool Parse() {
        return ParseId("defineAs", &defineAs) &&
               ParseBoolean("allowCrossOriginArguments", &allowCrossOriginArguments);
    }

    JS::RootedId defineAs;
    bool allowCrossOriginArguments;
};

class MOZ_STACK_CLASS FunctionForwarderOptions : public OptionsBase {
public:
    explicit FunctionForwarderOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                      JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , allowCrossOriginArguments(false)
    { }

    JSObject* ToJSObject(JSContext* cx) {
        JS::RootedObject obj(cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
        if (!obj)
            return nullptr;

        JS::RootedValue val(cx);
        unsigned attrs = JSPROP_READONLY | JSPROP_PERMANENT;
        val = JS::BooleanValue(allowCrossOriginArguments);
        if (!JS_DefineProperty(cx, obj, "allowCrossOriginArguments", val, attrs))
            return nullptr;

        return obj;
    }

    virtual bool Parse() {
        return ParseBoolean("allowCrossOriginArguments", &allowCrossOriginArguments);
    }

    bool allowCrossOriginArguments;
};

class MOZ_STACK_CLASS StackScopedCloneOptions : public OptionsBase {
public:
    explicit StackScopedCloneOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                     JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , wrapReflectors(false)
        , cloneFunctions(false)
        , deepFreeze(false)
    { }

    virtual bool Parse() {
        return ParseBoolean("wrapReflectors", &wrapReflectors) &&
               ParseBoolean("cloneFunctions", &cloneFunctions) &&
               ParseBoolean("deepFreeze", &deepFreeze);
    }

    // When a reflector is encountered, wrap it rather than aborting the clone.
    bool wrapReflectors;

    // When a function is encountered, clone it (exportFunction-style) rather than
    // aborting the clone.
    bool cloneFunctions;

    // If true, the resulting object is deep-frozen after being cloned.
    bool deepFreeze;
};

JSObject*
CreateGlobalObject(JSContext* cx, const JSClass* clasp, nsIPrincipal* principal,
                   JS::CompartmentOptions& aOptions);

// Modify the provided compartment options, consistent with |aPrincipal| and
// with globally-cached values of various preferences.
//
// Call this function *before* |aOptions| is used to create the corresponding
// global object, as not all of the options it sets can be modified on an
// existing global object.  (The type system should make this obvious, because
// you can't get a *mutable* JS::CompartmentOptions& from an existing global
// object.)
void
InitGlobalObjectOptions(JS::CompartmentOptions& aOptions,
                        nsIPrincipal* aPrincipal);

// Finish initializing an already-created, not-yet-exposed-to-script global
// object.  This will attach a Components object (if necessary) and call
// |JS_FireOnNewGlobalObject| (if necessary).
//
// If you must modify compartment options, see InitGlobalObjectOptions above.
bool
InitGlobalObject(JSContext* aJSContext, JS::Handle<JSObject*> aGlobal,
                 uint32_t aFlags);

// Helper for creating a sandbox object to use for evaluating
// untrusted code completely separated from all other code in the
// system using EvalInSandbox(). Takes the JSContext on which to
// do setup etc on, puts the sandbox object in *vp (which must be
// rooted by the caller), and uses the principal that's either
// directly passed in prinOrSop or indirectly as an
// nsIScriptObjectPrincipal holding the principal. If no principal is
// reachable through prinOrSop, a new null principal will be created
// and used.
nsresult
CreateSandboxObject(JSContext* cx, JS::MutableHandleValue vp, nsISupports* prinOrSop,
                    xpc::SandboxOptions& options);
// Helper for evaluating scripts in a sandbox object created with
// CreateSandboxObject(). The caller is responsible of ensuring
// that *rval doesn't get collected during the call or usage after the
// call. This helper will use filename and lineNo for error reporting,
// and if no filename is provided it will use the codebase from the
// principal and line number 1 as a fallback.
nsresult
EvalInSandbox(JSContext* cx, JS::HandleObject sandbox, const nsAString& source,
              const nsACString& filename, int32_t lineNo,
              JSVersion jsVersion, JS::MutableHandleValue rval);

nsresult
GetSandboxAddonId(JSContext* cx, JS::HandleObject sandboxArg,
                  JS::MutableHandleValue rval);

// Helper for retrieving metadata stored in a reserved slot. The metadata
// is set during the sandbox creation using the "metadata" option.
nsresult
GetSandboxMetadata(JSContext* cx, JS::HandleObject sandboxArg,
                   JS::MutableHandleValue rval);

nsresult
SetSandboxMetadata(JSContext* cx, JS::HandleObject sandboxArg,
                   JS::HandleValue metadata);

bool
CreateObjectIn(JSContext* cx, JS::HandleValue vobj, CreateObjectInOptions& options,
               JS::MutableHandleValue rval);

bool
EvalInWindow(JSContext* cx, const nsAString& source, JS::HandleObject scope,
             JS::MutableHandleValue rval);

bool
ExportFunction(JSContext* cx, JS::HandleValue vscope, JS::HandleValue vfunction,
               JS::HandleValue voptions, JS::MutableHandleValue rval);

bool
CloneInto(JSContext* cx, JS::HandleValue vobj, JS::HandleValue vscope,
          JS::HandleValue voptions, JS::MutableHandleValue rval);

bool
StackScopedClone(JSContext* cx, StackScopedCloneOptions& options, JS::MutableHandleValue val);

} /* namespace xpc */


/***************************************************************************/
// Inlined utilities.

inline bool
xpc_ForcePropertyResolve(JSContext* cx, JS::HandleObject obj, jsid id);

inline jsid
GetJSIDByIndex(JSContext* cx, unsigned index);

namespace xpc {

enum WrapperDenialType {
    WrapperDenialForXray = 0,
    WrapperDenialForCOW,
    WrapperDenialTypeCount
};
bool ReportWrapperDenial(JSContext* cx, JS::HandleId id, WrapperDenialType type, const char* reason);

class CompartmentPrivate
{
public:
    enum LocationHint {
        LocationHintRegular,
        LocationHintAddon
    };

    explicit CompartmentPrivate(JSCompartment* c);

    ~CompartmentPrivate();

    static CompartmentPrivate* Get(JSCompartment* compartment)
    {
        MOZ_ASSERT(compartment);
        void* priv = JS_GetCompartmentPrivate(compartment);
        return static_cast<CompartmentPrivate*>(priv);
    }

    static CompartmentPrivate* Get(JSObject* object)
    {
        JSCompartment* compartment = js::GetObjectCompartment(object);
        return Get(compartment);
    }

    // Controls whether this compartment gets Xrays to same-origin. This behavior
    // is deprecated, but is still the default for sandboxes for compatibity
    // reasons.
    bool wantXrays;

    // Controls whether this compartment is allowed to waive Xrays to content
    // that it subsumes. This should generally be true, except in cases where we
    // want to prevent code from depending on Xray Waivers (which might make it
    // more portable to other browser architectures).
    bool allowWaivers;

    // This flag is intended for a very specific use, internal to Gecko. It may
    // go away or change behavior at any time. It should not be added to any
    // documentation and it should not be used without consulting the XPConnect
    // module owner.
    bool writeToGlobalPrototype;

    // When writeToGlobalPrototype is true, we use this flag to temporarily
    // disable the writeToGlobalPrototype behavior (when resolving standard
    // classes, for example).
    bool skipWriteToGlobalPrototype;

    // This scope corresponds to a WebExtension content script, and receives
    // various bits of special compatibility behavior.
    bool isWebExtensionContentScript;

    // Even if an add-on needs interposition, it does not necessary need it
    // for every scope. If this flag is set we waive interposition for this
    // scope.
    bool waiveInterposition;

    // If CPOWs are disabled for browser code via the
    // dom.ipc.cpows.forbid-unsafe-from-browser preferences, then only
    // add-ons can use CPOWs. This flag allows a non-addon scope
    // to opt into CPOWs. It's necessary for the implementation of
    // RemoteAddonsParent.jsm.
    bool allowCPOWs;

    // This is only ever set during mochitest runs when enablePrivilege is called.
    // It's intended as a temporary stopgap measure until we can finish ripping out
    // enablePrivilege. Once set, this value is never unset (i.e., it doesn't follow
    // the old scoping rules of enablePrivilege).
    //
    // Using it in production is inherently unsafe.
    bool universalXPConnectEnabled;

    // This is only ever set during mochitest runs when enablePrivilege is called.
    // It allows the SpecialPowers scope to waive the normal chrome security
    // wrappers and expose properties directly to content. This lets us avoid a
    // bunch of overhead and complexity in our SpecialPowers automation glue.
    //
    // Using it in production is inherently unsafe.
    bool forcePermissiveCOWs;

    // True if this compartment has been nuked. If true, any wrappers into or
    // out of it should be considered invalid.
    bool wasNuked;

    // Whether we've emitted a warning about a property that was filtered out
    // by a security wrapper. See XrayWrapper.cpp.
    bool wrapperDenialWarnings[WrapperDenialTypeCount];

    // The scriptability of this compartment.
    Scriptability scriptability;

    // Our XPCWrappedNativeScope. This is non-null if and only if this is an
    // XPConnect compartment.
    XPCWrappedNativeScope* scope;

    const nsACString& GetLocation() {
        if (location.IsEmpty() && locationURI) {

            nsCOMPtr<nsIXPConnectWrappedJS> jsLocationURI =
                 do_QueryInterface(locationURI);
            if (jsLocationURI) {
                // We cannot call into JS-implemented nsIURI objects, because
                // we are iterating over the JS heap at this point.
                location =
                    NS_LITERAL_CSTRING("<JS-implemented nsIURI location>");
            } else if (NS_FAILED(locationURI->GetSpec(location))) {
                location = NS_LITERAL_CSTRING("<unknown location>");
            }
        }
        return location;
    }
    bool GetLocationURI(nsIURI** aURI) {
        return GetLocationURI(LocationHintRegular, aURI);
    }
    bool GetLocationURI(LocationHint aLocationHint, nsIURI** aURI) {
        if (locationURI) {
            nsCOMPtr<nsIURI> rval = locationURI;
            rval.forget(aURI);
            return true;
        }
        return TryParseLocationURI(aLocationHint, aURI);
    }
    void SetLocation(const nsACString& aLocation) {
        if (aLocation.IsEmpty())
            return;
        if (!location.IsEmpty() || locationURI)
            return;
        location = aLocation;
    }
    void SetLocationURI(nsIURI* aLocationURI) {
        if (!aLocationURI)
            return;
        if (locationURI)
            return;
        locationURI = aLocationURI;
    }

    JSObject2WrappedJSMap* GetWrappedJSMap() const { return mWrappedJSMap; }
    void UpdateWeakPointersAfterGC();

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

private:
    nsCString location;
    nsCOMPtr<nsIURI> locationURI;
    JSObject2WrappedJSMap* mWrappedJSMap;

    bool TryParseLocationURI(LocationHint aType, nsIURI** aURI);
};

bool IsUniversalXPConnectEnabled(JSCompartment* compartment);
bool IsUniversalXPConnectEnabled(JSContext* cx);
bool EnableUniversalXPConnect(JSContext* cx);

inline void
CrashIfNotInAutomation()
{
    MOZ_RELEASE_ASSERT(IsInAutomation());
}

inline XPCWrappedNativeScope*
ObjectScope(JSObject* obj)
{
    return CompartmentPrivate::Get(obj)->scope;
}

JSObject* NewOutObject(JSContext* cx);
bool IsOutObject(JSContext* cx, JSObject* obj);

nsresult HasInstance(JSContext* cx, JS::HandleObject objArg, const nsID* iid, bool* bp);

nsIPrincipal* GetObjectPrincipal(JSObject* obj);

} // namespace xpc

namespace mozilla {
namespace dom {
extern bool
DefineStaticJSVals(JSContext* cx);
} // namespace dom
} // namespace mozilla

bool
xpc_LocalizeContext(JSContext* cx);
void
xpc_DelocalizeContext(JSContext* cx);

/***************************************************************************/
// Inlines use the above - include last.

#include "XPCInlines.h"

/***************************************************************************/
// Maps have inlines that use the above - include last.

#include "XPCMaps.h"

/***************************************************************************/

#endif /* xpcprivate_h___ */
