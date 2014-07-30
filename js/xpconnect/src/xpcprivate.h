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

#include "mozilla/Alignment.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xpcpublic.h"
#include "js/TracingAPI.h"
#include "js/WeakMapPtr.h"
#include "pldhash.h"
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
#include "nsIJSRuntimeService.h"
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
#include "prclist.h"
#include "prcvar.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsAutoJSValHolder.h"

#include "MainThreadUtils.h"

#include "nsIConsoleService.h"
#include "nsIScriptError.h"
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
#include "nsNetUtil.h"

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptObjectPrincipal.h"
#include "xpcObjectHelper.h"
#include "nsIThreadInternal.h"

#include "SandboxPrivate.h"
#include "BackstagePass.h"
#include "nsCxPusher.h"
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

#include "nsINode.h"

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


#define WRAPPER_SLOTS (JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS )

#define INVALID_OBJECT ((JSObject *)1)

// If IS_WN_CLASS for the JSClass of an object is true, the object is a
// wrappednative wrapper, holding the XPCWrappedNative in its private slot.
static inline bool IS_WN_CLASS(const js::Class* clazz)
{
    return clazz->ext.isWrappedNative;
}

static inline bool IS_WN_REFLECTOR(JSObject *obj)
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

inline bool
AddToCCKind(JSGCTraceKind kind)
{
    return kind == JSTRACE_OBJECT || kind == JSTRACE_SCRIPT;
}

class nsXPConnect : public nsIXPConnect,
                    public nsIThreadObserver,
                    public nsSupportsWeakReference,
                    public nsIJSRuntimeService
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCONNECT
    NS_DECL_NSITHREADOBSERVER
    NS_DECL_NSIJSRUNTIMESERVICE

    // non-interface implementation
public:
    // These get non-addref'd pointers
    static nsXPConnect*  XPConnect()
    {
        // Do a release-mode assert that we're not doing anything significant in
        // XPConnect off the main thread. If you're an extension developer hitting
        // this, you need to change your code. See bug 716167.
        if (!MOZ_LIKELY(NS_IsMainThread()))
            MOZ_CRASH();

        return gSelf;
    }

    static XPCJSRuntime* GetRuntimeInstance();
    XPCJSRuntime* GetRuntime() {return mRuntime;}

    static bool IsISupportsDescendant(nsIInterfaceInfo* info);

    static nsIScriptSecurityManager* SecurityManager()
    {
        MOZ_ASSERT(NS_IsMainThread());
        return gScriptSecurityManager;
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
                                       bool allowShortCircuit) const;

    void RecordTraversal(void *p, nsISupports *s);
    virtual char* DebugPrintJSStack(bool showArgs,
                                    bool showLocals,
                                    bool showThisProps);


    static bool ReportAllJSExceptions()
    {
      return gReportAllJSExceptions > 0;
    }

protected:
    virtual ~nsXPConnect();

    nsXPConnect();

private:
    // Singleton instance
    static nsXPConnect*             gSelf;
    static bool                     gOnceAliveNowDead;

    XPCJSRuntime*                   mRuntime;
    bool                            mShuttingDown;

    // nsIThreadInternal doesn't remember which observers it called
    // OnProcessNextEvent on when it gets around to calling AfterProcessNextEvent.
    // So if XPConnect gets initialized mid-event (which can happen), we'll get
    // an 'after' notification without getting an 'on' notification. If we don't
    // watch out for this, we'll do an unmatched |pop| on the context stack.
    uint16_t                 mEventDepth;

    static uint32_t gReportAllJSExceptions;

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
        MOZ_ASSERT(!mNext, "Must be unlinked");
        MOZ_ASSERT(!mSelfp, "Must be unlinked");
    }

    inline XPCRootSetElem* GetNextRoot() { return mNext; }
    void AddToRootSet(XPCRootSetElem **listHead);
    void RemoveFromRootSet();

private:
    XPCRootSetElem *mNext;
    XPCRootSetElem **mSelfp;
};

/***************************************************************************/

// In the current xpconnect system there can only be one XPCJSRuntime.
// So, xpconnect can only be used on one JSRuntime within the process.

class XPCJSContextStack;
class WatchdogManager;

enum WatchdogTimestampCategory
{
    TimestampRuntimeStateChange = 0,
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
            if (mStrings[i].empty()) {
                mStrings[i].construct();
                return mStrings[i].addr();
            }
        }

        // All our internal string wrappers are used, allocate a new string.
        return new StringType();
    }

    void Destroy(StringType *string)
    {
        for (uint32_t i = 0; i < ArrayLength(mStrings); ++i) {
            if (!mStrings[i].empty() &&
                mStrings[i].addr() == string) {
                // One of our internal strings is no longer in use, mark
                // it as such and free its data.
                mStrings[i].destroy();
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
            MOZ_ASSERT(mStrings[i].empty(), "Short lived string still in use");
        }
#endif
    }

private:
    mozilla::Maybe<StringType> mStrings[2];
};

class XPCJSRuntime : public mozilla::CycleCollectedJSRuntime
{
public:
    static XPCJSRuntime* newXPCJSRuntime(nsXPConnect* aXPConnect);
    static XPCJSRuntime* Get() { return nsXPConnect::XPConnect()->GetRuntime(); }

    XPCJSContextStack* GetJSContextStack() {return mJSContextStack;}
    void DestroyJSContextStack();

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

    bool OnJSContextNew(JSContext* cx);

    virtual bool
    DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                          char (&aName)[72]) const MOZ_OVERRIDE;
    virtual bool
    NoteCustomGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                                   nsCycleCollectionTraversalCallback& aCb) const MOZ_OVERRIDE;

    /**
     * Infrastructure for classes that need to defer part of the finalization
     * until after the GC has run, for example for objects that we don't want to
     * destroy during the GC.
     */

public:
    bool GetDoingFinalization() const {return mDoingFinalization;}

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
        IDX_TOTAL_COUNT // just a count of the above
    };

    JS::HandleId GetStringID(unsigned index) const
    {
        MOZ_ASSERT(index < IDX_TOTAL_COUNT, "index out of range");
        // fromMarkedLocation() is safe because the string is interned.
        return JS::HandleId::fromMarkedLocation(&mStrIDs[index]);
    }
    JS::HandleValue GetStringJSVal(unsigned index) const
    {
        MOZ_ASSERT(index < IDX_TOTAL_COUNT, "index out of range");
        // fromMarkedLocation() is safe because the string is interned.
        return JS::HandleValue::fromMarkedLocation(&mStrJSVals[index]);
    }
    const char* GetStringName(unsigned index) const
    {
        MOZ_ASSERT(index < IDX_TOTAL_COUNT, "index out of range");
        return mStrings[index];
    }

    void TraceNativeBlackRoots(JSTracer* trc) MOZ_OVERRIDE;
    void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) MOZ_OVERRIDE;
    void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& cb) MOZ_OVERRIDE;
    void UnmarkSkippableJSHolders();
    void PrepareForForgetSkippable() MOZ_OVERRIDE;
    void BeginCycleCollectionCallback() MOZ_OVERRIDE;
    void EndCycleCollectionCallback(mozilla::CycleCollectorResults &aResults) MOZ_OVERRIDE;
    void DispatchDeferredDeletion(bool continuation) MOZ_OVERRIDE;

    void CustomGCCallback(JSGCStatus status) MOZ_OVERRIDE;
    void CustomOutOfMemoryCallback() MOZ_OVERRIDE;
    void CustomLargeAllocationFailureCallback() MOZ_OVERRIDE;
    bool CustomContextCallback(JSContext *cx, unsigned operation) MOZ_OVERRIDE;
    static void GCSliceCallback(JSRuntime *rt,
                                JS::GCProgress progress,
                                const JS::GCDescription &desc);
    static void FinalizeCallback(JSFreeOp *fop,
                                 JSFinalizeStatus status,
                                 bool isCompartmentGC,
                                 void *data);

    inline void AddVariantRoot(XPCTraceableVariant* variant);
    inline void AddWrappedJSRoot(nsXPCWrappedJS* wrappedJS);
    inline void AddObjectHolderRoot(XPCJSObjectHolder* holder);

    static void SuspectWrappedNative(XPCWrappedNative *wrapper,
                                     nsCycleCollectionNoteRootCallback &cb);

    void DebugDump(int16_t depth);

    void SystemIsBeingShutDown();

    bool GCIsRunning() const {return mGCIsRunning;}

    ~XPCJSRuntime();

    ShortLivedStringBuffer<nsString> mScratchStrings;
    ShortLivedStringBuffer<nsCString> mScratchCStrings;

    void AddGCCallback(xpcGCCallback cb);
    void RemoveGCCallback(xpcGCCallback cb);
    void AddContextCallback(xpcContextCallback cb);
    void RemoveContextCallback(xpcContextCallback cb);

    static JSContext* DefaultJSContextCallback(JSRuntime *rt);
    static void ActivityCallback(void *arg, bool active);
    static void CTypesActivityCallback(JSContext *cx,
                                       js::CTypesActivityType type);
    static bool InterruptCallback(JSContext *cx);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    AutoMarkingPtr**  GetAutoRootsAdr() {return &mAutoRoots;}

    JSObject* GetJunkScope();
    JSObject* GetCompilationScope();
    void DeleteSingletonScopes();

    PRTime GetWatchdogTimestamp(WatchdogTimestampCategory aCategory);
    void OnAfterProcessNextEvent() { mSlowScriptCheckpoint = mozilla::TimeStamp(); }

