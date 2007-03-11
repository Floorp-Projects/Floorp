/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Nate Nielsen <nielsen@memberwebs.com>
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

/* High level class and public functions implementation. */

#include "xpcprivate.h"
#include "XPCNativeWrapper.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "jsobj.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPConnect,nsIXPConnect,nsISupportsWeakReference)

nsXPConnect* nsXPConnect::gSelf = nsnull;
JSBool       nsXPConnect::gOnceAliveNowDead = JS_FALSE;

const char XPC_CONTEXT_STACK_CONTRACTID[] = "@mozilla.org/js/xpc/ContextStack;1";
const char XPC_RUNTIME_CONTRACTID[]       = "@mozilla.org/js/xpc/RuntimeService;1";
const char XPC_EXCEPTION_CONTRACTID[]     = "@mozilla.org/js/xpc/Exception;1";
const char XPC_CONSOLE_CONTRACTID[]       = "@mozilla.org/consoleservice;1";
const char XPC_SCRIPT_ERROR_CONTRACTID[]  = "@mozilla.org/scripterror;1";
const char XPC_ID_CONTRACTID[]            = "@mozilla.org/js/xpc/ID;1";
const char XPC_XPCONNECT_CONTRACTID[]     = "@mozilla.org/js/xpc/XPConnect;1";

/***************************************************************************/

nsXPConnect::nsXPConnect()
    :   mRuntime(nsnull),
        mInterfaceInfoManager(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID)),
        mContextStack(nsnull),
        mDefaultSecurityManager(nsnull),
        mDefaultSecurityManagerFlags(0),
        mShuttingDown(JS_FALSE),
        mObjRefcounts(nsnull)
{
    // Ignore the result. If the runtime service is not ready to rumble
    // then we'll set this up later as needed.
    CreateRuntime();

    CallGetService(XPC_CONTEXT_STACK_CONTRACTID, &mContextStack);

    nsCycleCollector_registerRuntime(nsIProgrammingLanguage::JAVASCRIPT, this);

#ifdef XPC_TOOLS_SUPPORT
  {
    char* filename = PR_GetEnv("MOZILLA_JS_PROFILER_OUTPUT");
    if(filename && *filename)
    {
        mProfilerOutputFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
        if(mProfilerOutputFile &&
           NS_SUCCEEDED(mProfilerOutputFile->InitWithNativePath(nsDependentCString(filename))))
        {
            mProfiler = do_GetService(XPCTOOLS_PROFILER_CONTRACTID);
            if(mProfiler)
            {
                if(NS_SUCCEEDED(mProfiler->Start()))
                {
#ifdef DEBUG
                    printf("***** profiling JavaScript. Output to: %s\n",
                           filename);
#endif
                }
            }
        }
    }
  }
#endif

}

typedef nsBaseHashtable<nsClearingVoidPtrHashKey, PRUint32, PRUint32> PointerSet;
#ifndef XPCONNECT_STANDALONE
typedef nsBaseHashtable<nsClearingVoidPtrHashKey, nsISupports*, nsISupports*> ScopeSet;
#endif

struct JSObjectRefcounts 
{
    PointerSet mRefCounts;
#ifndef XPCONNECT_STANDALONE
    ScopeSet mScopes;
#endif
    PRBool mMarkEnded;
    JSObjectRefcounts() : mMarkEnded(PR_FALSE)
    {
        mRefCounts.Init();
#ifndef XPCONNECT_STANDALONE
        mScopes.Init();
#endif
    }

    void Clear()
    {
        mRefCounts.Clear();
#ifndef XPCONNECT_STANDALONE
        mScopes.Clear();
#endif
    }

    void MarkStart()
    {
        Clear();
        mMarkEnded = PR_FALSE;
    }

    void MarkEnd()
    {
        mMarkEnded = PR_TRUE;
    }

    void Ref(void *obj, uint8 flags)
    {
        PRUint32 refs;
        if (mRefCounts.Get(obj, &refs))
            refs++;
        else
            refs = 1;
        mRefCounts.Put(obj, refs);
    }

    PRBool Get(void *obj, PRUint32 &refcount)
    {
        return mRefCounts.Get(obj, &refcount);
    }
};

nsXPConnect::~nsXPConnect()
{
    // XXX It would be nice if we could get away with doing a GC here and also
    // calling Release on the natives no longer reachable via XPConnect. As
    // noted all over the place, this makes bad things happen since shutdown is
    // an unstable time for so many modules who have not planned well for it.

    nsCycleCollector_forgetRuntime(nsIProgrammingLanguage::JAVASCRIPT);
    if (mObjRefcounts)
    {
        delete mObjRefcounts;
        mObjRefcounts = NULL;
    }

    mShuttingDown = JS_TRUE;
    { // scoped callcontext
        XPCCallContext ccx(NATIVE_CALLER);
        if(ccx.IsValid())
        {
            XPCWrappedNativeScope::SystemIsBeingShutDown(ccx);
            if(mRuntime)
                mRuntime->SystemIsBeingShutDown(&ccx);
                
        }
    }

    NS_IF_RELEASE(mContextStack);
    NS_IF_RELEASE(mDefaultSecurityManager);

    // Unfortunately calling CleanupAllThreads before the stuff above
    // (esp. SystemIsBeingShutDown) causes too many bad things to happen
    // as the Release calls propagate. See the comment in this function in
    // revision 1.35 of this file.
    //
    // I filed a bug on xpcom regarding the bad things that happen
    // if people try to create components during shutdown.
    // http://bugzilla.mozilla.org/show_bug.cgi?id=37058
    //
    // Also, we just plain need the context stack for at least the current
    // thread to be in place. Unfortunately, this will leak stuff on the
    // stacks' safeJSContexts. But, this is a shutdown leak only.

    XPCPerThreadData::CleanupAllThreads();

    // shutdown the logging system
    XPC_LOG_FINISH();

    delete mRuntime;

    gSelf = nsnull;
    gOnceAliveNowDead = JS_TRUE;
}

// static
nsXPConnect*
nsXPConnect::GetXPConnect()
{
    if(!gSelf)
    {
        if(gOnceAliveNowDead)
            return nsnull;
        gSelf = new nsXPConnect();
        if(!gSelf)
            return nsnull;

        if(!gSelf->mInterfaceInfoManager ||
           !gSelf->mContextStack)
        {
            // ctor failed to create an acceptable instance
            delete gSelf;
            gSelf = nsnull;
        }
        else
        {
            // Initial extra ref to keep the singleton alive
            // balanced by explicit call to ReleaseXPConnectSingleton()
            NS_ADDREF(gSelf);
        }
    }
    return gSelf;
}

// In order to enable this jsgc heap dumping you need to compile
// _both_ js/src/jsgc.c and this file with 'GC_MARK_DEBUG' #defined.
// Normally this is done by adding -DGC_MARK_DEBUG to the appropriate
// defines lists in the makefiles.

#ifdef GC_MARK_DEBUG
extern "C" JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
#endif

// static
nsXPConnect*
nsXPConnect::GetSingleton()
{
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    NS_IF_ADDREF(xpc);
    return xpc;
}

// static
void
nsXPConnect::ReleaseXPConnectSingleton()
{
    nsXPConnect* xpc = gSelf;
    if(xpc)
    {

#ifdef XPC_TOOLS_SUPPORT
        if(xpc->mProfiler)
        {
            xpc->mProfiler->Stop();
            xpc->mProfiler->WriteResults(xpc->mProfilerOutputFile);
        }
#endif

#ifdef GC_MARK_DEBUG
        // force a dump of the JavaScript gc heap if JS is still alive
        XPCCallContext ccx(NATIVE_CALLER);
        if(ccx.IsValid())
        {
            FILE* oldFileHandle = js_DumpGCHeap;
            js_DumpGCHeap = stdout;
            JS_GC(ccx);
            js_DumpGCHeap = oldFileHandle;
        }
#endif
#ifdef XPC_DUMP_AT_SHUTDOWN
        // NOTE: to see really interesting stuff turn on the prlog stuff.
        // See the comment at the top of xpclog.h to see how to do that.
        xpc->DebugDump(7);
#endif
        nsrefcnt cnt;
        NS_RELEASE2(xpc, cnt);
#ifdef XPC_DUMP_AT_SHUTDOWN
        if(0 != cnt)
            printf("*** dangling reference to nsXPConnect: refcnt=%d\n", cnt);
        else
            printf("+++ XPConnect had no dangling references.\n");
#endif
    }
}

