/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* Platform specific code to invoke XPCOM methods on JS objects */

#include "xpcprivate.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCWrappedJSClass, nsIXPCWrappedJSClass)

// the value of this variable is never used - we use its address as a sentinel
static uint32 zero_methods_descriptor;

// static
nsXPCWrappedJSClass*
nsXPCWrappedJSClass::GetNewOrUsedClass(XPCJSRuntime* rt,
                                       REFNSIID aIID)
{
    NS_PRECONDITION(rt, "bad param");

    nsXPCWrappedJSClass* clazz = nsnull;

    {   // scoped lock
        nsAutoLock lock(rt->GetMapLock());  
        IID2WrappedJSClassMap* map = rt->GetWrappedJSClassMap();
        clazz = map->Find(aIID);
    }


    if(clazz)
    {
        NS_ADDREF(clazz);
    }
    else
    {
        nsCOMPtr<nsIInterfaceInfoManager> iimgr =
            dont_AddRef(nsXPConnect::GetInterfaceInfoManager());
        if(iimgr)
        {
            nsCOMPtr<nsIInterfaceInfo> info;
            if(NS_SUCCEEDED(iimgr->GetInfoForIID(&aIID, getter_AddRefs(info))))
            {
                if(nsXPConnect::IsISupportsDescendant(info))
                {
                    clazz = new nsXPCWrappedJSClass(rt, aIID, info);
                    if(!clazz->mDescriptors)
                        NS_RELEASE(clazz);  // sets clazz to nsnull
                }
            }
        }
    }
    return clazz;
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(XPCJSRuntime* rt, REFNSIID aIID,
                                         nsIInterfaceInfo* aInfo)
    : mRuntime(rt),
      mInfo(aInfo),
      mName(nsnull),
      mIID(aIID),
      mDescriptors(nsnull)
{
    NS_ADDREF(mInfo);
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();

    {   // scoped lock
        nsAutoLock lock(rt->GetMapLock());  
        rt->GetWrappedJSClassMap()->Add(this);
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
        nsAutoLock lock(mRuntime->GetMapLock());  
        mRuntime->GetWrappedJSClassMap()->Remove(this);
    }
    if(mName)
        nsMemory::Free(mName);
    NS_IF_RELEASE(mInfo);
}

JSObject*
nsXPCWrappedJSClass::CallQueryInterfaceOnJSObject(JSObject* jsobj, REFNSIID aIID)
{
    AutoPushCompatibleJSContext autoContext(mRuntime->GetJSRuntime());
    JSContext* cx = autoContext.GetJSContext();
    if(!cx)
        return nsnull;

    JSObject* id;
    jsval retval;
    JSObject* retObj;
    JSBool success = JS_FALSE;
    jsid funid;
    jsval fun;

    // check upfront for the existence of the function property
    funid = mRuntime->GetStringID(XPCJSRuntime::IDX_QUERY_INTERFACE);
    if(!OBJ_GET_PROPERTY(cx, jsobj, funid, &fun) || JSVAL_IS_PRIMITIVE(fun))
        return nsnull;

    // XXX we should install an error reporter that will sent reports to 
    // some as yet non-existent JS error console via a service.

    jsval e;
    JSBool hadExpection = JS_GetPendingException(cx, &e);
    JSErrorReporter older = JS_SetErrorReporter(cx, nsnull);
    id = xpc_NewIDObject(cx, jsobj, aIID);
    if(id)
    {
        jsval args[1] = {OBJECT_TO_JSVAL(id)};
        success = JS_CallFunctionValue(cx, jsobj, fun, 1, args, &retval);
    }
    if(success)
        success = JS_ValueToObject(cx, retval, &retObj);
    JS_SetErrorReporter(cx, older);
    if(hadExpection)
        JS_SetPendingException(cx, e);
    else
        JS_ClearPendingException(cx);

    return success ? retObj : nsnull;
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
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

    static void* GetSingleton()
    {
        static WrappedJSIdentity* singleton = nsnull;
        if(!singleton)
            singleton = new WrappedJSIdentity();
        return (void*) singleton;
    }
};

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

NS_IMETHODIMP
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    nsXPCWrappedJS* sibling;

    // This includes checking for nsISupports and the iid of self.
    // And it also checks for other wrappers that have been constructed
    // for this object.
    if(nsnull != (sibling = self->Find(aIID)))
    {
        NS_ADDREF(sibling);
        *aInstancePtr = (void*) sibling;
        return NS_OK;
    }

    if(aIID.Equals(NS_GET_IID(WrappedJSIdentity)))
    {
        // asking to find out if this is a wrapper object
        *aInstancePtr = WrappedJSIdentity::GetSingleton();
        return NS_OK;
    }

    // else...

    // check if asking for an interface that we inherit from

    nsCOMPtr<nsIInterfaceInfo> current = GetInterfaceInfo();
    nsCOMPtr<nsIInterfaceInfo> parent;

    while(NS_SUCCEEDED(current->GetParent(getter_AddRefs(parent))) && parent)
    {
        current = parent;

        nsIID* iid;
        if(NS_SUCCEEDED(current->GetIID(&iid)) && iid)
        {
            PRBool found = aIID.Equals(*iid);
            nsMemory::Free(iid);
            if(found)
            {
                *aInstancePtr = (void*) self;
                NS_ADDREF(self);
                return NS_OK;
            }
        }
    }

    // else...
    // check if the JSObject claims to implement this interface
    JSObject* jsobj = CallQueryInterfaceOnJSObject(self->GetJSObject(), aIID);
    if(jsobj)
    {
        AutoPushCompatibleJSContext autoContext(mRuntime->GetJSRuntime());
        JSContext* cx = autoContext.GetJSContext();
        if(cx && XPCConvert::JSObject2NativeInterface(cx, aInstancePtr,
                                                      jsobj, &aIID, nsnull))
            return NS_OK;
    }

    // else...
    // no can do
    *aInstancePtr = nsnull;
    return NS_NOINTERFACE;
}