private:
    XPCJSRuntime(); // no implementation
    XPCJSRuntime(nsXPConnect* aXPConnect);

    void ReleaseIncrementally(nsTArray<nsISupports *> &array);

    static const char* const mStrings[IDX_TOTAL_COUNT];
    jsid mStrIDs[IDX_TOTAL_COUNT];
    jsval mStrJSVals[IDX_TOTAL_COUNT];

    XPCJSContextStack*       mJSContextStack;
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
    bool mGCIsRunning;
    nsTArray<nsXPCWrappedJS*> mWrappedJSToReleaseArray;
    nsTArray<nsISupports*> mNativesToReleaseArray;
    bool mDoingFinalization;
    XPCRootSetElem *mVariantRoots;
    XPCRootSetElem *mWrappedJSRoots;
    XPCRootSetElem *mObjectHolderRoots;
    nsTArray<xpcGCCallback> extraGCCallbacks;
    nsTArray<xpcContextCallback> extraContextCallbacks;
    nsRefPtr<WatchdogManager> mWatchdogManager;
    JS::GCSliceCallback mPrevGCSliceCallback;
    JS::PersistentRootedObject mJunkScope;
    JS::PersistentRootedObject mCompilationScope;
    nsRefPtr<AsyncFreeSnowWhite> mAsyncSnowWhiteFreer;

    mozilla::TimeStamp mSlowScriptCheckpoint;

    friend class Watchdog;
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
            MOZ_ASSERT(JS_GetSecondContextPrivate(aJSContext), "should already have XPCContext");
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
    bool CallerTypeIsJavaScript() const
        {
            return LANG_JS == mCallingLangType;
        }
    bool CallerTypeIsNative() const
        {
            return LANG_NATIVE == mCallingLangType;
        }
    bool CallerTypeIsKnown() const
        {
            return LANG_UNKNOWN != mCallingLangType;
        }

    nsresult GetException(nsIException** e)
        {
            nsCOMPtr<nsIException> rval = mException;
            rval.forget(e);
            return NS_OK;
        }
    void SetException(nsIException* e)
        {
            mException = e;
        }

    nsresult GetLastResult() {return mLastResult;}
    void SetLastResult(nsresult rc) {mLastResult = rc;}

    nsresult GetPendingResult() {return mPendingResult;}
    void SetPendingResult(nsresult rc) {mPendingResult = rc;}

    void DebugDump(int16_t depth);

    void MarkErrorUnreported() { mErrorUnreported = true; }
    void ClearUnreportedError() { mErrorUnreported = false; }
    bool WasErrorReported() { return !mErrorUnreported; }

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
    nsCOMPtr<nsIException> mException;
    LangType mCallingLangType;
    bool mErrorUnreported;
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

class MOZ_STACK_CLASS XPCCallContext : public nsAXPCNativeCallContext
{
public:
    NS_IMETHOD GetCallee(nsISupports **aResult);
    NS_IMETHOD GetCalleeMethodIndex(uint16_t *aResult);
    NS_IMETHOD GetJSContext(JSContext **aResult);
    NS_IMETHOD GetArgc(uint32_t *aResult);
    NS_IMETHOD GetArgvPtr(jsval **aResult);
    NS_IMETHOD GetCalleeInterface(nsIInterfaceInfo **aResult);
    NS_IMETHOD GetCalleeClassInfo(nsIClassInfo **aResult);
    NS_IMETHOD GetPreviousCallContext(nsAXPCNativeCallContext **aResult);
    NS_IMETHOD GetLanguage(uint16_t *aResult);

    enum {NO_ARGS = (unsigned) -1};

    static JSContext* GetDefaultJSContext();

    XPCCallContext(XPCContext::LangType callerLanguage,
                   JSContext* cx,
                   JS::HandleObject obj    = JS::NullPtr(),
                   JS::HandleObject funobj = JS::NullPtr(),
                   JS::HandleId id         = JSID_VOIDHANDLE,
                   unsigned argc           = NO_ARGS,
                   jsval *argv             = nullptr,
                   jsval *rval             = nullptr);

    virtual ~XPCCallContext();

    inline bool                         IsValid() const ;

    inline XPCJSRuntime*                GetRuntime() const ;
    inline XPCContext*                  GetXPCContext() const ;
    inline JSContext*                   GetJSContext() const ;
    inline bool                         GetContextPopRequired() const ;
    inline XPCContext::LangType         GetCallerLanguage() const ;
    inline XPCContext::LangType         GetPrevCallerLanguage() const ;
    inline XPCCallContext*              GetPrevCallContext() const ;

    inline JSObject*                    GetFlattenedJSObject() const ;
    inline nsISupports*                 GetIdentityObject() const ;
    inline XPCWrappedNative*            GetWrapper() const ;
    inline XPCWrappedNativeProto*       GetProto() const ;

    inline bool                         CanGetTearOff() const ;
    inline XPCWrappedNativeTearOff*     GetTearOff() const ;

    inline XPCNativeScriptableInfo*     GetScriptableInfo() const ;
    inline bool                         CanGetSet() const ;
    inline XPCNativeSet*                GetSet() const ;
    inline bool                         CanGetInterface() const ;
    inline XPCNativeInterface*          GetInterface() const ;
    inline XPCNativeMember*             GetMember() const ;
    inline bool                         HasInterfaceAndMember() const ;
    inline jsid                         GetName() const ;
    inline bool                         GetStaticMemberIsLocal() const ;
    inline unsigned                     GetArgc() const ;
    inline jsval*                       GetArgv() const ;
    inline jsval*                       GetRetVal() const ;

    inline uint16_t                     GetMethodIndex() const ;
    inline void                         SetMethodIndex(uint16_t index) ;

    inline jsid GetResolveName() const;
    inline jsid SetResolveName(JS::HandleId name);

    inline XPCWrappedNative* GetResolvingWrapper() const;
    inline XPCWrappedNative* SetResolvingWrapper(XPCWrappedNative* w);

    inline void SetRetVal(jsval val);

    void SetName(jsid name);
    void SetArgsAndResultPtr(unsigned argc, jsval *argv, jsval *rval);
    void SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                     bool isSetter);

    nsresult  CanCallNow();

    void SystemIsBeingShutDown();

    operator JSContext*() const {return GetJSContext();}

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
inline void CHECK_STATE(int s) const {MOZ_ASSERT(mState >= s, "bad state");}
#else
#define CHECK_STATE(s) ((void)0)
#endif

private:
    JSAutoRequest                   mAr;
    State                           mState;

    nsRefPtr<nsXPConnect>           mXPC;

    XPCContext*                     mXPCContext;
    JSContext*                      mJSContext;

    XPCContext::LangType            mCallerLanguage;

    // ctor does not necessarily init the following. BEWARE!

    XPCContext::LangType            mPrevCallerLanguage;

    XPCCallContext*                 mPrevCallContext;

    XPCWrappedNative*               mWrapper;
    XPCWrappedNativeTearOff*        mTearOff;

    XPCNativeScriptableInfo*        mScriptableInfo;

    XPCNativeSet*                   mSet;
    XPCNativeInterface*             mInterface;
    XPCNativeMember*                mMember;

    JS::RootedId                    mName;
    bool                            mStaticMemberIsLocal;

    unsigned                        mArgc;
    jsval*                          mArgv;
    jsval*                          mRetVal;

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

struct XPCWrappedNativeJSClass;
extern const XPCWrappedNativeJSClass XPC_WN_NoHelper_JSClass;
extern const js::Class XPC_WN_NoMods_WithCall_Proto_JSClass;
extern const js::Class XPC_WN_NoMods_NoCall_Proto_JSClass;
extern const js::Class XPC_WN_ModsAllowed_WithCall_Proto_JSClass;
extern const js::Class XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
extern const js::Class XPC_WN_Tearoff_JSClass;
extern const js::Class XPC_WN_NoHelper_Proto_JSClass;

extern bool
XPC_WN_CallMethod(JSContext *cx, unsigned argc, jsval *vp);

extern bool
XPC_WN_GetterSetter(JSContext *cx, unsigned argc, jsval *vp);

extern bool
XPC_WN_JSOp_Enumerate(JSContext *cx, JS::HandleObject obj, JSIterateOp enum_op,
                      JS::MutableHandleValue statep, JS::MutableHandleId idp);

extern JSObject*
XPC_WN_JSOp_ThisObject(JSContext *cx, JS::HandleObject obj);

// Macros to initialize Object or Function like XPC_WN classes
#define XPC_WN_WithCall_ObjectOps                                             \
    {                                                                         \
        nullptr, /* lookupGeneric */                                          \
        nullptr, /* lookupProperty */                                         \
        nullptr, /* lookupElement */                                          \
        nullptr, /* defineGeneric */                                          \
        nullptr, /* defineProperty */                                         \
        nullptr, /* defineElement */                                          \
        nullptr, /* getGeneric    */                                          \
        nullptr, /* getProperty    */                                         \
        nullptr, /* getElement    */                                          \
        nullptr, /* setGeneric    */                                          \
        nullptr, /* setProperty    */                                         \
        nullptr, /* setElement    */                                          \
        nullptr, /* getGenericAttributes  */                                  \
        nullptr, /* setGenericAttributes  */                                  \
        nullptr, /* deleteGeneric */                                          \
        nullptr, nullptr, /* watch/unwatch */                                 \
        nullptr, /* slice */                                                  \
        XPC_WN_JSOp_Enumerate,                                                \
        XPC_WN_JSOp_ThisObject,                                               \
    }

