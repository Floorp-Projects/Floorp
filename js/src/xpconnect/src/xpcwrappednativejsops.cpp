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

/* JavaScript Object Ops for our Wrapped Native JS Objects. */

#include "xpcprivate.h"

extern "C" JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

/***************************************************************************/

AutoPushJSContext::AutoPushJSContext(JSContext *cx, nsXPConnect* xpc /*= nsnull*/)
{
    NS_ASSERTION(cx, "pushing null cx");
#ifdef DEBUG
    mDebugCX = cx;
#endif
    mContextStack = nsXPConnect::GetContextStack(xpc);
    if(mContextStack)
    {
        JSContext* current;

        if(NS_FAILED(mContextStack->Peek(&current)) ||
           current == cx ||
           NS_FAILED(mContextStack->Push(cx)))
        {
            NS_RELEASE(mContextStack);
        }
    }
}

AutoPushJSContext::~AutoPushJSContext()
{
    if(mContextStack)
    {
#ifdef DEBUG
        JSContext* cx;
        nsresult rv = mContextStack->Pop(&cx);
        NS_ASSERTION(NS_SUCCEEDED(rv) && cx == mDebugCX, "unbalanced stack usage");
#else
        mContextStack->Pop(nsnull);
#endif
        NS_RELEASE(mContextStack);
    }
}

/***************************************************************************/

static void ThrowException(uintN errNum, JSContext* cx,
                           nsXPCWrappedNativeClass* clazz = nsnull,
                           const XPCNativeMemberDescriptor* desc = nsnull)
    {nsXPConnect::GetJSThrower()->ThrowException(errNum, cx, clazz, desc);}

// safer!
#define GET_WRAPPER nsXPCWrappedNativeClass::GetWrappedNativeOfJSObject
// #define GET_WRAPPER (nsXPCWrappedNative*) JS_GetPrivate

/***************************************************************************/

static JSObject*
GetDoubleWrappedJSObject(JSContext* cx, nsXPCWrappedNative* wrapper, jsid id)
{
    JSObject* obj = nsnull;
    
    if(wrapper && wrapper->GetNative())
    {
        nsCOMPtr<nsIXPConnectWrappedJS> 
            underware = do_QueryInterface(wrapper->GetNative());               
        if(underware)
        {
            JSObject* mainObj = nsnull;
            if(NS_SUCCEEDED(underware->GetJSObject(&mainObj)) && mainObj)
            {
                jsval val;
                if(OBJ_GET_PROPERTY(cx, mainObj, id, &val) &&
                   !JSVAL_IS_PRIMITIVE(val))
                    obj = JSVAL_TO_OBJECT(val);
            }
        }
    }
    return obj;
}

/***************************************************************************/

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);

    if (!wrapper || !wrapper->IsValid()) {
        if (type == JSTYPE_OBJECT) {
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
        ThrowException(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO, cx);
        return JS_FALSE;
    }

    switch (type) {
    case JSTYPE_OBJECT:
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;

    case JSTYPE_FUNCTION:
        if(wrapper->GetDynamicScriptable())
        {
            *vp = OBJECT_TO_JSVAL(obj);
            return JS_TRUE;
        }
        ThrowException(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN, cx);
        return JS_FALSE;

    case JSTYPE_VOID:
    case JSTYPE_STRING:
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");

        XPCJSRuntime* rt;
        const XPCNativeMemberDescriptor* desc;

        if(nsnull != (rt = clazz->GetRuntime()) &&
           nsnull != (desc = clazz->LookupMemberByID(
                           rt->GetStringID(XPCJSRuntime::IDX_TO_STRING))) &&
           desc->IsMethod())
        {
            if(!clazz->CallWrappedMethod(cx, wrapper, desc,
                                         nsXPCWrappedNativeClass::CALL_METHOD,
                                         0, nsnull, vp))
                return JS_FALSE;
            if(JSVAL_IS_PRIMITIVE(*vp))
                return JS_TRUE;
        }

        // else...
        char* sz = JS_smprintf("[xpconnect wrapped %s]",
                               wrapper->GetClass()->GetInterfaceName());
        if(sz)
        {
            *vp = STRING_TO_JSVAL(JS_NewString(cx, sz, strlen(sz)));
            return JS_TRUE;
        }
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    case JSTYPE_NUMBER:
        *vp = JSVAL_ONE;
        return JS_TRUE;

    case JSTYPE_BOOLEAN:
        *vp = JSVAL_TRUE;
        return JS_TRUE;

    default:
        NS_ASSERTION(0,"bad type in conversion");
        return JS_FALSE;
    }
}