JSObject*
nsXPCWrappedJSClass::GetRootJSObject(JSObject* aJSObj)
{
    JSObject* result = CallQueryInterfaceOnJSObject(aJSObj, 
                                                    NS_GET_IID(nsISupports));
    return result ? result : aJSObj;
}

JS_STATIC_DLL_CALLBACK(void)
xpcWrappedJSErrorReporter(JSContext *cx, const char *message,
                          JSErrorReport *report)
{
    nsIXPCException* e;
    XPCContext* xpcc;
    if(nsnull != (e = XPCConvert::JSErrorToXPCException(cx, message, 
                                        nsnull, nsnull, report)) &&
       nsnull != (xpcc = nsXPConnect::GetContext(cx)))
        xpcc->SetException(e);
}

JSBool
nsXPCWrappedJSClass::GetArraySizeFromParam(JSContext* cx,
                                           const nsXPTMethodInfo* method,
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

    const nsXPTParamInfo& arg_param = method->GetParam(argnum);
    const nsXPTType& arg_type = arg_param.GetType();

    // XXX require PRUint32 here - need to require in compiler too!
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
                                               const nsXPTMethodInfo* method,
                                               const nsXPTParamInfo& param,
                                               uint16 methodIndex,
                                               const nsXPTType& type,
                                               nsXPTCMiniVariant* nativeParams,
                                               JSBool* iidIsOwned,
                                               nsID** result)
{
    uint8 type_tag = type.TagPart();
    nsID* iid;

    if(type_tag == nsXPTType::T_INTERFACE)
    {
        if(NS_FAILED(GetInterfaceInfo()->
                GetIIDForParam(methodIndex, &param, &iid)))
        {
            return JS_FALSE;
        }
        *iidIsOwned = JS_TRUE;
    }
    else if(type_tag == nsXPTType::T_INTERFACE_IS)
    {
        uint8 argnum;
        nsresult rv;
        rv = mInfo->GetInterfaceIsArgNumberForParam(methodIndex, 
                                                    &param, &argnum);
        if(NS_FAILED(rv))
            return JS_FALSE;

        const nsXPTParamInfo& arg_param = method->GetParam(argnum);
        const nsXPTType& arg_type = arg_param.GetType();
        if(arg_type.IsPointer() && 
           arg_type.TagPart() == nsXPTType::T_IID)
        {
            if(arg_param.IsOut())
               iid = *((nsID**)nativeParams[argnum].val.p);
            else
               iid = (nsID*) nativeParams[argnum].val.p;
            *iidIsOwned = JS_FALSE;
        }
    }

    *result = iid;
    return iid ? JS_TRUE : JS_FALSE;
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

NS_IMETHODIMP
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper, uint16 methodIndex,
                                const nsXPTMethodInfo* info,
                                nsXPTCMiniVariant* nativeParams)
{
    jsval* stackbase;
    jsval* sp = nsnull;
    uint8 i;
    uint8 argc=0;
    jsval result;
    uint8 paramCount=0;
    nsresult retval = NS_ERROR_FAILURE;
    nsresult pending_result = NS_OK;
    JSErrorReporter older;
    JSBool success;
    JSBool readyToDoTheCall = JS_FALSE;
    nsID* conditional_iid = nsnull;
    JSBool iidIsOwned = JS_FALSE;
    uint8 outConversionFailedIndex;
    JSObject* obj = wrapper->GetJSObject();
    const char* name = info->GetName();
    jsval fval;
    nsIXPCException* xpc_exception;
    jsval js_exception;
    void* mark;
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    JSBool foundDependentParam;
    AutoPushCompatibleJSContext autoContext(mRuntime->GetJSRuntime());
    JSContext* cx = autoContext.GetJSContext();
    XPCContext* xpcc;
    
    if(cx && xpc)
        xpcc = nsXPConnect::GetContext(cx, xpc);
    else
        xpcc = nsnull;

    SET_CALLER_NATIVE(xpcc);

#ifdef DEBUG_stats_jband
    PRIntervalTime startTime = PR_IntervalNow();
    PRIntervalTime endTime = 0;
    static int totalTime = 0;
    

    static int count = 0;
    static const int interval = 10;
    if(0 == (++count % interval))
        printf("<<<<<<<< %d calls on nsXPCWrappedJSs made.  (%d)\n", count, PR_IntervalToMilliseconds(totalTime));
#endif

    // XXX ASSUMES that retval is last arg.
    paramCount = info->GetParamCount();
    argc = paramCount -
            (paramCount && info->GetParam(paramCount-1).IsRetval() ? 1 : 0);

    if(cx)
        older = JS_SetErrorReporter(cx, xpcWrappedJSErrorReporter);

    if(!xpc || !cx || !xpcc || !IsReflectable(methodIndex))
        goto pre_call_clean_up;

    xpcc->SetPendingResult(pending_result);
    xpcc->SetException(nsnull);
    xpc->SetPendingException(nsnull);

    // We use js_AllocStack, js_Invoke, and js_FreeStack so that the gcthings
    // we use as args will be rooted by the engine as we do conversions and
    // prepare to do the function call. This adds a fair amount of complexity,
    // but is a good optimization compared to calling JS_AddRoot for each item.

    // setup stack
    if(nsnull == (stackbase = sp = js_AllocStack(cx, argc + 2, &mark)))
    {
        retval = NS_ERROR_OUT_OF_MEMORY;
        goto pre_call_clean_up;
    }

    // if this is a function call, then push function and 'this'
    if(!info->IsGetter() && !info->IsSetter())
    {
        // later we will check if this might really be callable
        if(!JS_GetProperty(cx, obj, name, &fval))
            goto pre_call_clean_up;
        *sp++ = fval;
        *sp++ = OBJECT_TO_JSVAL(obj);
    }
    else
    {
        sp += 2;
    }

    // build the args
    for(i = 0; i < argc; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        nsXPTType datum_type;
        JSUint32 array_count;
        PRBool isArray = type.IsArray();
        jsval val;
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
                                          &iidIsOwned, &conditional_iid))
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

                if(!XPCConvert::NativeArray2JS(cx, &val, (const void**)&pv->val, 
                                               datum_type, conditional_iid, 
                                               array_count, obj, nsnull))
                    goto pre_call_clean_up;
            }
            else if(isSizedString)
            {
                if(!XPCConvert::NativeStringWithSize2JS(cx, &val, 
                                               (const void*)&pv->val,
                                               datum_type,
                                               array_count, nsnull))
                    goto pre_call_clean_up;
            }
            else
            {
                if(!XPCConvert::NativeData2JS(cx, &val, &pv->val, type,
                                              conditional_iid, obj, nsnull))
                    goto pre_call_clean_up;
            }
            if(conditional_iid)
            {
                if(iidIsOwned)
                {
                    nsMemory::Free((void*)conditional_iid);
                    iidIsOwned = JS_FALSE;
                }
                conditional_iid = nsnull;
            }
        }

        if(param.IsOut())
        {
            // create an 'out' object
            JSObject* out_obj = NewOutObject(cx);
            if(!out_obj)
            {
                retval = NS_ERROR_OUT_OF_MEMORY;
                goto pre_call_clean_up;
            }

            if(param.IsIn())
            {
                if(!OBJ_SET_PROPERTY(cx, out_obj, 
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
        const nsXPTParamInfo& param = info->GetParam(i);
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

    if(conditional_iid)
    {
        if(iidIsOwned)
        {
            nsMemory::Free((void*)conditional_iid);
            iidIsOwned = JS_FALSE;
        }
        conditional_iid = nsnull;
    }

    if(!readyToDoTheCall)
        goto done;

    // do the deed - note exceptions

    JS_ClearPendingException(cx);

    if(info->IsGetter())
        success = JS_GetProperty(cx, obj, name, &result);
    else if(info->IsSetter())
        success = JS_SetProperty(cx, obj, name, sp-1);
    else
    {
        if(!JSVAL_IS_PRIMITIVE(fval))
        {
            // Lift current frame (or make new one) to include the args
            // and do the call.
            JSStackFrame *fp, *oldfp, frame;
            jsval *oldsp;
       
            fp = oldfp = cx->fp;
            if(!fp)
            {
                memset(&frame, 0, sizeof frame);
                cx->fp = fp = &frame;
            }
            oldsp = fp->sp;
            fp->sp = sp;
    
            success = js_Invoke(cx, argc, JSINVOKE_INTERNAL);
          
            result = fp->sp[-1];
            fp->sp = oldsp;
            if(oldfp != fp)
                cx->fp = oldfp;
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

            nsCOMPtr<nsIXPCException> e =
               dont_AddRef(XPCConvert::ConstructException(code, sz,
                                            GetInterfaceName(), name, nsnull));
            xpcc->SetException(e);
            if(sz)
                JS_smprintf_free(sz);
        }
    }

    /* this one would be set by our error reporter */
    xpc_exception = xpcc->GetException();
    if(xpc_exception)
        xpcc->SetException(nsnull);

    // get this right away in case we do something below to cause JS code
    // to run on this JSContext
    pending_result = xpcc->GetPendingResult();

    /* JS might throw an expection whether the reporter was called or not */
    if(JS_GetPendingException(cx, &js_exception))
    {
        if(!xpc_exception)
            xpc_exception = XPCConvert::JSValToXPCException(cx, js_exception,
                                                    GetInterfaceName(), name);

        /* cleanup and set failed even if we can't build an exception */
        if(!xpc_exception)
        {
            xpc->SetPendingException(nsnull); // XXX necessary?
            success = JS_FALSE;
        }
        JS_ClearPendingException(cx);
    }

    if(xpc_exception)
    {
        nsresult e_result;
        if(NS_SUCCEEDED(xpc_exception->GetResult(&e_result)))
        {
            if(NS_FAILED(e_result))
            {
                // XXX we should send the error string to some as yet 
                // non-existent JS error console via a service.
                // Below is a quick hack to show the error/exception text
                // via printf in debug builds only
#ifdef DEBUG
                static const char line[] = 
                    "************************************************************\n";
                static const char disclaimer[] = 
                    "** NOTE: This report will only be printed in DEBUG builds.**\n";
                static const char preamble[] = 
                    "* Call to xpconnect wrapped JSObject produced this error:  *\n";
                static const char cant_get_text[] = 
                    "FAILED TO GET TEXT FROM EXCEPTION\n";
                printf(line);
                printf(disclaimer);     
                printf(preamble);
                char* text;
                if(NS_SUCCEEDED(xpc_exception->ToString(&text)) && text)
                {
                    printf(text);     
                    printf("\n");
                    nsMemory::Free(text);        
                }
                else
                    printf(cant_get_text);     
                printf(line);     
#endif
                // Log the exception to the JS Console, so that users can do
                // something with it.
                nsCOMPtr<nsIConsoleService> consoleService
                    (do_GetService("mozilla.consoleservice.1"));
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
                        scriptError = do_CreateInstance("@mozilla.org/scripterror;1");
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
                                PRUnichar* newMessageUni;
                                newMessageUni = newMessage.ToNewUnicode();

                                // try to get filename, lineno from the first
                                // stack frame location.
                                PRUnichar* sourceNameUni = nsnull;
                                PRInt32 lineNumber = 0;

                                nsCOMPtr<nsIJSStackFrameLocation> location;
                                xpc_exception->
                                    GetLocation(getter_AddRefs(location));
                                if(location)
                                {
                                    // Get line number w/o checking; 0 is ok.
                                    location->GetLineNumber(&lineNumber);

                                    // get a filename.
                                    char *csourceName;
                                    rv = location->GetFilename(&csourceName);
                                    nsAutoString newSourceName;
                                    newSourceName.
                                        AssignWithConversion(csourceName);
                                    nsMemory::Free((void *)csourceName);
                                    sourceNameUni = newSourceName.ToNewUnicode();
                                }

                                rv = scriptError->Init(newMessageUni,
                                                       sourceNameUni, nsnull,
                                                       lineNumber, 0, 0,
                                                       "XPConnect JavaScript");
                                if(NS_FAILED(rv))
                                    scriptError = nsnull;
                                nsMemory::Free((void *)newMessageUni);
                                if(nsnull != sourceNameUni)
                                    nsMemory::Free((void *)sourceNameUni);
                            }
                        }
                    }

                    if(nsnull != scriptError)
                        consoleService->LogMessage(scriptError);
                }
                xpc->SetPendingException(xpc_exception);
                retval = e_result;
            }
        }
        NS_RELEASE(xpc_exception);
        success = JS_FALSE;
    }
    else
    {
        // see if JS code signaled failure result without throwing exception
        if(NS_FAILED(pending_result))
        {
            retval = pending_result;
            success = JS_FALSE;
        }
    }

    if(!success)
        goto done;

    xpc->SetPendingException(nsnull); // XXX necessary?