#define XPC_WN_NoCall_ObjectOps                                               \
    {                                                                         \
        nullptr, /* lookupGeneric */                                          \
        nullptr, /* lookupProperty */                                         \
        nullptr, /* lookupElement */                                          \
        nullptr, /* defineGeneric */                                          \
        nullptr, /* defineProperty */                                         \
        nullptr, /* defineElement */                                          \
        nullptr, /* getGeneric    */                                          \
        nullptr, /* getProperty    */                                         \
        nullptr, /* getElement    */                                          \
        nullptr, /* setGeneric    */                                          \
        nullptr, /* setProperty    */                                         \
        nullptr, /* setElement    */                                          \
        nullptr, /* getGenericAttributes  */                                  \
        nullptr, /* setGenericAttributes  */                                  \
        nullptr, /* deleteGeneric */                                          \
        nullptr, nullptr, /* watch/unwatch */                                 \
        nullptr, /* slice */                                                  \
        XPC_WN_JSOp_Enumerate,                                                \
        XPC_WN_JSOp_ThisObject,                                               \
    }

// Maybe this macro should check for class->enumerate ==
// XPC_WN_Shared_Proto_Enumerate or something rather than checking for
// 4 classes?
static inline bool IS_PROTO_CLASS(const js::Class *clazz)
{
    return clazz == &XPC_WN_NoMods_WithCall_Proto_JSClass ||
           clazz == &XPC_WN_NoMods_NoCall_Proto_JSClass ||
           clazz == &XPC_WN_ModsAllowed_WithCall_Proto_JSClass ||
           clazz == &XPC_WN_ModsAllowed_NoCall_Proto_JSClass;
}

/***************************************************************************/
// XPCWrappedNativeScope is one-to-one with a JS global object.

class nsIAddonInterposition;
class nsXPCComponentsBase;
class XPCWrappedNativeScope : public PRCList
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

    bool AttachComponentsObject(JSContext *aCx);

    // Returns the JS object reflection of the Components object.
    bool
    GetComponentsJSObject(JS::MutableHandleObject obj);

    JSObject*
    GetGlobalJSObject() const {
        JS::ExposeObjectToActiveJS(mGlobalJSObject);
        return mGlobalJSObject;
    }

    JSObject*
    GetGlobalJSObjectPreserveColor() const {return mGlobalJSObject;}

    nsIPrincipal*
    GetPrincipal() const {
        JSCompartment *c = js::GetObjectCompartment(mGlobalJSObject);
        return nsJSPrincipals::get(JS_GetCompartmentPrincipals(c));
    }

    JSObject*
    GetExpandoChain(JS::HandleObject target);

    bool
    SetExpandoChain(JSContext *cx, JS::HandleObject target, JS::HandleObject chain);

    static void
    SystemIsBeingShutDown();

    static void
    TraceWrappedNativesInAllScopes(JSTracer* trc, XPCJSRuntime* rt);

    void TraceInside(JSTracer *trc) {
        MOZ_ASSERT(mGlobalJSObject);
        mGlobalJSObject.trace(trc, "XPCWrappedNativeScope::mGlobalJSObject");
        if (mContentXBLScope)
            mContentXBLScope.trace(trc, "XPCWrappedNativeScope::mXBLScope");
        for (size_t i = 0; i < mAddonScopes.Length(); i++)
            mAddonScopes[i].trace(trc, "XPCWrappedNativeScope::mAddonScopes");
        if (mXrayExpandos.initialized())
            mXrayExpandos.trace(trc);
    }

    static void
    SuspectAllWrappers(XPCJSRuntime* rt, nsCycleCollectionNoteRootCallback &cb);

    static void
    StartFinalizationPhaseOfGC(JSFreeOp *fop, XPCJSRuntime* rt);

    static void
    FinishedFinalizationPhaseOfGC();

    static void
    MarkAllWrappedNativesAndProtos();

#ifdef DEBUG
    static void
    ASSERT_NoInterfaceSetsAreMarked();