// static
nsresult
nsXPConnect::GetInterfaceInfoManager(nsIInterfaceInfoSuperManager** iim,
                                     nsXPConnect* xpc /*= nsnull*/)
{
    if(!xpc && !(xpc = GetXPConnect()))
        return NS_ERROR_FAILURE;

    *iim = xpc->mInterfaceInfoManager;
    NS_IF_ADDREF(*iim);
    return NS_OK;
}

// static
nsresult
nsXPConnect::GetContextStack(nsIThreadJSContextStack** stack,
                             nsXPConnect* xpc /*= nsnull*/)
{
    nsIThreadJSContextStack* temp;

    if(!xpc && !(xpc = GetXPConnect()))
        return NS_ERROR_FAILURE;

    *stack = temp = xpc->mContextStack;
    NS_IF_ADDREF(temp);
    return NS_OK;
}

// static
XPCJSRuntime*
nsXPConnect::GetRuntime(nsXPConnect* xpc /*= nsnull*/)
{
    if(!xpc && !(xpc = GetXPConnect()))
        return nsnull;

    return xpc->EnsureRuntime() ? xpc->mRuntime : nsnull;
}

// static 
nsIJSRuntimeService* 
nsXPConnect::GetJSRuntimeService(nsXPConnect* xpc /* = nsnull */)
{
    XPCJSRuntime* rt = GetRuntime(xpc); 
    return rt ? rt->GetJSRuntimeService() : nsnull;
}

// static
XPCContext*
nsXPConnect::GetContext(JSContext* cx, nsXPConnect* xpc /*= nsnull*/)
{
    NS_PRECONDITION(cx,"bad param");

    XPCJSRuntime* rt = GetRuntime(xpc);
    if(!rt)
        return nsnull;

    if(rt->GetJSRuntime() != JS_GetRuntime(cx))
    {
        NS_WARNING("XPConnect was passed aJSContext from a foreign JSRuntime!");
        return nsnull;
    }
    return rt->GetXPCContext(cx);
}

// static
JSBool
nsXPConnect::IsISupportsDescendant(nsIInterfaceInfo* info)
{
    PRBool found = PR_FALSE;
    if(info)
        info->HasAncestor(&NS_GET_IID(nsISupports), &found);
    return found;
}

JSBool
nsXPConnect::CreateRuntime()
{
    NS_ASSERTION(!mRuntime,"CreateRuntime called but mRuntime already init'd");
    nsresult rv;
    nsCOMPtr<nsIJSRuntimeService> rtsvc = 
             do_GetService(XPC_RUNTIME_CONTRACTID, &rv);
    if(NS_SUCCEEDED(rv) && rtsvc)
    {
        mRuntime = XPCJSRuntime::newXPCJSRuntime(this, rtsvc);
    }
    return nsnull != mRuntime;
}

/***************************************************************************/

typedef PRBool (*InfoTester)(nsIInterfaceInfoManager* manager, const void* data,
                             nsIInterfaceInfo** info);

static PRBool IIDTester(nsIInterfaceInfoManager* manager, const void* data,
                        nsIInterfaceInfo** info)
{
    return NS_SUCCEEDED(manager->GetInfoForIID((const nsIID *) data, info)) &&
           *info;
}

static PRBool NameTester(nsIInterfaceInfoManager* manager, const void* data,
                      nsIInterfaceInfo** info)
{
    return NS_SUCCEEDED(manager->GetInfoForName((const char *) data, info)) &&
           *info;
}

static nsresult FindInfo(InfoTester tester, const void* data, 
                         nsIInterfaceInfoSuperManager* iism,
                         nsIInterfaceInfo** info)
{
    if(tester(iism, data, info))
        return NS_OK;
    
    // If not found, then let's ask additional managers.

    PRBool yes;
    nsCOMPtr<nsISimpleEnumerator> list;

    if(NS_SUCCEEDED(iism->HasAdditionalManagers(&yes)) && yes &&
       NS_SUCCEEDED(iism->EnumerateAdditionalManagers(getter_AddRefs(list))) &&
       list)
    {
        PRBool more;
        nsCOMPtr<nsIInterfaceInfoManager> current;

        while(NS_SUCCEEDED(list->HasMoreElements(&more)) && more &&
              NS_SUCCEEDED(list->GetNext(getter_AddRefs(current))) && current)
        {
            if(tester(current, data, info))
                return NS_OK;
        }
    }
    
    return NS_ERROR_NO_INTERFACE;
}    

nsresult
nsXPConnect::GetInfoForIID(const nsIID * aIID, nsIInterfaceInfo** info)
{
    return FindInfo(IIDTester, aIID, mInterfaceInfoManager, info);
}

nsresult
nsXPConnect::GetInfoForName(const char * name, nsIInterfaceInfo** info)
{
    return FindInfo(NameTester, name, mInterfaceInfoManager, info);
}

static JSGCCallback gOldJSGCCallback;

JS_STATIC_DLL_CALLBACK(JSBool)
XPCCycleGCCallback(JSContext *cx, JSGCStatus status)
{
    // Chain to old GCCallback first, we want to get all the mark notifications
    // before recording the end of the mark phase.
    JSBool ok = gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;

    // Record the end of a mark phase. If we get more mark notifications then
    // the GC has restarted and we'll need to clear the refcounts first.
    if(status == JSGC_MARK_END)
        nsXPConnect::GetXPConnect()->GetJSObjectRefcounts()->MarkEnd();

    return ok;
}

void XPCMarkNotification(void *thing, uint8 flags, void *closure)
{
    uint8 ty = flags & GCF_TYPEMASK;
    if (ty != GCX_OBJECT &&
        ty != GCX_PRIVATE && 
        ty != GCX_XML)
        return;

    JSObjectRefcounts* jsr = NS_STATIC_CAST(JSObjectRefcounts*, closure);
    // We're marking after a mark phase ended, so the GC restarted itself and
    // we want to clear the refcounts first.
    // XXX With GC_MARK_DEBUG defined we end up too many times in
    //     XPCMarkNotification, even while taking JSGC_MARK_END into account.
    if(jsr->mMarkEnded)
        jsr->MarkStart();
    jsr->Ref(thing, flags);
}

nsresult 
nsXPConnect::BeginCycleCollection()
{
    if (!mObjRefcounts)
        mObjRefcounts = new JSObjectRefcounts;

    mObjRefcounts->MarkStart();
    XPCCallContext cx(NATIVE_CALLER);
    if (cx.IsValid()) {
        gOldJSGCCallback = JS_SetGCCallback(cx, XPCCycleGCCallback);
        JS_SetGCThingCallback(cx, XPCMarkNotification, mObjRefcounts);
        JS_GC(cx);
        JS_SetGCThingCallback(cx, nsnull, nsnull);
        JS_SetGCCallback(cx, gOldJSGCCallback);
        gOldJSGCCallback = nsnull;

#ifndef XPCONNECT_STANDALONE
        XPCWrappedNativeScope::TraverseScopes(cx);
#endif
    }

    return NS_OK;
}

#ifndef XPCONNECT_STANDALONE
void
nsXPConnect::RecordTraversal(void *p, nsISupports *s)
{
    mObjRefcounts->mScopes.Put(p, s);
}
#endif