#define HANDLE_OUT_CONVERSION_FAILURE       \
    PR_BEGIN_MACRO                          \
        outConversionFailedIndex = i;       \
        break;                              \
    PR_END_MACRO

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    // Convert independent params only.
    // When we later convert the dependent params (if any) we will know that 
    // the params upon which they depend will have already been converted - 
    // regardless of ordering.

    outConversionFailedIndex = paramCount;
    foundDependentParam = JS_FALSE;
    for(i = 0; i < paramCount; i++)
    {
        const nsXPTParamInfo& param = info->GetParam(i);
        if(!param.IsOut())
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

        pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;

        if(param.IsRetval())
            val = result;
        else if(!OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(stackbase[i+2]), 
                    mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE), 
                    &val))
            HANDLE_OUT_CONVERSION_FAILURE;

        // setup allocator and/or iid

        if(type_tag == nsXPTType::T_INTERFACE)
        {
            if(NS_FAILED(GetInterfaceInfo()->GetIIDForParam(methodIndex, &param,
                                                            &conditional_iid)))
                HANDLE_OUT_CONVERSION_FAILURE;
            iidIsOwned = JS_TRUE;
        }
        else if(type.IsPointer() && !param.IsShared())
            useAllocator = JS_TRUE;

        if(!XPCConvert::JSData2Native(cx, &pv->val, val, type,
                                      useAllocator, conditional_iid, nsnull))
            HANDLE_OUT_CONVERSION_FAILURE;

        if(conditional_iid)
        {
            if(iidIsOwned)
            {
                nsMemory::Free((void*)conditional_iid);
                iidIsOwned = JS_FALSE;
            }
            conditional_iid = nsnull;
        }
    }

    // if any params were dependent, then we must iterate again to convert them.
    if(foundDependentParam && outConversionFailedIndex == paramCount)
    {
        for(i = 0; i < paramCount; i++)
        {
            const nsXPTParamInfo& param = info->GetParam(i);
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
                val = result;
            else if(!OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(stackbase[i+2]), 
                        mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE), 
                        &val))
                HANDLE_OUT_CONVERSION_FAILURE;
    
            // setup allocator and/or iid
    
            if(isArray)
            {
                if(NS_FAILED(mInfo->GetTypeForParam(methodIndex, &param, 1, 
                                                    &datum_type)))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }
            else
                datum_type = type;
    
            if(datum_type.IsInterfacePointer())
            {
               if(!GetInterfaceTypeFromParam(cx, info, param, methodIndex, 
                                             datum_type, nativeParams, 
                                             &iidIsOwned, &conditional_iid))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }
            else if(type.IsPointer() && !param.IsShared())
                useAllocator = JS_TRUE;
    
            if(isArray || isSizedString)
            {
                if(!GetArraySizeFromParam(cx, info, param, methodIndex,
                                          i, GET_LENGTH, nativeParams,
                                          &array_count))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }

            if(isArray)
            {
                if(!XPCConvert::JSArray2Native(cx, (void**)&pv->val, val, 
                                               array_count, array_count, 
                                               datum_type,
                                               useAllocator, conditional_iid, 
                                               nsnull))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }
            else if(isSizedString)
            {
                if(!XPCConvert::JSStringWithSize2Native(cx, 
                                                   (void*)&pv->val, val,
                                                   array_count, array_count, 
                                                   datum_type, useAllocator, 
                                                   nsnull))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }
            else
            {
                if(!XPCConvert::JSData2Native(cx, &pv->val, val, type,
                                              useAllocator, conditional_iid, 
                                              nsnull))
                    HANDLE_OUT_CONVERSION_FAILURE;
            }
    
            if(conditional_iid)
            {
                if(iidIsOwned)
                {
                    nsMemory::Free((void*)conditional_iid);
                    iidIsOwned = JS_FALSE;
                }
                conditional_iid = nsnull;
            }
        }
    }

    if(outConversionFailedIndex != paramCount)
    {
        // We didn't manage all the result conversions!
        // We have to cleanup any junk that *did* get converted.

        for(uint8 k = 0; k < i; k++)
        {
            const nsXPTParamInfo& param = info->GetParam(k);
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

done:
    if(sp)
        js_FreeStack(cx, mark);

    if(conditional_iid && iidIsOwned)
        nsMemory::Free((void*)conditional_iid);

    if(cx)
        JS_SetErrorReporter(cx, older);

    NS_IF_RELEASE(xpc);
#ifdef DEBUG_stats_jband
    endTime = PR_IntervalNow();
    printf("%s::%s %d ( c->js ) \n", GetInterfaceName(), info->GetName(), PR_IntervalToMilliseconds(endTime-startTime));
    totalTime += endTime-startTime;
#endif
    return retval;
}

static JSClass WrappedJSOutArg_class = {
    "XPCOutArg", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

// static
JSBool
nsXPCWrappedJSClass::InitClasses(XPCContext* xpcc, JSObject* aGlobalJSObj)
{
    if (!JS_InitClass(xpcc->GetJSContext(), aGlobalJSObj,
        0, &WrappedJSOutArg_class, 0, 0,
        0, 0,
        0, 0))
        return JS_FALSE;
    return JS_TRUE;
}

const char*
nsXPCWrappedJSClass::GetInterfaceName()
{
    if(!mName)
        mInfo->GetName(&mName);
    return mName;
}

JSObject*
nsXPCWrappedJSClass::NewOutObject(JSContext* cx)
{
    return JS_NewObject(cx, &WrappedJSOutArg_class, nsnull, nsnull);
}


NS_IMETHODIMP
nsXPCWrappedJSClass::DebugDump(PRInt16 depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x with mRefCnt = %d", this, mRefCnt));
    XPC_LOG_INDENT();
        char* name;
        mInfo->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if(name)
            nsMemory::Free(name);
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid));
        delete iid;
        XPC_LOG_ALWAYS(("InterfaceInfo @ %x", mInfo));
        uint16 methodCount = 0;
        if(depth)
        {
            uint16 i;
            nsIInterfaceInfo* parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(&parent);
            XPC_LOG_ALWAYS(("parent @ %x", parent));
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