#endif

    static void
    SweepAllWrappedNativeTearOffs();

    static void
    DebugDumpAllScopes(int16_t depth);

    void
    DebugDump(int16_t depth);

    struct ScopeSizeInfo {
        ScopeSizeInfo(mozilla::MallocSizeOf mallocSizeOf)
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

    bool
    IsValid() const {return mRuntime != nullptr;}

    static bool
    IsDyingScope(XPCWrappedNativeScope *scope);

    typedef js::HashSet<JSObject *,
                        js::PointerHasher<JSObject *, 3>,
                        js::SystemAllocPolicy> DOMExpandoSet;

    bool RegisterDOMExpandoObject(JSObject *expando) {
        // Expandos are proxy objects, and proxies are always tenured.
        JS::AssertGCThingMustBeTenured(expando);
        if (!mDOMExpandoSet) {
            mDOMExpandoSet = new DOMExpandoSet();
            mDOMExpandoSet->init(8);
        }
        return mDOMExpandoSet->put(expando);
    }
    void RemoveDOMExpandoObject(JSObject *expando) {
        if (mDOMExpandoSet)
            mDOMExpandoSet->remove(expando);
    }

    typedef js::HashMap<JSAddonId *,
                        nsCOMPtr<nsIAddonInterposition>,
                        js::PointerHasher<JSAddonId *, 3>,
                        js::SystemAllocPolicy> InterpositionMap;

    static bool SetAddonInterposition(JSAddonId *addonId,
                                      nsIAddonInterposition *interp);

    // Gets the appropriate scope object for XBL in this scope. The context
    // must be same-compartment with the global upon entering, and the scope
    // object is wrapped into the compartment of the global.
    JSObject *EnsureContentXBLScope(JSContext *cx);

    JSObject *EnsureAddonScope(JSContext *cx, JSAddonId *addonId);

    XPCWrappedNativeScope(JSContext *cx, JS::HandleObject aGlobal);

    nsAutoPtr<JSObject2JSObjectMap> mWaiverWrapperMap;

    bool IsContentXBLScope() { return mIsContentXBLScope; }
    bool AllowContentXBLScope();
    bool UseContentXBLScope() { return mUseContentXBLScope; }

    bool IsAddonScope() { return mIsAddonScope; }

    bool HasInterposition() { return mInterposition; }
    nsCOMPtr<nsIAddonInterposition> GetInterposition();

protected:
    virtual ~XPCWrappedNativeScope();

    static void KillDyingScopes();

    XPCWrappedNativeScope(); // not implemented

private:
    class ClearInterpositionsObserver MOZ_FINAL : public nsIObserver {
        ~ClearInterpositionsObserver() {}

      public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER
    };

    static XPCWrappedNativeScope* gScopes;
    static XPCWrappedNativeScope* gDyingScopes;

    static InterpositionMap*         gInterpositionMap;

    XPCJSRuntime*                    mRuntime;
    Native2WrappedNativeMap*         mWrappedNativeMap;
    ClassInfo2WrappedNativeProtoMap* mWrappedNativeProtoMap;
    nsRefPtr<nsXPCComponentsBase>    mComponents;
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

    // This is a service that will be use to interpose on all calls out of this
    // scope. If it's null, no interposition is done.
    nsCOMPtr<nsIAddonInterposition>  mInterposition;

    nsAutoPtr<DOMExpandoSet> mDOMExpandoSet;

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
// XPCNativeMember represents a single idl declared method, attribute or
// constant.

// Tight. No virtual methods. Can be bitwise copied (until any resolution done).

class XPCNativeMember
{
public:
    static bool GetCallInfo(JSObject* funobj,
                            XPCNativeInterface** pInterface,
                            XPCNativeMember**    pMember);

    jsid   GetName() const {return mName;}

    uint16_t GetIndex() const {return mIndex;}

    bool GetConstantValue(XPCCallContext& ccx, XPCNativeInterface* iface,
                          jsval* pval)
        {MOZ_ASSERT(IsConstant(),
                    "Only call this if you're sure this is a constant!");
         return Resolve(ccx, iface, JS::NullPtr(), pval);}

    bool NewFunctionObject(XPCCallContext& ccx, XPCNativeInterface* iface,
                           JS::HandleObject parent, jsval* pval);

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

    /* default ctor - leave random contents */
    XPCNativeMember()  {MOZ_COUNT_CTOR(XPCNativeMember);}
    ~XPCNativeMember() {MOZ_COUNT_DTOR(XPCNativeMember);}

private:
    bool Resolve(XPCCallContext& ccx, XPCNativeInterface* iface,
                 JS::HandleObject parent, jsval *vp);

    enum {
        METHOD      = 0x01,
        CONSTANT    = 0x02,
        GETTER      = 0x04,
        SETTER_TOO  = 0x08
    };

private:
    // our only data...
    jsid     mName;
    uint16_t mIndex;
    uint16_t mFlags;
};

/***************************************************************************/
// XPCNativeInterface represents a single idl declared interface. This is
// primarily the set of XPCNativeMembers.

// Tight. No virtual methods.

class XPCNativeInterface
{
  public:
    static XPCNativeInterface* GetNewOrUsed(const nsIID* iid);
    static XPCNativeInterface* GetNewOrUsed(nsIInterfaceInfo* info);
    static XPCNativeInterface* GetNewOrUsed(const char* name);
    static XPCNativeInterface* GetISupports();

    inline nsIInterfaceInfo* GetInterfaceInfo() const {return mInfo.get();}
    inline jsid              GetName()          const {return mName;}

    inline const nsIID* GetIID() const;
    inline const char*  GetNameString() const;
    inline XPCNativeMember* FindMember(jsid name) const;

    inline bool HasAncestor(const nsIID* iid) const;

    uint16_t GetMemberCount() const {
        return mMemberCount;
    }
    XPCNativeMember* GetMemberAt(uint16_t i) {
        MOZ_ASSERT(i < mMemberCount, "bad index");
        return &mMembers[i];
    }

    void DebugDump(int16_t depth);

#define XPC_NATIVE_IFACE_MARK_FLAG ((uint16_t)JS_BIT(15)) // only high bit of 16 is set

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

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

  protected:
    static XPCNativeInterface* NewInstance(nsIInterfaceInfo* aInfo);

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
    uint16_t                   mMemberCount : 15;
    uint16_t                   mMarked : 1;
    XPCNativeMember            mMembers[1]; // always last - object sized for array
};

/***************************************************************************/
// XPCNativeSetKey is used to key a XPCNativeSet in a NativeSetMap.

class XPCNativeSetKey
{
public:
    XPCNativeSetKey(XPCNativeSet*       BaseSet  = nullptr,
                    XPCNativeInterface* Addition = nullptr,
                    uint16_t            Position = 0)
        : mIsAKey(IS_A_KEY), mPosition(Position), mBaseSet(BaseSet),
          mAddition(Addition) {}
    ~XPCNativeSetKey() {}

    XPCNativeSet*           GetBaseSet()  const {return mBaseSet;}
    XPCNativeInterface*     GetAddition() const {return mAddition;}
    uint16_t                GetPosition() const {return mPosition;}

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
    // virtual methods and their first data member is a uint16_t. We are
    // confident that XPCNativeSet->mMemberCount will never be 0xffff.

    bool                    IsAKey() const {return mIsAKey == IS_A_KEY;}

    enum {IS_A_KEY = 0xffff};

    // Allow shallow copy

private:
    uint16_t                mIsAKey;    // must be first data member
    uint16_t                mPosition;
    XPCNativeSet*           mBaseSet;
    XPCNativeInterface*     mAddition;
};

/***************************************************************************/
// XPCNativeSet represents an ordered collection of XPCNativeInterface pointers.

class XPCNativeSet
{
  public:
    static XPCNativeSet* GetNewOrUsed(const nsIID* iid);
    static XPCNativeSet* GetNewOrUsed(nsIClassInfo* classInfo);
    static XPCNativeSet* GetNewOrUsed(XPCNativeSet* otherSet,
                                      XPCNativeInterface* newInterface,
                                      uint16_t position);

    // This generates a union set.
    //
    // If preserveFirstSetOrder is true, the elements from |firstSet| come first,
    // followed by any non-duplicate items from |secondSet|. If false, the same
    // algorithm is applied; but if we detect that |secondSet| is a superset of
    // |firstSet|, we return |secondSet| without worrying about whether the
    // ordering might differ from |firstSet|.
    static XPCNativeSet* GetNewOrUsed(XPCNativeSet* firstSet,
                                      XPCNativeSet* secondSet,
                                      bool preserveFirstSetOrder);

    static void ClearCacheEntryForClassInfo(nsIClassInfo* classInfo);

    inline bool FindMember(jsid name, XPCNativeMember** pMember,
                             uint16_t* pInterfaceIndex) const;

    inline bool FindMember(jsid name, XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface) const;

    inline bool FindMember(jsid name,
                             XPCNativeMember** pMember,
                             XPCNativeInterface** pInterface,
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
    XPCNativeInterface **GetInterfaceArray() {
        return mInterfaces;
    }

    XPCNativeInterface* GetInterfaceAt(uint16_t i)
        {MOZ_ASSERT(i < mInterfaceCount, "bad index"); return mInterfaces[i];}

    inline bool MatchesSetUpToInterface(const XPCNativeSet* other,
                                          XPCNativeInterface* iface) const;

#define XPC_NATIVE_SET_MARK_FLAG ((uint16_t)JS_BIT(15)) // only high bit of 16 is set

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

    void DebugDump(int16_t depth);

    static void DestroyInstance(XPCNativeSet* inst);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

  protected:
    static XPCNativeSet* NewInstance(XPCNativeInterface** array,
                                     uint16_t count);
    static XPCNativeSet* NewInstanceMutate(XPCNativeSet*       otherSet,
                                           XPCNativeInterface* newInterface,
                                           uint16_t            position);
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
    uint16_t                mMemberCount;
    uint16_t                mInterfaceCount : 15;
    uint16_t                mMarked : 1;
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
    bool IsMarked() const {return 0 != (mFlags & XPC_WN_SJSFLAGS_MARK_FLAG);}

#ifdef GET_IT
#undef GET_IT
#endif
#define GET_IT(f_) const {return 0 != (mFlags & nsIXPCScriptable:: f_ );}

    bool WantPreCreate()                GET_IT(WANT_PRECREATE)
    bool WantCreate()                   GET_IT(WANT_CREATE)
    bool WantPostCreate()               GET_IT(WANT_POSTCREATE)
    bool WantAddProperty()              GET_IT(WANT_ADDPROPERTY)
    bool WantDelProperty()              GET_IT(WANT_DELPROPERTY)
    bool WantGetProperty()              GET_IT(WANT_GETPROPERTY)
    bool WantSetProperty()              GET_IT(WANT_SETPROPERTY)
    bool WantEnumerate()                GET_IT(WANT_ENUMERATE)
    bool WantNewEnumerate()             GET_IT(WANT_NEWENUMERATE)
    bool WantNewResolve()               GET_IT(WANT_NEWRESOLVE)
    bool WantConvert()                  GET_IT(WANT_CONVERT)
    bool WantFinalize()                 GET_IT(WANT_FINALIZE)
    bool WantCall()                     GET_IT(WANT_CALL)
    bool WantConstruct()                GET_IT(WANT_CONSTRUCT)
    bool WantHasInstance()              GET_IT(WANT_HASINSTANCE)
    bool WantOuterObject()              GET_IT(WANT_OUTER_OBJECT)
    bool UseJSStubForAddProperty()      GET_IT(USE_JSSTUB_FOR_ADDPROPERTY)
    bool UseJSStubForDelProperty()      GET_IT(USE_JSSTUB_FOR_DELPROPERTY)
    bool UseJSStubForSetProperty()      GET_IT(USE_JSSTUB_FOR_SETPROPERTY)
    bool DontEnumStaticProps()          GET_IT(DONT_ENUM_STATIC_PROPS)
    bool DontEnumQueryInterface()       GET_IT(DONT_ENUM_QUERY_INTERFACE)
    bool DontAskInstanceForScriptable() GET_IT(DONT_ASK_INSTANCE_FOR_SCRIPTABLE)
    bool ClassInfoInterfacesOnly()      GET_IT(CLASSINFO_INTERFACES_ONLY)
    bool AllowPropModsDuringResolve()   GET_IT(ALLOW_PROP_MODS_DURING_RESOLVE)
    bool AllowPropModsToPrototype()     GET_IT(ALLOW_PROP_MODS_TO_PROTOTYPE)
    bool IsGlobalObject()               GET_IT(IS_GLOBAL_OBJECT)
    bool DontReflectInterfaceNames()    GET_IT(DONT_REFLECT_INTERFACE_NAMES)

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
    uint32_t interfacesBitmap;
};

class XPCNativeScriptableShared
{
public:
    const XPCNativeScriptableFlags& GetFlags() const {return mFlags;}
    uint32_t                        GetInterfacesBitmap() const
        {return mJSClass.interfacesBitmap;}
    const JSClass*                  GetJSClass()
        {return Jsvalify(&mJSClass.base);}

    XPCNativeScriptableShared(uint32_t aFlags, char* aName,
                              uint32_t interfacesBitmap)
        : mFlags(aFlags)
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
    bool IsMarked() const {return mFlags.IsMarked();}

private:
    XPCNativeScriptableFlags mFlags;
    XPCWrappedNativeJSClass  mJSClass;
};

/***************************************************************************/
// XPCNativeScriptableInfo is used to hold the nsIXPCScriptable state for a
// given class or instance.

class XPCNativeScriptableInfo
{
public:
    static XPCNativeScriptableInfo*
    Construct(const XPCNativeScriptableCreateInfo* sci);

    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mShared->GetFlags();}

    uint32_t
    GetInterfacesBitmap() const {return mShared->GetInterfacesBitmap();}

    const JSClass*
    GetJSClass()          {return mShared->GetJSClass();}

    XPCNativeScriptableShared*
    GetScriptableShared() {return mShared;}

    void
    SetCallback(nsIXPCScriptable* s) {mCallback = s;}
    void
    SetCallback(already_AddRefed<nsIXPCScriptable>&& s) {mCallback = s;}

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

class MOZ_STACK_CLASS XPCNativeScriptableCreateInfo
{
public:

    XPCNativeScriptableCreateInfo(const XPCNativeScriptableInfo& si)
        : mCallback(si.GetCallback()), mFlags(si.GetFlags()),
          mInterfacesBitmap(si.GetInterfacesBitmap()) {}

    XPCNativeScriptableCreateInfo(already_AddRefed<nsIXPCScriptable>&& callback,
                                  XPCNativeScriptableFlags flags,
                                  uint32_t interfacesBitmap)
        : mCallback(callback), mFlags(flags),
          mInterfacesBitmap(interfacesBitmap) {}

    XPCNativeScriptableCreateInfo()
        : mFlags(0), mInterfacesBitmap(0) {}


    nsIXPCScriptable*
    GetCallback() const {return mCallback;}

    const XPCNativeScriptableFlags&
    GetFlags() const      {return mFlags;}

    uint32_t
    GetInterfacesBitmap() const     {return mInterfacesBitmap;}

    void
    SetCallback(already_AddRefed<nsIXPCScriptable>&& callback)
        {mCallback = callback;}

    void
    SetFlags(const XPCNativeScriptableFlags& flags)  {mFlags = flags;}

    void
    SetInterfacesBitmap(uint32_t interfacesBitmap)
        {mInterfacesBitmap = interfacesBitmap;}

private:
    nsCOMPtr<nsIXPCScriptable>  mCallback;
    XPCNativeScriptableFlags    mFlags;
    uint32_t                    mInterfacesBitmap;
};

/***********************************************/
// XPCWrappedNativeProto hold the additional shared wrapper data
// for XPCWrappedNative whose native objects expose nsIClassInfo.

class XPCWrappedNativeProto
{
public:
    static XPCWrappedNativeProto*
    GetNewOrUsed(XPCWrappedNativeScope* scope,
                 nsIClassInfo* classInfo,
                 const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
                 bool callPostCreatePrototype = true);

    XPCWrappedNativeScope*
    GetScope()   const {return mScope;}

    XPCJSRuntime*
    GetRuntime() const {return mScope->GetRuntime();}

    JSObject*
    GetJSProtoObject() const {
        JS::ExposeObjectToActiveJS(mJSProtoObject);
        return mJSProtoObject;
    }

    nsIClassInfo*
    GetClassInfo()     const {return mClassInfo;}

    XPCNativeSet*
    GetSet()           const {return mSet;}

    XPCNativeScriptableInfo*
    GetScriptableInfo()   {return mScriptableInfo;}

    uint32_t
    GetClassInfoFlags() const {return mClassInfoFlags;}

#ifdef GET_IT
#undef GET_IT
#endif
#define GET_IT(f_) const {return !!(mClassInfoFlags & nsIClassInfo:: f_ );}

    bool ClassIsSingleton()           GET_IT(SINGLETON)
    bool ClassIsDOMObject()           GET_IT(DOM_OBJECT)
    bool ClassIsPluginObject()        GET_IT(PLUGIN_OBJECT)

#undef GET_IT

    void SetScriptableInfo(XPCNativeScriptableInfo* si)
        {MOZ_ASSERT(!mScriptableInfo, "leak here!"); mScriptableInfo = si;}

    bool CallPostCreatePrototype();
    void JSProtoObjectFinalized(js::FreeOp *fop, JSObject *obj);

    void SystemIsBeingShutDown();

    void DebugDump(int16_t depth);

    void TraceSelf(JSTracer *trc) {
        if (mJSProtoObject)
            mJSProtoObject.trace(trc, "XPCWrappedNativeProto::mJSProtoObject");
    }

    void TraceInside(JSTracer *trc) {
        if (JS_IsGCMarkingTracer(trc)) {
            mSet->Mark();
            if (mScriptableInfo)
                mScriptableInfo->Mark();
        }

        GetScope()->TraceInside(trc);
    }

    void TraceJS(JSTracer *trc) {
        TraceSelf(trc);
        TraceInside(trc);
    }

    void WriteBarrierPre(JSRuntime* rt)
    {
        if (JS::IsIncrementalBarrierNeeded(rt) && mJSProtoObject)
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
                          uint32_t ClassInfoFlags,
                          XPCNativeSet* Set);

    bool Init(const XPCNativeScriptableCreateInfo* scriptableCreateInfo,
              bool callPostCreatePrototype);

private:
#ifdef DEBUG
    static int32_t gDEBUG_LiveProtoCount;
#endif

private:
    XPCWrappedNativeScope*   mScope;
    JS::ObjectPtr            mJSProtoObject;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    uint32_t                 mClassInfoFlags;
    XPCNativeSet*            mSet;
    XPCNativeScriptableInfo* mScriptableInfo;
};

/***********************************************/
// XPCWrappedNativeTearOff represents the info needed to make calls to one
// interface on the underlying native object of a XPCWrappedNative.

class XPCWrappedNativeTearOff
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
    {
        JS::ExposeObjectToActiveJS(mFlatJSObject);
        return mFlatJSObject;
    }

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

    XPCNativeSet*
    GetSet() const {return mSet;}

    void
    SetSet(XPCNativeSet* set) {mSet = set;}

    static XPCWrappedNative* Get(JSObject *obj) {
        MOZ_ASSERT(IS_WN_REFLECTOR(obj));
        return (XPCWrappedNative*)js::GetObjectPrivate(obj);
    }

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
    WrapNewGlobal(xpcObjectHelper &nativeHelper,
                  nsIPrincipal *principal, bool initStandardClasses,
                  JS::CompartmentOptions& aOptions,
                  XPCWrappedNative **wrappedGlobal);

    static nsresult
    GetNewOrUsed(xpcObjectHelper& helper,
                 XPCWrappedNativeScope* Scope,
                 XPCNativeInterface* Interface,
                 XPCWrappedNative** wrapper);

public:
    static nsresult
    GetUsedOnly(nsISupports* Object,
                XPCWrappedNativeScope* Scope,
                XPCNativeInterface* Interface,
                XPCWrappedNative** wrapper);

    static nsresult
    ReparentWrapperIfFound(XPCWrappedNativeScope* aOldScope,
                           XPCWrappedNativeScope* aNewScope,
                           JS::HandleObject aNewParent,
                           nsISupports* aCOMObj);

    nsresult RescueOrphans();

    void FlatJSObjectFinalized();

    void SystemIsBeingShutDown();

    enum CallMode {CALL_METHOD, CALL_GETTER, CALL_SETTER};

    static bool CallMethod(XPCCallContext& ccx,
                           CallMode mode = CALL_METHOD);

    static bool GetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_GETTER);}

    static bool SetAttribute(XPCCallContext& ccx)
        {return CallMethod(ccx, CALL_SETTER);}

    inline bool HasInterfaceNoQI(const nsIID& iid);

    XPCWrappedNativeTearOff* LocateTearOff(XPCNativeInterface* aInterface);
    XPCWrappedNativeTearOff* FindTearOff(XPCNativeInterface* aInterface,
                                         bool needJSObject = false,
                                         nsresult* pError = nullptr);
    XPCWrappedNativeTearOff* FindTearOff(const nsIID& iid);

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
            GetScope()->TraceInside(trc);
        if (mFlatJSObject && JS_IsGlobalObject(mFlatJSObject))
        {
            xpc::TraceXPCGlobal(trc, mFlatJSObject);
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
        if (mFlatJSObject) {
            JS_CallTenuredObjectTracer(trc, &mFlatJSObject,
                                       "XPCWrappedNative::mFlatJSObject");
        }
    }

    static void Trace(JSTracer *trc, JSObject *obj);

    void AutoTrace(JSTracer *trc) {
        TraceSelf(trc);
    }

