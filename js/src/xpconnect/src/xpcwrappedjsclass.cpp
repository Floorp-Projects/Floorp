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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* Sharable code and data for wrapper around JSObjects. */

#include "xpcprivate.h"
#include "nsArrayEnumerator.h"
#include "nsWrapperCache.h"
#include "XPCWrapper.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCWrappedJSClass, nsIXPCWrappedJSClass)

// the value of this variable is never used - we use its address as a sentinel
static uint32 zero_methods_descriptor;

void AutoScriptEvaluate::StartEvaluating(JSErrorReporter errorReporter)
{
    NS_PRECONDITION(!mEvaluated, "AutoScriptEvaluate::Evaluate should only be called once");

    if(!mJSContext)
        return;
    mEvaluated = PR_TRUE;
    if(!mJSContext->errorReporter)
    {
        JS_SetErrorReporter(mJSContext, errorReporter);
        mErrorReporterSet = PR_TRUE;
    }
    mContextHasThread = JS_GetContextThread(mJSContext);
    if (mContextHasThread)
        JS_BeginRequest(mJSContext);

    // Saving the exception state keeps us from interfering with another script
    // that may also be running on this context.  This occurred first with the
    // js debugger, as described in
    // http://bugzilla.mozilla.org/show_bug.cgi?id=88130 but presumably could
    // show up in any situation where a script calls into a wrapped js component
    // on the same context, while the context has a nonzero exception state.
    // Because JS_SaveExceptionState/JS_RestoreExceptionState use malloc
    // and addroot, we avoid them if possible by returning null (as opposed to
    // a JSExceptionState with no information) when there is no pending
    // exception.
    if(JS_IsExceptionPending(mJSContext))
    {
        mState = JS_SaveExceptionState(mJSContext);
        JS_ClearPendingException(mJSContext);
    }
}

AutoScriptEvaluate::~AutoScriptEvaluate()
{
    if(!mJSContext || !mEvaluated)
        return;
    if(mState)
        JS_RestoreExceptionState(mJSContext, mState);
    else
        JS_ClearPendingException(mJSContext);

    if(mContextHasThread)
        JS_EndRequest(mJSContext);

    // If this is a JSContext that has a private context that provides a
    // nsIXPCScriptNotify interface, then notify the object the script has
    // been executed.
    //
    // Note: We rely on the rule that if any JSContext in our JSRuntime has
    // private data that points to an nsISupports subclass, it has also set
    // the JSOPTION_PRIVATE_IS_NSISUPPORTS option.

    if (JS_GetOptions(mJSContext) & JSOPTION_PRIVATE_IS_NSISUPPORTS)
    {
        nsCOMPtr<nsIXPCScriptNotify> scriptNotify = 
            do_QueryInterface(static_cast<nsISupports*>
                                         (JS_GetContextPrivate(mJSContext)));
        if(scriptNotify)
            scriptNotify->ScriptExecuted();
    }

    if(mErrorReporterSet)
        JS_SetErrorReporter(mJSContext, NULL);
}

// It turns out that some errors may be not worth reporting. So, this
// function is factored out to manage that.
JSBool xpc_IsReportableErrorCode(nsresult code)
{
    if (NS_SUCCEEDED(code))
        return JS_FALSE;

    switch(code)
    {
        // Error codes that we don't want to report as errors...
        // These generally indicate bad interface design AFAIC. 
        case NS_ERROR_FACTORY_REGISTER_AGAIN:
        case NS_BASE_STREAM_WOULD_BLOCK:
            return JS_FALSE;
    }

    return JS_TRUE;
}

// static
nsresult
nsXPCWrappedJSClass::GetNewOrUsed(XPCCallContext& ccx, REFNSIID aIID,
                                  nsXPCWrappedJSClass** resultClazz)
{
    nsXPCWrappedJSClass* clazz = nsnull;
    XPCJSRuntime* rt = ccx.GetRuntime();

    {   // scoped lock
        XPCAutoLock lock(rt->GetMapLock());
        IID2WrappedJSClassMap* map = rt->GetWrappedJSClassMap();
        clazz = map->Find(aIID);
        NS_IF_ADDREF(clazz);
    }

    if(!clazz)
    {
        nsCOMPtr<nsIInterfaceInfo> info;
        ccx.GetXPConnect()->GetInfoForIID(&aIID, getter_AddRefs(info));
        if(info)
        {
            PRBool canScript;
            if(NS_SUCCEEDED(info->IsScriptable(&canScript)) && canScript &&
               nsXPConnect::IsISupportsDescendant(info))
            {
                clazz = new nsXPCWrappedJSClass(ccx, aIID, info);
                if(clazz && !clazz->mDescriptors)
                    NS_RELEASE(clazz);  // sets clazz to nsnull
            }
        }
    }
    *resultClazz = clazz;
    return NS_OK;
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(XPCCallContext& ccx, REFNSIID aIID,
                                         nsIInterfaceInfo* aInfo)
    : mRuntime(ccx.GetRuntime()),
      mInfo(aInfo),
      mName(nsnull),
      mIID(aIID),
      mDescriptors(nsnull)
{
    NS_ADDREF(mInfo);
    NS_ADDREF_THIS();

    {   // scoped lock
        XPCAutoLock lock(mRuntime->GetMapLock());
        mRuntime->GetWrappedJSClassMap()->Add(this);
    }

    uint16 methodCount;
    if(NS_SUCCEEDED(mInfo->GetMethodCount(&methodCount)))
    {
        if(methodCount)
        {
            int wordCount = (methodCount/32)+1;
            if(nsnull != (mDescriptors = new uint32[wordCount]))
            {
                int i;
                // init flags to 0;
                for(i = wordCount-1; i >= 0; i--)
                    mDescriptors[i] = 0;

                for(i = 0; i < methodCount; i++)
                {
                    const nsXPTMethodInfo* info;
                    if(NS_SUCCEEDED(mInfo->GetMethodInfo(i, &info)))
                        SetReflectable(i, XPCConvert::IsMethodReflectable(*info));
                    else
                    {
                        delete [] mDescriptors;
                        mDescriptors = nsnull;
                        break;
                    }
                }
            }
        }
        else
        {
            mDescriptors = &zero_methods_descriptor;
        }
    }
}

nsXPCWrappedJSClass::~nsXPCWrappedJSClass()
{
    if(mDescriptors && mDescriptors != &zero_methods_descriptor)
        delete [] mDescriptors;
    if(mRuntime)
    {   // scoped lock
        XPCAutoLock lock(mRuntime->GetMapLock());
        mRuntime->GetWrappedJSClassMap()->Remove(this);
    }
    if(mName)
        nsMemory::Free(mName);
    NS_IF_RELEASE(mInfo);
}

JSObject*
nsXPCWrappedJSClass::CallQueryInterfaceOnJSObject(XPCCallContext& ccx,
                                                  JSObject* jsobj,
                                                  REFNSIID aIID)
{
    JSContext* cx = ccx.GetJSContext();
    JSObject* id;
    jsval retval;
    JSObject* retObj;
    JSBool success = JS_FALSE;
    jsid funid;
    jsval fun;

    // Don't call the actual function on a content object. We'll determine
    // whether or not a content object is capable of implementing the
    // interface (i.e. whether the interface is scriptable) and most content
    // objects don't have QI implementations anyway. Also see bug 503926.
    if(XPCPerThreadData::IsMainThread(ccx) &&
       !JS_GetGlobalForObject(ccx, jsobj)->isSystem())
    {
        nsCOMPtr<nsIPrincipal> objprin;
        nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
        if(ssm)
        {
            nsresult rv = ssm->GetObjectPrincipal(ccx, jsobj, getter_AddRefs(objprin));
            NS_ENSURE_SUCCESS(rv, nsnull);

            PRBool isSystem;
            rv = ssm->IsSystemPrincipal(objprin, &isSystem);
            NS_ENSURE_SUCCESS(rv, nsnull);

            if(!isSystem)
                return nsnull;
        }
    }

    // check upfront for the existence of the function property
    funid = mRuntime->GetStringID(XPCJSRuntime::IDX_QUERY_INTERFACE);
    if(!JS_GetPropertyById(cx, jsobj, funid, &fun) || JSVAL_IS_PRIMITIVE(fun))
        return nsnull;

    // protect fun so that we're sure it's alive when we call it
    AUTO_MARK_JSVAL(ccx, fun);

    // Ensure that we are asking for a scriptable interface.
    // NB:  It's important for security that this check is here rather
    // than later, since it prevents untrusted objects from implementing
    // some interfaces in JS and aggregating a trusted object to
    // implement intentionally (for security) unscriptable interfaces.
    // We so often ask for nsISupports that we can short-circuit the test...
    if(!aIID.Equals(NS_GET_IID(nsISupports)))
    {
        nsCOMPtr<nsIInterfaceInfo> info;
        ccx.GetXPConnect()->GetInfoForIID(&aIID, getter_AddRefs(info));
        if(!info)
            return nsnull;
        PRBool canScript;
        if(NS_FAILED(info->IsScriptable(&canScript)) || !canScript)
            return nsnull;
    }

    // OK, it looks like we'll be calling into JS code.

    AutoScriptEvaluate scriptEval(cx);

    // XXX we should install an error reporter that will send reports to
    // the JS error console service.
    scriptEval.StartEvaluating();

    id = xpc_NewIDObject(cx, jsobj, aIID);
    if(id)
    {
        // Throwing NS_NOINTERFACE is the prescribed way to fail QI from JS. It
        // is not an exception that is ever worth reporting, but we don't want
        // to eat all exceptions either.

        uint32 oldOpts =
          JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_DONT_REPORT_UNCAUGHT);

        jsval args[1] = {OBJECT_TO_JSVAL(id)};
        success = JS_CallFunctionValue(cx, jsobj, fun, 1, args, &retval);

        JS_SetOptions(cx, oldOpts);

        if(!success)
        {
            NS_ASSERTION(JS_IsExceptionPending(cx),
                         "JS failed without setting an exception!");

            jsval jsexception = JSVAL_NULL;
            AUTO_MARK_JSVAL(ccx, &jsexception);

            if(JS_GetPendingException(cx, &jsexception))
            {
                nsresult rv;
                if(JSVAL_IS_OBJECT(jsexception))
                {
                    // XPConnect may have constructed an object to represent a
                    // C++ QI failure. See if that is the case.
                    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;

                    nsXPConnect::GetXPConnect()->
                        GetWrappedNativeOfJSObject(ccx,
                                                   JSVAL_TO_OBJECT(jsexception),
                                                   getter_AddRefs(wrapper));

                    if(wrapper)
                    {
                        nsCOMPtr<nsIException> exception =
                            do_QueryWrappedNative(wrapper);
                        if(exception &&
                           NS_SUCCEEDED(exception->GetResult(&rv)) &&
                           rv == NS_NOINTERFACE)
                        {
                            JS_ClearPendingException(cx);
                        }
                    }
                }
                else if(JSVAL_IS_NUMBER(jsexception))
                {
                    // JS often throws an nsresult.
                    if(JSVAL_IS_DOUBLE(jsexception))
                        rv = (nsresult)(*JSVAL_TO_DOUBLE(jsexception));
                    else
                        rv = (nsresult)(JSVAL_TO_INT(jsexception));

                    if(rv == NS_NOINTERFACE)
                        JS_ClearPendingException(cx);
                }
            }

            // Don't report if reporting was disabled by someone else.
            if(!(oldOpts & JSOPTION_DONT_REPORT_UNCAUGHT))
                JS_ReportPendingException(cx);
        }
    }

    if(success)
        success = JS_ValueToObject(cx, retval, &retObj);

    return success ? retObj : nsnull;
}