JSBool JS_DLL_CALLBACK
WrappedNative_CallMethod(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    JSFunction *fun;
    jsid id;
    jsval idval;

    nsXPCWrappedNative* wrapper;
    wrapper = GET_WRAPPER(cx, obj);
    if(!wrapper)
        return JS_FALSE;

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    NS_ASSERTION(JS_TypeOfValue(cx, argv[-2]) == JSTYPE_FUNCTION, "bad function");
    fun = (JSFunction*) JS_GetPrivate(cx, JSVAL_TO_OBJECT(argv[-2]));
    idval = STRING_TO_JSVAL(JS_InternString(cx, JS_GetFunctionName(fun)));
    JS_ValueToId(cx, idval, &id);

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(!desc || !desc->IsMethod())
    {
        HANDLE_POSSIBLE_NAME_CASE_ERROR(cx, clazz, id);
        return JS_FALSE;
    }

    return clazz->CallWrappedMethod(cx, wrapper, desc,
                                    nsXPCWrappedNativeClass::CALL_METHOD,
                                    argc, argv, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsXPCWrappedNative* wrapper;

    wrapper = GET_WRAPPER(cx, obj);
    if(!wrapper || !wrapper->IsValid())
    {
        XPCJSRuntime* rt = nsXPConnect::GetRuntime();
        if(rt && id == rt->GetStringID(XPCJSRuntime::IDX_CONSTRUCTOR))
        {
            // silently fail when looking for constructor property
            *vp = JSVAL_VOID;
            return JS_TRUE;
        }
        return JS_FALSE;
    }

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(desc)
    {
        if(desc->IsConstant())
        {
            if(!clazz->GetConstantAsJSVal(cx, wrapper, desc, vp))
                *vp = JSVAL_NULL; //XXX silent failure?
            return JS_TRUE;
        }
        else if(desc->IsMethod())
        {
            JSObject* funobj = clazz->NewFunObj(cx, obj, desc);
            if (!funobj)
                return JS_FALSE;
            *vp = OBJECT_TO_JSVAL(funobj);
            return JS_TRUE;
        }
        else    // attribute
            return clazz->GetAttributeAsJSVal(cx, wrapper, desc, vp);
    }
    else if(wrapper->GetNative() &&
            id == clazz->GetRuntime()->
                            GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT))
    {
        JSObject* realObject = GetDoubleWrappedJSObject(cx, wrapper, id);
        if(realObject)
        {
            // It is a double wrapped object. Figure out if the caller
            // is allowed to see it.

            XPCContext* xpcc = nsXPConnect::GetContext(cx);
            if(xpcc)
            {
                nsIXPCSecurityManager* sm;
    
                sm = xpcc->GetAppropriateSecurityManager(
                                nsIXPCSecurityManager::HOOK_GET_PROPERTY);
                if(sm)
                {
                    nsCOMPtr<nsIInterfaceInfoManager> iimgr =
                            dont_AddRef(nsXPConnect::GetInterfaceInfoManager());
                    if(iimgr)
                    {
                        const nsIID& iid = 
                            NS_GET_IID(nsIXPCWrappedJSObjectGetter);
                        nsCOMPtr<nsIInterfaceInfo> info;
                        if(NS_SUCCEEDED(iimgr->GetInfoForIID(&iid, 
                                                    getter_AddRefs(info))))
                        {
                            if(NS_OK != sm->CanGetProperty(cx, iid,
                                                    wrapper->GetNative(),
                                                    info, 3, id))
                            {
                                // The SecurityManager should have set an exception.
                                return JS_FALSE;
                            }
                        }
                    }
                }
                *vp = OBJECT_TO_JSVAL(realObject);
                return JS_TRUE;
            }
        }
    }
    else
    {
        nsIXPCScriptable* ds;
        nsIXPCScriptable* as;
        if(nsnull != (ds = wrapper->GetDynamicScriptable()) &&
           nsnull != (as = wrapper->GetArbitraryScriptable()))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->GetProperty(cx, obj, id, vp,
                                            wrapper, as, &retval)))
                return retval;
        }
        else
        {
            HANDLE_POSSIBLE_NAME_CASE_ERROR(cx, clazz, id);
        }

        // Check up the prototype chain to match JavaScript lookup behavior
        JSObject* proto = JS_GetPrototype(cx, obj); 
        if(proto)
            return OBJ_GET_PROPERTY(cx, proto, id, vp);
    }
    
    // XXX silently fail when property not found or call fails?
    *vp = JSVAL_VOID;
    return JS_TRUE;
}


JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsXPCWrappedNative* wrapper;

    wrapper = GET_WRAPPER(cx, obj);
    if(!wrapper || !wrapper->IsValid())
        return JS_FALSE;

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
    if(desc)
    {
        if(desc->IsWritableAttribute())
            return clazz->SetAttributeFromJSVal(cx, wrapper, desc, vp);
        else
        {
            // Don't fail silently!
            uintN errNum;

            if(desc->IsConstant())
                errNum = NS_ERROR_XPC_CANT_SET_READ_ONLY_CONSTANT;
            else if(desc->IsMethod())
                errNum = NS_ERROR_XPC_CANT_SET_READ_ONLY_METHOD;
            else
            {
                NS_ASSERTION(desc->IsReadOnlyAttribute(),"bad desc");    
                errNum = NS_ERROR_XPC_CANT_SET_READ_ONLY_ATTRIBUTE;
            }
            ThrowException(errNum, cx, clazz, desc);
            return JS_FALSE;        
        }
    }
    else
    {
        nsIXPCScriptable* ds;
        nsIXPCScriptable* as;
        if(nsnull != (ds = wrapper->GetDynamicScriptable()) &&
           nsnull != (as = wrapper->GetArbitraryScriptable()))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->SetProperty(cx, obj, id, vp,
                                            wrapper, as, &retval)))
                return retval;
            else
            {
                // Don't fail silently!
                ThrowException(NS_ERROR_XPC_CALL_TO_SCRIPTABLE_FAILED, cx, clazz);
                return JS_FALSE;        
            }
        }
        else
        {
            HANDLE_POSSIBLE_NAME_CASE_ERROR(cx, clazz, id);
            // Don't fail silently!
            ThrowException(NS_ERROR_XPC_CANT_ADD_PROP_TO_WRAPPED_NATIVE, cx, clazz);
            return JS_FALSE;        
        }
    }
}