#ifdef DEBUG
    void ASSERT_SetsNotMarked() const
        {mSet->ASSERT_NotMarked();
         if (HasProto()){GetProto()->ASSERT_SetNotMarked();}}
#endif

    inline void SweepTearOffs();

    // Returns a string that shuld be free'd using JS_smprintf_free (or null).
    char* ToString(XPCWrappedNativeTearOff* to = nullptr) const;

    static void GatherProtoScriptableCreateInfo(nsIClassInfo* classInfo,
                                                XPCNativeScriptableCreateInfo& sciProto);

    bool HasExternalReference() const {return mRefCnt > 1;}

    void NoteTearoffs(nsCycleCollectionTraversalCallback& cb);

    // Make ctor and dtor protected (rather than private) to placate nsCOMPtr.
protected:
    XPCWrappedNative(); // not implemented

    // This ctor is used if this object will have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                     XPCWrappedNativeProto* aProto);

    // This ctor is used if this object will NOT have a proto.
    XPCWrappedNative(already_AddRefed<nsISupports>&& aIdentity,
                     XPCWrappedNativeScope* aScope,
                     XPCNativeSet* aSet);

    virtual ~XPCWrappedNative();
    void Destroy();

    void UpdateScriptableInfo(XPCNativeScriptableInfo *si);

private:
    enum {
        // Flags bits for mFlatJSObject:
        FLAT_JS_OBJECT_VALID = JS_BIT(0)
    };