nsresult 
nsXPConnect::FinishCycleCollection()
{
    if (mObjRefcounts)
        mObjRefcounts->Clear();
    return NS_OK;
}

nsresult 
nsXPConnect::Root(const nsDeque &nodes)
{
    XPCCallContext cx(NATIVE_CALLER);
    for (PRInt32 i = 0; i < nodes.GetSize(); ++i)
    {
        void *p = nodes.ObjectAt(i);
        if (!p)
            continue;
        JSObject *obj = NS_STATIC_CAST(JSObject*, p);
        if (!JS_LockGCThing(cx, obj))
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult 
nsXPConnect::Unlink(const nsDeque &nodes)
{
    XPCCallContext cx(NATIVE_CALLER);
    
    for (PRInt32 i = 0; i < nodes.GetSize(); ++i)
    {
        void *p = nodes.ObjectAt(i);
        if (!p)
            continue;
        JSObject *obj = NS_STATIC_CAST(JSObject*, p);
        JS_ClearScope(cx, obj);
    }
    return NS_OK;
}

nsresult 
nsXPConnect::Unroot(const nsDeque &nodes)
{
    XPCCallContext cx(NATIVE_CALLER);
    for (PRInt32 i = 0; i < nodes.GetSize(); ++i)
    {
        void *p = nodes.ObjectAt(i);
        if (!p)
            continue;
        JSObject *obj = NS_STATIC_CAST(JSObject*, p);
        if (!JS_UnlockGCThing(cx, obj))
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult 
nsXPConnect::Traverse(void *p, nsCycleCollectionTraversalCallback &cb)
{
    XPCCallContext cx(NATIVE_CALLER);

    PRUint32 refcount = 0;
    if (!mObjRefcounts->Get(p, refcount))
        return NS_OK;

    JSObject *obj = NS_STATIC_CAST(JSObject*, p);
    JSClass* clazz = OBJ_GET_CLASS(cx, obj);

#ifdef DEBUG
    char name[72];
    if(XPCNativeWrapper::IsNativeWrapperClass(clazz))
    {
        XPCWrappedNative* wn = XPCNativeWrapper::GetWrappedNative(cx, obj);
        XPCNativeScriptableInfo* si = wn ? wn->GetScriptableInfo() : nsnull;
        if(si)
            JS_snprintf(name, sizeof(name), "XPCNativeWrapper (%s)",
                        si->GetJSClass()->name);
        else
            JS_snprintf(name, sizeof(name), "XPCNativeWrapper");
    }
    else if(obj)
    {
        XPCNativeScriptableInfo* si = nsnull;
        if(IS_PROTO_CLASS(clazz))
        {
            XPCWrappedNativeProto* p =
                (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
            si = p->GetScriptableInfo();
        }
        if(si)
            JS_snprintf(name, sizeof(name), "JS Object (%s - %s)", clazz->name,
                        si->GetJSClass()->name);
        else
            JS_snprintf(name, sizeof(name), "JS Object (%s)", clazz->name);
    }
    else
    {
        JS_snprintf(name, sizeof(name), "JS Object");
    }
    cb.DescribeNode(refcount, sizeof(JSObject), name);
#else
    cb.DescribeNode(refcount, sizeof(JSObject), "JS Object");
#endif
    if (!p)
        return NS_OK;    

    if(XPCNativeWrapper::IsNativeWrapperClass(clazz))
    {
        // XPCNativeWrapper keeps its wrapped native alive (see XPC_NW_Mark).
        XPCWrappedNative* wn = XPCNativeWrapper::GetWrappedNative(cx, obj);
        if(wn)
            cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                               wn->GetFlatJSObject());
    }
    else if(clazz == &XPC_WN_Tearoff_JSClass)
    {
        // A tearoff holds a strong reference to its native object
        // (see XPCWrappedNative::FlatJSObjectFinalized). Its XPCWrappedNative
        // will be held alive through the parent of the JSObject of the tearoff.
        XPCWrappedNativeTearOff *to =
            (XPCWrappedNativeTearOff*) JS_GetPrivate(cx, obj);
        cb.NoteXPCOMChild(to->GetNative());
    }
    else if(IS_PROTO_CLASS(clazz))
    {
        // See XPC_WN_Shared_Proto_Mark.
        XPCWrappedNativeProto* p =
            (XPCWrappedNativeProto*) JS_GetPrivate(cx, obj);
        if(p)
            // Mark scope.
            p->GetScope()->Traverse(cb);
    }
    else if(clazz->flags & JSCLASS_HAS_PRIVATE &&
            clazz->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)
    {
        cb.NoteXPCOMChild(NS_STATIC_CAST(nsISupports*, JS_GetPrivate(cx, obj)));
    }

    cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                       OBJ_GET_PARENT(cx, obj));
    cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                       OBJ_GET_PROTO(cx, obj));

    for(uint32 i = JSSLOT_START(clazz); i < STOBJ_NSLOTS(obj); ++i) 
    {
        jsval val = STOBJ_GET_SLOT(obj, i);
        if (JSVAL_IS_OBJECT(val)) 
            cb.NoteScriptChild(nsIProgrammingLanguage::JAVASCRIPT,
                               JSVAL_TO_OBJECT(val));
    }

#ifndef XPCONNECT_STANDALONE
    if(clazz->flags & JSCLASS_IS_GLOBAL)
    {
        nsISupports *principal = nsnull;
        mObjRefcounts->mScopes.Get(obj, &principal);
        cb.NoteXPCOMChild(principal);
    }
#endif

    return NS_OK;
}


/***************************************************************************/
/***************************************************************************/
// nsIXPConnect interface methods...

inline nsresult UnexpectedFailure(nsresult rv)
{
    NS_ERROR("This is not supposed to fail!");
    return rv;
}

/* void initClasses (in JSContextPtr aJSContext, in JSObjectPtr aGlobalJSObj); */
NS_IMETHODIMP
nsXPConnect::InitClasses(JSContext * aJSContext, JSObject * aGlobalJSObj)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aGlobalJSObj, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(!xpc_InitJSxIDClassObjects())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(!xpc_InitWrappedNativeJSOps())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::GetNewOrUsed(ccx, aGlobalJSObj);

    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    scope->RemoveWrappedNativeProtos();

    if(!nsXPCComponents::AttachNewComponentsObject(ccx, scope, aGlobalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

#ifdef XPC_IDISPATCH_SUPPORT
    // Initialize any properties IDispatch needs on the global object
    XPCIDispatchExtension::Initialize(ccx, aGlobalJSObj);
#endif

    if (!XPCNativeWrapper::AttachNewConstructorObject(ccx, aGlobalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if (!XPC_SJOW_AttachNewConstructorObject(ccx, aGlobalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    return NS_OK;
}

JS_STATIC_DLL_CALLBACK(JSBool)
TempGlobalResolve(JSContext *aJSContext, JSObject *obj, jsval id)
{
    JSBool resolved;
    return JS_ResolveStandardClass(aJSContext, obj, id, &resolved);
}

static JSClass xpcTempGlobalClass = {
    "xpcTempGlobalClass", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, TempGlobalResolve, JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* nsIXPConnectJSObjectHolder initClassesWithNewWrappedGlobal (in JSContextPtr aJSContext, in nsISupports aCOMObj, in nsIIDRef aIID, in PRUint32 aFlags); */
NS_IMETHODIMP
nsXPConnect::InitClassesWithNewWrappedGlobal(JSContext * aJSContext,
                                             nsISupports *aCOMObj,
                                             const nsIID & aIID,
                                             PRUint32 aFlags,
                                             nsIXPConnectJSObjectHolder **_retval)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aCOMObj, "bad param");
    NS_ASSERTION(_retval, "bad param");

    // XXX This is not pretty. We make a temporary global object and
    // init it with all the Components object junk just so we have a
    // parent with an xpc scope to use when wrapping the object that will
    // become the 'real' global.

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);

    JSObject* tempGlobal = JS_NewObject(aJSContext, &xpcTempGlobalClass,
                                        nsnull, nsnull);

    if(!tempGlobal ||
       !JS_SetParent(aJSContext, tempGlobal, nsnull) ||
       !JS_SetPrototype(aJSContext, tempGlobal, nsnull))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(aFlags & nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT)
        JS_FlagSystemObject(aJSContext, tempGlobal);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    {
        // Scope for our auto-marker; it just needs to keep tempGlobal alive
        // long enough for InitClasses and WrapNative to do their work
        AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(tempGlobal));

        if(NS_FAILED(InitClasses(aJSContext, tempGlobal)))
            return UnexpectedFailure(NS_ERROR_FAILURE);

        nsresult rv;
        if(!XPCConvert::NativeInterface2JSObject(ccx, getter_AddRefs(holder),
                                                 aCOMObj, &aIID, tempGlobal,
                                                 PR_FALSE, OBJ_IS_GLOBAL, &rv))
            return UnexpectedFailure(rv);

        NS_ASSERTION(NS_SUCCEEDED(rv) && holder, "Didn't wrap properly");
    }

    JSObject* globalJSObj;
    if(NS_FAILED(holder->GetJSObject(&globalJSObj)) || !globalJSObj)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(aFlags & nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT)
        NS_ASSERTION(JS_IsSystemObject(aJSContext, globalJSObj), "huh?!");

    // voodoo to fixup scoping and parenting...

    JS_SetParent(aJSContext, globalJSObj, nsnull);

    JSObject* oldGlobal = JS_GetGlobalObject(aJSContext);
    if(!oldGlobal || oldGlobal == tempGlobal)
        JS_SetGlobalObject(aJSContext, globalJSObj);

    if((aFlags & nsIXPConnect::INIT_JS_STANDARD_CLASSES) &&
       !JS_InitStandardClasses(aJSContext, globalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNative* wrapper =
        NS_REINTERPRET_CAST(XPCWrappedNative*, holder.get());
    XPCWrappedNativeScope* scope = wrapper->GetScope();

    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    NS_ASSERTION(scope->GetGlobalJSObject() == tempGlobal, "stealing scope!");

    scope->SetGlobal(ccx, globalJSObj);

    JSObject* protoJSObject = wrapper->HasProto() ?
                                    wrapper->GetProto()->GetJSProtoObject() :
                                    globalJSObj;
    if(protoJSObject)
    {
        if(protoJSObject != globalJSObj)
            JS_SetParent(aJSContext, protoJSObject, globalJSObj);
        JS_SetPrototype(aJSContext, protoJSObject, scope->GetPrototypeJSObject());
    }

    if(!nsXPCComponents::AttachNewComponentsObject(ccx, scope, globalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if (!XPCNativeWrapper::AttachNewConstructorObject(ccx, globalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if (!XPC_SJOW_AttachNewConstructorObject(ccx, globalJSObj))
        return UnexpectedFailure(NS_ERROR_FAILURE);

    NS_ADDREF(*_retval = holder);

    return NS_OK;
}

/* nsIXPConnectJSObjectHolder wrapNative (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsISupports aCOMObj, in nsIIDRef aIID); */
NS_IMETHODIMP
nsXPConnect::WrapNative(JSContext * aJSContext,
                        JSObject * aScope,
                        nsISupports *aCOMObj,
                        const nsIID & aIID,
                        nsIXPConnectJSObjectHolder **_retval)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aScope, "bad param");
    NS_ASSERTION(aCOMObj, "bad param");
    NS_ASSERTION(_retval, "bad param");

    *_retval = nsnull;

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsresult rv;
    if(!XPCConvert::NativeInterface2JSObject(ccx, _retval, aCOMObj, &aIID,
                                             aScope, PR_FALSE,
                                             OBJ_IS_NOT_GLOBAL, &rv))
        return rv;

#ifdef DEBUG
    JSObject* returnObj;
    (*_retval)->GetJSObject(&returnObj);
    NS_ASSERTION(!XPCNativeWrapper::IsNativeWrapper(aJSContext, returnObj),
                 "Shouldn't be returning a native wrapper here");
#endif
    
    return NS_OK;
}

/* void wrapJS (in JSContextPtr aJSContext, in JSObjectPtr aJSObj, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP
nsXPConnect::WrapJS(JSContext * aJSContext,
                    JSObject * aJSObj,
                    const nsIID & aIID,
                    void * *result)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aJSObj, "bad param");
    NS_ASSERTION(result, "bad param");

    *result = nsnull;

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsresult rv;
    if(!XPCConvert::JSObject2NativeInterface(ccx, result, aJSObj,
                                             &aIID, nsnull, &rv))
        return rv;
    return NS_OK;
}

/* void wrapJSAggregatedToNative (in nsISupports aOuter, in JSContextPtr aJSContext, in JSObjectPtr aJSObj, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP
nsXPConnect::WrapJSAggregatedToNative(nsISupports *aOuter,
                                      JSContext * aJSContext,
                                      JSObject * aJSObj,
                                      const nsIID & aIID,
                                      void * *result)
{
    NS_ASSERTION(aOuter, "bad param");
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aJSObj, "bad param");
    NS_ASSERTION(result, "bad param");

    *result = nsnull;

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsresult rv;
    if(!XPCConvert::JSObject2NativeInterface(ccx, result, aJSObj,
                                             &aIID, aOuter, &rv))
        return rv;
    return NS_OK;
}

/* nsIXPConnectWrappedNative getWrappedNativeOfJSObject (in JSContextPtr aJSContext, in JSObjectPtr aJSObj); */
NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfJSObject(JSContext * aJSContext,
                                        JSObject * aJSObj,
                                        nsIXPConnectWrappedNative **_retval)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aJSObj, "bad param");
    NS_ASSERTION(_retval, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsIXPConnectWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(aJSContext, aJSObj);
    if(wrapper)
    {
        NS_ADDREF(wrapper);
        *_retval = wrapper;
        return NS_OK;
    }
    // else...
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

/* nsIXPConnectWrappedNative getWrappedNativeOfNativeObject (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsISupports aCOMObj, in nsIIDRef aIID); */
NS_IMETHODIMP
nsXPConnect::GetWrappedNativeOfNativeObject(JSContext * aJSContext,
                                            JSObject * aScope,
                                            nsISupports *aCOMObj,
                                            const nsIID & aIID,
                                            nsIXPConnectWrappedNative **_retval)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aScope, "bad param");
    NS_ASSERTION(aCOMObj, "bad param");
    NS_ASSERTION(_retval, "bad param");

    *_retval = nsnull;

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aScope);
    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    AutoMarkingNativeInterfacePtr iface(ccx);
    iface = XPCNativeInterface::GetNewOrUsed(ccx, &aIID);
    if(!iface)
        return NS_ERROR_FAILURE;

    XPCWrappedNative* wrapper;

    nsresult rv = XPCWrappedNative::GetUsedOnly(ccx, aCOMObj, scope, iface,
                                                &wrapper);
    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    *_retval = NS_STATIC_CAST(nsIXPConnectWrappedNative*, wrapper);
    return NS_OK;
}

/* nsIXPConnectJSObjectHolder reparentWrappedNativeIfFound (in JSContextPtr aJSContext, in JSObjectPtr aScope, in JSObjectPtr aNewParent, in nsISupports aCOMObj); */
NS_IMETHODIMP
nsXPConnect::ReparentWrappedNativeIfFound(JSContext * aJSContext,
                                          JSObject * aScope,
                                          JSObject * aNewParent,
                                          nsISupports *aCOMObj,
                                          nsIXPConnectJSObjectHolder **_retval)
{
    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aScope);
    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope* scope2 =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aNewParent);
    if(!scope2)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    return XPCWrappedNative::
        ReparentWrapperIfFound(ccx, scope, scope2, aNewParent, aCOMObj,
                               (XPCWrappedNative**) _retval);
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
MoveableWrapperFinder(JSDHashTable *table, JSDHashEntryHdr *hdr,
                      uint32 number, void *arg)
{
    // Every element counts.
    nsVoidArray *va = NS_STATIC_CAST(nsVoidArray *,arg);
    va->AppendElement(((Native2WrappedNativeMap::Entry*)hdr)->value);
    return JS_DHASH_NEXT;
}

/* void reparentScopeAwareWrappers(in JSContextPtr aJSContext, in JSObjectPtr  aOldScope, in JSObjectPtr  aNewScope); */
NS_IMETHODIMP
nsXPConnect::ReparentScopeAwareWrappers(JSContext *aJSContext,
                                        JSObject *aOldScope,
                                        JSObject *aNewScope)
{
    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope *oldScope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aOldScope);
    if(!oldScope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope *newScope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aNewScope);
    if(!newScope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    {   // scoped lock
        XPCAutoLock lock(oldScope->GetRuntime()->GetMapLock());

        // First, look through the old scope and find all of the wrappers that
        // we're going to move.

        Native2WrappedNativeMap *map = oldScope->GetWrappedNativeMap();
        nsVoidArray wrappersToMove(map->Count());
        map->Enumerate(MoveableWrapperFinder, &wrappersToMove);

        // Now that we have the wrappers, reparent them to the new scope.
        for(PRInt32 i = 0, stop = wrappersToMove.Count(); i < stop; ++i)
        {
            // First, check to see if this wrapper really needs to be
            // reparented.

            XPCWrappedNative *wrapper =
                NS_STATIC_CAST(XPCWrappedNative *, wrappersToMove[i]);
            nsISupports *identity = wrapper->GetIdentityObject();
            nsCOMPtr<nsIClassInfo> info(do_QueryInterface(identity));

            // ClassInfo is implemented as singleton objects. If the identity
            // object here is the same object as returned by the QI, then it
            // is the singleton classinfo, so we don't need to reparent it.
            if(SameCOMIdentity(identity, info))
                info = nsnull;

            if(!info)
                continue;

            XPCNativeScriptableCreateInfo sciProto;
            XPCNativeScriptableCreateInfo sciWrapper;

            nsresult rv =
                XPCWrappedNative::GatherScriptableCreateInfo(identity,
                                                             info.get(),
                                                             &sciProto,
                                                             &sciWrapper);
            if(NS_FAILED(rv))
                return NS_ERROR_FAILURE;

            // If the wrapper doesn't want precreate, then we don't need to
            // worry about reparenting it.
            if(!sciWrapper.GetFlags().WantPreCreate())
                continue;

            JSObject *newParent = aOldScope;
            rv = sciWrapper.GetCallback()->PreCreate(identity, ccx, aOldScope,
                                                     &newParent);
            if(NS_FAILED(rv))
                return rv;

            if(newParent != aOldScope)
            {
                // The wrapper returned a new parent. If the new parent is in
                // a different scope, then we need to reparent it, otherwise,
                // the old scope is fine.

                XPCWrappedNativeScope *betterScope =
                    XPCWrappedNativeScope::FindInJSObjectScope(ccx, newParent);
                if(betterScope == oldScope)
                    continue;

                NS_ASSERTION(betterScope == newScope, "Weird scope returned");
            }
            else
            {
                // The old scope still works for this wrapper.
                continue;
            }

            // Now, reparent the wrapper, since we know that it wants to be
            // reparented.

            nsRefPtr<XPCWrappedNative> junk;
            rv = XPCWrappedNative::ReparentWrapperIfFound(ccx, oldScope,
                                                          newScope, newParent,
                                                          wrapper->GetIdentityObject(),
                                                          getter_AddRefs(junk));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

/* void setSecurityManagerForJSContext (in JSContextPtr aJSContext, in nsIXPCSecurityManager aManager, in PRUint16 flags); */
NS_IMETHODIMP
nsXPConnect::SetSecurityManagerForJSContext(JSContext * aJSContext,
                                            nsIXPCSecurityManager *aManager,
                                            PRUint16 flags)
{
    NS_ASSERTION(aJSContext, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCContext* xpcc = ccx.GetXPCContext();

    NS_IF_ADDREF(aManager);
    nsIXPCSecurityManager* oldManager = xpcc->GetSecurityManager();
    NS_IF_RELEASE(oldManager);

    xpcc->SetSecurityManager(aManager);
    xpcc->SetSecurityManagerFlags(flags);
    return NS_OK;
}

/* void getSecurityManagerForJSContext (in JSContextPtr aJSContext, out nsIXPCSecurityManager aManager, out PRUint16 flags); */
NS_IMETHODIMP
nsXPConnect::GetSecurityManagerForJSContext(JSContext * aJSContext,
                                            nsIXPCSecurityManager **aManager,
                                            PRUint16 *flags)
{
    NS_ASSERTION(aJSContext, "bad param");
    NS_ASSERTION(aManager, "bad param");
    NS_ASSERTION(flags, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCContext* xpcc = ccx.GetXPCContext();

    nsIXPCSecurityManager* manager = xpcc->GetSecurityManager();
    NS_IF_ADDREF(manager);
    *aManager = manager;
    *flags = xpcc->GetSecurityManagerFlags();
    return NS_OK;
}

/* void setDefaultSecurityManager (in nsIXPCSecurityManager aManager, in PRUint16 flags); */
NS_IMETHODIMP
nsXPConnect::SetDefaultSecurityManager(nsIXPCSecurityManager *aManager,
                                       PRUint16 flags)
{
    NS_IF_ADDREF(aManager);
    NS_IF_RELEASE(mDefaultSecurityManager);
    mDefaultSecurityManager = aManager;
    mDefaultSecurityManagerFlags = flags;
    return NS_OK;
}

/* void getDefaultSecurityManager (out nsIXPCSecurityManager aManager, out PRUint16 flags); */
NS_IMETHODIMP
nsXPConnect::GetDefaultSecurityManager(nsIXPCSecurityManager **aManager,
                                       PRUint16 *flags)
{
    NS_ASSERTION(aManager, "bad param");
    NS_ASSERTION(flags, "bad param");

    NS_IF_ADDREF(mDefaultSecurityManager);
    *aManager = mDefaultSecurityManager;
    *flags = mDefaultSecurityManagerFlags;
    return NS_OK;
}

/* nsIStackFrame createStackFrameLocation (in PRUint32 aLanguage, in string aFilename, in string aFunctionName, in PRInt32 aLineNumber, in nsIStackFrame aCaller); */
NS_IMETHODIMP
nsXPConnect::CreateStackFrameLocation(PRUint32 aLanguage,
                                      const char *aFilename,
                                      const char *aFunctionName,
                                      PRInt32 aLineNumber,
                                      nsIStackFrame *aCaller,
                                      nsIStackFrame **_retval)
{
    NS_ASSERTION(_retval, "bad param");

    return XPCJSStack::CreateStackFrameLocation(aLanguage,
                                                aFilename,
                                                aFunctionName,
                                                aLineNumber,
                                                aCaller,
                                                _retval);
}

/* readonly attribute nsIStackFrame CurrentJSStack; */
NS_IMETHODIMP
nsXPConnect::GetCurrentJSStack(nsIStackFrame * *aCurrentJSStack)
{
    NS_ASSERTION(aCurrentJSStack, "bad param");
    *aCurrentJSStack = nsnull;

    JSContext* cx;
    // is there a current context available?
    if(mContextStack && NS_SUCCEEDED(mContextStack->Peek(&cx)) && cx)
    {
        nsCOMPtr<nsIStackFrame> stack;
        XPCJSStack::CreateStack(cx, getter_AddRefs(stack));
        if(stack)
        {
            // peel off native frames...
            PRUint32 language;
            nsCOMPtr<nsIStackFrame> caller;
            while(stack &&
                  NS_SUCCEEDED(stack->GetLanguage(&language)) &&
                  language != nsIProgrammingLanguage::JAVASCRIPT &&
                  NS_SUCCEEDED(stack->GetCaller(getter_AddRefs(caller))) &&
                  caller)
            {
                stack = caller;
            }
            NS_IF_ADDREF(*aCurrentJSStack = stack);
        }
    }
    return NS_OK;
}

/* readonly attribute nsIXPCNativeCallContext CurrentNativeCallContext; */
NS_IMETHODIMP
nsXPConnect::GetCurrentNativeCallContext(nsIXPCNativeCallContext * *aCurrentNativeCallContext)
{
    NS_ASSERTION(aCurrentNativeCallContext, "bad param");

    XPCPerThreadData* data = XPCPerThreadData::GetData();
    if(data)
    {
        nsIXPCNativeCallContext* temp = data->GetCallContext();
        NS_IF_ADDREF(temp);
        *aCurrentNativeCallContext = temp;
        return NS_OK;
    }
    //else...
    *aCurrentNativeCallContext = nsnull;
    return UnexpectedFailure(NS_ERROR_FAILURE);
}

/* attribute nsIException PendingException; */
NS_IMETHODIMP
nsXPConnect::GetPendingException(nsIException * *aPendingException)
{
    NS_ASSERTION(aPendingException, "bad param");

    XPCPerThreadData* data = XPCPerThreadData::GetData();
    if(!data)
    {
        *aPendingException = nsnull;
        return UnexpectedFailure(NS_ERROR_FAILURE);
    }

    return data->GetException(aPendingException);
}

NS_IMETHODIMP
nsXPConnect::SetPendingException(nsIException * aPendingException)
{
    XPCPerThreadData* data = XPCPerThreadData::GetData();
    if(!data)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    data->SetException(aPendingException);
    return NS_OK;
}

/* void syncJSContexts (); */
NS_IMETHODIMP
nsXPConnect::SyncJSContexts(void)
{
    XPCJSRuntime* rt = GetRuntime(this);
    if(rt)
        rt->SyncXPCContextList();
    return NS_OK;
}

/* nsIXPCFunctionThisTranslator setFunctionThisTranslator (in nsIIDRef aIID, in nsIXPCFunctionThisTranslator aTranslator); */
NS_IMETHODIMP
nsXPConnect::SetFunctionThisTranslator(const nsIID & aIID,
                                       nsIXPCFunctionThisTranslator *aTranslator,
                                       nsIXPCFunctionThisTranslator **_retval)
{
    XPCJSRuntime* rt = GetRuntime(this);
    if(!rt)
        return NS_ERROR_UNEXPECTED;

    nsIXPCFunctionThisTranslator* old;
    IID2ThisTranslatorMap* map = rt->GetThisTranslatorMap();

    {
        XPCAutoLock lock(rt->GetMapLock()); // scoped lock
        if(_retval)
        {
            old = map->Find(aIID);
            NS_IF_ADDREF(old);
            *_retval = old;
        }
        map->Add(aIID, aTranslator);
    }
    return NS_OK;
}

/* nsIXPCFunctionThisTranslator getFunctionThisTranslator (in nsIIDRef aIID); */
NS_IMETHODIMP
nsXPConnect::GetFunctionThisTranslator(const nsIID & aIID,
                                       nsIXPCFunctionThisTranslator **_retval)
{
    XPCJSRuntime* rt = GetRuntime(this);
    if(!rt)
        return NS_ERROR_UNEXPECTED;

    nsIXPCFunctionThisTranslator* old;
    IID2ThisTranslatorMap* map = rt->GetThisTranslatorMap();

    {
        XPCAutoLock lock(rt->GetMapLock()); // scoped lock
        old = map->Find(aIID);
        NS_IF_ADDREF(old);
        *_retval = old;
    }
    return NS_OK;
}

/* void setSafeJSContextForCurrentThread (in JSContextPtr cx); */
NS_IMETHODIMP 
nsXPConnect::SetSafeJSContextForCurrentThread(JSContext * cx)
{
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);
    return ccx.GetThreadData()->GetJSContextStack()->SetSafeJSContext(cx);
}

/* void clearAllWrappedNativeSecurityPolicies (); */
NS_IMETHODIMP
nsXPConnect::ClearAllWrappedNativeSecurityPolicies()
{
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    return XPCWrappedNativeScope::ClearAllWrappedNativeSecurityPolicies(ccx);
}

/* void restoreWrappedNativePrototype (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsIClassInfo aClassInfo, in nsIXPConnectJSObjectHolder aPrototype); */
NS_IMETHODIMP 
nsXPConnect::RestoreWrappedNativePrototype(JSContext * aJSContext, 
                                           JSObject * aScope, 
                                           nsIClassInfo * aClassInfo, 
                                           nsIXPConnectJSObjectHolder * aPrototype)
{
    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(!aClassInfo || !aPrototype)
        return UnexpectedFailure(NS_ERROR_INVALID_ARG);

    JSObject *protoJSObject;
    nsresult rv = aPrototype->GetJSObject(&protoJSObject);
    if(NS_FAILED(rv))
        return UnexpectedFailure(rv);

    if(!IS_PROTO_CLASS(JS_GET_CLASS(ccx, protoJSObject)))
        return UnexpectedFailure(NS_ERROR_INVALID_ARG);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aScope);
    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeProto *proto =
        (XPCWrappedNativeProto*)JS_GetPrivate(ccx, protoJSObject);
    if(!proto)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    if(scope != proto->GetScope())
    {
        NS_ERROR("Attempt to reset prototype to a prototype from a"
                 "different scope!");

        return UnexpectedFailure(NS_ERROR_INVALID_ARG);
    }

    XPCNativeScriptableInfo *si = proto->GetScriptableInfo();

    if(si && si->GetFlags().DontSharePrototype())
        return UnexpectedFailure(NS_ERROR_INVALID_ARG);

    ClassInfo2WrappedNativeProtoMap* map = scope->GetWrappedNativeProtoMap();
    XPCLock* lock = scope->GetRuntime()->GetMapLock();

    {   // scoped lock
        XPCAutoLock al(lock);

        XPCWrappedNativeProtoMap* detachedMap =
            GetRuntime()->GetDetachedWrappedNativeProtoMap();

        // If we're replacing an old proto, make sure to put it on the
        // map of detached wrapped native protos so that the old proto
        // gets properly cleaned up, especially during shutdown.
        XPCWrappedNativeProto *oldProto = map->Find(aClassInfo);
        if(oldProto)
        {
            detachedMap->Add(oldProto);

            // ClassInfo2WrappedNativeProtoMap doesn't ever replace
            // entries in the map, so now since we know there's an
            // entry for aClassInfo in the map we have to remove it to
            // be able to add the new one.
            map->Remove(aClassInfo);

            // This code should do the right thing even if we're
            // restoring the current proto, but warn in that case
            // since doing that is pointless.
            NS_ASSERTION(proto != oldProto,
                         "Restoring current prototype, fix caller!");
        }

        map->Add(aClassInfo, proto);

        // Remove the prototype from the map of detached wrapped
        // native prototypes now that the prototype is part of a scope
        // again.
        detachedMap->Remove(proto);
    }

    // The global in this scope didn't change, but a prototype did
    // (most likely the global object's prototype), which means the
    // scope needs to get a chance to update its cached
    // Object.prototype pointers etc.
    scope->SetGlobal(ccx, aScope);

    return NS_OK;
}

NS_IMETHODIMP
nsXPConnect::CreateSandbox(JSContext *cx, nsIPrincipal *principal,
                           nsIXPConnectJSObjectHolder **_retval)
{
#ifdef XPCONNECT_STANDALONE
    return NS_ERROR_NOT_AVAILABLE;
#else /* XPCONNECT_STANDALONE */
    XPCCallContext ccx(NATIVE_CALLER, cx);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    *_retval = nsnull;

    jsval rval = JSVAL_VOID;
    AUTO_MARK_JSVAL(ccx, &rval);

    nsresult rv = xpc_CreateSandboxObject(cx, &rval, principal);
    NS_ASSERTION(NS_FAILED(rv) || !JSVAL_IS_PRIMITIVE(rval),
                 "Bad return value from xpc_CreateSandboxObject()!");

    if (NS_SUCCEEDED(rv) && !JSVAL_IS_PRIMITIVE(rval)) {
        *_retval = XPCJSObjectHolder::newHolder(cx, JSVAL_TO_OBJECT(rval));
        NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

        NS_ADDREF(*_retval);
    }

    return rv;
#endif /* XPCONNECT_STANDALONE */
}

NS_IMETHODIMP
nsXPConnect::EvalInSandboxObject(const nsAString& source, JSContext *cx,
                                 nsIXPConnectJSObjectHolder *sandbox,
                                 jsval *rval)
{
#ifdef XPCONNECT_STANDALONE
    return NS_ERROR_NOT_AVAILABLE;
#else /* XPCONNECT_STANDALONE */
    if (!sandbox)
        return NS_ERROR_INVALID_ARG;

    JSObject *obj;
    nsresult rv = sandbox->GetJSObject(&obj);
    NS_ENSURE_SUCCESS(rv, rv);

    return xpc_EvalInSandbox(cx, obj, source,
                             NS_ConvertUTF16toUTF8(source).get(), 1, rval);
#endif /* XPCONNECT_STANDALONE */
}

/* nsIXPConnectJSObjectHolder getWrappedNativePrototype (in JSContextPtr aJSContext, in JSObjectPtr aScope, in nsIClassInfo aClassInfo); */
NS_IMETHODIMP 
nsXPConnect::GetWrappedNativePrototype(JSContext * aJSContext, 
                                       JSObject * aScope, 
                                       nsIClassInfo *aClassInfo, 
                                       nsIXPConnectJSObjectHolder **_retval)
{
    XPCCallContext ccx(NATIVE_CALLER, aJSContext);
    if(!ccx.IsValid())
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, aScope);
    if(!scope)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    XPCNativeScriptableCreateInfo sciProto;
    XPCWrappedNative::GatherProtoScriptableCreateInfo(aClassInfo, &sciProto);

    AutoMarkingWrappedNativeProtoPtr proto(ccx);
    proto = XPCWrappedNativeProto::GetNewOrUsed(ccx, scope, aClassInfo, 
                                                &sciProto, JS_FALSE,
                                                OBJ_IS_NOT_GLOBAL);
    if(!proto)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    nsIXPConnectJSObjectHolder* holder;
    *_retval = holder = XPCJSObjectHolder::newHolder(ccx, 
                                                     proto->GetJSProtoObject());
    if(!holder)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    NS_ADDREF(holder);
    return NS_OK;
}

/* attribute PRBool collectGarbageOnMainThreadOnly; */
NS_IMETHODIMP 
nsXPConnect::GetCollectGarbageOnMainThreadOnly(PRBool *aCollectGarbageOnMainThreadOnly)
{
    XPCJSRuntime* rt = GetRuntime();
    if(!rt)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    *aCollectGarbageOnMainThreadOnly = rt->GetMainThreadOnlyGC();
    return NS_OK;
}

NS_IMETHODIMP 
nsXPConnect::SetCollectGarbageOnMainThreadOnly(PRBool aCollectGarbageOnMainThreadOnly)
{
    XPCJSRuntime* rt = GetRuntime();
    if(!rt)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    rt->SetMainThreadOnlyGC(aCollectGarbageOnMainThreadOnly);
    return NS_OK;
}

/* attribute PRBool deferReleasesUntilAfterGarbageCollection; */
NS_IMETHODIMP 
nsXPConnect::GetDeferReleasesUntilAfterGarbageCollection(PRBool *aDeferReleasesUntilAfterGarbageCollection)
{
    XPCJSRuntime* rt = GetRuntime();
    if(!rt)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    *aDeferReleasesUntilAfterGarbageCollection = rt->GetDeferReleases();
    return NS_OK;
}

NS_IMETHODIMP 
nsXPConnect::SetDeferReleasesUntilAfterGarbageCollection(PRBool aDeferReleasesUntilAfterGarbageCollection)
{
    XPCJSRuntime* rt = GetRuntime();
    if(!rt)
        return UnexpectedFailure(NS_ERROR_FAILURE);

    rt->SetDeferReleases(aDeferReleasesUntilAfterGarbageCollection);
    return NS_OK;
}

/* void releaseJSContext (in JSContextPtr aJSContext, in PRBool noGC); */
NS_IMETHODIMP 
nsXPConnect::ReleaseJSContext(JSContext * aJSContext, PRBool noGC)
{
    NS_ASSERTION(aJSContext, "bad param");
    XPCPerThreadData* tls = XPCPerThreadData::GetData();
    if(tls)
    {
        XPCCallContext* ccx = nsnull;
        for(XPCCallContext* cur = tls->GetCallContext(); 
            cur; 
            cur = cur->GetPrevCallContext())
        {
            if(cur->GetJSContext() == aJSContext)
            {
                ccx = cur;
                // Keep looping to find the deepest matching call context.
            }
        }
    
        if(ccx)
        {
#ifdef DEBUG_xpc_hacker
            printf("!xpc - deferring destruction of JSContext @ %0x\n", 
                   aJSContext);
#endif
            ccx->SetDestroyJSContextInDestructor(JS_TRUE);
            JS_ClearNewbornRoots(aJSContext);
            return NS_OK;
        }
        // else continue on and synchronously destroy the JSContext ...

        NS_ASSERTION(!tls->GetJSContextStack() || 
                     !tls->GetJSContextStack()->
                        DEBUG_StackHasJSContext(aJSContext),
                     "JSContext still in threadjscontextstack!");
    }
    
    if(noGC)
        JS_DestroyContextNoGC(aJSContext);
    else
        JS_DestroyContext(aJSContext);
    SyncJSContexts();
    return NS_OK;
}

/* void debugDump (in short depth); */
NS_IMETHODIMP
nsXPConnect::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPConnect @ %x with mRefCnt = %d", this, mRefCnt.get()));
    XPC_LOG_INDENT();
        XPC_LOG_ALWAYS(("gSelf @ %x", gSelf));
        XPC_LOG_ALWAYS(("gOnceAliveNowDead is %d", (int)gOnceAliveNowDead));
        XPC_LOG_ALWAYS(("mDefaultSecurityManager @ %x", mDefaultSecurityManager));
        XPC_LOG_ALWAYS(("mDefaultSecurityManagerFlags of %x", mDefaultSecurityManagerFlags));
        XPC_LOG_ALWAYS(("mInterfaceInfoManager @ %x", mInterfaceInfoManager.get()));
        XPC_LOG_ALWAYS(("mContextStack @ %x", mContextStack));
        if(mRuntime)
        {
            if(depth)
                mRuntime->DebugDump(depth);
            else
                XPC_LOG_ALWAYS(("XPCJSRuntime @ %x", mRuntime));
        }
        else
            XPC_LOG_ALWAYS(("mRuntime is null"));
        XPCWrappedNativeScope::DebugDumpAllScopes(depth);
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}

/* void debugDumpObject (in nsISupports aCOMObj, in short depth); */
NS_IMETHODIMP
nsXPConnect::DebugDumpObject(nsISupports *p, PRInt16 depth)
{
#ifdef DEBUG
    if(!depth)
        return NS_OK;
    if(!p)
    {
        XPC_LOG_ALWAYS(("*** Cound not dump object with NULL address"));
        return NS_OK;
    }

    nsIXPConnect* xpc;
    nsIXPCWrappedJSClass* wjsc;
    nsIXPConnectWrappedNative* wn;
    nsIXPConnectWrappedJS* wjs;

    if(NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnect),
                        (void**)&xpc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnect..."));
        xpc->DebugDump(depth);
        NS_RELEASE(xpc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPCWrappedJSClass),
                        (void**)&wjsc)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPCWrappedJSClass..."));
        wjsc->DebugDump(depth);
        NS_RELEASE(wjsc);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnectWrappedNative),
                        (void**)&wn)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedNative..."));
        wn->DebugDump(depth);
        NS_RELEASE(wn);
    }
    else if(NS_SUCCEEDED(p->QueryInterface(NS_GET_IID(nsIXPConnectWrappedJS),
                        (void**)&wjs)))
    {
        XPC_LOG_ALWAYS(("Dumping a nsIXPConnectWrappedJS..."));
        wjs->DebugDump(depth);
        NS_RELEASE(wjs);
    }
    else
        XPC_LOG_ALWAYS(("*** Could not dump the nsISupports @ %x", p));
#endif
    return NS_OK;
}

/* void debugDumpJSStack (in PRBool showArgs, in PRBool showLocals, in PRBool showThisProps); */
NS_IMETHODIMP
nsXPConnect::DebugDumpJSStack(PRBool showArgs,
                              PRBool showLocals,
                              PRBool showThisProps)
{
#ifdef DEBUG
    JSContext* cx;
    nsresult rv;
    nsCOMPtr<nsIThreadJSContextStack> stack = 
             do_GetService(XPC_CONTEXT_STACK_CONTRACTID, &rv);
    if(NS_FAILED(rv) || !stack)
        printf("failed to get nsIThreadJSContextStack service!\n");
    else if(NS_FAILED(stack->Peek(&cx)))
        printf("failed to peek into nsIThreadJSContextStack service!\n");
    else if(!cx)
        printf("there is no JSContext on the nsIThreadJSContextStack!\n");
    else
        xpc_DumpJSStack(cx, showArgs, showLocals, showThisProps);
#endif
    return NS_OK;
}

/* void debugDumpEvalInJSStackFrame (in PRUint32 aFrameNumber, in string aSourceText); */
NS_IMETHODIMP
nsXPConnect::DebugDumpEvalInJSStackFrame(PRUint32 aFrameNumber, const char *aSourceText)
{
#ifdef DEBUG
    JSContext* cx;
    nsresult rv;
    nsCOMPtr<nsIThreadJSContextStack> stack = 
             do_GetService(XPC_CONTEXT_STACK_CONTRACTID, &rv);
    if(NS_FAILED(rv) || !stack)
        printf("failed to get nsIThreadJSContextStack service!\n");
    else if(NS_FAILED(stack->Peek(&cx)))
        printf("failed to peek into nsIThreadJSContextStack service!\n");
    else if(!cx)
        printf("there is no JSContext on the nsIThreadJSContextStack!\n");
    else
        xpc_DumpEvalInJSStackFrame(cx, aFrameNumber, aSourceText);
#endif
    return NS_OK;
}

/* JSVal variantToJS (in JSContextPtr ctx, in JSObjectPtr scope, in nsIVariant value); */
NS_IMETHODIMP 
nsXPConnect::VariantToJS(JSContext* ctx, JSObject* scope, nsIVariant* value, jsval* _retval)
{
    NS_PRECONDITION(ctx, "bad param");
    NS_PRECONDITION(scope, "bad param");
    NS_PRECONDITION(value, "bad param");
    NS_PRECONDITION(_retval, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, ctx);
    if(!ccx.IsValid())
        return NS_ERROR_FAILURE;

    nsresult rv = NS_OK;
    if(!XPCVariant::VariantDataToJS(ccx, value, scope, &rv, _retval))
    {
        if(NS_FAILED(rv)) 
            return rv;

        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/* nsIVariant JSToVariant (in JSContextPtr ctx, in JSVal value); */
NS_IMETHODIMP 
nsXPConnect::JSToVariant(JSContext* ctx, jsval value, nsIVariant** _retval)
{
    NS_PRECONDITION(ctx, "bad param");
    NS_PRECONDITION(value, "bad param");
    NS_PRECONDITION(_retval, "bad param");

    XPCCallContext ccx(NATIVE_CALLER, ctx);
    if(!ccx.IsValid())
        return NS_ERROR_FAILURE;

    *_retval = XPCVariant::newVariant(ccx, value);
    if(!(*_retval)) 
        return NS_ERROR_FAILURE;

    return NS_OK;
}

/* void flagSystemFilenamePrefix (in string filenamePrefix); */
NS_IMETHODIMP 
nsXPConnect::FlagSystemFilenamePrefix(const char *aFilenamePrefix)
{
    NS_PRECONDITION(aFilenamePrefix, "bad param");

    nsIJSRuntimeService* rtsvc = nsXPConnect::GetJSRuntimeService();
    if(!rtsvc)
        return NS_ERROR_NOT_INITIALIZED;

    JSRuntime* rt;
    nsresult rv = rtsvc->GetRuntime(&rt);
    if(NS_FAILED(rv))
        return rv;

    if(!JS_FlagScriptFilenamePrefix(rt, aFilenamePrefix, JSFILENAME_SYSTEM))
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

#ifdef DEBUG
/* These are here to be callable from a debugger */
JS_BEGIN_EXTERN_C
void DumpJSStack()
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_SUCCEEDED(rv) && xpc)
        xpc->DebugDumpJSStack(PR_TRUE, PR_TRUE, PR_FALSE);
    else
        printf("failed to get XPConnect service!\n");
}

void DumpJSEval(PRUint32 frameno, const char* text)
{
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_SUCCEEDED(rv) && xpc)
        xpc->DebugDumpEvalInJSStackFrame(frameno, text);
    else
        printf("failed to get XPConnect service!\n");
}

void DumpJSObject(JSObject* obj)
{
    xpc_DumpJSObject(obj);
}

void DumpJSValue(jsval val)
{
    printf("Dumping 0x%lx. Value tag is %lu.\n", val, JSVAL_TAG(val));
    if(JSVAL_IS_NULL(val)) {
        printf("Value is null\n");
    }
    else if(JSVAL_IS_OBJECT(val)) {
        printf("Value is an object\n");
        JSObject* obj = JSVAL_TO_OBJECT(val);
        DumpJSObject(obj);
    }
    else if(JSVAL_IS_NUMBER(val)) {
        printf("Value is a number: ");
        if(JSVAL_IS_INT(val))
          printf("Integer %i\n", JSVAL_TO_INT(val));
        else if(JSVAL_IS_DOUBLE(val))
          printf("Floating-point value %f\n", *JSVAL_TO_DOUBLE(val));
    }
    else if(JSVAL_IS_STRING(val)) {
        printf("Value is a string: ");
        JSString* string = JSVAL_TO_STRING(val);
        char* bytes = JS_GetStringBytes(string);
        printf("<%s>\n", bytes);
    }
    else if(JSVAL_IS_BOOLEAN(val)) {
        printf("Value is boolean: ");
        printf(JSVAL_TO_BOOLEAN(val) ? "true" : "false");
    }
    else if(JSVAL_IS_VOID(val)) {
        printf("Value is undefined\n");
    }
    else {
        printf("No idea what this value is.\n");
    }
}
JS_END_EXTERN_C
#endif