#define XPC_BUILT_IN_PROPERTY ((JSProperty*)1)

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_LookupProperty(JSContext *cx, JSObject *obj, jsid id,
                         JSObject **objp, JSProperty **propp
#if defined JS_THREADSAFE && defined DEBUG
                            , const char *file, uintN line
#endif
                            )
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper && wrapper->IsValid())
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(clazz->LookupMemberByID(id))
        {
            *objp = obj;
            *propp = XPC_BUILT_IN_PROPERTY;
            return JS_TRUE;
        }
        else if(wrapper->GetNative() &&
                id == clazz->GetRuntime()->
                                GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT))
        {
            JSObject* realObject = GetDoubleWrappedJSObject(cx, wrapper, id);
            if(realObject)
            {
                *objp = realObject;
                *propp = XPC_BUILT_IN_PROPERTY;
                return JS_TRUE;
            }
        }
        else if(nsnull != (ds = wrapper->GetDynamicScriptable()) &&
                nsnull != (as = wrapper->GetArbitraryScriptable()))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->LookupProperty(cx, obj, id, objp, propp,
                                               wrapper, as, &retval)))
                return retval;
        }
        else
        {
            HANDLE_POSSIBLE_NAME_CASE_ERROR(cx, clazz, id);
        }
    }

    *objp = nsnull;
    *propp = nsnull;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                         JSPropertyOp getter, JSPropertyOp setter,
                         uintN attrs, JSProperty **propp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper && 
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(!clazz->LookupMemberByID(id))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->DefineProperty(cx, obj, id, value,
                                               getter, setter, attrs, propp,
                                               wrapper, as, &retval)))
                return retval;
        }
    }
    // else fall through
    ThrowException(NS_ERROR_XPC_CANT_DEFINE_PROP_ON_WN, cx);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_GetAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(!clazz->LookupMemberByID(id))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->GetAttributes(cx, obj, id, prop, attrsp,
                                              wrapper, as, &retval)))
                return retval;
        }
    }
    // else fall through
    *attrsp = JSPROP_PERMANENT|JSPROP_ENUMERATE;
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_SetAttributes(JSContext *cx, JSObject *obj, jsid id,
                        JSProperty *prop, uintN *attrsp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(!clazz->LookupMemberByID(id))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->SetAttributes(cx, obj, id, prop, attrsp,
                                              wrapper, as, &retval)))
                return retval;
        }
    }
    // else fall through and silently ignore
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(!clazz->LookupMemberByID(id))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->DeleteProperty(cx, obj, id, vp,
                                               wrapper, as, &retval)))
                return retval;
        }
    }
    // else fall through and silently ignore
    NS_ASSERTION(vp, "hey the engine gave me a null pointer");
    *vp = JSVAL_FALSE;
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_DefaultValue(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->DefaultValue(cx, obj, type, vp,
                                         wrapper, as, &retval)))
            return retval;
    }
    return WrappedNative_Convert(cx, obj, type, vp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
                        jsval *statep, jsid *idp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsXPCWrappedNative* wrapper;
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;

    wrapper = GET_WRAPPER(cx, obj);
    if (!wrapper || !wrapper->IsValid()) {
        *statep = JSVAL_NULL;
        if (idp)
            *idp = INT_TO_JSVAL(0);
        return JS_TRUE;
    }

    nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
    NS_ASSERTION(clazz,"wrapper without class");

    if(nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
        return clazz->DynamicEnumerate(wrapper, ds, as, cx, obj, enum_op,
                                       statep, idp);
    else
        return clazz->StaticEnumerate(wrapper, enum_op, statep, idp);
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_CheckAccess(JSContext *cx, JSObject *obj, jsid id,
                      JSAccessMode mode, jsval *vp, uintN *attrsp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx, obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");
        if(!clazz->LookupMemberByID(id))
        {
            JSBool retval;
            if(NS_SUCCEEDED(ds->CheckAccess(cx, obj, id, mode, vp, attrsp,
                                            wrapper, as, &retval)))
                return retval;
        }
    }
    // else fall through...
    switch (mode) {
    case JSACC_WATCH:
        ThrowException(NS_ERROR_XPC_CANT_WATCH_WN_STATIC, cx);
        return JS_FALSE;

    case JSACC_IMPORT:
        ThrowException(NS_ERROR_XPC_CANT_EXPORT_WN_STATIC, cx);
        return JS_FALSE;

    default:
        return JS_TRUE;
    }
}


JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Call(JSContext *cx, JSObject *obj,
                   uintN argc, jsval *argv, jsval *rval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx,obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->Call(cx, obj, argc, argv, rval,
                        wrapper, as, &retval)))
            return retval;
        ThrowException(NS_ERROR_XPC_SCRIPTABLE_CALL_FAILED, cx);
    }
    else
        ThrowException(NS_ERROR_XPC_CANT_CALL_WO_SCRIPTABLE, cx);
    return JS_FALSE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Construct(JSContext *cx, JSObject *obj,
                        uintN argc, jsval *argv, jsval *rval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    // this is a hack to get the obj of the actual object not the object
    // that JS thinks is the 'this' (which it passes as 'obj').
    if(!(obj = (JSObject*)argv[-2]))
        return JS_FALSE;

    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx,obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->Construct(cx, obj, argc, argv, rval,
                                      wrapper, as, &retval)))
            return retval;
        ThrowException(NS_ERROR_XPC_SCRIPTABLE_CTOR_FAILED, cx);
    }
    else
        ThrowException(NS_ERROR_XPC_CANT_CTOR_WO_SCRIPTABLE, cx);
    return JS_FALSE;
}

// this is the final resting place of non-handled hasInstance calls
JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_ClassHasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    //XXX our default policy is to just say no. Is this right?
    *bp = JS_FALSE;
    return JS_TRUE;
}