private:

    bool Init(JS::HandleObject parent, const XPCNativeScriptableCreateInfo* sci);
    bool FinishInit();

    bool ExtendSet(XPCNativeInterface* aInterface);

    nsresult InitTearOff(XPCWrappedNativeTearOff* aTearOff,
                         XPCNativeInterface* aInterface,
                         bool needJSObject);

    bool InitTearOffJSObject(XPCWrappedNativeTearOff* to);

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
    JS::TenuredHeap<JSObject*>   mFlatJSObject;
    XPCNativeScriptableInfo*     mScriptableInfo;
    XPCWrappedNativeTearOffChunk mFirstChunk;
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

class nsXPCWrappedJSClass : public nsIXPCWrappedJSClass
{
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_IMETHOD DebugDump(int16_t depth);
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

    static nsresult CheckForException(XPCCallContext & ccx,
                                      const char * aPropertyName,
                                      const char * anInterfaceName,
                                      bool aForceReport);
private:
    virtual ~nsXPCWrappedJSClass();

    nsXPCWrappedJSClass();   // not implemented
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
                               uint32_t* result);

    bool GetInterfaceTypeFromParam(JSContext* cx,
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
    nsCOMPtr<nsIInterfaceInfo> mInfo;
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
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIXPCONNECTJSOBJECTHOLDER
    NS_DECL_NSIXPCONNECTWRAPPEDJS
    NS_DECL_NSISUPPORTSWEAKREFERENCE
    NS_DECL_NSIPROPERTYBAG

    NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS_AMBIGUOUS(nsXPCWrappedJS, nsIXPConnectWrappedJS)

    NS_IMETHOD CallMethod(uint16_t methodIndex,
                          const XPTMethodDescriptor *info,
                          nsXPTCMiniVariant* params);

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
    JSObject* GetJSObjectPreserveColor() const {return mJSObj;}

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

    bool IsRootWrapper() const {return mRoot == this;}
    bool IsValid() const {return mJSObj != nullptr;}
    void SystemIsBeingShutDown();

    // These two methods are used by JSObject2WrappedJSMap::FindDyingJSObjects
    // to find non-rooting wrappers for dying JS objects. See the top of
    // XPCWrappedJS.cpp for more details.
    bool IsSubjectToFinalization() const {return IsValid() && mRefCnt == 1;}
    bool IsObjectAboutToBeFinalized() {return JS_IsAboutToBeFinalized(&mJSObj);}

    bool IsAggregatedToNative() const {return mRoot->mOuter != nullptr;}
    nsISupports* GetAggregatedNativeObject() const {return mRoot->mOuter;}
    void SetAggregatedNativeObject(nsISupports *aNative) {
        MOZ_ASSERT(aNative);
        if (mRoot->mOuter) {
            MOZ_ASSERT(mRoot->mOuter == aNative,
                       "Only one aggregated native can be set");
            return;
        }
        mRoot->mOuter = aNative;
    }

    void TraceJS(JSTracer* trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);

    virtual ~nsXPCWrappedJS();
protected:
    nsXPCWrappedJS();   // not implemented
    nsXPCWrappedJS(JSContext* cx,
                   JSObject* aJSObj,
                   nsXPCWrappedJSClass* aClass,
                   nsXPCWrappedJS* root);

    bool CanSkip();
    void Destroy();
    void Unlink();

private:
    JS::Heap<JSObject*> mJSObj;
    nsRefPtr<nsXPCWrappedJSClass> mClass;
    nsXPCWrappedJS* mRoot;    // If mRoot != this, it is an owning pointer.
    nsXPCWrappedJS* mNext;
    nsCOMPtr<nsISupports> mOuter;    // only set in root
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
    static XPCJSObjectHolder* newHolder(JSObject* obj);

    void TraceJS(JSTracer *trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);

private:
    virtual ~XPCJSObjectHolder();

    XPCJSObjectHolder(JSObject* obj);
    XPCJSObjectHolder(); // not implemented

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
     * Convert a native object into a jsval.
     *
     * @param d [out] the resulting jsval
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
     * @param Interface the interface of src that we want
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
                                         XPCNativeInterface** Interface,
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
     * Convert a native array into a jsval.
     *
     * @param d [out] the resulting jsval
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

    static nsresult JSErrorToXPCException(const char* message,
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

    static bool CheckForPendingException(nsresult result, JSContext *cx);

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

class nsJSID : public nsIJSID
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_JS_ID_CID)

    NS_DECL_ISUPPORTS
    NS_DECL_NSIJSID

    bool InitWithName(const nsID& id, const char *nameString);
    bool SetName(const char* name);
    void   SetNameToNoString()
        {MOZ_ASSERT(!mName, "name already set"); mName = gNoString;}
    bool NameIsSet() const {return nullptr != mName;}
    const nsID& ID() const {return mID;}
    bool IsValid() const {return !mID.Equals(GetInvalidIID());}

    static already_AddRefed<nsJSID> NewID(const char* str);
    static already_AddRefed<nsJSID> NewID(const nsID& id);

    nsJSID();
    virtual ~nsJSID();

    void Reset();
    const nsID& GetInvalidIID() const;

protected:
    static char gNoString[];
    nsID    mID;
    char*   mNumber;
    char*   mName;
};

namespace mozilla {
template<>
struct HasDangerousPublicDestructor<nsJSID>
{
  static const bool value = true;
};
}

// nsJSIID

class nsJSIID : public nsIJSIID,
                public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS

    // we manually delagate these to nsJSID
    NS_DECL_NSIJSID

    // we implement the rest...
    NS_DECL_NSIJSIID
    NS_DECL_NSIXPCSCRIPTABLE

    static already_AddRefed<nsJSIID> NewID(nsIInterfaceInfo* aInfo);

    nsJSIID(nsIInterfaceInfo* aInfo);
    nsJSIID(); // not implemented

private:
    virtual ~nsJSIID();

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

    static already_AddRefed<nsJSCID> NewID(const char* str);

    nsJSCID();

private:
    virtual ~nsJSCID();

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
        savedFrameChain(false)
    {}
    JSContext* cx;

    // Whether the frame chain was saved
    bool savedFrameChain;
};

namespace xpc {

// These functions are used in a few places where a callback model makes it
// impossible to push a JSContext using one of our stack-scoped classes. We
// depend on those stack-scoped classes to maintain nsIScriptContext
// invariants, so these functions may only be used of the context is not
// associated with an nsJSContext/nsIScriptContext.
bool PushJSContextNoScriptContext(JSContext *aCx);
void PopJSContextNoScriptContext();

} /* namespace xpc */

class XPCJSContextStack
{
public:
    XPCJSContextStack(XPCJSRuntime *aRuntime)
      : mRuntime(aRuntime)
      , mSafeJSContext(nullptr)
      , mSafeJSContextGlobal(aRuntime->Runtime(), nullptr)
    { }

    virtual ~XPCJSContextStack();

    uint32_t Count()
    {
        return mStack.Length();
    }

    JSContext *Peek()
    {
        return mStack.IsEmpty() ? nullptr : mStack[mStack.Length() - 1].cx;
    }

    JSContext *InitSafeJSContext();
    JSContext *GetSafeJSContext();
    JSObject *GetSafeJSContextGlobal();
    bool HasJSContext(JSContext *cx);

    const InfallibleTArray<XPCJSContextInfo>* GetStack()
    { return &mStack; }

private:
    friend class mozilla::AutoCxPusher;
    friend bool xpc::PushJSContextNoScriptContext(JSContext *aCx);;
    friend void xpc::PopJSContextNoScriptContext();

    // We make these private so that stack manipulation can only happen
    // through one of the above friends.
    JSContext *Pop();
    bool Push(JSContext *cx);

    AutoInfallibleTArray<XPCJSContextInfo, 16> mStack;
    XPCJSRuntime* mRuntime;
    JSContext*  mSafeJSContext;
    JS::PersistentRootedObject mSafeJSContextGlobal;
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

    XPCWrappedNativeScope *GetScope() { return mScope; }

protected:
    virtual ~nsXPCComponentsBase();

    nsXPCComponentsBase(XPCWrappedNativeScope* aScope);
    virtual void ClearMembers();

    XPCWrappedNativeScope*                   mScope;

    // Unprivileged members from nsIXPCComponentsBase.
    nsRefPtr<nsXPCComponents_Interfaces>     mInterfaces;
    nsRefPtr<nsXPCComponents_InterfacesByID> mInterfacesByID;
    nsRefPtr<nsXPCComponents_Results>        mResults;

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
    nsXPCComponents(XPCWrappedNativeScope* aScope);
    virtual ~nsXPCComponents();
    virtual void ClearMembers() MOZ_OVERRIDE;

    // Privileged members added by nsIXPCComponents.
    nsRefPtr<nsXPCComponents_Classes>     mClasses;
    nsRefPtr<nsXPCComponents_ClassesByID> mClassesByID;
    nsRefPtr<nsXPCComponents_ID>          mID;
    nsRefPtr<nsXPCComponents_Exception>   mException;
    nsRefPtr<nsXPCComponents_Constructor> mConstructor;
    nsRefPtr<nsXPCComponents_Utils>       mUtils;

    friend class XPCWrappedNativeScope;
};


/***************************************************************************/