/***************************************************************************/

static JSBool 
GetNamedPropertyAsVariantRaw(XPCCallContext& ccx, 
                             JSObject* aJSObj,
                             jsid aName, 
                             nsIVariant** aResult,
                             nsresult* pErr)
{
    nsXPTType type = nsXPTType((uint8)(TD_INTERFACE_TYPE | XPT_TDP_POINTER));
    jsval val;

    return JS_GetPropertyById(ccx, aJSObj, aName, &val) &&
           XPCConvert::JSData2Native(ccx, aResult, val, type, JS_FALSE, 
                                     &NS_GET_IID(nsIVariant), pErr);
}

// static
nsresult
nsXPCWrappedJSClass::GetNamedPropertyAsVariant(XPCCallContext& ccx, 
                                               JSObject* aJSObj,
                                               jsval aName, 
                                               nsIVariant** aResult)
{
    JSContext* cx = ccx.GetJSContext();
    JSBool ok;
    jsid id;
    nsresult rv = NS_ERROR_FAILURE;

    AutoScriptEvaluate scriptEval(cx);
    scriptEval.StartEvaluating();

    ok = JS_ValueToId(cx, aName, &id) && 
         GetNamedPropertyAsVariantRaw(ccx, aJSObj, id, aResult, &rv);

    return ok ? NS_OK : NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
}

/***************************************************************************/

// static 
nsresult 
nsXPCWrappedJSClass::BuildPropertyEnumerator(XPCCallContext& ccx,
                                             JSObject* aJSObj,
                                             nsISimpleEnumerator** aEnumerate)
{
    JSContext* cx = ccx.GetJSContext();
    nsresult retval = NS_ERROR_FAILURE;
    JSIdArray* idArray = nsnull;
    int i;

    // Saved state must be restored, all exits through 'out'...
    AutoScriptEvaluate scriptEval(cx);
    scriptEval.StartEvaluating();

    idArray = JS_Enumerate(cx, aJSObj);
    if(!idArray)
        return retval;
    
    nsCOMArray<nsIProperty> propertyArray(idArray->length);
    for(i = 0; i < idArray->length; i++)
    {
        nsCOMPtr<nsIVariant> value;
        jsid idName = idArray->vector[i];
        nsresult rv;

        if(!GetNamedPropertyAsVariantRaw(ccx, aJSObj, idName, 
                                         getter_AddRefs(value), &rv))
        {
            if(NS_FAILED(rv))
                retval = rv;                                        
            goto out;
        }

        jsval jsvalName;
        if(!JS_IdToValue(cx, idName, &jsvalName))
            goto out;

        JSString* name = JS_ValueToString(cx, jsvalName);
        if(!name)
            goto out;

        nsCOMPtr<nsIProperty> property = 
            new xpcProperty((const PRUnichar*) JS_GetStringChars(name), 
                            (PRUint32) JS_GetStringLength(name),
                            value);
        if(!property)
            goto out;

        if(!propertyArray.AppendObject(property))
            goto out;
    }

    retval = NS_NewArrayEnumerator(aEnumerate, propertyArray);

out:
    JS_DestroyIdArray(cx, idArray);

    return retval;
}

/***************************************************************************/

NS_IMPL_ISUPPORTS1(xpcProperty, nsIProperty)

xpcProperty::xpcProperty(const PRUnichar* aName, PRUint32 aNameLen, 
                         nsIVariant* aValue)
    : mName(aName, aNameLen), mValue(aValue)
{
}

/* readonly attribute AString name; */
NS_IMETHODIMP xpcProperty::GetName(nsAString & aName)
{
    aName.Assign(mName);
    return NS_OK;
}