// this is in the ObjectOps and is called first
JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsIXPCScriptable* ds;
    nsIXPCScriptable* as;
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx,obj);
    if(wrapper &&
       wrapper->IsValid() &&
       nsnull != (ds = wrapper->GetDynamicScriptable()) &&
       nsnull != (as = wrapper->GetArbitraryScriptable()))
    {
        JSBool retval;
        if(NS_SUCCEEDED(ds->HasInstance(cx, obj, v, bp,
                        wrapper, as, &retval)))
            return retval;
    }
    return WrappedNative_ClassHasInstance(cx, obj, v, bp);
}

JS_STATIC_DLL_CALLBACK(void)
WrappedNative_Finalize(JSContext *cx, JSObject *obj)
{
    nsXPCWrappedNative* wrapper = GET_WRAPPER(cx,obj);
    if(!wrapper || !wrapper->IsValid())
        return;

    // Defer this push until we know we have a valid wrapper to work with.
    // This call can *startup* XPConnect after it has been shutdown!
    AUTO_PUSH_JSCONTEXT(cx);
    // XXX we don't want to be setting this in finalization. RIGHT????
    // SET_CALLER_JAVASCRIPT(cx);
    NS_ASSERTION(obj == wrapper->GetJSObject(),"bad obj");
    // wrapper is responsible for calling DynamicScriptable->Finalize
    wrapper->JSObjectFinalized(cx, obj);
}

JS_STATIC_DLL_CALLBACK(void)
WrappedNative_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop)
{
    /* If this is not one of our 'built-in' native properties AND 
    *  the JS engine has a callback to handle dropProperty then call it.
    */
    if(prop != XPC_BUILT_IN_PROPERTY)
    {
        JSPropertyRefOp drop = js_ObjectOps.dropProperty;
        if(drop)
            drop(cx, obj, prop);
    }
}        

JS_STATIC_DLL_CALLBACK(JSBool)
WrappedNative_Resolve(JSContext *cx, JSObject *obj, jsval idval)
{
    AUTO_PUSH_JSCONTEXT(cx);
    SET_CALLER_JAVASCRIPT(cx);
    nsXPCWrappedNative* wrapper;

    wrapper = GET_WRAPPER(cx, obj);
    if(wrapper && wrapper->IsValid())
    {
        nsXPCWrappedNativeClass* clazz = wrapper->GetClass();
        NS_ASSERTION(clazz,"wrapper without class");

        jsid id; 
        if(JS_ValueToId(cx, idval, &id))
        {
            const XPCNativeMemberDescriptor* desc = clazz->LookupMemberByID(id);
            if(desc)
            {
                jsval val;            
                JSObject* real_obj = wrapper->GetJSObject();
                if(WrappedNative_GetProperty(cx, real_obj, id, &val))
                {
                    return js_ObjectOps.defineProperty(cx, real_obj, 
                                                       id, val,
                                                       nsnull, nsnull, 
                                                       0, nsnull);
                }
            }        
        }
    }
    return JS_TRUE;        
}        

/*
* We have two classes - one with and one without call and construct. We use
* the one without for any object without an nsIXPCScriptable so that the
* engine will show a typeof 'object' instead of 'function'
*/

static JSObjectOps WrappedNative_ops = {
    /* Mandatory non-null function pointer members. */
    nsnull,                      /* filled in at runtime! - newObjectMap */
    nsnull,                      /* filled in at runtime! - destroyObjectMap */
    WrappedNative_LookupProperty,
    WrappedNative_DefineProperty,
    WrappedNative_GetProperty,
    WrappedNative_SetProperty,
    WrappedNative_GetAttributes,
    WrappedNative_SetAttributes,
    WrappedNative_DeleteProperty,
    WrappedNative_DefaultValue,
    WrappedNative_Enumerate,
    WrappedNative_CheckAccess,

    /* Optionally non-null members start here. */
    nsnull,                     /* thisObject   */
    WrappedNative_DropProperty, /* dropProperty */
    nsnull,                     /* call         */
    nsnull,                     /* construct    */
    nsnull,                     /* xdrObject    */
    WrappedNative_HasInstance,  /* hasInstance  */
    nsnull,                     /* filled in at runtime! - setProto */
    nsnull,                     /* filled in at runtime! - setParent */
    nsnull,                     /* filled in at runtime! - mark */
    nsnull,                     /* filled in at runtime! - clear */
    0,0                         /* spare */
};