extern JSObject*
xpc_NewIDObject(JSContext *cx, JS::HandleObject jsobj, const nsID& aID);

extern const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj);

extern bool
xpc_JSObjectIsID(JSContext *cx, JSObject* obj);

/***************************************************************************/
// in XPCDebug.cpp

extern bool
xpc_DumpJSStack(JSContext* cx, bool showArgs, bool showLocals,
                bool showThisProps);

// Return a newly-allocated string containing a representation of the
// current JS stack.  It is the *caller's* responsibility to free this
// string with JS_smprintf_free().
extern char*
xpc_PrintJSStack(JSContext* cx, bool showArgs, bool showLocals,
                 bool showThisProps);

extern bool
xpc_DumpEvalInJSStackFrame(JSContext* cx, uint32_t frameno, const char* text);


/***************************************************************************/

// Definition of nsScriptError, defined here because we lack a place to put
// XPCOM objects associated with the JavaScript engine.
class nsScriptError : public nsIScriptError {
public:
    nsScriptError();

  // TODO - do something reasonable on getting null from these babies.

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICONSOLEMESSAGE
    NS_DECL_NSISCRIPTERROR

private:
    virtual ~nsScriptError();

    nsString mMessage;
    nsString mSourceName;
    uint32_t mLineNumber;
    nsString mSourceLine;
    uint32_t mColumnNumber;
    uint32_t mFlags;
    nsCString mCategory;
    uint64_t mOuterWindowID;
    uint64_t mInnerWindowID;
    int64_t mTimeStamp;
    bool mIsFromPrivateWindow;
};

/******************************************************************************
 * Handles pre/post script processing and the setting/resetting the error
 * reporter
 */
class MOZ_STACK_CLASS AutoScriptEvaluate
{
public:
    /**
     * Saves the JSContext as well as initializing our state
     * @param cx The JSContext, this can be null, we don't do anything then
     */
    AutoScriptEvaluate(JSContext * cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
         : mJSContext(cx), mErrorReporterSet(false), mEvaluated(false) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    /**
     * Does the pre script evaluation and sets the error reporter if given
     * This function should only be called once, and will assert if called
     * more than once
     * @param errorReporter the error reporter callback function to set
     */

    bool StartEvaluating(JS::HandleObject scope, JSErrorReporter errorReporter = nullptr);

    /**
     * Does the post script evaluation and resets the error reporter
     */
    ~AutoScriptEvaluate();
private:
    JSContext* mJSContext;
    mozilla::Maybe<JS::AutoSaveExceptionState> mState;
    bool mErrorReporterSet;
    bool mEvaluated;
    mozilla::Maybe<JSAutoCompartment> mAutoCompartment;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

    // No copying or assignment allowed
    AutoScriptEvaluate(const AutoScriptEvaluate &) MOZ_DELETE;
    AutoScriptEvaluate & operator =(const AutoScriptEvaluate &) MOZ_DELETE;
};

/***************************************************************************/
class MOZ_STACK_CLASS AutoResolveName
{
public:
    AutoResolveName(XPCCallContext& ccx, JS::HandleId name
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM) :
          mOld(ccx, XPCJSRuntime::Get()->SetResolveName(name))
#ifdef DEBUG
          ,mCheck(ccx, name)
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
            MOZ_ASSERT(old == mCheck, "Bad Nesting!");
        }

private:
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
    AutoMarkingPtr(JSContext* cx) {
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
    TypedAutoMarkingPtr(JSContext* cx) : AutoMarkingPtr(cx), mPtr(nullptr) {}
    TypedAutoMarkingPtr(JSContext* cx, T* ptr) : AutoMarkingPtr(cx), mPtr(ptr) {}

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
typedef TypedAutoMarkingPtr<XPCNativeScriptableInfo> AutoMarkingNativeScriptableInfoPtr;

template<class T>
class ArrayAutoMarkingPtr : public AutoMarkingPtr
{
  public:
    ArrayAutoMarkingPtr(JSContext* cx)
      : AutoMarkingPtr(cx), mPtr(nullptr), mCount(0) {}
    ArrayAutoMarkingPtr(JSContext* cx, T** ptr, uint32_t count, bool clear)
      : AutoMarkingPtr(cx), mPtr(ptr), mCount(count)
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
        for (uint32_t i = 0; i < mCount; i++) {
            if (mPtr[i]) {
                mPtr[i]->TraceJS(trc);
                mPtr[i]->AutoTrace(trc);
            }
        }
    }

    virtual void MarkAfterJSFinalize()
    {
        for (uint32_t i = 0; i < mCount; i++) {
            if (mPtr[i])
                mPtr[i]->Mark();
        }
    }

  private:
    T** mPtr;
    uint32_t mCount;
};

typedef ArrayAutoMarkingPtr<XPCNativeInterface> AutoMarkingNativeInterfacePtrArrayPtr;

/***************************************************************************/
namespace xpc {
// Allocates a string that grants all access ("AllAccess")
char *
CloneAllAccess();

// Returns access if wideName is in list
char *
CheckAccessList(const char16_t *wideName, const char *const list[]);
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

    static already_AddRefed<XPCVariant> newVariant(JSContext* cx, jsval aJSVal);

    /**
     * This getter clears the gray bit before handing out the jsval if the jsval
     * represents a JSObject. That means that the object is guaranteed to be
     * kept alive past the next CC.
     */
    jsval GetJSVal() const {
        if (!mJSVal.isPrimitive())
            JS::ExposeObjectToActiveJS(&mJSVal.toObject());
        return mJSVal;
    }

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

    XPCVariant(JSContext* cx, jsval aJSVal);

    /**
     * Convert a variant into a jsval.
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
    XPCTraceableVariant(JSContext* cx, jsval aJSVal)
        : XPCVariant(cx, aJSVal)
    {
         nsXPConnect::GetRuntimeInstance()->AddVariantRoot(this);
    }

    virtual ~XPCTraceableVariant();

    void TraceJS(JSTracer* trc);
    static void GetTraceName(JSTracer* trc, char *buf, size_t bufsize);
};

/***************************************************************************/
// Utilities

inline void *
xpc_GetJSPrivate(JSObject *obj)
{
    return js::GetObjectPrivate(obj);
}

inline JSContext *
xpc_GetSafeJSContext()
{
    return XPCJSRuntime::Get()->GetJSContextStack()->GetSafeJSContext();
}

namespace xpc {

JSAddonId *
NewAddonId(JSContext *cx, const nsACString &id);

// JSNatives to expose atob and btoa in various non-DOM XPConnect scopes.
bool
Atob(JSContext *cx, unsigned argc, jsval *vp);

bool
Btoa(JSContext *cx, unsigned argc, jsval *vp);

// Helper function that creates a JSFunction that wraps a native function that
// forwards the call to the original 'callable'.
bool
NewFunctionForwarder(JSContext *cx, JS::HandleId id, JS::HandleObject callable,
                     JS::MutableHandleValue vp);

// Old fashioned xpc error reporter. Try to use JS_ReportError instead.
nsresult
ThrowAndFail(nsresult errNum, JSContext *cx, bool *retval);

struct GlobalProperties {
    GlobalProperties() {
      mozilla::PodZero(this);

      // Promise is supposed to be part of ES, and therefore should appear on
      // every global.
      Promise = true;
    }
    bool Parse(JSContext *cx, JS::HandleObject obj);
    bool Define(JSContext *cx, JS::HandleObject obj);
    bool CSS : 1;
    bool Promise : 1;
    bool indexedDB : 1;
    bool XMLHttpRequest : 1;
    bool TextDecoder : 1;
    bool TextEncoder : 1;
    bool URL : 1;
    bool atob : 1;
    bool btoa : 1;
};

// Infallible.
already_AddRefed<nsIXPCComponents_utils_Sandbox>
NewSandboxConstructor();

// Returns true if class of 'obj' is SandboxClass.
bool
IsSandbox(JSObject *obj);

class MOZ_STACK_CLASS OptionsBase {
public:
    OptionsBase(JSContext *cx = xpc_GetSafeJSContext(),
                JSObject *options = nullptr)
        : mCx(cx)
        , mObject(cx, options)
    { }

    virtual bool Parse() = 0;

protected:
    bool ParseValue(const char *name, JS::MutableHandleValue prop, bool *found = nullptr);
    bool ParseBoolean(const char *name, bool *prop);
    bool ParseObject(const char *name, JS::MutableHandleObject prop);
    bool ParseJSString(const char *name, JS::MutableHandleString prop);
    bool ParseString(const char *name, nsCString &prop);
    bool ParseString(const char *name, nsString &prop);
    bool ParseId(const char* name, JS::MutableHandleId id);

    JSContext *mCx;
    JS::RootedObject mObject;
};

class MOZ_STACK_CLASS SandboxOptions : public OptionsBase {
public:
    SandboxOptions(JSContext *cx = xpc_GetSafeJSContext(),
                   JSObject *options = nullptr)
        : OptionsBase(cx, options)
        , wantXrays(true)
        , wantComponents(true)
        , wantExportHelpers(false)
        , proto(cx)
        , addonId(cx)
        , writeToGlobalPrototype(false)
        , sameZoneAs(cx)
        , invisibleToDebugger(false)
        , discardSource(false)
        , metadata(cx)
    { }