/* readonly attribute nsIVariant value; */
NS_IMETHODIMP xpcProperty::GetValue(nsIVariant * *aValue)
{
    NS_ADDREF(*aValue = mValue);
    return NS_OK;
}

/***************************************************************************/
// This 'WrappedJSIdentity' class and singleton allow us to figure out if
// any given nsISupports* is implemented by a WrappedJS object. This is done
// using a QueryInterface call on the interface pointer with our ID. If
// that call returns NS_OK and the pointer is to our singleton, then the
// interface must be implemented by a WrappedJS object. NOTE: the
// 'WrappedJSIdentity' object is not a real XPCOM object and should not be
// used for anything else (hence it is declared in this implementation file).

// {5C5C3BB0-A9BA-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID \
{ 0x5c5c3bb0, 0xa9ba, 0x11d2,                       \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class WrappedJSIdentity
{
    // no instance methods...
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

    static void* GetSingleton()
    {
        static WrappedJSIdentity* singleton = nsnull;
        if(!singleton)
            singleton = new WrappedJSIdentity();
        return (void*) singleton;
    }
};

NS_DEFINE_STATIC_IID_ACCESSOR(WrappedJSIdentity,
                              NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

/***************************************************************************/

// static
JSBool
nsXPCWrappedJSClass::IsWrappedJS(nsISupports* aPtr)
{
    void* result;
    NS_PRECONDITION(aPtr, "null pointer");
    return aPtr &&
           NS_OK == aPtr->QueryInterface(NS_GET_IID(WrappedJSIdentity), &result) &&
           result == WrappedJSIdentity::GetSingleton();
}

static JSContext *
GetContextFromObject(JSObject *obj)
{
    // Don't stomp over a running context.
    XPCJSContextStack* stack =
        XPCPerThreadData::GetData(nsnull)->GetJSContextStack();
    JSContext* topJSContext;

    if(stack && NS_SUCCEEDED(stack->Peek(&topJSContext)) && topJSContext)
        return nsnull;

    // In order to get a context, we need a context.
    XPCCallContext ccx(NATIVE_CALLER);
    if(!ccx.IsValid())
        return nsnull;
    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, obj);
    XPCContext *xpcc = scope->GetContext();

    if(xpcc)
    {
        JSContext *cx = xpcc->GetJSContext();
        if(cx->thread->id == js_CurrentThreadId())
            return cx;
    }

    return nsnull;
}

#ifndef XPCONNECT_STANDALONE
class SameOriginCheckedComponent : public nsISecurityCheckedComponent
{
public:
    SameOriginCheckedComponent(nsXPCWrappedJS* delegate)
        : mDelegate(delegate)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

private:
    nsRefPtr<nsXPCWrappedJS> mDelegate;
};

NS_IMPL_ADDREF(SameOriginCheckedComponent)
NS_IMPL_RELEASE(SameOriginCheckedComponent)

NS_INTERFACE_MAP_BEGIN(SameOriginCheckedComponent)
    NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
NS_INTERFACE_MAP_END_AGGREGATED(mDelegate)

NS_IMETHODIMP
SameOriginCheckedComponent::CanCreateWrapper(const nsIID * iid,
                                             char **_retval NS_OUTPARAM)
{
    // XXX This doesn't actually work because nsScriptSecurityManager doesn't
    // know what to do with "sameOrigin" for canCreateWrapper.
    *_retval = NS_strdup("sameOrigin");
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
SameOriginCheckedComponent::CanCallMethod(const nsIID * iid,
                                          const PRUnichar *methodName,
                                          char **_retval NS_OUTPARAM)
{
    *_retval = NS_strdup("sameOrigin");
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
SameOriginCheckedComponent::CanGetProperty(const nsIID * iid,
                                           const PRUnichar *propertyName,
                                           char **_retval NS_OUTPARAM)
{
    *_retval = NS_strdup("sameOrigin");
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
SameOriginCheckedComponent::CanSetProperty(const nsIID * iid,
                                           const PRUnichar *propertyName,
                                           char **_retval NS_OUTPARAM)
{
    *_retval = NS_strdup("sameOrigin");
    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

#endif

NS_IMETHODIMP
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    if(aIID.Equals(NS_GET_IID(nsIXPConnectJSObjectHolder)))
    {
        NS_ADDREF(self);
        *aInstancePtr = (void*) static_cast<nsIXPConnectJSObjectHolder*>(self);
        return NS_OK;
    }

    // Objects internal to xpconnect are the only objects that even know *how*
    // to ask for this iid. And none of them bother refcounting the thing.
    if(aIID.Equals(NS_GET_IID(WrappedJSIdentity)))
    {
        // asking to find out if this is a wrapper object
        *aInstancePtr = WrappedJSIdentity::GetSingleton();
        return NS_OK;
    }

#ifdef XPC_IDISPATCH_SUPPORT
    // If IDispatch is enabled and we're QI'ing to IDispatch
    if(nsXPConnect::IsIDispatchEnabled() && aIID.Equals(NSID_IDISPATCH))
    {
        return XPCIDispatchExtension::IDispatchQIWrappedJS(self, aInstancePtr);
    }
#endif
    if(aIID.Equals(NS_GET_IID(nsIPropertyBag)))
    {
        // We only want to expose one implementation from our aggregate.
        nsXPCWrappedJS* root = self->GetRootWrapper();

        if(!root->IsValid())
        {
            *aInstancePtr = nsnull;
            return NS_NOINTERFACE;
        }

        NS_ADDREF(root);
        *aInstancePtr = (void*) static_cast<nsIPropertyBag*>(root);
        return NS_OK;
    }

    // We can't have a cached wrapper.
    if(aIID.Equals(NS_GET_IID(nsWrapperCache)))
    {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }

    JSContext *context = GetContextFromObject(self->GetJSObject());
    XPCCallContext ccx(NATIVE_CALLER, context);
    if(!ccx.IsValid())
    {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }

    // We support nsISupportsWeakReference iff the root wrapped JSObject
    // claims to support it in its QueryInterface implementation.
    if(aIID.Equals(NS_GET_IID(nsISupportsWeakReference)))
    {
        // We only want to expose one implementation from our aggregate.
        nsXPCWrappedJS* root = self->GetRootWrapper();

        // Fail if JSObject doesn't claim support for nsISupportsWeakReference
        if(!root->IsValid() ||
           !CallQueryInterfaceOnJSObject(ccx, root->GetJSObject(), aIID))
        {
            *aInstancePtr = nsnull;
            return NS_NOINTERFACE;
        }

        NS_ADDREF(root);
        *aInstancePtr = (void*) static_cast<nsISupportsWeakReference*>(root);
        return NS_OK;
    }

    nsXPCWrappedJS* sibling;

    // Checks for any existing wrapper explicitly constructed for this iid.
    // This includes the current 'self' wrapper. This also deals with the
    // nsISupports case (for which it returns mRoot).
    if(nsnull != (sibling = self->Find(aIID)))
    {
        NS_ADDREF(sibling);
        *aInstancePtr = sibling->GetXPTCStub();
        return NS_OK;
    }

    // Check if asking for an interface from which one of our wrappers inherits.
    if(nsnull != (sibling = self->FindInherited(aIID)))
    {
        NS_ADDREF(sibling);
        *aInstancePtr = sibling->GetXPTCStub();
        return NS_OK;
    }

    // else we do the more expensive stuff...

#ifndef XPCONNECT_STANDALONE
    // Before calling out, ensure that we're not about to claim to implement
    // nsISecurityCheckedComponent for an untrusted object. Doing so causes
    // problems. See bug 352882.
    // But if this is a content object, then we might be wrapping it for
    // content. If our JS object isn't a double-wrapped object (that is, we
    // don't have XPCWrappedJS(XPCWrappedNative(some C++ object))), then it
    // definitely will not have classinfo (and therefore won't be a DOM
    // object). Since content wants to be able to use these objects (directly
    // or indirectly, see bug 483672), we implement nsISecurityCheckedComponent
    // for them and tell caps that they are also bound by the same origin
    // model.

    if(aIID.Equals(NS_GET_IID(nsISecurityCheckedComponent)))
    {
        // XXX This code checks to see if the given object has chrome (also
        // known as system) principals. It really wants to do a
        // UniversalXPConnect type check.

        *aInstancePtr = nsnull;

        if(!XPCPerThreadData::IsMainThread(ccx.GetJSContext()))
            return NS_NOINTERFACE;

        nsXPConnect *xpc = nsXPConnect::GetXPConnect();
        nsCOMPtr<nsIScriptSecurityManager> secMan =
            do_QueryInterface(xpc->GetDefaultSecurityManager());
        if(!secMan)
            return NS_NOINTERFACE;

        JSObject *selfObj = self->GetJSObject();
        nsCOMPtr<nsIPrincipal> objPrin;
        nsresult rv = secMan->GetObjectPrincipal(ccx, selfObj,
                                                 getter_AddRefs(objPrin));
        if(NS_FAILED(rv))
            return rv;

        PRBool isSystem;
        rv = secMan->IsSystemPrincipal(objPrin, &isSystem);
        if((NS_FAILED(rv) || !isSystem) &&
           !IS_WRAPPER_CLASS(selfObj->getClass()))
        {
            // A content object.
            nsRefPtr<SameOriginCheckedComponent> checked =
                new SameOriginCheckedComponent(self);
            if(!checked)
                return NS_ERROR_OUT_OF_MEMORY;
            *aInstancePtr = checked.forget().get();
            return NS_OK;
        }
    }
#endif

    // check if the JSObject claims to implement this interface
    JSObject* jsobj = CallQueryInterfaceOnJSObject(ccx, self->GetJSObject(),
                                                   aIID);
    if(jsobj)
    {
        // protect jsobj until it is actually attached
        AUTO_MARK_JSVAL(ccx, OBJECT_TO_JSVAL(jsobj));

        // We can't use XPConvert::JSObject2NativeInterface() here
        // since that can find a XPCWrappedNative directly on the
        // proto chain, and we don't want that here. We need to find
        // the actual JS object that claimed it supports the interface
        // we're looking for or we'll potentially bypass security
        // checks etc by calling directly through to a native found on
        // the prototype chain.
        //
        // Instead, simply do the nsXPCWrappedJS part of
        // XPConvert::JSObject2NativeInterface() here to make sure we
        // get a new (or used) nsXPCWrappedJS.
        nsXPCWrappedJS* wrapper;
        nsresult rv = nsXPCWrappedJS::GetNewOrUsed(ccx, jsobj, aIID, nsnull,
                                                   &wrapper);
        if(NS_SUCCEEDED(rv) && wrapper)
        {
            // We need to go through the QueryInterface logic to make
            // this return the right thing for the various 'special'
            // interfaces; e.g.  nsIPropertyBag.
            rv = wrapper->QueryInterface(aIID, aInstancePtr);
            NS_RELEASE(wrapper);
            return rv;
        }
    }

    // else...
    // no can do
    *aInstancePtr = nsnull;
    return NS_NOINTERFACE;
}

JSObject*
nsXPCWrappedJSClass::GetRootJSObject(XPCCallContext& ccx, JSObject* aJSObj)
{
    JSObject* result = CallQueryInterfaceOnJSObject(ccx, aJSObj,
                                                    NS_GET_IID(nsISupports));
    if(!result)
        return aJSObj;
    JSObject* inner = XPCWrapper::Unwrap(ccx, result);
    if (inner)
        return inner;
    return result;
}

void
xpcWrappedJSErrorReporter(JSContext *cx, const char *message,
                          JSErrorReport *report)
{
    if(report)
    {
        // If it is an exception report, then we can just deal with the
        // exception later (if not caught in the JS code).
        if(JSREPORT_IS_EXCEPTION(report->flags))
        {
            // XXX We have a problem with error reports from uncaught exceptions.
            //
            // http://bugzilla.mozilla.org/show_bug.cgi?id=66453
            //
            // The issue is...
            //
            // We can't assume that the exception will *stay* uncaught. So, if
            // we build an nsIXPCException here and the underlying exception
            // really is caught before our script is done running then we blow
            // it by returning failure to our caller when the script didn't
            // really fail. However, This report contains error location info
            // that is no longer available after the script is done. So, if the
            // exception really is not caught (and is a non-engine exception)
            // then we've lost the oportunity to capture the script location
            // info that we *could* have captured here.
            //
            // This is expecially an issue with nested evaluations.
            //
            // Perhaps we could capture an expception here and store it as
            // 'provisional' and then later if there is a pending exception
            // when the script is done then we could maybe compare that in some
            // way with the 'provisional' one in which we captured location info.
            // We would not want to assume that the one discovered here is the
            // same one that is later detected. This could cause us to lie.
            //
            // The thing is. we do not currently store the right stuff to compare
            // these two nsIXPCExceptions (triggered by the same exception jsval
            // in the engine). Maybe we should store the jsval and compare that?
            // Maybe without even rooting it since we will not dereference it.
            // This is inexact, but maybe the right thing to do?
            //
            // if(report->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)) ...
            //

            return;
        }

        if(JSREPORT_IS_WARNING(report->flags))
        {
            // XXX printf the warning (#ifdef DEBUG only!).
            // XXX send the warning to the console service.
            return;
        }
    }

    XPCCallContext ccx(NATIVE_CALLER, cx);
    if(!ccx.IsValid())
        return;

    nsCOMPtr<nsIException> e;
    XPCConvert::JSErrorToXPCException(ccx, message, nsnull, nsnull, report,
                                      getter_AddRefs(e));
    if(e)
        ccx.GetXPCContext()->SetException(e);
}

JSBool
nsXPCWrappedJSClass::GetArraySizeFromParam(JSContext* cx,
                                           const XPTMethodDescriptor* method,
                                           const nsXPTParamInfo& param,
                                           uint16 methodIndex,
                                           uint8 paramIndex,
                                           SizeMode mode,
                                           nsXPTCMiniVariant* nativeParams,
                                           JSUint32* result)
{
    uint8 argnum;
    nsresult rv;

    if(mode == GET_SIZE)
        rv = mInfo->GetSizeIsArgNumberForParam(methodIndex, &param, 0, &argnum);
    else
        rv = mInfo->GetLengthIsArgNumberForParam(methodIndex, &param, 0, &argnum);
    if(NS_FAILED(rv))
        return JS_FALSE;

    const nsXPTParamInfo& arg_param = method->params[argnum];
    const nsXPTType& arg_type = arg_param.GetType();

    // The xpidl compiler ensures this. We reaffirm it for safety.
    if(arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_U32)
        return JS_FALSE;

    if(arg_param.IsOut())
        *result = *(JSUint32*)nativeParams[argnum].val.p;
    else
        *result = nativeParams[argnum].val.u32;

    return JS_TRUE;
}

JSBool
nsXPCWrappedJSClass::GetInterfaceTypeFromParam(JSContext* cx,
                                               const XPTMethodDescriptor* method,
                                               const nsXPTParamInfo& param,
                                               uint16 methodIndex,
                                               const nsXPTType& type,
                                               nsXPTCMiniVariant* nativeParams,
                                               nsID* result)
{
    uint8 type_tag = type.TagPart();

    if(type_tag == nsXPTType::T_INTERFACE)
    {
        if(NS_SUCCEEDED(GetInterfaceInfo()->
                GetIIDForParamNoAlloc(methodIndex, &param, result)))
        {
            return JS_TRUE;
        }
    }
    else if(type_tag == nsXPTType::T_INTERFACE_IS)
    {
        uint8 argnum;
        nsresult rv;
        rv = mInfo->GetInterfaceIsArgNumberForParam(methodIndex,
                                                    &param, &argnum);
        if(NS_FAILED(rv))
            return JS_FALSE;

        const nsXPTParamInfo& arg_param = method->params[argnum];
        const nsXPTType& arg_type = arg_param.GetType();
        if(arg_type.IsPointer() &&
           arg_type.TagPart() == nsXPTType::T_IID)
        {
            if(arg_param.IsOut())
            {
                nsID** p = (nsID**) nativeParams[argnum].val.p;
                if(!p || !*p)
                    return JS_FALSE;
                *result = **p;
            }
            else
            {
                nsID* p = (nsID*) nativeParams[argnum].val.p;
                if(!p)
                    return JS_FALSE;
                *result = *p;
            }
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

void
nsXPCWrappedJSClass::CleanupPointerArray(const nsXPTType& datum_type,
                                         JSUint32 array_count,
                                         void** arrayp)
{
    if(datum_type.IsInterfacePointer())
    {
        nsISupports** pp = (nsISupports**) arrayp;
        for(JSUint32 k = 0; k < array_count; k++)
        {
            nsISupports* p = pp[k];
            NS_IF_RELEASE(p);
        }
    }
    else
    {
        void** pp = (void**) arrayp;
        for(JSUint32 k = 0; k < array_count; k++)
        {
            void* p = pp[k];
            if(p) nsMemory::Free(p);
        }
    }
}

void
nsXPCWrappedJSClass::CleanupPointerTypeObject(const nsXPTType& type,
                                              void** pp)
{
    NS_ASSERTION(pp,"null pointer");
    if(type.IsInterfacePointer())
    {
        nsISupports* p = *((nsISupports**)pp);
        if(p) p->Release();
    }
    else
    {
        void* p = *((void**)pp);
        if(p) nsMemory::Free(p);
    }
}

class AutoClearPendingException
{
public:
  AutoClearPendingException(JSContext *cx) : mCx(cx) { }
  ~AutoClearPendingException() { JS_ClearPendingException(mCx); }
private:
  JSContext* mCx;
};

nsresult
nsXPCWrappedJSClass::CheckForException(XPCCallContext & ccx,
                                       const char * aPropertyName,
                                       const char * anInterfaceName,
                                       PRBool aForceReport)
{
    XPCContext * xpcc = ccx.GetXPCContext();
    JSContext * cx = ccx.GetJSContext();
    nsCOMPtr<nsIException> xpc_exception;
    /* this one would be set by our error reporter */

    xpcc->GetException(getter_AddRefs(xpc_exception));
    if(xpc_exception)
        xpcc->SetException(nsnull);

    // get this right away in case we do something below to cause JS code
    // to run on this JSContext
    nsresult pending_result = xpcc->GetPendingResult();

    jsval js_exception;
    JSBool is_js_exception = JS_GetPendingException(cx, &js_exception);

    /* JS might throw an expection whether the reporter was called or not */
    if(is_js_exception)
    {
        if(!xpc_exception)
            XPCConvert::JSValToXPCException(ccx, js_exception, anInterfaceName,
                                            aPropertyName,
                                            getter_AddRefs(xpc_exception));

        /* cleanup and set failed even if we can't build an exception */
        if(!xpc_exception)
        {
            ccx.GetThreadData()->SetException(nsnull); // XXX necessary?
        }
    }

    AutoClearPendingException acpe(cx);

    if(xpc_exception)
    {
        nsresult e_result;
        if(NS_SUCCEEDED(xpc_exception->GetResult(&e_result)))
        {
            // Figure out whether or not we should report this exception.
            PRBool reportable = xpc_IsReportableErrorCode(e_result);
            if(reportable)
            {
                // Always want to report forced exceptions and XPConnect's own
                // errors.
                reportable = aForceReport ||
                    NS_ERROR_GET_MODULE(e_result) == NS_ERROR_MODULE_XPCONNECT;

                // See if an environment variable was set or someone has told us
                // that a user pref was set indicating that we should report all
                // exceptions.
                if(!reportable)
                    reportable = nsXPConnect::ReportAllJSExceptions();

                // Finally, check to see if this is the last JS frame on the
                // stack. If so then we always want to report it.
                if(!reportable)
                {
                    PRBool onlyNativeStackFrames = PR_TRUE;
                    JSStackFrame * fp = nsnull;
                    while((fp = JS_FrameIterator(cx, &fp)))
                    {
                        if(!JS_IsNativeFrame(cx, fp))
                        {
                            onlyNativeStackFrames = PR_FALSE;
                            break;
                        }
                    }
                    reportable = onlyNativeStackFrames;
                }
                
                // Ugly special case for GetInterface. It's "special" in the
                // same way as QueryInterface in that a failure is not
                // exceptional and shouldn't be reported. We have to do this
                // check here instead of in xpcwrappedjs (like we do for QI) to
                // avoid adding extra code to all xpcwrappedjs objects.
                if(reportable && e_result == NS_ERROR_NO_INTERFACE &&
                   !strcmp(anInterfaceName, "nsIInterfaceRequestor") &&
                   !strcmp(aPropertyName, "getInterface"))
                {
                    reportable = PR_FALSE;
                }
            }

            // Try to use the error reporter set on the context to handle this
            // error if it came from a JS exception.
            if(reportable && is_js_exception &&
               cx->errorReporter != xpcWrappedJSErrorReporter)
            {
                reportable = !JS_ReportPendingException(cx);
            }

            if(reportable)
            {
#ifdef DEBUG
                static const char line[] =
                    "************************************************************\n";
                static const char preamble[] =
                    "* Call to xpconnect wrapped JSObject produced this error:  *\n";
                static const char cant_get_text[] =
                    "FAILED TO GET TEXT FROM EXCEPTION\n";

                fputs(line, stdout);
                fputs(preamble, stdout);
                char* text;
                if(NS_SUCCEEDED(xpc_exception->ToString(&text)) && text)
                {
                    fputs(text, stdout);
                    fputs("\n", stdout);
                    nsMemory::Free(text);
                }
                else
                    fputs(cant_get_text, stdout);
                fputs(line, stdout);
#endif

                // Log the exception to the JS Console, so that users can do
                // something with it.
                nsCOMPtr<nsIConsoleService> consoleService
                    (do_GetService(XPC_CONSOLE_CONTRACTID));
                if(nsnull != consoleService)
                {
                    nsresult rv;
                    nsCOMPtr<nsIScriptError> scriptError;
                    nsCOMPtr<nsISupports> errorData;
                    rv = xpc_exception->GetData(getter_AddRefs(errorData));
                    if(NS_SUCCEEDED(rv))
                        scriptError = do_QueryInterface(errorData);

                    if(nsnull == scriptError)
                    {
                        // No luck getting one from the exception, so
                        // try to cook one up.
                        scriptError = do_CreateInstance(XPC_SCRIPT_ERROR_CONTRACTID);
                        if(nsnull != scriptError)
                        {
                            char* exn_string;
                            rv = xpc_exception->ToString(&exn_string);
                            if(NS_SUCCEEDED(rv))
                            {
                                // use toString on the exception as the message
                                nsAutoString newMessage;
                                newMessage.AssignWithConversion(exn_string);
                                nsMemory::Free((void *) exn_string);

                                // try to get filename, lineno from the first
                                // stack frame location.
                                PRInt32 lineNumber = 0;
                                nsXPIDLCString sourceName;

                                nsCOMPtr<nsIStackFrame> location;
                                xpc_exception->
                                    GetLocation(getter_AddRefs(location));
                                if(location)
                                {
                                    // Get line number w/o checking; 0 is ok.
                                    location->GetLineNumber(&lineNumber);

                                    // get a filename.
                                    rv = location->GetFilename(getter_Copies(sourceName));
                                }

                                rv = scriptError->Init(newMessage.get(),
                                                       NS_ConvertASCIItoUTF16(sourceName).get(),
                                                       nsnull,
                                                       lineNumber, 0, 0,
                                                       "XPConnect JavaScript");
                                if(NS_FAILED(rv))
                                    scriptError = nsnull;
                            }
                        }
                    }
                    if(nsnull != scriptError)
                        consoleService->LogMessage(scriptError);
                }
            }
            // Whether or not it passes the 'reportable' test, it might
            // still be an error and we have to do the right thing here...
            if(NS_FAILED(e_result))
            {
                ccx.GetThreadData()->SetException(xpc_exception);
                return e_result;
            }
        }
    }
    else
    {
        // see if JS code signaled failure result without throwing exception
        if(NS_FAILED(pending_result))
        {
            return pending_result;
        }
    }
    return NS_ERROR_FAILURE;
}

class ContextPrincipalGuard
{
    nsIScriptSecurityManager *ssm;
    XPCCallContext &ccx;
  public:
    ContextPrincipalGuard(XPCCallContext &ccx)
      : ssm(nsnull), ccx(ccx) {}
    void principalPushed(nsIScriptSecurityManager *ssm) { this->ssm = ssm; }
    ~ContextPrincipalGuard() { if (ssm) ssm->PopContextPrincipal(ccx); }
};

NS_IMETHODIMP
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                                const XPTMethodDescriptor* info,
                                nsXPTCMiniVariant* nativeParams)
{
    jsval* stackbase = nsnull;
    jsval* sp = nsnull;
    uint8 i;
    uint8 argc=0;
    uint8 paramCount=0;
    nsresult retval = NS_ERROR_FAILURE;
    nsresult pending_result = NS_OK;
    JSBool success;
    JSBool readyToDoTheCall = JS_FALSE;
    nsID  param_iid;
    JSObject* obj;
    const char* name = info->name;
    jsval fval;
    JSBool foundDependentParam;
    XPCContext* xpcc;
    JSContext* cx;
    JSObject* thisObj;
    bool invokeCall;

    // Make sure not to set the callee on ccx until after we've gone through
    // the whole nsIXPCFunctionThisTranslator bit.  That code uses ccx to
    // convert natives to JSObjects, but we do NOT plan to pass those JSObjects
    // to our real callee.
    JSContext *context = GetContextFromObject(wrapper->GetJSObject());
    XPCCallContext ccx(NATIVE_CALLER, context);
    if(ccx.IsValid())
    {
        xpcc = ccx.GetXPCContext();
        cx = ccx.GetJSContext();
    }
    else
    {
        xpcc = nsnull;
        cx = nsnull;
    }

    AutoScriptEvaluate scriptEval(cx);
    js::InvokeArgsGuard args;
    ContextPrincipalGuard principalGuard(ccx);

    obj = thisObj = wrapper->GetJSObject();

    // XXX ASSUMES that retval is last arg. The xpidl compiler ensures this.
    paramCount = info->num_args;
    argc = paramCount -
        (paramCount && XPT_PD_IS_RETVAL(info->params[paramCount-1].flags) ? 1 : 0);

    if(!cx || !xpcc || !IsReflectable(methodIndex))
        goto pre_call_clean_up;

    scriptEval.StartEvaluating(xpcWrappedJSErrorReporter);

    xpcc->SetPendingResult(pending_result);
    xpcc->SetException(nsnull);
    ccx.GetThreadData()->SetException(nsnull);

    if(XPCPerThreadData::IsMainThread(ccx))
    {
        nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
        if(ssm)
        {
            nsCOMPtr<nsIPrincipal> objPrincipal;
            ssm->GetObjectPrincipal(ccx, obj, getter_AddRefs(objPrincipal));
            if(objPrincipal)
            {
                JSStackFrame* fp = nsnull;
                nsresult rv =
                    ssm->PushContextPrincipal(ccx, JS_FrameIterator(ccx, &fp),
                                              objPrincipal);
                if(NS_FAILED(rv))
                {
                    JS_ReportOutOfMemory(ccx);
                    retval = NS_ERROR_OUT_OF_MEMORY;
                    goto pre_call_clean_up;
                }

                principalGuard.principalPushed(ssm);
            }
        }
    }

    // We use js_Invoke so that the gcthings we use as args will be rooted by
    // the engine as we do conversions and prepare to do the function call.
    // This adds a fair amount of complexity, but it's a good optimization
    // compared to calling JS_AddRoot for each item.

    js::LeaveTrace(cx);

    // setup stack

    // if this isn't a function call then we don't need to push extra stuff
    invokeCall = !(XPT_MD_IS_SETTER(info->flags) || XPT_MD_IS_GETTER(info->flags));
    if (invokeCall)
    {
        // We get fval before allocating the stack to avoid gc badness that can
        // happen if the GetProperty call leaves our request and the gc runs
        // while the stack we allocate contains garbage.

        // If the interface is marked as a [function] then we will assume that
        // our JSObject is a function and not an object with a named method.

        PRBool isFunction;
        if(NS_FAILED(mInfo->IsFunction(&isFunction)))
            goto pre_call_clean_up;

        // In the xpidl [function] case we are making sure now that the 
        // JSObject is callable. If it is *not* callable then we silently 
        // fallback to looking up the named property...
        // (because jst says he thinks this fallback is 'The Right Thing'.)
        //
        // In the normal (non-function) case we just lookup the property by 
        // name and as long as the object has such a named property we go ahead
        // and try to make the call. If it turns out the named property is not
        // a callable object then the JS engine will throw an error and we'll
        // pass this along to the caller as an exception/result code.

        if(isFunction && 
           JS_TypeOfValue(ccx, OBJECT_TO_JSVAL(obj)) == JSTYPE_FUNCTION)
        {
            fval = OBJECT_TO_JSVAL(obj);

            // We may need to translate the 'this' for the function object.

            if(paramCount)
            {
                const nsXPTParamInfo& firstParam = info->params[0];
                if(firstParam.IsIn())
                {
                    const nsXPTType& firstType = firstParam.GetType();
                    if(firstType.IsPointer() && firstType.IsInterfacePointer())
                    {
                        nsIXPCFunctionThisTranslator* translator;

                        IID2ThisTranslatorMap* map =
                            mRuntime->GetThisTranslatorMap();

                        {
                            XPCAutoLock lock(mRuntime->GetMapLock()); // scoped lock
                            translator = map->Find(mIID);
                        }

                        if(translator)
                        {
                            PRBool hideFirstParamFromJS = PR_FALSE;
                            nsIID* newWrapperIID = nsnull;
                            nsCOMPtr<nsISupports> newThis;

                            if(NS_FAILED(translator->
                              TranslateThis((nsISupports*)nativeParams[0].val.p,
                                            mInfo, methodIndex,
                                            &hideFirstParamFromJS,
                                            &newWrapperIID,
                                            getter_AddRefs(newThis))))
                            {
                                goto pre_call_clean_up;
                            }
                            if(hideFirstParamFromJS)
                            {
                                NS_ERROR("HideFirstParamFromJS not supported");
                                goto pre_call_clean_up;
                            }
                            if(newThis)
                            {
                                jsval v;
                                JSBool ok =
                                  XPCConvert::NativeInterface2JSObject(ccx,
                                        &v, nsnull, newThis, newWrapperIID,
                                        nsnull, nsnull, obj, PR_FALSE, PR_FALSE,
                                        nsnull);
                                if(newWrapperIID)
                                    nsMemory::Free(newWrapperIID);
                                if(!ok)
                                {
                                    goto pre_call_clean_up;
                                }
                                thisObj = JSVAL_TO_OBJECT(v);
                            }
                        }
                    }
                }
            }
        }
        else if(!JS_GetMethod(cx, obj, name, &thisObj, &fval))
        {
            // XXX We really want to factor out the error reporting better and
            // specifically report the failure to find a function with this name.
            // This is what we do below if the property is found but is not a
            // function. We just need to factor better so we can get to that
            // reporting path from here.
            goto pre_call_clean_up;
        }
    }

    /*
     * pushInvokeArgs allocates |2 + argc| slots, but getters and setters
     * require only one rooted jsval, so waste one value.
     */
    JS_ASSERT_IF(!invokeCall, argc < 2);
    if (!cx->stack().pushInvokeArgsFriendAPI(cx, invokeCall ? argc : 0, args))
    {
        retval = NS_ERROR_OUT_OF_MEMORY;
        goto pre_call_clean_up;
    }

    sp = stackbase = args.getvp();

    // this is a function call, so push function and 'this'
    if(invokeCall)
    {
        *sp++ = fval;
        *sp++ = OBJECT_TO_JSVAL(thisObj);
    }

    // Figure out what our callee is
    if(XPT_MD_IS_GETTER(info->flags) || XPT_MD_IS_SETTER(info->flags))
    {
        // Pull the getter or setter off of |obj|
        uintN attrs;
        JSBool found;
        JSPropertyOp getter;
        JSPropertyOp setter;
        if(!JS_GetPropertyAttrsGetterAndSetter(cx, obj, name,
                                               &attrs, &found,
                                               &getter, &setter))
        {
            // XXX Do we want to report this exception?
            JS_ClearPendingException(cx);
            goto pre_call_clean_up;
        }

        if(XPT_MD_IS_GETTER(info->flags) && (attrs & JSPROP_GETTER))
        {
            // JSPROP_GETTER means the getter is actually a
            // function object.
            ccx.SetCallee(JS_FUNC_TO_DATA_PTR(JSObject*, getter));
        }
        else if(XPT_MD_IS_SETTER(info->flags) && (attrs & JSPROP_SETTER))
        {
            // JSPROP_SETTER means the setter is actually a
            // function object.
            ccx.SetCallee(JS_FUNC_TO_DATA_PTR(JSObject*, setter));
        }
    }
    else if(JSVAL_IS_OBJECT(fval))
    {
        ccx.SetCallee(JSVAL_TO_OBJECT(fval));
    }

    // build the args
    for(i = 0; i < argc; i++)
    {
        const nsXPTParamInfo& param = info->params[i];
        const nsXPTType& type = param.GetType();
        nsXPTType datum_type;
        JSUint32 array_count;
        PRBool isArray = type.IsArray();
        jsval val = JSVAL_NULL;
        AUTO_MARK_JSVAL(ccx, &val);
        PRBool isSizedString = isArray ?
                JS_FALSE :
                type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;


        // verify that null was not passed for 'out' param
        if(param.IsOut() && !nativeParams[i].val.p)
        {
            retval = NS_ERROR_INVALID_ARG;
            goto pre_call_clean_up;
        }

        if(isArray)
        {
            if(NS_FAILED(mInfo->GetTypeForParam(methodIndex, &param, 1,
                                                &datum_type)))
                goto pre_call_clean_up;
        }
        else
            datum_type = type;

        if(param.IsIn())
        {
            nsXPTCMiniVariant* pv;

            if(param.IsOut())
                pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;
            else
                pv = &nativeParams[i];

            if(datum_type.IsInterfacePointer() &&
               !GetInterfaceTypeFromParam(cx, info, param, methodIndex,
                                          datum_type, nativeParams,
                                          &param_iid))
                goto pre_call_clean_up;

            if(isArray || isSizedString)
            {
                if(!GetArraySizeFromParam(cx, info, param, methodIndex,
                                          i, GET_LENGTH, nativeParams,
                                          &array_count))
                    goto pre_call_clean_up;
            }

            if(isArray)
            {
                XPCLazyCallContext lccx(ccx);
                if(!XPCConvert::NativeArray2JS(lccx, &val,
                                               (const void**)&pv->val,
                                               datum_type, &param_iid,
                                               array_count, obj, nsnull))
                    goto pre_call_clean_up;
            }
            else if(isSizedString)
            {
                if(!XPCConvert::NativeStringWithSize2JS(ccx, &val,
                                               (const void*)&pv->val,
                                               datum_type,
                                               array_count, nsnull))
                    goto pre_call_clean_up;
            }
            else
            {
                if(!XPCConvert::NativeData2JS(ccx, &val, &pv->val, type,
                                              &param_iid, obj, nsnull))
                    goto pre_call_clean_up;
            }
        }

        if(param.IsOut() || param.IsDipper())
        {
            // create an 'out' object
            JSObject* out_obj = NewOutObject(cx, obj);
            if(!out_obj)
            {
                retval = NS_ERROR_OUT_OF_MEMORY;
                goto pre_call_clean_up;
            }

            if(param.IsIn())
            {
                if(!JS_SetPropertyById(cx, out_obj,
                        mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                        &val))
                {
                    goto pre_call_clean_up;
                }
            }
            *sp++ = OBJECT_TO_JSVAL(out_obj);
        }
        else
            *sp++ = val;
    }

    readyToDoTheCall = JS_TRUE;

pre_call_clean_up:
    // clean up any 'out' params handed in
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->params[i];
        if(!param.IsOut())
            continue;

        const nsXPTType& type = param.GetType();
        if(!type.IsPointer())
            continue;
        void* p;
        if(!(p = nativeParams[i].val.p))
            continue;

        if(param.IsIn())
        {
            if(type.IsArray())
            {
                void** pp;
                if(nsnull != (pp = *((void***)p)))
                {

                    // we need to get the array length and iterate the items
                    JSUint32 array_count;
                    nsXPTType datum_type;

                    if(NS_SUCCEEDED(mInfo->GetTypeForParam(methodIndex, &param,
                                                           1, &datum_type)) &&
                       datum_type.IsPointer() &&
                       GetArraySizeFromParam(cx, info, param, methodIndex,
                                             i, GET_LENGTH, nativeParams,
                                             &array_count) && array_count)
                    {
                        CleanupPointerArray(datum_type, array_count, pp);
                    }
                    // always release the array if it is inout
                    nsMemory::Free(pp);
                }
            }
            else
                CleanupPointerTypeObject(type, (void**)p);
        }
        *((void**)p) = nsnull;
    }

    // Make sure "this" doesn't get deleted during this call.
    nsCOMPtr<nsIXPCWrappedJSClass> kungFuDeathGrip(this);

    if(!readyToDoTheCall)
        return retval;

    // do the deed - note exceptions

    JS_ClearPendingException(cx);

    /* On success, the return value is placed in |*stackbase|. */
    if(XPT_MD_IS_GETTER(info->flags))
        success = JS_GetProperty(cx, obj, name, stackbase);
    else if(XPT_MD_IS_SETTER(info->flags))
        success = JS_SetProperty(cx, obj, name, stackbase);
    else
    {
        if(!JSVAL_IS_PRIMITIVE(fval))
        {
            success = js_Invoke(cx, args, 0);
        }
        else
        {
            // The property was not an object so can't be a function.
            // Let's build and 'throw' an exception.

            static const nsresult code =
                    NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED;
            static const char format[] = "%s \"%s\"";
            const char * msg;
            char* sz = nsnull;

            if(nsXPCException::NameAndFormatForNSResult(code, nsnull, &msg) && msg)
                sz = JS_smprintf(format, msg, name);

            nsCOMPtr<nsIException> e;

            XPCConvert::ConstructException(code, sz, GetInterfaceName(), name,
                                           nsnull, getter_AddRefs(e), nsnull, nsnull);
            xpcc->SetException(e);
            if(sz)
                JS_smprintf_free(sz);
            success = JS_FALSE;
        }
    }

    if(!success)
    {
        PRBool forceReport;
        if(NS_FAILED(mInfo->IsFunction(&forceReport)))
            forceReport = PR_FALSE;

        // May also want to check if we're moving from content->chrome and force
        // a report in that case.

        return CheckForException(ccx, name, GetInterfaceName(), forceReport);
    }

    ccx.GetThreadData()->SetException(nsnull); // XXX necessary?

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    // Convert independent params only.
    // When we later convert the dependent params (if any) we will know that
    // the params upon which they depend will have already been converted -
    // regardless of ordering.

    foundDependentParam = JS_FALSE;
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->params[i];
        if(!param.IsOut() && !param.IsDipper())
            continue;

        const nsXPTType& type = param.GetType();
        if(type.IsDependent())
        {
            foundDependentParam = JS_TRUE;
            continue;
        }

        jsval val;
        uint8 type_tag = type.TagPart();
        JSBool useAllocator = JS_FALSE;
        nsXPTCMiniVariant* pv;

        if(param.IsDipper())
            pv = (nsXPTCMiniVariant*) &nativeParams[i].val.p;
        else
            pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;

        if(param.IsRetval())
            val = *stackbase;
        else if(JSVAL_IS_PRIMITIVE(stackbase[i+2]) ||
                !JS_GetPropertyById(cx, JSVAL_TO_OBJECT(stackbase[i+2]),
                    mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                    &val))
            break;

        // setup allocator and/or iid

        if(type_tag == nsXPTType::T_INTERFACE)
        {
            if(NS_FAILED(GetInterfaceInfo()->
                            GetIIDForParamNoAlloc(methodIndex, &param,
                                                  &param_iid)))
                break;
        }
        else if(type.IsPointer() && !param.IsShared() && !param.IsDipper())
            useAllocator = JS_TRUE;

        if(!XPCConvert::JSData2Native(ccx, &pv->val, val, type,
                                      useAllocator, &param_iid, nsnull))
            break;
    }

    // if any params were dependent, then we must iterate again to convert them.
    if(foundDependentParam && i == paramCount)
    {
        for(i = 0; i < paramCount; i++)
        {
            const nsXPTParamInfo& param = info->params[i];
            if(!param.IsOut())
                continue;

            const nsXPTType& type = param.GetType();
            if(!type.IsDependent())
                continue;

            jsval val;
            nsXPTCMiniVariant* pv;
            nsXPTType datum_type;
            JSBool useAllocator = JS_FALSE;
            JSUint32 array_count;
            PRBool isArray = type.IsArray();
            PRBool isSizedString = isArray ?
                    JS_FALSE :
                    type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                    type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

            pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;

            if(param.IsRetval())
                val = *stackbase;
            else if(!JS_GetPropertyById(cx, JSVAL_TO_OBJECT(stackbase[i+2]),
                        mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                        &val))
                break;

            // setup allocator and/or iid

            if(isArray)
            {
                if(NS_FAILED(mInfo->GetTypeForParam(methodIndex, &param, 1,
                                                    &datum_type)))
                    break;
            }
            else
                datum_type = type;

            if(datum_type.IsInterfacePointer())
            {
               if(!GetInterfaceTypeFromParam(cx, info, param, methodIndex,
                                             datum_type, nativeParams,
                                             &param_iid))
                   break;
            }
            else if(type.IsPointer() && !param.IsShared())
                useAllocator = JS_TRUE;

            if(isArray || isSizedString)
            {
                if(!GetArraySizeFromParam(cx, info, param, methodIndex,
                                          i, GET_LENGTH, nativeParams,
                                          &array_count))
                    break;
            }

            if(isArray)
            {
                if(array_count &&
                   !XPCConvert::JSArray2Native(ccx, (void**)&pv->val, val,
                                               array_count, array_count,
                                               datum_type,
                                               useAllocator, &param_iid,
                                               nsnull))
                    break;
            }
            else if(isSizedString)
            {
                if(!XPCConvert::JSStringWithSize2Native(ccx,
                                                   (void*)&pv->val, val,
                                                   array_count, array_count,
                                                   datum_type, useAllocator,
                                                   nsnull))
                    break;
            }
            else
            {
                if(!XPCConvert::JSData2Native(ccx, &pv->val, val, type,
                                              useAllocator, &param_iid,
                                              nsnull))
                    break;
            }
        }
    }

    if(i != paramCount)
    {
        // We didn't manage all the result conversions!
        // We have to cleanup any junk that *did* get converted.

        for(uint8 k = 0; k < i; k++)
        {
            const nsXPTParamInfo& param = info->params[k];
            if(!param.IsOut())
                continue;
            const nsXPTType& type = param.GetType();
            if(!type.IsPointer())
                continue;
            void* p;
            if(!(p = nativeParams[k].val.p))
                continue;

            if(type.IsArray())
            {
                void** pp;
                if(nsnull != (pp = *((void***)p)))
                {
                    // we need to get the array length and iterate the items
                    JSUint32 array_count;
                    nsXPTType datum_type;

                    if(NS_SUCCEEDED(mInfo->GetTypeForParam(methodIndex, &param,
                                                           1, &datum_type)) &&
                       datum_type.IsPointer() &&
                       GetArraySizeFromParam(cx, info, param, methodIndex,
                                             k, GET_LENGTH, nativeParams,
                                             &array_count) && array_count)
                    {
                        CleanupPointerArray(datum_type, array_count, pp);
                    }
                    nsMemory::Free(pp);
                }
            }
            else
                CleanupPointerTypeObject(type, (void**)p);
            *((void**)p) = nsnull;
        }
    }
    else
    {
        // set to whatever the JS code might have set as the result
        retval = pending_result;
    }

    return retval;
}

const char*
nsXPCWrappedJSClass::GetInterfaceName()
{
    if(!mName)
        mInfo->GetName(&mName);
    return mName;
}

JSObject*
nsXPCWrappedJSClass::NewOutObject(JSContext* cx, JSObject* scope)
{
    return JS_NewObject(cx, nsnull, nsnull, JS_GetGlobalForObject(cx, scope));
}


NS_IMETHODIMP
nsXPCWrappedJSClass::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x with mRefCnt = %d", this, mRefCnt.get()));
    XPC_LOG_INDENT();
        char* name;
        mInfo->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if(name)
            nsMemory::Free(name);
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
        if(iid)
            NS_Free(iid);
        XPC_LOG_ALWAYS(("InterfaceInfo @ %x", mInfo));
        uint16 methodCount = 0;
        if(depth)
        {
            uint16 i;
            nsCOMPtr<nsIInterfaceInfo> parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(getter_AddRefs(parent));
            XPC_LOG_ALWAYS(("parent @ %x", parent.get()));
            mInfo->GetMethodCount(&methodCount);
            XPC_LOG_ALWAYS(("MethodCount = %d", methodCount));
            mInfo->GetConstantCount(&i);
            XPC_LOG_ALWAYS(("ConstantCount = %d", i));
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
        XPC_LOG_ALWAYS(("mDescriptors @ %x count = %d", mDescriptors, methodCount));
        if(depth && mDescriptors && methodCount)
        {
            depth--;
            XPC_LOG_INDENT();
            for(uint16 i = 0; i < methodCount; i++)
            {
                XPC_LOG_ALWAYS(("Method %d is %s%s", \
                        i, IsReflectable(i) ? "":" NOT ","reflectable"));
            }
            XPC_LOG_OUTDENT();
            depth++;
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}