static JSObjectOps WrappedNativeWithCall_ops = {
    /* Mandatory non-null function pointer members. */
    nsnull,                     /* filled in at runtime! - newObjectMap */
    nsnull,                     /* filled in at runtime! - destroyObjectMap */
    WrappedNative_LookupProperty,
    WrappedNative_DefineProperty,
    WrappedNative_GetProperty,
    WrappedNative_SetProperty,
    WrappedNative_GetAttributes,
    WrappedNative_SetAttributes,
    WrappedNative_DeleteProperty,
    WrappedNative_DefaultValue,
    WrappedNative_Enumerate,
    WrappedNative_CheckAccess,

    /* Optionally non-null members start here. */
    nsnull,                     /* thisObject   */
    WrappedNative_DropProperty, /* dropProperty */
    WrappedNative_Call,         /* call         */
    WrappedNative_Construct,    /* construct    */
    nsnull,                     /* xdrObject    */
    WrappedNative_HasInstance,  /* hasInstance  */
    nsnull,                     /* filled in at runtime! - setProto */
    nsnull,                     /* filled in at runtime! - setParent */
    nsnull,                     /* filled in at runtime! - mark */
    nsnull,                     /* filled in at runtime! - clear */
    0,0                         /* spare */
};

JS_STATIC_DLL_CALLBACK(JSObjectOps *)
WrappedNative_getObjectOps(JSContext *cx, JSClass *clazz)
{
    return &WrappedNative_ops;
}

JS_STATIC_DLL_CALLBACK(JSObjectOps *)
WrappedNative_getWithCallObjectOps(JSContext *cx, JSClass *clazz)
{
    return &WrappedNativeWithCall_ops;
}


JSClass WrappedNative_class = {
    "XPCWrappedNative", 
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, 
    WrappedNative_Resolve,
    WrappedNative_Convert,
    WrappedNative_Finalize,
    /* Optionally non-null members start here. */
    WrappedNative_getObjectOps,         /* getObjectOps */
    nsnull,                             /* checkAccess  */
    nsnull,                             /* call         */
    nsnull,                             /* construct    */
    nsnull,                             /* xdrObject    */
    WrappedNative_ClassHasInstance      /* hasInstance  */
};

JSClass WrappedNativeWithCall_class = {
    "XPCWrappedNativeWithCall", 
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, 
    WrappedNative_Resolve,
    WrappedNative_Convert,
    WrappedNative_Finalize,
    /* Optionally non-null members start here. */
    WrappedNative_getWithCallObjectOps, /* getObjectOps */
    nsnull,                             /* checkAccess  */
    nsnull,                             /* call         */
    nsnull,                             /* construct    */
    nsnull,                             /* xdrObject    */
    WrappedNative_ClassHasInstance      /* hasInstance  */
};

JSBool xpc_InitWrappedNativeJSOps()
{
    if(!WrappedNative_ops.newObjectMap)
    {
        WrappedNative_ops.newObjectMap     = js_ObjectOps.newObjectMap;
        WrappedNative_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
        WrappedNative_ops.setProto         = js_ObjectOps.setProto;
        WrappedNative_ops.setParent        = js_ObjectOps.setParent;
        WrappedNative_ops.mark             = js_ObjectOps.mark;
        WrappedNative_ops.clear            = js_ObjectOps.clear;

        WrappedNativeWithCall_ops.newObjectMap     = js_ObjectOps.newObjectMap;
        WrappedNativeWithCall_ops.destroyObjectMap = js_ObjectOps.destroyObjectMap;
        WrappedNativeWithCall_ops.setProto         = js_ObjectOps.setProto;
        WrappedNativeWithCall_ops.setParent        = js_ObjectOps.setParent;
        WrappedNativeWithCall_ops.mark             = js_ObjectOps.mark;
        WrappedNativeWithCall_ops.clear            = js_ObjectOps.clear;
    }
    return JS_TRUE;
}