    virtual bool Parse();

    bool wantXrays;
    bool wantComponents;
    bool wantExportHelpers;
    JS::RootedObject proto;
    nsCString sandboxName;
    JS::RootedString addonId;
    bool writeToGlobalPrototype;
    JS::RootedObject sameZoneAs;
    bool invisibleToDebugger;
    bool discardSource;
    GlobalProperties globalProperties;
    JS::RootedValue metadata;

protected:
    bool ParseGlobalProperties();
};

class MOZ_STACK_CLASS CreateObjectInOptions : public OptionsBase {
public:
    CreateObjectInOptions(JSContext *cx = xpc_GetSafeJSContext(),
                          JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , defineAs(cx, JSID_VOID)
    { }

    virtual bool Parse() { return ParseId("defineAs", &defineAs); };

    JS::RootedId defineAs;
};

class MOZ_STACK_CLASS ExportFunctionOptions : public OptionsBase {
public:
    ExportFunctionOptions(JSContext *cx = xpc_GetSafeJSContext(),
                  JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , defineAs(cx, JSID_VOID)
    { }

    virtual bool Parse() {
        return ParseId("defineAs", &defineAs);
    };

    JS::RootedId defineAs;
};

class MOZ_STACK_CLASS StackScopedCloneOptions : public OptionsBase {
public:
    StackScopedCloneOptions(JSContext *cx = xpc_GetSafeJSContext(),
                            JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , wrapReflectors(false)
        , cloneFunctions(false)
    { }

    virtual bool Parse() {
        return ParseBoolean("wrapReflectors", &wrapReflectors) &&
               ParseBoolean("cloneFunctions", &cloneFunctions);
    };

    // When a reflector is encountered, wrap it rather than aborting the clone.
    bool wrapReflectors;

    // When a function is encountered, clone it (exportFunction-style) rather than
    // aborting the clone.
    bool cloneFunctions;
};

JSObject *
CreateGlobalObject(JSContext *cx, const JSClass *clasp, nsIPrincipal *principal,
                   JS::CompartmentOptions& aOptions);

// InitGlobalObject enters the compartment of aGlobal, so it doesn't matter what
// compartment aJSContext is in.
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
CreateSandboxObject(JSContext *cx, JS::MutableHandleValue vp, nsISupports *prinOrSop,
                    xpc::SandboxOptions& options);
// Helper for evaluating scripts in a sandbox object created with
// CreateSandboxObject(). The caller is responsible of ensuring
// that *rval doesn't get collected during the call or usage after the
// call. This helper will use filename and lineNo for error reporting,
// and if no filename is provided it will use the codebase from the
// principal and line number 1 as a fallback.
nsresult
EvalInSandbox(JSContext *cx, JS::HandleObject sandbox, const nsAString& source,
              const nsACString& filename, int32_t lineNo,
              JSVersion jsVersion, JS::MutableHandleValue rval);

nsresult
GetSandboxAddonId(JSContext *cx, JS::HandleObject sandboxArg,
                  JS::MutableHandleValue rval);

// Helper for retrieving metadata stored in a reserved slot. The metadata
// is set during the sandbox creation using the "metadata" option.
nsresult
GetSandboxMetadata(JSContext *cx, JS::HandleObject sandboxArg,
                   JS::MutableHandleValue rval);

nsresult
SetSandboxMetadata(JSContext *cx, JS::HandleObject sandboxArg,
                   JS::HandleValue metadata);

bool
CreateObjectIn(JSContext *cx, JS::HandleValue vobj, CreateObjectInOptions &options,
               JS::MutableHandleValue rval);

bool
EvalInWindow(JSContext *cx, const nsAString &source, JS::HandleObject scope,
             JS::MutableHandleValue rval);

bool
ExportFunction(JSContext *cx, JS::HandleValue vscope, JS::HandleValue vfunction,
               JS::HandleValue voptions, JS::MutableHandleValue rval);

bool
CloneInto(JSContext *cx, JS::HandleValue vobj, JS::HandleValue vscope,
          JS::HandleValue voptions, JS::MutableHandleValue rval);

bool
StackScopedClone(JSContext *cx, StackScopedCloneOptions &options, JS::MutableHandleValue val);

} /* namespace xpc */


/***************************************************************************/
// Inlined utilities.

inline bool
xpc_ForcePropertyResolve(JSContext* cx, JS::HandleObject obj, jsid id);

inline jsid
GetRTIdByIndex(JSContext *cx, unsigned index);

namespace xpc {

class CompartmentPrivate
{
public:
    enum LocationHint {
        LocationHintRegular,
        LocationHintAddon
    };

    CompartmentPrivate(JSCompartment *c)
        : wantXrays(false)
        , writeToGlobalPrototype(false)
        , skipWriteToGlobalPrototype(false)
        , universalXPConnectEnabled(false)
        , forcePermissiveCOWs(false)
        , adoptedNode(false)
        , donatedNode(false)
        , scriptability(c)
        , scope(nullptr)
    {
        MOZ_COUNT_CTOR(xpc::CompartmentPrivate);
    }

    ~CompartmentPrivate();

    static CompartmentPrivate* Get(JSCompartment *compartment)
    {
        MOZ_ASSERT(compartment);
        void *priv = JS_GetCompartmentPrivate(compartment);
        return static_cast<CompartmentPrivate*>(priv);
    }

    static CompartmentPrivate* Get(JSObject *object)
    {
        JSCompartment *compartment = js::GetObjectCompartment(object);
        return Get(compartment);
    }


    bool wantXrays;

    // This flag is intended for a very specific use, internal to Gecko. It may
    // go away or change behavior at any time. It should not be added to any
    // documentation and it should not be used without consulting the XPConnect
    // module owner.
    bool writeToGlobalPrototype;

    // When writeToGlobalPrototype is true, we use this flag to temporarily
    // disable the writeToGlobalPrototype behavior (when resolving standard
    // classes, for example).
    bool skipWriteToGlobalPrototype;

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

    // for telemetry. See bug 928476.
    bool adoptedNode;
    bool donatedNode;

    // The scriptability of this compartment.
    Scriptability scriptability;

    // Our XPCWrappedNativeScope. This is non-null if and only if this is an
    // XPConnect compartment.
    XPCWrappedNativeScope *scope;

    const nsACString& GetLocation() {
        if (location.IsEmpty() && locationURI) {
            if (NS_FAILED(locationURI->GetSpec(location)))
                location = NS_LITERAL_CSTRING("<unknown location>");
        }
        return location;
    }
    bool GetLocationURI(nsIURI **aURI) {
        return GetLocationURI(LocationHintRegular, aURI);
    }
    bool GetLocationURI(LocationHint aLocationHint, nsIURI **aURI) {
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
    void SetLocationURI(nsIURI *aLocationURI) {
        if (!aLocationURI)
            return;
        if (locationURI)
            return;
        locationURI = aLocationURI;
    }

private:
    nsCString location;
    nsCOMPtr<nsIURI> locationURI;

    bool TryParseLocationURI(LocationHint aType, nsIURI** aURI);
};

bool IsUniversalXPConnectEnabled(JSCompartment *compartment);
bool IsUniversalXPConnectEnabled(JSContext *cx);
bool EnableUniversalXPConnect(JSContext *cx);

inline void
CrashIfNotInAutomation()
{
    const char *prefName =
      "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer";
    MOZ_RELEASE_ASSERT(mozilla::Preferences::GetBool(prefName));
}

inline XPCWrappedNativeScope*
ObjectScope(JSObject *obj)
{
    return CompartmentPrivate::Get(obj)->scope;
}

extern const JSClass SafeJSContextGlobalClass;

JSObject* NewOutObject(JSContext* cx, JSObject* scope);
bool IsOutObject(JSContext* cx, JSObject* obj);

nsresult HasInstance(JSContext *cx, JS::HandleObject objArg, const nsID *iid, bool *bp);

/**
 * Define quick stubs on the given object, @a proto.
 *
 * @param cx
 *     A context.  Requires request.
 * @param proto
 *     The (newly created) prototype object for a DOM class.  The JS half
 *     of an XPCWrappedNativeProto.
 * @param flags
 *     Property flags for the quick stub properties--should be either
 *     JSPROP_ENUMERATE or 0.
 * @param interfaceCount
 *     The number of interfaces the class implements.
 * @param interfaceArray
 *     The interfaces the class implements; interfaceArray and
 *     interfaceCount are like what nsIClassInfo.getInterfaces returns.
 */
bool
DOM_DefineQuickStubs(JSContext *cx, JSObject *proto, uint32_t flags,
                     uint32_t interfaceCount, const nsIID **interfaceArray);

nsIPrincipal *GetObjectPrincipal(JSObject *obj);

} // namespace xpc

namespace mozilla {
namespace dom {
extern bool
DefineStaticJSVals(JSContext *cx);
} // namespace dom
} // namespace mozilla

bool
xpc_LocalizeRuntime(JSRuntime *rt);
void
xpc_DelocalizeRuntime(JSRuntime *rt);

/***************************************************************************/
// Inlines use the above - include last.

#include "XPCInlines.h"

/***************************************************************************/
// Maps have inlines that use the above - include last.

#include "XPCMaps.h"

/***************************************************************************/

#endif /* xpcprivate_h___ */
